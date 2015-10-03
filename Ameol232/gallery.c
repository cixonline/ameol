/* GALLERY.C - Gallery interface
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
#include <string.h>
#include <dos.h>
#include "amlib.h"
#include <io.h>
#include "lbf.h"
#include "itsybits.h"

extern BOOL fIsAmeol2Scratchpad;

static BOOL fDefDlgEx = FALSE;

#define  THIS_FILE   __FILE__

char  NEAR szGalleryWndClass[] = "amctl_gallerycls";
char  NEAR szGalleryWndName[] = "Gallery";
char  szMugshotIniFile[] = "mugshot.ini";

char  (*aszFilesToDelete)[ _MAX_PATH + 1];/* Array of strings (dynaically allocated) that */
                                          /* conatains the files set to be deleted if user */
                                          /* clicks O.K. YH 05/06/96 10:50                */

int   nFilesToDelete;                     /* Number of files set to be deleted.           */
int   nTotalFiles;                        /* Number of files in mugshots directory        */


#define  PRESHOW_DELAY              150

#define  IDT_MUGSHOT                0x7800
#define  IDT_PRESHOW_MUGSHOT        0x7801

static BOOL fRegistered = FALSE;       /* TRUE if we've registered the resumes window class */

HWND hwndGallery = NULL;               /* Musghot list window handle */
PICCTRLSTRUCT pcs;                     /* Mugshot bitmap */
BOOL fSelUpdating = FALSE;             /* TRUE if we're updating selection */
PTH pthSel = NULL;

LRESULT EXPORT CALLBACK GalleryWndProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL Gallery_OnCreate( HWND, LPCREATESTRUCT );
void FASTCALL Gallery_OnDestroy( HWND );
void FASTCALL Gallery_OnSize( HWND, UINT, int, int );
void FASTCALL Gallery_OnMove( HWND, int, int );
void FASTCALL Gallery_OnTimer( HWND, UINT );
void FASTCALL Gallery_ShowBitmap( HWND, LPPICCTRLSTRUCT );

