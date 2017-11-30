/* SETUPWIZ.C - The Ameol for Internet Setup Wizard
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
#include <stdio.h>
#include <string.h>
#include "amlib.h"
#include "intcomms.h"
#include "cix.h"
#include "admin.h"
#include "phone.h"
#include "news.h"

#define  THIS_FILE   __FILE__

#define  HOST_OPTIONAL     0
#define  HOST_NONE         1
#define  HOST_ALWAYS       2

static HFONT hTitleFont;                  /* Handle of title font */
static BOOL fDefault;                     /* TRUE if we've selected the default provider */
static SERIALCOMM FAR sc;                 /* Default connection card for serial port */
static IPCOMM FAR ic;                     /* Default connection card for IP port */
static COMMDESCRIPTOR FAR cd;             /* Comm descriptor */
static int nHostname;                     /* Does ISP require a hostname? */
static int idbSetupPic;                   /* ID of setup wizard logo */
static int nSetupAccountType;             /* Account type chosen during setup */
static BOOL fAccountTypeChanged = FALSE;     /* TRUE if account type changed */
static BOOL fOldUseCIX;                   /* The old UseCIX setting */
static BOOL fUseAmeol2ForIPMail;          /* TRUE if we use Ameol2 for internet e-mail */
static BOOL fOldCIXViaInternet;

static char szEmail1[ 25 ];
static char szEmail2[ ( sizeof (szMailAddress) - sizeof (szEmail1) - 2) ];

static BOOL fWin95;                       /* TRUE if host OS is Windows 95 or Windows NT 4.0 */

#ifdef CREATE_MODEM_CARDS
const char szDefModemConf[] = "Dial-Up Forums (Ameol)";     /* Default modem configuration name */
const char szDefTAPIConf[] = "Dial-Up Forums (Win95/NT)";      /* Default telephony modem name */
#endif

const char szDefICAIPConf[] = "Internet Forums";            /* Default internet configuration name */
const char szDefNAIPConf[] = "Net Access";                     /* Default internet configuration name */
BOOL fSetForward = FALSE;

void FASTCALL SetupAccountType( int );

