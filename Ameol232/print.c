/* PRINT.C - Print code and interface
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
#include <memory.h>
#include <cderr.h>
#include <drivinit.h>
#include "print.h"

static BOOL fDefDlgEx = FALSE;      /* DefDlg recursion flag trap */
static HANDLE hDevMode = NULL;      /* Handle of current device mode */
static HANDLE hDevNames = NULL;     /* Handle of current device names */
static HWND hwndPDlg;               /* Handle of print abort dialog */
static BOOL fAbort;                 /* TRUE if the user has aborted */
static int IOStatus;                /* I/O status word */

BOOL FASTCALL Printing_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL Printing_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL Printing_DlgProc( HWND, UINT, WPARAM, LPARAM );

static int FASTCALL ReportPrintError( HWND, int );
static int FASTCALL ClipLine( HDC, LPSTR, int *, int );
//static LPSTR FASTCALL GetDeviceName( PRINTDLG FAR * );
static LPSTR FASTCALL GetDeviceName( PAGESETUPDLG FAR * );
//static LPSTR FASTCALL GetPortName( PRINTDLG FAR * );
static LPSTR FASTCALL GetPortName( PAGESETUPDLG FAR * );

//static LPSTR FASTCALL GetDriverName( PRINTDLG FAR * );
static LPSTR FASTCALL GetDriverName( PAGESETUPDLG FAR * );

UINT EXPORT CALLBACK CommonPrintDlgHook( HWND, UINT, WPARAM, LPARAM );
BOOL EXPORT CALLBACK PrintDlgProc( HWND, UINT, WPARAM, LPARAM );
BOOL EXPORT CALLBACK AbortProc( HDC, int );

/* This function returns whether or not a printer driver
 * is installed. Because it needs to be fast, it relies on
 * us having called UpdatePrinterSelection in the past.
 */
BOOL FASTCALL IsPrinterDriver( void )
{
   return( hDevMode != NULL );
}

/* This function invokes the Print setup dialog using the
 * printer common dialog.
 */
/*void FASTCALL PrintSetup( HWND hwndParent )
{
   PRINTDLG pd;
   int nResponse;

   // MUST have a printer driver.
   //
   if( !IsPrinterDriver() )
      return;

   // Initialise the pd structure.
   //
   memset( &pd, 0, sizeof( PRINTDLG ) );
   pd.lStructSize = sizeof( PRINTDLG );
   pd.hwndOwner = hwndParent;
   pd.Flags = PD_PRINTSETUP;
   pd.lpfnSetupHook = NULL;

   // Get the current devmode structure.
   //
   UpdatePrinterSelection( FALSE );
   pd.hDevNames = hDevNames;
   pd.hDevMode = hDevMode;
   if( ( nResponse = PrintDlg( &pd ) ) != IDOK )
      {
      switch( CommDlgExtendedError() )
         {
         case PDERR_PRINTERNOTFOUND:
         case PDERR_DNDMMISMATCH:
            if( pd.hDevNames )
               {
               GlobalFree( pd.hDevNames );
               pd.hDevNames = NULL;
               hDevNames = NULL;
               }
            if( pd.hDevMode )
               {
               GlobalFree( pd.hDevMode );
               pd.hDevMode = NULL;
               hDevMode = NULL;
               }
            break;

         default:
            return;
         }
      }

   // Update the devmode structure from whatever the
   // user selected.
   //
   hDevMode = pd.hDevMode;
   hDevNames = pd.hDevNames;
}
*/

