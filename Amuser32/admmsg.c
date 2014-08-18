/* ADMMSG.C - The Ameol message box
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
#include "pdefs.h"
#include "amuser.h"
#include "multimon.h"

#define  MSGBOXEX_SPACING        18
#define  MSGBOXEX_BUTTONWIDTH    80
#define  MSGBOXEX_BUTTONHEIGHT   24
#define  MSGBOXEX_BUTTONSPACING  8
#define  MSGBOXEX_BOTTOMMARGIN   15
#define  MSGBOXEX_MINHEIGHT      70

#define  IDD_ICON                0xA90
#define  IDD_TEXT                0xA91

#ifndef WIN32
#pragma warning(disable:4103)
#pragma pack(1)         /* Microsoft C++: Assume byte packing throughout */
typedef struct {
   long  style;
   BYTE  cdit;
   int   x;
   int   y;
   int   cx;
   int   cy;
} DLGTEMPLATE;

typedef struct {
   int   x;
   int   y;
   int   cx;
   int   cy;
   int   id;
   long  style;
} DLGITEMTEMPLATE;
#endif

#define  CLA_BUTTON     0x0080
#define  CLA_STATIC     0x0082

int FASTCALL AddDlgItem( HANDLE, int, int, int, int, int, int, long, WORD, LPSTR );
int FASTCALL AddButtonItem( HANDLE, int, int, int, int, LPSTR );

static WNDPROC lpfnHookProc = NULL;    /* MessageBox hook procedure */
static HICON hIcon;                    /* Handle of icon */
static int nDefButton;                 /* Default button */
static int idDefButton;                /* ID of default button */
static int wUnitsX, wUnitsY;           /* Dialog units */
static int idCancel;                   /* ID to return when WM_CLOSE selected */

extern LOGFONT lfSysFont;

BOOL EXPORT WINAPI MsgBoxDlgProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT WINAPI IconProc( HWND, UINT, WPARAM, LPARAM );

#define WIDTHSTRING "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"

static char szOK[] = "OK";
static char szCancel[] = "Cancel";
static char szIgnore[] = "&Ignore";
static char szRetry[] = "&Retry";
static char szAbort[] = "&Abort";
static char szHelp[] = "&Help";
static char szYes[] = "&Yes";
static char szNo[] = "&No";
static char szAll[] = "&All";
   
