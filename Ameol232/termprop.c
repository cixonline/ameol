/* TERMPROP.C - Terminal properties
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
#include "palrc.h"
#include "resource.h"
#include <memory.h>
#include "amlib.h"
#include "string.h"
#include "terminal.h"

#define  THIS_FILE   __FILE__

/* Font sizes for TrueType fonts
 */
static WORD wDefTTFontSize[] = { 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 36, 48, 72 };
#define TOTAL_DEF_TT_FONT_SIZES (sizeof(wDefTTFontSize)/sizeof(WORD))

/* Type style names for non-TT fonts
 */
static char NEAR pstrRegular[] = "Regular";
static char NEAR pstrBold[] = "Bold";
static char NEAR pstrItalic[] = "Italic";
static char NEAR pstrBoldItalic[] = "Bold Italic";

/* Stuff used by the ChooseFont dialog box
 */
static BOOL fFixedOnly = FALSE;
static BYTE FAR nFontTypeByte[ 100 ];
static LOGFONT lfTmp;                  /* Current font definitions */
static LOGFONT lfPreTmp;               /* Font definitions before any changes were made */

BOOL EXPORT CALLBACK TerminalOptions( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL TerminalOptions_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL TerminalOptions_OnNotify( HWND, int, LPNMHDR );
void FASTCALL TerminalOptions_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK TerminalFonts( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL TerminalFonts_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL TerminalFonts_OnNotify( HWND, int, LPNMHDR );
void FASTCALL TerminalFonts_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
int FASTCALL TerminalFonts_OnCompareItem( HWND, const COMPAREITEMSTRUCT FAR * );
void FASTCALL TerminalFonts_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
void FASTCALL TerminalFonts_OnCommand( HWND, int, HWND, UINT );

static void FASTCALL ShowSelectedFontType( HWND, int );
static void FASTCALL PaintSample( HWND, HDC );
int EXPORT CALLBACK TerminalFillFontDialog( const ENUMLOGFONT FAR *, const NEWTEXTMETRIC FAR *, int, LPARAM );
static void FASTCALL ReadCurrentFontSettings( HWND );
static void FASTCALL WriteCurrentFontSettings( HWND );

BOOL EXPORT CALLBACK TerminalColours( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL TerminalColours_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL TerminalColours_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL TerminalColours_OnNotify( HWND, int, LPNMHDR );

BOOL EXPORT CALLBACK TerminalEmulations( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL TerminalEmulations_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL TerminalEmulations_OnNotify( HWND, int, LPNMHDR );
void FASTCALL TerminalEmulations_OnCommand( HWND, int, HWND, UINT );

/* This function sets the default terminal font based on the
 * settings.
 */
void FASTCALL Terminal_SetDefaultFont( TERMINALWND FAR * ptwnd, char * pszSection )
{
   lfTmp.lfHeight = Amuser_GetPPInt( pszSection, "FontSize", lfTermFont.lfHeight );
   lfTmp.lfWidth = lfTermFont.lfWidth;
   lfTmp.lfEscapement = lfTermFont.lfEscapement;
   lfTmp.lfOrientation = lfTermFont.lfOrientation;
   lfTmp.lfWeight = Amuser_GetPPInt( pszSection, "FontWeight", lfTermFont.lfWeight );
   lfTmp.lfItalic = Amuser_GetPPInt( pszSection, "FontItalic", lfTermFont.lfItalic );
   lfTmp.lfUnderline = lfTermFont.lfUnderline;
   lfTmp.lfStrikeOut = lfTermFont.lfStrikeOut;
   lfTmp.lfCharSet = Amuser_GetPPInt( pszSection, "FontCharset", lfTermFont.lfCharSet );
   lfTmp.lfOutPrecision = lfTermFont.lfOutPrecision;
   lfTmp.lfClipPrecision = lfTermFont.lfClipPrecision;
   lfTmp.lfQuality = lfTermFont.lfQuality;
   lfTmp.lfPitchAndFamily = Amuser_GetPPInt( pszSection, "FontPitchAndFamily", lfTermFont.lfPitchAndFamily );
   Amuser_GetPPString( pszSection, "FontName", lfTermFont.lfFaceName, lfTmp.lfFaceName, LF_FACESIZE );
   ptwnd->tp.hFont = CreateFontIndirect( &lfTmp );
}

/* This function handles the Terminal Properties dialog
 */
void FASTCALL Terminal_Properties( HWND hwnd, TERMINALWND FAR * ptwnd )
{
   AMPROPSHEETPAGE psp[ 4 ];
   AMPROPSHEETHEADER psh;

   psp[ 0 ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ 0 ].dwFlags = PSP_USETITLE;
   psp[ 0 ].hInstance = hInst;
   psp[ 0 ].pszTemplate = MAKEINTRESOURCE( IDDLG_TERMINAL_OPTIONS );
   psp[ 0 ].pszIcon = NULL;
   psp[ 0 ].pfnDlgProc = TerminalOptions;
   psp[ 0 ].pszTitle = "General";
   psp[ 0 ].lParam = (LPARAM)ptwnd;

   psp[ 1 ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ 1 ].dwFlags = PSP_USETITLE;
   psp[ 1 ].hInstance = hInst;
   psp[ 1 ].pszTemplate = MAKEINTRESOURCE( IDDLG_TERMINAL_FONT );
   psp[ 1 ].pszIcon = NULL;
   psp[ 1 ].pfnDlgProc = TerminalFonts;
   psp[ 1 ].pszTitle = "Font";
   psp[ 1 ].lParam = (LPARAM)ptwnd;

   psp[ 2 ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ 2 ].dwFlags = PSP_USETITLE;
   psp[ 2 ].hInstance = hInst;
   psp[ 2 ].pszTemplate = MAKEINTRESOURCE( IDDLG_TERMINAL_COLOURS );
   psp[ 2 ].pszIcon = NULL;
   psp[ 2 ].pfnDlgProc = TerminalColours;
   psp[ 2 ].pszTitle = "Colours";
   psp[ 2 ].lParam = (LPARAM)ptwnd;

   psp[ 3 ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ 3 ].dwFlags = PSP_USETITLE;
   psp[ 3 ].hInstance = hInst;
   psp[ 3 ].pszTemplate = MAKEINTRESOURCE( IDDLG_TERMINAL_EMULATIONS );
   psp[ 3 ].pszIcon = NULL;
   psp[ 3 ].pfnDlgProc = TerminalEmulations;
   psp[ 3 ].pszTitle = "Emulations";
   psp[ 3 ].lParam = (LPARAM)ptwnd;

   psh.dwSize = sizeof( AMPROPSHEETHEADER );
   psh.dwFlags = PSH_PROPSHEETPAGE|PSH_PROPTITLE|PSH_HASHELP;
   if( fMultiLineTabs )
      psh.dwFlags |= PSH_MULTILINETABS;
   psh.hwndParent = hwnd;
   psh.hInstance = hInst;
   psh.pszIcon = NULL;
   psh.pszCaption = ptwnd->szName;
   psh.nPages = sizeof( psp ) / sizeof( AMPROPSHEETPAGE );
   psh.nStartPage = 0;
   psh.ppsp = (LPCAMPROPSHEETPAGE)&psp;

   Amctl_PropertySheet(&psh );
}

/* This function dispatches messages for the Terminal Options dialog.
 */
BOOL EXPORT CALLBACK TerminalOptions( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, TerminalOptions_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, TerminalOptions_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, TerminalOptions_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL TerminalOptions_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   TERMINALWND FAR * ptwnd;
   LPCAMPROPSHEETPAGE psp;
   HWND hwndSpin;

   psp = (LPCAMPROPSHEETPAGE)lParam;
   ptwnd = (TERMINALWND FAR *)psp->lParam;
   SetWindowLong( hwnd, DWL_USER, (LPARAM)ptwnd );

   /* Set picture button.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_EDITCONNCARD ), hInst, IDB_EDITCONNCARD );

   /* Fill with list of available connection cards.
    */
   FillConnectionCards( hwnd, IDD_LIST, ptwnd->tp.szConnCard );

   /* Disable the name if a window is open.
    */
   if( ptwnd->hwnd )
      EnableControl( hwnd, IDD_EDIT, FALSE );

   /* Set the current property settings.
    */
   SetDlgItemText( hwnd, IDD_EDIT, ptwnd->szName );
   Edit_LimitText( GetDlgItem( hwnd, IDD_EDIT ), LEN_TERMNAME );

   /* Set the current property settings.
    */
   CheckDlgButton( hwnd, IDD_STRIPHIGH, ptwnd->tp.fStripHigh );
   CheckDlgButton( hwnd, IDD_LOCALECHO, ptwnd->tp.fLocalEcho );
   CheckDlgButton( hwnd, IDD_SOUND, ptwnd->tp.fSound );
   CheckDlgButton( hwnd, IDD_AUTOWRAP, ptwnd->tp.fAutoWrap );
   CheckDlgButton( hwnd, IDD_NEWLINE, ptwnd->tp.fNewline );
   CheckDlgButton( hwnd, IDD_OUTCRTOCRLF, ptwnd->tp.fOutCRtoCRLF );

   /* Set the buffer size values.
    */
   SetDlgItemInt( hwnd, IDD_BUFFERSIZE, ptwnd->tp.nBufferSize, FALSE );
   hwndSpin = GetDlgItem( hwnd, IDD_SPIN );
   SendMessage( hwndSpin, UDM_SETBUDDY, (WPARAM)GetDlgItem( hwnd, IDD_BUFFERSIZE ), 0L );
   SendMessage( hwndSpin, UDM_SETRANGE, 0, MAKELPARAM( ( ptwnd->tp.nLineWidth == 80 ? 800 : 480 ), 10 ) );
   SendMessage( hwndSpin, UDM_SETPOS, 0, MAKELONG( ptwnd->tp.nBufferSize, 0 ) );

   /* Set the terminal width values.
    */
   SetDlgItemInt( hwnd, IDD_TERMINALWIDTH, ptwnd->tp.nLineWidth, FALSE );
   hwndSpin = GetDlgItem( hwnd, IDD_SPIN2 );
   SendMessage( hwndSpin, UDM_SETBUDDY, (WPARAM)GetDlgItem( hwnd, IDD_TERMINALWIDTH ), 0L );
   SendMessage( hwndSpin, UDM_SETRANGE, 0, MAKELPARAM( 132, 40 ) );
   SendMessage( hwndSpin, UDM_SETPOS, 0, MAKELONG( ptwnd->tp.nLineWidth, 0 ) );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL TerminalOptions_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EDITCONNCARD: {
         COMMDESCRIPTOR cd;
         HWND hwndList;

         INITIALISE_PTR( cd.lpsc );
         INITIALISE_PTR( cd.lpic );

         /* Get the selected card name.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         ComboBox_GetText( hwndList, cd.szTitle, LEN_CONNCARD+1 );
         EditConnectionCard( hwnd, &cd );
         break;
         }

      case IDD_LIST:
         if( codeNotify == CBN_SELCHANGE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_STRIPHIGH:
      case IDD_LOCALECHO:
      case IDD_SOUND:
      case IDD_AUTOWRAP:
      case IDD_NEWLINE:
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_EDIT:
      case IDD_TERMINALWIDTH:
      case IDD_BUFFERSIZE:
         if( codeNotify == EN_CHANGE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL TerminalOptions_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsTERMINAL_OPTIONS );
         break;

      case PSN_APPLY: {
         char szSection[ LEN_TERMNAME+20+1 ];
         char szName[ LEN_TERMNAME+1 ];
         TERMINALWND FAR * ptwnd;
         int nBufferSize;
         HWND hwndCombo;
         int nLineWidth;
         HWND hwndEdit;
         BOOL fChange;

         /* Get the pointer to the properties record.
          */
         ptwnd = (TERMINALWND FAR *)GetWindowLong( hwnd, DWL_USER );
         fChange = FALSE;

         /* Get the connection card name.
          */
         hwndCombo = GetDlgItem( hwnd, IDD_LIST );
         ComboBox_GetText( hwndCombo, ptwnd->tp.szConnCard, LEN_CONNCARD );

         /* Get and store the new section name.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         Edit_GetText( hwndEdit, szName, sizeof(szName) );
         StripLeadingTrailingSpaces( szName );
         if( *szName == '\0' )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR755), MB_OK|MB_ICONEXCLAMATION );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }

         /* Delete the old section. The current settings will be saved
          * in the new section automatically.
          */
         wsprintf( szSection, "%s Properties", ptwnd->szName );
         Amuser_WritePPString( szSection, NULL, NULL );
         wsprintf( szSection, "%s Properties", szName );
         strcpy( ptwnd->szName, szName );

         /* Save the connection card name AFTER we've changed
          * the section name.
          */
         Amuser_WritePPString( szSection, "ConnectionCard", ptwnd->tp.szConnCard );

         /* Get and store the selected colours.
          */
         ptwnd->tp.fStripHigh = IsDlgButtonChecked( hwnd, IDD_STRIPHIGH );
         ptwnd->tp.fLocalEcho = IsDlgButtonChecked( hwnd, IDD_LOCALECHO );
         ptwnd->tp.fSound = IsDlgButtonChecked( hwnd, IDD_SOUND );
         ptwnd->tp.fAutoWrap = IsDlgButtonChecked( hwnd, IDD_AUTOWRAP );
         ptwnd->tp.fNewline = IsDlgButtonChecked( hwnd, IDD_NEWLINE );
         ptwnd->tp.fOutCRtoCRLF = IsDlgButtonChecked( hwnd, IDD_OUTCRTOCRLF );

         /* Get the buffer size and line width.
          */
         if( !GetDlgInt( hwnd, IDD_TERMINALWIDTH, &nLineWidth, 40, 132 ) )
            return( PSNRET_INVALID_NOCHANGEPAGE );
         if( !GetDlgInt( hwnd, IDD_BUFFERSIZE, &nBufferSize, 10, (nLineWidth == 80 ? 800 : 480) ) )
            return( PSNRET_INVALID_NOCHANGEPAGE );
         if( nLineWidth != ptwnd->tp.nLineWidth )
            fChange = TRUE;
         if( nBufferSize != ptwnd->tp.nBufferSize )
            fChange = TRUE;
         ptwnd->tp.nBufferSize = nBufferSize;
         ptwnd->tp.nLineWidth = nLineWidth;
         if( fChange && ptwnd->hwnd )
            Terminal_ResizeBuffer( ptwnd->hwndTerm );

         /* Save the settings.
          */
         wsprintf( szSection, "%s Properties", ptwnd->szName );
         Amuser_WritePPInt( szSection, "Newline", ptwnd->tp.fNewline );
         Amuser_WritePPInt( szSection, "OutCRtoCRLF", ptwnd->tp.fOutCRtoCRLF );
         Amuser_WritePPInt( szSection, "LocalEcho", ptwnd->tp.fLocalEcho );
         Amuser_WritePPInt( szSection, "AutoWrap", ptwnd->tp.fAutoWrap );
         Amuser_WritePPInt( szSection, "Sound", ptwnd->tp.fSound );
         Amuser_WritePPInt( szSection, "StripHigh", ptwnd->tp.fStripHigh );
         Amuser_WritePPInt( szSection, "BufferSize", ptwnd->tp.nBufferSize );
         Amuser_WritePPInt( szSection, "LineWidth", ptwnd->tp.nLineWidth );

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This function dispatches messages for the Terminal Fonts dialog.
 */
BOOL EXPORT CALLBACK TerminalFonts( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, TerminalFonts_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, TerminalFonts_OnNotify );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, TerminalFonts_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_DRAWITEM, TerminalFonts_OnDrawItem );
      HANDLE_DLGMSG( hwnd, WM_MEASUREITEM, TerminalFonts_OnMeasureItem );
      HANDLE_DLGMSG( hwnd, WM_COMPAREITEM, TerminalFonts_OnCompareItem );

      case WM_PAINT: {
         PAINTSTRUCT ps;
         HDC hdc;

         hdc = BeginPaint( hwnd, &ps );
         PaintSample( hwnd, hdc );
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
         EnumFontFamilies( hdc, NULL, (FONTENUMPROC)TerminalFillFontDialog, (LPARAM)(LPSTR)hwnd );
         ReleaseDC( hwnd, hdc );
         SetWindowRedraw( hwndCmb, TRUE );
         WriteCurrentFontSettings( hwnd );
         break;
         }

      case WM_USER+0x0201: {
         HDC hdc;

         /* The font size or style has changed.
          */
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
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL TerminalFonts_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   TERMINALWND FAR * ptwnd;
   LPCAMPROPSHEETPAGE psp;
   HFONT hFont;
   TEXTMETRIC tm;
   HDC hdc;

   /* Save the pointer to the terminal properties record.
    */
   psp = (LPCAMPROPSHEETPAGE)lParam;
   ptwnd = (TERMINALWND FAR *)psp->lParam;
   SetWindowLong( hwnd, DWL_USER, (LPARAM)ptwnd );

   /* Unpack the attributes for the current font.
    */
   GetObject( ptwnd->tp.hFont, sizeof( LOGFONT ), &lfTmp );
   lfPreTmp = lfTmp;

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
      }
   else
      {
      ComboBox_SetItemHeight( GetDlgItem( hwnd, IDD_FONTS ), -1, tm.tmHeight+8 );
      ComboBox_SetItemHeight( GetDlgItem( hwnd, IDD_FONTSIZE ), -1, tm.tmHeight+8 );
      }
   PostMessage( hwnd, WM_USER+0x202, 0, 0L );

   /* Make the status text non-bold.
    */
   SetWindowFont( GetDlgItem( hwnd, IDD_PAD1 ), hHelvB8Font, FALSE );
   return( TRUE );
}

/* This function handles the WM_COMMAND message. Currently only one control
 * on the dialog, the OK button, dispatches this message.
 */ 
LRESULT FASTCALL TerminalFonts_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsTERMINAL_FONT );
         break;

      case PSN_APPLY: {
         TERMINALWND FAR * ptwnd;
         char szSection[ LEN_TERMNAME+20+1 ];
         BOOL fChange;
         RECT rc;
         WORD wSize;

         /* Get the pointer to the properties record.
          */
         ptwnd = (TERMINALWND FAR *)GetWindowLong( hwnd, DWL_USER );
         fChange = FALSE;

         /* Do nothing unless the font attribute have REALLY
          * changed.
          */
         if( memcmp( &lfTmp, &lfPreTmp, sizeof( LOGFONT ) ) != 0 )
            {
            if( ptwnd->tp.hFont )
               DeleteFont( ptwnd->tp.hFont );
            fChange = TRUE;
            ptwnd->tp.hFont = CreateFontIndirect( &lfTmp );

            /* Now update the terminal window.
             */
            if( ptwnd->hwnd )
               {
               Terminal_ComputeCellDimensions( ptwnd->hwndTerm );
               GetClientRect( ptwnd->hwnd, &rc );
               wSize = IsIconic( ptwnd->hwnd ) ? SIZE_MINIMIZED : IsZoomed( hwnd ) ? SIZE_MAXIMIZED : SIZE_RESTORED;
               TerminalFrame_OnSize( ptwnd->hwnd, wSize, rc.right, rc.bottom );
               InvalidateRect( ptwnd->hwnd, NULL, FALSE );
               UpdateWindow( ptwnd->hwnd );
               }
            }

         /* Save the configuration.
          */
         wsprintf( szSection, "%s Properties", ptwnd->szName );
         Amuser_WritePPInt( szSection, "FontSize", lfTmp.lfHeight );
         Amuser_WritePPInt( szSection, "FontWeight", lfTmp.lfWeight );
         Amuser_WritePPInt( szSection, "FontItalic", lfTmp.lfItalic );
         Amuser_WritePPInt( szSection, "FontCharset", lfTmp.lfCharSet );
         Amuser_WritePPInt( szSection, "FontPitchAndFamily", lfTmp.lfPitchAndFamily );
         Amuser_WritePPString( szSection, "FontName", lfTmp.lfFaceName );

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This function handles the WM_MEASUREITEM message
 */
void FASTCALL TerminalFonts_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   HFONT hFont;
   HBITMAP hbmp;
   BITMAP bmp;
   HDC hdc;

   hdc = GetDC( hwnd );
   hFont = SelectFont( hdc, hHelvB8Font );
   GetTextMetrics( hdc, &tm );
   hbmp = LoadBitmap( hRscLib, MAKEINTRESOURCE(IDB_FONTBMPS) );
   GetObject( hbmp, sizeof( BITMAP ), &bmp );
   DeleteBitmap( hbmp );
   SelectFont( hdc, hFont );
   ReleaseDC( hwnd, hdc );
   lpMeasureItem->itemHeight = max( tm.tmHeight + tm.tmExternalLeading, bmp.bmHeight );
}

/* This function handles the WM_COMPAREITEM message
 */
int FASTCALL TerminalFonts_OnCompareItem( HWND hwnd, const COMPAREITEMSTRUCT FAR * lpCompareItem )
{
   switch( lpCompareItem->CtlID )
      {
      case IDD_FONTSIZE: {
         char chSize1[ 7 ];
         char chSize2[ 7 ];

         strcpy( chSize1, (LPSTR)lpCompareItem->itemData1 );
         strcpy( chSize2, (LPSTR)lpCompareItem->itemData2 );
         return( atoi( chSize2 ) - atoi( chSize1 ) );
         }
      }
   return( 0 );
}

/* This function handles the WM_DRAWITEM message
 */
void FASTCALL TerminalFonts_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   switch( lpDrawItem->CtlID )
      {
      case IDD_FONTS: {
         char sz[ LF_FACESIZE ];
         HBRUSH hbr;
         HFONT hFont;
         COLORREF tmpT, tmpB;
         COLORREF T, B;
         int index;
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

         if( nFontTypeByte[ lpDrawItem->itemID ] & TRUETYPE_FONTTYPE )
            index = 0;
         else if( nFontTypeByte[ lpDrawItem->itemID ] & DEVICE_FONTTYPE )
            index = 1;
         else
            index = -1;
         if( index != -1 )
            {
            HBITMAP hbmp;

            hbmp = LoadBitmap( hRscLib, MAKEINTRESOURCE(IDB_FONTBMPS) );
            Amuser_DrawBitmap( lpDrawItem->hDC, rc.left, rc.top, 20, 12, hbmp, index );
            DeleteBitmap( hbmp );
            }
         rc.left += 22;
         hFont = SelectFont( lpDrawItem->hDC, hHelvB8Font );
         DrawText( lpDrawItem->hDC, sz, -1, &rc, DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX );
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
         DrawText( lpDrawItem->hDC, sz, -1, &rc, DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX );
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
void FASTCALL TerminalFonts_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_FONTS:
         if( codeNotify == CBN_SELCHANGE )
            {
            PostMessage( hwnd, WM_USER+0x200, 0, 0L );
            PropSheet_Changed( GetParent( hwnd ), hwnd );
            }
         break;

      case IDD_FONTSTYLES:
      case IDD_FONTSIZE:
         if( codeNotify == CBN_SELCHANGE || codeNotify == CBN_KILLFOCUS )
            {
            PostMessage( hwnd, WM_USER+0x201, 0, 0L );
            PropSheet_Changed( GetParent( hwnd ), hwnd );
            }
         break;
      }
}

int EXPORT CALLBACK TerminalFillFontDialog( const ENUMLOGFONT FAR * lpnlf, const NEWTEXTMETRIC FAR * lpntm, int FontType, LPARAM lParam )
{
   HWND hwnd = (HWND)lParam;
   HWND hwndCmb;
   int index;
   int count;

   /* Reject proportional fonts.
    */
   hwndCmb = GetDlgItem( hwnd, IDD_FONTS );
   if( ( lpnlf->elfLogFont.lfPitchAndFamily & 0x3 ) != FIXED_PITCH )
      return( TRUE );
   if( ( count = ComboBox_GetCount( hwndCmb ) ) < 100 )
      {
      index = ComboBox_AddString( hwndCmb, lpnlf->elfLogFont.lfFaceName );
      ++count;
      if( count > 1 )
         {
         register int c;
   
         for( c = count - 1; c > index; --c )
            nFontTypeByte[ c ] = nFontTypeByte[ c - 1 ];
         }
      nFontTypeByte[ index ] = FontType;
      }
   return( TRUE );
}

/* This function updates the sample box with text shown
 * using the attributes of the selected font.
 */
void FASTCALL PaintSample( HWND hwnd, HDC hdc )
{
   RECT rc;
   HFONT hFont, hOldFont;
   HBRUSH hOldBrush;
   HBRUSH hbr;
   HPEN hOldPen;

   /* Compute the drawing area.
    */
   GetWindowRect( GetDlgItem( hwnd, IDD_SAMPLEBOX ), &rc );
   ScreenToClient( hwnd, (LPPOINT)&rc );
   ScreenToClient( hwnd, ((LPPOINT)&rc)+1 );

   /* Draw the surrounding box.
    */
   hbr = CreateSolidBrush( GetSysColor( COLOR_WINDOW ) );
   hOldBrush = SelectBrush( hdc, hbr );
   hOldPen = SelectPen( hdc, GetStockPen( BLACK_PEN ) );
   Rectangle( hdc, rc.left, rc.top, rc.right, rc.bottom );
   InflateRect( &rc, -1, -1 );

   /* Draw the text.
    */
   hFont = CreateFontIndirect( &lfTmp );
   hOldFont = SelectFont( hdc, hFont );
   SetBkColor( hdc, RGB(255,255,255) );
   SetTextColor( hdc, GetSysColor( COLOR_WINDOWTEXT ) );
   DrawText( hdc, "AaBbYyZz", -1, &rc, DT_SINGLELINE|DT_VCENTER|DT_CENTER );

   /* Clean up afterwards.
    */
   SelectFont( hdc, hOldFont );
   SelectBrush( hdc, hOldBrush );
   SelectPen( hdc, hOldPen );
   DeleteBrush( hbr );
   DeleteFont( hFont );
}

/* This function displays a message in the font dialog which
 * describes the attributes of the selected font, whether it be a
 * Truetype font, a raster font or a printer font.
 */
void FASTCALL ShowSelectedFontType( HWND hwnd, int index )
{
   char * psz;

   if( index == CB_ERR )
      psz = "";
   else if( nFontTypeByte[ index ] & TRUETYPE_FONTTYPE )
      psz = GS(IDS_STR530);
   else if( nFontTypeByte[ index ] & DEVICE_FONTTYPE )
      psz = GS(IDS_STR531);
   else
      psz = GS(IDS_STR532);
   SetDlgItemText( hwnd, IDD_PAD1, psz );
}

/* This function updates the lfTmp LOGFONT structure with the settings
 * from the font dialog.
 */
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
   ComboBox_GetText( hwndCmb, lfTmp.lfFaceName, LF_FACESIZE );

   hwndCmb = GetDlgItem( hwnd, IDD_FONTSTYLES );
   ComboBox_GetText( hwndCmb, sz, sizeof( sz ) );
   if( lstrcmpi( sz, pstrBold ) == 0 )
      iWeight = FW_BOLD;
   else if( lstrcmpi( sz, pstrItalic ) == 0 )
      fItalic = TRUE;
   else if( lstrcmpi( sz, pstrBoldItalic ) == 0 )
      {
      iWeight = FW_BOLD;
      fItalic = TRUE;
      }
   lfTmp.lfItalic = fItalic;
   lfTmp.lfWeight = iWeight;

   hwndCmb = GetDlgItem( hwnd, IDD_FONTSIZE );
   ComboBox_GetText( hwndCmb, sz, sizeof( sz ) );
   wHeight = atoi( sz );
   hdc = GetDC( hwnd );
   wHeight = -MulDiv( wHeight, GetDeviceCaps( hdc, LOGPIXELSY ), 72 );
   ReleaseDC( hwnd, hdc );

   lfTmp.lfHeight = wHeight;
   lfTmp.lfOutPrecision = OUT_DEFAULT_PRECIS;
   lfTmp.lfCharSet = DEFAULT_CHARSET;
   lfTmp.lfPitchAndFamily = FF_DONTCARE|DEFAULT_PITCH;
}

/* This function updates the font dialog with the settings
 * from the lfTmp LOGFONT structure.
 */
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
   nSel = ComboBox_RealFindStringExact( hwndCmb, lfTmp.lfFaceName );
   ComboBox_SetCurSel( hwndCmb, nSel );
   FillFontStylesAndSizes( hwnd );
   ShowSelectedFontType( hwnd, nSel );

   hwndCmb = GetDlgItem( hwnd, IDD_FONTSTYLES );
   if( lfTmp.lfWeight >= FW_BOLD && lfTmp.lfItalic )
      strcpy( sz, pstrBoldItalic );
   else if( lfTmp.lfWeight >= FW_BOLD )
      strcpy( sz, pstrBold );
   else if( lfTmp.lfItalic )
      strcpy( sz, pstrItalic );
   else
      strcpy( sz, pstrRegular );
   nSel = ComboBox_FindStringExact( hwndCmb, -1, sz );
   ComboBox_SetCurSel( hwndCmb, nSel );

   hwndCmb = GetDlgItem( hwnd, IDD_FONTSIZE );
   hdc = GetDC( hwnd );
   wHeight = -MulDiv( lfTmp.lfHeight, 72, GetDeviceCaps( hdc, LOGPIXELSY ) );
   ReleaseDC( hwnd, hdc );
   wsprintf( sz, "%u", wHeight );
   nSel = ComboBox_FindStringExact( hwndCmb, -1, sz );
   ComboBox_SetCurSel( hwndCmb, nSel );

   hdc = GetDC( hwnd );
   PaintSample( hwnd, hdc );
   ReleaseDC( hwnd, hdc );
}

/* This function dispatches messages for the Terminal Colours dialog.
 */
BOOL EXPORT CALLBACK TerminalColours( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, TerminalColours_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, TerminalColours_OnNotify );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, TerminalColours_OnCommand );
      }
   return( FALSE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL TerminalColours_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_CUSTFORE: {
         CHOOSECOLOR cc;
         HWND hwndWell;
         COLORREF aclrCust[ 16 ];
         register int i;
         TERMINALWND FAR * ptwnd;

         /* Get the pointer to the properties record.
          */
         ptwnd = (TERMINALWND FAR *)GetWindowLong( hwnd, DWL_USER );
         hwndWell = GetDlgItem( hwnd, IDD_FOREGWELL );

         /* Initialise the custom colour grid.
          */
         for( i = 0; i < 16; i++ )
            aclrCust[ i ] = RGB( 255, 255, 255 );

         /* Initialise the custom colour array.
          */
         cc.lStructSize = sizeof( CHOOSECOLOR );
         cc.hwndOwner = hwnd;
         cc.lpCustColors = aclrCust;
         cc.Flags = CC_RGBINIT;
         cc.rgbResult = SendMessage( hwndWell, WCM_GETSELECTEDCOLOUR, 0, 0L );
         if( ChooseColor( &cc ) )
            {
            COLORREF crTmp;
            int index;

            /* Hack! Replace the selected element in our local copy of the colour
             * map with the chosen colour, pass the map to the well control, then
             * restore the original colour.
             */
            index = (int)SendMessage( hwndWell, WCM_GETSELECTED, 0, 0L );
            crTmp = ptwnd->tp.lpcrFgMapTable[ index ];
            ptwnd->tp.lpcrFgMapTable[ index ] = cc.rgbResult;
            SendMessage( hwndWell, WCM_SETCOLOURARRAY, 8, (LPARAM)ptwnd->tp.lpcrFgMapTable );
            ptwnd->tp.lpcrFgMapTable[ index ] = crTmp;
            PropSheet_Changed( GetParent( hwnd ), hwnd );
            }
         break;
         }

      case IDD_CUSTBACK: {
         CHOOSECOLOR cc;
         HWND hwndWell;
         COLORREF aclrCust[ 16 ];
         register int i;
         TERMINALWND FAR * ptwnd;

         /* Get the pointer to the properties record.
          */
         ptwnd = (TERMINALWND FAR *)GetWindowLong( hwnd, DWL_USER );
         hwndWell = GetDlgItem( hwnd, IDD_BACKGWELL );

         /* Initialise the custom colour grid.
          */
         for( i = 0; i < 16; i++ )
            aclrCust[ i ] = RGB( 255, 255, 255 );

         /* Initialise the custom colour array.
          */
         cc.lStructSize = sizeof( CHOOSECOLOR );
         cc.hwndOwner = hwnd;
         cc.lpCustColors = aclrCust;
         cc.Flags = CC_RGBINIT;
         cc.rgbResult = SendMessage( hwndWell, WCM_GETSELECTEDCOLOUR, 0, 0L );
         if( ChooseColor( &cc ) )
            {
            COLORREF crTmp;
            int index;

            /* Hack! Replace the selected element in our local copy of the colour
             * map with the chosen colour, pass the map to the well control, then
             * restore the original colour.
             */
            index = (int)SendMessage( hwndWell, WCM_GETSELECTED, 0, 0L );
            crTmp = ptwnd->tp.lpcrBgMapTable[ index ];
            ptwnd->tp.lpcrBgMapTable[ index ] = cc.rgbResult;
            SendMessage( hwndWell, WCM_SETCOLOURARRAY, 8, (LPARAM)ptwnd->tp.lpcrBgMapTable );
            ptwnd->tp.lpcrBgMapTable[ index ] = crTmp;
            PropSheet_Changed( GetParent( hwnd ), hwnd );
            }
         break;
         }
      }
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL TerminalColours_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   TERMINALWND FAR * ptwnd;
   LPCAMPROPSHEETPAGE psp;

   /* Save the pointer to the terminal properties record.
    */
   psp = (LPCAMPROPSHEETPAGE)lParam;
   ptwnd = (TERMINALWND FAR *)psp->lParam;
   SetWindowLong( hwnd, DWL_USER, (LPARAM)ptwnd );

   /* Change the wells to use our own mappings.
    */
   SendMessage( GetDlgItem( hwnd, IDD_FOREGWELL ), WCM_SETCOLOURARRAY, 8, (LPARAM)ptwnd->tp.lpcrFgMapTable );
   SendMessage( GetDlgItem( hwnd, IDD_BACKGWELL ), WCM_SETCOLOURARRAY, 8, (LPARAM)ptwnd->tp.lpcrBgMapTable );
   SendMessage( GetDlgItem( hwnd, IDD_FOREGWELL ), WCM_SETITEMCOUNT, 0, MAKELPARAM( 8, 1 ) );
   SendMessage( GetDlgItem( hwnd, IDD_BACKGWELL ), WCM_SETITEMCOUNT, 0, MAKELPARAM( 8, 1 ) );

   /* Set the foreground and background well selections.
    */
   SendMessage( GetDlgItem( hwnd, IDD_FOREGWELL ), WCM_SETSELECTEDCOLOUR, 0, (LPARAM)ptwnd->tp.crTermText );
   SendMessage( GetDlgItem( hwnd, IDD_BACKGWELL ), WCM_SETSELECTEDCOLOUR, 0, (LPARAM)ptwnd->tp.crTermBack );
   return( TRUE );
}

