/* CUSTTOOL.C - The Customise Toolbar dialog
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
#include "string.h"
#include "amlib.h"
#include "command.h"
#include <shellapi.h>
#include "admin.h"

#define  THIS_FILE   __FILE__

typedef struct _TBSAVED {
   TBBUTTON FAR * lpButtonArray;
   int cArraySize;
} TBSAVED;

HWND hwndToolBarOptions = NULL;           /* Toolbar customisation dialog box */

static BOOL fDefDlgEx = FALSE;            /* DefDlg recursion flag trap */
static TBSAVED tbsMain;                   /* Saved toolbar state */
static WNDPROC lpProcListCtl;             /* Subclassed listbox window procedure */
static CMDREC mci;                        /* Structure of command entry for button being dragged */
static HCURSOR hcurToolbarDrag;           /* Handle of button drag cursor */
static HCURSOR hcurNoDrop;                /* Handle of cursor for when user is over a no-drop area */
static HCURSOR hcurCur;                   /* The current cursor */
static HCURSOR hcurOld;                   /* The saved cursor */
static BOOL fDragging = FALSE;            /* TRUE if we're dragging a button */
static BOOL fButtonDown = FALSE;          /* TRUE if pressed the left mouse button */
static POINT ptLast;                      /* Current coordinates of the cursor */
static HDC hdcDrag;                       /* HDC for the drag button bitmap */
static HDC hdcSaved;                      /* HDC for the saved screen background bitmap */
static HBITMAP hbmpDrag;                  /* The drag button bitmap */
static HBITMAP hbmpOld;                   /* The old bitmap selected in hdcDrag */
static HBITMAP hbmpOldSaved;              /* The old bitmap selected in hdcSaved */
static HBITMAP hbmpSaved;                 /* The saved screen bitmap */
static int iSelIndex;                     /* The index of the button being dragged */
static int cxButton;                      /* Button width */
static int cyButton;                      /* Button height */
static BOOL fDoneBitmap;                  /* TRUE if the screen background needs redrawing */
static BOOL fToolbarChanged;              /* TRUE if the toolbar has been changed */
static BOOL fPermUseTooltips;             /* These five variables save the */
static BOOL fPermLargeButtons;               /* original toolbar options status */
static BOOL fPerm3DStyle;
static BOOL fPermButtonLabels;
static BOOL fPermVisibleSeperators;
static BOOL fPermNewButtons;

static int lastindex = 0;                 /* Index of last selected category */

BOOL EXPORT CALLBACK CustToolbar( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL CustToolbar_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL CustToolbar_OnCommand( HWND, int, HWND, UINT );
void FASTCALL CustToolbar_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
void FASTCALL CustToolbar_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
HBRUSH FASTCALL CustToolbar_OnCtlColor( HWND, HDC, HWND, int );
LRESULT FASTCALL CustToolbar_OnNotify( HWND, int, LPNMHDR );

void FASTCALL FillCategoryList( HWND );
BOOL FASTCALL ReadToolbar( HWND, TBSAVED FAR * );
BOOL FASTCALL AddButtonToToolbar( HWND, int, int, int );
void FASTCALL AddExternalAppToToolbar( LPAPPINFO, int );
void FASTCALL UpdateButtonListStatus( HWND );

BOOL EXPORT CALLBACK ExtAppDlg( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL ExtAppDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL ExtAppDlg_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL ExtAppDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );

void FASTCALL DeleteCustomButtons( void );

LRESULT EXPORT FAR PASCAL CustToolbarListProc( HWND, UINT, WPARAM, LPARAM );

/* This function starts the toolbar customisation.
 */
BOOL FASTCALL CmdCustomiseToolbar( HWND hwnd )
{
   fPermUseTooltips = fUseTooltips;
   fPermLargeButtons = fLargeButtons;
   fPerm3DStyle = f3DStyle;
   fPermButtonLabels = fButtonLabels;
   fPermVisibleSeperators = fVisibleSeperators;
   fPermNewButtons = fNewButtons;
   
   if( TestPerm(PF_CANCONFIGURE) )
      if( hwndToolBarOptions )
         BringWindowToTop( hwndToolBarOptions );
      else
         hwndToolBarOptions = Adm_CreateDialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_TOOLBAR), (DLGPROC)CustToolbar, 0L );
   return( TRUE );
}

/* This function dispatches messages for the Toolbar page of the
 * Customise dialog.
 */
BOOL EXPORT CALLBACK CustToolbar( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, CustToolbar_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, CustToolbar_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_DRAWITEM, CustToolbar_OnDrawItem );
      HANDLE_DLGMSG( hwnd, WM_MEASUREITEM, CustToolbar_OnMeasureItem );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, CustToolbar_OnNotify );

      case WM_ADMHELP: {
         HWND hwndTab;

         VERIFY( hwndTab = GetDlgItem( hwnd, IDD_TABS ) );
         if( 0 == TabCtrl_GetCurSel( hwndTab ) )
            HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsTOOLBAR_OPTIONS );
         else
            HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsTOOLBAR_CONFIG );
         break;
         }

      case WM_CLOSE:
         /* Switch off toolbar edit mode.
          */
         SendMessage( hwndToolBar, TB_EDITMODE, FALSE, 0L );
         DestroyWindow( hwnd );
         hwndToolBarOptions = NULL;
         break;
      }
   return( FALSE );
}

/* Handle the WM_NOTIFY message.
 */
