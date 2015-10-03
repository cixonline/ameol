/* UDCTRL.C - Implements the up-down control
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
#include <string.h>
#include <ctype.h>
#include "amctrls.h"
#include "amctrli.h"

#define  GWW_EXTRA            4
#define  GetBlock(h)          ((UDMCTRL FAR *)GetWindowLong((h),0))
#define  PutBlock(h,b)        (SetWindowLong((h),0,(LONG)(b)))

#define  UDF_DOWN          1           /* Refers to the down or right button of the control */
#define  UDF_UP            2           /* Refers to the up or left button of the control */
#define  UDF_INCTRL        4           /* The mouse is within the button boundary */
#define  UDF_CLICKED       8           /* The button is pressed */
#define  UDF_UPDISABLED    16          /* The up control is disabled */
#define  UDF_DOWNDISABLED  32          /* The down control is disabled */
#define  UDF_DISABLED      48          /* The entire control is disabled */

#define  IDT_ACCELFIRST    1001        /* Timer ID for first spin check */
#define  IDT_ACCELNEXT     1002        /* Timer ID for subsequent spin checks */

#define  INIT_SPIN_DELAY   500         /* No. of ms before first spin check */
#define  SPIN_DELAY        50          /* No. of ms between spin checks */

typedef struct tagUDMCTRL {
   int nBase;                          /* Number based used in buddy control */
   int nPos;                           /* Current pos of up/down control */
   HWND hwndBuddy;                     /* Handle of buddy window */
   WNDPROC lpBuddyWndProc;             /* Saved pointer to buddy window old edit procedure */
   UINT wFlags;                        /* Control flags */
   int nUpper;                         /* Upper limit of position */
   int nLower;                         /* Lower limit of position */
   int cAccel;                         /* Number of accelerator entries in paAccels */
   UINT nSec;                          /* Number of seconds since start of spin */
   int nSpinRate;                      /* Rate of spin (1 second/number of ticks) */
   UINT nTicks;                        /* Number of ticks since last second */
   int nAccels;                        /* Index into accelerator table of current entry */
   UDACCEL FAR * paAccels;             /* Pointer to accelerator table */
} UDMCTRL;

static char szThousands[ 2 ];

/* Macros to allow sending explicit WM_VSCROLL messages
 */
#ifdef WIN32
#define  SendVScroll(hwnd, scrollCode, nPos, hwndCtl) \
   SendMessage((hwnd), WM_VSCROLL, MAKELPARAM((scrollCode),(nPos)),(LPARAM)(hwndCtl))
#else
#define  SendVScroll(hwnd, scrollCode, nPos, hwndCtl) \
   SendMessage((hwnd), WM_VSCROLL, (WPARAM)(scrollCode), MAKELPARAM((nPos), (hwndCtl)))
#endif

LRESULT EXPORT CALLBACK UpDownProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL DrawUpDown( HWND, HDC, int );
int FASTCALL AdjustUpDownPos( HWND, BOOL );
void FASTCALL DrawUpDownBuddy( HWND, UDMCTRL FAR * );
void FASTCALL ReadUpDownPos( HWND, UDMCTRL FAR * );
void FASTCALL HookBuddy( HWND, UDMCTRL FAR * );
void FASTCALL UnhookBuddy( HWND, UDMCTRL FAR * );
void FASTCALL SetBuddyPosition( HWND, HWND );

/* Registers the up-down window class.
 */
BOOL FASTCALL RegisterUpDownClass( HINSTANCE hInst )
{
   WNDCLASS wc;

   /* Get the character used to separate thousands in the
    * current locale.
    */
   GetProfileString( "intl", "sThousand", ",", szThousands, sizeof( szThousands ) );

   /* Register the up-down window class.
    */
   wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
   wc.hIcon          = NULL;
   wc.lpszMenuName   = NULL;
   wc.lpszClassName  = UPDOWN_CLASS;
   wc.hbrBackground  = (HBRUSH)( COLOR_BTNFACE+1 );
   wc.hInstance      = hInst;
   wc.style          = CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS;
   wc.lpfnWndProc    = (WNDPROC)UpDownProc;
   wc.cbClsExtra     = 0;
   wc.cbWndExtra     = GWW_EXTRA;
   return( RegisterClass( &wc ) );
}

/* This function provides a quick way to create an up-down control, set the range and position,
 * and associate it with an (optional) buddy window.
 */
WINCOMMCTRLAPI HWND WINAPI EXPORT Amctl_CreateUpDownControl( DWORD dwStyle, int x, int y, int cx, int cy, HWND hwndParent,
   int nID, HINSTANCE hInst, HWND hwndBuddy, int nUpper, int nLower, int nPos )
{
   HWND hwndUDCtrl;

   /* Must ALWAYS be a child window!
    */
   dwStyle |= WS_CHILD;
   hwndUDCtrl = CreateWindow( UPDOWN_CLASS,
                              "",
                              dwStyle,
                              x, y, cx, cy,
                              hwndParent,
                              (HMENU)nID,
                              hInst,
                              NULL );
   if( hwndUDCtrl )
      {
      if( hwndBuddy )
         SendMessage( hwndUDCtrl, UDM_SETBUDDY, (WPARAM)hwndBuddy, 0L );
      SendMessage( hwndUDCtrl, UDM_SETRANGE, 0, MAKELONG( nUpper, nLower ) );
      SendMessage( hwndUDCtrl, UDM_SETPOS, 0, MAKELONG( nPos, 0 ) );
      }
   return( hwndUDCtrl );
}

