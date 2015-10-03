/* CHGDIR.C - Implements the Directory Browser common dialog
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

#include "warnings.h"
#include <windows.h>
#include <windowsx.h>
#include <assert.h>
#include "pdefs.h"
#include "amuser.h"
#include "amuserrc.h"
#include "amcntxt.h"
#include <htmlhelp.h>
#include <direct.h>
#include <io.h>
#include <stdlib.h>
#include "amuser.h"
#include <string.h>
#include <dos.h>
#include "multimon.h"

#undef _PRSHT_H_

#ifdef WIN32
#define USE_SHELL_BROWSER  1
#endif

#ifdef USE_SHELL_BROWSER
#include <shlobj.h>
#endif

#define  THIS_FILE   __FILE__

#ifndef WIN32
/* Private drive types not covered in Win3.x GetDriveType but in Win32 */
#define DRIVE_CDROM                 5
#define DRIVE_RAMDISK               6

/* We also need this export from Kernel, normally in windows.inc */
void FAR PASCAL DOS3Call(void);
#define  API   WINAPI
#include "winnet.h"
#else
#include <winnetwk.h>
#define  WNTYPE_DRIVE      RESOURCETYPE_DISK
#endif

#define ROP_DSPDxax  0x00E20746

WORD FAR PASCAL WNetGetCaps( WORD );

extern HINSTANCE hLibInst;
extern WORD wWinVer;

#define IDB_DRIVEMIN                DRIVE_REMOVABLE
#define IDB_DRIVEMAX                DRIVE_RAMDISK
#define IDB_FOLDERMIN               10
#define IDB_FOLDERMAX               12

static BOOL fDefDlgEx;
static int iCurDrive;
static BOOL fSetFirstSel;
static char szCurDir[ _MAX_PATH ];
static char szDefDir[ _MAX_PATH ];
static BOOL fBitmapsLoaded = FALSE;
static HBITMAP rghBmpDrives[ 5 ];
static HBITMAP rghBmpFolders[ 3 ];
BOOL fFileDebug;
BOOL EXPORT CALLBACK ChangeDir( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL ChangeDir_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL ChangeDir_OnCommand( HWND, int, HWND, UINT );
void FASTCALL ChangeDir_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
LRESULT FASTCALL ChangeDir_DefProc( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL ChangeDir_DlgProc( HWND, UINT, WPARAM, LPARAM );

void FASTCALL Amuser_SetDialogFont( HWND );
void FASTCALL CenterWindow( HWND );
void FASTCALL DriveListInitialize( HWND, HWND, UINT );
UINT FASTCALL AmDriveType( LPCSTR );
void FASTCALL DriveDirDrawItem( LPDRAWITEMSTRUCT, BOOL );
void FASTCALL AmTransparentBlt( HDC, UINT, UINT, HBITMAP, COLORREF );
void FASTCALL DirectoryListInitialize( HWND, HWND, LPSTR );
BOOL FASTCALL ChangeDirFromDirectory( HWND, char * );
void FASTCALL ChangeDir_LoadBitmaps( void );

#ifdef USE_SHELL_BROWSER
BOOL FASTCALL BrowseForFolder( LPCHGDIR );
int CALLBACK BrowseCallbackProc( HWND, UINT, LPARAM, LPARAM );
#endif

const char NEAR szHelpFile[] = "ameol2.hlp";

/* This function handles the ChangeDir dialog box.
 */
BOOL EXPORT WINAPI Amuser_ChangeDirectory( LPCHGDIR lpChgDir )
{
#ifdef USE_SHELL_BROWSER
   if( wWinVer >= 0x35F )
      return( BrowseForFolder( lpChgDir ) );
#endif
   return( Adm_Dialog( hLibInst, lpChgDir->hwnd, MAKEINTRESOURCE( IDDLG_CHANGEDIR ), ChangeDir, (LPARAM)lpChgDir ) );
}

/* This function handles the ChangeDir dialog box
 */
BOOL EXPORT CALLBACK ChangeDir( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = ChangeDir_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the Login
 * dialog.
 */
LRESULT FASTCALL ChangeDir_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, ChangeDir_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, ChangeDir_OnCommand );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, ChangeDir_OnMeasureItem );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsCHANGEDIR );
         break;

      case WM_DRAWITEM:
         DriveDirDrawItem( (LPDRAWITEMSTRUCT)lParam, IDD_DRIVELIST == wParam );
         return TRUE;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_MEASUREITEM message
 */
