/* MAILDIR.C - Mail directory and commands.
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
#include <memory.h>
#include <string.h>
#include <ctype.h>
#include <io.h>
#include <sys\types.h>
#include <sys\stat.h>
#include "print.h"
#include "cix.h"
#include "ameol2.h"
#include "lbf.h"
#include "rules.h"

#define  THIS_FILE   __FILE__

extern BOOL fIsAmeol2Scratchpad;

static BOOL fDefDlgEx = FALSE;   /* DefDlg recursion flag trap */

static char szFileName[ LEN_CIXFILENAME+1 ];
static char szDirectory[ _MAX_DIR ];

BOOL EXPORT CALLBACK MailDownload( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL MailDownload_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL MailDownload_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL MailDownload_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT CALLBACK MailExport( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL MailExport_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL MailExport_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL MailExport_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL MailExport_UpdateOkButton( HWND );

BOOL EXPORT CALLBACK MailErase( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL MailErase_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL MailErase_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL MailErase_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT CALLBACK MailRename( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL MailRename_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL MailRename_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL MailRename_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT CALLBACK MailDir( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL MailDir_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL MailDir_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL MailDir_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL MailDir_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
void FASTCALL MailDir_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
LRESULT FASTCALL MailDir_OnNotify( HWND, int, LPNMHDR );
int FASTCALL MailDir_OnCompareItem( HWND, const COMPAREITEMSTRUCT FAR * );

BOOL FASTCALL DoMailUpload( MAILUPLOADOBJECT FAR * );
BOOL FASTCALL DoesMailDirFileExist( void );
void FASTCALL LoadDirFileName( HWND, LPSTR );
DWORD FASTCALL GetFileSizeFromMailDirectory( char * );
int FASTCALL MailDir_LoadListBox( HNDFILE, HWND );

LRESULT EXPORT CALLBACK MailDirListProc( HWND, UINT, WPARAM, LPARAM );

static int hdrColumns[ 3 ];               /* Header columns */
static BOOL fQuickUpdate = FALSE;         /* TRUE if we don't repaint bkgnd in mail dir */
static WNDPROC lpProcListCtl;             /* Mail directory list box window proc */
static int nCompare = 0;                  /* Index of column on which we're sorting */
static int nFlip = 1;                     /* Flag to invert direction of sort */

/* Uploads a file from the local hard disk to the users CIX mail directory.
 */
void FASTCALL CmdMailUpload( HWND hwnd )
{
   MAILUPLOADOBJECT mu;

   /* Initialise.
    */
   Amob_New( OBTYPE_MAILUPLOAD, &mu );
   if( DoMailUpload( &mu ) )
      if( Amob_Find( OBTYPE_MAILUPLOAD, &mu ) )
         {
         wsprintf( lpTmpBuf, GS(IDS_STR284), DRF(mu.recFileName) );
         fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
         }
      else
         Amob_Commit( NULL, OBTYPE_MAILUPLOAD, &mu );
   Amob_Delete( OBTYPE_MAILUPLOAD, &mu );
}

/* This function carries out the actual mail upload
 * action.
 */
BOOL FASTCALL DoMailUpload( MAILUPLOADOBJECT FAR * lpmul )
{
   static char strFilters[] = "All Files (*.*)\0*.*\0\0";
   BOOL fOk;

   /* Get the upload file name.
    */
   fOk = FALSE;
   strcpy( lpPathBuf, DRF(lpmul->recFileName) );
   if( CommonGetOpenFilename( hwndFrame, lpPathBuf, "Upload File", strFilters, "CachedMailUploadDir" ) )
      {
      /* Check for illegal characters in the filename and
       * reject if found.
       */
      if( CheckValidCixFilename( hwndFrame, lpPathBuf ) )
         {
         Amob_CreateRefString( &lpmul->recFileName, lpPathBuf );
         fOk = TRUE;
         }
      }
   return( fOk );
}

/* This function invokes the Mail Download command.
 */
void FASTCALL CmdMailDownload( HWND hwnd, char * pszFilename )
{
   MAILDOWNLOADOBJECT mdlo;
   char szText[ LEN_CIXFILENAME + 1 ];

   /* Initialise.
    */
   Amob_New( OBTYPE_MAILDOWNLOAD, &mdlo );
   if( *pszFilename == '\0' )
   {
      if( hwndTopic )
         {
         GetMarkedName( hwndTopic, szText, LEN_CIXFILENAME + 1, FALSE );
         if( *szText != '\0' )
            pszFilename = szText;
         }
   }

   if( NULL != pszFilename )
      Amob_CreateRefString( &mdlo.recCIXFileName, pszFilename );

   /* Always delete after exporting.
    */
   mdlo.wFlags = FDLF_DELETE;

   /* Fire up dialog to edit settings.
    */
   if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_MAILDOWNLOAD), MailDownload, (LPARAM)(LPSTR)&mdlo ) )
      {
      /* If this file is already marked for downloading,
       * warn. Otherwise place the new download request
       * in the out-basket.
       */
      if( Amob_Find( OBTYPE_MAILDOWNLOAD, &mdlo ) )
         {
         wsprintf( lpTmpBuf, GS(IDS_STR424), DRF(mdlo.recCIXFileName) );
         fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
         }
      else
         Amob_Commit( NULL, OBTYPE_MAILDOWNLOAD, &mdlo );
      }
   Amob_Delete( OBTYPE_MAILDOWNLOAD, &mdlo );
}

/* Downloads a file from the users mail directory.
 */
BOOL EXPORT CALLBACK MailDownload( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, MailDownload_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the MailDownload
 * dialog.
 */
LRESULT FASTCALL MailDownload_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, MailDownload_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, MailDownload_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsMAILDOWNLOAD );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL MailDownload_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   MAILDOWNLOADOBJECT FAR * lpmdlo;

   /* Dereference the MAILDOWNLOADOBJECT
    * structure.
    */
   lpmdlo = (MAILDOWNLOADOBJECT FAR *)lParam;
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* Fill in the filename field.
    */
   if( !*DRF(lpmdlo->recCIXFileName) && DoesMailDirFileExist() )
      ShowWindow( GetDlgItem( hwnd, IDD_FILENAMEBTN ), SW_SHOW );
   else
      {
      Edit_SetText( GetDlgItem( hwnd, IDD_FILENAME ), DRF(lpmdlo->recCIXFileName) );

      /* Hide filename button.
       */
      SetFocus( GetDlgItem( hwnd, IDD_FILENAME ) );
      ShowWindow( GetDlgItem( hwnd, IDD_FILENAMEBTN ), SW_HIDE );
      EnableControl( hwnd, IDD_FILENAMEBTN, FALSE );
      }

   /* Fill in the directory field.
    */
   strcpy( szDirectory, DRF(lpmdlo->recDirectory) );
   if( !*DRF(lpmdlo->recDirectory) )
      {
      strcpy( szFileName, DRF(lpmdlo->recCIXFileName) );
      GetAssociatedDir( NULL, szDirectory, szFileName );
      }
   Edit_SetText( GetDlgItem( hwnd, IDD_DIRECTORY ), szDirectory );

   /* Set the input field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_FILENAME ), LEN_CIXFILENAME );
   Edit_LimitText( GetDlgItem( hwnd, IDD_DIRECTORY ), _MAX_DIR - 1 );
   SetFocus( GetDlgItem( hwnd, IDD_FILENAME ) );

   /* Set the delete option.
    */
   CheckDlgButton( hwnd, IDD_DELETE, (lpmdlo->wFlags & FDLF_DELETE) );

   /* Set the Browse picture button.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_PICKER ), hInst, IDB_FOLDERBUTTON );

   /* Set the OK button.
    */
   EnableControl( hwnd, IDOK, Edit_GetTextLength( GetDlgItem( hwnd, IDD_FILENAME ) ) && 
                  Edit_GetTextLength( GetDlgItem( hwnd, IDD_DIRECTORY ) ) );
   return( FALSE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL MailDownload_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_FILENAMEBTN: {
         char sz[ LEN_CIXFILENAME+1 ];

         if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_MAILDIR), MailDir, (LPARAM)(LPSTR)sz ) )
            {
            Edit_SetText( GetDlgItem( hwnd, IDD_FILENAME ), sz );
            GetAssociatedDir( NULL, szDirectory, szFileName );
            Edit_SetText( GetDlgItem( hwnd, IDD_DIRECTORY ), szDirectory );
            SetFocus( GetDlgItem( hwnd, IDD_FILENAME ) );
            goto T1;
            }
         break;
         }

      case IDD_BROWSE: {
         CHGDIR chgdir;
         HWND hwndEdit;

         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_DIRECTORY ) );
         Edit_GetText( hwndEdit, chgdir.szPath, 144 );
         chgdir.hwnd = hwnd;
         strcpy( chgdir.szTitle, GS(IDS_STR539) );
         strcpy( chgdir.szPrompt, GS(IDS_STR540) );
         if( Amuser_ChangeDirectory( &chgdir ) )
            Edit_SetText( hwndEdit, chgdir.szPath );
         break;
         }

      case IDD_FILENAME:
         if( codeNotify == EN_CHANGE )
