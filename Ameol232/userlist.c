/* USERLIST.C - Display full list of CIX participants
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
#include <stdio.h>
#include <string.h>
#include "lbf.h"
#include "shared.h"
#include "cix.h"
#include "amlib.h"
#include <ctype.h>
#include "amaddr.h"

#define  THIS_FILE   __FILE__

#define  USERLIST_VERSION     0x4891
#define  MAX_LINELENGTH       200
#define  LEN_FINDTEXT         32

/* The intrinsic version of this function does not
 * properly handle huge pointers!
 */
#pragma function(_fmemcpy)

/* The following determine what type each line is. It can be
 * either a comment, which is non-selectable, or an entry which
 * can be highlighted, or a title.
 * NOTE: None of these must be NULL.
 */
#define  HTYP_ENTRY           0x40
#define  HTYP_COMMENT         0x41
#define  HTYP_TITLE           0x42

extern BOOL fIsAmeol2Scratchpad;

char NEAR szUserListWndClass[] = "amctl_userwndcls";
char NEAR szUserListWndName[] = "User List";

char szUsersDatFile[] = "cixusers.dat";   /* Compiled Users list */

static BOOL fDefDlgEx = FALSE;

HWND hwndUserList = NULL;              /* Handle of users list window */

static BOOL fRegistered = FALSE;       /* TRUE if we've registered the user list window */
static WNDPROC lpProcListCtl;          /* Subclassed listbox window procedure */
static WNDPROC lpProcVListCtl;         /* Subclassed virtual listbox window procedure */
static HPSTR lpLines;                  /* Pointer to text of list (can be > 64K!) */
static HPDWORD lpIndexTable;           /* Index into text */
static UINT cTotalLines;               /* Total lines in array */
static LONG cTotalInList;              /* Total lines in list */
static UINT nLastSel;                  /* Index of the last selected item in the list */
static BOOL fMouseClick;               /* TRUE if last action was a mouse click */
static BOOL fUp;                       /* TRUE if last list movement was up */
static HFONT hUserListBUFont = NULL;   /* Bold, underlined users list font */
static HFONT hUserListBFont = NULL;    /* Bold Users list font */
static int hdrColumns[ 2 ];            /* Header field column widths */
static DWORD cTotalEntryLines;         /* Number of entries in the list */
static BOOL fQuickUpdate;              /* TRUE if we skip WM_ERASEBKGND in list window */
static int nGrpViewMode;               /* User List view mode */
static HBRUSH hbrUserList;                /* Brush used to paint listbox */

#define  NGF_ALL           0
#define  NGF_NEW           1

BOOL FASTCALL UserListWnd_OnCreate( HWND, LPCREATESTRUCT );
void FASTCALL UserListWnd_OnClose( HWND );
void FASTCALL UserListWnd_OnDestroy( HWND );
void FASTCALL UserListWnd_OnSize( HWND, UINT, int, int );
void FASTCALL UserListWnd_OnMove( HWND, int, int );
void FASTCALL UserListWnd_OnTimer( HWND, UINT );
void FASTCALL UserListWnd_OnSetFocus( HWND, HWND );
void FASTCALL UserListWnd_MDIActivate( HWND, BOOL, HWND, HWND );
void FASTCALL UserListWnd_OnCommand( HWND, int, HWND, UINT );
void FASTCALL UserListWnd_OnAdjustWindows( HWND, int, int );
void FASTCALL UserListWnd_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
void FASTCALL UserListWnd_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
HBRUSH FASTCALL UserListWnd_OnCtlColor( HWND, HDC, HWND, int );
LRESULT FASTCALL UserListWnd_OnNotify( HWND, int, LPNMHDR );

void FASTCALL ReadUserList( HWND hwnd );
void FASTCALL BuildUserListFonts( HWND );
int FASTCALL BuildUserListIndex( char *, int );

BOOL EXPORT WINAPI UserList_Import( HWND, LPSTR );
LRESULT EXPORT CALLBACK UserListWndProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK UserListVListProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK UserListComboBoxProc( HWND, UINT, WPARAM, LPARAM );

/* This function creates the window that will display the full list of
 * Users.
 */
BOOL FASTCALL CreateUserListWindow( HWND hwnd )
{
   DWORD dwState;
   RECT rc;

   /* Locate USERS.LST and import it if found.
    */
   if( Amfile_QueryFile( szUsersLstFile ) )
      {
      HWND hwndBill;

      hwndBill = Adm_Billboard( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_IMPORTINGUSERSLIST) );
      UserList_Import( hwnd, (LPSTR)szUsersLstFile );
      Amfile_Delete( szUsersLstFile );
      DestroyWindow( hwndBill );
      }

   /* Open window if it already exists.
    */
   if( NULL != hwndUserList )
      {
      if( IsIconic( hwndUserList ) )
         SendMessage( hwndMDIClient, WM_MDIRESTORE, (WPARAM)hwndUserList, 0L );
      SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndUserList, 0L );
      return( TRUE );
      }

   /* Register the users list window class if we have
    * not already done so.
    */
   if( !fRegistered )
      {
      WNDCLASS wc;

      wc.style          = CS_HREDRAW | CS_VREDRAW;
      wc.lpfnWndProc    = UserListWndProc;
      wc.hIcon          = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_USERLIST) );
      wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
      wc.lpszMenuName   = NULL;
      wc.cbWndExtra     = DLGWINDOWEXTRA;
      wc.cbClsExtra     = 0;
      wc.hbrBackground  = NULL;
      wc.lpszClassName  = szUserListWndClass;
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
   InflateRect( &rc, -10, -10 );
   dwState = 0;

   /* Get the actual window size, position and state.
    */
   ReadProperWindowState( szUserListWndName, &rc, &dwState );

   /* Create the window.
    */
   hwndUserList = Adm_CreateMDIWindow( szUserListWndName, szUserListWndClass, hInst, &rc, dwState, 0L );
   if( NULL == hwndUserList )
      return( FALSE );

   /* Set the focus to the list box.
    */
   SetFocus( GetDlgItem( hwndUserList, IDD_FINDTEXT ) );
   return( TRUE );
}

/* This is the main window procedure.
 */
