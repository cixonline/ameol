/* JOINCONF.C - Routines to handle the Join Conference object
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
#include <memory.h>
#include <string.h>
#include "cix.h"
#include "amlib.h"
#include "ameol2.h"

static BOOL fDefDlgEx = FALSE;

BOOL EXPORT CALLBACK JoinConferenceDlg( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL JoinConferenceDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL JoinConferenceDlg_OnCommand( HWND, int, HWND, UINT );
void FASTCALL JoinConferenceDlg_OnPaint( HWND );
BOOL FASTCALL JoinConferenceDlg_OnInitDialog( HWND, HWND, LPARAM );

static PTL ptlJoined = NULL;           /* Handle of any topic joined */

/* This function joins the specified conference.
 */
PTL FASTCALL CmdJoinConference( HWND hwnd, char * pszConfName )
{
   char szText[ LEN_CONFNAME + LEN_TOPICNAME + 1 ];
   JOINOBJECT jo;
   PTL ptl;
   PCL pcl;  // 20060308 - 2.56.2049.13
   DWORD dwMsgNumber;

   ptlJoined = NULL;
   Amob_New( OBTYPE_JOINCONFERENCE, &jo );
   if( *pszConfName == '\0' )
      {
      if( hwndTopic )
         {
         GetMarkedName( hwndTopic, szText, LEN_CONFNAME + LEN_TOPICNAME + 1, FALSE );
         if( *szText != '\0' )
            pszConfName = szText;
         }
      if( *pszConfName == '\0' )
         {
         CURMSG curmsg;

         Ameol2_GetCurrentTopic( &curmsg );
         if( NULL != curmsg.ptl && Amdb_IsResignedTopic( curmsg.ptl ) )
            {
            wsprintf( szText, "%s/%s", Amdb_GetFolderName( curmsg.pcl ), Amdb_GetTopicName( curmsg.ptl ) );
            pszConfName = szText;
            }
         else if( NULL != curmsg.pcl )
            {
            strcpy( szText, Amdb_GetFolderName( curmsg.pcl ) );
            pszConfName = szText;
            }
         }
      }
   Amob_CreateRefString( &jo.recConfName, pszConfName );
   jo.wRecent = iRecent;
   if( Adm_Dialog( hInst, hwndFrame, MAKEINTRESOURCE(IDDLG_JOINHOST), JoinConferenceDlg, (LPARAM)(LPSTR)&jo ) )
      {
      if( !Amob_Find( OBTYPE_JOINCONFERENCE, &jo ) )
         Amob_Commit( NULL, OBTYPE_JOINCONFERENCE, &jo );
         
         ParseFolderTopicMessage( pszConfName, &ptl, &dwMsgNumber );

         if( ptl != NULL) // 20060428 - 2.56.2051 FS#126
         {
            if( strstr( pszConfName, "/" ) != NULL) // 20060308 - 2.56.2049.13
            {
               if( NULL != ptl )
               {
                  if( Amdb_IsResignedTopic( ptl ) )
                     Amdb_SetTopicFlags( ptl, TF_RESIGNED, FALSE );
               }
            }
            else // 20060308 - 2.56.2049.13
            {
               pcl = Amdb_FolderFromTopic(ptl);
               for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
               {
                  if( Amdb_IsResignedTopic( ptl ) )
                     Amdb_SetTopicFlags( ptl, TF_RESIGNED, FALSE );
               }
            }
         }
      }
   Amob_Delete( OBTYPE_JOINCONFERENCE, &jo );
   return( ptlJoined );
}

