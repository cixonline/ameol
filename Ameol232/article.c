/* ARTICLE.C - Handles Newsnet articles
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
#include <dos.h>
#include <ctype.h>
#include <memory.h>
#include <string.h>
#include "print.h"
#include "rules.h"
#include "shared.h"
#include "cixip.h"
#include <io.h>
#include "ameol2.h"
#include "news.h"
#include "cookie.h"
#include "editmap.h"

#ifndef USEBIGEDIT
#include "scintilla.h"
#include "scilexer.h"
#endif USEBIGEDIT

#define  THIS_FILE   __FILE__

extern BOOL fWordWrap;                 /* TRUE if we word wrap messages */

char NEAR szArticleWndClass[] = "amctl_articlewndcls";
char NEAR szArticleWndName[] = "Article";

static BOOL fDefDlgEx = FALSE;

static BOOL fRegistered = FALSE;       /* TRUE if we've registered the article window */
static WNDPROC lpEditCtrlProc = NULL;  /* Edit control window procedure address */
static BOOL fNewMsg;                   /* TRUE if we're editing a new message */

typedef struct tagARTICLEWNDINFO {
   LPOB lpob;
   DWORD dwReply;
   HWND hwndFocus;
   RECPTR recFolder;
   char szSigFile[ 16 ];
   PTL ptl;
} ARTICLEWNDINFO, FAR * LPARTICLEWNDINFO;

#define  GetArticleWndInfo(h)    (LPARTICLEWNDINFO)GetWindowLong((h),DWL_USER)
#define  SetArticleWndInfo(h,p)  SetWindowLong((h),DWL_USER,(long)(p))

BOOL FASTCALL ArticleWnd_OnCreate( HWND, LPCREATESTRUCT );
void FASTCALL ArticleWnd_OnClose( HWND );
void FASTCALL ArticleWnd_OnSize( HWND, UINT, int, int );
void FASTCALL ArticleWnd_OnMove( HWND, int, int );
void FASTCALL ArticleWnd_MDIActivate( HWND, BOOL, HWND, HWND );
void FASTCALL ArticleWnd_OnCommand( HWND, int, HWND, UINT );
void FASTCALL ArticleWnd_OnSetFocus( HWND, HWND );
void FASTCALL ArticleWnd_OnAdjustWindows( HWND, int, int );
LRESULT FASTCALL ArticleWnd_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr );

LRESULT EXPORT CALLBACK ArticleWndProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK ArticleEditProc( HWND, UINT, WPARAM, LPARAM );

BOOL FASTCALL CheckNewsAttachmentSize( HWND, LPSTR, int );

/* This function posts a new news article.
 */
void FASTCALL CmdPostArticle( HWND hwnd, LPSTR lpszText )
{
   if( hwndTopic || hwndActive == hwndInBasket )
      {
      ARTICLEOBJECT ao;
      char sz[ 128 ];

      Amob_New( OBTYPE_ARTICLEMESSAGE, &ao );
      Amob_CreateRefString( &ao.recReplyTo, szMailReplyAddress );
      if( hwndTopic )
         {
         LPWINDINFO lpWindInfo;
   
         lpWindInfo = GetBlock( hwndTopic );
         if( Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_NEWS )
            {
            char szReplyTo[ LEN_INTERNETNAME+1 ];

            Amob_CreateRefString( &ao.recNewsgroup, (HPSTR)Amdb_GetTopicName( lpWindInfo->lpTopic ) );
            WriteFolderPathname( sz, lpWindInfo->lpTopic );
            Amob_CreateRefString( &ao.recFolder, sz );
            Amdb_GetCookie( lpWindInfo->lpTopic, NEWS_REPLY_TO_COOKIE, szReplyTo, szMailReplyAddress, sizeof(szReplyTo) );
            Amob_CreateRefString( &ao.recReplyTo, szReplyTo );
            }
         }
      else if( hwndActive == hwndInBasket )
         {
         HWND hwndTreeCtl;
         HTREEITEM hSelItem;
   
         hwndTreeCtl = GetDlgItem( hwndInBasket, IDD_FOLDERLIST );
         if( 0L != ( hSelItem = TreeView_GetSelection( hwndTreeCtl ) ) )
            {
            TV_ITEM tv;
   
            /* Get the lParam of the selected item.
             */
            tv.hItem = hSelItem;
            tv.mask = TVIF_PARAM;
            VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
            if( Amdb_IsTopicPtr( (PTL)tv.lParam ) )
               if( Amdb_GetTopicType( (PTL)tv.lParam ) == FTYPE_NEWS )
                  {
                  char szReplyTo[ LEN_INTERNETNAME+1 ];

                  Amob_CreateRefString( &ao.recNewsgroup, (HPSTR)Amdb_GetTopicName( (PTL)tv.lParam ) );
                  WriteFolderPathname( sz, (PTL)tv.lParam );
                  Amob_CreateRefString( &ao.recFolder, sz );
                  Amdb_GetCookie( (PTL)tv.lParam, NEWS_REPLY_TO_COOKIE, szReplyTo, szMailReplyAddress, sizeof(szReplyTo) );
                  Amob_CreateRefString( &ao.recReplyTo, szReplyTo );
                  }
            }
         }
      ao.wEncodingScheme = wDefEncodingScheme;
      if( NULL != lpszText )
         Amob_CreateRefString( &ao.recText, lpszText );
      CreateArticleWindow( hwndFrame, &ao, NULL );
      Amob_Delete( OBTYPE_ARTICLEMESSAGE, &ao );
      }
}

