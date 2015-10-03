/* FWMAIL.C - Handles e-mail forwarding
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
#include "shared.h"
#include "cixip.h"
#include <io.h>
#include "ameol2.h"
#include "cix.h"
#include "tti.h"
#include "rules.h"
#include "cookie.h"
#include "listview.h"
#include "editmap.h"

#ifndef USEBIGEDIT
#include "scintilla.h"
#include "scilexer.h"
#endif USEBIGEDIT

#define  THIS_FILE   __FILE__

extern BOOL fWordWrap;                 /* TRUE if we word wrap messages */

#ifdef WIN32
#define  TASKHANDLE  DWORD
#else
#define  TASKHANDLE  HTASK
#endif

LPSTR GetReadableText(PTL ptl, LPSTR pSource); // !!SM!! 2039

char NEAR szFwMailWndClass[] = "amctl_fwmailwndcls";
char NEAR szFwMailWndName[] = "Forward Mail";

static BOOL fDefDlgEx = FALSE;

static BOOL fRegistered = FALSE;          /* TRUE if we've registered the mail window */
static WNDPROC lpEditCtrlProc = NULL;        /* Edit control window procedure address */
static BOOL fNewMsg;                   /* TRUE if we're editing a new message */

typedef struct tagFWMAILWNDINFO {
   LPOB lpob;
   HWND hwndFocus;
   RECPTR recMailbox;
   char szSigFile[ 16 ];
   PTL ptl;
} FWMAILWNDINFO, FAR * LPFWMAILWNDINFO;


#define  GetFwMailWndInfo(h)     (LPFWMAILWNDINFO)GetWindowLong((h),DWL_USER)
#define  SetFwMailWndInfo(h,p)   SetWindowLong((h),DWL_USER,(long)(p))

/* Array of controls that have tooltips and their string
 * resource IDs.
 */
static TOOLTIPITEMS tti[] = {
   IDD_LOWPRIORITY,     IDS_STR997,
   IDD_HIGHPRIORITY,    IDS_STR996,
   IDD_CCTOSENDER,         IDS_STR1072,
   IDD_RETURNRECEIPT,      IDS_STR1203
   };

#define  ctti     (sizeof(tti)/sizeof(TOOLTIPITEMS))

BOOL FASTCALL FwMailWnd_OnCreate( HWND, LPCREATESTRUCT );
void FASTCALL FwMailWnd_OnClose( HWND );
void FASTCALL FwMailWnd_OnSize( HWND, UINT, int, int );
void FASTCALL FwMailWnd_OnMove( HWND, int, int );
void FASTCALL FwMailWnd_MDIActivate( HWND, BOOL, HWND, HWND );
void FASTCALL FwMailWnd_OnCommand( HWND, int, HWND, UINT );
void FASTCALL FwMailWnd_OnSetFocus( HWND, HWND );
void FASTCALL FwMailWnd_OnAdjustWindows( HWND, int, int );
LRESULT FASTCALL FwMailWnd_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr );

LRESULT EXPORT CALLBACK FwMailWndProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK FwMailEditProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK FwMailWnd_GetMsgProc( int, WPARAM, LPARAM ); 

LRESULT EXPORT CALLBACK FwMailEditProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL CreateFwMailWindow( HWND, FORWARDMAILOBJECT FAR *, LPOB );

/* This function forwards a mail message.
 */
