/* PRNTTERM.C - Print from the terminal window
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
#include "print.h"
#include "terminal.h"

static BOOL fDefDlgEx = FALSE;      /* DefDlg recursion flag trap */
static BOOL fPrintSelected;         /* Whether or not to print selection */
static BOOL fPrintWindow;           /* Whether we print the current window */
static int cCopies;                 /* Number of copies to print */

BOOL EXPORT CALLBACK PrintTerminalDlg( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL PrintTerminalDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL PrintTerminalDlg_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL PrintTerminalDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );

/* This command handles the Print command in the terminal window.
 */
void FASTCALL CmdTerminalPrint( HWND hwnd )
{
   TERMINALWND FAR * ptwnd;
   register int c;
   char sz[ 60 ];
   HPRINT hPr;
   BOOL fOk;

   /* MUST have a printer driver.
    */
   if( !IsPrinterDriver() )
      return;

   /* Show and get the print terminal dialog box.
    */
   ptwnd = GetTerminalWnd( hwnd );
   if( !Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_PRINTTERMINAL), PrintTerminalDlg, (LPARAM)ptwnd ) )
      return;

   /* If terminal window gorn, stop now.
    */
   if( !IsWindow( hwnd ) )
      return;

   /* Start printing.
    */
   GetWindowText( hwnd, sz, sizeof(sz) );
   fOk = TRUE;
   if( NULL != ( hPr = BeginPrint( hwnd, sz, &lfTermFont ) ) )
      {
      if( fPrintSelected && ptwnd->fSelection )
         {
         LPSTR lpText;
         LPSTR lpMem;
         LPSTR lpMem2;
         WORD wCopySize;
         HGLOBAL hg;
         register int y1;
         register int y2;
         register int x1;
         register int x2;
            
         /* Get the coordinates */
         x1 = ptwnd->nSBQX1Anchor;
         x2 = ptwnd->nSBQX2Anchor;
         y1 = ptwnd->nSBQY1Anchor;
         y2 = ptwnd->nSBQY2Anchor;
            
         /* Swap, if necessary */
         if( y2 < y1 || ( y2 == y1 && x2 < x1 ) ) {
            register int n;
               
            n = y1;
            y1 = y2;
            y2 = n;
            n = x1;
            x1 = x2;
            x2 = n;
            }
            
         /* Compute the size of the block of memory needed */
         wCopySize = ( ( y2 - y1 ) + 1 ) * ( ptwnd->nSBQXSize + 2 );
         if( ( hg = GlobalAlloc( GHND, (DWORD)wCopySize ) ) == NULL )
            return;
         if( ( lpMem = GlobalLock( hg ) ) == NULL )
            {
            GlobalFree( hg );
            return;
            }
         lpMem2 = lpMem;
         lpText = ptwnd->lpScrollBuf + ( y1 * ptwnd->tp.nLineWidth );
         for( ; y1 <= y2; ++y1 )
            {
            BOOL fLastLine;
            register int n;
            
            fLastLine = y1 == y2;
            x2 = fLastLine ? x2 : ptwnd->nSBQXSize - 1;
            for( n = x2; n >= x1; --n )
               if( lpText[ n ] != ' ' )
                  break;
            if( n >= x1 ) {
               _fmemcpy( lpMem, lpText + x1, ( n - x1 ) + 1 );
               lpMem += ( n - x1 ) + 1;
               }
            lpText += ptwnd->tp.nLineWidth;
            if( !fLastLine ) {
               *lpMem++ = CR;
               *lpMem++ = LF;
               }
            x1 = 0;
            }
         *lpMem = '\0';
         fOk = TRUE;
         for( c = 0; fOk && c < cCopies; ++c )
            {
            ResetPageNumber( hPr );
            fOk = BeginPrintPage( hPr );
            if( fOk )
               fOk = PrintPage( hPr, lpMem2 );
            if( fOk )
               EndPrintPage( hPr );
            }
         GlobalUnlock( hg );
         GlobalFree( hg );
         }
      else
         {
         fOk = TRUE;
         for( c = 0; fOk && c < cCopies; ++c )
            {
            register int n;
            int nLineStart;
            int nLineEnd;

            ResetPageNumber( hPr );
            fOk = BeginPrintPage( hPr );
            if( fPrintWindow )
               {
               nLineStart = ptwnd->nSBQYShift;
               nLineEnd = ptwnd->nSBQYShift + ptwnd->nSBQYSize;
               }
            else
               {
               nLineStart = 0;
               nLineEnd = ptwnd->nSBQYCaret + ptwnd->nSBQYExtent;
               }
            for( n = nLineStart; fOk && n <= nLineEnd; ++n )
               {
               LPSTR lpText;
               register int d;

               lpText = &ptwnd->lpScrollBuf[ n * ptwnd->tp.nLineWidth ];
               _fmemcpy( lpTmpBuf, lpText, ptwnd->tp.nLineWidth );
               for( d = ptwnd->tp.nLineWidth - 1; d; --d )
                  if( lpTmpBuf[ d ] != ' ' )
                     break;
               lpTmpBuf[ d + 1 ] = '\0';
               fOk = PrintLine( hPr, lpTmpBuf );
               }
            if( fOk )
               EndPrintPage( hPr );
            }
         }
      EndPrint( hPr );
      }
}

/* This function handles the Print dialog.
 */
BOOL EXPORT CALLBACK PrintTerminalDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = PrintTerminalDlg_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the Print Message
 * dialog.
 */
LRESULT FASTCALL PrintTerminalDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, PrintTerminalDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, PrintTerminalDlg_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPRINTTERMINAL );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL PrintTerminalDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   TERMINALWND FAR * ptwnd;
   HWND hwndSpin;

   /* Set the input field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_COPIES ), 2 );

   /* Set number of copies.
    */
   SetDlgItemInt( hwnd, IDD_COPIES, 1, FALSE );
   hwndSpin = GetDlgItem( hwnd, IDD_SPIN );
   SendMessage( hwndSpin, UDM_SETBUDDY, (WPARAM)GetDlgItem( hwnd, IDD_COPIES ), 0L );
   SendMessage( hwndSpin, UDM_SETRANGE, 0, MAKELPARAM( 99, 1 ) );
   SendMessage( hwndSpin, UDM_SETPOS, 0, MAKELPARAM( 1, 0 ) );

   /* Display current printer name.
    */
   UpdatePrinterName( hwnd );

   /* Default to All if no selection, current selection
    * otherwise.
    */
   ptwnd = (TERMINALWND FAR *)lParam;
   if( ptwnd->fSelection )
      CheckDlgButton( hwnd, IDD_SELECTION, TRUE );
   else
      CheckDlgButton( hwnd, IDD_ALL, TRUE );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL PrintTerminalDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_SETUP:
         PrintSetup( hwnd );
         UpdatePrinterName( hwnd );
         break;

      case IDOK:
         /* Get number of copies.
          */
         if( !GetDlgInt( hwnd, IDD_COPIES, &cCopies, 1, 99 ) )
            break;

         /* Get the options.
          */
         fPrintSelected = IsDlgButtonChecked( hwnd, IDD_SELECTION );
         fPrintWindow = IsDlgButtonChecked( hwnd, IDD_WINDOW );
         EndDialog( hwnd, TRUE );
         break;

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}
