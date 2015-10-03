/* PICCTRL.C - Implements the picture control
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
#include "amuser.h"
#include "kludge.h"

#define  THIS_FILE   __FILE__

#define  GWW_EXTRA            4
#define  GetBlock(h)          ((PICCTRL FAR *)GetWindowLong((h),0))
#define  PutBlock(h,b)        (SetWindowLong((h),0,(LONG)(b)))

typedef struct tagPICCTRL {
   PICCTRLSTRUCT pcs;               /* Bitmap and palette */
   char * pszApologText;            /* Text to display if bitmap absent */
   COLORREF crText;                 /* Color of apology text */
   HFONT hFont;                     /* Apology text font */
   BOOL fIOwnBitmap;                /* TRUE if button owns bitmap */
} PICCTRL;

LRESULT EXPORT CALLBACK PicCtrlWndFn( HWND, UINT, WPARAM, LPARAM );

/* This function registers the picture control window class.
 */
BOOL FASTCALL RegisterPicCtrlClass( HINSTANCE hInst )
{
   WNDCLASS wc;

   wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
   wc.hIcon          = NULL;
   wc.lpszMenuName   = NULL;
   wc.lpszClassName  = WC_PICCTRL;
   wc.hbrBackground  = (HBRUSH)( COLOR_BTNFACE+1 );
   wc.hInstance      = hInst;
   wc.style          = CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS | CS_DBLCLKS;
   wc.lpfnWndProc    = PicCtrlWndFn;
   wc.cbClsExtra     = 0;
   wc.cbWndExtra     = GWW_EXTRA;
   return( RegisterClass( &wc ) );
}

