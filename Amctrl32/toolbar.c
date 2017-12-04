/* TOOLBAR.C - Implements the toolbar control
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
#include <memory.h>
#include <string.h>
#include "amctrls.h"
#include "amctrli.h"
#include "amctrlrc.h"

#define  THIS_FILE   __FILE__

#define  GWW_EXTRA            4
#define  GetBlock(h)          ((TOOLBAR FAR *)GetWindowLong((h),0))
#define  PutBlock(h,b)        (SetWindowLong((h),0,(LONG)(b)))

#ifndef COLOR_3DLIGHT
#define COLOR_3DLIGHT           22
#endif

#define  SEPARATOR_WIDTH      10
#define  RGB_TRANSPARENT      RGB(192,192,192)

#define  DEF_DXBUTTON         24
#define  DEF_DYBUTTON         22
#define  DEF_DXBITMAP         16
#define  DEF_DYBITMAP         15

#define  DYPADDING            3
#define  DXPADDING            3

#define  ROP_PSDPxax          0x00B8074A

typedef struct tagBITMAPLIST {
   struct tagBITMAPLIST FAR * pNextBitmapList;  /* Pointer to the next bitmap list */
   int cBitmaps;                                /* Number of bitmaps in this list */
   HBITMAP hbmp;                                /* Handle of the bitmap list */
} BITMAPLIST;

typedef BITMAPLIST FAR * LPBITMAPLIST;

typedef struct tagSTRINGLIST {
   struct tagSTRINGLIST FAR * pNextStringList;  /* Pointer to the next string list */
   int cStrings;                                /* Number of strings in this list */
   LPSTR pStr;                                  /* Pointer to the string list */
} STRINGLIST;

typedef struct tagBUTTON {
   struct tagBUTTON FAR * pNextButton;    /* Pointer to next button */
   TBDRAWBUTTON tbdb;                     /* Button descriptor */
} BUTTON;

typedef struct tagTOOLBAR {
   int cbButtonStructSize;       /* Size of TBBUTTON structure */
   BITMAPLIST FAR * pBitmapList; /* Pointer to bitmap list */
   STRINGLIST FAR * pStringList; /* Pointer to string list */
   BUTTON FAR * pFirstButton;    /* Pointer to buttons list */
   BUTTON FAR * pSelectedButton; /* Selected button */
   int nSelectedButton;          /* Index of selected button */
   int cButtons;                 /* Number of buttons on the toolbar */
   int dxButton;                 /* Width of each button */
   int dyButton;                 /* Height of each button */
   int dxBitmap;                 /* Width of each button bitmap */
   int dyBitmap;                 /* Height of each button bitmap */
   int dxSize;                   /* Width of toolbar, in pixels */
   int dySize;                   /* Height of toolbar, in pixels */
   int dyString;                 /* Height of string, in pixels */
   int dxMaxStrWidth;            /* Maximum string width */
   int dyLabelLine;              /* Height of label line */
   int dyMaxLabelRows;           /* Maximum rows in label */
   BOOL fEditMode;               /* TRUE if toolbar is in edit mode */
   HFONT hOwnedFont;             /* Handle of owned font */
   HFONT hFont;                  /* Toolbar string font */
   HWND hwndParent;              /* Handle of parent window */
   HWND hwndTooltip;             /* Handle of tooltip window */
   int cRows;                    /* Number of rows in toolbar */
} TOOLBAR;

static HBITMAP hbmpDither;                /* Dither bitmap for dither brush */
static RECT rcDrag;                       /* Toolbar dragging rectangle */
static POINT ptStart;                     /* Coordinates of start of drag */
static int nDragButton;                   /* Index of button being dragged */
static HCURSOR hcurToolbarDrag;           /* Handle of button drag cursor */
static HCURSOR hcurNoDrop;                /* Handle of cursor for when user is over a no-drop area */
static HCURSOR hcurCur;                   /* The current cursor */
static HCURSOR hcurOld;                   /* The saved cursor */
static BOOL fDragging = FALSE;            /* TRUE if we're dragging a button */
static POINT ptLast;                      /* Current coordinates of the cursor */
static HDC hdcDrag;                       /* HDC for the drag button bitmap */
static HDC hdcSaved;                      /* HDC for the saved screen background bitmap */
static HBITMAP hbmpDrag;                  /* The drag button bitmap */
static HBITMAP hbmpOld;                   /* The old bitmap selected in hdcDrag */
static HBITMAP hbmpOldSaved;              /* The old bitmap selected in hdcSaved */
static HBITMAP hbmpSaved;                 /* The saved screen bitmap */
static int cxButton;                      /* Button width */
static int cyButton;                      /* Button height */
static BOOL fCaptured = FALSE;            /* TRUE if we've captured mouse */
static BOOL fDoneBitmap;                  /* TRUE if the screen background needs redrawing */
static BOOL fIgnoreNextMouseMove = FALSE; /* TRUE if next WM_MOUSEMOVE is ignored */

static BOOL FASTCALL InsertButton( HWND, TOOLBAR FAR *, int, LPTBBUTTON );
static BOOL FASTCALL DeleteButton( HWND, TOOLBAR FAR *, int );
static BUTTON FAR * FASTCALL GetButton( TOOLBAR FAR *, int );
static BUTTON FAR * FASTCALL GetButtonByCommand( TOOLBAR FAR *, int );
static void FASTCALL RecalcButtonSize( HWND, TOOLBAR FAR * );
static BOOL FASTCALL RecalcToolbarSize( HWND, TOOLBAR FAR *, RECT *, BOOL );
static BOOL FASTCALL RecalcHorzToolbarSize( HWND, TOOLBAR FAR *, RECT *, BOOL );
static BOOL FASTCALL RecalcVertToolbarSize( HWND, TOOLBAR FAR *, RECT *, BOOL );
static void FASTCALL CheckButton( HWND, BUTTON FAR *, BOOL );
static void FASTCALL CheckGroupButton( HWND, BUTTON FAR *, int );
static void FASTCALL GroupButton( HWND, BUTTON FAR *, int );
static void FASTCALL PressButton( HWND, BUTTON FAR *, BOOL );
static void FASTCALL DrawToolbarButton( HWND, HDC, TOOLBAR FAR *, TBBUTTON FAR *, RECT *, BOOL );
static void FASTCALL CreateToolbarFont( TOOLBAR FAR * );
static LPSTR FASTCALL GetButtonString( HWND, TOOLBAR FAR *, TBBUTTON FAR *, char * );
static void FASTCALL AskParentForWorkspaceRect( HWND, LPRECT );
static void FASTCALL UpdateLabelFontHeight( HWND, TOOLBAR FAR * );

LRESULT EXPORT CALLBACK ToolbarWndFn( HWND, UINT, WPARAM, LPARAM );

/* This function registers the toolbar window class.
 */
BOOL FASTCALL RegisterToolbarClass( HINSTANCE hInst )
{
   WNDCLASS wc;

   wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
   wc.hIcon          = NULL;
   wc.lpszMenuName   = NULL;
   wc.lpszClassName  = TOOLBARCLASSNAME;
   wc.hbrBackground  = (HBRUSH)( COLOR_BTNFACE+1 );
   wc.hInstance      = hInst;
   wc.style          = CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS;
   wc.lpfnWndProc    = ToolbarWndFn;
   wc.cbClsExtra     = 0;
   wc.cbWndExtra     = GWW_EXTRA;
   return( RegisterClass( &wc ) );
}

WINCOMMCTRLAPI HWND WINAPI EXPORT Amctl_CreateToolbar( HWND hwnd, DWORD ws, UINT wID, int nBitmaps, HINSTANCE hBMInst,
   UINT wBMID, LPCTBBUTTON lpButtons, int iNumButtons, int dxButton, int dyButton, int dxBitmap,
   int dyBitmap, UINT uStructSize )
{
   HWND hwndToolbar;

   /* Create the toolbar using the default styles and settings.
    */
   hwndToolbar = CreateWindow( TOOLBARCLASSNAME,
                              "",
                              ws|ACTL_CCS_TOP|WS_CHILD,
                              0, 0, 0, 0,
                              hwnd,
                              (HMENU)wID,
                              GetWindowInstance( hwnd ),
                              NULL );
   if( hwndToolbar )
      {
      static TBADDBITMAP tbabmp;

      SendMessage( hwndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)uStructSize, 0L );
      SendMessage( hwndToolbar, TB_SETBITMAPSIZE, 0, MAKELONG( dxBitmap, dyBitmap ) );
      SendMessage( hwndToolbar, TB_SETBUTTONSIZE, 0, MAKELONG( dxButton, dyButton ) );

      tbabmp.hInst = hBMInst;
      tbabmp.nID = wBMID;
      if( SendMessage( hwndToolbar, TB_ADDBITMAP, nBitmaps, (LPARAM)(LPSTR)&tbabmp ) == -1 )
         {
         DestroyWindow( hwndToolbar );
         hwndToolbar = NULL;
         }
      if( !SendMessage( hwndToolbar, TB_ADDBUTTONS, iNumButtons, (LPARAM)lpButtons ) )
         {
         DestroyWindow( hwndToolbar );
         hwndToolbar = NULL;
         }
      }
   return( hwndToolbar );
}

