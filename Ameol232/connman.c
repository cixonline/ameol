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

#ifdef WIN32
HLINEAPP hTAPI;                     /* TAPI handle for TAPI dialogs */
#endif

int nRasPage;                       /* Index of RAS page in dialog */
int nLastCommsDialogPage = 0;       /* Last page selected in dialog */

static BOOL fRasFill = FALSE;

BOOL EXPORT CALLBACK ModemConnCard( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL ModemConnCard_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL ModemConnCard_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL ModemConnCard_DlgProc( HWND, UINT, WPARAM, LPARAM );

#ifdef WIN32
BOOL EXPORT CALLBACK TapiModemConnCard( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL TapiModemConnCard_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL TapiModemConnCard_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL TapiModemConnCard_DlgProc( HWND, UINT, WPARAM, LPARAM );
#endif

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

BOOL EXPORT CALLBACK NewConnCard_P2( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL NewConnCard_P2_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL NewConnCard_P2_OnNotify( HWND, int, LPNMHDR );
void FASTCALL NewConnCard_P2_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK NewConnCard_P2_TAPI( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL NewConnCard_P2_TAPI_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL NewConnCard_P2_TAPI_OnNotify( HWND, int, LPNMHDR );
void FASTCALL NewConnCard_P2_TAPI_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK NewConnCard_P1a( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL NewConnCard_P1a_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL NewConnCard_P1a_OnNotify( HWND, int, LPNMHDR );

BOOL EXPORT CALLBACK NewConnCard_P6( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL NewConnCard_P6_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL NewConnCard_P6_OnNotify( HWND, int, LPNMHDR );

BOOL EXPORT CALLBACK NewConnCard_P6_TAPI( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL NewConnCard_P6_TAPI_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL NewConnCard_P6_TAPI_OnNotify( HWND, int, LPNMHDR );

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

#ifdef WIN32
BOOL EXPORT CALLBACK Comms_RAS( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL Comms_RAS_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL Comms_RAS_OnNotify( HWND, int, LPNMHDR );
void FASTCALL Comms_RAS_OnCommand( HWND, int, HWND, UINT );

DWORD (APIENTRY * fRasEnumEntries)( LPSTR, LPSTR, LPRASENTRYNAMEA, LPDWORD, LPDWORD );
#endif

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
   992
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

#ifdef WIN32
   nRasPage = cPages;
   psp[ nRasPage ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ nRasPage ].dwFlags = PSP_USETITLE;
   psp[ nRasPage ].hInstance = hInst;
   psp[ nRasPage ].pszTemplate = MAKEINTRESOURCE( IDDLG_PREF_RAS );
   psp[ nRasPage ].pszIcon = NULL;
   psp[ nRasPage ].pfnDlgProc = Comms_RAS;
   psp[ nRasPage ].pszTitle = "DUN";
   psp[ nRasPage ].lParam = 0L;
   ++cPages;
#endif

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
         case PROTOCOL_MODEM:
            Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_MODEMCONNECTIONSCARD), ModemConnCard, (LPARAM)lpcd );
            FreeMemory( &lpcd->lpsc );
            return( TRUE );

         case PROTOCOL_NETWORK:
            Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_IPCONNECTIONSCARD), IPConnCard, (LPARAM)lpcd );
            FreeMemory( &lpcd->lpic );
            return( TRUE );

      #ifdef WIN32
         case PROTOCOL_TELEPHONY:
            Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_TAPI_MODEMCONNECTIONSCARD), TapiModemConnCard, (LPARAM)lpcd );
            FreeMemory( &lpcd->lpsc );
            return( TRUE );
      #endif
         }
   return( FALSE );
}

/* Handles the TAPI Modem Connections Card dialog.
 */
