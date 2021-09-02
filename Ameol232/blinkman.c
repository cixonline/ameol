/* BLINKMAN.C - Implements the Blink Manager
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
#include <string.h>
#include "command.h"
#include "toolbar.h"
#include "intcomms.h"
#include "blinkman.h"
#include <ctype.h>
#include "ini.h"

#define  THIS_FILE   __FILE__

static BOOL fDefDlgEx = FALSE;      /* DefDlg recursion flag trap */

BOOL FASTCALL NewBlinkEntryDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL NewBlinkEntryDlg_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL NewBlinkEntryDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );
BOOL EXPORT CALLBACK NewBlinkEntryDlg( HWND, UINT, WPARAM, LPARAM );

LPBLINKENTRY lpbeFirst = NULL;               /* First blink entry entry */

static UINT iCommandID = IDM_BLINKMANFIRST;     /* Blink command ID */
static LPBLINKENTRY lpbeThis;             /* Current blink entry */
static HMENU hBlinkMenu;                  /* Handle of blink menu */
static BOOL fBlinkListLoaded = FALSE;        /* TRUE if blink list read from registry */

int idRasTab;                 /* Index of RAS tab. */

void FASTCALL Comms_Blink_SelChange( HWND );
void FASTCALL SaveBlinkList( void );
void FASTCALL SaveBlinkListItem( LPBLINKENTRY );
void FASTCALL SaveOneBlinkListItem( LPBLINKENTRY );
void FASTCALL DeleteBlinkManagerEntry( LPBLINKENTRY );
BOOL FASTCALL BlinkmanProperties( HWND, LPBLINKENTRY );
LPBLINKENTRY FASTCALL AddBlinkToCommandTable( LPSTR, DWORD, LPSTR, WORD, BOOL );
LPBLINKENTRY FASTCALL AddBlinkToList( LPSTR, DWORD, LPSTR, UINT, BOOL );

BOOL FASTCALL Comms_Blink_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL Comms_Blink_OnCommand( HWND, int, HWND, UINT );
void FASTCALL Comms_Blink_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
void FASTCALL Comms_Blink_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
LRESULT FASTCALL Comms_Blink_OnNotify( HWND, int, LPNMHDR );

/* List of default buttons and their indexes
 * in the grid bitmap array.
 */
struct tagBLINK_BUTTON_ARRAY {
   char * pszBlinkName;
   int iBmpIndex;
} BlinkButtonArray[] = {
   "Full",                 0,
   "Mail Only",            2,
   "Upload Only",          3,
   "CIX Forums Only",         4,
   "CIX Forums Via Dial-Up",  4,
   "Internet Only",        5
   };

#define  maxBlinkButtonArray  (sizeof(BlinkButtonArray)/sizeof(BlinkButtonArray[0]))

static BOOL fRasFill = FALSE;

BOOL EXPORT CALLBACK RASBlinkProperties( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL RASBlinkProperties_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL RASBlinkProperties_OnNotify( HWND, int, LPNMHDR );
void FASTCALL RASBlinkProperties_OnCommand( HWND, int, HWND, UINT );

UINT CALLBACK EXPORT BlinkmanPropertiesCallbackFunc( HWND, UINT, LPARAM );

/* This function enables or disables all the blink buttons.
 */
void FASTCALL EnableBlinkButtons( BOOL fEnable )
{
   UINT iCommand;

   for( iCommand = IDM_BLINKMANFIRST; iCommand < iCommandID; ++iCommand )
      SendMessage( hwndToolBar, TB_ENABLEBUTTON, iCommand, fEnable );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_CUSTOMBLINK, fEnable );
}

/* This function enables or disables all the blink menu commands.
 */
void FASTCALL EnableBlinkMenu( HMENU hMenu, BOOL fEnable )
{
   UINT iCommand;

   for( iCommand = IDM_BLINKMANFIRST; iCommand < iCommandID; ++iCommand )
      MenuEnable( hMenu, iCommand, fEnable );
   MenuEnable( hMenu, IDM_CUSTOMBLINK, fEnable );
}

/* This function reads all blink manager entries into a locally
 * linked list.
 */