/* This is the Join CIX conference out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_JoinConference( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   JOINOBJECT FAR * lpjo;

   lpjo = (JOINOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_EDITABLE:
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR310), Amob_DerefObject( &lpjo->recConfName ) );
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( 0 );

      case OBEVENT_NEW:
         memset( lpjo, 0, sizeof( JOINOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         JOINOBJECT jo;
      
         INITIALISE_PTR(lpjo);
         Amob_LoadDataObject( fh, &jo, sizeof( JOINOBJECT ) );
         if( fNewMemory( &lpjo, sizeof( JOINOBJECT ) ) )
            {
            *lpjo = jo;
            lpjo->recConfName.hpStr = NULL;
            }
         return( (LRESULT)lpjo );
         }

      case OBEVENT_EDIT: {
         OBINFO obinfo;

         Amob_GetObInfo( (LPOB)dwData, &obinfo );
         Adm_Dialog( hInst, hwndFrame, MAKEINTRESOURCE(IDDLG_JOINHOST), JoinConferenceDlg, (LPARAM)obinfo.lpObData );
         Amob_Commit( (LPOB)dwData, OBTYPE_JOINCONFERENCE, obinfo.lpObData );
         return( TRUE );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpjo->recConfName );
         return( Amob_SaveDataObject( fh, lpjo, sizeof( JOINOBJECT ) ) );

      case OBEVENT_COPY: {
         JOINOBJECT FAR * lpjoNew;

         INITIALISE_PTR( lpjoNew );
         if( fNewMemory( &lpjoNew, sizeof( JOINOBJECT ) ) )
            {
            INITIALISE_PTR( lpjoNew->recConfName.hpStr );
            Amob_CopyRefObject( &lpjoNew->recConfName, &lpjo->recConfName );
            lpjoNew->wRecent = lpjo->wRecent;
            }
         return( (LRESULT)lpjoNew );
         }

      case OBEVENT_FIND: {
         JOINOBJECT FAR * lpjo1;
         JOINOBJECT FAR * lpjo2;

         lpjo1 = (JOINOBJECT FAR *)dwData;
         lpjo2 = (JOINOBJECT FAR *)lpBuf;
         return( strcmp( Amob_DerefObject( &lpjo1->recConfName ), Amob_DerefObject( &lpjo2->recConfName ) ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpjo );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpjo->recConfName );
         return( TRUE );
      }
   return( 0L );
}

/* Handles the Subscribe dialog.
 */
BOOL EXPORT CALLBACK JoinConferenceDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, JoinConferenceDlg_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the Get New Newsgroups dialog.
 */
LRESULT FASTCALL JoinConferenceDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, JoinConferenceDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, JoinConferenceDlg_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsJOINHOST );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL JoinConferenceDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   char szConfName[ LEN_CONFNAME + 1 ];
   char szTopicName[ LEN_TOPICNAME + 1 ];
   JOINOBJECT FAR * lpjo;
   HWND hwndSpin;
   LPSTR lp;

   /* Save the handle to the JOINOBJECT object.
    */
   SetWindowLong( hwnd, DWL_USER, lParam );
   lpjo = (JOINOBJECT FAR *)lParam;

   /* Set the default conference/topic name.
    */
   *szConfName = '\0';
   *szTopicName = '\0';
   lp = NULL;
   Amob_DerefObject( &lpjo->recConfName );
   lp = (LPSTR)lpjo->recConfName.hpStr;

   /* If lp points to a string, it is either a conference name or a combined
    * conference/topic name. Split them as appropriate.
    */
   if( NULL != lp )
      {
      char * np;
      register int c;
      register int max;

      np = szConfName;
      max = LEN_CONFNAME;
      for( c = 0; *lp; ++c, ++lp )
         if( *lp == '/' )
            {
            *np = '\0';
            np = szTopicName;
            max = LEN_TOPICNAME;
            c = -1;
            }
         else if( c < max )
            *np++ = *lp;
      *np = '\0';
      }
   AnsiLower( szConfName );
   AnsiLower( szTopicName );
   Edit_SetText( GetDlgItem( hwnd, IDD_CONFNAME ), szConfName );
   Edit_SetText( GetDlgItem( hwnd, IDD_TOPICNAME ), szTopicName );

   /* Set the input field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_CONFNAME ), LEN_CONFNAME );
   Edit_LimitText( GetDlgItem( hwnd, IDD_TOPICNAME ), LEN_TOPICNAME );
   Edit_LimitText( GetDlgItem( hwnd, IDD_EDIT ), 4 );

   /* Set the recent count.
    */
   SetDlgItemInt( hwnd, IDD_EDIT, lpjo->wRecent, FALSE );
   hwndSpin = GetDlgItem( hwnd, IDD_SPIN );
   SetWindowStyle( hwndSpin, GetWindowStyle( hwndSpin ) | UDS_NOTHOUSANDS ); 
   SendMessage( hwndSpin, UDM_SETBUDDY, (WPARAM)GetDlgItem( hwnd, IDD_EDIT ), 0L );
   SendMessage( hwndSpin, UDM_SETRANGE, 0, MAKELPARAM( 9999, 1 ) );
   SendMessage( hwndSpin, UDM_SETPOS, 0, MAKELPARAM( lpjo->wRecent, 0 ) );

   /* Disable OK button unless a conference name is provided.
    */
   EnableControl( hwnd, IDOK, *szConfName != '\0' );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */ 
