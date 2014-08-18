/* TAB.C - Implements the tabbed dialog control
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
#include <memory.h>
#include "amctrls.h"
#include "amctrli.h"
#include "kludge.h"

#ifndef COLOR_3DLIGHT
#define COLOR_3DLIGHT           22
#endif

/* Defines one tab
 */
typedef struct tagTAB {
   struct tagTAB FAR * pNextTab; /* Handle of next tab */
   struct tagTAB FAR * pPrevTab; /* Handle of previous tab */
   RECT rcItem;                  /* Bounding rectangle for item */
   RECT rcPrev;                  /* Temporary storage for rcItem */
   int iWidth;                   /* Width of text in this tab */
   int iImage;                   /* Index of icon in image list */
   int mask;                     /* Type of part */
   BOOL fFocus;                  /* TRUE if this part has the focus */
   BOOL fEnabled;                /* TRUE if this tab is enabled */
   BOOL fHilited;                /* TRUE if this tab is highlighted */
   LPSTR pText;                  /* Text of part */
} TAB;

/* Defines the header of the tab control
 */
typedef struct tagTABHDR {
   int nCurSel;                  /* Index of part with focus */
   int cTabs;                    /* Number of parts */
   int cxSize;                   /* Width of each tab */
   int cySize;                   /* Height of tabs */
   int cRows;                    /* Number of rows */
   int cExtra;                   /* Extra bytes in each tab record */
   int nFirstVisibleTab;         /* Index of first visible tab */
   int cxPadding;                /* Horizontal padding */
   int cyPadding;                /* Vertical padding */
   BOOL fRedraw;                 /* Whether or not redrawing is enabled */
   HFONT hFont;                  /* Font used for the labels */
   HWND hwndScroll;              /* Handle of scroller window */
   TAB FAR * pFirstTab;          /* Handle of first tab */
   TAB FAR * pLastTab;           /* Handle of last tab */
   TAB FAR * pHilitedTab;        /* Handle of highlighted tab */
} TABHDR;

#define  GWW_EXTRA            4
#define  GetBlock(h)          ((TABHDR FAR *)GetWindowLong((h),0))
#define  PutBlock(h,b)        (SetWindowLong((h),0,(LONG)(b)))

struct
   {
   BITMAPINFOHEADER bmiHeader;
   RGBQUAD bmiColors[ 16 ];
} bmi;

static SIZE sizeScroll;       /* Dimensions of the scroller control */
static COLORREF rgbGrey;      /* Grey colour */
static HBITMAP hbmpDither;    /* Dither bitmap */

void FASTCALL HighlightTab( HWND, TABHDR FAR *, TAB FAR * );
void FASTCALL DrawTabBar( HWND, HDC, TABHDR FAR * );
void FASTCALL DrawTabText( HWND, HDC, TAB FAR *, RECT *, int, BOOL, HFONT );
TAB FAR * FASTCALL GetPart( TABHDR FAR *, int );
void FASTCALL SetTabItems( TABHDR FAR *, TAB FAR *, TC_ITEM FAR * );
void FASTCALL ComputeTabRect( HWND, TABHDR FAR *, TAB FAR * );
void FASTCALL SetFixedWidthTabSize( HWND, TABHDR FAR * );
void FASTCALL SetFirstTab( HWND, TABHDR FAR *, int );
void FASTCALL NormaliseMultilineTabs( HWND, TABHDR FAR *, int );
void FASTCALL NormaliseSinglelineTabs( HWND, TABHDR FAR *, int );
void FASTCALL ScrollIntoView( HWND, TABHDR FAR *, int );
BOOL FASTCALL SetCurSel( HWND, TABHDR FAR *, int );
BOOL FASTCALL IsTabVisible( TABHDR FAR *, int, BOOL );
BOOL FASTCALL CanScroll( TABHDR FAR * );
void FASTCALL InvalidateTab( HWND, TAB FAR *, BOOL );
void FASTCALL DrawTabButton( HDC, RECT *, HPEN, HPEN, HPEN, HPEN, HBRUSH, BOOL );

LRESULT EXPORT CALLBACK TabBarWndFn( HWND, UINT, WPARAM, LPARAM );

/* This function registers the tab window class.
 */
BOOL FASTCALL RegisterTabClass( HINSTANCE hInst )
{
   WNDCLASS wc;

   wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
   wc.hIcon          = NULL;
   wc.lpszMenuName   = NULL;
   wc.lpszClassName  = WC_TABCONTROL;
   wc.hbrBackground  = (HBRUSH)( COLOR_BTNFACE + 1 );
   wc.hInstance      = hInst;
   wc.style          = CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS;
   wc.lpfnWndProc    = TabBarWndFn;
   wc.cbClsExtra     = 0;
   wc.cbWndExtra     = GWW_EXTRA;
   return( RegisterClass( &wc ) );
}

