/* MAILPRF.C - The Mail Settings dialog
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
#include <string.h>
#include "rules.h"
#include "mail.h"
#include "shrdob.h"
#include "intcomms.h"

#define  THIS_FILE   __FILE__

BOOL EXPORT CALLBACK MailGeneral( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL MailGeneral_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL MailGeneral_OnNotify( HWND, int, LPNMHDR );
void FASTCALL MailGeneral_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK MailHeaders( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL MailHeaders_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL MailHeaders_OnNotify( HWND, int, LPNMHDR );

BOOL EXPORT CALLBACK MailDelivery( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL MailDelivery_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL MailDelivery_OnNotify( HWND, int, LPNMHDR );
void FASTCALL MailDelivery_OnCommand( HWND, int, HWND, UINT );

void FASTCALL UsenetHeaders_OnCommand( HWND, int, HWND, UINT );
void FASTCALL UsenetHeaders_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
void FASTCALL UsenetHeaders_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );

static BOOL fMailHeadersLoaded = FALSE;      /* Mail custom headers not yet loaded */

CUSTOMHEADERS uMailHeaders[ MAX_HEADERS ];   /* Custom mail headers */
int cMailHeaders;                            /* Size of uMailHeaders array */

int nLastMailDialogPage = ODU_MAIL;
int nDeliveryPage;
int nLastPage;

UINT CALLBACK PrefsCallbackFunc( HWND, UINT, LPARAM );
int FASTCALL FillListWithMailboxes( HWND, int );

/* This function displays the Mail dialog.
 */
BOOL FASTCALL MailDialog( HWND hwnd, int nPage )
{
   AMPROPSHEETPAGE psp[ 4 ];
   AMPROPSHEETHEADER psh;
   int cPages;

   cPages = 0;
   psp[ ODU_MAIL ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ ODU_MAIL ].dwFlags = PSP_USETITLE;
   psp[ ODU_MAIL ].hInstance = hInst;
   psp[ ODU_MAIL ].pszTemplate = MAKEINTRESOURCE( IDDLG_PREF_MAIL );
   psp[ ODU_MAIL ].pszIcon = NULL;
   psp[ ODU_MAIL ].pfnDlgProc = MailGeneral;
   psp[ ODU_MAIL ].pszTitle = "General";
   psp[ ODU_MAIL ].lParam = 0L;
   ++cPages;

   psp[ ODU_MAILHEADERS ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ ODU_MAILHEADERS ].dwFlags = PSP_USETITLE;
   psp[ ODU_MAILHEADERS ].hInstance = hInst;
   psp[ ODU_MAILHEADERS ].pszTemplate = MAKEINTRESOURCE( IDDLG_NEWS_HEADERS );
   psp[ ODU_MAILHEADERS ].pszIcon = NULL;
   psp[ ODU_MAILHEADERS ].pfnDlgProc = MailHeaders;
   psp[ ODU_MAILHEADERS ].pszTitle = "Headers";
   psp[ ODU_MAILHEADERS ].lParam = 0L;
   ++cPages;

   if( fUseInternet )
      {
      nDeliveryPage = cPages;
      psp[ nDeliveryPage ].dwSize = sizeof( AMPROPSHEETPAGE );
      psp[ nDeliveryPage ].dwFlags = PSP_USETITLE;
      psp[ nDeliveryPage ].hInstance = hInst;
      psp[ nDeliveryPage ].pszTemplate = MAKEINTRESOURCE( IDDLG_PREF_DELIVERY );
      psp[ nDeliveryPage ].pszIcon = NULL;
      psp[ nDeliveryPage ].pfnDlgProc = MailDelivery;
      psp[ nDeliveryPage ].pszTitle = "Delivery";
      psp[ nDeliveryPage ].lParam = 0L;
      ++cPages;
      }

   ASSERT( cPages <= (sizeof( psp ) / sizeof( AMPROPSHEETPAGE )) );
   psh.dwSize = sizeof( AMPROPSHEETHEADER );
   psh.dwFlags = PSH_PROPSHEETPAGE|PSH_USECALLBACK|PSH_HASHELP;
   if( fMultiLineTabs )
      psh.dwFlags |= PSH_MULTILINETABS;
   psh.hwndParent = hwnd;
   psh.hInstance = hInst;
   psh.pszIcon = NULL;
   psh.pszCaption = "Mail";
   psh.nPages = cPages;
   psh.nStartPage = nLastMailDialogPage;
   psh.pfnCallback = PrefsCallbackFunc;
   psh.ppsp = (LPCAMPROPSHEETPAGE)&psp;
   nLastPage = nPage;
   return( Amctl_PropertySheet(&psh ) != -1 );
}