/* This function handles the WM_NOTIFY message. Currently only one control
 * on the dialog, the OK button, dispatches this message.
 */ 
LRESULT FASTCALL TerminalColours_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsTERMINAL_COLOURS );
         break;

      case WCN_SELCHANGED:
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case PSN_APPLY: {
         TERMINALWND FAR * ptwnd;
         char szSection[ LEN_TERMNAME+20+1 ];
         COLORREF crMapTable[ 8 ];
         COLORREF cr;
         register int i;
         BOOL fChange;
         HWND hwndBgWell;
         HWND hwndFgWell;

         /* Get the pointer to the properties record.
          */
         ptwnd = (TERMINALWND FAR *)GetWindowLong( hwnd, DWL_USER );
         wsprintf( szSection, "%s Properties", ptwnd->szName );
         fChange = FALSE;

         /* First check the foreground colour array.
          */
         hwndFgWell = GetDlgItem( hwnd, IDD_FOREGWELL );
         SendMessage( hwndFgWell, WCM_GETCOLOURARRAY, 8, (LPARAM)(LPSTR)crMapTable );
         for( i = 0; i < 8; ++i )
            if( ptwnd->tp.lpcrFgMapTable[ i ] != crMapTable[ i ] )
               {
               char sz[ 20 ];

               ptwnd->tp.lpcrFgMapTable[ i ] = crMapTable[ i ];
               wsprintf( sz, "ForeColourMap%u", i );
               Amuser_WritePPLong( szSection, sz, crMapTable[ i ] );
               }
         cr = SendMessage( hwndFgWell, WCM_GETSELECTEDCOLOUR, 0, 0L );
         if( cr != ptwnd->tp.crTermText )
            {
            ptwnd->tp.crTermText = cr;
            fChange = TRUE;
            }

         /* Then check the background colour array.
          */
         hwndBgWell = GetDlgItem( hwnd, IDD_BACKGWELL );
         SendMessage( hwndBgWell, WCM_GETCOLOURARRAY, 8, (LPARAM)(LPSTR)crMapTable );
         for( i = 0; i < 8; ++i )
            if( ptwnd->tp.lpcrBgMapTable[ i ] != crMapTable[ i ] )
               {
               char sz[ 20 ];

               ptwnd->tp.lpcrBgMapTable[ i ] = crMapTable[ i ];
               wsprintf( sz, "BackColourMap%u", i );
               Amuser_WritePPLong( szSection, sz, crMapTable[ i ] );
               }
         cr = SendMessage( hwndBgWell, WCM_GETSELECTEDCOLOUR, 0, 0L );
         if( cr != ptwnd->tp.crTermBack )
            {
            ptwnd->tp.crTermBack = cr;
            fChange = TRUE;
            }

         /* Update the configuration file.
          */
         Amuser_WritePPLong( szSection, "TextColor", ptwnd->tp.crTermText );
         Amuser_WritePPLong( szSection, "BackColor", ptwnd->tp.crTermBack );

         /* Now update the terminal window.
          */
         if( fChange && ptwnd->hwnd )
            {
            if( ptwnd->tp.fAnsi )
               {
               SetFgAttr( ptwnd->crAttrByte, FgRGBToB4( ptwnd, ptwnd->tp.crTermText ) );
               SetBgAttr( ptwnd->crAttrByte, BgRGBToB4( ptwnd, ptwnd->tp.crTermBack ) );
               ptwnd->crRealAttrByte = ptwnd->crAttrByte;
               memset( ptwnd->lpColourBuf, ptwnd->crAttrByte, ptwnd->tp.nBufferSize * ptwnd->tp.nLineWidth );
               }
            InvalidateRect( ptwnd->hwnd, NULL, FALSE );
            UpdateWindow( ptwnd->hwnd );
            }

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This function dispatches messages for the Terminal Emulations dialog.
 */
BOOL EXPORT CALLBACK TerminalEmulations( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, TerminalEmulations_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, TerminalEmulations_OnNotify );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, TerminalEmulations_OnCommand );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL TerminalEmulations_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   TERMINALWND FAR * ptwnd;
   LPCAMPROPSHEETPAGE psp;
   HWND hwndList;

   /* Save the pointer to the terminal properties record.
    */
   psp = (LPCAMPROPSHEETPAGE)lParam;
   ptwnd = (TERMINALWND FAR *)psp->lParam;
   SetWindowLong( hwnd, DWL_USER, (LPARAM)ptwnd );

   /* Fill the listbox with the list of emulations.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   ListBox_AddString( hwndList, "Ansi (DEC VT100)" );
   ListBox_AddString( hwndList, "Generic TTY" );
   if( ptwnd->tp.fAnsi )
      {
      ListBox_SetCurSel( hwndList, 0 );
      SetDlgItemText( hwnd, IDD_PAD1, GS(IDS_STR687 ) );
      }
   else
      {
      ListBox_SetCurSel( hwndList, 1 );
      SetDlgItemText( hwnd, IDD_PAD1, GS(IDS_STR688 ) );
      }
   return( TRUE );
}

/* This function handles the WM_COMMAND message. Currently only one control
 * on the dialog, the OK button, dispatches this message.
 */ 