void FASTCALL ChangeDir_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   static int cyItem = -1;      /* Height of a listbox item */

   if( !fBitmapsLoaded )
      ChangeDir_LoadBitmaps();
   if( -1 == cyItem ) {
      HFONT hFont;
      HDC hDC;
      TEXTMETRIC tm;
      BITMAP bm;

      /* Attempt to get the font of the dialog. However,
       * on the first attempt in the life of the dialog,
       * this could fail; in that case use the system font.
       */
      hFont= GetWindowFont( hwnd );
      if( NULL == hFont )
         hFont = GetStockFont( SYSTEM_FONT );
      hDC = GetDC( hwnd );
      hFont = SelectFont( hDC, hFont );

      /* Item height is the maximum of the font height and the
       * bitmap height.  We know that all bitmaps are the same
       * size, so we just get information for one of them.
       */
      GetTextMetrics( hDC, &tm );
      GetObject( rghBmpFolders[0], sizeof( bm ), &bm );
      cyItem = max( bm.bmHeight, tm.tmHeight );
      ReleaseDC( hwnd, hDC );
      }
   lpMeasureItem->itemHeight = cyItem;
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL ChangeDir_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
#ifndef WIN32
   HINSTANCE hinstNetDriver;
#endif
   LPCHGDIR lpChgDir;
   int iDrive;

   /* Set the current directory value
    */
   lpChgDir = (LPCHGDIR)lParam;
   if( *lpChgDir->szPath )
      {
      Edit_SetText( GetDlgItem( hwnd, IDD_EDIT ), lpChgDir->szPath );
      lstrcpy( szCurDir, lpChgDir->szPath );
      if( szCurDir[ 1 ] == ':' )
         iDrive = ( toupper( *szCurDir ) - 'A' ) + 1;
      else
         iDrive = _getdrive();
      }
   else
      {
      Amdir_GetCurrentDirectory( szCurDir, _MAX_PATH );
      iDrive = _getdrive();
      }

   /* Show the Network button if we're running WorkGroups
    * or a similiar networking O/S.
    */
#ifdef WIN32
   ShowWindow( GetDlgItem( hwnd, IDD_NETWORK ), SW_SHOW );
#else
   hinstNetDriver = (HINSTANCE)WNetGetCaps( 0xFFFF );
   if( hinstNetDriver )
      {
      LPWNETSERVERBROWSEDIALOG lpDialogAPI;

      lpDialogAPI = (LPWNETSERVERBROWSEDIALOG)GetProcAddress( hinstNetDriver, (LPSTR)ORD_WNETSERVERBROWSEDIALOG);
      if( lpDialogAPI != NULL )
         ShowWindow( GetDlgItem( hwnd, IDD_NETWORK ), SW_SHOW );
      }
#endif

   /* Set window title
    */
   if( *lpChgDir->szTitle )
      SetWindowText( hwnd, lpChgDir->szTitle );

   /* Set the prompt title
    */
   if( *lpChgDir->szPrompt )
      SetWindowText( GetDlgItem( hwnd, IDD_PROMPT ), lpChgDir->szPrompt );

   /* Initialise the drive and directory lists
    */
   DriveListInitialize( GetDlgItem( hwnd, IDD_DRIVELIST ), GetDlgItem( hwnd, IDD_TEMPLIST ), iDrive );
   DirectoryListInitialize( GetDlgItem( hwnd, IDD_DIRECTORYLIST ), GetDlgItem( hwnd, IDD_TEMPLIST), szCurDir );
   AnsiLower( szCurDir );

   /* Remember the current drive and directory
    */
   Amdir_GetCurrentDirectory( szDefDir, _MAX_PATH );
   iCurDrive = _getdrive();
   if( iDrive != iCurDrive )
      _chdrive( iDrive );
   if( strcmp( szDefDir, szCurDir ) != 0 )
      _chdir( szCurDir );

   /* Disable OK if directory blank
    */
   EnableControl( hwnd, IDOK, Edit_GetTextLength( GetDlgItem( hwnd, IDD_EDIT ) ) > 0 );

   /* Remember pointer to directory
    */
   SetWindowLong( hwnd, DWL_USER, lParam );
   return( TRUE );
}

