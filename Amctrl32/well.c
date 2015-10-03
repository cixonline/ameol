/* WELL.C - Implements the well control
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
#include "string.h"
#include "amctrls.h"
#include "amctrli.h"
#include "kludge.h"
#include <math.h>

#define  THIS_FILE   __FILE__

#define  GWW_EXTRA            4
#define  GetBlock(h)          ((WELL FAR *)GetWindowLong((h),0))
#define  PutBlock(h,b)        (SetWindowLong((h),0,(LONG)(b)))

typedef struct tagWELL {
   COLORREF FAR * pColourArray;     /* Array of colours */
   int cColours;                    /* Number of colours in array */
   int iSel;                        /* Index of selected item */
   int cx;                          /* Width of well in items */
   int cy;                          /* Height of well in items */
} WELL;

COLORREF aDefColours[] = {
   RGB( 255,0,0 ),         // Red
   RGB( 0,255,255 ),       // Cyan
   RGB( 255,0,255 ),       // Purple
   RGB( 255,255,0 ),       // Yellow
   RGB( 0,255,0 ),         // Green
   RGB( 0,128,128 ),       // Dark cyan
   RGB( 128,0,0 ),         // Dark red
   RGB( 0,128,0 ),         // Dark green
   RGB( 0,0,255 ),         // Blue
   RGB( 128,0,128 ),       // Dark purple
   RGB( 0,0,128 ),         // Dark blue
   RGB( 0,0,0 ),           // Black
   RGB( 128,128,0 ),       // Brown
   RGB( 128,128,128 ),     // Grey
   RGB( 192,192,192 ),     // Light grey
   RGB( 255,255,255 )      // White
   };
#define  cDefColours    (sizeof(aDefColours)/sizeof(aDefColours[0]))

void FASTCALL SetColourArray( WELL FAR *, int, COLORREF FAR * );
void FASTCALL InvalidateItem( HWND, WELL FAR *, int );

LRESULT EXPORT CALLBACK WellWndFn( HWND, UINT, WPARAM, LPARAM );

/* This function registers the well window class.
 */
BOOL FASTCALL RegisterWellClass( HINSTANCE hInst )
{
   WNDCLASS wc;

   wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
   wc.hIcon          = NULL;
   wc.lpszMenuName   = NULL;
   wc.lpszClassName  = WC_WELL;
   wc.hbrBackground  = (HBRUSH)( COLOR_WINDOW+1 );
   wc.hInstance      = hInst;
   wc.style          = CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS | CS_DBLCLKS;
   wc.lpfnWndProc    = WellWndFn;
   wc.cbClsExtra     = 0;
   wc.cbWndExtra     = GWW_EXTRA;
   return( RegisterClass( &wc ) );
}