void FASTCALL PrintSetup( HWND hwndParent )
{
   PAGESETUPDLG pd;
   int nResponse;
//  !!SM!! 2064
   LPDEVMODE dm;
   float nLeftMarginT;
   float nRightMarginT;
   float nTopMarginT;
   float nBottomMarginT;
   int   nOrientation;

   /* MUST have a printer driver.
    */
   if( !IsPrinterDriver() )
      return;

   /* Initialise the pd structure.
    */
   memset( &pd, 0, sizeof( PAGESETUPDLG ) );
   pd.lStructSize = sizeof( PAGESETUPDLG );
   pd.hwndOwner = hwndParent;
   //pd.Flags = PD_PRINTSETUP;
// pd.lpfnSetupHook = NULL;

   /* Get the current devmode structure.
    */
   UpdatePrinterSelection( FALSE );
//  !!SM!! 2064
   nLeftMarginT   = (Amuser_GetPPFloat( szPrinter, "LeftMargin", 1.91f ) * 1000 );
   nRightMarginT  = (Amuser_GetPPFloat( szPrinter, "RightMargin", 1.91f ) * 1000 );
   nTopMarginT    = (Amuser_GetPPFloat( szPrinter, "TopMargin", 2.54f ) * 1000 ) ;
   nBottomMarginT = (Amuser_GetPPFloat( szPrinter, "BottomMargin", 2.54f ) * 1000);
   nOrientation   = (Amuser_GetPPInt( szPrinter, "Orientation", PR_PORTRAIT ));

   dm = GlobalLock( hDevMode );
   dm->dmFields = dm->dmFields | DM_ORIENTATION;
   dm->dmOrientation = nOrientation + 1;
   GlobalUnlock( dm );

   pd.Flags = PSD_INHUNDREDTHSOFMILLIMETERS | PSD_MARGINS;

   pd.hDevNames = hDevNames;
   pd.hDevMode = hDevMode;

//  !!SM!! 2064
   pd.rtMargin.left   = (long)nLeftMarginT;
   pd.rtMargin.right  = (long)nRightMarginT;
   pd.rtMargin.top    = (long)nTopMarginT;
   pd.rtMargin.bottom = (long)nBottomMarginT;


   if( ( nResponse = PageSetupDlg( &pd ) ) != IDOK )
      {
      switch( CommDlgExtendedError() )
         {
         case PDERR_PRINTERNOTFOUND:
         case PDERR_DNDMMISMATCH:
            if( pd.hDevNames )
               {
               GlobalFree( pd.hDevNames );
               pd.hDevNames = NULL;
               hDevNames = NULL;
               }
            if( pd.hDevMode )
               {
               GlobalFree( pd.hDevMode );
               pd.hDevMode = NULL;
               hDevMode = NULL;
               }
            break;

         default:
            return;
         }
      }

   /* Update the devmode structure from whatever the
    * user selected.
    */
   hDevMode = pd.hDevMode;
   hDevNames = pd.hDevNames;

//  !!SM!! 2064
   dm = GlobalLock( hDevMode );

   nLeftMarginT   = (pd.rtMargin.left / 1000.0f );
   nRightMarginT  = (pd.rtMargin.right / 1000.0f );
   nTopMarginT    = (pd.rtMargin.top / 1000.0f );
   nBottomMarginT = (pd.rtMargin.bottom / 1000.0f );
   nOrientation   = (dm->dmOrientation - 1);

   Amuser_WritePPFloat( szPrinter, "LeftMargin", nLeftMarginT );
   Amuser_WritePPFloat( szPrinter, "RightMargin", nRightMarginT );
   Amuser_WritePPFloat( szPrinter, "TopMargin", nTopMarginT );
   Amuser_WritePPFloat( szPrinter, "BottomMargin", nBottomMarginT );
   if (dm->dmFields & DM_ORIENTATION)
      Amuser_WritePPInt( szPrinter, "Orientation", nOrientation );

   GlobalUnlock( dm );
}

/* This function begins a print job. We fill out the
 * PRINTSTRUCT structure with settings for the other
 * functions to use.
 */