int EXPORT WINAPI Adm_MsgBox( HWND hwnd, LPSTR lpcszStr, LPSTR lpcszTitle, UINT fuStyle, UINT fuStyleEx, WNDPROC lpfMsgProc )
{
   DLGTEMPLATE FAR * lpDlgTmp;
   HINSTANCE hInst;
   HGLOBAL hDlgTmp;
   LPSTR lp;
   TEXTMETRIC tm;
   HFONT hOldFont;
   SIZE size;
   LOGFONT lfMsgFont;
   HFONT hMsgFont;
   int nMaxWidth;
   int nRetCode;
   int cwButtonRow;
   int cbTemplate;
   int cxMsgBox;
   int cyMsgBox;
   int cItems;
   int dxLeft;
#ifdef WIN32
   NONCLIENTMETRICS nclm;
   int cbwSize;
   int cbwFontSize;
#endif
   HDC hdc;
   RECT rc;
   int x, y;

   /* The original code had a limitation - it did not show the special
    * system modal window if button style was not MB_OK, even though
    * the standard MessageBox() function permits more than one button
    * in a system modal message box. The following line has been fixed
    * to duplicate the behaviour of the MessageBox() function.
    */
   if( (fuStyle & MB_ICONHAND) && (fuStyle & MB_SYSTEMMODAL) &&
       fuStyleEx == 0 )
      return( MessageBox( hwnd, lpcszStr, lpcszTitle, fuStyle ) );

   nMaxWidth = GetSystemMetrics( SM_CXSCREEN ) / 2;

   /* Try to create the font using the system metrics provided by
    * Windows 95.
    */
   lfMsgFont = lfSysFont;
#ifdef WIN32
   nclm.cbSize = sizeof(NONCLIENTMETRICS);
   if( SystemParametersInfo( SPI_GETNONCLIENTMETRICS, 0, &nclm, 0 ) )
      lfMsgFont = nclm.lfMessageFont;
#endif
   hMsgFont = CreateFontIndirect( &lfMsgFont );

   hdc = GetDC( NULL);
   if( lfMsgFont.lfHeight < 0 )
      lfMsgFont.lfHeight = -MulDiv( lfMsgFont.lfHeight, 72, GetDeviceCaps( hdc, LOGPIXELSY ) );
   hOldFont = SelectObject( hdc, hMsgFont );
   SetRect( &rc, 0, 0, nMaxWidth, 0 );
   DrawText( hdc, lpcszStr, -1, &rc, DT_LEFT|DT_NOPREFIX|DT_WORDBREAK|DT_CALCRECT );
   rc.right += 10;

   GetTextMetrics( hdc, &tm );
   GetTextExtentPoint( hdc, WIDTHSTRING, 52, &size );
   wUnitsX = size.cx  / 52;
   wUnitsY = (WORD)tm.tmHeight;
   SelectObject( hdc, hOldFont );
   ReleaseDC( NULL, hdc );

   if( !lpcszTitle )
      lpcszTitle = "Error";

   cyMsgBox = rc.bottom + ( MSGBOXEX_SPACING * 2 );
   cxMsgBox = rc.right + ( MSGBOXEX_SPACING * 2 );
   dxLeft = MSGBOXEX_SPACING;
   hIcon = NULL;

#ifdef WIN32
   cbTemplate = sizeof( DLGTEMPLATE ) + sizeof( WORD ) + sizeof( WORD ) + sizeof( WORD );
   cbTemplate += ( cbwSize = MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, lpcszTitle, -1, NULL, 0 ) ) * 2;
   cbTemplate += ( cbwFontSize = MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, lfMsgFont.lfFaceName, -1, NULL, 0 ) ) * 2;
   while( cbTemplate & 0x03 )
      ++cbTemplate;
#else
   cbTemplate = sizeof( DLGTEMPLATE ) + lstrlen( lpcszTitle ) + sizeof( WORD ) + lstrlen( lfMsgFont.lfFaceName ) + 4;
#endif
   hDlgTmp = GlobalAlloc( GHND, cbTemplate );
   if( !hDlgTmp )
      return( 0 );

   lpDlgTmp = (DLGTEMPLATE FAR *)GlobalLock( hDlgTmp );
   if( !lpDlgTmp ) {
      GlobalFree( hDlgTmp );
      return( 0 );
      }

   lpDlgTmp->style = DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_SETFONT;
#ifdef WIN32
   lpDlgTmp->dwExtendedStyle = 0;
#endif
   lpDlgTmp->cdit = 0;
   lpDlgTmp->x = 0;
   lpDlgTmp->y = 0;
   lp = (LPSTR)(lpDlgTmp) + sizeof( DLGTEMPLATE );
#ifdef WIN32
   *((WORD FAR *)lp)++ = 0;
   *((WORD FAR *)lp)++ = 0;
   lp += MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, lpcszTitle, -1, (LPWSTR)lp, cbwSize ) * 2;
   *((WORD FAR *)lp)++ = (USHORT)lfMsgFont.lfHeight;
   lp += MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, lfMsgFont.lfFaceName, -1, (LPWSTR)lp, cbwFontSize ) * 2;
#else
   *lp++ = '\0';
   *lp++ = '\0';
   lstrcpy( lp, lpcszTitle );
   lp += lstrlen( lp ) + 1;
   *((WORD FAR *)lp)++ = lfMsgFont.lfHeight;
   lstrcpy( lp, lfMsgFont.lfFaceName );
