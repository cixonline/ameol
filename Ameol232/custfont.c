/* CUSTFONT.C - Font Customisation dialog.
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

#include "main.h"
#include "palrc.h"
#include "resource.h"
#include "amlib.h"
#include <stdio.h>
#include <memory.h>
#include <dlgs.h>

#define  THIS_FILE   __FILE__

/* Font sizes for TrueType fonts
 */
static WORD wDefTTFontSize[] = { 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 36, 48, 72 };
#define TOTAL_DEF_TT_FONT_SIZES (sizeof(wDefTTFontSize)/sizeof(WORD))

/* Type style names for non-TT fonts
 */
static char npRegular[] = "Regular";
static char npBold[] = "Bold";
static char npItalic[] = "Italic";
static char npBoldItalic[] = "Bold Italic";

static BOOL fDefDlgEx = FALSE;

#define  cbFonts     15

/* Stuff used by the ChooseFont dialog box
 */
static int nCurSel;
static BOOL fFixedOnly = FALSE;
static DLGPROC lpfnFontDlgHook;
static LOGFONT FAR * lplfTmp = NULL;
static LOGFONT FAR * lplfSaved = NULL;

BOOL FontSettings_OnInitDialog( HWND, HWND, LPARAM );
void FontSettings_OnCommand( HWND, int, HWND, UINT );
LRESULT FontSettings_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FontSettings_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
void FontSettings_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
int FontSettings_OnCompareItem( HWND, const COMPAREITEMSTRUCT FAR * );
LRESULT FASTCALL FontSettings_OnNotify( HWND, int, LPNMHDR );

void FASTCALL FillFontStylesAndSizes( HWND );
void FASTCALL ReadCurrentFontSettings( HWND );
void FASTCALL WriteCurrentFontSettings( HWND );
void FASTCALL ApplyFont( int );
void FASTCALL PaintSample( HWND, HDC );
void FASTCALL ShowSelectedFontType( HWND, int );
LOGFONT FAR * FASTCALL SaveFontSettings( void );
void FASTCALL ApplyChanges( void );

void FASTCALL CreateAmeolFont( int, LOGFONT * );

int EXPORT CALLBACK FillFontDialog( const ENUMLOGFONT FAR *, const NEWTEXTMETRIC FAR*, int, LPARAM );
int EXPORT CALLBACK StylesAndSizesCallback( const ENUMLOGFONT FAR *, const NEWTEXTMETRIC FAR*, int, LPARAM );

/* This function handles the Font Settings dialog.
 */
