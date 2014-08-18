/* STATUS.C - Implements the status bar control
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

#define  OEMRESOURCE

#define  THIS_FILE   __FILE__

#include "warnings.h"
#include <windows.h>
#include <windowsx.h>
#include "malloc.h"
#include "amctrls.h"
#include "amctrli.h"
#include "kludge.h"

#define  DEF_VBORDER          2
#define  DEF_HBORDER          0
#define  DEF_INTERSPACING     2

typedef struct tagPART {
   int iLeftEdge;             /* Left edge of part */
   int iRightEdge;            /* Right edge of part */
   int iWidth;                /* Width of part */
   unsigned uType;            /* Type of part */
   union {
      char FAR * pText;          /* Text of part */
      DWORD dw32;             /* Unique 32-bit identifier */
      } DUMMYUNIONNAME;
} PART;

typedef struct tagSTATUSFIELD {
   int wHeight;               /* Height of status bar */
   int vBorder;               /* Vertical border width */
   int hBorder;               /* Horizontal border width */
   int nInterSpacing;         /* Part spacing width */
   int nMinHeight;            /* Minimum height */
   BOOL fSimple;              /* TRUE if displaying simple text */
   BYTE cParts;               /* Number of parts (255 == simple) */
   HWND hwndGauge;            /* Progress bar gauge */
   WORD wStepPos;             /* Current progress bar % */
   HFONT hFont;               /* Status bar font */
   PART simple;               /* Structure for a simple part */
} STATUSFIELD;

#define  GWW_EXTRA            4
#define  GetBlock(h)          ((STATUSFIELD FAR *)GetWindowLong((h),0))
#define  PutBlock(h,b)        (SetWindowLong((h),0,(LONG)(b)))

static HFONT hStatusFont = NULL;

LRESULT EXPORT CALLBACK StatusWndFn( HWND, UINT, WPARAM, LPARAM );
static void NEAR PASCAL DrawStatusBorders( HDC, PART FAR *, RECT * );
static void NEAR PASCAL DrawSingleStatusText( HWND, STATUSFIELD FAR *, PART FAR *, int );
static void NEAR PASCAL PaintStatusText( HWND, HDC, PART FAR *, RECT *, int );
static void NEAR PASCAL SetStatusBarFont( HWND, HFONT );
static void NEAR PASCAL SetPartsWidth( STATUSFIELD FAR *, int );
static DWORD NEAR PASCAL CopyStatusText( HWND, int, LPSTR );
static void NEAR PASCAL DrawPossibleSizingGrips( HWND, HDC );

/* This function registers the status bar window class.
 */
BOOL FASTCALL RegisterStatusClass( HINSTANCE hInst )
{
   WNDCLASS wc;

   wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
   wc.hIcon          = NULL;
   wc.lpszMenuName   = NULL;
   wc.lpszClassName  = STATUSCLASSNAME;
   wc.hbrBackground  = (HBRUSH)( COLOR_BTNFACE+1 );
   wc.hInstance      = hInst;
   wc.style          = CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS | CS_DBLCLKS;
   wc.lpfnWndProc    = StatusWndFn;
   wc.cbClsExtra     = 0;
   wc.cbWndExtra     = GWW_EXTRA;
   return( RegisterClass( &wc ) );
}

WINCOMMCTRLAPI HWND WINAPI EXPORT Amctl_CreateStatusbar( LONG style, LPCSTR lpszText, HWND hwndParent, UINT wID )
{
   HWND hwndStatus;

   if( style & WS_BORDER )
      style &= ~WS_BORDER;
   hwndStatus = CreateWindow( STATUSCLASSNAME,
                              lpszText,
                              style,
                              0, 0, 0, 0,
                              hwndParent,
                              (HMENU)wID,
                              GetWindowInstance( hwndParent ),
                              NULL );
   if( hwndStatus )
      SendMessage( hwndStatus, SB_SETTEXT, 0, (LPARAM)lpszText );
   return( hwndStatus );
}