T1:         EnableControl( hwnd, IDOK, Edit_GetTextLength( GetDlgItem( hwnd, IDD_FILENAME ) ) > 0 );
         break;

      case IDOK: {
         MAILDOWNLOADOBJECT FAR * lpmdlo;
         int length;

         /* Get the input fields.
          */
         Edit_GetText( GetDlgItem( hwnd, IDD_FILENAME ), szFileName, sizeof(szFileName) );
         Edit_GetText( GetDlgItem( hwnd, IDD_DIRECTORY ), szDirectory, sizeof(szDirectory) );

         /* Remove any trailing separator from the end of the
          * directory path.
          */
         length = strlen( szDirectory );
         if( szDirectory[ length - 1 ] == '\\' )
            szDirectory[ length - 1 ] = '\0';

         /* Make sure the directory exists.
          */
         if( !ValidateChangedDir( hwnd, szDirectory ) )
            break;

         /* Get the pointer to the MAILDOWNLOADOBJECT structure and
          * fill in the input fields.
          */
         lpmdlo = (MAILDOWNLOADOBJECT FAR *)GetWindowLong( hwnd, DWL_USER );
         Amob_CreateRefString( &lpmdlo->recCIXFileName, szFileName );
         Amob_CreateRefString( &lpmdlo->recDirectory, szDirectory );

         /* Set the FDLF flag if the Delete after download button is
          * checked.
          */
         if( IsDlgButtonChecked( hwnd, IDD_DELETE ) )
            lpmdlo->wFlags |= FDLF_DELETE;
         else
            lpmdlo->wFlags &= ~FDLF_DELETE;

         /* Convert the CIX filename to a local filename.
          */
         if( !Ameol2_MakeLocalFileName( hwnd, szFileName, lpTmpBuf, TRUE ) )
            break;
         Amob_CreateRefString( &lpmdlo->recFileName, lpTmpBuf );

         /* Determine whether the file already exists and, if so,
          * offer to overwrite it.
          */
         wsprintf( lpPathBuf, "%s\\%s", szDirectory, lpTmpBuf );
         if( _access( lpPathBuf, 0 ) == 0 )
            {
            if( fMessageBox( hwnd, 0, GS(IDS_STR674), MB_YESNO|MB_ICONQUESTION ) == IDNO )
               break;
            lpmdlo->wFlags |= FDLF_OVERWRITE;
            }
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* This function invokes the Mail Export command.
 */
void FASTCALL CmdMailExport( HWND hwnd, char * pszFilename )
{
   MAILEXPORTOBJECT meo;
   CURMSG curmsg;

   /* Initialise.
    */
   Amob_New( OBTYPE_MAILEXPORT, &meo );
   if( NULL != pszFilename )
      Amob_CreateRefString( &meo.recFileName, pszFilename );

   /* Set the default export destination depending on
    * the active folder.
    */
   Ameol2_GetCurrentTopic( &curmsg );
   if( NULL == curmsg.pcat )
      curmsg.pcat = GetCIXCategory();
   if( NULL == curmsg.pcl )
      curmsg.pcl = Amdb_GetFirstFolder( curmsg.pcat );
   if( NULL == curmsg.ptl && NULL != curmsg.pcl )
      curmsg.ptl = Amdb_GetFirstTopic( curmsg.pcl );
   if( NULL != curmsg.ptl )
      {
      Amob_CreateRefString( &meo.recConfName, (LPSTR)Amdb_GetFolderName( curmsg.pcl ) );
      Amob_CreateRefString( &meo.recTopicName, (LPSTR)Amdb_GetTopicName( curmsg.ptl ) );
      }

   /* Always delete after exporting.
    */
   meo.wFlags = BINF_DELETE;

   /* Fire up dialog to edit settings.
    */
   if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_MAILEXPORT), MailExport, (LPARAM)(LPSTR)&meo ) )
      {
      /* If this file is already marked for exporting
       * warn. Otherwise place the new export request
       * in the out-basket.
       */
      if( Amob_Find( OBTYPE_MAILEXPORT, &meo ) )
         {
         wsprintf( lpTmpBuf, GS(IDS_STR293), DRF(meo.recFileName), DRF(meo.recConfName), DRF(meo.recTopicName) );
         fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
         }
      else
         Amob_Commit( NULL, OBTYPE_MAILEXPORT, &meo );
      }
   Amob_Delete( OBTYPE_MAILEXPORT, &meo );
}

/* This function handles the Mail Export dialog.
 */
