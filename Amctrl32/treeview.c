/* TREEVIEW.C - Implements the treeview control
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
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "amctrls.h"
#include "amctrli.h"
#include "kludge.h"

#define  THIS_FILE   __FILE__

#define  MIN_INDENTATION         18
#define  DEF_INDENTATION         19
#define  EDITZONEWIDTH           20

#define  IDT_EDITTIMER           42
#define  IDD_TREEVIEW_EDIT_CTL   560

#define  GWW_EXTRA            4
#define  GetBlock(h)          ((TREEVIEW FAR *)GetWindowLong((h),0))
#define  PutBlock(h,b)        (SetWindowLong((h),0,(LONG)(b)))

typedef struct tagITEMDATA {
   LPSTR pszText;                      /* The text of the item */
   RECT rcItem;                        /* Visible rectangle of item */
   UINT state;                         /* Item state */
   int nLevel;                         /* Indentation level (0 == root) */
   int iImage;                         /* Index of image in image list */
   int iSelectedImage;                 /* Index of selected image in image list */
   int cChildren;                      /* Number of child items */
   DWORD lParam;                       /* 32-bit user defined parameter */
} ITEMDATA;

typedef struct tagITEM {
   struct tagITEM FAR * pNextItem;     /* Pointer to next sibling item */
   struct tagITEM FAR * pPrevItem;     /* Pointer to previous sibling item */
   struct tagITEM FAR * pParentItem;   /* Pointer to parent item */
   struct tagITEM FAR * pFirstChild;   /* Pointer to first child */
   int    cTrueChildren;               /* The real number of child items */
   ITEMDATA id;                        /* Item data */
} ITEM;

typedef struct tagTREEVIEW {
   ITEM FAR * pRootItem;               /* Pointer to root item */
   ITEM FAR * pFirstVisible;           /* Pointer to item at top of listbox */
   ITEM FAR * pLastVisible;            /* Pointer to item at bottom of listbox */
   ITEM FAR * pSelectedItem;           /* Pointer to selected item */
   ITEM FAR * pEditItem;               /* Pointer to item being edited */
   ITEM FAR * pTooltipItem;            /* Pointer to item indicated by tooltip */
   HWND hwndTooltip;                   /* Tooltip window */
   WNDPROC lpfOldEditProc;             /* Subclassed edit control old window procedure */
   HIMAGELIST himlNormal;              /* Normal image list */
   HIMAGELIST himlState;               /* State image list */
   COLORREF crText;                    /* Text colour */
   COLORREF crBack;                    /* Background colour */
   int cxImage;                        /* Width of each image */
   int cyImage;                        /* Height of each image */
   HWND hwndEdit;                      /* Handle of edit control */
   int dxEdit;                         /* Width of edit control format rectangle */
   int cRedraw;                        /* Zero if we redraw after items changed */
   BOOL fMousedown;                    /* TRUE if mouse button down over a label */
   BOOL fEndingLabelEdit;              /* TRUE if ending label edit */
   POINT pt;                           /* Client coordinates of mouse when dragging */
   int cExpandedItems;                 /* Number of expanded items */
   int cVisibleItems;                  /* Number of visible items */
   int nScrollPos;                     /* Scroll index position */
   int cItems;                         /* Number of items */
   int cIndent;                        /* Indentation in pixels */
   HFONT hFont;                        /* Font used to draw treeview text */
   int dyFont;                         /* Height of text drawn using specified font */
   int dyLine;                         /* Height of one line */
   int dyEditTopBorder;                /* Height of border/tetx gap in edit control */
   char szScan[ 32 ];                  /* Incremental search string */
   int cScan;                          /* Index into szScan */
   BOOL fDragging;                     /* TRUE if dragging */ //!!SM!!
} TREEVIEW;

static void FASTCALL Treeview_OnVScroll(HWND , HWND , UINT , int );

static ITEM FAR * FASTCALL InsertItem( HWND, TREEVIEW FAR *, ITEM FAR *, ITEM FAR *, LPTV_ITEM );
static void FASTCALL DeleteAllItems( HWND, TREEVIEW FAR * );
static ITEM FAR * FASTCALL GetHitTestInfo( HWND, LPTV_HITTESTINFO );
static void FASTCALL RecalcTreelistSize( HWND, TREEVIEW FAR *, LPRECT  );
static void FASTCALL BeginLabelEdit( HWND, TREEVIEW FAR *, ITEM FAR * );
static void FASTCALL EndLabelEdit( HWND, TREEVIEW FAR *, BOOL );
static int FASTCALL DeleteItem( HWND, TREEVIEW FAR *, ITEM FAR * );
static void FASTCALL ScrollList( HWND, int );
static ITEM FAR * FASTCALL GetNextVisibleItem( ITEM FAR * );
static ITEM FAR * FASTCALL GetPrevVisibleItem( ITEM FAR * );
static void FASTCALL SelectItem( HWND, TREEVIEW FAR *, ITEM FAR *, int, BOOL );
static BOOL FASTCALL InvalidateItem( HWND, TREEVIEW FAR *, ITEM FAR *, BOOL );
static BOOL FASTCALL EnsureVisible( HWND, TREEVIEW FAR *, ITEM FAR * );
static BOOL FASTCALL ExpandAllItems( HWND, TREEVIEW FAR * );
static BOOL FASTCALL ExpandItem( HWND, TREEVIEW FAR *, ITEM FAR * );
static BOOL FASTCALL CollapseAllItems( HWND, TREEVIEW FAR * );
static BOOL FASTCALL CollapseItem( HWND, TREEVIEW FAR *, ITEM FAR *, BOOL );
static BOOL FASTCALL ToggleItem( HWND, TREEVIEW FAR *, ITEM FAR * );
static void FASTCALL ClearVisibleItemRect( ITEM FAR * );
static void FASTCALL ClearItemRect( ITEM FAR * );
static void FASTCALL DoKeyboard( HWND, TREEVIEW FAR *, int );
static int FASTCALL GetImageIndex( HWND, ITEM FAR * );
static int FASTCALL GetSelectedImageIndex( HWND, ITEM FAR * );
static LPSTR FASTCALL GetItemText( HWND, ITEM FAR * );
static BOOL FASTCALL SortChildItemsCB( HWND, TREEVIEW FAR *, LPTV_SORTCB, BOOL );
static BOOL FASTCALL SortChildItems( HWND, TREEVIEW FAR *, ITEM FAR *, BOOL );
static BOOL FASTCALL FindString( HWND, TV_SEARCH FAR * );
static void FASTCALL IncrementalSearch( HWND, TREEVIEW FAR *, char );
static ITEM FAR * FASTCALL ISearch( HWND, TREEVIEW FAR *, ITEM FAR * );
static void FASTCALL ScrollRange( HWND , TREEVIEW FAR * );
static int FASTCALL NumberExpandedDescendents( ITEM FAR * );
static int FASTCALL ItemFindIndex( ITEM FAR *, ITEM FAR *, BOOL * );
static BOOL FASTCALL IsCompletelyVisible( HWND, ITEM FAR * );
static void FASTCALL ReadjustScrollPos( HWND hwnd, TREEVIEW FAR * pTreeview );

LRESULT EXPORT CALLBACK TreeviewEditProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK TreeviewWndFn( HWND, UINT, WPARAM, LPARAM );

/* This function registers the treeview window class.
 */
BOOL FASTCALL RegisterTreeviewClass( HINSTANCE hInst )
{
   WNDCLASS wc;

   wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
   wc.hIcon          = NULL;
   wc.lpszMenuName   = NULL;
   wc.lpszClassName  = WC_TREEVIEW;
   wc.hbrBackground  = (HBRUSH)( COLOR_WINDOW+1 );
   wc.hInstance      = hInst;
   wc.style          = CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS | CS_DBLCLKS;
   wc.lpfnWndProc    = TreeviewWndFn;
   wc.cbClsExtra     = 0;
   wc.cbWndExtra     = GWW_EXTRA;
   return( RegisterClass( &wc ) );
}

