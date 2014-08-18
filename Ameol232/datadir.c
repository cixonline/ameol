/* DATADIR.C - Handles data directory I/O access
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
#include "resource.h"
#include <dos.h>
#include <io.h>
#include <string.h>
#include <sys\types.h>
#include <sys\stat.h>
#include <errno.h>
#include "amlib.h"

#define  THIS_FILE   __FILE__

static char * szDataSubDir[] = {
   "Parlist",
   "Flist",
   "Sig",
   NULL
   };

#define  MAX_DSD     (sizeof(szDataSubDir)/sizeof(szDataSubDir[0]))

static char * szDataSubDirExt[] = {
   "PAR",
   "FLS",
   "SIG",
   NULL
   };

/* This function checks the DSD (data subdirectories) and
 * creates them if they are not present. In addition, if it had
 * to create any DSD, it scans the data directory and moves any
 * specific files from the data directory into the appropriate DSD.
 */
BOOL FASTCALL CheckDSD( void )
{
   register int c;
   BOOL fMakeDir;

   fMakeDir = FALSE;
   for( c = 0; c < MAX_DSD; ++c )
      {
      struct _stat sbuf;
      register int n;

      if( szDataSubDir[ c ] )
         wsprintf( lpTmpBuf, "%s\\%s", (LPSTR)pszDataDir, (LPSTR)szDataSubDir[ c ] );
         if( ( n = _stat( lpTmpBuf, &sbuf ) ) == -1 || !( sbuf.st_mode & _S_IFDIR ) )
            {
            if( !Amdir_Create( lpTmpBuf ) )
               return( FALSE );
            fMakeDir = TRUE;
            }
      }
   return( TRUE );
}

/* This function allocates memory and stores a new directory path, first
 * deallocating the old one if necessary.
 */
char * FASTCALL CreateDirectoryPath( char * pszOldDir, char * pszDirectory )
{
   char * npDir;

   INITIALISE_PTR(npDir);
   if( fNewMemory( &npDir, strlen( pszDirectory ) + 1 ) )
      {
      if( pszOldDir )
         FreeMemory( &pszOldDir );
      strcpy( npDir, pszDirectory );
      }
   else
      npDir = pszOldDir;
   return( npDir );
}

void FASTCALL WriteDirectoryPath( int fnDirectory, char ** nppDirPath, HWND hwnd, int id, char * npDirName )
{
   char szDir[ _MAX_DIR ];
   char * npDirPath;
   int nLastPos;

   npDirPath = *nppDirPath;
   Edit_GetText( GetDlgItem( hwnd, id ), szDir, _MAX_DIR );
   nLastPos = strlen( szDir ) - 1;
   if( nLastPos > 2 && szDir[ nLastPos ] == '\\' )
      szDir[ nLastPos ] = '\0';
   Amuser_WritePPString( szDirects, npDirName, szDir );
   *nppDirPath = CreateDirectoryPath( npDirPath, szDir );
   Amuser_SetAppDirectory( fnDirectory, szDir );
}

/* This function ensures that the directory specified by npDir exists
 * and either creates it or displays an error if it could not be
 * created.
 */
BOOL FASTCALL ValidateChangedDir( HWND hwnd, char * pszDir )
{
   if( !QueryDirectoryExists( pszDir ) )
      {
      char sz[ 300 ];

      wsprintf( sz, GS( IDS_STR124 ), (LPSTR)pszDir );
      if( fMessageBox( hwnd, 0, sz, MB_YESNO|MB_ICONINFORMATION ) == IDNO )
         return( FALSE );
      if( !Amdir_Create( pszDir ) )
         {
         wsprintf( sz, GS( IDS_STR123 ), (LPSTR)pszDir );
         fMessageBox( hwnd, 0, sz, MB_OK|MB_ICONEXCLAMATION );
         return( FALSE );
         }
      return( TRUE );
      }
   return( TRUE );
}

/* This function checks whether the specified directory exists.
 */