BOOL EXPORT CALLBACK SetupWizard_P1( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL SetupWizard_P1_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL SetupWizard_P1_OnNotify( HWND, int, LPNMHDR );

BOOL EXPORT CALLBACK SetupWizard_P2( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL SetupWizard_P2_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL SetupWizard_P2_OnNotify( HWND, int, LPNMHDR );

BOOL EXPORT CALLBACK SetupWizard_P2_1( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL SetupWizard_P2_1_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL SetupWizard_P2_1_OnNotify( HWND, int, LPNMHDR );

BOOL EXPORT CALLBACK SetupWizard_P3( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL SetupWizard_P3_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL SetupWizard_P3_OnNotify( HWND, int, LPNMHDR );
void FASTCALL SetupWizard_P3_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK SetupWizard_P4( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL SetupWizard_P4_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL SetupWizard_P4_OnNotify( HWND, int, LPNMHDR );
void FASTCALL SetupWizard_P4_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK SetupWizard_P5( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL SetupWizard_P5_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL SetupWizard_P5_OnNotify( HWND, int, LPNMHDR );
void FASTCALL SetupWizard_P5_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK SetupWizard_P6( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL SetupWizard_P6_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL SetupWizard_P6_OnNotify( HWND, int, LPNMHDR );
void FASTCALL SetupWizard_P6_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK SetupWizard_P6_1( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL SetupWizard_P6_1_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL SetupWizard_P6_1_OnNotify( HWND, int, LPNMHDR );
void FASTCALL SetupWizard_P6_1_OnCommand( HWND, int, HWND, UINT );
void FASTCALL SetupWizard_P6_1_EnableButtons( HWND );

BOOL EXPORT CALLBACK SetupWizard_P7( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL SetupWizard_P7_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL SetupWizard_P7_OnNotify( HWND, int, LPNMHDR );

BOOL EXPORT CALLBACK SetupWizard_P8( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL SetupWizard_P8_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL SetupWizard_P8_OnNotify( HWND, int, LPNMHDR );
void FASTCALL SetupWizard_P8_OnCommand( HWND, int, HWND, UINT );

void FASTCALL CopyRegistry( HKEY, char *, HKEY, char * );

/* This function creates a new connection entry.
 */
BOOL FASTCALL SetupWizard( HWND hwnd )
{
   AMPROPSHEETPAGE psp[ 19 ];
   AMPROPSHEETHEADER psh;
   int cPage;
   BOOL fOk;

   /* Keep index of page number as we go.
    */
   cPage = 0;

   /* Some pages are different between Windows NT
    * and Windows 95.
    */
   fWin95 = wWinVer >= 0x035F;

   psp[ cPage ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ cPage ].dwFlags = PSP_USETITLE;
   psp[ cPage ].hInstance = hInst;
   psp[ cPage ].pszTemplate = MAKEINTRESOURCE( IDDLG_SETUPWIZ_P1 );
   psp[ cPage ].pszIcon = NULL;
   psp[ cPage ].pfnDlgProc = SetupWizard_P1;
   psp[ cPage ].pszTitle = "Setup Wizard";
   psp[ cPage ].lParam = 0L;
   ++cPage;

   psp[ cPage ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ cPage ].dwFlags = PSP_USETITLE;
   psp[ cPage ].hInstance = hInst;
   psp[ cPage ].pszTemplate = MAKEINTRESOURCE( IDDLG_SETUPWIZ_P2 );
   psp[ cPage ].pszIcon = NULL;
   psp[ cPage ].pfnDlgProc = SetupWizard_P2;
   psp[ cPage ].pszTitle = "Choose Your Setup";
   psp[ cPage ].lParam = 0L;
   ++cPage;

   psp[ cPage ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ cPage ].dwFlags = PSP_USETITLE;
   psp[ cPage ].hInstance = hInst;
   psp[ cPage ].pszTemplate = MAKEINTRESOURCE( IDDLG_SETUPWIZ_P2_1 );
   psp[ cPage ].pszIcon = NULL;
   psp[ cPage ].pfnDlgProc = SetupWizard_P2_1;
   psp[ cPage ].pszTitle = "CIXReader";
   psp[ cPage ].lParam = 0L;
   ++cPage;

   psp[ cPage ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ cPage ].dwFlags = PSP_USETITLE;
   psp[ cPage ].hInstance = hInst;
   psp[ cPage ].pszTemplate = MAKEINTRESOURCE( IDDLG_SETUPWIZ_P3 );
   psp[ cPage ].pszIcon = NULL;
   psp[ cPage ].pfnDlgProc = SetupWizard_P3;
   psp[ cPage ].pszTitle = "Configure CIX Forums";
   psp[ cPage ].lParam = 0L;
   ++cPage;

   psp[ cPage ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ cPage ].dwFlags = PSP_USETITLE;
   psp[ cPage ].hInstance = hInst;
   psp[ cPage ].pszTemplate = MAKEINTRESOURCE( IDDLG_SETUPWIZ_P4 );
   psp[ cPage ].pszIcon = NULL;
   psp[ cPage ].pfnDlgProc = SetupWizard_P4;
   psp[ cPage ].pszTitle = "Enter Your Internet Configuration";
   psp[ cPage ].lParam = 0L;
   ++cPage;

   psp[ cPage ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ cPage ].dwFlags = PSP_USETITLE;
   psp[ cPage ].hInstance = hInst;
   psp[ cPage ].pszTemplate = MAKEINTRESOURCE( IDDLG_SETUPWIZ_P5 );
   psp[ cPage ].pszIcon = NULL;
   psp[ cPage ].pfnDlgProc = SetupWizard_P5;
   psp[ cPage ].pszTitle = "Enter Your Details";
   psp[ cPage ].lParam = 0L;
   ++cPage;

   psp[ cPage ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ cPage ].dwFlags = PSP_USETITLE;
   psp[ cPage ].hInstance = hInst;
   psp[ cPage ].pszTemplate = MAKEINTRESOURCE( IDDLG_SETUPWIZ_P6 );
   psp[ cPage ].pszIcon = NULL;
   psp[ cPage ].pfnDlgProc = SetupWizard_P6;
   psp[ cPage ].pszTitle = "Enter Your POP3 Details";
   psp[ cPage ].lParam = 0L;
   ++cPage;

   psp[ cPage ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ cPage ].dwFlags = PSP_USETITLE;
   psp[ cPage ].hInstance = hInst;
   psp[ cPage ].pszTemplate = MAKEINTRESOURCE( IDDLG_SETUPWIZ_P6_1 );
   psp[ cPage ].pszIcon = NULL;
   psp[ cPage ].pfnDlgProc = SetupWizard_P6_1;
   psp[ cPage ].pszTitle = "Enter Your SMTP Details";
   psp[ cPage ].lParam = 0L;
   ++cPage;

   psp[ cPage ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ cPage ].dwFlags = PSP_USETITLE;
   psp[ cPage ].hInstance = hInst;
   psp[ cPage ].pszTemplate = MAKEINTRESOURCE( IDDLG_SETUPWIZ_P7 );
   psp[ cPage ].pszIcon = NULL;
   psp[ cPage ].pfnDlgProc = SetupWizard_P7;
   psp[ cPage ].pszTitle = "Default Email";
   psp[ cPage ].lParam = 0L;
   ++cPage;

   psp[ cPage ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ cPage ].dwFlags = PSP_USETITLE;
   psp[ cPage ].hInstance = hInst;
   psp[ cPage ].pszTemplate = MAKEINTRESOURCE( IDDLG_SETUPWIZ_P8 );
   psp[ cPage ].pszIcon = NULL;
   psp[ cPage ].pfnDlgProc = SetupWizard_P8;
   psp[ cPage ].pszTitle = "Setup Complete";
   psp[ cPage ].lParam = 0L;
   ++cPage;

   ASSERT( cPage <= (sizeof( psp ) / sizeof( AMPROPSHEETPAGE )) );
   psh.dwSize = sizeof( AMPROPSHEETHEADER );
   psh.dwFlags = PSH_PROPSHEETPAGE|PSH_WIZARD|PSH_HASHELP;
   psh.hwndParent = hwnd;
   psh.hInstance = hInst;
   psh.pszIcon = NULL;
   psh.pszCaption = "";
   psh.nPages = cPage;
   psh.nStartPage = 0;
   psh.ppsp = (LPCAMPROPSHEETPAGE)&psp;

   /* Initialise some variables.
    */
   fDefault = TRUE;
   fUseCIX = TRUE;

   /* Get default settings.
    */
   fOldUseCIX = Amuser_GetPPInt( szCIX, "UseCIX", FALSE );
   Amuser_GetPPString( szCIX, "Nickname", "", szCIXNickname, sizeof(szCIXNickname) );
   Amuser_GetPPString( szCIXIP, "MailFullName", "", szFullName, sizeof(szFullName) );
   Amuser_GetPPPassword( szCIX, "Password", "", szCIXPassword, LEN_PASSWORD );
   Amuser_GetPPPassword( szCIXIP, "MailPassword", "", szMailPassword, LEN_PASSWORD );
   fOldCIXViaInternet = Amuser_GetPPInt( szCIX, "CIXviaInternet", TRUE );

   /* Set the default connection cards.
    */
   strcpy( sc.md.szModemName, "" );
   strcpy( sc.szPhone, szCIXPhoneNumber );
   strcpy( sc.szMercury, "" );
   strcpy( sc.szPort, "COM1" );
   sc.wRetry = 5;
   sc.wDialType = DTYPE_TONE;
   sc.fCheckCTS = TRUE;
   sc.fCheckDCD = TRUE;

   /* Get the full name.
    */
   if( strlen( szFullName ) == 0 )
      About_GetUserName( szFullName, sizeof(szFullName) );

   /* Is RAS or Dial-Up Networking installed?
    */
   fUseAmeol2ForIPMail = FALSE;

   /* Default provider.
    */
   strcpy( szProvider, "Internet" );

   /* Create the font we'll use for the title of the wizard
    * pages.
    */
   idbSetupPic = fOldLogo ? IDB_SETUPWIZ_PIX : IDB_SETUPWIZ_PIX_NEW;
   hTitleFont = CreateFont( 28, 0, 0, 0, FW_BOLD, FALSE, FALSE,
                           FALSE, ANSI_CHARSET, OUT_CHARACTER_PRECIS,
                           CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
                           VARIABLE_PITCH | FF_SWISS, "Times New Roman" );

   /* Start the wizard running.
    */
   fOk = Amctl_PropertySheet( &psh ) != -1;

   /* Clean up before we return.
    */
   DeleteFont( hTitleFont );
   return( fOk );
}

/* This function handles a page of the Setup Wizard
 */
BOOL EXPORT CALLBACK SetupWizard_P1( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, SetupWizard_P1_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, SetupWizard_P1_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL SetupWizard_P1_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   /* Set the picture bitmap.
    */
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_PIX ), hInst, idbSetupPic );
   SetForegroundWindow(hwnd);

   /* Set the display attributes.
    */
   SetWindowFont( GetDlgItem( hwnd, IDD_TITLE ), hTitleFont, FALSE );
   return( TRUE );
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL SetupWizard_P1_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSETUPWIZ_P1 );
         break;

      case PSN_SETACTIVE:
         PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT );
         break;

      case PSN_WIZNEXT:
         return( IDDLG_SETUPWIZ_P2 );
   }
   return( FALSE );
}

/* This function handles a page of the Setup Wizard
 */
BOOL EXPORT CALLBACK SetupWizard_P2( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, SetupWizard_P2_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, SetupWizard_P2_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL SetupWizard_P2_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   /* Choose CIX Forums plus Email and Newsgroups by default.
    */
   CheckDlgButton( hwnd, IDD_CONFERENCING_PLUS_INTERNET, TRUE );

   /* Set the picture bitmap.
    */
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_PIX ), hInst, idbSetupPic );

   /* Set the display attributes.
    */
   SetWindowFont( GetDlgItem( hwnd, IDD_TITLE ), hTitleFont, FALSE );
   return( TRUE );
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL SetupWizard_P2_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSETUPWIZ_P2 );
         break;

      case PSN_SETACTIVE:
         PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_BACK|PSWIZB_NEXT );
         break;

      case PSN_WIZBACK:
         return( IDDLG_SETUPWIZ_P1 );

      case PSN_WIZNEXT:
         if( IsDlgButtonChecked( hwnd, IDD_CONFERENCING ) )
         {
            SetupAccountType( ACTYP_CONFERENCING );
            return( IDDLG_SETUPWIZ_P2_1 );
         }
         SetupAccountType( ACTYP_CONFERENCING_AND_INTERNET );
         return( IDDLG_SETUPWIZ_P3 );
      }
   return( FALSE );
}

