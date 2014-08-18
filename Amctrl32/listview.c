/* LISTVIEW.C - Palantir listview control
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
#include "listview.h"
#include "amlib.h"
#include "amctrls.h"
#include "amctrli.h"
#include "kludge.h"

#define  THIS_FILE   __FILE__

typedef struct tagLISTVIEWICON {
   struct tagLISTVIEWICON FAR * lpNext;   /* Next icon */
   RECT rcIcon;                           /* Dimensions of icon */
   RECT rcLabel;                          /* Dimensions of label */
   HICON hIcon;                           /* Handle of icon */
   LPSTR lpszText;                        /* Text label */
   DWORD dwData;                          /* User specified data item */
   BOOL fSelected;                        /* TRUE if this item is selected */
} LISTVIEWICON;

typedef struct tagLISTVIEWICON FAR * LPATTACHMENT;

typedef struct tagLISTVIEW {
   LPATTACHMENT lpFirstIcon;           /* Pointer to first icon */
   LPATTACHMENT lpLastIcon;            /* Pointer to last icon */
   COLORREF crBack;                    /* Background colour */
   COLORREF crText;                    /* Foreground colour */
   int xSpacing;                       /* Width of each icon space */
   int ySpacing;                       /* Height of each icon space */
   int cItems;                         /* Number of items */
   int cSelItems;                      /* Number of selected items */
   HFONT hFont;                        /* Font used to draw listview text */
   int cVisibleItems;                            /* Horizontal scroll bar */// 20060325 - 2.56.2051.22
   int nScrollPos;
   int nScrollMax;
} LISTVIEW;

#define  GWW_EXTRA               4
#define  GetLVBlock(h)           ((LISTVIEW FAR *)GetWindowLong((h),0))
#define  PutLVBlock(h,b)         (SetWindowLong((h),0,(LONG)(b)))

LRESULT EXPORT CALLBACK ListviewWndProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL ListviewWnd_OnPaint( HWND );
void FASTCALL ListviewWnd_OnCommand( HWND, int, HWND, UINT );
void FASTCALL ListviewWnd_OnLButtonDown( HWND, BOOL, int, int, UINT );

static LISTVIEWICON FAR * GetPart( LISTVIEW FAR *, int );
static void FASTCALL InvalidateIcon( HWND, LISTVIEWICON FAR *, BOOL );
void FASTCALL AdjustNameToFit( HDC , LPSTR , int , BOOL , SIZE * );
void FASTCALL AdjustNameToFitMiddle( HDC , LPSTR , int , int , SIZE * );
void FASTCALL AdjustNameToFitRight( HDC , LPSTR , int , int , SIZE * );

/* This function adjusts the name in the specified buffer to fit
 * the specified pixel width.
 */
void FASTCALL AdjustNameToFit( HDC hdc, LPSTR lpszText, int cbMax, BOOL fMiddle, SIZE * pSize )
{
   int length;

   /* Compute the current length. If it all fits,
    * quit now.
    */
   length = strlen( lpszText );
   GetTextExtentPoint( hdc, lpszText, length, pSize );
   if( pSize->cx > cbMax )
      {
      /* Otherwise call the appropriate function to
       * adjust the text. For performance reasons, I've
       * used separate functions rather than try to make
       * a general purpose one.
       */
      if( fMiddle )
         AdjustNameToFitMiddle( hdc, lpszText, length, cbMax, pSize );
      else
         AdjustNameToFitRight( hdc, lpszText, length, cbMax, pSize );
      }
}

/* Adjust the specified text to fit cbMax pixels by scooping
 * out from the middle.
 */
