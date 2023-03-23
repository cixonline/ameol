/* DECODE.C - Decode a uuencoded or MIME encoded message
 *
 * Copyright 1993-2015 CIX Online Ltd, All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define  THIS_FILE   __FILE__

#include "main.h"
#include "palrc.h"
#include "resource.h"
#include <string.h>
#include <ctype.h>
#include "listview.h"
#include "amlib.h"
#include "ameol2.h"
#include "cookie.h"
#include <io.h>

#define  STMCH_HEADER         0
#define  STMCH_BOUNDARY       1
#define  STMCH_DATA           2
#define  STMCH_FINISHED       3
#define  STMCH_PREAMBLE       4
#define  STMCH_SEEKINGBEGIN   5
#define  STMCH_UUDATA         6

#define  LEN_PARTSUBJ         100
#define  LEN_RAWLINE          1024
#define  LEN_LINEBUF          ((LEN_RAWLINE)+2)

/* single-character decode */
#define DEC(c)  (((c) - ' ') & 077)

#define  MIME_FORMAT_NONE        0
#define  MIME_FORMAT_TEXT        'T'
#define  MIME_FORMAT_BASE64      'B'
#define  MIME_FORMAT_UUENCODE    'U'
#define  MIME_FORMAT_BINARY      'Y'
#define  MIME_FORMAT_BINHEX      'X'

typedef struct tagSTACKPTR {
   struct tagSTACKPTR * lpNext;           /* Pointer to next object on stack */
   LPVOID pData;                          /* Pointer to stacked data */
} STACKPTR, FAR * LPSTACKPTR;

static LPSTACKPTR lpStackPtrFirst = NULL; /* Pointer to top of stack */

char lszAttachDir[_MAX_PATH]; // !!SM!! 2.45 moved to be global from OpenOutputFile() as on second run it's filled with garbage otherwise.

BOOL FASTCALL StackString( char * );
BOOL FASTCALL UnstackString( char * );
BOOL FASTCALL StackObject( LPVOID );
LPVOID FASTCALL UnstackObject( void );

static BOOL fDefDlgEx = FALSE;            /* DefDlg recursion flag trap */

static BOOL fStripBit8;                   /* Strip bit 8 from data */
static BOOL fQuotedPrintable;             /* Data is quoted printable encoding */
static char FAR szBoundary[ 100 ];        /* Multipart content separator boundary */
static char FAR szOutBuf[ _MAX_PATH ];    /* Full path to filename */
static char FAR szFilename[ _MAX_FNAME ]; /* A filename */
static char szExt[ 5 ];                   /* The file extension */
static HNDFILE hfile;                     /* The active file handle */
static int nMimeFormat;                   /* MIME format */
static BOOL fAlternative = FALSE;         /* TRUE if we have an alternative part MIME message */
static int nState;                        /* Decode state machine state */
static int cBoundary;                     /* Count of boundary */
static int cIndex;                        /* Index of multipart MIME message */
static int cTotal;                        /* Total number of multipart MIME messages */
static int iTemp;                      /* Temp variable */
static HPSTR hpMsg;                    /* Handle of message text */
static HPSTR hpTxtBuf;                 /* Pointer to memory buffer */
static LPSTR lpszLineBuf;              /* Pointer to line buffer */
static LPSTR lpszLinePtr;              /* Pointer to current text on line */
static int len;                        /* Length of line */
static DWORD dwMsgLength;              /* Length of message so far */
static DWORD dwBufSize;                /* Size of buffer */
static BOOL fFreehpTxtBuf = FALSE; /* TRUE if we need to fremem hpTxtBuf */

typedef struct tagPTHSTRUCT {
   int iTotal;                         /* Number of elements in array */
   PTH FAR * pthArray;                 /* Pointer to array */
} PTHSTRUCT;

int fPromptForFilenames;               /* TRUE if we prompt for filenames */
int fStripHTML;                        /* TRUE if we strip out HTML from messages */

BOOL fReplaceAttachmentText;
BOOL fBackupAttachmentMail;

BOOL FASTCALL ParseAttachments( HWND, PTH, BOOL, BOOL );
BOOL FASTCALL ParseAttachmentHeader( char *, int *, int *, char * );
LPSTR FASTCALL ReadField( LPSTR,  char *, size_t );
void FASTCALL ParseTypeSubtype( char *, char *, char * );
LPSTR FASTCALL ParseValue( LPSTR, char * );
void FASTCALL AddToTextBuffer( LPSTR );
char FASTCALL Decode64( char );
void FASTCALL CreateTempFilename( char *, char * );
void FASTCALL ReadLine( void );
//BOOL FASTCALL OpenOutputFile( HWND, BOOL );
BOOL FASTCALL OpenOutputFile( HWND, BOOL, PTH ); /*!!SM!!*/
//BOOL FASTCALL RenameOutputFile( HWND );
BOOL FASTCALL RenameOutputFile( HWND, PTH pth ); /*!!SM!!*/
void FASTCALL ExtractDecodedFilename( char *, char * );
char * FASTCALL SkipWhitespace( char * );

