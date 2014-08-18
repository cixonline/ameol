/* SPLITTER.C - Implements the message window splitter window functions
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

#include "warnings.h"
#include <windows.h>
#include <windowsx.h>
#include "amctrlrc.h"
#include "amctrls.h"
#include "amctrli.h"

#define  THIS_FILE   __FILE__

#define  FROM_MOUSE           0
#define  FROM_KEYBOARD        1

#define  SPLIT_CLICK          0xDEAD

static HCURSOR hVSplitCursor;
static HCURSOR hHSplitCursor;

int FASTCALL SplitBarDrag( HWND, WORD );
void FASTCALL DrawSplitBar( HDC, BOOL, int, int, int, int, int, int );

LRESULT EXPORT CALLBACK SplitterWndFn( HWND, UINT, WPARAM, LPARAM );

/* This function initializes the split box window class.
 */
BOOL FASTCALL RegisterSplitterClass( HINSTANCE hInst )
{
   WNDCLASS wc;

   wc.hCursor        = NULL;
   wc.hIcon          = NULL;
   wc.lpszMenuName   = NULL;
   wc.lpszClassName  = WC_SPLITTER;
   wc.hbrBackground  = (HBRUSH)( COLOR_BTNFACE+1 );
   wc.hInstance      = hInst;
   wc.style          = CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS;
   wc.lpfnWndProc    = SplitterWndFn;
   wc.cbClsExtra     = 0;
   wc.cbWndExtra     = 0;
   return( RegisterClass( &wc ) );
}

/* This function does the remainder of the split box window class
 * initialization.
 */
WINCOMMCTRLAPI HWND WINAPI EXPORT Amctl_CreateSplitterWindow( HWND hwndParent, WORD wID, BOOL fHorz )
{
   HWND hwnd;
   DWORD dwStyle;

   dwStyle = fHorz ? SPS_HORZ : SPS_VERT;
   hwnd = CreateWindow( WC_SPLITTER,
                        "",
                        WS_CHILD|WS_VISIBLE|dwStyle,
                        0, 0, 0, 0,
                        hwndParent,
                        (HMENU)wID,
                        GetWindowInstance( hwndParent ),
                        NULL );
   return( hwnd );
}

/* This function handles all messages for the split box window control.
 * It now uses Windows/NT message crackers for portability.
 */
