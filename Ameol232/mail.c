/* MAIL.C - Handles e-mailing
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
#include <memory.h>
#include <string.h>
#include "print.h"
#include <dos.h>
#include "amaddr.h"
#include <ctype.h>
#include "amlib.h"
#include <io.h>
#include "ameol2.h"
#include "command.h"
#include "lbf.h"
#include "rules.h"
#include "shared.h"
#include "cixip.h"
#include "cix.h"
#include "tti.h"
#include "mail.h"
#include "cookie.h"
#include "editmap.h"

#ifndef USEBIGEDIT
#include "scintilla.h"
#include "scilexer.h"
#endif USEBIGEDIT

#define  THIS_FILE   __FILE__

#ifdef WIN32
#define  TASKHANDLE  DWORD
#else
#define  TASKHANDLE  HTASK
#endif

extern BOOL fIsAmeol2Scratchpad;

extern BOOL fWordWrap;                 /* TRUE if we word wrap messages */

char NEAR szMailWndClass[] = "amctl_mailwndcls";
char NEAR szMailWndName[] = "Mail";

static BOOL fDefDlgEx = FALSE;

static BOOL fRegistered = FALSE;             /* TRUE if we've registered the mail window */
static WNDPROC lpEditCtrlProc = NULL;        /* Edit control window procedure address */
static BOOL fNewMsg;                         /* TRUE if we're editing a new message */
extern char * szDayOfWeek[ 7 ];
extern char * szMonth[ 12 ];

LPSTR GetReadableText(PTL ptl, LPSTR pSource);

void FASTCALL CreateMessageId( char * );
BOOL FASTCALL CheckForAttachment(HWND pEdit, HWND pAttach);
BOOL FASTCALL CheckValidMailAddress(HWND pEdit);
int FASTCALL b64decodePartial(char *d);

typedef struct tagMAILWNDINFO {
   LPOB lpob;
   DWORD dwReply;
   HWND hwndFocus;
   RECPTR recMailbox;
   char szSigFile[ 16 ];
   PTL ptl;
} MAILWNDINFO, FAR * LPMAILWNDINFO;

#define  GetMailWndInfo(h)    (LPMAILWNDINFO)GetWindowLong((h),DWL_USER)
#define  SetMailWndInfo(h,p)     SetWindowLong((h),DWL_USER,(long)(p))

/* Array of controls that have tooltips and their string
 * resource IDs.
 */
static TOOLTIPITEMS tti[] = {
   IDD_LOWPRIORITY,     IDS_STR997,
   IDD_HIGHPRIORITY,    IDS_STR996,
   IDD_TAGREPLY,        IDS_STR998,
   IDD_CCTOSENDER,         IDS_STR1072,
   IDD_RETURNRECEIPT,      IDS_STR1203
   };

#define  ctti     (sizeof(tti)/sizeof(TOOLTIPITEMS))

ENCODINGSCHEMES NEAR EncodingScheme[] =
   {
   "MIME/Base64", ENCSCH_MIME,
   "Uuencode",    ENCSCH_UUENCODE,
   "Binary",      ENCSCH_BINARY,
   NULL,       0
   };

#define  MAX_FORMNAME         20
#define  MAX_HDRITEMFIELD     10
#define NO_COMMENT            "NoComment"

WORD wDefEncodingScheme = ENCSCH_UUENCODE;      /* Default encoding scheme */
int iNextFormID = IDM_FORMFIRST;                /* Form command ID */
HMENU hFormsMenu = NULL;                        /* Forms menu handle */
int cMailFormsInstalled;                        /* Number of mail forms installed */
BOOL fAddFormsToMenu = FALSE;                   /* TRUE if we add forms to the Mail menu */

BOOL fGetMailDetails;

#define  cClrList          9
char * pClrList[ cClrList ] = { "Never", "Day", "2 Days", "3 Days", "4 Days", "5 Days", "6 Days", "Week", "Scheduled" };
int nClrRate[ cClrList ] = { 0, 1, 2, 3, 4, 5, 6, 7, 255 };

#define  cCountList           8
char * pCountList[ cCountList ] = { "Now", "A Day", "2 Days", "3 Days", "4 Days", "5 Days", "6 Days", "A Week" };
int nClearCount[ cCountList ] = { 0, 1, 2, 3, 4, 5, 6, 7 };

void FASTCALL AppendToTextField( HPSTR *, char * );
void FASTCALL ExpandMailFormParameters( HPSTR, char *, char *, int );
void FASTCALL CreateMailSubject( RECPTR *, char *, PTH );
BOOL FASTCALL IsNoCommentFolder( PTL, char *, LPSTR, int );
BOOL FASTCALL StrBuf_AppendParagraph( STRBUF FAR *, HPSTR );

BOOL FASTCALL MailWnd_OnCreate( HWND, LPCREATESTRUCT );
void FASTCALL MailWnd_OnClose( HWND );
void FASTCALL MailWnd_OnSize( HWND, UINT, int, int );
void FASTCALL MailWnd_OnMove( HWND, int, int );
void FASTCALL MailWnd_MDIActivate( HWND, BOOL, HWND, HWND );
void FASTCALL MailWnd_OnCommand( HWND, int, HWND, UINT );
void FASTCALL MailWnd_OnSetFocus( HWND, HWND );
void FASTCALL MailWnd_OnAdjustWindows( HWND, int, int );
LRESULT FASTCALL MailWnd_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr );

LRESULT EXPORT CALLBACK MailWndProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK MailEditProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK MailWnd_GetMsgProc( int, WPARAM, LPARAM ); 

void FASTCALL FormatTextIP( HPSTR lprpSrc, HPSTR * lprpDest, DWORD pMax, DWORD iWordWrapColumn ); // FS#76  

void FASTCALL GetURLPathName(LPCSTR lpszLongPath, LPSTR  lpszShortPath, DWORD cchBuffer)
{
   DWORD i,j;
   LPSTR lTemp;

   INITIALISE_PTR(lTemp);
   if( !fNewMemory( &lTemp, cchBuffer+1 ) )
      return;

   i = 0;
   j = 0;
   while((lpszLongPath[i]) && (i < cchBuffer))
   {     
      if (lpszLongPath[i] == ' ')
      {
         strcat(lTemp, "%20");
         j += 3;
      }
      else
      {
         lTemp[j] = lpszLongPath[i];
         j++;
      }
      i++;
   }
   if( i >= cchBuffer)
      GetShortPathName( lpszLongPath, lpszShortPath, cchBuffer );
   else
      strcpy(lpszShortPath, lTemp);

   FreeMemory( &lTemp );
}

/* This function fills the specified combo box with a list
 * of mail clear rates.
 */
void FASTCALL Common_FillClearRateList( HWND hwnd, int id, int nDefault )
{
   HWND hwndList;
   int c;

   VERIFY( hwndList = GetDlgItem( hwnd, id ) );
   for( c = 0; c < cClrList; ++c )
      {
      ComboBox_InsertString( hwndList, -1, pClrList[ c ] );
      if( nDefault == nClrRate[ c ] )
         ComboBox_SetCurSel( hwndList, c );
      }
}

/* This function fills the specified combo box with a list
 * of mail clear counts.
 */
void FASTCALL Common_FillClearCountList( HWND hwnd, int id, int nDefault )
{
   HWND hwndList;
   int c;

   VERIFY( hwndList = GetDlgItem( hwnd, id ) );
   for( c = 0; c < cCountList; ++c )
      {
      ComboBox_InsertString( hwndList, -1, pCountList[ c ] );
      if( nDefault == nClearCount[ c ] )
         ComboBox_SetCurSel( hwndList, c );
      }
}

/* This function displays the common Clear Mail scheduler
 * dialog.
 */
void FASTCALL CommonScheduleDialog( HWND hwnd, char * pszCommand )
{
   int nMailClearRate;
   register int c;
   HWND hwndList;
   SCHEDULE sch;
   int nRetCode;

   nRetCode = Ameol2_EditSchedulerInfo( hwnd, pszCommand );
   if( SCHDERR_NOSUCHCOMMAND == nRetCode )
      {
      SCHEDULE sch;

      /* If entry not present, then create a default one
       * with 7 days.
       */
      sch.schType = SCHTYPE_DAYPERIOD;
      sch.schDate.iDay = 3;
      Ameol2_SetSchedulerInfo( pszCommand, &sch );
      nRetCode = Ameol2_EditSchedulerInfo( hwnd, pszCommand );
      }
   if( SCHDERR_OK == nRetCode )
      {
      nMailClearRate = 0;
      if( SCHDERR_OK == Ameol2_GetSchedulerInfo( pszCommand, &sch ) )
         {
         nMailClearRate = 3;
         if( sch.schType == SCHTYPE_DAYPERIOD )
            if( sch.schDate.iDay <= 7 )
               nMailClearRate = sch.schDate.iDay;
         ShowEnableControl( hwnd, IDD_SCHEDULER, nMailClearRate == 3 );
         }
      VERIFY( hwndList = GetDlgItem( hwnd, IDD_CLRLIST ) );
      ComboBox_ResetContent( hwndList );
      for( c = 0; c < cClrList; ++c )
         {
         ComboBox_InsertString( hwndList, -1, pClrList[ c ] );
         if( nMailClearRate == nClrRate[ c ] )
            ComboBox_SetCurSel( hwndList, c );
         }
      }
}

/* This function installs all mail forms in the FORMS directory
 * as commands.
 */
void FASTCALL InstallMailForms( void )
{
   char szFormsDir[ _MAX_PATH ];
   BOOL fHasFormsCategory;
   register int n;
   FINDDATA ft;
   HFIND r;

   fHasFormsCategory = FALSE;
   cMailFormsInstalled = 0;
   wsprintf( szFormsDir, "%sFORMS\\*.AFM", pszAmeolDir ); // VistaAlert
   for( n = r = Amuser_FindFirst( szFormsDir, _A_NORMAL, &ft ); n != -1; n = Amuser_FindNext( r, &ft ) )
      {
      char szFilename[ MAX_FORMNAME + 1 ];
      char szCmdName[ MAX_FORMNAME + 20 ];
      register int c;
      CMDREC cmd;

      /* Ensure we have a Forms category.
       */
      if( !fHasFormsCategory )
         if( !CTree_FindCategory( CAT_FORMS ) )
            {
            CTree_AddCategory( CAT_FORMS, (WORD)-1, "Forms" );
            fHasFormsCategory = TRUE;
            }

      /* Prettify the file name.
       */
      strcpy( szFilename, ft.name );
      szFilename[ 0 ] = toupper( ft.name[ 0 ] );
      for( c = 1; c < MAX_FORMNAME && ft.name[ c ] && ft.name[ c ] != '.'; ++c )
         szFilename[ c ] = tolower( ft.name[ c ] );
      szFilename[ c ] = '\0';

      /* Create command and pretty names.
       */
      wsprintf( szCmdName, "Form:%s", szFilename );

      /* Create a command entry for this form.
       */
      if( iNextFormID > IDM_FORMLAST )
         iNextFormID = IDM_FORMFIRST;
      cmd.iCommand = iNextFormID++;
      cmd.lpszString = szFilename;
      cmd.lpszCommand = szCmdName;
      cmd.iDisabledString = 0;
      cmd.lpszTooltipString = szCmdName;
      cmd.iToolButton = 0;
      cmd.wCategory = CAT_FORMS;
      cmd.wDefKey = 0;
      cmd.iActiveMode = WIN_ALL;
      cmd.hLib = NULL;
      cmd.wNewKey = 0;
      cmd.wKey = 0;
      cmd.nScheduleFlags = NSCHF_NONE;
      SetCommandCustomBitmap( &cmd, BTNBMP_GRID, 24 );
      if( CTree_InsertCommand( &cmd ) )
         {
         /* Optionally add forms to the Mail menu.
          */
         if( fAddFormsToMenu )
            {
            char szMenuName[ 60 ];

            /* Create a Forms menu if we don't have one.
             */
            if( NULL == hFormsMenu )
               {
               HMENU hMailMenu;

               hMailMenu = GetSubMenu( hMainMenu, 2 );
               hFormsMenu = CreateMenu();
               InsertMenu( hMailMenu, 5, MF_POPUP|MF_BYPOSITION, (int)hFormsMenu, "&Forms" );
               }

            /* Add to the Forms submenu.
             */
            strcpy( szMenuName, szFilename );
            PutUniqueAccelerator( hFormsMenu, szMenuName );
            AppendMenu( hFormsMenu, MF_BYPOSITION|MF_STRING, cmd.iCommand, szMenuName );
            }

         /* Increment count of forms installed.
          */
         ++cMailFormsInstalled;
         }
      }
   Amuser_FindClose( r );
}

/* This function composes a mail form.
 */
