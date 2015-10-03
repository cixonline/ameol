/* HEADER.C - Implements the header control
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
#include <memory.h>
#include "amctrls.h"
#include "amctrli.h"
#include "amctrlrc.h"

#define  THIS_FILE   __FILE__

#define  DEF_VBORDER          2
#define  DEF_HBORDER          2
#define  DEF_INTERSPACING     0

#define  MINIMUM_MARGIN       10

#define  cxDragZone           20

typedef struct tagPART {
   struct tagPART FAR * pNextPart;  /* Next header part */
   struct tagPART FAR * pPrevPart;  /* Previous header part */
   RECT rcItem;                     /* Part rectangle */
   RECT rcOld;                      /* Part rectangle before resizing */
   BOOL fSelected;                  /* TRUE if part is pressed */
   BOOL fSpringy;                   /* TRUE if this part is springy */
   HD_ITEM HdItem;                  /* Item definitions */
} PART;

typedef struct tagHEADER {
   int cySize;                      /* Height of control */
   int cyExtent;                    /* Height of divider bar when resizing columns */
   int cParts;                      /* Number of parts */
   int nDragSelPart;                /* Index of part being selected/dragged */
   BOOL fSelecting;                 /* TRUE if a part is selected */
   BOOL fDragging;                  /* TRUE if a part is being dragged */
   BOOL fFirstDrag;                 /* TRUE if dragging but not yet moved */
   BOOL fRedraw;                    /* TRUE if repainting after update permitted */
   POINT ptDrag;                    /* Drag coordinates */
   int nDragPt;                     /* Start coordinates */
   int cSpringy;                    /* Number of springy parts */
   HFONT hFont;                     /* Header bar font */
   BOOL fDrawDivider;               /* TRUE if we draw the divider */
   PART FAR * pFirstPart;           /* First header part */
   PART FAR * pLastPart;            /* Last header part */
} HEADER;

#define  GWW_EXTRA            4
#define  GetBlock(h)          ((HEADER FAR *)GetWindowLong((h),0))
#define  PutBlock(h,b)        (SetWindowLong((h),0,(LONG)(b)))

static HCURSOR hHeaderDragCursor;

static void DrawHeaderBar( HWND, HDC, HEADER FAR * );
static void DrawHeaderText( HWND, HDC, PART FAR *, RECT *, int );
static PART FAR * GetPart( HEADER FAR *, int );
static void CalculateHeaderParts( HWND, HEADER FAR *, BOOL );
static void DrawDividerLine( HWND, HEADER FAR * );

LRESULT EXPORT CALLBACK HeaderWndFn( HWND,  UINT, WPARAM, LPARAM );

/* This function registers the header control class window to the system.
 */
BOOL FASTCALL RegisterHeaderClass( HINSTANCE hInst )
{
   WNDCLASS wc;

   wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
   wc.hIcon          = NULL;
   wc.lpszMenuName   = NULL;
   wc.lpszClassName  = WC_HEADER;
   wc.hbrBackground  = (HBRUSH)( COLOR_BTNFACE+1 );
   wc.hInstance      = hInst;
   wc.style          = CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS | CS_DBLCLKS;
   wc.lpfnWndProc    = HeaderWndFn;
   wc.cbClsExtra     = 0;
   wc.cbWndExtra     = GWW_EXTRA;
   return( RegisterClass( &wc ) );
}

