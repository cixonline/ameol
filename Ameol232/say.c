/* SAY.C - Handles CIX say messages
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
#include "editmap.h"
#include "webapi.h"

#ifndef USEBIGEDIT
#include "scintilla.h"
#include "scilexer.h"
#endif USEBIGEDIT

#define  THIS_FILE   __FILE__

extern BOOL fWordWrap;                 /* TRUE if we word wrap messages */

char NEAR szSayWndClass[] = "amctl_saywndcls";
char NEAR szSayWndName[] = "Say";

static BOOL fDefDlgEx = FALSE;

static BOOL fRegistered = FALSE;       /* TRUE if we've registered the say window */
static WNDPROC lpEditCtrlProc = NULL;  /* Edit control window procedure address */
static BOOL fNewMsg;                   /* TRUE if we're editing a new message */

typedef struct tagSAYWNDINFO {
   LPOB lpob;
   HWND hwndFocus;
   char szSigFile[ 16 ];
} SAYWNDINFO, FAR * LPSAYWNDINFO;

#define  GetSayWndInfo(h)     (LPSAYWNDINFO)GetWindowLong((h),DWL_USER)
#define  SetSayWndInfo(h,p)   SetWindowLong((h),DWL_USER,(long)(p))

BOOL FASTCALL SayWnd_OnCreate( HWND, LPCREATESTRUCT );
void FASTCALL SayWnd_OnClose( HWND );
void FASTCALL SayWnd_OnSize( HWND, UINT, int, int );
void FASTCALL SayWnd_OnMove( HWND, int, int );
void FASTCALL SayWnd_MDIActivate( HWND, BOOL, HWND, HWND );
void FASTCALL SayWnd_OnCommand( HWND, int, HWND, UINT );
void FASTCALL SayWnd_OnSetFocus( HWND, HWND );
void FASTCALL SayWnd_OnAdjustWindows( HWND, int, int );
LRESULT FASTCALL SayWnd_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr );

LRESULT EXPORT CALLBACK SayWndProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK SayEditProc( HWND, UINT, WPARAM, LPARAM );

typedef struct tagAUTOSAVEHEADER {
   DWORD dwSize;              /* Size of header, for version control */
   DWORD dwDataSize;          /* Size of data field */
   DWORD dwType;              /* Type of message saved */
} AUTOSAVEHEADER;

LPSTR FASTCALL AsvReadString( LPSTR * );
BOOL FASTCALL IsNoSayFolder( PCL, PTL );

/* This function posts a new CIX say message.
 */
void FASTCALL CmdSay( HWND hwnd )
{
   SAYOBJECT ao;
   CURMSG curmsg;

   Amob_New( OBTYPE_SAYMESSAGE, &ao );
   Ameol2_GetCurrentMsg( &curmsg );
   if( NULL != curmsg.ptl )
      if( Amdb_GetTopicType( curmsg.ptl ) == FTYPE_CIX )
         {
         TOPICINFO topicinfo;
         BOOL fReadOnly;
         PCL pcl;

         Amdb_GetTopicInfo( curmsg.ptl, &topicinfo );
         pcl = Amdb_FolderFromTopic( curmsg.ptl );
         fReadOnly = ( topicinfo.dwFlags & TF_READONLY ) && !Amdb_IsModerator( pcl, szCIXNickname );
         if( !fReadOnly )
            {
            if( IsNoSayFolder( pcl, curmsg.ptl ) && !Amdb_IsModerator( pcl, szCIXNickname ) )
               {
               int r;

               r = fDlgMessageBox( hwnd, idsREPORTPROMPT, IDDLG_REPORTPROMPT, NULL, 0L, 0 );
               if( IDD_REPORT == r )
                  {
                  ReportWindow( hwnd );
                  return;
                  }
               if( IDCANCEL == r )
                  return;
               }

            Amob_CreateRefString( &ao.recConfName, (LPSTR)Amdb_GetFolderName( curmsg.pcl ) );
            Amob_CreateRefString( &ao.recTopicName, (LPSTR)Amdb_GetTopicName( curmsg.ptl ) );
            CreateSayWindow( hwndFrame, &ao, NULL );
            }
         else
            fMessageBox( hwnd, 0, GS(IDS_STR1241), MB_OK|MB_ICONEXCLAMATION);
         }
   Amob_Delete( OBTYPE_SAYMESSAGE, &ao );
}