void FASTCALL AdjustNameToFitMiddle( HDC hdc, LPSTR lpszText, int length, int cbMax, SIZE * pSize )
{
   BOOL fTakeFromLeft;
   int middle;
   int left;
   int right;

   /* Jump to the middle of the name and replace the
    * middle 3 letter with '...'. Then start cutting
    * out text on the left and right until the text
    * fits.
    */
   left = ( length / 2 ) - 2;
   middle = left + 1;
   right = left + 2;
   lpszText[ left ] = '.';
   lpszText[ middle ] = '.';
   lpszText[ right ] = '.';
   GetTextExtentPoint( hdc, lpszText, length, pSize );
   fTakeFromLeft = TRUE;
   while( pSize->cx > cbMax && length > 5 )
      {
      if( fTakeFromLeft )
         {
         if( middle > 1 )
            {
            memcpy( &lpszText[ middle - 2 ], &lpszText[ middle - 1 ], ( length - middle ) + 2 );
            --length;
            --middle;
            }
         }
      else
         {
         if( middle < length - 1 )
            {
            memcpy( &lpszText[ middle + 2 ], &lpszText[ middle + 3 ], ( length - middle ) - 1 );
            --length;
            }
         }
      GetTextExtentPoint( hdc, lpszText, length, pSize );
      fTakeFromLeft = !fTakeFromLeft;
      }
}

/* Adjust the specified text to fit cbMax pixels by scooping
 * out from the right.
 */
void FASTCALL AdjustNameToFitRight( HDC hdc, LPSTR lpszText, int length, int cbMax, SIZE * pSize )
{
   /* Before we get here, we've determined that the string is too
    * long, so append ellipsis to the end and truncate until it
    * fits.
    */
   if( length > 3 )
      {
      lpszText[ length - 1 ] = '.';
      lpszText[ length - 2 ] = '.';
      lpszText[ length - 3 ] = '.';
      GetTextExtentPoint( hdc, lpszText, length, pSize );
      while( pSize->cx > cbMax && length > 5 )
         {
         int index;

         index = length - 3;
         memcpy( &lpszText[ index - 1 ], &lpszText[ index ], 4 );
         GetTextExtentPoint( hdc, lpszText, --length, pSize );
         }
      }
}

/* This function registers the listview window class.
 */
BOOL FASTCALL RegisterListviewClass( HINSTANCE hInst )
{
   WNDCLASS wc;

   /* Register the listview dock window class
    */
   wc.style       = CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS | CS_DBLCLKS;
   wc.lpfnWndProc    = ListviewWndProc;
   wc.hIcon       = NULL;
   wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
   wc.lpszMenuName      = NULL;
   wc.cbWndExtra     = GWW_EXTRA;
   wc.cbClsExtra     = 0;
   wc.hbrBackground  = (HBRUSH)( COLOR_WINDOW + 1 );
   wc.lpszClassName  = LISTVIEW_CLASS;
   wc.hInstance      = hInst;
   return( RegisterClass( &wc ) );
}


void FASTCALL SetupListview(HWND hwnd) // 20060325 - 2.56.2051.22
{
   SCROLLINFO lpsi;
   RECT rc;
   int xNewSize; 
   int lIconWidth;
   LISTVIEW FAR * lpListview;
// GetWindowRect(GetParent(hwnd), &rc);
   
   GetWindowRect(hwnd, &rc);
   lpListview = GetLVBlock( hwnd );
   
   lIconWidth = lpListview->xSpacing * lpListview->cItems;

   xNewSize = rc.right - rc.left; 
   lpListview->nScrollMax = lIconWidth ;//- xNewSize;//max(lIconWidth, xNewSize); 
   lpListview->cVisibleItems = xNewSize / ( lpListview->xSpacing );

   //lpListview->nScrollPos = min(lpListview->nScrollPos, lpListview->nScrollMax); 

   lpsi.cbSize = sizeof(SCROLLINFO);
   lpsi.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS; 
   lpsi.nMin   = 0; 
   lpsi.nMax   = lpListview->nScrollMax; //200
   lpsi.nPage  = xNewSize;   //100
   lpsi.nPos   = lpListview->nScrollPos;// - ((lpListview->xSpacing * lpListview->cVisibleItems) / 2); 
   SetScrollInfo(hwnd, SB_HORZ, &lpsi, TRUE); 
}

