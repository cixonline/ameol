/* IPNEWS.C - Processes News functions
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
#include "intcomms.h"
#include <memory.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "amlib.h"
#include "shared.h"
#include "cookie.h"
#include "rules.h"
#include "ameol2.h"
#include "nfl.h"
#include "news.h"
#include "admin.h"
#include "log.h"
#include "lbf.h"
#include "cix.h"
#include "news.h"

#define  THIS_FILE   __FILE__
#define  cbLineBuf               1000

#define  HPBUF_OFFSET            32

#define  NWS_GETLIST             0x0002
#define  NWS_DISCONNECTED        0x0003
#define  NWS_QUIT             0x0004
#define  NWS_WAITINGQUIT            0x0005
#define  NWS_GETNEWGROUPS        0x0007
#define  NWS_READNEWSGROUPSIZE      0x0008
#define  NWS_READHEAD            0x0009
#define  NWS_READNEXT            0x000A
#define  NWS_READHEADER          0x000B
#define  NWS_READTAGGED          0x000C
#define  NWS_READBODY            0x000D
#define  NWS_READBODY2           0x000E
#define  NWS_POSTING             0x000F
#define  NWS_POSTFINISH          0x0010
#define  NWS_READARTICLE            0x0011
#define  NWS_READXOVER           0x0012
#define  NWS_READXOVERLINE       0x0013
#define  NWS_READDESCRIPTIONS    0x0014
#define  NWS_AUTHINFOUSER        0x0015
#define  NWS_AUTHINFOPASS        0x0016
#define  NWS_AUTINFOACCEPT       0x0017
#define  NWS_READFULL            0x0018
#define  NWS_ACTIONMASK          0x7FFF

#define  IsBatchServerState(s)   ((s) == NWS_READNEWSGROUPSIZE || (s) == NWS_READTAGGED)

//#define   GRPLIST_VERSION         0x4893//0x4892 !!SM!!
#define  HTYP_ENTRY           0x40

typedef struct tagNEWSHEADER {
   LPSTR lpStrNewsgroup;
   LPSTR lpStrSubject;
   LPSTR lpStrReply;
   LPSTR lpStrBoundary;
   LPSTR lpStrReplyTo;
   LPSTR lpStrMailAddress;
   LPSTR lpStrFullName;
} NEWSHEADER;

typedef struct tagNNTPSRVR {
   struct tagNNTPSRVR * lpcNext;       /* Pointer to next news server */
   char szServerName[ 64 ];            /* Name of news server */
   int nNewsState;                     /* Current state action */
   int nSavedNewsState;                /* Saved news state for authentication */
   BOOL fCanDoXover;                   /* TRUE if news server can use xover */
   LPCOMMDEVICE lpcdevNNTP;            /* Handle of comms device */
   NEWSSERVERINFO nsi;                 /* News server information */
} NNTPSRVR, FAR * LPNNTPSRVR;

LPNNTPSRVR lpcNewsFirst = NULL;        /* First news server in list */

extern BOOL fIsAmeol2Scratchpad;

BOOL fNNTPAuthInfo;                    /* TRUE if we use authentication */
BOOL fAuthenticated;                   /* TRUE if authentication has been done */

char FAR szLastCommand[ 512 ];         /* Last command */
char szTmpName[ 64 ];

static char * szDayOfWeek[ 7 ] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static char * szMonth[ 12 ] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

static HEADER FAR newshdr;             /* News header */
static BOOL fEntry = FALSE;            /* Re-entrancy flag */
static BOOL fImmediateDisconnect;      /* Flag to detect immediate disconnects */
static WORD wOldCount;                 /* Previous percentage */
static NEWSHEADER NewsHdr;             /* Newsgroup header */

static HPSTR hpNewsBuf = NULL;         /* Newsgroups file in memory */
static DWORD cbBufSize;                /* Size of hpNewsBuf */
static DWORD cbAllocSize;              /* Size of hpNewsBuf */
static HPSTR hpDescBuf = NULL;         /* Newsgroups descriptions file in memory */
static DWORD cbDescBufSize;            /* Size of hpDescBuf */
static DWORD cbDescAllocSize;          /* Size of hpDescBuf */
//static UINT cbNewsgroups;               /* Number of newsgroups in buffer */
static LONG cbNewsgroups;              /* Number of newsgroups in buffer */
static UINT cDescriptions;             /* Number of descriptions in buffer */
static char szLastNewsgroup[ 8192 ];      /* Last newsgroup saved */
static HPSTR hpLastNewsPtr;            /* Last newsgroups pointer */
static BOOL fInsertGroup;              /* TRUE if okay to add further newsgroups */
static LPSTR lpFilters;                /* Filters, or NULL if none */
static WORD wFilterType;               /* Filter type */

static UINT cGroupsRead;               /* Number of newsgroups read so far */

static NEWSFOLDERLIST nfl;             /* News folder list */
static int cTopicIdx;                  /* Index of current topic */
static PTH pthTagged;                  /* Handle of current tagged message */

static BOOL fSingleArticle;            /* TRUE if we're reading a single article */
static long iTotalArticles;            /* Total articles in newsgroup */
static long iFirstArticle;             /* First article in newsgroup */
static long iLastArticle;              /* Last article in newsgroup */
static long iNextArticle;              /* Next article in newsgroup */
static DWORD dwTaggedRetrieved;        /* Number of tagged articles retrieved */
static DWORD dwHeadersRetrieved;       /* Number of headers retrieved */
static BOOL fAddedArticle;             /* TRUE if we've prepended the Article header */
static BOOL fReadFull;                 /* TRUE if we're reading full articles */

static HPSTR hpHdrText;                /* Pointer to memory buffer */
static DWORD dwHdrLength;              /* Length of message so far */

static HPSTR hpBuf;                    /* Pointer to memory buffer */
static DWORD dwMsgLength;              /* Length of message so far */
static DWORD dwBufSize;                /* Size of buffer */

static HPSTR hpEncodedParts;           /* Pointer to encoded parts buffer */
static UINT cEncodedParts;             /* Number of encoded parts */
static UINT nThisEncodedPart;          /* Number of this encoded part */
static int iGroup;
static char FAR szDefaultAuthor[] = "[No-author]\0";


void FASTCALL FormatTextIP( HPSTR lprpSrc, HPSTR * lprpDest, DWORD pMax, DWORD iWordWrapColumn ); // FS#76  

BOOL EXPORT CALLBACK NewsClientWndProc( LPCOMMDEVICE, int, DWORD, LPARAM );

BOOL FASTCALL WriteNewsSocketLine( LPNNTPSRVR, char *, int  );
BOOL FASTCALL ParseNews( LPCOMMDEVICE, char * );
LPNNTPSRVR FASTCALL EnsureNewsWindow( char *, NEWSSERVERINFO * );
void FASTCALL AppendToMessage( char *, BOOL );
void FASTCALL AddNewArticle( PTL, PTL, int, BOOL );
BOOL FASTCALL GetNextTagged( void );
void FASTCALL SetNewsIdle( LPNNTPSRVR );
void FASTCALL EndGetHeaders( LPNNTPSRVR );
void FASTCALL CreateNewsHeader( NEWSHEADER FAR * );
LPSTR FASTCALL GetArticleNewsgroup( PTH );
BOOL FASTCALL InsertNewsgroup( char *, LPSTR, WORD, BOOL );
BOOL FASTCALL InsertNewsgroupDescription( char *, char * );
BOOL FASTCALL AuthenticationFailed( LPNNTPSRVR, int, char * );
void FASTCALL ReportNewsOnline( LPNNTPSRVR, char * );
void FASTCALL RemoveNewsServer( LPCOMMDEVICE );
LPNNTPSRVR FASTCALL EnsureNewsgroupServer( PTL );
LPNNTPSRVR FASTCALL GetNewsServer( LPCOMMDEVICE );
void FASTCALL MergeDescriptions( void );
void FASTCALL LoadNewsgroupDataFile( LPNNTPSRVR );
void FASTCALL SaveNewsgroupDataFile( LPNNTPSRVR );

static BOOL FASTCALL AppendToHeader( char * );
static BOOL FASTCALL AppendMem32ToHeader( HPSTR, DWORD );
LPSTR FASTCALL ExtractString( LPSTR, LPSTR, int );

/* This function returns the number of open news
 * servers.
 */
int FASTCALL CountOpenNewsServers( void )
{
   LPNNTPSRVR lpcNews;
   int count;

   count = 0;
   for( lpcNews = lpcNewsFirst; lpcNews; lpcNews = lpcNews->lpcNext )
      if( lpcNews->lpcdevNNTP )
         ++count;
   return( count );
}

/* This function disconnects all open news servers.
 * It can be called at any time.
 */
void FASTCALL DisconnectAllNewsServers( void )
{
   LPNNTPSRVR lpcNewsNext;
   LPNNTPSRVR lpcNews;

   /* Controlled exit.
    */
   for( lpcNews = lpcNewsFirst; lpcNews; lpcNews = lpcNewsNext )
      {
      /* Do this because the news server could be deleted from
       * the list when it is disconnected.
       */
      lpcNewsNext = lpcNews->lpcNext;
      if( lpcNews->lpcdevNNTP )
         SendMessage( lpcNews->lpcdevNNTP->hwnd, WM_DISCONNECT, 0, 0L );
      }

   /* Any that could not be closed, close them now.
    */
   while( lpcNewsFirst )
      {
      lpcNewsNext = lpcNewsFirst->lpcNext;
      FreeMemory( &lpcNewsFirst );
      lpcNewsFirst = lpcNewsNext;
      }

   /* Clean up afterwards.
    */
   if( NULL != nfl.ptlTopicList )
      FreeMemory( (LPVOID)&nfl.ptlTopicList );
   nfl.cTopics = 0;
   dwMsgLength = HPBUF_OFFSET;
   if( hpBuf )
      FreeMemory32( &hpBuf );
}

/* This function deletes the specified news server associated
 * with lpcdev from the list.
 */
void FASTCALL RemoveNewsServer( LPCOMMDEVICE lpcdev )
{
   LPNNTPSRVR lpcNewsPrev;
   LPNNTPSRVR lpcNews;

   lpcNewsPrev = NULL;
   for( lpcNews = lpcNewsFirst; lpcNews; lpcNews = lpcNews->lpcNext )
      {
      if( lpcNews->lpcdevNNTP == lpcdev )
         {
         if( lpcNewsPrev == NULL )
            lpcNewsFirst = lpcNews->lpcNext;
         else
            lpcNewsPrev->lpcNext = lpcNews->lpcNext;
         FreeMemory( &lpcNews );
         return;
         }
      lpcNewsPrev = lpcNews;
      }
}

/* Check whether any of the news servers are busy.
 */
BOOL FASTCALL IsNewsServerBusy( void )
{
   LPNNTPSRVR lpcNews;

   for( lpcNews = lpcNewsFirst; lpcNews; lpcNews = lpcNews->lpcNext )
      if( lpcNews->nNewsState != NWS_IDLE )
         return( TRUE );
   return( FALSE );
}

/* This function returns the handle of the news server associated
 * with the specified connection card.
 */
LPNNTPSRVR FASTCALL GetNewsServer( LPCOMMDEVICE lpcdev )
{
   LPNNTPSRVR lpcNews;

   for( lpcNews = lpcNewsFirst; lpcNews; lpcNews = lpcNews->lpcNext )
      if( lpcNews->lpcdevNNTP == lpcdev )
         return( lpcNews );
   return( NULL );
}

/* This function retrieves the full list of Usenet newsgroups.
 */
int FASTCALL Exec_GetNewsgroups( LPVOID dwData )
{
   NEWSGROUPSOBJECT FAR * lpgrp;
   LPNNTPSRVR lpcNews;
   NEWSSERVERINFO nsi;

   /* Dereference the data structure.
    */
   lpgrp = (NEWSGROUPSOBJECT FAR *)dwData;

   /* Get the news server name.
    */
   if( !GetUsenetServerDetails( DRF(lpgrp->recServerName), &nsi ) )
      return( POF_CANNOTACTION );

   /* Okay to access server now?
    */
   if( !OkayUsenetAccess( nsi.szServerName ) )
      return( POF_CANNOTACTION );

   /* Start up the connection.
    */
   lpcNews = EnsureNewsWindow( nsi.szServerName, &nsi );
   if( NULL == lpcNews )
      return( POF_CANNOTACTION );

   /* If we're busy, this request goes into the
    * out-basket.
    */
   if( lpcNews->nNewsState != NWS_IDLE )
      return( POF_AWAITINGSERVICE );

   /* Send the LIST command to get a full list of newsgroups.
    */
   if( hwndGrpList )
      {
      SendMessage( hwndMDIClient, WM_MDIDESTROY, (WPARAM)hwndGrpList, 0L );
      OnlineStatusText( GS(IDS_STR738) );
      }
   cbDescBufSize = 0;
   cbBufSize = 0;
   if( WriteNewsSocketLine( lpcNews, "list\r\n", NWS_GETLIST ) )
      {
      cGroupsRead = 0;
      return( POF_PROCESSSTARTED );
      }
   return( POF_CANNOTACTION );
}

/* This function gets the newsgroups posted since the
 * specified date and time.
 */
int FASTCALL Exec_GetNewNewsgroups( LPVOID dwData )
{
   NEWNEWSGROUPSOBJECT FAR * lpgrp;
   LPNNTPSRVR lpcNews;
   NEWSSERVERINFO nsi;
   char sz[ 60 ];
   int iYear;

   /* Dereference the data structure.
    */
   lpgrp = (NEWNEWSGROUPSOBJECT FAR *)dwData;

   /* Get the news server name.
    */
   if( !GetUsenetServerDetails( DRF(lpgrp->recServerName), &nsi ) )
      return( POF_CANNOTACTION );

   /* Okay to access server now?
    */
   if( !OkayUsenetAccess( nsi.szServerName ) )
      return( POF_CANNOTACTION );

   /* Start up the connection.
    */
   lpcNews = EnsureNewsWindow( nsi.szServerName, &nsi );
   if( NULL == lpcNews )
      return( POF_CANNOTACTION );

   /* If we're busy, this request goes into the
    * out-basket.
    */
   if( lpcNews->nNewsState != NWS_IDLE )
      return( POF_AWAITINGSERVICE );

   /* Send the NEWSGROUPS command to get the list of new newsgroups.
    */
   iYear = lpgrp->date.iYear;
   if( iYear >= 100 )
      iYear -= 1900;
   wsprintf( sz, "newgroups %2.2u%2.2u%2.2u %2.2u%2.2u%2.2u\r\n",
             iYear, lpgrp->date.iMonth, lpgrp->date.iDay,
             lpgrp->time.iHour, lpgrp->time.iMinute, lpgrp->time.iSecond );
   cbDescBufSize = 0;
   cbBufSize = 0;
   if( WriteNewsSocketLine( lpcNews, sz, NWS_GETNEWGROUPS ) )
      {
      cGroupsRead = 0;
      return( POF_PROCESSSTARTED );
      }
   return( POF_CANNOTACTION );
}

/* This function reads new articles from all subscribed newsgroups.
 */
