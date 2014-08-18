/* COMMENT.C - Handles CIX comment messages
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
#include "amlib.h"
#include "shared.h"
#include "cix.h"
#include <io.h>
#include "ameol2.h"
#include "tti.h"
#include "..\Amdb32\amdbi.h"
#include "editmap.h"
#include "webapi.h"

#ifndef USEBIGEDIT
#include "scintilla.h"
#include "scilexer.h"
#endif USEBIGEDIT

#define  THIS_FILE   __FILE__
#define  NOCOMMENT_FILENAME   "nocommnt.ini"
#define NO_COMMENT         "NoComment"

char NEAR szCommentWndClass[] = "amctl_Commentwndcls";
char NEAR szCommentWndName[] = "Comment";
static char szNoCommentFile[ _MAX_PATH ];

extern BOOL fWordWrap;                 /* TRUE if we word wrap messages */

static BOOL fDefDlgEx = FALSE;

static BOOL fRegistered = FALSE;       /* TRUE if we've registered the comment window */
static WNDPROC lpEditCtrlProc = NULL;  /* Edit control window procedure address */
static BOOL fNewMsg;                   /* TRUE if we're editing a new message */

static WNDPROC lpWrdBrkProc = NULL; /*!!SM!!*/

typedef struct tagCOMMENTWNDINFO {
   LPOB lpob;
   HWND hwndFocus;
   char szSigFile[ 16 ];
} COMMENTWNDINFO, FAR * LPCOMMENTWNDINFO;

#define  GetCommentWndInfo(h)    (LPCOMMENTWNDINFO)GetWindowLong((h),DWL_USER)
#define  SetCommentWndInfo(h,p)  SetWindowLong((h),DWL_USER,(long)(p))

/* Array of controls that have tooltips and their string
 * resource IDs.
 */
static TOOLTIPITEMS tti[] = { IDD_ORIGINAL, IDS_STR1017 };
static BOOL fHasNoCommentFile = FALSE;


BOOL FASTCALL CommentWnd_OnCreate( HWND, LPCREATESTRUCT );
void FASTCALL CommentWnd_OnClose( HWND );
void FASTCALL CommentWnd_OnSize( HWND, UINT, int, int );
void FASTCALL CommentWnd_OnMove( HWND, int, int );
void FASTCALL CommentWnd_MDIActivate( HWND, BOOL, HWND, HWND );
void FASTCALL CommentWnd_OnCommand( HWND, int, HWND, UINT );
void FASTCALL CommentWnd_OnSetFocus( HWND, HWND );
void FASTCALL CommentWnd_OnAdjustWindows( HWND, int, int );
LRESULT FASTCALL CommentWnd_OnNotify( HWND, int, LPNMHDR );

LRESULT EXPORT CALLBACK CommentWndProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK CommentEditProc( HWND, UINT, WPARAM, LPARAM );

BOOL FASTCALL IsNoCommentFolder( PTL, char *, LPSTR, int );

/* This function posts a new CIX comment message.
 */
