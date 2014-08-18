/* PARLIST.C - Handles CIX participant lists
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
#include <string.h>
#include "print.h"
#include "amlib.h"
#include "lbf.h"
#include "ameol2.h"
#include "cix.h"
#include <ctype.h>

#define  THIS_FILE   __FILE__

extern BOOL fIsAmeol2Scratchpad;

char szParlistWndClass[] = "amctl_ParlistWndcls";
char szParlistWndName[] = "Parlist";

#define  PLFLG_ADDPAR         0
#define  PLFLG_REMPAR         1
#define  PLFLG_PAR            2
#define  PLFLG_COMOD          3
#define  PLFLG_EXMOD          4
#define  PLFLG_MOD            5

#define  LEN_EXMODLIST        90
#define  LEN_REMPARLIST       90
#define  LEN_COMODLIST        90
#define  LEN_ADDPARLIST       90

static BOOL fDefDlgEx = FALSE;

static BOOL fRegistered = FALSE;       /* TRUE if we've registered the comment window */
static HBITMAP hbmpThreadBitmaps;      /* Handle of thread window bitmaps */
static WNDPROC lpProcListCtl = NULL;   /* Procedure address of listbox func */
HWND hwndParlist = NULL;               /* Handle of participants list window */
static int cMsgWnds = 0;               /* Number of simultaneous open parlist windows */
static int dlgID;                      /* Used to pass a Help ID to the Participants dialog */

static int cCopies;                    /* Number of copies to print */
static BOOL fPrintParts;               /* We print participants */
static BOOL fPrintMods;                /* We print moderators */

#define  IPOP_PARLIST         18
#define  IPOP_MODPARLIST      19

typedef struct tagParlistWndINFO {
   HWND hwndFocus;
   PCL pcl;
} PARLISTWNDINFO, FAR * LPPARLISTWNDINFO;

#define  GetParlistWndInfo(h)       (LPPARLISTWNDINFO)GetWindowLong((h),DWL_USER)
#define  SetParlistWndInfo(h,p)     SetWindowLong((h),DWL_USER,(long)(p))

BOOL FASTCALL ParlistWnd_OnCreate( HWND, LPCREATESTRUCT );
void FASTCALL ParlistWnd_OnClose( HWND );
void FASTCALL ParlistWnd_OnSize( HWND, UINT, int, int );
void FASTCALL ParlistWnd_OnMove( HWND, int, int );
void FASTCALL ParlistWnd_MDIActivate( HWND, BOOL, HWND, HWND );
void FASTCALL ParlistWnd_OnCommand( HWND, int, HWND, UINT );
void FASTCALL ParlistWnd_OnSetFocus( HWND, HWND );
void FASTCALL ParlistWnd_OnAdjustWindows( HWND, int, int );
LRESULT FASTCALL ParlistWnd_OnNotify( HWND, int, LPNMHDR );
int FASTCALL ParlistWnd_OnCharToItem( HWND, UINT, HWND, int );
void FASTCALL ParlistWnd_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
void FASTCALL ParlistWnd_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
void FASTCALL ParlistWnd_OnDrawItemList( HWND, const DRAWITEMSTRUCT FAR * );
void FASTCALL ParlistWnd_OnDrawItemConferences( HWND, const DRAWITEMSTRUCT FAR * );

LRESULT EXPORT CALLBACK ParlistWndProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK ParListProc( HWND, UINT, WPARAM, LPARAM );

BOOL FASTCALL ParList_RemPar( HWND, PCL );
BOOL FASTCALL ParList_ExMod( HWND, PCL );
BOOL FASTCALL ParList_AddPar( HWND, PCL );
BOOL FASTCALL ParList_CoMod( HWND, PCL );
void FASTCALL ParList_BanPar( HWND, PCL );

void FASTCALL CmdParlistPrint( HWND );
void FASTCALL CmdFindInParlist( HWND );
BOOL FASTCALL FillParticipantsList( HWND, PCL );
BOOL FASTCALL FillModeratorsList( HWND, PCL );
void FASTCALL AddCoModerators( HWND, PCL, LPSTR );
void FASTCALL ExmodModerators( HWND, PCL, LPSTR );
void FASTCALL AddParticipants( HWND, PCL, LPSTR );
BOOL FASTCALL PrintParticipantsList( HWND, HPRINT );
BOOL FASTCALL PrintModeratorsList( HWND, HPRINT );
void FASTCALL ShowParlistDateTime( HWND, PCL );

BOOL EXPORT CALLBACK PrintParlistDlg( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL PrintParlistDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL PrintParlistDlg_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL PrintParlistDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT CALLBACK AddPar( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL AddPar_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL AddPar_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL AddPar_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT CALLBACK CoMod( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL CoMod_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL CoMod_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL CoMod_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT CALLBACK BanParticipant( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL BanParticipant_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL BanParticipant_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL BanParticipant_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL WINAPI EXPORT ParlistEvents( int, LPARAM, LPARAM );

BOOL FASTCALL UpdateDatabasePartLists( PCAT, BOOL * );
BOOL FASTCALL UpdateFolderPartLists( PCL, BOOL * );

BOOL EXPORT CALLBACK FindInParlistDlg( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL FindInParlistDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL FindInParlistDlg_OnCommand( HWND, int, HWND, UINT );
BOOL FASTCALL FindInParlistDlg_OnInitDialog( HWND, HWND, LPARAM );
BOOL FASTCALL ParlistWndResetContent( HWND );

/* This function creates the Parlist window.
 */
BOOL FASTCALL CreateParlistWindow( HWND hwnd, PCL pcl )
{
   LPPARLISTWNDINFO lppli;
   HWND hwndFocus;
   DWORD dwState;
   int cxBorder;
   int cyBorder;
   RECT rc;

   /* Get the current participants list window for this folder
    * if one exists.
    */
   if( NULL == hwndParlist )
      {
      /* Register the group list window class if we have
       * not already done so.
       */
      if( !fRegistered )
         {
         WNDCLASS wc;

         wc.style       = CS_HREDRAW | CS_VREDRAW;
         wc.lpfnWndProc    = ParlistWndProc;
         wc.hIcon       = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_PARLIST) );
         wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
         wc.lpszMenuName      = NULL;
         wc.cbWndExtra     = DLGWINDOWEXTRA;
         wc.cbClsExtra     = 0;
         wc.hbrBackground  = (HBRUSH)( COLOR_APPWORKSPACE + 1 );
         wc.lpszClassName  = szParlistWndClass;
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
      ReadProperWindowState( szParlistWndName, &rc, &dwState );

      /* Create the window.
       */
      hwndParlist = Adm_CreateMDIWindow( szParlistWndName, szParlistWndClass, hInst, &rc, dwState, (LPARAM)pcl );
      if( NULL == hwndParlist )
         return( FALSE );
      }

   /* Show the window.
    */
   if( IsIconic( hwndParlist ) )
      SendMessage( hwndMDIClient, WM_MDIRESTORE, (WPARAM)hwndParlist, 0L );
   SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndParlist, 0L );
   UpdateWindow( hwndParlist );

   /* Determine where we put the focus.
    */
   VERIFY( hwndFocus = GetDlgItem( hwndParlist, IDD_CONFERENCE ) );
   SetFocus( hwndFocus );

   /* Store the handle of the focus window.
    */
   lppli = GetParlistWndInfo( hwndParlist );
   lppli->hwndFocus = hwndFocus;

   /* Post a message asking it to fill the list.
    */
   PostDlgCommand( hwnd, IDD_SETLIST, 0L );
   return( TRUE );
}

/* This is the main window procedure.
 */
LRESULT EXPORT CALLBACK ParlistWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, ParlistWnd_OnCreate );
      HANDLE_MSG( hwnd, WM_CLOSE, ParlistWnd_OnClose );
      HANDLE_MSG( hwnd, WM_SIZE, ParlistWnd_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, ParlistWnd_OnMove );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, Common_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, ParlistWnd_MDIActivate );
      HANDLE_MSG( hwnd, WM_COMMAND, ParlistWnd_OnCommand );
      HANDLE_MSG( hwnd, WM_CHARTOITEM, ParlistWnd_OnCharToItem );
      HANDLE_MSG( hwnd, WM_MENUSELECT, MainWnd_OnMenuSelect );
      HANDLE_MSG( hwnd, WM_SETFOCUS, ParlistWnd_OnSetFocus );
      HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, ParlistWnd_OnAdjustWindows );
      HANDLE_MSG( hwnd, WM_NOTIFY, ParlistWnd_OnNotify );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, ParlistWnd_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_DRAWITEM, ParlistWnd_OnDrawItem );

      case WM_GETMINMAXINFO: {
         MINMAXINFO FAR * lpMinMaxInfo;
         LRESULT lResult;

         /* Set the minimum tracking size so the client window can never
          * be resized below 24 lines.
          */
         lResult = Adm_DefMDIDlgProc( hwnd, message, wParam, lParam );
         lpMinMaxInfo = (MINMAXINFO FAR *)lParam;
         lpMinMaxInfo->ptMinTrackSize.x = 410;
         lpMinMaxInfo->ptMinTrackSize.y = 250;
         return( lResult );
         }

      default:
         break;
      }
   return( Adm_DefMDIDlgProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_MEASUREITEM message
 */
void FASTCALL ParlistWnd_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   HFONT hFont;
   BITMAP bmp;
   HDC hdc;

   hdc = GetDC( hwnd );
   hFont = SelectFont( hdc, hInBasketFont );
   GetTextMetrics( hdc, &tm );
   GetObject( hbmpThreadBitmaps, sizeof( BITMAP ), &bmp );
   SelectFont( hdc, hFont );
   ReleaseDC( hwnd, hdc );
   lpMeasureItem->itemHeight = max( tm.tmHeight, bmp.bmHeight );
}

/* This function converts a char code to a listbox item.
 */
int FASTCALL ParlistWnd_OnCharToItem( HWND hwnd, UINT ch, HWND hwndList, int iCaret )
{
   if( isprint( (char)ch ) )
      {
      int iStartCaret;
      PCL pcl;

      /* Start from the selected item. Wrap round if
       * we've gone past the end.
       */
      iStartCaret = iCaret;
      do {
         if( ++iCaret == ListBox_GetCount( hwndList ) )
            iCaret = 0;

         /* Get the folder name and see if the first letter matches the
          * character we're testing.
          */
         pcl = (PCL)ListBox_GetItemData( hwndList, iCaret );
         if( *Amdb_GetFolderName( pcl ) == (char)ch )
            return( iCaret );
         }
      while( iCaret != iStartCaret );
      return( -2 );
      }
   return( -1 );
}

/* This function handles the WM_DRAWITEM message
 */
void FASTCALL ParlistWnd_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   switch( lpDrawItem->CtlID )
      {
      case IDD_LIST:
         ParlistWnd_OnDrawItemList( hwnd, lpDrawItem );
         break;

      case IDD_CONFERENCE:
         ParlistWnd_OnDrawItemConferences( hwnd, lpDrawItem );
         break;
      }
}

/* This function handles the WM_DRAWITEM message for the conferences
 * window.
 */
void FASTCALL ParlistWnd_OnDrawItemConferences( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   if( lpDrawItem->itemID != -1 )
      if( lpDrawItem->itemAction == ODA_FOCUS )
         DrawFocusRect( lpDrawItem->hDC, (LPRECT)&lpDrawItem->rcItem );
      else {
         HWND hwndList;
         COLORREF tmpT;
         COLORREF tmpB;
         COLORREF T;
         COLORREF B;
         BITMAP bmp;
         HFONT hOldFont;
         PCL pcl;
         HBRUSH hbr;
         RECT rc;
         int y;

         hwndList = GetDlgItem( hwnd, IDD_CONFERENCE );
         pcl = (PCL)ListBox_GetItemData( hwndList, lpDrawItem->itemID );
         rc = lpDrawItem->rcItem;
         if( lpDrawItem->itemState & ODS_SELECTED )
            {
            T = GetSysColor( COLOR_HIGHLIGHTTEXT );
            B = GetSysColor( COLOR_HIGHLIGHT );
            }
         else {
            T = GetSysColor( COLOR_WINDOWTEXT );
            B = GetSysColor( COLOR_WINDOW );
            }
         hbr = CreateSolidBrush( B );

         /* Erase the entire drawing area
          */
         FillRect( lpDrawItem->hDC, &rc, hbr );
         tmpT = SetTextColor( lpDrawItem->hDC, T );
         tmpB = SetBkColor( lpDrawItem->hDC, B );

         /* Draw the bitmap first
          */
         GetObject( hbmpThreadBitmaps, sizeof( BITMAP ), &bmp );
         y = rc.top + ( ( rc.bottom - rc.top ) - bmp.bmHeight ) / 2;
         Amuser_DrawBitmap( lpDrawItem->hDC, rc.left, y, 16, bmp.bmHeight, hbmpThreadBitmaps, ABMP_CLOSED );
         rc.left += 20;

         /* Now draw the conference CIX name
          */
         hOldFont = SelectFont( lpDrawItem->hDC, hInBasketFont );
         DrawText( lpDrawItem->hDC, Amdb_GetFolderName( pcl ), -1, &rc, DT_NOPREFIX|DT_SINGLELINE|DT_VCENTER );
         SelectFont( lpDrawItem->hDC, hOldFont );

         /* Restore things back to normal.
          */
         SetTextColor( lpDrawItem->hDC, tmpT );
         SetBkColor( lpDrawItem->hDC, tmpB );
         DeleteBrush( hbr );
         }
}

/* This function handles the WM_DRAWITEM message for the list
 * window.
 */
