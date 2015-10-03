/* COMMS.C - The Palantir communications driver
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
#include "intcomms.h"
#include "lbf.h"
#include "amlib.h"
#include <string.h>
#include <ctype.h>
#include "phone.h"

#define  THIS_FILE   __FILE__

static BOOL fDefDlgEx = FALSE;

extern BOOL fIsAmeol2Scratchpad;

/* Default modem strings
 */
char MS_INITIALISE[] =           "ATZ";
char MS_RESET[] =                "ATZ";
char MS_DIALTONE[] =             "ATDT";
char MS_DIALPULSE[] =            "ATDP";
char MS_HANGUP[] =               "ATH";
char MS_SUFFIX[] =               "^M";
char MS_ESCAPE[] =               "+++";
char MS_MODEMACK[] =             "OK";
char MS_MODEMNACK[] =            "ERROR";
char MS_CONNECTOK[] =            "CONNECT";
char MS_CONNECTFAIL[] =          "NO CARRIER";
char MS_LINEBUSY[] =             "BUSY";

/* Modem section names in MODEMS.INI and configuration file.
 */
char FAR szmsctModemName[] =         "Modem";
char FAR szmsctInitialise[] =        "Initialise";
char FAR szmsctReset[] =             "Reset";
char FAR szmsctDialTone[] =          "Dial Tone";
char FAR szmsctDialPulse[] =         "Dial Pulse";
char FAR szmsctHangup[] =            "Hang Up";
char FAR szmsctSuffix[] =            "Suffix";
char FAR szmsctEscape[] =            "Escape";
char FAR szmsctModemAck[] =          "Modem Acknowledge";
char FAR szmsctModemNack[] =         "Error";
char FAR szmsctConnectMessage[] =    "Connect Message";
char FAR szmsctConnectFail[] =       "Connect Failure";
char FAR szmsctLineBusy[] =          "Line Busy";

DWORD dwModemBauds[] = { 300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200 };
#define  cModemBauds    ( sizeof( dwModemBauds ) / sizeof( DWORD ) )

