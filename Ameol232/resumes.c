/* RESUMES.C - Opens a window to display resumes.
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
#include <string.h>
#include "amaddr.h"
#include "editmap.h"
#include "shellapi.h"
#include "amlib.h"
#include "print.h"
#include <dos.h>
#include <ctype.h>
#include <io.h>
#include "cix.h"
#include "editmap.h"

#ifndef USEBIGEDIT
#include "scintilla.h"
#include "scilexer.h"
#endif USEBIGEDIT

#define  THIS_FILE   __FILE__

extern BOOL fWordWrap;                 /* TRUE if we word wrap messages */

char szResumesWndClass[] = "amctl_resumescls";
char szResumesWndName[] = "Resumes";

char szResumesListWndClass[] = "amctl_resumeslstcls";
char szResumesListWndName[] = "Resumes List";

const char szResumesMap[] = "Resumes Name Map";

static BOOL fRegistered = FALSE;       /* TRUE if we've registered the resumes window class */
static BOOL fRegisteredList = FALSE;   /* TRUE if we've registered the resumes list window class */
static WNDPROC lpProcEditCtl;          /* Subclassed edit control procedure */

HWND hwndResumes = NULL;               /* Resumes window handle */
HWND hwndRsmPopup = NULL;              /* Resumes list window handle */

static BOOL fDefDlgEx = FALSE;               /* DefDlg recursion flag trap */
static BOOL fRsmPopupListChanging = FALSE;   /* TRUE if resumes list selection is being changed */
static int cCopies;                          /* Number of copies to print */

static BOOL fHasResumesIndex = FALSE;  /* TRUE if we have a resumes index */
static char szLastOn[] = " was last on ";

extern BOOL fShortFilenames;

typedef struct tagRESUMESIDXITEM {
   struct tagRESUMESIDXITEM FAR * lpridxNext;
   struct tagRESUMESIDXITEM FAR * lpridxPrev;
   BOOL fRead;
   char * pszUsername;
} RESUMESIDXITEM, FAR * LPRESUMESIDXITEM;

LPRESUMESIDXITEM lpridxFirst = NULL;   /* Pointer to first resumes index entry */
LPRESUMESIDXITEM lpridxLast = NULL;    /* Pointer to last resumes index entry */

typedef struct tagRESUMESWNDINFO {
   HWND hwndFocus;
   BOOL fFixedFont;
   HBITMAP hPicBmp;
   char * pszUsername;
} RESUMESWNDINFO, FAR * LPRESUMESWNDINFO;

#define  GetResumesWndInfo(h)       (LPRESUMESWNDINFO)GetWindowLong((h),DWL_USER)
#define  SetResumesWndInfo(h,p)     SetWindowLong((h),DWL_USER,(long)(p))

BOOL FASTCALL Resumes_OnCreate( HWND, CREATESTRUCT FAR * );
void FASTCALL Resumes_OnSize( HWND, UINT, int, int );
void FASTCALL Resumes_OnMove( HWND, int, int );
void FASTCALL Resumes_MDIActivate( HWND, BOOL, HWND, HWND );
void FASTCALL Resumes_OnDestroy( HWND );
BOOL FASTCALL Resumes_OnClose( HWND );
void FASTCALL Resumes_OnCommand( HWND, int, HWND, UINT );
void FASTCALL Resumes_OnSetFocus( HWND, HWND );
void FASTCALL Resumes_OnAdjustWindows( HWND, int, int );
LRESULT FASTCALL Resumes_OnNotify( HWND, int, LPNMHDR );

BOOL FASTCALL ResumesLst_OnCreate( HWND, CREATESTRUCT FAR * );
void FASTCALL ResumesLst_OnSize( HWND, UINT, int, int );
void FASTCALL ResumesLst_OnMove( HWND, int, int );
BOOL FASTCALL ResumesLst_OnClose( HWND );
void FASTCALL ResumesLst_OnDestroy( HWND );
void FASTCALL ResumesLst_OnCommand( HWND, int, HWND, UINT );
void FASTCALL ResumesLst_OnAdjustWindows( HWND, int, int );
void FASTCALL ResumesLst_OnSetFocus( HWND, HWND );
void FASTCALL ResumesLst_MDIActivate( HWND, BOOL, HWND, HWND );
void FASTCALL ResumesLst_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
void FASTCALL ResumesLst_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );

BOOL EXPORT CALLBACK PrintResumeDlg( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL PrintResumeDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL PrintResumeDlg_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL PrintResumeDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT CALLBACK GetResume( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL GetResume_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL GetResume_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL GetResume_DlgProc( HWND, UINT, WPARAM, LPARAM );

LRESULT EXPORT CALLBACK ResumesWndProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK ResumesLstWndProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT FAR PASCAL ResumeEditProc( HWND, UINT, WPARAM, LPARAM );

void FASTCALL MarkResumeRead( char *, BOOL );
BOOL FASTCALL IsResumeRead( char * );
void FASTCALL CmdResumePrint( HWND );
void FASTCALL SetCurrentResume( HWND, char * );
void FASTCALL CreateResumesIndex( BOOL );
BOOL FASTCALL DeleteResumeEntry( char * );
void FASTCALL DeleteResumesIndex( void );
void FASTCALL ShowResumesLastUpdated( HWND );
LPRESUMESIDXITEM FASTCALL GetResumeIndexPtr( char * );
LPRESUMESIDXITEM FASTCALL CreateResumeEntry( char * );

/* This function creates a new resumes window.
 */
BOOL FASTCALL OpenResumesWindow( char * pszUsername )
{
   LPRESUMESWNDINFO lprwi;
   HWND hwndFocus;

   /* If there is a resumes window already open then
    * re-use that window.
    */
   if( NULL == hwndResumes )
      {
      MDICREATESTRUCT mcs;
      DWORD dwState;
      RECT rc;

      /* Register the resumes viewer window class if we have
       * not already done so.
       */
      if( !fRegistered )
         {
         WNDCLASS wc;

         wc.style          = CS_HREDRAW | CS_VREDRAW;
         wc.lpfnWndProc    = ResumesWndProc;
         wc.hIcon          = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_RESUMES) );
         wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
         wc.lpszMenuName   = NULL;
         wc.cbWndExtra     = DLGWINDOWEXTRA;
         wc.cbClsExtra     = 0;
         wc.hbrBackground  = (HBRUSH)( COLOR_APPWORKSPACE + 1 );
         wc.lpszClassName  = szResumesWndClass;
         wc.hInstance      = hInst;
         if( !RegisterClass( &wc ) )
            return( FALSE );
         fRegistered = TRUE;
         }

      /* The default position of the message viewer window.
       */
      GetClientRect( hwndMDIClient, &rc );
      InflateRect( &rc, -20, -20 );
      dwState = 0;

      /* Get the actual window size, position and state.
       */
      ReadProperWindowState( szResumesWndName, &rc, &dwState );

      /* Create the window.
       */
      mcs.szTitle = szResumesWndName;
      mcs.szClass = szResumesWndClass;
      mcs.hOwner = hInst;
      mcs.x = rc.left;
      mcs.y = rc.top;
      mcs.cx = rc.right - rc.left;
      mcs.cy = rc.bottom - rc.top;
      mcs.style = dwState;
      mcs.lParam = 0L;

      /* Create the message window.
       */
      hwndResumes = (HWND)SendMessage( hwndMDIClient, WM_MDICREATE, 0, (LPARAM)(LPMDICREATESTRUCT)&mcs );
      if( hwndResumes == NULL )
         return( FALSE );
      }

   /* Show the window.
    */
   if( IsIconic( hwndResumes ) )
      SendMessage( hwndMDIClient, WM_MDIRESTORE, (WPARAM)hwndResumes, 0L );
   SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndResumes, 0L );
   UpdateWindow( hwndResumes );

   /* Determine where we put the focus.
    */
   VERIFY( hwndFocus = GetDlgItem( hwndResumes, IDD_EDIT ) );
   SetFocus( hwndFocus );

   /* Show the specified user in the resumes window.
    */
   SetCurrentResume( hwndResumes, pszUsername );

   /* Store the handle of the focus window.
    */
   lprwi = GetResumesWndInfo( hwndResumes );
   lprwi->hwndFocus = hwndFocus;
   return( TRUE );
}

/* This function handles all messages for the message viewer window.
 */
LRESULT EXPORT CALLBACK ResumesWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, Resumes_OnCreate );
      HANDLE_MSG( hwnd, WM_SIZE, Resumes_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, Resumes_OnMove );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, Resumes_MDIActivate );
      HANDLE_MSG( hwnd, WM_DESTROY, Resumes_OnDestroy );
      HANDLE_MSG( hwnd, WM_CLOSE, Resumes_OnClose );
      HANDLE_MSG( hwnd, WM_COMMAND, Resumes_OnCommand );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, Common_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_SETFOCUS, Resumes_OnSetFocus );
      HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, Resumes_OnAdjustWindows );
      HANDLE_MSG( hwnd, WM_NOTIFY, Resumes_OnNotify );

      case WM_TOGGLEHEADERS: {  // 2.25.2031
         HWND hwndEdit;
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_EDIT ) );
#ifdef USEBIGEDIT
         SetWindowFont( hwndEdit, hResumesFont, FALSE );
