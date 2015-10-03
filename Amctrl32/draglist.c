/* DRAGLIST.C - Implements the drag list control
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

#include "warnings.h"
#include <windows.h>
#include <windowsx.h>
#include "amctrls.h"
#include "amctrli.h"
#include <memory.h>
#include "amctrlrc.h"

#define  THIS_FILE   __FILE__

#define  cMaxSubclassed    10

#define  ID_DRAGTIMER      1234

/* One element of the subclassed list window array.
 */
typedef struct tagSUBCLIST {
   HWND hwndList;                   /* Handle of list box window */
   WNDPROC lpfOldWndProc;           /* Old list box window procedure */
} SUBCLIST;

static int cSubclassed = 0;         /* The number of consecutive subclassed listboxes */
static BOOL fDragging = FALSE;      /* TRUE if a drag is in progress */
static BOOL fDoneDrag = FALSE;      /* TRUE if a drag notification has just been processed */
static UINT uRegMsg;                /* The ID of the registered draglist window message */
static DWORD dwLastScrollTime= 0;   /* When we last scrolled the list box with LBItemFromPt */
static RECT rcInsertIcon;           /* Previous insert icon rectangle */

static SUBCLIST rgSubclassed[ cMaxSubclassed ];

static HCURSOR hStopCursor;         /* The handle of the stop cursor */
static HCURSOR hCopyCursor;         /* The handle of the copy cursor */
static HCURSOR hMoveCursor;         /* The handle of the move cursor */

LRESULT EXPORT CALLBACK DragListWndProc( HWND, UINT, WPARAM, LPARAM );

/* This function converts an ordinary, single selection, listbox into
 * a drag-list box. It does this by subclassing the listbox window procedure
 * so that all mouse activity is redirected to the drag-list window procedure.
 *
 * The following example subclasses a list box in a dialog box initialisation
 * procedure and registers the window message |
 *
 *    case WM_INITDIALOG:
 *       HWND hwndList;
 *
 *       ...
 *       hwndList = GetDlgItem( hwnd, IDD_LIST );
 *       MakeDragList( hwndList );
 *       uMsg = RegisterWindowMessage( DRAGLISTMSGSTRING );
 *       ...
 */
BOOL WINAPI EXPORT Amctl_MakeDragList( HWND hwndList )
{
   ASSERT( cSubclassed <= cMaxSubclassed );
   if( cSubclassed == cMaxSubclassed )
      return( FALSE );

   /* Create an entry in the array to hold the window handle and the old window procedure
    * of the drag-list window.
    */
   rgSubclassed[ cSubclassed ].hwndList = hwndList;
   rgSubclassed[ cSubclassed ].lpfOldWndProc = SubclassWindow( hwndList, DragListWndProc );
   ASSERT( rgSubclassed[ cSubclassed ].lpfOldWndProc != NULL );

   /* If this is the first drag-list box in existence, load the stock cursors
    * used during the drag phase.
    */
   if( 0 == cSubclassed++ )
      {
      hStopCursor = LoadCursor( hLibInst, MAKEINTRESOURCE( IDC_STOPCURSOR ) );
      hCopyCursor = LoadCursor( hLibInst, MAKEINTRESOURCE( IDC_COPYCURSOR ) );
      hMoveCursor = LoadCursor( hLibInst, MAKEINTRESOURCE( IDC_MOVECURSOR ) );
      }

   /* Register the window message that will be used to communicate with the
    * parent window of the drag-list window.
    */
   uRegMsg = RegisterWindowMessage( DRAGLISTMSGSTRING );
   return( TRUE );
}