/* This function creates the Say editor window.
 */
BOOL FASTCALL CreateSayWindow( HWND hwnd, SAYOBJECT FAR * lpsoInit, LPOB lpob )
{
   LPSAYWNDINFO lpswi;
   HWND hwndSay;
   HWND hwndFocus;
   DWORD dwState;
   int cxBorder;
   int cyBorder;
   RECT rc;

   /* Look for existing window
    */
   if( NULL != lpob )
      if( hwndSay = Amob_GetEditWindow( lpob ) )
         {
         SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndSay, 0L );
         return( TRUE );
         }

   /* Register the group list window class if we have
    * not already done so.
    */
   if( !fRegistered )
      {
      WNDCLASS wc;

      wc.style          = CS_HREDRAW | CS_VREDRAW;
      wc.lpfnWndProc    = SayWndProc;
      wc.hIcon          = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_SAY) );
      wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
      wc.lpszMenuName   = NULL;
      wc.cbWndExtra     = DLGWINDOWEXTRA;
      wc.cbClsExtra     = 0;
      wc.hbrBackground  = (HBRUSH)( COLOR_APPWORKSPACE + 1 );
      wc.lpszClassName  = szSayWndClass;
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
   ReadProperWindowState( szSayWndName, &rc, &dwState );

   /* If lpob is NULL, this is a new msg.
    */
   fNewMsg = lpob == NULL;

   /* Create the window.
    */
   hwndSay = Adm_CreateMDIWindow( szSayWndName, szSayWndClass, hInst, &rc, dwState, (LPARAM)lpsoInit );
   if( NULL == hwndSay )
      return( FALSE );

   /* Store the handle of the destination structure.
    */
   lpswi = GetSayWndInfo( hwndSay );
   lpswi->lpob = lpob;

   /* Show the window.
    */
   SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndSay, 0L );

   /* Determine where we put the focus.
    */
   hwndFocus = GetDlgItem( hwndSay, IDD_SUBJECT );
   SetFocus( hwndFocus );
   lpswi->hwndFocus = hwndFocus;
   if( NULL != lpob )
      Amob_SetEditWindow( lpob, hwndSay );
   return( TRUE );
}

/* This is the main window procedure.
 */