/* This is the window procedure for the listview dock window.
 */
LRESULT EXPORT CALLBACK ListviewWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LISTVIEW FAR * lpListview;
   LISTVIEWICON FAR * lpIcon;

   switch( message )
      {
      case WM_CREATE: {
         static LISTVIEW FAR * lpNewListview;
         /* Create a new LISTVIEW structure and initialise
          * the fields.
          */

         INITIALISE_PTR(lpNewListview);
         if( fNewMemory( &lpNewListview, sizeof( LISTVIEW ) ) )
            {
            lpNewListview->lpFirstIcon = NULL;
            lpNewListview->lpLastIcon = NULL;
            lpNewListview->xSpacing = GetSystemMetrics( SM_CXICONSPACING ) + 30;
            lpNewListview->ySpacing = GetSystemMetrics( SM_CYICONSPACING );
            lpNewListview->cItems = 0;
            lpNewListview->cSelItems = 0;
            lpNewListview->crText = GetSysColor( COLOR_WINDOWTEXT );
            lpNewListview->crBack = GetSysColor( COLOR_WINDOW );
            lpNewListview->hFont = GetStockFont( ANSI_VAR_FONT );
            lpNewListview->nScrollPos = 0;
            lpNewListview->nScrollMax = 0;
            }
         PutLVBlock( hwnd, lpNewListview );
         break;
         }

      case WM_SIZE:
         {
            lpListview = GetLVBlock( hwnd );
            lpListview->nScrollPos = 0;
            SetupListview(hwnd);
            return 0;
         }

      case WM_HSCROLL:  // 20060325 - 2.56.2051.22
         { 
            int xDelta;  
            int xNewPos; 
            int yDelta = 0; 
            SCROLLINFO si;
            LISTVIEW FAR * lpListview;

            lpListview = GetLVBlock( hwnd );
 
            switch (LOWORD(wParam)) 
            { 
               case SB_PAGEUP: 
                  xNewPos = lpListview->nScrollPos - (lpListview->xSpacing * 10); 
                  break; 

               case SB_PAGEDOWN: 
                  xNewPos = lpListview->nScrollPos + (lpListview->xSpacing * 10); 
                  break; 

               case SB_LINEUP: 
                  xNewPos = lpListview->nScrollPos - lpListview->xSpacing; 
                  break; 

               case SB_LINEDOWN: 
                  xNewPos = lpListview->nScrollPos + lpListview->xSpacing; 
                  break; 

               case SB_THUMBPOSITION: 
                  xNewPos = HIWORD(wParam); 
                  break; 
 
               case SB_THUMBTRACK:
                  xNewPos = HIWORD(wParam); 
                  break; 

               default: 
                  xNewPos = lpListview->nScrollPos; 
            } 
            
            si.cbSize = sizeof( SCROLLINFO );
            si.fMask = SIF_PAGE | SIF_RANGE;
            GetScrollInfo( hwnd, SB_HORZ, &si );
            lpListview->nScrollMax = si.nMax - si.nPage + 1;

            // New position must be between 0 and the screen width. 
 
            xNewPos = max(0, xNewPos); 
            xNewPos = min(lpListview->nScrollMax, xNewPos); 
 
            // If the current position does not change, do not scroll.

            if (xNewPos == lpListview->nScrollPos) 
               break; 
 
            // Determine the amount scrolled (in pixels). 
 
            xDelta = xNewPos - lpListview->nScrollPos; 
 
            // Reset the current scroll position. 
 
            lpListview->nScrollPos = xNewPos; 
 
            ScrollWindowEx(hwnd, -xDelta, 0, (CONST RECT *) NULL, 
               (CONST RECT *) NULL, (HRGN) NULL, (LPRECT) NULL, SW_INVALIDATE); 

            SetupListview(hwnd);
            
            InvalidateRect(hwnd, NULL, TRUE);

         } 
            break; 

      case WM_DESTROY:
         /* Delete all items, then delete the control.
          */
         lpListview = GetLVBlock( hwnd );
         lpIcon = lpListview->lpFirstIcon;
         while( lpIcon )
            {
            LISTVIEWICON FAR * pNextItem;

            pNextItem = lpIcon->lpNext;
            if( lpIcon->hIcon )
               DestroyIcon( lpIcon->hIcon );
            FreeMemory( &lpIcon->lpszText );
            FreeMemory( &lpIcon );
            lpIcon = pNextItem;
            }
         FreeMemory( &lpListview );
         return( 0L );

      case WM_CONTEXTMENU: /*!!SM!!*/
      case WM_RBUTTONDOWN:
      case WM_LBUTTONDBLCLK:
      case WM_LBUTTONDOWN: {
         BOOL fVertical;
         BOOL fShift;
         POINT pt;
         int x, y, loff;

         /* Get the button down coordinates.
          */
         pt.x = LOWORD( lParam );
         pt.y = HIWORD( lParam );

         /* Initialise for scanning.
          */
         lpListview = GetLVBlock( hwnd );
         lpIcon = lpListview->lpFirstIcon;
         x = 0;
         y = 0;
         loff = 0;   
         /* If the Shift key is down, don't deselect
          * existing selections.
          */
         fShift = ( GetKeyState( VK_SHIFT ) & 0x8000 ) == 0x8000;
         fVertical = ( GetWindowStyle( hwnd ) & LVS_VERT ) == LVS_VERT;

         /* Loop through each item and check whether
          * the cursor is in that item. If so, select the
          * item and repaint its label.
          */
         while( lpIcon )
            {
            RECT rcIcon;
            RECT rcLabel;

            if (loff >= lpListview->nScrollPos) 
            {
               rcIcon = lpIcon->rcIcon;
               rcLabel = lpIcon->rcLabel;
               OffsetRect( &rcIcon, x, y );
               OffsetRect( &rcLabel, x, y );
               if( ( PtInRect( &rcIcon, pt ) || PtInRect( &rcLabel, pt ) ) )
                  {
                  NMHDR nmhdr;

                  if( !lpIcon->fSelected )
                     {
                     lpIcon->fSelected = TRUE;
                     InvalidateRect( hwnd, &rcLabel, FALSE );
                     UpdateWindow( hwnd );
                     ++lpListview->cSelItems;
                     }
                  if( message == WM_LBUTTONDOWN )
                     Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_CLICK, &nmhdr );
                  else if( message == WM_LBUTTONDBLCLK )
                     Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_DBLCLK, &nmhdr );
                  else if( message == WM_RBUTTONDOWN || message == WM_CONTEXTMENU /*!!SM!!*/)
                     Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_RCLICK, &nmhdr );
                  }
               else if( lpIcon->fSelected && !fShift )
                  {
                  lpIcon->fSelected = FALSE;
                  InvalidateRect( hwnd, &rcLabel, FALSE );
                  UpdateWindow( hwnd );
                  --lpListview->cSelItems;
                  }
               if( fVertical )
                  y += lpListview->ySpacing;
               else
                  x += lpListview->xSpacing;
            }
            loff = loff + lpListview->xSpacing;
            lpIcon = lpIcon->lpNext;
            }
         break;
         }

      case WM_GETFONT:
         lpListview = GetLVBlock( hwnd );
         return( (LRESULT)(LPSTR)lpListview->hFont );

      case WM_SETFONT:
         /* Change the treeview control font.
          */
         lpListview = GetLVBlock( hwnd );
         lpListview->hFont = wParam ? (HFONT)wParam : GetStockFont( SYSTEM_FONT );
         if( LOWORD( lParam ) )
            {
            InvalidateRect( hwnd, NULL, TRUE );
            UpdateWindow( hwnd );
            }
         return( 0L );

      case LVM_ADDICON: {
         LISTVIEWICON FAR * lpNewIcon;
         LVIEW_ITEM FAR * lplvi;

         lpListview = GetLVBlock( hwnd );
         INITIALISE_PTR(lpNewIcon);
         if( fNewMemory( &lpNewIcon, sizeof( LISTVIEWICON ) ) )
            {
            HFONT hOldFont;
            SIZE size;
            LPSTR lpsz;
            HDC hdc;

            /* First create and store the
             * data structure.
             */
            INITIALISE_PTR(lpsz);
            lplvi = (LVIEW_ITEM FAR *)lParam;
            if( NULL == lpListview->lpFirstIcon )
               lpListview->lpFirstIcon = lpNewIcon;
            else
               lpListview->lpLastIcon->lpNext = lpNewIcon;
            lpListview->lpLastIcon = lpNewIcon;
            lpNewIcon->lpNext = NULL;
            lpNewIcon->hIcon = lplvi->hIcon;
            lpNewIcon->dwData = lplvi->dwData;
            if( fNewMemory( &lpsz, lstrlen( lplvi->lpszText ) + 1 ) )
               lstrcpy( lpsz, lplvi->lpszText );
            lpNewIcon->lpszText = lpsz;
            lpNewIcon->fSelected = FALSE;
            ++lpListview->cItems;

            /* Next compute some dimensions.
             */
            lpNewIcon->rcIcon.left = ( lpListview->xSpacing - GetSystemMetrics( SM_CXICON ) ) / 2;
            lpNewIcon->rcIcon.right = lpNewIcon->rcIcon.left + GetSystemMetrics( SM_CXICON );

            /* Compute the text size
             */
            hdc = GetDC( hwnd );
            hOldFont = SelectFont( hdc, lpListview->hFont );
            GetTextExtentPoint( hdc, "WWWWWWWWW", 9, &size );
            lpNewIcon->rcLabel.left = ( lpListview->xSpacing - size.cx ) / 2;
            lpNewIcon->rcLabel.right = lpNewIcon->rcLabel.left + size.cx;
            AdjustNameToFit( hdc, lpNewIcon->lpszText, lpNewIcon->rcLabel.right - lpNewIcon->rcLabel.left, TRUE, &size );
            SelectFont( hdc, hOldFont );
            ReleaseDC( hwnd, hdc );

            /* Compute verticals.
             */
            lpNewIcon->rcIcon.top = ( lpListview->ySpacing - ( GetSystemMetrics( SM_CYICON ) + size.cy + 4 ) ) / 2;
            lpNewIcon->rcIcon.bottom = lpNewIcon->rcIcon.top + GetSystemMetrics( SM_CYICON );
            lpNewIcon->rcLabel.top = lpNewIcon->rcIcon.bottom + 4;
            lpNewIcon->rcLabel.bottom = lpNewIcon->rcLabel.top + size.cy;

            SetupListview(hwnd);

            }
         return( FALSE );
         }

      case LVM_GETCOUNT:
         lpListview = GetLVBlock( hwnd );
         return( lpListview->cItems );

      case LVM_GETSELCOUNT:
         lpListview = GetLVBlock( hwnd );
         return( lpListview->cSelItems );

      case LVM_SETSEL:
         /* Selects the specified icon, scrolling it
          * into view if required.
          */
         lpListview = GetLVBlock( hwnd );
         if( ( lpIcon = GetPart( lpListview, wParam ) ) != NULL )
            if( !lpIcon->fSelected )
               {
               lpIcon->fSelected = TRUE;
               InvalidateRect( hwnd, &lpIcon->rcLabel, FALSE );
               UpdateWindow( hwnd );
               ++lpListview->cSelItems;
               }
         return( 0L );

      case LVM_SETTEXT:
         /* Changes the text of the specified item.
          */
         lpListview = GetLVBlock( hwnd );
         if( ( lpIcon = GetPart( lpListview, wParam ) ) != NULL )
            {
            LPSTR lpszText;
            HFONT hOldFont;
            SIZE size;
            HDC hdc;

            /* Set the new text
             */
            lpszText = (LPSTR)lParam;
            if( NULL != lpIcon->lpszText )
               FreeMemory( &lpIcon->lpszText );
            if( fNewMemory( &lpIcon->lpszText, lstrlen( lpszText ) + 1 ) )
               lstrcpy( lpIcon->lpszText, lpszText );

            /* Compute the text size
             */
            hdc = GetDC( hwnd );
            hOldFont = SelectFont( hdc, lpListview->hFont );
            GetTextExtentPoint( hdc, lpIcon->lpszText, lstrlen( lpIcon->lpszText ), &size );
            SelectFont( hdc, hOldFont );
            ReleaseDC( hwnd, hdc );
            InvalidateIcon( hwnd, lpIcon, FALSE );
            lpIcon->rcLabel.left = ( lpListview->xSpacing - size.cx ) / 2;
            lpIcon->rcLabel.right = lpIcon->rcLabel.left + size.cx;

            /* Redraw the icon
             */
            InvalidateIcon( hwnd, lpIcon, TRUE );
            }
         return( 0L );

      case LVM_GETSEL:
         /* Return whether or not the specified item is
          * selected.
          */
         lpListview = GetLVBlock( hwnd );
         if( ( lpIcon = GetPart( lpListview, wParam ) ) != NULL )
            return( lpIcon->fSelected );
         return( -1 );

      case LVM_GETCURSEL: {
         int index;

         /* Return whether or not the specified item is
          * selected.
          */
         lpListview = GetLVBlock( hwnd );
         index = 0;
         for( lpIcon = lpListview->lpFirstIcon; lpIcon; lpIcon = lpIcon->lpNext )
            {
            if( lpIcon->fSelected )
               return( index );
            ++index;
            }
         return( -1 );
         }

      case LVM_GETICONITEM:
         lpListview = GetLVBlock( hwnd );
         if( ( lpIcon = GetPart( lpListview, wParam ) ) != NULL )
            {
            LVIEW_ITEM FAR * lplvi;       
   
            lplvi = (LVIEW_ITEM FAR *)lParam;
            lplvi->dwData = lpIcon->dwData;
            lplvi->hIcon = lpIcon->hIcon;
            return( TRUE );
            }
         return( FALSE );

      case LVM_RESETCONTENT:
         /* Delete all items.
          */
         lpListview = GetLVBlock( hwnd );
         lpIcon = lpListview->lpFirstIcon;
         while( lpIcon )
            {
            LISTVIEWICON FAR * pNextItem;

            pNextItem = lpIcon->lpNext;
            FreeMemory( &lpIcon->lpszText );
            FreeMemory( &lpIcon );
            lpIcon = pNextItem;
            }
         lpListview->lpFirstIcon = NULL;
         lpListview->lpLastIcon = NULL;
         lpListview->cItems = 0;
         lpListview->cSelItems = 0;
         lpListview->nScrollPos = 0; 
         lpListview->nScrollMax = 0;
         SetupListview(hwnd);
         return( 0L );

      case WM_PAINT: {
         PAINTSTRUCT ps;
         BOOL fVertical;
         HDC hdc;
      
         /* Make sure listview window is visible.
          */
         hdc = BeginPaint( hwnd, &ps );
         fVertical = ( GetWindowStyle( hwnd ) & LVS_VERT ) == LVS_VERT;
         if( IsWindowVisible( hwnd ) )
            {
            RECT rcIntersect;
            RECT rc;
      
            /* Do nothing if the area being painted does not
             * overlap the listview window.
             */
            GetClientRect( hwnd, &rc );
            //if( IntersectRect( &rcIntersect, &ps.rcPaint, &rc ) )
               {
               HFONT hOldFont;
               HBRUSH hbr;
               int x, y, loff;
      
               /* Erase the background.
                */
               lpListview = GetLVBlock( hwnd );
               hbr = CreateSolidBrush( lpListview->crBack );
               FillRect( hdc, &rcIntersect, hbr );
               DeleteBrush( hbr );

               /* Set the painting attributes.
                */
               hOldFont = SelectFont( hdc, lpListview->hFont );
               SetTextColor( hdc, lpListview->crText );
               SetBkColor( hdc, lpListview->crBack );
      
               /* Now loop and draw the icons in the window.
                */
               lpIcon = lpListview->lpFirstIcon;
               x = 0;
               y = 0;
               loff = 0;
               while( lpIcon )
               {
                  RECT rc;

                  if (loff >= lpListview->nScrollPos) 
                  {
                     /* Draw the icon.
                      */
                     DrawIcon( hdc, x + lpIcon->rcIcon.left, y + lpIcon->rcIcon.top, lpIcon->hIcon );
                     rc = lpIcon->rcLabel;
                     rc.left += x;
                     rc.top += y;
                     rc.right += x;
                     rc.bottom += y;
         
                     /* Draw the filename.
                      */
                     if( lpIcon->fSelected )
                        {
                        COLORREF crText;
                        COLORREF crBack;

                        crText = SetTextColor( hdc, GetSysColor( COLOR_HIGHLIGHTTEXT ) );
                        crBack = SetBkColor( hdc, GetSysColor( COLOR_HIGHLIGHT ) );
                        DrawText( hdc, lpIcon->lpszText, -1, &rc, DT_VCENTER|DT_CENTER|DT_NOPREFIX );
                        SetTextColor( hdc, crText );
                        SetBkColor( hdc, crBack );
                        }
                     else
                        DrawText( hdc, lpIcon->lpszText, -1, &rc, DT_VCENTER|DT_CENTER|DT_NOPREFIX );
                     if( fVertical )
                        y += lpListview->ySpacing;
                     else
                        x += lpListview->xSpacing;
                  }
                  loff = loff + lpListview->xSpacing;
                  lpIcon = lpIcon->lpNext;
               }

               /* Restore the attributes.
                */
               SelectFont( hdc, hOldFont );
               }
            }
         EndPaint( hwnd, &ps );
         break;
         }
      }
   return( DefWindowProc( hwnd, message, wParam, lParam ) );
}

