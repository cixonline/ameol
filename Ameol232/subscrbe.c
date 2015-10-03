/* SUBSCRBE.C - Handles the Subscribe dialog
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
#include "cixip.h"
#include "amlib.h"
#include "rules.h"
#include "cookie.h"
#include "news.h"
#include "cix.h"

#define   THIS_FILE __FILE__

static BOOL fDefDlgEx = FALSE;

BOOL EXPORT CALLBACK SubscribeDlg( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL SubscribeDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL SubscribeDlg_OnCommand( HWND, int, HWND, UINT );
BOOL FASTCALL SubscribeDlg_OnInitDialog( HWND, HWND, LPARAM );

/* Remembered last subscribe info.
 */
static char szSubNewsServer[ 64 ];
static BOOL fGetAllBodies = FALSE;
static BOOL fGetLast50 = TRUE;

/* This function subscribes to the specified newsgroup.
 */
BOOL FASTCALL CmdSubscribe( HWND hwnd, SUBSCRIBE_INFO FAR * lpsi )
{
     return( Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_SUBSCRIBE), SubscribeDlg, (LPARAM)lpsi ) );
}

/* Handles the Subscribe dialog.
 */
BOOL EXPORT CALLBACK SubscribeDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
     CheckDefDlgRecursion( &fDefDlgEx );
     return( SetDlgMsgResult( hwnd, message, SubscribeDlg_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the Get New Newsgroups dialog.
 */
LRESULT FASTCALL SubscribeDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
     switch( message )
          {
          HANDLE_MSG( hwnd, WM_INITDIALOG, SubscribeDlg_OnInitDialog );
          HANDLE_MSG( hwnd, WM_COMMAND, SubscribeDlg_OnCommand );
          HANDLE_MSG( hwnd, WM_DRAWITEM, UsenetServers_OnDrawItem );
          HANDLE_MSG( hwnd, WM_MEASUREITEM, UsenetServers_OnMeasureItem );

          case WM_ADMHELP:
               HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSUBSCRIBE );
               break;

          default:
               return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
          }
     return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL SubscribeDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
     SUBSCRIBE_INFO FAR * psi;
     HWND hwndList;
     int index;

     /* Get the name of the newsgroup.
      */
     psi = (SUBSCRIBE_INFO FAR *)lParam;

     /* Fill the dialog box
      */
     Edit_SetText( GetDlgItem( hwnd, IDD_EDIT ), psi->pszNewsgroup );
     Edit_LimitText( GetDlgItem( hwnd, IDD_EDIT ), 79 );

     /* Disable OK if no text
      */
     EnableControl( hwnd, IDOK, *psi->pszNewsgroup );

     /* Fill the list box with the list of news servers.
      */
     if( NULL != psi->pszNewsServer )
          FillUsenetServersCombo( hwnd, IDD_LIST, psi->pszNewsServer );
     else
          {
          char szDefSrvr[ 64 ];

          Amuser_GetPPString( szNews, "Default Server", szNewsServer, szDefSrvr, 64 );
          FillUsenetServersCombo( hwnd, IDD_LIST, szDefSrvr );
          }

     /* Disable 'Get All Headers' if news server is cixnews.
      */
     VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
     index = ComboBox_GetCurSel( hwndList );
     if (index >= 0)
     {
          ComboBox_GetLBText( hwndList, index, szSubNewsServer );
          if( strcmp( szSubNewsServer, szCixnews ) == 0 )
               {
               EnableControl( hwnd, IDD_GETALLHDRS, FALSE );
               fGetLast50 = TRUE;
               }
          else
               EnableControl( hwnd, IDD_GETALLHDRS, TRUE );
     }
     else
          EnableControl( hwnd, IDOK, FALSE );

     /* Set default to get 50 most recent articles.
      */
     CheckDlgButton( hwnd, IDD_GETLAST50, fGetLast50 );
     CheckDlgButton( hwnd, IDD_GETALLHDRS, !fGetLast50 );
     CheckDlgButton( hwnd, IDD_GETALLBODIES, fGetAllBodies );
     if( fUseInternet)
          ShowWindow( GetDlgItem( hwnd, IDD_ADD ), SW_SHOW );
     return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */ 
void FASTCALL SubscribeDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
     switch( id )
          {
          case IDD_ADD: {
               NEWSSERVERINFO nsi;
               LPARAM dwServer;

               /* Get the new news server details.
                */
               dwServer = CmdAddNewsServer( hwnd, &nsi );
               if( 0L != dwServer )
                    {
                    HWND hwndList;
                    int index;

                    /* Add it to the combobox.
                     */
                    VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
                    index = ComboBox_AddString( hwndList, nsi.szServerName );
                    ComboBox_SetItemData( hwndList, index, dwServer );
                    ComboBox_SetCurSel( hwndList, index );
                    EnableControl( hwnd, IDOK, TRUE );
                    }
               break;
               }

          case IDD_LIST:
               if( codeNotify == CBN_SELCHANGE )
                    {
                    HWND hwndList;
                    int index;

                    /* Enable or disable 'Get all headers' if news server is
                     * cixnews.
                     */
                    VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
                    index = ComboBox_GetCurSel( hwndList );
                    ASSERT( CB_ERR != index );
                    ComboBox_GetLBText( hwndList, index, szSubNewsServer );
                    if( strcmp( szSubNewsServer, szCixnews ) == 0 )
                         {
                         EnableControl( hwnd, IDD_GETALLHDRS, FALSE );
                         CheckDlgButton( hwnd, IDD_GETLAST50, TRUE );
                         }
                    else
                         EnableControl( hwnd, IDD_GETALLHDRS, TRUE );
                    }
               break;

          case IDD_EDIT:
               if( codeNotify == EN_UPDATE )
               {
                    HWND hwndList;
                    VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
                    EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndCtl ) > 0 && ComboBox_GetCurSel( hwndList ) >= 0 );
               }
               break;

          case IDOK: {
               NEWARTICLESOBJECT nh;
               char sz[ 80 ];
               HWND hwndList;
               int index;
               PCAT pcat;
               PCL pcl;
               PTL ptl;
               BOOL fSuccess = FALSE;

               /* Get the newsgroup name.
                */
               GetDlgItemText( hwnd, IDD_EDIT, sz, 80 );
               StripLeadingTrailingSpaces( sz );

               /* Verify that this isn't a blank string (ie. it
                * was just spaces) and if so, force the input to
                * NULL again.
                */
               if( *sz == '\0' )
                    {
                    SetDlgItemText( hwnd, IDD_EDIT, "" );
                    EnableControl( hwnd, IDOK, FALSE );
                    break;
                    }

               /* Finally, add ourselves to the News folder.
                */
               ptl = NULL;
               for( pcat = Amdb_GetFirstCategory(); pcat; pcat = Amdb_GetNextCategory( pcat ) )
               {
                    for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
                    {
                         if( ( ptl = Amdb_OpenTopic( pcl, sz ) ) != NULL )
                         {
                              if( Amdb_GetTopicType( ptl )  == FTYPE_NEWS )
                              {
                              fSuccess = TRUE;
                              break;
                              }
                         }
                    }
                    if( fSuccess )
                         break;
               }

               /* If newsgroup not already in database.
                */
               if( NULL == ptl )
                    {
                    GetNewsFolder();
                    if( NULL == ( ptl = Amdb_CreateTopic( pclNewsFolder, sz, FTYPE_NEWS ) ) )
                         {
                         fMessageBox( hwnd, 0, GS(IDS_STR1045), MB_OK|MB_ICONINFORMATION );
                         break;
                         }
                    Amdb_CommitDatabase( FALSE );
                    }
               else if( Amdb_GetTopicType( ptl ) != FTYPE_NEWS )
                    {
                    /* Retype the folder.
                     */
                    Amdb_SetTopicType( ptl, FTYPE_NEWS );
                    if( NULL != hwndInBasket )
                         SendMessage( hwndInBasket, WM_UPDATE_UNREAD, 0, (LPARAM)ptl );
                    }

               /* Clear the resigned flag, if any.
                */
               Amdb_SetTopicFlags( ptl, TF_RESIGNED, FALSE );

               /* Get the selected news server.
                */
               VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
               index = ComboBox_GetCurSel( hwndList );
               ASSERT( CB_ERR != index );
               ComboBox_GetLBText( hwndList, index, szSubNewsServer );

               /* Save cookie.
                */
               Amdb_SetCookie( ptl, NEWS_SERVER_COOKIE, szSubNewsServer );

               /* Do we only get the last 50 articles?
                */
               if( fGetLast50 = IsDlgButtonChecked( hwnd, IDD_GETLAST50 ) )
                    Amdb_SetTopicFlags( ptl, TF_READ50, TRUE );
               if( fGetAllBodies = IsDlgButtonChecked( hwnd, IDD_GETALLBODIES ) )
                    Amdb_SetTopicFlags( ptl, TF_READFULL, TRUE );

               /* Now create an out-basket command to retrieve new articles
                * from this group.
                */
               if( strcmp( szSubNewsServer, szCixnews ) == 0 )
                    {
                    JOINUNETOBJECT ju;

                    Amob_New( OBTYPE_USENETJOIN, &ju );
                    Amob_CreateRefString( &ju.recGroupName, (char *)Amdb_GetTopicName( ptl ) );
                    Amob_SaveRefObject( &ju.recGroupName );
                    ju.wArticles = 50;
                    if( !Amob_Find( OBTYPE_USENETJOIN, &ju ) )
                         Amob_Commit( NULL, OBTYPE_USENETJOIN, &ju );
                    Amob_Delete( OBTYPE_USENETJOIN, &ju );
                    }
               else if( fOnline )
                    {
                    Amob_New( OBTYPE_GETNEWARTICLES, &nh );
                    WriteFolderPathname( sz, (LPVOID)ptl );
                    Amob_CreateRefString( &nh.recFolder, sz );
                    Amob_SaveRefObject( &nh.recFolder );
                    if( !Amob_Find( OBTYPE_GETNEWARTICLES, &nh ) )
                         Amob_Commit( NULL, OBTYPE_GETNEWARTICLES, &nh );
                    Amob_Delete( OBTYPE_GETNEWARTICLES, &nh );
                    }
               EndDialog( hwnd, TRUE );
               break;
               }

          case IDCANCEL:
               EndDialog( hwnd, FALSE );
               break;
          }
}

