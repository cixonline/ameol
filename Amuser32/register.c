/* REGISTER.C - Events registry
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

#include "warnings.h"
#include <windows.h>
#include <windowsx.h>
#include <assert.h>
#include "amlib.h"
#include "amuser.h"

typedef struct tagREGLIST {
   struct tagREGLIST FAR * next;    /* pointer to next registered entry */
   LPFNEEVPROC pMacro;              /* name of macro to be executed */
} REGLIST;

typedef REGLIST FAR * LPREGLIST;

static LPREGLIST reglist[ MAX_EVENTS ] = { NULL };    /* List of registered events */

/* This function adds a user-supplied procedure to a list of procedures
 * that are executed for a specific event.
 */
BOOL WINAPI EXPORT Amuser_RegisterEvent( int wEvent, LPFNEEVPROC lpfProc )
{
   LPREGLIST rp;
   LPREGLIST newp;

   INITIALISE_PTR(newp);
   if( wEvent < AE_FIRSTEVENT || wEvent > AE_LASTEVENT )
      return( FALSE );
   if( ( rp = reglist[ wEvent ] ) != NULL )
      while( rp->next != NULL )
         rp = rp->next;
   if( fNewMemory( &newp, sizeof( REGLIST ) ) )
      {
      if( !rp )
         reglist[ wEvent ] = newp;
      else
         rp->next = newp;
      newp->next = NULL;
      newp->pMacro = lpfProc;
      return( TRUE );
      }
   return( FALSE );
}

/* This function unregisters a macro. The macro is removed from the list
 * of macros attached to the specified registration.
 */
BOOL WINAPI EXPORT Amuser_UnregisterEvent( int wEvent, LPFNEEVPROC lpfProc )
{
   LPREGLIST rp;
   LPREGLIST lp;

   lp = NULL;
   if( wEvent < AE_FIRSTEVENT || wEvent > AE_LASTEVENT )
      return( FALSE );
   for( rp = reglist[ wEvent ]; rp; lp = rp, rp = rp->next )
      if( rp->pMacro == lpfProc )
         {
         if( lp )
            lp->next = rp->next;
         else
            reglist[ wEvent ] = rp->next;
         FreeMemory( &rp );
         return( TRUE );
         }
   return( FALSE );
}

/* This function unregisters all events.
 */
void WINAPI EXPORT UnregisterAllEvents( void )
{
   register int c;
   register int d = 0;
   LPREGLIST rp;
   LPREGLIST rpNext;

   for( c = AE_FIRSTEVENT; c <= AE_LASTEVENT; ++c )
      {
      for( rp = reglist[ c ]; rp; rp = rpNext )
         {
         rpNext = rp->next;
         FreeMemory( &rp );
         ++d;
         }
      reglist[ c ] = NULL;
      }
   
}

/* This function calls all macros registered at the specified level.
 */
BOOL WINAPI EXPORT Amuser_CallRegistered( int wEvent, LPARAM lParam1, LPARAM lParam2 )
{
   LPREGLIST rpNext;
   LPREGLIST rp;

   for( rp = reglist[ wEvent ]; rp != NULL; rp = rpNext )
      {
      rpNext = rp->next;
      if( !(*rp->pMacro)( wEvent, lParam1, lParam2 ) )
         return( FALSE );
      }
   return( TRUE );
}