HPRINT FASTCALL BeginPrint( HWND hwnd, char * lpszJobDescription, LOGFONT FAR * lplf )
{
   DOCINFO di;
   POINT offset;
   POINT page;
   float cxPixelsPerMM;
   float cyPixelsPerMM;
   HPRINT lpps;
   RECT rc;

   INITIALISE_PTR(lpps);
   
   /* Allocate a print structure.
    */
   if( !fNewMemory( &lpps, sizeof(PRINTSTRUCT ) ) )
      return( NULL );
   lpps->hwnd = hwnd;
   lpps->hCurFont = NULL;

   /* Disable the main window and create a print status
    * dialog box.
    */
   EnableWindow( hwndFrame, FALSE );
   hwndPDlg = Adm_CreateDialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_PRTABORTDLG), (DLGPROC)PrintDlgProc, (LONG)(LPSTR)lpszJobDescription );
   ShowWindow( hwndPDlg, SW_SHOW );
   UpdateWindow( hwndPDlg );

   /* Get the printer DC.
    */
   lpps->hDC = CreateCurrentPrinterDC();

   /* Set the abort proc!
    */
   SetAbortProc( lpps->hDC, AbortProc );
   fAbort = FALSE;

   /* Set the default font.
    */
   switch( nPrinterFont )
      {
      case PFON_DEFAULT: {
         TEXTMETRIC tm;

         SelectFont( lpps->hDC, GetStockFont( DEVICE_DEFAULT_FONT ) );
         GetTextMetrics( lpps->hDC, &tm );
         lpps->dy = tm.tmHeight + tm.tmExternalLeading;
         break;
         }

      case PFON_WINDOW:
         PrintSelectFont( lpps, lplf );
         break;

      case PFON_PRINTERFONT:
         PrintSelectFont( lpps, &lfPrinterFont );
         break;
      }

   /* Compute the margins, in device pixels.
    */
   cxPixelsPerMM = (float)GetDeviceCaps( lpps->hDC, HORZRES ) / (float)GetDeviceCaps( lpps->hDC, HORZSIZE );
   cyPixelsPerMM = (float)GetDeviceCaps( lpps->hDC, VERTRES ) / (float)GetDeviceCaps( lpps->hDC, VERTSIZE );

   /* Get the page size and reduce by the margin widths.
    */
   Escape( lpps->hDC, GETPHYSPAGESIZE, 0, NULL, &page );
   DPtoLP( lpps->hDC, &page, 1 );
   page.x = (WORD)( page.x / cxPixelsPerMM );
   page.y = (WORD)( page.y / cyPixelsPerMM );
   lpps->x1 = (WORD)( cxPixelsPerMM * ( nLeftMargin * 10 ) );
   lpps->y1 = (WORD)( cyPixelsPerMM * ( nTopMargin * 10 ) );
   lpps->x2 = (WORD)( cxPixelsPerMM * ( page.x - ( nRightMargin * 10 ) ) );
   lpps->y2 = (WORD)( cyPixelsPerMM * ( page.y - ( nBottomMargin * 10 ) ) );

   /* Adjust the page rectangle by the printing offset.
    */
   Escape( lpps->hDC, GETPRINTINGOFFSET, 0, NULL, &offset );
   DPtoLP( lpps->hDC, &offset, 1 );
   lpps->x1 -= (WORD)( ( offset.x * 10 ) / cxPixelsPerMM );
   lpps->y1 -= (WORD)( ( offset.y * 10 ) / cyPixelsPerMM );
   lpps->x2 -= ( (WORD)( ( offset.x * 10 ) / cxPixelsPerMM ) );
   lpps->y2 -= ( (WORD)( ( offset.y * 10 ) / cyPixelsPerMM ) );

   /* Set the print "cursor" start to the top left of the page.
    */
   lpps->x = lpps->x1;
   lpps->y = lpps->y1;

   /* Set the page number. This gets incremented BEFORE we start
    * a new page, so we set it to zero.
    */
   lpps->nPage = 0;

   /* Start the print job.
    */
   di.cbSize = sizeof( DOCINFO );
   di.lpszDocName = lpszJobDescription;
   di.lpszOutput = NULL;
#ifdef WIN32
   di.lpszDatatype = "emf";
   di.fwType = 0;
#endif
   if( ( IOStatus = StartDoc( lpps->hDC, &di ) ) == SP_ERROR )
      {
      fMessageBox( hwnd, 0, GS(IDS_STR587), MB_OK|MB_ICONEXCLAMATION );
      DestroyWindow( hwndPDlg );
      EnableWindow( hwndFrame, TRUE );
      return( FALSE );
      }

   /* Compute the header and footer heights.
    */
   SetRect( &rc, lpps->x1, lpps->y1, lpps->x2, lpps->y2 );
   lpps->cyHeader = DrawHeader( lpps->hDC, &rc, lpps, lpLeftHeader, lpCenterHeader, lpRightHeader, TRUE ) + lpps->dy;
   lpps->cyFooter = DrawFooter( lpps->hDC, &rc, lpps, lpLeftFooter, lpCenterFooter, lpRightFooter, TRUE ) + lpps->dy;
   return( lpps );
}

/* This function sets the font used by the print routines.
 */
void FASTCALL PrintSelectFont( HPRINT lpps, LOGFONT FAR * lplf )
{
   TEXTMETRIC tm;
   LOGFONT lf;

   lf = *lplf;
   if( lf.lfHeight < 0 )   
      {
      HDC hdc;
      int wHeight;

      hdc = GetDC( lpps->hwnd );
      wHeight = -MulDiv( lf.lfHeight, 72, GetDeviceCaps( hdc, LOGPIXELSY ) );
      ReleaseDC( lpps->hwnd, hdc );
      lf.lfHeight = -MulDiv( wHeight, GetDeviceCaps( lpps->hDC, LOGPIXELSY ), 72 );
      }
   if( NULL != lpps->hCurFont )
      DeleteFont( lpps->hCurFont );
   lpps->hCurFont = CreateFontIndirect( &lf );

   /* Compute the new line height based on the print font.
    */
   SelectFont( lpps->hDC, lpps->hCurFont );
   GetTextMetrics( lpps->hDC, &tm );
   lpps->dy = tm.tmHeight + tm.tmExternalLeading;
}

BOOL FASTCALL PrintLine( HPRINT lpps, LPSTR lpText )
{
   TextOut( lpps->hDC, lpps->x, lpps->y, lpText, lstrlen( lpText ) );
   lpps->y += lpps->dy;
   if( lpps->y + lpps->dy > lpps->y2 - ( lpps->dy * 2 ) )
      {
      if( !EndPrintPage( lpps ) )
         return( FALSE );
      if( !BeginPrintPage( lpps ) )
         return( FALSE );
      }
   return( TRUE );
}