#endif
   GlobalUnlock( hDlgTmp );

   if( fuStyle & MB_SYSTEMMODAL )
      lpDlgTmp->style |= DS_SYSMODAL;

   if( cyMsgBox < MSGBOXEX_MINHEIGHT )
      cyMsgBox = MSGBOXEX_MINHEIGHT;

   if( fuStyle & MB_ICONMASK ) {
      int cxIcon;
      int cyIcon;
      LPSTR lpIcon;

      switch( fuStyle & MB_ICONMASK )
         {
         default:
            lpIcon = (LPSTR)IDI_EXCLAMATION;
            break;
            
         case MB_ICONEXCLAMATION:
            lpIcon = (LPSTR)IDI_EXCLAMATION;
            break;

         case MB_ICONHAND:
            lpIcon = (LPSTR)IDI_HAND;
            break;

         case MB_ICONQUESTION:
            lpIcon = (LPSTR)IDI_QUESTION;
            break;

         case MB_ICONASTERISK:
            lpIcon = (LPSTR)IDI_ASTERISK;
            break;
         }

      hIcon = LoadIcon( NULL, lpIcon );

      cxIcon = GetSystemMetrics( SM_CXICON );
      cyIcon = GetSystemMetrics( SM_CYICON );
      dxLeft += MSGBOXEX_SPACING + cxIcon;
      cxMsgBox += MSGBOXEX_SPACING + cxIcon;
      y = ( cyMsgBox - cyIcon ) / 2;

      cbTemplate += AddDlgItem( hDlgTmp, cbTemplate,
            MSGBOXEX_SPACING, y, cxIcon, cyIcon,
            IDD_ICON, WS_CHILD|SS_ICON, CLA_STATIC, "" );
      }

   y = ( cyMsgBox - rc.bottom ) / 2;
   cbTemplate += AddDlgItem( hDlgTmp, cbTemplate,
            dxLeft, y, rc.right, rc.bottom, IDD_TEXT,
            WS_CHILD|SS_LEFT|SS_NOPREFIX, CLA_STATIC, lpcszStr );

   switch( fuStyle & MB_TYPEMASK )
      {
      default:             cItems = 0; break;
      case MB_OK:             cItems = 1; break;
      case MB_OKCANCEL:       cItems = 2; break;
      case MB_YESNO:          cItems = 2; break;
      case MB_YESNOCANCEL:    cItems = 3; break;
      case MB_YESNOALL:       cItems = 3; break;
      case MB_YESNOALLCANCEL:    cItems = 4; break;
      case MB_ABORTRETRYIGNORE:  cItems = 3; break;
      case MB_RETRYCANCEL:    cItems = 2; break;
      }
   if( fuStyleEx & MB_EX_HELP )
      ++cItems;

   cwButtonRow = MSGBOXEX_BUTTONWIDTH * cItems + ( MSGBOXEX_BUTTONSPACING * ( cItems - 1 ) );
   if( cwButtonRow > cxMsgBox )
      cxMsgBox = cwButtonRow;
   cxMsgBox += MSGBOXEX_SPACING;

   lpfnHookProc = lpfMsgProc;

   nDefButton = 1;
   switch( fuStyle & MB_DEFMASK ) {
      case MB_DEFBUTTON1:  nDefButton = 1;   break;
      case MB_DEFBUTTON2:  nDefButton = 2;   break;
      case MB_DEFBUTTON3:  nDefButton = 3;   break;
      }
   if( fuStyleEx & MB_EX_DEFBUTTON4 )
      nDefButton = 4;

   x = ( cxMsgBox - cwButtonRow ) / 2;
   switch( fuStyle & MB_TYPEMASK ) {
      case MB_OK:
         cbTemplate += AddButtonItem( hDlgTmp,
               cbTemplate, x, cyMsgBox, IDOK, szOK );
         idCancel = IDOK;
         break;

      case MB_OKCANCEL:
         cbTemplate += AddButtonItem( hDlgTmp,
               cbTemplate, x, cyMsgBox, IDOK, szOK );
         cbTemplate += AddButtonItem( hDlgTmp,
               cbTemplate, x += MSGBOXEX_BUTTONSPACING +
               MSGBOXEX_BUTTONWIDTH, cyMsgBox, IDCANCEL,
               szCancel );
         idCancel = IDCANCEL;
         break;

      case MB_YESNO:
         cbTemplate += AddButtonItem( hDlgTmp,
               cbTemplate, x, cyMsgBox, IDYES, szYes );
         cbTemplate += AddButtonItem( hDlgTmp,
               cbTemplate, x += MSGBOXEX_BUTTONSPACING +
               MSGBOXEX_BUTTONWIDTH, cyMsgBox, IDNO,
               szNo );
         idCancel = IDNO;
         break;

      case MB_YESNOCANCEL:
         cbTemplate += AddButtonItem( hDlgTmp,
               cbTemplate, x, cyMsgBox, IDYES, szYes );
         cbTemplate += AddButtonItem( hDlgTmp,
               cbTemplate, x += MSGBOXEX_BUTTONSPACING +
               MSGBOXEX_BUTTONWIDTH, cyMsgBox, IDNO, szNo );
         cbTemplate += AddButtonItem( hDlgTmp,
               cbTemplate, x += MSGBOXEX_BUTTONSPACING +
               MSGBOXEX_BUTTONWIDTH, cyMsgBox, IDCANCEL,
               szCancel );
         idCancel = IDCANCEL;
         break;

      case MB_YESNOALL:
         cbTemplate += AddButtonItem( hDlgTmp,
               cbTemplate, x, cyMsgBox, IDYES, szYes );
         cbTemplate += AddButtonItem( hDlgTmp,
               cbTemplate, x += MSGBOXEX_BUTTONSPACING +
               MSGBOXEX_BUTTONWIDTH, cyMsgBox, IDNO, szNo );
         cbTemplate += AddButtonItem( hDlgTmp,
               cbTemplate, x += MSGBOXEX_BUTTONSPACING +
               MSGBOXEX_BUTTONWIDTH, cyMsgBox, IDALL,
               szAll );
         idCancel = IDNO;
         break;

      case MB_YESNOALLCANCEL:
         cbTemplate += AddButtonItem( hDlgTmp,
               cbTemplate, x, cyMsgBox, IDYES, szYes );
         cbTemplate += AddButtonItem( hDlgTmp,
               cbTemplate, x += MSGBOXEX_BUTTONSPACING +
               MSGBOXEX_BUTTONWIDTH, cyMsgBox, IDNO, szNo );
         cbTemplate += AddButtonItem( hDlgTmp,
               cbTemplate, x += MSGBOXEX_BUTTONSPACING +
               MSGBOXEX_BUTTONWIDTH, cyMsgBox, IDALL,
               szAll );
         cbTemplate += AddButtonItem( hDlgTmp,
               cbTemplate, x += MSGBOXEX_BUTTONSPACING +
               MSGBOXEX_BUTTONWIDTH, cyMsgBox, IDCANCEL,
               szCancel );
         idCancel = IDCANCEL;
         break;

      case MB_ABORTRETRYIGNORE:
         cbTemplate += AddButtonItem( hDlgTmp,
               cbTemplate, x, cyMsgBox, IDABORT,
               szAbort );
         cbTemplate += AddButtonItem( hDlgTmp,
               cbTemplate, x += MSGBOXEX_BUTTONSPACING +
               MSGBOXEX_BUTTONWIDTH, cyMsgBox, IDRETRY,
               szRetry );
         cbTemplate += AddButtonItem( hDlgTmp,
               cbTemplate, x += MSGBOXEX_BUTTONSPACING +
               MSGBOXEX_BUTTONWIDTH, cyMsgBox, IDIGNORE,
               szIgnore );
         idCancel = IDABORT;
         break;

      case MB_RETRYCANCEL:
         cbTemplate += AddButtonItem( hDlgTmp,
               cbTemplate, x, cyMsgBox, IDRETRY,
               szRetry );
         cbTemplate += AddButtonItem( hDlgTmp,
               cbTemplate, x += MSGBOXEX_BUTTONSPACING +
               MSGBOXEX_BUTTONWIDTH, cyMsgBox,
               IDCANCEL, szCancel );
         idCancel = IDCANCEL;
         break;
      }

   if( fuStyleEx & MB_EX_HELP )
      cbTemplate += AddButtonItem( hDlgTmp, cbTemplate, x += MSGBOXEX_BUTTONSPACING + MSGBOXEX_BUTTONWIDTH,
            cyMsgBox, IDEXHELP, szHelp );

   lpDlgTmp = (DLGTEMPLATE FAR *)GlobalLock( hDlgTmp );
   if( !lpDlgTmp ) {
      GlobalFree( hDlgTmp );
      return( 0 );
      }

   cyMsgBox += MSGBOXEX_BOTTOMMARGIN + MSGBOXEX_BUTTONHEIGHT;
   cyMsgBox = ( cyMsgBox * 8 ) / wUnitsY;
   cxMsgBox = ( cxMsgBox * 4 ) / wUnitsX;

   lpDlgTmp->cx = cxMsgBox;
   lpDlgTmp->cy = cyMsgBox;
   GlobalUnlock( hDlgTmp );

   /* Note: The following code was added after the article went to press. It
    * handles the case where the user specifies a NULL window handle as the
    * parent. Because we need an instance handle, and we can't get one from
    * a NULL window handle, we cheat and use an undocumented scheme to
    * extract the instance handle from the task database of the current
    * task. The current task will _always_ be the application that called
    * this MessageBoxEx() function.
    */
