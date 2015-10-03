/* TOPICLST.C - Implements the topic list control
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
#include "kludge.h"

#define  THIS_FILE   __FILE__

#define  GWW_EXTRA            4
#define  GetBlock(h)          ((TOPICLST FAR *)GetWindowLong((h),0))
#define  PutBlock(h,b)        (SetWindowLong((h),0,(LONG)(b)))

#define  idHdr                (HMENU)0x2000
#define  idList               (HMENU)0x3000

typedef struct tagTOPICLST {
   HWND hwndList;                   /* Handle of list box control */
   HWND hwndHdr;                    /* Handle of header control */
} TOPICLST;

#ifndef WIN32
#define GET_WM_COMMAND_CMD(wp, lp)              HIWORD(lp)
#endif

LRESULT EXPORT CALLBACK TopicListWndFn( HWND, UINT, WPARAM, LPARAM );

/* This function registers the topic list control window class.
 */
BOOL FASTCALL RegisterTopicListClass( HINSTANCE hInst )
{
   WNDCLASS wc;

   wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
   wc.hIcon          = NULL;
   wc.lpszMenuName   = NULL;
   wc.lpszClassName  = WC_TOPICLST;
   wc.hbrBackground  = (HBRUSH)( COLOR_BTNFACE+1 );
   wc.hInstance      = hInst;
   wc.style          = CS_GLOBALCLASS | CS_DBLCLKS;
   wc.lpfnWndProc    = TopicListWndFn;
   wc.cbClsExtra     = 0;
   wc.cbWndExtra     = GWW_EXTRA;
   return( RegisterClass( &wc ) );
}