BOOL EXPORT CALLBACK FontSettings( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, FontSettings_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the FontSettings
 * dialog.
 */
LRESULT FontSettings_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, FontSettings_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, FontSettings_OnCommand );
      HANDLE_MSG( hwnd, WM_DRAWITEM, FontSettings_OnDrawItem );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, FontSettings_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_COMPAREITEM, FontSettings_OnCompareItem );
      HANDLE_MSG( hwnd, WM_NOTIFY, FontSettings_OnNotify );
      
      case WM_PAINT: {
         PAINTSTRUCT ps;

         BeginPaint( hwnd, &ps );
         PaintSample( hwnd, ps.hdc );
         EndPaint( hwnd, &ps );
         break;
         }

      case WM_USER+0x202: {
         HWND hwndCmb;
         HDC hdc;

         hwndCmb = GetDlgItem( hwnd, IDD_FONTS );
         ComboBox_ResetContent( hwndCmb );
         SetWindowRedraw( hwndCmb, FALSE );
         hdc = GetDC( hwnd );
         EnumFontFamilies( hdc, NULL, (FONTENUMPROC)FillFontDialog, (LPARAM)(LPSTR)hwnd );
         ReleaseDC( hwnd, hdc );
         EnableControl( hwnd, IDD_FIXEDONLY, nCurSel != 2 );
         SetWindowRedraw( hwndCmb, TRUE );
         WriteCurrentFontSettings( hwnd );
         break;
         }

      case WM_USER+0x0201: {
         HDC hdc;

         ReadCurrentFontSettings( hwnd );
         hdc = GetDC( hwnd );
         PaintSample( hwnd, hdc );
         ReleaseDC( hwnd, hdc );
         break;
         }

      case WM_USER+0x0200: {
         HDC hdc;
         HWND hwndCmb;
         int index;

         /* Fill the list of font styles and sizes for the selected font
          */
         FillFontStylesAndSizes( hwnd );
         hwndCmb = GetDlgItem( hwnd, IDD_FONTS );
         index = ComboBox_GetCurSel( hwndCmb );
         ShowSelectedFontType( hwnd, index );

         /* Paint a new sample of the selected font in the given style and size
          */
         hdc = GetDC( hwnd );
         ReadCurrentFontSettings( hwnd );
         PaintSample( hwnd, hdc );
         ReleaseDC( hwnd, hdc );
         break;
         }

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FontSettings_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndList;
   HFONT hFont;
   TEXTMETRIC tm;
   HDC hdc;

   hwndList = GetDlgItem( hwnd, IDD_APPLYSOURCE );
   ListBox_InsertString( hwndList, -1, "Message Window" );
   ListBox_InsertString( hwndList, -1, "Edit Window" );
   ListBox_InsertString( hwndList, -1, "Default Telnet Window" );
   ListBox_InsertString( hwndList, -1, "Thread Window" );
   ListBox_InsertString( hwndList, -1, "Status Bar" );
   ListBox_InsertString( hwndList, -1, "In Basket" );
   ListBox_InsertString( hwndList, -1, "Newsgroups List" );
   ListBox_InsertString( hwndList, -1, "Fixed Font" );
   ListBox_InsertString( hwndList, -1, "Out Basket" );
   ListBox_InsertString( hwndList, -1, "CIX User List" );
   ListBox_InsertString( hwndList, -1, "CIX Forums List" );
   ListBox_InsertString( hwndList, -1, "Resume Window" );
   ListBox_InsertString( hwndList, -1, "Printer Font" );
   ListBox_InsertString( hwndList, -1, "Scheduler Window" );
   ListBox_InsertString( hwndList, -1, "Address Book" );

   /* Set the active font depending on which window has the
    * focus.
    */
   nCurSel = 0;
   fFixedOnly = FALSE;
   if( fPageSetupDialogActive )
      nCurSel = 12;
   else switch( iActiveMode )
      {
      case WIN_THREAD:     nCurSel = 3; break;
      case WIN_OUTBASK:    nCurSel = 8; break;
      case WIN_MESSAGE:    nCurSel = 0; break;
      case WIN_EDITS:      nCurSel = 1; break;
      case WIN_INBASK:     nCurSel = 5; break;
      case WIN_TERMINAL:   nCurSel = 2; break;
      case WIN_GRPLIST:    nCurSel = 6; break;
      case WIN_USERLIST:   nCurSel = 9; break;
      case WIN_CIXLIST:    nCurSel = 10; break;
      case WIN_RESUMES:    nCurSel = 11; break;
      case WIN_SCHEDULE:   nCurSel = 13; break;
      case WIN_ADDRBOOK:   nCurSel = 14; break;
      }
   ListBox_SetCurSel( hwndList, nCurSel );

   /* Set input field limits for the fonts.
    */
   ComboBox_LimitText( GetDlgItem( hwnd, IDD_FONTS ), LF_FACESIZE-1 );
   ComboBox_LimitText( GetDlgItem( hwnd, IDD_FONTSTYLES ), 20 );
   ComboBox_LimitText( GetDlgItem( hwnd, IDD_FONTSIZE ), 7 );

   /* Set the height of the two owner-draw boxes.
    */
   hdc = GetDC( hwnd );
   hFont = SelectFont( hdc, hHelvB8Font );
   GetTextMetrics( hdc, &tm );
   SelectFont( hdc, hFont );
   ReleaseDC( hwnd, hdc );
   if( wWinVer >= 0x035F )
      {
      ComboBox_SetItemHeight( GetDlgItem( hwnd, IDD_FONTS ), -1, tm.tmHeight+2 );
      ComboBox_SetItemHeight( GetDlgItem( hwnd, IDD_FONTSIZE ), -1, tm.tmHeight+2 );
      ComboBox_SetItemHeight( GetDlgItem( hwnd, IDD_FONTSTYLES ), -1, tm.tmHeight+2 );
      }
   else
      {
      ComboBox_SetItemHeight( GetDlgItem( hwnd, IDD_FONTS ), -1, tm.tmHeight+8 );
      ComboBox_SetItemHeight( GetDlgItem( hwnd, IDD_FONTSIZE ), -1, tm.tmHeight+8 );
      ComboBox_SetItemHeight( GetDlgItem( hwnd, IDD_FONTSTYLES ), -1, tm.tmHeight+8 );
      }

   /* Allocate a save buffer for the current font settings. We
    * will manipulate the copies.
    */
   lplfTmp = SaveFontSettings();
   lplfSaved = SaveFontSettings();
   if( NULL == lplfTmp || NULL == lplfSaved )
      return( FALSE );
   PostMessage( hwnd, WM_USER+0x202, 0, 0L );
   return( TRUE );
}

/* This function allocates a buffer into which to make a copy
 * of the current font settings.
 */
LOGFONT FAR * FASTCALL SaveFontSettings( void )
{
   LOGFONT FAR * lplf;

   INITIALISE_PTR(lplf);
   if( fNewMemory( &lplf, cbFonts * sizeof(LOGFONT) ) )
      {
      lplf[ 0 ] = lfMsgFont;
      lplf[ 1 ] = lfEditFont;
      lplf[ 2 ] = lfTermFont;
      lplf[ 3 ] = lfThreadFont;
      lplf[ 4 ] = lfStatusFont;
      lplf[ 5 ] = lfInBasketFont;
      lplf[ 6 ] = lfGrpListFont;
      lplf[ 7 ] = lfFixedFont;
      lplf[ 8 ] = lfOutBasketFont;
      lplf[ 9 ] = lfUserListFont;
      lplf[ 10 ] = lfCIXListFont;
      lplf[ 11 ] = lfResumesFont;
      lplf[ 12 ] = lfPrinterFont;
      lplf[ 13 ] = lfSchedulerFont;
      lplf[ 14 ] = lfAddrBook;
      }
   return( lplf );
}