/* This function retrieves the handle of an item given the
 * index of the item, where 0 is the index of the first item.
 */
static LISTVIEWICON FAR * GetPart( LISTVIEW FAR * lpListview, int index )
{
   LISTVIEWICON FAR * lpIcon;

   ASSERT( lpListview != NULL );
   lpIcon = lpListview->lpFirstIcon;
   while( lpIcon && index-- )
      lpIcon = lpIcon->lpNext;
   return( lpIcon );
}

/* This function invalidates the specified icon rectangle.
 */
static void FASTCALL InvalidateIcon( HWND hwnd, LISTVIEWICON FAR * lpThisIcon, BOOL fUpdate )
{
   LISTVIEWICON FAR * lpIcon;
   LISTVIEW FAR * lpListview;
   BOOL fVertical;
   int x, y;

   lpListview = GetLVBlock( hwnd );
   x = 0;
   y = 0;
   fVertical = ( GetWindowStyle( hwnd ) & LVS_VERT ) == LVS_VERT;
   for( lpIcon = lpListview->lpFirstIcon; lpIcon; lpIcon = lpIcon->lpNext )
      if( lpIcon == lpThisIcon )
         {
         RECT rcUpdate;

         UnionRect( &rcUpdate, &lpIcon->rcIcon, &lpIcon->rcLabel );
         OffsetRect( &rcUpdate, x, y );
         InvalidateRect( hwnd, &rcUpdate, TRUE );
         if( fUpdate )
            UpdateWindow( hwnd );
         break;
         }
      else if( fVertical )
         y += lpListview->ySpacing;
      else
         x += lpListview->xSpacing;
}
