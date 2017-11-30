/* NEWS.C - Common Usenet stuff
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
#include <string.h>
#include "amlib.h"
#include "cookie.h"
#include "news.h"
#include "rules.h"
#include <ctype.h>
#include "nfl.h"
#include "ini.h"

#define  THIS_FILE   __FILE__

#define  NWO_GENERAL       0
#define  NWO_HEADERS       1
#define  NWO_SERVERS       2

static BOOL fDefDlgEx = FALSE;

typedef struct tagNEWSSERVER {
   struct tagNEWSSERVER FAR * lpsrvrNext;    /* Link to next server */
   struct tagNEWSSERVER FAR * lpsrvrPrev;    /* Link to previous server */
   NEWSSERVERINFO nsi;                    /* News server information */
} NEWSSERVER;

typedef NEWSSERVER FAR * LPNEWSSERVER;

CUSTOMHEADERS uNewsHeaders[ MAX_HEADERS ];   /* Custom usenet headers */
int cNewsHeaders;                            /* Size of uNewsHeaders array */

int nNewsDialogPage = 0;               /* Last accessed Usenet Settings dialog page */
LPNEWSSERVER lpsrvrFirst = NULL;       /* First news server */
LPNEWSSERVER lpsrvrDef = NULL;         /* Default news server */
BOOL fServerListLoaded = FALSE;        /* TRUE if list of news servers loaded */

static BOOL fUsenetHeadersLoaded = FALSE;    /* Usenet custom headers not yet loaded */

UINT CALLBACK NewsPrefsCallbackFunc( HWND, UINT, LPARAM );
BOOL EXPORT CALLBACK UsenetServers( HWND, UINT, WPARAM, LPARAM );
void FASTCALL UsenetServers_OnCommand( HWND, int, HWND, UINT );
BOOL FASTCALL UsenetServers_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL UsenetServers_OnNotify( HWND, int, LPNMHDR );

BOOL EXPORT CALLBACK CixnewsSettings( HWND, UINT, WPARAM, LPARAM );
void FASTCALL CixnewsSettings_OnCommand( HWND, int, HWND, UINT );
BOOL FASTCALL CixnewsSettings_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL CixnewsSettings_OnNotify( HWND, int, LPNMHDR );

BOOL EXPORT CALLBACK UsenetHeaders( HWND, UINT, WPARAM, LPARAM );
void FASTCALL UsenetHeaders_OnCommand( HWND, int, HWND, UINT );
BOOL FASTCALL UsenetHeaders_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL UsenetHeaders_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
void FASTCALL UsenetHeaders_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
LRESULT FASTCALL UsenetHeaders_OnNotify( HWND, int, LPNMHDR );