void FASTCALL CmdComment( HWND hwnd )
{
   if( hwndTopic )
      {
      LPWINDINFO lpWindInfo;

      lpWindInfo = GetBlock( hwndTopic );
      if( NULL != lpWindInfo->lpMessage )
         {
         TOPICINFO topicinfo;
         COMMENTOBJECT ao;
         MSGINFO msginfo;
         BOOL fReadOnly;
         LPOB lpob;
         PCL pcl;

         Amdb_GetTopicInfo( lpWindInfo->lpTopic, &topicinfo );
         pcl = Amdb_FolderFromTopic( lpWindInfo->lpTopic );
         fReadOnly = ( topicinfo.dwFlags & TF_READONLY ) && !Amdb_IsModerator( pcl, szCIXNickname );
         if( !fReadOnly )
            {
            /* Noticeboard conference? If so, offer to send via
             * mail.
             */
            Amdb_GetMsgInfo( lpWindInfo->lpMessage, &msginfo );
            if( IsNoCommentFolder( lpWindInfo->lpTopic, msginfo.szAuthor, NO_COMMENT, 10 ) )
               {
               int nDlgID = IDDLG_NOTICEBOARDPROMPTNOMAIL;
               int r;

               if (fUseLegacyCIX || fUseInternet)
                  {
                  nDlgID = IDDLG_NOTICEBOARDPROMPT;
                  }
               r = fDlgMessageBox( hwnd, idsNOTICEBOARDPROMPT, nDlgID, NULL, 0L, 0 );
               if( IDD_MAIL == r )
                  {
                  CmdMailReply( hwnd, FALSE, FALSE );
                  return;
                  }
               if( IDCANCEL == r )
                  return;
               }

            /* Create the COMMENT structure.
             */
            Amob_New( OBTYPE_COMMENTMESSAGE, &ao );
            Amob_CreateRefString( &ao.recConfName, (LPSTR)Amdb_GetFolderName( pcl ) );
            Amob_CreateRefString( &ao.recTopicName, (LPSTR)Amdb_GetTopicName( lpWindInfo->lpTopic ) );
            ao.dwReply = msginfo.dwMsg;
            if( NULL != ( lpob = Amob_Find( OBTYPE_COMMENTMESSAGE, &ao ) ) )
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
                        CreateCommentWindow( hwndFrame, obinfo.lpObData, lpob );
                        Amob_Delete( OBTYPE_COMMENTMESSAGE, &ao );
                        return;
                  }
                  else
                  {
                     CreateCommentWindow( hwndFrame, &ao, NULL );
                     Amob_Delete( OBTYPE_COMMENTMESSAGE, &ao );
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
                     CreateCommentWindow( hwndFrame, &ao, NULL );
                     Amob_Delete( OBTYPE_COMMENTMESSAGE, &ao );
                     return;
                  }
               }
            }
            CreateCommentWindow( hwndFrame, &ao, NULL );
            Amob_Delete( OBTYPE_COMMENTMESSAGE, &ao );
            }
            else
               fMessageBox( hwnd, 0, GS(IDS_STR1241), MB_OK|MB_ICONEXCLAMATION);

         }
      }
}

/* This function creates the Comment editor window.
 */
BOOL FASTCALL CreateCommentWindow( HWND hwnd, COMMENTOBJECT FAR * lpcoInit, LPOB lpob )
{
   LPCOMMENTWNDINFO lpcwi;
   HWND hwndComment;
   HWND hwndFocus;
   DWORD dwState;
   int cxBorder;
   int cyBorder;
   RECT rc;

   /* Look for existing window
    */
   if( NULL != lpob )
      if( hwndComment = Amob_GetEditWindow( lpob ) )
         {
         BringWindowToTop( hwndComment );
         return( TRUE );
         }

   /* Register the group list window class if we have
    * not already done so.
    */
   if( !fRegistered )
      {
      WNDCLASS wc;

      wc.style       = CS_HREDRAW | CS_VREDRAW;
      wc.lpfnWndProc    = CommentWndProc;
      wc.hIcon       = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_COMMENT) );
      wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
      wc.lpszMenuName      = NULL;
      wc.cbWndExtra     = DLGWINDOWEXTRA;
      wc.cbClsExtra     = 0;
      wc.hbrBackground  = (HBRUSH)( COLOR_APPWORKSPACE + 1 );
      wc.lpszClassName  = szCommentWndClass;
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
   ReadProperWindowState( szCommentWndName, &rc, &dwState );

   /* If lpob is NULL, this is a new msg.
    */
   fNewMsg = lpob == NULL;

   /* Create the window.
    */
   hwndComment = Adm_CreateMDIWindow( szCommentWndName, szCommentWndClass, hInst, &rc, dwState, (LPARAM)lpcoInit );
   if( NULL == hwndComment )
      return( FALSE );

   /* Store the handle of the destination structure.
    */
   lpcwi = GetCommentWndInfo( hwndComment );
   lpcwi->lpob = lpob;

   /* Show the window.
    */
   SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndComment, 0L );

   /* Determine where we put the focus.
    */
   hwndFocus = GetDlgItem( hwndComment, IDD_EDIT );
   SetFocus( hwndFocus );
   lpcwi->hwndFocus = hwndFocus;
   if( NULL != lpob )
      Amob_SetEditWindow( lpob, hwndComment );
   return( TRUE );
}

/* This is the main window procedure.
 */
