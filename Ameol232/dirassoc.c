/* DIRASSOC.C - Implements the Directory Association dialog
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
#include "amlib.h"
#include "resource.h"
#include <string.h>
#include "cookie.h"

#define  LEN_EXT     3

static BOOL fDefDlgEx = FALSE;   /* DefDlg recursion flag trap */

LRESULT FASTCALL DirAssociate_DlgProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL DirAssociate_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL DirAssociate_OnCommand( HWND, int, HWND, UINT );
void FASTCALL DirAssociate_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
void FASTCALL DirAssociate_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
LRESULT FASTCALL DirAssociate_OnNotify( HWND, int, LPNMHDR );

void FASTCALL DirAssociate_Add( HWND );
void FASTCALL SetAssociatedDir( char * );

/* This function handles the DirAssociate property sheet page
 */
BOOL EXPORT CALLBACK DirAssociate( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, DirAssociate_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, DirAssociate_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, DirAssociate_OnNotify );
      HANDLE_DLGMSG( hwnd, WM_DRAWITEM, DirAssociate_OnDrawItem );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL DirAssociate_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPSTR lpDirList;
   HWND hwndList;
   LPSTR lpDir;

   INITIALISE_PTR(lpDirList);

   hwndList = GetDlgItem( hwnd, IDD_LIST );
   if( fNewMemory( &lpDirList, 8000 ) )
      {
      Amuser_GetPPString( szExtensions, NULL, "", lpDirList, 8000 );
      lpDir = lpDirList;
      while( *lpDir )
         {
         char szDir[ _MAX_PATH+6 ];
         LPSTR lpDir2;

         lpDir2 = lpDir;
         if( *lpDir == '.' )
            ++lpDir;
         lstrcpy( szDir, lpDir );
         strcat( szDir, "\t" );
         Amuser_GetPPString( szExtensions, lpDir2, "", szDir + strlen( szDir ), _MAX_PATH+1 );
         AnsiUpper( szDir );
         ListBox_AddString( hwndList, szDir );
         lpDir += lstrlen( lpDir ) + 1;
         }
      FreeMemory( &lpDirList );
      }
   Edit_LimitText( GetDlgItem( hwnd, IDD_EXT ), LEN_EXT );
   Edit_LimitText( GetDlgItem( hwnd, IDD_DIRECTORY ), _MAX_PATH );
   EnableControl( hwnd, IDD_REMOVE, FALSE );
   EnableControl( hwnd, IDD_ADD, FALSE );

   /* Set the Browse picture button.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_BROWSE ), hInst, IDB_FOLDERBUTTON );
   return( TRUE );
}

/* This function handles the WM_MEASUREITEM message
 */
void FASTCALL DirAssociate_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   HFONT hOldFont;
   HDC hdc;

   hdc = GetDC( hwnd );
   hOldFont = SelectFont( hdc, hHelvB8Font );
   GetTextMetrics( hdc, &tm );
   lpMeasureItem->itemHeight = tm.tmHeight + 2;
   SelectFont( hdc, hOldFont );
   ReleaseDC( hwnd, hdc );
}

/* This function handles the WM_DRAWITEM message
 */