int FASTCALL Exec_GetNewArticles( LPVOID dwData )
{
   NEWARTICLESOBJECT FAR * lprh;

   /* Derefrence the data structure.
    */
   lprh = (NEWARTICLESOBJECT FAR *)dwData;
   Amob_DerefObject( &lprh->recFolder );

   /* Find the first newsgroup from which we start reading
    * headers.
    */
   if( BuildFolderList( lprh->recFolder.hpStr, &nfl ) )
      {
      LPNNTPSRVR lpcNews;
      char sz[ 100 ];

      /* Get the first newsgroup.
       */
      cTopicIdx = 0;
      newshdr.ptl = nfl.ptlTopicList[ cTopicIdx++ ];

      /* Open the news server for that newsgroup.
       */
      lpcNews = EnsureNewsgroupServer( newshdr.ptl );
      while( NULL == lpcNews && cTopicIdx < nfl.cTopics )
         {
         newshdr.ptl = nfl.ptlTopicList[ cTopicIdx++ ];
         lpcNews = EnsureNewsgroupServer( newshdr.ptl );
         if( NULL != lpcNews )
            break;
         }

      /* No server available? Fail now.
       */
      if( NULL == lpcNews )
         {
         FreeMemory( (LPVOID)&nfl.ptlTopicList );
         return( POF_CANNOTACTION );
         }

      /* Lock the topic.
       */
      Amdb_LockTopic( newshdr.ptl );
      Amdb_MarkTopicInUse( newshdr.ptl, TRUE );

      /* If we're busy, this request goes into the
       * out-basket.
       */
      if( lpcNews->nNewsState != NWS_IDLE )
         return( POF_AWAITINGSERVICE );

      /* Send first command.
       */
      OnlineStatusText( GS(IDS_STR692) );
      wsprintf( sz, "group %s\r\n", Amdb_GetTopicName( newshdr.ptl ) );
      if( WriteNewsSocketLine( lpcNews, sz, NWS_READNEWSGROUPSIZE ) )
         {
         dwHeadersRetrieved = 0L;
         return( POF_PROCESSSTARTED );
         }
      }
   return( POF_PROCESSCOMPLETED );
}

/* This function reads the articles for all tagged articles in
 * the specified folder.
 */
int FASTCALL Exec_GetTagged( LPVOID dwData )
{
   GETTAGGEDOBJECT FAR * lpgt;

   /* Derefrence the data structure.
    */
   lpgt = (GETTAGGEDOBJECT FAR *)dwData;
   Amob_DerefObject( &lpgt->recFolder );

   /* Find the first newsgroup from which we start looking for
    * marked articles.
    */
   if( BuildFolderList( lpgt->recFolder.hpStr, &nfl ) )
      {
      /* Find the first topic with a tagged
       * message.
       */
      cTopicIdx = 0;
      newshdr.ptl = NULL;
      do {
         if( NULL != newshdr.ptl )
            {
            Amdb_MarkTopicInUse( newshdr.ptl, FALSE );
            Amdb_UnlockTopic( newshdr.ptl );
            }
         newshdr.ptl = nfl.ptlTopicList[ cTopicIdx++ ];
         Amdb_LockTopic( newshdr.ptl );
         pthTagged = Amdb_GetTagged( newshdr.ptl, NULL );
         }
      while( cTopicIdx < nfl.cTopics && !pthTagged );

      /* Got one. The topic will be locked now, so mark
       * the message as being retrieved.
       */
      if( pthTagged )
         {
         char * pszGroupName;
         LPNNTPSRVR lpcNews;
         HWND hwndTopic2;
         char sz[ 100 ];

         /* Open the news server for that newsgroup. If it can't
          * be opened, try another newsgroup.
          */
         lpcNews = EnsureNewsgroupServer( newshdr.ptl );
         while( NULL == lpcNews && cTopicIdx < nfl.cTopics )
            {
            do {
               PTH pth;
         
               pth = NULL;
               Amdb_MarkTopicInUse( newshdr.ptl, FALSE );
               Amdb_UnlockTopic( newshdr.ptl );
               newshdr.ptl = nfl.ptlTopicList[ cTopicIdx++ ];
               Amdb_LockTopic( newshdr.ptl );
               pthTagged = Amdb_GetTagged( newshdr.ptl, pth );
               }
            while( cTopicIdx < nfl.cTopics && !pthTagged );
            lpcNews = EnsureNewsgroupServer( newshdr.ptl );
            }
         Amdb_MarkTopicInUse( newshdr.ptl, TRUE );

         /* No server available? Fail now.
          */
         if( NULL == lpcNews )
            {
            FreeMemory( (LPVOID)&nfl.ptlTopicList );
            return( POF_CANNOTACTION );
            }

         /* No new messages? Finished.
          */
         if( NULL == pthTagged )
            return( POF_PROCESSCOMPLETED );

         /* Mark the message as being retrieved.
          */
         Amdb_MarkMsgBeingRetrieved( pthTagged, TRUE );
         hwndTopic2 = Amdb_GetTopicSel( newshdr.ptl );
         if( hwndTopic2 )
            SendMessage( hwndTopic2, WM_UPDMSGFLAGS, 0, (LPARAM)pthTagged );
         OnlineStatusText( GS(IDS_STR705) );

         /* If we're busy, this request goes into the
          * out-basket.
          */
         if( lpcNews->nNewsState != NWS_IDLE )
            return( POF_AWAITINGSERVICE );

         /* Select that group.
          */
         pszGroupName = GetArticleNewsgroup( pthTagged );
         wsprintf( sz, "group %s\r\n", pszGroupName );
         if( WriteNewsSocketLine( lpcNews, sz, NWS_READTAGGED ) )
            {
            fSingleArticle = FALSE;
            dwTaggedRetrieved = 0L;
            return( POF_PROCESSSTARTED );
            }
         return( POF_CANNOTACTION );
         }
      if( NULL != newshdr.ptl )
         Amdb_UnlockTopic( newshdr.ptl );
      FreeMemory( (LPVOID)&nfl.ptlTopicList );
      nfl.cTopics = 0;
      }
   return( POF_PROCESSCOMPLETED );
}

/* This function returns the name of the newsgroup from which
 * the specified article was originally retrieved. To do this, it
 * parses the Newsgroup: field in the header.
 */
LPSTR FASTCALL GetArticleNewsgroup( PTH pth )
{
   char szArticle[ 128 ];
   static char szNewsgroup[ 80 ];
   static char scp_str7[] = " of ";
   char * psz;
   DWORD dwMsg;
   register int c;

   Amdb_GetMailHeaderField( pth, "Article", szArticle, sizeof(szArticle), FALSE );
   psz = szArticle;
   while( *psz == ' ' )
      ++psz;
   dwMsg = 0;
   while( isdigit( *psz ) )
      dwMsg = dwMsg * 10 + ( *psz++ - '0' );
   if( strncmp( psz, scp_str7, strlen( scp_str7 ) ) == 0 )
      {
      psz += strlen( scp_str7 );
      for( c = 0; *psz && *psz != ' ' && c < LEN_TOPICNAME; ++c )
         szNewsgroup[ c ] = *psz++;
      szNewsgroup[ c ] = '\0';
      }
   return( szNewsgroup );
}

/* This function retrieves ONE article from the news server.
 */
int FASTCALL Exec_GetArticle( LPVOID dwData )
{
   GETARTICLEOBJECT FAR * lpga;

   /* Dereference the object data structure
    */
   lpga = (GETARTICLEOBJECT FAR *)dwData;
   Amob_DerefObject( &lpga->recNewsgroup );

   /* Find the first newsgroup from which we start looking for
    * marked articles.
    */
   newshdr.ptl = PtlFromNewsgroup( lpga->recNewsgroup.hpStr );
   if( NULL != newshdr.ptl && Amdb_IsTopicPtr( newshdr.ptl ) )
      {
      char sz[ 100 ];

      Amdb_LockTopic( newshdr.ptl );
      if( NULL != ( pthTagged = Amdb_OpenMsg( newshdr.ptl, lpga->dwMsg ) ) )
         {
         LPNNTPSRVR lpcNews;
         HWND hwndTopic2;
   
         /* Open the news server for that newsgroup.
          */
         Amdb_MarkTopicInUse( newshdr.ptl, TRUE );
         lpcNews = EnsureNewsgroupServer( newshdr.ptl );
         if( NULL == lpcNews )
            return( POF_CANNOTACTION );

         /* If we're busy, this request goes into the
          * out-basket.
          */
         if( lpcNews->nNewsState != NWS_IDLE )
            return( POF_AWAITINGSERVICE );

         /* This article already exists, so retrieve it over the top of
          * the existing article.
          */
         wsprintf( sz, "group %s\r\n", GetArticleNewsgroup( pthTagged ) );
         if( WriteNewsSocketLine( lpcNews, sz, NWS_READTAGGED ) )
            {
            fSingleArticle = TRUE;
            Amdb_MarkMsgBeingRetrieved( pthTagged, TRUE );
            hwndTopic2 = Amdb_GetTopicSel( newshdr.ptl );
            if( hwndTopic2 )
               SendMessage( hwndTopic2, WM_UPDMSGFLAGS, 0, (LPARAM)pthTagged );
            OnlineStatusText( GS(IDS_STR707), lpga->dwMsg, (LPSTR)lpga->recNewsgroup.hpStr );
            return( POF_PROCESSSTARTED );
            }
         }
      else
         {
         LPNNTPSRVR lpcNews;

         /* Open the news server for that newsgroup.
          */
         lpcNews = EnsureNewsgroupServer( newshdr.ptl );
         if( NULL == lpcNews )
            return( POF_CANNOTACTION );

         /* If we're busy, this request goes into the
          * out-basket.
          */
         if( lpcNews->nNewsState != NWS_IDLE )
            return( POF_AWAITINGSERVICE );

         /* Article does not exist, to retrieve it anew.
          */
         wsprintf( sz, "group %s\r\n", Amdb_GetTopicName( newshdr.ptl ) );
         if( WriteNewsSocketLine( lpcNews, sz, NWS_READTAGGED ) )
            {
            fSingleArticle = TRUE;
            iNextArticle = lpga->dwMsg;
            OnlineStatusText( GS(IDS_STR707), lpga->dwMsg, (LPSTR)lpga->recNewsgroup.hpStr );
            return( POF_PROCESSSTARTED );
            }
         }
      Amdb_UnlockTopic( newshdr.ptl );
      }
   return( POF_CANNOTACTION );
}

/* This function posts an article to the news server
 */
int FASTCALL Exec_ArticleMessage( LPVOID dwData )
{
   ARTICLEOBJECT FAR * lpao;
   char szNewsgroup[ 64 ];
   LPNNTPSRVR lpcNews;
   PTL ptl;
   LPSTR lpNewsBuf;

   /* Dereference the object data structure
    */
   lpao = (ARTICLEOBJECT FAR *)dwData;

   /* Get the topic handle.
    */
   ptl = NULL;
   lpNewsBuf = DRF(lpao->recNewsgroup);
   while( *lpNewsBuf && ptl == NULL )
   {
      ExtractString( lpNewsBuf, szNewsgroup, sizeof(szNewsgroup) - 1 );
      ptl = PtlFromNewsgroup( szNewsgroup );
      if( ptl == NULL )
      {
         lpNewsBuf+= strlen( szNewsgroup );
         if( *lpNewsBuf == ',' || *lpNewsBuf == ' ' )
            lpNewsBuf++;
      }
   }

   if( NULL == ptl )
      return( POF_CANNOTACTION );

   /* Start up the connection.
    */
   lpcNews = EnsureNewsgroupServer( ptl );
   if( NULL == lpcNews )
      return( POF_CANNOTACTION );

   /* If we're busy, this request goes into the
    * out-basket.
    */
   if( lpcNews->nNewsState != NWS_IDLE )
      return( POF_AWAITINGSERVICE );

   /* Create the header text manually.
    */
   hpHdrText = NULL;
   dwHdrLength = 0;
   dwBufSize = 0;

   /* Initialise encoding variables.
    */
   cEncodedParts = 0;
   nThisEncodedPart = 1;
   hpEncodedParts = NULL;
   NewsHdr.lpStrBoundary = NULL;

   /* Do we have an attachment? If so, encode the file into one big memory
    * block in hpAttachment and set the cEncodedParts and hpEncodedParts
    * variables.
    */
   Amob_DerefObject( &lpao->recAttachments );
   if( lpao->recAttachments.dwSize > 1 )
      {
      /* Create the MIME boundary.
       */
      if( lpao->wEncodingScheme == ENCSCH_MIME )
         {
         NewsHdr.lpStrBoundary = CreateMimeBoundary();
         if( NULL == NewsHdr.lpStrBoundary )
            return( POF_CANNOTACTION );
         }
      hpEncodedParts = EncodeFiles( hwndFrame, lpao->recAttachments.hpStr, NewsHdr.lpStrBoundary, lpao->wEncodingScheme, &cEncodedParts );
      if( NULL == hpEncodedParts )
         return( POF_CANNOTACTION );
      if( fSeparateEncodedCover )
         nThisEncodedPart = 0;
      }

   /* Create local copies of the subject, to, cc and reply strings
    * for multi-part mail messages.
    */
   NewsHdr.lpStrNewsgroup = HpLpStrCopy( Amob_DerefObject( &lpao->recNewsgroup ) );
   NewsHdr.lpStrSubject = HpLpStrCopy( Amob_DerefObject( &lpao->recSubject ) );
   NewsHdr.lpStrReply = HpLpStrCopy( Amob_DerefObject( &lpao->recReply ) );
   NewsHdr.lpStrReplyTo = HpLpStrCopy( Amob_DerefObject( &lpao->recReplyTo ) );

   /* Get the other stuff from the cookie.
    */
   Amdb_GetCookie( ptl, NEWS_FULLNAME_COOKIE, lpTmpBuf, szFullName, 64 );
   NewsHdr.lpStrFullName = HpLpStrCopy( lpTmpBuf );
   Amdb_GetCookie( ptl, NEWS_EMAIL_COOKIE, lpTmpBuf, szMailAddress, 64 );
   NewsHdr.lpStrMailAddress = HpLpStrCopy( lpTmpBuf );

   /* Create news article header.
    */
   CreateNewsHeader( &NewsHdr );

   /* Now append the message text body.
    */
   if( NULL != NewsHdr.lpStrBoundary )
      {
      char sz[ 80 ];

      wsprintf( sz, "--%s", NewsHdr.lpStrBoundary );
      AppendToHeader( sz );                                    
      /*!!SM!!*/
//    AppendToHeader( "Content-Type: text/plain; charset=\"us-ascii\"" );
      AppendToHeader( "Content-Type: text/plain; charset=\"iso-8859-1\"" );
      AppendToHeader( "" );
      }
   Amob_DerefObject( &lpao->recText );
   
   // 20060228 - 2.56.2049.07
   // Need to wrap message body text here

   if( *lpao->recText.hpStr )
   {
      HPSTR lpStr ;
      HPSTR lpStrOut = NULL;
      DWORD lLen;

      lLen = (DWORD)(lstrlen(lpao->recText.hpStr) * 1.5);

      if (lLen <= 10)
         lLen = 10;


      INITIALISE_PTR(lpStrOut);
      
      fNewMemory32( &lpStrOut, lLen );
   
      //lpStr = lpm->recText.hpStr;

      // 20060228 - 2.56.2049.07
      // Need to wrap message body text here
      FormatTextIP( lpao->recText.hpStr, &lpStrOut, lLen - 1, inWrapCol ); // FS#76  // 2.56.2055

      lpStr = lpStrOut;

      if( lstrlen(lpStr) > 0 && !AppendMem32ToHeader( lpStr, lstrlen(lpStr) /*- 1*/ ) )
      {
         FreeMemory32( &hpHdrText );
         return( POF_CANNOTACTION );
      }
      FreeMemory32( &lpStrOut );
   }
   AppendToHeader( "" );

   /* If we have an attachment and we attach to the first page, then
    * attach the first part to this page.
    */
   if( cEncodedParts )
      {
      if( !fSeparateEncodedCover )
         {
         AppendMem32ToHeader( hpEncodedParts, hstrlen( hpEncodedParts ) );
         hpEncodedParts += hstrlen( hpEncodedParts ) + 1;
         }
      ++nThisEncodedPart;
      }

   /* Status bar message.
    */
   OnlineStatusText( GS(IDS_STR1218), lpao->recSubject.hpStr );

   /* Dereference the list of newsgroups. There can
    * be more than one.
    */
   if( WriteNewsSocketLine( lpcNews, "post\r\n", NWS_POSTING ) )
      return( POF_PROCESSSTARTED );
   return( POF_CANNOTACTION );
}