LRESULT EXPORT CALLBACK TabBarWndFn( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   TABHDR FAR * pTabHdr;
   TAB FAR * pTab;

#ifdef _DEBUG
   /* Help us to catch NULL pointers...
    */
   pTabHdr = NULL;
   pTab = NULL;
#endif
   switch( message )
      {
      case WM_CREATE: {
         static TABHDR FAR * pNewTabHdr;

         INITIALISE_PTR(pNewTabHdr);
         if( fNewMemory( &pNewTabHdr, sizeof( TABHDR ) ) )
            {
            LPCREATESTRUCT lpcs;

            lpcs = (LPCREATESTRUCT)lParam;
            pNewTabHdr->nCurSel = -1;
            pNewTabHdr->cTabs = 0;
            if( lpcs->style & TCS_OWNERDRAWFIXED )
               {
               MEASUREITEMSTRUCT ms;

               ms.itemID = (UINT)lpcs->hMenu;
               ms.itemWidth = 0;
               ms.itemHeight = 0;
               SendMessage( GetParent( hwnd ), WM_MEASUREITEM, ms.itemID, (LPARAM)(LPSTR)&ms );
               pNewTabHdr->cxSize = ms.itemWidth;
               pNewTabHdr->cySize = ms.itemHeight;
               }
            else
               {
               pNewTabHdr->cxSize = 0;
               pNewTabHdr->cySize = 0;
               }

            /* For single line items, create an invisible horizontal
             * up/down control for scrolling the tabs.
             */
            if( !( lpcs->style & TCS_MULTILINE ) )
               {
               RECT rc;

               pNewTabHdr->hwndScroll = Amctl_CreateUpDownControl( UDS_HORZ, 0, 0, 42, 20, hwnd, 0, hLibInst, NULL, 0, 0, 0 );
               GetClientRect( pNewTabHdr->hwndScroll, &rc );
               sizeScroll.cx = rc.right - rc.left;
               sizeScroll.cy = rc.bottom - rc.top;
               }
            else
               pNewTabHdr->hwndScroll = NULL;
            pNewTabHdr->cRows = 1;
            pNewTabHdr->cExtra = 4;
            pNewTabHdr->cxPadding = 6;
            pNewTabHdr->cyPadding = 4;
            pNewTabHdr->nFirstVisibleTab = 0;
            pNewTabHdr->fRedraw = TRUE;
            pNewTabHdr->hFont = GetStockFont( SYSTEM_FONT );
            pNewTabHdr->pFirstTab = NULL;
            pNewTabHdr->pLastTab = NULL;
            pNewTabHdr->pHilitedTab = NULL;
            PutBlock( hwnd, pNewTabHdr );

            /* If we don't have a proper 'grey' for disabled items, or the grey
             * is the same as the button face, then use the button shadow colour
             * as the grey.
             */
            if( ( rgbGrey = GetSysColor( COLOR_GRAYTEXT ) ) == 0L || rgbGrey == GetSysColor( COLOR_BTNFACE ) )
               rgbGrey = GetSysColor( COLOR_BTNSHADOW );

            /* Create a dithered bitmap for painting disabled
             * button tabs.
             */
            hbmpDither = CreateDitherBitmap();
            }
         return( pNewTabHdr ? 0 : -1 );
         }

      case WM_SETFONT: {
         register int c;

         pTabHdr = GetBlock( hwnd );
         pTabHdr->hFont = wParam ? (HFONT)wParam : GetStockFont( SYSTEM_FONT );
         pTab = pTabHdr->pFirstTab;
         for( c = 0; c < pTabHdr->cTabs; ++c )
            {
            ComputeTabRect( hwnd, pTabHdr, pTab );
            pTab = pTab->pNextTab;
            }
         if( pTabHdr->cTabs )
            SetFirstTab( hwnd, pTabHdr, pTabHdr->nFirstVisibleTab );
         if( LOWORD( lParam ) )
            UpdateWindow( hwnd );
         return( 0L );
         }

      case WM_ERASEBKGND:
         return( TRUE );

      case WM_SETREDRAW:
         pTabHdr = GetBlock( hwnd );
         pTabHdr->fRedraw = wParam;
         return( 0 );

      case WM_NCHITTEST: {
         POINT pt;
         RECT rc;

         GetClientRect( hwnd, &rc );
         pTabHdr = GetBlock( hwnd );
         rc.top += ( pTabHdr->cySize * pTabHdr->cRows ) + 2;
         rc.left += 4;
         rc.bottom -= 4;
         rc.right -= 4;
         pt.x = LOWORD( lParam );
         pt.y = HIWORD( lParam );
         ScreenToClient( hwnd, &pt );
         if( PtInRect( &rc, pt ) )
            return( HTTRANSPARENT );
         break;
         }

      case WM_GETFONT:
         pTabHdr = GetBlock( hwnd );
         return( (LRESULT)(LPSTR)pTabHdr->hFont );

      case TCM_GETROWCOUNT:
         pTabHdr = GetBlock( hwnd );
         return( pTabHdr->cRows );

      case TCM_GETITEMCOUNT:
         pTabHdr = GetBlock( hwnd );
         return( pTabHdr->cTabs );

      case TCM_GETITEM:
         pTabHdr = GetBlock( hwnd );
         if( (int)wParam < pTabHdr->cTabs )
            {
            TC_ITEM FAR * lpTcItem;

            pTab = GetPart( pTabHdr, wParam );
            lpTcItem = (TC_ITEM FAR *)lParam;
            lpTcItem->mask = pTab->mask;
            if( pTab->mask & TCIF_TEXT )
               {
               if( lpTcItem->cchTextMax > 0 )
                  {
                  lstrcpyn( lpTcItem->pszText, pTab->pText, lpTcItem->cchTextMax );
                  lpTcItem->pszText[ lpTcItem->cchTextMax - 1 ] = '\0';
                  }
               }
            if( pTab->mask & TCIF_IMAGE )
               lpTcItem->iImage = pTab->iImage;
            _fmemcpy( &lpTcItem->lParam, (LPSTR)pTab + sizeof( TAB ), pTabHdr->cExtra );
            }
         return( TRUE );

      case TCM_SETCURSEL: {
         int nLastFocus;

         pTabHdr = GetBlock( hwnd );
         nLastFocus = pTabHdr->nCurSel;
         if( SetCurSel( hwnd, pTabHdr, wParam ) )
            if( pTabHdr->fRedraw )
               UpdateWindow( hwnd );
         return( nLastFocus );
         }

      case TCM_GETENABLE:
         pTabHdr = GetBlock( hwnd );
         if( (int)wParam < pTabHdr->cTabs )
            {
            pTab = GetPart( pTabHdr, wParam );
            return( pTab->fEnabled );
            }
         return( -1 );

      case TCM_ENABLETAB:
         pTabHdr = GetBlock( hwnd );
         if( (int)wParam < pTabHdr->cTabs )
            {
            pTab = GetPart( pTabHdr, wParam );
            if( lParam && !pTab->fEnabled )
               {
               pTab->fEnabled = TRUE;
               InvalidateTab( hwnd, pTab, TRUE );
               }
            else if( !lParam && pTab->fEnabled )
               {
               pTab->fEnabled = FALSE;
               InvalidateTab( hwnd, pTab, TRUE );
               }
            UpdateWindow( hwnd );
            }
         return( TRUE );

      case TCM_INSERTITEM: {
         static TAB FAR * pNewPart;

         INITIALISE_PTR(pNewPart);
         pTabHdr = GetBlock( hwnd );
         if( (int)wParam >= pTabHdr->cTabs )
            wParam = pTabHdr->cTabs;
         pTab = GetPart( pTabHdr, wParam );
         if( fNewMemory( &pNewPart, sizeof( TAB ) + pTabHdr->cExtra ) )
            {
            if( pTab )
               {
               pNewPart->pPrevTab = pTab->pPrevTab;
               if( pTab->pPrevTab )
                  pTab->pPrevTab->pNextTab = pNewPart;
               pTab->pPrevTab = pNewPart;
               }
            else
               {
               if( pTabHdr->pLastTab )
                  pTabHdr->pLastTab->pNextTab = pNewPart;
               pNewPart->pPrevTab = pTabHdr->pLastTab;
               }
            pNewPart->pNextTab = pTab;
            if( pNewPart->pPrevTab == NULL )
               pTabHdr->pFirstTab = pNewPart;
            if( pNewPart->pNextTab == NULL )
               pTabHdr->pLastTab = pNewPart;
            pNewPart->fFocus = FALSE;
            pNewPart->fHilited = FALSE;
            if( pTabHdr->cTabs++ == 0 )
               {
               pTabHdr->nCurSel = 0;
               pNewPart->fFocus = TRUE;
               }
            if( pTabHdr->hwndScroll )
               SendMessage( pTabHdr->hwndScroll, UDM_SETRANGE, 0, MAKELPARAM( 0, pTabHdr->cTabs - 1 ) );
            pNewPart->pText = NULL;
            pNewPart->fEnabled = TRUE;
            SetTabItems( pTabHdr, pNewPart, (TC_ITEM FAR *)lParam );
            ComputeTabRect( hwnd, pTabHdr, pNewPart );
            SetFirstTab( hwnd, pTabHdr, pTabHdr->nFirstVisibleTab );
            if( pTabHdr->fRedraw )
               {
               HDC hdc;

               hdc = GetDC( hwnd );
               DrawTabBar( hwnd, hdc, pTabHdr );
               ReleaseDC( hwnd, hdc );
               }
            }
         return( pNewPart ? wParam : -1 );
         }

      case TCM_ADJUSTRECT: {
         LPRECT lprc;

         pTabHdr = GetBlock( hwnd );
         lprc = (LPRECT)lParam;
         if( wParam )
            {
            lprc->top -= ( pTabHdr->cySize * pTabHdr->cRows ) + 2;
            lprc->left -= 4;
            lprc->bottom += 4;
            lprc->right += 4;
            }
         else
            {
            lprc->top += ( pTabHdr->cySize * pTabHdr->cRows ) + 2;
            lprc->left += 4;
            lprc->bottom -= 4;
            lprc->right -= 4;
            }
         break;
         }

      case TCM_GETITEMRECT:
         pTabHdr = GetBlock( hwnd );
         if( pTab = GetPart( pTabHdr, wParam ) )
            {
            CopyRect( (LPRECT)lParam, &pTab->rcItem );
            return( TRUE );
            }
         return( FALSE );

      case TCM_HITTEST: {
         LPTC_HITTESTINFO lpHitInfo;
         register int c;

         lpHitInfo = (LPTC_HITTESTINFO)lParam;
         pTabHdr = GetBlock( hwnd );
         pTab = pTabHdr->pFirstTab;
         lpHitInfo->flags = TCHT_NOWHERE;
         for( c = 0; c < pTabHdr->cTabs; ++c )
            if( PtInRect( &pTab->rcItem, lpHitInfo->pt ) )
               {
               if( pTab->fEnabled )
                  lpHitInfo->flags = TCHT_ONITEM;
               break;
               }
         break;
         }

      case TCM_SETITEM:
         pTabHdr = GetBlock( hwnd );
         if( (int)wParam < pTabHdr->cTabs )
            {
            pTab = GetPart( pTabHdr, wParam );
            SetTabItems( pTabHdr, pTab, (TC_ITEM FAR *)lParam );
            ComputeTabRect( hwnd, pTabHdr, pTab );
            SetFirstTab( hwnd, pTabHdr, pTabHdr->nFirstVisibleTab );
            }
         return( TRUE );

      case TCM_SETITEMEXTRA:
         pTabHdr = GetBlock( hwnd );
         pTabHdr->cExtra = wParam;
         return( TRUE );

      case TCM_SETITEMSIZE: {
         LONG lRetCode = 0L;

         pTabHdr = GetBlock( hwnd );
         lRetCode = MAKELONG( pTabHdr->cxSize, pTabHdr->cySize );
         pTabHdr->cxSize = LOWORD( lParam );
         pTabHdr->cySize = HIWORD( lParam );
         if( pTabHdr->fRedraw )
            {
            HDC hdc;

            hdc = GetDC( hwnd );
            DrawTabBar( hwnd, hdc, pTabHdr );
            ReleaseDC( hwnd, hdc );
            }
         return( lRetCode );
         }

      case TCM_SETPADDING:
         pTabHdr = GetBlock( hwnd );
         pTabHdr->cxPadding = LOWORD( lParam );
         pTabHdr->cyPadding = HIWORD( lParam );
         if( pTabHdr->fRedraw )
            {
            HDC hdc;

            hdc = GetDC( hwnd );
            DrawTabBar( hwnd, hdc, pTabHdr );
            ReleaseDC( hwnd, hdc );
            }
         return( 0L );

      case TCM_DELETEALLITEMS:
         pTabHdr = GetBlock( hwnd );
         pTab = pTabHdr->pFirstTab;
         while( pTab )
            {
            TAB FAR * pNextTab;

            pNextTab = pTab->pNextTab;
            if( pTab->pText && !( GetWindowStyle( hwnd ) & TCS_OWNERDRAWFIXED ) )
               FreeMemory( &pTab->pText );
            pTab = pNextTab;
            }
         pTabHdr->pHilitedTab = NULL;
         pTabHdr->pFirstTab = NULL;
         pTabHdr->pLastTab = NULL;
         pTabHdr->nCurSel = 0;
         pTabHdr->cTabs = 0;
         pTabHdr->cxSize = 0;
         pTabHdr->cRows = 0;
         if( pTabHdr->fRedraw )
            {
            HDC hdc;

            hdc = GetDC( hwnd );
            DrawTabBar( hwnd, hdc, pTabHdr );
            ReleaseDC( hwnd, hdc );
            }
         return( TRUE );

      case TCM_DELETEITEM: {
         pTabHdr = GetBlock( hwnd );
         if( (int)wParam < pTabHdr->cTabs )
            {
            int nNextTab;

            pTab = GetPart( pTabHdr, wParam );
            nNextTab = wParam;
            if( nNextTab == pTabHdr->nFirstVisibleTab )
               {
               if( pTab->pPrevTab )
                  --nNextTab;
               else
                  ++nNextTab;
               }
            if( pTab->pPrevTab != NULL )
               pTab->pPrevTab->pNextTab = pTab->pNextTab;
            else
               pTabHdr->pFirstTab = pTab->pNextTab;
            if( pTab->pNextTab != NULL )
               pTab->pNextTab->pPrevTab = pTab->pPrevTab;
            else
               pTabHdr->pLastTab = pTab->pPrevTab;
            --pTabHdr->cTabs;
            if( pTabHdr->hwndScroll )
               SendMessage( pTabHdr->hwndScroll, UDM_SETRANGE, 0, MAKELPARAM( 0, pTabHdr->cTabs - 1 ) );
            if( pTab == pTabHdr->pHilitedTab )
               pTabHdr->pHilitedTab = NULL;
            FreeMemory( &pTab );
            SetFirstTab( hwnd, pTabHdr, nNextTab );
            if( pTabHdr->fRedraw )
               {
               HDC hdc;

               hdc = GetDC( hwnd );
               DrawTabBar( hwnd, hdc, pTabHdr );
               ReleaseDC( hwnd, hdc );
               }
            }
         return( TRUE );
         }

      case WM_GETDLGCODE:
         return( DLGC_WANTARROWS|DLGC_WANTCHARS );

      case TCM_GETCURSEL:
         pTabHdr = GetBlock( hwnd );
         return( pTabHdr->nCurSel );

      case WM_CONTEXTMENU: /*!!SM!!*/
      case WM_RBUTTONDOWN: {
         NMHDR nmh;

         Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_RCLICK, (LPNMHDR)&nmh );
         break;
         }

      case WM_NOTIFY: {
         LPNM_UPDOWN lpNmHdr;
         BOOL fRepaint;

         pTabHdr = GetBlock( hwnd );
         lpNmHdr = (LPNM_UPDOWN)lParam;
         fRepaint = FALSE;
         switch( lpNmHdr->hdr.code )
            {
            case UDN_DELTAPOS:
               if( lpNmHdr->iDelta < 0 )
                  {
                  if( IsTabVisible( pTabHdr, 0, TRUE ) )
                     break;
                  SetFirstTab( hwnd, pTabHdr, pTabHdr->nFirstVisibleTab - 1 );
                  fRepaint = TRUE;
                  break;
                  }
               else
                  {
                  if( IsTabVisible( pTabHdr, pTabHdr->cTabs - 1, TRUE ) )
                     break;
                  SetFirstTab( hwnd, pTabHdr, pTabHdr->nFirstVisibleTab + 1 );
                  fRepaint = TRUE;
                  break;
                  }
               break;
            }
         if( fRepaint )
            {
            RECT rc;

            GetClientRect( hwnd, &rc );
            rc.bottom = pTabHdr->cySize + 2;
            rc.right -= sizeScroll.cx;
            InvalidateRect( hwnd, &rc, TRUE );
            UpdateWindow( hwnd );
            }
         break;
         }

      case WM_SIZE: {
         pTabHdr = GetBlock( hwnd );
         if( pTabHdr->cTabs )
            {
            SetFirstTab( hwnd, pTabHdr, 0 );
            SetFirstTab( hwnd, pTabHdr, pTabHdr->nFirstVisibleTab );
            SetCurSel( hwnd, pTabHdr, pTabHdr->nCurSel );
            }
         if( pTabHdr->hwndScroll )
            {
            POINT pt;

            pt.x = LOWORD( lParam );
            pt.y = HIWORD( lParam );
            SetWindowPos( pTabHdr->hwndScroll, NULL, pt.x - sizeScroll.cx, 0, 0, 0, SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE );
            }
         if( pTabHdr->fRedraw )
            UpdateWindow( hwnd );
         break;
         }

      case WM_LBUTTONDOWN: {
         register int c;
         POINT pt;

         pTabHdr = GetBlock( hwnd );
         pt.x = LOWORD( lParam );
         pt.y = HIWORD( lParam );
         pTab = pTabHdr->pFirstTab;
         for( c = 0; c < pTabHdr->cTabs; ++c )
            {
            if( PtInRect( &pTab->rcItem, pt ) )
               {
               if( pTab->fEnabled )
                  {
                  if( SetCurSel( hwnd, pTabHdr, c ) )
                     UpdateWindow( hwnd );
                  if( ( GetWindowStyle( hwnd ) & TCS_FOCUSONBUTTONDOWN ) && GetFocus() != hwnd )
                     SetFocus( hwnd );
                  }
               break;
               }
            pTab = pTab->pNextTab;
            }
         break;
         }

      case WM_MOUSEMOVE:
         if( GetWindowStyle( hwnd ) & TCS_HOTTRACK )
            {
            register int c;
            POINT pt;

            /* This function tracks the mouse cursor and
             * highlights the text of each tab item over which
             * it moves
             */
            pTabHdr = GetBlock( hwnd );
            pt.x = LOWORD( lParam );
            pt.y = HIWORD( lParam );
            pTab = pTabHdr->pFirstTab;
            for( c = 0; c < pTabHdr->cTabs; ++c )
               {
               if( PtInRect( &pTab->rcItem, pt ) )
                  {
                  if( !pTab->fHilited )
                     HighlightTab( hwnd, pTabHdr, pTab );
                  break;
                  }
               pTab = pTab->pNextTab;
               }
            if( NULL == pTab && NULL != pTabHdr->pHilitedTab )
               HighlightTab( hwnd, pTabHdr, NULL );
            }
         break;

      case WM_KEYDOWN: {
         TC_KEYDOWN tck;

         tck.wVKey = wParam;
         tck.flags = LOWORD( lParam );
         Amctl_SendNotify( GetParent( hwnd ), hwnd, TCN_KEYDOWN, (LPNMHDR)&tck );

         pTabHdr = GetBlock( hwnd );
         switch( wParam )
            {
            case VK_LEFT:
               if( pTabHdr->nCurSel > 0 )
                  {
                  int nCurSel;

                  nCurSel = pTabHdr->nCurSel;
                  do {
                     --nCurSel;
                     pTab = GetPart( pTabHdr, nCurSel );
                     if( pTab->fEnabled )
                        {
                        if( SetCurSel( hwnd, pTabHdr, nCurSel ) )
                           UpdateWindow( hwnd );
                        break;
                        }
                     }
                  while( nCurSel > 0 );
                  SetFocus( hwnd );
                  }
               break;

            case VK_RIGHT:
               if( pTabHdr->nCurSel < pTabHdr->cTabs - 1 )
                  {
                  int nCurSel;

                  nCurSel = pTabHdr->nCurSel;
                  do {
                     ++nCurSel;
                     pTab = GetPart( pTabHdr, nCurSel );
                     if( pTab->fEnabled )
                        {
                        if( SetCurSel( hwnd, pTabHdr, nCurSel ) )
                           UpdateWindow( hwnd );
                        break;
                        }
                     }
                  while( nCurSel < pTabHdr->cTabs - 1 );
                  SetFocus( hwnd );
                  }
               break;
            }
         break;
         }

      case WM_DESTROY:
         pTabHdr = GetBlock( hwnd );
         pTab = pTabHdr->pFirstTab;
         while( pTab )
            {
            TAB FAR * pNextTab;

            pNextTab = pTab->pNextTab;
            if( pTab->pText && !( GetWindowStyle( hwnd ) & TCS_OWNERDRAWFIXED ) )
               FreeMemory( &pTab->pText );
            FreeMemory( &pTab );
            pTab = pNextTab;
            }
         if( pTabHdr->hwndScroll )
            DestroyWindow( pTabHdr->hwndScroll );
         DeleteBitmap( hbmpDither );
         FreeMemory( &pTabHdr );
         break;

      case WM_SETFOCUS:
         pTabHdr = GetBlock( hwnd );
         pTab = GetPart( pTabHdr, pTabHdr->nCurSel );
         pTab->fFocus = TRUE;
         if( pTab->fHilited )
            SetCapture( hwnd );
         InvalidateTab( hwnd, pTab, TRUE );
         break;

      case WM_KILLFOCUS:
         pTabHdr = GetBlock( hwnd );
         pTab = GetPart( pTabHdr, pTabHdr->nCurSel );
         pTab->fFocus = FALSE;
         if( pTab->fHilited )
            ReleaseCapture();
         InvalidateTab( hwnd, pTab, TRUE );
         break;

      case WM_PAINT: {
         PAINTSTRUCT ps;
         HDC hdc;

         pTabHdr = GetBlock( hwnd );
         hdc = BeginPaint( hwnd, (LPPAINTSTRUCT)&ps );
         DrawTabBar( hwnd, hdc, pTabHdr );
         EndPaint( hwnd, (LPPAINTSTRUCT)&ps );
         break;
         }
      }
   return( DefWindowProc( hwnd, message, wParam, lParam ) );
}