/* This function is called as soon as the property sheet has
 * finished initialising. We raise a AE_PREFSDIALOG event to allow
 * addons to attach their own property pages.
 */
UINT CALLBACK EXPORT PrefsCallbackFunc( HWND hwnd, UINT uMsg, LPARAM lParam )
{
   if( PSCB_INITIALIZED == uMsg )
      Amuser_CallRegistered( AE_PREFSDIALOG, (LPARAM)(LPSTR)hwnd, 0L );
   PropSheet_SetCurSel( hwnd, 0L, nLastPage );
   return( 0 );
}

/* This function handles the dialog box messages passed to the UsenetSettings
 * dialog.
 */
BOOL EXPORT CALLBACK MailHeaders( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, MailHeaders_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, UsenetHeaders_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_MEASUREITEM, UsenetHeaders_OnMeasureItem );
      HANDLE_DLGMSG( hwnd, WM_DRAWITEM, UsenetHeaders_OnDrawItem );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, MailHeaders_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL MailHeaders_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   register int c;
   HWND hwndList;

   /* Ensure headers are loaded.
    */
   LoadMailHeaders();

   /* Set the edit field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_TYPE ), LEN_RFCTYPE );
   Edit_LimitText( GetDlgItem( hwnd, IDD_SETTING ), 79 );

   /* Fill the list box with the existing Usenet headers.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   for( c = 0; c < cMailHeaders; ++c )
      {
      wsprintf( lpTmpBuf, "%s\t%s", (LPSTR)uMailHeaders[ c ].szType, (LPSTR)uMailHeaders[ c ].szValue );
      ListBox_AddString( hwndList, lpTmpBuf );
      }
   EnableControl( hwnd, IDD_REMOVE, FALSE );
   EnableControl( hwnd, IDD_ADD, FALSE );
   return( TRUE );
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL MailHeaders_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsMAIL_HEADERS );
         break;

      case PSN_SETACTIVE:
         nLastMailDialogPage = ODU_MAILHEADERS;
         break;

      case PSN_APPLY: {
         register int c;
         HWND hwndList;
         char sz[ 20 ];

         /* Save the Mail folder.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         for( c = 0; c < ListBox_GetCount( hwndList ); ++c )
            {
            char * psz;

            ListBox_GetText( hwndList, c, lpTmpBuf );
            psz = strchr( lpTmpBuf, '\t' );
            *psz++ = '\0';
            strcpy( uMailHeaders[ c ].szType, lpTmpBuf );
            if( _strcmpi( uMailHeaders[ c ].szType, "X-Olr" ) == 0 )
               {
               fMessageBox( hwnd, 0, GS(IDS_STR1189), MB_OK|MB_ICONINFORMATION );
               return( PSNRET_INVALID_NOCHANGEPAGE );
               }
            strcpy( uMailHeaders[ c ].szValue, psz );
            wsprintf( lpTmpBuf, "%s:%s", (LPSTR)uMailHeaders[ c ].szType, (LPSTR)uMailHeaders[ c ].szValue );
            wsprintf( sz, "CustomHeader%d", c + 1 );
            Amuser_WritePPString( szMail, sz, lpTmpBuf );
            }
         while( cMailHeaders > c )
            {
            wsprintf( sz, "CustomHeader%d", cMailHeaders-- );
            Amuser_WritePPString( szMail, sz, NULL );
            }
         cMailHeaders = c;

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This function dispatches messages for the Incoming Mail page of the
 * Preferences dialog.
 */