BOOL EXPORT FAR PASCAL DecodeFilename( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL DecodeFilename_DlgProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL DecodeFilename_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL DecodeFilename_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT FAR PASCAL ManualDecode( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL ManualDecode_DlgProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL ManualDecode_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL ManualDecode_OnCommand( HWND, int, HWND, UINT );
void FASTCALL ManualDecode_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );

BOOL FASTCALL BackupMessage( PTH pth );
BOOL FASTCALL DecodeWriteMsgText( HNDFILE fh, PTH pth, BOOL fLFOnly );
BOOL FASTCALL DecodeWriteMsgTextLine( HNDFILE fh, LPSTR lpText, BOOL fLFOnly );
BOOL FASTCALL DecodeWriteHugeMsgText( HNDFILE fh, HPSTR hpText, DWORD dwSize );

/* This function decodes any binary attachment in the current
 * message. It starts by looking in the subject field to determine
 * whether this is part of a multi-part message and, if so, it builds
 * an array of message pointers to each part.
 */
BOOL FASTCALL DecodeMessage( HWND hwnd, BOOL fAutodecode )
{
   LPINT lpi;
   BOOL fOk;


   /* Get the handle of the selected message(s).
    */
   fOk = FALSE;
   if( lpi = GetSelectedItems( hwnd ) )
      {
      char szPartSubj[ LEN_PARTSUBJ ];
      MSGINFO msginfo;
      HWND hwndList;
      int iTotal = 0;
      int iIndex = 0;
	  PTH pthStart;

      BOOL fCtrl;

      /* Get this message details.
       */
      hwndList = GetDlgItem( hwnd, IDD_LIST );
      pthStart = (PTH)ListBox_GetItemData( hwndList, lpi[ 0 ] );
      Amdb_GetMsgInfo( pthStart, &msginfo );
      szFilename[ 0 ] = '\0';
      szPartSubj[ 0 ] = '\0';
      strcpy( szExt, "dat" );
      nMimeFormat = MIME_FORMAT_NONE;
      fAlternative = FALSE;
      fCtrl = GetKeyState( VK_CONTROL ) & 0x8000;
      cIndex = cTotal = 1;
      if( ( fCtrl || !ParseAttachmentHeader( msginfo.szTitle, &iIndex, &iTotal, szPartSubj ) ) && lpi[ 1 ] == LB_ERR )
         {
         /* Try to extract a filename from the
          * part subject.
          */
         ExtractDecodedFilename( szPartSubj, szFilename );

         if(strstr(szFilename, "=?"))
         {
            b64decodePartial(szFilename); // !!SM!!
         }
         /* Parse one message.
          */
         fOk = ParseAttachments( hwnd, pthStart, TRUE, TRUE );
         ShowUnreadMessageCount();
         }
      else
         {
         PTH FAR * pthArray;

         /* Try to extract a filename from the
          * part subject.
          */
         ExtractDecodedFilename( szPartSubj, szFilename );

         if(strstr(szFilename, "=?"))
         {
            b64decodePartial(szFilename); // !!SM!!
         }

         /* Initialise variables.
          */
         INITIALISE_PTR(pthArray);
         fOk = TRUE;
         hfile = HNDFILE_ERROR;
         if( lpi[ 1 ] != LB_ERR )
            {
            for( iTotal = 0; lpi[ iTotal ] != LB_ERR; ++iTotal );
            if( fNewMemory( (LPVOID FAR *)&pthArray, iTotal * sizeof( PTH ) ) )
               {
               PTHSTRUCT pths;
               register int c;

               for( c = 0; c < iTotal; ++c )
                  pthArray[ c ] = (PTH)ListBox_GetItemData( hwndList, lpi[ c ] );
               pths.pthArray = pthArray;
               pths.iTotal = iTotal;
               if( !Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_MANUALDECODE), ManualDecode, (LPARAM)(LPSTR)&pths ) )
                  {
                  FreeMemory( (LPVOID FAR *)&pthArray );
                  pthArray = NULL;
                  fOk = FALSE;
                  }
               iTotal = pths.iTotal;
               }
            }
         else if( iTotal > 0 && iTotal < 500 && iIndex <= iTotal )
            {
            /* Create an array to store the handles of each part of the
             * message we're planning to decode.
             */
            if( fNewMemory( (LPVOID FAR *)&pthArray, iTotal * sizeof( PTH ) ) )
               {
               char szThisPartSubj[ LEN_PARTSUBJ ];
               int iThisIndex;
               int iThisTotal;
               int cFound;
               PTH pth;
   
               /* Zero the array so that we can tell if we find any
                * missing bits or duplicate parts.
                */
               _fmemset( pthArray, 0, iTotal * sizeof( PTH ) );
   
               /* Save this part then scan back and forward looking
                * for the other parts.
                */
               cFound = 0;
   
               /* Handle the case where we start on 0/xx (ie. the cover message)
                */
               if( iIndex > 0 && !( msginfo.dwFlags & MF_HDRONLY ) )
                  {
                  pthArray[ iIndex - 1 ] = pthStart;
                  ++cFound;
                  }
   
               /* Unless we're on the last item scan forward.
                */
               if( iIndex < iTotal )
                  {
                  pth = pthStart;
                  while( cFound < iTotal && ( pth = Amdb_GetNextMsg( pth, VM_VIEWCHRON ) ) )
                     {
                     Amdb_GetMsgInfo( pth, &msginfo );
                     ParseAttachmentHeader( msginfo.szTitle, &iThisIndex, &iThisTotal, szThisPartSubj );
                     if( strcmp( szThisPartSubj, szPartSubj ) == 0 && iThisTotal == iTotal && iThisIndex <= iTotal && iThisIndex > 0 )
                        if( NULL == pthArray[ iThisIndex - 1 ] )
                           {
                           if( msginfo.dwFlags & MF_HDRONLY )
                              {
                              /* BUGBUG: This article is part of the sequence, but hasn't
                               * been retrieved yet. If fAutoDecode is enabled, mark this
                               * article for retrieval. If we're offline, we'll just tag
                               * the article and move on, otherwise we get the article
                               * immediately.
                               */
                              continue;
                              }
                           pthArray[ iThisIndex - 1 ] = pth;
                           ++cFound;
                           }
                     }
                  }
   
               /* If the index of the message we're on is not the first, then the
                * previous ones are most likely before this message, so scan back and
                * try to find them.
                */
               if( cFound < iTotal || iIndex > 1 )
                  {
                  pth = pthStart;
                  while( cFound < iTotal && ( pth = Amdb_GetPreviousMsg( pth, VM_VIEWCHRON ) ) )
                     {
                     Amdb_GetMsgInfo( pth, &msginfo );
                     ParseAttachmentHeader( msginfo.szTitle, &iThisIndex, &iThisTotal, szThisPartSubj );
                     if( strcmp( szThisPartSubj, szPartSubj ) == 0 && iThisTotal == iTotal && iThisIndex <= iTotal && iThisIndex > 0 )
                        if( NULL == pthArray[ iThisIndex - 1 ] )
                           {
                           if( msginfo.dwFlags & MF_HDRONLY )
                              {
                              /* BUGBUG: This article is part of the sequence, but hasn't
                               * been retrieved yet. If fAutoDecode is enabled, mark this
                               * article for retrieval. If we're offline, we'll just tag
                               * the article and move on, otherwise we get the article
                               * immediately.
                               */
                              continue;
                              }
                           pthArray[ iThisIndex - 1 ] = pth;
                           ++cFound;
                           }
                     }
                  }
   
               /* Check we've got all the bits. If not, offer the user the
                * chance to decode it manually.
                */
               if( cFound != iTotal )
                  {
                  if( IDNO == fMessageBox( hwnd, 0, GS(IDS_STR724), MB_YESNO|MB_ICONEXCLAMATION ) )
                     fOk = FALSE;
                  else
                     {
                     PTHSTRUCT pths;
                     int m, n;

                     /* Compact the pthArray to remove all NULL
                      * entries.
                      */
                     for( m = n = 0; m < iTotal; ++m )
                        if( NULL != pthArray[ m ] )
                           pthArray[ n++ ] = pthArray[ m ];
                     pths.pthArray = pthArray;
                     pths.iTotal = cFound;
                     if( !Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_MANUALDECODE), ManualDecode, (LPARAM)(LPSTR)&pths ) )
                        fOk = FALSE;
                     iTotal = pths.iTotal;
                     }
                  }
               }
            else
               OutOfMemoryError( hwnd, FALSE, FALSE );
            }

         /* We've got all the bits now, so decode them.
          */
         if( pthArray )
            {
            HCURSOR hOldCursor;

            hOldCursor = SetCursor( hWaitCursor );
            OfflineStatusText( GS(IDS_STR966) );
            for( iIndex = 0; fOk && iIndex< iTotal; ++iIndex )
               {
               fOk = ParseAttachments( hwnd, pthArray[ iIndex ], iIndex == 0, TRUE );
               if( fOk )
                  {
                  wsprintf( lpTmpBuf, GS(IDS_STR967), (LPSTR)szFilename, iIndex + 1 );
                  OfflineStatusText( lpTmpBuf );
                  }
               }

            /* Only if we succeeded do we create the attachments.
             */
            if( fOk )
               {
               for( iIndex = 0; fOk && iIndex< iTotal; ++iIndex )
                  {
                  Amdb_MarkMsgRead( pthArray[ iIndex ], TRUE );
                  SendMessage( hwnd, WM_UPDMSGFLAGS, 0, (LPARAM)pthArray[ iIndex ] );
                  }
               }
            ShowUnreadMessageCount();
            FreeMemory( (LPVOID FAR *)&pthArray );
            SetCursor( hOldCursor );
            }
         }
      FreeMemory( &lpi );

	  if (fOk){
		  Amdb_MarkMsgDecoded(pthStart);
	  }
      }


   return( fOk );
}

/* Given a string, this function tries to extract a filename from
 * the string. It uses the following rules. It looks for a string
 * of characters not including a space up to the first '.'. Then it
 * looks for up to three characters followed by a whitespace after
 * the '.'. If found, it assumes it has a filename.
 */
void FASTCALL ExtractDecodedFilename( char * pString, char * pFilename )
{
   while( *pString )
      {
      while( *pString && !ValidFileNameChr( *pString ) )
         ++pString;
      if( *pString )
         {
         char * pPossibleFilename;

         pPossibleFilename = pString;
         while( *pString && ValidFileNameChr( *pString ) )
            ++pString;
         if( *pString == '.' )
            {
            ++pString;
            if( ValidFileNameChr( *pString ) )
               {
               ++pString;
               if( ValidFileNameChr( *pString ) )
                  ++pString;
               if( ValidFileNameChr( *pString ) )
                  ++pString;
               if( !ValidFileNameChr( *pString ) )
                  {
                  /* Okay, we've got a seemingly valid
                   * filename, so copy it to the szFilename
                   * buffer.
                   */
                  while( pPossibleFilename < pString )
                     *pFilename++ = *pPossibleFilename++;
                  break;
                  }
               }
            }
         }
      }
   *pFilename = '\0';

}

/* This function parses the subject line to attempt to extract
 * a multi-part message number and count.
 */
BOOL FASTCALL ParseAttachmentHeader( char * pstrSubject, int * piIndex, int * piTotal, char * pszPartSubj )
{
   register int c;

   /* Scan for the start of the index/total field, copying everything we skip
    * into pszPartSubj which we will use to find the other parts.
    */
   for( c = 0; *pstrSubject; )
      {
      if( *pstrSubject == '(' || *pstrSubject == '[' || isdigit( *pstrSubject ) )
         {
         BOOL fDigitAtStart;
         char * pstrSave;

         /* Look and see if a index/total field appears here. If not,
          * we back up to pstrSave and keep scanning.
          */
         pstrSave = pstrSubject;
         if( *pstrSubject == '(' || *pstrSubject == '[' )
            {
            ++pstrSubject;
            while( *pstrSubject == ' ' )
               ++pstrSubject;
            fDigitAtStart = FALSE;
            }
         else
            fDigitAtStart = TRUE;
         if( isdigit( *pstrSubject ) )
            {
            *piIndex = 0;
            while( isdigit( *pstrSubject ) )
               *piIndex = ( *piIndex * 10 ) + ( *pstrSubject++ - '0' );
            while( *pstrSubject == ' ' )
               ++pstrSubject;
            if( *pstrSubject == '/' || ( *pstrSubject == 'o' && *(pstrSubject+1) == 'f' ) )
               {
               if( *pstrSubject != '/' )
                  ++pstrSubject;
               ++pstrSubject;
               while( *pstrSubject == ' ' )
                  ++pstrSubject;
               if( isdigit( *pstrSubject ) )
                  {
                  *piTotal = 0;
                  while( isdigit( *pstrSubject ) )
                     *piTotal = ( *piTotal * 10 ) + ( *pstrSubject++ - '0' );
                  if( *piTotal >= *piIndex )
                     {
                     while( *pstrSubject == ' ' )
                        ++pstrSubject;
                     if( fDigitAtStart || *pstrSubject == ')' || *pstrSubject == ']' )
                        {
                        pszPartSubj[ c ] = '\0';
                        return( TRUE );
                        }
                     }
                  }
               }
            }
         pstrSubject = pstrSave;
         }
      if( c < LEN_PARTSUBJ - 1 )
         pszPartSubj[ c++ ] = *pstrSubject;
      ++pstrSubject;
      }
    if( c < LEN_PARTSUBJ - 1 )
        pszPartSubj[ c ] = '\0';
    return( FALSE );
}

/* 
	Parses a MIME type and puts the appropriate extension in szExt
*/
void GetExtensionForMime(char* szType, char* szSubtype) {
	// Overwritten if something more specific is found below
	strcpy(szExt, "dat");

	if( _strcmpi( szType, "image" ) == 0) {
		if (_strcmpi(szSubtype, "jpeg") == 0) {
			strcpy( szExt, "jpg" );
		} else if (_strcmpi(szSubtype, "gif") == 0) {
			strcpy( szExt, "gif" );
		} else if (_strcmpi(szSubtype, "mpeg") == 0) {
			strcpy(szExt, "mpg");
		}
	}

	// szType is garbage if plain is the subtype
	if (_strcmpi(szSubtype, "plain") == 0) {
		strcpy(szExt, "txt");
	} else if (_strcmpi(szSubtype, "html") == 0) {
		strcpy(szExt, "html");
	}
}