#else USEBIGEDIT
         SetEditorDefaults(hwndEdit, TRUE, lfResumesFont, fWordWrap, fHotLinks, fMsgStyles, crResumeText, crResumeWnd );
#endif USEBIGEDIT
         break;
         }

      case WM_TOGGLEFIXEDFONT: {
         LPRESUMESWNDINFO lprwi;
         HWND hwndEdit;

         /* Toggle fixed/normal font display.
          */
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_EDIT ) );
         lprwi = GetResumesWndInfo( hwnd );
         if( lprwi->fFixedFont )
            {
            SetWindowFont( hwndEdit, hResumesFont, TRUE );
            lprwi->fFixedFont = FALSE;
            }
         else
            {
#ifdef USEBIGEDIT
            SetWindowFont( hwndEdit, hFixedFont, TRUE );
#else USEBIGEDIT
            SetEditorDefaults(hwndEdit, TRUE, lfResumesFont, fWordWrap, fHotLinks, fMsgStyles, crResumeText, crResumeWnd );
#endif USEBIGEDIT
            lprwi->fFixedFont = TRUE;
            }
         break;
         }

      case WM_APPCOLOURCHANGE:
         if( wParam & WIN_RESUMES )
            {
            HWND hwndEdit;

            /* Reset the colours.
             */
            VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_EDIT ) );
#ifdef USEBIGEDIT
            SendMessage( hwndEdit, BEM_SETTEXTCOLOUR, 0, (COLORREF)crResumeText );
            SendMessage( hwndEdit, BEM_SETBACKCOLOUR, TRUE, (COLORREF)crResumeWnd );
            SendMessage( hwndEdit, BEM_SETHOTLINKCOLOUR, TRUE, (COLORREF)crHotlinks );
#else USEBIGEDIT
            SetEditorDefaults(hwndEdit, TRUE, lfResumesFont, fWordWrap, fHotLinks , fMsgStyles, crResumeText, crResumeWnd );
#endif USEBIGEDIT
            }
         return( 0L );

      case WM_CHANGEFONT:
         if( wParam == FONTYP_RESUMES || 0xFFFF == wParam )
            {
            LPRESUMESWNDINFO lprwi;
            HWND hwndEdit;

            /* Only change message font if we're not currently
             * showing the fixed font version.
             */
            VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_EDIT ) );
            lprwi = GetResumesWndInfo( hwnd );
            if( !lprwi->fFixedFont )
            {
#ifdef USEBIGEDIT
               SetWindowFont( hwndEdit, hResumesFont, FALSE );
#else USEBIGEDIT
               SetEditorDefaults(GetDlgItem( hwnd, IDD_EDIT ), TRUE, lfResumesFont, fWordWrap, fHotLinks , fMsgStyles, crResumeText, crResumeWnd );
#endif USEBIGEDIT
            }
            InvalidateRect( hwndEdit, NULL, TRUE );
            UpdateWindow( hwndEdit );
            }
         return( 0L );

      case WM_GETMINMAXINFO: {
         MINMAXINFO FAR * lpMinMaxInfo;
         LRESULT lResult;

         /* Set the minimum tracking size so the client window can never
          * be resized below 24 lines.
          */
         lResult = Adm_DefMDIDlgProc( hwnd, message, wParam, lParam );
         lpMinMaxInfo = (MINMAXINFO FAR *)lParam;
         lpMinMaxInfo->ptMinTrackSize.x = 320;
         lpMinMaxInfo->ptMinTrackSize.y = 140;
         return( lResult );
         }


      default:
         break;
      }
   return( Adm_DefMDIDlgProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL Resumes_OnCreate( HWND hwnd, CREATESTRUCT FAR * lpCreateStruct )
{
   LPMDICREATESTRUCT lpMDICreateStruct;
   LPRESUMESWNDINFO lprwi;
   HWND hwndEdit;

   INITIALISE_PTR(lprwi);

   /* Create the dialog window.
    */
   lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
   Adm_MDIDialog( hwnd, MAKEINTRESOURCE(IDDLG_RESUMES), lpMDICreateStruct );

   /* Create a structure to store the resumes window info.
    */
   if( !fNewMemory( &lprwi, sizeof(RESUMESWNDINFO) ) )
      return( FALSE );
   lprwi->fFixedFont = FALSE;
   lprwi->pszUsername = NULL;
   lprwi->hPicBmp = NULL;
   SetResumesWndInfo( hwnd, lprwi );

   /* Set the text window font and colour.
    */
   VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_EDIT ) );
#ifdef USEBIGEDIT
   SendMessage( hwndEdit, BEM_SETTEXTCOLOUR, 0, (COLORREF)crResumeText );
   SendMessage( hwndEdit, BEM_SETBACKCOLOUR, 0, (COLORREF)crResumeWnd );
   SendMessage( hwndEdit, BEM_SETHOTLINKCOLOUR, TRUE, (COLORREF)crHotlinks );
   SetWindowFont( hwndEdit, hResumesFont, FALSE );
   SendMessage( hwndEdit, BEM_ENABLEHOTLINKS, (WPARAM)fHotLinks, 0L );
// SendMessage( hwndEdit, BEM_SETWORDWRAP, fWordWrap, 0L );
   SendMessage( hwndEdit, BEM_SETWORDWRAP, !SendMessage( hwndEdit, BEM_GETWORDWRAP, 0, 0L ), 0L );
#else USEBIGEDIT
   SetEditorDefaults(hwndEdit, TRUE, lfResumesFont, fWordWrap, fHotLinks , fMsgStyles, crResumeText, crResumeWnd );
#endif USEBIGEDIT

   /* Set picture button bitmaps.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_NEXT ), hInst, IDB_BUTTONRIGHT );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_PREVIOUS ), hInst, IDB_BUTTONLEFT );

   /* Subclass the edit control.
    */
   lpProcEditCtl = SubclassWindow( GetDlgItem( hwnd, IDD_EDIT ), ResumeEditProc );
   
   /* Setup the WordWrap function !!SM!!
   */
#ifdef SMWORDBREAK
   SendDlgItemMessage(hwnd, IDD_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)(EDITWORDBREAKPROC)WordBreakProc) ;
#endif SMWORDBREAK

#ifndef USEBIGEDIT
   SendMessage( GetDlgItem( hwnd, IDD_EDIT ), SCI_SETSAVEPOINT,  (WPARAM)0, (LPARAM)0L ); 
#endif USEBIGEDIT
   return( TRUE );
}

/* This function handles notification messages from the
 * treeview control.
 */
LRESULT FASTCALL Resumes_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
{
   if( lpNmHdr->idFrom == IDD_EDIT )
#ifdef USEBIGEDIT
      if( BEMN_CLICK == lpNmHdr->code )
         {
         BEMN_HOTSPOT FAR * lpbemnHotspot;

         /* User clicked over a hotspot in the edit window, so
          * fire up the browser.
          */
         lpbemnHotspot = (BEMN_HOTSPOT FAR *)lpNmHdr;
         return( CommonHotspotHandler( hwnd, lpbemnHotspot->szText ) );
         }
#else USEBIGEDIT
      return Lexer_OnNotify( hwnd, idCode, lpNmHdr );
#endif USEBIGEDIT

   return( FALSE );
}

/* This is the subclassed resume window edit procedure.
 */
