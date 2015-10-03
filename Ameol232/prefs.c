/* PREFS.C - The preferences dialog
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

#define  OEMRESOURCE

#include "main.h"
#include "palrc.h"
#include "resource.h"
#include "amlib.h"
#include <string.h>
#include "intcomms.h"
#include "shrdob.h"
#include "rules.h"
#include "admin.h"
#include "editmap.h"

static BOOL fDefDlgEx = FALSE;   /* DefDlg recursion flag trap */

#define  THIS_FILE   __FILE__

BOOL EXPORT CALLBACK Prefs_P2( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL Prefs_P2_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL Prefs_P2_OnNotify( HWND, int, LPNMHDR );
void FASTCALL Prefs_P2_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK Prefs_P4( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL Prefs_P4_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL Prefs_P4_OnNotify( HWND, int, LPNMHDR );
void FASTCALL Prefs_P4_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK Prefs_P6( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL Prefs_P6_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL Prefs_P6_OnNotify( HWND, int, LPNMHDR );
void FASTCALL Prefs_P6_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK Prefs_P7( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL Prefs_P7_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL Prefs_P7_OnNotify( HWND, int, LPNMHDR );
void FASTCALL Prefs_P7_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK Prefs_P9( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL Prefs_P9_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL Prefs_P9_OnNotify( HWND, int, LPNMHDR );
void FASTCALL Prefs_P9_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK Attachments( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL Attachments_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL Attachments_OnNotify( HWND, int, LPNMHDR );
void FASTCALL Attachments_OnCommand( HWND, int, HWND, UINT );

void FASTCALL DrawCheckBox( HDC, int, int, int, SIZE * );
void FASTCALL GetAdvanceOption( HWND, int, char * );
void FASTCALL SetAdvanceOption( HWND, int, char * );

UINT CALLBACK EXPORT MainPrefsCallbackFunc( HWND, UINT, LPARAM );

struct tagADVANCE_OPTS {
   char * pszName;
   int nMode;
} szAdvanceOpts[] = {
   "Stay at current message",       ADVANCE_STAY,
   "Move to next message",          ADVANCE_NEXT,
   "Move to next unread message",   ADVANCE_NEXTUNREAD
   };

#define  cAdvanceOpts   (sizeof(szAdvanceOpts)/sizeof(szAdvanceOpts[0]))

int nLastOptionsDialogPage = ODU_GENERAL;
int nLastDirectoriesDialogPage = ODU_DIRECTORIES;
int cBrowserPage = 0;

int nLastPage;

BOOL fTmpShortHeadersSubject;
BOOL fTmpShortHeadersFrom;
BOOL fTmpShortHeadersTo;
BOOL fTmpShortHeadersCC;
BOOL fTmpShortHeadersDate;

/* This function displays the Directories dialog.
 */
BOOL FASTCALL DirectoriesDialog( HWND hwnd, int nPage )
{
   AMPROPSHEETPAGE psp[ 2 ];
   AMPROPSHEETHEADER psh;
   int cPages;

   cPages = 0;
   psp[ ODU_DIRASSOCIATE ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ ODU_DIRASSOCIATE ].dwFlags = PSP_USETITLE;
   psp[ ODU_DIRASSOCIATE ].hInstance = hInst;
   psp[ ODU_DIRASSOCIATE ].pszTemplate = MAKEINTRESOURCE( IDDLG_PREF_DIRASSOC );
   psp[ ODU_DIRASSOCIATE ].pszIcon = NULL;
   psp[ ODU_DIRASSOCIATE ].pfnDlgProc = DirAssociate;
   psp[ ODU_DIRASSOCIATE ].pszTitle = "Associations";
   psp[ ODU_DIRASSOCIATE ].lParam = 0L;
   ++cPages;

   psp[ ODU_DIRECTORIES ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ ODU_DIRECTORIES ].dwFlags = PSP_USETITLE;
   psp[ ODU_DIRECTORIES ].hInstance = hInst;
   psp[ ODU_DIRECTORIES ].pszTemplate = MAKEINTRESOURCE( IDDLG_PREF_DIRECTORIES );
   psp[ ODU_DIRECTORIES ].pszIcon = NULL;
   psp[ ODU_DIRECTORIES ].pfnDlgProc = Directories;
   psp[ ODU_DIRECTORIES ].pszTitle = "Directories";
   psp[ ODU_DIRECTORIES ].lParam = 0L;
   ++cPages;

   ASSERT( cPages <= (sizeof( psp ) / sizeof( AMPROPSHEETPAGE )) );
   psh.dwSize = sizeof( AMPROPSHEETHEADER );
   psh.dwFlags = PSH_PROPSHEETPAGE|PSH_HASHELP;
   if( fMultiLineTabs )
      psh.dwFlags |= PSH_MULTILINETABS;
   psh.hwndParent = hwnd;
   psh.hInstance = hInst;
   psh.pszIcon = NULL;
   psh.pszCaption = "Directories";
   psh.nPages = cPages;
   psh.nStartPage = nPage;
   psh.pfnCallback = NULL;
   psh.ppsp = (LPCAMPROPSHEETPAGE)&psp;
   return( Amctl_PropertySheet(&psh ) != -1 );
}

/* This function displays the Preferences dialog.
 */
BOOL FASTCALL PreferencesDialog( HWND hwnd, int nPage )
{
   AMPROPSHEETPAGE psp[ 8 ];
   AMPROPSHEETHEADER psh;
   int cPages;

   cPages = 0;
   psp[ ODU_GENERAL ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ ODU_GENERAL ].dwFlags = PSP_USETITLE;
   psp[ ODU_GENERAL ].hInstance = hInst;
   psp[ ODU_GENERAL ].pszTemplate = MAKEINTRESOURCE( IDDLG_PREF_GENERAL );
   psp[ ODU_GENERAL ].pszIcon = NULL;
   psp[ ODU_GENERAL ].pfnDlgProc = Prefs_P2;
   psp[ ODU_GENERAL ].pszTitle = "General";
   psp[ ODU_GENERAL ].lParam = 0L;
   ++cPages;

   psp[ ODU_READING ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ ODU_READING ].dwFlags = PSP_USETITLE;
   psp[ ODU_READING ].hInstance = hInst;
   psp[ ODU_READING ].pszTemplate = MAKEINTRESOURCE( IDDLG_PREF_READING );
   psp[ ODU_READING ].pszIcon = NULL;
   psp[ ODU_READING ].pfnDlgProc = Prefs_P6;
   psp[ ODU_READING ].pszTitle = "Reading";
   psp[ ODU_READING ].lParam = 0L;
   ++cPages;

   psp[ ODU_EDITOR ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ ODU_EDITOR ].dwFlags = PSP_USETITLE;
   psp[ ODU_EDITOR ].hInstance = hInst;
   psp[ ODU_EDITOR ].pszTemplate = MAKEINTRESOURCE( IDDLG_PREF_EDITOR );
   psp[ ODU_EDITOR ].pszIcon = NULL;
   psp[ ODU_EDITOR ].pfnDlgProc = Prefs_P7;
   psp[ ODU_EDITOR ].pszTitle = "Editor";
   psp[ ODU_EDITOR ].lParam = 0L;
   ++cPages;

   psp[ ODU_VIEWER ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ ODU_VIEWER ].dwFlags = PSP_USETITLE;
   psp[ ODU_VIEWER ].hInstance = hInst;
   psp[ ODU_VIEWER ].pszTemplate = MAKEINTRESOURCE( IDDLG_PREF_VIEWER );
   psp[ ODU_VIEWER ].pszIcon = NULL;
   psp[ ODU_VIEWER ].pfnDlgProc = Prefs_P4;
   psp[ ODU_VIEWER ].pszTitle = "Viewer";
   psp[ ODU_VIEWER ].lParam = 0L;
   ++cPages;

   psp[ ODU_ATTACHMENTS ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ ODU_ATTACHMENTS ].dwFlags = PSP_USETITLE;
   psp[ ODU_ATTACHMENTS ].hInstance = hInst;
   psp[ ODU_ATTACHMENTS ].pszTemplate = MAKEINTRESOURCE( IDDLG_PREF_ATTACHMENTS );
   psp[ ODU_ATTACHMENTS ].pszIcon = NULL;
   psp[ ODU_ATTACHMENTS ].pfnDlgProc = Attachments;
   psp[ ODU_ATTACHMENTS ].pszTitle = "Attachments";
   psp[ ODU_ATTACHMENTS ].lParam = 0L;
   ++cPages;

   psp[ ODU_PURGE ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ ODU_PURGE ].dwFlags = PSP_USETITLE;
   psp[ ODU_PURGE ].hInstance = hInst;
   psp[ ODU_PURGE ].pszTemplate = MAKEINTRESOURCE( IDDLG_PREF_PURGE );
   psp[ ODU_PURGE ].pszIcon = NULL;
   psp[ ODU_PURGE ].pfnDlgProc = DefaultPurgeOptions;
   psp[ ODU_PURGE ].pszTitle = "Purge";
   psp[ ODU_PURGE ].lParam = 0L;
   ++cPages;

   psp[ ODU_SPELLING ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ ODU_SPELLING ].dwFlags = PSP_USETITLE;
   psp[ ODU_SPELLING ].hInstance = hInst;
   psp[ ODU_SPELLING ].pszTemplate = MAKEINTRESOURCE( IDDLG_PREF_SPELLING );
   psp[ ODU_SPELLING ].pszIcon = NULL;
   psp[ ODU_SPELLING ].pfnDlgProc = SpellOpts;
   psp[ ODU_SPELLING ].pszTitle = "Spelling";
   psp[ ODU_SPELLING ].lParam = 0L;
   ++cPages;

   cBrowserPage = cPages;
   psp[ cPages ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ cBrowserPage ].dwFlags = PSP_USETITLE;
   psp[ cBrowserPage ].hInstance = hInst;
   psp[ cBrowserPage ].pszTemplate = MAKEINTRESOURCE( IDDLG_PREF_BROWSER );
   psp[ cBrowserPage ].pszIcon = NULL;
   psp[ cBrowserPage ].pfnDlgProc = Prefs_P9;
   psp[ cBrowserPage ].pszTitle = "Browser";
   psp[ cBrowserPage ].lParam = 0L;
   ++cPages;

   ASSERT( cPages <= (sizeof( psp ) / sizeof( AMPROPSHEETPAGE )) );
   psh.dwSize = sizeof( AMPROPSHEETHEADER );
   psh.dwFlags = PSH_PROPSHEETPAGE|PSH_USECALLBACK|PSH_HASHELP;
   if( fMultiLineTabs )
      psh.dwFlags |= PSH_MULTILINETABS;
   psh.hwndParent = hwnd;
   psh.hInstance = hInst;
   psh.pszIcon = NULL;
   psh.pszCaption = "Preferences";
   psh.nPages = cPages;
   psh.nStartPage = nPage;
   psh.pfnCallback = MainPrefsCallbackFunc;
   psh.ppsp = (LPCAMPROPSHEETPAGE)&psp;
   return( Amctl_PropertySheet(&psh ) != -1 );
}

/* This function is called as soon as the property sheet has
 * finished initialising. We raise a AE_PREFSDIALOG event to allow
 * addons to attach their own property pages.
 */
UINT CALLBACK EXPORT MainPrefsCallbackFunc( HWND hwnd, UINT uMsg, LPARAM lParam )
{
   if( PSCB_INITIALIZED == uMsg )
      Amuser_CallRegistered( AE_MAINPREFSDIALOG, (LPARAM)(LPSTR)hwnd, 0L );
   PropSheet_SetCurSel( hwnd, 0L, 0 );
   return( 0 );
}

/* This function dispatches messages for the General page of the
 * Preferences dialog.
 */
BOOL EXPORT CALLBACK Prefs_P2( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, Prefs_P2_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, Prefs_P2_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, Prefs_P2_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL Prefs_P2_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   /* Set the options.
    */
   CheckDlgButton( hwnd, IDD_WARNONEXIT, fWarnOnExit );
   CheckDlgButton( hwnd, IDD_WARNAFTERBLINK, fWarnAfterBlink );   
   CheckDlgButton( hwnd, IDD_SECUREOPEN, fSecureOpen );
   CheckDlgButton( hwnd, IDD_UNREADAFTERBLINK, fUnreadAfterBlink );
   CheckDlgButton( hwnd, IDD_BLINKANIMATION, fDoBlinkAnimation );
   CheckDlgButton( hwnd, IDD_USECPYMSGSUBJ, fUseCopiedMsgSubject );
   CheckDlgButton( hwnd, IDD_NOISES, fNoisy );
   CheckDlgButton( hwnd, IDD_USEMBUTTON, fUseMButton );
   CheckDlgButton( hwnd, IDD_MLT, fMultiLineTabs );

   /* Disable online stuff if no internet service.
    */
   if( !fUseInternet )
      {
      ShowEnableControl( hwnd, IDD_PAD1, FALSE );
      ShowEnableControl( hwnd, IDD_PAD2, FALSE );
      ShowEnableControl( hwnd, IDD_EDIT, FALSE );
      ShowEnableControl( hwnd, IDD_STARTONLINE, FALSE );
      ShowEnableControl( hwnd, IDD_DISCONNECTTIMEOUT, FALSE );
      }
   else
      {
      /* Show the disconnection timeout rate.
       */
      CheckDlgButton( hwnd, IDD_STARTONLINE, fStartOnline );
      CheckDlgButton( hwnd, IDD_DISCONNECTTIMEOUT, fDisconnectTimeout );
      EnableControl( hwnd, IDD_EDIT, fDisconnectTimeout );
      if( wDisconnectRate > 60000 )
         wDisconnectRate = 60000;
      SetDlgItemInt( hwnd, IDD_EDIT, wDisconnectRate / 1000, FALSE );
      }

   /* Disable Parse scratchpads while blinking if CIX service not installed
    */
   if( !fUseCIX )
   {
      ShowEnableControl( hwnd, IDD_PARSEDURINGBLINK, FALSE );
      ShowEnableControl( hwnd, IDD_ALERTONFAILURE, FALSE );
      
   }
   else
   {
      CheckDlgButton( hwnd, IDD_PARSEDURINGBLINK, fParseDuringBlink );
      CheckDlgButton( hwnd, IDD_ALERTONFAILURE, fAlertOnFailure );   
   }
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL Prefs_P2_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EDIT:
         if( codeNotify == EN_CHANGE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_DISCONNECTTIMEOUT:
         EnableControl( hwnd, IDD_EDIT, IsDlgButtonChecked( hwnd, id ) );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_MLT:
      case IDD_PARSEDURINGBLINK:
      case IDD_SECUREOPEN:
      case IDD_UNREADAFTERBLINK:
      case IDD_STARTONLINE:
      case IDD_WARNONEXIT:
      case IDD_WARNAFTERBLINK:
      case IDD_ALERTONFAILURE:
      case IDD_USECPYMSGSUBJ:
      case IDD_BLINKANIMATION:
      case IDD_NOISES:
      case IDD_USEMBUTTON:
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL Prefs_P2_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPREF_GENERAL );
         break;

      case PSN_SETACTIVE:
         nLastOptionsDialogPage = ODU_GENERAL;
         break;

      case PSN_APPLY: {
         UINT wTmpDisconnectRate = 60;

         /* First get the disconnect timeout rate
          */
         if( fUseInternet )
            {
            if( fDisconnectTimeout = IsDlgButtonChecked( hwnd, IDD_DISCONNECTTIMEOUT ) )
               {
               if( !GetDlgInt( hwnd, IDD_EDIT, &wTmpDisconnectRate, 5, 60 ) )
                  {
                  PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, ODU_GENERAL );
                  return( PSNRET_INVALID_NOCHANGEPAGE );
                  }
               
               /* We store the rate in milliseconds, so convert the seconds
                * to milliseconds.
                */
               wTmpDisconnectRate *= 1000;
               }
               fStartOnline = IsDlgButtonChecked( hwnd, IDD_STARTONLINE );
            }
         /* If secure open option checked, but no password, warn the
          * user.
          */
         if( IsDlgButtonChecked( hwnd, IDD_SECUREOPEN ) && BlankPassword( szLoginPassword ) )
            {
            wsprintf( lpTmpBuf, GS(IDS_STR983), GetActiveUsername() );
            if( IDNO == fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONINFORMATION ) )
               {
               PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, ODU_GENERAL );
               CheckDlgButton( hwnd, IDD_SECUREOPEN, FALSE );
               return( PSNRET_INVALID_NOCHANGEPAGE );
               }
            }

         /* Get the changed options.
          */
         fWarnOnExit = IsDlgButtonChecked( hwnd, IDD_WARNONEXIT );
         fWarnAfterBlink = IsDlgButtonChecked( hwnd, IDD_WARNAFTERBLINK );       
         fAlertOnFailure = IsDlgButtonChecked( hwnd, IDD_ALERTONFAILURE );

         fSecureOpen = IsDlgButtonChecked( hwnd, IDD_SECUREOPEN );
         fUnreadAfterBlink = IsDlgButtonChecked( hwnd, IDD_UNREADAFTERBLINK );
         fParseDuringBlink = IsDlgButtonChecked( hwnd, IDD_PARSEDURINGBLINK );
         fDoBlinkAnimation = IsDlgButtonChecked( hwnd, IDD_BLINKANIMATION );
         fUseCopiedMsgSubject = IsDlgButtonChecked( hwnd, IDD_USECPYMSGSUBJ );
         fNoisy = IsDlgButtonChecked( hwnd, IDD_NOISES );
         fUseMButton = IsDlgButtonChecked( hwnd, IDD_USEMBUTTON );
         fMultiLineTabs = IsDlgButtonChecked( hwnd, IDD_MLT );

         /* Save to the configuration file.
          */
         Amuser_WritePPInt( szSettings, "UseCopiedMsgSubject", fUseCopiedMsgSubject );
         Amuser_WritePPInt( szSettings, "StartOnline", fStartOnline );
         Amuser_WritePPInt( szSettings, "WarnOnExit", fWarnOnExit );
         Amuser_WritePPInt( szSettings, "OnlineTimeout", fDisconnectTimeout );
         Amuser_WritePPInt( szSettings, "OnlineTimeoutRate", wTmpDisconnectRate );
         Amuser_WritePPInt( szSettings, "SecureIcon", fSecureOpen );
         Amuser_WritePPInt( szSettings, "ParseDuringBlink", fParseDuringBlink );
         Amuser_WritePPInt( szSettings, "BlinkAnimation", fDoBlinkAnimation );
         Amuser_WritePPInt( szSettings, "ShowUnreadAfterBlink", fUnreadAfterBlink );
         Amuser_WritePPInt( szSettings, "MakeNoises", fNoisy );
         Amuser_WritePPInt( szSettings, "UnreadOnMiddleButton", fUseMButton );
         Amuser_WritePPInt( szSettings, "MultiLineTabs", fMultiLineTabs );
         Amuser_WritePPInt( szSettings, "WarnAfterBlink", fWarnAfterBlink );

         /* Is the disconnect timeout running? No, then suspend it
          * otherwise restart it with the new disconnect rate.
          */
         if( !fDisconnectTimeout )
            SuspendDisconnectTimeout();
         else if( wTmpDisconnectRate != wDisconnectRate )
            {
            wDisconnectRate = wTmpDisconnectRate;
            ResetDisconnectTimeout();
            }

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This function dispatches messages for the Viewing page of the
 * Preferences dialog.
 */
BOOL EXPORT CALLBACK Prefs_P4( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, Prefs_P4_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, Prefs_P4_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, Prefs_P4_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL Prefs_P4_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndEdit;

   /* Set the options.
    */
   CheckDlgButton( hwnd, IDD_SHOWFOLDERTYPE, fShowTopicType );
   CheckDlgButton( hwnd, IDD_SHOWCLOCK, fShowClock );
   CheckDlgButton( hwnd, IDD_DRAWTHREADLINES, fDrawThreadLines );
   CheckDlgButton( hwnd, IDD_COLOURQUOTE, fColourQuotes );
   CheckDlgButton( hwnd, IDD_SHOWMUGSHOT, fShowMugshot );
   CheckDlgButton( hwnd, IDD_WRKSPCSCROLL, fWorkspaceScrollbars );
   CheckDlgButton( hwnd, IDD_ALTMSG, fAltMsgLayout );
   CheckDlgButton( hwnd, IDD_SHORTHEADERS, fShortHeaders );
   EnableControl( hwnd, IDD_QUOTEPROMPT, fColourQuotes );
   EnableControl( hwnd, IDD_QUOTEDELIMITERS, fColourQuotes );
   EnableControl( hwnd, IDD_PAD4, fShowMugshot );
   EnableControl( hwnd, IDD_PAD3, fShowMugshot );
   EnableControl( hwnd, IDD_MUGSHOTLATENCY, fShowMugshot );
   fTmpShortHeadersSubject = Amuser_GetPPInt( szSettings, "ShortHeadersSubject", FALSE );
   fTmpShortHeadersFrom = Amuser_GetPPInt( szSettings, "ShortHeadersFrom", TRUE );
   fTmpShortHeadersTo = Amuser_GetPPInt( szSettings, "ShortHeadersTo", TRUE );
   fTmpShortHeadersCC = Amuser_GetPPInt( szSettings, "ShortHeadersCC", TRUE );
   fTmpShortHeadersDate = Amuser_GetPPInt( szSettings, "ShortHeadersDate", TRUE );


   /* Set the edit fields.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_DATEFORMAT );
   Edit_LimitText( hwndEdit, sizeof(szDateFormat) );
   Edit_SetText( hwndEdit, szDateFormat );

   hwndEdit = GetDlgItem( hwnd, IDD_THREADDEPTH );
   Edit_LimitText( hwndEdit, 3 );
   SetDlgItemInt( hwnd, IDD_THREADDEPTH, vdDef.nThreadIndentPixels, FALSE );

   hwndEdit = GetDlgItem( hwnd, IDD_MUGSHOTLATENCY );
   Edit_LimitText( hwndEdit, 3 );
   SetDlgItemInt( hwnd, IDD_MUGSHOTLATENCY, nMugshotLatency, FALSE );

   hwndEdit = GetDlgItem( hwnd, IDD_QUOTEDELIMITERS );
   Edit_LimitText( hwndEdit, LEN_QDELIM );
   Edit_SetText( hwndEdit, szQuoteDelimiters );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL Prefs_P4_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_DEFWINDOWS: {

         if( CloseOnlineWindows( hwnd, TRUE ) && CloseAllTerminals( hwnd, TRUE ) )
         {
         if( hwndToolBarOptions )
            SendMessage( hwndToolBarOptions, WM_CLOSE, 0, 0L );
         if( hwndStartWiz )
            SendMessage( hwndStartWiz, WM_CLOSE, 0, 0L );
         if( hwndStartWizGuide )
            SendMessage( hwndStartWizGuide, WM_CLOSE, 0, 0L );
         if( hwndStartWizProf )
            SendMessage( hwndStartWizProf, WM_CLOSE, 0, 0L );
         SendAllWindows( WM_CLOSE, 0, 0L );
         Amuser_WritePPString( "Windows", NULL, NULL );
         Amuser_WritePPString( "Message Viewer", NULL, NULL );
         OpenDefaultWindows();
         }
         break;
         }


      case IDD_ALTMSG:
      case IDD_SHORTHEADERS:
      case IDD_WRKSPCSCROLL:
      case IDD_SHOWFOLDERTYPE:
      case IDD_SHOWCLOCK:
      case IDD_DRAWTHREADLINES:
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_EDITLIST:
         CmdMugshotEdit( GetParent( hwnd ) );
         break;

      case IDD_EDSH: {
         CmdShortHeaderEdit( hwnd );
         if( fTmpShortHeadersSubject != fShortHeadersSubject ||
               fTmpShortHeadersFrom != fShortHeadersFrom ||
                  fTmpShortHeadersTo != fShortHeadersTo ||
                     fTmpShortHeadersCC != fShortHeadersCC ||
                         fTmpShortHeadersDate != fShortHeadersDate)
                        PropSheet_Changed( GetParent( hwnd ), hwnd );
         fTmpShortHeadersSubject = fShortHeadersSubject;
         fTmpShortHeadersFrom = fShortHeadersFrom;
         fTmpShortHeadersTo = fShortHeadersTo;
         fTmpShortHeadersCC = fShortHeadersCC;
         fTmpShortHeadersDate = fShortHeadersDate;
         break;
         }

      case IDD_COLOURQUOTE: {
         BOOL fChecked;

         fChecked = IsDlgButtonChecked( hwnd, IDD_COLOURQUOTE );
         EnableControl( hwnd, IDD_QUOTEPROMPT, fChecked );
         EnableControl( hwnd, IDD_QUOTEDELIMITERS, fChecked );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
         }

      case IDD_DATEFORMAT:
         if( codeNotify == EN_UPDATE )
            {
            char szFormat[ 30 ];
            AM_DATE date;
            AM_TIME time;
            char sz[ 80 ];

            /* Update the sample date format string using the
             * current date/time.
             */
            Edit_GetText( hwndCtl, szFormat, sizeof(szFormat) );
            Amdate_GetCurrentDate( &date );
            Amdate_GetCurrentTime( &time );
            Amdate_FormatFullDate( &date, &time, sz, szFormat );
            SetDlgItemText( hwnd, IDD_SAMPLE, sz );
            PropSheet_Changed( GetParent( hwnd ), hwnd );
            }
         break;

      case IDD_QUOTEDELIMITERS:
      case IDD_THREADDEPTH:
      case IDD_MUGSHOTLATENCY:
         if( codeNotify == EN_CHANGE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_SHOWMUGSHOT: {
         BOOL fStatus;

         /* Disable the mugshot options if required.
          */
         fStatus = IsDlgButtonChecked( hwnd, id );
         EnableControl( hwnd, IDD_PAD4, fStatus );
         EnableControl( hwnd, IDD_PAD3, fStatus );
         EnableControl( hwnd, IDD_MUGSHOTLATENCY, fStatus );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
         }
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL Prefs_P4_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_QUERYCANCEL: {
         fShortHeadersSubject = Amuser_GetPPInt( szSettings, "ShortHeadersSubject", FALSE );
         fShortHeadersFrom = Amuser_GetPPInt( szSettings, "ShortHeadersFrom", TRUE );
         fShortHeadersTo = Amuser_GetPPInt( szSettings, "ShortHeadersTo", TRUE );
         fShortHeadersCC = Amuser_GetPPInt( szSettings, "ShortHeadersCC", TRUE );
         fShortHeadersDate = Amuser_GetPPInt( szSettings, "ShortHeadersDate", TRUE );
         break;
         }

      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPREF_VIEWER );
         break;

      case PSN_SETACTIVE:
         nLastOptionsDialogPage = ODU_VIEWER;
         break;

      case PSN_APPLY: {
         BOOL fTmpWorkspaceScrollbars;
         BOOL fTmpAltMsgLayout;
         BOOL fTmpShortHeaders;
         BOOL fRedrawThreadWindows;
         BOOL fRedrawMessageWindows;
         BOOL fRedrawText;
         BOOL fTmpDrawThreadLines;
         BOOL fTmpColourQuotes;
         BOOL fOldShowTopicType;
         BOOL fOldShowClock;
         int nTmpThreadIndentPixels;
         int nTmpMugshotLatency;
         HWND hwndEdit;

         /* Initialise.
          */
         fRedrawThreadWindows = FALSE;
         fRedrawMessageWindows = FALSE;
         fRedrawText = FALSE;
         fOldShowClock = fShowClock;
         fOldShowTopicType = fShowTopicType;

         /* First get the thread indent.
          */
         if( !GetDlgInt( hwnd, IDD_THREADDEPTH, &nTmpThreadIndentPixels, 4, 20 ) )
            return( PSNRET_INVALID_NOCHANGEPAGE );

         /* First get the mugshot latency
          */
         if( !GetDlgInt( hwnd, IDD_MUGSHOTLATENCY, &nTmpMugshotLatency, 0, 60 ) )
            return( PSNRET_INVALID_NOCHANGEPAGE );

         /* Get the checkbox options.
          */
         fTmpWorkspaceScrollbars = IsDlgButtonChecked( hwnd, IDD_WRKSPCSCROLL );
         if( fTmpWorkspaceScrollbars != fWorkspaceScrollbars )
            {
            fWorkspaceScrollbars = fTmpWorkspaceScrollbars;
            Amuser_WritePPInt( szSettings, "WorkspaceScrollBars", fWorkspaceScrollbars );
            fMessageBox( hwnd, 0, GS(IDS_STR1182), MB_OK|MB_ICONINFORMATION );
            }
         fTmpAltMsgLayout = IsDlgButtonChecked( hwnd, IDD_ALTMSG );
         if( fTmpAltMsgLayout != fAltMsgLayout )
            {
            fAltMsgLayout = fTmpAltMsgLayout;
            Amuser_WritePPInt( szSettings, "AltMsgLayout", fAltMsgLayout );
            fMessageBox( hwnd, 0, GS(IDS_STR1244), MB_OK|MB_ICONINFORMATION );
            }
         fTmpDrawThreadLines = IsDlgButtonChecked( hwnd, IDD_DRAWTHREADLINES );
         if( fTmpDrawThreadLines != fDrawThreadLines )
            {
            fDrawThreadLines = fTmpDrawThreadLines;
            fRedrawThreadWindows = TRUE;
            }
         if( (WORD)nTmpThreadIndentPixels != vdDef.nThreadIndentPixels )
            {
            vdDef.nThreadIndentPixels = (WORD)nTmpThreadIndentPixels;
            fRedrawThreadWindows = TRUE;
            }

         /* Get the quote colour settings.
          */
         fTmpColourQuotes = IsDlgButtonChecked( hwnd, IDD_COLOURQUOTE );
         if( fTmpColourQuotes != fColourQuotes )
            {
            fColourQuotes = fTmpColourQuotes;
            fRedrawMessageWindows = TRUE;
            }
         hwndEdit = GetDlgItem( hwnd, IDD_QUOTEDELIMITERS );
         if( EditMap_GetModified( hwndEdit ) )
            {
            Edit_GetText( hwndEdit, szQuoteDelimiters, LEN_QDELIM+1 );
            fRedrawMessageWindows = TRUE;
            }

         /* Save the new date format.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_DATEFORMAT );
         if( EditMap_GetModified( hwndEdit ) )
            {
            Edit_GetText( hwndEdit, szDateFormat, sizeof(szDateFormat) );
            fRedrawThreadWindows = TRUE;
            }

         /* Deal with mugshot stuff.
          */
         fShowMugshot = IsDlgButtonChecked( hwnd, IDD_SHOWMUGSHOT );
         if( fShowMugshot )
            nMugshotLatency = nTmpMugshotLatency;
         EnableGallery();

         /* Other options.
          */
         fTmpShortHeaders = IsDlgButtonChecked( hwnd, IDD_SHORTHEADERS );
         if( fTmpShortHeaders != fShortHeaders )
            {
            fShortHeaders = fTmpShortHeaders;
            fRedrawText = TRUE;
            }

         /* Save the new settings.
          */
         Amuser_WritePPInt( szSettings, "ShortHeadersSubject", fShortHeadersSubject );
         Amuser_WritePPInt( szSettings, "ShortHeadersFrom", fShortHeadersFrom );
         Amuser_WritePPInt( szSettings, "ShortHeadersTo", fShortHeadersTo );
         Amuser_WritePPInt( szSettings, "ShortHeadersCC", fShortHeadersCC );
         Amuser_WritePPInt( szSettings, "ShortHeadersDate", fShortHeadersDate );

         fShowClock = IsDlgButtonChecked( hwnd, IDD_SHOWCLOCK );
         fShowTopicType = IsDlgButtonChecked( hwnd, IDD_SHOWFOLDERTYPE );

         fRedrawText = TRUE;

         /* If clock now shown or hidden, update the status bar.
          */
         if( fShowClock != fOldShowClock )
            {
            UpdateStatusBarWidths();
            if( IsWindowVisible( hwndStatus ) )
               {
               InvalidateRect( hwndStatus, NULL, TRUE );
               ShowUnreadMessageCount();
               }
            }

         /* If Show Folder Type has changed, need to redraw the
          * Treeview controls.
          */
         if( fShowTopicType != fOldShowTopicType )
            SendAllWindows( WM_REDRAWWINDOW, WIN_INBASK, 0L );

         /* Save to the configuration file.
          */
         Amuser_WritePPInt( szSettings, "ShowClock", fShowClock );
         Amuser_WritePPInt( szSettings, "ShortHeaders", fShortHeaders );
         Amuser_WritePPInt( szSettings, "ShowFolderType", fShowTopicType );
         Amuser_WritePPInt( szSettings, "ColourQuotedText", fColourQuotes );
         Amuser_WritePPInt( szSettings, "ThreadLines", fDrawThreadLines );
         Amuser_WritePPInt( szSettings, "ThreadIndentPixels", nTmpThreadIndentPixels );
         Amuser_WritePPString( szSettings, "QuoteDelimiters", szQuoteDelimiters );
         Amuser_WritePPInt( szSettings, "ShowMugshots", fShowMugshot );
         Amuser_WritePPInt( szSettings, "MugshotLatency", nMugshotLatency );
         Amuser_WritePPString( szSettings, "DateFormat", szDateFormat );

         /* Do we have to update the message viewer windows?
          */
         if( fRedrawThreadWindows )
            SendAllWindows( WM_CHANGEFONT, FONTYP_THREAD, 0L );
         if( fRedrawMessageWindows )
            SendAllWindows( WM_CHANGEFONT, FONTYP_MESSAGE, 0L );
         if( fRedrawText )
            SendAllWindows( WM_TOGGLEHEADERS, 0, 0L );

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
void FASTCALL CheckList_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   HFONT hFont;
   HBITMAP hbmp;
   BITMAP bmp;
   HDC hdc;

   hdc = GetDC( hwnd );
   hFont = SelectFont( hdc, hHelvB8Font );
   hbmp = LoadBitmap( NULL, MAKEINTRESOURCE(OBM_CHECKBOXES) );
   GetObject( hbmp, sizeof( BITMAP ), &bmp );
   GetTextMetrics( hdc, &tm );
   lpMeasureItem->itemHeight = max( tm.tmHeight, (bmp.bmHeight / 3)+2 );
   DeleteBitmap( hbmp );
   SelectFont( hdc, hFont );
   ReleaseDC( hwnd, hdc );
}

/* This function handles the WM_DRAWITEM message
 */
void FASTCALL Prefs_P4_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   if( lpDrawItem->itemID != -1 )
      {
      char sz[ 100 ];
      HFONT hOldFont;
      HBRUSH hbr;
      int nMode;
      SIZE size;
      int y;
      RECT rc;

      /* Save and fill the rectangle.
       */
      rc = lpDrawItem->rcItem;
      hbr = CreateSolidBrush( GetSysColor( COLOR_WINDOW ) );
      FillRect( lpDrawItem->hDC, &rc, hbr );
      DeleteBrush( hbr );

      /* Set the checkbox mode.
       */
      nMode = CXB_NORMAL;
      if( lpDrawItem->itemState & ODS_SELECTED )
         nMode |= CXB_SELECTED;

      /* Draw the checkbox.
       */
      DrawCheckBox( lpDrawItem->hDC, rc.left + 2, rc.top + 1, nMode, &size );
      rc.left += size.cx + 4;

      /* Get and draw the text.
       */
      hOldFont = SelectFont( lpDrawItem->hDC, hHelvB8Font );
      ListBox_GetText( lpDrawItem->hwndItem, lpDrawItem->itemID, sz );
      y = rc.top + ( ( rc.bottom - rc.top ) - size.cy ) / 2;
      ExtTextOut( lpDrawItem->hDC, rc.left, y, ETO_CLIPPED|ETO_OPAQUE, &rc, sz, strlen( sz ), NULL );

      /* Do we have focus?
       */
      if( lpDrawItem->itemState & ODS_FOCUS )
         DrawFocusRect( lpDrawItem->hDC, &lpDrawItem->rcItem );

      /* Restore things back to normal. */
      SelectFont( lpDrawItem->hDC, hOldFont );
      }
}

/* This function draws a checkbox bitmap.
 */
void FASTCALL DrawCheckBox( HDC hDC, int x, int y, int nMode, SIZE * pSize )
{
   HBITMAP hbmp;
   BITMAP bmp;
   int bx, by;
   HDC hdc2;
   HBITMAP hbmpOld;

   /* Get the system checkboxes bitmap and compute
    * the size.
    */
   hbmp = LoadBitmap( NULL, MAKEINTRESOURCE(OBM_CHECKBOXES) );
   GetObject( hbmp, sizeof( BITMAP ), &bmp );
   pSize->cx = bmp.bmWidth / 4;
   pSize->cy = bmp.bmHeight / 3;

   /* Set bx and by to the offsets of the bitmaps
    * that we're going to bitblt depending on the
    * state of fActive and fSelected
    */
   by = 0;
   bx = pSize->cx * nMode;
   hdc2 = CreateCompatibleDC( hDC );
   hbmpOld = SelectBitmap( hdc2, hbmp );
   BitBlt( hDC, x, y, pSize->cx, pSize->cy, hdc2, bx, by, SRCCOPY );
   SelectBitmap( hdc2, hbmpOld );
   DeleteDC( hdc2 );
   DeleteBitmap( hbmp );
}

/* This function dispatches messages for the Reading page of the
 * Preferences dialog.
 */
BOOL EXPORT CALLBACK Prefs_P6( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, Prefs_P6_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, Prefs_P6_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, Prefs_P6_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL Prefs_P6_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   SetDlgItemInt( hwnd, IDD_EDIT, irWrapCol, FALSE );
   Edit_LimitText( GetDlgItem( hwnd, IDD_EDIT ), 3 );

   /* Set the options
    */
   CheckDlgButton( hwnd, IDD_BEEP, fBeepAtEnd );
   CheckDlgButton( hwnd, IDD_LAPTOPKEYS, fLaptopKeys );
   CheckDlgButton( hwnd, IDD_AUTOCOLLAPSE, fAutoCollapse );

   /* Set the advancing options.
    */
   SetAdvanceOption( hwnd, IDD_ADVANCE_READ, "AdvanceRead" );
   SetAdvanceOption( hwnd, IDD_ADVANCE_READLOCK, "AdvanceReadLock" );
   SetAdvanceOption( hwnd, IDD_ADVANCE_BOOKMARK, "AdvanceBookmark" );
   SetAdvanceOption( hwnd, IDD_ADVANCE_KEEP, "AdvanceKeep" );
   SetAdvanceOption( hwnd, IDD_ADVANCE_DELETE, "AdvanceDelete" );
   SetAdvanceOption( hwnd, IDD_ADVANCE_WITHDRAW, "AdvanceWithdraw" );
   SetAdvanceOption( hwnd, IDD_ADVANCE_WATCH, "AdvanceWatch" );
   SetAdvanceOption( hwnd, IDD_ADVANCE_IGNORED, "AdvanceIgnore" );
   SetAdvanceOption( hwnd, IDD_ADVANCE_TAGGED, "AdvanceTagged" );
   SetAdvanceOption( hwnd, IDD_ADVANCE_PRIORITY, "AdvancePriority" );
   return( TRUE );
}

/* This function fills the combo box with a list of advance options and
 * selects the one specified by the name.
 */
void FASTCALL SetAdvanceOption( HWND hwnd, int id, char * pszName )
{
   int nAdvanceMode;
   register int c;
   HWND hwndList;
   int index;

   nAdvanceMode = Amuser_GetPPInt( szSettings, pszName, ADVANCE_NEXT );
   VERIFY( hwndList = GetDlgItem( hwnd, id ) );
   for( c = 0; c < cAdvanceOpts; ++c )
      {
      index = ComboBox_AddString( hwndList, szAdvanceOpts[ c ].pszName );
      ComboBox_SetItemData( hwndList, index, szAdvanceOpts[ c ].nMode );
      }
   index = RealComboBox_FindItemData( hwndList, -1, nAdvanceMode );
   ComboBox_SetCurSel( hwndList, index );
}

/* This function retrieves and sets the advance option.
 */
void FASTCALL GetAdvanceOption( HWND hwnd, int id, char * pszName )
{
   int nAdvanceMode;
   HWND hwndList;
   int index;

   VERIFY( hwndList = GetDlgItem( hwnd, id ) );
   index = ComboBox_GetCurSel( hwndList );
   ASSERT( index != CB_ERR );
   nAdvanceMode = (int)ComboBox_GetItemData( hwndList, index );
   Amuser_WritePPInt( szSettings, pszName, nAdvanceMode );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL Prefs_P6_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EDIT:
         if( codeNotify == EN_CHANGE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_ADVANCE_READ:
      case IDD_ADVANCE_READLOCK:
      case IDD_ADVANCE_BOOKMARK:
      case IDD_ADVANCE_KEEP:
      case IDD_ADVANCE_DELETE:
      case IDD_ADVANCE_WITHDRAW:
      case IDD_ADVANCE_WATCH:
      case IDD_ADVANCE_IGNORED:
      case IDD_ADVANCE_TAGGED:
      case IDD_ADVANCE_PRIORITY:
         if( codeNotify == CBN_SELCHANGE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_NOJUMPAFTERTAG:
      case IDD_LAPTOPKEYS:
      case IDD_AUTOCOLLAPSE:
      case IDD_BEEP:
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL Prefs_P6_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPREF_READING );
         break;

      case PSN_SETACTIVE:
         nLastOptionsDialogPage = ODU_READING;
         break;

      case PSN_APPLY: {
         if( !GetDlgInt( hwnd, IDD_EDIT, &irWrapCol, 40, 200 ) )
            {
            PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, ODU_EDITOR );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }

         /* Get the new options.
          */
         fBeepAtEnd = IsDlgButtonChecked( hwnd, IDD_BEEP );
         fLaptopKeys = IsDlgButtonChecked( hwnd, IDD_LAPTOPKEYS );
         fAutoCollapse = IsDlgButtonChecked( hwnd, IDD_AUTOCOLLAPSE );

         Amuser_WritePPInt( szSettings, "WordWrapRead", irWrapCol );

         /* Save the new settings.
          */
         Amuser_WritePPInt( szSettings, "BeepAtEnd", fBeepAtEnd );
         Amuser_WritePPInt( szSettings, "SwapPageKeyFunction", fLaptopKeys );
         Amuser_WritePPInt( szSettings, "AutoCollapseFolders", fAutoCollapse );

         /* Get and save the advance options.
          */
         GetAdvanceOption( hwnd, IDD_ADVANCE_READ, "AdvanceRead" );
         GetAdvanceOption( hwnd, IDD_ADVANCE_READLOCK, "AdvanceReadLock" );
         GetAdvanceOption( hwnd, IDD_ADVANCE_BOOKMARK, "AdvanceBookmark" );
         GetAdvanceOption( hwnd, IDD_ADVANCE_KEEP, "AdvanceKeep" );
         GetAdvanceOption( hwnd, IDD_ADVANCE_DELETE, "AdvanceDelete" );
         GetAdvanceOption( hwnd, IDD_ADVANCE_WITHDRAW, "AdvanceWithdraw" );
         GetAdvanceOption( hwnd, IDD_ADVANCE_WATCH, "AdvanceWatch" );
         GetAdvanceOption( hwnd, IDD_ADVANCE_IGNORED, "AdvanceIgnore" );
         GetAdvanceOption( hwnd, IDD_ADVANCE_TAGGED, "AdvanceTagged" );
         GetAdvanceOption( hwnd, IDD_ADVANCE_PRIORITY, "AdvancePriority" );

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This function dispatches messages for the Reading page of the
 * Preferences dialog.
 */
BOOL EXPORT CALLBACK Prefs_P7( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, Prefs_P7_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, Prefs_P7_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, Prefs_P7_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL Prefs_P7_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   /* Set the word wrap column.
    */
   SetDlgItemInt( hwnd, IDD_EDIT, iWrapCol, FALSE );
   Edit_LimitText( GetDlgItem( hwnd, IDD_EDIT ), 3 );

   SetDlgItemInt( hwnd, IDD_TABS, iTabSpaces, FALSE ); //FS#80
   Edit_LimitText( GetDlgItem( hwnd, IDD_TABS ), 3 );  //FS#80

   /* Set the options
    */
   CheckDlgButton( hwnd, IDD_AUTOINDENT, fAutoIndent );
   CheckDlgButton( hwnd, IDD_EDITATTOP, fEditAtTop );

   CheckDlgButton( hwnd, IDD_SHOWLINENUMBERS, fShowLineNumbers);   // 2.55.2031
   CheckDlgButton( hwnd, IDD_SHOWWRAPCHARACTER, fShowWrapCharacters); // 2.55.2031
// CheckDlgButton( hwnd, IDD_MULTILINEURLS, fMultiLineURLs);     // 2.55.2031
   CheckDlgButton( hwnd, IDD_SINGLECLICKHOTLINKE, fSingleClickHotspots); // 2.55.2031
   CheckDlgButton( hwnd, IDD_SHOWACRONYMS, fShowAcronyms); // 2.55.2033
   CheckDlgButton( hwnd, IDD_PAGEBEYONDEND, fPageBeyondEnd); // FS#57


   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL Prefs_P7_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_TABS:
      case IDD_EDIT:
         if( codeNotify == EN_CHANGE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_SHOWLINENUMBERS:     // 2.55.2031
      case IDD_SHOWWRAPCHARACTER:   // 2.55.2031
//    case IDD_MULTILINEURLS:       // 2.55.2031
      case IDD_SINGLECLICKHOTLINKE: // 2.55.2031
      case IDD_SHOWACRONYMS:        // FS#57
      case IDD_PAGEBEYONDEND:
      case IDD_EDITATTOP:
      case IDD_AUTOINDENT:
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
      }
}


/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL Prefs_P7_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPREF_EDITOR );
         break;

      case PSN_SETACTIVE:
         nLastOptionsDialogPage = ODU_EDITOR;
         break;

      case PSN_APPLY: {
         /* First get the disconnect timeout rate
          */
         if( !GetDlgInt( hwnd, IDD_EDIT, &iWrapCol, 40, 200 ) )
            {
            PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, ODU_EDITOR );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }
         if( !GetDlgInt( hwnd, IDD_TABS, &iTabSpaces, 1, 20 ) ) //FS#80
            {
            PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, ODU_EDITOR );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }
         
         /* Get the new options.
          */
         fAutoIndent = IsDlgButtonChecked( hwnd, IDD_AUTOINDENT );
         fEditAtTop = IsDlgButtonChecked( hwnd, IDD_EDITATTOP );

          fShowLineNumbers = IsDlgButtonChecked( hwnd, IDD_SHOWLINENUMBERS );   // 2.55.2031
//       fMultiLineURLs = IsDlgButtonChecked( hwnd,  IDD_MULTILINEURLS); // 2.55.2031
         fShowWrapCharacters = IsDlgButtonChecked( hwnd, IDD_SHOWWRAPCHARACTER );     // 2.55.2031
         fSingleClickHotspots = IsDlgButtonChecked( hwnd, IDD_SINGLECLICKHOTLINKE );     // 2.55.2031
         fShowAcronyms = IsDlgButtonChecked( hwnd, IDD_SHOWACRONYMS );     // 2.55.2033
         fPageBeyondEnd = IsDlgButtonChecked( hwnd, IDD_PAGEBEYONDEND );     // FS#57

         /* Save the new settings.
          */
         Amuser_WritePPInt( szSettings, "AutoIndent", fAutoIndent );
         Amuser_WritePPInt( szSettings, "EditAtTop", fEditAtTop );
         Amuser_WritePPInt( szSettings, "WordWrap", iWrapCol );
         Amuser_WritePPInt( szSettings, "TabSpaces", iTabSpaces ); //FS#80

          Amuser_WritePPInt( szSettings, "ShowLineNumbers", fShowLineNumbers );   // 2.55.2031
//       Amuser_WritePPInt( szSettings, "MultiLineURLs", fMultiLineURLs ); // 2.55.2031
         Amuser_WritePPInt( szSettings, "ShowWrapCharacters", fShowWrapCharacters );     // 2.55.2031
         Amuser_WritePPInt( szSettings, "SingleClickHotspots", fSingleClickHotspots );     // 2.55.2031
         Amuser_WritePPInt( szSettings, "ShowAcronyms", fShowAcronyms );     // 2.55.2034
         Amuser_WritePPInt( szSettings, "PageBeyondEnd", fPageBeyondEnd );     // FS#57
   
         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This function dispatches messages for the Browser page of the
 * Preferences dialog.
 */
BOOL EXPORT CALLBACK Prefs_P9( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, Prefs_P9_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, Prefs_P9_OnNotify );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, Prefs_P9_OnCommand );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL Prefs_P9_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   char szDefUser[ LEN_LOGINNAME + 1 ];

   /* Show the correct browser.
    */
   CheckDlgButton( hwnd, IDD_MSIE, fBrowserIsMSIE );
   CheckDlgButton( hwnd, IDD_OTHER, !fBrowserIsMSIE );
   if( !fBrowserIsMSIE )
      SetDlgItemText( hwnd, IDD_BROWSER, szBrowser );
   else
      {
      EnableControl( hwnd, IDD_BROWSER, FALSE );
      EnableControl( hwnd, IDD_BROWSE, FALSE );
      }

   /* Set the 'other' browser.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_BROWSER ), _MAX_PATH );

   /* Set the current username
    */
   Amuser_GetLMString( szInfo, "DefaultUser", "", szDefUser, sizeof( szDefUser ) );
   wsprintf( lpTmpBuf, GS(IDS_STR1250), *szDefUser ? szDefUser : GetActiveUsername() );
   SetDlgItemText( hwnd, IDD_CDEFUSER, lpTmpBuf );

   /* Hide the default mail/news commands if
    * we're not running the 16-bit version.
    */
   if (!fUseLegacyCIX)
   {
      ShowEnableControl( hwnd, IDD_PAD1, FALSE );
      ShowEnableControl( hwnd, IDD_DEFMAIL, FALSE );
      ShowEnableControl( hwnd, IDD_DEFNEWS, FALSE );
      ShowEnableControl( hwnd, IDD_DEFUSER, FALSE );
      ShowEnableControl( hwnd, IDD_CDEFUSER, FALSE );
   }

   /* Set the Browse picture button.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_BROWSE ), hInst, IDB_FILEBUTTON );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL Prefs_P9_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_OTHER:
      case IDD_MSIE: {
         BOOL fStatus;

         fStatus = IsDlgButtonChecked( hwnd, IDD_MSIE );
         EnableControl( hwnd, IDD_BROWSER, !fStatus );
         EnableControl( hwnd, IDD_BROWSE, !fStatus );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
         }

      case IDD_DEFMAIL:
         MakeAmeol2DefaultMailClient();
         EnableControl( hwnd, id, FALSE );
         break;

      case IDD_DEFNEWS:
         MakeAmeol2DefaultNewsClient();
         EnableControl( hwnd, id, FALSE );
         break;

      case IDD_DEFUSER:
         Amuser_WriteLMString( szInfo, "DefaultUser", GetActiveUsername() );
         wsprintf( lpTmpBuf, GS(IDS_STR1250), GetActiveUsername() );
         SetDlgItemText( hwnd, IDD_CDEFUSER, lpTmpBuf );
         EnableControl( hwnd, id, FALSE );
         break;

      case IDD_BROWSER:
         if( codeNotify == EN_CHANGE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_BROWSE: {
         static char strFilters[] = "Program Files (*.exe)\0*.exe\0All Files (*.*)\0*.*\0\0";
         OPENFILENAME ofn;
         HWND hwndEdit;

         BEGIN_PATH_BUF;
         hwndEdit = GetDlgItem( hwnd, IDD_BROWSER );
         Edit_GetText( hwndEdit, lpPathBuf, _MAX_PATH );
         if( ( strlen (lpPathBuf ) > 1 ) && ( lpPathBuf[ ( strlen( lpPathBuf ) - 1 ) ] == '\\' ) && ( lpPathBuf[ ( strlen( lpPathBuf ) - 2 ) ] != ':' ) )
            lpPathBuf[ ( strlen( lpPathBuf ) - 1 ) ] = '\0';
         
         // Strip the quotes                    //!!SM!! 2044
         ExtractFilename( lpPathBuf, lpPathBuf );
         // and check that the file exists         
         if( !Amfile_QueryFile( lpPathBuf ) )        //!!SM!! 2044
            lpPathBuf[0] = '\x0';

         ofn.lStructSize = sizeof( OPENFILENAME );
         ofn.hwndOwner = GetParent( hwnd );
         ofn.hInstance = NULL;
         ofn.lpstrFilter = strFilters;
         ofn.lpstrCustomFilter = NULL;
         ofn.nMaxCustFilter = 0;
         ofn.nFilterIndex = 1;
         ofn.lpstrFile = lpPathBuf;
         ofn.nMaxFile = _MAX_PATH;
         ofn.lpstrFileTitle = NULL;
         ofn.nMaxFileTitle = 0;
         ofn.lpstrInitialDir = NULL;
         ofn.lpstrTitle = "Choose Browser";
         ofn.Flags = OFN_NOCHANGEDIR|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
         ofn.nFileOffset = 0;
         ofn.nFileExtension = 0;
         ofn.lpstrDefExt = NULL;
         ofn.lCustData = 0;
         ofn.lpfnHook = NULL;
         ofn.lpTemplateName = 0;
         if( GetOpenFileName( &ofn ) )
            {
            Edit_SetText( hwndEdit, lpPathBuf );
            Edit_SetModify( hwndEdit, TRUE );
            }
         END_PATH_BUF;
         break;
         }
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL Prefs_P9_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPREF_BROWSER );
         break;

      case PSN_SETACTIVE:
         nLastOptionsDialogPage = cBrowserPage;
         break;

      case PSN_APPLY: {
         HWND hwndEdit;

         /* Get and save the new path.
          */
         fBrowserIsMSIE = IsDlgButtonChecked( hwnd, IDD_MSIE );
         if( !fBrowserIsMSIE )
            {
            hwndEdit = GetDlgItem( hwnd, IDD_BROWSER );
            if( EditMap_GetModified( hwndEdit ) )
               {
               Edit_GetText( hwndEdit, szBrowser, _MAX_PATH );
               StripLeadingTrailingSpaces( szBrowser );
               if( *szBrowser == '\0' )
                  {
                  fMessageBox( hwnd, 0, GS(IDS_STR1155), MB_OK|MB_ICONEXCLAMATION );
                  PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, cBrowserPage );
                  HighlightField( hwnd, IDD_BROWSER );
                  return( PSNRET_INVALID_NOCHANGEPAGE );
                  }
               QuotifyFilename( szBrowser );
               Amuser_WritePPString( szWWW, "Path", szBrowser );
               }
            }
         Amuser_WritePPInt( szWWW, "MSIE", fBrowserIsMSIE );

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This function dispatches messages for the Attachments page of the
 * Preferences dialog.
 */
BOOL EXPORT CALLBACK Attachments( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, Attachments_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, Attachments_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, Attachments_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL Attachments_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndList;
   register int i;
   int sel;

   /* Fill listbox with encoding schemes
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   sel = 0;
   for( i = 0; EncodingScheme[ i ].szDescription; ++i )
      if( EncodingScheme[ i ].wID != ENCSCH_BINARY )
         {
         int index;

         index = ComboBox_InsertString( hwndList, -1, EncodingScheme[ i ].szDescription );
         if( EncodingScheme[ i ].wID == wDefEncodingScheme )
            sel = i;
         ComboBox_SetItemData( hwndList, index, EncodingScheme[ i ].wID );
         }
   ComboBox_SetCurSel( hwndList, sel );

   /* Set the options.
    */
   CheckDlgButton( hwnd, IDD_SINGLEPART, !fSplitParts );
   CheckDlgButton( hwnd, IDD_MULTIPLEPARTS, fSplitParts );
   CheckDlgButton( hwnd, IDD_SIZEWARNING, fAttachmentSizeWarning );  
   CheckDlgButton( hwnd, IDD_CONVERTSPACES, fAttachmentConvertToShortName ); /*!!SM!!*/
   CheckDlgButton( hwnd, IDD_CHECKFORATTACH, fCheckForAttachments );         // !!SM!! 2.55.2033
   CheckDlgButton( hwnd, IDD_LAUNCHAFTERDECODE, fLaunchAfterDecode );
   CheckDlgButton( hwnd, IDD_SEPARATECOVER, fSeparateEncodedCover );
   CheckDlgButton( hwnd, IDD_PROMPTFILENAMS, fPromptForFilenames );
   CheckDlgButton( hwnd, IDD_STRIPHTML, fStripHTML );
   CheckDlgButton( hwnd, IDD_REPLACETEXT, fReplaceAttachmentText );
   CheckDlgButton( hwnd, IDD_BACKUPTEXT, fBackupAttachmentMail );
   EnableControl( hwnd, IDD_PAD1, fSplitParts );
   EnableControl( hwnd, IDD_EDIT, fSplitParts );
   EnableControl( hwnd, IDD_PAD2, fSplitParts );

   /* Set the max lines for multiple parts.
    */
   SetDlgItemLongInt( hwnd, IDD_EDIT, dwLinesMax );
   Edit_LimitText( GetDlgItem( hwnd, IDD_EDIT ), 5 );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL Attachments_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {

      case IDD_LIST:
         if( codeNotify == CBN_SELCHANGE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_MULTIPLEPARTS:
         EnableControl( hwnd, IDD_PAD1, TRUE );
         EnableControl( hwnd, IDD_EDIT, TRUE );
         EnableControl( hwnd, IDD_PAD2, TRUE );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_SINGLEPART:
         EnableControl( hwnd, IDD_PAD1, FALSE );
         EnableControl( hwnd, IDD_EDIT, FALSE );
         EnableControl( hwnd, IDD_PAD2, FALSE );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_REPLACETEXT:
      case IDD_BACKUPTEXT:
      case IDD_SIZEWARNING:
      case IDD_LAUNCHAFTERDECODE:
      case IDD_SEPARATECOVER:
      case IDD_PROMPTFILENAMS:
      case IDD_STRIPHTML:
      case IDD_CONVERTSPACES:
      case IDD_CHECKFORATTACH:
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_EDIT:
         if( codeNotify == EN_SETFOCUS )
            {
            CheckDlgButton( hwnd, IDD_SINGLEPART, FALSE );
            CheckDlgButton( hwnd, IDD_MULTIPLEPARTS, TRUE );
            }

      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL Attachments_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPREF_ATTACHMENTS );
         break;

      case PSN_SETACTIVE:
         nLastOptionsDialogPage = ODU_ATTACHMENTS;
         break;

      case PSN_APPLY: {
         HWND hwndList;
         int index;

         /* Get the attachment size field.
          */
         if( !GetDlgLongInt( hwnd, IDD_EDIT, &dwLinesMax, 10, 99999999 ) )
               return( PSNRET_INVALID_NOCHANGEPAGE );

         /* Get and save the options.
          */
         fSplitParts = IsDlgButtonChecked( hwnd, IDD_MULTIPLEPARTS );
         fAttachmentSizeWarning = IsDlgButtonChecked( hwnd, IDD_SIZEWARNING );
         fAttachmentConvertToShortName = IsDlgButtonChecked( hwnd, IDD_CONVERTSPACES );/*!!SM!!*/
           fCheckForAttachments = IsDlgButtonChecked( hwnd, IDD_CHECKFORATTACH );         // !!SM!! 2.55.2033
         fSeparateEncodedCover = IsDlgButtonChecked( hwnd, IDD_SEPARATECOVER );
         fLaunchAfterDecode = IsDlgButtonChecked( hwnd, IDD_LAUNCHAFTERDECODE );
         fPromptForFilenames = IsDlgButtonChecked( hwnd, IDD_PROMPTFILENAMS );
         fStripHTML = IsDlgButtonChecked( hwnd, IDD_STRIPHTML );
         fReplaceAttachmentText = IsDlgButtonChecked( hwnd, IDD_REPLACETEXT );
         fBackupAttachmentMail = IsDlgButtonChecked( hwnd, IDD_BACKUPTEXT );

         /* Get the encoding scheme
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         index = ComboBox_GetCurSel( hwndList );
         ASSERT( index != CB_ERR );
         wDefEncodingScheme = (WORD)ComboBox_GetItemData( hwndList, index );

         /* Save the new settings.
          */
         Amuser_WritePPInt( szSettings, "AttachmentCoverPage", fSeparateEncodedCover );
         Amuser_WritePPInt( szSettings, "SplitAttachment", fSplitParts );
         Amuser_WritePPInt( szSettings, "AttachmentSizeWarning", fAttachmentSizeWarning );
         Amuser_WritePPInt( szSettings, "AttachmentConvertToShortName", fAttachmentConvertToShortName ); /*!!SM!!*/
         Amuser_WritePPInt( szSettings, "CheckForAttachments", fCheckForAttachments ); /*!!SM!! 2.55.2036*/
         Amuser_WritePPLong( szSettings, "AttachmentPartMax", dwLinesMax );
         Amuser_WritePPInt( szSettings, "AutoLaunch", fLaunchAfterDecode );
         Amuser_WritePPInt( szSettings, "FilenamePrompt", fPromptForFilenames );
         Amuser_WritePPInt( szSettings, "EncodingScheme", wDefEncodingScheme );
         Amuser_WritePPInt( szSettings, "StripHTML", fStripHTML );
         Amuser_WritePPInt( szSettings, "ReplaceAttachmentText", fReplaceAttachmentText );
         Amuser_WritePPInt( szSettings, "BackupAttachmentMail", fBackupAttachmentMail );

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}