/* This function parses an attachment in the specified message. It may be
 * a MIME or uuencoded attachment, but this function will handle both.
 */
#ifndef WIN32
#pragma optimize("", off)
#endif
BOOL FASTCALL ParseAttachments( HWND hwnd, PTH pth, BOOL fFirst, BOOL fFiles )
{
   HPSTR hpBuf;
   BOOL fOk;
   BOOL fHasAttachment;
   BOOL fSkipSection;
   BOOL fPreambled;
   BOOL fPartial;
   BOOL fDisableDecoding;
   BOOL fAlreadyEmit;
   char szType[ LEN_RFCTYPE+1 ];
   char szSubtype[ 100 ];

   /* First get the pointer to the actual message. If we can't get it
    * all, no point in continuing.
    */
   hpMsg = hpBuf = Amdb_GetMsgText( pth );
   if( NULL == hpMsg )
      {
      OutOfMemoryError( hwnd, FALSE, FALSE );
      return( FALSE );
      }

   /* We need a 1K line buffer.
    */
   if( !fNewMemory( &lpszLineBuf, LEN_LINEBUF ) )
      {
      Amdb_FreeMsgTextBuffer( hpMsg );
      OutOfMemoryError( hwnd, FALSE, FALSE );
      return( FALSE );
      }

   /* Initialise variables.
    */
   fStripBit8 = FALSE;
   fQuotedPrintable = FALSE;
   szBoundary[ 0 ] = '\0';
   szType[ 0 ] = '\0';
   hfile = HNDFILE_ERROR;
   hpTxtBuf = NULL;
   cBoundary = 0;
   dwMsgLength = 0;
   fOk = TRUE;
   fPreambled = FALSE;
   fHasAttachment = FALSE;
   fPartial = FALSE;
   fSkipSection = FALSE;
   fDisableDecoding = FALSE;
   fAlreadyEmit = FALSE;

   /* We'll use a state machine to step through each line. Start
    * with the header state, since that's what we'll have.
    */
   nState = STMCH_HEADER;
   while( *hpMsg && nState != STMCH_FINISHED )
      {
      register int c;

      /* Find the end of the line and NULL terminate it. This
       * makes working with strings much easier.
       */
      ReadLine();

      /* Now we've got a full line, so start parsing at the
       * current state.
       */
      switch( nState )
         {
         case STMCH_BOUNDARY:
         case STMCH_HEADER: {
            /* Save the header lines. Don't need anything else.
             */
            if( nState == STMCH_HEADER )
               if( memcmp( lpszLineBuf, "Lines:", 6 ) )
                  {
                  AddToTextBuffer( lpszLineBuf );
                  AddToTextBuffer( "\r\n" );
                  }

            /* Blank line? Switch to data state.
             */
            if( *lpszLinePtr == '\0' )
               {
               if( MIME_FORMAT_BASE64 == nMimeFormat && !fFirst )
                  nState = STMCH_DATA;
               else if( szBoundary[ 0 ] && !fPreambled )
                  {
                  nState = STMCH_PREAMBLE;
                  fPreambled = TRUE;
                  }
               else if( MIME_FORMAT_UUENCODE == nMimeFormat )
                  nState = fFirst ? STMCH_SEEKINGBEGIN : STMCH_UUDATA;
               else if( fPartial && cIndex == 1 )
                  nState = ( nState == STMCH_BOUNDARY ) ? STMCH_DATA : STMCH_BOUNDARY;
               else if( fPartial && cIndex > 1 )
                  nState = STMCH_DATA;
               else
                  nState = STMCH_DATA;
               break;
               }

            /* If this line doesn't start with a space, read the header type. If it
             * does, then we use the previous header type.
             */
            if( *lpszLinePtr != ' ' && *lpszLinePtr != '\t' )
               {
               for( c = 0; c < LEN_RFCTYPE && *lpszLinePtr && *lpszLinePtr != ':'; ++c )
                  szType[ c ] = *lpszLinePtr++;
               szType[ c ] = '\0';
               if( *lpszLinePtr != ':' )
                  break;
               ++lpszLinePtr;
               }
            else if( *szType == '\0' )
               {
               /* Bug somewhere! The first line of the header starts with a space,
                * so ignore the entire line.
                */
               continue;
               }

            /* Skip to the data.
             */
            while( *lpszLinePtr == ' ' && *lpszLinePtr == '\t' )
               ++lpszLinePtr;

            /* Looking for Content-Separator.
             */
            if( _strcmpi( szType, "Content-Type" ) == 0 )
               {
               char szField[ 40 ];

               lpszLinePtr = ReadField( lpszLinePtr, szField, sizeof( szField ) );
               ParseTypeSubtype( szField, szType, szSubtype );

               /* A multipart field. Just get the boundary for now.
                */
               if( _strcmpi( szType, "multipart" ) == 0 )
                  {
                  if( _strcmpi( szSubtype, "alternative" ) == 0 )
                     fAlternative = TRUE;
                  do {
                     char szValue[ 100 ];

                     lpszLinePtr = ParseValue( lpszLinePtr, szValue );
                     if( _strcmpi( szValue, "boundary" ) == 0 )
                        {
                        if( cBoundary++ > 0 )
                           StackString( szBoundary );
                        lpszLinePtr = ReadField( lpszLinePtr, szBoundary, sizeof( szBoundary ) );
                        }
                     }
                  while( *lpszLinePtr );
                  }

               /* Message field. Check for partial and handle it especially
                * since if we've no boundary, we can expect a header field inside
                * the message.
                */
               else if( _strcmpi( szType, "message" ) == 0 )
                  {
                  if( _strcmpi( szSubtype, "partial" ) == 0 )
                     {
                     cIndex = cTotal = 1;
                     do {
                        char szValue[ 20 ];
                        char sz2[ 10 ];

                        lpszLinePtr = ParseValue( lpszLinePtr, szValue );
                        if( _strcmpi( szValue, "number" ) == 0 )
                           {
                           lpszLinePtr = ReadField( lpszLinePtr, sz2, sizeof( sz2 ) );
                           cIndex = atoi( sz2 );
                           }
                        else if( _strcmpi( szValue, "total" ) == 0 )
                           {
                           lpszLinePtr = ReadField( lpszLinePtr, sz2, sizeof( sz2 ) );
                           cTotal = atoi( sz2 );
                           }
                        else
                           lpszLinePtr = ReadField( lpszLinePtr, NULL, 0 );
                        }
                     while( *lpszLinePtr );
                     fPartial = cIndex == 1;
                     }
                  }

               /* Text field.
                */
               else if( _strcmpi( szType, "text" ) == 0 )
                  {
                  /* Deal with text/html by stripping it out. When we
                   * later support HTML viewing, provide an option.
                   */
                  if( _strcmpi( szSubtype, "html" ) == 0 )
                     if( ( fAlternative && fStripHTML ) )
                        fSkipSection = TRUE;
                     else
                     {
                        nMimeFormat = MIME_FORMAT_TEXT;
                        fFiles = TRUE;
                     }
                  if( _strcmpi( szSubtype, "plain" ) == 0 && nState == STMCH_BOUNDARY )
                     nMimeFormat = MIME_FORMAT_TEXT;

                  /* Parse rest of field.
                   */
                  while( *lpszLinePtr )
                     {
                     char szValue[ 20 ];
   
                     lpszLinePtr = ParseValue( lpszLinePtr, szValue );
                     if( _strcmpi( szValue, "name" ) == 0 )
                        lpszLinePtr = ReadField( lpszLinePtr, szFilename, sizeof( szFilename ) );
                     else
                        lpszLinePtr = ReadField( lpszLinePtr, NULL, 0 );
                     }
                  }

               /* An image field.
                */
               else if( _strcmpi( szType, "image" ) == 0 )
                  {
                  /* Parse rest of field.
                   */
                  while( *lpszLinePtr )
                     {
                     char szValue[ 20 ];
   
                     lpszLinePtr = ParseValue( lpszLinePtr, szValue );
                     if( _strcmpi( szValue, "name" ) == 0 )
                        lpszLinePtr = ReadField( lpszLinePtr, szFilename, sizeof( szFilename ) );
                     else
                        lpszLinePtr = ReadField( lpszLinePtr, NULL, 0 );
                     }
                  }

               /* An application field.
                */
               else if( _strcmpi( szType, "application" ) == 0 )
                  {
                  char szValue[ 20 ];

                  /* Binhex?
                   */
                  if( _strcmpi( szSubtype, "mac-binhex40" ) == 0 )
                     {
                     /* BinHex format. We can't handle this yet, but when we
                      * do, remove the error message.
                      */
                     nMimeFormat = MIME_FORMAT_BINHEX;
                     if( fFiles )
                        fMessageBox( hwnd, 0, GS(IDS_STR1144), MB_OK|MB_ICONINFORMATION );
                     fOk = FALSE;
                     nState = STMCH_FINISHED;
                     break;
                     }
                  else if( _strcmpi( szSubtype, "rtf" ) == 0 )
                     {
                     /* RTF format
                      */
                     nMimeFormat = MIME_FORMAT_TEXT;
                     }
                  else if( _strcmpi( szSubtype, "octet-stream" ) == 0 && ( nMimeFormat != MIME_FORMAT_BASE64 ) )
                     {
                     /* Treat as text format
                      */
                     nMimeFormat = MIME_FORMAT_TEXT;
                     }
//                else if( _strcmpi( szSubtype, "ms-tnef" ) == 0 )
//                      fSkipSection = TRUE;

                  /* Note that name= in an application type is obsolete, but if
                   * we don't see a subsequent Content-Disposition, then we use it.
                   * This is necessary if the mailer was an older type.
                   */
                  lpszLinePtr = ParseValue( lpszLinePtr, szValue );
                  if( _strcmpi( szValue, "name" ) == 0 )
                     ReadField( lpszLinePtr, szFilename, sizeof( szFilename ) );
                  if(strstr(szFilename, "=?"))
                  {
                     b64decodePartial(szFilename); // !!SM!!
                  }

                  }
               }

            /* Looking for Content-Separator.
             */
            else if( _strcmpi( szType, "Content-Disposition" ) == 0 )
               {
               char szField[ 20 ];

               lpszLinePtr = ReadField( lpszLinePtr, szField, sizeof( szField ) );
               if( _strcmpi( szField, "attachment" ) == 0 || _strcmpi( szField, "inline" ) == 0 )
               {
                  if( ( _strcmpi( szField, "inline" ) == 0 ) && ( _strcmpi( szSubtype, "plain" ) == 0 || _strcmpi( szSubtype, "html" ) == 0 ) )
                  {
                     nState = STMCH_DATA;
                     fDisableDecoding = TRUE;
                     ReadLine();
                     break;
                  }
                  else
                  {
                     char szValue[ 20 ];

                     lpszLinePtr = ParseValue( lpszLinePtr, szValue );
                     if( _strcmpi( szValue, "filename" ) == 0 )
                     {
                        ReadField( lpszLinePtr, szFilename, sizeof( szFilename ) );
                        if(strstr(szFilename, "=?"))
                        {
                           b64decodePartial(szFilename); // !!SM!!
                        }
                     }
                  }
                  if( nMimeFormat == MIME_FORMAT_NONE )
                     nMimeFormat = MIME_FORMAT_TEXT;
               }
               }

            /* Look for Content-Transfer-Encoding. Basically, this just sets
             * a bunch of flags.
             */
            else if( _strcmpi( szType, "Content-Transfer-Encoding" ) == 0 )
               {
               char szField[ 40 ];

               /* Parse content transfer encoding type and set the
                * appropriate variables.
                */
               ReadField( lpszLinePtr, szField, sizeof( szField ) );
               if( _strcmpi( szField, "7bit" ) == 0 )
                  fStripBit8 = TRUE;
               else if( _strcmpi( szField, "Quoted-printable" ) == 0 )
                  fQuotedPrintable = TRUE;
               else if( _strcmpi( szField, "8bit" ) == 0 )
                  fStripBit8 = FALSE;
               else if( _strcmpi( szField, "Base64" ) == 0 && fFiles )
                  nMimeFormat = MIME_FORMAT_BASE64;
               else if( _strnicmp( szField, "x-uue", 5 ) == 0  && fFiles )
                  nMimeFormat = MIME_FORMAT_UUENCODE;
               else if( _strcmpi( szField, "Binary" ) == 0 )
                  nMimeFormat = MIME_FORMAT_BINARY;
               }
            break;
            }

         case STMCH_PREAMBLE:
         case STMCH_DATA:
            /* If a non-blank line and we were previously decoding
             * a Base64 message, stop doing it now.
             */
            if( fDisableDecoding )
               {
               nMimeFormat = MIME_FORMAT_NONE;
               fStripBit8 = FALSE;
               fQuotedPrintable = FALSE;
               fDisableDecoding = FALSE;
               }

            /* Check for -- followed by the boundary specified by the multi-part
             * mixed type. If found then what follows is a new header, but we use
             * the STMCH_BOUNDARY type to make it clear that we're inside the
             * message body and not the real header.
             */
            if( len > 2 && lpszLinePtr[0] == '-' && lpszLinePtr[1] == '-' && *szBoundary )
               {
               lpszLinePtr += 2;
               if( _fmemcmp( lpszLinePtr, szBoundary, strlen(szBoundary) ) == 0 )
                  {
                  /* Close any open file then add this
                   * attachment to the list of attachments.
                   */
                  if( HNDFILE_ERROR != hfile )
                     {
                     Amfile_Close( hfile );
                     hfile = HNDFILE_ERROR;
                     Amdb_CreateAttachment( pth, szOutBuf );
                     }

                  /* If the boundary is followed by '--' then we're finished. Ignore
                   * everything that follows this line.
                   */
                  lpszLinePtr += strlen(szBoundary);
                  if( lstrlen( lpszLinePtr ) == 2 && lpszLinePtr[0] == '-' && lpszLinePtr[1] == '-'  )
                     {
                     if( --cBoundary == 0 )
                        nState = STMCH_FINISHED;
                     else
                        {
                        UnstackString( szBoundary );
                        nState = STMCH_BOUNDARY;
                        }
                     szType[ 0 ] = '\0';
                     szSubtype[ 0 ] = '\0';
                     }
                  else
                     {
                     nState = STMCH_BOUNDARY;
                     nMimeFormat = MIME_FORMAT_NONE;
                     fStripBit8 = FALSE;
                     fQuotedPrintable = FALSE;
                     szFilename[ 0 ] = '\0';
                     szType[ 0 ] = '\0';
                     szSubtype[ 0 ] = '\0';
                     }
                  fSkipSection = FALSE;
                  break;
                  }
               }

            /* If we're in the preamble, ignore this line.
             */
            if( nState == STMCH_PREAMBLE || fSkipSection )
               break;

            /* If blank line AND we're doing Base 64 decoding, stop doing
             * Base 64 decoding and switch back to boundary scanning.
             */
            if( ( MIME_FORMAT_BASE64 == nMimeFormat ) && *lpszLineBuf == '\0' && *( lpszLineBuf + 1 ) == '-' )
               {
               fDisableDecoding = TRUE;
               break;
               }              

            /* Could be uuencoded section header?
             */
            if( _fstrnicmp( lpszLineBuf, "section ", 8 ) == 0 )
               {
               LPSTR lpszFile;

               if( ( lpszFile = _fstrstr( lpszLineBuf, "file" ) ) != NULL )
                  {
                  register int i;

                  lpszFile += 4;
                  while( *lpszFile && *lpszFile == ' ' )
                     ++lpszFile;
                  for( i = 0; ( *lpszFile == '.' || ValidFileNameChr( *lpszFile ) ) && i < _MAX_FNAME; ++i )
                     szFilename[ i ] = *lpszFile++;
                  szFilename[ i ] = '\0';
                  }
               break;
               }

            /* If we have a filename, then open the file for writing.
             */
            if( MIME_FORMAT_NONE != nMimeFormat )
               if( HNDFILE_ERROR == hfile && *szFilename != '\0' && fFiles )
                  {
				  GetExtensionForMime(szType, szSubtype);
                  if( !OpenOutputFile( hwnd, fFirst, pth ) )
                     {
                     fOk = FALSE;
                     nState = STMCH_FINISHED;
                     break;
                     }
                  OfflineStatusText( GS(IDS_STR968), (LPSTR)szFilename );
                  fHasAttachment = TRUE;
                  }

            if ( MIME_FORMAT_TEXT == nMimeFormat && _strcmpi( szSubtype, "html") == 0 ) {
                if( HNDFILE_ERROR == hfile) {
                  GetExtensionForMime(szType, szSubtype);

                  if( !OpenOutputFile( hwnd, fFirst, pth ) )
                     {
                     fOk = FALSE;
                     nState = STMCH_FINISHED;
                     break;
                     }
                  OfflineStatusText( GS(IDS_STR968), (LPSTR)szFilename );
                  fHasAttachment = TRUE;
                  }

                 ASSERT( hfile != HNDFILE_ERROR );
                 c = strlen(lpszLinePtr);
                 while( Amfile_Write( hfile, lpszLinePtr, c ) != (UINT)c )
                   if( !DiskWriteError( hwnd, szFilename, TRUE, FALSE ) )
                     {
                       nState = STMCH_FINISHED;
                       fOk = FALSE;
                       break;
                     }
                   break;

            }

            /* If the Base64 flag is set, then this is another line of Base64 data, so
             * parse it and add it to the output file.
             */
            if( MIME_FORMAT_BASE64 == nMimeFormat )
               {

               c = DecodeLine64( lpszLinePtr, lpszLinePtr, LEN_RAWLINE );

               if( _strcmpi( szSubtype, "plain" ) == 0) {
                   // We found some base64 plain text to write, now don't write other data.
                   fAlreadyEmit = TRUE;
                   lpszLinePtr[c] = 0;
                   AddToTextBuffer( lpszLinePtr );
			   } else {
               if( HNDFILE_ERROR == hfile )
			     {
				  GetExtensionForMime(szType, szSubtype);
                  if( !OpenOutputFile( hwnd, fFirst, pth ) )
                     {
                     fOk = FALSE;
                     nState = STMCH_FINISHED;
                     break;
                     }
			     }

				 ASSERT( hfile != HNDFILE_ERROR );
			     while( Amfile_Write( hfile, lpszLinePtr, c ) != (UINT)c )
                   if( !DiskWriteError( hwnd, szFilename, TRUE, FALSE ) )
                     {
					   nState = STMCH_FINISHED;
                       fOk = FALSE;
                       break;
                     }
			   }

               break;
               }

            /* Writing to a text file.
             */
            if( ( nMimeFormat == MIME_FORMAT_TEXT ) && HNDFILE_ERROR != hfile )
               {
               int length;

               if( fQuotedPrintable )
                  {
                  register int s, d;

                  /* For quoted printable, scan the text and convert all
                   * lines of the form =xx where xx is a 2-digit hex number
                   * into the raw ASCII equivalent.
                   */
                  for( s = d = 0; lpszLinePtr[ s ]; )
                  {
                     if( lpszLinePtr[ s ] == '=' )
                        {
                        int nValue = 0;

                        ++s;
                        if( isxdigit( lpszLinePtr[ s ] ) )
                           {
                           if( isalpha( lpszLinePtr[ s ] ) )
                              nValue = 16 * ( ( tolower( lpszLinePtr[ s ] ) - 'a' ) + 10 );
                           else
                              nValue = 16 * ( lpszLinePtr[ s ] - '0' );
                           ++s;
                           if( isxdigit( lpszLinePtr[ s ] ) )
                              {
                              if( isalpha( lpszLinePtr[ s ] ) )
                                 nValue += ( ( tolower( lpszLinePtr[ s ] ) - 'a' ) + 10 );
                              else
                                 nValue += ( lpszLinePtr[ s ] - '0' );
                              ++s;
                              }
                           lpszLinePtr[ d++ ] = (BYTE)nValue;
                           }
                        else if( lpszLinePtr[ s ] )
                           nValue = lpszLinePtr[ s++ ];
                        else
                           /* An '=' at the end of the line means don't terminate this
                            * line with a newline.
                            */
                        lpszLinePtr[ d++ ] = (BYTE)nValue;
                        }
                     else
                        lpszLinePtr[ d++ ] = lpszLinePtr[ s++ ];
                  }
//                strcat( lpszLinePtr, "\r\n" );
//                d +=2;
                  lpszLinePtr[ d++ ] = '\r';
                  lpszLinePtr[ d++ ] = '\n';
                  lpszLinePtr[ d ] = '\0';
                  fHasAttachment = TRUE;
                  }
               else
                  strcat( lpszLinePtr, "\r\n" );
               length = strlen( lpszLinePtr );
               while( (int)Amfile_Write( hfile, lpszLinePtr, length ) != length )
                  if( !DiskWriteError( hwnd, szFilename, TRUE, FALSE ) )
                     {
                     nState = STMCH_FINISHED;
                     fOk = FALSE;
                     break;
                     }
               break;
               }

            /* Otherwise write the line to the output file and apply 7-bit
             * and quoted printable encoding as we go along.
             */
            if( _fstrnicmp( lpszLineBuf, "begin ", 6 ) != 0 )
               {
               BOOL fNewline;

               fNewline = TRUE;
               if( fStripBit8 )
                  {
                  for( c = 0; lpszLinePtr[ c ]; ++c )
                     lpszLinePtr[ c ] &= 0x7F;
                  }
               else if( fQuotedPrintable )
                  {
                  register int s, d;

                  /* For quoted printable, scan the text and convert all
                   * lines of the form =xx where xx is a 2-digit hex number
                   * into the raw ASCII equivalent.
                   */
                  for( s = d = 0; lpszLinePtr[ s ]; )
                     if( lpszLinePtr[ s ] == '=' )
                        {
                        int nValue = 0;

                        ++s;
                        if( isxdigit( lpszLinePtr[ s ] ) )
                           {
                           if( isalpha( lpszLinePtr[ s ] ) )
                              nValue = 16 * ( ( tolower( lpszLinePtr[ s ] ) - 'a' ) + 10 );
                           else
                              nValue = 16 * ( lpszLinePtr[ s ] - '0' );
                           ++s;
                           if( isxdigit( lpszLinePtr[ s ] ) )
                              {
                              if( isalpha( lpszLinePtr[ s ] ) )
                                 nValue += ( ( tolower( lpszLinePtr[ s ] ) - 'a' ) + 10 );
                              else
                                 nValue += ( lpszLinePtr[ s ] - '0' );
                              ++s;
                              }
                           }
                        else if( lpszLinePtr[ s ] )
                           nValue = lpszLinePtr[ s++ ];
                        else
                           /* An '=' at the end of the line means don't terminate this
                            * line with a newline.
                            */
                           fNewline = FALSE;
                        lpszLinePtr[ d++ ] = (BYTE)nValue;
                        }
                     else
                        lpszLinePtr[ d++ ] = lpszLinePtr[ s++ ];
                  lpszLinePtr[ d ] = '\0';
                  fHasAttachment = TRUE;
                  }

               /* Anything else, and the line is left as it stands except that
                * we shove a newline onto the end.
                */
               if( fNewline )
                  lstrcat( lpszLinePtr, "\r\n" );
               if (fAlreadyEmit == FALSE)
                  AddToTextBuffer( lpszLinePtr );
               break;
               }

         case STMCH_SEEKINGBEGIN:
            if( _fstrnicmp( lpszLineBuf, "begin ", 6 ) == 0 )
               {
               /* Found the start of the text, so skip to the
                * filename and collect it.
                */
               lpszLinePtr += 6;
               while( *lpszLinePtr == ' ' )
                  ++lpszLinePtr;
               if( isdigit( *lpszLinePtr ) )
                  {
                  while( isdigit( *lpszLinePtr ) )
                     ++lpszLinePtr;
                  while( *lpszLinePtr == ' ' )
                     ++lpszLinePtr;
   
                  /* Collect the filename. It may not be valid for DOS, so
                   * convert it if so.
                   */
                  lpszLinePtr = GetFileBasename( lpszLinePtr );
                  for( c = 0; c < _MAX_PATH && *lpszLinePtr; ++c )
                     szFilename[ c ] = *lpszLinePtr++;
                  szFilename[ c ] = '\0';
                  nState = STMCH_UUDATA;
                  nMimeFormat = MIME_FORMAT_UUENCODE;

                  /* Close any open file then add this
                   * attachment to the list of attachments.
                   */
                  if( HNDFILE_ERROR != hfile )
                     {
                     Amfile_Close( hfile );
                     hfile = HNDFILE_ERROR;
                     Amdb_CreateAttachment( pth, szOutBuf );
                     }
                  }
               }
            else
               {
               lstrcat( lpszLinePtr, "\r\n" );
               AddToTextBuffer( lpszLinePtr );
               }
            break;

         case STMCH_UUDATA: {
            int cExpected;
            int d, c, n;
            int cActual;

            /* Skip obvious headers and things.
             */
            if( _fstrnicmp( lpszLineBuf, "section ", 8 ) == 0 )
               break;
            if( _fstrnicmp( lpszLineBuf, "begin ", 6 ) == 0 )
               break;
            if( _fstrnicmp( lpszLineBuf, "end", 3 ) == 0 )
               {
               nState = STMCH_DATA;
               break;
               }

            /* Open the file if we haven't already done so. We
             * must have a successfully opened file to be able to
             * progress.
             */
            if( hfile == HNDFILE_ERROR )
               {
			   GetExtensionForMime(szType, szSubtype);
               if( !OpenOutputFile( hwnd, fFirst, pth ) )
                  {
                  nState = STMCH_FINISHED;
                  fOk = FALSE;
                  break;
                  }
               fHasAttachment = TRUE;
               }

            /* Read the line width.
             */
            n = DEC( *lpszLinePtr );
            if( n <= 0 || *lpszLinePtr == '\0' )
               break;

            /* Calculate expected # of chars and pad if necessary
             */
            cExpected = ( ( n + 2 ) / 3 ) << 2;
            cActual = strlen( lpszLineBuf ) - 1;
            if( cActual > cExpected || ( cExpected - cActual ) > 3  )
               {
                  fMessageBox( hwnd, 0, GS(IDS_STR1249), MB_OK|MB_ICONEXCLAMATION );
                  nState = STMCH_FINISHED;
                  fOk = FALSE;
                  break;
               }
            for( c = cActual + 1; c <= cExpected; c++ )
               lpszLineBuf[ c ] = ' ';
            ++lpszLinePtr;
            for( d = c = 0; n > 0; c += 4 )
               {
               int c1, c2, c3;
            
               c1 = DEC( lpszLinePtr[ c ] ) << 2 | DEC( lpszLinePtr[ c + 1 ] ) >> 4;
               c2 = DEC( lpszLinePtr[ c + 1 ] ) << 4 | DEC( lpszLinePtr[ c + 2 ] ) >> 2;
               c3 = DEC( lpszLinePtr[ c + 2 ]) << 6 | DEC( lpszLinePtr[ c + 3 ] );
               if( n >= 1 )
                  lpszLinePtr[ d++ ] = c1;
               if( n >= 2 )
                  lpszLinePtr[ d++ ] = c2;
               if( n >= 3 )
                  lpszLinePtr[ d++ ] = c3;
               n -= 3;
               }
            while( Amfile_Write( hfile, lpszLinePtr, d ) != (UINT)d )
               if( !DiskWriteError( hwnd, szFilename, TRUE, FALSE ) )
                  {
                  nState = STMCH_FINISHED;
                  fOk = FALSE;
                  break;
                  }
            break;
            }
         }
      }

   /* Did we decode anything?
    */
   if( ( nState == STMCH_PREAMBLE || nState == STMCH_SEEKINGBEGIN ) && fFiles )
      {
      fMessageBox( hwnd, 0, GS(IDS_STR726), MB_OK|MB_ICONEXCLAMATION );
      fOk = FALSE;
      }

   /* If anything in the text buffer, use it to
    * replace the current message.
    */
   if( fBackupAttachmentMail )
      BackupMessage( pth );
   if( fOk && fReplaceAttachmentText && hpTxtBuf )
   {
      CURMSG curmsg;
      MSGINFO msginfo;
      int count = 0;
      HPSTR hpNewBuf = NULL;
      Ameol2_GetCurrentMsg( &curmsg );
      Amdb_GetMsgInfo( curmsg.pth, &msginfo );
      while( *hpTxtBuf && *hpTxtBuf != '\r' && *hpTxtBuf != '\n' )
      {
         count++;
         *hpTxtBuf++;
      }
      while( *hpTxtBuf && ( *hpTxtBuf == '\r' || *hpTxtBuf == '\n' ) )
      {
         count++;
         *hpTxtBuf++;
      }
      wsprintf( lpTmpBuf, "Memo #%lu (%lu)\r\n", msginfo.dwMsg, dwMsgLength );
      dwMsgLength = dwMsgLength - count + lstrlen( lpTmpBuf );
      wsprintf( lpTmpBuf, "Memo #%lu (%lu)\r\n", msginfo.dwMsg, dwMsgLength );
      hpNewBuf = ConcatenateStrings( lpTmpBuf, hpTxtBuf );
      Amdb_ReplaceMsgText( pth, hpNewBuf, dwMsgLength );
      if( hpNewBuf )
         FreeMemory32( &hpNewBuf );

   }

   /* Free up memory.
    */
   FreeMemory( &lpszLineBuf );
   Amdb_FreeMsgTextBuffer( hpBuf );
   if( hpTxtBuf && fFreehpTxtBuf && !fReplaceAttachmentText)
      FreeMemory32( &hpTxtBuf );

   /* Close any open file. Don't delete the file
    * as it may be part of a multipart file!
    */
   if( HNDFILE_ERROR != hfile )
      {
      Amfile_Close( hfile );
      if( fOk )
         Amdb_CreateAttachment( pth, szOutBuf );
      hfile = HNDFILE_ERROR;
      }

   /* If error, then we don't have an attachment.
    */
   if( !fOk )
      fHasAttachment = FALSE;
   return( fOk );
}
//#pragma optimize("", on)

