/* HOTKEY.C - Implements the hotkey control
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
#include "kludge.h"
#include "amctrls.h"
#include "amctrli.h"

#define  THIS_FILE   __FILE__

#ifdef WIN32
#define  ST_UINT           (sizeof(DWORD))
#define  GetStdUnit        GetWindowLong
#define  SetStdUnit        SetWindowLong
#else
#define  ST_UINT           (sizeof(WORD))
#define  GetStdUnit        GetWindowWord
#define  SetStdUnit        SetWindowWord
#endif

#define  GW_COMBINV     (0*ST_UINT)
#define  GW_MODINV      (1*ST_UINT)
#define  GW_VIRTKEY     (2*ST_UINT)
#define  GW_MODIFIER    (3*ST_UINT)
#define  GW_HFONT       (4*ST_UINT)

struct { WORD wKey; char * szKeyName; } KeyTable[] = {
   VK_BACK,       "Backspace",
   VK_TAB,        "Tab",
   VK_RETURN,     "Enter",
   VK_PAUSE,      "Pause",
   VK_ESCAPE,     "Esc",
   VK_SPACE,      "Spacebar",
   VK_PRIOR,      "PgUp",
   VK_NEXT,       "Next",
   VK_END,        "End",
   VK_HOME,       "Home",
   VK_LEFT,       "Left",
   VK_UP,         "Up",
   VK_RIGHT,      "Right",
   VK_DOWN,       "Down",
   VK_PRINT,      "PrtSc",
   VK_INSERT,     "Ins",
   VK_DELETE,     "Del",
   VK_NUMPAD0,    "Num0",
   VK_NUMPAD1,    "Num1",
   VK_NUMPAD2,    "Num2",
   VK_NUMPAD3,    "Num3",
   VK_NUMPAD4,    "Num4",
   VK_NUMPAD5,    "Num5",
   VK_NUMPAD6,    "Num6",
   VK_NUMPAD7,    "Num7",
   VK_NUMPAD8,    "Num8",
   VK_NUMPAD9,    "Num9",
   VK_MULTIPLY,   "*",
   VK_ADD,        "+",
   VK_SEPARATOR,  "=",
   VK_SUBTRACT,   "-",
   VK_DECIMAL,    ".",
   VK_DIVIDE,     "/",
   VK_F1,         "F1",
   VK_F2,         "F2",
   VK_F3,         "F3",
   VK_F4,         "F4",
   VK_F5,         "F5",
   VK_F6,         "F6",
   VK_F7,         "F7",
   VK_F8,         "F8",
   VK_F9,         "F9",
   VK_F10,        "F10",
   VK_F11,        "F11",
   VK_F12,        "F12",
   VK_NUMLOCK,    "NumLock",
   VK_SCROLL,     "Scroll"
   };

#define cbKeyTable   (sizeof(KeyTable)/sizeof(KeyTable[0]))

WORD mapHotkeyFHkComb[] = { HKCOMB_NONE, HKCOMB_S, HKCOMB_C, HKCOMB_SC, HKCOMB_A, HKCOMB_SA, HKCOMB_CA, HKCOMB_SCA };

static void NEAR PASCAL DrawHotkeyControl( HWND );
LRESULT EXPORT CALLBACK HotkeyWndFn( HWND, UINT, WPARAM, LPARAM );

/* This function registers the hotkey class.
 */
BOOL FASTCALL RegisterHotkeyClass( HINSTANCE hInst )
{
   WNDCLASS wc;

   wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
   wc.hIcon          = NULL;
   wc.lpszMenuName   = NULL;
   wc.lpszClassName  = HOTKEY_CLASS;
   wc.hbrBackground  = (HBRUSH)( COLOR_WINDOW+1 );
   wc.hInstance      = hInst;
   wc.style          = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS | CS_GLOBALCLASS;
   wc.lpfnWndProc    = HotkeyWndFn;
   wc.cbClsExtra     = 0;
   wc.cbWndExtra     = 5 * ST_UINT;
   return( RegisterClass( &wc ) );
}
#pragma warning( default:4704 )

