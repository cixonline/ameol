/* MEM.C - Implements the memory management
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

#define  THIS_FILE   __FILE__

#include "warnings.h"
#include <windows.h>
#include <windowsx.h>
#include "amlib.h"
#include <memory.h>
#include <malloc.h>

#define USE_MALLOC

/* This function allocates memory. In the debug version, it initialises
 * the memory to a garbage pattern for easier debugging.
 */
BOOL WINAPI Amuser_AllocMemory( LPVOID FAR * ppv, size_t size )
{
#ifdef USE_MALLOC
   *ppv = malloc(size);
   return *ppv != NULL;
#else
   HGLOBAL hg;
   BYTE HUGEPTR * FAR * ppb = (BYTE HUGEPTR * FAR *)ppv;

   ASSERT( NULL == *ppb );
   ASSERT( ppv != NULL && size != 0 );
   hg = GlobalAlloc( GHND, size );
   if( hg )
      *ppb = (BYTE HUGEPTR *)GlobalLock( hg );
#ifdef MEMDEBUG
   if( hg != NULL )
      hmemset( *ppb, 0xCC, size );
#endif
   return( *ppb != NULL );
#endif
}

/* This function frees memory. In the debug version, it fills the freed
 * memory with a garabge pattern for easier debugging.
 */
void WINAPI Amuser_FreeMemory( LPVOID FAR * ppb )
{
#ifdef USE_MALLOC
   free(*ppb);
   *ppb = NULL;
#else
   HGLOBAL hg;

   ASSERT( *ppb != NULL );
#ifdef MEMDEBUG
   hmemset( *ppb, 0xDC, GlobalSize( GlobalPtrHandle( *ppb ) ) );
#endif
   hg = GlobalPtrHandle( *ppb );
   GlobalUnlock( hg );
   ASSERT( ( GlobalFlags( hg ) & GMEM_LOCKCOUNT ) == 0 );
   VERIFY( GlobalFree( hg ) == NULL );
   *ppb = NULL;
#endif
}

/* This function reallocates a block of memory.
 */
BOOL WINAPI Amuser_ReallocMemory( LPVOID * FAR * ppv, size_t size )
{
#ifdef USE_MALLOC
   *ppv = realloc(*ppv, size);
   return *ppv != NULL;
#else
   BYTE HUGEPTR * FAR * ppb = (BYTE HUGEPTR * FAR *)ppv;
   HGLOBAL hg;

   ASSERT( ppv != NULL && size != 0 );
   if( hg = GlobalPtrHandle( *ppb ) )
      {
      GlobalUnlock( hg );
      ASSERT( ( GlobalFlags( hg ) & GMEM_LOCKCOUNT ) == 0 );
      if( hg = GlobalReAlloc( hg, size, GMEM_MOVEABLE ) )
         {
         *ppb = (BYTE HUGEPTR *)GlobalLock( hg );
         return( TRUE );
         }
      }
   return( FALSE );
#endif
}