LRESULT EXPORT CALLBACK ToolbarWndFn( HWND hwnd,  UINT message, WPARAM wParam, LPARAM lParam )
{
   TOOLBAR FAR * pToolbar;
   BUTTON FAR * pButton;
   BITMAPLIST FAR * pBitmapList;
   STRINGLIST FAR * pStringList;

#ifdef _DEBUG
   /* Help us to catch NULL pointers...
    */
   pToolbar = NULL;
   pButton = NULL;
#endif
   switch( message )
      {
      case WM_CREATE: {
         static TOOLBAR FAR * pNewToolbar;

         INITIALISE_PTR(pNewToolbar);
         if( fNewMemory( &pNewToolbar, sizeof( TOOLBAR ) ) )
            {
            pNewToolbar->cButtons = 0;
            pNewToolbar->dxButton = DEF_DXBUTTON;
            pNewToolbar->dyButton = DEF_DYBUTTON;
            pNewToolbar->dxBitmap = DEF_DXBITMAP;
            pNewToolbar->dyBitmap = DEF_DYBITMAP;
            pNewToolbar->dxMaxStrWidth = DEF_DXBUTTON - 6;
            pNewToolbar->dxSize = 0;
            pNewToolbar->dySize = 0;
            pNewToolbar->pBitmapList = NULL;
            pNewToolbar->pStringList = NULL;
            pNewToolbar->pFirstButton = NULL;
            pNewToolbar->pSelectedButton = NULL;
            pNewToolbar->cbButtonStructSize = sizeof( TBBUTTON );
            pNewToolbar->nSelectedButton = -1;
            pNewToolbar->hwndParent = GetParent( hwnd );
            pNewToolbar->cRows = 1;
            pNewToolbar->dyMaxLabelRows = 2;
            pNewToolbar->fEditMode = FALSE;
            CreateToolbarFont( pNewToolbar );
            UpdateLabelFontHeight( hwnd, pNewToolbar );
            PutBlock( hwnd, pNewToolbar );

            /* Tooltips?
             */
            if( GetWindowStyle( hwnd ) & TBSTYLE_TOOLTIPS )
               pNewToolbar->hwndTooltip = CreateWindow( TOOLTIPS_CLASS, "",
                                                     0,
                                                     0, 0, 0, 0,
                                                     hwnd,
                                                     0,
                                                     hLibInst, 0L );
            else
               pNewToolbar->hwndTooltip = NULL;
            hbmpDither = CreateDitherBitmap();
            nDragButton = -1;
            fDragging = FALSE;
            }
         return( pNewToolbar ? 0 : -1 );
         }

      case WM_GETFONT:
         pToolbar = GetBlock( hwnd );
         return( (LRESULT)(LPSTR)pToolbar->hFont );

      case WM_SETFONT: {
         RECT rc;

         pToolbar = GetBlock( hwnd );
         pToolbar->hFont = (HFONT)wParam;
         UpdateLabelFontHeight( hwnd, pToolbar );
         RecalcButtonSize( hwnd, pToolbar );
         AskParentForWorkspaceRect( hwnd, &rc );
         RecalcToolbarSize( hwnd, pToolbar, &rc, TRUE );
         if( LOWORD( lParam ) && pToolbar->pStringList )
            {
            InvalidateRect( hwnd, NULL, TRUE );
            UpdateWindow( hwnd );
            }
         break;
         }

      case WM_DESTROY:
         /* Tooltips will get destroyed automatically before
          * this is called.
          */
         pToolbar = GetBlock( hwnd );
         pButton = pToolbar->pFirstButton;
         while( pButton )
            {
            BUTTON FAR * pNextButton;

            pNextButton = pButton->pNextButton;
            FreeMemory( &pButton );
            pButton = pNextButton;
            }
         pBitmapList = pToolbar->pBitmapList;
         while( pBitmapList )
            {
            BITMAPLIST FAR * pNextBitmapList;

            pNextBitmapList = pBitmapList->pNextBitmapList;
            DeleteBitmap( pBitmapList->hbmp );
            FreeMemory( &pBitmapList );
            pBitmapList = pNextBitmapList;
            }
         pStringList = pToolbar->pStringList;
         while( pStringList )
            {
            STRINGLIST FAR * pNextStringList;

            pNextStringList = pStringList->pNextStringList;
            FreeMemory( &pStringList->pStr );
            FreeMemory( &pStringList );
            pStringList = pNextStringList;
            }
         DeleteBitmap( hbmpDither );
         DeleteFont( pToolbar->hOwnedFont );
         FreeMemory( &pToolbar );
         return( 0L );

      case TB_ADDBITMAP: {
         LPTBADDBITMAP lptbab;
         static BITMAPLIST FAR * pNewBmpList;
         int cNewBitmaps;
         HBITMAP hbmpPic;
         NMHDR nmhdr;
         int cIndex;
/*       PICCTRLSTRUCT pcs;
         HDC hdc;
         HDC hdcSrc;

         hdc = GetDC( hwnd );
*/       lptbab = (LPTBADDBITMAP)lParam;
         cNewBitmaps = (int)wParam;
         pToolbar = GetBlock( hwnd );

         /* Create the BITMAPLIST structure first.
          */
         INITIALISE_PTR(pNewBmpList);
         if( fNewMemory( &pNewBmpList, sizeof( BITMAPLIST ) ) )
            {
            LPBITMAPLIST FAR * ppBitmapList;

            /* Get the bitmap. If lptbab->hInst is NULL, then lptbab->nID
             * must be the handle of the bitmap, otherwise the bitmap is
             * loaded from the instance resource table.
             */
            if( NULL == lptbab->hInst )
               hbmpPic = (HBITMAP)lptbab->nID;
            else
            {
/*             if( ( GetWindowStyle( hwnd ) & TBSTYLE_NEWBUTTONS ) == TBSTYLE_NEWBUTTONS )
               {

               Amctl_LoadDIBBitmap( lptbab->hInst, (LPSTR)MAKEINTRESOURCE( lptbab->nID ), &pcs );
               hdcSrc = CreateCompatibleDC( hdc );
               SelectPalette( hdc, pcs.hPalette, FALSE );
               RealizePalette( hdc );
               SelectPalette( hdcSrc, pcs.hPalette, FALSE );
               RealizePalette( hdcSrc );
               hbmpPic = pcs.hBitmap;
               }
               else
*/                hbmpPic = LoadBitmap( lptbab->hInst, MAKEINTRESOURCE( lptbab->nID ) );
            }

            /* Scan for the end of the bitmap list
             */
            ppBitmapList = &pToolbar->pBitmapList;
            cIndex = 0;
            while( *ppBitmapList )
               {
               cIndex += (*ppBitmapList)->cBitmaps;
               ppBitmapList = &(*ppBitmapList)->pNextBitmapList;
               }

            /* Link in and fill out the BITMAPLIST structure.
             */
            *ppBitmapList = pNewBmpList;
            pNewBmpList->pNextBitmapList = NULL;
            pNewBmpList->cBitmaps = cNewBitmaps;
            pNewBmpList->hbmp = hbmpPic;
            }
         else
            {
            Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_OUTOFMEMORY, &nmhdr );
            cIndex = -1;
            }
         return( cIndex );
         }

      case TB_ADDSTRING: {
         static STRINGLIST FAR * pNewStringList;
         NMHDR nmhdr;
         int cIndex;

         /* Get the toolbar handle.
          */
         pToolbar = GetBlock( hwnd );

         /* Create the STRINGLIST structure first.
          */
         INITIALISE_PTR(pNewStringList);
         if( fNewMemory( &pNewStringList, sizeof( STRINGLIST ) ) )
            {
            STRINGLIST FAR * FAR * ppStringList;
            static char sz[ 60 ];
            static LPSTR pStr;
            static LPSTR lpStr;
            int cNewStrings;
            int c;

            INITIALISE_PTR(pStr);

            /* Get the string. If wParam is NULL, then lParam must be a
             * 32-bit pointer to the string, otherwise wParam must be a
             * module instance handle and the low word of lParam must be
             * the ID of the string.
             */
            if( 0 == wParam )
               {
               ASSERT( lParam != 0 );
               lpStr = (LPSTR)lParam;
               }
            else
               {
               LoadString( (HINSTANCE)wParam, LOWORD( lParam ), sz, sizeof( sz ) );
               lpStr = sz;
               }

            /* Count how many strings there are.
             */
            cNewStrings = 0;
            for( c = 0; lpStr[ c ]; ++c )
               {
               ++cNewStrings;
               c += lstrlen( &lpStr[ c ] );
               }

            /* Allocate a buffer for the strings.
             */
            if( fNewMemory( &pStr, c + 1 ) )
               {
               _fmemcpy( pStr, lpStr, c + 1 );

               /* Scan for the end of the bitmap list
                */
               ppStringList = &pToolbar->pStringList;
               cIndex = 0;
               while( *ppStringList )
                  {
                  cIndex += (*ppStringList)->cStrings;
                  ppStringList = &(*ppStringList)->pNextStringList;
                  }
   
               /* Link in and fill out the STRINGLIST structure.
                */
               *ppStringList = pNewStringList;
               pNewStringList->pNextStringList = NULL;
               pNewStringList->cStrings = cNewStrings;
               pNewStringList->pStr = pStr;
               RecalcButtonSize( hwnd, pToolbar );
               }
            else
               {
               FreeMemory( &pNewStringList );
               Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_OUTOFMEMORY, &nmhdr );
               cIndex = -1;
               }
            }
         else
            {
            Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_OUTOFMEMORY, &nmhdr );
            cIndex = -1;
            }
         return( cIndex );
         }

      case TB_ADDBUTTONS: {
         LPTBBUTTON lpButtons;
         register int c;
         RECT rc;

         pToolbar = GetBlock( hwnd );
         lpButtons = (LPTBBUTTON)lParam;
         for( c = 0; c < (int)wParam; ++c )
            if( !InsertButton( hwnd, pToolbar, pToolbar->cButtons - 1, &lpButtons[ c ] ) )
               return( FALSE );
         AskParentForWorkspaceRect( hwnd, &rc );
         RecalcToolbarSize( hwnd, pToolbar, &rc, TRUE );
         return( TRUE );
         }

      case TB_AUTOSIZE: {
         RECT rc;

         pToolbar = GetBlock( hwnd );
         SetRect( &rc, 0, 0, LOWORD( lParam ), HIWORD( lParam ) );
         RecalcToolbarSize( hwnd, pToolbar, &rc, FALSE );
         return( MAKELONG( pToolbar->dxSize, pToolbar->dySize ) );
         }

      case TB_BUTTONCOUNT:
         pToolbar = GetBlock( hwnd );
         return( pToolbar->cButtons );       

      case TB_BUTTONSTRUCTSIZE:
         pToolbar = GetBlock( hwnd );
         pToolbar->cbButtonStructSize = (int)wParam;
         return( 0L );        

      case TB_CHANGEBITMAP: {
         int index;

         pToolbar = GetBlock( hwnd );
         if( ( index = (int)SendMessage( hwnd, TB_COMMANDTOINDEX, wParam, 0L ) ) != -1 )
            {
            ASSERT( index >= 0 && index < pToolbar->cButtons );
            pButton = GetButton( pToolbar, index );
            ASSERT( pButton != NULL );
            pButton->tbdb.tb.iBitmap = LOWORD( lParam );
            pButton->tbdb.tb.fsStyle |= TBSTYLE_CALLBACK;
            InvalidateRect( hwnd, &pButton->tbdb.rc, TRUE );
            return( TRUE );
            }
         return( FALSE );
         }

      case TB_CHECKBUTTON:
         pToolbar = GetBlock( hwnd );
         if( pButton = GetButtonByCommand( pToolbar, (int)wParam ) )
            {
            CheckButton( hwnd, pButton, LOWORD( lParam ) );
            return( TRUE );
            }
         return( FALSE );

      case TB_COMMANDTOINDEX: {
         register int c;

         pToolbar = GetBlock( hwnd );
         pButton = pToolbar->pFirstButton;
         for( c = 0; c < pToolbar->cButtons; ++c )
            {
            if( pButton->tbdb.tb.idCommand == (int)wParam )
               return( c );
            pButton = pButton->pNextButton;
            }
         return( -1 );
         }

      case TB_DELETEBUTTON:
         /* Delete the button specified by wParam and update the
          * button positions. We don't redraw the toolbar as we may
          * be getting multiple TB_DELETEBUTTONS in a row, but we do
          * recompute the button sizes to ensure that a subsequent
          * TB_GETITEMRECT returns correct data.
          */
         pToolbar = GetBlock( hwnd );
         if( DeleteButton( hwnd, pToolbar, (int)wParam ) )
            {
            RECT rc;

            AskParentForWorkspaceRect( hwnd, &rc );
            RecalcToolbarSize( hwnd, pToolbar, &rc, TRUE );
            return( TRUE );
            }
         return( FALSE );

      case TB_DRAWBUTTON: {
         HDC hdc;
         HBRUSH hbr;
         RECT rc;
         TBDRAWBUTTON FAR * lptbdb;
         COLORREF crBack;
         HPEN hpenWhite;
         HPEN hpenDkGrey;
         HPEN hpenBlack;
         HPEN hpenBrGrey;

         /* This message provides a way for an application to draw a
          * toolbar button independently of the toolbar (ie. when filling
          * a customisation dialog or creating a bitmap for dragging).
          */
         pToolbar = GetBlock( hwnd );
         hpenWhite = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNHIGHLIGHT ) );
         hpenDkGrey = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNSHADOW ) );
         hpenBrGrey = CreatePen( PS_SOLID, 1, GetSysColor( fNewShell ? COLOR_3DLIGHT : COLOR_BTNFACE ) );
         hpenBlack = GetStockPen( BLACK_PEN );

         /* Make sure the background colour is correct.
          */
         hdc = (HDC)wParam;
         crBack = SetBkColor( hdc, GetSysColor( COLOR_BTNFACE ) );

         /* Draw the button and it's contents.
          */
         lptbdb = (TBDRAWBUTTON FAR *)lParam;
         hbr = CreateSolidBrush( GetSysColor( COLOR_BTNFACE ) );
         rc = lptbdb->rc;
         DrawButton( hdc, &rc, hpenBlack, hpenDkGrey, hpenWhite, hpenBrGrey, hbr, FALSE );
         InflateRect( &rc, -2, -2 );
         DrawToolbarButton( hwnd, hdc, pToolbar, &lptbdb->tb, &rc, FALSE );

         /* Clean up.
          */
         SetBkColor( hdc, crBack );
         DeletePen( hpenWhite );
         DeletePen( hpenDkGrey );
         DeletePen( hpenBrGrey );
         DeleteBrush( hbr );
         return( TRUE );
         }

      case TB_EDITMODE: {
         BOOL fOldEditMode;

         pToolbar = GetBlock( hwnd );
         fOldEditMode = pToolbar->fEditMode;
         pToolbar->fEditMode = (BOOL)wParam;
         return( fOldEditMode );
         }

      case TB_ENABLEBUTTON:
         pToolbar = GetBlock( hwnd );
         if( pButton = GetButtonByCommand( pToolbar, (int)wParam ) )
            {
            if( LOWORD( lParam ) && !( pButton->tbdb.tb.fsState & TBSTATE_ENABLED ) )
               {
               pButton->tbdb.tb.fsState |= TBSTATE_ENABLED;
               InvalidateRect( hwnd, &pButton->tbdb.rc, TRUE );
               UpdateWindow( hwnd );
               }
            else if( !LOWORD( lParam ) && pButton->tbdb.tb.fsState & TBSTATE_ENABLED )
               {
               pButton->tbdb.tb.fsState &= ~TBSTATE_ENABLED;
               InvalidateRect( hwnd, &pButton->tbdb.rc, TRUE );
               UpdateWindow( hwnd );
               }
            return( TRUE );
            }
         return( FALSE );

      case TB_GETBITMAP:
         pToolbar = GetBlock( hwnd );
         if( pButton = GetButtonByCommand( pToolbar, (int)wParam ) )
            return( pButton->tbdb.tb.iBitmap );
         return( -1 );

      case TB_GETBITMAPFLAGS: {
         BOOL fLarge;
         HDC hdc;

         hdc = GetDC( hwnd );
         fLarge = GetDeviceCaps( hdc, LOGPIXELSX ) >= 120;
         ReleaseDC( hwnd, hdc );
         return( fLarge ? TBBF_LARGE : 0 );
         }

      case TB_GETBITMAPSIZE:
         pToolbar = GetBlock( hwnd );
         if( 0L != lParam )
            {
            ((SIZE FAR *)lParam)->cx = pToolbar->dxBitmap;
            ((SIZE FAR *)lParam)->cy = pToolbar->dyBitmap;
            }
         return( MAKELONG( (short)pToolbar->dxBitmap, (short)pToolbar->dyBitmap ) );

      case TB_GETBUTTON:
         pToolbar = GetBlock( hwnd );
         if( pButton = GetButton( pToolbar, (int)wParam ) )
            {
            LPTBBUTTON lptb;

            lptb = (LPTBBUTTON)lParam;
            *lptb = pButton->tbdb.tb;
            return( TRUE );
            }
         return( FALSE );

      case TB_GETBUTTONTEXT: {
         int index;

         pToolbar = GetBlock( hwnd );
         if( ( index = (int)SendMessage( hwnd, TB_COMMANDTOINDEX, wParam, 0L ) ) != -1 )
            {
            ASSERT( index >= 0 && index < pToolbar->cButtons );
            pButton = GetButton( pToolbar, index );
            ASSERT( pButton != NULL );
            GetButtonString( hwnd, pToolbar, &pButton->tbdb.tb, (LPSTR)lParam );
            return( lstrlen( (LPSTR)lParam ) );
            }
         return( -1 );
         }

      case TB_GETITEMRECT:
         pToolbar = GetBlock( hwnd );
         if( (int)wParam == -1 )
            {
            LPRECT lprc;

            lprc = (LPRECT)lParam;
            SetRect( lprc, 0, 0, pToolbar->dxButton, pToolbar->dyButton );
            return( TRUE );
            }
         else if( pButton = GetButton( pToolbar, (int)wParam ) )
            {
            LPRECT lprc;

            lprc = (LPRECT)lParam;
            *lprc = pButton->tbdb.rc;
            return( TRUE );
            }
         return( FALSE );

      case TB_GETROWS:
         pToolbar = GetBlock( hwnd );
         return( pToolbar->cRows );

      case TB_GETSTATE:
         pToolbar = GetBlock( hwnd );
         if( pButton = GetButtonByCommand( pToolbar, (int)wParam ) )
            return( pButton->tbdb.tb.fsState );
         return( -1 );

      case TB_GETTOOLTIPS:
         pToolbar = GetBlock( hwnd );
         return( (LRESULT)(LPSTR)pToolbar->hwndTooltip );

      case TB_HIDEBUTTON:
         pToolbar = GetBlock( hwnd );
         if( pButton = GetButtonByCommand( pToolbar, (int)wParam ) )
            {
            if( LOWORD( lParam ) )
               pButton->tbdb.tb.fsState |= TBSTATE_HIDDEN;
            else
               pButton->tbdb.tb.fsState &= ~TBSTATE_HIDDEN;
            InvalidateRect( hwnd, &pButton->tbdb.rc, TRUE );
            UpdateWindow( hwnd );
            return( TRUE );
            }
         return( FALSE );

      case TB_HITTEST: {
         register int c;
         POINT pt;

         pToolbar = GetBlock( hwnd );
         pButton = pToolbar->pFirstButton;
         pToolbar->nSelectedButton = -1;
         pt.x = LOWORD( lParam );
         pt.y = HIWORD( lParam );
         if( pToolbar->cButtons > 0 )
            {
            RECT rc;

            CopyRect( &rc, &pButton->tbdb.rc );
            if( GetWindowStyle( hwnd ) & TBSTYLE_VERT )
               {
               rc.right = rc.left - 1;
               rc.left = 0;
               }
            else
               {
               rc.bottom = rc.top - 1;
               rc.top = 0;
               }
            if( PtInRect( &rc, pt ) )
               return( TBHT_LEFTEND );
            }
         for( c = 0; c < pToolbar->cButtons; ++c )
            {
            if( PtInRect( &pButton->tbdb.rc, pt ) )
               return( c );
            pButton = pButton->pNextButton;
            }
         return( TBHT_RIGHTEND );
         }

      case TB_INDETERMINATE:
         pToolbar = GetBlock( hwnd );
         if( pButton = GetButtonByCommand( pToolbar, (int)wParam ) )
            {
            if( LOWORD( lParam ) )
               pButton->tbdb.tb.fsState |= TBSTATE_INDETERMINATE;
            else
               pButton->tbdb.tb.fsState &= ~TBSTATE_INDETERMINATE;
            InvalidateRect( hwnd, &pButton->tbdb.rc, TRUE );
            UpdateWindow( hwnd );
            return( TRUE );
            }
         return( FALSE );

      case TB_INSERTBUTTON: {
         RECT rc;

         pToolbar = GetBlock( hwnd );
         if( InsertButton( hwnd, pToolbar, wParam, (LPTBBUTTON)lParam ) )
            {
            AskParentForWorkspaceRect( hwnd, &rc );
            RecalcToolbarSize( hwnd, pToolbar, &rc, TRUE );
            return( TRUE );
            }
         return( FALSE );
         }

      case TB_ISBUTTONCHECKED:
         pToolbar = GetBlock( hwnd );
         if( pButton = GetButtonByCommand( pToolbar, (int)wParam ) )
            return( pButton->tbdb.tb.fsState & TBSTATE_CHECKED );
         return( FALSE );

      case TB_ISBUTTONENABLED:
         pToolbar = GetBlock( hwnd );
         if( pButton = GetButtonByCommand( pToolbar, (int)wParam ) )
            return( pButton->tbdb.tb.fsState & TBSTATE_ENABLED );
         return( FALSE );

      case TB_ISBUTTONHIDDEN:
         pToolbar = GetBlock( hwnd );
         if( pButton = GetButtonByCommand( pToolbar, (int)wParam ) )
            return( pButton->tbdb.tb.fsState & TBSTATE_HIDDEN );
         return( FALSE );

      case TB_ISBUTTONINDETERMINATE:
         pToolbar = GetBlock( hwnd );
         if( pButton = GetButtonByCommand( pToolbar, (int)wParam ) )
            return( pButton->tbdb.tb.fsState & TBSTATE_INDETERMINATE );
         return( FALSE );

      case TB_ISBUTTONPRESSED:
         pToolbar = GetBlock( hwnd );
         if( pButton = GetButtonByCommand( pToolbar, (int)wParam ) )
            return( pButton->tbdb.tb.fsState & TBSTATE_PRESSED );
         return( FALSE );

      case TB_PRESSBUTTON:
         pToolbar = GetBlock( hwnd );
         if( pButton = GetButtonByCommand( pToolbar, (int)wParam ) )
            {
            PressButton( hwnd, pButton, LOWORD( lParam ) );
            return( TRUE );
            }
         return( FALSE );

      case TB_SETBITMAPSIZE:
         pToolbar = GetBlock( hwnd );
         if( 0 == pToolbar->cButtons )
            {
            pToolbar->dxBitmap = LOWORD( lParam );
            pToolbar->dyBitmap = HIWORD( lParam );
            return( TRUE );
            }
         return( FALSE );

      case TB_SETBUTTONSIZE:
         pToolbar = GetBlock( hwnd );
         if( 0 == pToolbar->cButtons )
            {
            pToolbar->dxButton = LOWORD( lParam );
            pToolbar->dyButton = HIWORD( lParam );
            pToolbar->dxMaxStrWidth = pToolbar->dxButton - 6;
            return( TRUE );
            }
         return( FALSE );

      case TB_SETCMDID:
         pToolbar = GetBlock( hwnd );
         if( pButton = GetButton( pToolbar, (int)wParam ) )
            {
            pButton->tbdb.tb.idCommand = LOWORD( lParam );
            return( TRUE );
            }
         return( FALSE );

      case TB_SETPARENT:
         pToolbar = GetBlock( hwnd );
         ASSERT( IsWindow( (HWND)wParam ) );
         pToolbar->hwndParent = (HWND)wParam;
         return( 0L );

      case TB_SETSTATE:
         pToolbar = GetBlock( hwnd );
         if( pButton = GetButtonByCommand( pToolbar, (int)wParam ) )
            {
            if( LOWORD( lParam ) != pButton->tbdb.tb.fsState )
               {
               pButton->tbdb.tb.fsState = (BYTE)LOWORD( lParam );
               InvalidateRect( hwnd, &pButton->tbdb.rc, TRUE );
               UpdateWindow( hwnd );
               }
            return( TRUE );
            }
         return( -1 );

      case TB_SETTOOLTIPS:
         pToolbar = GetBlock( hwnd );
         pToolbar->hwndTooltip = (HWND)lParam;
         return( 0L );

      case WM_CANCELMODE:
         if( fCaptured )
            {
            RECT rc2;

            /* Release capture.
             */
            fCaptured = FALSE;
            ReleaseCapture();

            /* Pop tooltip.
             */
            pToolbar = GetBlock( hwnd );
            if( NULL != pToolbar->hwndTooltip )
               SendMessage( pToolbar->hwndTooltip, TTM_POP, 0, 0L );

            /* Un-raise the old selected button.
             */
            CopyRect( &rc2, &pToolbar->pSelectedButton->tbdb.rc );
            pToolbar->pSelectedButton = NULL;
            InvalidateRect( hwnd, &rc2, TRUE );
            UpdateWindow( hwnd );
            fIgnoreNextMouseMove = TRUE;
            }
         return( 0L );

      case WM_MOVE:
         /* Trap MoveWindow and SetWindowPos on this window so that the
          * status bar remains anchored to the top or bottom of the parent window.
          */
         if( !( GetWindowStyle( hwnd ) & ACTL_CCS_NOPARENTALIGN ) )
            {
            RECT rc;

            pToolbar = GetBlock( hwnd );
            AskParentForWorkspaceRect( hwnd, &rc );
            if( GetWindowStyle( hwnd ) & ACTL_CCS_TOP )
               SetWindowPos( hwnd, NULL, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER );
            else
               SetWindowPos( hwnd, NULL, 0, rc.bottom - pToolbar->dySize, 0, 0, SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER );
            }
         break;

      case WM_SIZE: {
         RECT rc;

         /* Trap MoveWindow and SetWindowPos on this window so that the
          * status bar remains the same width as the client area the parent window.
          */
         pToolbar = GetBlock( hwnd );
         SetRect( &rc, 0, 0, LOWORD( lParam ), HIWORD( lParam ) );
         if( !RecalcToolbarSize( hwnd, pToolbar, &rc, TRUE ) )
            {
            if( GetWindowStyle( hwnd ) & TBSTYLE_VERT )
               SetWindowPos( hwnd, NULL, 0, 0, pToolbar->dxSize, rc.bottom, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE );
            else
               SetWindowPos( hwnd, NULL, 0, 0, rc.right, pToolbar->dySize, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE );
            }
         break;
         }

      case WM_NOTIFY:
         /* Relay tooltips notification to the toolbar parent window.
          */
         pToolbar = GetBlock( hwnd );
         return( SendMessage( pToolbar->hwndParent, message, wParam, lParam ) );

      case WM_CONTEXTMENU: /*!!SM!!*/
      case WM_RBUTTONDOWN: {
         NMHDR nmh;
         pToolbar = GetBlock( hwnd );

         if( NULL != pToolbar->pSelectedButton )
         {
         RECT rc2;

         /* Un-raise the old selected button.
          */
         CopyRect( &rc2, &pToolbar->pSelectedButton->tbdb.rc );
         pToolbar->pSelectedButton = NULL;
         InvalidateRect( hwnd, &rc2, TRUE );
         UpdateWindow( hwnd );
         }
         Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_RCLICK, (LPNMHDR)&nmh );
         break;
         }

      case WM_LBUTTONDOWN: {
         register int c;
         NMHDR nmhdr;
         TBNOTIFY tbn;
         POINT pt;

         pToolbar = GetBlock( hwnd );
         pt.x = LOWORD( lParam );
         pt.y = HIWORD( lParam );

         /* First of all, if we've got tooltips, then relay this
          * message to the tooltip control to give it a chance to clear
          * any tooltip window from the screen.
          */
         if( pToolbar->hwndTooltip )
            {
            MSG msg;

            msg.message = message;
            msg.hwnd = hwnd;
            msg.wParam = wParam;
            msg.lParam = lParam;
            SendMessage( pToolbar->hwndTooltip, TTM_RELAYEVENT, 0, (LPARAM)(LPSTR)&msg );
            }

         /* Tell the parent that the user has clicked on a button.
          */
         Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_CLICK, &nmhdr );
         pButton = pToolbar->pFirstButton;
         pToolbar->nSelectedButton = -1;
         for( c = 0; c < pToolbar->cButtons; ++c )
            {
            if( PtInRect( &pButton->tbdb.rc, pt ) )
               {
               /* If the Shift key is down and the toolbar is adjustable, then this
                * is a potential drag event. If the Alt key is down and the TBSTYLE_ALTDRAG
                * is specified, then this is also a potential drag event.
                */
               if( ( pToolbar->fEditMode ||
                   ( ( GetWindowStyle( hwnd ) & ACTL_CCS_ADJUSTABLE ) && ( GetKeyState( VK_SHIFT ) & 0x8000 ) ) ||
                   ( ( GetWindowStyle( hwnd ) & TBSTYLE_ALTDRAG ) && ( GetKeyState( VK_MENU ) & 0x8000 ) ) ) &&
                   ( pButton->tbdb.tb.fsStyle != TBSTYLE_SEP ) )
                  {
                  TBDRAWBUTTON tbdb;
                  HDC hdc;

                  ASSERT( -1 == nDragButton );

                  /* Set the drag rectangle to be initially the position of the
                   * button being dragged, and set ptDragOffset to be the offset of
                   * the cursor from the top left corner of the drag rectangle.
                   */
                  nDragButton = c;
                  CopyRect( &rcDrag, &pButton->tbdb.rc );
                  MapWindowPoints( hwnd, NULL, (LPPOINT)&rcDrag, 2 );
                  ptStart = pt;

                  /* Get read for dragon drop.
                   */
                  hcurToolbarDrag = LoadCursor( hLibInst, MAKEINTRESOURCE(IDC_DRAGCURSOR) );
                  hcurNoDrop = LoadCursor( hLibInst, MAKEINTRESOURCE(IDC_NONODROP) );
                  hcurCur = hcurToolbarDrag;
                  hcurOld = GetCursor();
                  fDragging = TRUE;
                  fDoneBitmap = FALSE;

                  /* Create a drag image.
                   */
                  hdc = GetDC( NULL );
                  hdcDrag = CreateCompatibleDC( hdc );
                  hdcSaved = CreateCompatibleDC( hdc );
                  tbdb.tb = pButton->tbdb.tb;
                  tbdb.rc = pButton->tbdb.rc;
                  tbdb.rc.right -= tbdb.rc.left;
                  tbdb.rc.bottom -= tbdb.rc.top;
                  tbdb.rc.left = 0;
                  tbdb.rc.top = 0;
                  cxButton = tbdb.rc.right;
                  cyButton = tbdb.rc.bottom;

                  /* Create bitmaps compatible with the display
                   * attributes. These will hold the button bitmap and
                   * screen background.
                   */
                  hbmpDrag = CreateCompatibleBitmap( hdc, cxButton, cyButton );
                  hbmpSaved = CreateCompatibleBitmap( hdc, cxButton, cyButton );

                  /* Draw the button bitmap into hdcDrag, so we'll just BitBlt this
                   * to the screen which will be considerably faster than redrawing
                   * the button on each mouse move.
                   */
                  hbmpOld = SelectBitmap( hdcDrag, hbmpDrag );
                  hbmpOldSaved = SelectBitmap( hdcSaved, hbmpSaved );
                  SendMessage( hwnd, TB_DRAWBUTTON, (WPARAM)hdcDrag, (LPARAM)(LPSTR)&tbdb );
                  ReleaseDC( NULL, hdc );
                  SetCapture( hwnd );
                  break;
                  }

               if( pButton->tbdb.tb.fsState & TBSTATE_ENABLED )
                  {
                  int tbStyle;

                  tbStyle = pButton->tbdb.tb.fsStyle & 0x07;
                  if( TBSTYLE_BUTTON == tbStyle )
                     {
                     pToolbar->nSelectedButton = c;
                     pButton->tbdb.tb.fsState |= TBSTATE_PRESSED;
                     InvalidateRect( hwnd, &pButton->tbdb.rc, TRUE );
                     UpdateWindow( hwnd );
                     }
                  else if( TBSTYLE_CHECK == tbStyle )
                     {
                     pToolbar->nSelectedButton = c;
                     pButton->tbdb.tb.fsState |= TBSTATE_PRESSED;
                     InvalidateRect( hwnd, &pButton->tbdb.rc, TRUE );
                     UpdateWindow( hwnd );
                     }
                  else if( TBSTYLE_CHECKGROUP == tbStyle && !( pButton->tbdb.tb.fsState & TBSTATE_CHECKED ) )
                     {
                     pToolbar->nSelectedButton = c;
                     pButton->tbdb.tb.fsState |= TBSTATE_PRESSED;
                     InvalidateRect( hwnd, &pButton->tbdb.rc, TRUE );
                     UpdateWindow( hwnd );
                     }
                  else if( TBSTYLE_GROUP == tbStyle )
                     {
                     pToolbar->nSelectedButton = c;
                     pButton->tbdb.tb.fsState |= TBSTATE_PRESSED;
                     InvalidateRect( hwnd, &pButton->tbdb.rc, TRUE );
                     UpdateWindow( hwnd );
                     }

                  /* This is a non-editing press, so send parent a
                   * message saying that the state has changed.
                   */
                  tbn.iItem = pButton->tbdb.tb.idCommand;
                  tbn.tbButton = pButton->tbdb.tb;
                  Amctl_SendNotify( GetParent( hwnd ), hwnd, TBN_BUTTONSTATECHANGED, (LPNMHDR)&tbn );
                  }
               break;
               }
            pButton = pButton->pNextButton;
            }
         if( pToolbar->nSelectedButton != -1 )
            SetCapture( hwnd );
         return( 0L );
         }

      case WM_SETCURSOR:
         if( fDragging )
            {
            /* If we're dragging, keep the cursor as a drag cursor.
             */
            SetCursor( hcurCur );
            return( 0L );
            }
         break;

      case WM_MOUSEMOVE:
         pToolbar = GetBlock( hwnd );
         if( fDragging )
            {
            POINT pt;
            RECT rc;
            HDC hdc;

            /* Restore the screen portion overwritten by the
             * bitmap. This might look horrible on a slow machine!
             */
            hdc = GetDC( NULL );
            if( fDoneBitmap )
               {
               BitBlt( hdc, ptLast.x, ptLast.y, cxButton, cyButton, hdcSaved, 0, 0, SRCCOPY );
               fDoneBitmap = FALSE;
               }

            /* Where is the cursor? Check for it being in the list box then
             * check for it being on the toolbar. If neither of these, put up
             * the NoDrop cursor and finish now.
             */
            pt.x = (short)LOWORD( lParam );
            pt.y = (short)HIWORD( lParam );
            GetClientRect( hwnd, &rc );
            if( !PtInRect( &rc, pt ) )
               {
               HWND hwndAtCursor;

               MapWindowPoints( hwnd, GetParent( hwnd ), &pt, 1 );
               hwndAtCursor = ChildWindowFromPoint( GetParent( hwnd ), pt );
//             if( hwndAtCursor != hwnd )
//                {
//                SetCursor( hcurCur = hcurNoDrop );
//                ReleaseDC( NULL, hdc );
//                return( 0L );
//                }
               pt.x = (short)LOWORD( lParam );
               pt.y = (short)HIWORD( lParam );
               }

            /* Draw the button bitmap at the current mouse coordinates, offset by 16
             * pixels, then the cursor.
             */
            ClientToScreen( hwnd, &pt );
            pt.x -= 16;
            pt.y -= 16;
            BitBlt( hdcSaved, 0, 0, cxButton, cyButton, hdc, pt.x, pt.y, SRCCOPY );
            BitBlt( hdc, pt.x, pt.y, cxButton, cyButton, hdcDrag, 0, 0, SRCCOPY );
            ptLast = pt;
            SetCursor( hcurCur = hcurToolbarDrag );
            fDoneBitmap = TRUE;
            ReleaseDC( NULL, hdc );
            }
         else if( pToolbar->nSelectedButton != -1 )
            {
            POINT pt;
            BOOL fUpdate;

            pButton = GetButton( pToolbar, pToolbar->nSelectedButton );
            pt.x = LOWORD( lParam );
            pt.y = HIWORD( lParam );
            fUpdate = FALSE;
            if( PtInRect( &pButton->tbdb.rc, pt ) )
               {
               if( !( pButton->tbdb.tb.fsState & TBSTATE_PRESSED ) )
                  {
                  pButton->tbdb.tb.fsState |= TBSTATE_PRESSED;
                  fUpdate = TRUE;
                  }
               }
            else
               {
               if( pButton->tbdb.tb.fsState & TBSTATE_PRESSED )
                  {
                  pButton->tbdb.tb.fsState &= ~TBSTATE_PRESSED;
                  fUpdate = TRUE;
                  }
               }
            if( fUpdate )
               {
               InvalidateRect( hwnd, &pButton->tbdb.rc, TRUE );
               UpdateWindow( hwnd );
               }
            }
         else if( !fIgnoreNextMouseMove )
            {
            /* Flat toolbar style.
             * When the cursor moves over these buttons, it shows the button
             * in 3D relief format.
             */
            if( ( GetWindowStyle( hwnd ) & TBSTYLE_FLAT ) == TBSTYLE_FLAT )
               {
               register int c;
               POINT pt;

               /* First find the button at the cursor
                * position.
                */
               pt.x = (short)LOWORD( lParam );
               pt.y = (short)HIWORD( lParam );
               pButton = pToolbar->pFirstButton;
               for( c = 0; c < pToolbar->cButtons; ++c )
                  {
                  if( PtInRect( &pButton->tbdb.rc, pt ) )
                     break;
                  pButton = pButton->pNextButton;
                  }

               /* If this is different from the last selected button, we
                * change the last and new button state.
                */
               if( pButton != pToolbar->pSelectedButton )
                  {
                  if( NULL != pToolbar->pSelectedButton )
                     {
                     RECT rc2;

                     /* Un-raise the old selected button.
                      */
                     CopyRect( &rc2, &pToolbar->pSelectedButton->tbdb.rc );
                     pToolbar->pSelectedButton = NULL;
                     InvalidateRect( hwnd, &rc2, TRUE );
                     UpdateWindow( hwnd );
                     }
                  ReleaseCapture();
                  fCaptured = FALSE;
                  if( NULL != pButton && ( pButton->tbdb.tb.fsState & TBSTATE_ENABLED ) )
                     {
                     /* Raise the new selected button.
                      */
                     pToolbar->pSelectedButton = pButton;
                     InvalidateRect( hwnd, &pToolbar->pSelectedButton->tbdb.rc, TRUE );
                     UpdateWindow( hwnd );
                     SetCapture( hwnd );
                     fCaptured = TRUE;
                     }
                  }
               }
            if( pToolbar->hwndTooltip )
               {
               MSG msg;

               msg.message = message;
               msg.hwnd = hwnd;
               msg.wParam = wParam;
               msg.lParam = lParam;
               SendMessage( pToolbar->hwndTooltip, TTM_RELAYEVENT, 0, (LPARAM)(LPSTR)&msg );
               }
            }
         fIgnoreNextMouseMove = FALSE;
         break;

      case WM_LBUTTONUP:
         pToolbar = GetBlock( hwnd );
         if( fDragging )
            {
            NMHDR nmh;
            POINT pt;
            RECT rc;

            /* Restore the screen portion overwritten by the
             * bitmap.
             */
            if( fDoneBitmap )
               {
               HDC hdc;

               hdc = GetDC( NULL );
               BitBlt( hdc, ptLast.x, ptLast.y, cxButton, cyButton, hdcSaved, 0, 0, SRCCOPY );
               ReleaseDC( NULL, hdc );
               }

            /* First switch off the drag stuff.
             */
            SetCursor( hcurOld );
            DestroyCursor( hcurToolbarDrag );
            DestroyCursor( hcurNoDrop );
            ReleaseCapture();
            fCaptured = FALSE;
            fDragging = FALSE;

            /* Clean up the DC stuff.
             */
            SelectBitmap( hdcSaved, hbmpOldSaved );
            SelectBitmap( hdcDrag, hbmpOld );
            DeleteDC( hdcSaved );
            DeleteDC( hdcDrag );
            DeleteBitmap( hbmpSaved );
            DeleteBitmap( hbmpDrag );

            /* Now where did we drop? If the tool is outside the toolbar, delete
             * the tool (no need for confirmation since the parent has already
             * permitted the tool to be deleted via TBN_QUERYDELETE when the drag
             * started). If the tool is on the toolbar, move the tool to the left
             * of the selected button.
             */
            GetClientRect( hwnd, &rc );
            pt.x = LOWORD( lParam );
            pt.y = HIWORD( lParam );
            if( !PtInRect( &rc, pt ) )
               {
               BUTTON FAR * pDragButton;
               TBNOTIFY tbn;

               ASSERT( nDragButton != -1 );
               pDragButton = GetButton( pToolbar, nDragButton );
               tbn.iItem = nDragButton;
               tbn.tbButton = pDragButton->tbdb.tb;
               if( Amctl_SendNotify( pToolbar->hwndParent, hwnd, TBN_QUERYDELETE, (LPNMHDR)&tbn ) )
                  {
                  RECT rc;

                  DeleteButton( hwnd, pToolbar, nDragButton );
                  AskParentForWorkspaceRect( hwnd, &rc );
                  RecalcToolbarSize( hwnd, pToolbar, &rc, TRUE );
                  }
               }
            else
               {
               register int c;
               TBNOTIFY tbn;

               /* Which button are we over? Set c to the index of the button, or
                * pButton == NULL if we're past the end of the button.
                */
               pButton = pToolbar->pFirstButton;
               if( pt.x >= pButton->tbdb.rc.left )
                  {
                  for( c = 0; c < pToolbar->cButtons && !PtInRect( &pButton->tbdb.rc, pt ); ++c )
                     pButton = pButton->pNextButton;
   
                  /* Ask the parent if we can insert the button to the left of the
                   * button at position c.
                   */
                  tbn.iItem = c;
                  if( Amctl_SendNotify( pToolbar->hwndParent, hwnd, TBN_QUERYINSERT, (LPNMHDR)&tbn ) )
                     {
                     BUTTON FAR * pDragButton;
                     TBBUTTON tb;
                     RECT rc;
   
                     /* Save the attributes of the button being moved so
                      * we can pass them to InsertButton later.
                      */
                     pDragButton = GetButton( pToolbar, nDragButton );
                     ASSERT( pDragButton != NULL );
                     tb = pDragButton->tbdb.tb;
   
                     /* If the button over which we've dropped is the same as the
                      * drag button, then we're dealing with separators.
                      */
                     if( pButton == pDragButton )
                        {
                        BUTTON FAR * pPrevButton;
   
                        /* Did we move the button left or right?
                         */
                        pPrevButton = GetButton( pToolbar, c - 1 );
                        if( pt.x > ptStart.x )
                           {
                           /* Get the button before this one. If it is not a separator,
                            * then create the separator.
                            */
//                         if( !pPrevButton || ( pPrevButton->tbdb.tb.fsStyle != TBSTYLE_SEP ) )
//                            {
                              static TBBUTTON tbSep;
      
                              memset( &tbSep, 0, sizeof( TBBUTTON ) );
                              tbSep.fsStyle = TBSTYLE_SEP;
                              InsertButton( hwnd, pToolbar, c - 1, &tbSep );
//                            }
//                         else
                              /* If it is a separator, just increase the separator width.
                               */
//                            pPrevButton->tbdb.tb.iBitmap += SEPARATOR_WIDTH;
                           }
                        else if( pPrevButton && pPrevButton->tbdb.tb.fsStyle == TBSTYLE_SEP )
                           {
                           /* If the separator is the minimum width, delete the
                            * separator.
                            */
                           if( ( pPrevButton->tbdb.tb.iBitmap - SEPARATOR_WIDTH ) == 0 )
                              DeleteButton( hwnd, pToolbar, c - 1 );
                           else
                              /* Just decrease the separator width.
                               */
                              pPrevButton->tbdb.tb.iBitmap -= SEPARATOR_WIDTH;
                           }
                        }
                     else
                        {
                        /* Remove the button being dragged. If it appeared to
                         * the left of the new position, then decrement c to account
                         * for the buttons being shifted left.
                         */
                        DeleteButton( hwnd, pToolbar, nDragButton );
                        if( nDragButton < c )
                           --c;
      
                        /* Insert the button in its new position, then redraw the
                         * toolbar. Note we take 1 from c before calling InsertButton
                         * because it takes the index of the button AFTER which the new
                         * button appears, not before.
                         */
                        InsertButton( hwnd, pToolbar, c - 1, &tb );

                        /* Here's where we check for separators on the right of the
                         * last button and ensure that they are deleted.
                         */
                        pButton = GetButton( pToolbar, pToolbar->cButtons - 1 );
                        if( pButton->tbdb.tb.fsStyle == TBSTYLE_SEP )
                           DeleteButton( hwnd, pToolbar, pToolbar->cButtons - 1 );
                        }
                     AskParentForWorkspaceRect( hwnd, &rc );
                     RecalcToolbarSize( hwnd, pToolbar, &rc, TRUE );
                     }
                  }
               }
            nDragButton = -1;
            Amctl_SendNotify( pToolbar->hwndParent, hwnd, TBN_TOOLBARCHANGE, &nmh );
            }
         else if( pToolbar->nSelectedButton != -1 )
            {
            TBNOTIFY tbn;

            /* We've released the mouse button. If we're over the original
             * button, release it, handle it depending on whether it is a 
             * press, check, group or checkgroup button, then send a WM_COMMAND
             * to the parent window.
             */
            pButton = GetButton( pToolbar, pToolbar->nSelectedButton );
            ASSERT( NULL != pButton );
            if( pButton->tbdb.tb.fsState & TBSTATE_PRESSED )
               {
               int tbStyle;

               tbStyle = pButton->tbdb.tb.fsStyle & 0x07;
               if( TBSTYLE_BUTTON == tbStyle )
                  {
                  pButton->tbdb.tb.fsState &= ~TBSTATE_PRESSED;
                  InvalidateRect( hwnd, &pButton->tbdb.rc, TRUE );
                  UpdateWindow( hwnd );
                  }
               else if( TBSTYLE_CHECK == tbStyle )
                  CheckButton( hwnd, pButton, !( pButton->tbdb.tb.fsState & TBSTATE_CHECKED ) );
               else if( TBSTYLE_CHECKGROUP == tbStyle )
                  CheckGroupButton( hwnd, pButton, pToolbar->nSelectedButton );
               else if( TBSTYLE_GROUP == tbStyle )
                  GroupButton( hwnd, pButton, pToolbar->nSelectedButton );
               ReleaseCapture();
               fCaptured = FALSE;
               pToolbar->nSelectedButton = -1;
               if( NULL != pToolbar->pSelectedButton )
                  {
                  RECT rc2;

                  /* Un-raise the old selected button.
                   */
                  CopyRect( &rc2, &pToolbar->pSelectedButton->tbdb.rc );
                  pToolbar->pSelectedButton = NULL;
                  InvalidateRect( hwnd, &rc2, TRUE );
                  UpdateWindow( hwnd );
                  }
               PostCommand( pToolbar->hwndParent, pButton->tbdb.tb.idCommand, 2 );
               }
            else
               {
               /* The mouse cursor was not over the button when the button was
                * released, so do nothing.
                */
               ReleaseCapture();
               fCaptured = FALSE;
               pToolbar->nSelectedButton = -1;
               }

            /* This is a non-editing press, so send parent a
             * message saying that the state has changed.
             */
            tbn.iItem = pButton->tbdb.tb.idCommand;
            tbn.tbButton = pButton->tbdb.tb;
            Amctl_SendNotify( GetParent( hwnd ), hwnd, TBN_BUTTONSTATECHANGED, (LPNMHDR)&tbn );
            }
         return( 0L );

      case WM_PAINT: {
         PAINTSTRUCT ps;
         register int c;
         HBRUSH hbr;
         HBRUSH hbrDither;
         RECT rcClient;
         static RECT rc;
         HDC hdc;
         HPEN hpenWhite;
         HPEN hpenDkGrey;
         HPEN hpenBlack;
         HPEN hpenBrGrey;
         HPEN hpenOld;
         BOOL fFlatButtons;
         BOOL fVert;
         BOOL fVisibleSeperators;

         /* Create and cache some useful pens.
          */      
         hpenWhite = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNHIGHLIGHT ) );
         hpenDkGrey = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNSHADOW ) );
         hpenBrGrey = CreatePen( PS_SOLID, 1, GetSysColor( fNewShell ? COLOR_3DLIGHT : COLOR_BTNFACE ) );
         hpenBlack = GetStockPen( BLACK_PEN );


         /* Start to paint...
          */
         pToolbar = GetBlock( hwnd );
         hdc = BeginPaint( hwnd, (LPPAINTSTRUCT)&ps );

         /* Erase the toolbar background.
          */
         GetClientRect( hwnd, &rcClient );
         IntersectRect( &rc, &rcClient, &ps.rcPaint );
         hbr = CreateSolidBrush( GetSysColor( COLOR_BTNFACE ) );
         FillRect( hdc, &rc, hbr );

         /* Create dither brush.
          */
         hbrDither = CreatePatternBrush( hbmpDither );

         /* Draw the dividers if permitted.
          */
         hpenOld = SelectPen( hdc, hpenDkGrey );
         fVert = ( GetWindowStyle( hwnd ) & TBSTYLE_VERT ) == TBSTYLE_VERT;
         fFlatButtons = ( GetWindowStyle( hwnd ) & TBSTYLE_FLAT ) == TBSTYLE_FLAT;
         fVisibleSeperators = ( GetWindowStyle( hwnd ) & TBSTYLE_VISSEP ) == TBSTYLE_VISSEP;
         if( fVert && !( GetWindowStyle( hwnd ) & ACTL_CCS_NODIVIDER ) && ps.rcPaint.left < DXPADDING )
            {
            MoveToEx( hdc, 0, 0, NULL );
            LineTo( hdc, 0, rcClient.bottom );
            SelectPen( hdc, hpenWhite );
            MoveToEx( hdc, 1, 0, NULL );
            LineTo( hdc, 1, rcClient.bottom );
            }
         else if( !fVert && !( GetWindowStyle( hwnd ) & ACTL_CCS_NODIVIDER ) && ps.rcPaint.top < DYPADDING )
            {
            MoveToEx( hdc, 0, 0, NULL );
            LineTo( hdc, rcClient.right, 0 );
            SelectPen( hdc, hpenWhite );
            MoveToEx( hdc, 0, 1, NULL );
            LineTo( hdc, rcClient.right, 1 );
            }

         /* Draw the border at the bottom.
          */
         SelectPen( hdc, hpenBlack );
         if( fVert )
            {
            MoveToEx( hdc, rcClient.right - 1, 0, NULL );
            LineTo( hdc, rcClient.right - 1, rcClient.bottom );
            }
         else
            {
            MoveToEx( hdc, 0, rcClient.bottom - 1, NULL );
            LineTo( hdc, rcClient.right, rcClient.bottom - 1 );
            }

         /* Select the font into the DC.
          */
         SelectFont( hdc, pToolbar->hFont );

         /* Repaint each button.
          */
         pButton = pToolbar->pFirstButton;
         for( c = 0; c < pToolbar->cButtons; ++c )
            {
            CopyRect( &rc, &pButton->tbdb.rc );
            if( !( pButton->tbdb.tb.fsState & TBSTATE_HIDDEN ) && pButton->tbdb.tb.fsStyle != TBSTYLE_SEP )
               {
               static RECT rcIntersect;

               /* First draw the frame and the inside surface. Check
                * whether any part of this button appears in the repaint
                * region and skip if the button is clipped.
                */
               IntersectRect( &rcIntersect, &rc, &ps.rcPaint );
               if( !IsRectEmpty( &rcIntersect ) )
                  {
                  if( ( pButton->tbdb.tb.fsState & TBSTATE_PRESSED ) && fFlatButtons )
                     {
                     DrawButton( hdc, &rc, hpenBrGrey, hpenWhite, hpenBlack, hpenDkGrey, hbr, fFlatButtons );
                     OffsetRect( &rc, 1, 1 );
                     }
                  else if( pButton->tbdb.tb.fsState & TBSTATE_PRESSED )
                     {
                     DrawButton( hdc, &rc, hpenWhite, hpenBrGrey, hpenBlack, hpenDkGrey, hbr, fFlatButtons );
                     OffsetRect( &rc, 1, 1 );
                     }
                  else if( pButton->tbdb.tb.fsState & TBSTATE_CHECKED )
                     {
                     DrawButton( hdc, &rc, hpenWhite, hpenBrGrey, hpenBlack, hpenDkGrey, hbrDither, fFlatButtons );
                     OffsetRect( &rc, 1, 1 );
                     }
                  else if( pButton == pToolbar->pSelectedButton && fFlatButtons )
                     DrawButton( hdc, &rc, hpenBlack, hpenDkGrey, hpenWhite, hpenBrGrey, hbr, fFlatButtons );
                  else if( !fFlatButtons )
                     DrawButton( hdc, &rc, hpenBlack, hpenDkGrey, hpenWhite, hpenBrGrey, hbr, fFlatButtons );
   
                  /* Next. draw the picture.
                   */
                  InflateRect( &rc, -2, -2 );
                  DrawToolbarButton( hwnd, hdc, pToolbar, &pButton->tbdb.tb, &rc, TRUE );
                  }
               }
            else if( fVisibleSeperators && fFlatButtons && pButton->tbdb.tb.fsStyle == TBSTYLE_SEP )
               {
               /* Draw the vertical separators.
                */
               if( !fVert )
               {
               SelectPen( hdc, hpenDkGrey );
               MoveToEx( hdc, rc.left + ( ( rc.right - rc.left ) / 2 ), rc.top + 5, NULL );
               LineTo( hdc, rc.left + ( ( rc.right - rc.left ) / 2 ), rc.bottom - 5 );
               SelectPen( hdc, hpenWhite );
               MoveToEx( hdc, rc.left + 1 + ( ( rc.right - rc.left ) / 2 ), rc.top + 5, NULL );
               LineTo( hdc, rc.left + 1 + ( ( rc.right - rc.left ) / 2 ), rc.bottom - 5 );
               }
               else
               {
               SelectPen( hdc, hpenDkGrey );
               MoveToEx( hdc, rc.left + 5, rc.top + ( ( rc.bottom - rc.top ) / 2 ), NULL );
               LineTo( hdc, rc.right - 5, rc.top + ( ( rc.bottom - rc.top ) / 2 ) );
               SelectPen( hdc, hpenWhite );
               MoveToEx( hdc, rc.left + 5, rc.top + 1 + ( ( rc.bottom - rc.top ) / 2 ), NULL );
               LineTo( hdc, rc.right - 5, rc.top + 1 + ( ( rc.bottom - rc.top ) / 2 ) );
               }

               }
            pButton = pButton->pNextButton;
            }

         /* Clean up.
          */
         SelectPen( hdc, hpenOld );
         DeletePen( hpenWhite );
         DeletePen( hpenDkGrey );
         DeletePen( hpenBrGrey );
         DeleteBrush( hbr );
         DeleteBrush( hbrDither );
         EndPaint( hwnd, (LPPAINTSTRUCT)&ps );
         break;
         }
      }
   return( DefWindowProc( hwnd, message, wParam, lParam ) );
}

