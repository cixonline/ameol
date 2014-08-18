/* AMDBIMP.C - Ameol database importer
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
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <io.h>
#include "rules.h"
#include "editmap.h"

#define  THIS_FILE   __FILE__

static BOOL fDefDlgEx = FALSE;            /* DefDlg recursion flag trap */
static PCAT pcatImportDb;                 /* Import database */
static BOOL fDeleteAfterImporting;        /* TRUE if we delete files after importing them */
static BOOL fApplyRules;                  /* TRUE if we apply rules to imported messages */
static HEADER FAR cixhdr;                 /* Header structure */
static int wError;                        /* Import error status */
static DWORD dwBufSize;                   /* Size of import buffer */
static HPSTR hpBuf;                       /* Pointer to import buffer */
static HFONT hTitleFont;                  /* Title font */
static BOOL fStopped;                     /* Flag set TRUE to abort import process */

static char FAR szAm1DataDir[ _MAX_PATH + 1 ];  /* Path to Ameol1 data directory DATABASE.DAT */
static char FAR szAm1BaseDir[ _MAX_PATH + 1 ];  /* Path to Ameol1 base directory */

#define  DB_ID             0x44BC         /* Ameol v1 database format ID */
#define  DB_VERSION14      0x0140         /* Ameol v1 database version 1.2 */

#define  TF_LOCAL          0x0040

/* One entry in PASSWORD.DAT
 */
#pragma pack(1)
typedef struct tagLOGINENTRY {
   char szLogin[ 40 ];                    /* Login name */
   char szPassword[ 40 ];                 /* Password */
} LOGINENTRY;
#pragma pack()

/* Ameol purge settings.
 */
#pragma pack(1)
typedef struct tagA1_PURGEOPTIONS {
   WORD wFlags;                           /* Purge options flags */
   WORD wMaxSize;                         /* Maximum topic size if PO_SIZEPRUNE */
   WORD wMaxDate;                         /* Maximum days to keep if PO_DATEPRUNE */
} A1_PURGEOPTIONS;
#pragma pack()

/* Ameol database structure
 */
#pragma pack(1)
typedef struct tagA1_DBHEADER {
   WORD wID;                              /* Identifies this file */
   WORD wVersion;                         /* Version number of file format */
   A1_PURGEOPTIONS po;                    /* Purge options */
} A1_DBHEADER;
#pragma pack()

/* Ameol conference structure 
 */
#pragma pack(1)
typedef struct tagA1_CLITEM {
   char szConfName[ 80 ];                 /* Conference name */
   WORD cTopics;                          /* Number of topics in this conference */
   char szModerators[ 80 ];               /* List of moderators */
   char szFileName[ 9 ];                  /* Unique conference file name */
   WORD wFlags;                           /* General purpose flags */
   WORD dateCreated;                      /* Date when conference was created */
   WORD timeCreated;                      /* Time when conference was created */
   char szSigFile[ 9 ];                   /* Signature file */
   char reserved[ 51 ];                   /* Spare 51 bytes for expansion */
} A1_CLITEM;
#pragma pack()

/* Topic structure
 */
#pragma pack(1)
typedef struct tagA1_TLITEM {
   char szTopicName[ 78 ];                /* Topic name */
   WORD wFlags;                           /* Status flags */
   char szFileName[ 9 ];                  /* Unique topic file name */
   DWORD cMsgs;                           /* Number of messages in this topic */
   DWORD cUnRead;                         /* Number of un-read messages */
   DWORD cMarked;                         /* Number of marked messages */
   DWORD dwMinMsg;                        /* Smallest message in this topic */
   DWORD dwMaxMsg;                        /* Largest message in this topic */
   A1_PURGEOPTIONS po;                    /* Topic purge options */
   DWORD cPriority;                       /* Number of priority messages in topic */
   char szSigFile[ 9 ];                   /* Signature file */
   DWORD cUnReadPriority;                 /* Number of priority messages in topic */
   char reserved[ 47 ];                   /* 47 bytes reserved for expansion */
} A1_TLITEM;
#pragma pack()

/* Msg structure (version 1.12)
 */
#pragma pack(1)
typedef struct tagA1_THITEM {
   DWORD dwMsg;                           /* Message number */
   DWORD dwComment;                       /* Message to which this is a comment */
   char szTitle[ 80 ];                    /* Title of message */
   char szAuthor[ 80 ];                   /* Author of this message */
   WORD wDate;                            /* Date of this message (packed) */
   WORD wTime;                            /* Time of this message (packed) */
   DWORD dwOffset;                        /* Offset of text in data file */
   DWORD dwSize;                          /* Length of text in data file */
   WORD wFlags;                           /* Message flags */
   DWORD dwMsgPtr;                        /* Offset of message in thread file */
   char szMessageId[ 80 ];                /* Message ID (Mail/Usenet only) */
   DWORD dwActualMsg;                     /* For mail messages, the physical message number */
} A1_THITEM;
#pragma pack()

static char szOldDatabaseFilename[] = "database.dat";
static char szOldPasswordFilename[] = "password.dat";

BOOL EXPORT WINAPI AmdbImport( HWND, LPSTR );
BOOL EXPORT WINAPI UserList_Import( HWND, LPSTR );
BOOL EXPORT WINAPI CIXList_Import( HWND, LPSTR );
BOOL EXPORT WINAPI UsenetList_Import( HWND, LPSTR );

BOOL EXPORT CALLBACK ImportWizard( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL ImportWizard_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL ImportWizard_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL ImportWizard_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT CALLBACK ImportDatabase( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL ImportDatabase_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL ImportDatabase_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL ImportDatabase_DlgProc( HWND, UINT, WPARAM, LPARAM );

char * FASTCALL GetMonthName( int );
void FASTCALL ReadPasswordDat( HWND );
BOOL FASTCALL ImportTheDatabase( HWND );
int FASTCALL ImportTheMessage( HNDFILE, WORD, PTL, A1_THITEM FAR * );
BOOL FASTCALL CopyDirectories( HWND, const char *, const char * );
BOOL FASTCALL CopySignatures( HWND );
BOOL FASTCALL CopyResumes( HWND );
BOOL FASTCALL CopyListFiles( HWND );
BOOL FASTCALL ImportAddressBook( HWND );
int FASTCALL ReadAmeolRegistryString( LPSTR, LPSTR, LPCSTR, LPSTR, int, LPSTR );

#ifdef WIN32
void FASTCALL IniFileToLocalKey( LPCSTR, char * );
LPCSTR FASTCALL GetIniFileBasename( LPCSTR );
#endif

void FASTCALL CreateResumesIndex( BOOL );

BOOL EXPORT CALLBACK ImportStatus( HWND, UINT, WPARAM, LPARAM );
void FASTCALL ImportStatus_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL ImportStatus_DlgProc( HWND, UINT, WPARAM, LPARAM );

/* This function handles the Ameol database import wizard.
 */
void EXPORT WINAPI AmdbImportWizard( HWND hwnd )
{
   Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_IMPORTWIZARD), ImportWizard, 0L );
}