void FASTCALL ParlistWnd_OnDrawItemList( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   if( lpDrawItem->itemID != -1 )
      if( lpDrawItem->itemAction == ODA_FOCUS )
         DrawFocusRect( lpDrawItem->hDC, (LPRECT)&lpDrawItem->rcItem );
      else {
         HWND hwndList;
         COLORREF tmpT;
         COLORREF tmpB;
         COLORREF T;
         COLORREF B;
         HFONT hOldFont;
         HBRUSH hbr;
         char sz[ LEN_INTERNETNAME+2 ];
         int index;
         RECT rc;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         ListBox_GetText( hwndList, lpDrawItem->itemID, sz );
         rc = lpDrawItem->rcItem;
         if( lpDrawItem->itemState & ODS_SELECTED ) {
            T = GetSysColor( COLOR_HIGHLIGHTTEXT );
            B = GetSysColor( COLOR_HIGHLIGHT );
            }
         else {
            T = GetSysColor( COLOR_WINDOWTEXT );
            B = GetSysColor( COLOR_WINDOW );
            }
         hbr = CreateSolidBrush( B );
         hOldFont = NULL;

         /* Erase the entire drawing area
          */
         FillRect( lpDrawItem->hDC, &rc, hbr );
         tmpT = SetTextColor( lpDrawItem->hDC, T );
         tmpB = SetBkColor( lpDrawItem->hDC, B );

         /* Draw the bitmap first
          */
         switch( ListBox_GetItemData( hwndList, lpDrawItem->itemID ) )
            {
            default:       index = 0;  ASSERT(FALSE); break;
            case PLFLG_REMPAR:   index = ABMP_REMPAR;    break;
            case PLFLG_EXMOD: index = ABMP_EXMOD;        break;
            case PLFLG_PAR:      index = ABMP_USER;         break;
            case PLFLG_ADDPAR:   index = ABMP_NEWUSER;      break;
            case PLFLG_MOD:      index = ABMP_ADMIN;        break;
            case PLFLG_COMOD: index = ABMP_NEWADMIN;     break;
            }
         Amuser_DrawBitmap( lpDrawItem->hDC, rc.left, rc.top, 16, 16, hbmpThreadBitmaps, index );
         rc.left += 18;

         /* Now draw the participants CIX name
          */
         switch( ListBox_GetItemData( hwndList, lpDrawItem->itemID ) )
            {
            default:
               ASSERT(FALSE);
               break;

            case PLFLG_MOD:
            case PLFLG_COMOD:
            case PLFLG_EXMOD:
               hOldFont = SelectFont( lpDrawItem->hDC, hSys8Font );
               break;

            case PLFLG_PAR:
            case PLFLG_ADDPAR:
            case PLFLG_REMPAR:
               hOldFont = SelectFont( lpDrawItem->hDC, hHelv8Font );
               break;
            }
         DrawText( lpDrawItem->hDC, sz, strlen(sz), &rc, DT_NOPREFIX|DT_SINGLELINE|DT_VCENTER );
         SelectFont( lpDrawItem->hDC, hOldFont );

         /* Restore things back to normal.
          */
         SetTextColor( lpDrawItem->hDC, tmpT );
         SetBkColor( lpDrawItem->hDC, tmpB );
         DeleteBrush( hbr );
         }
}

/* This function handles the WM_SETFOCUS message.
 */
void FASTCALL ParlistWnd_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   LPPARLISTWNDINFO lppli;
   HWND hwndFocus;

   lppli = GetParlistWndInfo( hwnd );
   hwndFocus = lppli->hwndFocus;
   if( hwndFocus )
      SetFocus( hwndFocus );
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL ParlistWnd_OnCreate( HWND hwnd, LPCREATESTRUCT lpCreateStruct )
{
   LPMDICREATESTRUCT lpMDICreateStruct;
   LPPARLISTWNDINFO lppli;
   HWND hwndList;
   HWND hwndTab;
   TC_ITEM tie;
   PCAT pcat;
   int index;
   PCL pcl;

   INITIALISE_PTR(lppli);

   /* If this is the first message window, load the thread bitmaps.
    * (MUST be done before we create the listbox!)
    */
   if( cMsgWnds++ == 0 )
      hbmpThreadBitmaps = LoadBitmap( hRscLib, MAKEINTRESOURCE(IDB_THREADBITMAPS) );
   ASSERT( NULL != hbmpThreadBitmaps );

   /* Create the dialog window.
    */
   lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
   Adm_MDIDialog( hwnd, MAKEINTRESOURCE(IDDLG_PARLIST), lpMDICreateStruct );

   /* Create a structure to store the mail window info.
    */
   if( !fNewMemory( &lppli, sizeof(PARLISTWNDINFO) ) )
      return( FALSE );
   SetParlistWndInfo( hwnd, lppli );

   /* Fill the conference list window.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_CONFERENCE ) );
   for( pcat = Amdb_GetFirstCategory(); pcat; pcat = Amdb_GetNextCategory( pcat ) )
      for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
         if( IsCIXFolder( pcl ) )
            ListBox_InsertString( hwndList, -1, (LPARAM)pcl );

   /* If nothing in the tree, exit now.
    */
   if( 0 == ListBox_GetCount( hwndList ) )
      {
      fMessageBox( hwnd, 0, GS(IDS_STR995), MB_OK|MB_ICONEXCLAMATION );
      return( FALSE );
      }

   /* Set the tab control.
    */
   VERIFY( hwndTab = GetDlgItem( hwnd, IDD_TABS ) );
   tie.mask = TCIF_TEXT;
   tie.pszText = GS(IDS_STR936);
   TabCtrl_InsertItem( hwndTab, 0, &tie );
   tie.pszText = GS(IDS_STR937);
   TabCtrl_InsertItem( hwndTab, 1, &tie );
   SetWindowFont( hwndTab, hHelvB8Font, FALSE );

   /* Select the conference.
    */
   pcl = (PCL)lpMDICreateStruct->lParam;
   if( LB_ERR == ( index = ListBox_FindItemData( hwndList, -1, (LPARAM)pcl ) ) )
      index = 0;
   ListBox_SetSel( hwndList, TRUE, index );
   ListBox_SetTopIndex( hwndList, index );
   lppli->pcl = (PCL)ListBox_GetItemData( hwndList, index );

   /* Ask to see out-basket events.
    */
   Amuser_RegisterEvent( AE_DELETING_OUTBASKET_OBJECT, ParlistEvents );
   Amuser_RegisterEvent( AE_DELETED_OUTBASKET_OBJECT, ParlistEvents );

   /* Subclass the window so that the Del and Ins key will work
    * in it.
    */
   lpProcListCtl = SubclassWindow( GetDlgItem( hwnd, IDD_LIST ), ParListProc );
   return( TRUE );
}

/* This function handles outbasket events.
 */
BOOL WINAPI EXPORT ParlistEvents( int wEvent, LPARAM lParam1, LPARAM lParam2 )
{
   static BOOL fRefresh = FALSE;

   switch( wEvent )
      {
      case AE_DELETED_OUTBASKET_OBJECT:
         if( fRefresh )
            {
            LPPARLISTWNDINFO lppli;

            lppli = GetParlistWndInfo( hwndParlist );
            if( TabCtrl_GetCurSel( GetDlgItem( hwndParlist, IDD_TABS ) ) == 0 )
               FillParticipantsList( hwndParlist, lppli->pcl );
            else
               FillModeratorsList( hwndParlist, lppli->pcl );
            fRefresh = FALSE;
            }
         break;

      case AE_DELETING_OUTBASKET_OBJECT:
         if( TabCtrl_GetCurSel( GetDlgItem( hwndParlist, IDD_TABS ) ) == 0 )
            {
            if( (OBCLSID)lParam2 == OBTYPE_ADDPAR || (OBCLSID)lParam2 == OBTYPE_REMPAR )
               {
               ADDPAROBJECT FAR * lpap;
               LPPARLISTWNDINFO lppli;
               OBINFO obinfo;
               PCL pcl;

               /* See if the out-basket object just deleted is for
                * the conference being displayed. If so, refresh
                * the window.
                */
               Amob_GetObInfo( (LPOB)lParam1, &obinfo );
               lpap = (ADDPAROBJECT FAR *)obinfo.lpObData;
               pcl = Amdb_OpenFolder( NULL, DRF(lpap->recConfName) );

               /* Compare against displayed list. If they
                * match, refresh the list.
                */
               lppli = GetParlistWndInfo( hwndParlist );
               if( pcl == lppli->pcl )
                  fRefresh = TRUE;
               }
            else if( (OBCLSID)lParam2 == OBTYPE_REMPAR )
               {
               REMPAROBJECT FAR * lprm;
               LPPARLISTWNDINFO lppli;
               OBINFO obinfo;
               PCL pcl;

               /* See if the out-basket object just deleted is for
                * the conference being displayed. If so, refresh
                * the window.
                */
               Amob_GetObInfo( (LPOB)lParam1, &obinfo );
               lprm = (REMPAROBJECT FAR *)obinfo.lpObData;
               pcl = Amdb_OpenFolder( NULL, DRF(lprm->recConfName) );

               /* Compare against displayed list. If they
                * match, refresh the list.
                */
               lppli = GetParlistWndInfo( hwndParlist );
               if( pcl == lppli->pcl )
                  fRefresh = TRUE;
               }
            }
         else if( TabCtrl_GetCurSel( GetDlgItem( hwndParlist, IDD_TABS ) ) == 1 )
            {
            if( (OBCLSID)lParam2 == OBTYPE_COMOD )
               {
               COMODOBJECT FAR * lpcmd;
               LPPARLISTWNDINFO lppli;
               OBINFO obinfo;
               PCL pcl;

               /* See if the out-basket object just deleted is for
                * the conference being displayed. If so, refresh
                * the window.
                */
               Amob_GetObInfo( (LPOB)lParam1, &obinfo );
               lpcmd = (COMODOBJECT FAR *)obinfo.lpObData;
               pcl = Amdb_OpenFolder( NULL, DRF(lpcmd->recConfName) );

               /* Compare against displayed list. If they
                * match, refresh the list.
                */
               lppli = GetParlistWndInfo( hwndParlist );
               if( pcl == lppli->pcl )
                  fRefresh = TRUE;
               }
            else if( (OBCLSID)lParam2 == OBTYPE_EXMOD )
               {
               EXMODOBJECT FAR * lpex;
               LPPARLISTWNDINFO lppli;
               OBINFO obinfo;
               PCL pcl;

               /* See if the out-basket object just deleted is for
                * the conference being displayed. If so, refresh
                * the window.
                */
               Amob_GetObInfo( (LPOB)lParam1, &obinfo );
               lpex = (EXMODOBJECT FAR *)obinfo.lpObData;
               pcl = Amdb_OpenFolder( NULL, DRF(lpex->recConfName) );

               /* Compare against displayed list. If they
                * match, refresh the list.
                */
               lppli = GetParlistWndInfo( hwndParlist );
               if( pcl == lppli->pcl )
                  fRefresh = TRUE;
               }
            }
         break;
      }
   return( TRUE );
}

/* This is the list box subclassed procedure.
 */
LRESULT EXPORT CALLBACK ParListProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   switch( msg )
      {
      case WM_CONTEXTMENU: /*!!SM!!*/
      case WM_RBUTTONDOWN: {
         LPPARLISTWNDINFO lppli;
         HMENU hPopupMenu;
         BOOL fIsModerator;
         register int c;
         POINT pt;

         /* Make sure this window is active and select the item
          * at the cursor.
          */
         if( hwndActive != GetParent( hwnd ) )
            SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndScheduler, 0L );
         c = ItemFromPoint( hwnd, (short)LOWORD(lParam), (short)HIWORD(lParam) );
         if( ListBox_GetSel( hwnd, c ) == 0 )
            {
            CallWindowProc( lpProcListCtl, hwnd, WM_LBUTTONDOWN, wParam, lParam );
            CallWindowProc( lpProcListCtl, hwnd, WM_LBUTTONUP, wParam, lParam );
            }

         /* Get the popup menu.
          */
         lppli = GetParlistWndInfo( GetParent( hwnd ) );
         fIsModerator = Amdb_IsModerator( lppli->pcl, szCIXNickname );
         hPopupMenu = GetSubMenu( hPopupMenus, fIsModerator ? IPOP_MODPARLIST : IPOP_PARLIST );

         /* Only do this if a moderator.
          */
         if( fIsModerator )
            {
            int cModerators;
            int cParticipants;
            int count;

            /* Loop for each selected item and count how many of
             * each type (participant and moderator) are selected.
             */
            cParticipants = 0;
            cModerators = 0;
            count = ListBox_GetCount( hwnd );
            for( c = 0; c < count; ++c )
               if( ListBox_GetSel( hwnd, c ) )
                  {
                  int type;

                  type = (int) ListBox_GetItemData( hwnd, c );
                  if( PLFLG_PAR == type )
                     ++cParticipants;
                  if( PLFLG_MOD == type )
                     ++cModerators;
                  }

            /* Disable menu commands as appropriate.
             */
            MenuEnable( hPopupMenu, IDM_PROMOTETOMOD, cParticipants > 0 );
            MenuEnable( hPopupMenu, IDM_DEMOTETOPAR, cModerators > 0 );
            MenuEnable( hPopupMenu, IDM_REMPAR, cParticipants > 0 );
            MenuEnable( hPopupMenu, IDM_EXMOD, cModerators > 0 );
            MenuEnable( hPopupMenu, IDM_BANPAR, cParticipants > 0 );
            }
         MenuEnable( hPopupMenu, IDD_RESUME, ListBox_GetSelCount( hwnd ) );

         /* Display menu
          */
         GetCursorPos( &pt );
         TrackPopupMenu( hPopupMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndFrame, NULL );
         return( 0 );
         }

      case WM_KEYDOWN:
         if( wParam == VK_DELETE ) {
            PostDlgCommand( GetParent( hwnd ), IDD_REMOVE, 0 );
            return( 0 );
            }
         if( wParam == VK_INSERT ) {
            PostDlgCommand( GetParent( hwnd ), IDD_ADD, 1 );
            return( 0 );
            }
         break;
      }
   return( CallWindowProc( lpProcListCtl, hwnd, msg, wParam, lParam ) );
}

