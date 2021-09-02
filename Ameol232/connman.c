/* CONNMAN.C - Implements the Connections Manager
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
#include "amlib.h"
#include <string.h>
#include "intcomms.h"
#include "amcomms.h"
#include <ctype.h>
#include "blinkman.h"
#include "phone.h"

#define  THIS_FILE   __FILE__

static BOOL fDefDlgEx = FALSE;      /* DefDlg recursion flag trap */

static HFONT hTitleFont = NULL;     /* 28pt dialog font */
static BOOL fAutoDetectAbort;       /* Abort autodetect */

static SERIALCOMM FAR sc;           /* Default connection card for serial port */
static IPCOMM FAR ic;               /* Default connection card for IP port */
static COMMDESCRIPTOR FAR cd;       /* Comm descriptor */

int nRasPage;                       /* Index of RAS page in dialog */
int nLastCommsDialogPage = 0;       /* Last page selected in dialog */

static BOOL fRasFill = FALSE;

BOOL EXPORT CALLBACK IPConnCard( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL IPConnCard_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL IPConnCard_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL IPConnCard_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT CALLBACK AutoDetectDlg( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL AutoDetectDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL AutoDetectDlg_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL AutoDetectDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL FASTCALL NewConnCard( HWND, char * );
BOOL EXPORT CALLBACK NewConnCard_P1( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL NewConnCard_P1_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL NewConnCard_P1_OnNotify( HWND, int, LPNMHDR );

BOOL EXPORT CALLBACK NewConnCard_P6( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL NewConnCard_P6_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL NewConnCard_P6_OnNotify( HWND, int, LPNMHDR );

BOOL EXPORT CALLBACK NewConnCard_P3( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL NewConnCard_P3_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL NewConnCard_P3_OnNotify( HWND, int, LPNMHDR );
void FASTCALL NewConnCard_P3_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK NewConnCard_P4( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL NewConnCard_P4_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL NewConnCard_P4_OnNotify( HWND, int, LPNMHDR );
void FASTCALL NewConnCard_P4_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK NewConnCard_P5( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL NewConnCard_P5_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL NewConnCard_P5_OnNotify( HWND, int, LPNMHDR );
void FASTCALL NewConnCard_P5_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK Comms_Blink( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT CALLBACK Comms_Card( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL Comms_Card_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL Comms_Card_OnCommand( HWND, int, HWND, UINT );
void FASTCALL Comms_Card_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
void FASTCALL Comms_Card_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
LRESULT FASTCALL Comms_Card_OnNotify( HWND, int, LPNMHDR );

void FASTCALL CommonScriptPicker( HWND );
void FASTCALL DisablePhoneFields( HWND );
static void FASTCALL UpdateListBoxStatus( HWND );

/* List of services and their port numbers.
 */
static char FAR * pszServices[] = {
   "NNTP",
   "SMTP",
   "POP3",
   "FTP",
   "Finger",
   "Telnet",
   NULL };

static WORD nPorts[] = {
   IPPORT_NNTP,
   IPPORT_SMTP,
   IPPORT_POP3,
   IPPORT_FTP,
   IPPORT_FINGER,
   IPPORT_TELNET
   };

/* This function displays the Connections Manager dialog.
 */
void FASTCALL CmdConnections( HWND hwnd )
{
   AMPROPSHEETPAGE psp[ 3 ];
   AMPROPSHEETHEADER psh;
   int cPages;

   cPages = 0;
   psp[ 0 ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ 0 ].dwFlags = PSP_USETITLE;
   psp[ 0 ].hInstance = hInst;
   psp[ 0 ].pszTemplate = MAKEINTRESOURCE( IDDLG_COMM_BLINK );
   psp[ 0 ].pszIcon = NULL;
   psp[ 0 ].pfnDlgProc = Comms_Blink;
   psp[ 0 ].pszTitle = "Blinks";
   psp[ 0 ].lParam = 0L;
   ++cPages;

   psp[ 1 ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ 1 ].dwFlags = PSP_USETITLE;
   psp[ 1 ].hInstance = hInst;
   psp[ 1 ].pszTemplate = MAKEINTRESOURCE( IDDLG_COMM_CARD );
   psp[ 1 ].pszIcon = NULL;
   psp[ 1 ].pfnDlgProc = Comms_Card;
   psp[ 1 ].pszTitle = "Connections";
   psp[ 1 ].lParam = 0L;
   ++cPages;

   ASSERT( cPages <= (sizeof( psp ) / sizeof( AMPROPSHEETPAGE )) );
   psh.dwSize = sizeof( AMPROPSHEETHEADER );
   psh.dwFlags = PSH_PROPSHEETPAGE|PSH_HASHELP|PSH_NOAPPLYNOW;
   if( fMultiLineTabs )
      psh.dwFlags |= PSH_MULTILINETABS;
   psh.hwndParent = hwnd;
   psh.hInstance = hInst;
   psh.pszIcon = NULL;
   psh.pszCaption = "Communications";
   psh.nPages = cPages;
   psh.nStartPage = nLastCommsDialogPage;
   psh.pfnCallback = NULL;
   psh.ppsp = (LPCAMPROPSHEETPAGE)&psp;
   Amctl_PropertySheet(&psh );
}

/* This function dispatches messages for the About dialog.
 */
BOOL EXPORT CALLBACK Comms_Card( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, Comms_Card_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, Comms_Card_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_DRAWITEM, Comms_Card_OnDrawItem );
      HANDLE_DLGMSG( hwnd, WM_MEASUREITEM, Comms_Card_OnMeasureItem );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, Comms_Card_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL Comms_Card_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndList;
   LPSTR lpList;

   INITIALISE_PTR(lpList);

   /* Fill the list of available
    * connections.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   if( fNewMemory( &lpList, 8000 ) )
      {
      register int c;

      Amuser_GetPPString( szConnections, NULL, "", lpList, 8000 );
      for( c = 0; lpList[ c ]; c += strlen( &lpList[ c ] ) + 1 )
         ListBox_AddString( hwndList, &lpList[ c ] );
      FreeMemory( &lpList );
      }
   ListBox_SetCurSel( hwndList, 0 );

   /* Select the first Connection Card from
    * the list.
    */
   UpdateListBoxStatus( hwnd );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL Comms_Card_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_LIST:
         if( codeNotify != LBN_DBLCLK )
            break;
         /* Fall thru to edit selected item.
          */

      case IDD_EDIT: {
         HWND hwndList;
         int index;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         if( LB_ERR != ( index = ListBox_GetCurSel( hwndList ) ) )
            {
            COMMDESCRIPTOR cd;

            INITIALISE_PTR( cd.lpsc );
            INITIALISE_PTR( cd.lpic );

            /* Get the selected card name.
             */
            ListBox_GetText( hwndList, index, cd.szTitle );
            EditConnectionCard( GetParent( hwnd ), &cd );
            }
         break;
         }

      case IDD_NEW: {
         char sz[ LEN_CONNCARD+1 ];

         /* Fire up the New Connection Card wizard to create a
          * new connection card.
          */
         if( NewConnCard( GetParent( hwnd ), sz ) )
            {
            HWND hwndList;
            int index;

            /* If the listbox is disabled, then delete the
             * message saying that there are no connection
             * cards available.
             */
            hwndList = GetDlgItem( hwnd, IDD_LIST );
            if( !IsWindowEnabled( hwndList ) )
               ListBox_ResetContent( hwndList );
            index = ListBox_AddString( hwndList, sz );
            UpdateListBoxStatus( hwnd );
            ListBox_SetCurSel( hwndList, index );
            }
         break;
         }

      case IDD_REMOVE: {
         HWND hwndList;
         int index;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         if( LB_ERR != ( index = ListBox_GetCurSel( hwndList ) ) )
            {
            char sz[ LEN_CONNCARD+1 ];

            /* Get the name of the card to be deleted.
             */
            ListBox_GetText( hwndList, index, sz );

            /* Stop if we're deleting a CIX connection card.
             */
            if( strcmp( sz, szCIXConnCard ) == 0 )
               {
               fMessageBox( hwnd, 0, GS(IDS_STR971), MB_OK|MB_ICONINFORMATION );
               break;
               }

            /* Delete the card from the configuration file
             * as well as the list box.
             */
            if( fMessageBox( hwnd, 0, GS(IDS_STR334), MB_YESNO|MB_ICONINFORMATION ) == IDYES )
               {
               char szTitle[LEN_CONNCARD+20+1];

               /* Remove from list of connection cards.
                */
               Amuser_WritePPString( szConnections, sz, NULL );

               /* Remove the actual entry.
                */
               wsprintf( szTitle, "%s Connection Card", sz );
               Amuser_WritePPString( szTitle, NULL, NULL );

               /* Remove from the list box.
                */
               index = ListBox_DeleteString( hwndList, index );
               if( index == ListBox_GetCount( hwndList ) )
                  --index;
               ListBox_SetCurSel( hwndList, 0 );

               /* Update the list box.
                */
               UpdateListBoxStatus( hwnd );
               }
            }
         break;
         }
      }
}

/* This function handles the WM_MEASUREITEM message
 */
void FASTCALL Comms_Card_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   HFONT hFont;
   HBITMAP hbmpFont;
   BITMAP bmp;
   HDC hdc;

   hdc = GetDC( hwnd );
   hFont = SelectFont( hdc, hHelvB8Font );
   GetTextMetrics( hdc, &tm );
   hbmpFont = LoadBitmap( hRscLib, MAKEINTRESOURCE(IDB_MODEMCONNCARD) );
   GetObject( hbmpFont, sizeof( BITMAP ), &bmp );
   DeleteBitmap( hbmpFont );
   SelectFont( hdc, hFont );
   ReleaseDC( hwnd, hdc );
   lpMeasureItem->itemHeight = max( tm.tmHeight + tm.tmExternalLeading + 3, bmp.bmHeight );
}

/* This function handles the WM_DRAWITEM message
 */
void FASTCALL Comms_Card_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   COLORREF tmpT, tmpB;
   HBITMAP hbmpFont;
   char sz[ 100 ];
   COLORREF T, B;
   HFONT hFont;
   HBRUSH hbr;
   SIZE size;
   RECT rc;
   int y;

   /* Get the text we're drawing.
    */
   ListBox_GetText( lpDrawItem->hwndItem, lpDrawItem->itemID, sz );
   rc = lpDrawItem->rcItem;

   /* Set the drawing colours.
    */
   GetOwnerDrawListItemColours( lpDrawItem, &T, &B );

   /* Blank out the line we're drawing.
    */
   hbr = CreateSolidBrush( GetSysColor( COLOR_WINDOW ) );
   FillRect( lpDrawItem->hDC, &rc, hbr );

   /* Draw the standard bitmap for this entry.
    */
   hbmpFont = LoadBitmap( hRscLib, MAKEINTRESOURCE(IDB_BLINKMAN) );
   Amuser_DrawBitmap( lpDrawItem->hDC, rc.left, rc.top, 16, 16, hbmpFont, 0 );
   DeleteBitmap( hbmpFont );

   /* Draw the label.
    */
   tmpT = SetTextColor( lpDrawItem->hDC, T );
   tmpB = SetBkColor( lpDrawItem->hDC, B );
   hFont = SelectFont( lpDrawItem->hDC, hHelvB8Font );
   GetTextExtentPoint( lpDrawItem->hDC, sz, strlen(sz), &size );
   rc.left += 16;
   rc.right = rc.left + size.cx + 6;
   y = rc.top + ( ( rc.bottom - rc.top ) - size.cy ) / 2;
   ExtTextOut( lpDrawItem->hDC, rc.left + 2, y, ETO_OPAQUE, &rc, sz, strlen(sz), NULL );
   SelectFont( lpDrawItem->hDC, hFont );

   /* Draw a focus if needed.
    */
   if( lpDrawItem->itemState & ODS_FOCUS )
      DrawFocusRect( lpDrawItem->hDC, &rc );

   /* Clean up before we go home.
    */
   SetTextColor( lpDrawItem->hDC, tmpT );
   SetBkColor( lpDrawItem->hDC, tmpB );
   DeleteBrush( hbr );
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL Comms_Card_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsCOMM_CARD );
         break;

      case PSN_SETACTIVE:
         nLastCommsDialogPage = 0;
         break;
      }
   return( FALSE );
}

/* This function updates the list box status after items
 * have been added or removed.
 */
static void FASTCALL UpdateListBoxStatus( HWND hwnd )
{
   int count;
   HWND hwndList;

   hwndList = GetDlgItem( hwnd, IDD_LIST );
   if( ( count = ListBox_GetCount( hwndList ) ) > 0 )
      SetFocus( hwndList );
   else
      {
      ListBox_InsertString( hwndList, -1, GS(IDS_STR919) );
      ListBox_SetCurSel( hwndList, -1 );
      }
   EnableControl( hwnd, IDD_LIST, count > 0 );
   EnableControl( hwnd, IDD_EDIT, count > 0 );
   EnableControl( hwnd, IDD_REMOVE, count > 0 );
}

/* This function edits the specified connection card.
 */
BOOL FASTCALL EditConnectionCard( HWND hwnd, LPCOMMDESCRIPTOR lpcd )
{
   if( LoadCommDescriptor( lpcd ) )
      switch( lpcd->wProtocol )
         {
         case PROTOCOL_NETWORK:
            Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_IPCONNECTIONSCARD), IPConnCard, (LPARAM)lpcd );
            FreeMemory( &lpcd->lpic );
            return( TRUE );
         }
   return( FALSE );
}

/* Common code from both connection card editing dialogs
 * for picking a script.
 */
void FASTCALL CommonScriptPicker( HWND hwnd )
{
   static char strFilters[] = "Scripts (*.scr)\0*.scr\0All Files (*.*)\0*.*\0\0";
   char sz[ _MAX_PATH ];
   OPENFILENAME ofn;
   HWND hwndEdit;

   /* Get the current script name.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_SCRIPT );
   Edit_GetText( hwndEdit, sz, _MAX_PATH );
   if( ( strlen( sz) > 1 ) && ( sz[ ( strlen( sz ) - 1 ) ] == '\\' ) && ( sz[ strlen( sz ) - 2 ] != ':' ) )
      sz[ ( strlen( sz ) - 1 ) ] = '\0';

   /* Display the Open File dialog.
    */
   ofn.lStructSize = sizeof(OPENFILENAME);
   ofn.hwndOwner = hwnd;
   ofn.hInstance = NULL;
   ofn.lpstrFilter = strFilters;
   ofn.lpstrCustomFilter = NULL;
   ofn.nMaxCustFilter = 0;
   ofn.nFilterIndex = 0;
   ofn.lpstrFile = sz;
   ofn.nMaxFile = sizeof( sz );
   ofn.lpstrFileTitle = NULL;
   ofn.nMaxFileTitle = 0;
   ofn.lpstrInitialDir = pszScriptDir;
   ofn.lpstrTitle = "Select Startup Script";
   ofn.Flags = OFN_NOCHANGEDIR|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
   ofn.nFileOffset = 0;
   ofn.nFileExtension = 0;
   ofn.lpstrDefExt = "SCR";
   ofn.lCustData = 0;
   ofn.lpfnHook = NULL;
   ofn.lpTemplateName = 0;
   if( GetOpenFileName( &ofn ) )
      {
      Edit_SetText( hwndEdit, sz );
      SetFocus( hwndEdit );
      }
}

/* Handles the Connections dialog.
 */
BOOL EXPORT CALLBACK IPConnCard( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, IPConnCard_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the About dialog.
 */
LRESULT FASTCALL IPConnCard_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, IPConnCard_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, IPConnCard_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsIPCONNECTIONSCARD );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL IPConnCard_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPCOMMDESCRIPTOR lpcd;
   register int c;
   HWND hwndPorts;
   HWND hwndEdit;

   /* Get the existing card, if there is one.
    */
   lpcd = (LPCOMMDESCRIPTOR)lParam;
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* Fill the drop down list with the list of services.
    */
   hwndPorts = GetDlgItem( hwnd, IDD_PORT );
   for( c = 0; pszServices[ c ]; ++c )
      ComboBox_InsertString( hwndPorts, -1, pszServices[ c ] );

   /* Set the current settings.
    */
   if( NULL != lpcd )
      {
      /* Prime the address field.
       */
      hwndEdit = GetDlgItem( hwnd, IDD_ADDRESS );

      /* Set the current settings.
       */
      if( NULL != lpcd->lpic )
         {
         /* Select the port in the comm card.
          */
         for( c = 0; NULL != pszServices[ c ]; ++c )
            if( nPorts[ c ] == lpcd->lpic->wPort )
               {
               ComboBox_SetCurSel( hwndPorts, c );
               break;
               }
         if( CB_ERR == ComboBox_GetCurSel( hwndPorts ) )
            {
            char szPort[ 10 ];

            wsprintf( szPort, "%u", lpcd->lpic->wPort );
            ComboBox_SetText( hwndPorts, szPort );
            }

         /* Enter the address in the comm card.
          */
         Edit_SetText( hwndEdit, lpcd->lpic->szAddress );
         }

      /* Enter the current timeout rate.
       */
      SetDlgItemInt( hwnd, IDD_TIMEOUT, lpcd->nTimeout, FALSE );

      /* Set the current script.
       */
      SetDlgItemText( hwnd, IDD_SCRIPT, lpcd->szScript );
      }

   /* Set input field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_ADDRESS ), LEN_ADDRESS-1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_SCRIPT ), LEN_SCRIPT-1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_TIMEOUT ), 3 );

   /* Create the font we'll use for the database, folder and topic
    * names at the top of each page.
    */
   ASSERT( NULL == hTitleFont );
   hTitleFont = CreateFont( 22, 0, 0, 0, FW_BOLD, FALSE, FALSE,
                           FALSE, ANSI_CHARSET, OUT_CHARACTER_PRECIS,
                           CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
                           VARIABLE_PITCH | FF_SWISS, "Times New Roman" );
   SetWindowFont( GetDlgItem( hwnd, IDD_TITLE ), hTitleFont, FALSE );

   /* Set the Browse picture button.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_PICKER ), hInst, IDB_FILEBUTTON );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL IPConnCard_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_PICKER:
         CommonScriptPicker( hwnd );
         break;

      case IDOK: {
         LPCOMMDESCRIPTOR lpcd;
         HWND hwndPorts;
         HWND hwndEdit;
         char szPort[ 10 ];
         BOOL fFound = FALSE;

         /* Get the descriptor.
          */
         lpcd = (LPCOMMDESCRIPTOR)GetWindowLong( hwnd, DWL_USER );
         if( NULL != lpcd->lpic )
            FreeMemory( &lpcd->lpic );
         if( !fNewMemory( &lpcd->lpic, sizeof(IPCOMM) ) )
            {
            OutOfMemoryError( hwnd, FALSE, FALSE );
            break;
            }

         /* Get the port number for the selected
          * service.
          */
         hwndPorts = GetDlgItem( hwnd, IDD_PORT );
         ComboBox_GetText( hwndPorts, szPort, sizeof(szPort) );
         if( isdigit( *szPort ) )
         {
            lpcd->lpic->wPort = (WORD)atoi(szPort);
            fFound = TRUE;
         }
         else
            {
            register int c;

            /* User typed a name, so locate the name in the
             * list.
             */
            for( c = 0; NULL != pszServices[ c ]; ++c )
               if( strcmp( pszServices[ c ], szPort ) == 0 )
                  {
                  lpcd->lpic->wPort = nPorts[ c ];
                  fFound = TRUE;
                  break;
                  }
            }


         if( !fFound )
            lpcd->lpic->wPort = nPorts[ 5 ];
         /* Get the timeout rate.
          */
         if( !GetDlgInt( hwnd, IDD_TIMEOUT, &lpcd->nTimeout, 0, 999 ) )
            break;

         /* Get the address.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_ADDRESS );
         Edit_GetText( hwndEdit, lpcd->lpic->szAddress, sizeof(lpcd->lpic->szAddress) );

         /* Get the script name.
          */
         Edit_GetText( GetDlgItem( hwnd, IDD_SCRIPT ), lpcd->szScript, sizeof(lpcd->szScript) );
         SaveCommDescriptor( lpcd );
         }

      case IDCANCEL:
         DeleteFont( hTitleFont );
         hTitleFont = NULL;
         EndDialog( hwnd, FALSE );
         break;
      }
}