BOOL FASTCALL QueryDirectoryExists( char * pszDir )
{
   int nLastPos;

   if( lstrlen( pszDir ) == 3 && pszDir[ 1 ] == ':' && pszDir[ 2 ] == '\\' )
      return( TRUE );
   nLastPos = strlen( pszDir ) - 1;
   if( nLastPos > 0 && pszDir[ nLastPos ] == '\\' )
      pszDir[ nLastPos ] = '\0';
   return( Amdir_QueryDirectory( pszDir ) );
}

/* This function opens a file of the specified type from the
 * Ameol data directory.
 */
HNDFILE WINAPI EXPORT Ameol2_OpenFile( LPCSTR lpszFilename, WORD wType, WORD wMode )
{
   char sz[ _MAX_PATH ];
   char * pszDataSubDir;

   if( wType == DSD_ROOT )
      wsprintf( sz, "%s%s", (LPSTR)pszAmeolDir, lpszFilename );  // VistaAlert
   else if( wType == DSD_RESUME )
      wsprintf( sz, "%s/%s", (LPSTR)pszResumesDir, lpszFilename );
   else
      {
      if( wType != DSD_CURRENT && NULL == szDataSubDir[ wType ] )
         return( HNDFILE_ERROR );
      pszDataSubDir = ( wType == DSD_CURRENT ) ? "." : szDataSubDir[ wType ];
      wsprintf( sz, "%s\\%s\\%s", (LPSTR)pszDataDir, (LPSTR)pszDataSubDir, lpszFilename );
      }
   return( Amfile_Open( sz, wMode ) );
}

/* This function creates a file of the specified type in the
 * Ameol data directory.
 */
HNDFILE WINAPI EXPORT Ameol2_CreateFile( LPCSTR lpszFilename, WORD wType, WORD wMode )
{
   char sz[ _MAX_PATH ];
   HNDFILE fh;
   char * pszDataSubDir;

   if( wType == DSD_ROOT )
      wsprintf( sz, "%s%s", (LPSTR)pszAmeolDir, lpszFilename ); // VistaAlert
   else if( wType == DSD_RESUME )
      wsprintf( sz, "%s/%s", (LPSTR)pszResumesDir, lpszFilename );
   else
      {
      if( wType != DSD_CURRENT && NULL == szDataSubDir[ wType ] )
         return( HNDFILE_ERROR );
      pszDataSubDir = ( wType == DSD_CURRENT ) ? "." : szDataSubDir[ wType ];
      wsprintf( sz, "%s\\%s\\%s", (LPSTR)pszDataDir, (LPSTR)pszDataSubDir, lpszFilename );
      }
   return( fh = Amfile_Create( sz, wMode ) );
}

BOOL WINAPI EXPORT Ameol2_Archive( LPCSTR lpszFileName, BOOL fDeleteOriginal )
{
   /* Only archive if the file exists.
    */
   if( Amfile_QueryFile( lpszFileName ) )
      {
      char szBaseName[ 9 ];
      register int c;
      char sz[ 144 ];
      char * npsz;

      /* Find the file basename and the extension.
       */
      npsz = GetFileBasename( (LPSTR)lpszFileName );
      for( c = 0; c < 8 && npsz[ c ] && npsz[ c ] != '.'; ++c )
         szBaseName[ c ] = npsz[ c ];
      szBaseName[ c ] = '\0';

      /* In the archive directory, shift up the current files
       * by renumbering their extensions.
       */
      for( c = nMaxToArchive; c > 1; --c )
         {
         BEGIN_PATH_BUF;
         wsprintf( sz, "%s\\%s.%3.3d", pszArchiveDir, szBaseName, c );
         wsprintf( lpPathBuf, "%s\\%s.%3.3d", pszArchiveDir, szBaseName, c - 1 );
         Amfile_Delete( sz );
         Amfile_Rename( lpPathBuf, sz );
         END_PATH_BUF;
         }

      /* Delete any 001 file. It will previousy have been renumbered
       * to 002.
       */
      wsprintf( sz, "%s\\%s.001", (LPSTR)pszArchiveDir, (LPSTR)szBaseName );
      Amfile_Delete( sz );

      /* If we are permitted to archive, archive the file by copying
       * or moving depending on the fDeleteOriginal flag.
       */
      if( 0 == nMaxToArchive )
         {
         if( fDeleteOriginal )
            Amfile_Delete( lpszFileName );
         }
      else
         {
         if( !fDeleteOriginal )
            {
            BOOL f;
      
            f = Amfile_Copy( (LPSTR)lpszFileName, sz );
            return( f == 0 );
            }
         if( !Amfile_Rename( lpszFileName, sz ) )
            {
            BOOL f;

            /* Could not rename (poss x-device rename), so
             * copy it instead.
             */
            if( 0 == ( f = Amfile_Copy( (LPSTR)lpszFileName, sz ) ) )
               Amfile_Delete( lpszFileName );
            return( f == 0 );
            }
         }
      }
   return( TRUE );
}