void FASTCALL ComposeMailForm( char * pszFormName, BOOL fEdit, HPSTR hpszFormMsg, HEADER FAR * lpHdr )
{
   char szFormsDir[ _MAX_PATH ];
   HNDFILE fh;

   /* Locate the form in the FORMS directory
    */
   wsprintf( szFormsDir, "%sFORMS\\%s.AFM", pszAmeolDir, pszFormName ); // VistaAlert
   if( HNDFILE_ERROR != ( fh = Amfile_Open( szFormsDir, AOF_READ ) ) )
      {
      BOOL fInBody;
      MAILOBJECT mo;
      char sz[ 128 ];
      LPLBF hBuffer;
      HPSTR hpBuf;

      /* Create an empty mail object.
       */
      Amob_New( OBTYPE_MAILMESSAGE, &mo );

      /* Set the From address.
       */
      wsprintf( lpTmpBuf, "%s (%s)", (LPSTR)szMailAddress, (LPSTR)szFullName );
      Amob_CreateRefString( &mo.recFrom, lpTmpBuf );

      /* Set the defaults from the header
       */
      if( fUseInternet )
         if( lpHdr && NULL != lpHdr->ptl )
         {
            CreateReplyAddress( &mo.recReplyTo, lpHdr->ptl );
            CreateMailMailAddress( &mo.recFrom, lpHdr->ptl );
         }

      /* Make it a reply to the sender.
       */
      if( NULL != lpHdr )
         Amob_CreateRefString( &mo.recReply, lpHdr->szMessageId );


      /* Default subject
       */
      Amob_CreateRefString( &mo.recSubject, "" );

      /* Don't CC back to sender by default.
       */
      mo.wFlags = MOF_NOECHO;

      /* Default to no encoding format. This fixes a bug where if the delivery method is
       * set to AI and you do a conf only blink the mail is sent but not deleted from the
       * outbasket. The encoding format can be changed later by an Attachment: line.
       */
      mo.wEncodingScheme = ENCSCH_NONE;

      /* Read lines from the file.
       */
      fInBody = FALSE;
      hpBuf = NULL;
      hBuffer = Amlbf_Open( fh, AOF_READ );
      while( Amlbf_Read( hBuffer, sz, sizeof( sz ) - 1, NULL, NULL, &fIsAmeol2Scratchpad ) )
         {
         char * psz;

         /* Skip to first non-blank chr.
          */
         psz = sz;
         while( *psz == ' ' || *psz == '\t' )
            ++psz;
         if( fInBody )
            {
            /* We're parsing a body, so add this line to the
             * end of what we have so far.
             */
            if( _strnicmp( psz, "$TEXT$", 6 ) == 0 && NULL != hpszFormMsg )
               AppendToTextField( &hpBuf, hpszFormMsg );
            else if( _strnicmp( psz, "$BODY$", 6 ) == 0 && NULL != hpszFormMsg )
               {
               HPSTR hpszBody;

               /* Include the body of the message in the text
                */
               if( NULL == ( hpszBody = strstr( hpszFormMsg, "\r\n\r\n" ) ) )
                  hpszBody = hpszFormMsg;
               AppendToTextField( &hpBuf, hpszBody );
               }
            else if( _strnicmp( psz, "$INCLUDE$", 9 ) == 0 )
               {
               HNDFILE fhInc;

               /* Include the contents of another file at this
                * position. The filename can contain macros too.
                */
               psz += 9;
               while( *psz == ' ' || *psz == '\t' )
                  ++psz;
               BEGIN_PATH_BUF;
               ExpandMailFormParameters( hpszFormMsg, psz, lpPathBuf, _MAX_PATH - 1 );

               /* Open the file and expand in-place. Don't do any
                * macro expansion here.
                */
               if( HNDFILE_ERROR == ( fhInc = Amfile_Open( lpPathBuf, AOF_READ ) ) )
                  {
                  char * lpBasename;

                  /* File not found, so try !ERROR!.TXT in the same
                   * directory.
                   */
                  lpBasename = GetFileBasename( lpPathBuf );
                  if( NULL != lpBasename )
                     {
                     strcpy( lpBasename, "!ERROR!.TXT" );
                     fhInc = Amfile_Open( lpPathBuf, AOF_READ );
                     }
                  }
               if( HNDFILE_ERROR == fhInc )
                  {
                  wsprintf( lpTmpBuf, GS(IDS_STR1059), lpPathBuf );
                  AppendToTextField( &hpBuf, lpTmpBuf );
                  }
               else
                  {
                  LPLBF hBuffer2;

                  hBuffer2 = Amlbf_Open( fhInc, AOF_READ );
                  while( Amlbf_Read( hBuffer2, sz, sizeof( sz ) - 1, NULL, NULL, &fIsAmeol2Scratchpad ) )
                     AppendToTextField( &hpBuf, sz );
                  Amlbf_Close( hBuffer2 );
                  }
               END_PATH_BUF;
               }
            else
               AppendToTextField( &hpBuf, sz );
            }
         else {

            /* We're parsing a header. First check for the end of
             * the header, as marked by a blank line.
             */
            if( *psz == '\0' )
               fInBody = TRUE;
            else
               {
               char szHdrItemField[ MAX_HDRITEMFIELD + 1 ];
               register int c;

               /* Extract the header item name.
                */
               for( c = 0; *psz && *psz != ':' && c < MAX_HDRITEMFIELD; ++c )
                  szHdrItemField[ c ] = *psz++;
               szHdrItemField[ c ] = '\0';

               /* Skip the ':' and succeeding whitespace.
                */
               if( *psz == ':' )
                  ++psz;
               while( *psz == ' ' || *psz == '\t' )
                  ++psz;

               /* Expand the parameter fields.
                */
               ExpandMailFormParameters( hpszFormMsg, psz, lpTmpBuf, LEN_TEMPBUF - 1 );

               /* Take action depending on the header.
                * Any illegal field is ignored for now.
                */
               if( _strcmpi( szHdrItemField, "To" ) == 0 )
                  Amob_CreateRefString( &mo.recTo, lpTmpBuf );
               if( _strcmpi( szHdrItemField, "Reply-To" ) == 0 )
                  Amob_CreateRefString( &mo.recReplyTo, lpTmpBuf );
               if( _strcmpi( szHdrItemField, "From" ) == 0 )
                  Amob_CreateRefString( &mo.recFrom, lpTmpBuf );
               if( _strcmpi( szHdrItemField, "CC" ) == 0 )
                  Amob_CreateRefString( &mo.recCC, lpTmpBuf );
               if( _strcmpi( szHdrItemField, "Subject" ) == 0 )
               {
                  lpTmpBuf[LEN_TITLE] = '\x0';
                  Amob_CreateRefString( &mo.recSubject, lpTmpBuf );
               }
               if( _strcmpi( szHdrItemField, "BCC" ) == 0 )
                  Amob_CreateRefString( &mo.recBCC, lpTmpBuf );
               if( _strcmpi( szHdrItemField, "Attachment" ) == 0 )
                  {
                  /* For attachments, determine the mail format
                   * depending on the originating address.
                   */
                  Amob_CreateRefString( &mo.recAttachments, lpTmpBuf );
                  if( fUseCIX && IsCixAddress( lpHdr->szAuthor ) )
                     mo.wEncodingScheme = ENCSCH_BINARY;
                  else if( !fUseInternet || nMailDelivery == MAIL_DELIVER_CIX )
                     mo.wEncodingScheme = ENCSCH_UUENCODE;
                  else
                     mo.wEncodingScheme = wDefEncodingScheme;
                  }
               }
            }
         }
      Amlbf_Close( hBuffer );

      /* Add the body, if we have one.
       */
      if( NULL != hpBuf )
         Amob_CreateRefString( &mo.recText, hpBuf );

      /* All done. If we edit it, call the edit method otherwise
       * commit it to the out-basket.
       */
      if( fEdit )
         CreateMailWindow( hwndFrame, &mo, NULL );
      else
         Amob_Commit( NULL, OBTYPE_MAILMESSAGE, &mo );
      Amob_Delete( OBTYPE_MAILMESSAGE, &mo );
      }
}

/* This function expands the parameter field for a mail form header field
 * using information supplied by the rules manager.
 */
void FASTCALL ExpandMailFormParameters( HPSTR hpszMsg, char * pszSrc, char * pszDest, int cbDest )
{
   while( *pszSrc )
      if( *pszSrc != '$' )
         *pszDest++ = *pszSrc++;
      else
         {
         char szTokenName[ 64 ];
         int x;

         ++pszSrc;
         for( x = 0; *pszSrc && *pszSrc != '$'; ++pszSrc )
            if( x < 63 )
               szTokenName[ x++ ] = *pszSrc;
         szTokenName[ x ] = '\0';
         if( *pszSrc == '$' )
            ++pszSrc;
         if( cbDest > 0 )
            {
            if( _strcmpi( szTokenName, "from" ) == 0 )
            {
               Amdb_GetMailHeaderFieldInText( hpszMsg, "Reply-To", pszDest, cbDest, TRUE );
               if( !*pszDest )
                  Amdb_GetMailHeaderFieldInText( hpszMsg, szTokenName, pszDest, cbDest, TRUE );
            }
            else
               Amdb_GetMailHeaderFieldInText( hpszMsg, szTokenName, pszDest, cbDest, TRUE );
            if( _strcmpi( szTokenName, "from" ) == 0 )
               ParseToField( pszDest, pszDest, cbDest );
            pszDest += strlen( pszDest );
            cbDest -= strlen( pszDest );
            }
         }
   *pszDest = '\0';
}

/* This function composes a new mail message.
 */
void FASTCALL CmdMailMessage( HWND hwnd, LPSTR lpszText, LPSTR lpszMailTo )
{
   LPSTR lpszTo;
   MAILOBJECT mo;
   CURMSG curmsg;
   
   /* Need space for the reply address.
   */
   INITIALISE_PTR(lpszTo);
   if( !fNewMemory( &lpszTo, LEN_MAILADDR+1 ) )
      return;
   
   /* Create a new mail message.
   */ 
   Amob_New( OBTYPE_MAILMESSAGE, &mo );
   
   /* Set the mailbox.
   */
   Ameol2_GetCurrentTopic( &curmsg );
   if( NULL != curmsg.ptl )
   {
      char sz[ 128 ];
      
      WriteFolderPathname( sz, curmsg.ptl );
      Amob_CreateRefString( &mo.recMailbox, sz );
   }
   
   /* Set the From address.
   */
   wsprintf( lpTmpBuf, "%s (%s)", (LPSTR)szMailAddress, (LPSTR)szFullName );
   Amob_CreateRefString( &mo.recFrom, lpTmpBuf );
   
   /* If no prepared text...
   */
   if( NULL != lpszTo )
   {
      /* Get the recipient name from any of the various
       * open windows.
       */
      if( NULL != lpszMailTo )
         Amob_CreateRefString( &mo.recTo, lpszMailTo );
      else if( NULL != hwndTopic )
      {
         LPWINDINFO lpWindInfo;
         
         lpWindInfo = GetBlock( hwndTopic );
         if( lpWindInfo->lpMessage )
         {
            MSGINFO msginfo;
            
            
            Amdb_GetMsgInfo( lpWindInfo->lpMessage, &msginfo );
            if( fGetMailDetails && !Amdb_GetTopicFlags( lpWindInfo->lpTopic, TF_LISTFOLDER ) )
               GetMarkedName( hwndTopic, lpszTo, LEN_MAILADDR+1, TRUE );
            else
            {
               lpszTo[ 0 ] = '\0';
               Amdb_GetCookie( lpWindInfo->lpTopic, MAIL_LIST_ADDRESS, lpszTo, "", LEN_MAILADDR );
            }
            if( *lpszTo )
               Amob_CreateRefString( &mo.recTo, lpszTo );
            else if( fGetMailDetails && ( Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_MAIL || Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_NEWS ) )
            {
               Amdb_GetMailHeaderField( lpWindInfo->lpMessage, "From", lpszTo, LEN_MAILADDR+1, FALSE );
               if( *lpszTo && !( msginfo.dwFlags & MF_OUTBOXMSG ) )
               {
                  ParseToField( lpszTo, lpszTo, LEN_MAILADDR );
                  Amob_CreateRefString( &mo.recTo, lpszTo );
               }
               else
               {
                  char * pszAddress;
                  pszAddress = strchr( msginfo.szAuthor, '@' );
                  if( pszAddress )
                     Amob_CreateRefString( &mo.recTo, msginfo.szAuthor );
                  else
                  {
                     Amdb_GetMailHeaderField( lpWindInfo->lpMessage, "To", lpszTo, LEN_MAILADDR+1, FALSE );
                     Amob_CreateRefString( &mo.recTo, lpszTo );
                  }
               }
            }
            else if( fGetMailDetails )
            {
               /* If we are mailing from a CIX topic, append @cix.compulink.co.uk so it goes to the
                * right place, not to the default domain
                */
               if( Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_CIX )
                  strcat( msginfo.szAuthor, "@cix.co.uk" );
               
               Amob_CreateRefString( &mo.recTo, msginfo.szAuthor );
            }
            /* If this is a non-comment folder, fill in the
            * subject field.
            */
            if( fGetMailDetails && IsNoCommentFolder( lpWindInfo->lpTopic, msginfo.szAuthor, NO_COMMENT, 10 ) )
               CreateMailSubject( &mo.recSubject, msginfo.szTitle, NULL );
         }
         else
         {
            lpszTo[ 0 ] = '\0';
            if( Amdb_GetTopicFlags( lpWindInfo->lpTopic, TF_LISTFOLDER ) )
               Amdb_GetCookie( lpWindInfo->lpTopic, MAIL_LIST_ADDRESS, lpszTo, "", LEN_MAILADDR );
            if( *lpszTo )
               Amob_CreateRefString( &mo.recTo, lpszTo );
         }
         
      }
      else if( Ameol2_GetWindowType() == WIN_RESUMES )
      {
         GetResumeUserName( hwndResumes, lpszTo, LEN_MAILADDR+1 );
         Amob_CreateRefString( &mo.recTo, lpszTo );
      }
      else if( Ameol2_GetWindowType() == WIN_PARLIST )
      {
         LPSTR lpParList;
         
         /* Active window is a participants list window, so get
         * the selected participants.
         */
         lpParList = GetParticipants( hwndActive, TRUE );
         if( NULL != lpParList )
            Amob_CreateRefString( &mo.recTo, lpParList );
      }
      FreeMemory( &lpszTo );
      }
      
      if( !fUseInternet || nMailDelivery == MAIL_DELIVER_CIX )
         mo.wEncodingScheme = ENCSCH_UUENCODE;
      else
         mo.wEncodingScheme = wDefEncodingScheme;
      
      mo.nPriority = MAIL_PRIORITY_NORMAL;
      if( NULL != curmsg.ptl && Amdb_GetTopicFlags( curmsg.ptl, TF_MAILINGLIST ) )
         mo.wFlags = MOF_NOECHO;
      else
         mo.wFlags = fCCMailToSender ? 0 : MOF_NOECHO;
      
      /* Set the Reply-To address.
      */
      if( fUseInternet )
      {
         CreateReplyAddress( &mo.recReplyTo, curmsg.ptl );
         CreateMailMailAddress( &mo.recFrom, curmsg.ptl );
      }
      
      /* Set the default text.
      */
      if( NULL != lpszText )
         Amob_CreateRefString( &mo.recText, lpszText );
      CreateMailWindow( hwndFrame, &mo, NULL );
      Amob_Delete( OBTYPE_MAILMESSAGE, &mo );
}

/* This function composes a mail reply.
 */