/* This function creates a new connection card.
 */
BOOL FASTCALL NewConnCard( HWND hwnd, char * pszTitle )
{
   AMPROPSHEETPAGE psp[ 9 ];
   AMPROPSHEETHEADER psh;
   BOOL fWin95;
   int cPage;
   BOOL fOk;

   cPage = 0;
   fWin95 = wWinVer >= 0x035F;
   psp[ cPage ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ cPage ].dwFlags = PSP_USETITLE;
   psp[ cPage ].hInstance = hInst;
   psp[ cPage ].pszTemplate = MAKEINTRESOURCE( IDDLG_NEWCONNCARD1 );
   psp[ cPage ].pszIcon = NULL;
   psp[ cPage ].pfnDlgProc = NewConnCard_P1;
   psp[ cPage ].pszTitle = "Create New Connection Card";
   psp[ cPage ].lParam = 0L;
   ++cPage;

   psp[ cPage ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ cPage ].dwFlags = PSP_USETITLE;
   psp[ cPage ].hInstance = hInst;
   psp[ cPage ].pszTemplate = MAKEINTRESOURCE( IDDLG_NEWCONNCARD3 );
   psp[ cPage ].pszIcon = NULL;
   psp[ cPage ].pfnDlgProc = NewConnCard_P3;
   psp[ cPage ].pszTitle = "Internet Connection Card";
   psp[ cPage ].lParam = 0L;
   ++cPage;

   psp[ cPage ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ cPage ].dwFlags = PSP_USETITLE;
   psp[ cPage ].hInstance = hInst;
   psp[ cPage ].pszTemplate = MAKEINTRESOURCE( IDDLG_NEWCONNCARD4 );
   psp[ cPage ].pszIcon = NULL;
   psp[ cPage ].pfnDlgProc = NewConnCard_P4;
   psp[ cPage ].pszTitle = "Run A Script";
   psp[ cPage ].lParam = 0L;
   ++cPage;

   psp[ cPage ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ cPage ].dwFlags = PSP_USETITLE;
   psp[ cPage ].hInstance = hInst;
   psp[ cPage ].pszTemplate = MAKEINTRESOURCE( IDDLG_NEWCONNCARD5 );
   psp[ cPage ].pszIcon = NULL;
   psp[ cPage ].pfnDlgProc = NewConnCard_P5;
   psp[ cPage ].pszTitle = "Name Your Connection Card";
   psp[ cPage ].lParam = (LPARAM)(LPSTR)pszTitle;
   ++cPage;

   /* Create the font we'll use for the title of the wizard
    * pages.
    */
   hTitleFont = CreateFont( 28, 0, 0, 0, FW_BOLD, FALSE, FALSE,
                           FALSE, ANSI_CHARSET, OUT_CHARACTER_PRECIS,
                           CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
                           VARIABLE_PITCH | FF_SWISS, "Times New Roman" );

   /* Define the property sheet wizard header.
    */
   psh.dwSize = sizeof( AMPROPSHEETHEADER );
   psh.dwFlags = PSH_PROPSHEETPAGE|PSH_WIZARD|PSH_HASHELP;
   psh.hwndParent = hwnd;
   psh.hInstance = hInst;
   psh.pszIcon = NULL;
   psh.pszCaption = "";
   psh.nPages = cPage;
   psh.nStartPage = 0;
   psh.ppsp = (LPCAMPROPSHEETPAGE)&psp;

   /* Initialise the empty connection card.
    */
   cd.lpsc = &sc;
   cd.lpic = &ic;
   cd.szScript[ 0 ] = '\0';
   cd.nTimeout = 60;
   ic.wPort = IPPORT_TELNET;

   /* Start the wizard running.
    */
   fOk = Amctl_PropertySheet( &psh ) != -1;

   /* Clean up before we return.
    */
   DeleteFont( hTitleFont );
   hTitleFont = NULL;
   return( fOk );
}