/* This function posts an article to the news server
 */
int FASTCALL Exec_CancelArticle( LPVOID dwData )
{
   LPNNTPSRVR lpcNews;
   CANCELOBJECT FAR * lpco;
   char szMsgId[ 256 ];
   char sz[ 256 ];
// LPSTR lpMyBuf1;
// LPSTR lpMyBuf2;
   PTL ptl;

   /* Dereference the object data structure
    */
   lpco = (CANCELOBJECT FAR *)dwData;

   /* Get the topic handle.
    */
   ptl = PtlFromNewsgroup( DRF(lpco->recNewsgroup) );
   if( NULL == ptl )
      return( POF_CANNOTACTION );

   /* Start up the connection.
    */
   lpcNews = EnsureNewsgroupServer( ptl );
   if( NULL == lpcNews )
      return( POF_CANNOTACTION );

   /* If we're busy, this request goes into the
    * out-basket.
    */
   if( lpcNews->nNewsState != NWS_IDLE )
      return( POF_AWAITINGSERVICE );

   /* Create the header text manually.
    */
   cEncodedParts = 0;
   nThisEncodedPart = 1;
   hpHdrText = NULL;
   dwHdrLength = 0;
   dwBufSize = 0;

   /* Start with the cancel control.
    */
   Amob_DerefObject( &lpco->recReply );
   ASSERT( lpco->recReply.hpStr );
   wsprintf( sz, "Control: cancel <%s>", lpco->recReply.hpStr );
   AppendToHeader( sz );

   /* Show the list of newsgroups.
    */
   Amob_DerefObject( &lpco->recNewsgroup );
   wsprintf( sz, "Newsgroups: %s", lpco->recNewsgroup.hpStr );
   AppendToHeader( sz );

   /* Say who this is from.
    */
   Amob_DerefObject( &lpco->recFrom );
   ASSERT( lpco->recFrom.hpStr );
   Amob_DerefObject( &lpco->recRealName );
   ASSERT(lpco->recRealName.hpStr );
   wsprintf( sz, "From: %s (%s)", lpco->recFrom.hpStr, lpco->recRealName.hpStr );
   AppendToHeader( sz );

   /* And the subject.
    */
   wsprintf( sz, "Subject: cmsg cancel <%s>", lpco->recReply.hpStr );
   AppendToHeader( sz );

   /* Create and store our own message ID.
    */
   CreateMessageId( szMsgId );
   wsprintf( sz, "Message-Id: %s", (LPSTR)szMsgId );
   AppendToHeader( sz );

   /* Include organization field, if it exists.
    */
   CreateMessageId( szMsgId );
   wsprintf( sz, "Organization: %s", (LPSTR)"CIX" );
   AppendToHeader( sz );

   /* Now append the message text body.
    */
   AppendToHeader( "" );
   AppendToHeader( "Article cancelled" );

   /* Status bar message.
    */
   OnlineStatusText( GS(IDS_STR741), lpco->recReply.hpStr, lpco->recNewsgroup.hpStr );

   /* Dereference the list of newsgroups. There can
    * be more than one.
    */
   if( WriteNewsSocketLine( lpcNews, "post\r\n", NWS_POSTING ) )
      return( POF_PROCESSSTARTED );
   return( POF_CANNOTACTION );
}

/* This function returns the handle of the next tagged article.
 */
BOOL FASTCALL GetNextTagged( void )
{
   PTL ptl;

   /* One or no further tagged articles.
    */
   if( NULL == pthTagged || fSingleArticle )
      return( FALSE );
   newshdr.ptl = NULL;

   /* Now look for a topic with tagged articles.
    */
   ptl = Amdb_TopicFromMsg( pthTagged );
   pthTagged = Amdb_GetTagged( ptl, pthTagged );
   if( NULL == pthTagged )
      {
      /* Finished a topic, so report how many
       * tagged articles we got.
       */
      if( dwTaggedRetrieved > 0 )
         {
         wsprintf( lpTmpBuf, GS(IDS_STR1083), dwTaggedRetrieved, Amdb_GetTopicName( ptl ) );
         WriteToBlinkLog( lpTmpBuf );
         dwTaggedRetrieved = 0;
         }

      /* Look for more topics with tagged
       * articles.
       */
      Amdb_MarkTopicInUse( ptl, FALSE );
      if( cTopicIdx < nfl.cTopics )
         do {
            PTH pth;
      
            pth = NULL;
            Amdb_UnlockTopic( ptl );
            ptl = nfl.ptlTopicList[ cTopicIdx++ ];
            Amdb_LockTopic( ptl );
            pthTagged = Amdb_GetTagged( ptl, pth );
            }
         while( cTopicIdx < nfl.cTopics && !pthTagged );
      }
   return( pthTagged != NULL );
}

/* This function creates the news header using the ARTICLEOBJECT data and
 * stores it in hpHdrText.
 */
void FASTCALL CreateNewsHeader( NEWSHEADER FAR * lpNewsHdr )
{
   char szMsgId[ 256 ];
   LPSTR lpszGrp;
   LPSTR lpszGrpPtr;
   char sz[ 256 ];
   register int c;
   AM_TIMEZONEINFO tzi;
   AM_DATE date;
   AM_TIME time;

   /* Create the header text manually.
    */
   dwHdrLength = 0;
   dwBufSize = 0;
   INITIALISE_PTR( hpHdrText );

   /* Show the list of newsgroups.
    */
   lpszGrpPtr = lpszGrp = MakeCompactString( lpNewsHdr->lpStrNewsgroup );
   wsprintf( sz, "Newsgroups: %s", lpszGrpPtr );
   while( lpszGrpPtr += lstrlen( lpszGrpPtr ) + 1, *lpszGrpPtr )
      {
      strcat( sz, "," );
      strcat( sz, lpszGrpPtr );
      }
   AppendToHeader( sz );
   FreeMemory( &lpszGrp );

   /* Say who this is from.
    */
   wsprintf( sz, "From: %s (%s)", lpNewsHdr->lpStrMailAddress, lpNewsHdr->lpStrFullName );
   AppendToHeader( sz );

   /* Include a reply-address if desired.
    */
   if( *lpNewsHdr->lpStrReplyTo )
      {
      wsprintf( sz, "Reply-To: %s", lpNewsHdr->lpStrReplyTo );
      AppendToHeader( sz );
      }

   /* And the subject.
    */
   wsprintf( sz, "Subject: %s", lpNewsHdr->lpStrSubject );
   if( cEncodedParts )
      wsprintf( sz + strlen( sz ), " (%u/%u)", nThisEncodedPart, cEncodedParts );
   AppendToHeader( sz );

   /* Add a Date line
    */
   Amdate_GetCurrentDate( &date );
   Amdate_GetCurrentTime( &time );
   wsprintf( sz, "Date: %s, %u %s %u %02u:%2.2u ",
               szDayOfWeek[ date.iDayOfWeek ],
               date.iDay,
               szMonth[ date.iMonth - 1 ],
               date.iYear,
               time.iHour,
               time.iMinute );
   Amdate_GetTimeZoneInformation( &tzi );
   wsprintf( sz + strlen( sz ), "+%02u00 (%s)", tzi.diff, tzi.szZoneName );
   AppendToHeader( sz );

   /* Insert the custom headers.
    */
   LoadUsenetHeaders();
   for( c = 0; c < cNewsHeaders; ++c )
      {
      wsprintf( sz, "%s: %s", uNewsHeaders[ c ].szType, uNewsHeaders[ c ].szValue );
      AppendToHeader( sz );
      }

   /* MIME Boundary?
    */
   if( lpNewsHdr->lpStrBoundary != NULL )
      {
      AppendToHeader( "Mime-Version: 1.0" );
      wsprintf( sz, "Content-Type: multipart/mixed; boundary=\"%s\"", lpNewsHdr->lpStrBoundary );
      AppendToHeader( sz );
      }

   /* Create and store our own message ID.
    */
   CreateMessageId( szMsgId );
   wsprintf( sz, "Message-Id: %s", (LPSTR)szMsgId );
   AppendToHeader( sz );

   /* Is this a reply?
    */
   if( *lpNewsHdr->lpStrReply )
      {
      wsprintf( sz, "References: <%s>", lpNewsHdr->lpStrReply );
      AppendToHeader( sz );
      }

   /* Now append a blank line.
    */
   AppendToHeader( "" );
}

/* This function ensures that the server for the specified
 * newsgroup is online.
 */
LPNNTPSRVR FASTCALL EnsureNewsgroupServer( PTL ptl )
{
   char szSubNewsServer[ 64 ];
   NEWSSERVERINFO nsi;

   /* Get the group server and authentication
    * username and password.
    */
   Amdb_GetCookie( ptl, NEWS_SERVER_COOKIE, szSubNewsServer, szNewsServer, 64 );
   if( !GetUsenetServerDetails( szSubNewsServer, &nsi ) )
      return( NULL );

   /* Is this cixnews? Can't access it via
    * NNTP.
    */
   if( strcmp( szSubNewsServer, szCixnews ) == 0 )
      return( NULL );

   /* Okay to access server now?
    */
   if( !OkayUsenetAccess( szSubNewsServer ) )
      return( NULL );

   /* Go ahead and access server.
    */
   return( EnsureNewsWindow( szSubNewsServer, &nsi ) );
}

/* This function ensures that a news window is open for processing.
 */
LPNNTPSRVR FASTCALL EnsureNewsWindow( char * pszNewsServer, NEWSSERVERINFO * pnsi )
{
   LPCOMMDESCRIPTOR lpcdNNTP;
   LPCOMMDEVICE lpcdevNNTP;
   LPNNTPSRVR lpcNews;
   char * pszLogFile;
   //char szNewsLogFile[ _MAX_FNAME ];

   INITIALISE_PTR(lpcdevNNTP);
   INITIALISE_PTR(lpcdNNTP);

   /* Go no further if not allowed to use CIX internet.
    */
   if( !TestPerm(PF_CANUSECIXIP) )
      return( NULL );

   /* Locate this news server connection card.
    */
   for( lpcNews = lpcNewsFirst; lpcNews; lpcNews = lpcNews->lpcNext )
      if( strcmp( pszNewsServer, lpcNews->szServerName ) == 0 )
         break;

   /* Return now if lpcdevNNTP is not NULL, indicating that
    * a news server device is already open.
    */
   if( NULL != lpcNews )
      if( NULL == lpcNews->lpcdevNNTP )
         {
         if( fDebugMode )
            {
            wsprintf( lpTmpBuf, "Trying news server %s: Rejected (last attempt failed)", pszNewsServer );
            ReportOnline( lpTmpBuf );
            }
         return( NULL );
         }
      else
         {
         if( fDebugMode )
            {
            wsprintf( lpTmpBuf, "Trying news server %s: Using existing connection", pszNewsServer );
            ReportOnline( lpTmpBuf );
            }

         /* Context switch into appropriate service.
          */
         SwitchService( SERVICE_IP );
         SuspendDisconnectTimeout();
         return( lpcNews );
         }

   /* Create a temporary connection card.
    */
   if( !fNewMemory( &lpcdNNTP, sizeof(COMMDESCRIPTOR) ) )
      {
      ReportOnline( GS(IDS_STR1081) );
      return( NULL );
      }
   memset( lpcdNNTP, 0, sizeof(COMMDESCRIPTOR) );
   strcpy( lpcdNNTP->szTitle, "$NNTP" );
   INITIALISE_PTR(lpcdNNTP->lpic );

   /* Create an internet section in the CC.
    */
   if( !fNewMemory( &lpcdNNTP->lpic, sizeof(IPCOMM) ) )
      {
      ReportOnline( GS(IDS_STR1081) );
      FreeMemory( &lpcdNNTP );
      return( NULL );
      }
   lpcdNNTP->wProtocol = PROTOCOL_NETWORK;
   lpcdNNTP->lpic->wPort = pnsi->wNewsPort;
   lpcdNNTP->nTimeout = 60;
   strcpy( lpcdNNTP->lpic->szAddress, pszNewsServer );
#ifdef WIN32
   lpcdNNTP->lpic->rd = rdDef;
#endif
   
   /* Set a flag to trap immediate disconnects.
    */
   fImmediateDisconnect = TRUE;

   /* Add this server to the list.
    */
   if( !fNewMemory( &lpcNews, sizeof(NNTPSRVR) ) )
      ReportOnline( GS(IDS_STR1081) );
   else
      {
      if( fDebugMode )
         {
         wsprintf( lpTmpBuf, "Trying news server %s:%lu : Created new connection", pszNewsServer, lpcdNNTP->lpic->wPort );
         ReportOnline( lpTmpBuf );
         }
      strcpy( lpcNews->szServerName, pszNewsServer );
      lpcNews->lpcdevNNTP = NULL;
      lpcNews->lpcNext = lpcNewsFirst;
      lpcNews->nNewsState = NWS_IDLE;
      lpcNews->fCanDoXover = TRUE;
      memcpy( &lpcNews->nsi, pnsi, sizeof(NEWSSERVERINFO) );
      lpcNewsFirst = lpcNews;
      }

   /* Create a logging file.
    */
   pszLogFile = NULL;
   if( fLogNNTP )
   {
      char szTemp[_MAX_PATH];
      BEGIN_PATH_BUF;
      Amuser_GetActiveUserDirectory( lpPathBuf, _MAX_PATH );
      strcat( strcat( strcat( lpPathBuf, "\\" ), lpcNews->nsi.szShortName), ".log" );
      pszLogFile = (char *)lpPathBuf;
      if(GetShortPathName( pszLogFile, (char *)&szTemp, _MAX_PATH ) > 0)
         wsprintf( lpTmpBuf, GS(IDS_STR1117), "NNTP", (char *)szTemp );
      else
         wsprintf( lpTmpBuf, GS(IDS_STR1117), "NNTP", (char *)pszLogFile );
      WriteToBlinkLog( lpTmpBuf );
      END_PATH_BUF;
   }

   /* Open the news device.
    */
   fAuthenticated = FALSE;
   OnlineStatusText( GS(IDS_STR693), pszNewsServer );
   if( Amcomm_OpenCard( hwndFrame, &lpcdevNNTP, lpcdNNTP, NewsClientWndProc, 0L, pszLogFile, NULL, FALSE, FALSE ) )
      {
      /* All okay.
       */
      lpcNews->lpcdevNNTP = lpcdevNNTP;
      SuspendDisconnectTimeout();
      OnlineStatusText( GS(IDS_STR694), pszNewsServer );

      /* Context switch into appropriate service.
       */
      SwitchService( SERVICE_IP );
      }
   else
      {
      /* Failed.
       */
      lpcNews->lpcdevNNTP = NULL;
      wsprintf( lpTmpBuf, GS(IDS_STR1082), pszNewsServer, Amcomm_GetLastError() );
      ReportOnline( lpTmpBuf );
      WriteToBlinkLog( lpTmpBuf );
      FreeMemory( &lpcdNNTP->lpic );
      FreeMemory( &lpcdNNTP );
      return( NULL );
      }
   return( lpcNews );
}