BOOL FASTCALL ModemSettings_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL ModemSettings_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL ModemSettings_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT CALLBACK SaveModemSettings( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL SaveModemSettings_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL SaveModemSettings_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL SaveModemSettings_DlgProc( HWND, UINT, WPARAM, LPARAM );

void FASTCALL UpdateModemSetting( HWND, int, char *, char *, char *, char * );

/* This function fills the specified combo box with a list of
 * all available connection cards.
 */
void FASTCALL FillConnectionCards( HWND hwnd, int nID, char * pSelectedCard )
{
   LPSTR lpszConnCardList;
   HWND hwndList;

   INITIALISE_PTR(lpszConnCardList);
   hwndList = GetDlgItem( hwnd, nID );
   ComboBox_ResetContent( hwndList );
   if( fNewMemory( &lpszConnCardList, 8000 ) )
      {
      register UINT c;
      int index;

      /* Read and add the list to the combo box.
       */
      Amuser_GetPPString( szConnections, NULL, "", lpszConnCardList, 8000 );
      for( c = 0; lpszConnCardList[ c ]; c += strlen( &lpszConnCardList[ c ] ) + 1 )
         ComboBox_AddString( hwndList, &lpszConnCardList[ c ] );

      /* Find and select the selected card.
       */
      if( CB_ERR != ( index = ComboBox_FindStringExact( hwndList, -1, pSelectedCard ) ) )
         ComboBox_SetCurSel( hwndList, index );
      FreeMemory( &lpszConnCardList );
      }
   if( CB_ERR == ( ComboBox_GetCurSel( hwndList ) ) )
      ComboBox_SetCurSel( hwndList, 0 );
}

/* This function creates a new comm descriptor.
 */
BOOL FASTCALL CreateCommDescriptor( LPCOMMDESCRIPTOR lpcd )
{
   char szTitle[LEN_CONNCARD+20+1];

   wsprintf( szTitle, "%s Connection Card", lpcd->szTitle );
   Amuser_WritePPString( szConnections, lpcd->szTitle, szTitle );
   return( SaveCommDescriptor( lpcd ) );
}

/* This function saves the specified descriptor after the
 * user has finished editing it.
 */
BOOL FASTCALL SaveCommDescriptor( LPCOMMDESCRIPTOR lpcd )
{
   char szTitle[LEN_CONNCARD+20+1];

   wsprintf( szTitle, "%s Connection Card", lpcd->szTitle );
   Amuser_WritePPString( szTitle, NULL, NULL );
   Amuser_WritePPInt( szTitle, "Timeout", lpcd->nTimeout );
   Amuser_WritePPInt( szTitle, "Protocol", lpcd->wProtocol );
   if( lpcd->wProtocol & PROTOCOL_SERIAL )
      {
      /* Write serial settings.
       */
      Amuser_WritePPLong( szTitle, "Baud Rate", lpcd->lpsc->dwBaud );
      Amuser_WritePPInt( szTitle, "Dial Type", lpcd->lpsc->wDialType );
      Amuser_WritePPInt( szTitle, "CTS Check", lpcd->lpsc->fCheckCTS );
      Amuser_WritePPInt( szTitle, "DCD Check", lpcd->lpsc->fCheckDCD );
      Amuser_WritePPInt( szTitle, "Retry", lpcd->lpsc->wRetry );
      Amuser_WritePPString( szTitle, "Primary Phone Number", lpcd->lpsc->szPhone );
   #ifdef WIN32
      Amuser_WritePPString( szTitle, "Area Code", lpcd->lpsc->szAreaCode );
      Amuser_WritePPString( szTitle, "Country Code", lpcd->lpsc->szCountryCode );
      Amuser_WritePPString( szTitle, "Location", lpcd->lpsc->szLocation );
   #endif
      Amuser_WritePPPassword( szTitle, "Mercury", lpcd->lpsc->szMercury );
      Amuser_WritePPString( szTitle, "Port Name", lpcd->lpsc->szPort );
      Amuser_WritePPString( szTitle, "Startup Script", lpcd->szScript );
      Amuser_WritePPString( szTitle, "Modem Name", lpcd->lpsc->md.szModemName );
      Amuser_WritePPString( szTitle, szmsctInitialise, lpcd->lpsc->md.szInitialise );
      Amuser_WritePPString( szTitle, szmsctReset, lpcd->lpsc->md.szReset );
      Amuser_WritePPString( szTitle, szmsctDialTone, lpcd->lpsc->md.szDialTone );
      Amuser_WritePPString( szTitle, szmsctDialPulse, lpcd->lpsc->md.szDialPulse );
      Amuser_WritePPString( szTitle, szmsctHangup, lpcd->lpsc->md.szHangup );
      Amuser_WritePPString( szTitle, szmsctSuffix, lpcd->lpsc->md.szSuffix );
      Amuser_WritePPString( szTitle, szmsctEscape, lpcd->lpsc->md.szEscape );
      Amuser_WritePPString( szTitle, szmsctModemAck, lpcd->lpsc->md.szModemAck );
      Amuser_WritePPString( szTitle, szmsctModemNack, lpcd->lpsc->md.szModemNack );
      Amuser_WritePPString( szTitle, szmsctConnectMessage, lpcd->lpsc->md.szConnectMessage );
      Amuser_WritePPString( szTitle, szmsctConnectFail, lpcd->lpsc->md.szConnectFail );
      Amuser_WritePPString( szTitle, szmsctLineBusy, lpcd->lpsc->md.szLineBusy );
      }
   else
      {
      /* Write IP settings.
       */
      Amuser_WritePPInt( szTitle, "IP Port", lpcd->lpic->wPort );
      Amuser_WritePPString( szTitle, "Address", lpcd->lpic->szAddress );
      Amuser_WritePPString( szTitle, "Startup Script", lpcd->szScript );
      }
   return( TRUE );
}

/* This function loads the specified descriptor. Just fill the lpcd->szTitle
 * field with the descriptor name before calling this function.
 */
BOOL FASTCALL LoadCommDescriptor( LPCOMMDESCRIPTOR lpcd )
{
   char szTitle[LEN_CONNCARD+20+1];

   wsprintf( szTitle, "%s Connection Card", lpcd->szTitle );
   lpcd->wProtocol = Amuser_GetPPInt( szTitle, "Protocol", PROTOCOL_MODEM );
   lpcd->nTimeout = Amuser_GetPPInt( szTitle, "Timeout", 60 );
   if( lpcd->wProtocol & PROTOCOL_SERIAL )
      {
      /* This is a serial connection card, so look for
       * and load serial settings.
       */
      if( lpcd->lpsc )
         FreeMemory( &lpcd->lpsc );
      if( fNewMemory( &lpcd->lpsc, sizeof(SERIALCOMM) ) )
         {
         LPMODEMDESCRIPTOR lpmd;
         char szModemName[ 100 ];

         /* First load the serial settings.
          */
         lpcd->lpsc->dwBaud = Amuser_GetPPLong( szTitle, "Baud Rate", 9600 );
         lpcd->lpsc->wDialType = Amuser_GetPPInt( szTitle, "Dial Type", DTYPE_TONE );
         lpcd->lpsc->fCheckCTS = Amuser_GetPPInt( szTitle, "CTS Check", TRUE );
         lpcd->lpsc->fCheckDCD = Amuser_GetPPInt( szTitle, "DCD Check", TRUE );
         lpcd->lpsc->wRetry = Amuser_GetPPInt( szTitle, "Retry", 5 );
         Amuser_GetPPString( szTitle, "Primary Phone Number", "", lpcd->lpsc->szPhone, 80 );
      #ifdef WIN32
         Amuser_GetPPString( szTitle, "Area Code", CIX_AREA_CODE, lpcd->lpsc->szAreaCode, 8);
         Amuser_GetPPString( szTitle, "Country Code", "44", lpcd->lpsc->szCountryCode, 8 );
         Amuser_GetPPString( szTitle, "Location", "(Default)", lpcd->lpsc->szLocation, 64 );
      #endif
         Amuser_GetPPString( szTitle, "Port Name", "", lpcd->lpsc->szPort, 8 );
         Amuser_GetPPString( szTitle, "Startup Script", "", lpcd->szScript, sizeof(lpcd->szScript) );

         /* Get old mercury prefix and copy to new, deleting the old
          * non-encrypted entry.
          */
         if( Amuser_GetPPString( szTitle, "Mercury Prefix", "", lpcd->lpsc->szMercury, 40 ) )
            {
            Amuser_Encrypt( lpcd->lpsc->szMercury, rgEncodeKey );
            Amuser_WritePPString( szTitle, "Mercury Prefix", NULL );
            Amuser_WritePPPassword( szTitle, "Mercury", lpcd->lpsc->szMercury );
            }
         Amuser_GetPPPassword( szTitle, "Mercury", "", lpcd->lpsc->szMercury, 40 );

         /* Strip area code from phone number.
          */
      #ifdef WIN32
         if( PROTOCOL_TELEPHONY == lpcd->wProtocol )
            {
            int cbAreaCode;

            cbAreaCode = strlen( lpcd->lpsc->szAreaCode );
            if( memcmp( lpcd->lpsc->szAreaCode, lpcd->lpsc->szPhone, cbAreaCode ) == 0 )
               {
               strcpy( lpcd->lpsc->szPhone, lpcd->lpsc->szPhone + cbAreaCode );
               Amuser_WritePPString( szTitle, "Primary Phone Number", lpcd->lpsc->szPhone );
               }
            }
      #endif

         /* Fix blank script for beta testers.
          */
      #ifdef WIN32
         if( *lpcd->lpsc->szAreaCode == '\0' )
            strcpy( lpcd->lpsc->szAreaCode, CIX_AREA_CODE );
         if( *lpcd->lpsc->szCountryCode == '\0' )
            strcpy( lpcd->lpsc->szCountryCode, "44" );
      #endif

         /* Next, get the modem name.
          */
         Amuser_GetPPString( szTitle, "Modem Name", "", szModemName, sizeof(szModemName) );
         strcpy( lpcd->lpsc->md.szModemName, szModemName );

         /* Load the default modem settings.
          */
         lpmd = &lpcd->lpsc->md;
         LoadModemDescriptor( lpmd );

         /* Override any user settings.
          */
         Amuser_GetPPString( szTitle, szmsctInitialise, lpmd->szInitialise, lpmd->szInitialise, sizeof(lpmd->szInitialise) );
         Amuser_GetPPString( szTitle, szmsctReset, lpmd->szReset, lpmd->szReset, sizeof(lpmd->szReset) );
         Amuser_GetPPString( szTitle, szmsctDialTone, lpmd->szDialTone, lpmd->szDialTone, sizeof(lpmd->szDialTone) );
         Amuser_GetPPString( szTitle, szmsctDialPulse, lpmd->szDialPulse, lpmd->szDialPulse, sizeof(lpmd->szDialPulse) );
         Amuser_GetPPString( szTitle, szmsctHangup, lpmd->szHangup, lpmd->szHangup, sizeof(lpmd->szHangup) );
         Amuser_GetPPString( szTitle, szmsctSuffix, lpmd->szSuffix, lpmd->szSuffix, sizeof(lpmd->szSuffix) );
         Amuser_GetPPString( szTitle, szmsctEscape, lpmd->szEscape, lpmd->szEscape, sizeof(lpmd->szEscape) );
         Amuser_GetPPString( szTitle, szmsctModemAck, lpmd->szModemAck, lpmd->szModemAck, sizeof(lpmd->szModemAck) );
         Amuser_GetPPString( szTitle, szmsctModemNack, lpmd->szModemNack, lpmd->szModemNack, sizeof(lpmd->szModemNack) );
         Amuser_GetPPString( szTitle, szmsctConnectMessage, lpmd->szConnectMessage, lpmd->szConnectMessage, sizeof(lpmd->szConnectMessage) );
         Amuser_GetPPString( szTitle, szmsctConnectFail, lpmd->szConnectFail, lpmd->szConnectFail, sizeof(lpmd->szConnectFail) );
         Amuser_GetPPString( szTitle, szmsctLineBusy, lpmd->szLineBusy, lpmd->szLineBusy, sizeof(lpmd->szLineBusy) );
         return( TRUE );
         }
      }
   else
      {
      /* This is an IP connection card, so look for and
       * load IP settings.
       */
      if( lpcd->lpic )
         FreeMemory( &lpcd->lpic );
      if( fNewMemory( &lpcd->lpic, sizeof(IPCOMM) ) )
         {
         lpcd->lpic->wPort = (WORD)Amuser_GetPPInt( szTitle, "IP Port", IPPORT_TELNET );
         Amuser_GetPPString( szTitle, "Address", "", lpcd->lpic->szAddress, sizeof(lpcd->lpic->szAddress) );
         Amuser_GetPPString( szTitle, "Startup Script", "", lpcd->szScript, sizeof(lpcd->szScript) );

         /* Use default RAS.
          */
      #ifdef WIN32
         lpcd->lpic->rd = rdDef;
      #endif
         return( TRUE );
         }
      }
   return( FALSE );
}

/* This function reads the settings for the specified modem.
 */
void FASTCALL LoadModemDescriptor( MODEMDESCRIPTOR * lpmd )
{
   char szPrivIniFile[ _MAX_PATH ];

   strcat( strcpy( szPrivIniFile, pszAmeolDir ), szModemsIni );
   GetPrivateProfileString( lpmd->szModemName, szmsctInitialise, MS_INITIALISE, lpmd->szInitialise, sizeof( lpmd->szInitialise ), szPrivIniFile );
   GetPrivateProfileString( lpmd->szModemName, szmsctReset, MS_RESET, lpmd->szReset, sizeof( lpmd->szReset ), szPrivIniFile );
   GetPrivateProfileString( lpmd->szModemName, szmsctDialTone, MS_DIALTONE, lpmd->szDialTone, sizeof( lpmd->szDialTone ), szPrivIniFile );
   GetPrivateProfileString( lpmd->szModemName, szmsctDialPulse, MS_DIALPULSE, lpmd->szDialPulse, sizeof( lpmd->szDialPulse ), szPrivIniFile );
   GetPrivateProfileString( lpmd->szModemName, szmsctHangup, MS_HANGUP, lpmd->szHangup, sizeof( lpmd->szHangup ), szPrivIniFile );
   GetPrivateProfileString( lpmd->szModemName, szmsctSuffix, MS_SUFFIX, lpmd->szSuffix, sizeof( lpmd->szSuffix ), szPrivIniFile );
   GetPrivateProfileString( lpmd->szModemName, szmsctEscape, MS_ESCAPE, lpmd->szEscape, sizeof( lpmd->szEscape ), szPrivIniFile );
   GetPrivateProfileString( lpmd->szModemName, szmsctModemAck, MS_MODEMACK, lpmd->szModemAck, sizeof( lpmd->szModemAck ), szPrivIniFile );
   GetPrivateProfileString( lpmd->szModemName, szmsctModemNack, MS_MODEMNACK, lpmd->szModemNack, sizeof( lpmd->szModemNack ), szPrivIniFile );
   GetPrivateProfileString( lpmd->szModemName, szmsctConnectMessage, MS_CONNECTOK, lpmd->szConnectMessage, sizeof( lpmd->szConnectMessage ), szPrivIniFile );
   GetPrivateProfileString( lpmd->szModemName, szmsctConnectFail, MS_CONNECTFAIL, lpmd->szConnectFail, sizeof( lpmd->szConnectFail ), szPrivIniFile );
   GetPrivateProfileString( lpmd->szModemName, szmsctLineBusy, MS_LINEBUSY, lpmd->szLineBusy, sizeof( lpmd->szLineBusy ), szPrivIniFile );
}

/* This function handles the Modem settings dialog.
 */
BOOL EXPORT CALLBACK ModemSettings( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = ModemSettings_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the ModemSettings
 * dialog that it does not handle.
 */
LRESULT FASTCALL ModemSettings_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, ModemSettings_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, ModemSettings_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsMODEMSETTINGS );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL ModemSettings_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPMODEMDESCRIPTOR lpmd;
   HWND hwndList;
   int index;

   /* Get the modem descriptor.
    */
   lpmd = (LPMODEMDESCRIPTOR)lParam;
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* Fill the drop down list with the list of
    * modems.
    */
   FillModemList( hwnd, IDD_LIST );

   /* Select the one specified by the lpmd structure.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   if( CB_ERR != ( index = ComboBox_FindStringExact( hwndList, -1, lpmd->szModemName ) ) )
      ComboBox_SetCurSel( hwndList, index );

   /* Initialise the fields.
    */
   SetDlgItemText( hwnd, IDD_INITIALISE, lpmd->szInitialise );
   SetDlgItemText( hwnd, IDD_RESET, lpmd->szReset );
   SetDlgItemText( hwnd, IDD_DIALTONE, lpmd->szDialTone );
   SetDlgItemText( hwnd, IDD_DIALPULSE, lpmd->szDialPulse );
   SetDlgItemText( hwnd, IDD_HANGUP, lpmd->szHangup );
   SetDlgItemText( hwnd, IDD_SUFFIX, lpmd->szSuffix );
   SetDlgItemText( hwnd, IDD_ESCAPE, lpmd->szEscape );
   SetDlgItemText( hwnd, IDD_MODEM_ACK, lpmd->szModemAck );
   SetDlgItemText( hwnd, IDD_ERROR, lpmd->szModemNack );
   SetDlgItemText( hwnd, IDD_CONNECT_MESSAGE, lpmd->szConnectMessage );
   SetDlgItemText( hwnd, IDD_CONNECT_FAIL, lpmd->szConnectFail );
   SetDlgItemText( hwnd, IDD_LINEBUSY, lpmd->szLineBusy );

   /* Set the input field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_INITIALISE), sizeof(lpmd->szInitialise)-1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_RESET), sizeof(lpmd->szReset)-1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_DIALTONE), sizeof(lpmd->szDialTone)-1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_DIALPULSE), sizeof(lpmd->szDialPulse)-1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_HANGUP), sizeof(lpmd->szHangup)-1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_SUFFIX), sizeof(lpmd->szSuffix)-1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_ESCAPE), sizeof(lpmd->szEscape)-1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_MODEM_ACK), sizeof(lpmd->szModemAck)-1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_ERROR), sizeof(lpmd->szModemNack)-1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_CONNECT_MESSAGE), sizeof(lpmd->szConnectMessage)-1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_CONNECT_FAIL), sizeof(lpmd->szConnectFail)-1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_LINEBUSY), sizeof(lpmd->szLineBusy)-1 );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL ModemSettings_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_SAVE:
         Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_SAVEMODEMSETTINGS), SaveModemSettings, 0L );
         break;

      case IDD_LIST:
         if( codeNotify == CBN_SELCHANGE )
            {
            char szPrivIniFile[ _MAX_PATH ];
            char sz2[ 100 ];
            char sz[ 100 ];
            HWND hwndList;
            int index;

            /* The user has changed the modem type, so get the new modem
             * name and the path to the MODEMS.INI file.
             */
            hwndList = GetDlgItem( hwnd, IDD_LIST );
            index = ComboBox_GetCurSel( hwndList );
            ComboBox_GetLBText( hwndList, index, sz );
            strcat( strcpy( szPrivIniFile, pszAmeolDir ), szModemsIni );

            /* Read the settings for the selected modem from the MODEMS.INI file.
             */
            GetPrivateProfileString( sz, szmsctInitialise, MS_INITIALISE, sz2, sizeof( sz2 ), szPrivIniFile );
            SetDlgItemText( hwnd, IDD_INITIALISE, sz2 );
            GetPrivateProfileString( sz, szmsctReset, MS_RESET, sz2, sizeof( sz2 ), szPrivIniFile );
            SetDlgItemText( hwnd, IDD_RESET, sz2 );
            GetPrivateProfileString( sz, szmsctDialTone, MS_DIALTONE, sz2, sizeof( sz2 ), szPrivIniFile );
            SetDlgItemText( hwnd, IDD_DIALTONE, sz2 );
            GetPrivateProfileString( sz, szmsctDialPulse, MS_DIALPULSE, sz2, sizeof( sz2 ), szPrivIniFile );
            SetDlgItemText( hwnd, IDD_DIALPULSE, sz2 );
            GetPrivateProfileString( sz, szmsctHangup, MS_HANGUP, sz2, sizeof( sz2 ), szPrivIniFile );
            SetDlgItemText( hwnd, IDD_HANGUP, sz2 );
            GetPrivateProfileString( sz, szmsctSuffix, MS_SUFFIX, sz2, sizeof( sz2 ), szPrivIniFile );
            SetDlgItemText( hwnd, IDD_SUFFIX, sz2 );
            GetPrivateProfileString( sz, szmsctEscape, MS_ESCAPE, sz2, sizeof( sz2 ), szPrivIniFile );
            SetDlgItemText( hwnd, IDD_ESCAPE, sz2 );
            GetPrivateProfileString( sz, szmsctModemAck, MS_MODEMACK, sz2, sizeof( sz2 ), szPrivIniFile );
            SetDlgItemText( hwnd, IDD_MODEM_ACK, sz2 );
            GetPrivateProfileString( sz, szmsctModemNack, MS_MODEMNACK, sz2, sizeof( sz2 ), szPrivIniFile );
            SetDlgItemText( hwnd, IDD_ERROR, sz2 );
            GetPrivateProfileString( sz, szmsctConnectMessage, MS_CONNECTOK, sz2, sizeof( sz2 ), szPrivIniFile );
            SetDlgItemText( hwnd, IDD_CONNECT_MESSAGE, sz2 );
            GetPrivateProfileString( sz, szmsctConnectFail, MS_CONNECTFAIL, sz2, sizeof( sz2 ), szPrivIniFile );
            SetDlgItemText( hwnd, IDD_CONNECT_FAIL, sz2 );
            GetPrivateProfileString( sz, szmsctLineBusy, MS_LINEBUSY, sz2, sizeof( sz2 ), szPrivIniFile );
            SetDlgItemText( hwnd, IDD_LINEBUSY, sz2 );
            }
         break;

      case IDOK: {
         LPMODEMDESCRIPTOR lpmd;
         HWND hwndList;
         int index;

         /* Get the modem descriptor.
          */
         lpmd = (LPMODEMDESCRIPTOR)GetWindowLong( hwnd, DWL_USER );

         /* Get the modem name.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         index = ComboBox_GetCurSel( hwndList );
         ComboBox_GetLBText( hwndList, index, lpmd->szModemName );

         /* Fill out the modem settings in the descriptor.
          */
         GetDlgItemText( hwnd, IDD_INITIALISE, lpmd->szInitialise, sizeof(lpmd->szInitialise) );
         GetDlgItemText( hwnd, IDD_RESET, lpmd->szReset, sizeof(lpmd->szReset) );
         GetDlgItemText( hwnd, IDD_DIALTONE, lpmd->szDialTone, sizeof(lpmd->szDialTone) );
         GetDlgItemText( hwnd, IDD_DIALPULSE, lpmd->szDialPulse, sizeof(lpmd->szDialPulse) );
         GetDlgItemText( hwnd, IDD_HANGUP, lpmd->szHangup, sizeof(lpmd->szHangup) );
         GetDlgItemText( hwnd, IDD_SUFFIX, lpmd->szSuffix, sizeof(lpmd->szSuffix) );
         GetDlgItemText( hwnd, IDD_ESCAPE, lpmd->szEscape, sizeof(lpmd->szEscape) );
         GetDlgItemText( hwnd, IDD_MODEM_ACK, lpmd->szModemAck, sizeof(lpmd->szModemAck) );
         GetDlgItemText( hwnd, IDD_ERROR, lpmd->szModemNack, sizeof(lpmd->szModemNack) );
         GetDlgItemText( hwnd, IDD_CONNECT_MESSAGE, lpmd->szConnectMessage, sizeof(lpmd->szConnectMessage) );
         GetDlgItemText( hwnd, IDD_CONNECT_FAIL, lpmd->szConnectFail, sizeof(lpmd->szConnectFail) );
         GetDlgItemText( hwnd, IDD_LINEBUSY, lpmd->szLineBusy, sizeof(lpmd->szLineBusy) );
         EndDialog( hwnd, TRUE );
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}

