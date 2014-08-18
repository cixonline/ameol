/* IMPORT.C - Implements the File Import command
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
#include <direct.h>
#include <io.h>
#include <string.h>
#include <dos.h>
#include "amlib.h"

#define  THIS_FILE   __FILE__

static BOOL fDefDlgEx = FALSE;

BOOL EXPORT CALLBACK ImportDlg( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL ImportDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL ImportDlg_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL ImportDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );

typedef void (FAR PASCAL * IMPORTPROC)(HWND, LPSTR);

/* This function implements the Import command.
 */
BOOL FASTCALL CmdImport( HWND hwnd )
{
   char sz[ 100 ];

   if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_IMPORT), ImportDlg, (LPARAM)(LPSTR)sz ) )
      {
      char szImport[ 80 ];
      char * pszProcName;

      /* Get the import filter details.
       */
      Amuser_GetLMString( "Import", sz, "", szImport, sizeof(szImport) );
      if( NULL == ( pszProcName = strchr( szImport, '|' ) ) )
         {
         if( WinExec( szImport, SW_SHOWNORMAL ) < 32 )
            {
            wsprintf( lpTmpBuf, GS(IDS_STR828), (LPSTR)szImport );
            fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
            return( FALSE );
            }
         }
      else
         {
         char szModule[ 144 ];
         char * pszModule;
         HINSTANCE hImpLib;
         HINSTANCE hAppInst;
         IMPORTPROC lpImpProc;

         /* Set pszProcName to point to the procedure entry
          * address for this import filter.
          */
         *pszProcName++ = '\0';
         GetModuleFileName( hInst, szModule, sizeof(szModule) );
         pszModule = GetFileBasename( szModule );

         /* Strip off quotes from procedure name.
          */
         StripLeadingTrailingQuotes( pszProcName );

         /* Load the module if this isn't the main executable
          */
      #ifdef WIN32
         hAppInst = hInst;
      #else
         hAppInst = NULL;
      #endif
         hImpLib = hAppInst;
         _strupr( szImport );
         if( _strcmpi( pszModule, szImport ) )
            {
            if( ( hImpLib = LoadLibrary( szImport ) ) < (HINSTANCE)32 )
               {
               wsprintf( lpTmpBuf, GS(IDS_STR821), (LPSTR)szImport );
               fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
               return( FALSE );
               }
            }

         /* Find the import procedure entry point in the
          * main executable.
          */
         if( NULL == ( lpImpProc = (IMPORTPROC)GetProcAddress( hImpLib, pszProcName ) ) )
            {
            wsprintf( lpTmpBuf, GS(IDS_STR822), (LPSTR)pszProcName, (LPSTR)szImport );
            fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
            return( FALSE );
            }

         /* If we get here, we've got an import procedure entry point, so
          * call it and wait for it to do the biz.
          */
         lpImpProc( hwnd, NULL );

         /* Clean up after us.
          */
         if( hImpLib != hAppInst )
            FreeLibrary( hImpLib );
         }
      }
   return( TRUE );
}

/* This function handles the Import dialog box.
 */
BOOL EXPORT CALLBACK ImportDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = ImportDlg_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the Import
 * dialog.
 */
LRESULT FASTCALL ImportDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, ImportDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, ImportDlg_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsIMPORT );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL ImportDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPSTR lpszImporters;
   HWND hwndList;

   INITIALISE_PTR(lpszImporters);

   /* Fill with a list of import filters from
    * the [Import] section of AMEOL2.INI
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   if( fNewMemory( &lpszImporters, 16000 ) )
      {
      LPSTR lp;

      Amuser_GetLMString( szImport, NULL, "", lpszImporters, 16000 );
      lp = lpszImporters;
      while( *lp )
         {
         if( ListBox_InsertString( hwndList, -1, lp ) == LB_ERR )
            break;
         lp += lstrlen( lp ) + 1;
         }
      FreeMemory( &lpszImporters );
      }

   /* Disable listbox and OK button if no import
    * filters.
    */
   if( ListBox_GetCount( hwndList ) )  
      ListBox_SetCurSel( hwndList, 0 );
   else
      {
      ListBox_AddString( hwndList, GS(IDS_STR827) );
      EnableControl( hwnd, IDOK, FALSE );
      EnableWindow( hwndList, FALSE );
      }
   SetWindowLong( hwnd, DWL_USER, lParam );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL ImportDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_LIST:
         if( codeNotify != LBN_DBLCLK )
            break;
         id = IDOK;

      case IDOK: {
         HWND hwndList;
         char sz[ 100 ];
         int index;

         /* Get and save the current selection.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         if( ( index = ListBox_GetCurSel( hwndList ) ) != LB_ERR )
            {
            LPSTR lpText;

            ListBox_GetText( hwndList, index, sz );
            lpText = (LPSTR)GetWindowLong( hwnd, DWL_USER );
            lstrcpy( lpText, sz );
            }
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}