LRESULT EXPORT CALLBACK UserListWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LPVLBSTRUCT lpvlbInStruct;

   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, UserListWnd_OnCreate );
      HANDLE_MSG( hwnd, WM_CLOSE, UserListWnd_OnClose );
      HANDLE_MSG( hwnd, WM_DESTROY, UserListWnd_OnDestroy );
      HANDLE_MSG( hwnd, WM_SIZE, UserListWnd_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, UserListWnd_OnMove );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, Common_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, UserListWnd_MDIActivate );
      HANDLE_MSG( hwnd, WM_COMMAND, UserListWnd_OnCommand );
      HANDLE_MSG( hwnd, WM_MENUSELECT, MainWnd_OnMenuSelect );
   #ifdef WIN32
      HANDLE_MSG( hwnd, WM_CTLCOLORLISTBOX, UserListWnd_OnCtlColor );
   #else
      HANDLE_MSG( hwnd, WM_CTLCOLOR, UserListWnd_OnCtlColor );
   #endif
      HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, UserListWnd_OnAdjustWindows );
      HANDLE_MSG( hwnd, WM_DRAWITEM, UserListWnd_OnDrawItem );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, UserListWnd_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_NOTIFY, UserListWnd_OnNotify );
      HANDLE_MSG( hwnd, WM_TIMER, UserListWnd_OnTimer );
      HANDLE_MSG( hwnd, WM_SETFOCUS, UserListWnd_OnSetFocus );

      case VLB_PREV:
         lpvlbInStruct = (LPVLBSTRUCT)lParam;
         if ( lpvlbInStruct->lIndex > 0 )
            {
            lpvlbInStruct->nStatus = VLB_OK;
            --lpvlbInStruct->lIndex;
            lpvlbInStruct->lData = lpvlbInStruct->lIndex;
            return TRUE;
            }
         else {
            lpvlbInStruct->nStatus = VLB_ENDOFFILE;
            return TRUE;
            }
         break;

      case VLB_FINDPOS:
         lpvlbInStruct = (LPVLBSTRUCT)lParam;
         if ( lpvlbInStruct->lIndex == 0L )
            goto First;
         else if ( lpvlbInStruct->lIndex == 100L )
            goto Last;
         else {
            lpvlbInStruct->lIndex = lpvlbInStruct->lData * cTotalInList;
            lpvlbInStruct->nStatus = VLB_OK;
            return TRUE;
            }
         break;

      case VLB_FINDITEM:
         lpvlbInStruct = (LPVLBSTRUCT)lParam;
         lpvlbInStruct->lIndex = lpvlbInStruct->lData;
         lpvlbInStruct->nStatus = VLB_OK;
         return TRUE;

      case VLB_FINDSTRING:
      case VLB_FINDSTRINGEXACT:
      case VLB_SELECTSTRING:
      case VLBR_FINDSTRING:
      case VLBR_FINDSTRINGEXACT:
      case VLBR_SELECTSTRING: {
         lpvlbInStruct = (LPVLBSTRUCT)lParam;
         _fstrcpy( lpTmpBuf, lpvlbInStruct->lpFindString );
         lpvlbInStruct->lIndex = atol( lpTmpBuf );
         hstrcpy( lpTmpBuf, &lpLines[ lpIndexTable[ lpvlbInStruct->lIndex ] ] );
         lpvlbInStruct->nStatus = VLB_OK;
         return TRUE;
         }

      case VLB_RANGE:
         /* The vlist control is asking us for the range of
          * the listbox. So tell it.
          */
         lpvlbInStruct = (LPVLBSTRUCT)lParam;
         lpvlbInStruct->lIndex = cTotalInList;
         lpvlbInStruct->nStatus = VLB_OK;
         return TRUE;

      case VLB_NEXT:
         lpvlbInStruct = (LPVLBSTRUCT)lParam;
         if( lpvlbInStruct->lIndex < (LONG)cTotalInList - 1 )
            {
            lpvlbInStruct->nStatus = VLB_OK;
            ++lpvlbInStruct->lIndex;
            lpvlbInStruct->lData = lpvlbInStruct->lIndex;
            }
         else
            lpvlbInStruct->nStatus = VLB_ENDOFFILE;
         return TRUE;

      case VLB_FIRST:
         /* Return the index of the first item in the
          * list.
          */
First:   lpvlbInStruct = (LPVLBSTRUCT)lParam;
         lpvlbInStruct->nStatus = VLB_OK;
         lpvlbInStruct->lIndex = 0L;
         lpvlbInStruct->lData = lpvlbInStruct->lIndex;
         return TRUE;

      case VLB_LAST:
         /* Return the index of the last item in the
          * list.
          */
