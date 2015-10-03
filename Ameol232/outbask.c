/* OUTBASK.C - Implements the Palantir out-basket
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
#include "amlib.h"
#include "print.h"

#define  THIS_FILE   __FILE__

#define  MWE_FOCUS         (DLGWINDOWEXTRA)
#define  MWE_EXTRA         (DLGWINDOWEXTRA+4)

char NEAR szOutBaskWndClass[] = "amctl_outbaskcls";
char NEAR szOutBaskWndName[] = "Out Basket";

static BOOL fDefDlgEx = FALSE;            /* DefDlg recursion flag trap */

static BOOL fRegistered = FALSE;          /* TRUE if we've registered the out-basket window class */
static HBITMAP hbmpOutBasket;             /* Out-basket bitmaps */
static WNDPROC lpProcListCtl;             /* Subclassed listbox window procedure */
static HBRUSH hbrOutBaskWnd = NULL;       /* Handle of out-basket window brush */
static BOOL fIgnoreDeleteEvent = FALSE;   /* TRUE if we ignore the AE_DELETING... event */
static int cCopies;                    /* Number of copies to print */

HWND hwndOutBasket = NULL;                /* Handle of out-basket window */

void FASTCALL AddStringToOutBasket( HWND, LPOB );
void FASTCALL ShowOutBasketTotal( HWND );
void FASTCALL DeleteFromOutBasket( LPOB );

BOOL FASTCALL OutBasket_OnCreate( HWND, LPCREATESTRUCT );
void FASTCALL OutBasket_OnClose( HWND );
void FASTCALL OutBasket_OnSize( HWND, UINT, int, int );
void FASTCALL OutBasket_OnMove( HWND, int, int );
void FASTCALL OutBasket_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
void FASTCALL OutBasket_OnCommand( HWND, int, HWND, UINT );
void FASTCALL OutBasket_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
void FASTCALL OutBasket_MDIActivate( HWND, BOOL, HWND, HWND );
HBRUSH FASTCALL OutBasket_OnCtlColor( HWND, HDC, HWND, int );
void FASTCALL OutBasket_OnSetFocus( HWND, HWND );
void FASTCALL OutBasket_OnAdjustWindows( HWND, int, int );

BOOL WINAPI EXPORT OutBasketEvents( int, LPARAM, LPARAM );

LRESULT EXPORT CALLBACK OutBasketWndProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT FAR PASCAL OutBaskListProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT CALLBACK PrintOutbasketDlg( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL PrintOutbasketDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL PrintOutbasketDlg_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL PrintOutbasketDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL CmdPrintOutbasket( HWND );

/* This function creates the out-basket window.
 */
BOOL FASTCALL CreateOutBasket( HWND hwnd )
{
   DWORD dwState;
   RECT rc;

   /* If outbasket window already open, bring it to the front
    * and display it.
    */
   if( hwndOutBasket )
      {
      Adm_MakeMDIWindowActive( hwndOutBasket );
      return( TRUE );
      }

   /* Register the out-basket window class if we have
    * not already done so.
    */
   if( !fRegistered )
      {
      WNDCLASS wc;

      wc.style          = CS_HREDRAW | CS_VREDRAW;
      wc.lpfnWndProc    = OutBasketWndProc;
      wc.hIcon          = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_OUTBASKET) );
      wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
      wc.lpszMenuName   = NULL;
      wc.cbWndExtra     = MWE_EXTRA;
      wc.cbClsExtra     = 0;
      wc.hbrBackground  = NULL;
      wc.lpszClassName  = szOutBaskWndClass;
      wc.hInstance      = hInst;
      if( !RegisterClass( &wc ) )
         return( FALSE );
      fRegistered = TRUE;
      }

   /* The default position of the out-basket.
    */
   GetClientRect( hwndMDIClient, &rc );
   rc.left = rc.right / 3;
   InflateRect( &rc, -5, -5 );
   dwState = 0;

   /* Load the out-basket bitmaps.
    */
   hbmpOutBasket = LoadBitmap( hRscLib, MAKEINTRESOURCE(IDB_OUTBASKETBMPS) );

   /* Get the actual window size, position and state.
    */
   ReadProperWindowState( szOutBaskWndName, &rc, &dwState );

   if( hwndActive && IsMaximized( hwndActive ) )
      dwState = WS_MAXIMIZE;

         
   /* Create the window.
    */
   hwndOutBasket = Adm_CreateMDIWindow( szOutBaskWndName, szOutBaskWndClass, hInst, &rc, dwState, 0L );
   if( NULL == hwndOutBasket )
      return( FALSE );
   UpdateWindow( hwndOutBasket );

   /* Set the initial focus.
    */
   OutBasket_OnSetFocus( hwndOutBasket, NULL );
   UpdateOutBasketStatus();
   return( TRUE );

}

/* This function deletes the out-basket.
 */