void FASTCALL LoadBlinkList( void )
{
   LPSTR lpsz;

   INITIALISE_PTR(lpsz);

   /* First create the Blink menu.
    */
   hBlinkMenu = GetSubMenu( GetSubMenu( hMainMenu, 0 ), 12 );

   /* Get the list of blink manager entries and
    * install them into the command table.
    */
   if( fNewMemory( &lpsz, 8000 ) )
      {
      UINT c;

      Amuser_GetPPString( szBlinkman, NULL, "", lpsz, 8000 );
      for( c = 0; lpsz[ c ]; ++c )
         {
         char szBtnBmpFilename[ 64 ];
         LPCSTR pszBtnBmpFilename;
         char szConnCard[ 40 ];
         DWORD dwBlinkFlags;
         LPBLINKENTRY lpbe;
         RASDATA rd2;
         int index;
         LPSTR lp;

         /* Read the configuration setting
          */
         Amuser_GetPPString( szBlinkman, &lpsz[ c ], "", lpTmpBuf, LEN_TEMPBUF );
         lp = lpTmpBuf;

         /* Set the default RAS settings
          */
         rd2 = rdDef;

         /* Parse blink flags value.
          */
         lp = IniReadLong( lp, &dwBlinkFlags );
         lp = IniReadText( lp, szConnCard, sizeof(szConnCard)-1 );
         if( *lp )
            {
            lp = IniReadInt( lp, &rd2.fUseRAS );
            if( rd2.fUseRAS )
               {
               char szRASPassword[ 80 ];

               lp = IniReadText( lp, rd2.szRASEntryName, RAS_MaxEntryName );
               lp = IniReadText( lp, rd2.szRASUserName, UNLEN );
               lp = IniReadText( lp, szRASPassword, PWLEN );
               DecodeLine64( szRASPassword, rd2.szRASPassword, PWLEN );
               }
            }

         /* Get button bitmap details.
          */
         lp = IniReadText( lp, szBtnBmpFilename, sizeof(szBtnBmpFilename) );
         lp = IniReadInt( lp, &index );

         /* Convert to proper filename or index.
          */
         pszBtnBmpFilename = szBtnBmpFilename;
         if( '\0' == *pszBtnBmpFilename )
            pszBtnBmpFilename = BTNBMP_GRID;

         /* Store the settings.
          */
         if( *szConnCard == '\0' )
            strcpy( szConnCard, szCIXConnCard );
         if( strcmp( &lpsz[ c ], "Custom" ) == 0 )
            lpbe = AddBlinkToList( &lpsz[ c ], dwBlinkFlags, szConnCard, IDM_CUSTOMBLINK, FALSE );
         else {
            WORD wDefKey;

            wDefKey = 0;
            if( strcmp( &lpsz[ c ], "Full" ) == 0 )
               wDefKey = MAKEKEY(HOTKEYF_CONTROL,'T');
            lpbe = AddBlinkToCommandTable( &lpsz[ c ], dwBlinkFlags, szConnCard, wDefKey, FALSE );
            }

         /* Store RAS settings.
          */
         if( NULL != lpbe )
            lpbe->rd = rd2;
         c += strlen( &lpsz[ c ] );
         }
      FreeMemory( &lpsz );
      }

   /* If no blink entries, install some defaults.
    */
   if( NULL == lpbeFirst )
      {
      AddBlinkToCommandTable( "Full", BF_FULLCONNECT, szCIXConnCard, 0, TRUE );
      AddBlinkToList( "Custom", BF_POSTCIX|BF_GETCIX, szCIXConnCard, IDM_CUSTOMBLINK, TRUE );
      if( fUseInternet )
         AddBlinkToCommandTable( "Mail Only", BF_GETIPMAIL|BF_POSTIPMAIL, szCIXConnCard, 0, TRUE );
      AddBlinkToCommandTable( "Upload Only", BF_POSTIPMAIL|BF_POSTIPNEWS|BF_POSTCIX, szCIXConnCard, 0, TRUE );
      AddBlinkToCommandTable( "CIX Forums Only", BF_POSTCIX|BF_GETCIX, szCIXConnCard, 0, TRUE );
      if( fUseInternet )
         AddBlinkToCommandTable( "Internet Only", BF_IPFLAGS, szCIXConnCard, 0, TRUE );
      fBlinkListLoaded = FALSE;
      }
   else
      {
      /* Always make sure we have a Default blink
       * command.
       */
      if( 0 == GetBlinkCommandID( "Full" ) )
         AddBlinkToCommandTable( "Full", BF_FULLCONNECT, szCIXConnCard, MAKEKEY(HOTKEYF_CONTROL,'T'), TRUE );
      fBlinkListLoaded = TRUE;
      }
}