/* This function handles the ImportDatabase dialog.
 */
BOOL EXPORT CALLBACK ImportWizard( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, ImportWizard_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the ImportWizard
 * dialog.
 */
LRESULT FASTCALL ImportWizard_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, ImportWizard_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, ImportWizard_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsIMPORTWIZARD );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL ImportWizard_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   /* Set the caption font.
    */
   hTitleFont = CreateFont( 24, 0, 0, 0, FW_BOLD, FALSE, FALSE,
                           FALSE, ANSI_CHARSET, OUT_CHARACTER_PRECIS,
                           CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
                           VARIABLE_PITCH | FF_SWISS, "Times New Roman" );
   SetWindowFont( GetDlgItem( hwnd, IDD_TITLE ), hTitleFont, FALSE );

   /* Set the picture buttons.
    */
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_PIX ), hInst, IDB_NC_PIX );
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_TICK_MSGBASE ), hInst, 0 );
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_TICK_RESUMES ), hInst, 0 );
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_TICK_SIGNATURES ), hInst, 0 );
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_TICK_ADDRBOOK ), hInst, 0 );
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_TICK_DATFILES ), hInst, 0 );

   /* Check all the boxes by default.
    */
   CheckDlgButton( hwnd, IDD_MSGBASE, TRUE );
   CheckDlgButton( hwnd, IDD_RESUMES, TRUE );
   CheckDlgButton( hwnd, IDD_SIGNATURES, TRUE );
   CheckDlgButton( hwnd, IDD_ADDRBOOK, TRUE );
   CheckDlgButton( hwnd, IDD_DATFILES, TRUE );

   /* Make Location text nice'n'bold.
    */
   SetWindowFont( GetDlgItem( hwnd, IDD_PAD1 ), hSys8Font, FALSE );

   /* Start by looking for the default Ameol setup.
    */
   if( GetProfileString( "Ameol", "Setup", "", szAm1BaseDir, sizeof(szAm1BaseDir) ) )
      {
      SetDlgItemText( hwnd, IDD_PAD2, szAm1BaseDir );

      /* Assume only one user.
       */
      strcpy( szAm1DataDir, szAm1BaseDir );
      strcat( szAm1DataDir, "\\DATA\\" );

      /* Use default database.
       */
      pcatImportDb = Amdb_GetFirstCategory();

      /* Use safe default options.
       */
      fDeleteAfterImporting = FALSE;
      fApplyRules = TRUE;
      }
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL ImportWizard_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   static BOOL fStarted = FALSE;

   switch( id )
      {
      case IDD_CHANGE:
         if( Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_IMPORTDATABASE), ImportDatabase, 0L ) )
            SetDlgItemText( hwnd, IDD_PAD2, szAm1BaseDir );
         break;

      case IDOK:
         /* If we've already started, set the fParsing flag to
          * FALSE to abort any imports.
          */
         if( fStarted )
            {
            fStopped = TRUE;
            fParsing = FALSE;
            break;
            }

         /* Ensure we have a path to Ameol. Force them to enter one if not.
          */
         if( *szAm1BaseDir == '\0' )
            {
            if( Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_IMPORTDATABASE), ImportDatabase, 0L ) )
               SetDlgItemText( hwnd, IDD_PAD2, szAm1BaseDir );
            break;
            }

         /* First disable all options.
          */
         EnableControl( hwnd, IDD_MSGBASE, FALSE );
         EnableControl( hwnd, IDD_RESUMES, FALSE );
         EnableControl( hwnd, IDD_SIGNATURES, FALSE );
         EnableControl( hwnd, IDD_ADDRBOOK, FALSE );
         EnableControl( hwnd, IDD_DATFILES, FALSE );
         EnableControl( hwnd, IDCANCEL, FALSE );
         EnableControl( hwnd, IDD_HELP, FALSE );

         /* Clear 'ticked' flags for checked items.
          */
         if( IsDlgButtonChecked( hwnd, IDD_MSGBASE ) )
            PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_TICK_MSGBASE ), hInst, 0 );
         if( IsDlgButtonChecked( hwnd, IDD_RESUMES ) )
            PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_TICK_RESUMES ), hInst, 0 );
         if( IsDlgButtonChecked( hwnd, IDD_SIGNATURES ) )
            PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_TICK_SIGNATURES ), hInst, 0 );
         if( IsDlgButtonChecked( hwnd, IDD_ADDRBOOK ) )
            PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_TICK_ADDRBOOK ), hInst, 0 );
         if( IsDlgButtonChecked( hwnd, IDD_DATFILES ) )
            PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_TICK_DATFILES ), hInst, 0 );

         /* Change Start button to Stop
          */
         SetWindowText( GetDlgItem( hwnd, IDOK ), "&Stop" );
         fStarted = TRUE;
         fStopped = FALSE;

         /* Import the Ameol database.
          */
         if( IsDlgButtonChecked( hwnd, IDD_MSGBASE ) )
            if( ImportTheDatabase( hwnd ) )
               PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_TICK_MSGBASE ), hInst, IDB_TICK );
            else
               PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_TICK_MSGBASE ), hInst, IDB_CROSS );

         /* Import resumes.
          */
         if( !fStopped && IsDlgButtonChecked( hwnd, IDD_RESUMES ) )
            if( CopyResumes( hwnd ) )
               PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_TICK_RESUMES ), hInst, IDB_TICK );
            else
               PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_TICK_RESUMES ), hInst, IDB_CROSS );

         /* Import signatures.
          */
         if( !fStopped && IsDlgButtonChecked( hwnd, IDD_SIGNATURES ) )
            if( CopySignatures( hwnd ) )
               PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_TICK_SIGNATURES ), hInst, IDB_TICK );
            else
               PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_TICK_SIGNATURES ), hInst, IDB_CROSS );

         /* Import the address book.
          */
         if( !fStopped && IsDlgButtonChecked( hwnd, IDD_ADDRBOOK ) )
            if( ImportAddressBook( hwnd ) )
               PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_TICK_ADDRBOOK ), hInst, IDB_TICK );
            else
               PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_TICK_ADDRBOOK ), hInst, IDB_CROSS );

         /* Import the data files
          */
         if( !fStopped && IsDlgButtonChecked( hwnd, IDD_DATFILES ) )
            if( CopyListFiles( hwnd ) )
               PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_TICK_DATFILES ), hInst, IDB_TICK );
            else
               PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_TICK_DATFILES ), hInst, IDB_CROSS );

         /* Re-enable all controls.
          */
         EnableControl( hwnd, IDD_MSGBASE, TRUE );
         EnableControl( hwnd, IDD_RESUMES, TRUE );
         EnableControl( hwnd, IDD_SIGNATURES, TRUE );
         EnableControl( hwnd, IDD_ADDRBOOK, TRUE );
         EnableControl( hwnd, IDD_DATFILES, TRUE );
         EnableControl( hwnd, IDCANCEL, TRUE );
         EnableControl( hwnd, IDD_HELP, TRUE );

         /* Change Stop button to Start and the Cancel
          * button to Finish
          */
         SetWindowText( GetDlgItem( hwnd, IDOK ), "&Start" );
         SetWindowText( GetDlgItem( hwnd, IDCANCEL ), "&Finish" );
         ChangeDefaultButton( hwnd, IDCANCEL );
         fStarted = FALSE;
         break;

      case IDCANCEL:
         DeleteFont( hTitleFont );
         EndDialog( hwnd, 0 );
         break;
      }
}