Last:    lpvlbInStruct = (LPVLBSTRUCT)lParam;
         lpvlbInStruct->nStatus = VLB_OK;
         lpvlbInStruct->lIndex = cTotalInList - 1;
         lpvlbInStruct->lData = lpvlbInStruct->lIndex;
         return TRUE;

      case VLB_GETITEMDATA:
      case VLBR_GETITEMDATA:
         lpvlbInStruct = (LPVLBSTRUCT)lParam;
         lpvlbInStruct->nStatus = VLB_OK;
         lpvlbInStruct->lData = lpvlbInStruct->lIndex;
         return TRUE;

      case VLB_GETTEXT:
      case VLBR_GETTEXT:
         lpvlbInStruct = (LPVLBSTRUCT)lParam;
         lpvlbInStruct->nStatus = VLB_OK;
         lpvlbInStruct->lData = lpvlbInStruct->lIndex;
         hstrcpy( lpTmpBuf, &lpLines[ lpIndexTable[ lpvlbInStruct->lIndex ] ] );
         lpvlbInStruct->lpTextPointer = lpTmpBuf;
         return TRUE;

      case WM_CHANGEFONT:
         if( wParam == FONTYP_USERLIST || 0xFFFF == wParam )
            {
            MEASUREITEMSTRUCT mis;
            HWND hwndList;

            /* Update the users list window font. Will also have to
             * rebuild the font variations.
             */
            hwndList = GetDlgItem( hwnd, IDD_LIST );
            BuildUserListFonts( hwnd );
            SendMessage( hwnd, WM_MEASUREITEM, IDD_LIST, (LPARAM)(LPSTR)&mis );
            SendMessage( hwndList, VLB_SETITEMHEIGHT, 0, MAKELPARAM( mis.itemHeight, 0) );
            InvalidateRect( hwndList, NULL, TRUE );
            UpdateWindow( hwndList );
            }
         return( 0L );

      case WM_APPCOLOURCHANGE:
         if( wParam == WIN_USERLIST )
            {
            HWND hwndList;

            /* Redraw the users list window in the new colours.
             */
            if( NULL != hbrUserList )
               DeleteBrush( hbrUserList );
            hbrUserList = CreateSolidBrush( crUserListWnd );
            hwndList = GetDlgItem( hwnd, IDD_LIST );
            InvalidateRect( hwndList, NULL, TRUE );
            UpdateWindow( hwndList );
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
         lpMinMaxInfo->ptMinTrackSize.x = 365;
         lpMinMaxInfo->ptMinTrackSize.y = 215;
         return( lResult );
         }

      default:
         break;
      }
   return( Adm_DefMDIDlgProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL UserListWnd_OnCreate( HWND hwnd, LPCREATESTRUCT lpCreateStruct )
{
   static int aArray[ 2 ] = { 150, 300 };
   LPMDICREATESTRUCT lpMDICreateStruct;
   HWND hwndList;
   HWND hwndTab;
   TC_ITEM tie;
   HD_ITEM hdi;

   /* Create the dialog window.
    */
   lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
   Adm_MDIDialog( hwnd, MAKEINTRESOURCE(IDDLG_USERLIST), lpMDICreateStruct );

   /* Initialise the list box.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   fQuickUpdate = FALSE;
   nGrpViewMode = NGF_ALL;
   SendMessage( hwndList, VLB_INITIALIZE, 0, 0L );

   /* Create an underlined and boldface variant of the
    * users list font.
    */
   BuildUserListFonts( hwnd );

   /* Read the current header settings.
    */
   Amuser_GetPPArray( szSettings, "UserListColumns", aArray, 2 );

   /* Set the tab control.
    */
   VERIFY( hwndTab = GetDlgItem( hwnd, IDD_TABS ) );
   tie.mask = TCIF_TEXT;
   tie.pszText = "        All        ";
   TabCtrl_InsertItem( hwndTab, 0, &tie );
#ifdef HAS_NEW_TAB
   tie.pszText = "        New        ";
   TabCtrl_InsertItem( hwndTab, 1, &tie );
#endif
   SetWindowFont( hwndTab, hHelvB8Font, FALSE );

   /* Fill in the header fields.
    */
   hdi.mask = HDI_TEXT|HDI_WIDTH|HDI_FORMAT;
   hdi.cxy = hdrColumns[ 0 ] = aArray[ 0 ];
   hdi.pszText = "Nickname";
   hdi.fmt = HDF_STRING|HDF_LEFT;
   Header_InsertItem( hwndList, 0, &hdi );
   
   hdi.cxy = hdrColumns[ 1 ] = aArray[ 1 ];
   hdi.pszText = "Full Name";
   hdi.fmt = HDF_STRING|HDF_LEFT;
   Header_InsertItem( hwndList, 1, &hdi );

   hbrUserList = CreateSolidBrush( crUserListWnd );

   SendMessage( hwndList, WM_SETTEXT, 1 | HBT_SPRING, 0L );
   SetWindowFont( hwndList, hHelv8Font, FALSE );

   /* Set the limit of the search text
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_FINDTEXT ), LEN_FINDTEXT );

   /* Subclass the list box.
    */
   hwndList = GetDlgItem( hwndList, 0x3000 );
   hwndList = GetWindow( hwndList, GW_CHILD );
   lpProcVListCtl = SubclassWindow( hwndList, UserListVListProc );
   SetWindowText( GetDlgItem( hwnd, IDD_PAD3 ), GS(IDS_STR1178) );
   return( TRUE );
}

/* This function builds two variations of the users lists
 * font.
 */
void FASTCALL BuildUserListFonts( HWND hwnd )
{
   LOGFONT lf;

   /* Delete any old ones.
    */
   if( NULL != hUserListBFont )
      DeleteFont( hUserListBFont );
   if( NULL != hUserListBUFont )
      DeleteFont( hUserListBUFont );

   /* Create the new ones.
    */
   lf = lfUserListFont;
   lf.lfWeight = FW_BOLD;
   hUserListBFont = CreateFontIndirect( &lf );
   lf.lfUnderline = TRUE;
   hUserListBUFont = CreateFontIndirect( &lf );
}

/* This function handles the WM_SETFOCUS message.
 */
void FASTCALL UserListWnd_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   SetFocus( GetDlgItem( hwnd, IDD_LIST ) );
}

/* This function processes the WM_COMMAND message.
 */
void FASTCALL UserListWnd_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDM_ADDTOADDRBOOK: {
         HWND hwndList;
         int index;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         if( ( index = (UINT)SendMessage( hwndList, VLB_GETCURSEL, 0, 0L ) ) != LB_ERR )
            {
            char szUserName[ LEN_CIXNAME + 1 ];
            register int c;
            HPSTR lpStr;

            /* Get the highlighted user name.
             */
            lpStr = &lpLines[ lpIndexTable[ index ] ];
            ++lpStr;
            ++lpStr;
            for( c = 0; c < LEN_CIXNAME && *lpStr && *lpStr != '\t'; ++c )
               szUserName[ c ] = *lpStr++;
            szUserName[ c ] = '\0';
            Amaddr_AddEntry( hwnd, szUserName, "" );
            }
         break;
         }

      case IDM_QUICKPRINT:
      case IDM_PRINT: {
         PRINTLISTWND plw;

         plw.hwnd = hwnd;
         plw.idDlg = IDDLG_PRINTUSERLIST;
         plw.cLines = cTotalLines;
         plw.lpIndexTable = lpIndexTable;
         plw.lpLines = lpLines;
         CmdPrintGeneralList( hwnd, &plw );
         break;
         }

      case IDM_COLOURSETTINGS:
         CustomiseDialog( hwnd, CDU_COLOURS );
         break;

      case IDM_FONTSETTINGS:
         CustomiseDialog( hwnd, CDU_FONTS );
         break;

      case IDD_FINDTEXT:
         if( codeNotify == EN_CHANGE )
            {
            KillTimer( hwnd, IDD_FINDTEXT );
            SetTimer( hwnd, IDD_FINDTEXT, 500, NULL );
            }
         break;

      case IDCANCEL:
//       if( GetFocus() != GetDlgItem( hwnd, IDD_FINDTEXT ) )
            UserListWnd_OnClose( hwnd );
         break;

      case IDD_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, NGF_ALL == nGrpViewMode ? idsUSERLIST_ALL : idsUSERLIST_NEW );
         break;

      case IDD_LIST:
         if( codeNotify == LBN_SELCHANGE && cTotalInList )
            {
            LONG index;
            HPSTR lpStr;

            /* Deal with the user moving the selection up and down the
             * list. We need to skip title items.
             */
            index = (UINT)SendMessage( hwndCtl, VLB_GETCURSEL, 0, 0L );
            lpStr = &lpLines[ lpIndexTable[ index ] ];
            if( *lpStr != HTYP_ENTRY )
               {
               if( fMouseClick )
                  index = nLastSel;
               else if( fUp )
                  while( (int)--index >= 0 )
                     {
                     lpStr = &lpLines[ lpIndexTable[ index ] ];
                     if( *lpStr == HTYP_ENTRY )
                        break;
                     }
               else
                  while( ++index < cTotalInList )
                     {
                     lpStr = &lpLines[ lpIndexTable[ index ] ];
                     if( *lpStr == HTYP_ENTRY )
                        break;
                     }
               if( index < 0 || index >= cTotalInList )
                  index = nLastSel;
               SendMessage( hwndCtl, VLB_SETCURSEL, VLB_FINDITEM, (LPARAM)index );
               UpdateWindow( hwndCtl );
               }
            nLastSel = (UINT)index;
            }
         if( codeNotify != LBN_DBLCLK )
            break;
         id = IDM_RESUME;

      case IDM_RESUME:
      case IDM_MAIL: {
         HWND hwndList;
         int index;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         if( ( index = (UINT)SendMessage( hwndList, VLB_GETCURSEL, 0, 0L ) ) != LB_ERR )
            {
            char szUserName[ LEN_CIXNAME + 1 ];
            register int c;
            HPSTR lpStr;

            /* Get the highlighted user name.
             */
            lpStr = &lpLines[ lpIndexTable[ index ] ];
            ++lpStr;
            ++lpStr;
            for( c = 0; c < LEN_CIXNAME && *lpStr && *lpStr != '\t'; ++c )
               szUserName[ c ] = *lpStr++;
            szUserName[ c ] = '\0';

            /* Now handle appropriate command.
             */
            switch( id )
               {
               case IDM_RESUME:
                  CmdShowResume( hwnd, szUserName, TRUE );
                  break;

               case IDM_MAIL:
                  CmdMailMessage( hwnd, NULL, szUserName );
                  break;
               }
            }
         break;
         }

      case IDD_UPDATE:
         if( Amob_Find( OBTYPE_GETUSERLIST, NULL ) )
            fMessageBox( hwnd, 0, GS(IDS_STR148), MB_OK|MB_ICONEXCLAMATION );
         else
            if( fMessageBox( hwnd, 0, GS(IDS_STR149), MB_YESNO|MB_ICONQUESTION ) == IDYES )
               {
               Amob_New( OBTYPE_GETUSERLIST, NULL );
               Amob_Commit( NULL, OBTYPE_GETUSERLIST, NULL );
               Amob_Delete( OBTYPE_GETUSERLIST, NULL );
               }
         break;
      }
}