/* The LBItemFromPt function determines the index of the listbox item at
 * the specified mouse (screen) co-ordinates. If the co-ordinates are above or below
 * the listbox and fAutoScroll is TRUE, then the function scrolls the list box up or
 * down one line and returns -1. Otherwise it simply returns -1.
 *
 * The following example calls LBItemFromPt in response to a DL_DRAGGING
 * notification message to determine whether or not the cursor is in the list box.
 * Note the use of SetWindowLong to return the result via DWL_MSGRESULT. |
 *
 *       case DL_DRAGGING: {
 *          int iItem;
 *
 *          iItem = LBItemFromPt( lpdli->hWnd, lpdli->ptCursor, TRUE );
 *          if( iItem != -1 )
 *             SetWindowLong( hwnd, DWL_MSGRESULT, DL_MOVECURSOR );
 *          else
 *             SetWindowLong( hwnd, DWL_MSGRESULT, DL_STOPCURSOR );
 *          return( 0 );
 *          }
 */
int WINAPI EXPORT Amctl_LBItemFromPt( HWND hwndList, POINT pt, BOOL fAutoScroll )
{
   DWORD dwTicksSinceLastScroll;
   int nItemHeight;
   int nTopIndex;
   int cVisibleItems;
   RECT rc;

   /* Get the listbox client coordinates and convert the mouse
    * coordinates to coordinates relative to the listbox top left corner.
    */
   GetClientRect( hwndList, &rc );
   ScreenToClient( hwndList, &pt );

   /* The dwTicksSinceLastScroll count is used to ignore fAutoScroll
    * if the last time we scrolled the list box by the last call to this
    * function is less than 50ms. This stops the list from scrolling too
    * fast to be usable, especially on fast machines.
    */
   dwTicksSinceLastScroll = GetTickCount() - dwLastScrollTime;

   /* If the cursor is over the top of the listbox, scroll the list box
    * down if fAutoScroll is TRUE and we're not within 50ms of the last
    * scroll.
    */
   if( pt.y < 0 )
      {
      if( !fAutoScroll || dwTicksSinceLastScroll < 50 )
         return( LB_ERR );
      SendMessage( hwndList, WM_VSCROLL, SB_LINEUP, 0L );
      dwLastScrollTime = GetTickCount();
      return( LB_ERR );
      }

   /* If the cursor is past the bottom of the listbox, scroll the list box
    * up if fAutoScroll is TRUE and we're not within 50ms of the last
    * scroll.
    */
   if( pt.y > rc.bottom )
      {
      if( !fAutoScroll || dwTicksSinceLastScroll < 50 )
         return( LB_ERR );
      SendMessage( hwndList, WM_VSCROLL, SB_LINEDOWN, 0L );
      dwLastScrollTime = GetTickCount();
      return( LB_ERR );
      }

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
   return( min( pt.y / nItemHeight, cVisibleItems - 1 ) + nTopIndex );
}

/* The DrawInsert function draws an insertion icon next to the drag list window
 * in the parent dialog. The icon appears pointing to the gap between the current
 * insertion point of the drag operation.
 *
 * The following example shows how to call DrawInsert in response to a DL_DRAGGING
 * notification message |
 *
 *       case DL_DRAGGING: {
 *          int iItem;
 *
 *          iItem = LBItemFromPt( lpdli->hWnd, lpdli->ptCursor, TRUE );
 *          DrawInsert( hwnd, lpdli->hWnd, -1 );
 *          ...
 */