/* This function opens the output file for any decoded
 * attachment.
 */
BOOL FASTCALL OpenOutputFile( HWND hwnd, BOOL fFirst, PTH pth)
{
   if( fFirst )
   {
      LPSTR lpTempFilename;

      if( Amdb_GetTopicType( Amdb_TopicFromMsg(pth) ) == FTYPE_MAIL )
         Amdb_GetCookie( Amdb_TopicFromMsg(pth), MAIL_ATTACHMENTS_COOKIE, (LPSTR)&lszAttachDir, pszAttachDir, _MAX_PATH - 1 );
      else if (Amdb_GetTopicType( Amdb_TopicFromMsg(pth) ) == FTYPE_NEWS )
         Amdb_GetCookie( Amdb_TopicFromMsg(pth), NEWS_ATTACHMENTS_COOKIE, (LPSTR)&lszAttachDir, pszAttachDir, _MAX_PATH - 1 );
      else
         wsprintf(lszAttachDir, "%s", pszAttachDir);
                                                  
      lpTempFilename = GetFileBasename( szFilename );
      for( iTemp = 0; iTemp < _MAX_PATH && *lpTempFilename; ++iTemp )
         szFilename[ iTemp ] = *lpTempFilename++;
      szFilename[ iTemp ] = '\0';
   }

   if( szFilename[ 0 ] )
      {
      if( !Ameol2_MakeLocalFileName( hwnd, szFilename, szOutBuf, TRUE ) )
         return( FALSE );
      strcpy( szFilename, szOutBuf );
      wsprintf( szOutBuf, "%s\\%s", (LPSTR)&lszAttachDir, (LPSTR)szFilename );
      if( fFirst )
      {
         /* If the output file already exists, prompt before overwriting it.
          */
T1:         if( Amfile_QueryFile( szOutBuf ) )
         {
            LPSTR lpStr;
            int cbStr;

            INITIALISE_PTR(lpStr);
            cbStr = strlen( szOutBuf ) + strlen( GS(IDS_STR673) ) + 1;
            if( fNewMemory( &lpStr, cbStr ) )
            {
               int r;

               wsprintf( lpStr, GS(IDS_STR673), szOutBuf );
               r = fDlgMessageBox( hwnd, idsFDLPROMPT, IDDLG_DECODEEXISTS, szOutBuf, 0, 0 );
               FreeMemory( &lpStr );
               if( IDNO == r || IDCANCEL == r )
               {
                  if( IDCANCEL == r || IDYES == fMessageBox( hwnd, 0, "Are you sure, this will abort all other decodes?", MB_YESNO|MB_ICONEXCLAMATION ) )
                     return( FALSE );
                  else
                     goto T1;
               }
               if( IDD_RENAME == r )
               {
                     strcpy(szOutBuf, szFilename);
                     if( !RenameOutputFile( hwnd, pth ) )
                        return( FALSE );
                     else
                        goto T1;
               }
            }
         }
         hfile = Amfile_Create( szOutBuf, 0 );
      }
      else
      {
         if( ( hfile = Amfile_Open( szOutBuf, AOF_WRITE ) ) == HNDFILE_ERROR )
            FileOpenError( hwnd, szOutBuf, FALSE, FALSE );
         else
            Amfile_SetPosition( hfile, 0L, ASEEK_END );
      }
   }
   else
   {
      CreateTempFilename( szFilename, szExt );
      wsprintf( szOutBuf, "%s\\%s", (LPSTR)lszAttachDir, (LPSTR)szFilename );
      hfile = Amfile_Create( szOutBuf, 0 );
   }

   /* Check that we actually got the file opened.
    */
   if( hfile == HNDFILE_ERROR )
   {
      FileCreateError( hwnd, szOutBuf, FALSE, FALSE );
      szOutBuf[0] = '\x0';
      if( RenameOutputFile( hwnd, pth ) )
         goto T1;
      return( FALSE );
   }

   /* Create some text to add to the text buffer.
    */
   _strupr( szOutBuf );
   wsprintf( lpTmpBuf, GS(IDS_STR725), (LPSTR)szOutBuf );
   AddToTextBuffer( lpTmpBuf );
   return( TRUE );
}