LRESULT FASTCALL CustToolbar_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
{
   switch( lpNmHdr->code )
      {
      case TCN_SELCHANGE: {
         BOOL fPage1;

         fPage1 = TabCtrl_GetCurSel( lpNmHdr->hwndFrom ) == 0;
         ShowEnableControl( hwnd, IDD_PAD3, !fPage1 );
         ShowEnableControl( hwnd, IDD_PAD2, !fPage1 );
         ShowEnableControl( hwnd, IDD_LIST, !fPage1 );
         ShowEnableControl( hwnd, IDD_CATEGORIES, !fPage1 );
         ShowEnableControl( hwnd, IDD_SHOWTOOLTIPS, fPage1 );
         ShowEnableControl( hwnd, IDD_LARGEBUTTONS, fPage1 );
         ShowEnableControl( hwnd, IDD_3DSTYLE, fPage1 );
         ShowEnableControl( hwnd, IDD_VISSEP, fPage1 );
         ShowEnableControl( hwnd, IDD_BUTTONLABELS, fPage1 );
         ShowEnableControl( hwnd, IDD_NEWTB, fPage1 );
         if( fPage1 )
            {
            SendMessage( hwndToolBar, TB_EDITMODE, FALSE, 0L );
            ShowEnableControl( hwnd, IDD_PAD1, FALSE );
            }
         else
            {
            SendMessage( hwndToolBar, TB_EDITMODE, TRUE, 0L );
            UpdateButtonListStatus( hwnd );
            }
         return( 0L );
         }

      case TBN_TOOLBARCHANGE:
         fToolbarChanged = TRUE;
         break;

      case TBN_QUERYINSERT:
         return( TRUE );

      case TBN_QUERYDELETE: {
         CMDREC msi;
         LPTBNOTIFY lptbn;
         HWND hwndList;
         BOOL fDone;
         int index;
         int index2;
         HCMD hCmd;

         /* Button deleted from toolbar, so add it to the
          * listbox as well.
          */
         lptbn = (LPTBNOTIFY)lpNmHdr;
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         fDone = FALSE;
         index = index2 = 0;
         hCmd = NULL;
         while( !fDone && ( NULL != ( hCmd = CTree_EnumTree( hCmd, &msi ) ) ) )
            {
            /* For each command found, if it appears in the list box, save its index
             * so that we add back the deleted button in the proper place.
             */
            if( ( index2 = ListBox_FindItemData( hwndList, -1, hCmd ) ) != -1 )
               index = index2 + 1;
            else if( msi.iCommand == (UINT)lptbn->tbButton.idCommand )
               {
               HWND hwndCats;
               WORD wCategory;
               int index3;

               /* Get the active category and skip if we're not showing
                * the category to which this button belongs.
                */
               hwndCats = GetDlgItem( hwnd, IDD_CATEGORIES );
               index3 = ListBox_GetCurSel( hwndCats );
               ASSERT( index3 != LB_ERR );
               wCategory = (WORD)ListBox_GetItemData( hwndCats, index3 );
               if( wCategory != msi.wCategory )
                  {
                  fDone = TRUE;
                  break;
                  }

               /* Found a match, so add back the deleted button.
                */
               index = ListBox_InsertItemData( hwndList, index, hCmd );
               ListBox_SetCurSel( hwndList, index );
//             UpdateButtonListStatus( hwnd );
               fDone = TRUE;
               fToolbarChanged = TRUE;
               break;
               }
            }
         SetFocus( hwndToolBarOptions );
         return( TRUE );
         }
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL CustToolbar_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   CATEGORYITEM ci;
   HWND hwndList;
   HWND hwndTab;
   TC_ITEM tie;
   WORD i;

   /* Fill the categories list box, then select the first
    * category from the list.
    */
   hwndList = GetDlgItem( hwnd, IDD_CATEGORIES );
   for( i = 0; CTree_GetCategory( i++, &ci ); )
      {
      int index;

      /* Separate the menu from the non-menu categories.
       */
      if( ci.wCategory == CAT_NON_MENU )
         {
         index = ListBox_InsertString( hwndList, -1, "--------------------------" );
         ListBox_SetItemData( hwndList, index, 0xFFFF );
         }
      if( ci.wCategory != CAT_EXTAPP && ci.wCategory != CAT_FORMS )
         {
         index = ListBox_InsertString( hwndList, -1, ci.pCategory );
         ListBox_SetItemData( hwndList, index, ci.wCategory );
         }
      }
   ListBox_SetCurSel( hwndList, 0 );

   /* Set the tab control.
    */
   VERIFY( hwndTab = GetDlgItem( hwnd, IDD_TABS ) );
   tie.mask = TCIF_TEXT;
   tie.pszText = "Options";
   TabCtrl_InsertItem( hwndTab, 0, &tie );
   tie.pszText = "Customise";
   TabCtrl_InsertItem( hwndTab, 1, &tie );
   SetWindowFont( hwndTab, hHelvB8Font, FALSE );

   /* Fill the listbox with those buttons which are
    * NOT already on the toolbar.
    */
   FillCategoryList( hwnd );

   /* Save the current toolbar state. We'll use this to restore
    * the state if the user clicks Cancel.
    */
   INITIALISE_PTR( tbsMain.lpButtonArray );
   ReadToolbar( hwndToolBar, &tbsMain );
   fToolbarChanged = FALSE;

   /* Put the toolbar into edit mode.
    */
   SendMessage( hwndToolBar, TB_EDITMODE, TRUE, 0L );

   /* Subclass the listbox so we can handle dragging.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   lpProcListCtl = SubclassWindow( hwndList, CustToolbarListProc );

   /* Set the options.
    */
   CheckDlgButton( hwnd, IDD_SHOWTOOLTIPS, fUseTooltips );
   CheckDlgButton( hwnd, IDD_LARGEBUTTONS, fLargeButtons );
   CheckDlgButton( hwnd, IDD_3DSTYLE, f3DStyle );
   CheckDlgButton( hwnd, IDD_VISSEP, fVisibleSeperators );
   CheckDlgButton( hwnd, IDD_BUTTONLABELS, fButtonLabels );
   CheckDlgButton( hwnd, IDD_NEWTB, fNewButtons );
   EnableControl( hwnd, IDD_VISSEP, !f3DStyle );
   return( TRUE );
}

/* This function fills the tool button listbox with those
 * buttons in the selected category.
 */
void FASTCALL FillCategoryList( HWND hwnd )
{
   WORD wCategory;
   HWND hwndList;
   int index;
   CMDREC msi;
   HCMD hCmd;

   /* Get the selected category.
    */
   hwndList = GetDlgItem( hwnd, IDD_CATEGORIES );
   index = ListBox_GetCurSel( hwndList );
   ASSERT( index != LB_ERR );
   wCategory = (WORD)ListBox_GetItemData( hwndList, index );

   /* If this is a separator, select the previous
    * category.
    */
   if( 0xFFFF == wCategory )
      {
      if( lastindex < index )
         ListBox_SetCurSel( hwndList, ++index );
      else
         ListBox_SetCurSel( hwndList, --index );
      wCategory = (WORD)ListBox_GetItemData( hwndList, index );
      ASSERT( 0xFFFF != wCategory );
      }

   /* Fill the toolbutton list with those buttons in
    * the selected category.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   ListBox_ResetContent( hwndList );
   hCmd = NULL;
   while( NULL != ( hCmd = CTree_EnumTree( hCmd, &msi ) ) )
      {
      if( msi.wCategory == wCategory )
         if( SendMessage( hwndToolBar, TB_COMMANDTOINDEX, msi.iCommand, 0L ) == -1 )
            ListBox_InsertString( hwndList, -1, hCmd );
      }

   /* Take action depending on whether or not the list
    * box is empty.
    */
   UpdateButtonListStatus( hwnd );
   if( 0 == ListBox_GetCount( hwndList ) )
      ListBox_SetCurSel( hwndList, 0 );
   lastindex = index;
}

/* This function intercepts messages for the Customise Toolbar list box window
 * so that we can handle dragging from the listbox.
 */
LRESULT EXPORT FAR PASCAL CustToolbarListProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   switch( msg )
      {
      case WM_MOUSEMOVE:
         if( fButtonDown )
            {
            int index;
            POINT pt;

            /* Which item are we over, if any?
             */
            pt.x = (short)LOWORD( lParam );
            pt.y = (short)HIWORD( lParam );
            index = ListBox_GetTopIndex( hwnd ) + ( pt.y / ListBox_GetItemHeight( hwnd, 0 ) );
            if( index >= 0 && index < ListBox_GetCount( hwnd ) )
               {
               TBDRAWBUTTON tbdb;
               SIZE size;
               HCMD hCmd;
               HDC hdc;

               /* Get ready for dragon drop.
                */
               hcurToolbarDrag = LoadCursor( hRscLib, MAKEINTRESOURCE(IDC_TOOLBARDRAG) );
               hcurNoDrop = LoadCursor( hRscLib, MAKEINTRESOURCE(IDC_NONODROP) );
               hcurCur = hcurToolbarDrag;
               hCmd = (HCMD)ListBox_GetItemData( hwnd, index );
               CTree_GetItem( hCmd, &mci );
               hcurOld = GetCursor();
               fDragging = TRUE;
               iSelIndex = index;
               fDoneBitmap = FALSE;

               /* Create a DC containing the button bitmap and another
                * to which we'll save the screen background.
                */
               hdc = GetDC( hwndFrame );
               hdcDrag = CreateCompatibleDC( hdc );
               hdcSaved = CreateCompatibleDC( hdc );
               tbdb.tb.iBitmap = mci.iToolButton;
               tbdb.tb.idCommand = mci.iCommand;
               tbdb.tb.fsState = TBSTATE_ENABLED;
               tbdb.tb.fsStyle = TBSTYLE_BUTTON;
               if( mci.iCommand >= IDM_DYNAMICCOMMANDS )
                  tbdb.tb.fsStyle |= TBSTYLE_CALLBACK;
               tbdb.tb.dwData = 0L;
               tbdb.tb.iString = LOWORD( mci.lpszString );
               SendMessage( hwndToolBar, TB_GETBITMAPSIZE, 0, (LPARAM)(LPSTR)&size );
               tbdb.rc.right = size.cx + 8;
               tbdb.rc.bottom = size.cy + 7;
               tbdb.rc.left = 0;
               tbdb.rc.top = 0;
               cxButton = tbdb.rc.right;
               cyButton = tbdb.rc.bottom;

               /* Create bitmaps compatible with the display
                * attributes. These will hold the button bitmap and
                * screen background.
                */
               hbmpDrag = CreateCompatibleBitmap( hdc, cxButton, cyButton );
               hbmpSaved = CreateCompatibleBitmap( hdc, cxButton, cyButton );

               /* Draw the button bitmap into hdcDrag, so we'll just BitBlt this
                * to the screen which will be considerably faster than redrawing
                * the button on each mouse move.
                */
               hbmpOld = SelectBitmap( hdcDrag, hbmpDrag );
               hbmpOldSaved = SelectBitmap( hdcSaved, hbmpSaved );
               SendMessage( hwndToolBar, TB_DRAWBUTTON, (WPARAM)hdcDrag, (LPARAM)(LPSTR)&tbdb );
               ReleaseDC( hwndFrame, hdc );
               }
            fButtonDown = FALSE;
            }
         else if( fDragging )
            {
            HWND hwndAtCursor;
            HDC hdc;
            RECT rc;
            POINT pt;

            /* Restore the screen portion overwritten by the
             * bitmap. This might look horrible on a slow machine!
             */
            hdc = GetDC( NULL );
            if( fDoneBitmap )
               {
               BitBlt( hdc, ptLast.x, ptLast.y, cxButton, cyButton, hdcSaved, 0, 0, SRCCOPY );
               fDoneBitmap = FALSE;
               }

            /* Where is the cursor? Check for it being in the list box then
             * check for it being on the toolbar. If neither of these, put up
             * the NoDrop cursor and finish now.
             */
            pt.x = (short)LOWORD( lParam );
            pt.y = (short)HIWORD( lParam );
            GetClientRect( hwnd, &rc );
            if( !PtInRect( &rc, pt ) )
               {
               MapWindowPoints( hwnd, hwndFrame, &pt, 1 );
               hwndAtCursor = ChildWindowFromPoint( hwndFrame, pt );
               if( hwndAtCursor != hwndToolBar )
                  {
                  SetCursor( hcurCur = hcurNoDrop );
                  ReleaseDC( NULL, hdc );
                  return( 0L );
                  }
               pt.x = (short)LOWORD( lParam );
               pt.y = (short)HIWORD( lParam );
               }
               

            /* Draw the button bitmap at the current mouse coordinates, offset by 16
             * pixels, then the cursor.
             */
            ClientToScreen( hwnd, &pt );
            pt.x -= 16;
            pt.y -= 16;
            BitBlt( hdcSaved, 0, 0, cxButton, cyButton, hdc, pt.x, pt.y, SRCCOPY );
            BitBlt( hdc, pt.x, pt.y, cxButton, cyButton, hdcDrag, 0, 0, SRCCOPY );
            ptLast = pt;
            SetCursor( hcurCur = hcurToolbarDrag );
            fDoneBitmap = TRUE;
            ReleaseDC( NULL, hdc );
            return( 0L );
            }
         break;

      case WM_SETCURSOR:
         if( fDragging )
            {
            SetCursor( hcurCur );
            return( TRUE );
            }
         break;

      case WM_LBUTTONUP:
         if( fDragging )
            {
            POINT pt;
   
            /* Restore the screen portion overwritten by the
             * bitmap.
             */
            if( fDoneBitmap )
               {
               HDC hdc;

               hdc = GetDC( NULL );
               BitBlt( hdc, ptLast.x, ptLast.y, cxButton, cyButton, hdcSaved, 0, 0, SRCCOPY );
               ReleaseDC( NULL, hdc );
               }

            /* First switch off the drag stuff.
             */
            SetCursor( hcurOld );
            DestroyCursor( hcurToolbarDrag );
            DestroyCursor( hcurNoDrop );
            ReleaseCapture();
            fDragging = FALSE;

            /* Clean up the DC stuff.
             */
            SelectBitmap( hdcSaved, hbmpOldSaved );
            SelectBitmap( hdcDrag, hbmpOld );
            DeleteDC( hdcSaved );
            DeleteDC( hdcDrag );
            DeleteBitmap( hbmpSaved );
            DeleteBitmap( hbmpDrag );

            /* Okay, now are we over the toolbar?
             */
            pt.x = (short)LOWORD( lParam );
            pt.y = (short)HIWORD( lParam );
            MapWindowPoints( hwnd, hwndFrame, &pt, 1 );
            if( ChildWindowFromPoint( hwndFrame, pt ) == hwndToolBar )
               {
               int iDropIndex;

               /* Yup. Figure out where on the toolbar the cursor was
                * released, then call TB_ADDBUTTON to add a new button
                * before that position.
                */
               MapWindowPoints( hwndFrame, hwndToolBar, &pt, 1 );
               iDropIndex = (int)SendMessage( hwndToolBar, TB_HITTEST, 0, MAKELPARAM(pt.x, pt.y) );
               if( iDropIndex != TBHT_NOWHERE )
                  {
                  /* Back up, as we're inserting BEFORE the selected
                   * item.
                   */
                  if( iDropIndex >= 0 )
                     --iDropIndex;

                  /* Add this button to the toolbar.
                   */
                  if( !AddButtonToToolbar( GetParent(hwnd), mci.iToolButton, mci.iCommand, iDropIndex ) )
                     break;

                  /* Also delete it from the listbox if it is not
                   * one of the persistent ones.
                   */
                  if( IDM_EXTERNALAPP != mci.iCommand && IDM_SEP != mci.iCommand )
                     {
                     if( ListBox_DeleteString( hwnd, iSelIndex ) == ListBox_GetCount( hwnd ) )
                        --iSelIndex;
                     ListBox_SetCurSel( hwnd, iSelIndex );
                     UpdateButtonListStatus( GetParent( hwnd ) );
                     }
                  fToolbarChanged = TRUE;
                  }
               }
            return( 0L );
            }
         break;

      case WM_LBUTTONDOWN:
         /* First, make sure the selected item is selected.
          */
         CallWindowProc( lpProcListCtl, hwnd, msg, wParam, lParam );
         CallWindowProc( lpProcListCtl, hwnd, WM_LBUTTONUP, wParam, lParam );

         /* Want all mouse messages.
          */
         SetCapture( hwnd );
         fButtonDown = TRUE;
         return( 0L );
      }
   return( CallWindowProc( lpProcListCtl, hwnd, msg, wParam, lParam ) );
}