void FASTCALL CmdForwardMailMessage( HWND hwnd, PTH pth )
{
   FORWARDMAILOBJECT mo;
   MSGINFO msginfo;
   HPSTR hpText;
   CURMSG curmsg;
   char szTo[ 501 ];
   char szFrom[ 501 ];
   char szCC[ 501 ];
   char szSubject[ 501 ];
   char szDate[ 501 ];

   /* Create a new FORWARDMAIL object.
    */
   Amob_New( OBTYPE_FORWARDMAIL, &mo );
   mo.nPriority = MAIL_PRIORITY_NORMAL;
   mo.wFlags = fCCMailToSender ? 0 : MOF_NOECHO;

   /* Set the From address.
    */
   wsprintf( lpTmpBuf, "%s (%s)", (LPSTR)szMailAddress, (LPSTR)szFullName );
   Amob_CreateRefString( &mo.recFrom, lpTmpBuf );

   if( !fUseInternet || nMailDelivery == MAIL_DELIVER_CIX )
      mo.wEncodingScheme = ENCSCH_UUENCODE;
   else
      mo.wEncodingScheme = wDefEncodingScheme;


   /* Set the mailbox.
    */
   Ameol2_GetCurrentTopic( &curmsg );
   if( NULL != curmsg.ptl )
      {
      char sz[ 128 ];

      WriteFolderPathname( sz, curmsg.ptl );
      Amob_CreateRefString( &mo.recMailbox, sz );
      if( fUseInternet )
         {
            CreateReplyAddress( &mo.recReplyTo, curmsg.ptl );
            CreateMailMailAddress( &mo.recFrom, curmsg.ptl );
         }
      }


   /* Prepend Fw: to the subject title.
    */
   /*
     !!SM!!
    */
   Amdb_GetMsgInfo( pth, &msginfo );
   Amdb_GetMailHeaderField( pth, "Subject", (char *)&szSubject, 500, TRUE );
   if( szSubject[0] )
   {
      b64decodePartial((char *)&szSubject); 
      wsprintf( lpTmpBuf, "Fw: %s", szSubject );
   }
   else
   {
      wsprintf( lpTmpBuf, "Fw: %s", msginfo.szTitle );
   }
   Amob_CreateRefString( &mo.recSubject, lpTmpBuf );

   /* Get the current message and use that as the contents of the
    * forwarded message.
    */
   hpText = Amdb_GetMsgText( pth );
   if( NULL != hpText )
      {
      HPSTR hpNewBody;
      HPSTR hpNewBody2;
      HPSTR hpNewBody3;
      HPSTR hpNewBody4;
      HPSTR hpBody;
      PTL ptl;
      char * pszSig;
      LPSTR lpszSignature;

      INITIALISE_PTR(hpNewBody);

      /* Get the message body. Skip the first line as for news and
       * mail messages it will be a junk line and for CIX or local 
       * messages we will be replacing it with our own line.
       */
      ptl = Amdb_TopicFromMsg( pth );
      hpBody = hpText;
      if( hpBody = (HPSTR)_fstrstr( (LPSTR)hpText, "\r\n" ) )
         hpBody += 2;


      if( Amdb_GetTopicType( ptl ) == FTYPE_CIX )
      {
         AM_DATE date;
         AM_TIME time;
         char szDate[ 40 ];

         Amdate_UnpackDate( msginfo.wDate, &date );
         Amdate_UnpackTime( msginfo.wTime, &time );
         Amdate_FormatShortDate( &date, szDate );
         wsprintf( lpTmpBuf, "\r\n\r\nFwd: cix:%s/%s:%lu %s(%lu) %s %2.2d:%2.2d\r\n---- Forwarded Message ----\r\n", 
            Amdb_GetFolderName( Amdb_FolderFromTopic( ptl ) ), 
            Amdb_GetTopicName( ptl ), 
            msginfo.dwMsg, 
            msginfo.szAuthor, 
            msginfo.dwSize, 
            szDate, time.iHour, time.iMinute );
      }
         
      else
      {

      /* Create a delimiter to separate the body from the text we'll
       * be adding.
       */
      wsprintf( lpTmpBuf, "\r\n\r\n---- Forwarded Message ----\r\n" );
      }

      /* Try to get a sig, then go through hoops to get that sig at the top of
       * the message (before the text).
       */
      pszSig = NULL;
      if( NULL == ptl )
         pszSig = szGlobalSig;
      else
      {
      TOPICINFO topicinfo;

      Amdb_GetTopicInfo( ptl, &topicinfo );
      if( topicinfo.szSigFile[ 0 ] )
         pszSig = topicinfo.szSigFile;
      }
      if( NULL != pszSig )
         {
         if( NULL != ( lpszSignature = GetEditSignature( pszSig ) ) )
         {
            hpNewBody2 = ConcatenateStrings( lpszSignature, lpTmpBuf );

            lpTmpBuf[0] = '\x0';

            if ( fStripHeaders )
            {
               if ( fShortHeaders )
               {
                  if( fShortHeadersTo )
                  {
                     Amdb_GetMailHeaderField( pth, "To", (char *)&szTo, 500, TRUE );
                     if (strlen(szTo) > 0)
                     {
                        strcat(lpTmpBuf, "To: ");
                        strcat(lpTmpBuf, szTo);
                        strcat(lpTmpBuf, "\r\n");
                     }

                  }
                  if( fShortHeadersFrom )
                  {
                     Amdb_GetMailHeaderField( pth, "From", (char *)&szFrom, 500, TRUE );
                     if (strlen(szFrom) > 0)
                     {
                        strcat(lpTmpBuf, "From: ");
                        strcat(lpTmpBuf, szFrom);
                        strcat(lpTmpBuf, "\r\n");
                     }
                  }
                  if( fShortHeadersCC )
                  {
                     Amdb_GetMailHeaderField( pth, "CC", (char *)&szCC, 500, TRUE );
                     if (strlen(szCC) > 0)
                     {
                        strcat(lpTmpBuf, "CC: ");
                        strcat(lpTmpBuf, szCC);
                        strcat(lpTmpBuf, "\r\n");
                     }
                  }
                  if( fShortHeadersSubject )
                  {
                     Amdb_GetMailHeaderField( pth, "Subject", (char *)&szSubject, 500, TRUE );
                     if (strlen(szSubject) > 0)
                     {
                        strcat(lpTmpBuf, "Subject: ");
                        strcat(lpTmpBuf, szSubject);
                        strcat(lpTmpBuf, "\r\n");
                     }
                  }
                  if( fShortHeadersDate )
                  {
                     Amdb_GetMailHeaderField( pth, "Date", (char *)&szDate, 500, TRUE );
                     if (strlen(szSubject) > 0)
                     {
                        strcat(lpTmpBuf, "Date: ");
                        strcat(lpTmpBuf, szDate);
                        strcat(lpTmpBuf, "\r\n");
                     }
                  }
                  strcat(lpTmpBuf, "\r\n");
               }
            }

            if( fStripHeaders && ( Amdb_GetTopicType( Amdb_TopicFromMsg( pth ) ) == FTYPE_MAIL || Amdb_GetTopicType( Amdb_TopicFromMsg( pth ) ) == FTYPE_NEWS ) )
               hpBody = GetReadableText(Amdb_TopicFromMsg( pth ), hpText ); // !!SM!! 2039
            else
               hpBody = GetTextBody( Amdb_TopicFromMsg( pth ), hpText );

            if (strlen(lpTmpBuf) > 0)
            {
               hpNewBody4 = ConcatenateStrings( hpNewBody2, lpTmpBuf );
               hpNewBody3 = ConcatenateStrings( hpNewBody4, hpBody );
            }
            else
            {
               hpNewBody3 = ConcatenateStrings( hpNewBody2, hpBody );
            }
         }
         else
            hpNewBody3 = ConcatenateStrings( lpTmpBuf, hpBody );
         }
      else
         hpNewBody3 = ConcatenateStrings( lpTmpBuf, hpBody );
      wsprintf( lpTmpBuf, "\r\n\r\n---- End Forwarded Message ----\r\n\r\n" );
      hpNewBody = ConcatenateStrings( hpNewBody3, lpTmpBuf );



      /* Allocate memory for the combined string.
       */
      if( NULL != hpNewBody )
         {
         Amob_CreateRefString( &mo.recText, hpNewBody );
         FreeMemory32( &hpNewBody );
         }
      Amdb_FreeMsgTextBuffer( hpText );
      CreateFwMailWindow( hwndFrame, &mo, NULL );
      }

   /* Clean up before mother gets home.
    */
   Amob_Delete( OBTYPE_FORWARDMAIL, &mo );
}

/* This function creates the Forward Mail window.
 */