/* This function handles the WM_MEASUREITEM message
 */
void FontSettings_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   HFONT hFont;
   HBITMAP hbmpFont;
   BITMAP bmp;
   HDC hdc;

   hdc = GetDC( hwnd );
   hFont = SelectFont( hdc, hHelvB8Font );
   GetTextMetrics( hdc, &tm );
   hbmpFont = LoadBitmap( hRscLib, MAKEINTRESOURCE(IDB_FONTBMPS) );
   GetObject( hbmpFont, sizeof( BITMAP ), &bmp );
   DeleteBitmap( hbmpFont );
   SelectFont( hdc, hFont );
   ReleaseDC( hwnd, hdc );
   lpMeasureItem->itemHeight = max( tm.tmHeight + tm.tmExternalLeading, bmp.bmHeight );
}

int FontSettings_OnCompareItem( HWND hwnd, const COMPAREITEMSTRUCT FAR * lpCompareItem )
{
   switch( lpCompareItem->CtlID )
      {
      case IDD_FONTSIZE: {
         char chSize1[ 7 ];
         char chSize2[ 7 ];

         lstrcpy( chSize1, (LPSTR)lpCompareItem->itemData1 );
         lstrcpy( chSize2, (LPSTR)lpCompareItem->itemData2 );
         return( atoi( chSize2 ) - atoi( chSize1 ) );
         }
      }
   return( 0 );
}

/* This function handles the WM_DRAWITEM message
 */
void FontSettings_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   switch( lpDrawItem->CtlID )
      {
      case IDD_FONTS: {
         char sz[ LF_FACESIZE ];
         HBRUSH hbr;
         HFONT hFont;
         COLORREF tmpT, tmpB;
         COLORREF T, B;
         int nFontType;
         int index;
         RECT rc;

         ComboBox_GetLBText( lpDrawItem->hwndItem, lpDrawItem->itemID, sz );
         nFontType = (int)ComboBox_GetItemData( lpDrawItem->hwndItem, lpDrawItem->itemID );
         rc = lpDrawItem->rcItem;

         if( lpDrawItem->itemState & ODS_SELECTED ) {
            T = GetSysColor( COLOR_HIGHLIGHTTEXT );
            B = GetSysColor( COLOR_HIGHLIGHT );
            }
         else {
            T = GetSysColor( COLOR_WINDOWTEXT );
            B = GetSysColor( COLOR_WINDOW );
            }
         hbr = CreateSolidBrush( B );
         FillRect( lpDrawItem->hDC, &rc, hbr );
         tmpT = SetTextColor( lpDrawItem->hDC, T );
         tmpB = SetBkColor( lpDrawItem->hDC, B );

         if( nFontType & TRUETYPE_FONTTYPE )
            index = 0;
         else if( nFontType & DEVICE_FONTTYPE )
            index = 1;
         else
            index = -1;
         if( index != -1 )
            {
            HBITMAP hbmpFont;

            hbmpFont = LoadBitmap( hRscLib, MAKEINTRESOURCE(IDB_FONTBMPS) );
            Amuser_DrawBitmap( lpDrawItem->hDC, rc.left, rc.top, 20, 12, hbmpFont, index );
            DeleteBitmap( hbmpFont );
            }
         rc.left += 22;
         hFont = SelectFont( lpDrawItem->hDC, hHelvB8Font );
         DrawText( lpDrawItem->hDC, sz, -1, &rc, DT_SINGLELINE|DT_VCENTER );
         SelectFont( lpDrawItem->hDC, hFont );

         SetTextColor( lpDrawItem->hDC, tmpT );
         SetBkColor( lpDrawItem->hDC, tmpB );
         DeleteBrush( hbr );
         break;
         }

      case IDD_FONTSIZE: {
         char sz[ 7 ];
         HBRUSH hbr;
         HFONT hFont;
         COLORREF tmpT, tmpB;
         COLORREF T, B;
         RECT rc;

         ComboBox_GetLBText( lpDrawItem->hwndItem, lpDrawItem->itemID, sz );
         rc = lpDrawItem->rcItem;

         if( lpDrawItem->itemState & ODS_SELECTED ) {
            T = GetSysColor( COLOR_HIGHLIGHTTEXT );
            B = GetSysColor( COLOR_HIGHLIGHT );
            }
         else {
            T = GetSysColor( COLOR_WINDOWTEXT );
            B = GetSysColor( COLOR_WINDOW );
            }
         hbr = CreateSolidBrush( B );
         FillRect( lpDrawItem->hDC, &rc, hbr );
         tmpT = SetTextColor( lpDrawItem->hDC, T );
         tmpB = SetBkColor( lpDrawItem->hDC, B );

         hFont = SelectFont( lpDrawItem->hDC, hHelvB8Font );
         ++rc.left;
         DrawText( lpDrawItem->hDC, sz, -1, &rc, DT_SINGLELINE|DT_VCENTER );
         SelectFont( lpDrawItem->hDC, hFont );

         SetTextColor( lpDrawItem->hDC, tmpT );
         SetBkColor( lpDrawItem->hDC, tmpB );
         DeleteBrush( hbr );
         break;
         }
      }
}