void FASTCALL CmdMailReply( HWND hwnd, BOOL fAll, BOOL fMailOnly )
{
   LPWINDINFO lpWindInfo;
   MAILOBJECT mo;
   LPOB lpob;

   lpWindInfo = GetBlock( hwnd );
   if( fMailOnly && Amdb_GetTopicType( lpWindInfo->lpTopic ) != FTYPE_MAIL )
      return;
   Amob_New( OBTYPE_MAILMESSAGE, &mo );
   if( lpWindInfo->lpMessage )
      {
      LPSTR lpszTo;
      LPSTR lpszCC;

      INITIALISE_PTR(lpszTo);
      INITIALISE_PTR(lpszCC);
      
      /* Allocate some memory buffers.
       */
      fNewMemory( &lpszTo, LEN_TOCCLIST+1 );
      fNewMemory( &lpszCC, LEN_TOCCLIST+1 );
      if( NULL != lpszTo && NULL != lpszCC )
         {
         char sz[ 128 ];

         MSGINFO msginfo;
         PTH pth;

         /* When replying to a comment we posted, find the original
          * message.
          */
         pth = lpWindInfo->lpMessage;
         Amdb_GetMsgInfo( pth, &msginfo );

         /* Create the subject line for the reply.
          */
         CreateMailSubject( &mo.recSubject, msginfo.szTitle, pth );
   
         /* The folder name is the mailbox name and the reply-to
          * name.
          */
         if( !fMailOnly )
            {
            PTL ptl;

            /* Find first mail folder.
             */
            ptl = GetPostmasterFolder();
            ASSERT( NULL != ptl );
            WriteFolderPathname( sz, ptl );
            }
         else
            WriteFolderPathname( sz, lpWindInfo->lpTopic );
         Amob_CreateRefString( &mo.recMailbox, sz );
   
         /* If we're replying to all, then collect the names in the
          * To: and CC field and combine them.
          */
         *lpszCC = '\0';
         if( fAll )
            {
            int length;
            BOOL fOK;
            char * pszAddress;

            /* Get the From field.
             */
            strcpy( lpszTo, msginfo.szAuthor );
            pszAddress = strchr( msginfo.szAuthor, '@' );

            /* First get the To fields and strip out the original message author
             */
            strcat( lpszTo, " " );
            length = strlen( lpszTo );
            fOK = GetReplyMailHeaderField( pth, "To", lpszTo + length, ( LEN_TOCCLIST - length ) + 1 );
            StripOutName( lpszTo + length, msginfo.szAuthor );
            StripOutName( lpszTo + length, szCIXNickname );
            StripOutName( lpszTo + length, szMailAddress );
            StripOutName( lpszTo + length, pszAddress );

            /* If we didn't get all the original To fields, display a
             * warning.
             */
            if( !fOK && !( Amdb_GetTopicFlags( lpWindInfo->lpTopic, TF_LISTFOLDER ) ) )
               fMessageBox( hwnd, 0, GS(IDS_STR1206), MB_OK|MB_ICONINFORMATION );
   
            /* Then get the CC fields and strip out the original message author from there too!
             */
            fOK = GetReplyMailHeaderField( pth, "Cc", lpszCC, LEN_TOCCLIST + 1 );

            StripOutName( lpszCC, msginfo.szAuthor );
            StripOutName( lpszCC, szCIXNickname );
            StripOutName( lpszCC, szMailAddress );
            StripOutName( lpszCC, pszAddress );

            /* If we didn't get all the original CC fields, display a
             * warning.
             */
            if( !fOK && !( Amdb_GetTopicFlags( lpWindInfo->lpTopic, TF_LISTFOLDER ) ) )
               fMessageBox( hwnd, 0, GS(IDS_STR1206), MB_OK|MB_ICONINFORMATION );
            }

         else if( !fMailOnly && Amdb_GetTopicType( lpWindInfo->lpTopic ) != FTYPE_NEWS )
         {
            if( Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_CIX ) // !!SM!! 2038
               strcat( msginfo.szAuthor, "@cix.co.uk" );
            strcpy( lpszTo, msginfo.szAuthor );
         }
         else if( ( msginfo.dwFlags & MF_OUTBOXMSG ) || _strcmpi( msginfo.szAuthor, szCIXNickname ) == 0 )
         {
            Amdb_GetMailHeaderField( pth, "To", lpszTo, LEN_TOCCLIST + 1, FALSE );
            StripLeadingTrailingChars( lpszTo, '<' );
            StripLeadingTrailingChars( lpszTo, '>' );
         }
         else
            {
            Amdb_GetMailHeaderField( pth, "Reply-To", lpszTo, LEN_MAILADDR + 1, FALSE );
            if( ( *lpszTo == '\0' ) || ( ( lpszTo[ 0 ] == '<' || lpszTo[ 0 ] == '"' ) && ( lpszTo[ 1 ] == '@' || lpszTo[ 1 ] == '>' ) ) )
               {
               if( *lpszTo != '\0' && ( lpszTo[ 1 ] == '@' || lpszTo[ 1] == '>' ) )
                  fMessageBox( hwnd, 0, "Reply address is invalid - trying From: address", MB_OK|MB_ICONINFORMATION );
               Amdb_GetMailHeaderField( pth, "From", lpszTo, LEN_MAILADDR + 1, FALSE );
               if( *lpszTo == '\0' )
                  Amdb_GetMailHeaderField( pth, "Sender", lpszTo, LEN_MAILADDR + 1, FALSE );
               }
            if( *lpszTo != '\0' )
               ParseToField( lpszTo, lpszTo, LEN_MAILADDR );
            }

         if( Amdb_GetTopicFlags( lpWindInfo->lpTopic, TF_LISTFOLDER ) )
         {
            Amdb_GetCookie( lpWindInfo->lpTopic, MAIL_LIST_ADDRESS, lpszTo, "", LEN_MAILADDR );
            *lpszCC = '\0';
         }
   
         /* Add the original author as the recipient.
          */
         if( *lpszTo )
            Amob_CreateRefString( &mo.recTo, lpszTo );
   
         /* Add the CC stuff.
          */
         if( *lpszCC )
            Amob_CreateRefString( &mo.recCC, lpszCC );
   
         /* Add the message ID of the message to which we're replying as
          * the reply-to message number.
          */
         Amob_CreateRefString( &mo.recReply, msginfo.szReply );
         mo.dwReply = msginfo.dwMsg;
         }

      /* Free memory.
       */
      if( NULL != lpszTo ) FreeMemory( &lpszTo );
      if( NULL != lpszCC ) FreeMemory( &lpszCC );
      }

   if( !fUseInternet || nMailDelivery == MAIL_DELIVER_CIX )
      mo.wEncodingScheme = ENCSCH_UUENCODE;
   else
      mo.wEncodingScheme = wDefEncodingScheme;
   
   mo.nPriority = MAIL_PRIORITY_NORMAL;
   if( Amdb_GetTopicFlags( lpWindInfo->lpTopic, TF_MAILINGLIST ) )
      mo.wFlags = MOF_NOECHO;
   else
      mo.wFlags = fCCMailToSender ? 0 : MOF_NOECHO;

   /* Set the default sender details.
    */
   wsprintf( lpTmpBuf, "%s (%s)", (LPSTR)szMailAddress, (LPSTR)szFullName );
   Amob_CreateRefString( &mo.recFrom, lpTmpBuf );

   /* Set the Reply-To address.
    */
   if( fUseInternet )
      {
      CURMSG curmsg;

      Ameol2_GetCurrentTopic( &curmsg );
      CreateReplyAddress( &mo.recReplyTo, curmsg.ptl );
      CreateMailMailAddress( &mo.recFrom, curmsg.ptl );
      }

   /* Finally, compose the quote reply if automatic
    * quoting is enabled.
    */
   if( fAutoQuoteMail && hwndQuote && ( Amdb_GetTopicType( lpWindInfo->lpTopic ) != FTYPE_CIX ) )
      CreateFollowUpQuote( lpWindInfo->lpMessage, &mo.recText, szQuoteMailHeader );

   /* Locate an existing reply and offer to edit that if found.
    */
   if( NULL != ( lpob = Amob_Find( OBTYPE_MAILMESSAGE, &mo ) ) )
      {
      OBINFO obinfo;
      register int r;

      Amob_GetObInfo( lpob, &obinfo );
      if (!fInitiatingBlink && Amob_IsEditable( obinfo.obHdr.clsid ) && !TestF(obinfo.obHdr.wFlags, OBF_PENDING) && !TestF(obinfo.obHdr.wFlags, OBF_ACTIVE ) )
      {
         r = fMessageBox( hwnd, 0, GS(IDS_STR64), MB_YESNOCANCEL|MB_ICONINFORMATION );
         if( r == IDCANCEL )
            return;
         if( r == IDYES )
         {
         CreateMailWindow( hwndFrame, obinfo.lpObData, lpob );
         Amob_Delete( OBTYPE_MAILMESSAGE, &mo );
         return;
         }
         else
         {
         CreateMailWindow( hwndFrame, &mo, NULL );
         Amob_Delete( OBTYPE_MAILMESSAGE, &mo );
         return;
         }
      }
      else
      {
         r = fMessageBox( hwnd, 0, GS(IDS_STR1256), MB_YESNO|MB_ICONINFORMATION );
         if( r == IDNO )
            return;
         else
         {           
         CreateMailWindow( hwndFrame, &mo, NULL );
         Amob_Delete( OBTYPE_MAILMESSAGE, &mo );
         return;
         }
      }
   }
   CreateMailWindow( hwndFrame, &mo, lpob );
   Amob_Delete( OBTYPE_MAILMESSAGE, &mo );
}

/* This function extracts a mail header field and copies all
 * e-mail addresses, removing any compulink domains.
 */
BOOL FASTCALL GetReplyMailHeaderField( PTH pth, char * pszField, char * pszDest, int cbMax )
{
   register int i;
   char * psz;
   
   BOOL fOk;

   fOk = Amdb_GetMailHeaderField( pth, pszField, lpTmpBuf, LEN_TEMPBUF, TRUE );
   ParseToField( lpTmpBuf, lpTmpBuf, LEN_TEMPBUF );
   psz = lpTmpBuf;
   for( i = 0; i < cbMax && *psz; )
      {
      char szLclBuf[ LEN_MAILADDR ];
      char * pszAddress;

      psz = ExtractMailAddress( psz, szLclBuf, LEN_MAILADDR );
      pszAddress = strchr( szLclBuf, '@' );
      if( pszAddress )
         if( IsCompulinkDomain( pszAddress + 1 ) && IsCompulinkDomain( szDefMailDomain ))
            *pszAddress = '\0';
      if( i + (int)strlen( szLclBuf ) + 1 < cbMax - 1 )
         {
         if( i > 0 )
               pszDest[ i++ ] = ' ';
            strcpy( &pszDest[ i ], szLclBuf );
            i += strlen( szLclBuf );
            
         }
      else
         {
         fOk = FALSE;
         break;
         }
      }
   return( fOk );
}

/* This function creates a reply address. By default, this is
 * the name of the mailbox specified by the ptl handle. If this
 * is NULL or there's no hostname, then use the reply address
 * set in the Preferences.
 */
void FASTCALL CreateReplyAddress( RECPTR * prcp, PTL ptl )
{
   if( NULL == ptl )
      Amob_CreateRefString( prcp, szMailReplyAddress );
   else
      {
      char szReplyTo[ 64 ];

      Amdb_GetCookie( ptl, MAIL_REPLY_TO_COOKIE_1, szReplyTo, "", sizeof(szReplyTo) );
      if( !*szReplyTo )
         strcpy( szReplyTo, szMailReplyAddress );
      Amob_CreateRefString( prcp, szReplyTo );
      }
}

/* This function creates a mail address. By default, this is
 * the name of the mailbox specified by the ptl handle. If this
 * is NULL or there's no hostname, then use the mail address
 * set in the Preferences.
 */
void FASTCALL CreateMailMailAddress( RECPTR * prcp, PTL ptl )
{
   if( NULL == ptl )
   {
      wsprintf( lpTmpBuf, "%s (%s)", (LPSTR)szMailAddress, (LPSTR)szFullName );
      Amob_CreateRefString( prcp, lpTmpBuf );
   }
   else
      {
      char szThisMailAddress[ 64 ];
      char szThisMailName[ 64 ];

      Amdb_GetCookie( ptl, MAIL_ADDRESS_COOKIE, szThisMailAddress, szMailAddress, sizeof(szThisMailAddress) );
      if( !*szThisMailAddress )
         strcpy( szThisMailAddress, szMailAddress );
      Amdb_GetCookie( ptl, MAIL_NAME_COOKIE, szThisMailName, szFullName, sizeof(szThisMailName) );
      if( !*szThisMailName )
         strcpy( szThisMailName, szFullName );
      wsprintf( lpTmpBuf, "%s (%s)", (LPSTR)szThisMailAddress, (LPSTR)szThisMailName );
      Amob_CreateRefString( prcp, lpTmpBuf );
      }
}

/* This function fills out the subject field.
 */
void FASTCALL CreateMailSubject( RECPTR * prcp, char * pszSubject, PTH pth )
{
   char * pszNewSubject;

   INITIALISE_PTR(pszNewSubject);
   if (pth)
   {
      Amdb_GetMailHeaderField( pth, "Subject", lpTmpBuf, LEN_TEMPBUF - 1, TRUE );
      if( *lpTmpBuf )
      {
         b64decodePartial(lpTmpBuf); // !!SM!!
         pszSubject = lpTmpBuf;
      }
   }
   if( _strnicmp( pszSubject, "Re:", 3 ) == 0 )
   {
      Amob_CreateRefString( prcp, pszSubject );
      return;
   }
   if (!fNewMemory( &pszNewSubject, strlen( pszSubject) + 32 ))
   {
      Amob_CreateRefString( prcp, pszSubject );
      return;
   }
   if( _strnicmp( pszSubject, "Re[", 3 ) == 0 && isdigit( pszSubject[3] ) )
      {
      int uReCount;

      uReCount = atoi( pszSubject + 3 ) + 1;
      wsprintf( pszNewSubject, "Re[%u]: %s", uReCount, (LPSTR)pszSubject + 6 );
      Amob_CreateRefString( prcp, pszNewSubject );
      FreeMemory( &pszNewSubject );
      return;
      }
   wsprintf( pszNewSubject, "Re: %s", (LPSTR)pszSubject );
   Amob_CreateRefString( prcp, pszNewSubject );
   FreeMemory( &pszNewSubject );
}

/* This function creates the mail editor window.
 */
HWND FASTCALL CreateMailWindow( HWND hwnd, MAILOBJECT FAR * lpmoInit, LPOB lpob )
{
   LPMAILWNDINFO lpmwi;
   HWND hwndMail;
   HWND hwndFocus;
   DWORD dwState;
   int cxBorder;
   int cyBorder;
   RECT rc;

   /* Look for existing window
    */
   if( NULL != lpob )
      if( hwndMail = Amob_GetEditWindow( lpob ) )
         {
         BringWindowToTop( hwndMail );
         return( hwndMail );
         }

   /* Register the group list window class if we have
    * not already done so.
    */
   if( !fRegistered )
      {
      WNDCLASS wc;

      wc.style       = CS_HREDRAW | CS_VREDRAW;
      wc.lpfnWndProc    = MailWndProc;
      wc.hIcon       = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_MAIL) );
      wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
      wc.lpszMenuName      = NULL;
      wc.cbWndExtra     = DLGWINDOWEXTRA;
      wc.cbClsExtra     = 0;
      wc.hbrBackground  = (HBRUSH)( COLOR_APPWORKSPACE + 1 );
      wc.lpszClassName  = szMailWndClass;
      wc.hInstance      = hInst;
      if( !RegisterClass( &wc ) )
         return( NULL ); 
      fRegistered = TRUE;
      }

   /* The window will appear in the left half of the
    * client area.
    */
   GetClientRect( hwndMDIClient, &rc );
   rc.bottom -= GetSystemMetrics( SM_CYICON );
   cyBorder = ( rc.bottom - rc.top ) / 6;
   cxBorder = ( rc.right - rc.left ) / 6;
   InflateRect( &rc, -cxBorder, -cyBorder );
   dwState = 0;

   /* Get the actual window size, position and state.
    */
   ReadProperWindowState( szMailWndName, &rc, &dwState );

   /* If lpob is NULL, this is a new msg.
    */
   fNewMsg = lpob == NULL;

   /* Create the window.
    */
   hwndMail = Adm_CreateMDIWindow( szMailWndName, szMailWndClass, hInst, &rc, dwState, (LPARAM)lpmoInit );
   if( NULL == hwndMail )
      return( FALSE );

   /* Rename the Send button to Save if we're offline.
    */
   if( !fOnline )
      SetWindowText( GetDlgItem( hwndMail, IDOK ), "&Save" );

   /* Store the handle of the destination structure.
    */
   lpmwi = GetMailWndInfo( hwndMail );
   lpmwi->lpob = lpob;
   lpmwi->dwReply = lpmoInit->dwReply;
   lpmwi->recMailbox.hpStr = NULL;
   Amob_CopyRefObject( &lpmwi->recMailbox, &lpmoInit->recMailbox );

   /* Determine where we put the focus.
    */
   if( '\0' == *DRF(lpmoInit->recTo) )
      hwndFocus = GetDlgItem( hwndMail, IDD_TO );
   else if( '\0' == *DRF(lpmoInit->recSubject) )
      hwndFocus = GetDlgItem( hwndMail, IDD_SUBJECT );
   else
      {
      hwndFocus = GetDlgItem( hwndMail, IDD_EDIT );
      if( NULL == lpob )
         {
         Edit_SetSel( hwndFocus, 32767, 32767 );
         Edit_ScrollCaret( hwndFocus );
         }
      }
   SetFocus( hwndFocus );
   lpmwi->hwndFocus = hwndFocus;
   if( NULL != lpob )
      Amob_SetEditWindow( lpob, hwndMail );
   
   /* Set the signature now.
    */
   if( fNewMsg )
      {
      SetEditSignature( GetDlgItem( hwndMail, IDD_EDIT ), lpmwi->ptl );
      Edit_SetModify( GetDlgItem( hwndMail, IDD_EDIT ), FALSE );
#ifndef USEBIGEDIT
      SendMessage( GetDlgItem( hwndMail, IDD_EDIT ), SCI_SETSAVEPOINT,  (WPARAM)0, (LPARAM)0L );   
#endif USEBIGEDIT
      }

   return( hwndMail ); //!!SM!! 2.55.2032
}

/* This is the main window procedure.
 */
