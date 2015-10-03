/* TOOLTIP.C - Implements the tooltips control
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

#include <windows.h>
#include <windowsx.h>
#include "string.h"
#include "amctrls.h"
#include "amctrli.h"
#include "multimon.h"

#define  THIS_FILE   __FILE__

#define  IDT_SHOWTIMER     0x400
#define  IDT_HIDETIMER     0x401

#define  DEF_RESHOWDELAYTIME     50
#define  DEF_INITIALDELAYTIME    1000
#define  DEF_AUTOPOPDELAYTIME    5000
#define  DEF_AUTODELAYCOUNTBIT   100
#define  MAX_TIP_WIDTH           170

#ifndef COLOR_INFOTEXT
#define COLOR_INFOTEXT          23
#define COLOR_INFOBK            24
#define COLOR_3DLIGHT           22
#endif

#define  GWW_EXTRA            4
#define  GetBlock(h)          ((LPTOOLTIP)GetWindowLong((h),0))
#define  PutBlock(h,b)        (SetWindowLong((h),0,(LONG)(b)))

typedef struct tagTOOL {
   struct tagTOOL FAR * pNextTool;     /* Pointer to next tool entry */
   LPSTR pszText;                      /* Tooltip text */
   HWND hwndOwner;                     /* Owner window of tooltip */
   TOOLINFO ti;                        /* Tool structure */
} TOOL, FAR * LPTOOL;

typedef struct tagTOOLTIP {
   LPTOOL pFirstTool;                  /* Pointer to first tooltip tool. */
   LPTOOL pLastTool;                   /* Pointer to last tooltip tool. */
   LPTOOL pToolTipTool;                /* Pointer to displayed tooltip tool */
   BOOL fActivated;                    /* TRUE if we're activated */
   int nInitialDelayTime;              /* Delay, in ms, before we show the tooltip */
   int nAutoPopupDelayTime;            /* Delay, in ms, before we hide the tooltip */
   int nReshowDelayTime;               /* Delay, in ms, before we show tooltip when cursor moved to another tool */
   int nAutoDelayCount;                /* Counter used to monitor timer */
   int cTools;                         /* Number of tools */
   HFONT hFont;                        /* Font used to draw tooltip text */
   RECT rcMargin;                      /* Tooltip margin */
   int nMaxTipWidth;                   /* Maximum tooltip width */
} TOOLTIP, FAR * LPTOOLTIP;

static LPTOOL FASTCALL FindTool( LPTOOLTIP, LPTOOLINFO );
static LPTOOL FASTCALL GetTool( LPTOOLTIP, int );
static void FASTCALL GetToolTipText( HWND, LPTOOL );
static LPTOOL FASTCALL ToolFromPoint( HWND, LPTOOLTIP, POINT * );
static void FASTCALL CreateTooltipFont( LPTOOLTIP );
static void FASTCALL HideTooltip( HWND, LPTOOL );
static BOOL FASTCALL OwnerHasFocus( HWND );

LRESULT EXPORT CALLBACK TooltipWndFn( HWND, UINT, WPARAM, LPARAM );

/* This function registers the tooltip window class.
 */
BOOL FASTCALL RegisterTooltipClass( HINSTANCE hInst )
{
   WNDCLASS wc;

   wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
   wc.hIcon       = NULL;
   wc.lpszMenuName       = NULL;
   wc.lpszClassName  = TOOLTIPS_CLASS;
   wc.hbrBackground  = NULL;
   wc.hInstance      = hInst;
   wc.style       = CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS | CS_SAVEBITS;
   wc.lpfnWndProc    = TooltipWndFn;
   wc.cbClsExtra     = 0;
   wc.cbWndExtra     = GWW_EXTRA;
   return( RegisterClass( &wc ) );
}

