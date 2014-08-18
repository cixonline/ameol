/* LOCALCMT.C - Handles Local comment messages
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
#include "local.h"
#include <io.h>
#include "ameol2.h"
#include "tti.h"
#include "parse.h"
#include "rules.h"
#include "admin.h"
#include "editmap.h"

#ifndef USEBIGEDIT
#include "scintilla.h"
#include "scilexer.h"
#endif USEBIGEDIT

#define  THIS_FILE   __FILE__

extern BOOL fWordWrap;                 /* TRUE if we word wrap messages */

char szLocalCmtWndClass[] = "amctl_localcmtwndcls";
char szLocalCmtWndName[] = "Comment";

static BOOL fDefDlgEx = FALSE;

static BOOL fRegistered = FALSE;       /* TRUE if we've registered the comment window */
static WNDPROC lpEditCtrlProc = NULL;  /* Edit control window procedure address */

typedef struct tagLOCALCMTWNDINFO {
   HWND hwndFocus;
   char szSigFile[ 16 ];
} LOCALCMTWNDINFO, FAR * LPLOCALCMTWNDINFO;

#define  GetLocalCmtWndInfo(h)      (LPLOCALCMTWNDINFO)GetWindowLong((h),DWL_USER)
#define  SetLocalCmtWndInfo(h,p) SetWindowLong((h),DWL_USER,(long)(p))

/* Array of controls that have tooltips and their string
 * resource IDs.
 */
static TOOLTIPITEMS tti[] = { IDD_ORIGINAL, IDS_STR1017 };

BOOL FASTCALL LocalCmtWnd_OnCreate( HWND, LPCREATESTRUCT );
void FASTCALL LocalCmtWnd_OnClose( HWND );
void FASTCALL LocalCmtWnd_OnSize( HWND, UINT, int, int );
void FASTCALL LocalCmtWnd_OnMove( HWND, int, int );
void FASTCALL LocalCmtWnd_MDIActivate( HWND, BOOL, HWND, HWND );
void FASTCALL LocalCmtWnd_OnCommand( HWND, int, HWND, UINT );
void FASTCALL LocalCmtWnd_OnSetFocus( HWND, HWND );
void FASTCALL LocalCmtWnd_OnAdjustWindows( HWND, int, int );
LRESULT FASTCALL LocalCmtWnd_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr );

LRESULT EXPORT CALLBACK LocalCmtWndProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK LocalCmtEditProc( HWND, UINT, WPARAM, LPARAM );

PTH FASTCALL LocalSayCommentStore( HEADER *, HPSTR, DWORD );

/* This function posts a new CIX comment message.
 */
void FASTCALL CmdLocalCmt( HWND hwnd )
{
   if( hwndTopic )
      {
      LPWINDINFO lpWindInfo;

      lpWindInfo = GetBlock( hwndTopic );
      if( NULL != lpWindInfo->lpMessage )
         {
         LOCALCMTOBJECT lco;
         MSGINFO msginfo;
         PCL pcl;

         Amob_New( OBTYPE_LOCALCMTMESSAGE, &lco );
         pcl = Amdb_FolderFromTopic( lpWindInfo->lpTopic );
         Amob_CreateRefString( &lco.recConfName, (LPSTR)Amdb_GetFolderName( pcl ) );
         Amob_CreateRefString( &lco.recTopicName, (LPSTR)Amdb_GetTopicName( lpWindInfo->lpTopic ) );
         Amdb_GetMsgInfo( lpWindInfo->lpMessage, &msginfo );
         lco.dwReply = msginfo.dwMsg;
         CreateLocalCmtWindow( hwndFrame, &lco );
         Amob_Delete( OBTYPE_LOCALCMTMESSAGE, &lco );
         }
      }
}

/* This function creates the LocalCmt editor window.
 */