/* This function is the Ameol database import entry point.
 */
BOOL EXPORT WINAPI AmdbImport( HWND hwnd, LPSTR lpszFilename )
{
   /* Initialise.
    */
   fApplyRules = TRUE;
   fDeleteAfterImporting = FALSE;
   wError = AMDBERR_NOERROR;
   if( !Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_IMPORTDATABASE), ImportDatabase, 0L ) )
      return( FALSE );
   if( !ImportTheDatabase( hwnd ) )
      return( FALSE );
   return( TRUE );
}

/* This function handles the ImportDatabase dialog.
 */
BOOL EXPORT CALLBACK ImportDatabase( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, ImportDatabase_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the ImportDatabase
 * dialog.
 */
LRESULT FASTCALL ImportDatabase_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, ImportDatabase_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, ImportDatabase_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_MEASUREITEM, FolderCombo_OnMeasureItem );
      HANDLE_DLGMSG( hwnd, WM_DRAWITEM, FolderCombo_OnDrawItem );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsIMPORTDATABASE );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL ImportDatabase_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndEdit;
   HWND hwndList;
   PCAT pcat;
   int index;

   /* Set the default Ameol directory.
    */
   GetProfileString( "Ameol", "Setup", "", szAm1BaseDir, sizeof(szAm1BaseDir) );
   hwndEdit = GetDlgItem( hwnd, IDD_DIRECTORY );
   Edit_SetText( hwndEdit, szAm1BaseDir );
   Edit_LimitText( hwndEdit, _MAX_PATH );

   /* Fill the list of users.
    */
   ReadPasswordDat( hwnd );

   /* Fill the list of databases.
    */
   hwndList = GetDlgItem( hwnd, IDD_DBLIST );
   for( pcat = Amdb_GetFirstCategory(); pcat; pcat = Amdb_GetNextCategory( pcat ) )
      ComboBox_InsertString( hwndList, -1, pcat );

   /* Highlight the CIX database.
    */
   pcat = GetCIXCategory();
   index = ComboBox_FindItemData( hwndList, -1, pcat );
   if( CB_ERR == index )
      index = 0;  
   ComboBox_SetCurSel( hwndList, index );

   /* Disable list box if only one database.
    */
   EnableWindow( hwndList, ComboBox_GetCount( hwndList ) > 1 );

   /* Set the Browse picture button.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_PICKER ), hInst, IDB_FOLDERBUTTON );

   /* Check option buttons.
    */
   CheckDlgButton( hwnd, IDD_APPLYRULES, fApplyRules );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL ImportDatabase_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_DIRECTORY:
         if( codeNotify == EN_KILLFOCUS && EditMap_GetModified( hwndCtl ) )
            ReadPasswordDat( hwnd );
         break;

      case IDD_BROWSE: {
         HWND hwndEdit;
         CHGDIR chgdir;

         hwndEdit = GetDlgItem( hwnd, IDD_DIRECTORY );
         Edit_GetText( hwndEdit, chgdir.szPath, sizeof(chgdir.szPath) );
         strcpy( chgdir.szTitle, GS(IDS_STR882) );
         strcpy( chgdir.szPrompt, GS(IDS_STR883) );
         chgdir.hwnd = hwnd;
         if( Amuser_ChangeDirectory( &chgdir ) )
            {
            Edit_SetText( hwndEdit, chgdir.szPath );
            ReadPasswordDat( hwnd );
            }
         SetFocus( hwndEdit );
         break;
         }

      case IDOK: {
         char szPath[ _MAX_PATH+1 ];
         HWND hwndEdit;
         HWND hwndList;
         int index;

         /* Create the path to the DATABASE.DAT file.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_DIRECTORY );
         hwndList = GetDlgItem( hwnd, IDD_USERLIST );
         Edit_GetText( hwndEdit, szAm1BaseDir, sizeof(szAm1BaseDir) );

         /* Look in the INI file to locate the data directory for
          * this user.
          */
         index = ComboBox_GetCurSel( hwndList );
         ASSERT( index != CB_ERR );
         if( 0 == index )
            strcpy( szPath, "AMEOL.INI" );
         else
            {
            char szLoginUser[ 40 ];

            ComboBox_GetLBText( hwndList, index, szLoginUser );
            wsprintf( szPath, "%s\\%s.INI", szAm1BaseDir, szLoginUser );
            }
         if( !ReadAmeolRegistryString( "Directories", "data", "", szAm1DataDir, _MAX_DIR, szPath ) )
            {
            /* No registry entry for the Ameol database, so it
             * has to be the default of the data directory in
             * the base directory.
             */
            strcpy( szAm1DataDir, szAm1BaseDir );
            if( szAm1DataDir[ strlen( szAm1DataDir ) - 1 ] != '\\' )
               strcat( szAm1DataDir, "\\" );
            strcat( szAm1DataDir, "data" );
            }
         strcat( szAm1DataDir, "\\" );

         /* There must be a DATABASE.DAT in the target directory.
          */
         strcpy( szPath, szAm1DataDir );
         strcat( szPath, szOldDatabaseFilename );
         if( !Amfile_QueryFile( szPath ) )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR881), MB_OK|MB_ICONINFORMATION );
            HighlightField( hwnd, IDD_DIRECTORY );
            break;
            }

         /* Get the options.
          */
         fDeleteAfterImporting = IsDlgButtonChecked( hwnd, IDD_DELETE );
         fApplyRules = IsDlgButtonChecked( hwnd, IDD_APPLYRULES );

         /* Finally, get the target database.
          */
         hwndList = GetDlgItem( hwnd, IDD_DBLIST );
         index = ComboBox_GetCurSel( hwndList );
         ASSERT( CB_ERR != index );
         pcatImportDb = (LPVOID)ComboBox_GetItemData( hwndList, index );
         ASSERT( NULL != pcatImportDb );
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* This function extracts the directory specified in the
 * Directory field and uses that to locate the PASSWORD.DAT
 * file. Having found it, it fills the user list with the
 * list of names specified therein.
 */