void FASTCALL DestroyOutBasket( void )
{
   /* If the out-basket window is open, delete
    * the contents of the list box.
    */
   if( hwndOutBasket )
      {
      HWND hwndList;

      hwndList = GetDlgItem( hwndOutBasket, IDD_LIST );
      ListBox_ResetContent( hwndList );
      }

   /* Delete all out-basket items.
    */
   Amob_ClearMemory();
}

/* This function handles all messages for the out-basket window.
 */
LRESULT EXPORT CALLBACK OutBasketWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, OutBasket_OnCreate );
      HANDLE_MSG( hwnd, WM_CLOSE, OutBasket_OnClose );
      HANDLE_MSG( hwnd, WM_SIZE, OutBasket_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, OutBasket_OnMove );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, Common_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_COMMAND, OutBasket_OnCommand );
      HANDLE_MSG( hwnd, WM_DRAWITEM, OutBasket_OnDrawItem );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, OutBasket_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, OutBasket_MDIActivate );
      HANDLE_MSG( hwnd, WM_MENUSELECT, MainWnd_OnMenuSelect );
   #ifdef WIN32
      HANDLE_MSG( hwnd, WM_CTLCOLORLISTBOX, OutBasket_OnCtlColor );
   #else
      HANDLE_MSG( hwnd, WM_CTLCOLOR, OutBasket_OnCtlColor );
   #endif
      HANDLE_MSG( hwnd, WM_SETFOCUS, OutBasket_OnSetFocus );
      HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, OutBasket_OnAdjustWindows );

      case WM_CHANGEFONT:
         if( wParam == FONTYP_OUTBASKET || 0xFFFF == wParam )
            {
            MEASUREITEMSTRUCT mis;
            HWND hwndList;

            hwndList = GetDlgItem( hwnd, IDD_LIST );
            SendMessage( hwnd, WM_MEASUREITEM, IDD_LIST, (LPARAM)(LPSTR)&mis );
            SendMessage( hwndList, LB_SETITEMHEIGHT, 0, MAKELPARAM( mis.itemHeight, 0) );
            SetWindowFont( hwndList, hOutBasketFont, TRUE );
            }
         return( 0L );

      case WM_APPCOLOURCHANGE:
         if( wParam == WIN_OUTBASK )
            {
            HWND hwndList;

            if( NULL != hbrOutBaskWnd )
               DeleteBrush( hbrOutBaskWnd );
            hbrOutBaskWnd = CreateSolidBrush( crOutBaskWnd );
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
         lpMinMaxInfo->ptMinTrackSize.x = 175;
         lpMinMaxInfo->ptMinTrackSize.y = 192;
         return( lResult );
         }

      default:
         break;
      }
   return( Adm_DefMDIDlgProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_CTLCOLOR message.
 */
HBRUSH FASTCALL OutBasket_OnCtlColor( HWND hwnd, HDC hdc, HWND hwndChild, int type )
{
   switch( GetDlgCtrlID( hwndChild ) )
      {
      case IDD_LIST:
         SetBkColor( hdc, crOutBaskWnd );
         SetTextColor( hdc, crOutBaskText );
         return( hbrOutBaskWnd );
      }
#ifdef WIN32
   return( FORWARD_WM_CTLCOLORLISTBOX( hwnd, hdc, hwndChild, Adm_DefMDIDlgProc ) );
#else
   return( FORWARD_WM_CTLCOLOR( hwnd, hdc, hwndChild, type, Adm_DefMDIDlgProc ) );
#endif
}

/* This function handles the WM_SETFOCUS message.
 */
void FASTCALL OutBasket_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   HWND hwndList;
   int count;

   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   count = ListBox_GetCount( hwndList );
   if( count > 0 )
      SetFocus( hwndList );
   else
      {
      ChangeDefaultButton( hwnd, IDCANCEL );
      SetFocus( GetDlgItem( hwnd, IDCANCEL ) );
      }
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL OutBasket_OnCreate( HWND hwnd, LPCREATESTRUCT lpCreateStruct )
{
   LPMDICREATESTRUCT lpMDICreateStruct;
   HWND hwndList;
   LPOB lpob;

   /* Create the dialog window.
    */
   lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
   Adm_MDIDialog( hwnd, MAKEINTRESOURCE(IDDLG_OUTBASKET), lpMDICreateStruct );

   /* Now add the out-basket items to the out-basket window.
    */
   for( lpob = Amob_GetOb( NULL ); lpob; lpob = Amob_GetOb( lpob ) )
      AddStringToOutBasket( hwnd, lpob );

   /* Enable or disable blink button.
    */
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_BLINK, !fBlinking && !IsServerBusy() );

   /* Subclass the listbox so we can handle the right mouse
    * button.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   lpProcListCtl = SubclassWindow( hwndList, OutBaskListProc );

   /* Create the out-basket window handle.
    */
   hbrOutBaskWnd = CreateSolidBrush( crOutBaskWnd );

   /* Register to receive some events.
    */
   Amuser_RegisterEvent( AE_INSERTED_OUTBASKET_OBJECT, OutBasketEvents );
   Amuser_RegisterEvent( AE_DELETING_OUTBASKET_OBJECT, OutBasketEvents );
   Amuser_RegisterEvent( AE_EDITING_OUTBASKET_OBJECT, OutBasketEvents );
   Amuser_RegisterEvent( AE_EDITED_OUTBASKET_OBJECT, OutBasketEvents );

   /* If there are any items in the outbasket, select the
    * first item, otherwise disable those buttons that only
    * work on a selected item.
    */
   ShowOutBasketTotal( hwnd );
   return( TRUE );
}

