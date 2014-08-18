/* MAILBOX.C - Create a new mailbox
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
#include <string.h>
#include "amlib.h"
#include "rules.h"
#include "ameol2.h"

#define  THIS_FILE   __FILE__

static BOOL fDefDlgEx = FALSE;

BOOL EXPORT CALLBACK Mailbox( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL Mailbox_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL Mailbox_OnCommand( HWND, int, HWND, UINT );
BOOL FASTCALL Mailbox_OnInitDialog( HWND, HWND, LPARAM );

/* This function creates a new mailbox and a rule to accompany
 * that mailbox.
 */
void FASTCALL CreateNewMailFolder( HWND hwnd )
{
   Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_MAILBOX), Mailbox, 0L );
}

/* Handles the Mailbox dialog.
 */
BOOL EXPORT CALLBACK Mailbox( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, Mailbox_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the Get New Newsgroups dialog.
 */
LRESULT FASTCALL Mailbox_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, Mailbox_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, Mailbox_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_MEASUREITEM, FolderCombo_OnMeasureItem );
      HANDLE_DLGMSG( hwnd, WM_DRAWITEM, FolderCombo_OnDrawItem );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsMAILBOX );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL Mailbox_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndList;
   CURMSG curmsg;
   int index;

   /* Fill the listbox with the list of folders.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   FillListWithFolders( hwnd, IDD_LIST );

   /* Highlight the current folder or topic.
    */
   Ameol2_GetCurrentTopic( &curmsg );
   if( NULL != curmsg.pFolder )
      {
      ASSERT( NULL != curmsg.pcat );
      if( NULL == curmsg.pcl )
         curmsg.pcl = Amdb_GetFirstFolder( curmsg.pcat );
      if( CB_ERR != ( index = ComboBox_FindStringExact( hwndList, -1, curmsg.pcl ) ) )
         ComboBox_SetCurSel( hwndList, index );
      }

   /* Limit the input field.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_EDIT ), LEN_MAILBOX-1 );
   EnableControl( hwnd, IDOK, FALSE );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */ 
void FASTCALL Mailbox_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EDIT:
         if( EN_CHANGE == codeNotify )
            EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndCtl ) > 0 );
         break;

      case IDOK: {
         char szMailBox[ LEN_MAILBOX ];
         HWND hwndList;
         int index;
         PCL pcl;
         PTL ptl;

         ptlNewMailBox = NULL;

         /* First check that the user selected a folder.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         if( CB_ERR == ( index = ComboBox_GetCurSel( hwndList ) ) )
            break;
         pcl = (PCL)ComboBox_GetItemData( hwndList, index );
         if( !Amdb_IsFolderPtr( pcl ) )
            break;

         /* Create the topic in the Mail folder. (Check it doesn't exist!)
          */
         GetDlgItemText( hwnd, IDD_EDIT, szMailBox, sizeof( szMailBox ) );
         StripLeadingTrailingSpaces( szMailBox );
         if( !IsValidFolderName( hwnd, szMailBox ) )
            break;
         if( strchr( szMailBox, ' ' ) != NULL )
            {
            wsprintf( lpTmpBuf, GS(IDS_STR1154), ' ' );
            fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONINFORMATION );
            break;
            }
         ptl = NULL;
         if( pcl )
            {
            if( Amdb_OpenTopic( pcl, szMailBox ) )
               {
               fMessageBox( hwnd, 0, GS(IDS_STR832), MB_OK|MB_ICONEXCLAMATION );
               break;
               }
            ptl = Amdb_CreateTopic( pcl, szMailBox, FTYPE_MAIL );
            Amdb_CommitDatabase( FALSE );
            }

         /* Error if we couldn't create the topic (out of memory?)
          */
         if( NULL == ptl )
            {
            wsprintf( lpTmpBuf, GS(IDS_STR845), szMailBox );
            fMessageBox( hwndFrame, 0, lpTmpBuf, MB_OK|MB_ICONINFORMATION );
            break;
            }

         ptlNewMailBox = ptl;
         EndDialog( hwnd, TRUE );
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}

/* This function creates a mail address from its mailbox, hostname and domain
 * components.
 */
void FASTCALL CreateMailAddress( char * pDest, char * pMailBox, char * pHostname, char * pDomain )
{
   strcpy( pDest, pMailBox );
   strcat( pDest, "@" );
   if( *pHostname )
      {
      strcat( pDest, pHostname );
      strcat( pDest, "." );
      }
   strcat( pDest, pDomain );
}

/* This function creates an e-mail mailbox.
 */
BOOL FASTCALL CreateMailbox( PTL ptl, char * pszMailbox )
{
   RULE rule;

   memset( &rule, 0, sizeof(RULE) );
   rule.wFlags = FILF_MOVE;
   rule.pData = GetPostmasterFolder();
   rule.pMoveData = ptl;
   lstrcpy( rule.szToText, pszMailbox );
   return( CreateRule( &rule ) != NULL );
}

/* This function checks that pszFolderName is a valid
 * folder name.
 */
BOOL FASTCALL IsValidFolderName( HWND hwnd, char * pszFolderName )
{
   char * pInvChr;

   if( ( pInvChr = strpbrk( pszFolderName, ",\\/?*:" ) ) != NULL )
      {
      wsprintf( lpTmpBuf, GS(IDS_STR1154), *pInvChr );
      fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONINFORMATION );
      return( FALSE );
      }
   return( TRUE );
}
