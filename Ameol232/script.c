/* SCRIPT.C - Handles the script dialog and script database
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
#include "amlib.h"
#include <direct.h>
#include <io.h>
#include <memory.h>
#include <commdlg.h>
#include <string.h>
#include "editmap.h"

static BOOL fDefDlgEx = FALSE;   /* DefDlg recursion flag trap */

static LPSCRIPT lpFirstScript = NULL;
static LPSCRIPT lpLastScript = NULL;
static HBITMAP hbmpScript;

static char FAR szScriptsIni[ 144 ];

BOOL EXPORT CALLBACK ScriptEdit( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL ScriptEdit_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL ScriptEdit_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL ScriptEdit_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT CALLBACK Script( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL Script_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL Script_OnCommand( HWND, int, HWND, UINT );
void FASTCALL Script_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
void FASTCALL Script_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
int FASTCALL Script_OnCompareItem( HWND, const COMPAREITEMSTRUCT FAR * );
int FASTCALL Script_OnCharToItem( HWND, UINT, HWND, int );
LRESULT FASTCALL Script_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL FASTCALL RunScript_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL RunScript_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL RunScript_DlgProc( HWND, UINT, WPARAM, LPARAM );

/* This function loads all scripts in the scripts directory, indexed by
 * the SCRIPTS.INI file, into memory.
 */
void FASTCALL LoadScriptList( void )
{
   LPSTR lpScripts;
   UINT cbScripts;
   LPSTR lp;

   INITIALISE_PTR(lpScripts);

   /* Work out where the SCRIPTS.INI file is to be found.
    */
   strcat( strcpy( szScriptsIni, pszAmeolDir ), szScriptList );

   /* Allocate some global memory. Start from 32K.
    */
   cbScripts = 32768;
   while( cbScripts > 1024 && !fNewMemory( &lpScripts, cbScripts ) )
      cbScripts -= 1024;

   /* Read list of scripts from SCRIPTS.INI file. We open the file and
    * read each line by line rather than GetProfileString
    */
   GetPrivateProfileString( "Scripts", NULL, "", lp = lpScripts, cbScripts, szScriptsIni );
   while( *lp )
      {
      LPSCRIPT lpScript;

      INITIALISE_PTR(lpScript);
      if( fNewMemory( &lpScript, sizeof( SCRIPT ) ) )
         {
         GetPrivateProfileString( "Scripts", lp, "", lpScript->szDescription, MAX_DESCRIPTION, szScriptsIni );
         strcpy( lpScript->szFileName, lp );
         if( !lpFirstScript )
            lpFirstScript = lpScript;
         else
            lpLastScript->lpNext = lpScript;
         lpScript->lpNext = NULL;
         lpScript->lpPrev = lpLastScript;
         lpLastScript = lpScript;
         }
      lp += strlen( lp ) + 1;
      }
   FreeMemory( &lpScripts );
}

/* This function fills the specified combo box with a list of
 * all scripts.
 */
void FASTCALL FillScriptsList( HWND hwnd, int nID )
{
   LPSCRIPT lpScript;
   HWND hwndList;
   int count;

   hwndList = GetDlgItem( hwnd, IDD_LIST );
   for( lpScript = lpFirstScript; lpScript; lpScript = lpScript->lpNext )
      {
      int index;

      index = ComboBox_AddString( hwndList, lpScript->szDescription );
      ComboBox_SetItemData( hwndList, index, (LPARAM)lpScript );
      }
   count = ComboBox_GetCount( hwndList );
   if( count == 0 )
      {
      ComboBox_AddString( hwndList, GS(IDS_STR948) );
      ComboBox_SetCurSel( hwndList, 0 );
      }
   EnableWindow( hwndList, count > 0 );
}

/* This function displays the Script dialog.
 */
void FASTCALL CmdScripts( HWND hwnd )
{
   Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_SCRIPT), Script, 0L );
}

/* This function handles the Script dialog box.
 */
