/* GRPLIST.C - Display list of newsgroups
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
#include "cixip.h"
#include "cix.h"
#include "amlib.h"
#include "ameol2.h"
#include <ctype.h>
#include "news.h"

#define  THIS_FILE   __FILE__

#define  MAX_LINELENGTH       510
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

char NEAR szGroupListWndClass[] = "amctl_grpwndcls";
char NEAR szGroupListWndName[] = "Newsgroups List";

static BOOL fDefDlgEx = FALSE;

HWND hwndGrpList = NULL;               /* Handle of usenet groups list window */

static BOOL fRegistered = FALSE;       /* TRUE if we've registered the usenet list window */
static WNDPROC lpProcListCtl;          /* Subclassed listbox window procedure */
static WNDPROC lpProcVListCtl;         /* Subclassed virtual listbox window procedure */
static HPSTR lpLines;                  /* Pointer to text of list (can be > 64K!) */
static HPDWORD lpIndexTable;           /* Index into text */
static LONG cTotalLines;               /* Total lines in array */
static LONG cTotalNewsgroups;          /* Total lines in list */
static UINT nLastSel;                  /* Index of the last selected item in the list */
static BOOL fMouseClick;               /* TRUE if last action was a mouse click */
static BOOL fUp;                       /* TRUE if last list movement was up */
static HFONT hGroupListBUFont = NULL;  /* Bold, underlined newslist list font */
static HFONT hGroupListBFont = NULL;   /* Bold newslist list font */
static int hdrColumns[ 2 ];            /* Header field column widths */
static DWORD cTotalEntryLines;         /* Number of entries in the list */
static HBITMAP hbmpGrpList;            /* CIX list bitmap */
static BOOL fSkipFirst;                /* TRUE if we ignore first EN_CHANGE */
static BOOL fQuickUpdate;              /* TRUE if we skip WM_ERASEBKGND in list window */
static int nGrpViewMode;               /* Newsgroup list view mode */
static HBRUSH hbrGrpList;                 /* Brush used to paint listbox */

BOOL FASTCALL GroupListWnd_OnCreate( HWND, LPCREATESTRUCT );
void FASTCALL GroupListWnd_OnClose( HWND );
void FASTCALL GroupListWnd_OnDestroy( HWND );
void FASTCALL GroupListWnd_OnSize( HWND, UINT, int, int );
void FASTCALL GroupListWnd_OnMove( HWND, int, int );
void FASTCALL GroupListWnd_OnTimer( HWND, UINT );
void FASTCALL GroupListWnd_OnSetFocus( HWND, HWND );
void FASTCALL GroupListWnd_MDIActivate( HWND, BOOL, HWND, HWND );
void FASTCALL GroupListWnd_OnCommand( HWND, int, HWND, UINT );
void FASTCALL GroupListWnd_OnAdjustWindows( HWND, int, int );
void FASTCALL GroupListWnd_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
void FASTCALL GroupListWnd_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
HBRUSH FASTCALL GroupListWnd_OnCtlColor( HWND, HDC, HWND, int );
LRESULT FASTCALL GroupListWnd_OnNotify( HWND, int, LPNMHDR );

#define  NGF_ALL           0
#define  NGF_NEW           1

void FASTCALL BuildGroupListFonts( HWND );
int FASTCALL BuildGroupListIndex( char *, int );

BOOL EXPORT WINAPI UsenetList_Import( HWND, LPSTR );
LRESULT EXPORT CALLBACK GroupListWndProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK GroupListVListProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK GroupListComboBoxProc( HWND, UINT, WPARAM, LPARAM );

/* This function creates the window that will display the full list of
 * usenet newsgroups.
 */
BOOL FASTCALL CreateGroupListWindow( HWND hwnd )
{
   DWORD dwState;
   RECT rc;

   /* Locate USENET.LST and import it if found.
    */
   if( Amfile_QueryFile( szUsenetLstFile ) )
      {
      HWND hwndBill;

      hwndBill = Adm_Billboard( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_IMPORTINGUSENETLIST) );
      UsenetList_Import( hwnd, (LPSTR)szUsenetLstFile );
      Amfile_Delete( szUsenetLstFile );
      DestroyWindow( hwndBill );
      }

   /* Open window if it already exists.
    */
   if( NULL != hwndGrpList )
      {
      if( IsIconic( hwndGrpList ) )
         SendMessage( hwndMDIClient, WM_MDIRESTORE, (WPARAM)hwndGrpList, 0L );
      SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndGrpList, 0L );
      return( TRUE );
      }

   /* Register the usenet list window class if we have
    * not already done so.
    */
   if( !fRegistered )
      {
      WNDCLASS wc;

      wc.style          = CS_HREDRAW | CS_VREDRAW;
      wc.lpfnWndProc    = GroupListWndProc;
      wc.hIcon          = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_GRPLIST) );
      wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
      wc.lpszMenuName   = NULL;
      wc.cbWndExtra     = DLGWINDOWEXTRA;
      wc.cbClsExtra     = 0;
      wc.hbrBackground  = NULL;
      wc.lpszClassName  = szGroupListWndClass;
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
   ReadProperWindowState( szGroupListWndName, &rc, &dwState );

   /* Create the window.
    */
   hwndGrpList = Adm_CreateMDIWindow( szGroupListWndName, szGroupListWndClass, hInst, &rc, dwState, 0L );
   if( NULL == hwndGrpList )
      return( FALSE );

   /* Set the focus to the list box.
    */
   SetFocus( GetDlgItem( hwndGrpList, IDD_FINDTEXT ) );
   return( TRUE );
}

/* This is the main window procedure.
 */