BOOL FASTCALL CreateLocalCmtWindow( HWND hwnd, LOCALCMTOBJECT FAR * lplcoInit )
{
   LPLOCALCMTWNDINFO lpmwi;
   HWND hwndLocalCmt;
   HWND hwndFocus;
   DWORD dwState;
   RECT rc;

   /* Register the group list window class if we have
    * not already done so.
    */
   if( !fRegistered )
      {
      WNDCLASS wc;

      wc.style          = CS_HREDRAW | CS_VREDRAW;
      wc.lpfnWndProc    = LocalCmtWndProc;
      wc.hIcon          = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_LOCALCMT) );
      wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
      wc.lpszMenuName   = NULL;
      wc.cbWndExtra     = DLGWINDOWEXTRA;
      wc.cbClsExtra     = 0;
      wc.hbrBackground  = (HBRUSH)( COLOR_APPWORKSPACE + 1 );
      wc.lpszClassName  = szLocalCmtWndClass;
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
   InflateRect( &rc, -5, -5 );
   dwState = 0;

   /* Get the actual window size, position and state.
    */
   ReadProperWindowState( szLocalCmtWndName, &rc, &dwState );

   /* Create the window.
    */
   hwndLocalCmt = Adm_CreateMDIWindow( szLocalCmtWndName, szLocalCmtWndClass, hInst, &rc, dwState, (LPARAM)lplcoInit );
   if( NULL == hwndLocalCmt )
      return( FALSE );

   /* Store the handle of the destination structure.
    */
   lpmwi = GetLocalCmtWndInfo( hwndLocalCmt );

   /* Show the window.
    */
   SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndLocalCmt, 0L );

   /* Determine where we put the focus.
    */
   hwndFocus = GetDlgItem( hwndLocalCmt, IDD_EDIT );
   SetFocus( hwndFocus );
   lpmwi->hwndFocus = hwndFocus;
   return( TRUE );
}

/* This is the main window procedure.
 */