LRESULT EXPORT CALLBACK CommentWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, CommentWnd_OnCreate );
      HANDLE_MSG( hwnd, WM_CLOSE, CommentWnd_OnClose );
      HANDLE_MSG( hwnd, WM_SIZE, CommentWnd_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, CommentWnd_OnMove );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, Common_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, CommentWnd_MDIActivate );
      HANDLE_MSG( hwnd, WM_COMMAND, CommentWnd_OnCommand );
      HANDLE_MSG( hwnd, WM_MENUSELECT, MainWnd_OnMenuSelect );
      HANDLE_MSG( hwnd, WM_SETFOCUS, CommentWnd_OnSetFocus );
      HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, CommentWnd_OnAdjustWindows );
      HANDLE_MSG( hwnd, WM_CHANGEFONT, Common_ChangeFont );
      HANDLE_MSG( hwnd, WM_NOTIFY, CommentWnd_OnNotify );
      case WM_APPCOLOURCHANGE:
      case WM_TOGGLEHEADERS: { // 2.55.2031
#ifdef USEBIGEDIT
         SetWindowFont( GetDlgItem( hwnd, IDD_EDIT ), hEditFont, FALSE );
#else USEBIGEDIT
         SetEditorDefaults(GetDlgItem( hwnd, IDD_EDIT ), FALSE, lfEditFont, fWordWrap, fHotLinks , fMsgStyles, crEditText, crEditWnd );
#endif USEBIGEDIT
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
         lpMinMaxInfo->ptMinTrackSize.x = 420;
         lpMinMaxInfo->ptMinTrackSize.y = 230;
         return( lResult );
         }

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
         char szClassName[ 40 ];
         HWND hwndControl;

         /* Figure out which control we're under.
          */
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

                     StripNonPrintCharacters( lpText, (int)lSize );

                     lpText = ExpandUnixTextToPCFormat( lpText );

//                   SetWindowRedraw( hwndControl, FALSE ); // 20060302 - 2.56.2049.09
                     dwSel = Edit_GetSel( hwndControl );
                     Edit_ReplaceSel( hwndControl, lpText );
                     Edit_SetSel( hwndControl, LOWORD( dwSel ), HIWORD( dwSel ) );
                     Edit_ScrollCaret( hwndControl );