LRESULT EXPORT CALLBACK GroupListWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LPVLBSTRUCT lpvlbInStruct;

   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, GroupListWnd_OnCreate );
      HANDLE_MSG( hwnd, WM_CLOSE, GroupListWnd_OnClose );
      HANDLE_MSG( hwnd, WM_DESTROY, GroupListWnd_OnDestroy );
      HANDLE_MSG( hwnd, WM_SIZE, GroupListWnd_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, GroupListWnd_OnMove );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, Common_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, GroupListWnd_MDIActivate );
      HANDLE_MSG( hwnd, WM_COMMAND, GroupListWnd_OnCommand );
      HANDLE_MSG( hwnd, WM_MENUSELECT, MainWnd_OnMenuSelect );
   #ifdef WIN32
      HANDLE_MSG( hwnd, WM_CTLCOLORLISTBOX, GroupListWnd_OnCtlColor );
   #else
      HANDLE_MSG( hwnd, WM_CTLCOLOR, GroupListWnd_OnCtlColor );
   #endif
      HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, GroupListWnd_OnAdjustWindows );
      HANDLE_MSG( hwnd, WM_DRAWITEM, GroupListWnd_OnDrawItem );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, GroupListWnd_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_NOTIFY, GroupListWnd_OnNotify );
      HANDLE_MSG( hwnd, WM_TIMER, GroupListWnd_OnTimer );
      HANDLE_MSG( hwnd, WM_SETFOCUS, GroupListWnd_OnSetFocus );

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
            lpvlbInStruct->lIndex = lpvlbInStruct->lData * cTotalNewsgroups;
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
         lpvlbInStruct->lIndex = cTotalNewsgroups;
         lpvlbInStruct->nStatus = VLB_OK;
         return TRUE;

      case VLB_NEXT:
         lpvlbInStruct = (LPVLBSTRUCT)lParam;
         if( lpvlbInStruct->lIndex < (LONG)cTotalNewsgroups - 1 )
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
         lpvlbInStruct->lIndex = cTotalNewsgroups - 1;
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
         if( wParam == FONTYP_GRPLIST || 0xFFFF == wParam )
            {
            MEASUREITEMSTRUCT mis;
            HWND hwndList;

            /* Update the usenet list window font. Will also have to
             * rebuild the font variations.
             */
            hwndList = GetDlgItem( hwnd, IDD_LIST );
            BuildGroupListFonts( hwnd );
            SendMessage( hwnd, WM_MEASUREITEM, IDD_LIST, (LPARAM)(LPSTR)&mis );
            SendMessage( hwndList, VLB_SETITEMHEIGHT, 0, MAKELPARAM( mis.itemHeight, 0) );
            InvalidateRect( hwndList, NULL, TRUE );
            UpdateWindow( hwndList );
            }
         return( 0L );

      case WM_APPCOLOURCHANGE:
         if( wParam == WIN_GRPLIST )
            {
            HWND hwndList;

            /* Redraw the usenet list window in the new colours.
             */
            if( NULL != hbrGrpList )
               DeleteBrush( hbrGrpList );
            hbrGrpList = CreateSolidBrush( crGrpListWnd );
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
         lpMinMaxInfo->ptMinTrackSize.x = 410;
         lpMinMaxInfo->ptMinTrackSize.y = 290;
         return( lResult );
         }

      default:
         break;
      }
   return( Adm_DefMDIDlgProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL GroupListWnd_OnCreate( HWND hwnd, LPCREATESTRUCT lpCreateStruct )
{
   static int aArray[ 2 ] = { 300, 260 };
   LPMDICREATESTRUCT lpMDICreateStruct;
   char szDefSrvr[ 64 ];
   HWND hwndList;
   HWND hwndTab;
   TC_ITEM tie;
   HD_ITEM hdi;

   /* Create the dialog window.
    */
   lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
   Adm_MDIDialog( hwnd, MAKEINTRESOURCE(IDDLG_NEWSLIST), lpMDICreateStruct );

   /* Initialise the list box.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   nGrpViewMode = NGF_ALL;
   fQuickUpdate = FALSE;
   SendMessage( hwndList, VLB_INITIALIZE, 0, 0L );

   /* Fill the list of newsgroups.
    */
   Amuser_GetPPString( szNews, "Default Server", szNewsServer, szDefSrvr, 64 );
   FillUsenetServersCombo( hwnd, IDD_SERVERLIST, szDefSrvr );

   /* Create an underlined and boldface variant of the
    * usenet list font.
    */
   BuildGroupListFonts( hwnd );

   /* Read the current header settings.
    */
   Amuser_GetPPArray( szSettings, "NewsListColumns", aArray, 2 );

   /* Set the tab control.
    */
   VERIFY( hwndTab = GetDlgItem( hwnd, IDD_TABS ) );
   tie.mask = TCIF_TEXT;
   tie.pszText = "All";
   TabCtrl_InsertItem( hwndTab, 0, &tie );
   tie.pszText = "        New        ";
   TabCtrl_InsertItem( hwndTab, 1, &tie );
   SetWindowFont( hwndTab, hHelvB8Font, FALSE );

   /* Fill in the header fields.
    */
   hdi.mask = HDI_TEXT|HDI_WIDTH|HDI_FORMAT;
   hdi.cxy = hdrColumns[ 0 ] = aArray[ 0 ];
   hdi.pszText = "Newsgroup";
   hdi.fmt = HDF_STRING|HDF_LEFT;
   Header_InsertItem( hwndList, 0, &hdi );
   
   hdi.cxy = hdrColumns[ 1 ] = aArray[ 1 ];
   hdi.pszText = "Description";
   hdi.fmt = HDF_STRING|HDF_LEFT;
   Header_InsertItem( hwndList, 1, &hdi );

   hbrGrpList = CreateSolidBrush( crGrpListWnd );

   SendMessage( hwndList, WM_SETTEXT, 1 | HBT_SPRING, 0L );
   SetWindowFont( hwndList, hHelv8Font, FALSE );

   /* Set the limit of the search text
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_FINDTEXT ), LEN_FINDTEXT );

   /* Load the bitmaps.
    */
   hbmpGrpList = LoadBitmap( hRscLib, MAKEINTRESOURCE(IDB_GRPLIST) );

   /* Subclass the list box.
    */
   hwndList = GetDlgItem( hwndList, 0x3000 );
   hwndList = GetWindow( hwndList, GW_CHILD );
   lpProcVListCtl = SubclassWindow( hwndList, GroupListVListProc );
   SetWindowText( GetDlgItem( hwnd, IDD_PAD3 ), GS(IDS_STR1164) );
   return( TRUE );
}

/* This function builds two variations of the usenet lists
 * font.
 */
void FASTCALL BuildGroupListFonts( HWND hwnd )
{
   LOGFONT lf;

   /* Delete any old ones.
    */
   if( NULL != hGroupListBFont )
      DeleteFont( hGroupListBFont );
   if( NULL != hGroupListBUFont )
      DeleteFont( hGroupListBUFont );

   /* Create the new ones.
    */
   lf = lfGrpListFont;
   lf.lfWeight = FW_BOLD;
   hGroupListBFont = CreateFontIndirect( &lf );
   lf.lfUnderline = TRUE;
   hGroupListBUFont = CreateFontIndirect( &lf );
}

/* This function handles the WM_SETFOCUS message.
 */
void FASTCALL GroupListWnd_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   SetFocus( GetDlgItem( hwnd, IDD_LIST ) );
}