LRESULT EXPORT CALLBACK TooltipWndFn( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LPTOOLTIP pTooltip;
   LPTOOL pTool;

#ifdef _DEBUG
   /* Help us to catch NULL pointers...
    */
   pTooltip = NULL;
   pTool = NULL;
#endif
   switch( message )
      {
      case WM_NCCREATE: {
         LPCREATESTRUCT lpcs;

         /* Must not be visible, and must not be a child.
          * Also, must not have a caption. Must be a popup
          * and have a border.
          */
         lpcs = (LPCREATESTRUCT)lParam;
         lpcs->style &= ~(WS_VISIBLE|WS_CHILD|WS_CAPTION|WS_SYSMENU);
         lpcs->style |= WS_POPUP;
         SetWindowStyle( hwnd, lpcs->style );
         break;
         }

      case WM_CREATE: {
         static LPTOOLTIP pNewTooltip;

         INITIALISE_PTR(pNewTooltip);
         if( fNewMemory( &pNewTooltip, sizeof( TOOLTIP ) ) )
            {
            pNewTooltip->pFirstTool = NULL;
            pNewTooltip->pLastTool = NULL;
            pNewTooltip->pToolTipTool = NULL;
            pNewTooltip->fActivated = TRUE;
            pNewTooltip->cTools = 0;
            pNewTooltip->nInitialDelayTime = DEF_INITIALDELAYTIME;
            pNewTooltip->nAutoPopupDelayTime = DEF_AUTOPOPDELAYTIME;
            pNewTooltip->nReshowDelayTime = DEF_RESHOWDELAYTIME;
            pNewTooltip->nMaxTipWidth = MAX_TIP_WIDTH;
            SetRect( &pNewTooltip->rcMargin, 2, 2, 2, 2 );
            CreateTooltipFont( pNewTooltip );
            PutBlock( hwnd, pNewTooltip );
            }
         return( pNewTooltip ? 0 : -1 );
         }

      case TTM_ACTIVATE:
         pTooltip = GetBlock( hwnd );
         pTooltip->fActivated = (BOOL)wParam;
         return( 0L );

      case TTM_ADDTOOL: {
         static LPTOOL pNewTool;

         pTooltip = GetBlock( hwnd );
         INITIALISE_PTR(pNewTool);
         if( fNewMemory( &pNewTool, sizeof( TOOL ) ) )
            {
            LPTOOLINFO lpti;

            /* This code adds a new tool to the list. To
             * be frank, I'd be happier if it could add the
             * tool in some ordered sequence, but there doesn't
             * seem to be any obvious candidate for ordering
             * at the moment.
             */
            lpti = (LPTOOLINFO)lParam;
            if( NULL == pTooltip->pFirstTool )
               pTooltip->pFirstTool = pNewTool;
            else
               pTooltip->pLastTool->pNextTool = pNewTool;
            pNewTool->pNextTool = NULL;
            pTooltip->pLastTool = pNewTool;
            pNewTool->pszText = NULL;
            pNewTool->ti = *lpti;
            ++pTooltip->cTools;
            return( TRUE );
            }
         return( FALSE );
         }

      case TTM_DELTOOL: {
         LPTOOL pPrevTool;
         LPTOOLINFO lpti;

         /* This code removes a tool. It tries to be clever by
          * using pointer indirection to look forward through the
          * link, thereby saving the need for an pPrevTool variable.
          * I don't like clever code, but this seems to be simple
          * enough to understand, so I'll leave it.
          */
         pTooltip = GetBlock( hwnd );
         lpti = (LPTOOLINFO)lParam;
         pPrevTool = NULL;
         pTool = pTooltip->pFirstTool;
         while( pTool )
            {
            if( pTool->ti.hwnd == lpti->hwnd && pTool->ti.uId == lpti->uId )
               break;
            pPrevTool = pTool;
            pTool = pTool->pNextTool;
            }
         if( pTool )
            {
            if( pPrevTool )
               pPrevTool->pNextTool = pTool->pNextTool;
            else
               pTooltip->pFirstTool = pTool->pNextTool;
            if( NULL == pTool->pNextTool )
               pTooltip->pLastTool = pPrevTool;
            if( pTool->pszText )
               FreeMemory( &pTool->pszText );
            FreeMemory( &pTool );
            --pTooltip->cTools;
            }
         return( 0L );
         }

      case TTM_ENUMTOOLS:
         pTooltip = GetBlock( hwnd );
         if( pTool = GetTool( pTooltip, (int)wParam ) )
            {
            LPTOOLINFO lpti;

            lpti = (LPTOOLINFO)lParam;
            *lpti = pTool->ti;
            return( TRUE );
            }
         return( FALSE );

      case TTM_GETTEXT: {
         LPTOOLINFO lpti;

         pTooltip = GetBlock( hwnd );
         lpti = (LPTOOLINFO)lParam;
         if( pTool = FindTool( pTooltip, lpti ) )
            {
            GetToolTipText( hwnd, pTool );
            lpti->lpszText = pTool->pszText;
            return( TRUE );
            }
         return( FALSE );
         }

      case TTM_GETTOOLCOUNT:
         pTooltip = GetBlock( hwnd );
         return( pTooltip->cTools );         

      case TTM_GETTOOLINFO: {
         LPTOOLINFO lpti;

         pTooltip = GetBlock( hwnd );
         lpti = (LPTOOLINFO)lParam;
         if( pTool = FindTool( pTooltip, lpti ) )
            {
            *lpti = pTool->ti;
            return( TRUE );
            }
         return( FALSE );
         }

      case TTM_SETMARGIN:
         pTooltip = GetBlock( hwnd );
         pTooltip->rcMargin = *(RECT *)lParam;
         return( TRUE );

      case TTM_GETMARGIN:
         pTooltip = GetBlock( hwnd );
         *(RECT *)lParam = pTooltip->rcMargin;
         return( TRUE );

      case TTM_SETMAXTIPWIDTH:
         pTooltip = GetBlock( hwnd );
         pTooltip->nMaxTipWidth = (int)lParam;
         return( TRUE );

      case TTM_GETMAXTIPWIDTH:
         pTooltip = GetBlock( hwnd );
         return( pTooltip->nMaxTipWidth );

      case TTM_NEWTOOLRECT: {
         LPTOOLINFO lpti;

         pTooltip = GetBlock( hwnd );
         lpti = (LPTOOLINFO)lParam;
         if( pTool = FindTool( pTooltip, lpti ) )
            pTool->ti.rect = lpti->rect;
         return( 0L );
         }

      case TTM_RELAYEVENT: {
         LPMSG lpmsg;

         /* Check whether the top level owner window of
          * this control has the focus.
          */
         if( !OwnerHasFocus( hwnd ) )
            return( 0L );

         /* Do nothing if tooltips are deactivated.
          */
         pTooltip = GetBlock( hwnd );
         if( !pTooltip->fActivated )
            return( 0L );

         /* Do nothing if the parent window isn't active
          * and TTS_ALWAYSTIP not specified.
          */
         lpmsg = (LPMSG)lParam;
         if( !IsWindowEnabled( lpmsg->hwnd ) && !( GetWindowStyle( hwnd ) & TTS_ALWAYSTIP ) )
            return( 0L );
         switch( lpmsg->message )
            {
            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
            case WM_LBUTTONDOWN:
            case WM_CONTEXTMENU: /*!!SM!!*/
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
               /* Any mouse button up/down kills tooltips.
                */
               HideTooltip( hwnd, pTooltip->pToolTipTool );
               KillTimer( hwnd, IDT_SHOWTIMER );
               KillTimer( hwnd, IDT_HIDETIMER );
               break;

            case WM_MOUSEMOVE: {
               static POINT pt;

               /* Get the mouse cursor position in client
                * coordinates.
                */
               pt.x = LOWORD( lpmsg->lParam );
               pt.y = HIWORD( lpmsg->lParam );

               /* Find the tool at the coordinates. If no tool,
                * then the cursor has been moved off the toolbar,
                * so hide the tooltips.
                */
               pTool = ToolFromPoint( lpmsg->hwnd, pTooltip, &pt );
               if( NULL == pTool )
                  {
                  HideTooltip( hwnd, pTooltip->pToolTipTool );
                  KillTimer( hwnd, IDT_SHOWTIMER );
                  KillTimer( hwnd, IDT_HIDETIMER );
                  pTooltip->pToolTipTool = NULL;
                  break;
                  }
               pTool->hwndOwner = lpmsg->hwnd;

               /* If we're still on the same tool, do nothing.
                */
               if( pTool == pTooltip->pToolTipTool )
                  break;

               /* Restart the timer. If we've moved from one tool to
                * another, use the shorter reshow delay time to determine
                * when to show the tooltip. Otherwise use the longer
                * initial delay time.
                */
               KillTimer( hwnd, IDT_SHOWTIMER );
               KillTimer( hwnd, IDT_HIDETIMER );
               if( pTooltip->pToolTipTool )
                  SetTimer( hwnd, IDT_SHOWTIMER, pTooltip->nReshowDelayTime, NULL );
               else
                  SetTimer( hwnd, IDT_SHOWTIMER, pTooltip->nInitialDelayTime, NULL );
               pTooltip->pToolTipTool = pTool;
               break;
               }
            }
         return( 0L );
         }

      case TTM_POP:
         pTooltip = GetBlock( hwnd );
         HideTooltip( hwnd, pTooltip->pToolTipTool );
         KillTimer( hwnd, IDT_SHOWTIMER );
         KillTimer( hwnd, IDT_HIDETIMER );
         return( 0L );

      case TTM_GETDELAYTIME:
         pTooltip = GetBlock( hwnd );
         switch( wParam )
            {
            case TTDT_INITIAL:
               return( pTooltip->nInitialDelayTime );

            case TTDT_AUTOPOP:
               return( pTooltip->nAutoPopupDelayTime );

            case TTDT_RESHOW:
               return( pTooltip->nReshowDelayTime );
            }
         return( 0L );

      case TTM_SETDELAYTIME:
         pTooltip = GetBlock( hwnd );
         switch( wParam )
            {
            case TTDT_AUTOMATIC:
               /* Automatically calculates the initial, reshow, and
                * autopopup durations based on the value of iDelay.
                * (Well, not exactly, but that's what we'll do when
                * I get round to figuring out how it works.)
                */
               pTooltip->nInitialDelayTime = 1000;
               pTooltip->nAutoPopupDelayTime = 2000;
               pTooltip->nReshowDelayTime = 100;
               break;

            case TTDT_INITIAL:
               /* Sets the length of time that the cursor must remain
                * stationary within the bounding rectangle of a tool
                * before the tooltip window is displayed.
                */
               pTooltip->nInitialDelayTime = (int)LOWORD( lParam );
               break;

            case TTDT_AUTOPOP:
               /* Sets the length of time before the tooltip window is
                * hidden if the cursor remains stationary in the tool's
                * bounding rectangle after the tooltip window has
                * appeared.
                */
               pTooltip->nAutoPopupDelayTime = (int)LOWORD( lParam );
               break;

            case TTDT_RESHOW:
               /* Sets the length of the delay before subsequent tooltip
                * windows are displayed when the cursor is moved from
                * one tool to another.
                */
               pTooltip->nReshowDelayTime = (int)LOWORD( lParam );
               break;
            }
         break;

      case TTM_SETTOOLINFO: {
         LPTOOLINFO lpti;

         pTooltip = GetBlock( hwnd );
         lpti = (LPTOOLINFO)lParam;
         if( pTool = FindTool( pTooltip, lpti ) )
            {
            pTool->ti = *lpti;
            return( TRUE );
            }
         return( FALSE );
         }

      case WM_PAINT: {
         PAINTSTRUCT ps;
         RECT rc;
         HDC hdc;

         /* Begin painting.
          */
         hdc = BeginPaint( hwnd, &ps );
         pTooltip = GetBlock( hwnd );
         GetClientRect ( hwnd, &rc );

         /* Do nothing if no text.
          */
         ASSERT( pTooltip->pToolTipTool != NULL );
         if( NULL != pTooltip->pToolTipTool->pszText )
         {
            HPEN hpenDkGrey;
            HPEN hpenBlack;
            COLORREF crOldBack;
            COLORREF crOldText;
            int nOldMode;
            HFONT hOldFont;
            COLORREF crBack;
            COLORREF crText;
            HBRUSH hbr;

            /* Pens for the frame.
             */
            hpenDkGrey = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNSHADOW ) );
            hpenBlack = GetStockPen( BLACK_PEN );
   
            /* Draw the frame.
             */
            SelectPen( hdc, hpenDkGrey );
            MoveToEx( hdc, rc.left, rc.bottom - 2, NULL );
            LineTo( hdc, rc.left, rc.top );
            LineTo( hdc, rc.right - 1, rc.top );
            SelectPen( hdc, hpenBlack );
            MoveToEx( hdc, rc.left, rc.bottom - 1, NULL );
            LineTo( hdc, rc.right - 1, rc.bottom - 1 );
            LineTo( hdc, rc.right - 1, rc.top - 1 );
            InflateRect( &rc, -1, -1 );
   
            /* Create a brush to fill the tooltip background. Note the
             * test for Win95; to be compatible, we use the Win95 tooltips colours
             * if we're running under Windows 95.
             */
            crBack = fNewShell ? GetSysColor( COLOR_INFOBK ) : RGB(255, 255, 128);
            crText = fNewShell ? GetSysColor( COLOR_INFOTEXT ) : RGB(0, 0, 0);
            hbr = CreateSolidBrush( crBack );
            FillRect( hdc, &rc, hbr );
            DeleteBrush( hbr );
   
            /* Draw the tooltip text.
             */
            crOldText = SetTextColor( hdc, crText );
            crOldBack = SetBkColor( hdc, crBack );
            hOldFont = SelectFont( hdc, pTooltip->hFont );
            nOldMode = SetBkMode( hdc, TRANSPARENT );
            rc.left += pTooltip->rcMargin.left;
            rc.top += pTooltip->rcMargin.top;
            rc.bottom -= pTooltip->rcMargin.bottom;
            rc.right -= pTooltip->rcMargin.right;
            DrawText( hdc, pTooltip->pToolTipTool->pszText, lstrlen( pTooltip->pToolTipTool->pszText ), &rc, DT_WORDBREAK|DT_NOPREFIX );
            SetBkMode( hdc, nOldMode );
            SetTextColor( hdc, crOldText );
            SetBkColor( hdc, crOldBack );
            SelectFont( hdc, hOldFont );
   
            /* End painting.
             */
            DeletePen( hpenDkGrey );
         }
         EndPaint( hwnd, &ps );
         break;
      }

      case WM_TIMER:
         if( wParam == IDT_SHOWTIMER )
            {
            LPTOOL pCTool;
            HDC hdc;
            POINT pt;

            /* Get the cursor position, and convert it to an offset from
             * the tool window.
             */
            KillTimer( hwnd, IDT_SHOWTIMER );
            GetCursorPos( &pt );
            pTooltip = GetBlock( hwnd );
            ASSERT( pTooltip->pToolTipTool != NULL );
            pCTool = pTooltip->pToolTipTool;

            /* Make sure cursor is still on the tool.
             */
            ScreenToClient( pCTool->hwndOwner, &pt );
            if( !ToolFromPoint( (HWND)pCTool->ti.uId, pTooltip, &pt ) )
               {
               HideTooltip( hwnd, pCTool );
               pTooltip->pToolTipTool = NULL;
               return( 0L );
               }

            /* Check whether the top level owner window of
             * this control has the focus.
             */
            if( !OwnerHasFocus( hwnd ) )
               {
               HideTooltip( hwnd, pCTool );
               pTooltip->pToolTipTool = NULL;
               return( 0L );
               }

            /* Make sure we have the text. If no text, then no window.
             */
            GetToolTipText( hwnd, pCTool );
            if( pCTool->pszText )
            {
               int cxScreenEdge;
               RECT rc;

               /* Compute tooltip size.
                */
               hdc = GetDC( hwnd );
               SelectFont( hdc, pTooltip->hFont );
               SetRect( &rc, 0, 0, pTooltip->nMaxTipWidth, 0 );
               DrawText( hdc, pCTool->pszText, lstrlen( pCTool->pszText ), &rc, DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX );

               /* If TTF_CENTERTIP specified, center the tooltip under the
                * tool.
                */
               if( pCTool->ti.uFlags & TTF_CENTERTIP )
                  {
                  pt.x = pCTool->ti.rect.left - ( ( rc.right - ( pCTool->ti.rect.right-pCTool->ti.rect.left) ) / 2 );
                  pt.y = pCTool->ti.rect.bottom + 2;
                  ClientToScreen( pCTool->ti.hwnd, &pt );
                  }
               else if( pCTool->ti.uFlags & TTF_COVERHWND )
                  {
                  pt.x = pCTool->ti.rect.left;
                  pt.y = pCTool->ti.rect.top;
                  ClientToScreen( pCTool->ti.hwnd, &pt );
                  }
               else
                  {
                  GetCursorPos( &pt );
                  pt.y += 16;
                  }

               /* Adjust to stop tooltip from going beyond the edge of the
                * screen.
                */
               cxScreenEdge = GetSystemMetrics( SM_CXVIRTUALSCREEN ) - 2;
               //cxScreenEdge = GetSystemMetrics( SM_CXSCREEN ) - 2;
               if( pt.x + rc.right + 4 >= cxScreenEdge )
                  pt.x = cxScreenEdge - ( rc.right + 4 );
               rc.right += pTooltip->rcMargin.left + pTooltip->rcMargin.right + 2;
               rc.bottom += pTooltip->rcMargin.top + pTooltip->rcMargin.bottom + 2;
//             SetWindowPos( hwnd, NULL, pt.x, pt.y, rc.right, rc.bottom, SWP_NOZORDER|SWP_NOACTIVATE );
               // !!SM!! 2.56.2061
               SetWindowPos( hwnd, HWND_TOPMOST, pt.x, pt.y, rc.right, rc.bottom, SWP_SHOWWINDOW|SWP_NOACTIVATE );
               ReleaseDC( hwnd, hdc );
               
               /* Need to force a repaint.
                */
               InvalidateRect( hwnd, NULL, TRUE );
               UpdateWindow( hwnd );

               /* Now show the window. Use SW_SHOWNA to ensure that
                * the window isn't activated.
                */
               ShowWindow( hwnd, SW_SHOWNA );
               //SetWindowPos( hwnd, NULL, pt.x, pt.y, rc.right, rc.bottom, SWP_NOZORDER|SWP_NOACTIVATE );
               // !!SM!! 2.56.2061
               SetWindowPos( hwnd, HWND_TOPMOST, pt.x, pt.y, rc.right, rc.bottom, SWP_SHOWWINDOW|SWP_NOACTIVATE );


               FreeMemory(&pCTool->pszText);
            }
   
            /* Set a timer to hide the tooltip after autodelay seconds.
             */
            if( 0 != pTooltip->nAutoPopupDelayTime )
               {
               pTooltip->nAutoDelayCount = pTooltip->nAutoPopupDelayTime / DEF_AUTODELAYCOUNTBIT;
               SetTimer( hwnd, IDT_HIDETIMER, DEF_AUTODELAYCOUNTBIT, NULL );
               }
            }
         else if( IDT_HIDETIMER == wParam )
            {
            LPTOOL pCTool;
            POINT pt;

            /* Make sure the cursor is still on the tool while we're
             * counting up to the autopop delay before removing the
             * tooltip.
             */
            pTooltip = GetBlock( hwnd );
            ASSERT( pTooltip->pToolTipTool != NULL );
            pCTool = pTooltip->pToolTipTool;
            GetCursorPos( &pt );
            ScreenToClient( pCTool->hwndOwner, &pt );
            if( !ToolFromPoint( (HWND)pCTool->ti.uId, pTooltip, &pt ) )
               {
               HideTooltip( hwnd, pCTool );
               pTooltip->pToolTipTool = NULL;
               KillTimer( hwnd, IDT_HIDETIMER );
               }
            else if( --pTooltip->nAutoDelayCount == 0 )
               {
               HideTooltip( hwnd, pCTool );
               KillTimer( hwnd, IDT_HIDETIMER );
               }
            }
         return( 0L );

      case WM_DESTROY:
         pTooltip = GetBlock( hwnd );
         pTool = pTooltip->pFirstTool;
         KillTimer( hwnd, IDT_SHOWTIMER );
         KillTimer( hwnd, IDT_HIDETIMER );
         while( pTool )
            {
            LPTOOL pNextTool;

            pNextTool = pTool->pNextTool;
            FreeMemory( &pTool );
            pTool = pNextTool;
            }
         DeleteFont( pTooltip->hFont );
         FreeMemory( &pTooltip );
         return( 0L );
      }
   return( DefWindowProc( hwnd, message, wParam, lParam ) );
}