void FASTCALL ChangeDir_LoadBitmaps( void )
{
   register int i;

   for( i = IDB_DRIVEMIN; i <= IDB_DRIVEMAX; i++ )
      rghBmpDrives[ i - IDB_DRIVEMIN ] = LoadBitmap( hLibInst, MAKEINTRESOURCE( i ) );
   for( i = IDB_FOLDERMIN; i <= IDB_FOLDERMAX; i++ )
      rghBmpFolders[ i - IDB_FOLDERMIN ] = LoadBitmap( hLibInst, MAKEINTRESOURCE( i ) );
   fBitmapsLoaded = TRUE;
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL ChangeDir_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   static BOOL fListHasFocus = FALSE;

   switch( id )
      {
      case IDD_NETWORK:
         if( WNetConnectionDialog( hwnd, WNTYPE_DRIVE ) == WN_SUCCESS )
            {
#ifndef WIN32
            HINSTANCE hinstNetDriver;
            int iDrive;

            hinstNetDriver = (HINSTANCE)WNetGetCaps( 0xFFFF );
            if( hinstNetDriver )
               {
               LPWNETGETLASTCONNECTION lpDialogAPI;

               lpDialogAPI = (LPWNETGETLASTCONNECTION)GetProcAddress( hinstNetDriver, (LPSTR)ORD_WNETGETLASTCONNECTION);
               if( lpDialogAPI != NULL )
                  if( lpDialogAPI( WNTYPE_DRIVE, &iDrive ) == WN_SUCCESS )
                     {
                     ++iDrive;
                     DriveListInitialize( GetDlgItem( hwnd, IDD_DRIVELIST ), GetDlgItem( hwnd, IDD_TEMPLIST ), iDrive );
//                   SendDlgCommand( hwnd, IDD_DRIVELIST, CBN_SELCHANGE );
                     }
               }
#endif
            }
         break;

      case IDD_DIRECTORYLIST:
         if( codeNotify == LBN_SETFOCUS )
            fListHasFocus = TRUE;
         else if( codeNotify == LBN_KILLFOCUS )
            fListHasFocus = FALSE;
         else if( codeNotify == LBN_DBLCLK ) {
            UINT i;
            DWORD dw;
            char szDir[_MAX_PATH];
            char szCurDir[_MAX_PATH];
            LPSTR lpsz;

            /* On double-click, change directory and reinit the
             * listbox.  But all we stored in the string was
             * the directory's single name, so we use the bitmap
             * type to tell if we're below the current directory
             * (in which case we just chdir to our name) or above
             * (in which case we prepend "..\"'s as necessary.
             */
            i = ListBox_GetCurSel( hwndCtl );
            dw = ListBox_GetItemData( hwndCtl, i );

            /* If out bitmap is IDB_FOLDERCLOSED or the root,
             * then just .  If we're IDB_FOLDEROPENSELECT,
             * don't do anything.  If we're IDB_FOLDEROPEN then
             * we get the full current path and truncate it
             * after the directory to which we're switching.
             */
            if( IDB_FOLDEROPENSELECT == HIWORD( dw ) )
               {
               id = IDOK;
               goto CloseDialog;
               }

            /* Get get the directory for sub-directory changes. */
            ListBox_GetText( hwndCtl, i, szCurDir );
            if( IDB_FOLDEROPEN == HIWORD( dw ) && 0 != i )
               {
               /* Get the current path and find us in this path */
               GetWindowText( hwndCtl, szDir, sizeof( szDir ) );
               lpsz=_fstrstr( szDir, szCurDir );

               /* Null terminate right after us. */
               *( lpsz + strlen( szCurDir ) ) = '\0';

               /* Get this new directory in the right place */
               strcpy( szCurDir, szDir );
               }
            /* chdir has a nice way of validating for us. */
            if( 0 == _chdir( szCurDir ) )
               {
               /* Get the new full path. */
               Amdir_GetCurrentDirectory( szCurDir, _MAX_PATH );
               DirectoryListInitialize( hwndCtl, GetDlgItem( hwnd, IDD_TEMPLIST ), szCurDir );
               //???? YH18/04/96 AnsiLower( szCurDir );
               SetDlgItemText( hwnd, IDD_EDIT, szCurDir );
               }
            }
         break;

      case IDD_DRIVELIST:
         if( codeNotify == CBN_SELCHANGE )
            {
            UINT i, iCurDrive;
            char szCurDir[ _MAX_PATH ];
            char szDrive[ _MAX_PATH ]; /* Enough for drive:volume */

            /* Get the first letter in the current selection */
            i = ComboBox_GetCurSel( hwndCtl );
            ComboBox_GetLBText( hwndCtl, i, szDrive );
            iCurDrive = _getdrive();  /* Save in case of restore */

            /* Attempt to set the drive and get the current
             * directory on it.  Both must work for the change
             * to be certain.  If we are certain, reinitialize
             * the directories.  Note that we depend on drives
             * stored as lower case in the combobox.
             */
            if( _chdrive( (int)( szDrive[ 0 ] - 'a' + 1 ) ) == 0 && ( Amdir_GetCurrentDirectory( szCurDir, _MAX_PATH ) != 0 ) )
               {
               DirectoryListInitialize( GetDlgItem( hwnd, IDD_DIRECTORYLIST ), GetDlgItem( hwnd, IDD_TEMPLIST ), szCurDir );

               /* Insure that the root is visible (UI guideline) */
               SendDlgItemMessage( hwnd, IDD_DIRECTORYLIST, LB_SETTOPINDEX, 0, 0L );
               AnsiLower( szCurDir );
               SetDlgItemText( hwnd, IDD_EDIT, szCurDir );
               break;
               }

            /* Changing drives failed so restore drive and selection */
            _chdrive( (int)iCurDrive );
            wsprintf( szDrive, "%c:", (char)( iCurDrive + 'a' - 1 ) );
            ComboBox_SelectString( hwndCtl, -1, szDrive );
            }
         break;

      case IDD_EDIT:
         if( codeNotify == EN_UPDATE )
            EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndCtl ) > 0 );
         break;

      case IDOK: {
         LPCHGDIR lpChgDir;

         if( fListHasFocus )
            {
//          PostDlgCommand( hwnd, IDD_DIRECTORYLIST, LBN_DBLCLK );
            break;
            }
CloseDialog:
         lpChgDir = (LPCHGDIR)GetWindowLong( hwnd, DWL_USER );
         Edit_GetText( GetDlgItem( hwnd, IDD_EDIT ), lpChgDir->szPath, sizeof( lpChgDir->szPath ) );
         }

      case IDCANCEL: {
         register int i;

         for( i = IDB_DRIVEMIN; i <= IDB_DRIVEMAX; i++ )
            DeleteBitmap( rghBmpDrives[ i - IDB_DRIVEMIN ] );
         for( i = IDB_FOLDERMIN; i <= IDB_FOLDERMAX; i++ )
            DeleteBitmap( rghBmpFolders[ i - IDB_FOLDERMIN ] );
         fBitmapsLoaded = FALSE;
         _chdrive( iCurDrive );
         _chdir( szDefDir );
         EndDialog( hwnd, id == IDOK );
         break;
         }
      }
}