LRESULT EXPORT CALLBACK LocalCmtWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, LocalCmtWnd_OnCreate );
      HANDLE_MSG( hwnd, WM_CLOSE, LocalCmtWnd_OnClose );
      HANDLE_MSG( hwnd, WM_SIZE, LocalCmtWnd_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, LocalCmtWnd_OnMove );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, Common_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, LocalCmtWnd_MDIActivate );
      HANDLE_MSG( hwnd, WM_COMMAND, LocalCmtWnd_OnCommand );
      HANDLE_MSG( hwnd, WM_MENUSELECT, MainWnd_OnMenuSelect );
      HANDLE_MSG( hwnd, WM_SETFOCUS, LocalCmtWnd_OnSetFocus );
      HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, LocalCmtWnd_OnAdjustWindows );
      HANDLE_MSG( hwnd, WM_CHANGEFONT, Common_ChangeFont );
      HANDLE_MSG( hwnd, WM_NOTIFY, LocalCmtWnd_OnNotify );
      case WM_APPCOLOURCHANGE:
      case WM_TOGGLEHEADERS: {  // 2.25.2031
#ifdef USEBIGEDIT
         SetWindowFont( GetDlgItem( hwnd, IDD_EDIT ), hEditFont, FALSE );
#else USEBIGEDIT
         SetEditorDefaults(GetDlgItem( hwnd, IDD_EDIT ), FALSE, lfEditFont, fWordWrap, fHotLinks , fMsgStyles, crEditText, crEditWnd );
#endif USEBIGEDIT
         break;
         }

      default:
         break;
      }
   return( Adm_DefMDIDlgProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_SETFOCUS message.
 */
void FASTCALL LocalCmtWnd_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   LPLOCALCMTWNDINFO lpmwi;
   HWND hwndFocus;

   lpmwi = GetLocalCmtWndInfo( hwnd );
   hwndFocus = lpmwi->hwndFocus;
   if( hwndFocus )
      SetFocus( hwndFocus );
   iActiveMode = WIN_EDITS;
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL LocalCmtWnd_OnCreate( HWND hwnd, LPCREATESTRUCT lpCreateStruct )
{
   LPMDICREATESTRUCT lpMDICreateStruct;
   LPLOCALCMTWNDINFO lpmwi;
   LOCALCMTOBJECT FAR * lplco;
   HWND hwndList;
   PTL ptl;

   INITIALISE_PTR(lpmwi);

   /* Create the dialog window.
    */
   lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
   lplco = (LOCALCMTOBJECT FAR *)lpMDICreateStruct->lParam;
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
   if( !fNewMemory( &lpmwi, sizeof(LOCALCMTWNDINFO) ) )
      return( FALSE );
   SetLocalCmtWndInfo( hwnd, lpmwi );

   /* Set the title.
    */
   wsprintf( lpTmpBuf, GS(IDS_STR27), lplco->dwReply, DRF(lplco->recConfName), DRF(lplco->recTopicName) );
   SetWindowText( hwnd, lpTmpBuf );

   /* Fill the list of signatures and set the
    * first one to (None).
    */
   FillSignatureList( hwnd, IDD_LIST );

   /* Select the Conference field.
    */
   FillLocalConfList( hwnd, DRF(lplco->recConfName) );
   ptl = FillLocalTopicList( hwnd, DRF(lplco->recConfName), DRF(lplco->recTopicName) );

   /* Select the signature.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   CommonSetSignature( hwndList, ptl );
   ComboBox_GetText( hwndList, lpmwi->szSigFile, sizeof(lpmwi->szSigFile) );

   /* Set the parameter picker button.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_ORIGINAL ), hInst, IDB_ORIGINALCOMMENT );

   /* Fill the reply number field.
    */
   SetDlgItemLongInt( hwnd, IDD_COMMENT, lplco->dwReply );

   /* Subclass the edit control so we can do indents.
    */
   lpEditCtrlProc = SubclassWindow( GetDlgItem( hwnd, IDD_EDIT ), LocalCmtEditProc );

   /* Setup the WordWrap function !!SM!!
   */
#ifdef SMWORDBREAK
   SendDlgItemMessage(hwnd, IDD_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)(EDITWORDBREAKPROC)WordBreakProc) ;
#endif SMWORDBREAK

   /* Fill the article edit control.
    */
   SetEditText( hwnd, ptl, IDD_EDIT, DRF(lplco->recText), TRUE );
#ifndef USEBIGEDIT
   SendMessage( GetDlgItem( hwnd, IDD_EDIT ), SCI_SETSAVEPOINT,  (WPARAM)0, (LPARAM)0L ); 
#endif USEBIGEDIT
   Amob_FreeRefObject( &lplco->recText );

   /* Add them tooltips.
    */
   AddTooltipsToWindow( hwnd, 1, (LPTOOLTIPITEMS)&tti );
   
   return( TRUE );
}

/* This function handles indents in the edit window.
 */
LRESULT EXPORT CALLBACK LocalCmtEditProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   return( EditCtrlIndent( lpEditCtrlProc, hwnd, msg, wParam, lParam ) );
}