/* Add an item to the out-basket to download the full list
 * of CIX users.
 */
void FASTCALL UpdateUsersList( void )
{
   if( !Amob_Find( OBTYPE_GETUSERLIST, NULL ) )
      {
      Amob_New( OBTYPE_GETUSERLIST, NULL );
      Amob_Commit( NULL, OBTYPE_GETUSERLIST, NULL );
      Amob_Delete( OBTYPE_GETUSERLIST, NULL );
      }
}

/* This function handles the WM_MEASUREITEM message
 */
void FASTCALL UserListWnd_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   HFONT hOldFont;
   HDC hdc;

   hdc = GetDC( hwnd );
   hOldFont = SelectFont( hdc, hUserListFont );
   GetTextMetrics( hdc, &tm );
   lpMeasureItem->itemHeight = tm.tmHeight + tm.tmExternalLeading + 2;
   SelectFont( hdc, hOldFont );
   ReleaseDC( hwnd, hdc );
}

/* This function handles the WM_DRAWITEM message.
 */
void FASTCALL UserListWnd_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   COLORREF tmpT;
   COLORREF tmpB;
   HFONT hOldFont;
   char sz2[ MAX_LINELENGTH+1 ];
   RECT rc;
   int len;

   /* Do nothing if we're not drawing a valid item.
    */
   if( cTotalInList == 0 || (LONG)lpDrawItem->itemData < 0 || (LONG)lpDrawItem->itemData >= cTotalInList )
      return;

   /* Compute the length of the string we're drawing and make a local
    * copy of it.
    */
   len = (int)hstrlen( &lpLines[ lpIndexTable[ lpDrawItem->itemData ] ] ) + 1;
   if( len > MAX_LINELENGTH )
      return;
   _fmemcpy( sz2, &lpLines[ lpIndexTable[ lpDrawItem->itemData ] ], len );

   /* Initialise brushes and pens.
    */
   rc = lpDrawItem->rcItem;
   tmpB = SetBkColor( lpDrawItem->hDC, crUserListWnd );
   tmpT = SetTextColor( lpDrawItem->hDC, crUserListText );

   /* Now we're ready to draw.
    */
   if( len && strlen( sz2 + 1 ) )
      {
      if( sz2[ 0 ] == HTYP_COMMENT && ( lpDrawItem->itemAction & ODA_DRAWENTIRE ) )
         {
         /* Draw a comment field.
          */
         SetTextColor( lpDrawItem->hDC, crUserListWnd );
         hOldFont = SelectFont( lpDrawItem->hDC, hUserListBFont );
         ExtTextOut( lpDrawItem->hDC, rc.left + 2, rc.top, ETO_OPAQUE, &rc, sz2 + 1, strlen( sz2 + 1 ), NULL );
         SetTextColor( lpDrawItem->hDC, tmpT );
         }
      else if( sz2[ 0 ] == HTYP_TITLE && ( lpDrawItem->itemAction & ODA_DRAWENTIRE ) )
         {
         /* Draw the title field.
          */
         SetTextColor( lpDrawItem->hDC, crUserListText );
         hOldFont = SelectFont( lpDrawItem->hDC, hUserListBUFont );
         ExtTextOut( lpDrawItem->hDC, rc.left + 2, rc.top, ETO_OPAQUE, &rc, sz2 + 1, strlen( sz2 + 1 ), NULL );
         SetTextColor( lpDrawItem->hDC, tmpT );
         }
      else if( sz2[ 0 ] == HTYP_ENTRY )
         {
         int length;
         char * psz;
         SIZE size;

         /* Draw an entry. The first part is the user's nickname, and the
          * second is the user's full name. Each part is separate by a tab.
          */
         psz = sz2 + 2;
         for( length = 0; psz[ length ] != '\t'; ++length );
         psz[ length ] = '\0';

         /* Only do this lot if we're repainting the entire
          * line.
          */
         if( lpDrawItem->itemAction & ODA_DRAWENTIRE )
            {
            rc.right = 16;
            ExtTextOut( lpDrawItem->hDC, rc.left, rc.top, ETO_OPAQUE|ETO_CLIPPED, &rc, NULL, 0, NULL );
            }

         /* Set colours.
          */
         if( lpDrawItem->itemState & ODS_SELECTED )
            {
            if( lpDrawItem->itemState & ODS_FOCUS )
               {
               SetTextColor( lpDrawItem->hDC, GetSysColor( COLOR_HIGHLIGHTTEXT ) );
               SetBkColor( lpDrawItem->hDC, GetSysColor( COLOR_HIGHLIGHT ) );
               }
            else
               {
               SetTextColor( lpDrawItem->hDC, crUserListWnd );
               SetBkColor( lpDrawItem->hDC, GetSysColor( COLOR_BTNFACE ) );
               }
            }
         else if( IsResumeAvailable( psz ) )
            {
            /* This resume is available locally, so show it
             * in red.
             */
            SetTextColor( lpDrawItem->hDC, RGB( 255, 0, 0 ) );
            SetBkColor( lpDrawItem->hDC, crUserListWnd );
            }

         /* Adjust the size of the string to fit.
          */
         rc.left = 16;
         hOldFont = SelectFont( lpDrawItem->hDC, hUserListBFont );
         AdjustNameToFit( lpDrawItem->hDC, psz, hdrColumns[ 0 ] - rc.left, FALSE, &size );
         rc.right = rc.left + size.cx;
         ExtTextOut( lpDrawItem->hDC, rc.left, rc.top, ETO_OPAQUE|ETO_CLIPPED, &rc, psz, length, NULL );

         /* Draw the focus, if appropriate.
          */
         if( lpDrawItem->itemState & ODS_FOCUS )
            DrawFocusRect( lpDrawItem->hDC, &rc );

         /* Now draw the description.
          */
         if( lpDrawItem->itemAction & ODA_DRAWENTIRE )
            {
            /* First fill the gap.
             */
            SelectFont( lpDrawItem->hDC, hUserListFont );
            SetTextColor( lpDrawItem->hDC, crUserListText );
            SetBkColor( lpDrawItem->hDC, crUserListWnd );
            rc.left = rc.right;
            rc.right = hdrColumns[ 0 ];
            ExtTextOut( lpDrawItem->hDC, rc.left, rc.top, ETO_OPAQUE|ETO_CLIPPED, &rc, NULL, 0, NULL );

            /* Then draw descriptions.
             */
            rc.left = hdrColumns[ 0 ];
            rc.right = lpDrawItem->rcItem.right;
            psz += length + 1;
            length = strlen( psz );
            ExtTextOut( lpDrawItem->hDC, rc.left, rc.top, ETO_OPAQUE|ETO_CLIPPED, &rc, psz, length, NULL );
            }
         SelectFont( lpDrawItem->hDC, hOldFont );
         }
      }
   else if( lpDrawItem->itemAction & ODA_DRAWENTIRE )
      ExtTextOut( lpDrawItem->hDC, rc.left, rc.top, ETO_OPAQUE, &rc, NULL, 0, NULL );
   SetBkColor( lpDrawItem->hDC, tmpB );
   SetTextColor( lpDrawItem->hDC, tmpT );
}