/* Resets and fills the given combobox with a list of current
 * drives.  The type of drive is stored with the item data.
 */
void FASTCALL DriveListInitialize(HWND hList, HWND hTempList, UINT iCurDrive )
{
   UINT i, iItem;
   UINT cch;
   UINT cItems;
   UINT iDrive, iType;
   char szDrive[ 10 ];
   char szNet[ 64 ];
   char szItem[ 68 ];

   if( NULL == hList )
      return;

   /* Clear out all the lists. */
   ComboBox_ResetContent( hList );
   ListBox_ResetContent( hTempList );

   /* Get available drive letters in the temp list */
   ListBox_Dir( hTempList, DDL_DRIVES | DDL_EXCLUSIVE, "*" );
   iCurDrive -= 1;       /* Fix for zero-based drive indexing */

   /* Walk through the list of drives, parsing off the "[-" and "-]"
    * For each drive letter parsed, add a string to the combobox
    * composed of the drive letter and the volume label.  We then
    * determine the drive type and add that information as the item data.
    */
   cItems = ListBox_GetCount( hTempList );
   for( i = 0; i < cItems; i++ )
      {
      ListBox_GetText( hTempList, i, szDrive );

      /* Ensure lowercase drive letters */
      AnsiLower(szDrive);
      iDrive = szDrive[ 2 ] - 'a';
      iType = AmDriveType( szDrive );        /* See Below */
      if( iType < 2 )                  /* Skip non-existent drive B's */
         continue;

      /* Start the item string with the drive letter, color, and two spaces */
      wsprintf( szItem, "%c%s", szDrive[ 2 ], (LPSTR)":  " );

      /* For fixed or ram disks, append the volume label which we find
       * using _dos_findfirst and attribute _A_VOLID.
       */
      if( DRIVE_FIXED == iType || DRIVE_RAMDISK == iType )
         {
      #ifdef WIN32
         wsprintf( szDrive, "%c:\\", szDrive[ 2 ] );
         GetVolumeInformation( szDrive, szItem + strlen( szItem ), 20, NULL, NULL, NULL, NULL, 0 );
      #else
         FINDDATA fi;
         HFIND hFind;
         char ch;

         wsprintf( szDrive, "%c:\\*.*", szDrive[ 2 ] );
         if( ( hFind = Amuser_FindFirst( szDrive, _A_VOLID, &fi ) ) != -1 )
            {
            /* Convert volume to lowercase and strip any '.' in the name. */
            AnsiLower( fi.name );

            /* If a period exists, it has to be in position 8, so clear it. */
            ch = fi.name[ 8 ];
            fi.name[ 8 ] = 0;
            strcat( szItem, fi.name );

            /* If we did have a broken volume name, append the last 3 chars */
            if( '.' == ch )
               strcat( szItem, &fi.name[ 9 ] );
            }
         Amuser_FindClose( hFind );
      #endif
         }

      /* For network drives, go grab the \\server\share for it. */
      if( DRIVE_REMOTE == iType )
         {
         szNet[ 0 ] = 0;
         cch = sizeof( szNet );

         wsprintf( szDrive, "%c:", szDrive[ 2 ] );
         AnsiUpper(szDrive);

//       if( WNetGetConnection( (LPSTR)szDrive, (LPSTR)szNet, &cch ) == NO_ERROR )
//          {
//          AnsiLower(szNet);
//          strcat( szItem, szNet );
//          }
         }
      iItem = ComboBox_AddString( hList, szItem );
      ComboBox_SetItemData( hList, iItem, MAKELONG( iDrive, iType ) );
      if( iDrive == iCurDrive )
         ComboBox_SetCurSel( hList, iItem );
      }
}

/* Augments the Windows API GetDriveType with a call to the CD-ROM
 * extensions to determine if a drive is a floppy, hard disk, CD-ROM,
 * RAM-drive, or networked  drive.
 */