/* This function posts a follow-up to a news article.
 */
void FASTCALL CmdFollowUpArticle( HWND hwnd, BOOL fFollowUpAll )
{
   LPWINDINFO lpWindInfo;

   lpWindInfo = GetBlock( hwnd );
   if( lpWindInfo->lpMessage && ( Amdb_GetTopicType ( lpWindInfo->lpTopic ) == FTYPE_NEWS ) )
      {
      char szReplyTo[ LEN_INTERNETNAME+1 ];
      char szSubject[ LEN_TITLE+4 ];
      ARTICLEOBJECT ao;
      MSGINFO msginfo;
      char sz[ 4096 ];
      LPOB lpob;

      /* Create a new follow-up message.
       */
      Amob_New( OBTYPE_ARTICLEMESSAGE, &ao );

      /* Create reply address.
       */
      Amdb_GetCookie( lpWindInfo->lpTopic, NEWS_REPLY_TO_COOKIE, szReplyTo, szMailReplyAddress, sizeof(szReplyTo) );
      Amob_CreateRefString( &ao.recReplyTo, szReplyTo );

      /* Create the subject line for the reply.
       */
      Amdb_GetMsgInfo( lpWindInfo->lpMessage, &msginfo );
      ao.dwReply = msginfo.dwMsg;
      if( _strnicmp( msginfo.szTitle, "Re[", 3 ) == 0 && isdigit( msginfo.szTitle[3] ) )
         {
         int uReCount;

         uReCount = atoi( msginfo.szTitle + 3 ) + 1;
         wsprintf( szSubject, "Re[%u]: %s", uReCount, (LPSTR)msginfo.szTitle );
         }
      else if( _strnicmp( msginfo.szTitle, "Re:", 3 ) == 0 )
         strcpy( szSubject, msginfo.szTitle );
      else
         wsprintf( szSubject, "Re: %s", (LPSTR)msginfo.szTitle );
      Amob_CreateRefString( &ao.recSubject, szSubject );

      /* Fill in the list of newsgroups.
       * NEW: Look for Followup-To and if found, use that as the
       * destination newsgroup unless it is 'poster' in which case
       * we send the message via e-mail.
       */
      Amdb_GetMailHeaderField( lpWindInfo->lpMessage, "Followup-To", sz, sizeof(sz), FALSE );
      if( '\0' == *sz )
         {
         if( fFollowUpAll )
         {
            BOOL fOk;
            fOk = Amdb_GetMailHeaderField( lpWindInfo->lpMessage, "Newsgroups", sz, sizeof(sz), TRUE );
            if( !fOk )
            {
               BOOL fDone = FALSE;
               int x;
               for( x = strlen( sz ); !fDone && x != 0; x-- )
               {
                  if( sz[ x ] == ',' || sz[ x ] == ' ' )
                  {
                     sz[ x ] = '\0';
                     fDone = TRUE;
                  }
               }
            }
         }
         if( '\0' == *sz )
            Amob_CreateRefString( &ao.recNewsgroup, (HPSTR)Amdb_GetTopicName( lpWindInfo->lpTopic ) );
         else
            Amob_CreateRefString( &ao.recNewsgroup, sz );
         }

      else
         {
         /* Reply to poster.
          */
         if( _strcmpi( sz, "poster" ) == 0 )
            {
            CmdMailReply( hwnd, FALSE, FALSE );
            Amob_Delete( OBTYPE_ARTICLEMESSAGE, &ao );
            return;
            }
         Amob_CreateRefString( &ao.recNewsgroup, sz );
         }

      /* The folder name is the mailbox name and the reply-to
       * name.
       */
      WriteFolderPathname( sz, lpWindInfo->lpTopic );
      Amob_CreateRefString( &ao.recFolder, sz );

      /* Add the message ID of the message to which we're replying as
       * the reply-to message number.
       */
      Amob_CreateRefString( &ao.recReply, msginfo.szReply );

      /* Check for an existing comment of this type.
       */
      if( NULL != ( lpob = Amob_Find( OBTYPE_ARTICLEMESSAGE, &ao ) ) )
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
            CreateArticleWindow( hwndFrame, obinfo.lpObData, lpob );
            Amob_Delete( OBTYPE_ARTICLEMESSAGE, &ao );
            return;
         }
         else
         {
            /* Set the encoding scheme.
             */
            ao.wEncodingScheme = wDefEncodingScheme;

            /* Finally, compose the quote reply if automatic
             * quoting is enabled.
             */
            if( fAutoQuoteNews && hwndQuote )
               CreateFollowUpQuote( lpWindInfo->lpMessage, &ao.recText, szQuoteNewsHeader );

            CreateArticleWindow( hwndFrame, &ao, NULL );
            Amob_Delete( OBTYPE_ARTICLEMESSAGE, &ao );
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
            /* Set the encoding scheme.
             */
            ao.wEncodingScheme = wDefEncodingScheme;

            /* Finally, compose the quote reply if automatic
             * quoting is enabled.
             */
            if( fAutoQuoteNews && hwndQuote )
               CreateFollowUpQuote( lpWindInfo->lpMessage, &ao.recText, szQuoteNewsHeader );

            CreateArticleWindow( hwndFrame, &ao, NULL );
            Amob_Delete( OBTYPE_ARTICLEMESSAGE, &ao );
            return;
         }
      }
      }

      /* Set the encoding scheme.
       */
      ao.wEncodingScheme = wDefEncodingScheme;

      /* Finally, compose the quote reply if automatic
       * quoting is enabled.
       */
      if( fAutoQuoteNews && hwndQuote )
         CreateFollowUpQuote( lpWindInfo->lpMessage, &ao.recText, szQuoteNewsHeader );

      /* Create the article window.
       */
      CreateArticleWindow( hwndFrame, &ao, NULL );
      Amob_Delete( OBTYPE_ARTICLEMESSAGE, &ao );
      }
}

