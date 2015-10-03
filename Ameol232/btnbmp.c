/* BTNBMP.C - Implements the button bitmaps interface
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
#include "resource.h"
#include "palrc.h"
#include "command.h"
#include <string.h>

#define  THIS_FILE   __FILE__

#define  CX_LARGE_BMP      24
#define  CY_LARGE_BMP      24
#define  CX_SMALL_BMP      16
#define  CY_SMALL_BMP      16

static BOOL fDefDlgEx = FALSE;         /* DefDlg recursion flag trap */

static BUTTONBMP * pbtnFirst = NULL;   /* Pointer to first button bitmap */

BOOL EXPORT CALLBACK ButtonBitmapDlg( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL ButtonBitmapDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL ButtonBitmapDlg_OnCommand( HWND, int, HWND, UINT );
void FASTCALL ButtonBitmapDlg_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
void FASTCALL ButtonBitmapDlg_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
LRESULT FASTCALL ButtonBitmapDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );

/* This function allows the user to change a button bitmap.
 */
BOOL FASTCALL CmdButtonBitmap( HWND hwnd, BUTTONBMP * pBtnBmp )
{
   return( Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_BUTTONBITMAP), ButtonBitmapDlg, (LPARAM)pBtnBmp ) );
}

/* This function handles the ButtonBitmapDlg dialog box
 */
BOOL EXPORT CALLBACK ButtonBitmapDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, ButtonBitmapDlg_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the Login
 * dialog.
 */
LRESULT FASTCALL ButtonBitmapDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, ButtonBitmapDlg_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, ButtonBitmapDlg_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_DRAWITEM, ButtonBitmapDlg_OnDrawItem );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsBUTTONBITMAP );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL ButtonBitmapDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   BUTTONBMP * pBtnBmp;
   register int i;
   HWND hwndList;
   int cxButton;
   int cyButton;

   /* Save the input parameter
    */
   ASSERT( 0L != lParam );
   SetWindowLong( hwnd, DWL_USER, lParam );
   pBtnBmp = (BUTTONBMP *)lParam;

   /* Set the item height and width based on one
    * button from the bitmap (all bitmaps are the
    * same width and height).
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   cxButton = fLargeButtons ? CX_LARGE_BMP : CX_SMALL_BMP;
   cyButton = fLargeButtons ? CY_LARGE_BMP : CY_SMALL_BMP;
   ListBox_SetItemHeight( hwndList, 0, cyButton + 6 );
   ListBox_SetColumnWidth( hwndList, cxButton + 6 );

   /* Fill the dialog with the number of items.
    */
   for( i = 0; i < 48; ++i )
      ListBox_AddString( hwndList, i );

   /* Select the appropriate bitmap.
    */
   ListBox_SetCurSel( hwndList, pBtnBmp->index );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL ButtonBitmapDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_RESET: {
         BUTTONBMP * pBtnBmp;

         /* Get the indexed bitmap item and simply set
          * the index field.
          */
         pBtnBmp = (BUTTONBMP *)GetWindowLong( hwnd, DWL_USER );
         pBtnBmp->fIsValid = FALSE;
         pBtnBmp->pszFilename = NULL;
         pBtnBmp->hbmpSmall = NULL;
         pBtnBmp->hbmpLarge = NULL;
         pBtnBmp->index = 0;
         EndDialog( hwnd, TRUE );
         break;
         }

      case IDOK: {
         BUTTONBMP * pBtnBmp;
         HWND hwndList;
         int index;

         /* Get the indexed bitmap item and simply set
          * the index field.
          */
         pBtnBmp = (BUTTONBMP *)GetWindowLong( hwnd, DWL_USER );
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         if( LB_ERR != ( index = ListBox_GetCurSel( hwndList ) ) )
            {
            pBtnBmp->fIsValid = TRUE;
            pBtnBmp->pszFilename = BTNBMP_GRID;
            pBtnBmp->hbmpSmall = NULL;
            pBtnBmp->hbmpLarge = NULL;
            pBtnBmp->index = index;
            EndDialog( hwnd, TRUE );
            }
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}

/* This function handles the WM_DRAWITEM message.
 */
void FASTCALL ButtonBitmapDlg_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   BUTTONBMP * pBtnBmp;
   BUTTONBMP btnBmp;
   HBITMAP hBmp;
   RECT rc;

   /* Prepare thyself.
    */
   rc = lpDrawItem->rcItem;

   /* Only do this if we're drawing the whole bitmap.
    */
   if( lpDrawItem->itemAction != ODA_FOCUS )
      {
      HBITMAP hBmpOld;
      BITMAP bmp;
      COLORREF B;
      HBRUSH hbr;
      HDC hDC;

      /* Extract the bitmap indexed by this item.
       */
      pBtnBmp = (BUTTONBMP *)GetWindowLong( hwnd, DWL_USER );
      btnBmp.pszFilename = BTNBMP_GRID;
      btnBmp.index = lpDrawItem->itemID;
      btnBmp.hbmpSmall = NULL;
      btnBmp.hbmpLarge = NULL;
      hBmp = GetButtonBitmapEntry( &btnBmp );

      /* Draw the item.
       */
      if( lpDrawItem->itemState & ODS_SELECTED )
         B = GetSysColor( COLOR_HIGHLIGHT );
      else
         B = GetSysColor( COLOR_WINDOW );

      /* Fill the drawing area with the selection or
       * background colour.
       */
      hbr = CreateSolidBrush( B );
      FillRect( lpDrawItem->hDC, &rc, hbr );
      DeleteBrush( hbr );

      /* Deflate a bit.
       */
      InflateRect( &rc, -3, -3 );

      /* Now draw the bitmap.
       */
      hDC = CreateCompatibleDC( lpDrawItem->hDC );
      hBmpOld = SelectBitmap( hDC, hBmp );
      GetObject( hBmp, sizeof(BITMAP), &bmp );
      BitBlt( lpDrawItem->hDC, rc.left, rc.top, bmp.bmWidth, bmp.bmHeight, hDC, 0, 0, SRCCOPY );
      SelectBitmap( hDC, hBmpOld );
      DeleteDC( hDC );
      InflateRect( &rc, 3, 3 );
      }

   /* Draw focus rectangle if required.
    */
   InflateRect( &rc, -1, -1 );
   if( lpDrawItem->itemState & ODS_FOCUS )
      DrawFocusRect( lpDrawItem->hDC, &rc );
}