LRESULT EXPORT CALLBACK SayWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, SayWnd_OnCreate );
      HANDLE_MSG( hwnd, WM_CLOSE, SayWnd_OnClose );
      HANDLE_MSG( hwnd, WM_SIZE, SayWnd_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, SayWnd_OnMove );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, Common_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, SayWnd_MDIActivate );
      HANDLE_MSG( hwnd, WM_COMMAND, SayWnd_OnCommand );
      HANDLE_MSG( hwnd, WM_MENUSELECT, MainWnd_OnMenuSelect );
      HANDLE_MSG( hwnd, WM_SETFOCUS, SayWnd_OnSetFocus );
      HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, SayWnd_OnAdjustWindows );
      HANDLE_MSG( hwnd, WM_CHANGEFONT, Common_ChangeFont );
      HANDLE_MSG( hwnd, WM_NOTIFY, SayWnd_OnNotify );

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
         lpMinMaxInfo->ptMinTrackSize.x = 410;
         lpMinMaxInfo->ptMinTrackSize.y = 220;
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
void FASTCALL SayWnd_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   LPSAYWNDINFO lpswi;
   HWND hwndFocus;

   lpswi = GetSayWndInfo( hwnd );
   hwndFocus = lpswi->hwndFocus;
   if( hwndFocus )
      SetFocus( hwndFocus );
   iActiveMode = WIN_EDITS;
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL SayWnd_OnCreate( HWND hwnd, LPCREATESTRUCT lpCreateStruct )
{
   LPMDICREATESTRUCT lpMDICreateStruct;
   LPSAYWNDINFO lpswi;
   SAYOBJECT FAR * lpso;
   HWND hwndList;
   HWND hwndEdit;
   PTL ptl;

   INITIALISE_PTR(lpswi);

   /* Create the dialog window.
    */
   lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
   lpso = (SAYOBJECT FAR *)lpMDICreateStruct->lParam;
   Adm_MDIDialog( hwnd, MAKEINTRESOURCE(IDDLG_SAYEDITOR), lpMDICreateStruct );

   /* Set the edit window font.
    */
#ifdef USEBIGEDIT
   SetWindowFont( GetDlgItem( hwnd, IDD_EDIT ), hEditFont, FALSE );
#else USEBIGEDIT
   SetEditorDefaults(GetDlgItem( hwnd, IDD_EDIT ), FALSE, lfEditFont, fWordWrap, fHotLinks, fMsgStyles, crEditText, crEditWnd );
#endif USEBIGEDIT

   /* Create a structure to store the mail window info.
    */
   if( !fNewMemory( &lpswi, sizeof(SAYWNDINFO) ) )
      return( FALSE );

   SetSayWndInfo( hwnd, lpswi );
   lpswi->hwndFocus = NULL;

   /* Fill the list of signatures and set the
    * first one to (None).
    */
   FillSignatureList( hwnd, IDD_LIST );

   /* Select the Conference field.
    */
   FillConfList( hwnd, DRF(lpso->recConfName) );
   ptl = FillTopicList( hwnd, DRF(lpso->recConfName), DRF(lpso->recTopicName) );

   /* Select the signature.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   CommonSetSignature( hwndList, ptl );
   ComboBox_GetText( hwndList, lpswi->szSigFile, sizeof(lpswi->szSigFile) );

   /* Fill the Subject field.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_SUBJECT );
   Edit_SetText( hwndEdit, DRF(lpso->recSubject) );
   Edit_LimitText( hwndEdit, iMaxSubjectSize );

   /* Disable the OK button if the Subject field is blank.
    */
   EnableControl( hwnd, IDOK, lpso->recSubject.dwSize > 0 );

   /* Subclass the edit control so we can do indents.
    */
   lpEditCtrlProc = SubclassWindow( GetDlgItem( hwnd, IDD_EDIT ), SayEditProc );

   /* Setup the WordWrap function !!SM!!
   */
#ifdef SMWORDBREAK
   SendDlgItemMessage(hwnd, IDD_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)(EDITWORDBREAKPROC)WordBreakProc) ;
#endif SMWORDBREAK

   DragAcceptFiles( ( GetDlgItem( hwnd, IDD_EDIT ) ), TRUE );

   /* Fill the article edit control.
    */
   SetEditText( hwnd, ptl, IDD_EDIT, DRF(lpso->recText), fNewMsg );
#ifndef USEBIGEDIT
   SendMessage( GetDlgItem( hwnd, IDD_EDIT ), SCI_SETSAVEPOINT,  (WPARAM)0, (LPARAM)0L ); 
#endif USEBIGEDIT
   Amob_FreeRefObject( &lpso->recText );
   return( TRUE );
}

/* This function handles indents in the edit window.
 */
LRESULT EXPORT CALLBACK SayEditProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   return( EditCtrlIndent( lpEditCtrlProc, hwnd, msg, wParam, lParam ) );
}