BOOL EXPORT FAR PASCAL MugshotEdit( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL MugshotEdit_DlgProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL MugshotEdit_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL MugshotEdit_OnDestroy( HWND hwnd );
void FASTCALL MugshotEdit_OnCommand( HWND, int, HWND, UINT );
void FASTCALL MugshotEdit_ListSelChange( HWND, HWND, BOOL );
void FASTCALL MugshotEdit_DisplayPreview( HWND hwnd, LPCSTR lpszFile );
void FASTCALL MugshotEdit_AdjustAddState( HWND hwnd, LPCSTR lpszUser, LPCSTR lpszFile );
void FASTCALL MugshotEdit_AmendList( HWND hwnd, LPSTR szUsername, LPSTR szFilename );
void FASTCALL MugshotEdit_ApplyChanges( HWND hwnd );

BOOL EXPORT CALLBACK GalleryWndEvents( int, LPARAM, LPARAM );

void FASTCALL MyComboBox_Dir( HWND, int, char * );
void FASTCALL FillMugshotListbox( HWND );
void FASTCALL CmdShowMugshot( char * );

/* This function enables or disables the gallery
 * mugshot command.
 */
void FASTCALL EnableGallery( void )
{
   if( !fShowMugshot )
      {
      if( hwndGallery )
         DestroyWindow( hwndGallery );
      }
   else if( hwndGallery == NULL )
      {
      DWORD dwState;
      RECT rcDef;
      RECT rcMDI;
      RECT rc;

      /* Register the gallery list window class if we have
       * not already done so.
       */
      if( !fRegistered )
         {
         WNDCLASS wc;
   
         wc.style          = 0; //CS_HREDRAW | CS_VREDRAW;
         wc.lpfnWndProc    = GalleryWndProc;
         wc.hIcon          = NULL;
         wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
         wc.lpszMenuName   = NULL;
         wc.cbWndExtra     = 0;
         wc.cbClsExtra     = 0;
         wc.hbrBackground  = (HBRUSH)( COLOR_WINDOW + 1 );
         wc.lpszClassName  = szGalleryWndClass;
         wc.hInstance      = hInst;
         if( !RegisterClass( &wc ) )
            return;
         fRegistered = TRUE;
         }
   
      /* The default position of the gallery window.
       */
      GetClientRect( hwndMDIClient, &rc );
      CopyRect( &rcMDI, &rc );
      rc.left = rc.right - ( rc.right / 5 );
      rc.top = rc.bottom - ( rc.bottom / 3 );
      rc.right -= 10;
      rc.bottom -= 10;
      CopyRect( &rcDef, &rc );
   
      /* Get the actual window size, position and state.
       */
      Amuser_ReadWindowState( szGalleryWndName, &rc, &dwState );

      /* Make sure the mugshot stays visible.
       */
      GetClientRect( GetDesktopWindow(), &rcMDI );
      if( rc.left > rcMDI.right || rc.top > rcMDI.bottom || rc.right < rcMDI.left || rc.bottom < rcMDI.top )
         CopyRect( &rc, &rcDef );

      /* Create the gallery window. It will be invisible to
       * start with...
       */
      ASSERT( NULL == hwndGallery );
      hwndGallery = CreateWindow( szGalleryWndClass,        /* Class name */
                                  "",                       /* Caption */
                                  WS_THICKFRAME|IBS_HORZCAPTION|
                                  WS_POPUP|WS_BORDER,       /* Popup window */
                                  rc.left,                  /* Left edge */
                                  rc.top,                   /* Top edge */
                                  ( rc.right - rc.left ),   /* Width */
                                  ( rc.bottom - rc.top ),   /* Height */
                                  hwndFrame,                /* Parent */
                                  NULL,                     /* Window ID */
                                  hInst,                    /* Instance handle */
                                  0L );                     /* No parameter */

      /* If we got a window, reduce the caption size slightly.
       */
      if( NULL != hwndGallery )
         {
         int cyCaption;

         cyCaption = (GetSystemMetrics( SM_CYCAPTION ) / 3) * 2;
         ibSetCaptionSize( hwndGallery, cyCaption );
         }
      }
}

/* This function handles all gallery window events.
 */
BOOL EXPORT CALLBACK GalleryWndEvents( int wEvent, LPARAM lParam1, LPARAM lParam2 )
{
   switch( wEvent )
      {
      case AE_MSGCHANGE:
         /* Delay before showing mugshot
          */
         KillTimer( hwndGallery, IDT_PRESHOW_MUGSHOT );
         if( (LPARAM)-1 == lParam1 )
            ShowWindow( hwndGallery, SW_HIDE );
         else
            {
            LPWINDINFO lpWindInfo;

            lpWindInfo = GetBlock( hwndMsg );
            pthSel = lpWindInfo->lpMessage;
//          pthSel = (PTH)lParam1;
            SetTimer( hwndGallery, IDT_PRESHOW_MUGSHOT, PRESHOW_DELAY, NULL );
            }
         break;
      }
   return( TRUE );
}

/* This function shows the mugshot for the specified user.
 * It obtains the filename from the mugshot database and displays
 * the resulting BMP in a small caption window.
 */
void FASTCALL CmdShowMugshot( char * pszUsername )
{
   ASSERT( NULL != hwndGallery );

   /* Is there a mugshot for this user?
    * No? Hide the window.
    */
   if( NULL == ( pcs.hBitmap = Amuser_GetMugshot( pszUsername, &pcs.hPalette ) ) )
      {
      if( IsWindowVisible( hwndGallery ) )
         ShowWindow( hwndGallery, SW_HIDE );
      return;
      }

   /* Finally make the window visible if needed
    */
   if( !IsWindowVisible( hwndGallery ) )
      if( !IsIconic( hwndFrame ) )
         ShowWindow( hwndGallery, SW_SHOWNA );

   /* Now show the bitmap.
    */
   Gallery_ShowBitmap( hwndGallery, &pcs );

   /* Set the window caption.
    */
   SetWindowText( hwndGallery, pszUsername );
}

/* This function handles all messages for the gallery window.
 */
LRESULT EXPORT CALLBACK GalleryWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, Gallery_OnCreate );
      HANDLE_MSG( hwnd, WM_DESTROY, Gallery_OnDestroy );
      HANDLE_MSG( hwnd, WM_TIMER, Gallery_OnTimer );
      HANDLE_MSG( hwnd, WM_SIZE, Gallery_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, Gallery_OnMove );
      }
   return( ibDefWindowProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL Gallery_OnCreate( HWND hwnd, LPCREATESTRUCT lpCreateStruct )
{
   /* Trap the AE_MSGCHANGE event.
    */
   Amuser_RegisterEvent( AE_MSGCHANGE, GalleryWndEvents );

   /* Create a picture control.
    */
   return( NULL != CreateWindow( WC_PICCTRL, "", WS_VISIBLE|WS_CHILD|PCTL_STRETCH, 0, 0, 0, 0, hwnd, (HMENU)IDD_PIX, hInst, 0L ) );
}

/* This function handles the WM_DESTROY message.
 */
void FASTCALL Gallery_OnDestroy( HWND hwnd )
{
   Amuser_UnregisterEvent( AE_MSGCHANGE, GalleryWndEvents );
   hwndGallery = NULL;
}

/* This function handles the WM_TIMER message.
 */
void FASTCALL Gallery_OnTimer( HWND hwnd, UINT id )
{
   if( id == IDT_PRESHOW_MUGSHOT )
      {
      MSGINFO msginfo;

      /* The current message has changed, so update the
       * mugshot.
       */
      ASSERT( NULL != hwndGallery );
      Amdb_GetMsgInfo( pthSel, &msginfo );
      CmdShowMugshot( msginfo.szAuthor );
      KillTimer( hwnd, id );
      }
   if( id == IDT_MUGSHOT )
      ShowWindow( hwnd, SW_HIDE );
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL Gallery_OnMove( HWND hwnd, int x, int y )
{
   Amuser_WriteWindowState( szGalleryWndName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, ibDefWindowProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL Gallery_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   HWND hwndPic;

   /* Resize the picture control to fill the client
    * area.
    */
   hwndPic = GetDlgItem( hwnd, IDD_PIX );
   SetWindowPos( hwndPic, NULL, 0, 0, cx, cy, SWP_NOACTIVATE|SWP_NOZORDER );
   Amuser_WriteWindowState( szGalleryWndName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, ibDefWindowProc );
}

/* Show the appropriate bitmap in the dialog.
 */
void FASTCALL Gallery_ShowBitmap( HWND hwnd, LPPICCTRLSTRUCT lpcs )
{
   HWND hwndPic;

   /* Start the timer for this mugshot if the
    * latency is non-zero.
    */
   if( 0 != nMugshotLatency )
      SetTimer( hwnd, IDT_MUGSHOT, nMugshotLatency * 1000, NULL );
   else
      KillTimer( hwnd, IDT_MUGSHOT );

   /* Update the bitmap.
    */
   hwndPic = GetDlgItem( hwnd, IDD_PIX );
   PicCtrl_SetBitmapHandle( hwndPic, lpcs );
}

/* This function displays the mugshot editor dialog.
 */
void FASTCALL CmdMugshotEdit( HWND hwnd )
{
   Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_MUGSHOTEDIT), MugshotEdit, 0L );
}

/* This function handles the Edit Mugshot dialog box.
 */
BOOL EXPORT CALLBACK MugshotEdit( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = MugshotEdit_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the Admin
 *  dialog.
 */
LRESULT FASTCALL MugshotEdit_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, MugshotEdit_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, MugshotEdit_OnCommand );
      HANDLE_MSG( hwnd, WM_DESTROY, MugshotEdit_OnDestroy );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsMUGSHOTEDIT );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL MugshotEdit_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   int DlgWidthUnits;
   HWND hwndList;
   HWND hwndCombo;
   int count;
   RECT rc2;

   /* Set the listbox tab division.
    * BUGBUG: Doesn't work, so I've hardcoded the pixel
    * division to 138!
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   GetWindowRect( GetDlgItem( hwnd, IDD_FILENAMETAG ), &rc2 );
   ScreenToClient( hwndList, (LPPOINT)&rc2 );
   DlgWidthUnits = LOWORD(GetDialogBaseUnits());
   rc2.left = 138;
   rc2.left = ( rc2.left * 4 ) / DlgWidthUnits;
   ListBox_SetTabStops( hwndList, 1, &rc2.left );

   /* Fill the list box with the list of filenames
    * and their associated user names.
    */
   FillMugshotListbox( hwndList );

   /* Fill combobox with a list of files in the mugshot
    * directory.
    */
   hwndCombo = GetDlgItem( hwnd, IDD_FILENAME );
   wsprintf( lpPathBuf, "%s\\*.GIF", pszMugshotDir );
   MyComboBox_Dir( hwndCombo, DDL_ARCHIVE|DDL_READONLY|DDL_READWRITE, lpPathBuf );
   wsprintf( lpPathBuf, "%s\\*.BMP", pszMugshotDir );
   MyComboBox_Dir( hwndCombo, DDL_ARCHIVE|DDL_READONLY|DDL_READWRITE, lpPathBuf );

   /* Allocate an array for holding strings of filesnames which user has asked
    * to be deleted. If the user presses cancel at the end of this dialog, then
    * these are ignored. If he presses O.K. then all file names stored in this
    * array are deleted.
    */
   count = ComboBox_GetCount( hwndCombo );
   aszFilesToDelete = NULL;
   nTotalFiles = count;
   if( count > 0 )
      if( fNewMemory( &( (LPVOID)aszFilesToDelete ), sizeof( char[_MAX_PATH + 1] ) * count ) )
         nFilesToDelete = 0;
      else
         return( FALSE );

   /* Select the first name in the list.
    */
   count = ListBox_GetCount( hwndList );
   if( count > 0 )
      ListBox_SetCurSel( hwndList, 0 );
   EnableWindow( hwndList, count > 0 );
   EnableControl( hwnd, IDD_DELETE, count > 0 );
   EnableControl( hwnd, IDD_ADD, FALSE );
   EnableControl( hwnd, IDD_CHANGE, FALSE );
   EnableControl( hwnd, IDD_REMOVE, count > 0 );
   if( count > 0 )
      MugshotEdit_ListSelChange( hwnd, hwndList, TRUE );
   return( TRUE );
}

