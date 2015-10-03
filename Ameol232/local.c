/* LOCAL.C - Local topics driver main program
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

#define  THIS_FILE   __FILE__

#include "main.h"
#include "resource.h"
#include "amlib.h"
#include "local.h"
#include "ameol2.h"
#include "rules.h"
#include <string.h>

static BOOL fDefDlgEx = FALSE;

BOOL EXPORT CALLBACK LocalTopic( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL LocalTopic_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL LocalTopic_OnCommand( HWND, int, HWND, UINT );
BOOL FASTCALL LocalTopic_OnInitDialog( HWND, HWND, LPARAM );

BOOL WINAPI Local_Command( int, LPARAM, LPARAM );

BOOL EXPORT CALLBACK Prefs_P8( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL Prefs_P8_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL Prefs_P8_OnNotify( HWND, int, LPNMHDR );
void FASTCALL Prefs_P8_OnCommand( HWND, int, HWND, UINT );

/* This function registers all out-basket object types.
 */
BOOL FASTCALL LoadLocal( void )
{
   /* First, register some events.
    */
   Amuser_RegisterEvent( AE_COMMAND, Local_Command );

   /* Next, register out-basket types used by the
    * Local conferencing service.
    */
   if( !Amob_RegisterObjectClass( OBTYPE_LOCALSAYMESSAGE, 0, ObProc_LocalSayMessage ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_LOCALCMTMESSAGE, 0, ObProc_LocalCmtMessage ) )
      return( FALSE );
   return( TRUE );
}

/* Initialise the local conferencing interface.
 */
BOOL FASTCALL InitialiseLocalInterface( HWND hwnd )
{
   return( TRUE );
}

/* This callback is called when the service is unloaded.
 */
void FAR PASCAL UnloadLocal( int nWhyAmIBeingKilled )
{
   Amuser_UnregisterEvent( AE_COMMAND, Local_Command );
}

/* This callback function is called when the user chooses a
 * menu command or presses a keystroke corresponding to a menu
 * command.
 */
BOOL WINAPI EXPORT Local_Command( int wEvent, LPARAM lParam1, LPARAM lParam2 )
{
   return( TRUE );
}

/* This function creates a new mailbox and a rule to accompany
 * that mailbox.
 */
void FASTCALL CreateNewLocalTopic( HWND hwnd )
{
   Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_LOCALTOPIC), LocalTopic, 0L );
}

/* Handles the LocalTopic dialog.
 */
BOOL EXPORT CALLBACK LocalTopic( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, LocalTopic_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the Get New Newsgroups dialog.
 */
LRESULT FASTCALL LocalTopic_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, LocalTopic_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, LocalTopic_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_MEASUREITEM, FolderCombo_OnMeasureItem );
      HANDLE_DLGMSG( hwnd, WM_DRAWITEM, FolderCombo_OnDrawItem );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsLOCALTOPIC );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL LocalTopic_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndList;
   HWND hwndEdit;
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
   VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_EDIT ) );
   Edit_LimitText( hwndEdit, LEN_TOPICNAME );
   EnableControl( hwnd, IDOK, FALSE );
   SetFocus( hwndEdit );
   return( FALSE );
}

/* This function handles the WM_COMMAND message.
 */ 
void FASTCALL LocalTopic_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EDIT:
         if( codeNotify == EN_UPDATE )
            EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndCtl ) > 0 );
         break;

      case IDOK: {
         HWND hwndList;
         int index;
         PCL pcl;

         ptlLocalTopicName = NULL;

         /* First check that the user selected a folder.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         if( CB_ERR != ( index = ComboBox_GetCurSel( hwndList ) ) )
            {
            pcl = (PCL)ComboBox_GetItemData( hwndList, index );
            if( Amdb_IsFolderPtr( pcl ) )
               {
               char szTopicName[ LEN_TOPICNAME+1 ];
               HWND hwndEdit;

               /* Folder okay. Get the topic name.
                */
               hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
               Edit_GetText( hwndEdit, szTopicName, LEN_TOPICNAME+1 );
               StripLeadingTrailingSpaces( szTopicName );
               if( *szTopicName )
                  {
                  /* Make sure the name is valid. Another check for
                   * spaces which aren't permitted in local topics.
                   */
                  if( !IsValidFolderName( hwnd, szTopicName ) )
                     break;
                  if( strchr( szTopicName, ' ' ) != NULL )
                     {
                     wsprintf( lpTmpBuf, GS(IDS_STR1154), ' ' );
                     fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONINFORMATION );
                     break;
                     }

                  /* Check that the topic doesn't already exist.
                   */
                  if( NULL == Amdb_OpenTopic( pcl, szTopicName ) )
                     {
                     ptlLocalTopicName = Amdb_CreateTopic( pcl, szTopicName, FTYPE_LOCAL );
                     Amdb_CommitDatabase( FALSE );
                     EndDialog( hwnd, FALSE );
                     break;
                     }

                  /* Error - topic already exists.
                   */
                  fMessageBox( hwnd, 0, GS(IDS_STR833), MB_OK|MB_ICONEXCLAMATION );
                  break;
                  }
               }
            }
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}
