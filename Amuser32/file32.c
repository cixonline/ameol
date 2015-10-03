/* FILE32.C - Win32 file handling functions
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
#include "amlib.h"
#include "amuser.h"

/* This function checks whether or not file sharing is
 * loaded.
 */
BOOL EXPORT WINAPI Amfile_IsSharingLoaded( void )
{
   return( TRUE );
}

/* This function opens a file of the specified type from the
 * Ameol data directory.
 */
HNDFILE EXPORT WINAPI Amfile_Open( LPCSTR lpszFileName, int wMode )
{
   SECURITY_ATTRIBUTES sa;
   DWORD dwFlagsAndAttributes;
   DWORD dwDesiredAccess;
   DWORD dwShareMode;

   dwDesiredAccess = 0;
   dwShareMode = 0;
   dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
   if( wMode & AOF_READ )
      dwDesiredAccess |= GENERIC_READ;
   if( wMode & AOF_WRITE )
      dwDesiredAccess |= GENERIC_WRITE;
   if( wMode & AOF_SHARE_READ )
      {
      dwShareMode |= FILE_SHARE_READ;
      dwDesiredAccess |= GENERIC_READ;
      }
   if( wMode & AOF_SHARE_WRITE )
      {
      dwShareMode |= FILE_SHARE_WRITE;
      dwDesiredAccess |= GENERIC_WRITE;
      }
   if( wMode & AOF_SEQUENTIAL_IO )
      dwFlagsAndAttributes = FILE_FLAG_SEQUENTIAL_SCAN;
   sa.nLength = sizeof(SECURITY_ATTRIBUTES);
   sa.lpSecurityDescriptor = NULL;
   sa.bInheritHandle = FALSE;
   return( CreateFile( lpszFileName, dwDesiredAccess, dwShareMode, &sa, OPEN_EXISTING, dwFlagsAndAttributes, NULL ) );
}

/* This function creates a file of the specified type in the
 * Ameol data directory.
 */
HNDFILE EXPORT WINAPI Amfile_Create( LPCSTR lpszFileName, int wMode )
{
   SECURITY_ATTRIBUTES sa;
   DWORD dwDesiredAccess;
   DWORD dwShareMode;

   dwDesiredAccess = GENERIC_READ|GENERIC_WRITE;
   dwShareMode = FILE_SHARE_READ|FILE_SHARE_WRITE;
   sa.nLength = sizeof(SECURITY_ATTRIBUTES);
   sa.lpSecurityDescriptor = NULL;
   sa.bInheritHandle = FALSE;
   return( CreateFile( lpszFileName, dwDesiredAccess, dwShareMode, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL ) );
}

/* This function determines whether the specified file exists.
 */
BOOL EXPORT WINAPI Amfile_QueryFile( LPCSTR lpszFileName )
{
   DWORD dwAttr;
// DWORD dwLastError;

   if( 0xFFFFFFFF == ( dwAttr = GetFileAttributes( lpszFileName ) ) )
   {
/*    HNDFILE fhOut;
      static char szCurDir[ _MAX_PATH ];
      Amdir_GetCurrentDirectory( szCurDir, _MAX_PATH );

      if( ( fhOut = Amfile_Create( "zpwpwpw.pw", 0 ) ) == HNDFILE_ERROR )
         dwLastError = GetLastError();
      else
         Amfile_Close( fhOut );
      dwLastError = GetLastError();
*/    return( FALSE );
   }
   if( dwAttr & FILE_ATTRIBUTE_DIRECTORY )
      return( FALSE );
   return( TRUE );
}

/* This function reads data from the file. The data must be less than
 * 64K, despite the function being handle to handle more.
 */
UINT EXPORT WINAPI Amfile_Read( HNDFILE hfile, LPVOID lpBuf, UINT cbBuf )
{
   DWORD cbRead;

   return( ReadFile( hfile, lpBuf, cbBuf, &cbRead, NULL ) ? (UINT)cbRead : -1 );
}

/* This function reads data from the file.
 */
DWORD EXPORT WINAPI Amfile_Read32( HNDFILE hfile, LPVOID lpBuf, DWORD cbBuf )
{
   DWORD cbRead;

   return( ReadFile( hfile, lpBuf, cbBuf, &cbRead, NULL ) ? cbRead : -1 );
}

/* This function writes data to the file. The data must be less than
 * 64K, despite the function being handle to handle more.
 */
UINT EXPORT WINAPI Amfile_Write( HNDFILE hfile, LPCSTR lpBuf, UINT cbBuf )
{
   DWORD cbWritten;

   return( WriteFile( hfile, lpBuf, cbBuf, &cbWritten, NULL ) ? cbWritten : -1 );
}

/* This function writes data to the file.
 */
DWORD EXPORT WINAPI Amfile_Write32( HNDFILE hfile, LPCSTR lpBuf, DWORD cbBuf )
{
   DWORD cbWritten;

   return( WriteFile( hfile, lpBuf, cbBuf, &cbWritten, NULL ) ? cbWritten : -1 );
}

/* This function sets a file's position.
 */