/* Given the index of a tab, this function returns the handle of the tab
 * specified by that index, or NULL if the index is out of range.
 */
TAB FAR * FASTCALL GetPart( TABHDR FAR * pTabHdr, int index )
{
   TAB FAR * pTab;

   pTab = pTabHdr->pFirstTab;
   while( pTab && index-- )
      pTab = pTab->pNextTab;
   return( pTab );
}

void FASTCALL SetTabItems( TABHDR FAR * pTabHdr, TAB FAR * pTab, TC_ITEM FAR * lpTcItem )
{
   pTab->mask = lpTcItem->mask;
   if( pTab->mask & TCIF_TEXT )
      {
      static LPSTR lpszText;

      if( pTab->pText )
         FreeMemory( &pTab->pText );
      INITIALISE_PTR(lpszText);
      if( fNewMemory( &lpszText, lstrlen( lpTcItem->pszText ) + 1 ) )
         {
         pTab->pText = lpszText;
         lstrcpy( pTab->pText, lpTcItem->pszText );
         }
      }
   if( pTab->mask & TCIF_IMAGE )
      pTab->iImage = lpTcItem->iImage;
   _fmemcpy( (LPSTR)pTab + sizeof( TAB ), (LPSTR)&lpTcItem->lParam, pTabHdr->cExtra );
}