/* This function processes the WM_COMMAND message.
 */
void FASTCALL GroupListWnd_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_SERVERLIST:
         if( codeNotify == CBN_SELCHANGE )
            {
            char szSubNewsServer[ 64 ];
            int index;

            /* Get the name of the selected server.
             */
            index = ComboBox_GetCurSel( hwndCtl );
            ASSERT( CB_ERR != index );
            ComboBox_GetLBText( hwndCtl, index, szSubNewsServer );

            /* Display the list.
             */
            ReadGroupsDat( hwndFrame, szSubNewsServer );
            }
         break;

      case IDM_QUICKPRINT:
      case IDM_PRINT: {
         PRINTLISTWND plw;

         plw.hwnd = hwnd;
         plw.idDlg = IDDLG_PRINTGRPLIST;
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
         if( codeNotify == EN_CHANGE && !fSkipFirst )
            {
            KillTimer( hwnd, IDD_FINDTEXT );
            SetTimer( hwnd, IDD_FINDTEXT, 1000, NULL );
            fSkipFirst = FALSE;
            }
         break;

      case IDCANCEL:
//       if( GetFocus() != GetDlgItem( hwnd, IDD_FINDTEXT ) )
            GroupListWnd_OnClose( hwnd );
         break;

      case IDD_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, NGF_ALL == nGrpViewMode ? idsGRPLIST_ALL : idsGRPLIST_NEW );
         break;

      case IDD_LIST:
         if( codeNotify == LBN_SELCHANGE && cTotalNewsgroups )
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
                  while( ++index < cTotalNewsgroups )
                     {
                     lpStr = &lpLines[ lpIndexTable[ index ] ];
                     if( *lpStr == HTYP_ENTRY )
                        break;
                     }
               if( index < 0 || index >= cTotalNewsgroups )
                  index = nLastSel;
               SendMessage( hwndCtl, VLB_SETCURSEL, VLB_FINDITEM, (LPARAM)index );
               UpdateWindow( hwndCtl );
               }
            nLastSel = (UINT)index;
            }
         if( codeNotify != LBN_DBLCLK )
            break;

      case IDM_SUBSCRIBE:
      case IDM_RESIGN: {
         HWND hwndList;
         int index;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         if( ( index = (UINT)SendMessage( hwndList, VLB_GETCURSEL, 0, 0L ) ) != LB_ERR )
            {
            char szNewsgroup[ 80 ];
            register int c;
            HPSTR lpStr;
            PTL ptl;

            /* Get the highlighted newsgroup name.
             */
            lpStr = &lpLines[ lpIndexTable[ index ] ];
            ++lpStr;
            ++lpStr;
            for( c = 0; c < 79 && *lpStr && *lpStr != '\t'; ++c )
               szNewsgroup[ c ] = *lpStr++;
            szNewsgroup[ c ] = '\0';

            /* Get this folder handle.
             */
            ptl = PtlFromNewsgroup( szNewsgroup );

            /* Handle toggle.
             */
            if( IDD_LIST == id )
               if( NULL == ptl )
                  id = IDM_SUBSCRIBE;
               else if(( Amdb_IsResignedTopic( ptl ) ) && ( Amdb_GetTopicType( ptl )  == FTYPE_NEWS ))
                  id = IDM_SUBSCRIBE;
               else if ( Amdb_GetTopicType( ptl )  == FTYPE_NEWS )
                  id = IDM_RESIGN;
               else id = IDM_SUBSCRIBE;
            /* Now handle appropriate command.
             */
            switch( id )
               {
               case IDM_RESIGN:
                  if( NULL != ptl )
                     CmdResign( hwnd, ptl, TRUE, FALSE );
                  else
                     {
                     wsprintf( lpTmpBuf, GS(IDS_STR153), (LPSTR)szNewsgroup );
                     fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
                     }
                  break;

               case IDM_SUBSCRIBE: {
                  char szSubNewsServer[ 64 ];
                  SUBSCRIBE_INFO si;
                  HWND hwndSrvrList;
                  int index2;

                  /* Get the selected news server.
                   */
                  VERIFY( hwndSrvrList = GetDlgItem( hwnd, IDD_SERVERLIST ) );
                  index2 = ComboBox_GetCurSel( hwndSrvrList );
                  ASSERT( CB_ERR != index2 );
                  ComboBox_GetLBText( hwndSrvrList, index2, szSubNewsServer );

                  /* Subscribe to that newsgroup
                   */
                  si.pszNewsgroup = szNewsgroup;
                  si.pszNewsServer =  szSubNewsServer;
                  if( CmdSubscribe( hwnd, &si ) )
                     {
                     VLBSTRUCT vlb;
                     RECT rc;

                     /* Show the newsgroup as 'subscribed' in the
                      * list.
                      */
                     vlb.lIndex = index;
                     vlb.lpTextPointer = (LPSTR)&rc;
                     SendMessage( hwndList, VLB_GETITEMRECT, 0, (LPARAM)(LPSTR)&vlb );
                     InvalidateRect( hwndList, &rc, TRUE );
                     UpdateWindow( hwndList );
                     }
                  break;
                  }
               }
            }
         break;
         }

      case IDD_FILTERS: {
         char szSubNewsServer[ 64 ];
         HWND hwndList;
         int index;

         /* Get the selected news server.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_SERVERLIST ) );
         index = ComboBox_GetCurSel( hwndList );
         ASSERT( CB_ERR != index );
         ComboBox_GetLBText( hwndList, index, szSubNewsServer );

         /* Edit filters for that news server
          */
         CmdNewsgroupsFilter( hwnd, szSubNewsServer );
         break;
         }

      case IDD_RESET:
      case IDD_UPDATE: {
         char szSubNewsServer[ 64 ];
         NEWSSERVERINFO nsi;
         HWND hwndList;
         int index;

         /* Get the selected news server.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_SERVERLIST ) );
         index = ComboBox_GetCurSel( hwndList );
         ASSERT( CB_ERR != index );
         ComboBox_GetLBText( hwndList, index, szSubNewsServer );

         /* Get the last update date and time.
          */
         if( GetUsenetServerDetails( szSubNewsServer, &nsi ) )
            {
            BOOL fFullUpdate;
            HNDFILE fh;
            WORD wDate = 0;
            WORD wTime = 0;

            /* If the newsgroups file doesn't exist, then zap
             * the update date/time.
             */
            wsprintf( lpPathBuf, "%s.dat", nsi.szShortName );
            if( IDD_RESET == id )
               fFullUpdate = TRUE;
            else if( HNDFILE_ERROR == ( fh = Amfile_Open( lpPathBuf, AOF_READ ) ) )
               fFullUpdate = TRUE;
            else
               {
               Amfile_GetFileTime( fh, &wDate, &wTime );
               Amfile_Close( fh );
               fFullUpdate = FALSE;
               }

            /* If there is already this command in the out-basket,
             * then open it for editing.
             */
            if( fFullUpdate )
               {
               NEWSGROUPSOBJECT grp;

               /* List not retrieved before, so get it now.
                */
               Amob_New( OBTYPE_GETNEWSGROUPS, &grp );
               Amob_CreateRefString( &grp.recServerName, szSubNewsServer );
               if( Amob_Find( OBTYPE_GETNEWSGROUPS, &grp ) )
                  fMessageBox( hwnd, 0, GS(IDS_STR146), MB_OK|MB_ICONEXCLAMATION );
               else
                  if( fMessageBox( hwnd, 0, GS(IDS_STR105), MB_YESNO|MB_ICONQUESTION ) == IDYES )
                     Amob_Commit( NULL, OBTYPE_GETNEWSGROUPS, &grp );
               Amob_Delete( OBTYPE_GETNEWSGROUPS, &grp );
               }
            else
               {
               NEWNEWSGROUPSOBJECT grp;

               Amob_New( OBTYPE_GETNEWNEWSGROUPS, &grp );
               Amob_CreateRefString( &grp.recServerName, szSubNewsServer );
               if( NULL == Amob_Find( OBTYPE_GETNEWNEWSGROUPS, &grp ) )
                  {
                  /* Fill out the date and time from when we look
                   * for new newsgroups.
                   */
                  Amdate_UnpackDate( wDate, &grp.date );
                  Amdate_UnpackTime( wTime, &grp.time );
               
                  /* The year field must be century based.
                   */
                  ASSERT( grp.date.iYear >= 1900 );
                  grp.date.iYear %= 100;

                  /* Go prompt the user for the required data.
                   */
                  wsprintf( lpTmpBuf, GS(IDS_STR1195),
                            Amdate_FormatTime( &grp.time, NULL ),
                            Amdate_FormatShortDate( &grp.date, NULL ),
                            szSubNewsServer );
                  if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION ) == IDYES )
                     Amob_Commit( NULL, OBTYPE_GETNEWNEWSGROUPS, &grp );
                  }
               Amob_Delete( OBTYPE_GETNEWNEWSGROUPS, &grp );
               }
            }
         break;
         }
      }
}

