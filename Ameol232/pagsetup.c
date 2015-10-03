/* PAGSETUP.C - Implements the Page Setup dialog
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

#define  THIS_FILE   __FILE__

#include "main.h"
#include "palrc.h"
#include <stdio.h>
#include "resource.h"
#include <string.h>
#include "print.h"
#include "amlib.h"
#include "tti.h"
#include "editmap.h"

static BOOL fDefDlgEx = FALSE;   /* DefDlg recursion flag trap */

float nLeftMargin;               /* Left margin, in millimetres */
float nRightMargin;              /* Right margin, in millimetres */
float nTopMargin;                /* Top margin, in millimetres */
float nBottomMargin;             /* Bottom margin, in millimetres */
int nOrientation;                /* Page orientation */
int nPrinterFont;                /* Printer font source */

static HDC hdcPr;                /* Display context for printer */
static RECT rcPortrait;          /* Portrait orientation */
static RECT rcLandscape;         /* Portrait orientation */

static HWND hwndActiveHdrEdit;   /* Active header edit pane */
static HWND hwndActiveFtrEdit;   /* Active footer edit pane */

BOOL fPageSetupDialogActive = FALSE;      /* TRUE if Page Setup dialog is open */

#define  HFM_FIRST      10
#define  HFM_PAGE       10
#define  HFM_DATE       11
#define  HFM_TIME       12
#define  HFM_LAST       12

struct tagMARKERLIST
   {
   char * pDescription;          /* Description of this marker */
   char * pTag;                  /* Tag field associated with marker */
   UINT wID;                     /* ID of this marker */
} FAR MarkerList[] = {
   "Page Number",    "{Page}",      HFM_PAGE,
   "System Date",    "{Date}",      HFM_DATE,
   "System Time",    "{Time}",      HFM_TIME
   };

#define  cbMarkerList   (sizeof(MarkerList)/sizeof(MarkerList[0]))

static HFONT hDlg6Font = NULL;      /* 6pt dialog font */

/* Array of controls that have tooltips and their string
 * resource IDs.
 */
static TOOLTIPITEMS tti[] = {
   IDD_PAGE,         IDS_STR1126,
   IDD_DATE,         IDS_STR1127,
   IDD_TIME,         IDS_STR1128
   };
#define  ctti     (sizeof(tti)/sizeof(TOOLTIPITEMS))

LPSTR lpLeftHeader;
LPSTR lpCenterHeader;
LPSTR lpRightHeader;
LPSTR lpLeftFooter;
LPSTR lpCenterFooter;
LPSTR lpRightFooter;

void FASTCALL ShowOrientation( HWND, RECT * );
void FASTCALL InsertMarker( HWND, UINT );
LPSTR FASTCALL GetAndAllocPPString( LPCSTR, LPSTR, LPSTR );
void FASTCALL UpdateHeader( HWND );
void FASTCALL UpdateFooter( HWND );
void FASTCALL ExpandMarker( LPSTR, PRINTSTRUCT FAR *, LPSTR );
void FASTCALL PaintFooter( HWND, HDC, const RECT FAR * );
void FASTCALL PaintHeader( HWND, HDC, const RECT FAR * );
void FASTCALL ReadHeaderFooterSettings( HWND, LPSTR *, LPSTR *, LPSTR * );
void FASTCALL CvtEOLToBreak( LPSTR );
void FASTCALL CvtBreakToEOL( LPSTR );