#pragma optimize("",off)
UINT FASTCALL AmDriveType( LPCSTR lpszDrive )
{
#ifdef WIN32
   char sz[ 4 ];

   wsprintf( sz, "%c:\\", *( lpszDrive+2 ) );
   return( GetDriveType( sz ) );
#else
   int iType;
   BOOL fCDROM = FALSE;
   BOOL fRAM = FALSE;
   int iDrive;

   iDrive = lpszDrive[ 2 ] - 'a';
   iType = GetDriveType( iDrive );
    /*
     * Under Windows NT, GetDriveType returns complete information
     * not provided under Windows 3.x which we now get through other
     * means.
     */
   if( GetWinFlags() & 0x4000 )
      return( iType );

    /* Check for CDROM on FIXED and REMOTE drives only */
    if (DRIVE_FIXED==iType || DRIVE_REMOTE==iType)
        {
        _asm
            {
            mov     ax,1500h        /* Check if MSCDEX exists */
            xor     bx,bx
            int     2fh

            or      bx,bx           /* BX unchanged if MSCDEX is not around */
            jz      CheckRAMDrive   /* No?  Go check for RAM drive. */

            mov     ax,150Bh        /* Check if drive is using CD driver */
            mov     cx,iDrive
            int     2fh

            mov     fCDROM,ax       /* AX if the CD-ROM flag */
            or      ax,ax
            jnz     Exit            /* Leave if we found a CD-ROM drive. */

            CheckRAMDrive:
            }
        }

    /* Check for RAM drives on FIXED disks only. */
    if (DRIVE_FIXED==iType)
        {
        /*
         * Check for RAM drive is done by reading the boot sector and
         * looking at the number of FATs.  Ramdisks only have 1 while
         * all others have 2.
         */
        _asm
            {
            push    ds

            mov     bx,ss
            mov     ds,bx

            sub     sp,0200h            /* Reserve 512 bytes to read a sector */
            mov     bx,sp               /* and point BX there. */

            mov     ax,iDrive           /* Read the boot sector of the drive */
            mov     cx,1
            xor     dx,dx

            int     25h
            add     sp,2                /* Int 25h requires our stack cleanup. */
            jc      DriveNotRAM

            mov     bx,sp
            cmp     ss:[bx+15h],0f8h    /* Reverify fixed disk */
            jne     DriveNotRAM
            cmp     ss:[bx+10h],1       /* Check if there's only one FATs */
            jne     DriveNotRAM
            mov     fRAM,1

            DriveNotRAM:
            add     sp,0200h
            pop     ds

            Exit:
            /* Leave fRAM untouched  it's FALSE by default. */
            }
        }

    /*
     * If either CD-ROM or RAM drive flags are set, return privately
     * defined flags for them (outside of Win32).  Otherwise return
     * the type given from GetDriveType.
     */
       
       
    if (fCDROM)
        return DRIVE_CDROM;

    if (fRAM)
        return DRIVE_RAMDISK;

    /* Drive B on a one drive system returns < 2 from GetDriveType. */
    return iType;
#endif
}
#pragma optimize("",on)

/* Initializes strings in a listbox given a directory path.  The first
 * string in the listbox is the drive letter.  The remaining items are
 * each directory in the path completed with a listing of all sub-
 * directories under the last directory in the path.
 */
