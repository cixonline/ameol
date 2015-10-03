/* LOCALSAY.C - Handles Say to local topics
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
#include "parse.h"
#include "rules.h"
#include "admin.h"
#include "cix.h"
#include "editmap.h"

#ifndef USEBIGEDIT
#include "scintilla.h"
#include "scilexer.h"
#endif USEBIGEDIT

#define  THIS_FILE   __FILE__

extern BOOL fWordWrap;                 /* TRUE if we word wrap messages */

char szLocalSayClass[] = "amctl_localsaywndcls";
char szLocalSayName[] = "Say";

static BOOL fDefDlgEx = FALSE;

static BOOL fRegistered = FALSE;       /* TRUE if we've registered the say window */
static WNDPROC lpEditCtrlProc = NULL;  /* Edit control window procedure address */

typedef struct tagLOCALSAYWNDINFO {
   HWND hwndFocus;
   char szSigFile[ 16 ];
} LOCALSAYWNDINFO, FAR * LPLOCALSAYWNDINFO;

#define  GetLocalSayInfo(h)      (LPLOCALSAYWNDINFO)GetWindowLong((h),DWL_USER)
#define  SetLocalSayInfo(h,p)    SetWindowLong((h),DWL_USER,(long)(p))

BOOL FASTCALL LocalSay_OnCreate( HWND, LPCREATESTRUCT );
void FASTCALL LocalSay_OnClose( HWND );
void FASTCALL LocalSay_OnSize( HWND, UINT, int, int );
void FASTCALL LocalSay_OnMove( HWND, int, int );
void FASTCALL LocalSay_MDIActivate( HWND, BOOL, HWND, HWND );
void FASTCALL LocalSay_OnCommand( HWND, int, HWND, UINT );
void FASTCALL LocalSay_OnSetFocus( HWND, HWND );
void FASTCALL LocalSay_OnAdjustWindows( HWND, int, int );
LRESULT FASTCALL LocalSay_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr );

LRESULT EXPORT CALLBACK LocalSayProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK LocalSayEditProc( HWND, UINT, WPARAM, LPARAM );

PTH FASTCALL LocalSayCommentStore( HEADER *, HPSTR, DWORD );

/* This function posts a message to a local topic
 */
void FASTCALL CmdLocalSay( HWND hwnd )
{
   LOCALSAYOBJECT lso;
   CURMSG curmsg;

   Amob_New( OBTYPE_LOCALSAYMESSAGE, &lso );
   Ameol2_GetCurrentMsg( &curmsg );
   if( NULL != curmsg.ptl )
      if( Amdb_GetTopicType( curmsg.ptl ) == FTYPE_LOCAL )
         {
         Amob_CreateRefString( &lso.recConfName, (LPSTR)Amdb_GetFolderName( curmsg.pcl ) );
         Amob_CreateRefString( &lso.recTopicName, (LPSTR)Amdb_GetTopicName( curmsg.ptl ) );
         CreateLocalSayWindow( hwndFrame, &lso );
         }
   Amob_Delete( OBTYPE_LOCALSAYMESSAGE, &lso );
}

/* This function creates the Local Say editor window.
 */
BOOL FASTCALL CreateLocalSayWindow( HWND hwnd, LOCALSAYOBJECT FAR * lplsoInit )
{
   LPLOCALSAYWNDINFO lpmwi;
   HWND hwndSay;
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
      wc.lpfnWndProc    = LocalSayProc;
      wc.hIcon          = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_LOCALSAY) );
      wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
      wc.lpszMenuName   = NULL;
      wc.cbWndExtra     = DLGWINDOWEXTRA;
      wc.cbClsExtra     = 0;
      wc.hbrBackground  = (HBRUSH)( COLOR_APPWORKSPACE + 1 );
      wc.lpszClassName  = szLocalSayClass;
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
   ReadProperWindowState( szLocalSayName, &rc, &dwState );

   /* Create the window.
    */
   hwndSay = Adm_CreateMDIWindow( szLocalSayName, szLocalSayClass, hInst, &rc, dwState, (LPARAM)lplsoInit );
   if( NULL == hwndSay )
      return( FALSE );

   /* Store the handle of the destination structure.
    */
   lpmwi = GetLocalSayInfo( hwndSay );

   /* Show the window.
    */
   SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndSay, 0L );

   /* Determine where we put the focus.
    */
   hwndFocus = GetDlgItem( hwndSay, IDD_SUBJECT );
   SetFocus( hwndFocus );
   lpmwi->hwndFocus = hwndFocus;
   return( TRUE );
}

