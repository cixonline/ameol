/* CANCEL.C - Routines to handle the Cancel Article object
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
#include <string.h>
#include "cixip.h"
#include "news.h"

static BOOL fDefDlgEx = FALSE;

/* This is the Cancel out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_CancelArticle( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   CANCELOBJECT FAR * lpcao;

   lpcao = (CANCELOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_EXEC:
         if( IsCixnewsNewsgroup( DRF(lpcao->recNewsgroup) ) )
            return( POF_HELDBYSCRIPT );
         return( Exec_CancelArticle( dwData ) );

      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR740), Amob_DerefObject( &lpcao->recReply ), Amob_DerefObject( &lpcao->recNewsgroup ) );
         return( TRUE );

      case OBEVENT_GETCLSID:
         if( IsCixnewsNewsgroup( DRF(lpcao->recNewsgroup) ) )
            return( 3 );
         return( Amob_GetNextObjectID() );

      case OBEVENT_NEW:
         _fmemset( lpcao, 0, sizeof( CANCELOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         CANCELOBJECT cao;

         INITIALISE_PTR(lpcao);
         Amob_LoadDataObject( fh, &cao, sizeof( CANCELOBJECT ) );
         if( fNewMemory( &lpcao, sizeof( CANCELOBJECT ) ) )
            {
            *lpcao = cao;
            lpcao->recNewsgroup.hpStr = NULL;
            lpcao->recReply.hpStr = NULL;
            lpcao->recFrom.hpStr = NULL;
            lpcao->recRealName.hpStr = NULL;

            }
         return( (LRESULT)lpcao );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpcao->recNewsgroup );
         Amob_SaveRefObject( &lpcao->recReply );
         Amob_SaveRefObject( &lpcao->recFrom );
         Amob_SaveRefObject( &lpcao->recRealName );
         return( Amob_SaveDataObject( fh, lpcao, sizeof( CANCELOBJECT ) ) );

      case OBEVENT_COPY: {
         CANCELOBJECT FAR * lpcaoNew;

         INITIALISE_PTR( lpcaoNew );
         if( fNewMemory( &lpcaoNew, sizeof( CANCELOBJECT ) ) )
            {
            INITIALISE_PTR( lpcaoNew->recNewsgroup.hpStr );
            INITIALISE_PTR( lpcaoNew->recReply.hpStr );
            INITIALISE_PTR( lpcaoNew->recRealName.hpStr );
            INITIALISE_PTR( lpcaoNew->recFrom.hpStr );
            Amob_CopyRefObject( &lpcaoNew->recNewsgroup, &lpcao->recNewsgroup );
            Amob_CopyRefObject( &lpcaoNew->recReply, &lpcao->recReply );
            Amob_CopyRefObject( &lpcaoNew->recFrom, &lpcao->recFrom );
            Amob_CopyRefObject( &lpcaoNew->recRealName, &lpcao->recRealName );
            }
         return( (LRESULT)lpcaoNew );
         }

      case OBEVENT_FIND: {
         CANCELOBJECT FAR * lpcao1;
         CANCELOBJECT FAR * lpcao2;

         lpcao1 = (CANCELOBJECT FAR *)dwData;
         lpcao2 = (CANCELOBJECT FAR *)lpBuf;
         return( strcmp( Amob_DerefObject( &lpcao->recReply ), Amob_DerefObject( &lpcao->recNewsgroup ) ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpcao );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpcao->recNewsgroup );
         Amob_FreeRefObject( &lpcao->recReply );
         Amob_FreeRefObject( &lpcao->recFrom );
         Amob_FreeRefObject( &lpcao->recRealName );
         return( TRUE );
      }
   return( 0L );
}
           