void FASTCALL DirAssociate_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   if( lpDrawItem->itemID != -1 )
      if( lpDrawItem->itemAction == ODA_FOCUS )
         DrawFocusRect( lpDrawItem->hDC, (LPRECT)&lpDrawItem->rcItem );
      else {
         HWND hwndList;
         HPEN hpen;
         HPEN hpenOld;
         COLORREF T, tmpT;
         COLORREF B, tmpB;
         HBRUSH hbr;
         RECT rc2;
         RECT rc;
         char * psz;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         ListBox_GetText( hwndList, lpDrawItem->itemID, lpTmpBuf );
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

         /* Split the name into type and settings field
          */
         psz = strchr( lpTmpBuf, '\t' );
         *psz++ = '\0';

         /* Erase the entire drawing area
          */
         FillRect( lpDrawItem->hDC, &rc, hbr );
         tmpT = SetTextColor( lpDrawItem->hDC, T );
         tmpB = SetBkColor( lpDrawItem->hDC, B );
         hpen = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNFACE ) );
         hpenOld = SelectPen( lpDrawItem->hDC, hpen );

         /* Get the left coordinate of the Settings edit box
          */
         GetWindowRect( GetDlgItem( hwnd, IDD_DIRECTORY ), &rc2 );
         ScreenToClient( hwndList, (LPPOINT)&rc2 );

         /* Draw the bottom separator
          */
         MoveToEx( lpDrawItem->hDC, rc.left, rc.bottom - 1, NULL );
         LineTo( lpDrawItem->hDC, rc.right, rc.bottom - 1 );

         /* Draw the name and setting and middle separator
          */
         rc.left += 2;
         ExtTextOut( lpDrawItem->hDC, rc.left, rc.top, 0, &rc, lpTmpBuf, strlen( lpTmpBuf ), NULL );
         rc.left = rc2.left - 2;
         MoveToEx( lpDrawItem->hDC, rc.left, rc.top, NULL );
         LineTo( lpDrawItem->hDC, rc.left, rc.bottom );
         rc.left += 2;
         FitPathString( lpDrawItem->hDC, psz, rc.right - rc.left );
         ExtTextOut( lpDrawItem->hDC, rc.left, rc.top, 0, &rc, psz, strlen( psz ), NULL );

         /* Clean up afterwards.
          */
         SelectPen( lpDrawItem->hDC, hpenOld );
         DeletePen( hpen );
         SetTextColor( lpDrawItem->hDC, tmpT );
         SetBkColor( lpDrawItem->hDC, tmpB );
         DeleteBrush( hbr );
         }
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL DirAssociate_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_BROWSE: {
         CHGDIR chgdir;
         HWND hwndEdit;
         char szExt[ LEN_EXT+1 ];

         hwndEdit = GetDlgItem( hwnd, IDD_DIRECTORY );
         Edit_GetText( hwndEdit, chgdir.szPath, 144 );
         GetDlgItemText( hwnd, IDD_EXT, szExt, sizeof( szExt ) );
         strcpy( chgdir.szTitle, GS(IDS_STR522) );
         if( *szExt )
            wsprintf( chgdir.szPrompt, GS(IDS_STR523), (LPSTR)szExt );
         else
            strcpy( chgdir.szPrompt, GS(IDS_STR524) );
         chgdir.hwnd = hwnd;
         if( Amuser_ChangeDirectory( &chgdir ) )
            Edit_SetText( hwndEdit, chgdir.szPath );
         SetFocus( hwndEdit );
         break;
         }

      case IDD_LIST:
         if( codeNotify == LBN_SELCHANGE )
            {
            HWND hwndList;
            int index;

LB1:        hwndList = GetDlgItem( hwnd, IDD_LIST );
            if( ( index = ListBox_GetCurSel( hwndList ) ) != LB_ERR )
               {
               char * psz;

               ListBox_GetText( hwndList, index, lpTmpBuf );
               psz = strchr( lpTmpBuf, '\t' );
               *psz++ = '\0';
               SetDlgItemText( hwnd, IDD_EXT, lpTmpBuf );
               SetDlgItemText( hwnd, IDD_DIRECTORY, psz );
               SetFocus( GetDlgItem( hwnd, IDD_EXT ) );
               EnableControl( hwnd, IDD_ADD, TRUE );
               EnableControl( hwnd, IDD_REMOVE, TRUE );
               }
            }
         break;

      case IDD_REMOVE: {
         HWND hwndList;
         int index;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         index = ListBox_GetCurSel( hwndList );
         if( ListBox_DeleteString( hwndList, index ) == index )
            --index;
         if( index != LB_ERR )
            {
            ListBox_SetCurSel( hwndList, index );
            goto LB1;
            }
         else
            {
            SetDlgItemText( hwnd, IDD_EXT, "" );
            SetDlgItemText( hwnd, IDD_DIRECTORY, "" );
            EnableControl( hwnd, IDD_REMOVE, FALSE );
            }
         break;
         }

      case IDD_ADD:
         DirAssociate_Add( hwnd );
         break;

      case IDD_EXT:
      case IDD_DIRECTORY:
         if( codeNotify == EN_UPDATE )
            EnableControl( hwnd, IDD_ADD,
               Edit_GetTextLength( GetDlgItem( hwnd, IDD_EXT ) ) > 0 &&
               Edit_GetTextLength( GetDlgItem( hwnd, IDD_DIRECTORY ) ) > 0 );
         break;
      }
}