BOOL EXPORT CALLBACK UsenetGeneral( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL UsenetGeneral_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL UsenetGeneral_OnNotify( HWND, int, LPNMHDR );
void FASTCALL UsenetGeneral_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK NewsPropertiesCix( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL NewsPropertiesCix_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL NewsPropertiesCix_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL NewsPropertiesCix_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT CALLBACK NewsPropertiesIP( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL NewsPropertiesIP_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL NewsPropertiesIP_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL NewsPropertiesIP_DlgProc( HWND, UINT, WPARAM, LPARAM );

void FASTCALL LoadAllUsenetServers( void );
void FASTCALL SaveUsenetServer( NEWSSERVERINFO * );
void FASTCALL CreateNewsServerShortName( char *, char * );
LPNEWSSERVER FASTCALL InstallUsenetServer( NEWSSERVERINFO * );

LPSTR FASTCALL ExtractString( LPSTR, LPSTR, int );

/* This function displays the Usenet Settings dialog.
 */
BOOL FASTCALL CmdUsenetSettings( HWND hwnd )
{
   AMPROPSHEETPAGE psp[ 3 ];
   AMPROPSHEETHEADER psh;
   int cPages;

   cPages = 0;
   psp[ NWO_GENERAL ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ NWO_GENERAL ].dwFlags = PSP_USETITLE;
   psp[ NWO_GENERAL ].hInstance = hInst;
   psp[ NWO_GENERAL ].pszTemplate = MAKEINTRESOURCE( IDDLG_NEWS_GENERAL );
   psp[ NWO_GENERAL ].pszIcon = NULL;
   psp[ NWO_GENERAL ].pfnDlgProc = UsenetGeneral;
   psp[ NWO_GENERAL ].pszTitle = "General";
   psp[ NWO_GENERAL ].lParam = 0L;
   ++cPages;

   psp[ NWO_HEADERS ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ NWO_HEADERS ].dwFlags = PSP_USETITLE;
   psp[ NWO_HEADERS ].hInstance = hInst;
   psp[ NWO_HEADERS ].pszTemplate = MAKEINTRESOURCE( IDDLG_NEWS_HEADERS );
   psp[ NWO_HEADERS ].pszIcon = NULL;
   psp[ NWO_HEADERS ].pfnDlgProc = UsenetHeaders;
   psp[ NWO_HEADERS ].pszTitle = "Headers";
   psp[ NWO_HEADERS ].lParam = 0L;
   ++cPages;

   psp[ NWO_SERVERS ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ NWO_SERVERS ].dwFlags = PSP_USETITLE;
   psp[ NWO_SERVERS ].hInstance = hInst;
   psp[ NWO_SERVERS ].pszTemplate = MAKEINTRESOURCE( IDDLG_NEWS_SERVERS );
   psp[ NWO_SERVERS ].pszIcon = NULL;
   psp[ NWO_SERVERS ].pfnDlgProc = UsenetServers;
   psp[ NWO_SERVERS ].pszTitle = "Servers";
   psp[ NWO_SERVERS ].lParam = 0L;
   ++cPages;

   ASSERT( cPages <= (sizeof( psp ) / sizeof( AMPROPSHEETPAGE )) );
   psh.dwSize = sizeof( AMPROPSHEETHEADER );
   psh.dwFlags = PSH_PROPSHEETPAGE|PSH_HASHELP|PSH_NOAPPLYNOW;
   if( fMultiLineTabs )
      psh.dwFlags |= PSH_MULTILINETABS;
   psh.hwndParent = hwnd;
   psh.hInstance = hInst;
   psh.pszIcon = NULL;
   psh.pszCaption = "Usenet Settings";
   psh.nPages = cPages;
   psh.nStartPage = nNewsDialogPage;
   psh.pfnCallback = NewsPrefsCallbackFunc;
   psh.ppsp = (LPCAMPROPSHEETPAGE)&psp;
   return( Amctl_PropertySheet(&psh ) != -1 );
}

UINT CALLBACK EXPORT NewsPrefsCallbackFunc( HWND hwnd, UINT uMsg, LPARAM lParam )
{
   if( PSCB_INITIALIZED == uMsg )
      Amuser_CallRegistered( AE_PREFSDIALOG, (LPARAM)(LPSTR)hwnd, 0L );
   PropSheet_SetCurSel( hwnd, 0L, nNewsDialogPage );
   return( 0 );
}

/* This function handles the dialog box messages passed to the UsenetSettings
 * dialog.
 */
BOOL EXPORT CALLBACK UsenetServers( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, UsenetServers_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, UsenetServers_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, UsenetServers_OnNotify );
      HANDLE_DLGMSG( hwnd, WM_DRAWITEM, UsenetServers_OnDrawItem );
      HANDLE_DLGMSG( hwnd, WM_MEASUREITEM, UsenetServers_OnMeasureItem );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL UsenetServers_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPNEWSSERVER lpsrvr;
   HWND hwndList;
   int index;

   /* Ensure list of Usenet servers is installed.
    */
   LoadAllUsenetServers();

   /* Read the list of Usenet servers
    * installed.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   for( lpsrvr = lpsrvrFirst; lpsrvr; lpsrvr = lpsrvr->lpsrvrNext )
      {
      index = ListBox_AddString( hwndList, lpsrvr->nsi.szServerName );
      ListBox_SetItemData( hwndList, index, (LPARAM)lpsrvr );
      }

   /* Select the default server.
    */
   if( NULL == lpsrvrFirst )
      {
      /* Nothing in the list, so disable the Remove and Properties
       * buttons.
       */
      EnableControl( hwnd, IDD_REMOVE, FALSE );
      EnableControl( hwnd, IDD_PROPERTIES, FALSE );
      EnableControl( hwnd, IDD_DEFAULT, FALSE );
      }
   else
      {
      index = RealListBox_FindItemData( hwndList, -1, (LPARAM)lpsrvrDef );
      if( LB_ERR == index )
         index = 0;
      ListBox_SetCurSel( hwndList, index );
      }

   /* Get the selected news server
    */
   index = ListBox_GetCurSel( hwndList );
   if (index >= 0)
   {
      lpsrvr = (LPNEWSSERVER)ListBox_GetItemData( hwndList, index );
      ASSERT( NULL != lpsrvr );

      /* Can't vape cixnews.
       */
      if( _strcmpi( lpsrvr->nsi.szServerName, szCixnews ) == 0 )
         EnableControl( hwnd, IDD_REMOVE, FALSE );
      else
         EnableControl( hwnd, IDD_REMOVE, TRUE );
   }
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL UsenetServers_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_ADD: {
         NEWSSERVERINFO nsi;
         LPARAM dwServer;
         HWND hwndList;

         /* Get the new news server details.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         dwServer = CmdAddNewsServer( GetParent(hwnd), &nsi );
         if( 0L != dwServer )
            {
            int index;

            /* Add it to the combobox.
             */
            index = ListBox_AddString( hwndList, nsi.szServerName );
            ListBox_SetItemData( hwndList, index, dwServer );
            ListBox_SetCurSel( hwndList, index );
            EnableControl( hwnd, IDD_REMOVE, TRUE );
            EnableControl( hwnd, IDD_PROPERTIES, TRUE );
            EnableControl( hwnd, IDD_DEFAULT, TRUE );
            }
         SetFocus( hwndList );
         break;
         }

      case IDD_DEFAULT: {
         LPNEWSSERVER lpsrvr;
         HWND hwndList;
         int index2;
         int index;
         RECT rc;

         /* Get the selected news server
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         index = ListBox_GetCurSel( hwndList );
         ASSERT( LB_ERR != index );
         lpsrvr = (LPNEWSSERVER)ListBox_GetItemData( hwndList, index );
         ASSERT( NULL != lpsrvr );

         /* Remove default tag.
          */
         index2 = RealListBox_FindItemData( hwndList, -1, (LPARAM)lpsrvrDef );
         ASSERT( LB_ERR != index2 );
         ListBox_GetItemRect( hwndList, index2, &rc );
         lpsrvrDef = NULL;
         InvalidateRect( hwndList, &rc, TRUE );
         UpdateWindow( hwndList );

         /* Make it the default.
          */
         Amuser_WritePPString( szNews, "Default Server", lpsrvr->nsi.szServerName );
         lpsrvrDef = lpsrvr;

         /* Set new default tag.
          */
         ListBox_GetItemRect( hwndList, index, &rc );
         InvalidateRect( hwndList, &rc, TRUE );
         UpdateWindow( hwndList );
         break;
         }

      case IDD_REMOVE: {
         LPNEWSSERVER lpsrvr;
         HWND hwndList;
         int index;

         /* Get the selected news server
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         index = ListBox_GetCurSel( hwndList );
         ASSERT( LB_ERR != index );
         lpsrvr = (LPNEWSSERVER)ListBox_GetItemData( hwndList, index );
         ASSERT( NULL != lpsrvr );

         /* Can't vape cixnews.
          */
         if( _strcmpi( lpsrvr->nsi.szServerName, szCixnews ) == 0 )
            break;

         /* Ask if they wish to delete it.
          */
         wsprintf( lpTmpBuf, GS(IDS_STR1067), lpsrvr->nsi.szServerName );
         if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONINFORMATION ) == IDYES )
            {
            int count;

            /* Vape the entry for this server from the config
             * file.
             */
            Amuser_WritePPString( szNewsServers, lpsrvr->nsi.szServerName, NULL );

            /* Did we just vape the default? If so, assign the next
             * server to the default.
             */
            if( lpsrvr == lpsrvrDef )
               {
               int newindex;

               newindex = index;
               if( ++newindex == ListBox_GetCount( hwndList ) )
                  newindex -= 2;
               if (newindex > 0)
                  {
                  lpsrvrDef = (LPNEWSSERVER)ListBox_GetItemData( hwndList, newindex );
                  ASSERT( NULL != lpsrvr );
                  Amuser_WritePPString( szNews, "Default Server", lpsrvrDef->nsi.szServerName );
                  }
               }

            /* Unlink it from the internal list.
             */
            if( NULL == lpsrvr->lpsrvrPrev )
               lpsrvrFirst = lpsrvr->lpsrvrNext;
            else
               lpsrvr->lpsrvrPrev->lpsrvrNext = lpsrvr->lpsrvrNext;
            if( NULL != lpsrvr->lpsrvrNext )
               lpsrvr->lpsrvrNext->lpsrvrPrev = lpsrvr->lpsrvrPrev;
            FreeMemory( &lpsrvr );

            /* Vape it from the listbox.
             */
            count = ListBox_DeleteString( hwndList, index );
            if( count == index )
               --index;
            if( index != -1 )
               ListBox_SetCurSel( hwndList, index );
            else
               {
               EnableControl( hwnd, IDD_REMOVE, FALSE );
               EnableControl( hwnd, IDD_PROPERTIES, FALSE );
               EnableControl( hwnd, IDD_DEFAULT, FALSE );
               }
            }
         SetFocus( hwndList );
         break;
         }

      case IDD_LIST:
         if( codeNotify == LBN_SELCHANGE )
            {
            LPNEWSSERVER lpsrvr;
            HWND hwndList;
            int index;

            /* Get the selected news server
             */
            VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
            index = ListBox_GetCurSel( hwndList );
            ASSERT( LB_ERR != index );
            lpsrvr = (LPNEWSSERVER)ListBox_GetItemData( hwndList, index );
            ASSERT( NULL != lpsrvr );

            /* Can't vape cixnews.
             */
            if( _strcmpi( lpsrvr->nsi.szServerName, szCixnews ) == 0 )
               EnableControl( hwnd, IDD_REMOVE, FALSE );
            else
               EnableControl( hwnd, IDD_REMOVE, TRUE );
            break;
            }
         if( codeNotify != LBN_DBLCLK )
            break;

      case IDD_PROPERTIES: {
         char szTmpServerName[ 64 ];
         LPNEWSSERVER lpsrvr;
         HWND hwndList;
         int index;

         /* Get the selected news server
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         index = ListBox_GetCurSel( hwndList );
         ASSERT( LB_ERR != index );
         lpsrvr = (LPNEWSSERVER)ListBox_GetItemData( hwndList, index );
         ASSERT( NULL != lpsrvr );

         /* If name is 'cixnews', display the CIX Usenet properties
          * dialog.
          */
         strcpy( szTmpServerName, lpsrvr->nsi.szServerName );
         if( strcmp( lpsrvr->nsi.szServerName, szCixnews ) == 0 )
            Adm_Dialog( hInst, GetParent(hwnd), MAKEINTRESOURCE(IDDLG_NEWS_PROPERTIES_CIX), NewsPropertiesCix, (LPARAM)&lpsrvr->nsi );
         else
            Adm_Dialog( hInst, GetParent(hwnd), MAKEINTRESOURCE(IDDLG_NEWS_PROPERTIES_IP), NewsPropertiesIP, (LPARAM)&lpsrvr->nsi );

         /* If server name has changed, so delete and re-insert
          * the listbox item.
          */
         if( strcmp( szTmpServerName, lpsrvr->nsi.szServerName ) != 0 )
            {
            ListBox_DeleteString( hwndList, index );
            index = ListBox_AddString( hwndList, lpsrvr->nsi.szServerName );
            ListBox_SetItemData( hwndList, index, (LPARAM)lpsrvr );
            ListBox_SetCurSel( hwndList, index );

            /* Vape old server from the config file.
             */
            Amuser_WritePPString( szNewsServers, szTmpServerName, NULL );

            /* If we renamed the default, update the config file.
             */
            if( lpsrvr == lpsrvrDef )
               Amuser_WritePPString( szNews, "Default Server", lpsrvr->nsi.szServerName );
            }

         /* Update the properties.
          */
         SaveUsenetServer( &lpsrvr->nsi );
         SetFocus( hwndList );
         break;
         }
      }
}

/* This function handles the WM_DRAWITEM message
 */