LRESULT EXPORT FAR PASCAL ResumeEditProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   if( ( msg == WM_RBUTTONDOWN || msg == WM_CONTEXTMENU /*!!SM!!*/) && hwndActive == GetParent( hwnd ) )
      {
      BEC_SELECTION bsel;
      HMENU hPopupMenu;
      HEDIT hedit;
      LPSTR pSelectedText;
      int cchSelectedText;
      AEPOPUP aep;
      POINT pt;

      INITIALISE_PTR(pSelectedText);

      /* Get the menu and its popup position.
       */
      GetCursorPos( &pt );
      if( hPopupMenus )
         DestroyMenu( hPopupMenus );
      hPopupMenus = LoadMenu( hRscLib, MAKEINTRESOURCE( IDMNU_POPUPS ) );
      hPopupMenu = GetSubMenu( hPopupMenus, IPOP_MSGWINDOW );

      /* If there is any selection, get that.
       */
      hedit = Editmap_Open( hwnd );
      Editmap_GetSelection( hedit, hwnd, &bsel );
      if( bsel.lo != bsel.hi )
         {
//       cchSelectedText = LOWORD( (bsel.hi - bsel.lo) ) + 1;
         cchSelectedText = (bsel.hi - bsel.lo) + 1; // !!SM!! 2.55.2035
         if( fNewMemory( &pSelectedText, cchSelectedText ) )
            Editmap_GetSel( hedit, hwnd, cchSelectedText, pSelectedText );
         else
            cchSelectedText = 0;
         }
      else
         {
         cchSelectedText = 0;
         pSelectedText = 0;
         }

      /* Call the AE_POPUP event.
       */
      aep.wType = WIN_RESUMES;
      aep.pFolder = NULL;
      aep.pSelectedText = pSelectedText;
      aep.cchSelectedText = cchSelectedText;
      aep.hMenu = hPopupMenu;
      aep.nInsertPos = 3;
      Amuser_CallRegistered( AE_POPUP, (LPARAM)(LPSTR)&aep, 0L );

      /* Popup the menu.
       */
      TrackPopupMenu( hPopupMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndFrame, NULL );
      return( TRUE );
      }
   if( msg == BEM_RESUMEDISPLAY )
   {
         HWND hwndEdit;
         LPRESUMESWNDINFO lprwi;
         LPRESUMESIDXITEM lpridx;

         VERIFY( hwndEdit = GetDlgItem( hwndResumes, IDD_EDIT ) );

         /* Re-display the resume.
          */
         lprwi = GetResumesWndInfo( hwndResumes );
         lpridx = GetResumeIndexPtr( lprwi->pszUsername );
         if( NULL != lpridx && NULL != lpridx->pszUsername )
            SetCurrentResume( hwndResumes, lpridx->pszUsername );
   }

   return( CallWindowProc( lpProcEditCtl, hwnd, msg, wParam, lParam ) );
}

/* This function handles the WM_SETFOCUS message.
 */
void FASTCALL Resumes_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   LPRESUMESWNDINFO lprwi;

   lprwi = GetResumesWndInfo( hwnd );
   if( lprwi->hwndFocus )
      SetFocus( lprwi->hwndFocus );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL Resumes_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   /* Save the new window state to the configuration file.
    */
   Amuser_WriteWindowState( szResumesWndName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL Resumes_OnMove( HWND hwnd, int x, int y )
{
   Amuser_WriteWindowState( szResumesWndName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL Resumes_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_RESUMES:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsRESUMES );
         break;

      case IDD_NEXT: {
         LPRESUMESWNDINFO lprwi;
         LPRESUMESIDXITEM lpridx;

         /* Display the next resume.
          */
         lprwi = GetResumesWndInfo( hwnd );
         lpridx = GetResumeIndexPtr( lprwi->pszUsername );
         if( NULL != lpridx && NULL != lpridx->lpridxNext )
            SetCurrentResume( hwnd, lpridx->lpridxNext->pszUsername );
         break;
         }

      case IDD_PREVIOUS: {
         LPRESUMESWNDINFO lprwi;
         LPRESUMESIDXITEM lpridx;

         /* Display the previous resume.
          */
         lprwi = GetResumesWndInfo( hwnd );
         lpridx = GetResumeIndexPtr( lprwi->pszUsername );
         if( NULL != lpridx && NULL != lpridx->lpridxPrev )
            SetCurrentResume( hwnd, lpridx->lpridxPrev->pszUsername );
         break;
         }

      case IDM_WORDWRAP: {
         HWND hwndEdit;

         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_EDIT ) );
#ifdef USEBIGEDIT
         SendMessage( hwndEdit, BEM_SETWORDWRAP, !SendMessage( hwndEdit, BEM_GETWORDWRAP, 0, 0L ), 0L );
#else USEBIGEDIT
         SetEditorDefaults(hwndEdit, TRUE, lfResumesFont, fWordWrap, fHotLinks , fMsgStyles, crResumeText, crResumeWnd );
#endif USEBIGEDIT
         break;
         }

      case IDM_HOTLINKS: {
         HWND hwndEdit;
         LPRESUMESWNDINFO lprwi;
         LPRESUMESIDXITEM lpridx;

         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_EDIT ) );
//       SendMessage( hwndFrame, BEM_ENABLEHOTLINKS, !fHotLinks, 0L );
         SendCommand( hwndFrame, IDM_HOTLINKS, 0 );

         /* Display the next resume.
          */
         lprwi = GetResumesWndInfo( hwnd );
         lpridx = GetResumeIndexPtr( lprwi->pszUsername );
         if( NULL != lpridx && NULL != lpridx->pszUsername )
            SetCurrentResume( hwnd, lpridx->pszUsername );
         break;
         }

      case IDM_QUICKPRINT:
      case IDM_PRINT:
         CmdResumePrint( hwnd );
         break;

      case IDD_EDITRESUME:
         /* Edit the current resume.
          */
         CmdEditResume( hwndFrame );
         break;

      case IDD_LIST:
         ShowResumesListWindow();
         break;

      case IDCANCEL:
      case IDD_CLOSE:
         /* Close the resumes window.
          */
         Resumes_OnClose( hwnd );
         break;

      case IDD_UPDATE: {
         LPRESUMESWNDINFO lprwi;
         RESUMEOBJECT ro;

         /* Create a RESUME object for retrieving the resume for the
          * selected user.
          */
         lprwi = GetResumesWndInfo( hwnd );
         Amob_New( OBTYPE_RESUME, &ro );
         Amob_CreateRefString( &ro.recUserName, lprwi->pszUsername );
         wsprintf( lpPathBuf, "%s\\", pszResumesDir );
         GetResumeFilename( lprwi->pszUsername, lpPathBuf + strlen(lpPathBuf) );
         Amob_CreateRefString( &ro.recFileName, lpPathBuf );
         if( Amob_Find( OBTYPE_RESUME, &ro ) )
            {
            wsprintf( lpTmpBuf, GS(IDS_STR396), lprwi->pszUsername );
            fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
            }
         else
            {
            wsprintf( lpTmpBuf, GS(IDS_STR395), lprwi->pszUsername );
            if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION ) == IDYES )
               Amob_Commit( NULL, OBTYPE_RESUME, &ro );
            }
         Amob_Delete( OBTYPE_RESUME, &ro );
         break;
         }
      }
}

/* This function handles the WM_MDIACTIVATE message.
 * We replace the current menu bar with the message viewer window
 * menu.
 */
void FASTCALL Resumes_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_RESUMES ), (LPARAM)(LPSTR)hwnd );
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, DefMDIChildProc );
}

/* This function handles the WM_CLOSE message.
 */
BOOL FASTCALL Resumes_OnClose( HWND hwnd )
{
   SendMessage( hwndMDIClient, WM_MDIDESTROY, (WPARAM)hwnd, 0L );
   ShowUnreadMessageCount();
   return( TRUE );
}

/* This function handles the WM_DESTROY message.
 */
void FASTCALL Resumes_OnDestroy( HWND hwnd )
{
   LPRESUMESWNDINFO lprwi;

   lprwi = GetResumesWndInfo( hwnd );
   if( NULL != lprwi->hPicBmp )
      DeleteBitmap( lprwi->hPicBmp );
   hwndResumes = NULL;
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL Resumes_OnAdjustWindows( HWND hwnd, int dx, int dy )
{
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_EDIT ), dx, dy );
// Adm_MoveWnd( GetDlgItem( hwnd, IDD_PIX ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_CLOSE ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_UPDATE ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_EDITRESUME ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_LIST ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_PREVIOUS ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_NEXT ), 0, dy );
}

/* This function displays the specified resume in
 * the edit window.
 */