/* This function sends a message to the parent window asking for the
 * parent window client area. The toolbar will use this to determine
 * how wide and tall it can be.
 */
static void FASTCALL AskParentForWorkspaceRect( HWND hwnd, LPRECT lprc )
{
   TBRECT tbrc;

   SetRectEmpty( &tbrc.rc );
   Amctl_SendNotify( GetParent( hwnd ), hwnd, TBN_NEEDRECT, (LPNMHDR)&tbrc );
   *lprc = tbrc.rc;
}

/* This function deletes a button from the toolbar.
 */
static BOOL FASTCALL DeleteButton( HWND hwnd, TOOLBAR FAR * pToolbar, int iButton )
{
   BUTTON FAR * pPreButton;
   BUTTON FAR * pButton;

   pPreButton = GetButton( pToolbar, iButton - 1 );
   pButton = GetButton( pToolbar, iButton );
   if( pButton )
      {
      if( !pPreButton )
         pToolbar->pFirstButton = pButton->pNextButton;
      else
         pPreButton->pNextButton = pButton->pNextButton;
      InvalidateRect( hwnd, &pButton->tbdb.rc, TRUE );

      /* Delete the tooltip for this button.
       */
      if( pToolbar->hwndTooltip )
         {
         TOOLINFO ti;

         ti.hwnd = hwnd;
         ti.uId = pButton->tbdb.tb.idCommand;
         SendMessage( pToolbar->hwndTooltip, TTM_DELTOOL, 0, (LPARAM)(LPTOOLINFO)&ti );
         }
      FreeMemory( &pButton );
      --pToolbar->cButtons;
      return( TRUE );
      }
   return( FALSE );
}