LRESULT EXPORT CALLBACK TreeviewWndFn( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   TREEVIEW FAR * pTreeview;
   ITEM FAR * pItem;

#ifdef _DEBUG
   /* Help us to catch NULL pointers...
    */
   pTreeview = NULL;
   pItem = NULL;
#endif
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_VSCROLL, Treeview_OnVScroll );

      case WM_NCCREATE: {
         LPCREATESTRUCT lpcs;

         /* Must not have a border.
          */
         lpcs = (LPCREATESTRUCT)lParam;
         lpcs->style &= ~WS_BORDER;
         SetWindowStyle( hwnd, lpcs->style );
         break;
         }
         
      case WM_CREATE: {
         static TREEVIEW FAR * pNewTreeview;

         INITIALISE_PTR(pNewTreeview);
         if( fNewMemory( &pNewTreeview, sizeof( TREEVIEW ) ) )
            {
            pNewTreeview->pRootItem = NULL;
            pNewTreeview->pFirstVisible = NULL;
            pNewTreeview->pLastVisible = NULL;
            pNewTreeview->pSelectedItem = NULL;
            pNewTreeview->pEditItem = NULL;
            pNewTreeview->pTooltipItem = NULL;
            pNewTreeview->lpfOldEditProc = NULL;
            pNewTreeview->himlNormal = NULL;
            pNewTreeview->himlState = NULL;
            pNewTreeview->hwndEdit = NULL;
            pNewTreeview->cxImage = 0;
            pNewTreeview->cyImage = 0;
            pNewTreeview->cRedraw = 0;
            pNewTreeview->cExpandedItems = 0;
            pNewTreeview->cVisibleItems = 0;
            pNewTreeview->nScrollPos = 0;
            pNewTreeview->fMousedown = FALSE;
            pNewTreeview->fEndingLabelEdit = FALSE;
            pNewTreeview->cItems = 0;
            pNewTreeview->cIndent = 19;
            pNewTreeview->dyFont = 0;
            pNewTreeview->dyEditTopBorder = fNewShell ? 2 : 4;
            pNewTreeview->crText = GetSysColor( COLOR_WINDOWTEXT );
            pNewTreeview->crBack = GetSysColor( COLOR_WINDOW );
            pNewTreeview->hFont = GetStockFont( ANSI_VAR_FONT );
            pNewTreeview->cScan = 0;
            pNewTreeview->fDragging = FALSE;

            /* Hide the scroll bar.
             */
            if( GetWindowStyle( hwnd ) & WS_VSCROLL )
               SetScrollRange( hwnd, SB_VERT, 0, 0, FALSE );
            PutBlock( hwnd, pNewTreeview );
            }
         return( pNewTreeview ? 0 : -1 );
         }

      case WM_ERASEBKGND: {
         HBRUSH hbr;
         RECT rc;

         pTreeview = GetBlock( hwnd );
         GetClientRect( hwnd, &rc );
         hbr = CreateSolidBrush( pTreeview->crBack );
         FillRect( (HDC)wParam, &rc, hbr );
         DeleteBrush( hbr );
         return( TRUE );
         }

      case WM_COMMAND:
         if( HIWORD( lParam ) == EN_CHANGE )
            {
            LPSTR pszText;
            int cchEdit;

            /* This code is called when the used edits the
             * text label. We attempt to resize the edit field so
             * that the entire label is visible.
             */
            INITIALISE_PTR(pszText);
            pTreeview = GetBlock( hwnd );
            if( NULL != pTreeview->hwndEdit )
               {
               cchEdit = Edit_GetTextLength( pTreeview->hwndEdit );
               if( fNewMemory( &pszText, cchEdit + 1 ) )
                  {
                  HFONT hOldFont;
                  int dxEdit;
                  SIZE size;
                  RECT rcClient;
                  int diff;
                  HDC hdc;
                  RECT rc; 

                  /* Get the current text (post-change) and compute
                   * the width, in pixels.
                   */
                  Edit_GetRect( pTreeview->hwndEdit, &rc );
                  dxEdit = rc.right - rc.left;
                  Edit_GetText( pTreeview->hwndEdit, pszText, cchEdit + 1 );
                  hdc = GetDC( pTreeview->hwndEdit );
                  hOldFont = SelectFont( hdc, pTreeview->hFont );
                  GetTextExtentPoint( hdc, pszText, cchEdit, &size );

                  /* The new width is longer than the edit control
                   * formatting rectangle, so widen the formatting
                   * rectangle.
                   */
                  if( size.cx > pTreeview->dxEdit - EDITZONEWIDTH )
                     {
                     diff = size.cx - dxEdit;
                     rc.right += diff;
                     GetWindowRect( pTreeview->hwndEdit, &rcClient );
                     rcClient.bottom -= rcClient.top;
                     rcClient.right -= rcClient.left;
                     SetWindowPos( pTreeview->hwndEdit, NULL, 0, 0, rcClient.right + diff, rcClient.bottom,
                                   SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE );
                     Edit_SetRect( pTreeview->hwndEdit, &rc );
                     }

                  /* Clean up before we exit.
                   */
                  SelectFont( hdc, hOldFont );
                  ReleaseDC( pTreeview->hwndEdit, hdc );
                  FreeMemory( &pszText );
                  }
               }
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
            if( WHEEL_PAGESCROLL == iScrollLines )
               {
               if( nLines > 0 )
                  SendMessage( hwnd, WM_VSCROLL, SB_PAGEUP, 0L );
               else
                  SendMessage( hwnd, WM_VSCROLL, SB_PAGEDOWN, 0L );
               }
            else
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
               }
            zDeltaAccumulate = 0;
            }
         break;
         }
   #endif

      case WM_SYSCOLORCHANGE:
         pTreeview = GetBlock( hwnd );
         pTreeview->crText = GetSysColor( COLOR_WINDOWTEXT );
         pTreeview->crBack = GetSysColor( COLOR_WINDOW );
         break;

      case TVM_CREATEDRAGIMAGE: {
         ITEM FAR * pItem;
         HIMAGELIST himl;
         int cx, cy;
         int index;

         /* Get the item for which we're creating the
          * drag image.
          */
         pTreeview = GetBlock( hwnd );
         pItem = (ITEM FAR *)lParam;
         index = I_NOIMAGE;
         if( pTreeview->cxImage )
            index = GetImageIndex( hwnd, pItem );
         cx = ( pItem->id.rcItem.right - pItem->id.rcItem.left ) + 6;
         if( I_NOIMAGE != index )
            cx += pTreeview->cxImage;
         cy = pItem->id.rcItem.bottom - pItem->id.rcItem.top;
         himl = Amctl_ImageList_Create( cx, cy, ILC_MASK|ILC_COLOR, 1, 1 );

         /* Create a bitmap into which to draw the label and the text,
          * then add it to the image.
          */
         if( NULL != himl )
            {
            HBITMAP hbmpOld;
            HBITMAP hbmp;
            HBRUSH hbr;
            RECT rc;
            HDC hdcSrc;
            HDC hdcDest;
            int cxImage;

            /* Prime the drawing area.
             */
            hdcSrc = GetDC( hwnd );
            hdcDest = CreateCompatibleDC( hdcSrc );
            hbmp = CreateCompatibleBitmap( hdcSrc, cx, cy );
            hbmpOld = SelectBitmap( hdcDest, hbmp );

            /* Fill the drawing area with the background colour.
             */
            SetRect( &rc, 0, 0, cx, cy );
            hbr = CreateSolidBrush( pTreeview->crBack );
            FillRect( hdcDest, &rc, hbr );
            DeleteBrush( hbr );

            /* Draw the image.
             */
            cxImage = 0;
            if( pTreeview->himlNormal )
               {
               int x1, y1;

               x1 = 0;
               y1 = ( ( pTreeview->dyLine - pTreeview->cyImage ) / 2 );
               if( I_NOIMAGE != index )
                  {
                  Amctl_ImageList_Draw( pTreeview->himlNormal, index, hdcDest, x1, y1, ILD_TRANSPARENT );
                  cxImage = pTreeview->cxImage;
                  }
               }

            /* Draw the label.
             */
            if( GetWindowStyle( hwnd ) & TVS_OWNERDRAW )
               {
               DRAWITEMSTRUCT dis;
               TV_DRAWITEM tvid;

               dis.CtlType = 0;
               dis.CtlID = GetDlgCtrlID( hwnd );
               dis.hwndItem = hwnd;
               dis.hDC = hdcDest;
               dis.rcItem.top = 0;
               dis.rcItem.bottom = cy;
               dis.rcItem.left = cxImage;
               dis.rcItem.right = cx;
               dis.itemState = 0x8000;
               dis.itemAction = ODA_DRAWENTIRE;
               dis.itemData = (DWORD)(LPSTR)&tvid;
               tvid.pszText = GetItemText( hwnd, pItem );
               tvid.tv.state = pItem->id.state;
               tvid.tv.lParam = pItem->id.lParam;
               SendMessage( GetParent( hwnd ), WM_DRAWITEM, dis.CtlID, (LPARAM)(LPSTR)&dis );
               }
            else
               {
               char * pszText;
               HFONT hOldFont;
               int x, y;

               /* Compute the text rectangle.
                */
               hOldFont = SelectFont( hdcDest, pTreeview->hFont );
               rc.left = cxImage;
               rc.top = 0;
               rc.right = cx;
               rc.bottom = cy;
               pszText = GetItemText( hwnd, pItem );
               x = rc.left + 2;
               y = rc.top + ( ( rc.bottom - rc.top ) - pTreeview->dyFont ) / 2;
               ExtTextOut( hdcDest, x, y, ETO_OPAQUE, &rc, pszText, lstrlen( pszText ), NULL );
               SelectFont( hdcDest, hOldFont );
               }

            /* Clean up afterwards.
             */
            SelectBitmap( hdcDest, hbmpOld );
            DeleteDC( hdcDest );
            ReleaseDC( hwnd, hdcSrc );
            Amctl_ImageList_AddMasked( himl, hbmp, pTreeview->crBack );
            DeleteBitmap( hbmp );
            }
         return( (LRESULT)himl );
         }

      case TVM_DELETEITEM: {
         ITEM FAR * pParentItem;

         /* Cancel any label editing.
          */
         pTreeview = GetBlock( hwnd );
         pParentItem = NULL;
         EndLabelEdit( hwnd, pTreeview, TRUE );

         if( TVI_ROOT == (HTREEITEM)lParam )
            DeleteAllItems( hwnd, pTreeview );
         else
            {
            int cDeleted;

            pItem = (ITEM FAR *)lParam;
            pParentItem = pItem->pParentItem;
            cDeleted = DeleteItem( hwnd, pTreeview, pItem );
            ASSERT( pTreeview->cItems - cDeleted >= 0 );
            pTreeview->cItems -= cDeleted;

            /* If this item has a parent which is expanded then reduce the number of
             * expanded items by one.
             */
            if( NULL != pParentItem )
               if( pParentItem->id.state & TVIS_EXPANDED )
                  --pTreeview->cExpandedItems;
            }

         /* If redrawing is permitted, update the treelist
          * view rectangles.
          */
         if( 0 == pTreeview->cRedraw )
            {
            /* If this item has a parent and this item was the only
             * child item of that parent, then redraw the parent to
             * reflect that the parent item now has no children.
             */
            if( pParentItem && pParentItem->id.cChildren == 0 )
               InvalidateItem( hwnd, pTreeview, pParentItem, TRUE );
            RecalcTreelistSize( hwnd, pTreeview, NULL );
            ReadjustScrollPos( hwnd, pTreeview );
            UpdateWindow( hwnd );
            }
         return( TRUE );
         }

      case TVM_EDITLABEL:
         pTreeview = GetBlock( hwnd );
         BeginLabelEdit( hwnd, pTreeview, (ITEM FAR *)lParam );
         return( (LRESULT)(LPSTR)pTreeview->hwndEdit );

      case TVM_ENDEDITLABELNOW:
         pTreeview = GetBlock( hwnd );
         EndLabelEdit( hwnd, pTreeview, wParam );
         return( TRUE );

      case TVM_ENSUREVISIBLE:
         pTreeview = GetBlock( hwnd );
         if( EnsureVisible( hwnd, pTreeview, (ITEM FAR *)lParam ) )
            {
            UpdateWindow( hwnd );
            return( TRUE );
            }
         return( FALSE );

      case TVM_EXPAND:
         pTreeview = GetBlock( hwnd );
         switch( wParam )
            {
            case TVE_COLLAPSERESET: return( CollapseItem( hwnd, pTreeview, (ITEM FAR *)lParam, TRUE ) );
            case TVE_COLLAPSE:      return( CollapseItem( hwnd, pTreeview, (ITEM FAR *)lParam, FALSE ) );
            case TVE_EXPAND:        return( ExpandItem( hwnd, pTreeview, (ITEM FAR *)lParam ) );
            case TVE_TOGGLE:        return( ToggleItem( hwnd, pTreeview, (ITEM FAR *)lParam ) );
            }
         return( FALSE );

      case TVM_FINDSTRING:
         pTreeview = GetBlock( hwnd );
         return( (LRESULT)FindString( hwnd, (TV_SEARCH FAR *)lParam ) );

      case TVM_GETCOUNT:
         pTreeview = GetBlock( hwnd );
         return( pTreeview->cItems );

      case TVM_GETEDITCONTROL:
         pTreeview = GetBlock( hwnd );
         return( (LRESULT)(LPSTR)pTreeview->hwndEdit );

      case TVM_GETIMAGELIST:
         pTreeview = GetBlock( hwnd );
         if( wParam == TVSIL_NORMAL )
            return( (LRESULT)(LPSTR)pTreeview->himlNormal );
         else
            return( (LRESULT)(LPSTR)pTreeview->himlState );

      case TVM_GETINDENT:
         pTreeview = GetBlock( hwnd );
         return( pTreeview->cIndent );

      case TVM_GETITEM: {
         TV_ITEM FAR * lptvi;

         pTreeview = GetBlock( hwnd );
         lptvi = (TV_ITEM FAR *)lParam;
         pItem = (ITEM FAR *)lptvi->hItem;
         if( lptvi->mask & TVIF_TEXT )
            {
            if( LPSTR_TEXTCALLBACK == pItem->id.pszText )
               lptvi->pszText = LPSTR_TEXTCALLBACK;
            else
               {
               UINT len;
   
               len = min( lptvi->cchTextMax, lstrlen( pItem->id.pszText ) );
               lstrcpyn( lptvi->pszText, pItem->id.pszText, len+1 );
               lptvi->pszText[ len ] = '\0';
               }
            }
         if( lptvi->mask & TVIF_STATE )
            lptvi->state = pItem->id.state & lptvi->stateMask;
         if( lptvi->mask & TVIF_IMAGE )
            lptvi->iImage = pItem->id.iImage;
         if( lptvi->mask & TVIF_SELECTEDIMAGE )
            lptvi->iSelectedImage = pItem->id.iSelectedImage;
         if( lptvi->mask & TVIF_PARAM )
            lptvi->lParam = pItem->id.lParam;
         if( lptvi->mask & TVIF_CHILDREN )
            lptvi->cChildren = pItem->id.cChildren;
         if( lptvi->mask & TVIF_RECT )
            {
            lptvi->rcItem = pItem->id.rcItem;
            if( GetWindowStyle( hwnd ) & TVS_OWNERDRAW )
               lptvi->rcItem.right = 2000;
            }
         return( TRUE );
         }

      case TVM_GETNEXTITEM:
         pTreeview = GetBlock( hwnd );
         pItem = (ITEM FAR *)lParam;
         switch( wParam )
            {
            case TVGN_PARENT:             return( (LRESULT)(LPSTR)pItem->pParentItem );
            case TVGN_CHILD:              return( (LRESULT)(LPSTR)pItem->pFirstChild );
            case TVGN_NEXT:               return( (LRESULT)(LPSTR)pItem->pNextItem );
            case TVGN_PREVIOUS:           return( (LRESULT)(LPSTR)pItem->pPrevItem );
            case TVGN_ROOT:               return( (LRESULT)(LPSTR)pTreeview->pRootItem );
            case TVGN_CARET:              return( (LRESULT)(LPSTR)pTreeview->pSelectedItem );
            case TVGN_FIRSTVISIBLE:       return( (LRESULT)(LPSTR)pTreeview->pFirstVisible );
            case TVGN_NEXTVISIBLE:        return( (LRESULT)(LPSTR)GetNextVisibleItem( pItem ) );
            case TVGN_PREVIOUSVISIBLE:    return( (LRESULT)(LPSTR)GetPrevVisibleItem( pItem ) );
            }
         return( 0 );

      case TVM_GETVISIBLECOUNT:
         pTreeview = GetBlock( hwnd );
         return( pTreeview->cVisibleItems );

      case TVM_HITTEST: {
         LPTV_HITTESTINFO lpht;

         lpht = (LPTV_HITTESTINFO)lParam;
         return( (LRESULT)(LPSTR)GetHitTestInfo( hwnd, lpht ) );
         }

      case TVM_INSERTITEM: {
         LPTV_INSERTSTRUCT lpis;
         ITEM FAR * pParentItem;
         ITEM FAR * pInsertAfter;

         pItem = NULL;
         /* Dereference what we're going to use.
          */
         pTreeview = GetBlock( hwnd );
         lpis = (LPTV_INSERTSTRUCT)lParam;
         ASSERT( lpis != NULL );

         /* Get the parent node. If lpis->hParent is NULL or
          * TVI_ROOT, we're adding messages at the root level.
          */
         if( TVI_ROOT == lpis->hParent )
            pParentItem = NULL;
         else
            pParentItem = (ITEM FAR *)lpis->hParent;

         /* pInsertAfter points to the item after which the new
          * item will be inserted. If pInsertAfter is NULL, then
          * the item is inserted as the first item of the parent.
          */
         if( NULL == pParentItem )
            pInsertAfter = pTreeview->pRootItem;
         else
            pInsertAfter = pParentItem->pFirstChild;

         /* Cancel any label editing.
          */
         EndLabelEdit( hwnd, pTreeview, TRUE );

         /* The lpis->hInsertAfter parameter specifies where the new item
          * is inserted. It can be TVI_FIRST or TVI_LAST for the start or
          * end of the list, or it can be the handle of an item after which
          * the new item is inserted.
          */
         ASSERT( NULL == pItem );
         if( TVI_FIRST == lpis->hInsertAfter )
            pItem = InsertItem( hwnd, pTreeview, pParentItem, NULL, &lpis->item );
         else if( TVI_LAST == lpis->hInsertAfter )
            {
            if( pInsertAfter )
               {
               while( pInsertAfter->pNextItem )
                  pInsertAfter = pInsertAfter->pNextItem;
               }
            pItem = InsertItem( hwnd, pTreeview, pParentItem, pInsertAfter, &lpis->item );
            }
         else if( TVI_SORT == lpis->hInsertAfter )
            {
            ITEM FAR * pPrevItem;

            pPrevItem = NULL;
            while( pInsertAfter )
               {
               if( lstrcmpi( pInsertAfter->id.pszText, lpis->item.pszText ) > 0 )
                  break;
               pPrevItem = pInsertAfter;
               pInsertAfter = pInsertAfter->pNextItem;
               }
            pItem = InsertItem( hwnd, pTreeview, pParentItem, pPrevItem, &lpis->item );
            }
         else
            {
            while( pInsertAfter )
               {
               if( pInsertAfter == (ITEM FAR *)lpis->hInsertAfter )
                  {
                  pItem = InsertItem( hwnd, pTreeview, pParentItem, pInsertAfter, &lpis->item );
                  break;
                  }
               pInsertAfter = pInsertAfter->pNextItem;
               }
            }

         /* If we've got a new item and redrawing is
          * permitted, update the treelist view rectangles.
          */
         if( 0 == pTreeview->cRedraw )
            {
            /* If this item has a parent and this item is the only
             * child item of that parent, then redraw the parent to
             * reflect that the parent item can be expanded.
             */
            if( pItem->pParentItem && pItem->pParentItem->id.cChildren == 1 )
               InvalidateItem( hwnd, pTreeview, pItem->pParentItem, TRUE );
            RecalcTreelistSize( hwnd, pTreeview, NULL );
            ReadjustScrollPos( hwnd, pTreeview );
            UpdateWindow( hwnd );
            }
         return( (LRESULT)(LPSTR)pItem );
         }

      case TVM_SELECTITEM:
         pTreeview = GetBlock( hwnd );
         switch( wParam )
            {
            case TVGN_DROPHILITE:
               pItem = (ITEM FAR *)lParam;
               if( pItem != pTreeview->pSelectedItem )
                  SelectItem( hwnd, pTreeview, pItem, TVC_UNKNOWN, TRUE );
               return( TRUE );

            case TVGN_FIRSTVISIBLE:
               if( pTreeview->pFirstVisible != (ITEM FAR *)lParam )
                  {
                  pTreeview->pFirstVisible = (ITEM FAR *)lParam;
                  RecalcTreelistSize( hwnd, pTreeview, NULL );
                  ReadjustScrollPos( hwnd, pTreeview );
                  UpdateWindow( hwnd );
                  }
               return( TRUE );

            case TVGN_CARET:
               SelectItem( hwnd, pTreeview, (ITEM FAR *)lParam, TVC_UNKNOWN, TRUE );
               return( TRUE );
            }
         return( FALSE );

      case TVM_SETIMAGELIST: {
         HIMAGELIST himlPrev;
         IMAGEINFO ImageInfo;
         int cxImage, cyImage;

         pTreeview = GetBlock( hwnd );
         if( wParam == TVSIL_NORMAL )
            {
            himlPrev = pTreeview->himlNormal;
            pTreeview->himlNormal = (HIMAGELIST)lParam;
            Amctl_ImageList_GetImageInfo( pTreeview->himlNormal, 0, &ImageInfo );
            }
         else
            {
            himlPrev = pTreeview->himlState;
            pTreeview->himlState = (HIMAGELIST)lParam;
            Amctl_ImageList_GetImageInfo( pTreeview->himlState, 0, &ImageInfo );
            }

         /* Compute the new image dimensions. If these differ from the
          * current image dimensions, save the new dimensions and recompute
          * the visible item rectangles, if any.
          */
         cyImage = ImageInfo.rcImage.bottom - ImageInfo.rcImage.top;
         cxImage = ( ImageInfo.rcImage.right - ImageInfo.rcImage.left ) + 3;
         if( cyImage != pTreeview->cyImage || cxImage != pTreeview->cxImage )
            {
            pTreeview->cyImage = cyImage;
            pTreeview->cxImage = cxImage;
            if( pTreeview->cItems )
            {
               RecalcTreelistSize( hwnd, pTreeview, NULL );
               ReadjustScrollPos( hwnd, pTreeview );
            }
            }
         return( (LRESULT)(LPSTR)himlPrev );
         }

      case TVM_SETINDENT:
         pTreeview = GetBlock( hwnd );
         pTreeview->cIndent = max( wParam, MIN_INDENTATION );
         RecalcTreelistSize( hwnd, pTreeview, NULL );
         ReadjustScrollPos( hwnd, pTreeview );
         UpdateWindow( hwnd );
         return( 0L );

      case TVM_SETITEM: {
         TV_ITEM FAR * lptvi;

         pTreeview = GetBlock( hwnd );
         lptvi = (TV_ITEM FAR *)lParam;
         pItem = (ITEM FAR *)lptvi->hItem;
         if( lptvi->mask & TVIF_TEXT )
            {
            if( pItem->id.pszText != LPSTR_TEXTCALLBACK )
               {
               static LPSTR lpszText;
               NMHDR nmhdr;

               INITIALISE_PTR(lpszText);
               if( pItem->id.pszText )
                  FreeMemory( &pItem->id.pszText );
               if( fNewMemory( &lpszText, lstrlen( lptvi->pszText ) + 1 ) )
                  {
                  pItem->id.pszText = lpszText;
                  lstrcpy( pItem->id.pszText, lptvi->pszText );
                  }
               else
                  Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_OUTOFMEMORY, &nmhdr );
               }
            else
               pItem->id.pszText = lptvi->pszText;
            }
         if( lptvi->mask & TVIF_STATE )
            pItem->id.state = lptvi->state & lptvi->stateMask;
         if( lptvi->mask & TVIF_IMAGE )
            pItem->id.iImage = lptvi->iImage;
         if( lptvi->mask & TVIF_SELECTEDIMAGE )
            pItem->id.iSelectedImage = lptvi->iSelectedImage;
         if( lptvi->mask & TVIF_PARAM )
            pItem->id.lParam = lptvi->lParam;
         if( lptvi->mask & TVIF_CHILDREN )
            pItem->id.cChildren = lptvi->cChildren;
         RecalcTreelistSize( hwnd, pTreeview, NULL );
         ReadjustScrollPos( hwnd, pTreeview );
         return( 0 );
         }

      case TVM_SETTEXTCOLOR: {
         COLORREF crText;

         pTreeview = GetBlock( hwnd );
         crText = pTreeview->crText;
         pTreeview->crText = (COLORREF)lParam;
         return( crText );
         }

      case TVM_SETBACKCOLOR: {
         COLORREF crBack;

         pTreeview = GetBlock( hwnd );
         crBack = pTreeview->crBack;
         pTreeview->crBack = (COLORREF)lParam;
         return( crBack );
         }

      case TVM_SORTCHILDREN:
         pTreeview = GetBlock( hwnd );
         return( SortChildItems( hwnd, pTreeview, (ITEM FAR *)lParam, wParam ) );

      case TVM_SORTCHILDRENCB:
         pTreeview = GetBlock( hwnd );
         return( SortChildItemsCB( hwnd, pTreeview, (LPTV_SORTCB)lParam, wParam ) );

      case WM_NOTIFY: {
         NMHDR FAR * lpnmHdr;

         lpnmHdr = (NMHDR FAR *)lParam;
         if( lpnmHdr->code == TTN_NEEDTEXT )
            {
            LPTOOLTIPTEXT lpttt;

            lpttt = (LPTOOLTIPTEXT)lParam;
            pTreeview = GetBlock( hwnd );
            if( NULL != pTreeview->pTooltipItem )
               {
               strcpy( lpttt->szText, GetItemText( hwnd, pTreeview->pTooltipItem ) );
               lpttt->lpszText = lpttt->szText;
               lpttt->hinst = NULL;
               return( TRUE );
               }
            }
         return( FALSE );
         }

      case WM_GETDLGCODE:
         pTreeview = GetBlock( hwnd );
         if( NULL != pTreeview->hwndEdit )
            return( DLGC_WANTARROWS|DLGC_WANTCHARS|DLGC_WANTALLKEYS );
         return( DLGC_WANTARROWS|DLGC_WANTCHARS );

      case WM_GETFONT:
         pTreeview = GetBlock( hwnd );
         return( (LRESULT)(LPSTR)pTreeview->hFont );

      case WM_NCPAINT: {
         RECT rc;
         HDC hdc;

         /* Since we're doing NC painting, client things do not
          * apply to us.
          */
         hdc = GetDC( GetParent( hwnd ) );
         GetWindowRect( hwnd, &rc );
         MapWindowPoints( NULL, GetParent( hwnd ), (LPPOINT)&rc, 2 );
         rc.right += 2;
         rc.bottom += 2;
         rc.left -= 2;
         rc.top -= 2;
         Draw3DFrame( hdc, &rc );
         ReleaseDC( GetParent( hwnd ), hdc );
         break;
         }

      case WM_SETREDRAW:
         pTreeview = GetBlock( hwnd );
         if( !wParam )
            ++pTreeview->cRedraw;
         else if( pTreeview->cRedraw )
            --pTreeview->cRedraw;
         if( 0 == pTreeview->cRedraw )
         {
            RecalcTreelistSize( hwnd, pTreeview, NULL );
            ReadjustScrollPos( hwnd, pTreeview );
         }
         return( 0L );

      case WM_SIZE:
         pTreeview = GetBlock( hwnd );
         if( pTreeview == NULL )
         {
            DWORD dwPlop = GetLastError();
         }