void WINAPI EXPORT Amctl_DrawInsert( HWND hwndParent, HWND hwndList, int nItem )
{
   RECT rc;

   if( nItem != -1 )
      {
      ListBox_GetItemRect( hwndList, nItem, &rc );
      if( !IsRectEmpty( &rc ) )
         {
         HICON hInsertIcon;
         static RECT rcNew;
         HDC hdcParent;

         /* Convert the list box item coordinates to be offset from
          * the top left corner of the parent window.
          */
         ClientToScreen( hwndList, (LPPOINT)&rc );
         ClientToScreen( hwndList, ((LPPOINT)&rc)+1 );
         ScreenToClient( hwndParent, (LPPOINT)&rc );
         ScreenToClient( hwndParent, ((LPPOINT)&rc)+1 );

         /* Compute the dimensions of the insertion icon. If it differs from the
          * dimensions of the previous insertion icon, then blank the previous
          * icon and redraw it in the new position.
          */
         SetRect( &rcNew, rc.left - 34, rc.top - 16, rc.left - 2, rc.top + 16 );
         if( memcmp( &rcInsertIcon, &rcNew, sizeof( RECT ) ) != 0 )
            {
            if( !IsRectEmpty( &rcInsertIcon ) )
               {
               InvalidateRect( hwndParent, &rcInsertIcon, TRUE );
               UpdateWindow( hwndParent );
               }
            hInsertIcon = LoadIcon( hLibInst, MAKEINTRESOURCE(IDI_INSERTICON) );
            hdcParent = GetDC( hwndParent );
            DrawIcon( hdcParent, rcNew.left, rcNew.top, hInsertIcon );
            ReleaseDC( hwndParent, hdcParent );
            DestroyIcon( hInsertIcon );
            rcInsertIcon = rcNew;
            }
         }
      }
   else {
      /* If nItem is -1, just remove the current insertion icon if
       * there is one.
       */
      if( !IsRectEmpty( &rcInsertIcon ) )
         {
         InvalidateRect( hwndParent, &rcInsertIcon, TRUE );
         UpdateWindow( hwndParent );
         SetRectEmpty( &rcInsertIcon );
         }
      }
}

/* This is the drag list window procedure. This procedure is called for every window
 * message sent to the drag-list window, and gives us an opportunity to intercept all mouse
 * messages to handle the dragging phase. We also intercept the WM_DESTROY message to allow
 * ourselves to clean up just before the drag-list box is destroyed.
 */