void FASTCALL UsenetServers_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   if( lpDrawItem->itemID != -1 )
      {
      COLORREF tmpT, tmpB;
      char sz[ 100 ];
      COLORREF T, B;
      HBITMAP hbmpFont;
      BOOL fDefault;
      HFONT hFont;
      HBRUSH hbr;
      SIZE size;
      RECT rc;
      int y;

      /* Get the text we're drawing.
       */
      if( ODT_COMBOBOX == lpDrawItem->CtlType )
         ComboBox_GetLBText( lpDrawItem->hwndItem, lpDrawItem->itemID, sz );
      else
         ListBox_GetText( lpDrawItem->hwndItem, lpDrawItem->itemID, sz );
      rc = lpDrawItem->rcItem;

      /* Is this the default news server?
       */
      if( ODT_COMBOBOX == lpDrawItem->CtlType )
         fDefault = (LPNEWSSERVER)ComboBox_GetItemData( lpDrawItem->hwndItem, lpDrawItem->itemID ) == lpsrvrDef;
      else
         fDefault = (LPNEWSSERVER)ListBox_GetItemData( lpDrawItem->hwndItem, lpDrawItem->itemID ) == lpsrvrDef;
      if( fDefault )
         strcat( sz, GS(IDS_STR1068) );

      /* Set the drawing colours.
       */
      GetOwnerDrawListItemColours( lpDrawItem, &T, &B );

      /* Blank out the line we're drawing.
       * Draw the standard bitmap for this entry.
       */
      hbr = CreateSolidBrush( GetSysColor( COLOR_WINDOW ) );
      tmpB = SetBkColor( lpDrawItem->hDC, GetSysColor( COLOR_WINDOW ) );
      tmpT = SetTextColor( lpDrawItem->hDC, GetSysColor( COLOR_WINDOWTEXT ) );
      rc.right = 18;
      FillRect( lpDrawItem->hDC, &rc, hbr );
      hbmpFont = LoadBitmap( hRscLib, MAKEINTRESOURCE(IDB_NEWSSERVER) );
      Amuser_DrawBitmap( lpDrawItem->hDC, rc.left, rc.top, 16, 16, hbmpFont, 0 );
      DeleteBitmap( hbmpFont );

      /* Draw the label.
       */
      SetTextColor( lpDrawItem->hDC, T );
      SetBkColor( lpDrawItem->hDC, B );
      hFont = SelectFont( lpDrawItem->hDC, fDefault ? hSys8Font : hHelvB8Font );
      GetTextExtentPoint( lpDrawItem->hDC, sz, strlen(sz), &size );
      rc.left = rc.right;
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
}

/* This function handles the WM_MEASUREITEM message
 */
void FASTCALL UsenetServers_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   HFONT hFont;
   HBITMAP hbmpFont;
   BITMAP bmp;
   HDC hdc;

   hdc = GetDC( hwnd );
   hFont = SelectFont( hdc, hHelvB8Font );
   GetTextMetrics( hdc, &tm );
   hbmpFont = LoadBitmap( hRscLib, MAKEINTRESOURCE(IDB_NEWSSERVER) );
   GetObject( hbmpFont, sizeof( BITMAP ), &bmp );
   DeleteBitmap( hbmpFont );
   SelectFont( hdc, hFont );
   ReleaseDC( hwnd, hdc );
   lpMeasureItem->itemHeight = max( tm.tmHeight + tm.tmExternalLeading + 3, bmp.bmHeight );
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL UsenetServers_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsNEWS_SERVERS );
         break;

      case PSN_SETACTIVE:
         nNewsDialogPage = NWO_SERVERS;
         break;
      }
   return( FALSE );
}

/* This function handles the dialog box messages passed to the UsenetSettings
 * dialog.
 */
BOOL EXPORT CALLBACK UsenetHeaders( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, UsenetHeaders_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, UsenetHeaders_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_MEASUREITEM, UsenetHeaders_OnMeasureItem );
      HANDLE_DLGMSG( hwnd, WM_DRAWITEM, UsenetHeaders_OnDrawItem );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, UsenetHeaders_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL UsenetHeaders_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   register int c;
   HWND hwndList;

   /* Ensure headers are loaded.
    */
   LoadUsenetHeaders();

   /* Set the edit field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_TYPE ), LEN_RFCTYPE );
   Edit_LimitText( GetDlgItem( hwnd, IDD_SETTING ), 79 );

   /* Fill the list box with the existing Usenet headers.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   for( c = 0; c < cNewsHeaders; ++c )
      {
      wsprintf( lpTmpBuf, "%s\t%s", (LPSTR)uNewsHeaders[ c ].szType, (LPSTR)uNewsHeaders[ c ].szValue );
      ListBox_AddString( hwndList, lpTmpBuf );
      }
   EnableControl( hwnd, IDD_REMOVE, FALSE );
   EnableControl( hwnd, IDD_ADD, FALSE );
   return( TRUE );
}

/* This function handles the WM_MEASUREITEM message
 */
void FASTCALL UsenetHeaders_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   HFONT hOldFont;
   HDC hdc;

   hdc = GetDC( hwnd );
   hOldFont = SelectFont( hdc, hHelvB8Font );
   GetTextMetrics( hdc, &tm );
   lpMeasureItem->itemHeight = tm.tmHeight + 2;
   SelectFont( hdc, hOldFont );
   ReleaseDC( hwnd, hdc );
}

/* This function handles the WM_DRAWITEM message
 */