/* This is the main window procedure.
 */
LRESULT EXPORT CALLBACK LocalSayProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, LocalSay_OnCreate );
      HANDLE_MSG( hwnd, WM_CLOSE, LocalSay_OnClose );
      HANDLE_MSG( hwnd, WM_SIZE, LocalSay_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, LocalSay_OnMove );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, Common_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, LocalSay_MDIActivate );
      HANDLE_MSG( hwnd, WM_COMMAND, LocalSay_OnCommand );
      HANDLE_MSG( hwnd, WM_MENUSELECT, MainWnd_OnMenuSelect );
      HANDLE_MSG( hwnd, WM_SETFOCUS, LocalSay_OnSetFocus );
      HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, LocalSay_OnAdjustWindows );
      HANDLE_MSG( hwnd, WM_CHANGEFONT, Common_ChangeFont );
      HANDLE_MSG( hwnd, WM_NOTIFY, LocalSay_OnNotify );

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


      default:
         break;
      }
   return( Adm_DefMDIDlgProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_SETFOCUS message.
 */
void FASTCALL LocalSay_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   LPLOCALSAYWNDINFO lpmwi;
   HWND hwndFocus;

   lpmwi = GetLocalSayInfo( hwnd );
   hwndFocus = lpmwi->hwndFocus;
   if( hwndFocus )
      SetFocus( hwndFocus );
   iActiveMode = WIN_EDITS;
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL LocalSay_OnCreate( HWND hwnd, LPCREATESTRUCT lpCreateStruct )
{
   LPMDICREATESTRUCT lpMDICreateStruct;
   LPLOCALSAYWNDINFO lpmwi;
   LOCALSAYOBJECT FAR * lplso;
   HWND hwndList;
   HWND hwndEdit;
   PTL ptl;

   INITIALISE_PTR(lpmwi);

   /* Create the dialog window.
    */
   lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
   lplso = (LOCALSAYOBJECT FAR *)lpMDICreateStruct->lParam;
   Adm_MDIDialog( hwnd, MAKEINTRESOURCE(IDDLG_SAYEDITOR), lpMDICreateStruct );

   /* Set the edit window font.
    */
#ifdef USEBIGEDIT
   SetWindowFont( GetDlgItem( hwnd, IDD_EDIT ), hEditFont, FALSE );
#else USEBIGEDIT
   SetEditorDefaults(GetDlgItem( hwnd, IDD_EDIT ), FALSE, lfEditFont, fWordWrap, fHotLinks , fMsgStyles, crEditText, crEditWnd );
#endif USEBIGEDIT

   /* Create a structure to store the mail window info.
    */
   if( !fNewMemory( &lpmwi, sizeof(LOCALSAYWNDINFO) ) )
      return( FALSE );
   SetLocalSayInfo( hwnd, lpmwi );

   /* Fill the list of signatures and set the
    * first one to (None).
    */
   FillSignatureList( hwnd, IDD_LIST );

   /* Select the Conference field.
    */
   FillLocalConfList( hwnd, DRF(lplso->recConfName) );
   ptl = FillLocalTopicList( hwnd, DRF(lplso->recConfName), DRF(lplso->recTopicName) );

   /* Select the signature.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   CommonSetSignature( hwndList, ptl );
   ComboBox_GetText( hwndList, lpmwi->szSigFile, sizeof(lpmwi->szSigFile) );

   /* Fill the Subject field.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_SUBJECT );
   Edit_SetText( hwndEdit, DRF(lplso->recSubject) );
   Edit_LimitText( hwndEdit, iMaxSubjectSize );

   /* Disable the OK button if the Subject field is blank.
    */
   EnableControl( hwnd, IDOK, lplso->recSubject.dwSize > 0 );

   /* Subclass the edit control so we can do indents.
    */
   lpEditCtrlProc = SubclassWindow( GetDlgItem( hwnd, IDD_EDIT ), LocalSayEditProc );

   /* Setup the WordWrap function !!SM!!
   */
#ifdef SMWORDBREAK
   SendDlgItemMessage(hwnd, IDD_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)(EDITWORDBREAKPROC)WordBreakProc) ;
#endif SMWORDBREAK

   /* Fill the article edit control.
    */
   SetEditText( hwnd, ptl, IDD_EDIT, DRF(lplso->recText), TRUE );
#ifndef USEBIGEDIT
   SendMessage( GetDlgItem( hwnd, IDD_EDIT ), SCI_SETSAVEPOINT,  (WPARAM)0, (LPARAM)0L ); 
#endif USEBIGEDIT
   Amob_FreeRefObject( &lplso->recText );

   return( TRUE );
}