/* This function implements the Custom Blink command.
 */
void FASTCALL CmdCustomBlink( HWND hwnd )
{
   LPBLINKENTRY lpbe;

   /* Do nowt if already blinking.
    */
   if( fBlinking )
      return;

   /* Find the Custom command in the command table.
    */
   for( lpbe = lpbeFirst; lpbe; lpbe = lpbe->lpbeNext )
      if( strcmp( lpbe->szName, "Custom" ) == 0 )
         break;

   /* Not there? Create it from scratch.
    */
   if( NULL == lpbe )
      lpbe = AddBlinkToList( "Custom", BF_POSTCIX|BF_GETCIX, szCIXConnCard, IDM_CUSTOMBLINK, TRUE );

   /* Still there? Bring up Properties dialog on
    * its settings.
    */
   if( NULL != lpbe )
      if( BlinkmanProperties( hwnd, lpbe ) )
         BeginBlink( lpbe );
}

/* This function saves the blink manager list after it has
 * been edited.
 */
void FASTCALL SaveBlinkList( void )
{
   LPBLINKENTRY lpbe;

   Amuser_WritePPString( szBlinkman, NULL, NULL );
   for( lpbe = lpbeFirst; lpbe; lpbe = lpbe->lpbeNext )
      SaveOneBlinkListItem( lpbe );
   fBlinkListLoaded = TRUE;
}

/* This function saves one blink list item. If the list has
 * not been loaded, it saves all the entries.
 */
void FASTCALL SaveBlinkListItem( LPBLINKENTRY lpbe )
{
   if( !fBlinkListLoaded )
      SaveBlinkList();
   else
      SaveOneBlinkListItem( lpbe );
}

/* This function saves the configuration for one blink list
 * item.
 */
void FASTCALL SaveOneBlinkListItem( LPBLINKENTRY lpbe )
{
   char szRASPassword[ LEN_PASSWORD * 3 ];
   char * lp;

   lp = lpTmpBuf;
   lp = IniWriteValue( lp, lpbe->dwBlinkFlags );
   *lp++ = ',';
   lp = IniWriteText( lp, lpbe->szConnCard );
   *lp++ = ',';
   lp = IniWriteValue( lp, lpbe->rd.fUseRAS );
   if( lpbe->rd.fUseRAS )
      {
      *lp++ = ',';
      lp = IniWriteText( lp, lpbe->rd.szRASEntryName );
      *lp++ = ',';
      lp = IniWriteText( lp, lpbe->rd.szRASUserName );
      *lp++ = ',';
      EncodeLine64( lpbe->rd.szRASPassword, LEN_PASSWORD, szRASPassword );
      lp = IniWriteText( lp, szRASPassword );
      }
   Amuser_WritePPString( szBlinkman, lpbe->szName, lpTmpBuf );
}

/* This function installs the specified blink manager entry
 * in the Ameol command table. Up to 1000 simultaneous commands
 * can be accommodated - enough for most.
 */