LRESULT EXPORT CALLBACK MailWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, MailWnd_OnCreate );
      HANDLE_MSG( hwnd, WM_CLOSE, MailWnd_OnClose );
      HANDLE_MSG( hwnd, WM_SIZE, MailWnd_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, MailWnd_OnMove );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, Common_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, MailWnd_MDIActivate );
      HANDLE_MSG( hwnd, WM_COMMAND, MailWnd_OnCommand );
      HANDLE_MSG( hwnd, WM_MENUSELECT, MainWnd_OnMenuSelect );
      HANDLE_MSG( hwnd, WM_SETFOCUS, MailWnd_OnSetFocus );
      HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, MailWnd_OnAdjustWindows );
      HANDLE_MSG( hwnd, WM_CHANGEFONT, Common_ChangeFont );
      HANDLE_MSG( hwnd, WM_NOTIFY, MailWnd_OnNotify );

      case WM_APPCOLOURCHANGE:
      case WM_TOGGLEHEADERS: {  // 2.25.2031
#ifdef USEBIGEDIT
         SetWindowFont( GetDlgItem( hwnd, IDD_EDIT ), hEditFont, FALSE );
#else USEBIGEDIT
         SetEditorDefaults(GetDlgItem( hwnd, IDD_EDIT ), FALSE, lfEditFont, fWordWrap, fHotLinks , fMsgStyles, crEditText, crEditWnd );
#endif USEBIGEDIT
         break;
         }

      case WM_CONTEXTSWITCH:
         if( wParam == AECSW_ONLINE )
            SetDlgItemText( hwnd, IDOK, (BOOL)lParam ? "&Send" : "&Save" );
         break;

      case WM_GETMINMAXINFO: {
         MINMAXINFO FAR * lpMinMaxInfo;
         LRESULT lResult;

         /* Set the minimum tracking size so the client window can never
          * be resized below 24 lines.
          */
         lResult = Adm_DefMDIDlgProc( hwnd, message, wParam, lParam );
         lpMinMaxInfo = (MINMAXINFO FAR *)lParam;
         lpMinMaxInfo->ptMinTrackSize.x = 330;
         lpMinMaxInfo->ptMinTrackSize.y = 305;
         return( lResult );
         }

      default:
         break;
      }
   return( Adm_DefMDIDlgProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_SETFOCUS message.
 */
void FASTCALL MailWnd_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   LPMAILWNDINFO lpmwi;
   HWND hwndFocus;

   lpmwi = GetMailWndInfo( hwnd );
   hwndFocus = lpmwi->hwndFocus;
   if( hwndFocus )
      SetFocus( hwndFocus );
   iActiveMode = WIN_EDITS;
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL MailWnd_OnCreate( HWND hwnd, LPCREATESTRUCT lpCreateStruct )
{
   LPMDICREATESTRUCT lpMDICreateStruct;
   LPMAILWNDINFO lpmwi;
   char szReplyTo[ 64 ];
   MAILOBJECT FAR * lpm;
   LPCSTR lpszDlg;
   HWND hwndCombo;
   HWND hwndEdit;
   HWND hwndList;
   LPSTR lToAddr;
   PTL ptl;
   char szThisMailAddress[ LEN_MAILADDR + 1];
   char szThisMailName[ LEN_MAILADDR + 1 ];

   INITIALISE_PTR(lpmwi);

   /* Create the dialog window.
    */
   lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
   lpm = (MAILOBJECT FAR *)lpMDICreateStruct->lParam;
   lpszDlg = MAKEINTRESOURCE(IDDLG_MAILEDITOR);
   Adm_MDIDialog( hwnd, lpszDlg, lpMDICreateStruct );

   /* Create a structure to store the mail window info.
    */
   if( !fNewMemory( &lpmwi, sizeof(MAILWNDINFO) ) )
      return( FALSE );
   SetMailWndInfo( hwnd, lpmwi );
   lpmwi->hwndFocus = NULL;

   /* Set the edit window font.
    */
#ifdef USEBIGEDIT
   SetWindowFont( GetDlgItem( hwnd, IDD_EDIT ), hEditFont, FALSE );
#else USEBIGEDIT
   SetEditorDefaults(GetDlgItem( hwnd, IDD_EDIT ), FALSE, lfEditFont, fWordWrap, fHotLinks , fMsgStyles, crEditText, crEditWnd );
#endif USEBIGEDIT

   /* Hide the tag-reply option if there is no quote window.
    */
   if( !hwndQuote )
      ShowWindow( GetDlgItem( hwnd, IDD_TAGREPLY ), FALSE );

   /* Fill the list of signatures and set the
    * first one to (None).
    */
   FillSignatureList( hwnd, IDD_LIST );

   /* Fill the list of available encoding schemes.
    */
   FillEncodingScheme( hwnd, IDD_ENCODING, lpm->wEncodingScheme, TRUE, FALSE );

   /* Get the topic handle of the mailbox.
    */
   ptl = NULL;
   if( *DRF(lpm->recMailbox) )
      ParseFolderPathname( DRF(lpm->recMailbox), &ptl, FALSE, 0 );

   /* Select the signature.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   CommonSetSignature( hwndList, ptl );
   ComboBox_GetText( hwndList, lpmwi->szSigFile, sizeof(lpmwi->szSigFile) );

   /* Save topic handle.
    */
   lpmwi->ptl = ptl;

   /* Fill Reply-To field.
    */
   VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_REPLYTO ) );
   if( NULL != ptl )
      {
      Amdb_GetCookie( ptl, MAIL_REPLY_TO_COOKIE_1, szReplyTo, "", sizeof(szReplyTo) );
      if( *szReplyTo )
         ComboBox_InsertString( hwndCombo, -1, szReplyTo );
      Amdb_GetCookie( ptl, MAIL_REPLY_TO_COOKIE_2, szReplyTo, "", sizeof(szReplyTo) );
      if( *szReplyTo )
         ComboBox_InsertString( hwndCombo, -1, szReplyTo );
      Amdb_GetCookie( ptl, MAIL_REPLY_TO_COOKIE_3, szReplyTo, "", sizeof(szReplyTo) );
      if( *szReplyTo )
         ComboBox_InsertString( hwndCombo, -1, szReplyTo );
      Amdb_GetCookie( ptl, MAIL_REPLY_TO_COOKIE_4, szReplyTo, "", sizeof(szReplyTo) );
      if( *szReplyTo )
         ComboBox_InsertString( hwndCombo, -1, szReplyTo );
      Amdb_GetCookie( ptl, MAIL_REPLY_TO_COOKIE_5, szReplyTo, "", sizeof(szReplyTo) );
      if( *szReplyTo )
         ComboBox_InsertString( hwndCombo, -1, szReplyTo );
      Amdb_GetCookie( ptl, MAIL_REPLY_TO_COOKIE_6, szReplyTo, "", sizeof(szReplyTo) );
      if( *szReplyTo )
         ComboBox_InsertString( hwndCombo, -1, szReplyTo );
      }
   if( CB_ERR == ComboBox_FindString( hwndCombo, -1, szMailReplyAddress ) )
      ComboBox_InsertString( hwndCombo, -1, szMailReplyAddress );
   ComboBox_LimitText( hwndCombo, 63 );

   CheckDlgButton( hwnd, IDC_MAKEDEFAULT, BST_UNCHECKED );
   
   /* Fill in the various fields.
    */
   lToAddr = DRF(lpm->recTo);

   QuickReplace(lToAddr, lToAddr,"mailto:", ""); // FS#155

   Edit_SetText( GetDlgItem( hwnd, IDD_TO ), lToAddr );
   Edit_LimitText( GetDlgItem( hwnd, IDD_TO ), LEN_TOCCLIST - 5 );
   Edit_SetText( GetDlgItem( hwnd, IDD_CC ), DRF(lpm->recCC) );
   Edit_LimitText( GetDlgItem( hwnd, IDD_CC ), LEN_TOCCLIST - 5 );
   Edit_SetText( GetDlgItem( hwnd, IDD_BCC ), DRF(lpm->recBCC) );
   Edit_LimitText( GetDlgItem( hwnd, IDD_BCC ), LEN_TOCCLIST - 6 );
   Edit_SetText( GetDlgItem( hwnd, IDD_REPLYFIELD ), DRF(lpm->recReply) );
   Edit_SetText( GetDlgItem( hwnd, IDD_ATTACHMENT ), DRF(lpm->recAttachments) );
   if( fUseInternet && ( nMailDelivery != MAIL_DELIVER_CIX ) )
   {
      *szThisMailAddress = '\0';
      *szThisMailName = '\0';
   if( !*DRF(lpm->recFrom ) )
   {
      if( NULL != ptl )
      {
      Amdb_GetCookie( ptl, MAIL_ADDRESS_COOKIE, szThisMailAddress, "", sizeof(szThisMailAddress) );
      Amdb_GetCookie( ptl, MAIL_NAME_COOKIE, szThisMailName, "", sizeof(szThisMailName) );
      }
   }
   else
      ParseFromField( DRF(lpm->recFrom), szThisMailAddress, szThisMailName );
   wsprintf( lpTmpBuf, "Mail Message from %s (%s)", *szThisMailAddress ? szThisMailAddress : szMailAddress, *szThisMailName ? szThisMailName : szFullName );
   SetWindowText( hwnd, lpTmpBuf);
   }

   /* Set the priority fields.
    */
   if( lpm->nPriority == MAIL_PRIORITY_HIGHEST )
      CheckDlgButton( hwnd, IDD_HIGHPRIORITY, TRUE );
   if( lpm->nPriority == MAIL_PRIORITY_LOWEST )
      CheckDlgButton( hwnd, IDD_LOWPRIORITY, TRUE );

   /* Set the subject field.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_SUBJECT );
   Edit_SetText( hwndEdit, DRF(lpm->recSubject) );
   Edit_LimitText( hwndEdit, iMaxSubjectSize );

   /* Disable the OK button if the To field is blank and
    * the From field, because it can't be edited.
    */
   if( !fUseInternet )
      EnableControl( hwnd, IDOK, *DRF(lpm->recTo) );
   else
      {
      ComboBox_SetText( GetDlgItem( hwnd, IDD_REPLYTO ), DRF(lpm->recReplyTo) );
      ComboBox_SetEditSel( GetDlgItem( hwnd, IDD_REPLYTO ), 0, 0 );
      EnableControl( hwnd, IDOK, *DRF(lpm->recTo) && *DRF(lpm->recReplyTo) );
      }

   EnableControl( hwnd, IDD_REPLYTO, fUseInternet );
   ShowWindow( GetDlgItem( hwnd, IDD_RETURNRECEIPT ), ( fUseInternet && !( nMailDelivery == MAIL_DELIVER_CIX ) ) );

   /* Hide the encoding style field if no attachment.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_ATTACHMENT );
   ShowWindow( GetDlgItem( hwnd, IDD_ENCODING ), Edit_GetTextLength( hwndEdit ) > 0 );

   /* Subclass the edit control so we can do indents.
    */
   lpEditCtrlProc = SubclassWindow( GetDlgItem( hwnd, IDD_EDIT ), MailEditProc );

   /* Setup the WordWrap function !!SM!!
   */
#ifdef SMWORDBREAK
   SendDlgItemMessage(hwnd, IDD_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)(EDITWORDBREAKPROC)WordBreakProc) ;
#endif SMWORDBREAK

   /* Set the options.
    */
   if( !( lpm->wFlags & MOF_NOECHO ) )
      CheckDlgButton( hwnd, IDD_CCTOSENDER, TRUE );
   if( lpm->wFlags & MOF_RETURN_RECEIPT )
      CheckDlgButton( hwnd, IDD_RETURNRECEIPT, TRUE );

   /* Set the picture button bitmaps.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_TAGREPLY ), hInst, IDB_TAGREPLY );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_LOWPRIORITY ), hInst, IDB_LOWPRIORITY );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_HIGHPRIORITY ), hInst, IDB_HIGHPRIORITY );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_CCTOSENDER ), hInst, IDB_CCTOSENDER );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_RETURNRECEIPT ), hInst, IDB_RETURNRECEIPT );

   /* Fill the mail edit control.
    */
   SetEditText( hwnd, ptl, IDD_EDIT, DRF( lpm->recText ), FALSE );
#ifndef USEBIGEDIT
   SendMessage( GetDlgItem( hwnd, IDD_EDIT ), SCI_SETSAVEPOINT,  (WPARAM)0, (LPARAM)0L ); 
#endif USEBIGEDIT
   Amob_FreeRefObject( &lpm->recText );

   /* Add them tooltips.
    */
   AddTooltipsToWindow( hwnd, ctti, (LPTOOLTIPITEMS)&tti );
   return( TRUE );
}

/* This function handles indents in the edit window.
 */
LRESULT EXPORT CALLBACK MailEditProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   return( EditCtrlIndent( lpEditCtrlProc, hwnd, msg, wParam, lParam ) );
}

BOOL FASTCALL IsReplyCookie(PTL ptl, LPSTR pStr)
{
   int i;
   char szCookie[ 64 ];
   char lpAddr[ LEN_MAILADDR+1 ];

   for ( i=1; i<7; i++)
   {
      wsprintf( szCookie, "ReplyAddress%u", i );
      Amdb_GetCookie( ptl, szCookie, lpAddr, "", LEN_MAILADDR );
      if(_stricmp(lpAddr, pStr) == 0)
         return TRUE;
   }
   return FALSE;
}

LRESULT FASTCALL MailWnd_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
{
#ifndef USEBIGEDIT
   if( lpNmHdr->idFrom == IDD_EDIT )
   {
      return Lexer_OnNotify( hwnd, idCode, lpNmHdr );
   }
#endif USEBIGEDIT
   return( FALSE );
}


/* This function processes the WM_COMMAND message.
 */