//       ReadjustScrollPos( hwnd, pTreeview );
         RecalcTreelistSize( hwnd, pTreeview, NULL );
         UpdateWindow( hwnd );
         break;

      case WM_SETFONT:
         /* Change the treeview control font.
          */
         pTreeview = GetBlock( hwnd );//if( pTreeview == NULL )
         pTreeview->hFont = wParam ? (HFONT)wParam : GetStockFont( SYSTEM_FONT );
         if( LOWORD( lParam ) )
            {
            RecalcTreelistSize( hwnd, pTreeview, NULL );
            ReadjustScrollPos( hwnd, pTreeview );
            UpdateWindow( hwnd );
            }
         return( 0L );

      case WM_CONTEXTMENU: /*!!SM!!*/
      case WM_RBUTTONDOWN: {
         NMHDR nmhdr;

         /* Kill the timer set if we right-clicked on
          * a selected item.
          */
         KillTimer( hwnd, IDT_EDITTIMER );
         Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_RCLICK, &nmhdr );
         return( 0L );
         }

      case WM_LBUTTONDBLCLK: {
         NMHDR nmhdr;

         /* Kill the timer set if we double-clicked on
          * a selected item.
          */
         KillTimer( hwnd, IDT_EDITTIMER );

         /* If this item doesn't have the
          * focus, give it the focus.
          */
         if( hwnd != GetFocus() )
            SetFocus( hwnd );

         /* Determine where we are on the control. If
          * pItem refers to an item, then check that we're
          * on the label itself.
          */
         if( Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_DBLCLK, &nmhdr ) == TRUE )
            {
            TV_HITTESTINFO ht;

            ht.pt.x = LOWORD( lParam );
            ht.pt.y = HIWORD( lParam );
            if( pItem = GetHitTestInfo( hwnd, &ht ) )
               {
               pTreeview = GetBlock( hwnd );
               if( ht.flags & TVHT_ONITEMLABEL )
                  ToggleItem( hwnd, pTreeview, pItem );
               }
            }
         return( 0L );
         }

      case WM_LBUTTONDOWN: {
         TV_HITTESTINFO ht;

         /* If this item doesn't have the
          * focus, give it the focus.
          */
         if( hwnd != GetFocus() )
            SetFocus( hwnd );

         /* Determine where we are on the control. If
          * pItem refers to an item, then test whether
          * we're on the item or the button.
          */
         ht.pt.x = LOWORD( lParam );
         ht.pt.y = HIWORD( lParam );
         pTreeview = GetBlock( hwnd );
         pTreeview->fMousedown = FALSE;
         if( pItem = GetHitTestInfo( hwnd, &ht ) )
            {
            if( ht.flags & (TVHT_ONITEMLABEL|TVHT_ONITEMICON) )
               {
               if( pItem == pTreeview->pSelectedItem && (ht.flags & TVHT_ONITEMLABEL) )
                  {
                  if( GetWindowStyle( hwnd ) & TVS_EDITLABELS )
                     SetTimer( hwnd, IDT_EDITTIMER, GetDoubleClickTime() + 20, NULL );
                  }
               else
                  {
                  SelectItem( hwnd, pTreeview, pItem, TVC_BYMOUSE, FALSE );
                  pTreeview->cScan = 0;
                  }

               /* Unless this control has dragon drop disabled, set the fMousedown
                * flag to indicate that we may be starting a drag operation.
                */
               if( !( GetWindowStyle( hwnd ) & TVS_DISABLEDRAGDROP ) )
                  {
                  pTreeview->fMousedown = TRUE;
                  pTreeview->pt = ht.pt;
                  }
               }
            else if( ht.flags & TVHT_ONITEMBUTTON )
               ToggleItem( hwnd, pTreeview, pItem );
            }
         return( 0L );
         }

      case WM_MOUSEMOVE:
         pTreeview = GetBlock( hwnd );
         if( pTreeview->fMousedown )
            {
            POINT pt;

            /* Kill the edit timer, whatever we do.
             */
            KillTimer( hwnd, IDT_EDITTIMER );

            /* Wait for the mouse to move out of the double-click
             * zone before we decide whether or not we're dragging.
             */
            pt.x = LOWORD( lParam );
            pt.y = HIWORD( lParam );
            if( abs( pt.x - pTreeview->pt.x ) > GetSystemMetrics( SM_CXDOUBLECLK ) ||
                abs( pt.y - pTreeview->pt.y ) > GetSystemMetrics( SM_CYDOUBLECLK ) )
               {
               NM_TREEVIEW tv;

               /* Tell the parent that a drag is about to begin.
                */
               tv.ptDrag = pt;
               tv.itemNew.state = pTreeview->pSelectedItem->id.state;
               tv.itemNew.lParam = pTreeview->pSelectedItem->id.lParam;
               tv.itemNew.hItem = (HTREEITEM)pTreeview->pSelectedItem;
               Amctl_SendNotify( GetParent( hwnd ), hwnd, TVN_BEGINDRAG, (NMHDR FAR *)&tv );
               pTreeview->fDragging = TRUE;  // !!SM!!
               pTreeview->fMousedown = FALSE;
               }
            break;
            }

         /* Check whether we're over a tooltip item.
          */
         if( NULL != pTreeview->pTooltipItem )
            {
            POINT pt;

            pt.x = LOWORD( lParam );
            pt.y = HIWORD( lParam );
            if( !PtInRect( &pTreeview->pTooltipItem->id.rcItem, pt ) )
               {
               /* Cursor moved out of tooltip item, so destroy the
                * tooltip window and NULL the flags.
                */
               DestroyWindow( pTreeview->hwndTooltip );
               pTreeview->hwndTooltip = NULL;
               pTreeview->pTooltipItem = NULL;
               ReleaseCapture();
               }
            else
               {
               MSG msg;

               msg.message = message;
               msg.hwnd = hwnd;
               msg.wParam = wParam;
               msg.lParam = lParam;
               SendMessage( pTreeview->hwndTooltip, TTM_RELAYEVENT, 0, (LPARAM)(LPSTR)&msg );
               }
            }

         /* Check whether we're over a label whose size is too wide
          * for the treeview control.
          */
         if( GetWindowStyle( hwnd ) & TVS_TOOLTIPS )
            if( NULL == pTreeview->pTooltipItem )
               {
               TV_HITTESTINFO tvhi;

               tvhi.pt.x = LOWORD( lParam );
               tvhi.pt.y = HIWORD( lParam );
               if( NULL != ( pItem = GetHitTestInfo( hwnd, &tvhi ) ) )
                  if( tvhi.flags & TVHT_ONITEMLABEL )
                     {
                     RECT rc;

                     GetClientRect( hwnd, &rc );
                     if( pItem->id.rcItem.right > rc.right )
                        {
                        TOOLINFO ti;

                        /* If we get this far, we create a tooltip for the
                         * selected item.
                         */
                        pTreeview->pTooltipItem = pItem;
                        pTreeview->hwndTooltip = CreateWindow( TOOLTIPS_CLASS, "", 0, 0, 0, 0, 0, hwnd, 0, hLibInst, 0L );

                        /* Add one tooltip, for this item.
                         */
                        ti.cbSize = sizeof( TOOLINFO );
                        ti.uFlags = TTF_COVERHWND;
                        ti.hwnd = hwnd;
                        ti.hinst = hLibInst;
                        ti.uId = 0;
                        ti.lpszText = LPSTR_TEXTCALLBACK;
                        CopyRect( &ti.rect, &pItem->id.rcItem );
                        SendMessage( pTreeview->hwndTooltip, TTM_ADDTOOL, 0, (LPARAM)(LPTOOLINFO)&ti );

                        /* Adjust the autopop timers.
                         */
                        SendMessage( pTreeview->hwndTooltip, TTM_SETDELAYTIME, TTDT_INITIAL, 0 );
                        SendMessage( pTreeview->hwndTooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 0 );

                        /* We want all mouse activity as long as the tooltip
                         * is visible.
                         */
                        SetCapture( hwnd );
                        }
                     }
               }
         return( 0L );

      case WM_LBUTTONUP:
         pTreeview = GetBlock( hwnd );
         pTreeview->fMousedown = FALSE;
         pTreeview->fDragging = FALSE;

         return( 0L );

      case WM_TIMER:
         /* The use of the timer fixes the problem where the user double-clicks
          * a selected item, intending to collapse the item rather than edit it.
          * If the timer expires before the WM_LBUTTONDBLCLK, then the user really
          * did intend to edit the label.
          */
         pTreeview = GetBlock( hwnd );
         KillTimer( hwnd, IDT_EDITTIMER );
         ASSERT( NULL != pTreeview->pSelectedItem );
         BeginLabelEdit( hwnd, pTreeview, pTreeview->pSelectedItem );
         return( 0L );

      case WM_CHAR:
         pTreeview = GetBlock( hwnd );
         switch( wParam )
            {
            case '+':
               if( GetKeyState( VK_CONTROL ) & 0x8000 )
                  ExpandAllItems( hwnd, pTreeview );
               else
                  ExpandItem( hwnd, pTreeview, pTreeview->pSelectedItem );
               return( 0L );

            case '-':
               if( GetKeyState( VK_CONTROL ) & 0x8000 )
                  CollapseAllItems( hwnd, pTreeview );
               else
                  CollapseItem( hwnd, pTreeview, pTreeview->pSelectedItem, FALSE );
               return( 0L );

            default:
               IncrementalSearch( hwnd, pTreeview, (char)wParam );
               return( 0L );
            }
         return( DefWindowProc( hwnd, message, wParam, lParam ) );

      case WM_KEYDOWN:
         pTreeview = GetBlock( hwnd );
         DoKeyboard( hwnd, pTreeview, wParam );
         return( 0L );

      case WM_KILLFOCUS: {
         NMHDR nmhdr;

         /* We're losing the focus, so if there's a selected item, remove
          * the focus round this item.
          */
         pTreeview = GetBlock( hwnd );
         pTreeview->cScan = 0;
         if( pTreeview->pSelectedItem )
            {
            pTreeview->pSelectedItem->id.state &= ~TVIS_FOCUSED;
            InvalidateItem( hwnd, pTreeview, pTreeview->pSelectedItem, FALSE );
            }
         Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_KILLFOCUS, &nmhdr );
         return( 0L );
         }

      case WM_SETFOCUS: {
         NMHDR nmhdr;

         /* We're receiving the focus, so if there's a selected item, set
          * the focus round this item.
          */
         pTreeview = GetBlock( hwnd );
         if( pTreeview->pSelectedItem )
            {
            pTreeview->pSelectedItem->id.state |= TVIS_FOCUSED;
            InvalidateItem( hwnd, pTreeview, pTreeview->pSelectedItem, FALSE );
            }
         Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_SETFOCUS, &nmhdr );
         return( 0L );
         }

      case WM_PAINT: {
         PAINTSTRUCT ps;
         register int y;
         BOOL fButtons;
         BOOL fLinesAtRoot;
         BOOL fOwnerdraw;
         BOOL fLines;
         DWORD dwStyle;
         HFONT hOldFont;
         HBRUSH hbrBtn;
         HPEN hpenBtn;
         HPEN hpenOld;
         HPEN hpenText;
         HBRUSH hbrOld;
         HDC hdc;
         RECT rcWnd;
         DRAWITEMSTRUCT dis;

         /* Begin painting,
          */
         hdc = BeginPaint( hwnd, &ps );
         pTreeview = GetBlock( hwnd );
         hOldFont = SelectFont( hdc, pTreeview->hFont );
         SetTextColor( hdc, pTreeview->crText );
         SetBkColor( hdc, pTreeview->crBack );
         GetClientRect( hwnd, &rcWnd );

         /* Do we draw buttons ?
          */
         dwStyle = GetWindowStyle( hwnd );
         fButtons = ( dwStyle & TVS_HASBUTTONS ) != 0;
         hpenOld = NULL;
         hbrOld = NULL;
         hpenText = NULL;
         hpenBtn = NULL;
         hbrBtn = NULL;
         if( fButtons )
            {
            hbrBtn = CreateSolidBrush( pTreeview->crBack );
            hpenBtn = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNSHADOW ) );
            hpenText = CreatePen( PS_SOLID, 1, pTreeview->crText );
            hbrOld = SelectBrush( hdc, hbrBtn );
            hpenOld = SelectPen( hdc, hpenBtn );
            }

         /* Do we draw lines, especially those at the root?
          */
         fLines = ( dwStyle & TVS_HASLINES ) != 0;
         fLinesAtRoot = ( dwStyle & TVS_LINESATROOT ) != 0;
         fOwnerdraw = ( dwStyle & TVS_OWNERDRAW ) != 0;

         /* If ownerdraw, prime the structure.
          */
         if( fOwnerdraw )
            {
            dis.CtlType = 0;
            dis.CtlID = GetDlgCtrlID( hwnd );
            dis.hwndItem = hwnd;
            dis.hDC = hdc;
            }

         /* Start from the first visible item and only
          * paint those items within the clipping rectangle.
          */
         pItem = pTreeview->pFirstVisible;
         y = 0;
         while( pItem && y < ps.rcPaint.bottom )
            {
            if( pItem->id.rcItem.bottom > ps.rcPaint.top )
               {
               COLORREF crText;
               COLORREF crBack;
               LPSTR pszText;
               int x1, y1;

               /* Draw the thread linking lines if TVS_HASLINES is defined.
                */
               if( fLines && pItem->id.nLevel > 0 )
                  {
                  ITEM FAR * pThisItem;
                  int yh;
                  int xh;

                  /* Compute some ever-so-useful values.
                   */
                  yh = pTreeview->dyLine / 2;
                  xh = pTreeview->cIndent / 2;
                  x1 = ( ( pItem->id.nLevel - 1 ) * pTreeview->cIndent ) + xh;
                  y1 = y;
                  if( fButtons )
                     x1 += pTreeview->cIndent;

                  /* Draw the node lines (ie. the lines at the
                   * right-most part of the line.
                   */
                  if( pItem->pNextItem == NULL )
                     {
                     MoveToEx( hdc, x1, y1, NULL );
                     LineTo( hdc, x1, y1 + yh );
                     LineTo( hdc, x1 + xh, y1 + yh );
                     }
                  else
                     {
                     MoveToEx( hdc, x1, y1, NULL );
                     LineTo( hdc, x1, y1 + pTreeview->dyLine );
                     MoveToEx( hdc, x1, y1 + yh, NULL );
                     LineTo( hdc, x1 + xh, y1 + yh );
                     }

                  /* Now draw the lines to the left. To do this, we
                   * go to our parent and if it isn't the last item
                   * in the list, draw a vertical bar.
                   */
                  pThisItem = pItem->pParentItem;
                  while( pThisItem )
                     {
                     if( !fLinesAtRoot && NULL == pThisItem->pParentItem )
                        break;
                     x1 -= pTreeview->cIndent;
                     if( pThisItem->pNextItem )
                        {
                        MoveToEx( hdc, x1, y1, NULL );
                        LineTo( hdc, x1, y1 + pTreeview->dyLine );
                        }
                     pThisItem = pThisItem->pParentItem;
                     }
                  }

               /* Draw buttons iff.
                * 1. The control has the TVS_HASBUTTONS attribute.
                * 2. The item has children.
                */
               if( fButtons && pItem->id.cChildren )
                  {
                  HPEN hpen;
                  int dxButtonOffset;
                  int dyButtonOffset;
                  int dxIndent;
                  int dxButton;
                  int dyButton;
                  RECT rc;

                  /* Compute the stuff we need to work with.
                   */
                  dxButton = 9;
                  dyButton = 9;
                  dyButtonOffset = ( pTreeview->dyLine - dyButton ) / 2;
                  dxButtonOffset = ( pTreeview->cIndent - dxButton ) / 2;
                  if( pItem->id.nLevel )
                     dxIndent = pItem->id.nLevel * pTreeview->cIndent;
                  else
                     dxIndent = 0;

                  /* Draw the button frame.
                   */
                  SetRect( &rc, dxIndent + dxButtonOffset,
                                y + dyButtonOffset,
                                dxIndent + dxButtonOffset + dxButton,
                                y + dyButtonOffset + dyButton );
                  Rectangle( hdc, rc.left, rc.top, rc.right, rc.bottom );

                  /* Draw the plus or minus symbol inside the button depending
                   * on whether the item is expanded or not.
                   */
                  ASSERT( NULL != hpenText );
                  hpen = SelectPen( hdc, hpenText );
                  if( pItem->id.state & TVIS_EXPANDED )
                     {
                     MoveToEx( hdc, rc.left + 2, rc.top + ( dyButton / 2 ), NULL );
                     LineTo( hdc, rc.right - 2, rc.top + ( dyButton / 2 ) );
                     }
                  else
                     {
                     MoveToEx( hdc, rc.left + 2, rc.top + ( dyButton / 2 ), NULL );
                     LineTo( hdc, rc.right - 2, rc.top + ( dyButton / 2 ) );
                     MoveToEx( hdc, rc.left + ( dxButton / 2 ), rc.top + 2, NULL );
                     LineTo( hdc, rc.left + ( dxButton / 2 ), rc.bottom - 2 );
                     }
                  SelectPen( hdc, hpen );
                  }

               /* If this item has an associated image, draw the image.
                */
               if( pTreeview->himlNormal )
                  {
                  int x1, y1;
                  int index;

                  x1 = pItem->id.nLevel * pTreeview->cIndent;
                  y1 = y + ( ( pTreeview->dyLine - pTreeview->cyImage ) / 2 );
                  if( pItem->id.state & TVIS_SELECTED )
                     index = GetSelectedImageIndex( hwnd, pItem );
                  else
                     index = GetImageIndex( hwnd, pItem );
                  if( fButtons )
                     x1 += pTreeview->cIndent;
                  if( I_NOIMAGE != index )
                     Amctl_ImageList_Draw( pTreeview->himlNormal, index, hdc, x1, y1, ILD_TRANSPARENT );
                  }
            
               /* If we're an owner-draw control, call the parent to do the
                * drawing for us.
                */
               if( fOwnerdraw )
                  {
                  TV_DRAWITEM tvid;

                  dis.rcItem = pItem->id.rcItem;
                  dis.rcItem.right = rcWnd.right;
                  dis.itemState = 0;
                  dis.itemAction = ODA_DRAWENTIRE;
                  dis.itemData = (DWORD)(LPSTR)&tvid;
                  tvid.pszText = GetItemText( hwnd, pItem );
                  tvid.tv.state = pItem->id.state;
                  tvid.tv.lParam = pItem->id.lParam;
                  tvid.tv.hItem = (HTREEITEM)pItem;
                  if( pItem->id.state & TVIS_SELECTED )
                     dis.itemState |= ODS_SELECTED;
                  if( pItem->id.state & TVIS_FOCUSED )
                     dis.itemState |= ODS_FOCUS;
                  SendMessage( GetParent( hwnd ), WM_DRAWITEM, dis.CtlID, (LPARAM)(LPSTR)&dis );
                  }
            
               /* Draw the label of the item. If it is selected, draw the
                * selection rectangle around it.
                */
               else
                  {
                  RECT rc;
                  int xt;
                  int yt;

                  /* Compute the text rectangle.
                   */
                  rc = pItem->id.rcItem;
                  yt = rc.top + ( pTreeview->dyLine - pTreeview->dyFont ) / 2;
                  xt = rc.left + 3;
                  pszText = GetItemText( hwnd, pItem );

                  /* If the item is selected, change to the selection colours.
                   */
                  if( pItem->id.state & TVIS_SELECTED )
                     {
                     crText = SetTextColor( hdc, GetSysColor( COLOR_HIGHLIGHTTEXT ) );
                     crBack = SetBkColor( hdc, GetSysColor( COLOR_HIGHLIGHT ) );
            
                     /* Draw the text label.
                      */
                     ExtTextOut( hdc, xt, yt, ETO_OPAQUE, &rc, pszText, lstrlen( pszText ), NULL );
                  
                     /* If the treeview control has the focus, then draw
                      * a focus rectangle around the selected item.
                      */
                     if( pItem->id.state & TVIS_FOCUSED )
                        DrawFocusRect( hdc, &rc );
                  
                     /* Restore the original colours.
                      */
                     SetTextColor( hdc, crText );
                     SetBkColor( hdc, crBack );
                     }
                  else
                     ExtTextOut( hdc, xt, yt, ETO_OPAQUE, &rc, pszText, lstrlen( pszText ), NULL );
                  }
               }
            pItem = GetNextVisibleItem( pItem );
            y += pTreeview->dyLine;
            }

         /* End painting and tidy up.
          */
         if( fButtons )
            {
            ASSERT( NULL != hpenOld );
            ASSERT( NULL != hbrOld );
            ASSERT( NULL != hpenText );
            ASSERT( NULL != hpenBtn );
            ASSERT( NULL != hbrBtn );
            SelectPen( hdc, hpenOld );
            SelectBrush( hdc, hbrOld );
            DeleteBrush( hbrBtn );
            DeletePen( hpenBtn );
            DeletePen( hpenText );
            }
         SelectFont( hdc, hOldFont );
         EndPaint( hwnd, &ps );
         return( 0L );
         }

      case WM_DESTROY:
         pTreeview = GetBlock( hwnd );
         EndLabelEdit( hwnd, pTreeview, TRUE );
         KillTimer( hwnd, IDT_EDITTIMER );
         DeleteAllItems( hwnd, pTreeview );
         pItem = pTreeview->pRootItem;

         while( pItem )
         {
            ITEM FAR * pNextItem;

            pNextItem = pItem->pNextItem;
            if( pItem->id.pszText && pItem->id.pszText != LPSTR_TEXTCALLBACK )
               FreeMemory( &pItem->id.pszText );
            FreeMemory( &pItem );
            pItem = pNextItem;
         }

         FreeMemory( &pTreeview );
         return( 0L );
      }
   return( DefWindowProc( hwnd, message, wParam, lParam ) );
}