BOOL EXPORT CALLBACK MailExport( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, MailExport_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the MailExport
 * dialog.
 */
LRESULT FASTCALL MailExport_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, MailExport_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, MailExport_OnCommand );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, FolderCombo_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_DRAWITEM, FolderCombo_OnDrawItem );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsMAILEXPORT );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL MailExport_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   MAILEXPORTOBJECT FAR * lpmeo;
   HWND hwndCombo;
   int index;
   PCL pcl;
   PTL ptl;

   /* Dereference the MAILEXPORTOBJECT
    * structure.
    */
   lpmeo = (MAILEXPORTOBJECT FAR *)lParam;
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* Fill in the filename field.
    */
   if( !*DRF(lpmeo->recFileName) && DoesMailDirFileExist() )
      {
      ShowWindow( GetDlgItem( hwnd, IDD_FILENAMEBTN ), SW_SHOW );
      ShowWindow( GetDlgItem( hwnd, IDD_PAD2 ), SW_HIDE );
      EnableControl( hwnd, IDD_FILENAMEBTN, TRUE );
      }
   else
      {
      Edit_SetText( GetDlgItem( hwnd, IDD_FILENAME ), DRF(lpmeo->recFileName) );

      /* Hide filename button.
       */
      SetFocus( GetDlgItem( hwnd, IDD_FILENAME ) );
      ShowWindow( GetDlgItem( hwnd, IDD_PAD2 ), SW_SHOW );
      ShowWindow( GetDlgItem( hwnd, IDD_FILENAMEBTN ), SW_HIDE );
      EnableControl( hwnd, IDD_FILENAMEBTN, FALSE );
      }

   /* Fill in the list of folders.
    */
   FillListWithTopics( hwnd, IDD_FOLDERS, FTYPE_CIX|FTYPE_TOPICS );
   VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_FOLDERS ) );
   index = CB_ERR;
   if( NULL != ( pcl = Amdb_OpenFolder( NULL, DRF(lpmeo->recConfName) ) ) )
      if( NULL != ( ptl = Amdb_OpenTopic( pcl, DRF(lpmeo->recTopicName) ) ) )
         if( ( index = RealComboBox_FindItemData( hwndCombo, -1, (DWORD)ptl ) ) != CB_ERR )
            ComboBox_SetCurSel( hwndCombo, index );

   /* Set the delete option.
    */
   CheckDlgButton( hwnd, IDD_DELETE, (lpmeo->wFlags & BINF_DELETE) );

   /* Set the input field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_FILENAME ), LEN_CIXFILENAME );
   MailExport_UpdateOkButton( hwnd );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL MailExport_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_FILENAMEBTN: {
         char sz[ LEN_CIXFILENAME+1 ];

         if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE( IDDLG_MAILDIR ), MailDir, (LPARAM)(LPSTR)sz ) )
            {
            Edit_SetText( GetDlgItem( hwnd, IDD_FILENAME ), sz );
            SetFocus( GetDlgItem( hwnd, IDD_FILENAME ) );
            EnableControl( hwnd, IDOK, TRUE );
            }
         break;
         }

      case IDD_FOLDERS:
         if( codeNotify == CBN_SELCHANGE )
            MailExport_UpdateOkButton( hwnd );
         break;

      case IDD_FILENAME:
         if( codeNotify == EN_CHANGE )
            MailExport_UpdateOkButton( hwnd );
         break;

      case IDOK: {
         char szFileName[ LEN_CIXFILENAME+1 ];
         MAILEXPORTOBJECT FAR * lpmeo;
         TOPICINFO topicinfo;
         HWND hwndCombo;
         LPVOID pFolder;
         int index;
         PCL pcl;

         /* Get the new field values.
          */
         Edit_GetText( GetDlgItem( hwnd, IDD_FILENAME ), szFileName, LEN_CIXFILENAME+1 );

         /* Get the pointer to the MAILEXPORTOBJECT structure and
          * fill in the input fields.
          */
         lpmeo = (MAILEXPORTOBJECT FAR *)GetWindowLong( hwnd, DWL_USER );
         Amob_CreateRefString( &lpmeo->recFileName, szFileName );

         /* Get the selected topic.
          */
         VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_FOLDERS ) );
         index = ComboBox_GetCurSel( hwndCombo );
         ASSERT( index != CB_ERR );
         pFolder = (LPVOID)ComboBox_GetItemData( hwndCombo, index );

         /* Must be a topic.
          */
         if( !Amdb_IsTopicPtr( (PTL)pFolder ) )
            break;
         pcl = Amdb_FolderFromTopic( (PTL)pFolder );
         Amob_CreateRefString( &lpmeo->recConfName, (LPSTR)Amdb_GetFolderName( pcl ) );
         Amob_CreateRefString( &lpmeo->recTopicName, (LPSTR)Amdb_GetTopicName( pFolder ) );

         /* If folder doesn't have an flist, warn the
          * user.
          */
         Amdb_GetTopicInfo( (PTL)pFolder, &topicinfo );
         if( !( topicinfo.dwFlags & TF_HASFILES ) )
            if( fMessageBox( hwnd, 0, GS(IDS_STR425), MB_YESNO|MB_ICONQUESTION ) == IDNO )
               {
               SetFocus( hwndCombo );
               break;
               }

         /* Set the delete flag if the user enabled delete
          * after exporting.
          */
         if( IsDlgButtonChecked( hwnd, IDD_DELETE ) )
            lpmeo->wFlags |= BINF_DELETE;
         else
            lpmeo->wFlags &= ~BINF_DELETE;
         EndDialog( hwnd, TRUE );
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}

/* This function enables or disables the Ok button depending
 * on the current dialog selections.
 */
void FASTCALL MailExport_UpdateOkButton( HWND hwnd )
{
   HWND hwndCombo;
   HWND hwndEdit;
   BOOL fIsTopic;
   int index;

   fIsTopic = FALSE;
   VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_FOLDERS ) );
   index = ComboBox_GetCurSel( hwndCombo );
   if( CB_ERR != index )
      {
      LPVOID pFolder;

      /* Set fIsTopic TRUE if selected item is a topic.
       */
      pFolder = (LPVOID)ComboBox_GetItemData( hwndCombo, index );
      fIsTopic = Amdb_IsTopicPtr( (PTL)pFolder );
      }
   VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_FILENAME ) );
   EnableControl( hwnd, IDOK, fIsTopic && Edit_GetTextLength( hwndEdit ) > 0 );
}

/* This function invokes the Mail Directory command.
 */
void FASTCALL CmdMailDirectory( HWND hwnd )
{
   Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_MAILDIR), MailDir, 0L );
}

/* This function handles the Mail Directory command.
 */
BOOL EXPORT CALLBACK MailDir( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, MailDir_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the MailDir
 * dialog.
 */
LRESULT FASTCALL MailDir_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, MailDir_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, MailDir_OnCommand );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, MailDir_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_DRAWITEM, MailDir_OnDrawItem );
      HANDLE_MSG( hwnd, WM_NOTIFY, MailDir_OnNotify );
      HANDLE_MSG( hwnd, WM_COMPAREITEM, MailDir_OnCompareItem );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsMAILDIR );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_ERASEBKGND message.
 */