/* This function processes the WM_COMMAND message.
 */
void FASTCALL ParlistWnd_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPARLIST );
         break;

      case IDM_QUICKPRINT:
      case IDM_PRINT:
         CmdParlistPrint( hwnd );
         break;

      case IDD_CONFERENCE:
         if( codeNotify == LBN_SELCHANGE )
            {
            int index;

            if( LB_ERR != ( index = ListBox_GetCurSel( hwndCtl ) ) )
               {
               LPPARLISTWNDINFO lppli;
               HWND hwndTab;
               PCL pcl;

               /* Get the details of the selected folder.
                */
               pcl = (PCL)ListBox_GetItemData( hwndCtl, index );

               /* Save the new folder handle.
                */
               lppli = GetParlistWndInfo( hwnd );
               lppli->pcl = pcl;

               /* Fill the left pane with the list of participants for
                * the selected folder.
                */
               hwndTab = GetDlgItem( hwnd, IDD_TABS );
               if( TabCtrl_GetCurSel( hwndTab ) == 1 )
                  FillModeratorsList( hwnd, pcl );
               else
                  FillParticipantsList( hwnd, pcl );
               EnableControl( hwnd, IDD_REMOVE, FALSE );
               EnableControl( hwnd, IDD_ADD, TRUE );
               ShowParlistDateTime( hwnd, pcl );
               }
            }
         break;


      case IDD_SETLIST: {
         LPPARLISTWNDINFO lppli;

         lppli = GetParlistWndInfo( hwnd );
         if( TabCtrl_GetCurSel( GetDlgItem( hwnd, IDD_TABS ) ) == 1 )
            FillModeratorsList( hwnd, lppli->pcl );
         else
            FillParticipantsList( hwnd, lppli->pcl );
         ShowParlistDateTime( hwnd, lppli->pcl );
         break;
         }

      case IDD_LIST:
         if( codeNotify == LBN_SELCHANGE )
            {
            EnableControl( hwnd, IDD_REMOVE, ListBox_GetSelCount( hwndCtl ) > 0 );
            EnableControl( hwnd, IDD_RESUME, ListBox_GetSelCount( hwndCtl ) > 0 );
            }
         break;

      case IDM_PROMOTETOMOD: {
         LPPARLISTWNDINFO lppli;
         LPSTR lpCoModList;
         UINT cbModListSize;
         register int c;
         HWND hwndList;
         int count;

         INITIALISE_PTR(lpCoModList);

         /* Initialise.
          */
         lppli = GetParlistWndInfo( hwnd );
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         count = ListBox_GetCount( hwndList );
         cbModListSize = 0;

         /* Loop and count the number of participants selected
          * from the list.
          */
         for( c = 0; c < count; ++c )
            if( ListBox_GetSel( hwndList, c ) )
               if( PLFLG_PAR == (int) ListBox_GetItemData( hwndList, c ) )
                  {
                  cbModListSize += ListBox_GetTextLen( hwndList, c );
                  ++cbModListSize;
                  }
         ++cbModListSize;

         /* Allocate a block of memory to contain the list.
          */
         if( 0 < cbModListSize )
            if( fNewMemory( &lpCoModList, cbModListSize ) )
               {
               lpCoModList[ 0 ] = '\0';
               for( c = 0; c < count; ++c )
                  if( ListBox_GetSel( hwndList, c ) )
                     {
                     if( PLFLG_PAR == (int) ListBox_GetItemData( hwndList, c ) )
                        {
                        ListBox_GetText( hwndList, c, lpCoModList + strlen(lpCoModList) );
                        strcat( lpCoModList, " " );
                        }
                     ListBox_SetSel( hwndList, 0, c );
                     }
               AddCoModerators( hwnd, lppli->pcl, lpCoModList );
               EnableControl( hwnd, IDD_REMOVE, FALSE );
               fMessageBox( hwnd, 0, GS(IDS_STR1113), MB_OK|MB_ICONEXCLAMATION );
               UpdatePartLists( lppli->pcl, FALSE );
               FreeMemory( &lpCoModList );
               }
         break;
         }

      case IDM_DEMOTETOPAR: {
         LPPARLISTWNDINFO lppli;
         LPSTR lpCoModList;
         UINT cbModListSize;
         register int c;
         HWND hwndList;
         int count;

         INITIALISE_PTR(lpCoModList);

         /* Initialise.
          */
         lppli = GetParlistWndInfo( hwnd );
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         count = ListBox_GetCount( hwndList );
         cbModListSize = 0;

         /* Loop and count the number of participants selected
          * from the list.
          */
         for( c = 0; c < count; ++c )
            if( ListBox_GetSel( hwndList, c ) )
               if( PLFLG_MOD == (int) ListBox_GetItemData( hwndList, c ) )
                  {
                  cbModListSize += ListBox_GetTextLen( hwndList, c );
                  ++cbModListSize;
                  }
         ++cbModListSize;

         /* Allocate a block of memory to contain the list.
          */
         if( 0 < cbModListSize )
            if( fNewMemory( &lpCoModList, cbModListSize ) )
               {
               lpCoModList[ 0 ] = '\0';
               for( c = 0; c < count; ++c )
                  if( ListBox_GetSel( hwndList, c ) )
                     {
                     if( PLFLG_MOD == (int) ListBox_GetItemData( hwndList, c ) )
                        {
                        ListBox_GetText( hwndList, c, lpCoModList + strlen(lpCoModList) );
                        strcat( lpCoModList, " " );
                        }
                     ListBox_SetSel( hwndList, 0, c );
                     }
               ExmodModerators( hwnd, lppli->pcl, lpCoModList );
               EnableControl( hwnd, IDD_REMOVE, FALSE );
               fMessageBox( hwnd, 0, GS(IDS_STR1114), MB_OK|MB_ICONEXCLAMATION );
               UpdatePartLists( lppli->pcl, FALSE );
               FreeMemory( &lpCoModList );
               }
         break;
         }

      case IDM_REMPAR: {
         LPPARLISTWNDINFO lppli;

         /* Switching between participants and moderator
          * views.
          */
         lppli = GetParlistWndInfo( hwnd );
         if( !Amdb_IsModerator( lppli->pcl, szCIXNickname ) )
            fMessageBox( hwnd, 0, GS(IDS_STR97), MB_OK|MB_ICONEXCLAMATION );
         else
            {
            HWND hwndTab;

            /* Special case handling for moderators
             */
            hwndTab = GetDlgItem( hwnd, IDD_TABS );
            if( ParList_RemPar( hwnd, lppli->pcl ) )
               if( TabCtrl_GetCurSel( hwndTab ) == 0 )
                  FillParticipantsList( hwnd, lppli->pcl );
               else
                  FillModeratorsList( hwnd, lppli->pcl );
            }
         break;
         }

      case IDM_BANPAR: {
         LPPARLISTWNDINFO lppli;

         lppli = GetParlistWndInfo( hwnd );
         if( !Amdb_IsModerator( lppli->pcl, szCIXNickname ) )
            fMessageBox( hwnd, 0, GS(IDS_STR97), MB_OK|MB_ICONEXCLAMATION );
         else
         {
            BOOL fPrompt = FALSE;
            ParList_BanPar( hwnd, lppli->pcl );
            UpdateParList( hwndFrame, lppli->pcl, &fPrompt, FALSE );
         }
         break;
         }

      case IDM_EXMOD: {
         LPPARLISTWNDINFO lppli;

         /* Switching between participants and moderator
          * views.
          */
         lppli = GetParlistWndInfo( hwnd );
         if( !Amdb_IsModerator( lppli->pcl, szCIXNickname ) )
            fMessageBox( hwnd, 0, GS(IDS_STR97), MB_OK|MB_ICONEXCLAMATION );
         else
            {
            HWND hwndTab;

            /* Special case handling for moderators
             */
            hwndTab = GetDlgItem( hwnd, IDD_TABS );
            if( ParList_ExMod( hwnd, lppli->pcl ) )
               if( TabCtrl_GetCurSel( hwndTab ) == 0 )
                  FillParticipantsList( hwnd, lppli->pcl );
               else
                  FillModeratorsList( hwnd, lppli->pcl );
            }
         break;
         }

      case IDD_UNBAN:
      case IDD_BAN: {
         LPPARLISTWNDINFO lppli;
         char szUsername[LEN_CIXNAME+1];

         lppli = GetParlistWndInfo( hwnd );
         if( !Amdb_IsModerator( lppli->pcl, szCIXNickname ) )
            fMessageBox( hwnd, 0, GS(IDS_STR97), MB_OK|MB_ICONEXCLAMATION );
         else
         {
            HWND hwndList;
            WORD wSelCount;

            VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
            wSelCount = ListBox_GetSelCount( hwndList );
            if (wSelCount > 1 && id == IDD_BAN)
               ParList_BanPar(hwnd, lppli->pcl);
            else
            {
               int iDlg = (id == IDD_BAN) ? IDDLG_BANPAR : IDDLG_UNBANPAR;
               dlgID = (id == IDD_BAN) ? idsBANPAR : idsUNBANPAR;

               szUsername[0] = '\0';
               if (wSelCount > 0)
               {
                  int index = ListBox_GetCurSel( hwndList );
                  ListBox_GetText( hwndList, index, szUsername);
               }

               if (Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(iDlg), BanParticipant, (LPARAM)szUsername))
               {
                  BANPAROBJECT ro;
                  BOOL fPrompt = FALSE;
                  int obType = (id == IDD_BAN) ? OBTYPE_BANPAR : OBTYPE_UNBANPAR;

                  Amob_New( obType, &ro );
                  Amob_CreateRefString( &ro.recUserName, szUsername );
                  Amob_CreateRefString( &ro.recConfName, (LPSTR)Amdb_GetFolderName( lppli->pcl ) );
                  if( !Amob_Find( obType, &ro ) )
                     Amob_Commit( NULL, obType, &ro );
                  Amob_Delete( obType, &ro );
                  UpdateParList( hwnd, lppli->pcl, &fPrompt, FALSE );
               }
            }
         }
         break;
         }

      case IDD_REMOVE: {
         LPPARLISTWNDINFO lppli;

         /* Switching between participants and moderator
          * views.
          */
         lppli = GetParlistWndInfo( hwnd );
         if( !Amdb_IsModerator( lppli->pcl, szCIXNickname ) )
            fMessageBox( hwnd, 0, GS(IDS_STR97), MB_OK|MB_ICONEXCLAMATION );
         else
            {
            HWND hwndTab;

            /* Special case handling for moderators
             */
            hwndTab = GetDlgItem( hwnd, IDD_TABS );
            if( TabCtrl_GetCurSel( hwndTab ) == 1 )
               {
               if( ParList_ExMod( hwnd, lppli->pcl ) )
                  FillModeratorsList( hwnd, lppli->pcl );
               }
            else
               {
               if( ParList_RemPar( hwnd, lppli->pcl ) )
                  FillParticipantsList( hwnd, lppli->pcl );
               }
            }
         SetFocus( GetDlgItem( hwnd, IDD_LIST ) );
         break;
         }

      case IDD_ADD: {
         LPPARLISTWNDINFO lppli;

         /* Switching between participants and moderator
          * views.
          */
         lppli = GetParlistWndInfo( hwnd );
         if( !Amdb_IsModerator( lppli->pcl, szCIXNickname ) )
            fMessageBox( hwnd, 0, GS(IDS_STR97), MB_OK|MB_ICONEXCLAMATION );
         else
            {
            HWND hwndTab;

            /* Special case handling for moderators
             */
            hwndTab = GetDlgItem( hwnd, IDD_TABS );
            if( TabCtrl_GetCurSel( hwndTab ) == 1 )
               {
               if( ParList_CoMod( hwnd, lppli->pcl ) )
                  FillModeratorsList( hwnd, lppli->pcl );
               }
            else
               {
               if( ParList_AddPar( hwnd, lppli->pcl ) )
                  FillParticipantsList( hwnd, lppli->pcl );
               }
            }
         SetFocus( GetDlgItem( hwnd, IDD_LIST ) );
         break;
         }

      case IDD_FIND:
         CmdFindInParlist( hwnd );
         break;

      case IDD_RESUME: {
         register int c, d;
         HWND hwndList;
         BOOL fPrompt;
         int wButtonType;
         int wSelCount;
         int count;

         /* Display resumes for selected participants.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         count = ListBox_GetCount( hwndList );
         wSelCount = ListBox_GetSelCount( hwndList );
         wButtonType = ( wSelCount < 2 ) ? MB_YESNO : MB_YESNOALLCANCEL;
         wButtonType |= MB_ICONQUESTION;
         fPrompt = TRUE;
         for( d = 1, c = 0; c < count; ++c )
            if( ListBox_GetSel( hwndList, c ) )
               {
               char sz[ LEN_INTERNETNAME+1 ];

               ListBox_GetText( hwndList, c, sz );
               if( !IsResumeAvailable( sz ) )
                  {
                  RESUMEOBJECT ro;

                  Amob_New( OBTYPE_RESUME, &ro );
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
                     wsprintf( lpTmpBuf, GS(IDS_STR394), sz );
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
               else if( 1 == wSelCount )
                  CmdShowResume( hwnd, sz, FALSE );
               }
         break;
         }

      case IDD_UPDATE: {
         register int c, d;
         HWND hwndList;
         BOOL fPrompt;
         int wSelCount;
         int wCount;

         /* Get the selected items.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_CONFERENCE ) );
         wSelCount = ListBox_GetSelCount( hwndList );
         wCount = ListBox_GetCount( hwndList );
         fPrompt = TRUE;
         for( d = 1, c = 0; c < wCount; ++c )
            if( ListBox_GetSel( hwndList, c ) )
               {
               PCL pcl;

               pcl = (PCL)ListBox_GetItemData( hwndList, c );
               if( !UpdateParList( hwnd, pcl, &fPrompt, d != wSelCount ) )
                  break;
               ++d;
               }
         SetFocus( hwndList );
         break;
         }

      case IDOK:
      case IDCANCEL:
         /* Close the participants window.
          */
         ParlistWnd_OnClose( hwnd );
         break;
      }
}

