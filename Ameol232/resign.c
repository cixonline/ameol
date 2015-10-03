/* RESIGN.C - Routines to resign from a conference, topic or newsgroup
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
#include <memory.h>
#include <string.h>
#include "shared.h"
#include "cixip.h"
#include "cix.h"
#include "amlib.h"
#include "ameol2.h"
#include "news.h"

static BOOL fDefDlgEx = FALSE;

BOOL EXPORT CALLBACK ResignDlg( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL ResignDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL ResignDlg_OnCommand( HWND, int, HWND, UINT );
void FASTCALL ResignDlg_OnPaint( HWND );
BOOL FASTCALL ResignDlg_OnInitDialog( HWND, HWND, LPARAM );

/* This is the Resign out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_CIXResign( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   RESIGNOBJECT FAR * lpro;

   lpro = (RESIGNOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR237), Amob_DerefObject( &lpro->recConfName ) );
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( 0 );

      case OBEVENT_NEW:
         _fmemset( lpro, 0, sizeof( RESIGNOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         RESIGNOBJECT gmo;

         INITIALISE_PTR(lpro);
         Amob_LoadDataObject( fh, &gmo, sizeof( RESIGNOBJECT ) );
         if( fNewMemory( &lpro, sizeof( RESIGNOBJECT ) ) )
            {
            *lpro = gmo;
            lpro->recConfName.hpStr = NULL;
            }
         return( (LRESULT)lpro );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpro->recConfName );
         return( Amob_SaveDataObject( fh, lpro, sizeof( RESIGNOBJECT ) ) );

      case OBEVENT_COPY: {
         RESIGNOBJECT FAR * lproNew;

         INITIALISE_PTR( lproNew );
         if( fNewMemory( &lproNew, sizeof( RESIGNOBJECT ) ) )
            {
            INITIALISE_PTR( lproNew->recConfName.hpStr );
            Amob_CopyRefObject( &lproNew->recConfName, &lpro->recConfName );
            }
         return( (LRESULT)lproNew );
         }

      case OBEVENT_FIND: {
         RESIGNOBJECT FAR * lpro1;
         RESIGNOBJECT FAR * lpro2;

         lpro1 = (RESIGNOBJECT FAR *)dwData;
         lpro2 = (RESIGNOBJECT FAR *)lpBuf;
         return( strcmp( Amob_DerefObject( &lpro1->recConfName ), Amob_DerefObject( &lpro2->recConfName ) ) == 0 );
         }

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpro->recConfName );
         return( TRUE );
      }
   return( 0L );
}

/* This function handles the resign command.
 */
