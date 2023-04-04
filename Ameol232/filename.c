/* FILENAME.C - Handles filename functions
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
#include <string.h>
#include "resource.h"
#include "amlib.h"
#include <ctype.h>
#include <sys/stat.h>

#define  THIS_FILE   __FILE__

static BOOL fDefDlgEx = FALSE;

BOOL EXPORT CALLBACK FileNameCvtDlg( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL FileNameCvtDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL FileNameCvtDlg_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL FileNameCvtDlg_DefProc( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL FileNameCvtDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );

static char szIllegalCixFilenameChrs[] = "$;&()|^<> \t";

BOOL FASTCALL ExtractPathAndBasename( OPENFILENAME *, char * );

/* Returns whether or not ch is a valid character in a DOS filename.
 * Knows nothing about Windows/NT filenames, though.
 */
BOOL FASTCALL ValidFileNameChr( char ch )

{
   if( isalnum( ch ) || ch == '_' || ch == '^' || ch == '$' || ch == '~'
      || ch == '!' || ch == '#' || ch == '%' || ch == '&' || ch == '-'
      || ch == '{' || ch == '}' || ch == '(' || ch == ')' || ch == '@'
      || ch == '\'' || ch == '`' || ch == '+')
      return( TRUE );
   return( FALSE );
}

/* This function scans the specified filename and wraps any
 * portion of the path which contains spaces in quotes.
 */
void FASTCALL QuotifyFilename( char * pFilename )
{
   char * pPathComp;
   BOOL fHasSpace;
   BOOL fInQuote;

   pPathComp = pFilename;
   fHasSpace = FALSE;
   fInQuote = FALSE;
   while( TRUE )
      {
      if( *pFilename == '\\' || *pFilename == '\0' )
         {
         if( fHasSpace )
            {
            memmove( pPathComp + 1, pPathComp, strlen( pPathComp ) + 1 );
            *pPathComp = '"';
            pPathComp = ++pFilename;
            memmove( pPathComp + 1, pPathComp, strlen( pPathComp ) + 1 );
            *pPathComp = '"';
            }
         if( *pFilename == '\0' )
            break;
         pPathComp = pFilename + 1;
         fHasSpace = FALSE;
         }
      else if( *pFilename == ' ' && !fInQuote )
         fHasSpace = TRUE;
      else if( *pFilename == '"' )
         fInQuote = !fInQuote;
      ++pFilename;
      }
}

/* OLD This function extracts a filename from pSrc and copies it to
 * pDest. The filename may contain a path which, in turn, contains
 * spaces. If this is the case, the spaces must be quoted.
 */
char * FASTCALL OldExtractFilename( char * pDest, char * pSrc )
{
   BOOL fInQuote;

   fInQuote = FALSE;
   while( *pSrc == ' ' )
      ++pSrc;
   while( *pSrc )
      {
      if( *pSrc == ' ' && !fInQuote )
         break;
      if( *pSrc == '"' )
         fInQuote = !fInQuote;
      else if( NULL != pDest )
         *pDest++ = *pSrc;
      ++pSrc;
      }
   if( NULL != pDest )
      *pDest = '\0';
   while( *pSrc == ' ' )
      ++pSrc;


   return( pSrc );
}

/* This function extracts a filename from pSrc and copies it to
 * pDest. The filename may contain a path which, in turn, contains
 * spaces. If this is the case, the spaces must be quoted.
 */
char * FASTCALL ExtractFilename( char * pDest, char * pSrc )
{
   BOOL fInQuote;

   fInQuote = FALSE;
   if( NULL != strchr( pSrc, '"' ) )
   {
   while( *pSrc )
      {
      if( *pSrc == ' ' && !fInQuote )
         break;
      if( *pSrc == '"' )
         fInQuote = !fInQuote;
      else if( NULL != pDest )
         *pDest++ = *pSrc;
      ++pSrc;
      }
   if( NULL != pDest )
      *pDest = '\0';
   while( *pSrc == ' ' )
      ++pSrc;
   }
   else
   {
   while( *pSrc == ' ' )
      ++pSrc;
   while( *pSrc == '>' )
      ++pSrc;
   while( *pSrc == '<' )
      ++pSrc;
   while( *pSrc )
      {
      if( *pSrc == '>' )
         break;
      if( NULL != pDest )
         *pDest++ = *pSrc;
      ++pSrc;
      }
   if( NULL != pDest )
      *pDest = '\0';
   while( *pSrc == ' ' )
      ++pSrc;
   while( *pSrc == '<' )
      ++pSrc;
   while( *pSrc == '>' )
      ++pSrc;
   }
   return( pSrc );
}

/* This function checks whether the specified filename
 * contains wildcard characters.
 */