/* This function updates the status of the list of buttons
 * in the Customise toolbar dialog. If the list is empty, it
 * disables the control. If it is not empty and disabled, it
 * re-enables the control.
 */
void FASTCALL UpdateButtonListStatus( HWND hwnd )
{
   HWND hwndList;
   int count;

   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   count = ListBox_GetCount( hwndList );
   if( 0 == count && IsWindowEnabled( hwndList ) )
      {
      EnableWindow( hwndList, FALSE );
      ListBox_SetCurSel( hwndList, -1 );
      ShowWindow( GetDlgItem( hwnd, IDD_PAD1 ), SW_SHOW );
      ShowWindow( hwndList, SW_HIDE );
      }
   else if( 0 < count && !IsWindowEnabled( hwndList ) )
      {
      ShowWindow( GetDlgItem( hwnd, IDD_PAD1 ), SW_HIDE );
      ShowWindow( hwndList, SW_SHOW );
      EnableWindow( hwndList, TRUE );
      }
}

/* This function adds a button to the toolbar. If the button
 * ID is not -1, then it is an index into a preloaded bitmap.
 * Otherwise the user must select a bitmap to assign to the
 * button.
 */
BOOL FASTCALL AddButtonToToolbar( HWND hwnd, int iToolButton, int iCommand, int iDropIndex )
{
   if( IDM_EXTERNALAPP == iCommand )
      {
      APPINFO appinfo;

      /* We're installing an external application, so bring up
       * the dialog box from which the user can select the
       * application.
       */
      memset( &appinfo, 0, sizeof(appinfo) );
      if( !Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_EXTAPP), ExtAppDlg, (LPARAM)(LPSTR)&appinfo ) )
         return( FALSE );

      /* Get the bitmap for this application and pass it to
       * the toolbar in advance. hIcon will be deleted by the
       * IconToBitmap function. hbmp will get deleted whenever
       * the toolbar is deleted.
       */
      AddExternalAppToToolbar( &appinfo, iDropIndex );
      }
   else if( IDM_SEP == iCommand )
      {
      TBBUTTON tb;

      tb.iBitmap = 10;
      tb.idCommand = 0;
      tb.fsState = TBSTATE_ENABLED;
      tb.fsStyle = TBSTYLE_SEP;
      tb.dwData = 0;
      tb.iString = 0;
      if( iCommand >= IDM_DYNAMICCOMMANDS )
         tb.fsStyle |= TBSTYLE_CALLBACK;
      SendMessage( hwndToolBar, TB_INSERTBUTTON, iDropIndex, (LPARAM)(LPSTR)&tb );
      }
   else
      {
      TBBUTTON tb;

      tb.iBitmap = iToolButton;
      tb.idCommand = iCommand;
      tb.fsState = TBSTATE_ENABLED;
      tb.fsStyle = TBSTYLE_BUTTON;
      tb.dwData = 0;
      tb.iString = tb.iBitmap;
      if( iCommand >= IDM_DYNAMICCOMMANDS )
         tb.fsStyle |= TBSTYLE_CALLBACK;
      SendMessage( hwndToolBar, TB_INSERTBUTTON, iDropIndex, (LPARAM)(LPSTR)&tb );
      }
   return( TRUE );
}