#ifdef WIN32
BOOL EXPORT CALLBACK TapiModemConnCard( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, TapiModemConnCard_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the About dialog.
 */
LRESULT FASTCALL TapiModemConnCard_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, TapiModemConnCard_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, TapiModemConnCard_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsTAPIMODEMCONNECTIONSCARD );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL TapiModemConnCard_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPCOMMDESCRIPTOR lpcd;

   /* Get the existing card, if there is one.
    */
   lpcd = (LPCOMMDESCRIPTOR)lParam;
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* Get a TAPI handle.
    */
   if( INVALID_TAPI_HANDLE == ( hTAPI = Amcomm_LoadTAPI( NULL ) ) )
      {
      EndDialog( hwnd, 0 );
      return( FALSE );
      }

   /* Fill list of modems from MODEMS.INI file.
    */
   Amcomm_FillDevices( hTAPI, hwnd, IDD_MODEMS );

   /* Set the current settings.
    */
   if( NULL != lpcd )
      {
      DWORD dwCountryCode;
      HWND hwndModems;
      HWND hwndCombo;
      int index;

      /* Select the current modem from the list of
       * modems.
       */
      hwndModems = GetDlgItem( hwnd, IDD_MODEMS );
      if( CB_ERR != ( index = ComboBox_FindStringExact( hwndModems, -1, lpcd->lpsc->md.szModemName ) ) )
         ComboBox_SetCurSel( hwndModems, index );

      /* Enter the current timeout rate.
       */
      SetDlgItemInt( hwnd, IDD_TIMEOUT, lpcd->nTimeout, FALSE );
      SetDlgItemInt( hwnd, IDD_RETRY, lpcd->lpsc->wRetry, FALSE );

      /* Fill out the edit fields.
       */
      SetDlgItemText( hwnd, IDD_AREACODE, lpcd->lpsc->szAreaCode );
      SetDlgItemText( hwnd, IDD_PHONE, lpcd->lpsc->szPhone );
      SetDlgItemText( hwnd, IDD_SCRIPT, lpcd->szScript );

      /* Fill list of countries.
       */
      VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_COUNTRYLIST ) );
      Amcomm_FillCountries( hwndCombo );
      dwCountryCode = atoi( lpcd->lpsc->szCountryCode );
      if( CB_ERR != ( index = RealComboBox_FindItemData( hwndCombo, -1, dwCountryCode ) ) )
         ComboBox_SetCurSel( hwndCombo, index );

      /* Fill list of locations.
       */
      VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_LOCATIONLIST ) );
      Amcomm_FillLocations( hTAPI, hwndCombo );
      if( CB_ERR != ( index = ComboBox_FindStringExact( hwndCombo, -1, lpcd->lpsc->szLocation ) ) )
         ComboBox_SetCurSel( hwndCombo, index );
      }

   /* Set some limits on the input fields.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_SCRIPT ), LEN_SCRIPT );
   Edit_LimitText( GetDlgItem( hwnd, IDD_AREACODE ), 7 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_PHONE ), LEN_PHONE-1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_TIMEOUT ), 3 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_RETRY ), 2 );

   /* Disable fields which require a non-blank phone number.
    */
   DisablePhoneFields( hwnd );

   /* Set the Browse picture button.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_PICKER ), hInst, IDB_FILEBUTTON );

   /* Create the font we'll use for the database, folder and topic
    * names at the top of each page.
    */
   ASSERT( NULL == hTitleFont );
   hTitleFont = CreateFont( 22, 0, 0, 0, FW_BOLD, FALSE, FALSE,
                           FALSE, ANSI_CHARSET, OUT_CHARACTER_PRECIS,
                           CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
                           VARIABLE_PITCH | FF_SWISS, "Times New Roman" );
   SetWindowFont( GetDlgItem( hwnd, IDD_TITLE ), hTitleFont, FALSE );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL TapiModemConnCard_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_LOCATION: {
         LPCOMMDESCRIPTOR lpcd;
         char szModemName[ 100 ];
         char szNumber[ 256 ];
         char szAreaCode[ 8 ];
         char szCountryCode[ 8 ];
         char szPhone[ LEN_PHONE ];
         DWORD dwCountryCode;
         HWND hwndCombo;
         int index;

         /* Get the selected modem
          */
         VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_MODEMS ) );
         ComboBox_GetText( hwndCombo, szModemName, sizeof(szModemName) );

         /* Get the selected country.
          */
         VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_COUNTRYLIST ) );
         index = ComboBox_GetCurSel( hwndCombo );
         ASSERT( CB_ERR != index );
         dwCountryCode = ComboBox_GetItemData( hwndCombo, index );
         wsprintf( szCountryCode, "%u", dwCountryCode );

         /* Get the number details so far.
          */
         Edit_GetText( GetDlgItem( hwnd, IDD_AREACODE ), szAreaCode, sizeof(szAreaCode) );
         Edit_GetText( GetDlgItem( hwnd, IDD_PHONE ), szPhone, sizeof(szPhone) );

         /* Create a canonical phone number.
          */
         szNumber[ 0 ] = '+';
         szNumber[ 1 ] = '\0';
         strcat( szNumber, szCountryCode );
         strcat( szNumber, "(" );
         strcat( szNumber, szAreaCode );
         strcat( szNumber, ")" );
         strcat( szNumber, szPhone );
         Amcomm_LocationConfigDialog( hTAPI, hwnd, szModemName, szNumber );

         /* Fill list of locations again.
          */
         VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_LOCATIONLIST ) );
         Amcomm_FillLocations( hTAPI, hwndCombo );
         lpcd = (LPCOMMDESCRIPTOR)GetWindowLong( hwnd, DWL_USER );
         if( CB_ERR != ( index = ComboBox_FindStringExact( hwndCombo, -1, lpcd->lpsc->szLocation ) ) )
            ComboBox_SetCurSel( hwndCombo, index );
         break;
         }

      case IDD_CONFMODEM: {
         char szModemName[ 100 ];
         HWND hwndCombo;

         /* Get the selected modem
          */
         VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_MODEMS ) );
         ComboBox_GetText( hwndCombo, szModemName, sizeof(szModemName) );
         Amcomm_LineConfigDialog( hTAPI, hwnd, szModemName );
         break;
         }

      case IDD_PHONE:
         if( codeNotify == EN_CHANGE )
            DisablePhoneFields( hwnd );
         break;

      case IDD_PICKER:
         CommonScriptPicker( hwnd );
         break;

      case IDOK: {
         LPCOMMDESCRIPTOR lpcd;
         DWORD dwCountryCode;
         HWND hwndCombo;
         int index;

         /* Delete the old serial descriptor and create
          * it anew.
          */
         lpcd = (LPCOMMDESCRIPTOR)GetWindowLong( hwnd, DWL_USER );
         if( NULL == lpcd->lpsc )
            if( !fNewMemory( &lpcd->lpsc, sizeof(SERIALCOMM) ) )
               {
               OutOfMemoryError( hwnd, FALSE, FALSE );
               break;
               }

         /* Get the timeout rate.
          */
         if( !GetDlgInt( hwnd, IDD_TIMEOUT, &lpcd->nTimeout, 0, 999 ) )
            break;

         /* Get the retry rate.
          */
         if( !GetDlgInt( hwnd, IDD_RETRY, &lpcd->lpsc->wRetry, 0, 99 ) )
            break;

         /* Read and store the modem.
          */
         hwndCombo = GetDlgItem( hwnd, IDD_MODEMS );
         if( CB_ERR != ( index = ComboBox_GetCurSel( hwndCombo ) ) )
            ComboBox_GetLBText( hwndCombo, index, lpcd->lpsc->md.szModemName );

         /* Read and store the location.
          */
         hwndCombo = GetDlgItem( hwnd, IDD_LOCATIONLIST );
         if( CB_ERR != ( index = ComboBox_GetCurSel( hwndCombo ) ) )
            ComboBox_GetLBText( hwndCombo, index, lpcd->lpsc->szLocation );

         /* Get the selected country.
          */
         VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_COUNTRYLIST ) );
         index = ComboBox_GetCurSel( hwndCombo );
         ASSERT( CB_ERR != index );
         dwCountryCode = ComboBox_GetItemData( hwndCombo, index );
         wsprintf( lpcd->lpsc->szCountryCode, "%u", dwCountryCode );

         /* Read and store the phone numbers.
          */
         Edit_GetText( GetDlgItem( hwnd, IDD_AREACODE ), lpcd->lpsc->szAreaCode, sizeof(lpcd->lpsc->szAreaCode) );
         Edit_GetText( GetDlgItem( hwnd, IDD_PHONE ), lpcd->lpsc->szPhone, sizeof(lpcd->lpsc->szPhone) );

         /* Read and store the script name.
          */
         Edit_GetText( GetDlgItem( hwnd, IDD_SCRIPT ), lpcd->szScript, sizeof(lpcd->szScript) );
         SaveCommDescriptor( lpcd );
         }

      case IDCANCEL:
         DeleteFont( hTitleFont );
         hTitleFont = NULL;
         Amcomm_UnloadTAPI( hTAPI );
         EndDialog( hwnd, FALSE );
         break;
      }
}
#endif

/* Handles the Connections dialog.
 */
