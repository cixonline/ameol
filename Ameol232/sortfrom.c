/* SORTFROM.C - Create a new rule for redirecting mail from an address
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
#include "cix.h"
#include "rules.h"

#define  THIS_FILE   __FILE__

static BOOL fDefDlgEx = FALSE;

static BOOL fMoveExisting;          /* TRUE if we move existing messages */
static PTL ptlDest;                 /* Destination topic */
static char szRuleAuthor[ LEN_INTERNETNAME+1 ];

int FASTCALL AddressRuleMatch( LPSTR, LPSTR );
BOOL EXPORT CALLBACK SortMailFrom( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL SortMailFrom_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL SortMailFrom_OnCommand( HWND, int, HWND, UINT );
BOOL FASTCALL SortMailFrom_OnInitDialog( HWND, HWND, LPARAM );

/* This function creates a new mailbox and a rule to accompany
 * that mailbox.
 */
void FASTCALL CreateSortMailFrom( HWND hwnd )
{
   LPWINDINFO lpWindInfo;

   lpWindInfo = GetBlock( hwndTopic );
   if( Amdb_GetTopicType( lpWindInfo->lpTopic ) != FTYPE_MAIL )
      return;

   if( Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_SORTMAILFROM), SortMailFrom, 0L ) )
      {
      MSGINFO msginfo;
      RULE rule;
      PTH pth;

      /* Get the current folder handle.
       */
      Amdb_GetMsgInfo( lpWindInfo->lpMessage, &msginfo );

      /* First create a new rule.
       */
      memset( &rule, 0, sizeof(RULE) );
      rule.wFlags = FILF_MOVE;
      rule.pData = lpWindInfo->lpTopic;
      rule.pMoveData = ptlDest;
      lstrcpy( rule.szFromText, szRuleAuthor );
      if( CreateRule( &rule ) == NULL )
         {
         fMessageBox( hwnd, 0, GS(IDS_STR1043), MB_OK|MB_ICONINFORMATION );
         return;
         }

      /* If we move existing messages, then loop and move 'em.
       */
      if( fMoveExisting )
         {
         PTH FAR * lppth;
         int count;
         HCURSOR hOldCursor;

         INITIALISE_PTR(lppth);

         /* Busy, busy, bee
          */
         hOldCursor = SetCursor( hWaitCursor );

         /* First pass - count the number of messages.
          */
         count = 0;
         for( pth = Amdb_GetFirstMsg( lpWindInfo->lpTopic, VM_VIEWCHRON ); pth; pth = Amdb_GetNextMsg( pth, VM_VIEWCHRON ) )
            {
            MSGINFO msginfo2;

            Amdb_GetMsgInfo( pth, &msginfo2 );
//          if( strcmp( msginfo2.szAuthor, szRuleAuthor ) == 0 || ( szRuleAuthor[ 0 ] == '@' && strstr( msginfo2.szAuthor, szRuleAuthor ) != NULL ) )
            if( AddressRuleMatch( szRuleAuthor, msginfo2.szAuthor ) != -1 )
               ++count;
            }

         /* Second pass, allocate a PTH array of count messages and collect
          * the pth handles to the array.
          */
         if( count > 0 )
         {
         if( fNewMemory( (PTH FAR *)&lppth, ( count + 1 ) * sizeof(PTH) ) )
            {
            int index;

            index = 0;
            for( pth = Amdb_GetFirstMsg( lpWindInfo->lpTopic, VM_VIEWCHRON ); pth; pth = Amdb_GetNextMsg( pth, VM_VIEWCHRON ) )
               {
               MSGINFO msginfo2;

               Amdb_GetMsgInfo( pth, &msginfo2 );
//          if( strcmp( msginfo2.szAuthor, szRuleAuthor ) == 0 || ( szRuleAuthor[ 0 ] == '@' && strstr( msginfo2.szAuthor, szRuleAuthor ) != NULL ) )
            if( AddressRuleMatch( szRuleAuthor, msginfo2.szAuthor ) != -1 )
                  {
                  ASSERT( index < count );
                  lppth[ index++ ] = pth;
                  }
               }
            ASSERT( index == count );
            lppth[ index++ ] = NULL;
            DoCopyLocalRange( hwndTopic, lppth, count, ptlDest, TRUE, FALSE );
            FreeMemory( (PTH FAR *)&lppth );
            }
         }
         SetCursor( hOldCursor );
         }
      }
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL SortMailFrom_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPWINDINFO lpWindInfo;
   MSGINFO msginfo;
   HWND hwndList;
   HWND hwndEdit;
   HWND hwndName;
   int index;
   PCAT pcat;
   PCL pcl;
   PTL ptl;

   /* Must have a message window open.
    */
   if( NULL == hwndTopic )
      {
      EndDialog( hwnd, 0 );
      return( FALSE );
      }

   /* Get details of the message we'll be using and set the
    * static text field in the dialog. If there is no author field (shouldn't
    * be possible now) we tell the user and quit the dialog.
    */
   lpWindInfo = GetBlock( hwndTopic );
   Amdb_GetMsgInfo( lpWindInfo->lpMessage, &msginfo );
   if( msginfo.szAuthor[ 0 ] == '\0' )
      {
      fMessageBox( hwnd, 0, GS(IDS_STR1220), MB_OK|MB_ICONINFORMATION );
      EndDialog( hwnd, 0 );
      return( FALSE );
      }

   SetDlgItemText( hwnd, IDD_PAD1, GS(IDS_STR1042) );

   /* Set the edit box to the author details
    */
   hwndName = GetDlgItem( hwnd, IDD_NAME );
   Edit_SetText( hwndName, msginfo.szAuthor );
   Edit_LimitText( hwndName, LEN_INTERNETNAME+1 );

   /* Fill the list of mail folders.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   for( pcat = Amdb_GetFirstCategory(); pcat; pcat = Amdb_GetNextCategory( pcat ) )
      for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
         for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
            if( Amdb_GetTopicType( ptl ) == FTYPE_MAIL )
               {
               int index;

               index = ComboBox_InsertString( hwndList, -1, (LPSTR)Amdb_GetTopicName( ptl ) );
               ComboBox_SetItemData( hwndList, index, (LPARAM)ptl );
               }

   /* Select the first Mail topic.
    */
   if( ( index = RealComboBox_FindItemData( hwndList, -1, (LPARAM)lpWindInfo->lpTopic ) ) == CB_ERR )
      index = 0;
   ComboBox_SetCurSel( hwndList, index );

   /* Set the edit field limits.
    */
   VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_EDIT ) );
   Edit_LimitText( hwndEdit, LEN_TOPICNAME );

   /* Choose to move to an existing folder.
    */
   CheckDlgButton( hwnd, IDD_EXISTING, TRUE );
   EnableControl( hwnd, IDD_PAD2, FALSE );
   EnableControl( hwnd, IDD_EDIT, FALSE );

   /* Move other messages.
    */
   CheckDlgButton( hwnd, IDD_FILEALL, TRUE );
   return( TRUE );
}

