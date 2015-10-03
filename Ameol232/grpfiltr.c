/* GRPFILTR.C - Newsgroup filtering
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
#include <ctype.h>
#include "amlib.h"
#include "news.h"
#include <memory.h>
#include <string.h>

#define  THIS_FILE   __FILE__

static BOOL fDefDlgEx = FALSE;

/* Filter list.
 */
#define  NFT_ACCEPTALL           1
#define  NFT_ACCEPTBUT           2
#define  NFT_REJECTBUT           3

BOOL EXPORT CALLBACK GroupFilterDlg( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL GroupFilterDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL GroupFilterDlg_OnCommand( HWND, int, HWND, UINT );
void FASTCALL GroupFilterDlg_OnPaint( HWND );
BOOL FASTCALL GroupFilterDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL GroupFilterDlg_ShowReason( HWND );

/* This function displays the Newsgroups filter dialog.
 */
void FASTCALL CmdNewsgroupsFilter( HWND hwnd, char * pszNewsServer )
{
   Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_NEWSGROUPSFILTER), GroupFilterDlg, (LPARAM)pszNewsServer );
}

/* Handles the Newsgroup filter dialog.
 */
BOOL EXPORT CALLBACK GroupFilterDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, GroupFilterDlg_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the dialog.
 */
LRESULT FASTCALL GroupFilterDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, GroupFilterDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, GroupFilterDlg_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsNEWSGROUPSFILTER );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL GroupFilterDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   NEWSSERVERINFO nsi;
   char * pszNewsServer;
   LPSTR lpFilterBuf;
   HNDFILE fhFilter;
   WORD fNewsFilterType;

   INITIALISE_PTR(lpFilterBuf);

   /* Get the news server name.
    */
   pszNewsServer = (char *)lParam;
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* Open the filter file for that news server.
    */
   GetUsenetServerDetails( pszNewsServer, &nsi );
   BEGIN_PATH_BUF;
   wsprintf( lpPathBuf, "%s.flt", nsi.szShortName );
   fhFilter = Amfile_Open( lpPathBuf, AOF_READ );
   END_PATH_BUF;

   /* If we can't find the file, set defaults.
    */
   if( HNDFILE_ERROR == fhFilter )
      fNewsFilterType = NFT_ACCEPTALL;
   else
      {
      /* Read the filter type.
       */
      Amfile_Read( fhFilter, &fNewsFilterType, sizeof(WORD) );

      /* If this is not NFT_ACCEPTALL, then read the list of
       * filters.
       */
      if( fNewsFilterType != NFT_ACCEPTALL )
         {
         DWORD dwFileSize;
         HPSTR lpFilters;

         INITIALISE_PTR(lpFilters);

         /* Get the file size and read in the file into a block
          * of memory. Each line will be CR/LF terminated.
          */
         dwFileSize = Amfile_GetFileSize( fhFilter ) - sizeof(WORD);
         if( fNewMemory32( &lpFilters, dwFileSize + 1 ) )
            {
            DWORD dwRead;
            HWND hwndEdit;

            /* Read the list.
             */
            dwRead = Amfile_Read32( fhFilter, lpFilters, dwFileSize );
            if( dwRead != dwFileSize )
               DiskReadError( hwnd, lpPathBuf, FALSE, FALSE );
            else
               {
               /* NULL terminate the list.
                */
               lpFilters[ dwRead ] = '\0';

               /* Set the edit field directly from the list.
                */
               hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
               Edit_SetText( hwndEdit, lpFilters );
               }
            FreeMemory32( &lpFilters );
            }
         }
      Amfile_Close( fhFilter );
      }

   /* Set the options.
    */
   CheckDlgButton( hwnd, IDD_ACCEPTALL, fNewsFilterType == NFT_ACCEPTALL );
   CheckDlgButton( hwnd, IDD_ACCEPTBUT, fNewsFilterType == NFT_ACCEPTBUT );
   CheckDlgButton( hwnd, IDD_REJECTBUT, fNewsFilterType == NFT_REJECTBUT );
   GroupFilterDlg_ShowReason( hwnd );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */ 
