/* SPLASH.C - Handles the splash screen
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

#include "main.h"
#include "palrc.h"
#include "resource.h"
#include <string.h>
#include "amlib.h"
#include "multimon.h"

#define   THIS_FILE __FILE__

static BOOL fDefDlgEx = FALSE;     /* DefDlg recursion flag trap */

BOOL EXPORT CALLBACK SplashScreen( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL SplashScreen_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL SplashScreen_OnPaint( HWND );
BOOL FASTCALL SplashScreen_OnInitDialog( HWND, HWND, LPARAM );
BOOL FASTCALL SplashScreen_OnEraseBkgnd( HWND, HDC );

HWND hSplashScreen;                     /* Handle to splash screen */
static BOOL fSplashVisible = FALSE;          /* Whether splash screen is visible */

/* This function displays the initial splash screen. The dialog is
 *   modeless and therefore remains on the screen until removed with a call
 *   to HideIntroduction.
 *
 *   The procedure function, SplashScreen, handles dialog initalisation.
 */
void FASTCALL ShowIntroduction( void )
{
     if( fSplashVisible )
          BringWindowToTop( hSplashScreen );
     else
          {
          hSplashScreen = CreateDialog( hRscLib, MAKEINTRESOURCE(IDDLG_SPLASH), NULL, (DLGPROC)SplashScreen );
          SetWindowPos( hSplashScreen, NULL, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER );
          SetWindowText( hSplashScreen, GS(IDS_STR1214) );
          ShowWindow( hSplashScreen, SW_SHOW );
          UpdateWindow( hSplashScreen );
          SetFocus( NULL );
          fSplashVisible = TRUE;
          }
}

/* This function removes the initial splash screen.
 */
void FASTCALL HideIntroduction( void )
{
     if( fSplashVisible )
          {
          DestroyWindow( hSplashScreen );
          fSplashVisible = FALSE;
          }
}

/* Handles the splash screen.
 */
BOOL EXPORT CALLBACK SplashScreen( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
     CheckDefDlgRecursion( &fDefDlgEx );
     return( SetDlgMsgResult( hwnd, message, SplashScreen_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the About dialog.
 */
LRESULT FASTCALL SplashScreen_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
     switch( message )
          {
          HANDLE_MSG( hwnd, WM_INITDIALOG, SplashScreen_OnInitDialog );
          HANDLE_MSG( hwnd, WM_PAINT, SplashScreen_OnPaint );
          HANDLE_MSG( hwnd, WM_ERASEBKGND, SplashScreen_OnEraseBkgnd );

          default:
               return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
          }
     return( FALSE );
}

/* Don't erase the background
 */
BOOL FASTCALL SplashScreen_OnEraseBkgnd( HWND hwnd, HDC hdc )
{
     return( TRUE );
}

/* This function paints the splash screen
 */
void FASTCALL SplashScreen_OnPaint( HWND hwnd )
{
     PICCTRLSTRUCT pcs;
     HDC hdc;
     HDC hdcSrc;
     HBITMAP hbmpOld;
     char szSitename[ 40 ];
     char szUsername[ 40 ];
     LPCSTR lpszLogo;
     PAINTSTRUCT ps;
     HFONT hOldFont;
     HFONT hSplashFont;
     LOGFONT lfSysFont;
     int mode;
     RECT rc;
     static int aSplashRGB[ 3 ];
     register int c, n;

     /* Begin painting.
      */
     hdc = BeginPaint( hwnd, &ps );
     GetClientRect( hwnd, &rc );

     /* Get the logo and select it into a DC. Also get
      */
     if( !*szSplashLogo )
          {
          lpszLogo = MAKEINTRESOURCE(fOldLogo ? IDB_SPLASH : IDB_SPLASH_NEW);
          Amctl_LoadDIBBitmap( hInst, lpszLogo, &pcs );
          }
     else
          {
          pcs.hBitmap = Amuser_LoadBitmapFromDisk( szSplashLogo, &pcs.hPalette );
          }

     hdcSrc = CreateCompatibleDC( hdc );
     SelectPalette( hdc, pcs.hPalette, FALSE );
     RealizePalette( hdc );
     SelectPalette( hdcSrc, pcs.hPalette, FALSE );
     RealizePalette( hdcSrc );
     hbmpOld = SelectBitmap( hdcSrc, pcs.hBitmap );

     /* Splash it onto the dialog.
      */
     BitBlt( hdc, 0, 0, rc.right, rc.bottom, hdcSrc, 0, 0, SRCCOPY );

     /* Get the font metrics, depending on the display size.
      */
     GetObject( GetStockFont( ANSI_VAR_FONT ), sizeof( LOGFONT ), &lfSysFont );
     lfSysFont.lfWeight = -MulDiv( 8, GetDeviceCaps( hdc, LOGPIXELSY ), 72 );
     lfSysFont.lfWeight = FW_BOLD;
     hSplashFont = CreateFontIndirect( &lfSysFont );

     for( n = c = 0; szSplashRGB[ n ] && c < 3; ++c )
          {
          aSplashRGB[ c ] = atoi( szSplashRGB + n );
          while( szSplashRGB[ n ] && szSplashRGB[ n ] != ' ' && szSplashRGB[ n ] != ',' )
               ++n;
          if( szSplashRGB[ n ] == ' ' ||  szSplashRGB[ n ] == ',' )
               ++n;
          }

     /* Find where we're going to show the user name
      * and site name.
      */
     mode = SetBkMode( hdc, TRANSPARENT );
     hOldFont = SelectFont( hdc, hSplashFont );
     memset( szUsername, '\0', sizeof(szUsername) );
     memset( szSitename, '\0', sizeof(szSitename) );
     About_GetUserName( szUsername, sizeof(szUsername) );
     About_GetSiteName( szSitename, sizeof(szSitename) );
     wsprintf( lpTmpBuf, GS(IDS_STR917),
                     szUsername, 
                     szSitename );
     if( !*szSplashLogo && fOldLogo )
          {
          /* Show the old logo.
           */
          SetTextColor( hdc, RGB( 255, 255, 255 ) );
          wsprintf( lpTmpBuf, GS(IDS_STR917), szUsername, szSitename );
          SetRect( &rc, 10, 10, rc.right, rc.bottom );
          DrawText( hdc, lpTmpBuf, -1, &rc, DT_NOCLIP|DT_NOPREFIX );

          /* Show copyright information
           */
          wsprintf( lpTmpBuf, GS(IDS_STR1221),
                          amv.nMaxima,
                          amv.nMinima,
                          amv.nBuild );
          SetRect( &rc, 15, 60, rc.right, rc.bottom );
          DrawText( hdc, lpTmpBuf, -1, &rc, DT_NOCLIP|DT_NOPREFIX );

          }
     else
          {
          /* Show the new logo.
           */
          SetTextColor( hdc, RGB( aSplashRGB[ 0 ], aSplashRGB[ 1 ], aSplashRGB[ 2 ] ) );
          GetClientRect( hwnd, &rc );
          rc.top = 7;
          DrawText( hdc, lpTmpBuf, -1, &rc, DT_CENTER|DT_NOCLIP|DT_NOPREFIX );

          /* Show copyright information
           */
          wsprintf( lpTmpBuf, GS(IDS_VERSION),
                          amv.nMaxima,
                          amv.nMinima,
                          amv.nBuild );
          strcat( lpTmpBuf, "\n" );
          strcat( lpTmpBuf, amv.szLegalCopyright );
          GetClientRect( hwnd, &rc );
          rc.top = 313;
          DrawText( hdc, lpTmpBuf, -1, &rc, DT_CENTER|DT_NOCLIP|DT_NOPREFIX );
          }

     /* Now clean up before we go home.
      */
     SelectFont( hdc, hOldFont );
     SetBkMode( hdc, mode );
     DeleteFont( hSplashFont );
     SelectBitmap( hdcSrc, hbmpOld );
     DeleteDC( hdcSrc );
     DeleteBitmap( pcs.hBitmap );
     if( NULL != pcs.hPalette )
          DeletePalette( pcs.hPalette );
     EndPaint( hwnd, &ps );
}

/* This function handles the WM_INITDIALOG message. Here we
 * adjust the size of the splash screen so that it exactly wraps
 * round the bitmap.
 */
BOOL FASTCALL SplashScreen_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
     HBITMAP hbmpLogo;
     BITMAP bmp;
     RECT rc;
     int x, y;

     /* Get the bitmap dimensions.
      */
     hbmpLogo = LoadBitmap( hInst, MAKEINTRESOURCE(fOldLogo ? IDB_SPLASH : IDB_SPLASH_NEW) );
     ASSERT( hbmpLogo != NULL );
     GetObject( hbmpLogo, sizeof(BITMAP), &bmp );
     DeleteBitmap( hbmpLogo );

     /* Now adjust the dimensions to account for the dialog frame.
      * The resulting width and height are used to resize the dialog.
      */
     SetRect( &rc, 0, 0, bmp.bmWidth, bmp.bmHeight );
     AdjustWindowRect( &rc, WS_POPUP|DS_MODALFRAME, FALSE );
     x = ( GetSystemMetrics( SM_CXSCREEN ) - bmp.bmWidth ) / 2;
     y = ( GetSystemMetrics( SM_CYSCREEN ) - bmp.bmHeight ) / 3;
     SetWindowPos( hwnd, NULL, x, y, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER|SWP_NOACTIVATE );
#ifdef NOUSEMM
     CenterWindowToMonitor(hwnd, hwnd, TRUE);
#endif NOUSEMM
     return( TRUE );
}