/* This function handles a page of the Setup Wizard
 */
BOOL EXPORT CALLBACK SetupWizard_P2_1( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, SetupWizard_P2_1_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, SetupWizard_P2_1_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL SetupWizard_P2_1_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   /* Set the picture bitmap.
    */
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_PIX ), hInst, idbSetupPic );

   /* Set the display attributes.
    */
   SetWindowFont( GetDlgItem( hwnd, IDD_TITLE ), hTitleFont, FALSE );
   return( TRUE );
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL SetupWizard_P2_1_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   UINT uRetCode;
   switch( lpnmhdr->code )
      {
      case NM_CLICK:
      case NM_RETURN:
         if( ( uRetCode = (UINT)ShellExecute( hwnd, NULL, "https://www.cix.uk/forums/accessing-forums", NULL, NULL, SW_SHOW ) ) < 32 )
         {
            DisplayShellExecuteError( hwnd, szBrowser, uRetCode );
         }
         break;

      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSETUPWIZ_P2 );
         break;

      case PSN_SETACTIVE:
         PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_BACK|PSWIZB_NEXT );
         break;

      case PSN_WIZBACK:
         return( IDDLG_SETUPWIZ_P2 );

      case PSN_WIZNEXT:
         return( IDDLG_SETUPWIZ_P3 );
      }
   return( FALSE );
}

/* This function initialises the setup wizard for a particular
 * account type.
 */
void FASTCALL SetupAccountType( int nType )
{
   switch( nSetupAccountType = nType )
      {
      case ACTYP_CONFERENCING_AND_INTERNET:
         fUseInternet = TRUE;
         nHostname = HOST_NONE;
         strcpy( szNewsServer, "" );
         strcpy( szSMTPMailServer, szDefMail );
         strcpy( szMailServer, szDefMail );
         strcpy( szDomain, "compulink.co.uk" );
         strcpy( szAccountName, GS(IDS_STR1151) );
         break;

      case ACTYP_CONFERENCING:
         fUseInternet = FALSE;
         nHostname = HOST_NONE;
         strcpy( szAccountName, GS(IDS_STR1150) );
         strcpy( szNewsServer, "" );
         strcpy( szSMTPMailServer, szDefMail );
         strcpy( szMailServer, szDefMail );
         strcpy( szDomain, "cix.co.uk" );
         wsprintf( szMailAddress, "%s@cix.co.uk", szCIXNickname );
         break;
      }
   fAccountTypeChanged = TRUE;
}

/* This function handles a page of the Setup Wizard
 */