/* This function places a command in the out-basket to update the
 * participants lists for the specified folder.
 */
void FASTCALL UpdatePartLists( LPVOID pFolder, BOOL fPrompt )
{
   /* If no folder specified, request one visually.
    */
   if( NULL == pFolder )
      {
      CURMSG unr;
      PICKER cp;

      Ameol2_GetCurrentTopic( &unr );
      cp.wType = CPP_CONFERENCES;
      cp.ptl = NULL;
      cp.pcl = unr.pcl;
      strcpy( cp.szCaption, "Update Participants Lists" );
      if( !Amdb_ConfPicker( hwndFrame, &cp ) )
         return;
      pFolder = (LPVOID)cp.pcl;
      }

   /* Now go deal with the folder.
    */
   if( Amdb_IsCategoryPtr( (PCAT)pFolder ) )
      UpdateDatabasePartLists( (PCAT)pFolder, &fPrompt );
   else
      UpdateFolderPartLists( pFolder, &fPrompt );
}

/* This function places a command in the out-basket to update the
 * participants lists for the specified database.
 */
BOOL FASTCALL UpdateDatabasePartLists( PCAT pFolder, BOOL * pfPrompt )
{
   PCL pcl;

   for( pcl = Amdb_GetFirstFolder( (PCAT)pFolder ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
      if( !UpdateFolderPartLists( pcl, pfPrompt ) )
         return( FALSE );
   return( TRUE );
}

/* This function places a command in the out-basket to update the
 * participants lists for the specified topic.
 */
BOOL FASTCALL UpdateFolderPartLists( PCL pcl, BOOL * pfPrompt )
{
   /* If pcl is a topic, make it a folder. This is a
    * horrible hack and should be removed.
    */
   if( Amdb_IsTopicPtr( (PTL)pcl ) )
      pcl = Amdb_FolderFromTopic( (PTL)pcl );

   /* Make sure this is a CIX folder.
    */
   if( !IsCIXFolder( pcl ) )
      return( TRUE );

   /* Make sure topic has a file list.
    */
   return( UpdateParList( hwndFrame, pcl, pfPrompt, FALSE ) );
}

/* This function creates an out-basket object to update the
 * participant list for the selected folder.
 */
BOOL FASTCALL UpdateParList( HWND hwnd, PCL pcl, BOOL * pfPrompt, BOOL fAll )
{
   GETPARLISTOBJECT gpl;

   Amob_New( OBTYPE_GETPARLIST, &gpl );
   Amob_CreateRefString( &gpl.recConfName, (LPSTR)Amdb_GetFolderName( pcl ) );
   if( Amob_Find( OBTYPE_GETPARLIST, &gpl ) )
      {
      if( *pfPrompt )
         {
         wsprintf( lpTmpBuf, GS(IDS_STR155), Amdb_GetFolderName( pcl ) );
         fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
         }
      }
   else
      {
      if( *pfPrompt )
         {
         int wButtonType;
         int retCode;

         wsprintf( lpTmpBuf, GS(IDS_STR100), Amdb_GetFolderName( pcl ) );
         wButtonType = fAll ? MB_YESNOALLCANCEL|MB_ICONQUESTION : MB_YESNO|MB_ICONQUESTION;
         retCode = fMessageBox( hwnd, 0, lpTmpBuf, wButtonType );
         if( IDNO == retCode )
            {
            Amob_Delete( OBTYPE_GETPARLIST, &gpl );
            return( TRUE );
            }
         if( IDCANCEL == retCode )
            {
            Amob_Delete( OBTYPE_GETPARLIST, &gpl );
            return( FALSE );
            }
         if( IDALL == retCode )
            *pfPrompt = FALSE;
         }
      Amob_Commit( NULL, OBTYPE_GETPARLIST, &gpl );
      }
   Amob_Delete( OBTYPE_GETPARLIST, &gpl );
   return( TRUE );
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL ParlistWnd_OnAdjustWindows( HWND hwnd, int dx, int dy )
{
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_TABS ), dx / 2, dy );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_LIST ), dx / 2, dy );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_CONFERENCE ), dx / 2, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_LIST ), dx / 2, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_TABS ), dx / 2, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_PAD1 ), dx / 2, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDOK ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_ADD ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_REMOVE ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_BAN ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_UNBAN ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_RESUME ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_FIND ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_UPDATE ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_HELP ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_LASTUPDATED ), 0, dy );
}

/* This function handles the WM_CLOSE message.
 */
void FASTCALL ParlistWnd_OnClose( HWND hwnd )
{
   /* Destroy window.
    */
   hwndParlist = NULL;
   Adm_DestroyMDIWindow( hwnd );

   /* Remove out-basket events.
    */
   Amuser_UnregisterEvent( AE_DELETING_OUTBASKET_OBJECT, ParlistEvents );
   Amuser_UnregisterEvent( AE_DELETED_OUTBASKET_OBJECT, ParlistEvents );

   /* Free the bitmaps if we're the last message window to close.
    */
   ASSERT( cMsgWnds != 0 );
   if( --cMsgWnds == 0 )
      DeleteBitmap( hbmpThreadBitmaps );
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL ParlistWnd_OnMove( HWND hwnd, int x, int y )
{
   if( !IsIconic( hwnd ) )
      Amuser_WriteWindowState( szParlistWndName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL ParlistWnd_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   /* Save the new window state to the configuration file.
    */
   if( !IsIconic( hwnd ) )
      Amuser_WriteWindowState( szParlistWndName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* This function handles the WM_MDIACTIVATE message.
 */
void FASTCALL ParlistWnd_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   if( !fActive )
      ShowUnreadMessageCount();
   Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_PARLIST ), (LPARAM)(LPSTR)hwnd );
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, Adm_DefMDIDlgProc );
}

/* This function handles notification messages from the
 * treeview control.
 */
LRESULT FASTCALL ParlistWnd_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
{
   switch( lpNmHdr->code )
      {
      case TCN_SELCHANGE: {
         LPPARLISTWNDINFO lppli;

         /* Switching between participants and moderator
          * views.
          */
         lppli = GetParlistWndInfo( hwnd );
         if( TabCtrl_GetCurSel( lpNmHdr->hwndFrom ) == 1 )
            FillModeratorsList( hwnd, lppli->pcl );
         else
            FillParticipantsList( hwnd, lppli->pcl );
         return( 0L );
         }
      }
   return( FALSE );
}

/* This function shows the last date/time when the participants
 * list was last updated.
 */
void FASTCALL ShowParlistDateTime( HWND hwnd, PCL pcl )
{
   char szFileName[ _MAX_FNAME ];
   FOLDERINFO confinfo;
   HNDFILE fh;

   Amdb_GetFolderInfo( pcl, &confinfo );
   wsprintf( szFileName, "%s.PAR", (LPSTR)confinfo.szFileName );
   if( ( fh = Ameol2_OpenFile( szFileName, DSD_PARLIST, AOF_READ ) ) != HNDFILE_ERROR )
      {
      WORD uDate;
      WORD uTime;

      Amfile_GetFileTime( fh, &uDate, &uTime );
      wsprintf( lpTmpBuf, GS(IDS_STR336), Amdate_FriendlyDate( NULL, uDate, uTime ) );
      SetDlgItemText( hwnd, IDD_LASTUPDATED, lpTmpBuf );
      Amfile_Close( fh );
      }
   else
      SetDlgItemText( hwnd, IDD_LASTUPDATED, GS(IDS_STR606) );
}

/* This function fills the list box with the list of participants
 * of the specified conference.
 */
BOOL FASTCALL FillModeratorsList( HWND hwnd, PCL pcl )
{
   COMODOBJECT FAR * lpcm;
   COMODOBJECT cm;
   EXMODOBJECT FAR * lpem;
   EXMODOBJECT em;
   LPOB lpob;
   LPSTR lpModList;
   HCURSOR hOldCursor;
   HWND hwndList;
   char sz[ 100 ];
   int count;
   int index;

   /* Clear the listbox
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   ListBox_ResetContent( hwndList );

   /* Show wait cursor.
    */
   hOldCursor = SetCursor( hWaitCursor );

   /* Allocate space to read the list of moderators from
    * the database.
    */
   INITIALISE_PTR(lpModList);
   if( fNewMemory( &lpModList, 0xF000 ) )
      {
      LPSTR lpModListStart;

      lpModListStart = lpModList;
      if( Amdb_GetModeratorList( pcl, lpModList, 0xF000 ) )
         {
         SetWindowRedraw( hwndList, FALSE );
         while( *lpModList )
            {
            char szName[ LEN_INTERNETNAME+1 ];
            register int c;

            for( c = 0; *lpModList && *lpModList != ' '; ++c )
               szName[ c ] = *lpModList++;
            szName[ c ] = '\0';
            if( ( index = ListBox_AddString( hwndList, szName ) ) == LB_ERR )
               break;
            ListBox_SetItemData( hwndList, index, PLFLG_MOD );
            while( *lpModList == ' ' )
               ++lpModList;
            }
         SetWindowRedraw( hwndList, TRUE );
         }
      FreeMemory( &lpModListStart );
      }
   else
      OutOfMemoryError( hwnd, FALSE, FALSE );

   /* Find an out-basket object for updating the list of
    * moderators and add these to the listbox too.
    */
   Amob_New( OBTYPE_COMOD, &cm );
   Amob_CreateRefString( &cm.recConfName, (LPSTR)Amdb_GetFolderName( pcl ) );
   if( NULL != ( lpob = Amob_Find( OBTYPE_COMOD, &cm ) ) )
      do {
         OBINFO obinfo;
         LPSTR lp;
   
         Amob_GetObInfo( lpob, &obinfo );
         lpcm = obinfo.lpObData;
         lp = DRF(lpcm->recUserName);
         while( *lp )
            {
            char szName[ LEN_INTERNETNAME+1 ];
            register int c;
   
            for( c = 0; *lp && *lp != ' '; ++c )
               szName[ c ] = *lp++;
            szName[ c ] = '\0';
            if( ( index = ListBox_AddString( hwndList, szName ) ) == LB_ERR )
               break;
            ListBox_SetItemData( hwndList, index, PLFLG_COMOD );
            while( *lp == ' ' )
               ++lp;
            }
         }
      while( lpob = Amob_FindNext( lpob, OBTYPE_COMOD, &cm ) );
   Amob_Delete( OBTYPE_COMOD, &cm );

   /* Find an out-basket object for updating the list of
    * moderators and add these to the listbox too.
    */
   Amob_New( OBTYPE_EXMOD, &em );
   Amob_CreateRefString( &em.recConfName, (LPSTR)Amdb_GetFolderName( pcl ) );
   if( NULL != ( lpob = Amob_Find( OBTYPE_EXMOD, &em ) ) )
      do {
         OBINFO obinfo;
         LPSTR lp;
   
         Amob_GetObInfo( lpob, &obinfo );
         lpem = obinfo.lpObData;
         lp = DRF(lpem->recUserName);
         while( *lp )
            {
            char szName[ LEN_INTERNETNAME+1 ];
            register int c;
   
            for( c = 0; *lp && *lp != ' '; ++c )
               szName[ c ] = *lp++;
            szName[ c ] = '\0';
            if( ( index = ListBox_FindStringExact( hwndList, -1, szName ) ) == LB_ERR )
               break;
            ListBox_SetItemData( hwndList, index, PLFLG_EXMOD );
            while( *lp == ' ' )
               ++lp;
            }
         }
      while( lpob = Amob_FindNext( lpob, OBTYPE_EXMOD, &em ) );
   Amob_Delete( OBTYPE_EXMOD, &em );

   /* Show updated count and disable the listbox
    * if empty.
    */
   if( ( count = ListBox_GetCount( hwndList ) ) > 0 )
      wsprintf( sz, GS(IDS_STR603), count );
   else
      strcpy( sz, GS(IDS_STR101) );
   EnableWindow( hwndList, count > 0 );
   SetDlgItemText( hwnd, IDD_PAD1, sz );
   EnableControl( hwnd, IDD_REMOVE, FALSE );
   EnableControl( hwnd, IDD_RESUME, FALSE );

   /* Hide wait cursor.
    */
   SetCursor( hOldCursor );
   return( TRUE );
}

/* This function fills the list box with the list of participants
 * of the specified conference.
 */