/* This function starts a new output page. It draws any header
 * at the top of the page and advances the print cursor to the
 * first line.
 */
BOOL FASTCALL BeginPrintPage( HPRINT lpps )
{
   int IOStatus;
   RECT rc;

   IOStatus = StartPage( lpps->hDC );
   IOStatus = ReportPrintError( lpps->hwnd, IOStatus );
   SelectFont( lpps->hDC, lpps->hCurFont );
   ++lpps->nPage;
   SetRect( &rc, lpps->x, lpps->y1, lpps->x2, lpps->y2 );
   DrawHeader( lpps->hDC, &rc, lpps, lpLeftHeader, lpCenterHeader, lpRightHeader, FALSE );
   lpps->y = lpps->y1 + lpps->cyHeader;
   return( IOStatus >= 0 );
}

/* This function finishes one page. It prints any footer at the bottom
 * and flushes the page to the print engine.
 */
BOOL FASTCALL EndPrintPage( HPRINT lpps )
{
   int IOStatus = 0;

   if( lpps->y > lpps->y1 )
      {
      RECT rc;

      SetRect( &rc, lpps->x, ( lpps->y2 - lpps->cyFooter ) + lpps->dy, lpps->x2, lpps->y2 );
      DrawFooter( lpps->hDC, &rc, lpps, lpLeftFooter, lpCenterFooter, lpRightFooter, FALSE );
      IOStatus = EndPage( lpps->hDC );
      IOStatus = ReportPrintError( lpps->hwnd, IOStatus );
      }
   return( IOStatus >= 0 );
}

/* This function resets the page number back to one. We actually
 * set it to zero as this function should precede BeginPrintPage.
 */
void FASTCALL ResetPageNumber( HPRINT lpps )
{
   lpps->nPage = 0;
}

BOOL FASTCALL PrintPage( HPRINT lpps, HPSTR lpText )
{
   int aCharWidths[ 256 ];

   if( GetCharWidth( lpps->hDC, 0, 255, aCharWidths ) == 0 )
      {
      register int c;
      char sz[ 2 ];

      sz[ 1 ] = '\0';
      for( c = 0; c < 256; ++c )
         {
         SIZE size;

         sz[ 0 ] = c;
         GetTextExtentPoint( lpps->hDC, sz, 1, &size );
         aCharWidths[ c ] = size.cx;
         }
      }
   while( !fAbort && *lpText )
      {
      register int c;
   
      if( lpps->y + lpps->dy > lpps->y2 - lpps->cyFooter )
         {
         if( !EndPrintPage( lpps ) )
            return( FALSE );
         if( !BeginPrintPage( lpps ) )
            return( FALSE );
         }
      c = ClipLine( lpps->hDC, lpText, aCharWidths, lpps->x2 - lpps->x1 );
      if( lpText[ c ] != CR && lpText[ c ] != LF && lpText[ c ] != ' ' && lpText[ c ] )
         {
         register int p;

         p = c;
         while( c > 0 ) {
            if( lpText[ c ] == ' ' )
               break;
            --c;
            }
         if( c == 0 )
            c = p;
         }
      if( lpText[ c ] == ' ' )
         ++c;
      TabbedTextOut( lpps->hDC, lpps->x, lpps->y, lpText, c, 0, NULL, lpps->x );
      lpps->y += lpps->dy;
      lpText += c;
      if( *lpText == CR ) ++lpText;
      if( *lpText == LF ) ++lpText;
      }
   return( TRUE );
}

BOOL FASTCALL EndPrint( HPRINT lpps )
{
   if( fAbort )
      IOStatus = AbortDoc( lpps->hDC );
   if( IOStatus >= 0 && !fAbort )
   {
      EndDoc( lpps->hDC );
      EndDoc( lpps->hDC );
   }
   EnableWindow( hwndFrame, TRUE );
   DestroyWindow( hwndPDlg );
   DeleteFont( lpps->hCurFont );
   DeleteDC( lpps->hDC );
   FreeMemory( &lpps );
   return( TRUE );
}

static int FASTCALL ClipLine( HDC hdc, LPSTR lpText, int * pChrWidths, int cxLine )
{
   int cx;
   register int n;

   for( n = cx = 0; lpText[ n ] && lpText[ n ] != CR; ++n )
      {
      UINT index = (UINT)(unsigned char)lpText[ n ];

      cx += pChrWidths[ index ];
      if( cx > cxLine )
         break;
      }
   if( cx > cxLine )
      do {
         n--;
         cx = LOWORD( GetTabbedTextExtent( hdc, lpText, n, 0, NULL ) );
         }
      while( cx > cxLine );
   return( n );
}