BOOL FASTCALL CreateFwMailWindow( HWND hwnd, FORWARDMAILOBJECT FAR * lpfmoInit, LPOB lpob )
{
   LPFWMAILWNDINFO lpmwi;
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
         return( TRUE );
         }

   /* Register the group list window class if we have
    * not already done so.
    */
   if( !fRegistered )
      {
      WNDCLASS wc;

      wc.style          = CS_HREDRAW | CS_VREDRAW;
      wc.lpfnWndProc    = FwMailWndProc;
      wc.hIcon          = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_MAIL) );
      wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
      wc.lpszMenuName   = NULL;
      wc.cbWndExtra     = DLGWINDOWEXTRA;
      wc.cbClsExtra     = 0;
      wc.hbrBackground  = (HBRUSH)( COLOR_APPWORKSPACE + 1 );
      wc.lpszClassName  = szFwMailWndClass;
      wc.hInstance      = hInst;
      if( !RegisterClass( &wc ) )
         return( FALSE );
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
   ReadProperWindowState( szFwMailWndName, &rc, &dwState );

   /* If lpob is NULL, this is a new msg.
    */
   fNewMsg = lpob == NULL;

   /* Create the window.
    */
   hwndMail = Adm_CreateMDIWindow( szFwMailWndName, szFwMailWndClass, hInst, &rc, dwState, (LPARAM)lpfmoInit );
   if( NULL == hwndMail )
      return( FALSE );

   /* Rename the Send button to Save if we're offline.
    */
   if( !fOnline )
      SetWindowText( GetDlgItem( hwndMail, IDOK ), "&Save" );

   /* Store the handle of the destination structure.
    */
   lpmwi = GetFwMailWndInfo( hwndMail );
   lpmwi->lpob = lpob;
   lpmwi->recMailbox.hpStr = NULL;
   Amob_CopyRefObject( &lpmwi->recMailbox, &lpfmoInit->recMailbox );

   /* Determine where we put the focus.
    */
   if( '\0' == *DRF(lpfmoInit->recTo) )
      hwndFocus = GetDlgItem( hwndMail, IDD_TO );
   else if( '\0' == *DRF(lpfmoInit->recSubject) )
      hwndFocus = GetDlgItem( hwndMail, IDD_SUBJECT );
   else
      hwndFocus = GetDlgItem( hwndMail, IDD_EDIT );
   lpmwi->hwndFocus = hwndFocus;
   SetFocus( hwndFocus );
   if( NULL != lpob )
      Amob_SetEditWindow( lpob, hwndMail );
   return( TRUE );
}

/* This is the main window procedure.
 */
LRESULT EXPORT CALLBACK FwMailWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, FwMailWnd_OnCreate );
      HANDLE_MSG( hwnd, WM_CLOSE, FwMailWnd_OnClose );
      HANDLE_MSG( hwnd, WM_SIZE, FwMailWnd_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, FwMailWnd_OnMove );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, Common_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, FwMailWnd_MDIActivate );
      HANDLE_MSG( hwnd, WM_COMMAND, FwMailWnd_OnCommand );
      HANDLE_MSG( hwnd, WM_MENUSELECT, MainWnd_OnMenuSelect );
      HANDLE_MSG( hwnd, WM_SETFOCUS, FwMailWnd_OnSetFocus );
      HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, FwMailWnd_OnAdjustWindows );
      HANDLE_MSG( hwnd, WM_CHANGEFONT, Common_ChangeFont );
      HANDLE_MSG( hwnd, WM_NOTIFY, FwMailWnd_OnNotify );

      case WM_APPCOLOURCHANGE:
      case WM_TOGGLEHEADERS: {  // 2.25.2031
#ifdef USEBIGEDIT
         SetWindowFont( GetDlgItem( hwnd, IDD_EDIT ), hEditFont, FALSE );
#else USEBIGEDIT
         SetEditorDefaults(GetDlgItem( hwnd, IDD_EDIT ), FALSE, lfEditFont, fWordWrap, fHotLinks , fMsgStyles, crEditText, crEditWnd );
#endif USEBIGEDIT
         break;
         }
/* Crashes under Win9x
 */