/* This function handles all messages from the news server.
 */
BOOL EXPORT CALLBACK NewsClientWndProc( LPCOMMDEVICE lpcdev, int nCode, DWORD dwAppData, LPARAM lParam )
{
   switch( nCode )
      {
      case CCB_DISCONNECTED: {
         LPNNTPSRVR lpcNews;

         lpcNews = GetNewsServer( lpcdev );
         
         WriteNewsSocketLine( lpcNews, "quit\r\n", NWS_WAITINGQUIT ); // !!SM!! 2.56.2059

         OnlineStatusText( GS(IDS_STR695), lpcNews->nsi.szServerName );
         if( pthTagged && Amdb_IsBeingRetrieved( pthTagged ) )
            Amdb_MarkMsgBeingRetrieved( pthTagged, FALSE );
         Amcomm_Close( lpcdev );
         Amcomm_Destroy( lpcdev );
         if( fLogNNTP )
            {
            char szNewsLogFile[ _MAX_FNAME ];

            wsprintf( szNewsLogFile, "%s.log", lpcNews->nsi.szShortName );
            Amuser_GetActiveUserDirectory( lpTmpBuf, _MAX_PATH );
            strcat( strcat( lpTmpBuf, "\\" ), szNewsLogFile );
            Ameol2_Archive( lpTmpBuf, FALSE );
            }
         lpcNews->lpcdevNNTP = NULL;
         if( fImmediateDisconnect )
            EndProcessing( 0xFFFF );
         return( TRUE );
         }

      case CCB_DATAREADY:
         ReadAndProcessSocket( lpcdev, (LPBUFFER)lParam, ParseNews );
         break;
      }
   return( FALSE );
}

/* This function writes the specified command to the news server.
 * It also saves a copy of the command in case of authentication.
 */
BOOL FASTCALL WriteNewsSocketLine( LPNNTPSRVR lpcNews, char * pszText, int nNextState )
{
   if( *lpcNews->nsi.szAuthInfoUser && !fAuthenticated )
      {
      /* If we do voluntary NNTP authentication and we haven't
       * done this yet, then save the command and do authentication
       * first.
       */
      lpcNews->nSavedNewsState = nNextState;
      strcpy( szLastCommand, pszText );
      wsprintf( lpTmpBuf, "authinfo user %s\r\n", lpcNews->nsi.szAuthInfoUser );
      pszText = lpTmpBuf;
      lpcNews->nNewsState = NWS_AUTHINFOPASS;
      fAuthenticated = TRUE;
      }
   else
      {
      ASSERT( strlen(pszText)+1 < sizeof(szLastCommand) );
      strcpy( szLastCommand, pszText );
      lpcNews->nNewsState = nNextState;
      }
   return( WriteSocketLine( lpcNews->lpcdevNNTP, pszText ) );
}

/* Parse a complete line read from the news server.
 */