BOOL FASTCALL CmdResign( HWND hwnd, LPVOID pFolder, BOOL fOfferToRead, BOOL fRejoin )
{
   BOOL fConfirm;
   int fMsgRet;
   PCL pcl;
   PTL ptl;

   /* Get the selected topic or conference. Need to put up
    * dialog if topic selected.
    */
   if( NULL == pFolder )
      {
      CURMSG curmsg;

      Ameol2_GetCurrentTopic( &curmsg );
      if( NULL == ( pFolder = curmsg.pFolder ) )
         return( FALSE );
      }
   fConfirm = FALSE;
/* if( NULL == hwndTopic )
      fConfirm = TRUE;
   else */if( pFolder && Amdb_IsFolderPtr( pFolder ) )
      fConfirm = TRUE;
   else if( pFolder && Amdb_IsTopicPtr( pFolder ) && Amdb_GetTopicType( pFolder ) == FTYPE_CIX )
      {
      if( !Amdb_IsResignedTopic( (PTL)pFolder ) )
         if( !Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_RESIGN), ResignDlg, (LPARAM)(LPSTR)&pFolder ) )
            return( FALSE );
      }

   /* Compose an appropriate message.
    */
   if( Amdb_IsTopicPtr( pFolder ) && Amdb_GetTopicType( pFolder ) == FTYPE_CIX )
      {
      /* Return now if already resigned this topic.
       */
      ptl = pFolder;
      pcl = Amdb_FolderFromTopic( ptl );
      if( Amdb_IsResignedTopic( ptl ) )
         {
         if( fRejoin )
            {
            wsprintf( lpTmpBuf, "%s/%s", Amdb_GetFolderName( pcl ), Amdb_GetTopicName( ptl ) );
            CmdJoinConference( hwndFrame, lpTmpBuf );
            }
         return( TRUE );
         }
      wsprintf( lpTmpBuf, GS(IDS_STR32), Amdb_GetFolderName( pcl ), Amdb_GetTopicName( ptl ) );
      }
   else if( Amdb_IsFolderPtr( pFolder ) && IsCIXFolder( pFolder ) )
      {
      /* Return now if already resigned this folder.
       */
      ptl = NULL;
      pcl = pFolder;
      if( Amdb_IsResignedFolder( pcl ) )
         {
         if( fRejoin )
            CmdJoinConference( hwndFrame, (LPSTR)Amdb_GetFolderName( pcl ) );
         return( TRUE );
         }
      wsprintf( lpTmpBuf, GS(IDS_STR33), Amdb_GetFolderName( pcl ) );
      }
   else if( Amdb_IsTopicPtr( pFolder ) && Amdb_GetTopicType( pFolder ) == FTYPE_NEWS )
      {
      /* Return now if already resigned this newsgroup.
       */
      ptl = pFolder;
      pcl = Amdb_FolderFromTopic( ptl );
      if( Amdb_IsResignedTopic( ptl ) )
         {
         if( fRejoin )
            Subscribe( hwnd, (LPSTR)Amdb_GetTopicName( ptl ) );
         return( TRUE );
         }
      wsprintf( lpTmpBuf, GS(IDS_STR29), Amdb_GetTopicName( ptl ) );
      fConfirm = TRUE;
      }
   else
      return( TRUE );

   /* Put up the query
    */
   if( !fConfirm )
      fMsgRet = IDYES;
   else
      if( ( fMsgRet = fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONINFORMATION ) ) == IDNO )
         return( FALSE );

   /* Okay. Resign the topic and return OK.
    */
   if( IDYES == fMsgRet )
      {
      if( NULL != ptl && Amdb_GetTopicType( ptl ) == FTYPE_NEWS )
         {
         Amdb_SetTopicFlags( ptl, TF_RESIGNED, TRUE );
         if( IsCixnewsNewsgroup( (char *)Amdb_GetTopicName( ptl ) ) )
            {
            RESIGNUNETOBJECT ro;

            /* Create a Usenet Resign object.
             */
            Amob_New( OBTYPE_USENETRESIGN, &ro );
            wsprintf( lpTmpBuf, "%s", Amdb_GetTopicName( ptl ) );
            Amob_CreateRefString( &ro.recGroupName, lpTmpBuf );
            if( !Amob_Find( OBTYPE_USENETRESIGN, &ro ) )
               Amob_Commit( NULL, OBTYPE_USENETRESIGN, &ro );
            Amob_Delete( OBTYPE_USENETRESIGN, &ro );
            }
         }
      else
         {
         RESIGNOBJECT ro;

         /* Create a Resign object.
          */
         Amob_New( OBTYPE_CIXRESIGN, &ro );
         if( NULL != ptl )
            wsprintf( lpTmpBuf, "%s/%s", Amdb_GetFolderName( pcl ), Amdb_GetTopicName( ptl ) );
         else
            wsprintf( lpTmpBuf, "%s", Amdb_GetFolderName( pcl ) );
         Amob_CreateRefString( &ro.recConfName, lpTmpBuf );
         if( !Amob_Find( OBTYPE_CIXRESIGN, &ro ) )
            Amob_Commit( NULL, OBTYPE_CIXRESIGN, &ro );
         Amob_Delete( OBTYPE_CIXRESIGN, &ro );
         }

      /* Offer to mark topics as read.
       */
      if( Amdb_IsFolderPtr( pFolder ) )
         {
         BOOL fPromptUnread;
         BOOL fQuery;
         PTL ptl;

         fQuery = FALSE;
         fPromptUnread = fOfferToRead;
         for( ptl = Amdb_GetFirstTopic( pFolder ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
            {
            TOPICINFO topicinfo;

            Amdb_GetTopicInfo( ptl, &topicinfo );
            if( topicinfo.cUnRead && fPromptUnread )
               {
               if( !fQuery )
                  {
                  if( fMessageBox( hwnd, 0, GS(IDS_STR34), MB_YESNO|MB_ICONQUESTION ) == IDNO )
                     fPromptUnread = FALSE;
                  fQuery = TRUE;
                  }
               if( fPromptUnread )
                  {
                  Amdb_MarkTopicRead( ptl );
                  if( hwndTopic )
                     SendAllWindows( WM_UPDMSGFLAGS, 0, (LPARAM)ptl );
                  }
               }
            Amdb_SetTopicFlags( ptl, TF_RESIGNED, TRUE );
            }
         }
      else {
         TOPICINFO topicinfo;

         Amdb_GetTopicInfo( (PTL)pFolder, &topicinfo );
         if( topicinfo.cUnRead && fOfferToRead )
            {
            if( fMessageBox( hwnd, 0, GS(IDS_STR35), MB_YESNO|MB_ICONQUESTION ) == IDYES )
               {
               Amdb_MarkTopicRead( (PTL)pFolder );
               if( (PTL)pFolder )
                  SendAllWindows( WM_UPDMSGFLAGS, 0, (LPARAM)pFolder );
               }
            }
         Amdb_SetTopicFlags( (PTL)pFolder, TF_RESIGNED, TRUE );
         }
      }
   return( TRUE );
}

/* Handles the Resign dialog.
 */
BOOL EXPORT CALLBACK ResignDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, ResignDlg_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the Get New Newsgroups dialog.
 */
LRESULT FASTCALL ResignDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, ResignDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, ResignDlg_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsRESIGN );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL ResignDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   char szPrompt[ 60 ];
   char sz[ 100 ];
   LPVOID * ppFolder;
   PCL pcl;
   PTL ptl;

   /* Get the handle of the topic we're resigning and
    * set the appropriate option.
    */
   ppFolder = (LPVOID *)lParam;
   ptl = *ppFolder;
   pcl = Amdb_FolderFromTopic( ptl );
   CheckDlgButton( hwnd, IDD_TOPIC, TRUE );
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* Set the conference name.
    */
   Edit_GetText( GetDlgItem( hwnd, IDD_CONFERENCE ), szPrompt, sizeof( szPrompt ) );
   wsprintf( sz, szPrompt, Amdb_GetFolderName( pcl ) );
   Edit_SetText( GetDlgItem( hwnd, IDD_CONFERENCE ), sz );

   /* Set the conference/topic name.
    */
   Edit_GetText( GetDlgItem( hwnd, IDD_TOPIC ), szPrompt, sizeof( szPrompt ) );
   wsprintf( sz, szPrompt, Amdb_GetFolderName( pcl ), Amdb_GetTopicName( ptl ) );
   Edit_SetText( GetDlgItem( hwnd, IDD_TOPIC ), sz );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */ 