LRESULT FASTCALL TerminalEmulations_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsTERMINAL_EMULATIONS );
         break;

      case PSN_APPLY: {
         TERMINALWND FAR * ptwnd;
         char szEmulator[ 20 ];
         char szSection[ LEN_TERMNAME+20+1 ];
         HWND hwndList;
         BOOL fAnsi;
         int index;

         /* Get the pointer to the properties record.
          */
         ptwnd = (TERMINALWND FAR *)GetWindowLong( hwnd, DWL_USER );

         /* Get and store the emulation.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         index = ListBox_GetCurSel( hwndList );
         ASSERT( index != LB_ERR );

         /* Read the connection name
          */
         memset( szEmulator, '\0', sizeof( szEmulator ) );
         ListBox_GetText( hwndList, index, szEmulator );
         ASSERT( szEmulator[ sizeof( szEmulator ) - 1 ] == '\0' );

         /* Set Ansi as appropriate.
          */
         fAnsi = lstrcmp( szEmulator, "Ansi (DEC VT100)" ) == 0;
         if( fAnsi != ptwnd->tp.fAnsi )
            /* Update the terminal window if it changed.
             */
            if( ptwnd->hwnd )
               {
               Terminal_SetAnsi( ptwnd->hwndTerm, fAnsi );
               InvalidateRect( ptwnd->hwndTerm, NULL, FALSE );
               UpdateWindow( ptwnd->hwndTerm );
               }
         wsprintf( szSection, "%s Properties", ptwnd->szName );
         Amuser_WritePPInt( szSection, "Ansi", ptwnd->tp.fAnsi );

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL TerminalEmulations_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_LIST:
         if( codeNotify == LBN_SELCHANGE )
            {
            int index;

            index = ListBox_GetCurSel( hwndCtl );
            if( index == 0 )
               SetDlgItemText( hwnd, IDD_PAD1, GS(IDS_STR687 ) );
            else
               SetDlgItemText( hwnd, IDD_PAD1, GS(IDS_STR688 ) );
            PropSheet_Changed( GetParent( hwnd ), hwnd );
            }
         break;
      }
}