static void FASTCALL Treeview_OnVScroll(HWND hwnd, HWND hwndCtl, UINT code, int pos)
{
   TREEVIEW FAR   *pTreeview;

   pTreeview = GetBlock( hwnd );
   switch( code )
      {
      case SB_THUMBTRACK:
         ScrollList( hwnd, pos - pTreeview->nScrollPos );
         break;

      case SB_THUMBPOSITION:
         ScrollList( hwnd, pos - pTreeview->nScrollPos );
         break;

      case SB_LINEDOWN:
         ScrollList( hwnd, 1 );
         break;

      case SB_LINEUP:
         ScrollList( hwnd, -1 );
         break;

      case SB_PAGEDOWN:
         ScrollList( hwnd, pTreeview->cVisibleItems );
         break;

      case SB_PAGEUP:
         ScrollList( hwnd, -pTreeview->cVisibleItems );
         break;
      }
}

/* This function sorts child items by calling a specified
 * comparision function to determine the order. If fRecurse is
 * set, all sub-parent items are also sorted.
 */
static BOOL FASTCALL SortChildItemsCB( HWND hwnd, TREEVIEW FAR * pTreeview, LPTV_SORTCB lptvscb, BOOL fRecurse )
{
   ITEM FAR * pParentItem;
   ITEM FAR * pItem;
   BOOL fSwapped;
   BOOL fItemVisible;

   /* First sort all child items.
    */
   pParentItem = (ITEM FAR *)lptvscb->hParent;
   if( NULL == pParentItem->pFirstChild )
      return( FALSE );

   /* How shall we sort? Bubble sort?
    */
   fSwapped = TRUE;
   fItemVisible = FALSE;
   while( fSwapped )
      {
      pItem = pParentItem->pFirstChild;
      fSwapped = FALSE;
      while( pItem->pNextItem )
         {
         if( (*lptvscb->lpfnCompare)( pItem->id.lParam, pItem->pNextItem->id.lParam, lptvscb->lParam ) > 0 )
            {
            ITEMDATA id;

            id = pItem->id;
            pItem->id = pItem->pNextItem->id;
            pItem->pNextItem->id = id;
            fSwapped = TRUE;
            fItemVisible = !IsRectEmpty( &pItem->id.rcItem ) || !IsRectEmpty( &pItem->pNextItem->id.rcItem );
            }
         pItem = pItem->pNextItem;
         }
      }

   /* Don't bother refreshing if no item is visible.
    */
   if( fItemVisible )
   {
      RecalcTreelistSize( hwnd, pTreeview, NULL );
      ReadjustScrollPos( hwnd, pTreeview );
   }


   /* Okay. If we recurse, call this function for all
    * sub-parents.
    */
   if( fRecurse )
      {
      pItem = pParentItem->pFirstChild;
      while( pItem )
         {
         if( pItem->pFirstChild )
            {
            lptvscb->hParent = (HTREEITEM)pItem;
            SortChildItemsCB( hwnd, pTreeview, lptvscb, fRecurse );
            }
         pItem = pItem->pNextItem;
         }
      }
   return( TRUE );
}

/* This function sorts child items into ascending alphabetical order. If
 * fRecurse is TRUE, we also sort any sub-child items.
 */