BOOL FASTCALL ParseNews( LPCOMMDEVICE lpcdev, char * pLine )
{
   LPNNTPSRVR lpcNews;
   BOOL fServiceActive;
   int iCode;

   /* This code must not be re-entrant!
    */
   ASSERT( FALSE == fEntry );
   fEntry = TRUE;
   fServiceActive = TRUE;
   fImmediateDisconnect = FALSE;

   /* Get the associated news server handle.
    */
   lpcNews = GetNewsServer( lpcdev );

   /* Read the code off the line.
    */
   iCode = 0;
   if( !( lpcNews->nNewsState & NWS_ISBUSY ) && isdigit( *pLine ) )
      {
      iCode = atoi( pLine );

      /* Server busy? Fail the command if we're not in an action which
       * involves scanning multiple servers.
       */
      if( iCode == 400 )
         {
         fEntry = FALSE;
         ReportNewsOnline( lpcNews, GS(IDS_STR969) );
         if( !IsBatchServerState(lpcNews->nNewsState & NWS_ACTIONMASK) )
            {
            FailedExec();
            if( NULL != nfl.ptlTopicList )
               FreeMemory( (LPVOID)&nfl.ptlTopicList );
            nfl.cTopics = 0;
            SetNewsIdle( lpcNews );
            EndProcessing( 0xFFFF );
            fServiceActive = FALSE;
            return( FALSE );
            }
         }

      /* Connection rejected by remote server.
       */
      if( iCode == 502 )
         {
         fEntry = FALSE;
         if( AuthenticationFailed( lpcNews, iCode, pLine ) )
            return( FALSE );
         }

      /* Authentication info?
       */
      if( iCode == 480 )
         {
         lpcNews->nSavedNewsState = lpcNews->nNewsState;
         lpcNews->nNewsState = NWS_AUTHINFOUSER;
         fAuthenticated = TRUE;
         }

      /* Connection accepted. More to follow.
       */
      else if( iCode == 200 || iCode == 201 /*|| iCode == 224*/ /*!!SM!! 'data follows'*/)
         {
         fEntry = FALSE;
         return( TRUE );
         }
      }

   /* Use the state machine to determine what to do now.
    */
   switch( lpcNews->nNewsState & NWS_ACTIONMASK )
      {
      case NWS_AUTHINFOUSER:
         if( !*lpcNews->nsi.szAuthInfoUser )
            {
            /* User or password authentication failed, so
             * report error and fail the current command.
             */
            AuthenticationFailed( lpcNews, iCode, GS(IDS_STR809) );
            fServiceActive = FALSE;
            }
         else
            {
            wsprintf( lpTmpBuf, "authinfo user %s\r\n", lpcNews->nsi.szAuthInfoUser );
            WriteSocketLine( lpcdev, lpTmpBuf );
            lpcNews->nNewsState = NWS_AUTHINFOPASS;
            }
         break;

      case NWS_AUTINFOACCEPT:
      case NWS_AUTHINFOPASS:
         if( iCode > 400 )
            {
            /* User or password authentication failed, so
             * report error and fail the current command.
             */
            AuthenticationFailed( lpcNews, iCode, pLine );
            fServiceActive = FALSE;
            }
         else if( ( lpcNews->nNewsState & NWS_ACTIONMASK ) == NWS_AUTHINFOPASS )
            {
            char szPassword[ 40 ];

            memcpy( szPassword, lpcNews->nsi.szAuthInfoPass, sizeof(lpcNews->nsi.szAuthInfoPass) );
            Amuser_Decrypt( szPassword, rgEncodeKey );
            wsprintf( lpTmpBuf, "authinfo pass %s\r\n", (LPSTR)szPassword );
            memset( szPassword, 0, sizeof(lpcNews->nsi.szAuthInfoPass) );
            WriteSocketLineRedacted( lpcdev, lpTmpBuf, "authinfo pass ****************\r\n" );
            lpcNews->nNewsState = NWS_AUTINFOACCEPT;
            }
         else
            {
            /* Carry out original command which required authentication
             */
            WriteSocketLine( lpcdev, szLastCommand );
            lpcNews->nNewsState = lpcNews->nSavedNewsState;
            }
         break;

      case NWS_QUIT:
         WriteNewsSocketLine( lpcNews, "quit\r\n", NWS_WAITINGQUIT );
         break;

      case NWS_WAITINGQUIT:
         if( iCode == 250 )
            lpcNews->nNewsState = NWS_DISCONNECTED;
         break;

      case NWS_POSTING:
         if( iCode >= 400 )
            {
            ReportNewsOnline( lpcNews, pLine );
            FailedExec();
            if( hpHdrText )
               FreeMemory32( &hpHdrText );
            SetNewsIdle( lpcNews );
            EndProcessing( 0xFFFF );
            fServiceActive = FALSE;
            }
         else
            {
            HPSTR hpMsg;
            LPSTR lpszLineBuf;

            INITIALISE_PTR(lpszLineBuf);
            hpMsg = hpHdrText;
            if( fNewMemory( &lpszLineBuf, cbLineBuf ) )
               {
               while( *hpMsg )
                  {
                  register int c;

                  /* Find the end of the line and NULL terminate it. This
                   * makes working with strings much easier.
                   */
                  c = 0;
                  if( *hpMsg == '.' )
                     lpszLineBuf[ c++ ] = '.';
                  while( c < cbLineBuf - 3 && *hpMsg && *hpMsg != '\r' && *hpMsg != '\n' )
                     lpszLineBuf[ c++ ] = *hpMsg++;
//                while( c > 1 && lpszLineBuf[ c - 1 ] == ' ' )
//                   --c;
                  lpszLineBuf[ c++ ] = '\r';
                  lpszLineBuf[ c++ ] = '\n';
                  lpszLineBuf[ c ] = '\0';
                  WriteNewsSocketLine( lpcNews, lpszLineBuf, 0 );

                  /* Move hpMsg forward to start at the next line. Note the
                   * check for CR/CR/LF which is a soft line break under Windows.
                   */
                  if( *hpMsg == '\r' )
                     ++hpMsg;
                  if( *hpMsg == '\r' )
                     ++hpMsg;
                  if( *hpMsg == '\n' )
                     ++hpMsg;
                  }
               FreeMemory( &lpszLineBuf );
               }
            WriteNewsSocketLine( lpcNews, ".\r\n", NWS_POSTFINISH );
            }
         break;

      case NWS_POSTFINISH:
         if( iCode < 400 && nThisEncodedPart <= cEncodedParts )
            {
            /* Create the next header.
             */
            CreateNewsHeader( &NewsHdr );

            /* Append the next block of encoded data.
             */
            AppendMem32ToHeader( hpEncodedParts, hstrlen( hpEncodedParts ) );
            hpEncodedParts += hstrlen( hpEncodedParts ) + 1;
            ++nThisEncodedPart;

            /* Now start with the From line.
             */   
            WriteNewsSocketLine( lpcNews, "post\r\n", NWS_POSTING );
            }
         else
            {
            if( iCode >= 400 )
               {
               ReportNewsOnline( lpcNews, pLine );
               FailedExec();
               }
            else
               OnlineStatusText( GS(IDS_STR735) );
            if( NewsHdr.lpStrNewsgroup )  FreeMemory( &NewsHdr.lpStrNewsgroup );
            if( NewsHdr.lpStrBoundary )   FreeMemory( &NewsHdr.lpStrBoundary );
            if( NewsHdr.lpStrSubject )    FreeMemory( &NewsHdr.lpStrSubject );
            if( NewsHdr.lpStrReply )      FreeMemory( &NewsHdr.lpStrReply );
            if( hpHdrText )
               FreeMemory32( &hpHdrText );
            SetNewsIdle( lpcNews );
            EndProcessing( 0xFFFF );
            fServiceActive = FALSE;
            }
         break;

      case NWS_READTAGGED:
         if( iCode != 211 )
            {
            /* Newsgroup doesn't exist?
             */
            if( iCode >= 411 )
               {
               wsprintf( lpTmpBuf, GS(IDS_STR796), GetArticleNewsgroup( pthTagged ) );
               ReportOnline( lpTmpBuf );
               }

            /* No such newsgroup, so try the next one. If no
             * next newsgroup, then we're done.
             */
            if( pthTagged )
               {
               HWND hwndTopic2;

               Amdb_MarkMsgBeingRetrieved( pthTagged, FALSE );
               Amdb_MarkMsgTagged( pthTagged, FALSE );
               Amdb_MarkMsgUnavailable( pthTagged, TRUE );
               hwndTopic2 = Amdb_GetTopicSel( newshdr.ptl );
               if( hwndTopic2 )
                  SendMessage( hwndTopic2, WM_UPDMSGFLAGS, 0, (LPARAM)pthTagged );
               }
RT3:        Amdb_MarkTopicInUse( newshdr.ptl, FALSE );
            Amdb_UnlockTopic( newshdr.ptl );
            if( cTopicIdx < nfl.cTopics )
               {
               newshdr.ptl = nfl.ptlTopicList[ cTopicIdx++ ];
               Amdb_LockTopic( newshdr.ptl );
               pthTagged = Amdb_GetTagged( newshdr.ptl, NULL );
RT2:           if( !fSingleArticle && pthTagged )
                  {
                  char sz[ 100 ];
                  HWND hwndTopic2;

                  Amdb_MarkMsgBeingRetrieved( pthTagged, TRUE );
                  hwndTopic2 = Amdb_GetTopicSel( Amdb_TopicFromMsg( pthTagged ) );
                  if( hwndTopic2 )
                     SendMessage( hwndTopic2, WM_UPDMSGFLAGS, 0, (LPARAM)pthTagged );
                  if( Amdb_TopicFromMsg( pthTagged ) != newshdr.ptl )
                     {
                     LPNNTPSRVR lpcNewsNext;

                     /* We're switching newsgroups.
                      */
                     if( newshdr.ptl )
                        {
                        Amdb_MarkTopicInUse( newshdr.ptl, FALSE );
                        Amdb_UnlockTopic( newshdr.ptl );
                        }
                     newshdr.ptl = Amdb_TopicFromMsg( pthTagged );
                     Amdb_LockTopic( newshdr.ptl );
                     SetNewsIdle( lpcNews );

                     /* Open the news server for that newsgroup.
                      */
                     Amdb_MarkTopicInUse( newshdr.ptl, TRUE );
                     lpcNewsNext = EnsureNewsgroupServer( newshdr.ptl );
                     if( NULL == lpcNewsNext )
                        goto RT3;

                     /* Server available, so switch to it.
                      */
                     wsprintf( sz, "group %s\r\n", GetArticleNewsgroup( pthTagged ) );
                     WriteNewsSocketLine( lpcNewsNext, sz, NWS_READTAGGED );
                     }
                  else
                     {
                     MSGINFO msginfo;
            
                     Amdb_GetMsgInfo( pthTagged, &msginfo );
                     iNextArticle = msginfo.dwMsg;
                     wsprintf( sz, "article <%s>\r\n", (LPSTR)msginfo.szReply );
                     WriteNewsSocketLine( lpcNews, sz, NWS_READBODY );
                     }
                  break;
                  }
               }
            if( NULL != hwndInBasket )
               SendMessage( hwndInBasket, WM_UPDATE_UNREAD, 0, (LPARAM)newshdr.ptl );
            if( NULL != nfl.ptlTopicList )
               FreeMemory( (LPVOID)&nfl.ptlTopicList );
            nfl.cTopics = 0;
            OnlineStatusText( GS(IDS_STR696) );
            pthTagged = NULL;
            SetNewsIdle( lpcNews );
            EndProcessing( 0xFFFF );
            fServiceActive = FALSE;
            }
         else {
            char sz[ 100 ];

            if( NULL != pthTagged )
               {
               MSGINFO msginfo;

               Amdb_GetMsgInfo( pthTagged, &msginfo );
               iNextArticle = msginfo.dwMsg;
               wsprintf( sz, "article <%s>\r\n", (LPSTR)msginfo.szReply );
               }
            else
               wsprintf( sz, "article %lu\r\n", (LPSTR)iNextArticle );
            WriteNewsSocketLine( lpcNews, sz, NWS_READBODY );
            }
         break;

      case NWS_READBODY:
         if( iCode >= 400 )
            {
            if( pthTagged )
               {
               HWND hwndTopic2;

               Amdb_MarkMsgBeingRetrieved( pthTagged, FALSE );
               Amdb_MarkMsgTagged( pthTagged, FALSE );
               Amdb_MarkMsgUnavailable( pthTagged, TRUE );
               hwndTopic2 = Amdb_GetTopicSel( newshdr.ptl );
               if( hwndTopic2 )
                  SendMessage( hwndTopic2, WM_UPDMSGFLAGS, 0, (LPARAM)pthTagged );
               }
            lpcNews->nNewsState = NWS_READTAGGED;
            GetNextTagged();
            goto RT2;
            }
         else if( iCode == 220 )
            {
            /* We've got an article. Now start reading the body into
             * a memory block.
             */
            InitialiseHeader( &newshdr );
            newshdr.fInHeader = TRUE;
            lpcNews->nNewsState = NWS_READBODY2|NWS_ISBUSY;
            dwMsgLength = HPBUF_OFFSET;
            fAddedArticle = FALSE;

            /* Feedback on the status bar.
             */
            OnlineStatusText( GS(IDS_STR706), Amdb_GetTopicName( newshdr.ptl ), iNextArticle );
            }
         break;

      case NWS_READBODY2:
         if( pLine[ 0 ] == '.' && pLine[ 1 ] == '\0' )
            {
            /* We're all done. Add this message to the database, then
             * skip to the next message, if any.
             */
            if( dwMsgLength )
               {
               PTL ptl;
               int nMatch;

               /* Add a blank line.
                */
               AppendToMessage( "", TRUE );

               /* Put the message thru rules.
                */
               if( NULL != pthTagged )
                  Amdb_MarkMsgBeingRetrieved( pthTagged, FALSE );
               ptl = newshdr.ptl;
               nMatch = MatchRule( &ptl, hpBuf, &newshdr, TRUE );
               if( !( nMatch & FLT_MATCH_COPY ) && ptl != newshdr.ptl )
                  {
                  Amdb_LockTopic( ptl );
                  Amdb_MarkTopicInUse( newshdr.ptl, TRUE );
                  newshdr.ptl = ptl;
                  }
               if( !( nMatch & FLT_MATCH_DELETE ) )
                  {
                  HWND hwndTopic2;
                  int cbHdr;
                  register int r = 0;

                  /* Create a news header.
                   */
                  cbHdr = wsprintf( hpBuf, "#! rnews %lu\r\n", dwMsgLength - HPBUF_OFFSET );
                  ASSERT( cbHdr <= 32 );
                  hmemcpy( hpBuf + cbHdr, hpBuf + HPBUF_OFFSET, dwMsgLength - HPBUF_OFFSET );
                  dwMsgLength -= HPBUF_OFFSET - cbHdr;
                  if( NULL == pthTagged )
                     {
                        if( *newshdr.szReference )
                           {
                              DWORD dwPossibleComment;

                              if( dwPossibleComment = Amdb_MessageIdToComment( newshdr.ptl, newshdr.szReference ) )
                              {
                                 newshdr.dwComment = dwPossibleComment;
                                 newshdr.fPossibleRefScanning = FALSE;
                              }
                           }

                        /* If this message had an Re: and no matching message ID, scan for the
                         * original by doing Re: pattern matching.
                         */
                        if( newshdr.dwComment == 0L && fThreadMailBySubject && newshdr.fPossibleRefScanning )
                           newshdr.dwComment = Amdb_LookupSubjectReference( newshdr.ptl, newshdr.szTitle, "" );
                        if( strlen (newshdr.szAuthor ) == 0 )
                           strcpy( newshdr.szAuthor, szDefaultAuthor );
                        r = Amdb_CreateMsg( newshdr.ptl, &pthTagged, iNextArticle, newshdr.dwComment, newshdr.szTitle, newshdr.szAuthor, newshdr.szMessageId, &newshdr.date, &newshdr.time, hpBuf, dwMsgLength, 0L );
                     }
                  else
                     Amdb_ReplaceMsgText( pthTagged, hpBuf, dwMsgLength );

                  /* Decode if required.
                   */
                  if( NULL != pthTagged )
                     {
                     /* Save number of lines.
                      */
                     if( newshdr.cLines > 0 )
                        Amdb_SetLines( pthTagged, newshdr.cLines );

                     /* Take post storage action based on rules
                      * match.
                      */
                     Amdb_GetCookie( newshdr.ptl, NEWS_EMAIL_COOKIE, szTmpName, szMailAddress, 64 );
                     if( _strcmpi( szTmpName, newshdr.szAuthor ) == 0 )
                        newshdr.fPriority = TRUE;

                     if( r == AMDBERR_NOERROR )
                     {

                     if( nMatch & FLT_MATCH_COPY && ( ptl != newshdr.ptl ) )
                     {
                     PTH FAR * lppth;

                     /* Okay, message got moved so move the message after
                      * we've applied all the above.
                      */
                     INITIALISE_PTR(lppth);
                     if( fNewMemory( (PTH FAR *)&lppth, 2 * sizeof( PTH ) ) )
                        {
                        lppth[ 0 ] = pthTagged;
                        lppth[ 1 ] = NULL;
                        DoCopyMsgRange( hwndFrame, lppth, 1, ptl, FALSE, FALSE, FALSE );
                        }
                     }
                     if( ( nMatch & FLT_MATCH_PRIORITY ) || newshdr.fPriority )
                        Amdb_MarkMsgPriority( pthTagged, TRUE, TRUE );
                     if( !( nMatch & FLT_MATCH_READ ) && !Amdb_IsIgnored( pthTagged ) )
                        Amdb_MarkMsgRead( pthTagged, FALSE );
                     if( nMatch & FLT_MATCH_IGNORE )
                        Amdb_MarkMsgIgnore( pthTagged, TRUE, TRUE );
                     if( newshdr.fXref )
                        Amdb_MarkMsgCrossReferenced( pthTagged, TRUE );

                     Amdb_MarkHdrOnly( pthTagged, FALSE );
                     Amdb_MarkMsgTagged( pthTagged, FALSE );
                     Amdb_MarkMsgUnavailable( pthTagged, FALSE );
                     if( nMatch & FLT_MATCH_READLOCK )
                        Amdb_MarkMsgReadLock( pthTagged, TRUE );
                     if( nMatch & FLT_MATCH_KEEP )
                        Amdb_MarkMsgKeep( pthTagged, TRUE );
                     if( nMatch & FLT_MATCH_MARK )
                        Amdb_MarkMsg( pthTagged, TRUE );
                     if( nMatch & FLT_MATCH_WATCH )
                        {
                     /* Only get article if DESTINATION topic is a newsgroup
                      * and we've currently only got the header.
                      */
                     if( Amdb_GetTopicType( newshdr.ptl ) == FTYPE_NEWS )
                           Amdb_MarkMsgWatch( pthTagged, TRUE, TRUE );
                        }

                     hwndTopic2 = Amdb_GetTopicSel( ptl );
                     if( hwndTopic2 )
                        SendMessage( hwndTopic2, WM_UPDMSGFLAGS, 0, (LPARAM)pthTagged );
                     hwndTopic2 = Amdb_GetTopicSel( newshdr.ptl );
                     if( hwndTopic2 )
                        SendMessage( hwndTopic2, WM_UPDMSGFLAGS, 0, (LPARAM)pthTagged );
                     }
                     ++dwTaggedRetrieved;
                     }
                  }
               if( !( nMatch & FLT_MATCH_COPY ) && ptl != newshdr.ptl )
                  {
                  Amdb_MarkTopicInUse( ptl, FALSE );
                  Amdb_UnlockTopic( ptl );
                  }
               newshdr.dwComment = 0L;
               }
            lpcNews->nNewsState = NWS_READTAGGED;
            GetNextTagged();
            goto RT2;
            }
         else
            {
            BOOL fNewline;

            /* First, add this line to the memory buffer.
            /* Next, parse the contents.
             */
            if( !fAddedArticle )
               {
               wsprintf( lpTmpBuf, "Article: %lu of %s", iNextArticle, Amdb_GetTopicName( newshdr.ptl ) );
               AppendToMessage( lpTmpBuf, TRUE );
               fAddedArticle = TRUE;
               }
            ParseSocketLine( pLine, &newshdr );
            fNewline = ApplyEncodingType( pLine, &newshdr );
            AppendToMessage( pLine, fNewline );
            if( *newshdr.szAuthor )
            {
               if( strcmp( szMailAddress, newshdr.szAuthor ) == 0 )
                  newshdr.fPriority = TRUE;
               else
               {
                  Amdb_GetCookie( newshdr.ptl, NEWS_EMAIL_COOKIE, szTmpName, szMailAddress, 64 );
                  if( strcmp( szTmpName, newshdr.szAuthor ) == 0 )
                     newshdr.fPriority = TRUE;
               }
            }

            }
         break;

      case NWS_READNEWSGROUPSIZE: {
         TOPICINFO topicinfo;

         /* Newsgroup doesn't exist?
          */
         if( iCode >= 411 )
            {
            wsprintf( lpTmpBuf, GS(IDS_STR934), Amdb_GetTopicName( newshdr.ptl ) );
            ReportOnline( lpTmpBuf );
            }

         /* We're in a newsgroup. Parse off the number of the
          * last article and remember it.
          */
         if( iCode == 211 )
            sscanf( pLine, "%u %lu %lu %lu", &iCode, &iTotalArticles, &iFirstArticle, &iLastArticle );

         /* Expire all articles from topicinfo.dwLastExpired to iFirstArticle-1.
          * BUGBUG: Not finished yet!
          */
         Amdb_GetTopicInfo( newshdr.ptl, &topicinfo );

         /* If no such group, or iTotalArticles is zero, skip to
          * the next group.
          */
         if( iCode != 211 || iTotalArticles == 0 || topicinfo.dwUpperMsg >= (DWORD)iLastArticle )
            {
            /* Warn if articles have been renumbered in this group. This would
             * be an ideal time to resolve the problem automatically.
             */
            if( iCode == 211 && ( topicinfo.dwUpperMsg > (DWORD)iLastArticle ) &&
               ( 0 != iTotalArticles + iFirstArticle + iLastArticle ) )
               {
               wsprintf( lpTmpBuf, GS(IDS_STR1118), Amdb_GetTopicName( newshdr.ptl ), iLastArticle );
               WriteToBlinkLog( lpTmpBuf );
               }

            /* No such newsgroup, so try the next one. If no
             * next newsgroup, then we're done.
             */
            if( iCode != 502 && cTopicIdx < nfl.cTopics )
            {
               LPNNTPSRVR lpcNewsNext;
               char sz[ 100 ];

               lpcNewsNext = NULL;
               while( cTopicIdx < nfl.cTopics )
               {
                  Amdb_MarkTopicInUse( newshdr.ptl, FALSE );
                  Amdb_UnlockTopic( newshdr.ptl );
                  newshdr.ptl = nfl.ptlTopicList[ cTopicIdx++ ];
                  Amdb_LockTopic( newshdr.ptl );
                  SetNewsIdle( lpcNews );
                  Amdb_MarkTopicInUse( newshdr.ptl, TRUE );
                  lpcNewsNext = EnsureNewsgroupServer( newshdr.ptl );
                  if( iCode >= 500 && lpcNewsNext == lpcNews )
                     continue;
                  if( NULL != lpcNewsNext )
                     break;
               }

               /* If we got a working news server, use it.
                */
               if( NULL != lpcNewsNext )
               {
                  wsprintf( sz, "group %s\r\n", Amdb_GetTopicName( newshdr.ptl ) );
                  WriteNewsSocketLine( lpcNewsNext, sz, NWS_READNEWSGROUPSIZE );
                  break;
               }
            }
            FreeMemory( (LPVOID)&nfl.ptlTopicList );
            nfl.cTopics = 0;
            OnlineStatusText( GS(IDS_STR696) );
            if( newshdr.ptl )
            {
               Amdb_MarkTopicInUse( newshdr.ptl, FALSE );
               Amdb_UnlockTopic( newshdr.ptl );
            }
            SetNewsIdle( lpcNews );
            EndProcessing( 0xFFFF );
            fServiceActive = FALSE;
         }
         else
         {
            char sz[ 100 ];

            /* Find the largest article number in this newsgroup. If this is
             * a new newsgroup, then topicinfo.dwUpperMsg will be zero, so we'll
             * start from the first article in the entire topic.
             */
            if( topicinfo.dwFlags & TF_READ50 )
               iFirstArticle = max( iFirstArticle, ( iLastArticle - 50 ) + 1 );
            else
               iFirstArticle = max( iFirstArticle, (long)topicinfo.dwUpperMsg + 1 );
            iNextArticle = iFirstArticle;
            if( topicinfo.dwFlags & TF_READFULL )
               {
               wsprintf( sz, "article %lu\r\n", iNextArticle );
               WriteNewsSocketLine( lpcNews, sz, NWS_READFULL );
               fReadFull = TRUE;
               OnlineStatusText( GS(IDS_STR871), Amdb_GetTopicName( newshdr.ptl ) );
               }
            else {
               if( lpcNews->fCanDoXover )
                  {
                  wsprintf( sz, "xover %lu-%lu\r\n", iNextArticle, iLastArticle );
                  WriteNewsSocketLine( lpcNews, sz, NWS_READXOVER );
                  fReadFull = FALSE;
                  }
               else
                  {
                  wsprintf( sz, "head %lu\r\n", iNextArticle );
                  WriteNewsSocketLine( lpcNews, sz, NWS_READHEAD );
                  fReadFull = FALSE;
                  }
               OnlineStatusText( GS(IDS_STR697), Amdb_GetTopicName( newshdr.ptl ) );
               }
            wOldCount = 0;
            ResetBlinkGauge();
            }
         break;
         }

      case NWS_READFULL:
         if( iCode >= 400 )
         {
            if( iNextArticle >= iLastArticle )
               {
               EndGetHeaders( lpcNews );
               fServiceActive = FALSE;
               }
            else
               {
               char sz[ 20 ];

               /* No such article, so use the next command to find the
                * next valid article.
                */
               wsprintf( sz, "article %lu\r\n", ++iNextArticle );
               WriteNewsSocketLine( lpcNews, sz, NWS_READFULL );
               }
         }
         else if( iCode == 220 )
         {
            long iRange;
            WORD wCount;

            /* We've got an article. Now start reading the article into
             * a memory block.
             */
            InitialiseHeader( &newshdr );
            newshdr.fInHeader = TRUE;
            lpcNews->nNewsState = NWS_READHEADER|NWS_ISBUSY;
            dwMsgLength = HPBUF_OFFSET;
            fAddedArticle  = FALSE;

            /* Feedback on the status bar.
             */
            iRange = max( iLastArticle - iFirstArticle, 1 );
            wCount = (WORD)( ( (DWORD)( iNextArticle - iFirstArticle ) * 100 ) / iRange );
            if( wCount != wOldCount )
               {
               StepBlinkGauge( wCount );
               //ShowDownloadCount( wCount, "Total New Headers: " ); /*!!SM!!*/
               wOldCount = wCount;
               }
         }
         break;

      case NWS_READXOVER:
         if( iCode == 500 )
            {
            char sz[ 20 ];

            /* XOVER not implemented, so fall back to the good
             * old header scheme.
             */
            WriteToBlinkLog( GS(IDS_STR1098) );
            lpcNews->fCanDoXover = FALSE;
            wsprintf( sz, "head %lu\r\n", iNextArticle );
            WriteNewsSocketLine( lpcNews, sz, NWS_READHEAD );
            }
         else
            {
            dwHeadersRetrieved = 0;
            lpcNews->nNewsState = NWS_READXOVERLINE|NWS_ISBUSY;
            }
         break;

      case NWS_READXOVERLINE:
         /* We have a line from the xover command.
          */
         if( pLine[ 0 ] == '.' && pLine[ 1 ] == '\0' )
            {
            EndGetHeaders( lpcNews );
            fServiceActive = FALSE;
            }
         else
            {
            PTL ptl;
            char * pReference;
            char * pMsgNumber;
            char * pLinePtr;
            char * pSubject;
            char * pAuthor;
            char * pDateStr;
            char * pMessageID;
            DWORD cMsgSize;
            int nMatch;
            long iRange;
            WORD wCount;

            /* Parse off the information from the line, then feed it into
             * CreateMsg.
             */
            pLinePtr = pLine;
            newshdr.fPossibleRefScanning = FALSE;
            *newshdr.szType = '\0';
            newshdr.fPriority = FALSE;
            newshdr.fMime = FALSE;
            newshdr.fXref = FALSE;
            newshdr.fInHeader = TRUE;
            newshdr.dwComment = 0L;
            dwMsgLength = HPBUF_OFFSET;

            /* Parse the message number.
             */
            iNextArticle = 0;
            pMsgNumber = pLinePtr;
            while( isdigit( *pLinePtr ) )
               iNextArticle = ( iNextArticle * 10 ) + ( *pLinePtr++ - '0' );
            if( *pLinePtr == '\t' )
               *pLinePtr++ = '\0';

            /* Feedback on the status bar.
             */
            iRange = max( iLastArticle - iFirstArticle, 1 );
            wCount = (WORD)( ( (DWORD)( iNextArticle - iFirstArticle ) * 100 ) / ( iRange ) );
            if( wCount != wOldCount )
               {
               if (wCount < 100) /*!!SM!!*/
                  StepBlinkGauge( wCount );
               //ShowDownloadCount( wCount, "Total Articles: " ); /*!!SM!!*/
               wOldCount = wCount;
               }

            /* Parse subject
             */
            pSubject = pLinePtr;
            while( *pLinePtr && *pLinePtr != '\t' )
               ++pLinePtr;
            if( *pLinePtr == '\t' )
               *pLinePtr++ = '\0';
            ParseMailSubject( pSubject, &newshdr );

            /* Parse the author.
             */
            pAuthor = pLinePtr;
            while( *pLinePtr && *pLinePtr != '\t' )
               ++pLinePtr;
            if( *pLinePtr == '\t' )
               *pLinePtr++ = '\0';
            ParseMailAuthor( pAuthor, &newshdr );

            /* Parse the date
             */
            pDateStr = pLinePtr;
            while( *pLinePtr && *pLinePtr != '\t' )
               ++pLinePtr;
            if( *pLinePtr == '\t' )
               *pLinePtr++ = '\0';
            ParseMailDate( pDateStr, &newshdr );

            /* Parse the message ID.
             */
            pMessageID = pLinePtr;
            while( *pLinePtr && *pLinePtr != '\t' )
               ++pLinePtr;
            if( *pLinePtr == '\t' )
               *pLinePtr++ = '\0';
            ParseMessageId( pMessageID, &newshdr, FALSE );

            /* Parse the reference.
             */
            pReference = pLinePtr;
            while( *pLinePtr && *pLinePtr != '\t' )
               ++pLinePtr;
            if( *pLinePtr == '\t' )
               *pLinePtr++ = '\0';
            ParseReference( pReference, &newshdr );

            /* Parse the message size and number of lines.
             */
            cMsgSize = 0;
            while( isdigit( *pLinePtr ) )
               cMsgSize = cMsgSize * 10 + ( *pLinePtr++ - '0' );
            if( *pLinePtr == '\t' )
               *pLinePtr++ = '\0';
            newshdr.cLines = 0;
            while( isdigit( *pLinePtr ) )
               newshdr.cLines = newshdr.cLines * 10 + ( *pLinePtr++ - '0' );

            /* Create the header.
             */
            wsprintf( lpTmpBuf, "Article: %lu of %s", iNextArticle, Amdb_GetTopicName( newshdr.ptl ) );
            AppendToMessage( lpTmpBuf, TRUE );
            wsprintf( lpTmpBuf, "From: %s", (LPSTR)pAuthor );
            AppendToMessage( lpTmpBuf, TRUE );
            wsprintf( lpTmpBuf, "Newsgroups: %s", Amdb_GetTopicName( newshdr.ptl ) );
            AppendToMessage( lpTmpBuf, TRUE );
            wsprintf( lpTmpBuf, "Subject: %s", (LPSTR)pSubject );
            AppendToMessage( lpTmpBuf, TRUE );
            wsprintf( lpTmpBuf, "Message-ID: %s", (LPSTR)pMessageID );
            AppendToMessage( lpTmpBuf, TRUE );
            wsprintf( lpTmpBuf, "Lines: %u", newshdr.cLines );
            AppendToMessage( lpTmpBuf, TRUE );
            wsprintf( lpTmpBuf, "Date: %s", (LPSTR)pDateStr );
            AppendToMessage( lpTmpBuf, TRUE );

            /* Got everything, so bung it through the rule manager
             * and then put it into the database if applicable.
             */
            ptl = newshdr.ptl;
            nMatch = MatchRule( &ptl, hpBuf, &newshdr, TRUE );
            if( !( nMatch & FLT_MATCH_COPY ) && ptl != newshdr.ptl )
               {
               Amdb_LockTopic( ptl );
               Amdb_MarkTopicInUse( newshdr.ptl, TRUE );
               }
            if( !( nMatch & FLT_MATCH_DELETE ) )
               AddNewArticle( ptl, newshdr.ptl, nMatch, TRUE );
            newshdr.dwComment = 0L;
            if( !( nMatch & FLT_MATCH_COPY ) && ptl != newshdr.ptl )
               {
               Amdb_MarkTopicInUse( ptl, FALSE );
               Amdb_UnlockTopic( ptl );
               }
            }
         break;

      case NWS_READHEAD:
         if( iCode == 423 )
         {
            char sz[ 20 ];

            /* No such article, so use the next command to find the
             * next valid article.
             */
            wsprintf( sz, "head %lu\r\n", ++iNextArticle );
            WriteNewsSocketLine( lpcNews, sz, NWS_READHEAD );
         }
         else if( iCode == 221 )
         {
            long iRange;
            WORD wCount;

            /* We've got an article. Now start reading the header into
             * a memory block.
             */
            InitialiseHeader( &newshdr );
            newshdr.fInHeader = TRUE;
            Amdate_GetCurrentDate( &newshdr.date );
            lpcNews->nNewsState = NWS_READHEADER|NWS_ISBUSY;
            dwMsgLength = HPBUF_OFFSET;
            fAddedArticle  = FALSE;

            /* Feedback on the status bar.
             */
            iRange = max( iLastArticle - iFirstArticle, 1 );
            wCount = (WORD)( ( (DWORD)( iNextArticle - iFirstArticle ) * 100 ) / ( iRange ) );
            if( wCount != wOldCount )
               {
               StepBlinkGauge( wCount );
               //ShowDownloadCount( wCount, "Total Headers: " ); /*!!SM!!*/
               wOldCount = wCount;
               }
         }
         break;

      case NWS_READARTICLE: {
         char sz[ 10 ];

         wsprintf( sz, "next\r\n" );
         WriteNewsSocketLine( lpcNews, sz, NWS_READNEXT );
         break;
         }

      case NWS_READNEXT:
         if( iCode == 421 )
            {
            /* No next article, so try the next newsgroup. If no
             * next newsgroup, then we're done.
             */
            EndGetHeaders( lpcNews );
            fServiceActive = FALSE;
            }
         else
            {
            char sz[ 20 ];
   
            /* Parse the Next command. Read the number of the next article
             * and issue a 'head' command for that.
             */
            sscanf( pLine, "%u %lu", &iCode, &iNextArticle );
            wsprintf( sz, "head %lu\r\n", iNextArticle );
            WriteNewsSocketLine( lpcNews, sz, NWS_READHEAD );
            }
         break;

      case NWS_READHEADER:
         if( pLine[ 0 ] == '.' && pLine[ 1 ] == '\0' )
            {
            char sz[ 20 ];

            /* We're all done. Add this message to the database, then
             * skip to the next message, if any.
             */
            if( dwMsgLength )
               {
               PTL ptl;
               int nMatch;

               ptl = newshdr.ptl;
               nMatch = MatchRule( &ptl, hpBuf, &newshdr, TRUE );
               if( !( nMatch & FLT_MATCH_DELETE ) )
                  AddNewArticle( ptl, newshdr.ptl, nMatch, !fReadFull );
               newshdr.dwComment = 0L;
               }

            /* Got all articles in this group? Move to the
             * next group or finish the news.
             */
            if( iNextArticle == iLastArticle )
               {
               EndGetHeaders( lpcNews );
               fServiceActive = FALSE;
               }
            else
               {
               TOPICINFO topicinfo;

               Amdb_GetTopicInfo( newshdr.ptl, &topicinfo );
               if( topicinfo.dwFlags & TF_READFULL )
                  {
                  wsprintf( sz, "article %lu\r\n", ++iNextArticle );
                  WriteNewsSocketLine( lpcNews, sz, NWS_READFULL );
                  }
               else
                  {
                  wsprintf( sz, "head %lu\r\n", ++iNextArticle );
                  WriteNewsSocketLine( lpcNews, sz, NWS_READHEAD );
                  }
               }
            }
         else
            {
            BOOL fNewline;

            /* First, add this line to the memory buffer.
            /* Next, parse the contents.
             */
            if( !fAddedArticle )
               {
               wsprintf( lpTmpBuf, "Article: %lu of %s", iNextArticle, Amdb_GetTopicName( newshdr.ptl ) );
               AppendToMessage( lpTmpBuf, TRUE );
               fAddedArticle = TRUE;
               }
            ParseSocketLine( pLine, &newshdr );
            fNewline = ApplyEncodingType( pLine, &newshdr );
            AppendToMessage( pLine, fNewline );
            if( *newshdr.szAuthor )
               if( strcmp( szMailAddress, newshdr.szAuthor ) == 0 )
                  newshdr.fPriority = TRUE;
            }
         break;

      case NWS_GETNEWGROUPS:
         /* We're receiving a list of new newsgroups, so append
          * the list to the existing newsgroup list.
          */
         if( *pLine == '.' )
         {
            /* All done, save what we got.
             */
            if( NULL != lpFilters )
               FreeMemory( &lpFilters );
            if( fInsertGroup )
               SaveNewsgroupDataFile( lpcNews );
            
            FinishStepingBlinkGauge( (int) (( cGroupsRead * 100 ) / fMaxNewsgroups) );

/*
            for( iGroup = ( cGroupsRead / 300 ); iGroup <= 100; ++iGroup )
               StepBlinkGauge( iGroup );
*/
            ShowDownloadCount( 0, "" ); /*!!SM!!*/
            SetNewsIdle( lpcNews );
            EndProcessing( 0xFFFF );
            HideBlinkGauge();
            OnlineStatusText( GS(IDS_STR698) );

            /* Report to the blink log
             */
            wsprintf( lpTmpBuf, GS(IDS_STR1085), lpcNews->nsi.szServerName );
            WriteToBlinkLog( lpTmpBuf );

            /* Save the date and time when we updated the
             * list.
             */
            lpcNews->nsi.wUpdateDate = Amdate_GetPackedCurrentDate();
            lpcNews->nsi.wUpdateTime = Amdate_GetPackedCurrentTime();
            SetUsenetServerDetails( &lpcNews->nsi );
            fServiceActive = FALSE;
         }
         else if( iCode == 231 )
         {
            /* OK from server, so prepare to read the new
             * newsgroups.
             */
            LoadNewsgroupDataFile( lpcNews );
            szLastNewsgroup[ 0 ] = '\0';
            OnlineStatusText( GS(IDS_STR699) );
            lpcNews->nNewsState |= NWS_ISBUSY;
            fInsertGroup = TRUE;
            lpFilters = LoadFilters( &lpcNews->nsi, &wFilterType );
            hpLastNewsPtr = NULL;
         }
         else
         {

            ++cGroupsRead;
            if( hwndStatus && ( cGroupsRead % 10 ) == 0 )
            {
               if( NULL == hwndBlink )
                  OnlineStatusText( GS(IDS_STR700), cGroupsRead );
               else
               {
                  if( cGroupsRead == 10 )
                  {
                     OnlineStatusText( "Reading new newsgroups..." );
                     ResetBlinkGauge();
                  }

                  if ( cGroupsRead <= fMaxNewsgroups && fMaxNewsgroups > 0)
                     StepBlinkGauge( (int) (( cGroupsRead * 100 ) / fMaxNewsgroups) ); // !!SM!!
                  else
                     fMaxNewsgroups = max(fMaxNewsgroups, cGroupsRead);
                     
/*
                  if( ( cGroupsRead / 300 ) < 100 )
                  {
                     StepBlinkGauge( cGroupsRead / 300 );
                  }
*/
                  ShowDownloadCount( cGroupsRead, "Total New Newsgroups: " ); /*!!SM!!*/
               }
            }

            if( fInsertGroup )
               if( !InsertNewsgroup( pLine, lpFilters, wFilterType, TRUE ) )
                  fInsertGroup = FALSE;
         }
         break;

      case NWS_READDESCRIPTIONS:
         /* We're reading a list of newsgroup descriptions.
          */
         if( iCode >= 400 || *pLine == '.' )
         {
            /* All done, save what we got.
             */
            if( fInsertGroup )
            {
               MergeDescriptions();
               SaveNewsgroupDataFile( lpcNews );
            }
            FinishStepingBlinkGauge( (int) (( cGroupsRead * 100 ) / fMaxNewsgroups) );

/*
            for( iGroup = ( cGroupsRead / 300 ); iGroup <= 100; ++iGroup )
               StepBlinkGauge( iGroup );
*/
            SetNewsIdle( lpcNews );
            EndProcessing( 0xFFFF );
            HideBlinkGauge();
            OnlineStatusText( GS(IDS_STR701) );

            /* Report to the blink log
             */
            wsprintf( lpTmpBuf, GS(IDS_STR1085), lpcNews->nsi.szServerName );
            WriteToBlinkLog( lpTmpBuf );

            /* Save the date and time when we updated the
             * list.
             */
            lpcNews->nsi.wUpdateDate = Amdate_GetPackedCurrentDate();
            lpcNews->nsi.wUpdateTime = Amdate_GetPackedCurrentTime();
            SetUsenetServerDetails( &lpcNews->nsi );
            fServiceActive = FALSE;
         }
         else if( iCode == 215 )
         {
            lpcNews->nNewsState |= NWS_ISBUSY;
            szLastNewsgroup[ 0 ] = '\0';
            fInsertGroup = TRUE;
         }
         else
         {
            char * pDescription;

            ++cGroupsRead;
            if( hwndStatus && ( cGroupsRead % 10 ) == 0 )
            {
               if( NULL == hwndBlink )
                  OnlineStatusText( GS(IDS_STR804 ), cGroupsRead );
               else
               {
                  if( cGroupsRead == 10 )
                  {
                     OnlineStatusText( "Reading descriptions..." );
                     ResetBlinkGauge();
                  }

                  if ( cGroupsRead <= fMaxNewsgroups && fMaxNewsgroups > 0)
                     StepBlinkGauge( (int) (( cGroupsRead * 100 ) / fMaxNewsgroups) ); /*!!SM!!*/
                  else
                     fMaxNewsgroups = max(fMaxNewsgroups, cGroupsRead);
/*
                  if( ( cGroupsRead / 300 ) < 100 )
                  {
                     StepBlinkGauge( cGroupsRead / 300 );
                  }
*/
                  ShowDownloadCount( cGroupsRead, "Total Descriptions: " ); /*!!SM!!*/
               }
            }

            if( fInsertGroup )
            {
               for( pDescription = pLine; *pDescription && !isspace( *pDescription ); ++pDescription );
               if( *pDescription )
               {
                  *pDescription++ = '\0';
                  while( isspace( *pDescription ) )
                     ++pDescription;
               }
               if( !InsertNewsgroupDescription( pLine, pDescription ) )
                  fInsertGroup = FALSE;
            }
         }
         break;

      case NWS_GETLIST:
         /* We're receiving a full list of newsgroups. If the line
          * doesn't start with a '.', then we've got another line, so
          * write the details to the newsgroup list.
          */
         if( *pLine == '.' )
         {
            if( NULL != lpFilters )
               FreeMemory( &lpFilters );
            if( lpcNews->nsi.fGetDescriptions && fInsertGroup )
            {
               WriteNewsSocketLine( lpcNews, "list newsgroups\r\n", NWS_READDESCRIPTIONS );
               cGroupsRead = 0;
            }
            else
            {
               /* All done, save what we got.
                */
               if( fInsertGroup )
                  SaveNewsgroupDataFile( lpcNews );

               /* Step the progress bar up to 100
                */
               FinishStepingBlinkGauge( (int) (( cGroupsRead * 100 ) / fMaxNewsgroups) );
/*
               for( iGroup = ( cGroupsRead / 300 ); iGroup <= 100; ++iGroup )
                  StepBlinkGauge( iGroup );
*/
               ShowDownloadCount( 0, "" ); /*!!SM!!*/

               SetNewsIdle( lpcNews );
               EndProcessing( 0xFFFF );
               HideBlinkGauge();
               OnlineStatusText( GS(IDS_STR701) );

               /* Report to the blink log
                */
               wsprintf( lpTmpBuf, GS(IDS_STR1085), lpcNews->nsi.szServerName );
               WriteToBlinkLog( lpTmpBuf );

               /* Save the date and time when we updated the
                * list.
                */
               lpcNews->nsi.wUpdateDate = Amdate_GetPackedCurrentDate();
               lpcNews->nsi.wUpdateTime = Amdate_GetPackedCurrentTime();
               SetUsenetServerDetails( &lpcNews->nsi );
               fServiceActive = FALSE;
            }
         }
         else if( iCode == 215 )
         {
            /* Ready to start reading new groups.
             */
            hpNewsBuf = NULL;
            lpcNews->nNewsState |= NWS_ISBUSY;
            szLastNewsgroup[ 0 ] = '\0';
            fInsertGroup = TRUE;
            lpFilters = LoadFilters( &lpcNews->nsi, &wFilterType );
            hpLastNewsPtr = NULL;
         }
         else
         {
            ++cGroupsRead;
            if( hwndStatus && ( cGroupsRead % 10 ) == 0 )
            {
               if( NULL == hwndBlink )
                  OnlineStatusText( GS(IDS_STR700), cGroupsRead );
               else
               {
                  if( cGroupsRead == 10 )
                  {                 
                     OnlineStatusText( "Reading newsgroups..." );
                     ResetBlinkGauge();
                  }
                  
                  if ( cGroupsRead <= fMaxNewsgroups && fMaxNewsgroups > 0)
                     StepBlinkGauge( (int) (( cGroupsRead * 100 ) / fMaxNewsgroups) ); /*!!SM!!*/
                  else
                     fMaxNewsgroups = max(fMaxNewsgroups, cGroupsRead);
/*
                  if( ( cGroupsRead / 300 ) < 100 )
                  {
                     StepBlinkGauge( cGroupsRead / 300 );
                  }
*/
                  ShowDownloadCount( cGroupsRead, "Total Newsgroups: " ); /*!!SM!!*/
               }
            }

            if( fInsertGroup )
               if( !InsertNewsgroup( pLine, lpFilters, wFilterType, FALSE ) )
                  fInsertGroup = FALSE;
         }
         break;
      }
   fEntry = FALSE;
   return( fServiceActive );

}