void FASTCALL UsenetHeaders_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   if( lpDrawItem->itemID != -1 )
      if( lpDrawItem->itemAction == ODA_FOCUS )
         DrawFocusRect( lpDrawItem->hDC, (LPRECT)&lpDrawItem->rcItem );
      else {
         HWND hwndList;
         HPEN hpen;
         HPEN hpenOld;
         COLORREF T, tmpT;
         COLORREF B, tmpB;
         HBRUSH hbr;
         RECT rc2;
         RECT rc;
         char * psz;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         ListBox_GetText( hwndList, lpDrawItem->itemID, lpTmpBuf );
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

         /* Split the name into type and settings field
          */
         psz = strchr( lpTmpBuf, '\t' );
         *psz++ = '\0';

         /* Erase the entire drawing area
          */
         FillRect( lpDrawItem->hDC, &rc, hbr );
         tmpT = SetTextColor( lpDrawItem->hDC, T );
         tmpB = SetBkColor( lpDrawItem->hDC, B );
         hpen = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNFACE ) );
         hpenOld = SelectPen( lpDrawItem->hDC, hpen );

         /* Get the left coordinate of the Settings edit box
          */
         GetWindowRect( GetDlgItem( hwnd, IDD_SETTING ), &rc2 );
         ScreenToClient( hwndList, (LPPOINT)&rc2 );

         /* Draw the bottom separator */
         MoveToEx( lpDrawItem->hDC, rc.left, rc.bottom - 1, NULL );
         LineTo( lpDrawItem->hDC, rc.right, rc.bottom - 1 );

         /* Draw the name and setting and middle separator */
         rc.left += 2;
         ExtTextOut( lpDrawItem->hDC, rc.left, rc.top, 0, &rc, lpTmpBuf, strlen(lpTmpBuf), NULL );
         rc.left = rc2.left - 2;
         MoveToEx( lpDrawItem->hDC, rc.left, rc.top, NULL );
         LineTo( lpDrawItem->hDC, rc.left, rc.bottom );
         rc.left += 2;
         ExtTextOut( lpDrawItem->hDC, rc.left, rc.top, 0, &rc, psz, strlen( psz ), NULL );

         /* Clean up afterwards. */
         SelectPen( lpDrawItem->hDC, hpenOld );
         DeletePen( hpen );
         SetTextColor( lpDrawItem->hDC, tmpT );
         SetBkColor( lpDrawItem->hDC, tmpB );
         DeleteBrush( hbr );
         }
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL UsenetHeaders_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_LIST:
         if( codeNotify == LBN_SELCHANGE )
            {
            HWND hwndList;
            int index;

LB1:        hwndList = GetDlgItem( hwnd, IDD_LIST );
            if( ( index = ListBox_GetCurSel( hwndList ) ) != LB_ERR )
               {
               char * psz;

               ListBox_GetText( hwndList, index, lpTmpBuf );
               psz = strchr( lpTmpBuf, '\t' );
               *psz++ = '\0';
               SetDlgItemText( hwnd, IDD_TYPE, lpTmpBuf );
               SetDlgItemText( hwnd, IDD_SETTING, psz );
               EnableControl( hwnd, IDD_ADD, *lpTmpBuf != '\0' && *psz != '\0' );
               EnableControl( hwnd, IDD_REMOVE, TRUE );
               SetFocus( GetDlgItem( hwnd, IDD_TYPE ) );
               }
            }
         break;

      case IDD_REMOVE: {
         HWND hwndList;
         int index;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         index = ListBox_GetCurSel( hwndList );
         if( ListBox_DeleteString( hwndList, index ) == index )
            --index;
         if( index != LB_ERR )
            {
            ListBox_SetCurSel( hwndList, index );
            goto LB1;
            }
         SetDlgItemText( hwnd, IDD_TYPE, "" );
         SetDlgItemText( hwnd, IDD_SETTING, "" );
         EnableControl( hwnd, IDD_REMOVE, FALSE );
         EnableControl( hwnd, IDD_ADD, FALSE );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         SetFocus( GetDlgItem( hwnd, IDD_TYPE ) );
         break;
         }

      case IDD_TYPE:
      case IDD_SETTING:
         if( codeNotify == EN_UPDATE )
            {
            EnableControl( hwnd, IDD_ADD,
               Edit_GetTextLength( GetDlgItem( hwnd, IDD_TYPE ) ) > 0 &&
               Edit_GetTextLength( GetDlgItem( hwnd, IDD_SETTING ) ) > 0 );
            PropSheet_Changed( GetParent( hwnd ), hwnd );
            }
         break;

      case IDD_ADD: {
         char chType[ LEN_RFCTYPE+1 ];
         char chValue[ 80 ];
         HWND hwndList;
         int index;

         GetDlgItemText( hwnd, IDD_TYPE, chType, LEN_RFCTYPE+1 );
         GetDlgItemText( hwnd, IDD_SETTING, chValue, 80 );
         if( *chType && *chValue )
            {
            if( strchr( chType, ' ' ) || strchr( chType, ':' ) )
               {
               fMessageBox( hwnd, 0, GS(IDS_STR453), MB_OK|MB_ICONEXCLAMATION );
               HighlightField( hwnd, IDD_TYPE );
               break;
               }
            wsprintf( lpTmpBuf, "%s\t", (LPSTR)chType );
            hwndList = GetDlgItem( hwnd, IDD_LIST );
            if( ( index = ListBox_FindString( hwndList, -1, lpTmpBuf ) ) != LB_ERR )
               {
               if( ListBox_GetCount( hwndList ) == MAX_HEADERS )
                  {
                  wsprintf( lpTmpBuf, GS(IDS_STR262), MAX_HEADERS );
                  fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
                  break;
                  }
               lstrcat( lpTmpBuf, chValue );
               ListBox_DeleteString( hwndList, index );
               index = ListBox_AddString( hwndList, lpTmpBuf );
               }
            else
               {
               lstrcat( lpTmpBuf, chValue );
               index = ListBox_InsertString( hwndList, -1, lpTmpBuf );
               }
            ListBox_SetCurSel( hwndList, index );
            SetDlgItemText( hwnd, IDD_TYPE, "" );
            SetDlgItemText( hwnd, IDD_SETTING, "" );
            EnableControl( hwndList, IDD_ADD, FALSE );
            EnableControl( hwndList, IDD_REMOVE, TRUE );
            SetFocus( GetDlgItem( hwnd, IDD_TYPE ) );
            PropSheet_Changed( GetParent( hwnd ), hwnd );
            }
         break;
         }
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL UsenetHeaders_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsNEWS_HEADERS );
         break;

      case PSN_SETACTIVE:
         nNewsDialogPage = NWO_HEADERS;
         break;

      case PSN_APPLY: {
         register int c;
         HWND hwndList;
         char sz[ 20 ];

         /* Save the News folder.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         for( c = 0; c < ListBox_GetCount( hwndList ); ++c )
            {
            char * psz;

            ListBox_GetText( hwndList, c, lpTmpBuf );
            psz = strchr( lpTmpBuf, '\t' );
            *psz++ = '\0';
            strcpy( uNewsHeaders[ c ].szType, lpTmpBuf );
            strcpy( uNewsHeaders[ c ].szValue, psz );
            wsprintf( lpTmpBuf, "%s:%s", (LPSTR)uNewsHeaders[ c ].szType, (LPSTR)uNewsHeaders[ c ].szValue );
            wsprintf( sz, "CustomHeader%d", c + 1 );
            Amuser_WritePPString( szNews, sz, lpTmpBuf );
            }
         while( cNewsHeaders > c )
            {
            wsprintf( sz, "CustomHeader%d", cNewsHeaders-- );
            Amuser_WritePPString( szNews, sz, NULL );
            }
         cNewsHeaders = c;

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This function dispatches messages for the News page of the
 * Preferences dialog.
 */
BOOL EXPORT CALLBACK UsenetGeneral( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, UsenetGeneral_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, UsenetGeneral_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, UsenetGeneral_OnNotify );
      HANDLE_DLGMSG( hwnd, WM_MEASUREITEM, FolderCombo_OnMeasureItem );
      HANDLE_DLGMSG( hwnd, WM_DRAWITEM, FolderListCombo_OnDrawItem );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL UsenetGeneral_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndList;
   int index;
   PCL pcl;

   SetDlgItemInt( hwnd, IDD_EDIT, inWrapCol, FALSE ); //2.56.2055
   Edit_LimitText( GetDlgItem( hwnd, IDD_EDIT ), 3 );

   /* Fill the list with all folders.
    */
   FillListWithFolders( hwnd, IDD_LIST );

   /* Set the options.
    */
   CheckDlgButton( hwnd, IDD_AUTOQUOTE, fAutoQuoteNews );
   SetDlgItemText( hwnd, IDD_QUOTEHEADER, szQuoteNewsHeader );
   Edit_LimitText( GetDlgItem( hwnd, IDD_QUOTEHEADER ), sizeof(szQuoteNewsHeader) - 1 );

   /* Highlight the incoming news folder.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   pcl = GetNewsFolder();
   index = RealComboBox_FindItemData( hwndList, -1, (LPARAM)pcl );
   ComboBox_SetCurSel( hwndList, index );

   /* Set the parameter picker button.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_PARMPICKER ), hInst, IDB_PICKER );
   return( TRUE );
}

/* This function writes the specified text to the current insertion
 * point in the Quote Header input field.
 */
static void FASTCALL PutQuoteHeaderData( HWND hwnd, char * pData )
{
   HWND hwndEdit;

   hwndEdit = GetDlgItem( hwnd, IDD_QUOTEHEADER );
   Edit_ReplaceSel( hwndEdit, pData );
   SetFocus( hwndEdit );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL UsenetGeneral_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EDIT:
         if( codeNotify == EN_CHANGE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDM_QP_ADDRESS:       PutQuoteHeaderData( hwnd, "%a" ); break;
      case IDM_QP_FULLNAME:      PutQuoteHeaderData( hwnd, "%f" ); break;
      case IDM_QP_MESSAGEID:     PutQuoteHeaderData( hwnd, "%m" ); break;
      case IDM_QP_DATE:          PutQuoteHeaderData( hwnd, "%d" ); break;

      case IDD_PARMPICKER: {
         HMENU hPopupMenu;
         RECT rc;

         GetWindowRect( GetDlgItem( hwnd, id ), &rc );
         hPopupMenu = GetSubMenu( hPopupMenus, IPOP_QUOTEHDRPICKER );
         TrackPopupMenu( hPopupMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, rc.right, rc.top, 0, hwnd, NULL );
         break;
         }

      case IDD_AUTOQUOTE:
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_QUOTEHEADER:
         if( codeNotify == EN_CHANGE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_LIST:
         if( codeNotify == CBN_SELCHANGE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL UsenetGeneral_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsNEWS_GENERAL );
         break;

      case PSN_SETACTIVE:
         nLastOptionsDialogPage = NWO_GENERAL;
         break;

      case PSN_APPLY: {
         HWND hwndList;
         PCL pclFolder;
         int index;

         if( !GetDlgInt( hwnd, IDD_EDIT, &inWrapCol, 40, 510 ) ) //2.56.2055
            {
            PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, NWO_GENERAL );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }

         /* Save the News folder.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         index = ComboBox_GetCurSel( hwndList );
         if( CB_ERR == index )
            {
            PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, NWO_GENERAL );
            fMessageBox( hwnd, 0, GS(IDS_STR854), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_LIST );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }
         pclFolder = (PCL)ComboBox_GetItemData( hwndList, index );
         if( NULL == pclFolder || !Amdb_IsFolderPtr( pclFolder ) )
            {
            PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, NWO_GENERAL );
            fMessageBox( hwnd, 0, GS(IDS_STR854), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_LIST );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }
         Amuser_WritePPInt( szSettings, "WordWrapNews", inWrapCol ); //2.56.2055
         WriteFolderPathname( lpTmpBuf, pclFolder );
         Amuser_WritePPString( szPreferences, "NewsFolder", lpTmpBuf );
         pclNewsFolder = pclFolder;

         /* Get options.
          */
         fAutoQuoteNews = IsDlgButtonChecked( hwnd, IDD_AUTOQUOTE );
         Edit_GetText( GetDlgItem( hwnd, IDD_QUOTEHEADER ), szQuoteNewsHeader, sizeof(szQuoteNewsHeader) );

         /* Save new options.
          */
         Amuser_WritePPInt( szSettings, "AutoQuote", fAutoQuoteNews );
         Amuser_WritePPString( szSettings, "QuoteHeader", szQuoteNewsHeader );

         /* Update the inbasket/message window folder lists (in
          * case the store in folders have changed, they will need re-colouring).
          */
         SendAllWindows( WM_APPCOLOURCHANGE, WIN_INBASK, 0L );

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This function handles the CIX Usenet properties dialog.
 */
BOOL EXPORT CALLBACK NewsPropertiesCix( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, NewsPropertiesCix_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the NewsPropertiesCix
 * dialog.
 */
LRESULT FASTCALL NewsPropertiesCix_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, NewsPropertiesCix_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, NewsPropertiesCix_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsNEWS_PROPERTIES_CIX );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL NewsPropertiesCix_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   NEWSSERVERINFO * pnsi;

   /* Get the news server record.
    */
   pnsi = (NEWSSERVERINFO *)lParam;
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* Set the batch size limit.
    */
   CheckDlgButton( hwnd, IDD_LIMITUSENETBATCHSIZE, pnsi->dwUsenetBatchsize > 0 );
   if( pnsi->dwUsenetBatchsize )
      SetDlgItemLongInt( hwnd, IDD_USENETBATCHSIZE, pnsi->dwUsenetBatchsize );
   else
      EnableControl( hwnd, IDD_USENETBATCHSIZE, FALSE );
   Edit_LimitText( GetDlgItem( hwnd, IDD_USENETBATCHSIZE ), 7 );

   /* Set the restriction info.
    */
   CheckDlgButton( hwnd, IDD_RESTRICTION, pnsi->fRestrictedAccess );
   CheckDlgButton( hwnd, IDD_WEEKEND, pnsi->fWeekendAccess );
   SetDlgItemText( hwnd, IDD_RESTRICTEDLIST, pnsi->szAccessTimes );
   EnableControl( hwnd, IDD_RESTRICTEDLIST, pnsi->fRestrictedAccess );
   fAntiSpamCixnews = Amuser_GetPPInt( szSettings, "AntiSpamCixnews", FALSE );
   CheckDlgButton( hwnd, IDD_ANTISPAMCIXNEWS, fAntiSpamCixnews );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL NewsPropertiesCix_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_RESTRICTION:
         EnableControl( hwnd, IDD_RESTRICTEDLIST, IsDlgButtonChecked( hwnd, id ) );
         break;

      case IDD_LIMITUSENETBATCHSIZE:
         EnableControl( hwnd, IDD_USENETBATCHSIZE, IsDlgButtonChecked( hwnd, id ) );
         break;

      case IDOK: {
         NEWSSERVERINFO * pnsi;
         HWND hwndEdit;

         /* Get a pointer to this server record. Then vape
          * the existing contents.
          */
         pnsi = (NEWSSERVERINFO *)GetWindowLong( hwnd, DWL_USER );

         /* Initialise all fields that get changed.
          */
         memset( &pnsi->szServerName, 0, LEN_SERVERNAME );
         memset( &pnsi->szAccessTimes, 0, sizeof(pnsi->szAccessTimes) );
         pnsi->dwUsenetBatchsize = 0L;
         pnsi->fWeekendAccess = FALSE;
         pnsi->fRestrictedAccess = FALSE;

         /* Set the server name.
          */
         strcpy( pnsi->szServerName, szCixnews );

         /* Get the usenet batch size
          */
         if( IsDlgButtonChecked( hwnd, IDD_LIMITUSENETBATCHSIZE ) )
            if( !GetDlgLongInt( hwnd, IDD_USENETBATCHSIZE, &pnsi->dwUsenetBatchsize, 50, 99999999 ) )
               break;

         /* Get the restriction list.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_RESTRICTEDLIST );
         if( IsDlgButtonChecked( hwnd, IDD_RESTRICTION ) )
            {
            if( Edit_GetTextLength( hwndEdit ) == 0 )
               {
               fMessageBox( hwnd, 0, GS(IDS_STR1234), MB_OK|MB_ICONEXCLAMATION );
               HighlightField( hwnd, IDD_RESTRICTEDLIST );
               break;
               }
            pnsi->fRestrictedAccess = TRUE;
            }
         Edit_GetText( hwndEdit, pnsi->szAccessTimes, sizeof(pnsi->szAccessTimes) );


         /* Set weekend access if selected.
          */
         if( IsDlgButtonChecked( hwnd, IDD_WEEKEND ) )
            pnsi->fWeekendAccess = TRUE;

         /* Set antispamfrom if selected.
          */
         Amuser_WritePPInt( szSettings, "AntiSpamCixnews", IsDlgButtonChecked( hwnd, IDD_ANTISPAMCIXNEWS ) );
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* This function handles the Internet Usenet properties dialog.
 */
BOOL EXPORT CALLBACK NewsPropertiesIP( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, NewsPropertiesIP_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the NewsPropertiesIP
 * dialog.
 */
LRESULT FASTCALL NewsPropertiesIP_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, NewsPropertiesIP_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, NewsPropertiesIP_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsNEWS_PROPERTIES_IP );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL NewsPropertiesIP_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   NEWSSERVERINFO * pnsi;
   BOOL fNNTPAuthInfo;
   HWND hwndEdit;

   /* Get the news server record.
    */
   pnsi = (NEWSSERVERINFO *)lParam;
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* Fill out the details.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_NEWSSERVER );
   Edit_SetText( hwndEdit, pnsi->szServerName );
   Edit_LimitText( hwndEdit, sizeof(pnsi->szServerName) - 1 );

   SetDlgItemInt( hwnd, IDD_NEWSPORT, pnsi->wNewsPort, FALSE );
   SendDlgItemMessage( hwnd, IDD_NEWSPORT, EM_LIMITTEXT, 5, 0L );

   /* Fill out authentication details if applicable.
    */
   fNNTPAuthInfo = *pnsi->szAuthInfoUser != '\0';
   if( fNNTPAuthInfo )
      {
      Edit_SetText( GetDlgItem( hwnd, IDD_AUTHINFOUSER ), pnsi->szAuthInfoUser );
      Amuser_Decrypt( pnsi->szAuthInfoPass, rgEncodeKey );
      Edit_SetText( GetDlgItem( hwnd, IDD_AUTHINFOPASS ), pnsi->szAuthInfoPass );
      Amuser_Encrypt( pnsi->szAuthInfoPass, rgEncodeKey );
      }
   Edit_LimitText( GetDlgItem( hwnd, IDD_AUTHINFOUSER ), sizeof(pnsi->szAuthInfoUser) - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_AUTHINFOPASS ), sizeof(pnsi->szAuthInfoPass) - 1 );

   /* Set the authinfo stuff.
    */
   CheckDlgButton( hwnd, IDD_NNTPAUTH, fNNTPAuthInfo );
   EnableControl( hwnd, IDD_PAD1, fNNTPAuthInfo );
   EnableControl( hwnd, IDD_PAD2, fNNTPAuthInfo );
   EnableControl( hwnd, IDD_AUTHINFOUSER, fNNTPAuthInfo );
   EnableControl( hwnd, IDD_AUTHINFOPASS, fNNTPAuthInfo );

   /* Do we get newsgroup descriptions?
    */
   CheckDlgButton( hwnd, IDD_GETDESCRIPTIONS, pnsi->fGetDescriptions );

   /* Set the restriction info.
    */
   SetDlgItemText( hwnd, IDD_RESTRICTEDLIST, pnsi->szAccessTimes );
   EnableControl( hwnd, IDD_RESTRICTEDLIST, pnsi->fRestrictedAccess );
   CheckDlgButton( hwnd, IDD_RESTRICTION, pnsi->fRestrictedAccess );
   CheckDlgButton( hwnd, IDD_WEEKEND, pnsi->fWeekendAccess );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL NewsPropertiesIP_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_RESTRICTION:
         EnableControl( hwnd, IDD_RESTRICTEDLIST, IsDlgButtonChecked( hwnd, id ) );
         break;

      case IDD_NNTPAUTH: {
         BOOL fSelected;

         fSelected = IsDlgButtonChecked( hwnd, IDD_NNTPAUTH );
         EnableControl( hwnd, IDD_PAD1, fSelected );
         EnableControl( hwnd, IDD_PAD2, fSelected );
         EnableControl( hwnd, IDD_AUTHINFOUSER, fSelected );
         EnableControl( hwnd, IDD_AUTHINFOPASS, fSelected );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
         }

      case IDOK: {
         NEWSSERVERINFO * pnsi;
         HWND hwndEdit;
         int wNewsPort;

         /* Get a pointer to this server record. Then vape
          * the existing contents.
          */
         pnsi = (NEWSSERVERINFO *)GetWindowLong( hwnd, DWL_USER );

         /* Initialise all fields that get changed.
          */
         memset( &pnsi->szServerName, 0, LEN_SERVERNAME );
         memset( &pnsi->szAccessTimes, 0, sizeof(pnsi->szAccessTimes) );
         memset( &pnsi->szAuthInfoUser, 0, sizeof(pnsi->szAuthInfoUser) );
         memset( &pnsi->szAuthInfoPass, 0, sizeof(pnsi->szAuthInfoPass) );
         pnsi->dwUsenetBatchsize = 0L;
         pnsi->wNewsPort = 0L;
         pnsi->fGetDescriptions = FALSE;
         pnsi->fWeekendAccess = FALSE;
         pnsi->fRestrictedAccess = FALSE;

         /* Read and store the news server name.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_NEWSSERVER );
         if( Edit_GetTextLength( hwndEdit ) == 0 )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR736), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_NEWSSERVER );
            break;
            }

         /* Save the news server.
          */
         Edit_GetText( hwndEdit, pnsi->szServerName, sizeof(pnsi->szServerName) );

         /* Save the news server port.
          */
         if( !GetDlgInt( hwnd, IDD_NEWSPORT, &wNewsPort, 0, 65535 ) )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR1230), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_NEWSPORT );
            break;
            }
         pnsi->wNewsPort = wNewsPort;

         /* Use authentication info?
          */
         fNNTPAuthInfo = IsDlgButtonChecked( hwnd, IDD_NNTPAUTH );
         if( fNNTPAuthInfo )
            {
            /* Get the user name.
             */
            hwndEdit = GetDlgItem( hwnd, IDD_AUTHINFOUSER );
            Edit_GetText( hwndEdit, pnsi->szAuthInfoUser, sizeof(pnsi->szAuthInfoUser) );

            /* Get the password.
             */
            hwndEdit = GetDlgItem( hwnd, IDD_AUTHINFOPASS );
            Edit_GetText( hwndEdit, pnsi->szAuthInfoPass, sizeof(pnsi->szAuthInfoPass) );
            Amuser_Encrypt( pnsi->szAuthInfoPass, rgEncodeKey );
            }

         /* Do we get descriptions?
          */
         pnsi->fGetDescriptions = IsDlgButtonChecked( hwnd, IDD_GETDESCRIPTIONS );

         /* Get the restriction list.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_RESTRICTEDLIST );
         if( IsDlgButtonChecked( hwnd, IDD_RESTRICTION ) )
            {
            if( Edit_GetTextLength( hwndEdit ) == 0 )
               {
               fMessageBox( hwnd, 0, GS(IDS_STR1234), MB_OK|MB_ICONEXCLAMATION );
               HighlightField( hwnd, IDD_RESTRICTEDLIST );
               break;
               }
            pnsi->fRestrictedAccess = TRUE;
            }
         Edit_GetText( hwndEdit, pnsi->szAccessTimes, sizeof(pnsi->szAccessTimes) );

         /* Set weekend access if selected.
          */
         if( IsDlgButtonChecked( hwnd, IDD_WEEKEND ) )
            pnsi->fWeekendAccess = TRUE;
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* This function loads the Usenet custom headers if they have
 * not already been loaded.
 */
void FASTCALL LoadUsenetHeaders( void )
{
   if( !fUsenetHeadersLoaded )
      {
      register int c;

      /* Get Usenet header formats
       */
      cNewsHeaders = 0;
      for( c = 0; c < MAX_HEADERS; ++c )
         {
         char sz[ 20 ];
         register int d;
         char * psz;

         wsprintf( sz, "CustomHeader%d", c + 1 );
         if( !Amuser_GetPPString( szNews, sz, "", lpTmpBuf, 128 ) )
            break;
         psz = lpTmpBuf;
         for( d = 0; *psz && *psz != ':'; ++d )
            {
            if( d < sizeof( uNewsHeaders->szType ) )
               {
               if( *psz == ' ' )
                  uNewsHeaders[ c ].szType[ d ] = '-';
               else
                  uNewsHeaders[ c ].szType[ d ] = *psz;
               }
            ++psz;
            }
         uNewsHeaders[ c ].szType[ d ] = '\0';
         ++psz;
         for( d = 0; *psz; ++d )
            {
            if( d < sizeof( uNewsHeaders->szValue ) )
               uNewsHeaders[ c ].szValue[ d ] = *psz;
            ++psz;
            }
         uNewsHeaders[ c ].szValue[ d ] = '\0';
         ++cNewsHeaders;
         }

      /* If no headers, create defaults
       */
      if( cNewsHeaders == 0 && fFirstTimeThisUser )
         {
         char szValue[ 80 ];

         strcpy( uNewsHeaders[ cNewsHeaders ].szType, "X-URL" );
         strcpy( uNewsHeaders[ cNewsHeaders ].szValue, "http://cix.uk" );
         ++cNewsHeaders;
         strcpy( uNewsHeaders[ cNewsHeaders ].szType, "X-News-Software" );
         strcpy( uNewsHeaders[ cNewsHeaders ].szValue, szAppName );
         ++cNewsHeaders;
         About_GetSiteName( szValue, sizeof(szValue) - 1 );
         if( *szValue )
            {
            strcpy( uNewsHeaders[ cNewsHeaders ].szType, "Organization" );
            strcpy( uNewsHeaders[ cNewsHeaders ].szValue, szValue );
            ++cNewsHeaders;
            }
         }
      fUsenetHeadersLoaded = TRUE;
      }
}

/* This function determines whether or not Usenet access is
 * permitted.
 */
BOOL FASTCALL OkayUsenetAccess( char * pszNewsServer )
{
   WORD wCurTime;
   AM_DATE date;

   LPNEWSSERVER lpsrvr;

   /* Ensure list of servers is loaded.
    */
   LoadAllUsenetServers();

   /* First locate our news server.
    */
   for( lpsrvr = lpsrvrFirst; lpsrvr; lpsrvr = lpsrvr->lpsrvrNext )
      if( strcmp( lpsrvr->nsi.szServerName, pszNewsServer ) == 0 )
         break;
   if( NULL == lpsrvr )
      return( FALSE );

   if( !lpsrvr->nsi.fRestrictedAccess )
      return( TRUE );

   /* Get the current date.
    */
   Amdate_GetCurrentDate( &date );
   if( lpsrvr->nsi.fWeekendAccess && ( date.iDayOfWeek == 0 || date.iDayOfWeek == 6 ) )
      return( TRUE );

   /* Consult the access times string.
    */
   wCurTime = Amdate_GetPackedCurrentTime();
   if( *lpsrvr->nsi.szAccessTimes )
      {
      AM_TIME tmEnd;
      AM_TIME tmStart;
      char * p;

      tmStart.iHour = 0;
      tmStart.iMinute = 0;
      tmStart.iSecond = tmStart.iHSeconds = 0;
      tmEnd.iSecond = tmEnd.iHSeconds = 0;
      for( p = lpsrvr->nsi.szAccessTimes; *p; )
         {
         tmEnd.iHour = 0;
         tmEnd.iMinute = 0;
         while( *p == ' ' )
            ++p;
         while( isdigit( *p ) )
            tmEnd.iHour = ( tmEnd.iHour * 10 ) + ( *p++ - '0' );
         while( *p && !isdigit( *p ) )
            ++p;
         while( isdigit( *p ) )
            tmEnd.iMinute = ( tmEnd.iMinute * 10 ) + ( *p++ - '0' );
         while( *p == ' ' )
            ++p;
         if( *p == '-' )
            {
            ++p;
            tmStart = tmEnd;
            }
         else
            {
            WORD wStartTime;
            WORD wEndTime;

            wStartTime = Amdate_PackTime( &tmStart );
            wEndTime = Amdate_PackTime( &tmEnd );
            if( wEndTime < wStartTime )
               {
               if( !( wCurTime > wEndTime && wCurTime < wStartTime ) )
                  return( TRUE );
               }
            else if( wCurTime >= wStartTime && wCurTime <= wEndTime )
               return( TRUE );
            }
         tmEnd.iHour = 23;
         tmEnd.iMinute = 59;
         }
      return( FALSE );
      }
   return( TRUE );
}

/* Given a newsgroup name, this function locates the name of the
 * server from which this newsgroup is dervived.
 */
BOOL FASTCALL GetNewsgroupNewsServer( char * pszNewsgroup, char * pszServerName, int cbServer )
{
   char szNewsgroup[ 80 ];
   PTL ptl;
   LPSTR lpNewsBuf;

   ptl = NULL;
      
   lpNewsBuf = pszNewsgroup;
   while( *lpNewsBuf && ptl == NULL )
   {
      ExtractString( lpNewsBuf, szNewsgroup, sizeof(szNewsgroup) - 1 );
      ptl = PtlFromNewsgroup( szNewsgroup );
      if( ptl == NULL )
      {
         lpNewsBuf+= strlen( szNewsgroup );
         if( *lpNewsBuf == ',' || *lpNewsBuf == ' ' )
            lpNewsBuf++;
      }
   }

   if( NULL != ptl )
      {
      Amdb_GetCookie( ptl, NEWS_SERVER_COOKIE, pszServerName, "", cbServer );
      if( *pszServerName )
         return( TRUE );
      }
   return( FALSE );
}

/* This function determines whether the specified newsgroup is
 * subscribed under CIX Usenet.
 */
BOOL FASTCALL IsCixnewsNewsgroup( char * pszNewsgroup )
{
   char szSubNewsServer[ 64 ];
   char szDefSrvr[ 64 ];

   Amuser_GetPPString( szNews, "Default Server", szNewsServer, szDefSrvr, 64 );

   if( GetNewsgroupNewsServer( pszNewsgroup, szSubNewsServer, 64 ) )
      return( strcmp( szSubNewsServer, szCixnews ) == 0 );
   else if( strlen( szDefSrvr ) > 0 )
      return( _strcmpi( szCixnews, szDefSrvr ) == 0 );
   else
      return( FALSE );
}

/* This function locates the PTL for the specified newsgroup.
 */
PTL FASTCALL PtlFromNewsgroup( char * pszNewsgroup )
{
   PCAT pcat;
   PCL pcl;
   PTL ptl;

   for( pcat = Amdb_GetFirstCategory(); pcat; pcat = Amdb_GetNextCategory( pcat ) )
      for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextUnconstrainedFolder( pcl ) )
         for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
            if( strcmp( pszNewsgroup, Amdb_GetTopicName( ptl ) ) == 0 )
               return( ptl );
   return( NULL );
}