/* This function creates the window that will display the full list of
 * Usenet newsgroups.
 */
BOOL FASTCALL CreateArticleWindow( HWND hwnd, ARTICLEOBJECT FAR * lpaoInit, LPOB lpob )
{
   LPARTICLEWNDINFO lpawi;
   HWND hwndArticle;
   HWND hwndFocus;
   DWORD dwState;
   int cxBorder;
   int cyBorder;
   RECT rc;

   /* Look for existing window
    */
   if( NULL != lpob )
      if( hwndArticle = Amob_GetEditWindow( lpob ) )
         {
         BringWindowToTop( hwndArticle );
         return( TRUE );
         }

   /* Register the group list window class if we have
    * not already done so.
    */
   if( !fRegistered )
      {
      WNDCLASS wc;

      wc.style          = CS_HREDRAW | CS_VREDRAW;
      wc.lpfnWndProc    = ArticleWndProc;
      wc.hIcon          = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_MAIL) );
      wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
      wc.lpszMenuName   = NULL;
      wc.cbWndExtra     = DLGWINDOWEXTRA;
      wc.cbClsExtra     = 0;
      wc.hbrBackground  = (HBRUSH)( COLOR_APPWORKSPACE + 1 );
      wc.lpszClassName  = szArticleWndClass;
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
   cxBorder = ( rc.right - rc.left ) / 6;
   cyBorder = ( rc.bottom - rc.top ) / 6;
   InflateRect( &rc, -cxBorder, -cyBorder );
   dwState = 0;

   /* Get the actual window size, position and state.
    */
   ReadProperWindowState( szArticleWndName, &rc, &dwState );

   /* If lpob is NULL, this is a new msg.
    */
   fNewMsg = lpob == NULL;

   /* Create the window.
    */
   hwndArticle = Adm_CreateMDIWindow( szArticleWndName, szArticleWndClass, hInst, &rc, dwState, (LPARAM)lpaoInit );
   if( NULL == hwndArticle )
      return( FALSE );

   /* Rename the Send button to Save if we're offline.
    */
   if( !fOnline )
      SetWindowText( GetDlgItem( hwndArticle, IDOK ), "&Save" );

   /* Store the handle of the destination structure.
    */
   lpawi = GetArticleWndInfo( hwndArticle );
   lpawi->lpob = lpob;
   lpawi->dwReply = lpaoInit->dwReply;
   lpawi->recFolder.hpStr = NULL;
   Amob_CopyRefObject( &lpawi->recFolder, &lpaoInit->recFolder );

   /* Show the window.
    */
   SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndArticle, 0L );

   /* Determine where we put the focus.
    */
   if( lpaoInit->recSubject.dwSize == 0 )
      hwndFocus = GetDlgItem( hwndArticle, IDD_SUBJECT );
   else
      {
      /* Pop the insertion point at the end of the edit text
       * if this is a new message.
       */
      hwndFocus = GetDlgItem( hwndArticle, IDD_EDIT );
      if( NULL == lpob )
         {
         Edit_SetSel( hwndFocus, 32767, 32767 );
         Edit_ScrollCaret( hwndFocus );
         }
      }
   SetFocus( hwndFocus );
   lpawi->hwndFocus = hwndFocus;
   if( NULL != lpob )
      Amob_SetEditWindow( lpob, hwndArticle );

   /* Set the signature now.
    */
   if( fNewMsg )
      {
      SetEditSignature( GetDlgItem( hwndArticle, IDD_EDIT ), lpawi->ptl );
      Edit_SetModify( GetDlgItem( hwndArticle, IDD_EDIT ), FALSE );
#ifndef USEBIGEDIT
      SendMessage( GetDlgItem( hwndArticle, IDD_EDIT ), SCI_SETSAVEPOINT,  (WPARAM)0, (LPARAM)0L );   
#endif USEBIGEDIT
      }
   return( TRUE );
}

/* This is the main window procedure.
 */
