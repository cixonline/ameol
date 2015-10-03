/* AMEOL.C - The old Ameol interface
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
#include <stdlib.h>
#include "amlib.h"
#include "amuser.h"
#include <string.h>
#include "amaddr.h"
#include "amctrls.h"
#include "amdb.h"
#include "ameol2.h"
#include <memory.h>
#include "amevent.h"
#include "intcomms.h"
#include "common.bld"
#include "layer.h"

#define  THIS_FILE   __FILE__

/* Get/Set bits */
#define  BSB_CLRBIT           0
#define  BSB_SETBIT           1
#define  BSB_GETBIT           2

/* Obsolete flags.
 */
#define  TF_LOCAL             0x0040

/* Some lengths.
 */
#define  LEN_MODERATORS    80                /* List of moderators */

#pragma warning(disable:4103)

#pragma pack(1)
typedef struct tagOLDTOPICINFO {
   char szTopicName[ 78 ];                /* Topic name */
   char szFileName[ 9 ];                  /* Unique topic file name */
   WORD wFlags;                           /* Topic flags */
   DWORD cMsgs;                           /* Number of messages in this topic */
   DWORD cUnRead;                         /* Number of un-read messages */
   DWORD dwMinMsg;                        /* Smallest message in this topic */
   DWORD dwMaxMsg;                        /* Largest message in this topic */
   char szSigFile[ 9 ];                   /* Topic signature file */
} OLDTOPICINFO;
#pragma pack()

#pragma pack(1)
typedef struct tagOLDCONFERENCEINFO {
   char szConfName[ LEN_CONFNAME+1 ];     /* Conference name */
   char szFileName[ 9 ];                  /* Conference file name */
   DWORD cMsgs;                           /* Number of messages in this conference */
   DWORD cUnRead;                         /* Number of un-read messages */
   WORD cTopics;                          /* Number of topics in conference */
   WORD wTime;                            /* Time when conference created */
   WORD wDate;                            /* Date when conference created */
} OLDCONFERENCEINFO;
#pragma pack()

#pragma pack(1)
typedef struct tagOLDMSGINFO {
   DWORD dwMsg;                           /* Message number */
   DWORD dwComment;                       /* Message to which this is a comment */
   char szTitle[ LEN_TITLE+1 ];           /* Title of this message */
   char szAuthor[ LEN_INTERNETNAME+1 ];   /* Author of this message */
   char szReply[ LEN_REPLY+1 ];           /* Message identification */
   WORD wFlags;                           /* Message flags (MF_xxx) */
   WORD wDate;                            /* Date of this message (packed) */
   WORD wTime;                            /* Time of this message (packed) */
   DWORD cUnRead;                         /* Number of un-read messages */
   BYTE wLevel;                           /* Level number of message */
   BYTE cPriority;                        /* Non-zero if thread has priority messages */
   WORD wComments;                        /* Number of comments to message */
   DWORD cUnReadInThread;                 /* Number of un-read messages in thread */
   DWORD dwActual;                        /* Physical mail message number */
} OLDMSGINFO;
#pragma pack()

#pragma pack(1)
typedef struct tagOLDTOOLINFO {
   DWORD dwVersion;                       /* Version number, in Ameol format */
   char szFileName[ 13 ];                 /* File name */
   char szAuthor[ 40 ];                   /* Author's identification field */
   char szDescription[ 100 ];             /* Product description field */
} OLDTOOLINFO;
#pragma pack()

#pragma pack(1)
typedef struct tagOLDCURMSG {
   LPVOID pcl;                            /* Conference */
   LPVOID ptl;                            /* Topic */
   LPVOID pth;                            /* Message */
} OLDCURMSG;
#pragma pack()

#pragma pack(1)
typedef struct tagOLDPURGEOPTIONS {
   WORD wFlags;                           /* Purge options flags */
   WORD wMaxSize;                         /* Maximum topic size if PO_SIZEPRUNE */
   WORD wMaxDate;                         /* Maximum days to keep if PO_DATEPRUNE */
} OLDPURGEOPTIONS;
#pragma pack()

HINSTANCE hLibInst;                       /* Library instance handle */
HINSTANCE hRegLib;                        /* Regular Expression and Mail Library */

/* This is the Ameol DLL entry point. Two separate functions are
 * provided: one for Windows 32 and one for 16-bit Windows.
 */
BOOL WINAPI EXPORT DllMain( HANDLE hInstance, DWORD fdwReason, LPVOID lpvReserved )
{
   switch( fdwReason )
   {
   case DLL_PROCESS_ATTACH:
      hLibInst = hInstance;
      hRegLib = NULL;
      return( 1 );

   case DLL_PROCESS_DETACH:
      return( 1 );

   default:
      return( 1 );
   }
}

/* This function returns the version number of Amctrls.
 */
DWORD WINAPI EXPORT GetBuildVersion( void )
{
   return( MAKEVERSION( PRODUCT_MAX_VER, PRODUCT_MIN_VER, PRODUCT_BUILD ) );
}

/* This function implements the CreateCategory API call.
 */
LPVOID EXPORT FAR PASCAL CreateCategory( LPSTR lpszCatName )
{
   if( !PVDataAddress( lpszCatName, szFNPVCreateCategory ) )
      return( NULL );
   return( Amdb_CreateCategory( lpszCatName ) );
}

/* This function implements the OpenCategory API call.
 */
LPVOID EXPORT FAR PASCAL OpenCategory( LPSTR lpszCatName )
{
   if( !PVDataAddress( lpszCatName, szFNPVOpenCategory ) )
      return( NULL );
   return( Amdb_OpenCategory( lpszCatName ) );
}

/* This function implements the DeleteCategory API call.
 */
BOOL EXPORT FAR PASCAL DeleteCategory( PCAT hCat )
{
   if( !PVCategoryHandle( hCat, szFNPVDeleteCategory ) )
      return( FALSE );
   return( Amdb_DeleteCategory( hCat ) == AMDBERR_NOERROR );
}

/* This function implements the GetCategory API call.
 */
PCAT EXPORT FAR PASCAL GetCategory( PCAT hCat )
{
   if( NULL == hCat )
      return( Amdb_GetFirstCategory() );
   if( !PVCategoryHandle( hCat, szFNPVGetCategory ) )
      return( NULL );
   return( Amdb_GetNextCategory( hCat ) );
}

/* This function implements the GetPreviousCategory API call.
 */
PCAT EXPORT FAR PASCAL GetPreviousCategory( PCAT hCat )
{
   if( NULL == hCat )
      return( Amdb_GetLastCategory() );
   if( !PVCategoryHandle( hCat, szFNPVGetPreviousCategory ) )
      return( NULL );
   return( Amdb_GetPreviousCategory( hCat ) );
}

/* This function implements the CategoryFromConference API call.
 */
PCAT EXPORT FAR PASCAL CategoryFromConference( PCL hConf )
{
   if( !PVConferenceHandle( hConf, szFNPVCategoryFromConference ) )
      return( NULL );
   return( Amdb_CategoryFromFolder( hConf ) );
}

/* This function implements the GetCategoryName API call.
 */
LPCSTR EXPORT FAR PASCAL GetCategoryName( PCAT pcat )
{
   if( !PVCategoryHandle( pcat, szFNPVGetCategoryName ) )
      return( NULL );
   return( Amdb_GetCategoryName( pcat ) );
}

/* This function implements the GetLastCategory API call.
 */
PCAT EXPORT FAR PASCAL GetLastCategory( void )
{
   return( Amdb_GetLastCategory() );
}

/* This function implements the GetCategoryInfo API call.
 */
void EXPORT FAR PASCAL GetCategoryInfo( PCAT pcat, CATEGORYINFO FAR * lpocati )
{
   CATEGORYINFO ci;

   if( !PVCategoryHandle( pcat, szFNPVGetCategoryInfo ) )
      return;
   if( !PVDataAddress( lpocati, szFNPVGetCategoryInfo ) )
      return;
   Amdb_GetCategoryInfo( pcat, &ci );
   lstrcpyn( lpocati->szCategoryName, ci.szCategoryName, sizeof( lpocati->szCategoryName ) );
   lpocati->cMsgs = ci.cMsgs;
   lpocati->cUnRead = ci.cUnRead;
   lpocati->cFolders = ci.cFolders;
   lpocati->wCreationTime = ci.wCreationTime;
   lpocati->wCreationDate = ci.wCreationDate;
   lpocati->dwFlags = ci.dwFlags;
}

/* This function implements the CreateConferenceInCategory API call.
 */