BOOL EXPORT CALLBACK PageSetup_Margins( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL PageSetup_Margins_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL PageSetup_Margins_OnCommand( HWND, int, HWND, UINT );
BOOL FASTCALL PageSetup_Margins_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL PageSetup_Margins_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
LRESULT FASTCALL PageSetup_Margins_OnNotify( HWND, int, NMHDR FAR * );

BOOL EXPORT CALLBACK PageSetup_Headers( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL PageSetup_Headers_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL PageSetup_Headers_OnCommand( HWND, int, HWND, UINT );
BOOL FASTCALL PageSetup_Headers_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL PageSetup_Headers_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
LRESULT FASTCALL PageSetup_Headers_OnNotify( HWND, int, NMHDR FAR * );

BOOL EXPORT CALLBACK PageSetup_Footers( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL PageSetup_Footers_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL PageSetup_Footers_OnCommand( HWND, int, HWND, UINT );
BOOL FASTCALL PageSetup_Footers_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL PageSetup_Footers_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
LRESULT FASTCALL PageSetup_Footers_OnNotify( HWND, int, NMHDR FAR * );

BOOL EXPORT CALLBACK PageSetup_Font( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL PageSetup_Font_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL PageSetup_Font_OnCommand( HWND, int, HWND, UINT );
BOOL FASTCALL PageSetup_Font_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL PageSetup_Font_OnNotify( HWND, int, NMHDR FAR * );

/* This function initialises the page setup default settings.
 */
BOOL FASTCALL PageSetupInit( void )
{
   /* First get the margin settings.
    */
   nLeftMargin = Amuser_GetPPFloat( szPrinter, "LeftMargin", 1.91f );;
   nRightMargin = Amuser_GetPPFloat( szPrinter, "RightMargin", 1.91f );
   nTopMargin = Amuser_GetPPFloat( szPrinter, "TopMargin", 2.54f );
   nBottomMargin = Amuser_GetPPFloat( szPrinter, "BottomMargin", 2.54f );
   nOrientation = Amuser_GetPPInt( szPrinter, "Orientation", PR_PORTRAIT );

   /* Next, get the header/footer settings.
    */
   lpLeftHeader = GetAndAllocPPString( szPrinter, "LeftHeader", "" );
   lpCenterHeader = GetAndAllocPPString( szPrinter, "CenterHeader", "" );
   lpRightHeader = GetAndAllocPPString( szPrinter, "RightHeader", "" );
   lpLeftFooter = GetAndAllocPPString( szPrinter, "LeftFooter", "" );
   lpCenterFooter = GetAndAllocPPString( szPrinter, "CenterFooter", "Page {Page}" );
   lpRightFooter = GetAndAllocPPString( szPrinter, "RightFooter", "" );

   /* Get the font option.
    */
   nPrinterFont = Amuser_GetPPInt( szPrinter, "FontSource", PFON_PRINTERFONT );

   /* Set orientation in the devmode.
    */
   SetOrientation();
   return( TRUE );
}

/* This function cleans up page setup stuff
 */
void FASTCALL PageSetupTerm( void )
{
   FreeMemory( &lpLeftHeader );
   FreeMemory( &lpCenterHeader );
   FreeMemory( &lpRightHeader );
   FreeMemory( &lpLeftFooter );
   FreeMemory( &lpCenterFooter );
   FreeMemory( &lpRightFooter );
}

/* Gets and allocates memory for a string read from the profile.
 */
LPSTR FASTCALL GetAndAllocPPString( LPCSTR lpszSection, LPSTR lpszKey, LPSTR lpszDefault )
{
   LPSTR lpszValue;

   INITIALISE_PTR(lpszValue);
   Amuser_GetPPString( lpszSection, lpszKey, lpszDefault, lpTmpBuf, LEN_TEMPBUF );
   if( fNewMemory( &lpszValue, strlen( lpTmpBuf ) + 1 ) )
      strcpy( lpszValue, lpTmpBuf );
   CvtBreakToEOL( lpszValue );
   return( lpszValue );
}

/* Handles the Page Setup dialog.
 */
void FASTCALL CmdPageSetup( HWND hwnd )
{
   AMPROPSHEETPAGE psp[ 4 ];
   AMPROPSHEETHEADER psh;

   /* Create a 6pt font for the header/footer pages.
    */
   hDlg6Font = CreateFont( 12, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                           FALSE, ANSI_CHARSET, OUT_CHARACTER_PRECIS,
                           CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
                           VARIABLE_PITCH | FF_SWISS, "Arial" );

   psp[ 0 ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ 0 ].dwFlags = PSP_USETITLE;
   psp[ 0 ].hInstance = hInst;
   psp[ 0 ].pszTemplate = MAKEINTRESOURCE( IDDLG_PAGESETUP_MARGINS );
   psp[ 0 ].pszIcon = NULL;
   psp[ 0 ].pfnDlgProc = PageSetup_Margins;
   psp[ 0 ].pszTitle = "Margins";
   psp[ 0 ].lParam = 0L;

   psp[ 1 ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ 1 ].dwFlags = PSP_USETITLE;
   psp[ 1 ].hInstance = hInst;
   psp[ 1 ].pszTemplate = MAKEINTRESOURCE( IDDLG_PAGESETUP_HEADERS );
   psp[ 1 ].pszIcon = NULL;
   psp[ 1 ].pfnDlgProc = PageSetup_Headers;
   psp[ 1 ].pszTitle = "Headers";
   psp[ 1 ].lParam = 0L;

   psp[ 2 ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ 2 ].dwFlags = PSP_USETITLE;
   psp[ 2 ].hInstance = hInst;
   psp[ 2 ].pszTemplate = MAKEINTRESOURCE( IDDLG_PAGESETUP_HEADERS );
   psp[ 2 ].pszIcon = NULL;
   psp[ 2 ].pfnDlgProc = PageSetup_Footers;
   psp[ 2 ].pszTitle = "Footers";
   psp[ 2 ].lParam = 0L;

   psp[ 3 ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ 3 ].dwFlags = PSP_USETITLE;
   psp[ 3 ].hInstance = hInst;
   psp[ 3 ].pszTemplate = MAKEINTRESOURCE( IDDLG_PAGESETUP_FONT );
   psp[ 3 ].pszIcon = NULL;
   psp[ 3 ].pfnDlgProc = PageSetup_Font;
   psp[ 3 ].pszTitle = "Font";
   psp[ 3 ].lParam = 0L;

   psh.dwSize = sizeof( AMPROPSHEETHEADER );
   psh.dwFlags = PSH_PROPSHEETPAGE|PSH_HASHELP|PSH_NOAPPLYNOW;
   if( fMultiLineTabs )
      psh.dwFlags |= PSH_MULTILINETABS;
   psh.hwndParent = hwnd;
   psh.hInstance = hInst;
   psh.pszIcon = NULL;
   psh.pszCaption = (LPSTR)"Page Setup";
   psh.nPages = sizeof( psp ) / sizeof( AMPROPSHEETPAGE );
   psh.nStartPage = 0;
   psh.ppsp = (LPCAMPROPSHEETPAGE)&psp;

   Amctl_PropertySheet(&psh );
   if( hdcPr )
      DeleteDC( hdcPr );

   /* Delete the font.
    */
   DeleteFont( hDlg6Font );
}

/* Handles the Page Setup dialog.
 */
BOOL EXPORT CALLBACK PageSetup_Margins( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, PageSetup_Margins_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the About dialog.
 */
LRESULT FASTCALL PageSetup_Margins_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, PageSetup_Margins_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, PageSetup_Margins_OnCommand );
      HANDLE_MSG( hwnd, WM_DRAWITEM, PageSetup_Margins_OnDrawItem );
      HANDLE_MSG( hwnd, WM_NOTIFY, PageSetup_Margins_OnNotify );

      case WM_USER+0x200: {
         HWND hwndCtl = (HWND)wParam;

         if( hwndCtl == NULL || ( hwndCtl != NULL && EditMap_GetModified( hwndCtl ) ) )
            {
            HWND hwndPagePic;

            hwndPagePic = GetDlgItem( hwnd, IDD_PAGEPICTURE );
            InvalidateRect( hwndPagePic, NULL, FALSE );
            UpdateWindow( hwndPagePic );
            if( hwndCtl != NULL )
               Edit_SetModify( hwndCtl, FALSE );
            PropSheet_Changed( GetParent( hwnd ), hwnd );
            }
         break;
         }
      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL PageSetup_Margins_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndPagePic;
   int iWidth;
   int iHeight;
   int dxDiff;
   int dyDiff;

   /* The Page Setup dialog refers to the current printer (as set
    * in the Print Setup dialog), so get a DC to it.
    */
   if( ( hdcPr = CreateCurrentPrinterDC() ) == NULL )
      {
      EndDialog( hwnd, FALSE );
      return( FALSE );
      }

   PageSetupInit(); // !!SM!! 2064

   /* Write the current margin settings.
    */
   SetDlgItemFloat( hwnd, IDD_LEFT, nLeftMargin );
   SetDlgItemFloat( hwnd, IDD_RIGHT, nRightMargin );
   SetDlgItemFloat( hwnd, IDD_TOP, nTopMargin );
   SetDlgItemFloat( hwnd, IDD_BOTTOM, nBottomMargin );

   /* The sample picture is initially in portrait mode,
    * so save its portrait rectangle and compute it's
    * landscape rectangle.
    */
   hwndPagePic = GetDlgItem( hwnd, IDD_PAGEPICTURE );
   GetWindowRect( hwndPagePic, &rcPortrait );
   MapWindowPoints( NULL, hwnd, (LPPOINT)&rcPortrait, 2 );
   CopyRect( &rcLandscape, &rcPortrait );
   iWidth = rcLandscape.bottom - rcLandscape.top;
   iHeight = rcLandscape.right - rcLandscape.left;
   dyDiff = ( ( rcLandscape.bottom - rcLandscape.top ) - iHeight ) / 2;
   dxDiff = ( ( rcLandscape.right - rcLandscape.left ) - iWidth ) / 2;
   rcLandscape.left += dxDiff;
   rcLandscape.top += dyDiff;
   rcLandscape.right = rcLandscape.left + iWidth;
   rcLandscape.bottom = rcLandscape.top + iHeight;

   /* Set orientation.
    */
   if( nOrientation == PR_LANDSCAPE )
      {
      CheckDlgButton( hwnd, IDD_LANDSCAPE, TRUE );
      ShowOrientation( hwnd, &rcLandscape );
      }
   else
      {
      CheckDlgButton( hwnd, IDD_PORTRAIT, TRUE );
      ShowOrientation( hwnd, &rcPortrait );
      }
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL PageSetup_Margins_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_LEFT:
      case IDD_TOP:
      case IDD_RIGHT:
      case IDD_BOTTOM:
         if( codeNotify == EN_KILLFOCUS )
            PostMessage( hwnd, WM_USER + 0x200, (WPARAM)hwndCtl, 0L );
         break;

      case IDD_LANDSCAPE:
         ShowOrientation( hwnd, &rcLandscape );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_PORTRAIT: {
         ShowOrientation( hwnd, &rcPortrait );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
         }
      }
}

/* This function shows the current page orientation
 */
void FASTCALL ShowOrientation( HWND hwnd, RECT * prc )
{
   HWND hwndPagePic;

   hwndPagePic = GetDlgItem( hwnd, IDD_PAGEPICTURE );
   SetWindowPos( hwndPagePic, NULL, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top,
                 SWP_NOZORDER|SWP_NOACTIVATE );
   PostMessage( hwnd, WM_USER + 0x200, 0, 0L );
}

/* This function handles the notification messages from the property
 * sheet dialog.
 */
LRESULT FASTCALL PageSetup_Margins_OnNotify( HWND hwnd, int iCode, NMHDR FAR * lpNmHdr )
{
   switch( lpNmHdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPAGESETUP_MARGINS );
         break;

      case PSN_APPLY: {
         float cxPixelsPerMM;
         float cyPixelsPerMM;
         float nLeftMarginT;
         float nRightMarginT;
         float nTopMarginT;
         float nBottomMarginT;

         /* Get the specified margin settings.
          */
         nLeftMarginT = GetDlgItemFloat( hwnd, IDD_LEFT );
         nRightMarginT = GetDlgItemFloat( hwnd, IDD_RIGHT );
         nTopMarginT = GetDlgItemFloat( hwnd, IDD_TOP );
         nBottomMarginT = GetDlgItemFloat( hwnd, IDD_BOTTOM );

         /* Check that the entered range is valid for the current printer.
          */
         if( hdcPr )
            {
            float nMinLeftMargin;
            float nMinRightMargin;
            float nMinTopMargin;
            float nMinBottomMargin;
            POINT offset;

            Escape( hdcPr, GETPRINTINGOFFSET, 0, NULL, &offset );
            cxPixelsPerMM = (float)GetDeviceCaps( hdcPr, HORZRES ) / (float)GetDeviceCaps( hdcPr, HORZSIZE );
            cyPixelsPerMM = (float)GetDeviceCaps( hdcPr, VERTRES ) / (float)GetDeviceCaps( hdcPr, VERTSIZE );
            nMinLeftMargin = (int)( ( offset.x * 10 ) / cxPixelsPerMM ) / (float)100.0;
            nMinTopMargin = (int)( ( offset.y * 10 ) / cyPixelsPerMM ) / (float)100.0;
            nMinRightMargin = (int)( ( offset.x * 10 ) / cxPixelsPerMM ) / (float)100.0;
            nMinBottomMargin = (int)( ( offset.y * 10 ) / cyPixelsPerMM ) / (float)100.0;
            if ( nLeftMarginT < nMinLeftMargin || nTopMarginT < nMinTopMargin ||
                 nRightMarginT < nMinRightMargin || nBottomMarginT < nMinBottomMargin )
               {
               // wsprintf() won't handle floating point format specifiers :-(
               sprintf( lpTmpBuf, GS(IDS_STR69),
                        (double)nMinLeftMargin,
                        (double)nMinTopMargin,
                        (double)nMinRightMargin,
                        (double)nMinBottomMargin );
               fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
               return( PSNRET_INVALID );
               }
            }
         Amuser_WritePPFloat( szPrinter, "LeftMargin", nLeftMargin = nLeftMarginT );
         Amuser_WritePPFloat( szPrinter, "RightMargin", nRightMargin = nRightMarginT );
         Amuser_WritePPFloat( szPrinter, "TopMargin", nTopMargin = nTopMarginT );
         Amuser_WritePPFloat( szPrinter, "BottomMargin", nBottomMargin = nBottomMarginT );

         /* Get the orientation.
          */
         nOrientation = IsDlgButtonChecked( hwnd, IDD_PORTRAIT ) ? PR_PORTRAIT : PR_LANDSCAPE;
         Amuser_WritePPInt( szPrinter, "Orientation", nOrientation );
         SetOrientation();

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

void FASTCALL PageSetup_Margins_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItemStruct )
{
   float nLeftMarginT;
   float nRightMarginT;
   float nTopMarginT;
   float nBottomMarginT;
   int nPageWidth;
   int nPageHeight;
   int x1, y1;
   int x2, y2;
   RECT rc;
   HBRUSH hbr;
   HPEN hpen;

   nLeftMarginT = GetDlgItemFloat( hwnd, IDD_LEFT ) * 10;
   nRightMarginT = GetDlgItemFloat( hwnd, IDD_RIGHT ) * 10;
   nTopMarginT = GetDlgItemFloat( hwnd, IDD_TOP ) * 10;
   nBottomMarginT = GetDlgItemFloat( hwnd, IDD_BOTTOM ) * 10;

   nPageWidth = hdcPr ? GetDeviceCaps( hdcPr, HORZSIZE ) : 1000;
   nPageHeight = hdcPr ? GetDeviceCaps( hdcPr, VERTSIZE ) : 1000;

   nLeftMarginT = ( 100.0f / nPageWidth ) * nLeftMarginT;
   nRightMarginT = ( 100.0f / nPageWidth ) * nRightMarginT;
   nTopMarginT = ( 100.0f / nPageHeight ) * nTopMarginT;
   nBottomMarginT = ( 100.0f / nPageHeight ) * nBottomMarginT;

   rc = lpDrawItemStruct->rcItem;
   hbr = CreateSolidBrush( GetSysColor( COLOR_BTNFACE ) );
   FillRect( lpDrawItemStruct->hDC, &rc, hbr );
   DeleteBrush( hbr );

   InflateRect( &rc, -8, -8 );
   FrameRect( lpDrawItemStruct->hDC, &rc, GetStockBrush( BLACK_BRUSH ) );
   MoveToEx( lpDrawItemStruct->hDC, rc.left + 1, rc.bottom, NULL );
   LineTo( lpDrawItemStruct->hDC, rc.right, rc.bottom );
   LineTo( lpDrawItemStruct->hDC, rc.right, rc.top + 1 );

   InflateRect( &rc, -1, -1 );
   hbr = CreateSolidBrush( GetSysColor( COLOR_WINDOW ) );
   FillRect( lpDrawItemStruct->hDC, &rc, hbr );
   DeleteBrush( hbr );

   nPageWidth = rc.right - rc.left;
   nPageHeight = rc.bottom - rc.top;
   x1 = rc.left + (int)( ( nPageWidth / 100.0f ) * nLeftMarginT );
   y1 = rc.top + (int)( ( nPageHeight / 100.0f ) * nTopMarginT );
   x2 = rc.right - (int)( ( nPageWidth / 100.0f ) * nRightMarginT );
   y2 = rc.bottom - (int)( ( nPageHeight / 100.0f ) * nBottomMarginT );

   if( x1 < rc.left )   x1 = rc.left;
   if( x2 > rc.right )  x2 = rc.right;
   if( y1 < rc.top )    y1 = rc.top;
   if( y2 > rc.bottom ) y2 = rc.bottom;

   if( x1 < x2 && y1 < y2 )
      {
      HPEN hpenOld;

      hpen = CreatePen( PS_DOT, 1, 0L );
      hpenOld = SelectPen( lpDrawItemStruct->hDC, hpen );
      MoveToEx( lpDrawItemStruct->hDC, x1, y1, NULL );
      LineTo( lpDrawItemStruct->hDC, x1, y2 );
      LineTo( lpDrawItemStruct->hDC, x2, y2 );
      LineTo( lpDrawItemStruct->hDC, x2, y1 );
      LineTo( lpDrawItemStruct->hDC, x1, y1 );
      SelectPen( lpDrawItemStruct->hDC, hpenOld );
      DeletePen( hpen );
      }
}

/* Handles the Page Setup Headers page
 */
BOOL EXPORT CALLBACK PageSetup_Headers( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, PageSetup_Headers_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the About dialog.
 */
LRESULT FASTCALL PageSetup_Headers_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, PageSetup_Headers_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, PageSetup_Headers_OnCommand );
      HANDLE_MSG( hwnd, WM_DRAWITEM, PageSetup_Headers_OnDrawItem );
      HANDLE_MSG( hwnd, WM_NOTIFY, PageSetup_Headers_OnNotify );

      case WM_DESTROY:
         RemoveTooltipsFromWindow( hwnd );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL PageSetup_Headers_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndList;
   HWND hwndEdit;
   register int i;

   /* Then set the bitmaps for the picture buttons.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_PAGE ), hInst, IDB_PAGE );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_DATE ), hInst, IDB_DATE );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_TIME ), hInst, IDB_TIME );

   /* Fill the edit fields.
    */
   SetDlgItemText( hwnd, IDD_LEFT, lpLeftHeader );
   SetDlgItemText( hwnd, IDD_CENTER, lpCenterHeader );
   SetDlgItemText( hwnd, IDD_RIGHT, lpRightHeader );

   /* Let the edit fields use a non-bold font.
    */
   SetWindowFont( GetDlgItem( hwnd, IDD_LEFT ), hHelv8Font, FALSE );
   SetWindowFont( GetDlgItem( hwnd, IDD_CENTER ), hHelv8Font, FALSE );
   SetWindowFont( GetDlgItem( hwnd, IDD_RIGHT ), hHelv8Font, FALSE );

   /* Fill the combo box with the options on the
    * buttons.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   for( i = 0; i < cbMarkerList; ++i )
      {
      int index;

      index = ComboBox_InsertString( hwndList, -1, MarkerList[ i ].pDescription );
      ComboBox_SetItemData( hwndList, index, MarkerList[ i ].wID );
      }
   ComboBox_SetCurSel( hwndList, 0 );

   /* Add tooltips.
    */
   AddTooltipsToWindow( hwnd, ctti, (LPTOOLTIPITEMS)&tti );

   /* Set the focus to the Left edit box and make that the active
    * edit control (for the buttons, etc)
    */
   hwndEdit = GetDlgItem( hwnd, IDD_LEFT );
   hwndActiveHdrEdit = hwndEdit;
   SetFocus( hwndEdit );
   return( FALSE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL PageSetup_Headers_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_LEFT:
      case IDD_CENTER:
      case IDD_RIGHT:
         if( codeNotify == EN_SETFOCUS )
            hwndActiveHdrEdit = hwndCtl;
         else if( codeNotify == EN_CHANGE )
            {
            UpdateHeader( hwnd );
            PropSheet_Changed( GetParent( hwnd ), hwnd );
            }
         break;

      case IDD_PAGE:
         InsertMarker( hwndActiveHdrEdit, HFM_PAGE );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_TIME:
         InsertMarker( hwndActiveHdrEdit, HFM_TIME );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_DATE:
         InsertMarker( hwndActiveHdrEdit, HFM_DATE );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_ADD: {
         HWND hwndList;
         int index;
         UINT wID;

         /* Get the index of the selected item and use
          * that to insert the appropriate marker.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         index = ComboBox_GetCurSel( hwndList );
         ASSERT( index != CB_ERR );
         wID = (UINT)ComboBox_GetItemData( hwndList, index );
         ASSERT( wID >= HFM_FIRST && wID <= HFM_LAST );
         InsertMarker( hwndActiveHdrEdit, wID );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
         }
      }
}

/* This function handles the notification messages from the property
 * sheet dialog.
 */
LRESULT FASTCALL PageSetup_Headers_OnNotify( HWND hwnd, int iCode, NMHDR FAR * lpNmHdr )
{
   switch( lpNmHdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPAGESETUP_HEADERS );
         break;

      case PSN_APPLY: {
         /* Deallocate the existing header strings.
          */
         FreeMemory( &lpLeftHeader );
         FreeMemory( &lpCenterHeader );
         FreeMemory( &lpRightHeader );

         /* Read the new settings.
          */
         ReadHeaderFooterSettings( hwnd, &lpLeftHeader, &lpCenterHeader, &lpRightHeader );

         /* Convert newlines to break characters.
          */
         CvtEOLToBreak( lpLeftHeader );
         CvtEOLToBreak( lpCenterHeader );
         CvtEOLToBreak( lpRightHeader );

         /* Save them...
          */
         Amuser_WritePPString( szPrinter, "LeftHeader", lpLeftHeader );
         Amuser_WritePPString( szPrinter, "CenterHeader", lpCenterHeader );
         Amuser_WritePPString( szPrinter, "RightHeader", lpRightHeader );

         /* Convert newlines to break characters.
          */
         CvtBreakToEOL( lpLeftHeader );
         CvtBreakToEOL( lpCenterHeader );
         CvtBreakToEOL( lpRightHeader );

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This function handles drawing the sample display.
 */
void FASTCALL PageSetup_Headers_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItemStruct )
{
   PaintHeader( hwnd, lpDrawItemStruct->hDC, &lpDrawItemStruct->rcItem );
}

/* This function redraws the footer sample.
 */
void FASTCALL UpdateHeader( HWND hwnd )
{
   HWND hwndSample;
   RECT rc;
   HDC hdc;

   hwndSample = GetDlgItem( hwnd, IDD_HEADERPICTURE );
   GetClientRect( hwndSample, &rc );
   hdc = GetDC( hwndSample );
   PaintHeader( hwnd, hdc, &rc );
   ReleaseDC( hwndSample, hdc );
}

/* This function paints the header control.
 */
void FASTCALL PaintHeader( HWND hwnd, HDC hdc, const RECT FAR * lprc )
{
   HBRUSH hbr;
   HPEN hpenBlack;
   HPEN hpenOld;
   HFONT hOldFont;
   RECT rc;
   LPSTR lpLeft;
   LPSTR lpRight;
   LPSTR lpCenter;
   PRINTSTRUCT ps;
   int cyHdr;

   /* First read the current settings from the edit
    * controls.
    */
   ReadHeaderFooterSettings( hwnd, &lpLeft, &lpCenter, &lpRight );

   /* Draw the frame.
    */
   rc = *lprc;
   hpenBlack = CreatePen( PS_SOLID, 0, GetSysColor( COLOR_WINDOWTEXT ) );
   hpenOld = SelectPen( hdc, hpenBlack );
   MoveToEx( hdc, 0, rc.bottom - 1, NULL );
   LineTo( hdc, 0, rc.top );
   LineTo( hdc, rc.right - 2, rc.top );
   LineTo( hdc, rc.right - 2, rc.bottom );
   MoveToEx( hdc, rc.right - 1, rc.top + 1, NULL );
   LineTo( hdc, rc.right - 1, rc.bottom );

   /* Fill the sample area with the window colour.
    */
   rc.left += 1;
   rc.top += 1;
   rc.right -= 2;
   hbr = CreateSolidBrush( GetSysColor( COLOR_WINDOW ) );
   FillRect( hdc, &rc, hbr );
   DeleteBrush( hbr );

   /* Deflate the rectangle for drawing the footer details.
    */
   rc.left += 3;
   rc.right -= 3;
   rc.top += 3;

   /* Create a dummy PRINTSTRUCT.
    */
   ps.nPage = 1;

   /* Select the 6pt font into the DC.
    */
   hOldFont = SelectFont( hdc, hDlg6Font );
   SetTextColor( hdc, GetSysColor( COLOR_WINDOWTEXT ) );
   SetBkColor( hdc, GetSysColor( COLOR_WINDOW ) );
   cyHdr = DrawHeader( hdc, &rc, &ps, lpLeft, lpCenter, lpRight, TRUE );
   rc.top = rc.top + ( ( ( rc.bottom - rc.top ) - cyHdr ) / 2 );
   rc.bottom = rc.top + cyHdr;
   DrawHeader( hdc, &rc, &ps, lpLeft, lpCenter, lpRight, FALSE );
   SelectFont( hdc, hOldFont );

   /* Deallocate the strings.
    */
   if( lpLeft )   FreeMemory( &lpLeft );
   if( lpCenter ) FreeMemory( &lpCenter );
   if( lpRight )  FreeMemory( &lpRight );

   /* Clean up afterwards.
    */
   SelectPen( hdc, hpenOld );
   DeletePen( hpenBlack );
}

/* Handles the Page Setup Footers page
 */
BOOL EXPORT CALLBACK PageSetup_Footers( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, PageSetup_Footers_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the About dialog.
 */
LRESULT FASTCALL PageSetup_Footers_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, PageSetup_Footers_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, PageSetup_Footers_OnCommand );
      HANDLE_MSG( hwnd, WM_DRAWITEM, PageSetup_Footers_OnDrawItem );
      HANDLE_MSG( hwnd, WM_NOTIFY, PageSetup_Footers_OnNotify );

      case WM_DESTROY:
         RemoveTooltipsFromWindow( hwnd );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL PageSetup_Footers_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndList;
   HWND hwndEdit;
   register int i;

   /* Then set the bitmaps for the picture buttons.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_PAGE ), hInst, IDB_PAGE );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_DATE ), hInst, IDB_DATE );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_TIME ), hInst, IDB_TIME );

   /* Fill the edit fields.
    */
   SetDlgItemText( hwnd, IDD_LEFT, lpLeftFooter );
   SetDlgItemText( hwnd, IDD_CENTER, lpCenterFooter );
   SetDlgItemText( hwnd, IDD_RIGHT, lpRightFooter );

   /* Let the edit fields use a non-bold font.
    */
   SetWindowFont( GetDlgItem( hwnd, IDD_LEFT ), hHelv8Font, FALSE );
   SetWindowFont( GetDlgItem( hwnd, IDD_CENTER ), hHelv8Font, FALSE );
   SetWindowFont( GetDlgItem( hwnd, IDD_RIGHT ), hHelv8Font, FALSE );

   /* Fill the combo box with the options on the
    * buttons.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   for( i = 0; i < cbMarkerList; ++i )
      {
      int index;

      index = ComboBox_InsertString( hwndList, -1, MarkerList[ i ].pDescription );
      ComboBox_SetItemData( hwndList, index, MarkerList[ i ].wID );
      }
   ComboBox_SetCurSel( hwndList, 0 );

   /* Add tooltips.
    */
   AddTooltipsToWindow( hwnd, ctti, (LPTOOLTIPITEMS)&tti );

   /* Set the focus to the Left edit box and make that the active
    * edit control (for the buttons, etc)
    */
   hwndEdit = GetDlgItem( hwnd, IDD_LEFT );
   hwndActiveHdrEdit = hwndEdit;
   SetFocus( hwndEdit );
   return( FALSE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL PageSetup_Footers_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_LEFT:
      case IDD_CENTER:
      case IDD_RIGHT:
         if( codeNotify == EN_SETFOCUS )
            hwndActiveFtrEdit = hwndCtl;
         else if( codeNotify == EN_CHANGE )
            {
            UpdateFooter( hwnd );
            PropSheet_Changed( GetParent( hwnd ), hwnd );
            }
         break;

      case IDD_PAGE:
         InsertMarker( hwndActiveFtrEdit, HFM_PAGE );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_TIME:
         InsertMarker( hwndActiveFtrEdit, HFM_TIME );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_DATE:
         InsertMarker( hwndActiveFtrEdit, HFM_DATE );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_ADD: {
         HWND hwndList;
         int index;
         UINT wID;

         /* Get the index of the selected item and use
          * that to insert the appropriate marker.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         index = ComboBox_GetCurSel( hwndList );
         ASSERT( index != CB_ERR );
         wID = (UINT)ComboBox_GetItemData( hwndList, index );
         ASSERT( wID >= HFM_FIRST && wID <= HFM_LAST );
         InsertMarker( hwndActiveFtrEdit, wID );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
         }
      }
}

/* This function handles the notification messages from the property
 * sheet dialog.
 */
LRESULT FASTCALL PageSetup_Footers_OnNotify( HWND hwnd, int iCode, NMHDR FAR * lpNmHdr )
{
   switch( lpNmHdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPAGESETUP_HEADERS );
         break;

      case PSN_APPLY: {
         /* Deallocate the existing Footer strings.
          */
         FreeMemory( &lpLeftFooter );
         FreeMemory( &lpCenterFooter );
         FreeMemory( &lpRightFooter );

         /* Read the new settings.
          */
         ReadHeaderFooterSettings( hwnd, &lpLeftFooter, &lpCenterFooter, &lpRightFooter );

         /* Convert newlines to break characters.
          */
         CvtEOLToBreak( lpLeftFooter );
         CvtEOLToBreak( lpCenterFooter );
         CvtEOLToBreak( lpRightFooter );

         /* Save them...
          */
         Amuser_WritePPString( szPrinter, "LeftFooter", lpLeftFooter );
         Amuser_WritePPString( szPrinter, "CenterFooter", lpCenterFooter );
         Amuser_WritePPString( szPrinter, "RightFooter", lpRightFooter );

         /* Convert newlines to break characters.
          */
         CvtBreakToEOL( lpLeftFooter );
         CvtBreakToEOL( lpCenterFooter );
         CvtBreakToEOL( lpRightFooter );

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This function reads the strings from the three edit controls for the
 * left, center and right header and footers and stores them in allocated
 * memory blocks.
 */
void FASTCALL ReadHeaderFooterSettings( HWND hwnd, LPSTR * lppLeft, LPSTR * lppCenter, LPSTR * lppRight )
{
   HWND hwndEdit;
   LPSTR lpTmp;
   UINT cch;

   /* Now allocate the new left Footer.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_LEFT );
   cch = Edit_GetTextLength( hwndEdit );
   INITIALISE_PTR(lpTmp);
   if( fNewMemory( &lpTmp, cch + 1 ) )
      {
      Edit_GetText( hwndEdit, lpTmp, cch + 1 );
      *lppLeft = lpTmp;
      }

   /* Now allocate the new center Footer.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_CENTER );
   cch = Edit_GetTextLength( hwndEdit );
   INITIALISE_PTR(lpTmp);
   if( fNewMemory( &lpTmp, cch + 1 ) )
      {
      Edit_GetText( hwndEdit, lpTmp, cch + 1 );
      *lppCenter = lpTmp;
      }

   /* Now allocate the new right Footer.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_RIGHT );
   cch = Edit_GetTextLength( hwndEdit );
   INITIALISE_PTR(lpTmp);
   if( fNewMemory( &lpTmp, cch + 1 ) )
      {
      Edit_GetText( hwndEdit, lpTmp, cch + 1 );
      *lppRight = lpTmp;
      }
}

/* This function handles drawing the sample display.
 */
void FASTCALL PageSetup_Footers_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItemStruct )
{
   PaintFooter( hwnd, lpDrawItemStruct->hDC, &lpDrawItemStruct->rcItem );
}

/* This function redraws the footer sample.
 */
void FASTCALL UpdateFooter( HWND hwnd )
{
   HWND hwndSample;
   RECT rc;
   HDC hdc;

   hwndSample = GetDlgItem( hwnd, IDD_HEADERPICTURE );
   GetClientRect( hwndSample, &rc );
   hdc = GetDC( hwndSample );
   PaintFooter( hwnd, hdc, &rc );
   ReleaseDC( hwndSample, hdc );
}

/* This function paints the footer control.
 */
void FASTCALL PaintFooter( HWND hwnd, HDC hdc, const RECT FAR * lprc )
{
   PRINTSTRUCT ps;
   HBRUSH hbr;
   HPEN hpenBlack;
   HPEN hpenOld;
   HFONT hOldFont;
   RECT rc;
   LPSTR lpLeft;
   LPSTR lpRight;
   LPSTR lpCenter;
   int cyHdr;

   /* First read the current settings from the edit
    * controls.
    */
   ReadHeaderFooterSettings( hwnd, &lpLeft, &lpCenter, &lpRight );

   /* Draw the frame.
    */
   rc = *lprc;
   hpenBlack = CreatePen( PS_SOLID, 0, GetSysColor( COLOR_WINDOWTEXT ) );
   hpenOld = SelectPen( hdc, hpenBlack );
   MoveToEx( hdc, 0, rc.top, NULL );
   LineTo( hdc, 0, rc.bottom - 2 );
   LineTo( hdc, rc.right - 2, rc.bottom - 2 );
   LineTo( hdc, rc.right - 2, rc.top - 1 );
   MoveToEx( hdc, rc.right - 1, rc.top + 1, NULL );
   LineTo( hdc, rc.right - 1, rc.bottom - 1 );
   LineTo( hdc, rc.left + 1, rc.bottom - 1 );

   /* Fill the sample area with the window colour.
    */
   rc.left += 1;
   rc.bottom -= 2;
   rc.right -= 2;
   hbr = CreateSolidBrush( GetSysColor( COLOR_WINDOW ) );
   FillRect( hdc, &rc, hbr );
   DeleteBrush( hbr );

   /* Deflate the rectangle for drawing the footer details.
    */
   rc.left += 3;
   rc.right -= 3;
   rc.bottom -= 6;

   /* Create a dummy PRINTSTRUCT.
    */
   ps.nPage = 1;

   /* Select the 6pt font into the DC.
    */
   hOldFont = SelectFont( hdc, hDlg6Font );
   SetTextColor( hdc, GetSysColor( COLOR_WINDOWTEXT ) );
   SetBkColor( hdc, GetSysColor( COLOR_WINDOW ) );
   cyHdr = DrawFooter( hdc, &rc, &ps, lpLeft, lpCenter, lpRight, TRUE );
   rc.top = rc.top + ( ( ( rc.bottom - rc.top ) - cyHdr ) / 2 );
   rc.bottom = rc.top + cyHdr;
   DrawFooter( hdc, &rc, &ps, lpLeft, lpCenter, lpRight, FALSE );
   SelectFont( hdc, hOldFont );

   /* Deallocate the strings.
    */
   if( lpLeft )   FreeMemory( &lpLeft );
   if( lpCenter ) FreeMemory( &lpCenter );
   if( lpRight )  FreeMemory( &lpRight );

   /* Clean up afterwards.
    */
   SelectPen( hdc, hpenOld );
   DeletePen( hpenBlack );
}

/* This function inserts the specified marker at the caret position
 * in the active edit control.
 */
void FASTCALL InsertMarker( HWND hwnd, UINT wID )
{  
   register int c;

   for( c = 0; c < cbMarkerList; ++c )
      if( MarkerList[ c ].wID == wID )
         {
         Edit_ReplaceSel( hwnd, MarkerList[ c ].pTag );
         SetFocus( hwnd );
         break;
         }
   ASSERT( c < cbMarkerList );
}

/* This function parses the header and draws the current header in the
 * specified rectangle. The font used for drawing must have been selected
 * into the DC before calling this function.
 */
int FASTCALL DrawHeader( HDC hdc, RECT * prc, PRINTSTRUCT FAR * lpps, LPSTR lpLeft, LPSTR lpCenter, LPSTR lpRight, BOOL fCalc )
{
   int cyLeft;
   int cyCenter;
   int cyRight;
   UINT dtFlags;
   RECT rc;

   /* If we're calculating the height, set the
    * DT_CALCRECT flag.
    */
   dtFlags = fCalc ? DT_CALCRECT|DT_NOPREFIX : DT_NOPREFIX;

   /* Draw the left part of the header.
    */
   ExpandMarker( lpLeft, lpps, lpTmpBuf );
   rc = *prc;
   cyLeft = DrawText( hdc, lpTmpBuf, -1, &rc, DT_LEFT|dtFlags );

   /* Draw the center part of the header.
    */
   ExpandMarker( lpCenter, lpps, lpTmpBuf );
   rc = *prc;
   cyCenter = DrawText( hdc, lpTmpBuf, -1, &rc, DT_CENTER|dtFlags );

   /* Draw the right part of the header.
    */
   ExpandMarker( lpRight, lpps, lpTmpBuf );
   rc = *prc;
   cyRight = DrawText( hdc, lpTmpBuf, -1, &rc, DT_RIGHT|dtFlags );
   return( max( max( cyLeft, cyCenter ), cyRight ) );
}

/* This function parses the header and draws the current header in the
 * specified rectangle. The font used for drawing must have been selected
 * into the DC before calling this function.
 */
int FASTCALL DrawFooter( HDC hdc, RECT * prc, PRINTSTRUCT FAR * lpps, LPSTR lpLeft, LPSTR lpCenter, LPSTR lpRight, BOOL fCalc )
{
   int cyLeft;
   int cyCenter;
   int cyRight;
   UINT dtFlags;
   RECT rc;

   /* If we're calculating the height, set the
    * DT_CALCRECT flag.
    */
   dtFlags = fCalc ? DT_CALCRECT|DT_NOPREFIX : DT_NOPREFIX;

   /* Draw the left part of the footer.
    */
   ExpandMarker( lpLeft, lpps, lpTmpBuf );
   rc = *prc;
   cyLeft = DrawText( hdc, lpTmpBuf, -1, &rc, DT_LEFT|dtFlags );

   /* Draw the center part of the footer.
    */
   ExpandMarker( lpCenter, lpps, lpTmpBuf );
   rc = *prc;
   cyCenter = DrawText( hdc, lpTmpBuf, -1, &rc, DT_CENTER|dtFlags );

   /* Draw the right part of the footer.
    */
   ExpandMarker( lpRight, lpps, lpTmpBuf );
   rc = *prc;
   cyRight = DrawText( hdc, lpTmpBuf, -1, &rc, DT_RIGHT|dtFlags );
   return( max( max( cyLeft, cyCenter ), cyRight ) );
}

/* This function expands a marker.
 */
void FASTCALL ExpandMarker( LPSTR lpszMarker, PRINTSTRUCT FAR * lpps, LPSTR lpszBuf )
{
   while( *lpszMarker )
      {
      if( *lpszMarker == '{' )
         {
         register int c;

         for( c = 0; c < cbMarkerList; ++c )
            if( _fstrnicmp( lpszMarker, MarkerList[ c ].pTag, strlen(MarkerList[ c ].pTag) ) == 0 )
               {
               AM_DATE date;
               AM_TIME time;

               lpszMarker += strlen(MarkerList[ c ].pTag );
               switch( MarkerList[ c ].wID )
                  {
                  case HFM_PAGE:
                     lpszBuf += wsprintf( lpszBuf, "%u", lpps->nPage );
                     break;

                  case HFM_DATE:
                     Amdate_GetCurrentDate( &date );
                     Amdate_FormatShortDate( &date, lpszBuf );
                     lpszBuf += lstrlen( lpszBuf );
                     break;

                  case HFM_TIME:
                     Amdate_GetCurrentTime( &time );
                     Amdate_FormatTime( &time, lpszBuf );
                     lpszBuf += lstrlen( lpszBuf );
                     break;
                  }
               break;
               }
         if( c == cbMarkerList )
            *lpszBuf++ = *lpszMarker++;
         }
      else
         *lpszBuf++ = *lpszMarker++;
      }
   *lpszBuf = '\0';
}

/* This function scans the string looking for newlines and
 * if any are found, they are converted to the ~~ characters.
 */
void FASTCALL CvtEOLToBreak( LPSTR lpHeader )
{
   while( *lpHeader )
      {
      if( *lpHeader == '\r' )
         *lpHeader++ = '~';
      if( *lpHeader == '\n' )
         *lpHeader++ = '~';
      ++lpHeader;
      }
}

/* This function scans the string looking for the ~~ sequence and
 * if found, converts it to the \r\n newline sequence.
 */
void FASTCALL CvtBreakToEOL( LPSTR lpHeader )
{
   while( *lpHeader )
      {
      if( *lpHeader == '~' )
         *lpHeader++ = '\r';
      if( *lpHeader == '~' )
         *lpHeader++ = '\n';
      ++lpHeader;
      }
}

/* Handles the Page Setup Font page
 */
BOOL EXPORT CALLBACK PageSetup_Font( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, PageSetup_Font_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the About dialog.
 */
LRESULT FASTCALL PageSetup_Font_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, PageSetup_Font_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, PageSetup_Font_OnCommand );
      HANDLE_MSG( hwnd, WM_NOTIFY, PageSetup_Font_OnNotify );

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL PageSetup_Font_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   CheckDlgButton( hwnd, IDD_DEFAULT, nPrinterFont == PFON_DEFAULT );
   CheckDlgButton( hwnd, IDD_WINDOW, nPrinterFont == PFON_WINDOW );
   CheckDlgButton( hwnd, IDD_SPECIFIC, nPrinterFont == PFON_PRINTERFONT );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL PageSetup_Font_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EDITFONT:
         fPageSetupDialogActive = TRUE;
         CustomiseDialog( GetParent( hwnd ), CDU_FONTS );
         fPageSetupDialogActive = FALSE;
         break;

      case IDD_DEFAULT:
      case IDD_WINDOW:
      case IDD_SPECIFIC:
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
      }
}

/* This function handles the notification messages from the property
 * sheet dialog.
 */
LRESULT FASTCALL PageSetup_Font_OnNotify( HWND hwnd, int iCode, NMHDR FAR * lpNmHdr )
{
   switch( lpNmHdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPAGESETUP_FONT );
         break;

      case PSN_APPLY: {
         if( IsDlgButtonChecked( hwnd, IDD_DEFAULT ) )
            nPrinterFont = PFON_DEFAULT;
         if( IsDlgButtonChecked( hwnd, IDD_WINDOW ) )
            nPrinterFont = PFON_WINDOW;
         if( IsDlgButtonChecked( hwnd, IDD_SPECIFIC ) )
            nPrinterFont = PFON_PRINTERFONT;
         Amuser_WritePPInt( szPrinter, "FontSource", nPrinterFont );

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}