/* This function dispatches messages for the first page of the
 * New Connection Card wizard.
 */
BOOL EXPORT CALLBACK NewConnCard_P1( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, NewConnCard_P1_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, NewConnCard_P1_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL NewConnCard_P1_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   /* Default to internet.
    */
   CheckDlgButton( hwnd, IDD_INTERNET, TRUE );

   /* Set the picture bitmap.
    */
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_PIX ), hInst, IDB_NEWCONNCARD );

   /* Prepare the input fields.
    */
   SetWindowFont( GetDlgItem( hwnd, IDD_TITLE ), hTitleFont, FALSE );
   return( TRUE );
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL NewConnCard_P1_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsNEWCONNCARD1 );
         break;

      case PSN_SETACTIVE:
         PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT );
         break;

      case PSN_WIZNEXT:
        cd.wProtocol = PROTOCOL_NETWORK;
        return( IDDLG_NEWCONNCARD3 );
      }
   return( FALSE );
}

/* This function dispatches messages for the third page of the
 * New Connection Card wizard.
 */
BOOL EXPORT CALLBACK NewConnCard_P3(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        HANDLE_DLGMSG(hwnd, WM_INITDIALOG, NewConnCard_P3_OnInitDialog);
        HANDLE_DLGMSG(hwnd, WM_NOTIFY, NewConnCard_P3_OnNotify);
        HANDLE_DLGMSG(hwnd, WM_COMMAND, NewConnCard_P3_OnCommand);
    }
    return(FALSE);
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL NewConnCard_P3_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndEdit;

   /* Prime the address field.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_ADDRESS );
   Edit_LimitText( hwndEdit, sizeof(ic.szAddress) - 1 );

   /* Set the picture bitmap.
    */
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_PIX ), hInst, IDB_NEWCONNCARD );

   /* Set the display attributes.
    */
   SetWindowFont( GetDlgItem( hwnd, IDD_TITLE ), hTitleFont, FALSE );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL NewConnCard_P3_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_ADDRESS:
         if( codeNotify == EN_UPDATE )
            if( Edit_GetTextLength( hwndCtl ) )
               PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT|PSWIZB_BACK );
            else
               PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_BACK );
         break;
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL NewConnCard_P3_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsNEWCONNCARD3 );
         break;

      case PSN_SETACTIVE:
         if( Edit_GetTextLength( GetDlgItem( hwnd, IDD_ADDRESS ) ) > 0 )
            PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT|PSWIZB_BACK );
         else
            PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_BACK );
         break;

      case PSN_WIZBACK:
         return( IDDLG_NEWCONNCARD1 );

      case PSN_WIZNEXT: {
         HWND hwndEdit;

         /* Get the address.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_ADDRESS );
         Edit_GetText( hwndEdit, ic.szAddress, sizeof(ic.szAddress) );
         StripLeadingTrailingSpaces( ic.szAddress );
         if( *ic.szAddress == '\0' )
            {
            HighlightField( hwnd, IDD_ADDRESS );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }
         return( IDDLG_NEWCONNCARD4 );
         }
      }
   return( FALSE );
}

/* This function dispatches messages for the fourth page of the
 * New Connection Card wizard.
 */
BOOL EXPORT CALLBACK NewConnCard_P4( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, NewConnCard_P4_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, NewConnCard_P4_OnNotify );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, NewConnCard_P4_OnCommand );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL NewConnCard_P4_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   /* Default to no script.
    */
   CheckDlgButton( hwnd, IDD_NO, TRUE );


   Edit_LimitText( GetDlgItem( hwnd, IDD_SCRIPT ), LEN_SCRIPT );

   /* Set the Browse picture button.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_PICKER ), hInst, IDB_FILEBUTTON );

   /* Set the picture bitmap.
    */
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_PIX ), hInst, IDB_NEWCONNCARD );

   /* Set the display attributes.
    */
   SetWindowFont( GetDlgItem( hwnd, IDD_TITLE ), hTitleFont, FALSE );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL NewConnCard_P4_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_NO:
         PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT|PSWIZB_BACK );
         break;

      case IDD_YES:
         if( Edit_GetTextLength( GetDlgItem( hwnd, IDD_SCRIPT ) ) == 0 )
            PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_BACK );
         else
            PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT|PSWIZB_BACK );
         break;
      case IDD_PICKER:
         CommonScriptPicker( hwnd );
         if( Edit_GetTextLength( GetDlgItem( hwnd, IDD_SCRIPT ) ) == 0 )
            PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_BACK );
         else
            PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT|PSWIZB_BACK );
         break;

      case IDD_SCRIPT:
         if( codeNotify == EN_UPDATE )
         {
         if( Edit_GetTextLength( GetDlgItem( hwnd, IDD_SCRIPT ) ) == 0 )
            PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_BACK );
         else
            PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT|PSWIZB_BACK );
         }
         break;
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL NewConnCard_P4_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsNEWCONNCARD4 );
         break;

      case PSN_SETACTIVE:
         if( IsDlgButtonChecked( hwnd, IDD_YES ) )
            if( Edit_GetTextLength( GetDlgItem( hwnd, IDD_SCRIPT ) ) == 0 )
               {
               PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_BACK );
               break;
               }
         PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT|PSWIZB_BACK );
         break;

      case PSN_WIZBACK:
         if( cd.wProtocol & PROTOCOL_SERIAL )
            return( ( wWinVer >= 0x35F ) ? IDDLG_NEWCONNCARD6_TAPI : IDDLG_NEWCONNCARD6 );
         return( IDDLG_NEWCONNCARD3 );

      case PSN_WIZNEXT:
         if( IsDlgButtonChecked( hwnd, IDD_YES ) )
            {
            /* Get the selected script name.
             */
            Edit_GetText( GetDlgItem( hwnd, IDD_SCRIPT ), cd.szScript, sizeof(cd.szScript) );
            }
         return( IDDLG_NEWCONNCARD5 );
      }
   return( FALSE );
}

