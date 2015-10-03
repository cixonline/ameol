/* DIRECTRY.C - Implements the Directories page in the Preferences
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
#include "amlib.h"
#include "resource.h"
#include <string.h>

#define  THIS_FILE   __FILE__

extern BOOL fUseU3;   // !!SM!! 2.56.2053

static BOOL fDefDlgEx = FALSE;   /* DefDlg recursion flag trap */

BOOL FASTCALL Directories_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL Directories_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL Directories_OnNotify( HWND, int, LPNMHDR );

int FASTCALL LoadAllDatabases( void );
PCAT FASTCALL CreateCIXDatabase( void );

/* This function handles the dialog box messages passed to the Login
 * dialog.
 */
BOOL EXPORT CALLBACK Directories( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, Directories_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, Directories_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, Directories_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL Directories_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   /* Set the input field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_MESSAGES ), _MAX_DIR - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_SCRIPTS ), _MAX_DIR - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_DOWNLOADS ), _MAX_DIR - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_UPLOADS ), _MAX_DIR - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_ARCHIVES ), _MAX_DIR - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_ADDONS ), _MAX_DIR - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_SCRATCHPAD ), _MAX_DIR - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_MUGSHOTS ), _MAX_DIR - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_RESUMES ), _MAX_DIR - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_ATTACHMENTS ), _MAX_DIR - 1 );

   /* Set the current paths.
    */
   Edit_SetText( GetDlgItem( hwnd, IDD_ADDONS ), pszAddonsDir );
   Edit_SetText( GetDlgItem( hwnd, IDD_SCRATCHPAD ), pszScratchDir );
   Edit_SetText( GetDlgItem( hwnd, IDD_MESSAGES ), pszDataDir );
   Edit_SetText( GetDlgItem( hwnd, IDD_SCRIPTS ), pszScriptDir );
   Edit_SetText( GetDlgItem( hwnd, IDD_DOWNLOADS ), pszDownloadDir );
   Edit_SetText( GetDlgItem( hwnd, IDD_UPLOADS ), pszUploadDir );
   Edit_SetText( GetDlgItem( hwnd, IDD_ARCHIVES ), pszArchiveDir );
   Edit_SetText( GetDlgItem( hwnd, IDD_MUGSHOTS ), pszMugshotDir );
   Edit_SetText( GetDlgItem( hwnd, IDD_RESUMES ), pszResumesDir );
   Edit_SetText( GetDlgItem( hwnd, IDD_ATTACHMENTS ), pszAttachDir );

   /* Set the picture buttons.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_CHG_ADDONS ), hInst, IDB_FOLDERBUTTON );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_CHG_SCRATCHPAD ), hInst, IDB_FOLDERBUTTON );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_CHG_MESSAGES ), hInst, IDB_FOLDERBUTTON );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_CHG_SCRIPTS ), hInst, IDB_FOLDERBUTTON );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_CHG_UPLOADS ), hInst, IDB_FOLDERBUTTON );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_CHG_ARCHIVES ), hInst, IDB_FOLDERBUTTON );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_CHG_DOWNLOADS ), hInst, IDB_FOLDERBUTTON );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_CHG_RESUMES ), hInst, IDB_FOLDERBUTTON );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_CHG_MUGSHOTS ), hInst, IDB_FOLDERBUTTON );
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_CHG_ATTACHMENTS ), hInst, IDB_FOLDERBUTTON );

   if (fUseU3)
   {
      EnableWindow(GetDlgItem( hwnd, IDD_MESSAGES ), FALSE );
      EnableWindow(GetDlgItem( hwnd, IDD_SCRIPTS ), FALSE);
      EnableWindow(GetDlgItem( hwnd, IDD_DOWNLOADS ), FALSE);
      EnableWindow(GetDlgItem( hwnd, IDD_UPLOADS ), FALSE);
      EnableWindow(GetDlgItem( hwnd, IDD_ARCHIVES ), FALSE);
      EnableWindow(GetDlgItem( hwnd, IDD_ADDONS ), FALSE);
      EnableWindow(GetDlgItem( hwnd, IDD_SCRATCHPAD ), FALSE);
      EnableWindow(GetDlgItem( hwnd, IDD_MUGSHOTS ), FALSE);
      EnableWindow(GetDlgItem( hwnd, IDD_RESUMES ), FALSE);
      EnableWindow(GetDlgItem( hwnd, IDD_ATTACHMENTS ), FALSE);
   }

   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL Directories_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_RESUMES:
      case IDD_SCRATCHPAD:
      case IDD_MESSAGES:
      case IDD_SCRIPTS:
      case IDD_UPLOADS:
      case IDD_ARCHIVES:
      case IDD_MUGSHOTS:
      case IDD_ADDONS:
      case IDD_DOWNLOADS:
      case IDD_ATTACHMENTS:
         if( codeNotify == EN_CHANGE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_CHG_RESUMES:
      case IDD_CHG_SCRATCHPAD:
      case IDD_CHG_MESSAGES:
      case IDD_CHG_SCRIPTS:
      case IDD_CHG_UPLOADS:
      case IDD_CHG_ARCHIVES:
      case IDD_CHG_MUGSHOTS:
      case IDD_CHG_ADDONS:
      case IDD_CHG_ATTACHMENTS:
      case IDD_CHG_DOWNLOADS: {
         HWND hwndEditFocus;
         CHGDIR chgdir;
         int idFocus;
         char * np;

         strcpy( chgdir.szTitle, GS(IDS_STR370) );
         np = "";
         switch( id )
            {
            case IDD_CHG_RESUMES:      idFocus = IDD_RESUMES;     np = GS(IDS_STR989); break;
            case IDD_CHG_ADDONS:       idFocus = IDD_ADDONS;      np = GS(IDS_STR563); break;
            case IDD_CHG_SCRATCHPAD:   idFocus = IDD_SCRATCHPAD;  np = GS(IDS_STR676); break;
            case IDD_CHG_MESSAGES:     idFocus = IDD_MESSAGES;    np = GS(IDS_STR371); break;
            case IDD_CHG_SCRIPTS:      idFocus = IDD_SCRIPTS;     np = GS(IDS_STR372); break;
            case IDD_CHG_DOWNLOADS:    idFocus = IDD_DOWNLOADS;   np = GS(IDS_STR373); break;
            case IDD_CHG_UPLOADS:      idFocus = IDD_UPLOADS;     np = GS(IDS_STR374); break;
            case IDD_CHG_ARCHIVES:     idFocus = IDD_ARCHIVES;    np = GS(IDS_STR375); break;
            case IDD_CHG_MUGSHOTS:     idFocus = IDD_MUGSHOTS;    np = GS(IDS_STR993); break;
            case IDD_CHG_ATTACHMENTS:  idFocus = IDD_ATTACHMENTS; np = GS(IDS_STR1237); break;
            default:                   idFocus = 0; np = "";      ASSERT(FALSE);       break;
            }
         hwndEditFocus = GetDlgItem( hwnd, idFocus );
         Edit_GetText( hwndEditFocus, chgdir.szPath, sizeof(chgdir.szPath) );
         wsprintf( chgdir.szPrompt, GS(IDS_STR376), (LPSTR)np );
         chgdir.hwnd = GetParent( hwnd );
         if( Amuser_ChangeDirectory( &chgdir ) )
            Edit_SetText( hwndEditFocus, chgdir.szPath );
         SetFocus( hwndEditFocus );
         break;
         }
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL Directories_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPREF_DIRECTORIES );
         break;

      case PSN_SETACTIVE:
         nLastOptionsDialogPage = ODU_DIRECTORIES;
         break;

      case PSN_APPLY: {
         char szDir[ _MAX_DIR ];

         /* Validate the script and download directories.
          */
         Edit_GetText( GetDlgItem( hwnd, IDD_SCRIPTS ), szDir, _MAX_DIR );
         if( !ValidateChangedDir( hwnd, szDir ) )
            break;
         Edit_GetText( GetDlgItem( hwnd, IDD_RESUMES ), szDir, _MAX_DIR );
         if( !ValidateChangedDir( hwnd, szDir ) )
            break;
         Edit_GetText( GetDlgItem( hwnd, IDD_DOWNLOADS ), szDir, _MAX_DIR );
         if( !ValidateChangedDir( hwnd, szDir ) )
            break;
         Edit_GetText( GetDlgItem( hwnd, IDD_UPLOADS ), szDir, _MAX_DIR );
         if( !ValidateChangedDir( hwnd, szDir ) )
            break;
         Edit_GetText( GetDlgItem( hwnd, IDD_ARCHIVES ), szDir, _MAX_DIR );
         if( !ValidateChangedDir( hwnd, szDir ) )
            break;
         Edit_GetText( GetDlgItem( hwnd, IDD_ADDONS ), szDir, _MAX_DIR );
         if( !ValidateChangedDir( hwnd, szDir ) )
            break;
         Edit_GetText( GetDlgItem( hwnd, IDD_MUGSHOTS ), szDir, _MAX_DIR );
         if( !ValidateChangedDir( hwnd, szDir ) )
            break;
         Edit_GetText( GetDlgItem( hwnd, IDD_SCRATCHPAD ), szDir, _MAX_DIR );
         if( !ValidateChangedDir( hwnd, szDir ) )
            break;
         Edit_GetText( GetDlgItem( hwnd, IDD_ATTACHMENTS ), szDir, _MAX_DIR );
         if( !ValidateChangedDir( hwnd, szDir ) )
            break;

         /* Special case for changing DATA directory, for obvious reasons!
          */
         Edit_GetText( GetDlgItem( hwnd, IDD_MESSAGES ), szDir, _MAX_DIR );
         if( _strcmpi( szDir, pszDataDir ) )
            if( ( fMessageBox( hwnd, 0, GS(IDS_STR1253), MB_YESNO|MB_ICONQUESTION ) == IDYES ) )
            {
            /* First, ensure the new directory exists.
             */
            if( !ValidateChangedDir( hwnd, szDir ) )
               break;

            /* Data directory changed, so save the current directories
             * and close them all.
             */
            while( Amdb_CommitDatabase( TRUE ) != AMDBERR_NOERROR )
               {
               register int r;

               r = fMessageBox( hwnd, 0, GS(IDS_STR84), MB_RETRYCANCEL|MB_ICONEXCLAMATION );
               if( IDCANCEL == r )
                  break;
               }
//          Amdb_CloseDatabase();

            /* Switch to the new directory.
             */
            WriteDirectoryPath( APDIR_DATA, &pszDataDir, hwnd, IDD_MESSAGES, "Data" );
//          Amdb_SetDataDirectory( szDir );

            /* Finally, load the directory from there.
             */
//          if( 0 == LoadAllDatabases() )
//             CreateCIXDatabase();

            /* Make sure we have the cached handles.
             */
//          (void)GetCIXCategory();
            PostMessage( hwndFrame, WM_CLOSE, 0, 0L );

            }
            else
               Edit_SetText( GetDlgItem( hwnd, IDD_MESSAGES ), pszDataDir );


         /* The others get handled as usual.
          */
         WriteDirectoryPath( APDIR_ADDONS, &pszAddonsDir, hwnd, IDD_ADDONS, "Addons" );
         WriteDirectoryPath( APDIR_SCRATCHPADS, &pszScratchDir, hwnd, IDD_SCRATCHPAD, "Scratch" );
         WriteDirectoryPath( APDIR_SCRIPTS, &pszScriptDir, hwnd, IDD_SCRIPTS, "Script" );
         WriteDirectoryPath( APDIR_RESUMES, &pszResumesDir, hwnd, IDD_RESUMES, "Resumes" );
         WriteDirectoryPath( APDIR_DOWNLOADS, &pszDownloadDir, hwnd, IDD_DOWNLOADS, "Download" );
         WriteDirectoryPath( APDIR_UPLOADS, &pszUploadDir, hwnd, IDD_UPLOADS, "Upload" );
         WriteDirectoryPath( APDIR_ARCHIVES, &pszArchiveDir, hwnd, IDD_ARCHIVES, "Archive" );
         WriteDirectoryPath( APDIR_MUGSHOTS, &pszMugshotDir, hwnd, IDD_MUGSHOTS, "Mugshot" );
         WriteDirectoryPath( APDIR_ATTACHMENTS, &pszAttachDir, hwnd, IDD_ATTACHMENTS, "Attach" );

         /* If the mugshot index has changed, make sure
          * the target has an index.
          */
         EnsureMugshotIndex();

//       CreateResumesIndex( FALSE );

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}