//                   SetWindowRedraw( hwndControl, TRUE ); // 20060302 - 2.56.2049.09
                     }
                  FreeMemory32( &lpText );
                  }
               Amfile_Close( fh );
               }
            }
         }
         DragFinish( hDrop );
         break;
         }

      default:
         break;
      }
   return( Adm_DefMDIDlgProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_SETFOCUS message.
 */
void FASTCALL CommentWnd_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   LPCOMMENTWNDINFO lpcwi;
   HWND hwndFocus;

   lpcwi = GetCommentWndInfo( hwnd );
   hwndFocus = lpcwi->hwndFocus;
   if( hwndFocus )
      SetFocus( hwndFocus );
   iActiveMode = WIN_EDITS;
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL CommentWnd_OnCreate( HWND hwnd, LPCREATESTRUCT lpCreateStruct )
{
   LPMDICREATESTRUCT lpMDICreateStruct;
   LPCOMMENTWNDINFO lpcwi;
   COMMENTOBJECT FAR * lpco;
   HWND hwndList;
   PTL ptl;

   INITIALISE_PTR(lpcwi);

   /* Create the dialog window.
    */
   lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
   lpco = (COMMENTOBJECT FAR *)lpMDICreateStruct->lParam;
   Adm_MDIDialog( hwnd, MAKEINTRESOURCE(IDDLG_COMMENTEDITOR), lpMDICreateStruct );

   /* Set the edit window font.
    */
#ifdef USEBIGEDIT
   SetWindowFont( GetDlgItem( hwnd, IDD_EDIT ), hEditFont, FALSE );
#else USEBIGEDIT
   SetEditorDefaults(GetDlgItem( hwnd, IDD_EDIT ), FALSE, lfEditFont, fWordWrap, fHotLinks , fMsgStyles, crEditText, crEditWnd );
#endif USEBIGEDIT

   /* Create a structure to store the mail window info.
    */
   if( !fNewMemory( &lpcwi, sizeof(COMMENTWNDINFO) ) )
      return( FALSE );
   SetCommentWndInfo( hwnd, lpcwi );
   lpcwi->hwndFocus = NULL;

   /* Set the title.
    */
   wsprintf( lpTmpBuf, GS(IDS_STR27), lpco->dwReply, DRF(lpco->recConfName), DRF(lpco->recTopicName) );
   SetWindowText( hwnd, lpTmpBuf );

   /* Fill the list of signatures and set the
    * first one to (None).
    */
   FillSignatureList( hwnd, IDD_LIST );

   /* Select the Conference field.
    */
   FillConfList( hwnd, DRF(lpco->recConfName) );
   ptl = FillTopicList( hwnd, DRF(lpco->recConfName), DRF(lpco->recTopicName) );

   /* Select the signature.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   CommonSetSignature( hwndList, ptl );
   ComboBox_GetText( hwndList, lpcwi->szSigFile, sizeof(lpcwi->szSigFile) );

   /* Fill the reply number field.
    */
   SetDlgItemLongInt( hwnd, IDD_COMMENT, lpco->dwReply );

   /* Subclass the edit control so we can do indents.
    */
   lpEditCtrlProc = SubclassWindow( GetDlgItem( hwnd, IDD_EDIT ), CommentEditProc );

   /* Setup the WordWrap function !!SM!!
   */
#ifdef SMWORDBREAK
   SendDlgItemMessage(hwnd, IDD_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)(EDITWORDBREAKPROC)WordBreakProc) ;
#endif SMWORDBREAK                                       
   /* Set the parameter picker button.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_ORIGINAL ), hInst, IDB_ORIGINALCOMMENT );

   /* Fill the article edit control.
    */
   SetEditText( hwnd, ptl, IDD_EDIT, Amob_DerefObject( &lpco->recText ), fNewMsg );
   Amob_FreeRefObject( &lpco->recText );

   DragAcceptFiles( ( GetDlgItem( hwnd, IDD_EDIT ) ), TRUE );

#ifndef USEBIGEDIT
   SendMessage( GetDlgItem( hwnd, IDD_EDIT ), SCI_SETSAVEPOINT,  (WPARAM)0, (LPARAM)0L ); 
#endif USEBIGEDIT

   /* Add them tooltips.
    */
   AddTooltipsToWindow( hwnd, 1, (LPTOOLTIPITEMS)&tti );
   return( TRUE );
}

/* This function handles indents in the edit window.
 */
LRESULT EXPORT CALLBACK CommentEditProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   return( EditCtrlIndent( lpEditCtrlProc, hwnd, msg, wParam, lParam ) );
}

