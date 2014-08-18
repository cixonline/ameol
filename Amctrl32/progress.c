/* PROGRESS.C - Implements the progress control
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
#include "amctrls.h"
#include "amctrli.h"

#define  SIZE_BLOCK     10

/* The PROBAR structure holds information for one progress bar
 */
typedef struct tagPROBAR {
   UINT nMinRange;            /* The minimum range of the progress bar */
   UINT nMaxRange;            /* The maximum range of the progress bar */
   UINT nPos;                 /* The current position of the bar */
   UINT nStepInc;             /* The increments by which the bar steps */
   HFONT hFont;               /* The font used in the progress bar */
} PROBAR;

#define  GWW_EXTRA            4
#define  GetBlock(h)          ((PROBAR FAR *)GetWindowLong((h),0))
#define  PutBlock(h,b)        (SetWindowLong((h),0,(LONG)(b)))

LRESULT EXPORT CALLBACK ProgressWndFn( HWND,  UINT, WPARAM, LPARAM );

/* This function registers the progress bar class window to the system.
 */
BOOL FASTCALL RegisterProgressClass( HINSTANCE hInst )
{
   WNDCLASS wc;

   wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
   wc.hIcon          = NULL;
   wc.lpszMenuName   = NULL;
   wc.lpszClassName  = PROGRESS_CLASS;
   wc.hbrBackground  = (HBRUSH)( COLOR_BTNFACE+1 );
   wc.hInstance      = hInst;
   wc.style          = CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS;
   wc.lpfnWndProc    = (WNDPROC)ProgressWndFn;
   wc.cbClsExtra     = 0;
   wc.cbWndExtra     = GWW_EXTRA;
   return( RegisterClass( &wc ) );
}