/* Tidy up before we go YH 05/06/96 11:15
 */
void FASTCALL MugshotEdit_OnDestroy( HWND hwnd )
{
   if( NULL != aszFilesToDelete )
      FreeMemory( &( (LPVOID)aszFilesToDelete ) );
   nFilesToDelete = 0;
}

/* This function fills the mugshot list box with files of the
 * specified type.
 */
void FASTCALL FillMugshotListbox( HWND hwndList )
{
   HNDFILE fh;

   /* First try to open the mugshot index file. If it cannot be
    * found, build it from all GIF and BMP files in the mugshot
    * directory.
    */
   EnsureMugshotIndex();

   /* Now open the file as an ASCII text file and fill the
    * listbox.
    */
   BEGIN_PATH_BUF;
   wsprintf( lpPathBuf, "%s\\%s", pszMugshotDir, szMugshotIniFile );
   if( HNDFILE_ERROR != ( fh = Amfile_Open( lpPathBuf, AOF_READ ) ) )
      {
      char szUsername[ 60 ];
      LPLBF lbf;

      lbf = Amlbf_Open( fh, AOF_READ );
      while( Amlbf_Read( lbf, szUsername, sizeof(szUsername), NULL, NULL, &fIsAmeol2Scratchpad ) )
         {
         char * pszFilename;

         /* Read each line and replace the '=' delimiter with
          * a tab before adding it to the listbox.
          */
         pszFilename = strchr( szUsername, '=' );
         if( pszFilename )
            {
            *pszFilename++ = '\t';
            if( LB_ERR == ListBox_AddString( hwndList, szUsername ) )
               break;
            }
         }
      Amlbf_Close( lbf );
      }
   END_PATH_BUF
}