LRESULT FASTCALL CommentWnd_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
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
void FASTCALL CommentWnd_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsCOMMENTEDITOR );
         break;

      case IDD_ORIGINAL: {
         DWORD dwReply;

         /* Get the comment number.
          */
         if( GetDlgLongInt( hwnd, IDD_COMMENT, &dwReply, 1, 99999999 ) )
            {
            char szTopicName[ LEN_TOPICNAME+1 ];
            char szConfName[ LEN_CONFNAME+1 ];
            CURMSG unr;

            /* Get the topic to which this message refers.
             */
            GetDlgItemText( hwnd, IDD_CONFNAME, szConfName, LEN_CONFNAME+1 );
            GetDlgItemText( hwnd, IDD_TOPICNAME, szTopicName, LEN_TOPICNAME+1 );
            unr.pcl = Amdb_OpenFolder( NULL, szConfName );
            if( NULL == unr.pcl )
            {
               fMessageBox( hwnd, 0, "This forum does not exist in your database", MB_OK|MB_ICONEXCLAMATION);
               break;
            }
            unr.ptl = Amdb_OpenTopic( unr.pcl, szTopicName );
            if( NULL == unr.ptl )
            {
               fMessageBox( hwnd, 0, "This topic does not exist in your database", MB_OK|MB_ICONEXCLAMATION);
               break;
            }
            Amdb_LockTopic( unr.ptl );
            unr.pth = Amdb_OpenMsg( unr.ptl, dwReply );
            unr.pcat = Amdb_CategoryFromFolder( unr.pcl );
            if( NULL != unr.pth )
               SetCurrentMsgEx( &unr, TRUE, FALSE, FALSE );
            Amdb_UnlockTopic( unr.ptl );
            }
         break;
         }

      case IDD_CONFNAME:
         if( codeNotify == CBN_SELCHANGE )
            {
            char szTopicName[ LEN_TOPICNAME+1 ];
            LPCOMMENTWNDINFO lpcwi;
            char sz[ LEN_CONFNAME+1 ];
            HWND hwndList;
            int nSel;

            hwndList = GetDlgItem( hwnd, IDD_CONFNAME );
            nSel = ComboBox_GetCurSel( hwndList );
            ComboBox_GetLBText( hwndList, nSel, sz );
            GetDlgItemText( hwnd, IDD_TOPICNAME, szTopicName, LEN_TOPICNAME+1 );
            FillTopicList( hwnd, sz, szTopicName );

            /* Set the signature for the selected topic.
             */
            lpcwi = GetCommentWndInfo( hwnd );
            CommonTopicSigChangeCode( hwnd, lpcwi->szSigFile );
            }
         break;

      case IDD_TOPICNAME:
         if( codeNotify == CBN_SELCHANGE )
            {
            LPCOMMENTWNDINFO lpcwi;

            lpcwi = GetCommentWndInfo( hwnd );
            CommonTopicSigChangeCode( hwnd, lpcwi->szSigFile );
            }
         break;

      case IDM_QUICKPRINT:
      case IDM_PRINT:
         CmdComposedPrint( hwnd );
         break;

      case IDD_COMMENT:
         if( codeNotify == EN_CHANGE )
            EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndCtl ) > 0 );

      case IDD_EDIT:
         if( codeNotify == EN_SETFOCUS )
            {
            LPCOMMENTWNDINFO lpcwi;

            lpcwi = GetCommentWndInfo( hwnd );
            lpcwi->hwndFocus = hwndCtl;
            }
         else if( codeNotify == EN_ERRSPACE || codeNotify == EN_MAXTEXT )
            fMessageBox( hwnd, 0, GS(IDS_STR77), MB_OK|MB_ICONEXCLAMATION);
         break;

      case IDD_LIST:
         if( codeNotify == CBN_SELCHANGE )
            {
            LPCOMMENTWNDINFO lpcwi;
   
            lpcwi = GetCommentWndInfo( hwnd );
            CommonSigChangeCode( hwnd, lpcwi->szSigFile, FALSE, FALSE );
            }
         break;

      case IDM_SPELLCHECK:
         Ameol2_SpellCheckDocument( hwnd, GetDlgItem( hwnd, IDD_EDIT ), SC_FL_DIALOG );
         break;

      case IDOK: {
         char szTopicName[ LEN_TOPICNAME+1 ];
         char szConfName[ LEN_CONFNAME+1 ];
         COMMENTOBJECT FAR * lpco;
         LPCOMMENTWNDINFO lpcwi;
         TOPICINFO topicinfo;
         COMMENTOBJECT ao;
         HWND hwndEdit;
         DWORD dwReply;
         int length;
         PCL pcl;
         PTL ptl;
#ifndef USEBIGEDIT
         HEDIT hedit;
#endif USEBIGEDIT

         /* First spell check the document.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         Edit_SetSel( hwndEdit, 0, 0 );
         if( fAutoSpellCheck && EditMap_GetModified( hwndEdit ) )
            {
            if( Ameol2_SpellCheckDocument( hwnd, hwndEdit, 0 ) == SP_CANCEL )
               break;
            }

         /* Dereference the COMMENTOBJECT data structure.
          */
         lpcwi = GetCommentWndInfo( hwnd );
         if( NULL == lpcwi->lpob )
            {
            lpco = &ao;
            Amob_New( OBTYPE_COMMENTMESSAGE, &ao );
            }
         else
            {
            OBINFO obinfo;

            Amob_GetObInfo( lpcwi->lpob, &obinfo );
            lpco = obinfo.lpObData;
            }

         /* Get the comment number.
          */
         if( !GetDlgLongInt( hwnd, IDD_COMMENT, &dwReply, 1, 99999999 ) )
            break;
         if( dwReply == 0 )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR436), MB_OK|MB_ICONEXCLAMATION );
            break;
            }

         /* If we've edited an existing out-basket object and we've
          * changed the comment number, offer to store this message as
          * a separate comment rather than replace the original.
          */
         if( lpcwi->lpob )
            if( lpco->dwReply != dwReply )
               {
               register int r;

               r = fMessageBox( hwnd, 0, GS(IDS_STR362), MB_YESNOCANCEL|MB_ICONQUESTION );
               if( r == IDYES )
                  {
                  Amob_Uncommit( lpcwi->lpob );
                  Amob_New( OBTYPE_COMMENTMESSAGE, &ao );
                  lpcwi->lpob = NULL;
                  lpco = &ao;
                  }
               if( r == IDCANCEL )
                  break;
               }
         lpco->dwReply = dwReply;

         /* Check that the topic to which we're writing this comment is not
          * read-only.
          */
         GetDlgItemText( hwnd, IDD_CONFNAME, szConfName, LEN_CONFNAME+1 );
         GetDlgItemText( hwnd, IDD_TOPICNAME, szTopicName, LEN_TOPICNAME+1 );
         pcl = Amdb_OpenFolder( NULL, szConfName );
         if( NULL == pcl )
         {
            fMessageBox( hwnd, 0, "This forum does not exist in your database", MB_OK|MB_ICONEXCLAMATION);
            break;
         }
         ptl = Amdb_OpenTopic( pcl, szTopicName );
         if( NULL == ptl )
         {
            fMessageBox( hwnd, 0, "This topic does not exist in your database", MB_OK|MB_ICONEXCLAMATION);
            break;
         }
         Amdb_GetTopicInfo( ptl, &topicinfo );
         if( ( topicinfo.dwFlags & TF_READONLY ) && !Amdb_IsModerator( pcl, szCIXNickname ) )
            {
            wsprintf( lpTmpBuf, GS(IDS_STR456),
                      Amdb_GetFolderName( pcl ),
                      Amdb_GetTopicName( ptl ),
                      Amdb_GetFolderName( pcl ) );
            if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION ) == IDNO )
               break;
            }

         /* Okay to save comment, so update in out-basket.
          */
         Amob_CreateRefString( &lpco->recTopicName, szTopicName );
         Amob_CreateRefString( &lpco->recConfName, szConfName );
         Amob_SaveRefObject( &lpco->recTopicName );
         Amob_SaveRefObject( &lpco->recConfName );

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
         if( Amob_CreateRefObject( &lpco->recText, length + 1 ) )
            {
            Edit_GetText( hwndEdit, lpco->recText.hpStr, length + 1 );
            Amob_SaveRefObject( &lpco->recText );
            }
         Amob_Commit( lpcwi->lpob, OBTYPE_COMMENTMESSAGE, lpco );
         RemoveTooltipsFromWindow( hwnd );
         Adm_DestroyMDIWindow( hwnd );
         break;
         }

      case IDCANCEL:
         /* Destroy the article window.
          */
         CommentWnd_OnClose( hwnd );
         break;
      }
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL CommentWnd_OnAdjustWindows( HWND hwnd, int dx, int dy )
{
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_EDIT ), dx, dy );
   SetEditWindowWidth( GetDlgItem( hwnd, IDD_EDIT ), iWrapCol );
}

