/* FUL.C - Handles the CIX file upload command
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
#include "amlib.h"
#include "ameol2.h"
#include "shared.h"
#include "cix.h"
#include <string.h>
#include "editmap.h"

#ifndef USEBIGEDIT
#include "scintilla.h"
#include "scilexer.h"
#endif USEBIGEDIT

#define  THIS_FILE   __FILE__

extern BOOL fWordWrap;                 /* TRUE if we word wrap messages */

char NEAR szFileUploadWndClass[] = "amctl_fulwndcls";
char NEAR szFileUploadWndName[] = "Upload File";

static BOOL fDefDlgEx = FALSE;

static BOOL fRegistered = FALSE;          /* TRUE if we've registered the file upload window */
static WNDPROC lpEditCtrlProc = NULL;     /* Edit control window procedure address */
static BOOL fNewMsg;                      /* TRUE if we're editing a new message */

typedef struct tagFILEUPLOADINFO {
   LPOB lpob;
   HWND hwndFocus;
   char szSigFile[ 16 ];
} FILEUPLOADINFO, FAR * LPFILEUPLOADINFO;

#define  GetFileUploadWndInfo(h)    (LPFILEUPLOADINFO)GetWindowLong((h),DWL_USER)
#define  SetFileUploadWndInfo(h,p)  SetWindowLong((h),DWL_USER,(long)(p))

BOOL FASTCALL FileUploadWnd_OnCreate( HWND, LPCREATESTRUCT );
void FASTCALL FileUploadWnd_OnClose( HWND );
void FASTCALL FileUploadWnd_OnSize( HWND, UINT, int, int );
void FASTCALL FileUploadWnd_OnMove( HWND, int, int );
void FASTCALL FileUploadWnd_MDIActivate( HWND, BOOL, HWND, HWND );
void FASTCALL FileUploadWnd_OnCommand( HWND, int, HWND, UINT );
void FASTCALL FileUploadWnd_OnSetFocus( HWND, HWND );
void FASTCALL FileUploadWnd_OnAdjustWindows( HWND, int, int );
LRESULT FASTCALL FileUploadWnd_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr );

BOOL FASTCALL CreateFileUploadWindow( HWND, FULOBJECT FAR *, LPOB );

LRESULT EXPORT CALLBACK FileUploadWndProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK FileUploadEditProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK FileUploadWnd_GetMsgProc( int, WPARAM, LPARAM ); 

/* This function creates a file upload window.
 */
void FASTCALL CmdFileUpload( HWND hwnd )
{
   FULOBJECT fulo;
   CURMSG curmsg;

   Amob_New( OBTYPE_FUL, &fulo );
   Ameol2_GetCurrentTopic( &curmsg );
   if( NULL != curmsg.ptl )
      if( Amdb_GetTopicType( curmsg.ptl ) == FTYPE_CIX )
         {
         TOPICINFO topicinfo;

         /* Check whether topic is allowed to have a file list
          * If not, warn the user but allow them to override the internal flag. This
          * will be necessary if the topic was just created, but the user hasn't blinked
          * to download any messages yet.
          */
         Amdb_GetTopicInfo( curmsg.ptl, &topicinfo );
         if( !( topicinfo.dwFlags & TF_HASFILES ) )
            if( fMessageBox( GetFocus(), 0, GS(IDS_STR425), MB_YESNO|MB_ICONQUESTION ) == IDNO )
               return;

         /* Initialise the out-basket object.
          */
         Amob_CreateRefString( &fulo.recConfName, (LPSTR)Amdb_GetFolderName( curmsg.pcl ) );
         Amob_CreateRefString( &fulo.recTopicName, (LPSTR)Amdb_GetTopicName( curmsg.ptl ) );
         }
   CreateFileUploadWindow( hwndFrame, &fulo, NULL );
   Amob_Delete( OBTYPE_FUL, &fulo );
}

/* This function creates the window that will display the full list of
 * Usenet newsgroups.
 */