BOOL EXPORT CALLBACK Script( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, Script_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the Script
 * dialog.
 */
LRESULT FASTCALL Script_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, Script_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, Script_OnCommand );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, Script_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_DRAWITEM, Script_OnDrawItem );
      HANDLE_MSG( hwnd, WM_COMPAREITEM, Script_OnCompareItem );
      HANDLE_MSG( hwnd, WM_CHARTOITEM, Script_OnCharToItem );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSCRIPT );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL Script_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPSCRIPT lpScript;
   HWND hwndList;

   /* Fill the list box with all scripts.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   lpScript = lpFirstScript;
   while( lpScript )
      {
      ListBox_AddString( hwndList, lpScript );
      lpScript = lpScript->lpNext;
      }

   /* If no scripts, disable the edit controls.
    * Otherwise select the first one.
    */
   if( !lpFirstScript ) {
      EnableControl( hwnd, IDD_ADD, FALSE );
      EnableControl( hwnd, IDD_EDIT, FALSE );
      EnableControl( hwnd, IDD_REMOVE, FALSE );
      }
   else
      ListBox_SetCurSel( hwndList, 0 );

   /* Load the script bitmap.
    */
   hbmpScript = LoadBitmap( hRscLib, MAKEINTRESOURCE(IDB_SCRIPT) );
   return( TRUE );
}

int FASTCALL Script_OnCompareItem( HWND hwnd, const COMPAREITEMSTRUCT FAR * lpCompareItem )
{
   LPSCRIPT lpScript2;
   LPSCRIPT lpScript1;

   lpScript2 = (LPSCRIPT)lpCompareItem->itemData2;
   if( lpCompareItem->itemID1 == -1 )
      return( ((LPSTR)lpCompareItem->itemData1)[ 0 ] - lpScript2->szDescription[ 0 ] );
   lpScript1 = (LPSCRIPT)lpCompareItem->itemData1;
   return( lstrcmpi( lpScript1->szDescription, lpScript2->szDescription ) );
}

int FASTCALL Script_OnCharToItem( HWND hwnd, UINT nKey, HWND hwndCtl, int iCaretPos )
{
   LPSCRIPT lpScript;
   int iStartCaretPos;
   int iCount;

   iCount = ListBox_GetCount( hwndCtl );
   if( ++iCaretPos == iCount )
      iCaretPos = 0;
   iStartCaretPos = iCaretPos;
   do {
      lpScript = (LPSCRIPT)ListBox_GetItemData( hwndCtl, iCaretPos );
      if( tolower( lpScript->szDescription[ 0 ] ) == tolower( (char)nKey ) )
         return( iCaretPos );
      if( ++iCaretPos == iCount )
         iCaretPos = 0;
      }
   while( iCaretPos != iStartCaretPos );
   return( -2 );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL Script_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_NEW:
         if( !Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_SCRIPTEDIT), ScriptEdit, 0L ) )
            break;
         if( lpFirstScript ) {
            EnableControl( hwnd, IDD_ADD, TRUE );
            EnableControl( hwnd, IDD_EDIT, TRUE );
            EnableControl( hwnd, IDD_REMOVE, TRUE );
            }
         fCancelToClose( hwnd, IDOK );
         break;

      case IDD_EDIT: {
         HWND hwndList;
         LPSCRIPT lpScript;
         int index;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         index = ListBox_GetCurSel( hwndList );
         lpScript = (LPSCRIPT)ListBox_GetItemData( hwndList, index );
         if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_SCRIPTEDIT), ScriptEdit, (LPARAM)lpScript ) )
            {
            RECT rc;

            ListBox_GetItemRect( hwndList, index, &rc );
            InvalidateRect( hwndList, &rc, FALSE );
            UpdateWindow( hwndList );
            fCancelToClose( hwnd, IDOK );
            }
         break;
         }

      case IDD_REMOVE: {
         HWND hwndList;
         LPSCRIPT lpScript;
         int index;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         index = ListBox_GetCurSel( hwndList );
         lpScript = (LPSCRIPT)ListBox_GetItemData( hwndList, index );
         wsprintf( lpTmpBuf, GS( IDS_STR150 ), lpScript->szDescription );
         if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION ) == IDYES )
            {
            if( lpScript->lpPrev == NULL )
               lpFirstScript = lpScript->lpNext;
            else
               lpScript->lpPrev->lpNext = lpScript->lpNext;
            if( lpScript->lpNext == NULL )
               lpLastScript = lpScript->lpPrev;
            else
               lpScript->lpNext->lpPrev = lpScript->lpPrev;
            WritePrivateProfileString( "Scripts", lpScript->szFileName, NULL, szScriptsIni );
            FreeMemory( &lpScript );
            if( ListBox_DeleteString( hwndList, index ) == index )
               --index;
            if( index == LB_ERR ) {
               EnableControl( hwnd, IDD_ADD, FALSE );
               EnableControl( hwnd, IDD_EDIT, FALSE );
               EnableControl( hwnd, IDD_REMOVE, FALSE );
               }
            else
               ListBox_SetCurSel( hwndList, index );
            fCancelToClose( hwnd, IDOK );
            }
         break;
         }

      case IDD_LIST:
         if( codeNotify != LBN_DBLCLK )
            break;

      case IDD_ADD: {
         HWND hwndList;
         LPSCRIPT lpScript;
         int index;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         index = ListBox_GetCurSel( hwndList );
         lpScript = (LPSCRIPT)ListBox_GetItemData( hwndList, index );
         if( CompileScript( hwnd, lpScript ) )
            {
            fCancelToClose( hwnd, IDOK );
            fMessageBox( hwnd, 0, GS(IDS_STR40), MB_OK|MB_ICONINFORMATION );
            }
         break;
         }

      case IDCANCEL:
      case IDOK:
         DeleteBitmap( hbmpScript );
         EndDialog( hwnd, 0 );
         break;
      }
}

