/* WITHDRAW.C - Routines to handle the Withdraw command
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
#include "resource.h"
#include <memory.h>
#include <string.h>
#include "cix.h"
#include "amlib.h"

static BOOL fDefDlgEx = FALSE;

/* This is the withdraw out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_Withdraw( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   WITHDRAWOBJECT FAR * lpwo;

   lpwo = (WITHDRAWOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR235), lpwo->dwMsg, Amob_DerefObject( &lpwo->recTopicName ) );
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( 0 );

      case OBEVENT_NEW:
         lpwo = (WITHDRAWOBJECT FAR *)dwData;
         _fmemset( lpwo, 0, sizeof( WITHDRAWOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         WITHDRAWOBJECT wo;

         Amob_LoadDataObject( fh, &wo, sizeof( WITHDRAWOBJECT ) );
         if( fNewMemory( &lpwo, sizeof( WITHDRAWOBJECT ) ) )
            {
            *lpwo = wo;
            lpwo->recTopicName.hpStr = NULL;
            }
         return( (LRESULT)lpwo );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpwo->recTopicName );
         return( Amob_SaveDataObject( fh, lpwo, sizeof( WITHDRAWOBJECT ) ) );

      case OBEVENT_COPY: {
         WITHDRAWOBJECT FAR * lpwoNew;

         INITIALISE_PTR( lpwoNew );
         if( fNewMemory( &lpwoNew, sizeof( WITHDRAWOBJECT ) ) )
            {
            INITIALISE_PTR( lpwoNew->recTopicName.hpStr );
            lpwoNew->dwMsg = lpwo->dwMsg;
            Amob_CopyRefObject( &lpwoNew->recTopicName, &lpwo->recTopicName );
            }
         return( (LRESULT)lpwoNew );
         }

      case OBEVENT_FIND: {
         WITHDRAWOBJECT FAR * lpwo1;
         WITHDRAWOBJECT FAR * lpwo2;

         lpwo1 = (WITHDRAWOBJECT FAR *)dwData;
         lpwo2 = (WITHDRAWOBJECT FAR *)lpBuf;
         if( strcmp( DRF(lpwo1->recTopicName), DRF(lpwo2->recTopicName) ) != 0 )
            return( FALSE );
         return( lpwo1->dwMsg == lpwo2->dwMsg );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpwo );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpwo->recTopicName );
         return( TRUE );
      }
   return( 0L );
}

/* This is the withdraw out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_Restore( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   RESTOREOBJECT FAR * lpro;

   lpro = (RESTOREOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR1239), lpro->dwMsg, Amob_DerefObject( &lpro->recTopicName ) );
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( 0 );

      case OBEVENT_NEW:
         lpro = (RESTOREOBJECT FAR *)dwData;
         _fmemset( lpro, 0, sizeof( RESTOREOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         RESTOREOBJECT ro;

         Amob_LoadDataObject( fh, &ro, sizeof( RESTOREOBJECT ) );
         if( fNewMemory( &lpro, sizeof( RESTOREOBJECT ) ) )
            {
            *lpro = ro;
            lpro->recTopicName.hpStr = NULL;
            }
         return( (LRESULT)lpro );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpro->recTopicName );
         return( Amob_SaveDataObject( fh, lpro, sizeof( RESTOREOBJECT ) ) );

      case OBEVENT_COPY: {
         RESTOREOBJECT FAR * lproNew;

         INITIALISE_PTR( lproNew );
         if( fNewMemory( &lproNew, sizeof( RESTOREOBJECT ) ) )
            {
            INITIALISE_PTR( lproNew->recTopicName.hpStr );
            lproNew->dwMsg = lpro->dwMsg;
            Amob_CopyRefObject( &lproNew->recTopicName, &lpro->recTopicName );
            }
         return( (LRESULT)lproNew );
         }

      case OBEVENT_FIND: {
         RESTOREOBJECT FAR * lpro1;
         RESTOREOBJECT FAR * lpro2;

         lpro1 = (RESTOREOBJECT FAR *)dwData;
         lpro2 = (RESTOREOBJECT FAR *)lpBuf;
         if( strcmp( DRF(lpro1->recTopicName), DRF(lpro2->recTopicName) ) != 0 )
            return( FALSE );
         return( lpro1->dwMsg == lpro2->dwMsg );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpro );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpro->recTopicName );
         return( TRUE );
      }
   return( 0L );
}