/* This function handles newsgroup events.
 */
BOOL WINAPI EXPORT OutBasketEvents( int wEvent, LPARAM lParam1, LPARAM lParam2 )
{
   switch( wEvent )
      {
      case AE_DELETING_OUTBASKET_OBJECT:
         DeleteFromOutBasket( (LPOB)lParam1 );
         if( !fBlinking )
            OutBasket_OnSetFocus( hwndOutBasket, NULL );
         break;

      case AE_INSERTED_OUTBASKET_OBJECT:
         /* A new item has been added to the out-basket, so make it
          * appear in the listbox.
          */
         AddStringToOutBasket( hwndOutBasket, (LPOB)lParam1 );
         break;

      case AE_EDITED_OUTBASKET_OBJECT:
      case AE_EDITING_OUTBASKET_OBJECT:
         /* An out-basket object is being or has been edited, so simply
          * update its state.
          */
         UpdateOutbasketItem( (LPOB)lParam1, FALSE );
         UpdateOutBasketStatus();
         break;
      }
   return( TRUE );
}

/* This function intercepts messages for the out-basket list box window
 * so that we can handle the right mouse button.
 */
LRESULT EXPORT FAR PASCAL OutBaskListProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   switch( msg )
      {
      case WM_ERASEBKGND: {
         RECT rc;

         GetClientRect( hwnd, &rc );
         FillRect( (HDC)wParam, &rc, hbrOutBaskWnd );
         return( TRUE );
         }

      case WM_CONTEXTMENU: /*!!SM!!*/
      case WM_RBUTTONDOWN:
         {
         POINT pt;
         LPOB lpob;
         HWND hwndOpen;
         HMENU hPopupMenu;
         int index;
         AEPOPUP aep;

         /* Make sure this window is active and select the item
          * at the cursor.
          */
         if( hwndActive != GetParent( hwnd ) )
            SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndOutBasket, 0L );

         /* Select the item under the cursor only if nothing is
          * currently selected.
          */
         index = ItemFromPoint( hwnd, (short)LOWORD(lParam), (short)HIWORD(lParam) );
         if( ListBox_GetSel( hwnd, index ) == 0 )
            {
            CallWindowProc( lpProcListCtl, hwnd, WM_LBUTTONDOWN, wParam, lParam );
            CallWindowProc( lpProcListCtl, hwnd, WM_LBUTTONUP, wParam, lParam );
            }

         /* Get the out-basket menu.
          */
         hPopupMenu = GetSubMenu( hPopupMenus, IPOP_OBASKWINDOW );
         hwndOpen = GetDlgItem( GetParent( hwnd ), IDOK );
         MenuEnable( hPopupMenu, IDOK, IsWindowEnabled( hwndOpen ) );

         /* If we have any selected items, use the attributes
          * of the first item to determine the menu items.
          */
         if( ListBox_GetSelCount( hwnd ) > 0 )
            {
            OBINFO obinfo;

            ListBox_GetSelItems( hwnd, 1, &index );
            lpob = (LPOB)ListBox_GetItemData( hwnd, index );
            Amob_GetObInfo( lpob, &obinfo );
            if( TestF(obinfo.obHdr.wFlags, OBF_HOLD) )
               MenuString( hPopupMenu, IDD_HOLD, GS(IDS_STR275) );
            else
               MenuString( hPopupMenu, IDD_HOLD, GS(IDS_STR277) );
            if( TestF(obinfo.obHdr.wFlags, OBF_KEEP) )
               MenuString( hPopupMenu, IDD_KEEP, GS(IDS_STR440) );
            else
               MenuString( hPopupMenu, IDD_KEEP, GS(IDS_STR439) );
            MenuEnable( hPopupMenu, IDOK, !fInitiatingBlink && Amob_IsEditable( obinfo.obHdr.clsid ) && !TestF(obinfo.obHdr.wFlags, OBF_PENDING) && !TestF(obinfo.obHdr.wFlags, OBF_ACTIVE ) );
            MenuEnable( hPopupMenu, IDD_DELETE, !fInitiatingBlink && !TestF(obinfo.obHdr.wFlags, OBF_PENDING ) && !TestF(obinfo.obHdr.wFlags, OBF_ACTIVE ) );
            MenuEnable( hPopupMenu, IDD_KEEP, !fInitiatingBlink && !TestF(obinfo.obHdr.wFlags, OBF_PENDING ) && !TestF(obinfo.obHdr.wFlags, OBF_ACTIVE ) );
            MenuEnable( hPopupMenu, IDD_HOLD, !fInitiatingBlink && !TestF(obinfo.obHdr.wFlags, OBF_PENDING ) && !TestF(obinfo.obHdr.wFlags, OBF_ACTIVE ) );
            }
         else
            {
            MenuEnable( hPopupMenu, IDOK, FALSE );
            MenuEnable( hPopupMenu, IDD_KEEP, FALSE );
            MenuEnable( hPopupMenu, IDD_HOLD, FALSE );
            MenuEnable( hPopupMenu, IDD_DELETE, FALSE );
            }
            /* Call the AE_POPUP event.
             */
         aep.wType = WIN_OUTBASK;
         aep.pFolder = NULL;
         aep.pSelectedText = NULL;
         aep.cchSelectedText = 0;
         aep.hMenu = hPopupMenu;
         aep.nInsertPos = 2;
         Amuser_CallRegistered( AE_POPUP, (LPARAM)(LPSTR)&aep, 0L );

         GetCursorPos( &pt );
         TrackPopupMenu( hPopupMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndFrame, NULL );
         return( 0 );
         }

      case WM_KEYDOWN:
         /* Trap the Delete and Shift+Delete key in the outbasket
          * list box to handle the Delete command.
          */
         if( wParam == VK_DELETE ) 
            {
            PostDlgCommand( GetParent( hwnd ), IDD_DELETE, 0 );
            return( 0 );
            }

         /* Trap the Pause and Shift+Pause key in the outbasket
          * list box to handle the Hold command.
          */
         if( wParam == VK_PAUSE )
            {
            PostDlgCommand( GetParent( hwnd ), IDD_HOLD, 1 );
            return( 0 );
            }
         break;
      }
   return( CallWindowProc( lpProcListCtl, hwnd, msg, wParam, lParam ) );
}