BOOL EXPORT CALLBACK AbortProc( HDC hDC, int reserved )
{
   MSG msg;

   while( !fAbort && PeekMessage( &msg, NULL, 0, 0, TRUE ) )
      if( !hwndPDlg || !IsDialogMessage( hwndPDlg, &msg ) )
         {
         TranslateMessage( &msg );
         DispatchMessage( &msg );
         }
   return( !fAbort );
}

BOOL EXPORT CALLBACK PrintDlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, Printing_DlgProc( hwnd, message, wParam, lParam ) ) );
}

LRESULT FASTCALL Printing_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, Printing_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, Printing_OnCommand );

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/*BOOL FASTCALL Printing_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   char aBuffer[ 80 ];
   PRINTDLG pd;

   pd.hDevNames = hDevNames;
   wsprintf( aBuffer, GS(IDS_STR589), (LPSTR)lParam );
   SetDlgItemText( hwnd, IDD_PAD2, aBuffer );
   wsprintf( aBuffer, GS(IDS_STR590), GetDeviceName( &pd ), GetPortName( &pd ) );
   SetDlgItemText( hwnd, IDD_PAD3, aBuffer );
   SetFocus( GetDlgItem( hwnd, IDCANCEL ) );
   return( TRUE );
} */

BOOL FASTCALL Printing_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   char aBuffer[ 80 ];
   PAGESETUPDLG pd;

   pd.hDevNames = hDevNames;
   wsprintf( aBuffer, GS(IDS_STR589), (LPSTR)lParam );
   SetDlgItemText( hwnd, IDD_PAD2, aBuffer );
   wsprintf( aBuffer, GS(IDS_STR590), GetDeviceName( &pd ), GetPortName( &pd ) );
   SetDlgItemText( hwnd, IDD_PAD3, aBuffer );
   SetFocus( GetDlgItem( hwnd, IDCANCEL ) );
   return( TRUE );
}

void FASTCALL Printing_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   if( id == IDCANCEL )
      fAbort = TRUE;
}

static int FASTCALL ReportPrintError( HWND hwnd, int IOStatus )
{
   if( IOStatus >= 0 )
      return( IOStatus );
   if( ( IOStatus & SP_NOTREPORTED ) == 0 )
      return( IOStatus );
   if( fAbort && IOStatus == SP_ERROR )
      IOStatus = SP_USERABORT;
   switch( IOStatus )
      {
      case SP_OUTOFDISK:
         fMessageBox( hwnd, 0, GS(IDS_STR591), MB_OK|MB_ICONEXCLAMATION );
         IOStatus &= ~SP_NOTREPORTED;
         break;

      case SP_OUTOFMEMORY:
         fMessageBox( hwnd, 0, GS(IDS_STR592), MB_OK|MB_ICONEXCLAMATION );
         IOStatus &= ~SP_NOTREPORTED;
         break;

      case SP_USERABORT:
      case SP_APPABORT:
         fMessageBox( hwnd, 0, GS(IDS_STR593), MB_OK|MB_ICONEXCLAMATION );
         IOStatus &= ~SP_NOTREPORTED;
         break;

      case SP_ERROR:
         fMessageBox( hwnd, 0, GS(IDS_STR594), MB_OK|MB_ICONEXCLAMATION );
         IOStatus &= ~SP_NOTREPORTED;
         break;
      }
   return( IOStatus );
}

/* This function updates the hDevNames and hDevMode structure by
 * setting them for the default printer. If bForceDefaults is TRUE,
 * we set them to the default printer settings. Otherwise we update
 * the hDevNames and hDevMode structure only if they currently refer
 * to a default printer.
 */
/*
BOOL FASTCALL UpdatePrinterSelectionOld( BOOL bForceDefaults )
{
   if( !bForceDefaults && hDevNames != NULL )
      {
      LPDEVNAMES lpDevNames;

      // If lpDevNames is not a default printer, do
      // nothing.
      //
      lpDevNames = GlobalLock( hDevNames );
      if( ( lpDevNames->wDefault & DN_DEFAULTPRN ) != 0 )
         {
         PRINTDLG pd;

         // Call PrintDlg to get the default printer
         // devmode and devnames structures.
         //
         memset( &pd, 0, sizeof( PRINTDLG ) );
         pd.lStructSize = sizeof( PRINTDLG );
         pd.Flags = PD_RETURNDEFAULT;
         PrintDlg( &pd );
         if( lstrcmp( (LPSTR)lpDevNames + lpDevNames->wDeviceOffset, GetDeviceName( &pd ) ) )
            {
            // If default printer has changed, update our
            // local settings.
            //
            if( hDevMode )
               GlobalFree( hDevMode );
            GlobalUnlock( hDevNames );
            GlobalFree( hDevNames );
            hDevMode = pd.hDevMode;
            hDevNames = pd.hDevNames;
            lpDevNames = GlobalLock( hDevNames );
            }
         }
      GlobalUnlock( hDevNames );
      }
   else {
      PRINTDLG pd;

      // Get the default settings and use them regardless
      // of whatever we had before.
      //
      memset( &pd, 0, sizeof( PRINTDLG ) );
      pd.lStructSize = sizeof( PRINTDLG );
      pd.Flags = PD_RETURNDEFAULT;
      PrintDlg( &pd );
      if( hDevMode )
         GlobalFree( hDevMode );
      if( hDevNames )
         GlobalFree( hDevNames );
      hDevMode = pd.hDevMode;
      hDevNames = pd.hDevNames;
      }
   return( TRUE );
}
*/