/* This function sets the button bitmap entry for the specified
 * command to the given filename and index.
 */
void FASTCALL SetCommandCustomBitmap( CMDREC FAR * pcmd, LPSTR lpszFilename, int index )
{
   pcmd->fCustomBitmap = TRUE;
   pcmd->btnBmp.fIsValid = TRUE;
   pcmd->btnBmp.hbmpSmall = NULL;
   pcmd->btnBmp.hbmpLarge = NULL;
   pcmd->btnBmp.index = index;
   if( 0 == HIWORD( lpszFilename ) )
      pcmd->btnBmp.pszFilename = lpszFilename;
   else
      {
      INITIALISE_PTR( pcmd->btnBmp.pszFilename );
      if( fNewMemory( &pcmd->btnBmp.pszFilename, strlen(lpszFilename) + 1 ) )
         strcpy( pcmd->btnBmp.pszFilename, lpszFilename );
      }
}

/* This function returns the bitmap referenced by the specified
 * button bitmap entry.
 */
HBITMAP FASTCALL GetButtonBitmapEntry( BUTTONBMP * pBtnBmp )
{
   PICCTRLSTRUCT pcs;
   HBITMAP hbmpPic = NULL;
   HBITMAP hbmpPicOld;
   HBITMAP hbmpCpyOld;
   int cxButton;
   int cyButton;
   int xOffset;
   int yOffset;
   BITMAP bmp;
   HDC hdcPic;
   HDC hdcCpy;
   HDC hdc;

   pcs.hPalette = NULL;
   pcs.hBitmap = NULL;

   /* If button is cached, return it from the
    * cache.
    */
   if( pBtnBmp->hbmpSmall && !fLargeButtons )
      return( pBtnBmp->hbmpSmall );
   if( pBtnBmp->hbmpLarge && fLargeButtons )
      return( pBtnBmp->hbmpLarge );

   /* Not cached, so need to load it anew and then
    * cache it.
    */
   if( 0 == HIWORD( pBtnBmp->pszFilename ) )
      {
      LPCSTR pszGridBtns;

      /* Don't cache these.
       */
      pszGridBtns = fLargeButtons ? MAKEINTRESOURCE(IDB_GRID24) : MAKEINTRESOURCE(IDB_GRID16);
//    Amctl_LoadDIBBitmap( hRscLib, (LPSTR)pszGridBtns, &pcs );
      hbmpPic = LoadBitmap( hRscLib, MAKEINTRESOURCE( pszGridBtns ) );
      }
   else
      pcs.hBitmap = Amuser_LoadBitmapFromDisk( (LPSTR)pBtnBmp->pszFilename, &pcs.hPalette );

   /* Next, need to extract individual bitmap.
    */
   cxButton = fLargeButtons ? CX_LARGE_BMP : CX_SMALL_BMP;
   cyButton = fLargeButtons ? CY_LARGE_BMP : CY_SMALL_BMP;
   GetObject( pcs.hBitmap, sizeof(BITMAP), &bmp );
   yOffset = cyButton * ( pBtnBmp->index / 6 );
   xOffset = cxButton * ( pBtnBmp->index % 6 );

   /* Extract our bitmap to hbmpPic.
    */
   hdc = GetDC( hwndFrame );
   hdcPic = CreateCompatibleDC( hdc );
   hdcCpy = CreateCompatibleDC( hdc );
/* if( fNewButtons )
   {
   SelectPalette( hdcCpy, pcs.hPalette, FALSE );
   RealizePalette( hdcCpy );
   SelectPalette( hdcPic, pcs.hPalette, FALSE );
   RealizePalette( hdcPic );
   }
*/ hbmpCpyOld = SelectBitmap( hdcCpy, hbmpPic );
   hbmpPic = CreateCompatibleBitmap( hdcCpy, cxButton, cyButton );
   hbmpPicOld = SelectBitmap( hdcPic, hbmpPic );
   BitBlt( hdcPic, 0, 0, cxButton, cyButton, hdcCpy, xOffset, yOffset, SRCCOPY );
   SelectBitmap( hdcCpy, hbmpCpyOld );
   DeleteDC( hdcCpy );
   SelectBitmap( hdcPic, hbmpPicOld );
   DeleteDC( hdcPic );
   ReleaseDC( hwndFrame, hdc );

   /* Delete bitmap
    */
   if( NULL != pcs.hBitmap )
      DeleteBitmap( pcs.hBitmap );
   if( NULL != pcs.hPalette )
      DeletePalette( pcs.hPalette );

   /* Cache this new bitmap.
    */
   if( fLargeButtons )
      {
      ASSERT( NULL == pBtnBmp->hbmpLarge );
      pBtnBmp->hbmpLarge = hbmpPic;
      }
   else
      {
      ASSERT( NULL == pBtnBmp->hbmpSmall );
      pBtnBmp->hbmpSmall = hbmpPic;
      }

   /* Finally scale the button if required.
    */
   return( hbmpPic );
}