void FASTCALL SetCurrentResume( HWND hwnd, char * pszUsername )
{
   LPRESUMESWNDINFO lprwi;
// HWND hwndPic;
   HNDFILE fh;

   /* Get the resume window structure.
    */
   lprwi = GetResumesWndInfo( hwnd );

   /* Get the picture from the address book.
    */
// if( NULL != lprwi->hPicBmp )
//    DeleteBitmap( lprwi->hPicBmp );
// lprwi->hPicBmp = Amuser_GetMugshot( pszUsername );
// hwndPic = GetDlgItem( hwnd, IDD_PIX );
// if( NULL != lprwi->hPicBmp )
//    PicCtrl_SetBitmapHandle( hwndPic, lprwi->hPicBmp );
// else
//    {
//    SetWindowFont( hwndPic, hHelvB8Font, FALSE );
//    PicCtrl_SetText( hwndPic, GS(IDS_STR941) );
//    PicCtrl_SetBitmapHandle( hwndPic, NULL );
//    }

   /* Create the pathname to the resume file.
    */
   wsprintf( lpPathBuf, "%s\\", pszResumesDir );
   GetResumeFilename( pszUsername, lpPathBuf + strlen( lpPathBuf ) );

   /* Open the file and read the contents into the
    * edit control.
    */
   if( HNDFILE_ERROR != ( fh = Amfile_Open( lpPathBuf, AOF_READ ) ) )
      {
      DWORD dwSize;
      WORD wDate;
      WORD wTime;

      dwSize = Amfile_GetFileSize( fh );
      if( (long)dwSize > 0L )
         {
         HPSTR lpszBuf;

         /* Allocate a big buffer for the resume.
          */
         INITIALISE_PTR(lpszBuf);
         if( fNewMemory32( &lpszBuf, dwSize + 1 ) )
            {
            if( Amfile_Read32( fh, lpszBuf, dwSize ) == dwSize )
               {
               HWND hwndEdit;

               /* File retrieved okay, so go fill the edit
                * control.
                */
               lpszBuf[ dwSize ] = '\0';
               hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
#ifndef USEBIGEDIT
               SetEditorDefaults(hwndEdit, FALSE, lfResumesFont, fWordWrap, fHotLinks , fMsgStyles, crResumeText, crResumeWnd );
#endif USEBIGEDIT
               SetWindowText( hwndEdit, lpszBuf );
#ifndef USEBIGEDIT
               SetEditorDefaults(hwndEdit, TRUE, lfResumesFont, fWordWrap, fHotLinks , fMsgStyles, crResumeText, crResumeWnd );
#endif USEBIGEDIT
               }
            else
               DiskReadError( hwnd, lpPathBuf, FALSE, FALSE );
            FreeMemory32( &lpszBuf );
            }
         else
            OutOfMemoryError( hwnd, FALSE, FALSE );
         }

      /* Get date and time of file.
       */
      Amfile_GetFileTime( fh, &wDate, &wTime );

      /* Show in the dialog box.
       */
      wsprintf( lpTmpBuf, GS(IDS_STR1158), Amdate_FriendlyDate( NULL, wDate, wTime ) );
      SetDlgItemText( hwnd, IDD_LASTUPDATED, lpTmpBuf );
      Amfile_Close( fh );
      }

   /* If this is the current CIX user, show the Edit
    * button.
    */
   ShowWindow( GetDlgItem( hwnd, IDD_EDITRESUME ), IsLoginAuthor( pszUsername ) ? SW_SHOW : SW_HIDE );

   /* Change title.
    */
   wsprintf( lpTmpBuf, GS(IDS_STR942), pszUsername );
   SetWindowText( hwnd, lpTmpBuf );

   /* If the list available, highlight this name in the
    * list.
    */
   if( hwndRsmPopup && !fRsmPopupListChanging )
      {
      HWND hwndList;
      int index;

      VERIFY( hwndList = GetDlgItem( hwndRsmPopup, IDD_LIST ) );
      if( LB_ERR != ( index = ListBox_FindStringExact( hwndList, -1, pszUsername ) ) )
         {
         LPRESUMESIDXITEM lpridx;

         lpridx = (LPRESUMESIDXITEM)ListBox_GetItemData( hwndList, index );
         ASSERT( NULL != lpridx );
         lpridx->fRead = TRUE;
         MarkResumeRead( pszUsername, TRUE );
         ListBox_SetSel( hwndList, FALSE, -1 );
         ListBox_SetSel( hwndList, TRUE, index );
         }
      }

   /* Save this user name.
    */
   if( NULL != lprwi->pszUsername )
      free( lprwi->pszUsername );
   lprwi->pszUsername = _strdup( pszUsername );
}

/* Mark a resume read or unread in the resumes index.
 */
void FASTCALL MarkResumeRead( char * pszUsername, BOOL fRead )
{
   Amuser_GetActiveUserDirectory( lpPathBuf, _MAX_PATH );
   strcat( lpPathBuf, "\\resumes.ini" );
   WritePrivateProfileString( "Unread", pszUsername, fRead ? NULL : "True", lpPathBuf );
}

/* Returns whether or not the specified resume is read.
 */
BOOL FASTCALL IsResumeRead( char * pszUsername )
{
   char szBuf[ 5 ];

   Amuser_GetActiveUserDirectory( lpPathBuf, _MAX_PATH );
   strcat( lpPathBuf, "\\resumes.ini" );
   GetPrivateProfileString( "Unread", pszUsername, "", szBuf, sizeof(szBuf), lpPathBuf );
   return( *szBuf != 'T' && *szBuf != 't' );
}

/* This function displays the resume for the specified user. If
 * the resume is not available, it offers to retrieve it from CIX.
 */
void FASTCALL CmdShowResume( HWND hwnd, LPSTR pszAuthor, BOOL fPrompt )
{
   char szAuthor[ 128 ];
   char * pszUsername;
   char * psz;

   /* Make sure the resume is for a CIX user.
    */
   strcpy( szAuthor, pszAuthor );
   pszUsername = szAuthor;
   if( psz = strchr( pszUsername, '@' ) )
      {
      if( !IsCompulinkDomain( psz + 1 ) )
         {
         fMessageBox( hwnd, 0, GS(IDS_STR414), MB_OK|MB_ICONINFORMATION );
         return;
         }
      *psz = '\0';
      }

   /* Create the pathname to the resume file.
    */
   wsprintf( lpPathBuf, "%s\\", pszResumesDir );
   GetResumeFilename( pszUsername, lpPathBuf + strlen( lpPathBuf ) );

   /* Does resume exist? If not, offer to retrieve it.
    */
   if( Amfile_QueryFile( lpPathBuf ) )
      {
      LPRESUMESIDXITEM lpridx;

      if( fHasResumesIndex )
         if( lpridx = GetResumeIndexPtr( pszUsername ) )
            lpridx->fRead = TRUE;
      MarkResumeRead( pszUsername, TRUE );
      OpenResumesWindow( pszUsername );
      }
   else
      {
      RESUMEOBJECT ro;

      /* Resume not available, so offer to retrieve it
       * from CIX.
       */
      Amob_New( OBTYPE_RESUME, &ro );
      Amob_CreateRefString( &ro.recUserName, pszUsername );
      Amob_CreateRefString( &ro.recFileName, lpPathBuf );
      if( Amob_Find( OBTYPE_RESUME, &ro ) )
         {
         wsprintf( lpTmpBuf, GS(IDS_STR393), pszUsername );
         fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
         }
      else if( !fPrompt )
         Amob_Commit( NULL, OBTYPE_RESUME, &ro );
      else
         {
         wsprintf( lpTmpBuf, GS(IDS_STR394), pszUsername );
         if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION ) == IDYES )
            Amob_Commit( NULL, OBTYPE_RESUME, &ro );
         }
      Amob_Delete( OBTYPE_RESUME, &ro );
      }
}

/* If hpszMsg points to a resume (which may be >64K), this function
 * extracts the CIX user name and stores the details in the database.
 */
void FASTCALL CreateNewResume( char * pszUsername, HPSTR hpszMsg )
{
   HNDFILE fh;

   /* Don't file invalid resumes.
    */
   if( _fmemcmp( hpszMsg, "No resume for ", 14 ) == 0 )
      return;

   /* Create the pathname to the resume file.
    */
   wsprintf( lpPathBuf, "%s\\", pszResumesDir );
   GetResumeFilename( pszUsername, lpPathBuf + strlen( lpPathBuf ) );

   /* Write the resume to the file.
    */
   if( HNDFILE_ERROR != ( fh = Amfile_Create( lpPathBuf, 0 ) ) )
      {
      LPRESUMESIDXITEM lpridx;
      DWORD dwSize;

      dwSize = hstrlen( hpszMsg );
      while( Amfile_Write32( fh, hpszMsg, dwSize ) != dwSize )
         if( !DiskWriteError( GetFocus(), lpPathBuf, TRUE, FALSE ) )
            {
            Amfile_Close( fh );
            Amfile_Delete( lpPathBuf );
            fh = HNDFILE_ERROR;
            break;
            }
      if( fh != HNDFILE_ERROR )
         Amfile_Close( fh );

      /* This resume is unread.
       */
      MarkResumeRead( pszUsername, FALSE );

      /* Okay, now add a new entry to the list of
       * resumes. If the popup window is visible, add
       * it to the window too.
       */
      if( fHasResumesIndex )
         if( lpridx = GetResumeIndexPtr( pszUsername ) )
            lpridx->fRead = FALSE;
         else
            {
            lpridx = CreateResumeEntry( pszUsername );
            if( NULL != hwndRsmPopup )
               {
               HWND hwndList;
               int index;

               /* Add to the listbox and remove first entry if the
                * list control was disabled.
                */
               VERIFY( hwndList = GetDlgItem( hwndRsmPopup, IDD_LIST ) );
               if( !IsWindowEnabled( hwndList ) )
                  {
                  ListBox_ResetContent( hwndList );
                  EnableControl( hwndRsmPopup, IDD_LIST, TRUE );
                  EnableControl( hwndRsmPopup, IDOK, TRUE );
                  EnableControl( hwndRsmPopup, IDD_DELETE, TRUE );
                  EnableControl( hwndRsmPopup, IDD_UPDATE, TRUE );
                  }
               index = ListBox_AddString( hwndList, pszUsername );
               ListBox_SetItemData( hwndList, index, (LPARAM)lpridx );
               if( 0 == ListBox_GetCount( hwndList ) )
                  ListBox_SetCurSel( hwndList, index );
               }
            }
      }
}

/* This function returns the name of the user being displayed in the
 * current resume.
 */