LRESULT EXPORT CALLBACK StatusWndFn( HWND hwnd,  UINT message, WPARAM wParam, LPARAM lParam )
{
   STATUSFIELD FAR * pHdr;
   PART FAR * pPart;

#ifdef _DEBUG
   /* Help us to catch NULL pointers...
    */
   pHdr = NULL;
   pPart = NULL;
#endif
   switch( message )
      {
      case WM_CREATE: {
         static STATUSFIELD FAR * pNewHdr;

         INITIALISE_PTR(pNewHdr);
         if( fNewMemory( &pNewHdr, sizeof( STATUSFIELD ) + sizeof( PART ) ) )
            {
         #ifdef WIN32
            NONCLIENTMETRICS nclm;
         #endif
            TEXTMETRIC tm;
            HFONT hOldFont;
            int wHeight;
            HDC hdc;

            pNewHdr->vBorder = DEF_VBORDER;
            pNewHdr->hBorder = DEF_HBORDER;
            pNewHdr->nInterSpacing = DEF_INTERSPACING;
            pNewHdr->fSimple = FALSE;
            pNewHdr->cParts = 1;
            pNewHdr->hFont = NULL;
            pNewHdr->hwndGauge = NULL;
            pNewHdr->simple.iLeftEdge = 0;
            pNewHdr->simple.iRightEdge = -1;
            pNewHdr->simple.uType = 0;
            pNewHdr->simple.U1(pText) = NULL;

            pPart = (PART FAR *)( (char FAR *)pNewHdr + sizeof( STATUSFIELD ) );
            pPart->iLeftEdge = pNewHdr->hBorder;
            pPart->iRightEdge = -1;
            pPart->iWidth = -1;
            pPart->uType = 0;
            pPart->U1(pText) = NULL;

            hStatusFont = NULL;
            wHeight = 10;
         #ifdef WIN32
            /* Try creating the status bar font using the system
             * metrics first.
             */
            nclm.cbSize = sizeof(NONCLIENTMETRICS);
            if( SystemParametersInfo( SPI_GETNONCLIENTMETRICS, 0, &nclm, 0 ) )
               {
               hStatusFont = CreateFontIndirect( &nclm.lfStatusFont );
               wHeight = nclm.lfStatusFont.lfHeight;
               if( wHeight < 0 )
                  wHeight = -wHeight;
               }
         #endif
            hdc = GetDC( hwnd );
            if( NULL == hStatusFont )
               {
               int FntSize;

               FntSize = -MulDiv( wHeight, GetDeviceCaps( hdc, LOGPIXELSY ), 72 );
               hStatusFont = CreateFont( FntSize, 0, 0, 0, 400, 0, 0, 0,
                                         ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         DEFAULT_QUALITY, VARIABLE_PITCH|FF_SWISS, "Helv" );
               }
            hOldFont = SelectFont( hdc, hStatusFont );
            GetTextMetrics( hdc, &tm );
            wHeight = ( tm.tmHeight + tm.tmExternalLeading + 2 ) + ( 2 * pNewHdr->vBorder );
            pNewHdr->hFont = hStatusFont;
            pNewHdr->wHeight = wHeight;
            pNewHdr->nMinHeight = wHeight;
            SelectFont( hdc, hOldFont );
            ReleaseDC( hwnd, hdc );
            PutBlock( hwnd, pNewHdr );
            }
         return( pNewHdr ? 0 : -1 );
         }

      case WM_GETFONT:
         pHdr = GetBlock( hwnd );
         return( (LRESULT)(LPSTR)pHdr->hFont );

      case WM_SETFONT: {
         RECT rc;

         SetStatusBarFont( hwnd, (HFONT)wParam );
         GetClientRect( GetParent( hwnd ), &rc );
         SendMessage( hwnd, WM_MOVE, 0, 0L );
         SendMessage( hwnd, WM_SIZE, SIZE_RESTORED, 0L );
         SendMessage( GetParent( hwnd ), WM_SIZE, SIZE_RESTORED, MAKELPARAM( rc.right - rc.left, rc.bottom - rc.top ) );
         if( LOWORD( lParam ) )
            {
            pHdr = GetBlock( hwnd );
            if( NULL != pHdr->hwndGauge )
               {
               int x, y, cx, cy;
               HFONT hOldFont;
               SIZE sizeText;
               HDC hdc;

               /* Recompute the position of the gauge window.
                */
               pPart = (PART FAR *)( (char FAR *)pHdr + sizeof( STATUSFIELD ) );
               hdc = GetDC( hwnd );
               hOldFont = SelectFont( hdc, pHdr->hFont );
               GetTextExtentPoint( hdc, pPart->U1(pText), lstrlen(pPart->U1(pText)), &sizeText );
               SelectFont( hdc, hOldFont );
               ReleaseDC( hwnd, hdc );

               /* Move the gauge window.
                */
               x = pHdr->hBorder + sizeText.cx + 8;
               y = 5;
               cx = x + 130;
               cy = pHdr->wHeight - 9;
               SetWindowPos( pHdr->hwndGauge, NULL, x, y, cx, cy, SWP_NOZORDER|SWP_NOACTIVATE );
               }
            InvalidateRect( hwnd, NULL, FALSE );
            UpdateWindow( hwnd );
            }
         break;
         }

      case WM_DESTROY: {
         register int c;

         pHdr = GetBlock( hwnd );
         pPart = (PART FAR *)( (char FAR *)pHdr + sizeof( STATUSFIELD ) );
         for( c = 0; c < pHdr->cParts; ++c )
            if( pPart[ c ].U1(pText) && !( pPart[ c ].uType & SBT_OWNERDRAW ) )
               FreeMemory( &pPart[ c ].U1(pText) );
         if( pHdr->simple.U1(pText) && !( pHdr->simple.uType & SBT_OWNERDRAW ) )
            FreeMemory( &pHdr->simple.U1(pText) );
         if( pHdr->hFont == hStatusFont )
            {
            DeleteFont( hStatusFont );
            hStatusFont = NULL;
            }
         if( NULL != pHdr->hwndGauge )
            DestroyWindow( pHdr->hwndGauge );
         FreeMemory( &pHdr );
         break;
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

      case SB_GETBORDERS: {
         LPINT lpInt;

         lpInt = (LPINT)lParam;
         pHdr = GetBlock( hwnd );
         *lpInt++ = pHdr->hBorder;
         *lpInt++ = pHdr->vBorder;
         *lpInt = pHdr->nInterSpacing;
         return( TRUE );
         }

      case SB_SETMINHEIGHT:
         pHdr = GetBlock( hwnd );
         pHdr->nMinHeight = wParam + ( 2 * pHdr->vBorder );
         return( TRUE );

      case SB_GETPARTS: {
         int cPartsToGet;
         register int c;

         pHdr = GetBlock( hwnd );
         pPart = (PART FAR *)( (char FAR *)pHdr + sizeof( STATUSFIELD ) );
         cPartsToGet = min( pHdr->cParts, (int)wParam );
         for( c = 0; c < cPartsToGet; ++c )
            ((LPINT)(LPSTR)lParam)[ c ] = pPart[ c ].iWidth;
         return( cPartsToGet );
         }

      case SB_GETPARTRECT: {
         LPRECT lprc;

         /* This message retrieves the bounding rectangle of
          * the specified part. This is my own extension to the
          * common controls specification.
          */
         pHdr = GetBlock( hwnd );
         if( 255 == ( wParam & 0xFF ) )
            pPart = &pHdr->simple;
         else {
            pPart = (PART FAR *)( (char FAR *)pHdr + sizeof( STATUSFIELD ) );
            pPart += wParam & 0xFF;
            }
         lprc = (LPRECT)lParam;
         GetClientRect( hwnd, lprc );
         lprc->top += pHdr->vBorder;
         lprc->left = pPart->iLeftEdge + 1;
         if( pPart->iRightEdge != -1 )
            lprc->right = pPart->iRightEdge;
         else
            lprc->right -= pHdr->hBorder;
         lprc->bottom -= pHdr->vBorder;
         return( TRUE );
         }

      case WM_GETTEXTLENGTH:
         return( LOWORD( CopyStatusText( hwnd, 0, NULL ) ) );

      case WM_GETTEXT:
         return( LOWORD( CopyStatusText( hwnd, 0, (LPSTR)lParam ) ) );
         
      case SB_GETTEXTLENGTH:
         return( CopyStatusText( hwnd, wParam, NULL ) );

      case SB_GETTEXT:
         return( CopyStatusText( hwnd, wParam, (LPSTR)lParam ) );

      case SB_SETPARTS: {
         static STATUSFIELD FAR * pNewHdr;
         UINT cbBlock;
         register int c;
         RECT rc;

         pNewHdr = GetBlock( hwnd );
         pPart = (PART FAR *)( (char FAR *)pNewHdr + sizeof( STATUSFIELD ) );
         if( wParam != pNewHdr->cParts )
            {
            for( c = 0; c < pNewHdr->cParts; ++c )
               if( pPart[ c ].U1(pText) && !( pPart[ c ].uType & SBT_OWNERDRAW ) )
                  {
                  FreeMemory( &pPart[ c ].U1(pText) );
                  pPart[ c ].U1(pText) = NULL;
                  }
            cbBlock = sizeof( STATUSFIELD ) + ( wParam * sizeof( PART ) );
            if( fResizeMemory( &pNewHdr, cbBlock ) )
               pNewHdr->cParts = wParam;
            pPart = (PART FAR *)( (char FAR *)pNewHdr + sizeof( STATUSFIELD ) );
            for( c = 0; c < pNewHdr->cParts; ++c )
               {
               pPart[ c ].U1(pText) = NULL;
               pPart[ c ].uType = 0;
               }
            }
         for( c = 0; c < pNewHdr->cParts; ++c )
            pPart[ c ].iWidth = ((LPINT)(LPSTR)lParam)[ c ];
         GetClientRect( hwnd, &rc );
         SetPartsWidth( pNewHdr, rc.right - rc.left );
         PutBlock( hwnd, pNewHdr );
         break;
         }

      case SB_BEGINSTATUSGAUGE: {
         LPSTR lpszText;
         HFONT hOldFont;
         SIZE sizeText;
         RECT rc;
         HDC hdc;
         int cx;
         int cy;

         /* Set sizeText.cx to the width of the text
          * in the status bar font.
          */
         pHdr = GetBlock( hwnd );
         lpszText = (LPSTR)lParam;
         hdc = GetDC( hwnd );
         hOldFont = SelectFont( hdc, pHdr->hFont );
         GetTextExtentPoint( hdc, lpszText, lstrlen(lpszText), &sizeText );
         SelectFont( hdc, hOldFont );
         ReleaseDC( hwnd, hdc );

         /* Save the text as the text of the
          * first item on the status bar.
          */
         pPart = (PART FAR *)( (char FAR *)pHdr + sizeof( STATUSFIELD ) );
         if( lpszText && ( NULL == pPart->U1(pText) || lstrcmp( pPart->U1(pText), lpszText ) != 0 ) )
            {
            static char FAR * pText;

            INITIALISE_PTR(pText);
            if( fNewMemory( &pText, lstrlen( lpszText ) + 1 ) )
               lstrcpy( pText, lpszText );
            if( pPart->U1(pText) )
               FreeMemory( &pPart->U1(pText) );
            pPart->U1(pText) = pText;
            }

         /* Destroy any existing status gauge window
          */
         if( NULL != pHdr->hwndGauge )
            DestroyWindow( pHdr->hwndGauge );

         /* Set the progress bar to begin just past the text.
          */
         rc.top = 5;
         rc.left = pHdr->hBorder + sizeText.cx + 8;
         cx = rc.left + 130;
         cy = pHdr->wHeight - 9;
         pHdr->hwndGauge = CreateWindow( PROGRESS_CLASS, "", WS_CHILD|WS_VISIBLE, rc.left, rc.top, cx, cy, hwnd, (HMENU)-1, hLibInst, 0L );
         if( NULL == pHdr->hwndGauge )
            return( FALSE );
         pHdr->wStepPos = 0;
         SendMessage( pHdr->hwndGauge, PBM_SETPOS, 0, 0L );

         /* Update the status bar.
          */
         SendMessage( hwnd, SB_GETPARTRECT, 0, (LPARAM)&rc );
         InvalidateRect( hwnd, &rc, TRUE );
         UpdateWindow( hwnd );
         return( TRUE );
         }

      case SB_STEPSTATUSGAUGE:
         pHdr = GetBlock( hwnd );
         if( NULL != pHdr->hwndGauge )
            {
            LPSTR lpszText;
            RECT rc2;
            RECT rc;

            /* Get the part rectangle to be going on with.
             */
            SendMessage( hwnd, SB_GETPARTRECT, 0, (LPARAM)&rc );

            /* Update the text supplied with the step.
             */
            lpszText = (LPSTR)lParam;
            pPart = (PART FAR *)( (char FAR *)pHdr + sizeof( STATUSFIELD ) );
            if( lpszText && ( NULL == pPart->U1(pText) || lstrcmp( pPart->U1(pText), lpszText ) != 0 ) )
               {
               static char FAR * pText;

               INITIALISE_PTR(pText);
               if( fNewMemory( &pText, lstrlen( lpszText ) + 1 ) )
                  lstrcpy( pText, lpszText );
               if( pPart->U1(pText) )
                  FreeMemory( &pPart->U1(pText) );
               pPart->U1(pText) = pText;
               InvalidateRect( hwnd, &rc, TRUE );
               }

            /* Update the progress bar.
             */
            if( wParam != pHdr->wStepPos )
               {
               pHdr->wStepPos = wParam;
               SendMessage( pHdr->hwndGauge, PBM_SETPOS, wParam, 0L );

               /* Invalidate the space to the right of the
                * progress bar.
                */
               GetWindowRect( pHdr->hwndGauge, &rc2 );
               MapWindowPoints( NULL, hwnd, (LPPOINT)&rc2, 1 );
               rc.left = rc2.right + 4;
               InvalidateRect( hwnd, &rc, FALSE );
               }

            /* Update everything that changed.
             */
            UpdateWindow( hwnd );
            }
         return( 0L );

      case SB_ENDSTATUSGAUGE:
         pHdr = GetBlock( hwnd );
         if( NULL != pHdr->hwndGauge )
            {
            DestroyWindow( pHdr->hwndGauge );
            pHdr->hwndGauge = NULL;
            }
         return( 0L );

      case WM_SETTEXT:
         wParam = 0;

      case SB_SETTEXT:
         pHdr = GetBlock( hwnd );
         if( 255 == ( wParam & 0xFF ) )
            pPart = &pHdr->simple;
         else {
            pPart = (PART FAR *)( (char FAR *)pHdr + sizeof( STATUSFIELD ) );
            pPart += wParam & 0xFF;
            }
         pPart->uType = wParam & 0xFF00;
         if( pPart->uType & SBT_OWNERDRAW )
            pPart->U1(dw32) = (DWORD)lParam;
         else {
            static char FAR * pText;
   
            INITIALISE_PTR(pText);
            if( fNewMemory( &pText, lstrlen( (LPSTR)lParam ) + 1 ) )
               lstrcpy( pText, (LPSTR)lParam );
            if( pPart->U1(pText) )
               FreeMemory( &pPart->U1(pText) );
            pPart->U1(pText) = pText;
            }
         if( !pHdr->fSimple || 255 == ( wParam & 0xFF ) )
            DrawSingleStatusText( hwnd, pHdr, pPart, 0 );
         break;

      case SB_SIMPLE:
         pHdr = GetBlock( hwnd );
         pHdr->fSimple = (BOOL)wParam;
         if( pHdr->hwndGauge )
            if( pHdr->fSimple && IsWindowVisible( pHdr->hwndGauge ) )
               ShowWindow( pHdr->hwndGauge, SW_HIDE );
            else if( !pHdr->fSimple && !IsWindowVisible( pHdr->hwndGauge ) ) 
               ShowWindow( pHdr->hwndGauge, SW_SHOW );
         InvalidateRect( hwnd, NULL, TRUE );
         UpdateWindow( hwnd );
         return( TRUE );

      case WM_NCHITTEST:
         if( GetWindowStyle( hwnd ) & SBARS_SIZEGRIP )
            {
            POINT pt;
            RECT rc;

            pt.x = LOWORD( lParam );
            pt.y = HIWORD( lParam );
            ScreenToClient( hwnd, &pt );
            GetClientRect( hwnd, &rc );
            if( pt.x >= rc.right - 10 )
               return( HTBOTTOMRIGHT );
            }
         break;

      case WM_MOVE: {
         RECT rc;

         /* Trap MoveWindow and SetWindowPos on this window so that the
          * status bar remains anchored to the top or bottom of the parent window.
          */
         if( !( GetWindowStyle( hwnd ) & ACTL_CCS_NOPARENTALIGN ) )
            {
            pHdr = GetBlock( hwnd );
            GetClientRect( GetParent( hwnd ), &rc );
            if( GetWindowStyle( hwnd ) & ACTL_CCS_TOP )
               SetWindowPos( hwnd, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER );
            else
               SetWindowPos( hwnd, NULL, 0, rc.bottom - pHdr->wHeight, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER );
            }
         break;
         }

      case WM_SIZE: {
         RECT rc;

         /* Trap MoveWindow and SetWindowPos on this window so that the
          * status bar remains the same width as the client area the parent window.
          */
         pHdr = GetBlock( hwnd );
         if( 0 != lParam )
            SetPartsWidth( pHdr, LOWORD( lParam ) );
         GetClientRect( GetParent( hwnd ), &rc );
         SetWindowPos( hwnd, NULL, 0, 0, rc.right, pHdr->wHeight, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER );
         break;
         }

      case WM_ERASEBKGND: {
         static RECT rc;
         HBRUSH hbr;

         hbr = CreateSolidBrush( GetSysColor( COLOR_BTNFACE ) );
         GetClientRect( hwnd, &rc );
         FillRect( (HDC)wParam, &rc, hbr );
         DeleteBrush( hbr );
         return( TRUE );
         }

      case WM_PAINT: {
         PAINTSTRUCT ps;
         DWORD dwStyle;
         HFONT hfontOld;
         COLORREF crTmp;
         COLORREF crBack;
         static RECT rc;
         HDC hdc;

         pHdr = GetBlock( hwnd );
         hdc = BeginPaint( hwnd, (LPPAINTSTRUCT)&ps );
         GetClientRect( hwnd, &rc );
         dwStyle = GetWindowStyle( hwnd );
         hfontOld = SelectFont( hdc, pHdr->hFont );
         crTmp = SetTextColor( hdc, GetSysColor( COLOR_WINDOWTEXT ) );
         crBack = SetBkColor( hdc, GetSysColor( COLOR_BTNFACE ) );
         if( pHdr->fSimple )
            {
            InflateRect( &rc, -pHdr->hBorder, -pHdr->vBorder );
            InflateRect( &rc, -1, -1 );
            PaintStatusText( hwnd, hdc, &pHdr->simple, &rc, 0 );
            }
         else {
            register int c;
            RECT rc2;
   
            InflateRect( &rc, -pHdr->hBorder, -pHdr->vBorder );
            rc2 = rc;
            pPart = (PART FAR *)( (char FAR *)pHdr + sizeof( STATUSFIELD ) );
            for( c = 0; c < pHdr->cParts; ++c )
               {
               static RECT rc3;
   
               if( -1 == pPart->iRightEdge )
                  rc2.right = rc.right;
               else
                  rc2.right = pPart->iRightEdge;
               rc3 = rc2;
               DrawStatusBorders( hdc, pPart, &rc3 );
               InflateRect( &rc3, -1, -1 );
               PaintStatusText( hwnd, hdc, pPart, &rc3, c );
               if( c == 0 && pHdr->hwndGauge )
                  {
                  char szBuf[ 20 ];
                  RECT rc4;

                  GetWindowRect( pHdr->hwndGauge, &rc4 );
                  MapWindowPoints( NULL, hwnd, (LPPOINT)&rc4, 1 );
                  rc3.left = rc4.right + 4;
                  wsprintf( szBuf, "%u%% complete.", pHdr->wStepPos );
                  DrawText( hdc, szBuf, -1, &rc3, DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_LEFT );
                  }
               rc2.left = rc2.right + pHdr->nInterSpacing;
               ++pPart;
               }
            }
         DrawPossibleSizingGrips( hwnd, hdc );
         SetTextColor( hdc, crTmp );
         SetBkColor( hdc, crBack );
         SelectFont( hdc, hfontOld );
         EndPaint( hwnd, (LPPAINTSTRUCT)&ps );
         break;
         }
      }
   return( DefWindowProc( hwnd, message, wParam, lParam ) );
}

static DWORD NEAR PASCAL CopyStatusText( HWND hwnd, int cPart, LPSTR lpBuf )
{
   DWORD dwRet;
   STATUSFIELD FAR * pHdr;
   PART FAR * pPart;

   ASSERT( lpBuf != NULL );
   pHdr = GetBlock( hwnd );
   pPart = (PART FAR *)( (char FAR *)pHdr + sizeof( STATUSFIELD ) );
   if( cPart > pHdr->cParts - 1 )
      return( FALSE );
   if( pPart[ cPart ].uType & SBT_OWNERDRAW )
      dwRet = pPart[ cPart ].U1(dw32);
   else
      {
      if( lpBuf )
         lstrcpy( lpBuf, pPart[ cPart ].U1(pText) );
      dwRet = MAKELONG( lstrlen( pPart[ cPart ].U1(pText) ), pPart[ cPart ].uType );
      }
   return( dwRet );
}

static void NEAR PASCAL SetPartsWidth( STATUSFIELD FAR * pHdr, int wWidth )
{
   register int c, x;
   PART FAR * pPart;

   ASSERT( pHdr != NULL );
   pPart = (PART FAR *)( (char FAR *)pHdr + sizeof( STATUSFIELD ) );
   x = wWidth - pHdr->hBorder;
   for( c = pHdr->cParts - 1; c >= 0; --c )
      {
      pPart[ c ].iRightEdge = x;
      if( -1 == pPart[ c ].iWidth )
         pPart[ c ].iLeftEdge = pHdr->hBorder;
      else
         {
         pPart[ c ].iLeftEdge = pPart[ c ].iRightEdge - pPart[ c ].iWidth;
         x = pPart[ c ].iLeftEdge - pHdr->nInterSpacing;
         }
      }
}

static void NEAR PASCAL DrawStatusBorders( HDC hdc, PART FAR * pPart, RECT * prc )
{
   if( !( pPart->uType & SBT_NOBORDERS ) )
      {
      HPEN hpenGray;
      HPEN hpenWhite;
      HPEN hpenOld;

      ASSERT( pPart != NULL );
      ASSERT( prc != NULL );

      hpenGray = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNSHADOW ) );
      hpenWhite = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNHIGHLIGHT ) );

      hpenOld = SelectPen( hdc, ( pPart->uType & SBT_POPOUT ) ? hpenWhite : hpenGray );
      MoveToEx( hdc, prc->left, prc->bottom, NULL );
      LineTo( hdc, prc->left, prc->top );
      LineTo( hdc, prc->right - 1, prc->top );

      SelectPen( hdc, ( pPart->uType & SBT_POPOUT ) ? hpenGray : hpenWhite );
      MoveToEx( hdc, prc->right - 1, prc->top, NULL );
      LineTo( hdc, prc->right - 1, prc->bottom );
      LineTo( hdc, prc->left - 1, prc->bottom );

      SelectPen( hdc, hpenOld );
      DeletePen( hpenGray );
      DeletePen( hpenWhite );
      }
}