/* This function handles the WM_TIMER message.
 */
void FASTCALL UserListWnd_OnTimer( HWND hwnd, UINT id )
{
   if( id == IDD_FINDTEXT )
      {
      char szFindText[ LEN_FINDTEXT ];
      HCURSOR hOldCursor;
      HWND hwndList;
      int iStart;

      /* Get the new find text
       */
      KillTimer( hwnd, id );
      Edit_GetText( GetDlgItem( hwnd, IDD_FINDTEXT ), szFindText, sizeof(szFindText) );

      /* Build the new list.
       */
      hOldCursor = SetCursor( hWaitCursor );
      iStart = BuildUserListIndex( szFindText, nGrpViewMode );

      /* Update the list.
       */
      hwndList = GetDlgItem( hwnd, IDD_LIST );
      SendMessage( hwndList, VLB_RESETCONTENT, 0, 0L );
      SendMessage( hwndList, VLB_SETCURSEL, VLB_FINDITEM, (LPARAM)iStart );
      SetCursor( hOldCursor );
      nLastSel = iStart;

      EnableControl( hwnd, IDM_MAIL, ( cTotalEntryLines > 0 ) );
      EnableControl( hwnd, IDM_RESUME, ( cTotalEntryLines > 0 ) );

      /* Set focus back to Find box.
       */
      SetFocus( GetDlgItem( hwndUserList, IDD_FINDTEXT ) );

      }
}

/* This function handles notification messages from the
 * treeview control.
 */
LRESULT FASTCALL UserListWnd_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
{
   switch( lpNmHdr->code )
      {
      case TCN_SELCHANGE: {
         HCURSOR hOldCursor;
         HWND hwndList;
         int iStart;

         /* Build the new list.
          */
         hOldCursor = SetCursor( hWaitCursor );
         nGrpViewMode = TabCtrl_GetCurSel( lpNmHdr->hwndFrom );
         iStart = BuildUserListIndex( "", nGrpViewMode );

         /* Change the prompt.
          */
         SetWindowText( GetDlgItem( hwnd, IDD_PAD3 ), GS(IDS_STR1178 + nGrpViewMode) );

         /* Update the list.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         SendMessage( hwndList, VLB_RESETCONTENT, 0, 0L );
         SendMessage( hwndList, VLB_SETCURSEL, VLB_FINDITEM, (LPARAM)iStart );
         SetCursor( hOldCursor );
         nLastSel = iStart;

         EnableControl( hwnd, IDM_MAIL, ( cTotalEntryLines > 0 ) );
         EnableControl( hwnd, IDM_RESUME, ( cTotalEntryLines > 0 ) );

         /* Set focus back to Find box.
          */
         SetFocus( GetDlgItem( hwndUserList, IDD_FINDTEXT ) );
         return( FALSE );
         }

      case HDN_ITEMCLICK:
         return( TRUE );

      case HDN_BEGINTRACK:
         return( TRUE );

      case HDN_TRACK:
      case HDN_ENDTRACK: {
         HD_NOTIFY FAR * lpnf;
         HWND hwndList;
         int diff;
         RECT rc;

         /* Save the old offset of the column being
          * dragged.
          */
         lpnf = (HD_NOTIFY FAR *)lpNmHdr;
         diff = lpnf->pitem->cxy - hdrColumns[ lpnf->iItem ];
         hdrColumns[ lpnf->iItem ] = lpnf->pitem->cxy;
         if( HDN_ENDTRACK == lpNmHdr->code )
            Amuser_WritePPArray( szSettings, "UserListColumns", hdrColumns, 2 );

         /* Invalidate from the column being dragged.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         VERIFY( hwndList = GetDlgItem( hwndList, 0x3000 ) );
         GetClientRect( GetWindow( hwndList, GW_CHILD ), &rc );
         rc.left = lpnf->cxyUpdate + 1;
         if( diff > 0 )
            rc.left -= diff;

         /* Now use ScrollWindow.
          */
         fQuickUpdate = TRUE;
         ScrollWindow( hwndList, diff, 0, &rc, &rc );
         rc.right = rc.left;
         rc.left -= 20;
         InvalidateRect( hwndList, &rc, FALSE );
         UpdateWindow( hwndList );
         fQuickUpdate = FALSE;
         return( TRUE );
         }
      }
   return( FALSE );
}

/* This function intercepts messages for list box so that we can repaint the
 * background in our own colour.
 */