void FASTCALL ReadPasswordDat( HWND hwnd )
{
   HWND hwndEdit;
   HWND hwndList;

   /* Create the full path to the Password file.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_DIRECTORY );
   hwndList = GetDlgItem( hwnd, IDD_USERLIST );
   ComboBox_ResetContent( hwndList );
   Edit_GetText( hwndEdit, szAm1DataDir, sizeof(szAm1DataDir) );
   if( *szAm1DataDir )
      {
      HNDFILE fh;

      /* Make the full pathname.
       */
      if( szAm1DataDir[ strlen( szAm1DataDir ) - 1 ] != '\\' )
         strcat( szAm1DataDir, "\\" );
      strcat( szAm1DataDir, szOldPasswordFilename );

      /* Try to open the file.
       */
      if( HNDFILE_ERROR != ( fh = Amfile_Open( szAm1DataDir, AOF_READ ) ) )
         {
         LOGINENTRY le;

         /* Loop for each user and fill the combobox.
          */
         while( Amfile_Read( fh, &le, sizeof( LOGINENTRY ) ) == sizeof( LOGINENTRY ) )
            {
            Amuser_Decrypt( le.szLogin, rgEncodeKey );
            ComboBox_InsertString( hwndList, -1, le.szLogin );
            }
         ComboBox_SetCurSel( hwndList, 0 );

         /* Disable if no more than 2 users listed.
          */
         Amfile_Close( fh );
         }
      }
   EnableWindow( hwndList, ComboBox_GetCount( hwndList ) > 1 );
   EnableControl( hwnd, IDOK, ComboBox_GetCount( hwndList ) > 0 );
}

/* This function copies signatures from \DATA\SIG in the old
 * Ameol directory to \DATA\SIG in the new Ameol directory.
 */
BOOL FASTCALL CopySignatures( HWND hwnd )
{
   char szDestDir[ _MAX_PATH+1 ];
   char szSrcDir[ _MAX_PATH+1 ];

   /* Create the source directory name.
    */
   strcpy( szSrcDir, szAm1DataDir );
   strcat( szSrcDir, "SIG" );

   /* Create the target directory name.
    */
   Amuser_GetDataDirectory( szDestDir, sizeof(szDestDir ) );
   strcat( szDestDir, "\\" );
   strcat( szDestDir, "SIG" );
   return( CopyDirectories( hwnd, szSrcDir, szDestDir ) );
}

/* This function copies resumes from \DATA\RESUME in the old
 * Ameol directory to \RESUMES in the new Ameol directory.
 */
BOOL FASTCALL CopyResumes( HWND hwnd )
{
   char szSrcDir[ _MAX_PATH+1 ];

   /* Create the source directory name.
    */
   strcpy( szSrcDir, szAm1DataDir );
   strcat( szSrcDir, "RESUME" );

   /* Copy them, then call CreateResumesIndex to expand
    * truncated resumes to their long filename format.
    */
   if( CopyDirectories( hwnd, szSrcDir, pszResumesDir ) )
      {
   #ifdef WIN32
      CreateResumesIndex( TRUE );
   #endif
      return( TRUE );
      }
   return( FALSE );
}

/* This function copies across USERS.DAT, CIXLIST.DAT and
 * USENET.DAT
 */
BOOL FASTCALL CopyListFiles( HWND hwnd )
{
   char szSrcDir[ _MAX_PATH+1 ];

   /* Do the users list file.
    */
   strcpy( szSrcDir, szAm1BaseDir );
   strcat( szSrcDir, "\\" );
   strcat( szSrcDir, "users.dat" );
   if( Amfile_QueryFile( szSrcDir ) )
      if( !UserList_Import( hwnd, szSrcDir ) )
         return( FALSE );

   /* Do the CIX conferences list file.
    */
   strcpy( szSrcDir, szAm1BaseDir );
   strcat( szSrcDir, "\\" );
   strcat( szSrcDir, "cix.dat" );
   if( Amfile_QueryFile( szSrcDir ) )
      if( !fStopped && !CIXList_Import( hwnd, szSrcDir ) )
         return( FALSE );

   /* Do the Usenet newsgroups list file.
    */
   strcpy( szSrcDir, szAm1BaseDir );
   strcat( szSrcDir, "\\" );
   strcat( szSrcDir, "usenet.dat" );
   if( Amfile_QueryFile( szSrcDir ) )
      if( !fStopped && !UsenetList_Import( hwnd, szSrcDir ) )
         return( FALSE );
   return( !fStopped );
}

/* Import an Ameol address book.
 */
BOOL FASTCALL ImportAddressBook( HWND hwnd )
{
   char szSrcDir[ _MAX_PATH+1 ];

   /* The Ameol1 address book is in a fixed
    * location.
    */
   strcpy( szSrcDir, szAm1DataDir );
   strcat( szSrcDir, "\\" );
   strcat( szSrcDir, "ADDRBOOK" );
   strcat( szSrcDir, "\\" );
   strcat( szSrcDir, "ADDRBOOK.DAT" );
   if( Amfile_QueryFile( szSrcDir ) )
      {
      char szDestDir[ _MAX_PATH+1];

      Amuser_GetActiveUserDirectory( szDestDir, _MAX_FNAME );
      strcat( szDestDir, "\\" );
      strcat( szDestDir, "ADDRBOOK.DAT" );
      if( !Amfile_Copy( szSrcDir, szDestDir ) )
         return( FALSE );
      }
   return( TRUE );
}

/* This function copies the contents of the source directory to the
 * destination directory.
 */
BOOL FASTCALL CopyDirectories( HWND hwnd, const char * pszSrcDir, const char * pszDestDir )
{
   LPSTR pszDestDirA;
   LPSTR pszSrcDirA;

   /* Make sure that the target directory exists.
    */
   if( !Amdir_QueryDirectory( pszDestDir ) )
      if( !Amdir_Create( pszDestDir ) )
         {
         wsprintf( lpTmpBuf, GS(IDS_STR1070), pszDestDir );
         fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
         return( FALSE );
         }

   /* Make a copy of the src and dest dirs.
    */
   INITIALISE_PTR(pszSrcDirA);
   INITIALISE_PTR(pszDestDirA);
   if( fNewMemory( &pszSrcDirA, _MAX_PATH ) )
      {
      if( fNewMemory( &pszDestDirA, _MAX_PATH ) )
         {
         register int n;
         int nDestBase;
         int nSrcBase;
         FINDDATA ft;
         HFIND r;

         /* Now loop for each file in the source directory and
          * copy it to the destination.
          */
         strcpy( pszSrcDirA, pszSrcDir );
         strcpy( pszDestDirA, pszDestDir );
         strcat( pszSrcDirA, "\\" );
         strcat( pszDestDirA, "\\" );
         nSrcBase = strlen( pszSrcDirA );
         nDestBase = strlen( pszDestDirA );
         strcpy( pszSrcDirA + nSrcBase, "*.*" );
         for( n = r = Amuser_FindFirst( pszSrcDirA, _A_NORMAL, &ft ); n != -1; n = Amuser_FindNext( r, &ft ) )
            {
            if( fStopped )
               break;
            if( strcmp( ft.name, "." ) == 0 || strcmp( ft.name, ".." ) == 0 )
               continue;
            strcpy( pszSrcDirA + nSrcBase, ft.name );
            strcpy( pszDestDirA + nDestBase, ft.name );
            if( !Amfile_Copy( pszSrcDirA, pszDestDirA ) )
               break;

            /* Delete each file after importing if copied okay.
             */
            if( fDeleteAfterImporting )
               Amfile_Delete( pszSrcDirA );
            }
         Amuser_FindClose( r );
         FreeMemory( &pszDestDirA );
         }
      FreeMemory( &pszSrcDirA );
      }

   /* If told to do so, remove the original directory
    * after importing.
    */
   if( !fStopped && fDeleteAfterImporting )
      if( !Amdir_Remove( pszSrcDir ) )
         {
         wsprintf( lpTmpBuf, GS(IDS_STR1071), pszSrcDir );
         fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
         }
   return( !fStopped );
}