/* This function checks that the mugshot index exists and builds
 * it otherwise.
 */
void FASTCALL EnsureMugshotIndex( void )
{
   HNDFILE fh;

   BEGIN_PATH_BUF
   wsprintf( lpPathBuf, "%s\\%s", pszMugshotDir, szMugshotIniFile );
   if( !Amfile_QueryFile( lpPathBuf ) )
      if( HNDFILE_ERROR != ( fh = Amfile_Create( lpPathBuf, 0 ) ) )
         {
         register int n;
         FINDDATA ft;
         HFIND r;

         Amfile_Write( fh, "[Index]\r\n", 9 );
         wsprintf( lpPathBuf, "%s\\*.*", pszMugshotDir );
         for( n = r = Amuser_FindFirst( lpPathBuf, _A_NORMAL, &ft ); n != -1; n = Amuser_FindNext( r, &ft ) )
            {
            char szUsername[ 20 ];
            char * pExt;

            /* Synthesize a username from the filename.
             */
            _strlwr( ft.name );
            pExt = strrchr( ft.name, '.' );
            if( NULL != pExt )
               if( strcmp( pExt, ".bmp" ) == 0 || strcmp( pExt, ".gif" ) == 0 )
                  {
                  register int c;
                  char sz[ 60 ];

                  for( c = 0; c < sizeof(szUsername) - 1 && ft.name[ c ] != '.' && ft.name[ c ]; ++c )
                     szUsername[ c ] = ft.name[ c ];
                  szUsername[ c ] = '\0';
                  wsprintf( sz, "%s=%s\r\n", szUsername, ft.name );
                  Amfile_Write( fh, sz, strlen(sz) );
                  }
            }
         Amuser_FindClose( r );
         Amfile_Close( fh );
         }
   END_PATH_BUF
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL MugshotEdit_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_CHANGE: {
         char  szFilename[ 20 ];
         char  szUsername[ 60 ];
         HWND  hwndList;
         HWND  hwndEdit;
         int   iLBSel;

         /* Extract the user name from the selected list box  item
          * YH 04/06/96 15:20
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         iLBSel = ListBox_GetCurSel( hwndList );
         ASSERT( iLBSel != LB_ERR );
         ListBox_GetText( hwndList, iLBSel, szUsername );
         *( strchr( szUsername, '\t' ) ) = '\0';
         
         /* Get the file name YH 04/06/96 15:25
          */
         hwndEdit = GetDlgItem( hwnd, IDD_FILENAME );
         ComboBox_GetText( hwndEdit, szFilename, sizeof(szFilename) );
         ASSERT( *szFilename );

         /* Make it isn't blank.
          */
         StripLeadingTrailingSpaces( szFilename );

         /* Make sure filename has no path.
          * BUGBUG: No error shown!!
          */
         if( NULL != strstr( szFilename, "\\/:" ) )
            break;

         /* Okay. Add this to the listbox and the mugshot config
          * file.
          */

         MugshotEdit_AmendList( hwnd, szUsername, szFilename );
         }
         break;

      case IDD_ADD: {
         char szUsername[ 40 ];
         char szFilename[ 20 ];
         HWND hwndEdit;

         /* Get the user name and filename. Both must be
          * non-empty.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_USERNAME );
         Edit_GetText( hwndEdit, szUsername, sizeof(szUsername) );
         hwndEdit = GetDlgItem( hwnd, IDD_FILENAME );
         ComboBox_GetText( hwndEdit, szFilename, sizeof(szFilename) );
         ASSERT( *szUsername );
         ASSERT( *szFilename );

         /* Make sure neither are blank.
          */
         StripLeadingTrailingSpaces( szUsername );
         StripLeadingTrailingSpaces( szFilename );

         /* Make sure filename has no path.
          * BUGBUG: No error shown!!
          */
         if( NULL != strstr( szFilename, "\\/:" ) )
            break;

         /* Okay. Add this to the listbox and the mugshot config
          * file.
          */

         MugshotEdit_AmendList( hwnd, szUsername, szFilename );

         break;
         }

      case IDD_REMOVE: {
         char szUsername[ 60 ];
         HWND hwndList;
         char * pszFilename;
         int index;

         /* Remove the selected user from the list of
          * mugshots.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         index = ListBox_GetCurSel( hwndList );
         ASSERT( LB_ERR != index );
         ListBox_GetText( hwndList, index, szUsername );
         pszFilename = strchr( szUsername, '\t' );
         ASSERT( NULL != pszFilename );
         *pszFilename++ = '\0';

         /* Ask for confirmation.
          */
         wsprintf( lpTmpBuf, GS(IDS_STR911), szUsername );
         if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONINFORMATION ) == IDYES )
            {
            int count;

            /* Next, delete this string from the listbox
             * and select the next one.
             */
            count = ListBox_DeleteString( hwndList, index );
            if( index >= count )
               index = count - 1;
            ListBox_SetCurSel( hwndList, index );
            EnableControl( hwnd, IDD_REMOVE, count > 0 );
            EnableControl( hwnd, IDD_DELETE, count > 0 );
            EnableWindow( hwndList, count > 0 );
            MugshotEdit_ListSelChange( hwnd, hwndList, TRUE );
            if( count > 0 )
               SetFocus( hwndList );
            }
         break;
         }

      case IDD_PAD1: {
         char  szUsername[ 20 ];
         HWND  hwndList;
         HWND  hwndEdit;
         int   index;

         /* Get the username typed so far.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         hwndEdit = GetDlgItem( hwnd, IDD_USERNAME );
         Edit_GetText( hwndEdit, szUsername, sizeof(szUsername) );

         /* Match it in the listbox.
          */
         index = ListBox_FindString( hwndList, -1, szUsername );
         if( LB_ERR != index )
            {
            ListBox_SetCurSel( hwndList, index );
            MugshotEdit_ListSelChange( hwnd, hwndList, FALSE );
            }

         /* Enable or disbale the Add button accordingly
          * YH 04/06/96 12:15
          */
         MugshotEdit_AdjustAddState( hwnd, szUsername, NULL );

         break;
         }

      case IDD_USERNAME:
         if( codeNotify == EN_CHANGE && !fSelUpdating )
            PostDlgCommand( hwnd, IDD_PAD1, 0L );
         break;

      case IDD_FILENAME: {
         HWND     hwndList;
         char     szUsername[ 20 ];
         char     szFileName[ 20 ];
         HWND     hwndEdit;
         char     szListBoxItem[ 60 ];
         LPCSTR   lpszFileInListBox;
         int      iLBSel;
         int      iCBSel;
         BOOL     fNotInList;

         /* Enable or disable the Change button depending on whether or not
          * the list box item selected contains the same file name as has been
          * chosen in the Filename combo box.
          * YH 04/06/96 12:55
          */

         fNotInList = FALSE;
         iCBSel = CB_ERR;
         if( codeNotify == CBN_SELCHANGE )
            {
            iCBSel = ComboBox_GetCurSel( hwndCtl );
            ComboBox_GetLBText( hwndCtl, iCBSel, szFileName );
            }
         else
            ComboBox_GetText( hwndCtl, szFileName, sizeof(szFileName) );

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         iLBSel = ListBox_GetCurSel( hwndList );
         if( iLBSel != LB_ERR )
            {
            ListBox_GetText( hwndList, iLBSel, szListBoxItem );
            lpszFileInListBox = strchr( szListBoxItem, '\t' ) + 1;
            fNotInList = _stricmp( szFileName, lpszFileInListBox );
         }

         EnableControl( hwnd, IDD_CHANGE, fNotInList && *szFileName );

         /* If user had just changed selection of file name, then display
          * that file *without* changing the item in the list box.
          * YH 03/06/96 17:30
          */
         if( codeNotify == CBN_SELCHANGE && iCBSel != CB_ERR )
            MugshotEdit_DisplayPreview( hwnd, szFileName );

         /* Enable or disbale the Add button accordingly
          * YH 04/06/96 12:15
          */
         hwndEdit = GetDlgItem( hwnd, IDD_USERNAME );
         Edit_GetText( hwndEdit, szUsername, sizeof(szUsername) );
         MugshotEdit_AdjustAddState( hwnd, szUsername, szFileName );
         }
         break;

      case IDD_DELETE: {
         char szUsername[ 60 ];
         HWND hwndList;
         HWND hwndCombo;
         char * pszFilename;
         int index;
         int iComboIndex;

         /* Get the filename.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         index = ListBox_GetCurSel( hwndList );
         ASSERT( LB_ERR != index );
         ListBox_GetText( hwndList, index, szUsername );
         pszFilename = strchr( szUsername, '\t' );
         ASSERT( NULL != pszFilename );
         *pszFilename++ = '\0';

         /* Ask for confirmation.
          */
         wsprintf( lpTmpBuf, GS(IDS_STR908), pszFilename );
         if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONINFORMATION ) == IDYES )
            {
            register int c;
            int count;

            /* Remove from the combo box YH 05/06/96 11:20
             */
            hwndCombo = GetDlgItem( hwnd, IDD_FILENAME );
            ASSERT( hwndCombo );
            iComboIndex = ComboBox_FindStringExact( hwndCombo, -1, pszFilename );
            ASSERT( iComboIndex != CB_ERR );
            ComboBox_DeleteString( hwndCombo, iComboIndex );

            /* First mark the file for deletion. YH 05/06/96 11:15
             */
            BEGIN_PATH_BUF
            wsprintf( lpPathBuf, "%s\\%s", pszMugshotDir, pszFilename );
            ASSERT( nFilesToDelete < nTotalFiles );
            strcpy( aszFilesToDelete[ nFilesToDelete++ ], lpPathBuf );
            END_PATH_BUF

            /* Now scan the entire listbox and remove all entries which
             * have that file as being assigned to a particular user.
             */
            count = ListBox_GetCount( hwndList );
            for( c = 0; c < count; ++c )
               {
               char szUsername2[ 60 ];
               char * pszFilename2;

               /* Compare filename and delete entry if they match
                * the one we just deleted.
                */
               ListBox_GetText( hwndList, c, szUsername2 );
               pszFilename2 = strchr( szUsername2, '\t' );
               ASSERT( NULL != pszFilename2 );
               *pszFilename2++ = '\0';
               if( _strcmpi( pszFilename2, pszFilename ) == 0 )
                  {
                  ListBox_DeleteString( hwndList, c );
                  --count;
                  }
               }

            /* Set the new selection.
             */
            if( index >= count )
               index = count - 1;
            ListBox_SetCurSel( hwndList, index );
            EnableControl( hwnd, IDD_REMOVE, count > 0 );
            EnableControl( hwnd, IDD_DELETE, count > 0 );
            EnableWindow( hwndList, count > 0 );
            MugshotEdit_ListSelChange( hwnd, hwndList, TRUE );
            if( count > 0 )
               SetFocus( hwndList );
            }
         break;
         }

      case IDD_LIST:
         if( codeNotify == LBN_SELCHANGE )
            MugshotEdit_ListSelChange( hwnd, hwndCtl, TRUE );

         /* Added YH 04/06/96 14:20
          */
         EnableControl( hwnd, IDD_ADD, FALSE );
         EnableControl( hwnd, IDD_CHANGE, FALSE );

         break;

      case IDOK:
         MugshotEdit_ApplyChanges( hwnd );

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* This function is called when the listbox selection changes.
 */