LRESULT EXPORT CALLBACK ProgressWndFn( HWND hwnd,  UINT message, WPARAM wParam, LPARAM lParam )
{
   PROBAR FAR * pProBar;

#ifdef _DEBUG
   /* Help us to catch NULL pointers...
    */
   pProBar = NULL;
#endif
   switch( message )
      {
      case WM_CREATE: {
         static PROBAR FAR * pNewProBar;

         INITIALISE_PTR(pNewProBar);
         if( fNewMemory( &pNewProBar, sizeof( PROBAR ) ) )
            {
            pNewProBar->nMinRange = 0;
            pNewProBar->nMaxRange = 100;
            pNewProBar->nPos = 0;
            pNewProBar->nStepInc = 10;
            pNewProBar->hFont = GetStockFont( SYSTEM_FONT );
            PutBlock( hwnd, pNewProBar );
            }
         return( pNewProBar ? 0 : -1 );
         }

      case PBM_GETPOS:
         /* Return the current progress bar position.
          * NEW for 1.2!
          */
         pProBar = GetBlock( hwnd );
         return( pProBar->nPos );

      case PBM_SETPOS: {
         UINT nPrevPos;

         pProBar = GetBlock( hwnd );
         nPrevPos = pProBar->nPos;
         if( wParam != nPrevPos )
            {
            if( ( pProBar->nPos = wParam ) > pProBar->nMaxRange )
               pProBar->nPos = pProBar->nMinRange;
            InvalidateRect( hwnd, NULL, TRUE );
            UpdateWindow( hwnd );
            }
         return( nPrevPos );
         }

      case PBM_STEPIT: {
         UINT nPrevPos;

         pProBar = GetBlock( hwnd );
         nPrevPos = pProBar->nPos;
         if( ( pProBar->nPos += pProBar->nStepInc ) > pProBar->nMaxRange )
            pProBar->nPos = pProBar->nMinRange;
         InvalidateRect( hwnd, NULL, TRUE );
         UpdateWindow( hwnd );
         return( nPrevPos );
         }

      case PBM_DELTAPOS: {
         UINT nPrevPos;

         pProBar = GetBlock( hwnd );
         nPrevPos = pProBar->nPos;
         if( ( pProBar->nPos += wParam ) > pProBar->nMaxRange )
            pProBar->nPos -= pProBar->nMaxRange;
         InvalidateRect( hwnd, NULL, TRUE );
         UpdateWindow( hwnd );
         return( nPrevPos );
         }

      case PBM_GETRANGE: {
         PBRANGE FAR * lppbr;

         /* Return a 32-bit range. If lParam is non-NULL, it should be a
          * pointer to a PBRANGE structure. Otherwise wParam specifies
          * which of the low/high range is returned.
          * NEW for 1.2!
          */
         lppbr = (PBRANGE FAR *)lParam;
         pProBar = GetBlock( hwnd );
         if( NULL != lppbr )
            {
            lppbr->iLow = pProBar->nMinRange;
            lppbr->iHigh = pProBar->nMaxRange;
            return( TRUE );
            }
         return( wParam ? pProBar->nMinRange : pProBar->nMaxRange );
         }

   #ifdef WIN32
      case PBM_SETRANGE32:
         /* Set a 32-bit range. No return value.
          * NEW for 1.2!
          */
         pProBar = GetBlock( hwnd );
         pProBar->nMinRange = wParam;
         pProBar->nMaxRange = lParam;
         InvalidateRect( hwnd, NULL, TRUE );
         UpdateWindow( hwnd );
         return( 0L );
   #endif

      case PBM_SETRANGE: {
         DWORD nPrevRange;

         pProBar = GetBlock( hwnd );
         nPrevRange = MAKELPARAM( pProBar->nMinRange, pProBar->nMaxRange );
         pProBar->nMinRange = LOWORD( lParam );
         pProBar->nMaxRange = HIWORD( lParam );
         InvalidateRect( hwnd, NULL, TRUE );
         UpdateWindow( hwnd );
         return( nPrevRange );
         }

      case PBM_SETSTEP: {
         DWORD nPrevStep;

         pProBar = GetBlock( hwnd );
         nPrevStep = pProBar->nStepInc;
         pProBar->nStepInc = wParam;
         return( nPrevStep );
         }

      case WM_SETFONT: {
         HFONT hOldFont;

         pProBar = GetBlock( hwnd );
         hOldFont = pProBar->hFont;
         pProBar->hFont = wParam ? (HFONT)wParam : GetStockFont( SYSTEM_FONT );
         if( LOWORD( lParam ) )
            {
            InvalidateRect( hwnd, NULL, TRUE );
            UpdateWindow( hwnd );
            }
         return( (LRESULT)(LPSTR)hOldFont );
         }

      case WM_GETFONT:
         pProBar = GetBlock( hwnd );
         return( (LRESULT)(LPSTR)pProBar->hFont );

      case WM_ERASEBKGND: {
         HDC hdc;
         HPEN hpenWhite;
         HPEN hpenGray;
         HPEN hOldPen;
         RECT rc;

         hdc = (HDC)wParam;
         hpenWhite = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNHIGHLIGHT ) );
         hpenGray = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNSHADOW ) );
         GetClientRect( hwnd, &rc );
         hOldPen = SelectPen( hdc, hpenGray );
         MoveToEx( hdc, rc.left, rc.bottom - 2, NULL );
         LineTo( hdc, rc.left, rc.top );
         LineTo( hdc, rc.right - 1, rc.top );
         SelectPen( hdc, hpenWhite );
         MoveToEx( hdc, rc.left, rc.bottom - 1, NULL );
         LineTo( hdc, rc.right - 1, rc.bottom  - 1 );
         LineTo( hdc, rc.right - 1, rc.top - 1 );
         SelectPen( hdc, hOldPen );
         DeletePen( hpenGray );
         DeletePen( hpenWhite );
         return( TRUE );
         }

      case WM_PAINT: {
         PAINTSTRUCT ps;
         HBRUSH hbrBlock;
         HBRUSH hbrFace;
         int cBlocks;
         UINT nRange;
         RECT rcBlock;
         int nRightEdge;
         int c, x;
         RECT rc;
         HDC hdc;

         hdc = BeginPaint( hwnd, &ps );
         pProBar = GetBlock( hwnd );
         GetClientRect( hwnd, &rc );
         InflateRect( &rc, -1, -2 );
         nRange = max( pProBar->nMaxRange - pProBar->nMinRange, 1 );
         x = (WORD)( ( (DWORD)pProBar->nPos * ( rc.right - rc.left ) ) / nRange ) + 1;
         if( GetWindowStyle( hwnd ) & PBS_SMOOTH )
            {
            int nPercentage;
            char ach[ 30 ];
            SIZE size;
            RECT rc1;
            RECT rc2;
            int dx;
            int dy;

            /* Smooth mode, so show progress as a gradual bar with the
             * percentage inside it.
             */
            if( ( nPercentage = (WORD)( ( (DWORD)pProBar->nPos * 100 ) / nRange ) ) > 100 )
               nPercentage = 100;
            wsprintf( ach, "%3d%%", nPercentage );
            GetTextExtentPoint( hdc, ach, 4, &size );
            rc1 = rc;
            rc2 = rc;
            dx = rc1.right;
            dy = rc1.bottom;
            rc1.right = x;
            rc2.left  = x;
            SetBkColor( hdc, GetSysColor( COLOR_HIGHLIGHT ) );
            SetTextColor( hdc, GetSysColor( COLOR_HIGHLIGHTTEXT ) );
            ExtTextOut( hdc, ( dx - size.cx ) / 2,
                        ( dy - size.cy ) / 2,
                        ETO_OPAQUE | ETO_CLIPPED, &rc1, ach, 4, NULL );
            SetBkColor( hdc, GetSysColor( COLOR_HIGHLIGHTTEXT ) );
            SetTextColor( hdc, GetSysColor( COLOR_HIGHLIGHT ) );
            ExtTextOut( hdc, ( dx - size.cx ) / 2,
                        ( dy - size.cy ) / 2,
                        ETO_OPAQUE | ETO_CLIPPED, &rc2, ach, 4, NULL );
            }
         else
            {
            /* Block mode, so show progress as a series of fixed width
             * blocks running right to left.
             */
            cBlocks = x / SIZE_BLOCK;
            hbrBlock = CreateSolidBrush( GetSysColor( COLOR_HIGHLIGHT ) );
            nRightEdge = rc.left;
            for( c = 0; c < cBlocks; ++c )
               {
               SetRect( &rcBlock, rc.left + ( c * SIZE_BLOCK ) + 1, rc.top, ( c * SIZE_BLOCK ) + SIZE_BLOCK, rc.bottom );
               FillRect( hdc, &rcBlock, hbrBlock );
               nRightEdge = rcBlock.right;
               }
            hbrFace = CreateSolidBrush( GetSysColor( COLOR_BTNFACE ) );
            rc.left = nRightEdge;
            FillRect( hdc, &rc, hbrFace );
            DeleteBrush( hbrFace );
            DeleteBrush( hbrBlock );
            }
         EndPaint( hwnd, &ps );
         return( 0L );
         }

      case WM_DESTROY:
         pProBar = GetBlock( hwnd );
         FreeMemory( &pProBar );
         break;
      }
   return( DefWindowProc( hwnd, message, wParam, lParam ) );
}