BOOL EXPORT CALLBACK SetupWizard_P3( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, SetupWizard_P3_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, SetupWizard_P3_OnNotify );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, SetupWizard_P3_OnCommand );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL SetupWizard_P3_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   char szPassword[ 40 ];

   /* Enable the have CIX nickname and password option.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_USERNAME ), 14 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_PASSWORD ), 14 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_PASSWORD3 ), 14 );

   /* Set the current nickname and password.
    */
   SetDlgItemText( hwnd, IDD_USERNAME, szCIXNickname );
   memcpy( szPassword, szCIXPassword, sizeof(szCIXPassword) );
   Amuser_Decrypt( szPassword, rgEncodeKey );
   SetDlgItemText( hwnd, IDD_PASSWORD, szPassword );
   SetDlgItemText( hwnd, IDD_PASSWORD3, szPassword );

   /* Set the picture bitmap.
    */
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_PIX ), hInst, idbSetupPic );

   /* Set the display attributes.
    */
   SetWindowFont( GetDlgItem( hwnd, IDD_TITLE ), hTitleFont, FALSE );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL SetupWizard_P3_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_USERNAME:
      case IDD_PASSWORD:
      case IDD_PASSWORD3:
         if( codeNotify == EN_CHANGE && IsWindowVisible( hwndCtl ) )
            {
            if( Edit_GetTextLength( GetDlgItem( hwnd, IDD_USERNAME ) ) > 0 &&
                Edit_GetTextLength( GetDlgItem( hwnd, IDD_PASSWORD ) ) > 0 &&
                Edit_GetTextLength( GetDlgItem( hwnd, IDD_PASSWORD3 ) ) )
               PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT|PSWIZB_BACK );
            else
               PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_BACK );
            }
         break;
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL SetupWizard_P3_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSETUPWIZ_P3 );
         break;

      case PSN_SETACTIVE:
         SetDlgItemText( hwnd, IDD_FULLNAME, szFullName );
         SetDlgItemText( hwnd, IDD_PAD1, GS(IDS_STR1180) );
         if( Edit_GetTextLength( GetDlgItem( hwnd, IDD_USERNAME ) ) > 0 &&
             Edit_GetTextLength( GetDlgItem( hwnd, IDD_PASSWORD ) ) > 0 &&
             Edit_GetTextLength( GetDlgItem( hwnd, IDD_PASSWORD3 ) ) )
            PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT|PSWIZB_BACK );
         else
            PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_BACK );
         break;

      case PSN_WIZBACK:
         return( (nSetupAccountType == ACTYP_CONFERENCING) ? IDDLG_SETUPWIZ_P2_1 : IDDLG_SETUPWIZ_P2 );

      case PSN_WIZNEXT: {
         char szPassword3[ 40 ];

         GetDlgItemText( hwnd, IDD_USERNAME, szCIXNickname, 16 );
         if( !IsValidCIXNickname( szCIXNickname ) )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR904), MB_OK|MB_ICONINFORMATION );
            return( -1 );
            }
         GetDlgItemText( hwnd, IDD_PASSWORD, szCIXPassword, 16 );
         GetDlgItemText( hwnd, IDD_PASSWORD3, szPassword3, 16 );
         if( !IsValidCIXPassword( szCIXPassword ) )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR905), MB_OK|MB_ICONINFORMATION );
            return( -1 );
            }
         if( strcmp( szCIXPassword, szPassword3 ) != 0 )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR264), MB_OK|MB_ICONINFORMATION );
            return( -1 );
            }
         Amuser_Encrypt( szCIXPassword, rgEncodeKey );

         rdDef.fUseRAS = FALSE;

         // need to set default POP3, News details etc
         strcpy( szAccountName, GS(IDS_STR1211) );
         strcpy( szNewsServer, "" );
         strcpy( szSMTPMailServer, "mail.cix.co.uk" );
         strcpy( szMailServer, "mail.cix.co.uk" );
         strcpy( szDomain, "cix.co.uk" );
         GetDlgItemText( hwnd, IDD_FULLNAME, szFullName, sizeof( szFullName ) );
         GetDlgItemText( hwnd, IDD_USERNAME, szMailLogin, sizeof( szMailLogin ) );
         wsprintf( szMailAddress, "%s@cix.co.uk", szMailLogin );

         GetDlgItemText( hwnd, IDD_PASSWORD, szMailPassword, sizeof( szMailPassword ) );
         GetDlgItemText( hwnd, IDD_PASSWORD3, szEmail2, sizeof( szEmail2 ) );

         if ( strcmp(szMailPassword,szEmail2)==0 )
         {
            strcpy(szCIXPassword, szMailPassword );
            Amuser_Encrypt( szMailPassword, rgEncodeKey );
            Amuser_Encrypt( szCIXPassword, rgEncodeKey );
         }
         else
         {
            fMessageBox( hwnd, 0, GS(IDS_STR264), MB_OK|MB_ICONINFORMATION );
            return( -1 );
         }
         if ( nSetupAccountType == ACTYP_CONFERENCING_AND_INTERNET )
            return( IDDLG_SETUPWIZ_P4 );

         return( IDDLG_SETUPWIZ_P8);
      }
   }
   return( FALSE );
}

/* This function handles a page of the Setup Wizard
 */
BOOL EXPORT CALLBACK SetupWizard_P4( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, SetupWizard_P4_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, SetupWizard_P4_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, SetupWizard_P4_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL SetupWizard_P4_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   /* Set edit limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_POP3SERVER ), sizeof(szMailServer) - 1 );
   //Edit_LimitText( GetDlgItem( hwnd, IDD_NEWSSERVER ), sizeof(szNewsServer) - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_MAILSERVER ), sizeof(szSMTPMailServer) - 1 );

   /* Set the picture bitmap.
    */
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_PIX ), hInst, idbSetupPic );

   /* Set the display attributes.
    */
   SetWindowFont( GetDlgItem( hwnd, IDD_TITLE ), hTitleFont, FALSE );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL SetupWizard_P4_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_POP3SERVER:
      case IDD_MAILSERVER:
      //case IDD_NEWSSERVER:
         if( codeNotify == EN_CHANGE )
            {
            if( Edit_GetTextLength( GetDlgItem( hwnd, IDD_MAILSERVER ) ) > 0 &&
                Edit_GetTextLength( GetDlgItem( hwnd, IDD_POP3SERVER ) ) > 0)
               PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT|PSWIZB_BACK );
            else
               PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_BACK );
            }
         break;
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL SetupWizard_P4_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSETUPWIZ_P4 );
         break;

      case PSN_SETACTIVE:
         if( Edit_GetTextLength( GetDlgItem( hwnd, IDD_MAILSERVER ) ) > 0 &&
             Edit_GetTextLength( GetDlgItem( hwnd, IDD_POP3SERVER ) ) > 0)
            PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT|PSWIZB_BACK );
         else
            PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_BACK );
         break;

      case PSN_WIZBACK:
         return( IDDLG_SETUPWIZ_P3 );

      case PSN_WIZNEXT:
         GetDlgItemText( hwnd, IDD_POP3SERVER, szMailServer, sizeof(szMailServer) );
         GetDlgItemText( hwnd, IDD_MAILSERVER, szSMTPMailServer, sizeof(szSMTPMailServer) );
         //GetDlgItemText( hwnd, IDD_NEWSSERVER, szNewsServer, sizeof(szNewsServer) );
         return( IDDLG_SETUPWIZ_P5 );
      }
   return( FALSE );
}