void FASTCALL MugshotEdit_ListSelChange( HWND hwnd, HWND hwndList, BOOL fUpdEditFields )
{
   int index;

   /* Get the username and filename.
    */
   index = ListBox_GetCurSel( hwndList );
   if( LB_ERR == index )
      {
      SetDlgItemText( hwnd, IDD_USERNAME, "" );
      SetDlgItemText( hwnd, IDD_FILENAME, "" );
      }
   else
      {
      PICCTRLSTRUCT pcs;
      char szUsername[ 60 ];
      char * pszFilename;
      char * pszExt;
      HWND hwndPic;
      HNDFILE fh;

      /* We're updating the selection.
       */
      fSelUpdating = TRUE;
      ListBox_GetText( hwndList, index, szUsername );
      pszFilename = strchr( szUsername, '\t' );
      ASSERT( NULL != pszFilename );
      *pszFilename++ = '\0';

      /* Selection changed, so repaint the
       * preview window.
       */
      pcs.hBitmap = NULL;

      /* Find the file extension.
       */
      if( NULL != ( pszExt = strrchr( pszFilename, '.' ) ) )
         ++pszExt;

      /* Clear the current bitmap.
       */
      hwndPic = GetDlgItem( hwnd, IDD_PREVIEW );
      PicCtrl_SetBitmapHandle( hwndPic, NULL );
         
      /* Try looking for the BMP file first.
       */
      BEGIN_PATH_BUF
      wsprintf( lpPathBuf, "%s\\%s", pszMugshotDir, pszFilename );
      if( pszExt && _strcmpi( pszExt, "bmp" ) == 0 )
         pcs.hBitmap = Amuser_LoadBitmapFromDisk( lpPathBuf, &pcs.hPalette );
      
      /* If not there, so try the GIF file.
       */
      if( NULL == pcs.hBitmap )
         if( pszExt && _strcmpi( pszExt, "gif" ) == 0 )
            pcs.hBitmap = Amuser_LoadGIFFromDisk( lpPathBuf, &pcs.hPalette );
      END_PATH_BUF

      /* If we've a picture, set the picture control to reflect
       * the bitmap.
       */
      if( NULL != pcs.hBitmap )
         PicCtrl_SetBitmapHandle( hwndPic, &pcs );
      else
         {
         SetWindowFont( hwndPic, hHelvB8Font, FALSE );
         PicCtrl_SetText( hwndPic, GS(IDS_STR910) );
         PicCtrl_SetBitmapHandle( hwndPic, NULL );
         }

      /* Also get the newly selected username and filename
       * and place in the edit fields.
       */
      if( fUpdEditFields )
         SetDlgItemText( hwnd, IDD_USERNAME, szUsername );
      SetDlgItemText( hwnd, IDD_FILENAME, pszFilename );

      /* Open file and get file size.
       */
      lpTmpBuf[ 0 ] = '\0';
      BEGIN_PATH_BUF
      wsprintf( lpPathBuf, "%s\\%s", pszMugshotDir, pszFilename );
      if( HNDFILE_ERROR != ( fh = Amfile_Open( lpPathBuf, AOF_READ ) ) )
         {
         DWORD dwSize;

         dwSize = Amfile_GetFileSize( fh );
         wsprintf( lpTmpBuf, GS(IDS_STR909), FormatThousands( dwSize, NULL ) );
         Amfile_Close( fh );
         }
      SetDlgItemText( hwnd, IDD_STATISTICS, lpTmpBuf );
      END_PATH_BUF

      /* We're done updating the selection.
       */
      fSelUpdating = FALSE;
      }
}

