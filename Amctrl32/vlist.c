/* VLIST.C - Handles the big edit window
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

#define  _WIN32_WINNT 0x0400

#include "warnings.h"
#include <windows.h>
#include <windowsx.h>
#include "amctrls.h"
#include "amctrli.h"
#include "string.h"

#define _LB_MOUSESCROLL         0x0118

#ifdef WIN32
static BOOL fWIN32s;          /* TRUE if we're running under WIN32S */
#endif

#define IDS_VLBOXNAME  1
#define VLBEDITID      101

#define HASSTRINGS     0x01       /* List box stores strings */
#define USEDATAVALUES  0x02       /* Use Data Values to talk to parent */
#define MULTIPLESEL    0x04       /* VLB has extended or multiple selection */
#define PARENTFOCUS    0x08       /* Ok for parent to have focus */
#define HASFOCUS       0x10       /* 0x10 - Control has focus */

typedef struct tagVLISTBox {
   HWND hwnd;                 /* hwnd of this VLIST box */
   int nId;                   /* Id of Control */
   HINSTANCE hInstance;       /* Instance of parent */
   HWND hwndParent;           /* hwnd of parent of VLIST box */
   HWND hwndList;             /* hwnd of List box */
   WNDPROC lpfnLBWndProc;     /* Window procedure of list box */
   int nchHeight;             /* Height of text line */
   int nLines;                /* Number of lines in listbox */
   LONG styleSave;            /* Save the Style Bits */
   WORD VLBoxStyle;           /* List Box Style */
   HANDLE hFont;              /* Font for List box */
   LONG lToplIndex;           /* Top logical record number; */
   int nCountInBox;           /* Number of Items in box. */
   LONG lNumLogicalRecs;      /* Number of logical records */
   VLBSTRUCT vlbStruct;       /* Buffer to communicate to app */
   WORD wFlags;               /* Various flags fot the VLB */
   LONG lSelItem;             /* List of selected items */
   int nvlbRedrawState;       /* Redraw State */
   BOOL bHScrollBar;          /* Does it have a H Scroll */
} VLBOX;

typedef VLBOX NEAR * PVLBOX;
typedef VLBOX FAR  * LPVLBOX;

LRESULT EXPORT WINAPI VListBoxWndProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT WINAPI LBSubclassProc( HWND, UINT, WPARAM, LPARAM );

LONG FASTCALL VLBMessageItemHandler( PVLBOX,  UINT, LPSTR );
LONG FASTCALL VLBParentMessageHandler( PVLBOX, UINT, WPARAM, LPARAM );
LONG FASTCALL VLBNcCreateHandler( HWND, LPCREATESTRUCT );
LONG FASTCALL VLBCreateHandler( PVLBOX, HWND, LPCREATESTRUCT );
void FASTCALL VLBNcDestroyHandler(HWND,  PVLBOX, WPARAM, LPARAM );
void FASTCALL VLBSetFontHandler( PVLBOX, HANDLE, BOOL );
int  FASTCALL VLBScrollDownLine( PVLBOX);
int  FASTCALL VLBScrollUpLine( PVLBOX);
int  FASTCALL VLBScrollDownPage( PVLBOX, int );
int  FASTCALL VLBScrollUpPage( PVLBOX, int );
void FASTCALL UpdateVLBWindow( PVLBOX, LPRECT );
int  FASTCALL VLBFindPage( PVLBOX, LONG, BOOL );
int  FASTCALL VLBFindPos( PVLBOX, int );
void FASTCALL VLBFirstPage( PVLBOX);
void FASTCALL VLBLastPage( PVLBOX);
LONG FASTCALL vlbSetCurSel( PVLBOX, int, LONG );
int  FASTCALL vlbFindData( PVLBOX, LONG );
void FASTCALL VLBSizeHandler( PVLBOX, int );
int  FASTCALL vlbInVLB( PVLBOX, LONG );
void FASTCALL VLBCountLines( PVLBOX);
void FASTCALL vlbRedrawOff( PVLBOX );
void FASTCALL vlbRedrawOn( PVLBOX );
BOOL FASTCALL TestSelectedItem( PVLBOX, VLBSTRUCT );
void FASTCALL SetSelectedItem( PVLBOX );
void FASTCALL vlbPGDN( PVLBOX );
void FASTCALL vlbPGUP( PVLBOX );
void FASTCALL vlbLineDn( PVLBOX );
void FASTCALL vlbLineUp( PVLBOX );
void FASTCALL SetFocustoLB( PVLBOX );
void FASTCALL SetFocustoVLB( PVLBOX );

/* This function registers the virtual list class.
 */
BOOL FASTCALL RegisterVListBox( HINSTANCE hInstance )
{
   BOOL bRegistered;
   WNDCLASS wndcls;

   wndcls.style         = CS_DBLCLKS| CS_GLOBALCLASS|CS_PARENTDC;
   wndcls.lpfnWndProc   = VListBoxWndProc;
   wndcls.cbClsExtra    = 0;
   wndcls.cbWndExtra    = sizeof(HANDLE);
   wndcls.hInstance     = hInstance;
   wndcls.hIcon         = NULL;
   wndcls.hCursor       = LoadCursor(NULL, IDC_ARROW);
   wndcls.hbrBackground = NULL;
   wndcls.lpszMenuName  = NULL;
   wndcls.lpszClassName = (LPSTR)VLIST_CLASSNAME;
   bRegistered = RegisterClass( &wndcls );
#ifdef WIN32
   fWIN32s = ((DWORD)GetVersion() & 0x80000000) ? TRUE : FALSE;
#endif
   return( bRegistered );
}

LRESULT EXPORT WINAPI VListBoxWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   PVLBOX pVLBox;
   LONG lRetVal;

  /* Get the pVLBox for the given window now since we will use it a lot in
   * various handlers. This was stored using SetWindowWord(hwnd,0,pVLBox) when
   * we initially created the Virtual List Box control.
   */
#ifdef WIN32
   pVLBox = (PVLBOX)GetWindowLong(hwnd,0);
   if (message != WM_NCCREATE && (DWORD)pVLBox == (DWORD)-1)
      return(0L);
#else
   pVLBox = (PVLBOX)GetWindowWord(hwnd,0);
   if (message != WM_NCCREATE && (WORD)pVLBox == (WORD)-1)
      return(0L);
#endif

  /* Dispatch the various messages we can receive */
  //
  // check for ListBox message coming from application
  // and forward them onto the listbox itself...
  //
#ifdef WIN32  
  if ( ((fWIN32s == TRUE) && (message >= WM_USER && message < VLB_MSGMIN) ) ||  // WIN32s version 
       ((fWIN32s == FALSE) && (message >= LB_ADDSTRING && message < LB_MSGMAX)) // NT version    
     ) {

     return(SendMessage(pVLBox->hwndList, message, wParam, lParam));
  }