LPVOID EXPORT FAR PASCAL CreateConferenceInCategory( PCAT pcat, LPSTR lpszConfName )
{
   if( !PVCategoryHandle( pcat, szFNPVCreateConferenceInCategory ) )
      return( NULL );
   if( !PVDataAddress( lpszConfName, szFNPVCreateConferenceInCategory ) )
      return( NULL );
   return( Amdb_CreateFolder( pcat, lpszConfName, CFF_SORT ) );
}

/* This function implements the CreateConference API call.
 */
LPVOID EXPORT FAR PASCAL CreateConference( LPSTR lpszConfName )
{
   if( !PVDataAddress( lpszConfName, szFNPVCreateConference ) )
      return( NULL );
   return( Amdb_CreateFolder( Amdb_GetFirstCategory(), lpszConfName, CFF_SORT ) );
}

/* This function implements the OpenConference API call.
 */
LPVOID EXPORT FAR PASCAL OpenConference( LPSTR lpszConfName )
{
   if( !PVDataAddress( lpszConfName, szFNPVOpenConference ) )
      return( NULL );
   if( strcmp( lpszConfName, "mail" ) == 0 )
      lpszConfName = "Mail";
   return( Amdb_OpenFolder( NULL, lpszConfName ) );
}

/* This function implements the DeleteConference API call.
 */
BOOL EXPORT FAR PASCAL DeleteConference( PCL hConf )
{
   if( !PVConferenceHandle( hConf, szFNPVDeleteConference ) )
      return( FALSE );
   return( Amdb_DeleteFolder( hConf ) == AMDBERR_NOERROR );
}

/* This function implements the DeleteTopic API call.
 */
BOOL EXPORT FAR PASCAL DeleteTopic( PTL hTopic )
{
   if( !PVTopicHandle( hTopic, szFNPVDeleteTopic ) )
      return( FALSE );
   return( Amdb_DeleteTopic( hTopic ) == AMDBERR_NOERROR );
}

/* This function implements the GetConference API call.
 */
PCL EXPORT FAR PASCAL GetConference( PCL hConf )
{
   if( NULL == hConf )
      return( Amdb_GetFirstRealFolder( Amdb_GetFirstCategory() ) );
   if( !PVConferenceHandle( hConf, szFNPVGetConference ) )
      return( NULL );
   return( Amdb_GetNextUnconstrainedFolder( hConf ) );
}

/* This function implements the GetConferenceEx API call.
 */
PCL EXPORT FAR PASCAL GetConferenceEx( PCAT hCat, PCL hConf )
{
   if( NULL == hConf )
      return( Amdb_GetFirstRealFolder( hCat ) );
   if( !PVConferenceHandle( hConf, szFNPVGetConference ) )
         return( NULL );
   return( Amdb_GetNextFolder( hConf ) );
}

/* This function implements the GetPreviousConference API call.
 */
PCL EXPORT FAR PASCAL GetPreviousConference( PCL hConf )
{
   if( NULL == hConf )
      return( Amdb_GetLastRealFolder( Amdb_GetLastCategory() ) );
   if( !PVConferenceHandle( hConf, szFNPVGetPreviousConference ) )
      return( NULL );
   return( Amdb_GetPreviousUnconstrainedFolder( hConf ) );
}

/* This function implements the ConferenceFromTopic API call.
 */
PCL EXPORT FAR PASCAL ConferenceFromTopic( PTL hTopic )
{
   if( !PVTopicHandle( hTopic, szFNPVConferenceFromTopic ) )
      return( NULL );
   return( Amdb_FolderFromTopic( hTopic ) );
}

/* This function implements the SetCategoryName API call.
 */
void EXPORT FAR PASCAL SetCategoryName( PCAT pcat, LPCSTR lpszName )
{
   Amdb_SetCategoryName( pcat, lpszName );
}

/* This function implements the GetTopicType API call.
 */
WORD EXPORT FAR PASCAL GetTopicType( PTL hTopic )
{
   if( !PVTopicHandle( hTopic, szFNPVGetTopicType ) )
      return( FALSE );
   return( Amdb_GetTopicType( hTopic ) );
}

/* This function implements the RegisterEvent API call.
 */
BOOL EXPORT FAR PASCAL RegisterEvent( int wEvent, LPFNEEVPROC lpfProc )
{
   if( !PVCodeAddress( (FARPROC)lpfProc, szFNPVRegisterEvent ) )
      return( FALSE );
   return( Amuser_RegisterEvent( wEvent, lpfProc ) );
}

/* This function implements the UnregisterEvent API call.
 */
BOOL EXPORT FAR PASCAL UnregisterEvent( int wEvent, LPFNEEVPROC lpfProc )
{
   if( !PVCodeAddress( (FARPROC)lpfProc, szFNPVUnregisterEvent ) )
      return( FALSE );
   return( Amuser_UnregisterEvent( wEvent, lpfProc ) );
}

/* This function implements the GetConferenceName API call.
 */
LPCSTR EXPORT FAR PASCAL GetConferenceName( PCL pcl )
{
   if( !PVConferenceHandle( pcl, szFNPVGetConferenceName ) )
      return( NULL );
   return( Amdb_GetFolderName( pcl ) );
}

/* This function implements the CreateTopic API call.
 */
PTL EXPORT FAR PASCAL CreateTopic( PCL pcl, LPSTR lpszTopicName )
{
   if( !PVConferenceHandle( pcl, szFNPVCreateTopic ) )
      return( NULL );
   if( !PVDataAddress( lpszTopicName, szFNPVCreateTopic ) )
      return( NULL );
   if( _strcmpi( Amdb_GetFolderName( pcl ), "mail" ) == 0 )
      return( Amdb_CreateTopic( pcl, lpszTopicName, FTYPE_MAIL) );
   return( Amdb_CreateTopic( pcl, lpszTopicName, FTYPE_CIX ) );
}

/* This function implements the OpenTopic API call.
 */
PTL EXPORT FAR PASCAL OpenTopic( PCL pcl, LPSTR lpszTopicName )
{
   if( !PVConferenceHandle( pcl, szFNPVOpenTopic ) )
      return( NULL );
   if( !PVDataAddress( lpszTopicName, szFNPVOpenTopic ) )
      return( NULL );
   return( Amdb_OpenTopic( pcl, lpszTopicName ) );
}

/* This function implements the GetTopicName API call.
 */
LPCSTR EXPORT FAR PASCAL GetTopicName( PTL ptl )
{
   if( !PVTopicHandle( ptl, szFNPVGetTopicName ) )
      return( NULL );
   return( Amdb_GetTopicName( ptl ) );
}

/* This function implements the GetTopic API call.
 */
PTL EXPORT FAR PASCAL GetTopic( PCL pcl, PTL ptl )
{
   if( NULL == ptl )
      return( Amdb_GetFirstTopic( pcl ) );
   if( pcl != NULL )
      if( !PVConferenceHandle( pcl, szFNPVGetTopic ) )
         return( NULL );
   if( !PVTopicHandle( ptl, szFNPVGetTopic ) )
      return( NULL );
   return( Amdb_GetNextTopic( ptl ) );
}

/* This function implements the GetPreviousTopic API call.
 */
PTL EXPORT FAR PASCAL GetPreviousTopic( PTL ptl )
{
   if( !PVTopicHandle( ptl, szFNPVGetPreviousTopic ) )
      return( NULL );
   return( Amdb_GetPreviousTopic( ptl ) );
}

/* This function implements the GetLastTopic API call.
 */
PTL EXPORT FAR PASCAL GetLastTopic( PCL pcl )
{
   if( !PVConferenceHandle( pcl, szFNPVGetLastTopic ) )
      return( NULL );
   return( Amdb_GetLastTopic( pcl ) );
}

/* This function implements the GetTopicInfo API call.
 */
void EXPORT FAR PASCAL GetTopicInfo( PTL ptl, OLDTOPICINFO FAR * lpoti )
{
   TOPICINFO ti;

   /* Validate parameters.
    */
   if( !PVTopicHandle( ptl, szFNPVGetTopicInfo ) )
      return;
   if( !PVDataAddress( lpoti, szFNPVGetTopicInfo ) )
      return;

   /* Get and convert between TOPICINFO structures.
    */
   Amdb_GetTopicInfo( ptl, &ti );
   lstrcpyn( lpoti->szTopicName, ti.szTopicName, sizeof( lpoti->szTopicName ) );
   lstrcpyn( lpoti->szFileName, ti.szFileName, sizeof( lpoti->szFileName ) );
   lstrcpyn( lpoti->szSigFile, ti.szSigFile, sizeof( lpoti->szSigFile ) );
   lpoti->wFlags = LOWORD(ti.dwFlags);
   lpoti->cMsgs = ti.cMsgs;
   lpoti->cUnRead = ti.cUnRead;
   lpoti->dwMinMsg = ti.dwMinMsg;
   lpoti->dwMaxMsg = ti.dwMaxMsg;

   /* Hack. Manually set TF_LOCAL flag.
    */
   if( Amdb_GetTopicType( ptl ) == FTYPE_LOCAL )
      lpoti->wFlags |= TF_LOCAL;
}

