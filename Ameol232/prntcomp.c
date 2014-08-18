/* PRNTCOMP.C - Print the message being composed
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
#include "print.h"
#include "terminal.h"

static BOOL fDefDlgEx = FALSE;      /* DefDlg recursion flag trap */
static BOOL fPrintSelected;         /* Whether or not to print selection */
static int cCopies;                 /* Number of copies to print */

BOOL EXPORT CALLBACK PrintComposedDlg( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL PrintComposedDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL PrintComposedDlg_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL PrintComposedDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );


/* This command handles the Print command in the terminal window.
 */
void FASTCALL CmdComposedPrint( HWND hwnd )
{
   char sz[ 500 ];
   HWND hwndEdit;
   HWND hwndDlg;
   HPRINT hPr;
   BOOL fOk;

   /* MUST have a printer driver.
    */
   if( !IsPrinterDriver() )
      return;

   /* Show and get the print terminal dialog box.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
   if( !Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_PRINTCOMPOSING), PrintComposedDlg, (LPARAM)(LPSTR)hwndEdit ) )
      return;

   /* Start printing.
    */
   GetWindowText( hwnd, sz, sizeof(sz) );
   fOk = TRUE;
   if( NULL != ( hPr = BeginPrint( hwnd, sz, &lfEditFont ) ) )
      {
      LPSTR lpText;
      UINT cch;

      INITIALISE_PTR(lpText);
      cch = Edit_GetTextLength( hwndEdit );
      if( fNewMemory( &lpText, cch + 1 ) )
         {
         register int c;
         LPSTR lpSrc;
         LPSTR lpDst;
         BOOL fOk;

         /* Get the text to be printed. If the edit control has
          * formatting enabled, remove CRCRLF sequences.
          */
         Edit_GetText( hwndEdit, lpText, cch + 1 );
         if( fPrintSelected )
            {
            DWORD dwSel;
            UINT cbSel;

            /* If printing selection, set lpText to the
             * selection.
             */
            dwSel = Edit_GetSel( hwndEdit );
            cbSel = HIWORD(dwSel) - LOWORD(dwSel);
            if( LOWORD( dwSel ) )
               _fmemcpy( lpText, lpText + LOWORD(dwSel), cbSel );
            lpText[ cbSel ] = '\0';
            }

         /* Remove all CRCRLF sequences.
          */
         lpSrc = lpDst = lpText;
         while( *lpSrc )
            if( *lpSrc == 13 && *(lpSrc + 1) == 13 && *(lpSrc + 2 ) == 10 )
               lpSrc += 3;
            else
               *lpDst++ = *lpSrc++;
         *lpDst = '\0';

         /* Print here.
          */
         fOk = TRUE;
         for( c = 0; fOk && c < cCopies; ++c )
            {
            ResetPageNumber( hPr );
            fOk = BeginPrintPage( hPr );
            if( fOk )
            {
               /*
                !!SM!!
                */
               hwndDlg = GetDlgItem( hwnd, IDD_CONFNAME );
               if(IsWindow(hwndDlg) && fOk)
               {
                  Edit_GetText( hwndDlg, lpTmpBuf, 500 );
                  if (strlen(lpTmpBuf) > 0)
                  {
                     wsprintf( sz, "Forum:    %s", (LPSTR)lpTmpBuf);
                     fOk = PrintLine( hPr, sz );
                  }
               }
               hwndDlg = GetDlgItem( hwnd, IDD_TOPICNAME );
               if(IsWindow(hwndDlg) && fOk)
               {
                  Edit_GetText( hwndDlg, lpTmpBuf, 500 );
                  if (strlen(lpTmpBuf) > 0)
                  {
                     wsprintf( sz, "Topic:         %s", (LPSTR)lpTmpBuf);
                     fOk = PrintLine( hPr, sz );
                  }
               }
               hwndDlg = GetDlgItem( hwnd, IDD_COMMENT );
               if(IsWindow(hwndDlg) && fOk)
               {
                  Edit_GetText( hwndDlg, lpTmpBuf, 500 );
                  if (strlen(lpTmpBuf) > 0)
                  {
                     wsprintf( sz, "Comment To:    #%s", (LPSTR)lpTmpBuf);
                     fOk = PrintLine( hPr, sz );
                  }
               }
               hwndDlg = GetDlgItem( hwnd, IDD_TO );
               if(IsWindow(hwndDlg) && fOk)
               {
                  Edit_GetText( hwndDlg, lpTmpBuf, 500 );
                  if (strlen(lpTmpBuf) > 0)
                  {
                     wsprintf( sz, "To:            %s", (LPSTR)lpTmpBuf);
                     fOk = PrintLine( hPr, sz );
                  }
               }
               hwndDlg = GetDlgItem( hwnd, IDD_CC );
               if(IsWindow(hwndDlg) && fOk)
               {
                  Edit_GetText( hwndDlg, lpTmpBuf, 500 );
                  if (strlen(lpTmpBuf) > 0)
                  {
                     wsprintf( sz, "CC:            %s", (LPSTR)lpTmpBuf);
                     fOk = PrintLine( hPr, sz );
                  }
               }
               hwndDlg = GetDlgItem( hwnd, IDD_BCC );
               if(IsWindow(hwndDlg) && fOk)
               {
                  Edit_GetText( hwndDlg, lpTmpBuf, 500 );
                  if (strlen(lpTmpBuf) > 0)
                  {
                     wsprintf( sz, "BCC:           %s", (LPSTR)lpTmpBuf);
                     fOk = PrintLine( hPr, sz );
                  }
               }
               hwndDlg = GetDlgItem( hwnd, IDD_REPLYTO );
               if(IsWindow(hwndDlg) && fOk)
               {
                  Edit_GetText( hwndDlg, lpTmpBuf, 500 );
                  if (strlen(lpTmpBuf) > 0)
                  {
                     wsprintf( sz, "Reply To:      %s", (LPSTR)lpTmpBuf);
                     fOk = PrintLine( hPr, sz );
                  }
               }
               hwndDlg = GetDlgItem( hwnd, IDD_SUBJECT );
               if(IsWindow(hwndDlg) && fOk)
               {
                  Edit_GetText( hwndDlg, lpTmpBuf, 500 );
                  if (strlen(lpTmpBuf) > 0)
                  {
                     wsprintf( sz, "Subject:       %s", (LPSTR)lpTmpBuf);
                     fOk = PrintLine( hPr, sz );
                  }
               }
               hwndDlg = GetDlgItem( hwnd, IDD_ATTACHMENT );
               if(IsWindow(hwndDlg) && fOk)
               {
                  Edit_GetText( hwndDlg, lpTmpBuf, 500 );
                  if (strlen(lpTmpBuf) > 0)
                  {
                     wsprintf( sz, "Attachment(s): %s", (LPSTR)lpTmpBuf);
                     fOk = PrintLine( hPr, sz );
                  }
               }
               fOk = PrintLine( hPr, "" );
               /*
                !!SM!!
                */

               if( fOk )
                  fOk = PrintPage( hPr, lpText );
            }
            if( fOk )
               EndPrintPage( hPr );
            }
         FreeMemory( &lpText );
         }
      EndPrint( hPr );
      }
}

/* This function handles the Print dialog.
 */
BOOL EXPORT CALLBACK PrintComposedDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = PrintComposedDlg_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the Print Message
 * dialog.
 */
LRESULT FASTCALL PrintComposedDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, PrintComposedDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, PrintComposedDlg_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPRINTCOMPOSING );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL PrintComposedDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndSpin;
   HWND hwndEdit;
   DWORD dwSel;

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
   hwndEdit = (HWND)lParam;
   dwSel = Edit_GetSel( hwndEdit );
   if( LOWORD(dwSel) != HIWORD(dwSel) )
      CheckDlgButton( hwnd, IDD_SELECTION, TRUE );
   else
   {
      CheckDlgButton( hwnd, IDD_ALL, TRUE );
      EnableControl( hwnd, IDD_SELECTION, FALSE );
   }
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL PrintComposedDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
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
         EndDialog( hwnd, TRUE );
         break;

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}