LRESULT FASTCALL SayWnd_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
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
void FASTCALL SayWnd_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSAYEDITOR );
         break;

      case IDD_CONFNAME:
         if( codeNotify == CBN_SELCHANGE )
            {
            char szTopicName[ LEN_TOPICNAME+1 ];
            char sz[ LEN_CONFNAME+1 ];
            HWND hwndList;
            int nSel;
            LPSAYWNDINFO lpswi;

            hwndList = GetDlgItem( hwnd, IDD_CONFNAME );
            nSel = ComboBox_GetCurSel( hwndList );
            ComboBox_GetLBText( hwndList, nSel, sz );
            GetDlgItemText( hwnd, IDD_TOPICNAME, szTopicName, LEN_TOPICNAME+1 );
            FillTopicList( hwnd, sz, szTopicName );

            /* Set the signature for the selected topic.
             */
            lpswi = GetSayWndInfo( hwnd );
            CommonTopicSigChangeCode( hwnd, lpswi->szSigFile );
            }
         break;

      case IDD_TOPICNAME:
         if( codeNotify == CBN_SELCHANGE )
            {
            LPSAYWNDINFO lpswi;

            /* The topic has changed, so change the signature
             * in the message.
             */
            lpswi = GetSayWndInfo( hwnd );
            CommonTopicSigChangeCode( hwnd, lpswi->szSigFile );
            }
         break;

      case IDM_QUICKPRINT:
      case IDM_PRINT:
         CmdComposedPrint( hwnd );
         break;

      case IDD_SUBJECT:
         if( codeNotify == EN_CHANGE )
            EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndCtl ) > 0 );

      case IDD_EDIT:
         if( codeNotify == EN_SETFOCUS )
            {
            LPSAYWNDINFO lpswi;

            lpswi = GetSayWndInfo( hwnd );
            lpswi->hwndFocus = hwndCtl;
            }
         else if( codeNotify == EN_ERRSPACE || codeNotify == EN_MAXTEXT )
            fMessageBox( hwnd, 0, GS(IDS_STR77), MB_OK|MB_ICONEXCLAMATION);
         break;

      case IDD_LIST:
         if( codeNotify == CBN_SELCHANGE )
            {
            LPSAYWNDINFO lpswi;
   
            lpswi = GetSayWndInfo( hwnd );
            CommonSigChangeCode( hwnd, lpswi->szSigFile, FALSE, FALSE );
            }
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

      case IDOK: {
         char szTopicName[ LEN_TOPICNAME+1 ];
         char szConfName[ LEN_CONFNAME+1 ];
         TOPICINFO topicinfo;
         LPSAYWNDINFO lpswi;
         SAYOBJECT ao;
         HWND hwndEdit;
         HWND hwndSay;
         int length;
         PCL pcl;
         PTL ptl;
#ifndef USEBIGEDIT
         HEDIT hedit;
#endif USEBIGEDIT

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

         /* Dereference the SAYOBJECT data structure.
          */
         lpswi = GetSayWndInfo( hwnd );
         Amob_New( OBTYPE_SAYMESSAGE, &ao );

         /* Check that the topic to which we're writing this message is not
          * read-only.
          */
         GetDlgItemText( hwnd, IDD_CONFNAME, szConfName, LEN_CONFNAME+1 );
         GetDlgItemText( hwnd, IDD_TOPICNAME, szTopicName, LEN_TOPICNAME+1 );
         pcl = Amdb_OpenFolder( NULL, szConfName );
         if( NULL == pcl )
         {
            fMessageBox( hwnd, 0, "This folder does not exist in your database", MB_OK|MB_ICONEXCLAMATION);
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

         /* Okay to save say, so update in out-basket.
          */
         Amob_CreateRefString( &ao.recTopicName, szTopicName );
         Amob_CreateRefString( &ao.recConfName, szConfName );
         Amob_SaveRefObject( &ao.recTopicName );
         Amob_SaveRefObject( &ao.recConfName );

         /* Get and update the Subject field.
          */
         length = Edit_GetTextLength( hwndSay );
         if( Amob_CreateRefObject( &ao.recSubject, length + 1 ) )
            {
            Edit_GetText( hwndSay, ao.recSubject.hpStr, length + 1 );
            Amob_SaveRefObject( &ao.recSubject );
            }

         /* Get and update the Text field.
          */
#ifdef USEBIGEDIT
         Edit_FmtLines( hwndEdit, TRUE );
#else USEBIGEDIT
         hedit = Editmap_Open( hwndEdit );
         Editmap_FormatLines( hedit, hwndEdit);
#endif USEBIGEDIT
         length = Edit_GetTextLength( hwndEdit );
         if( Amob_CreateRefObject( &ao.recText, length + 1 ) )
            {
            Edit_GetText( hwndEdit, ao.recText.hpStr, length + 1 );
            Amob_SaveRefObject( &ao.recText );
            }
         Amob_Commit( lpswi->lpob, OBTYPE_SAYMESSAGE, &ao );
         Adm_DestroyMDIWindow( hwnd );
         break;
         }

      case IDCANCEL:
         /* Destroy the article window.
          */
         SayWnd_OnClose( hwnd );
         break;
      }
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL SayWnd_OnAdjustWindows( HWND hwnd, int dx, int dy )
{
   Adm_MoveWnd( GetDlgItem( hwnd, IDOK ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDCANCEL ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_HELP ), dx, 0 );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_SUBJECT ), dx, 0 );