LRESULT EXPORT CALLBACK UserListVListProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   switch( msg )
      {
      case WM_CONTEXTMENU: /*!!SM!!*/
      case WM_RBUTTONDOWN: {
         HMENU hPopupMenu;
         int index;
         POINT pt;

         /* Select the item under the cursor only if nothing is
          * currently selected.
          */
         index = ItemFromPoint( hwnd, (short)LOWORD(lParam), (short)HIWORD(lParam) );
         if( ListBox_GetSel( hwnd, index ) == 0 )
            {
            CallWindowProc( lpProcVListCtl, hwnd, WM_LBUTTONDOWN, wParam, lParam );
            CallWindowProc( lpProcVListCtl, hwnd, WM_LBUTTONUP, wParam, lParam );
            }

         /* Display the popup menu.
          */
         hPopupMenu = GetPopupMenu( IPOP_ULISTWINDOW );
         GetCursorPos( &pt );
         TrackPopupMenu( hPopupMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndUserList, NULL );
         break;
         }

      case WM_ERASEBKGND:
         if( !fQuickUpdate )
            {
            RECT rc;
         
            GetClientRect( hwnd, &rc );
            SetBkColor( (HDC)wParam, GetSysColor( COLOR_WINDOW ) );
            ExtTextOut( (HDC)wParam, rc.left, rc.top, ETO_OPAQUE, &rc, NULL, 0, NULL );
            }
         return( TRUE );

      case WM_LBUTTONDOWN:
         fMouseClick = TRUE;
         break;

      case WM_KEYDOWN:
         fMouseClick = FALSE;
         if( wParam == VK_UP || wParam == VK_PRIOR || wParam == VK_END )
            fUp = TRUE;
         else if( wParam == VK_DOWN || wParam == VK_NEXT || wParam == VK_HOME )
            fUp = FALSE;
         break;
   #ifdef WIN32
      case WM_MOUSEWHEEL: {
         static int zDeltaAccumulate = 0;
         UINT iScrollLines;               /* Scroll lines */
         int zDelta;
         int nLines;

         /* Handle the Intellimouse wheel.
          */
         zDelta = (short)HIWORD( wParam );
         zDeltaAccumulate += zDelta;
         iScrollLines = Amctl_GetControlWheelScrollLines();
         if( nLines = zDeltaAccumulate / WHEEL_DELTA )
            {
            if( WHEEL_PAGESCROLL == iScrollLines )
               {
               if( nLines > 0 )
                  SendMessage( hwnd, WM_VSCROLL, SB_PAGEUP, 0L );
               else
                  SendMessage( hwnd, WM_VSCROLL, SB_PAGEDOWN, 0L );
               }
            else
               {
               nLines *= iScrollLines;
               while( nLines > 0 )
                  {
                  SendMessage( hwnd, WM_VSCROLL, SB_LINEUP, 0L );
                  --nLines;
                  }
               while( nLines < 0 )
                  {
                  SendMessage( hwnd, WM_VSCROLL, SB_LINEDOWN, 0L );
                  ++nLines;
                  }
               }
            zDeltaAccumulate = 0;
            }
         return( 0L );
         }
   #endif
      }
   return( CallWindowProc( lpProcVListCtl, hwnd, msg, wParam, lParam ) );
}

/* This function handles the WM_CTLCOLOR message.
 */
HBRUSH FASTCALL UserListWnd_OnCtlColor( HWND hwnd, HDC hdc, HWND hwndChild, int type )
{
   switch( GetDlgCtrlID( hwndChild ) )
      {
      case IDD_LIST:
         SetBkColor( hdc, crUserListWnd );
         SetTextColor( hdc, crUserListText );
         return( hbrUserList );
      }
#ifdef WIN32
   return( FORWARD_WM_CTLCOLORLISTBOX( hwnd, hdc, hwndChild, Adm_DefMDIDlgProc ) );
#else
   return( FORWARD_WM_CTLCOLOR( hwnd, hdc, hwndChild, type, Adm_DefMDIDlgProc ) );
#endif
}

/* This function handles the WM_MDIACTIVATE message.
 */
void FASTCALL UserListWnd_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   if( !fActive )
      ShowUnreadMessageCount();
   Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_USERLIST ), (LPARAM)(LPSTR)hwnd );
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, Adm_DefMDIDlgProc );
}

/* Open the users list file and fill the treeview control with
 * items from the file. This function can be called at any time to
 * reinitialise the list of users in the window.
 */
void FASTCALL ReadUsersList( HWND hwnd )
{
   HCURSOR hOldCursor;
   HWND hwndList;
   DWORD dwSize;
   UINT iStart;
   HNDFILE fh;

   /* Make sure users list window is visible.
    */
   CreateUserListWindow( hwnd );

   /* Set a wait cursor as this could take some
    * time.
    */
   hOldCursor = SetCursor( hWaitCursor );

   /* Vape any existing lists.
    */
   if( lpLines )
      FreeMemory32( &lpLines );

   /* Initialise to read the lists file.
    */
   iStart = 0;
   cTotalLines = 0;
   lpIndexTable = NULL;
   lpLines = NULL;
   nLastSel = 0;
   fUp = fMouseClick = FALSE;

   /* Open the file. If we can't find it, then clear all
    * items.
    */
   if( ( fh = Amfile_Open( szUsersDatFile, AOF_READ ) ) == HNDFILE_ERROR )
      {
      SetDlgItemText( hwndUserList, IDD_LASTUPDATED, GS(IDS_STR341) );
      SetDlgItemText( hwndUserList, IDD_NUMBER, "" );
      EnableControl( hwndUserList, IDM_MAIL, FALSE );
      EnableControl( hwndUserList, IDM_RESUME, FALSE );
      }
   else
      {
      WORD nVersion;

      /* Get the file size now.
       */
      dwSize = Amfile_GetFileSize( fh );

      /* Make sure we have the correct version.
       */
      Amfile_Read( fh, (LPSTR)&nVersion, sizeof( WORD ) );
      if( nVersion != USERLIST_VERSION )
         {
         SetDlgItemText( hwndUserList, IDD_LASTUPDATED, GS(IDS_STR342) );
         SetDlgItemText( hwndUserList, IDD_NUMBER, "" );
         EnableControl( hwndUserList, IDM_MAIL, FALSE );
         EnableControl( hwndUserList, IDD_FIND, FALSE );
         EnableControl( hwndUserList, IDM_RESUME, FALSE );
         }
      else
         {
         /* Allocate memory for the entire list.
          */
         dwSize -= sizeof( WORD ) * 2;
         if( fNewMemory32( &lpLines, dwSize + 1  ) )
            {
            WORD uDate;
            WORD uTime;

            /* Read into memory. Remember it can be > 64K!
             */
            dwSize = Amfile_Read32( fh, lpLines, dwSize );
            lpLines[ dwSize ] = '\0';

            /* The last two bytes are the total number of lines
             * in the list.
             */
            Amfile_Read( fh, (LPSTR)&cTotalLines, sizeof( WORD ) );
   
            /* Now build the index using an empty qualifier.
             */
            iStart = BuildUserListIndex( "", nGrpViewMode );
         
            /* Show the date and time when this list was downloaded
             */
            Amfile_GetFileLocalTime( fh, &uDate, &uTime );
            wsprintf( lpTmpBuf, GS(IDS_STR336), Amdate_FriendlyDate( NULL, uDate, uTime ) );
            SetDlgItemText( hwndUserList, IDD_LASTUPDATED, lpTmpBuf );
            EnableControl( hwndUserList, IDM_MAIL, TRUE );
            EnableControl( hwndUserList, IDM_RESUME, TRUE );
            }
         else
            SetDlgItemText( hwndUserList, IDD_LASTUPDATED, GS(IDS_STR344) );
         }
      Amfile_Close( fh );
      }
   SetCursor( hOldCursor );

   /* All done, so start the virtual list control
    * so that it'll start loading items.
    */
   VERIFY( hwndList = GetDlgItem( hwndUserList, IDD_LIST ) );
   SendMessage( hwndList, VLB_RESETCONTENT, 0, 0L );
   SendMessage( hwndList, VLB_SETCURSEL, VLB_FINDITEM, (LPARAM)iStart );
   nLastSel = iStart;
}