LRESULT EXPORT CALLBACK PicCtrlWndFn( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   PICCTRL FAR * pPicCtrl;

#ifdef _DEBUG
   /* Help us to catch NULL pointers...
    */
   pPicCtrl = NULL;
#endif
   switch( message )
      {
      case WM_CREATE: {
         static PICCTRL FAR * pNewPicCtrl;

         INITIALISE_PTR(pNewPicCtrl);
         if( fNewMemory( &pNewPicCtrl, sizeof( PICCTRL ) ) )
            {
            pNewPicCtrl->pcs.hBitmap = NULL;
            pNewPicCtrl->pcs.hPalette = NULL;
            pNewPicCtrl->pszApologText = NULL;
            pNewPicCtrl->hFont = GetStockFont( ANSI_VAR_FONT );
            pNewPicCtrl->crText = GetSysColor( COLOR_WINDOWTEXT );
            pNewPicCtrl->fIOwnBitmap = FALSE;
            PutBlock( hwnd, pNewPicCtrl );
            }
         return( pNewPicCtrl ? 0 : -1 );
         }

      case PICCTL_SETBITMAP: {
         HBITMAP hbmpOld;

         /* Set the bitmap we'll use with this control. There are two
          * ways to do this: pass the bitmap handle itself or pass an
          * instance handle and bitmap resource ID. In the latter case,
          * the control 'owns' the bitmap and will free it when it is
          * destroyed.
          */
         pPicCtrl = GetBlock( hwnd );
         if( !pPicCtrl->fIOwnBitmap )
            hbmpOld = pPicCtrl->pcs.hBitmap;
         else
            {
            if( NULL != pPicCtrl->pcs.hBitmap )
               DeleteBitmap( pPicCtrl->pcs.hBitmap );
            if( NULL != pPicCtrl->pcs.hPalette )
               DeletePalette( pPicCtrl->pcs.hPalette );
            hbmpOld = NULL;
            }
         if( NULL != (HINSTANCE)wParam )
            {
            Amctl_LoadDIBBitmap( (HINSTANCE)wParam, MAKEINTRESOURCE(lParam), &pPicCtrl->pcs );
            pPicCtrl->fIOwnBitmap = TRUE;
            }
         else
            {
            LPPICCTRLSTRUCT lpcs;

            lpcs = (LPPICCTRLSTRUCT)lParam;
            if( NULL != lpcs )
               pPicCtrl->pcs = *lpcs;
            else
               {
               pPicCtrl->pcs.hBitmap = NULL;
               pPicCtrl->pcs.hPalette = NULL;
               }
            pPicCtrl->fIOwnBitmap = TRUE;
            }
         InvalidateRect( hwnd, NULL, TRUE );
         return( (LRESULT)(LPSTR)hbmpOld );
         }

      case WM_SETFONT:
         /* Change the font used to display the apology
          * text.
          */
         pPicCtrl = GetBlock( hwnd );
         pPicCtrl->hFont = wParam ? (HFONT)wParam : GetStockFont( ANSI_VAR_FONT );
         if( LOWORD( lParam ) )
            {
            InvalidateRect( hwnd, NULL, TRUE );
            UpdateWindow( hwnd );
            }
         break;

      case PICCTL_SETTEXT: {
         char * pszNewText;

         /* Set the text that we'll show if the bitmap is
          * absent.
          */
         pPicCtrl = GetBlock( hwnd );
         if( pPicCtrl->pszApologText )
            FreeMemory( &pPicCtrl->pszApologText );
         pszNewText = (char *)lParam;
         if( fNewMemory( &pPicCtrl->pszApologText, strlen(pszNewText) + 1 ) )
            strcpy( pPicCtrl->pszApologText, pszNewText );
         return( 0L );
         }

      case WM_LBUTTONDOWN: {
         NMHDR nmh;

         /* User clicked on control.
          */
         Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_CLICK, (LPNMHDR)&nmh );
         break;
         }

      case WM_LBUTTONDBLCLK: {
         NMHDR nmh;

         /* User double-clicked on control.
          */
         Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_DBLCLK, (LPNMHDR)&nmh );
         break;
         }

      case WM_CONTEXTMENU: /*!!SM!!*/
      case WM_RBUTTONDOWN: {
         NMHDR nmh;

         Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_RCLICK, (LPNMHDR)&nmh );
         break;
         }

      case WM_PALETTECHANGED:
         if( (HWND)wParam != hwnd )
            {
            InvalidateRect( hwnd, NULL, TRUE );
            UpdateWindow( hwnd );
            }
         break;

      case WM_PAINT: {
         PAINTSTRUCT ps;
         HDC hdc;

         /* Begin painting,
          */
         hdc = BeginPaint( hwnd, &ps );
         pPicCtrl = GetBlock( hwnd );
         if( pPicCtrl->pcs.hBitmap )
            {
            BITMAP bmp;
            HDC hdcMem;
            HBITMAP hbmpOld;
            DWORD dwStyle;
            int cxPic;
            int cyPic;
            RECT rc;
      
            /* Initialise for drawing bitmap.
             */
            GetObject( pPicCtrl->pcs.hBitmap, sizeof( BITMAP ), &bmp );
            hdcMem = CreateCompatibleDC( hdc );
            SelectPalette( hdc, pPicCtrl->pcs.hPalette, FALSE );
            RealizePalette( hdc );
            SelectPalette( hdcMem, pPicCtrl->pcs.hPalette, FALSE );
            RealizePalette( hdcMem );
            hbmpOld = SelectBitmap( hdcMem, pPicCtrl->pcs.hBitmap );

            /* Draw the bitmap to the dialog
             */
            GetClientRect( hwnd, &rc );
            cxPic = rc.right - rc.left;
            cyPic = rc.bottom - rc.top;
            dwStyle = GetWindowStyle( hwnd );
            if( dwStyle & PCTL_STRETCH )
               {
               int nMode;

               /* We're stretching (or shrinking) the picture to fit
                * the control.
                */
               if( dwStyle & PCTL_MAINTAINAR )
                  {
                  if( bmp.bmHeight > bmp.bmWidth )
                     cxPic = (int)( (float)cyPic / ( (float)bmp.bmHeight / (float)bmp.bmWidth ) );
                  else
                     cyPic = (int)( (float)cxPic / ( (float)bmp.bmHeight / (float)bmp.bmWidth ) );
                  rc.left += ( ( rc.right - rc.left ) - cxPic ) / 2;
                  rc.top += ( ( rc.bottom - rc.top ) - cyPic ) / 2;
                  }
               nMode = SetStretchBltMode( hdc, COLORONCOLOR );
               SetBrushOrgEx( hdc, 0, 0, NULL );
               StretchBlt( hdc, rc.left, rc.top, cxPic, cyPic, hdcMem, 0, 0, bmp.bmWidth, bmp.bmHeight, SRCCOPY );
               SetStretchBltMode( hdc, nMode );
               }
            else {
               /* Picture is not stretched, so center it by default
                * unless one of the edge styles are used to buttress
                * it against one of the edges.
                */
               rc.left += max( 0, ( cxPic - bmp.bmWidth ) / 2);
               rc.top += max( 0, ( cyPic - bmp.bmHeight ) / 2);
               if( dwStyle & PCTL_LEFT )
                  rc.left = 0;
               if( dwStyle & PCTL_TOP )
                  rc.top = 0;
               if( dwStyle & PCTL_RIGHT )
                  rc.left = rc.right - bmp.bmWidth;
               if( dwStyle & PCTL_TOP )
                  rc.top = rc.bottom - bmp.bmHeight;
               rc.right = rc.left + bmp.bmWidth;
               rc.bottom = rc.top + bmp.bmHeight;
               BitBlt( hdc, rc.left, rc.top, bmp.bmWidth, bmp.bmHeight, hdcMem, 0, 0, SRCCOPY );
               }
      
            /* Tidy up afterwards.
             */
            SelectBitmap( hdcMem, hbmpOld );
            DeleteDC( hdcMem );
            }
         else if( pPicCtrl->pszApologText )
            {
            COLORREF crOldText;
            HFONT hOldFont;
            int nHeight;
            int oldMode;
            RECT rc2;
            RECT rc;

            /* No bitmap available, so print the apology text
             * instead.
             */
            GetClientRect( hwnd, &rc );
            hOldFont = SelectFont( hdc, pPicCtrl->hFont );
            crOldText = SetTextColor( hdc, pPicCtrl->crText );

            /* Compute the text dimensions. It may be multiline and we may need
             * to wrap, but DrawText won't vertically center multiline text (!)
             * so we have to do it manually.
             */
            CopyRect( &rc2, &rc );
            nHeight = DrawText( hdc, pPicCtrl->pszApologText, -1, &rc2, DT_CALCRECT|DT_CENTER|DT_WORDBREAK|DT_NOPREFIX );
            rc2.top = ( ( rc.bottom - rc.top ) - nHeight ) / 2;
            rc2.bottom = rc2.top + nHeight;
            rc2.right = rc.right;
            oldMode = SetBkMode( hdc, TRANSPARENT );
            DrawText( hdc, pPicCtrl->pszApologText, -1, &rc2, DT_CENTER|DT_WORDBREAK|DT_NOPREFIX );
            SetBkMode( hdc, oldMode );

            /* Clean up before we go home.
             */
            SetTextColor( hdc, crOldText );
            SelectFont( hdc, hOldFont );
            }
         EndPaint( hwnd, &ps );
         return( 0L );
         }

      case WM_DESTROY:
         pPicCtrl = GetBlock( hwnd );
         if( pPicCtrl->fIOwnBitmap )
            DeleteBitmap( pPicCtrl->pcs.hBitmap );
         if( NULL != pPicCtrl->pcs.hPalette )
            DeletePalette( pPicCtrl->pcs.hPalette );
         if( pPicCtrl->pszApologText )
            FreeMemory( &pPicCtrl->pszApologText );
         FreeMemory( &pPicCtrl );
         return( 0L );
      }
   return( DefWindowProc( hwnd, message, wParam, lParam ) );
}