/* This function handles the WM_MEASUREITEM message
 */
void FASTCALL GroupListWnd_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   HFONT hOldFont;
   HDC hdc;

   hdc = GetDC( hwnd );
   hOldFont = SelectFont( hdc, hGrpListFont );
   GetTextMetrics( hdc, &tm );
   lpMeasureItem->itemHeight = tm.tmHeight + tm.tmExternalLeading + 2;
   SelectFont( hdc, hOldFont );
   ReleaseDC( hwnd, hdc );
}

/* This function handles the WM_DRAWITEM message.
 */
void FASTCALL GroupListWnd_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   COLORREF tmpT;
   COLORREF tmpB;
   HFONT hOldFont;
   char sz2[ MAX_LINELENGTH+1 ];
   RECT rc;
   int len;

   /* If we're drawing the usenet folder list, call
    * UsenetServers_OnDrawItem now.
    */
   if( IDD_SERVERLIST == lpDrawItem->CtlID )
      {
      UsenetServers_OnDrawItem( hwnd, lpDrawItem );
      return;
      }

   /* Do nothing if we're not drawing a valid item.
    */
   if( cTotalNewsgroups == 0 || (LONG)lpDrawItem->itemData < 0 || (LONG)lpDrawItem->itemData >= cTotalNewsgroups )
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
   tmpB = SetBkColor( lpDrawItem->hDC, crGrpListWnd );
   tmpT = SetTextColor( lpDrawItem->hDC, crGrpListText );

   /* Now we're ready to draw.
    */
   if( len && strlen( sz2 + 1 ) )
      {
      BOOL fClosed;
      BOOL fNew;
      int length;
      char * psz;
      SIZE size;

      /* Get the flags.
       */
      fClosed = tolower( sz2[ 1 ] ) == 'c';
      fNew = isupper( sz2[ 1 ] );

      /* Draw an entry. The first part is the newsgroup name, and the
       * second is the description. Each part is separate by a tab.
       */
      psz = sz2 + 2;
      for( length = 0; psz[ length ] && psz[ length ] != '\t'; ++length );
      psz[ length ] = '\0';

      /* Only do this lot if we're repainting the entire
       * line.
       */
      if( lpDrawItem->itemAction & ODA_DRAWENTIRE )
         {
         rc.right = 16;
         ExtTextOut( lpDrawItem->hDC, rc.left, rc.top, ETO_OPAQUE|ETO_CLIPPED, &rc, NULL, 0, NULL );
         if( fNew )
            Amuser_DrawBitmap( lpDrawItem->hDC, rc.left, rc.top, 16, 16, hbmpGrpList, 0 );
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
            SetTextColor( lpDrawItem->hDC, crGrpListText );
            SetBkColor( lpDrawItem->hDC, GetSysColor( COLOR_BTNFACE ) );
            }
         }
         else
            {
            PTL ptl;

            /* If we're subscribed to this newsgroup, show it in the
             * active conference colour.
             */
            ptl = PtlFromNewsgroup( psz );
            if( NULL != ptl )
               {
               if(( !Amdb_IsResignedTopic( ptl ) ) && ( Amdb_GetTopicType( ptl )  == FTYPE_NEWS ))
                  SetTextColor( lpDrawItem->hDC, RGB( 255, 0, 0 ) );
                  else
                  SetTextColor( lpDrawItem->hDC, crGrpListText );
               }
               else
               SetTextColor( lpDrawItem->hDC, crGrpListText );
            SetBkColor( lpDrawItem->hDC, crGrpListWnd );
            }

      /* Adjust the size of the string to fit.
       */
      rc.left = 16;
      hOldFont = SelectFont( lpDrawItem->hDC, hGroupListBFont );
      AdjustNameToFit( lpDrawItem->hDC, psz, ( hdrColumns[ 0 ] - rc.left ) + 1, FALSE, &size );
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
         SelectFont( lpDrawItem->hDC, hGrpListFont );
         SetTextColor( lpDrawItem->hDC, crGrpListText );
         SetBkColor( lpDrawItem->hDC, crGrpListWnd );
         rc.left = rc.right;
         rc.right = hdrColumns[ 0 ] + 1;
         ExtTextOut( lpDrawItem->hDC, rc.left, rc.top, ETO_OPAQUE|ETO_CLIPPED, &rc, NULL, 0, NULL );

         /* Then draw descriptions.
          */
         rc.left = rc.right;
         rc.right = lpDrawItem->rcItem.right;
         psz += length + 1;
         length = strlen( psz );
         ExtTextOut( lpDrawItem->hDC, rc.left, rc.top, ETO_OPAQUE|ETO_CLIPPED, &rc, psz, length, NULL );
         SetTextColor( lpDrawItem->hDC, tmpT );
         }
      SelectFont( lpDrawItem->hDC, hOldFont );
      }
   else if( lpDrawItem->itemAction & ODA_DRAWENTIRE )
      ExtTextOut( lpDrawItem->hDC, rc.left, rc.top, ETO_OPAQUE, &rc, NULL, 0, NULL );
   SetBkColor( lpDrawItem->hDC, tmpB );
   SetTextColor( lpDrawItem->hDC, tmpT );
}