void FASTCALL DirectoryListInitialize( HWND hList, HWND hwndNotUsed, LPSTR lpszDir )
{
   LPSTR       lpsz;
   LPSTR       lpszLast;
   char        ch;
   char        szDir[ _MAX_DIR ];
   UINT        cch;
   UINT        i;
   UINT        iItem = 0;
   UINT        iIndent;
   BOOL        fFirst = TRUE;
   HFIND       hfFirst;
   HFIND       hfNext;
   FINDDATA    fd;

   fFirst = TRUE;

   if( NULL == hList || NULL == lpszDir )
      return;

   /* If the path ends in a \, strip the '\' */
   cch = lstrlen( lpszDir );
   if( '\\' == lpszDir[ cch - 1 ] )
      lpszDir[ cch - 1 ] = 0;

   /* Clear out all the lists. */
   SetWindowRedraw( hList, FALSE );
   ListBox_ResetContent( hList );

   /* Walk through the path one \ at a time.  At each one found,
    * we add the string composed of the characters between it and
    * the last \ found.  We also make sure everything is lower case.
    */
   lpszLast = lpszDir; //AnsiLower( lpszDir ); YH 19/04/96

   /* Save this for changing directories */
   SetWindowText( hList, lpszDir );

   /* Save the directory appended with \*.* */
   wsprintf( szDir, "%s\\*.*", lpszDir );
   while( TRUE )
      {
      ch = '\0';
      lpsz = _fstrchr( lpszLast, '\\' );
      if( NULL != lpsz )
         {
         /* Save the character here so we can NULL terminate.  If this
          * if the first entry, it's a drive root, so keep the \
          */
         if( fFirst )
            ch = *( ++lpsz );
         else
            ch = *lpsz;
         *lpsz = 0;
         }
      else
         {
         /* If we're looking at a drive only, then append a backslash */
         if( lpszLast == lpszDir && fFirst )
            lstrcat( lpszLast, "\\" );
         }

      /* Add the drive string--includes the last one where lpsz==NULL */
      iItem = ListBox_AddString( hList, lpszLast );

      /* The item data here has in the HIWORD the bitmap to use for
       * the item with the LOWORD containing the indentation.  The
       * bitmap is IDB_FOLDEROPEN for anything above the current
       * directory (that is, c:\foo is above than c:\foo\bar),
       * IDB_FOLDERCLOSED for anything below the current, and
       * IDB_FOLDEROPENSELECT for the current directory.
       */
      i = ( NULL != lpsz ) ? IDB_FOLDEROPEN : IDB_FOLDEROPENSELECT;
      ListBox_SetItemData( hList, iItem, MAKELONG( iItem, i ) );
      if( NULL == lpsz )
         break;

      /* Restore last character. */
      *lpsz = ch;
      lpsz += ( fFirst ) ? 0 : 1;
      fFirst=FALSE;
      lpszLast=lpsz;
      }

   /* Now that we have the path in, enumerate the subdirectories here
    * and place them in the list at the indentation iItem+1 since iItem
    * was the index of the last item in the path added to the list.
    *
    * To enumerate the directories, we send LB_DIR to an alternate
    * listbox.  On return, we have to parse off the brackets around
    * those directory names before bringing them into this listbox.
    *
    * NOT ANYMORE! See below
    */
   iIndent=iItem+1;

   /* Used to send LB_DIR but now uses FindFirst and FindNext because
    * we were having problems with long file names under W95.
    * YH 11/04/96.
    */

   for(  hfFirst = hfNext = Amuser_FindFirst( szDir, _A_SUBDIR, &fd );
         hfNext != -1;
         hfNext = Amuser_FindNext( hfFirst, &fd ))
      if( ( fd.attrib & _A_SUBDIR ) && fd.name[0] != '.' )
         {
         iItem = ListBox_AddString( hList, fd.name );
         ListBox_SetItemData( hList, iItem, MAKELONG(iIndent, IDB_FOLDERCLOSED) );
         }

   Amuser_FindClose( hfFirst );

   /* Force a listbox repaint. */
   SetWindowRedraw( hList, TRUE );
   InvalidateRect( hList, NULL, TRUE );

   /* If there's a vertical scrollbar, then we've added more items than
    * are visible at once.  To meet the UI specifications, we're supposed
    * to make the next directory up the top visible one.
    */
   GetScrollRange( hList, SB_VERT, (LPINT)&i, (LPINT)&iItem );
   if( !( 0 == i && 0 == iItem ) )
      ListBox_SetTopIndex( hList, max( (int)( iIndent - 2 ), 0 ) );

   /* Last thing is to set the current directory as the selection
    */
   ListBox_SetCurSel( hList, iIndent - 1 );
}

/* Handles WM_DRAWITEM for both drive and directory listboxes.
 */
void FASTCALL DriveDirDrawItem( LPDRAWITEMSTRUCT pDI, BOOL fDrive )
{
   char szItem[ 200 ];
   int iType = 0;
   int iIndent = 0;
   DWORD dw;
   BITMAP bm;
   COLORREF crText, crBack;
   HBITMAP hBmp;

   if( (int)pDI->itemID < 0 )
      return;
   dw = 0L;
   if( fDrive )
      dw = ComboBox_GetItemData( pDI->hwndItem, pDI->itemID );

   /* Get the text string for this item (controls have different messages)
    */
   if( fDrive )
      ComboBox_GetLBText( pDI->hwndItem, pDI->itemID, szItem );
   else
      ListBox_GetText( pDI->hwndItem, pDI->itemID, szItem );

   if( ( ODA_DRAWENTIRE|ODA_SELECT) & pDI->itemAction )
      {
      if( ODS_SELECTED & pDI->itemState )
         {
         crText = SetTextColor( pDI->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT) );
         crBack = SetBkColor( pDI->hDC, GetSysColor(COLOR_HIGHLIGHT) );
         }
      else
         {
         crText = SetTextColor( pDI->hDC, GetSysColor(COLOR_WINDOWTEXT) );
         crBack = SetBkColor( pDI->hDC, GetSysColor(COLOR_WINDOW) );
         }

      /* References to the two bitmap arrays here are the only external
       * dependencies of this code.  To keep it simple, we didn't use
       * a more complex scheme like putting all the images into one bitmap.
       */
      if( fDrive )
         {
         /* For drives, get the type, which determines the bitmap. */
         ASSERT( 0 != dw );
         iType = (int)HIWORD( dw );
         hBmp = rghBmpDrives[ iType - IDB_DRIVEMIN ];
         }
      else
         {
         /* For directories, indentation level is 4 pixels per indent. */
         iIndent = 4 * ( 1 + LOWORD( pDI->itemData ) );
         hBmp = rghBmpFolders[ HIWORD( pDI->itemData ) - IDB_FOLDERMIN ];
         }
      GetObject( hBmp, sizeof( bm ), &bm );

      /* Paint the text and the rectangle in whatever colors.  If
       * we're drawing drives, iIndent is zero so it's ineffective.
       */
      ExtTextOut( pDI->hDC, pDI->rcItem.left + bm.bmWidth + 4 + iIndent, pDI->rcItem.top, ETO_CLIPPED|ETO_OPAQUE,
                  &pDI->rcItem, szItem, strlen( szItem ), (LPINT)NULL );

      /* Go draw the bitmap we want. */
      AmTransparentBlt( pDI->hDC, pDI->rcItem.left + iIndent, pDI->rcItem.top, hBmp, RGB( 0, 0, 255 ) );

      /* Restore original colors if we changed them above. */
      if( ODS_SELECTED & pDI->itemState )
         {
         SetTextColor( pDI->hDC, crText );
         SetBkColor( pDI->hDC, crBack );
         }
      }

   if( ( ODA_FOCUS & pDI->itemAction ) || ( ODS_FOCUS & pDI->itemState ) )
      DrawFocusRect( pDI->hDC, &pDI->rcItem );
}