LRESULT EXPORT CALLBACK HeaderWndFn( HWND hwnd,  UINT message, WPARAM wParam, LPARAM lParam )
{
   static int nCalls = 0;
   HEADER FAR * pHeader;
   PART FAR * pPart;

#ifdef _DEBUG
   /* Help us to catch NULL pointers...
    */
   pHeader = NULL;
   pPart = NULL;
#endif
   switch( message )
      {
      case WM_CREATE: {
         static HEADER FAR * pNewHeader;

         /* If this is the first header control created, load the drag
          * cursor.
          */
         if( 0 == nCalls++ )
            {
            hHeaderDragCursor = LoadCursor( hLibInst, MAKEINTRESOURCE( IDC_VSPLITCUR ) );
            ASSERT( NULL != hHeaderDragCursor );
            }

         INITIALISE_PTR(pNewHeader);
         if( fNewMemory( &pNewHeader, sizeof( HEADER ) ) )
            {
            TEXTMETRIC tm;
            HDC hdc;

            pNewHeader->cParts = 0;
            pNewHeader->nDragSelPart = -1;
            pNewHeader->hFont = GetStockFont( SYSTEM_FONT );
            pNewHeader->pFirstPart = NULL;
            pNewHeader->pLastPart = NULL;
            pNewHeader->fDragging = FALSE;
            pNewHeader->fSelecting = FALSE;
            pNewHeader->cSpringy = 0;
            pNewHeader->cyExtent = 0;
            pNewHeader->fRedraw = TRUE;
            pNewHeader->fDrawDivider = FALSE;

            hdc = GetDC( hwnd );
            SelectFont( hdc, pNewHeader->hFont );
            GetTextMetrics( hdc, &tm );
            ReleaseDC( hwnd, hdc );
            pNewHeader->cySize = tm.tmHeight + DEF_HBORDER * 2;
            }
         PutBlock( hwnd, pNewHeader );
         break;
         }

      case WM_SETREDRAW:
         pHeader = GetBlock( hwnd );
         pHeader->fRedraw = wParam;
         return( 0L );

      case HDM_DELETEITEM:
         pHeader = GetBlock( hwnd );
         if( (int)wParam < pHeader->cParts )
            {
            pPart = GetPart( pHeader, wParam );
            if( NULL == pPart->pPrevPart )
               pHeader->pFirstPart = pPart->pNextPart;
            else
               pPart->pPrevPart->pNextPart = pPart->pNextPart;
            if( NULL == pPart->pNextPart )
               pHeader->pLastPart = pPart->pPrevPart;
            else
               pPart->pNextPart->pPrevPart = pPart->pPrevPart;
            --pHeader->cParts;
            FreeMemory( &pPart );
            CalculateHeaderParts( hwnd, pHeader, pHeader->fRedraw );
            return( TRUE );
            }
         return( FALSE );

      case HDM_INSERTITEM: {
         static PART FAR * pNewPart;
         NMHDR nmhdr;

         pHeader = GetBlock( hwnd );
         if( (int)wParam >= pHeader->cParts )
            wParam = pHeader->cParts;
         pPart = GetPart( pHeader, wParam );
         INITIALISE_PTR(pNewPart);
         if( fNewMemory( &pNewPart, sizeof( PART ) ) )
            {
            HD_ITEM FAR * lpHdItem;

            lpHdItem = (HD_ITEM FAR *)lParam;
            if( pPart )
               {
               pNewPart->pPrevPart = pPart->pPrevPart;
               if( pPart->pPrevPart )
                  pPart->pPrevPart->pNextPart = pNewPart;
               pPart->pPrevPart = pNewPart;
               }
            else
               {
               if( pHeader->pLastPart )
                  pHeader->pLastPart->pNextPart = pNewPart;
               pNewPart->pPrevPart = pHeader->pLastPart;
               }
            pNewPart->pNextPart = pPart;
            if( NULL == pNewPart->pPrevPart )
               pHeader->pFirstPart = pNewPart;
            if( NULL == pNewPart->pNextPart )
               pHeader->pLastPart = pNewPart;
            pNewPart->HdItem = *lpHdItem;
            pNewPart->fSelected = FALSE;
            pNewPart->fSpringy = FALSE;
            if( pNewPart->HdItem.mask & HDI_TEXT )
               {
               static LPSTR lpszText;

               INITIALISE_PTR(lpszText);
               if( fNewMemory( &lpszText, lstrlen( lpHdItem->pszText ) + 1 ) )
                  {
                  pNewPart->HdItem.pszText = lpszText;
                  lstrcpy( pNewPart->HdItem.pszText, lpHdItem->pszText );
                  }
               else
                  Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_OUTOFMEMORY, &nmhdr );
               }
            else
               pNewPart->HdItem.pszText = NULL;
            ++pHeader->cParts;
            CalculateHeaderParts( hwnd, pHeader, pHeader->fRedraw );
            }
         else
            Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_OUTOFMEMORY, &nmhdr );
         return( pNewPart ? wParam : -1 );
         }

      case HDM_GETITEM:
         pHeader = GetBlock( hwnd );
         if( (int)wParam < pHeader->cParts )
            {
            HD_ITEM FAR * lpHdItem;

            pPart = GetPart( pHeader, wParam );
            lpHdItem = (HD_ITEM FAR *)lParam;
            *lpHdItem = pPart->HdItem;
            return( TRUE );
            }
         return( FALSE );

      case HDM_SETITEMWIDTH:
         pHeader = GetBlock( hwnd );
         if( (int)wParam < pHeader->cParts )
            {
            pPart = GetPart( pHeader, wParam );
            pPart->HdItem.cxy = LOWORD(lParam);
            CalculateHeaderParts( hwnd, pHeader, pHeader->fRedraw );
            }
         return( FALSE );

      case HDM_SETITEM:
         pHeader = GetBlock( hwnd );
         if( (int)wParam < pHeader->cParts )
            {
            HD_ITEM FAR * lpHdItem;
            HD_NOTIFY hdn;

            pPart = GetPart( pHeader, wParam );
            lpHdItem = (HD_ITEM FAR *)lParam;
            hdn.iItem = wParam;
            hdn.pitem = lpHdItem;
            if( Amctl_SendNotify( GetParent( hwnd ), hwnd, HDN_ITEMCHANGING, (LPNMHDR)&hdn ) )
               {
               pPart->HdItem = *lpHdItem;
               CalculateHeaderParts( hwnd, pHeader, pHeader->fRedraw );
               Amctl_SendNotify( GetParent( hwnd ), hwnd, HDN_ITEMCHANGED, (LPNMHDR)&hdn );
               return( TRUE );
               }
            }
         return( FALSE );

      case HDM_GETITEMCOUNT:
         pHeader = GetBlock( hwnd );
         return( pHeader->cParts );

      case HDM_LAYOUT: {
         HD_LAYOUT FAR * lpLayout;

         pHeader = GetBlock( hwnd );
         lpLayout = (HD_LAYOUT FAR *)lParam;
         lpLayout->pwpos->x = lpLayout->prc->left;
         lpLayout->pwpos->y = lpLayout->prc->top;
         lpLayout->pwpos->cx = lpLayout->prc->right - lpLayout->prc->left;
         lpLayout->pwpos->cy = pHeader->cySize;
         lpLayout->pwpos->hwndInsertAfter = GetParent( hwnd );
         lpLayout->pwpos->hwnd = hwnd;
         lpLayout->pwpos->flags = 0;
         return( TRUE );
         }

      case HDM_SHOWITEM: {
         pHeader = GetBlock( hwnd );
         if( (int)wParam < pHeader->cParts )
            {
            pPart = GetPart( pHeader, wParam );
            if( lParam )
               pPart->HdItem.fmt &= ~HDF_HIDDEN;
            else
               pPart->HdItem.fmt |= HDF_HIDDEN;
            CalculateHeaderParts( hwnd, pHeader, pHeader->fRedraw );
            return( TRUE );
            }
         return( FALSE );
         }

      case HDM_SETEXTENT: {
         int nOldExtent;

         pHeader = GetBlock( hwnd );
         nOldExtent = pHeader->cyExtent;
         pHeader->cyExtent = wParam;
         return( nOldExtent );
         }

      case WM_SETFONT: {
         TEXTMETRIC tm;
         HDC hdc;

         pHeader = GetBlock( hwnd );
         pHeader->hFont = wParam ? (HFONT)wParam : GetStockFont( SYSTEM_FONT );
         if( !( GetWindowStyle( hwnd ) & ACTL_CCS_NORESIZE ) )
            {
            RECT rc;

            hdc = GetDC( hwnd );
            SelectFont( hdc, pHeader->hFont );
            GetTextMetrics( hdc, &tm );
            ReleaseDC( hwnd, hdc );

            pHeader->cySize = tm.tmHeight + DEF_HBORDER * 2;
            GetClientRect( GetParent( hwnd ), &rc );
            SetWindowPos( hwnd, NULL, 0, 0, rc.right - 1, pHeader->cySize, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER );
            }
         CalculateHeaderParts( hwnd, pHeader, pHeader->fRedraw );
         if( LOWORD( lParam ) )
            UpdateWindow( hwnd );
         return( 0L );
         }

      case WM_SETTEXT:
         if( wParam & HBT_SPRING )
            {
            pHeader = GetBlock( hwnd );
            pPart = GetPart( pHeader, wParam & 0xFF );
            pPart->fSpringy = TRUE;
            ++pHeader->cSpringy;
            CalculateHeaderParts( hwnd, pHeader, pHeader->fRedraw );
            UpdateWindow( hwnd );
            }
         break;

      case WM_GETFONT:
         pHeader = GetBlock( hwnd );
         return( (LRESULT)(LPSTR)pHeader->hFont );

      case WM_GETDLGCODE:
         return( DLGC_WANTTAB | DLGC_WANTARROWS );

      case WM_SETCURSOR: {
         register int c;
         POINT pt;

         pHeader = GetBlock( hwnd );
         if( pHeader->fDragging )
            {
            SetCursor( hHeaderDragCursor );
            return( TRUE );
            }
         pPart = pHeader->pFirstPart;
         GetCursorPos( &pt );
         ScreenToClient( hwnd, &pt );
         for( c = 0; c < pHeader->cParts - 1; ++c )
            {
            RECT rc;

            CopyRect( &rc, &pPart->rcItem );
            rc.left = rc.right - ( cxDragZone / 2 );
            rc.right = rc.right + ( cxDragZone / 2 );
            if( PtInRect( &rc, pt ) )
               {
               SetCursor( hHeaderDragCursor );
               return( TRUE );
               }
            pPart = pPart->pNextPart;
            }
         break;
         }

      case WM_CONTEXTMENU: /*!!SM!!*/
      case WM_RBUTTONDOWN: {
         NMHDR nmh;

         Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_RCLICK, (LPNMHDR)&nmh );
         break;
         }

      case WM_LBUTTONDBLCLK:
      case WM_LBUTTONDOWN: {
         register int c;
         HD_NOTIFY hdn;
         POINT pt;

         pHeader = GetBlock( hwnd );
         pPart = pHeader->pLastPart;
         pHeader->nDragSelPart = -1;
         pt.x = LOWORD( lParam );
         pt.y = HIWORD( lParam );
         for( c = pHeader->cParts - 1; c >= 0; --c )
            {
            RECT rc;

            CopyRect( &rc, &pPart->rcItem );
            if( c > 0 )
               rc.left += cxDragZone / 2;
            if( c < pHeader->cParts - 1 )
               rc.right -= cxDragZone / 2;
            if( PtInRect( &rc, pt ) && ( GetWindowStyle( hwnd ) & HDS_BUTTONS ) )
               {
               if( WM_LBUTTONDBLCLK == message )
                  {
                  hdn.iButton = 0;
                  hdn.iItem = c;
                  hdn.pitem = &pPart->HdItem;
                  Amctl_SendNotify( GetParent( hwnd ), hwnd, HDN_ITEMDBLCLICK, (LPNMHDR)&hdn );
                  }
               else
                  {
                  pHeader->fSelecting = TRUE;
                  pPart->fSelected = TRUE;
                  pHeader->nDragSelPart = c;
                  InvalidateRect( hwnd, &pPart->rcItem, TRUE );
                  UpdateWindow( hwnd );
                  SetCapture( hwnd );
                  }
               break;
               }
            if( c < pHeader->cParts - 1 )
               {
               CopyRect( &rc, &pPart->rcItem );
               rc.left = rc.right - ( cxDragZone / 2 );
               rc.right = rc.right + ( cxDragZone / 2 );
               if( PtInRect( &rc, pt ) )
                  {
                  hdn.iButton = 0;
                  hdn.iItem = c;
                  hdn.pitem = &pPart->HdItem;
                  if( WM_LBUTTONDBLCLK == message )
                     Amctl_SendNotify( GetParent( hwnd ), hwnd, HDN_DIVIDERDBLCLICK, (LPNMHDR)&hdn );
                  else if( pHeader->fDragging = (BOOL)Amctl_SendNotify( GetParent( hwnd ), hwnd, HDN_BEGINTRACK, (LPNMHDR)&hdn ) )
                     {
                     SetCapture( hwnd );
                     pHeader->nDragSelPart = c;
                     pHeader->ptDrag = pt;
                     pHeader->nDragPt = pPart->rcItem.right;
                     pHeader->fFirstDrag = TRUE;
                     }
                  break;
                  }
               }
            pPart = pPart->pPrevPart;
            }
         return( 0L );
         }

      case WM_LBUTTONUP: {
         HD_NOTIFY hdn;

         pHeader = GetBlock( hwnd );
         if( pHeader->nDragSelPart >= 0 )
            {
            pPart = GetPart( pHeader, pHeader->nDragSelPart );
            if( pHeader->fSelecting )
               {
               if( pPart->fSelected )
                  {
                  pPart->fSelected = FALSE;
                  InvalidateRect( hwnd, &pPart->rcItem, TRUE );
                  UpdateWindow( hwnd );
                  hdn.iButton = 0;
                  hdn.iItem = pHeader->nDragSelPart;
                  hdn.pitem = &pPart->HdItem;
                  Amctl_SendNotify( GetParent( hwnd ), hwnd, HDN_ITEMCLICK, (LPNMHDR)&hdn );
                  }
               pHeader->fSelecting = FALSE;
               }
            else if( pHeader->fDragging )
               {
               int nDiff;

               if( !pHeader->fFirstDrag )
                  DrawDividerLine( hwnd, pHeader );
               nDiff = pHeader->nDragPt - pPart->rcItem.right;
               pPart->HdItem.cxy += nDiff;
               CalculateHeaderParts( hwnd, pHeader, TRUE );
               UpdateWindow( hwnd );

               hdn.iButton = 0;
               hdn.iItem = pHeader->nDragSelPart;
               hdn.pitem = &pPart->HdItem;
               hdn.cxyUpdate = pPart->rcItem.right + nDiff;
               Amctl_SendNotify( GetParent( hwnd ), hwnd, HDN_ENDTRACK, (LPNMHDR)&hdn );
               pHeader->fDragging = FALSE;
               }
            ReleaseCapture();
            }
         return( 0L );
         }

      case WM_MOUSEMOVE:
         pHeader = GetBlock( hwnd );
         if( pHeader->nDragSelPart >= 0 )
            {
            POINT pt;

            pPart = GetPart( pHeader, pHeader->nDragSelPart );
            pt.x = LOWORD( lParam );
            pt.y = HIWORD( lParam );
            if( pHeader->fSelecting )
               {
               BOOL fOldSelected;

               fOldSelected = pPart->fSelected;
               pPart->fSelected = PtInRect( &pPart->rcItem, pt );
               if( fOldSelected != pPart->fSelected )
                  {
                  InvalidateRect( hwnd, &pPart->rcItem, FALSE );
                  UpdateWindow( hwnd );
                  }
               }
            else if( pHeader->fDragging )
               {
               RECT rc;
               int nDiff;

               nDiff = pt.x - pHeader->ptDrag.x;
               GetClientRect( hwnd, &rc );
               if( rc.left + pHeader->nDragPt + nDiff < pPart->rcItem.left + ( cxDragZone / 2 ) )
                  {
                  pt.x = rc.left + pPart->rcItem.left + ( cxDragZone / 2 );
                  nDiff = pt.x - pHeader->ptDrag.x;
                  }
               else if( rc.left + pHeader->nDragPt + nDiff > rc.right - ( cxDragZone / 2 ) )
                  {
                  pt.x = rc.right - ( cxDragZone / 2 );
                  nDiff = pt.x - pHeader->ptDrag.x;
                  }
               if( nDiff )
                  {
                  HD_NOTIFY hdn;

                  /* Update internal drag stuff.
                   */
                  if( !pHeader->fFirstDrag )
                     DrawDividerLine( hwnd, pHeader );
                  pHeader->fFirstDrag = FALSE;
                  pHeader->nDragPt += nDiff;

                  /* Update part position.
                   */
                  nDiff = pHeader->nDragPt - pPart->rcItem.right;
                  pPart->HdItem.cxy += nDiff;
                  CalculateHeaderParts( hwnd, pHeader, FALSE );

                  /* Update header by scrolling it.
                   */
                  rc.left = pPart->rcItem.right - 4;
                  rc.right -= 1;
                  if( nDiff > 0 )
                     rc.left -= nDiff;
                  ScrollWindow( hwnd, nDiff, 0, &rc, &rc );
                  UpdateWindow( hwnd );

                  /* Send notification to parent about change
                   * to the drag.
                   */
                  hdn.iButton = 0;
                  hdn.iItem = pHeader->nDragSelPart;
                  hdn.pitem = &pPart->HdItem;
                  hdn.cxyUpdate = pPart->rcItem.right;
                  Amctl_SendNotify( GetParent( hwnd ), hwnd, HDN_TRACK, (LPNMHDR)&hdn );

                  /* Draw the divider line
                   */
                  DrawDividerLine( hwnd, pHeader );
                  pHeader->ptDrag = pt;
                  }
               }
            }
         break;

      case WM_DESTROY: {
         register int c;

         if( 0 == --nCalls )
            DestroyCursor( hHeaderDragCursor );
         pHeader = GetBlock( hwnd );
         pPart = pHeader->pFirstPart;
         for( c = 0; c < pHeader->cParts; ++c )
            {
            PART FAR * pNextPart;

            pNextPart = pPart->pNextPart;
            if( pPart->HdItem.pszText )
               FreeMemory( &pPart->HdItem.pszText );
            FreeMemory( &pPart );
            pPart = pNextPart;
            }
         FreeMemory( &pHeader );
         break;
         }

      case WM_MOVE:
         /* Trap MoveWindow and SetWindowPos on this window so that the
          * status bar remains anchored to the top or bottom of the parent window.
          */
         if( !( GetWindowStyle( hwnd ) & ACTL_CCS_NOPARENTALIGN ) )
            {
            RECT rc;
   
            GetClientRect( GetParent( hwnd ), &rc );
            SetWindowPos( hwnd, NULL, rc.left, rc.top, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER );
            }
         break;

      case WM_SIZE:
         pHeader = GetBlock( hwnd );
         if( !( GetWindowStyle( hwnd ) & ACTL_CCS_NORESIZE ) )
            {
            RECT rc;
   
            GetClientRect( GetParent( hwnd ), &rc );
            SetWindowPos( hwnd, NULL, 0, 0, rc.right, pHeader->cySize, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER );
            }
         else
            pHeader->cySize = HIWORD( lParam );
         CalculateHeaderParts( hwnd, pHeader, TRUE );
         break;

      case WM_PAINT: {
         PAINTSTRUCT ps;
         HDC hdc;

         pHeader = GetBlock( hwnd );
         hdc = BeginPaint( hwnd, (LPPAINTSTRUCT)&ps );
         DrawHeaderBar( hwnd, hdc, pHeader );
         EndPaint( hwnd, (LPPAINTSTRUCT)&ps );
         break;
         }
      }
   return( DefWindowProc( hwnd, message, wParam, lParam ) );
}