/* This function handles a page of the Setup Wizard
 */
BOOL EXPORT CALLBACK SetupWizard_P5( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, SetupWizard_P5_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, SetupWizard_P5_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, SetupWizard_P5_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL SetupWizard_P5_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndEdit;

   /* Set edit limits.
    */
   VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_FULLNAME ) );
   Edit_LimitText( hwndEdit, sizeof(szFullName) - 1 );
   SetDlgItemText( hwnd, IDD_FULLNAME, szFullName );

   VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_EMAIL1 ) );
   Edit_LimitText( hwndEdit, sizeof(szEmail1) - 1 );

   VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_EMAIL2 ) );
   Edit_LimitText( hwndEdit, sizeof(szEmail2) - 1 );
   SetDlgItemText( hwnd, IDD_EMAIL2, szDomain);

   /* Set the picture bitmap.
    */
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_PIX ), hInst, idbSetupPic );

   /* Set the display attributes.
    */
   SetWindowFont( GetDlgItem( hwnd, IDD_TITLE ), hTitleFont, FALSE );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL SetupWizard_P5_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_FULLNAME:
      case IDD_EMAIL1:
      case IDD_EMAIL2:
         if( codeNotify == EN_CHANGE && IsWindowVisible( hwndCtl ) )
            if( Edit_GetTextLength( GetDlgItem( hwnd, IDD_FULLNAME ) ) > 0 &&
                Edit_GetTextLength( GetDlgItem( hwnd, IDD_EMAIL1 ) ) > 0 &&
                 Edit_GetTextLength( GetDlgItem( hwnd, IDD_EMAIL2 ) ) > 0 )
               PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT|PSWIZB_BACK );
            else
               PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_BACK );
         break;
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL SetupWizard_P5_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSETUPWIZ_P5 );
         break;

      case PSN_SETACTIVE:

         if( Edit_GetTextLength( GetDlgItem( hwnd, IDD_FULLNAME ) ) > 0 &&
             Edit_GetTextLength( GetDlgItem( hwnd, IDD_EMAIL1 ) ) > 0 &&
               Edit_GetTextLength( GetDlgItem( hwnd, IDD_EMAIL2 ) ) > 0 )
            PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT|PSWIZB_BACK );
         else
            PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_BACK );

         break;

      case PSN_WIZBACK:
         return( IDDLG_SETUPWIZ_P4 );

      case PSN_WIZNEXT:
         GetDlgItemText( hwnd, IDD_FULLNAME, szFullName, sizeof( szFullName ) );
         GetDlgItemText( hwnd, IDD_EMAIL1, szEmail1, sizeof( szEmail1 ) );
         GetDlgItemText( hwnd, IDD_EMAIL2, szEmail2, sizeof( szEmail2 ) );
         return( IDDLG_SETUPWIZ_P6 );
      }
   return( FALSE );
}

/* This function handles a page of the Setup Wizard
 */
BOOL EXPORT CALLBACK SetupWizard_P6( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, SetupWizard_P6_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, SetupWizard_P6_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, SetupWizard_P6_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL SetupWizard_P6_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   /* Set edit limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_FULLNAME ), sizeof(szMailLogin) - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_PASSWORD ), sizeof(szMailPassword) - 1 );

   /* Set the picture bitmap.
    */
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_PIX ), hInst, idbSetupPic );

   /* Set the display attributes.
    */
   SetWindowFont( GetDlgItem( hwnd, IDD_TITLE ), hTitleFont, FALSE );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL SetupWizard_P6_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_FULLNAME:
      case IDD_PASSWORD:
         if( codeNotify == EN_CHANGE && IsWindowVisible( hwndCtl ) )
            {
            if( Edit_GetTextLength( GetDlgItem( hwnd, IDD_FULLNAME ) ) > 0 &&
                Edit_GetTextLength( GetDlgItem( hwnd, IDD_PASSWORD ) ) )
               PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT|PSWIZB_BACK );
            else
               PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_BACK );
            }
         break;
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL SetupWizard_P6_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSETUPWIZ_P6 );
         break;

      case PSN_SETACTIVE:
         if( Edit_GetTextLength( GetDlgItem( hwnd, IDD_FULLNAME ) ) > 0 &&
             Edit_GetTextLength( GetDlgItem( hwnd, IDD_PASSWORD ) ) )
            PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT|PSWIZB_BACK );
         else
            PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_BACK );
         break;

      case PSN_WIZBACK:
         return( IDDLG_SETUPWIZ_P5 );

      case PSN_WIZNEXT:
         GetDlgItemText( hwnd, IDD_FULLNAME, szMailLogin, sizeof( szMailLogin ) );
         GetDlgItemText( hwnd, IDD_PASSWORD, szMailPassword, sizeof( szMailPassword ) );
         Amuser_Encrypt( szMailPassword, rgEncodeKey );
         strcpy( szHostname, szMailLogin );
         return( IDDLG_SETUPWIZ_P6_1 );
      }
   return( FALSE );
}

/* This function handles a page of the Setup Wizard
 */