/* Here's where we carry out the actual import.
 */
BOOL FASTCALL ImportTheDatabase( HWND hwnd )
{
   HNDFILE fh;

   /* Open the database file.
    */
   BEGIN_PATH_BUF;
   wError = AMDBERR_NOERROR;
   wsprintf( lpPathBuf, "%s%s", szAm1DataDir, szOldDatabaseFilename );
   if( ( fh = Amfile_Open( lpPathBuf, AOF_READ ) ) == HNDFILE_ERROR )
      {
      /* This shouldn't happen, because we've checked for the existence of
       * DATABASE.DAT before now. But what the heck...!
       */
      FileOpenError( hwnd, lpPathBuf, FALSE, FALSE );
      }
   else {
      char szDBPass[ 40 ];
      A1_DBHEADER dbHdr;
      A1_CLITEM clItem;
      DWORD dwFileSize;
      DWORD dwFileRead;
      HWND hwndGauge;
      UINT total;
      HWND hDlg;
      WORD wCount;
      int cRead;
      
      /* Okay. Now's the time to show the progress window.
       */
      EnableWindow( GetParent( hwnd ), FALSE );
      hDlg = Adm_CreateDialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_IMPORTSTATUS), ImportStatus, 0L );

      /* Get the database file size.
       */
      dwFileSize = Amfile_GetFileSize( fh );
      dwFileRead = 0L;

      /* Set the progress bar gauge range.
       */
      VERIFY( hwndGauge = GetDlgItem( hDlg, IDD_GAUGE ) );
      SendMessage( hwndGauge, PBM_SETRANGE, 0, MAKELPARAM( 0, 100 ) );
      SendMessage( hwndGauge, PBM_SETSTEP, 1, 0L );

      /* Check that we're importing a version 14 format database. We can't
       * handle any other type.
       */
      cRead = Amfile_Read( fh, &dbHdr, sizeof( A1_DBHEADER ) );
      if( cRead != sizeof( A1_DBHEADER ) || dbHdr.wVersion != DB_VERSION14 || dbHdr.wID != DB_ID )
         {
         fMessageBox( hDlg, 0, GS(IDS_STR672), MB_OK|MB_ICONEXCLAMATION );
         Amfile_Close( fh );
         }
      else
         {
         /* Update the gauge.
          */
         dwFileRead += cRead;
         wCount = (WORD)( ( dwFileRead * 100 ) / dwFileSize );
         SendMessage( hwndGauge, PBM_SETPOS, wCount, 0L );
   
         /* The next thing is the password. Skip it for now, but we might like to
          * take a more agressive stance in the future.
          */
         if( ( cRead = Amfile_Read( fh, &szDBPass, 40 ) ) != 40 )
            {
            DiskReadError( hDlg, lpPathBuf, FALSE, FALSE );
            Amfile_Close( fh );
            EnableWindow( GetParent( hwnd ), TRUE );
            DestroyWindow( hDlg );
            return( FALSE );
            }
         total = 0;
   
         /* Update the gauge.
          */
         dwFileRead += cRead;
         wCount = (WORD)( ( dwFileRead * 100 ) / dwFileSize );
         SendMessage( hwndGauge, PBM_SETPOS, wCount, 0L );
   
         /* Read and link the list of conferences and topics from the database file.
          */
         cRead = Amfile_Read( fh, &clItem, sizeof( A1_CLITEM ) );
         while( wError == AMDBERR_NOERROR && cRead == sizeof( A1_CLITEM ) )
            {
            char szDestDir[ _MAX_PATH+1 ];
            register WORD c;
            FOLDERINFO flItem;
            PCL pcl;
   
            /* Yield.
             */
            TaskYield();
            if( AMDBERR_USERABORT == wError )
               break;
   
            /* Update the gauge.
             */
            dwFileRead += cRead;
            wCount = (WORD)( ( dwFileRead * 100 ) / dwFileSize );
            SendMessage( hwndGauge, PBM_SETPOS, wCount, 0L );
   
            /* Special case for Mail and Usenet.
             */
            if( strcmp( clItem.szConfName, "mail" ) == 0 )
               strcpy( clItem.szConfName, "Mail" );
            if( strcmp( clItem.szConfName, "Usenet" ) == 0 )
               strcpy( clItem.szConfName, "News" );

            /* Create the folder. We'll be using defaults here since we can't
             * manipulate the pcl pointer directly.
             */
            if( NULL == ( pcl = Amdb_OpenFolder( pcatImportDb, clItem.szConfName ) ) )
               if( NULL == ( pcl = Amdb_CreateFolder( pcatImportDb, clItem.szConfName, CFF_LAST ) ) )
                  {
                  OutOfMemoryError( hDlg, FALSE, FALSE );
                  Amfile_Close( fh );
                  EnableWindow( GetParent( hwnd ), TRUE );
                  DestroyWindow( hDlg );
                  return( FALSE );
                  }
   
            /* If this structure has a list of moderators, pass it to the
             * Amdb_SetModeratorList function to fill in the moderator list.
             */
            if( *clItem.szModerators )
               Amdb_SetModeratorList( pcl, clItem.szModerators );
            else
               {
               HNDFILE fhCnf;
   
               /* Look for the <conf>.CNF file in the database directory and
                * if found, read it into memory.
                */
               wsprintf( lpPathBuf, "%s%s.CNF", szAm1DataDir, clItem.szFileName );
               if( HNDFILE_ERROR != ( fhCnf = Amfile_Open( lpPathBuf, AOF_READ ) ) )
                  {
                  HPSTR lpModList;
                  DWORD dwSize;
   
                  INITIALISE_PTR(lpModList);
   
                  /* Get the file size.
                   */
                  dwSize = Amfile_GetFileSize( fhCnf );
   
                  /* Allocate a block of memory to accommodate the file
                   * and read the file. Then pass the block to the Amdb_SetModeratorList
                   * function to create the CNF file anew locally.
                   */
                  if( fNewMemory32( &lpModList, dwSize + 1 ) )
                     {
                     if( Amfile_Read32( fhCnf, lpModList, dwSize ) != dwSize )
                        {
                        DiskReadError( hDlg, lpPathBuf, FALSE, FALSE );
                        wError = AMDBERR_DISKREADERROR;
                        }
                     else
                        {
                        lpModList[ dwSize ] = '\0';
                        Amdb_SetModeratorList( pcl, lpModList );
                        }
                     FreeMemory32( &lpModList );
                     }
                  Amfile_Close( fhCnf );
   
                  /* Delete the CNF file
                   */
                  if( wError == AMDBERR_NOERROR && fDeleteAfterImporting )
                     Amfile_Delete( lpPathBuf );
                  }
               }
   
            /* Import the participants list.
             */
            wsprintf( lpPathBuf, "%sPARLIST\\%s.PAR", szAm1DataDir, clItem.szFileName );
            Amuser_GetDataDirectory( szDestDir, sizeof(szDestDir ) );
            Amdb_GetFolderInfo( pcl, &flItem );
            strcat( szDestDir, "\\PARLIST" );
            Amdir_Create( szDestDir );
            strcat( szDestDir, "\\" );
            strcat( szDestDir, flItem.szFileName );
            strcat( szDestDir, ".PAR" );
            if( Amfile_Copy( lpPathBuf, szDestDir ) )
               if( fDeleteAfterImporting )
                  Amfile_Delete( lpPathBuf );
   
            /* Import the CTI conference/topic information
             * list as used by Moderate.
             */
            wsprintf( lpPathBuf, "%s%s.CTI", szAm1DataDir, clItem.szFileName );
            Amuser_GetDataDirectory( szDestDir, sizeof(szDestDir ) );
            strcat( szDestDir, "\\" );
            strcat( szDestDir, flItem.szFileName );
            strcat( szDestDir, ".CTI" );
            if( Amfile_Copy( lpPathBuf, szDestDir ) )
               if( fDeleteAfterImporting )
                  Amfile_Delete( lpPathBuf );
   
            /* Set the folder signature.
             */
            Amdb_SetFolderSignature( pcl, clItem.szSigFile );
   
            /* Loop for each topic in the folder.
             */
            for( c = 0; wError == AMDBERR_NOERROR && c < clItem.cTopics; ++c )
               {
               A1_TLITEM tlItem;
               PURGEOPTIONS po;
               HNDFILE fhThd;
               HNDFILE fhTxt;
               PTL ptl;
               WORD wType;
   
               /* Yield.
                */
               TaskYield();
               if( AMDBERR_USERABORT == wError )
                  break;
   
               /* Read a topic record.
                */
               cRead = Amfile_Read( fh, &tlItem, sizeof( A1_TLITEM ) );
               if( cRead != sizeof( A1_TLITEM ) )
                  {
                  DiskReadError( hDlg, lpPathBuf, FALSE, FALSE );
                  wError = AMDBERR_DISKREADERROR;
                  break;
                  }
   
               /* Update the gauge.
                */
               dwFileRead += cRead;
               wCount = (WORD)( ( dwFileRead * 100 ) / dwFileSize );
               SendMessage( hwndGauge, PBM_SETPOS, wCount, 0L );
   
               /* Update the status
                */
               SetDlgItemText( hDlg, IDD_CONFNAME, clItem.szConfName );
               SetDlgItemText( hDlg, IDD_TOPICNAME, tlItem.szTopicName );
   
               /* Create the topic.
                */
               if( strcmp( tlItem.szTopicName, "messages" ) == 0 )
                  strcpy( tlItem.szTopicName, szPostmaster );
               if( NULL == ( ptl = Amdb_OpenTopic( pcl, tlItem.szTopicName ) ) )
                  {
                  wType = FTYPE_CIX;
                  if( strcmp( clItem.szConfName, "Mail" ) == 0 )
                     wType = FTYPE_MAIL;
                  if( strcmp( clItem.szConfName, "News" ) == 0 )
                     wType = FTYPE_NEWS;
                  if( ( tlItem.wFlags & TF_LOCAL ) && wType != FTYPE_MAIL )
                     wType = FTYPE_LOCAL;
                  ptl = Amdb_CreateTopic( pcl, tlItem.szTopicName, wType );
                  if( NULL == ptl )
                     {
                     OutOfMemoryError( hDlg, FALSE, FALSE );
                     Amfile_Close( fh );
                     EnableWindow( GetParent( hwnd ), TRUE );
                     DestroyWindow( hDlg );
                     return( FALSE );
                     }
                  }
               else
                  wType = Amdb_GetTopicType( ptl );
   
               /* Set the topic signature.
                */
               Amdb_SetTopicSignature( ptl, tlItem.szSigFile );
   
               /* Set the topic purge options.
                */
               po.wFlags = tlItem.po.wFlags;
               po.wMaxSize = tlItem.po.wMaxSize;
               po.wMaxDate = tlItem.po.wMaxDate;
               Amdb_SetTopicPurgeOptions( ptl, &po );
   
               /* Now here's where we read the actual messages. Open the
                * message files.
                */
               wsprintf( lpPathBuf, "%s%s.THD", szAm1DataDir, tlItem.szFileName );
               fhThd = Amfile_Open( lpPathBuf, AOF_READ );
               wsprintf( lpPathBuf, "%s%s.TXT", szAm1DataDir, tlItem.szFileName );
               fhTxt = Amfile_Open( lpPathBuf, AOF_READ );
   
               /* Did we get 'em?
                */
               if( HNDFILE_ERROR != fhThd && HNDFILE_ERROR != fhTxt )
                  {
                  A1_THITEM thItem;

                  /* Lock our topic.
                   */
                  Amdb_LockTopic( ptl );

                  /* Allocate a temporary buffer of 8K.
                   */
                  INITIALISE_PTR(hpBuf);
                  if( !fNewMemory32( &hpBuf, 8192 ) )
                     {
                     OutOfMemoryError( hDlg, FALSE, FALSE );
                     wError = AMDBERR_OUTOFMEMORY;
                     }
                  dwBufSize = 0L;

                  /* Loop for every message in the topic.
                   */
                  while( wError == AMDBERR_NOERROR && Amfile_Read( fhThd, &thItem, sizeof( A1_THITEM ) ) == sizeof( A1_THITEM ) )
                     wError = ImportTheMessage( fhTxt, wType, ptl, &thItem );

                  /* Deal with out of memory error.
                   */
                  if( AMDBERR_OUTOFMEMORY == wError )
                     OutOfMemoryError( hDlg, FALSE, FALSE );
                  if( NULL != hpBuf )
                     FreeMemory32( &hpBuf );

                  /* Unlock our topic.
                   */
                  Amdb_UnlockTopic( ptl );

                  /* Close the files now.
                   */
                  Amfile_Close( fhThd );
                  Amfile_Close( fhTxt );
                  fhThd = HNDFILE_ERROR;
                  fhTxt = HNDFILE_ERROR;
   
                  /* Import the file lists.
                   */
                  if( wError == AMDBERR_NOERROR )
                     {
                     TOPICINFO topicInfo;
   
                     /* Copy FLS files.
                      */
                     wsprintf( lpPathBuf, "%sFLIST\\%s.FLS", szAm1DataDir, tlItem.szFileName );
                     Amuser_GetDataDirectory( szDestDir, sizeof(szDestDir ) );
                     Amdb_GetTopicInfo( ptl, &topicInfo );
                     strcat( szDestDir, "\\FLIST" );
                     Amdir_Create( szDestDir );
                     strcat( szDestDir, "\\" );
                     strcat( szDestDir, topicInfo.szFileName );
                     strcat( szDestDir, ".FLS" );
                     if( Amfile_Copy( lpPathBuf, szDestDir ) )
                        if( fDeleteAfterImporting )
                           Amfile_Delete( lpPathBuf );
   
                     /* Copy FLD files.
                      */
                     wsprintf( lpPathBuf, "%sFLIST\\%s.FLD", szAm1DataDir, tlItem.szFileName );
                     Amuser_GetDataDirectory( szDestDir, sizeof(szDestDir ) );
                     Amdb_GetTopicInfo( ptl, &topicInfo );
                     strcat( szDestDir, "\\FLIST" );
                     Amdir_Create( szDestDir );
                     strcat( szDestDir, "\\" );
                     strcat( szDestDir, topicInfo.szFileName );
                     strcat( szDestDir, ".FLD" );
                     if( Amfile_Copy( lpPathBuf, szDestDir ) )
                        if( fDeleteAfterImporting )
                           Amfile_Delete( lpPathBuf );
   
                     /* Copy FLM files.
                      */
                     wsprintf( lpPathBuf, "%sFLIST\\%s.FLM", szAm1DataDir, tlItem.szFileName );
                     Amuser_GetDataDirectory( szDestDir, sizeof(szDestDir ) );
                     Amdb_GetTopicInfo( ptl, &topicInfo );
                     strcat( szDestDir, "\\FLIST" );
                     Amdir_Create( szDestDir );
                     strcat( szDestDir, "\\" );
                     strcat( szDestDir, topicInfo.szFileName );
                     strcat( szDestDir, ".FLM" );
                     if( Amfile_Copy( lpPathBuf, szDestDir ) )
                        if( fDeleteAfterImporting )
                           Amfile_Delete( lpPathBuf );
                     }
   
                  /* If topic imported okay AND we delete as we go,
                   * delete the data files.
                   */
                  if( wError == AMDBERR_NOERROR && fDeleteAfterImporting )
                     {
                     wsprintf( lpPathBuf, "%s%s.THD", szAm1DataDir, tlItem.szFileName );
                     Amfile_Delete( lpPathBuf );
                     wsprintf( lpPathBuf, "%s%s.TXT", szAm1DataDir, tlItem.szFileName );
                     Amfile_Delete( lpPathBuf );
                     }
                  }
   
               /* Close any handles not yet closed.
                */
               if( HNDFILE_ERROR != fhTxt )
                  Amfile_Close( fhTxt );
               if( HNDFILE_ERROR != fhThd )
                  Amfile_Close( fhThd );

               /* Finally set the topic flags. The flag values will be identical
                * between Ameol v1 and Ameol v2.
                */
               tlItem.wFlags &= ~(TF_ONDISK|TF_CHANGED);
               if( wType == FTYPE_NEWS )
                  tlItem.wFlags |= TF_READFULL;
               Amdb_SetTopicFlags( ptl, tlItem.wFlags, TRUE );
                  

               }
            cRead = Amfile_Read( fh, &clItem, sizeof( A1_CLITEM ) );
            }
   
         /* Close the file now.
          */
         SendMessage( hwndGauge, PBM_SETPOS, 100, 0L );
         Amfile_Close( fh );
   
         /* If we're not in batch mode, try and save the database file now.
          */
         if( wError == AMDBERR_NOERROR )
            while( Amdb_CommitDatabase( FALSE ) != AMDBERR_NOERROR )
               {
               wError = AMDBERR_OUTOFDISKSPACE;
               if( !DiskSaveError( hDlg, pszDataDir, TRUE, FALSE ) )
                  break;
               wError = AMDBERR_NOERROR;
               }
   
         /* All done, so delete the database if all okay.
          */
         if( wError == AMDBERR_NOERROR && fDeleteAfterImporting )
            {
            wsprintf( lpPathBuf, "%s%s", szAm1DataDir, szOldDatabaseFilename );
            Amfile_Delete( lpPathBuf );
            }
         }

      /* Set the Stopped flag if the user aborted.
       */
      if( AMDBERR_USERABORT == wError )
         fStopped = TRUE;

      /* Close the status window
       */
      EnableWindow( GetParent( hwnd ), TRUE );
      DestroyWindow( hDlg );
      }
   END_PATH_BUF;
   return( AMDBERR_NOERROR == wError );
}