LPBLINKENTRY FASTCALL AddBlinkToCommandTable( LPSTR lpszName, DWORD dwBlinkFlags, LPSTR lpszConnCard, WORD wDefKey, BOOL fNew )
{
   LPBLINKENTRY lpbeNew;

   if( iCommandID > IDM_BLINKMANLAST )
      iCommandID = IDM_BLINKMANFIRST;
   lpbeNew = AddBlinkToList( lpszName, dwBlinkFlags, lpszConnCard, iCommandID, fNew );
   if( NULL != lpbeNew )
      {
      CMDREC cmd;
      int index;
      int c;

      /* Ensure we have a Blink category.
       */
      if( !CTree_FindCategory( CAT_BLINKS ) )
         CTree_AddCategory( CAT_BLINKS, (WORD)-1, "Connect" );

      /* Next, create an entry in the command table unless it is
       * the Custom or Full entry.
       */
      ++iCommandID;
      cmd.iCommand = lpbeNew->iCommandID;
      cmd.lpszString = lpbeNew->szName;
      cmd.lpszCommand = lpbeNew->szName;
      cmd.iDisabledString = 0;
      cmd.lpszTooltipString = lpbeNew->szName;
      cmd.iToolButton = TB_BLINK;
      cmd.wCategory = CAT_BLINKS;
      cmd.wDefKey = wDefKey;
      cmd.iActiveMode = WIN_ALL;
      cmd.hLib = NULL;
      cmd.wNewKey = 0;
      cmd.wKey = 0;
      cmd.nScheduleFlags = NSCHF_CANSCHEDULE;

      /* Set blink button bitmap.
       */
      index = fNewButtons ? 31 : 0;
      for( c = 0; c < maxBlinkButtonArray; ++c )
         if( strcmp( BlinkButtonArray[ c ].pszBlinkName, lpbeNew->szName ) == 0 )
            {
            index = fNewButtons ? BlinkButtonArray[ c ].iBmpIndex + 28 : BlinkButtonArray[ c ].iBmpIndex;
            break;
            }
      SetCommandCustomBitmap( &cmd, BTNBMP_GRID, index );

      /* Insert this command.
       */
      if( CTree_InsertCommand( &cmd ) )
         {
         char szMenuName[ 60 ];
         int nPos;

         /* Add to the Blink submenu.
          */
         strcpy( szMenuName, lpbeNew->szName );
         PutUniqueAccelerator( hBlinkMenu, szMenuName );
         nPos = GetMenuItemCount( hBlinkMenu ) - 2;
         InsertMenu( hBlinkMenu, nPos, MF_BYPOSITION|MF_STRING, lpbeNew->iCommandID, szMenuName );
         return( lpbeNew );
         }
      }
   return( NULL );
}

/* This function adds the specified blink entry to the linked list
 * of blink entries.
 */
LPBLINKENTRY FASTCALL AddBlinkToList( LPSTR lpszName, DWORD dwBlinkFlags, LPSTR lpszConnCard, UINT iCommand, BOOL fNew )
{
   LPBLINKENTRY lpbeNew;

   INITIALISE_PTR(lpbeNew);

   /* First create an entry for this blink in the list.
    */
   if( fNewMemory( &lpbeNew, sizeof(BLINKENTRY ) ) )
      {
      /* Link this entry into the list.
       */
      lpbeNew->lpbeNext = lpbeFirst;
      lpbeNew->lpbePrev = NULL;
      if( lpbeFirst )
         lpbeFirst->lpbePrev = lpbeNew;
      lpbeFirst = lpbeNew;

      /* Fill out the structure.
       */
      strcpy( lpbeNew->szName, lpszName );
      strcpy( lpbeNew->szConnCard, lpszConnCard );
      lpbeNew->iCommandID = iCommand;
      lpbeNew->dwBlinkFlags = dwBlinkFlags;

      /* For Internet type blinks, set the SMTP authentication
       * information.
       */
      if (fNew && ( dwBlinkFlags & BF_IPFLAGS) )
      {
         strcpy( lpbeNew->szAuthInfoUser, szSMTPMailLogin );
         memcpy( lpbeNew->szAuthInfoPass, szSMTPMailPassword, LEN_PASSWORD );
         SaveBlinkSMTPAuth( lpbeNew );
      }

      /* Fill out the RAS elements (Win32 only)
       * If this is a CIX only conn card and we connect to
       * CIX via dial-up, disable RAS.
       */
      lpbeNew->rd = rdDef;
      lpbeNew->rd.fUseRAS = FALSE;
      }
   return( lpbeNew );
}

void FASTCALL SaveBlinkSMTPAuth( BLINKENTRY * lpbe )
{
   char szAuthInfoPass[ 64 ];

   strcpy( lpTmpBuf, lpbe->szName );
   strcat( lpTmpBuf, "-SMTP User" );
   Amuser_WritePPString( szSMTPServers, lpTmpBuf, lpbe->szAuthInfoUser );

   EncodeLine64( lpbe->szAuthInfoPass, LEN_PASSWORD, szAuthInfoPass );

   strcpy( lpTmpBuf, lpbe->szName );
   strcat( lpTmpBuf, "-SMTP Password" );
   Amuser_WritePPString( szSMTPServers, lpTmpBuf, szAuthInfoPass);
}