BOOL FASTCALL IsWild( char * pFilename )
{
   while( *pFilename )
      {
      if( *pFilename == '?' || *pFilename == '*' )
         return( TRUE );
      ++pFilename;
      }
   return( FALSE );
}

/* This function determines whether or not the specified
 * filename is valid for DOS.
 */
BOOL FASTCALL IsValidDOSFileName( LPSTR lpszFileName )
{
   OFSTRUCT of;

   if( OpenFile( lpszFileName, &of, OF_PARSE ) == -1 )
      return( FALSE );
   return( TRUE );
}

/* This function determines whether or not the specified
 * filename is valid for CIX.
 */
BOOL FASTCALL IsValidCIXFileName( LPSTR lpszFileName )
{
   return( strpbrk( GetFileBasename( lpszFileName ), szIllegalCixFilenameChrs ) == NULL );
}

/* This function converts a CIX filename, which may not be valid for
 * this OS, to a filename valid for the local system.
 */
BOOL WINAPI EXPORT Ameol2_MakeLocalFileName( HWND hwnd, LPSTR lpszCIXFileName, LPSTR lpszFileName, BOOL fPrompt )
{
   LPSTR lpNames[ 2 ];

   if( IsValidDOSFileName( lpszCIXFileName ) )
      lstrcpy( lpszFileName, lpszCIXFileName );
   else
      {
      Amuser_CreateCompatibleFilename( lpszCIXFileName, lpszFileName );
      if( fPrompt )
         {
         lpNames[ 0 ] = lpszCIXFileName;
         lpNames[ 1 ] = lpszFileName;
         return( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_FILENAMECVT), FileNameCvtDlg, (LPARAM)(LPSTR)lpNames ) );
         }
      }
   return( TRUE );
}

/* This function handles the Convert Filename dialog box.
 */
BOOL EXPORT CALLBACK FileNameCvtDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, FileNameCvtDlg_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the FileNameCvtDlg
 * dialog.
 */
LRESULT FASTCALL FileNameCvtDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, FileNameCvtDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, FileNameCvtDlg_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsFILENAMECVT );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL FileNameCvtDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPSTR FAR * lpNames;

   lpNames = (LPSTR FAR *)lParam;
   Edit_LimitText( GetDlgItem( hwnd, IDD_EDIT ), LEN_FILENAME );
   Edit_SetText( GetDlgItem( hwnd, IDD_OLDNAME ), lpNames[ 0 ] );
   Edit_SetText( GetDlgItem( hwnd, IDD_EDIT ), lpNames[ 1 ] );
   SetWindowLong( hwnd, DWL_USER, lParam );
   EnableControl( hwnd, IDOK, *lpNames[ 1 ] != '\0' );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL FileNameCvtDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EDIT:
         if( codeNotify == EN_UPDATE )
            EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndCtl ) > 0 );
         break;

      case IDOK: {
         LPSTR FAR * lpNames;

         lpNames = (LPSTR FAR *)GetWindowLong( hwnd, DWL_USER );
         GetDlgItemText( hwnd, IDD_EDIT, lpNames[ 1 ], LEN_FILENAME+1 );
         if( !IsValidDOSFileName( lpNames[ 1 ] ) )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR428), MB_OK|MB_ICONEXCLAMATION );
            break;
            }
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* This function returns a pointer to the file basename.
 */
LPSTR FASTCALL GetFileBasename( LPSTR npszFile )
{
   LPSTR npszFileT;

   npszFileT = npszFile;
   while( *npszFileT )
      {
      if( *npszFileT == ':' || *npszFileT == '\\' || *npszFileT == '/' )
         npszFile = npszFileT + 1;
      ++npszFileT;
      }
   return( npszFile );
}

/* Extracts the pathname portion of a filename
 */
void FASTCALL GetFilePathname( LPSTR pszFullPath, LPSTR pszPathBuf )
{
   LPSTR pszBasename;

   *pszPathBuf = '\0';
   pszBasename = GetFileBasename( pszFullPath );
   if( pszBasename > pszFullPath )
      {
      int diff;

      diff = pszBasename - pszFullPath;
      if( pszFullPath[ diff - 1 ] == '\\' )
         --diff;
      memcpy( pszPathBuf, pszFullPath, diff );
      pszPathBuf[ diff ] = '\0';
      }
}

/* This function replaces the file extension with the specified
 * extension.
 */
int FASTCALL ChangeExtension( LPSTR pszName, LPSTR pszExt )
{
   pszName = GetFileBasename( pszName );
   while( *pszName && *pszName != '.' )
      ++pszName;
   *pszName++ = '.';
   strcpy( pszName, pszExt );
   return( TRUE );
}



/* This function displays the common Open File dialog
 */