LRESULT EXPORT CALLBACK MailDirListProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      case WM_ERASEBKGND:
         if( fQuickUpdate )
            return( TRUE );
         break;
      }
   return( CallWindowProc( lpProcListCtl, hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL MailDir_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   static int aArray[ 3 ] = { 120, 100, 100 };
   char szUserDir[ _MAX_PATH ];
   HD_ITEM hdi;
   HWND hwndHdr;
   HWND hwndList;
   int count;
   HNDFILE fh;
   AM_DATE date;
   AM_TIME time;
   WORD uDate;
   WORD uTime;

   /* Look for the mail directory file. If not found, offer to
    * retrieve it from CIX.
    */
   Amuser_GetActiveUserDirectory( szUserDir, _MAX_PATH );
   wsprintf( lpPathBuf, "%s\\%s", szUserDir, szMailDirFile );
   if( ( fh = Amfile_Open( lpPathBuf, AOF_READ ) ) == HNDFILE_ERROR )
      {
      if( Amob_Find( OBTYPE_MAILLIST, NULL ) )
         fMessageBox( hwnd, 0, GS(IDS_STR285), MB_OK|MB_ICONEXCLAMATION );
      else
         {
         if( fMessageBox( hwnd, 0, GS(IDS_STR286), MB_YESNO|MB_ICONQUESTION ) == IDYES )
            Amob_Commit( NULL, OBTYPE_MAILLIST, NULL );
         }
      if( lParam )
         ((LPSTR)lParam)[ 0 ] = '\0';
      EndDialog( hwnd, TRUE );
      return( FALSE );
      }

   /* lParam can point to a buffer for the selected filename.
    */
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* Read the current header settings.
    */
   Amuser_GetPPArray( szCIX, "MailDirColumns", aArray, 3 );

   /* Fill in the header fields.
    */
   hwndHdr = GetDlgItem( hwnd, IDD_LIST );
   hdi.mask = HDI_TEXT|HDI_WIDTH|HDI_FORMAT;
   hdi.cxy = hdrColumns[ 0 ] = aArray[ 0 ];
   hdi.pszText = "Filename";
   hdi.fmt = HDF_STRING|HDF_LEFT;
   Header_InsertItem( hwndHdr, 0, &hdi );
   
   hdi.cxy = hdrColumns[ 1 ] = aArray[ 1 ];
   hdi.pszText = "Size";
   hdi.fmt = HDF_STRING|HDF_LEFT;
   Header_InsertItem( hwndHdr, 1, &hdi );

   hdi.cxy = hdrColumns[ 2 ] = aArray[ 2 ];
   hdi.pszText = "Date";
   hdi.fmt = HDF_STRING|HDF_LEFT;
   Header_InsertItem( hwndHdr, 2, &hdi );
   SendMessage( hwndHdr, WM_SETTEXT, 2 | HBT_SPRING, 0L );
   SetWindowFont( hwndHdr, hHelv8Font, FALSE );

   /* Otherwise read the directory and parse it. The first three lines contain
    * header information which can be skipped.
    */
   count = MailDir_LoadListBox( fh, hwnd );

   /* Subclass the list control.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   lpProcListCtl = SubclassWindow( GetDlgItem( hwndList, 0x3000 ), MailDirListProc );

   /* Nothing in list box - disable controls
    */
   if( !count )
      {
      EnableControl( hwnd, IDD_DOWNLOAD, FALSE );
      EnableControl( hwnd, IDD_DELETE, FALSE );
      EnableControl( hwnd, IDD_BINMAIL, FALSE );
      EnableControl( hwnd, IDD_EXPORT, FALSE );
      EnableControl( hwnd, IDD_RENAME, FALSE );
      if( lParam )
         EnableControl( hwnd, IDOK, FALSE );
      }

   /* If we were called from the Filename button on a Download or Binmail dialog,
    * disable the four function buttons.
    */
   if( lParam )
      {
      ShowWindow( GetDlgItem( hwnd, IDD_DOWNLOAD ), SW_HIDE );
      ShowWindow( GetDlgItem( hwnd, IDD_DELETE ), SW_HIDE );
      ShowWindow( GetDlgItem( hwnd, IDD_BINMAIL ), SW_HIDE );
      ShowWindow( GetDlgItem( hwnd, IDD_EXPORT ), SW_HIDE );
      ShowWindow( GetDlgItem( hwnd, IDD_RENAME ), SW_HIDE );
      }
   else {
      SetWindowText( GetDlgItem( hwnd, IDOK ), GS(IDS_STR298) );
      ShowWindow( GetDlgItem( hwnd, IDCANCEL ), SW_HIDE );
      }

   /* Add the last downloaded text item
    */
   BEGIN_PATH_BUF;
   fh = Amfile_Open( lpPathBuf, AOF_READ );
   END_PATH_BUF;
// Amfile_GetFileTime( fh, &uDate, &uTime );
   Amfile_GetFileLocalTime( fh, &uDate, &uTime );
   Amdate_UnpackDate( uDate, &date );
   Amdate_UnpackTime( uTime, &time );
   wsprintf( lpTmpBuf, GS(IDS_STR336), Amdate_FriendlyDate( NULL, uDate, uTime ) );
   SetDlgItemText( hwnd, IDD_LASTUPDATED, lpTmpBuf );
   Amfile_Close( fh );

   return( TRUE );
}

/* This function fills the mail directory listbox.
 */
int FASTCALL MailDir_LoadListBox( HNDFILE fh, HWND hwnd )
{
   HCURSOR hOldCursor;
   HWND hwndList;
   LPLBF hBuffer;
   int wState;
   int cToSkip;
   int count;
   char szSrc[ 80 ];
   char szDst[ 80 ];

   hOldCursor = SetCursor( hWaitCursor );
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   ListBox_ResetContent( hwndList );
   hBuffer = Amlbf_Open( fh, AOF_READ );
   wState = 0;
   cToSkip = 3;
   count = 0;
   while( Amlbf_Read( hBuffer, szSrc, sizeof( szSrc ) - 1, NULL, NULL, &fIsAmeol2Scratchpad ) )
      switch( wState )
         {
         case 0:
            if( cToSkip-- )
               continue;
            wState = 1;

         case 1: {
            LPSTR lpStr;
            char * pSrc;
            char * pDst;

            INITIALISE_PTR(lpStr);

            pSrc = szSrc;
            pDst = szDst;
            while( *pSrc == ' ' )
               ++pSrc;
            while( *pSrc && *pSrc != ' ' )
               *pDst++ = *pSrc++;
            *pDst++ = '\t';
            while( *pSrc == ' ' )
               ++pSrc;
            while( *pSrc && *pSrc != ' ' )
               *pDst++ = *pSrc++;
            *pDst++ = '\t';
            while( *pSrc == ' ' )
               ++pSrc;
            while( *pSrc )
               *pDst++ = *pSrc++;
            *pDst = '\0';
            if( fNewMemory( &lpStr, strlen( szDst ) + 1 ) )
               {
               strcpy( lpStr, szDst );
               ListBox_AddString( hwndList, lpStr );
               ++count;
               }
            break;
            }
         }
   Amlbf_Close( hBuffer );
   SetCursor( hOldCursor );
   ListBox_SetCurSel( hwndList, 0 );
   return( count );
}

/* This function handles the WM_MEASUREITEM message,
 */
void FASTCALL MailDir_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   HFONT hFont;
   HDC hdc;

   hdc = GetDC( hwnd );
   hFont = SelectFont( hdc, hHelvB8Font );
   GetTextMetrics( hdc, &tm );
   SelectFont( hdc, hFont );
   ReleaseDC( hwnd, hdc );
   lpMeasureItem->itemHeight = max( tm.tmHeight + tm.tmExternalLeading, 16 );
}

/* This function handles the WM_DRAWITEM message,
 */
void FASTCALL MailDir_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   if( lpDrawItem->itemID != -1 )
      {
      char sz[ 80 ];
      char * lpStr;
      RECT rc;

      /* Prime the pumps, man the ballasts!
       */
      lpStr = (LPSTR)ListBox_GetItemData( lpDrawItem->hwndItem, lpDrawItem->itemID );
      strcpy( sz, lpStr );
      rc = lpDrawItem->rcItem;

      /* Dinna waste time with those ODA_FOCUS things, laddie!
       */
      if( lpDrawItem->itemAction == ODA_DRAWENTIRE || lpDrawItem->itemAction == ODA_SELECT )
         {
         COLORREF tmpT;
         COLORREF tmpB;
         COLORREF T;
         COLORREF B;
         HFONT hOldFont;
         HBRUSH hbr;
         char * npFileName;
         char * npSize;
         char * npDate;

         /* Cast orf the anchors, and we're aweigh(!)
          */
         if( ( lpDrawItem->itemState & ODS_SELECTED ) ) {
            T = GetSysColor( COLOR_HIGHLIGHTTEXT );
            B = GetSysColor( COLOR_HIGHLIGHT );
            }
         else {
            T = GetSysColor( COLOR_WINDOWTEXT );
            B = GetSysColor( COLOR_WINDOW );
            }
         hbr = CreateSolidBrush( B );
         tmpT = SetTextColor( lpDrawItem->hDC, T );
         tmpB = SetBkColor( lpDrawItem->hDC, B );
         hOldFont = SelectFont( lpDrawItem->hDC, hHelvB8Font );

         /* Break off the file description components
          */
         for( npFileName = npSize = sz; *npSize != '\t'; ++npSize );
         *npSize++ = '\0';
         for( npDate = npSize; *npDate != '\t'; ++npDate );
         *npDate++ = '\0';

         /* Now display each and every one in the alloted fields.
          */
         rc.left = 0;
         rc.right = rc.left + hdrColumns[ 0 ];
         ExtTextOut( lpDrawItem->hDC, 2, rc.top + 1, ETO_CLIPPED|ETO_OPAQUE, &rc, npFileName, strlen(npFileName), NULL );
         SelectFont( lpDrawItem->hDC, hHelv8Font );

         rc.left = rc.right;
         rc.right = rc.left + hdrColumns[ 1 ];
         ExtTextOut( lpDrawItem->hDC, rc.left + 2, rc.top + 1, ETO_CLIPPED|ETO_OPAQUE, &rc, npSize, strlen(npSize), NULL );

         rc.left = rc.right;
         rc.right = rc.left + hdrColumns[ 2 ];
         ExtTextOut( lpDrawItem->hDC, rc.left + 2, rc.top + 1, ETO_CLIPPED|ETO_OPAQUE, &rc, npDate, strlen(npDate), NULL );

         /* Swab the foredeck before dis-embarkation. Leave a clean ship
          */
         SetTextColor( lpDrawItem->hDC, tmpT );
         SetBkColor( lpDrawItem->hDC, tmpB );
         SelectFont( lpDrawItem->hDC, hOldFont );
         DeleteBrush( hbr );
         }
      }
}

/* This function handles the WM_COMPAREITEM message
 */