#ifndef WIN32
   if( hwnd == NULL ) {
      HTASK hTask;

      hTask = GetCurrentTask();
      hInst = *((HINSTANCE _far *)MAKELONG( 0x1C, hTask ) );
      }
   else
      hInst = (HINSTANCE)GetWindowWord( hwnd, GWW_HINSTANCE );
#else
   hInst = (HINSTANCE)GetWindowLong( hwnd, GWL_HINSTANCE );
#endif

   /* Alternative approach - SWP
    *  Get hInst using the following code:
    *
    *    hInst = (HANDLE)( ( (DWORD)(LPSTR)&nDefButton >> 16 ) & 0xFFFF );
    *
    *  The instance handle is the segment of the application data segment
    *  (or at least it is in Windows 3.1 - it _won't_ be in Windows/NT.
    *
    * Better approach - pass the instance handle as a parameter to this
    * function.
    */
#ifdef WIN32
   lpDlgTmp = (DLGTEMPLATE FAR *)GlobalLock( hDlgTmp );
   nRetCode = DialogBoxIndirect( hInst, lpDlgTmp, hwnd, MsgBoxDlgProc );
   GlobalUnlock( hDlgTmp );
#else
   nRetCode = DialogBoxIndirect( hInst, hDlgTmp, hwnd, MsgBoxDlgProc );