BOOL FASTCALL UpdatePrinterSelectionOld( BOOL bForceDefaults )
{
   if( !bForceDefaults && hDevNames != NULL )
      {
      LPDEVNAMES lpDevNames;

      /* If lpDevNames is not a default printer, do
       * nothing.
       */
      lpDevNames = GlobalLock( hDevNames );
      if( ( lpDevNames->wDefault & DN_DEFAULTPRN ) != 0 )
         {
         PAGESETUPDLG pd;

         /* Call PrintDlg to get the default printer
          * devmode and devnames structures.
          */
         memset( &pd, 0, sizeof( PAGESETUPDLG ) );
         pd.lStructSize = sizeof( PAGESETUPDLG );
         pd.Flags = PD_RETURNDEFAULT;
         PageSetupDlg( &pd );
         if( lstrcmp( (LPSTR)lpDevNames + lpDevNames->wDeviceOffset, GetDeviceName( &pd ) ) )
            {
            /* If default printer has changed, update our
             * local settings.
             */
            if( hDevMode )
               GlobalFree( hDevMode );
            GlobalUnlock( hDevNames );
            GlobalFree( hDevNames );
            hDevMode = pd.hDevMode;
            hDevNames = pd.hDevNames;
            lpDevNames = GlobalLock( hDevNames );
            }
         }
      GlobalUnlock( hDevNames );
      }
   else {
      PAGESETUPDLG pd;

      /* Get the default settings and use them regardless
       * of whatever we had before.
       */
      memset( &pd, 0, sizeof( PAGESETUPDLG ) );
      pd.lStructSize = sizeof( PAGESETUPDLG );
      pd.Flags = PD_RETURNDEFAULT;
      PageSetupDlg( &pd );
      if( hDevMode )
         GlobalFree( hDevMode );
      if( hDevNames )
         GlobalFree( hDevNames );
      hDevMode = pd.hDevMode;
      hDevNames = pd.hDevNames;
      }
   return( TRUE );
}


BOOL FASTCALL UpdatePrinterSelection( BOOL bForceDefaults )
{
   if( !bForceDefaults && hDevNames != NULL )
      {
      LPDEVNAMES lpDevNames;

      /* If lpDevNames is not a default printer, do
       * nothing.
       */
      lpDevNames = GlobalLock( hDevNames );
      if( ( lpDevNames->wDefault & DN_DEFAULTPRN ) != 0 )
         {
         PAGESETUPDLG pd;

         /* Call PrintDlg to get the default printer
          * devmode and devnames structures.
          */
         memset( &pd, 0, sizeof( PAGESETUPDLG ) );
         pd.lStructSize = sizeof( PAGESETUPDLG );
         pd.Flags = PSD_RETURNDEFAULT | PSD_NOWARNING; // 20060228 - 2.56.2049.03
         PageSetupDlg( &pd );
         if( lstrcmp( (LPSTR)lpDevNames + lpDevNames->wDeviceOffset, GetDeviceName( &pd ) ) )
            {
            /* If default printer has changed, update our
             * local settings.
             */
            if( hDevMode )
               GlobalFree( hDevMode );
            GlobalUnlock( hDevNames );
            GlobalFree( hDevNames );
            hDevMode = pd.hDevMode;
            hDevNames = pd.hDevNames;
            lpDevNames = GlobalLock( hDevNames );
            }
         }
      GlobalUnlock( hDevNames );
      }
   else {
      PAGESETUPDLG pd;

      /* Get the default settings and use them regardless
       * of whatever we had before.
       */
      memset( &pd, 0, sizeof( PAGESETUPDLG ) );
      pd.lStructSize = sizeof( PAGESETUPDLG );
      pd.Flags = PSD_RETURNDEFAULT | PSD_NOWARNING; // 20060228 - 2.56.2049.03
      PageSetupDlg( &pd );
      if( hDevMode )
         GlobalFree( hDevMode );
      if( hDevNames )
         GlobalFree( hDevNames );
      hDevMode = pd.hDevMode;
      hDevNames = pd.hDevNames;
      }
   return( TRUE );
}
/* This function extracts the driver name from the DevNames
 * structure.
 */
