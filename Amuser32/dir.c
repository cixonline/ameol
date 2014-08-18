/* DIR.C - Directory interface functions
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

#define  THIS_FILE   __FILE__

#include "warnings.h"
#include <windows.h>
#include <direct.h>
#include <dos.h>
#include <io.h>
#include "amlib.h"
#include "amuser.h"
#include <ctype.h>
#include <string.h>

int FASTCALL MakeOneDirectory( LPCSTR );
int FASTCALL RemoveOneDirectory( LPCSTR );

/* This function determines whether the specified file exists.
 */
BOOL EXPORT WINAPI Amdir_QueryDirectory( LPCSTR lpszDirName )
{
#ifdef WIN32
   DWORD dwAttr;

   if( 0xFFFFFFFF == ( dwAttr = GetFileAttributes( lpszDirName ) ) )
      return( FALSE );
   return( dwAttr & FILE_ATTRIBUTE_DIRECTORY );
#else
   return( access( lpszDirName, 0 ) == 0 );
#endif
}

/* This function returns the drive letter of the
 * specified file.
 */
#ifdef WIN32
char EXPORT WINAPI Amdir_GetDrive( LPSTR lpszFileName )
{
   char sz[ 2 ];
   int chDrive;

   if( lpszFileName[ 0 ] && lpszFileName[ 1 ] && lpszFileName[ 1 ] == ':' )
      chDrive = lpszFileName[ 0 ] & 0xDF;
   else
      {
      GetCurrentDirectory( 2, (LPTSTR)&sz );
      chDrive = sz[ 0 ] & 0xDF;
      }
   return( (char)chDrive );
}
#else
char EXPORT WINAPI Amdir_GetDrive( LPSTR lpszFileName )
{
   int chDrive;

   if( lpszFileName[ 0 ] && lpszFileName[ 1 ] && lpszFileName[ 1 ] == ':' )
      chDrive = lpszFileName[ 0 ] & 0xDF;
   else {
      _dos_getdrive( &chDrive );
      chDrive += 'A';
      }
   return( (char)chDrive );
}
#endif

/* This function sets the default drive
 */
#ifdef WIN32
BOOL EXPORT WINAPI Amdir_SetCurrentDrive( char chDrive )
{
   char sz[ 3 ];

   wsprintf( sz, "%c:", chDrive );
   return( SetCurrentDirectory( sz ) );
}
#else
BOOL EXPORT WINAPI Amdir_SetCurrentDrive( char chDrive )
{
   unsigned cDrives;
   unsigned chCurDrive;

   _dos_setdrive( (unsigned)( toupper( chDrive ) - '@' ), &cDrives );
   _dos_getdrive( &chCurDrive );
   return( chDrive == (char)chCurDrive + '@' );
}
#endif

/* This function sets the current directory.
 */
#ifdef WIN32
BOOL EXPORT WINAPI Amdir_SetCurrentDirectory( char *pszDir )
{
   if( strlen( pszDir ) > 2 && pszDir[ 1 ] == ':' )
      if( !Amdir_SetCurrentDrive( pszDir[ 0 ] ) )
         return( FALSE );
   return( SetCurrentDirectory( pszDir ) );
}
#else
BOOL EXPORT WINAPI Amdir_SetCurrentDirectory( char *pszDir )
{
   if( strlen( pszDir ) > 2 && pszDir[ 1 ] == ':' )
      if( !Amdir_SetCurrentDrive( pszDir[ 0 ] ) )
         return( FALSE );
   return( _chdir( pszDir ) == 0 );
}
#endif

/* This function returns the free disk space.
 */
#ifdef WIN32
DWORD EXPORT WINAPI Amdir_FreeDiskSpace( char * lpszDir )
{
   DWORD SectorsPerCluster;
   DWORD BytesPerSector;
   DWORD FreeClusters;
   DWORD TotalClusters;
   char szRoot[ 64 ];

   /* Extract the root drive. This may be of the form
    * <drive>: or <machine>\\<share>
    */
   Amdir_ExtractRoot( lpszDir, szRoot, sizeof(szRoot) );
   GetDiskFreeSpace( szRoot, &SectorsPerCluster, &BytesPerSector, &FreeClusters, &TotalClusters );
   return( FreeClusters * SectorsPerCluster * BytesPerSector );
}
#else
DWORD EXPORT WINAPI Amdir_FreeDiskSpace( char * lpszDir )
{
   struct _diskfree_t df;
   int wDrive;

   wDrive = Amdir_GetDrive( lpszDir );
   _dos_getdiskfree( wDrive, &df );
   return( (DWORD)df.avail_clusters * (DWORD)df.sectors_per_cluster * (DWORD)df.bytes_per_sector );
}
#endif

/* Given a filename, this function retrieves the root
 * directory.
 */
