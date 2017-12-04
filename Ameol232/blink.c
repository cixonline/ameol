/* BLINK.C - Implements the blink stuff
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
#include <ctype.h>
#include "intcomms.h"
#include "cix.h"
#include "cixip.h"
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "ameol2.h"
#include "transfer.h"
#include "blinkman.h"
#include "cookie.h"
#include "news.h"
#include "trace.h"
#include "log.h"
#include "demo.h"
#include "admin.h"
#include "rules.h"

#include <limits.h> // 2.55.2034

#define  THIS_FILE   __FILE__

#define  BLINK_MILLISECONDS            220
#define  NUM_ICONS                     8

/* Old (Ameol1) message script options.
 */
#define  GS_RECOVER           1
#define  GS_STAYONLINE        2
#define  GS_PROCESSMAIL       4
#define  GS_PROCESSUSENET     8
#define  GS_PROCESSCIX        16
#define  GS_DOWNLOADMAIL      32
#define  GS_DOWNLOADUSENET    64
#define  GS_DOWNLOADCIX       128
#define  GS_FORCEUSENET       256

#define SetAppIcon(w,h)    SetClassLong((w),GCL_HICON,(LONG)(h))

static BOOL fDefDlgEx = FALSE;                  /* DefDlg recursion flag trap */
static int nBlinkAnimationStep = 0;             /* Blink animation step */
static BOOL fStartedBlinkAnimation = FALSE;     /* TRUE if the bunny rabbit is hopping */
static LPOB lpobCurrent = NULL;                 /* Handle of current processed item */
static DWORD dwConnectOn;                       /* Time when blink started */
static DWORD dwConnectThisService;              /* Time when service entered started */
static BOOL fScreenSaveActive = FALSE;          /* TRUE if screen saver was active */
static int wOldService = 0;                     /* Current service being processed */
static BOOL fEntry = FALSE;                     /* Re-entry flag */
static BOOL fNeedSocket = FALSE;                /* TRUE if we loaded WINSOCK.DLL before blinking */
static BOOL fRecoverNow = FALSE;                /* TRUE if we force a scratchpad recovery */
static AM_DATE curDate;                         /* Date when we logged onto CIX */
static AM_TIME curTime;                         /* Time when we logged onto CIX */
static BOOL fLoggedOn;                          /* TRUE if logon was successful */
static BOOL fConnectLogOpen;                          /* TRUE if the logfile is open */
int wCommErr;                                   /* Communications error code */
BOOL fInitiatingBlink = FALSE;                  /* TRUE if we're starting a blink */
BOOL fCIXSyncInProgress = FALSE;                /* TRUE if a synchronous CIX blink in progress */
HWND hwndBlink = NULL;                          /* Blink window */
HWND hwndCixTerminal = NULL;                    /* CIX terminal */
BOOL fDoBlinkAnimation = TRUE;                  /* TRUE if we do blink animation */
BLINKENTRY beCur;                               /* Current blink entry */
int iAddonsUsingConnection;

LPCOMMDEVICE lpcdevCIX = NULL;                  /* Handle of CIX comm device */

static HICON FAR * pBlinkIcons = NULL;          /* Array of blink icons */
static int cBlinkIcons = 0;                     /* Number of icons in array */

BOOL FASTCALL CreateBlinkWindow( void );
void FASTCALL DestroyBlinkWindow( void );
BOOL FASTCALL DisableScreenSaver( void );
BOOL FASTCALL EnableScreenSaver( void );
void FASTCALL EndOnlineBlink( void );
void FASTCALL GetServiceName( int, char *, int );
BOOL FASTCALL LoadBlinkIcons( void );
void FASTCALL UnloadBlinkIcons( void );
void FASTCALL ShowOutBasketTotal( HWND );
int FASTCALL CountNewsgroupsInDatabase( DWORD );