void FASTCALL MailWnd_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   if( codeNotify == EN_SETFOCUS )
   {
      LPMAILWNDINFO lpmwi;

      lpmwi = GetMailWndInfo( hwnd );
      lpmwi->hwndFocus = hwndCtl;
   }

   CheckMenuStates();
   switch( id )
      {
      case IDD_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsMAILEDITOR );
         break;

      case IDM_QUICKPRINT:
      case IDM_PRINT:
         CmdComposedPrint( hwnd );
         break;

      case IDC_MAKEDEFAULT: /*!!SM!!*/
         {
            if( codeNotify == BN_CLICKED )
            {
               if( IsDlgButtonChecked( hwnd, IDC_MAKEDEFAULT ) )
                  CheckDlgButton( hwnd, IDC_MAKEDEFAULT, BST_UNCHECKED);
               else
                  CheckDlgButton( hwnd, IDC_MAKEDEFAULT, BST_CHECKED);
            }
            break;
         }

      case IDD_ATTACHMENTSBTN: {
         HWND hwndAttach;
         int length;

         BEGIN_PATH_BUF;
         VERIFY( hwndAttach = GetDlgItem( hwnd, IDD_ATTACHMENT ) );

         Edit_GetText( hwndAttach, lpPathBuf, _MAX_PATH+1 );
         length = Edit_GetTextLength( hwndAttach );
         if( length > 0 )
         {
            LPSTR lpFileBuf;
            LPSTR lpFileBuf2;
            BOOL fOk = TRUE;
         
            INITIALISE_PTR(lpFileBuf);
            fNewMemory( &lpFileBuf, length + 1 );
            INITIALISE_PTR(lpFileBuf2);
            fNewMemory( &lpFileBuf2, length + 1 );
            Edit_GetText( hwndAttach, lpFileBuf, length + 1 );


            while( *lpFileBuf && fOk )
            {
            /* Extract one filename.
             */
            lpFileBuf2 = ExtractFilename( lpPathBuf, lpFileBuf );
            lstrcpy( lpFileBuf, lpFileBuf2 );

            if( !Amfile_QueryFile( lpPathBuf ) )
               fOk = FALSE;
            }
            if( lpFileBuf )
               FreeMemory( &lpFileBuf );
         }

         if( CommonGetOpenFilename( hwnd, lpPathBuf, "Attach File", NULL, "CachedBinmailDir" ) )
            {
            wsprintf( lpTmpBuf, "<%s>", lpPathBuf );
            Edit_ReplaceSel( hwndAttach, lpTmpBuf );
            Edit_SetSel( hwndAttach, 32767, 32767 );
            ShowWindow( GetDlgItem( hwnd, IDD_ENCODING ), Edit_GetTextLength( hwndAttach ) > 0 );
            }
         END_PATH_BUF;
         break;
         }

      case IDD_LOWPRIORITY:
         if( IsDlgButtonChecked( hwnd, id ) )
            CheckDlgButton( hwnd, IDD_HIGHPRIORITY, FALSE );
         break;

      case IDD_HIGHPRIORITY:
         if( IsDlgButtonChecked( hwnd, id ) )
            CheckDlgButton( hwnd, IDD_LOWPRIORITY, FALSE );
         break;

      case IDM_SPELLCHECK: {
         DWORD dwSel = 0;

         dwSel = Edit_GetSel( GetDlgItem( hwnd, IDD_EDIT ) );
         if( LOWORD( dwSel ) != HIWORD( dwSel ) )
            Ameol2_SpellCheckDocument( hwnd, GetDlgItem( hwnd, IDD_EDIT ), SC_FL_KEEPSESSION );
         else if( Ameol2_SpellCheckDocument( hwnd, GetDlgItem( hwnd, IDD_SUBJECT ), SC_FL_KEEPSESSION ) == SP_OK )
            Ameol2_SpellCheckDocument( hwnd, GetDlgItem( hwnd, IDD_EDIT ), SC_FL_USELASTSESSION|SC_FL_DIALOG );
         break;
         }

      case IDD_PAD4:
      case IDD_PAD1:
      case IDD_PAD8:
         CommonPickerCode( hwnd, IDD_TO, IDD_CC, IDD_BCC, id == IDD_PAD4, id == IDD_PAD8 );
         break;

      case IDD_LIST:
         if( codeNotify == CBN_SELCHANGE )
            {
            LPMAILWNDINFO lpmwi;

            lpmwi = GetMailWndInfo( hwnd );
            CommonSigChangeCode( hwnd, lpmwi->szSigFile, FALSE, FALSE );
            }
         break;

      case IDD_REPLYTO:
         if( codeNotify == CBN_EDITCHANGE || codeNotify == CBN_SELCHANGE )
            PostCommand( hwnd, IDD_TO, EN_CHANGE );
         break;

      case IDD_TO:
         /* The To and Reply To fields are mandatory. If they are blank, don't let the user
          * finish the mail message.
          */
         if( codeNotify == EN_CHANGE )
            {
            int cbTo;
            int cbReplyTo;

            cbTo = Edit_GetTextLength( GetDlgItem( hwnd, IDD_TO ) );
            cbReplyTo = fUseInternet ? ComboBox_GetTextLength( GetDlgItem( hwnd, IDD_REPLYTO ) ) : 1;

            /*!!SM!!*/
            if ( (!fUseInternet || ( nMailDelivery == MAIL_DELIVER_CIX )) && !CheckValidMailAddress(GetDlgItem( hwnd, IDD_TO )))
               EnableControl( hwnd, IDOK,  FALSE );
            else
               EnableControl( hwnd, IDOK, (cbReplyTo > 0) ); /*!!SM!!*/
            }
         break;

      case IDD_ATTACHMENT:
         if( codeNotify == EN_CHANGE && id == IDD_ATTACHMENT )
            ShowWindow( GetDlgItem( hwnd, IDD_ENCODING ), Edit_GetTextLength( hwndCtl ) > 0 );

      case IDD_SUBJECT:
      case IDD_EDIT:
         if( codeNotify == EN_SETFOCUS )
            {
            LPMAILWNDINFO lpmwi;

            lpmwi = GetMailWndInfo( hwnd );
            lpmwi->hwndFocus = hwndCtl;
            }
         else if( codeNotify == EN_ERRSPACE || codeNotify == EN_MAXTEXT )
            fMessageBox( hwnd, 0, GS(IDS_STR77), MB_OK|MB_ICONEXCLAMATION);
         break;

      case IDOK: {
         LPMAILWNDINFO lpmwi;
         MAILOBJECT mo;
         MAILOBJECT FAR * lpmo;
         HWND hwndEdit;
         HWND hwndSay;
         int length;
#ifndef USEBIGEDIT
         HEDIT hedit;
#endif USEBIGEDIT

         if ((!fUseInternet || ( nMailDelivery == MAIL_DELIVER_CIX )) && !CheckValidMailAddress(GetDlgItem( hwnd, IDD_TO ))) /*!!SM!!*/
         {
            fMessageBox( hwnd, 0, GS(IDS_STRING8508), MB_OK|MB_ICONEXCLAMATION ); 
            SetFocus( GetDlgItem( hwnd, IDD_TO ) );
            break;
         }

         if (!CheckForAttachment(GetDlgItem( hwnd, IDD_EDIT ), GetDlgItem( hwnd, IDD_ATTACHMENT )))
            break;

         /* First spell check the document.
          */
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_EDIT ) );
         VERIFY( hwndSay = GetDlgItem( hwnd, IDD_SUBJECT ) );
         Edit_SetSel( hwndEdit, 0, 0 );
         Edit_SetSel( hwndSay, 0, 0 );
         if( fAutoSpellCheck && ( EditMap_GetModified( hwndEdit ) || EditMap_GetModified( hwndSay ) ) )
            {
            int r;

            if( ( r = Ameol2_SpellCheckDocument( hwnd, hwndSay, SC_FL_KEEPSESSION ) ) == SP_CANCEL )
               break;
            if( r != SP_FINISH )
               if( Ameol2_SpellCheckDocument( hwnd, hwndEdit, SC_FL_USELASTSESSION ) == SP_CANCEL )
                  break;
            }

         /* Dereference the MAILOBJECT data structure.
          */
         lpmwi = GetMailWndInfo( hwnd );
         if( NULL == lpmwi->lpob )
            {
            Amob_New( OBTYPE_MAILMESSAGE, &mo );
            lpmo = &mo;
            lpmo->dwReply = lpmwi->dwReply;
            }
         else
            {
            OBINFO obinfo;

            Amob_GetObInfo( lpmwi->lpob, &obinfo );
            lpmo = obinfo.lpObData;
            }

         /* Get and update the In-Reply-To field.
          */
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_REPLYFIELD ) );
         length = Edit_GetTextLength( hwndEdit );
         if( Amob_CreateRefObject( &lpmo->recReply, length + 1 ) )
            {
            Edit_GetText( hwndEdit, lpmo->recReply.hpStr, length + 1 );
            Amob_SaveRefObject( &lpmo->recReply );
            }

         /* Get and update the Reply To field.
          */
         if( fUseInternet )
            {
            char szReplyTo[ 64 ];

            VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_REPLYTO ) );
            ComboBox_GetText( hwndEdit, szReplyTo, 64 );
            Amob_CreateRefString( &lpmo->recReplyTo, szReplyTo );
            Amob_SaveRefObject( &lpmo->recReplyTo );

            /* Save Reply-To address in cookie if it differs from the
             * current mail reply address.
             */
            if( NULL != lpmwi->ptl )
            {
               int index;
               int c;

               if( IsDlgButtonChecked( hwnd, IDC_MAKEDEFAULT ) ) /*!!SM!!*/
               {
                  Amdb_SetCookie( lpmwi->ptl, MAIL_REPLY_TO_COOKIE_1, DRF(lpmo->recReplyTo) );
                  c = 2;
               }
               else
               {
                  Amdb_SetCookie( lpmwi->ptl, MAIL_REPLY_TO_COOKIE_2, DRF(lpmo->recReplyTo) );
                  c = 3;
               }

               for( index = 0; index < 6, c < 6; ++index )
               {
                  char szCookie[ 64 ];

                  wsprintf( szCookie, "ReplyAddress%u", c );
                  if( CB_ERR == ComboBox_GetLBText( hwndEdit, index, szReplyTo ) )
                     break;

                  if( strcmp( szReplyTo, DRF(lpmo->recReplyTo ) ) != 0 )
                  {
//                   if( !IsReplyCookie(lpmwi->ptl, szReplyTo))
                     {
                        Amdb_SetCookie( lpmwi->ptl, szCookie, szReplyTo );
                        ++c;
                     }
                  }
               }
            }

            if( NULL != lpmwi->ptl )
            {
               Amdb_GetCookie( lpmwi->ptl, MAIL_NAME_COOKIE, lpPathBuf, szFullName, 64 );
               Amdb_GetCookie( lpmwi->ptl, MAIL_ADDRESS_COOKIE, lpTmpBuf, szMailAddress, 64 );
            }
            else
            {
               strcpy( lpPathBuf, szFullName );
               strcpy( lpTmpBuf, szMailAddress );
            }
            wsprintf( lpTmpBuf, "%s (%s)", lpTmpBuf, lpPathBuf );
            Amob_CreateRefString( &lpmo->recFrom, lpTmpBuf );
            Amob_SaveRefObject( &lpmo->recFrom );

            }

         /* Get and update the To field.
          */
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_TO ) );
         length = Edit_GetTextLength( hwndEdit );
         if(length > LEN_TOCCLIST - 5)
         {
            wsprintf( lpTmpBuf, GS(IDS_STRING8509), LEN_TOCCLIST - 5);
            fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION ); 
            SetFocus( GetDlgItem( hwnd, IDD_TO ) );
            break;
         }
         if( Amob_CreateRefObject( &lpmo->recTo, length + 1 ) )
            {
            Edit_GetText( hwndEdit, lpmo->recTo.hpStr, length + 1 );
            CleanAddresses(&lpmo->recTo, length);
            Amob_SaveRefObject( &lpmo->recTo );
            }

         /* Get and update the CC field.
          */
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_CC ) );
         length = Edit_GetTextLength( hwndEdit );
         if(length > LEN_TOCCLIST - 5)
         {
            wsprintf( lpTmpBuf, GS(IDS_STRING8510), LEN_TOCCLIST - 5);
            fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION ); 
            SetFocus( GetDlgItem( hwnd, IDD_CC ) );
            break;
         }
         if( Amob_CreateRefObject( &lpmo->recCC, length + 1 ) )
            {
            Edit_GetText( hwndEdit, lpmo->recCC.hpStr, length + 1 );
            CleanAddresses(&lpmo->recCC, length);
            Amob_SaveRefObject( &lpmo->recCC );
            }

         /* Get and update the BCC field.
          */
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_BCC ) );
         length = Edit_GetTextLength( hwndEdit );
         if(length > LEN_TOCCLIST - 6)
         {
            wsprintf( lpTmpBuf, GS(IDS_STRING8511), LEN_TOCCLIST - 6);
            fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION ); 
            SetFocus( GetDlgItem( hwnd, IDD_BCC ) );
            break;
         }
         if( Amob_CreateRefObject( &lpmo->recBCC, length + 1 ) )
            {
            Edit_GetText( hwndEdit, lpmo->recBCC.hpStr, length + 1 );
            CleanAddresses(&lpmo->recBCC, length);
            Amob_SaveRefObject( &lpmo->recBCC );
            }

         /* Get and update the Subject field.
          */
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_SUBJECT ) );
         length = Edit_GetTextLength( hwndEdit );
         if( length == 0  && fBlankSubjectWarning )
         {
            int nRetCode;

            nRetCode = fMessageBox( hwnd, 0, GS(IDS_STR1235), MB_YESNO|MB_ICONQUESTION );
            if( nRetCode == IDNO )
               {
               HighlightField( hwnd, IDD_SUBJECT );
               break;
               }
         }

         if( Amob_CreateRefObject( &lpmo->recSubject, length + 1 ) )
            {
            Edit_GetText( hwndEdit, lpmo->recSubject.hpStr, length + 1 );
            Amob_SaveRefObject( &lpmo->recSubject );
            }

         /* Get and update the Attachment field.
          */
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_ATTACHMENT ) );
         length = Edit_GetTextLength( hwndEdit );
         if( Amob_CreateRefObject( &lpmo->recAttachments, length + 1 ) )
            {
            Edit_GetText( hwndEdit, lpmo->recAttachments.hpStr, length + 1 );
            if( !VerifyFilenamesExist( hwnd, DRF(lpmo->recAttachments) ) )
               {
               HighlightField( hwnd, IDD_ATTACHMENT );
               break;
               }
            Amob_SaveRefObject( &lpmo->recAttachments );
            }

         /* And the encoding scheme if an attachment was specified.
          */
         if( !IsWindowVisible( GetDlgItem( hwnd, IDD_ENCODING ) ) )
            lpmo->wEncodingScheme = ENCSCH_NONE;
         else
            {
            lpmo->wEncodingScheme = GetEncodingScheme( hwnd, IDD_ENCODING );

            /* If MIME chosen and the address is a CIX conferencing address AND
             * the delivery scheme is not MAIL_DELIVER_IP, then ensure that we
             * deliver thru the ISP.
             */
            if( lpmo->wEncodingScheme == ENCSCH_MIME )
               if( !fUseInternet || nMailDelivery == MAIL_DELIVER_CIX )
                  {
                  fMessageBox( hwnd, 0, GS(IDS_STR973), MB_OK|MB_ICONINFORMATION );
                  break;
                  }

            /* Prohibit sending binary to an internet address.
             */
            if( !IsCixAddress( DRF(lpmo->recTo) ) && lpmo->wEncodingScheme == ENCSCH_BINARY )
               {
               fMessageBox( hwnd, 0, GS(IDS_STR972), MB_OK|MB_ICONINFORMATION );
               break;
               }

            /* For binary files, check every filename for CIX compliance.
             */
            if( lpmo->wEncodingScheme == ENCSCH_BINARY )
               if( !CheckValidCixFilenames( hwnd, DRF(lpmo->recAttachments) ) )
                  {
                  HighlightField( hwnd, IDD_ATTACHMENT );
                  break;
                  }
            }

         /* Warn if encoded attachment is over 10K
          */
         if( *DRF(lpmo->recAttachments) && lpmo->wEncodingScheme != ENCSCH_BINARY )
            if( !CheckAttachmentSize( hwnd, DRF(lpmo->recAttachments), lpmo->wEncodingScheme ) )
               break;

         /* Get the priority level.
          */
         lpmo->nPriority = 0;
         if( IsDlgButtonChecked( hwnd, IDD_HIGHPRIORITY ) )
            lpmo->nPriority = MAIL_PRIORITY_HIGHEST;
         if( IsDlgButtonChecked( hwnd, IDD_LOWPRIORITY ) )
            lpmo->nPriority = MAIL_PRIORITY_LOWEST;

         /* Set the mailbox.
          */
         Amob_CopyRefObject( &lpmo->recMailbox, &lpmwi->recMailbox );
         Amob_FreeRefObject( &lpmwi->recMailbox );

         /* Set any flags.
          */
         if( !IsDlgButtonChecked( hwnd, IDD_CCTOSENDER ) )
            lpmo->wFlags |= MOF_NOECHO;
         else
            lpmo->wFlags &= ~MOF_NOECHO;
         if( IsDlgButtonChecked( hwnd, IDD_RETURNRECEIPT ) )
            lpmo->wFlags |= MOF_RETURN_RECEIPT;
         else
            lpmo->wFlags &= ~MOF_RETURN_RECEIPT;

         /* Get and update the Text field.
          */
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_EDIT ) );
#ifdef USEBIGEDIT
         Edit_FmtLines( hwndEdit, TRUE );
#else USEBIGEDIT
         hedit = Editmap_Open( hwndEdit );
         Editmap_FormatLines( hedit, hwndEdit);