/* An authentication has failed.
 */
BOOL FASTCALL AuthenticationFailed( LPNNTPSRVR lpcNews, int iCode, char * pszError )
{
   /* Fail this news server session.
    */
   ReportNewsOnline( lpcNews, pszError );
   HideBlinkGauge();
   if( !IsBatchServerState(lpcNews->nNewsState & NWS_ACTIONMASK) )
      {
      FailedExec();
      if( NULL != nfl.ptlTopicList )
         FreeMemory( (LPVOID)&nfl.ptlTopicList );
      nfl.cTopics = 0;
      if( hpHdrText )
         FreeMemory32( &hpHdrText );
      SetNewsIdle( lpcNews );
      EndProcessing( 0xFFFF );
      return( TRUE );
      }
   return( FALSE );
}

/* Report online for a specific news server.
 */
void FASTCALL ReportNewsOnline( LPNNTPSRVR lpcNews, char * pszError )
{
   while( *pszError && *pszError != ' ' )
      ++pszError;
   wsprintf( lpTmpBuf, "%s:%s", lpcNews->szServerName, pszError );
   ReportOnline( lpTmpBuf );
}

/* This function adds the newly collected news article to the
 * database.
 */
void FASTCALL AddNewArticle( PTL ptl, PTL ptl2, int nMatch, BOOL fHeaderOnly )
{
   int cbHdr;
   PTH pth;
   register int r;
   PTL ptl3;

   if( nMatch & FLT_MATCH_COPY )
   {
      ptl3 = ptl2;
      ptl2 = ptl;
      ptl = ptl3;
   }

   if( !fHeaderOnly && *newshdr.szReference )
   {
      DWORD dwPossibleComment;
      if( dwPossibleComment = Amdb_MessageIdToComment( ptl, newshdr.szReference ) )
      {
         newshdr.dwComment = dwPossibleComment;
         newshdr.fPossibleRefScanning = FALSE;
      }
   }

   /* If this message had an Re: and no matching message ID, scan for the
    * original by doing Re: pattern matching.
    */
   if( fThreadMailBySubject && newshdr.dwComment == 0L && newshdr.fPossibleRefScanning )
      newshdr.dwComment = Amdb_LookupSubjectReference( ptl, newshdr.szTitle, "" );

   /* Set a title if there wasn't one.
    */
   if( *newshdr.szTitle == '\0' )
      strcpy( newshdr.szTitle, GS(IDS_STR933) );

   /* Add a blank line.
    */
   AppendToMessage( "", TRUE );

   /* Get the topic info and use that to
    * create a news header.
    */
   cbHdr = wsprintf( hpBuf, "#! rnews %lu\r\n", dwMsgLength - HPBUF_OFFSET );
   ASSERT( cbHdr <= 32 );
   hmemcpy( hpBuf + cbHdr, hpBuf + HPBUF_OFFSET, dwMsgLength - HPBUF_OFFSET );
   dwMsgLength -= HPBUF_OFFSET - cbHdr;
   if( strlen (newshdr.szAuthor ) == 0 )
      strcpy( newshdr.szAuthor, szDefaultAuthor );
   r = Amdb_CreateMsg( ptl, &pth, iNextArticle, newshdr.dwComment, newshdr.szTitle, newshdr.szAuthor, newshdr.szMessageId, &newshdr.date, &newshdr.time, hpBuf, dwMsgLength, 0L );

   /* Decode if required.
    */
   if( NULL != pth )
      {
      /* Save number of lines.
       */
      if( newshdr.cLines > 0 )
         Amdb_SetLines( pth, newshdr.cLines );

      /* Take post storage action based on rules.
       */
      Amdb_GetCookie( ptl, NEWS_EMAIL_COOKIE, szTmpName, szMailAddress, 64 );
      if( strcmp( szTmpName, newshdr.szAuthor ) == 0 )
         newshdr.fPriority = TRUE;

      if( r == AMDBERR_NOERROR )
      {
      if( nMatch & FLT_MATCH_COPY && ( ptl != ptl2 ) )
         {
         PTH FAR * lppth;

         /* Okay, message got moved so move the message after
          * we've applied all the above.
          */
         INITIALISE_PTR(lppth);
         if( fNewMemory( (PTH FAR *)&lppth, 2 * sizeof( PTH ) ) )
            {
            lppth[ 0 ] = pth;
            lppth[ 1 ] = NULL;
            DoCopyMsgRange( hwndFrame, lppth, 1, ptl2, FALSE, FALSE, FALSE );
            }
         }
      if( ( nMatch & FLT_MATCH_PRIORITY ) || newshdr.fPriority )
         Amdb_MarkMsgPriority( pth, TRUE, TRUE );
      if( nMatch & FLT_MATCH_WATCH )
         {
         /* Only get article if DESTINATION topic is a newsgroup
          * and we've currently only got the header.
          */
         if( Amdb_GetTopicType( ptl2 ) == FTYPE_NEWS )
            Amdb_MarkMsgWatch( pth, TRUE, TRUE );
         }
      if( nMatch & FLT_MATCH_READ )
         Amdb_MarkMsgRead( pth, TRUE );
      if( nMatch & FLT_MATCH_IGNORE )
         Amdb_MarkMsgIgnore( pth, TRUE, TRUE );
      if( ( nMatch & FLT_MATCH_GETARTICLE ) || Amdb_IsWatched( pth ) )
         Amdb_MarkMsgTagged( pth, TRUE );
      if( newshdr.fXref )
         Amdb_MarkMsgCrossReferenced( pth, TRUE );
      if( nMatch & FLT_MATCH_READLOCK )
         Amdb_MarkMsgReadLock( pth, TRUE );
      if( nMatch & FLT_MATCH_KEEP )
         Amdb_MarkMsgKeep( pth, TRUE );
      if( nMatch & FLT_MATCH_MARK )
         Amdb_MarkMsg( pth, TRUE );
      Amdb_MarkHdrOnly( pth, fHeaderOnly );
      }
      ++dwHeadersRetrieved;
      }
}