int FASTCALL GetResumeUserName( HWND hwnd, char * pszBuf, int cchMax )
{
   LPRESUMESWNDINFO lprwi;

   /* Get the resume window structure.
    */
   lprwi = GetResumesWndInfo( hwnd );
   ASSERT( cchMax > 0 );
   lstrcpyn( pszBuf, lprwi->pszUsername, cchMax-1 );
   return( strlen( pszBuf ) );
}

/* This function creates a filename for the specified user. If the
 * OS allows long filenames, we use the entire username. Otherwise
 * we use the first 8 characters.
 */
void FASTCALL GetResumeFilename( char * pszUsername, char * pszDest )
{
   register int c;

#ifdef WIN32
   if( !fShortFilenames )
      {
      wsprintf( pszDest, "%s.rsm", pszUsername );
      return;
      }
#endif
   /* If the username is 8 chrs or longer, then under Windows 3.x it
    * can map to multiple filenames. So consult the resumes map to get
    * the actual filename.
    */
   if( strlen( pszUsername ) >= 8 )
      {
      char szPathname[ _MAX_PATH ];
      char * pszChr8;

      Amuser_GetPPString( szResumesMap, pszUsername, "", pszDest, _MAX_FNAME );
      if( *pszDest )
         return;

      /* Create a default filename.
       */
      for( c = 0; c < 8; ++c )
         pszDest[ c ] = toupper( pszUsername[ c ] );
      pszDest[ c ] = '\0';
      strcat( pszDest, ".rsm" );
      pszChr8 = &pszDest[ 7 ];

      /* Loop, changing 8th chr of the filename, until we
       * find one that is unique.
       */
      wsprintf( szPathname, "%s\\%s", pszResumesDir, pszDest );
      if( Amfile_QueryFile( szPathname ) )
         {
         *pszChr8 = '0';
         while( Amfile_QueryFile( szPathname ) && *pszChr8 != 'z' )
            {
            if( *pszChr8 == '9' )
               *pszChr8 = 'a';
            else
               ++*pszChr8;
            wsprintf( szPathname, "%s\\%s", pszResumesDir, pszDest );
            }
         }

      /* Add this name to the map
       */
      Amuser_WritePPString( szResumesMap, pszUsername, pszDest );
      return;
      }

   /* If less than 8 chrs, then the username is
    * the filename.
    */
   for( c = 0; pszUsername[ c ]; ++c )
      pszDest[ c ] = toupper( pszUsername[ c ] );
   pszDest[ c ] = '\0';
   strcat( pszDest, ".rsm" );
}

/* This is the Get Resume out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_GetResume( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   RESUMEOBJECT FAR * lpro;

   lpro = (RESUMEOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR247), DRF(lpro->recUserName) );
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( 0 );

      case OBEVENT_NEW:
         memset( lpro, 0, sizeof( RESUMEOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         RESUMEOBJECT gpl;
      
         INITIALISE_PTR(lpro);
         Amob_LoadDataObject( fh, &gpl, sizeof( RESUMEOBJECT ) );
         if( fNewMemory( &lpro, sizeof( RESUMEOBJECT ) ) )
            {
            gpl.recUserName.hpStr = NULL;
            gpl.recFileName.hpStr = NULL;
            *lpro = gpl;
            }
         return( (LRESULT)lpro );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpro->recUserName );
         Amob_SaveRefObject( &lpro->recFileName );
         return( Amob_SaveDataObject( fh, lpro, sizeof( RESUMEOBJECT ) ) );

      case OBEVENT_COPY: {
         RESUMEOBJECT FAR * lproNew;
      
         INITIALISE_PTR( lproNew );
         lpro = (RESUMEOBJECT FAR *)dwData;
         if( fNewMemory( &lproNew, sizeof( RESUMEOBJECT ) ) )
            {
            INITIALISE_PTR( lproNew->recUserName.hpStr );
            INITIALISE_PTR( lproNew->recFileName.hpStr );
            Amob_CopyRefObject( &lproNew->recUserName, &lpro->recUserName );
            Amob_CopyRefObject( &lproNew->recFileName, &lpro->recFileName );
            }
         return( (LRESULT)lproNew );
         }

      case OBEVENT_FIND: {
         RESUMEOBJECT FAR * lpro1;
         RESUMEOBJECT FAR * lpro2;

         lpro1 = (RESUMEOBJECT FAR *)dwData;
         lpro2 = (RESUMEOBJECT FAR *)lpBuf;
         return( strcmp( DRF(lpro1->recUserName), DRF(lpro2->recUserName) ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpro );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpro->recUserName );
         Amob_FreeRefObject( &lpro->recFileName );
         return( TRUE );
      }
   return( 0L );
}

/* This function creates the popup window that shows the list of
 * available resumes, and fills the list with all resumes found.
 */
void FASTCALL ShowResumesListWindow( void )
{
   LPRESUMESIDXITEM lpridx;
   DWORD dwState;
   HWND hwndFocus;
   HWND hwndList;
   RECT rc;

   /* If resumes list already available, bring it to the
    * top.
    */
   if( NULL != hwndRsmPopup )
      {
      if( IsIconic( hwndRsmPopup ) )
         SendMessage( hwndMDIClient, WM_MDIRESTORE, (WPARAM)hwndRsmPopup, 0L );
      }
   else
      {
      /* Register the resumes list window class if we have
       * not already done so.
       */
      if( !fRegisteredList )
         {
         WNDCLASS wc;

         wc.style          = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
         wc.lpfnWndProc    = ResumesLstWndProc;
         wc.hIcon          = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_RESUMES) );
         wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
         wc.lpszMenuName   = NULL;
         wc.cbWndExtra     = DLGWINDOWEXTRA;
         wc.cbClsExtra     = 0;
         wc.hbrBackground  = (HBRUSH)( COLOR_WINDOW + 1 );
         wc.lpszClassName  = szResumesListWndClass;
         wc.hInstance      = hInst;
         if( !RegisterClass( &wc ) )
            return;
         fRegisteredList = TRUE;
         }

      /* The default position of the resumes list window.
       */
      GetClientRect( hwndMDIClient, &rc );
      rc.left += 20;
      rc.top += 20;
      rc.right = rc.left + 365;
      rc.bottom = rc.top + 365;
      dwState = 0L;

      /* Get the actual window size, position and state.
       */
      ReadProperWindowState( szResumesListWndName, &rc, &dwState );

      /* Create the window.
       */
      hwndRsmPopup = Adm_CreateMDIWindow( szResumesListWndName, szResumesListWndClass, hInst, &rc, dwState, 0L );
      if( NULL == hwndRsmPopup )
         return;

      /* Make sure we have the resumes index.
       */
      if( !fHasResumesIndex )
         CreateResumesIndex( FALSE );

      /* Fill the listbox.
       */
      VERIFY( hwndList = GetDlgItem( hwndRsmPopup, IDD_LIST ) );
      for( lpridx = lpridxFirst; lpridx; lpridx = lpridx->lpridxNext )
         {
         int index;

         index = ListBox_AddString( hwndList, lpridx->pszUsername );
         ListBox_SetItemData( hwndList, index, (LPARAM)lpridx );
         }

      /* Any resumes? Say so.
       */
      if( ListBox_GetCount( hwndList ) )
         {
         ListBox_SetSel( hwndList, TRUE, 0 );
         ShowResumesLastUpdated( hwndRsmPopup );
         }
      else
         {
         EnableWindow( hwndList, FALSE );
         ListBox_AddString( hwndList, GS(IDS_STR943) );
         EnableControl( hwndRsmPopup, IDOK, FALSE );
         EnableControl( hwndRsmPopup, IDD_DELETE, FALSE );
         EnableControl( hwndRsmPopup, IDD_UPDATE, FALSE );
         }
      }

   /* Show the window.
    */
   SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndRsmPopup, 0L );

   /* Determine where we put the focus.
    */
   VERIFY( hwndFocus = GetDlgItem( hwndRsmPopup, IDD_LIST ) );
   SetFocus( hwndFocus );
}

/* This function handles all messages for the resumes list window.
 */
