/* PICBUTTN.C - Implements the picture button
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
#include "string.h"
#include "amctrls.h"
#include "amctrli.h"
#include "kludge.h"

#define  THIS_FILE   __FILE__

#define  GWW_EXTRA            4
#define  GetBlock(h)          ((PICBUTTON FAR *)GetWindowLong((h),0))
#define  PutBlock(h,b)        (SetWindowLong((h),0,(LONG)(b)))

#ifndef COLOR_3DLIGHT
#define COLOR_3DLIGHT           22
#endif

#define  PBS_CHECKED             1
#define  PBS_HIGHLIGHTED         4
#define  PBS_FOCUS               8
#define  PBS_BUTTONDOWN          16

typedef struct tagPICBUTTON {
   HBITMAP hbmp;                    /* Handle of bitmap */
   LPSTR lpszText;                  /* Pointer to text */
   WORD nState;                     /* State word */
   BOOL fIOwnBitmap;                /* TRUE if button owns bitmap */
} PICBUTTON;

LRESULT EXPORT CALLBACK PicButtonWndFn( HWND, UINT, WPARAM, LPARAM );

/* This function registers the picture button window class.
 */
BOOL FASTCALL RegisterPicButtonClass( HINSTANCE hInst )
{
   WNDCLASS wc;

   wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
   wc.hIcon          = NULL;
   wc.lpszMenuName   = NULL;
   wc.lpszClassName  = WC_PICBUTTON;
   wc.hbrBackground  = (HBRUSH)( COLOR_BTNFACE+1 );
   wc.hInstance      = hInst;
   wc.style          = CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS;
   wc.lpfnWndProc    = PicButtonWndFn;
   wc.cbClsExtra     = 0;
   wc.cbWndExtra     = GWW_EXTRA;
   return( RegisterClass( &wc ) );
}