/* This function prompts for a replacement name for the file
 * being decoded.
 */
BOOL FASTCALL RenameOutputFile( HWND hwnd, PTH pth )
{
   OPENFILENAME ofn;
   static char strFilters[] = "All Files (*.*)\0*.*\0\0";
   char lszAttachDir[_MAX_PATH];

   if( Amdb_GetTopicType( Amdb_TopicFromMsg(pth) ) == FTYPE_MAIL )                                                            // !!SM!! 2033
      Amdb_GetCookie( Amdb_TopicFromMsg(pth), MAIL_ATTACHMENTS_COOKIE, (LPSTR)&lszAttachDir, pszAttachDir, _MAX_PATH - 1 );  // !!SM!! 2033
   else if (Amdb_GetTopicType( Amdb_TopicFromMsg(pth) ) == FTYPE_NEWS )                                                       // !!SM!! 2033
      Amdb_GetCookie( Amdb_TopicFromMsg(pth), NEWS_ATTACHMENTS_COOKIE, (LPSTR)&lszAttachDir, pszAttachDir, _MAX_PATH - 1 );  // !!SM!! 2033
   else                                                                                                                       // !!SM!! 2033
      wsprintf(lszAttachDir, "%s", pszAttachDir);                                                                            // !!SM!! 2033

// szOutBuf[ 0 ] = '\0';
   ofn.lStructSize = sizeof( OPENFILENAME );
   ofn.hwndOwner = hwnd;
   ofn.hInstance = NULL;
   ofn.lpstrFilter = strFilters;
   ofn.lpstrCustomFilter = NULL;
   ofn.nMaxCustFilter = 0;
   ofn.nFilterIndex = 1;
   ofn.lpstrFile = szOutBuf;
   ofn.nMaxFile = sizeof( szOutBuf );
   ofn.lpstrFileTitle = NULL;
   ofn.nMaxFileTitle = 0;
   ofn.lpstrInitialDir = lszAttachDir;
   ofn.lpstrTitle = "Save Decoded Attachment";
   ofn.Flags = OFN_NOCHANGEDIR|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST;
   ofn.nFileOffset = 0;
   ofn.nFileExtension = 0;
   ofn.lpstrDefExt = NULL;
   ofn.lCustData = 0;
   ofn.lpfnHook = NULL;
   ofn.lpTemplateName = 0;
   if( GetSaveFileName( &ofn ) )
      {
      strcpy( szFilename, szOutBuf + ofn.nFileOffset );
      return( TRUE );
      }
   return( FALSE );
}