/* This function handles indents in the edit window.
 */
LRESULT EXPORT CALLBACK LocalSayEditProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   return( EditCtrlIndent( lpEditCtrlProc, hwnd, msg, wParam, lParam ) );
}

LRESULT FASTCALL LocalSay_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
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
void FASTCALL LocalSay_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   CheckMenuStates( ); // !!SM!! 2.55
   switch( id )
      {
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

      case IDD_SUBJECT:
         if( codeNotify == EN_CHANGE )
            EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndCtl ) > 0 );

      case IDD_EDIT:
         if( codeNotify == EN_SETFOCUS )
            {
            LPLOCALSAYWNDINFO lpmwi;

            lpmwi = GetLocalSayInfo( hwnd );
            lpmwi->hwndFocus = hwndCtl;
            }
         else if( codeNotify == EN_ERRSPACE || codeNotify == EN_MAXTEXT )
            fMessageBox( hwnd, 0, GS(IDS_STR77), MB_OK|MB_ICONEXCLAMATION);
         break;

      case IDD_LIST:
         if( codeNotify == CBN_SELCHANGE )
            {
            LPLOCALSAYWNDINFO lpmwi;

            lpmwi = GetLocalSayInfo( hwnd );
            CommonSigChangeCode( hwnd, lpmwi->szSigFile, FALSE, FALSE );
            }
         break;

      case IDM_SPELLCHECK:
         Ameol2_SpellCheckDocument( hwnd, GetDlgItem( hwnd, IDD_EDIT ), SC_FL_DIALOG );
         break;

      case IDOK: {
         char szTopicName[ LEN_TOPICNAME+1 ];
         char szConfName[ 100 ];
         LPLOCALSAYWNDINFO lpmwi;
         TOPICINFO topicinfo;
         LOCALSAYOBJECT lso;
         HWND hwndEdit;
         HWND hwndSay;
         AM_DATE date;
         AM_TIME time;
         int cbSubject;
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

         /* Dereference the LOCALSAYOBJECT data structure.
          */
         lpmwi = GetLocalSayInfo( hwnd );
         Amob_New( OBTYPE_LOCALSAYMESSAGE, &lso );

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

         /* Okay to save say, so update in out-basket.
          */
         Amob_CreateRefString( &lso.recConfName, szConfName );
         Amob_CreateRefString( &lso.recTopicName, szTopicName );
         Amob_SaveRefObject( &lso.recConfName );
         Amob_SaveRefObject( &lso.recTopicName );

         /* Get and update the Subject field.
          */
         cbSubject = Edit_GetTextLength( hwndSay );
         if( Amob_CreateRefObject( &lso.recSubject, cbSubject + 1 ) )
            {
            Edit_GetText( hwndSay, lso.recSubject.hpStr, cbSubject + 1 );
            Amob_SaveRefObject( &lso.recSubject );
            }

         /* Get and update the Text field.
          */
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
                  cbSubject + cbText + 2,
                  date.iDay,
                  (LPSTR)pszCixMonths[ date.iMonth - 1 ],
                  date.iYear % 100,
                  time.iHour,
                  time.iMinute );
         strcat( lpTmpBuf, "\r\n" );
         strcat( lpTmpBuf, DRF(lso.recSubject) );
         strcat( lpTmpBuf, "\r\n" );
         cbHdr = strlen(lpTmpBuf);
         if( Amob_CreateRefObject( &lso.recText, cbText + cbHdr + 1 ) )
            {
            strcpy( lso.recText.hpStr, lpTmpBuf );
            Edit_GetText( hwndEdit, lso.recText.hpStr + cbHdr, cbText + 1 );
            Amob_SaveRefObject( &lso.recText );
            }
         Amob_Commit( NULL, OBTYPE_LOCALSAYMESSAGE, &lso );
         Adm_DestroyMDIWindow( hwnd );
         break;
         }

      case IDCANCEL:
         /* Destroy the article window.
          */
         LocalSay_OnClose( hwnd );
         break;
      }
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL LocalSay_OnAdjustWindows( HWND hwnd, int dx, int dy )
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
void FASTCALL LocalSay_OnClose( HWND hwnd )
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
   Adm_DestroyMDIWindow( hwnd );
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL LocalSay_OnMove( HWND hwnd, int x, int y )
{
   Amuser_WriteWindowState( szLocalSayName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL LocalSay_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   /* Save the new window state to the configuration file.
    */
   Amuser_WriteWindowState( szLocalSayName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* This function handles the WM_MDIACTIVATE message.
 */
void FASTCALL LocalSay_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   if( fActive )
      ToolbarState_CopyPaste();
   Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_EDITS ), (LPARAM)(LPSTR)hwnd );
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, Adm_DefMDIDlgProc );
}