/* Inserts a new button on the toolbar after the button specified by iPreButton.
 * To insert a button at the first position (position 0), set iPreButton to -1.
 * The new button is set with zero dimensions, so a call to InsertButton must
 * always be followed by a call to RecalcButtonSize or WM_SIZE.
 */
static BOOL FASTCALL InsertButton( HWND hwnd, TOOLBAR FAR * pToolbar, int iPreButton, LPTBBUTTON lptb )
{
   static BUTTON FAR * pButton;
   NMHDR nmhdr;

   INITIALISE_PTR(pButton);
   if( fNewMemory( &pButton, sizeof( BUTTON ) ) )
      {
      BUTTON FAR * pPreButton;
      BUTTON FAR * pNextButton;

      if( iPreButton == TBHT_LEFTEND )
         iPreButton = 0;
      else if( iPreButton == TBHT_RIGHTEND )
         iPreButton = pToolbar->cButtons - 1;
      pPreButton = GetButton( pToolbar, iPreButton );
      if( !pPreButton )
         {
         pNextButton = pToolbar->pFirstButton;
         pToolbar->pFirstButton = pButton;
         }
      else
         {
         pNextButton = pPreButton->pNextButton;
         pPreButton->pNextButton = pButton;
         }
      pButton->pNextButton = pNextButton;
      pButton->tbdb.tb = *lptb;
      if( pButton->tbdb.tb.fsStyle == TBSTYLE_SEP && 0 == pButton->tbdb.tb.iBitmap )
         pButton->tbdb.tb.iBitmap = SEPARATOR_WIDTH;
      SetRectEmpty( &pButton->tbdb.rc );

      /* Set the tooltip for this tool.
       */
      if( pToolbar->hwndTooltip )
         {
         TOOLINFO ti;

         ti.cbSize = sizeof( TOOLINFO );
         ti.uFlags = 0;
         ti.hwnd = hwnd;
         ti.hinst = hLibInst;
         ti.uId = pButton->tbdb.tb.idCommand;
         ti.lpszText = LPSTR_TEXTCALLBACK;
         SetRectEmpty( &ti.rect );
         SendMessage( pToolbar->hwndTooltip, TTM_ADDTOOL, 0, (LPARAM)(LPTOOLINFO)&ti );
         }
      ++pToolbar->cButtons;
      return( TRUE );
      }
   else
      Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_OUTOFMEMORY, &nmhdr );
   return( FALSE );
}