LRESULT FASTCALL LocalCmtWnd_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
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
void FASTCALL LocalCmtWnd_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   CheckMenuStates( ); // !!SM!! 2.55
   switch( id )
      {
      case IDD_ORIGINAL: {
         DWORD dwReply;

         /* Get the comment number.
          */
         if( GetDlgLongInt( hwnd, IDD_COMMENT, &dwReply, 1, 99999999 ) )
            {
            char szTopicName[ LEN_TOPICNAME+1 ];
            char szConfName[ 100 ];
            CURMSG unr;

            /* Get the topic to which this message refers.
             */
            GetDlgItemText( hwnd, IDD_CONFNAME, szConfName, 100 );
            GetDlgItemText( hwnd, IDD_TOPICNAME, szTopicName, LEN_TOPICNAME+1 );
            unr.pcl = Amdb_OpenFolder( NULL, szConfName );
            unr.ptl = Amdb_OpenTopic( unr.pcl, szTopicName );
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
            char sz[ 14 ];
            HWND hwndList;
            int nSel;

            hwndList = GetDlgItem( hwnd, IDD_CONFNAME );
            nSel = ComboBox_GetCurSel( hwndList );
            ComboBox_GetLBText( hwndList, nSel, sz );
            GetDlgItemText( hwnd, IDD_TOPICNAME, szTopicName, LEN_TOPICNAME+1 );
            FillLocalTopicList( hwnd, sz, szTopicName );
            }
         break;

      case IDM_QUICKPRINT:
      case IDM_PRINT:
         CmdComposedPrint( hwnd );
         break;

      case IDD_COMMENT:
         if( codeNotify == EN_CHANGE )
            EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndCtl ) > 0 );

      case IDD_LIST:
         if( codeNotify == CBN_SELCHANGE )
            {
            LPLOCALCMTWNDINFO lpmwi;

            lpmwi = GetLocalCmtWndInfo( hwnd );
            CommonSigChangeCode( hwnd, lpmwi->szSigFile, FALSE, FALSE );
            }
         break;

      case IDD_EDIT:
         if( codeNotify == EN_SETFOCUS )
            {
            LPLOCALCMTWNDINFO lpmwi;

            lpmwi = GetLocalCmtWndInfo( hwnd );
            lpmwi->hwndFocus = hwndCtl;
            }
         else if( codeNotify == EN_ERRSPACE || codeNotify == EN_MAXTEXT )
            fMessageBox( hwnd, 0, GS(IDS_STR77), MB_OK|MB_ICONEXCLAMATION);
         break;

      case IDM_SPELLCHECK:
         Ameol2_SpellCheckDocument( hwnd, GetDlgItem( hwnd, IDD_EDIT ), SC_FL_DIALOG );
         break;

      case IDOK: {
         char szTopicName[ LEN_TOPICNAME+1 ];
         char szConfName[ 100 ];
         LPLOCALCMTWNDINFO lpmwi;
         TOPICINFO topicinfo;
         LOCALCMTOBJECT lco;
         HWND hwndEdit;
         DWORD dwReply;
         AM_DATE date;
         AM_TIME time;
         int cbText;
         int cbHdr;
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

         /* Dereference the LOCALCMTOBJECT data structure.
          */
         lpmwi = GetLocalCmtWndInfo( hwnd );
         Amob_New( OBTYPE_LOCALCMTMESSAGE, &lco );

         /* Get the comment number.
          */
         if( !GetDlgLongInt( hwnd, IDD_COMMENT, &dwReply, 1, 99999999 ) )
            break;
         if( dwReply == 0 )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR436), MB_OK|MB_ICONEXCLAMATION );
            break;
            }
         lco.dwReply = dwReply;

         /* Get the topic name.
          */
         GetDlgItemText( hwnd, IDD_CONFNAME, szConfName, 100 );
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
         /* Get the topic name.
          */
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
         Amob_CreateRefString( &lco.recConfName, szConfName );
         Amob_CreateRefString( &lco.recTopicName, szTopicName );
         Amob_SaveRefObject( &lco.recConfName );
         Amob_SaveRefObject( &lco.recTopicName );

         /* Get and update the Text field.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
#ifdef USEBIGEDIT
         Edit_FmtLines( hwndEdit, TRUE );
#else USEBIGEDIT
         hedit = Editmap_Open( hwndEdit );
         Editmap_FormatLines( hedit, hwndEdit);
#endif USEBIGEDIT
         cbText = Edit_GetTextLength( hwndEdit );

         /* Create a compact header for a local topic.
          */
         Amdate_GetCurrentDate( &date );
         Amdate_GetCurrentTime( &time );
         wsprintf( lpTmpBuf, szCompactHdrTmpl,
                  szConfName,
                  szTopicName,
                  topicinfo.cMsgs + 1,
                  GetActiveUsername(),
                  cbText + 2,
                  date.iDay,
                  (LPSTR)pszCixMonths[ date.iMonth - 1 ],
                  date.iYear % 100,
                  time.iHour,
                  time.iMinute );
         strcat( lpTmpBuf, "\r\n" );
         cbHdr = strlen(lpTmpBuf);
         if( Amob_CreateRefObject( &lco.recText, cbText + cbHdr + 1 ) )
            {
            strcpy( lco.recText.hpStr, lpTmpBuf );
            Edit_GetText( hwndEdit, lco.recText.hpStr + cbHdr, cbText + 1 );
            Amob_SaveRefObject( &lco.recText );
            }
         Amob_Commit( NULL, OBTYPE_LOCALCMTMESSAGE, &lco );
         RemoveTooltipsFromWindow( hwnd );
         Adm_DestroyMDIWindow( hwnd );
         break;
         }

      case IDCANCEL:
         /* Destroy the article window.
          */
         LocalCmtWnd_OnClose( hwnd );
         break;
      }
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL LocalCmtWnd_OnAdjustWindows( HWND hwnd, int dx, int dy )
{
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_EDIT ), dx, dy );
   SetEditWindowWidth( GetDlgItem( hwnd, IDD_EDIT ), iWrapCol );
}