/* This function implements the TopicFromMsg API call.
 */
PTL EXPORT FAR PASCAL TopicFromMsg( PTH pth )
{
   if( !PVMsgHandle( pth, szFNPVTopicFromMsg ) )
      return( NULL );
   return( Amdb_TopicFromMsg( pth ) );
}

/* This function implements the CreateMsg API call.
 */
UINT EXPORT FAR PASCAL CreateMsg( PTL ptl, PTH FAR * lppth, DWORD dwMsg,
   DWORD dwComment, LPSTR lpszTitle, LPSTR lpszAuthor, LPSTR lpszMessageId, AM_DATE FAR * lpDate,
   AM_TIME FAR * lpTime, HPSTR lpszMsg, DWORD cbsz )
{

   if( !PVTopicHandle( ptl, szFNPVCreateMsg ) )
      return( 0 );
   if( lppth )
      if( !PVDataAddress( lppth, szFNPVCreateMsg ) )
         return( 0 );
   if( !PVDataAddress( lpszTitle, szFNPVCreateMsg ) )
      return( 0 );
   if( !PVDataAddress( lpszAuthor, szFNPVCreateMsg ) )
      return( 0 );
   if( lpszMessageId )
      if( !PVDataAddress( lpszMessageId, szFNPVCreateMsg ) )
         return( 0 );
   if( !PVDataAddress( lpDate, szFNPVCreateMsg ) )
      return( 0 );
   if( !PVDataAddress( lpTime, szFNPVCreateMsg ) )
      return( 0 );
   if( !PVDataAddress( lpszMsg, szFNPVCreateMsg ) )
      return( 0 );

   /* Create a dummy compact header. Then prepend that to
    * the actual message.
    */
   if( ( Amdb_GetTopicType( ptl ) == FTYPE_CIX ) || ( Amdb_GetTopicType( ptl ) == FTYPE_LOCAL ) )
   {
   TOPICINFO topicinfo;
   HPSTR lpszMsg2;
   LPSTR lpMyTmpBuf;                /* General purpose buffer */
   char FAR szCompactHdrTmpl[] = ">>>%.14s/%.14s %lu %.14s(%lu)%d%s%2.2d %2.2d:%2.2d";
   char * pszCixMonths[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

   INITIALISE_PTR(lpMyTmpBuf);
   if( !fNewMemory( &lpMyTmpBuf, 512 ) )
      return( 0 );
   Amdb_GetTopicInfo( ptl, &topicinfo );
   wsprintf( lpMyTmpBuf, szCompactHdrTmpl,
         Amdb_GetFolderName( Amdb_FolderFromTopic( ptl ) ),
         Amdb_GetTopicName( ptl ),
         dwMsg,
         lpszAuthor,
         cbsz,
         lpDate->iDay,
         (LPSTR)pszCixMonths[ lpDate->iMonth - 1 ],
         lpDate->iYear % 100,
         lpTime->iHour,
         lpTime->iMinute );
         strcat( lpMyTmpBuf, "\r\n" );
         lpszMsg2 = ConcatenateStrings( lpMyTmpBuf, lpszMsg );
   if( NULL != lpMyTmpBuf )
      FreeMemory( &lpMyTmpBuf );

   return( Amdb_CreateMsg( ptl, lppth, dwMsg, dwComment, lpszTitle, lpszAuthor, lpszMessageId, lpDate,
                           lpTime, lpszMsg2, strlen( lpszMsg2 ), 0L ) );
   }
   else
   return( Amdb_CreateMsg( ptl, lppth, dwMsg, dwComment, lpszTitle, lpszAuthor, lpszMessageId, lpDate,
                           lpTime, lpszMsg, cbsz, 0L ) );

}

/* This function implements the GetNextMsg API call.
 */
PTH EXPORT FAR PASCAL GetNextMsg( PTL ptl, PTH pth, int nMode )
{
   if( ptl )
      if( !PVTopicHandle( ptl, szFNPVGetNextMsg ) )
         return( NULL );
   if( pth )
      if( !PVMsgHandle( pth, szFNPVGetNextMsg ) )
         return( NULL );
   if( NULL == pth )
      return( Amdb_GetFirstMsg( ptl, nMode ) );
   return( Amdb_GetNextMsg( pth, nMode ) );
}

/* This function implements the GetPreviousMsg API call.
 */
PTH EXPORT FAR PASCAL GetPreviousMsg( PTH pth, int nMode )
{
   if( pth )
      if( !PVMsgHandle( pth, szFNPVGetPreviousMsg ) )
         return( NULL );
   return( Amdb_GetPreviousMsg( pth, nMode ) );
}

/* This function implements the GetLastMsg API call.
 */
PTH EXPORT FAR PASCAL GetLastMsg( PTL ptl, int nMode )
{
   if( !PVTopicHandle( ptl, szFNPVGetLastMsg ) )
      return( NULL );
   return( Amdb_GetLastMsg( ptl, nMode ) );
}

/* This function implements the GetRootMsg API call.
 */
PTH EXPORT FAR PASCAL GetRootMsg( PTH pth, BOOL fPrev )
{
   if( !PVMsgHandle( pth, szFNPVGetRootMsg ) )
      return( NULL );
   return( Amdb_GetRootMsg( pth, fPrev ) );
}

/* This function implements the GetNearestMsg API call.
 */
PTH EXPORT FAR PASCAL GetNearestMsg( PTH pth, int nMode )
{
   if( pth )
      if( !PVMsgHandle( pth, szFNPVGetNearestMsg ) )
         return( NULL );
   return( Amdb_GetNearestMsg( pth, nMode ) );
}

/* This function implements the GetNextUnReadMsg API call.
 */
BOOL EXPORT FAR PASCAL GetNextUnReadMsg( OLDCURMSG FAR * lpunr, int nMode )
{
   CURMSG cm;

   if( lpunr->pth )
      if( !PVMsgHandle( lpunr->pth, szFNPVGetNextUnReadMsg ) )
         return( FALSE );
   if( lpunr->ptl )
      if( !PVTopicHandle( lpunr->ptl, szFNPVGetNextUnReadMsg ) )
         return( FALSE );
   if( lpunr->pcl )
      if( !PVConferenceHandle( lpunr->pcl, szFNPVGetNextUnReadMsg ) )
         return( FALSE );
   cm.pcat = NULL;
   cm.pcl = lpunr->pcl;
   cm.ptl = lpunr->ptl;
   cm.pth = lpunr->pth;
   if( Amdb_GetNextUnReadMsg( &cm, nMode ) )
      {
      lpunr->pcl = cm.pcl;
      lpunr->ptl = cm.ptl;
      lpunr->pth = cm.pth;
      return( TRUE );
      }
   return( FALSE );
}

/* This function implements the GetMsg API call.
 */
PTH EXPORT FAR PASCAL GetMsg( PTH pth, DWORD dwMsg )
{
   if( !PVMsgHandle( pth, szFNPVGetMsg ) )
      return( FALSE );
   return( Amdb_GetMsg( pth, dwMsg ) );
}

/* This function implements the GetMsgInfo API call.
 */
void EXPORT FAR PASCAL GetMsgInfo( PTH pth, OLDMSGINFO FAR * lpomi )
{
   MSGINFO mi;

   if( !PVMsgHandle( pth, szFNPVGetMsgInfo ) )
      return;
   if( !PVDataAddress( lpomi, szFNPVGetMsgInfo ) )
      return;
   Amdb_GetMsgInfo( pth, &mi );
   lpomi->dwMsg = mi.dwMsg;
   lpomi->dwComment = mi.dwComment;
   lpomi->wFlags = LOWORD(mi.dwFlags);
   lpomi->wDate = mi.wDate;
   lpomi->wTime = mi.wTime;
   lpomi->cUnRead = mi.cUnRead;
   lpomi->wLevel = mi.wLevel;
   lpomi->cPriority = mi.cPriority;
   lpomi->wComments = mi.wComments;
   lpomi->cUnReadInThread = mi.cUnReadInThread;
   lpomi->dwActual = mi.dwActual;
   lstrcpyn( lpomi->szTitle, mi.szTitle, sizeof(lpomi->szTitle) );
   lstrcpyn( lpomi->szAuthor, mi.szAuthor, sizeof(lpomi->szAuthor) );
   lstrcpyn( lpomi->szReply, mi.szReply, sizeof(lpomi->szReply) );
}

/* This function implements the GetMsgText API call.
 */
HPSTR EXPORT FAR PASCAL GetMsgText( PTH pth )
{
   HPSTR hpStr;

   if( !PVMsgHandle( pth, szFNPVGetMsgText ) )
      return( FALSE );
   hpStr = Amdb_GetMsgText( pth );
   if( NULL != hpStr )
      {
      DWORD dwNewLength;
      HPSTR hpTruePos;
      HPSTR hpNewStr;

      INITIALISE_PTR(hpNewStr);

      /* Skip the dummy Ameol imposed header and create a
       * new block to include the new message sans header.
       */
      hpTruePos = hpStr;
      while( *hpTruePos && *hpTruePos != 13 && *hpTruePos != 10 )
         ++hpTruePos;
      if( *hpTruePos == 13 )
         ++hpTruePos;
      if( *hpTruePos == 10 )
         ++hpTruePos;
      dwNewLength = hstrlen( hpTruePos );
      if( fNewMemory32( &hpNewStr, dwNewLength + 1 ) )
         {
         hstrcpy( hpNewStr, hpTruePos );
         FreeMemory32( &hpStr );
         hpStr = hpNewStr;
         }
      }
   return( hpStr );
}

/* This function implements the GetMsgText API call.
 */
HPSTR EXPORT FAR PASCAL GetMsgTextEx( PTH pth )
{
   if( !PVMsgHandle( pth, szFNPVGetMsgText ) )
      return( FALSE );
   return( Amdb_GetMsgText( pth ) );
}

/* This function implements the IsRead API call.
 */
BOOL EXPORT FAR PASCAL IsRead( PTH pth )
{
   if( !PVMsgHandle( pth, szFNPVIsRead ) )
      return( FALSE );
   return( Amdb_IsRead( pth ) );
}

/* This function implements the MarkMsgRead API call.
 */
void EXPORT FAR PASCAL MarkMsgRead( PTH pth, BOOL fRead )
{
   if( !PVMsgHandle( pth, szFNPVMarkMsgRead ) )
      return;
   Amdb_MarkMsgRead( pth, fRead );
}

/* This function implements the IsTagged API call.
 */
BOOL EXPORT FAR PASCAL IsTagged( PTH pth )
{
   if( !PVMsgHandle( pth, szFNPVIsTagged ) )
      return( FALSE );
   return( Amdb_IsTagged( pth ) );
}

/* This function implements the MarkMsgTagged API call.
 */
void EXPORT FAR PASCAL MarkMsgTagged( PTH pth, BOOL fTagged )
{
   if( !PVMsgHandle( pth, szFNPVMarkMsgTagged ) )
      return;
   Amdb_MarkMsgTagged( pth, fTagged );
}
/* This function implements the IsUnavailable API call.
 */
BOOL EXPORT FAR PASCAL IsUnavailable( PTH pth )
{
   if( !PVMsgHandle( pth, szFNPVIsUnavailable ) )
      return( FALSE );
   return( Amdb_IsMsgUnavailable( pth ) );
}

/* This function implements the MarkMsgUnavailable API call.
 */
void EXPORT FAR PASCAL MarkMsgUnavailable( PTH pth, BOOL fTrue )
{
   if( !PVMsgHandle( pth, szFNPVMarkMsgUnavailable ) )
      return;
   Amdb_MarkMsgUnavailable( pth, fTrue );
}

/* This function implements the IsWatch API call.
 */
BOOL EXPORT FAR PASCAL IsWatch( PTH pth )
{
   if( !PVMsgHandle( pth, szFNPVIsWatch ) )
      return( FALSE );
   return( Amdb_IsWatched( pth ) );
}

/* This function implements the MarkMsgWatch API call.
 */
void EXPORT FAR PASCAL MarkMsgWatch( PTH pth, BOOL fWatch )
{
   if( !PVMsgHandle( pth, szFNPVMarkMsgWatch ) )
      return;
   Amdb_MarkMsgWatch( pth, fWatch, TRUE );
}

/* This function implements the IsBeingRetrieved API call.
 */
BOOL EXPORT FAR PASCAL IsBeingRetrieved( PTH pth )
{
   if( !PVMsgHandle( pth, szFNPVIsBeingRetrieved ) )
      return( FALSE );
   return( Amdb_IsBeingRetrieved( pth ) );
}

/* This function implements the MarkMsgBeingRetrieved API call.
 */
void EXPORT FAR PASCAL MarkMsgBeingRetrieved( PTH pth, BOOL fTrue )
{
   if( !PVMsgHandle( pth, szFNPVMarkMsgBeingRetrieved ) )
      return;
   Amdb_MarkMsgBeingRetrieved( pth, fTrue );
}

/* This function implements the IsKept API call.
 */
BOOL EXPORT FAR PASCAL IsKept( PTH pth )
{
   if( !PVMsgHandle( pth, szFNPVIsKept ) )
      return( FALSE );
   return( Amdb_IsKept( pth ) );
}

/* This function implements the MarkMsgKeep API call.
 */
void EXPORT FAR PASCAL MarkMsgKeep( PTH pth, BOOL fKeep )
{
   if( !PVMsgHandle( pth, szFNPVMarkMsgKeep ) )
      return;
   Amdb_MarkMsgKeep( pth, fKeep );
}

/* This function implements the IsMarked API call.
 */
BOOL EXPORT FAR PASCAL IsMarked( PTH pth )
{
   if( !PVMsgHandle( pth, szFNPVIsMarked ) )
      return( FALSE );
   return( Amdb_IsMarked( pth ) );
}

/* This function implements the MarkMsg API call.
 */
void EXPORT FAR PASCAL MarkMsg( PTH pth, BOOL fMark )
{
   if( !PVMsgHandle( pth, szFNPVMarkMsg ) )
      return;
   Amdb_MarkMsg( pth, fMark );
}

/* This function implements the IsMarked API call.
 */
BOOL EXPORT FAR PASCAL IsHdrOnly( PTH pth )
{
   if( !PVMsgHandle( pth, szFNPVIsHdrOnly ) )
      return( FALSE );
   return( Amdb_IsHeaderMsg( pth ) );
}

/* This function implements the MarkMsg API call.
 */
void EXPORT FAR PASCAL MarkMsgHdrOnly( PTH pth, BOOL fMark )
{
   if( !PVMsgHandle( pth, szFNPVMarkMsgHdrOnly ) )
      return;
   Amdb_MarkHdrOnly( pth, fMark );
}

/* This function implements the IsAttachment API call.
 */
BOOL EXPORT FAR PASCAL IsAttachment( PTH pth )
{
   if( !PVMsgHandle( pth, szFNPVIsAttachment ) )
      return( FALSE );
   return( Amdb_IsMsgHasAttachments( pth ) );
}

/* This function implements the GetSetPurgeOptions API call.
 */
BOOL EXPORT FAR PASCAL GetSetPurgeOptions( PTL ptl, OLDPURGEOPTIONS FAR * lpopo, BOOL fSet )
{
   if( ptl )
      if( !PVTopicHandle( ptl, szFNPVGetSetPurgeOptions ) )
         return( FALSE );
   if( !PVDataAddress( lpopo, szFNPVGetSetPurgeOptions ) )
      return( FALSE );
   if( fSet )
      {
      PURGEOPTIONS po;

      po.wFlags = lpopo->wFlags;
      po.wMaxDate = lpopo->wMaxDate;
      po.wMaxSize = lpopo->wMaxSize;
      Amdb_SetTopicPurgeOptions( ptl, &po );
      }
   else
      {
      TOPICINFO ti;

      Amdb_GetTopicInfo( ptl, &ti );
      lpopo->wFlags = ti.po.wFlags;
      lpopo->wMaxDate = ti.po.wMaxDate;
      lpopo->wMaxSize = ti.po.wMaxSize;
      }
   return( TRUE );
}

/* This function implements the StdInitDialog API call.
 */
BOOL EXPORT FAR PASCAL StdInitDialog( HWND hwnd, WPARAM wParam, LPARAM lParam )
{
   return( TRUE );
}

/* This function implements the StdCtlColor API call.
 */
BOOL EXPORT FAR PASCAL StdCtlColor( HWND hwnd, WPARAM wParam, LPARAM lParam )
{
   return( TRUE );
}

/* This function implements the MarkMsgDelete API call.
 */
void EXPORT FAR PASCAL MarkMsgDelete( PTH pth, BOOL fDelete )
{
   if( !PVMsgHandle( pth, szFNPVMarkMsgDelete ) )
      return;
   Amdb_MarkMsgDelete( pth, fDelete );
}

/* This function implements the IsDeleted API call.
 */
BOOL EXPORT FAR PASCAL IsDeleted( PTH pth )
{
   if( !PVMsgHandle( pth, szFNPVIsDeleted ) )
      return( FALSE );
   return( Amdb_IsDeleted( pth ) );
}

/* This function implements the FreeMsgTextBuffer API call.
 */
void EXPORT FAR PASCAL FreeMsgTextBuffer( HPSTR hpMsgText )
{
   if( !PVDataAddress( hpMsgText, szFNPVFreeMsgTextBuffer ) )
      return;
   Amdb_FreeMsgTextBuffer( hpMsgText );
}

/* This function implements the DeleteMsg API call.
 */
void EXPORT FAR PASCAL DeleteMsg( PTH pth )
{
   if( !PVMsgHandle( pth, szFNPVDeleteMsg ) )
      return;
   Amdb_DeleteMsg( pth, TRUE );
}

/* This function implements the MarkMsgIgnore API call.
 */
void EXPORT FAR PASCAL MarkMsgIgnore( PTH pth, BOOL fIgnore )
{
   if( !PVMsgHandle( pth, szFNPVMarkMsgIgnore ) )
      return;
   Amdb_MarkMsgIgnore( pth, fIgnore, TRUE );
}

/* This function implements the IsIgnored API call.
 */
BOOL EXPORT FAR PASCAL IsIgnored( PTH pth )
{
   if( !PVMsgHandle( pth, szFNPVIsIgnored ) )
      return( FALSE );
   return( Amdb_IsIgnored( pth ) );
}

/* This function implements the LockTopic API call.
 */
void EXPORT FAR PASCAL LockTopic( PTL ptl )
{
   if( !PVTopicHandle( ptl, szFNPVLockTopic ) )
      return;
   Amdb_LockTopic( ptl );
}

/* This function implements the UnlockTopic API call.
 */
void EXPORT FAR PASCAL UnlockTopic( PTL ptl )
{
   if( !PVTopicHandle( ptl, szFNPVUnlockTopic ) )
      return;
   Amdb_UnlockTopic( ptl );
}

/* This function implements the GetAmeolVersion API call.
 */
DWORD EXPORT FAR PASCAL GetAmeolVersion( void )
{
   return( CvtToAmeolVersion( Ameol2_GetVersion() ) );
}

/* This function implements the GetCurrentMsg API call.
 */
BOOL EXPORT FAR PASCAL GetCurrentMsg( OLDCURMSG FAR * lpunr )
{
   CURMSG cm;

   if( !PVDataAddress( lpunr, szFNPVGetCurrentMsg ) )
      return( FALSE );
   if( Ameol2_GetCurrentMsg( &cm ) )
      {
      lpunr->pcl = cm.pcl;
      lpunr->ptl = cm.ptl;
      lpunr->pth = cm.pth;
      return( TRUE );
      }
   return( FALSE );
}

/* This function implements the SetCurrentMsg API call.
 */
BOOL EXPORT FAR PASCAL SetCurrentMsg( OLDCURMSG FAR * lpunr )
{
   CURMSG cm;

   if( !PVDataAddress( lpunr, szFNPVSetCurrentMsg ) )
      return( FALSE );
   cm.pcat = NULL;
   cm.pcl = lpunr->pcl;
   cm.ptl = lpunr->ptl;
   cm.pth = lpunr->pth;
   return( Ameol2_SetCurrentMsg( &cm ) );
}

/* This function implements the GetAmeolWindows API call.
 */
void EXPORT FAR PASCAL GetAmeolWindows( HWND FAR * lphwnd )
{
   if( !PVDataAddress( (LPVOID)lphwnd, szFNPVGetAmeolWindows ) )
      return;
   Ameol2_GetWindows( lphwnd );
}

/* This function implements the GetRegistry API call.
 */
int EXPORT FAR PASCAL GetRegistry( LPSTR lpszLogin )
{
   if( !PVDataAddress( lpszLogin, szFNPVGetRegistry ) )
      return( 0 );
   if( strcmp( lpszLogin, "$CIX$" ) == 0 )
      return( Ameol2_GetSystemParameter( "CIX\\User", lpszLogin, 40 ) );
   return( Amuser_GetActiveUser( lpszLogin, 40 ) );
}

/* This function implements the StdEndDialog API call.
 */
void EXPORT FAR PASCAL StdEndDialog( HWND hwnd, int nCode )
{
   EndDialog( hwnd, nCode );
}

/* This function implements the GetUtilityMenuID API call.
 */
WORD EXPORT FAR PASCAL GetUtilityMenuID( HINSTANCE hLib )
{
   return( (WORD)-1 );
}

/* This function implements the StdDialogBox API call.
 */
int EXPORT FAR PASCAL StdDialogBox( HINSTANCE hInst, LPCSTR psDlgName, HWND hwnd, DLGPROC lfpDlgProc )
{
   return( Adm_Dialog( hInst, hwnd, psDlgName, lfpDlgProc, 0L ) );
}

/* This function implements the StdDialogBoxParam API call.
 */
int EXPORT FAR PASCAL StdDialogBoxParam( HINSTANCE hInst, LPCSTR psDlgName, HWND hwnd, DLGPROC lfpDlgProc, LPARAM lParam )
{
   return( Adm_Dialog( hInst, hwnd, psDlgName, lfpDlgProc, lParam ) );
}

/* This function implements the GetAmeolFont API call.
 */
HFONT EXPORT FAR PASCAL GetAmeolFont( int index )
{
   return( Ameol2_GetStockFont( index ) );
}

/* This function implements the CreateAttachment API call.
 */
BOOL EXPORT FAR PASCAL CreateAttachment( PTH pth, LPSTR lpszFilename )
{
   return( Amdb_CreateAttachment( pth, lpszFilename ) );
}

/* This function implements the GetSetModeratorList API call.
 */
LPSTR EXPORT FAR PASCAL GetSetModeratorList( PCL pcl, LPSTR lp, BOOL fSet )
{
   if( !PVConferenceHandle( pcl, szFNPVGetSetModeratorList ) )
      return( FALSE );
   if( lp != NULL && !PVDataAddress( lp, szFNPVGetSetModeratorList ) )
      return( FALSE );
   if( fSet )
      return( Amdb_SetModeratorList( pcl, lp ) );
   if( NULL == lp )
      return( NULL );
   return( Amdb_GetModeratorList( pcl, lp, LEN_MODERATORS ) ? lp : NULL );
}

/* This function implements the GetLastConference API call.
 */
PCL EXPORT FAR PASCAL GetLastConference( void )
{
   return( Amdb_GetLastFolder( Amdb_GetLastCategory() ) );
}

/* This function implements the StdMDIDialogBox API call.
 */
void EXPORT FAR PASCAL StdMDIDialogBox( HWND hwnd, LPCSTR lpszDlgName, LPMDICREATESTRUCT lpMDICreateStruct )
{
   Adm_MDIDialog( hwnd, lpszDlgName, lpMDICreateStruct );
}

/* This function implements the DefAmeolMDIDlgProc API call.
 */
LRESULT EXPORT FAR PASCAL DefAmeolMDIDlgProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   return( Ameol2_DefMDIDlgProc( hwnd, msg, wParam, lParam ) );
}