/* Retrieve a pointer to a button, given the index of the button.
 * The index of the first button is always zero.
 */
static BUTTON FAR * FASTCALL GetButton( TOOLBAR FAR * pToolbar, int index )
{
   BUTTON FAR * pButton;

   if( index < 0 )
      return( NULL );
   pButton = pToolbar->pFirstButton;
   while( pButton && index-- > 0 )
      pButton = pButton->pNextButton;
   return( pButton );
}

/* Retrieve a pointer to a button, given the command ID of the button.
 */
static BUTTON FAR * FASTCALL GetButtonByCommand( TOOLBAR FAR * pToolbar, int iCommand )
{
   BUTTON FAR * pButton;

   pButton = pToolbar->pFirstButton;
   while( pButton && pButton->tbdb.tb.idCommand != iCommand )
      pButton = pButton->pNextButton;
   return( pButton );
}

/* This function is called when strings are added or removed form the
 * string list. It computes the button widths based on the width of
 * the longest string.
 */
static void FASTCALL RecalcButtonSize( HWND hwnd, TOOLBAR FAR * pToolbar )
{
   HDC hdc;

   /* Get a DC and select the current font.
    */
   hdc = GetDC( hwnd );
   SelectFont( hdc, pToolbar->hFont );

   /* Handle fixed width buttons easily.
    */
   if( !( GetWindowStyle( hwnd ) & TBSTYLE_FIXEDWIDTH ) )
      {
      STRINGLIST FAR * pStringList;
      int dxButtonSoFar;
      int dyButtonSoFar;
      BOOL fWrapText;

      /* Now iterate over all strings, including multiple strings
       * in each list.
       */
      pStringList = pToolbar->pStringList;
      dxButtonSoFar = 0;
      dyButtonSoFar = 0;
      fWrapText = TRUE;    /* Later, get from the styles */
      while( pStringList )
         {
         LPSTR pStr;
         int c;
         
         pStr = pStringList->pStr;
         for( c = 0; pStr[ c ]; ++c )
            {
            SIZE size;

            if( fWrapText )
               {
               RECT rc;

               SetRect( &rc, 0, 0, pToolbar->dxMaxStrWidth, 0 );
               DrawText( hdc, &pStr[ c ], -1, &rc, DT_CALCRECT|DT_CENTER|DT_WORDBREAK );
               size.cx = pToolbar->dxMaxStrWidth;
               size.cy = min( rc.bottom, pToolbar->dyMaxLabelRows * pToolbar->dyLabelLine );
               }
            else
               GetTextExtentPoint( hdc, &pStr[ c ], lstrlen( &pStr[ c ] ), &size );
            size.cy += pToolbar->dyBitmap;
            if( size.cx > dxButtonSoFar )
               dxButtonSoFar = size.cx;
            if( size.cy > dyButtonSoFar )
               dyButtonSoFar = size.cy;
            c += lstrlen( &pStr[ c ] );
            }
         pStringList = pStringList->pNextStringList;
         }

      /* Okay. We've got the width of the longest string in dxButtonSoFar.
       * Add 8 pixels, two on each side to separate the text from the edges,
       * and two for the button borders.
       */
      if( dxButtonSoFar + 4 > pToolbar->dxButton )
         pToolbar->dxButton = dxButtonSoFar + 4;
      if( dyButtonSoFar + 8 > pToolbar->dyButton )
         pToolbar->dyButton = dyButtonSoFar + 8;
      if( dyButtonSoFar > pToolbar->dyString )
         pToolbar->dyString = dyButtonSoFar;
      }
   else
      {
      /* Compute string height as button height minus 8 minus the
       * bitmap height.
       */
      pToolbar->dyString = ( pToolbar->dyButton - pToolbar->dyBitmap ) - 8;
      }

   /* Release the DC
    */
   ReleaseDC( hwnd, hdc );
}