/* This function handles the WM_MEASUREITEM message
 */
void FASTCALL Script_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   HFONT hOldFont;
   HDC hdc;

   hdc = GetDC( hwnd );
   hOldFont = SelectFont( hdc, hHelvB8Font );
   GetTextMetrics( hdc, &tm );
   lpMeasureItem->itemHeight = tm.tmHeight;
   SelectFont( hdc, hOldFont );
   ReleaseDC( hwnd, hdc );
}

/* This function handles the WM_DRAWITEM message
 */
void FASTCALL Script_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   if( lpDrawItem->itemID != -1 )
      if( lpDrawItem->itemAction == ODA_FOCUS )
         DrawFocusRect( lpDrawItem->hDC, (LPRECT)&lpDrawItem->rcItem );
      else {
         LPSCRIPT lpScript;
         HWND hwndList;
         COLORREF tmpT;
         COLORREF tmpB;
         COLORREF T;
         COLORREF B;
         HBRUSH hbr;
         HFONT hOldFont;
         RECT rc;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         lpScript = (LPSCRIPT)ListBox_GetItemData( hwndList, lpDrawItem->itemID );
         rc = lpDrawItem->rcItem;
         if( lpDrawItem->itemState & ODS_SELECTED ) {
            T = GetSysColor( COLOR_HIGHLIGHTTEXT );
            B = GetSysColor( COLOR_HIGHLIGHT );
            }
         else {
            T = GetSysColor( COLOR_WINDOWTEXT );
            B = GetSysColor( COLOR_WINDOW );
            }
         hbr = CreateSolidBrush( B );

         /* Erase the entire drawing area
          */
         FillRect( lpDrawItem->hDC, &rc, hbr );
         tmpT = SetTextColor( lpDrawItem->hDC, T );
         tmpB = SetBkColor( lpDrawItem->hDC, B );

         /* Draw the bitmap.
          */
         Amuser_DrawBitmap( lpDrawItem->hDC, rc.left, rc.top, 16, 16, hbmpScript, 0 );

         /* Draw the description.
          */
         hOldFont = SelectFont( lpDrawItem->hDC, hHelvB8Font );
         rc.left = 16 + 2;
         DrawText( lpDrawItem->hDC, lpScript->szDescription, -1, &rc, DT_NOPREFIX|DT_SINGLELINE|DT_VCENTER );

         /* Tidy up at the end
          */
         SelectFont( lpDrawItem->hDC, hOldFont );
         SetTextColor( lpDrawItem->hDC, tmpT );
         SetBkColor( lpDrawItem->hDC, tmpB );
         DeleteBrush( hbr );
         }
}

/* This function handles the script editor.
 */
