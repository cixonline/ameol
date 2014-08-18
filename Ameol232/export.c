/* EXPORT.C - Implements the File Export command
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

BOOL EXPORT CALLBACK ExportDlg( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL ExportDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL ExportDlg_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL ExportDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );

typedef void (FAR PASCAL * EXPORTPROC)(HWND);

/* This function implements the Export command.
 */
BOOL FASTCALL CmdExport( HWND hwnd )
{
   char sz[ 100 ];

   if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_EXPORT), ExportDlg, (LPARAM)(LPSTR)sz ) )
      {
      char szExport[ 80 ];
      char * pszProcName;

      /* Get the export filter details.
       */
      Amuser_GetLMString( "Export", sz, "", szExport, sizeof(szExport) );
      if( NULL == ( pszProcName = strchr( szExport, '|' ) ) )
         {
         if( WinExec( szExport, SW_SHOWNORMAL ) < 32 )
            {
            wsprintf( lpTmpBuf, GS(IDS_STR828), (LPSTR)szExport );
            fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
            return( FALSE );
            }
         }
      else
         {
         char szModule[ 144 ];
         char * pszModule;
         HINSTANCE hExpLib;
         HINSTANCE hAppInst;
         EXPORTPROC lpExpProc;

         /* Set pszProcName to point to the procedure entry
          * address for this export filter.
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
         hExpLib = hAppInst;
         _strupr( szExport );
         if( _strcmpi( pszModule, szExport ) )
            {
            if( ( hExpLib = LoadLibrary( szExport ) ) < (HINSTANCE)32 )
               {
               wsprintf( lpTmpBuf, GS(IDS_STR821), (LPSTR)szExport );
               fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
               return( FALSE );
               }
            }

         /* Find the export procedure entry point in the
          * main executable.
          */
         if( NULL == ( lpExpProc = (EXPORTPROC)GetProcAddress( hExpLib, pszProcName ) ) )
            {
            wsprintf( lpTmpBuf, GS(IDS_STR822), (LPSTR)pszProcName, (LPSTR)szExport );
            fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
            return( FALSE );
            }

         /* If we get here, we've got an export procedure entry point, so
          * call it and wait for it to do the biz.
          */
         lpExpProc( hwnd );

         /* Clean up after us.
          */
         if( hExpLib != hAppInst )
            FreeLibrary( hExpLib );
         }
      }
   return( TRUE );
}

/* This function handles the Export dialog box.
 */
BOOL EXPORT CALLBACK ExportDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = ExportDlg_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the Export
 * dialog.
 */
LRESULT FASTCALL ExportDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, ExportDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, ExportDlg_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsEXPORT );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL ExportDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPSTR lpszExporters;
   HWND hwndList;

   INITIALISE_PTR(lpszExporters);

   /* Fill with a list of export filters from
    * the [Export] section of AMEOL2.INI
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   if( fNewMemory( &lpszExporters, 16000 ) )
      {
      LPSTR lp;

      Amuser_GetLMString( szExport, NULL, "", lpszExporters, 16000 );
      lp = lpszExporters;
      while( *lp )
         {
         if( ListBox_InsertString( hwndList, -1, lp ) == LB_ERR )
            break;
         lp += lstrlen( lp ) + 1;
         }
      FreeMemory( &lpszExporters );
      }

   /* Disable listbox and OK button if no export
    * filters.
    */
   if( ListBox_GetCount( hwndList ) )  
      ListBox_SetCurSel( hwndList, 0 );
   else
      {
      ListBox_AddString( hwndList, GS(IDS_STR843) );
      EnableControl( hwnd, IDOK, FALSE );
      EnableWindow( hwndList, FALSE );
      }
   SetWindowLong( hwnd, DWL_USER, lParam );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL ExportDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
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
