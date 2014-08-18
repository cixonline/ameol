/* NEWMAIL.C - Routines to handle the Get New Mail object
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
#include <string.h>
#include "resource.h"
#include "amlib.h"
#include "cixip.h"

/* This is the Get New Mail out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_GetNewMail( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   switch( uType )
      {
      case OBEVENT_EXEC:
         return( Exec_GetNewMail( dwData ) );

      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_DESCRIBE:
         strcpy( lpBuf, GS(IDS_STR716) );
         return( TRUE );

      case OBEVENT_DELETE:
      case OBEVENT_FIND:
         return( TRUE );
      }
   return( 0L );
}
