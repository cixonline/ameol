/* NEWTAGGD.C - Routines to handle the Get Marked Articles object
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
#include "cixip.h"
#include "amlib.h"
#include "rules.h"
#include "news.h"

static BOOL fDefDlgEx = FALSE;

LRESULT FASTCALL GetTaggedDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL GetTaggedDlg_OnCommand( HWND, int, HWND, UINT );
void FASTCALL GetTaggedDlg_OnPaint( HWND );
BOOL FASTCALL GetTaggedDlg_OnInitDialog( HWND, HWND, LPARAM );

/* This is the Get Tagged out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_GetTagged( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   GETTAGGEDOBJECT FAR * lpgto;

   lpgto = (GETTAGGEDOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_EXEC:
         if( lpgto->wService != W_SERVICE_BOTH )
            {
            if( lpgto->wService & W_SERVICE_CIX )
               return( POF_HELDBYSCRIPT );
            return( Exec_GetTagged( dwData ) );
            }
         if( !IsNNTPInFolderList( DRF(lpgto->recFolder) ) )
            return( POF_HELDBYSCRIPT );
         return( Exec_GetTagged( dwData ) );

      case OBEVENT_EDITABLE:
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR193), Amob_DerefObject( &lpgto->recFolder ) );
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_NEW:
         _fmemset( lpgto, 0, sizeof( GETTAGGEDOBJECT ) );
         lpgto->wService = W_SERVICE_BOTH;
         return( TRUE );

      case OBEVENT_LOAD: {
         GETTAGGEDOBJECT gto;

         INITIALISE_PTR(lpgto);
         Amob_LoadDataObject( fh, &gto, sizeof( GETTAGGEDOBJECT ) );
         if( fNewMemory( &lpgto, sizeof( GETTAGGEDOBJECT ) ) )
            {
            *lpgto = gto;
            lpgto->recFolder.hpStr = NULL;
            }
         return( (LRESULT)lpgto );
         }

      case OBEVENT_EDIT: {
         OBINFO obinfo;

         Amob_GetObInfo( (LPOB)dwData, &obinfo );
         Adm_Dialog( hInst, hwndFrame, MAKEINTRESOURCE(IDDLG_GETTAGGEDARTICLES), GetTaggedDlg, (LPARAM)obinfo.lpObData );
         Amob_Commit( (LPOB)dwData, OBTYPE_GETTAGGED, obinfo.lpObData );
         return( 0L );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpgto->recFolder );
         return( Amob_SaveDataObject( fh, lpgto, sizeof( GETTAGGEDOBJECT ) ) );

      case OBEVENT_COPY: {
         GETTAGGEDOBJECT FAR * lpgtoNew;

         INITIALISE_PTR( lpgtoNew );
         if( fNewMemory( &lpgtoNew, sizeof( GETTAGGEDOBJECT ) ) )
            {
            lpgtoNew->wService = lpgto->wService;
            INITIALISE_PTR( lpgtoNew->recFolder.hpStr );
            Amob_CopyRefObject( &lpgtoNew->recFolder, &lpgto->recFolder );
            }
         return( (LRESULT)lpgtoNew );
         }

      case OBEVENT_FIND: {
         GETTAGGEDOBJECT FAR * lpgto1;
         GETTAGGEDOBJECT FAR * lpgto2;

         lpgto1 = (GETTAGGEDOBJECT FAR *)dwData;
         lpgto2 = (GETTAGGEDOBJECT FAR *)lpBuf;
         return( lstrcmp( Amob_DerefObject( &lpgto1->recFolder ), Amob_DerefObject( &lpgto2->recFolder ) ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpgto );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpgto->recFolder );
         return( TRUE );
      }
   return( 0L );
}

/* Handles the Get Tagged dialog.
 */
BOOL EXPORT CALLBACK GetTaggedDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, GetTaggedDlg_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the Get New Newsgroups dialog.
 */
LRESULT FASTCALL GetTaggedDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, GetTaggedDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, GetTaggedDlg_OnCommand );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, FolderCombo_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_DRAWITEM, FolderListCombo_OnDrawItem );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsGETTAGGEDARTICLES );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}


/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL GetTaggedDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   GETTAGGEDOBJECT FAR * lpnh;
   HWND hwndList;
   int index;

   /* Save the handle to the GETTAGGEDOBJECT object.
    */
   SetWindowLong( hwnd, DWL_USER, lParam );
   lpnh = (GETTAGGEDOBJECT FAR *)lParam;

   /* Show list of databases and folders.
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
void FASTCALL GetTaggedDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_LIST:
         if( codeNotify == CBN_SELCHANGE )
            EnableControl( hwnd, IDOK, TRUE );
         break;

      case IDOK: {
         GETTAGGEDOBJECT FAR * lpnh;
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
         lpnh = (GETTAGGEDOBJECT FAR *)GetWindowLong( hwnd, DWL_USER );
         WriteFolderPathname( sz, pData );
         Amob_FreeRefObject( &lpnh->recFolder );
         Amob_CreateRefString( &lpnh->recFolder, sz );
         EndDialog( hwnd, TRUE );
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}