void FASTCALL ComputeTabRect( HWND hwnd, TABHDR FAR * pTabHdr, TAB FAR * pTab )
{
   int iWidth;
   int iHeight;
   HFONT hOldFont;
   HFONT hBoldFont;
   SIZE size;
   HDC hdc;

   /* If we're not running Windows 95, then the selected tabs will
    * show text in a boldface. So use the boldface version of the
    * font for computing the tab rectangles since it will be slightly
    * wider than a non-bold face.
    */
   hdc = GetDC( hwnd );
   if( !fNewShell )
      {
      LOGFONT lf;

      GetObject( pTabHdr->hFont, sizeof( LOGFONT ), &lf );
      lf.lfWeight = FW_BOLD;
      hBoldFont = CreateFontIndirect( &lf );
      hOldFont = SelectFont( hdc, hBoldFont );
      }
   else
      {
      hBoldFont = NULL;
      hOldFont = SelectFont( hdc, pTabHdr->hFont );
      }

   /* For owner-draw fixed tabs, the tab width is determined by the
    * values returned in response to WM_MEASUREITEM which was sent before
    * any tabs were added. For non owner-draw tabs, the tab width is
    * the width of the text.
    */
   iWidth = 0;
   iHeight = 0;
   if( GetWindowStyle( hwnd ) & TCS_OWNERDRAWFIXED )
      {
      iWidth += pTabHdr->cxSize;
      iHeight += pTabHdr->cySize;
      }
   else if( pTab->mask & TCIF_TEXT )
      {
      GetTextExtentPoint( hdc, pTab->pText, lstrlen( pTab->pText ), &size );
      iWidth += size.cx;
      iHeight += size.cy;
      }

   /* For both owner-draw and non-owner-draw tabs, add some extra for
    * the border.
    *
    * Are we doing this right? Should the owner-draw tab already include the
    * border in the dimensions returned by the caller?
    */
   iWidth += pTabHdr->cxPadding * 2;
   iHeight += pTabHdr->cyPadding * 2;

   /* If we're using fixed width tabs and the width of this tab is wider than
    * the longest tab so far (cached in pTabHdr->cxSize), then set the width
    * of all tabs to this new width and cache the new width.
    */
   if( GetWindowStyle( hwnd ) & TCS_FIXEDWIDTH )
      {
      if( iWidth > pTabHdr->cxSize )
         {
         pTabHdr->cxSize = iWidth;
         SetFixedWidthTabSize( hwnd, pTabHdr );
         }
      else
         pTab->iWidth = pTabHdr->cxSize;
      }
   else
      pTab->iWidth = iWidth;

   /* Update the tab height if this tab is taller than others.
    */
   if( iHeight > pTabHdr->cySize )
      pTabHdr->cySize = iHeight;

   /* Clean up before we leave.
    */
   SelectFont( hdc, hOldFont );
   if( hBoldFont )
      DeleteFont( hBoldFont );
   ReleaseDC( hwnd, hdc );
}