/* This function is called whenever there is a message for the buddy window. This is
 * the subclassed buddy control edit window procedure. All messages in which we have no
 * interest are passed onto the original buddy window procedure.
 *
 * The handle of the up-down control is stored within the buddy window as a property. From
 * this, we can extract the pointer to the up-down control header structure.
 */
LRESULT EXPORT CALLBACK UpDownBuddyProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   UDMCTRL FAR * pUdmCtrl;
   HWND hwndBuddy;

   hwndBuddy = GetProp( hwnd, "updown_buddy" );
   pUdmCtrl = GetBlock( hwndBuddy );
   switch( message )
      {
      case WM_KEYUP:
         switch( wParam )
            {
            case VK_DOWN:
            case VK_UP:
               /* The user has released the key, so draw the control
                * in the up state.
                */
               if( VK_DOWN == wParam )
                  DrawUpDown( hwndBuddy, NULL, pUdmCtrl->wFlags|UDF_DOWN );
               else
                  DrawUpDown( hwndBuddy, NULL, pUdmCtrl->wFlags|UDF_UP );
               return( 0L );
            }
         break;

      case WM_KEYDOWN:
         switch( wParam )
            {
            case VK_DOWN:
            case VK_UP: {
               int iDelta;

               /* The user has pressed a up or down key. Adjust the position
                * and update the value in the buddy control. To reduce flicker,
                * we use the 'repeat' bit in the lParam (bit 31) to indicate
                * whether or not the button is already down.
                */
               ReadUpDownPos( hwndBuddy, pUdmCtrl );
               pUdmCtrl->cAccel = 0;
               if( VK_DOWN == wParam )
                  {
                  iDelta = AdjustUpDownPos( hwndBuddy, TRUE );
                  if( !( HIWORD( lParam ) & 0x4000 ) )
                     DrawUpDown( hwndBuddy, NULL, pUdmCtrl->wFlags|UDF_DOWN|UDF_INCTRL );
                  }
               else
                  {
                  iDelta = AdjustUpDownPos( hwndBuddy, FALSE );
                  if( !( HIWORD( lParam ) & 0x4000 ) )
                     DrawUpDown( hwndBuddy, NULL, pUdmCtrl->wFlags|UDF_UP|UDF_INCTRL );
                  }
               if( iDelta )
                  DrawUpDownBuddy( hwndBuddy, pUdmCtrl );
               return( 0L );
               }
            }
         break;
      }
   return( CallWindowProc( pUdmCtrl->lpBuddyWndProc, hwnd, message, wParam, lParam ) );
}

/* This is the up-down control window procedure.
 */