/* This function draws the divider line used when resizing
 * a header column. It uses DSTINVERT so it needs to be called
 * once to draw the line and again to remove it.
 */
static void DrawDividerLine( HWND hwnd, HEADER FAR * pHeader )
{
   if( pHeader->fDrawDivider )
      {
      HWND hwndParent;
      RECT rcParent;
      RECT rc;
      HDC hdc;

      ASSERT( pHeader != NULL );
      hwndParent = GetParent( hwnd );
      hdc = GetDC( hwndParent );
      GetClientRect( hwndParent, &rcParent );
      if( pHeader->cyExtent )
         rcParent.bottom = pHeader->cyExtent;
      GetClientRect( hwnd, &rc );
      MapWindowPoints( hwnd, hwndParent, (LPPOINT)&rc, 1 );
      PatBlt( hdc, rc.left + pHeader->nDragPt, 0, 1, rcParent.bottom, DSTINVERT );
      ReleaseDC( hwndParent, hdc );
      }
}

/* This function retrieves the handle of an item given the
 * index of the item, where 0 is the index of the first item.
 */
static PART FAR * GetPart( HEADER FAR * pHeader, int index )
{
   PART FAR * pPart;

   ASSERT( pHeader != NULL );
   pPart = pHeader->pFirstPart;
   while( pPart && index-- )
      pPart = pPart->pNextPart;
   return( pPart );
}