/* This function adds any text in the extension and directory fields
 * to the listbox.
 */
void FASTCALL DirAssociate_Add( HWND hwnd )
{
   char chType[ LEN_EXT+1 ];
   char chValue[ _MAX_PATH+1 ];
   HWND hwndList;
   int index;

   GetDlgItemText( hwnd, IDD_EXT, chType, LEN_EXT+1 );
   GetDlgItemText( hwnd, IDD_DIRECTORY, chValue, _MAX_PATH+1 );
   AnsiUpper( chType );
   AnsiUpper( chValue );
   if( *chType && *chValue )
      {
      wsprintf( lpTmpBuf, "%s\t", (LPSTR)chType );
      hwndList = GetDlgItem( hwnd, IDD_LIST );
      if( ( index = ListBox_FindString( hwndList, -1, lpTmpBuf ) ) != LB_ERR )
         {
         strcat( lpTmpBuf, chValue );
         ListBox_DeleteString( hwndList, index );
         index = ListBox_AddString( hwndList, lpTmpBuf );
         }
      else
         {
         strcat( lpTmpBuf, chValue );
         index = ListBox_InsertString( hwndList, -1, lpTmpBuf );
         }
      ListBox_SetCurSel( hwndList, index );
      SetDlgItemText( hwnd, IDD_EXT, "" );
      SetDlgItemText( hwnd, IDD_DIRECTORY, "" );
      EnableControl( hwnd, IDD_ADD, FALSE );
      EnableControl( hwnd, IDD_REMOVE, TRUE );
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL DirAssociate_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPREF_DIRASSOC );
         break;

      case PSN_SETACTIVE:
         nLastOptionsDialogPage = ODU_DIRASSOCIATE;
         break;

      case PSN_APPLY: {
         register int c;
         HWND hwndList;

         /* Add any remaining input to the listbox.
          */
         DirAssociate_Add( hwnd );

         /* Set the configuration file from the listbox.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         Amuser_WritePPString( szExtensions, NULL, NULL );
         for( c = 0; c < ListBox_GetCount( hwndList ); ++c )
            {
            char sz[ _MAX_PATH+6 ];

            ListBox_GetText( hwndList, c, sz );
            SetAssociatedDir( sz );
            }

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* Given a filename, this function returns the download
 * directory associated with that filename.
 */
void FASTCALL GetAssociatedDir( PTL ptl, char * npDir, char * npFileName )
{
   char szDirectory[ _MAX_PATH ];

   /* Locate the file extension. Used to search forward,
    * now scans backwards.
    */
   npFileName = strrchr( npFileName, '.' );
   if( NULL != npFileName && *npFileName == '.' )
      ++npFileName;

   /* Locate the default file download path cookie for this
    * topic.
    */
   if( NULL != ptl )
      Amdb_GetCookie( ptl, FDL_PATH_COOKIE, szDirectory, pszDownloadDir, _MAX_PATH );
   else
      strcpy( szDirectory, pszDownloadDir );

   /* Look up the extensions.
    */
   Amuser_GetPPString( szExtensions, npFileName, szDirectory, npDir, _MAX_PATH );
}

/* This function sets up a directory association.
 */
void FASTCALL SetAssociatedDir( char * npAssc )
{
   char sz[ _MAX_PATH ];

   strcpy( sz, npAssc );
   npAssc = sz;
   while( *npAssc && *npAssc != '\t' )
      ++npAssc;
   if( *npAssc )
      *npAssc++ = '\0';
   Amuser_WritePPString( szExtensions, sz, npAssc );
}