void EXPORT WINAPI Amdir_ExtractRoot( char * lpszDir, char * pszRoot, int cbMax )
{
   int cSlashCount;

   cSlashCount = 1;
   while( *lpszDir )
      if( *lpszDir == ':' && cbMax > 2 )
         {
         *pszRoot++ = *lpszDir++;
         *pszRoot++ = '\\';
         cbMax -= 2;
         break;
         }
      else if( *lpszDir == '\\' && *(lpszDir+1) == '\\' && cbMax > 2 )
         {
         *pszRoot++ = *lpszDir++;
         *pszRoot++ = *lpszDir++;
         cSlashCount = 2;
         cbMax -= 2;
         }
      else if( *lpszDir == '\\' && 0 == --cSlashCount && cbMax > 1 )
         {
         *pszRoot++ = '\\';
         --cbMax;
         break;
         }
      else if( cbMax > 1 )
         {
         *pszRoot++ = *lpszDir++;
         --cbMax;
         }
   *pszRoot = '\0';
}

/* This function creates a new directory. The specified directory
 * may specify multiple paths to create.
 */
BOOL EXPORT WINAPI Amdir_Create( LPCSTR npszDir )
{
   LPSTR npszDirA;
   register int c;

   INITIALISE_PTR(npszDirA);
   if( fNewMemory( &npszDirA, strlen(npszDir) + 1 ) )
      {
      strcpy( npszDirA, npszDir );
      for( c = 0; npszDirA[ c ]; ++c )
         if( npszDirA[ c ] == '\\' || npszDirA[ c ] == '/' && ( c > 0 && npszDirA[ c - 1 ] != ':' ) )
            {
            char chSav;

            chSav = npszDirA[ c ];
            npszDirA[ c ] = '\0';
            MakeOneDirectory( npszDirA );
            npszDirA[ c ] = chSav;
            }
      FreeMemory( &npszDirA );
      }
   return( MakeOneDirectory( npszDir ) );
}

/* Gets the absolute path to the current directory
 * Added by YH 18/04/96
 */
DWORD EXPORT WINAPI Amdir_GetCurrentDirectory(LPSTR lpszBuffer, DWORD dwMaxLen)
{
   DWORD    dwRet;

#ifdef WIN32
   dwRet = GetCurrentDirectory( dwMaxLen, lpszBuffer );
#else
   if( dwMaxLen > 0xFFFF )
      dwRet = 0;
   else
      {
      if( _getcwd( lpszBuffer, (int) dwMaxLen ) == NULL )
         dwRet = 0;
      else
         dwRet = strlen( lpszBuffer );
      }
#endif

   return dwRet;
}

/* This function creates a new directory.
 */
#ifdef WIN32
int FASTCALL MakeOneDirectory( LPCSTR pszDir )
{
   return( CreateDirectory( pszDir, NULL ) );
}
#else
#pragma warning( disable:4704 )
int FASTCALL MakeOneDirectory( LPCSTR pszDir )
{
   short retval;

   /* Needed because we store the
    * return value using a 16-bit register.
    */
   ASSERT( sizeof(retval) == 2 );
   _asm {
      push  ds
      lds   dx,pszDir
      mov   ax,7139h
      stc
      int   21h
      jnc   MKD1
      cmp   ax,7100h
      jne   MKD2
      mov   ah,39h
      int   21h
      jnc   MKD1
MKD2: mov   ax,0FFFFH
      jmp   MKD3
MKD1: xor   ax,ax
MKD3: pop   ds
      mov   retval,ax
      }
   return( (int)retval == 0 );
}
#pragma warning( default:4704 )
#endif

/* This function removes a directory. The specified directory
 * may specify multiple paths to remove.
 */
BOOL EXPORT WINAPI Amdir_Remove( LPCSTR npszDir )
{
   LPSTR npszDirA;
   register int c;

   INITIALISE_PTR(npszDirA);
   if( fNewMemory( &npszDirA, strlen(npszDir) + 1 ) )
      {
      strcpy( npszDirA, npszDir );
      for( c = 0; npszDirA[ c ]; ++c )
         if( npszDirA[ c ] == '\\' || npszDirA[ c ] == '/' && ( c > 0 && npszDirA[ c - 1 ] != ':' ) )
            {
            char chSav;

            chSav = npszDirA[ c ];
            npszDirA[ c ] = '\0';
            RemoveOneDirectory( npszDirA );
            npszDirA[ c ] = chSav;
            }
      FreeMemory( &npszDirA );
      }
   return( RemoveOneDirectory( npszDir ) == 0 );
}

/* This function removes a directory.
 */
#ifdef WIN32
int FASTCALL RemoveOneDirectory( LPCSTR pszDir )
{
   return( RemoveDirectory( pszDir ) );
}
#else
#pragma warning( disable:4704 )
int FASTCALL RemoveOneDirectory( LPCSTR pszDir )
{
   short retval;

   /* Needed because we store the
    * return value using a 16-bit register.
    */
   ASSERT( sizeof(retval) == 2 );
   _asm {
      push  ds
      lds   dx,pszDir
      mov   ax,713Ah
      stc
      int   21h
      jnc   MKD1
      cmp   ax,7100h
      jne   MKD2
      mov   ah,3Ah
      int   21h
      jnc   MKD1
MKD2: mov   ax,0FFFFH
      jmp   MKD3
MKD1: xor   ax,ax
MKD3: pop   ds
      mov   retval,ax
      }
   return( (int)retval );
}
#pragma warning( default:4704 )
#endif