/* This function handles the WM_TIMER message.
 */
void FASTCALL GroupListWnd_OnTimer( HWND hwnd, UINT id )
{
   if( id == IDD_FINDTEXT && cTotalLines > 0 )
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
      iStart = BuildGroupListIndex( szFindText, nGrpViewMode );

      /* Update the list.
       */
      hwndList = GetDlgItem( hwnd, IDD_LIST );
      SendMessage( hwndList, VLB_RESETCONTENT, 0, 0L );
      SendMessage( hwndList, VLB_SETCURSEL, VLB_FINDITEM, (LPARAM)iStart );
      SetCursor( hOldCursor );
      nLastSel = iStart;

      EnableControl( hwnd, IDM_SUBSCRIBE, ( cTotalEntryLines > 0 ) );
      EnableControl( hwnd, IDM_RESIGN, ( cTotalEntryLines > 0 ) );

      /* Set focus back to Find box.
       */
      SetFocus( GetDlgItem( hwndGrpList, IDD_FINDTEXT ) );
      }
}

/* This function handles notification messages from the
 * treeview control.
 */
LRESULT FASTCALL GroupListWnd_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
{
   switch( lpNmHdr->code )
      {
      case TCN_SELCHANGE: {
         char szFindText[ LEN_FINDTEXT ];
         HCURSOR hOldCursor;
         HWND hwndList;
         int iStart;

         /* Build the new list.
          */
         hOldCursor = SetCursor( hWaitCursor );
         nGrpViewMode = TabCtrl_GetCurSel( lpNmHdr->hwndFrom );
         Edit_GetText( GetDlgItem( hwnd, IDD_FINDTEXT ), szFindText, sizeof(szFindText) );
         iStart = BuildGroupListIndex( szFindText, nGrpViewMode );

         /* Change the prompt.
          */
         SetWindowText( GetDlgItem( hwnd, IDD_PAD3 ), GS(IDS_STR1164 + nGrpViewMode) );

         /* Update the list.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         SendMessage( hwndList, VLB_RESETCONTENT, 0, 0L );
         SendMessage( hwndList, VLB_SETCURSEL, VLB_FINDITEM, (LPARAM)iStart );
         SetCursor( hOldCursor );
         nLastSel = iStart;

         EnableControl( hwnd, IDM_SUBSCRIBE, ( cTotalEntryLines > 0 ) );
         EnableControl( hwnd, IDM_RESIGN, ( cTotalEntryLines > 0 ) );

         /* Set focus back to Find box.
          */
         SetFocus( GetDlgItem( hwndGrpList, IDD_FINDTEXT ) );
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
            Amuser_WritePPArray( szSettings, "NewsListColumns", hdrColumns, 2 );

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
LRESULT EXPORT CALLBACK GroupListVListProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
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
         hPopupMenu = GetPopupMenu( IPOP_GRPLISTWINDOW );
         GetCursorPos( &pt );
         TrackPopupMenu( hPopupMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndGrpList, NULL );
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
      }
   return( CallWindowProc( lpProcVListCtl, hwnd, msg, wParam, lParam ) );
}

/* This function handles the WM_CTLCOLOR message.
 */
HBRUSH FASTCALL GroupListWnd_OnCtlColor( HWND hwnd, HDC hdc, HWND hwndChild, int type )
{
   switch( GetDlgCtrlID( hwndChild ) )
      {
      case IDD_LIST:
         SetBkColor( hdc, crGrpListWnd );
         SetTextColor( hdc, crGrpListText );
         return( hbrGrpList );
      }
   return( FORWARD_WM_CTLCOLORLISTBOX( hwnd, hdc, hwndChild, Adm_DefMDIDlgProc ) );
}

/* This function handles the WM_MDIACTIVATE message.
 */
void FASTCALL GroupListWnd_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_GRPLIST ), (LPARAM)(LPSTR)hwnd );
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, Adm_DefMDIDlgProc );
}

/* Open the usenet list file and fill the list with
 * items from the file. This function can be called at any time to
 * reinitialise the list of newsgroups in the window.
 */