/*
   case WM_DROPFILES: {
         HDROP hDrop = (HDROP)wParam;
         HNDFILE fh;
         char sz[ _MAX_PATH ];
         LPSTR lpText;
         LONG lSize;
         UINT iNumFiles = 0;
         UINT count = 0;
         BOOL fBigEdit;
         BOOL fEdit;
         BOOL fFileOpenError = FALSE;
         char szClassName[ 40 ];
         HWND hwndControl;

         hwndControl = (HWND)lParam;
         GetClassName( hwndControl, szClassName, sizeof( szClassName ) );
         fEdit = _strcmpi( szClassName, "edit" ) == 0;
         fBigEdit = fEdit && ( GetWindowStyle( hwndControl ) & ES_MULTILINE );

         sz[ 0 ] = '\0';
         iNumFiles = DragQueryFile( hDrop, -1, sz, _MAX_PATH );
         if( fBigEdit )
         {
         SetFocus( hwndControl );
         for( count = 0; count < iNumFiles; count++ )
         {
         DragQueryFile( hDrop, count, sz, _MAX_PATH );
         INITIALISE_PTR(lpText);
         if( ( fh = Amfile_Open( sz, AOF_READ ) ) == HNDFILE_ERROR )
            FileOpenError( hwndControl, sz, FALSE, FALSE );
         else
            {
            lSize = Amfile_GetFileSize( fh );
            if( 0L == lSize )
                  {
                  wsprintf( lpTmpBuf, GS(IDS_STR83), (LPSTR)sz );
                  fMessageBox( hwndControl, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
                  }
               else if( lSize > 65535L )
                  fMessageBox( hwndControl, 0, GS(IDS_STR77), MB_OK|MB_ICONEXCLAMATION);
               else if( !fNewMemory32( &lpText, (WORD)lSize + 1 ) )
                  {
                  wsprintf( lpTmpBuf, GS(IDS_STR80), (LPSTR)sz );
                  fMessageBox( hwndControl, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
                  }
               else
                  {
                  if( Amfile_Read( fh, lpText, (int)lSize ) != (UINT)lSize )
                     DiskReadError( hwndControl, sz, FALSE, FALSE );
                  else
                     {
                     DWORD dwSel;

                     if( lpText[ (int)lSize - 1 ] == 26 )
                        --lSize;
                     lpText[ (int)lSize ] = '\0';

                     StripBinaryCharacter( lpText, (int)lSize, '\0' );

                     lpText = ExpandUnixTextToPCFormat( lpText );

                     SetWindowRedraw( hwndControl, FALSE );
                     dwSel = Edit_GetSel( hwndControl );
                     Edit_ReplaceSel( hwndControl, lpText );
                     Edit_SetSel( hwndControl, LOWORD( dwSel ), HIWORD( dwSel ) );
                     Edit_ScrollCaret( hwndControl );
                     SetWindowRedraw( hwndControl, TRUE );
                     }
                  FreeMemory32( &lpText );
                  }
               Amfile_Close( fh );
               }
            }
         }
         else
         {
         strcpy( lpTmpBuf, "\0");
         for( count = 0; count < iNumFiles; count++ )
         {
            DragQueryFile( hDrop, count, sz, _MAX_PATH );
            if( HNDFILE_ERROR == ( fh = Amfile_Open( sz, AOF_SHARE_READ|AOF_SHARE_WRITE ) ) )
            {
               if( ( fh = Amfile_Open( sz, AOF_SHARE_READ ) ) == HNDFILE_ERROR )
               {
                  FileOpenError( hwndControl, sz, FALSE, FALSE );
                  fFileOpenError = TRUE;
               }
            }
            if( !fFileOpenError )
               {
               *lpTmpBuf ? strcat( lpTmpBuf, "<") : strcpy( lpTmpBuf, "<" );
               strcat( lpTmpBuf, sz );
               strcat( lpTmpBuf, ">" );
               Amfile_Close( fh );
               }
         }
         if( !fFileOpenError )
         {
            char sz2[ 4096 ];
            int oldlength = 0;
            int length = 0;
            HWND hwndAttachField;

            sz2[ 0 ] = '\0';
            hwndAttachField = GetDlgItem( hwnd, IDD_ATTACHMENT );
            oldlength = Edit_GetTextLength( hwndAttachField );
            if( oldlength > 0 )
            {
               Edit_GetText( hwndAttachField, sz2, oldlength + 1 );
               strcat( lpTmpBuf, "<" );
               strcat( lpTmpBuf, sz2 );
               strcat( lpTmpBuf, ">" );
            }
            Edit_SetText( hwndAttachField, lpTmpBuf );
         }
      }
         DragFinish( hDrop );
         break;
         }
*/
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
         lpMinMaxInfo->ptMinTrackSize.x = 290;
         lpMinMaxInfo->ptMinTrackSize.y = 270;
         return( lResult );
         }

      default:
         break;
      }
   return( Adm_DefMDIDlgProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_SETFOCUS message.
 */
void FASTCALL FwMailWnd_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   LPFWMAILWNDINFO lpmwi;
   HWND hwndFocus;

   lpmwi = GetFwMailWndInfo( hwnd );
   hwndFocus = lpmwi->hwndFocus;
   if( hwndFocus )
      SetFocus( hwndFocus );
   iActiveMode = WIN_EDITS;
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL FwMailWnd_OnCreate( HWND hwnd, LPCREATESTRUCT lpCreateStruct )
{
   LPMDICREATESTRUCT lpMDICreateStruct;
   LPFWMAILWNDINFO lpmwi;
   LPCSTR lpszDlg;
   char szReplyTo[ 64 ];
   FORWARDMAILOBJECT FAR * lpm;
   HWND hwndEdit;
   HWND hwndList;
   HWND hwndCombo;
   PTL ptl;
   char szThisMailAddress[ 64 ];
   char szThisMailName[ 64 ];
   CURMSG curmsg;

   INITIALISE_PTR(lpmwi);

   /* Create the dialog window.
    */
   lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
   lpm = (FORWARDMAILOBJECT FAR *)lpMDICreateStruct->lParam;
   lpszDlg =  MAKEINTRESOURCE(IDDLG_FORWARDMAILEDITOR_IP);
   Adm_MDIDialog( hwnd, lpszDlg, lpMDICreateStruct );

   /* Create a structure to store the forward mail window info.
    */
   if( !fNewMemory( &lpmwi, sizeof(FWMAILWNDINFO) ) )
      return( FALSE );
   SetFwMailWndInfo( hwnd, lpmwi );

   /* Set the edit window font.
    */
#ifdef USEBIGEDIT
   SetWindowFont( GetDlgItem( hwnd, IDD_EDIT ), hEditFont, FALSE );
#else USEBIGEDIT
   SetEditorDefaults(GetDlgItem( hwnd, IDD_EDIT ), FALSE, lfEditFont, fWordWrap, fHotLinks , fMsgStyles, crEditText, crEditWnd );
#endif USEBIGEDIT

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
   Edit_SetText( GetDlgItem( hwnd, IDD_TO ), DRF(lpm->recTo) );
   Edit_LimitText( GetDlgItem( hwnd, IDD_TO ), LEN_TOCCLIST - 5 );

   Edit_SetText( GetDlgItem( hwnd, IDD_SUBJECT ), DRF(lpm->recSubject) );
   Edit_LimitText( GetDlgItem( hwnd, IDD_SUBJECT ), iMaxSubjectSize );
   Edit_SetText( GetDlgItem( hwnd, IDD_CC ), DRF(lpm->recCC) );
   Edit_LimitText( GetDlgItem( hwnd, IDD_CC ), LEN_TOCCLIST - 5 );
   Edit_SetText( GetDlgItem( hwnd, IDD_BCC ), DRF(lpm->recBCC) );
   Edit_LimitText( GetDlgItem( hwnd, IDD_BCC ), LEN_TOCCLIST - 6 );

   if( fUseInternet && ( nMailDelivery == MAIL_DELIVER_IP ) )
   {
      Edit_SetText( GetDlgItem( hwnd, IDD_ATTACHMENT ), DRF(lpm->recAttachments) );
      if( !*DRF(lpm->recAttachments ) )
      {
         Ameol2_GetCurrentMsg( &curmsg );
         if( NULL != curmsg.pth )
            if( Amdb_IsMsgHasAttachments( curmsg.pth ) )
            {
               HWND hwndAttach;
               register int c;
               int cItems;

               /* Loop for all selected items and get them.
                */
               VERIFY( hwndAttach = GetDlgItem( hwndMsg, IDD_ATTACHMENT ) );

               cItems = (int)SendMessage( hwndAttach, LVM_GETCOUNT, 0, 0L );
               strcpy( lpTmpBuf, "<");
               for( c = 0; c < cItems; ++c )
               {           
                     LVIEW_ITEM lvi;
                     ATTACHMENT at;

                     SendMessage( hwndAttach, LVM_GETICONITEM, c, (LPARAM)(LPSTR)&lvi );
                     Amdb_GetMsgAttachment( NULL, (PAH)lvi.dwData, &at );
                     strcat( lpTmpBuf, at.szPath );
                     if( ( c + 1 ) < cItems )
                        strcat( lpTmpBuf, "><");
               }
               strcat( lpTmpBuf, ">");
               if( *lpTmpBuf )
               {
                  int length = 0;
                  length = lstrlen( lpTmpBuf );
                  if( Amob_CreateRefObject( &lpm->recAttachments, length + 1 ) )
                  {
                     strcpy( lpm->recAttachments.hpStr, lpTmpBuf );
                     Amob_SaveRefObject( &lpm->recAttachments );
                     Edit_SetText( GetDlgItem( hwnd, IDD_ATTACHMENT ), DRF(lpm->recAttachments) );
                  }
               }
            }
      }
   }
   else
   {
      ShowWindow( GetDlgItem( hwnd, IDD_ATTACHMENT),  FALSE );
      ShowWindow( GetDlgItem( hwnd, IDD_ATTACHMENTSBTN),  FALSE );
   }

   if( fUseInternet && ( nMailDelivery != MAIL_DELIVER_CIX ) )
   {
      if( !*DRF(lpm->recFrom ) )
      {
         *szThisMailAddress = '\0';
         *szThisMailName = '\0';
         if( NULL != ptl )
         {
            Amdb_GetCookie( ptl, MAIL_ADDRESS_COOKIE, szThisMailAddress, "", sizeof(szThisMailAddress) );
            Amdb_GetCookie( ptl, MAIL_NAME_COOKIE, szThisMailName, "", sizeof(szThisMailName) );
         }
         if( *szThisMailAddress && *szThisMailName )
            wsprintf( lpTmpBuf, "Forward Mail Message from %s (%s)", szThisMailAddress, szThisMailName );
         else
            wsprintf( lpTmpBuf, "Forward Mail Message from %s (%s)", szMailAddress, szFullName );
      }
      else
      {
         ParseFromField( DRF(lpm->recFrom), szThisMailAddress, szThisMailName );
         wsprintf( lpTmpBuf, "Forward Mail Message from %s (%s)", szThisMailAddress, szThisMailName );
      }
      SetWindowText( hwnd, lpTmpBuf);
   }

   /* Set the priority fields.
    */
   if( lpm->nPriority == MAIL_PRIORITY_HIGHEST )
      CheckDlgButton( hwnd, IDD_HIGHPRIORITY, TRUE );
   if( lpm->nPriority == MAIL_PRIORITY_LOWEST )
      CheckDlgButton( hwnd, IDD_LOWPRIORITY, TRUE );

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
   lpEditCtrlProc = SubclassWindow( GetDlgItem( hwnd, IDD_EDIT ), FwMailEditProc );

   /* Setup the WordWrap function !!SM!!
   */
#ifdef SMWORDBREAK
   SendDlgItemMessage(hwnd, IDD_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)(EDITWORDBREAKPROC)WordBreakProc) ;
#endif SMWORDBREAK

   /* Set the CC option.
    */
   if( !( lpm->wFlags & MOF_NOECHO ) )
      CheckDlgButton( hwnd, IDD_CCTOSENDER, TRUE );
   if( lpm->wFlags & MOF_RETURN_RECEIPT )
      CheckDlgButton( hwnd, IDD_RETURNRECEIPT, TRUE );

   /* Set the picture button bitmaps.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_LOWPRIORITY ), hInst, IDB_LOWPRIORITY );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_HIGHPRIORITY ), hInst, IDB_HIGHPRIORITY );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_CCTOSENDER ), hInst, IDB_CCTOSENDER );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_RETURNRECEIPT ), hInst, IDB_RETURNRECEIPT );

   /* Fill the forward mail edit control.
    */
   SetEditText( hwnd, ptl, IDD_EDIT, DRF( lpm->recText ), FALSE );
#ifndef USEBIGEDIT
   SendMessage( GetDlgItem( hwnd, IDD_EDIT ), SCI_SETSAVEPOINT,  (WPARAM)0, (LPARAM)0L ); 
#endif USEBIGEDIT
   Amob_FreeRefObject( &lpm->recText );

   /* Add them tooltips.
    */
   AddTooltipsToWindow( hwnd, ctti, (LPTOOLTIPITEMS)&tti );
/* Crashes under Win9x
 */
/*
   if( fWinNT )
   {
   DragAcceptFiles( ( GetDlgItem( hwnd, IDD_EDIT ) ), TRUE );
   DragAcceptFiles( ( GetDlgItem( hwnd, IDD_ATTACHMENT ) ), TRUE );
   }
*/ return( TRUE );
}