/* This function modifies the Blink name to turn it into a valid menu string. It does
 * this by turning all letters in the name except the first to lower case. Then it
 * inserts an accelerator control character ('&') at an unique position in the string
 * such that no two menu items in the Blink menu have the same accelerator control
 * character.
 */
int FASTCALL PutUniqueAccelerator( HMENU hMenu, char * szMenuName )
{
   BOOL fAccelMap[ 36 ];
   register int c;
   char p2[ 40 ];
   char * np2;
   char * np3;
   int count;
   char ch;

   /* First specify that all letters and digits
    * are unassigned.
    */
   for( c = 0; c < 36; ++c )
      fAccelMap[ c ] = FALSE;

   /* Scan the blink list and look for accelerators
    * in the list.
    */
   count = GetMenuItemCount( hMenu );
   for( c = 0; c < count; ++c )
      {
      char sz[ 60 ];
      char * p;

      if( 0 <= GetMenuString( hMenu, c, sz, sizeof(sz), MF_BYPOSITION ) )
         for( p = sz; *p && *p != '\t'; ++p )
            if( *p == '&' && *( p + 1 ) != '&' )
               {
               ch = *( p + 1 );
               if( isdigit( ch ) )
                  fAccelMap[ 26 + ( ch - '0' ) ] = TRUE;
               else
                  fAccelMap[ toupper( ch ) - 'A' ] = TRUE;
               break;
               }
      }

   /* Okay, fAccelMap now has TRUE set for any character
    * already assigned an accelerator. Scan szMenuName and
    * set the first letter which has NOT been used as an
    * accelerator.
    */
   np2 = p2;
   np3 = szMenuName;
   while( ch = *np3 )
      {
      if( ch == '&' )
         *np2++ = '&';
      else if( isdigit( ch ) )
         {
         if( !fAccelMap[ 26 + ( ch - '0' ) ] )
            {
            *np2++ = '&';
            *np2++ = ch;
            ++np3;
            break;
            }
         }
      else if( isalpha( ch ) )
         {
         if( !fAccelMap[ toupper( ch ) - 'A' ] )
            {
            *np2++ = '&';
            *np2++ = ch;
            ++np3;
            break;
            }
         }
      if( np2 > p2 )
         ch = tolower( ch );
      *np2++ = ch;
      ++np3;
      }
   while( *np3 )
      *np2++ = *np3++;
   *np2 = '\0';
   strcpy( szMenuName, p2 );
   return( TRUE );
}

/* Delete the specified blink manager entry.
 */
void FASTCALL DeleteBlinkManagerEntry( LPBLINKENTRY lpbe )
{
   /* First unlink this entry from
    * the list.
    */
   if( lpbe->lpbePrev )
      lpbe->lpbePrev->lpbeNext = lpbe->lpbeNext;
   else
      lpbeFirst = lpbe->lpbeNext;
   if( lpbe->lpbeNext )
      lpbe->lpbeNext->lpbePrev = lpbe->lpbePrev;

   /* Delete the associated command from the
    * command table. This will also remove any
    * toolbar button.
    */
   CTree_DeleteCommand( lpbe->iCommandID );

   /* Delete from the Blink menu.
    */
   DeleteMenu( hBlinkMenu, lpbe->iCommandID, MF_BYCOMMAND );

   /* Delete from the registry.
    */
   Amuser_WritePPString( szBlinkman, lpbe->szName, NULL );

   /* Free the allocated memory.
    */
   FreeMemory( &lpbe );
}

/* Given the blink command name, this function retrieves the
 * blink command ID.
 */
UINT FASTCALL GetBlinkCommandID( LPSTR lpszName )
{
   LPBLINKENTRY lpbe;

   for( lpbe = lpbeFirst; lpbe; lpbe = lpbe->lpbeNext )
      if( strcmp( lpszName, lpbe->szName ) == 0 )
         return( lpbe->iCommandID );
   return( 0 );
}

/* Given a blink command ID, this function retrieves the
 * name of the blink command.
 */