static BOOL FASTCALL SortChildItems( HWND hwnd, TREEVIEW FAR * pTreeview, ITEM FAR * pParentItem, BOOL fRecurse )
{
   ITEM FAR * pItem;
   BOOL fSwapped;
   BOOL fItemVisible;

   /* Make sure we have some children to sort.
    */
   if( NULL == pParentItem->pFirstChild )
      return( FALSE );

   /* Sort using the standard bubble sort and the early-out
    * flag. Since we're sorting on text items, we may have to issue
    * a series of callbacks for each LPSTR_TEXTCALLBACK item.
    */
   fSwapped = TRUE;
   fItemVisible = FALSE;
   while( fSwapped )
      {
      pItem = pParentItem->pFirstChild;
      fSwapped = FALSE;
      while( pItem->pNextItem )
         {
         LPSTR pszText1;
         LPSTR pszText2;

         pszText1 = GetItemText( hwnd, pItem );
         pszText2 = GetItemText( hwnd, pItem->pNextItem );
         if( lstrcmpi( pszText1, pszText2 ) > 0 )
            {
            ITEMDATA id;

            id = pItem->id;
            pItem->id = pItem->pNextItem->id;
            pItem->pNextItem->id = id;
            fSwapped = TRUE;
            fItemVisible = !IsRectEmpty( &pItem->id.rcItem ) || !IsRectEmpty( &pItem->pNextItem->id.rcItem );
            }
         pItem = pItem->pNextItem;
         }
      }

   /* Don't bother refreshing if no item is visible.
    */
   if( fItemVisible )
   {
      RecalcTreelistSize( hwnd, pTreeview, NULL );
      ReadjustScrollPos( hwnd, pTreeview );
   }

   /* Okay. If we recurse, call this function for all
    * sub-parents.
    */
   if( fRecurse )
      {
      pItem = pParentItem->pFirstChild;
      while( pItem )
         {
         if( pItem->pFirstChild )
            SortChildItems( hwnd, pTreeview, pItem, fRecurse );
         pItem = pItem->pNextItem;
         }
      }
   return( TRUE );
}

/* This function retrieves the index of the selected image to be
 * displayed in the treeview control. If the iSelectedImage field
 * of the pItem structure is I_IMAGECALLBACK, then we call the
 * parent window for the information.
 */
static int FASTCALL GetSelectedImageIndex( HWND hwnd, ITEM FAR * pItem )
{
   if( pItem->id.iSelectedImage == I_IMAGECALLBACK )
      {
      TV_DISPINFO tvdi;

      tvdi.item.mask = TVIF_IMAGE|TVIF_PARAM|TVIF_HANDLE|TVIF_STATE;
      tvdi.item.lParam = pItem->id.lParam;
      tvdi.item.hItem = (HTREEITEM)pItem;
      tvdi.item.state = pItem->id.state;
      Amctl_SendNotify( GetParent( hwnd ), hwnd, TVN_GETDISPINFO, (LPNMHDR)&tvdi );
      return( tvdi.item.iSelectedImage );
      }
   return( pItem->id.iSelectedImage );
}

/* This function retrieves the index of the image to be displayed
 * in the treeview control. If the iSelectedImage field of the pItem
 * structure is I_IMAGECALLBACK, then we call the parent window for
 * the information.
 */
static int FASTCALL GetImageIndex( HWND hwnd, ITEM FAR * pItem )
{
   if( pItem->id.iSelectedImage == I_IMAGECALLBACK )
      {
      TV_DISPINFO tvdi;

      tvdi.item.mask = TVIF_IMAGE|TVIF_PARAM|TVIF_HANDLE|TVIF_STATE;
      tvdi.item.lParam = pItem->id.lParam;
      tvdi.item.hItem = (HTREEITEM)pItem;
      tvdi.item.state = pItem->id.state;
      Amctl_SendNotify( GetParent( hwnd ), hwnd, TVN_GETDISPINFO, (LPNMHDR)&tvdi );
      return( tvdi.item.iImage );
      }
   return( pItem->id.iImage );
}

/* This function retrieves a pointer to the text label for the specified
 * item. If the pszText field is LPSTR_TEXTCALLBACK, then it calls the parent
 * window for the actual text string.
 */
static LPSTR FASTCALL GetItemText( HWND hwnd, ITEM FAR * pItem )
{
   if( pItem->id.pszText == LPSTR_TEXTCALLBACK )
      {
      TV_DISPINFO tvdi;

      tvdi.item.mask = TVIF_TEXT|TVIF_PARAM|TVIF_HANDLE|TVIF_STATE;
      tvdi.item.lParam = pItem->id.lParam;
      tvdi.item.hItem = (HTREEITEM)pItem;
      tvdi.item.state = pItem->id.state;
      Amctl_SendNotify( GetParent( hwnd ), hwnd, TVN_GETDISPINFO, (LPNMHDR)&tvdi );
      return( tvdi.item.pszText );
      }
   return( pItem->id.pszText );
}

/* This function selects the specified item. The pItem parameter is
 * the handle of the item to be selected; the action parameter specifies
 * whether the item was selected with the mouse, keyboard or any other
 * action. Finally fExpand is set if the child item is inside a collapsed
 * parent and we must expand the parent item.
 */
static void FASTCALL SelectItem( HWND hwnd, TREEVIEW FAR * pTreeview, ITEM FAR * pItem, int action, BOOL fExpand )
{
   NM_TREEVIEW nmtv;
   BOOL fSelect;

   EndLabelEdit( hwnd, pTreeview, FALSE );
   nmtv.action = action;
   nmtv.itemOld.hItem = (HTREEITEM)pTreeview->pSelectedItem;
   nmtv.itemNew.hItem = (HTREEITEM)pItem;
   GetCursorPos( &nmtv.ptDrag );
   if( Amctl_SendNotify( GetParent( hwnd ), hwnd, TVN_SELCHANGING, (LPNMHDR)&nmtv ) == FALSE )
      {
      /* If the selected item is inside a collapsed parent, then
       * expand the parent and any parents of the parent until we
       * reach a parent which is expanded.
       */
//    if( !Amdb_IsCategoryPtr( (PCAT)pItem ) )
         fSelect = EnsureVisible( hwnd, pTreeview, pItem );

      /* We got the okay to go ahead with the selection.
       */
      if( fSelect )
         {
         if( NULL != pTreeview->pSelectedItem )
            {
            pTreeview->pSelectedItem->id.state &= ~(TVIS_SELECTED|TVIS_FOCUSED);
            if( InvalidateItem( hwnd, pTreeview, pTreeview->pSelectedItem, FALSE ) )
               UpdateWindow( hwnd );
            }
         pTreeview->pSelectedItem = pItem;
         pTreeview->pSelectedItem->id.state |= TVIS_SELECTED;

         /* Set the TVIS_FOCUSED attribute if the window has focus
          */
         if( GetFocus() == hwnd )
            pTreeview->pSelectedItem->id.state |= TVIS_FOCUSED;
         if( InvalidateItem( hwnd, pTreeview, pTreeview->pSelectedItem, FALSE ) )
            UpdateWindow( hwnd );
         Amctl_SendNotify( GetParent( hwnd ), hwnd, TVN_SELCHANGED, (LPNMHDR)&nmtv );
         }
      }
}

/* This function toggles the expanded/collapsed state of the specified
 * item.
 */
static BOOL FASTCALL ToggleItem( HWND hwnd, TREEVIEW FAR * pTreeview, ITEM FAR * pItem )
{
   if( pItem->id.cChildren )
      {
      if( !( pItem->id.state & TVIS_EXPANDED ) )
         return( ExpandItem( hwnd, pTreeview, pItem ) );
      else
         return( CollapseItem( hwnd, pTreeview, pItem, FALSE ) );
      }
   return( FALSE );
}

/* This function invalidates the specified item for repainting,
 */
static BOOL FASTCALL InvalidateItem( HWND hwnd, TREEVIEW FAR * pTreeview, ITEM FAR * pItem, BOOL fButton )
{
   if( !IsRectEmpty( &pItem->id.rcItem ) )
      {
      RECT rc;

      CopyRect( &rc, &pItem->id.rcItem );
      if( fButton && pItem->id.cChildren )
         rc.left -= ( pTreeview->cIndent + pTreeview->cxImage );
      if( GetWindowStyle( hwnd ) & TVS_OWNERDRAW )
         rc.right = 2000;
      InvalidateRect( hwnd, &rc, TRUE );
      return( TRUE );
      }
   return( FALSE );
}

static void FASTCALL IncrementalSearch( HWND hwnd, TREEVIEW FAR * pTreeview, char ch )
{
   if( pTreeview->cScan < 31 && isprint(ch) )
      {
      ITEM FAR * pItem;

      /* Start from the selected item and find the
       * next item with this pattern.
       */
      pItem = pTreeview->pSelectedItem;
      if( NULL == pItem )
         pItem = pTreeview->pRootItem;

      /* If nothing to search, exit now.
       */
      if( NULL == pItem )
         return;

      /* Append this to the incremental scan string.
       */
      if( ch != ' ' )
         {
         pTreeview->szScan[ pTreeview->cScan++ ] = tolower(ch);
         pTreeview->szScan[ pTreeview->cScan ] = '\0';
         }
      else if( NULL != pItem->pFirstChild )
         pItem = pItem->pFirstChild;
      else if( NULL == pItem->pNextItem && pItem->pParentItem )
         {
         while( NULL == pItem->pNextItem && pItem->pParentItem )
            pItem = pItem->pParentItem;
         pItem = pItem->pNextItem;
         }
      else
         pItem = pItem->pNextItem;

      /* If nothing to search, exit now.
       */
      if( NULL == pItem )
         return;

      /* Do the search. If it failed, wrap back to the start of the list and
       * continue from there.
       */
      if( NULL != ( pItem = ISearch( hwnd, pTreeview, pItem ) ) )
         SelectItem( hwnd, pTreeview, pItem, TVC_BYKEYBOARD, TRUE );
      else
         {
         pItem = pTreeview->pRootItem;
         if( NULL != ( pItem = ISearch( hwnd, pTreeview, pItem ) ) )
            SelectItem( hwnd, pTreeview, pItem, TVC_BYKEYBOARD, TRUE );
         }
      }
}

/* This function handles keyboard interaction between the user and the
 * treeview control. NOTE: ALL keys must eventually be passed onto the
 * parent window via TVN_KEYDOWN!
 */
static void FASTCALL DoKeyboard( HWND hwnd, TREEVIEW FAR * pTreeview, int vKey )
{
   TV_KEYDOWN tvkd;
   ITEM FAR * pItem;
   BOOL fCtrl;
   int c;

   fCtrl = GetKeyState( VK_CONTROL ) & 0x8000;
   switch( vKey )
      {
      case VK_F2:
         if( NULL != pTreeview->pSelectedItem )
            BeginLabelEdit( hwnd, pTreeview, pTreeview->pSelectedItem );
         break;

      case VK_RETURN: {
         NMHDR nmhdr;

         Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_RETURN, &nmhdr );
         break;
         }

      case VK_BACK:
         /* Backspace sets the current selection to the parent of
          * the current item, if there is one.
          */
         pItem = pTreeview->pSelectedItem;
         if( pItem && pItem->pParentItem )
            SelectItem( hwnd, pTreeview, pItem->pParentItem, TVC_BYKEYBOARD, FALSE );
         pTreeview->cScan = 0;
         break;

      case VK_ADD:
         if( fCtrl )
            ExpandAllItems( hwnd, pTreeview );
         else if( NULL != pTreeview->pSelectedItem )
            ExpandItem( hwnd, pTreeview, pTreeview->pSelectedItem );
         pTreeview->cScan = 0;
         break;

      case VK_SUBTRACT:
         if( fCtrl )
            CollapseAllItems( hwnd, pTreeview );
         else
            {
            pItem = pTreeview->pSelectedItem;
            if( NULL == pItem->pFirstChild )
               pItem = pItem->pParentItem;
            if( NULL != pItem )
               CollapseItem( hwnd, pTreeview, pItem, FALSE );
            }
         pTreeview->cScan = 0;
         break;

      case VK_HOME:
         /* The user pressed the Home key. If the Control key
          * is down, scroll to the home position otherwise
          * move the selection to the first item.
          */
         if( fCtrl )
            ScrollList( hwnd, -pTreeview->cExpandedItems );
         else
            {
            SelectItem( hwnd, pTreeview, pTreeview->pRootItem, TVC_BYKEYBOARD, FALSE );
            pTreeview->cScan = 0;
            }
         break;

      case VK_END:
         /* The user pressed the Home key. If the Control key
          * is down, scroll to the home position otherwise
          * move the selection to the last item.
          */
         if( fCtrl )
            ScrollList( hwnd, pTreeview->cExpandedItems );
         else
            {
            ITEM FAR * pNextItem;

            pItem = pTreeview->pSelectedItem;
            while( pNextItem = GetNextVisibleItem( pItem ) )
               pItem = pNextItem;
            SelectItem( hwnd, pTreeview, pItem, TVC_BYKEYBOARD, FALSE );
            pTreeview->cScan = 0;
            }
         break;

      case VK_PRIOR:
         /* The user pressed the PgUp key. If the Control key
          * is down, scroll up one page, otherwise move the
          * selection up one page.
          */
         if( fCtrl )
            ScrollList( hwnd, -pTreeview->cVisibleItems );
         else
            {
            ITEM FAR * pPrevVisible;

            pItem = pTreeview->pSelectedItem;
            if( pItem )
               {
               pPrevVisible = GetPrevVisibleItem( pItem );
               for( c = 0; pPrevVisible && c < pTreeview->cVisibleItems; ++c )
                  {
                  pItem = pPrevVisible;
                  pPrevVisible = GetPrevVisibleItem( pItem );
                  }
               SelectItem( hwnd, pTreeview, pItem, TVC_BYKEYBOARD, FALSE );
               pTreeview->cScan = 0;
               }
            }
         break;
         
      case VK_UP:
         /* The user pressed the Up key. If the Control key
          * is down, scroll up one visible item, otherwise move the
          * selection up visible item.
          */
         if( fCtrl )
            ScrollList( hwnd, -1 );
         else if( pTreeview->pSelectedItem )
            {
            if( pItem = GetPrevVisibleItem( pTreeview->pSelectedItem ) )
               SelectItem( hwnd, pTreeview, pItem, TVC_BYKEYBOARD, FALSE );
            }
         pTreeview->cScan = 0;
         break;

      case VK_DOWN:
         /* The user pressed the Down key. If the Control key
          * is down, scroll down one visible item, otherwise move the
          * selection down visible item.
          */
         if( fCtrl )
            ScrollList( hwnd, 1 );
         else if( pTreeview->pSelectedItem )
            {
            if( pItem = GetNextVisibleItem( pTreeview->pSelectedItem ) )
               SelectItem( hwnd, pTreeview, pItem, TVC_BYKEYBOARD, FALSE );
            }
         pTreeview->cScan = 0;
         break;

      case VK_NEXT:
         /* The user pressed the PgDn key. If the Control key
          * is down, scroll down one page, otherwise move the
          * selection down a page.
          */
         if( fCtrl )
            ScrollList( hwnd, pTreeview->cVisibleItems );
         else
            {
            ITEM FAR * pNextVisible;

            pItem = pTreeview->pSelectedItem;
            if( pItem )
               {
               pNextVisible = GetNextVisibleItem( pItem );
               for( c = 0; pNextVisible && c < pTreeview->cVisibleItems; ++c )
                  {
                  pItem = pNextVisible;
                  pNextVisible = GetNextVisibleItem( pItem );
                  }
               SelectItem( hwnd, pTreeview, pItem, TVC_BYKEYBOARD, FALSE );
               }
            }
         pTreeview->cScan = 0;
         break;
      }

   /* Notify the parent that a key was pressed.
    */
   tvkd.wVKey = vKey;
   tvkd.flags = 0;
   Amctl_SendNotify( GetParent( hwnd ), hwnd, TVN_KEYDOWN, (LPNMHDR)&tvkd );
}

