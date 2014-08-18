/* RUNSCRPT.C - Executes an Ameol script
 *
 * Copyright 1993-2014 CIX Online Ltd, All Rights Reserved
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

#include "main.h"
#include "palrc.h"
#include "resource.h"
#include <io.h>
#include "amlib.h"
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include "parse.h"
#include "amevent.h"
#include "intcomms.h"
#include "hxfer.h"
#include "ameol2.h"
#include <dos.h>
#include "cix.h"
#include "lbf.h"
#include "log.h"

#define  TK_EOF               0
#define  TK_PAUSE          1
#define  TK_PUT               2
#define  TK_WAITFOR           3
#define  TK_STR               4
#define  TK_NUMBER            5
#define  TK_USERNAME          7
#define  TK_PASSWORD          8
#define  TK_STATUS            9
#define  TK_DOWNLOAD          10
#define  TK_UPLOAD            11
#define  TK_COPY              12
#define  TK_LSHIFT            13
#define  TK_DELETE            14
#define  TK_CLEAR          15
#define  TK_CLOSELOG          16
#define  TK_END               17
#define  TK_HANGUP            18
#define  TK_HOLDLOG           19
#define  TK_OPENLOG           20
#define  TK_REM               21
#define  TK_RESUMELOG         22
#define  TK_INCLUDE           23
#define  TK_SUCCESS           24
#define  TK_OLRINDEX          25
#define  TK_PUTNOCR           26
#define  TK_IF             27
#define  TK_ENDIF          28
#define  TK_LPAREN            29
#define  TK_RPAREN            30
#define  TK_COMMA          31
#define  TK_EQ             32
#define  TK_NEQ               33
#define  TK_LE             34
#define  TK_GE             35
#define  TK_LT             36
#define  TK_GT             37
#define  TK_NOT               38
#define  TK_BREAK          39
#define  TK_CONTINUE          40
#define  TK_WHILE          41
#define  TK_ENDWHILE          42
#define  TK_LASTERROR         43
#define  TK_RECOVER           44
#define  TK_SET               45
#define  TK_TIMEOUT           46
#define  TK_ADD               47
#define  TK_RENAME            48
#define  TK_GETTIME           49
#define  TK_OVERWRITE         50
#define  TK_IMPORT            51
#define  TK_RECORD            52
#define  TK_LOG               53
#define  TK_VERSION           54
#define TK_AMEOLUPDATE        55


#define  MAX_NAME          14
#define  MAX_WHILE            10

/* Table of built-in keywords.
 */
struct tagKWORDS { char * psKeyWord; int nID; } KWORDS[] = {
   "break",    TK_BREAK,
   "clear",    TK_CLEAR,
   "closelog",    TK_CLOSELOG,
   "continue",    TK_CONTINUE,
   "copy",        TK_COPY,
   "delete",      TK_DELETE,
   "download",    TK_DOWNLOAD,
   "end",         TK_END,
   "endif",    TK_ENDIF,
   "endwhile",    TK_ENDWHILE,
   "gettime",     TK_GETTIME,
   "hangup",      TK_HANGUP,
   "holdlog",     TK_HOLDLOG,
   "if",       TK_IF,
   "import",      TK_IMPORT,
   "include",     TK_INCLUDE,
   "lasterror",   TK_LASTERROR,
   "log",         TK_LOG,
   "name",        TK_USERNAME,
   "openlog",     TK_OPENLOG,
   "overwrite",   TK_OVERWRITE,
   "password",    TK_PASSWORD,
   "pause",    TK_PAUSE,
   "put",         TK_PUT,
   "putnocr",     TK_PUTNOCR,
   "record",      TK_RECORD,
   "recover",     TK_RECOVER,
   "rem",         TK_REM,
   "rename",      TK_RENAME,
   "resumelog",   TK_RESUMELOG,
   "set",         TK_SET,
   "status",      TK_STATUS,
   "success",     TK_SUCCESS,
   "timeout",     TK_TIMEOUT,
   "upload",      TK_UPLOAD,
   "version",     TK_VERSION,
   "waitfor",     TK_WAITFOR,
   "while",    TK_WHILE,
   "ameolupdate",  TK_AMEOLUPDATE,
   NULL,          0
   };

typedef struct tagLOOPSTACK {
   LONG pos;
   int cLine;
} LOOPSTACK;

extern BOOL fIsAmeol2Scratchpad;

extern AMAPI_VERSION FAR amvLatest;       /* Latest Version information from COSY*/

int nLastError;                                 /* Last error code */

static char szGoAmeolversion[] = "go ameolversion\n";
static char szGoAmeolupdate[]  = "go ameolupdate\n";

static int iValue;                              /* Number read from script */
static char FAR sBuf[ 512 ];                    /* String buffer */
static int cLine;                               /* Current line number */
static char FAR szFileName[ _MAX_PATH ];        /* Script filename */
static char * lpLex;                            /* Next character in script file */
static BOOL fHasToken = FALSE;                  /* TRUE if we have a token */
static int nPushedBackToken;                    /* Pushed token */
static LOOPSTACK FAR aWhileStack[ MAX_WHILE ];  /* Loop stack */
static int nWhileStackPtr = 0;                  /* Loop stack pointer */
static LPCOMMDEVICE lpcdevCur;                  /* Current comm device handle */

#define  UngetToken(n)  { if( (n) != TK_EOF ) { fHasToken = TRUE; nPushedBackToken = (n); } }