BOOL EXPORT CALLBACK MailGeneral( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, MailGeneral_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, MailGeneral_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, MailGeneral_OnNotify );
      HANDLE_DLGMSG( hwnd, WM_MEASUREITEM, FolderCombo_OnMeasureItem );
      HANDLE_DLGMSG( hwnd, WM_DRAWITEM, FolderListCombo_OnDrawItem );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL MailGeneral_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndList;
   int index;
   PTL ptl;

   SetDlgItemInt( hwnd, IDD_EDIT, imWrapCol, FALSE ); // 2.56.2055
   Edit_LimitText( GetDlgItem( hwnd, IDD_EDIT ), 4 );

   /* Set incoming mail folder.
    */
   FillListWithMailboxes( hwnd, IDD_LIST );

   /* Highlight the incoming mail folder.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   ptl = GetPostmasterFolder();
   index = RealComboBox_FindItemData( hwndList, -1, (LPARAM)ptl );
   ComboBox_SetCurSel( hwndList, index );

   /* Set the options.
    */
   CheckDlgButton( hwnd, IDD_AUTOQUOTE, fAutoQuoteMail );
   SetDlgItemText( hwnd, IDD_QUOTEHEADER, szQuoteMailHeader );
   Edit_LimitText( GetDlgItem( hwnd, IDD_QUOTEHEADER ), sizeof(szQuoteMailHeader) - 1 );

   /* Set CC to sender option.
    */
   CheckDlgButton( hwnd, IDD_SENDMAIL, fCCMailToSender );
   CheckDlgButton( hwnd, IDD_RETHREADMAIL, fThreadMailBySubject );
   CheckDlgButton( hwnd, IDD_PROTECTMAIL, fProtectMailFolders );
   CheckDlgButton( hwnd, IDD_BLANKSUBJECTWARNING, fBlankSubjectWarning );
   CheckDlgButton( hwnd, IDD_GETMAILDETAILS, fGetMailDetails );
   CheckDlgButton( hwnd, IDD_NEWMAILALERT, fNewMailAlert );

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
void FASTCALL MailGeneral_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EDIT:
         if( codeNotify == EN_CHANGE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDM_QP_ADDRESS:    PutQuoteHeaderData( hwnd, "%a" ); break;
      case IDM_QP_FULLNAME:      PutQuoteHeaderData( hwnd, "%f" ); break;
      case IDM_QP_MESSAGEID:     PutQuoteHeaderData( hwnd, "%m" ); break;
      case IDM_QP_DATE:       PutQuoteHeaderData( hwnd, "%d" ); break;

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

      case IDD_GETMAILDETAILS:
      case IDD_SENDMAIL:
      case IDD_RETHREADMAIL:
      case IDD_PROTECTMAIL:
      case IDD_NEWMAILALERT:
      case IDD_BLANKSUBJECTWARNING:
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
LRESULT FASTCALL MailGeneral_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPREF_MAIL );
         break;

      case PSN_SETACTIVE:
         nLastMailDialogPage = ODU_MAIL;
         break;

      case PSN_APPLY: {
         char sz[ 256 ];
         HWND hwndList;
         int index;
         PTL ptl;
         LPVOID pData;

         if( !GetDlgInt( hwnd, IDD_EDIT, &imWrapCol, 40, 4000 ) ) // 2.56.2055
         {
            PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, ODU_MAIL ); // 2.56.2056
            return( PSNRET_INVALID_NOCHANGEPAGE );
         }

         /* Save the incoming mail folder.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         index = ComboBox_GetCurSel( hwndList );
         if( CB_ERR == index )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR1232), MB_OK|MB_ICONEXCLAMATION );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }
         pData = (LPVOID)ComboBox_GetItemData( hwndList, index );
         if( NULL == pData || !Amdb_IsTopicPtr( pData ) )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR1232), MB_OK|MB_ICONEXCLAMATION );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }

         Amuser_WritePPInt( szSettings, "WordWrapMail", imWrapCol ); // 2.56.2055

         ptl = (LPVOID)ComboBox_GetItemData( hwndList, index );
         WriteFolderPathname( sz, ptl );
         Amuser_WritePPString( szCIXIP, "Postmaster", sz );
         ptlPostmaster = ptl;

         /* Get options.
          */
         fAutoQuoteMail = IsDlgButtonChecked( hwnd, IDD_AUTOQUOTE );
         Edit_GetText( GetDlgItem( hwnd, IDD_QUOTEHEADER ), szQuoteMailHeader, sizeof(szQuoteMailHeader) );

         /* Save new options.
          */
         Amuser_WritePPInt( szSettings, "AutoQuoteMail", fAutoQuoteMail );
         Amuser_WritePPString( szSettings, "QuoteMailHeader", szQuoteMailHeader );

         /* Get CC option.
          */
         fCCMailToSender = IsDlgButtonChecked( hwnd, IDD_SENDMAIL );
         fThreadMailBySubject = IsDlgButtonChecked( hwnd, IDD_RETHREADMAIL );
         fProtectMailFolders = IsDlgButtonChecked( hwnd, IDD_PROTECTMAIL );
         fBlankSubjectWarning = IsDlgButtonChecked( hwnd, IDD_BLANKSUBJECTWARNING );
         fGetMailDetails = IsDlgButtonChecked( hwnd, IDD_GETMAILDETAILS );
         fNewMailAlert = IsDlgButtonChecked( hwnd, IDD_NEWMAILALERT );
         Amuser_WritePPInt( szSettings, "GetMailDetails", fGetMailDetails );
         Amuser_WritePPInt( szSettings, "CCToSender", fCCMailToSender );
         Amuser_WritePPInt( szSettings, "ThreadMailBySubject", fThreadMailBySubject );
         Amuser_WritePPInt( szSettings, "ProtectMailFolders", fProtectMailFolders );
         Amuser_WritePPInt( szSettings, "BlankSubjectWarning", fBlankSubjectWarning );
         Amuser_WritePPInt( szSettings, "NewMailAlert", fNewMailAlert );

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