/* This function builds an index into the CIX List table.
 */
int FASTCALL BuildUserListIndex( char * pszFindText, int nViewMode )
{
   int iStart;

   /* Create an index table
    */
   if( lpIndexTable )
      FreeMemory32( &lpIndexTable );
   iStart = 0;
   cTotalEntryLines = 0;
   if( fNewMemory32( &lpIndexTable, (LONG)cTotalLines * 4 ) )
      {
      HPSTR lpLinesPtr;
      register UINT c;
      DWORD offset;
      int iIndex;

      lpLinesPtr = lpLines;
      offset = 0;
      iStart = (UINT)-1;
      iIndex = 0;
      cTotalInList = 0;
      for( c = 0; c < cTotalLines; ++c )
         {
         int len;

         for( len = 0; lpLinesPtr[ len ]; ++len );
         ++len;
         if( *pszFindText && *lpLinesPtr != HTYP_ENTRY )
            {
            /* Don't look in title or comment fields when matching
             * search patterns.
             */
            lpLinesPtr += len;
            offset += len;
            }
         else if( *pszFindText && FStrMatch( lpLinesPtr + 2, pszFindText, 0, 0 ) == -1 )
            {
            /* A search pattern was specified but no match on this line
             * so skip it.
             */
            lpLinesPtr += len;
            offset += len;
            }
         else if( nViewMode == NGF_NEW && !isupper( lpLinesPtr[ 1 ] ) )
            {
            /* We're only showing new groups and this isn't new, so
             * skip it.
             */
            lpLinesPtr += len;
            offset += len;
            }
         else if( nViewMode == NGF_NEW && *lpLinesPtr != HTYP_ENTRY )
            {
            /* If showing new entries and no match so far, skip
             * this entry.
             */
            lpLinesPtr += len;
            offset += len;
            }
         else
            {
            /* Add this line to the index table. If it is
             * an entry, bump count of entries.
             */
            lpIndexTable[ iIndex ] = offset;
            if( *lpLinesPtr == HTYP_ENTRY )
               {
               ++cTotalEntryLines;
               if( iStart == (UINT)-1 )
                  iStart = iIndex;
               }
            offset += len;
            lpLinesPtr += len;
            ++iIndex;
            }
         }
      cTotalInList = iIndex;
      if( iStart == -1 )
         iStart = 0;
      }

   /* Show the number of items in the list
    */
   wsprintf( lpTmpBuf, GS(IDS_STR343), FormatThousands( cTotalEntryLines, NULL ) );
   SetDlgItemText( hwndUserList, IDD_NUMBER, lpTmpBuf );
   return( iStart );
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL UserListWnd_OnAdjustWindows( HWND hwnd, int dx, int dy )
{
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_FINDTEXT ), dx, 0 );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_LIST ), dx, dy );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_TABS ), dx, dy );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_LASTUPDATED ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_UPDATE ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_LASTUPDATED ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_NUMBER ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDM_MAIL ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDM_RESUME ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDCANCEL ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_HELP ), dx, 0 );
}

/* This function handles the WM_CLOSE message.
 */
void FASTCALL UserListWnd_OnClose( HWND hwnd )
{
   Adm_DestroyMDIWindow( hwnd );
}

/* This function handles the WM_DESTROY message.
 */