static int FASTCALL GetToken( void );
static int FASTCALL ReadName( char, char *, int );
static void NEAR CDECL ScriptError( LPSTR, ... );
static int FASTCALL GetExpression( LPCOMMDEVICE );
static int FASTCALL GetInteger( void );
static int FASTCALL GetOperand( LPCOMMDEVICE );
static BOOL FASTCALL ExitLoop( LPVOID, int, int );

#ifdef WIN32
static BOOL FASTCALL GetPrivilege( void );
#endif

int EXPORT CALLBACK ScriptFileTransferStatus( LPFILETRANSFERINFO );
void FASTCALL ShowDownloadStatistics( LPFILETRANSFERINFO );

/* This function opens the specified script file and executes the commands
 * contained therein.
 */
BOOL FASTCALL ExecuteScript( LPCOMMDEVICE lpcdev, LPSTR lpFileName, BOOL fShowError )
{
   char sz[ 200 ];
   HNDFILE fh;
   BOOL fEnd;
   LPLBF hBuffer;
   int nLineLength;

   /* Open the script file.
    */
   MakePath( lpPathBuf, pszScriptDir, lpFileName );
   if( HNDFILE_ERROR == ( fh = Amfile_Open( lpPathBuf, AOF_READ ) ) )
      {
      if( !fQuitAfterConnect && fShowError )
         {
         wsprintf( sz, GS(IDS_STR457), lpFileName );
         fMessageBox( GetFocus(), 0, sz, MB_OK|MB_ICONEXCLAMATION );
         }
      return( FALSE );
      }
   fRunningScript = TRUE;
   hBuffer = Amlbf_Open( fh, AOF_READ );
   cLine = 1;
   strcpy( szFileName, lpFileName );
   fEnd = FALSE;
   lpcdevCur = lpcdev;
   while( !fEnd && Amlbf_Read( hBuffer, sz, sizeof( sz ) - 1, &nLineLength, NULL, &fIsAmeol2Scratchpad ) )
      {
      int nToken;

      lpLex = sz;
      fHasToken = FALSE;
      switch( nToken = GetToken() )
         {
         case TK_IMPORT:
            if( GetToken() != TK_STR )
               ScriptError( GS(IDS_STR458) );
            else if( Amfile_QueryFile( sBuf ) )
               CIXImportAndArchive( hwndFrame, sBuf, PARSEFLG_ARCHIVE|PARSEFLG_REPLACECR );
            break;

         case TK_END:
            fEnd = TRUE;
            break;

         case TK_UPLOAD:
            nLastError = 0;
            if( GetToken() != TK_STR )
               ScriptError( GS(IDS_STR458) );
            else {
               static char * wstr1[] = { "\x18", "ERROR" };
               static char * wstr2[] = { "M:", "Ml:", "R:", "Rf:", "MAIL TO", "Newsnet:", "ip>" };
               register int r;
               HXFER pftp;

               /* Get the protocol to use for CIX
                */
               pftp = GetFileTransferProtocolHandle( szCixProtocol );
               if( NULL == pftp )
                  pftp = GetFileTransferProtocolHandle( "ZMODEM" );
               if( ( r = Amcomm_WaitForString( lpcdev, 2, wstr1, lpcdev->lpcd->nTimeout ) ) == 0 )
                  {
                  if( !SendFileViaProtocol( pftp, lpcdev, ScriptFileTransferStatus, hwndFrame, sBuf, TRUE ) )
                     nLastError = 1;
                  }
               else {
                  wsprintf( sz, GS(IDS_STR459), (LPSTR)sBuf );
                  Amevent_AddEventLogItem( ETYP_SEVERE|ETYP_COMMS, sz );
                  nLastError = 1;
                  }
               Amcomm_WaitForString( lpcdev, 7, wstr2, lpcdev->lpcd->nTimeout );
               }
            break;

         case TK_REM:      /* Ignore rest of line */
            break;

         case TK_SUCCESS:
            RemoveObItem( GetInteger() );
            break;

         case TK_GETTIME: {
            register int c;
         #ifdef WIN32
            SYSTEMTIME st;
         #else
            struct _dostime_t dos_time;
            struct _dosdate_t dos_date;
         #endif
            AM_DATE date;
            AM_TIME time;
            static char szTime[] = { 'r','u','n',' ','d','a','t','e','t','i','m','e',13 };
            char ch;
            char chWait;

            szTime[ 12 ] = chWait = 10;
            Amcomm_WriteString( lpcdev, szTime, sizeof( szTime ) );
            Amcomm_WaitFor( lpcdev, chWait );
            Amcomm_Get( lpcdev, &ch, lpcdev->lpcd->nTimeout );
            c = 0;
            if( ch == 10 )
               Amcomm_Get( lpcdev, &ch, lpcdev->lpcd->nTimeout );
            while( ( ch != 13 && ch != 10 ) && c < 30 )
               {
               sBuf[ c++ ] = ch;
               Amcomm_Get( lpcdev, &ch, lpcdev->lpcd->nTimeout );
               }
            sBuf[ c ] = '\0';
            ParseExactCIXTime( sBuf, &date, &time );

            /* Set the system time */
         #ifdef WIN32
            if( GetPrivilege() )
               {
               GetLocalTime( &st );
               wsprintf( lpTmpBuf, "Date/time correction: old setting was %02u/%02u/%04u %02u:%02u:%02u", st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute, st.wSecond );
               WriteToBlinkLog( lpTmpBuf );
               st.wHour = time.iHour;
               st.wMinute = time.iMinute;
               st.wSecond = time.iSecond;
               st.wYear = date.iYear;
               st.wMonth = date.iMonth;
               st.wDayOfWeek = date.iDayOfWeek;
               st.wDay = date.iDay;
               SetLocalTime( &st );
               wsprintf( lpTmpBuf, "Date/time correction: new setting is %02u/%02u/%04u %02u:%02u:%02u", st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute, st.wSecond );
               WriteToBlinkLog( lpTmpBuf );
               }
            else
               Amevent_AddEventLogItem( ETYP_MESSAGE|ETYP_COMMS, GS(IDS_STR679) );
         #else
            dos_time.hour = (unsigned char)time.iHour;
            dos_time.minute = (unsigned char)time.iMinute;
            dos_time.second = (unsigned char)time.iSecond;
            dos_time.hsecond = 0;
            _dos_settime( &dos_time );
            dos_date.day = (unsigned char)date.iDay;
            dos_date.month = (unsigned char)date.iMonth;
            dos_date.year = (unsigned)date.iYear;
            dos_date.dayofweek = (unsigned char)date.iDayOfWeek;
            _dos_setdate( &dos_date );
         #endif
            break;
            }

         case TK_LOG:
            /* Write to the blink log.
             */
            if( GetToken() != TK_STR )
               ScriptError( GS(IDS_STR458) );
            else
               WriteToBlinkLog( sBuf );
            break;

         case TK_RECORD:
            /* Record a downloaded file.
             */
            if( GetToken() != TK_STR )
               ScriptError( GS(IDS_STR458) );
            else
               {
               char szConfName[ LEN_CONFNAME+1 ];
               char szTopicName[ LEN_TOPICNAME+1 ];
               register int c;
               char * pBuf;

               /* Extract the conference name.
                */
               pBuf = sBuf;
               for( c = 0; *pBuf && *pBuf != '/'; )
                  {
                  if( c < LEN_CONFNAME+1 ) //FS#154 2054
                     szConfName[ c++ ] = *pBuf;
                  ++pBuf;
                  }
               szConfName[ c ] = '\0';
               if( *pBuf == '/' )
                  ++pBuf;
               for( c = 0; *pBuf; )
                  {
                  if( c < LEN_TOPICNAME )
                     szTopicName[ c++ ] = *pBuf;
                  ++pBuf;
                  }
               szTopicName[ c ] = '\0';

               /* Get the parameter.
                */
               if( GetToken() != TK_STR )
                  ScriptError( GS(IDS_STR458) );
               else
                  {
                  char * pBasename;
                  char * pPathname;
                  DWORD dwSize;
                  HNDFILE fh;
                  PCL pcl;
                  PTL ptl;

                  /* Open the file we downloaded.
                   */
                  dwSize = 0;
                  if( HNDFILE_ERROR != ( fh = Amfile_Open( sBuf, AOF_READ ) ) )
                     {
                     dwSize = Amfile_GetFileSize( fh );
                     Amfile_Close( fh );
                     }

                  /* Get the file basename and pathname and separate
                   * them by placing a NULL after the final '/'.
                   */
                  pBasename = GetFileBasename( sBuf );
                  if( pBasename > sBuf )
                     *( pBasename-1 ) = '\0';
                  pPathname = sBuf;

                  /* Get a handle to the topic.
                   */
                  if( NULL != ( pcl = Amdb_OpenFolder( NULL, szConfName ) ) )
                     if( NULL != ( ptl = Amdb_OpenTopic( pcl, szTopicName ) ) )
                        {
                        char szFileName[ _MAX_FNAME ];

                        /* Write a record to the FPR file.
                         */
                        Amdb_GetInternalTopicName( ptl, szFileName );
                        strcat( szFileName, ".FPR" );
                        if( HNDFILE_ERROR == ( fh = Ameol2_OpenFile( szFileName, DSD_FLIST, AOF_WRITE ) ) )
                           fh = Ameol2_CreateFile( szFileName, DSD_FLIST, 0 );
                        if( HNDFILE_ERROR != fh );
                           {
                           Amfile_SetPosition( fh, 0L, ASEEK_END );
                           wsprintf( lpTmpBuf, "%s\t%lu\t%s\r\n", pBasename, dwSize, pPathname );
                           Amfile_Write( fh, lpTmpBuf, strlen( lpTmpBuf ) );
                           Amfile_Close( fh );
                           }
                        }

                  /* Write to the log as well.
                   */
                  wsprintf( lpTmpBuf, GS(IDS_STR1096), pBasename, szConfName, szTopicName );
                  WriteToBlinkLog( lpTmpBuf );
                  }
               }        
            break;

         case TK_SET:
            switch( GetToken() )
               {
               case TK_TIMEOUT:
                  /* BUGBUG: Unfinished code! */
                  break;

               case TK_RECOVER:
                  Amuser_WritePPInt( szSettings, "RecoverScratchpad", GetInteger() );
                  break;
               }
            break;

         case TK_INCLUDE:
            if( GetToken() != TK_STR )
               ScriptError( GS(IDS_STR458) );
            else {
               char * lpLexTmp;
               int cLineTmp;

               lpLexTmp = lpLex;
               cLineTmp = cLine;
               ExecuteScript( lpcdev, sBuf, TRUE );
               fRunningScript = TRUE;
               lpLex = lpLexTmp;
               cLine = cLineTmp;
               }
            break;

         case TK_HANGUP:
            break;

         case TK_DOWNLOAD: {
            static char * wstr1[] = { "\x18", "File not found", "is empty", "y/n)? ",
                                      "No scratchpad", "nothing to download", "Error",
                                      "File does not exist" };
            static char * wstr2[] = { "ERROR:", "M:", "Ml:", "R:", "Rf:", "y/n)? ", "Newsnet:", "ip>",
                                      "Download succeeded", "File(s) skipped.", "Cancelled by user" };
            BOOL fSkipErrorCheck;
            WORD wDownloadFlags;
            char * pszFilename;
            BOOL fDownloadOk;
            HXFER pftp;
            register int r;
            int nToken;
            BOOL fCancelledByUser;

            /* Get the filename.
             */
            if( GetToken() != TK_STR )
               {
               ScriptError( GS(IDS_STR458) );
               break;
               }
            pszFilename = _strdup( sBuf );

            /* Get the protocol to use for CIX
             */
            pftp = GetFileTransferProtocolHandle( szCixProtocol );

            /* Get the download mode.
             */
            wDownloadFlags = ZDF_DEFAULT;
            fDownloadOk = FALSE;
            nLastError = 0;
            if( ( nToken = GetToken() ) == TK_COMMA )
               {
               switch( GetToken() )
                  {
                  case TK_OVERWRITE:
                     wDownloadFlags = ZDF_OVERWRITE;
                     break;

                  case TK_RENAME:
                     wDownloadFlags = ZDF_RENAME;
                     break;

                  default:
                     ScriptError( GS(IDS_STR458) );
                     break;
                  }
               }
            else
               UngetToken( nToken );

            /* Wait for the prompt.
             */
            fSkipErrorCheck = FALSE;
            switch( r = Amcomm_WaitForString( lpcdev, 8, wstr1, lpcdev->lpcd->nTimeout ) )
               {
               case 0:
                  fDownloadOk = ReceiveFileViaProtocol( pftp, lpcdev, ScriptFileTransferStatus, hwndFrame, pszFilename, TRUE, wDownloadFlags );
                  break;

               case 1:
                  wsprintf( sz, GS(IDS_STR469), (LPSTR)pszFilename );
                  Amevent_AddEventLogItem( ETYP_SEVERE|ETYP_COMMS, sz );
                  break;

               case 2:
                  Amevent_AddEventLogItem( ETYP_SEVERE|ETYP_COMMS, GS(IDS_STR470) );
                  break;

               case 3:
                  Amcomm_WriteChar( lpcdev, 'Y' );
                  Amcomm_WriteChar( lpcdev, CR );
                  break;

               case 5:
                  fSkipErrorCheck = TRUE;
                  break;

               case 4:
                  fSkipErrorCheck = TRUE;
                  Amevent_AddEventLogItem( ETYP_SEVERE|ETYP_COMMS, GS(IDS_STR471) ); // Scratchpad Not Found Error
                  break;
               }

            nLastError = !fSkipErrorCheck;
            fCancelledByUser = FALSE;
TK1:        if( ( r = Amcomm_WaitForString( lpcdev, 11, wstr2, lpcdev->lpcd->nTimeout ) ) == 5 )
               {
               Amcomm_WriteChar( lpcdev, (char)( nLastError ? 'N' : 'Y' ) );
               Amcomm_WriteChar( lpcdev, CR );
               goto TK1; 
               }
            else if( r == 10 )
               {
               nLastError = 1;
               fCancelledByUser = TRUE;
               goto TK1;
               }
            else if( r == 9 )
               {
               if( !fCancelledByUser )
                  nLastError = 0;
               goto TK1;
               }
            else if( r == 8 ) 
               {
               if( !fCancelledByUser )
                  nLastError = 0;
               goto TK1;
               }
            else if( r == 0 )
               {
               if( !fDownloadOk )
                  {
                  wsprintf( sz, GS(IDS_STR472), pszFilename ); // Download Error
                  Amevent_AddEventLogItem( ETYP_SEVERE|ETYP_COMMS, sz );
                  nLastError = 1;
                  }
               goto TK1;
               }
            else if( r == 6 )
               nLastError = 0;
            free( pszFilename );
            break;
            }

         case TK_CLEAR:
            /* BUGBUG: Clear terminal buffer.
             */
            break;

         case TK_RESUMELOG:
            Amcomm_ResumeLogFile( lpcdev );
            break;

         case TK_HOLDLOG:
            Amcomm_PauseLogFile( lpcdev );
            break;

         case TK_CLOSELOG:
            Amcomm_CloseLogFile( lpcdev );
            break;

         case TK_OPENLOG:
            if( GetToken() != TK_STR )
               ScriptError( GS(IDS_STR458) );
            else
               Amcomm_OpenLogFile( lpcdev, sBuf );
            break;

         case TK_RENAME:
            if( GetToken() != TK_STR )
               ScriptError( GS(IDS_STR458) );
            else
               {
               char szOldName[ 144 ];

               strcpy( szOldName, sBuf );
               if( GetToken() != TK_STR )
                  ScriptError( GS(IDS_STR458) );
               else
                  rename( szOldName, sBuf );
               }
            break;

         case TK_DELETE:
            if( GetToken() != TK_STR )
               ScriptError( GS(IDS_STR458) );
            else
               _unlink( sBuf );
            break;

         case TK_COPY:
            if( GetToken() != TK_STR )
               ScriptError( GS(IDS_STR458) );
            else {
               HNDFILE fhOut;
               BOOL fAppend = FALSE;

               if( GetToken() != TK_LSHIFT )
                  ScriptError( GS(IDS_STR473) );
               if( GetToken() == TK_ADD )
                  fAppend = TRUE;
               if( fAppend )
                  {
                  if( ( fhOut = Amfile_Open( sBuf, AOF_WRITE ) ) == HNDFILE_ERROR )
                     fhOut = Amfile_Create( sBuf, 0 );
                  else
                     Amfile_SetPosition( fhOut, 0L, ASEEK_END );
                  }
               else
                  fhOut = Amfile_Create( sBuf, 0 );
               if( fhOut == HNDFILE_ERROR )
                  ScriptError( GS(IDS_STR691), sBuf );
               else {
                  while( Amlbf_Read( hBuffer, sz, sizeof( sz ) - 1, NULL, NULL, &fIsAmeol2Scratchpad ) )
                     {
                     lpLex = sz;
                     if( strlen( sz ) == 2 && sz[ 0 ] == '<' && sz[ 1 ] == '<' )
                        break;
                     if( strlen( sz ) > 1 )
                        Amfile_Write( fhOut, sz + 1, strlen( sz ) - 1 );
                     Amfile_Write( fhOut, "\r\n", 2 );
                     ++cLine;
                     }
                  Amfile_Close( fhOut );
                  }
               }
            break;

         case TK_STATUS:
            if( GetToken() != TK_STR )
               ScriptError( GS(IDS_STR458) );
            else
               OnlineStatusText( sBuf );
            break;

         case TK_EOF:
            break;

         case TK_PAUSE: {
            WORD wPause;

            wPause = GetInteger();
            if( wCommErr == WCE_NOERROR )
               Amcomm_Delay( lpcdev, wPause );
            break;
            }

         case TK_BREAK:
            if( !ExitLoop( hBuffer, TK_WHILE, TK_ENDWHILE ) )
               ScriptError( GS(IDS_STR475) );
            break;

         case TK_CONTINUE:
            if( nWhileStackPtr == 0 )
               ScriptError( GS(IDS_STR476) );
            Amlbf_SetPos( hBuffer, aWhileStack[ nWhileStackPtr ].pos );
            cLine = aWhileStack[ nWhileStackPtr ].cLine;
            Amlbf_Read( hBuffer, sz, sizeof( sz ) - 1, &nLineLength, NULL, &fIsAmeol2Scratchpad );

         case TK_WHILE:
            if( !GetExpression( lpcdev ) )
               {
               if( !ExitLoop( hBuffer, TK_WHILE, TK_ENDWHILE ) )
                  ScriptError( GS(IDS_STR477) );
               break;
               }
            if( nToken == TK_CONTINUE )
               break;
            if( nWhileStackPtr == MAX_WHILE )
               ScriptError( GS(IDS_STR478) );
            aWhileStack[ nWhileStackPtr ].pos = Amlbf_GetPos( hBuffer ) - nLineLength;
            aWhileStack[ nWhileStackPtr ].cLine = cLine;
            ++nWhileStackPtr;
            break;

         case TK_ENDWHILE:
            if( nWhileStackPtr == 0 )
               ScriptError( GS(IDS_STR479) );
            --nWhileStackPtr;
            Amlbf_SetPos( hBuffer, aWhileStack[ nWhileStackPtr ].pos );
            cLine = aWhileStack[ nWhileStackPtr ].cLine;
            break;
         case TK_AMEOLUPDATE: {
            register int c;
            char ch;
            char chWait;
            char szTkn[20];

            /* Check for a new version of Ameol
             */

            chWait = 10;
            Amcomm_WriteString( lpcdev, szGoAmeolupdate, strlen(szGoAmeolupdate) );

            Amcomm_WaitFor( lpcdev, chWait );
            Amcomm_Get( lpcdev, &ch, lpcdev->lpcd->nTimeout );
            c = 0;
            if( ch == 10 )
               Amcomm_Get( lpcdev, &ch, lpcdev->lpcd->nTimeout );
            while( ( ch != 13 && ch != 10 ) && c < 30 )
               {
               sBuf[ c++ ] = ch;
               Amcomm_Get( lpcdev, &ch, lpcdev->lpcd->nTimeout );
               }
            sBuf[ c ] = '\0';

            ExtractToken((char *)&sBuf, (char *)&szTkn, 0, '.');
            amvLatest.nMaxima = atoi(szTkn);
      
            ExtractToken((char *)&sBuf, (char *)&szTkn, 1, '.');
            amvLatest.nMinima = atoi(szTkn);

            ExtractToken((char *)&sBuf, (char *)&szTkn, 2, '.');
            amvLatest.nBuild  = atoi(szTkn);

/*
            MakePath( sz, pszAmeolDir, "version.txt" );

            if( HNDFILE_ERROR != ( fh = Amfile_Create( sz, AOF_READWRITE ) ) )
               {
               Amfile_Write( fh, sBuf, strlen(sBuf) );
               Amfile_Close( fh );
               }
*/
            break;

                         }


/*       case TK_VERSION: {
            char * wstr3[ 2 ];
*/
            /* Send the secret code to CIX to tell it that this
             * is Ameol.
             */
/*          Amcomm_WriteString( lpcdev, szGoAmeolversion, strlen(szGoAmeolversion) );
            wstr3[ 0 ] = "Succeeded";
            wstr3[ 1 ] = "M:";
            if( 0 == Amcomm_WaitForString( lpcdev, 2, wstr3, lpcdev->lpcd->nTimeout ) )
               {
               char sz[ 40 ];

               Amcomm_Delay( lpcdev, 500 );
            #ifdef WIN32
               wsprintf( sz, "%%^#%%**#%u.%2.2u.%u (32-bit)\n", amv.nMaxima, amv.nMinima, amv.nBuild );
            #else
               wsprintf( sz, "%%^#%%**#%u.%2.2u.%u (16-bit)\n", amv.nMaxima, amv.nMinima, amv.nBuild );
            #endif
               Amcomm_WriteString( lpcdev, sz, strlen(sz) );
               Amcomm_WaitForString( lpcdev, 2, wstr3, lpcdev->lpcd->nTimeout );
               }
            break;
            }
*/
         case TK_IF:
            if( !GetExpression( lpcdev ) )
               {
               int nNesting;
               BOOL fInCopy;

               nNesting = 0;
               fInCopy = FALSE;
               while( nNesting >= 0 && Amlbf_Read( hBuffer, sz, sizeof( sz ) - 1, NULL, NULL, &fIsAmeol2Scratchpad ) )
                  {
                  lpLex = sz;
                  if( fInCopy && strlen( sz ) == 2 && sz[ 0 ] == '<' && sz[ 1 ] == '<' )
                     fInCopy = FALSE;
                  else if( !fInCopy )
                     {
                     nToken = GetToken();
                     if( nToken == TK_IF )
                        ++nNesting;
                     if( nToken == TK_ENDIF )
                        --nNesting;
                     else if( nToken == TK_COPY )
                        fInCopy = TRUE;
                     }
                  }
               }
            break;

         case TK_ENDIF:
            break;

         case TK_WAITFOR: {
            char * wstr3[ 1 ];

            if( GetToken() != TK_STR )
               ScriptError( GS(IDS_STR458) );
            Amcomm_TranslateString( sBuf, FALSE );
            wstr3[ 0 ] = sBuf;
            Amcomm_WaitForString( lpcdev, 1, wstr3, lpcdev->lpcd->nTimeout );
            break;
            }

         case TK_PUTNOCR:
         case TK_PUT: {
            int length;
            char ch;

            switch( GetToken() )
               {
               case TK_STR:
                  break;

               case TK_USERNAME:
                  strcpy( sBuf, szCIXNickname );
                  break;

               case TK_PASSWORD: {
                  char szPassword[ 40 ];

                  memcpy( szPassword, szCIXPassword, sizeof(szCIXPassword) );
                  Amuser_Decrypt( szPassword, rgEncodeKey );
                  strcpy( sBuf, szPassword );
                  break;
                  }

               default:
                  ScriptError( GS(IDS_STR458) );
                  break;
               }
            while( wCommErr == WCE_NOERROR && Amcomm_QueryCharReady( lpcdev ) )
               Amcomm_Get( lpcdev, &ch, lpcdev->lpcd->nTimeout );
            if( wCommErr == WCE_NOERROR )
               {
               Amcomm_TranslateString( sBuf, FALSE );
               length = strlen( sBuf );
               if( nToken == TK_PUT )
                  {
                  sBuf[ length++ ] = 10;
                  sBuf[ length ] = '\0';
                  }
               Amcomm_WriteString( lpcdev, sBuf, length );
               if( nToken == TK_PUT )
                  Amcomm_WaitFor( lpcdev, 10 );
               }
            break;
            }

         default: {
            ScriptError( GS(IDS_STR480) );
            break;
            }
         }
      if( wCommErr != WCE_NOERROR )
         break;
      ++cLine;
      }
   Amlbf_Close( hBuffer );
   fRunningScript = FALSE;
   return( TRUE );
}