/* This function will display the preview of the bitmap with the file
 * name passed. YH 03/05/96 16:55
 */
void FASTCALL MugshotEdit_DisplayPreview( HWND hwnd, LPCSTR lpszFile )
{
   PICCTRLSTRUCT pcs;
   char * pszExt;
   HWND hwndPic;
   HNDFILE fh;

   /* Assume no bitmap.
    */
   pcs.hBitmap = NULL;

   /* Find the file extension.
    */
   if( NULL != ( pszExt = strrchr( lpszFile, '.' ) ) )
      ++pszExt;
      
   /* Try looking for the BMP file first.
    */
   BEGIN_PATH_BUF
   wsprintf( lpPathBuf, "%s\\%s", pszMugshotDir, lpszFile );
   if( pszExt && _strcmpi( pszExt, "bmp" ) == 0 )
      pcs.hBitmap = Amuser_LoadBitmapFromDisk( lpPathBuf, &pcs.hPalette );
   
   /* If not there, so try the GIF file.
    */
   if( NULL == pcs.hBitmap )
      if( pszExt && _strcmpi( pszExt, "gif" ) == 0 )
         pcs.hBitmap = Amuser_LoadGIFFromDisk( lpPathBuf, &pcs.hPalette );
   END_PATH_BUF

   /* If we've a picture, set the picture control to reflect
    * the bitmap.
    */
   hwndPic = GetDlgItem( hwnd, IDD_PREVIEW );
   if( NULL != pcs.hBitmap )
      PicCtrl_SetBitmapHandle( hwndPic, &pcs );
   else
      {
      SetWindowFont( hwndPic, hHelvB8Font, FALSE );
      PicCtrl_SetText( hwndPic, GS(IDS_STR910) );
      PicCtrl_SetBitmapHandle( hwndPic, NULL );
      }

   /* Open file and get file size.
    */
   lpTmpBuf[ 0 ] = '\0';
   BEGIN_PATH_BUF
   wsprintf( lpPathBuf, "%s\\%s", pszMugshotDir, lpszFile );
   if( HNDFILE_ERROR != ( fh = Amfile_Open( lpPathBuf, AOF_READ ) ) )
      {
      DWORD dwSize;

      dwSize = Amfile_GetFileSize( fh );
      wsprintf( lpTmpBuf, GS(IDS_STR909), FormatThousands( dwSize, NULL ) );
      Amfile_Close( fh );
      }
   SetDlgItemText( hwnd, IDD_STATISTICS, lpTmpBuf );
   END_PATH_BUF
}