LRESULT EXPORT CALLBACK ResumesLstWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, ResumesLst_OnCreate );
      HANDLE_MSG( hwnd, WM_COMMAND, ResumesLst_OnCommand );
      HANDLE_MSG( hwnd, WM_SIZE, ResumesLst_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, ResumesLst_OnMove );
      HANDLE_MSG( hwnd, WM_CLOSE, ResumesLst_OnClose );
      HANDLE_MSG( hwnd, WM_SETFOCUS, ResumesLst_OnSetFocus );
      HANDLE_MSG( hwnd, WM_DESTROY, ResumesLst_OnDestroy );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, ResumesLst_MDIActivate );
      HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, ResumesLst_OnAdjustWindows );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, Common_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_DRAWITEM, ResumesLst_OnDrawItem );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, ResumesLst_OnMeasureItem );

      case WM_GETMINMAXINFO: {
         MINMAXINFO FAR * lpMinMaxInfo;
         LRESULT lResult;

         /* Set the minimum tracking size so the client window can never
          * be resized below 24 lines.
          */
         lResult = Adm_DefMDIDlgProc( hwnd, message, wParam, lParam );
         lpMinMaxInfo = (MINMAXINFO FAR *)lParam;
         lpMinMaxInfo->ptMinTrackSize.x = 335;
         lpMinMaxInfo->ptMinTrackSize.y = 235;
         return( lResult );
         }

      default:
         break;
      }
   return( Adm_DefMDIDlgProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL ResumesLst_OnCreate( HWND hwnd, CREATESTRUCT FAR * lpCreateStruct )
{
   LPMDICREATESTRUCT lpMDICreateStruct;

   /* Create the dialog window.
    */
   lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
   Adm_MDIDialog( hwnd, MAKEINTRESOURCE(IDDLG_RESUMELIST), lpMDICreateStruct );
   return( TRUE );
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL ResumesLst_OnMove( HWND hwnd, int x, int y )
{
   Amuser_WriteWindowState( szResumesListWndName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL ResumesLst_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   Amuser_WriteWindowState( szResumesListWndName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL ResumesLst_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsRESUMELIST );
         break;

      case IDD_UPDATE: {
         register int c, d;
         HWND hwndList;
         BOOL fPrompt;
         int wButtonType;
         int wSelCount;
         int wCount;

         /* Create a RESUME object for retrieving the resume for the
          * selected users.
          */
         fPrompt = TRUE;
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         wCount = ListBox_GetCount( hwndList );
         wSelCount = ListBox_GetSelCount( hwndList );
         wButtonType = MB_YESNOALLCANCEL|MB_ICONQUESTION;
         for( d = 1, c = 0; c < wCount; ++c )
            if( ListBox_GetSel( hwndList, c ) )
               {
               RESUMEOBJECT ro;
               char sz[ 100 ];

               Amob_New( OBTYPE_RESUME, &ro );
               ListBox_GetText( hwndList, c, sz );
               Amob_CreateRefString( &ro.recUserName, sz );
               wsprintf( lpPathBuf, "%s\\", pszResumesDir );
               GetResumeFilename( sz, lpPathBuf + strlen(lpPathBuf) );
               Amob_CreateRefString( &ro.recFileName, lpPathBuf );
               if( Amob_Find( OBTYPE_RESUME, &ro ) )
                  {
                  wsprintf( lpTmpBuf, GS(IDS_STR396), sz );
                  fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
                  }
               else if( !fPrompt )
                  Amob_Commit( NULL, OBTYPE_RESUME, &ro );
               else
                  {
                  int r;

                  if( d == wSelCount )
                     wButtonType = MB_YESNO|MB_ICONQUESTION;
                  wsprintf( lpTmpBuf, GS(IDS_STR395), sz );
                  r = fMessageBox( hwnd, 0, lpTmpBuf, wButtonType );
                  if( r == IDCANCEL )
                     break;
                  if( r == IDALL )
                     fPrompt = FALSE;
                  if( r != IDNO )
                     Amob_Commit( NULL, OBTYPE_RESUME, &ro );
                  }
               Amob_Delete( OBTYPE_RESUME, &ro );
               ++d;
               }
         break;
         }

      case IDD_GETRESUME: {
         char sz[ 100 ];

         if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_GETRESUME), GetResume, (LPARAM)(LPSTR)sz ) )
            CmdShowResume( hwnd, sz, FALSE );
         break;
         }

      case IDD_DELETE: {
         HWND hwndList;
         BOOL fPrompt;
         register int c;

         /* Loop for all selected resumes and prompt the
          * user before deleting them.
          */
         fPrompt = TRUE;
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         for( c = 0; c < ListBox_GetCount( hwndList ); ++c )
            if( ListBox_GetSel( hwndList, c ) )
               {
               char sz[ 100 ];
               BOOL fDelete;

               /* Get the name and prompt unless prompting has been
                * disabled by the user choosing All
                */
               ListBox_GetText( hwndList, c, sz );
               if( fPrompt )
                  {
                  register int r;
                  int mbType;
                  int count;

                  wsprintf( lpTmpBuf, GS(IDS_STR401), sz );
                  count = ListBox_GetSelCount( hwndList );
                  mbType = (count > 1) ? MB_YESNOALLCANCEL : MB_YESNOCANCEL;
                  r = fMessageBox( hwnd, 0, lpTmpBuf, mbType|MB_ICONQUESTION );
                  if( r == IDNO )
                     continue;
                  if( r == IDCANCEL )
                     break;
                  if( r == IDALL )
                     fPrompt = FALSE;
                  }

               /* If we're deleting our own resume, look for an out-basket
                * object which is uploading our resume and offer to remove
                * it. If they decline, don't delete our own resume.
                */
               fDelete = TRUE;
               if( _strcmpi( sz, szCIXNickname ) == 0 )
                  {
                  LPVOID lpob;

                  if( NULL != ( lpob = Amob_Find( OBTYPE_PUTRESUME, NULL ) ) )
                     {
                     wsprintf( lpTmpBuf, GS(IDS_STR156), szCIXNickname );
                     if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION ) == IDYES )
                        Amob_RemoveObject( lpob, TRUE );
                     else
                        fDelete = FALSE;
                     }
                  }

               /* Okay to delete resume, so kindly vape it and kick
                * it off the list too.
                */
               if( fDelete )
                  {
                  /* Delete from our linked list.
                   */
                  DeleteResumeEntry( sz );

                  /* Delete the file.
                   */
                  wsprintf( lpPathBuf, "%s\\", pszResumesDir );
                  GetResumeFilename( sz, lpPathBuf + strlen( lpPathBuf ) );
                  Amfile_Delete( lpPathBuf );

                  /* Delete from the map.
                   */
                  Amuser_WritePPString( szResumesMap, sz, NULL );

                  /* Delete from the listbox.
                   */
                  ListBox_DeleteString( hwndList, c-- );
                  }
               }

         /* Is Listbox now empty?
          */
         if( 0 == ListBox_GetCount( hwndList ) )
            {
            EnableWindow( hwndList, FALSE );
            ListBox_AddString( hwndList, GS(IDS_STR943) );
            EnableControl( hwnd, IDOK, FALSE );
            EnableControl( hwnd, IDD_DELETE, FALSE );
            EnableControl( hwnd, IDD_UPDATE, FALSE );
            }
         SetFocus( hwndList );
         break;
         }

      case IDCANCEL:
         ResumesLst_OnClose( hwnd );
         break;

      case IDD_LIST:
         if( codeNotify == LBN_SELCHANGE )
            {
            ShowResumesLastUpdated( hwnd );
            break;
            }
         if( codeNotify != LBN_DBLCLK )
            break;

      case IDOK: {
         LPRESUMESIDXITEM lpridx;
         HWND hwndList;
         int index;
         RECT rc;

         /* Get the selected resume and show it in the
          * resumes window.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         index = ListBox_GetCurSel( hwndList );
         ASSERT( index != LB_ERR );
         lpridx = (LPRESUMESIDXITEM)ListBox_GetItemData( hwndList, index );
         fRsmPopupListChanging = TRUE;
         OpenResumesWindow( lpridx->pszUsername );
         lpridx->fRead = TRUE;
         MarkResumeRead( lpridx->pszUsername, TRUE );
         fRsmPopupListChanging = FALSE;

         /* Redraw this item to show it is read.
          */
         ListBox_GetItemRect( hwndList, index, &rc );
         InvalidateRect( hwndList, &rc, TRUE );
         UpdateWindow( hwndList );
         break;
         }
      }
}

/* This function shows when the selected resume was last
 * updated.
 */
void FASTCALL ShowResumesLastUpdated( HWND hwnd )
{
   HWND hwndList;

   /* Show when resume was last updated.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   if( ListBox_GetSelCount( hwndList ) > 1 )
      SetDlgItemText( hwnd, IDD_LASTUPDATED, "" );
   else
      {
      LPRESUMESIDXITEM lpridx;
      HNDFILE fh;
      int index;
      WORD wDate;
      WORD wTime;

      /* Get resume filename.
       */
      index = ListBox_GetCurSel( hwndList );
      ASSERT( index != LB_ERR );
      lpridx = (LPRESUMESIDXITEM)ListBox_GetItemData( hwndList, index );
      wsprintf( lpPathBuf, "%s\\", pszResumesDir );
      GetResumeFilename( lpridx->pszUsername, lpPathBuf + strlen(lpPathBuf) );

      /* Get date and time of file.
       */
      fh = Amfile_Open( lpPathBuf, AOF_READ );
      Amfile_GetFileTime( fh, &wDate, &wTime );
      Amfile_Close( fh );

      /* Show in the dialog box.
       */
      wsprintf( lpTmpBuf, GS(IDS_STR1158), Amdate_FriendlyDate( NULL, wDate, wTime ) );
      SetDlgItemText( hwnd, IDD_LASTUPDATED, lpTmpBuf );
      }
}