/* This function is called by the file transfer functions to report
 * status information.
 */
int EXPORT CALLBACK ScriptFileTransferStatus( LPFILETRANSFERINFO lpftinfo )
{
   static BOOL fReceiving;

   switch( lpftinfo->wStatus )
      {
      case FTST_DOWNLOAD:
         fReceiving = TRUE;
         break;

      case FTST_UPLOAD:
         fReceiving = FALSE;
         break;

      case FTST_YIELD:
         TaskYield();
         break;

      case FTST_BEGINTRANSMIT:
         wsprintf( lpTmpBuf, GS(IDS_STR1119), GetFileBasename(lpftinfo->szFilename) );
         OnlineStatusText( lpTmpBuf );
         ResetBlinkGauge();
         fReceiving = FALSE;
         break;

      case FTST_BEGINRECEIVE:
         wsprintf( lpTmpBuf, GS(IDS_STR1120), GetFileBasename(lpftinfo->szFilename) );
         OnlineStatusText( lpTmpBuf );
         ResetBlinkGauge();
         fReceiving = TRUE;
         break;

      case FTST_UPDATE:
         StepBlinkGauge( lpftinfo->wPercentage );
         ShowDownloadStatistics( lpftinfo );
         break;

      case FTST_COMPLETED:
         if( fReceiving )
            {
            wsprintf( lpTmpBuf, GS(IDS_STR1121), GetFileBasename(lpftinfo->szFilename) );
            OnlineStatusText( lpTmpBuf );
            wsprintf( lpTmpBuf, GS(IDS_STR403),
                                GetFileBasename( lpftinfo->szFilename ),
                                lpftinfo->dwTotalSize,
                                Amdate_FormatLongTime( lpftinfo->dwTimeSoFar ),
                                lpftinfo->cps  );
            WriteToBlinkLog( lpTmpBuf );
            }
         else
            {
            wsprintf( lpTmpBuf, GS(IDS_STR1122), GetFileBasename(lpftinfo->szFilename) );
            OnlineStatusText( lpTmpBuf );
            wsprintf( lpTmpBuf, GS(IDS_STR405),
                                GetFileBasename( lpftinfo->szFilename ),
                                lpftinfo->dwTotalSize,
                                Amdate_FormatLongTime( lpftinfo->dwTotalSize / lpftinfo->cps ),
                                lpftinfo->cps );
            WriteToBlinkLog( lpTmpBuf );
            }
         HideBlinkGauge();
         break;

      case FTST_FAILED:
         if( fReceiving )
            {
            wsprintf( lpTmpBuf, GS(IDS_STR991), GetFileBasename( lpftinfo->szFilename ) );
            WriteToBlinkLog( lpTmpBuf );
            }
         else
            wsprintf( lpTmpBuf, GS(IDS_STR990), GetFileBasename( lpftinfo->szFilename ) );
         WriteToBlinkLog( lpTmpBuf );
         HideBlinkGauge();
         break;
      }
   return( 0 );
}