BOOL EXPORT CALLBACK SetupWizard_P6_1( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, SetupWizard_P6_1_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, SetupWizard_P6_1_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, SetupWizard_P6_1_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL SetupWizard_P6_1_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   /* Set edit limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_FULLNAME ), sizeof(szSMTPMailLogin) - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_PASSWORD ), sizeof(szSMTPMailPassword) - 1 );

   /* Set the picture bitmap.
    */
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_PIX ), hInst, idbSetupPic );

   /* Set the display attributes.
    */
   SetWindowFont( GetDlgItem( hwnd, IDD_TITLE ), hTitleFont, FALSE );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL SetupWizard_P6_1_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDC_USESMTPAUTH: {
         BOOL fUseAuth = IsDlgButtonChecked( hwnd, IDC_USESMTPAUTH);
         EnableControl( hwnd, IDD_FULLNAME, fUseAuth );
         EnableControl( hwnd, IDD_PASSWORD, fUseAuth );
         EnableControl( hwnd, IDD_PAD1, fUseAuth );
         EnableControl( hwnd, IDD_PAD2, fUseAuth );
         SetupWizard_P6_1_EnableButtons( hwnd );
         break;
         }

      case IDD_FULLNAME:
      case IDD_PASSWORD:
         if( codeNotify == EN_CHANGE )
            SetupWizard_P6_1_EnableButtons( hwnd );
         break;
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL SetupWizard_P6_1_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSETUPWIZ_P6_1 );
         break;

      case PSN_SETACTIVE:
         SetupWizard_P6_1_EnableButtons( hwnd );
         break;

      case PSN_WIZBACK:
         return( IDDLG_SETUPWIZ_P6 );

      case PSN_WIZNEXT:
         if (IsDlgButtonChecked( hwnd, IDC_USESMTPAUTH))
         {
            GetDlgItemText( hwnd, IDD_FULLNAME, szSMTPMailLogin, sizeof( szSMTPMailLogin ) );
            GetDlgItemText( hwnd, IDD_PASSWORD, szSMTPMailPassword, sizeof( szSMTPMailPassword ) );
            Amuser_Encrypt( szSMTPMailPassword, rgEncodeKey );
         }
         return( IDDLG_SETUPWIZ_P7 );
      }
   return( FALSE );
}

void FASTCALL SetupWizard_P6_1_EnableButtons( HWND hwnd )
{
   if( IsDlgButtonChecked( hwnd, IDC_USESMTPAUTH ) &&
      ( Edit_GetTextLength( GetDlgItem( hwnd, IDD_FULLNAME ) ) == 0 ||
      Edit_GetTextLength( GetDlgItem( hwnd, IDD_PASSWORD ) ) == 0 ) )
      PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_BACK );
   else
      PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT|PSWIZB_BACK );
}

/* This function handles a page of the Setup Wizard
 */
BOOL EXPORT CALLBACK SetupWizard_P7( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, SetupWizard_P7_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, SetupWizard_P7_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL SetupWizard_P7_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   /* Default is No.
    */
   CheckDlgButton( hwnd, IDD_NO, TRUE );

   /* Set the picture bitmap.
    */
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_PIX ), hInst, idbSetupPic );

   /* Set the display attributes.
    */
   SetWindowFont( GetDlgItem( hwnd, IDD_TITLE ), hTitleFont, FALSE );
   return( TRUE );
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL SetupWizard_P7_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSETUPWIZ_P7 );
         break;

      case PSN_SETACTIVE:
         PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_BACK|PSWIZB_NEXT );
         break;

      case PSN_WIZBACK:
         return( IDDLG_SETUPWIZ_P6_1 );

      case PSN_WIZNEXT:
         fUseAmeol2ForIPMail = IsDlgButtonChecked( hwnd, IDD_YES );
         return( IDDLG_SETUPWIZ_P8 );
      }
   return( FALSE );
}

/* This function handles a page of the Setup Wizard
 */