void FASTCALL ReadGroupsDat( HWND hwnd, char * pszNewsServer )
{
   NEWSSERVERINFO nsi;
   HCURSOR hOldCursor;
   HWND hwndList;
   DWORD dwSize;
   UINT iStart;
   HNDFILE fh;

   /* Make sure usenet list window is visible.
    */
   CreateGroupListWindow( hwnd );

   /* Find the news server info.
    */
   if( !GetUsenetServerDetails( pszNewsServer, &nsi ) )
      {
      pszNewsServer = (char *)szCixnews;
      GetUsenetServerDetails( pszNewsServer, &nsi );
      }

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
   cTotalNewsgroups = 0;
   lpIndexTable = NULL;
   lpLines = NULL;
   nLastSel = 0;
   fUp = fMouseClick = FALSE;

   /* Vape the text in the Find box.
    */
   fSkipFirst = TRUE;
   Edit_SetText( GetDlgItem( hwndGrpList, IDD_FINDTEXT ), "" );
   fSkipFirst = FALSE;

   /* Create the newsgroup list filename.
    */
   BEGIN_PATH_BUF;
   wsprintf( lpPathBuf, "%s.dat", nsi.szShortName );

   /* Open the file. If we can't find it, then clear all
    * items.
    */
   if( ( fh = Amfile_Open( lpPathBuf, AOF_READ ) ) == HNDFILE_ERROR )
      {
      SetDlgItemText( hwndGrpList, IDD_LASTUPDATED, GS(IDS_STR341) );
      SetDlgItemText( hwndGrpList, IDD_NUMBER, "" );
      EnableControl( hwndGrpList, IDM_SUBSCRIBE, FALSE );
      EnableControl( hwndGrpList, IDM_RESIGN, FALSE );
      EnableControl( hwndGrpList, IDD_UPDATE, FALSE );
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
      if( nVersion != GRPLIST_VERSION )
         {
         SetDlgItemText( hwndGrpList, IDD_LASTUPDATED, GS(IDS_STR342) );
         SetDlgItemText( hwndGrpList, IDD_NUMBER, "" );
         EnableControl( hwndGrpList, IDM_SUBSCRIBE, FALSE );
         EnableControl( hwndGrpList, IDD_FIND, FALSE );
         EnableControl( hwndGrpList, IDM_RESIGN, FALSE );
         }
      else
         {
         /* Allocate memory for the entire list.
          */
//       dwSize -= sizeof( WORD ) * 2;
         dwSize -= 6; /*!!SM!! Not sure why it's 6, should be 4 for a long??*/
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
//             Amfile_Read( fh, (LPSTR)&cTotalLines, sizeof( WORD ) ); 

               Amfile_Read32( fh, (LPSTR)&cTotalLines, sizeof( LONG ) ); /*!!SM!!*/
               if ( cTotalLines <= 0 ) 
               {
                  SetDlgItemText( hwndGrpList, IDD_LASTUPDATED, GS(IDS_STR342) );
                  SetDlgItemText( hwndGrpList, IDD_NUMBER, "" );
                  EnableControl( hwndGrpList, IDM_SUBSCRIBE, FALSE );
                  EnableControl( hwndGrpList, IDD_FIND, FALSE );
                  EnableControl( hwndGrpList, IDM_RESIGN, FALSE );
               }
               else
               {
                  /* Now build the index using an empty qualifier.
                   */
                  iStart = BuildGroupListIndex( "", nGrpViewMode );
               
                  /* Show the date and time when this list was downloaded
                   */
      //          Amfile_GetFileLocalTime( fh, &uDate, &uTime ); // !!SM!!
                  Amfile_GetFileTime( fh, &uDate, &uTime );
                  wsprintf( lpTmpBuf, GS(IDS_STR336), Amdate_FriendlyDate( NULL, uDate, uTime ) );
                  SetDlgItemText( hwndGrpList, IDD_LASTUPDATED, lpTmpBuf );
                  EnableControl( hwndGrpList, IDM_SUBSCRIBE, TRUE );
                  EnableControl( hwndGrpList, IDM_RESIGN, TRUE );
               }
            }
         else
            SetDlgItemText( hwndGrpList, IDD_LASTUPDATED, GS(IDS_STR344) );
         EnableControl( hwndGrpList, IDD_UPDATE, strcmp( pszNewsServer, szCixnews ) != 0 );
         }
      Amfile_Close( fh );
      }
   SetCursor( hOldCursor );

   /* All done, so start the virtual list control
    * so that it'll start loading items.
    */
   VERIFY( hwndList = GetDlgItem( hwndGrpList, IDD_LIST ) );
   SendMessage( hwndList, VLB_RESETCONTENT, 0, 0L );
   SendMessage( hwndList, VLB_SETCURSEL, VLB_FINDITEM, (LPARAM)iStart );
   nLastSel = iStart;
   END_PATH_BUF;
}

/* This function builds an index into the Usenet list table.
 */
int FASTCALL BuildGroupListIndex( char * pszFindText, int nViewMode )
{
   int iStart;

   /* Create an index table
    */
   if( lpIndexTable )
      FreeMemory32( &lpIndexTable );
   iStart = 0;
   cTotalEntryLines = 0;
   if( cTotalLines )
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
         cTotalNewsgroups = 0;
         for( c = 0; c < (UINT)cTotalLines; ++c )
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
         cTotalNewsgroups = iIndex;
         if( iStart == -1 )
            iStart = 0;
         }

   /* Show the number of items in the list
    */
   wsprintf( lpTmpBuf, GS(IDS_STR343), FormatThousands( cTotalEntryLines, NULL ) );
   SetDlgItemText( hwndGrpList, IDD_NUMBER, lpTmpBuf );
   return( iStart );
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL GroupListWnd_OnAdjustWindows( HWND hwnd, int dx, int dy )
{
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_FINDTEXT ), dx, 0 );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_LIST ), dx, dy );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_TABS ), dx, dy );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_PAD2 ), dx, 0 );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_LASTUPDATED ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_UPDATE ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_RESET ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_LASTUPDATED ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_NUMBER ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDM_SUBSCRIBE ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDM_RESIGN ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_FILTERS ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDCANCEL ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_HELP ), dx, 0 );
}

/* This function handles the WM_CLOSE message.
 */
void FASTCALL GroupListWnd_OnClose( HWND hwnd )
{
   Adm_DestroyMDIWindow( hwnd );
}

/* This function handles the WM_DESTROY message.
 */