/*static LPSTR FASTCALL GetDriverName( PRINTDLG FAR * nppd )
{
   LPDEVNAMES lpDev;
   LPSTR lpDriverName;

   if( nppd->hDevNames )
      {
      lpDev = (LPDEVNAMES)GlobalLock( nppd->hDevNames );
      lpDriverName = (LPSTR)(lpDev) + (UINT)(lpDev->wDriverOffset);
      GlobalUnlock( nppd->hDevNames );
      }
   else
      lpDriverName = "";
   return( lpDriverName );
}
*/
static LPSTR FASTCALL GetDriverName( PAGESETUPDLG FAR * nppd )
{
   LPDEVNAMES lpDev;
   LPSTR lpDriverName;

   if( nppd->hDevNames )
      {
      lpDev = (LPDEVNAMES)GlobalLock( nppd->hDevNames );
      lpDriverName = (LPSTR)(lpDev) + (UINT)(lpDev->wDriverOffset);
      GlobalUnlock( nppd->hDevNames );
      }
   else
      lpDriverName = "";
   return( lpDriverName );
}

/* This function extracts the device name from the DevNames
 * structure.
 */
/*static LPSTR FASTCALL GetDeviceName( PRINTDLG FAR * nppd )
{
   LPDEVNAMES lpDev;
   LPSTR lpDeviceName;

   if( nppd->hDevNames )
      {
      lpDev = (LPDEVNAMES)GlobalLock( nppd->hDevNames );
      lpDeviceName = (LPSTR)(lpDev) + (UINT)(lpDev->wDeviceOffset);
      GlobalUnlock( nppd->hDevNames );
      }
   else
      lpDeviceName = "";
   return( lpDeviceName );
}*/

static LPSTR FASTCALL GetDeviceName( PAGESETUPDLG FAR * nppd )
{
   LPDEVNAMES lpDev;
   LPSTR lpDeviceName;

   if( nppd->hDevNames )
      {
      lpDev = (LPDEVNAMES)GlobalLock( nppd->hDevNames );
      lpDeviceName = (LPSTR)(lpDev) + (UINT)(lpDev->wDeviceOffset);
      GlobalUnlock( nppd->hDevNames );
      }
   else
      lpDeviceName = "";
   return( lpDeviceName );
}

/* This function supplies a user defined buffer with the name
 * of the current printer.
 */
int FASTCALL GetPrinterName( LPSTR lpBuf, int cbMax )
{
   LPDEVNAMES lpDevNames;
   LPSTR lpPrinterName;
   LPSTR lpPortName;

   lpDevNames = GlobalLock( hDevNames );
   lpPrinterName = (LPSTR)(lpDevNames) + (UINT)(lpDevNames->wDeviceOffset);
   lpPortName = (LPSTR)(lpDevNames) + (UINT)(lpDevNames->wOutputOffset);
   if( lpDevNames->wDefault & DN_DEFAULTPRN )
      wsprintf( lpBuf, GS(IDS_STR938), lpPrinterName );
   else
      wsprintf( lpBuf, GS(IDS_STR939), lpPrinterName, lpPortName );
   GlobalUnlock( hDevNames );
   return( lstrlen( lpBuf ) );
}

/* This function extracts the port name from the DevNames
 * structure.
 */
/*static LPSTR FASTCALL GetPortName( PRINTDLG FAR * nppd )
{
   LPDEVNAMES lpDev;
   LPSTR lpPortName;

   if( nppd->hDevNames )
      {
      lpDev = (LPDEVNAMES)GlobalLock( nppd->hDevNames );
      lpPortName = (LPSTR)(lpDev) + (UINT)(lpDev->wOutputOffset);
      GlobalUnlock( nppd->hDevNames );
      }
   else
      lpPortName = "";
   return( lpPortName );
} */

static LPSTR FASTCALL GetPortName( PAGESETUPDLG FAR * nppd )
{
   LPDEVNAMES lpDev;
   LPSTR lpPortName;

   if( nppd->hDevNames )
      {
      lpDev = (LPDEVNAMES)GlobalLock( nppd->hDevNames );
      lpPortName = (LPSTR)(lpDev) + (UINT)(lpDev->wOutputOffset);
      GlobalUnlock( nppd->hDevNames );
      }
   else
      lpPortName = "";
   return( lpPortName );
}

/* This function creates a DC for the current printer.
 */