BOOL EXPORT CALLBACK ScriptEdit( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, ScriptEdit_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the ScriptEdit
 * dialog.
 */
LRESULT FASTCALL ScriptEdit_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, ScriptEdit_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, ScriptEdit_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSCRIPTEDIT );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL ScriptEdit_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPSCRIPT lpScript;
   HNDFILE fh;
   HWND hwndEdit;

   /* Prefill the input fields if an existing script name
    * is supplied.
    */
   lpScript = (LPSCRIPT)lParam;
   if( lpScript )
      {
      SetDlgItemText( hwnd, IDD_FILENAME, lpScript->szFileName );
      SetDlgItemText( hwnd, IDD_DESCRIPTION, lpScript->szDescription );
      ShowWindow( GetDlgItem( hwnd, IDD_FILENAMEBTN ), SW_HIDE );
      EnableControl( hwnd, IDD_FILENAME, FALSE );
      }
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* Set the input field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_FILENAME ), MAX_FILENAME - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_DESCRIPTION ), MAX_DESCRIPTION - 1 );

   /* Set the focus.
    */
   if( Edit_GetTextLength( GetDlgItem( hwnd, IDD_FILENAME ) ) == 0 )
      SetFocus( GetDlgItem( hwnd, IDD_FILENAME ) );
   else
      SetFocus( GetDlgItem( hwnd, IDD_DESCRIPTION ) );

   /* Disable the OK button unless we have both a filename and a
    * description.
    */
   EnableControl( hwnd, IDOK,
         Edit_GetTextLength( GetDlgItem( hwnd, IDD_FILENAME ) ) > 0 &&
         Edit_GetTextLength( GetDlgItem( hwnd, IDD_DESCRIPTION ) ) );

   /* Open the script file and fill out the edit field.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
   if( lpScript != NULL )
      {
      wsprintf( lpPathBuf, "%s\\%s", pszScriptDir, lpScript->szFileName );
      if( HNDFILE_ERROR == ( fh = Amfile_Open( lpPathBuf, AOF_READ ) ) )
         {
         EnableWindow( hwndEdit, FALSE );
         Edit_SetText( hwndEdit, "\r\n  The specified script file could not be opened." );
         }
      else
         {
         LPSTR lpText;
         UINT cbSize;

         INITIALISE_PTR(lpText);

         /* Read the file into a local buffer then write it
          * to the edit field.
          */
         cbSize = (UINT)Amfile_GetFileSize( fh );
         if( fNewMemory( &lpText, cbSize + 1 ) )
            {
            if( (UINT)Amfile_Read( fh, lpText, cbSize ) != cbSize )
               DiskReadError( hwnd, lpScript->szFileName, FALSE, FALSE );
            else
               {
               lpText[ cbSize ] = '\0';
               Edit_SetText( hwndEdit, lpText );
               }
            FreeMemory( &lpText );
            }
         Amfile_Close( fh );
         }
      }

   /* Set the Browse picture button.
    */
   if( lpScript == NULL )
      PicButton_SetBitmap( GetDlgItem( hwnd, IDD_FILENAMEBTN ), hInst, IDB_FILEBUTTON );
   return( FALSE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL ScriptEdit_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_FILENAMEBTN: {
         static char strFilters[] = "Script Files (*.scr)\0*.SCR\0All Files (*.*)\0*.*\0\0";
         char szDir[ _MAX_PATH ];
         char sz[ _MAX_PATH ];
         OPENFILENAME ofn;
         HWND hwndEdit;

         hwndEdit = GetDlgItem( hwnd, IDD_FILENAME );
         Edit_GetText( hwndEdit, sz, sizeof( sz ) );
         ofn.lStructSize = sizeof( OPENFILENAME );
         ofn.hwndOwner = hwnd;
         ofn.hInstance = NULL;
         ofn.lpstrFilter = strFilters;
         ofn.lpstrCustomFilter = NULL;
         ofn.nMaxCustFilter = 0;
         ofn.nFilterIndex = 1;
         ofn.lpstrFile = sz;
         ofn.nMaxFile = sizeof( sz );
         ofn.lpstrFileTitle = NULL;
         ofn.nMaxFileTitle = 0;
         ofn.lpstrInitialDir = pszScriptDir;
         ofn.lpstrTitle = "Script File";
         ofn.Flags = OFN_NOCHANGEDIR|OFN_HIDEREADONLY|OFN_FILEMUSTEXIST;
         ofn.nFileOffset = 0;
         ofn.nFileExtension = 0;
         ofn.lpstrDefExt = NULL;
         ofn.lCustData = 0;
         ofn.lpfnHook = NULL;
         ofn.lpTemplateName = 0;
         if( GetOpenFileName( &ofn ) )
            {
            if( ofn.nFileOffset )
               {
               memcpy( szDir, sz, ofn.nFileOffset - 1 );
               szDir[ ofn.nFileOffset - 1 ] = '\0';
               }
            if( _strcmpi( szDir, pszScriptDir ) )
               Edit_SetText( hwndEdit, sz );
            else
               Edit_SetText( hwndEdit, sz + ofn.nFileOffset );
            SetFocus( GetDlgItem( hwnd, IDD_DESCRIPTION ) );
            }
         break;
         }

      case IDD_FILENAME:
      case IDD_DESCRIPTION:
         if( codeNotify == EN_UPDATE )
            EnableControl( hwnd, IDOK,
               Edit_GetTextLength( GetDlgItem( hwnd, IDD_FILENAME ) ) > 0 &&
               Edit_GetTextLength( GetDlgItem( hwnd, IDD_DESCRIPTION ) ) );
         break;

      case IDOK: {
         LPSCRIPT lpScript;
         HWND hwndEdit;
         BOOL fNew;

         /* If we're creating a new script, create a new script
          * record and link it.
          */
         fNew = FALSE;
         lpScript = (LPSCRIPT)GetWindowLong( hwnd, DWL_USER );
         if( lpScript == NULL )
            {
            if( !fNewMemory( &lpScript, sizeof( SCRIPT ) ) )
               {
               OutOfMemoryError( hwnd, FALSE, FALSE );
               break;
               }
            if( !lpFirstScript )
               lpFirstScript = lpScript;
            else
               lpLastScript->lpNext = lpScript;
            lpScript->lpNext = NULL;
            lpScript->lpPrev = lpLastScript;
            lpLastScript = lpScript;
            fNew = TRUE;
            }

         /* Get the script filename and description.
          */
         GetDlgItemText( hwnd, IDD_FILENAME, lpScript->szFileName, MAX_FILENAME );
         GetDlgItemText( hwnd, IDD_DESCRIPTION, lpScript->szDescription, MAX_DESCRIPTION );
         AnsiLower( lpScript->szFileName );

         /* Get the script itself if it has been edited.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         if( IsWindowEnabled( hwndEdit ) && EditMap_GetModified( hwndEdit ) )
            {
            LPSTR lpText;
            UINT cbSize;

            INITIALISE_PTR(lpText);

            cbSize = Edit_GetTextLength( hwndEdit );
            if( fNewMemory( &lpText, cbSize + 1 ) )
               {
               HNDFILE fh;

               /* Now save the edit text to the script file.
                */
               Edit_GetText( hwndEdit, lpText, cbSize + 1 );
               wsprintf( lpPathBuf, "%s\\%s", pszScriptDir, lpScript->szFileName );
               if( HNDFILE_ERROR == ( fh = Amfile_Create( lpPathBuf, 0 ) ) )
                  {
                  FileCreateError( hwnd, lpScript->szFileName, FALSE, FALSE );
                  break;
                  }
               if( Amfile_Write( fh, lpText, cbSize + 1 ) != cbSize + 1 )
                  {
                  DiskWriteError( hwnd, lpScript->szFileName, FALSE, FALSE );
                  break;
                  }
               Amfile_Close( fh );
               FreeMemory( &lpText );
               }
            }

         /* If this is a new script, add it to the parent
          * listbox.
          */
         if( fNew )
            {
            HWND hwndList;
            int index;

            hwndList = GetDlgItem( GetParent( hwnd ), IDD_LIST );
            index = ListBox_AddString( hwndList, lpScript );
            ListBox_SetCurSel( hwndList, index );
            }

         /* Add this script to the scripts index file.
          */
         WritePrivateProfileString( "Scripts", lpScript->szFileName, lpScript->szDescription, szScriptsIni );
         EndDialog( hwnd, TRUE );
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}