/* This function handles the WM_MDIACTIVATE message.
 */
void FASTCALL OutBasket_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_OUTBASK ), (LPARAM)(LPSTR)hwnd );
   if( fActive )
      {
      ToolbarState_EditWindow();
      if( NULL != hwndOutBasket )
         OutBasket_OnSetFocus( hwndOutBasket, NULL );
      }
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, Adm_DefMDIDlgProc );
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL OutBasket_OnAdjustWindows( HWND hwnd, int dx, int dy )
{
   Adm_MoveWnd( GetDlgItem( hwnd, IDOK ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDCANCEL ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_HELP ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_KEEP ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_HOLD ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_DELETE ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_PAD1 ), dx, 0 );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_LIST ), dx, dy );
}

/* This function handles the WM_CLOSE message.
 */
void FASTCALL OutBasket_OnClose( HWND hwnd )
{
   /* Unregister to receive some events.
    */
   Amuser_UnregisterEvent( AE_INSERTED_OUTBASKET_OBJECT, OutBasketEvents );
   Amuser_UnregisterEvent( AE_DELETING_OUTBASKET_OBJECT, OutBasketEvents );
   Amuser_UnregisterEvent( AE_EDITING_OUTBASKET_OBJECT, OutBasketEvents );
   Amuser_UnregisterEvent( AE_EDITED_OUTBASKET_OBJECT, OutBasketEvents );

   /* Clean up
    */
   Adm_DestroyMDIWindow( hwnd );
   DeleteBitmap( hbmpOutBasket );
   DeleteBrush( hbrOutBaskWnd );
   hwndOutBasket = NULL;
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL OutBasket_OnMove( HWND hwnd, int x, int y )
{
   Amuser_WriteWindowState( szOutBaskWndName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL OutBasket_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   /* Save the new window state to the configuration file.
    */
   Amuser_WriteWindowState( szOutBaskWndName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* This function processes the WM_COMMAND message.
 */
void FASTCALL OutBasket_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDM_QUICKPRINT:
      case IDM_PRINT:
         CmdPrintOutbasket( hwnd );
         break;

      case IDD_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsOUTBASKET );
         break;

      case IDCANCEL:
         SendMessage( hwnd, WM_CLOSE, 0, 0L );
         break;

      case IDD_DELETE: {
         HWND hwndList;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         if( IsWindowEnabled( GetDlgItem( hwnd, id ) ) )
            if( fMessageBox( hwnd, 0, GS(IDS_STR253), MB_YESNO|MB_ICONINFORMATION ) == IDYES )
               {
               int cDeleted;
               int nCount;
               int nSel;
               int c;

               /* Set fIgnoreDeleteEvent because we could be
                * deleting lots of items, and we don't want each
                * deletion from the out-basket to be individual.
                */
               nCount = ListBox_GetCount( hwndList );
               SetWindowRedraw( hwndList, FALSE );
               cDeleted = c = 0;
               nSel = -1;
               fIgnoreDeleteEvent = TRUE;
               while( c < nCount )
                  {
                  if( ListBox_GetSel( hwndList, c ) > 0 )
                     {
                     OBINFO obinfo;
                     LPOB lpob;
         
                     lpob = (LPOB)ListBox_GetItemData( hwndList, c );
                     Amob_GetObInfo( lpob, &obinfo );
                     if( !( obinfo.obHdr.wFlags & OBF_ACTIVE ) && !( obinfo.obHdr.wFlags & OBF_PENDING ) )
                        {
                        ListBox_DeleteString( hwndList, c );
                        nSel = c;
                        Amob_RemoveObject( lpob, FALSE );
                        ++cDeleted;
                        --nCount;
                        }
                     else
                        ++c;
                     }
                  else
                      ++c;
                  }
               if( nSel >= nCount )
                  nSel = nCount - 1;
               SetWindowRedraw( hwndList, TRUE );
               if( cDeleted )
                  {
                  InvalidateRect( hwndList, NULL, TRUE );
                  UpdateWindow( hwndList );
                  if( nSel > -1 )
                     ListBox_SetSel( hwndList, TRUE, nSel );
                  Amob_SaveOutBasket( FALSE );
                  UpdateOutBasketStatus();
                  ShowOutBasketTotal( hwndOutBasket );
                  }
               fIgnoreDeleteEvent = FALSE;
               OutBasket_OnSetFocus( hwnd, NULL );
               }
         SetFocus( hwndList );
         break;
         }

      case IDD_HOLD: {
         HWND hwndList;
         int nCount;
         BOOL fFirst;
         BOOL fHold;
         int c;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         nCount = ListBox_GetCount( hwndList );
         fFirst = TRUE;                        
         fHold = FALSE;
         for( c = 0; c < nCount; ++c )
            if( ListBox_GetSel( hwndList, c ) )
               {
               OBINFO obinfo;
               LPOB lpob;
               RECT rc;

               lpob = (LPOB)ListBox_GetItemData( hwndList, c );
               Amob_GetObInfo( lpob, &obinfo );
               if( !( obinfo.obHdr.wFlags & OBF_ACTIVE ) && !( obinfo.obHdr.wFlags & OBF_PENDING ) )
                  {
                  if( fFirst )
                     {
                     if( obinfo.obHdr.wFlags & OBF_HOLD )
                        {
                        SetWindowText( GetDlgItem( hwnd, IDD_HOLD ), GS(IDS_STR281) );
                        fHold = FALSE;
                        }
                     else
                        {
                        SetWindowText( GetDlgItem( hwnd, IDD_HOLD ), GS(IDS_STR282) );
                        fHold = TRUE;
                        }
                     fFirst = FALSE;
                     }
                  obinfo.obHdr.wFlags &= ~OBF_ERROR;
                  if( fHold )
                     obinfo.obHdr.wFlags |= OBF_HOLD;
                  else
                     obinfo.obHdr.wFlags &= ~OBF_HOLD;
                  Amob_SetObInfo( lpob, &obinfo );
                  ListBox_GetItemRect( hwndList, c, &rc );
                  if( !IsRectEmpty( &rc ) )
                     InvalidateRect( hwndList, &rc, FALSE );
                  }
               }
         // !!SM!! 2.55.2035
         Amob_SaveOutBasket( FALSE );
         UpdateOutBasketStatus();
         ShowOutBasketTotal( hwndOutBasket );
         // !!SM!! 2.55.2035

         UpdateWindow( hwndList );
         SetFocus( hwndList );
         break;
         }

      case IDD_KEEP: {
         HWND hwndList;
         int nCount;
         BOOL fFirst;
         BOOL fKeep;
         int c;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         nCount = ListBox_GetCount( hwndList );
         fFirst = TRUE;
         fKeep = FALSE;
         for( c = 0; c < nCount; ++c )
            {
            if( ListBox_GetSel( hwndList, c ) )
               {
               OBINFO obinfo;
               LPOB lpob;
               RECT rc;

               lpob = (LPOB)ListBox_GetItemData( hwndList, c );
               Amob_GetObInfo( lpob, &obinfo );
               if( fFirst )
                  {
                  if( obinfo.obHdr.wFlags & OBF_KEEP )
                     {
                     fKeep = FALSE;
                     SetWindowText( GetDlgItem( hwnd, IDD_KEEP ), GS(IDS_STR319) );
                     }
                  else
                     {
                     fKeep = TRUE;
                     SetWindowText( GetDlgItem( hwnd, IDD_KEEP ), GS(IDS_STR318) );
                     }
                  fFirst = FALSE;
                  }
               obinfo.obHdr.wFlags &= ~OBF_ERROR;
               if( fKeep )
                  obinfo.obHdr.wFlags |= OBF_KEEP;
               else
                  obinfo.obHdr.wFlags &= ~OBF_KEEP;
               Amob_SetObInfo( lpob, &obinfo );
               ListBox_GetItemRect( hwndList, c, &rc );
               if( !IsRectEmpty( &rc ) )
                  InvalidateRect( hwndList, &rc, FALSE );
               }
            }
         // !!SM!! 2.55.2035
         Amob_SaveOutBasket( FALSE );
         UpdateOutBasketStatus();
         ShowOutBasketTotal( hwndOutBasket );
         // !!SM!! 2.55.2035

         UpdateWindow( hwndList );
         SetFocus( hwndList );
         break;
         }

      case IDD_LIST:
         if( codeNotify == LBN_SELCHANGE )
            {
            UpdateOutBasketStatus();
            break;
            }
         else if( codeNotify != LBN_DBLCLK )
            break;
         if( !IsWindowEnabled( GetDlgItem( hwnd, IDOK ) ) )
            break;

      case IDOK: {
         HWND hwndList;
         int nCount;
         int c;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         nCount = ListBox_GetCount( hwndList );
         for( c = 0; c < nCount; ++c )
            if( ListBox_GetSel( hwndList, c ) > 0 )
               {
               OBINFO obinfo;
               LPOB lpob;

               lpob = (LPOB)ListBox_GetItemData( hwndList, c );
               Amob_GetObInfo( lpob, &obinfo );
               if( Amob_IsEditable( obinfo.obHdr.clsid ) && !( obinfo.obHdr.wFlags & OBF_ACTIVE ) && !( obinfo.obHdr.wFlags & OBF_PENDING ) )
                  Amob_Edit( lpob );
               }
         break;
         }
      }
}

/* This function updates the out-basket status
 */
void FASTCALL UpdateOutBasketStatus( void )
{
   if( hwndOutBasket )
      {
      BOOL fIsEditable;
      BOOL fHasItem;
      BOOL fCanDelete;
      HWND hwndList;
      int nSelCount;

      hwndList = GetDlgItem( hwndOutBasket, IDD_LIST );
      fIsEditable = FALSE;
      fHasItem = ( ListBox_GetCount( hwndList ) > 0 );
      fCanDelete = FALSE;
      if( ( nSelCount = ListBox_GetSelCount( hwndList ) ) > 0 )
         {
         OBINFO obinfo;
         LPOB lpob;
         int nSel;

         ListBox_GetSelItems( hwndList, 1, &nSel );
         lpob = (LPOB)ListBox_GetItemData( hwndList, nSel );
         Amob_GetObInfo( lpob, &obinfo );
         SetWindowText( GetDlgItem( hwndOutBasket, IDD_HOLD ), ( obinfo.obHdr.wFlags & OBF_HOLD ) ? GS(IDS_STR282) : GS(IDS_STR281) );
         SetWindowText( GetDlgItem( hwndOutBasket, IDD_KEEP ), ( obinfo.obHdr.wFlags & OBF_KEEP ) ? GS(IDS_STR318) : GS(IDS_STR319) );
         fIsEditable = !fInitiatingBlink && Amob_IsEditable( obinfo.obHdr.clsid ) && !TestF(obinfo.obHdr.wFlags, OBF_PENDING ) && !TestF(obinfo.obHdr.wFlags, OBF_ACTIVE );
         fCanDelete = !fInitiatingBlink && !TestF(obinfo.obHdr.wFlags, OBF_PENDING ) && !TestF(obinfo.obHdr.wFlags, OBF_ACTIVE );
         }
      EnableControl( hwndOutBasket, IDOK, fIsEditable );
      EnableControl( hwndOutBasket, IDD_LIST, fHasItem );
      EnableControl( hwndOutBasket, IDD_DELETE, fCanDelete );
      EnableControl( hwndOutBasket, IDD_HOLD, fHasItem && fCanDelete );
      EnableControl( hwndOutBasket, IDD_KEEP, fHasItem && fCanDelete );
      }
}

/* This function handles the WM_MEASUREITEM message
 */
void FASTCALL OutBasket_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   HFONT hOldFont;
   BITMAP bmp;
   HDC hdc;

   hdc = GetDC( hwnd );
   hOldFont = SelectFont( hdc, hOutBasketFont );
   GetTextMetrics( hdc, &tm );
   GetObject( hbmpOutBasket, sizeof( BITMAP ), &bmp );
   lpMeasureItem->itemHeight = max( tm.tmHeight + tm.tmExternalLeading, bmp.bmHeight );
   SelectFont( hdc, hOldFont );
   ReleaseDC( hwnd, hdc );
}