/* This function scans the specified folder list and returns
 * TRUE if there are any NNTP folders in the list.
 */
BOOL FASTCALL IsNNTPInFolderList( HPSTR hpszFolderList )
{
   NEWSFOLDERLIST nfl;

   /* Build a list of PTLs for each folder referenced then
    * scan the list.
    */
   if( BuildFolderList( hpszFolderList, &nfl ) )
      {
      register int c;

      /* Loop for each newsgroup in the list
       */
      for( c = 0; c < nfl.cTopics; ++c )
         {
         TOPICINFO topicinfo;

         Amdb_GetTopicInfo( nfl.ptlTopicList[ c ], &topicinfo );
         if( !IsCixnewsNewsgroup( topicinfo.szTopicName ) )
            {
            FreeMemory( (LPVOID)&nfl.ptlTopicList );
            return( TRUE );
            }
         }
      FreeMemory( (LPVOID)&nfl.ptlTopicList );
      }
   return( FALSE );
}

/* This function brings up the Add News Server dialog. The user
 * can install a new news server. On return, it fills out the
 * NEWSSERVERINFO structure.
 */
LPARAM FASTCALL CmdAddNewsServer( HWND hwnd, NEWSSERVERINFO * pnsi )
{
   memset( pnsi, 0, sizeof(NEWSSERVERINFO) );
   pnsi->wNewsPort = 119;
   if( Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_NEWS_PROPERTIES_IP), NewsPropertiesIP, (LPARAM)pnsi ) )
      {
      LPNEWSSERVER lpsrvr;

      /* Install this into the internal list of Usenet
       * servers.
       */
      CreateNewsServerShortName( pnsi->szServerName, pnsi->szShortName );
      pnsi->fActive = TRUE;
      lpsrvr = InstallUsenetServer( pnsi );

      /* Save the configuration
       */
      if( NULL != lpsrvr )
         SaveUsenetServer( pnsi );
      return( (LPARAM)lpsrvr );
      }
   return( 0L );
}