/* This function implements the StdMDIDialogBoxFrame API call.
 */
HWND EXPORT FAR PASCAL StdMDIDialogBoxFrame( HINSTANCE hInst, LPCSTR lpszClassName, int x, int y, int cx, int cy )
{
   RECT rc;

   SetRect( &rc, x, y, x + cx, y + cy );
   return( Adm_CreateMDIWindow( "", lpszClassName, hInst, &rc, 0, 0L ) );
}

/* This function implements the StdEndMDIDialog API call.
 */
void EXPORT FAR PASCAL StdEndMDIDialog( HWND hwnd )
{
   Adm_DestroyMDIWindow( hwnd );
}

/* This function implements the StdOpenMDIDialog API call.
 */
void EXPORT FAR PASCAL StdOpenMDIDialog( HWND hwnd )
{
   Adm_MakeMDIWindowActive( hwnd );
}

/* This function implements the AmOpenFile API call.
 */
HFILE EXPORT FAR PASCAL AmOpenFile( LPSTR lpszFileName, WORD wType, WORD wMode )
{
   if( !PVDataAddress( lpszFileName, szFNPVAmOpenFile ) )
      return( -1 );
   return( (HFILE)Ameol2_OpenFile( lpszFileName, wType, wMode ) );
}

/* This function implements the AmCreateFile API call.
 */