LRESULT EXPORT CALLBACK ArticleWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, ArticleWnd_OnCreate );
      HANDLE_MSG( hwnd, WM_CLOSE, ArticleWnd_OnClose );
      HANDLE_MSG( hwnd, WM_SIZE, ArticleWnd_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, ArticleWnd_OnMove );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, Common_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, ArticleWnd_MDIActivate );
      HANDLE_MSG( hwnd, WM_COMMAND, ArticleWnd_OnCommand );
      HANDLE_MSG( hwnd, WM_MENUSELECT, MainWnd_OnMenuSelect );
      HANDLE_MSG( hwnd, WM_SETFOCUS, ArticleWnd_OnSetFocus );
      HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, ArticleWnd_OnAdjustWindows );
      HANDLE_MSG( hwnd, WM_CHANGEFONT, Common_ChangeFont );
      HANDLE_MSG( hwnd, WM_NOTIFY, ArticleWnd_OnNotify );

      case WM_APPCOLOURCHANGE:
      case WM_TOGGLEHEADERS: { // 2.55.2031
#ifdef USEBIGEDIT
         SetWindowFont( GetDlgItem( hwnd, IDD_EDIT ), hEditFont, FALSE );
#else USEBIGEDIT
         SetEditorDefaults(GetDlgItem( hwnd, IDD_EDIT ), FALSE, lfEditFont, fWordWrap, fHotLinks , fMsgStyles, crEditText, crEditWnd );
#endif USEBIGEDIT
         break;
         }
/* Drag and drop attachment code. Crashes under Win9x.
 */
/* case WM_DROPFILES: {
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

                     StripNonPrintCharacters( lpText, (int)lSize );

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
         strcpy( lpTmpBuf, "\0" );
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
            hwndAttachField = GetDlgItem( hwnd, IDD_ATTACHMENTS );
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
      case WM_CONTEXTSWITCH: {
         if( wParam == AECSW_ONLINE )
            SetDlgItemText( hwnd, IDOK, (BOOL)lParam ? "&Send" : "&Save" );
         break;
                        }

      case WM_GETMINMAXINFO: {
         MINMAXINFO FAR * lpMinMaxInfo;
         LRESULT lResult;

         /* Set the minimum tracking size so the client window can never
          * be resized below 24 lines.
          */
         lResult = Adm_DefMDIDlgProc( hwnd, message, wParam, lParam );
         lpMinMaxInfo = (MINMAXINFO FAR *)lParam;
         lpMinMaxInfo->ptMinTrackSize.x = 250;
         lpMinMaxInfo->ptMinTrackSize.y = 225;
         return( lResult );
         }

      default:
         break;
      }
   return( Adm_DefMDIDlgProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_SETFOCUS message.
 */
void FASTCALL ArticleWnd_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   LPARTICLEWNDINFO lpawi;
   HWND hwndFocus;

   lpawi = GetArticleWndInfo( hwnd );
   hwndFocus = lpawi->hwndFocus;
   if( hwndFocus )
      SetFocus( hwndFocus );
   iActiveMode = WIN_EDITS;
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL ArticleWnd_OnCreate( HWND hwnd, LPCREATESTRUCT lpCreateStruct )
{
   LPMDICREATESTRUCT lpMDICreateStruct;
   LPARTICLEWNDINFO lpawi;
   ARTICLEOBJECT FAR * lpao;
   char szReplyTo[ 64 ];
   HWND hwndCombo;
   HWND hwndList;
   HWND hwndEdit;

   PTL ptl;


   INITIALISE_PTR(lpawi);

   /* Create the dialog window.
    */
   lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
   lpao = (ARTICLEOBJECT FAR *)lpMDICreateStruct->lParam;
   Adm_MDIDialog( hwnd, MAKEINTRESOURCE(IDDLG_ARTICLEEDITOR), lpMDICreateStruct );

   /* Set the edit window font.
    */
#ifdef USEBIGEDIT
   SetWindowFont( GetDlgItem( hwnd, IDD_EDIT ), hEditFont, FALSE );
#else USEBIGEDIT
   SetEditorDefaults(GetDlgItem( hwnd, IDD_EDIT ), FALSE, lfEditFont, fWordWrap, fHotLinks , fMsgStyles, crEditText, crEditWnd );
#endif USEBIGEDIT

   /* Create a structure to store the mail window info.
    */
   if( !fNewMemory( &lpawi, sizeof(ARTICLEWNDINFO) ) )
      return( FALSE );
   SetArticleWndInfo( hwnd, lpawi );
   lpawi->hwndFocus = NULL;

   /* Fill the list of signatures and set the
    * first one to (None).
    */
   FillSignatureList( hwnd, IDD_LIST );

   /* Fill the list of available encoding schemes.
    */
   FillEncodingScheme( hwnd, IDD_ENCODING, lpao->wEncodingScheme, TRUE, TRUE );

   /* Get the topic handle of the folder.
    */
   ptl = NULL;
   if( DRF(lpao->recFolder) )
      ParseFolderPathname( DRF(lpao->recFolder), &ptl, FALSE, 0 );
   lpawi->ptl = ptl;

   /* Fill Reply-To field.
    */
   VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_REPLYTO ) );
   ComboBox_AddString( hwndCombo, szMailReplyAddress );
   Amdb_GetCookie( ptl, NEWS_REPLY_TO_COOKIE, szReplyTo, "", sizeof(szReplyTo) );
   if( *szReplyTo )
      ComboBox_AddString( hwndCombo, szReplyTo );
   ComboBox_LimitText( hwndCombo, 63 );
   EnableControl( hwnd, IDD_REPLYTO, fUseInternet );

   /* Select the signature.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   CommonSetSignature( hwndList, ptl );
   ComboBox_GetText( hwndList, lpawi->szSigFile, sizeof(lpawi->szSigFile) );

   /* Fill in the Newsgroups field.
    */
   if( DRF(lpao->recNewsgroup) )
   {
      hwndEdit = GetDlgItem( hwnd, IDD_TO  );
      Edit_SetText( hwndEdit, lpao->recNewsgroup.hpStr );
      Edit_LimitText( hwndEdit, iMaxSubjectSize );
   }

   /* Fill the Subject field.
    */
   if( DRF(lpao->recSubject) )
   {
      hwndEdit = GetDlgItem( hwnd, IDD_SUBJECT );
      Edit_SetText( hwndEdit, lpao->recSubject.hpStr );
      Edit_LimitText( hwndEdit, iMaxSubjectSize );
   }


   /* Fill the attachments field.
    */
   if( DRF(lpao->recAttachments) )
      Edit_SetText( GetDlgItem( hwnd, IDD_ATTACHMENTS ), lpao->recAttachments.hpStr );

   /* Store the reply field.
    */
   if( DRF(lpao->recReply) )
      Edit_SetText( GetDlgItem( hwnd, IDD_REPLYFIELD ), lpao->recReply.hpStr );

   /* Disable the OK button if the To field is blank.
    */
   ComboBox_SetText( GetDlgItem( hwnd, IDD_REPLYTO ), DRF(lpao->recReplyTo) );
   EnableControl( hwnd, IDOK, lpao->recNewsgroup.dwSize > 0 && lpao->recSubject.dwSize > 0 );

   /* Hide the encoding style field if no attachment.
    */
   ShowHideEncodingField( hwnd );

   /* Subclass the edit control so we can do indents.
    */
   lpEditCtrlProc = SubclassWindow( GetDlgItem( hwnd, IDD_EDIT ), ArticleEditProc );

   /* Setup the WordWrap function
   */