/* Handles the SortMailFrom dialog.
 */
BOOL EXPORT CALLBACK SortMailFrom( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, SortMailFrom_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the Get New Newsgroups dialog.
 */
LRESULT FASTCALL SortMailFrom_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, SortMailFrom_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, SortMailFrom_OnCommand );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, FolderCombo_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_DRAWITEM, FolderCombo_OnDrawItem );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSORTMAILFROM );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_COMMAND message.
 */ 
void FASTCALL SortMailFrom_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EXISTING:
         EnableControl( hwnd, IDD_LIST, TRUE );
         EnableControl( hwnd, IDD_PAD2, FALSE );
         EnableControl( hwnd, IDD_EDIT, FALSE );
         break;

      case IDD_NEW:
         EnableControl( hwnd, IDD_LIST, FALSE );
         EnableControl( hwnd, IDD_PAD2, TRUE );
         EnableControl( hwnd, IDD_EDIT, TRUE );
         SetFocus( GetDlgItem( hwnd, IDD_EDIT ) );
         break;

      case IDOK:
         /* Get the destination folder.
          */
         if( IsDlgButtonChecked( hwnd, IDD_EXISTING ) )
            {
            HWND hwndList;
            int index;

            VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
            index = ComboBox_GetCurSel( hwndList );
            ASSERT( CB_ERR != index );
            ptlDest = (PTL)ComboBox_GetItemData( hwndList, index );
            ASSERT( NULL != ptlDest );
            }
         else
            {
            char szMailBox[ LEN_MAILBOX ];
            PCL pcl;

            /* Get the destination folder name.
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
            if( NULL == ( pcl = Amdb_OpenFolder( NULL, "Mail" ) ) )
               pcl = Amdb_CreateFolder( Amdb_GetFirstCategory(), "Mail", CFF_SORT );
            ptlDest = NULL;
            if( pcl )
               {
               if( Amdb_OpenTopic( pcl, szMailBox ) )
                  {
                  fMessageBox( hwnd, 0, GS(IDS_STR832), MB_OK|MB_ICONEXCLAMATION );
                  break;
                  }
               ptlDest = Amdb_CreateTopic( pcl, szMailBox, FTYPE_MAIL );
               Amdb_CommitDatabase( FALSE );
               }

            /* Error if we couldn't create the folder (out of memory?)
             */
            if( NULL == ptlDest )
               {
               wsprintf( lpTmpBuf, GS(IDS_STR845), szMailBox );
               fMessageBox( hwndFrame, 0, lpTmpBuf, MB_OK|MB_ICONINFORMATION );
               break;
               }
            }

         /* Get the address, reject if there isn't one.
          */

         Edit_GetText( GetDlgItem( hwnd, IDD_NAME ), szRuleAuthor, LEN_INTERNETNAME+1 );
         if( strlen( szRuleAuthor ) == 0 )
         {
            fMessageBox( hwnd, 0, GS(IDS_STR783), MB_OK|MB_ICONINFORMATION );
            HighlightField( hwnd, IDD_NAME);
            break;
         }

         /* Set a flag if all messages are moved to the
          * destination folder.
          */
         fMoveExisting = IsDlgButtonChecked( hwnd, IDD_FILEALL );
         EndDialog( hwnd, TRUE );
         break;

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}