/* This function handles the Save Modem Settings dialog
 */
BOOL EXPORT CALLBACK SaveModemSettings( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = SaveModemSettings_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the SaveModemSettings
 * dialog.
 */
LRESULT FASTCALL SaveModemSettings_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, SaveModemSettings_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, SaveModemSettings_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSAVEMODEMSETTINGS );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL SaveModemSettings_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   Edit_LimitText( GetDlgItem( hwnd, IDOK ), 99 );
   EnableControl( hwnd, IDOK, FALSE );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL SaveModemSettings_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EDIT:
         if( codeNotify == EN_UPDATE )
            EnableControl( hwnd, IDOK, GetWindowTextLength( hwndCtl ) > 0 );
         break;

      case IDOK: {
         char szPrivIniFile[ _MAX_PATH ];
         char szModemName[ 100 ];
         HWND hwndParent;
         HWND hwndEdit;
         HWND hwndList;
         int index;

         /* Compute the path to the MODEMS.INI file.
          */
         strcat( strcpy( szPrivIniFile, pszAmeolDir ), szModemsIni );

         /* Add this string to the Modem Settings dialog drop down list of
          * modems. If this string already exists, query the user before
          * replacing it.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         hwndParent = GetParent( hwnd );
         hwndList = GetDlgItem( hwndParent, IDD_LIST );
         Edit_GetText( hwndEdit, szModemName, sizeof( szModemName ) );
         if( ( index = ComboBox_RealFindStringExact( hwndList, szModemName ) ) != CB_ERR )
            {
            char sz[ 140 ];

            wsprintf( sz, GS( IDS_STR49 ), (LPSTR)szModemName );
            if( fMessageBox( hwnd, 0, sz, MB_YESNO|MB_ICONQUESTION ) == IDNO )
               break;
            }
         else
            index = ComboBox_AddString( hwndList, szModemName );
         ComboBox_SetCurSel( hwndList, index );

         /* Now read the settings from the Modem Settings dialog and save them
          * to the MODEMS.INI file under the new title.
          */
         UpdateModemSetting( hwndParent, IDD_INITIALISE, szModemName, szmsctInitialise, MS_INITIALISE, szPrivIniFile );
         UpdateModemSetting( hwndParent, IDD_RESET, szModemName, szmsctReset, MS_RESET, szPrivIniFile );
         UpdateModemSetting( hwndParent, IDD_DIALTONE, szModemName, szmsctDialTone, MS_DIALTONE, szPrivIniFile );
         UpdateModemSetting( hwndParent, IDD_DIALPULSE, szModemName, szmsctDialPulse, MS_DIALPULSE, szPrivIniFile );
         UpdateModemSetting( hwndParent, IDD_HANGUP, szModemName, szmsctHangup, MS_HANGUP, szPrivIniFile );
         UpdateModemSetting( hwndParent, IDD_SUFFIX, szModemName, szmsctSuffix, MS_SUFFIX, szPrivIniFile );
         UpdateModemSetting( hwndParent, IDD_ESCAPE, szModemName, szmsctEscape, MS_ESCAPE, szPrivIniFile );
         UpdateModemSetting( hwndParent, IDD_MODEM_ACK, szModemName, szmsctModemAck, MS_MODEMACK, szPrivIniFile );
         UpdateModemSetting( hwndParent, IDD_ERROR, szModemName, szmsctModemNack, MS_MODEMNACK, szPrivIniFile );
         UpdateModemSetting( hwndParent, IDD_CONNECT_MESSAGE, szModemName, szmsctConnectMessage, MS_CONNECTOK, szPrivIniFile );
         UpdateModemSetting( hwndParent, IDD_CONNECT_FAIL, szModemName, szmsctConnectFail, MS_CONNECTFAIL, szPrivIniFile );
         UpdateModemSetting( hwndParent, IDD_LINEBUSY, szModemName, szmsctLineBusy, MS_LINEBUSY, szPrivIniFile );
         EndDialog( hwnd, TRUE );
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}