/* This function adds the specified external application to
 * the toolbar.
 */
void FASTCALL AddExternalAppToToolbar( LPAPPINFO lpAppInfo, int iDropIndex )
{
   TBBUTTON tb;
   int iCommand;

   /* Add this application as a command to the command
    * table.
    */
   iCommand = InstallExternalApp( lpAppInfo );

   /* Okay. Now add the lot to the toolbar.
    */
   tb.iBitmap = 0;
   tb.idCommand = iCommand;
   tb.fsState = TBSTATE_ENABLED;
   tb.fsStyle = TBSTYLE_BUTTON|TBSTYLE_CALLBACK;
   tb.dwData = 0;
   tb.iString = 0;
   SendMessage( hwndToolBar, TB_INSERTBUTTON, iDropIndex, (LPARAM)(LPSTR)&tb );
   END_PATH_BUF;

}

/* This function retrieves the bitmap for an application.
 */
HBITMAP FASTCALL GetExternalAppBitmap( CMDREC FAR * pmsi )
{
   APPINFO appInfo;
   HBITMAP hBmp;
   HICON hIcon;
#ifdef WIN32
   WORD index;
#endif

   /* Extract the bitmap for this application.
    */
   GetExternalApp( pmsi->iCommand, &appInfo );
   StripCharacter( appInfo.szAppPath, '"' );
#ifdef WIN32
   index = 0;
   hIcon = ExtractAssociatedIcon( hInst, appInfo.szAppPath, &index ); 
#else
   hIcon = ExtractIcon( hInst, appInfo.szAppPath, 0 ); 
#endif
   if( NULL == hIcon )
      hIcon = LoadIcon( NULL, IDI_APPLICATION );

   /* Now hIcon is the application icon. Convert it to a bitmap
    * and cache it.
    */
   if( fLargeButtons )
      {
      if( NULL != pmsi->btnBmp.hbmpLarge )
         DeleteBitmap( pmsi->btnBmp.hbmpLarge );
      hBmp = pmsi->btnBmp.hbmpLarge = IconToBitmap( hIcon, 24, 24 );
      }
   else
      {
      if( NULL != pmsi->btnBmp.hbmpSmall )
         DeleteBitmap( pmsi->btnBmp.hbmpSmall );
      hBmp = pmsi->btnBmp.hbmpSmall = IconToBitmap( hIcon, 16, 16 );
      }
   pmsi->fCustomBitmap = TRUE;
   return( hBmp );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL CustToolbar_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_CATEGORIES:
         if( codeNotify == LBN_SELCHANGE )
            FillCategoryList( hwnd );
         break;

      case IDD_VISSEP:
      case IDD_BUTTONLABELS:
      case IDD_3DSTYLE:
      case IDD_NEWTB:
      case IDD_LARGEBUTTONS: {
         BOOL fTmpLargeButtons;
         BOOL fTmpButtonLabels;
         BOOL fTmp3DStyle;
         BOOL fTmpVisibleSeperators;
         BOOL fTmpNewButtons;

         /* Has the button size actually changed?
          */
         fTmpVisibleSeperators = IsDlgButtonChecked( hwnd, IDD_VISSEP );
         fTmpLargeButtons = IsDlgButtonChecked( hwnd, IDD_LARGEBUTTONS );
         fTmp3DStyle = IsDlgButtonChecked( hwnd, IDD_3DSTYLE );
         fTmpButtonLabels = IsDlgButtonChecked( hwnd, IDD_BUTTONLABELS );
         fTmpNewButtons = IsDlgButtonChecked( hwnd, IDD_NEWTB );
         if( fTmpLargeButtons != fLargeButtons || fTmp3DStyle != f3DStyle || fTmpButtonLabels != fButtonLabels || fTmpVisibleSeperators != fVisibleSeperators || fTmpNewButtons != fNewButtons )
            {
            HWND hwndList;
            TBSAVED tbs;
            SIZE size;

            /* Save the new button size state information.
             */
            fVisibleSeperators = fTmpVisibleSeperators;
            fLargeButtons = fTmpLargeButtons;
            f3DStyle = fTmp3DStyle;
            fButtonLabels = fTmpButtonLabels;
            fNewButtons = fTmpNewButtons;
            Amuser_WritePPInt( szPreferences, "LargeButtons", fLargeButtons );
            Amuser_WritePPInt( szPreferences, "3DToolbar", f3DStyle );
            Amuser_WritePPInt( szPreferences, "ButtonLabels", fButtonLabels );
            Amuser_WritePPInt( szPreferences, "VisibleSeperators", fVisibleSeperators );
            Amuser_WritePPInt( szPreferences, "NewButtons", fNewButtons );
            fToolbarChanged = TRUE;
         
            EnableControl( hwnd, IDD_VISSEP, !f3DStyle );

            /* Now reload the toolbar.
             */
            INITIALISE_PTR( tbs.lpButtonArray );
            ReadToolbar( hwndToolBar, &tbs );
            UpdateBlinkBitmaps();
            UpdateTerminalBitmaps();
            ChangeToolbar( hwndFrame, TRUE, tbs.lpButtonArray, tbs.cArraySize );
            FreeMemory( &tbs.lpButtonArray );

            /* Put the toolbar back into edit mode.
             */
            SendMessage( hwndToolBar, TB_EDITMODE, TRUE, 0L );

            /* Update the listbox to show the buttons in the appropriate
             * size as well.
             */
            hwndList = GetDlgItem( hwnd, IDD_LIST );
            SendMessage( hwndToolBar, TB_GETBITMAPSIZE, 0, (LPARAM)(LPSTR)&size );
            SendMessage( hwndList, LB_SETITEMHEIGHT, 0, MAKELPARAM( size.cy + 15, 0) );
            InvalidateRect( hwndList, NULL, TRUE );
            }
         break;
         }

      case IDD_SHOWTOOLTIPS:
         fUseTooltips = IsDlgButtonChecked( hwnd, IDD_SHOWTOOLTIPS );
         Amuser_WritePPInt( szPreferences, "Tooltips", fUseTooltips );
         break;

      case IDD_RESET: {
         BOOL fPage1;

         /* First revert back to the default toolbar.
          */
         ChangeToolbar( hwndFrame, TRUE, NULL, 0 );

         /* Vape custom button bitmap mappings.
          */
         DeleteCustomButtons();

         /* Put the toolbar into edit mode.
          */
         SendMessage( hwndToolBar, TB_EDITMODE, TRUE, 0L );

         /* Now fill the listbox again with those buttons not
          * on the toolbar.
          */
         FillCategoryList( hwnd );
         fToolbarChanged = TRUE;

         /* Fixup toolbar buttons
          */
         SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_STOP, fOnline );
         SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_ONLINE, !fOnline );
         ToolbarState_Permissions();
         ToolbarState_Printer();
         ToolbarState_Import();
         ToolbarState_EditWindow();

         fPage1 = TabCtrl_GetCurSel( GetDlgItem( hwnd, IDD_TABS)  ) == 0;
         ShowEnableControl( hwnd, IDD_PAD3, !fPage1 );
         ShowEnableControl( hwnd, IDD_PAD2, !fPage1 );
         ShowEnableControl( hwnd, IDD_LIST, !fPage1 );
         ShowEnableControl( hwnd, IDD_CATEGORIES, !fPage1 );
         ShowEnableControl( hwnd, IDD_SHOWTOOLTIPS, fPage1 );
         ShowEnableControl( hwnd, IDD_LARGEBUTTONS, fPage1 );
         ShowEnableControl( hwnd, IDD_3DSTYLE, fPage1 );
         ShowEnableControl( hwnd, IDD_VISSEP, fPage1 );
         ShowEnableControl( hwnd, IDD_BUTTONLABELS, fPage1 );
         ShowEnableControl( hwnd, IDD_NEWTB, fPage1 );
         if( fPage1 )
            {
            SendMessage( hwndToolBar, TB_EDITMODE, FALSE, 0L );
            ShowEnableControl( hwnd, IDD_PAD1, FALSE );
            }
         else
            {
            SendMessage( hwndToolBar, TB_EDITMODE, TRUE, 0L );
            UpdateButtonListStatus( hwnd );
            }

         break;
         }

      case IDOK:
         /* OK button clicked, so save the toolbar.
          */
         if( fToolbarChanged )
            SaveToolbar( hwnd );
         FreeMemory( &tbsMain.lpButtonArray );
         PostMessage( hwnd, WM_CLOSE, 0, 0L );
         break;

      case IDCANCEL:
         /* Cancel button clicked, so restore the toolbar that was
          * when the Customise dialog was invoked.
          */
         Amuser_WritePPInt( szPreferences, "LargeButtons", fPermLargeButtons );
         Amuser_WritePPInt( szPreferences, "3DToolbar", fPerm3DStyle );
         Amuser_WritePPInt( szPreferences, "ButtonLabels", fPermButtonLabels );
         Amuser_WritePPInt( szPreferences, "Tooltips", fPermUseTooltips );
         Amuser_WritePPInt( szPreferences, "NewButtons", fPermNewButtons );
         Amuser_WritePPInt( szPreferences, "VisibleSeperators", fPermVisibleSeperators );
         if( fToolbarChanged )
            ChangeToolbar( hwndFrame, TRUE, tbsMain.lpButtonArray, tbsMain.cArraySize );
         UpdateBlinkBitmaps();
         UpdateTerminalBitmaps();
         FreeMemory( &tbsMain.lpButtonArray );
         PostMessage( hwnd, WM_CLOSE, 0, 0L );
         break;
      }
}