static void NEAR PASCAL DrawSingleStatusText( HWND hwnd, STATUSFIELD FAR * pHdr, PART FAR * pPart, int index )
{
   HFONT hfontOld;
   HBRUSH hbr;
   COLORREF crTmp;
   COLORREF crBack;
   HDC hdc;
   static RECT rc;

   ASSERT( pHdr != NULL );
   ASSERT( pPart != NULL );

   hdc = GetDC( hwnd );
   GetClientRect( hwnd, &rc );
   rc.top += pHdr->vBorder;
   rc.left = pPart->iLeftEdge + 1;
   if( pPart->iRightEdge != -1 )
      rc.right = pPart->iRightEdge;
   else
      rc.right -= pHdr->hBorder;
   rc.bottom -= pHdr->vBorder;
   InflateRect( &rc, -1, -1 );
   hfontOld = SelectFont( hdc, pHdr->hFont );
   hbr = CreateSolidBrush( GetSysColor( COLOR_BTNFACE ) );
   crTmp = SetTextColor( hdc, GetSysColor( COLOR_WINDOWTEXT ) );
   crBack = SetBkColor( hdc, GetSysColor( COLOR_BTNFACE ) );
   FillRect( hdc, &rc, hbr );
   DeleteBrush( hbr );
   PaintStatusText( hwnd, hdc, pPart, &rc, index );
   if( index == pHdr->cParts - 1 )
      DrawPossibleSizingGrips( hwnd, hdc );
   SetBkColor( hdc, crBack );
   SetTextColor( hdc, crTmp );
   SelectFont( hdc, hfontOld );
   ReleaseDC( hwnd, hdc );
}