void FASTCALL UserListWnd_OnDestroy( HWND hwnd )
{
   /* Delete resources.
    */
   DeleteFont( hUserListBUFont );
   DeleteFont( hUserListBFont );
   hwndUserList = NULL;
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL UserListWnd_OnMove( HWND hwnd, int x, int y )
{
   Amuser_WriteWindowState( szUserListWndName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL UserListWnd_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   Amuser_WriteWindowState( szUserListWndName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* This is the users list out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_GetUserList( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   switch( uType )
      {
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_DESCRIBE:
         strcpy( lpBuf, GS(IDS_STR243) );
         return( TRUE );

      case OBEVENT_DELETE:
      case OBEVENT_FIND:
         return( TRUE );
      }
   return( 0L );
}

/* This function is the users list import entry point.
 */
BOOL EXPORT WINAPI UserList_Import( HWND hwnd, LPSTR lpszFilename )
{
   char szStatusTxt[ 60 ];
   char sz[ _MAX_PATH ];
   HNDFILE fh;
   BOOL fParsingUserList;

   /* Initialise variables.
    */
   fMainAbort = FALSE;

   /* If no filename supplied, prompt the user
    * for one.
    */
   if( NULL == lpszFilename )
      {
      static char strFilters[] = "Ameol Users List\0users.dat\0All Files (*.*)\0*.*\0\0";
      OPENFILENAME ofn;

      strcpy( sz, "users.dat" );
      ofn.lStructSize = sizeof( OPENFILENAME );
      ofn.hwndOwner = hwnd;
      ofn.hInstance = NULL;
      ofn.lpstrFilter = strFilters;
      ofn.lpstrCustomFilter = NULL;
      ofn.nMaxCustFilter = 0;
      ofn.nFilterIndex = 1;
      ofn.lpstrFile = sz;
      ofn.nMaxFile = sizeof( sz );
      ofn.lpstrFileTitle = NULL;
      ofn.nMaxFileTitle = 0;
      ofn.lpstrInitialDir = "";
      ofn.lpstrTitle = "CIX Users List";
      ofn.Flags = OFN_NOCHANGEDIR|OFN_OVERWRITEPROMPT|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
      ofn.nFileOffset = 0;
      ofn.nFileExtension = 0;
      ofn.lpstrDefExt = NULL;
      ofn.lCustData = 0;
      ofn.lpfnHook = NULL;
      ofn.lpTemplateName = 0;
      if( !GetOpenFileName( &ofn ) )
         return( FALSE );
      lpszFilename = sz;
      }

   /* Uncompress the file if required.
    */
   UnarcFile( hwnd, lpszFilename );

   /* Open the file. If the first two bytes are 0x4891 then this is a
    * Ameol v1 users list data file. Otherwise treat it as a
    * text file.
    */
   if( ( fh = Amfile_Open( lpszFilename, AOF_READ ) ) == HNDFILE_ERROR )
      FileOpenError( hwnd, lpszFilename, FALSE, FALSE );
   else
      {
      WORD wVersion;
      WORD wOldCount;
      HNDFILE fhOut;
      WORD wDate;
      WORD wTime;

      /* Get the source file date/time.
       */
      Amfile_GetFileTime( fh, &wDate, &wTime );

      /* Create the output file.
       */
      if( ( fhOut = Amfile_Create( szUsersDatFile, 0 ) ) == HNDFILE_ERROR )
         {
         FileCreateError( hwnd, szUsersDatFile, FALSE, FALSE );
         Amfile_Close( fh );
         return( FALSE );
         }

      /* Write a 2-byte header to the file.
       */
      wVersion = USERLIST_VERSION;
      Amfile_Write( fhOut, (LPCSTR)&wVersion, sizeof(WORD) );

      /* Create a gauge window on the status bar.
       */
      wsprintf( szStatusTxt, GS(IDS_STR869), GetFileBasename(lpszFilename) );
      SendMessage( hwndStatus, SB_BEGINSTATUSGAUGE, 0, (LPARAM)(LPSTR)szStatusTxt );

      /* Delete the current list.
       */
      wOldCount = 0;
      fParsingUserList = TRUE;

      /* Check the version word.
       */
      if( sizeof(WORD) == Amfile_Read( fh, (LPSTR)&wVersion, sizeof(WORD) ) )
         if( wVersion == 0x4891 )
            {
            UINT cbBufSize;
            LPSTR lpBuf;

            INITIALISE_PTR(lpBuf);

            /* It's an encoded Ameol USERS.DAT file.
             */
            cbBufSize = 8000;
            if( fNewMemory( &lpBuf, cbBufSize ) )
               {
               DWORD dwAccum;
               DWORD dwSize;
               UINT cRead;

               /* Reset to the start of the actual data.
                */
               Amfile_SetPosition( fh, 2L, ASEEK_BEGINNING );

               /* Read from the file in cbBufSize chunks.
                */
               dwSize = Amfile_GetFileSize( fh );
               dwAccum = 0;
               while( fParsingUserList && 0 != ( cRead = Amfile_Read( fh, lpBuf, cbBufSize ) ) && cRead != -1 )
                  {
                  WORD wCount;

                  /* Yield.
                   */
                  TaskYield();

                  /* Update the status gauge.
                   */
                  dwAccum += cbBufSize;
                  wCount = (WORD)( ( dwAccum * 100 ) / dwSize );
                  if( wCount != wOldCount )
                     {
                     SendMessage( hwndStatus, SB_STEPSTATUSGAUGE, wCount, (LPARAM)(LPSTR)szStatusTxt );
                     wOldCount = wCount;
                     }
                  Amfile_Write( fhOut, lpBuf, cRead );
                  }
               FreeMemory( &lpBuf );
               }
            }
         else
            {
            LPLBF lplbf;
            BOOL fTitle;

            /* It's most likely a text file. Rewind back to
             * the beginning and use the buffer functions to
             * parse the file.
             */
            Amfile_SetPosition( fh, 0L, ASEEK_BEGINNING );
            if( NULL ==  ( lplbf = Amlbf_Open( fh, AOF_READ ) ) )
               OutOfMemoryError( hwnd, FALSE, FALSE );
            else
               {
               UINT cTotalLines;
               DWORD dwSize;
               char chLast;
               BOOL fNew;

               /* Get the input file size.
                */
               dwSize = Amlbf_GetSize( lplbf );
               fTitle = FALSE;
               fNew = FALSE;

               /* Loop for each line.
                */
               cTotalLines = 0;
               chLast = ' ';
               while( fParsingUserList && Amlbf_Read( lplbf, sz, sizeof(sz), NULL, NULL, &fIsAmeol2Scratchpad ) )
                  {
                  DWORD dwAccum;
                  WORD wCount;
                  char szOut[ 160 ];
                  char * npIn;
                  char * npOut;

                  /* Update progress gauge.
                   */
                  dwAccum = Amlbf_GetCount( lplbf );
                  wCount = (WORD)( ( dwAccum * 100 ) / dwSize );
                  if( wCount != wOldCount )
                     {
                     SendMessage( hwndStatus, SB_STEPSTATUSGAUGE, wCount, (LPARAM)(LPSTR)szStatusTxt );
                     wOldCount = wCount;
                     }

                  /* Yield.
                   */
                  TaskYield();

                  /* Examine this line.
                   */
                  npIn = sz;
                  npOut = szOut;
                  if( *npIn )
                     {
                     if( chLast && tolower( *npIn ) != chLast )
                        {
                        if( cTotalLines > 0 )
                           {
                           *npOut++ = HTYP_COMMENT;
                           *npOut = '\0';
                           Amfile_Write( fhOut, szOut, strlen( szOut ) + 1 );
                           ++cTotalLines;
                           }
                        npOut = szOut;
                        *npOut++ = HTYP_TITLE;
                        *npOut++ = '-';
                        *npOut++ = ' ';
                        if( *npIn != ' ' && tolower( *npIn ) < chLast )
                           {
                           strcpy( npOut, "Newcomers" );
                           fNew = TRUE;
                           npOut += 9;
                           chLast = 0;
                           }
                        else {
                           *npOut++ = toupper( *npIn );
                           chLast = tolower( *npIn );
                           }
                        *npOut++ = ' ';
                        *npOut++ = '-';
                        *npOut = '\0';
                        Amfile_Write( fhOut, (LPSTR)szOut, strlen( szOut ) + 1 );
                        ++cTotalLines;
                        npOut = szOut;
                        }
                     *npOut++ = HTYP_ENTRY;
                     *npOut++ = fNew ? 'O' : 'o';
                     while( *npIn && *npIn != '\t' && *npIn != ' ' )
                        *npOut++ = *npIn++;
                     *npOut++ = '\t';
                     while( *npIn == '\t' || *npIn == ' ' )
                        npIn++;
                     while( *npIn )
                        *npOut++ = *npIn++;
                     *npOut++ = '\0';
                     if( *szOut )
                        {
                        Amfile_Write( fhOut, (LPCSTR)szOut, strlen( szOut ) + 1 );
                        ++cTotalLines;
                        }
                     }
                  else
                     {
                     *npOut++ = HTYP_COMMENT;
                     while( *npIn )
                        *npOut++ = *npIn++;
                     }
                  }
               Amfile_Write( fhOut, (char *)&cTotalLines, sizeof( WORD ) );
               }
            }

      /* Step up to 100 if we didn't get there because of rounding
       * errors.
       */
      if( wOldCount != 100 )
         SendMessage( hwndStatus, SB_STEPSTATUSGAUGE, 100, (LPARAM)(LPSTR)szStatusTxt );
      fParsingUserList = FALSE;

      /* Set the source file date/time.
       */
      Amfile_SetFileTime( fhOut, wDate, wTime );

      /* Close the output file.
       */
      Amfile_Close( fhOut );

      /* Delete the gauge window.
       */
      SendMessage( hwndStatus, SB_ENDSTATUSGAUGE, 0, 0L );
      ShowUnreadMessageCount();
      Amfile_Close( fh );
      }
   return( TRUE );
}