LRESULT EXPORT CALLBACK WellWndFn( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   WELL FAR * pWell;

#ifdef _DEBUG
   /* Help us to catch NULL pointers...
    */
   pWell = NULL;
#endif
   switch( message )
      {
      case WM_CREATE: {
         static WELL FAR * pNewWell;

         INITIALISE_PTR(pNewWell);
         if( fNewMemory( &pNewWell, sizeof( WELL ) ) )
            {
            pNewWell->cx = 4;
            pNewWell->cy = 4;
            pNewWell->iSel = -1;
            pNewWell->pColourArray = NULL;
            SetColourArray( pNewWell, cDefColours, aDefColours );
            PutBlock( hwnd, pNewWell );
            }
         return( pNewWell ? 0 : -1 );
         }

      case WCM_SETITEMCOUNT: {
         DWORD dwOldCount;

         pWell = GetBlock( hwnd );
         dwOldCount = MAKELPARAM( pWell->cx, pWell->cy );
         pWell->cx = LOWORD( lParam );
         pWell->cy = HIWORD( lParam );
         return( dwOldCount );
         }

      case WCM_GETITEMCOUNT:
         pWell = GetBlock( hwnd );
         return( MAKELPARAM( pWell->cx, pWell->cy ) );

      case WCM_SETCOLOURARRAY:
         pWell = GetBlock( hwnd );
         SetColourArray( pWell, (int)wParam, (COLORREF FAR *)lParam );
         InvalidateRect( hwnd, NULL, FALSE );
         return( TRUE );

      case WCM_SETSELECTED: {
         int iOldItem;

         pWell = GetBlock( hwnd );
         iOldItem = pWell->iSel;
         if( pWell->iSel != -1 )
            InvalidateItem( hwnd, pWell, pWell->iSel );
         pWell->iSel = wParam;
         InvalidateItem( hwnd, pWell, pWell->iSel );
         UpdateWindow( hwnd );
         return( iOldItem );
         }

      case WCM_SETSELECTEDCOLOUR: {
         register int c;

         /* Find the colour in the colour array.
          */
         pWell = GetBlock( hwnd );
         if( pWell->iSel != -1 )
            {
            pWell->iSel = -1;
            InvalidateItem( hwnd, pWell, pWell->iSel );
            UpdateWindow( hwnd );
            }
         for( c = 0; c < pWell->cColours; ++c )
            if( (COLORREF)lParam == pWell->pColourArray[ c ] )
               {
               pWell->iSel = c;
               InvalidateItem( hwnd, pWell, pWell->iSel );
               UpdateWindow( hwnd );
               return( TRUE );
               }
         return( FALSE );
         }

      case WCM_GETSELECTED:
         pWell = GetBlock( hwnd );
         return( pWell->iSel );

      case WCM_GETSELECTEDCOLOUR:
         pWell = GetBlock( hwnd );
         if( pWell->iSel == -1 )
            return( -1 );
         if( pWell->pColourArray == NULL )
            return( -1 );
         return( pWell->pColourArray[ pWell->iSel ] );

      case WCM_GETCOLOURARRAY: {
         COLORREF FAR * lpcr;
         register int i;
         int count;

         pWell = GetBlock( hwnd );
         lpcr = (COLORREF FAR *)lParam;
         count = min( (int)wParam, pWell->cColours );
         for( i = 0; i < count; ++i )
            lpcr[ i ] = pWell->pColourArray[ i ];
         return( TRUE );
         }

      case WM_CONTEXTMENU: /*!!SM!!*/
      case WM_RBUTTONDOWN: {
         NMHDR nmh;

         Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_RCLICK, (LPNMHDR)&nmh );
         break;
         }

      case WM_LBUTTONDOWN: {
         int dx, dy;
         RECT rc;
         POINT pt;
         NMHDR nmh;

         /* Do nowt if window disabled.
          */
         if( !IsWindowEnabled( hwnd ) )
            break;
         pWell = GetBlock( hwnd );
         pt.x = LOWORD( lParam );
         pt.y = HIWORD( lParam );

         /* Make sure control has the focus.
          */
         SetFocus( hwnd );

         /* Compute the size of each slot in the well.
          */
         GetClientRect( hwnd, &rc );
         dx = ( rc.right - rc.left ) / pWell->cx;
         dy = ( rc.bottom - rc.top ) / pWell->cy;

         /* Compute the index of the item at the
          * cursor. (0,0) is the coordinate of the
          * top left corner of the control.
          */
         if( pt.x < pWell->cx * dx && pt.y < pWell->cy * dy )
            {
            if( Amctl_SendNotify( GetParent( hwnd ), hwnd, WCN_SELCHANGING, &nmh ) == 0 )
               {
               int iSel;

               iSel = pWell->cx * ( pt.y / dy );
               iSel += pt.x / dx;
               if( pWell->iSel != -1 )
                  {
                  InvalidateItem( hwnd, pWell, pWell->iSel );
                  UpdateWindow( hwnd );
                  }
               pWell->iSel = iSel;
               InvalidateItem( hwnd, pWell, pWell->iSel );
               UpdateWindow( hwnd );
               Amctl_SendNotify( GetParent( hwnd ), hwnd, WCN_SELCHANGED, &nmh );
               }
            }
         return( 0L );
         }

      case WM_GETDLGCODE:
         return( DLGC_WANTARROWS|DLGC_WANTCHARS );

      case WM_KEYDOWN: {
         int iNewSel;

         if( !IsWindowEnabled( hwnd ) )
            break;
         pWell = GetBlock( hwnd );
         iNewSel = pWell->iSel;
         if( pWell->iSel == -1 )
            iNewSel = 0;
         else
            {
            switch( wParam )
               {
               case VK_LEFT:
                  if( pWell->iSel > 0 )
                     iNewSel = pWell->iSel - 1;
                  break;

               case VK_RIGHT:
                  if( pWell->iSel < pWell->cColours - 1 )
                     iNewSel = pWell->iSel + 1;
                  break;
               }
            }
         if( iNewSel != pWell->iSel )
            {
            NMHDR nmh;

            if( Amctl_SendNotify( GetParent( hwnd ), hwnd, WCN_SELCHANGING, &nmh ) == 0 )
               {
               if( pWell->iSel != -1 )
                  InvalidateItem( hwnd, pWell, pWell->iSel );
               pWell->iSel = iNewSel;
               InvalidateItem( hwnd, pWell, pWell->iSel );
               UpdateWindow( hwnd );
               Amctl_SendNotify( GetParent( hwnd ), hwnd, WCN_SELCHANGED, &nmh );
               }
            }
         break;
         }

      case WM_ERASEBKGND: {
         HBRUSH hbr;
         RECT rc;

         GetClipBox( (HDC)wParam, &rc );
#ifdef WIN32
         hbr = (HBRUSH)SendMessage( GetParent( hwnd ), WM_CTLCOLORDLG, wParam, (LPARAM)hwnd );
#else
         hbr = (HBRUSH)SendMessage( GetParent( hwnd ), WM_CTLCOLOR, wParam, MAKELPARAM( hwnd, CTLCOLOR_DLG ) );
#endif
         FillRect( (HDC)wParam, &rc, hbr );
         return( TRUE );
         }

      case WM_SETFOCUS:
      case WM_KILLFOCUS:
         if( !IsWindowEnabled( hwnd ) )
            break;
         pWell = GetBlock( hwnd );
         if( pWell->iSel != -1 )
            {
            /* Invalidate the selected item so that we draw
             * a focus rectangle around it.
             */
            InvalidateItem( hwnd, pWell, pWell->iSel );
            UpdateWindow( hwnd );
            }
         return( 0L );

      case WM_PAINT: {
         PAINTSTRUCT ps;
         COLORREF FAR * pColour;
         RECT rc;
         HDC hdc;
         int dx, dy;
         int n, x, y;

         /* Begin painting,
          */
         hdc = BeginPaint( hwnd, &ps );
         pWell = GetBlock( hwnd );

         /* Compute the size of each slot in the well.
          */
         GetClientRect( hwnd, &rc );
         dx = ( rc.right - rc.left ) / pWell->cx;
         dy = ( rc.bottom - rc.top ) / pWell->cy;

         /* Loop for each item and draw the well.
          */
         pColour = pWell->pColourArray;
         n = 0;
         for( y = 0; n < pWell->cColours && y < pWell->cy; ++y )
            {
            for( x = 0; n < pWell->cColours && x < pWell->cx; ++x )
               {
               RECT rcItem;
               HBRUSH hbr;

               SetRect( &rcItem, x * dx, y * dy, x * dx + dx, y * dy + dy );
               if( n == pWell->iSel )
                  {
                  FrameRect( hdc, &rcItem, GetStockBrush( BLACK_BRUSH ) );
                  if( GetFocus() == hwnd )
                     DrawFocusRect( hdc, &rcItem );
                  InflateRect( &rcItem, -1, -1 );
                  FrameRect( hdc, &rcItem, GetStockBrush( WHITE_BRUSH ) );
                  InflateRect( &rcItem, -1, -1 );
                  FrameRect( hdc, &rcItem, GetStockBrush( BLACK_BRUSH ) );
                  InflateRect( &rcItem, -1, -1 );
                  }
               else
                  {
               #ifdef WIN32
                  hbr = (HBRUSH)SendMessage( GetParent( hwnd ), WM_CTLCOLORDLG, (WPARAM)hdc, (LPARAM)hwnd );
               #else
                  hbr = (HBRUSH)SendMessage( GetParent( hwnd ), WM_CTLCOLOR, (WPARAM)hdc, MAKELPARAM( hwnd, CTLCOLOR_DLG ) );
               #endif
                  FrameRect( hdc, &rcItem, hbr );
                  InflateRect( &rcItem, -1, -1 );
                  Draw3DFrame( hdc, &rcItem );
                  }
               hbr = CreateSolidBrush( *pColour );
               FillRect( hdc, &rcItem, hbr );
               DeleteBrush( hbr );
               ++pColour;
               ++n;
               }
            }

         /* Finish painting.
          */
         EndPaint( hwnd, &ps );
         return( 0L );
         }

      case WM_DESTROY:
         pWell = GetBlock( hwnd );
         if( pWell->pColourArray )
            FreeMemory( &pWell->pColourArray );
         FreeMemory( &pWell );
         return( 0L );
      }
   return( DefWindowProc( hwnd, message, wParam, lParam ) );
}

