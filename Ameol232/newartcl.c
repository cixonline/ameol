/* NEWARTCL.C - Routines to handle the Get New Articles object
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
#include <memory.h>
#include "cixip.h"
#include "amlib.h"
#include "rules.h"
#include "news.h"

static BOOL fDefDlgEx = FALSE;

LRESULT FASTCALL GetNewArticlesDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL GetNewArticlesDlg_OnCommand( HWND, int, HWND, UINT );
void FASTCALL GetNewArticlesDlg_OnPaint( HWND );
BOOL FASTCALL GetNewArticlesDlg_OnInitDialog( HWND, HWND, LPARAM );

/* This is the Get New Articles out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_GetNewArticles( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   NEWARTICLESOBJECT FAR * lpnho;

   lpnho = (NEWARTICLESOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_EXEC:
         if( lpnho->wService != W_SERVICE_BOTH )
            {
            if( lpnho->wService & W_SERVICE_CIX )
               return( POF_HELDBYSCRIPT );
            return( Exec_GetNewArticles( dwData ) );
            }
         if( !IsNNTPInFolderList( DRF(lpnho->recFolder) ) )
            return( POF_HELDBYSCRIPT );
         return( Exec_GetNewArticles( dwData ) );

      case OBEVENT_EDITABLE:
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR192), Amob_DerefObject( &lpnho->recFolder ) );
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_NEW:
         _fmemset( lpnho, 0, sizeof( NEWARTICLESOBJECT ) );
         lpnho->wService = W_SERVICE_BOTH;
         return( TRUE );

      case OBEVENT_LOAD: {
         NEWARTICLESOBJECT nho;

         INITIALISE_PTR(lpnho);
         Amob_LoadDataObject( fh, &nho, sizeof( NEWARTICLESOBJECT ) );
         if( fNewMemory( &lpnho, sizeof( NEWARTICLESOBJECT ) ) )
            {
            *lpnho = nho;
            lpnho->recFolder.hpStr = NULL;
            }
         return( (LRESULT)lpnho );
         }

      case OBEVENT_EDIT: {
         OBINFO obinfo;

         Amob_GetObInfo( (LPOB)dwData, &obinfo );
         Adm_Dialog( hInst, hwndFrame, MAKEINTRESOURCE(IDDLG_GETNEWHEADERS), GetNewArticlesDlg, (LPARAM)obinfo.lpObData );
         Amob_Commit( (LPOB)dwData, OBTYPE_GETNEWARTICLES, obinfo.lpObData );
         return( 0L );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpnho->recFolder );
         return( Amob_SaveDataObject( fh, lpnho, sizeof( NEWARTICLESOBJECT ) ) );

      case OBEVENT_COPY: {
         NEWARTICLESOBJECT FAR * lpnhoNew;

         INITIALISE_PTR( lpnhoNew );
         if( fNewMemory( &lpnhoNew, sizeof( NEWARTICLESOBJECT ) ) )
            {
            lpnhoNew->wService = lpnho->wService;
            INITIALISE_PTR( lpnhoNew->recFolder.hpStr );
            Amob_CopyRefObject( &lpnhoNew->recFolder, &lpnho->recFolder );
            }
         return( (LRESULT)lpnhoNew );
         }

      case OBEVENT_FIND: {
         NEWARTICLESOBJECT FAR * lpnho1;
         NEWARTICLESOBJECT FAR * lpnho2;

         lpnho1 = (NEWARTICLESOBJECT FAR *)dwData;
         lpnho2 = (NEWARTICLESOBJECT FAR *)lpBuf;
         return( lstrcmp( Amob_DerefObject( &lpnho1->recFolder ), Amob_DerefObject( &lpnho2->recFolder ) ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpnho );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpnho->recFolder );
         return( TRUE );
      }
   return( 0L );
}

/* Handles the Subscribe dialog.
 */
BOOL EXPORT CALLBACK GetNewArticlesDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, GetNewArticlesDlg_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the Get New Newsgroups dialog.
 */
LRESULT FASTCALL GetNewArticlesDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, GetNewArticlesDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, GetNewArticlesDlg_OnCommand );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, FolderCombo_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_DRAWITEM, FolderListCombo_OnDrawItem );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsGETNEWHEADERS );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL GetNewArticlesDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   NEWARTICLESOBJECT FAR * lpnh;
   HWND hwndList;
   int index;

   /* Save the handle to the NEWARTICLESOBJECT object.
    */
   SetWindowLong( hwnd, DWL_USER, lParam );
   lpnh = (NEWARTICLESOBJECT FAR *)lParam;

   /* Show list of newsgroups.
    */
   FillListWithNewsgroups( hwnd, IDD_LIST );

   /* Select the current object.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   Amob_DerefObject( &lpnh->recFolder );
   index = 0;
   if( lpnh->recFolder.dwSize )
      {
      LPVOID pData;

      ParseFolderPathname( lpnh->recFolder.hpStr, &pData, FALSE, FTYPE_NEWS );
      if( pData )
         index = ComboBox_FindItemData( hwndList, -1, pData );
      }
   ComboBox_SetCurSel( hwndList, index );

   /* Disable OK if nothing selected.
    */
   index = ComboBox_GetCurSel( hwndList );
   EnableControl( hwnd, IDOK, index != CB_ERR );
   return( FALSE );
}

/* This function handles the WM_COMMAND message.
 */ 
void FASTCALL GetNewArticlesDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_LIST:
         if( codeNotify == CBN_SELCHANGE )
            EnableControl( hwnd, IDOK, TRUE );
         break;

      case IDOK: {
         NEWARTICLESOBJECT FAR * lpnh;
         HWND hwndList;
         int index;
         char sz[ 100 ];
         LPVOID pData;

         /* Get the handle of the database under which we
          * create this folder.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         index = ComboBox_GetCurSel( hwndList );
         pData = (LPVOID)ComboBox_GetItemData( hwndList, index );

         /* Create the folder.
          */
         lpnh = (NEWARTICLESOBJECT FAR *)GetWindowLong( hwnd, DWL_USER );
         Amob_FreeRefObject( &lpnh->recFolder );
         WriteFolderPathname( sz, pData );
         Amob_CreateRefString( &lpnh->recFolder, sz );
         EndDialog( hwnd, TRUE );
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}

/* This function fills the combo box with a list of all newsgroups
 * in every database.
 */
void FASTCALL FillListWithNewsgroups( HWND hwnd, int id )
{
   HWND hwndList;
   int index;
   PCAT pcat;
   PCL pcl;
   PTL ptl;

   hwndList = GetDlgItem( hwnd, id );
   ComboBox_ResetContent( hwndList );
   SetWindowRedraw( hwndList, FALSE );
   for( pcat = Amdb_GetFirstCategory(); pcat; pcat = Amdb_GetNextCategory( pcat ) )
      for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
         for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
            if( FTYPE_NEWS == Amdb_GetTopicType( ptl ) )
               {
               index = ComboBox_AddString( hwndList, Amdb_GetTopicName( ptl ) );
               ComboBox_SetItemData( hwndList, index, (LPARAM)ptl );
               }
   SetWindowRedraw( hwndList, TRUE );
   ComboBox_SetCurSel( hwndList, 0 );
   ComboBox_SetExtendedUI( hwndList, TRUE );
}