int FASTCALL MailDir_OnCompareItem( HWND hwnd, const COMPAREITEMSTRUCT FAR * lpCompareItem )
{
   LPSTR lpStr1;
   LPSTR lpStr2;
   int nSort;

   lpStr1 = (LPSTR)lpCompareItem->itemData1;
   lpStr2 = (LPSTR)lpCompareItem->itemData2;
   nSort = 0;
   switch( nCompare )
      {
      case 0:
         /* Sort by filename.
          */
         nSort = _strcmpi( lpStr1, lpStr2 );
         break;

      case 2: {
         AM_DATE date;
         AM_TIME time;
         WORD wDate1, wDate2;
         WORD wTime1, wTime2;

         /* Sort by file date and time
          */
         lpStr1 = strchr( lpStr1, '\t' ) + 1;
         lpStr2 = strchr( lpStr2, '\t' ) + 1;
         lpStr1 = strchr( lpStr1, '\t' ) + 1;
         lpStr2 = strchr( lpStr2, '\t' ) + 1;
         ParseCIXTime( lpStr1, &date, &time );
         wDate1 = Amdate_PackDate( &date );
         wTime1 = Amdate_PackTime( &time );
         ParseCIXTime( lpStr2, &date, &time );
         wDate2 = Amdate_PackDate( &date );
         wTime2 = Amdate_PackTime( &time );
         if( wDate1 != wDate2 )
            nSort = ( wDate2 > wDate1 ) ? 1 : -1;
         else
            nSort = ( wTime2 > wTime1 ) ? 1 : -1;
         break;
         }

      case 1:
         /* Sort by file size
          */
         lpStr1 = strchr( lpStr1, '\t' ) + 1;
         lpStr2 = strchr( lpStr2, '\t' ) + 1;
         nSort = ( atoi( lpStr2 ) > atoi( lpStr1 ) ) ? 1 : -1;
         break;
      }
   return( nSort * nFlip );
}

/* This function handles notification messages from the
 * treeview control.
 */
LRESULT FASTCALL MailDir_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
{
   switch( lpNmHdr->code )
      {
      case HDN_ITEMCLICK: {
         HD_NOTIFY FAR * lphdn;
         HNDFILE fh;

         /* Save the new sort column.
          */
         lphdn = (HD_NOTIFY FAR *)lpNmHdr;
         nFlip *= -1;
         nCompare = lphdn->iItem;

         /* Refill the listbox.
          */
         Amuser_GetActiveUserDirectory( lpPathBuf, _MAX_PATH );
         strcat( lpPathBuf, "\\" );
         strcat( lpPathBuf, szMailDirFile );
         if( ( fh = Amfile_Open( lpPathBuf, AOF_READ ) ) != HNDFILE_ERROR )
            MailDir_LoadListBox( fh, hwnd );
         return( TRUE );
         }

      case HDN_BEGINTRACK:
         return( TRUE );

      case HDN_TRACK:
      case HDN_ENDTRACK: {
         register int c;
         HWND hwndList;

         /* Read the new column settings into the
          * global hdrColumns array.
          */
         for( c = 0; c < 3; ++c )
            {
            HD_ITEM hdi;

            SendMessage( lpNmHdr->hwndFrom, HDM_GETITEM, c, (LPARAM)(LPSTR)&hdi );
            hdrColumns[ c ] = hdi.cxy;
            }
         if( HDN_ENDTRACK == lpNmHdr->code )
            Amuser_WritePPArray( szCIX, "MailDirColumns", hdrColumns, 3 );

         /* Update the listbox
          */
         fQuickUpdate = TRUE;
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         hwndList = GetDlgItem( hwndList, 0x3000 );
         InvalidateRect( hwndList, NULL, FALSE );
         UpdateWindow( hwndList );
         fQuickUpdate = FALSE;
         return( TRUE );
         }
      }
   return( FALSE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL MailDir_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   char sz[ LEN_CIXFILENAME+1 ];
   LPSTR lpBuf;

   switch( id )
      {
      case IDD_UPDATE: {
         if( Amob_Find( OBTYPE_MAILLIST, NULL ) )
            fMessageBox( hwnd, 0, GS(IDS_STR287), MB_OK|MB_ICONEXCLAMATION );
         else
            if( fMessageBox( hwnd, 0, GS(IDS_STR288), MB_YESNO|MB_ICONQUESTION ) == IDYES )
               Amob_Commit( NULL, OBTYPE_MAILLIST, NULL );
         break;
         }

      case IDD_EXPORT:
         LoadDirFileName( hwnd, sz );
         CmdMailExport( hwnd, sz );
         break;

      case IDD_RENAME:
         LoadDirFileName( hwnd, sz );
         CmdMailRename( hwnd, sz );
         break;

      case IDD_DOWNLOAD:
         LoadDirFileName( hwnd, sz );
         CmdMailDownload( hwnd, sz );
         break;

      case IDD_DELETE:
         LoadDirFileName( hwnd, sz );
         CmdMailErase( hwnd, sz );
         break;

      case IDD_BINMAIL:
         LoadDirFileName( hwnd, sz );
         CmdBinmail( hwnd, sz );
         break;

      case IDD_LIST:
         if( codeNotify != LBN_DBLCLK )
            break;
         if( NULL != ( lpBuf = (LPSTR)GetWindowLong( hwnd, DWL_USER ) ) )
            {
            LoadDirFileName( hwnd, lpBuf );
            EndDialog( hwnd, TRUE );
            }
         break;

      case IDOK:
         lpBuf = (LPSTR)GetWindowLong( hwnd, DWL_USER );
         if( lpBuf )
            LoadDirFileName( hwnd, lpBuf );

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* This function invokes the Mail Erase command.
 */
void FASTCALL CmdMailErase( HWND hwnd, char * pszFilename )
{
   MAILERASEOBJECT meo;

   Amob_New( OBTYPE_MAILERASE, &meo );
   if( NULL != pszFilename )
      Amob_CreateRefString( &meo.recFileName, pszFilename );
   if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_MAILERASE), MailErase, (LPARAM)(LPSTR)&meo ) )
      {
      /* If this file is already marked for erasing
       * warn. Otherwise place the new erase request
       * in the out-basket.
       */
      if( Amob_Find( OBTYPE_MAILERASE, &meo ) )
         {
         wsprintf( lpTmpBuf, GS(IDS_STR290), DRF(meo.recFileName) );
         fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
         }
      else
         Amob_Commit( NULL, OBTYPE_MAILERASE, &meo );
      }
   Amob_Delete( OBTYPE_MAILERASE, &meo );
}

/* Erases a file from the users mail directory.
 */
BOOL EXPORT CALLBACK MailErase( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, MailErase_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the MailErase
 * dialog.
 */
LRESULT FASTCALL MailErase_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, MailErase_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, MailErase_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsMAILERASE );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL MailErase_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   MAILERASEOBJECT FAR * lpmeo;

   /* Dereference the MAILERASE
    * structure.
    */
   lpmeo = (MAILERASEOBJECT FAR *)lParam;
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* Fill in the old name field.
    */
   SetFocus( GetDlgItem( hwnd, IDD_FILENAME ) );
   if( !*DRF(lpmeo->recFileName) && DoesMailDirFileExist() )
      ShowWindow( GetDlgItem( hwnd, IDD_FILENAMEBTN ), SW_SHOW );
   else
      {
      Edit_SetText( GetDlgItem( hwnd, IDD_FILENAME ), DRF(lpmeo->recFileName) );
      ShowWindow( GetDlgItem( hwnd, IDD_FILENAMEBTN ), SW_HIDE );
      }

   /* Set the input field limits and the OK button as
    * appropriate.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_FILENAME ), LEN_CIXFILENAME );
   EnableControl( hwnd, IDOK, *DRF(lpmeo->recFileName) );
   SetFocus( hwndFocus );
   return( FALSE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL MailErase_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_FILENAMEBTN: {
         char sz[ LEN_CIXFILENAME+1 ];

         if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE( IDDLG_MAILDIR ), MailDir, (LPARAM)(LPSTR)sz ) )
            {
            Edit_SetText( GetDlgItem( hwnd, IDD_FILENAME ), sz );
            SetFocus( GetDlgItem( hwnd, IDD_FILENAME ) );
            EnableControl( hwnd, IDOK, TRUE );
            }
         break;
         }

      case IDD_FILENAME:
         if( codeNotify == EN_CHANGE )
            EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndCtl ) > 0 );
         break;

      case IDOK: {
         char szFileName[ LEN_CIXFILENAME+1 ];
         MAILERASEOBJECT FAR * lpmeo;

         /* Get the new field values.
          */
         Edit_GetText( GetDlgItem( hwnd, IDD_FILENAME ), szFileName, LEN_CIXFILENAME+1 );

         /* Get the pointer to the MAILERASEOBJECT structure and
          * fill in the input fields.
          */
         lpmeo = (MAILERASEOBJECT FAR *)GetWindowLong( hwnd, DWL_USER );
         Amob_CreateRefString( &lpmeo->recFileName, szFileName );
         EndDialog( hwnd, TRUE );
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}