/* This function dispatches messages for the Delivery page of the
 * Preferences dialog.
 */
BOOL EXPORT CALLBACK MailDelivery( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, MailDelivery_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, MailDelivery_OnNotify );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, MailDelivery_OnCommand );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL MailDelivery_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndEdit;

   /* Set the templates
   char sz[ 100 ];
   GetDlgItemText( hwnd, IDD_SENDVIAIP, sz, sizeof(sz) );
   wsprintf( lpTmpBuf, sz, szProvider );
   SetDlgItemText( hwnd, IDD_SENDVIAIP, lpTmpBuf );
   GetDlgItemText( hwnd, IDD_PAD1, sz, sizeof(sz) );
   wsprintf( lpTmpBuf, sz, szProvider );
   SetDlgItemText( hwnd, IDD_PAD1, lpTmpBuf );
    */

   /* Set the default mail domain.
    */
   VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_DEFMAILDOMAIN ) );
   Edit_SetText( hwndEdit, szDefMailDomain );
   Edit_LimitText( hwndEdit, sizeof(szDefMailDomain) - 1 );

   /* Set the options.
    */
   CheckDlgButton( hwnd, IDD_SENDVIACIX, nMailDelivery == MAIL_DELIVER_CIX  );
   CheckDlgButton( hwnd, IDD_SENDVIAIP, nMailDelivery == MAIL_DELIVER_IP );
   CheckDlgButton( hwnd, IDD_SENDAI, nMailDelivery == MAIL_DELIVER_AI );

   if( !fUseCIX )
   {
      ShowWindow( GetDlgItem( hwnd, IDD_SENDVIACIX ), SW_HIDE );
      ShowWindow( GetDlgItem( hwnd, IDD_SENDAI ), SW_HIDE );
      ShowWindow( GetDlgItem( hwnd, IDD_SENDVIAIP ), SW_HIDE );
      ShowWindow( GetDlgItem( hwnd, IDD_PAD1 ), SW_HIDE );
      ShowWindow( GetDlgItem( hwnd, IDD_DELIVERYSTATIC ), SW_HIDE );
   }


   
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL MailDelivery_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch (id)
   {
      case IDD_SENDVIACIX:
      case IDD_SENDVIAIP:
      case IDD_SENDAI:
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
      
      case IDD_DEFMAILDOMAIN:
         if( codeNotify == EN_CHANGE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

   }
}
/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL MailDelivery_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPREF_DELIVERY );
         break;

      case PSN_SETACTIVE:
         nLastMailDialogPage = nDeliveryPage;
         break;

      case PSN_APPLY: {
         HWND hwndEdit;

         /* Get and save the new path.
          */
         if( IsDlgButtonChecked( hwnd, IDD_SENDVIACIX ) )
            nMailDelivery = MAIL_DELIVER_CIX;
         if( IsDlgButtonChecked( hwnd, IDD_SENDVIAIP ) || ( fUseInternet && !fUseCIX ) )
            nMailDelivery = MAIL_DELIVER_IP;
         if( IsDlgButtonChecked( hwnd, IDD_SENDAI ) )
            nMailDelivery = MAIL_DELIVER_AI;
         Amuser_WritePPInt( szSettings, "MailDelivery", nMailDelivery );

         /* Get the default mail domain.
          */
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_DEFMAILDOMAIN ) );
         Edit_GetText( hwndEdit, szDefMailDomain, sizeof(szDefMailDomain) );
         Amuser_WritePPString( szSettings, "DefaultMailDomain", szDefMailDomain );

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This function loads the Mail custom headers if they have
 * not already been loaded.
 */