/* This function handles the WM_CLOSE message.
 */
void FASTCALL CommentWnd_OnClose( HWND hwnd )
{
   LPCOMMENTWNDINFO lpcwi;
   BOOL fModified;

   /* Set fModified to TRUE if any of the input fields have been
    * modified.
    */
   fModified = EditMap_GetModified( GetDlgItem( hwnd, IDD_EDIT ) );
   fModified |= EditMap_GetModified( GetDlgItem( hwnd, IDD_SUBJECT ) );

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
   lpcwi = GetCommentWndInfo( hwnd );
   if( NULL != lpcwi->lpob )
      Amob_Uncommit( lpcwi->lpob );
   RemoveTooltipsFromWindow( hwnd );
   Adm_DestroyMDIWindow( hwnd );
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL CommentWnd_OnMove( HWND hwnd, int x, int y )
{
   Amuser_WriteWindowState( szCommentWndName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL CommentWnd_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   /* Save the new window state to the configuration file.
    */
   Amuser_WriteWindowState( szCommentWndName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* This function handles the WM_MDIACTIVATE message.
 */
void FASTCALL CommentWnd_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_EDITS ), (LPARAM)(LPSTR)hwnd );
   if( fActive )
      ToolbarState_EditWindow();
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, Adm_DefMDIDlgProc );
}