HFILE EXPORT FAR PASCAL AmCreateFile( LPSTR lpszFileName, WORD wType, WORD wMode )
{
   if( !PVDataAddress( lpszFileName, szFNPVAmCreateFile ) )
      return( -1 );
   return( (HFILE)Ameol2_CreateFile( lpszFileName, wType, wMode ) );
}

/* This function implements the AmRenameFile API call.
 */
BOOL EXPORT FAR PASCAL AmRenameFile( LPSTR lpszOldName, LPSTR lpszNewName, WORD wType )
{
   if( !PVDataAddress( lpszOldName, szFNPVAmRenameFile ) )
      return( FALSE );
   if( !PVDataAddress( lpszNewName, szFNPVAmRenameFile ) )
      return( FALSE );
   return( Ameol2_RenameFile( lpszOldName, lpszNewName, wType ) );
}

/* This function implements the AmQueryFileExists API call.
 */
BOOL EXPORT FAR PASCAL AmQueryFileExists( LPSTR lpszFileName, WORD wType )
{
   if( !PVDataAddress( lpszFileName, szFNPVAmQueryFileExists ) )
      return( FALSE );
   return( Ameol2_QueryFileExists( lpszFileName, wType ) );
}

/* This function implements the AmDeleteFile API call.
 */
BOOL EXPORT FAR PASCAL AmDeleteFile( LPSTR lpszFileName, WORD wType )
{
   if( !PVDataAddress( lpszFileName, szFNPVAmDeleteFile ) )
      return( FALSE );
   return( Ameol2_DeleteFile( lpszFileName, wType ) );
}