void FASTCALL JoinConferenceDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_CONFNAME:
         if( codeNotify == EN_UPDATE )
            EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndCtl ) > 0 );
         break;

      case IDOK: {
         char sz[ LEN_TOPICNAME + LEN_CONFNAME + 2 ];
         char szTopicName[ LEN_TOPICNAME + 1 ];
         char szConfName[ LEN_CONFNAME + 1 ];
         JOINOBJECT FAR * lpjo;
         HWND hwndEdit;
         int iRecent;
         LPOB lpob;

         /* Get the recent count.
          */
         lpjo = (JOINOBJECT FAR *)GetWindowLong( hwnd, DWL_USER );
         if( !GetDlgInt( hwnd, IDD_EDIT, &iRecent, 1, 9999 ) )
            break;
         lpjo->wRecent = iRecent;

         /* Get the conference name.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_CONFNAME );
         Edit_GetText( hwndEdit, szConfName, LEN_CONFNAME+1 ); //FS#154 2054

         /* Get the topic name.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_TOPICNAME );
         Edit_GetText( hwndEdit, szTopicName, LEN_TOPICNAME );

         /* Combine if a topic name is specified.
          */
         if( *szTopicName )
            wsprintf( sz, "%s/%s", szConfName, szTopicName );
         else
            strcpy( sz, szConfName );

         /* Create the folder.
          */
         Amob_FreeRefObject( &lpjo->recConfName );
         Amob_CreateRefString( &lpjo->recConfName, sz );

         /* Check whether this JOIN exists.
          */
         if( NULL != ( lpob = Amob_Find( OBTYPE_JOINCONFERENCE, lpjo ) ) )
            {
            OBINFO obinfo;

            Amob_GetObInfo( lpob, &obinfo );
            if( (JOINOBJECT FAR *)obinfo.lpObData != lpjo )
               {
               wsprintf( lpTmpBuf, GS(IDS_STR25), szConfName );
               fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
               break;
               }
            }

         /* If a topic name was provided, create the conference and
          * topic in the database.
          */
         /*
         if( *szTopicName )
            {
            PCAT pcat;
            PCL pcl;

            pcat = GetCIXCategory();
            if( NULL == ( pcl = Amdb_OpenFolder( NULL, szConfName ) ) )
               pcl = Amdb_CreateFolder( pcat, szConfName, CFF_SORT );
            if( NULL != pcl )
               if( NULL == ( ptlJoined = Amdb_OpenTopic( pcl, szTopicName ) ) )
                  ptlJoined = Amdb_CreateTopic( pcl, szTopicName, FTYPE_CIX );
            }
         */
            if( *szConfName ) // 20060428 - 2.56.2052 FS#126
            {
               PCAT pcat;
               PCL pcl;

               pcat = GetCIXCategory();
               if( NULL == ( pcl = Amdb_OpenFolder( NULL, szConfName ) ) )
                  pcl = Amdb_CreateFolder( pcat, szConfName, CFF_SORT );
               if( *szTopicName )
               {
                  if( NULL != pcl )
                  {
                     if( NULL == ( ptlJoined = Amdb_OpenTopic( pcl, szTopicName ) ) )
                        ptlJoined = Amdb_CreateTopic( pcl, szTopicName, FTYPE_CIX );
                  }
               }
            }
         EndDialog( hwnd, TRUE );
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}