LRESULT EXPORT CALLBACK HotkeyWndFn( HWND hwnd,  UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      case WM_NCCREATE: {
         CREATESTRUCT FAR * lpcs;

         lpcs = (CREATESTRUCT FAR *)lParam;
         lpcs->dwExStyle |= WS_EX_CLIENTEDGE;
         break;
         }

      case WM_WINDOWPOSCHANGING: {
         HWND hwndParent;
         RECT rc;

         GetWindowRect( hwnd, &rc );
         InflateRect( &rc, 1, 1 );
         hwndParent = GetParent( hwnd );
         ASSERT( IsWindow( hwndParent ) );
         ScreenToClient( hwndParent, (LPPOINT)&rc );
         ScreenToClient( hwndParent, ((LPPOINT)&rc) + 1 );
         InvalidateRect( hwndParent, &rc, FALSE );
         break;
         }

      case WM_CREATE:
         SetStdUnit( hwnd, GW_COMBINV, HKCOMB_A|HKCOMB_C|HKCOMB_S|HKCOMB_NONE );
         SetStdUnit( hwnd, GW_MODINV, HOTKEYF_CONTROL|HOTKEYF_ALT );
         SetStdUnit( hwnd, GW_VIRTKEY, 0 );
         SetStdUnit( hwnd, GW_MODIFIER, 0 );
         SetStdUnit( hwnd, GW_HFONT, (WPARAM)GetStockObject( SYSTEM_FONT ) );
         break;

      case WM_ERASEBKGND: {
         LRESULT lRetCode;

         HideCaret( hwnd );
         lRetCode = DefWindowProc( hwnd, message, wParam, lParam );
         ShowCaret( hwnd );
         return( lRetCode );
         }

      case WM_LBUTTONDOWN:
         SetFocus( hwnd );
         break;

      case WM_SETFOCUS: {
         int nHeight;
         RECT rc;

         /* Each time we get the focus, we have to recreate
          * the caret based on the height of the control.
          */
         GetClientRect( hwnd, &rc );
         nHeight = ( rc.bottom - rc.top ) - 7;
         CreateCaret( hwnd, NULL, 1, nHeight );
         ShowCaret( hwnd );
         DrawHotkeyControl( hwnd );
         break;
         }

      case WM_KILLFOCUS:
         HideCaret( hwnd );
         DestroyCaret();
         break;

      case WM_KEYUP:
      case WM_SYSKEYUP:
      case WM_SYSCHAR:
      case WM_CHAR:
         if( 0 == GetStdUnit( hwnd, GW_VIRTKEY ) )
            {
            SetStdUnit( hwnd, GW_MODIFIER, 0 );
            DrawHotkeyControl( hwnd );
            }
         return( 0L );

      case WM_SYSKEYDOWN:
      case WM_KEYDOWN: {
         NMHDR nmh;
         WORD wModInv;
         BOOL fUpdate;

         switch( wParam )
            {
            case VK_SPACE:
            case VK_DELETE:
            case VK_BACK:
               if( 0 != GetStdUnit( hwnd, GW_VIRTKEY ) )
                  {
                  SetStdUnit( hwnd, GW_VIRTKEY, 0 );
                  DrawHotkeyControl( hwnd );
                  Amctl_SendNotify( GetParent( hwnd ), hwnd, HCN_SELCHANGED, &nmh );
                  break;
                  }
               goto ProcKey;

            case VK_TAB:
               if( 0 == GetStdUnit( hwnd, GW_VIRTKEY ) )
                  {
                  SetStdUnit( hwnd, GW_MODIFIER, 0 );
                  DrawHotkeyControl( hwnd );
                  Amctl_SendNotify( GetParent( hwnd ), hwnd, HCN_SELCHANGED, &nmh );
                  }

            case VK_RETURN:
            case VK_ESCAPE:
               return( DefWindowProc( hwnd, message, wParam, lParam ) );

            default:
ProcKey:       wModInv = 0;
               fUpdate = FALSE;
               if( GetKeyState( VK_SHIFT ) & 0x8000 )
                  wModInv |= HOTKEYF_SHIFT;
               if( GetKeyState( VK_CONTROL ) & 0x8000 )
                  wModInv |= HOTKEYF_CONTROL;
               if( GetKeyState( VK_MENU ) & 0x8000 )
                  wModInv |= HOTKEYF_ALT;
               if( 0 != GetStdUnit( hwnd, GW_COMBINV ) )
                  if( mapHotkeyFHkComb[ wModInv ] & GetStdUnit( hwnd, GW_COMBINV ) )
                     wModInv |= GetStdUnit( hwnd, GW_MODINV );
               if( wModInv != GetStdUnit( hwnd, GW_MODIFIER ) )
                  {
                  SetStdUnit( hwnd, GW_MODIFIER, wModInv );
                  SetStdUnit( hwnd, GW_VIRTKEY, 0 );
                  fUpdate = TRUE;
                  }
               if( wParam != VK_SHIFT && wParam != VK_CONTROL && wParam != VK_MENU )
                  {
                  if( (WPARAM)GetStdUnit( hwnd, GW_VIRTKEY ) != wParam )
                     {
                     SetStdUnit( hwnd, GW_VIRTKEY, wParam );
                     fUpdate = TRUE;
                     }
                  }
               if( fUpdate )
                  {
                  DrawHotkeyControl( hwnd );
                  Amctl_SendNotify( GetParent( hwnd ), hwnd, HCN_SELCHANGED, &nmh );
                  }
               return( 0L );
            }
         break;
         }

      case WM_GETDLGCODE:
         return( DLGC_WANTCHARS|DLGC_WANTARROWS );

      case WM_GETFONT:
         return( GetStdUnit( hwnd, GW_HFONT ) );

      case WM_SETFONT:
         SetStdUnit( hwnd, GW_HFONT, wParam );
         if( LOWORD( lParam ) )
            DrawHotkeyControl( hwnd );
         return( 0L );

      case HKM_SETRULES:
         SetStdUnit( hwnd, GW_COMBINV, wParam );
         SetStdUnit( hwnd, GW_MODINV, LOWORD( lParam ) );
         return( 0L );

      case HKM_SETHOTKEY:
         SetStdUnit( hwnd, GW_VIRTKEY, LOBYTE( wParam ) );
         SetStdUnit( hwnd, GW_MODIFIER, HIBYTE( wParam ) );
         DrawHotkeyControl( hwnd );
         return( 0L );

      case HKM_GETHOTKEY:
         return( MAKEWORD( GetStdUnit( hwnd, GW_VIRTKEY ), GetStdUnit( hwnd, GW_MODIFIER ) ) );

      case WM_PAINT: {
         PAINTSTRUCT ps;
         HDC hdc;

         hdc = BeginPaint( hwnd, &ps );
         DrawHotkeyControl( hwnd );
         EndPaint( hwnd, &ps );
         break;
         }
      }
   return( DefWindowProc( hwnd, message, wParam, lParam ) );
}