#endif USEBIGEDIT
         length = Edit_GetTextLength( hwndEdit );
         if( hwndQuote && IsDlgButtonChecked( hwnd, IDD_TAGREPLY ) )
         {
            LPWINDINFO lpWindInfo;

            if( NULL != ( lpWindInfo = GetBlock( hwndQuote ) ) )
               if( lpWindInfo->lpMessage )
                  {
                  LPSTR lpszText3;
                  LPSTR lpszText4;
                  LPSTR lpszText5;
                  LPSTR lp2;
                  int cbOriginal;

                  lpszText3 = Amdb_GetMsgText( lpWindInfo->lpMessage );

                  lpszText4 = NULL;

                  switch( Amdb_GetTopicType( lpWindInfo->lpTopic ) )
                  {
                     case FTYPE_NEWS:  
                     case FTYPE_MAIL:  
                        if( fStripHeaders ) // !!SM!! 2041
                           lpszText4 = GetReadableText(lpWindInfo->lpTopic, lpszText3);
                        else
                           lpszText4 = GetTextBody( lpWindInfo->lpTopic, lpszText3 );

                        if( fShortHeaders && fStripHeaders )
                        {
                           lpszText5 = GetShortHeaderText( lpWindInfo->lpMessage );
                           if( NULL != lpszText5 && *lpszText5 )
                           {
                              INITIALISE_PTR( lp2 );
                              if( fNewMemory32( &lp2, hstrlen( lpszText5 ) + strlen( lpszText4 ) + 1) )
                              {
                                 strcpy(lp2, lpszText5);
                                 hstrcat(lp2, lpszText4);
                                 lpszText4 = lp2;
                              }        
                           }
                        }
                        break;
                     case FTYPE_CIX:   
                     case FTYPE_LOCAL:
                        lpszText4 = lpszText3;
                        break;
                  }
                  
                  if (lpszText4 != NULL) 
                  {
                     cbOriginal = lstrlen( lpszText4 ) + strlen( GS(IDS_STR295) );
                     if( Amob_CreateRefObject( &lpmo->recText, length + cbOriginal + 1 ) )
                     {
                        Edit_GetText( hwndEdit, lpmo->recText.hpStr, length + 1 );
                        strcat( lpmo->recText.hpStr, GS(IDS_STR295) );
                        strcat( lpmo->recText.hpStr, lpszText4 );
                        Amob_SaveRefObject( &lpmo->recText );
                     }
                  }
                  Amdb_FreeMsgTextBuffer( lpszText3 );
               }
            }
         else if( Amob_CreateRefObject( &lpmo->recText, length + 1 ) )
            {
            Edit_GetText( hwndEdit, lpmo->recText.hpStr, length + 1 );
            Amob_SaveRefObject( &lpmo->recText );
            }
         Amob_Commit( lpmwi->lpob, OBTYPE_MAILMESSAGE, lpmo );
         RemoveTooltipsFromWindow( hwnd );
         Adm_DestroyMDIWindow( hwnd );
         
         break;
         }

      case IDCANCEL:
         /* Destroy the mail window.
          */
         MailWnd_OnClose( hwnd );
         break;
      }
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL MailWnd_OnAdjustWindows( HWND hwnd, int dx, int dy )
{
   Adm_MoveWnd( GetDlgItem( hwnd, IDOK ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDCANCEL ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_HELP ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_ENCODING ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDC_MAKEDEFAULT ), dx, 0 );
   
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_TO ), dx, 0 );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_CC ), dx, 0 );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_BCC ), dx, 0 );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_SUBJECT ), dx, 0 );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_ATTACHMENT ), dx, 0 );
#ifdef WIN32
   if( fUseInternet )
      Adm_InflateWnd( GetDlgItem( hwnd, IDD_REPLYTO ), dx, 0 );
#endif
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_EDIT ), dx, dy );
   SetEditWindowWidth( GetDlgItem( hwnd, IDD_EDIT ), iWrapCol );
}

/* This function handles the WM_CLOSE message.
 */
void FASTCALL MailWnd_OnClose( HWND hwnd )
{
   LPMAILWNDINFO lpmwi;
   BOOL fModified;

   /* Set fModified to TRUE if any of the input fields have been
    * modified.
    */
   fModified = EditMap_GetModified( GetDlgItem( hwnd, IDD_EDIT ) );
   fModified |= EditMap_GetModified( GetDlgItem( hwnd, IDD_TO ) );
   fModified |= EditMap_GetModified( GetDlgItem( hwnd, IDD_CC ) );
   fModified |= EditMap_GetModified( GetDlgItem( hwnd, IDD_BCC ) );
   fModified |= EditMap_GetModified( GetDlgItem( hwnd, IDD_SUBJECT ) );
   fModified |= EditMap_GetModified( GetDlgItem( hwnd, IDD_ATTACHMENT ) );
   fModified |= EditMap_GetModified( GetDlgItem( hwnd, IDD_REPLYTO ) );

   /* If anything modified, then get confirmation before closing
    * the dialog.
    */
   if( fModified )
      {
      int nRetCode;
      int nCode;

      if( hwndActive != hwnd )
         ShowWindow( hwnd, SW_RESTORE );
      nCode = fQuitting ? MB_YESNO : MB_YESNOCANCEL;
      nRetCode = fMessageBox( hwnd, 0, GS(IDS_STR418), nCode|MB_ICONQUESTION );
      if( nRetCode == IDCANCEL )
         {
         fQuitting = FALSE;
         return;
         }
      if( nRetCode == IDYES )
         {
         SendDlgCommand( hwnd, IDOK, 0 );
         return;
         }
      }
   lpmwi = GetMailWndInfo( hwnd );
   if( NULL != lpmwi->lpob )
      Amob_Uncommit( lpmwi->lpob );
   RemoveTooltipsFromWindow( hwnd );
   Adm_DestroyMDIWindow( hwnd );
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL MailWnd_OnMove( HWND hwnd, int x, int y )
{
   Amuser_WriteWindowState( szMailWndName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL MailWnd_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   /* Save the new window state to the configuration file.
    */
   Amuser_WriteWindowState( szMailWndName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* This function handles the WM_MDIACTIVATE message.
 */
void FASTCALL MailWnd_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   if( fActive )
      ToolbarState_CopyPaste();
   Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_EDITS ), (LPARAM)(LPSTR)hwnd );
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, Adm_DefMDIDlgProc );
}

/* This function implements the address book picker interface for
 * all mail windows.
 */
void FASTCALL CommonPickerCode( HWND hwnd, int nTo, int nCC, int nBCC, BOOL fPickCC, BOOL fPickBCC )
{
   AMADDRPICKLIST aPickList;
   HWND hwndEdit;
   LPSTR lpszTo;
   LPSTR lpszCC;
   LPSTR lpszBCC;
   int cbEdit;

   /* Initialise for empty fields.
    */
   aPickList.lpszTo = lpszTo = NULL;
   aPickList.lpszCC = lpszCC = NULL;
   aPickList.lpszBCC = lpszBCC = NULL;
   aPickList.wFlags = 0;
   if( fPickCC )
      aPickList.wFlags |= AMPCKFLG_SETCC;
   if( fPickBCC )
      aPickList.wFlags |= AMPCKFLG_SETBCC;

   /* Fill To array
    */
   hwndEdit = GetDlgItem( hwnd, nTo );
   if( ( cbEdit = Edit_GetTextLength( hwndEdit ) ) > 0 )
      if( fNewMemory( &lpszTo, cbEdit + 1 ) )
         {
         Edit_GetText( hwndEdit, lpszTo, cbEdit + 1 );
         aPickList.lpszTo = MakeCompactString( lpszTo );
         FreeMemory( &lpszTo );
         lpszTo = aPickList.lpszTo;
         }

   /* Fill CC array
    */
   hwndEdit = GetDlgItem( hwnd, nCC );
   if( ( cbEdit = Edit_GetTextLength( hwndEdit ) ) > 0 )
      if( fNewMemory( &lpszCC, cbEdit + 1 ) )
         {
         Edit_GetText( hwndEdit, lpszCC, cbEdit + 1 );
         aPickList.lpszCC = MakeCompactString( lpszCC );
         FreeMemory( &lpszCC );
         lpszCC = aPickList.lpszCC;
         }

   /* Fill BCC array
    */
   hwndEdit = GetDlgItem( hwnd, nBCC );
   if( ( cbEdit = Edit_GetTextLength( hwndEdit ) ) > 0 )
      if( fNewMemory( &lpszBCC, cbEdit + 1 ) )
         {
         Edit_GetText( hwndEdit, lpszBCC, cbEdit + 1 );
         aPickList.lpszBCC = MakeCompactString( lpszBCC );
         FreeMemory( &lpszBCC );
         lpszBCC = aPickList.lpszBCC;
         }

   /* Invoke the picker with the current To and CC fields.
    * On return, the picker will replace lpszTo and lpszCC
    * with pointers to the new values.
    */
   if( Amaddr_PickEntry( hwnd, &aPickList ) )
      {
      /* Set the To string.
       */
      hwndEdit = GetDlgItem( hwnd, nTo );
      if( NULL == aPickList.lpszTo )
         SetWindowText( hwndEdit, "" );
      else
         {
         UncompactString( aPickList.lpszTo );
         SetWindowText( hwndEdit, aPickList.lpszTo );
         FreeMemory( &aPickList.lpszTo );
         }

      /* Set the CC string.
       */
      hwndEdit = GetDlgItem( hwnd, nCC );
      if( NULL == aPickList.lpszCC )
         SetWindowText( hwndEdit, "" );
      else
         {
         UncompactString( aPickList.lpszCC );
         SetWindowText( hwndEdit, aPickList.lpszCC );
         FreeMemory( &aPickList.lpszCC );
         }

      /* Set the BCC string.
       */
      hwndEdit = GetDlgItem( hwnd, nBCC );
      if( NULL == aPickList.lpszBCC )
         SetWindowText( hwndEdit, "" );
      else
         {
         UncompactString( aPickList.lpszBCC );
         SetWindowText( hwndEdit, aPickList.lpszBCC );
         FreeMemory( &aPickList.lpszBCC );
         }
      }

   /* Whatever happens, deallocate our own strings.
    */
   if( lpszTo ) FreeMemory( &lpszTo );
   if( lpszCC ) FreeMemory( &lpszCC );
   if( lpszBCC ) FreeMemory( &lpszBCC );
}

/* This is the mail message out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_MailMessage( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   MAILOBJECT FAR * lpmo;

   lpmo = (MAILOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_GETTYPE: {
         DWORD dwType;

         dwType = 0;
         if( ( nMailDelivery == MAIL_DELIVER_IP ) && ( lpmo->wEncodingScheme != ENCSCH_BINARY ) )
            dwType = BF_POSTIPMAIL;
         else if( ( nMailDelivery == MAIL_DELIVER_IP ) && ( lpmo->wEncodingScheme == ENCSCH_BINARY ) )
            dwType = BF_POSTCIXMAIL;
         else if( nMailDelivery == MAIL_DELIVER_CIX )
            dwType = BF_POSTCIXMAIL;
         else if( nMailDelivery == MAIL_DELIVER_AI )
            {
            if( IsCixAddress( DRF( lpmo->recTo ) ) && lpmo->wEncodingScheme != ENCSCH_MIME )
               dwType = BF_POSTCIXMAIL;
            else
            dwType = BF_POSTIPMAIL;
            }
         ASSERT( dwType != 0 );
         return( (LRESULT) dwType );
         }

      case OBEVENT_GETCLSID:
         if( ( nMailDelivery == MAIL_DELIVER_AI ) && IsCixAddress( DRF(lpmo->recTo) ) )
            return( lpmo->wEncodingScheme == ENCSCH_BINARY ? Amob_GetNextObjectID() : 2 );
         if( nMailDelivery == MAIL_DELIVER_CIX )
            return( lpmo->wEncodingScheme == ENCSCH_BINARY ? Amob_GetNextObjectID() : 2 );
         return( Amob_GetNextObjectID() );

      case OBEVENT_EXEC:
         /* If AI delivery and any attachment is being sent via MIME, then do NOT
          * send via CIX conferencing!
          */
         if( nMailDelivery == MAIL_DELIVER_AI && IsCixAddress( DRF(lpmo->recTo) ) )
            if( !( *DRF(lpmo->recAttachments) && lpmo->wEncodingScheme == ENCSCH_MIME ) )
               return( POF_HELDBYSCRIPT );
         if( nMailDelivery == MAIL_DELIVER_CIX )
            {
            if( lpmo->wEncodingScheme == ENCSCH_MIME )
               return( POF_CANNOTACTION );
            return( POF_HELDBYSCRIPT );
            }
         /* If IP delivery and the attachment is binary, send it via conferencing.
          */
         if( nMailDelivery == MAIL_DELIVER_IP && IsCixAddress( DRF(lpmo->recTo) ) )
            if( ( *DRF(lpmo->recAttachments) && lpmo->wEncodingScheme == ENCSCH_BINARY ) )
               return( POF_HELDBYSCRIPT );
         return( Exec_MailMessage( dwData ) );

      case OBEVENT_EDITABLE:
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR256), DRF(lpmo->recTo), DRF(lpmo->recSubject) );
         return( TRUE );

      case OBEVENT_NEW:
         memset( lpmo, 0, sizeof( MAILOBJECT ) );
         lpmo->wEncodingScheme = 0;
         lpmo->nPriority = MAIL_PRIORITY_NORMAL;
         lpmo->wFlags = fCCMailToSender ? 0 : MOF_NOECHO;
         return( TRUE );

      case OBEVENT_LOAD: {
         MAILOBJECT mo;

         Amob_LoadDataObject( fh, &mo, sizeof( MAILOBJECT ) );
         if( fNewMemory( &lpmo, sizeof( MAILOBJECT ) ) )
            {
            *lpmo = mo;
            lpmo->recFrom.hpStr = NULL;
            lpmo->recTo.hpStr = NULL;
            lpmo->recReplyTo.hpStr = NULL;
            lpmo->recBCC.hpStr = NULL;
            lpmo->recCC.hpStr = NULL;
            lpmo->recText.hpStr = NULL;
            lpmo->recReply.hpStr = NULL;
            lpmo->recSubject.hpStr = NULL;
            lpmo->recAttachments.hpStr = NULL;
            lpmo->recMailbox.hpStr = NULL;
            }
         return( (LRESULT)lpmo );
         }

      case OBEVENT_EDIT: {
         OBINFO obinfo;

         Amob_GetObInfo( (LPOB)dwData, &obinfo );
         CreateMailWindow( hwndFrame, obinfo.lpObData, (LPOB)dwData );
         return( TRUE );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpmo->recFrom );
         Amob_SaveRefObject( &lpmo->recTo );
         Amob_SaveRefObject( &lpmo->recReplyTo );
         Amob_SaveRefObject( &lpmo->recCC );
         Amob_SaveRefObject( &lpmo->recBCC );
         Amob_SaveRefObject( &lpmo->recSubject );
         Amob_SaveRefObject( &lpmo->recText );
         Amob_SaveRefObject( &lpmo->recReply );
         Amob_SaveRefObject( &lpmo->recAttachments );
         Amob_SaveRefObject( &lpmo->recMailbox );
         return( Amob_SaveDataObject( fh, lpmo, sizeof( MAILOBJECT ) ) );

      case OBEVENT_COPY: {
         MAILOBJECT FAR * lpmoNew;

         INITIALISE_PTR( lpmoNew );
         lpmo = (MAILOBJECT FAR *)dwData;
         if( fNewMemory( &lpmoNew, sizeof( MAILOBJECT ) ) )
            {
            lpmoNew->dwReply = lpmo->dwReply;
            lpmoNew->wEncodingScheme = lpmo->wEncodingScheme;
            lpmoNew->nPriority = lpmo->nPriority;
            lpmoNew->wFlags = lpmo->wFlags;
            INITIALISE_PTR( lpmoNew->recFrom.hpStr );
            INITIALISE_PTR( lpmoNew->recTo.hpStr );
            INITIALISE_PTR( lpmoNew->recReplyTo.hpStr );
            INITIALISE_PTR( lpmoNew->recCC.hpStr );
            INITIALISE_PTR( lpmoNew->recBCC.hpStr );
            INITIALISE_PTR( lpmoNew->recText.hpStr );
            INITIALISE_PTR( lpmoNew->recReply.hpStr );
            INITIALISE_PTR( lpmoNew->recSubject.hpStr );
            INITIALISE_PTR( lpmoNew->recAttachments.hpStr );
            INITIALISE_PTR( lpmoNew->recMailbox.hpStr );
            Amob_CopyRefObject( &lpmoNew->recFrom, &lpmo->recFrom );
            Amob_CopyRefObject( &lpmoNew->recTo, &lpmo->recTo );
            Amob_CopyRefObject( &lpmoNew->recReplyTo, &lpmo->recReplyTo );
            Amob_CopyRefObject( &lpmoNew->recCC, &lpmo->recCC );
            Amob_CopyRefObject( &lpmoNew->recBCC, &lpmo->recBCC );
            Amob_CopyRefObject( &lpmoNew->recText, &lpmo->recText );
            Amob_CopyRefObject( &lpmoNew->recReply, &lpmo->recReply );
            Amob_CopyRefObject( &lpmoNew->recSubject, &lpmo->recSubject );
            Amob_CopyRefObject( &lpmoNew->recAttachments, &lpmo->recAttachments );
            Amob_CopyRefObject( &lpmoNew->recMailbox, &lpmo->recMailbox );
            }
         return( (LRESULT)lpmoNew );
         }

      case OBEVENT_FIND: {
         MAILOBJECT FAR * lpmo1;
         MAILOBJECT FAR * lpmo2;

         lpmo1 = (MAILOBJECT FAR *)dwData;
         lpmo2 = (MAILOBJECT FAR *)lpBuf;
         if( strcmp( DRF(lpmo1->recMailbox), DRF(lpmo2->recMailbox) ) != 0 )
            return( FALSE );
         if( lpmo1->dwReply != lpmo2->dwReply )
            return( FALSE );
         return( TRUE );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpmo );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpmo->recFrom );
         Amob_FreeRefObject( &lpmo->recTo );
         Amob_FreeRefObject( &lpmo->recReplyTo );
         Amob_FreeRefObject( &lpmo->recCC );
         Amob_FreeRefObject( &lpmo->recBCC );
         Amob_FreeRefObject( &lpmo->recSubject );
         Amob_FreeRefObject( &lpmo->recReply );
         Amob_FreeRefObject( &lpmo->recText );
         Amob_FreeRefObject( &lpmo->recAttachments );
         Amob_FreeRefObject( &lpmo->recMailbox );
         return( TRUE );
      }
   return( 0L );
}