#endif
   GlobalFree( hDlgTmp );
   DeleteFont( hMsgFont );
   return( nRetCode );
}

int FASTCALL AddButtonItem( HANDLE hDlgTmp, int cbTemplate, int x, int y, int id, LPSTR lpszStr )
{
   long lStyle;

   lStyle = WS_CHILD | WS_TABSTOP | ( --nDefButton ? BS_PUSHBUTTON : BS_DEFPUSHBUTTON );
   if( nDefButton == 0 )
      idDefButton = id;
   return( AddDlgItem( hDlgTmp, cbTemplate, x, y, MSGBOXEX_BUTTONWIDTH, MSGBOXEX_BUTTONHEIGHT, id, lStyle, CLA_BUTTON, lpszStr ) );
}

BOOL EXPORT WINAPI MsgBoxDlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   if( lpfnHookProc != NULL )
      {
      LONG lResult;

      lResult = (*lpfnHookProc)( hwnd, message, wParam, lParam );
      if( lResult && message != WM_INITDIALOG )
         return( (BOOL)lResult );
      }
   switch( message )
      {
      case WM_INITDIALOG: {
#ifdef NOUSEMM
         RECT rc;
#endif NOUSEMM

         if( hIcon ) {
            HWND hwndIcon;

            hwndIcon = GetDlgItem( hwnd, IDD_ICON );
            SetWindowLong( hwndIcon, GWL_WNDPROC, (LONG)IconProc );
            }
#ifdef NOUSEMM
         GetWindowRect( hwnd, &rc );
         SetWindowPos( hwnd, NULL, ( GetSystemMetrics( SM_CXSCREEN ) - ( rc.right - rc.left ) ) / 2,
               ( GetSystemMetrics( SM_CYSCREEN ) - ( rc.bottom - rc.top ) ) / 3, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE );
#else NOUSEMM
         CenterWindowToMonitor(hwnd, hwnd, TRUE);
#endif NOUSEMM

         SetFocus( GetDlgItem( hwnd, idDefButton ) );
         return( FALSE );
         }

      case WM_COMMAND:
         if( wParam != IDEXHELP )
            {
            if( wParam == IDCANCEL )
               wParam = idCancel;
            EndDialog( hwnd, wParam );
            }
         break;

      default:
         return( FALSE );
      }
   return( FALSE );
}