LRESULT EXPORT CALLBACK SplitterWndFn( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   static BOOL fClicked = FALSE;
   static BOOL fDblClick = FALSE;
   static BOOL fDown = FALSE;
   static int nCalls = 0;

   switch( message )
      {
      case WM_CREATE:
         /* If this is the splitter bar control created, load the drag
          * cursor.
          */
         if( 0 == nCalls++ )
            {
            hVSplitCursor = LoadCursor( hLibInst, MAKEINTRESOURCE(IDC_VSPLITCUR) );
            hHSplitCursor = LoadCursor( hLibInst, MAKEINTRESOURCE(IDC_HSPLITCUR) );
            }
         break;

      case WM_DESTROY:
         if( 0 == --nCalls )
            {
            DestroyCursor( hVSplitCursor );
            DestroyCursor( hHSplitCursor );
            }
         break;

      case WM_PAINT: {
         RECT rc;
         PAINTSTRUCT ps;
         HPEN hpenOld;
         HPEN hpenWhite;
         HPEN hpenBlack;
         HPEN hpenLtGrey;
         HPEN hpenDkGrey;
         HBRUSH hbr;
         HDC hdc;

         /* Prepare for painting.
          */
         hdc = BeginPaint( hwnd, &ps );
         GetClientRect( hwnd, &rc );

         /* Create some pens that we're going
          * to be using.
          */
         hpenWhite = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNHIGHLIGHT ) );
         hpenBlack = CreatePen( PS_SOLID, 1, RGB( 0, 0, 0 ) );
         hpenDkGrey = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNSHADOW ) );
         hpenLtGrey = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNFACE ) );

         /* Draw the horizontal or vertical splitter.
          */
         if( GetWindowStyle( hwnd ) & SPS_HORZ )
            {
            hpenOld = SelectPen( hdc, hpenLtGrey );
            MoveToEx( hdc, 0, rc.top, NULL );
            LineTo( hdc, rc.right, rc.top );
            SelectPen( hdc, hpenWhite );
            MoveToEx( hdc, 0, rc.top + 1, NULL );
            LineTo( hdc, rc.right, rc.top + 1 );
            SelectPen( hdc, hpenDkGrey );
            MoveToEx( hdc, 0, rc.bottom - 1, NULL );
            LineTo( hdc, rc.right, rc.bottom - 1 );
            SelectPen( hdc, hpenBlack );
            MoveToEx( hdc, 0, rc.bottom - 2, NULL );
            LineTo( hdc, rc.right, rc.bottom - 2 );
            SelectPen( hdc, hpenOld );
            rc.top += 2;
            rc.bottom -= 1;
            }
         else
            {
            hpenOld = SelectPen( hdc, hpenLtGrey );
            MoveToEx( hdc, rc.left, 0, NULL );
            LineTo( hdc, rc.left, rc.bottom );
            SelectPen( hdc, hpenWhite );
            MoveToEx( hdc, rc.left + 1, 0, NULL );
            LineTo( hdc, rc.left + 1, rc.bottom );
            SelectPen( hdc, hpenDkGrey );
            MoveToEx( hdc, rc.right - 1, 0, NULL );
            LineTo( hdc, rc.right - 1, rc.bottom );
            SelectPen( hdc, hpenBlack );
            MoveToEx( hdc, rc.right - 2, 0, NULL );
            LineTo( hdc, rc.right - 2, rc.bottom );
            SelectPen( hdc, hpenOld );
            rc.left += 2;
            rc.right -= 1;
            }

         /* Fill the interior with the button face
          * colour.
          */
         hbr = CreateSolidBrush( GetSysColor( COLOR_BTNFACE ) );
         FillRect( hdc, &rc, hbr );
         DeleteBrush( hbr );

         /* Clean up at the end.
          */
         DeletePen( hpenWhite );
         DeletePen( hpenBlack );
         DeletePen( hpenDkGrey );
         DeletePen( hpenLtGrey );
         EndPaint( hwnd, &ps );
         break;
         }

      case SPM_ADJUST:
         SplitBarDrag( hwnd, FROM_MOUSE );
         break;

      case WM_MOUSEMOVE:
         if( !fDown )
            break;

      case WM_TIMER:
         KillTimer( hwnd, SPLIT_CLICK );
         if( !fClicked )
            SplitBarDrag( hwnd, FROM_MOUSE );
         fClicked = fDown = FALSE;
         break;

      case WM_LBUTTONUP:
         if( !fDblClick )
            fClicked = TRUE;
         fDown = FALSE;
         break;

      case WM_LBUTTONDOWN:
         if( IsWindowEnabled( hwnd ) )
            if( fClicked )
               {
               HWND hwndParent;
               int id;

               /* Kill the existing split timer.
                */
               KillTimer( hwnd, SPLIT_CLICK );
               fClicked = FALSE;

               /* Now send a message to the parent saying that we
                * double-clicked.
                */
               hwndParent = GetParent( hwnd );
               id = GetDlgCtrlID( hwnd );
            #ifdef WIN32
               SendMessage( hwndParent, WM_COMMAND, MAKELPARAM( id, (UINT)SPN_SPLITDBLCLK ), (LPARAM)hwnd );
            #else
               SendMessage( hwndParent, WM_COMMAND, id, MAKELPARAM( hwnd, (UINT)SPN_SPLITDBLCLK ) );
            #endif
               fDblClick = TRUE;
               }
            else {
               /* Otherwise begin a single-click
                */
               fDown = TRUE;
               SetTimer( hwnd, SPLIT_CLICK, GetDoubleClickTime(), NULL );
               fDblClick = FALSE;
               }
         break;

      case WM_SETCURSOR:
         if( !IsWindowEnabled( hwnd ) )
            break;
         if( GetWindowStyle( hwnd ) & SPS_HORZ )
            SetCursor( hHSplitCursor );
         else
            SetCursor( hVSplitCursor );
         return( FALSE );
      }
   return( DefWindowProc( hwnd, message, wParam, lParam ) );
}