/* This function handles the WM_COMMAND message.
 */
void FontSettings_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   BOOL fSelUpdate = TRUE;

   switch( id )
      {
      case IDD_RESET:
         /* Recreate the selected LOGFONT.
          */
         CreateAmeolFont( nCurSel, &lplfTmp[ nCurSel ] );
         WriteCurrentFontSettings( hwnd );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_RESETALL: {
         int c;

         /* Recreate ALL the fonts.
          */
         for( c = 0; c < cbFonts; ++c )
            CreateAmeolFont( c, &lplfTmp[ c ] );
         WriteCurrentFontSettings( hwnd );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
         }

      case IDD_FIXEDONLY:
         fFixedOnly = IsDlgButtonChecked( hwnd, id );
         PostMessage( hwnd, WM_USER+0x202, 0, 0L );
         break;

      case IDD_FONTS:
         /* Post a message, because the selected font isn't actually selected yet.
          */
         if( codeNotify == CBN_SELCHANGE )
            {
            PostMessage( hwnd, WM_USER+0x200, 0, 0L );
            PropSheet_Changed( GetParent( hwnd ), hwnd );
            }
         else if( codeNotify == CBN_EDITUPDATE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_FONTSTYLES:
      case IDD_FONTSIZE:
         /* Post a message, because the selected style isn't actually selected yet.
          */
         if( codeNotify == CBN_SELCHANGE || codeNotify == CBN_KILLFOCUS )
            {
            PostMessage( hwnd, WM_USER+0x201, 0, 0L );
            PropSheet_Changed( GetParent( hwnd ), hwnd );
            }
         else if( codeNotify == CBN_EDITUPDATE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_APPLYSOURCE:
         if( codeNotify == LBN_SELCHANGE )
            {
            HWND hwndFont;
            HDC hdc;

            ReadCurrentFontSettings( hwnd );
            nCurSel = ListBox_GetCurSel( hwndCtl );
            hdc = GetDC( hwnd );
            hwndFont = GetDlgItem( hwnd, IDD_FONTS );
            ComboBox_ResetContent( hwndFont );
            SetWindowRedraw( hwndFont, FALSE );
            EnumFontFamilies( hdc, NULL, (FONTENUMPROC)FillFontDialog, (LPARAM)(LPSTR)hwnd );
            SetWindowRedraw( hwndFont, TRUE );
            ReleaseDC( hwnd, hdc );
            EnableControl( hwnd, IDD_FIXEDONLY, nCurSel != 2 );
            WriteCurrentFontSettings( hwnd );
            }
         break;
      }
}


/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL FontSettings_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsCUSTOMISE_FONTS );
         break;

      case PSN_SETACTIVE:
         nLastCustomiseDialogPage = CDU_FONTS;
         break;

      case PSN_RESET: {
         int i;

         /* The user has cancelled, so undo the changes
          * made.
          */
         for( i = 0; i < cbFonts; ++i )
            lplfTmp[ i ] = lplfSaved[ i ];
         ApplyChanges();
         return( TRUE );
         }

      case PSN_APPLY: {
         /* Read the current dialog first.
          */
         ReadCurrentFontSettings( hwnd );

         /* Now apply all changes.
          */
         ApplyChanges();

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

void FASTCALL ReadCurrentFontSettings( HWND hwnd )
{
   HWND hwndCmb;
   int iWeight = FW_NORMAL;
   int fItalic = FALSE;
#ifdef WIN32
   LONG wHeight;
#else
   WORD wHeight;
#endif
   char sz[ 20 ];
   HDC hdc;

   hwndCmb = GetDlgItem( hwnd, IDD_FONTS );
   ComboBox_GetText( hwndCmb, lplfTmp[ nCurSel ].lfFaceName, LF_FACESIZE );

   hwndCmb = GetDlgItem( hwnd, IDD_FONTSTYLES );
   ComboBox_GetText( hwndCmb, sz, sizeof( sz ) );
   if( lstrcmpi( sz, npBold ) == 0 )
      iWeight = FW_BOLD;
   else if( lstrcmpi( sz, npItalic ) == 0 )
      fItalic = TRUE;
   else if( lstrcmpi( sz, npBoldItalic ) == 0 )
      {
      iWeight = FW_BOLD;
      fItalic = TRUE;
      }
   lplfTmp[ nCurSel ].lfItalic = fItalic;
   lplfTmp[ nCurSel ].lfWeight = iWeight;

   hwndCmb = GetDlgItem( hwnd, IDD_FONTSIZE );
   ComboBox_GetText( hwndCmb, sz, sizeof( sz ) );
   wHeight = atoi( sz );
   hdc = GetDC( hwnd );
   wHeight = -MulDiv( wHeight, GetDeviceCaps( hdc, LOGPIXELSY ), 72 );
   ReleaseDC( hwnd, hdc );

   lplfTmp[ nCurSel ].lfHeight = wHeight;
   lplfTmp[ nCurSel ].lfOutPrecision = OUT_DEFAULT_PRECIS;
   lplfTmp[ nCurSel ].lfCharSet = DEFAULT_CHARSET;
   lplfTmp[ nCurSel ].lfPitchAndFamily = FF_DONTCARE|DEFAULT_PITCH;
}

void FASTCALL WriteCurrentFontSettings( HWND hwnd )
{
   HWND hwndCmb;
#ifdef WIN32
   LONG wHeight;
#else
   WORD wHeight;
#endif
   char sz[ 20 ];
   int nSel;
   HDC hdc;

   hwndCmb = GetDlgItem( hwnd, IDD_FONTS );
   nSel = ComboBox_RealFindStringExact( hwndCmb, lplfTmp[ nCurSel ].lfFaceName );
   ComboBox_SetCurSel( hwndCmb, nSel );
   if( nSel == CB_ERR )
      ComboBox_SetText( hwndCmb, lplfTmp[ nCurSel ].lfFaceName );
   FillFontStylesAndSizes( hwnd );
   ShowSelectedFontType( hwnd, nSel );

   hwndCmb = GetDlgItem( hwnd, IDD_FONTSTYLES );
   if( lplfTmp[ nCurSel ].lfWeight >= FW_BOLD && lplfTmp[ nCurSel ].lfItalic )
      lstrcpy( sz, npBoldItalic );
   else if( lplfTmp[ nCurSel ].lfWeight >= FW_BOLD )
      lstrcpy( sz, npBold );
   else if( lplfTmp[ nCurSel ].lfItalic )
      lstrcpy( sz, npItalic );
   else
      lstrcpy( sz, npRegular );
   nSel = ComboBox_FindStringExact( hwndCmb, -1, sz );
   ComboBox_SetCurSel( hwndCmb, nSel );

   hwndCmb = GetDlgItem( hwnd, IDD_FONTSIZE );
   hdc = GetDC( hwnd );
   wHeight = -MulDiv( lplfTmp[ nCurSel ].lfHeight, 72, GetDeviceCaps( hdc, LOGPIXELSY ) );
   ReleaseDC( hwnd, hdc );
   wsprintf( sz, "%u", wHeight );
   nSel = ComboBox_FindStringExact( hwndCmb, -1, sz );
   ComboBox_SetCurSel( hwndCmb, nSel );
   if( nSel == CB_ERR )
      ComboBox_SetText( hwndCmb, sz );

   hdc = GetDC( hwnd );
   PaintSample( hwnd, hdc );
   ReleaseDC( hwnd, hdc );
}

/* This function applies any changes made to the font
 * settings.
 */
void FASTCALL ApplyChanges( void )
{
   if( memcmp( &lfMsgFont, &lplfTmp[ 0 ], sizeof( LOGFONT ) ) )
      ApplyFont( 0 );
   if( memcmp( &lfEditFont, &lplfTmp[ 1 ], sizeof( LOGFONT ) ) )
      ApplyFont( 1 );
   if( memcmp( &lfTermFont, &lplfTmp[ 2 ], sizeof( LOGFONT ) ) )
      ApplyFont( 2 );
   if( memcmp( &lfThreadFont, &lplfTmp[ 3 ], sizeof( LOGFONT ) ) )
      ApplyFont( 3 );
   if( memcmp( &lfStatusFont, &lplfTmp[ 4 ], sizeof( LOGFONT ) ) )
      ApplyFont( 4 );
   if( memcmp( &lfInBasketFont, &lplfTmp[ 5 ], sizeof( LOGFONT ) ) )
      ApplyFont( 5 );
   if( memcmp( &lfGrpListFont, &lplfTmp[ 6 ], sizeof( LOGFONT ) ) )
      ApplyFont( 6 );
   if( memcmp( &lfFixedFont, &lplfTmp[ 7 ], sizeof( LOGFONT ) ) )
      ApplyFont( 7 );
   if( memcmp( &lfOutBasketFont, &lplfTmp[ 8 ], sizeof( LOGFONT ) ) )
      ApplyFont( 8 );
   if( memcmp( &lfUserListFont, &lplfTmp[ 9 ], sizeof( LOGFONT ) ) )
      ApplyFont( 9 );
   if( memcmp( &lfCIXListFont, &lplfTmp[ 10 ], sizeof( LOGFONT ) ) )
      ApplyFont( 10 );
   if( memcmp( &lfResumesFont, &lplfTmp[ 11 ], sizeof( LOGFONT ) ) )
      ApplyFont( 11 );
   if( memcmp( &lfPrinterFont, &lplfTmp[ 12 ], sizeof( LOGFONT ) ) )
      ApplyFont( 12 );
   if( memcmp( &lfSchedulerFont, &lplfTmp[ 13 ], sizeof( LOGFONT ) ) )
      ApplyFont( 13 );
   if( memcmp( &lfAddrBook, &lplfTmp[ 14 ], sizeof( LOGFONT ) ) )
      ApplyFont( 14 );
}

void FASTCALL ApplyFont( int nSel )
{
   switch( nSel )
      {
      case 0:
         lfMsgFont = lplfTmp[ 0 ];
         DeleteFont( hMsgFont );
         hMsgFont = CreateFontIndirect( &lfMsgFont );
         SendAllWindows( WM_CHANGEFONT, FONTYP_MESSAGE, 0L );
         WriteFontData( "Message", &lfMsgFont );
         break;

      case 1:
         lfEditFont = lplfTmp[ 1 ];
         DeleteFont( hEditFont );
         hEditFont = CreateFontIndirect( &lfEditFont );
         SendAllWindows( WM_CHANGEFONT, FONTYP_EDIT, 0L );
         WriteFontData( "Edit", &lfEditFont );
         break;

      case 2:
         lfTermFont = lplfTmp[ 2 ];
         DeleteFont( hTermFont );
         hTermFont = CreateFontIndirect( &lfTermFont );
         SendAllWindows( WM_CHANGEFONT, FONTYP_TERMINAL, 0L );
         WriteFontData( "Terminal", &lfTermFont );
         break;

      case 3:
         lfThreadFont = lplfTmp[ 3 ];
         DeleteFont( hThreadFont );
         DeleteFont( hThreadBFont );
         hThreadFont = CreateFontIndirect( &lfThreadFont );
         lfThreadBFont = lfThreadFont;
         lfThreadBFont.lfWeight = FW_BOLD;
         hThreadBFont = CreateFontIndirect( &lfThreadBFont );
         SendAllWindows( WM_CHANGEFONT, FONTYP_THREAD, 0L );
         WriteFontData( "Thread", &lfThreadFont );
         break;

      case 4:
         lfStatusFont = lplfTmp[ 4 ];
         DeleteFont( hStatusFont );
         hStatusFont = CreateFontIndirect( &lfStatusFont );
         if( NULL != hwndStatus )
            {
            ComputeStatusBarWidth();
            UpdateStatusBarWidths();
            OfflineStatusText( "" );
            SendMessage( hwndStatus, WM_SETFONT, (WPARAM)hStatusFont, 1L );
            ShowUnreadMessageCount();
            }
         WriteFontData( "Status Bar", &lfStatusFont );
         break;

      case 5:
         lfInBasketFont = lplfTmp[ 5 ];
         DeleteFont( hInBasketFont );
         DeleteFont( hInBasketBFont );
         hInBasketFont = CreateFontIndirect( &lfInBasketFont );
         lfInBasketBFont = lfInBasketFont;
         lfInBasketBFont.lfWeight = FW_BOLD;
         hInBasketBFont = CreateFontIndirect( &lfInBasketBFont );
         SendAllWindows( WM_CHANGEFONT, FONTYP_INBASKET, 0L );
         WriteFontData( "In Basket", &lfInBasketFont );
         break;

      case 6:
         lfGrpListFont = lplfTmp[ 6 ];
         DeleteFont( hGrpListFont );
         hGrpListFont = CreateFontIndirect( &lfGrpListFont );
         if( hwndGrpList )
            SendMessage( hwndGrpList, WM_CHANGEFONT, FONTYP_GRPLIST, 0L );
         WriteFontData( "Newsgroup Lists", &lfGrpListFont );
         break;

      case 7:
         lfFixedFont = lplfTmp[ 7 ];
         DeleteFont( hFixedFont );
         hFixedFont = CreateFontIndirect( &lfFixedFont );
         SendAllWindows( WM_CHANGEFONT, FONTYP_FIXEDFONT, 0L );
         WriteFontData( "Fixed", &lfFixedFont );
         break;

      case 8:
         lfOutBasketFont = lplfTmp[ 8 ];
         DeleteFont( hOutBasketFont );
         hOutBasketFont = CreateFontIndirect( &lfOutBasketFont );
         SendAllWindows( WM_CHANGEFONT, FONTYP_OUTBASKET, 0L );
         WriteFontData( "Out Basket", &lfOutBasketFont );
         break;

      case 9:
         lfUserListFont = lplfTmp[ 9 ];
         DeleteFont( hUserListFont );
         hUserListFont = CreateFontIndirect( &lfUserListFont );
         if( hwndUserList )
            SendMessage( hwndUserList, WM_CHANGEFONT, FONTYP_USERLIST, 0L );
         WriteFontData( "User List", &lfUserListFont );
         break;

      case 10:
         lfCIXListFont = lplfTmp[ 10 ];
         DeleteFont( hCIXListFont );
         hCIXListFont = CreateFontIndirect( &lfCIXListFont );
         if( hwndCIXList )
            SendMessage( hwndCIXList, WM_CHANGEFONT, FONTYP_CIXLIST, 0L );
         WriteFontData( "CIX List", &lfCIXListFont );
         break;

      case 11:
         lfResumesFont = lplfTmp[ 11 ];
         DeleteFont( hResumesFont );
         hResumesFont = CreateFontIndirect( &lfResumesFont );
         if( hwndResumes )
            SendMessage( hwndResumes, WM_CHANGEFONT, FONTYP_RESUMES, 0L );
         WriteFontData( "Resumes", &lfResumesFont );
         break;

      case 12:
         lfPrinterFont = lplfTmp[ 12 ];
         DeleteFont( hPrinterFont );
         hPrinterFont = CreateFontIndirect( &lfPrinterFont );
         WriteFontData( "Printer", &lfPrinterFont );
         break;

      case 13:
         lfSchedulerFont = lplfTmp[ 13 ];
         DeleteFont( hSchedulerFont );
         hSchedulerFont = CreateFontIndirect( &lfSchedulerFont );
         if( hwndScheduler )
            SendMessage( hwndScheduler, WM_CHANGEFONT, FONTYP_SCHEDULE, 0L );
         WriteFontData( "Scheduler", &lfSchedulerFont );
         break;

      case 14:
         lfAddrBook = lplfTmp[ 14 ];
         if( hwndAddrBook )
            SendMessage( hwndAddrBook, WM_CHANGEFONT, FONTYP_ADDRBOOK, 0L );
         WriteFontData( "Address Book", &lfAddrBook );
         break;
      }
}

void FASTCALL FillFontStylesAndSizes( HWND hwnd )
{
   char sz[ LF_FACESIZE ];
   HWND hwndFonts;
   HWND hwndFontStyles;
   HDC hdc;

   ComboBox_GetText( GetDlgItem( hwnd, IDD_FONTS ), sz, LF_FACESIZE );
   hdc = GetDC( hwnd );
   hwndFonts = GetDlgItem( hwnd, IDD_FONTSTYLES );
   hwndFontStyles = GetDlgItem( hwnd, IDD_FONTSIZE );
   ComboBox_ResetContent( hwndFonts );
   ComboBox_ResetContent( hwndFontStyles );
   if( *sz )
      {
      SetWindowRedraw( hwndFonts, FALSE );
      SetWindowRedraw( hwndFontStyles, FALSE );
      EnumFontFamilies( hdc, sz, (FONTENUMPROC)StylesAndSizesCallback, (LPARAM)(LPSTR)hwnd );
      SetWindowRedraw( hwndFonts, TRUE );
      SetWindowRedraw( hwndFontStyles, TRUE );
      }

   /* If this font has no renumerated styles, add a default style
    */
   if( ComboBox_GetCount( hwndFonts ) == 0 )
      ComboBox_AddString( hwndFonts, npRegular );
   ComboBox_SetCurSel( hwndFonts, 0 );
   ComboBox_SetCurSel( hwndFontStyles, 0 );
   ReleaseDC( hwnd, hdc );
}

int EXPORT CALLBACK FillFontDialog( const ENUMLOGFONT FAR * lpnlf, const NEWTEXTMETRIC FAR * lpntm, int FontType, LPARAM lParam )
{
   HWND hwnd = (HWND)lParam;
   HWND hwndCmb;
   int index;

   /* Reject proportional fonts if this is the Terminal window
    */
   hwndCmb = GetDlgItem( hwnd, IDD_FONTS );
   if( ( nCurSel == 2 || fFixedOnly ) && ( lpnlf->elfLogFont.lfPitchAndFamily & 0x3 ) != FIXED_PITCH )
      return( TRUE );
   index = ComboBox_AddString( hwndCmb, lpnlf->elfLogFont.lfFaceName );
   ComboBox_SetItemData( hwndCmb, index, FontType );
   return( TRUE );
}

int EXPORT CALLBACK StylesAndSizesCallback( const ENUMLOGFONT FAR * lpnlf, const NEWTEXTMETRIC FAR * lpntm, int FontType, LPARAM lParam )
{
   HWND hwnd = (HWND)lParam;
   HWND hwndCmb;
   char sz[ 7 ];
   register int c;

   if( FontType & TRUETYPE_FONTTYPE )
      {
      hwndCmb = GetDlgItem( hwnd, IDD_FONTSTYLES );
      if( ComboBox_FindStringExact( hwndCmb, -1, lpnlf->elfStyle ) == CB_ERR ) /*!!SM!!*/
         ComboBox_InsertString( hwndCmb, -1, lpnlf->elfStyle );
      hwndCmb = GetDlgItem( hwnd, IDD_FONTSIZE );
      ComboBox_ResetContent( hwndCmb );
      for( c = 0; c < TOTAL_DEF_TT_FONT_SIZES; ++c )
         {
         wsprintf( sz, "%u", wDefTTFontSize[ c ] );
         if( ComboBox_FindStringExact( hwndCmb, -1, sz ) == CB_ERR ) /*!!SM!!*/
            ComboBox_InsertString( hwndCmb, -1, sz );
         }
      }
   else if( ( FontType & DEVICE_FONTTYPE ) && !( FontType & RASTER_FONTTYPE ) )
      {
      hwndCmb = GetDlgItem( hwnd, IDD_FONTSIZE );
      ComboBox_ResetContent( hwndCmb );
      for( c = 0; c < TOTAL_DEF_TT_FONT_SIZES; ++c )
         {
         wsprintf( sz, "%u", wDefTTFontSize[ c ] );
         if( ComboBox_FindStringExact( hwndCmb, -1, sz ) == CB_ERR ) /*!!SM!!*/
            ComboBox_InsertString( hwndCmb, -1, sz );
         }

      hwndCmb = GetDlgItem( hwnd, IDD_FONTSTYLES );
      if( lpnlf->elfLogFont.lfWeight < FW_BOLD )
         {
         if( ComboBox_FindStringExact( hwndCmb, -1, npRegular ) == CB_ERR ) /*!!SM!!*/
            ComboBox_InsertString( hwndCmb, -1, npRegular );
         if( ComboBox_FindStringExact( hwndCmb, -1, npItalic ) == CB_ERR ) /*!!SM!!*/
            ComboBox_InsertString( hwndCmb, -1, npItalic );
         }
      if( ComboBox_FindStringExact( hwndCmb, -1, npBold ) == CB_ERR ) /*!!SM!!*/
         ComboBox_InsertString( hwndCmb, -1, npBold );
      if( ComboBox_FindStringExact( hwndCmb, -1, npBoldItalic ) == CB_ERR ) /*!!SM!!*/
         ComboBox_InsertString( hwndCmb, -1, npBoldItalic );
      return( FALSE );
      }
   else
      {
      char * np;
      COMPAREITEMSTRUCT cis;
      BOOL fInsert = FALSE;
      HDC hdc;
#ifdef WIN32
      LONG wHeight;
#else
      WORD wHeight;
#endif
      register int r;

      hwndCmb = GetDlgItem( hwnd, IDD_FONTSTYLES );
      if( lpnlf->elfLogFont.lfWeight >= FW_BOLD )
         if( lpnlf->elfLogFont.lfItalic )
            np = npBoldItalic;
         else
            np = npBold;
      else if( lpnlf->elfLogFont.lfItalic )
         np = npItalic;
      else
         np = npRegular;
      if( ComboBox_FindStringExact( hwndCmb, -1, np ) == CB_ERR )
         ComboBox_InsertString( hwndCmb, -1, np );

      hwndCmb = GetDlgItem( hwnd, IDD_FONTSIZE );
      hdc = GetDC( hwnd );
      wHeight = MulDiv( lpnlf->elfLogFont.lfHeight, 72, GetDeviceCaps( hdc, LOGPIXELSY ) );
      ReleaseDC( hwnd, hdc );
      wsprintf( sz, "%u", wHeight );

      r = 0;
      if( ComboBox_GetCount( hwndCmb ) == 0 )
         fInsert = TRUE;
      else for( c = 0; c < ComboBox_GetCount( hwndCmb ); ++c )
         {
         char sz2[ 7 ];

         cis.CtlType = ODT_COMBOBOX;
         cis.CtlID = IDD_FONTSIZE;
         cis.hwndItem = hwndCmb;
         cis.itemID1 = (UINT)-1;
         cis.itemData1 = (DWORD)(LPSTR)sz;
         cis.itemID2 = c;
         cis.itemData2 = (DWORD)(LPSTR)sz2;
         ComboBox_GetLBText( hwndCmb, c, sz2 );
         if( ( r = (int)SendMessage( hwnd, WM_COMPAREITEM, cis.CtlID, (LPARAM)(LPSTR)&cis ) ) == 0 )
            break;
         if( r > 0 )
            {
            fInsert = TRUE;
            break;
            }
         }
      if( fInsert )
         if( ComboBox_FindStringExact( hwndCmb, -1, sz ) == CB_ERR ) /*!!SM!!*/
            ComboBox_InsertString( hwndCmb, r, sz );
      }
   return( TRUE );
}

void FASTCALL PaintSample( HWND hwnd, HDC hdc )
{
   RECT rc;
   HFONT hFont, hOldFont;
   HBRUSH hOldBrush;
   HPEN hOldPen;
   int oldMode;

   GetWindowRect( GetDlgItem( hwnd, IDD_SAMPLE ), &rc );
   ScreenToClient( hwnd, (LPPOINT)&rc );
   ScreenToClient( hwnd, ((LPPOINT)&rc)+1 );
   hFont = CreateFontIndirect( &lplfTmp[ nCurSel ] );
   hOldFont = SelectFont( hdc, hFont );
   hOldBrush = SelectBrush( hdc, GetStockBrush( WHITE_BRUSH ) );
   hOldPen = SelectPen( hdc, GetStockPen( BLACK_PEN ) );
   Rectangle( hdc, rc.left, rc.top, rc.right, rc.bottom );
   InflateRect( &rc, -1, -1 );
   SetBkColor( hdc, RGB(255,255,255) );
   oldMode = SetBkMode( hdc, TRANSPARENT );
   DrawText( hdc, "AaBbYyZz", -1, &rc, DT_SINGLELINE|DT_VCENTER|DT_CENTER );
   SetBkMode( hdc, oldMode );
   SelectFont( hdc, hOldFont );
   SelectBrush( hdc, hOldBrush );
   SelectPen( hdc, hOldPen );
   DeleteFont( hFont );
}

void FASTCALL ShowSelectedFontType( HWND hwnd, int index )
{
   char * psz;

   if( index == CB_ERR )
      psz = "";
   else {
      int nFontType;
      HWND hwndCmb;

      VERIFY( hwndCmb = GetDlgItem( hwnd, IDD_FONTS ) );
      nFontType = (int)ComboBox_GetItemData( hwndCmb, index );
      if( nFontType & TRUETYPE_FONTTYPE )
         psz = GS(IDS_STR530);
      else if( nFontType & DEVICE_FONTTYPE )
         psz = GS(IDS_STR531);
      else
         psz = GS(IDS_STR532);
      }
   SetDlgItemText( hwnd, IDD_PAD1, psz );
}