BOOL EXPORT CALLBACK ModemConnCard( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, ModemConnCard_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the About dialog.
 */
LRESULT FASTCALL ModemConnCard_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, ModemConnCard_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, ModemConnCard_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsMODEMCONNECTIONSCARD );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL ModemConnCard_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPCOMMDESCRIPTOR lpcd;

   /* Get the existing card, if there is one.
    */
   lpcd = (LPCOMMDESCRIPTOR)lParam;
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* Fill list of modems from MODEMS.INI file.
    */
   FillModemList( hwnd, IDD_MODEMS );

   /* Fill list of ports.
    */
   FillPortsList( hwnd, IDD_PORTS );

   /* Fill list of speeds.
    */
   FillSpeedsList( hwnd, IDD_SPEEDS );

   /* Set the current settings.
    */
   if( NULL != lpcd )
      {
      char szPassword[ 40 ];
      HWND hwndModems;
      HWND hwndBaud;
      HWND hwndPorts;
      char sz[ 10 ];
      int index;

      /* Select the current modem from the list of
       * modems.
       */
      hwndModems = GetDlgItem( hwnd, IDD_MODEMS );
      if( CB_ERR != ( index = ComboBox_FindStringExact( hwndModems, -1, lpcd->lpsc->md.szModemName ) ) )
         ComboBox_SetCurSel( hwndModems, index );

      /* Select the baud rate.
       */
      hwndBaud = GetDlgItem( hwnd, IDD_SPEEDS );
      wsprintf( sz, "%lu", lpcd->lpsc->dwBaud );
      if( CB_ERR != ( index = ComboBox_FindStringExact( hwndBaud, -1, sz ) ) )
         ComboBox_SetCurSel( hwndBaud, index );

      /* Select the port.
       */
      hwndPorts = GetDlgItem( hwnd, IDD_PORTS );
      if( CB_ERR != ( index = ComboBox_FindStringExact( hwndPorts, -1, lpcd->lpsc->szPort ) ) )
         ComboBox_SetCurSel( hwndPorts, index );

      /* Set dial type.
       */
      CheckDlgButton( hwnd, IDD_PULSE, lpcd->lpsc->wDialType == DTYPE_PULSE );
      CheckDlgButton( hwnd, IDD_TONE, lpcd->lpsc->wDialType == DTYPE_TONE );

      /* Set the CTS and DCD checks.
       */
      CheckDlgButton( hwnd, IDD_CHECKCTS, lpcd->lpsc->fCheckCTS );
      CheckDlgButton( hwnd, IDD_CHECKDCD, lpcd->lpsc->fCheckDCD );

      /* Enter the current timeout rate.
       */
      SetDlgItemInt( hwnd, IDD_TIMEOUT, lpcd->nTimeout, FALSE );
      SetDlgItemInt( hwnd, IDD_RETRY, lpcd->lpsc->wRetry, FALSE );

      /* Fill out the edit fields.
       */
      SetDlgItemText( hwnd, IDD_PHONE, lpcd->lpsc->szPhone );
      SetDlgItemText( hwnd, IDD_SCRIPT, lpcd->szScript );

      /* Decrypt password before displaying.
       */
      memcpy( szPassword, lpcd->lpsc->szMercury, 40 );
      Amuser_Decrypt( szPassword, rgEncodeKey );
      SetDlgItemText( hwnd, IDD_MERCURY, szPassword );
      }

   /* Set some limits on the input fields.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_SCRIPT ), LEN_SCRIPT );
   Edit_LimitText( GetDlgItem( hwnd, IDD_PHONE ), LEN_PHONE-1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_MERCURY ), LEN_MERCURY-1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_TIMEOUT ), 3 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_RETRY ), 2 );

   /* Disable fields which require a non-blank phone number.
    */
   DisablePhoneFields( hwnd );

   /* Set the Browse picture button.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_PICKER ), hInst, IDB_FILEBUTTON );

   /* Create the font we'll use for the database, folder and topic
    * names at the top of each page.
    */
   ASSERT( NULL == hTitleFont );
   hTitleFont = CreateFont( 22, 0, 0, 0, FW_BOLD, FALSE, FALSE,
                           FALSE, ANSI_CHARSET, OUT_CHARACTER_PRECIS,
                           CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
                           VARIABLE_PITCH | FF_SWISS, "Times New Roman" );
   SetWindowFont( GetDlgItem( hwnd, IDD_TITLE ), hTitleFont, FALSE );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL ModemConnCard_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_PHONE:
         if( codeNotify == EN_CHANGE )
            DisablePhoneFields( hwnd );
         break;

      case IDD_MODEMEDIT: {
         HWND hwndModems;
         int index;

         /* Get the selected modem name.
          */
         hwndModems = GetDlgItem( hwnd, IDD_MODEMS );
         if( CB_ERR != ( index = ComboBox_GetCurSel( hwndModems ) ) )
            {
            LPMODEMDESCRIPTOR lpmd;

            INITIALISE_PTR(lpmd);

            /* Read the selected modem name and put up the Modem Settings dialog
             * so that it can be edited.
             */
            if( fNewMemory( &lpmd, sizeof(MODEMDESCRIPTOR) ) )
               {
               LPMODEMDESCRIPTOR lpmd2;
               LPCOMMDESCRIPTOR lpcd;

               ComboBox_GetLBText( hwndModems, index, lpmd->szModemName );
               lpcd = (LPCOMMDESCRIPTOR)GetWindowLong( hwnd, DWL_USER );
               if( strcmp( lpmd->szModemName, lpcd->lpsc->md.szModemName ) == 0 )
                  lpmd2 = &lpcd->lpsc->md;
               else
                  {
                  lpmd2 = lpmd;
                  LoadModemDescriptor( lpmd2 );
                  }
               if( Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_MODEMSETTINGS), ModemSettings, (LPARAM)lpmd2 ) )
                  {
                  /* Refill the list box. The user may have added new modems
                   * to the list.
                   */
                  FillModemList( hwnd, IDD_MODEMS );
   
                  /* Highlight the selected modem.
                   */
                  if( CB_ERR != ( index = ComboBox_FindStringExact( hwndModems, -1, lpmd2->szModemName ) ) )
                     ComboBox_SetCurSel( hwndModems, index );
                  }
               FreeMemory( &lpmd );
               }
            }
         break;
         }

      case IDD_AUTODETECT:
         if( fMessageBox( hwnd, 0, GS(IDS_STR855), MB_OKCANCEL|MB_ICONINFORMATION ) == IDOK )
            {
            EnableWindow( GetParent( hwnd ), FALSE );
            AutoDetect( hwnd, IDD_MODEMS, IDD_PORTS, IDD_SPEEDS );
            EnableWindow( GetParent( hwnd ), TRUE );
            }
         break;

      case IDD_PICKER:
         CommonScriptPicker( hwnd );
         break;

      case IDOK: {
         LPCOMMDESCRIPTOR lpcd;
         char szPassword[ 40 ];
         HWND hwndModems;
         HWND hwndSpeeds;
         HWND hwndPorts;
         int index;

         /* Delete the old serial descriptor and create
          * it anew.
          */
         lpcd = (LPCOMMDESCRIPTOR)GetWindowLong( hwnd, DWL_USER );
         if( NULL == lpcd->lpsc )
            if( !fNewMemory( &lpcd->lpsc, sizeof(SERIALCOMM) ) )
               {
               OutOfMemoryError( hwnd, FALSE, FALSE );
               break;
               }

         /* Get the timeout rate.
          */
         if( !GetDlgInt( hwnd, IDD_TIMEOUT, &lpcd->nTimeout, 0, 999 ) )
            break;

         /* Get the retry rate.
          */
         if( !GetDlgInt( hwnd, IDD_RETRY, &lpcd->lpsc->wRetry, 0, 99 ) )
            break;

         /* Read and store the modem.
          */
         hwndModems = GetDlgItem( hwnd, IDD_MODEMS );
         if( CB_ERR != ( index = ComboBox_GetCurSel( hwndModems ) ) )
            ComboBox_GetLBText( hwndModems, index, lpcd->lpsc->md.szModemName );

         /* Read and store the port name.
          */
         hwndPorts = GetDlgItem( hwnd, IDD_PORTS );
         if( CB_ERR != ( index = ComboBox_GetCurSel( hwndPorts ) ) )
            ComboBox_GetLBText( hwndPorts, index, lpcd->lpsc->szPort );

         /* Read and store the baud rate.
          */
         hwndSpeeds = GetDlgItem( hwnd, IDD_SPEEDS );
         if( CB_ERR != ( index = ComboBox_GetCurSel( hwndSpeeds ) ) )
            {
            char sz[ 10 ];

            ComboBox_GetLBText( hwndSpeeds, index, sz );
            lpcd->lpsc->dwBaud = atol( sz );
            }

         /* Read and store the phone numbers.
          */
         Edit_GetText( GetDlgItem( hwnd, IDD_PHONE ), lpcd->lpsc->szPhone, sizeof(lpcd->lpsc->szPhone) );

         /* Encrypt Mercury PIN.
          */
         Edit_GetText( GetDlgItem( hwnd, IDD_MERCURY ), szPassword, 40 );
         Amuser_Encrypt( szPassword, rgEncodeKey );
         memcpy( lpcd->lpsc->szMercury, szPassword, 40 );

         /* Get the CTS and DCD check states.
          */
         lpcd->lpsc->fCheckCTS = IsDlgButtonChecked( hwnd, IDD_CHECKCTS );
         lpcd->lpsc->fCheckDCD = IsDlgButtonChecked( hwnd, IDD_CHECKDCD );

         /* Get the dial type.
          */
         if( IsDlgButtonChecked( hwnd, IDD_TONE ) )
            lpcd->lpsc->wDialType = DTYPE_TONE;
         else
            lpcd->lpsc->wDialType = DTYPE_PULSE;

         /* Read and store the script name.
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

/* Disable the Retry, Mercury and Script fields if the phone
 * number field is blank.
 */
void FASTCALL DisablePhoneFields( HWND hwnd )
{
   BOOL fBlank;

   fBlank = Edit_GetTextLength( GetDlgItem( hwnd, IDD_PHONE ) ) == 0;
   EnableControl( hwnd, IDD_PAD1, !fBlank );
   EnableControl( hwnd, IDD_PICKER, !fBlank );
   EnableControl( hwnd, IDD_SCRIPT, !fBlank );
   EnableControl( hwnd, IDD_PAD2, !fBlank );
   EnableControl( hwnd, IDD_PAD3, !fBlank );
   EnableControl( hwnd, IDD_RETRY, !fBlank );
   EnableControl( hwnd, IDD_PAD4, !fBlank );
   EnableControl( hwnd, IDD_MERCURY, !fBlank );
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

/* This is the autodetect function. Before calling it, warn the user to check that
 * each modem is plugged in and switched on.
 */
BOOL FASTCALL AutoDetect( HWND hwnd, int idModems, int idPorts, int idSpeeds )
{
   LPCOMMDESCRIPTOR lpcd;
   BOOL fFoundModem;
   HWND hwndPorts;
   HNDFILE fhLog;
   register int i;
   HWND hDlg;
   int count;

   /* Create a temporary serial descriptor.
    */
   INITIALISE_PTR(lpcd);
   if( !fNewMemory( &lpcd, sizeof(COMMDESCRIPTOR) ) )
      return( FALSE );
   memset( lpcd, 0, sizeof(COMMDESCRIPTOR) );
   if( !fNewMemory( &lpcd->lpsc, sizeof(SERIALCOMM) ) )
      {
      FreeMemory( &lpcd );
      return( FALSE );
      }
   memset( lpcd->lpsc, 0, sizeof(SERIALCOMM) );
   lpcd->wProtocol = PROTOCOL_MODEM;

   /* Open the status dialog.
    */
   hDlg = Adm_CreateDialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_AUTODETECT), AutoDetectDlg, 0L );
   EnableWindow( hwnd, FALSE );
   UpdateWindow( hwnd );

   /* Create the DETECT.LOG file.
    */
   fhLog = Amfile_Create( "DETECT.LOG", 0 );

   /* Loop for each port in the ports drop down
    * listbox.
    */
   fFoundModem = FALSE;
   hwndPorts = GetDlgItem( hwnd, idPorts );
   count = ComboBox_GetCount( hwndPorts );
   fAutoDetectAbort = FALSE;
   for( i = 0; !fAutoDetectAbort && !fFoundModem && i < count; ++i )
      {
      HWND hwndSpeeds;
      register int x;
      int count2;

      /* Initialise the descriptors.
       */
      ComboBox_GetLBText( hwndPorts, i, lpcd->lpsc->szPort );

      /* Update the status.
       */
      SetDlgItemText( hDlg, IDD_PORT, lpcd->lpsc->szPort );
      SetDlgItemText( hDlg, IDD_STATUS, GS(IDS_STR921) );

      /* Loop for each possible baud rate until we find
       * one that returns OK for the reset.
       */
      hwndSpeeds = GetDlgItem( hwnd, idSpeeds );
      count2 = ComboBox_GetCount( hwndSpeeds );
      for( x = count2 - 1; !fAutoDetectAbort && !fFoundModem && x >= 0; --x )
         {
         LPCOMMDEVICE lpcdev;
         char szBaud[ 10 ];

         /* Get the baud rates, starting with the fastest.
          */
         ComboBox_GetLBText( hwndSpeeds, x, szBaud );
         lpcd->lpsc->dwBaud = atol( szBaud );

         /* Skip 115,200 baud test.
          */
         if( 115200 == lpcd->lpsc->dwBaud )
            continue;

         /* Ensure signal checking is on.
          */
         lpcd->lpsc->fCheckCTS = TRUE;
         lpcd->lpsc->fCheckDCD = TRUE;

         /* Try to open this port. If we couldn't, skip the other
          * baud rates.
          */
         if( !Amcomm_OpenCard( hwnd, &lpcdev, lpcd, NULL, 0L, NULL, NULL, FALSE, FALSE ) )
            x = 0;
         else
            {
            /* Port opened okay. Check CTS and skip port if it is not
             * active.
             */
            if( !Amcomm_IsCTSActive( lpcdev ) )
               x = 0;
            else
               {
               char szResponse[ 100 ];
               int cBytesRead;

               /* Write to the log.
                */
               wsprintf( lpTmpBuf, GS(IDS_STR1000), lpcd->lpsc->szPort, szBaud );
               Amfile_Write( fhLog, lpTmpBuf, strlen(lpTmpBuf) );

               /* Wait for 1 second before writing any data, to allow slow modems
                * time to wake up after DTR. Then hit them with a reset again!
                */
               SetDlgItemText( hDlg, IDD_STATUS, GS(IDS_STR922) );
               if( !Amcomm_Delay( lpcdev, 1000 ) )
                  break;
               Amcomm_WriteString( lpcdev, "ATZ\r\n", 5 );

               /* Write to the log.
                */
               wsprintf( lpTmpBuf, GS(IDS_STR1001) );
               Amfile_Write( fhLog, lpTmpBuf, strlen(lpTmpBuf) );

               /* Wait for up to 5 seconds to read some data back. If we
                * don't see anything after 5 seconds, we try a lower baud
                * rate.
                */
               cBytesRead = Amcomm_ReadStringTimed( lpcdev, szResponse, sizeof(szResponse), 5000 );
               if( cBytesRead > 2 && strstr( szResponse, "OK" ) != NULL )
                  {
                  static char szATI4[] = "ATI4\r\n";
                  HWND hwndModems;
                  char szModem[ 60 ];

                  /* Write to the log.
                   */
                  wsprintf( lpTmpBuf, GS(IDS_STR1002), szResponse );
                  Amfile_Write( fhLog, lpTmpBuf, strlen(lpTmpBuf) );

                  /* Bingo! Got a response on this port and at this baud. Now
                   * send ATI4.
                   */
                  SetDlgItemText( hDlg, IDD_STATUS, GS(IDS_STR923) );
                  Amcomm_WriteString( lpcdev, szATI4, strlen( szATI4 ) );

                  /* Write to the log.
                   */
                  wsprintf( lpTmpBuf, GS(IDS_STR1003) );
                  Amfile_Write( fhLog, lpTmpBuf, strlen(lpTmpBuf) );

                  /* Wait for response
                   */
                  cBytesRead = Amcomm_ReadStringTimed( lpcdev, szResponse, sizeof(szResponse) - 1, 2000 );

                  /* Locate what we got back from the MODEMS.INI file. We need to
                   * loop for every modem listed and check the Response= string.
                   */
                  strcpy( szModem, GS(IDS_STR924) );
                  hwndModems = GetDlgItem( hwnd, idModems );
                  if( cBytesRead > (int)strlen( szATI4 ) )
                     {
                     char szPrivIniFile[ _MAX_PATH ];
                     char * pResponse;
                     register int m;
                     int count3;

                     /* Find the first non-blank after the response.
                      */
                     szResponse[ cBytesRead ] = '\0';
                     pResponse = szResponse + strlen( szATI4 );
                     while( *pResponse && isspace( *pResponse ) )
                        ++pResponse;

                     /* Write to the log.
                      */
                     wsprintf( lpTmpBuf, GS(IDS_STR1002), pResponse );
                     Amfile_Write( fhLog, lpTmpBuf, strlen(lpTmpBuf) );

                     /* Get the list of modems.
                      */
                     count3 = ComboBox_GetCount( hwndModems );
                     strcat( strcpy( szPrivIniFile, pszAmeolDir ), szModemsIni );
                     for( m = 0; m < count3; ++m )
                        {
                        char szExpected[ 100 ];
                        char szModemName[ 60 ];
                        int cbExpected;

                        /* Read each modem name and consult MODEMS.INI
                         */
                        ComboBox_GetLBText( hwndModems, m, szModemName );
                        cbExpected = GetPrivateProfileString( szModemName, "Response", "", szExpected, sizeof(szExpected), szPrivIniFile );

                        /* Does the expected string and the response match?
                         */
                        if( cbExpected > 0 && cbExpected <= (int)strlen(pResponse) )
                           if( memcmp( szExpected, pResponse, cbExpected ) == 0 )
                              {
                              /* Write to the log.
                               */
                              wsprintf( lpTmpBuf, GS(IDS_STR1004), szModemName );
                              Amfile_Write( fhLog, lpTmpBuf, strlen(lpTmpBuf) );

                              /* Save modem name.
                               */
                              strcpy( szModem, szModemName );
                              break;
                              }
                        }
                     }

                  /* Tell the user what we found and ask whether or not to accept this
                   * setting.
                   */
                  wsprintf( lpTmpBuf, GS(IDS_STR856), szModem, lpcd->lpsc->szPort );
                  if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONINFORMATION ) == IDYES )
                     {
                     ComboBox_SetCurSel( hwndModems, ComboBox_FindStringExact( hwndModems, -1, szModem ) );
                     ComboBox_SetCurSel( hwndPorts, i );
                     ComboBox_SetCurSel( hwndSpeeds, x );
                     fFoundModem = TRUE;
                     i = count;
                     }
                  x = 0;
                  }
               }
            Amcomm_Close( lpcdev );
            Amcomm_Destroy( lpcdev );
            }
         }
      }

   /* Close the dialog.
    */
   if( NULL != hDlg )
      {
      EnableWindow( hwnd, TRUE );
      DestroyWindow( hDlg );
      }

   /* Close the log
    */
   if( HNDFILE_ERROR != fhLog )
      Amfile_Close( fhLog );

   /* Deallocate our structures.
    */
   FreeMemory( &lpcd->lpsc );
   FreeMemory( &lpcd );

   /* Did we find a modem?
    */
   if( !fFoundModem )
      fMessageBox( hwnd, 0, GS(IDS_STR857), MB_OK|MB_ICONINFORMATION );
   return( fFoundModem );
}