/*
HDC FASTCALL CreateCurrentPrinterDC( void )
{
   HDC hdc = NULL;

   if( hDevMode == NULL )
      UpdatePrinterSelection( TRUE );
   if( hDevMode != NULL )
      {
      LPDEVMODE lpDevMode;
      LPSTR lpDriverName;
      LPSTR lpDeviceName;
      LPSTR lpPortName;
      PRINTDLG pd;

      pd.hDevNames = hDevNames;
      lpDriverName = GetDriverName( &pd );
      lpDeviceName = GetDeviceName( &pd );
      lpPortName = GetPortName( &pd );
      lpDevMode = GlobalLock( hDevMode );
      hdc = CreateDC( lpDriverName, lpDeviceName, lpPortName, lpDevMode );
      GlobalUnlock( hDevMode );
      }
   return( hdc );
}
*/
HDC FASTCALL CreateCurrentPrinterDC( void )
{
   HDC hdc = NULL;

   if( hDevMode == NULL )
      UpdatePrinterSelection( TRUE );
   if( hDevMode != NULL )
      {
      LPDEVMODE lpDevMode;
      LPSTR lpDriverName;
      LPSTR lpDeviceName;
      LPSTR lpPortName;
      PAGESETUPDLG pd;

      pd.hDevNames = hDevNames;
      lpDriverName = GetDriverName( &pd );
      lpDeviceName = GetDeviceName( &pd );
      lpPortName = GetPortName( &pd );
      lpDevMode = GlobalLock( hDevMode );
      hdc = CreateDC( lpDriverName, lpDeviceName, lpPortName, lpDevMode );
      GlobalUnlock( hDevMode );
      }
   return( hdc );
}

/* This function sets the page orientation in the current
 * DEVMODE structure.
 */
/*void FASTCALL SetOrientation( void )
{
   if( hDevNames != NULL )
      {
      LPDEVMODE lpDevMode;
      LPSTR lpDriverName;
      LPSTR lpDeviceName;
      LPSTR lpPortName;
      PRINTDLG pd;
   
      pd.hDevNames = hDevNames;
      lpDriverName = GetDriverName( &pd );
      lpDeviceName = GetDeviceName( &pd );
      lpPortName = GetPortName( &pd );
      lpDevMode = GlobalLock( hDevMode );
      if( lpDevMode->dmFields & DM_ORIENTATION )
         {
         HINSTANCE hDriver;
         char pmodule[ 32 ];
   
         wsprintf( pmodule, "%s.drv", lpDriverName );
         if( ( hDriver = LoadLibrary( pmodule ) ) >= (HINSTANCE)32 )
            {
            LPFNDEVMODE lpfnExtDeviceMode;
   
            if( NULL != ( lpfnExtDeviceMode = (LPFNDEVMODE)GetProcAddress( hDriver, "EXTDEVICEMODE" ) ) )
               {
               lpDevMode->dmFields = DM_ORIENTATION;
               lpDevMode->dmOrientation = (nOrientation == PR_LANDSCAPE) ? DMORIENT_LANDSCAPE : DMORIENT_PORTRAIT;
               lpfnExtDeviceMode( 0, hDriver, lpDevMode, lpDeviceName, lpPortName, lpDevMode, NULL, DM_OUT_BUFFER|DM_IN_BUFFER );
               }
            FreeLibrary( hDriver );
            }
         }
      GlobalUnlock( hDevMode );
      }
}
*/
void FASTCALL SetOrientation( void )
{
   if( hDevNames != NULL )
      {
      LPDEVMODE lpDevMode;
      LPSTR lpDriverName;
      LPSTR lpDeviceName;
      LPSTR lpPortName;
      PAGESETUPDLG pd;
   
      pd.hDevNames = hDevNames;
      lpDriverName = GetDriverName( &pd );
      lpDeviceName = GetDeviceName( &pd );
      lpPortName = GetPortName( &pd );
      lpDevMode = GlobalLock( hDevMode );
      if( lpDevMode->dmFields & DM_ORIENTATION )
         {
         HINSTANCE hDriver;
         char pmodule[ 32 ];
   
         wsprintf( pmodule, "%s.drv", lpDriverName );
         if( ( hDriver = LoadLibrary( pmodule ) ) >= (HINSTANCE)32 )
            {
            LPFNDEVMODE lpfnExtDeviceMode;
   
            if( NULL != ( lpfnExtDeviceMode = (LPFNDEVMODE)GetProcAddress( hDriver, "EXTDEVICEMODE" ) ) )
               {
               lpDevMode->dmFields = DM_ORIENTATION;
               lpDevMode->dmOrientation = (nOrientation == PR_LANDSCAPE) ? DMORIENT_LANDSCAPE : DMORIENT_PORTRAIT;
               lpfnExtDeviceMode( 0, hDriver, lpDevMode, lpDeviceName, lpPortName, lpDevMode, NULL, DM_OUT_BUFFER|DM_IN_BUFFER );
               }
            FreeLibrary( hDriver );
            }
         }
      GlobalUnlock( hDevMode );
      }
}