/* This function handles the WM_MEASUREITEM message.
 */
void FASTCALL CustToolbar_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpmis )
{
   HBITMAP hbmp;
   BITMAP bmp;

   if( fLargeButtons )
      hbmp = LoadBitmap( hRscLib, MAKEINTRESOURCE(IDB_BIGBITMAP1) );
   else
      hbmp = LoadBitmap( hRscLib, MAKEINTRESOURCE(IDB_BITMAP1) );
   GetObject( hbmp, sizeof( BITMAP ), &bmp );
   DeleteBitmap( hbmp );
   lpmis->itemHeight = bmp.bmHeight + 15;
}

/* This function handles the WM_DRAWITEM message.
 */
void FASTCALL CustToolbar_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   COLORREF tmpT;
   COLORREF tmpB;
   COLORREF T;
   COLORREF B;
   HBRUSH hbr;
   RECT rc;
      
   /* Set the brush and pen colours and save the
    * current text and background colours.
    */
   if( lpDrawItem->itemState & ODS_SELECTED )
      {
      T = GetSysColor( COLOR_HIGHLIGHTTEXT );
      B = GetSysColor( COLOR_HIGHLIGHT );
      }
   else
      {
      T = GetSysColor( COLOR_WINDOWTEXT );
      B = GetSysColor( COLOR_WINDOW );
      }
   tmpT = GetTextColor( lpDrawItem->hDC );
   tmpB = GetBkColor( lpDrawItem->hDC );

   /* Erase the drawing area.
    */
   hbr = CreateSolidBrush( B );
   rc = lpDrawItem->rcItem;
   FillRect( lpDrawItem->hDC, &rc, hbr );

   /* If there is an item to draw, draw it.
    */
   if( lpDrawItem->itemID != -1 )
      {
      CMDREC msi;
      TBDRAWBUTTON tbdb;
      HFONT hOldFont;
      LPCSTR psz;
      RECT rc2;
      SIZE size;
      HCMD hCmd;

      /* Get the index of the command we're drawing.
       */
      hCmd = (HCMD)ListBox_GetItemData( lpDrawItem->hwndItem, lpDrawItem->itemID );

      /* Draw the button.
       */
      CTree_GetItem( hCmd, &msi );
      tbdb.tb.iBitmap = msi.iToolButton;
      tbdb.tb.idCommand = msi.iCommand;
      tbdb.tb.fsState = TBSTATE_ENABLED;
      tbdb.tb.fsStyle = TBSTYLE_BUTTON;
      if( msi.iCommand >= IDM_DYNAMICCOMMANDS )
         tbdb.tb.fsStyle |= TBSTYLE_CALLBACK;
      tbdb.tb.dwData = 0L;
      tbdb.tb.iString = LOWORD( msi.lpszString );
      SendMessage( hwndToolBar, TB_GETBITMAPSIZE, 0, (LPARAM)(LPSTR)&size );
      tbdb.rc.right = size.cx + 8;
      tbdb.rc.bottom = size.cy + 7;
      tbdb.rc.left = 0;
      tbdb.rc.top = 0;
      OffsetRect( &tbdb.rc, rc.left + 2, rc.top + 2 );
      SendMessage( hwndToolBar, TB_DRAWBUTTON, (WPARAM)lpDrawItem->hDC, (LPARAM)(LPSTR)&tbdb );

      /* Draw the description, vertically centered
       */
      SetTextColor( lpDrawItem->hDC, T );
      SetBkColor( lpDrawItem->hDC, B );
      rc.left = tbdb.rc.right + 8;
      rc.right -= 8;
      rc2 = rc;
      hOldFont = SelectFont( lpDrawItem->hDC, hHelv8Font );
      psz = msi.lpszString;
      if( HIWORD( psz ) == 0 )
         psz = GS( LOWORD( msi.lpszString ) );
      DrawText( lpDrawItem->hDC, psz, -1, &rc2, DT_NOPREFIX|DT_CALCRECT|DT_WORDBREAK );
      rc.top = rc.top + ( ( ( rc.bottom - rc.top ) - ( rc2.bottom - rc2.top ) ) / 2 );
      rc.bottom = rc.top + ( rc2.bottom - rc2.top );
      DrawText( lpDrawItem->hDC, psz, -1, &rc, DT_NOPREFIX|DT_WORDBREAK );

      /* Clean up before we go home.
       */
      SelectFont( lpDrawItem->hDC, hOldFont );
      SetTextColor( lpDrawItem->hDC, tmpT );
      SetBkColor( lpDrawItem->hDC, tmpB );
      }

   /* If this item has the focus, draw the focus box.
    */
   if( lpDrawItem->itemState & ODS_FOCUS )
      DrawFocusRect( lpDrawItem->hDC, &lpDrawItem->rcItem );
   DeleteBrush( hbr );
}