/* Open a current newsgroup data file and read it into
 * memory.
 */
void FASTCALL SaveNewsgroupDataFile( LPNNTPSRVR lpcNews )
{
   HNDFILE fhNews;

   wsprintf( lpPathBuf, "%s.dat", lpcNews->nsi.szShortName );
   if( HNDFILE_ERROR != ( fhNews = Amfile_Create( lpPathBuf, 0 ) ) )
      {
      WORD wVersion;

      /* Read the version word and make sure it
       * is correct.
       */
      wVersion = GRPLIST_VERSION;
      if( Amfile_Write( fhNews, (LPSTR)&wVersion, sizeof(WORD) ) == sizeof(WORD) )
         if( Amfile_Write32( fhNews, hpNewsBuf, cbBufSize - 1 ) == cbBufSize - 1 )
//          if( Amfile_Write( fhNews, (LPSTR)&cbNewsgroups, sizeof(WORD) ) == sizeof(WORD) )
            if( Amfile_Write( fhNews, (LPSTR)&cbNewsgroups, sizeof(LONG) ) == sizeof(LONG) ) /*!!SM!!*/
               {
               Amfile_Close( fhNews );
               return;
               }

      /* Failed, so delete the file.
       */
      Amfile_Close( fhNews );
      Amfile_Delete( lpPathBuf );
      }
}

/* Open a current newsgroup data file and read it into
 * memory.
 */
void FASTCALL LoadNewsgroupDataFile( LPNNTPSRVR lpcNews )
{
   HNDFILE fhNews;

   wsprintf( lpPathBuf, "%s.dat", lpcNews->nsi.szShortName );
   if( HNDFILE_ERROR != ( fhNews = Amfile_Open( lpPathBuf, AOF_READ ) ) )
      {
      WORD wVersion;

      /* Read the version word and make sure it
       * is correct.
       */
      Amfile_Read( fhNews, (LPSTR)&wVersion, sizeof(WORD) );
      if( wVersion == GRPLIST_VERSION )
         {
         INITIALISE_PTR(hpNewsBuf);

         /* Read the file into a local buffer.
          */
//       cbBufSize = Amfile_GetFileSize( fhNews ) - ( sizeof(WORD) * 2 );
         cbBufSize = Amfile_GetFileSize( fhNews ) - 6;// !!SM!!
         if( fNewMemory32( &hpNewsBuf, cbBufSize + 1 ) )
            {
            if( Amfile_Read32( fhNews, hpNewsBuf, cbBufSize ) == cbBufSize )
               {
               HPSTR hpNewsPtr;

               /* NULL terminate the buffer
                */
               hpNewsBuf[ cbBufSize++ ] = '\0';
               cbAllocSize = cbBufSize;

               /* Finally read the count of newsgroups.
                */
//             Amfile_Read( fhNews, &cbNewsgroups, sizeof(WORD) );
               Amfile_Read( fhNews, &cbNewsgroups, sizeof(LONG) ); /*!!SM!!*/

               /* Scan the list and make the newsgroups type field
                * lower case.
                */
               hpNewsPtr = hpNewsBuf;
               while( *hpNewsPtr )
                  {
                  hpNewsPtr[ 1 ] = tolower( hpNewsPtr[ 1 ] );
                  hpNewsPtr += hstrlen( hpNewsPtr ) + 1;
                  }
               }
            else
               FreeMemory32( &hpNewsBuf );
            }
         }
      Amfile_Close( fhNews );
      }
}

/* Import IP newsgroup list from a file.
 */
BOOL EXPORT WINAPI IPListImport( HWND hwnd, LPSTR lpszScratchFile )
{
   HNDFILE fh;

   if( HNDFILE_ERROR != ( fh = Amfile_Open( "GRPS.LST", AOF_READ ) ) )
      {
      NNTPSRVR news;
      BOOL fContinuedLine;
      char sz[ 160 ];
      LPLBF hBuffer;
      UINT cbLine;

      hBuffer = Amlbf_Open( fh, AOF_READ );
      while( Amlbf_Read( hBuffer, sz, sizeof(sz), &cbLine, &fContinuedLine, &fIsAmeol2Scratchpad ) )
         if( *sz && *sz != '.' )
            InsertNewsgroup( sz, NULL, 0, FALSE );
      Amlbf_Close( hBuffer );
      strcpy( news.nsi.szShortName, "newscomp" );
      SaveNewsgroupDataFile( &news );
      }
   return( TRUE );
}

/* This function parses a newsgroup
 * line.
 */