/* This function will look at the username and filename fields, compare them
 * to what's in the list box, and enable or disable the Ad button accordingly
 * YH 04/06/96 12:10
 */
void FASTCALL MugshotEdit_AdjustAddState( HWND hwnd, LPCSTR lpszUser, LPCSTR lpszFile )
{
   int index;
   BOOL fNotInList;
   char szListItem[60];
   HWND hwndList;
   BOOL fFileValid;

   hwndList = GetDlgItem( hwnd, IDD_LIST );
   ASSERT( hwndList );
   index = ListBox_FindString( hwndList, -1, lpszUser );

   /* Find out if word typed in is in list
    * YH 03/06/06 18:30
    */
   fNotInList = TRUE;
   if( index != LB_ERR )
      {
      ListBox_GetText( hwndList, index, szListItem );
      *( strchr( szListItem, '\t' ) ) = '\0';
      fNotInList = _stricmp( szListItem, lpszUser );
      }

   /* The lpszFile can be NULL which means that this function should look
    * at the dialog item to determine whether there is a file name entered
    * YH 04/06/96 17:45
    */

   if( lpszFile )
      fFileValid = *lpszFile;
   else
      fFileValid = Edit_GetTextLength( GetDlgItem( hwnd, IDD_FILENAME ) ) > 0;

   /* If the username name typed in is not in the list box,
    * and the username and filenames are not null strings,
    * then enable the Add button.
    * YH 03/06/96 16:40
    */
   EnableControl( hwnd, IDD_ADD, fNotInList &&
         ComboBox_GetTextLength( GetDlgItem( hwnd, IDD_USERNAME ) ) > 0 &&
         fFileValid );
}                    