/* This function reads a setting field for the specified modem and
 * writes the field to the MODEMS.INI file unless it matches the
 * default in which case it deletes any existing field.
 */
void FASTCALL UpdateModemSetting( HWND hwnd, int id, char * pModemName, char * pIniEntry, char * pDefEntry, char * pIniFile )
{
   char sz[ 100 ];

   GetDlgItemText( hwnd, id, sz, sizeof( sz ) );
   if( _strcmpi( sz, pDefEntry ) )
      WritePrivateProfileString( pModemName, pIniEntry, sz, pIniFile );
   else
      WritePrivateProfileString( pModemName, pIniEntry, NULL, pIniFile );
}

/* This function fills the specified combo box with the list
 * of modems in the MODEMS.INI file.
 */
void FASTCALL FillModemList( HWND hwnd, int id )
{
   HWND hwndList;
   HNDFILE fh;

   if( ( fh = Amfile_Open( szModemsIni, AOF_READ ) ) == HNDFILE_ERROR )
      FileOpenError( hwnd, (LPSTR)szModemsIni, FALSE, FALSE );
   else
      {
      char sz[ 100 ];
      LPLBF hBuffer;

      hBuffer = Amlbf_Open( fh, AOF_READ );
      hwndList = GetDlgItem( hwnd, id );
      ComboBox_ResetContent( hwndList );
      while( Amlbf_Read( hBuffer, sz, sizeof( sz ) - 1, NULL, NULL, &fIsAmeol2Scratchpad ) )
         if( *sz == '[' )
            {
            sz[ strlen( sz ) - 1 ] = '\0';
            ComboBox_AddString( hwndList, sz+1 );
            }
      Amlbf_Close( hBuffer );
      }
}