BOOL EXPORT CALLBACK SetupWizard_P8( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, SetupWizard_P8_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, SetupWizard_P8_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL SetupWizard_P8_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   /* Set the picture bitmap.
    */
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_PIX ), hInst, idbSetupPic );

   /* Set the display attributes.
    */
   SetWindowFont( GetDlgItem( hwnd, IDD_TITLE ), hTitleFont, FALSE );
   return( TRUE );
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL SetupWizard_P8_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSETUPWIZ_P8 );
         break;

      case PSN_SETACTIVE:
         PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_FINISH|PSWIZB_BACK );
         break;

      case PSN_WIZBACK:
         PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT|PSWIZB_BACK );
         if ( nSetupAccountType == ACTYP_CONFERENCING_AND_INTERNET )
         {
            return( IDDLG_SETUPWIZ_P7 );
         }
         return( IDDLG_SETUPWIZ_P3 );

      case PSN_WIZFINISH: {
         char szName[LEN_TERMNAME+1];
         char szSection[LEN_TERMNAME+21+1];
         char szBuf[ 30 ];

         /* Start by unloading all services.
          */
         Amuser_WritePPInt( szCIXIP, "UseInternet", FALSE );
         Amuser_WritePPInt( szCIXIP, "Use RAS", FALSE );
         Amuser_WritePPInt( szCIX, "UseCIX", FALSE );
         Amuser_WritePPInt( szSettings, "MailDelivery", MAIL_DELIVER_IP );

         /* Save the version details which help to detect when Setup Wizard
          * needs to be re-run.
          */
         wsprintf( szBuf, "%u.%2.2u.%3.3u", amv.nMaxima, amv.nMinima, amv.nBuild );
         Amuser_WritePPString( szPreferences, "Version", szBuf );

         /* Save the account name.
          */
         Amuser_WritePPString( szPreferences, "Account Type", szAccountName );

         /* If we are changing from an Internet Only account to another type,
          * vape the toolbar, blink and connection settings. This ensures we 
          * install the default ones.
          */
         if (( fUseCIX ) && !( fOldUseCIX ))
         {
            Amuser_WritePPString( szToolbar, NULL, NULL );
            Amuser_WritePPString( szBlinkman, NULL, NULL );
            Amuser_WritePPString( szConnections, NULL, NULL );
         }

         /* Create internet e-mail address and reply. If we use CIX, then
          * the CIX nickname is the mailbox name. Otherwise we use the
          * hostname.
          */
         if( fUseInternet )
            {
            strcpy( szMailAddress, szEmail1 );
            strcat( szMailAddress, "@" );
            strcat( szMailAddress, szEmail2 );
            strcpy( szMailReplyAddress, szMailAddress );

            Amuser_WritePPInt( szCIXIP, "UseInternet", TRUE );
            Amuser_WritePPString( szCIXIP, "MailServer", szMailServer );
            Amuser_WritePPString( szCIXIP, "SMTPMailServer", szSMTPMailServer );
            Amuser_WritePPString( szCIXIP, "MailLogin", szMailLogin );
            Amuser_WritePPString( szCIXIP, "Hostname", szHostname );
            Amuser_WritePPString( szCIXIP, "MailAddress", szMailAddress );
            Amuser_WritePPString( szCIXIP, "MailReplyAddress", szMailReplyAddress );
            Amuser_WritePPString( szCIXIP, "MailFullName", szFullName );
            Amuser_WritePPString( szCIXIP, "NewsServer", szNewsServer );
            Amuser_WritePPString( szCIXIP, "Domain", szDomain );
            Amuser_WritePPString( szCIXIP, "Provider", szProvider );
            Amuser_WritePPPassword( szCIXIP, "MailPassword", szMailPassword );
            Amuser_WritePPString( szCIXIP, "SMTPMailLogin", szSMTPMailLogin );
            Amuser_WritePPPassword( szCIXIP, "SMTPMailPassword", szSMTPMailPassword );
            fRememberPassword = TRUE;
         }

         Amuser_WritePPInt( szCIX, "UseCIX", fUseCIX );

         /* For Windows 95, create a TAPI conn card
          * too.
          */
      #ifdef CREATE_MODEM_CARDS
         if( fUseCIX )
         {
            int cbAreaCode;

            cd.wProtocol = PROTOCOL_TELEPHONY;
            cd.nTimeout = 60;
            cd.lpsc = &sc;
            cd.lpsc->dwBaud = 0L;
            strcpy( cd.lpsc->szCountryCode, "44" );
            strcpy( cd.lpsc->szAreaCode, CIX_AREA_CODE );
            strcpy( cd.lpsc->szLocation, "" );
            strcpy( cd.szScript, "connect.scr" );
            strcpy( cd.szTitle, szDefTAPIConf );

            /* Strip off explicit area code.
             */
            cbAreaCode = strlen( cd.lpsc->szAreaCode );
            if( memcmp( cd.lpsc->szAreaCode, cd.lpsc->szPhone, cbAreaCode ) == 0 )
               strcpy( cd.lpsc->szPhone, cd.lpsc->szPhone + cbAreaCode );
            else if( memcmp( "0845", cd.lpsc->szPhone, 4 ) == 0 )
            {
               strcpy( cd.lpsc->szPhone, cd.lpsc->szPhone + 4 );
               strcpy( cd.lpsc->szAreaCode, "0845" );
            }
            else if( memcmp( "020", cd.lpsc->szPhone, 3 ) == 0 )
            {
               strcpy( cd.lpsc->szPhone, cd.lpsc->szPhone + 3 );
               strcpy( cd.lpsc->szAreaCode, "020" );
            }
            else if( memcmp( "0181", cd.lpsc->szPhone, 4 ) == 0 )
            {
               strcpy( cd.lpsc->szPhone, cd.lpsc->szPhone + 4 );
               strcpy( cd.lpsc->szAreaCode, "0181" );
            }
                  

            /* Create the connection card.
             */
            CreateCommDescriptor( &cd );
         }
      #endif

         /* Create an Ameol Communications connection
          * card.
          */
      #ifdef CREATE_MODEM_CARDS
         cd.wProtocol = PROTOCOL_MODEM;
         cd.nTimeout = 60;
         cd.lpsc = &sc;
         strcpy( cd.szScript, "connect.scr" );
         strcpy( cd.szTitle, szDefModemConf );
         wsprintf( lpTmpBuf, "%s%s", sc.szAreaCode, sc.szPhone );
         strcpy( cd.lpsc->szPhone, lpTmpBuf );

         if( fWin95 )
            strcpy( sc.md.szModemName, "Generic Modem" );
         LoadModemDescriptor( &sc.md );
         CreateCommDescriptor( &cd );
      #endif
         
         /* Use Ameol for Internet mail?
          */
         if( fUseAmeol2ForIPMail )
            MakeAmeol2DefaultMailClient();

         if( fUseCIX )
         {
            /* Create the default IP Connection card.
             */
            cd.wProtocol = PROTOCOL_NETWORK;
            cd.nTimeout = 60;
            cd.lpic = &ic;
            ic.wPort = IPPORT_TELNET;
            strcpy( ic.szAddress, szDefCIXTelnet );
            strcpy( cd.szTitle, szDefNAIPConf );
            strcpy( cd.szScript, "connect.scr" );
            CreateCommDescriptor( &cd );
            strcpy( szCIXConnCard, cd.szTitle );

            /* Create the default terminal.
             */
            strcpy( szName, "CIX Forums" );
            wsprintf( szSection, "%s Properties", szName );
            Amuser_WritePPString( szTerminals, szName, szSection );
            Amuser_WritePPString( szSection, "ConnectionCard", szCIXConnCard );
         
            /* Save the CIX settings, if any.
             * Encode the password in DES format before saving.
             */
            Amuser_WritePPString( szCIX, "Nickname", szCIXNickname );
            Amuser_WritePPPassword( szCIX, "Password", szCIXPassword );
            Amuser_WritePPString( szCIX, "ConnectionCard", szCIXConnCard );
            Amuser_WritePPInt( szCIX, "CIXviaInternet", fCIXViaInternet );
            }

         /* If internet service installed, add the ISP's
          * news server details.
          */
         if( fUseInternet && *szNewsServer )
            {
            CmdCreateNewsServer( szNewsServer );
            Amuser_WritePPString( szNews, "Default Server", szNewsServer );
            }
         fCompletingSetupWizard = TRUE;
         return( 0 );
         }
      }
   return( FALSE );
}

/* This function registers Ameol as the default mail client on the
 * server. (Windows 95 and Windows NT 3.51 only)
 */