/* This is the Local Say out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_LocalSayMessage( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   LOCALSAYOBJECT FAR * lplso;

   lplso = (LOCALSAYOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_NEW:
         memset( lplso, 0, sizeof( LOCALSAYOBJECT ) );
         return( TRUE );

      case OBEVENT_REMOVE:
         FreeMemory( &lplso );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lplso->recConfName );
         Amob_FreeRefObject( &lplso->recTopicName );
         Amob_FreeRefObject( &lplso->recSubject );
         Amob_FreeRefObject( &lplso->recText );
         return( TRUE );

      case OBEVENT_EXEC: {
         HEADER * pHeader;
         CURMSG curmsg;
         HPSTR hpBuf;
         PCL pcl;
         PTH pth;

         /* Initialise the header.
          */
         INITIALISE_PTR(pHeader);
         if( fNewMemory( &pHeader, sizeof(HEADER) ) )
            {
            PTL ptlSaved;

            InitialiseHeader( pHeader );
            strcpy( pHeader->szAuthor, GetActiveUsername() );
            strcpy( pHeader->szTitle, DRF(lplso->recSubject) );
            hpBuf = DRF(lplso->recText);
   
            /* Set the postal date to the current date/time.
             */
            Amdate_GetCurrentDate( &pHeader->date );
            Amdate_GetCurrentTime( &pHeader->time );
         
            /* Derefence the structure elements.
             */
            pcl = Amdb_OpenFolder( NULL, DRF(lplso->recConfName) );
            ptlSaved = pHeader->ptl = Amdb_OpenTopic( pcl, DRF(lplso->recTopicName) );
   
            /* Call the common local say/comment code to put the message
             * into the database.
             */
            pth = LocalSayCommentStore( pHeader, hpBuf, lplso->recText.dwSize );
         
            /* Set this new message as current.
             */
            if( NULL != ptlSaved && NULL != pHeader->ptl && ( ptlSaved == pHeader->ptl ) && NULL != pth && hwndMsg )
               {
               curmsg.pth = pth;
               curmsg.ptl = pHeader->ptl;
               curmsg.pcl = Amdb_FolderFromTopic( curmsg.ptl );
               curmsg.pcat = Amdb_CategoryFromFolder( curmsg.pcl );
               SetCurrentMsg( &curmsg, FALSE, TRUE );
               }
            FreeMemory( &pHeader );
            }
         return( POF_PROCESSCOMPLETED );
         }
      }
   return( 0L );
}