BOOL FASTCALL FillParticipantsList( HWND hwnd, PCL pcl )
{
   char szFileName[ _MAX_FNAME ];
   FOLDERINFO confinfo;
   char sz[ 100 ];
   HWND hwndList;
   ADDPAROBJECT FAR * lpap;
   ADDPAROBJECT ap;
   REMPAROBJECT FAR * lprp;
   REMPAROBJECT rp;
   HCURSOR hOldCursor;
   HNDFILE fh;
   int count;
   int index;
   LPOB lpob;

   /* Clear the listbox
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   ListBox_ResetContent( hwndList );

   /* Show wait cursor.
    */
   hOldCursor = SetCursor( hWaitCursor );

   /* Open the participants list file. Some day all this crap will
    * be moved into the database manager itself.
    */
   Amdb_GetFolderInfo( pcl, &confinfo );
   wsprintf( szFileName, "%s.PAR", (LPSTR)confinfo.szFileName );
   if( ( fh = Ameol2_OpenFile( szFileName, DSD_PARLIST, AOF_READ ) ) != HNDFILE_ERROR )
      {
      LPLBF hBuffer;
      BOOL fError;

      hBuffer = Amlbf_Open( fh, AOF_READ );
      SetWindowRedraw( hwndList, FALSE );
      fError = FALSE;
      while( Amlbf_Read( hBuffer, sz, sizeof( sz ) - 2, NULL, NULL, &fIsAmeol2Scratchpad ) )
      {
         StripLeadingTrailingSpaces( sz );
         if( sz[0] != '\x0' ) // 2.56.2055
         {
            if( ( index = ListBox_AddString( hwndList, sz ) ) == LB_ERR )
               break;
            ListBox_SetItemData( hwndList, index, Amdb_IsModerator( pcl, sz ) ? PLFLG_MOD : PLFLG_PAR );
         }
      }
      SetWindowRedraw( hwndList, TRUE );
      if( fError )
         SetDlgItemText( hwnd, IDD_LASTUPDATED, GS(IDS_STR656) );
      Amlbf_Close( hBuffer );
      }

   /* Find an out-basket object for updating the list of
    * participants and add these to the listbox too.
    */
   Amob_New( OBTYPE_ADDPAR, &ap );
   Amob_CreateRefString( &ap.recConfName, (LPSTR)Amdb_GetFolderName( pcl ) );
   if( NULL != ( lpob = Amob_Find( OBTYPE_ADDPAR, &ap ) ) )
      do {
         OBINFO obinfo;
         LPSTR lp;
   
         Amob_GetObInfo( lpob, &obinfo );
         lpap = obinfo.lpObData;
         lp = DRF(lpap->recUserName);
         while( *lp )
            {
            char szName[ LEN_INTERNETNAME+1 ];
            register int c;
   
            for( c = 0; *lp && *lp != ' '; ++c )
               szName[ c ] = *lp++;
            szName[ c ] = '\0';
            if( ( index = ListBox_AddString( hwndList, szName ) ) == LB_ERR )
               break;
            ListBox_SetItemData( hwndList, index, PLFLG_ADDPAR );
            while( *lp == ' ' )
               ++lp;
            }
         }
      while( lpob = Amob_FindNext( lpob, OBTYPE_ADDPAR, &ap ) );
   Amob_Delete( OBTYPE_ADDPAR, &ap );

   /* Find an out-basket object for removing a list of
    * participants and add these to the listbox too.
    */
   Amob_New( OBTYPE_REMPAR, &rp );
   Amob_CreateRefString( &rp.recConfName, (LPSTR)Amdb_GetFolderName( pcl ) );
   if( NULL != ( lpob = Amob_Find( OBTYPE_REMPAR, &rp ) ) )
      do {
         OBINFO obinfo;
         LPSTR lp;
   
         Amob_GetObInfo( lpob, &obinfo );
         lprp = obinfo.lpObData;
         lp = DRF(lprp->recUserName);
         while( *lp )
            {
            char szName[ LEN_INTERNETNAME+1 ];
            register int c;
   
            for( c = 0; *lp && *lp != ' '; ++c )
               szName[ c ] = *lp++;
            szName[ c ] = '\0';
            if( ( index = ListBox_FindStringExact( hwndList, -1, szName ) ) == LB_ERR )
               break;
            ListBox_SetItemData( hwndList, index, PLFLG_REMPAR );
            while( *lp == ' ' )
               ++lp;
            }
         }
      while( lpob = Amob_FindNext( lpob, OBTYPE_REMPAR, &rp ) );
   Amob_Delete( OBTYPE_REMPAR, &rp );

   /* Show updated count and disable the listbox
    * if empty.
    */
   if( ( count = ListBox_GetCount( hwndList ) ) > 0 )
      wsprintf( sz, GS(IDS_STR604), count );
   else
      strcpy( sz, GS(IDS_STR903) );
   EnableWindow( hwndList, count > 0 );
   SetDlgItemText( hwnd, IDD_PAD1, sz );
   EnableControl( hwnd, IDD_REMOVE, FALSE );
   EnableControl( hwnd, IDD_RESUME, FALSE );

   /* Hide wait cursor.
    */
   SetCursor( hOldCursor );
   return( TRUE );
}

/* This function adds participants to the current
 * conference.
 */
BOOL FASTCALL ParList_AddPar( HWND hwnd, PCL pcl )
{
   if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_ADDPAR), AddPar, (LPARAM)(LPSTR)pcl ) )
      {
      BOOL fPrompt;

      fPrompt = FALSE;
      UpdateParList( hwnd, pcl, &fPrompt, FALSE );
      return( TRUE );
      }
   return( FALSE );
}

/* This function adds co-moderators to the current
 * conference.
 */
BOOL FASTCALL ParList_CoMod( HWND hwnd, PCL pcl )
{
   if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_COMOD), CoMod, (LPARAM)(LPSTR)pcl ) )
      {
      BOOL fPrompt;

      fPrompt = FALSE;
      UpdateParList( hwnd, pcl, &fPrompt, FALSE );
      return( TRUE );
      }
   return( FALSE );
}

/* This function removes co-moderators from the current
 * conference.
 */
BOOL FASTCALL ParList_ExMod( HWND hwnd, PCL pcl )
{
   char szUserName[ LEN_EXMODLIST+1 ];
   EXMODOBJECT emo;
   register int c;
   HWND hwndList;
   BOOL fPrompt;
   int cModsRemoved;

   /* Initialise the EXMOD structure.
    */
   Amob_New( OBTYPE_EXMOD, &emo );
   Amob_CreateRefString( &emo.recConfName, (LPSTR)Amdb_GetFolderName( pcl ) );

   /* Loop for each selected name from the list and create an EXMOD
    * structures for them. If the length of all selected names exceeds
    * LEN_EXMODLIST, then we need to create additional EXMOD structures
    * as needed. (CIX limit)
    */
   szUserName[ 0 ] = '\0';
   cModsRemoved = 0;
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   for( c = 0; c < ListBox_GetCount( hwndList ); ++c )
      if( ListBox_GetSel( hwndList, c ) )
         {
         char sz[ LEN_INTERNETNAME+1 ];

         ListBox_GetText( hwndList, c, sz );
         if( PLFLG_MOD == ListBox_GetItemData( hwndList, c ) )
            {
            if( strlen( szUserName ) + strlen( sz ) + 1 > LEN_EXMODLIST )
               {
               Amob_CreateRefString( &emo.recUserName, szUserName );
               Amob_Commit( NULL, OBTYPE_EXMOD, &emo );
               ++cModsRemoved;
               szUserName[ 0 ] = '\0';
               }
            if( *szUserName )
               strcat( szUserName, " " );
            strcat( szUserName, sz );
            }
         ListBox_SetSel( hwndList, FALSE, c );
         }

   /* Add this out-basket object.
    */
   if( *szUserName )
      {
      Amob_CreateRefString( &emo.recUserName, szUserName );
      Amob_Commit( NULL, OBTYPE_EXMOD, &emo );
      ++cModsRemoved;
      }
   Amob_Delete( OBTYPE_EXMOD, &emo );

   /* Update the participant list.
    */
   if( cModsRemoved )
      {
      fPrompt = FALSE;
      UpdateParList( hwnd, pcl, &fPrompt, FALSE );
      EnableControl( hwnd, IDD_REMOVE, FALSE );
      }

   /* If any moderators removed, show details.
    */
   if( *szUserName )
      fMessageBox( hwnd, 0, GS(IDS_STR99), MB_OK|MB_ICONEXCLAMATION );
   return( cModsRemoved > 0 );
}

/* This function removes participants from the current
 * conference.
 */
BOOL FASTCALL ParList_RemPar( HWND hwnd, PCL pcl )
{
   char szUserName[ LEN_REMPARLIST+1 ];
   EXMODOBJECT emo;
   register int c;
   HWND hwndList;
   BOOL fPrompt;
   int cParsRemoved;

   /* Initialise the REMPAR structure.
    */
   Amob_New( OBTYPE_REMPAR, &emo );
   Amob_CreateRefString( &emo.recConfName, (LPSTR)Amdb_GetFolderName( pcl ) );

   /* Loop for each selected name from the list and create an REMPAR
    * structures for them. If the length of all selected names exceeds
    * LEN_REMPARLIST, then we need to create additional REMPAR structures
    * as needed. (CIX limit)
    */
   szUserName[ 0 ] = '\0';
   cParsRemoved = 0;
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   for( c = 0; c < ListBox_GetCount( hwndList ); ++c )
      if( ListBox_GetSel( hwndList, c ) )
         {
         char sz[ LEN_INTERNETNAME+1 ];
         int type;

         ListBox_GetText( hwndList, c, sz );
         type = (int) ListBox_GetItemData( hwndList, c );
         if( PLFLG_PAR == type || PLFLG_MOD == type )
            {
            /* Ensure we don't rem par somebody who is being added or
             * removed from the list! :-)
             */
            if( strlen( szUserName ) + strlen( sz ) + 1 > LEN_REMPARLIST )
               {
               Amob_CreateRefString( &emo.recUserName, szUserName );
               Amob_Commit( NULL, OBTYPE_REMPAR, &emo );
               ++cParsRemoved;
               szUserName[ 0 ] = '\0';
               }
            if( *szUserName )
               strcat( szUserName, " " );
            strcat( szUserName, sz );
            }
         ListBox_SetSel( hwndList, FALSE, c );
         }

   /* Add this out-basket object.
    */
   if( *szUserName )
      {
      Amob_CreateRefString( &emo.recUserName, szUserName );
      Amob_Commit( NULL, OBTYPE_REMPAR, &emo );
      ++cParsRemoved;
      }
   Amob_Delete( OBTYPE_REMPAR, &emo );

   /* Update the participant list.
    */
   if( cParsRemoved )
      {
      fPrompt = FALSE;
      UpdateParList( hwnd, pcl, &fPrompt, FALSE );
      EnableControl( hwnd, IDD_REMOVE, FALSE );
      }

   /* If any participants removed, show details.
    */
   if( *szUserName )
      fMessageBox( hwnd, 0, GS(IDS_STR98), MB_OK|MB_ICONEXCLAMATION );
   return( cParsRemoved > 0 );
}


/* This function bans participants from the current
 * conference.
 */
void FASTCALL ParList_BanPar( HWND hwnd, PCL pcl )
{
   char szUserName[ LEN_CIXNAME+1 ];
   BANPAROBJECT bpo;
   register int c;
   HWND hwndList;
   WORD wSelCount;

   /* Initialise the BANPAR structure.
    */
   Amob_New( OBTYPE_BANPAR, &bpo );
   Amob_CreateRefString( &bpo.recConfName, (LPSTR)Amdb_GetFolderName( pcl ) );

   szUserName[ 0 ] = '\0';
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   wSelCount = ListBox_GetSelCount(hwndList);

   if (wSelCount < 1)
      return; // Shouldn't happen.

   if (wSelCount > 1)
      wsprintf( lpTmpBuf, GS(IDS_STR1261), wSelCount);
   else
   {
      int index = ListBox_GetCurSel( hwndList );
      ListBox_GetText( hwndList, index, szUserName);
      wsprintf( lpTmpBuf, GS(IDS_STR1262), szUserName);
   }
   if (fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION) == IDNO)
      return;

   for( c = 0; c < ListBox_GetCount( hwndList ); ++c )
      if( ListBox_GetSel( hwndList, c ) )
         {
         ListBox_GetText( hwndList, c, szUserName );
         Amob_CreateRefString( &bpo.recUserName, szUserName );
         Amob_Commit( NULL, OBTYPE_BANPAR, &bpo );
         ListBox_SetSel( hwndList, FALSE, c );
         }

   Amob_Delete( OBTYPE_BANPAR, &bpo );
   fMessageBox( hwnd, 0, GS(IDS_STR1260), MB_OK|MB_ICONEXCLAMATION );
}