BOOL FASTCALL CreateFileUploadWindow( HWND hwnd, FULOBJECT FAR * lpfuloInit, LPOB lpob )
{
   LPFILEUPLOADINFO lpmwi;
   HWND hwndFul;
   HWND hwndFocus;
   DWORD dwState;
   int cxBorder;
   int cyBorder;
   RECT rc;

   /* Look for existing window
    */
   if( NULL != lpob )
      if( hwndFul = Amob_GetEditWindow( lpob ) )
         {
         BringWindowToTop( hwndFul );
         return( TRUE );
         }

   /* Register the group list window class if we have
    * not already done so.
    */
   if( !fRegistered )
      {
      WNDCLASS wc;

      wc.style          = CS_HREDRAW | CS_VREDRAW;
      wc.lpfnWndProc    = FileUploadWndProc;
      wc.hIcon          = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_FUL) );
      wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
      wc.lpszMenuName   = NULL;
      wc.cbWndExtra     = DLGWINDOWEXTRA;
      wc.cbClsExtra     = 0;
      wc.hbrBackground  = (HBRUSH)( COLOR_APPWORKSPACE + 1 );
      wc.lpszClassName  = szFileUploadWndClass;
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
   ReadProperWindowState( szFileUploadWndName, &rc, &dwState );

   /* If lpob is NULL, this is a new msg.
    */
   fNewMsg = lpob == NULL;

   /* Create the window.
    */
   hwndFul = Adm_CreateMDIWindow( szFileUploadWndName, szFileUploadWndClass, hInst, &rc, dwState, (LPARAM)lpfuloInit );
   if( NULL == hwndFul )
      return( FALSE );

   /* Store the handle of the destination structure.
    */
   lpmwi = GetFileUploadWndInfo( hwndFul );
   lpmwi->lpob = lpob;

   /* Determine where we put the focus.
    */
   hwndFocus = GetDlgItem( hwndFul, *DRF(lpfuloInit->recFileName) ? IDD_EDIT : IDD_FILENAME );
   lpmwi->hwndFocus = hwndFocus;
   SetFocus( hwndFocus );
   if( NULL != lpob )
      Amob_SetEditWindow( lpob, hwndFul );
   return( TRUE );
}

/* This is the main window procedure.
 */
LRESULT EXPORT CALLBACK FileUploadWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, FileUploadWnd_OnCreate );
      HANDLE_MSG( hwnd, WM_CLOSE, FileUploadWnd_OnClose );
      HANDLE_MSG( hwnd, WM_SIZE, FileUploadWnd_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, FileUploadWnd_OnMove );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, Common_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, FileUploadWnd_MDIActivate );
      HANDLE_MSG( hwnd, WM_COMMAND, FileUploadWnd_OnCommand );
      HANDLE_MSG( hwnd, WM_MENUSELECT, MainWnd_OnMenuSelect );
      HANDLE_MSG( hwnd, WM_SETFOCUS, FileUploadWnd_OnSetFocus );
      HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, FileUploadWnd_OnAdjustWindows );
      HANDLE_MSG( hwnd, WM_CHANGEFONT, Common_ChangeFont );
      HANDLE_MSG( hwnd, WM_NOTIFY, FileUploadWnd_OnNotify );

      case WM_APPCOLOURCHANGE:
      case WM_TOGGLEHEADERS: {  // 2.25.2031
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
         lpMinMaxInfo->ptMinTrackSize.x = 415;
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
void FASTCALL FileUploadWnd_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   LPFILEUPLOADINFO lpmwi;
   HWND hwndFocus;

   lpmwi = GetFileUploadWndInfo( hwnd );
   hwndFocus = lpmwi->hwndFocus;
   if( hwndFocus )
      SetFocus( hwndFocus );
   iActiveMode = WIN_EDITS;
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL FileUploadWnd_OnCreate( HWND hwnd, LPCREATESTRUCT lpCreateStruct )
{
   LPMDICREATESTRUCT lpMDICreateStruct;
   LPFILEUPLOADINFO lpmwi;
   FULOBJECT FAR * lpm;
   HWND hwndList;
   PTL ptl;

   INITIALISE_PTR(lpmwi);

   /* Create the dialog window.
    */
   lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
   lpm = (FULOBJECT FAR *)lpMDICreateStruct->lParam;
   Adm_MDIDialog( hwnd, MAKEINTRESOURCE(IDDLG_FUL), lpMDICreateStruct );

   /* Create a structure to store the file upload window info.
    */
   if( !fNewMemory( &lpmwi, sizeof(FILEUPLOADINFO) ) )
      return( FALSE );
   SetFileUploadWndInfo( hwnd, lpmwi );

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

   /* Select the Conference field.
    */
   FillConfList( hwnd, DRF(lpm->recConfName) );
   ptl = FillTopicList( hwnd, DRF(lpm->recConfName), DRF(lpm->recTopicName) );

   /* Select the signature.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   CommonSetSignature( hwndList, ptl );
   ComboBox_GetText( hwndList, lpmwi->szSigFile, sizeof(lpmwi->szSigFile) );

   /* Fill in the various fields.
    */
   Edit_SetText( GetDlgItem( hwnd, IDD_FILENAME ), DRF(lpm->recFileName) );

   /* Disable the OK button if the Filename field is blank and
    * the From field, because it can't be edited.
    */
   EnableControl( hwnd, IDOK, *DRF(lpm->recFileName) );

   /* Subclass the edit control so we can do indents.
    */
   lpEditCtrlProc = SubclassWindow( GetDlgItem( hwnd, IDD_EDIT ), FileUploadEditProc );

   /* Setup the WordWrap function !!SM!!
   */
#ifdef SMWORDBREAK
   SendDlgItemMessage(hwnd, IDD_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)(EDITWORDBREAKPROC)WordBreakProc) ;
#endif SMWORDBREAK
   /* Set the Browse picture button.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_PICKER ), hInst, IDB_FILEBUTTON );

   /* Fill the edit control.
    */
   SetEditText( hwnd, ptl, IDD_EDIT, DRF( lpm->recText ), fNewMsg );
#ifndef USEBIGEDIT
   SendMessage( GetDlgItem( hwnd, IDD_EDIT ), SCI_SETSAVEPOINT,  (WPARAM)0, (LPARAM)0L ); 
#endif USEBIGEDIT
   Amob_FreeRefObject( &lpm->recText );
   return( TRUE );
}