/* This function handles indents in the edit window.
 */
LRESULT EXPORT CALLBACK FwMailEditProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   return( EditCtrlIndent( lpEditCtrlProc, hwnd, msg, wParam, lParam ) );
}

LRESULT FASTCALL FwMailWnd_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
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
void FASTCALL FwMailWnd_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsFORWARDMAIL );
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

      case IDM_SPELLCHECK:
         /* Spell check the body of the text.
          */
         Ameol2_SpellCheckDocument( hwnd, GetDlgItem( hwnd, IDD_EDIT ), SC_FL_DIALOG );
         break;

      case IDD_PAD8:
      case IDD_PAD4:
      case IDD_PAD1:
         CommonPickerCode( hwnd, IDD_TO, IDD_CC, IDD_BCC, id == IDD_PAD4, id == IDD_PAD8 );
         break;

      case IDD_LIST:
         if( codeNotify == CBN_SELCHANGE )
            {
            LPFWMAILWNDINFO lpmwi;

            lpmwi = GetFwMailWndInfo( hwnd );
            CommonSigChangeCode( hwnd, lpmwi->szSigFile, FALSE, TRUE );
            }
         break;

      case IDD_REPLYTO:
         if( codeNotify == CBN_EDITCHANGE || codeNotify == CBN_SELCHANGE )
            PostCommand( hwnd, IDD_TO, EN_CHANGE );
         break;

      case IDD_TO:
         /* The To and Reply To fields are mandatory. If they are blank, don't let the user
          * finish the message.
          */
         if( codeNotify == EN_CHANGE )
            {
            int cbTo;
            int cbReplyTo;

            cbTo = Edit_GetTextLength( GetDlgItem( hwnd, IDD_TO ) );
            cbReplyTo = fUseInternet ? Edit_GetTextLength( GetDlgItem( hwnd, IDD_REPLYTO ) ) : 1;
            
            EnableControl( hwnd, IDOK, CheckValidMailAddress(GetDlgItem( hwnd, IDD_TO )) && cbReplyTo > 0 ); /*!!SM!!*/
//          EnableControl( hwnd, IDOK, cbTo > 0 && cbReplyTo > 0 );
            }

      case IDD_ATTACHMENT:
         if( codeNotify == EN_CHANGE && id == IDD_ATTACHMENT )
            ShowWindow( GetDlgItem( hwnd, IDD_ENCODING ), Edit_GetTextLength( hwndCtl ) > 0 );

      case IDD_SUBJECT:
      case IDD_EDIT:
         if( codeNotify == EN_SETFOCUS )
            {
            LPFWMAILWNDINFO lpmwi;

            lpmwi = GetFwMailWndInfo( hwnd );
            lpmwi->hwndFocus = hwndCtl;
            }
         break;

      case IDOK: {
         LPFWMAILWNDINFO lpmwi;
         FORWARDMAILOBJECT mo;
         FORWARDMAILOBJECT FAR * lpfmo;
         HWND hwndEdit;
         int length;
#ifndef USEBIGEDIT
         HEDIT hedit;
#endif USEBIGEDIT

         if (!CheckValidMailAddress(GetDlgItem( hwnd, IDD_TO )))                /*!!SM!!*/
         {
            fMessageBox( hwnd, 0, GS(IDS_STRING8508), MB_OK|MB_ICONEXCLAMATION );
            SetFocus( GetDlgItem( hwnd, IDD_TO ) );
            break;
         }

         if (!CheckForAttachment(GetDlgItem( hwnd, IDD_EDIT ), GetDlgItem( hwnd, IDD_ATTACHMENT ))) /*!!SM!!*/
            break;

         /* First spell check the document.
          */
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_EDIT ) );
         Edit_SetSel( hwndEdit, 0, 0 );
         if( fAutoSpellCheck && EditMap_GetModified( hwndEdit ) )
            {
            if( Ameol2_SpellCheckDocument( hwnd, hwndEdit, 0 ) == SP_CANCEL )
               break;
            }

         /* Dereference the FORWARDMAILOBJECT data structure.
          */
         lpmwi = GetFwMailWndInfo( hwnd );
         if( NULL == lpmwi->lpob )
            {
            Amob_New( OBTYPE_FORWARDMAIL, &mo );
            lpfmo = &mo;
            }
         else
            {
            OBINFO obinfo;

            Amob_GetObInfo( lpmwi->lpob, &obinfo );
            lpfmo = obinfo.lpObData;
            }

         if( fUseInternet )
            {
            char szReplyTo[ 64 ];

            VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_REPLYTO ) );
            ComboBox_GetText( hwndEdit, szReplyTo, 64 );
            Amob_CreateRefString( &lpfmo->recReplyTo, szReplyTo );
            Amob_SaveRefObject( &lpfmo->recReplyTo );

            /* Save Reply-To address in cookie if it differs from the
             * current mail reply address.
             */
            if( NULL != lpmwi->ptl )
               {
               int index;
               int c;

               if( IsDlgButtonChecked( hwnd, IDC_MAKEDEFAULT ) ) /*!!SM!!*/
               {
                  Amdb_SetCookie( lpmwi->ptl, MAIL_REPLY_TO_COOKIE_1, DRF(lpfmo->recReplyTo) );
                  c = 2;
               }
               else
                  c = 1;

               for( index = 0, c = 2; index < 6; ++index )
                  {
                  char szCookie[ 64 ];

                  wsprintf( szCookie, "ReplyAddress%u", c );
                  if( CB_ERR == ComboBox_GetLBText( hwndEdit, index, szReplyTo ) )
                     break;
                  if( strcmp( szReplyTo, DRF(lpfmo->recReplyTo ) ) != 0 )
                     {
                     Amdb_SetCookie( lpmwi->ptl, szCookie, szReplyTo );
                     ++c;
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
            Amob_CreateRefString( &lpfmo->recFrom, lpTmpBuf );
            Amob_SaveRefObject( &lpfmo->recFrom );
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
         if( Amob_CreateRefObject( &lpfmo->recTo, length + 1 ) )
            {
            Edit_GetText( hwndEdit, lpfmo->recTo.hpStr, length + 1 );
            CleanAddresses(&lpfmo->recTo, length);
            Amob_SaveRefObject( &lpfmo->recTo );
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
         if( Amob_CreateRefObject( &lpfmo->recCC, length + 1 ) )
            {
            Edit_GetText( hwndEdit, lpfmo->recCC.hpStr, length + 1 );
            CleanAddresses(&lpfmo->recCC, length);
            Amob_SaveRefObject( &lpfmo->recCC );
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
         if( Amob_CreateRefObject( &lpfmo->recBCC, length + 1 ) )
            {
            Edit_GetText( hwndEdit, lpfmo->recBCC.hpStr, length + 1 );
            CleanAddresses(&lpfmo->recBCC, length);
            Amob_SaveRefObject( &lpfmo->recBCC );
            }

         /* Get and update the Subject field.
          */
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_SUBJECT ) );
         length = Edit_GetTextLength( hwndEdit );
         if( Amob_CreateRefObject( &lpfmo->recSubject, length + 1 ) )
            {
            Edit_GetText( hwndEdit, lpfmo->recSubject.hpStr, length + 1 );
            Amob_SaveRefObject( &lpfmo->recSubject );
            }

         /* Set any flags.
          */
         if( !IsDlgButtonChecked( hwnd, IDD_CCTOSENDER ) )
            lpfmo->wFlags |= MOF_NOECHO;
         else
            lpfmo->wFlags &= ~MOF_NOECHO;
         if( IsDlgButtonChecked( hwnd, IDD_RETURNRECEIPT ) )
            lpfmo->wFlags |= MOF_RETURN_RECEIPT;
         else
            lpfmo->wFlags &= ~MOF_RETURN_RECEIPT;

         /* Get and update the Attachment field.
          */
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_ATTACHMENT ) );
         length = Edit_GetTextLength( hwndEdit );
         if( Amob_CreateRefObject( &lpfmo->recAttachments, length + 1 ) )
            {
            Edit_GetText( hwndEdit, lpfmo->recAttachments.hpStr, length + 1 );
            if( !VerifyFilenamesExist( hwnd, DRF(lpfmo->recAttachments) ) )
               {
               HighlightField( hwnd, IDD_ATTACHMENT );
               break;
               }
            Amob_SaveRefObject( &lpfmo->recAttachments );
            }

         /* And the encoding scheme if an attachment was specified.
          */
         if( !IsWindowVisible( GetDlgItem( hwnd, IDD_ENCODING ) ) )
            lpfmo->wEncodingScheme = ENCSCH_NONE;
         else
            {
            lpfmo->wEncodingScheme = GetEncodingScheme( hwnd, IDD_ENCODING );

            /* If MIME chosen and the address is a CIX conferencing address AND
             * the delivery scheme is not MAIL_DELIVER_IP, then ensure that we
             * deliver thru the ISP.
             */
            if( lpfmo->wEncodingScheme == ENCSCH_MIME )
               if( !fUseInternet || nMailDelivery == MAIL_DELIVER_CIX )
                  {
                  fMessageBox( hwnd, 0, GS(IDS_STR973), MB_OK|MB_ICONINFORMATION );
                  break;
                  }

            /* Prohibit sending binary to an internet address.
             */
            if( !IsCixAddress( DRF(lpfmo->recTo) ) && lpfmo->wEncodingScheme == ENCSCH_BINARY )
               {
               fMessageBox( hwnd, 0, GS(IDS_STR972), MB_OK|MB_ICONINFORMATION );
               break;
               }

            /* For binary files, check every filename for CIX compliance.
             */
            if( lpfmo->wEncodingScheme == ENCSCH_BINARY )
               if( !CheckValidCixFilenames( hwnd, DRF(lpfmo->recAttachments) ) )
                  {
                  HighlightField( hwnd, IDD_ATTACHMENT );
                  break;
                  }
            }

         /* Warn if encoded attachment is over 10K
          */
         if( *DRF(lpfmo->recAttachments) && lpfmo->wEncodingScheme != ENCSCH_BINARY )
            if( !CheckAttachmentSize( hwnd, DRF(lpfmo->recAttachments), lpfmo->wEncodingScheme ) )
               break;

         /* Get the priority level.
          */
         if( IsDlgButtonChecked( hwnd, IDD_HIGHPRIORITY ) )
            lpfmo->nPriority = MAIL_PRIORITY_HIGHEST;
         if( IsDlgButtonChecked( hwnd, IDD_LOWPRIORITY ) )
            lpfmo->nPriority = MAIL_PRIORITY_LOWEST;

         /* Set the mailbox.
          */
         Amob_CopyRefObject( &lpfmo->recMailbox, &lpmwi->recMailbox );
         Amob_FreeRefObject( &lpmwi->recMailbox );

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
         if( Amob_CreateRefObject( &lpfmo->recText, length + 1 ) )
            {
            Edit_GetText( hwndEdit, lpfmo->recText.hpStr, length + 1 );
            Amob_SaveRefObject( &lpfmo->recText );
            }
         Amob_Commit( lpmwi->lpob, OBTYPE_FORWARDMAIL, lpfmo );
         RemoveTooltipsFromWindow( hwnd );
         Adm_DestroyMDIWindow( hwnd );
         break;
         }

      case IDCANCEL:
         /* Destroy the window.
          */
         FwMailWnd_OnClose( hwnd );
         break;
      }
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL FwMailWnd_OnAdjustWindows( HWND hwnd, int dx, int dy )
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
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_REPLYTO ), dx, 0 );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_EDIT ), dx, dy );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_ATTACHMENT ), dx, 0 );
   SetEditWindowWidth( GetDlgItem( hwnd, IDD_EDIT ), iWrapCol );
}