/* This function handles the dialog box messages passed to the ExportDlgStatus
 * dialog.
 */
BOOL EXPORT CALLBACK AutoDetectDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = AutoDetectDlg_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the ExportDlgStatus
 * dialog.
 */
LRESULT FASTCALL AutoDetectDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, AutoDetectDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, AutoDetectDlg_OnCommand );

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL AutoDetectDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDCANCEL:
         fAutoDetectAbort = TRUE;
         break;
      }
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL AutoDetectDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   return( TRUE );
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

#ifdef WIN32
   psp[ cPage ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ cPage ].dwFlags = PSP_USETITLE;
   psp[ cPage ].hInstance = hInst;
   psp[ cPage ].pszTemplate = MAKEINTRESOURCE( IDDLG_NEWCONNCARD1a );
   psp[ cPage ].pszIcon = NULL;
   psp[ cPage ].pfnDlgProc = NewConnCard_P1a;
   psp[ cPage ].pszTitle = "Connection Card Type";
   psp[ cPage ].lParam = 0L;
   ++cPage;
#endif

#ifdef WIN32
   psp[ cPage ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ cPage ].dwFlags = PSP_USETITLE;
   psp[ cPage ].hInstance = hInst;
   psp[ cPage ].pszTemplate = MAKEINTRESOURCE( IDDLG_NEWCONNCARD6_TAPI );
   psp[ cPage ].pszIcon = NULL;
   psp[ cPage ].pfnDlgProc = NewConnCard_P6_TAPI;
   psp[ cPage ].pszTitle = "Enter phone number";
   psp[ cPage ].lParam = 0L;
   ++cPage;