BOOL FASTCALL InsertNewsgroup( char * pLine, LPSTR lpFilters, WORD wFilterType, BOOL fNew )
{
   HPSTR hpNewsPtr;
   BOOL fClosed;
   char * pSep;
   HPSTR hpSrc;
   HPSTR hpDest;
   int cbExtra;
   int cbLine;

   /* Skip to end of newsgroup name.
    */
   fClosed = FALSE;
   if( ( pSep = strchr( pLine, ' ' ) ) != NULL )
      {
      *pSep++ = '\0';

      /* Skip lowest article number.
       */
      if( ( pSep = strchr( pSep, ' ' ) ) != NULL )
         {
         *pSep++ = '\0';
   
         /* Skip highest article number.
          */
         if( ( pSep = strchr( pSep, ' ' ) ) != NULL )
            {
            *pSep++ = '\0';
      
            /* Now pSep should point to either the end of
             * the line of a moderator flag.
             */
            if( *pSep == 'm' || *pSep == 'n' )
               fClosed = TRUE;
            }
         }
      }

   /* Skip this if matched by a filter.
    */
   if( NULL != lpFilters )
      if( MatchFilters( lpFilters, wFilterType, pLine ) )
         return( TRUE );

   /* Default extra space needed for this new entry.
    */
   cbLine = strlen( pLine );
   cbExtra = cbLine + 4;

   /* Okay, pLine points to the newsgroup name. Scan
    * hpNewsBuf for that name and find a place into which
    * to insert the new name.
    */
   if( hpNewsPtr = hpNewsBuf )
      {
      /* Compare new newsgroup with the last one and if we
       * come later, start scanning from after that.
       */
      if( NULL != hpLastNewsPtr && _stricmp( pLine, szLastNewsgroup ) > 0 )
         hpNewsPtr = hpLastNewsPtr;
      while( *hpNewsPtr )
         {
         int r;

         if( ( r = _fstrnicmp( hpNewsPtr + 2, pLine, cbLine ) ) == 0 && ( '\t' == hpNewsPtr[ cbLine + 2 ] ) )
            {
            cbExtra -= (UINT)hstrlen( hpNewsPtr ) + 1;
            if( 0 >= cbExtra )
               return( TRUE );
            break;
            }
         if( r > 0 )
            break;
         hpNewsPtr += hstrlen( hpNewsPtr ) + 1;
         }
      }

   /* Now hpNewsPtr points to where we'll insert the new
    * string. Move it down 
    */
   if( NULL == hpNewsPtr )
      {
      cbNewsgroups = 0;
      cbAllocSize = 64000;
      cbBufSize = 1;
      if( !fNewMemory32( &hpNewsBuf, cbAllocSize ) )
         {
         ReportOnline( GS(IDS_STR1099) );
         return( FALSE );
         }
      *hpNewsBuf = '\0';
      hpNewsPtr = hpNewsBuf;
      }
   if( cbBufSize + cbExtra >= cbAllocSize )
      {
      DWORD cbBufPtr;

      cbAllocSize += 64000;
      cbBufPtr = hpNewsPtr - hpNewsBuf;
      if( !fResizeMemory32( &hpNewsBuf, cbAllocSize ) )
         {
         ReportOnline( GS(IDS_STR1099) );
         return( FALSE );
         }
      hpNewsPtr = hpNewsBuf + cbBufPtr;
      }
   hpDest = ( hpNewsBuf + cbBufSize + cbExtra ) - 1;
   hpSrc = ( hpNewsBuf + cbBufSize ) - 1;
   while( hpSrc >= hpNewsPtr )
      {
      *hpDest = *hpSrc;
      --hpDest;
      --hpSrc;
      }

   /* Save this newsgroup name.
    */
   strcpy( szLastNewsgroup, pLine );

   /* Now insert our item.
    */
   *hpNewsPtr++ = HTYP_ENTRY;
   if( fNew )
      *hpNewsPtr++ = fClosed ? 'C' : 'O';
   else
      *hpNewsPtr++ = fClosed ? 'c' : 'o';
   while( *pLine )
      *hpNewsPtr++ = *pLine++;
   *hpNewsPtr++ = '\t';
   *hpNewsPtr++ = '\0';
   hpLastNewsPtr = hpNewsPtr;
   cbBufSize += cbExtra;

   /* Bump count of newsgroups read.
    */
   ++cbNewsgroups;
   return( TRUE );
}

/* This function parses a newsgroup descriptions
 * line.
 */
BOOL FASTCALL InsertNewsgroupDescription( char * pLine, char * pDescription )
{
   HPSTR hpDescPtr;
   BOOL fClosed;
   char * pSep;
   HPSTR hpSrc;
   HPSTR hpDest;
   int cbExtra;
   int cbLine;

   /* Skip to end of newsgroup name.
    */
   fClosed = FALSE;
   if( ( pSep = strchr( pLine, ' ' ) ) != NULL )
      {
      *pSep++ = '\0';

      /* Skip lowest article number.
       */
      if( ( pSep = strchr( pSep, ' ' ) ) != NULL )
         {
         *pSep++ = '\0';
   
         /* Skip highest article number.
          */
         if( ( pSep = strchr( pSep, ' ' ) ) != NULL )
            {
            *pSep++ = '\0';
      
            /* Now pSep should point to either the end of
             * the line of a moderator flag.
             */
            if( *pSep == 'm' || *pSep == 'n' )
               fClosed = TRUE;
            }
         }
      }

   /* Default extra space needed for this new entry.
    */
   cbLine = strlen( pLine );
   cbExtra = cbLine + strlen( pDescription ) + 2;

   /* Okay, pLine points to the newsgroup name. Scan
    * hpNewsBuf for that name and find a place into which
    * to insert the new name.
    */
   if( hpDescPtr = hpDescBuf )
      {
      /* Compare new newsgroup with the last one and if we
       * come later, start scanning from after that.
       */
      ASSERT( NULL != hpLastNewsPtr );
      if( _stricmp( pLine, szLastNewsgroup ) > 0 )
         hpDescPtr = hpLastNewsPtr;
      while( *hpDescPtr )
         {
         int r;

         if( ( r = _fstrnicmp( hpDescPtr, pLine, cbLine ) ) == 0 )
            return( TRUE );
         if( r > 0 )
            break;
         hpDescPtr += hstrlen( hpDescPtr ) + 1;
         }
      }

   /* Now hpDescPtr points to where we'll insert the new
    * string. Move it down 
    */
   if( NULL == hpDescPtr )
      {
      cDescriptions = 0;
      cbDescAllocSize = 64000;
      cbDescBufSize = 1;
      if( !fNewMemory32( &hpDescBuf, cbDescAllocSize ) )
         return( FALSE );
      *hpDescBuf = '\0';
      hpDescPtr = hpDescBuf;
      }
   if( cbDescBufSize + cbExtra >= cbDescAllocSize )
      {
      DWORD cbBufPtr;

      cbDescAllocSize += 64000;
      cbBufPtr = hpDescPtr - hpDescBuf;
      if( !fResizeMemory32( &hpDescBuf, cbDescAllocSize ) )
         return( FALSE );
      hpDescPtr = hpDescBuf + cbBufPtr;
      }
   hpDest = hpDescBuf + cbDescBufSize + cbExtra;
   hpSrc = hpDescBuf + cbDescBufSize;
   hmemmove( hpDescPtr + cbExtra, hpDescPtr, hpSrc - hpDescPtr );

   /* Save this newsgroup name.
    */
   strcpy( szLastNewsgroup, pLine );

   /* Now insert our item.
    */
   while( *pLine )
      *hpDescPtr++ = *pLine++;
   *hpDescPtr++ = '\t';
   while( *pDescription )
      *hpDescPtr++ = *pDescription++;
   *hpDescPtr++ = '\0';
   hpLastNewsPtr = hpDescPtr;
   cbDescBufSize += cbExtra;

   /* Bump count of descriptions read.
    */
   ++cDescriptions;
   return( TRUE );
}

/* This function merges the newsgroups descriptions with the
 * full list of newsgroups.
 */
void FASTCALL MergeDescriptions( void )
{
   HPSTR hpMergedBuf;
   DWORD cbMergedList;

   INITIALISE_PTR(hpMergedBuf);

   cbMergedList = cbDescBufSize + cbBufSize;
   if( cbMergedList > cbBufSize )
      if( fNewMemory32( &hpMergedBuf, cbMergedList ) )
         {
         HPSTR hpMergedPtr;
         HPSTR hpDescPtr;
         HPSTR hpNewsPtr;

         /* Scan the full list and copy each item to the merged
          * buffer. Also check for each item's description in
          * hpDescBuf and copy that too.
          */
         hpDescPtr = hpDescBuf;
         hpNewsPtr = hpNewsBuf;
         hpMergedPtr = hpMergedBuf;
         while( *hpNewsPtr )
            {
            HPSTR hpThisDescPtr;
            HPSTR hpDescPtr2;
            HPSTR hpNewsPtr2;

            /* Copy HTYP_ENTRY tag and closed/open tag
             */
            *hpMergedPtr++ = *hpNewsPtr++;   
            *hpMergedPtr++ = *hpNewsPtr++;

            /* Locate the newsgroup in the description.
             */
            hpThisDescPtr = NULL;
            hpNewsPtr2 = hpNewsPtr;
            hpDescPtr2 = hpDescPtr;
            while( *hpDescPtr )
               {
               int diff;

               while( *hpDescPtr2 != '\t' && *hpDescPtr2 == *hpNewsPtr2 )
                  {
                  ++hpDescPtr2;
                  ++hpNewsPtr2;
                  }
               diff = *hpDescPtr2 - *hpNewsPtr2;
               if( diff == 0 )
                  {
                  hpThisDescPtr = ++hpDescPtr2;
                  break;
                  }
               if( diff > 0 )
                  break;
               while( *hpDescPtr2++ != '\0' );
               hpDescPtr = hpDescPtr2;
               hpNewsPtr2 = hpNewsPtr;
               }

            /* Copy the newsgroup name.
             */
            while( *hpNewsPtr != '\t' )
               *hpMergedPtr++ = *hpNewsPtr++;
            *hpMergedPtr++ = '\t';
            ++hpNewsPtr;

            /* Copy the description, if we have one.
             */
            if( hpThisDescPtr )
               while( *hpThisDescPtr )
                  *hpMergedPtr++ = *hpThisDescPtr++;
            *hpMergedPtr++ = '\0';
            ++hpNewsPtr;
            }
         *hpMergedPtr++ = '\0';

         /* All done, so delete the original buffers
          * and make the merged buffer the active one.
          */
         FreeMemory32( &hpDescBuf );
         FreeMemory32( &hpNewsBuf );
         hpNewsBuf = hpMergedBuf;
         cbBufSize = hpMergedPtr - hpMergedBuf;
         }
}

/* This function is called after we have retrieved one header, or we get
 * an error while attempting selecting a newsgroup while attemping to
 * retrieve a header.
 */
void FASTCALL EndGetHeaders( LPNNTPSRVR lpcNews )
{
   Amuser_CallRegistered( AE_UNREADCHANGED, (LPARAM)newshdr.ptl, 0L );
   OnlineStatusText( GS(IDS_STR697), Amdb_GetTopicName( newshdr.ptl ) );
   HideBlinkGauge();
   Amdb_SetTopicFlags( newshdr.ptl, TF_READ50, FALSE );

   /* Report to the blink log if appropriate.
    */
   if( dwHeadersRetrieved > 0 )
      {
      wsprintf( lpTmpBuf, GS(IDS_STR1084), dwHeadersRetrieved, Amdb_GetTopicName( newshdr.ptl ) );
      WriteToBlinkLog( lpTmpBuf );
      dwHeadersRetrieved = 0;
      }

   /* Now find the next news server for which we want
    * headers.
    */
   while( cTopicIdx < nfl.cTopics )
      {
      LPNNTPSRVR lpcNewsNext;
      char sz[ 100 ];

      SetNewsIdle( lpcNews );
      Amdb_MarkTopicInUse( newshdr.ptl, FALSE );
      Amdb_UnlockTopic( newshdr.ptl );
      newshdr.ptl = nfl.ptlTopicList[ cTopicIdx++ ];
      Amdb_LockTopic( newshdr.ptl );
      Amdb_MarkTopicInUse( newshdr.ptl, TRUE );
      lpcNewsNext = EnsureNewsgroupServer( newshdr.ptl );
      if( NULL != lpcNewsNext )
         {
         wsprintf( sz, "group %s\r\n", Amdb_GetTopicName( newshdr.ptl ) );
         WriteNewsSocketLine( lpcNewsNext, sz, NWS_READNEWSGROUPSIZE );
         return;
         }
      }

   /* All done, so disconnect from NNTP processing.
    */   
   if( NULL != nfl.ptlTopicList )
      FreeMemory( (LPVOID)&nfl.ptlTopicList );
   nfl.cTopics = 0;
   OnlineStatusText( GS(IDS_STR696) );
   if( newshdr.ptl )
      {
      Amdb_MarkTopicInUse( newshdr.ptl, FALSE );
      Amdb_UnlockTopic( newshdr.ptl );
      }
   SetNewsIdle( lpcNews );
   EndProcessing( 0xFFFF );
}

/* This function switches the news client into idle mode.
 */
void FASTCALL SetNewsIdle( LPNNTPSRVR lpcNews )
{
   if( NULL != hpNewsBuf )
      FreeMemory32( &hpNewsBuf );
   if( NULL != hpDescBuf )
      FreeMemory32( &hpDescBuf );
   lpcNews->nNewsState = NWS_IDLE;
   Amdb_CommitDatabase( FALSE );
   ResetDisconnectTimeout();
}

/* This function appends the specified line to the memory buffer.
 */
void FASTCALL AppendToMessage( char * pstr, BOOL fNewline )
{
   /* Cast to long to avoid wrap round because MAX_MSGSIZE is close to 0xFFFF
    */
   if( hpBuf == NULL )
      {
      dwBufSize = 64000;
      fNewMemory32( &hpBuf, dwBufSize );
      }
   else if( dwMsgLength + (DWORD)strlen( pstr ) + 2L >= dwBufSize )
      {
      dwBufSize += 64000;
      fResizeMemory32( &hpBuf, dwBufSize );
      }
   if( hpBuf )
      {
      if( *pstr == '.' )
         ++pstr;
      hmemcpy( hpBuf + dwMsgLength, pstr, strlen( pstr ) );
      dwMsgLength += strlen( pstr );
      if( fNewline )
         {
         hpBuf[ dwMsgLength++ ] = CR;
         hpBuf[ dwMsgLength++ ] = LF;
         }
      hpBuf[ dwMsgLength ] = '\0';
      }
}

/* This function appends the specified line to the memory buffer.
 */
static BOOL FASTCALL AppendToHeader( char * pstr )
{
   /* Cast to long to avoid wrap round because MAX_MSGSIZE is close to 0xFFFF
    */
   if( hpHdrText == NULL )
      {
      dwBufSize = 64000;
      dwHdrLength = 0;
      fNewMemory32( &hpHdrText, dwBufSize );
      }
   else if( dwHdrLength + (DWORD)strlen( pstr ) + 2L >= dwBufSize )
      {
      dwBufSize += 64000;
      fResizeMemory32( &hpHdrText, dwBufSize );
      }
   if( hpHdrText )
      {
      hmemcpy( hpHdrText + dwHdrLength, pstr, strlen( pstr ) );
      dwHdrLength += strlen( pstr );
      hpHdrText[ dwHdrLength++ ] = CR;
      hpHdrText[ dwHdrLength++ ] = LF;
      hpHdrText[ dwHdrLength ] = '\0';
      return( TRUE );
      }
   return( FALSE );
}

/* This function appends a huge memory block onto the end of the
 * one we currently have.
 */
static BOOL FASTCALL AppendMem32ToHeader( HPSTR hpstr, DWORD dwSize )
{
   if( hpHdrText == NULL )
      {
      dwBufSize = dwSize + 1;
      dwHdrLength = 0;
      fNewMemory32( &hpHdrText, dwBufSize );
      }
   else if( dwHdrLength + dwSize >= dwBufSize )
      {
      dwBufSize += dwSize;
      fResizeMemory32( &hpHdrText, dwBufSize );
      }
   if( hpHdrText )
      {
      hmemcpy( hpHdrText + dwHdrLength, hpstr, dwSize );
      dwHdrLength += dwSize;
      hpHdrText[ dwHdrLength ] = '\0';
      return( TRUE );
      }
   return( FALSE );
}

/* This function extracts a string from the source to the
 * destination, stopping at the first delimiter.
 */
LPSTR FASTCALL ExtractString( LPSTR lpSrc, LPSTR lpDest, int cbDest )
{
   while( *lpSrc && *lpSrc != ' ' && *lpSrc != ',' )
      {
      if( cbDest-- )
         *lpDest++ = *lpSrc;
      ++lpSrc;
      }
   *lpDest = '\0';
   while( *lpSrc == ' ' || *lpSrc == ',' )
      ++lpSrc;
   return( lpSrc );
}