/* This function loops through each tab and sets the tab width to the
 * width defined in pTabHdr->cxSize. It is called from ComputeTabRect when
 * we update the width of fixed width tabs.
 */
void FASTCALL SetFixedWidthTabSize( HWND hwnd, TABHDR FAR * pTabHdr )
{
   TAB FAR * pTab;

   pTab = pTabHdr->pFirstTab;
   while( pTab )
      {
      pTab->iWidth = pTabHdr->cxSize;
      pTab = pTab->pNextTab;
      }
}

/* This function attempts to adjust the tabs so that the tab selected by nTab is
 * the selected tab, and that it is visible. For multiline tabs, this means shuffling
 * the rows so that the selected tab is on the bottom row. For singleline tabs, this
 * means scrolling the tabs so that the selected tab is fully visible.
 */
void FASTCALL SetFirstTab( HWND hwnd, TABHDR FAR * pTabHdr, int nTab )
{
   if( GetWindowStyle( hwnd ) & TCS_MULTILINE )
      NormaliseMultilineTabs( hwnd, pTabHdr, nTab );
   else
      NormaliseSinglelineTabs( hwnd, pTabHdr, nTab );
   pTabHdr->nFirstVisibleTab = nTab;
}

void FASTCALL NormaliseMultilineTabs( HWND hwnd, TABHDR FAR * pTabHdr, int nTab )
{
   BOOL fMultiRow;
   TAB FAR * pTab;
   RECT rc;
   int nHeight;
   int nBaseOffset;
   register int c;
   int x, y;

   GetClientRect( hwnd, &rc );
   pTabHdr->cRows = 1;

   fMultiRow = FALSE;
   x = 2;
   y = 0;
   pTab = pTabHdr->pFirstTab;
   for( c = 0; c < pTabHdr->cTabs; ++c )
      {
      if( x + pTab->iWidth > rc.right - 2 )
         {
         y += pTabHdr->cySize;
         x = 2;
         ++pTabHdr->cRows;
         fMultiRow = TRUE;
         }
      pTab->rcPrev = pTab->rcItem;
      pTab->rcItem.left = x;
      pTab->rcItem.top = y + 2;
      pTab->rcItem.right = x + pTab->iWidth;
      pTab->rcItem.bottom = y + pTabHdr->cySize;
      x = pTab->rcItem.right;
      pTab = pTab->pNextTab;
      }

   nHeight = y + pTabHdr->cySize;

   pTab = GetPart( pTabHdr, pTabHdr->nCurSel );
   nBaseOffset = nHeight - pTab->rcItem.bottom;
   pTab = pTabHdr->pFirstTab;
   for( c = 0; c < pTabHdr->cTabs; ++c )
      {
      OffsetRect( &pTab->rcItem, 0, nBaseOffset );
      if( pTab->rcItem.bottom > nHeight )
         OffsetRect( &pTab->rcItem, 0, -nHeight );
      pTab = pTab->pNextTab;
      }

   if( fMultiRow )
      {
      pTab = pTabHdr->pFirstTab;
      while( pTab )
         {
         TAB FAR * pRow;
         int nRight;
         int nLastVert;
         int cTabsOnRow;
         int iDiff;

         nLastVert = pTab->rcItem.top;
         cTabsOnRow = 0;
         nRight = 0;
         pRow = pTab;
         while( pTab && pTab->rcItem.top == nLastVert )
            {
            ++cTabsOnRow;
            nRight = pTab->rcItem.right;
            pTab = pTab->pNextTab;
            }
         iDiff = ( rc.right - nRight ) / cTabsOnRow;
         x = 2;
         while( pRow != pTab )
            {
            pRow->rcItem.left = x;
            if( pRow->pNextTab == pTab )
               pRow->rcItem.right = rc.right - 2;
            else
               pRow->rcItem.right = x + pRow->iWidth + iDiff;
            x = pRow->rcItem.right;
            pRow = pRow->pNextTab;
            }
         }
      }
   else if( !( GetWindowStyle( hwnd ) & TCS_RAGGEDRIGHT ) )
      {
      int iDiff;

      pTab = pTabHdr->pFirstTab;
      iDiff = ( rc.right - 2 - x ) / pTabHdr->cTabs;
      x = 2;
      for( c = 0; c < pTabHdr->cTabs; ++c )
         {
         pTab->rcItem.left = x;
         if( pTab->pNextTab )
            pTab->rcItem.right = x + pTab->iWidth + iDiff;
         else
            pTab->rcItem.right = rc.right - 2;
         x = pTab->rcItem.right;
         pTab = pTab->pNextTab;
         }
      }

   pTab = pTabHdr->pFirstTab;
   for( c = 0; c < pTabHdr->cTabs; ++c )
      {
      if( _fmemcmp( &pTab->rcItem, &pTab->rcPrev, sizeof( RECT ) ) )
         InvalidateTab( hwnd, pTab, TRUE );
      pTab = pTab->pNextTab;
      }
}