/* This function draws one item in the out-basket list.
 */
void FASTCALL OutBasket_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   if( lpDrawItem->itemID != -1 )
      if( lpDrawItem->itemAction == ODA_FOCUS )
         DrawFocusRect( lpDrawItem->hDC, (LPRECT)&lpDrawItem->rcItem );
      else {
         HFONT hOldFont;
         HWND hwndList;
         OBINFO obinfo;
         TEXTMETRIC tm;
         COLORREF tmpT;
         COLORREF tmpB;
         COLORREF T;
         COLORREF B;
         HBRUSH hbr;
         LPOB lpob;
         RECT rc;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         lpob = (LPOB)ListBox_GetItemData( hwndList, lpDrawItem->itemID );
         Amob_GetObInfo( lpob, &obinfo );
         rc = lpDrawItem->rcItem;
         hOldFont = SelectFont( lpDrawItem->hDC, hOutBasketFont );
         if( lpDrawItem->itemState & ODS_SELECTED )
            {
            T = crHighlightText;
            B = crHighlight;
            }
         else
            {
            T = (obinfo.obHdr.wFlags & OBF_OPEN) ? crIgnoredTopic : crOutBaskText;
            B = crOutBaskWnd;
            }
         hbr = CreateSolidBrush( B );
         tmpT = SetTextColor( lpDrawItem->hDC, T );
         tmpB = SetBkColor( lpDrawItem->hDC, B );
         rc.left = 0;
         rc.right = 32;
         FillRect( lpDrawItem->hDC, &rc, hbr );
         if( obinfo.obHdr.wFlags & OBF_KEEP )
            Amuser_DrawBitmap( lpDrawItem->hDC, rc.left, rc.top, 16, 16, hbmpOutBasket, 1 );
         if( obinfo.obHdr.wFlags & OBF_HOLD )
            Amuser_DrawBitmap( lpDrawItem->hDC, rc.left + 16, rc.top, 16, 16, hbmpOutBasket, 0 );
         if( obinfo.obHdr.wFlags & OBF_ACTIVE )
            Amuser_DrawBitmap( lpDrawItem->hDC, rc.left + 16, rc.top, 16, 16, hbmpOutBasket, 2 );
         if( obinfo.obHdr.wFlags & OBF_SCRIPT )
            Amuser_DrawBitmap( lpDrawItem->hDC, rc.left + 16, rc.top, 16, 16, hbmpOutBasket, 4 );
         if( obinfo.obHdr.wFlags & OBF_ERROR )
            Amuser_DrawBitmap( lpDrawItem->hDC, rc.left + 16, rc.top, 16, 16, hbmpOutBasket, 3 );
         rc.left = rc.right;
         rc.right = lpDrawItem->rcItem.right;
         Amob_Describe( lpob, lpTmpBuf );
         GetTextMetrics( lpDrawItem->hDC, &tm );
         ExtTextOut( lpDrawItem->hDC, rc.left, rc.top, ETO_OPAQUE, &rc, lpTmpBuf, strlen( lpTmpBuf ), NULL );
         SelectFont( lpDrawItem->hDC, hOldFont );
         SetTextColor( lpDrawItem->hDC, tmpT );
         SetBkColor( lpDrawItem->hDC, tmpB );
         DeleteBrush( hbr );
         }
}