/* This is the Get Participants List out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_GetParList( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   GETPARLISTOBJECT FAR * lpgpl;

   lpgpl = (GETPARLISTOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR214), DRF(lpgpl->recConfName) );
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( 0 );

      case OBEVENT_NEW:
         _fmemset( lpgpl, 0, sizeof( GETPARLISTOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         GETPARLISTOBJECT gpl;
      
         INITIALISE_PTR(lpgpl);
         Amob_LoadDataObject( fh, &gpl, sizeof( GETPARLISTOBJECT ) );
         if( fNewMemory( &lpgpl, sizeof( GETPARLISTOBJECT ) ) )
            {
            gpl.recConfName.hpStr = NULL;
            *lpgpl = gpl;
            }
         return( (LRESULT)lpgpl );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpgpl->recConfName );
         return( Amob_SaveDataObject( fh, lpgpl, sizeof( GETPARLISTOBJECT ) ) );

      case OBEVENT_COPY: {
         GETPARLISTOBJECT FAR * lpcoNew;
      
         INITIALISE_PTR( lpcoNew );
         lpgpl = (GETPARLISTOBJECT FAR *)dwData;
         if( fNewMemory( &lpcoNew, sizeof( GETPARLISTOBJECT ) ) )
            {
            INITIALISE_PTR( lpcoNew->recConfName.hpStr );
            Amob_CopyRefObject( &lpcoNew->recConfName, &lpgpl->recConfName );
            }
         return( (LRESULT)lpcoNew );
         }

      case OBEVENT_FIND: {
         GETPARLISTOBJECT FAR * lpgpl1;
         GETPARLISTOBJECT FAR * lpgpl2;

         lpgpl1 = (GETPARLISTOBJECT FAR *)dwData;
         lpgpl2 = (GETPARLISTOBJECT FAR *)lpBuf;
         return( strcmp( DRF(lpgpl1->recConfName), DRF(lpgpl2->recConfName) ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpgpl );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpgpl->recConfName );
         return( TRUE );
      }
   return( 0L );
}

/* This is the Add Participants out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_AddPar( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   ADDPAROBJECT FAR * lpap;

   lpap = (ADDPAROBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR215), DRF(lpap->recUserName), DRF(lpap->recConfName) );
         return( TRUE );

      case OBEVENT_NEW:
         _fmemset( lpap, 0, sizeof( ADDPAROBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         ADDPAROBJECT ap;
      
         INITIALISE_PTR(lpap);
         Amob_LoadDataObject( fh, &ap, sizeof( ADDPAROBJECT ) );
         if( fNewMemory( &lpap, sizeof( ADDPAROBJECT ) ) )
            {
            ap.recConfName.hpStr = NULL;
            ap.recUserName.hpStr = NULL;
            *lpap = ap;
            }
         return( (LRESULT)lpap );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpap->recUserName );
         Amob_SaveRefObject( &lpap->recConfName );
         return( Amob_SaveDataObject( fh, lpap, sizeof( ADDPAROBJECT ) ) );

      case OBEVENT_COPY: {
         ADDPAROBJECT FAR * lpapNew;
      
         INITIALISE_PTR( lpapNew );
         lpap = (ADDPAROBJECT FAR *)dwData;
         if( fNewMemory( &lpapNew, sizeof( ADDPAROBJECT ) ) )
            {
            INITIALISE_PTR( lpapNew->recConfName.hpStr );
            INITIALISE_PTR( lpapNew->recUserName.hpStr );
            Amob_CopyRefObject( &lpapNew->recConfName, &lpap->recConfName );
            Amob_CopyRefObject( &lpapNew->recUserName, &lpap->recUserName );
            }
         return( (LRESULT)lpapNew );
         }

      case OBEVENT_FIND: {
         ADDPAROBJECT FAR * lpap1;
         ADDPAROBJECT FAR * lpap2;

         lpap1 = (ADDPAROBJECT FAR *)dwData;
         lpap2 = (ADDPAROBJECT FAR *)lpBuf;
         return( strcmp( DRF(lpap1->recConfName), DRF(lpap2->recConfName) ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpap );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpap->recConfName );
         Amob_FreeRefObject( &lpap->recUserName );
         return( TRUE );
      }
   return( 0L );
}

/* This is the Remove Participants out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_RemPar( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   REMPAROBJECT FAR * lprp;

   lprp = (REMPAROBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR216), DRF(lprp->recUserName), DRF(lprp->recConfName) );
         return( TRUE );

      case OBEVENT_NEW:
         _fmemset( lprp, 0, sizeof( REMPAROBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         REMPAROBJECT rp;
      
         INITIALISE_PTR(lprp);
         Amob_LoadDataObject( fh, &rp, sizeof( REMPAROBJECT ) );
         if( fNewMemory( &lprp, sizeof( REMPAROBJECT ) ) )
            {
            rp.recConfName.hpStr = NULL;
            rp.recUserName.hpStr = NULL;
            *lprp = rp;
            }
         return( (LRESULT)lprp );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lprp->recUserName );
         Amob_SaveRefObject( &lprp->recConfName );
         return( Amob_SaveDataObject( fh, lprp, sizeof( REMPAROBJECT ) ) );

      case OBEVENT_COPY: {
         REMPAROBJECT FAR * lprpNew;
      
         INITIALISE_PTR( lprpNew );
         lprp = (REMPAROBJECT FAR *)dwData;
         if( fNewMemory( &lprpNew, sizeof( REMPAROBJECT ) ) )
            {
            INITIALISE_PTR( lprpNew->recConfName.hpStr );
            INITIALISE_PTR( lprpNew->recUserName.hpStr );
            Amob_CopyRefObject( &lprpNew->recConfName, &lprp->recConfName );
            Amob_CopyRefObject( &lprpNew->recUserName, &lprp->recUserName );
            }
         return( (LRESULT)lprpNew );
         }

      case OBEVENT_FIND: {
         REMPAROBJECT FAR * lprp1;
         REMPAROBJECT FAR * lprp2;

         lprp1 = (REMPAROBJECT FAR *)dwData;
         lprp2 = (REMPAROBJECT FAR *)lpBuf;
         return( strcmp( DRF(lprp1->recConfName), DRF(lprp2->recConfName) ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lprp );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lprp->recConfName );
         Amob_FreeRefObject( &lprp->recUserName );
         return( TRUE );
      }
   return( 0L );
}


/* This is the Ban Participants out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_BanPar( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   BANPAROBJECT FAR * lpap;

   lpap = (BANPAROBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR1258), DRF(lpap->recUserName), DRF(lpap->recConfName) );
         return( TRUE );

      case OBEVENT_NEW:
         _fmemset( lpap, 0, sizeof( BANPAROBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         BANPAROBJECT ap;
      
         INITIALISE_PTR(lpap);
         Amob_LoadDataObject( fh, &ap, sizeof( BANPAROBJECT ) );
         if( fNewMemory( &lpap, sizeof( BANPAROBJECT ) ) )
            {
            ap.recConfName.hpStr = NULL;
            ap.recUserName.hpStr = NULL;
            *lpap = ap;
            }
         return( (LRESULT)lpap );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpap->recUserName );
         Amob_SaveRefObject( &lpap->recConfName );
         return( Amob_SaveDataObject( fh, lpap, sizeof( BANPAROBJECT ) ) );

      case OBEVENT_COPY: {
         BANPAROBJECT FAR * lpapNew;
      
         INITIALISE_PTR( lpapNew );
         lpap = (BANPAROBJECT FAR *)dwData;
         if( fNewMemory( &lpapNew, sizeof( BANPAROBJECT ) ) )
            {
            INITIALISE_PTR( lpapNew->recConfName.hpStr );
            INITIALISE_PTR( lpapNew->recUserName.hpStr );
            Amob_CopyRefObject( &lpapNew->recConfName, &lpap->recConfName );
            Amob_CopyRefObject( &lpapNew->recUserName, &lpap->recUserName );
            }
         return( (LRESULT)lpapNew );
         }

      case OBEVENT_FIND: {
         BANPAROBJECT FAR * lpap1;
         BANPAROBJECT FAR * lpap2;

         lpap1 = (BANPAROBJECT FAR *)dwData;
         lpap2 = (BANPAROBJECT FAR *)lpBuf;
         return( strcmp( DRF(lpap1->recConfName), DRF(lpap2->recConfName) ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpap );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpap->recConfName );
         Amob_FreeRefObject( &lpap->recUserName );
         return( TRUE );
      }
   return( 0L );
}

/* This is the Unban Participants out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_UnBanPar( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   BANPAROBJECT FAR * lpap;

   lpap = (BANPAROBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR1259), DRF(lpap->recUserName), DRF(lpap->recConfName) );
         return( TRUE );

      case OBEVENT_NEW:
         _fmemset( lpap, 0, sizeof( BANPAROBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         BANPAROBJECT ap;
      
         INITIALISE_PTR(lpap);
         Amob_LoadDataObject( fh, &ap, sizeof( BANPAROBJECT ) );
         if( fNewMemory( &lpap, sizeof( BANPAROBJECT ) ) )
            {
            ap.recConfName.hpStr = NULL;
            ap.recUserName.hpStr = NULL;
            *lpap = ap;
            }
         return( (LRESULT)lpap );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpap->recUserName );
         Amob_SaveRefObject( &lpap->recConfName );
         return( Amob_SaveDataObject( fh, lpap, sizeof( BANPAROBJECT ) ) );

      case OBEVENT_COPY: {
         BANPAROBJECT FAR * lpapNew;
      
         INITIALISE_PTR( lpapNew );
         lpap = (BANPAROBJECT FAR *)dwData;
         if( fNewMemory( &lpapNew, sizeof( BANPAROBJECT ) ) )
            {
            INITIALISE_PTR( lpapNew->recConfName.hpStr );
            INITIALISE_PTR( lpapNew->recUserName.hpStr );
            Amob_CopyRefObject( &lpapNew->recConfName, &lpap->recConfName );
            Amob_CopyRefObject( &lpapNew->recUserName, &lpap->recUserName );
            }
         return( (LRESULT)lpapNew );
         }

      case OBEVENT_FIND: {
         BANPAROBJECT FAR * lpap1;
         BANPAROBJECT FAR * lpap2;

         lpap1 = (BANPAROBJECT FAR *)dwData;
         lpap2 = (BANPAROBJECT FAR *)lpBuf;
         return( strcmp( DRF(lpap1->recConfName), DRF(lpap2->recConfName) ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpap );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpap->recConfName );
         Amob_FreeRefObject( &lpap->recUserName );
         return( TRUE );
      }
   return( 0L );
}

/* This is the Add Co-Moderator out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_CoMod( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   COMODOBJECT FAR * lpcm;

   lpcm = (COMODOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR217), DRF(lpcm->recUserName), DRF(lpcm->recConfName) );
         return( TRUE );

      case OBEVENT_NEW:
         _fmemset( lpcm, 0, sizeof( COMODOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         COMODOBJECT cm;
      
         INITIALISE_PTR(lpcm);
         Amob_LoadDataObject( fh, &cm, sizeof( COMODOBJECT ) );
         if( fNewMemory( &lpcm, sizeof( COMODOBJECT ) ) )
            {
            cm.recConfName.hpStr = NULL;
            cm.recUserName.hpStr = NULL;
            *lpcm = cm;
            }
         return( (LRESULT)lpcm );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpcm->recUserName );
         Amob_SaveRefObject( &lpcm->recConfName );
         return( Amob_SaveDataObject( fh, lpcm, sizeof( COMODOBJECT ) ) );

      case OBEVENT_COPY: {
         COMODOBJECT FAR * lpcmNew;
      
         INITIALISE_PTR( lpcmNew );
         lpcm = (COMODOBJECT FAR *)dwData;
         if( fNewMemory( &lpcmNew, sizeof( COMODOBJECT ) ) )
            {
            INITIALISE_PTR( lpcmNew->recConfName.hpStr );
            INITIALISE_PTR( lpcmNew->recUserName.hpStr );
            Amob_CopyRefObject( &lpcmNew->recConfName, &lpcm->recConfName );
            Amob_CopyRefObject( &lpcmNew->recUserName, &lpcm->recUserName );
            }
         return( (LRESULT)lpcmNew );
         }

      case OBEVENT_FIND: {
         COMODOBJECT FAR * lpcm1;
         COMODOBJECT FAR * lpcm2;

         lpcm1 = (COMODOBJECT FAR *)dwData;
         lpcm2 = (COMODOBJECT FAR *)lpBuf;
         return( strcmp( DRF(lpcm1->recConfName), DRF(lpcm2->recConfName) ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpcm );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpcm->recConfName );
         Amob_FreeRefObject( &lpcm->recUserName );
         return( TRUE );
      }
   return( 0L );
}

/* This is the Remove Co-Moderator out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_ExMod( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   EXMODOBJECT FAR * lpem;

   lpem = (EXMODOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR218), DRF(lpem->recUserName), DRF(lpem->recConfName) );
         return( TRUE );

      case OBEVENT_NEW:
         _fmemset( lpem, 0, sizeof( EXMODOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         EXMODOBJECT em;
      
         INITIALISE_PTR(lpem);
         Amob_LoadDataObject( fh, &em, sizeof( EXMODOBJECT ) );
         if( fNewMemory( &lpem, sizeof( EXMODOBJECT ) ) )
            {
            em.recConfName.hpStr = NULL;
            em.recUserName.hpStr = NULL;
            *lpem = em;
            }
         return( (LRESULT)lpem );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpem->recUserName );
         Amob_SaveRefObject( &lpem->recConfName );
         return( Amob_SaveDataObject( fh, lpem, sizeof( EXMODOBJECT ) ) );

      case OBEVENT_COPY: {
         EXMODOBJECT FAR * lpemNew;
      
         INITIALISE_PTR( lpemNew );
         lpem = (EXMODOBJECT FAR *)dwData;
         if( fNewMemory( &lpemNew, sizeof( EXMODOBJECT ) ) )
            {
            INITIALISE_PTR( lpemNew->recConfName.hpStr );
            INITIALISE_PTR( lpemNew->recUserName.hpStr );
            Amob_CopyRefObject( &lpemNew->recConfName, &lpem->recConfName );
            Amob_CopyRefObject( &lpemNew->recUserName, &lpem->recUserName );
            }
         return( (LRESULT)lpemNew );
         }

      case OBEVENT_FIND: {
         EXMODOBJECT FAR * lpem1;
         EXMODOBJECT FAR * lpem2;

         lpem1 = (EXMODOBJECT FAR *)dwData;
         lpem2 = (EXMODOBJECT FAR *)lpBuf;
         return( strcmp( DRF(lpem1->recConfName), DRF(lpem2->recConfName) ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpem );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpem->recConfName );
         Amob_FreeRefObject( &lpem->recUserName );
         return( TRUE );
      }
   return( 0L );
}

/* This function handles the CoMod dialog box
 */