// Adm_InflateWnd( GetDlgItem( hwnd, IDD_CONFNAME ), dx, 0 );
// Adm_InflateWnd( GetDlgItem( hwnd, IDD_TOPICNAME ), dx, 0 );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_EDIT ), dx, dy );
   SetEditWindowWidth( GetDlgItem( hwnd, IDD_EDIT ), iWrapCol );
}

/* This function handles the WM_CLOSE message.
 */
void FASTCALL SayWnd_OnClose( HWND hwnd )
{
   LPSAYWNDINFO lpswi;
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
   lpswi = GetSayWndInfo( hwnd );
   if( NULL != lpswi->lpob )
      Amob_Uncommit( lpswi->lpob );
   Adm_DestroyMDIWindow( hwnd );
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL SayWnd_OnMove( HWND hwnd, int x, int y )
{
   Amuser_WriteWindowState( szSayWndName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL SayWnd_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   /* Save the new window state to the configuration file.
    */
   Amuser_WriteWindowState( szSayWndName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* This function handles the WM_MDIACTIVATE message.
 */
void FASTCALL SayWnd_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_EDITS ), (LPARAM)(LPSTR)hwnd );
   if( fActive )
      ToolbarState_EditWindow();
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, Adm_DefMDIDlgProc );
}

/* This is the Say out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_SayMessage( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   SAYOBJECT FAR * lpso;

   lpso = (SAYOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_EDITABLE:
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_EXEC:
         if (fUseWebServices) // !!SM!! 2.56.2054
         { 
            return (Am2Post(szCIXNickname, szCIXPassword, DRF(lpso->recConfName), DRF(lpso->recTopicName), DRF(lpso->recText), -1));
         }
         else
            return( POF_HELDBYSCRIPT );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR834), DRF(lpso->recConfName), DRF(lpso->recTopicName), DRF(lpso->recSubject) );
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( 0 );

      case OBEVENT_NEW:
         memset( lpso, 0, sizeof( SAYOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         SAYOBJECT mo;
      
         INITIALISE_PTR(lpso);
         Amob_LoadDataObject( fh, &mo, sizeof( SAYOBJECT ) );
         if( fNewMemory( &lpso, sizeof( SAYOBJECT ) ) )
            {
            mo.recConfName.hpStr = NULL;
            mo.recTopicName.hpStr = NULL;
            mo.recSubject.hpStr = NULL;
            mo.recText.hpStr = NULL;
            *lpso = mo;
            }
         return( (LRESULT)lpso );
         }

      case OBEVENT_EDIT: {
         OBINFO obinfo;

         Amob_GetObInfo( (LPOB)dwData, &obinfo );
         CreateSayWindow( hwndFrame, obinfo.lpObData, (LPOB)dwData );
         return( TRUE );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpso->recConfName );
         Amob_SaveRefObject( &lpso->recTopicName );
         Amob_SaveRefObject( &lpso->recSubject );
         Amob_SaveRefObject( &lpso->recText );
         return( Amob_SaveDataObject( fh, lpso, sizeof( SAYOBJECT ) ) );

      case OBEVENT_COPY: {
         SAYOBJECT FAR * lpsoNew;

         INITIALISE_PTR( lpsoNew );
         lpso = (SAYOBJECT FAR *)dwData;
         if( fNewMemory( &lpsoNew, sizeof( SAYOBJECT ) ) )
            {
            INITIALISE_PTR( lpsoNew->recConfName.hpStr );
            INITIALISE_PTR( lpsoNew->recTopicName.hpStr );
            INITIALISE_PTR( lpsoNew->recSubject.hpStr );
            INITIALISE_PTR( lpsoNew->recText.hpStr );
            Amob_CopyRefObject( &lpsoNew->recConfName, &lpso->recConfName );
            Amob_CopyRefObject( &lpsoNew->recTopicName, &lpso->recTopicName );
            Amob_CopyRefObject( &lpsoNew->recSubject, &lpso->recSubject );
            Amob_CopyRefObject( &lpsoNew->recText, &lpso->recText );
            }
         return( (LRESULT)lpsoNew );
         }

      case OBEVENT_FIND:
         return( FALSE );

      case OBEVENT_REMOVE:
         FreeMemory( &lpso );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpso->recConfName );
         Amob_FreeRefObject( &lpso->recTopicName );
         Amob_FreeRefObject( &lpso->recSubject );
         Amob_FreeRefObject( &lpso->recText );
         return( TRUE );
      }
   return( 0L );
}

/* This function looks for an undeleted autosave file and, if found,
 * offers to retrieve it into the appropriate edit window.
 */