static int FASTCALL GetExpression( LPCOMMDEVICE lpcdev )
{
   BOOL fDoneAtLevel;
   register int r;
   register int s;
   int nToken;
   BOOL fCond;

   r = fCond = GetOperand( lpcdev );
   fDoneAtLevel = FALSE;
   while( !fDoneAtLevel )
      if( ( nToken = GetToken() ) == TK_LT )
         {
         s = GetOperand( lpcdev );
         fCond = r < s;
         }
      else if( nToken == TK_GT )
         {
         s = GetOperand( lpcdev );
         fCond = r > s;
         }
      else if( nToken == TK_LE )
         {
         s = GetOperand( lpcdev );
         fCond = r <= s;
         }
      else if( nToken == TK_GE )
         {
         s = GetOperand( lpcdev );
         fCond = r >= s;
         }
      else if( nToken == TK_EQ )
         {
         s = GetOperand( lpcdev );
         fCond = r == s;
         }
      else if( nToken == TK_NEQ )
         {
         s = GetOperand( lpcdev );
         fCond = r != s;
         }
      else
         {
         UngetToken( nToken );
         fDoneAtLevel = TRUE;
         }
   return( fCond );
}

static int FASTCALL GetOperand( LPCOMMDEVICE lpcdev )
{
   register int r;

   r = 0;
   switch( GetToken() )
      {
      case TK_LPAREN:
         r = GetExpression( lpcdev );
         if( GetToken() != TK_RPAREN )
            ScriptError( GS(IDS_STR481) );
         break;

      case TK_NOT:
         return( !GetExpression( lpcdev ) );

      case TK_TIMEOUT:
         return( lpcdev->lpcd->nTimeout );

      case TK_RECOVER:
         return( Amuser_GetPPInt( szSettings, "RecoverScratchpad", FALSE ) );

      case TK_NUMBER:
         return( iValue );

      case TK_LASTERROR:
         return( nLastError );

      case TK_WAITFOR: {
         char * wStr3[ 10 ];
         register int c;
         int nToken;

         if( GetToken() != TK_LPAREN )
            ScriptError( GS(IDS_STR482) );
         c = 0;
         nToken = GetToken();
         while( nToken != TK_RPAREN )
            {
            if( nToken != TK_STR )
               ScriptError( GS(IDS_STR458) );
            if( c == 10 )
               ScriptError( GS(IDS_STR460) );
            Amcomm_TranslateString( sBuf, FALSE );
            wStr3[ c++ ] = _strdup( sBuf );
            if( ( nToken = GetToken() ) == TK_COMMA )
               nToken = GetToken();
            }
         r = Amcomm_WaitForString( lpcdevCur, c, wStr3, lpcdev->lpcd->nTimeout );
         while( --c >= 0 )
            free( wStr3[ c ] );
         break;
         }

      default:
         ScriptError( GS(IDS_STR461) );
         break;
      }
   return( r );
}