/* If the status bar control has SBARS_SIZEGRIP, then draw sizing grips in
 * the bottom right hand corner of the control. This attribute should be
 * specified if the status bar is at the bottom of a resizable window.
 */
static void NEAR PASCAL DrawPossibleSizingGrips( HWND hwnd, HDC hdc )
{
   if( GetWindowStyle( hwnd ) & SBARS_SIZEGRIP )
      {
      BITMAP bm;
      HBITMAP hbmpSizeGrips;
      HBITMAP hbmpOld;
      int nHeight;
      int nWidth;
      RECT rc;
      HDC hdc2;

      GetClientRect( hwnd, &rc );
      hbmpSizeGrips = LoadBitmap( NULL, MAKEINTRESOURCE( OBM_BTSIZE ) );
      GetObject( hbmpSizeGrips, sizeof( BITMAP ), &bm );
      hdc2 = CreateCompatibleDC( hdc );
      hbmpOld = SelectBitmap( hdc2, hbmpSizeGrips );
      nHeight = bm.bmHeight;
      nWidth = bm.bmWidth;
      BitBlt( hdc, rc.right - nWidth, rc.bottom - nHeight, nWidth, nHeight, hdc2, 0, 0, SRCCOPY );
      SelectBitmap( hdc2, hbmpOld );
      DeleteDC( hdc2 );
      }
}