/* This function handles indents in the edit window.
 */
LRESULT EXPORT CALLBACK FileUploadEditProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   return( EditCtrlIndent( lpEditCtrlProc, hwnd, msg, wParam, lParam ) );
}

LRESULT FASTCALL FileUploadWnd_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
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
void FASTCALL FileUploadWnd_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsFUL );
         break;

      case IDD_PICKER: {
         static char strFilters[] = "Archive Files (*.zip;*.arc)\0*.zip;*.arc\0All Files (*.*)\0*.*\0\0";
         OPENFILENAME ofn;
         HWND hwndEdit;

         BEGIN_PATH_BUF;
         hwndEdit = GetDlgItem( hwnd, IDD_FILENAME );
         Edit_GetText( hwndEdit, lpPathBuf, _MAX_PATH+1 );
         if( ( strlen (lpPathBuf ) > 1 ) && ( lpPathBuf[ ( strlen( lpPathBuf ) - 1 ) ] == '\\' ) && ( lpPathBuf[ ( strlen( lpPathBuf ) - 2 ) ] != ':' ) )
            lpPathBuf[ ( strlen( lpPathBuf ) - 1 ) ] = '\0';
         ofn.lStructSize = sizeof( OPENFILENAME );
         ofn.hwndOwner = hwnd;
         ofn.hInstance = NULL;
         ofn.lpstrFilter = strFilters;
         ofn.lpstrCustomFilter = NULL;
         ofn.nMaxCustFilter = 0;
         ofn.nFilterIndex = 1;
         ofn.lpstrFile = lpPathBuf;
         ofn.nMaxFile = _MAX_FNAME;
         ofn.lpstrFileTitle = NULL;
         ofn.nMaxFileTitle = 0;
         ofn.lpstrTitle = "Upload File";
         ofn.Flags = OFN_NOCHANGEDIR|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
         ofn.nFileOffset = 0;
         ofn.nFileExtension = 0;
         ofn.lpstrDefExt = NULL;
         ofn.lCustData = 0;
         ofn.lpfnHook = NULL;
         ofn.lpTemplateName = 0;
         ExtractPathAndBasename( &ofn, lpPathBuf );
         if( GetOpenFileName( &ofn ) )
            {
            QuotifyFilename( ofn.lpstrFile );
            Edit_SetText( hwndEdit, ofn.lpstrFile );
            EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndEdit ) > 0 );
            SetFocus( hwndEdit );
            }
         if( ofn.lpstrInitialDir )
            FreeMemory( &(LPSTR)ofn.lpstrInitialDir );
         if( ofn.lpstrFile )
            FreeMemory( &(LPSTR)ofn.lpstrFile );
         END_PATH_BUF;
         break;
         }

      case IDD_CONFNAME:
         if( codeNotify == CBN_SELCHANGE )
            {
            char szTopicName[ LEN_TOPICNAME+1 ];
            char sz[ 14 ];
            HWND hwndList;
            int nSel;

            hwndList = GetDlgItem( hwnd, IDD_CONFNAME );
            nSel = ComboBox_GetCurSel( hwndList );
            ComboBox_GetLBText( hwndList, nSel, sz );
            GetDlgItemText( hwnd, IDD_TOPICNAME, szTopicName, LEN_TOPICNAME+1 );
            FillTopicList( hwnd, sz, szTopicName );
            }
         break;

      case IDD_EDIT:
         if( codeNotify == EN_SETFOCUS )
            {
            LPFILEUPLOADINFO lpawi;

            lpawi = GetFileUploadWndInfo( hwnd );
            lpawi->hwndFocus = hwndCtl;
            }
         else if( codeNotify == EN_ERRSPACE || codeNotify == EN_MAXTEXT )
            fMessageBox( hwnd, 0, GS(IDS_STR77), MB_OK|MB_ICONEXCLAMATION);
         break;

      case IDM_QUICKPRINT:
      case IDM_PRINT:
         CmdComposedPrint( hwnd );
         break;

      case IDM_SPELLCHECK:
         /* Spell check the body of the text.
          */
         Ameol2_SpellCheckDocument( hwnd, GetDlgItem( hwnd, IDD_EDIT ), SC_FL_DIALOG );
         break;

      case IDD_LIST:
         if( codeNotify == CBN_SELCHANGE )
            {
            LPFILEUPLOADINFO lpmwi;

            lpmwi = GetFileUploadWndInfo( hwnd );
            CommonSigChangeCode( hwnd, lpmwi->szSigFile, FALSE, FALSE );
            }
         break;

      case IDD_FILENAME:
         if( codeNotify == EN_SETFOCUS )
            {
            LPFILEUPLOADINFO lpmwi;

            lpmwi = GetFileUploadWndInfo( hwnd );
            lpmwi->hwndFocus = hwndCtl;
            }
         else if( codeNotify == EN_CHANGE )
            EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndCtl ) > 0 );
         break;

      case IDOK: {
         char szTopicName[ LEN_TOPICNAME+1 ];
         char szConfName[ LEN_CONFNAME+1 ];
         LPFILEUPLOADINFO lpmwi;
         FULOBJECT fulo;
         FULOBJECT FAR * lpfulo;
         HWND hwndEdit;
         int length;
#ifndef USEBIGEDIT
         HEDIT hedit;
#endif USEBIGEDIT

         /* First spell check the document.
          */
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_EDIT ) );
         Edit_SetSel( hwndEdit, 0, 0 );
         if( fAutoSpellCheck && EditMap_GetModified( hwndEdit ) )
            {
            if( Ameol2_SpellCheckDocument( hwnd, hwndEdit, 0 ) == SP_CANCEL )
               break;
            }

         /* Dereference the FULOBJECT data structure.
          */
         lpmwi = GetFileUploadWndInfo( hwnd );
         if( NULL == lpmwi->lpob )
            {
            Amob_New( OBTYPE_FUL, &fulo );
            lpfulo = &fulo;
            }
         else
            {
            OBINFO obinfo;

            Amob_GetObInfo( lpmwi->lpob, &obinfo );
            lpfulo = obinfo.lpObData;
            }

         /* Get the topic name.
          */
         GetDlgItemText( hwnd, IDD_CONFNAME, szConfName, LEN_CONFNAME+1 );
         GetDlgItemText( hwnd, IDD_TOPICNAME, szTopicName, LEN_TOPICNAME+1 );

         /* Get and validate the filename.
          */
         GetDlgItemText( hwnd, IDD_FILENAME, lpPathBuf, _MAX_PATH );
         if( !CheckValidCixFilename( hwnd, lpPathBuf ) )
            {
            HighlightField( hwnd, IDD_FILENAME );
            break;
            }

         /* Okay to save say, so update in out-basket.
          */
         Amob_CreateRefString( &lpfulo->recTopicName, szTopicName );
         Amob_CreateRefString( &lpfulo->recConfName, szConfName );
         Amob_SaveRefObject( &lpfulo->recTopicName );
         Amob_SaveRefObject( &lpfulo->recConfName );

         /* Save the filename.
          */
         Amob_CreateRefString( &lpfulo->recFileName, lpPathBuf );
         Amob_SaveRefObject( &lpfulo->recFileName );

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
         if( Amob_CreateRefObject( &lpfulo->recText, length + 1 ) )
            {
            Edit_GetText( hwndEdit, lpfulo->recText.hpStr, length + 1 );
            Amob_SaveRefObject( &lpfulo->recText );
            }
         Amob_Commit( lpmwi->lpob, OBTYPE_FUL, lpfulo );
         Adm_DestroyMDIWindow( hwnd );
         break;
         }

      case IDCANCEL:
         /* Destroy the window.
          */
         FileUploadWnd_OnClose( hwnd );
         break;
      }
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL FileUploadWnd_OnAdjustWindows( HWND hwnd, int dx, int dy )
{
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_PAD1 ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_PAD2 ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_PAD3 ), 0, dy );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_EDIT ), dx, dy );
   SetEditWindowWidth( GetDlgItem( hwnd, IDD_EDIT ), iWrapCol );
}