/* This function begins an interaction with the user to drag the split bar
 * in the active edit window. It handles dragging with either the mouse or
 * the keyboard, and will handle both concurrently. wDragType is either
 * FROM_MOUSE if dragging was started by clicking on the split box, or
 * FROM_KEYBOARD if the Split command in the document control menu started
 * the operation.
 *
 * While dragging is in progress, the user holds down the mouse button and
 * clicks in the position at which the windows are to be split. The up and
 * down cursor keys may similarly be used. Holding down the CTRL key while
 * using the cursor keys moves the split bar in single pixel increments.
 * The RETURN key splits the window at the split bar position, while ESC
 * aborts the operation.
 */
int FASTCALL SplitBarDrag( HWND hwnd, WORD wDragType )
{
   BOOL fTracking;
   BOOL fAbort;
   RECT rc;
   HWND hwndParent;
   int nPlayWidth;
   int nPlayHeight;
   int nWidth;
   int nHeight;
   int nSplit;
   int nSplitXCoord;
   int nSplitYCoord;
   BOOL fHorz;
   POINT pt;
   POINT ptTemp;
   RECT rcClip;
   RECT rcSplit;
   HDC hdc;

   /* Get the splitter bar dimensions
    */
   GetClientRect( hwnd, &rc );
   nWidth = rc.right - rc.left;
   nHeight = rc.bottom - rc.top;
   fHorz = ( GetWindowStyle( hwnd ) & SPS_HORZ ) == SPS_HORZ;

   /* Get the parent window handle, and compute its dimensions.
    */
   hwndParent = GetParent( hwnd );
   GetClientRect( hwndParent, &rc );
   nPlayWidth = rc.right - rc.left;
   nPlayHeight = rc.bottom - rc.top;
   pt.x = rc.left;
   pt.y = rc.top;
   ClientToScreen( hwndParent, &pt );

   /* Get the current vertical co-ordinate of the split bar, relative to
    * the parent window.
    */
   GetWindowRect( hwnd, &rcSplit );
   ScreenToClient( hwndParent, (LPPOINT)&rcSplit );
   nSplitYCoord = rcSplit.top;
   nSplitXCoord = rcSplit.left;

   if( wDragType == FROM_MOUSE )
      {
      GetCursorPos( &ptTemp );
      if( fHorz )
         {
         pt.x += nSplitXCoord + 1;
         pt.y += nSplitYCoord + 1;
         }
      else
         {
         pt.x += nSplitXCoord + 1;
         pt.y += ptTemp.y;
         }
      }
   else
      {
      if( fHorz )
         {
         pt.x += nPlayWidth / 2;
         pt.y += nSplitYCoord + 2;
         }
      else
         {
         pt.x += nSplitXCoord + 2;
         pt.y += nPlayHeight / 2;
         }
      SetCursorPos( pt.x, pt.y );
      SetCursor( hVSplitCursor );
      }

   hdc = GetDC( hwndParent );
   SetCapture( hwndParent );
   SetRect( &rcClip, 0, 0, nPlayWidth, nPlayHeight );
   ClientToScreen( hwndParent, (LPPOINT)&rcClip );
   ClientToScreen( hwndParent, ((LPPOINT)&rcClip) + 1 );
   ClipCursor( &rcClip );

   fTracking = TRUE;
   fAbort = FALSE;
   ScreenToClient( hwndParent, &pt );

   DrawSplitBar( hdc, fHorz, pt.x, pt.y, nWidth, nHeight, nPlayWidth, nPlayHeight );
   while( fTracking )
      {
      MSG msg;

      while( !PeekMessage( &msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE ) &&
             !PeekMessage( &msg, NULL, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE ) )
         WaitMessage();
      switch( msg.message )
         {
         case WM_KEYDOWN: {
            int nSteps;

            nSteps = ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) ? 1: 8;
            if( fHorz && msg.wParam == VK_UP || msg.wParam == VK_DOWN )
               {
               DrawSplitBar( hdc, fHorz, pt.x, pt.y, nWidth, nHeight, nPlayWidth, nPlayHeight );
               switch( msg.wParam )
                  {
                  case VK_UP:
                     if( pt.y > 0 )
                        pt.y -= nSteps;
                     break;
   
                  case VK_DOWN:
                     if( pt.y < nPlayHeight )
                        pt.y += nSteps;
                     break;
                  }
               ClientToScreen( hwndParent, &pt );
               SetCursorPos( pt.x, pt.y );
               ScreenToClient( hwndParent, &pt );
               DrawSplitBar( hdc, fHorz, pt.x, pt.y, nWidth, nHeight, nPlayWidth, nPlayHeight );
               }
            else if( !fHorz && msg.wParam == VK_LEFT || msg.wParam == VK_RIGHT )
               {
               DrawSplitBar( hdc, fHorz, pt.x, pt.y, nWidth, nHeight, nPlayWidth, nPlayHeight );
               switch( msg.wParam )
                  {
                  case VK_LEFT:
                     if( pt.x > 0 )
                        pt.x -= nSteps;
                     break;
   
                  case VK_RIGHT:
                     if( pt.x < nPlayWidth )
                        pt.x += nSteps;
                     break;
                  }
               ClientToScreen( hwndParent, &pt );
               SetCursorPos( pt.x, pt.y );
               ScreenToClient( hwndParent, &pt );
               DrawSplitBar( hdc, fHorz, pt.x, pt.y, nWidth, nHeight, nPlayWidth, nPlayHeight );
               }
            else if( msg.wParam == VK_ESCAPE )
               {
               fTracking = FALSE;
               fAbort = TRUE;
               }
            else if( msg.wParam == VK_RETURN )
               fTracking = FALSE;
            break;
            }

         case WM_MOUSEMOVE:
            if( fHorz )
               {
               if( (int)HIWORD( msg.lParam ) + 2 < nPlayHeight )
                  {
                  DrawSplitBar( hdc, fHorz, pt.x, pt.y, nWidth, nHeight, nPlayWidth, nPlayHeight );
                  pt.y = HIWORD( msg.lParam );
                  DrawSplitBar( hdc, fHorz, pt.x, pt.y, nWidth, nHeight, nPlayWidth, nPlayHeight );
                  }
               }
            else
               {
               if( (int)LOWORD( msg.lParam ) + 2 < nPlayWidth )
                  {
                  DrawSplitBar( hdc, fHorz, pt.x, pt.y, nWidth, nHeight, nPlayWidth, nPlayHeight );
                  pt.x = LOWORD( msg.lParam );
                  DrawSplitBar( hdc, fHorz, pt.x, pt.y, nWidth, nHeight, nPlayWidth, nPlayHeight );
                  }
               }
            break;

         case WM_LBUTTONUP:
            if( fHorz )
               pt.y = HIWORD( msg.lParam );
            else
               pt.x = LOWORD( msg.lParam );
            fTracking = FALSE;
            break;
         }
      }
   DrawSplitBar( hdc, fHorz, pt.x, pt.y, nWidth, nHeight, nPlayWidth, nPlayHeight );

   ClipCursor( NULL );
   ReleaseCapture();
   ReleaseDC( hwndParent, hdc );
   if( !fAbort ) {
      RECT rc;
      int id;

      GetClientRect( hwndParent, &rc );
      id = GetDlgCtrlID( hwnd );
      if( fHorz )
         nSplit = (int)( ( (float)pt.y / nPlayHeight ) * 100 );
      else
         nSplit = (int)( ( (float)pt.x / nPlayWidth ) * 100 );
#ifdef WIN32
      SendMessage( hwndParent, WM_COMMAND, MAKELPARAM( id, (UINT)SPN_SPLITSET ), (LPARAM)nSplit );
#else
      SendMessage( hwndParent, WM_COMMAND, id, MAKELPARAM( nSplit, (UINT)SPN_SPLITSET ) );
#endif
      }
   return( FALSE );
}

/* This function draws the horizontal split bar assuming that it is visible
 * on the screen.
 */
void FASTCALL DrawSplitBar( HDC hdc, BOOL fHorz, int x, int y, int nWidth, int nHeight, int nPlayWidth, int nPlayHeight )
{
   if( fHorz )
      {
      if( y + 2 < nPlayHeight )
         PatBlt( hdc, x, y, nWidth + 1, 2, DSTINVERT );
      }
   else
      {
      if( x + 2 < nPlayWidth )
         PatBlt( hdc, x, 0, 2, nHeight + 1, DSTINVERT );
      }
}