/* This function dispatches messages for the last page of the
 * New Connection Card wizard.
 */
BOOL EXPORT CALLBACK NewConnCard_P5( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, NewConnCard_P5_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, NewConnCard_P5_OnNotify );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, NewConnCard_P5_OnCommand );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL NewConnCard_P5_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPCAMPROPSHEETPAGE psp;

   /* Get the pointer to the buffer to which we'll save a copy
    * of the title of this connection card.
    */
   psp = (LPCAMPROPSHEETPAGE)lParam;
   SetWindowLong( hwnd, DWL_USER, psp->lParam );

   /* Set the connection card title limit.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_EDIT ), LEN_CONNCARD );

   /* Set the picture bitmap.
    */
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_PIX ), hInst, IDB_NEWCONNCARD );

   /* Set the display attributes.
    */
   SetWindowFont( GetDlgItem( hwnd, IDD_TITLE ), hTitleFont, FALSE );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL NewConnCard_P5_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EDIT:
         if( codeNotify == EN_CHANGE )
            if( Edit_GetTextLength( hwndCtl ) > 0 )
               PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_FINISH|PSWIZB_BACK );
            else
               PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_BACK );
         break;
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL NewConnCard_P5_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsNEWCONNCARD5 );
         break;

      case PSN_SETACTIVE:
         /* Disable the Finish button unless there's something in the
          * connection card name field.
          */
         if( Edit_GetTextLength( GetDlgItem( hwnd, IDD_EDIT ) ) > 0 )
            PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_FINISH|PSWIZB_BACK );
         else
            PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_BACK );
         break;

      case PSN_WIZBACK:
         return( IDDLG_NEWCONNCARD4 );

      case PSN_WIZFINISH: {
         char * pszTitle;
         HWND hwndEdit;
         char sz[ 6 ];

         /* Get the title and verify that this title has not
          * been used already.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         Edit_GetText( hwndEdit, cd.szTitle, LEN_CONNCARD+1 );
         StripLeadingTrailingSpaces( cd.szTitle );
         if( *cd.szTitle == '\0' )
            {
            HighlightField( hwnd, IDD_EDIT );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }
         if( Amuser_GetPPString( szConnections, cd.szTitle, "", sz, sizeof(sz) ) )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR833), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_EDIT );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }

         /* Save a copy of this connection card title.
          */
         pszTitle = (char *)GetWindowLong( hwnd, DWL_USER );
         strcpy( pszTitle, cd.szTitle );

         /* All done, so create the connection card.
          */
         CreateCommDescriptor( &cd );
         return( 0 );
         }
      }
   return( FALSE );
}