/* This function simply inserts the pointer to the out-basket object into the
 * list box for the out-basket then updates the total of out-basket items. It
 * does not check to ascertain whether the object already appears in the list,
 * so this function should only be called for new out-basket items.
 */
void FASTCALL AddStringToOutBasket( HWND hwnd, LPOB lpob )
{
   HWND hwndList;

   hwndList = GetDlgItem( hwnd, IDD_LIST );
   if( hwndList )
      {
      ListBox_InsertString( hwndList, -1, lpob );
      if( ListBox_GetSelCount( hwndList ) == 0 )
         ListBox_SetSel( hwndList, TRUE, 0 );
      ShowOutBasketTotal( hwnd );
      }
}

/* This function deletes an out-basket item, removes it from the list box
 * then updates the list box.
 *
 * It does not delete the actual contents from the out-basket data file.
 */
void FASTCALL RemoveObject( LPOB lpob )
{
   DeleteFromOutBasket( lpob );
   Amob_RemoveObject( lpob, TRUE );
   if( NULL != hwndOutBasket )
      ShowOutBasketTotal( hwndOutBasket );
}

/* This function removes an item from the outbasket.
 */
void FASTCALL DeleteFromOutBasket( LPOB lpob )
{
   /* Now closes down the window if there is one
    * YH 03/05/96
    */
   HWND hwndSay;

   if( hwndSay = Amob_GetEditWindow( lpob ) )
      SendDlgCommand( hwndSay, IDCANCEL, BN_CLICKED );
   if( NULL != hwndOutBasket && !fIgnoreDeleteEvent )
      {
      HWND hwndList;

      hwndList = GetDlgItem( hwndOutBasket, IDD_LIST );
      if( hwndList )
         {
         int wCount;
         int nSel;

         wCount = ListBox_GetCount( hwndList );
         for( nSel = 0; nSel < wCount; ++nSel )
            if( (LPOB)ListBox_GetItemData( hwndList, nSel ) == lpob )
               {
               if( ListBox_DeleteString( hwndList, nSel ) == nSel )
                  --nSel;
               if( ListBox_GetSelCount( hwndList ) == 0 )
                  ListBox_SetSel( hwndList, TRUE, nSel );
               break;
               }
         }
      UpdateOutBasketStatus();
      }
}

