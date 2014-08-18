/* GS.C - Get a string from the resource file
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
#include "windows.h"
#include "pdefs.h"
#include "amlib.h"

#define  MAX_GETSTR     3              /* Can remember this number of strings at once */

static char FAR szBuffer[ MAX_GETSTR ][ 256 ];
static int iStr = -1;

/* This string must NOT be placed in the resource table!
 */
static char FAR str1[] = "LoadString #%u failed\n\nPlease contact CIX Technical Support";

extern HINSTANCE hRscLib;

/* This function extracts a string from the string table resource into a
 * local, static buffer, then returns the address of the buffer.
 */
char * FASTCALL GS( UINT wID )
{
   if( ++iStr == MAX_GETSTR )
      iStr = 0;
   if( 0 == LoadString( hRscLib, wID, szBuffer[ iStr ], 255 ) )
      wsprintf( szBuffer[ iStr ], str1, wID );
   return( szBuffer[ iStr ] );
}

/* This function rereferences an int resource pointer.
 */
LPCSTR FASTCALL GINTRSC( LPCSTR lpcstr )
{
   if( HIWORD( lpcstr ) == 0 )
      return( (LPCSTR)GS( LOWORD( lpcstr ) ) );
   return( lpcstr );
}