/* This is the CIX Usenet join out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_UsenetJoin( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
     JOINUNETOBJECT FAR * lpju;

     lpju = (JOINUNETOBJECT FAR *)dwData;
     switch( uType )
          {
          case OBEVENT_EXEC:
               return( POF_HELDBYSCRIPT );

          case OBEVENT_GETCLSID:
               return( Amob_GetNextObjectID() );

          case OBEVENT_DESCRIBE:
               wsprintf( lpBuf, GS(IDS_STR225), DRF(lpju->recGroupName) );
               return( TRUE );

          case OBEVENT_PERSISTENCE:
               return( TRUE );

          case OBEVENT_NEW:
               _fmemset( lpju, 0, sizeof(JOINUNETOBJECT) );
               return( TRUE );

          case OBEVENT_LOAD: {
               JOINUNETOBJECT juo;

               INITIALISE_PTR(lpju);
               Amob_LoadDataObject( fh, &juo, sizeof(JOINUNETOBJECT) );
               if( fNewMemory( &lpju, sizeof(JOINUNETOBJECT) ) )
                    {
                    *lpju = juo;
                    lpju->recGroupName.hpStr = NULL;
                    }
               return( (LRESULT)lpju );
               }

          case OBEVENT_SAVE:
               Amob_SaveRefObject( &lpju->recGroupName );
               return( Amob_SaveDataObject( fh, lpju, sizeof(JOINUNETOBJECT) ) );

          case OBEVENT_COPY: {
               JOINUNETOBJECT FAR * lpjuNew;

               INITIALISE_PTR( lpjuNew );
               if( fNewMemory( &lpjuNew, sizeof(JOINUNETOBJECT) ) )
                    {
                    INITIALISE_PTR( lpjuNew->recGroupName.hpStr );
                    Amob_CopyRefObject( &lpjuNew->recGroupName, &lpju->recGroupName );
                    lpjuNew->wArticles = lpju->wArticles;
                    }
               return( (LRESULT)lpjuNew );
               }

          case OBEVENT_FIND: {
               JOINUNETOBJECT FAR * lpju1;
               JOINUNETOBJECT FAR * lpju2;

               lpju1 = (JOINUNETOBJECT FAR *)dwData;
               lpju2 = (JOINUNETOBJECT FAR *)lpBuf;
               return( strcmp( lpju1->recGroupName.hpStr, lpju2->recGroupName.hpStr ) == 0 );
               }

          case OBEVENT_REMOVE:
               FreeMemory( &lpju );
               return( TRUE );

          case OBEVENT_DELETE:
               Amob_FreeRefObject( &lpju->recGroupName );
               return( TRUE );
          }
     return( 0L );
}