LRESULT EXPORT CALLBACK UpDownProc( HWND hwnd,  UINT message, WPARAM wParam, LPARAM lParam )
{
   UDMCTRL FAR * pUdmCtrl;

#ifdef _DEBUG
   /* Help us to catch NULL pointers...
    */
   pUdmCtrl = NULL;
#endif
   switch( message )
      {
      case WM_CREATE: {
         static UDMCTRL FAR * pNewUdmCtrl;

         INITIALISE_PTR(pNewUdmCtrl);
         if( fNewMemory( &pNewUdmCtrl, sizeof( UDMCTRL ) ) )
            {
            CREATESTRUCT FAR * lpcs;

            lpcs = (CREATESTRUCT FAR *)lParam;
            pNewUdmCtrl->nBase = 10;
            pNewUdmCtrl->nPos = 0;
            pNewUdmCtrl->lpBuddyWndProc = NULL;
            if( lpcs->style & UDS_AUTOBUDDY )
               {
               pNewUdmCtrl->hwndBuddy = GetWindow( hwnd, GW_HWNDPREV );
               if( lpcs->style & UDS_ARROWKEYS )
                  HookBuddy( hwnd, pNewUdmCtrl );
               }
            else
               pNewUdmCtrl->hwndBuddy = NULL;
            if( pNewUdmCtrl->hwndBuddy )
               SetBuddyPosition( hwnd, pNewUdmCtrl->hwndBuddy );
            else if( !( lpcs->style & UDS_HORZ ) )
               {
               int iWidth;
               int iHeight;

               iWidth = GetSystemMetrics( SM_CXVSCROLL ) - 1;
               iHeight = lpcs->cy;
               SetWindowPos( hwnd, NULL, 0, 0, iWidth, iHeight, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE );
               }
            pNewUdmCtrl->nUpper = 100;
            pNewUdmCtrl->nLower = 0;
            pNewUdmCtrl->nAccels = 0;
            pNewUdmCtrl->cAccel = 0;
            pNewUdmCtrl->wFlags = UDF_UPDISABLED;
            INITIALISE_PTR(pNewUdmCtrl->paAccels);
            if( fNewMemory( &pNewUdmCtrl->paAccels, 3 * sizeof( UDACCEL ) ) )
               {
               pNewUdmCtrl->paAccels[ 0 ].nSec = 0;
               pNewUdmCtrl->paAccels[ 0 ].nInc = 1;
               pNewUdmCtrl->paAccels[ 1 ].nSec = 2;
               pNewUdmCtrl->paAccels[ 1 ].nInc = 5;
               pNewUdmCtrl->paAccels[ 2 ].nSec = 5;
               pNewUdmCtrl->paAccels[ 2 ].nInc = 20;
               pNewUdmCtrl->nAccels = 3;
               }
            PutBlock( hwnd, pNewUdmCtrl );
            }
         return( pNewUdmCtrl ? 0 : -1 );
         }

      case WM_DESTROY:
         pUdmCtrl = GetBlock( hwnd );
         UnhookBuddy( hwnd, pUdmCtrl );
         if( pUdmCtrl->paAccels )
            FreeMemory( &pUdmCtrl->paAccels );
         FreeMemory( &pUdmCtrl );
         break;

      case WM_ENABLE:
         pUdmCtrl = GetBlock( hwnd );
         if( wParam )
            pUdmCtrl->wFlags &= ~UDF_DISABLED;
         else
            pUdmCtrl->wFlags |= UDF_DISABLED;
         InvalidateRect( hwnd, NULL, TRUE );
         UpdateWindow( hwnd );
         break;

      case UDM_SETBUDDY: {
         HWND hwndPrevBuddy;
         HWND hwndBuddy;

         hwndPrevBuddy = NULL;
         pUdmCtrl = GetBlock( hwnd );
         hwndPrevBuddy = pUdmCtrl->hwndBuddy;
         UnhookBuddy( hwnd, pUdmCtrl );
         hwndBuddy = pUdmCtrl->hwndBuddy = (HWND)wParam;
         if( GetWindowStyle( hwnd ) & UDS_ARROWKEYS )
            HookBuddy( hwnd, pUdmCtrl );
         SetBuddyPosition( hwnd, hwndBuddy );
         return( (LRESULT)(LPSTR)hwndPrevBuddy );
         }

      case UDM_SETRANGE:
         pUdmCtrl = GetBlock( hwnd );
         pUdmCtrl->nLower = HIWORD( lParam );
         pUdmCtrl->nUpper = LOWORD( lParam );
         return( 0L );

      case UDM_GETACCEL: {
         int cToRetrieve;

         cToRetrieve = 0;
         pUdmCtrl = GetBlock( hwnd );
         cToRetrieve = min( pUdmCtrl->nAccels, (int)wParam );
         _fmemcpy( (LPSTR)lParam, pUdmCtrl->paAccels, cToRetrieve * sizeof( UDACCEL ) );
         return( cToRetrieve );
         }

      case UDM_SETACCEL: {
         int cToRetrieve;
         BOOL fOk = FALSE;

         cToRetrieve = 0;
         pUdmCtrl = GetBlock( hwnd );
         if( wParam > 0 )
            {
            if( pUdmCtrl->paAccels )
               FreeMemory( &pUdmCtrl->paAccels );
            pUdmCtrl->nAccels = (int)wParam;
            if( fNewMemory( &pUdmCtrl->paAccels, pUdmCtrl->nAccels * sizeof( UDACCEL ) ) )
               {
               _fmemcpy( pUdmCtrl->paAccels, (LPSTR)lParam, pUdmCtrl->nAccels * sizeof( UDACCEL ) );
               fOk = TRUE;
               }
            }
         return( fOk );
         }

      case UDM_GETPOS:
         pUdmCtrl = GetBlock( hwnd );
         ReadUpDownPos( hwnd, pUdmCtrl );
         return( pUdmCtrl->nPos );

      case UDM_GETRANGE:
         pUdmCtrl = GetBlock( hwnd );
         return( MAKELONG( pUdmCtrl->nUpper, pUdmCtrl->nLower ) );

      case UDM_GETBUDDY:
         pUdmCtrl = GetBlock( hwnd );
         return( (LRESULT)(LPSTR)pUdmCtrl->hwndBuddy );

      case UDM_GETBASE:
         pUdmCtrl = GetBlock( hwnd );
         return( pUdmCtrl->nBase );

      case UDM_SETBASE: {
         int nPrevBase = 0;

         pUdmCtrl = GetBlock( hwnd );
         if( 10 == wParam || 16 == wParam )
            {
            nPrevBase = pUdmCtrl->nBase;
            pUdmCtrl->nBase = wParam;
            }
         return( nPrevBase );
         }

      case UDM_SETPOS: {
         int nPrevPos = 0;
         WORD wFlags;
         int nPos;

         pUdmCtrl = GetBlock( hwnd );
         nPos = (int)LOWORD( lParam );
         nPrevPos = pUdmCtrl->nPos;
         wFlags = pUdmCtrl->wFlags;
         if( pUdmCtrl->nUpper < pUdmCtrl->nLower )
            {
            if( nPos < pUdmCtrl->nUpper )
               nPos = pUdmCtrl->nUpper;
            if( nPos > pUdmCtrl->nLower )
               nPos = pUdmCtrl->nLower;
            pUdmCtrl->wFlags &= ~UDF_DISABLED;
            if( nPos == pUdmCtrl->nUpper )
               pUdmCtrl->wFlags |= UDF_UPDISABLED;
            if( nPos == pUdmCtrl->nLower )
               pUdmCtrl->wFlags |= UDF_DOWNDISABLED;
            }
         else
            {
            if( nPos < pUdmCtrl->nLower )
               nPos = pUdmCtrl->nLower;
            if( nPos > pUdmCtrl->nUpper )
               nPos = pUdmCtrl->nUpper;
            pUdmCtrl->wFlags &= ~UDF_DISABLED;
            if( nPos == pUdmCtrl->nLower )
               pUdmCtrl->wFlags |= UDF_DOWNDISABLED;
            if( nPos == pUdmCtrl->nUpper )
               pUdmCtrl->wFlags |= UDF_UPDISABLED;
            }
         pUdmCtrl->nPos = (int)nPos;
         DrawUpDownBuddy( hwnd, pUdmCtrl );
         if( wFlags != pUdmCtrl->wFlags )
            {
            DrawUpDown( hwnd, NULL, UDF_UP|pUdmCtrl->wFlags );
            DrawUpDown( hwnd, NULL, UDF_DOWN|pUdmCtrl->wFlags );
            }
         return( nPrevPos );
         }

      case WM_LBUTTONDOWN: {
         RECT rect;

         /* The user clicked on one of the arrows. Figure
          * out which and update the position accordingly.
          */
         pUdmCtrl = GetBlock( hwnd );
         GetClientRect( hwnd, &rect );
         ReadUpDownPos( hwnd, pUdmCtrl );
         if( GetWindowStyle( hwnd ) & UDS_HORZ )
            {
            /* Horizontal up/down control.
             */
            if( (int)LOWORD( lParam ) > rect.right >> 1 )
               pUdmCtrl->wFlags = UDF_DOWN|UDF_INCTRL|UDF_CLICKED;
            else
               pUdmCtrl->wFlags = UDF_UP|UDF_INCTRL|UDF_CLICKED;
            }
         else
            {
            /* Horizontal up/down control.
             */
            if( (int)HIWORD( lParam ) > rect.bottom >> 1 )
               pUdmCtrl->wFlags = UDF_DOWN|UDF_INCTRL|UDF_CLICKED;
            else
               pUdmCtrl->wFlags = UDF_UP|UDF_INCTRL|UDF_CLICKED;
            }

         /* Set the focus to the buddy.
          */
         if( pUdmCtrl->hwndBuddy )
            SetFocus( pUdmCtrl->hwndBuddy );

         /* Now do the adjustment and redraw the control.
          */
         if( AdjustUpDownPos( hwnd, pUdmCtrl->wFlags & UDF_DOWN ) )
            {
            DrawUpDownBuddy( hwnd, pUdmCtrl );
            DrawUpDown( hwnd, NULL, pUdmCtrl->wFlags );
            if( pUdmCtrl->wFlags & UDF_DOWN )
               DrawUpDown( hwnd, NULL, UDF_UP );
            if( pUdmCtrl->wFlags & UDF_UP )
               DrawUpDown( hwnd, NULL, UDF_DOWN );
            }

         /* This is where we deal with the control accelerating
          * as long as the mouse button is down.
          */
         pUdmCtrl->cAccel = 0;
         pUdmCtrl->nSec = 0;
         pUdmCtrl->nTicks = 0;
         SetCapture( hwnd );
         SetTimer( hwnd, IDT_ACCELFIRST, pUdmCtrl->nSpinRate = INIT_SPIN_DELAY, NULL );
         break;
         }

      case WM_MOUSEMOVE:
         pUdmCtrl = GetBlock( hwnd );
         if( pUdmCtrl->wFlags & UDF_CLICKED )
            {
            POINT pt;
            UINT wPrevFlags;
            RECT rect;

            /* The mouse was moved while it was held down over
             * one of the buttons. Check that we're still in
             * the control and clear the UDF_INCTRL flag if not.
             */
            GetClientRect( hwnd, &rect );
            wPrevFlags = pUdmCtrl->wFlags;
            if( GetWindowStyle( hwnd ) & UDS_HORZ )
               {
               if( pUdmCtrl->wFlags & UDF_DOWN )
                  rect.left = rect.right >> 1;
               else
                  rect.right = rect.right >> 1;
               }
            else
               {
               if( pUdmCtrl->wFlags & UDF_DOWN )
                  rect.top = rect.bottom >> 1;
               else
                  rect.bottom = rect.bottom >> 1;
               }
            pt.x = LOWORD( lParam );
            pt.y = HIWORD( lParam );
            if( PtInRect( &rect, pt ) )
               pUdmCtrl->wFlags |= UDF_INCTRL;
            else
               pUdmCtrl->wFlags &= ~UDF_INCTRL;
            if( pUdmCtrl->wFlags != wPrevFlags )
               DrawUpDown( hwnd, NULL, pUdmCtrl->wFlags );
            }
         break;

      case WM_CANCELMODE:
      case WM_LBUTTONUP:
         /* Okay, the user has stopped clicking on the 
          * control. Cancel the timer and restore the
          * control to the up position.
          */
         pUdmCtrl = GetBlock( hwnd );
         KillTimer( hwnd, IDT_ACCELFIRST );
         KillTimer( hwnd, IDT_ACCELNEXT );
         if( pUdmCtrl->wFlags & UDF_INCTRL )
            {
            pUdmCtrl->wFlags &= ~UDF_INCTRL;
            DrawUpDown( hwnd, NULL, UDF_UP|pUdmCtrl->wFlags );
            DrawUpDown( hwnd, NULL, UDF_DOWN|pUdmCtrl->wFlags );
            }
         pUdmCtrl->wFlags = 0;
         ReleaseCapture();
         break;

      case WM_TIMER:
         pUdmCtrl = GetBlock( hwnd );
         if( IDT_ACCELFIRST == wParam  )
            {
            KillTimer( hwnd, IDT_ACCELFIRST );
            SetTimer( hwnd, IDT_ACCELNEXT, pUdmCtrl->nSpinRate = SPIN_DELAY, NULL );
            }
         if( pUdmCtrl->wFlags & UDF_INCTRL )
            {
            if( AdjustUpDownPos( hwnd, pUdmCtrl->wFlags & UDF_DOWN ) )
               DrawUpDownBuddy( hwnd, pUdmCtrl );
            if( ( pUdmCtrl->nTicks += pUdmCtrl->nSpinRate ) >= 1000 )
               {
               ++pUdmCtrl->nSec;
               pUdmCtrl->nTicks = 0;
               if( pUdmCtrl->cAccel < pUdmCtrl->nAccels - 1 )
                  {
                  if( pUdmCtrl->nSec > pUdmCtrl->paAccels[ pUdmCtrl->cAccel + 1 ].nSec )
                     ++pUdmCtrl->cAccel;
                  }
               }
            }
         break;

      case WM_PAINT: {
         PAINTSTRUCT ps;
         HDC hdc;

         hdc = BeginPaint( hwnd, &ps );
         pUdmCtrl = GetBlock( hwnd );
         ReadUpDownPos( hwnd, pUdmCtrl );
         SendMessage( hwnd, UDM_SETPOS, 0, MAKELPARAM( pUdmCtrl->nPos, 0 ) );

         DrawUpDown( hwnd, hdc, pUdmCtrl->wFlags|UDF_UP );
         DrawUpDown( hwnd, hdc, pUdmCtrl->wFlags|UDF_DOWN );
         EndPaint( hwnd, &ps );
         return( 0L );
         }
      }
   return( DefWindowProc( hwnd, message, wParam, lParam ) );
}