void FASTCALL ResignDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDOK: {
         LPVOID * ppFolder;
         PCL pcl;
         PTL ptl;

         /* Get the topic we're supposed to be resigning.
          */
         ppFolder = (LPVOID *)GetWindowLong( hwnd, DWL_USER );
         ptl = *ppFolder;
         pcl = Amdb_FolderFromTopic( ptl );

         /* Create a Resign object.
          */
         if( IsDlgButtonChecked( hwnd, IDD_TOPIC ) )
            *ppFolder = ptl;
         else
            *ppFolder = pcl;
         EndDialog( hwnd, TRUE );
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}

/* This is the CIX Usenet resign out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_UsenetResign( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   RESIGNUNETOBJECT FAR * lpng;

   lpng = (RESIGNUNETOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR226), DRF(lpng->recGroupName) );
         return( TRUE );

      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_NEW:
         _fmemset( lpng, 0, sizeof(RESIGNUNETOBJECT) );
         return( TRUE );

      case OBEVENT_LOAD: {
         RESIGNUNETOBJECT ngo;

         INITIALISE_PTR(lpng);
         Amob_LoadDataObject( fh, &ngo, sizeof(RESIGNUNETOBJECT) );
         if( fNewMemory( &lpng, sizeof(RESIGNUNETOBJECT) ) )
            {
            *lpng = ngo;
            lpng->recGroupName.hpStr = NULL;
            }
         return( (LRESULT)lpng );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpng->recGroupName );
         return( Amob_SaveDataObject( fh, lpng, sizeof(RESIGNUNETOBJECT) ) );

      case OBEVENT_COPY: {
         RESIGNUNETOBJECT FAR * lpngNew;

         INITIALISE_PTR( lpngNew );
         if( fNewMemory( &lpngNew, sizeof(RESIGNUNETOBJECT) ) )
            {
            INITIALISE_PTR( lpngNew->recGroupName.hpStr );
            Amob_CopyRefObject( &lpngNew->recGroupName, &lpng->recGroupName );
            }
         return( (LRESULT)lpngNew );
         }

      case OBEVENT_FIND: {
         RESIGNUNETOBJECT FAR * lpng1;
         RESIGNUNETOBJECT FAR * lpng2;

         lpng1 = (RESIGNUNETOBJECT FAR *)dwData;
         lpng2 = (RESIGNUNETOBJECT FAR *)lpBuf;
         return( strcmp( lpng1->recGroupName.hpStr, lpng2->recGroupName.hpStr ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpng );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpng->recGroupName );
         return( TRUE );
      }
   return( 0L );
}