BOOL EXPORT CALLBACK BlinkStatus( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL BlinkStatus_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL BlinkStatus_OnCommand( HWND, int, HWND, UINT );
void FASTCALL BlinkStatus_OnMove( HWND, int, int );
LRESULT FASTCALL BlinkStatus_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL FASTCALL AmCheckForNewVersion( void ); /*!!SM!!*/

/* This function returns a flag which indicates whether or not the
 * last scratchpad download aborted.
 */
BOOL FASTCALL MustRecoverScratchpad()
{
   return( Amuser_GetPPInt( szSettings, "RecoverScratchpad", FALSE ) );
}

/* Check whether we have room in the default mail folder to store any
 * further messages.
 */
BOOL FASTCALL CanGetMail()
{
   PTL mailFolder = GetPostmasterFolder();
   if (Amdb_GetTopicTextFileSize( mailFolder ) >= Amdb_GetTextFileThreshold())
   {
      fMessageBox( hwndFrame, 0, GS(IDS_STR1264), MB_OK|MB_ICONEXCLAMATION );
      return FALSE;
   }
   return TRUE;
}

/* This function starts a blink. We set all outbasket entries to be
 * pending, then we call the actual blink code.
 */
BOOL FASTCALL BeginBlink( LPBLINKENTRY lpbe )
{
   WORD wConnectFlags;
   LPRASDATA lprd;
   int cNewsgroups;
   LPOB lpob;
   char szName[ sizeof( szAppName ) ];
   HWND hwndOldFocus = NULL;
   BOOL fCanGetMail;

   fNewMailPlayed = FALSE;
   fInitiatingBlink = TRUE;

   /* Must not already be blinking!
    */
   ASSERT( !fBlinking );

   /* Must not be parsing a scratchpad.
    */
   if( fParsing )
      {
      WriteToBlinkLog( GS(IDS_STR1170) );
      return( FALSE );
      }

   /* We are just starting a blink to ensure that we are not
    * flagged to end a blink (resulting from a user stop on a
    * previous blink).
    * YH 10/05/96
    */
   fEndBlinking = FALSE;

   /* Make this the current blink entry.
    */
   beCur = *lpbe;

   /* If not using CIX, mask out CIX flags
    * Ditto for internet.
    */
   if( !fUseCIX )
      beCur.dwBlinkFlags &= ~BF_CIXFLAGS;
   if( !fUseInternet )
      beCur.dwBlinkFlags &= ~BF_IPFLAGS;

   /* Count number of newsgroups.
    */
   cNewsgroups = CountNewsgroupsInDatabase( beCur.dwBlinkFlags );

   /* Commit the databases before we go any further.
    */
   Amdb_CommitDatabase( FALSE );

   /* Update the object IDs.
    */
   Amob_ResyncObjectIDs();

   if( fUseCIX )
   {
   /* Deal with initial CIX stuff.
    */
   if( beCur.dwBlinkFlags & BF_CIXFLAGS )
      {
      BOOL fScratchRecover;

      /* Check that we aren't within 30 minutes of a downtime.
       */
      if( !CheckForDowntime( hwndFrame ) )
         return( FALSE );

      /* Deal with any unparsed scratchpads first.
       */
      HandleUnparsedScratchpads( hwndFrame, TRUE );

      /* Handle possible scratchpad recovery.
       */
      fScratchRecover = MustRecoverScratchpad();
      if( fScratchRecover )
         {
         int r;

         if( fRecoverNow )
            fRecoverNow = FALSE;
         else if( beCur.dwBlinkFlags & BF_CIXRECOVER )
            {
            /* If we've already asked to recover the scratchpad, clear the
             * flag as no prompting is needed.
             */
            fScratchRecover = FALSE;
            fRecoverNow = FALSE;
            Amuser_WritePPInt( szSettings, "RecoverScratchpad", fScratchRecover );
            }
         else if( !fQuitAfterConnect )
            {
               EnsureMainWindowOpen();
               if( ( r = fDlgMessageBox( hwndFrame, idsQUERYRECOVER, IDDLG_QUERYRECOVER, NULL, 30000, IDYES ) ) == IDNO )
               {
               fScratchRecover = FALSE;
               Amuser_WritePPInt( szSettings, "RecoverScratchpad", fScratchRecover );
               }
            else if( r == IDCANCEL )
               return( FALSE );
            }
         }
      if( fScratchRecover )
         beCur.dwBlinkFlags |= BF_CIXRECOVER;
      }
   }

   /* Send AE_CONNECTSTART to all addons. Synthesize the old style
    * flags for the benefit of old addons.
    */
   wConnectFlags = 0;
   if( beCur.dwBlinkFlags & BF_STAYONLINE )
      wConnectFlags |= GS_STAYONLINE;
   if( beCur.dwBlinkFlags & BF_CIXRECOVER )
      wConnectFlags |= GS_RECOVER;
   if( beCur.dwBlinkFlags & BF_POSTCIXMAIL )
      wConnectFlags |= GS_PROCESSMAIL;
   if( beCur.dwBlinkFlags & BF_POSTCIXNEWS )
      wConnectFlags |= GS_PROCESSUSENET;
   if( beCur.dwBlinkFlags & BF_POSTCIXMSGS )
      wConnectFlags |= GS_PROCESSCIX;
   if( beCur.dwBlinkFlags & BF_GETCIXMAIL )
      wConnectFlags |= GS_DOWNLOADMAIL;
   if( beCur.dwBlinkFlags & BF_GETCIXNEWS )
      wConnectFlags |= GS_DOWNLOADUSENET;
   if( beCur.dwBlinkFlags & BF_GETCIXMSGS )
      wConnectFlags |= GS_DOWNLOADCIX;
   Amuser_CallRegistered( AE_CONNECTSTART, beCur.dwBlinkFlags, MAKELPARAM( wConnectFlags, 0 ) );
   
   /* If we're getting new news articles, add a Get New Articles
    * command to the out-basket now.
    */
   if( beCur.dwBlinkFlags & (BF_GETIPNEWS|BF_GETCIXNEWS))
      {
      NEWARTICLESOBJECT nh;
      LPOB lpob;

      Amob_New( OBTYPE_GETNEWARTICLES, &nh );
      nh.wService = 0;
      if( beCur.dwBlinkFlags & BF_GETIPNEWS )
         nh.wService |= W_SERVICE_IP;
      if( beCur.dwBlinkFlags & BF_GETCIXNEWS )
         nh.wService |= W_SERVICE_CIX;
      Amob_CreateRefString( &nh.recFolder, "(All)" );
      lpob = Amob_Find( OBTYPE_GETNEWARTICLES, &nh );
      if( cNewsgroups )
         Amob_Commit( lpob, OBTYPE_GETNEWARTICLES, &nh );
      else
         if( NULL != lpob )
            RemoveObject( lpob );
      Amob_Delete( OBTYPE_GETNEWARTICLES, &nh );
      }

   /* If we're getting tagged news articles, add a Get Tagged Articles
    * command to the out-basket now.
    */
   if( beCur.dwBlinkFlags & (BF_GETIPTAGGEDNEWS|BF_GETCIXTAGGEDNEWS) )
      {
      GETTAGGEDOBJECT to;
      LPOB lpob;

      Amob_New( OBTYPE_GETTAGGED, &to );
      to.wService = 0;
      if( beCur.dwBlinkFlags & BF_GETIPNEWS )
         to.wService |= W_SERVICE_IP;
      if( beCur.dwBlinkFlags & BF_GETCIXNEWS )
         to.wService |= W_SERVICE_CIX;
      Amob_CreateRefString( &to.recFolder, "(All)" );
      lpob = Amob_Find( OBTYPE_GETTAGGED, &to );
      if( cNewsgroups )
         Amob_Commit( lpob, OBTYPE_GETTAGGED, &to );
      else
         if( NULL != lpob )
            RemoveObject( lpob );
      Amob_Delete( OBTYPE_GETTAGGED, &to );
      }

   fCanGetMail = CanGetMail();
   if( !fPOP3Last && fCanGetMail )
   {
      /* If we're collecting new mail, add a Get New Mail command
       * to the out-basket now.
       */
      if( beCur.dwBlinkFlags & BF_GETIPMAIL )
         if( !Amob_Find( OBTYPE_GETNEWMAIL, NULL ) )
            {
            Amob_New( OBTYPE_GETNEWMAIL, NULL );
            Amob_Commit( NULL, OBTYPE_GETNEWMAIL, NULL );
            Amob_Delete( OBTYPE_GETNEWMAIL, NULL );
            }
   }

   /* If any CIX stuff, create a script.
    */
   if( beCur.dwBlinkFlags & BF_CIXFLAGS )
      {
      /* Create the script in NEWMES.SCR.
       */
      if( !GenerateScript( beCur.dwBlinkFlags ) )
         beCur.dwBlinkFlags &= ~BF_CIXFLAGS;
      else
         {
         /* Add a command to the out-basket to run the NEWMES.SCR
          * script.
          */
         if( !Amob_Find( OBTYPE_EXECCIXSCRIPT, NULL ) )
            {
            Amob_New( OBTYPE_EXECCIXSCRIPT, NULL );
            Amob_Commit( NULL, OBTYPE_EXECCIXSCRIPT, NULL );
            Amob_Delete( OBTYPE_EXECCIXSCRIPT, NULL );
            }
         }
      }

   if( fPOP3Last && fCanGetMail )
   {
      /* If we're collecting new mail, add a Get New Mail command
       * to the out-basket now.
       */
      if( beCur.dwBlinkFlags & BF_GETIPMAIL )
         if( !Amob_Find( OBTYPE_GETNEWMAIL, NULL ) )
            {
            Amob_New( OBTYPE_GETNEWMAIL, NULL );
            Amob_Commit( NULL, OBTYPE_GETNEWMAIL, NULL );
            Amob_Delete( OBTYPE_GETNEWMAIL, NULL );
            }
   }

   /* We're blinking...
    */
   fBlinking = TRUE;

   /* First set the toolbar.
    */
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_ONLINE, FALSE );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_STOP, TRUE );
   EnableBlinkButtons( FALSE );
   if( !fOnline )
      Amuser_CallRegistered( AE_CONTEXTSWITCH, AECSW_ONLINE, TRUE );

   /* Show the Blink dialog window if Ameol is the
    * active application.
    */
   wOldService = 0;
   if( !IsIconic( hwndFrame ) )
      {
      GetWindowText( GetParent( GetFocus() ), szName, sizeof( szName ) );
      if( strcmp( szName, szAppName ) != 0 )
         hwndOldFocus = GetFocus();
      else
         hwndOldFocus = NULL;
      }
   CreateBlinkWindow();
   if( NULL != hwndBlink )
   {
      ShowWindow( hwndBlink, SW_SHOWNA );
      if( NULL != hwndOldFocus )
      {
      ShowWindow( hwndOldFocus, SW_SHOW );
      SetFocus( hwndOldFocus );
      }
   }

   /* More hacks to compensate for lack of
    * RAS in Win16
    */
   lprd = &beCur.rd;

   /* Make sure we can load WINSOCK.DLL
    */
   DisableScreenSaver();
   StartBlinkAnimation();

   /* Open the blink log.
    */
   OpenBlinkLog( beCur.szName );

   wsprintf( lpTmpBuf, "Blink '%s' using '%s' (", beCur.szName, beCur.szConnCard ); // FS#143

   if (beCur.dwBlinkFlags & BF_GETIPMAIL)
      strcat(lpTmpBuf, "Get Internet Mail Messages, ");
   if (beCur.dwBlinkFlags & BF_POSTIPMAIL)
      strcat(lpTmpBuf, "Post Internet Mail Messages, ");
   if (beCur.dwBlinkFlags & BF_GETIPNEWS)
      strcat(lpTmpBuf, "Get Usenet News Messages, ");
   if (beCur.dwBlinkFlags & BF_POSTIPNEWS)
      strcat(lpTmpBuf, "Post Usenet News Messages, ");
   if (beCur.dwBlinkFlags & BF_GETIPTAGGEDNEWS)
      strcat(lpTmpBuf, "Get Tagged IP News Messages, ");
   if (beCur.dwBlinkFlags & BF_POSTCIXMAIL)
      strcat(lpTmpBuf, "Post CIX Mail Messages, ");
   if (beCur.dwBlinkFlags & BF_GETCIXMAIL)
      strcat(lpTmpBuf, "Get CIX Mail Messages, ");
   if (beCur.dwBlinkFlags & BF_POSTCIXMSGS)
      strcat(lpTmpBuf, "Post CIX Messages, ");
   if (beCur.dwBlinkFlags & BF_GETCIXMSGS)
      strcat(lpTmpBuf, "Get CIX Messages, ");
   if (beCur.dwBlinkFlags & BF_POSTCIXNEWS)
      strcat(lpTmpBuf, "Post CIX News Messages, ");
   if (beCur.dwBlinkFlags & BF_GETCIXNEWS)
      strcat(lpTmpBuf, "Get CIX News Messages, ");
   if (beCur.dwBlinkFlags & BF_GETCIXTAGGEDNEWS)
      strcat(lpTmpBuf, "Get CIX Tagged News Messages, ");
   if (beCur.dwBlinkFlags & BF_CIXRECOVER)
      strcat(lpTmpBuf, "Recover Scratchpad, ");
   if (beCur.dwBlinkFlags & BF_STAYONLINE)
      strcat(lpTmpBuf, "Stay Online, ");

   lpTmpBuf[strlen(lpTmpBuf) - 2] = '\x0';
   strcat(lpTmpBuf, ")");
   WriteToBlinkLog( lpTmpBuf );   // FS#143

   /* If we use RAS, then initiate the RAS
    * connection.
    */
   fNeedSocket = ( beCur.dwBlinkFlags & BF_IPFLAGS ) || beCur.rd.fUseRAS;
   if( fNeedSocket )
      if( Pal_IncrementSocketCount( lprd ) == 0 )
         {
         EndBlink();
         DestroyBlinkWindow();
         return( FALSE );
         }

   /* Now loop and set all out-basket items pending. If any are
    * set active (from a failed blink), clear the flags.
    */
   for( lpob = Amob_GetOb( NULL ); lpob; lpob = Amob_GetOb( lpob ) )
      {
      OBINFO obinfo;

      Amob_GetObInfo( lpob, &obinfo );
      if( obinfo.obHdr.wFlags & (OBF_ACTIVE|OBF_SCRIPT|OBF_ERROR) )
         {
         obinfo.obHdr.wFlags &= ~(OBF_ACTIVE|OBF_SCRIPT|OBF_ERROR);
         UpdateOutbasketItem( lpob, TRUE );
         }
      if( !( obinfo.obHdr.wFlags & (OBF_OPEN|OBF_HOLD) ) )
         {
         if( Amob_GetObjectType( lpob ) & beCur.dwBlinkFlags )
            obinfo.obHdr.wFlags |= OBF_PENDING;
         }
      Amob_SetObInfo( lpob, &obinfo );
      }

   UpdateOutBasketStatus();

   /* Initialise globals, etc.
    */
   lpobCurrent = NULL;
   fInitiatingBlink = FALSE;
   return( Blink( 0xFFFF ) );
}