void FASTCALL NormaliseSinglelineTabs( HWND hwnd, TABHDR FAR * pTabHdr, int nTab )
{
   int cVisibleTabs;
   TAB FAR * pTab;
   RECT rc;
   int c, x;

   pTab = pTabHdr->pFirstTab;
   GetClientRect( hwnd, &rc );
   x = 2;
   pTabHdr->cRows = 1;
   cVisibleTabs = 0;
   for( c = 0; c < pTabHdr->cTabs; ++c )
      {
      if( c < nTab )
         SetRectEmpty( &pTab->rcItem );
      else
         {
         pTab->rcItem.left = x;
         pTab->rcItem.top = 2;
         pTab->rcItem.right = x + pTab->iWidth;
         pTab->rcItem.bottom = pTabHdr->cySize;
         x = pTab->rcItem.right;
         ++cVisibleTabs;
         }
      pTab = pTab->pNextTab;
      }
   pTab = pTabHdr->pLastTab;
   x = rc.right - ( ( sizeScroll.cx / 3 ) + sizeScroll.cx );
   if( pTab->rcItem.right > ( nTab ? x : rc.right ) )
      {
      while( pTab )
         {
         --cVisibleTabs;
         if( pTab->rcItem.left > x )
            SetRectEmpty( &pTab->rcItem );
         else
            {
            if( pTab->rcItem.right > x )
               pTab->rcItem.right = x;
            break;
            }
         pTab = pTab->pPrevTab;
         }
      }
   else if( !( GetWindowStyle( hwnd ) & TCS_RAGGEDRIGHT ) && pTab->rcItem.right < rc.right && cVisibleTabs )
      {
      int iDiff;

      iDiff = ( rc.right - pTab->rcItem.right ) / cVisibleTabs;
      pTab = pTabHdr->pFirstTab;
      x = 2;
      for( c = 0; c < pTabHdr->cTabs; ++c )
         {
         if( !IsRectEmpty( &pTab->rcItem ) )
            {
            pTab->rcItem.left = x;
            pTab->rcItem.right = x + pTab->iWidth + iDiff;
            x = pTab->rcItem.right;
            }
         pTab = pTab->pNextTab;
         }
      }
   if( !IsRectEmpty( &rc ) )
      if( cVisibleTabs != pTabHdr->cTabs )
         {
         if( !IsWindowVisible( pTabHdr->hwndScroll ) )
            ShowWindow( pTabHdr->hwndScroll, SW_SHOW );
         }
      else
         {
         if( IsWindowVisible( pTabHdr->hwndScroll ) )
            ShowWindow( pTabHdr->hwndScroll, SW_HIDE );
         }
   if( pTabHdr->hwndScroll )
      {
      int cRange;

      cRange = pTabHdr->cTabs - cVisibleTabs;
      SendMessage( pTabHdr->hwndScroll, UDM_SETRANGE, 0, MAKELPARAM( 0, cRange ) );
      }
}

BOOL FASTCALL SetCurSel( HWND hwnd, TABHDR FAR * pTabHdr, int nSel )
{
   TAB FAR * pTab;
   NMHDR nmh;

   if( pTabHdr->nCurSel == nSel )
      {
      if( !IsTabVisible( pTabHdr, nSel, TRUE ) )
         ScrollIntoView( hwnd, pTabHdr, nSel );
      return( TRUE );
      }

   if( Amctl_SendNotify( GetParent( hwnd ), hwnd, TCN_SELCHANGING, &nmh ) == 0 )
      {
      pTab = GetPart( pTabHdr, pTabHdr->nCurSel );
      pTab->fFocus = FALSE;
      InvalidateTab( hwnd, pTab, TRUE );

      pTab = GetPart( pTabHdr, nSel );
      pTab->fFocus = TRUE;
      InvalidateTab( hwnd, pTab, TRUE );

      pTabHdr->nCurSel = nSel;

      Amctl_SendNotify( GetParent( hwnd ), hwnd, TCN_SELCHANGE, &nmh );
      ScrollIntoView( hwnd, pTabHdr, nSel );
      if( pTabHdr->hwndScroll )
         SendMessage( pTabHdr->hwndScroll, UDM_SETPOS, 0, MAKELPARAM( pTabHdr->nFirstVisibleTab, 0 ) );
      return( TRUE );
      }
   return( FALSE );
}

/* This function highlights the specified
 * tab.
 */
void FASTCALL HighlightTab( HWND hwnd, TABHDR FAR * pTabHdr, TAB FAR * pTab )
{
   RECT rc;

   if( pTab != pTabHdr->pHilitedTab && pTabHdr->pHilitedTab )
      {
      if( pTabHdr->pHilitedTab->fHilited && NULL == pTab )
         ReleaseCapture();
      pTabHdr->pHilitedTab->fHilited = FALSE;
      rc = pTabHdr->pHilitedTab->rcItem;
      InflateRect( &rc, -2, -2 );
      InvalidateRect( hwnd, &rc, FALSE );
      }
   if( NULL != pTab )
      {
      if( !pTab->fHilited && NULL == pTabHdr->pHilitedTab )
         SetCapture( hwnd );
      pTab->fHilited = TRUE;
      rc = pTab->rcItem;
      InflateRect( &rc, -2, -2 );
      InvalidateRect( hwnd, &rc, FALSE );
      }
   pTabHdr->pHilitedTab = pTab;
}

/* This function invalidates the specified tab so that it is repainted by the next
 * WM_PAINT call. The fInflate flag specifies whether the tab is selected, in which
 * case the tab dimensions are slightly larger.
 */
void FASTCALL InvalidateTab( HWND hwnd, TAB FAR * pTab, BOOL fInflate )
{
   RECT rc;

   rc = pTab->rcItem;
   if( fInflate )
      InflateRect( &rc, 2, 2 );
   InvalidateRect( hwnd, &rc, FALSE );
}

/* This function determines whether or not scroll bars need to be shown by determining
 * whether or not the first or last tabs are completely visible.
 */
BOOL FASTCALL CanScroll( TABHDR FAR * pTabHdr )
{
   return( !IsTabVisible( pTabHdr, 0, FALSE ) || !IsTabVisible( pTabHdr, pTabHdr->cTabs - 1, FALSE ) );
}

/* Determines whether or not a tab is visible. If the fComplete flag is set then
 * we are asking whether the specified tab is completely visible or cut off at the
 * edge. If fComplete is TRUE and the specified tab is truncated then this function
 * returns FALSE, otherwise it returns TRUE.
 */
BOOL FASTCALL IsTabVisible( TABHDR FAR * pTabHdr, int nTab, BOOL fComplete )
{
   TAB FAR * pTab;
   BOOL fResult;

   pTab = GetPart( pTabHdr, nTab );
   if( IsRectEmpty( &pTab->rcItem ) )
      fResult = FALSE;
   else if( ( pTab->rcItem.right - pTab->rcItem.left ) >= pTab->iWidth )
      fResult = TRUE;
   else
      fResult = !fComplete;
   return( fResult );
}

void FASTCALL ScrollIntoView( HWND hwnd, TABHDR FAR * pTabHdr, int nTab )
{
   if( GetWindowStyle( hwnd ) & TCS_MULTILINE )
      NormaliseMultilineTabs( hwnd, pTabHdr, nTab );
   else
      {
      int nIncrement;
      int nFirstVisibleTab;

      nIncrement = ( nTab > pTabHdr->nFirstVisibleTab ) ? 1 : -1;
      nFirstVisibleTab = pTabHdr->nFirstVisibleTab;
      while( !IsTabVisible( pTabHdr, nTab, TRUE ) && nTab != pTabHdr->nFirstVisibleTab )
         {
         SetFirstTab( hwnd, pTabHdr, pTabHdr->nFirstVisibleTab + nIncrement );
         }
      if( pTabHdr->nFirstVisibleTab != nFirstVisibleTab )
         InvalidateRect( hwnd, NULL, TRUE );
      }
}

