/* CUSTOM.C - The Customise dialog
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
#include "amlib.h"
#include "string.h"
#include "command.h"
#include "admin.h"

#define  THIS_FILE   __FILE__

#define  LEN_KEYLISTITEM      80

BOOL EXPORT CALLBACK Customise_P1( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL Customise_P1_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL Customise_P1_OnNotify( HWND, int, LPNMHDR );
void FASTCALL Customise_P1_OnCommand( HWND, int, HWND, UINT );

int nLastCustomiseDialogPage = CDU_KEYBOARD;

void FASTCALL UpdateKeyList( HWND );
void FASTCALL GetKeyListItem( CMDREC FAR *, char * );

/* This function creates a new connection entry.
 */
BOOL FASTCALL CustomiseDialog( HWND hwnd, int nPage )
{
   AMPROPSHEETPAGE psp[ 3 ];
   AMPROPSHEETHEADER psh;

   ASSERT( nPage >= 0 && nPage < sizeof(psp) );

   /* Do nothing if not permitted to change
    * configuration.
    */
   if( !TestPerm(PF_CANCONFIGURE) )
      return( FALSE );

   psp[ CDU_KEYBOARD ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ CDU_KEYBOARD ].dwFlags = PSP_USETITLE;
   psp[ CDU_KEYBOARD ].hInstance = hInst;
   psp[ CDU_KEYBOARD ].pszTemplate = MAKEINTRESOURCE( IDDLG_CUSTOMISE_KEYBOARD );
   psp[ CDU_KEYBOARD ].pszIcon = NULL;
   psp[ CDU_KEYBOARD ].pfnDlgProc = Customise_P1;
   psp[ CDU_KEYBOARD ].pszTitle = "Keyboard";
   psp[ CDU_KEYBOARD ].lParam = 0L;

   psp[ CDU_COLOURS ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ CDU_COLOURS ].dwFlags = PSP_USETITLE;
   psp[ CDU_COLOURS ].hInstance = hInst;
   psp[ CDU_COLOURS ].pszTemplate = MAKEINTRESOURCE( IDDLG_CUSTOMISE_COLOURS );
   psp[ CDU_COLOURS ].pszIcon = NULL;
   psp[ CDU_COLOURS ].pfnDlgProc = Customise_Colour;
   psp[ CDU_COLOURS ].pszTitle = "Colours";
   psp[ CDU_COLOURS ].lParam = 0L;

   psp[ CDU_FONTS ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ CDU_FONTS ].dwFlags = PSP_USETITLE;
   psp[ CDU_FONTS ].hInstance = hInst;
   psp[ CDU_FONTS ].pszTemplate = MAKEINTRESOURCE( IDDLG_CUSTOMISE_FONTS );
   psp[ CDU_FONTS ].pszIcon = NULL;
   psp[ CDU_FONTS ].pfnDlgProc = FontSettings;
   psp[ CDU_FONTS ].pszTitle = "Fonts";
   psp[ CDU_FONTS ].lParam = 0L;

   psh.dwSize = sizeof( AMPROPSHEETHEADER );
   psh.dwFlags = PSH_PROPSHEETPAGE|PSH_HASHELP;
   if( fMultiLineTabs )
      psh.dwFlags |= PSH_MULTILINETABS;
   psh.hwndParent = hwnd;
   psh.hInstance = hInst;
   psh.pszIcon = NULL;
   psh.pszCaption = "Customise";
   psh.nPages = sizeof( psp ) / sizeof( AMPROPSHEETPAGE );
   psh.nStartPage = nPage;
   psh.ppsp = (LPCAMPROPSHEETPAGE)&psp;

   Amctl_PropertySheet(&psh );
   return( FALSE );
}

/* This function dispatches messages for the News page of the
 * Preferences dialog.
 */