/* This function counts how many topics of type FTYPE_NEWS are
 * in the database.
 */
int FASTCALL CountNewsgroupsInDatabase( DWORD dwBlinkFlags )
{
   BOOL fNewsViaCIXIP;
   BOOL fNewsViaCIX;
   int cNewsgroups;
   PCAT pcat;
   PCL pcl;
   PTL ptl;

   fNewsViaCIXIP = ( dwBlinkFlags & BF_IPUSENET ) != 0;
   fNewsViaCIX = ( dwBlinkFlags & BF_CIXUSENET ) != 0;
   cNewsgroups = 0;
   for( pcat = Amdb_GetFirstCategory(); pcat; pcat = Amdb_GetNextCategory( pcat ) )
      for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
         for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
            if( Amdb_GetTopicType( ptl ) == FTYPE_NEWS && !Amdb_IsResignedTopic( ptl ) )
               {
               char szSubNewsServer[ 64 ];

               Amdb_GetCookie( ptl, NEWS_SERVER_COOKIE, szSubNewsServer, "", 64 );
               if( strcmp( szSubNewsServer, szCixnews ) == 0 )
                  {
                  if( fNewsViaCIX )
                     ++cNewsgroups;
                  }
               else
                  {
                  if( fNewsViaCIXIP )
                     ++cNewsgroups;
                  }
               }
   return( cNewsgroups );
}

/* This function is called if the current exec failed for
 * some reason.
 */
void FASTCALL FailedExec( void )
{
   if( lpobCurrent )
      {
      OBINFO obinfo;

      Amob_GetObInfo( lpobCurrent, &obinfo );
      obinfo.obHdr.wFlags &= ~(OBF_PENDING|OBF_ACTIVE|OBF_SCRIPT);
      obinfo.obHdr.wFlags |= OBF_ERROR;
      Amob_SetObInfo( lpobCurrent, &obinfo );
      UpdateOutbasketItem( lpobCurrent, TRUE );
      lpobCurrent = NULL;
      }
}

/* This function ends a blink. We set all outbasket entries to be
 * pending, then we call the actual blink code.
 */
BOOL FASTCALL EndBlink( void )
{
   if( CloseOnlineWindows( hwndFrame, FALSE ) )
      {
      EndOnlineBlink();
      DestroyBlinkWindow();
      }
   return( TRUE );
}

/* This function cleans up at the end of a blink.
 */