/* This function imports one message.
 */
int FASTCALL ImportTheMessage( HNDFILE fhTxt, WORD wType, PTL ptl, A1_THITEM FAR * pThItem )
{
   DWORD dwAllocSize;
   UINT cbHdr;
   
   /* Yield.
    */
   TaskYield();
   if( AMDBERR_USERABORT == wError )
      return( wError );

   /* Skip this message if the size is zero.
    */
   if( 0 == pThItem->dwSize )
      return( wError );

   /* Allocate or resize our text buffer to accommodate the
    * text of this message. dwAllocSize is the text size plus
    * 132 bytes to accommodate a CIX compact header.
    */
   dwAllocSize = pThItem->dwSize + 132;
   cbHdr = 0;
   if( dwAllocSize > dwBufSize )
      {
      if( !fResizeMemory32( &hpBuf, dwAllocSize ) )
         return( AMDBERR_OUTOFMEMORY );
      dwBufSize = dwAllocSize;
      }
   
   /* Initialise the CIXHDR structure
    */
   memset( &cixhdr, 0, sizeof(HEADER) );
   strcpy( cixhdr.szAuthor, pThItem->szAuthor );
   strcpy( cixhdr.szTitle, pThItem->szTitle );
   
   /* Seek to the message position and read it.
    */
   Amfile_SetPosition( fhTxt, pThItem->dwOffset, ASEEK_BEGINNING );
   if( pThItem->dwSize != Amfile_Read32( fhTxt, hpBuf, pThItem->dwSize ) )
      {
      strcpy( hpBuf, "*** Message failed to import ***\r\n" );
      pThItem->dwSize = strlen( hpBuf );
      /*DiskReadError( hwnd, lpPathBuf, FALSE );
      wError = AMDBERR_DISKREADERROR; */
      }
   
   /* If still no error, continue.
    */
   if( wError == AMDBERR_NOERROR )
      {
      DWORD cbRealSize;
      DWORD dwMsgSize;
      char sz[ 160 ];
      PTL ptlDst;
      int nMatch;
   
      /* NULL terminate the text.
       */
      hpBuf[ pThItem->dwSize ] = '\0';
   
      /* Unpack the date and time.
       */
      Amdate_UnpackDate( pThItem->wDate, &cixhdr.date );
      Amdate_UnpackTime( pThItem->wTime, &cixhdr.time );
   
      /* Write the header. Ameol didn't store the header line, so we have to
       * add it manually for each message.
       */
      switch( wType )
         {
         default:
            ASSERT( FALSE );
            break;
   
         case FTYPE_CIX:
         case FTYPE_LOCAL:
            cbRealSize = CountSize( hpBuf );
            cbHdr = wsprintf( sz, szCompactHdrTmpl,
               Amdb_GetFolderName( Amdb_FolderFromTopic( ptl ) ),
               Amdb_GetTopicName( ptl ),
               pThItem->dwMsg,
               pThItem->szAuthor,
               cbRealSize,
               cixhdr.date.iDay,
               GetMonthName( cixhdr.date.iMonth ),
               cixhdr.date.iYear,
               cixhdr.time.iHour,
               cixhdr.time.iMinute );
            if( pThItem->dwComment )
               cbHdr += wsprintf( sz + cbHdr, " c%lu*", pThItem->dwComment );
            cbHdr += wsprintf( sz + cbHdr, "\r\n", pThItem->dwComment );
            break;
   
         case FTYPE_MAIL:
            cbRealSize = CountSize( hpBuf );
            cbHdr = wsprintf( sz, "Memo #%lu (%lu)\r\n", pThItem->dwMsg, cbRealSize );
            break;
   
         case FTYPE_NEWS:
            cbRealSize = CountSize( hpBuf );
            cbHdr = wsprintf( sz, "#! rnews %lu\r\n", cbRealSize );
            break;
         }
   
      /* Insert the header into the top of the buffer.
       */
      hmemmove( hpBuf + cbHdr, hpBuf, pThItem->dwSize );
      hmemcpy( hpBuf, sz, cbHdr );
   
      /* Pass this message thru the filters.
       */
      dwMsgSize = pThItem->dwSize + cbHdr;
      hpBuf[ dwMsgSize ] = '\0';
      ptlDst = ptl;
      nMatch = FLT_MATCH_NONE;
      if( fApplyRules )
         nMatch = MatchRule( &ptlDst, hpBuf, &cixhdr, TRUE );
      if( !( nMatch & FLT_MATCH_DELETE ) )
         {
         PTH pth;
   
         /* Now create the message. If the destination topic is different
          * from the source topic, unlock it.
          */
         if( ptlDst != ptl )
            Amdb_LockTopic( ptlDst );
         wError = Amdb_CreateMsg( ptlDst, &pth, pThItem->dwMsg, pThItem->dwComment, pThItem->szTitle, pThItem->szAuthor, pThItem->szMessageId, &cixhdr.date, &cixhdr.time, hpBuf, dwMsgSize, 0L );
         if( wError != AMDBERR_NOERROR && wError != AMDBERR_IGNOREDERROR )
            return( wError );
         wError = AMDBERR_NOERROR;
         if( nMatch & FLT_MATCH_PRIORITY )
            Amdb_MarkMsgPriority( pth, TRUE, TRUE );
   
         /* Set the old flag attributes.
          */
         if( pThItem->wFlags & MF_KEEP )
            Amdb_MarkMsgKeep( pth, TRUE );
         if( pThItem->wFlags & MF_IGNORE )
            Amdb_MarkMsgIgnore( pth, TRUE, FALSE );
         if( pThItem->wFlags & MF_MARKED )
            Amdb_MarkMsg( pth, TRUE );
         if( pThItem->wFlags & MF_PRIORITY )
            Amdb_MarkMsgPriority( pth, TRUE, FALSE );
         if( pThItem->wFlags & MF_READ )
            Amdb_MarkMsgRead( pth, TRUE );
         if( pThItem->wFlags & MF_DELETED )
            Amdb_MarkMsgDelete( pth, TRUE );
         if( pThItem->wFlags & MF_WITHDRAWN )
            Amdb_MarkMsgWithdrawn( pth, TRUE );
         if( pThItem->wFlags & MF_READLOCK )
            Amdb_MarkMsgReadLock( pth, TRUE );
         if( pThItem->wFlags & MF_OUTBOXMSG )
            Amdb_SetOutboxMsgBit( pth, TRUE );
   
         /* Unlock the topic if we switched topics.
          */
         if( ptlDst != ptl )
            Amdb_InternalUnlockTopic( ptlDst );
         }
      }
   return( wError );
}