/* This function hides the tooltip and sends a TTN_POP
 * to the owner application.
 */
static void FASTCALL HideTooltip( HWND hwnd, LPTOOL pTool )
{
   NMHDR nmhdr;

   if( IsWindowVisible( hwnd ) )
      ShowWindow( hwnd, SW_HIDE );
   if( NULL != pTool )
      Amctl_SendNotify( pTool->ti.hwnd, hwnd, TTN_POP, &nmhdr );
}

/* This function locates a tool given a pointer to a TOOINFO structure
 * in which the hwnd and uID fields have been set to the attributes of
 * the tool to be located. All other entries in the structure are
 * ignored.
 */
static LPTOOL FASTCALL FindTool( LPTOOLTIP pTooltip, LPTOOLINFO lpToolInfo )
{
   LPTOOL pTool;

   pTool = pTooltip->pFirstTool;
   while( pTool )
      {
      if( pTool->ti.hwnd == lpToolInfo->hwnd && pTool->ti.uId == lpToolInfo->uId )
         break;
      pTool = pTool->pNextTool;
      }
   return( pTool );
}

/* Retrieves a tool given the index of the tool, where the first tool
 * has index 0. Not very useful, as all tools must be referenced by their
 * attributes (use FindTool for this), but provided for EnumTools.
 */