void FASTCALL EndOnlineBlink( void )
{
   LPOB lpob;

   /* Out of all services.
    */
   SwitchService( 0 );

   /* Clean up the current item.
    */
   if( lpobCurrent )
      {
      OBINFO obinfo;

      Amob_GetObInfo( lpobCurrent, &obinfo );
      obinfo.obHdr.wFlags &= ~(OBF_PENDING|OBF_ACTIVE|OBF_SCRIPT);
      Amob_SetObInfo( lpobCurrent, &obinfo );
      UpdateOutbasketItem( lpobCurrent, TRUE );
      lpobCurrent = NULL;
      }

   /* Now loop and remove the active flag from all items.
    */
   for( lpob = Amob_GetOb( NULL ); lpob; lpob = Amob_GetOb( lpob ) )
      {
      OBINFO obinfo;

      Amob_GetObInfo( lpob, &obinfo );
      if( obinfo.obHdr.wFlags & (OBF_ACTIVE|OBF_SCRIPT|OBF_PENDING) )
         {
         obinfo.obHdr.wFlags &= ~(OBF_ACTIVE|OBF_SCRIPT|OBF_PENDING);
         UpdateOutbasketItem( lpob, TRUE );
         Amob_SetObInfo( lpob, &obinfo );
         UpdateOutbasketItem( lpob, TRUE );
         }
      }
   fInitiatingBlink = FALSE;
   UpdateOutBasketStatus();
   
   /* If we're blinking, end the blink.
    */
   if( fBlinking )
      {
      fBlinking = FALSE;
      EndBlinkAnimation();
      if( fScreenSaveActive )
         EnableScreenSaver();
      if( fNeedSocket && Pal_GetOpenSocketCount() )
         Pal_DecrementSocketCount();
      if( fDebugMode )
      {                  
         wsprintf( lpTmpBuf, "Open Socket Count: %d", Pal_GetOpenSocketCount() );         
         WriteToBlinkLog( lpTmpBuf );
      }
      DestroyBlinkWindow();
      }

   if( !fOnline )
      Amuser_CallRegistered( AE_CONTEXTSWITCH, AECSW_ONLINE, FALSE );

   Amuser_CallRegistered( AE_CONNECTEND, 0, 0 );

   /* Close the blink log.
    */
   CloseBlinkLog();

   /* Close the connect log
    */
   if( fOpenTerminalLog && fConnectLogOpen )
      {
      HNDFILE fh;
      PCL pclLog;
      const char NEAR szLogFolder[] = "Logs";

      Amuser_GetActiveUserDirectory( lpTmpBuf, _MAX_PATH );
      strcat( strcat( lpTmpBuf, "\\" ), szConnectLog );
      Ameol2_Archive( lpTmpBuf, FALSE );
      fConnectLogOpen = FALSE;
      if( ( fh = Amfile_Open( lpTmpBuf, AOF_READ ) ) != HNDFILE_ERROR )
      {
      LONG wSize;                      //!!SM!! 2042
      HPSTR lpBuf;

      INITIALISE_PTR(lpBuf);

      wSize = /*(WORD)*/Amfile_GetFileSize( fh ); //!!SM!! 2042
        if( fNewMemory32( &lpBuf, wSize + 1 ) )
         {
         /* Read the note, make sure the text is in PC format
          * and skip the spurious date at the start.
          */
         Amfile_Read32( fh, lpBuf, wSize ); //!!SM!! 2042
         lpBuf[ wSize ] = '\0';
         StripBinaryCharacter32( lpBuf, wSize, '\0' );   //!!SM!! 2042
         lpBuf = ExpandUnixTextToPCFormat( lpBuf );
      if( NULL == ( pclLog = Amdb_OpenFolder( NULL, (char *)szLogFolder ) ) )
         pclLog = Amdb_CreateFolder( Amdb_GetFirstCategory(), (char *)szLogFolder, CFF_SORT|CFF_KEEPATTOP );
      if( NULL != pclLog )
         {
         PTL ptlLog;

         /* Open or create the log topic.
          */
         if( NULL == ( ptlLog = Amdb_OpenTopic( pclLog, (char *)"Connects" ) ) )
         {
            ptlLog = Amdb_CreateTopic( pclLog, (char *)"Connects", FTYPE_LOCAL );
            if( NULL != ptlLog )
               Amdb_SetTopicFlags( ptlLog, TF_IGNORED, TRUE );
         }
         if( NULL != ptlLog )
            {
            TOPICINFO topicinfo;
            char szTitle[ 200 ];
            PTH pth;
            static AM_DATE StartDate;                    /* Date when blink started */
            static AM_TIME StartTime;                    /* Time when blink started */
            HPSTR lpszLogText2;

            /* Record the start of logging.
             */
            Amdate_GetCurrentDate( &StartDate );
            Amdate_GetCurrentTime( &StartTime );


            /* Create a dummy compact header. Then prepend that to
             * the actual message.
             */
            Amdb_GetTopicInfo( ptlLog, &topicinfo );
            wsprintf( lpTmpBuf, szCompactHdrTmpl,
                     szLogFolder,
                     "Connects",
                     topicinfo.dwMaxMsg + 1,
                     "AmeolLog",
                     hstrlen( lpBuf),
                     StartDate.iDay,
                     (LPSTR)pszCixMonths[ StartDate.iMonth - 1 ],
                     StartDate.iYear % 100,
                     StartTime.iHour,
                     StartTime.iMinute );
            strcat( lpTmpBuf, "\r\n" );

            lpszLogText2 = ConcatenateStrings( lpTmpBuf, lpBuf );
            /* Concatenated okay, so create the actual message.
             */
            if( NULL != lpszLogText2 )
               {
               char lpResult[500];
               HEADER loghdr;
               int nMatch;
               PTL ptl;
               PTL ptl2;
               int r;

               if( fAlertOnFailure && CheckForBlinkErrors(lpszLogText2, (char *)&lpResult, 499) )
               {
                  char lBuf[600];

                  wsprintf( lBuf, "Possible Errors Detected During Blink: \r\n\r\n%s\r\nPlease check Logs/Connects for more information", lpResult );
//                fDlgMessageBox( hwndFrame, 0, IDDLG_WARNOB, lBuf, 60000, IDD_OBOK );
                  fMessageBox( hwndFrame, 0, lBuf, MB_OK|MB_ICONEXCLAMATION );

               }
               /* Create a title.
                */
               wsprintf( szTitle, GS(IDS_STR1076),
                         "Connect Log",
                         Amdate_FormatLongDate( &StartDate, NULL ),
                         Amdate_FormatTime( &StartTime, NULL ) );

               /* Now create the message.
                */
               ptl = ptl2 = ptlLog;
               memset( &loghdr, 0, sizeof(HEADER) );
               strncpy( loghdr.szFrom, (char *)"AmeolLog", sizeof( loghdr.szFrom ) );
               strncpy( loghdr.szTitle, szTitle, sizeof( loghdr.szTitle ) );
               nMatch = MatchRule( &ptl, lpszLogText2, &loghdr, TRUE );
               if( !( nMatch & FLT_MATCH_COPY ) && ptl != ptl2 )
                  {
                     ptl2 = ptl;
                     Amdb_LockTopic( ptl2 );
                  }
               if( !( nMatch & FLT_MATCH_DELETE ) )
                  {
                  r = Amdb_CreateMsg( ptl2,
                             &pth,
                             topicinfo.dwMaxMsg + 1,
                             0,
                             szTitle,
                             (char *)"AmeolLog",
                             "",
                             &StartDate,
                             &StartTime,
                             lpszLogText2,
                             hstrlen( lpszLogText2 ), 0L );

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
                           DoCopyMsgRange( hwndFrame, lppth, 1, ptl, FALSE, FALSE, FALSE );
                           }
                        }
                     if( nMatch & FLT_MATCH_PRIORITY )
                        Amdb_MarkMsgPriority( pth, TRUE, TRUE );
                     if( nMatch & FLT_MATCH_READ )
                        Amdb_MarkMsgRead( pth, TRUE );
                     if( nMatch & FLT_MATCH_IGNORE )
                        Amdb_MarkMsgIgnore( pth, TRUE, TRUE );
                     if( nMatch & FLT_MATCH_READLOCK )
                        Amdb_MarkMsgReadLock( pth, TRUE );
                     if( nMatch & FLT_MATCH_KEEP )
                        Amdb_MarkMsgKeep( pth, TRUE );
                     if( nMatch & FLT_MATCH_MARK )
                        Amdb_MarkMsg( pth, TRUE );
                     }
                  }
               if( !( nMatch & FLT_MATCH_COPY ) && ptl != ptl2 )
                  Amdb_UnlockTopic( ptl2 );
               FreeMemory32( &lpBuf );
               FreeMemory32( &lpszLogText2 );
               Amuser_CallRegistered( AE_UNREADCHANGED, (LPARAM)ptlLog, 0L );
               }
            }
         }
         }
      Amfile_Close( fh );
      }
      }

   /* Fix toolbar buttons.
    */
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_ONLINE, !fOnline );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_STOP, fOnline );
   EnableBlinkButtons( TRUE );

   /* If the blink failed, offer to recover.
    */
   if( MustRecoverScratchpad() && ( beCur.dwBlinkFlags & BF_CIXFLAGS ) )
      {
      int r;

      EnsureMainWindowOpen();
      r = fDlgMessageBox( hwndFrame, idsRECOVERSCRATCHPAD, IDDLG_BLINKFAILED, NULL, 30000, IDD_RECOVERLATER );
      if( IDD_RECOVERNOW == r )
         {
         int id;

         /* Try a CIX Only blink. If not found, then
          * use the last blink entry.
          */
         if( 0 == ( id = GetBlinkCommandID( "CIX Recovery" ) ) )
            id = beCur.iCommandID;
         fRecoverNow = TRUE;
         PostCommand( hwndFrame, id, 0L );
         }
      }

   /* If parsing deferred until post blink, kick off the
    * scratchpad parser now.
    */
   else if( !fParsing && !fParseDuringBlink )
      {
      if( DequeueScratchpad() )
      {
         return;
      }

      /* If no scratchpad to be parsed and we quit after
       * connecting, then quit now.
       */
      if( fQuitAfterConnect && !fNoExit )
         PostMessage( hwndFrame, WM_CLOSE, 0, 0L );
      }


   /* If enabled, show the first unread message.
    */
   if( !fQuitting && fUnreadAfterBlink ) 
      if( !( fCompatFlag & COMF_NOTNEXTUNREAD  && !( NULL == hwndTopic ) ) )
      {
      CURMSG unr;

      unr.pcat = NULL;
      unr.pcl = NULL;
      unr.ptl = NULL;
      unr.pth = NULL;
      if( Amdb_GetNextUnReadMsg( &unr, VM_VIEWREF ) )
         {
         OpenMsgViewerWindow( unr.ptl, unr.pth, FALSE );
         Amdb_InternalUnlockTopic( unr.ptl );
         }
      }
   if( !fQuitting && fNewMail && !fNewMailPlayed )
   {
      if( fNoisy )
         Am_PlaySound( "New Mail" );
      if( fNewMailAlert )
      {
         fAlertTimedOut = FALSE;
         if( !IsIconic( hwndFrame ) )
         {
            SetTimer( hwndFrame, TID_MAILALERTTO, 120000, NULL );
            SetTimer( hwndFrame, TID_MAILALERT, 500, NULL );
            fMessageBox( hwndFrame, 0, GS(IDS_STR1254), MB_OK|MB_ICONINFORMATION );

// Count do similar to this here to stop it locking Ameol until they OK the dialogue
//          fDlgMessageBox( hwndFrame, 0, IDDLG_WARNOB, lBuf, 30000, IDD_OBOK );
         }
         else
         {
            StatusText( GS(IDS_STR1254) );
            SetTimer( hwndFrame, TID_MAILALERTTO, 3000, NULL );
            SetTimer( hwndFrame, TID_MAILALERT, 250, NULL );
         }
         if( !IsIconic( hwndFrame ) && !fAlertTimedOut )
         {
            KillTimer( hwndFrame, TID_MAILALERTTO );
            KillTimer( hwndFrame, TID_MAILALERT );
            FlashWindow( hwndFrame, 0 );
         }
      }
      fNewMail = FALSE;
      fNewMailPlayed = TRUE;
   }
}

/* This function ends processing one service and posts
 * a message to force Blink() to be called.
 */
void FASTCALL EndProcessing( OBCLSID obclsidWanted )
{
   PostMessage( hwndFrame, WM_BLINK2, 0, (LPARAM)obclsidWanted );
}

/* Blink
 * Find the first pending object in the out-basket. If none are
 * found, clear the fBlinking flag.
 */