/* This function returns the name of the specified month, or
 * an error string if the month is invalid.
 */
char * FASTCALL GetMonthName( int iMonth )
{
   if( cixhdr.date.iMonth < 1 || cixhdr.date.iMonth > 12 )
      return( "???" );
   return( pszCixMonths[ cixhdr.date.iMonth - 1 ] );
}

/* This function handles the dialog box messages passed to the ExportDlgStatus
 * dialog.
 */
BOOL EXPORT CALLBACK ImportStatus( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = ImportStatus_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the ExportDlgStatus
 * dialog.
 */
LRESULT FASTCALL ImportStatus_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_COMMAND, ImportStatus_OnCommand );

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL ImportStatus_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDCANCEL:
         if( fMessageBox( hwnd, 0, GS(IDS_STR145), MB_YESNO|MB_ICONQUESTION ) == IDYES )
            wError = AMDBERR_USERABORT;
         break;
      }
}

/* Opens the Ameol registry and reads the specified string.
 */
#ifdef WIN32
int FASTCALL ReadAmeolRegistryString( LPSTR lpSection, LPSTR lpKey, LPCSTR lpDefBuf, LPSTR lpBuf, int cbBuf, LPSTR lpszIniFile )
{
   char szKey[ 9 ];
   char sz[ 100 ];
   HKEY hkResult;

   IniFileToLocalKey( lpszIniFile, szKey );
   wsprintf( sz, "SOFTWARE\\CIX\\%s\\%s", szKey, lpSection );
   if( RegOpenKeyEx( HKEY_CURRENT_USER, sz, 0L, KEY_READ, &hkResult ) == ERROR_SUCCESS )
      {
      DWORD dwType;
      DWORD dwszInt;

      dwszInt = (DWORD)cbBuf;
      if( RegQueryValueEx( hkResult, lpKey, 0L, &dwType, lpBuf, &dwszInt ) != ERROR_SUCCESS )
         lstrcpy( lpBuf, lpDefBuf );
      RegCloseKey( hkResult );
      }
   else
      lstrcpy( lpBuf, lpDefBuf );
   return( lstrlen( lpBuf ) );
}