/* This function handles the WM_CLOSE message.
 */
void FASTCALL FileUploadWnd_OnClose( HWND hwnd )
{
   LPFILEUPLOADINFO lpmwi;
   BOOL fModified;

   /* Set fModified to TRUE if any of the input fields have been
    * modified.
    */
   fModified = EditMap_GetModified( GetDlgItem( hwnd, IDD_EDIT ) );
   fModified |= EditMap_GetModified( GetDlgItem( hwnd, IDD_FILENAME ) );

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
   lpmwi = GetFileUploadWndInfo( hwnd );
   if( NULL != lpmwi->lpob )
      Amob_Uncommit( lpmwi->lpob );
   Adm_DestroyMDIWindow( hwnd );
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL FileUploadWnd_OnMove( HWND hwnd, int x, int y )
{
   Amuser_WriteWindowState( szFileUploadWndName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL FileUploadWnd_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   /* Save the new window state to the configuration file.
    */
   Amuser_WriteWindowState( szFileUploadWndName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* This function handles the WM_MDIACTIVATE message.
 */
void FASTCALL FileUploadWnd_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_EDITS ), (LPARAM)(LPSTR)hwnd );
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, Adm_DefMDIDlgProc );
}

/* This is the file upload out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_FileUpload( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   FULOBJECT FAR * lpfulo;

   lpfulo = (FULOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_EDITABLE:
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR227), DRF(lpfulo->recFileName), DRF(lpfulo->recConfName), DRF(lpfulo->recTopicName) );
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_NEW:
         memset( lpfulo, 0, sizeof( FULOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         FULOBJECT fulo;

         Amob_LoadDataObject( fh, &fulo, sizeof( FULOBJECT ) );
         if( fNewMemory( &lpfulo, sizeof( FULOBJECT ) ) )
            {
            *lpfulo = fulo;
            lpfulo->recConfName.hpStr = NULL;
            lpfulo->recTopicName.hpStr = NULL;
            lpfulo->recFileName.hpStr = NULL;
            lpfulo->recText.hpStr = NULL;
            }
         return( (LRESULT)lpfulo );
         }

      case OBEVENT_EDIT: {
         OBINFO obinfo;

         Amob_GetObInfo( (LPOB)dwData, &obinfo );
         CreateFileUploadWindow( hwndFrame, obinfo.lpObData, (LPOB)dwData );
         return( TRUE );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpfulo->recConfName );
         Amob_SaveRefObject( &lpfulo->recTopicName );
         Amob_SaveRefObject( &lpfulo->recFileName );
         Amob_SaveRefObject( &lpfulo->recText );
         return( Amob_SaveDataObject( fh, lpfulo, sizeof( FULOBJECT ) ) );

      case OBEVENT_COPY: {
         FULOBJECT FAR * lpfuloNew;

         INITIALISE_PTR( lpfuloNew );
         lpfulo = (FULOBJECT FAR *)dwData;
         if( fNewMemory( &lpfuloNew, sizeof( FULOBJECT ) ) )
            {
            INITIALISE_PTR( lpfuloNew->recConfName.hpStr );
            INITIALISE_PTR( lpfuloNew->recTopicName.hpStr );
            INITIALISE_PTR( lpfuloNew->recFileName.hpStr );
            INITIALISE_PTR( lpfuloNew->recText.hpStr );
            Amob_CopyRefObject( &lpfuloNew->recConfName, &lpfulo->recConfName );
            Amob_CopyRefObject( &lpfuloNew->recTopicName, &lpfulo->recTopicName );
            Amob_CopyRefObject( &lpfuloNew->recFileName, &lpfulo->recFileName );
            Amob_CopyRefObject( &lpfuloNew->recText, &lpfulo->recText );
            }
         return( (LRESULT)lpfuloNew );
         }

      case OBEVENT_FIND: {
         FULOBJECT FAR * lpfulo1;
         FULOBJECT FAR * lpfulo2;

         lpfulo1 = (FULOBJECT FAR *)dwData;
         lpfulo2 = (FULOBJECT FAR *)lpBuf;
         return( TRUE );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpfulo );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpfulo->recConfName );
         Amob_FreeRefObject( &lpfulo->recTopicName );
         Amob_FreeRefObject( &lpfulo->recFileName );
         Amob_FreeRefObject( &lpfulo->recText );
         return( TRUE );
      }
   return( 0L );
}