BOOL FASTCALL GetBlinkCommandName( UINT iCommand, char * lpUsrBuf )
{
   LPBLINKENTRY lpbe;

   for( lpbe = lpbeFirst; lpbe; lpbe = lpbe->lpbeNext )
      if( iCommand == lpbe->iCommandID )
         {
         strcpy( lpUsrBuf, lpbe->szName );
         return( TRUE );
         }
   return( FALSE );
}

/* This function locates the blink manager entry assigned
 * to this command, then starts a blink using that entry.
 */
BOOL FASTCALL BlinkByCmd( UINT iCommand )
{
   LPBLINKENTRY lpbe;

   /* Do nowt if already blinking.
    */
   if( fBlinking )
      return( FALSE );

   for( lpbe = lpbeFirst; lpbe; lpbe = lpbe->lpbeNext )
      if( iCommand == lpbe->iCommandID )
         return( BeginBlink( lpbe ) );
   return( FALSE );
}

/* This function displays the properties dialog for the
 * specified blink command.
 */
void FASTCALL EditBlinkProperties( HWND hwnd, UINT iCommand )
{
   LPBLINKENTRY lpbe;

   for( lpbe = lpbeFirst; lpbe; lpbe = lpbe->lpbeNext )
      if( iCommand == lpbe->iCommandID )
         {
         BlinkmanProperties( hwnd, lpbe );
         break;
         }
}

/* This function handles the NewBlinkEntryDlg dialog box
 */
BOOL EXPORT CALLBACK NewBlinkEntryDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, NewBlinkEntryDlg_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the Login
 *  dialog.
 */
LRESULT FASTCALL NewBlinkEntryDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, NewBlinkEntryDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, NewBlinkEntryDlg_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsNEWBLINKENTRY );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL NewBlinkEntryDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   Edit_LimitText( GetDlgItem( hwnd, IDD_EDIT ), 39 );
   EnableControl( hwnd, IDOK, FALSE );
   SetWindowLong( hwnd, DWL_USER, lParam );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL NewBlinkEntryDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EDIT:
         if( codeNotify == EN_CHANGE )
            EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndCtl ) > 0 );
         break;

      case IDOK: {
         LPSTR lpText;
         HWND hwndEdit;

         lpText = (LPSTR)GetWindowLong( hwnd, DWL_USER );
         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         Edit_GetText( hwndEdit, lpText, 40 );
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* This function displays the property dialog the specified
 * blink entry.
 */
BOOL FASTCALL BlinkmanProperties( HWND hwnd, LPBLINKENTRY lpbe )
{
   AMPROPSHEETHEADER psh;
   int cPages;

   /* Initialise.
    */
   cPages = 0;

   /* Create the properties dialog.
    */
   psh.dwSize = sizeof( AMPROPSHEETHEADER );
   psh.dwFlags = PSH_PROPSHEETPAGE|PSH_PROPTITLE|PSH_USECALLBACK|PSH_HASHELP;
   if( fMultiLineTabs )
      psh.dwFlags |= PSH_MULTILINETABS;
   psh.hwndParent = hwnd;
   psh.hInstance = hInst;
   psh.pszIcon = NULL;
   psh.pszCaption = lpbe->szName;
   psh.nPages = cPages;
   psh.nStartPage = 0;
   psh.pfnCallback = BlinkmanPropertiesCallbackFunc;
   psh.ppsp = NULL;
   lpbeThis = lpbe;

   /* Display the dialog. The callback will fill out
    * any other pages.
    */
   if( Amctl_PropertySheet(&psh ) != -1 )
      {
      SaveBlinkListItem( lpbe );
      return( TRUE );
      }
   return( FALSE );
}

/* This function is called as soon as the property sheet has
 * finished initialising. We raise a AE_PREFSDIALOG event to allow
 * addons to attach their own property pages.
 */
UINT CALLBACK EXPORT BlinkmanPropertiesCallbackFunc( HWND hwnd, UINT uMsg, LPARAM lParam )
{
   if( PSCB_INITIALIZED == uMsg )
      Amuser_CallRegistered( AE_BLINKPROPERTIESDIALOG, (LPARAM)(LPSTR)hwnd, (LPARAM)lpbeThis );

   /* Set default selection.
    */
   PropSheet_SetCurSel( hwnd, 0L, 0 );
   return( 0 );
}

/* This function dispatches messages for the Blink dialog.
 */
BOOL EXPORT CALLBACK Comms_Blink( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, Comms_Blink_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, Comms_Blink_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_DRAWITEM, Comms_Blink_OnDrawItem );
      HANDLE_DLGMSG( hwnd, WM_MEASUREITEM, Comms_Blink_OnMeasureItem );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, Comms_Blink_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL Comms_Blink_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsCOMM_BLINK );
         break;

      case PSN_SETACTIVE:
         nLastCommsDialogPage = 0;
         break;

      case PSN_APPLY:
         fShowBlinkWindow = IsDlgButtonChecked( hwnd, IDD_SHOWBLINKWINDOW );
         Amuser_WritePPInt( szSettings, "ShowBlinkWindow", fShowBlinkWindow );
         break;
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL Comms_Blink_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndList;
   LPBLINKENTRY lpbe;

   /* Fill the list of available
    * blinks
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   for( lpbe = lpbeFirst; lpbe; lpbe = lpbe->lpbeNext )
      {
      int index;

      index = ListBox_AddString( hwndList, lpbe->szName );
      ListBox_SetItemData( hwndList, index, (LPARAM)lpbe );
      }
   ListBox_SetCurSel( hwndList, 0 );
   Comms_Blink_SelChange( hwnd );
   CheckDlgButton( hwnd, IDD_SHOWBLINKWINDOW, fShowBlinkWindow );
   return( TRUE );
}

/* The selection has changed in the Blink Manager dialog, so
 * update the status buttons.
 */