BOOL FASTCALL Blink( OBCLSID clsidWanted )
{
   static BOOL fEntry = FALSE;
   BOOL fStarted;
   BOOL fActive;
   LPOB lpob;

   /* Guard against re-entrancy!
    */
   ASSERT( FALSE == fEntry );
   fEntry = TRUE;

   /* Remove the current item, if any.
    */
   if( lpobCurrent )
      {
      OBINFO obinfo;

      Amob_GetObInfo( lpobCurrent, &obinfo );
      if( !( obinfo.obHdr.wFlags & OBF_KEEP ) )
         RemoveObject( lpobCurrent );
      else
         {
         obinfo.obHdr.wFlags &= ~(OBF_PENDING|OBF_ACTIVE|OBF_SCRIPT);
         Amob_SetObInfo( lpobCurrent, &obinfo );
         UpdateOutbasketItem( lpobCurrent, TRUE );
         }
      lpobCurrent = NULL;
      }

   /* Find a pending action in the out-basket.
    */
   lpob = Amob_GetOb( NULL );
   fStarted = FALSE;
   fActive = FALSE;

   /* If user stopped the blink then this lpbob is set to NULL. This ensures
    * that no objects are retrived from the Out Basket and processed. It
    * also ensures that at the end of this function, blink is terminated.
    * YH 10/05/96
    */
   if( fEndBlinking )
      lpob = NULL;

// SetTimer( hwndFrame, TID_OLT, (UINT)60000, NULL );

   while( lpob && fBlinking )
      {
      OBINFO obinfo;

      Amob_GetObInfo( lpob, &obinfo );
      if( obinfo.obHdr.wFlags & OBF_ACTIVE )
         fActive = TRUE;
      else if( ( obinfo.obHdr.wFlags & OBF_PENDING ) && !( obinfo.obHdr.wFlags & (OBF_OPEN|OBF_HOLD|OBF_ERROR) ) && (obinfo.dwClsType & beCur.dwBlinkFlags) )
         {
         if( 0xFFFF == clsidWanted || ( clsidWanted == obinfo.obHdr.clsid ) )
            {
            obinfo.obHdr.wFlags |= OBF_ACTIVE;
            obinfo.obHdr.wLastActionDate = Amdate_GetPackedCurrentDate();
            obinfo.obHdr.wLastActionTime = Amdate_GetPackedCurrentTime();
            Amob_SetObInfo( lpob, &obinfo );
            lpobCurrent = lpob;
            UpdateOutbasketItem( lpob, TRUE );
            switch( Amob_Exec( obinfo.obHdr.clsid, obinfo.lpObData ) )
               {
               case POF_HELDBYSCRIPT:
                  obinfo.obHdr.wFlags &= ~OBF_ACTIVE;
                  obinfo.obHdr.wFlags |= OBF_SCRIPT;
                  Amob_SetObInfo( lpob, &obinfo );
                  UpdateOutbasketItem( lpob, TRUE );
                  lpob = Amob_GetOb( lpob );
                  continue;

               case POF_AWAITINGSERVICE:
                  {
                     ;
                     //
                  }
                  
                  break;

               case POF_CANNOTACTION:
                  obinfo.obHdr.wFlags &= ~OBF_ACTIVE;
                  obinfo.obHdr.wFlags |= OBF_ERROR;
                  Amob_SetObInfo( lpob, &obinfo );
                  UpdateOutbasketItem( lpob, TRUE );
                  lpob = Amob_GetOb( lpob );
                  continue;
      
               case POF_PROCESSSTARTED:
                  fStarted = TRUE;
                  break;

               case POF_PROCESSCOMPLETED:
                  if( !( obinfo.obHdr.wFlags & OBF_KEEP ) )
                     {
                     RemoveObject( lpobCurrent );
                     lpobCurrent = NULL;
                     }
                  else
                     {
                     obinfo.obHdr.wFlags &= ~(OBF_PENDING|OBF_ACTIVE|OBF_ERROR);
                     Amob_SetObInfo( lpob, &obinfo );
                     UpdateOutbasketItem( lpobCurrent, TRUE );
                     }
                  lpob = Amob_GetOb( NULL );
                  continue;
               }
            break;
            }
         }
      lpob = Amob_GetOb( lpob );
      }

   /* No more out-basket objects found, so clear
    * the blink flag.
    */
   if( !lpob && clsidWanted == 0xFFFF && !fActive )
      {
      if( fBlinking )
         {
            BOOL fCalled = FALSE;

            while( iAddonsUsingConnection > 0 )
            {
               if( !fCalled )
               {
                  Amuser_CallRegistered( AE_DISCONNECTWAITING, 0, 0 );
                  fCalled = TRUE;
               }
               Sleep( 0 );
               TaskYield();
            }
            if( !fOnline )
               EndBlink();
            else
               EndOnlineBlink();
//          KillTimer( hwndFrame, TID_OLT );

            //!!SM!!
            if( fWarnAfterBlink && ( 0 != Amob_GetCount( OBF_UNFLAGGED ) ) )
            {
               char lBuf[100];

               if (Amob_GetCount( OBF_UNFLAGGED ) == 1)
                  wsprintf( lBuf, "There is an un-processed item still in your Out Basket");
               else
                  wsprintf( lBuf, "There are %d un-processed items still in your Out Basket", Amob_GetCount( OBF_UNFLAGGED ) );
               fDlgMessageBox( hwndFrame, 0, IDDLG_WARNOB, lBuf, 30000, IDD_OBOK );
            }
         }
      }
   fEntry = FALSE;
   return( fStarted );
}

/* This function locates the out-basket object with the specified
 * ID and delete it.
 */
void FASTCALL RemoveObItem( int id )
{
   LPOB lpob;

   lpob = NULL;
   while( NULL != ( lpob = Amob_GetByID( lpob, (WORD)id ) ) )
      {
      OBINFO obinfo;
      LPOB lpobNext;

      lpobNext = Amob_GetOb( lpob );
      Amob_GetObInfo( lpob, &obinfo );
      if( obinfo.obHdr.wFlags & OBF_PENDING )
         {
         if( lpob == lpobCurrent )
            lpobCurrent = NULL;
         Amob_RemoveObject( lpob, FALSE );
         }
      lpob = lpobNext;
      if( NULL == lpob )
         break;
      }
   if( NULL != hwndOutBasket )
      ShowOutBasketTotal( hwndOutBasket );
   Amob_SaveOutBasket( FALSE );
}

/* This function disables the screen saver if it was enabled, and returns
 * a flag that specifies its state prior to being disabled.
 */
BOOL FASTCALL DisableScreenSaver( void )
{
   int dummy;

   SystemParametersInfo( SPI_GETSCREENSAVEACTIVE, 0, &fScreenSaveActive, FALSE );
   SystemParametersInfo( SPI_SETSCREENSAVEACTIVE, FALSE, &dummy, FALSE );
   return( fScreenSaveActive );
}

/* This function re-enables the screen saver depending on the state
 * of the fScreenSaveActive flag.
 */
BOOL FASTCALL EnableScreenSaver( void )
{
   int dummy;

   SystemParametersInfo( SPI_SETSCREENSAVEACTIVE, fScreenSaveActive, &dummy, FALSE );
   return( TRUE );
}

/* This function starts the blink icon timer running if the animation
 * has been enabled.
 */
void FASTCALL StartBlinkAnimation( void )
{
   if( fDoBlinkAnimation )
      {
      ASSERT( !fStartedBlinkAnimation );
      nBlinkAnimationStep = 0;
      if( cBlinkIcons )
         fStartedBlinkAnimation = TRUE;
      }
   SetTimer( hwndFrame, TID_BLINKANIMATION, BLINK_MILLISECONDS, NULL );
}

/* This function loads the icons used during
 * blink animation.
 */
BOOL FASTCALL LoadBlinkIcons( void )
{
   cBlinkIcons = 0;
   if( fNewMemory( (LPVOID)&pBlinkIcons, NUM_ICONS * sizeof(HICON) ) )
      {
      for( cBlinkIcons = 0; cBlinkIcons < NUM_ICONS; ++cBlinkIcons )
         pBlinkIcons[ cBlinkIcons ] = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_ICON1 + cBlinkIcons) );
      return( TRUE );
      }
   return( FALSE );
}

/* This function unloads the icons used during
 * blink animation.
 */
void FASTCALL UnloadBlinkIcons( void )
{
   if( 0 < cBlinkIcons )
      {
      register int i;

      for( i = 0; i < cBlinkIcons; ++i )
         DestroyIcon( pBlinkIcons[ i ] );
      FreeMemory( (LPVOID)&pBlinkIcons );
      cBlinkIcons = 0;
      }
}

/* This function ends the blink animation.
 */
void FASTCALL EndBlinkAnimation( void )
{
   /* First restore sanity.
    */
   KillTimer( hwndFrame, TID_BLINKANIMATION );

   /* Only do this if we're doing blink
    * animation.
    */

   if( fDoBlinkAnimation && cBlinkIcons > 0 )
   {
      ASSERT( fStartedBlinkAnimation );

         /* Restore the application icon.
       */
      if( IsIconic( hwndFrame ) )
         {
         /* Needed because of a Windows 'quirk'. You have
          * to do a FlashWindow before and after changing
          * the icon of a minimised window.
          */
//       FlashWindow( hwndFrame, TRUE );
         SetAppIcon( hwndFrame, hAppIcon );
//       FlashWindow( hwndFrame, TRUE );
         }
      else
         SetAppIcon( hwndFrame, hAppIcon );
      fStartedBlinkAnimation = FALSE;
   }
}

/* This function is called when the blink animation timer
 * times out. We step thru the next frame of the animation.
 */
void FASTCALL StepBlinkAnimation( void )
{
   if( fDoBlinkAnimation && cBlinkIcons > 0 )
      {
      ASSERT( fStartedBlinkAnimation );

      /* Step to next frame.
       */
      if( ++nBlinkAnimationStep == cBlinkIcons )
         nBlinkAnimationStep = 0;

      /* If we're minimised, cycle the program icon. Otherwise
       * change the icon in the Blink window.
       */
      if( IsIconic( hwndFrame ) )
         {
//       FlashWindow( hwndFrame, TRUE );
         SetAppIcon( hwndFrame, pBlinkIcons[ nBlinkAnimationStep ] );
//       FlashWindow( hwndFrame, TRUE );
         }
      else if( NULL != hwndBlink )
         SendDlgItemMessage( hwndBlink, IDD_BLINKICON, STM_SETICON, (WPARAM)pBlinkIcons[ nBlinkAnimationStep ], 0L );
      }

   /* Also update the elapsed blink time.
    */
   if( NULL != hwndBlink && 0 != wOldService )
   {
      DWORD dwConnectTime;
      char sz[ 20 ];
      int seconds;
      int minutes;
      int hours;

      dwConnectTime = ( GetTickCount() - dwConnectOn ) / 1000;
      seconds = (int)( dwConnectTime % 60 );
      minutes = (int)( ( dwConnectTime / 60 ) % 60 );
      hours = (int)( dwConnectTime / 3600 );
      wsprintf( sz, "%2.2u:%2.2u:%2.2u", hours, minutes, seconds );
      SetDlgItemText( hwndBlink, IDD_TIME, sz );
   }  
}

/* This function creates the modeless blink dialog window. The
 * window is initially invisible.
 */