static LPTOOL FASTCALL GetTool( LPTOOLTIP pTooltip, int iTool )
{
   LPTOOL pTool;

   pTool = pTooltip->pFirstTool;
   while( pTool && iTool-- )
      pTool = pTool->pNextTool;
   return( pTool );
}

/* This function attempts to retrieve the text for a tooltip. Because the
 * tooltip scheme allows three possible ways to retrieve the text, it seems
 * better to centralise them into one function and cache the text pointer.
 * Note that it is not a good idea to cache if LPSTR_TEXTCALLBACK is specified
 * since this somewhat presumes that the tooltip text is dynamic. However we
 * will cache it for the lifetime of displaying the tooltip at least.
 */
static void FASTCALL GetToolTipText( HWND hwnd, LPTOOL pTool )
{
   static LPSTR pszText;

   INITIALISE_PTR(pszText);
   if( LPSTR_TEXTCALLBACK == pTool->ti.lpszText )
      {
      TOOLTIPTEXT ttt;

      ttt.lpszText = pTool->ti.lpszText;
      ttt.hinst = pTool->ti.hinst;
      ttt.hdr.idFrom = pTool->ti.uId;
      ttt.hdr.hwndFrom = hwnd;
      ttt.hdr.code = TTN_NEEDTEXT;
      if( SendMessage( pTool->ti.hwnd, WM_NOTIFY, pTool->ti.uId, (LPARAM)(LPTOOLTIPTEXT)&ttt ) )
         {
         if( ttt.hinst != NULL )
            {
            if( fNewMemory( &pszText, 256 ) )
               LoadString( ttt.hinst, LOWORD( ttt.lpszText ), pszText, 256 );
            }
         else
            {
            if( fNewMemory( &pszText, strlen( ttt.lpszText ) + 1 ) )
               lstrcpy( pszText, ttt.lpszText );
            }
         }
      }
   else if( 0 == HIWORD( pTool->ti.lpszText ) )
      {
      if( fNewMemory( &pszText, 256 ) )
         LoadString( pTool->ti.hinst, LOWORD( pTool->ti.lpszText ), pszText, 256 );
      }
   else
      {
      if( fNewMemory( &pszText, strlen( pTool->ti.lpszText ) + 1 ) )
         lstrcpy( pszText, pTool->ti.lpszText );
      }

   /* Strip ampersand prefixes unless TTS_NOPREFIX is specified. This
    * allows applications to use the same text for the tooltip as used for
    * the associated menu text.
    */
   if( NULL != pszText && !( GetWindowStyle( hwnd ) & TTS_NOPREFIX ) )
      {
      int s, d;

      for( s = d = 0; pszText[ s ]; ++s )
         {
         if( '&' == pszText[ s ] )
            if( '&' == pszText[ s + 1 ] )
               ++s;
            else
               continue;
         pszText[ d++ ] = pszText[ s ];
         }
      pszText[ d ] = '\0';
      }

   /* Set the new text.
    */
   if( pTool->pszText )
      FreeMemory( &pTool->pszText );
   pTool->pszText = pszText;
}