/* This function reads the toolbar into an array. This is used to "save" the
 * state of the toolbar so that it can be restored at a later date. The caller
 * is responsible for doing a FreeMemory() on the array.
 */
BOOL FASTCALL ReadToolbar( HWND hwnd, TBSAVED FAR * lptbs )
{
   lptbs->cArraySize = (int)SendMessage( hwndToolBar, TB_BUTTONCOUNT, 0, 0L );
   if( fNewMemory( &lptbs->lpButtonArray, lptbs->cArraySize * sizeof(TBBUTTON) ) )
      {
      register int c;

      for( c = 0; c < lptbs->cArraySize; ++c )
         SendMessage( hwndToolBar, TB_GETBUTTON, c, (LPARAM)(LPSTR)&lptbs->lpButtonArray[ c ] );
      return( TRUE );
      }
   return( FALSE );
}

/* This function edits the specified command line.
 */
BOOL FASTCALL CmdExternalApp( HWND hwnd, LPAPPINFO lpAppInfo )
{
   if( Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_EXTAPP), ExtAppDlg, (LPARAM)lpAppInfo ) )
      SaveToolbar( hwnd );
   return( TRUE );
}

/* This function handles the ExtAppDlg dialog box
 */
BOOL EXPORT CALLBACK ExtAppDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, ExtAppDlg_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the Login
 * dialog.
 */