void FASTCALL ReadUpDownPos( HWND hwnd, UDMCTRL FAR * pUdmCtrl )
{
   if( pUdmCtrl->hwndBuddy )
      {
      static char szBuf[ 14 ];
      register int c;
      int nPos;

      SendMessage( pUdmCtrl->hwndBuddy, WM_GETTEXT, sizeof( szBuf ), (LPARAM)(LPSTR)szBuf );
      nPos = 0;
      for( c = 0; szBuf[ c ]; ++c )
         if( szBuf[ c ] != ',' )
            {
            if( isxdigit( szBuf[ c ] ) && isalpha( szBuf[ c ] ) && 16 == pUdmCtrl->nBase )
               nPos = ( nPos * pUdmCtrl->nBase ) + ( toupper( szBuf[ c ] ) - 'A' - 10 );
            else if( isdigit( szBuf[ c ] ) )
               nPos = ( nPos * pUdmCtrl->nBase ) + ( szBuf[ c ] - '0' );
            else
               return;
            }
      pUdmCtrl->nPos = nPos;
      }
}

/* This function adjusts the up/down control position
 * by the specified delta.
 */
int FASTCALL AdjustUpDownPos( HWND hwnd, BOOL fDown )
{
   UDMCTRL FAR * pUdmCtrl;
   int nMin, nMax;
   int iDelta = 0;

   pUdmCtrl = GetBlock( hwnd );
   iDelta = pUdmCtrl->nPos;
   if( pUdmCtrl->nUpper < pUdmCtrl->nLower )
      {
      fDown = !fDown;
      nMin = pUdmCtrl->nUpper;
      nMax = pUdmCtrl->nLower;
      }
   else
      {
      nMin = pUdmCtrl->nLower;
      nMax = pUdmCtrl->nUpper;
      }

   /* Now adjust the position and handle wrap-around if
    * we've reached the limit and UDS_WRAP is specified
    * in the control's style.
    */
   if( fDown )
      {
      pUdmCtrl->nPos -= pUdmCtrl->paAccels[ pUdmCtrl->cAccel ].nInc;
      if( pUdmCtrl->nPos < nMin )
         {
         if( GetWindowStyle( hwnd ) & UDS_WRAP )
            pUdmCtrl->nPos = nMax;
         else
            pUdmCtrl->nPos = nMin;
         }
      }
   else
      {
      pUdmCtrl->nPos += pUdmCtrl->paAccels[ pUdmCtrl->cAccel ].nInc;
      if( pUdmCtrl->nPos > nMax )
         {
         if( GetWindowStyle( hwnd ) & UDS_WRAP )
            pUdmCtrl->nPos = nMin;
         else
            pUdmCtrl->nPos = nMax;
         }
      }

   /* Set the appropriate disabled flag if the
    * control has reached one of the limits.
    */
   pUdmCtrl->wFlags &= ~UDF_DISABLED;
   if( pUdmCtrl->nPos == nMin )
      pUdmCtrl->wFlags |= UDF_DOWNDISABLED;
   if( pUdmCtrl->nPos == nMax )
      pUdmCtrl->wFlags |= UDF_UPDISABLED;

   /* Send a UDN_DELTAPOS and WM_VSCROLL to the parent
    * if the control position has changed.
    */
   if( iDelta = pUdmCtrl->nPos - iDelta )
      {
      NM_UPDOWN nmHdr;

      nmHdr.iPos = pUdmCtrl->nPos;
      nmHdr.iDelta = iDelta;
      Amctl_SendNotify( GetParent( hwnd ), hwnd, UDN_DELTAPOS, (NMHDR FAR *)&nmHdr );
      SendVScroll( GetParent( hwnd ), SB_THUMBPOSITION, pUdmCtrl->nPos, hwnd );
      }
   return( iDelta );
}