#ifdef SMWORDBREAK
   SendDlgItemMessage(hwnd, IDD_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)(EDITWORDBREAKPROC)WordBreakProc) ;
#endif SMWORDBREAK
/* Drag and drop attachment code. Crashes under Win9x.
 */
/* if( fWinNT )
   {
   DragAcceptFiles( ( GetDlgItem( hwnd, IDD_EDIT ) ), TRUE );
   DragAcceptFiles( ( GetDlgItem( hwnd, IDD_ATTACHMENTS ) ), TRUE );
   }
*/
   /* Fill the article edit control.
    */
   SetEditText( hwnd, ptl, IDD_EDIT, DRF(lpao->recText), FALSE );
#ifndef USEBIGEDIT
   SendMessage( GetDlgItem( hwnd, IDD_EDIT ), SCI_SETSAVEPOINT,  (WPARAM)0, (LPARAM)0L ); 
#endif USEBIGEDIT
   Amob_FreeRefObject( &lpao->recText );
   return( TRUE );
}

/* This function shows or hides the encoding format drop-down
 * combo depending on whether or not an attachment is specified.
 */
void FASTCALL ShowHideEncodingField( HWND hwnd )
{
   HWND hwndEdit;

   hwndEdit = GetDlgItem( hwnd, IDD_ATTACHMENTS );
   ShowWindow( GetDlgItem( hwnd, IDD_ENCODING ), Edit_GetTextLength( hwndEdit ) > 0 );
   ShowWindow( GetDlgItem( hwnd, IDD_PAD7 ), Edit_GetTextLength( hwndEdit ) > 0 );
}

/* This function handles indents in the edit window.
 */
LRESULT EXPORT CALLBACK ArticleEditProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   return( EditCtrlIndent( lpEditCtrlProc, hwnd, msg, wParam, lParam ) );
}