/* This function installs the specified news server.
 */
void FASTCALL CmdCreateNewsServer( char * pszServerName )
{
   LPNEWSSERVER lpsrvr;
   NEWSSERVERINFO nsi;

   /* Install this into the internal list of Usenet
    * servers.
    */
   memset( &nsi, 0, sizeof(NEWSSERVERINFO) );
   strcpy( nsi.szServerName, pszServerName );
   CreateNewsServerShortName( nsi.szServerName, nsi.szShortName );
   nsi.fActive = TRUE;
   nsi.wNewsPort = (WORD)119;

   lpsrvr = InstallUsenetServer( &nsi );

   /* Save the configuration
    */
   if( NULL != lpsrvr )
      SaveUsenetServer( &nsi );
}

/* This function updates the settings for the specified news
 * server from the NEWSSERVERINFO structure.
 */
BOOL FASTCALL SetUsenetServerDetails( NEWSSERVERINFO * pnsi )
{
   LPNEWSSERVER lpsrvr;

   /* Ensure list of servers is loaded.
    */
   LoadAllUsenetServers();

   /* Scan the list looking for ours
    */
   for( lpsrvr = lpsrvrFirst; lpsrvr; lpsrvr = lpsrvr->lpsrvrNext )
      if( strcmp( lpsrvr->nsi.szServerName, pnsi->szServerName ) == 0 )
         {
         lpsrvr->nsi = *pnsi;
         SaveUsenetServer( &lpsrvr->nsi );
         return( TRUE );
         }
   return( FALSE );
}