/* Recalculates the position and size of each button on the toolbar with
 * respect to the current dimensions of the parent window client area. This
 * function also handles wrap-round and sets the toolbar width and height
 * parameters.
 */
static BOOL FASTCALL RecalcToolbarSize( HWND hwnd, TOOLBAR FAR * pToolbar, RECT * prc, BOOL fUpdate )
{
   /* Use the width of the parent window as a guide as to when
    * we need to wrap, if wrapping is permitted.
    */
   if( IsRectEmpty( prc ) )
      return( FALSE );
   if( IsIconic( GetParent( hwnd ) ) )
      return( FALSE );

   /* Call appropriate routines to recalculate the toolbar size
    * and position.
    */
   if( ( GetWindowStyle( hwnd ) & TBSTYLE_VERT ) == TBSTYLE_VERT )
      return( RecalcVertToolbarSize( hwnd, pToolbar, prc, fUpdate ) );
   return( RecalcHorzToolbarSize( hwnd, pToolbar, prc, fUpdate ) );
}

/* Recalculates the position and size of each button on the toolbar with
 * respect to the current dimensions of the parent window client area. This
 * function also handles wrap-round and sets the toolbar width and height
 * parameters.
 */
static BOOL FASTCALL RecalcHorzToolbarSize( HWND hwnd, TOOLBAR FAR * pToolbar, RECT * prc, BOOL fUpdate )
{
   BUTTON FAR * pButton;
   BUTTON FAR * pPrevButton;
   static BOOL fUpdatePending = FALSE;
   int x, y;

   /* Initialise the variables we're going to use.
    */
   pButton = pToolbar->pFirstButton;
   pToolbar->cRows = 1;
   pPrevButton = NULL;
   x = SEPARATOR_WIDTH;
   y = DYPADDING;
   if( !( GetWindowStyle( hwnd ) & ACTL_CCS_NODIVIDER ) )
      y += DYPADDING;

   /* Loop for each button and compute the button position. If
    * we go beyond the parent window right edge AND wrapping is
    * permitted, set the state of the last button on the current
    * row to TBSTATE_WRAP and start a new row.
    */
   while( pButton )
      {
      int nWidth;

      /* If the button is a separator, then the iBitmap field is the
       * width of the separator. If it is zero, then we use the standard
       * separator width. This allows us to 'reserve' space on the toolbar
       * for another control by setting the iBitmap field of a separator
       * to just slightly longer than the width of the control.
       */
      pButton->tbdb.tb.fsState &= ~TBSTATE_WRAP;
      if( TBSTYLE_SEP == pButton->tbdb.tb.fsStyle )
         nWidth = pButton->tbdb.tb.iBitmap ? pButton->tbdb.tb.iBitmap : SEPARATOR_WIDTH;
      else
         nWidth = pToolbar->dxButton;
      if( x + nWidth > prc->right )
         {
         if( GetWindowStyle( hwnd ) & TBSTYLE_WRAPABLE )
            {
            ASSERT( pPrevButton != NULL );
            pPrevButton->tbdb.tb.fsState |= TBSTATE_WRAP;
            x = SEPARATOR_WIDTH;
            y += pToolbar->dyButton + DYPADDING;
            ++pToolbar->cRows;
            pButton = pPrevButton;
            }
         else
            break;
         }
      else
         {
         static RECT rcTmp;

         CopyRect( &rcTmp, &pButton->tbdb.rc );
         SetRect( &pButton->tbdb.rc, x, y, x + nWidth, y + pToolbar->dyButton );
         if( _fmemcmp( &rcTmp, &pButton->tbdb.rc, sizeof( RECT ) ) )
            {
            InvalidateRect( hwnd, &rcTmp, TRUE );
            InvalidateRect( hwnd, &pButton->tbdb.rc, TRUE );
            }
         x += ( pButton->tbdb.rc.right - pButton->tbdb.rc.left );
         if( x > pToolbar->dxSize )
            pToolbar->dxSize = x;
         if( !( pButton->tbdb.tb.fsStyle & TBSTYLE_GROUP ) )
            pPrevButton = pButton;
         }
      pButton = pButton->pNextButton;
      }

   /* Any further buttons are marked as hidden.
    */
   while( pButton )
      {
      SetRectEmpty( &pButton->tbdb.rc );
      pButton = pButton->pNextButton;
      }

   /* Adjust the tooltip rectangle for all tools.
    */
   if( pToolbar->hwndTooltip )
      {
      register int c;

      pButton = pToolbar->pFirstButton;
      for( c = 0; c < pToolbar->cButtons; ++c )
         {
         TOOLINFO ti;
      
         ti.hwnd = hwnd;
         ti.uId = pButton->tbdb.tb.idCommand;
         CopyRect( &ti.rect, &pButton->tbdb.rc );
         SendMessage( pToolbar->hwndTooltip, TTM_NEWTOOLRECT, 0, (LPARAM)(LPTOOLINFO)&ti );
         pButton = pButton->pNextButton;
         }
      }

   /* Store the toolbar height.
    */
   if( fUpdatePending )
      {
      fUpdatePending = FALSE;
      if( IsWindowVisible( hwnd ) )
         SendMessage( GetParent( hwnd ), WM_SIZE, 0, MAKELPARAM( prc->right - prc->left, prc->bottom - prc->top ) );
      SetWindowPos( hwnd, NULL, 0, 0, prc->right, pToolbar->dySize, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE );
      }
   else if( pToolbar->dySize != y + pToolbar->dyButton + DYPADDING )
      {
      pToolbar->dySize = y + pToolbar->dyButton + DYPADDING;
      if( fUpdate )
         {
         fUpdatePending = FALSE;
         SetWindowPos( hwnd, NULL, 0, 0, prc->right, pToolbar->dySize, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE );
         if( IsWindowVisible( hwnd ) )
            SendMessage( GetParent( hwnd ), WM_SIZE, 0, MAKELPARAM( prc->right - prc->left, prc->bottom - prc->top ) );
         fUpdate = FALSE;
         }
      else
         fUpdatePending = TRUE;
      return( TRUE );
      }
   return( FALSE );
}

/* Recalculates the position and size of each button on the toolbar with
 * respect to the current dimensions of the parent window client area. This
 * function also handles wrap-round and sets the toolbar width and height
 * parameters.
 */