/* This function fills the specified combo box with the list
 * of ports on this computer.
 */
void FASTCALL FillPortsList( HWND hwnd, int id )
{
   char * pPorts;
   HWND hwndCombo;
   register int c;
   int len;

   GetProfileString( "ports", NULL, "", pPorts = lpTmpBuf, LEN_TEMPBUF );
   hwndCombo = GetDlgItem( hwnd, id );
   for( c = 0; *pPorts; pPorts += len + 1 )
      if( _strnicmp( pPorts, "COM", 3 ) == 0 )
         {
         len = strlen( pPorts );
         if( pPorts[ len - 1 ] == ':' )
            pPorts[ len - 1 ] = '\0';
         ComboBox_AddString( hwndCombo, pPorts );
         ++c;
         }
      else
         len = strlen( pPorts );
   ComboBox_SetCurSel( hwndCombo, 0 );
}

/* This function fills the specified combo box with the list
 * of ports on this computer.
 */
void FASTCALL FillSpeedsList( HWND hwnd, int id )
{
   HWND hwndCombo;
   register int c;

   hwndCombo = GetDlgItem( hwnd, id );
   for( c = 0; c < cModemBauds; ++c )
      {
      char sz[ 14 ];

      wsprintf( sz, "%lu", dwModemBauds[ c ] );
      ComboBox_InsertString( hwndCombo, -1, sz );
      }
   ComboBox_SetCurSel( hwndCombo, 5 );
}

/* This function reads one segment of an IP address (eg. 193,188.69.4)
 * from a string. It expects the delimiters to be periods.
 *
 * If no error, returns the value of the segment and sets the BOOL pointed
 * to by npfError to TRUE. Otherwise it sets the BOOL to FALSE and the error
 * return result is undefined. The character pointer, p, is updated to point
 * to the start of the next segment.
 */
int FASTCALL ReadIPNumber( LPSTR * lpp, BOOL * npfError )
{
   LPSTR lpsz = *lpp;
   int v = 0;

   while( *lpsz == ' ' )
      ++lpsz;
   while( *lpsz && *lpsz != '.' )
      {
      if( *lpsz != ' ' )
         {
         if( !isdigit( *lpsz ) )
            {
            *npfError = FALSE;
            break;
            }
         v = v * 10 + ( *lpsz - '0' );
         }
      ++lpsz;
      }
   if( *lpsz == '.' )
      ++lpsz;
   *lpp = lpsz;
   return( v );
}