LONG EXPORT WINAPI Amfile_SetPosition( HNDFILE hfile, LONG lSeekOffset, int nMethod )
{
   DWORD dwTravelScheme;

   if( nMethod == ASEEK_BEGINNING )
      dwTravelScheme = FILE_BEGIN;
   else if( nMethod == ASEEK_END )
      dwTravelScheme = FILE_END;
   else
      dwTravelScheme = FILE_CURRENT;
   return( SetFilePointer( hfile, lSeekOffset, NULL, dwTravelScheme ) );
}

/* This function gets a file's position.
 */
DWORD EXPORT WINAPI Amfile_GetPosition( HNDFILE hfile )
{
   return( SetFilePointer( hfile, 0, NULL, FILE_CURRENT ) );
}

/* This function sets a file's position.
 */
LONG EXPORT WINAPI Amfile_GetFileSize( HNDFILE hfile )
{
   DWORD dwHiSize;

   return( GetFileSize( hfile, &dwHiSize ) );
}

/* This function locks an area of the file for updating.
 */
BOOL EXPORT WINAPI Amfile_Lock( HNDFILE hfile, DWORD dwPos, DWORD dwSize )
{
   return( LockFile( hfile, dwPos, 0, dwSize, 0 ) );
}

/* This function unlocks an area of the file.
 */
BOOL EXPORT WINAPI Amfile_Unlock( HNDFILE hfile, DWORD dwPos, DWORD dwSize )
{
   return( UnlockFile( hfile, dwPos, 0, dwSize, 0 ) );
}

/* This function closes a file.
 */
void EXPORT WINAPI Amfile_Close( HNDFILE hfile )
{
   CloseHandle( hfile );
}

/* This function deletes the specified file.
 */
BOOL EXPORT WINAPI Amfile_Delete( LPCSTR lpszFileName )
{
   return( DeleteFile( lpszFileName ) );
}

/* This function renames the specified file.
 */
BOOL EXPORT WINAPI Amfile_Rename( LPCSTR lpszOldName, LPCSTR lpszNewName )
{
   return( MoveFile( lpszOldName, lpszNewName ) );
}

/* Given a file handle, returns the creation date of that file.
 */
void EXPORT WINAPI Amfile_GetFileTime( HNDFILE fh, WORD * puDate, WORD * puTime )
{
   SYSTEMTIME systemTime;
   FILETIME fileTime2;
   FILETIME fileTime;
   AM_DATE date;
   AM_TIME time;

   GetFileTime( fh, NULL, NULL, &fileTime );
   FileTimeToLocalFileTime( &fileTime, &fileTime2 );
   FileTimeToSystemTime( &fileTime2, &systemTime );
   date.iYear = systemTime.wYear;
   date.iMonth = systemTime.wMonth;
   date.iDay = systemTime.wDay;
   time.iHour = systemTime.wHour;
   time.iMinute = systemTime.wMinute;
   time.iSecond = systemTime.wSecond;
   *puDate = Amdate_PackDate( &date );
   *puTime = Amdate_PackTime( &time );
}

/* Given a file handle, returns the creation date of that file.
 */
void EXPORT WINAPI Amfile_GetFileLocalTime( HNDFILE fh, WORD * puDate, WORD * puTime )
{
   SYSTEMTIME systemTime;
   SYSTEMTIME localTime;
   FILETIME fileTime2;
   FILETIME fileTime;
   AM_DATE date;
   AM_TIME time;
   TIME_ZONE_INFORMATION tzi;

   GetTimeZoneInformation( &tzi );
   GetFileTime( fh, NULL, NULL, &fileTime );
   FileTimeToLocalFileTime( &fileTime, &fileTime2 );
   FileTimeToSystemTime( &fileTime2, &systemTime );
   if( SystemTimeToTzSpecificLocalTime( &tzi, &systemTime, &localTime ) )
   {
   date.iYear = localTime.wYear;
   date.iMonth = localTime.wMonth;
   date.iDay = localTime.wDay;
   time.iHour = localTime.wHour;
   time.iMinute = localTime.wMinute;
   time.iSecond = localTime.wSecond;
   }
   else
   {
   date.iYear = systemTime.wYear;
   date.iMonth = systemTime.wMonth;
   date.iDay = systemTime.wDay;
   time.iHour = systemTime.wHour;
   time.iMinute = systemTime.wMinute;
   time.iSecond = systemTime.wSecond;
   }
   *puDate = Amdate_PackDate( &date );
   *puTime = Amdate_PackTime( &time );
}

/* Given a file handle, sets the creation date of that file.
 */

void EXPORT WINAPI Amfile_SetFileTime( HNDFILE fh, WORD wDate, WORD wTime )
{
   SYSTEMTIME systemTime;
   FILETIME fileTime2;
   FILETIME fileTime;
   AM_DATE date;
   AM_TIME time;

   Amdate_UnpackDate( wDate, &date );
   Amdate_UnpackTime( wTime, &time );
   systemTime.wYear = date.iYear;
   systemTime.wMonth = date.iMonth;
   systemTime.wDay = date.iDay;
   systemTime.wDayOfWeek = date.iDayOfWeek;
   systemTime.wHour = time.iHour;
   systemTime.wMinute = time.iMinute;
   systemTime.wSecond = time.iSecond;
   systemTime.wMilliseconds = 0;
   SystemTimeToFileTime( &systemTime, &fileTime );
   LocalFileTimeToFileTime( &fileTime, &fileTime2 );
   SetFileTime( fh, NULL, NULL, &fileTime2 );
}