/* This function recomputes the widths and heights of each header item
 * after a new item has been added, a header column has been adjusted or
 * the attributes of an existing item has been changed.
 */
static void CalculateHeaderParts( HWND hwnd, HEADER FAR * pHeader, BOOL fInvalidate )
{
   register int c;
   PART FAR * pPart;
   BOOL fChange;
   RECT rc;
   int x;

   ASSERT( pHeader != NULL );

   /* First update the rectangles of each item.
    */
   pPart = pHeader->pFirstPart;
   x = 0;
   for( c = 0; c < pHeader->cParts; ++c )
      {
      if( !( pPart->HdItem.fmt & HDF_HIDDEN ) )
         {
         CopyRect( &pPart->rcOld, &pPart->rcItem );
         SetRect( &pPart->rcItem, x, 0, x + pPart->HdItem.cxy, pHeader->cySize );
         x += pPart->HdItem.cxy;
         }
      else
         SetRectEmpty( &pPart->rcItem );
      pPart = pPart->pNextPart;
      }

   /* If this is a springy header, distribute the slack at the end amongst
    * all the springy items. So if, for example, we have 30 pixels slack at
    * the end and 6 springy items, we would add 5 pixels to the width of those
    * 6 items so that the control filled the entire parent window width.
    */
   if( pHeader->cSpringy )
      {
      int nBits;

      GetClientRect( hwnd, &rc );
      nBits = ( rc.right - x ) / pHeader->cSpringy;
      pPart = pHeader->pFirstPart;
      x = 0;
      for( c = 0; c < pHeader->cParts; ++c )
         {
         if( !( pPart->HdItem.fmt & HDF_HIDDEN ) )
            {
            if( pPart->fSpringy )
               pPart->HdItem.cxy += nBits;
            if( c == pHeader->cParts - 1 )
               {
               pPart->HdItem.cxy = rc.right - x;
               SetRect( &pPart->rcItem, x, 0, rc.right, pHeader->cySize );
               }
            else
               SetRect( &pPart->rcItem, x, 0, x + pPart->HdItem.cxy, pHeader->cySize );
            x += pPart->HdItem.cxy;
            }
         pPart = pPart->pNextPart;
         }
      }

   /* If we have been asked to invalidate changed items, compare the
    * old rectangle with the new for each item and invalidate those that
    * differ. This way, we only repaint the parts that actually change.
    */
   if( fInvalidate )
      {
      fChange = FALSE;
      pPart = pHeader->pFirstPart;
      for( c = 0; c < pHeader->cParts; ++c )
         {
         if( _fmemcmp( &pPart->rcItem, &pPart->rcOld, sizeof( RECT ) ) )
            {
            InvalidateRect( hwnd, &pPart->rcOld, FALSE );
            InvalidateRect( hwnd, &pPart->rcItem, FALSE );
            fChange = TRUE;
            }
         pPart = pPart->pNextPart;
         }

      /* If anything changed, refresh the cursor shape.
       */
      if( fChange )
         SendMessage( hwnd, WM_SETCURSOR, 0, 0L );
      }
}