/* This function updates the buddy control if the UDS_SETBUDDYINT
 * style is specified.
 */
void FASTCALL DrawUpDownBuddy( HWND hwnd, UDMCTRL FAR * pUdmCtrl )
{
   if( pUdmCtrl->hwndBuddy && ( GetWindowStyle( hwnd ) & UDS_SETBUDDYINT ) )
      {
      static char HexTable[] = "0123456789ABCDEF";
      static char szBuf[ 14 ];
      BOOL fThousandsSep;
      int c, nPos;

      fThousandsSep = !( GetWindowStyle( hwnd ) & UDS_NOTHOUSANDS );
      c = 0;
      nPos = pUdmCtrl->nPos;
      do {
         if( ( c + 1 ) % 4 == 0 && fThousandsSep )
            szBuf[ c++ ] = *szThousands;
         szBuf[ c++ ] = HexTable[ nPos % pUdmCtrl->nBase ];
         nPos /= pUdmCtrl->nBase;
         }
      while( nPos );
      szBuf[ c ] = '\0';
      _strrev( szBuf );
      SendMessage( pUdmCtrl->hwndBuddy, WM_SETTEXT, 0, (LPARAM)(LPSTR)szBuf );
      }
}

/* This function draws the up/down control.
 */
void FASTCALL DrawUpDown( HWND hwnd, HDC hdc, int flags )
{
   static POINT pt[ 3 ];
   HBRUSH hBrush, hOldBrush;
   HBRUSH hShadowBrush = NULL;
   DWORD dwStyle;
   int offset;
   HPEN hFacePen, hHilitePen, hShadowPen, hBlackPen;
   HPEN hOldPen;
   RECT rect;
   int iHeight;
   int iMidPoint;
   BOOL fLclDC = FALSE;

   /* If we're called from WM_PAINT, then hdc will already be set up for
    * us, otherwise we have to get an explicit DC.
    */
   if( !hdc )
      {
      hdc = GetDC( hwnd );
      fLclDC = TRUE;
      }

   /* Create some pens and brushes.
    */
   GetClientRect( hwnd, &rect );
   hShadowPen = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNSHADOW ) );
   hShadowBrush = CreateSolidBrush( GetSysColor( COLOR_BTNSHADOW ) );
   hFacePen = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNFACE ) );
   hHilitePen = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNHIGHLIGHT ) );
   hBlackPen = GetStockObject( BLACK_PEN );

   /* Draw the border.
    */
   dwStyle = GetWindowStyle( hwnd );
   if( dwStyle & UDS_ALIGNRIGHT )
      {
      hOldPen = SelectPen( hdc, hShadowPen );
      MoveToEx( hdc, rect.left, rect.top, NULL );
      LineTo( hdc, rect.right - 1, rect.top );

      SelectPen( hdc, hBlackPen );
      MoveToEx( hdc, rect.left, rect.top + 1, NULL );
      LineTo( hdc, rect.right - 2, rect.top + 1 );

      SelectPen( hdc, hHilitePen );
      MoveToEx( hdc, rect.right - 1, rect.top, NULL );
      LineTo( hdc, rect.right - 1, rect.bottom - 1 );
      LineTo( hdc, rect.left - 1, rect.bottom - 1 );

      SelectPen( hdc, hFacePen );
      MoveToEx( hdc, rect.right - 2, rect.top + 1, NULL );
      LineTo( hdc, rect.right - 2, rect.bottom - 2 );
      LineTo( hdc, rect.left - 1, rect.bottom - 2 );

      rect.top += 2;
      rect.bottom -= 2;
      rect.right -= 2;
      }
   else
      hOldPen = SelectPen( hdc, hBlackPen );
   hOldBrush = SelectBrush( hdc, hShadowBrush );

   /* Fill the inside of the control.
    */
   hBrush = CreateSolidBrush( GetSysColor( COLOR_BTNFACE ) );
   if( flags & UDF_UP ) 
      {
      if( dwStyle & UDS_HORZ )
         rect.right = rect.right >> 1;
      else
         rect.bottom = rect.bottom >> 1;
      }
   else
      {
      if( dwStyle & UDS_HORZ )
         rect.left = rect.right >> 1;
      else
         rect.top = rect.bottom >> 1;
      }
   FillRect( hdc, &rect, hBrush );
   DeleteObject( hBrush );

   /* Fill the inset if the control is pushed
    * in.
    */
   if( flags & UDF_INCTRL )
      {
      SelectPen( hdc, hShadowPen );
      MoveToEx( hdc, rect.left, rect.bottom - 2, NULL );
      LineTo( hdc, rect.left, rect.top + 1 );
      LineTo( hdc, rect.right - 1, rect.top + 1 );

      SelectPen( hdc, hBlackPen );
      MoveToEx( hdc, rect.left + 1, rect.bottom - 3, NULL );
      LineTo( hdc, rect.left + 1, rect.top + 2 );
      LineTo( hdc, rect.right - 2, rect.top + 2 );
   
      SelectPen( hdc, hFacePen );
      MoveToEx( hdc, rect.left + 1, rect.bottom - 2, NULL );
      LineTo( hdc, rect.right - 2, rect.bottom - 2 );
      LineTo( hdc, rect.right - 2, rect.top + 1 );

      SelectPen( hdc, hHilitePen );
      MoveToEx( hdc, rect.left, rect.bottom - 1, NULL );
      LineTo( hdc, rect.right - 1, rect.bottom - 1 );
      LineTo( hdc, rect.right - 1, rect.top );
      offset = 1;
      }
   else
      {
      SelectPen( hdc, hFacePen );
      MoveToEx( hdc, rect.left, rect.bottom - 2, NULL );
      LineTo( hdc, rect.left, rect.top );
      LineTo( hdc, rect.right - 1, rect.top );

      SelectPen( hdc, hHilitePen );
      MoveToEx( hdc, rect.left + 1, rect.bottom - 3, NULL );
      LineTo( hdc, rect.left + 1, rect.top + 1 );
      LineTo( hdc, rect.right - 2, rect.top + 1 );
   
      SelectPen( hdc, hShadowPen );
      MoveToEx( hdc, rect.left + 1, rect.bottom - 2, NULL );
      LineTo( hdc, rect.right - 2, rect.bottom - 2 );
      LineTo( hdc, rect.right - 2, rect.top );

      SelectPen( hdc, hBlackPen );
      MoveToEx( hdc, rect.left, rect.bottom - 1, NULL );
      LineTo( hdc, rect.right - 1, rect.bottom - 1 );
      LineTo( hdc, rect.right - 1, rect.top - 1 );
      offset = 0;
      }

   /* Draw the arrows on the control.
    */
   InflateRect( &rect, -2, -2 );
   iHeight = ( ( rect.bottom - rect.top ) + 1 ) / 3;
   iMidPoint = ( ( rect.right - rect.left ) - 1 ) >> 1;
   if( iHeight > iMidPoint )
      iHeight = iMidPoint;

   /* Draw the up (or left) arrow.
    */
   if( flags & UDF_UP )
      {
      if( flags & UDF_UPDISABLED )
         {
         SelectBrush( hdc, hShadowBrush );
         SelectPen( hdc, hShadowPen );
         }
      else
         {
         SelectBrush( hdc, GetStockObject( BLACK_BRUSH ) );
         SelectPen( hdc, hBlackPen );
         }
      if( dwStyle & UDS_HORZ )
         {
         pt[ 0 ].x = ( rect.left + iHeight ) + offset;
         pt[ 0 ].y = ( rect.top + iMidPoint ) + offset;
         pt[ 1 ].x = ( rect.left + iHeight + iHeight - 1 ) + offset;
         pt[ 1 ].y = ( rect.top + iMidPoint - ( iHeight - 1 ) ) + offset;
         pt[ 2 ].x = ( rect.left + iHeight + iHeight - 1 ) + offset;
         pt[ 2 ].y = ( rect.top + iMidPoint + ( iHeight - 1 ) ) + offset;
         Polygon( hdc, pt, 3 );
         }
      else
         {
         pt[ 0 ].x = ( rect.left + iMidPoint ) + offset;
         pt[ 0 ].y = ( rect.top + iHeight ) + offset;
         pt[ 1 ].x = ( rect.left + iMidPoint - ( iHeight - 1 ) ) + offset;
         pt[ 1 ].y = ( rect.top + iHeight + iHeight - 1 ) + offset;
         pt[ 2 ].x = ( rect.left + iMidPoint + ( iHeight - 1 ) ) + offset;
         pt[ 2 ].y = ( rect.top + iHeight + iHeight - 1 ) + offset;
         Polygon( hdc, pt, 3 );
         }
      }

   /* Draw the down (or right) arrow.
    */
   else if( flags & UDF_DOWN )
      {
      if( flags & UDF_DOWNDISABLED )
         {
         SelectBrush( hdc, hShadowBrush );
         SelectPen( hdc, hShadowPen );
         }
      else
         {
         SelectBrush( hdc, GetStockObject( BLACK_BRUSH ) );
         SelectPen( hdc, hBlackPen );
         }
      if( dwStyle & UDS_HORZ )
         {
         pt[ 0 ].x = ( rect.left + iHeight ) + offset;
         pt[ 0 ].y = ( rect.top + iMidPoint - ( iHeight - 1 ) ) + offset;
         pt[ 1 ].x = ( rect.left + iHeight + iHeight - 1 ) + offset;
         pt[ 1 ].y = ( rect.top + iMidPoint ) + offset;
         pt[ 2 ].x = ( rect.left + iHeight ) + offset;
         pt[ 2 ].y = ( rect.top + iMidPoint + ( iHeight - 1 ) ) + offset;
         Polygon( hdc, pt, 3 );
         }
      else
         {
         pt[ 0 ].x = ( rect.left + iMidPoint - ( iHeight - 1 ) ) + offset;
         pt[ 0 ].y = ( rect.top + iHeight ) + offset;
         pt[ 1 ].x = ( rect.left + iMidPoint + ( iHeight - 1 ) ) + offset;
         pt[ 1 ].y = ( rect.top + iHeight ) + offset;
         pt[ 2 ].x = ( rect.left + iMidPoint ) + offset;
         pt[ 2 ].y = ( rect.top + iHeight + iHeight - 1 ) + offset;
         Polygon( hdc, pt, 3 );
         }
      }

   /* Clean up before we leave.
    */
   DeleteBrush( hShadowBrush );
   SelectPen( hdc, hOldPen );
   SelectBrush( hdc, hOldBrush );
   DeletePen( hShadowPen );
   DeletePen( hHilitePen );
   DeletePen( hFacePen );
   if( fLclDC )
      ReleaseDC( hwnd, hdc );
}