#endif

  switch (message)
  {
    case VLB_INITIALIZE:
      SetScrollRange(pVLBox->hwndList, SB_VERT, 0, 100, TRUE);
      pVLBox->vlbStruct.nCtlID = pVLBox->nId;
      SendMessage(pVLBox->hwndParent, VLB_RANGE, 0, (LPARAM)(LPVLBSTRUCT)&(pVLBox->vlbStruct));
      if ( pVLBox->vlbStruct.nStatus == VLB_OK ) {
         pVLBox->lNumLogicalRecs = pVLBox->vlbStruct.lIndex;
      }
      else {
         pVLBox->lNumLogicalRecs = -1L;
         pVLBox->wFlags = pVLBox->wFlags | USEDATAVALUES;
      }

      if ( pVLBox->styleSave & VLBS_USEDATAVALUES )
         pVLBox->wFlags |= USEDATAVALUES;

      if ( pVLBox->lNumLogicalRecs != 0 && pVLBox->nLines != 0)
         VLBFirstPage(pVLBox);

      pVLBox->lSelItem = -1L;

      return VLB_OK;

      break;

    case WM_VSCROLL:
#ifdef WIN32
       switch(LOWORD(wParam)) {
#else
       switch(wParam) {
#endif
          case SB_LINEDOWN:
              VLBScrollDownLine(pVLBox);
              SetSelectedItem(pVLBox);
          break;

          case SB_PAGEDOWN:
              VLBScrollDownPage(pVLBox, 0);
              SetSelectedItem(pVLBox);
          break;

          case SB_LINEUP:
              VLBScrollUpLine(pVLBox);
              SetSelectedItem(pVLBox);
          break;

          case SB_PAGEUP:
              VLBScrollUpPage(pVLBox, 0);
              SetSelectedItem(pVLBox);
          break;

          case SB_TOP:
              VLBFirstPage(pVLBox);
              SetSelectedItem(pVLBox);
          break;

          case SB_BOTTOM:
              VLBLastPage(pVLBox);
              SetSelectedItem(pVLBox);
          break;

          case SB_THUMBPOSITION:
            {
              int nPos;
#ifdef WIN32
              nPos = HIWORD(wParam);
#else
              nPos = LOWORD(lParam);
#endif
              if ( nPos == 0 ) {
                 VLBFirstPage(pVLBox);
                 break;
              }
              else if ( nPos == 100 ) {
                 VLBLastPage(pVLBox);
                 break;
              }

              if ( pVLBox->wFlags & USEDATAVALUES ) {
                    if ( VLBFindPos(pVLBox, nPos) )
                        return VLB_ERR;
                    else
                        SetSelectedItem(pVLBox);
              }
              else {
                 if ( VLBFindPage(pVLBox,
#ifdef WIN32
                 (((LONG)HIWORD(wParam)
#else
                 (((LONG)LOWORD(lParam)
#endif
                   *(pVLBox->lNumLogicalRecs-pVLBox->nLines+1))/100L), TRUE) )
                    return VLB_ERR;
                 else
                    SetSelectedItem(pVLBox);
              }
            }
          break;

       }
       return((LONG)TRUE);
       break;

    case WM_COMMAND:
      /* So that we can handle notification messages from the listbox.
       */
#ifdef WIN32
      if ( HIWORD(wParam) == LBN_SELCHANGE ) {
#else
      if ( HIWORD(lParam) == LBN_SELCHANGE ) {
#endif
         SetSelectedItem(pVLBox);
      }
#ifdef WIN32
      else if (HIWORD(wParam) == LBN_SELCANCEL) {
#else
      else if (HIWORD(lParam) == LBN_SELCANCEL) {
#endif
         pVLBox->lSelItem = -1L;
      }
      if (pVLBox->styleSave & VLBS_NOTIFY ) {
#ifdef WIN32
         return(VLBParentMessageHandler(pVLBox, message, (WPARAM)MAKELONG(pVLBox->nId, HIWORD(wParam)), lParam));

#else
         return(VLBParentMessageHandler(pVLBox, message, (WPARAM)(WORD)pVLBox->nId, lParam));
#endif
      }
      else {
         return TRUE;
      }
      break;

    case WM_CHARTOITEM: {
           long lRet;

           if (pVLBox->styleSave & VLBS_WANTKEYBOARDINPUT ) {
              lRet = VLBParentMessageHandler(pVLBox, WM_CHARTOITEM, wParam, lParam);
           }
           else {
              lRet = -1;
           }

           return lRet;
        }
        break;

#ifdef WIN32
   case WM_MOUSEWHEEL: {
      static int zDeltaAccumulate = 0;
      int zDelta;
      int nLines;

      /* Handle the Intellimouse wheel.
       */
      zDelta = (short)HIWORD( wParam );
      zDeltaAccumulate += zDelta;
      if( nLines = zDeltaAccumulate / WHEEL_DELTA )
         {
         nLines *= iScrollLines;
         while( nLines > 0 )
            {
            SendMessage( hwnd, WM_VSCROLL, SB_LINEUP, 0L );
            --nLines;
            }
         while( nLines < 0 )
            {
            SendMessage( hwnd, WM_VSCROLL, SB_LINEDOWN, 0L );
            ++nLines;
            }
         zDeltaAccumulate = 0;
         }
      break;
      }
#endif

    case WM_VKEYTOITEM: {
           long lRet;

           if (pVLBox->styleSave & VLBS_WANTKEYBOARDINPUT ) {
              lRet = VLBParentMessageHandler(pVLBox, WM_VKEYTOITEM, wParam, lParam);
           }
           else {
              lRet = -1;
           }

           if ( lRet == -1 ) {
#ifdef WIN32
       switch(LOWORD(wParam)) {
#else
       switch(wParam) {
#endif
                 case VK_DOWN:
                    vlbLineDn(pVLBox);
                    lRet = -2L;
                 break;

                 case VK_UP:
                    vlbLineUp(pVLBox);
                    lRet = -2L;
                 break;

                 case VK_PRIOR:
                     vlbPGUP(pVLBox);
                    lRet = -2L;
                 break;

                 case VK_NEXT:
                     vlbPGDN(pVLBox);
                    lRet = -2L;
                 break;

                 case VK_HOME:
                     VLBFirstPage(pVLBox);
                     SendMessage(pVLBox->hwndList, LB_SETCURSEL, 0, 0L);
                     SetSelectedItem(pVLBox);
                     lRet = -2L;
                 break;

                 case VK_END:
                  {
                     int nLastLine;

                     if ( (pVLBox->nLines-1) < pVLBox->nCountInBox )
                        nLastLine = pVLBox->nLines-1;
                     else
                        nLastLine = pVLBox->nCountInBox-1;

                     VLBLastPage(pVLBox);
                     SendMessage(pVLBox->hwndList, LB_SETCURSEL, nLastLine, 0L);
                     SetSelectedItem(pVLBox);
                     lRet = -2L;
                  }
                 break;

                 default:
                    return lRet;
              }
            SendDlgCommand( pVLBox->hwndParent, pVLBox->nId, LBN_SELCHANGE );
           }
           return lRet;
        }
        break;

    case _LB_MOUSESCROLL:
       {
         //
         // The user is holding the mouse down outside the LB....
         // Scroll the VLB as needed.
         //
         // Dont need to LB_SETCURSEL since the LB is going to do this for us.
         //
         int nSelected;

         nSelected = (int)SendMessage(pVLBox->hwndList, LB_GETCURSEL, 0, 0L);
         if ( nSelected == pVLBox->nLines-1 ) {
            VLBScrollDownLine(pVLBox);
            SetSelectedItem(pVLBox);
         }
         else if ( nSelected == 0 ) {
            VLBScrollUpLine(pVLBox);
            SetSelectedItem(pVLBox);
         }
         //
         // Values doesn't mean much since this messsage is passed back to
         // original LB proc
         //
         return 0L;
      }
      break;

    case WM_CREATE:
      /* wParam - not used
         lParam - Points to the CREATESTRUCT data structure for the window.
       */
      return(VLBCreateHandler(pVLBox, hwnd, (LPCREATESTRUCT)lParam));
      break;

    case WM_ERASEBKGND:
      /* Just return 1L so that the background isn't erased */
      return((LONG)TRUE);
      break;

    case WM_GETFONT:
       return((LONG)(LPSTR)(pVLBox->hFont));
       break;

    case WM_GETDLGCODE:
      /* wParam - not used
         lParam - not used */
      return((LONG)(DLGC_WANTCHARS | DLGC_WANTARROWS));
      break;

    case WM_SETFONT:
#ifdef WIN32
      VLBSetFontHandler(pVLBox, (HANDLE)wParam, wParam);
#else
      VLBSetFontHandler(pVLBox, (HANDLE)wParam, LOWORD(lParam));
#endif
      break;

    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
   case WM_CONTEXTMENU: /*!!SM!!*/
    case WM_RBUTTONDOWN:
    case WM_KEYDOWN:
       // Set the focus to the Virtual List Box if we get a mouse click
       // or key press on the parent window.
       SetFocus(pVLBox->hwndList);
       SendMessage(pVLBox->hwndList, message, wParam, lParam);
      break;

    case WM_NCDESTROY:
      /* wParam - used by DefWndProc called within VLBNcDestroyHandler
         lParam - used by DefWndProc called within VLBNcDestroyHandler */
      VLBNcDestroyHandler(hwnd, pVLBox, wParam, lParam);
      break;

    case WM_KILLFOCUS:
          if ( (HWND) wParam != pVLBox->hwndList )
              pVLBox->wFlags  &= ~HASFOCUS;

      break;

    case WM_SETFOCUS:
        {
          int i;

          pVLBox->wFlags  |= HASFOCUS;

          if ( ! (pVLBox->wFlags & PARENTFOCUS) ) {
             if ( (i=vlbInVLB(pVLBox, pVLBox->lSelItem)) >= 0 ) {
                pVLBox->wFlags &= ~PARENTFOCUS;
                SetFocus(pVLBox->hwndList);
             }
             else {
                pVLBox->wFlags |= PARENTFOCUS;
             }
          }
        }
      break;

    case WM_SETREDRAW:
      /* wParam - specifies state of the redraw flag. nonzero = redraw
         lParam - not used */
      if ( wParam) {
         vlbRedrawOn(pVLBox);
      }
      else {
         vlbRedrawOff(pVLBox);
      }
      return TRUE;
      break;

    case WM_ENABLE:
      /* Invalidate the rect to cause it to be drawn in grey for its disabled
       * view or ungreyed for non-disabled view.
       */
      InvalidateRect(pVLBox->hwnd, NULL, FALSE);
      /* Enable/disable the listbox window
       */
      EnableWindow(pVLBox->hwndList, wParam);
      // EnableWindow(pVLBox->hwndScroll, wParam);
      break;

    case WM_SIZE:
      /* wParam - defines the type of resizing fullscreen, sizeiconic,
                  sizenormal etc.
         lParam - new width in LOWORD, new height in HIGHWORD of client area */
      if (!LOWORD(lParam) || !HIWORD(lParam) || !pVLBox->hwndList)
          /* If being sized to a zero width or to a zero height or we aren't
           * fully initialized, just return.
           */
             return(0);
      VLBSizeHandler(pVLBox, 0);
      break;

    case VLB_FINDSTRING:
    case VLB_FINDSTRINGEXACT:
    case VLB_SELECTSTRING:
       {
          VLBSTRUCT FAR *lpvlbInStruct;
          UINT  ReturnMsg;

          ReturnMsg = 0;
          switch (message)
          {
            case VLB_FINDSTRING:
                ReturnMsg = VLBR_FINDSTRING;
                break;

            case VLB_FINDSTRINGEXACT:
                ReturnMsg = VLBR_FINDSTRINGEXACT;
                break;

            case VLB_SELECTSTRING:
                ReturnMsg = VLBR_FINDSTRINGEXACT;
                break;
          }

          lpvlbInStruct = (LPVLBSTRUCT)lParam;

          pVLBox->vlbStruct.lIndex        = lpvlbInStruct->lIndex;
          pVLBox->vlbStruct.lData         = lpvlbInStruct->lData;
          pVLBox->vlbStruct.lpFindString  = lpvlbInStruct->lpFindString;
          pVLBox->vlbStruct.lpTextPointer = NULL;

          pVLBox->vlbStruct.nCtlID = pVLBox->nId;
          SendMessage(pVLBox->hwndParent, ReturnMsg, 0,  (LPARAM)(LPVLBSTRUCT)&(pVLBox->vlbStruct));
          if ( pVLBox->vlbStruct.nStatus == VLB_OK ) {
             if ( pVLBox->wFlags & USEDATAVALUES )
                lRetVal = pVLBox->vlbStruct.lData;
             else
                lRetVal = pVLBox->vlbStruct.lIndex;

             if ( message != VLB_SELECTSTRING ) {
                return lRetVal;
             }

             if ( pVLBox->wFlags & USEDATAVALUES ) {
                //
                // Strings or search for data
                //
                int i;
                if ( (i=vlbFindData(pVLBox, pVLBox->vlbStruct.lData)) != LB_ERR ) {
                    SendMessage(pVLBox->hwndList, LB_SETCURSEL, i, 0L);
                    SetSelectedItem(pVLBox);
                    return lRetVal;
                }
                else {
                   if ( VLBFindPage(pVLBox, pVLBox->vlbStruct.lData, TRUE) )
                        return VLB_ERR;

                   SendMessage(pVLBox->hwndList, LB_SETCURSEL, 0, 0L);
                   SetSelectedItem(pVLBox);
                   return lRetVal;
                }
             }
             else {
                //
                // Is this item in the list box now ??
                //
                if ( pVLBox->vlbStruct.lIndex >= pVLBox->lToplIndex &&
                     pVLBox->vlbStruct.lIndex <= (pVLBox->lToplIndex+(LONG)(pVLBox->nCountInBox)-1)) {
                     int nItemNum;

                     nItemNum = (int) (pVLBox->vlbStruct.lIndex-pVLBox->lToplIndex);
                     SendMessage(pVLBox->hwndList, LB_SETCURSEL, nItemNum, 0L);
                     SetSelectedItem(pVLBox);
                     return lRetVal;
                }
                //
                // OK Adjust to show item
                //
                if ( VLBFindPage(pVLBox, pVLBox->vlbStruct.lIndex, TRUE) )
                    return VLB_ERR;

                SendMessage(pVLBox->hwndList, LB_SETCURSEL, 0, 0L);
                SetSelectedItem(pVLBox);
                return lRetVal;
             }
          }
          else
             return VLB_ERR;
       }
      break;

    case VLB_RESETCONTENT:
      SendMessage(pVLBox->hwndList, LB_RESETCONTENT, 0, 0L);
      SendMessage(pVLBox->hwndParent, VLBN_FREEALL, pVLBox->nId, 0L);
      pVLBox->vlbStruct.nCtlID = pVLBox->nId;
      SendMessage(pVLBox->hwndParent, VLB_RANGE, 0, (LPARAM)(LPVLBSTRUCT)&(pVLBox->vlbStruct));
      if ( pVLBox->vlbStruct.nStatus == VLB_OK )
         pVLBox->lNumLogicalRecs = pVLBox->vlbStruct.lIndex;
      else {
         pVLBox->lNumLogicalRecs = -1L;
         pVLBox->wFlags |= USEDATAVALUES;
      }

      if ( pVLBox->lNumLogicalRecs != 0 )
         VLBFirstPage(pVLBox);

      pVLBox->lSelItem = -1L;

      return VLB_OK;
      break;

    case VLB_SETCURSEL:
       // wParam Has Set Option:
       //   VLBC_FIRST
       //   VLBC_PREV
       //   VLBC_NEXT
       //   VLBC_LAST
       //   VLBC_FINDITEM
       //
       //  lParam has the item number or item data
       if ( lParam == -1L ) {
              SendMessage(pVLBox->hwndList, LB_SETCURSEL, (WORD)-1, 0L);
              pVLBox->lSelItem = -1L;
              return ( -1L );
       }
       else {
          return ( vlbSetCurSel(pVLBox, wParam, lParam) );
       }
       break;

    case VLB_UPDATEPAGE:
         pVLBox->vlbStruct.nCtlID = pVLBox->nId;
         SendMessage(pVLBox->hwndParent, VLB_RANGE, 0, (LPARAM)(LPVLBSTRUCT)&(pVLBox->vlbStruct));
         if ( pVLBox->vlbStruct.nStatus == VLB_OK ) {
            pVLBox->lNumLogicalRecs = pVLBox->vlbStruct.lIndex;
         }
         else {
            pVLBox->lNumLogicalRecs = -1L;
            pVLBox->wFlags |= USEDATAVALUES;
         }

         if ( pVLBox->lNumLogicalRecs == 0L ) {
            SendMessage(pVLBox->hwndList, LB_RESETCONTENT, 0, 0L);
            SendMessage(pVLBox->hwndParent, VLBN_FREEALL, pVLBox->nId, 0L);
            pVLBox->lSelItem = -1L;
         }
         else if ( pVLBox->wFlags & USEDATAVALUES ) {
             if ( VLBFindPage(pVLBox, SendMessage(pVLBox->hwndList, LB_GETITEMDATA, 0, 0L), FALSE ) )
                return VLB_ERR;
         }
         else if ( pVLBox->lNumLogicalRecs <= pVLBox->nLines ) {
            VLBFirstPage(pVLBox);
         }
         else {
             if ( VLBFindPage(pVLBox, pVLBox->lToplIndex, FALSE) )
                return VLB_ERR;
         }
       break;

    case VLB_PAGEUP:
         VLBScrollUpPage(pVLBox, wParam);
         break;

    case VLB_PAGEDOWN:
         VLBScrollDownPage(pVLBox, wParam);
         break;

    case VLB_GETCURSEL:
         return pVLBox->lSelItem;
         break;

    case VLB_GETLINES:
         return (LONG)pVLBox->nLines;
         break;

    case VLB_GETITEMDATA:
        {
          LPVLBSTRUCT lpvlbInStruct;
          int i;
          LPARAM SendlParam;


          lpvlbInStruct = (LPVLBSTRUCT)lParam;

          if (pVLBox->wFlags & USEDATAVALUES) {
             SendlParam = lpvlbInStruct->lData;
          }
          else {
             SendlParam = lpvlbInStruct->lIndex;
          }

          if ( (i=vlbInVLB(pVLBox, SendlParam)) >= 0 ) {
             return(SendMessage(pVLBox->hwndList, LB_GETITEMDATA, i, 0L));
          }
          else {
             pVLBox->vlbStruct.lData = pVLBox->vlbStruct.lIndex = SendlParam;
             pVLBox->vlbStruct.nCtlID = pVLBox->nId;
             SendMessage(pVLBox->hwndParent, VLBR_GETITEMDATA, 0, (LPARAM)(LPVLBSTRUCT)&(pVLBox->vlbStruct));
             if ( pVLBox->vlbStruct.nStatus == VLB_OK )
                return pVLBox->vlbStruct.lData;
             else
                return VLB_ERR;
          }
        }
        break;

    case VLB_GETCOUNT:
      return pVLBox->lNumLogicalRecs;

    case VLB_GETTEXT:
        {
          LPVLBSTRUCT lpvlbInStruct;
          int i;
          LPARAM SendlParam;

          lpvlbInStruct = (LPVLBSTRUCT)lParam;

          if (pVLBox->wFlags & USEDATAVALUES) {
             SendlParam = lpvlbInStruct->lData;
          }
          else {
             SendlParam = lpvlbInStruct->lIndex;
          }

          if ( (i=vlbInVLB(pVLBox, SendlParam)) >= 0 ) {
             return(SendMessage(pVLBox->hwndList, LB_GETTEXT, i, (LPARAM)lpvlbInStruct->lpTextPointer));
          }
          else {
             pVLBox->vlbStruct.lData = pVLBox->vlbStruct.lIndex = SendlParam;
             pVLBox->vlbStruct.nCtlID = pVLBox->nId;
             SendMessage(pVLBox->hwndParent, VLBR_GETTEXT, 0, (LPARAM)(LPVLBSTRUCT)&(pVLBox->vlbStruct));
             if ( pVLBox->vlbStruct.nStatus == VLB_OK ) {
                _fstrcpy(lpvlbInStruct->lpTextPointer, pVLBox->vlbStruct.lpTextPointer);
                return _fstrlen(pVLBox->vlbStruct.lpTextPointer);
             }
             else
                return VLB_ERR;
          }
        }
        break;

    case VLB_GETTEXTLEN:
        {
          LPVLBSTRUCT lpvlbInStruct;
          int i;
          LPARAM SendlParam;

          lpvlbInStruct = (LPVLBSTRUCT)lParam;

          if (pVLBox->wFlags & USEDATAVALUES) {
             SendlParam = lpvlbInStruct->lData;
          }
          else {
             SendlParam = lpvlbInStruct->lIndex;
          }

          if ( (i=vlbInVLB(pVLBox, SendlParam)) >= 0 ) {
             return(SendMessage(pVLBox->hwndList, LB_GETTEXTLEN, i, 0L));
          }
          else {
             pVLBox->vlbStruct.lData = pVLBox->vlbStruct.lIndex = SendlParam;
             pVLBox->vlbStruct.nCtlID = pVLBox->nId;
             return SendMessage(pVLBox->hwndParent, VLBR_GETTEXTLEN, 0, (LPARAM)(LPVLBSTRUCT)&(pVLBox->vlbStruct));
          }
        }
        break;

    case VLB_SETITEMDATA:
        {
          LPVLBSTRUCT lpvlbInStruct;
          int i;
          LPARAM SendlParam;

          lpvlbInStruct = (LPVLBSTRUCT)lParam;

          if (pVLBox->wFlags & USEDATAVALUES) {
             SendlParam = lpvlbInStruct->lData;
          }
          else {
             SendlParam = lpvlbInStruct->lIndex;
          }

          if ( (i=vlbInVLB(pVLBox, SendlParam)) >= 0 ) {
             return(SendMessage(pVLBox->hwndList, LB_SETITEMDATA, i, (LPARAM)lpvlbInStruct->lIndex));
          }
          else return VLB_ERR;
        }
        break;

    case VLB_SETITEMHEIGHT:
          return SendMessage(pVLBox->hwndList, LB_SETITEMHEIGHT, 0, lParam);
        break;

    case VLB_GETITEMHEIGHT:
          return SendMessage(pVLBox->hwndList, LB_GETITEMHEIGHT, 0, 0L);
        break;

    case VLB_GETITEMRECT:
        {
          LPVLBSTRUCT lpvlbInStruct;
          int i;
          LPARAM SendlParam;

          lpvlbInStruct = (LPVLBSTRUCT)lParam;

          if (pVLBox->wFlags & USEDATAVALUES) {
             SendlParam = lpvlbInStruct->lData;
          }
          else {
             SendlParam = lpvlbInStruct->lIndex;
          }

          if ( (i=vlbInVLB(pVLBox, SendlParam)) >= 0 ) {
             return(SendMessage(pVLBox->hwndList, LB_GETITEMRECT, i, (LPARAM)lpvlbInStruct->lpTextPointer));
          }
          else return VLB_ERR;
        }
        break;

    case VLB_GETHORIZONTALEXTENT:
          return SendMessage(pVLBox->hwndList, LB_GETHORIZONTALEXTENT, 0, 0L);
        break;

    case VLB_SETHORIZONTALEXTENT:
        {
          int nOrigin;
          RECT rc;
          int  nScrollBar;

          SendMessage(pVLBox->hwndList, LB_GETITEMRECT, 0,(LPARAM)(LPRECT)&rc);
          nOrigin = rc.left;
          GetClientRect(pVLBox->hwndList, &rc);
          nScrollBar = max((int)wParam - (rc.right - rc.left),0);
          if (nOrigin || nScrollBar) {
             if ( pVLBox->styleSave & VLBS_DISABLENOSCROLL )
                   EnableScrollBar(pVLBox->hwndList, SB_HORZ, ESB_ENABLE_BOTH);
             else {
             if (!pVLBox->bHScrollBar) {
                   pVLBox->bHScrollBar = TRUE;
                   ShowScrollBar(pVLBox->hwndList, SB_HORZ, TRUE);
                   VLBCountLines(pVLBox);
                }
             }
          }
          else {
             if ( pVLBox->styleSave & VLBS_DISABLENOSCROLL )
                   EnableScrollBar(pVLBox->hwndList, SB_HORZ, ESB_DISABLE_BOTH);
             else {
                if (pVLBox->bHScrollBar) {
                   pVLBox->bHScrollBar = FALSE;
                   ShowScrollBar(pVLBox->hwndList, SB_HORZ, FALSE);
                   VLBCountLines(pVLBox);
                }
             }
          }
          return SendMessage(pVLBox->hwndList, LB_SETHORIZONTALEXTENT, wParam, 0L);
        }
        break;

    case VLB_SETTOPINDEX:
        {
          int i, nScroll;

          if ( (i=vlbInVLB(pVLBox, lParam)) > 0 ) {
             nScroll = (int)(-1*(pVLBox->nLines-i));
             VLBScrollDownPage(pVLBox, nScroll);
          }
          else if ( pVLBox->wFlags & USEDATAVALUES ) {
             if ( VLBFindPage(pVLBox, lParam, TRUE ))
                return VLB_ERR;
          }
          else {
             if ( VLBFindPage(pVLBox, lParam, TRUE) )
                return VLB_ERR;
          }

        }
        break;

    case VLB_GETTOPINDEX:
          if ( pVLBox->wFlags & USEDATAVALUES )
             return SendMessage(pVLBox->hwndList, LB_GETITEMDATA, 0, 0L);
          else
             return pVLBox->lToplIndex;
        break;   
        
    case WM_PAINT:  
         DefWindowProc(hwnd, message, wParam, lParam);
         break;              
      
    case WM_MEASUREITEM:
    case WM_DELETEITEM:
    case WM_DRAWITEM:
    case WM_COMPAREITEM:
      return(VLBMessageItemHandler(pVLBox, message, (LPSTR)lParam));
      break;

    case WM_NCCREATE:
      /* wParam - Contains a handle to the window being created
         lParam - Points to the CREATESTRUCT data structure for the window.
       */
      return(VLBNcCreateHandler(hwnd, (LPCREATESTRUCT)lParam));
      break;

    default:
#ifndef WIN32    
      if ( message >= WM_USER )
         return(SendMessage(pVLBox->hwndList, message, wParam, lParam));
      else
#endif      
         return DefWindowProc(hwnd, message, wParam, lParam);
      break;

  } /* switch (message) */

  return((LONG)(LONG)TRUE);
} /* VListBoxWndProc */


LONG FASTCALL VLBMessageItemHandler( PVLBOX pVLBox,  UINT message, LPSTR lpfoo)
/*
 * effects: Handles WM_DRAWITEM,WM_MEASUREITEM,WM_DELETEITEM,WM_COMPAREITEM
 * messages from the listbox.
 */
{
  /*
   * Send the <foo>item message back to the application after changing some
   * parameters to their Virtual List Box specific versions.
   */
  long lRetVal;

  ((LPMEASUREITEMSTRUCT)lpfoo)->CtlType = ODT_LISTBOX;
#ifdef WIN32
  ((LPMEASUREITEMSTRUCT)lpfoo)->CtlID   = (DWORD)pVLBox->nId;
#else
  ((LPMEASUREITEMSTRUCT)lpfoo)->CtlID   = (WORD)pVLBox->nId;
#endif

  if (message == WM_DRAWITEM)
      ((LPDRAWITEMSTRUCT)lpfoo)->hwndItem    = pVLBox->hwnd;
  else if (message == WM_DELETEITEM)
      ((LPDELETEITEMSTRUCT)lpfoo)->hwndItem  = pVLBox->hwnd;
  else if (message == WM_COMPAREITEM)
      ((LPCOMPAREITEMSTRUCT)lpfoo)->hwndItem  = pVLBox->hwnd;

  lRetVal = SendMessage(pVLBox->hwndParent, message,
                    (WPARAM)pVLBox->nId, (LPARAM)lpfoo);

  if (message == WM_MEASUREITEM ) {
     // Size the list box based on height in message
     VLBSizeHandler( pVLBox , ((LPMEASUREITEMSTRUCT)lpfoo)->itemHeight);
  }

  return lRetVal;

}

LONG FASTCALL VLBParentMessageHandler( PVLBOX pVLBox, UINT message, WPARAM wParam, LPARAM lParam)
{
   return( SendMessage(pVLBox->hwndParent, message, wParam, MAKELPARAM(pVLBox->hwnd, HIWORD(lParam) ) ) );
}

int FASTCALL vlbInVLB( PVLBOX pVLBox, LONG lData)
{
   int i;

   if ( pVLBox->wFlags & USEDATAVALUES ) {
      if ( (i=vlbFindData(pVLBox, lData)) != LB_ERR ) {
        return i;
      }
      else {
        return  VLB_ERR;
      }
   }
   else {
      if ( lData >= pVLBox->lToplIndex &&
           lData <= (pVLBox->lToplIndex+(LONG)(pVLBox->nCountInBox)-1)) {
        return (int) (lData - pVLBox->lToplIndex);
      }
      else {
        return  VLB_ERR;
      }
   }
}

int FASTCALL vlbFindData( PVLBOX pVLBox, LONG lData)
{
   int i;

   i = 0;
   while ( i < pVLBox->nCountInBox ) {
      if ( SendMessage(pVLBox->hwndList, LB_GETITEMDATA, i, 0L) == lData )
         return i;
      i++;
   }

   return VLB_ERR;

}

void FASTCALL vlbRedrawOff(PVLBOX pVLBox)
{
     pVLBox->nvlbRedrawState--;
     if ( pVLBox->nvlbRedrawState == 0 ) {
        SendMessage(pVLBox->hwndList, WM_SETREDRAW, 0, 0L);
     }
}

void FASTCALL vlbRedrawOn(PVLBOX pVLBox)
{
     pVLBox->nvlbRedrawState++;
     if ( pVLBox->nvlbRedrawState == 1 ) {
        SendMessage(pVLBox->hwndList, WM_SETREDRAW, 1, 0L);
        InvalidateRect(pVLBox->hwnd, NULL, FALSE);
     }
}


//
// List Box subclass function
//

LRESULT  EXPORT WINAPI LBSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PVLBOX pVLBox;

    switch (message) {
    case WM_VSCROLL:
        //
        // Handle all the scrolling in the Vlist proc
        //
        return SendMessage(GetParent(hwnd), message, wParam, lParam);

    case _LB_MOUSESCROLL:
        //
        // Check for scroll then pass message on the the LB
        //
        SendMessage(GetParent(hwnd), message, wParam, lParam);
      break;

    default:
        break;
    }

    //
    // call the old window proc
    //
#ifdef WIN32
   pVLBox = (PVLBOX) GetWindowLong(GetParent(hwnd),0);
#else
   pVLBox = (PVLBOX) GetWindowWord(GetParent(hwnd),0);
#endif

    if ( message == WM_SETFOCUS ) {
          pVLBox->wFlags  |= HASFOCUS;
    }

    return CallWindowProc((WNDPROC)(pVLBox->lpfnLBWndProc) , hwnd, message, (WPARAM) wParam, (LPARAM)lParam);

}


BOOL FASTCALL TestSelectedItem(PVLBOX pVLBox, VLBSTRUCT vlbStruct)
{
    if ( pVLBox->wFlags & USEDATAVALUES ) {
       if ( pVLBox->lSelItem == vlbStruct.lData )
         return TRUE;
       else
         return FALSE;
    }
    else {
       if ( pVLBox->lSelItem == vlbStruct.lIndex )
         return TRUE;
       else
         return FALSE;
    }
}

void FASTCALL SetSelectedItem(PVLBOX pVLBox)
{
    int nSelected;

    nSelected = (int)SendMessage(pVLBox->hwndList, LB_GETCURSEL, 0, 0L);

    if ( nSelected == LB_ERR )
       return;

    if ( pVLBox->wFlags & USEDATAVALUES ) {
       pVLBox->lSelItem =
            SendMessage(pVLBox->hwndList, LB_GETITEMDATA, nSelected, 0L);
    }
    else {
       pVLBox->lSelItem = (LONG)nSelected + pVLBox->lToplIndex;
    }
}

void FASTCALL vlbLineDn(PVLBOX pVLBox)
{
    //
    // Do we have a selected item ???
    //
    if ( pVLBox->lSelItem != -1L )
    {
        long lSelected;
        if ( (lSelected = SendMessage(pVLBox->hwndList, LB_GETCURSEL,
             0, 0L)) == -1L ) {
               //
               // Current selction is visible....NOT
               //
               vlbRedrawOff(pVLBox);

               //
               // Put selected item at top of LB
               //
               VLBFindPage(pVLBox, pVLBox->lSelItem, TRUE);

               //
               // Scroll the LB down a line
               //
               VLBScrollDownLine(pVLBox);
               SendMessage(pVLBox->hwndList, LB_SETCURSEL, 0, 0L);
               SetFocus(pVLBox->hwndList);
               SetSelectedItem(pVLBox);

               //
               // Update the Screen
               //
               vlbRedrawOn(pVLBox);
        }
        else {
               //
               // Current selection is visible
               //

               //
               // Where is it ????
               //
               if ( (int)lSelected == (pVLBox->nLines-1) ) {
                  //
                  // At the Bottom... Scroll down 1 page less 1 line
                  //                  Put selection at bottom
                  //
                  VLBScrollDownLine(pVLBox);
                  SendMessage(pVLBox->hwndList, LB_SETCURSEL,
                              pVLBox->nLines-1, 0L);
                  SetFocus(pVLBox->hwndList);
                  SetSelectedItem(pVLBox);
               }
               else {
                   SendMessage(pVLBox->hwndList, LB_SETCURSEL,
                               (int)(lSelected)+1, 0L);
                   SetFocus(pVLBox->hwndList);
                   SetSelectedItem(pVLBox);
               }
        }
    }
}

void FASTCALL vlbLineUp(PVLBOX pVLBox)
{
    //
    // Do we have a selected item ???
    //
    if ( pVLBox->lSelItem != -1L )
    {
        long lSelected;
        if ( (lSelected = SendMessage(pVLBox->hwndList, LB_GETCURSEL,
             0, 0L)) == -1L ) {
               //
               // Current selction is visible....NOT
               //
               vlbRedrawOff(pVLBox);

               //
               // Put selected item at top of LB
               //
               VLBFindPage(pVLBox, pVLBox->lSelItem, TRUE);

               //
               // Scroll the LB down a line
               //
               VLBScrollUpLine(pVLBox);
               SendMessage(pVLBox->hwndList, LB_SETCURSEL, 0, 0L);
               SetFocus(pVLBox->hwndList);
               SetSelectedItem(pVLBox);

               //
               // Update the Screen
               //
               vlbRedrawOn(pVLBox);
        }
        else {
               //
               // Current selection is visible
               //

               //
               // Where is it ????
               //
               if ( (int)lSelected == 0 ) {
                  //
                  // At the Bottom... Scroll up 1 page less 1 line
                  //                  Put selection at bottom
                  //
                  VLBScrollUpLine(pVLBox);
                  SendMessage(pVLBox->hwndList, LB_SETCURSEL, 0, 0L);
                  SetFocus(pVLBox->hwndList);
                  SetSelectedItem(pVLBox);
               }
               else {
                   SendMessage(pVLBox->hwndList, LB_SETCURSEL,
                               (int)(lSelected)-1, 0L);
                   SetFocus(pVLBox->hwndList);
                   SetSelectedItem(pVLBox);
               }
        }
    }
}


void FASTCALL vlbPGDN(PVLBOX pVLBox)
{
    //
    // Do we have a selected item ???
    //
    if ( pVLBox->lSelItem != -1L )
    {
        long lSelected;
        if ( (lSelected = SendMessage(pVLBox->hwndList, LB_GETCURSEL,
             0, 0L)) == -1L ) {
               //
               // Current selction is visible....NOT
               //
               vlbRedrawOff(pVLBox);

               //
               // Put selected item at top of LB
               //
               VLBFindPage(pVLBox, pVLBox->lSelItem, TRUE);

               //
               // Scroll the LB down a page
               //
               VLBScrollDownPage(pVLBox, -1);
               SendMessage(pVLBox->hwndList, LB_SETCURSEL, 0, 0L);
               SetFocus(pVLBox->hwndList);
               SetSelectedItem(pVLBox);

               //
               // Update the Screen
               //
               vlbRedrawOn(pVLBox);
        }
        else {
               //
               // Current selection is visible
               //

               //
               // Where is it ????
               //
               if ( lSelected == 0L ) {
                  //
                  // At the top... put selection at bottom
                  //
                  SendMessage(pVLBox->hwndList, LB_SETCURSEL,
                              pVLBox->nLines-1, 0L);
                  SetFocus(pVLBox->hwndList);
                  SetSelectedItem(pVLBox);
               }
               else if ( (int)lSelected == (pVLBox->nLines-1) ) {
                  //
                  // At the Bottom... Scroll down 1 page less 1 line
                  //                  Put selection at bottom
                  //
                  VLBScrollDownPage(pVLBox, -1);
                  SendMessage(pVLBox->hwndList, LB_SETCURSEL,
                              pVLBox->nLines-1, 0L);
                  SetFocus(pVLBox->hwndList);
                  SetSelectedItem(pVLBox);
               }
               else {
                   //
                   // In the middle ... scroll down 1 page less
                   //                   the number of lines
                   //                   the selection is away
                   //                   from the bottom
                   //
                   //                   Put selection at bottom
                   //
                   int nAdjust;
                   nAdjust = -1 * (pVLBox->nLines-(int)lSelected);
                   VLBScrollDownPage(pVLBox, nAdjust);
                   SendMessage(pVLBox->hwndList, LB_SETCURSEL,
                               pVLBox->nLines-1, 0L);
                   SetFocus(pVLBox->hwndList);
                   SetSelectedItem(pVLBox);
               }
        }
    }
}


void FASTCALL vlbPGUP(PVLBOX pVLBox)
{
    //
    // Do we have a selected item ???
    //
    if ( pVLBox->lSelItem != -1L )
    {
        long lSelected;
        if ( (lSelected = SendMessage(pVLBox->hwndList, LB_GETCURSEL,
             0, 0L)) == -1L ) {
               //
               // Current selction is visible....NOT
               //
               vlbRedrawOff(pVLBox);

               //
               // Put selected item at top of LB
               //
               VLBFindPage(pVLBox, pVLBox->lSelItem, TRUE);

               //
               // Scroll the LB UP a page less 1 line
               //
               VLBScrollUpPage(pVLBox, -1);
               SendMessage(pVLBox->hwndList, LB_SETCURSEL, 0, 0L);
               SetFocus(pVLBox->hwndList);
               SetSelectedItem(pVLBox);

               //
               // Update the Screen
               //
               vlbRedrawOn(pVLBox);
        }
        else {
               //
               // Current selction is visible
               //

               //
               // Where is it ????
               //
               if ( lSelected == (pVLBox->nLines-1) ) {
                  //
                  // At the bottom... put selection at top
                  //
                  SendMessage(pVLBox->hwndList, LB_SETCURSEL,
                              0, 0L);
                  SetFocus(pVLBox->hwndList);
                  SetSelectedItem(pVLBox);
               }
               else if ( (int)lSelected ==  0L) {
                  //
                  // At the TOP ... Scroll up 1 page less 1 line
                  //                  Put selection at top
                  //
                  VLBScrollUpPage(pVLBox, -1);
                  SendMessage(pVLBox->hwndList, LB_SETCURSEL,
                              0, 0L);
                  SetFocus(pVLBox->hwndList);
                  SetSelectedItem(pVLBox);
               }
               else {
                   //
                   // In the middle ... scroll up 1 page less
                   //                   the number of lines
                   //                   the selection is away
                   //                   from the top
                   //
                   //                   Put selection at top
                   //
                   int nAdjust;
                   nAdjust = -1 * ((int)lSelected + 1 );
                   VLBScrollUpPage(pVLBox, nAdjust);
                   SendMessage(pVLBox->hwndList, LB_SETCURSEL,
                               0, 0L);
                   SetFocus(pVLBox->hwndList);
                   SetSelectedItem(pVLBox);
               }
        }
    }
}

LONG FASTCALL VLBNcCreateHandler( HWND hwnd, LPCREATESTRUCT lpcreateStruct)
{
  PVLBOX pVLBox;

  //
  // Allocate storage for the vlbox structure
  //
  pVLBox = (PVLBOX) LocalAlloc(LPTR, sizeof(VLBOX));

  if (!pVLBox)
      // Error, no memory
      return((long)NULL);

#ifdef WIN32
  SetWindowLong(hwnd, 0, (WPARAM)pVLBox);
#else
  SetWindowWord(hwnd, 0, (WPARAM)pVLBox);
#endif

  pVLBox->styleSave = lpcreateStruct->style | LBS_NOINTEGRALHEIGHT;

  //
  // Make sure that there are no scroll bar styles
  //
  SetWindowLong(hwnd, GWL_STYLE,
                (LPARAM)(DWORD)( pVLBox->styleSave &
                ~WS_VSCROLL & ~WS_HSCROLL));
  return((LONG)(LPSTR)hwnd);

}


LONG FASTCALL VLBCreateHandler( PVLBOX pVLBox, HWND hwnd, LPCREATESTRUCT lpcreateStruct)
{
  LONG           windowStyle = pVLBox->styleSave;
  RECT           rc;
  TEXTMETRIC     TextMetric;
  HDC            hdc;

  //
  // Initialize Variables
  //
  pVLBox->hwnd            = hwnd;
  pVLBox->hwndParent      = lpcreateStruct->hwndParent;
  pVLBox->nId             = (int) lpcreateStruct->hMenu;
  pVLBox->hInstance       = lpcreateStruct->hInstance;
  pVLBox->nvlbRedrawState = 1;
  pVLBox->lNumLogicalRecs = -2L;
  pVLBox->lSelItem        = -1L;
  pVLBox->wFlags          = 0;
  pVLBox->hwndList        = NULL;

  //
  // Check for USEDATAVALUES
  //
  if ( windowStyle & VLBS_USEDATAVALUES )
      pVLBox->wFlags  |= USEDATAVALUES;
  else
      pVLBox->wFlags &= ~USEDATAVALUES;

  //
  // Dertermine if this VLB is storing string
  //
  pVLBox->wFlags |= HASSTRINGS;
  if ((windowStyle & VLBS_OWNERDRAWFIXED )
     && (!(windowStyle & VLBS_HASSTRINGS)))
     pVLBox->wFlags &= ~HASSTRINGS;

  //
  // Get the font height and Number of lines
  //
  hdc = GetDC(hwnd);
  GetTextMetrics(hdc, &TextMetric);
  ReleaseDC(hwnd,hdc);
  pVLBox->nchHeight = TextMetric.tmHeight;
  GetClientRect(hwnd,&rc);
  pVLBox->nLines = ((rc.bottom - rc.top) / pVLBox->nchHeight);

  //
  // Remove borders and scroll bars, add keyboard input
  //
  windowStyle = windowStyle & ~WS_BORDER & ~WS_THICKFRAME;
  windowStyle = windowStyle & ~WS_VSCROLL & ~WS_HSCROLL;

  //
  // Remove regular list box we don't support
  //
  windowStyle = windowStyle & ~LBS_SORT;
  windowStyle = windowStyle & ~LBS_MULTIPLESEL;
  windowStyle = windowStyle & ~LBS_OWNERDRAWVARIABLE;
  windowStyle = windowStyle & ~LBS_MULTICOLUMN;
  windowStyle = windowStyle & ~VLBS_USEDATAVALUES;

  //
  // Add List box styles we have to have
  //
  windowStyle = windowStyle | LBS_WANTKEYBOARDINPUT;
  windowStyle = windowStyle | LBS_NOINTEGRALHEIGHT;
  windowStyle = windowStyle | LBS_NOTIFY;

  //
  // create the list box window
  //
  pVLBox->hwndList =
    CreateWindowEx((DWORD)0L,
                 (LPSTR)"LISTBOX",(LPSTR)NULL,
                 windowStyle | WS_CHILD,
                 0,
                 0,
                 rc.right,
                 rc.bottom,
                 pVLBox->hwnd,
                 (HMENU)VLBLBOXID,
                 pVLBox->hInstance,
                 NULL);

  if (!pVLBox->hwndList)
      return((LONG)-1L);

  if ( pVLBox->styleSave & VLBS_DISABLENOSCROLL ) {
     ShowScrollBar(pVLBox->hwndList, SB_VERT, TRUE);
     EnableScrollBar(pVLBox->hwndList, SB_VERT, ESB_DISABLE_BOTH);
     if ( pVLBox->styleSave & WS_HSCROLL) {
        pVLBox->bHScrollBar = TRUE;
        ShowScrollBar(pVLBox->hwndList, SB_HORZ, TRUE);
        EnableScrollBar(pVLBox->hwndList, SB_HORZ, ESB_DISABLE_BOTH);
        VLBCountLines(pVLBox);
     }
  }
  else {
     pVLBox->bHScrollBar = FALSE;
  }

  //
  // Subclass the list box
  //
  pVLBox->lpfnLBWndProc = (WNDPROC)SetWindowLong(pVLBox->hwndList, GWL_WNDPROC,
                                        (LONG)(WNDPROC)LBSubclassProc);

  return((LONG)(LPSTR)hwnd);
}

void FASTCALL VLBNcDestroyHandler(HWND hwnd,  PVLBOX pVLBox, WPARAM wParam, LPARAM lParam)
{

  if (pVLBox)
      LocalFree((HANDLE)pVLBox);
      
  // In case rogue messages float through after we have freed the pVLBox, set
  // the handle in the window structure to FFFF and test for this value at the
  // top of the WndProc
  //
#ifdef WIN32
  SetWindowLong(hwnd, 0, (WPARAM)-1);
#else
  SetWindowWord(hwnd, 0, (WPARAM)-1);
#endif

  DefWindowProc(hwnd, WM_NCDESTROY, wParam, lParam);
}


void FASTCALL VLBSetFontHandler( PVLBOX pVLBox, HANDLE hFont, BOOL fRedraw)
{
  pVLBox->hFont = hFont;

#ifdef WIN32
  SendMessage(pVLBox->hwndList, WM_SETFONT, (WPARAM)hFont, (LPARAM)MAKELPARAM(FALSE,0));
#else
  SendMessage(pVLBox->hwndList, WM_SETFONT, (WPARAM)hFont, (LPARAM)FALSE);
#endif

  VLBSizeHandler(pVLBox, 0);

  if (fRedraw)
    {
      InvalidateRect(pVLBox->hwnd, NULL, TRUE);
    }
}


void FASTCALL VLBSizeHandler( PVLBOX pVLBox, int nItemHeight)
//
// Recalculates the sizes of the internal control in response to a
// resizing of the Virtual List Box window.
//
{
  HDC             hdc;
  TEXTMETRIC      TextMetric;
  RECT            rcWindow;
  RECT            rcClient;
  HANDLE          hOldFont;

  //
  // Set the line height
  //
   if(!IsWindow( pVLBox->hwndList ) )
      return;
  if ( nItemHeight ) {
    pVLBox->nchHeight = nItemHeight;
  }
  else if ( (pVLBox->styleSave & VLBS_OWNERDRAWFIXED) ) {
    pVLBox->nchHeight = (int) SendMessage(pVLBox->hwndList, LB_GETITEMHEIGHT, 0,0L);
  }
  else {
    hdc = GetDC(pVLBox->hwndList);
    hOldFont = NULL;
    if (pVLBox->hFont)
       hOldFont = SelectFont(hdc, pVLBox->hFont);
    GetTextMetrics(hdc, &TextMetric);
    pVLBox->nchHeight = TextMetric.tmHeight;
    if (pVLBox->hFont)
       SelectFont(hdc, hOldFont);
    ReleaseDC(pVLBox->hwndList,hdc);
  }

  //
  // Get the main windows client area
  //
  GetClientRect(pVLBox->hwnd,&rcClient);

  //
  // If there is a Window and
  // If the list box is integral height ...
  //
  if ( pVLBox->hwnd && !(pVLBox->styleSave & VLBS_NOINTEGRALHEIGHT) ) {
      //
      // Does it need adjusting ??????
      //
      if (rcClient.bottom % pVLBox->nchHeight) {
          //
          // Adjust
          //
             GetWindowRect(pVLBox->hwnd,&rcWindow);
             SetWindowPos(pVLBox->hwnd, NULL, 0, 0,
                 rcWindow.right - rcWindow.left,
                 ((rcClient.bottom / pVLBox->nchHeight) * pVLBox->nchHeight)
                     + ((rcWindow.bottom-rcWindow.top) - (rcClient.bottom)),
                 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
      }
  }

  //
  // Now adjust the child list box to fill the new main window's
  // client area.
  //
  if ( pVLBox->hwndList ) {
      //
      // Get the main windows client area
      //
      GetClientRect(pVLBox->hwnd,&rcClient);
      if( pVLBox->styleSave & WS_BORDER )
         {
         rcClient.right += (GetSystemMetrics(SM_CXBORDER)*2);
         rcClient.bottom += (GetSystemMetrics(SM_CXBORDER)*2);
         }
      SetWindowPos(pVLBox->hwndList, NULL, 0, 0,
         rcClient.right,
         rcClient.bottom,
         SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
  }

  VLBCountLines( pVLBox);
}



void FASTCALL VLBCountLines( PVLBOX pVLBox)
{
  RECT   rcClient;
  LONG   lFreeItem;

  //
  // calculate the number of lines
  //
  GetClientRect(pVLBox->hwndList,&rcClient);
  pVLBox->nLines = rcClient.bottom / pVLBox->nchHeight;

  //
  // If there is stuff already int the list box
  // update the display ( more items or fewer items now )
  //
  if ( pVLBox->lNumLogicalRecs != -2L ) {
      int nItemsinLB;

      nItemsinLB = (int)SendMessage(pVLBox->hwndList, LB_GETCOUNT, 0, 0L);
      if ( nItemsinLB > pVLBox->nLines ) {
         // Free the items you can see.
         vlbRedrawOff(pVLBox);
         while ( nItemsinLB > pVLBox->nLines ) {
             nItemsinLB--;
             if ( pVLBox->wFlags & USEDATAVALUES ) {
                 lFreeItem = SendMessage(pVLBox->hwndList, LB_GETITEMDATA, nItemsinLB, 0L);
             }
             else {
                 lFreeItem = pVLBox->lToplIndex+nItemsinLB;
             }
             SendMessage(pVLBox->hwndParent, VLBN_FREEITEM, pVLBox->nId, lFreeItem);
             SendMessage(pVLBox->hwndList, LB_DELETESTRING, nItemsinLB, 0L);
         }
         UpdateVLBWindow(pVLBox, NULL);
         vlbRedrawOn(pVLBox);
      }
      else if ( nItemsinLB < pVLBox->nLines) {
         // Add items to fill box.
         //
         // Special case. LB can hold all the items. Jump to top.
         if ( pVLBox->lNumLogicalRecs <= pVLBox->nLines ) {
            VLBFirstPage(pVLBox);
         }
         else if ( pVLBox->wFlags & USEDATAVALUES ) {
             VLBFindPage(pVLBox, SendMessage(pVLBox->hwndList, LB_GETITEMDATA, 0, 0L), FALSE );
         }
         else {
             VLBFindPage(pVLBox, pVLBox->lToplIndex, FALSE);
         }
     }
  }
}

int FASTCALL VLBScrollDownLine( PVLBOX pVLBox)
{
    RECT   UpdRect;
    int    nSelected;
    LONG   lFreeItem;

    if ( pVLBox->wFlags & USEDATAVALUES ) {
        pVLBox->vlbStruct.lIndex = -1L;
        lFreeItem = SendMessage(pVLBox->hwndList, LB_GETITEMDATA, 0, 0L);
    }
    else {
        pVLBox->vlbStruct.lIndex = pVLBox->lToplIndex + pVLBox->nLines - 1L;
        lFreeItem = pVLBox->lToplIndex;
    }

    pVLBox->vlbStruct.lpTextPointer = NULL;
    pVLBox->vlbStruct.lData = SendMessage(pVLBox->hwndList, LB_GETITEMDATA, pVLBox->nCountInBox-1, 0L);
    pVLBox->vlbStruct.nCtlID = pVLBox->nId;
    SendMessage(pVLBox->hwndParent, VLB_NEXT, 0, (LPARAM)((LPVLBSTRUCT)&(pVLBox->vlbStruct)));
    if ( pVLBox->vlbStruct.nStatus == VLB_OK ) {
       nSelected = (int)SendMessage(pVLBox->hwndList, LB_GETCURSEL, 0, 0L);
       if ( nSelected == 0 ) {
           SendMessage(pVLBox->hwndList, LB_SETCURSEL, (WPARAM)-1, (LPARAM)0L);
           SetFocustoVLB(pVLBox);
       }

       //
       // Remove the top String
       //
       SendMessage(pVLBox->hwndList, LB_DELETESTRING, 0, 0L);
       SendMessage(pVLBox->hwndParent, VLBN_FREEITEM, pVLBox->nId, lFreeItem);

       //
       // Add the new line
       //
       if ( pVLBox->wFlags & HASSTRINGS) {
           SendMessage(pVLBox->hwndList, LB_ADDSTRING, 0, (LPARAM) pVLBox->vlbStruct.lpTextPointer);
           SendMessage(pVLBox->hwndList, LB_SETITEMDATA, pVLBox->nCountInBox-1, pVLBox->vlbStruct.lData);
       }
       else
           SendMessage(pVLBox->hwndList, LB_ADDSTRING, 0, (LPARAM) pVLBox->vlbStruct.lData);



       pVLBox->lToplIndex++;
       UpdateVLBWindow(pVLBox, &UpdRect);

       //
       // Tell Windows not to paint the whole LB
       //
       ValidateRect(pVLBox->hwndList, NULL);

       //
       // Scroll the window up
       //
       ScrollWindow(pVLBox->hwndList, 0, (-1)*pVLBox->nchHeight, NULL, NULL);

       //
       // Now tell windows the bottom line needs fixing
       //
       SendMessage(pVLBox->hwndList, LB_GETITEMRECT,
                   pVLBox->nLines-1, (LPARAM) (LPRECT) &UpdRect);

       InvalidateRect(pVLBox->hwndList, &UpdRect, TRUE);

       UpdateWindow(pVLBox->hwndList);

       if ( TestSelectedItem(pVLBox, pVLBox->vlbStruct) ) {
          SendMessage(pVLBox->hwndList, LB_SETCURSEL, pVLBox->nLines-1, 0L);
          SetFocustoLB(pVLBox);
       }
       else if ( nSelected != LB_ERR ) {
          if ( nSelected != 0 ) {
              // Need to move the selection Up 1
              SendMessage(pVLBox->hwndList, LB_SETCURSEL, nSelected-1, 0L);
          }
       }


       return VLB_OK;
    }
    return VLB_ERR;
}

int FASTCALL VLBScrollUpLine( PVLBOX pVLBox)
{
    RECT   UpdRect;
    RECT   ListRect;
    RECT   FAR *lpRect;
    int    nSelected;
    LONG   lFreeItem;

    if ( pVLBox->wFlags & USEDATAVALUES ) {
        pVLBox->vlbStruct.lIndex = -1L;
        lFreeItem = SendMessage(pVLBox->hwndList, LB_GETITEMDATA, pVLBox->nCountInBox-1, 0L);
    }
    else {
        pVLBox->vlbStruct.lIndex = pVLBox->lToplIndex;
        lFreeItem = pVLBox->lToplIndex + pVLBox->nLines - 1L;
    }

    pVLBox->vlbStruct.lpTextPointer = NULL;
    pVLBox->vlbStruct.lData = SendMessage(pVLBox->hwndList, LB_GETITEMDATA, 0, 0L);
    pVLBox->vlbStruct.nCtlID = pVLBox->nId;
    SendMessage(pVLBox->hwndParent, VLB_PREV, 0, (LPARAM)(LPVLBSTRUCT)&(pVLBox->vlbStruct));
    if ( pVLBox->vlbStruct.nStatus  == VLB_OK ) {
       nSelected = (int)SendMessage(pVLBox->hwndList, LB_GETCURSEL, 0, 0L);
       if ( nSelected == pVLBox->nLines-1 ) {
          SendMessage(pVLBox->hwndList, LB_SETCURSEL, (WPARAM)-1, (LPARAM)0L);
          SetFocustoVLB(pVLBox);
       }

       //
       // Remove the bottom String
       //
       SendMessage(pVLBox->hwndList, LB_DELETESTRING, pVLBox->nLines-1, 0L);
       SendMessage(pVLBox->hwndParent, VLBN_FREEITEM, pVLBox->nId, lFreeItem);

       //
       // Add the new line
       //
       if ( pVLBox->wFlags & HASSTRINGS) {
           SendMessage(pVLBox->hwndList, LB_INSERTSTRING, 0, (LPARAM) pVLBox->vlbStruct.lpTextPointer);
           SendMessage(pVLBox->hwndList, LB_SETITEMDATA, 0, pVLBox->vlbStruct.lData);
       }
       else
           SendMessage(pVLBox->hwndList, LB_INSERTSTRING, 0, (LPARAM) pVLBox->vlbStruct.lData);

       pVLBox->lToplIndex--;
       UpdateVLBWindow(pVLBox, &UpdRect);

       //
       // Tell Windows not to paint the whole LB
       //
       ValidateRect(pVLBox->hwndList, NULL);

       //
       // Check for partial line at bottom...
       // if so clear it
       //
       SendMessage(pVLBox->hwndList, LB_GETITEMRECT,
                   pVLBox->nLines-1, (LPARAM)(LPRECT)&UpdRect);

       GetClientRect(pVLBox->hwndList, &ListRect);
       if ( pVLBox->bHScrollBar || (UpdRect.bottom != ListRect.bottom) ) {
           ListRect.bottom = UpdRect.top ;
           lpRect = &ListRect;
       }
       else {
           lpRect = NULL;
       }

       //
       // Scroll the window down
       //
       ScrollWindow(pVLBox->hwndList, 0, pVLBox->nchHeight, lpRect, NULL);

       //
       // Now tell windows the top line needs fixing
       //
       SendMessage(pVLBox->hwndList, LB_GETITEMRECT,
                   0, (LPARAM)(LPRECT)&UpdRect);
       InvalidateRect(pVLBox->hwndList, &UpdRect, TRUE);
       UpdateWindow(pVLBox->hwndList);

       if ( TestSelectedItem(pVLBox, pVLBox->vlbStruct) ) {
          SendMessage(pVLBox->hwndList, LB_SETCURSEL, 0, 0L);
          SetFocustoLB(pVLBox);
       }
       else if ( nSelected != LB_ERR ) {
          if ( nSelected !=  pVLBox->nLines-1) {
              // Need to move the selection Up 1
              SendMessage(pVLBox->hwndList, LB_SETCURSEL, nSelected+1, 0L);
          }
       }


       return VLB_OK;
    }
    return VLB_ERR;
}


int FASTCALL VLBScrollDownPage( PVLBOX pVLBox, int nAdjustment)
{
    int  nCount;
    int  nSelected;
    LONG lFreeItem;

    nSelected = (int)SendMessage(pVLBox->hwndList, LB_GETCURSEL, 0, 0L);
    if ( nSelected == LB_ERR )
        nSelected = -10000;

    SendMessage(pVLBox->hwndList, LB_SETCURSEL, (WPARAM)-1, (LPARAM)0L);
    SetFocustoVLB(pVLBox);

    for ( nCount = 0; nCount < pVLBox->nLines+nAdjustment; nCount++) {
        if ( pVLBox->wFlags & USEDATAVALUES )
            pVLBox->vlbStruct.lIndex = -1L;
        else
            pVLBox->vlbStruct.lIndex = pVLBox->lToplIndex + pVLBox->nLines - 1L;

        pVLBox->vlbStruct.lpTextPointer = NULL;
        pVLBox->vlbStruct.lData = SendMessage(pVLBox->hwndList, LB_GETITEMDATA, pVLBox->nCountInBox-1, 0L);
        pVLBox->vlbStruct.nCtlID = pVLBox->nId;
        SendMessage(pVLBox->hwndParent, VLB_NEXT, 0, (LPARAM)(LPVLBSTRUCT)&(pVLBox->vlbStruct));
        if (  pVLBox->vlbStruct.nStatus == VLB_OK ) {

           if ( nCount == 0 )
              vlbRedrawOff(pVLBox);

           if ( pVLBox->wFlags & USEDATAVALUES ) {
               lFreeItem = SendMessage(pVLBox->hwndList, LB_GETITEMDATA, 0, 0L);
           }
           else {
               lFreeItem = pVLBox->lToplIndex;
           }
           SendMessage(pVLBox->hwndList, LB_DELETESTRING, 0, 0L);
           SendMessage(pVLBox->hwndParent, VLBN_FREEITEM, pVLBox->nId, lFreeItem);

           if ( pVLBox->wFlags & HASSTRINGS) {
               SendMessage(pVLBox->hwndList, LB_ADDSTRING, 0, (LPARAM) pVLBox->vlbStruct.lpTextPointer);
               SendMessage(pVLBox->hwndList, LB_SETITEMDATA, pVLBox->nCountInBox-1, pVLBox->vlbStruct.lData);
           }
           else
               SendMessage(pVLBox->hwndList, LB_ADDSTRING, 0, (LPARAM) pVLBox->vlbStruct.lData);

           if ( TestSelectedItem(pVLBox, pVLBox->vlbStruct) ) {
              nSelected = pVLBox->nLines-1;
           }
           else {
              nSelected--;
           }

           pVLBox->lToplIndex++;

        }
        else {
            if ( nCount == 0 )
                return VLB_ERR;
            break;
        }
    }

    if ( nSelected >= 0 ) {
       SendMessage(pVLBox->hwndList, LB_SETCURSEL, nSelected, 0L);
       SetFocustoLB(pVLBox);
    }
    else {
       SetFocustoVLB(pVLBox);
       SendMessage(pVLBox->hwndList, LB_SETCURSEL, (WPARAM)-1, (LPARAM)0L);
    }

    UpdateVLBWindow(pVLBox, NULL);
    vlbRedrawOn(pVLBox);
    return VLB_OK;

}


int FASTCALL VLBScrollUpPage( PVLBOX pVLBox, int nAdjustment)
{
    int  nCount;
    int  nSelected;
    LONG lFreeItem;

    nSelected = (int)SendMessage(pVLBox->hwndList, LB_GETCURSEL, 0, 0L);
    if ( nSelected == LB_ERR )
        nSelected = -10000;

    SetFocustoVLB(pVLBox);
    SendMessage(pVLBox->hwndList, LB_SETCURSEL, (WPARAM)-1, (LPARAM)0L);

    for ( nCount = 0; nCount < pVLBox->nLines+nAdjustment; nCount++) {
        if ( pVLBox->wFlags & USEDATAVALUES ) {
            pVLBox->vlbStruct.lIndex = -1L;
        }
        else {
            pVLBox->vlbStruct.lIndex = pVLBox->lToplIndex;
        }

        pVLBox->vlbStruct.lpTextPointer = NULL;
        pVLBox->vlbStruct.lData = SendMessage(pVLBox->hwndList, LB_GETITEMDATA, 0, 0L);
        pVLBox->vlbStruct.nCtlID = pVLBox->nId;
        SendMessage(pVLBox->hwndParent, VLB_PREV, 0, (LPARAM)(LPVLBSTRUCT)&(pVLBox->vlbStruct));
        if ( pVLBox->vlbStruct.nStatus  == VLB_OK ) {
           if ( nCount == 0 )
              vlbRedrawOff(pVLBox);

           if ( pVLBox->wFlags & HASSTRINGS) {
               SendMessage(pVLBox->hwndList, LB_INSERTSTRING, 0, (LPARAM) pVLBox->vlbStruct.lpTextPointer);
               SendMessage(pVLBox->hwndList, LB_SETITEMDATA, 0, pVLBox->vlbStruct.lData);
           }
           else
               SendMessage(pVLBox->hwndList, LB_INSERTSTRING, 0, (LPARAM) pVLBox->vlbStruct.lData);

           if ( TestSelectedItem(pVLBox, pVLBox->vlbStruct) ) {
              nSelected = 0;
           }
           else {
              nSelected++;
           }

           if ( pVLBox->wFlags & USEDATAVALUES ) {
               lFreeItem = SendMessage(pVLBox->hwndList, LB_GETITEMDATA, pVLBox->nLines, 0L);
           }
           else {
               lFreeItem = pVLBox->lToplIndex + pVLBox->nLines - 1L;
           }
           SendMessage(pVLBox->hwndList, LB_DELETESTRING, pVLBox->nLines, 0L);
           SendMessage(pVLBox->hwndParent, VLBN_FREEITEM, pVLBox->nId, lFreeItem);

           pVLBox->lToplIndex--;
        }
        else  {
            if ( nCount == 0 )
               return VLB_ERR;
           break;
        }
    }

    if ( nSelected >= 0 && nSelected <= pVLBox->nLines ) {
       SendMessage(pVLBox->hwndList, LB_SETCURSEL, nSelected, 0L);
       SetFocustoLB(pVLBox);
    }
    else {
       SetFocustoVLB(pVLBox);
       SendMessage(pVLBox->hwndList, LB_SETCURSEL, (WPARAM)-1, (LPARAM)0L);
    }

    UpdateVLBWindow(pVLBox, NULL);
    vlbRedrawOn(pVLBox);
    return VLB_OK;
}


void FASTCALL UpdateVLBWindow( PVLBOX pVLBox, LPRECT lpRect)
{
    int   nPos;

    if ( pVLBox->lNumLogicalRecs == -1L )
       SetScrollPos(pVLBox->hwndList, SB_VERT, 50, TRUE);
    else {
       if ( pVLBox->lNumLogicalRecs <= pVLBox->nLines ) {
          if ( pVLBox->styleSave & VLBS_DISABLENOSCROLL )
             EnableScrollBar(pVLBox->hwndList, SB_VERT, ESB_DISABLE_BOTH);
          else
             ShowScrollBar(pVLBox->hwndList, SB_VERT, FALSE);
       }
       else {
           if ( pVLBox->styleSave & VLBS_DISABLENOSCROLL )
              EnableScrollBar(pVLBox->hwndList, SB_VERT, ESB_ENABLE_BOTH);
           else
              ShowScrollBar(pVLBox->hwndList, SB_VERT, TRUE);
           if ( pVLBox->lToplIndex >= (pVLBox->lNumLogicalRecs-pVLBox->nLines) ) {
               nPos = 100;
           }
           else if (pVLBox->lToplIndex == 0L) {
               nPos = 0;
           }
           else {
               nPos = (int) ((pVLBox->lToplIndex*100L) / (pVLBox->lNumLogicalRecs-pVLBox->nLines+1));
           }
           SetScrollPos(pVLBox->hwndList, SB_VERT, nPos, TRUE);
       }

    }

}



int FASTCALL VLBFindPos( PVLBOX pVLBox, int nPos)
{
    pVLBox->vlbStruct.lpTextPointer = NULL;
    pVLBox->vlbStruct.lIndex = (LONG) nPos;
    pVLBox->vlbStruct.lData = (LONG) nPos;
    pVLBox->vlbStruct.nCtlID = pVLBox->nId;
    SendMessage(pVLBox->hwndParent, VLB_FINDPOS, 0, (LPARAM)(LPVLBSTRUCT)&(pVLBox->vlbStruct));
    if ( pVLBox->vlbStruct.nStatus  == VLB_OK ) {
       SetFocustoVLB(pVLBox);
       vlbRedrawOff(pVLBox);
       SendMessage(pVLBox->hwndParent, VLBN_FREEALL, pVLBox->nId, 0L);
       SendMessage(pVLBox->hwndList, LB_RESETCONTENT, 0, 0L);

       if ( pVLBox->wFlags & HASSTRINGS) {
           SendMessage(pVLBox->hwndList, LB_ADDSTRING, 0, (LPARAM) pVLBox->vlbStruct.lpTextPointer);
           SendMessage(pVLBox->hwndList, LB_SETITEMDATA, 0, pVLBox->vlbStruct.lData);
       }
       else
           SendMessage(pVLBox->hwndList, LB_ADDSTRING, 0, (LPARAM) pVLBox->vlbStruct.lData);

       if ( TestSelectedItem(pVLBox, pVLBox->vlbStruct) ) {
          SendMessage(pVLBox->hwndList, LB_SETCURSEL, 0, 0L);
          SetFocustoLB(pVLBox);
       }

       if ( pVLBox->lNumLogicalRecs == -1L )
          pVLBox->lToplIndex = pVLBox->vlbStruct.lData;
       else if ( pVLBox->wFlags & USEDATAVALUES )
          pVLBox->lToplIndex = (LONG)nPos* (pVLBox->lNumLogicalRecs-pVLBox->nLines+1)/100L;
       else
          pVLBox->lToplIndex = pVLBox->vlbStruct.lIndex;

       pVLBox->vlbStruct.lIndex = 0L;
       pVLBox->nCountInBox = 1;
       while ( pVLBox->nCountInBox < pVLBox->nLines ) {
          pVLBox->vlbStruct.nCtlID = pVLBox->nId;
          SendMessage(pVLBox->hwndParent, VLB_NEXT, 0, (LPARAM)(LPVLBSTRUCT)&(pVLBox->vlbStruct));
          if ( pVLBox->vlbStruct.nStatus  == VLB_OK ) {
             if ( pVLBox->wFlags & HASSTRINGS) {
                 SendMessage(pVLBox->hwndList, LB_ADDSTRING, 0, (LPARAM) pVLBox->vlbStruct.lpTextPointer);
                 SendMessage(pVLBox->hwndList, LB_SETITEMDATA, pVLBox->nCountInBox, pVLBox->vlbStruct.lData);
             }
             else
                 SendMessage(pVLBox->hwndList, LB_ADDSTRING, 0, (LPARAM) pVLBox->vlbStruct.lData);

             if ( TestSelectedItem(pVLBox, pVLBox->vlbStruct) ) {
                SendMessage(pVLBox->hwndList, LB_SETCURSEL, 0, 0L);
                SetFocustoLB(pVLBox);
             }

             pVLBox->nCountInBox++;
          }
          else
            break;
       }
       UpdateVLBWindow(pVLBox, NULL);
       vlbRedrawOn(pVLBox);
       return VLB_OK;
    }
    else
        return VLB_ERR;
}


int FASTCALL VLBFindPage( PVLBOX pVLBox, LONG lFindRecNum, BOOL bUpdateTop)
{
    int nSelected;

    nSelected = -1000;
    pVLBox->vlbStruct.lpTextPointer = NULL;
    pVLBox->vlbStruct.lData = pVLBox->vlbStruct.lIndex = lFindRecNum;
    pVLBox->vlbStruct.nCtlID = pVLBox->nId;
    SendMessage(pVLBox->hwndParent, VLB_FINDITEM, 0, (LPARAM)(LPVLBSTRUCT)&(pVLBox->vlbStruct));
    if ( pVLBox->vlbStruct.nStatus  == VLB_OK ) {
       SetFocustoVLB(pVLBox);
       vlbRedrawOff(pVLBox);

       SendMessage(pVLBox->hwndParent, VLBN_FREEALL, pVLBox->nId, 0L);
       SendMessage(pVLBox->hwndList, LB_RESETCONTENT, 0, 0L);

       if ( pVLBox->wFlags & HASSTRINGS) {
           SendMessage(pVLBox->hwndList, LB_ADDSTRING, 0, (LPARAM) pVLBox->vlbStruct.lpTextPointer);
           SendMessage(pVLBox->hwndList, LB_SETITEMDATA, 0, pVLBox->vlbStruct.lData);
       }
       else
           SendMessage(pVLBox->hwndList, LB_ADDSTRING, 0, (LPARAM) pVLBox->vlbStruct.lData);

       if ( TestSelectedItem(pVLBox, pVLBox->vlbStruct) ) {
          nSelected = 0;
       }

       if ( bUpdateTop ) {
           if ( pVLBox->lNumLogicalRecs == -1L )
              pVLBox->lToplIndex = pVLBox->vlbStruct.lData;
           else if ( pVLBox->wFlags & USEDATAVALUES )
              pVLBox->lToplIndex = pVLBox->lNumLogicalRecs/2L;
           else
              pVLBox->lToplIndex = pVLBox->vlbStruct.lIndex;
       }

       pVLBox->vlbStruct.lIndex = lFindRecNum;
       pVLBox->nCountInBox = 1;
       while ( pVLBox->nCountInBox < pVLBox->nLines ) {
          pVLBox->vlbStruct.nCtlID = pVLBox->nId;
          SendMessage(pVLBox->hwndParent, VLB_NEXT, 0, (LPARAM)(LPVLBSTRUCT)&(pVLBox->vlbStruct));
          if ( pVLBox->vlbStruct.nStatus  == VLB_OK ) {
             if ( pVLBox->wFlags & HASSTRINGS) {
                 SendMessage(pVLBox->hwndList, LB_ADDSTRING, 0, (LPARAM) pVLBox->vlbStruct.lpTextPointer);
                 SendMessage(pVLBox->hwndList, LB_SETITEMDATA, pVLBox->nCountInBox, pVLBox->vlbStruct.lData);
             }
             else
                 SendMessage(pVLBox->hwndList, LB_ADDSTRING, 0, (LPARAM) pVLBox->vlbStruct.lData);

             if ( TestSelectedItem(pVLBox, pVLBox->vlbStruct) ) {
                 nSelected = pVLBox->nCountInBox;
             }

             pVLBox->nCountInBox++;
          }
          else
            break;
       }

       if ( pVLBox->nCountInBox < pVLBox->nLines ) {
          if ( pVLBox->wFlags & USEDATAVALUES )
              pVLBox->vlbStruct.lIndex = -1L;
          else
              pVLBox->vlbStruct.lIndex = pVLBox->lToplIndex;
          pVLBox->vlbStruct.lpTextPointer = NULL;
          pVLBox->vlbStruct.lData = SendMessage(pVLBox->hwndList, LB_GETITEMDATA, 0, 0L);
          while ( pVLBox->nCountInBox < pVLBox->nLines ) {
             pVLBox->vlbStruct.nCtlID = pVLBox->nId;
             SendMessage(pVLBox->hwndParent, VLB_PREV, 0, (LPARAM)(LPVLBSTRUCT)&(pVLBox->vlbStruct));
             if ( pVLBox->vlbStruct.nStatus  == VLB_OK ) {
                if ( pVLBox->wFlags & HASSTRINGS) {
                    SendMessage(pVLBox->hwndList, LB_INSERTSTRING, 0, (LPARAM) pVLBox->vlbStruct.lpTextPointer);
                    SendMessage(pVLBox->hwndList, LB_SETITEMDATA, 0, pVLBox->vlbStruct.lData);
                }
                else
                    SendMessage(pVLBox->hwndList, LB_INSERTSTRING, 0, (LPARAM) pVLBox->vlbStruct.lData);

                if ( TestSelectedItem(pVLBox, pVLBox->vlbStruct) ) {
                    nSelected = 0;
                }
                else {
                    nSelected++;
                }

                pVLBox->nCountInBox++;
                if ( pVLBox->lNumLogicalRecs != -1L )
                   pVLBox->lToplIndex--;
             }
             else
                break;
          }
       }

       if ( nSelected >= 0 ) {
          SendMessage(pVLBox->hwndList, LB_SETCURSEL, nSelected, 0L);
          SetFocustoLB(pVLBox);
       }

       UpdateVLBWindow(pVLBox, NULL);
       vlbRedrawOn(pVLBox);
       return VLB_OK;
    }
    else
        return VLB_ERR;
}


void FASTCALL VLBFirstPage( PVLBOX pVLBox)
{
    pVLBox->vlbStruct.lpTextPointer = NULL;
    pVLBox->vlbStruct.lData = pVLBox->vlbStruct.lIndex = 0L;
    pVLBox->vlbStruct.nCtlID = pVLBox->nId;
    SendMessage(pVLBox->hwndParent, VLB_FIRST, 0, (LPARAM)((LPVLBSTRUCT)&(pVLBox->vlbStruct)));
    if ( pVLBox->vlbStruct.nStatus  == VLB_OK ) {
       SetFocustoVLB(pVLBox);
       vlbRedrawOff(pVLBox);
       SendMessage(pVLBox->hwndParent, VLBN_FREEALL, pVLBox->nId, 0L);
       SendMessage(pVLBox->hwndList, LB_RESETCONTENT, 0, 0L);

       if ( pVLBox->wFlags & HASSTRINGS) {
           SendMessage(pVLBox->hwndList, LB_ADDSTRING, 0,  (LPARAM) pVLBox->vlbStruct.lpTextPointer);
           SendMessage(pVLBox->hwndList, LB_SETITEMDATA, 0, pVLBox->vlbStruct.lData);
       }
       else
           SendMessage(pVLBox->hwndList, LB_ADDSTRING, 0,  (LPARAM) pVLBox->vlbStruct.lData);

       if ( TestSelectedItem(pVLBox, pVLBox->vlbStruct) ) {
           SendMessage(pVLBox->hwndList, LB_SETCURSEL, 0, 0L);
           SetFocustoLB(pVLBox);
       }

       if ( pVLBox->lNumLogicalRecs == -1L )
          pVLBox->lToplIndex = pVLBox->vlbStruct.lData;
       else if ( pVLBox->wFlags & USEDATAVALUES )
          pVLBox->lToplIndex = 0L;
       else
          pVLBox->lToplIndex = pVLBox->vlbStruct.lIndex;

       pVLBox->vlbStruct.lIndex = 0L;
       pVLBox->nCountInBox = 1;
       while ( pVLBox->nCountInBox < pVLBox->nLines ) {
          pVLBox->vlbStruct.nCtlID = pVLBox->nId;
          SendMessage(pVLBox->hwndParent, VLB_NEXT, 0, (LPARAM)(LPVLBSTRUCT)&(pVLBox->vlbStruct));
          if ( pVLBox->vlbStruct.nStatus  == VLB_OK ) {
            if ( pVLBox->wFlags & HASSTRINGS) {
                SendMessage(pVLBox->hwndList, LB_ADDSTRING, 0, (LPARAM) pVLBox->vlbStruct.lpTextPointer);
                SendMessage(pVLBox->hwndList, LB_SETITEMDATA, pVLBox->nCountInBox, pVLBox->vlbStruct.lData);
            }
            else
                SendMessage(pVLBox->hwndList, LB_ADDSTRING, 0, (LPARAM) pVLBox->vlbStruct.lData);

            if ( TestSelectedItem(pVLBox, pVLBox->vlbStruct) ) {
               SendMessage(pVLBox->hwndList, LB_SETCURSEL, pVLBox->nCountInBox, 0L);
               SetFocustoLB(pVLBox);
            }

            pVLBox->vlbStruct.lIndex = (LONG) pVLBox->nCountInBox;
            pVLBox->nCountInBox++;
          }
          else
            break;
       }
    }
    UpdateVLBWindow(pVLBox, NULL);
    vlbRedrawOn(pVLBox);
}

void FASTCALL VLBLastPage(PVLBOX pVLBox)
{
    pVLBox->vlbStruct.lpTextPointer = NULL;
    pVLBox->vlbStruct.lData = pVLBox->vlbStruct.lIndex = 0L;
    pVLBox->vlbStruct.nCtlID = pVLBox->nId;
    SendMessage(pVLBox->hwndParent, VLB_LAST, 0, (LPARAM)(LPVLBSTRUCT)&(pVLBox->vlbStruct));
    if ( pVLBox->vlbStruct.nStatus  == VLB_OK ) {
       SetFocustoVLB(pVLBox);
       vlbRedrawOff(pVLBox);
       SendMessage(pVLBox->hwndParent, VLBN_FREEALL, pVLBox->nId, 0L);
       SendMessage(pVLBox->hwndList, LB_RESETCONTENT, 0, 0L);

       if ( pVLBox->wFlags & HASSTRINGS) {
           SendMessage(pVLBox->hwndList, LB_ADDSTRING, 0, (LPARAM) pVLBox->vlbStruct.lpTextPointer);
           SendMessage(pVLBox->hwndList, LB_SETITEMDATA, 0, pVLBox->vlbStruct.lData);
       }
       else
           SendMessage(pVLBox->hwndList, LB_ADDSTRING, 0, (LPARAM) pVLBox->vlbStruct.lData);

       if ( TestSelectedItem(pVLBox, pVLBox->vlbStruct) ) {
          SendMessage(pVLBox->hwndList, LB_SETCURSEL, 0, 0L);
          SetFocustoLB(pVLBox);
       }

       if ( pVLBox->lNumLogicalRecs == -1L )
          pVLBox->lToplIndex = pVLBox->vlbStruct.lData;
       else if ( pVLBox->wFlags & USEDATAVALUES )
          pVLBox->lToplIndex = pVLBox->lNumLogicalRecs;
       else
          pVLBox->lToplIndex = pVLBox->vlbStruct.lIndex;

       pVLBox->nCountInBox = 1;
       while ( pVLBox->nCountInBox < pVLBox->nLines ) {
          pVLBox->vlbStruct.nCtlID = pVLBox->nId;
          SendMessage(pVLBox->hwndParent, VLB_PREV, 0, (LPARAM)(LPVLBSTRUCT)&(pVLBox->vlbStruct));
          if ( pVLBox->vlbStruct.nStatus  == VLB_OK ) {
             if ( pVLBox->wFlags & HASSTRINGS) {
                 SendMessage(pVLBox->hwndList, LB_INSERTSTRING, 0, (LPARAM) pVLBox->vlbStruct.lpTextPointer);
                 SendMessage(pVLBox->hwndList, LB_SETITEMDATA, 0, pVLBox->vlbStruct.lData);
             }
             else
                 SendMessage(pVLBox->hwndList, LB_INSERTSTRING, 0, (LPARAM) pVLBox->vlbStruct.lData);

             if ( TestSelectedItem(pVLBox, pVLBox->vlbStruct) ) {
                 SendMessage(pVLBox->hwndList, LB_SETCURSEL, pVLBox->nCountInBox, 0L);
                 SetFocustoLB(pVLBox);
             }

             pVLBox->nCountInBox++;
             if ( pVLBox->lNumLogicalRecs != -1L )
                pVLBox->lToplIndex--;
          }
          else
             break;
       }

       UpdateVLBWindow(pVLBox, NULL);
       vlbRedrawOn(pVLBox);
    }
}


void FASTCALL SetFocustoLB(PVLBOX pVLBox)
{
   pVLBox->wFlags &= ~PARENTFOCUS;
   if ( pVLBox->wFlags & HASFOCUS ) {
      SetFocus(pVLBox->hwndList);
   }
}


void FASTCALL SetFocustoVLB(PVLBOX pVLBox)
{
   pVLBox->wFlags |= PARENTFOCUS;
   if ( pVLBox->wFlags & HASFOCUS ) {
      SetFocus(pVLBox->hwnd);
   }
}

LONG FASTCALL vlbSetCurSel( PVLBOX pVLBox, int nOption, LONG lParam)
{
   int i;
   if ( pVLBox->wFlags & USEDATAVALUES ) {
       switch ( nOption) {

          case VLB_FIRST:
              VLBFirstPage(pVLBox);
              SendMessage(pVLBox->hwndList, LB_SETCURSEL, 0, 0L);
              SetSelectedItem(pVLBox);
          break;

          case VLB_PREV:
             if ( (i=vlbFindData(pVLBox, lParam)) == LB_ERR ) {
                if  ( VLBFindPage(pVLBox, (LONG)lParam, TRUE) ) {
                    return VLB_ERR;
                }
                if ( pVLBox->nCountInBox < pVLBox->nLines ) {
                   VLBLastPage(pVLBox);
                }
                else {
                    InvalidateRect(pVLBox->hwndList, NULL, TRUE);
                    UpdateWindow(pVLBox->hwndList);
                }
             }
             i=vlbFindData(pVLBox, lParam);
             if ( i == 0 ) {
                if ( VLBScrollUpLine(pVLBox) )
                    return VLB_ERR;
                else
                    SendMessage(pVLBox->hwndList, LB_SETCURSEL, 0, 0L);
                    SetSelectedItem(pVLBox);
             }
             else {
                SendMessage(pVLBox->hwndList, LB_SETCURSEL, i-1, 0L);
                SetSelectedItem(pVLBox);
             }
          break;

          case VLB_NEXT:
             if ( (i=vlbFindData(pVLBox, lParam)) == LB_ERR ) {
                if  ( VLBFindPage(pVLBox, (LONG)lParam, TRUE) ) {
                    return VLB_ERR;
                }
                UpdateWindow(pVLBox->hwndList);
                i=vlbFindData(pVLBox, lParam);
             }
             if ( i == (pVLBox->nCountInBox-1) ) {
                if ( VLBScrollDownLine(pVLBox) )
                    return VLB_ERR;
                else
                    SendMessage(pVLBox->hwndList, LB_SETCURSEL, pVLBox->nCountInBox-1, 0L);
                    SetSelectedItem(pVLBox);
             }
             else {
                if ( SendMessage(pVLBox->hwndList, LB_SETCURSEL, i+1, 0L) == -1L )
                    return VLB_ERR;
                SetSelectedItem(pVLBox);
             }
          break;

          case VLB_LAST:
              VLBLastPage(pVLBox);
              SendMessage(pVLBox->hwndList, LB_SETCURSEL, pVLBox->nCountInBox-1, 0L);
              SetSelectedItem(pVLBox);
          break;

          case VLB_FINDITEM:
             if ( (i=vlbFindData(pVLBox, lParam)) == LB_ERR ) {
                vlbRedrawOff(pVLBox);
                if  ( VLBFindPage(pVLBox, (LONG)lParam, TRUE) )
                    return VLB_ERR;
                else {
                    if ( pVLBox->nCountInBox < pVLBox->nLines ) {
                       VLBLastPage(pVLBox);
                    }
                    i=vlbFindData(pVLBox, lParam);
                    SendMessage(pVLBox->hwndList, LB_SETCURSEL, i, 0L);
                    pVLBox->lSelItem = (LONG) lParam;
                }
                vlbRedrawOn(pVLBox);
             }
             else {
                SendMessage(pVLBox->hwndList, LB_SETCURSEL, i, 0L);
                SetSelectedItem(pVLBox);
             }
          break;
       }
   }
   else {
       pVLBox->vlbStruct.lIndex = lParam;
       switch ( nOption) {
          case VLB_FIRST:
              VLBFirstPage(pVLBox);
              SendMessage(pVLBox->hwndList, LB_SETCURSEL, 0, 0L);
              SetSelectedItem(pVLBox);
          break;

          case VLB_LAST:
              VLBLastPage(pVLBox);
              SendMessage(pVLBox->hwndList, LB_SETCURSEL, pVLBox->nCountInBox-1, 0L);
              SetSelectedItem(pVLBox);
          break;

          case VLB_PREV:
             if ( pVLBox->vlbStruct.lIndex > pVLBox->lToplIndex &&
                  pVLBox->vlbStruct.lIndex <= (pVLBox->lToplIndex+(LONG)(pVLBox->nCountInBox)-1)) {
                if ( SendMessage(pVLBox->hwndList, LB_SETCURSEL, (int)(pVLBox->vlbStruct.lIndex-pVLBox->lToplIndex)-1, 0L) == -1L)
                    return VLB_ERR;
                else
                    SetSelectedItem(pVLBox);
             }
             else {
                if ( VLBScrollUpLine(pVLBox) )
                    return VLB_ERR;
                else {
                    SendMessage(pVLBox->hwndList, LB_SETCURSEL, 0, 0L);
                    SetSelectedItem(pVLBox);
                }
             }
          break;

          case VLB_NEXT:
             if ( pVLBox->vlbStruct.lIndex >= pVLBox->lToplIndex &&
                  pVLBox->vlbStruct.lIndex < (pVLBox->lToplIndex+(LONG)(pVLBox->nCountInBox)-1)) {
                if ( SendMessage(pVLBox->hwndList, LB_SETCURSEL, (int)(pVLBox->vlbStruct.lIndex-pVLBox->lToplIndex)+1, 0L) == -1L )
                    return VLB_ERR;
                else
                    SetSelectedItem(pVLBox);
             }
             else {
                if ( VLBScrollDownLine(pVLBox) )
                    return VLB_ERR;
                else {
                    SendMessage(pVLBox->hwndList, LB_SETCURSEL, pVLBox->nLines-1, 0L);
                    SetSelectedItem(pVLBox);
                }
             }
          break;

          case VLB_FINDITEM:
             if ( pVLBox->vlbStruct.lIndex >= pVLBox->lToplIndex &&
                  pVLBox->vlbStruct.lIndex <= (pVLBox->lToplIndex+(LONG)(pVLBox->nCountInBox)-1)) {
                SendMessage(pVLBox->hwndList, LB_SETCURSEL, (int)(pVLBox->vlbStruct.lIndex-pVLBox->lToplIndex), lParam);
                SetSelectedItem(pVLBox);
             }
             else {
                if ( VLBFindPage(pVLBox, pVLBox->vlbStruct.lIndex, TRUE) )
                    return VLB_ERR;
                else {
                    SendMessage(pVLBox->hwndList, LB_SETCURSEL, (int)(lParam-pVLBox->lToplIndex), 0L);
                    SetSelectedItem(pVLBox);
                }
             }
          break;
       }
   }
   return (LONG)VLB_OK;
}