/* Given the name of a news server, this function fills out a
 * NEWSSERVERINFO structure with details of that server.
 */
BOOL FASTCALL GetUsenetServerDetails( char * pszNewsServer, NEWSSERVERINFO * pnsi )
{
   LPNEWSSERVER lpsrvr;

   /* Ensure list of servers is loaded.
    */
   LoadAllUsenetServers();

   /* Scan the list looking for ours
    */
   for( lpsrvr = lpsrvrFirst; lpsrvr; lpsrvr = lpsrvr->lpsrvrNext )
      if( strcmp( lpsrvr->nsi.szServerName, pszNewsServer ) == 0 )
         {
         *pnsi = lpsrvr->nsi;
         return( TRUE );
         }
   return( FALSE );
}

/* This function reads the list of Usenet servers from the
 * configuration file and creates defaults if none are
 * listed.
 */
void FASTCALL LoadAllUsenetServers( void )
{
   if( !fServerListLoaded )
      {
      LPNEWSSERVER lpsrvr;
      char szDefSrvr[ 64 ];
      LPSTR lpsz;

      INITIALISE_PTR(lpsz);

      /* Get the list of News Server entries in the
       * configuration file.
       */
      if( fNewMemory( &lpsz, 8000 ) )
         {
         UINT c;

         Amuser_GetPPString( szNewsServers, NULL, "", lpsz, 8000 );
         for( c = 0; lpsz[ c ]; ++c )
            {
            NEWSSERVERINFO nsi;
            int wUpdateDate;
            int wUpdateTime;
            int wNewsPort;
            LPSTR lp;

            /* Read the configuration setting
             */
            memset( &nsi, 0, sizeof(NEWSSERVERINFO) );
            strcpy( nsi.szServerName, &lpsz[ c ] );
            Amuser_GetPPString( szNewsServers, &lpsz[ c ], "", lpTmpBuf, LEN_TEMPBUF );
            lp = lpTmpBuf;

            /* Read the shortname.
             */
            lp = IniReadText( lp, nsi.szShortName, sizeof(nsi.szShortName)-1 );

            /* For cixnews, get batch size as next parameter.
             */
            if( strcmp( nsi.szServerName, szCixnews ) == 0 )
               lp = IniReadLong( lp, &nsi.dwUsenetBatchsize );
            else
               {
               char szAuthInfoPass[ 64 ];

               /* Read authentication info.
                */
               lp = IniReadText( lp, nsi.szAuthInfoUser, sizeof(nsi.szAuthInfoUser)-1 );
               lp = IniReadText( lp, szAuthInfoPass, sizeof(szAuthInfoPass)-1 );
               if( *nsi.szAuthInfoUser )
                  DecodeLine64( szAuthInfoPass, nsi.szAuthInfoPass, sizeof(nsi.szAuthInfoPass)-1 );
               }

            /* Read access times.
             */
            lp = IniReadText( lp, nsi.szAccessTimes, sizeof(nsi.szAccessTimes)-1 );

            /* Read the flags.
             */
            lp = IniReadInt( lp, &nsi.fActive );
            lp = IniReadInt( lp, &nsi.fGetDescriptions );

            /* Get the date/time when the list of newsgroups
             * for this server was last retrieved.
             */
            lp = IniReadInt( lp, &wUpdateDate );
            lp = IniReadInt( lp, &wUpdateTime );
            nsi.wUpdateDate = (WORD)wUpdateDate;
            nsi.wUpdateTime = (WORD)wUpdateTime;

            /* New parameter - weekend access flag.
             */
            lp = IniReadInt( lp, &nsi.fWeekendAccess );

            /* Read the port number.
             */
            lp = IniReadInt( lp, &wNewsPort );

            /* If the port setting is blank, set it to 119
             */

            if( wNewsPort == 0 )
               wNewsPort = 119;
            nsi.wNewsPort = (WORD)wNewsPort;

            /* New parameter - restricted access flag.
             */
            lp = IniReadInt( lp, &nsi.fRestrictedAccess );

            /* Install this news server.
             */
            InstallUsenetServer( &nsi );

            /* Skip to next item.
             */
            c += strlen( &lpsz[ c ] );
            }
         FreeMemory( &lpsz );
         }

      /* If none installed, add some defaults.
       */
      if( NULL == lpsrvrFirst )
         {
         NEWSSERVERINFO nsi;

         /* If CIX service installed, add the CIX usenet
          * gateway details.
          */
         if( fUseLegacyCIX )
            {
            memset( &nsi, 0, sizeof(NEWSSERVERINFO) );
            strcpy( nsi.szServerName, szCixnews );
            CreateNewsServerShortName( nsi.szServerName, nsi.szShortName );
            nsi.fActive = TRUE;
            InstallUsenetServer( &nsi );
            SaveUsenetServer( &nsi );
            }

         /* If internet service installed, add the ISP's
          * news server details.
          */
         if( fUseInternet && *szNewsServer )
            {
            memset( &nsi, 0, sizeof(NEWSSERVERINFO) );
            strcpy( nsi.szServerName, szNewsServer );
            CreateNewsServerShortName( nsi.szServerName, nsi.szShortName );
            nsi.fActive = TRUE;
            nsi.wNewsPort = 119;
            InstallUsenetServer( &nsi );
            SaveUsenetServer( &nsi );
            }
         }

      /* Get the default server.
       */
      Amuser_GetPPString( szNews, "Default Server", szNewsServer, szDefSrvr, 64 );

      /* Scan the list looking for ours
       */
      for( lpsrvr = lpsrvrFirst; lpsrvr; lpsrvr = lpsrvr->lpsrvrNext )
         if( strcmp( lpsrvr->nsi.szServerName, szDefSrvr ) == 0 )
            {
            lpsrvrDef = lpsrvr;
            break;
            }

      /* We've got the list now.
       */
      fServerListLoaded = TRUE;
      }
}