void FASTCALL DrawTabBar( HWND hwnd, HDC hdc, TABHDR FAR * pTabHdr )
{
   HPEN hpenOld;
   HPEN hpenBlack;
   HPEN hpenWhite;
   HPEN hpenGray;
   HPEN hpenBrGray;
   HPEN hpenLtGray;
   BOOL fLastWasSelected;
   COLORREF crText;
   TAB FAR * pTab;
   register int c;
   register int x;
   int y;
   int n;
   int nLastVert;
   BOOL fButtons;
   BOOL fMultiline;
   HFONT hfontOld;
   HFONT hBoldFont;
   HBRUSH hbrGrey;
   HBRUSH hbrDither;
   RECT rc;

   GetClientRect( hwnd, &rc );
   hpenBlack = GetStockPen( BLACK_PEN );
   hpenWhite = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNHIGHLIGHT ) );
   hpenGray = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNSHADOW ) );
   hpenLtGray = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNFACE ) );
   hpenBrGray = CreatePen( PS_SOLID, 1, GetSysColor( fNewShell ? COLOR_3DLIGHT : COLOR_BTNFACE ) );

   y = rc.bottom;
   x = rc.right;
   rc.bottom = rc.top + ( pTabHdr->cySize * pTabHdr->cRows ) + 2;
   if( CanScroll( pTabHdr ) )
      rc.right -= sizeScroll.cx;
   hbrGrey = CreateSolidBrush( GetSysColor( COLOR_BTNFACE ) );
   hbrDither = CreatePatternBrush( hbmpDither );
   FillRect( hdc, &rc, hbrGrey );
   rc.bottom -= 2;
   rc.right = x;
   hpenOld = NULL;

   fButtons = ( GetWindowStyle( hwnd ) & TCS_BUTTONS ) != 0;
   fMultiline = ( GetWindowStyle( hwnd ) & TCS_MULTILINE ) != 0;
   if( !fButtons )
      {
      hpenOld = SelectPen( hdc, hpenBlack );
      MoveToEx( hdc, rc.right - 1, rc.bottom, NULL );
      LineTo( hdc, rc.right - 1, y - 1 );
      LineTo( hdc, rc.left - 1, y - 1 );

      SelectPen( hdc, hpenWhite );
      MoveToEx( hdc, rc.left, rc.bottom, NULL );
      LineTo( hdc, rc.left, y - 1 );

      SelectPen( hdc, hpenBrGray );
      MoveToEx( hdc, rc.left + 1, rc.bottom + 1, NULL );
      LineTo( hdc, rc.left + 1, y - 2 );

      SelectPen( hdc, hpenGray );
      MoveToEx( hdc, rc.right - 2, rc.bottom + 1, NULL );
      LineTo( hdc, rc.right - 2, y - 2 );
      LineTo( hdc, rc.left, y - 2 );
      }

   crText = SetTextColor( hdc, GetSysColor( COLOR_BTNTEXT ) );
   pTab = pTabHdr->pFirstTab;
   hfontOld = SelectObject( hdc, pTabHdr->hFont );
   if( !fNewShell )
      {
      LOGFONT lf;

      GetObject( pTabHdr->hFont, sizeof( LOGFONT ), &lf );
      lf.lfWeight = FW_BOLD;
      hBoldFont = CreateFontIndirect( &lf );
      }
   else
      hBoldFont = NULL;
   x = rc.left;
   n = 1;
   fLastWasSelected = FALSE;
   nLastVert = 0;
   for( c = 0; c < pTabHdr->cTabs; ++c )
      {
      if( !IsRectEmpty( &pTab->rcItem ) )
         {
         BOOL fSelected;
         BOOL fClipped;
         static RECT rc2;

         rc2 = pTab->rcItem;
         fSelected = c == pTabHdr->nCurSel;
         fClipped = !fMultiline && ( rc2.right - rc2.left ) < pTab->iWidth;
         if( fButtons )
            {
            InflateRect( &rc2, -2, -2 );
            if( fSelected )
               DrawTabButton( hdc, &rc2, hpenBlack, hpenGray, hpenWhite, hpenBrGray, hbrDither, fClipped );
            else
               DrawTabButton( hdc, &rc2, hpenWhite, hpenBrGray, hpenBlack, hpenGray, hbrGrey, fClipped );
            }
         else
            {
            if( rc2.top != nLastVert )
               {
               fLastWasSelected = FALSE;
               x = rc.left;
               }
            nLastVert = rc2.top;
            if( fSelected )
               {
               rc2.left -= 2;
               rc2.top -= 2;
               rc2.right += 2;
               rc2.bottom += 1;
               }
            if( fLastWasSelected )
               {
               SelectPen( hdc, hpenBrGray );
               MoveToEx( hdc, rc2.left + 2, rc2.top + 1, NULL );
               LineTo( hdc, rc2.right - 2, rc2.top + 1 );
               SelectPen( hdc, hpenWhite );
               MoveToEx( hdc, rc2.left + 2, rc2.top, NULL );
               LineTo( hdc, rc2.right - 2, rc2.top );
               }
            else
               {
               SelectPen( hdc, hpenBrGray );
               MoveToEx( hdc, rc2.left + 1, rc2.bottom - 1, NULL );
               LineTo( hdc, rc2.left + 1, rc2.top + 1 );
               MoveToEx( hdc, rc2.left + 2, rc2.top + 1, NULL );
               LineTo( hdc, rc2.right - 2, rc2.top + 1 );
               SelectPen( hdc, hpenWhite );
               MoveToEx( hdc, rc2.left, rc2.bottom - 1, NULL );
               LineTo( hdc, rc2.left, rc2.top + 2 );
               LineTo( hdc, rc2.left + 2, rc2.top );
               LineTo( hdc, rc2.right - 2, rc2.top );
               }
            if( fClipped )
               {
               int i;

               SelectPen( hdc, hpenGray );
               for( i = rc2.top; i < rc2.bottom; i += 3 )
                  {
                  int j = ( ( 6 - ( i - rc2.top ) % 12 ) / 3 ) % 2;

                  MoveToEx( hdc, rc2.right + j, i, NULL );
                  LineTo( hdc, rc2.right + j, min( i + 3, rc2.bottom ) );
                  }
               }
            else
               {
               SelectPen( hdc, hpenBlack );
               MoveToEx( hdc, rc2.right - 2, rc2.top + 1, NULL );
               LineTo( hdc, rc2.right - 1, rc2.top + 2 );
               LineTo( hdc, rc2.right - 1, rc2.bottom );
               SelectPen( hdc, hpenGray );
               MoveToEx( hdc, rc2.right - 2, rc2.top + 2, NULL );
               LineTo( hdc, rc2.right - 2, rc2.bottom );
               }
            if( !fSelected && rc2.bottom == rc.bottom )
               {
               SelectPen( hdc, hpenWhite );
               MoveToEx( hdc, x, rc2.bottom, NULL );
               LineTo( hdc, rc2.right, rc2.bottom );
               SelectPen( hdc, hpenBrGray );
               MoveToEx( hdc, x + n, rc2.bottom + 1, NULL );
               LineTo( hdc, rc2.right, rc2.bottom + 1 );
               }
            fLastWasSelected = fSelected;
            x = rc2.right;
            n = 0;
            }
         InflateRect( &rc2, -2, -2 );
         DrawTabText( hwnd, hdc, pTab, &rc2, c, fSelected, hBoldFont );
         }
      pTab = pTab->pNextTab;
      }
   if( x < rc.right && !fButtons )
      {
      SelectPen( hdc, hpenWhite );
      MoveToEx( hdc, x, rc.bottom, NULL );
      LineTo( hdc, rc.right - 1, rc.bottom );
      SelectPen( hdc, hpenBrGray );
      MoveToEx( hdc, x + n, rc.bottom + 1, NULL );
      LineTo( hdc, rc.right - 2, rc.bottom + 1 );
      }
   SelectObject( hdc, hfontOld );
   if( hBoldFont )
      DeleteFont( hBoldFont );
   DeleteBrush( hbrGrey );
   DeleteBrush( hbrDither );
   SetTextColor( hdc, crText );
   if( NULL != hpenOld )
      SelectObject( hdc, hpenOld );
   DeletePen( hpenWhite );
   DeletePen( hpenBrGray );
   DeletePen( hpenLtGray );
   DeletePen( hpenGray );
}