/* This function ensures that the specified item is visible. If it
 * is a child item and the parent is collapsed, then the parent items
 * are all expanded. Next, it checks that the item is visible in
 * the client rectangle and, if not, redraws the item in the client
 * rectangle.
 */
static BOOL FASTCALL EnsureVisible( HWND hwnd, TREEVIEW FAR * pTreeview, ITEM FAR * pItem )
{
   if( !IsCompletelyVisible( hwnd, pItem ) && !pTreeview->fDragging)
      {
      if( pItem->pParentItem && !( pItem->pParentItem->id.state & TVIS_EXPANDED ) )
      {
         if( !ExpandItem( hwnd, pTreeview, pItem->pParentItem ) )
            return( FALSE );
      }
      else if( ( pItem->id.nLevel == 2 ) && !( pItem->id.state & TVIS_EXPANDED ) && !( pItem->id.state & TVIS_EXPANDEDONCE ) )
      {
         if( !ExpandItem( hwnd, pTreeview, pItem ) )
            return( FALSE );
      }
      if( !IsCompletelyVisible( hwnd, pItem ) )
         {
         ITEM FAR * pVisibleItem;
         int iMiddle;
         RECT rc;

         /* Quick test. If pItem is pVisible->pPrevItem, then scroll down
          * one line.
          */
         ClearVisibleItemRect( pTreeview->pFirstVisible );
         if( ( NULL != pTreeview->pFirstVisible ) && pItem == pTreeview->pFirstVisible->pPrevItem )
            ScrollList( hwnd, -1 );
         else
            {
            /* Still not visible? Put this item in the middle of the treeview
             * control and set pFirstVisible as appropriate.
             */
            GetClientRect( hwnd, &rc );
            ASSERT( 0 != pTreeview->dyFont );
            iMiddle = ( ( rc.bottom - rc.top ) / pTreeview->dyFont ) / 2;

            /* Loop back until iMiddle is zero or pItem->pPrevItem is NULL.
             */
            pVisibleItem = pItem;
            while( iMiddle && ( pItem = GetPrevVisibleItem( pVisibleItem ) ) )
               {
               pVisibleItem = pItem;
               --iMiddle;
               }
            /* If the first visible item has changed, recompute the
             * treeview control.
             */
            if( pVisibleItem != pTreeview->pFirstVisible )
               {
               pTreeview->pFirstVisible = pVisibleItem;
               RecalcTreelistSize( hwnd, pTreeview, NULL );
               ReadjustScrollPos( hwnd, pTreeview );
               }
            }
         }
      }
   return( TRUE );
}

/* Adjusts the scroll pos according to the first visible item and the number of
 * displayble items "above" it.
 */
static void FASTCALL ReadjustScrollPos( HWND hwnd, TREEVIEW FAR * pTreeview )
{
   BOOL fFound;

   pTreeview->nScrollPos = ItemFindIndex( pTreeview->pFirstVisible, pTreeview->pRootItem, &fFound );
   ASSERT( fFound );
   SetScrollPos( hwnd, SB_VERT, pTreeview->nScrollPos, TRUE );
}

/* Returns whether or not the specified item is completely visible
 * within the listbox.
 */
static BOOL FASTCALL IsCompletelyVisible( HWND hwnd, ITEM FAR * pItem )
{
   RECT rc;

   if( IsRectEmpty( &pItem->id.rcItem ) )
      return( FALSE );
   GetClientRect( hwnd, &rc );
   if( pItem->id.rcItem.bottom <= rc.bottom )
      return( TRUE );
   return( FALSE );
}

/* This function expands ALL treeview items.
 */
static BOOL FASTCALL ExpandAllItems( HWND hwnd, TREEVIEW FAR * pTreeview )
{
   ITEM FAR * pItem;

// pItem = pTreeview->pRootItem;
   pItem = pTreeview->pSelectedItem;
   while( pItem )
      {
      /* Expand this item. Will do nothing if the item
       * is already expanded.
       */
      ExpandItem( hwnd, pTreeview, pItem );

      /* If this item has any children, expand those
       */
      if( pItem->pFirstChild )
         {
         pItem = pItem->pFirstChild;
         continue;
         }

      /* Jump to the next item.
       */
      if( NULL == pItem->pNextItem && pItem->pParentItem && ( pItem->pParentItem != pTreeview->pSelectedItem ) )
         while( NULL == pItem->pNextItem && pItem->pParentItem )
            pItem = pItem->pParentItem;
      pItem = pItem->pNextItem;

      if( pItem )
      {
      if( NULL == pItem->pParentItem  )
         {
         pItem = NULL;
         continue;
         }
      }
      }

   ReadjustScrollPos( hwnd, pTreeview );
   return( TRUE );
}

/* This function expands the specified item. The specified item must
 * be a parent item and if it is also a child item, the parent item
 * must also be expanded.
 */
static BOOL FASTCALL ExpandItem( HWND hwnd, TREEVIEW FAR * pTreeview, ITEM FAR * pItem )
{
   if( pItem->id.cChildren && !( pItem->id.state & TVIS_EXPANDED ) )
      {
      NM_TREEVIEW nmtv;

      nmtv.action = TVE_EXPAND;
      nmtv.itemNew.hItem = (HTREEITEM)pItem;
      nmtv.itemNew.lParam = pItem->id.lParam;
      nmtv.itemNew.state = pItem->id.state;
      nmtv.itemNew.pszText = pItem->id.pszText;
      GetCursorPos( &nmtv.ptDrag );
      if( Amctl_SendNotify( GetParent( hwnd ), hwnd, TVN_ITEMEXPANDING, (LPNMHDR)&nmtv ) == FALSE )
         {
         ClearVisibleItemRect( pItem );
         pItem->id.state |= TVIS_EXPANDED|TVIS_EXPANDEDONCE;
         pTreeview->cExpandedItems += pItem->cTrueChildren;
         InvalidateItem( hwnd, pTreeview, pItem, TRUE );
         if( pItem->pParentItem )
            ExpandItem( hwnd, pTreeview, pItem->pParentItem );
         RecalcTreelistSize( hwnd, pTreeview, NULL );
         ReadjustScrollPos( hwnd, pTreeview );
         Amctl_SendNotify( GetParent( hwnd ), hwnd, TVN_ITEMEXPANDED, (LPNMHDR)&nmtv );
         return( TRUE );
         }
      return( FALSE );
      }
   return( TRUE );
}

/* This function collapse ALL treeview items.
 */
static BOOL FASTCALL CollapseAllItems( HWND hwnd, TREEVIEW FAR * pTreeview )
{
   ITEM FAR * pItem;

   pItem = pTreeview->pSelectedItem;
   if( NULL != pItem )
      {
      pItem = pItem->pFirstChild;
      while( pItem )
         {
         /* Collapse this item. This will collapse all
          * subitems.
          */
         CollapseItem( hwnd, pTreeview, pItem, FALSE );
         pItem = pItem->pNextItem;
         }
      }
   pItem = pTreeview->pSelectedItem;
   CollapseItem( hwnd, pTreeview, pItem, FALSE );

   ReadjustScrollPos( hwnd, pTreeview );
   return( TRUE );
}

/* This function collapses the specified item.
 */
static BOOL FASTCALL CollapseItem( HWND hwnd, TREEVIEW FAR * pTreeview, ITEM FAR * pItem, BOOL fReset )
{
   if( pItem->id.cChildren && ( pItem->id.state & TVIS_EXPANDED ) )
      {
      NM_TREEVIEW nmtv;

      nmtv.action = TVE_COLLAPSE;
      nmtv.itemNew.hItem = (HTREEITEM)pItem;
      nmtv.itemNew.lParam = pItem->id.lParam;
      nmtv.itemNew.state = pItem->id.state;
      nmtv.itemNew.pszText = pItem->id.pszText;
      GetCursorPos( &nmtv.ptDrag );
      if( Amctl_SendNotify( GetParent( hwnd ), hwnd, TVN_ITEMEXPANDING, (LPNMHDR)&nmtv ) == FALSE )
         {
         ITEM FAR * pItemChild;

         /* If one of the items children is the first visible item, then make the
          * parent the first visible item.
          */
         pItemChild = pItem->pFirstChild;
         while( pItemChild )
            {
            if( pItemChild == pTreeview->pFirstVisible )
               pTreeview->pFirstVisible = pItem;
            pItemChild = pItemChild->pNextItem;
            }

         pTreeview->cExpandedItems -= NumberExpandedDescendents( pItem );

         /* If the fReset flag is true, we remove all child items
          * from this item. Otherwise we set their item rectangles
          * to empty.
          */
         if( fReset )
            {
            pItemChild = pItem->pFirstChild;
            while( pItemChild )
               {
               ITEM FAR * pNextItem;
               int cDeleted;

               pNextItem = pItemChild->pNextItem;
               cDeleted = DeleteItem( hwnd, pTreeview, pItemChild );
               pTreeview->cItems -= cDeleted;
               pItemChild = pNextItem;
               }
            pItem->pFirstChild = NULL;
            pItem->id.cChildren = 0;
            pItem->cTrueChildren = 0;
            }
         else
            ClearItemRect( pItem );

         /* Is the selected item hidden now? If so, set the
          * current control to be the selected item.
          */
         if( pTreeview->pSelectedItem )
            {
            if( IsRectEmpty( &pTreeview->pSelectedItem->id.rcItem ) )
               SelectItem( hwnd, pTreeview, pItem, TVC_UNKNOWN, FALSE );
            }

         /* Finally, update the control's rectangles.
          */
         InvalidateItem( hwnd, pTreeview, pItem, TRUE );
         RecalcTreelistSize( hwnd, pTreeview, NULL );
         ReadjustScrollPos( hwnd, pTreeview );
         Amctl_SendNotify( GetParent( hwnd ), hwnd, TVN_ITEMEXPANDED, (LPNMHDR)&nmtv );
         return( TRUE );
         }
      return( FALSE );
      }
   return( TRUE );
}

/* This function clears the item rectangles of all child items and
 * any child items below it, then marks each parent collapsed.
 */
static void FASTCALL ClearItemRect( ITEM FAR * pParentItem )
{
   ITEM FAR * pItem;

   pParentItem->id.state &= ~TVIS_EXPANDED;
   pItem = pParentItem->pFirstChild;
   while( pItem )
      {
      SetRectEmpty( &pItem->id.rcItem );
      if( pItem->pFirstChild )
         ClearItemRect( pItem );
      pItem = pItem->pNextItem;
      }
}

static void FASTCALL ClearVisibleItemRect( ITEM FAR * pItem )
{
   while( pItem && !IsRectEmpty( &pItem->id.rcItem ) )
      {
      SetRectEmpty( &pItem->id.rcItem );
      pItem = GetNextVisibleItem( pItem );
      }
}

/* This function begins label editing.
 */
static void FASTCALL BeginLabelEdit( HWND hwnd, TREEVIEW FAR * pTreeview, ITEM FAR * pItem )
{
   int dyRealHeight;
   HFONT hOldFont;
   TEXTMETRIC tm;
   LPSTR pszText;
   int diff;
   RECT rc;
   HDC hdc;

   /* Cancel any existing label edit.
    */
   EndLabelEdit( hwnd, pTreeview, TRUE );

   /* Make sure the label is visible.
    */
   EnsureVisible( hwnd, pTreeview, pItem );

   /* Get some info about the edit font.
    */
   hdc = GetDC( hwnd );
   hOldFont = SelectFont( hdc, pTreeview->hFont );
   GetTextMetrics( hdc, &tm );
   SelectFont( hdc, hOldFont );
   ReleaseDC( hwnd, hdc );

   /* Compute the edit control rectangle.
    */
   CopyRect( &rc, &pItem->id.rcItem );
   dyRealHeight = ( tm.tmHeight + tm.tmExternalLeading ) + ( pTreeview->dyEditTopBorder * 2 );
   diff =  dyRealHeight - ( rc.bottom - rc.top );
   InflateRect( &rc, 0, diff / 2 );

   /* Create the edit control first. This allows the caller to
    * set up the control in response to TVN_BEGINLABELEDIT.
    */
   pszText = GetItemText( hwnd, pItem );
   pTreeview->hwndEdit = CreateWindow( "edit",
                                       pszText,
                                       WS_CHILD|WS_BORDER|ES_AUTOHSCROLL,
                                       rc.left,
                                       rc.top,
                                       ( rc.right - rc.left ) + EDITZONEWIDTH,
                                       rc.bottom - rc.top,
                                       hwnd,
                                       (HMENU)IDD_TREEVIEW_EDIT_CTL,
                                       hLibInst,
                                       0L );
   if( pTreeview->hwndEdit )
      {
      TV_DISPINFO tvdi;
      RECT rc;

      /* Initialise the treeview edit control sufficiently so that if
       * the parent attempts to alter the control styles or send messages to
       * the edit control from within TVN_BEGINLABELEDIT processing, then
       * it works properly.
       */
      SetWindowFont( pTreeview->hwndEdit, pTreeview->hFont, FALSE );
      Edit_SetSel( pTreeview->hwndEdit, 0, 32767 );
      pTreeview->pEditItem = pItem;

      /* Set formatting rectangle width.
       */
      Edit_GetRect( pTreeview->hwndEdit, &rc );
      pTreeview->dxEdit = rc.right - rc.left;

      /* Ask the parent if label editing is permitted.
       */
      tvdi.item.mask = TVIF_HANDLE|TVIF_PARAM|TVIF_STATE;
      tvdi.item.state = pItem->id.state;
      tvdi.item.hItem = (HTREEITEM)pItem;
      tvdi.item.lParam = pItem->id.lParam;
      if( Amctl_SendNotify( GetParent( hwnd ), hwnd, TVN_BEGINLABELEDIT, (LPNMHDR)&tvdi ) == FALSE )
         {
         /* Subclass the edit window to trap RETURN and Esc, and other
          * KEWL things!
          */
         pTreeview->lpfOldEditProc = SubclassWindow( pTreeview->hwndEdit, TreeviewEditProc );
         ShowWindow( pTreeview->hwndEdit, SW_SHOW );
         SetFocus( pTreeview->hwndEdit );
         }
      else
         {
         DestroyWindow( pTreeview->hwndEdit );
         pTreeview->hwndEdit = NULL;
         pTreeview->pEditItem = NULL;
         }
      }
}

/* This is the subclassed edit control window procedure.
 * We subclass it to trap Escape or RETURN and use these to signal
 * the end of the edit operation.
 */
LRESULT EXPORT CALLBACK TreeviewEditProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   TREEVIEW FAR * pTreeview;

   pTreeview = GetBlock( GetParent( hwnd ) );
   ASSERT( NULL != pTreeview );
   ASSERT( NULL != pTreeview->lpfOldEditProc );
   if( WM_GETDLGCODE == msg )
      return( DLGC_WANTCHARS );
   else if( WM_KILLFOCUS == msg && !pTreeview->fEndingLabelEdit )
      PostMessage( GetParent( hwnd ), TVM_ENDEDITLABELNOW, FALSE, 0L );
   else if( WM_CHAR == msg )
      {
      switch( wParam )
         {
         case VK_ESCAPE:
            PostMessage( GetParent( hwnd ), TVM_ENDEDITLABELNOW, TRUE, 0L );
            break;

         case VK_RETURN:
            PostMessage( GetParent( hwnd ), TVM_ENDEDITLABELNOW, FALSE, 0L );
            /* Changed break to return (0). This fixes a bug where if you
               pressed return in the edit box of the tree view in the address
               book, it beeped. The reason it beeped is the windows edit 
               control WndProc was being sent an enter key in a single line
               edit and it repos reponds by beeping. Returning now bypasses
               the windows control altogether.
               YH 16:50 03/04/96 */
            return( 0 ); 
         }
      }
   return( CallWindowProc( pTreeview->lpfOldEditProc, hwnd, msg, wParam, lParam ) );
}