/* This is the Comment out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_CommentMessage( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   COMMENTOBJECT FAR * lpco;

   lpco = (COMMENTOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_EDITABLE:
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_EXEC:
         if (fUseWebServices) // !!SM!! 2.56.2054
         { 
            return (Am2Post(szCIXNickname, szCIXPassword, DRF(lpco->recConfName), DRF(lpco->recTopicName), DRF(lpco->recText), lpco->dwReply));
         }
         else
            return( POF_HELDBYSCRIPT );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR27), lpco->dwReply, DRF(lpco->recConfName), DRF(lpco->recTopicName) );
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( 0 );

      case OBEVENT_NEW:
         memset( lpco, 0, sizeof( COMMENTOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         COMMENTOBJECT mo;
      
         INITIALISE_PTR(lpco);
         Amob_LoadDataObject( fh, &mo, sizeof( COMMENTOBJECT ) );
         if( fNewMemory( &lpco, sizeof( COMMENTOBJECT ) ) )
            {
            mo.recConfName.hpStr = NULL;
            mo.recTopicName.hpStr = NULL;
            mo.recText.hpStr = NULL;
            *lpco = mo;
            }
         return( (LRESULT)lpco );
         }

      case OBEVENT_EDIT: {
         OBINFO obinfo;

         Amob_GetObInfo( (LPOB)dwData, &obinfo );
         CreateCommentWindow( hwndFrame, obinfo.lpObData, (LPOB)dwData );
         return( TRUE );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpco->recConfName );
         Amob_SaveRefObject( &lpco->recTopicName );
         Amob_SaveRefObject( &lpco->recText );
         return( Amob_SaveDataObject( fh, lpco, sizeof( COMMENTOBJECT ) ) );

      case OBEVENT_COPY: {
         COMMENTOBJECT FAR * lpcoNew;
      
         INITIALISE_PTR( lpcoNew );
         lpco = (COMMENTOBJECT FAR *)dwData;
         if( fNewMemory( &lpcoNew, sizeof( COMMENTOBJECT ) ) )
            {
            lpcoNew->dwReply = lpco->dwReply;
            INITIALISE_PTR( lpcoNew->recConfName.hpStr );
            INITIALISE_PTR( lpcoNew->recTopicName.hpStr );
            INITIALISE_PTR( lpcoNew->recText.hpStr );
            Amob_CopyRefObject( &lpcoNew->recConfName, &lpco->recConfName );
            Amob_CopyRefObject( &lpcoNew->recTopicName, &lpco->recTopicName );
            Amob_CopyRefObject( &lpcoNew->recText, &lpco->recText );
            }
         return( (LRESULT)lpcoNew );
         }

      case OBEVENT_FIND: {
         COMMENTOBJECT FAR * lpco1;
         COMMENTOBJECT FAR * lpco2;

         lpco1 = (COMMENTOBJECT FAR *)dwData;
         lpco2 = (COMMENTOBJECT FAR *)lpBuf;
         if( strcmp( DRF(lpco1->recConfName), DRF(lpco2->recConfName) ) != 0 )
            return( FALSE );
         if( strcmp( DRF(lpco1->recTopicName), DRF(lpco2->recTopicName) ) != 0 )
            return( FALSE );
         return( lpco1->dwReply == lpco2->dwReply );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpco );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpco->recConfName );
         Amob_FreeRefObject( &lpco->recTopicName );
         Amob_FreeRefObject( &lpco->recText );
         return( TRUE );
      }
   return( 0L );
}

/* Returns TRUE if pcl is a folder in which comments to a message
 * from the specified author are prohibited.
 */
BOOL FASTCALL IsNoCommentFolder( PTL ptl, char * pszAuthor, LPSTR lpKey, int cbMax  )
{
   char szSection[ 256 ];
   char szResult[ 10 ];

   if( !fHasNoCommentFile )
      {
      wsprintf( szNoCommentFile, "%s%s", pszAmeolDir, NOCOMMENT_FILENAME ); // VistaAlert
      fHasNoCommentFile = TRUE;
      }
   wsprintf( szSection, "%s", ptl->pcl->clItem.szFolderName );
   if( GetPrivateProfileString( szSection, lpKey, "", szResult, cbMax, szNoCommentFile ) )
      return( strcmp( pszAuthor, szCIXNickname ) != 0 );
   return( FALSE );
}