/* This function handles the WM_CLOSE message.
 */
void FASTCALL LocalCmtWnd_OnClose( HWND hwnd )
{
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
   RemoveTooltipsFromWindow( hwnd );
   Adm_DestroyMDIWindow( hwnd );
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL LocalCmtWnd_OnMove( HWND hwnd, int x, int y )
{
   Amuser_WriteWindowState( szLocalCmtWndName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL LocalCmtWnd_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   /* Save the new window state to the configuration file.
    */
   Amuser_WriteWindowState( szLocalCmtWndName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* This function handles the WM_MDIACTIVATE message.
 */
void FASTCALL LocalCmtWnd_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   if( fActive )
      ToolbarState_CopyPaste();
   Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_EDITS ), (LPARAM)(LPSTR)hwnd );
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, Adm_DefMDIDlgProc );
}

/* This is the Local Comment out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_LocalCmtMessage( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   LOCALCMTOBJECT FAR * lplco;

   lplco = (LOCALCMTOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_NEW:
         memset( lplco, 0, sizeof( LOCALCMTOBJECT ) );
         return( TRUE );

      case OBEVENT_REMOVE:
         FreeMemory( &lplco );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lplco->recConfName );
         Amob_FreeRefObject( &lplco->recTopicName );
         Amob_FreeRefObject( &lplco->recText );
         return( TRUE );

      case OBEVENT_EXEC: {
         HEADER * pHeader;

         /* Initialise the header.
          */
         INITIALISE_PTR(pHeader);
         if( fNewMemory( &pHeader, sizeof(HEADER) ) )
            {
            CURMSG curmsg;
            HPSTR hpBuf;
            HPSTR hpBuf2;
            PCL pcl;
            PTH pth;

            InitialiseHeader( pHeader );
            strcpy( pHeader->szAuthor, GetActiveUsername() );
            hpBuf = DRF(lplco->recText);
            hpBuf2 = AdvanceLine( hpBuf );
            StoreTitle( pHeader->szTitle, hpBuf2 );
            pHeader->dwComment = lplco->dwReply;
   
            /* Set the postal date to the current date/time.
             */
            Amdate_GetCurrentDate( &pHeader->date );
            Amdate_GetCurrentTime( &pHeader->time );
         
            /* Derefence the structure elements.
             */
            pcl = Amdb_OpenFolder( NULL, DRF(lplco->recConfName) );
            pHeader->ptl = Amdb_OpenTopic( pcl, DRF(lplco->recTopicName) );
   
            /* Call the common local say/comment code to put the message
             * into the database.
             */
            pth = LocalSayCommentStore( pHeader, hpBuf, lplco->recText.dwSize );
         
            /* Set this new message as current.
             */
            if( NULL != pth && hwndMsg )
               {
               curmsg.pth = pth;
               curmsg.ptl = pHeader->ptl;
               curmsg.pcl = Amdb_FolderFromTopic( curmsg.ptl );
               curmsg.pcat = Amdb_CategoryFromFolder( curmsg.pcl );
               SetCurrentMsg( &curmsg, FALSE, TRUE );
               }
            }
         return( POF_PROCESSCOMPLETED );
         }
      }
   return( 0L );
}