LRESULT EXPORT CALLBACK DragListWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   WNDPROC lpfOldWndProc;
   register int c;

   /* Locate the old list box window procedure for this window by
    * searching the array. Since the window handle MUST be in the
    * array for this function to even be called, something would have
    * to be seriously wrong for the search to exit without finding
    * anything.
    */
   lpfOldWndProc = NULL;
   for( c = 0; c < cSubclassed; ++c )
      if( rgSubclassed[ c ].hwndList == hwnd )
         {
         lpfOldWndProc = rgSubclassed[ c ].lpfOldWndProc;
         break;
         }
   ASSERT( lpfOldWndProc != NULL );

   switch( message )
      {
      case WM_LBUTTONDOWN: {
         DRAGLISTINFO dli;

         CallWindowProc( lpfOldWndProc, hwnd, WM_LBUTTONDOWN, wParam, lParam );
         CallWindowProc( lpfOldWndProc, hwnd, WM_LBUTTONUP, wParam, lParam );

         dli.uNotification = DL_BEGINDRAG;
         dli.hWnd = hwnd;
         dli.ptCursor.x = LOWORD( lParam );
         dli.ptCursor.y = HIWORD( lParam );
         ClientToScreen( hwnd, &dli.ptCursor );
         if( SendMessage( GetParent( hwnd ), uRegMsg, GetDlgCtrlID( hwnd ), (LPARAM)(LPSTR)&dli ) )
            {
            /* Start a timer running to feed the parent window DL_DRAGGING messages
             * even when the mouse isn't moving.
             */
            SetTimer( hwnd, ID_DRAGTIMER, 10, NULL );
            SetCapture( hwnd );
            SetRectEmpty( &rcInsertIcon );
            fDragging = TRUE;
            fDoneDrag = FALSE;
            return( 0L );
            }
         break;
         }

      case WM_KEYDOWN:
         if( wParam != VK_ESCAPE )
            break;

      case WM_CONTEXTMENU: /*!!SM!!*/
      case WM_RBUTTONDOWN:
         if( fDragging )
            {
            DRAGLISTINFO dli;

            /* The user pressed the right mouse button in mid-drag or the
             * ESCAPE key. Release the capture, kill the timer and cancel the
             * drag by sending a cancel notification message to the parent.
             */
            ReleaseCapture();
            KillTimer( hwnd, ID_DRAGTIMER );
            dli.uNotification = DL_CANCELDRAG;
            dli.hWnd = hwnd;
            dli.ptCursor.x = LOWORD( lParam );
            dli.ptCursor.y = HIWORD( lParam );
            ClientToScreen( hwnd, &dli.ptCursor );
            SendMessage( GetParent( hwnd ), uRegMsg, GetDlgCtrlID( hwnd ), (LPARAM)(LPSTR)&dli );
            fDragging = FALSE;
            return( 0L );
            }
         break;

      case WM_LBUTTONUP:
         if( fDragging )
            {
            DRAGLISTINFO dli;

            /* The user released the mouse button, so release the capture on the
             * window, kill the timer and report back to the parent window with a
             * DL_DROPPED notification.
             */
            ReleaseCapture();
            KillTimer( hwnd, ID_DRAGTIMER );
            dli.uNotification = DL_DROPPED;
            dli.hWnd = hwnd;
            dli.ptCursor.x = LOWORD( lParam );
            dli.ptCursor.y = HIWORD( lParam );
            ClientToScreen( hwnd, &dli.ptCursor );
            SendMessage( GetParent( hwnd ), uRegMsg, GetDlgCtrlID( hwnd ), (LPARAM)(LPSTR)&dli );
            fDragging = FALSE;
            return( 0L );
            }
         break;

      case WM_MOUSEMOVE:
         if( fDragging )
            {
            DRAGLISTINFO dli;
            HCURSOR hCursor;
            WORD wCursor;
   
            /* The user moved the mouse button (or a timer event forced a simulated mouse move).
             * Notify the parent window with the DL_DRAGGING notification message and use the
             * return value to determine which of the cursors to show.
             */
            dli.uNotification = DL_DRAGGING;
            dli.hWnd = hwnd;
            dli.ptCursor.x = LOWORD( lParam );
            dli.ptCursor.y = HIWORD( lParam );
            ClientToScreen( hwnd, &dli.ptCursor );
            wCursor = (WORD)SendMessage( GetParent( hwnd ), uRegMsg, GetDlgCtrlID( hwnd ), (LPARAM)(LPSTR)&dli );
            switch( wCursor )
               {
               default:
                  hCursor = NULL;
                  break;

               case DL_COPYCURSOR:
                  hCursor = hCopyCursor;
                  break;

               case DL_MOVECURSOR:
                  hCursor = hMoveCursor;
                  break;

               case DL_STOPCURSOR:
                  hCursor = hStopCursor;
                  break;
               }
            SetCursor( hCursor );
            fDoneDrag = TRUE;
            return( 0L );
            }
         break;

      case WM_TIMER: {
         POINT pt;

         /* The timer procedure feeds the parent window DL_DRAGGING
          * messages even when the mouse isn't moving. This keeps the list
          * scrolling up or down when the mouse is outside the list box.
          */
         GetCursorPos( &pt );
         ScreenToClient( hwnd, &pt );
         SendMessage( hwnd, WM_MOUSEMOVE, 0, MAKELPARAM( pt.x, pt.y ) );
         return( 0L );
         }

      case WM_DESTROY:
         /* The drag-list window is being destroyed, so remove the
          * window from the array and restore the old window procedure
          * so that any future messages are no longer received by our
          * drag list window procedure.
          */
         --cSubclassed;
         for( ; c < cSubclassed; ++c )
            rgSubclassed[ c ] = rgSubclassed[ c + 1 ];
         SubclassWindow( hwnd, lpfOldWndProc );

         /* Just in case...
          */
         if( fDragging )
            {
            ReleaseCapture();
            KillTimer( hwnd, ID_DRAGTIMER );
            fDragging = FALSE;
            }
         SetRectEmpty( &rcInsertIcon );

         /* All consecutive drag-list boxes have been destroyed, so free
          * up the cursor resources.
          */
         if( 0 == cSubclassed )
            {
            DestroyCursor( hStopCursor );
            DestroyCursor( hCopyCursor );
            DestroyCursor( hMoveCursor );
            }
         break;
      }
   return( CallWindowProc( lpfOldWndProc, hwnd, message, wParam, lParam ) );
}