void FASTCALL Comms_Blink_SelChange( HWND hwnd )
{
   LPBLINKENTRY lpbe;
   HWND hwndList;
   int index;

   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   index = ListBox_GetCurSel( hwndList );
   ASSERT( index != LB_ERR );
   lpbe = (LPBLINKENTRY)ListBox_GetItemData( hwndList, index );
   ASSERT( 0L != lpbe );
   EnableControl( hwnd, IDD_REMOVE, ( strcmp( lpbe->szName, "Full" ) != 0 && strcmp( lpbe->szName, "Custom" ) != 0 ) );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL Comms_Blink_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_NEW: {
         LPBLINKENTRY lpbe;
         HWND hwndList;
         char sz[ 40 ];
         int index;

         /* First get the name for our new entry.
          */
         if( !Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_NEWBLINKENTRY), NewBlinkEntryDlg, (LPARAM)(LPSTR)sz ) )
            break;

         /* Next, create an entry with default attributes and
          * add it to the listbox.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         lpbe = AddBlinkToCommandTable( sz, BF_FULLCONNECT, szCIXConnCard, 0, FALSE );
         index = ListBox_AddString( hwndList, lpbe->szName );
         ListBox_SetItemData( hwndList, index, (LPARAM)lpbe );
         ListBox_SetCurSel( hwndList, index );

         /* Finally, fall thru to the Properties
          * dialog to edit the blink properties.
          * PS: Yuk!
          */
         codeNotify = LBN_DBLCLK;
         }

      case IDD_LIST:
         if( codeNotify == LBN_SELCHANGE )
            {
            Comms_Blink_SelChange( hwnd );
            break;
            }
         else if( codeNotify != LBN_DBLCLK )
            break;
         /* Fall thru to edit selected item.
          */

      case IDD_PROPERTIES: {
         LPBLINKENTRY lpbe;
         HWND hwndList;
         int index;

         /* Get the selected entry.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         index = ListBox_GetCurSel( hwndList );
         ASSERT( index != LB_ERR );
         lpbe = (LPBLINKENTRY)ListBox_GetItemData( hwndList, index );
         ASSERT( 0L != lpbe );
         BlinkmanProperties( GetParent( hwnd ), lpbe );
         break;
         }

      case IDD_REMOVE: {
         LPBLINKENTRY lpbe;
         HWND hwndList;
         int index;

         /* Get the selected entry.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         index = ListBox_GetCurSel( hwndList );
         ASSERT( index != LB_ERR );
         lpbe = (LPBLINKENTRY)ListBox_GetItemData( hwndList, index );
         ASSERT( 0L != lpbe );

         /* Cannot delete the Default blink command.
          */
         if( strcmp( lpbe->szName, "Full" ) == 0 )
            break;

         /* Ask before we chop.
          */
         wsprintf( lpTmpBuf, GS(IDS_STR988), lpbe->szName );
         if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONINFORMATION ) == IDYES )
            {
            int count;

            /* Delete the entry and also remove it from the
             * listbox and highlight the next entry.
             */
            DeleteBlinkManagerEntry( lpbe );
            count = ListBox_DeleteString( hwndList, index );
            if( count == index )
               --index;
            ListBox_SetCurSel( hwndList, index );
            }
         SetFocus( hwndList );
         break;
         }
      }
}