/* This function associates the specified colour array with this well
 * control. The cColours parameter specifies the number of RGB colours
 * pointed to by pColours. The function uses this to compute the width
 * and height of the well based on best fit of the specified number of
 * colours.
 */
void FASTCALL SetColourArray( WELL FAR * pWell, int cColours, COLORREF FAR * pColours )
{
   ASSERT( 0 != cColours );
   if( pWell->pColourArray )
      FreeMemory( &pWell->pColourArray );
   if( fNewMemory( &pWell->pColourArray, cColours * sizeof( COLORREF ) ) )
      {
      _fmemcpy( pWell->pColourArray, pColours, cColours * sizeof( COLORREF ) );
      pWell->cColours = cColours;
      }
}

void FASTCALL InvalidateItem( HWND hwnd, WELL FAR * pWell, int iSel )
{
   int dx, dy;
   int x, y;
   RECT rcItem;
   RECT rc;

   /* Compute the size of each slot in the well.
    */
   GetClientRect( hwnd, &rc );
   dx = ( rc.right - rc.left ) / pWell->cx;
   dy = ( rc.bottom - rc.top ) / pWell->cy;

   /* Compute the coordinates of the selected item.
    */
   y = iSel / pWell->cx;
   x = iSel % pWell->cx;
   SetRect( &rcItem, x * dx, y * dy, x * dx + dx, y * dy + dy );
   InvalidateRect( hwnd, &rc, FALSE );
}