/* Given a DC, a bitmap, and a color to assume as transparent in that
 * bitmap, BitBlts the bitmap to the DC letting the existing background
 * show in place of the transparent color.
 */
void FASTCALL AmTransparentBlt(HDC hDC, UINT x, UINT y, HBITMAP hBmp, COLORREF cr)
{
   HDC         hDCSrc, hDCMid, hMemDC;
    HBITMAP     hBmpMono, hBmpT;
    HBRUSH      hBr, hBrT;
    COLORREF    crBack, crText;
    BITMAP      bm;

    if (NULL==hBmp)
        return;

    GetObject(hBmp, sizeof(bm), &bm);

    /* Get three intermediate DC's */
    hDCSrc=CreateCompatibleDC(hDC);
    hDCMid=CreateCompatibleDC(hDC);
    hMemDC=CreateCompatibleDC(hDC);

    SelectBitmap(hDCSrc, hBmp);

    /* Create a monochrome bitmap for masking */
    hBmpMono=CreateCompatibleBitmap(hDCMid, bm.bmWidth, bm.bmHeight);
    SelectBitmap(hDCMid, hBmpMono);

    /* Create a middle bitmap */
    hBmpT=CreateCompatibleBitmap(hDC, bm.bmWidth, bm.bmHeight);
    SelectBitmap(hMemDC, hBmpT);


    /* Create a monochrome mask where we have 0's in the image, 1's elsewhere. */
    crBack=SetBkColor(hDCSrc, cr);
    BitBlt(hDCMid, 0, 0, bm.bmWidth, bm.bmHeight, hDCSrc, 0, 0, SRCCOPY);
    SetBkColor(hDCSrc, crBack);

    /* Put the unmodified image in the temporary bitmap */
    BitBlt(hMemDC, 0, 0, bm.bmWidth, bm.bmHeight, hDCSrc, 0, 0, SRCCOPY);

    /* Create an select a brush of the background color */
    hBr=CreateSolidBrush(GetBkColor(hDC));
    hBrT=SelectBrush(hMemDC, hBr);

    /* Force conversion of the monochrome to stay black and white. */
    crText=SetTextColor(hMemDC, 0L);
    crBack=SetBkColor(hMemDC, RGB(255, 255, 255));

    /*
     * Where the monochrome mask is 1, Blt the brush; where the mono mask
     * is 0, leave the destination untouches.  This results in painting
     * around the image with the background brush.  We do this first
     * in the temporary bitmap, then put the whole thing to the screen.
     */
    BitBlt(hMemDC, 0, 0, bm.bmWidth, bm.bmHeight, hDCMid, 0, 0, ROP_DSPDxax);
    BitBlt(hDC,    x, y, bm.bmWidth, bm.bmHeight, hMemDC, 0, 0, SRCCOPY);


    SetTextColor(hMemDC, crText);
    SetBkColor(hMemDC, crBack);

    SelectBrush(hMemDC, hBrT);
    DeleteBrush(hBr);

    DeleteDC(hMemDC);
    DeleteDC(hDCSrc);
    DeleteDC(hDCMid);
    DeleteBitmap(hBmpT);
    DeleteBitmap(hBmpMono);

    return;
}

/* This function centers the specified window in the middle of the screen.
 */
void FASTCALL CenterWindow( HWND hWnd )
{
#ifdef NOUSEMM
   RECT rc;
   int x, y;

   GetWindowRect( hWnd, &rc );
   x = ( GetSystemMetrics( SM_CXSCREEN ) - ( rc.right - rc.left ) ) / 2;
   y = ( GetSystemMetrics( SM_CYSCREEN ) - ( rc.bottom - rc.top ) ) / 3;
   SetWindowPos( hWnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE );
#else NOUSEMM
   CenterWindowToMonitor(hWnd, hWnd, TRUE);
#endif NOUSEMM
}

/* This function implements a directory browser using Windows 95
 * shell functions.
 */