LRESULT EXPORT CALLBACK PicButtonWndFn( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   PICBUTTON FAR * pPicButton;

#ifdef _DEBUG
   /* Help us to catch NULL pointers...
    */
   pPicButton = NULL;
#endif
   switch( message )
      {
      case WM_CREATE: {
         static PICBUTTON FAR * pNewPicButton;
         LPCREATESTRUCT lpcs;

         INITIALISE_PTR(pNewPicButton);
         if( fNewMemory( &pNewPicButton, sizeof( PICBUTTON ) ) )
            {
            lpcs = (LPCREATESTRUCT)lParam;
            pNewPicButton->hbmp = NULL;
            pNewPicButton->lpszText = NULL;
            pNewPicButton->nState = 0;
            PutBlock( hwnd, pNewPicButton );
            if( lpcs->lpszName )
               SetWindowText( hwnd, lpcs->lpszName );
            }
         return( pNewPicButton ? 0 : -1 );
         }

      case PICBM_SETBITMAP: {
         HBITMAP hbmpOld;

         /* Set the bitmap we'll use with this control. There are two
          * ways to do this: pass the bitmap handle itself or pass an
          * instance handle and bitmap resource ID. In the latter case,
          * the control 'owns' the bitmap and will free it when it is
          * destroyed.
          */
         pPicButton = GetBlock( hwnd );
         if( !pPicButton->fIOwnBitmap )
            hbmpOld = pPicButton->hbmp;
         else
            {
            if( NULL != pPicButton->hbmp )
               DeleteBitmap( pPicButton->hbmp );
            hbmpOld = NULL;
            }
         if( 0 == HIWORD(lParam) )
            {
            pPicButton->hbmp = LoadBitmap( (HINSTANCE)wParam, MAKEINTRESOURCE(lParam) );
            pPicButton->fIOwnBitmap = TRUE;
            }
         else
            {
            pPicButton->hbmp = (HBITMAP)lParam;
            pPicButton->fIOwnBitmap = TRUE;
            }
         return( (LRESULT)(LPSTR)hbmpOld );
         }

      case WM_SETTEXT:
      case PICBM_SETTEXT:
         pPicButton = GetBlock( hwnd );
         if( pPicButton->lpszText )
            FreeMemory( &pPicButton->lpszText );
         if( fNewMemory( &pPicButton->lpszText, lstrlen( (LPSTR)lParam ) + 1 ) )
            {
            lstrcpy( pPicButton->lpszText, (LPSTR)lParam );
            return( TRUE );
            }
         return( FALSE );

      case WM_GETTEXT:
         pPicButton = GetBlock( hwnd );
         return( (LRESULT)pPicButton->lpszText );

      case BM_SETCHECK: {
         RECT rc;

         pPicButton = GetBlock( hwnd );
         GetClientRect( hwnd, &rc );
         if( wParam && !(pPicButton->nState & PBS_CHECKED) )
            {
            InvalidateRect( hwnd, &rc, TRUE );
            pPicButton->nState |= PBS_CHECKED;
            }
         else if( !wParam && pPicButton->nState & PBS_CHECKED )
            {
            InvalidateRect( hwnd, &rc, TRUE );
            pPicButton->nState &= ~PBS_CHECKED;
            }
         UpdateWindow( hwnd );
         return( TRUE );
         }

      case WM_CHAR:
         if( IsWindowEnabled( hwnd ) )
            switch( wParam )
               {
               case VK_SPACE:
                  if( GetWindowStyle( hwnd ) & PBS_TOGGLE )
                     {
                     BOOL fCheck;

                     fCheck = (BOOL)SendMessage( hwnd, BM_GETCHECK, 0, 0L );
                     SendMessage( hwnd, BM_SETCHECK, (WPARAM)!fCheck, 0L );
                     }
                  else
                     {
                     SendMessage( hwnd, BM_SETSTATE, TRUE, 0L );
                     SendMessage( hwnd, BM_SETSTATE, FALSE, 0L );
                     }
                  SendDlgCommand( GetParent( hwnd ), GetDlgCtrlID( hwnd ), BN_CLICKED );
                  break;
               }
         return( 0L );

      case WM_GETDLGCODE: {
         DWORD dwStyle;
         WORD wBtnStyle;

         dwStyle = GetWindowStyle( hwnd );
         wBtnStyle = LOWORD(dwStyle) & 0x000F;
         switch( wBtnStyle )
            {
            case BS_PUSHBUTTON:
               return( (LONG)(DLGC_UNDEFPUSHBUTTON/*|DLGC_BUTTON|DLGC_WANTCHARS*/) );

            case BS_DEFPUSHBUTTON:
               return( (LONG)(DLGC_DEFPUSHBUTTON/*|DLGC_BUTTON|DLGC_WANTCHARS*/) );

            case BS_RADIOBUTTON:
            case BS_AUTORADIOBUTTON:
               return( (LONG)(DLGC_RADIOBUTTON/*|DLGC_BUTTON|DLGC_WANTCHARS*/) );

            case BS_GROUPBOX:
               return( (LRESULT)DLGC_STATIC );

            default:
               return( (LONG)DLGC_BUTTON );
            }
         return( 1L );
         }

      case BM_GETCHECK:
         pPicButton = GetBlock( hwnd );
         return( (pPicButton->nState & PBS_CHECKED) ? TRUE : FALSE );

      case BM_SETSTATE: {
         RECT rc;

         pPicButton = GetBlock( hwnd );
         if( wParam )
            pPicButton->nState |= PBS_BUTTONDOWN;
         else
            pPicButton->nState &= ~PBS_BUTTONDOWN;
         GetClientRect( hwnd, &rc );
         InvalidateRect( hwnd, &rc, TRUE );
         UpdateWindow( hwnd );
         break;
         }

      case BM_GETSTATE:
         pPicButton = GetBlock( hwnd );
         return( (LRESULT)pPicButton->nState );

      case WM_CONTEXTMENU: /*!!SM!!*/
      case WM_RBUTTONDOWN: {
         NMHDR nmh;

         Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_RCLICK, (LPNMHDR)&nmh );
         break;
         }

      case WM_LBUTTONDOWN: {
         RECT rc;

         /* Only need to bother if control is
          * enabled.
          */
         if( IsWindowEnabled( hwnd ) )
            {
            POINT pt;

            pPicButton = GetBlock( hwnd );
            pt.x = LOWORD( lParam );
            pt.y = HIWORD( lParam );
   
            /* Make sure control has the focus.
             */
            SetFocus( hwnd );

            /* Set the attribute to indicate that this button
             * is pressed.
             */
            pPicButton->nState |= PBS_BUTTONDOWN;
            GetClientRect( hwnd, &rc );
            InvalidateRect( hwnd, &rc, TRUE );
            UpdateWindow( hwnd );
            SetCapture( hwnd );
            }
         return( 0L );
         }

      case WM_MOUSEMOVE:
         pPicButton = GetBlock( hwnd );
         if( pPicButton->nState & PBS_BUTTONDOWN )
            {
            BOOL fUpdate;
            POINT pt;
            RECT rc;

            pt.x = LOWORD( lParam );
            pt.y = HIWORD( lParam );
            GetClientRect( hwnd, &rc );
            fUpdate = FALSE;
            if( PtInRect( &rc, pt ) )
               {
               if( !(pPicButton->nState & PBS_BUTTONDOWN) )
                  {
                  pPicButton->nState |= PBS_BUTTONDOWN;
                  fUpdate = TRUE;
                  }
               }
            else
               {
               if( pPicButton->nState & PBS_BUTTONDOWN )
                  {
                  pPicButton->nState &= ~PBS_BUTTONDOWN;
                  fUpdate = TRUE;
                  }
               }
            if( fUpdate )
               {
               InvalidateRect( hwnd, &rc, TRUE );
               UpdateWindow( hwnd );
               }
            }
         break;

      case WM_LBUTTONUP: {
         POINT pt;

         pPicButton = GetBlock( hwnd );
         pt.x = LOWORD( lParam );
         pt.y = HIWORD( lParam );

         /* Make sure control has the focus.
          */
         SetFocus( hwnd );
         ReleaseCapture();

         /* Set the attribute to indicate that this button
          * is pressed.
          */
         if( pPicButton->nState & PBS_BUTTONDOWN )
            {
            RECT rc;

            if( GetWindowStyle( hwnd ) & PBS_TOGGLE )
               if( pPicButton->nState & PBS_CHECKED )
                  pPicButton->nState &= ~PBS_CHECKED;
               else
                  pPicButton->nState |= PBS_CHECKED;
            pPicButton->nState &= ~PBS_BUTTONDOWN;
            GetClientRect( hwnd, &rc );
            InvalidateRect( hwnd, &rc, TRUE );
            SendDlgCommand( GetParent( hwnd ), GetDlgCtrlID( hwnd ), BN_CLICKED );
            }
         UpdateWindow( hwnd );
         return( 0L );
         }

      case WM_ENABLE:
      case WM_SETFOCUS:
         pPicButton = GetBlock( hwnd );
         pPicButton->nState |= PBS_FOCUS;
         InvalidateRect( hwnd, NULL, FALSE );
         UpdateWindow( hwnd );
         return( 0L );

      case WM_KILLFOCUS:
         pPicButton = GetBlock( hwnd );
         pPicButton->nState &= ~PBS_FOCUS;
         InvalidateRect( hwnd, NULL, FALSE );
         UpdateWindow( hwnd );
         return( 0L );

      case WM_PAINT: {
         PAINTSTRUCT ps;
         HDC hdc;
         HPEN hpenWhite;
         HPEN hpenDkGrey;
         HPEN hpenBlack;
         HPEN hpenBrGrey;
         HPEN hpenOld;
         BITMAP bmp;
         HBRUSH hbr;
         int dyString;
         static RECT rc;
         BOOL fEnabled;
         RECT rcClient;
         HBITMAP hbmpPicOld;
         HBITMAP hbmpCpyOld;
         HBITMAP hbmpPic;
         HDC hdcPic;
         HDC hdcCpy;

         /* Begin painting,
          */
         hdc = BeginPaint( hwnd, &ps );
         pPicButton = GetBlock( hwnd );

         /* Create and cache some useful pens.
          */      
         hpenWhite = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNHIGHLIGHT ) );
         hpenDkGrey = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNSHADOW ) );
         hpenBrGrey = CreatePen( PS_SOLID, 1, GetSysColor( fNewShell ? COLOR_3DLIGHT : COLOR_BTNFACE ) );
         hpenBlack = GetStockPen( BLACK_PEN );

         /* Erase the background.
          */
         GetClientRect( hwnd, &rc );
         hbr = CreateSolidBrush( GetSysColor( COLOR_BTNFACE ) );
         FillRect( hdc, &rc, hbr );

         /* If the button is pressed, draw it pressed.
          */
         hpenOld = SelectPen( hdc, hpenWhite );
         if( pPicButton->nState & (PBS_CHECKED|PBS_BUTTONDOWN) )
            {
            DrawButton( hdc, &rc, hpenWhite, hpenBrGrey, hpenBlack, hpenDkGrey, hbr, FALSE );
            OffsetRect( &rc, 1, 1 );
            }
         else
            DrawButton( hdc, &rc, hpenBlack, hpenDkGrey, hpenWhite, hpenBrGrey, hbr, FALSE );

         /* Get the height of the string.
          */
         dyString = 0;
         fEnabled = IsWindowEnabled( hwnd );

         /* First, draw the string if there is one.
          */
         InflateRect( &rc, -2, -2 );
         rcClient = rc;
         GetObject( pPicButton->hbmp, sizeof( BITMAP ), &bmp );
         if( pPicButton->lpszText && *(pPicButton->lpszText) )
            {
            COLORREF crBack;
            HFONT hOldFont;

            hOldFont = SelectFont( hdc, GetWindowFont( GetParent( hwnd ) ) );
            crBack = SetBkColor( hdc, GetSysColor( COLOR_BTNFACE ) );
            rc.left += 4;
            DrawText( hdc, pPicButton->lpszText, -1, &rc, DT_VCENTER|DT_SINGLELINE|DT_CALCRECT );
            DrawText( hdc, pPicButton->lpszText, -1, &rc, DT_VCENTER|DT_SINGLELINE );
            rc.left = rc.right + 2;
            rc.right = rcClient.right;
            SetBkColor( hdc, crBack );
            SelectFont( hdc, hOldFont );
            }

         /* Create a copy of the bitmap. After this, hbmpPic will be the
          * bitmap being painted, and hdcPic will be the DC into which it
          * has beens selected.
          */
         hdcPic = CreateCompatibleDC( hdc );
         hdcCpy = CreateCompatibleDC( hdc );
         hbmpCpyOld = SelectBitmap( hdcCpy, pPicButton->hbmp );
         hbmpPic = CreateCompatibleBitmap( hdcCpy, bmp.bmWidth, bmp.bmHeight );
         hbmpPicOld = SelectBitmap( hdcPic, hbmpPic );
         BitBlt( hdcPic, 0, 0, bmp.bmWidth, bmp.bmHeight, hdcCpy, 0, 0, SRCCOPY );
         SelectBitmap( hdcCpy, hbmpCpyOld );
         DeleteDC( hdcCpy );
         SelectBitmap( hdcPic, hbmpPicOld );
         DeleteDC( hdcPic );

         /* Next, draw the picture.
          */
         DrawButtonBitmap( hdc, &rc, hbmpPic, NULL, bmp.bmWidth, bmp.bmHeight, dyString, 0, 0, fEnabled );
         DeleteBitmap( hbmpPic );

         /* Finally draw a focus around the button if we have
          * focus.
          */
         if( pPicButton->nState & PBS_FOCUS )
            DrawFocusRect( hdc, &rcClient );

         /* Clean up.
          */
         SelectPen( hdc, hpenOld );
         DeletePen( hpenWhite );
         DeletePen( hpenDkGrey );
         DeletePen( hpenBrGrey );
         DeleteBrush( hbr );
         EndPaint( hwnd, &ps );
         return( 0L );
         }

      case WM_DESTROY:
         pPicButton = GetBlock( hwnd );
         if( pPicButton->fIOwnBitmap )
            DeleteBitmap( pPicButton->hbmp );
         if( pPicButton->lpszText )
            FreeMemory( &pPicButton->lpszText );
         FreeMemory( &pPicButton );
         return( 0L );
      }
   return( DefWindowProc( hwnd, message, wParam, lParam ) );
}