/* This function handles the WM_MEASUREITEM message
 */
void FASTCALL Comms_Blink_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   HFONT hFont;
   HBITMAP hbmpFont;
   BITMAP bmp;
   HDC hdc;

   hdc = GetDC( hwnd );
   hFont = SelectFont( hdc, hHelvB8Font );
   GetTextMetrics( hdc, &tm );
   hbmpFont = LoadBitmap( hRscLib, MAKEINTRESOURCE(IDB_BLINKMAN) );
   GetObject( hbmpFont, sizeof( BITMAP ), &bmp );
   DeleteBitmap( hbmpFont );
   SelectFont( hdc, hFont );
   ReleaseDC( hwnd, hdc );
   lpMeasureItem->itemHeight = max( tm.tmHeight + tm.tmExternalLeading + 3, bmp.bmHeight );
}

/* This function handles the WM_DRAWITEM message
 */
void FASTCALL Comms_Blink_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
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

void FASTCALL UpdateBlinkBitmaps( void )
{
   LPBLINKENTRY lpbe;

   for( lpbe = lpbeFirst; lpbe; lpbe = lpbe->lpbeNext )
   {

      CMDREC cmd;
      int index;
      int c;

      WORD wDefKey;
      WORD wMyKey;

      wDefKey = 0;
      wMyKey = 0;

      /* Next, create an entry in the command table unless it is
       * the Custom or Full entry.
       */
      cmd.iCommand = lpbe->iCommandID;
      cmd.iDisabledString = 0;
      cmd.lpszTooltipString = lpbe->szName;
      cmd.iToolButton = TB_BLINK;
      if( cmd.iCommand == IDM_CUSTOMBLINK )
         {
         cmd.lpszString = GS( IDS_CUSTOMBLINK );
         cmd.lpszCommand = GS( IDS_CUSTOMBLINK );
         cmd.wCategory = CAT_FILE_MENU;
         }
      else
         {
         cmd.lpszString = lpbe->szName;
         cmd.lpszCommand = lpbe->szName;
         cmd.wCategory = CAT_BLINKS;
         }
      if( CTree_GetCommandKey( lpbe->szName, &wMyKey ) )
         cmd.wDefKey = wMyKey;
      else if( strcmp( lpbe->szName, "Full" ) == 0 )
      {
         if( wMyKey == 0 )
            wDefKey = MAKEKEY(HOTKEYF_CONTROL,'T');
      }
      else
         cmd.wDefKey = wDefKey;
      cmd.iActiveMode = WIN_ALL;
      cmd.hLib = NULL;
      cmd.wNewKey = 0;
      cmd.wKey = 0;
      cmd.nScheduleFlags = NSCHF_CANSCHEDULE;

      /* Set blink button bitmap.
       */
      if( cmd.iCommand != IDM_CUSTOMBLINK )
      {
         index = fNewButtons ? 31 : 0;
         for( c = 0; c < maxBlinkButtonArray; ++c )
            if( strcmp( BlinkButtonArray[ c ].pszBlinkName, lpbe->szName ) == 0 )
               {
               index = fNewButtons ? BlinkButtonArray[ c ].iBmpIndex + 28 : BlinkButtonArray[ c ].iBmpIndex;
               break;
               }
         SetCommandCustomBitmap( &cmd, BTNBMP_GRID, index );
      }
      else
      {
         cmd.iToolButton = TB_CUSTOMBLINK;
//       strcpy( lpbe->szName, GS( IDS_CUSTOMBLINK ) );
      }
      CTree_PutCommand( &cmd );
      CTree_ChangeKey( cmd.iCommand, cmd.wDefKey );
   }

}