BOOL FASTCALL CreateBlinkWindow( void )
{
   if( fShowBlinkWindow )
      {
      /* Load the blink animation icons.
       */
      LoadBlinkIcons();
      /* Create the Blink window.
       */
      ASSERT( NULL == hwndBlink );
      hwndBlink = Adm_CreateDialog( hRscLib, hwndFrame, MAKEINTRESOURCE(IDDLG_BLINK), BlinkStatus, 0L );
      }
   return( hwndBlink != NULL );
}

/* This function closes the modeless blink dialog window.
 */
void FASTCALL DestroyBlinkWindow( void )
{
   if( NULL != hwndBlink )
      {
      DestroyWindow( hwndBlink );
      hwndBlink = NULL;
      }
   UnloadBlinkIcons();
}

/* This function writes the online status text information. It writes
 * to the blink window if it is visible, to the status bar otherwise.
 */

void CDECL AppendBuffer(HWND hWnd, LPSTR lpszText) // 2.56.2051 FS#91
{
   
   LPSTR lpTemp;
   unsigned int l, c;
   
   c = 10000;

   INITIALISE_PTR(lpTemp);
   if( fNewMemory( &lpTemp, c + 1 ) )
   {
      Edit_GetText(hWnd, lpTemp, c);
      l = strlen(lpszText)+3;
      if(strlen(lpTemp) + l > c)
      {
         memmove( lpTemp, lpTemp + l, c );
      }
      strcat(lpTemp, lpszText);
      strcat(lpTemp, "\r\n" );
      
      Edit_SetSel( hWnd, 0, c + 1 );
      Edit_ReplaceSel( hWnd, lpTemp );
      
      FreeMemory( &lpTemp );
   }
}

void CDECL OnlineStatusText( LPSTR lpszText, ... )
{
   char szBuffer[ 300 ];
   va_list va_arg;

   va_start( va_arg, lpszText );
   if( NULL != hwndBlink )
      {
      HWND hwndEdit;

      wvsprintf( szBuffer, lpszText, va_arg );
      //strcat( szBuffer, "\r\n" );
      VERIFY( hwndEdit = GetDlgItem( hwndBlink, IDD_PROGRESS ) );
      AppendBuffer(hwndEdit, szBuffer);                      // 2.56.2051 FS#91
//    Edit_SetSel( hwndEdit, 32767, 32767 );
//    Edit_ReplaceSel( hwndEdit, szBuffer );
      }
   else
      {
      // char szServiceName[ 40 ];

      /* Display on status bar prefixed with service
       * name.
       */
      *szBuffer = '\0';
      
      // Removed by pw as it makes the status guage overwrite the 
      // outbasket indicator and clock

      /* if( 0 != wOldService )
         {
         GetServiceName( wOldService, szServiceName, sizeof(szServiceName) );
         wsprintf( szBuffer, "%s: ", szServiceName );
         }
      */
      wvsprintf( szBuffer + strlen( szBuffer ), lpszText, va_arg );
      StatusText( szBuffer );
      }
   va_end( va_arg );
}

/* This function is called whenever the connection code
 * switches between services.
 */
void FASTCALL SwitchService( int wService )
{
   if( wService != wOldService )
      {
      char szServiceName[ 40 ];

      /* First fire off an event.
       */
      Amuser_CallRegistered( AE_SWITCHING_SERVICE, wService, wOldService );

      /* If this is the first service, start the blink timer.
       */
      if( 0 == wOldService )
         dwConnectOn = GetTickCount();

      /* Switch to the new service.
       */
      if( 0 != wService )
         {
         GetServiceName( wService, szServiceName, sizeof(szServiceName) );
         SetDlgItemText( hwndBlink, IDD_SERVICE, szServiceName );
         dwConnectThisService = GetTickCount();
         }
      wOldService = wService;

      /* Finally fire off an event.
       */
      Amuser_CallRegistered( AE_SWITCHED_SERVICE, wService, wOldService );
      }
}

/* This function returns the name of the specified service.
 */
void FASTCALL GetServiceName( int wService, char * pBuf, int cbBufSize )
{
   switch( wService )
      {
      case SERVICE_RAS:    lstrcpyn( pBuf, "DUN", cbBufSize ); break;
      case SERVICE_CIX:    lstrcpyn( pBuf, "CIX Forums", cbBufSize ); break;
      case SERVICE_IP:     lstrcpyn( pBuf, szProvider, cbBufSize ); break;
      }
}

/* This function resets the progress gauge in the blink dialog.
 * We make the window (and other stuff) visible and set the
 * counter to zero.
 */
void FASTCALL ResetBlinkGauge( void )
{
   if( hwndBlink )
   {
      HWND hwndGauge;

      VERIFY( hwndGauge = GetDlgItem( hwndBlink, IDD_GAUGE ) );
      SendMessage( hwndGauge, PBM_SETRANGE, 0, MAKELPARAM( 0, 100 ) );
      SendMessage( hwndGauge, PBM_SETPOS, 0, 0L );
      ShowWindow( hwndGauge, SW_SHOWNA );
      SetDlgItemText( hwndBlink, IDD_PERCENTAGE, "0%" );
      SetDlgItemText( hwndBlink, IDD_INFO, "" );
      SetDlgItemText( hwndBlink, IDD_SIZEINFO, "" );
      SetDlgItemText( hwndBlink, IDD_FILETIME, "" );
      SetDlgItemText( hwndBlink, IDD_LASTMESSAGE, "" );
      ShowWindow( GetDlgItem( hwndBlink, IDD_PERCENTAGE ), SW_SHOWNA );
      ShowWindow( GetDlgItem( hwndBlink, IDD_INFO ), SW_SHOWNA );
      ShowWindow( GetDlgItem( hwndBlink, IDD_SIZEINFO ), SW_SHOWNA );
      ShowWindow( GetDlgItem( hwndBlink, IDD_FILETIME ), SW_SHOWNA );
      ShowWindow( GetDlgItem( hwndBlink, IDD_LASTMESSAGE ), SW_SHOWNA );
   }
   else
   {
      SendMessage( hwndStatus, SB_GETTEXT, idmsbOnlineText, (LPARAM)lpTmpBuf );
      SendMessage( hwndStatus, SB_BEGINSTATUSGAUGE, 0, (LPARAM)lpTmpBuf );
   }
}

/* This function hides the progress gauge in the Blink
 * dialog.
 */
void FASTCALL HideBlinkGauge( void )
{
   if( hwndBlink )
   {
      ShowWindow( GetDlgItem( hwndBlink, IDD_GAUGE ), SW_HIDE );
      ShowWindow( GetDlgItem( hwndBlink, IDD_PERCENTAGE ), SW_HIDE );
      ShowWindow( GetDlgItem( hwndBlink, IDD_INFO ), SW_HIDE );
      ShowWindow( GetDlgItem( hwndBlink, IDD_SIZEINFO ), SW_HIDE );
      ShowWindow( GetDlgItem( hwndBlink, IDD_FILETIME ), SW_HIDE );
      ShowWindow( GetDlgItem( hwndBlink, IDD_LASTMESSAGE ), SW_HIDE );
   }
   else
      SendMessage( hwndStatus, SB_ENDSTATUSGAUGE, 0, 0L );
}

/* This function steps the progress gauge in the Blink dialog.
 * The input range must be a percentage.
 */
void FASTCALL StepBlinkGauge( int wStep )
{
   ASSERT( wStep >= 0 && wStep <= 100 );
   if( hwndBlink )
   {
      HWND hwndGauge;
      char sz[ 10 ];

      VERIFY( hwndGauge = GetDlgItem( hwndBlink, IDD_GAUGE ) );
      SendMessage( hwndGauge, PBM_SETPOS, wStep, 0L );
      wsprintf( sz, "%u%%", wStep );
      SetDlgItemText( hwndBlink, IDD_PERCENTAGE, sz );
   }
   else
   {
      SendMessage( hwndStatus, SB_GETTEXT, idmsbOnlineText, (LPARAM)lpTmpBuf );
      SendMessage( hwndStatus, SB_STEPSTATUSGAUGE, wStep, (LPARAM)lpTmpBuf );
   }
}

void FASTCALL FinishStepingBlinkGauge( int pCurrent )
{
   int i;
   for( i = pCurrent; i <= 100; ++i )
      StepBlinkGauge( i );
}


void EXPORT FAR PASCAL MyDialogYield(HWND hwnd)
{
   MSG msg;

   if (!PeekMessage(&msg, hwnd, 0, 0, PM_REMOVE))
      return;

   TranslateMessage(&msg);
   DispatchMessage(&msg);
}

void FASTCALL ShowDownloadCount( DWORD pAmount, LPSTR pText )
{
   char lStr[255];

   if ( IsWindow(hwndBlink) && IsWindowVisible(GetDlgItem(hwndBlink, IDD_SIZEINFO)) )
   {
      wsprintf(lStr, "%s %d", pText, pAmount);  
      SetDlgItemText(hwndBlink, IDD_SIZEINFO, lStr); 
//    if ( (pAmount % 5) == 0)
//       MyDialogYield(hwndBlink);
   }
}