/* This function redraws the header bar.
 */
static void DrawHeaderBar( HWND hwnd, HDC hdc, HEADER FAR * pHeader )
{
   HPEN hpenOld;
   HPEN hpenBlack;
   HPEN hpenDkGray;
   HPEN hpenWhite;
   PART FAR * pPart;
   register int c;
   HBRUSH hbr;
   RECT rc;

   ASSERT( pHeader != NULL );

   /* Get the header bar rectangle.
    */
   GetClientRect( hwnd, &rc );

   /* Create the pens being used to draw the header bar.
    */
   hpenWhite = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNHIGHLIGHT ) );
   hpenDkGray = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNSHADOW ) );
   hpenBlack = GetStockPen( BLACK_PEN );
   hpenOld = SelectPen( hdc, hpenBlack );
#ifndef WIN32
   ASSERT( NULL != hpenDkGray );
   ASSERT( NULL != hpenBlack );
   ASSERT( NULL != hpenWhite );
#endif

   /* Fill the header bar with the background colour.
    */
   hbr = CreateSolidBrush( GetSysColor( COLOR_BTNFACE ) );
   FillRect( hdc, &rc, hbr );
   DeleteBrush( hbr );

   /* Now, for each button, draw the button and the text
    * in the button.
    */
   pPart = pHeader->pFirstPart;
   SetTextColor( hdc, GetSysColor( COLOR_BTNTEXT ) );
   SetBkColor( hdc, GetSysColor( COLOR_BTNFACE ) );
   SelectFont( hdc, pHeader->hFont );
   for( c = 0; c < pHeader->cParts; ++c )
      {
      if( !( pPart->HdItem.fmt & HDF_HIDDEN ) )
         {
         static RECT rc2;
   
         CopyRect( &rc2, &pPart->rcItem );
         --rc2.bottom;
         if( c < pHeader->cParts - 1 )
            rc2.right -= 1;
         if( pPart->fSelected )
            {
            /* If the header part is a button and it's selected,
             * draw it in the down position.
             */
            SelectPen( hdc, hpenDkGray );
            MoveToEx( hdc, rc2.left, rc2.bottom - 1, NULL );
            LineTo( hdc, rc2.left, rc2.top );
            LineTo( hdc, rc2.right, rc2.top );
            SelectPen( hdc, hpenWhite );
            MoveToEx( hdc, rc2.left + 1, rc2.bottom - 1, NULL );
            LineTo( hdc, rc2.right - 1, rc2.bottom - 1 );
            LineTo( hdc, rc2.right - 1, rc2.top );
            OffsetRect( &rc2, 1, 1 );
            }
         else
            {
            SelectPen( hdc, hpenWhite );
            MoveToEx( hdc, rc2.left, rc2.bottom - 1, NULL );
            LineTo( hdc, rc2.left, rc2.top );
            LineTo( hdc, rc2.right, rc2.top );
            SelectPen( hdc, hpenDkGray );
            MoveToEx( hdc, rc2.left + 1, rc2.bottom - 1, NULL );
            LineTo( hdc, rc2.right - 1, rc2.bottom - 1 );
            LineTo( hdc, rc2.right - 1, rc2.top );
            }
   
         /* Draw the text in the header item.
          */
         InflateRect( &rc2, -1, -1 );
         DrawHeaderText( hwnd, hdc, pPart, &rc2, c );
         if( c < pHeader->cParts - 1 )
            {
            SelectPen( hdc, hpenBlack );
            MoveToEx( hdc, pPart->rcItem.right - 1, rc.top, NULL );
            LineTo( hdc, pPart->rcItem.right - 1, rc.bottom );
            }
         }
      pPart = pPart->pNextPart;
      }

   /* Black line across base.
    */
   SelectPen( hdc, hpenBlack );
   MoveToEx( hdc, 0, rc.bottom - 1, NULL );
   LineTo( hdc, rc.right, rc.bottom - 1 );

   /* Clean up afterwards.
    */
   SelectPen( hdc, hpenOld );
   DeletePen( hpenWhite );
   DeletePen( hpenDkGray );
}