#endif

   psp[ cPage ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ cPage ].dwFlags = PSP_USETITLE;
   psp[ cPage ].hInstance = hInst;
   psp[ cPage ].pszTemplate = MAKEINTRESOURCE( IDDLG_NEWCONNCARD6 );
   psp[ cPage ].pszIcon = NULL;
   psp[ cPage ].pfnDlgProc = NewConnCard_P6;
   psp[ cPage ].pszTitle = "Enter phone number";
   psp[ cPage ].lParam = 0L;
   ++cPage;

#ifdef WIN32
   psp[ cPage ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ cPage ].dwFlags = PSP_USETITLE;
   psp[ cPage ].hInstance = hInst;
   psp[ cPage ].pszTemplate = MAKEINTRESOURCE( IDDLG_NEWCONNCARD2_TAPI );
   psp[ cPage ].pszIcon = NULL;
   psp[ cPage ].pfnDlgProc = NewConnCard_P2_TAPI;
   psp[ cPage ].pszTitle = "Telephony Connection Card";
   psp[ cPage ].lParam = 0L;
   ++cPage;
#endif

   psp[ cPage ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ cPage ].dwFlags = PSP_USETITLE;
   psp[ cPage ].hInstance = hInst;
   psp[ cPage ].pszTemplate = MAKEINTRESOURCE( IDDLG_NEWCONNCARD2 );
   psp[ cPage ].pszIcon = NULL;
   psp[ cPage ].pfnDlgProc = NewConnCard_P2;
   psp[ cPage ].pszTitle = "Modem Connection Card";
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

   /* Initialise TAPI
    */