BOOL EXPORT CALLBACK CoMod( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = CoMod_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the Login
 * dialog.
 */
LRESULT FASTCALL CoMod_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, CoMod_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, CoMod_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsCOMOD );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL CoMod_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   Edit_LimitText( GetDlgItem( hwnd, IDD_EDIT ), LEN_INTERNETNAME );
   EnableControl( hwnd, IDOK, FALSE );
   EnableControl( hwnd, IDD_ADD, FALSE );
   EnableControl( hwnd, IDD_REMOVE, FALSE );
   SetWindowLong( hwnd, DWL_USER, lParam );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL CoMod_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   char sz[ LEN_INTERNETNAME+1 ];

   switch( id )
      {
      case IDD_EDIT:
         if( codeNotify == EN_CHANGE )
            PostMessage( hwnd, WM_COMMAND, IDD_PAD1, 0L );
         break;

      /* This is NOT a control, but is called indirectly from IDD_EDIT as
       * trying to alter the edit control during EN_CHANGE processing causes
       * Windows to crash!
       */
      case IDD_PAD1: {
         HWND hwndEdit;
         HWND hwndList;
         HWND hwndAdd;

         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         Edit_GetText( hwndEdit, sz, LEN_INTERNETNAME+1 );
         EnableControl( hwnd, IDD_ADD, *sz != '\0' );
         if( *sz )
            {
            hwndAdd = GetDlgItem( hwnd, IDD_ADD );
            if( ListBox_FindStringExact( hwndList, -1, sz ) != LB_ERR )
               {
               char * psz;

               GetWindowText( hwndAdd, sz, 20 );
               psz = GS(IDS_STR600);
               if( _strcmpi( sz, psz ) )
                  SetWindowText( hwndAdd, psz );
               }
            else
               {
               char * psz;

               GetWindowText( hwndAdd, sz, 20 );
               psz = GS(IDS_STR599);
               if( _strcmpi( sz, psz ) )
                  SetWindowText( hwndAdd, psz );
               }
            ChangeDefaultButton( hwnd, IDD_ADD );
            }
         break;
         }

      case IDD_REMOVE: {
         HWND hwndList;
         int index;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         if( ( index = ListBox_GetCurSel( hwndList ) ) != LB_ERR )
            {
            if( index == ListBox_DeleteString( hwndList, index ) )
               --index;
            ListBox_SetCurSel( hwndList, index );
            if( index == LB_ERR )
               {
               EnableControl( hwnd, IDD_REMOVE, FALSE );
               EnableControl( hwnd, IDOK, FALSE );
               }
            }
         break;
         }

      case IDD_ADD: {
         HWND hwndEdit;
         HWND hwndList;

         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         Edit_GetText( hwndEdit, sz, LEN_INTERNETNAME+1 );
         if( ListBox_FindStringExact( hwndList, -1, sz ) == LB_ERR )
            {
            int index;

            index = ListBox_AddString( hwndList, sz );
            ListBox_SetCurSel( hwndList, index );
            }
         Edit_SetText( hwndEdit, "" );
         EnableControl( hwnd, IDD_ADD, FALSE );
         EnableControl( hwnd, IDOK, TRUE );
         EnableControl( hwnd, IDD_REMOVE, TRUE );
         ChangeDefaultButton( hwnd, IDOK );
         break;
         }

      case IDOK: {
         LPSTR lpCoModList;
         LPVOID pcl;
         HWND hwndList;
         HWND hwndEdit;

         INITIALISE_PTR(lpCoModList);

         /* Initialise.
          */
         pcl = (LPVOID)GetWindowLong( hwnd, DWL_USER );
         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         hwndList = GetDlgItem( hwnd, IDD_LIST );

         /* If anything in the edit field, add it to the list
          * box unless already there.
          */
         if( Edit_GetTextLength( hwndEdit ) )
            {
            Edit_GetText( hwndEdit, sz, LEN_INTERNETNAME+1 );
            if( ListBox_FindStringExact( hwndList, -1, sz ) == LB_ERR )
               ListBox_AddString( hwndList, sz );
            }

         /* Now create a large memory block to contain all the names
          * in the list box and pass it to the function that actually
          * creates the COMOD structures.
          */
         lpCoModList = GetParticipants( hwnd, FALSE );
         if( NULL != lpCoModList )
            {
            /* Call the function that adds the list of names to
             * the out-basket.
             */
            AddCoModerators( hwnd, pcl, lpCoModList );
            FreeMemory( &lpCoModList );
            }
         else
            {
            OutOfMemoryError( hwnd, FALSE, FALSE );
            break;
            }
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* This function handles the AddPar dialog box
 */
BOOL EXPORT CALLBACK AddPar( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = AddPar_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the Login
 * dialog.
 */
LRESULT FASTCALL AddPar_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, AddPar_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, AddPar_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsADDPAR );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL AddPar_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   Edit_LimitText( GetDlgItem( hwnd, IDD_EDIT ), LEN_INTERNETNAME );
   EnableControl( hwnd, IDOK, FALSE );
   EnableControl( hwnd, IDD_ADD, FALSE );
   EnableControl( hwnd, IDD_REMOVE, FALSE );
   SetWindowLong( hwnd, DWL_USER, lParam );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL AddPar_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   char sz[ LEN_INTERNETNAME+1 ];

   switch( id )
      {
      case IDD_EDIT:
         if( codeNotify == EN_CHANGE )
            PostMessage( hwnd, WM_COMMAND, IDD_PAD1, 0L );
         break;

      /* This is NOT a control, but is called indirectly from IDD_EDIT as
       * trying to alter the edit control during EN_CHANGE processing causes
       * Windows to crash!
       */
      case IDD_PAD1: {
         HWND hwndEdit;
         HWND hwndList;
         HWND hwndAdd;

         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         Edit_GetText( hwndEdit, sz, LEN_INTERNETNAME+1 );
         EnableControl( hwnd, IDD_ADD, *sz != '\0' );
         if( *sz )
            {
            hwndAdd = GetDlgItem( hwnd, IDD_ADD );
            if( ListBox_FindStringExact( hwndList, -1, sz ) != LB_ERR )
               {
               char * psz;

               GetWindowText( hwndAdd, sz, 20 );
               psz = GS(IDS_STR600);
               if( _strcmpi( sz, psz ) )
                  SetWindowText( hwndAdd, psz );
               }
            else
               {
               char * psz;

               GetWindowText( hwndAdd, sz, 20 );
               psz = GS(IDS_STR599);
               if( _strcmpi( sz, psz ) )
                  SetWindowText( hwndAdd, psz );
               }
            ChangeDefaultButton( hwnd, IDD_ADD );
            }
         break;
         }

      case IDD_REMOVE: {
         HWND hwndList;
         int index;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         if( ( index = ListBox_GetCurSel( hwndList ) ) != LB_ERR )
            {
            if( index == ListBox_DeleteString( hwndList, index ) )
               --index;
            ListBox_SetCurSel( hwndList, index );
            if( index == LB_ERR )
               {
               EnableControl( hwnd, IDD_REMOVE, FALSE );
               EnableControl( hwnd, IDOK, FALSE );
               }
            }
         break;
         }

      case IDD_ADD: {
         HWND hwndEdit;
         HWND hwndList;

         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         Edit_GetText( hwndEdit, sz, LEN_INTERNETNAME+1 );
         if( ListBox_FindStringExact( hwndList, -1, sz ) == LB_ERR )
            {
            int index;

            index = ListBox_AddString( hwndList, sz );
            ListBox_SetCurSel( hwndList, index );
            }
         Edit_SetText( hwndEdit, "" );
         EnableControl( hwnd, IDD_ADD, FALSE );
         EnableControl( hwnd, IDOK, TRUE );
         EnableControl( hwnd, IDD_REMOVE, TRUE );
         ChangeDefaultButton( hwnd, IDOK );
         break;
         }

      case IDOK: {
         LPSTR lpAddParList;
         LPVOID pcl;
         HWND hwndList;
         HWND hwndEdit;

         INITIALISE_PTR(lpAddParList);

         /* Initialise.
          */
         pcl = (LPVOID)GetWindowLong( hwnd, DWL_USER );
         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         hwndList = GetDlgItem( hwnd, IDD_LIST );

         /* If anything in the edit field, add it to the list
          * box unless already there.
          */
         if( Edit_GetTextLength( hwndEdit ) )
            {
            Edit_GetText( hwndEdit, sz, LEN_INTERNETNAME+1 );
            if( ListBox_FindStringExact( hwndList, -1, sz ) == LB_ERR )
               ListBox_AddString( hwndList, sz );
            }

         /* Now create a large memory block to contain all the names
          * in the list box and pass it to the function that actually
          * creates the ADDPAR structures.
          */
         lpAddParList = GetParticipants( hwnd, FALSE );
         if( NULL != lpAddParList )
            {
            /* Call the function that adds the list of names to
             * the out-basket.
             */
            AddParticipants( hwnd, pcl, lpAddParList );
            FreeMemory( &lpAddParList );
            }
         else
            {
            OutOfMemoryError( hwnd, FALSE, FALSE );
            break;
            }
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* This function returns a memory block containing the
 * participants as a whitespace delimited string.
 */
LPSTR FASTCALL GetParticipants( HWND hwndParList, BOOL fSelected )
{
   LPSTR lpParList;

   INITIALISE_PTR(lpParList);
   if( NULL != hwndParList )
      {
      HWND hwndList;
      register int c;
      UINT cbSize;

      /* First count the size of the list.
       */
      cbSize = 0;
      VERIFY( hwndList = GetDlgItem( hwndParList, IDD_LIST ) );
      for( c = 0; c < ListBox_GetCount( hwndList ); ++c )
         if( !fSelected || ListBox_GetSel( hwndList, c ) )
            cbSize += ListBox_GetTextLen( hwndList, c ) + 1;
      if( cbSize > 0 && fNewMemory( &lpParList, cbSize ) )
         {
         LPSTR lp;

         /* Gather ye apricots while ye may...
          */
         lp = lpParList;
         for( c = 0; c < ListBox_GetCount( hwndList ); ++c )
            if( !fSelected || ListBox_GetSel( hwndList, c ) )
               {
               if( lp > lpParList )
                  *lp++ = ' ';
               ListBox_GetText( hwndList, c, lp );
               lp += strlen( lp );
               }
         }
      }
   return( lpParList );
}

/* This function adds the specified list of space delimited names as
 * participants.
 */
void FASTCALL AddParticipants( HWND hwnd, PCL pcl, LPSTR lpParList )
{
   char szUserName[ LEN_ADDPARLIST+1 ];
   ADDPAROBJECT cm;

   /* Initialise the structure.
    */
   Amob_New( OBTYPE_ADDPAR, &cm );
   Amob_CreateRefString( &cm.recConfName, (LPSTR)Amdb_GetFolderName( pcl ) );
   szUserName[ 0 ] = '\0';

   /* Loop for each name.
    */
   while( strlen( lpParList ) > LEN_ADDPARLIST )
      {
      LPSTR lp;

      lp = lpParList + LEN_ADDPARLIST;
      while( lp > lpParList && *lp && *lp != ' ' )
         --lp;
      *lp++ = '\0';
      Amob_CreateRefString( &cm.recUserName, lpParList );
      Amob_Commit( NULL, OBTYPE_ADDPAR, &cm );
      lpParList = lp;
      }

   /* Add the last chunk to the out-basket.
    */
   Amob_CreateRefString( &cm.recUserName, lpParList );
   Amob_Commit( NULL, OBTYPE_ADDPAR, &cm );
   Amob_Delete( OBTYPE_ADDPAR, &cm );
}

/* This function removes the specified list of space delimited names as
 * co-moderators.
 */
void FASTCALL ExmodModerators( HWND hwnd, PCL pcl, LPSTR lpCoModList )
{
   char szUserName[ LEN_EXMODLIST+1 ];
   EXMODOBJECT em;

   /* Initialise the structure.
    */
   Amob_New( OBTYPE_EXMOD, &em );
   Amob_CreateRefString( &em.recConfName, (LPSTR)Amdb_GetFolderName( pcl ) );
   szUserName[ 0 ] = '\0';

   /* Loop for each name.
    */
   while( strlen( lpCoModList ) > LEN_EXMODLIST )
      {
      LPSTR lp;

      lp = lpCoModList + LEN_EXMODLIST;
      while( lp > lpCoModList && *lp && *lp != ' ' )
         --lp;
      *lp++ = '\0';
      Amob_CreateRefString( &em.recUserName, lpCoModList );
      Amob_Commit( NULL, OBTYPE_EXMOD, &em );
      lpCoModList = lp;
      }

   /* Add the last chunk to the out-basket.
    */
   Amob_CreateRefString( &em.recUserName, lpCoModList );
   Amob_Commit( NULL, OBTYPE_EXMOD, &em );
   Amob_Delete( OBTYPE_EXMOD, &em );
}

/* This function adds the specified list of space delimited names as
 * co-moderators.
 */
void FASTCALL AddCoModerators( HWND hwnd, PCL pcl, LPSTR lpCoModList )
{
   char szUserName[ LEN_COMODLIST+1 ];
   COMODOBJECT cm;

   /* Initialise the structure.
    */
   Amob_New( OBTYPE_COMOD, &cm );
   Amob_CreateRefString( &cm.recConfName, (LPSTR)Amdb_GetFolderName( pcl ) );
   szUserName[ 0 ] = '\0';

   /* Loop for each name.
    */
   while( strlen( lpCoModList ) > LEN_COMODLIST )
      {
      LPSTR lp;

      lp = lpCoModList + LEN_COMODLIST;
      while( lp > lpCoModList && *lp && *lp != ' ' )
         --lp;
      *lp++ = '\0';
      Amob_CreateRefString( &cm.recUserName, lpCoModList );
      Amob_Commit( NULL, OBTYPE_COMOD, &cm );
      lpCoModList = lp;
      }

   /* Add the last chunk to the out-basket.
    */
   Amob_CreateRefString( &cm.recUserName, lpCoModList );
   Amob_Commit( NULL, OBTYPE_COMOD, &cm );
   Amob_Delete( OBTYPE_COMOD, &cm );
}

/* This command handles the Print command in the resume window.
 */
void FASTCALL CmdParlistPrint( HWND hwnd )
{
   LPPARLISTWNDINFO lppli;
   char sz[ 60 ];
   HPRINT hPr;
   BOOL fOk;

   /* MUST have a printer driver.
    */
   if( !IsPrinterDriver() )
      return;

   /* Get participants list window handle.
    */
   lppli = GetParlistWndInfo( hwnd );

   /* Show and get the print terminal dialog box.
    */
   if( !Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_PRINTPARLIST), PrintParlistDlg, (LPARAM)lppli ) )
      return;

   /* Start printing.
    */
   GetWindowText( hwnd, sz, sizeof(sz) );
   fOk = TRUE;
   if( NULL != ( hPr = BeginPrint( hwnd, sz, &lfFixedFont ) ) )
      {
      register int c;

      for( c = 0; fOk && c < cCopies; ++c )
         {
         ResetPageNumber( hPr );
         fOk = BeginPrintPage( hPr );
         if( fPrintParts )
            fOk = PrintParticipantsList( hwnd, hPr );
         if( fPrintMods )
            fOk = PrintModeratorsList( hwnd, hPr );
         if( fOk )
            EndPrintPage( hPr );
         }
      EndPrint( hPr );
      }
}

/* This function prints the list of participants in the
 * specified conference.
 */
BOOL FASTCALL PrintParticipantsList( HWND hwnd, HPRINT hPr )
{
   LPPARLISTWNDINFO lppli;
   BOOL fOk;

   /* Get participants list window handle.
    */
   lppli = GetParlistWndInfo( hwnd );
   wsprintf( lpTmpBuf, GS(IDS_STR1039), Amdb_GetFolderName( lppli->pcl ) );
   if( fOk = PrintLine( hPr, lpTmpBuf ) )
      if( fOk = PrintLine( hPr, "" ) )
         {
         char szFileName[ 13 ];
         FOLDERINFO confinfo;
         HNDFILE fh;

         /* Open the PAR file associated with this folder.
          */
         Amdb_GetFolderInfo( lppli->pcl, &confinfo );
         wsprintf( szFileName, "%s.PAR", (LPSTR)confinfo.szFileName );
         if( ( fh = Ameol2_OpenFile( szFileName, DSD_PARLIST, AOF_READ ) ) != HNDFILE_ERROR )
            {
            LPLBF hBuffer;
            char sz[ 40 ];

            /* Loop for each line in the file and print the
             * names found.
             */
            hBuffer = Amlbf_Open( fh, AOF_READ );
            while( fOk && Amlbf_Read( hBuffer, sz, sizeof( sz ) - 2, NULL, NULL, &fIsAmeol2Scratchpad ) )
               {
               StripLeadingTrailingSpaces( sz );
               fOk = PrintLine( hPr, sz );
               }
            Amlbf_Close( hBuffer );
            fOk = PrintLine( hPr, "" );
            }
         }
   return( fOk );
}

/* This function prints the list of moderators in the
 * specified conference.
 */
BOOL FASTCALL PrintModeratorsList( HWND hwnd, HPRINT hPr )
{
   LPPARLISTWNDINFO lppli;
   BOOL fOk;

   /* Get participants list window handle.
    */
   lppli = GetParlistWndInfo( hwnd );
   wsprintf( lpTmpBuf, GS(IDS_STR1040), Amdb_GetFolderName( lppli->pcl ) );
   if( fOk = PrintLine( hPr, lpTmpBuf ) )
      if( fOk = PrintLine( hPr, "" ) )
         {
         LPSTR lpModList;

         /* Read the space delimited list of moderators
          * from the conference.
          */
         INITIALISE_PTR(lpModList);
         if( fNewMemory( &lpModList, 0xFFFF ) )
            {
            LPSTR lpModListStart;

            lpModListStart = lpModList;
            if( Amdb_GetModeratorList( lppli->pcl, lpModList, 0xFFFF ) )
               while( fOk && *lpModList )
                  {
                  char szName[ LEN_INTERNETNAME+1 ];
                  register int c;

                  /* Extract each name and print it.
                   */
                  for( c = 0; *lpModList && *lpModList != ' '; ++c )
                     szName[ c ] = *lpModList++;
                  szName[ c ] = '\0';
                  fOk = PrintLine( hPr, szName );
                  while( *lpModList == ' ' )
                     ++lpModList;
                  }
            FreeMemory( &lpModListStart );
            fOk = PrintLine( hPr, "" );
            }
         }
   return( fOk );
}

/* This function handles the Print dialog.
 */
BOOL EXPORT CALLBACK PrintParlistDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = PrintParlistDlg_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the Print Message
 * dialog.
 */
LRESULT FASTCALL PrintParlistDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, PrintParlistDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, PrintParlistDlg_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPRINTPARLIST );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL PrintParlistDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPPARLISTWNDINFO lppli;
   HWND hwndSpin;
   HWND hwndTab;

   /* Get participant list record.
    */
   lppli = (LPPARLISTWNDINFO)lParam;

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
   hwndTab = GetDlgItem( hwndParlist, IDD_TABS );
   if( TabCtrl_GetCurSel( hwndTab ) == 1 )
      CheckDlgButton( hwnd, IDD_MODS, TRUE );
   else
      CheckDlgButton( hwnd, IDD_PARTS, TRUE );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL PrintParlistDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
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
         fPrintParts = IsDlgButtonChecked( hwnd, IDD_PARTS );
         fPrintMods = IsDlgButtonChecked( hwnd, IDD_MODS );
         EndDialog( hwnd, TRUE );
         break;

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}