LRESULT EXPORT CALLBACK TopicListWndFn( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   TOPICLST FAR * pTopicList;

#ifdef _DEBUG
   /* Help us to catch NULL pointers...
    */
   pTopicList = NULL;
#endif
   switch( message )
      {
      case WM_CREATE: {
         static TOPICLST FAR * pNewTopicList;

         INITIALISE_PTR(pNewTopicList);
         if( fNewMemory( &pNewTopicList, sizeof( TOPICLST ) ) )
            {
            LPCREATESTRUCT lpcs;
            DWORD dwListStyle;
            DWORD dwHdrStyle;
            HWND hwndList;
            HWND hwndHdr;

            /* Compute the styles.
             */
            lpcs = (LPCREATESTRUCT)lParam;
            dwListStyle = WS_CHILD|WS_VISIBLE|LBS_NOINTEGRALHEIGHT|LBS_NOTIFY|LBS_SORT|WS_VSCROLL|WS_CLIPSIBLINGS;
            dwHdrStyle = WS_CHILD|WS_VISIBLE|ACTL_CCS_NOPARENTALIGN|ACTL_CCS_NORESIZE;
            if( lpcs->style & TLS_OWNERDRAWFIXED )
               dwListStyle |= LBS_OWNERDRAWFIXED;
            if( lpcs->style & TLS_OWNERDRAWVARIABLE )
               dwListStyle |= LBS_OWNERDRAWVARIABLE;
            if( lpcs->style & TLS_HASSTRINGS )
               dwListStyle |= LBS_HASSTRINGS;
            if( lpcs->style & TLS_SORT )
               dwListStyle |= LBS_SORT;
            if( lpcs->style & TLS_MULTIPLESEL )
               dwListStyle |= LBS_MULTIPLESEL;
            if( lpcs->style & TLS_EXTENDEDSEL )
               dwListStyle |= LBS_EXTENDEDSEL;
            if( lpcs->style & TLS_BUTTONS )
               dwHdrStyle |= HDS_BUTTONS;

            /* Create the header and listbox controls.
             */
            hwndHdr = CreateWindow( WC_HEADER, "", dwHdrStyle, 0, 0, 0, 0, hwnd, idHdr, hLibInst, NULL );
            if( lpcs->style & TLS_VLIST )
               hwndList = CreateWindow( VLIST_CLASSNAME, "", dwListStyle, 0, 0, 0, 0, hwnd, idList, hLibInst, NULL );
            else
               hwndList = CreateWindow( "listbox", "", dwListStyle, 0, 0, 0, 0, hwnd, idList, hLibInst, NULL );
            if( NULL == hwndList || NULL == hwndHdr )
               return( -1 );
            pNewTopicList->hwndHdr = hwndHdr;
            pNewTopicList->hwndList = hwndList;
            PutBlock( hwnd, pNewTopicList );
            }
         return( pNewTopicList ? 0 : -1 );
         }

      case WM_COMMAND:
         /* Replace ID of WM_COMMAND with that of our
          * control.
          */
         return( SendDlgCommand( GetParent( hwnd ), GetDlgCtrlID( hwnd ), GET_WM_COMMAND_CMD(wParam,lParam) ) );

      case VLBR_FINDSTRING:
      case VLBR_FINDSTRINGEXACT:
      case VLBR_SELECTSTRING:
      case VLBR_GETITEMDATA:
      case VLBR_GETTEXT:
      case VLBR_GETTEXTLEN:
      case VLB_FIRST:
      case VLB_PREV:
      case VLB_NEXT:
      case VLB_LAST:
      case VLB_FINDITEM:
      case VLB_RANGE:
      case VLB_FINDPOS:
      case VLBN_FREEITEM:
      case VLBN_FREEALL:
      case WM_MEASUREITEM:
      case WM_COMPAREITEM:
      case WM_DELETEITEM:
      case WM_VKEYTOITEM:
      case WM_CHARTOITEM:
      case WM_NOTIFY:
         /* Pass these messages on to the
          * parent window.
          */
         return( SendMessage( GetParent( hwnd ), message, wParam, lParam ) );

      case WM_DRAWITEM: {
         const DRAWITEMSTRUCT FAR * lpDrawItem;
         DRAWITEMSTRUCT dis;

         /* Fake WM_DRAWITEM so that the user always sees the
          * ID of the topic list control.
          */
         lpDrawItem = (const DRAWITEMSTRUCT FAR *)lParam;
         dis = *lpDrawItem;
         dis.CtlID = GetDlgCtrlID( hwnd );
         return( SendMessage( GetParent( hwnd ), message, wParam, (LPARAM)(LPSTR)&dis ) );
         }

   #ifdef WIN32
      case WM_CTLCOLORLISTBOX:
         return( SendMessage( GetParent( hwnd ), message, wParam, GetDlgCtrlID( hwnd ) ) );
   #else
      case WM_CTLCOLOR:
         lParam = MAKELPARAM( GetDlgCtrlID( hwnd ), HIWORD( lParam ) );
         return( SendMessage( GetParent( hwnd ), message, wParam, lParam ) );
   #endif

   #ifdef WIN32
      case LB_ADDFILE:
      case LB_GETANCHORINDEX:
      case LB_GETLOCALE:
      case LB_SELITEMRANGEEX:
      case LB_SETANCHORINDEX:
      case LB_SETCOUNT:
      case LB_SETLOCALE:
   #endif
      case LB_ADDSTRING:
      case LB_DELETESTRING:
      case LB_DIR:
      case LB_FINDSTRING:
      case LB_FINDSTRINGEXACT:
      case LB_GETCARETINDEX:
      case LB_GETCOUNT:
      case LB_GETCURSEL:
      case LB_GETHORIZONTALEXTENT:
      case LB_GETITEMDATA:
      case LB_GETITEMHEIGHT:
      case LB_GETITEMRECT:
      case LB_GETSEL:
      case LB_GETSELCOUNT:
      case LB_GETSELITEMS:
      case LB_GETTEXT:
      case LB_GETTEXTLEN:
      case LB_GETTOPINDEX:
      case LB_INSERTSTRING:
      case LB_RESETCONTENT:
      case LB_SELECTSTRING:
      case LB_SELITEMRANGE:
      case LB_SETCARETINDEX:
      case LB_SETCOLUMNWIDTH:
      case LB_SETCURSEL:
      case LB_SETHORIZONTALEXTENT:
      case LB_SETITEMDATA:
      case LB_SETITEMHEIGHT:
      case LB_SETSEL:
      case LB_SETTABSTOPS:
      case LB_SETTOPINDEX:
         /* Pass on messages for the listbox
          * control to the list control.
          */
         pTopicList = GetBlock( hwnd );
         return( SendMessage( pTopicList->hwndList, message, wParam, lParam ) );

      case VLB_RESETCONTENT:
      case VLB_SETCURSEL:
      case VLB_GETCURSEL:
      case VLB_GETTEXT:
      case VLB_GETTEXTLEN:
      case VLB_GETCOUNT:
      case VLB_SELECTSTRING:
      case VLB_FINDSTRING:
      case VLB_GETITEMRECT:
      case VLB_GETITEMDATA:
      case VLB_SETITEMDATA:
      case VLB_SETITEMHEIGHT:
      case VLB_GETITEMHEIGHT:
      case VLB_FINDSTRINGEXACT:
      case VLB_INITIALIZE:
      case VLB_SETTABSTOPS:
      case VLB_GETTOPINDEX:
      case VLB_SETTOPINDEX:
      case VLB_GETHORIZONTALEXTENT:
      case VLB_SETHORIZONTALEXTENT:
      case VLB_UPDATEPAGE:
      case VLB_GETLINES:
      case VLB_PAGEDOWN:
      case VLB_PAGEUP:
         /* Pass on messages for the virtual listbox
          * control to the list control.
          */
         pTopicList = GetBlock( hwnd );
         return( SendMessage( pTopicList->hwndList, message, wParam, lParam ) );

      case HDM_GETITEMCOUNT:
      case HDM_INSERTITEM:
      case HDM_DELETEITEM:
      case HDM_GETITEM:
      case HDM_SETITEM:
      case HDM_LAYOUT:
      case HDM_HITTEST:
      case HDM_SETEXTENT:
      case HDM_SHOWITEM:
      case WM_SETFONT:
      case WM_SETTEXT:
         /* Pass on messages for the header
          * control to the header.
          */
         pTopicList = GetBlock( hwnd );
         return( SendMessage( pTopicList->hwndHdr, message, wParam, lParam ) );

      case WM_SETFOCUS:
         pTopicList = GetBlock( hwnd );
         SetFocus( pTopicList->hwndList );
         return( 0L );

      case WM_NCHITTEST:
         return( HTTRANSPARENT );

      case WM_SIZE: {
         int cx, cy;
         int cyHdr;
         RECT rc;

         /* Resize the header and listbox.
          */
         pTopicList = GetBlock( hwnd );
         SetRect( &rc, 0, 0, LOWORD( lParam ), HIWORD( lParam ) );
         InflateRect( &rc, -2, -2 );
         cx = rc.right - rc.left;
         cy = rc.bottom - rc.top;
         cyHdr = 20;
         SetWindowPos( pTopicList->hwndHdr, NULL, rc.left, rc.top, cx, cyHdr, SWP_NOZORDER|SWP_NOACTIVATE );
         SetWindowPos( pTopicList->hwndList, NULL, rc.left, rc.top + cyHdr, cx, cy - cyHdr, SWP_NOZORDER|SWP_NOACTIVATE );
         break;
         }

      case WM_PAINT: {
         PAINTSTRUCT ps;
         RECT rc;
         HDC hdc;

         /* Paint a 3D border around the control.
          */
         hdc = BeginPaint( hwnd, &ps );
         GetClientRect( hwnd, &rc );
         Draw3DFrame( hdc, &rc );
         EndPaint( hwnd, &ps );
         return( 0L );
         }

      case WM_DESTROY:
         pTopicList = GetBlock( hwnd );
         DestroyWindow( pTopicList->hwndHdr );
         DestroyWindow( pTopicList->hwndList );
         FreeMemory( &pTopicList );
         return( 0L );
      }
   return( DefWindowProc( hwnd, message, wParam, lParam ) );
}