/* This function calculates the total number of items in the
 * out-basket and redisplays this total in the dialog box.
 */
void FASTCALL ShowOutBasketTotal( HWND hwnd )
{
   DWORD dwCount;
   char sz[ 40 ];

   /* Get count.
    */
   dwCount = Amob_GetCount( 0 );

   /* Show total in Out Basket dialog
    */
   wsprintf( sz, GS(IDS_STR257), dwCount );
   SetDlgItemText( hwnd, IDD_PAD1, sz );
   UpdateOutBasketStatus();

   /* Show total in window
    */
   if( 0 == dwCount )
      SetWindowText( hwnd, "Out Basket (Empty)" );
   else
      {
      wsprintf( sz, "Out Basket (%lu)", dwCount );
      SetWindowText( hwnd, sz );
      }
}

/* This function updates the specified item in the out-basket
 * window. If fStatus is TRUE, only the item status (ie. the three
 * bitmaps to the left of the description) is updated.
 */
void FASTCALL UpdateOutbasketItem( LPOB lpob, BOOL fStatus )
{
   if( NULL != hwndOutBasket )
      {
      HWND hwndList;
      int cTotal;
      int c;

      hwndList = GetDlgItem( hwndOutBasket, IDD_LIST );
      cTotal = ListBox_GetCount( hwndList );
      for( c = 0; c < cTotal; ++c )
         if( (LPOB)ListBox_GetItemData( hwndList, c ) == lpob )
            {
            RECT rc;

            ListBox_GetItemRect( hwndList, c, &rc );
            if( fStatus )
               rc.right = 48;
            InvalidateRect( hwndList, &rc, TRUE );
            UpdateWindow( hwndList );
            break;
            }
      }
}