/* This function implements the MarkMsgWithdrawn API call.
 */
BOOL EXPORT FAR PASCAL MarkMsgWithdrawn( PTH pth, BOOL fWithdrawn )
{
   if( !PVMsgHandle( pth, szFNPVMarkMsgWithdrawn ) )
      return( FALSE );
   return( Amdb_MarkMsgWithdrawn( pth, fWithdrawn ) );
}

/* This function implements the IsWithdrawn API call.
 */
BOOL EXPORT FAR PASCAL IsWithdrawn( PTH pth )
{
   if( !PVMsgHandle( pth, szFNPVIsWithdrawn ) )
      return( FALSE );
   return( Amdb_IsWithdrawn( pth ) );
}

/* This function implements the AddEventLogItem API call.
 */
BOOL EXPORT FAR PASCAL AddEventLogItem( WORD wType, LPSTR lpszText )
{
   if( !PVDataAddress( lpszText, szFNPVAddEventLogItem ) )
      return( FALSE );
   return( Amevent_AddEventLogItem( wType, lpszText ) );
}

/* This function implements the GetConferenceInfo API call.
 */
void EXPORT FAR PASCAL GetConferenceInfo( PCL pcl, OLDCONFERENCEINFO FAR * lpoci )
{
   FOLDERINFO fi;

   if( !PVConferenceHandle( pcl, szFNPVGetConferenceInfo ) )
      return;
   if( !PVDataAddress( lpoci, szFNPVGetConferenceInfo ) )
      return;
   Amdb_GetFolderInfo( pcl, &fi );
   lstrcpyn( lpoci->szConfName, fi.szFolderName, sizeof( lpoci->szConfName ) );
   lstrcpyn( lpoci->szFileName, fi.szFileName, sizeof( lpoci->szFileName ) );
   lpoci->cMsgs = fi.cMsgs;
   lpoci->cUnRead = fi.cUnRead;
   lpoci->cTopics = fi.cTopics;
   lpoci->wTime = fi.wCreationTime;
   lpoci->wDate = fi.wCreationDate;
}

/* This function implements the CreateOSCompatibleFileName API call.
 */
BOOL EXPORT FAR PASCAL CreateOSCompatibleFileName( LPSTR lpszHostFName, LPSTR lpszOSFName )
{
   return( Amuser_CreateCompatibleFilename( (LPCSTR)lpszHostFName, lpszOSFName ) );
}

/* This function implements the GetAddressBook API call.
 */
BOOL EXPORT FAR PASCAL GetAddressBook( LPSTR lpszCixName, ADDRINFO FAR * lpAddrInfo )
{
   if( !PVDataAddress( lpszCixName, szFNPVGetAddressBook ) )
      return( FALSE );
   if( !PVDataAddress( lpAddrInfo, szFNPVGetAddressBook ) )
      return( FALSE );
   return( Amaddr_GetEntry( lpszCixName, lpAddrInfo ) );
}

/* This function implements the SetAddressBook API call.
 */
BOOL EXPORT FAR PASCAL SetAddressBook( LPSTR lpszFullName, ADDRINFO FAR * lpAddrInfo )
{
   if( lpszFullName )
      if( !PVDataAddress( lpszFullName, szFNPVSetAddressBook ) )
         return( FALSE );
   if( lpAddrInfo )
      if( !PVDataAddress( lpAddrInfo, szFNPVSetAddressBook ) )
         return( FALSE );
   return( Amaddr_SetEntry( lpszFullName, lpAddrInfo ) );
}

/* This function implements the GetAddressBookGroup API call.
 */
int EXPORT FAR PASCAL GetAddressBookGroup( LPSTR lpszGroupName, char FAR * FAR lpGroupList[] )
{
   if( !PVDataAddress( lpszGroupName, szFNPVGetAddressBookGroup ) )
      return( FALSE );
   return( Amaddr_GetGroup( lpszGroupName, lpGroupList ) );
}

/* This function implements the CreateAddressBookGroup API call.
 */
BOOL EXPORT FAR PASCAL CreateAddressBookGroup( LPSTR lpszGroupName )
{
   if( !PVDataAddress( lpszGroupName, szFNPVCreateAddressBookGroup ) )
      return( FALSE );
   return( Amaddr_CreateGroup( lpszGroupName ) );
}

/* This function implements the DeleteAddressBookGroup API call.
 */
BOOL EXPORT FAR PASCAL DeleteAddressBookGroup( LPSTR lpszGroupName )
{
   if( !PVDataAddress( lpszGroupName, szFNPVDeleteAddressBookGroup ) )
      return( FALSE );
   return( Amaddr_DeleteGroup( lpszGroupName ) );
}

/* This function implements the AddAddressBookGroupItem API call.
 */
BOOL EXPORT FAR PASCAL AddAddressBookGroupItem( LPSTR lpszGroupName, LPSTR lpszUserName )
{
   if( !PVDataAddress( lpszGroupName, szFNPVAddAddressBookGroupItem ) )
      return( FALSE );
   if( !PVDataAddress( lpszUserName, szFNPVAddAddressBookGroupItem ) )
      return( FALSE );
   return( Amaddr_AddToGroup( lpszGroupName, lpszUserName ) );
}

/* This function implements the SetTopicFlags API call.
 */
void EXPORT FAR PASCAL SetTopicFlags( PTL ptl, WORD wFlags, BOOL fSet )
{
   if( !PVTopicHandle( ptl, szFNPVSetTopicFlags ) )
      return;
   if( ( wFlags & TF_LOCAL ) && Amdb_GetTopicType( ptl ) != FTYPE_MAIL )
      {
      Amdb_SetTopicType( ptl, FTYPE_LOCAL );
      wFlags &= ~TF_LOCAL;
      }
   if( ( wFlags & TF_LOCAL ) && Amdb_GetTopicType( ptl ) == FTYPE_MAIL )
      wFlags &= ~TF_LOCAL;
   Amdb_SetTopicFlags( ptl, wFlags, fSet );
}

/* This function implements the MarkMsgPriority API call.
 */
void EXPORT FAR PASCAL MarkMsgPriority( PTH pth, BOOL fPriority )
{
   if( !PVMsgHandle( pth, szFNPVMarkMsgPriority ) )
      return;
   Amdb_MarkMsgPriority( pth, fPriority, TRUE );
}

/* This function implements the IsPriority API call.
 */
BOOL EXPORT FAR PASCAL IsPriority( PTH pth )
{
   if( !PVMsgHandle( pth, szFNPVIsPriority ) )
      return( FALSE );
   return( Amdb_IsPriority( pth ) );
}

/* This function implements the ChangeDirectory API call.
 */
BOOL EXPORT FAR PASCAL ChangeDirectory( LPCHGDIR lpChgDir )
{
   if( !PVDataAddress( lpChgDir, szFNPVChangeDirectory ) )
      return( FALSE );
   return( Amuser_ChangeDirectory( lpChgDir ) );
}

/* This function implements the StdCreateDialog API call.
 */
HWND EXPORT FAR PASCAL StdCreateDialog( HINSTANCE hInst, LPCSTR psDlgName, HWND hwnd, DLGPROC lpfDlgProc )
{
   return( Adm_CreateDialog( hInst, hwnd, psDlgName, lpfDlgProc, 0 ) );
}

/* This function implements the StdCreateDialogParam API call.
 */