/* Converts a INI filename path to an 8-character local key name.
 */
void FASTCALL IniFileToLocalKey( LPCSTR lpszIniFile, char * pszKey )
{
   register int c;
   LPCSTR p;

   for( c = 0, p = GetIniFileBasename( lpszIniFile ); *p && *p != '.' && c < 8; ++c, ++p )
      if( c == 0 )
         *pszKey++ = toupper(*p);
      else
         *pszKey++ = tolower(*p);
   *pszKey = '\0';
}

/* Returns a pointer to the basename of an INI file path.
 */
LPCSTR FASTCALL GetIniFileBasename( LPCSTR pszFile )
{
   LPCSTR pszFileT;

   pszFileT = pszFile;
   while( *pszFileT )
      {
      if( *pszFileT == ':' || *pszFileT == '\\' || *pszFileT == '/' )
         pszFile = pszFileT + 1;
      ++pszFileT;
      }
   return( pszFile );
}
#else
int FASTCALL ReadAmeolRegistryString( LPSTR lpSection, LPSTR lpKey, LPCSTR lpDefBuf, LPSTR lpBuf, int cbBuf, LPSTR lpszIniFile )
{
   return( GetPrivateProfileString( lpSection, lpKey, lpDefBuf, lpBuf, cbBuf, lpszIniFile ) );
}
#endif