/* This function redraws the hotkey control after it has changed.
 */
static void NEAR PASCAL DrawHotkeyControl( HWND hwnd )
{
   HFONT hOldFont;
   char sz[ 60 ];
   WORD wModifier;
   WORD wVirtKey;
   HWND hwndParent;
   BOOL fHasCaret;
   HBRUSH hbr;
   RECT rc;
   HDC hdc;

   /* Do we have the focus? If so, hide the caret and remember
    * to update it's position after we've drawn the next text.
    */
   if( fHasCaret = ( GetFocus() == hwnd ) )
      HideCaret( hwnd );

   /* This code draws the 3D frame rectangle. Because
    * the outside edge of the 3D frame is outside the
    * control dimensions, we have to use the parent DC to
    * draw the frame.
    */
   hwndParent = GetParent( hwnd );
   ASSERT( IsWindow( hwndParent ) );
   hdc = GetDC( hwndParent );
   ASSERT( NULL != hdc );
   GetWindowRect( hwnd, &rc );
   ScreenToClient( hwndParent, (LPPOINT)&rc );
   ScreenToClient( hwndParent, ((LPPOINT)&rc) + 1 );
   InflateRect( &rc, 1, 1 );
   Draw3DFrame( hdc, &rc );

   /* Work out the key name based on the current modifier
    * and virtual key settings.
    */
   wModifier = (WORD)GetStdUnit( hwnd, GW_MODIFIER );
   wVirtKey = (WORD)GetStdUnit( hwnd, GW_VIRTKEY );
   if( 0 == wVirtKey && 0 == wModifier )
      lstrcpy( sz, "None" );
   else
      {
      sz[ 0 ] = '\0';
      if( wModifier & HOTKEYF_CONTROL )
         lstrcat( sz, "Ctrl + " );
      if( wModifier & HOTKEYF_ALT )
         lstrcat( sz, "Alt + " );
      if( wModifier & HOTKEYF_SHIFT )
         lstrcat( sz, "Shift + " );
      Amctl_NewGetKeyNameText( wVirtKey, sz + lstrlen( sz ), 38 );
      }

   /* Blank out the control.
    */
   hbr = CreateSolidBrush( GetSysColor( COLOR_WINDOW ) );
   FillRect( hdc, &rc, hbr );
   DeleteBrush( hbr );

   /* Draw the text in the control. If we have the focus, update
    * the caret to appear after the end of the text.
    */
   hOldFont = SelectFont( hdc, GetStdUnit( hwnd, GW_HFONT ) );
   SetTextColor( hdc, GetSysColor( COLOR_WINDOWTEXT ) );
   SetBkColor( hdc, GetSysColor( COLOR_WINDOW ) );
   DrawText( hdc, sz, -1, &rc, DT_LEFT|DT_SINGLELINE|DT_VCENTER );
   GetClientRect( hwnd, &rc );
   DrawText( hdc, sz, -1, &rc, DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_CALCRECT );
   if( fHasCaret )
      {
      SetCaretPos( rc.right + 1, rc.top + 2 );
      ShowCaret( hwnd );
      }
   SelectObject( hdc, hOldFont );
   ReleaseDC( hwndParent, hdc );
}