/* Given an e-mail address, this function determines whether the
 * address specifies CIX or an external CIX e-mail address.
 */
BOOL FASTCALL IsCixAddress( char * pszAddress )
{
   char szFirstAddr[ LEN_MAILADDR+1 ];
   char * pszAddr;

   /* Get the first address from the list.
    */
   ExtractMailAddress( pszAddress, szFirstAddr, LEN_MAILADDR );
   StripLeadingTrailingQuotes( szFirstAddr );
   if( Amaddr_IsGroup( szFirstAddr ) )
      Amaddr_ExpandGroup( szFirstAddr, 0, szFirstAddr );
   if( NULL != ( pszAddr = strchr( szFirstAddr, '@' ) ) )
      {
      /* Address has a domain specified, so return TRUE if
       * it is compulink.
       */
      if( IsCompulinkDomain( pszAddr + 1 ) )
         return( TRUE );
      return( FALSE );
      }

   /* No domain specified, so use the default.
    */
   return( IsCompulinkDomain( szDefMailDomain ) );
}

/* This function handles the signature change in the edit
 * field.
 */
void FASTCALL CommonSigChangeCode( HWND hwnd, char * pszSigFile, BOOL fSetFocus, BOOL fTop )
{
   char szNewSig[ 20 ];
   HWND hwndCtl;
   HWND hwndEdit;

   /* Get the newly selected signature.
    */
   VERIFY( hwndCtl = GetDlgItem( hwnd, IDD_LIST ) );
   VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_EDIT ) );
   ASSERT( *pszSigFile );
   ComboBox_GetText( hwndCtl, szNewSig, sizeof(szNewSig) );
   ASSERT( *szNewSig );
   ReplaceEditSignature( hwndEdit, pszSigFile, szNewSig, fTop );
   strcpy( pszSigFile, szNewSig );
   if( fSetFocus )
      SetFocus( hwndEdit );
}

/* This function changes the signature when a topic is selected
 * from the conference/topic list.
 */
void FASTCALL CommonTopicSigChangeCode( HWND hwnd, char * pszSigFile )
{
   char szTopicName[ LEN_TOPICNAME+1 ];
   char szConfName[ LEN_CONFNAME+1 ];
   HWND hwndList;
   int index;
   PCL pcl;
   PTL ptl;

   /* Get the selected conference/topic
    */
   GetDlgItemText( hwnd, IDD_CONFNAME, szConfName, LEN_CONFNAME+1 );
   GetDlgItemText( hwnd, IDD_TOPICNAME, szTopicName, LEN_TOPICNAME+1 );
   pcl = Amdb_OpenFolder( NULL, szConfName );
   ptl = Amdb_OpenTopic( pcl, szTopicName );

   /* Get the topic information and the default signature for the
    * topic from that.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   index = 0;
   if( NULL != ptl )
      {
      TOPICINFO topicinfo;

      Amdb_GetTopicInfo( ptl, &topicinfo );
      if( ( index = ComboBox_FindStringExact( hwndList, -1, topicinfo.szSigFile ) ) != CB_ERR )
         {
         ComboBox_SetCurSel( hwndList, index );
         CommonSigChangeCode( hwnd, pszSigFile, FALSE, FALSE );
         }
      }
}

/* This function fills a drop down list with a list of supported
 * encoding schemes and selects the specified one.
 */
void FASTCALL FillEncodingScheme( HWND hwnd, int id, UINT wEncodingScheme, BOOL fIncludeMime, BOOL fFromUsenet )
{
   BOOL fIncludeBinary;
   register int i;
   HWND hwndList;
   int index;

   /* Binary, only if CIX service installed.
    */
   fIncludeBinary = ( fUseCIX && !fFromUsenet );

   /* Override fIncludeMime if no means of sending mail
    * via the internet.
    */
   if( !fFromUsenet )
      if( !fUseInternet || nMailDelivery == MAIL_DELIVER_CIX )
      fIncludeMime = FALSE;

   /* First fill the list.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_ENCODING ) );
   for( i = 0; EncodingScheme[ i ].szDescription; ++i )
      {
      /* Don't allow MIME if we aren't using the internet or mail
       * is always delivered thru CIX.
       */
      if( ENCSCH_MIME == EncodingScheme[ i ].wID && !fIncludeMime )
         continue;
      if( ENCSCH_BINARY == EncodingScheme[ i ].wID && !fIncludeBinary )
         continue;
      index = ComboBox_InsertString( hwndList, -1, EncodingScheme[ i ].szDescription );
      ComboBox_SetItemData( hwndList, index, EncodingScheme[ i ].wID );
      }

   /* Select the mail encoding scheme.
    */
   index = 0;
   for( i = 0; EncodingScheme[ i ].szDescription; ++i )
      if( wEncodingScheme == EncodingScheme[ i ].wID )
         {
         index = ComboBox_FindStringExact( hwndList, -1, EncodingScheme[ i ].szDescription );
         break;
         }
   ComboBox_SetCurSel( hwndList, index );
}

/* This function returns the selected encoding scheme from
 * the drop-down list.
 */
UINT FASTCALL GetEncodingScheme( HWND hwnd, int id )
{
   WORD wEncodingScheme;
   HWND hwndList;

   VERIFY( hwndList = GetDlgItem( hwnd, IDD_ENCODING ) );
   if( !IsWindowVisible( hwndList ) )
      wEncodingScheme = ENCSCH_NONE;
   else
      {
      int index;

      /* Get the selected encoding scheme.
       */
      index = ComboBox_GetCurSel( hwndList );
      ASSERT( index != CB_ERR );
      wEncodingScheme = (int)ComboBox_GetItemData( hwndList, index );
      }
   return( wEncodingScheme );
}

/* This function appends the specified line to the memory buffer.
 */
void FASTCALL AppendToTextField( HPSTR * phpBuf, char * pstr )
{
   static DWORD dwBufSize;
   static DWORD dwMsgLength;
   HPSTR hpBuf;

   /* Cast to long to avoid wrap round because MAX_MSGSIZE is close to 0xFFFF
    */
   hpBuf = *phpBuf;
   if( hpBuf == NULL )
      {
      dwBufSize = (DWORD)strlen( pstr ) + 2L ;
      dwMsgLength = 0L;
      fNewMemory32( &hpBuf, dwBufSize );
      }
   else if( dwMsgLength + (DWORD)strlen( pstr ) + 2L >= dwBufSize )
      {
      dwBufSize += (DWORD)strlen( pstr ) + 2L;
      fResizeMemory32( &hpBuf, dwBufSize );
      }
   if( hpBuf )
      {
      if( *pstr == '.' )
         ++pstr;
      hmemcpy( hpBuf + dwMsgLength, pstr, strlen( pstr ) );
      dwMsgLength += strlen( pstr );
      hpBuf[ dwMsgLength++ ] = CR;
      hpBuf[ dwMsgLength++ ] = LF;
      hpBuf[ dwMsgLength ] = '\0';
      }
   *phpBuf = hpBuf;
}

/* This function extracts a mail address from a list of mail addresses.
 * The lpName parameter should point to the start of an address and, on
 * exit, the return value is a pointer past the extracted address.
 */
LPSTR FASTCALL ExtractMailAddress( LPSTR lpName, LPSTR lpOut, int nMax )
{
   register int c = 0;
   char chTerminator;
   int fInQuote;

   while( *lpName == ' ' )
      ++lpName;
   chTerminator = ' ';
   if( *lpName == '\'' )
      chTerminator = *lpName++;
   fInQuote = 0;
   while( *lpName )
      {
      if( *lpName == chTerminator && !fInQuote )
         break;
      if( *lpName == '\"' )
         fInQuote ^= 1;
      if( c < nMax - 1 )
         *lpOut++ = *lpName;
      ++lpName;
      ++c;
      }
   if( *lpName == chTerminator )
      ++lpName;
   *lpOut = '\0';
   while( *lpName == ' ' )
      ++lpName;
   return( lpName );
}

/* This function sets the correct signature for this message
 * in the drop-down list of signatures.
 */
void FASTCALL CommonSetSignature( HWND hwndList, PTL ptl )
{
   char * pszSig;
   int index;

   index = 0;
   if( NULL == ptl )
      pszSig = szGlobalSig;
   else
      {
      TOPICINFO topicinfo;

      Amdb_GetTopicInfo( ptl, &topicinfo );
      pszSig = topicinfo.szSigFile;
      }
   if( ( index = ComboBox_FindStringExact( hwndList, -1, pszSig ) ) == CB_ERR )
      index = 0;
   ComboBox_SetCurSel( hwndList, index );
}

/* This function creates a local copy of a mail message
 * containing an attachment.
 */
void FASTCALL CreateLocalCopyOfAttachmentMail( MAILOBJECT FAR * lpm, LPSTR lpMailHeader,
   int cEncodedParts, char * pszMsgId )
{
   char szMsgId[ 256 ];
   STRBUF mailbuf;
   HPSTR hpAttachments;
   BOOL fOk = TRUE;
   PTL ptl, ptll;
   PTH pth;

   StrBuf_Init( &mailbuf );
   ptl = GetPostmasterFolder();
   if( NULL != ptl )
      {
      TOPICINFO topicinfo;
      LPSTR lpszSubject;
      AM_DATE date;
      AM_TIME time;
      LPSTR lpszTo;
      DWORD cbMsg;
      HEADER temphdr;
      char szTempAuthor[ LEN_INTERNETNAME + 1   ];
      HNDFILE fh;
      DWORD dwFileSize = 0;
      char sz[ 4096 ];
      BOOL fFileOpenError = FALSE;

      /* Want date and time
       */
      Amdate_GetCurrentDate( &date );
      Amdate_GetCurrentTime( &time );

      /* Have subject, will travel.
       */
      lpszSubject = DRF(lpm->recSubject);
      if( NULL == lpszSubject || *lpszSubject == '\0' )
         lpszSubject = "(No Subject)";

      /* Create header. If one was supplied, use that otherwise
       * create our own from lpm.
       */
      if( NULL != lpMailHeader )
         StrBuf_AppendParagraph( &mailbuf, lpMailHeader );
      else
         {
         AM_TIMEZONEINFO tzi;

         /* Create a mail header from the information we
          * have in lpm.
          */
         wsprintf( sz, "Date: %s, %u %s %u %02u:%2.2u ",
                     szDayOfWeek[ date.iDayOfWeek ],
                     date.iDay,
                     szMonth[ date.iMonth - 1 ],
                     date.iYear,
                     time.iHour,
                     time.iMinute );
         Amdate_GetTimeZoneInformation( &tzi );
         wsprintf( sz + strlen( sz ), "+%02u00 (%s)", tzi.diff, tzi.szZoneName );
         StrBuf_AppendLine( &mailbuf, sz );
         wsprintf( sz, "From: %s (%s)", szCIXNickname, szFullName );
         StrBuf_AppendLine( &mailbuf, sz );
         wsprintf( sz, "To: %s", DRF(lpm->recTo) );
         StrBuf_AppendLine( &mailbuf, sz );
         if( *DRF( lpm->recCC ) )
            {
            wsprintf( sz, "CC: %s", DRF(lpm->recCC) );
            StrBuf_AppendLine( &mailbuf, sz );
            }
         else if( *DRF( lpm->recReplyTo ) )
         {
            wsprintf( sz, "CC: %s", DRF( lpm->recReplyTo ) );
            StrBuf_AppendLine( &mailbuf, sz );
         }
         wsprintf( sz, "Subject: %s (0/%u)", lpszSubject, cEncodedParts );
         StrBuf_AppendLine( &mailbuf, sz );
         if( *DRF(lpm->recReply) )
            {
            wsprintf( sz, "In-Reply-To: %s", DRF(lpm->recReply) );
            StrBuf_AppendLine( &mailbuf, sz );
            }
         if( *DRF(lpm->recReplyTo) )
            {
            wsprintf( sz, "Reply-To: %s", DRF(lpm->recReplyTo) );
            StrBuf_AppendLine( &mailbuf, sz );
            }
         CreateMessageId( szMsgId );
         pszMsgId = szMsgId;
         wsprintf( sz, "Message-Id: %s", (LPSTR)szMsgId );
         StrBuf_AppendLine( &mailbuf, sz );
         StrBuf_AppendLine( &mailbuf, "" );
         }

      StripLeadingTrailingChars( pszMsgId, '<' );
      StripLeadingTrailingChars( pszMsgId, '>' );

      /* If this attachment had a cover note, include it in this
       * message.
       */
      StrBuf_AppendParagraph( &mailbuf, DRF(lpm->recText) );

      /* Compute subject field.
       */
      hpAttachments = DRF(lpm->recAttachments);
      while( *hpAttachments && fOk )
      {
         /* Extract one filename.
          */
         hpAttachments = ExtractFilename( lpPathBuf, hpAttachments );
         if( !Amfile_QueryFile( lpPathBuf ) )
            fOk = FALSE;

         if( HNDFILE_ERROR == ( fh = Amfile_Open( lpPathBuf, AOF_SHARE_READ|AOF_SHARE_WRITE ) ) )
         {
            if( ( fh = Amfile_Open( lpPathBuf, AOF_SHARE_READ ) ) == HNDFILE_ERROR )
               fFileOpenError = TRUE;
         }
         if( !fFileOpenError )
            {
            dwFileSize = Amfile_GetFileSize( fh );
            Amfile_Close( fh );
            }

         if( NULL != strchr( lpPathBuf, ' ' ) )
            if (fAttachmentConvertToShortName)
               GetShortPathName( lpPathBuf, lpPathBuf, _MAX_PATH );
            else
               GetURLPathName( lpPathBuf, lpPathBuf, _MAX_PATH );

         if( 1 == cEncodedParts )
            wsprintf( lpTmpBuf, "\r\n[file://%s (%uk) - %s attachment sent in 1 part ]",
                     lpPathBuf, (dwFileSize / 1024 ),
                     ( lpm->wEncodingScheme == ENCSCH_MIME ? "MIME/Base64" : "Uuencoded" ) );
         else
            wsprintf( lpTmpBuf, "\r\n[file://%s (%uk) - %s attachment sent in %u parts ]",
                     lpPathBuf, (dwFileSize / 1024 ),
                     ( lpm->wEncodingScheme == ENCSCH_MIME ? "MIME/Base64" : "Uuencoded" ),
                     cEncodedParts );
         StrBuf_AppendLine( &mailbuf, lpTmpBuf );
      }

      /* Want sensible To and Subject fields.
       */
      lpszTo = MakeCompactString( DRF(lpm->recTo) );
      if( NULL != lpszTo )
         {
         HEADER mailhdr;
         int nMatch;

         /* Create a MAILHEADER structure and fill in the
          * fields necessary for MatchRule to work.
          */
         InitialiseHeader( &mailhdr );
         mailhdr.ptl = ptl;
         strcpy( mailhdr.szAuthor, lpszTo );
         strcpy( mailhdr.szTo, lpszTo );
         if( !*mailhdr.szCC )
            if( *DRF(lpm->recCC) )
               strcpy( mailhdr.szCC, DRF(lpm->recCC) );
            else
               strcpy( mailhdr.szCC, DRF(lpm->recReplyTo) );
         strcpy( mailhdr.szTitle, lpszSubject );

         InitialiseHeader( &temphdr );
         if( !*mailhdr.szFrom && *DRF(lpm->recFrom) )
            strcpy( mailhdr.szFrom, DRF(lpm->recFrom) );

         ParseMailAuthor( mailhdr.szFrom, &temphdr );
         strcpy( szTempAuthor, mailhdr.szAuthor );
         strcpy( mailhdr.szAuthor, temphdr.szAuthor );

         mailhdr.fPriority = TRUE;
         mailhdr.fOutboxMsg = TRUE;

         ptll = mailhdr.ptl;
         nMatch = MatchRule( &ptll, StrBuf_GetBuffer( &mailbuf ), &mailhdr, TRUE );
         if( !( nMatch & FLT_MATCH_COPY ) && ( ptll != mailhdr.ptl ) )
            mailhdr.ptl = ptll;

         strcpy( mailhdr.szAuthor, szTempAuthor);

         /* Compute memo field and insert at the beginning of
          * the message.
          */
         Amdb_GetTopicInfo( mailhdr.ptl, &topicinfo );
         cbMsg = StrBuf_GetBufferSize( &mailbuf );
         wsprintf( lpTmpBuf, "Memo #%lu (%lu)", topicinfo.dwMaxMsg + 1, cbMsg );
         StrBuf_PrependLine( &mailbuf, lpTmpBuf );


         if( !( nMatch & FLT_MATCH_DELETE ) )
            {
            DWORD dwPossibleComment;
            DWORD dwComment;
            int r;

            /* Try to thread this message to the original.
             */
            Amdb_LockTopic( mailhdr.ptl );
            
            dwComment = 0L;
            if( *DRF(lpm->recReply) )
               if( dwPossibleComment = Amdb_MessageIdToComment( mailhdr.ptl, DRF(lpm->recReply) ) )
                  dwComment = dwPossibleComment;

            /* Create the message.
             */
            r = Amdb_CreateMsg( mailhdr.ptl, &pth, (DWORD)-1, dwComment, mailhdr.szTitle,
                            mailhdr.szAuthor, pszMsgId, &date, &time,
                            StrBuf_GetBuffer( &mailbuf ),
                            StrBuf_GetBufferSize( &mailbuf ), 0 );
            if( r == AMDBERR_NOERROR )
            {
               /* Take post storage action based on rules.
                */
               Amdb_MarkMsgPriority( pth, TRUE, TRUE );
               if( nMatch & FLT_MATCH_READ )
                  Amdb_MarkMsgRead( pth, TRUE );
               if( nMatch & FLT_MATCH_IGNORE )
                  Amdb_MarkMsgIgnore( pth, TRUE, TRUE );
               Amdb_SetOutboxMsgBit( pth, TRUE );
               if( nMatch & FLT_MATCH_READLOCK )
                  Amdb_MarkMsgReadLock( pth, TRUE );
               if( nMatch & FLT_MATCH_KEEP )
                  Amdb_MarkMsgKeep( pth, TRUE );
               if( nMatch & FLT_MATCH_MARK )
                  Amdb_MarkMsg( pth, TRUE );
            }
            
            Amuser_CallRegistered( AE_UNREADCHANGED, (LPARAM)mailhdr.ptl, 0L );
            Amdb_InternalUnlockTopic( mailhdr.ptl );
            Amdb_CommitCachedFiles();
            }
         FreeMemory( &lpszTo );
         }
      }
}