static int FASTCALL GetInteger( void )
{
   if( GetToken() != TK_NUMBER )
      ScriptError( GS(IDS_STR462) );
   return( iValue );
}

static int FASTCALL GetToken( void )
{
   char ch;

   if( fHasToken )
      {
      fHasToken = FALSE;
      return( nPushedBackToken );
      }
   while( TRUE )
      if( ( ch = *lpLex++ ) == 0 )
         {
         --lpLex;
         break;
         }
      else if( isspace( ch ) )
         continue;
      else if( ch == '(' )
         return( TK_LPAREN );
      else if( ch == ')' )
         return( TK_RPAREN );
      else if( ch == ',' )
         return( TK_COMMA );
      else if( ch == '<' && *lpLex == '<' )
         {
         ++lpLex;
         return( TK_LSHIFT );
         }
      else if( ch == '=' && *lpLex == '=' )
         {
         ++lpLex;
         return( TK_EQ );
         }
      else if( ch == '+' )
         return( TK_ADD );
      else if( ch == '<' )
         {
         if( *lpLex == '=' )
            {
            ++lpLex;
            return( TK_LE );
            }
         return( TK_LT );
         }
      else if( ch == '>' )
         {
         if( *lpLex == '=' )
            {
            ++lpLex;
            return( TK_GE );
            }
         return( TK_GT );
         }
      else if( ch == '!' )
         {
         if( *lpLex == '=' )
            {
            ++lpLex;
            return( TK_NEQ );
            }
         return( TK_NOT );
         }
      else if( isdigit( ch ) )
         {
         int iBase;

         iValue = 0;
         iBase = 10;
         if( ch == '0' )
            {
            ch = *lpLex++;
            iBase = 8;
            if( ch == 'x' || ch == 'X' )
               {
               iBase = 16;
               ch = *lpLex++;
               }
            }
         while( isxdigit( ch ) )
            {
            if( iBase == 16 && isalpha( ch ) )
               ch = (char)( ( ch & 15 ) + 10 );
            else
               ch &= 15;
            iValue = iValue * iBase + ch;
            ch = *lpLex++;
            }  /* End of file */
         --lpLex;
         return( TK_NUMBER );
         }
      else if( ch == '"' )
         {
         int sLen;

         sLen = 0;
         ch = *lpLex++;
         while( TRUE )
            {
            if( ch == '"' )
               {
               if( *lpLex != '"' )
                  break;
               ++lpLex;
               }
            if( ch == 0 ) {
               ScriptError( GS(IDS_STR463) );
               break;
               }
            if( sLen == 512 )
               {
               ScriptError( GS(IDS_STR464) );
               sLen = 513;
               }
            else if( sLen < 512 )
               sBuf[ sLen++ ] = ch;
            ch = *lpLex++;
            }  /* End of while */
         sBuf[ sLen ] = '\0';
         return( TK_STR );
         }
      else if( ch == '\'' )
         {
         char ch2;

         if( ( ch2 = *lpLex++ ) == '\'' )
            ScriptError( GS(IDS_STR465) );
         if( *lpLex++ != '\'' )
            ScriptError( GS(IDS_STR466) );
         iValue = (int)ch2;
         return( TK_NUMBER );
         }
      else if( isalpha( ch ) || ch == '_' )
         {
         register int c;

         ReadName( ch, sBuf, sizeof(sBuf) );
         for( c = 0; KWORDS[ c ].psKeyWord; ++c )
            if( _strcmpi( KWORDS[ c ].psKeyWord, sBuf ) == 0 )
               return( KWORDS[ c ].nID );
         return( TK_STR );
         }
      else
         ScriptError( GS(IDS_STR467), ch );
   return( TK_EOF );
}