/* This function derives an unique short name from the news
 * server name. This will be used as a filename.
 */
void FASTCALL CreateNewsServerShortName( char * pszServerName, char * pszShortName )
{
   char szShortName[ 9 ];
   LPNEWSSERVER lpsrvr;
   register int i;
   register int j;
   char chDigit;

   /* Get the first 8 characters.
    */
   for( i = j = 0; j < 8 && pszServerName[ i ]; ++i )
      if( pszServerName[ i ] != '.' )
         szShortName[ j++ ] = pszServerName[ i ];
   szShortName[ j ] = '\0';

   /* Look for a collision with existing shortnames.
    */
   chDigit = '0';
   lpsrvr = lpsrvrFirst;
   while( lpsrvr )
      if( strcmp( szShortName, lpsrvr->nsi.szShortName ) == 0 )
         {
         /* Collision found, so add a digit to the end
          * of the name.
          */
         szShortName[ j-1 ] = chDigit++;
         if( chDigit > '9' )
            chDigit = 'A';
         lpsrvr = lpsrvrFirst;
         }
      else
         lpsrvr = lpsrvr->lpsrvrNext;

   /* Return the successful shortname.
    */
   strcpy( pszShortName, szShortName );
}

/* This function saves the configuration for the specified
 * server.
 */
void FASTCALL SaveUsenetServer( NEWSSERVERINFO * pnsi )
{
   LPSTR lp;

   lp = lpTmpBuf;
   lp = IniWriteText( lp, pnsi->szShortName );
   *lp++ = ',';
   if( strcmp( pnsi->szServerName, szCixnews ) == 0 )
      lp = IniWriteValue( lp, pnsi->dwUsenetBatchsize );
   else
   {
      lp = IniWriteText( lp, pnsi->szAuthInfoUser );
      *lp++ = ',';
      if( *pnsi->szAuthInfoUser )
         {
         char szAuthInfoPass[ 64 ];

         EncodeLine64( pnsi->szAuthInfoPass, LEN_PASSWORD, szAuthInfoPass );
         lp = IniWriteText( lp, szAuthInfoPass );
         }
   }
   *lp++ = ',';
   lp = IniWriteText( lp, pnsi->szAccessTimes );
   *lp++ = ',';
   lp = IniWriteValue( lp, pnsi->fActive );
   *lp++ = ',';
   lp = IniWriteValue( lp, pnsi->fGetDescriptions );
   *lp++ = ',';
   lp = IniWriteValue( lp, pnsi->wUpdateDate );
   *lp++ = ',';
   lp = IniWriteValue( lp, pnsi->wUpdateTime );
   *lp++ = ',';
   lp = IniWriteValue( lp, pnsi->fWeekendAccess );
   *lp++ = ',';
   lp = IniWriteValue( lp, pnsi->wNewsPort );
   *lp++ = ',';
   lp = IniWriteValue( lp, pnsi->fRestrictedAccess );
   Amuser_WritePPString( szNewsServers, pnsi->szServerName, lpTmpBuf );
}

/* This function fills the specified combobox with a list of
 * all installed news servers.
 */
void FASTCALL FillUsenetServersCombo( HWND hwnd, int id, char * pszSelectedServer )
{
   LPNEWSSERVER lpsrvrThis;
   LPNEWSSERVER lpsrvr;
   int cbMaxWidth;
   HWND hwndList;
   int index;

   /* Make a list handle.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, id ) );

   /* Ensure list of Usenet servers is installed.
    */
   LoadAllUsenetServers();

   /* Read the list of Usenet servers
    * installed.
    */
   cbMaxWidth = 0;
   lpsrvrThis = NULL;
   for( lpsrvr = lpsrvrFirst; lpsrvr; lpsrvr = lpsrvr->lpsrvrNext )
      {
      HFONT hFont;
      SIZE size;
      HDC hdc;

      /* Add to the list.
       */
      index = ComboBox_AddString( hwndList, lpsrvr->nsi.szServerName );
      ComboBox_SetItemData( hwndList, index, (LPARAM)lpsrvr );
      if( strcmp( pszSelectedServer, lpsrvr->nsi.szServerName ) == 0 )
         lpsrvrThis = lpsrvr;

      /* Get the item width. Use it to set the max width if
       * necessary.
       */
      hdc = GetDC( hwndList );
      hFont = SelectFont( hdc, hSys8Font );
      strcpy( lpTmpBuf, lpsrvr->nsi.szServerName );
      if( lpsrvr == lpsrvrDef )
         strcat( lpTmpBuf, GS(IDS_STR1068) );
      GetTextExtentPoint( hdc, lpTmpBuf, strlen(lpTmpBuf), &size );
      SelectFont( hdc, hFont );
      ReleaseDC( hwndList, hdc );
      if( size.cx + 24 > cbMaxWidth )
         cbMaxWidth = size.cx + 24;
      }

   /* CB_SETDROPPEDWIDTH only valid in Win95.
    */
#ifdef WIN32
   if( 0 < cbMaxWidth && wWinVer >= 0x35F )
      SendMessage( hwndList, CB_SETDROPPEDWIDTH, cbMaxWidth, 0L );
#endif

   /* Select the default server.
    */
   if( CB_ERR == ComboBox_GetCurSel( hwndList ) )
      {
      index = RealComboBox_FindItemData( hwndList, -1, (LPARAM)( lpsrvrThis ? lpsrvrThis : lpsrvrDef ) );
      if( LB_ERR == index )
         index = 0;
      ComboBox_SetCurSel( hwndList, index );
      }
}

/* This function installs the specified news server into the list of
 * available news servers.
 */
LPNEWSSERVER FASTCALL InstallUsenetServer( NEWSSERVERINFO * pnsi )
{
   LPNEWSSERVER lpsrvrNew;
   
   INITIALISE_PTR(lpsrvrNew);
   if( fNewMemory( &lpsrvrNew, sizeof(NEWSSERVER) ) )
      {
      LPNEWSSERVER lpsrvr;

      /* Check whether this server already appears in the list.
       * If so, skip adding it again.
       */
      for( lpsrvr = lpsrvrFirst; lpsrvr; lpsrvr = lpsrvr->lpsrvrNext )
         if( strcmp( lpsrvr->nsi.szServerName, pnsi->szServerName ) == 0 )
            {
            FreeMemory( &lpsrvrNew );
            return( lpsrvr );
            }

      /* Nope, new entry, so fill out the
       * record and link it into the list.
       */
      if( NULL != lpsrvrFirst )
         lpsrvrFirst->lpsrvrPrev = lpsrvrNew;
      lpsrvrNew->lpsrvrPrev = NULL;
      lpsrvrNew->lpsrvrNext = lpsrvrFirst;
      lpsrvrNew->nsi = *pnsi;
      lpsrvrFirst = lpsrvrNew;
      }
   return( lpsrvrNew );
}