LRESULT FASTCALL ArticleWnd_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
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
void FASTCALL ArticleWnd_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsARTICLEEDITOR );
         break;

      case IDM_QUICKPRINT:
      case IDM_PRINT:
         CmdComposedPrint( hwnd );
         break;

      case IDD_ATTACHMENTSBTN: {
         HWND hwndAttach;
         int length;

         BEGIN_PATH_BUF;
         VERIFY( hwndAttach = GetDlgItem( hwnd, IDD_ATTACHMENTS ) );

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
            ShowHideEncodingField( hwnd );
            }
         END_PATH_BUF;
         break;
         }

      case IDD_ATTACHMENTS:
         if( codeNotify == EN_CHANGE && id == IDD_ATTACHMENTS )
            ShowHideEncodingField( hwnd );

      case IDD_TO:
      case IDD_SUBJECT:
         if( codeNotify == EN_CHANGE && ( id == IDD_TO || id == IDD_SUBJECT ) )
            {
            EnableControl( hwnd, IDOK,
               Edit_GetTextLength( GetDlgItem( hwnd, IDD_TO ) ) > 0 &&
               Edit_GetTextLength( GetDlgItem( hwnd, IDD_SUBJECT ) ) > 0 );
            break;
            }

      case IDD_REPLYFIELD:
      case IDD_EDIT:
         if( codeNotify == EN_SETFOCUS )
            {
            LPARTICLEWNDINFO lpawi;

            lpawi = GetArticleWndInfo( hwnd );
            lpawi->hwndFocus = hwndCtl;
            }
         else if( codeNotify == EN_ERRSPACE || codeNotify == EN_MAXTEXT )
            fMessageBox( hwnd, 0, GS(IDS_STR77), MB_OK|MB_ICONEXCLAMATION);
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

      case IDD_LIST:
         if( codeNotify == CBN_SELCHANGE )
            {
            LPARTICLEWNDINFO lpawi;
   
            lpawi = GetArticleWndInfo( hwnd );
            CommonSigChangeCode( hwnd, lpawi->szSigFile, FALSE, FALSE );
            }
         break;

      case IDOK: {
         LPARTICLEWNDINFO lpawi;
         ARTICLEOBJECT FAR * lpao;
         ARTICLEOBJECT ao;
         LPSTR lpNewsgroups;
         char szReplyTo[ 64 ];
         HWND hwndList;
         HWND hwndEdit;
         HWND hwndSay;
         int length;
         int index;
#ifndef USEBIGEDIT
         HEDIT hedit;
#endif USEBIGEDIT

         INITIALISE_PTR(lpNewsgroups);

         /* First spell check the document.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         hwndSay = GetDlgItem( hwnd, IDD_SUBJECT );
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

         /* Make sure we have a subject and newsgroup!
          */
         if( 0 == Edit_GetTextLength( GetDlgItem( hwnd, IDD_TO ) ) )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR1183), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_TO );
            break;
            }
         if( 0 == Edit_GetTextLength( GetDlgItem( hwnd, IDD_SUBJECT ) ) )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR1184), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_SUBJECT );
            break;
            }

         /* Dereference the ARTICLEOBJECT data structure.
          */
         lpawi = GetArticleWndInfo( hwnd );
         if( NULL == lpawi->lpob )
            {
            lpao = &ao;
            Amob_New( OBTYPE_ARTICLEMESSAGE, &ao );
            lpao->dwReply = lpawi->dwReply;
            }
         else
            {
            OBINFO obinfo;

            Amob_GetObInfo( lpawi->lpob, &obinfo );
            lpao = obinfo.lpObData;
            }

         /* Get and update the In-Reply-To field.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_REPLYFIELD );
         length = Edit_GetTextLength( hwndEdit );
         if( Amob_CreateRefObject( &lpao->recReply, length + 1 ) )
            {
            Edit_GetText( hwndEdit, lpao->recReply.hpStr, length + 1 );
            Amob_SaveRefObject( &lpao->recReply );
            }

         /* Get and update the Reply To field.
          */
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_REPLYTO ) );
         ComboBox_GetText( hwndEdit, szReplyTo, 64 );
         Amob_CreateRefString( &lpao->recReplyTo, szReplyTo );
         Amob_SaveRefObject( &lpao->recReplyTo );

         /* Get and update the Newsgroup field.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_TO );
         length = Edit_GetTextLength( hwndEdit );
         if( fNewMemory( &lpNewsgroups, length + 1 ) )
            {
            BOOL fCommad;
            char * lpStr;
            char * lpDest;

            /* First get the raw list of newsgroups, then compact them
             * so that they are separated by just commas. Spaces used as
             * separators are replaced with commas.
             */
            Edit_GetText( hwndEdit, lpNewsgroups, length + 1 );
            StripLeadingTrailingSpaces( lpNewsgroups );
            fCommad = FALSE;
            for( lpStr = lpNewsgroups, lpDest = lpStr; *lpStr; ++lpStr )
               if( *lpStr == ' ' && !fCommad )
                  fCommad = TRUE;
               else if( *lpStr == ',' || *lpStr == ';' )
                  fCommad = TRUE;
               else if( *lpStr != ' ' )
                  {
                  if( fCommad )
                     *lpDest++ = ',';
                  *lpDest++ = *lpStr;
                  fCommad = FALSE;
                  }
            *lpDest = '\0';
            Amob_CreateRefString( &lpao->recNewsgroup, lpNewsgroups );
            Amob_SaveRefObject( &lpao->recNewsgroup );
            FreeMemory( &lpNewsgroups );
            }

         /* Get and update the Subject field.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_SUBJECT );
         length = Edit_GetTextLength( hwndEdit );
         if( Amob_CreateRefObject( &lpao->recSubject, length + 1 ) )
            {
            Edit_GetText( hwndEdit, lpao->recSubject.hpStr, length + 1 );
            lstrcpy( lpTmpBuf, lpao->recSubject.hpStr );
            StripLeadingTrailingSpaces( lpTmpBuf );
            if( 0 == strlen( lpTmpBuf ) )
//          if( 1 == strlen( lpao->recSubject.hpStr ) )
//             if( lpao->recSubject.hpStr[ 0 ] == ' ' )
               {
               fMessageBox( hwnd, 0, GS(IDS_STR1184), MB_OK|MB_ICONEXCLAMATION );
               HighlightField( hwnd, IDD_SUBJECT );
               break;
               }
            Amob_SaveRefObject( &lpao->recSubject );
            }

         /* Get and update the Attachments field.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_ATTACHMENTS );
         length = Edit_GetTextLength( hwndEdit );
         if( Amob_CreateRefObject( &lpao->recAttachments, length + 1 ) )
            {
            Edit_GetText( hwndEdit, lpao->recAttachments.hpStr, length + 1 );
            Amob_SaveRefObject( &lpao->recAttachments );
            }

         /* And the encoding scheme.
          */
         if( !IsWindowVisible( GetDlgItem( hwnd, IDD_ENCODING ) ) )
            lpao->wEncodingScheme = ENCSCH_NONE;
         else
            {
            VERIFY( hwndList = GetDlgItem( hwnd, IDD_ENCODING ) );
            index = ComboBox_GetCurSel( hwndList );
            ASSERT( index != CB_ERR );
            lpao->wEncodingScheme = (int)ComboBox_GetItemData( hwndList, index );
            }

         /* Warn if encoded attachment is over 10K
          */
         if( *DRF(lpao->recAttachments) && lpao->wEncodingScheme != ENCSCH_BINARY )
            if( !CheckNewsAttachmentSize( hwnd, DRF(lpao->recAttachments), lpao->wEncodingScheme ) )
               break;


         /* Set the mailbox.
          */
         Amob_CopyRefObject( &lpao->recFolder, &lpawi->recFolder );
         Amob_FreeRefObject( &lpawi->recFolder );

         /* Get and update the Text field.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
#ifdef USEBIGEDIT
         Edit_FmtLines( hwndEdit, TRUE );
#else USEBIGEDIT
         hedit = Editmap_Open( hwndEdit );
         Editmap_FormatLines( hedit, hwndEdit);
#endif USEBIGEDIT
         length = Edit_GetTextLength( hwndEdit );
         if( Amob_CreateRefObject( &lpao->recText, length + 1 ) )
            {
            Edit_GetText( hwndEdit, lpao->recText.hpStr, length + 1 );
            Amob_SaveRefObject( &lpao->recText );
            }
         Amob_Commit( lpawi->lpob, OBTYPE_ARTICLEMESSAGE, lpao );

/* Drag and drop attachment code. Crashes under Win9x.
 */