/* This function handles the WM_MEASUREITEM message.
 */
void FASTCALL ResumesLst_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   HFONT hFont;
   HDC hdc;

   hdc = GetDC( hwnd );
   hFont = SelectFont( hdc, hHelvB8Font );
   GetTextMetrics( hdc, &tm );
   lpMeasureItem->itemHeight = tm.tmHeight;
   SelectFont( hdc, hFont );
   ReleaseDC( hwnd, hdc );
}

/* This function handles the WM_DRAWITEM message.
 */
void FASTCALL ResumesLst_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   if( lpDrawItem->itemID != -1 )
      {
      LPRESUMESIDXITEM lpridx;

      /* Get listbox colours.
       */
      lpridx = (LPRESUMESIDXITEM)ListBox_GetItemData( lpDrawItem->hwndItem, lpDrawItem->itemID );
      if( NULL != lpridx )
         {
         COLORREF tmpT;
         COLORREF tmpB;
         COLORREF T;
         COLORREF B;
         HFONT hOldFont;
         HBRUSH hbr;
         RECT rc;

         /* Get drawing area and colours.
          */
         rc = lpDrawItem->rcItem;
         GetOwnerDrawListItemColours( lpDrawItem, &T, &B );
         hbr = CreateSolidBrush( B );

         /* Erase the entire drawing area
          */
         FillRect( lpDrawItem->hDC, &rc, hbr );
         tmpT = SetTextColor( lpDrawItem->hDC, T );
         tmpB = SetBkColor( lpDrawItem->hDC, B );

         /* Draw the resume name.
          */
         hOldFont = SelectFont( lpDrawItem->hDC, lpridx->fRead ? hHelv8Font : hSys8Font );
         rc.left += 2;
         DrawText( lpDrawItem->hDC, lpridx->pszUsername, -1, &rc, DT_NOPREFIX|DT_SINGLELINE|DT_VCENTER );
         SelectFont( lpDrawItem->hDC, hOldFont );

         /* Draw a focus if needed.
          */
         if( lpDrawItem->itemState & ODS_FOCUS )
            DrawFocusRect( lpDrawItem->hDC, &lpDrawItem->rcItem );

         /* Restore things back to normal.
          */
         SetTextColor( lpDrawItem->hDC, tmpT );
         SetBkColor( lpDrawItem->hDC, tmpB );
         DeleteBrush( hbr );
         }
      }
}

/* This function handles the WM_SETFOCUS message.
 */
void FASTCALL ResumesLst_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   SetFocus( GetDlgItem( hwnd, IDD_LIST ) );
}

/* This function handles the WM_CLOSE message.
 */
BOOL FASTCALL ResumesLst_OnClose( HWND hwnd )
{
   SendMessage( hwndMDIClient, WM_MDIDESTROY, (WPARAM)hwnd, 0L );
   ShowUnreadMessageCount();
   return( TRUE );
}

/* This function handles the WM_DESTROY message.
 */
void FASTCALL ResumesLst_OnDestroy( HWND hwnd )
{
   hwndRsmPopup = NULL;
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL ResumesLst_OnAdjustWindows( HWND hwnd, int dx, int dy )
{
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_LIST ), dx, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDOK ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_HELP ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDCANCEL ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_GETRESUME ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_UPDATE ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_DELETE ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_PAD1 ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_LASTUPDATED ), 0, dy );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_LASTUPDATED ), dx, 0 );
}

/* This function handles the WM_MDIACTIVATE message.
 */
void FASTCALL ResumesLst_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   if( !fActive )
      ShowUnreadMessageCount();
   Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_RESUMESLIST ), (LPARAM)(LPSTR)hwnd );
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, Adm_DefMDIDlgProc );
}

/* This function creates a linked list of all resumes
 * on the hard disk.
 */
void FASTCALL CreateResumesIndex( BOOL fFixupNames )
{
   int cResumesFound;
   register int n;
   FINDDATA ft;
   HFIND r;

   /* Delete any existing index.
    */
   if( fHasResumesIndex )
      DeleteResumesIndex();

   /* Keep count of resumes.
    */
   cResumesFound = 0;

   /* Scan the RESUMES directory for all RSM files and create index
    * entries for them.
    */
   wsprintf( lpPathBuf, "%s\\*.rsm", pszResumesDir );
   for( n = r = Amuser_FindFirst( lpPathBuf, _A_NORMAL, &ft ); n != -1; n = Amuser_FindNext( r, &ft ) )
      {
      LPRESUMESIDXITEM lpridx;
      char szUsername[ 80 ];
      char * pExt;

      /* Under Win32, the filename is the username, so
       * no need to open the file. Otherwise if the name is less
       * than 8 chrs, then it is the username. Otherwise open the
       * file and get the user name from the first line.
       */
   #ifdef WIN32
      if( !fFixupNames && !fShortFilenames )
         {
         strcpy( szUsername, ft.name );
         _strlwr( szUsername );
         if( NULL != ( pExt = stristr( szUsername, ".rsm" ) ) )
            *pExt = '\0';
         }
      else
   #endif
      if( strlen( ft.name ) < 8 )
         {
         strcpy( szUsername, ft.name );
         _strlwr( szUsername );
         if( NULL != ( pExt = stristr( szUsername, ".rsm" ) ) )
            *pExt = '\0';
         }
      else
         {
         HNDFILE fh;

         szUsername[ 0 ] = '\0';
         wsprintf( lpPathBuf, "%s\\%s", pszResumesDir, ft.name );
         if( ( fh = Amfile_Open( lpPathBuf, AOF_READ ) ) != HNDFILE_ERROR )
            {
            char sz[ 80 ];
            int cRead;

            /* Read the first line of the file.
             */
            cRead = Amfile_Read( fh, sz, sizeof(sz) - 1 );
            if( cRead > 0 && !isspace( sz[ 0 ] ) )
               {
               char * pszSep;

               /* The first word on the line is the username.
                * NULL terminate it.
                */
               sz[ cRead ] = '0';
               if( NULL != ( pszSep = strchr( sz, ' ' ) ) )
                  {
                  *pszSep = '\0';
                  strcpy( szUsername, sz );
                  }
               }
            Amfile_Close( fh );
            }
         }

      /* Null resumes caused by retrieving the resumes of people
       * who don't exist. Delete them.
       */
      if( *szUsername == '\0' || strcmp( szUsername, "No" ) == 0 )
         {
         Amfile_Delete( lpPathBuf );
         continue;
         }

      /* Under Win32, fFixupNames == TRUE specifies that the
       * filename is to be renamed to the longer format.
       */
   #ifdef WIN32
      if( fFixupNames && !fShortFilenames )
         {
         char szAssumedname[ _MAX_PATH ];

         strcpy( szAssumedname, ft.name );
         _strlwr( szAssumedname );
         if( NULL != ( pExt = stristr( szAssumedname, ".rsm" ) ) )
            *pExt = '\0';
         if( strcmp( szAssumedname, szUsername ) != 0 )
            {
            wsprintf( lpPathBuf, "%s\\%s", pszResumesDir, ft.name );
            wsprintf( szAssumedname, "%s\\%s.rsm", pszResumesDir, szUsername );
            Amfile_Rename( lpPathBuf, szAssumedname );
            }
         }
   #endif

      /* If this name is exactly 8 chrs, then add this name and
       * filename to the resumes map.
       */
   #ifdef WIN32
      if( fShortFilenames && strlen( ft.name ) == 12 )
         Amuser_WritePPString( szResumesMap, szUsername, ft.name );
   #else
      if( strlen( ft.name ) == 12 )
         Amuser_WritePPString( szResumesMap, szUsername, ft.name );
   #endif

      /* Create a new entry for this resume.
       */
      if( NULL == ( lpridx = CreateResumeEntry( szUsername ) ) )
         break;
      lpridx->fRead = IsResumeRead( szUsername );

      /* Bump statistics.
       */
      ++cResumesFound;
      if( 0 == (cResumesFound % 5 ) )
         StatusText( GS(IDS_STR944), cResumesFound );
      }
   Amuser_FindClose( r );

   /* Inform user we're done...
    */
   StatusText( GS(IDS_STR945), cResumesFound );
   fHasResumesIndex = TRUE;
}

/* This function creates a new entry in the linked list of resume
 * entries.
 */