/* If label editing is in progress, terminate editing and destroy
 * the edit window.
 */
static void FASTCALL EndLabelEdit( HWND hwnd, TREEVIEW FAR * pTreeview, BOOL fCancel )
{
   if( pTreeview->hwndEdit )
      {
      TV_DISPINFO tvdi;
      ITEM FAR * pItem;
      static LPSTR pszText;
      int cchEdit;

      ASSERT( IsWindow( pTreeview->hwndEdit ) );
      ASSERT( pTreeview->pEditItem != NULL );
      INITIALISE_PTR(pszText);
      pTreeview->fEndingLabelEdit = TRUE;

      /* If fCancel is FALSE, then the user wants to accept the
       * changes in the edit control (ie. he pressed RETURN in the
       * control itself), so copy the new text to a temporary memory
       * block for passing to the parent. This is temporary in the sense
       * that the parent should NOT cache the pointer to this text!
       */
      if( !fCancel )
         {
         NMHDR nmhdr;

         cchEdit = Edit_GetTextLength( pTreeview->hwndEdit );
         cchEdit = cchEdit > 99 ? 99 : cchEdit;
         if( fNewMemory( &pszText, cchEdit + 1 ) )
            Edit_GetText( pTreeview->hwndEdit, pszText, cchEdit + 1 );
         else
            Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_OUTOFMEMORY, &nmhdr );
         }
      else
         pszText = NULL;

      /* Fill in the TV_ITEM for return to the parent. On
       * return from Amctl_SendNotify, tvdi.item may be different, so
       * don't assume anything about it.
       */
      pItem = pTreeview->pEditItem;
      tvdi.item.mask = TVIF_TEXT|TVIF_HANDLE|TVIF_PARAM;
      tvdi.item.pszText = pszText ? pszText : 0L;
      tvdi.item.hItem = (HTREEITEM)pItem;
      tvdi.item.lParam = pItem->id.lParam;
      Amctl_SendNotify( GetParent( hwnd ), hwnd, TVN_ENDLABELEDIT, (LPNMHDR)&tvdi );

      /* If we had a text block, free it.
       */
      if( pszText )
         FreeMemory( &pszText );

      /* Finally, destroy the edit control.
       */
      DestroyWindow( pTreeview->hwndEdit );
      pTreeview->hwndEdit = NULL;
      pTreeview->pEditItem = NULL;
      pTreeview->fEndingLabelEdit = FALSE;
      }
}

/* This function retrieves hit test info. The lpht parameter must point
 * to a TV_HITTESTINFO structure which will be filled with details about
 * the item at the cursor position specified by lpht->pt.
 */
static ITEM FAR * FASTCALL GetHitTestInfo( HWND hwnd, LPTV_HITTESTINFO lpht )
{
   RECT rc;

   /* Get the client window rectangle.
    */
   GetClientRect( hwnd, &rc );

   /* Start by looking to see if the coordinates are in
    * the client rectangle. If, after all this, lpht->flags
    * is non-zero, don't bother looking further since the
    * cursor is not in the treeview control.
    */
   lpht->flags = 0;
   lpht->hItem = (HTREEITEM)NULL;
   if( lpht->pt.x < rc.left )
      lpht->flags = TVHT_TOLEFT;
   else if( lpht->pt.x > rc.right )
      lpht->flags = TVHT_TORIGHT;
   if( lpht->pt.y < rc.top )
      lpht->flags |= TVHT_ABOVE;
   else if( lpht->pt.y > rc.bottom )
      lpht->flags |= TVHT_BELOW;

   /* The cursor is in the treeview control. Scan the list
    * of visible items to determine where it is.
    */
   if( 0 == lpht->flags )
      {
      TREEVIEW FAR * pTreeview;
      ITEM FAR * pItem;
      BOOL fHasButtons;
      BOOL fOwnerDraw;

      pTreeview = GetBlock( hwnd );
      pItem = pTreeview->pFirstVisible;
      fHasButtons = ( GetWindowStyle( hwnd ) & TVS_HASBUTTONS ) != 0;
      fOwnerDraw = ( GetWindowStyle( hwnd ) & TVS_OWNERDRAW ) != 0;
      while( pItem )
         {
         if( lpht->pt.y >= pItem->id.rcItem.top && lpht->pt.y < pItem->id.rcItem.bottom )
            {
            int dxButtonWidth;
            int dxIndent;
            int index;

            dxIndent = pTreeview->cIndent * pItem->id.nLevel;
            dxButtonWidth = fHasButtons ? pTreeview->cIndent : 0;
            index = I_NOIMAGE;
            if( pTreeview->himlNormal )
               index = GetImageIndex( hwnd, pItem );
            if( lpht->pt.x < dxIndent )
               lpht->flags = TVHT_ONITEMINDENT;
            else if( pItem->id.cChildren && fHasButtons && lpht->pt.x < dxIndent + dxButtonWidth )
               lpht->flags = TVHT_ONITEMBUTTON;
            else if( I_NOIMAGE != index && lpht->pt.x < pItem->id.rcItem.left )
               lpht->flags = TVHT_ONITEMICON;
            else if( fOwnerDraw )
               lpht->flags = TVHT_ONITEMLABEL;
            else if( lpht->pt.x < pItem->id.rcItem.right )
               lpht->flags = TVHT_ONITEMLABEL;
            else
               lpht->flags = TVHT_ONITEMRIGHT;
            break;
            }
         pItem = GetNextVisibleItem( pItem );
         }

      /* If, after all this, pItem is NULL, then we're
       * below the last item in the list so set TVHT_NOWHERE.
       */
      if( !pItem )
         lpht->flags = TVHT_NOWHERE;
      else
         lpht->hItem = (HTREEITEM)pItem;
      }
   return( (ITEM FAR *)lpht->hItem );
}

/* This function scrolls the treeview list by the specified
 * number of rows.
 */
static void FASTCALL ScrollList( HWND hwnd, int cToScroll )
{
   TREEVIEW FAR * pTreeview;
   ITEM FAR * pVisibleItem;
   ITEM FAR * pItem;
   RECT rectClip;
   RECT rectDraw;
   int nScroll;
   int nMaxScrollPos;
   int nRealThumbPos;
   int nScratch;

   /* The following few lines ensure that the bottom of the list is kept at the
    * bottom of the window. If you want to understand what that means then
    * comment this out aand compile. Then bring up a treeview, scroll to the bottom
    * expand the window and click on the down arrow of the scroll bar.
    */
   pTreeview = GetBlock( hwnd );
   nRealThumbPos = GetScrollPos( hwnd, SB_VERT );
   if( nRealThumbPos != pTreeview->nScrollPos )
      cToScroll = nRealThumbPos - pTreeview->nScrollPos;
   nScroll = -cToScroll;

   /* If there is an image list, get the width and height of the images.
    */
   pTreeview->dyLine = max( pTreeview->cyImage, pTreeview->dyFont ) + 2;
   GetClientRect( hwnd, &rectClip );
   rectDraw.left = 0;
   rectDraw.right = rectClip.right;

   /* Code different for win32 because the page size has to be accounted for.
    */
#ifdef WIN32
   if( fNewShell )
      {
      SCROLLINFO scrollInfo;

      scrollInfo.cbSize = sizeof( SCROLLINFO );
      scrollInfo.fMask = SIF_PAGE | SIF_RANGE;
      GetScrollInfo( hwnd, SB_VERT, &scrollInfo );
      nMaxScrollPos = scrollInfo.nMax - scrollInfo.nPage + 1;
      }
   else
#endif
      GetScrollRange( hwnd, SB_VERT, &nScratch, &nMaxScrollPos );

   /* Scan the list to compute the new top item.
    */
   pVisibleItem = pTreeview->pFirstVisible;
   if( cToScroll < 0 )
      {
      rectDraw.top = 0;
      rectDraw.bottom = -cToScroll * pTreeview->dyLine;

      /* Readjust cToScroll so that no scrolling is allowed
       * outside range of scroll bar
       */
      if( cToScroll + pTreeview->nScrollPos < 0 )
         cToScroll = - pTreeview->nScrollPos;
      ASSERT( pTreeview->pFirstVisible != NULL );
      pVisibleItem = pTreeview->pFirstVisible;
      while( cToScroll && ( pItem = GetPrevVisibleItem( pVisibleItem ) ) )
         {
         pVisibleItem = pItem;
         ++cToScroll;
         --pTreeview->nScrollPos;
         }
      }
   else if( cToScroll > 0 )
      {
      ASSERT( pTreeview->pFirstVisible != NULL );
      rectDraw.top = ( ( rectClip.bottom / pTreeview->dyLine ) - cToScroll ) * pTreeview->dyLine;
      rectDraw.bottom = rectClip.bottom;

      /* Readjust cToScroll so that no scrolling is allowed
       * outside range of scroll bar
       */
      if( cToScroll + pTreeview->nScrollPos > nMaxScrollPos )
         cToScroll = nMaxScrollPos - pTreeview->nScrollPos;
      pVisibleItem = pTreeview->pFirstVisible;
      while( cToScroll && ( pItem = GetNextVisibleItem( pVisibleItem ) ) )
         {
         SetRectEmpty( &pVisibleItem->id.rcItem );
         pVisibleItem = pItem;
         --cToScroll;
         ++pTreeview->nScrollPos;
         }
      }

   /* Scroll the window if required.
    */
   if( pVisibleItem != pTreeview->pFirstVisible )
      {
      pTreeview->pFirstVisible = pVisibleItem;
      RecalcTreelistSize( hwnd, pTreeview, &rectDraw);
      ReadjustScrollPos( hwnd, pTreeview );
      AnimScrollWindow( hwnd, 0, nScroll * pTreeview->dyLine, NULL, &rectClip );
      SetScrollPos( hwnd, SB_VERT, pTreeview->nScrollPos, TRUE );
      UpdateWindow( hwnd );
      }
}

/* This function recomputes the rectangles for each visible item.
 * It only works on the visible area, setting their rectangles based
 * on the selected font and client area size. The rectangles for
 * non-visible items are not touched for performance reasons. Instead,
 * the scrolling code is responsible for ensuring that they are set
 * to empty.
 */
static void FASTCALL RecalcTreelistSize( HWND hwnd, TREEVIEW FAR * pTreeview, LPRECT lpRectDraw )
{
   TEXTMETRIC tm;
   HFONT hOldFont;
   ITEM FAR * pItem;
   int cVisibleItems;
   BOOL fButtons;
   BOOL fLinesAtRoot;
   int index;
   RECT rc;
   RECT rcClip;
   HDC hdc;
   int y;
   int nMaxCanDisplay;


   GetClientRect( hwnd, &rc );

   if( lpRectDraw )
      rcClip = *lpRectDraw;
   else
      rcClip = rc;

   fButtons = ( GetWindowStyle( hwnd ) & TVS_HASBUTTONS ) != 0;
   fLinesAtRoot = ( GetWindowStyle( hwnd ) & TVS_LINESATROOT ) != 0;

   /* Compute the item heights.
    */
   hdc = GetDC( hwnd );
   hOldFont = SelectFont( hdc, pTreeview->hFont );
   GetTextMetrics( hdc, &tm );
   pTreeview->dyFont = tm.tmHeight;

   /* If there is an image list, get the width and height of the images.
    */
   pTreeview->dyLine = max( pTreeview->cyImage, pTreeview->dyFont ) + 2;

   /* If the user grows the window, we have to check whether all the
    * items are now displayable, and if so, we need to make sure that
    * the complete list is drawn, otherswise when the scroll bar goes
    * the user won't see the top.
    */
   nMaxCanDisplay = ( rc.bottom - rc.top ) / pTreeview->dyLine;
   if( nMaxCanDisplay >= pTreeview->cExpandedItems )
      {
      pTreeview->pFirstVisible = pTreeview->pRootItem;
      pTreeview->nScrollPos = 0;
      }

   /* Start from the first visible item, if any, and
    * update the rectangles. Don't forget that the rectangles
    * are adjusted horizontally by the indentation depth.
    */
   pItem = pTreeview->pFirstVisible;
   y = rc.top;
   cVisibleItems = 0;
   index = I_NOIMAGE;
   while( pItem && y < rc.bottom )
      {
      static RECT rcItem;
      LPSTR pszText;
      SIZE size;

      if( pTreeview->cxImage )
         index = GetImageIndex( hwnd, pItem );
      pszText = GetItemText( hwnd, pItem );
      GetTextExtentPoint( hdc, pszText, lstrlen( pszText ), &size );
      SetRect( &rcItem, pItem->id.nLevel * pTreeview->cIndent,
                        y,
                        pItem->id.nLevel * pTreeview->cIndent + size.cx + 6,
                        y + pTreeview->dyLine );
      if( fLinesAtRoot || fButtons )
         OffsetRect( &rcItem, pTreeview->cIndent, 0 );
      if( I_NOIMAGE != index )
         OffsetRect( &rcItem, pTreeview->cxImage, 0 );
      if( _fmemcmp( &rcItem, &pItem->id.rcItem, sizeof( RECT ) ) )
         {
         POINT ptTopLeft;

         pItem->id.rcItem = rcItem;
         rcItem.left = 0;
         rcItem.right = rc.right;
         ptTopLeft.x = 0;
         ptTopLeft.y = rcItem.top;
         rcItem.top -= pTreeview->dyLine / 2;
         rcItem.bottom += pTreeview->dyLine / 2;
         if( PtInRect( &rcClip, ptTopLeft ) )
            InvalidateRect( hwnd, &rcItem, TRUE );
         }
      ++cVisibleItems;
      y += pTreeview->dyLine;
      pItem = GetNextVisibleItem( pItem );
      }
   pTreeview->cVisibleItems = cVisibleItems;
   if( pItem )
      pTreeview->pLastVisible = pItem;

   /* If there is any blank space after the end of the last
    * item, then invalidate that space.
    */
   if( y < rcClip.bottom )
      {
      RECT rcSpace;

      SetRect( &rcSpace, 0, y, rc.right, rc.bottom );
      InvalidateRect( hwnd, &rcSpace, TRUE );
      }

   /* Set any items that follow pItem to empty.
    */
   while( pItem && !IsRectEmpty( &pItem->id.rcItem ) )
      {
      SetRectEmpty( &pItem->id.rcItem );
      pItem = GetNextVisibleItem( pItem );
      }

   /* Update the scroll bar.
    */
   ScrollRange( hwnd, pTreeview ); 

   /* Now done.
    */
   SelectFont( hdc, hOldFont );
   ReleaseDC( hwnd, hdc );
}

/* This function returns the handle of the next visible item.
 */
static ITEM FAR * FASTCALL GetNextVisibleItem( ITEM FAR * pItem )
{
   if( pItem->pFirstChild && ( pItem->id.state & TVIS_EXPANDED ) )
      pItem = pItem->pFirstChild;
   else
      {
      while( NULL == pItem->pNextItem && pItem->pParentItem )
         pItem = pItem->pParentItem;
      pItem = pItem->pNextItem;
      }
   return( pItem );
}