/*       if( fWinNT )
         {
         DragAcceptFiles( ( GetDlgItem( hwnd, IDD_EDIT ) ), FALSE );
         DragAcceptFiles( ( GetDlgItem( hwnd, IDD_ATTACHMENTS ) ), FALSE );
         }
*/       if ( lpEditCtrlProc != NULL ) // Shouldn't be NULL at this point, but..
            SubclassWindow( GetDlgItem( hwnd, IDD_EDIT ), lpEditCtrlProc );
         Adm_DestroyMDIWindow( hwnd );
         break;
         }

      case IDCANCEL:
         /* Destroy the article window.
          */
         ArticleWnd_OnClose( hwnd );
         break;
      }
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL ArticleWnd_OnAdjustWindows( HWND hwnd, int dx, int dy )
{
   Adm_MoveWnd( GetDlgItem( hwnd, IDOK ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDCANCEL ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_HELP ), dx, 0 );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_TO ), dx, 0 );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_SUBJECT ), dx, 0 );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_ATTACHMENTS ), dx, 0 );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_EDIT ), dx, dy );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_REPLYTO ), dx, 0 );
   SetEditWindowWidth( GetDlgItem( hwnd, IDD_EDIT ), iWrapCol );
}

/* This function handles the WM_CLOSE message.
 */
void FASTCALL ArticleWnd_OnClose( HWND hwnd )
{
   LPARTICLEWNDINFO lpawi;
   BOOL fModified;

   /* Set fModified to TRUE if any of the input fields have been
    * modified.
    */
   fModified = EditMap_GetModified( GetDlgItem( hwnd, IDD_EDIT ) );
   fModified |= EditMap_GetModified( GetDlgItem( hwnd, IDD_TO ) );
   fModified |= EditMap_GetModified( GetDlgItem( hwnd, IDD_SUBJECT ) );
   fModified |= EditMap_GetModified( GetDlgItem( hwnd, IDD_ATTACHMENTS ) );

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
   lpawi = GetArticleWndInfo( hwnd );
   if( NULL != lpawi->lpob )
      Amob_Uncommit( lpawi->lpob );
/* Drag and drop attachment code. Crashes under Win9x.
 */
/* if( fWinNT )
   {
   DragAcceptFiles( ( GetDlgItem( hwnd, IDD_EDIT ) ), FALSE );
   DragAcceptFiles( ( GetDlgItem( hwnd, IDD_ATTACHMENTS ) ), FALSE );
   }
*/
   if ( lpEditCtrlProc != NULL ) // Shouldn't be NULL at this point, but..
      SubclassWindow( GetDlgItem( hwnd, IDD_EDIT ), lpEditCtrlProc );
   Adm_DestroyMDIWindow( hwnd );
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL ArticleWnd_OnMove( HWND hwnd, int x, int y )
{
   Amuser_WriteWindowState( szArticleWndName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL ArticleWnd_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   /* Save the new window state to the configuration file.
    */
   Amuser_WriteWindowState( szArticleWndName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* This function handles the WM_MDIACTIVATE message.
 */
void FASTCALL ArticleWnd_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   if( fActive )
      ToolbarState_CopyPaste();
   Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_EDITS ), (LPARAM)(LPSTR)hwnd );
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, Adm_DefMDIDlgProc );
}