/* This function appends the specified line to the memory buffer.
 */
void FASTCALL AddToTextBuffer( LPSTR lpstr )
{
   /* Cast to long to avoid wrap round because MAX_MSGSIZE is close to 0xFFFF
    */
   if( hpTxtBuf == NULL )
      {
      dwBufSize = 64000;
      fNewMemory32( &hpTxtBuf, dwBufSize );
      fFreehpTxtBuf = TRUE;
      }
   else if( dwMsgLength + (DWORD)lstrlen( lpstr ) >= dwBufSize )
      {
      dwBufSize += 2048;
      fResizeMemory32( &hpTxtBuf, dwBufSize );
      fFreehpTxtBuf = FALSE;
      }
   if( hpTxtBuf )
      {
      hmemcpy( hpTxtBuf + dwMsgLength, lpstr, lstrlen( lpstr ) );
      dwMsgLength += lstrlen( lpstr );
      hpTxtBuf[ dwMsgLength ] = '\0';
      }
}

/* This function does Base 64 decoding of the NULL terminated line at
 * lpszSrc to the buffer at lpszDest and returns the number of
 * characters decoded.
 */
int FASTCALL DecodeLine64( LPSTR lpszSrc, LPSTR lpszDest, int cbDest )
{
   register int c;
   register int d;

   --cbDest;
   for( c = d = 0; lpszSrc[ d ]; d += 4 )
      {
      char cTempHigh;
      char cTempLow;
      char cLetter1, cLetter2, cLetter3, cLetter4;

      cLetter1 = Decode64( lpszSrc[ d ] );
      cLetter2 = Decode64( lpszSrc[ d + 1 ] );
      cLetter3 = Decode64( lpszSrc[ d + 2 ] );
      cLetter4 = Decode64( lpszSrc[ d + 3 ] );
      if( cLetter1 == -2 || cLetter2 == -2 )
         break;
      cTempHigh = ( cLetter1 << 2 ) & 0xFC;
      cTempLow = ( cLetter2 >> 4 ) & 0x03;
      if( c < cbDest )
         lpszDest[ c++ ] = cTempHigh | cTempLow;
      if( cLetter3 == -2 )
         break;
      cTempHigh = ( cLetter2 << 4 ) & 0xF0;
      cTempLow = ( cLetter3 >> 2 ) & 0x0F;
      if( c < cbDest )
         lpszDest[ c++ ] = cTempHigh | cTempLow;
      if( cLetter4 == -2 )
         break;
      cTempHigh = ( cLetter3 << 6 ) & 0xC0;
      if( c < cbDest )
         lpszDest[ c++ ] = cTempHigh | cLetter4;
      }
   return( c );
}