/* This function returns the handle of the previous visible item.
 */
static ITEM FAR * FASTCALL GetPrevVisibleItem( ITEM FAR * pItem )
{
   if( NULL == pItem->pPrevItem )
      pItem = pItem->pParentItem;
   else if( pItem->pPrevItem )
      {
      pItem = pItem->pPrevItem;
      while( pItem->pFirstChild && ( pItem->id.state & TVIS_EXPANDED ) )
         {
         pItem = pItem->pFirstChild;
         while( pItem->pNextItem )
            pItem = pItem->pNextItem;
         }
      }
   return( pItem );
}

/* This function inserts an item into the treeview.
 */
static ITEM FAR * FASTCALL InsertItem( HWND hwnd, TREEVIEW FAR * pTreeview, ITEM FAR * pParentItem, ITEM FAR * pInsertAfter, LPTV_ITEM lptvi )
{
   static ITEM FAR * pNewItem;
   NMHDR nmhdr;

   INITIALISE_PTR(pNewItem);
   if( fNewMemory( &pNewItem, sizeof( ITEM ) ) )
      {
      if( NULL == pInsertAfter )
         {
         if( NULL == pParentItem )
            {
            pNewItem->pNextItem = pTreeview->pRootItem;
            pTreeview->pRootItem = pNewItem;
            }
         else
            {
            pNewItem->pNextItem = pParentItem->pFirstChild;
            pParentItem->pFirstChild = pNewItem;
            }
         }
      else
         {
         pNewItem->pNextItem = pInsertAfter->pNextItem;
         pInsertAfter->pNextItem = pNewItem;
         }
      pNewItem->pPrevItem = pInsertAfter;
      if( pNewItem->pNextItem )
         pNewItem->pNextItem->pPrevItem = pNewItem;
      pNewItem->pParentItem = pParentItem;
      pNewItem->pFirstChild = NULL;
      pNewItem->id.cChildren = lptvi->cChildren;
      pNewItem->cTrueChildren = 0;
      pNewItem->id.state = lptvi->state;
      if( pNewItem->id.state & TVIS_EXPANDED )
         pNewItem->id.state |= TVIS_EXPANDEDONCE;
      pNewItem->id.iImage = lptvi->iImage;
      pNewItem->id.iSelectedImage = lptvi->iSelectedImage;
      pNewItem->id.lParam = lptvi->lParam;
      if( lptvi->pszText != LPSTR_TEXTCALLBACK )
         {
         static LPSTR lpszText;

         INITIALISE_PTR(lpszText);
         pNewItem->id.pszText = lpszText;
         if( fNewMemory( &lpszText, lstrlen( lptvi->pszText ) + 1 ) )
            {
            pNewItem->id.pszText = lpszText;
            lstrcpy( pNewItem->id.pszText, lptvi->pszText );
            }
         }
      else
         pNewItem->id.pszText = LPSTR_TEXTCALLBACK;
      SetRectEmpty( &pNewItem->id.rcItem );

      /* Set the indentation level.
       */
      if( !pParentItem )
         {
         pNewItem->id.nLevel = 0;
         ++pTreeview->cExpandedItems;
         }
      else
         {
         pNewItem->id.nLevel = pParentItem->id.nLevel + 1;
         ++pParentItem->cTrueChildren;
         ++pParentItem->id.cChildren;
         if( pParentItem->id.state & TVIS_EXPANDED )
            ++pTreeview->cExpandedItems;
         }

      /* If this is the first item, set pFirstVisible.
       */
      if( pTreeview->cItems++ == 0 )
         pTreeview->pFirstVisible = pNewItem;
      }
   else
      Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_OUTOFMEMORY, &nmhdr );
   return( pNewItem );
}

/* This function deletes all items from the treeview control. For
 * each root item to be deleted, it calls DeleteItem which handles the
 * deletion of sub-items.
 */
static void FASTCALL DeleteAllItems( HWND hwnd, TREEVIEW FAR * pTreeview )
{
   ITEM FAR * pItem;
   int cDeleted;

   pItem = pTreeview->pRootItem;
   cDeleted = 0;
   while( pItem )
      {
      ITEM FAR * pNextItem;

      pNextItem = pItem->pNextItem;
      cDeleted += DeleteItem( hwnd, pTreeview, pItem );
      pItem = pNextItem;
      }
   ASSERT( NULL == pTreeview->pRootItem );
   ASSERT( NULL == pTreeview->pSelectedItem );
   ASSERT( NULL == pTreeview->pFirstVisible );
   ASSERT( 0 == pTreeview->cItems - cDeleted );
   pTreeview->cExpandedItems = 0;
   pTreeview->cItems = 0;
}

/* This function deletes an item. It starts by calling DeleteItem on
 * all child items, then it removes itself from the item list.
 *
 * Note: I've declared this FASTCALL to try and save stack space. I'm
 * not sure if this is the right thing to do - we'll have to do an
 * assembler listing of the optimised code and see.
 */
static int FASTCALL DeleteItem( HWND hwnd, TREEVIEW FAR * pTreeview, ITEM FAR * pItem )
{
   NM_TREEVIEW nmtv;
   ITEM FAR * pFirstChild;
   int cDeleted;

   ASSERT( pItem != NULL );

   /* If this item has any children, delete them.
    */
   pFirstChild = pItem->pFirstChild;
   cDeleted = 0;
   while( pFirstChild )
      {
      ITEM FAR * pNextChildItem;

      pNextChildItem = pFirstChild->pNextItem;
      if( pFirstChild->id.state & TVIS_EXPANDED )
         pTreeview->cExpandedItems -= pFirstChild->cTrueChildren;
      cDeleted += DeleteItem( hwnd, pTreeview, pFirstChild );
      if( pItem->id.state & TVIS_EXPANDED )
         --pTreeview->cExpandedItems;
      ASSERT( pNextChildItem == pItem->pFirstChild );
      pFirstChild = pNextChildItem;
      }


   /* Save information about this item.
    */
   nmtv.itemOld.hItem = (HTREEITEM)pItem;
   nmtv.itemOld.lParam = pItem->id.lParam;

   /* If this is the visible item, make the next sibling visible if
    * there is one, the previous sibiling otherwise.
    */
   if( pItem == pTreeview->pFirstVisible )
      pTreeview->pFirstVisible = pItem->pNextItem ? pItem->pNextItem : pItem->pPrevItem;
   if( pItem == pTreeview->pSelectedItem )
      pTreeview->pSelectedItem = NULL;
   if( pItem == pTreeview->pTooltipItem )
      pTreeview->pTooltipItem = NULL;

   /* The next line was changed by me so that the root item did not 
    * go NULL if there were still items after that. Previously,
    * the root item was set to NULL, so that if an item were
    * to be inserted before any items left, the new item would
    * be have it's next pointer pointing to the root (in this
    * case NULL) and so it would never form part of the list
    * that would get displayed. Hence the change made here
    * YH 09/04/96
    */
   if( pItem == pTreeview->pRootItem )
      pTreeview->pRootItem = pItem->pNextItem;

   /* Unlink this item.
    */
   if( pItem->pNextItem )
      pItem->pNextItem->pPrevItem = pItem->pPrevItem;
   if( pItem->pPrevItem )
      pItem->pPrevItem->pNextItem = pItem->pNextItem;
   else if( pItem->pParentItem )
      pItem->pParentItem->pFirstChild = pItem->pNextItem;

   /* Reduce the count of parent items.
    */
   if( pItem->pParentItem )
      {
      --pItem->pParentItem->id.cChildren;
      --pItem->pParentItem->cTrueChildren;
      if( 0 == pItem->pParentItem->id.cChildren )
         {
         pItem->pParentItem->id.state &= ~TVIS_EXPANDED;
         InvalidateItem( hwnd, pTreeview, pItem->pParentItem, TRUE );
         }
      }

   /* Delete the item title
    */
   if( pItem->id.pszText )
      FreeMemory(&pItem->id.pszText); // 20060325 - 2.56.2049.20

   /* Delete the item.
    */
   FreeMemory( &pItem );

   /* Send a notification message to the parent.
    */
   Amctl_SendNotify( hwnd, GetParent( hwnd ), TVN_DELETEITEM, (LPNMHDR)&nmtv );
   return( cDeleted + 1 );
}

static BOOL FASTCALL FindString( HWND hwnd, TV_SEARCH FAR * lptvs )
{
   while( lptvs->hItem )
      {
      ITEM FAR * pItem;
      LPSTR lpText;

      /* Get the string for this item (handling LPSTR_TEXTCALLBACK as appropriate)
       * and look for lpstr in this string.
       */
      pItem = (ITEM FAR *)lptvs->hItem;
      lpText = GetItemText( hwnd, pItem );
      if( _fstrstr( lpText, lptvs->pszText ) != NULL )
         break;

      /* If this item has any children but the children have not been
       * supplied, callback to get the children.
       */
      if( pItem->id.cChildren && NULL == pItem->pFirstChild && !(pItem->id.state & TVIS_EXPANDEDONCE) )
         {
         NM_TREEVIEW nmtv;
   
         nmtv.action = TVE_EXPAND;
         nmtv.itemNew.hItem = (HTREEITEM)pItem;
         nmtv.itemNew.lParam = pItem->id.lParam;
         nmtv.itemNew.state = pItem->id.state;
         nmtv.itemNew.pszText = pItem->id.pszText;
         GetCursorPos( &nmtv.ptDrag );
         if( Amctl_SendNotify( GetParent( hwnd ), hwnd, TVN_ITEMEXPANDING, (LPNMHDR)&nmtv ) == FALSE )
            {
            pItem->id.state |= TVIS_EXPANDEDONCE;
            Amctl_SendNotify( GetParent( hwnd ), hwnd, TVN_ITEMEXPANDED, (LPNMHDR)&nmtv );
            }
         }

      /* If this item has any children, recurse to find though the
       * child items.
       */
      if( pItem->pFirstChild )
         {
         lptvs->hItem = (HTREEITEM)pItem->pFirstChild;
         continue;
         }

      /* Jump to the next item.
       */
      if( NULL == pItem->pNextItem && pItem->pParentItem )
         while( NULL == pItem->pNextItem && pItem->pParentItem )
            {
            lptvs->hItem = (HTREEITEM)( pItem->pParentItem );
            pItem = (ITEM FAR *)lptvs->hItem;
            }
      lptvs->hItem = (HTREEITEM)( pItem->pNextItem );
      }
   return( lptvs->hItem != 0 );
}

static ITEM FAR * FASTCALL ISearch( HWND hwnd, TREEVIEW FAR * pTreeview, ITEM FAR * pItem )
{
   while( pItem )
      {
      LPSTR lpText;

      /* Get the string for this item (handling LPSTR_TEXTCALLBACK as appropriate)
       * and look for lpstr in this string.
       */
      lpText = GetItemText( hwnd, pItem );
      if( _strnicmp( lpText, pTreeview->szScan, strlen(pTreeview->szScan) ) == 0 )
         break;

      /* If this item has any children but the children have not been
       * supplied, callback to get the children.
       */
      if( pItem->id.cChildren && NULL == pItem->pFirstChild && !(pItem->id.state & TVIS_EXPANDEDONCE) )
         {
         NM_TREEVIEW nmtv;
   
         nmtv.action = TVE_EXPAND;
         nmtv.itemNew.hItem = (HTREEITEM)pItem;
         nmtv.itemNew.lParam = pItem->id.lParam;
         nmtv.itemNew.state = pItem->id.state;
         nmtv.itemNew.pszText = pItem->id.pszText;
         GetCursorPos( &nmtv.ptDrag );
         if( Amctl_SendNotify( GetParent( hwnd ), hwnd, TVN_ITEMEXPANDING, (LPNMHDR)&nmtv ) == FALSE )
            {
            pItem->id.state |= TVIS_EXPANDEDONCE;
            Amctl_SendNotify( GetParent( hwnd ), hwnd, TVN_ITEMEXPANDED, (LPNMHDR)&nmtv );
            }
         }

      /* If this item has any children, recurse to find though the
       * child items.
       */
      if( pItem->pFirstChild )
         {
         pItem = pItem->pFirstChild;
         continue;
         }

      /* Jump to the next item.
       */
      if( NULL == pItem->pNextItem && pItem->pParentItem )
         while( NULL == pItem->pNextItem && pItem->pParentItem )
            pItem = pItem->pParentItem;
      pItem = pItem->pNextItem;
      }
   return( pItem );
}

/* Sets the scroll range based on the size of the window and the total
 * number of expaanded items.
 */
static void FASTCALL ScrollRange( HWND hwnd, TREEVIEW FAR * pTreeView )
{
   int cVisibleItems;
   RECT rectClient;

   GetClientRect( hwnd, &rectClient );
   cVisibleItems = ( rectClient.bottom - rectClient.top ) / pTreeView->dyLine;

   /* We have separete Win32 code because we can set the page size for proportional
    * scroll bars that way.
    */
#ifdef WIN32
   if( fNewShell )
      {
      SCROLLINFO scrollInfo;

      scrollInfo.cbSize = sizeof( SCROLLINFO );
      scrollInfo.fMask = SIF_PAGE | SIF_RANGE;
      scrollInfo.nPage = cVisibleItems;

      /* If there are a total of X items that can be displayed
       * then the range is 0..X-1, that's what's recomendded in
       * the docs.
       */
      scrollInfo.nMin = 0;
      scrollInfo.nMax = pTreeView->cExpandedItems - 1;
      SetScrollInfo( hwnd, SB_VERT, &scrollInfo, TRUE );
      }
   else
#endif
      SetScrollRange( hwnd, SB_VERT, 0, max( pTreeView->cExpandedItems - cVisibleItems, 0 ), TRUE );
   pTreeView->cVisibleItems = cVisibleItems;
}

/* Given an item, this will find the sum of its expanded children, and its children's
 * children, and its children's children's children's children and all 
 * all descendents have been accounted for.
 * YH 12/06/96 10:50
 */
static int FASTCALL NumberExpandedDescendents( ITEM FAR * pItem )
{
   int cChildren;
   ITEM FAR * pItemChild;

   if( pItem->id.state & TVIS_EXPANDED )
      {
      cChildren = pItem->cTrueChildren;
      pItemChild = pItem->pFirstChild;
      while( pItemChild )
         {
         cChildren += NumberExpandedDescendents( pItemChild );
         pItemChild = pItemChild->pNextItem;
         }
      }
   else
      cChildren = 0;
   return( cChildren );
}


/* Given any starting point in the tree of items this function will find the number of
 * expanded (displayable) items it searched through before finding the item requested.
 * It will NOT go up the tree. It is a recursive function and is inteded to be used
 * with the parameter pItemStart == pTreeview->pRootItem
 */
static int FASTCALL ItemFindIndex( ITEM FAR * pItemFind, ITEM FAR * pItemStart, BOOL * lpfFound )
{
   ITEM FAR * pItemNext;
   int nTraversed;

   *lpfFound = FALSE;
   nTraversed = 0;
   pItemNext = pItemStart;
   while( pItemNext && !*lpfFound )
      {
      if( pItemNext == pItemFind )
         *lpfFound = TRUE;
      else
         {
         if( pItemNext->pFirstChild && ( pItemNext->id.state & TVIS_EXPANDED ) )
            nTraversed += ItemFindIndex( pItemFind, pItemNext->pFirstChild, lpfFound ) + 1;
         else
            nTraversed++;
         pItemNext = pItemNext->pNextItem;
         }
      }
   return( nTraversed );
}