void FASTCALL DrawTabText( HWND hwnd, HDC hdc, TAB FAR * pTab, RECT * prc, int index, BOOL fSelected, HFONT hBoldFont )
{
   if( GetWindowStyle( hwnd ) & TCS_OWNERDRAWFIXED )
      {
      DRAWITEMSTRUCT dis;

      dis.CtlType = ODT_TAB;
      dis.CtlID = GetDlgCtrlID( hwnd );
      dis.itemID = index;
      dis.itemAction = 0;
      dis.itemState = 0;
      dis.hwndItem = hwnd;
      dis.hDC = hdc;
      dis.rcItem = *prc;
      dis.itemData = (LPARAM)( (LPSTR)pTab + sizeof( TAB ) );
      SendMessage( GetParent( hwnd ), WM_DRAWITEM, dis.CtlID, (LPARAM)(LPSTR)&dis );
      }
   else if( pTab->pText )
      {
      COLORREF crText;
      int oldmode;
      HFONT hfontOld;
      RECT rc;
      int iWidth;
      SIZE size;

      rc = *prc;
      iWidth = rc.right - rc.left;
      crText = 0L;
      hfontOld = NULL;
      if( fSelected && hBoldFont )
         hfontOld = SelectFont( hdc, hBoldFont );
      if( !pTab->fEnabled )
         crText = SetTextColor( hdc, rgbGrey );
   #if TAB_CTL_ANIMATION
      else if( pTab->fHilited && fCtlAnimation )
         crText = SetTextColor( hdc, GetSysColor( COLOR_HIGHLIGHT ) );
   #endif
      GetTextExtentPoint( hdc, pTab->pText, lstrlen( pTab->pText ), &size );
      if( GetWindowStyle( hwnd ) & TCS_FORCELABELLEFT )
         {
         rc.left += 4;
         rc.right = min( rc.right, rc.left + size.cx );
         }
      else
         {
         if( size.cx > iWidth )
            size.cx = iWidth;
         rc.left += ( iWidth - size.cx ) / 2;
         rc.right = rc.left + size.cx;
         }
      rc.top += ( ( rc.bottom - rc.top ) - size.cy ) / 2;
      rc.bottom = rc.top + size.cy;
      oldmode = SetBkMode( hdc, TRANSPARENT );
      ExtTextOut( hdc, rc.left, rc.top, ETO_CLIPPED, &rc, pTab->pText, lstrlen( pTab->pText ), NULL );
      SetBkMode( hdc, oldmode );
      if( GetFocus() == hwnd && pTab->fFocus )
         {
         InflateRect( &rc, 1, 1 );
         DrawFocusRect( hdc, &rc );
         }
      if( pTab->fHilited )
         SetTextColor( hdc, crText );
      if( !pTab->fEnabled )
         SetTextColor( hdc, crText );
      if( fSelected && hBoldFont )
          SelectFont( hdc, hfontOld );
      }
}

void FASTCALL DrawTabButton( HDC hdc, RECT * prc, HPEN hbr1, HPEN hbr2, HPEN hbr3, HPEN hbr4, HBRUSH hbr, BOOL fClipped )
{
   RECT rc;

   SelectPen( hdc, hbr1 );
   MoveToEx( hdc, prc->left, prc->bottom - 1, NULL );
   LineTo( hdc, prc->left, prc->top - 1 );
   LineTo( hdc, prc->right, prc->top - 1 );
   SelectPen( hdc, hbr2 );
   MoveToEx( hdc, prc->left + 1, prc->bottom - 2, NULL );
   LineTo( hdc, prc->left + 1, prc->top );
   LineTo( hdc, prc->right - 1, prc->top );
   if( fClipped )
      {
      int i;

      SelectPen( hdc, hbr3 );
      MoveToEx( hdc, prc->left, prc->bottom, NULL );
      LineTo( hdc, prc->right, prc->bottom );
      SelectPen( hdc, hbr4 );
      MoveToEx( hdc, prc->left + 1, prc->bottom - 1, NULL );
      LineTo( hdc, prc->right - 1, prc->bottom - 1 );
      for( i = prc->top; i < prc->bottom; i += 3 )
         {
         int j = ( ( 6 - ( i - prc->top ) % 12 ) / 3 ) % 2;

         MoveToEx( hdc, prc->right + j, i, NULL );
         LineTo( hdc, prc->right + j, min( i + 3, prc->bottom ) );
         }
      }
   else
      {
      SelectPen( hdc, hbr3 );
      MoveToEx( hdc, prc->left, prc->bottom, NULL );
      LineTo( hdc, prc->right, prc->bottom );
      LineTo( hdc, prc->right, prc->top - 2 );
      SelectPen( hdc, hbr4 );
      MoveToEx( hdc, prc->left + 1, prc->bottom - 1, NULL );
      LineTo( hdc, prc->right - 1, prc->bottom - 1 );
      LineTo( hdc, prc->right - 1, prc->top - 1 );
      }
   SetRect( &rc, prc->left + 2, prc->top + 1, prc->right - 1, prc->bottom - 1 );
   FillRect( hdc, &rc, hbr );
}

HBITMAP FASTCALL CreateDitherBitmap( void )
{
   COLORREF clr;
   HBITMAP hbm;
   long patGray[ 8 ];
   register int i;
   HDC hDC;

   memset( &bmi, 0, sizeof( bmi ) );
   bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
   bmi.bmiHeader.biWidth = 8;
   bmi.bmiHeader.biHeight = 8;
   bmi.bmiHeader.biPlanes = 1;
   bmi.bmiHeader.biBitCount = 1;
   bmi.bmiHeader.biCompression = BI_RGB;

   clr = GetSysColor(COLOR_BTNFACE);
   bmi.bmiColors[0].rgbBlue = GetBValue(clr);
   bmi.bmiColors[0].rgbGreen = GetGValue(clr);
   bmi.bmiColors[0].rgbRed = GetRValue(clr);

   clr = GetSysColor(COLOR_BTNHIGHLIGHT);
   bmi.bmiColors[1].rgbBlue = GetBValue(clr);
   bmi.bmiColors[1].rgbGreen = GetGValue(clr);
   bmi.bmiColors[1].rgbRed = GetRValue(clr);

   for( i = 0; i < 8; i++ )
      patGray[i] = (i & 1) ? 0xAAAA5555L : 0x5555AAAAL;

   hDC = GetDC( NULL );
   hbm = CreateDIBitmap( hDC, &bmi.bmiHeader, CBM_INIT, (LPBYTE)patGray, (LPBITMAPINFO)&bmi, DIB_RGB_COLORS );
   ReleaseDC( NULL, hDC );
   return( hbm );
}