BOOL EXPORT CALLBACK Customise_P1( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, Customise_P1_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, Customise_P1_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, Customise_P1_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL Customise_P1_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   CMDREC msi;
   HWND hwndList;
   LPCSTR lpsz;
   int tabstop = 120;
   HCMD hCmd;

   /* Make a copy of the current key assignment.
    */
   CTree_SaveKey();

   /* Fill the listbox with a list of all commands and
    * their keystrokes.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   UpdateKeyList( hwndList );

   /* Set the tab stops.
    */
   ListBox_SetTabStops( hwndList, 1, &tabstop );

   /* Show the first selection.
    */
   ListBox_SetCurSel( hwndList, 0 );
   hCmd = (HCMD)ListBox_GetItemData( hwndList, 0 );
   CTree_GetItem( hCmd, &msi );
   lpsz = msi.lpszString;
   if( HIWORD( lpsz ) == 0 )
      lpsz = GS( LOWORD( msi.lpszString ) );
   Edit_SetText( GetDlgItem( hwnd, IDD_DESCRIPTION ), lpsz );

   /* Disable the Reset All button if the keyboard not changed.
    * Disable the Assign button.
    */
   EnableControl( hwnd, IDD_RESET, fKeyboardChanged );
   EnableControl( hwnd, IDD_ASSIGN, FALSE );
   EnableControl( hwnd, IDD_REMOVE, msi.wKey != 0 );

   /* Set the hotkey.
    */
   SendMessage( GetDlgItem( hwnd, IDD_HOTKEY ), HKM_SETRULES, 0, HOTKEYF_CONTROL|HOTKEYF_ALT );
   SendMessage( GetDlgItem( hwnd, IDD_HOTKEY ), HKM_SETHOTKEY, 0, 0L );
   return( TRUE );
}

/* Fill the list box with a list of commands and their
 * associated short-cut keys.
 */
void FASTCALL UpdateKeyList( HWND hwndList )
{
   CMDREC msi;
   HCMD hCmd;

   hCmd = NULL;
   while( NULL != ( hCmd = CTree_EnumTree( hCmd, &msi ) ) )
      {
      char sz[ LEN_KEYLISTITEM ];
      int index;

      /* Create the key name and description.
       */
      GetKeyListItem( &msi, sz );
      index = ListBox_AddString( hwndList, sz );
      ListBox_SetItemData( hwndList, index, hCmd );
      }
}

/* Translate the key name for the specified item.
 * psz must point to a buffer of at least 80 bytes.
 */