LRESULT FASTCALL ExtAppDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, ExtAppDlg_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, ExtAppDlg_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsEXTAPP );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL ExtAppDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPAPPINFO lpAppInfo;

   /* Save the input parameter
    */
   ASSERT( 0L != lParam );
   SetWindowLong( hwnd, DWL_USER, lParam );
   lpAppInfo = (LPAPPINFO)lParam;

   /* Extract the filename and parameters and store
    * them in the input fields.
    */
   Edit_SetText( GetDlgItem( hwnd, IDD_FILENAME ), lpAppInfo->szAppPath );
   Edit_SetText( GetDlgItem( hwnd, IDD_PARAMETERS ), lpAppInfo->szCmdLine );

   /* Set the input field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_FILENAME ), sizeof(lpAppInfo->szAppPath) - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_PARAMETERS ), sizeof(lpAppInfo->szCmdLine) - 1 );

   /* Set the picture button bitmaps.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_PARMPICKER ), hInst, IDB_PICKER );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_BROWSE ), hInst, IDB_FILEBUTTON );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL ExtAppDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_BROWSE: {
         static char strFilters[] = "Programs (*.exe)\0*.EXE\0All Files (*.*)\0*.*\0\0";
         OPENFILENAME ofn;
         HWND hwndEdit;

         hwndEdit = GetDlgItem( hwnd, IDD_FILENAME );
         Edit_GetText( hwndEdit, lpTmpBuf, LEN_TEMPBUF );
         if( ( strlen( lpTmpBuf ) > 1 ) && ( lpTmpBuf[ ( strlen( lpTmpBuf ) - 1 ) ] == '\\' ) && ( lpTmpBuf[ ( strlen( lpTmpBuf ) - 2 ) ] == ':' ) )
            lpTmpBuf[ ( strlen( lpTmpBuf ) - 1 ) ] = '\0';
         ofn.lStructSize = sizeof( OPENFILENAME );
         ofn.hwndOwner = hwnd;
         ofn.hInstance = NULL;
         ofn.lpstrFilter = strFilters;
         ofn.lpstrCustomFilter = NULL;
         ofn.nMaxCustFilter = 0;
         ofn.nFilterIndex = 1;
         ofn.lpstrFile = lpTmpBuf;
         ofn.nMaxFile = _MAX_FNAME / 2;
         ofn.lpstrFileTitle = NULL;
         ofn.nMaxFileTitle = 0;
         ofn.lpstrInitialDir = NULL;
         ofn.lpstrTitle = "External Program";
         ofn.Flags = OFN_NOCHANGEDIR|OFN_HIDEREADONLY|OFN_FILEMUSTEXIST;
         ofn.nFileOffset = 0;
         ofn.nFileExtension = 0;
         ofn.lpstrDefExt = NULL;
         ofn.lCustData = 0;
         ofn.lpfnHook = NULL;
         ofn.lpTemplateName = 0;
         if( GetOpenFileName( &ofn ) )
            {
            Edit_SetText( hwndEdit, lpTmpBuf );
            Edit_SetSel( hwndEdit, 0, 32767 );
            SetFocus( hwndEdit );
            }
         break;
         }

      case IDM_PPR_AUTHOR:
         Edit_ReplaceSel( GetDlgItem( hwnd, IDD_PARAMETERS ), "%a" );
         break;

      case IDM_PPR_SELECTED:
         Edit_ReplaceSel( GetDlgItem( hwnd, IDD_PARAMETERS ), "%s" );
         break;

      case IDD_PARMPICKER: {
         HMENU hPopupMenu;
         RECT rc;

         GetWindowRect( GetDlgItem( hwnd, id ), &rc );
         hPopupMenu = GetSubMenu( hPopupMenus, IPOP_PARMPICKER );
         TrackPopupMenu( hPopupMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, rc.right, rc.top, 0, hwnd, NULL );
         break;
         }

      case IDOK: {
         LPAPPINFO lpAppInfo;
         HWND hwndEdit;

         /* Get the buffer to which we're writing the
          * command line stuff.
          */
         lpAppInfo = (LPAPPINFO)GetWindowLong( hwnd, DWL_USER );

         /* First get the application name and verify that
          * it exists. Then wrap any portion of the path that
          * contains spaces in quotes.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_FILENAME );
         Edit_GetText( hwndEdit, lpAppInfo->szAppPath, sizeof(lpAppInfo->szAppPath) );
         QuotifyFilename( lpAppInfo->szAppPath );

         /* Get the parameters field, if they exist.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_PARAMETERS );
         Edit_GetText( hwndEdit, lpAppInfo->szCmdLine, sizeof(lpAppInfo->szCmdLine) );
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}