void FASTCALL LocateAutosavedFiles( void )
{
   register int n;
   FINDDATA ft;
   HFIND r;
    // VistaAlert
   wsprintf( lpPathBuf, "%s*.asv", pszAmeolDir );
   for( n = r = Amuser_FindFirst( lpPathBuf, _A_NORMAL, &ft ); n != -1; n = Amuser_FindNext( r, &ft ) )
      {
      HNDFILE fh;

      /* Found one, so open the file and extract the
       * details.
       */
      wsprintf( lpPathBuf, "%s%s.asv", pszAmeolDir, ft.name );
      if( HNDFILE_ERROR != ( fh = Amfile_Open( lpPathBuf, AOF_READ ) ) )
         {
         AUTOSAVEHEADER asvHdr;
         LPSTR lpText;

         if( Amfile_Read( fh, &asvHdr, sizeof(AUTOSAVEHEADER) ) == sizeof(AUTOSAVEHEADER) )
            if( asvHdr.dwSize == sizeof(AUTOSAVEHEADER) )
               if( fNewMemory( &lpText, (WORD)asvHdr.dwDataSize ) )
                  {
                  /* Read the rest of the file into a memory buffer.
                   */
                  if( Amfile_Read( fh, lpText, (WORD)asvHdr.dwDataSize ) == asvHdr.dwDataSize )
                     {
                     LPSTR lpPtr;

                     lpPtr = lpText;
                     switch( asvHdr.dwType )
                        {
                        case OBTYPE_SAYMESSAGE: {
                           SAYOBJECT so;

                           Amob_New( OBTYPE_SAYMESSAGE, &so );
                           Amob_CreateRefString( &so.recConfName, AsvReadString( &lpPtr ) );
                           Amob_CreateRefString( &so.recTopicName, AsvReadString( &lpPtr ) );
                           Amob_CreateRefString( &so.recSubject, AsvReadString( &lpPtr ) );
                           Amob_CreateRefString( &so.recText, AsvReadString( &lpPtr ) );
                           CreateSayWindow( hwndFrame, &so, NULL );
                           Amob_Delete( OBTYPE_SAYMESSAGE, &so );
                           break;
                           }
                        }
                     }
                  FreeMemory( &lpText );
                  }
         Amfile_Close( fh );
         }
      Amfile_Delete( lpPathBuf );
      }
   Amuser_FindClose( r );
}

/* This function reads a string from a memory buffer
 * and advances the pointer to the next string.
 */
LPSTR FASTCALL AsvReadString( LPSTR * plpPtr )
{
   LPSTR lpPtrNext;
   LPSTR lpPtr;

   lpPtr = *plpPtr;
   lpPtrNext = lpPtr;
   while( *lpPtrNext++ );
   *plpPtr = lpPtrNext;
   return( lpPtr );
}

/* Returns TRUE if pcl is a folder in which report should be used.
 */
BOOL FASTCALL IsNoSayFolder( PCL pcl, PTL ptl )
{
   LPCSTR lpFolderName;             /* Folder name */
   LPCSTR lpTopicName;
   
   lpFolderName = Amdb_GetFolderName( pcl );
   lpTopicName = Amdb_GetTopicName( ptl );

   if( strcmp( lpFolderName, "cix.beta" ) == 0 )
   {
      if( NULL != strstr( lpTopicName, "ameol" ) )
            return( TRUE );
   }
   else if( strcmp( lpFolderName, "ameol_libdems" ) == 0 )
   {
      if( ( NULL != strstr( lpTopicName, "addons" ) ) || ( NULL != strstr( lpTopicName, "queries" ) ) || ( NULL != strstr( lpTopicName, "bugs" ) )
         || ( NULL != strstr( lpTopicName, "wishlist" ) ) )
            return( TRUE );
   }
   else if( ( strcmp( lpFolderName, "cix.support" ) == 0 ) || ( strcmp( lpFolderName, "ld.cix.support" ) == 0 ) )
      return( TRUE );
   return( FALSE );
}