static BOOL FASTCALL RecalcVertToolbarSize( HWND hwnd, TOOLBAR FAR * pToolbar, RECT * prc, BOOL fUpdate )
{
   static BOOL fSentResize = FALSE;
   BUTTON FAR * pButton;
   BUTTON FAR * pPrevButton;
   int x, y;

   /* Initialise the variables we're going to use.
    */
   pButton = pToolbar->pFirstButton;
   pToolbar->cRows = 1;
   pPrevButton = NULL;
   y = SEPARATOR_WIDTH;
   x = DXPADDING;
   if( !( GetWindowStyle( hwnd ) & ACTL_CCS_NODIVIDER ) )
      x += DXPADDING;

   /* Loop for each button and compute the button position. If
    * we go beyond the parent window right edge AND wrapping is
    * permitted, set the state of the last button on the current
    * row to TBSTATE_WRAP and start a new row.
    */
   while( pButton )
      {
      int nHeight;

      /* If the button is a separator, then the iBitmap field is the
       * width of the separator. If it is zero, then we use the standard
       * separator width. This allows us to 'reserve' space on the toolbar
       * for another control by setting the iBitmap field of a separator
       * to just slightly longer than the width of the control.
       */
      pButton->tbdb.tb.fsState &= ~TBSTATE_WRAP;
      if( TBSTYLE_SEP == pButton->tbdb.tb.fsStyle )
         nHeight = pButton->tbdb.tb.iBitmap ? pButton->tbdb.tb.iBitmap : SEPARATOR_WIDTH;
      else
         nHeight = pToolbar->dxButton;
      if( y + nHeight > prc->bottom )
         {
         if( ( GetWindowStyle( hwnd ) & TBSTYLE_WRAPABLE ) && NULL != pPrevButton )
            {
            ASSERT( pPrevButton != NULL );
            pPrevButton->tbdb.tb.fsState |= TBSTATE_WRAP;
            y = SEPARATOR_WIDTH;
            x += pToolbar->dxButton + DXPADDING;
            ++pToolbar->cRows;
            pButton = pPrevButton;
            }
         else
            break;
         }
      else
         {
         static RECT rcTmp;

         CopyRect( &rcTmp, &pButton->tbdb.rc );
         SetRect( &pButton->tbdb.rc, x, y, x + pToolbar->dxButton, y + nHeight );
         if( _fmemcmp( &rcTmp, &pButton->tbdb.rc, sizeof( RECT ) ) )
            {
            InvalidateRect( hwnd, &rcTmp, TRUE );
            InvalidateRect( hwnd, &pButton->tbdb.rc, TRUE );
            }
         y += ( pButton->tbdb.rc.bottom - pButton->tbdb.rc.top );
         if( y > pToolbar->dySize )
            pToolbar->dySize = y;
         if( !( pButton->tbdb.tb.fsStyle & TBSTYLE_GROUP ) )
            pPrevButton = pButton;
         }
      pButton = pButton->pNextButton;
      }

   /* Any further buttons are marked as hidden.
    */
   while( pButton )
      {
      SetRectEmpty( &pButton->tbdb.rc );
      pButton = pButton->pNextButton;
      }

   /* Adjust the tooltip rectangle for all tools.
    */
   if( pToolbar->hwndTooltip )
      {
      register int c;

      pButton = pToolbar->pFirstButton;
      for( c = 0; c < pToolbar->cButtons; ++c )
         {
         if( !( pButton->tbdb.tb.fsStyle & TBSTYLE_SEP ) )
            {
            TOOLINFO ti;
         
            ti.hwnd = hwnd;
            ti.uId = pButton->tbdb.tb.idCommand;
            CopyRect( &ti.rect, &pButton->tbdb.rc );
            SendMessage( pToolbar->hwndTooltip, TTM_NEWTOOLRECT, 0, (LPARAM)(LPTOOLINFO)&ti );
            }
         pButton = pButton->pNextButton;
         }
      }

   /* Store the toolbar width.
    */
   if( pToolbar->dxSize != x + pToolbar->dxButton + DXPADDING )
      {
      pToolbar->dxSize = x + pToolbar->dxButton + DXPADDING;
      if( fUpdate )
         {
         RECT rc;

         AskParentForWorkspaceRect( hwnd, &rc );
         SetWindowPos( hwnd, NULL, 0, 0, pToolbar->dxSize, rc.bottom, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE );
         if( IsWindowVisible( hwnd ) && !fSentResize )
            {
            fSentResize = TRUE;
            SendMessage( GetParent( hwnd ), WM_SIZE, 0, MAKELPARAM( rc.right - rc.left, rc.bottom - rc.top ) );
            fSentResize = FALSE;
            }
         }
      return( TRUE );
      }
   return( FALSE );
}

/* This function draws a button frame in pressed or unpressed state. The
 * pens passed determine the colours for the Bottom/Right (BR) and Top/Left
 * (TL) inside (I) or outside (O) edges.
 */
void FASTCALL DrawButton( HDC hdc, RECT * prc, HPEN hpenBRO, HPEN hpenBRI, HPEN hpenTLO, HPEN hpenTLI, HBRUSH hbr, BOOL fThin )
{
   RECT rc;
   HPEN hpenOld;

#ifndef WIN32
   ASSERT( IsGDIObject( hpenBRO ) );
   ASSERT( IsGDIObject( hpenBRI ) );
   ASSERT( IsGDIObject( hpenTLO ) );
   ASSERT( IsGDIObject( hpenTLI ) );
   ASSERT( IsGDIObject( hbr ) );
#endif

   if( fThin )
      {
      hpenOld = SelectPen( hdc, hpenBRI );
      MoveToEx( hdc, prc->left + 1, prc->bottom - 2, NULL );
      LineTo( hdc, prc->right - 2, prc->bottom - 2 );
      LineTo( hdc, prc->right - 2, prc->top );
      SelectPen( hdc, hpenTLO );
      MoveToEx( hdc, prc->left + 1, prc->bottom - 3, NULL );
      LineTo( hdc, prc->left + 1, prc->top + 1 );
      LineTo( hdc, prc->right - 2, prc->top + 1 );
      SetRect( &rc, prc->left + 2, prc->top + 2, prc->right - 2, prc->bottom - 2 );
      FillRect( hdc, &rc, hbr );
      SelectPen( hdc, hpenOld );
      }
   else
      {
      hpenOld = SelectPen( hdc, hpenBRO );
      MoveToEx( hdc, prc->left, prc->bottom - 1, NULL );
      LineTo( hdc, prc->right -1, prc->bottom - 1 );
      LineTo( hdc, prc->right -1, prc->top - 1 );
      SelectPen( hdc, hpenBRI );
      MoveToEx( hdc, prc->left + 1, prc->bottom - 2, NULL );
      LineTo( hdc, prc->right - 2, prc->bottom - 2 );
      LineTo( hdc, prc->right - 2, prc->top );
      SelectPen( hdc, hpenTLO );
      MoveToEx( hdc, prc->left, prc->bottom - 2, NULL );
      LineTo( hdc, prc->left, prc->top );
      LineTo( hdc, prc->right - 1, prc->top );
      SelectPen( hdc, hpenTLI );
      MoveToEx( hdc, prc->left + 1, prc->bottom - 3, NULL );
      LineTo( hdc, prc->left + 1, prc->top + 1 );
      LineTo( hdc, prc->right - 2, prc->top + 1 );
      SetRect( &rc, prc->left + 2, prc->top + 2, prc->right - 2, prc->bottom - 2 );
      FillRect( hdc, &rc, hbr );
      SelectPen( hdc, hpenOld );
      }
}

/* This function toggles a check button. If the button is pressed, it
 * is unpressed, otherwise it is pressed.
 */
static void FASTCALL CheckButton( HWND hwnd, BUTTON FAR * pButton, BOOL fCheck )
{
   if( TBSTYLE_CHECK == ( pButton->tbdb.tb.fsStyle & 0x07 ) )
      {
      if( fCheck )
         pButton->tbdb.tb.fsState |= TBSTATE_CHECKED;
      else
         pButton->tbdb.tb.fsState &= ~TBSTATE_CHECKED;
      InvalidateRect( hwnd, &pButton->tbdb.rc, TRUE );
      UpdateWindow( hwnd );
      }
}

/* This function presses or releases a button. If the button is pressed, it
 * is unpressed, otherwise it is pressed.
 */
static void FASTCALL PressButton( HWND hwnd, BUTTON FAR * pButton, BOOL fPress )
{
   if( TBSTYLE_BUTTON == ( pButton->tbdb.tb.fsStyle & 0x07 ) )
      {
      BOOL fStateChanged;

      fStateChanged = FALSE;
      if( fPress )
         {
         if( !( pButton->tbdb.tb.fsState & TBSTATE_PRESSED ) )
            {
            pButton->tbdb.tb.fsState |= TBSTATE_PRESSED;
            fStateChanged = TRUE;
            }
         }
      else
         {
         if( pButton->tbdb.tb.fsState & TBSTATE_PRESSED )
            {
            pButton->tbdb.tb.fsState &= ~TBSTATE_PRESSED;
            fStateChanged = TRUE;
            }
         }
      if( fStateChanged )
         {
         InvalidateRect( hwnd, &pButton->tbdb.rc, TRUE );
         UpdateWindow( hwnd );
         }
      }
}

/* This function handles checked group buttons. Checked group buttons are buttons
 * that are grouped together and mutually exclusive (like radio buttons). When one
 * button in the group is pressed, the currently pressed button in the same group
 * is unpressed.
 */
static void FASTCALL CheckGroupButton( HWND hwnd, BUTTON FAR * pButton, int nButtonIndex )
{
   if( TBSTYLE_CHECKGROUP == ( pButton->tbdb.tb.fsStyle & 0x07 ) )
      {
      register int index;
      TOOLBAR FAR * pToolbar;
      BUTTON FAR * pButton2;

      /* Search to the left and uncheck any checkgroup boxes
       * found.
       */
      pToolbar = GetBlock( hwnd );
      index = nButtonIndex;
      while( pButton2 = GetButton( pToolbar, --index ) )
         {
         if( !( TBSTYLE_CHECKGROUP == ( pButton2->tbdb.tb.fsStyle & 0x07 ) ) )
            break;
         if( pButton2->tbdb.tb.fsState & TBSTATE_CHECKED )
            {
            pButton2->tbdb.tb.fsState &= ~TBSTATE_CHECKED;
            InvalidateRect( hwnd, &pButton2->tbdb.rc, TRUE );
            }
         }

      /* Search to the right and uncheck any checkgroup boxes
       * found.
       */
      index = nButtonIndex;
      while( pButton2 = GetButton( pToolbar, ++index ) )
         {
         if( !( TBSTYLE_CHECKGROUP == ( pButton2->tbdb.tb.fsStyle & 0x07 ) ) )
            break;
         if( pButton2->tbdb.tb.fsState & TBSTATE_CHECKED )
            {
            pButton2->tbdb.tb.fsState &= ~TBSTATE_CHECKED;
            InvalidateRect( hwnd, &pButton2->tbdb.rc, TRUE );
            }
         }

      /* Check this button.
       */
      pButton->tbdb.tb.fsState |= TBSTATE_CHECKED;
      pButton->tbdb.tb.fsState &= ~TBSTATE_PRESSED;
      InvalidateRect( hwnd, &pButton->tbdb.rc, TRUE );
      UpdateWindow( hwnd );
      }
}

/* This function handles group buttons. Group buttons are buttons that are grouped
 * together and mutually exclusive (like radio buttons). When onebutton in the group
 * is pressed, the currently pressed button in the same group is unpressed.
 */
static void FASTCALL GroupButton( HWND hwnd, BUTTON FAR * pButton, int nButtonIndex )
{
   if( TBSTYLE_GROUP == ( pButton->tbdb.tb.fsStyle & 0x07 ) )
      {
      register int index;
      TOOLBAR FAR * pToolbar;
      BUTTON FAR * pButton2;

      /* Search to the left and unpress any buttons
       * found.
       */
      pToolbar = GetBlock( hwnd );
      index = nButtonIndex;
      while( pButton2 = GetButton( pToolbar, --index ) )
         {
         if( !( TBSTYLE_GROUP == ( pButton2->tbdb.tb.fsStyle & 0x07 ) ) )
            break;
         if( pButton2->tbdb.tb.fsState & TBSTATE_PRESSED )
            {
            pButton2->tbdb.tb.fsState &= ~TBSTATE_PRESSED;
            InvalidateRect( hwnd, &pButton2->tbdb.rc, TRUE );
            }
         }

      /* Search to the right and unpress any buttons
       * found.
       */
      index = nButtonIndex;
      while( pButton2 = GetButton( pToolbar, ++index ) )
         {
         if( !( TBSTYLE_GROUP == ( pButton2->tbdb.tb.fsStyle & 0x07 ) ) )
            break;
         if( pButton2->tbdb.tb.fsState & TBSTATE_PRESSED )
            {
            pButton2->tbdb.tb.fsState &= ~TBSTATE_PRESSED;
            InvalidateRect( hwnd, &pButton2->tbdb.rc, TRUE );
            }
         }

      /* Check this button.
       */
      pButton->tbdb.tb.fsState ^= TBSTATE_PRESSED;
      InvalidateRect( hwnd, &pButton->tbdb.rc, TRUE );
      UpdateWindow( hwnd );
      }
}

