/* GETMESS.C - Routines to handle the Get Message object
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
#include "resource.h"
#include <memory.h>
#include <string.h>
#include "cix.h"
#include "amlib.h"
#include "webapi.h"

/* This is the Get Message out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_GetCIXMessage( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   GETMESSAGEOBJECT FAR * lpgmo;

   lpgmo = (GETMESSAGEOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_EXEC:
         if (fUseWebServices){ // !!SM!! 2.56.2053
            char lPath[255];
            wsprintf(lPath, "%s\\%s", (LPSTR)pszScratchDir, (LPSTR)szScratchPad );
            return (Am2DownloadScratch(szCIXNickname, szCIXPassword, lPath));
         }
         else
            return( POF_HELDBYSCRIPT );

      case OBEVENT_DESCRIBE:
         if( lpgmo->dwStartMsg == lpgmo->dwEndMsg )
            wsprintf( lpBuf, GS(IDS_STR219), lpgmo->dwStartMsg );
         else
            wsprintf( lpBuf, GS(IDS_STR220), lpgmo->dwStartMsg, lpgmo->dwEndMsg );
         wsprintf( lpBuf + strlen(lpBuf), GS(IDS_STR221), Amob_DerefObject( &lpgmo->recTopicName ) );
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( 0 );

      case OBEVENT_NEW:
         _fmemset( lpgmo, 0, sizeof( GETMESSAGEOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         GETMESSAGEOBJECT gmo;

         INITIALISE_PTR(lpgmo);
         Amob_LoadDataObject( fh, &gmo, sizeof( GETMESSAGEOBJECT ) );
         if( fNewMemory( &lpgmo, sizeof( GETMESSAGEOBJECT ) ) )
            {
            *lpgmo = gmo;
            lpgmo->recTopicName.hpStr = NULL;
            }
         return( (LRESULT)lpgmo );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpgmo->recTopicName );
         return( Amob_SaveDataObject( fh, lpgmo, sizeof( GETMESSAGEOBJECT ) ) );

      case OBEVENT_COPY: {
         GETMESSAGEOBJECT FAR * lpgmoNew;

         INITIALISE_PTR( lpgmoNew );
         if( fNewMemory( &lpgmoNew, sizeof( GETMESSAGEOBJECT ) ) )
            {
            lpgmoNew->dwStartMsg = lpgmo->dwStartMsg;
            lpgmoNew->dwEndMsg = lpgmo->dwEndMsg;
            INITIALISE_PTR( lpgmoNew->recTopicName.hpStr );
            Amob_CopyRefObject( &lpgmoNew->recTopicName, &lpgmo->recTopicName );
            }
         return( (LRESULT)lpgmoNew );
         }

      case OBEVENT_FIND: {
         GETMESSAGEOBJECT FAR * lpgmo1;
         GETMESSAGEOBJECT FAR * lpgmo2;

         lpgmo1 = (GETMESSAGEOBJECT FAR *)dwData;
         lpgmo2 = (GETMESSAGEOBJECT FAR *)lpBuf;
         if( strcmp( DRF(lpgmo1->recTopicName), DRF(lpgmo2->recTopicName) ) != 0 )
            return( FALSE );
         if( lpgmo1->dwStartMsg != lpgmo2->dwStartMsg || lpgmo1->dwEndMsg != lpgmo2->dwEndMsg )
            return( FALSE );
         return( TRUE );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpgmo );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpgmo->recTopicName );
         return( TRUE );
      }
   return( 0L );
}