/* This function converts an ASCII letter into a Base 64
 * 6-bit code.
 */
char FASTCALL Decode64( char ch )
{
   if( ch >= 'A' && ch <= 'Z' )
      return( ch - 'A' );
   if( ch >= 'a' && ch <= 'z' )
      return( ( ch - 'a' ) + 26 );
   if( ch >= '0' && ch <= '9' )
      return( ( ch - '0' ) + 52 );
   if( ch == '+' )
      return( 62 );
   if( ch == '/' )
      return( 63 );
   if( ch == '=' )
      return( -2 );
   return( -1 );
}

/* This function attempts to read a subfield from a Mime header fields. A
 * field is assumed to be terminated by either the end of the line or a
 * semicolon. Quotes are handled.
 */
LPSTR FASTCALL ReadField( LPSTR lpszLPtr, char * pszField, size_t max )
{
   register size_t c;
   BOOL fQuote;

   /* Skip leading spaces.
    */
   lpszLPtr = SkipWhitespace( lpszLPtr );

   /* If we're at a terminator, read the next line
    */
   if( *lpszLPtr == ';' )
      {
      ReadLine();
      lpszLPtr = SkipWhitespace( lpszLineBuf );
      }

   /* Scan up to max characters into pszField.
    */
   fQuote = FALSE;
   c = 0;
   while( *lpszLPtr )
      {
      if( !fQuote && *lpszLPtr == ';' )
         break;
      else if( *lpszLPtr == '"' )
         fQuote = !fQuote;
      else if( pszField )
         {
         if( c < max - 1 )
            pszField[ c++ ] = *lpszLPtr;
         }
      ++lpszLPtr;
      }
   if( pszField )
      pszField[ c ] = '\0';

   /* Skip trailing spaces.
    */
   return( SkipWhitespace( lpszLPtr ) );
}

/* This function skips whitespace.
 */
char * FASTCALL SkipWhitespace( char * lpszLPtr )
{
   while( *lpszLPtr == ' ' || *lpszLPtr == '\t' || *lpszLPtr == '\r' || *lpszLPtr == '\n' )
      ++lpszLPtr;
   return( lpszLPtr );
}

/* This function splits a content type field into a type and subtype.
 */
void FASTCALL ParseTypeSubtype( char * pszField, char *pszType, char * pszSubtype )
{
   /* Parse the type field.
    */
   while( *pszField && *pszField != '/' )
      *pszType++ = *pszField++;
   *pszType = '\0';

   /* Skip the separator. It SHOULD be there, but
    * we can't guarantee it (faulty MIME mailer).
    */
   if( *pszField == '/' )
      ++pszField;

   /* Parse the type field.
    */
   while( *pszField && *pszField != '/' )
      *pszSubtype++ = *pszField++;
   *pszSubtype = '\0';
}

/* This function reads a value field. A value field is a name followed by
 * an '=' symbol. On return, the name is written to the pszValue buffer and
 * the return value is lpszLPtr updated to point to after the '='.
 */
LPSTR FASTCALL ParseValue( LPSTR lpszLPtr, char * pszValue )
{
   /* Skip whitespace.
    */
   lpszLPtr = SkipWhitespace( lpszLPtr );

   /* Skip terminators.
    */
   if( *lpszLPtr == ';' )
      {
      lpszLPtr = SkipWhitespace( ++lpszLPtr );
      if( *lpszLPtr == '\0' )
         {
         ReadLine();
         lpszLPtr = SkipWhitespace( lpszLineBuf );
         }
      }

   /* Read the value name.
    */
   while( *lpszLPtr && *lpszLPtr != '=' && *lpszLPtr != ';' /*&& *lpszLPtr != '\"'*/) //!!SM!! 2038/2039
      *pszValue++ = *lpszLPtr++;
   *pszValue = '\0';

   /* Skip the '='
    */
   if( *lpszLPtr == '=' )
      ++lpszLPtr;
   return( lpszLPtr );
}

/* This function creates a temporary filename using the packed form
 * of the current system date and time, and the specified extension.
 */
void FASTCALL CreateTempFilename( char * pszFilename, char * pszReqdExt )
{
   wsprintf( pszFilename, "%4.4X%4.4X.%s", Amdate_GetPackedCurrentDate(), Amdate_GetPackedCurrentTime(), (LPSTR)pszReqdExt );
}

/* Reads another line from the message buffer into lpszLineBuf.
 */
void FASTCALL ReadLine( void )
{
   register int c;

   /* Read a line.
    */
   c = 0;
   do {
      while( c < LEN_RAWLINE && *hpMsg && *hpMsg != '\r' && *hpMsg != '\n' )
         lpszLineBuf[ c++ ] = *hpMsg++;

      /* Move hpMsg forward to start at the next line in case we
       * need to peek at it later.
       */
      if( *hpMsg == '\r' )
         ++hpMsg;
      if( *hpMsg == '\n' )
         ++hpMsg;

      /* Peek at next line. If it starts with a space or TAB and we're
       * in header mode, concatenate it to the end of the current line.
       */
      if( STMCH_HEADER == nState && c == 0 )
         break;
      if( ( *hpMsg != ' ' && *hpMsg != '\t' ) || STMCH_HEADER != nState )
         break;

      /* Skip whitespace.
       */
      if( c < LEN_RAWLINE )
         lpszLineBuf[ c++ ] = '\r';
      if( c < LEN_RAWLINE )
         lpszLineBuf[ c++ ] = '\n';
      while( *hpMsg == ' ' || *hpMsg == '\t' )
         {
         if( c < LEN_RAWLINE )
            lpszLineBuf[ c++ ] = ' ';
         ++hpMsg;
         }
      }
   while( *hpMsg );

   /* End of line
    */
   lpszLineBuf[ c ] = '\0';
   len = c;
   lpszLinePtr = lpszLineBuf;
}

/* This function handles the DecodeFilename dialog box.
 */
BOOL EXPORT CALLBACK DecodeFilename( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = DecodeFilename_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the DecodeFilename
 *  dialog.
 */
LRESULT FASTCALL DecodeFilename_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, DecodeFilename_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, DecodeFilename_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsDECODEFILENAME );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL DecodeFilename_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndEdit;

   /* Save pointer to filename buffer
    */
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* Set the default filename.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
   Edit_LimitText( hwndEdit, _MAX_FNAME );
   Edit_SetText( hwndEdit, (LPSTR)lParam );
   EnableControl( hwnd, IDOK, *(LPSTR)lParam );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL DecodeFilename_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EDIT: {
         HWND hwndEdit;

         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndEdit ) > 0 );
         break;
         }

      case IDOK: {
         LPSTR lpszFilename;
         HWND hwndEdit;

         lpszFilename = (LPSTR)GetWindowLong( hwnd, DWL_USER );
         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         Edit_GetText( hwndEdit, lpszFilename, _MAX_FNAME+1 );
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* This function handles the ManualDecode dialog box.
 */
BOOL EXPORT CALLBACK ManualDecode( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = ManualDecode_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the ManualDecode
 *  dialog.
 */
LRESULT FASTCALL ManualDecode_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, ManualDecode_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, ManualDecode_OnCommand );
      HANDLE_MSG( hwnd, WM_DRAWITEM, ManualDecode_OnDrawItem );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsMANUALDECODE );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL ManualDecode_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndList;
   PTHSTRUCT FAR * lppths;
   register int c;
   SIZE size;
   RECT rc;
   HDC hdc;

   /* Default to Yes for this file.
    */
   GetClientRect( GetDlgItem( hwnd, IDD_LIST ), &rc );
   hdc = GetDC( hwnd );

   /* Save pointer to filename buffer
    */
   lppths = (PTHSTRUCT FAR *)lParam;
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* Store PTH handles in the listbox.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   for( c = 0; c < lppths->iTotal; ++c )
      {
      MSGINFO msginfo;
      int index;

      Amdb_GetMsgInfo( lppths->pthArray[ c ], &msginfo );
      strcpy( lpTmpBuf, msginfo.szTitle );
      AdjustNameToFit( hdc, lpTmpBuf, rc.right, TRUE, &size );
      index = ListBox_InsertString( hwndList, -1, lpTmpBuf );
      ListBox_SetItemData( hwndList, index, (LPARAM)lppths->pthArray[ c ] );
      }
   ListBox_SetCurSel( hwndList, 0 );
   ReleaseDC( hwnd, hdc );

   /* Enable buttons.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_UP ), hInst, IDB_BUTTONUP );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_DOWN ), hInst, IDB_BUTTONDOWN );
   EnableControl( hwnd, IDD_UP, FALSE );
   EnableControl( hwnd, IDD_DOWN, lppths->iTotal > 1 );
   return( TRUE );
}

/* This function handles the WM_DRAWITEM command.
 */