static int FASTCALL ReadName( char chFirst, char * psBuf, int iMaxToRead )
{
   register char ch;
   register int c;

   c = 0;
   psBuf[ c++ ] = chFirst;
   while( ch = *lpLex++, isalnum( ch ) || ch == '_' )
      if( c < iMaxToRead )
         psBuf[ c++ ] = ch;
   --lpLex;
   psBuf[ c ] = '\0';
   return( c );
}

static void NEAR CDECL ScriptError( LPSTR lpszErrStg, ... )
{
   va_list varArg;
   char sz[ 100 ];
   char sz2[ 100 ];

   va_start( varArg, lpszErrStg );
   wvsprintf( sz, lpszErrStg, varArg );
   wsprintf( sz2, GS(IDS_STR468), (LPSTR)GetFileBasename( szFileName ), cLine, (LPSTR)sz );
   ReportOnline( sz2 );
   va_end( varArg );
   wCommErr = WCE_SCRIPTERR;
}

BOOL FASTCALL CompileInline( HNDFILE fh, char * np )
{
   BOOL fDownload = FALSE;

   lpLex = np;
   if( GetToken() == TK_PUT )
      if( GetToken() == TK_STR )
         {
         if( _strcmpi( "download", sBuf ) == 0 )
            fDownload = TRUE;
         WriteToScriptFile( fh, FALSE, sBuf );
         }
   return( fDownload );
}