void FASTCALL LoadMailHeaders( void )
{
   if( !fMailHeadersLoaded )
      {
      register int c;

      /* Get Mail header formats
       */
      cMailHeaders = 0;
      for( c = 0; c < MAX_HEADERS; ++c )
         {
         char sz[ 20 ];
         register int d;
         char * psz;

         wsprintf( sz, "CustomHeader%d", c + 1 );
         if( !Amuser_GetPPString( szMail, sz, "", lpTmpBuf, 128 ) )
            break;
         psz = lpTmpBuf;
         for( d = 0; *psz && *psz != ':'; ++d )
            {
            if( d < sizeof( uMailHeaders->szType ) )
               {
               if( *psz == ' ' )
                  uMailHeaders[ cMailHeaders ].szType[ d ] = '-';
               else
                  uMailHeaders[ cMailHeaders ].szType[ d ] = *psz;
               }
            ++psz;
            }
         uMailHeaders[ cMailHeaders ].szType[ d ] = '\0';
         if( _strcmpi( uMailHeaders[ cMailHeaders ].szType, "X-Olr" ) != 0 && _strcmpi( uMailHeaders[ cMailHeaders ].szType, "X-Ameol-Version" ) != 0 )
            {
            ++psz;
            for( d = 0; *psz; ++d )
               {
               if( d < sizeof( uMailHeaders->szValue ) )
                  uMailHeaders[ cMailHeaders ].szValue[ d ] = *psz;
               ++psz;
               }
            uMailHeaders[ cMailHeaders ].szValue[ d ] = '\0';
            ++cMailHeaders;
            }
         }

      /* If no headers, create defaults
       */
      if( cMailHeaders == 0 && fFirstTimeThisUser )
         {
         strcpy( uMailHeaders[ cMailHeaders ].szType, "X-URL" );
         strcpy( uMailHeaders[ cMailHeaders ].szValue, "http://www.ameol.com" );
         ++cMailHeaders;
         strcpy( uMailHeaders[ cMailHeaders ].szType, "X-Mail-Software" );
         strcpy( uMailHeaders[ cMailHeaders ].szValue, szAppName );
         ++cMailHeaders;
         }
      if( !( fCompatFlag & COMF_NO_X_AMEOL ) )
         {
         char szWinVer[ 100 ];            /* Windows version */

         if (!GetOSDisplayString(szWinVer))
            strcpy(szWinVer, "Unknown");

         strcpy( uMailHeaders[ cMailHeaders ].szType, "X-Ameol-Version" );
         wsprintf( lpTmpBuf, "%u.%2.2u.%3.3u, %s", amv.nMaxima, amv.nMinima, amv.nBuild, szWinVer );
         strcpy( uMailHeaders[ cMailHeaders ].szValue, lpTmpBuf );
         ++cMailHeaders;
         }
      fMailHeadersLoaded = TRUE;
      }
}