#ifdef WIN32
   hTAPI = Amcomm_LoadTAPI( NULL );
#endif

   /* Start the wizard running.
    */
   fOk = Amctl_PropertySheet( &psh ) != -1;

   /* Close down TAPI
    */
#ifdef WIN32
   Amcomm_UnloadTAPI( hTAPI );
#endif

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
   /* Default to a modem.
    */
   CheckDlgButton( hwnd, IDD_MODEM, TRUE );

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
         if( IsDlgButtonChecked( hwnd, IDD_MODEM ) )
         {
            if( wWinVer > 0x35F )
            return( IDDLG_NEWCONNCARD1a );
            else
            return( IDDLG_NEWCONNCARD2 );
         }
         if( IsDlgButtonChecked( hwnd, IDD_INTERNET ) )
            {
            cd.wProtocol = PROTOCOL_NETWORK;
            return( IDDLG_NEWCONNCARD3 );
            }
         
         ASSERT( FALSE );
         return( 0 );
      }
   return( FALSE );
}


BOOL EXPORT CALLBACK NewConnCard_P1a( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, NewConnCard_P1a_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, NewConnCard_P1a_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL NewConnCard_P1a_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   /* Default to TAPI.
    */
   CheckDlgButton( hwnd, IDD_TAPICARD, TRUE );

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
LRESULT FASTCALL NewConnCard_P1a_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsNEWCONNCARD1a );
         break;

      case PSN_SETACTIVE:
         PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT|PSWIZB_BACK );
         break;

      case PSN_WIZNEXT:
         {
         if( IsDlgButtonChecked( hwnd, IDD_TAPICARD ) )
            {
            cd.wProtocol = PROTOCOL_TELEPHONY;
            return( IDDLG_NEWCONNCARD2_TAPI );
            }
         if( IsDlgButtonChecked( hwnd, IDD_AMCOMCARD ) )
            {
            cd.wProtocol = PROTOCOL_MODEM;
            return( IDDLG_NEWCONNCARD2 );
            }
         ASSERT( FALSE );
         return( 0 );
         }
      case PSN_WIZBACK:
         return( IDDLG_NEWCONNCARD1 );

      }
   return( FALSE );
}

/* This function dispatches messages for the second page of the
 * New Connection Card wizard.
 */
BOOL EXPORT CALLBACK NewConnCard_P2( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, NewConnCard_P2_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, NewConnCard_P2_OnNotify );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, NewConnCard_P2_OnCommand );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL NewConnCard_P2_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndList;
   int index;

   /* Fill list of modems from MODEMS.INI file.
    */
   FillModemList( hwnd, IDD_MODEMS );

   /* Fill list of ports.
    */
   FillPortsList( hwnd, IDD_PORTS );

   /* Fill list of speeds.
    */
   FillSpeedsList( hwnd, IDD_SPEEDS );

   /* Select the generic modem.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_MODEMS ) );
   if( CB_ERR != ( index = ComboBox_FindString( hwndList, -1, GS(IDS_STR924) ) ) )
      ComboBox_SetCurSel( hwndList, index );

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
void FASTCALL NewConnCard_P2_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_AUTODETECT:
         if( fMessageBox( hwnd, 0, GS(IDS_STR855), MB_OKCANCEL|MB_ICONINFORMATION ) == IDOK )
            {
            EnableWindow( GetParent( hwnd ), FALSE );
            AutoDetect( hwnd, IDD_MODEMS, IDD_PORTS, IDD_SPEEDS );
            EnableWindow( GetParent( hwnd ), TRUE );
            }
         break;
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL NewConnCard_P2_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsNEWCONNCARD2 );
         break;

      case PSN_SETACTIVE:
         if( CB_ERR != ComboBox_GetCurSel( GetDlgItem( hwnd, IDD_MODEMS ) ) )
            PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT|PSWIZB_BACK );
         else
            PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_BACK );
         break;

      case PSN_WIZBACK:
            if( wWinVer > 0x35F )
            return( IDDLG_NEWCONNCARD1a );
            else
            return( IDDLG_NEWCONNCARD1 );

      case PSN_WIZNEXT: {
         HWND hwndCombo;
         char sz[ 14 ];

         /* Fill out the serial connection card descriptor.
          */
         hwndCombo = GetDlgItem( hwnd, IDD_PORTS );
         ComboBox_GetText( hwndCombo, sc.szPort, sizeof(sc.szPort ) );

         /* Set the speed.
          */
         hwndCombo = GetDlgItem( hwnd, IDD_SPEEDS );
         ComboBox_GetText( hwndCombo, sz, sizeof(sz) );
         sc.dwBaud = atol( sz );

         /* Set the modem type.
          */
         hwndCombo = GetDlgItem( hwnd, IDD_MODEMS );
         ComboBox_GetText( hwndCombo, sc.md.szModemName, sizeof(sc.md.szModemName) );
         LoadModemDescriptor( &sc.md );

         /* Set the remaining to defaults.
          */
         sc.wRetry = 5;
         sc.wDialType = DTYPE_TONE;
         sc.fCheckDCD = TRUE;
         sc.fCheckCTS = TRUE;
         return( IDDLG_NEWCONNCARD6 );
         }
      }
   return( FALSE );
}

/* This function dispatches messages for the second page of the
 * New Connection Card wizard.
 */