HWND EXPORT FAR PASCAL StdCreateDialogParam( HINSTANCE hInst, LPCSTR psDlgName, HWND hwnd, DLGPROC lpfDlgProc, LPARAM lParam )
{
   return( Adm_CreateDialog( hInst, hwnd, psDlgName, lpfDlgProc, lParam ) );
}

/* This function implements the EnumAllTools API call.
 */
LPVOID EXPORT FAR PASCAL EnumAllTools( LPVOID lpTool, OLDTOOLINFO FAR * lpoti )
{
   ADDONINFO ai;

   if( lpTool )
      if( !PVDataAddress( lpTool, szFNPVEnumAllTools ) )
         return( NULL );
   if( !PVDataAddress( lpoti, szFNPVEnumAllTools ) )
      return( NULL );
   lpTool = Ameol2_EnumAllTools( lpTool, &ai );
   lpoti->dwVersion = ai.dwVersion;
   lstrcpyn( lpoti->szFileName, ai.szFileName, sizeof(lpoti->szFileName) );
   lstrcpyn( lpoti->szAuthor, ai.szAuthor, sizeof(lpoti->szAuthor) );
   lstrcpyn( lpoti->szDescription, ai.szDescription, sizeof(lpoti->szDescription) );
   return( lpTool );
}

/* This function implements the ExpandVersion API call.
 */
LPSTR EXPORT FAR PASCAL ExpandVersion( DWORD dwVersion )
{
   return( Ameol2_ExpandAddonVersion( dwVersion ) );
}

/* This function implements the InsertAmeolMenu API call.
 */
void EXPORT FAR PASCAL InsertAmeolMenu( AMEOLMENU FAR * lpam )
{
   if( !PVDataAddress( lpam, szFNPVInsertAmeolMenu ) )
      return;
   Ameol2_InsertMenu( lpam );
}

/* This function implements the DeleteAmeolMenu API call.
 */
void EXPORT FAR PASCAL DeleteAmeolMenu( HMENU hMenu, WORD iCommand )
{
   if( NULL != hMenu )
      if( !PVMenuHandle( hMenu, szFNPVDeleteAmeolMenu ) )
         return;
   Ameol2_DeleteMenu( hMenu, iCommand );
}

/* This function implements the IsModerator API call.
 */
BOOL EXPORT FAR PASCAL IsModerator( PCL pcl, LPSTR lpszUserName )
{
   char szNickname[ 16 ];

   if( !PVConferenceHandle( pcl, szFNPVIsModerator ) )
      return( FALSE );
   if( lpszUserName )
      if( !PVDataAddress( lpszUserName, szFNPVIsModerator ) )
         return( FALSE );
   if( NULL == lpszUserName )
      {
      Ameol2_GetCixNickname( szNickname );
      lpszUserName = szNickname;
      }
   return( Amdb_IsModerator( pcl, lpszUserName ) );
}

/* This function implements the IsAmeolQuitting API call.
 */
BOOL EXPORT FAR PASCAL IsAmeolQuitting( void )
{
   return( Ameol2_IsAmeolQuitting() );
}

/* This function implements the GetInitialisationFile API call.
 */
int EXPORT FAR PASCAL GetInitialisationFile( LPSTR lpszFileName )
{
   if( !PVDataAddress( lpszFileName, szFNPVGetInitialisationFile ) )
      return( 0 );
   Amuser_GetActiveUserDirectory( lpszFileName, _MAX_DIR );
   lstrcat( lpszFileName, "\\" );
   Amuser_GetActiveUser( lpszFileName + lstrlen(lpszFileName), 9 );
   lstrcat( lpszFileName, ".INI" );
   return( lstrlen( lpszFileName ) );
}

/* This function implements the GetMsgTextLength API call.
 */
DWORD EXPORT FAR PASCAL GetMsgTextLength( PTH pth )
{
   if( !PVMsgHandle( pth, szFNPVGetMsgTextLength ) )
      return( 0L );
   return( Amdb_GetMsgTextLength( pth ) );
}

/* This function implements the GetMarked API call.
 */
PTH EXPORT FAR PASCAL GetMarked( PTL ptlv, PTH pth, int nMode, BOOL fFwd )
{
   if( !PVTopicHandle( ptlv, szFNPVGetMarked ) )
      return( FALSE );
   if( pth )
      if( !PVMsgHandle( pth, szFNPVGetMarked ) )
         return( FALSE );
   return( Amdb_GetMarked( ptlv, pth, nMode, fFwd ) );
}

/* This function implements the GetNextEventLogItem API call.
 */
int EXPORT FAR PASCAL GetNextEventLogItem( WORD wType, int index, EVENTLOGITEM FAR * lpeli )
{
   if( !PVDataAddress( lpeli, szFNPVGetNextEventLogItem ) )
      return( FALSE );
   return( Amevent_GetNextEventLogItem( wType, index, lpeli ) );
}

/* This function implements the MarkMsgReadLock API call.
 */
void EXPORT FAR PASCAL MarkMsgReadLock( PTH pth, BOOL fReadLock )
{
   if( !PVMsgHandle( pth, szFNPVMarkMsgReadLock ) )
      return;
   Amdb_MarkMsgReadLock( pth, fReadLock );
}

/* This function implements the IsReadLock API call.
 */
BOOL EXPORT FAR PASCAL IsReadLock( PTH pth )
{
   if( !PVMsgHandle( pth, szFNPVIsReadLock ) )
      return( FALSE );
   return( Amdb_IsReadLock( pth ) );
}

/* This function implements the AmCommIdentify API call.
 */
LPSTR EXPORT FAR PASCAL AmCommIdentify( void )
{
   static COMMQUERYSTRUCT cqs;

   cqs.cbStructSize = sizeof(COMMQUERYSTRUCT);
   Amcomm_Query( NULL, &cqs );
   return( cqs.szCommDevice );
}

/* This function implements the ExpandAmeolVersion API call.
 */
LPSTR EXPORT FAR PASCAL ExpandAmeolVersion( DWORD dwVersion )
{
   return( Ameol2_ExpandVersion( Ameol2_GetVersion() ) );
}

/* This function implements the GetAmeolCodename API call.
 */
LPSTR EXPORT FAR PASCAL GetAmeolCodename( void )
{
   return( Ameol2_GetAmeolCodename() );
}

/* This function implements the SpellCheckDocument API call.
 */
int EXPORT FAR PASCAL SpellCheckDocument( HWND hwnd, HWND hwndEdit, BOOL fPrompt )
{
   return( Ameol2_SpellCheckDocument( hwnd, hwndEdit, fPrompt ) );
}

/* This function implements the CustomConnect API call.
 */
void EXPORT FAR PASCAL CustomConnect( void )
{
}

/* This function implements the SetAmeolMenuBar API call.
 */
void EXPORT FAR PASCAL SetAmeolMenuBar( int idMenu )
{
   /* Do nothing - not supported under Ameol */
}

/* This function implements the SetConferenceSignature API call.
 */
void EXPORT FAR PASCAL SetConferenceSignature( PCL pcl, LPSTR lpszFileName )
{
   Amdb_SetFolderSignature( pcl, lpszFileName );
}

/* This function implements the GetConferenceSignature API call.
 */
void EXPORT FAR PASCAL GetConferenceSignature( PCL pcl, LPSTR lpszFileName )
{
   Amdb_GetFolderSignature( pcl, lpszFileName );
}

/* This function implements the SetConferenceName API call.
 */
void EXPORT FAR PASCAL SetConferenceName( PCL pcl, LPCSTR lpszName )
{
   Amdb_SetFolderName( pcl, lpszName );
}

/* This function implements the AmWritePrivateProfileString API call.
 */
BOOL EXPORT FAR PASCAL AmWritePrivateProfileString( LPSTR lpApplicationName, LPSTR lpKeyName, LPSTR lpDefBuf, LPSTR lpszIniFile )
{
   return( Amuser_WritePrivateProfileString( lpApplicationName, lpKeyName, lpDefBuf, lpszIniFile ) );
}

/* This function implements the AmWritePrivateProfileInt API call.
 */
BOOL EXPORT FAR PASCAL AmWritePrivateProfileInt( LPSTR lpApplicationName, LPSTR lpKeyName, int nValue, LPSTR lpszIniFile )
{
   return( Amuser_WritePrivateProfileInt( lpApplicationName, lpKeyName, nValue, lpszIniFile ) );
}

/* This function implements the AmWritePrivateProfileFloat API call.
 */
BOOL EXPORT FAR PASCAL AmWritePrivateProfileFloat( LPSTR lpApplicationName, LPSTR lpKeyName, float nValue, LPSTR lpszIniFile )
{
   return( Amuser_WritePrivateProfileFloat( lpApplicationName, lpKeyName, nValue, lpszIniFile ) );
}

