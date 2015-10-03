/* FFIND.C - Implements FindFirst/FindNext
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
#ifndef WIN32
#include <dos.h>
#include <stdlib.h>
#else
#include <io.h>
#endif

/* Searches for the first file that matches the given filename pattern.
 */
HFIND EXPORT WINAPI Amuser_FindFirst( LPSTR npFilename, UINT attrib, FINDDATA FAR * npFindData )
{
#ifdef WIN32
   return( _findfirst( npFilename, npFindData ) );
#else
   char FileName[ _MAX_PATH ];

   lstrcpy( FileName, npFilename );
   return( _dos_findfirst( FileName, attrib, npFindData ) ? -1 : 0 );
#endif
}

/* Searches for the next file that matches the given filename pattern previously
 * specified by a call to Amuser_FindFirst.
 */
HFIND EXPORT WINAPI Amuser_FindNext( HFIND hFind, FINDDATA FAR * npFindData )
{
#ifdef WIN32
   return( _findnext( hFind, npFindData ) );
#else
   return( _dos_findnext( npFindData ) ? -1 : 0 );
#endif
}

/* Completes the find operation initiated by a call to Amuser_FindFirst.
 */
HFIND EXPORT WINAPI Amuser_FindClose( HFIND hFind )
{
#ifdef WIN32
   return( _findclose( hFind ) );
#else
   return( 0 );
#endif
}