#ifdef WIN32
BOOL EXPORT CALLBACK NewConnCard_P2_TAPI( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, NewConnCard_P2_TAPI_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, NewConnCard_P2_TAPI_OnNotify );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, NewConnCard_P2_TAPI_OnCommand );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL NewConnCard_P2_TAPI_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   /* Fill list of modems from MODEMS.INI file.
    */
   Amcomm_FillDevices( hTAPI, hwnd, IDD_MODEMS );

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
void FASTCALL NewConnCard_P2_TAPI_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_LIST:
         if( codeNotify == CBN_SELCHANGE )
            PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT|PSWIZB_BACK );
         break;
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL NewConnCard_P2_TAPI_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsNEWCONNCARD2_TAPI );
         break;

      case PSN_SETACTIVE:
         if( CB_ERR != ComboBox_GetCurSel( GetDlgItem( hwnd, IDD_MODEMS ) ) )
            PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT|PSWIZB_BACK );
         else
            PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_BACK );
         break;

      case PSN_WIZBACK:
         return( IDDLG_NEWCONNCARD1a );

      case PSN_WIZNEXT: {
         HWND hwndCombo;

         /* Set the modem type.
          */
         VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_MODEMS ) );
         ComboBox_GetText( hwndCombo, sc.md.szModemName, sizeof(sc.md.szModemName) );
         LoadModemDescriptor( &sc.md );
         return( IDDLG_NEWCONNCARD6_TAPI );
         }
      }
   return( FALSE );
}
#endif

/* This function dispatches messages for the third page of the
 * New Connection Card wizard.
 * YH 05/06/96 17:10
 */
BOOL EXPORT CALLBACK NewConnCard_P6( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, NewConnCard_P6_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, NewConnCard_P6_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL NewConnCard_P6_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndEdit;

   /* Set the picture bitmap.
    */
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_PIX ), hInst, IDB_NEWCONNCARD );

   /* Set the display attributes.
    */
   SetWindowFont( GetDlgItem( hwnd, IDD_TITLE ), hTitleFont, FALSE );

   /* Set default phone number
    */
   hwndEdit = GetDlgItem( hwnd, IDC_PHONE );
   Edit_SetText( hwndEdit, szCIXPhoneNumber );

   /* If Windows 95, fill out other TAPI fields.
    */
#ifdef WIN32
   if( wWinVer >= 0x35F )
      {
      SetDlgItemText( hwnd, IDD_COUNTRYCODE, szCountryCode );
      SetDlgItemText( hwnd, IDD_AREACODE, szAreaCode );
      SetDlgItemText( hwnd, IDC_PHONE, szPhoneNumber );
      }
#endif
   return( TRUE );
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL NewConnCard_P6_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsNEWCONNCARD6 );
         break;

      case PSN_SETACTIVE:
         PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT|PSWIZB_BACK );
         break;

      case PSN_WIZBACK:
         if( cd.wProtocol == PROTOCOL_TELEPHONY )
            return( IDDLG_NEWCONNCARD2_TAPI );
         return( IDDLG_NEWCONNCARD2 );

      case PSN_WIZNEXT: {
         char szPassword[ 40 ];
         HWND hwndEdit;

         /* Get phone number.
          */
         hwndEdit = GetDlgItem( hwnd, IDC_PHONE );
         Edit_GetText( hwndEdit, sc.szPhone, LEN_PHONE );
         
         /* Encrypt Mercury PIN.
          */
         Edit_GetText( GetDlgItem( hwnd, IDC_MERCURY_PIN ), szPassword, 40 );
         Amuser_Encrypt( szPassword, rgEncodeKey );
         memcpy( sc.szMercury, szPassword, 40 );

      #ifdef WIN32
         if( wWinVer >= 0x35F )
            {
            /* Get TAPI information.
             */
            hwndEdit = GetDlgItem( hwnd, IDD_AREACODE );
            Edit_GetText( hwndEdit, sc.szAreaCode, sizeof(sc.szAreaCode) );
            hwndEdit = GetDlgItem( hwnd, IDD_COUNTRYCODE );
            Edit_GetText( hwndEdit, sc.szCountryCode, sizeof(sc.szCountryCode) );
            }
      #endif

         /* Set the remaining to defaults.
          */
         sc.wRetry = 5;
         sc.wDialType = DTYPE_TONE;
         return( IDDLG_NEWCONNCARD4 );
         }
      }
   return( FALSE );
}

/* This function dispatches messages for the third page of the
 * New Connection Card wizard.
 * YH 05/06/96 17:10
 */
#ifdef WIN32
BOOL EXPORT CALLBACK NewConnCard_P6_TAPI( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, NewConnCard_P6_TAPI_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, NewConnCard_P6_TAPI_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL NewConnCard_P6_TAPI_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   DWORD dwCountryCode;
   HWND hwndCombo;
   int index;

   /* Set the picture bitmap.
    */
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_PIX ), hInst, IDB_NEWCONNCARD );

   /* Set the display attributes.
    */
   SetWindowFont( GetDlgItem( hwnd, IDD_TITLE ), hTitleFont, FALSE );

   /* Set default phone number
    */
   SetDlgItemText( hwnd, IDD_AREACODE, szAreaCode );
   SetDlgItemText( hwnd, IDC_PHONE, szPhoneNumber );

   /* Fill list of countries.
    */
   VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_COUNTRYLIST ) );
   Amcomm_FillCountries( hwndCombo );
   dwCountryCode = (DWORD)atoi( szCountryCode );
   if( CB_ERR != ( index = RealComboBox_FindItemData( hwndCombo, -1, dwCountryCode ) ) )
      ComboBox_SetCurSel( hwndCombo, index );
   return( TRUE );
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL NewConnCard_P6_TAPI_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsNEWCONNCARD6_TAPI );
         break;

      case PSN_SETACTIVE:
         PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT|PSWIZB_BACK );
         break;

      case PSN_WIZBACK:
         return( IDDLG_NEWCONNCARD2_TAPI );

      case PSN_WIZNEXT: {
         DWORD dwCountryCode;
         HWND hwndCombo;
         HWND hwndEdit;
         int index;

         /* Get phone number.
          */
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDC_PHONE ) );
         Edit_GetText( hwndEdit, sc.szPhone, LEN_PHONE );
         
         /* Get area code.
          */
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_AREACODE ) );
         Edit_GetText( hwndEdit, sc.szAreaCode, sizeof(sc.szAreaCode) );

         /* Get the selected country.
          */
         VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_COUNTRYLIST ) );
         index = ComboBox_GetCurSel( hwndCombo );
         ASSERT( CB_ERR != index );
         dwCountryCode = ComboBox_GetItemData( hwndCombo, index );
         wsprintf( sc.szCountryCode, "%u", dwCountryCode );

         /* Set the remaining to defaults.
          */
         sc.wRetry = 5;
         sc.wDialType = DTYPE_TONE;
         return( IDDLG_NEWCONNCARD4 );
         }
      }
   return( FALSE );
}
#endif

/* This function dispatches messages for the third page of the
 * New Connection Card wizard.
 */
BOOL EXPORT CALLBACK NewConnCard_P3( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, NewConnCard_P3_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, NewConnCard_P3_OnNotify );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, NewConnCard_P3_OnCommand );
      }
   return( FALSE );
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

/* This function dispatches messages for the Dial-Up Networking page of the
 * Preferences dialog.
 */