#ifdef USE_SHELL_BROWSER
BOOL FASTCALL BrowseForFolder( LPCHGDIR lpChgDir )
{
   HRESULT (WINAPI * fSHGetMalloc)(LPMALLOC *);
   HRESULT (WINAPI * fSHGetSpecialFolderLocation)(HWND, int, LPITEMIDLIST *);
   LPITEMIDLIST (WINAPI * fSHBrowseForFolder)( LPBROWSEINFO );
   BOOL (WINAPI * fSHGetPathFromIDList)(LPCITEMIDLIST, LPSTR );
   LPITEMIDLIST pidlPrograms;
   LPITEMIDLIST pidlBrowse;
   LPMALLOC lpMalloc;
   BOOL fUseOldIface;
   HINSTANCE hLib;
   BROWSEINFO bi; 
   HWND hwnd;

   hwnd = GetFocus();

   /* Load SHELL32.DLL
    */
   fUseOldIface = FALSE;
   if( NULL == ( hLib = LoadLibrary( "SHELL32.DLL" ) ) )
      fUseOldIface = TRUE;

   fFileDebug = Amuser_GetPPInt( "Settings", "FileDebug", FALSE );

   /* Get the exported function entrypoints.
    */
   (FARPROC)fSHGetMalloc = GetProcAddress( hLib, "SHGetMalloc" );
   (FARPROC)fSHGetSpecialFolderLocation = GetProcAddress( hLib, "SHGetSpecialFolderLocation" );
   (FARPROC)fSHBrowseForFolder = GetProcAddress( hLib, "SHBrowseForFolder" );
   (FARPROC)fSHGetPathFromIDList = GetProcAddress( hLib, "SHGetPathFromIDList" );

   /* If any of the functions are NULL, can't use the new IF
    */
   if( NULL == fSHGetMalloc || NULL == fSHGetSpecialFolderLocation ||
       NULL == fSHBrowseForFolder || NULL == fSHGetPathFromIDList )
      fUseOldIface = TRUE;

   /* If we must use the old interface...
    */
   if( fUseOldIface )
      {
      if( NULL != hLib )
         FreeLibrary( hLib );
      return( Adm_Dialog( hLibInst, lpChgDir->hwnd, MAKEINTRESOURCE( IDDLG_CHANGEDIR ), ChangeDir, (LPARAM)lpChgDir ) );
      }

   /* Get the Shell's malloc function.
    */
   if( !SUCCEEDED( fSHGetMalloc( &lpMalloc ) ) )
      {
      FreeLibrary( hLib );
      return( FALSE );
      }

   /* Save current path in a global for BrowseCallback
    */
   strcpy( szCurDir, lpChgDir->szPath );

   /* Get the PIDL for the Programs folder.
    */
   if( !SUCCEEDED( fSHGetSpecialFolderLocation( lpChgDir->hwnd, CSIDL_DESKTOP, &pidlPrograms ) ) )
      {
      FreeLibrary( hLib );
      return( FALSE );
      }

   /* Fill in the BROWSEINFO structure.
    */
   bi.hwndOwner = lpChgDir->hwnd;
   bi.pidlRoot = pidlPrograms;
   bi.pszDisplayName = lpChgDir->szPath;
   bi.lpszTitle = lpChgDir->szPrompt;
   bi.ulFlags = BIF_RETURNONLYFSDIRS;
   bi.lpfn = BrowseCallbackProc;
   bi.lParam = (LPARAM)lpChgDir;
   fSetFirstSel = TRUE;
 
   /* Browse for a folder and return its PIDL.
    */
   if( fFileDebug )
      MessageBox( hwnd, "About to browse for folder", "FileDebug", MB_OK|MB_ICONQUESTION );

   pidlBrowse = fSHBrowseForFolder( &bi );
   if( fFileDebug )
      MessageBox( hwnd, "Browsed folder", "FileDebug", MB_OK|MB_ICONQUESTION );
   if( NULL != pidlBrowse )
      {
      if( fFileDebug )
         MessageBox( hwnd, "Setting path..", "FileDebug", MB_OK|MB_ICONQUESTION );
      fSHGetPathFromIDList( pidlBrowse, lpChgDir->szPath );
      if( fFileDebug )
         MessageBox( hwnd, "Path set", "FileDebug", MB_OK|MB_ICONQUESTION );
      lpMalloc->lpVtbl->Free( lpMalloc, pidlBrowse );
      if( fFileDebug )
         MessageBox( hwnd, "Freed pidlBrowse", "FileDebug", MB_OK|MB_ICONQUESTION );

      }
   lpMalloc->lpVtbl->Free( lpMalloc, pidlPrograms );
   if( fFileDebug )
      MessageBox( hwnd, "Freed pidlPrograms", "FileDebug", MB_OK|MB_ICONQUESTION );
   FreeLibrary( hLib );
   if( fFileDebug )
      MessageBox( hwnd, "Freed hLib", "FileDebug", MB_OK|MB_ICONQUESTION );
   return( NULL != pidlBrowse );
}

/* This is the directory browser callback procedure.
 */
int CALLBACK BrowseCallbackProc( HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData )
{
   LPCHGDIR lpChgDir;

   if( uMsg == BFFM_SELCHANGED && fSetFirstSel )
      {
      lpChgDir = (LPCHGDIR)lpData;
      SendMessage( hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)(LPSTR)lpChgDir->szPath );
      fSetFirstSel = FALSE;
      }
   return( 0L );
}
#endif