/* This function subclasses the buddy window so that all window messages go to
 * our own window procedure (UpDownBuddyProc) first. The handle of the original
 * window procedure is stored in the UDMCTRL. The handle of the up-down control
 * is also stored as a property in the buddy window.
 */
void FASTCALL HookBuddy( HWND hwnd, UDMCTRL FAR * pUdmCtrl )
{
   if( pUdmCtrl->hwndBuddy )
      {
      pUdmCtrl->lpBuddyWndProc = SubclassWindow( pUdmCtrl->hwndBuddy, UpDownBuddyProc );
      SetProp( pUdmCtrl->hwndBuddy, "updown_buddy", hwnd );
      }
}

/* This function removes the subclassing from the buddy window and deletes the
 * property which was used to store the handle of the up-down control.
 */
void FASTCALL UnhookBuddy( HWND hwnd, UDMCTRL FAR * pUdmCtrl )
{
   if( pUdmCtrl->lpBuddyWndProc )
      {
      SubclassWindow( pUdmCtrl->hwndBuddy, pUdmCtrl->lpBuddyWndProc );
      RemoveProp( pUdmCtrl->hwndBuddy, "updown_buddy" );
      }
}

/* This function physically moves the up/down control buddy
 * so that it is properly aligned next to the up/down control.
 */