/* This function shows download statistics in the Blink dialog.
 */
void FASTCALL ShowDownloadStatistics( LPFILETRANSFERINFO lpftinfo )
{
   if( NULL != hwndBlink )
      {
      char szSizeSoFar[ 40 ];
      char szTotalSize[ 40 ];

      FormatThousands( lpftinfo->dwSizeSoFar, szSizeSoFar ),
      FormatThousands( lpftinfo->dwTotalSize, szTotalSize ),
      wsprintf( lpTmpBuf, GS(IDS_STR1005), szSizeSoFar, szTotalSize );
      SetDlgItemText( hwndBlink, IDD_SIZEINFO, lpTmpBuf );
      if( 0 != lpftinfo->dwTimeRemaining )
         {
         wsprintf( lpTmpBuf, GS(IDS_STR1006), Amdate_FormatLongTime( lpftinfo->dwTimeRemaining ) );
         SetDlgItemText( hwndBlink, IDD_INFO, lpTmpBuf );
         }
      wsprintf( lpTmpBuf, GS(IDS_STR1007), lpftinfo->cps, lpftinfo->cErrors );
      SetDlgItemText( hwndBlink, IDD_FILETIME, lpTmpBuf );
      SetDlgItemText( hwndBlink, IDD_LASTMESSAGE, lpftinfo->szLastMessage );
      }
}

/* This function handles the dialog box messages passed to the ExportDlgStatus
 * dialog.
 */
BOOL EXPORT CALLBACK BlinkStatus( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = BlinkStatus_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the ExportDlgStatus
 * dialog.
 */
LRESULT FASTCALL BlinkStatus_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, BlinkStatus_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, BlinkStatus_OnCommand );
      HANDLE_MSG( hwnd, WM_MOVE, BlinkStatus_OnMove );

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL BlinkStatus_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   int x;
   int y;
   RECT rcWnd;
   RECT rcDef;

   /* Place the dialog where the user last left it.
    */
   x = Amuser_GetPPInt( szSettings, "BlinkStatusDialogX", 0 );
   y = Amuser_GetPPInt( szSettings, "BlinkStatusDialogY", 0 );

   /* Make sure status window is on-screen.
    */
   GetClientRect( GetDesktopWindow(), &rcWnd );
   rcDef.left = rcWnd.right / 3;
   rcDef.top = rcWnd.bottom / 3;

   if( ( ( x + 308 ) > rcWnd.right ) || x < 0 )
      x = rcDef.left;
   if( ( ( y + 266 ) > rcWnd.bottom ) || y < 0 )
      y = rcDef.top;
   if( x > 0 && y > 0 )
      SetWindowPos( hwnd, NULL, x, y, 0, 0, SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER );

   /* Set the first icon.
    */
   if( cBlinkIcons )
      SendDlgItemMessage( hwnd, IDD_BLINKICON, STM_SETICON, (WPARAM)pBlinkIcons[ 0 ], 0L );
   return( TRUE );
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL BlinkStatus_OnMove( HWND hwnd, int x, int y )
{
   if( IsWindowVisible( hwnd ) )
      {
      RECT rc;

      GetWindowRect( hwnd, &rc );
      Amuser_WritePPInt( szSettings, "BlinkStatusDialogX", rc.left );
      Amuser_WritePPInt( szSettings, "BlinkStatusDialogY", rc.top );
      }
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL BlinkStatus_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDCANCEL:
      PostCommand( hwndFrame, IDM_STOP, 0L );
      break;
      }
}

/* This function clears all logged downtimes that precede the current
 * system time.
 */
void FASTCALL ClearOldPendingDowntimes( void )
{
   char * pTmpBuf;
   WORD wCurDate;

   wCurDate = Amdate_GetPackedCurrentDate();
   Amuser_GetPPString( szDowntime, NULL, "", pTmpBuf = lpTmpBuf, LEN_TEMPBUF );
   while( *pTmpBuf )
      {
      AM_DATE date;
      char * psz;

      psz = pTmpBuf;
      date.iDay = 0;
      date.iMonth = 0;
      date.iYear = 0;
      while( isdigit( *psz ) )
         date.iDay = date.iDay * 10 + ( *psz++ - '0' );
      if( *psz == '/' )
         ++psz;
      while( isdigit( *psz ) )
         date.iMonth = date.iMonth * 10 + ( *psz++ - '0' );
      if( *psz == '/' )
         ++psz;
      while( isdigit( *psz ) )
         date.iYear = date.iYear * 10 + ( *psz++ - '0' );
      if( Amdate_PackDate( &date ) < wCurDate )
         Amuser_WritePPString( szDowntime, pTmpBuf, NULL );
      pTmpBuf += strlen( pTmpBuf ) + 1;
      }
}

/* This function checks whether we're currently within a CIX downtime
 * period and warn the user.
 */
BOOL FASTCALL CheckForDowntime( HWND hwnd )
{
   AM_DATE curDate;
   char szDate[ 40 ];
   char szTime[ 40 ];
   int iHoursDiff;

   _tzset();
   iHoursDiff = (int)( _timezone / 3600 );
   Amdate_GetCurrentDate( &curDate );
   wsprintf( szDate, "%d/%d/%d", curDate.iDay, curDate.iMonth, curDate.iYear );
   if( Amuser_GetPPString( szDowntime, szDate, "", szTime, sizeof( szTime ) ) )
      {
      WORD wCurTime;
      WORD wStartTime;
      WORD wEndTime;
      AM_TIME pendingTime;
      AM_TIME startTime;
      AM_TIME endTime;
      char * psz;

      /* Parse off start of downtime
       */
      psz = szTime;
      startTime.iHour = 0;
      startTime.iMinute = 0;
      startTime.iSecond = 0;
      while( isdigit( *psz ) )
         startTime.iHour = startTime.iHour * 10 + ( *psz++ - '0' );
      if( *psz == ':' )
         ++psz;
      while( isdigit( *psz ) )
         startTime.iMinute = startTime.iMinute * 10 + ( *psz++ - '0' );
      ++psz;

      /* Parse off end of downtime 
       */
      endTime.iHour = 0;
      endTime.iMinute = 0;
      endTime.iSecond = 0;
      while( isdigit( *psz ) )
         endTime.iHour = endTime.iHour * 10 + ( *psz++ - '0' );
      if( *psz == ':' )
         ++psz;
      while( isdigit( *psz ) )
         endTime.iMinute = endTime.iMinute * 10 + ( *psz++ - '0' );

      /* Compensate for timezone.
       */
      startTime.iHour -= iHoursDiff;
      endTime.iHour -= iHoursDiff;

      /* Are we in the downtime range?
       */
      wCurTime = Amdate_GetPackedCurrentTime();
      wStartTime = Amdate_PackTime( &startTime );
      wEndTime = Amdate_PackTime( &endTime );
      if( wCurTime >= wStartTime && wCurTime <= wEndTime )
         {
         EnsureMainWindowOpen();
         wsprintf( lpTmpBuf, GS(IDS_STR567), endTime.iHour, endTime.iMinute );
         return( fDlgMessageBox( hwnd, 0, IDDLG_YESNODLG, lpTmpBuf, 15000, IDNO ) == IDYES );
         }

      /* Subtract 30 minutes from the current time. Is a downtime pending?
       */
      pendingTime = startTime;
      if( ( (int)( pendingTime.iMinute -= 30 ) ) < 0 ) {
         pendingTime.iMinute += 60;
         --pendingTime.iHour;
         }
      wStartTime = Amdate_PackTime( &pendingTime );
      if( wCurTime >= wStartTime && wCurTime <= wEndTime )
         {
         EnsureMainWindowOpen();
         wsprintf( lpTmpBuf, GS(IDS_STR568), startTime.iHour, startTime.iMinute, endTime.iHour, endTime.iMinute );
         return( fDlgMessageBox( hwnd, 0, IDDLG_YESNODLG, lpTmpBuf, 15000, IDNO ) == IDYES );
         }
      }
   return( TRUE );
}

/* This function initiates CIX connectivity. Because it is expected to start
 * a thread and return, we simulate this by posting a message to kick off
 * the actual connection.
 */
int FASTCALL Exec_CIX( void )
{
   if( fEntry )
      return( POF_AWAITINGSERVICE );
   PostMessage( hwndFrame, WM_EXEC_CIX, 0, 0L );
   return( POF_PROCESSSTARTED );
}

/* This is the actual CIX connectivity function. We're called from the
 * main program code in response to a WM_EXEC_CIX message.
 */
