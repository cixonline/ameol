/* GETARTCL.C - Routines to handle the Get Article object
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
#include "palrc.h"
#include "resource.h"
#include <memory.h>
#include "cixip.h"
#include "amlib.h"
#include "news.h"

/* This is the Get Article out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_GetArticle( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   GETARTICLEOBJECT FAR * lpgao;

   lpgao = (GETARTICLEOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_GETTYPE:
         return( BF_GETIPNEWS );

      case OBEVENT_EXEC:
         if( IsCixnewsNewsgroup( DRF(lpgao->recNewsgroup) ) )
            return( POF_HELDBYSCRIPT );
         return( Exec_GetArticle( dwData ) );

// This event should not be editable! -pw
//    case OBEVENT_EDITABLE:
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR707), lpgao->dwMsg, Amob_DerefObject( &lpgao->recNewsgroup ) );
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_NEW:
         _fmemset( lpgao, 0, sizeof( GETARTICLEOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         GETARTICLEOBJECT ga;

         INITIALISE_PTR(lpgao);
         Amob_LoadDataObject( fh, &ga, sizeof( GETARTICLEOBJECT ) );
         if( fNewMemory( &lpgao, sizeof( GETARTICLEOBJECT ) ) )
            {
            *lpgao = ga;
            lpgao->recNewsgroup.hpStr = NULL;
            }
         return( (LRESULT)lpgao );
         }

/*    case OBEVENT_EDIT: {
         OBINFO obinfo;

         Amob_GetObInfo( (LPOB)dwData, &obinfo );
         Adm_Dialog( hInst, hwndFrame, MAKEINTRESOURCE(IDDLG_GETTAGGEDARTICLES), GetTaggedDlg, (LPARAM)obinfo.lpObData );
         Amob_Commit( (LPOB)dwData, OBTYPE_GETARTICLE, obinfo.lpObData );
         return( TRUE );
         }
*/
      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpgao->recNewsgroup );
         return( Amob_SaveDataObject( fh, lpgao, sizeof( GETARTICLEOBJECT ) ) );

      case OBEVENT_COPY: {
         GETARTICLEOBJECT FAR * lpgaoNew;

         INITIALISE_PTR( lpgaoNew );
         if( fNewMemory( &lpgaoNew, sizeof( GETARTICLEOBJECT ) ) )
            {
            lpgaoNew->dwMsg = lpgao->dwMsg;
            INITIALISE_PTR( lpgaoNew->recNewsgroup.hpStr );
            Amob_CopyRefObject( &lpgaoNew->recNewsgroup, &lpgao->recNewsgroup );
            }
         return( (LRESULT)lpgaoNew );
         }

      case OBEVENT_FIND: {
         GETARTICLEOBJECT FAR * lpga1;
         GETARTICLEOBJECT FAR * lpga2;

         lpga1 = (GETARTICLEOBJECT FAR *)dwData;
         lpga2 = (GETARTICLEOBJECT FAR *)lpBuf;
         if( lpga1->dwMsg != lpga2->dwMsg )
            return( FALSE );
         return( TRUE );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpgao );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpgao->recNewsgroup );
         return( TRUE );
      }
   return( 0L );
}