void FASTCALL DrawToolbarButton( HWND hwnd, HDC hdc, TOOLBAR FAR * pToolbar, TBBUTTON FAR * ptb, RECT * prc, BOOL fDrawString )
{
   BITMAPLIST FAR * pBitmapList;
   HBITMAP hbmpPic;
   HBITMAP hbmpPicOld;
   HBITMAP hbmpCpyOld;
   HBITMAP hBmp;
   BOOL fFreeBmp;
   register int c;
   LPSTR pStr;
   HDC hdcPic;
   HDC hdcCpy;
   int dx, dy;

   /* If button has callback style, get the bitmap by
    * asking the parent window.
    * BUGBUG: Cache returned bitmaps!
    */
   if( ptb->fsStyle & TBSTYLE_CALLBACK )
      {
      TBCALLBACK tbc;

      tbc.tb = *ptb;
      tbc.flag = TBCBF_WANTBITMAP;
      tbc.hBmp = NULL;
      Amctl_SendNotify( GetParent( hwnd ), hwnd, TBN_CALLBACK, (LPNMHDR)&tbc );
      dx = dy = 0;
      hBmp = tbc.hBmp;
      fFreeBmp = FALSE;
      }
   else
      {
      /* Find the bitmap in the bitmap list.
       */
      pBitmapList = pToolbar->pBitmapList;
      c = 0;
      while( pBitmapList && pBitmapList->cBitmaps + c <= ptb->iBitmap )
         {
         c += pBitmapList->cBitmaps;
         pBitmapList = pBitmapList->pNextBitmapList;
         }

      /* Compute dx as the offset of the required bitmap in the selected
       * bitmap list.
       */
      dx = ( ptb->iBitmap - c ) * pToolbar->dxBitmap;
      dy = 0;
      hBmp = pBitmapList->hbmp;
      fFreeBmp = FALSE;
      }

   /* Create a copy of the bitmap. After this, hbmpPic will be the
    * bitmap being painted, and hdcPic will be the DC into which it
    * has beens selected.
    */
   if( NULL != hBmp )
      {
      char szBuf[ 64 ];

      hdcPic = CreateCompatibleDC( hdc );
      hdcCpy = CreateCompatibleDC( hdc );
      hbmpCpyOld = SelectBitmap( hdcCpy, hBmp );
      hbmpPic = CreateCompatibleBitmap( hdcCpy, pToolbar->dxBitmap, pToolbar->dyBitmap );
      hbmpPicOld = SelectBitmap( hdcPic, hbmpPic );
      BitBlt( hdcPic, 0, 0, pToolbar->dxBitmap, pToolbar->dyBitmap, hdcCpy, dx, dy, SRCCOPY );
      SelectBitmap( hdcCpy, hbmpCpyOld );
      DeleteDC( hdcCpy );
      SelectBitmap( hdcPic, hbmpPicOld );
      DeleteDC( hdcPic );

      /* Do we have a string? If so, set dyStrTop to the top edge of where
       * the string will be output. Set dyBmpTop to the top edge of the
       * bitmap in the button.
       */
      pStr = fDrawString ? GetButtonString( hwnd, pToolbar, ptb, szBuf ) : NULL;
      DrawButtonBitmap( hdc, prc, hbmpPic, pStr, pToolbar->dxBitmap, pToolbar->dyBitmap, pToolbar->dyString,
                        pToolbar->dxMaxStrWidth,
                        pToolbar->dyMaxLabelRows * pToolbar->dyLabelLine,
                        ptb->fsState & TBSTATE_ENABLED );
      DeleteBitmap( hbmpPic );
      if( fFreeBmp )
         DeleteBitmap( hBmp );
      }
}

/* This function draws a bitmap and a string within the specified rectangle.
 */
void FASTCALL DrawButtonBitmap( HDC hdc, RECT * prc, HBITMAP hbmpPic, LPSTR pStr, int dxBitmap, int dyBitmap, int dyString,
   int dxMaxStrWidth, int dyMaxStrHeight, BOOL fEnabled )
{
   HBITMAP hbmpMask;
   HBITMAP hbmpMaskOld;
   HBITMAP hbmpPicOld;
   BOOL fWrapText;
   HDC hdcMask;
   HDC hdcPic;
   int dyStrTop;
   int dyBmpTop;

   fWrapText = TRUE;

   /* Compute the vertical offsets of the bitmap and any
    * strings.
    */
   if( pStr )
      {
      dyBmpTop = 2;
      dyStrTop = dyBitmap + dyBmpTop + 2;
      }
   else
      {
      dyStrTop = prc->bottom - prc->top;
      dyBmpTop = ( ( prc->bottom - prc->top ) - dyBitmap ) / 2;
      }

   /* Create a bitmap that will be used to hold the mask.
    */
   hdcMask = CreateCompatibleDC( hdc );
   hbmpMask = CreateCompatibleBitmap( hdcMask, dxBitmap, dyBitmap );
   hbmpMaskOld = SelectObject( hdcMask, hbmpMask );

   /* Select the bitmap into a DC.
    */
   hdcPic = CreateCompatibleDC( hdc );
   hbmpPicOld = SelectObject( hdcPic, hbmpPic );

   /* Create a monochrome mask where we have zeroes in the image, ones
    * elsewhere.
    */
   SetBkColor( hdcPic, RGB_TRANSPARENT );
   BitBlt( hdcMask, 0, 0, dxBitmap, dyBitmap, hdcPic, 0, 0, SRCCOPY );

   /* For disabled buttons, take a different approach.
    */
   if( !fEnabled )
      {
      HBRUSH hBr;
      HBRUSH hBrOld;

      /* First paint the mask using the highlight colour, offset
       * down and to the right one pixel.
       */
      hBr = CreateSolidBrush( GetSysColor( COLOR_BTNHIGHLIGHT ) );
      hBrOld = SelectBrush( hdc, hBr );
      BitBlt( hdc, prc->left + ( ( ( prc->right - prc->left ) - dxBitmap ) / 2 ) + 1,
                   prc->top + dyBmpTop + 1,
                   dxBitmap,
                   dyBitmap,
              hdcMask, 0, 0, ROP_PSDPxax );
      SelectBrush( hdc, hBrOld );
      DeleteBrush( hBr );

      /* Then paint the mask again, using the shadow colour, in the
       * actual button position. This will overlap the image above
       * and produce a bright shadow effect.
       */
      hBr = CreateSolidBrush( GetSysColor( COLOR_BTNSHADOW ) );
      hBrOld = SelectBrush( hdc, hBr );
      BitBlt( hdc, prc->left + ( ( ( prc->right - prc->left ) - dxBitmap ) / 2 ),
                   prc->top + dyBmpTop,
                   dxBitmap,
                   dyBitmap,
              hdcMask, 0, 0, ROP_PSDPxax );
      SelectBrush( hdc, hBrOld );
      DeleteBrush( hBr );
      }
   else
      {
      /* Force conversion of the monochrome to stay black and white
       */
      SetTextColor( hdcPic, RGB( 255, 255, 255 ) );
      SetBkColor( hdcPic, 0L );
      BitBlt( hdcPic, 0, 0, dxBitmap, dyBitmap, hdcMask, 0, 0, SRCAND );

      /* Now draw the bitmap.
       */
      SetTextColor( hdcPic, 0L );
      SetBkColor( hdcPic, RGB( 255, 255, 255 ) );
      BitBlt( hdc, prc->left + ( ( ( prc->right - prc->left ) - dxBitmap ) / 2 ),
                   prc->top + dyBmpTop,
                   dxBitmap,
                   dyBitmap,
              hdcMask, 0, 0, SRCAND );

      SetTextColor( hdc, 0L );
      SetBkColor( hdc, RGB( 255, 255, 255 ) );
      BitBlt( hdc, prc->left + ( ( ( prc->right - prc->left ) - dxBitmap ) / 2 ),
                   prc->top + dyBmpTop,
                   dxBitmap,
                   dyBitmap,
              hdcPic, 0, 0, SRCPAINT );
      }

   /* Finally, draw the string.
    * The font should have been selected into the DC before this
    * function was called.
    */
   if( pStr )
      {
      int oldmode;
      SIZE size;

      GetTextExtentPoint( hdc, pStr, lstrlen( pStr ), &size );
      oldmode = SetBkMode( hdc, TRANSPARENT );
      if( !fEnabled )
         {
         COLORREF crText;

         crText = SetTextColor( hdc, GetSysColor( COLOR_BTNHIGHLIGHT ) );
         if( fWrapText )
            {
            RECT rcText;

            SetRect( &rcText, prc->left, prc->top + dyStrTop, prc->left + dxMaxStrWidth, 0 );
            rcText.bottom = prc->top + dyStrTop + dyMaxStrHeight;
         #ifdef WIN32
            DrawText( hdc, pStr, -1, &rcText, DT_WORDBREAK|DT_CENTER|DT_END_ELLIPSIS );
            OffsetRect( &rcText, -1, -1 );
            SetTextColor( hdc, GetSysColor( COLOR_BTNSHADOW ) );
            DrawText( hdc, pStr, -1, &rcText, DT_WORDBREAK|DT_CENTER|DT_END_ELLIPSIS );
         #else
            DrawText( hdc, pStr, -1, &rcText, DT_WORDBREAK|DT_CENTER );
            OffsetRect( &rcText, -1, -1 );
            SetTextColor( hdc, GetSysColor( COLOR_BTNSHADOW ) );
            DrawText( hdc, pStr, -1, &rcText, DT_WORDBREAK|DT_CENTER );
         #endif
            }
         else
            {
            TextOut( hdc, prc->left + ( ( ( prc->right - prc->left ) - size.cx ) / 2 ) + 1, prc->top + dyStrTop + 1, pStr, lstrlen( pStr ) );
            SetTextColor( hdc, GetSysColor( COLOR_BTNSHADOW ) );
            TextOut( hdc, prc->left + ( ( ( prc->right - prc->left ) - size.cx ) / 2 ), prc->top + dyStrTop, pStr, lstrlen( pStr ) );
            }
         SetTextColor( hdc, crText );
         }
      else if( fWrapText )
         {
         RECT rcText;

         SetRect( &rcText, prc->left, prc->top + dyStrTop, prc->left + dxMaxStrWidth, 0 );
         rcText.bottom = prc->top + dyStrTop + dyMaxStrHeight;
      #ifdef WIN32
         DrawText( hdc, pStr, -1, &rcText, DT_WORDBREAK|DT_CENTER|DT_END_ELLIPSIS );
      #else
         DrawText( hdc, pStr, -1, &rcText, DT_WORDBREAK|DT_CENTER );
      #endif
         }
      else
         TextOut( hdc, prc->left + ( ( ( prc->right - prc->left ) - size.cx ) / 2 ), prc->top + dyStrTop, pStr, lstrlen( pStr ) );
      SetBkMode( hdc, oldmode );
      }

   /* Clean up afterwards.
    */
   SelectObject( hdcMask, hbmpMaskOld );
   SelectObject( hdcPic, hbmpPicOld );
   DeleteDC( hdcMask );
   DeleteDC( hdcPic );
   DeleteBitmap( hbmpMask );
}

/* This function creates the default font for the toolbar strings
 */
static void FASTCALL CreateToolbarFont( TOOLBAR FAR * pToolbar )
{
   HDC hdc;
   int cPixelsPerInch;
   int cyPixels;

   hdc = GetDC( NULL );
   ASSERT( hdc != NULL );
   cPixelsPerInch = GetDeviceCaps( hdc, LOGPIXELSY );
   cyPixels = -MulDiv( 8, cPixelsPerInch, 72 );
   pToolbar->hFont = CreateFont( cyPixels, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                                 FALSE, ANSI_CHARSET, OUT_CHARACTER_PRECIS, CLIP_DEFAULT_PRECIS,
                                 PROOF_QUALITY, VARIABLE_PITCH | FF_SWISS, "Helv" );
   pToolbar->hOwnedFont = pToolbar->hFont;
   ReleaseDC( NULL, hdc );
}

/* This function updates the toolbar label font height.
 */
static void FASTCALL UpdateLabelFontHeight( HWND hwnd, TOOLBAR FAR * pToolbar )
{
   HFONT hOldFont;
   TEXTMETRIC tm;
   HDC hdc;

   hdc = GetDC( hwnd );
   hOldFont = SelectFont( hdc, pToolbar->hFont );
   GetTextMetrics( hdc, &tm );
   pToolbar->dyLabelLine = tm.tmHeight;
   SelectFont( hdc, hOldFont );
   ReleaseDC( hwnd, hdc );
}

/* This function returns a pointer to the string associated with the
 * specified button. It is NULL if there is no string list or the button
 * has a string ID that does not correspond to a string in the string
 * list.
 */
static LPSTR FASTCALL GetButtonString( HWND hwnd, TOOLBAR FAR * pToolbar, TBBUTTON FAR * ptb, char * pStr )
{
   STRINGLIST FAR * pStringList;
   register int c;

   *pStr = '\0';
   if( NULL != pToolbar->pStringList )
      if( ptb->fsStyle & TBSTYLE_CALLBACK )
         {
         TBCALLBACK tbc;

         tbc.tb = *ptb;
         tbc.flag = TBCBF_WANTSTRING;
         tbc.pStr = NULL;
         Amctl_SendNotify( GetParent( hwnd ), hwnd, TBN_CALLBACK, (LPNMHDR)&tbc );
         strcpy( pStr, tbc.pStr );
         }
      else if( pStringList = pToolbar->pStringList )
         {
         /* Find the stringlist containing this string.
          */
         c = 0;
         while( pStringList && pStringList->cStrings + c < ptb->iString )
            {
            c += pStringList->cStrings;
            pStringList = pStringList->pNextStringList;
            }
      
         /* Extract the string from pStringList.
          */
         if( pStringList )
            {
            char * pStrInList;

            c = ptb->iString - c;
            pStrInList = pStringList->pStr;
            while( c-- && *pStrInList )
               pStrInList += lstrlen( pStrInList ) + 1;
            strcpy( pStr, pStrInList );
            }
         }
   return( *pStr ? pStr : NULL );
}