BOOL FASTCALL IsInlineToken( char * psz )
{
   int nToken;

   lpLex = psz;
   if( *lpLex != ' ' )
      if( ( nToken = GetToken() ) == TK_PUT || nToken == TK_EOF || nToken == TK_REM )
         return( TRUE );
   return( FALSE );
}

static BOOL FASTCALL ExitLoop( LPVOID hBuffer, int nLoopStart, int nLoopEnd )
{
   int nNesting;
   char sz[ 80 ];

   nNesting = 0;
   while( nNesting >= 0 && Amlbf_Read( hBuffer, sz, sizeof( sz ) - 1, NULL, NULL, &fIsAmeol2Scratchpad ) )
      {
      int nToken;

      lpLex = sz;
      if( ( nToken = GetToken() ) == nLoopStart )
         ++nNesting;
      if( nToken == nLoopEnd )
         --nNesting;
      }
   return( nNesting == -1 );
}

/* This function attempts to get sufficient privileges to allow the user
 * to change the system time. This code is courtesy of mthorn@cix.
 */
#ifdef WIN32
static BOOL FASTCALL GetPrivilege( void )
{
   HANDLE hToken;
   LUID ChangeSystemTime;
   TOKEN_PRIVILEGES tkp;
   WORD wWinVer;

   wWinVer = LOWORD( GetVersion() );
   wWinVer = (( (WORD)(LOBYTE( wWinVer ) ) ) << 8 ) | ( (WORD)HIBYTE( wWinVer ) );
   if( wWinVer >= 0x035F )
      return( TRUE );

   if( !OpenProcessToken( GetCurrentProcess(),  TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken ) )
      return( FALSE );

   if( !LookupPrivilegeValue( NULL, SE_SYSTEMTIME_NAME, &ChangeSystemTime ) )
      return( FALSE );

   tkp.PrivilegeCount = 1;
   tkp.Privileges[0].Luid = ChangeSystemTime;
   tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
   AdjustTokenPrivileges( hToken, FALSE, &tkp, sizeof(TOKEN_PRIVILEGES), NULL, NULL );

   /* apparently the return value from 'AdjustTokenPrivileges' is not valid
    */
   if( GetLastError() != ERROR_SUCCESS )
      return( FALSE );
   return( TRUE );
}
#endif