/* This function handles the Find in participants list
 * command.
 */
void FASTCALL CmdFindInParlist( HWND hwnd )
{
   Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_FINDINPARLIST), FindInParlistDlg, 0L );
}

/* Handles the Find In Parlist dialog.
 */
BOOL EXPORT CALLBACK FindInParlistDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, FindInParlistDlg_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the About dialog.
 */
LRESULT FASTCALL FindInParlistDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, FindInParlistDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, FindInParlistDlg_OnCommand );

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_COMMAND message. Currently only one control
 * on the dialog, the OK button, dispatches this message.
 */ 
void FASTCALL FindInParlistDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   static BOOL fSearching = FALSE;
   static BOOL fError = FALSE;

   switch( id )
      {
      case IDD_EDIT:
         if( codeNotify == EN_CHANGE )
            EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndCtl ) > 0 );
         break;

      case IDD_LIST:
         if( codeNotify == LBN_SELCHANGE )
            EnableControl( hwnd, IDD_REMOVE, TRUE );
         break;

      case IDD_REMOVE: {
         char szFolderName[ LEN_CONFNAME+1 ];
         char szParName[ 15 ];
         REMPAROBJECT emo;
         HWND hwndList;
         int index;
         PCL pcl;

         /* Get the folder name.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         index = ListBox_GetCurSel( hwndList );
         ASSERT( LB_ERR != index );
         ListBox_GetText( hwndList, index, szFolderName );
         pcl = Amdb_OpenFolder( NULL, szFolderName );
         ASSERT( NULL != pcl );

         /* Make sure we're the moderator.
          */
         if( !Amdb_IsModerator( pcl, szCIXNickname ) )
            fMessageBox( hwnd, 0, GS(IDS_STR97), MB_OK|MB_ICONEXCLAMATION );
         else
            {
            /* Get the username.
             */
            Edit_GetText( GetDlgItem( hwnd, IDD_EDIT ), szParName, 15 );
            StripLeadingTrailingSpaces( szParName );

            /* Create the REMPAR structure.
             */
            Amob_New( OBTYPE_REMPAR, &emo );
            Amob_CreateRefString( &emo.recConfName, (LPSTR)Amdb_GetFolderName( pcl ) );
            Amob_CreateRefString( &emo.recUserName, szParName );
            Amob_Commit( NULL, OBTYPE_REMPAR, &emo );
            Amob_Delete( OBTYPE_REMPAR, &emo );
            }
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, 0 );
         break;

      case IDOK: {
         char szParName[ 16 ];
         HWND hwndList;
         PCAT pcat;
         PCL pcl;

         /* If we're searching, abort the search.
          */
         if( fSearching )
            {
            fError = TRUE;
            break;
            }

         /* Get and trim the nickname.
          */
         Edit_GetText( GetDlgItem( hwnd, IDD_EDIT ), szParName, 15 );
         StripLeadingTrailingSpaces( szParName );
         if( '\0' == *szParName )
            break;

         /* Enable Stop button.
          */
         SetWindowText( GetDlgItem( hwnd, IDOK ), "&Stop" );
         EnableControl( hwnd, IDD_EDIT, FALSE );
         EnableControl( hwnd, IDCANCEL, FALSE );
         EnableControl( hwnd, IDD_REMOVE, FALSE );
         fSearching = TRUE;

         /* Empty the list and begin searching in every
          * topic.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         ListBox_ResetContent( hwndList );
         fError = FALSE;
         for( pcat = Amdb_GetFirstCategory(); !fError && pcat; pcat = Amdb_GetNextCategory( pcat ) )
            for( pcl = Amdb_GetFirstFolder( pcat ); !fError && pcl; pcl = Amdb_GetNextFolder( pcl ) )
               if( IsCIXFolder( pcl ) )
                  {
                  char szFileName[ _MAX_FNAME ];
                  FOLDERINFO confinfo;
                  HNDFILE fh;

                  /* Open PAR file and search it, line by line.
                   */
                  Amdb_GetFolderInfo( pcl, &confinfo );
                  wsprintf( szFileName, "%s.PAR", (LPSTR)confinfo.szFileName );
                  if( ( fh = Ameol2_OpenFile( szFileName, DSD_PARLIST, AOF_READ ) ) != HNDFILE_ERROR )
                     {
                     LPLBF hBuffer;
                     char sz[ 20 ];

                     hBuffer = Amlbf_Open( fh, AOF_READ );
                     fError = FALSE;
                     while( Amlbf_Read( hBuffer, sz, sizeof( sz ) - 2, NULL, NULL, &fIsAmeol2Scratchpad ) )
                        {
                        TaskYield();
                        StripLeadingTrailingSpaces( sz );
                        if( _strcmpi( sz, szParName ) == 0 )
                           if( ListBox_AddString( hwndList, confinfo.szFolderName ) == LB_ERR )
                              {
                              fError = TRUE;
                              break;
                              }
                        }
                     Amlbf_Close( hBuffer );
                     }
                  }

         /* Restore buttons.
          */
         fSearching = FALSE;
         SetWindowText( GetDlgItem( hwnd, IDOK ), "&Find" );
         EnableControl( hwnd, IDD_EDIT, TRUE );
         EnableControl( hwnd, IDCANCEL, TRUE );
         SetFocus( GetDlgItem( hwnd, IDD_EDIT ) );
         break;
         }
      }
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL FindInParlistDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   Edit_LimitText( GetDlgItem( hwnd, IDD_EDIT ), 14 );
   EnableControl( hwnd, IDOK, FALSE );
   EnableControl( hwnd, IDD_REMOVE, FALSE );
   return( TRUE );
}

BOOL FASTCALL ParlistWndResetContent( HWND hwnd )
{
   HWND hwndList;
   PCAT pcat;
   PCL pcl;
   LPPARLISTWNDINFO lppli;

   /* Fill the conference list window.
    */
   VERIFY( hwndList = GetDlgItem( hwndParlist, IDD_CONFERENCE ) );
   ListBox_ResetContent( hwndList );
   for( pcat = Amdb_GetFirstCategory(); pcat; pcat = Amdb_GetNextCategory( pcat ) )
      for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
         if( IsCIXFolder( pcl ) )
            ListBox_InsertString( hwndList, -1, (LPARAM)pcl );

   /* If nothing in the tree, exit now.
    */
   if( 0 == ListBox_GetCount( hwndList ) )
      {
      fMessageBox( hwnd, 0, GS(IDS_STR995), MB_OK|MB_ICONEXCLAMATION );
      return( FALSE );
      }

   /* Select the conference.
    */
   ListBox_SetSel( hwndList, TRUE, 0 );
   ListBox_SetTopIndex( hwndList, 0 );
   lppli = GetParlistWndInfo( hwndParlist );
   lppli->pcl = (PCL)ListBox_GetItemData( hwndList, 0 );

   if( TabCtrl_GetCurSel( GetDlgItem( hwndParlist, IDD_TABS ) ) == 0 )
      FillParticipantsList( hwndParlist, lppli->pcl );
   else
      FillModeratorsList( hwndParlist, lppli->pcl );
   return( TRUE );

}

/* This function handles the Ban and Unban participant dialogs.
 */
BOOL EXPORT CALLBACK BanParticipant( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = BanParticipant_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the GetResume
 * dialog.
 */
LRESULT FASTCALL BanParticipant_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, BanParticipant_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, BanParticipant_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, dlgID );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL BanParticipant_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPSTR psz = (LPSTR)lParam;
   Edit_LimitText( GetDlgItem( hwnd, IDD_EDIT ), LEN_CIXNAME );
   Edit_SetText( GetDlgItem( hwnd, IDD_EDIT ), psz );
   EnableControl( hwnd, IDOK, *psz != '\0' );
   SetWindowLong( hwnd, DWL_USER, lParam );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL BanParticipant_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EDIT:
         if( codeNotify == EN_UPDATE )
            EnableControl( hwnd, IDOK, Edit_GetTextLength( GetDlgItem( hwnd, IDD_EDIT ) ) > 0 );
         break;

      case IDOK: {
         LPSTR psz = (LPSTR)GetWindowLong( hwnd, DWL_USER );
         Edit_GetText( GetDlgItem( hwnd, IDD_EDIT ), psz, LEN_CIXNAME+1 );

         EndDialog( hwnd, TRUE );
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}