LPRESUMESIDXITEM FASTCALL CreateResumeEntry( char * pszUsername )
{
   LPRESUMESIDXITEM lpridxNew;

   INITIALISE_PTR(lpridxNew);
   if( !fNewMemory( &lpridxNew, sizeof( RESUMESIDXITEM ) ) )
      OutOfMemoryError( hwndFrame, FALSE, FALSE );
   else
      {
      LPRESUMESIDXITEM lpridxNext;
      LPRESUMESIDXITEM lpridxPrev;

      INITIALISE_PTR(lpridxNew->pszUsername);

      /* Scan the list so we insert this
       * alphabetically.
       */
      lpridxPrev = NULL;
      lpridxNext = lpridxFirst;
      while( lpridxNext )
         {
         int comp;

         if( ( comp = _strcmpi( lpridxNext->pszUsername, pszUsername ) ) == 0 )
            {
            /* Already exists, so return now.
             */
            FreeMemory( &lpridxNew );
            return( lpridxNext );
            }
         else if( comp > 0 )
            break;
         lpridxPrev = lpridxNext;
         lpridxNext = lpridxNext->lpridxNext;
         }

      /* Allocate memory to store the name.
       */
      if( fNewMemory( &lpridxNew->pszUsername, strlen(pszUsername) + 1 ) )
         strcpy( lpridxNew->pszUsername, pszUsername );
      else
         {
         FreeMemory( &lpridxNew );
         OutOfMemoryError( hwndFrame, FALSE, FALSE );
         return( NULL );
         }

      /* First link this in.
       */
      if( NULL == lpridxPrev )
         lpridxFirst = lpridxNew;
      else
         lpridxPrev->lpridxNext = lpridxNew;
      if( NULL == lpridxNext )
         lpridxLast = lpridxNew;
      else
         lpridxNext->lpridxPrev = lpridxNew;
      lpridxNew->lpridxPrev = lpridxPrev;
      lpridxNew->lpridxNext = lpridxNext;
      lpridxNew->fRead = FALSE;
      }
   return( lpridxNew );
}

/* This function deletes the entry for the specified user
 * from the list.
 */
BOOL FASTCALL DeleteResumeEntry( char * pszUsername )
{
   LPRESUMESIDXITEM lpridx;

   if( !fHasResumesIndex )
      CreateResumesIndex( FALSE );
   for( lpridx = lpridxFirst; lpridx; lpridx = lpridx->lpridxNext )
      if( strcmp( lpridx->pszUsername, pszUsername ) == 0 )
         {
         if( NULL == lpridx->lpridxNext )
            lpridxLast = lpridx->lpridxPrev;
         else
            lpridx->lpridxNext->lpridxPrev = lpridx->lpridxPrev;
         if( NULL == lpridx->lpridxPrev )
            lpridxFirst = lpridx->lpridxNext;
         else
            lpridx->lpridxPrev->lpridxNext = lpridx->lpridxNext;
         FreeMemory( &lpridx->pszUsername );
         FreeMemory( &lpridx );
         return( TRUE );
         }
   return( FALSE );
}

/* Returns whether or not a resume is available for
 * the specified user.
 */
BOOL FASTCALL IsResumeAvailable( char * pszUsername )
{
   return( NULL != GetResumeIndexPtr( pszUsername ) );
}

/* This function returns the index item corresponding to the
 * specified user name.
 */
LPRESUMESIDXITEM FASTCALL GetResumeIndexPtr( char * pszUsername )
{
   LPRESUMESIDXITEM lpridx;

   if( !fHasResumesIndex )
      CreateResumesIndex( FALSE );
   for( lpridx = lpridxFirst; lpridx; lpridx = lpridx->lpridxNext )
      if( strcmp( lpridx->pszUsername, pszUsername ) == 0 )
         return( lpridx );
   return( NULL );
}

/* This function deletes the linked list of all resumes
 * on the hard disk.
 */
void FASTCALL DeleteResumesIndex( void )
{
   LPRESUMESIDXITEM lpridx;

   lpridx = lpridxFirst;
   while( lpridx )
      {
      LPRESUMESIDXITEM lpridxNext;

      lpridxNext = lpridx->lpridxNext;
      FreeMemory( &lpridx->pszUsername );
      FreeMemory( &lpridx );
      lpridx = lpridxNext;
      }
   lpridxFirst = NULL;
   lpridxLast = NULL;
   fHasResumesIndex = FALSE;
}

/* This command handles the Print command in the resume window.
 */
void FASTCALL CmdResumePrint( HWND hwnd )
{
   LPRESUMESWNDINFO lprwi;
   char sz[ 60 ];
   HPRINT hPr;
   BOOL fOk;

   /* MUST have a printer driver.
    */
   if( !IsPrinterDriver() )
      return;

   /* Get resume window handle.
    */
   lprwi = GetResumesWndInfo( hwnd );

   /* Show and get the print terminal dialog box.
    */
   if( !Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_PRINTRESUME), PrintResumeDlg, (LPARAM)lprwi ) )
      return;

   /* Start printing.
    */
   GetWindowText( hwnd, sz, sizeof(sz) );
   fOk = TRUE;
   if( NULL != ( hPr = BeginPrint( hwnd, sz, &lfResumesFont ) ) )
      {
      SELRANGE cbText;
      HWND hwndEdit;
      HPSTR lpText;
      HEDIT hedit;
      register int c;

      INITIALISE_PTR(lpText);
      for( c = 0; fOk && c < cCopies; ++c )
         {
         ResetPageNumber( hPr );
         fOk = BeginPrintPage( hPr );


         /* Select the appropriate font for printing.
          * Fixed font on screen means fixed font output.
          */
         PrintSelectFont( hPr, lprwi->fFixedFont ? &lfFixedFont : &lfResumesFont );

         /* Now basically just print the entire text using
          * the print engine's page formatting.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         hedit = Editmap_Open( hwndEdit );
         ASSERT( ETYP_NOTEDIT != hedit );
         cbText = Editmap_GetTextLength( hedit, hwndEdit );
         if( 0 < cbText )
            if( fNewMemory32( &lpText, cbText ) )
               {
               Editmap_GetText( hedit, hwndEdit, cbText, (LPSTR)lpText );
               fOk = PrintPage( hPr, lpText );
               FreeMemory32( &lpText );
               }
         if( fOk )
            EndPrintPage( hPr );
         }
      EndPrint( hPr );
      }
}

/* This function handles the Print dialog.
 */
BOOL EXPORT CALLBACK PrintResumeDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = PrintResumeDlg_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the Print Message
 * dialog.
 */
LRESULT FASTCALL PrintResumeDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, PrintResumeDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, PrintResumeDlg_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPRINTRESUME );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL PrintResumeDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPRESUMESWNDINFO lprwi;
   HWND hwndSpin;

   /* Set the input field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_COPIES ), 2 );

   /* Set number of copies.
    */
   SetDlgItemInt( hwnd, IDD_COPIES, 1, FALSE );
   hwndSpin = GetDlgItem( hwnd, IDD_SPIN );
   SendMessage( hwndSpin, UDM_SETBUDDY, (WPARAM)GetDlgItem( hwnd, IDD_COPIES ), 0L );
   SendMessage( hwndSpin, UDM_SETRANGE, 0, MAKELPARAM( 99, 1 ) );
   SendMessage( hwndSpin, UDM_SETPOS, 0, MAKELPARAM( 1, 0 ) );

   /* Display current printer name.
    */
   UpdatePrinterName( hwnd );

   /* Default to All if no selection, current selection
    * otherwise.
    */
   lprwi = (LPRESUMESWNDINFO)lParam;
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL PrintResumeDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_SETUP:
         PrintSetup( hwnd );
         UpdatePrinterName( hwnd );
         break;

      case IDOK:
         /* Get number of copies.
          */
         if( !GetDlgInt( hwnd, IDD_COPIES, &cCopies, 1, 99 ) )
            break;

         /* Get the options.
          */
         EndDialog( hwnd, TRUE );
         break;

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}

/* This function handles the GetResume dialog.
 */
BOOL EXPORT CALLBACK GetResume( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = GetResume_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the GetResume
 * dialog.
 */
LRESULT FASTCALL GetResume_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, GetResume_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, GetResume_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsGETRESUME );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL GetResume_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   Edit_LimitText( GetDlgItem( hwnd, IDD_EDIT ), LEN_CIXNAME );
   EnableControl( hwnd, IDOK, FALSE );
   SetWindowLong( hwnd, DWL_USER, lParam );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL GetResume_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EDIT:
         if( codeNotify == EN_UPDATE )
            EnableControl( hwnd, IDOK, Edit_GetTextLength( GetDlgItem( hwnd, IDD_EDIT ) ) > 0 );
         break;

      case IDOK: {
         LPSTR lpchr;
         BOOL fOk = TRUE;
         register int c;

         lpchr = (LPSTR)GetWindowLong( hwnd, DWL_USER );
         Edit_GetText( GetDlgItem( hwnd, IDD_EDIT ), lpchr, LEN_INTERNETNAME+1 );
         for( c = 0; lpchr[ c ] && lpchr[ c ] != ' ' ; ++c )
            if( lpchr[ c ] == '@' )
               {
               if( !IsCompulinkDomain( &lpchr[ c ] + 1 ) )
                  {
                  fMessageBox( hwnd, 0, GS(IDS_STR414), MB_OK|MB_ICONEXCLAMATION );
                  fOk = FALSE;
                  }
               break;
               }
         if( fOk )
            EndDialog( hwnd, TRUE );
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}