/* This is the article message out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_ArticleMessage( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   ARTICLEOBJECT FAR * lpao;

   lpao = (ARTICLEOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_GETTYPE:
         return( BF_POSTIPNEWS );

      case OBEVENT_EXEC:
         if( IsCixnewsNewsgroup( DRF(lpao->recNewsgroup) ) )
            return( POF_HELDBYSCRIPT );
         return( Exec_ArticleMessage( dwData ) );

      case OBEVENT_EDITABLE:
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR734), DRF(lpao->recSubject), DRF(lpao->recNewsgroup) );
         return( TRUE );

      case OBEVENT_GETCLSID:
         if( IsCixnewsNewsgroup( DRF(lpao->recNewsgroup) ) )
            return( 3 );
         return( Amob_GetNextObjectID() );

      case OBEVENT_NEW:
         memset( lpao, 0, sizeof( ARTICLEOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         ARTICLEOBJECT ao;

         INITIALISE_PTR(lpao);
         Amob_LoadDataObject( fh, &ao, sizeof( ARTICLEOBJECT ) );
         if( fNewMemory( &lpao, sizeof( ARTICLEOBJECT ) ) )
            {
            ao.recNewsgroup.hpStr = NULL;
            ao.recText.hpStr = NULL;
            ao.recReply.hpStr = NULL;
            ao.recReplyTo.hpStr = NULL;
            ao.recSubject.hpStr = NULL;
            ao.recFolder.hpStr = NULL;
            ao.recAttachments.hpStr = NULL;
            *lpao = ao;
            }
         return( (LRESULT)lpao );
         }

      case OBEVENT_EDIT: {
         OBINFO obinfo;

         Amob_GetObInfo( (LPOB)dwData, &obinfo );
         CreateArticleWindow( hwndFrame, obinfo.lpObData, (LPOB)dwData );
         return( TRUE );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpao->recNewsgroup );
         Amob_SaveRefObject( &lpao->recText );
         Amob_SaveRefObject( &lpao->recReply );
         Amob_SaveRefObject( &lpao->recReplyTo );
         Amob_SaveRefObject( &lpao->recSubject );
         Amob_SaveRefObject( &lpao->recFolder );
         Amob_SaveRefObject( &lpao->recAttachments );
         return( Amob_SaveDataObject( fh, lpao, sizeof( ARTICLEOBJECT ) ) );

      case OBEVENT_COPY: {
         ARTICLEOBJECT FAR * lpaoNew;

         INITIALISE_PTR( lpaoNew );
         if( fNewMemory( &lpaoNew, sizeof( ARTICLEOBJECT ) ) )
            {
            lpaoNew->dwReply = lpao->dwReply;
            lpaoNew->wEncodingScheme = lpao->wEncodingScheme;
            INITIALISE_PTR( lpaoNew->recNewsgroup.hpStr );
            INITIALISE_PTR( lpaoNew->recSubject.hpStr );
            INITIALISE_PTR( lpaoNew->recReply.hpStr );
            INITIALISE_PTR( lpaoNew->recReplyTo.hpStr );
            INITIALISE_PTR( lpaoNew->recText.hpStr );
            INITIALISE_PTR( lpaoNew->recFolder.hpStr );
            INITIALISE_PTR( lpaoNew->recAttachments.hpStr );
            Amob_CopyRefObject( &lpaoNew->recNewsgroup, &lpao->recNewsgroup );
            Amob_CopyRefObject( &lpaoNew->recSubject, &lpao->recSubject );
            Amob_CopyRefObject( &lpaoNew->recReply, &lpao->recReply );
            Amob_CopyRefObject( &lpaoNew->recReplyTo, &lpao->recReplyTo );
            Amob_CopyRefObject( &lpaoNew->recText, &lpao->recText );
            Amob_CopyRefObject( &lpaoNew->recFolder, &lpao->recFolder );
            Amob_CopyRefObject( &lpaoNew->recAttachments, &lpao->recAttachments );
            }
         return( (LRESULT)lpaoNew );
         }

      case OBEVENT_FIND: {
         ARTICLEOBJECT FAR * lpao1;
         ARTICLEOBJECT FAR * lpao2;

         lpao1 = (ARTICLEOBJECT FAR *)dwData;
         lpao2 = (ARTICLEOBJECT FAR *)lpBuf;
         if( lpao1->dwReply != lpao2->dwReply )
            return( FALSE );
         return( TRUE );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpao );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpao->recNewsgroup );
         Amob_FreeRefObject( &lpao->recSubject );
         Amob_FreeRefObject( &lpao->recReply );
         Amob_FreeRefObject( &lpao->recReplyTo );
         Amob_FreeRefObject( &lpao->recText );
         Amob_FreeRefObject( &lpao->recFolder );
         Amob_FreeRefObject( &lpao->recAttachments );
         return( TRUE );
      }
   return( 0L );
}

/* This function checks the attachment size and warns if warnings are on */

BOOL FASTCALL CheckNewsAttachmentSize( HWND hwnd, LPSTR lpAttachments, int wScheme )
{
   while( *lpAttachments )
      {
      HNDFILE fh;

      lpAttachments = ExtractFilename( lpPathBuf, lpAttachments );
      if( HNDFILE_ERROR == ( fh = Amfile_Open( lpPathBuf, AOF_SHARE_READ|AOF_SHARE_WRITE ) ) )
      {
         if( ( fh = Amfile_Open( lpPathBuf, AOF_SHARE_READ ) ) == HNDFILE_ERROR )
         {
         /* File not found, so offer to cancel
          */
         wsprintf( lpTmpBuf, GS(IDS_STR1200), lpPathBuf );
         fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONINFORMATION );
         return( FALSE );
         }
      }
      else 
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