void FASTCALL SetBuddyPosition( HWND hwnd, HWND hwndBuddy )
{
   RECT rc;
   RECT rcCtrl;
   int iHeight;
   int iWidth;

   /* Get the buddy's physical location offset from the
    * parent client area.
    */
   GetWindowRect( hwndBuddy, &rc );
   ScreenToClient( GetParent( hwnd ), (LPPOINT)&rc );
   ScreenToClient( GetParent( hwnd ), ((LPPOINT)&rc)+1 );

   /* Get the up/down control's dimensions.
    */
   GetClientRect( hwnd, &rcCtrl );
   iHeight = rcCtrl.bottom - rcCtrl.top;;
   iWidth = rcCtrl.right - rcCtrl.left;

   if( GetWindowStyle( hwnd ) & UDS_ALIGNLEFT )
      {
      /* Align the control on the left of the buddy
       */
      }
   else if( GetWindowStyle( hwnd ) & UDS_ALIGNRIGHT )
      {
      /* Align the control on the right of the buddy. We resize
       * the buddy to make it a bit shorter so that the control
       * fits into the space.
       */
      rc.right -= iWidth;
      SetWindowPos( hwndBuddy, NULL, 0, 0, rc.right-rc.left, rc.bottom-rc.top, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE );
      --rc.top;
      rc.bottom += 2;
      SetWindowPos( hwnd, NULL, rc.right - 1, rc.top, iWidth, iHeight + 1, SWP_NOZORDER|SWP_NOACTIVATE );
      }
   else if( GetWindowStyle( hwnd ) & UDS_HORZ )
      {
      }     
   else
      {
      /* No buddy alignment. Just make sure the width of the
       * control is the same as the scroll bar width.
       */
      iWidth = GetSystemMetrics( SM_CXVSCROLL ) - 1;
      SetWindowPos( hwnd, NULL, 0, 0, iWidth, iHeight, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE );
      }
}