/* This function handles the WM_CLOSE message.
 */
void FASTCALL FwMailWnd_OnClose( HWND hwnd )
{
   LPFWMAILWNDINFO lpmwi;
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
   lpmwi = GetFwMailWndInfo( hwnd );
   if( NULL != lpmwi->lpob )
      Amob_Uncommit( lpmwi->lpob );

   RemoveTooltipsFromWindow( hwnd );
   Adm_DestroyMDIWindow( hwnd );
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL FwMailWnd_OnMove( HWND hwnd, int x, int y )
{
   Amuser_WriteWindowState( szFwMailWndName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL FwMailWnd_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   /* Save the new window state to the configuration file.
    */
   Amuser_WriteWindowState( szFwMailWndName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* This function handles the WM_MDIACTIVATE message.
 */
void FASTCALL FwMailWnd_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   if( fActive )
      ToolbarState_CopyPaste();
   Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_EDITS ), (LPARAM)(LPSTR)hwnd );
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, Adm_DefMDIDlgProc );
}

/* This is the forward mail message out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_ForwardMail( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   FORWARDMAILOBJECT FAR * lpfmo;

   lpfmo = (FORWARDMAILOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_GETTYPE: {
         DWORD dwType;
         
         dwType = 0;
         if( ( nMailDelivery == MAIL_DELIVER_IP ) && ( lpfmo->wEncodingScheme != ENCSCH_BINARY ) )
            dwType = BF_POSTIPMAIL;
         else if( ( nMailDelivery == MAIL_DELIVER_IP ) && ( lpfmo->wEncodingScheme == ENCSCH_BINARY ) )
            dwType = BF_POSTCIXMAIL;
         else if( nMailDelivery == MAIL_DELIVER_CIX )
            dwType = BF_POSTCIXMAIL;
         else if( nMailDelivery == MAIL_DELIVER_AI )
            {
            if( IsCixAddress( DRF( lpfmo->recTo ) ) && lpfmo->wEncodingScheme != ENCSCH_MIME )
               dwType = BF_POSTCIXMAIL;
            else
            dwType = BF_POSTIPMAIL;
            }
         ASSERT( dwType != 0 );
         return( (LRESULT) dwType );
         }

      case OBEVENT_GETCLSID:
         if( ( nMailDelivery == MAIL_DELIVER_AI ) && IsCixAddress( DRF(lpfmo->recTo) ) )
            return( lpfmo->wEncodingScheme == ENCSCH_BINARY ? Amob_GetNextObjectID() : 2 );
         if( nMailDelivery == MAIL_DELIVER_CIX )
            return( lpfmo->wEncodingScheme == ENCSCH_BINARY ? Amob_GetNextObjectID() : 2 );
         return( Amob_GetNextObjectID() );

      case OBEVENT_EXEC:
         /* If AI delivery and any attachment is being sent via MIME, then do NOT
          * send via CIX conferencing!
          */
         if( nMailDelivery == MAIL_DELIVER_AI && IsCixAddress( DRF(lpfmo->recTo) ) )
            if( !( *DRF(lpfmo->recAttachments) && lpfmo->wEncodingScheme == ENCSCH_MIME ) )
               return( POF_HELDBYSCRIPT );
         if( nMailDelivery == MAIL_DELIVER_CIX )
         {
            if( lpfmo->wEncodingScheme == ENCSCH_MIME )
               return( POF_CANNOTACTION );
            return( POF_HELDBYSCRIPT );
         }
         /* If IP delivery and the attachment is binary, send it via conferencing.
          */
         if( nMailDelivery == MAIL_DELIVER_IP && IsCixAddress( DRF(lpfmo->recTo) ) )
            if( ( *DRF(lpfmo->recAttachments) && lpfmo->wEncodingScheme == ENCSCH_BINARY ) )
               return( POF_HELDBYSCRIPT );
         return( Exec_ForwardMail( dwData ) );

      case OBEVENT_EDITABLE:
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR748), DRF(lpfmo->recSubject), DRF(lpfmo->recTo) );
         return( TRUE );

      case OBEVENT_NEW:
         memset( lpfmo, 0, sizeof( FORWARDMAILOBJECT ) );
         lpfmo->wEncodingScheme = 0;
         lpfmo->wFlags = fCCMailToSender ? 0 : MOF_NOECHO;
         lpfmo->nPriority = MAIL_PRIORITY_NORMAL;
         return( TRUE );

      case OBEVENT_LOAD: {
         FORWARDMAILOBJECT mo;

         Amob_LoadDataObject( fh, &mo, sizeof( FORWARDMAILOBJECT ) );
         if( fNewMemory( &lpfmo, sizeof( FORWARDMAILOBJECT ) ) )
            {
            *lpfmo = mo;
            lpfmo->recTo.hpStr = NULL;
            lpfmo->recReplyTo.hpStr = NULL;
            lpfmo->recCC.hpStr = NULL;
            lpfmo->recBCC.hpStr = NULL;
            lpfmo->recText.hpStr = NULL;
            lpfmo->recSubject.hpStr = NULL;
            lpfmo->recAttachments.hpStr = NULL;
            lpfmo->recMailbox.hpStr = NULL;
            lpfmo->recFrom.hpStr = NULL;
            }
         return( (LRESULT)lpfmo );
         }

      case OBEVENT_EDIT: {
         OBINFO obinfo;

         Amob_GetObInfo( (LPOB)dwData, &obinfo );
         CreateFwMailWindow( hwndFrame, obinfo.lpObData, (LPOB)dwData );
         return( TRUE );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpfmo->recTo );
         Amob_SaveRefObject( &lpfmo->recReplyTo );
         Amob_SaveRefObject( &lpfmo->recCC );
         Amob_SaveRefObject( &lpfmo->recBCC );
         Amob_SaveRefObject( &lpfmo->recSubject );
         Amob_SaveRefObject( &lpfmo->recText );
         Amob_SaveRefObject( &lpfmo->recMailbox );
         Amob_SaveRefObject( &lpfmo->recAttachments );
         Amob_SaveRefObject( &lpfmo->recFrom );
         return( Amob_SaveDataObject( fh, lpfmo, sizeof( FORWARDMAILOBJECT ) ) );

      case OBEVENT_COPY: {
         FORWARDMAILOBJECT FAR * lpfmoNew;

         INITIALISE_PTR( lpfmoNew );
         lpfmo = (FORWARDMAILOBJECT FAR *)dwData;
         if( fNewMemory( &lpfmoNew, sizeof( FORWARDMAILOBJECT ) ) )
            {
            lpfmoNew->nPriority = lpfmo->nPriority;
            lpfmoNew->wFlags = lpfmo->wFlags;
            lpfmoNew->wEncodingScheme = lpfmo->wEncodingScheme;
            INITIALISE_PTR( lpfmoNew->recTo.hpStr );
            INITIALISE_PTR( lpfmoNew->recReplyTo.hpStr );
            INITIALISE_PTR( lpfmoNew->recCC.hpStr );
            INITIALISE_PTR( lpfmoNew->recBCC.hpStr );
            INITIALISE_PTR( lpfmoNew->recText.hpStr );
            INITIALISE_PTR( lpfmoNew->recAttachments.hpStr );
            INITIALISE_PTR( lpfmoNew->recSubject.hpStr );
            INITIALISE_PTR( lpfmoNew->recMailbox.hpStr );
            INITIALISE_PTR( lpfmoNew->recFrom.hpStr );
            Amob_CopyRefObject( &lpfmoNew->recFrom, &lpfmo->recFrom );
            Amob_CopyRefObject( &lpfmoNew->recTo, &lpfmo->recTo );
            Amob_CopyRefObject( &lpfmoNew->recReplyTo, &lpfmo->recReplyTo );
            Amob_CopyRefObject( &lpfmoNew->recCC, &lpfmo->recCC );
            Amob_CopyRefObject( &lpfmoNew->recBCC, &lpfmo->recBCC );
            Amob_CopyRefObject( &lpfmoNew->recText, &lpfmo->recText );
            Amob_CopyRefObject( &lpfmoNew->recSubject, &lpfmo->recSubject );
            Amob_CopyRefObject( &lpfmoNew->recAttachments, &lpfmo->recAttachments );
            Amob_CopyRefObject( &lpfmoNew->recMailbox, &lpfmo->recMailbox );
            }
         return( (LRESULT)lpfmoNew );
         }

      case OBEVENT_FIND: {
         FORWARDMAILOBJECT FAR * lpfmo1;
         FORWARDMAILOBJECT FAR * lpfmo2;

         lpfmo1 = (FORWARDMAILOBJECT FAR *)dwData;
         lpfmo2 = (FORWARDMAILOBJECT FAR *)lpBuf;
         if( strcmp( DRF(lpfmo1->recMailbox), DRF(lpfmo2->recMailbox) ) != 0 )
            return( FALSE );
         if( strcmp( DRF(lpfmo1->recSubject), DRF(lpfmo2->recSubject) ) != 0 )
            return( FALSE );
         return( TRUE );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpfmo );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpfmo->recFrom );
         Amob_FreeRefObject( &lpfmo->recTo );
         Amob_FreeRefObject( &lpfmo->recReplyTo );
         Amob_FreeRefObject( &lpfmo->recCC );
         Amob_FreeRefObject( &lpfmo->recBCC );
         Amob_FreeRefObject( &lpfmo->recSubject );
         Amob_FreeRefObject( &lpfmo->recText );
         Amob_FreeRefObject( &lpfmo->recAttachments );
         Amob_FreeRefObject( &lpfmo->recMailbox );
         return( TRUE );
      }
   return( 0L );
}