void FASTCALL GroupListWnd_OnDestroy( HWND hwnd )
{
   /* Delete resources.
    */
   DeleteBitmap( hbmpGrpList );
   DeleteFont( hGroupListBUFont );
   DeleteFont( hGroupListBFont );
   hwndGrpList = NULL;
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL GroupListWnd_OnMove( HWND hwnd, int x, int y )
{
   Amuser_WriteWindowState( szGroupListWndName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL GroupListWnd_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   Amuser_WriteWindowState( szGroupListWndName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* Subscribe to the selected newsgroup. If no newsgroup is
 * selected, or the hierarchy item is selected, then the user is
 * presented with a blank Subscribe box.
 */
BOOL FASTCALL Subscribe( HWND hwnd, LPSTR lpNewsgroup )
{
   SUBSCRIBE_INFO si;
   char sz[ 80 ];

   /* Get the selected item, if any.
    */
   strcpy( sz, lpNewsgroup );
   if( *sz == '\0' && hwndTopic && hwndActive == hwndTopic )
      GetMarkedName( hwndTopic, sz, sizeof(sz), FALSE );

   /* No selected item, try the newsgroup list window.
    */
   if( *sz == '\0' )
      if( hwndActive && hwndActive == hwndGrpList )
         {
         HWND hwndList;
         int index;

         hwndList = GetDlgItem( hwndGrpList, IDD_LIST );
         if( ( index = (UINT)SendMessage( hwndList, VLB_GETCURSEL, 0, 0L ) ) != LB_ERR )
            {
            register int c;
            HPSTR lpStr;

            /* Get the highlighted newsgroup name.
             */
            lpStr = &lpLines[ lpIndexTable[ index ] ];
            ++lpStr;
            ++lpStr;
            for( c = 0; c < 79 && *lpStr && *lpStr != '\t'; ++c )
               sz[ c ] = *lpStr++;
            sz[ c ] = '\0';
            }
         }
      else
         {
         CURMSG curmsg;

         /* Get the current topic. If it is a newsgroup,
          * use that.
          */
         Ameol2_GetCurrentTopic( &curmsg );
         if( NULL != curmsg.ptl && Amdb_GetTopicType( curmsg.ptl ) == FTYPE_NEWS )
            strcpy( sz, Amdb_GetTopicName( curmsg.ptl ) );
         }
   si.pszNewsgroup = sz;
   si.pszNewsServer = NULL;
   return( CmdSubscribe( hwnd, &si ) );
}

/* This is the New usenet newsgroups list out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_GetNewNewsgroups( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   NEWNEWSGROUPSOBJECT FAR * lpnng;

   lpnng = (NEWNEWSGROUPSOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_NEW:
         _fmemset( lpnng, 0, sizeof(NEWNEWSGROUPSOBJECT) );
         return( TRUE );

      case OBEVENT_COPY: {
         NEWNEWSGROUPSOBJECT FAR * lpnngNew;

         INITIALISE_PTR( lpnngNew );
         if( fNewMemory( &lpnngNew, sizeof( NEWNEWSGROUPSOBJECT ) ) )
            {
            INITIALISE_PTR( lpnngNew->recServerName.hpStr );
            Amob_CopyRefObject( &lpnngNew->recServerName, &lpnng->recServerName );
            lpnngNew->date = lpnng->date;
            lpnngNew->time = lpnng->time;
            }
         return( (LRESULT)lpnngNew );
         }

      case OBEVENT_LOAD: {
         NEWNEWSGROUPSOBJECT nng;

         INITIALISE_PTR(lpnng);
         Amob_LoadDataObject( fh, &nng, sizeof( NEWNEWSGROUPSOBJECT ) );
         if( fNewMemory( &lpnng, sizeof( NEWNEWSGROUPSOBJECT ) ) )
            {
            *lpnng = nng;
            lpnng->recServerName.hpStr = NULL;
            }
         return( (LRESULT)lpnng );
         }

      case OBEVENT_EXEC:
         if( strcmp( DRF(lpnng->recServerName), szCixnews ) == 0 )
            return( POF_HELDBYSCRIPT );
         return( Exec_GetNewNewsgroups( dwData ) );

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpnng->recServerName );
         return( Amob_SaveDataObject( fh, lpnng, sizeof( NEWNEWSGROUPSOBJECT ) ) );

      case OBEVENT_EDITABLE:
         return( FALSE );

      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR241), Amdate_FormatTime( &lpnng->time, NULL ),
                                          Amdate_FormatShortDate( &lpnng->date, NULL ),
                                          DRF(lpnng->recServerName) );
         return( TRUE );

      case OBEVENT_FIND: {
         NEWNEWSGROUPSOBJECT FAR * lpng1;
         NEWNEWSGROUPSOBJECT FAR * lpng2;

         lpng1 = (NEWNEWSGROUPSOBJECT FAR *)dwData;
         lpng2 = (NEWNEWSGROUPSOBJECT FAR *)lpBuf;
         return( strcmp( lpng1->recServerName.hpStr, lpng2->recServerName.hpStr ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpnng );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpnng->recServerName );
         return( TRUE );
      }
   return( 0L );
}

/* This is the Usenet newsgroups list out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_GetNewsgroups( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   NEWSGROUPSOBJECT FAR * lpng;

   lpng = (NEWSGROUPSOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_GETTYPE:
         return( BF_GETIPNEWS );

      case OBEVENT_EXEC:
         if( strcmp( DRF(lpng->recServerName), szCixnews ) == 0 )
            return( POF_HELDBYSCRIPT );
         return( Exec_GetNewsgroups( dwData ) );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR242), DRF(lpng->recServerName) );
         return( TRUE );

      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_NEW:
         _fmemset( lpng, 0, sizeof(NEWSGROUPSOBJECT) );
         return( TRUE );

      case OBEVENT_LOAD: {
         NEWSGROUPSOBJECT ngo;

         INITIALISE_PTR(lpng);
         Amob_LoadDataObject( fh, &ngo, sizeof(NEWSGROUPSOBJECT) );
         if( fNewMemory( &lpng, sizeof(NEWSGROUPSOBJECT) ) )
            {
            *lpng = ngo;
            lpng->recServerName.hpStr = NULL;
            }
         return( (LRESULT)lpng );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpng->recServerName );
         return( Amob_SaveDataObject( fh, lpng, sizeof(NEWSGROUPSOBJECT) ) );

      case OBEVENT_COPY: {
         NEWSGROUPSOBJECT FAR * lpngNew;

         INITIALISE_PTR( lpngNew );
         if( fNewMemory( &lpngNew, sizeof(NEWSGROUPSOBJECT) ) )
            {
            INITIALISE_PTR( lpngNew->recServerName.hpStr );
            Amob_CopyRefObject( &lpngNew->recServerName, &lpng->recServerName );
            }
         return( (LRESULT)lpngNew );
         }

      case OBEVENT_FIND: {
         NEWSGROUPSOBJECT FAR * lpng1;
         NEWSGROUPSOBJECT FAR * lpng2;

         lpng1 = (NEWSGROUPSOBJECT FAR *)dwData;
         lpng2 = (NEWSGROUPSOBJECT FAR *)lpBuf;
         return( strcmp( lpng1->recServerName.hpStr, lpng2->recServerName.hpStr ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpng );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpng->recServerName );
         return( TRUE );
      }
   return( 0L );
}

/* This function is the usenet list import entry point.
 */
BOOL EXPORT WINAPI UsenetList_Import( HWND hwnd, LPSTR lpszFilename )
{
   char szStatusTxt[ 60 ];
   char sz[ _MAX_PATH ];
   HNDFILE fh;
   BOOL fParsingGroupList;

   /* Initialise variables.
    */
   fMainAbort = FALSE;

   /* If no filename supplied, prompt the user
    * for one.
    */
   if( NULL == lpszFilename )
      {
      static char strFilters[] = "Ameol Usenet list\0usenet.dat\0All Files (*.*)\0*.*\0\0";
      OPENFILENAME ofn;

      strcpy( sz, "usenet.dat" );
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
      ofn.lpstrTitle = "CIX usenet list";
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
    * Ameol v1 usenet list data file. Otherwise treat it as a
    * text file.
    */
   if( ( fh = Amfile_Open( lpszFilename, AOF_READ ) ) == HNDFILE_ERROR )
      FileOpenError( hwnd, lpszFilename, FALSE, FALSE );
   else
      {
      NEWSSERVERINFO nsi;
      WORD wVersion;
      WORD wOldCount;
      HNDFILE fhOut;
      WORD wDate;
      WORD wTime;

      /* Get the source file date/time.
       */
      Amfile_GetFileTime( fh, &wDate, &wTime );

      /* Find the news server info.
       */
      GetUsenetServerDetails( (char *)szCixnews, &nsi );

      /* Create the output file.
       */
      wsprintf( lpPathBuf, "%s.dat", nsi.szShortName );
      if( ( fhOut = Amfile_Create( lpPathBuf, 0 ) ) == HNDFILE_ERROR )
         {
         FileCreateError( hwnd, lpPathBuf, FALSE, FALSE );
         Amfile_Close( fh );
         return( FALSE );
         }

      /* Write a 2-byte header to the file.
       */
      wVersion = GRPLIST_VERSION;
      Amfile_Write( fhOut, (LPCSTR)&wVersion, sizeof(WORD) );

      /* Create a gauge window on the status bar.
       */
      wsprintf( szStatusTxt, GS(IDS_STR869), GetFileBasename(lpszFilename) );
      SendMessage( hwndStatus, SB_BEGINSTATUSGAUGE, 0, (LPARAM)(LPSTR)szStatusTxt );

      /* Delete the current list.
       */
      wOldCount = 0;
      fParsingGroupList = TRUE;

      /* Check the version word.
       */
      if( sizeof(WORD) == Amfile_Read( fh, (LPSTR)&wVersion, sizeof(WORD) ) )
         if( wVersion == 0x4891 )
            {
            UINT cbBufSize;
            LPSTR lpBuf;

            INITIALISE_PTR(lpBuf);

            /* It's an encoded Ameol USENET.DAT file.
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
               while( fParsingGroupList && 0 != ( cRead = Amfile_Read( fh, lpBuf, cbBufSize ) ) && cRead != -1 )
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
               WORD wFilterType;
//             UINT cTotalLines;
               static LONG cTotalLines; /*!!SM!!*/
               LPSTR lpFilters;
               DWORD dwSize;

               /* Get the input file size.
                */
               dwSize = Amlbf_GetSize( lplbf );
               fTitle = FALSE;

               /* Load the filters for the cixnews
                */
               lpFilters = LoadFilters( &nsi, &wFilterType );

               /* Loop for each line.
                */
               cTotalLines = 0;
               while( fParsingGroupList && Amlbf_Read( lplbf, sz, sizeof(sz), NULL, NULL, &fIsAmeol2Scratchpad ) )
                  {
                  DWORD dwAccum;
                  WORD wCount;
                  char szOut[ 160 ];
                  char * npIn;
                  char * npOut;
                  char * npEnd;

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
                  npEnd = szOut + 159;
                  if( *npIn == '!' )
                     continue;
                  *npOut++ = HTYP_ENTRY;
                  *npOut++ = 'o';
                  while( *npIn && *npIn != '\t' && *npIn != ' ' && npOut < npEnd )
                     *npOut++ = *npIn++;
                  *npOut++ = '\t';
                  while( *npIn == '\t' || *npIn == ' ' )
                     npIn++;
                  while( *npIn && npOut < npEnd )
                     *npOut++ = *npIn++;
                  *npOut++ = '\0';
                  if( *szOut )
                     {
                     if( NULL != lpFilters )
                        if( MatchFilters( lpFilters, wFilterType, szOut + 2 ) )
                           continue;
                     Amfile_Write( fhOut, szOut, strlen( szOut ) + 1 );
                     ++cTotalLines;
                     }
                  }

               /* Write total lines as last word in file.
                */
//             Amfile_Write( fhOut, (LPSTR)&cTotalLines, sizeof( WORD ) );
               Amfile_Write( fhOut, (LPSTR)&cTotalLines, sizeof( LONG ) ); /*!!SM!!*/

               /* Delete any filters.
                */
               if( NULL != lpFilters )
                  FreeMemory( &lpFilters );
               }
            }

      /* Step up to 100 if we didn't get there because of rounding
       * errors.
       */
      if( wOldCount != 100 )
         SendMessage( hwndStatus, SB_STEPSTATUSGAUGE, 100, (LPARAM)(LPSTR)szStatusTxt );
      fParsingGroupList = FALSE;

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