void FASTCALL ManualDecode_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItemStruct )
{
   switch( lpDrawItemStruct->CtlID )
      {
      case IDD_LIST:
         break;
      }
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL ManualDecode_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_REMOVE: {
         HWND hwndList;
         int index;
         int count;

         /* Remove this item from the list box.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         index = ListBox_GetCurSel( hwndList );
         ASSERT( index > 0 );
         count = ListBox_DeleteString( hwndList, index );
         if( index == count )
            --index;
         ListBox_SetCurSel( hwndList, index );

         /* Enable controls as appropriate.
          */
         EnableControl( hwnd, IDD_UP, index > 0 );
         EnableControl( hwnd, IDD_DOWN, count > 1 );
         EnableControl( hwnd, IDOK, count > 0 );
         EnableControl( hwnd, IDD_REMOVE, count > 0 );
         break;
         }

      case IDD_UP: {
         MSGINFO msginfo;
         HWND hwndList;
         PTH pthThis;
         PTH pthPrev;
         int count;
         int index;

         /* First get the PTHs of the selected and previous items,
          * then delete them.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         index = ListBox_GetCurSel( hwndList );
         ASSERT( index > 0 );
         pthThis = (PTH)ListBox_GetItemData( hwndList, index );
         pthPrev = (PTH)ListBox_GetItemData( hwndList, index - 1 );
         ListBox_DeleteString( hwndList, index );
         ListBox_DeleteString( hwndList, index - 1 );
         index = index - 1;

         /* Insert the selected item back in first.
          */
         Amdb_GetMsgInfo( pthThis, &msginfo );
         index = ListBox_InsertString( hwndList, index, msginfo.szTitle );
         ListBox_SetItemData( hwndList, index, pthThis );
         ++index;

         /* Then insert the previous item, so the items are
          * effectively swapped.
          */
         Amdb_GetMsgInfo( pthPrev, &msginfo );
         index = ListBox_InsertString( hwndList, index, msginfo.szTitle );
         ListBox_SetItemData( hwndList, index, pthPrev );

         /* Set the selection again and update the buttons.
          */
         ListBox_SetCurSel( hwndList, --index );
         count = ListBox_GetCount( hwndList );
         EnableControl( hwnd, IDD_UP, index > 0 );
         EnableControl( hwnd, IDD_DOWN, index < count - 1 );
         break;
         }

      case IDD_DOWN: {
         MSGINFO msginfo;
         HWND hwndList;
         PTH pthThis;
         PTH pthNext;
         int count;
         int index;

         /* First get the PTHs of the selected and previous items,
          * then delete them.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         index = ListBox_GetCurSel( hwndList );
         count = ListBox_GetCount( hwndList );
         ASSERT( count > 0 );
         ASSERT( index < count - 1 );
         pthThis = (PTH)ListBox_GetItemData( hwndList, index );
         pthNext = (PTH)ListBox_GetItemData( hwndList, index + 1 );
         ListBox_DeleteString( hwndList, index + 1 );
         ListBox_DeleteString( hwndList, index );

         /* Insert the next item back in first.
          */
         Amdb_GetMsgInfo( pthNext, &msginfo );
         index = ListBox_InsertString( hwndList, index, msginfo.szTitle );
         ListBox_SetItemData( hwndList, index, pthNext );
         ++index;

         /* Then insert the selected item, so the items are
          * effectively swapped.
          */
         Amdb_GetMsgInfo( pthThis, &msginfo );
         index = ListBox_InsertString( hwndList, index, msginfo.szTitle );
         ListBox_SetItemData( hwndList, index, pthThis );

         /* Set the selection again and update the buttons.
          */
         ListBox_SetCurSel( hwndList, index );
         EnableControl( hwnd, IDD_UP, index > 0 );
         EnableControl( hwnd, IDD_DOWN, index < count - 1 );
         break;
         }

      case IDD_LIST:
         if( codeNotify == LBN_SELCHANGE )
            {
            int index;
            int count;

            index = ListBox_GetCurSel( hwndCtl );
            count = ListBox_GetCount( hwndCtl );
            EnableControl( hwnd, IDD_UP, index > 0 );
            EnableControl( hwnd, IDD_DOWN, index < count - 1 );
            }
         break;

      case IDOK: {
         HWND hwndList;
         PTHSTRUCT FAR * lppths;
         int count;
         register int i;

         /* Dereference the PTHSTRUCT structure.
          */
         lppths = (PTHSTRUCT FAR *)GetWindowLong( hwnd, DWL_USER );

         /* Now fill the pthArray list from the listbox, this time
          * in order.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         count = ListBox_GetCount( hwndList );
         for( i = 0; i < count; ++i )
            lppths->pthArray[ i ] = (PTH)ListBox_GetItemData( hwndList, i );
         lppths->iTotal = count;
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}


/* This function adds the specified string to the
 * stack
 */
BOOL FASTCALL StackString( char * pszStr )
{
   char * pszNewStr;

   INITIALISE_PTR(pszNewStr);
   if( fNewMemory( &pszNewStr, strlen(pszStr) + 1 ) )
      {
      strcpy( pszNewStr, pszStr );
      return( StackObject( pszNewStr ) );
      }
   return( FALSE );
}

/* This function removes a string from the stack.
 */
BOOL FASTCALL UnstackString( char * pszStr )
{
   char * pszNewStr;

   pszNewStr = UnstackObject();
   strcpy( pszStr, pszNewStr );
   FreeMemory( &pszNewStr );
   return( TRUE );
}

/* This function adds a memory object to the stack.
 */
BOOL FASTCALL StackObject( LPVOID lpObject )
{
   LPSTACKPTR lpStackPtr;

   INITIALISE_PTR(lpStackPtr);
   if( fNewMemory( &lpStackPtr, sizeof(STACKPTR) ) )
      {
      lpStackPtr->lpNext = lpStackPtrFirst;
      lpStackPtrFirst = lpStackPtr;
      lpStackPtr->pData = lpObject;
      return( TRUE );
      }
   return( FALSE );
}

/* This function removes a memory object from the stack.
 */
LPVOID FASTCALL UnstackObject( void )
{
   LPSTACKPTR lpStackPtr;
   LPVOID lpObject;

   lpObject = lpStackPtrFirst->pData;
   lpStackPtr = lpStackPtrFirst;
   lpStackPtrFirst = lpStackPtrFirst->lpNext;
   FreeMemory( &lpStackPtr );
   return( lpObject );
}

BOOL FASTCALL BackupMessage( PTH pth )
{
   BOOL fOk = FALSE;
   HNDFILE fh;
   AM_DATE date;

   Amdate_GetCurrentDate( &date );
   Amuser_GetActiveUserDirectory( lpTmpBuf, _MAX_PATH );
   wsprintf( lpPathBuf, "%s\\at%02u%02u%02u.dat", lpTmpBuf, date.iDay, date.iMonth, ( date.iYear % 100 ) );
   if( ( fh = Amfile_Open( lpPathBuf, AOF_WRITE ) ) != HNDFILE_ERROR )
      Amfile_SetPosition( fh, 0L, ASEEK_END );
   else
      fh = Amfile_Create( lpPathBuf, AOF_WRITE );
   if( fh != HNDFILE_ERROR )
      {
      fOk = DecodeWriteMsgText( fh, pth, FALSE );
      Amfile_Close( fh );
      }
   return( fOk );
}

BOOL FASTCALL DecodeWriteMsgText( HNDFILE fh, PTH pth, BOOL fLFOnly )
{
   HPSTR hpText;
   HPSTR lpSrc;
   HPSTR lpDest;
   DWORD dwSize = 0;

   /* Strip out CR from the message and compute the actual size.
    */
   hpText = lpDest = lpSrc = Amdb_GetMsgText( pth );
   if( hpText )
      {
      dwSize = 0;
      while( *lpSrc )
         {
         if( !( *lpSrc == 13 && fLFOnly ) )
            {
            *lpDest++ = *lpSrc;
            ++dwSize;
            }
         ++lpSrc;
         }
      *lpDest = '\0';
      }

   /* If the associated message text was found, write it. Otherwise insert
    * a message saying that it couldn't be found.
    */
   if( hpText ) {
      if( !DecodeWriteHugeMsgText( fh, hpText, dwSize ) )
         return( FALSE );
      Amdb_FreeMsgTextBuffer( hpText );
      }
   else {
      if( !DecodeWriteMsgTextLine( fh, "xxx", fLFOnly ) )
         return( FALSE );
      }
   if( !DecodeWriteMsgTextLine( fh, "", fLFOnly ) )
      return( FALSE );
   return( TRUE );
}

BOOL FASTCALL DecodeWriteMsgTextLine( HNDFILE fh, LPSTR lpText, BOOL fLFOnly )
{
   register UINT n;
   char chCRLF[2] = { 13, 10 };
   char chLF[1] = { 10 };

   n = lstrlen( lpText );
   while( Amfile_Write( fh, lpText, n ) != n ) {
      if( !DiskSaveError( GetFocus(), fh, TRUE, FALSE ) )
         return( FALSE );
      }
   if( !fLFOnly )
      while( Amfile_Write( fh, (LPSTR)&chCRLF, 2 ) != 2 )
         {
         if( !DiskSaveError( GetFocus(), fh, TRUE, FALSE ) )
            return( FALSE );
         }
   else
      while( Amfile_Write( fh, (LPSTR)&chLF, 1 ) != 1 )
         {
         if( !DiskSaveError( GetFocus(), fh, TRUE, FALSE ) )
            return( FALSE );
         }
   return( TRUE );
}

BOOL FASTCALL DecodeWriteHugeMsgText( HNDFILE fh, HPSTR hpText, DWORD dwSize )
{
   while( (DWORD)Amfile_Write32( fh, hpText, dwSize ) != dwSize )
      {
      if( !DiskSaveError( GetFocus(), fh, TRUE, FALSE ) )
         return( FALSE );
      }
   return( TRUE );
}