/* This function invokes the Mail Rename command.
 */
void FASTCALL CmdMailRename( HWND hwnd, char * pszFilename )
{
   MAILRENAMEOBJECT mro;

   Amob_New( OBTYPE_MAILRENAME, &mro );
   if( NULL != pszFilename )
      Amob_CreateRefString( &mro.recOldName, pszFilename );
   if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_MAILRENAME), MailRename, (LPARAM)(LPSTR)&mro ) )
      {
      /* If this file is already marked for renaming
       * warn. Otherwise place the new rename request
       * in the out-basket.
       */
      if( Amob_Find( OBTYPE_MAILRENAME, &mro ) )
         {
         wsprintf( lpTmpBuf, GS(IDS_STR297), (LPSTR)DRF(mro.recOldName) );
         fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
         }
      else
         Amob_Commit( NULL, OBTYPE_MAILRENAME, &mro );
      }
   Amob_Delete( OBTYPE_MAILRENAME, &mro );
}

/* Renames a file in the users mail directory.
 */
BOOL EXPORT CALLBACK MailRename( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, MailRename_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the MailRename
 * dialog.
 */
LRESULT FASTCALL MailRename_DlgProc( HWND hwnd, UINT message, WPARAM wParam,  LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, MailRename_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, MailRename_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsMAILRENAME );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL MailRename_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   MAILRENAMEOBJECT FAR * lpmro;

   /* Dereference the MAILRENAME
    * structure.
    */
   lpmro = (MAILRENAMEOBJECT FAR *)lParam;
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* Fill in the old name field.
    */
   SetFocus( GetDlgItem( hwnd, IDD_OLDNAME ) );
   if( !*DRF(lpmro->recOldName) && DoesMailDirFileExist() )
      ShowWindow( GetDlgItem( hwnd, IDD_FILENAMEBTN ), SW_SHOW );
   else
      {
      Edit_SetText( GetDlgItem( hwnd, IDD_OLDNAME ), DRF(lpmro->recOldName) );
      ShowWindow( GetDlgItem( hwnd, IDD_FILENAMEBTN ), SW_HIDE );
      }

   /* Fill in the new name field.
    */
   Edit_SetText( GetDlgItem( hwnd, IDD_NEWNAME ), DRF(lpmro->recNewName) );

   /* Set the focus as appropriate.
    */
   if( *DRF(lpmro->recOldName) )
      hwndFocus = GetDlgItem( hwnd, IDD_NEWNAME );
   SetFocus( hwndFocus );

   /* Set the input field limits and the OK button as
    * appropriate.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_OLDNAME ), LEN_CIXFILENAME );
   Edit_LimitText( GetDlgItem( hwnd, IDD_NEWNAME ), LEN_CIXFILENAME );
   EnableControl( hwnd, IDOK, *DRF(lpmro->recOldName) && *DRF(lpmro->recNewName) );
   return( FALSE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL MailRename_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_FILENAMEBTN: {
         char sz[ LEN_CIXFILENAME+1 ];

         if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE( IDDLG_MAILDIR ), MailDir, (LPARAM)(LPSTR)sz ) )
            {
            Edit_SetText( GetDlgItem( hwnd, IDD_OLDNAME ), sz );
            SetFocus( GetDlgItem( hwnd, IDD_NEWNAME ) );
            goto T1;
            }
         break;
         }

      case IDD_OLDNAME:
      case IDD_NEWNAME:
         if( codeNotify == EN_CHANGE )
T1:         EnableControl( hwnd, IDOK,
               Edit_GetTextLength( GetDlgItem( hwnd, IDD_OLDNAME ) ) > 0 &&
               Edit_GetTextLength( GetDlgItem( hwnd, IDD_NEWNAME ) ) > 0 );
         break;

      case IDOK: {
         char szOldName[ LEN_CIXFILENAME+1 ];
         char szNewName[ LEN_CIXFILENAME+1 ];
         MAILRENAMEOBJECT FAR * lpmro;

         /* Get the new field values.
          */
         Edit_GetText( GetDlgItem( hwnd, IDD_OLDNAME ), szOldName, LEN_CIXFILENAME+1 );
         Edit_GetText( GetDlgItem( hwnd, IDD_NEWNAME ), szNewName, LEN_CIXFILENAME+1 );

         /* Get the pointer to the MAILRENAMEOBJECT structure and
          * fill in the input fields.
          */
         lpmro = (MAILRENAMEOBJECT FAR *)GetWindowLong( hwnd, DWL_USER );
         Amob_CreateRefString( &lpmro->recOldName, szOldName );
         Amob_CreateRefString( &lpmro->recNewName, szNewName );
         EndDialog( hwnd, TRUE );
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}

/* This function retrieves the selected directory filename from the Mail
 * Directory list box.
 */
void FASTCALL LoadDirFileName( HWND hwnd, LPSTR lpsz )
{
   HWND hwndList;
   int i;
   int d;

   hwndList = GetDlgItem( hwnd, IDD_LIST );
   d = 0;
   if( LB_ERR != ( i = ListBox_GetCurSel( hwndList ) ) )
      {
      LPSTR lpStr;
      int c;

      lpStr = (LPSTR)ListBox_GetItemData( hwndList, i );
      if( d > 0 )
         lpsz[ d++ ] = ' ';
      for( c = 0; lpStr[ c ] && lpStr[ c ] != '\t' && d < LEN_CIXFILENAME; ++c )
         lpsz[ d++ ] = lpStr[ c ];
      lpsz[ d ] = '\0';
      }
}

/* This function retrieves the size of the specified file from the
 * mail directory.
 */
DWORD FASTCALL GetFileSizeFromMailDirectory( char * pszFileName )
{
   char szUserDir[ _MAX_PATH ];
   DWORD dwSize;
   HNDFILE fh;

   dwSize = 0L;
   Amuser_GetActiveUserDirectory( szUserDir, _MAX_PATH );
   wsprintf( lpPathBuf, "%s\\%s", szUserDir, szMailDirFile );
   if( ( fh = Amfile_Open( lpPathBuf, AOF_READ ) ) != HNDFILE_ERROR )
      {
      LPLBF hBuffer;
      char sz[ 80 ];

      hBuffer = Amlbf_Open( fh, AOF_READ );
      while( Amlbf_Read( hBuffer, sz, sizeof( sz ) - 1, NULL, NULL, &fIsAmeol2Scratchpad ) )
         {
         char szCixFileName[ LEN_CIXFILENAME+1 ];
         register int c;
         char * npsz;

         npsz = sz;
         while( *npsz == ' ' )
            ++npsz;
         for( c = 0; *npsz && *npsz != ' ' && c < LEN_CIXFILENAME; ++c )
            szCixFileName[ c ] = *npsz++;
         szCixFileName[ c ] = '\0';
         if( _strcmpi( szCixFileName, pszFileName ) == 0 )
            {
            while( *npsz == ' ' )
               ++npsz;
            while( isdigit( *npsz ) )
               dwSize = dwSize * 10 + ( *npsz++ - '0' );
            break;
            }
         }
      Amlbf_Close( hBuffer );
      }
   return( dwSize );
}