/* This is common code shared with the local Say and Comment code for storing
 * a local comment in the database.
 */
PTH FASTCALL LocalSayCommentStore( HEADER * pHeader, HPSTR hpBuf, DWORD cbBuf )
{
   int nMatch;
   PTL ptlReal;
   PTH pth;

   Amdb_LockTopic( pHeader->ptl );
   ptlReal = pHeader->ptl;
   pth = NULL;
   nMatch = MatchRule( &ptlReal, hpBuf, pHeader, TRUE );
   if( !(nMatch & FLT_MATCH_COPY ) && ptlReal != pHeader->ptl )
      {
      Amdb_InternalUnlockTopic( pHeader->ptl );
      pHeader->ptl = ptlReal;
      Amdb_LockTopic( pHeader->ptl );
      }
   if( !( nMatch & FLT_MATCH_DELETE ) )
      {
      int r;

      /* Add the message to the database.
       */
      r = Amdb_CreateMsg( pHeader->ptl, &pth, (DWORD)-1, pHeader->dwComment, pHeader->szTitle, pHeader->szAuthor, "", &pHeader->date, &pHeader->time, hpBuf, cbBuf, 0L );
      if( r == AMDBERR_NOERROR )
         {
         if( ( nMatch & FLT_MATCH_COPY ) && ( ptlReal != pHeader->ptl ) )
            {
            PTH FAR * lppth;
            /* Okay, message got moved so move the message after
             * we've applied all the above.
             */
            INITIALISE_PTR(lppth);
            if( fNewMemory( (PTH FAR *)&lppth, 2 * sizeof( PTH ) ) )
               {
               lppth[ 0 ] = pth;
               lppth[ 1 ] = NULL;
               DoCopyMsgRange( hwndFrame, lppth, 1, ptlReal, FALSE, FALSE, FALSE );
               }
            }
         if( ( nMatch & FLT_MATCH_PRIORITY ) || pHeader->fPriority )
            Amdb_MarkMsgPriority( pth, TRUE, TRUE );
         if( nMatch & FLT_MATCH_READ )
            Amdb_MarkMsgRead( pth, TRUE );
         if( nMatch & FLT_MATCH_IGNORE )
            Amdb_MarkMsgIgnore( pth, TRUE, TRUE );
         if( nMatch & FLT_MATCH_READLOCK )
            Amdb_MarkMsgReadLock( pth, TRUE );
         if( nMatch & FLT_MATCH_KEEP )
            Amdb_MarkMsgKeep( pth, TRUE );
         if( nMatch & FLT_MATCH_MARK )
            Amdb_MarkMsg( pth, TRUE );

         Amdb_MarkMsgTagged( pth, FALSE );
         Amdb_MarkMsgUnavailable( pth, FALSE );

         }
      }
   Amdb_UnlockTopic( pHeader->ptl );
//  The unlocktopic above trashes the pth if the user has moved messages since starting to compose the local message
// return( pth ); //!!SM!! 2043
   return( NULL );
}