int FASTCALL Exec_CIX2( void )
{
   DWORD dwSecondsOnCix;
   char * pszLogFile;

   /* This code must not be re-entrant!
    */
   ASSERT( FALSE == fEntry );
   fEntry = TRUE;
   fCIXSyncInProgress = TRUE;
   wCommErr = WCE_NOERROR;
   fLoggedOn = FALSE;
   dwSecondsOnCix = 0;

   /* Context switch into appropriate service.
    */
   SwitchService( SERVICE_CIX );

   /* Open the log file.
    */
   pszLogFile = NULL;
   if( fOpenTerminalLog )
      {
         BEGIN_PATH_BUF;
         Amuser_GetActiveUserDirectory( lpPathBuf, _MAX_PATH );
         strcat( strcat( lpPathBuf, "\\" ), szConnectLog );
         pszLogFile = (char *)lpPathBuf;
         fConnectLogOpen = TRUE;
         END_PATH_BUF;
      }

   /* Create a dumb viewer terminal.
    */
   if( ( NULL == hwndCixTerminal && fViewCixTerminal ) || ( beCur.dwBlinkFlags & BF_STAYONLINE ) )
      hwndCixTerminal = NewTerminal( hwndFrame, "Dumb Terminal", TRUE );

   /* Connect to CIX using the CIX connection card. No need for a
    * callback as we're working asynchronously. But if this changes, the
    * callback will be useful.
    */
#ifdef WIN32
   if( Amcomm_Open( &lpcdevCIX, beCur.szConnCard, NULL, 0L, pszLogFile, hwndCixTerminal, &beCur.rd, TRUE ) )
#else
   if( Amcomm_Open( &lpcdevCIX, beCur.szConnCard, NULL, 0L, pszLogFile, hwndCixTerminal, NULL ) )
#endif
      {
      static char * wstr1[] = { "More ?", "Main:", "M:", "\x1B[;37m", "R:", "Rf:", "Read:", "Mail:", "Ml:", "Your pointers are being updated." };
      static char * wstr2[] = { "y/n)? ", "NOW!!!", "y/n/q)? " };
      static char * wstr3[] = { "NOW!!!" };
      register int r;

      /* Wade through to the Main: or M: prompt.
       */
      if( wCommErr == WCE_NOERROR )
         {
         DWORD dwCixConnectTime;

         /* Save the date and time when we logged onto CIX.
          */
         dwCixConnectTime = GetTickCount();
         Amdate_GetCurrentDate( &curDate );
         Amdate_GetCurrentTime( &curTime );
         fLoggedOn = TRUE;

         wsprintf( lpTmpBuf, GS(IDS_STR1215), szCIXNickname );
         WriteToBlinkLog( lpTmpBuf );

         /* Wade through to the Main: or M: prompt.
          */
         do {
            while( ( r = Amcomm_WaitForString( lpcdevCIX, 9, wstr1, lpcdevCIX->lpcd->nTimeout ) ) == 0 )
               Amcomm_WriteChar( lpcdevCIX, CR );
            if( r >= 4 && r <= 8 )
               Amcomm_WriteString( lpcdevCIX, "q\n", 2 );
            if( r == 9 )
               wCommErr = WCE_ABORT;
            Amcomm_Delay( lpcdevCIX, 200 );
            }
         while( wCommErr == WCE_NOERROR && Amcomm_QueryCharReady( lpcdevCIX ) );

         /* Disable ANSI if ansi detected.
          */
         if( wCommErr == WCE_NOERROR )
            {
            if( r == 3 )
               {
               Amcomm_WriteString( lpcdevCIX, "opt ansi n q\n", 13 );
               Amcomm_WaitForString( lpcdevCIX, 3, wstr1, lpcdevCIX->lpcd->nTimeout );
               }

            /* Now execute the main script.
             */
            if( wCommErr == WCE_NOERROR )
               {
               OnlineStatusText( GS(IDS_STR1008) );
               Amuser_GetActiveUserDirectory( lpTmpBuf, _MAX_PATH );
               strcat( strcat( lpTmpBuf, "\\" ), szNewmesScr );
               ExecuteScript( lpcdevCIX, lpTmpBuf, TRUE );

               /* Save the main script.
                */
               Amuser_GetActiveUserDirectory( lpTmpBuf, _MAX_PATH );
               strcat( strcat( lpTmpBuf, "\\" ), szNewmesScr );
               Ameol2_Archive( lpTmpBuf, FALSE );

               /* Disconnect from CIX.
                */
               if( wCommErr == WCE_NOERROR && !( beCur.dwBlinkFlags & BF_STAYONLINE ) )
                  {
                  BOOL fReGotMail = FALSE;
                  OnlineStatusText( GS(IDS_STR1009) );
                  while( ( r = Amcomm_WaitForString( lpcdevCIX, 3, wstr2, lpcdevCIX->lpcd->nTimeout ) ) == 0 || r == 2 )
                     if( r == 2 ) {
                        Amcomm_WriteChar( lpcdevCIX, (char)( nLastError ? 'N' : 'Y' ) );
                        Amcomm_WriteChar( lpcdevCIX, CR );
                        }
                     else if( !nLastError && ( r == 0 ) && ( beCur.dwBlinkFlags & BF_GETCIXMAIL ) && !fReGotMail)
                     {
                        fReGotMail = TRUE;
                        Amcomm_WriteChar( lpcdevCIX, 'N' );
                        Amcomm_WriteChar( lpcdevCIX, CR );
                        if( CreateMailScript() )
                        {
                        Amuser_GetActiveUserDirectory( lpTmpBuf, _MAX_PATH );
                        strcat( strcat( lpTmpBuf, "\\" ), "ugetmail.scr" );
                        ExecuteScript( lpcdevCIX, lpTmpBuf, TRUE );
                        Amuser_GetActiveUserDirectory( lpTmpBuf, _MAX_PATH );
                        strcat( strcat( lpTmpBuf, "\\" ), "ugetmail.scr" );
                        Ameol2_Archive( lpTmpBuf, FALSE );
                        }
                     }
                     else {
                        Amcomm_WriteChar( lpcdevCIX, 'Y' );
                        Amcomm_WriteChar( lpcdevCIX, CR );
                        }

                  }
               else if( wCommErr == WCE_NOERROR && ( beCur.dwBlinkFlags & BF_STAYONLINE ) )
               {
                  OnlineStatusText( "Online to CIX - enter \"bye\" to log off" );
                  fBlinking = FALSE;
                  fInitiatingBlink = FALSE;
                  r = Amcomm_WaitForString( lpcdevCIX, 1, wstr3, lpcdevCIX->lpcd->nTimeout );
                  while( r != 0 && r != -1 )
                     /* Do nowt */
                     ;
                  OnlineStatusText( GS(IDS_STR1009) );
                  fBlinking = TRUE;
               }
               
               if( wCommErr == WCE_NOERROR )
               {
                  wsprintf( lpTmpBuf, GS(IDS_STR1216), szCIXNickname );
                  WriteToBlinkLog( lpTmpBuf );
               }
               }
            }

         /* Save how many seconds we were online to CIX.
          */
         dwSecondsOnCix = ( GetTickCount() - dwCixConnectTime ) / 1000;
         }

      /* Report back on any errors.
       */
      if( wCommErr != WCE_NOERROR )
         switch( wCommErr )
            {
            case WCE_LINEBUSY:   
               ReportOnline( GS(IDS_STR139) );
               if( !fBlinking )
                  WriteToBlinkLog( GS(IDS_STR139) );
               break;
            case WCE_NOCARRIER:  
               ReportOnline( GS(IDS_STR141) ); 
               if( !fBlinking )
                  WriteToBlinkLog( GS(IDS_STR141) );
               break;
            case WCE_TIMEOUT:    
               ReportOnline( GS(IDS_STR142) ); 
               if( !fBlinking )
                  WriteToBlinkLog( GS(IDS_STR142) );
               break;
            case WCE_ABORT:      
               ReportOnline( GS(IDS_STR143) ); 
               if( !fBlinking )
                  WriteToBlinkLog( GS(IDS_STR143) );
               break;
            case WCE_NOCONNECT:  
               ReportOnline( GS(IDS_STR144) ); 
               if( !fBlinking )
                  WriteToBlinkLog( GS(IDS_STR144) );
               break;
            }

      /* Finished or aborter, whatever. Close the port.
       * and tidy up.
       */
      fCIXSyncInProgress = FALSE;
      GoToSleep( 1000 );
      if( fSetForward && ( wCommErr == WCE_NOERROR ) )
         Amuser_WritePPInt( szCIX, "SetForward", FALSE );
      Amcomm_Close( lpcdevCIX );
      Amcomm_Destroy( lpcdevCIX );
      }
      else if( fDebugMode )
         {
         wsprintf( lpTmpBuf, GS(IDS_STR1222), "Amcomm_Open in blink.c" );
         WriteToBlinkLog( lpTmpBuf );
         }
      else
         WriteToBlinkLog( GS(IDS_STR1251) );



   /* Close any terminal window.
    */
   if( NULL != hwndCixTerminal )
      SendMessage( hwndCixTerminal, WM_CLOSE, 0, 0L );
   fCIXSyncInProgress = FALSE;
   fEntry = FALSE;
   EndProcessing( 0xFFFF );
   return( 0 );
}

void EXPORT WINAPI Ameol2_OnlineStatusMessage( LPSTR lpszMessage )
{
   OnlineStatusText( lpszMessage );
}

void EXPORT WINAPI Ameol2_ConnectionInUse( void )
{
   iAddonsUsingConnection++;
}

void EXPORT WINAPI Ameol2_ConnectionFinished( void )
{
   iAddonsUsingConnection--;
}