/* This function creates a local copy of a mail message
 * containing an attachment.
 */
void FASTCALL CreateLocalCopyOfForwardAttachmentMail( FORWARDMAILOBJECT FAR * lpm, LPSTR lpMailHeader,
   int cEncodedParts, char * pszMsgId )
{
   char szMsgId[ 256 ];
   STRBUF mailbuf;
   HPSTR hpAttachments;
   BOOL fOk = TRUE;
   PTL ptl, ptll;
   PTH pth;
   BOOL fFileOpenError = FALSE;

   StrBuf_Init( &mailbuf );
   ptl = GetPostmasterFolder();
   if( NULL != ptl )
      {
      TOPICINFO topicinfo;
      LPSTR lpszSubject;
      AM_DATE date;
      AM_TIME time;
      LPSTR lpszTo;
      DWORD cbMsg;
      HEADER temphdr;
      char szTempAuthor[ LEN_INTERNETNAME + 1   ];
      HNDFILE fh;
      DWORD dwFileSize = 0;

      /* Want date and time
       */
      Amdate_GetCurrentDate( &date );
      Amdate_GetCurrentTime( &time );

      /* Have subject, will travel.
       */
      lpszSubject = DRF(lpm->recSubject);
      if( NULL == lpszSubject || *lpszSubject == '\0' )
         lpszSubject = "(No Subject)";

      /* Create header. If one was supplied, use that otherwise
       * create our own from lpm.
       */
      if( NULL != lpMailHeader )
         StrBuf_AppendParagraph( &mailbuf, lpMailHeader );
      else
         {
         AM_TIMEZONEINFO tzi;
         char sz[ 4096 ];

         /* Create a mail header from the information we
          * have in lpm.
          */
         wsprintf( sz, "Date: %s, %u %s %u %02u:%2.2u ",
                     szDayOfWeek[ date.iDayOfWeek ],
                     date.iDay,
                     szMonth[ date.iMonth - 1 ],
                     date.iYear,
                     time.iHour,
                     time.iMinute );
         Amdate_GetTimeZoneInformation( &tzi );
         wsprintf( sz + strlen( sz ), "+%02u00 (%s)", tzi.diff, tzi.szZoneName );
         StrBuf_AppendLine( &mailbuf, sz );
         wsprintf( sz, "From: %s (%s)", szCIXNickname, szFullName );
         StrBuf_AppendLine( &mailbuf, sz );
         wsprintf( sz, "To: %s", DRF(lpm->recTo) );
         StrBuf_AppendLine( &mailbuf, sz );
         if( *DRF(lpm->recCC ) )
            {
            wsprintf( sz, "CC: %s", DRF(lpm->recCC) );
            StrBuf_AppendLine( &mailbuf, sz );
            }
         wsprintf( sz, "Subject: %s (0/%u)", lpszSubject, cEncodedParts );
         StrBuf_AppendLine( &mailbuf, sz );
         if( *DRF(lpm->recReplyTo) )
            {
            wsprintf( sz, "Reply-To: %s", DRF(lpm->recReplyTo) );
            StrBuf_AppendLine( &mailbuf, sz );
            }
         CreateMessageId( szMsgId );
         pszMsgId = szMsgId;
         wsprintf( sz, "Message-Id: %s", (LPSTR)szMsgId );
         StrBuf_AppendLine( &mailbuf, sz );
         StrBuf_AppendLine( &mailbuf, "" );
         }

      StripLeadingTrailingChars( pszMsgId, '<' );
      StripLeadingTrailingChars( pszMsgId, '>' );

      /* If this attachment had a cover note, include it in this
       * message.
       */
      StrBuf_AppendParagraph( &mailbuf, DRF(lpm->recText) );

      /* Compute subject field.
       */
      hpAttachments = DRF(lpm->recAttachments);
      while( *hpAttachments && fOk )
      {
         /* Extract one filename.
          */
         hpAttachments = ExtractFilename( lpPathBuf, hpAttachments );
         if( !Amfile_QueryFile( lpPathBuf ) )
            fOk = FALSE;   

         if( HNDFILE_ERROR == ( fh = Amfile_Open( lpPathBuf, AOF_SHARE_READ|AOF_SHARE_WRITE ) ) )
         {
            if( ( fh = Amfile_Open( lpPathBuf, AOF_SHARE_READ ) ) == HNDFILE_ERROR )
               fFileOpenError = TRUE;
         }
         if( !fFileOpenError )
            {
            dwFileSize = Amfile_GetFileSize( fh );
            Amfile_Close( fh );
            }           

         if( NULL != strchr( lpPathBuf, ' ' ) )
            if (fAttachmentConvertToShortName)
               GetShortPathName( lpPathBuf, lpPathBuf, _MAX_PATH );
            else
               GetURLPathName( lpPathBuf, lpPathBuf, _MAX_PATH );

         if( 1 == cEncodedParts )
            wsprintf( lpTmpBuf, "\r\n[file://%s (%uk) - %s attachment sent in 1 part ]",
                     lpPathBuf, (dwFileSize / 1024 ),
                     ( lpm->wEncodingScheme == ENCSCH_MIME ? "MIME/Base64" : "Uuencoded" ) );
         else
            wsprintf( lpTmpBuf, "\r\n[file://%s (%uk) - %s attachment sent in %u parts ]",
                     lpPathBuf, (dwFileSize / 1024 ),
                     ( lpm->wEncodingScheme == ENCSCH_MIME ? "MIME/Base64" : "Uuencoded" ),
                     cEncodedParts );
         StrBuf_AppendLine( &mailbuf, lpTmpBuf );
      }

      /* Want sensible To and Subject fields.
       */
      lpszTo = MakeCompactString( DRF(lpm->recTo) );
      if( NULL != lpszTo )
         {
         HEADER mailhdr;
         int nMatch;

         /* Create a MAILHEADER structure and fill in the
          * fields necessary for MatchRule to work.
          */
         InitialiseHeader( &mailhdr );
         mailhdr.ptl = ptl;
         strcpy( mailhdr.szAuthor, lpszTo );
         strcpy( mailhdr.szTo, lpszTo );
         strcpy( mailhdr.szCC, "" );
         strcpy( mailhdr.szTitle, lpszSubject );

         InitialiseHeader( &temphdr );
         if( !*mailhdr.szFrom && *DRF(lpm->recFrom) )
            strcpy( mailhdr.szFrom, DRF(lpm->recFrom) );

         ParseMailAuthor( mailhdr.szFrom, &temphdr );
         strcpy( szTempAuthor, mailhdr.szAuthor );
         strcpy( mailhdr.szAuthor, temphdr.szAuthor );

         mailhdr.fPriority = TRUE;
         mailhdr.fOutboxMsg = TRUE;

         ptll = mailhdr.ptl;
         nMatch = MatchRule( &ptll, StrBuf_GetBuffer( &mailbuf ), &mailhdr, TRUE );
         if( !( nMatch & FLT_MATCH_COPY ) && ( ptll != mailhdr.ptl ) )
            mailhdr.ptl = ptll;

         strcpy( mailhdr.szAuthor, szTempAuthor);
         /* Compute memo field and insert at the beginning of
          * the message.
          */
         Amdb_GetTopicInfo( mailhdr.ptl, &topicinfo );
         cbMsg = StrBuf_GetBufferSize( &mailbuf );
         wsprintf( lpTmpBuf, "Memo #%lu (%lu)", topicinfo.dwMaxMsg + 1, cbMsg );
         StrBuf_PrependLine( &mailbuf, lpTmpBuf );


         if( !( nMatch & FLT_MATCH_DELETE ) )
            {
            DWORD dwComment;
            int r;

            /* Try to thread this message to the original.
             */
            Amdb_LockTopic( mailhdr.ptl );
            
            dwComment = 0L;

            /* Create the message.
             */
            r = Amdb_CreateMsg( mailhdr.ptl, &pth, (DWORD)-1, dwComment, mailhdr.szTitle,
                            mailhdr.szAuthor, pszMsgId, &date, &time,
                            StrBuf_GetBuffer( &mailbuf ),
                            StrBuf_GetBufferSize( &mailbuf ), 0 );
            if( r == AMDBERR_NOERROR )
            {
               /* Take post storage action based on rules.
                */
               Amdb_MarkMsgPriority( pth, TRUE, TRUE );
               if( nMatch & FLT_MATCH_READ )
                  Amdb_MarkMsgRead( pth, TRUE );
               if( nMatch & FLT_MATCH_IGNORE )
                  Amdb_MarkMsgIgnore( pth, TRUE, TRUE );
               Amdb_SetOutboxMsgBit( pth, TRUE );
               if( nMatch & FLT_MATCH_READLOCK )
                  Amdb_MarkMsgReadLock( pth, TRUE );
               if( nMatch & FLT_MATCH_KEEP )
                  Amdb_MarkMsgKeep( pth, TRUE );
               if( nMatch & FLT_MATCH_MARK )
                  Amdb_MarkMsg( pth, TRUE );
            }
            
            Amuser_CallRegistered( AE_UNREADCHANGED, (LPARAM)mailhdr.ptl, 0L );
            Amdb_InternalUnlockTopic( mailhdr.ptl );
            Amdb_CommitCachedFiles();
            }
         FreeMemory( &lpszTo );
         }
      }
}

/* Scan the list of attachments and if any are greater than 10K,
 * issue a warning message.
 */
BOOL FASTCALL CheckAttachmentSize( HWND hwnd, LPSTR lpAttachments, int wScheme )
{
   while( *lpAttachments )
      {
      HNDFILE fh = NULL;

      lpAttachments = ExtractFilename( lpPathBuf, lpAttachments );
      if( HNDFILE_ERROR == ( fh = Amfile_Open( lpPathBuf, AOF_SHARE_READ|AOF_SHARE_WRITE ) ) )
      {
         if( HNDFILE_ERROR == ( fh = Amfile_Open( lpPathBuf, AOF_SHARE_READ ) )  )
            {
            /* File not found, so offer to cancel
             */
            DWORD ret = GetLastError();
            wsprintf( lpTmpBuf, GS(IDS_STR1200), lpPathBuf );
            fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONINFORMATION );
            return( FALSE );
            }
      }
      if( fh != NULL )
         {
         DWORD dwFileSize;
         DWORD dwEstimatedLines;

         dwFileSize = Amfile_GetFileSize( fh );
         Amfile_Close( fh );
         dwEstimatedLines = dwFileSize / 45;
         if(( fSplitParts ) && ( fAttachmentSizeWarning ))
            {
            if( dwEstimatedLines / dwLinesMax > 5 )
               {
               wsprintf( lpTmpBuf, GS(IDS_STR1199), lpPathBuf, ( dwEstimatedLines / dwLinesMax ), dwLinesMax );
               if( IDNO == fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONINFORMATION ) )
                  return( FALSE );
               }
            }
         else if(( dwEstimatedLines > 500 ) && ( fAttachmentSizeWarning ))
            {
            wsprintf( lpTmpBuf, GS(IDS_STR1201), lpPathBuf, dwEstimatedLines );
            if( IDNO == fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONINFORMATION ) )
               return( FALSE );
            }
         }
      }
   return( TRUE );
}

/* Scan thru each filename and make sure that they
 * all exist.
 */
BOOL FASTCALL CheckValidCixFilenames( HWND hwnd, LPSTR lpFilename )
{
   while( *lpFilename )
      {
      lpFilename = ExtractFilename( lpPathBuf, lpFilename );
      if( !CheckValidCixFilename( hwnd, lpPathBuf ) )
         return( FALSE );
      }
   return( TRUE );
}

/* This function appends a paragraph to the buffer. A paragraph is
 * a series of lines which use the CR/CR/LF convention for delimiting
 * soft line breaks.
 */
BOOL FASTCALL StrBuf_AppendParagraph(STRBUF FAR * lpstrbuf, HPSTR lpStr)
{
   HPSTR lpStrOut = NULL;
   BOOL fResult;
   DWORD lLen;

   lLen = (DWORD)(lstrlen(lpStr) * 1.5);

   if (lLen <= 10)
      lLen = 10;

   INITIALISE_PTR(lpStrOut);
   
   fNewMemory32( &lpStrOut, lLen );
   FormatTextIP( lpStr, &lpStrOut, lLen - 1, imWrapCol > 4093 ? 4094 : imWrapCol );

   fResult = StrBuf_AppendText(lpstrbuf, lpStrOut, FALSE);

   FreeMemory32( &lpStrOut );
   return fResult;
}