void FASTCALL MugshotEdit_AmendList( HWND hwnd, LPSTR szUsername, LPSTR szFilename )
{
   HWND hwndList;
   char sz[ 60 ];
   int index;

   hwndList = GetDlgItem( hwnd, IDD_LIST );
   wsprintf( sz, "%s\t", szUsername );
   if( LB_ERR != ( index = ListBox_FindString( hwndList, -1, sz ) ) )
      ListBox_DeleteString( hwndList, index );
   strcat( sz, szFilename );
   if( LB_ERR != ( index = ListBox_AddString( hwndList, sz ) ) )
      {
      ListBox_SetCurSel( hwndList, index );
      EnableControl( hwnd, IDD_REMOVE, TRUE );
      EnableControl( hwnd, IDD_DELETE, TRUE );
      EnableWindow( hwndList, TRUE );
      MugshotEdit_ListSelChange( hwnd, hwndList, TRUE );
      }
   SetFocus( hwndList );
}

/* This function handles the Apply button.
 */
void FASTCALL MugshotEdit_ApplyChanges( HWND hwnd )
{
   int i;
   int count;
   HWND hwndList;
   HNDFILE fh;
   char szEntry[ 60 ];

   /* Delete all files marked for deletion
    */
   for( i = 0; i < nFilesToDelete; i++ )
      Amfile_Delete( aszFilesToDelete[ i ] );

   /* Write the mugshots.ini file from scratch, using the list box.
    */
   BEGIN_PATH_BUF
   wsprintf( lpPathBuf, "%s\\%s", pszMugshotDir, szMugshotIniFile );

   /* First delete exsting mugshot file
    */
   Amfile_Delete( lpPathBuf );

   /* Now create another one and fill it with current settings in list box.
    */
   if( HNDFILE_ERROR != ( fh = Amfile_Create( lpPathBuf, 0 ) ) )
      {
      Amfile_Write( fh, "[Index]\r\n", 9 );
      hwndList = GetDlgItem( hwnd, IDD_LIST );
      count = ListBox_GetCount( hwndList );
      for( i = 0; i < count; i++ )
         {
         ListBox_GetText( hwndList, i , szEntry );
         *( strchr( szEntry , '\t' ) ) = '=';
         strcat( szEntry, "\r\n" );
         Amfile_Write( fh, szEntry, strlen( szEntry ) );
         }
      Amfile_Close( fh );
      }
   END_PATH_BUF
}

/* Replacement for ComboBox_Dir that deals with long filenames.
 */
void FASTCALL MyComboBox_Dir( HWND hwndCombo, int flags, char * pszPath )
{
   HFIND hf;
   HFIND hfN;
   FINDDATA fd;

   for( hf = hfN = Amuser_FindFirst( pszPath, _A_NORMAL, &fd ); hfN != -1; hfN = Amuser_FindNext( hf, &fd ) )
      if( fd.name[ 0 ] != '.' )
         ComboBox_AddString( hwndCombo, fd.name );
   Amuser_FindClose( hf );
}