void FASTCALL GroupFilterDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_ACCEPTALL:
      case IDD_ACCEPTBUT:
      case IDD_REJECTBUT:
         GroupFilterDlg_ShowReason( hwnd );
         break;

      case IDOK: {
         char * pszNewsServer;
         WORD fNewsFilterType;
         NEWSSERVERINFO nsi;
         HNDFILE fhFilter;

         /* Create the filter file.
          */
         pszNewsServer = (char *)GetWindowLong( hwnd, DWL_USER );
         GetUsenetServerDetails( pszNewsServer, &nsi );
         BEGIN_PATH_BUF;
         wsprintf( lpPathBuf, "%s.flt", nsi.szShortName );
         fhFilter = Amfile_Create( lpPathBuf, 0 );
         END_PATH_BUF;
         if( HNDFILE_ERROR == fhFilter )
            {
            FileCreateError( hwnd, lpPathBuf, FALSE, FALSE );
            break;
            }

         /* Get filter options.
          */
         if( IsDlgButtonChecked( hwnd, IDD_ACCEPTALL ) )
            fNewsFilterType = NFT_ACCEPTALL;
         else if( IsDlgButtonChecked( hwnd, IDD_ACCEPTBUT ) )
            fNewsFilterType = NFT_ACCEPTBUT;
         else
            fNewsFilterType = NFT_REJECTBUT;

         /* Write filter mode as first byte.
          */
         if( Amfile_Write( fhFilter, (LPSTR)&fNewsFilterType, sizeof(WORD) ) != sizeof(WORD) )
            {
            DiskWriteError( hwnd, lpPathBuf, FALSE, FALSE );
            Amfile_Close( fhFilter );
            break;
            }

         /* For the accept-but and reject-but filter types,
          * get, parse and store the list of filters.
          */
         if( NFT_ACCEPTALL != fNewsFilterType )
            {
            LPSTR lpFilterList;
            UINT cbFilterList;
            HWND hwndEdit;

            INITIALISE_PTR(lpFilterList);

            /* Get the filter list from the edit control into
             * a local buffer.
             */
            hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
            cbFilterList = Edit_GetTextLength( hwndEdit );
            if( fNewMemory( &lpFilterList, cbFilterList + 1 ) )
               {
               Edit_GetText( hwndEdit, lpFilterList, cbFilterList + 1 );
               if( Amfile_Write( fhFilter, lpFilterList, cbFilterList ) != cbFilterList )
                  {
                  FreeMemory( &lpFilterList );
                  DiskWriteError( hwnd, lpPathBuf, FALSE, FALSE );
                  Amfile_Close( fhFilter );
                  break;
                  }
               FreeMemory( &lpFilterList );
               }
            else
               {
               OutOfMemoryError( hwnd, FALSE, FALSE );
               break;
               }
            }
         Amfile_Close( fhFilter );

         /* Filters changed, so apply it to the next
          * download of the file.
          */
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}

/* This function changes the text and edit control depending on which of
 * the radio buttons were selected.
 */
void FASTCALL GroupFilterDlg_ShowReason( HWND hwnd )
{
   if( IsDlgButtonChecked( hwnd, IDD_ACCEPTALL ) )
      {
      ShowWindow( GetDlgItem( hwnd, IDD_REASON ), SW_HIDE );
      ShowWindow( GetDlgItem( hwnd, IDD_EDIT ), SW_HIDE );
      }
   else
      {
      ShowWindow( GetDlgItem( hwnd, IDD_REASON ), SW_SHOW );
      ShowWindow( GetDlgItem( hwnd, IDD_EDIT ), SW_SHOW );
      if( IsDlgButtonChecked( hwnd, IDD_ACCEPTBUT ) )
         SetDlgItemText( hwnd, IDD_REASON, GS(IDS_STR929) );
      else
         SetDlgItemText( hwnd, IDD_REASON, GS(IDS_STR930) );
      }
}

/* This function reads the filters for the specified news server
 * into a buffer.
 */
LPSTR FASTCALL LoadFilters( NEWSSERVERINFO FAR * lpnsi, WORD * pfNewsFilterType )
{
   HNDFILE fhFilter;
   LPSTR lpFilters;

   INITIALISE_PTR(lpFilters);

   /* Open the file.
    */
   BEGIN_PATH_BUF;
   wsprintf( lpPathBuf, "%s.flt", lpnsi->szShortName );
   fhFilter = Amfile_Open( lpPathBuf, AOF_READ );
   END_PATH_BUF;

   /* If it exists, read it into a buffer.
    */
   if( HNDFILE_ERROR != fhFilter )
      {
      DWORD dwSize;

      dwSize = Amfile_GetFileSize( fhFilter );
      if( fNewMemory( &lpFilters, (UINT)dwSize + 1 ) )
         {
         WORD fNewsFilterType;
         UINT cRead;

         /* Read the filter type.
          */
         Amfile_Read( fhFilter, &fNewsFilterType, sizeof(WORD) );
         dwSize -= 2;

         /* Read the rest of the stuff
          */
         if( ( cRead = Amfile_Read( fhFilter, lpFilters, (UINT)dwSize ) ) != (UINT)dwSize )
            FreeMemory( &lpFilters );
         else
            {
            char * lpCompctFilters;

            /* Make a compact string out of the filters
             * list.
             */
            lpFilters[ cRead ] = '\0';
            lpCompctFilters = MakeCompactString( lpFilters );
            FreeMemory( &lpFilters );
            lpFilters = lpCompctFilters;
            *pfNewsFilterType = fNewsFilterType;
            }
         }
      Amfile_Close( fhFilter );
      }

   /* Return pointer to filter buffer.
    */
   return( lpFilters );
}

/* This function determines whether the newsgroup name in
 * pStrToMatch is matched by one of the filters.
 */
BOOL FASTCALL MatchFilters( LPSTR lpFilters, WORD wFilterType, char * pStrToMatch )
{
   if( wFilterType != NFT_ACCEPTALL )
      while( *lpFilters )
         {
         if( memcmp( pStrToMatch, lpFilters, strlen(lpFilters) ) == 0 )
            {
            if( wFilterType == NFT_ACCEPTBUT )
               return( TRUE );
            return( FALSE );
            }
         lpFilters += strlen( lpFilters ) + 1;
         }
   return( wFilterType == NFT_REJECTBUT );
}