/* This function draws the text inside one header item. On entry, prc is expected to
 * hold the rectangle in the text can be drawn (ie. it must not overlap the borders).
 */
static void DrawHeaderText( HWND hwnd, HDC hdc, PART FAR * pPart, RECT * prc, int index )
{
   UINT fmt;

   ASSERT( pPart != NULL );
   ASSERT( prc != NULL );
   fmt = pPart->HdItem.fmt & ~HDF_JUSTIFYMASK;
   if( HDF_OWNERDRAW & fmt  )
      {
      DRAWITEMSTRUCT dis;
   
      dis.CtlType = ODT_HEADER;
      dis.CtlID = GetDlgCtrlID( hwnd );
      dis.itemID = index;
      dis.itemAction = ODA_DRAWENTIRE;
      dis.itemState = pPart->fSelected ? ODS_SELECTED : 0;
      dis.hwndItem = hwnd;
      dis.hDC = hdc;
      dis.rcItem = *prc;
      dis.itemData = pPart->HdItem.lParam;
      SendMessage( GetParent( hwnd ), WM_DRAWITEM, dis.CtlID, (LPARAM)(LPSTR)&dis );
      }
   else if( HDF_STRING == fmt )
      {
      WORD wMode;
      RECT rc;
      int mode;
   
      rc = *prc;
      InflateRect( &rc, -2, 0 );
      mode = SetBkMode( hdc, TRANSPARENT );
      wMode = DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX;
      if( ( pPart->HdItem.fmt & HDF_JUSTIFYMASK ) == HDF_LEFT )
         wMode |= DT_LEFT;
      else if( ( pPart->HdItem.fmt & HDF_JUSTIFYMASK ) == HDF_CENTER )
         wMode |= DT_CENTER;
      else
         wMode |= DT_RIGHT;
      DrawText( hdc, pPart->HdItem.pszText, -1, &rc, wMode );
      SetBkMode( hdc, mode );
      }
}