/* This function draws a standard 3D frame. It's used by other
 * functions in this DLL, so it has to be as generic as possible.
 * On return, it adjusts the rectangle coordinates so that they
 * point into the frame.
 */
void FASTCALL Draw3DFrame( HDC hdc, RECT FAR * lprc )
{
   HPEN hpen;
   HPEN hpenGray;
   HPEN hpenBlack;
   HPEN hpenLtGray;
   HPEN hpenWhite;

   /* Create some pens.
    */
   hpenGray = CreatePen( PS_SOLID, 0, GetSysColor( COLOR_BTNSHADOW ) );
   hpenBlack = CreatePen( PS_SOLID, 0, RGB( 0, 0, 0 ) );
   hpenLtGray = CreatePen( PS_SOLID, 0, GetSysColor( COLOR_BTNFACE ) );
   hpenWhite = CreatePen( PS_SOLID, 0, GetSysColor( COLOR_BTNHIGHLIGHT ) );
#ifndef WIN32
   ASSERT( NULL != hpenGray );
   ASSERT( NULL != hpenBlack );
   ASSERT( NULL != hpenLtGray );
   ASSERT( NULL != hpenWhite );
#endif

   /* Draw the left and bottom outside edge.
    */
   hpen = SelectPen( hdc, hpenGray );
   MoveToEx( hdc, lprc->left, lprc->bottom - 2, NULL );
   LineTo( hdc, lprc->left, lprc->top );
   LineTo( hdc, lprc->right - 1, lprc->top );

   /* Draw the left and bottom inside edge.
    */
   SelectPen( hdc, hpenBlack );
   MoveToEx( hdc, lprc->left + 1, lprc->bottom - 3, NULL );
   LineTo( hdc, lprc->left + 1, lprc->top + 1 );
   LineTo( hdc, lprc->right - 2, lprc->top + 1 );

   /* Draw the top and right outside edge.
    */
   SelectPen( hdc, hpenWhite );
   MoveToEx( hdc, lprc->left, lprc->bottom - 1, NULL );
   LineTo( hdc, lprc->right - 1, lprc->bottom - 1 );
   LineTo( hdc, lprc->right - 1, lprc->top - 1 );

   /* Draw the top and right inside edge.
    */
   SelectPen( hdc, hpenLtGray );
   MoveToEx( hdc, lprc->left + 1, lprc->bottom - 2, NULL );
   LineTo( hdc, lprc->right - 2, lprc->bottom - 2 );
   LineTo( hdc, lprc->right - 2, lprc->top );

   /* Clean up when we've finished.
    */
   SelectPen( hdc, hpen );
   DeletePen( hpenGray );
   DeletePen( hpenBlack );
   DeletePen( hpenLtGray );
   DeletePen( hpenWhite );
   lprc->left += 2;
   lprc->top += 2;
   lprc->right -= 2;
   lprc->bottom -= 2;
}

/* This function converts a virtual key name into a description of
 * that key.
 */
WINCOMMCTRLAPI void WINAPI EXPORT Amctl_NewGetKeyNameText( WORD wKey, LPSTR lpszBuffer, UINT cbszMaxKey )
{
   register int c;
   BOOL fGotcha = FALSE;

   for( c = 0; c < cbKeyTable; ++c )
   {
      if( wKey == KeyTable[ c ].wKey )
         {
         lstrcpyn( lpszBuffer, KeyTable[ c ].szKeyName, cbszMaxKey );
         lpszBuffer[ cbszMaxKey ] = '\0';
         fGotcha = TRUE;
         break;
         }
   }

   if( !fGotcha )
   {
      wKey = MapVirtualKey( wKey, 2 );
      if( wKey >= '!' && wKey <= '~' )
         wsprintf( lpszBuffer, "%c", wKey );
   }



}