/* Give a mouse coordinate, this function returns the index of
 * the item under the mouse cursor.
 */
int FASTCALL ItemFromPoint( HWND hwndList, short x, short y )
{
   int cVisibleItems;
   int nItemHeight;
   int nTopIndex;

   /* Get the height of each item in the list box. This assumes that
    * each item the listbox is the same height. It will fail if the listbox
    * has the LBS_OWNERDRAWVARIABLE style.
    */
   nItemHeight = ListBox_GetItemHeight( hwndList, 0 );
   if( 0 == nItemHeight )
      return( LB_ERR );

   /* We're now sure that the cursor is in the listbox, so compute the index
    * of the item under the cursor.
    */
   nTopIndex = ListBox_GetTopIndex( hwndList );
   cVisibleItems = ListBox_GetCount( hwndList ) - nTopIndex;
   return( min( y / nItemHeight, cVisibleItems - 1 ) + nTopIndex );
}

void FASTCALL CmdPrintOutbasket( HWND hwnd )
{
   HPRINT hPr;
   BOOL fOk = TRUE;
   LPSTR lpOBPrintBuf;

   /* Show and get the print terminal dialog box.
    */
   if( !Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_PRINTOUTBASKET), PrintOutbasketDlg, 0L ) )
      return;

   fNewMemory( &lpOBPrintBuf, LEN_TEMPBUF );
   GetWindowText( hwnd, lpOBPrintBuf, LEN_TEMPBUF );
   if(   NULL != ( hPr = BeginPrint( hwnd, lpOBPrintBuf, &lfFixedFont ) ) )
      {
      register int cCount;

      for( cCount = 0; fOk && cCount < cCopies; ++cCount )
         {

      int nCount;
      int c;
      HWND hwndList;

      ResetPageNumber( hPr );
      fOk = BeginPrintPage( hPr );

      hwndList = GetDlgItem( hwnd, IDD_LIST );

      nCount = ListBox_GetCount( hwndList );
      for( c = 0; c != nCount; c++ )
         {
         OBINFO obinfo;
         LPOB lpob;

         lpob = (LPOB)ListBox_GetItemData( hwndList, c );
         Amob_GetObInfo( lpob, &obinfo );
         Amob_Describe( lpob, lpOBPrintBuf );
         if( TestF(obinfo.obHdr.wFlags, OBF_HOLD) )
            wsprintf( lpOBPrintBuf, "%s (H)", lpOBPrintBuf );
         if( TestF(obinfo.obHdr.wFlags, OBF_KEEP) )
            wsprintf( lpOBPrintBuf, "%s (K)", lpOBPrintBuf );
         if( fOk )
            fOk = PrintLine( hPr, lpOBPrintBuf );
         }
      if( fOk )
         EndPrintPage( hPr );
      }
      }
   EndPrint( hPr );
   if( NULL != lpOBPrintBuf )
      FreeMemory( &lpOBPrintBuf );
}
         
/* This function handles the Print dialog.
 */
BOOL EXPORT CALLBACK PrintOutbasketDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = PrintOutbasketDlg_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the Print Message
 * dialog.
 */
LRESULT FASTCALL PrintOutbasketDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, PrintOutbasketDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, PrintOutbasketDlg_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPRINTOUTBASKET );
         break;
      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL PrintOutbasketDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
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
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL PrintOutbasketDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
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
         EndDialog( hwnd, TRUE );
         break;

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}