/* Given a set of (mouse) coordinates in client coordinates, this
 * function determines the tool at those coordinates.
 */
static LPTOOL FASTCALL ToolFromPoint( HWND hwnd, LPTOOLTIP pTooltip, POINT * ppt )
{
   LPTOOL pTool;

   pTool = pTooltip->pFirstTool;
   while( pTool )
      {
      if( pTool->ti.uFlags & TTF_IDISHWND )
         {
         ASSERT( IsWindow( (HWND)pTool->ti.uId ) );
         if( hwnd == (HWND)pTool->ti.uId )
            {
            RECT rc;

            GetClientRect( (HWND)pTool->ti.uId, &rc );
            if( PtInRect( &rc, *ppt ) )
               break;
            }
         }
      else if( PtInRect( &pTool->ti.rect, *ppt ) )
         break;
      pTool = pTool->pNextTool;
      }
   return( pTool );
}

/* This function creates the default font for the tooltip text
 */
static void FASTCALL CreateTooltipFont( LPTOOLTIP pTooltip )
{
   HDC hdc;
   int cPixelsPerInch;
   int cyPixels;

   hdc = GetDC( NULL );
   ASSERT( hdc != NULL );
   cPixelsPerInch = GetDeviceCaps( hdc, LOGPIXELSY );
   cyPixels = -MulDiv( 8, cPixelsPerInch, 72 );
   pTooltip->hFont = CreateFont( cyPixels, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                                 FALSE, ANSI_CHARSET, OUT_CHARACTER_PRECIS, CLIP_DEFAULT_PRECIS,
                                 PROOF_QUALITY, VARIABLE_PITCH | FF_SWISS, "Helv" );
   ReleaseDC( NULL, hdc );
}

/* This function locates the owner window of the specified
 * window and returns TRUE if it has the focus.
 */
static BOOL FASTCALL OwnerHasFocus( HWND hwnd )
{
   while( GetParent( hwnd ) )
      {
      hwnd = GetParent( hwnd );
      if( GetWindowStyle( hwnd ) & WS_POPUP )
         return( TRUE );
      }
   return( GetActiveWindow() == hwnd );
}
