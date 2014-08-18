/* NFL.C - News folder list
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
#include "amlib.h"
#include "nfl.h"
#include "rules.h"
#include <string.h>

/* This function builds an array of unsubscribed news folders.
 */
BOOL FASTCALL BuildFolderList( LPSTR lpPathname, LPNEWSFOLDERLIST lpnfl )
{
   lpnfl->cTopics = 0;
   lpnfl->ptlTopicList = NULL;
   if( _strcmpi( lpPathname, "(All)" ) == 0 )
      {
      PCAT pcat;
      PCL pcl;
      PTL ptl;

      /* Count number of topics in all categories.
       */
      for( pcat = Amdb_GetFirstCategory(); pcat; pcat = Amdb_GetNextCategory( pcat ) )
         for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
            {
            FOLDERINFO folderinfo;

            Amdb_GetFolderInfo( pcl, &folderinfo );
            lpnfl->cTopics += folderinfo.cTopics;
            }

      /* Build a list of the newsgroups in those categories.
       */
      if( lpnfl->cTopics )
         if( fNewMemory( (LPVOID)&lpnfl->ptlTopicList, lpnfl->cTopics * sizeof(PTL) ) )
            {
            lpnfl->cTopics = 0;
            for( pcat = Amdb_GetFirstCategory(); pcat; pcat = Amdb_GetNextCategory( pcat ) )
               for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
                  for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
                     if( Amdb_GetTopicType( ptl ) == FTYPE_NEWS && !Amdb_IsResignedTopic( ptl ) )
                        lpnfl->ptlTopicList[ lpnfl->cTopics++ ] = ptl;
            if( 0 == lpnfl->cTopics )
               FreeMemory( (LPVOID)&lpnfl->ptlTopicList );
            }
      }
   else
      {
      LPVOID pData;

      ParseFolderPathname( lpPathname, &pData, FALSE, FTYPE_NEWS );
      if( pData )
         if( Amdb_IsCategoryPtr( pData ) )
            {
            PCL pcl;
            PTL ptl;
      
            /* Build a list of subscribed newsgroups in the
             * specified database.
             */
            for( pcl = Amdb_GetFirstFolder( (PCAT)pData ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
               {
               FOLDERINFO folderinfo;
      
               Amdb_GetFolderInfo( pcl, &folderinfo );
               lpnfl->cTopics += folderinfo.cTopics;
               }
            if( lpnfl->cTopics )
               if( fNewMemory( (LPVOID)&lpnfl->ptlTopicList, lpnfl->cTopics * sizeof(PTL) ) )
                  {
                  lpnfl->cTopics = 0;
                  for( pcl = Amdb_GetFirstFolder( (PCAT)pData ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
                     for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
                        if( Amdb_GetTopicType( ptl ) == FTYPE_NEWS && !Amdb_IsResignedTopic( ptl ) )
                           lpnfl->ptlTopicList[ lpnfl->cTopics++ ] = ptl;
                  if( 0 == lpnfl->cTopics )
                     FreeMemory( (LPVOID)&lpnfl->ptlTopicList );
                  }
            }
         else if( Amdb_IsFolderPtr( pData ) )
            {
            FOLDERINFO folderinfo;
            PTL ptl;
      
            Amdb_GetFolderInfo( (PCL)pData, &folderinfo );
            if( folderinfo.cTopics )
               if( fNewMemory( (LPVOID)&lpnfl->ptlTopicList, folderinfo.cTopics * sizeof(PTL) ) )
                  {
                  lpnfl->cTopics = 0;
                  for( ptl = Amdb_GetFirstTopic( (PCL)pData ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
                     if( Amdb_GetTopicType( ptl ) == FTYPE_NEWS && !Amdb_IsResignedTopic( ptl ) )
                        lpnfl->ptlTopicList[ lpnfl->cTopics++ ] = ptl;
                  if( 0 == lpnfl->cTopics )
                     FreeMemory( (LPVOID)&lpnfl->ptlTopicList );
                  }
            }
         else
            {
            lpnfl->cTopics = 0;
            if( fNewMemory( (LPVOID)&lpnfl->ptlTopicList, sizeof(PTL) ) )
               lpnfl->ptlTopicList[ lpnfl->cTopics++ ] = (PTL)pData;
            }
      }
   return( lpnfl->cTopics > 0 );
}