/* This function implements the AmWritePrivateProfileLong API call.
 */
BOOL EXPORT FAR PASCAL AmWritePrivateProfileLong( LPSTR lpApplicationName, LPSTR lpKeyName, long lValue, LPSTR lpszIniFile )
{
   return( Amuser_WritePrivateProfileLong( lpApplicationName, lpKeyName, lValue, lpszIniFile ) );
}

/* This function implements the AmGetPrivateProfileString API call.
 */
int EXPORT FAR PASCAL AmGetPrivateProfileString( LPSTR lpApplicationName, LPSTR lpKeyName, LPSTR lpDefBuf, LPSTR lpBuf, int cbBuf, LPSTR lpszIniFile )
{
   return( Amuser_GetPrivateProfileString( lpApplicationName, lpKeyName, lpDefBuf, lpBuf, cbBuf, lpszIniFile ) );
}

/* This function implements the AmGetPrivateProfileInt API call.
 */
int EXPORT FAR PASCAL AmGetPrivateProfileInt( LPSTR lpApplicationName, LPSTR lpKeyName, int nDefInt, LPSTR lpszIniFile )
{
   return( Amuser_GetPrivateProfileInt( lpApplicationName, lpKeyName, nDefInt, lpszIniFile ) );
}

/* This function implements the AmGetPrivateProfileFloat API call.
 */
float EXPORT FAR PASCAL AmGetPrivateProfileFloat( LPSTR lpApplicationName, LPSTR lpKeyName, float nDefValue, LPSTR lpszIniFile )
{
   return( Amuser_GetPrivateProfileFloat( lpApplicationName, lpKeyName, nDefValue, lpszIniFile ) );
}

/* This function implements the AmGetPrivateProfileLong API call.
 */
LONG EXPORT FAR PASCAL AmGetPrivateProfileLong( LPSTR lpApplicationName, LPSTR lpKeyName, long lDefValue, LPSTR lpszIniFile )
{
   return( Amuser_GetPrivateProfileLong( lpApplicationName, lpKeyName, lDefValue, lpszIniFile ) );
}

/* This function implements the GetSetOutboxMsgBit API call.
 */
BOOL EXPORT FAR PASCAL GetSetOutboxMsgBit( PTH pth, int nSet )
{
   if( nSet == BSB_SETBIT || nSet == BSB_CLRBIT )
      return( Amdb_SetOutboxMsgBit( pth, nSet == BSB_SETBIT ) );
   return( Amdb_GetOutboxMsgBit( pth ) );
}

/* This function implements the GetSetCoverNoteBit API call.
 */
BOOL EXPORT FAR PASCAL GetSetCoverNoteBit( PTH pth, int nSet )
{
   return( Amdb_GetSetCoverNoteBit( pth, nSet ) );
}

/* This function implements the GetSetCoverNoteBit API call.
 */
BOOL EXPORT FAR PASCAL GetSetNotificationBit( PTH pth, int nSet )
{
   return( Amdb_GetSetNotificationBit( pth, nSet ) );
}

/* This function implements the GetCurrentDate API call.
 */
void EXPORT FAR PASCAL GetCurrentDate( AM_DATE FAR * pDate )
{
   Amdate_GetCurrentDate( pDate );
}

/* This function implements the GetPackedCurrentDate API call.
 */
WORD EXPORT FAR PASCAL GetPackedCurrentDate( void )
{
   return( Amdate_GetPackedCurrentDate() );
}

/* This function implements the PackDate API call.
 */
WORD EXPORT FAR PASCAL PackDate( AM_DATE FAR * pDate )
{
   return( Amdate_PackDate( pDate ) );
}

/* This function implements the UnpackDate API call.
 */
void EXPORT FAR PASCAL UnpackDate( WORD nDate, AM_DATE FAR * pDate )
{
   Amdate_UnpackDate( nDate, pDate );
}

/* This function implements the GetPackedCurrentTime API call.
 */
WORD EXPORT FAR PASCAL GetPackedCurrentTime( void )
{
   return( Amdate_GetPackedCurrentTime() );
}

/* This function implements the PackTime API call.
 */
WORD EXPORT FAR PASCAL PackTime( AM_TIME FAR * pTime )
{
   return( Amdate_PackTime( pTime ) );
}

/* This function implements the UnpackTime API call.
 */
void EXPORT FAR PASCAL UnpackTime( WORD nTime, AM_TIME FAR * pTime )
{
   Amdate_UnpackTime( nTime, pTime );
}

/* This function implements the CreateFaxBookEntry API call.
 */
BOOL EXPORT FAR PASCAL CreateFaxBookEntry( LPSTR lpszFullName, LPSTR lpszFaxNo )
{
   return( Amaddr_CreateFaxBookEntry( lpszFullName, lpszFaxNo ) );
}

/* This function implements the DeleteFaxBookEntry API call.
 */
BOOL EXPORT FAR PASCAL DeleteFaxBookEntry( LPSTR lpszFullName, LPSTR lpszFaxNo )
{
   return( Amaddr_DeleteFaxBookEntry( lpszFullName, lpszFaxNo ) );
}

/* This function implements the GetNextAddressBook API call.
 */
HADDRBOOK EXPORT FAR PASCAL GetNextAddressBook( HADDRBOOK hEntry, ADDRINFO FAR * lpAddrInfo )
{
   return( Amaddr_GetNextEntry( hEntry, lpAddrInfo ) );
}

/* This function implements the GetNextAddressBookGroup API call.
 */
HADDRBOOK EXPORT FAR PASCAL GetNextAddressBookGroup( HADDRBOOK hGroup, LPSTR lpszGroupName )
{
   if( !PVDataAddress( lpszGroupName, szFNPVGetNextAddressBookGroup ) )
      return( FALSE );
   return( Amaddr_GetNextGroup( hGroup, lpszGroupName ) );
}

/* This function implements the IsAddressBookGroup API call.
 */
BOOL EXPORT FAR PASCAL IsAddressBookGroup( LPSTR lpszGroupName )
{
   return( Amaddr_IsGroup( lpszGroupName ) );
}

/* This function implements the GetPriorityUnRead API call.
 */
PTH EXPORT FAR PASCAL GetPriorityUnRead( PTL ptlv, PTH pth, int nMode, BOOL fFwd )
{
   return( Amdb_GetPriorityUnRead( ptlv, pth, nMode, fFwd ) );
}


/* Scheduler
 */

int EXPORT FAR PASCAL Am2GetSchedulerInfo( LPSTR lpCommand, SCHEDULE * lpSched )
{
   return( Ameol2_GetSchedulerInfo( lpCommand, lpSched ) );
}

int EXPORT FAR PASCAL Am2SetSchedulerInfo( LPSTR lpCommand, SCHEDULE * lpSched )
{
   return( Ameol2_SetSchedulerInfo( lpCommand, lpSched ) );
}

int EXPORT FAR PASCAL Am2EditSchedulerInfo( HWND hwnd, LPSTR lpCommand )
{
   return( Ameol2_EditSchedulerInfo( hwnd, lpCommand ) );
}

BOOL EXPORT FAR PASCAL Am2UnloadAddon( LPADDON lpaddon )
{
   return( Ameol2_UnloadAddon( lpaddon ) );
}

LPADDON EXPORT FAR PASCAL Am2LoadAddon( LPSTR lpszFilename )
{
   return( Ameol2_LoadAddon( lpszFilename ) );
}

void EXPORT FAR PASCAL Am2StatusMessage( LPSTR lpszMessage )
{
   Ameol2_OnlineStatusMessage( lpszMessage );
}

void EXPORT FAR PASCAL Am2ConnectionInUse( void )
{
   Ameol2_ConnectionInUse();
}

void EXPORT FAR PASCAL Am2ConnectionFinished( void )
{
   Ameol2_ConnectionFinished();
}

void EXPORT FAR PASCAL Am2ShowUnreadMessageCount( void )
{
   Ameol2_ShowUnreadMessageCount();
}

DWORD EXPORT FAR PASCAL Am2ThreadByReference( PTL hTopic, LPSTR lpszReference )
{
   return( Amdb_MessageIdToComment( hTopic, lpszReference ) );
}

DWORD EXPORT FAR PASCAL Am2ThreadBySubject( PTL hTopic, LPSTR lpszSubject, LPSTR lpszIgnoreSubject )
{
   return( Amdb_LookupSubjectReference( hTopic, lpszSubject, lpszIgnoreSubject ) );
}

void EXPORT FAR PASCAL Am2WriteToBlinkLog( LPSTR lpszLogText )
{
   Ameol2_WriteToBlinkLog( lpszLogText );
}