LRESULT EXPORT WINAPI IconProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   if( message == WM_PAINT )
      {
      PAINTSTRUCT ps;
      HDC hdc;

      hdc = BeginPaint( hwnd, &ps );
      DrawIcon( hdc, 0, 0, hIcon );
      EndPaint( hwnd, &ps );
      }
   return( FALSE );
}

int FASTCALL AddDlgItem( HANDLE hDlgTmp, int cbTemplate,
   int x, int y, int cx, int cy,
   int id, long style, WORD wClass,
   LPSTR lpdtilText )
{
   DLGTEMPLATE FAR * lpDlgTmp;
   DLGITEMTEMPLATE FAR * lpDlgItemTmp;
   int cbItem;
   LPSTR lpstr;
#ifdef WIN32
   int cbwTextSize;
#endif

#ifdef WIN32
   cbItem = sizeof( DLGITEMTEMPLATE ) + sizeof( WORD );
   cbItem += sizeof( WORD ) * 2;
   cbItem += ( cbwTextSize = MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, lpdtilText, -1, NULL, 0 ) ) * 2;
   while( cbItem & 0x03 )
      ++cbItem;
#else
   cbItem = sizeof( DLGITEMTEMPLATE ) + 1 + lstrlen( lpdtilText ) + 2;
#endif

   hDlgTmp = GlobalReAlloc( hDlgTmp, cbTemplate + cbItem, GMEM_MOVEABLE );
   lpDlgTmp = (DLGTEMPLATE FAR *)GlobalLock( hDlgTmp );

   ++lpDlgTmp->cdit;

   y = ( y * 8 ) / wUnitsY;
   x = ( x * 4 ) / wUnitsX;
   cy = ( cy * 8 ) / wUnitsY;
   cx = ( cx * 4 ) / wUnitsX;

   lpDlgItemTmp = (DLGITEMTEMPLATE FAR *)( (LPSTR)lpDlgTmp + cbTemplate );
   lpDlgItemTmp->x = x;
   lpDlgItemTmp->y = y;
   lpDlgItemTmp->cx = cx;
   lpDlgItemTmp->cy = cy;
   lpDlgItemTmp->id = id;
   lpDlgItemTmp->style = style | WS_VISIBLE;
#ifdef WIN32
   lpDlgItemTmp->dwExtendedStyle = 0L;
#endif

   /* Copy in the class name. */
   lpstr = (LPSTR)(lpDlgItemTmp) + sizeof( DLGITEMTEMPLATE );
#ifdef WIN32
   *((WORD FAR *)lpstr)++ = 0xFFFF;
   *((WORD FAR *)lpstr)++ = wClass;
   lpstr += MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, lpdtilText, -1, (LPWSTR)lpstr, cbwTextSize ) * 2;
   *((WORD FAR *)lpstr)++ = 0;
#else
   *lpstr++ = LOBYTE( wClass );
   lstrcpy( lpstr, lpdtilText );
   lpstr += lstrlen( lpdtilText ) + 1;
   *lpstr = 0;
#endif

   GlobalUnlock( hDlgTmp );
   return( cbItem );
}