void FASTCALL MakeAmeol2DefaultMailClient( void )
{
   DWORD dwDisposition;
   HKEY hkOut;

   CopyRegistry( HKEY_LOCAL_MACHINE, "SOFTWARE\\Clients\\mail\\Ameol2\\Protocols\\mailto",
              HKEY_CURRENT_USER, "SOFTWARE\\Classes\\mailto" );
   if( RegCreateKeyEx( HKEY_CLASSES_ROOT, "mailto", 0L, "", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkOut, &dwDisposition ) == ERROR_SUCCESS )
      {
      static char szDefault[] = "URL:MailTo Protocol";
      static BYTE Data[ 4 ] = { 0x02, 0x00, 0x00, 0x00 };

      RegSetValueEx( hkOut, "EditFlags", 0L, REG_BINARY, Data, 4 );
      RegSetValueEx( hkOut, NULL, 0L, REG_SZ, szDefault, strlen(szDefault) + 1 );
      RegSetValueEx( hkOut, "URL Protocol", 0L, REG_SZ, "", 1 );
      RegCloseKey( hkOut );
      }
   if( RegCreateKeyEx( HKEY_LOCAL_MACHINE, "SOFTWARE\\Clients\\mail", 0L, "", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkOut, &dwDisposition ) == ERROR_SUCCESS )
      {
      RegSetValueEx( hkOut, NULL, 0L, REG_SZ, "Ameol2", 7 );
      RegCloseKey( hkOut );
      }
}

/* This function registers Ameol as the default news client on the
 * server. (Windows 95 and Windows NT 3.51 only)
 */
void FASTCALL MakeAmeol2DefaultNewsClient( void )
{
   DWORD dwDisposition;
   HKEY hkOut;

   CopyRegistry( HKEY_LOCAL_MACHINE, "SOFTWARE\\Clients\\news\\Ameol2\\Protocols\\nntp",
              HKEY_CURRENT_USER, "SOFTWARE\\Classes\\nntp" );
   CopyRegistry( HKEY_LOCAL_MACHINE, "SOFTWARE\\Clients\\news\\Ameol2\\Protocols\\news",
              HKEY_CURRENT_USER, "SOFTWARE\\Classes\\news" );
   if( RegCreateKeyEx( HKEY_CLASSES_ROOT, "news", 0L, "", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkOut, &dwDisposition ) == ERROR_SUCCESS )
      {
      static char szDefault[] = "URL:News Protocol";
      static BYTE Data[ 4 ] = { 0x02, 0x00, 0x00, 0x00 };

      RegSetValueEx( hkOut, "EditFlags", 0L, REG_BINARY, Data, 4 );
      RegSetValueEx( hkOut, NULL, 0L, REG_SZ, szDefault, strlen(szDefault) + 1 );
      RegSetValueEx( hkOut, "URL Protocol", 0L, REG_SZ, "", 1 );
      RegCloseKey( hkOut );
      }
   if( RegCreateKeyEx( HKEY_CLASSES_ROOT, "nntp", 0L, "", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkOut, &dwDisposition ) == ERROR_SUCCESS )
      {
      static char szDefault[] = "URL:News Protocol";
      static BYTE Data[ 4 ] = { 0x02, 0x00, 0x00, 0x00 };

      RegSetValueEx( hkOut, "EditFlags", 0L, REG_BINARY, Data, 4 );
      RegSetValueEx( hkOut, NULL, 0L, REG_SZ, szDefault, strlen(szDefault) + 1 );
      RegSetValueEx( hkOut, "URL Protocol", 0L, REG_SZ, "", 1 );
      RegCloseKey( hkOut );
      }
   if( RegCreateKeyEx( HKEY_LOCAL_MACHINE, "SOFTWARE\\Clients\\news", 0L, "", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkOut, &dwDisposition ) == ERROR_SUCCESS )
      {
      RegSetValueEx( hkOut, NULL, 0L, REG_SZ, "Ameol2", 7 );
      RegCloseKey( hkOut );
      }
}

/* This function copies a branch of the registry to another branch, overwriting
 * any values if they already exist.
 */
void FASTCALL CopyRegistry( HKEY hkSrc, char * pszSrc, HKEY hkDest, char * pszDest )
{
   HKEY hkResult;

   if( RegOpenKeyEx( hkSrc, pszSrc, 0L, KEY_READ, &hkResult ) == ERROR_SUCCESS )
      {
      char szKeyName[ 40 ];
      DWORD cbKeyName;
      DWORD dwIndex;

      /* Enumerate all subkeys under the root key.
       */
      dwIndex = 0L;
      cbKeyName = 40;
      while( ERROR_SUCCESS == RegEnumKey( hkResult, dwIndex, szKeyName, cbKeyName ) )
         {
         char szSrcKeyPath[ 100 ];
         char szDestKeyPath[ 100 ];
         HKEY hkSubKey;

         /* For each subkey, copy across the values.
          */
         wsprintf( szSrcKeyPath, "%s\\%s", pszSrc, szKeyName );
         wsprintf( szDestKeyPath, "%s\\%s", pszDest, szKeyName );
         if( RegOpenKeyEx( hkSrc, szSrcKeyPath, 0L, KEY_READ, &hkSubKey ) == ERROR_SUCCESS )
            {
            DWORD dwDisposition;
            DWORD index;
            HKEY hkOut;

            index = 0;
            if( RegCreateKeyEx( hkDest, szDestKeyPath, 0L, "", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkOut, &dwDisposition ) == ERROR_SUCCESS )
               {
               char szValueName[30];
               DWORD cbValueName;

               /* Now read one entry from the source and write it to the
                * destination key.
                */
               cbValueName = sizeof( szValueName );
               while( ERROR_SUCCESS == RegEnumValue( hkSubKey, index++, szValueName, &cbValueName, NULL, NULL, 0, NULL ) )
                  {
                  BYTE Data[ 100 ];
                  DWORD dwType;
                  DWORD cbData;

                  cbData = sizeof( Data );
                  if( ERROR_SUCCESS == RegQueryValueEx( hkSubKey, szValueName, 0, &dwType, Data, &cbData ) )
                     RegSetValueEx( hkOut, szValueName, 0L, dwType, Data, cbData );
                  }
               RegCloseKey( hkOut );
               }
            RegCloseKey( hkSubKey );
            }

         /* Now recur to copy branches
          */
         CopyRegistry( hkSrc, szSrcKeyPath, hkDest, szDestKeyPath );
         ++dwIndex;
         }
      RegCloseKey( hkResult );
      }
}