void FASTCALL GetKeyListItem( CMDREC FAR * lpms, char * psz )
{
   lstrcpyn( psz, GINTRSC(lpms->lpszTooltipString), LEN_KEYLISTITEM - 1 );
   if( lpms->wNewKey )
      {
      int len;

      len = ( LEN_KEYLISTITEM - strlen( psz ) ) - 1;
      if( len > 1 )
         {
         strcat( psz, "\t" );
         DescribeKey( lpms->wNewKey, psz + strlen( psz ), len - 1 );
         }
      }
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL Customise_P1_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_ASSIGN: {
         char sz[ LEN_KEYLISTITEM ];
         CMDREC msi;
         HWND hwndHotkey;
         HWND hwndList;
         int index;
         HCMD hCmd;

         /* Get the selected item.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         index = ListBox_GetCurSel( hwndList );
         ASSERT( LB_ERR != index );

         /* Get the index of the selected item in the
          * CMDREC table.
          */
         hCmd = (HCMD)ListBox_GetItemData( hwndList, index );
         CTree_GetItem( hCmd, &msi );

         /* Get the hotkey from the input field.
          */
         hwndHotkey = GetDlgItem( hwnd, IDD_HOTKEY );
         msi.wNewKey = (WORD)SendMessage( hwndHotkey, HKM_GETHOTKEY, 0, 0L );
         if( msi.wNewKey < 0x80 )
            msi.iActiveMode = ( WIN_OUTBASK|WIN_THREAD|WIN_MESSAGE|WIN_INFOBAR|WIN_INBASK|WIN_RESUMES|WIN_SCHEDULE );
         CTree_SetItem( hCmd, &msi );

         /* Get the full text of the listbox item.
          */
         GetKeyListItem( &msi, sz );

         /* Set the wKey element to zero, then update the
          * listbox.
          */
         SetWindowRedraw( hwndList, FALSE );
         ListBox_DeleteString( hwndList, index );
         ListBox_InsertString( hwndList, index, sz );
         ListBox_SetItemData( hwndList, index, hCmd );
         SetWindowRedraw( hwndList, TRUE );
         ListBox_SetCurSel( hwndList, index );

         /* Clear the hotkey.
          */
         SendMessage( hwndHotkey, HKM_SETHOTKEY, 0, 0L );
         SetDlgItemText( hwnd, IDD_ASSIGNMENT, "" );

         /* Enable the RESET ALL and REMOVE buttons now.
          */
         EnableControl( hwnd, IDD_ASSIGN, FALSE );
         EnableControl( hwnd, IDD_REMOVE, TRUE );
         EnableControl( hwnd, IDD_RESET, TRUE );
         fKeyboardChanged = TRUE;
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         
         /* Set focus to the listbox or Ameol locks up when
            using the keyboard 
         */
         SetFocus( GetDlgItem( hwnd, IDD_LIST));         
         break;
         }

      case IDD_REMOVE: {
         CMDREC msi;
         HWND hwndList;
         int index;
         HCMD hCmd;

         /* Get the selected item.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         index = ListBox_GetCurSel( hwndList );
         ASSERT( LB_ERR != index );

         /* Get the index of the selected item in the
          * CMDREC table.
          */
         hCmd = (HCMD)ListBox_GetItemData( hwndList, index );
         CTree_GetItem( hCmd, &msi );

         /* Set the wKey element to zero, then update the
          * listbox.
          */
         msi.wNewKey = 0;
         CTree_SetItem( hCmd, &msi );
         SetWindowRedraw( hwndList, FALSE );
         ListBox_DeleteString( hwndList, index );
         ListBox_InsertString( hwndList, index, GINTRSC(msi.lpszTooltipString) );
         ListBox_SetItemData( hwndList, index, hCmd );
         SetWindowRedraw( hwndList, TRUE );
         ListBox_SetCurSel( hwndList, index );

         /* Enable the RESET ALL button now.
          */
         EnableControl( hwnd, IDD_RESET, TRUE );
         fKeyboardChanged = TRUE;
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
         }

      case IDD_RESET: {
         CMDREC msi;
         HWND hwndList;
         LPCSTR lpsz;
         HCMD hCmd;

         /* Reset the keys back to their defaults.
          */
         CTree_ResetKey();

         /* Update the listbox again.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         SetWindowRedraw( hwndList, FALSE );
         ListBox_ResetContent( hwndList );
         UpdateKeyList( hwndList );
         SetWindowRedraw( hwndList, TRUE );
         InvalidateRect( hwndList, NULL, TRUE );
         UpdateWindow( hwndList );

         /* Show the first selection.
          */
         ListBox_SetCurSel( hwndList, 0 );
         hCmd = (HCMD)ListBox_GetItemData( hwndList, 0 );
         CTree_GetItem( hCmd, &msi );
         lpsz = msi.lpszString;
         if( HIWORD( lpsz ) == 0 )
            lpsz = GS( LOWORD( msi.lpszString ) );
         Edit_SetText( GetDlgItem( hwnd, IDD_DESCRIPTION ), lpsz );
         EnableControl( hwnd, IDD_REMOVE, msi.wKey != 0 );
      
         /* The keyboard has now not changed.
          */
         EnableControl( hwnd, IDD_RESET, FALSE );
         fKeyboardChanged = FALSE;
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
         }

      case IDD_LIST:
         if( codeNotify == LBN_SELCHANGE )
            {
            CMDREC msi;
            LPCSTR lpsz;
            int index;
            HCMD hCmd;

            /* Update the description.
             */
            index = ListBox_GetCurSel( hwndCtl );
            hCmd = (HCMD)ListBox_GetItemData( hwndCtl, index );
            CTree_GetItem( hCmd, &msi );
            lpsz = msi.lpszString;
            if( HIWORD( lpsz ) == 0 )
               lpsz = GS( LOWORD( msi.lpszString ) );
            Edit_SetText( GetDlgItem( hwnd, IDD_DESCRIPTION ), lpsz );

            /* Clear the hotkey field.
             */
            SendMessage( GetDlgItem( hwnd, IDD_HOTKEY ), HKM_SETHOTKEY, 0, 0L );
            SetDlgItemText( hwnd, IDD_ASSIGNMENT, "" );

            /* Disable the Remove button if this command has no key.
             */
            EnableControl( hwnd, IDD_REMOVE, msi.wNewKey != 0 );
            }
         break;
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL Customise_P1_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsCUSTOMISE_KEYBOARD );
         break;

      case PSN_SETACTIVE:
         nLastCustomiseDialogPage = CDU_KEYBOARD;
         break;

      case HCN_SELCHANGED: {
         WORD wKey;
         BOOL fFound;

         /* Get the current hotkey. Beware that it may
          * already be incomplete.
          */
         wKey = (WORD)SendMessage( lpnmhdr->hwndFrom, HKM_GETHOTKEY, 0, 0L );

         /* Find and display the current assignment.
          */
         fFound = FALSE;
         if( wKey )
            {
            CMDREC msi;

            msi.wNewKey = wKey;
            if( CTree_FindNewKey( &msi ) )
               {
               SetDlgItemText( hwnd, IDD_ASSIGNMENT, GINTRSC(msi.lpszTooltipString) );
               EnableControl( hwnd, IDD_ASSIGN, FALSE );
               fFound = TRUE;
               }
            }
         if( 0 == wKey || !fFound )
            {
            if( GETKEY( wKey ) != 0 )
               {
               EnableControl( hwnd, IDD_ASSIGN, TRUE );
               SetDlgItemText( hwnd, IDD_ASSIGNMENT, "Unassigned" );
               }
            else
               SetDlgItemText( hwnd, IDD_ASSIGNMENT, "" );
            }
         break;
         }

      case PSN_APPLY: {
         CMDREC msi;
         HCMD hCmd;
         WORD wMyKey;

         /* Update the menu assignments for those keys that
          * actually changed.
          */
         hCmd = NULL;
         while( NULL != ( hCmd = CTree_EnumTree( hCmd, &msi ) ) )
            if( msi.wNewKey != msi.wKey )
               {
               char sz[ 80 ];

               msi.wKey = msi.wNewKey;
               wMyKey = MapVirtualKey( msi.wNewKey, 2 );
               if( wMyKey < 0x80 )
                  msi.iActiveMode = ( WIN_OUTBASK|WIN_THREAD|WIN_MESSAGE|WIN_INFOBAR|WIN_INBASK|WIN_RESUMES|WIN_SCHEDULE );
               CTree_SetItem( hCmd, &msi );
               if( GetMenuString( hMainMenu, msi.iCommand, sz, sizeof( sz ), MF_BYCOMMAND ) )
                  {
                  char * pTab;
                  int len;

                  pTab = strchr( sz, '\t' );
                  if( pTab )
                     *pTab = '\0';
                  len = ( sizeof(sz) - strlen( sz ) ) - 1;
                  if( msi.wKey && len > 1 )
                     {
                     strcat( sz, "\t" );
                     DescribeKey( msi.wKey, sz + strlen( sz ), len - 1 );
                     }
                  ModifyMenu( hMainMenu, msi.iCommand, MF_BYCOMMAND|MF_STRING, msi.iCommand, sz );
                  }
               }
         DrawMenuBar( hwndFrame );

         /* Save the new keyboard arrangement.
          */
         SaveKeyboard();

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This function expands wFullKey, in MAKEKEY format, into a full
 * description of that key.
 */
void FASTCALL DescribeKey( WORD wFullKey, char * psz, int maxsz )
{
   WORD wModifier;
   WORD wKey;
                  
   wModifier = GETMODIFIER( wFullKey );
   wKey = GETKEY( wFullKey );
   *psz = '\0';
   if( ( wModifier & HOTKEYF_CONTROL ) && maxsz > 5 )
      {
      strcat( psz, "Ctrl+" );
      maxsz -= 5;
      }
   if( ( wModifier & HOTKEYF_ALT ) && maxsz > 4 )
      {
      strcat( psz, "Alt+" );
      maxsz -= 4;
      }
   if( ( wModifier & HOTKEYF_SHIFT ) && maxsz > 6 )
      {
      strcat( psz, "Shift+" );
      maxsz -= 6;
      }
   if( wKey && maxsz > 0 )
      Amctl_NewGetKeyNameText( wKey, psz + strlen( psz ), maxsz );
}