#ifdef WIN32
BOOL EXPORT CALLBACK Comms_RAS( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, Comms_RAS_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, Comms_RAS_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, Comms_RAS_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL Comms_RAS_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndList;
   HWND hwndEdit;

   /* Set Use Ras checkbox.
    */
   CheckDlgButton( hwnd, IDD_USERAS, rdDef.fUseRAS );

   /* Set flag indicating that the combo box has not been
    * filled with the RAS connections.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   ComboBox_AddString( hwndList, rdDef.szRASEntryName );
   ComboBox_SetCurSel( hwndList, 0 );
   fRasFill = FALSE;

   /* Show current user name and password.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_USERNAME );
   Edit_SetText( hwndEdit, rdDef.szRASUserName );
   Edit_LimitText( hwndEdit, UNLEN );

   /* Show current password.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_PASSWORD );
   Amuser_Decrypt( rdDef.szRASPassword, rgEncodeKey );
   Edit_LimitText( hwndEdit, PWLEN );
   Edit_SetText( hwndEdit, rdDef.szRASPassword );
   Amuser_Encrypt( rdDef.szRASPassword, rgEncodeKey );

   /* Disable list box and label if RAS disabled.
    */
   EnableControl( hwnd, IDD_PAD1, rdDef.fUseRAS );
   EnableControl( hwnd, IDD_PAD2, rdDef.fUseRAS );
   EnableControl( hwnd, IDD_PAD3, rdDef.fUseRAS );
   EnableControl( hwnd, IDD_LIST, rdDef.fUseRAS );
   EnableControl( hwnd, IDD_USERNAME, rdDef.fUseRAS );
   EnableControl( hwnd, IDD_PASSWORD, rdDef.fUseRAS );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL Comms_RAS_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_USERNAME:
      case IDD_PASSWORD:
         if( codeNotify == EN_CHANGE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_LIST:
         if( codeNotify == CBN_SELCHANGE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         else if( codeNotify == CBN_DROPDOWN && !fRasFill )
            {
            FillRasConnections( hwnd, IDD_LIST );
            fRasFill = TRUE;
            }
         break;

      case IDD_USERAS:{
         BOOL fChecked;

         fChecked = IsDlgButtonChecked( hwnd, IDD_USERAS );
         EnableControl( hwnd, IDD_PAD1, fChecked );
         EnableControl( hwnd, IDD_PAD2, fChecked );
         EnableControl( hwnd, IDD_PAD3, fChecked );
         EnableControl( hwnd, IDD_LIST, fChecked );
         EnableControl( hwnd, IDD_USERNAME, fChecked );
         EnableControl( hwnd, IDD_PASSWORD, fChecked );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
         }
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL Comms_RAS_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPREF_RAS );
         break;

      case PSN_SETACTIVE:
         nLastOptionsDialogPage = nRasPage;
         break;

      case PSN_APPLY: {
         /* Enable or disable RAS?
          */
         rdDef.fUseRAS = IsDlgButtonChecked( hwnd, IDD_USERAS );

         /* If we use RAS, we'll need the entry name.
          */
         if( rdDef.fUseRAS )
            {
            HWND hwndList;

            /* Get the entry name.
             */
            hwndList = GetDlgItem( hwnd, IDD_LIST );
            ComboBox_GetText( hwndList, rdDef.szRASEntryName, RAS_MaxEntryName+1 );

            /* Get username and password.
             */
            Edit_GetText( GetDlgItem( hwnd, IDD_USERNAME ), rdDef.szRASUserName, UNLEN+1 );
            Edit_GetText( GetDlgItem( hwnd, IDD_PASSWORD ), rdDef.szRASPassword, PWLEN+1 );
            if( ( strlen( rdDef.szRASEntryName ) == 0 ) || ( strlen( rdDef.szRASUserName ) == 0 ) || ( strlen( rdDef.szRASPassword ) == 0 ) )
            {
               fMessageBox( hwnd, 0, GS(IDS_STR1242), MB_OK|MB_ICONEXCLAMATION );
               return( PSNRET_INVALID_NOCHANGEPAGE );
            }
            Amuser_Encrypt( rdDef.szRASPassword, rgEncodeKey );
            }

         /* Save the new settings.
          */
         Amuser_WritePPInt( szCIXIP, "Use RAS", rdDef.fUseRAS );
         Amuser_WritePPString( szCIXIP, "RAS Entry Name", rdDef.szRASEntryName );
         Amuser_WritePPString( szCIXIP, "RAS User Name", rdDef.szRASUserName );
         Amuser_WritePPPassword( szCIXIP, "RAS Password", rdDef.szRASPassword );

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This function fills the specified listbox with a list of
 * default RAS phonebook entries.
 */
BOOL FASTCALL FillRasConnections( HWND hwnd, int idCtl )
{
   HINSTANCE hRasLib;
   HWND hwndList;

   /* Open the RAS DLL and return FALSE if it couldn't
    * be found.
    */
   if( NULL == ( hRasLib = LoadLibrary( szRasLib ) ) )
      return( FALSE );

   /* Locate the RasEnumEntries procedure in the RAS DLL.
    */
   hwndList = GetDlgItem( hwnd, idCtl );
   ComboBox_ResetContent( hwndList );
   (FARPROC)fRasEnumEntries = GetProcAddress( hRasLib, "RasEnumEntriesA" );
   if( NULL != fRasEnumEntries )
      {
      RASENTRYNAME FAR * lpren;
      RASENTRYNAME ren;
      DWORD cEntries;
      DWORD ret;
      DWORD cb;

      /* Start with space for just one entry. That
       * might be all that is needed!
       */
      lpren = &ren;
      lpren->dwSize = sizeof(RASENTRYNAME);
      cb = sizeof(RASENTRYNAME);
      if( 0 != ( ret = fRasEnumEntries( NULL, NULL, lpren, &cb, &cEntries ) ) )
         if( ERROR_BUFFER_TOO_SMALL == ret )
            {
            INITIALISE_PTR(lpren);
            if( fNewMemory( &lpren, cb ) )
               {
               lpren->dwSize = sizeof(RASENTRYNAME);
               ret = fRasEnumEntries( NULL, NULL, lpren, &cb, &cEntries );
               }
            }

      /* Have we got our buffer? If so,
       * fill the list box with the list of
       * entries.
       */
      if( 0 == ret )
         {
         register DWORD c;
         int index;

         for( c = 0; c < cEntries; ++c )
            ComboBox_AddString( hwndList, lpren[ c ].szEntryName );
         if( lpren != &ren )
            FreeMemory( &lpren );

         /* Locate and select the current setting. If not
          * found, look for one with CIX and Internet in it.
          */
         if( CB_ERR == ( index = ComboBox_FindStringExact( hwndList, -1, rdDef.szRASEntryName ) ) )
            if( CB_ERR == ( index = ComboBox_FindString( hwndList, -1, "cix" ) ) )
               index = 0;
         ComboBox_SetCurSel( hwndList, index );
         }
      }
   FreeLibrary( hRasLib );
   return( TRUE );
}
#endif