BOOL FASTCALL CommonGetOpenFilename( HWND hwnd, char * pszFilePath, char * pszTitle,
   char * pszFilters, char * pszCacheName )
{
   static char strFilters[] = "All Files (*.*)\0*.*\0\0";
   OPENFILENAME ofn;
   BOOL fOk;

   /* If supplied initial file and path is blank, look in the
    * cache for the last one used.
    */
   if( *pszFilePath == '\0' )
      Amuser_GetPPString( szSettings, pszCacheName, "", pszFilePath, _MAX_PATH );
   ofn.lStructSize = sizeof( OPENFILENAME );
   ofn.hwndOwner = hwnd;
   ofn.lpstrFilter = pszFilters ? pszFilters : strFilters;
   ofn.lpstrCustomFilter = NULL;
   ofn.nMaxCustFilter = 0;
   ofn.nFilterIndex = 1;
   ofn.nMaxFile = _MAX_FNAME;
   ofn.lpstrFileTitle = NULL;
   ofn.nMaxFileTitle = 0;
   ofn.lpstrTitle = pszTitle;
   ofn.Flags = OFN_NOCHANGEDIR|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
   ofn.nFileOffset = 0;
   ofn.nFileExtension = 0;
   ofn.lpstrDefExt = NULL;
   ofn.lCustData = 0;
   ofn.lpfnHook = NULL;
   ofn.lpTemplateName = NULL;
// ofn.hInstance = hInst;//NULL;
   ofn.hInstance = NULL;

   /* Split the supplied file and path into ofn.lpstrInitialDir and
    * ofn.lpstrFile.
    */
   ExtractPathAndBasename( &ofn, pszFilePath );



   /* Get the file. If OK, save the path to the cache.
    */
   if( fOk = GetOpenFileName( &ofn ) )
      {
      char * pszBasename;

      strcpy( pszFilePath, ofn.lpstrFile );
//    QuotifyFilename( pszFilePath );
      pszBasename = GetFileBasename( pszFilePath );
      if( pszBasename > pszFilePath )
         {
         int length;

         length = pszBasename - pszFilePath;
         memcpy( lpTmpBuf, pszFilePath, length );
         if( length > 1 && ( lpTmpBuf[ length - 1 ] == '\\' ) && ( lpTmpBuf[ length - 2 ] != ':' ) )
            --length;
         lpTmpBuf[ length ] = '\0';
         Amuser_WritePPString( szSettings, pszCacheName, lpTmpBuf );
         }
      }
   /* Deallocate buffers created by ExtractPathAndBasename.
    */
   if( ofn.lpstrInitialDir )
      FreeMemory( &(LPSTR)ofn.lpstrInitialDir );
   if( ofn.lpstrFile )
      FreeMemory( &(LPSTR)ofn.lpstrFile );
   return( fOk );
}

/* This function extracts the path and basename of a complete
 * filename and stores them in the OPENFILENAME structure.
 */
BOOL FASTCALL ExtractPathAndBasename( OPENFILENAME * pofn, char * pszFilename )
{
   char szFilename[ _MAX_PATH ];
   char * lpstrInitialDir;
   char * lpstrFile;
   struct _stat buf;
   BOOL fIsDirectory;
   BOOL fOk;

   INITIALISE_PTR(lpstrInitialDir);
   INITIALISE_PTR(lpstrFile );

   /* Check whether pszFilename points to a filename or
    * a directory.
    */
   fOk = FALSE;
   fIsDirectory = FALSE;
   szFilename[ 0 ] = '\0';
   if( _stat( pszFilename, &buf ) == 0 )
      if( buf.st_mode & _S_IFDIR )
         fIsDirectory = TRUE;
      else
         ExtractFilename( szFilename, pszFilename );

   /* Separate out the two.
    */
   if( fNewMemory( &lpstrInitialDir, _MAX_PATH ) )
      if( fNewMemory( &lpstrFile, _MAX_FNAME ) )
         {
         if( fIsDirectory )
            {
            strcpy( lpstrInitialDir, pszFilename );
            strcpy( lpstrFile, "" );
            }
         else
            {
            char * pszBasename;

            pszBasename = GetFileBasename( szFilename );
            if( pszBasename == szFilename )
               strcpy( lpstrInitialDir, pszUploadDir );
            else
               {
               memcpy( lpstrInitialDir, szFilename, pszBasename - szFilename );
               lpstrInitialDir[ pszBasename - szFilename ] = '\0';
               }
            strcpy( lpstrFile, pszBasename );
            }
         fOk = TRUE;
         }
   pofn->lpstrInitialDir = lpstrInitialDir;
   pofn->lpstrFile = lpstrFile;
   return( fOk );
}