BOOL WINAPI EXPORT Ameol2_QueryFileExists( LPCSTR lpszFilename, WORD wType )
{
   char sz[ _MAX_PATH ];
   HNDFILE fh;
   char * pszDataSubDir;

   if( wType == DSD_ROOT )
      wsprintf( sz, "%s%s", (LPSTR)pszAmeolDir, lpszFilename ); // VistaAlert
   else if( wType == DSD_RESUME )
      wsprintf( sz, "%s/%s", (LPSTR)pszResumesDir, lpszFilename );
   else
      {
      if( wType != DSD_CURRENT && NULL == szDataSubDir[ wType ] )
         return( FALSE );
      pszDataSubDir = ( wType == DSD_CURRENT ) ? "." : szDataSubDir[ wType ];
      wsprintf( sz, "%s\\%s\\%s", (LPSTR)pszDataDir, (LPSTR)pszDataSubDir, lpszFilename );
      }
   if( ( fh = Amfile_Open( sz, AOF_READ ) ) == HNDFILE_ERROR )
      return( FALSE );
   Amfile_Close( fh );
   return( TRUE );
}

BOOL WINAPI EXPORT Ameol2_DeleteFile( LPCSTR lpszFilename, WORD wType )
{
   char sz[ _MAX_PATH ];
   char * pszDataSubDir;

   if( wType == DSD_ROOT )
      wsprintf( sz, "%s%s", (LPSTR)pszAmeolDir, lpszFilename ); // VistaAlert
   else if( wType == DSD_RESUME )
      wsprintf( sz, "%s/%s", (LPSTR)pszResumesDir, lpszFilename );
   else
      {
      if( wType != DSD_CURRENT && NULL == szDataSubDir[ wType ] )
         return( 0 );
      pszDataSubDir = ( wType == DSD_CURRENT ) ? "." : szDataSubDir[ wType ];
      wsprintf( sz, "%s\\%s\\%s", (LPSTR)pszDataDir, (LPSTR)pszDataSubDir, lpszFilename );
      }
   return( Amfile_Delete( sz ) != -1 );
}

BOOL WINAPI EXPORT Ameol2_RenameFile( LPCSTR lpszOldName, LPCSTR lpszNewName, WORD wType )
{
   char sz1[ _MAX_PATH ];
   char sz2[ _MAX_PATH ];
   char * pszDataSubDir;

   pszDataSubDir = ( wType == DSD_CURRENT ) ? "." : szDataSubDir[ wType ];
   wsprintf( sz1, "%s\\%s\\%s", (LPSTR)pszDataDir, (LPSTR)pszDataSubDir, lpszOldName );
   wsprintf( sz2, "%s\\%s\\%s", (LPSTR)pszDataDir, (LPSTR)pszDataSubDir, lpszNewName );
   return( Amfile_Rename( sz1, sz2 ) == 0 );
}

HFIND FASTCALL AmFindFirst( LPCSTR lpszFilename, WORD wType, UINT attrib, FINDDATA * fdp )
{
   char sz[ _MAX_PATH ];
   char * pszDataSubDir;

   if( wType != DSD_CURRENT && NULL == szDataSubDir[ wType ] )
      return( -1 );
   pszDataSubDir = ( wType == DSD_CURRENT ) ? "." : szDataSubDir[ wType ];
   wsprintf( sz, "%s\\%s\\%s", (LPSTR)pszDataDir, (LPSTR)pszDataSubDir, lpszFilename );
   return( Amuser_FindFirst( sz, attrib, fdp ) );
}