static void NEAR PASCAL PaintStatusText( HWND hwnd, HDC hdc, PART FAR * pPart, RECT * prc, int index )
{  
   ASSERT( pPart != NULL );
   ASSERT( prc != NULL );
   if( pPart->uType & SBT_OWNERDRAW )
      {
      DRAWITEMSTRUCT dis;

      /* For owner-draw status bar items, get the parent
       * to do the drawing.
       */
      dis.CtlType = 0;
      dis.CtlID = GetDlgCtrlID( hwnd );
      dis.itemID = index;
      dis.itemAction = 0;
      dis.itemState = 0;
      dis.hwndItem = hwnd;
      dis.hDC = hdc;
      dis.rcItem = *prc;
      dis.itemData = pPart->U1(dw32);
      SendMessage( GetParent( hwnd ), WM_DRAWITEM, dis.CtlID, (LPARAM)(LPSTR)&dis );
      }
   else if( pPart->U1(pText) )
      {
      static int nTabPos[] = { DT_LEFT, DT_CENTER, DT_RIGHT };
      int c, n;
      int nTab;
      RECT rc;
      int mode;

      rc = *prc;
      rc.left += 3;
      rc.right -= 3;
      mode = SetBkMode( hdc, TRANSPARENT );
      for( nTab = n = c = 0; pPart->U1(pText)[ c ]; ++c )
         if( '\t' == pPart->U1(pText)[ c ] )
            {
            if( c - n && nTab < 3 )
               DrawText( hdc, pPart->U1(pText) + n, c - n, &rc, DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|nTabPos[ nTab ] );
            ++nTab;
            n = c + 1;
            break;
            }
      if( c - n && nTab < 3 )
         DrawText( hdc, pPart->U1(pText) + n, c - n, &rc, DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|nTabPos[ nTab ] );
      SetBkMode( hdc, mode );
      }
}

/* This function sets a new font for the status bar based on the
 * current dimensions for the status bar itself.
 */
static void NEAR PASCAL SetStatusBarFont( HWND hwnd, HFONT hFont )
{
   STATUSFIELD FAR * pHdr;
   HFONT hOldFont;
   TEXTMETRIC tm;
   HDC hdc;
   int wHeight;

   pHdr = GetBlock( hwnd );
   if( pHdr->hFont == hStatusFont )
      {
      DeleteFont( hStatusFont );
      hStatusFont = NULL;
      }
   pHdr->hFont = hFont;
   hdc = GetDC( hwnd );
   hOldFont = SelectFont( hdc, hFont );
   GetTextMetrics( hdc, &tm );
   wHeight = ( tm.tmHeight + tm.tmExternalLeading + 2 ) + ( 2 * pHdr->vBorder );
   if( wHeight < pHdr->nMinHeight )
      wHeight = pHdr->nMinHeight;
   pHdr->wHeight = wHeight;
   SelectFont( hdc, hOldFont );
   ReleaseDC( hwnd, hdc );
}