/* This is the Get Mail Directory out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_MailDirectory( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   switch( uType )
      {
      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_DESCRIBE:
         strcpy( lpBuf, GS(IDS_STR230 ) );
         return( TRUE );

      case OBEVENT_PERSISTENCE:
      case OBEVENT_DELETE:
      case OBEVENT_FIND:
         return( TRUE );
      }
   return( 0L );
}

/* This is the Erase Mail out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_MailErase( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   MAILERASEOBJECT FAR * lpmeo;

   lpmeo = (MAILERASEOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_EDIT: {
         OBINFO obinfo;

         Amob_GetObInfo( (LPOB)dwData, &obinfo );
         Adm_Dialog( hRscLib, hwndFrame, MAKEINTRESOURCE(IDDLG_MAILERASE), MailErase, (LPARAM)(LPSTR)obinfo.lpObData );
         Amob_Commit( (LPOB)dwData, OBTYPE_MAILERASE, obinfo.lpObData );
         return( 0L );
         }

      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_DESCRIBE:
         wsprintf( lpTmpBuf, GS(IDS_STR228), DRF(lpmeo->recFileName) );
         return( TRUE );

      case OBEVENT_EDITABLE:
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_NEW:
         lpmeo = (MAILERASEOBJECT FAR *)dwData;
         _fmemset( lpmeo, 0, sizeof( MAILERASEOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         MAILERASEOBJECT meo;

         Amob_LoadDataObject( fh, &meo, sizeof( MAILERASEOBJECT ) );
         if( fNewMemory( &lpmeo, sizeof( MAILERASEOBJECT ) ) )
            {
            *lpmeo = meo;
            lpmeo->recFileName.hpStr = NULL;
            }
         return( (LRESULT)lpmeo );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpmeo->recFileName );
         return( Amob_SaveDataObject( fh, lpmeo, sizeof( MAILERASEOBJECT ) ) );

      case OBEVENT_COPY: {
         MAILERASEOBJECT FAR * lpmeoNew;

         INITIALISE_PTR( lpmeoNew );
         if( fNewMemory( &lpmeoNew, sizeof( MAILERASEOBJECT ) ) )
            {
            INITIALISE_PTR( lpmeoNew->recFileName.hpStr );
            Amob_CopyRefObject( &lpmeoNew->recFileName, &lpmeo->recFileName );
            }
         return( (LRESULT)lpmeoNew );
         }

      case OBEVENT_FIND: {
         MAILERASEOBJECT FAR * lpmeo1;
         MAILERASEOBJECT FAR * lpmeo2;

         lpmeo1 = (MAILERASEOBJECT FAR *)dwData;
         lpmeo2 = (MAILERASEOBJECT FAR *)lpBuf;
         return( strcmp( DRF(lpmeo1->recFileName), DRF(lpmeo2->recFileName) ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpmeo );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpmeo->recFileName );
         return( TRUE );
      }
   return( 0L );
}

/* This is the Rename Mail out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_MailRename( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   MAILRENAMEOBJECT FAR * lpmro;

   lpmro = (MAILRENAMEOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_EDIT: {
         OBINFO obinfo;

         Amob_GetObInfo( (LPOB)dwData, &obinfo );
         Adm_Dialog( hRscLib, hwndFrame, MAKEINTRESOURCE(IDDLG_MAILRENAME), MailRename, (LPARAM)(LPSTR)obinfo.lpObData );
         Amob_Commit( (LPOB)dwData, OBTYPE_MAILRENAME, obinfo.lpObData );
         return( 0L );
         }

      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_DESCRIBE:
         wsprintf( lpTmpBuf, GS(IDS_STR229), DRF(lpmro->recOldName), DRF(lpmro->recNewName) );
         return( TRUE );

      case OBEVENT_EDITABLE:
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_NEW:
         lpmro = (MAILRENAMEOBJECT FAR *)dwData;
         _fmemset( lpmro, 0, sizeof( MAILRENAMEOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         MAILRENAMEOBJECT meo;

         Amob_LoadDataObject( fh, &meo, sizeof( MAILRENAMEOBJECT ) );
         if( fNewMemory( &lpmro, sizeof( MAILRENAMEOBJECT ) ) )
            {
            *lpmro = meo;
            lpmro->recOldName.hpStr = NULL;
            lpmro->recNewName.hpStr = NULL;
            }
         return( (LRESULT)lpmro );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpmro->recOldName );
         Amob_SaveRefObject( &lpmro->recNewName );
         return( Amob_SaveDataObject( fh, lpmro, sizeof( MAILRENAMEOBJECT ) ) );

      case OBEVENT_COPY: {
         MAILRENAMEOBJECT FAR * lpmroNew;

         INITIALISE_PTR( lpmroNew );
         if( fNewMemory( &lpmroNew, sizeof( MAILRENAMEOBJECT ) ) )
            {
            INITIALISE_PTR( lpmroNew->recOldName.hpStr );
            INITIALISE_PTR( lpmroNew->recNewName.hpStr );
            Amob_CopyRefObject( &lpmroNew->recOldName, &lpmro->recOldName );
            Amob_CopyRefObject( &lpmroNew->recNewName, &lpmro->recNewName );
            }
         return( (LRESULT)lpmroNew );
         }

      case OBEVENT_FIND: {
         MAILRENAMEOBJECT FAR * lpmro1;
         MAILRENAMEOBJECT FAR * lpmro2;

         lpmro1 = (MAILRENAMEOBJECT FAR *)dwData;
         lpmro2 = (MAILRENAMEOBJECT FAR *)lpBuf;
         return( strcmp( DRF(lpmro1->recOldName), DRF(lpmro2->recOldName) ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpmro );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpmro->recOldName );
         Amob_FreeRefObject( &lpmro->recNewName );
         return( TRUE );
      }
   return( 0L );
}

/* This is the Download Mail out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_MailDownload( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   MAILDOWNLOADOBJECT FAR * lpmdlo;

   lpmdlo = (MAILDOWNLOADOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_EDIT: {
         OBINFO obinfo;

         Amob_GetObInfo( (LPOB)dwData, &obinfo );
         Adm_Dialog( hRscLib, hwndFrame, MAKEINTRESOURCE(IDDLG_MAILDOWNLOAD), MailDownload, (LPARAM)(LPSTR)obinfo.lpObData );
         Amob_Commit( (LPOB)dwData, OBTYPE_MAILDOWNLOAD, obinfo.lpObData );
         return( 0L );
         }

      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_DESCRIBE:
         wsprintf( lpTmpBuf, GS(IDS_STR231), DRF(lpmdlo->recCIXFileName) );
         return( TRUE );

      case OBEVENT_EDITABLE:
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_NEW:
         lpmdlo = (MAILDOWNLOADOBJECT FAR *)dwData;
         _fmemset( lpmdlo, 0, sizeof( MAILDOWNLOADOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         MAILDOWNLOADOBJECT mdlo;

         Amob_LoadDataObject( fh, &mdlo, sizeof( MAILDOWNLOADOBJECT ) );
         if( fNewMemory( &lpmdlo, sizeof( MAILDOWNLOADOBJECT ) ) )
            {
            *lpmdlo = mdlo;
            lpmdlo->recDirectory.hpStr = NULL;
            lpmdlo->recFileName.hpStr = NULL;
            lpmdlo->recCIXFileName.hpStr = NULL;
            }
         return( (LRESULT)lpmdlo );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpmdlo->recDirectory );
         Amob_SaveRefObject( &lpmdlo->recFileName );
         Amob_SaveRefObject( &lpmdlo->recCIXFileName );
         return( Amob_SaveDataObject( fh, lpmdlo, sizeof( MAILDOWNLOADOBJECT ) ) );

      case OBEVENT_COPY: {
         MAILDOWNLOADOBJECT FAR * lpmdloNew;

         INITIALISE_PTR( lpmdloNew );
         if( fNewMemory( &lpmdloNew, sizeof( MAILDOWNLOADOBJECT ) ) )
            {
            lpmdloNew->wFlags = lpmdlo->wFlags;
            INITIALISE_PTR( lpmdloNew->recDirectory.hpStr );
            INITIALISE_PTR( lpmdloNew->recFileName.hpStr );
            INITIALISE_PTR( lpmdloNew->recCIXFileName.hpStr );
            Amob_CopyRefObject( &lpmdloNew->recDirectory, &lpmdlo->recDirectory );
            Amob_CopyRefObject( &lpmdloNew->recFileName, &lpmdlo->recFileName );
            Amob_CopyRefObject( &lpmdloNew->recCIXFileName, &lpmdlo->recCIXFileName );
            }
         return( (LRESULT)lpmdloNew );
         }

      case OBEVENT_FIND: {
         MAILDOWNLOADOBJECT FAR * lpmdlo1;
         MAILDOWNLOADOBJECT FAR * lpmdlo2;

         lpmdlo1 = (MAILDOWNLOADOBJECT FAR *)dwData;
         lpmdlo2 = (MAILDOWNLOADOBJECT FAR *)lpBuf;
         return( strcmp( DRF(lpmdlo1->recCIXFileName), DRF(lpmdlo2->recCIXFileName) ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpmdlo );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpmdlo->recCIXFileName );
         Amob_FreeRefObject( &lpmdlo->recDirectory );
         Amob_FreeRefObject( &lpmdlo->recFileName );
         return( TRUE );
      }
   return( 0L );
}

/* This is the Export Mail out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_MailExport( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   MAILEXPORTOBJECT FAR * lpmeo;

   lpmeo = (MAILEXPORTOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_EDIT: {
         OBINFO obinfo;

         Amob_GetObInfo( (LPOB)dwData, &obinfo );
         Adm_Dialog( hRscLib, hwndFrame, MAKEINTRESOURCE(IDDLG_MAILEXPORT), MailExport, (LPARAM)(LPSTR)obinfo.lpObData );
         Amob_Commit( (LPOB)dwData, OBTYPE_MAILEXPORT, obinfo.lpObData );
         return( 0L );
         }

      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_DESCRIBE:
         wsprintf( lpTmpBuf, GS(IDS_STR234), DRF(lpmeo->recFileName),
               DRF(lpmeo->recConfName), DRF(lpmeo->recTopicName) );
         return( TRUE );

      case OBEVENT_EDITABLE:
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_NEW:
         lpmeo = (MAILEXPORTOBJECT FAR *)dwData;
         _fmemset( lpmeo, 0, sizeof( MAILEXPORTOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         MAILEXPORTOBJECT meo;

         Amob_LoadDataObject( fh, &meo, sizeof( MAILEXPORTOBJECT ) );
         if( fNewMemory( &lpmeo, sizeof( MAILEXPORTOBJECT ) ) )
            {
            *lpmeo = meo;
            lpmeo->recConfName.hpStr = NULL;
            lpmeo->recTopicName.hpStr = NULL;
            lpmeo->recFileName.hpStr = NULL;
            }
         return( (LRESULT)lpmeo );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpmeo->recConfName );
         Amob_SaveRefObject( &lpmeo->recTopicName );
         Amob_SaveRefObject( &lpmeo->recFileName );
         return( Amob_SaveDataObject( fh, lpmeo, sizeof( MAILEXPORTOBJECT ) ) );

      case OBEVENT_COPY: {
         MAILEXPORTOBJECT FAR * lpmeoNew;

         INITIALISE_PTR( lpmeoNew );
         if( fNewMemory( &lpmeoNew, sizeof( MAILEXPORTOBJECT ) ) )
            {
            INITIALISE_PTR( lpmeoNew->recConfName.hpStr );
            INITIALISE_PTR( lpmeoNew->recTopicName.hpStr );
            INITIALISE_PTR( lpmeoNew->recFileName.hpStr );
            Amob_CopyRefObject( &lpmeoNew->recConfName, &lpmeo->recConfName );
            Amob_CopyRefObject( &lpmeoNew->recTopicName, &lpmeo->recTopicName );
            Amob_CopyRefObject( &lpmeoNew->recFileName, &lpmeo->recFileName );
            lpmeoNew->wFlags = lpmeo->wFlags;
            }
         return( (LRESULT)lpmeoNew );
         }

      case OBEVENT_FIND: {
         MAILEXPORTOBJECT FAR * lpmeo1;
         MAILEXPORTOBJECT FAR * lpmeo2;

         lpmeo1 = (MAILEXPORTOBJECT FAR *)dwData;
         lpmeo2 = (MAILEXPORTOBJECT FAR *)lpBuf;
         if( strcmp( DRF(lpmeo1->recFileName), DRF(lpmeo2->recFileName) ) != 0 )
            return( FALSE );
         if( strcmp( DRF(lpmeo1->recTopicName), DRF(lpmeo2->recTopicName) ) != 0 )
            return( FALSE );
         return( strcmp( DRF(lpmeo1->recConfName), DRF(lpmeo2->recConfName) ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpmeo );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpmeo->recFileName );
         Amob_FreeRefObject( &lpmeo->recConfName );
         Amob_FreeRefObject( &lpmeo->recTopicName );
         return( TRUE );
      }
   return( 0L );
}

/* This is the Upload Mail out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_MailUpload( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   MAILUPLOADOBJECT FAR * lpmupl;

   lpmupl = (MAILUPLOADOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_EDIT: {
         OBINFO obinfo;

         Amob_GetObInfo( (LPOB)dwData, &obinfo );
         DoMailUpload( (MAILUPLOADOBJECT FAR *)obinfo.lpObData );
         Amob_Commit( (LPOB)dwData, OBTYPE_MAILUPLOAD, obinfo.lpObData );
         return( 0L );
         }

      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_DESCRIBE:
         wsprintf( lpTmpBuf, GS(IDS_STR232), GetFileBasename( DRF(lpmupl->recFileName) ) );
         return( TRUE );

      case OBEVENT_EDITABLE:
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_NEW:
         lpmupl = (MAILUPLOADOBJECT FAR *)dwData;
         _fmemset( lpmupl, 0, sizeof( MAILUPLOADOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         MAILUPLOADOBJECT mupl;

         Amob_LoadDataObject( fh, &mupl, sizeof( MAILUPLOADOBJECT ) );
         if( fNewMemory( &lpmupl, sizeof( MAILUPLOADOBJECT ) ) )
            {
            *lpmupl = mupl;
            lpmupl->recFileName.hpStr = NULL;
            }
         return( (LRESULT)lpmupl );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpmupl->recFileName );
         return( Amob_SaveDataObject( fh, lpmupl, sizeof( MAILUPLOADOBJECT ) ) );

      case OBEVENT_COPY: {
         MAILUPLOADOBJECT FAR * lpmuplNew;

         INITIALISE_PTR( lpmuplNew );
         if( fNewMemory( &lpmuplNew, sizeof( MAILUPLOADOBJECT ) ) )
            {
            INITIALISE_PTR( lpmuplNew->recFileName.hpStr );
            Amob_CopyRefObject( &lpmuplNew->recFileName, &lpmupl->recFileName );
            }
         return( (LRESULT)lpmuplNew );
         }

      case OBEVENT_FIND: {
         MAILUPLOADOBJECT FAR * lpmupl1;
         MAILUPLOADOBJECT FAR * lpmupl2;

         lpmupl1 = (MAILUPLOADOBJECT FAR *)dwData;
         lpmupl2 = (MAILUPLOADOBJECT FAR *)lpBuf;
         return( strcmp( DRF(lpmupl1->recFileName), DRF(lpmupl2->recFileName) ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpmupl );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpmupl->recFileName );
         return( TRUE );
      }
   return( 0L );
}

/* This function returns whether or not the CIX mail directory
 * file exists.
 */
BOOL FASTCALL DoesMailDirFileExist( void )
{
   char szUserDir[ _MAX_PATH ];

   Amuser_GetActiveUserDirectory( szUserDir, _MAX_PATH );
   wsprintf( lpPathBuf, "%s\\%s", szUserDir, szMailDirFile );
   return( Amfile_QueryFile( lpPathBuf ) );
}
