/* LOG.C - Log blink actions to a Log topic.
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
#include "amlib.h"
#include <string.h>
#include "log.h"
#include "admin.h"
#include <ctype.h>
#include "rules.h"
#include "cix.h"

#define  LOG_LINE_WIDTH    90

static BOOL fBlinkLogOpened = FALSE;         /* TRUE if a blink log is open */
static AM_DATE StartDate;                    /* Date when blink started */
static AM_TIME StartTime;                    /* Time when blink started */
static int cEntries;                         /* Number of entries in the log */
static HPSTR lpszLogText;                    /* Pointer to log buffer */
static DWORD cbLogText;                      /* Size of log message */
static DWORD cbTrueLogText;                  /* Size of log message */
static DWORD cbLogBuffer;                    /* Size of log buffer */
static char szLogTitle[ 100 ];                  /* Title of current log entry */

const char NEAR szLogFolder[] = "Logs";
const char NEAR szLogTopic[] = "Blinks";

const char NEAR szLogAuthor[] = "AmeolLog";

void FASTCALL WriteRawToBlinkLog( char * );

/* This function opens a new blink log.
 */
void FASTCALL OpenBlinkLog( char * pszTitle )
{
   if( !fBlinkLogOpened )
      {
      char sz[ 200 ];

      /* Initialise for logging.
       */
      lpszLogText = NULL;
      cbLogText = 0;
      cbTrueLogText = 0;
      cEntries = 0;

      /* Save the title.
       */
      strncpy( szLogTitle, pszTitle, sizeof( szLogTitle ) );

      /* Record the start of logging.
       */
      Amdate_GetCurrentDate( &StartDate );
      Amdate_GetCurrentTime( &StartTime );

      /* Write the date/time of the start of the blink.
       */
      wsprintf( sz, GS(IDS_STR1077),
                Amdate_FormatLongDate( &StartDate, NULL ),
                Amdate_FormatTime( &StartTime, NULL ) );
      WriteRawToBlinkLog( sz );
      fBlinkLogOpened = TRUE;
      }
}

/* This function closes a blink log and writes
 * it as a message to the Log folder.
 */
void FASTCALL CloseBlinkLog( void )
{
   if( fBlinkLogOpened )
      {
      char sz[ 200 ];
      AM_DATE EndDate;
      AM_TIME EndTime;
      PCL pclLog;

      /* Write the date/time of the end of the blink.
       */
      Amdate_GetCurrentDate( &EndDate );
      Amdate_GetCurrentTime( &EndTime );
      wsprintf( sz, GS(IDS_STR1075),
                Amdate_FormatLongDate( &EndDate, NULL ),
                Amdate_FormatTime( &EndTime, NULL ) );
      WriteRawToBlinkLog( sz );

      /* Must have a blank line to delimit the message!
       */
      WriteRawToBlinkLog( "" );

      /* Now write the entire log to the Log topic.
       */
      if( NULL == ( pclLog = Amdb_OpenFolder( NULL, (char *)szLogFolder ) ) )
         pclLog = Amdb_CreateFolder( Amdb_GetFirstCategory(), (char *)szLogFolder, CFF_SORT|CFF_KEEPATTOP );
      if( NULL != pclLog )
         {
         PTL ptlLog;

         /* Open or create the log topic.
          */
         if( NULL == ( ptlLog = Amdb_OpenTopic( pclLog, (char *)szLogTopic ) ) )
         {
            ptlLog = Amdb_CreateTopic( pclLog, (char *)szLogTopic, FTYPE_LOCAL );
            if( NULL != ptlLog )
               Amdb_SetTopicFlags( ptlLog, TF_IGNORED, TRUE );
         }
         if( NULL != ptlLog )
            {
            TOPICINFO topicinfo;
            char szTitle[ 200 ];
            HPSTR lpszLogText2;
            PTH pth;

            /* Create a dummy compact header. Then prepend that to
             * the actual message.
             */
            Amdb_GetTopicInfo( ptlLog, &topicinfo );
            wsprintf( lpTmpBuf, szCompactHdrTmpl,
                     szLogFolder,
                     szLogTopic,
                     topicinfo.dwMaxMsg + 1,
                     szLogAuthor,
                     cbTrueLogText,
                     StartDate.iDay,
                     (LPSTR)pszCixMonths[ StartDate.iMonth - 1 ],
                     StartDate.iYear % 100,
                     StartTime.iHour,
                     StartTime.iMinute );
            strcat( lpTmpBuf, "\r\n" );
            lpszLogText2 = ConcatenateStrings( lpTmpBuf, lpszLogText );

            /* Concatenated okay, so create the actual message.
             */
            if( NULL != lpszLogText2 )
               {
               HEADER loghdr;
               int nMatch;
               PTL ptl;
               PTL ptl2;
               int r;

               /*
               if( HStrMatch(lpszLogText2, "unable to post", FALSE, FALSE) > 0 )
               {
                  char lBuf[100];

                  wsprintf( lBuf, "There were errors during your blink and Ameol was unable to post all your new messages" );
                  fDlgMessageBox( hwndFrame, 0, IDDLG_WARNOB, lBuf, 60000, IDD_OBOK );
               }
               */
               /* Create a title.
                */
               wsprintf( szTitle, GS(IDS_STR1076),
                         szLogTitle,
                         Amdate_FormatLongDate( &StartDate, NULL ),
                         Amdate_FormatTime( &StartTime, NULL ) );

               /* Now create the message.
                */
               ptl = ptl2 = ptlLog;
               memset( &loghdr, 0, sizeof(HEADER) );
               strncpy( loghdr.szFrom, (char *)szLogAuthor, sizeof( loghdr.szFrom ) );
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
                             (char *)szLogAuthor,
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
               FreeMemory32( &lpszLogText2 );
               }
            FreeMemory32( &lpszLogText );
            Amuser_CallRegistered( AE_UNREADCHANGED, (LPARAM)ptlLog, 0L );
            }
         }

      /* The log is now closed.
       */
      fBlinkLogOpened = FALSE;
   }
}

/* This function writes a line to the blink log prefixed with
 * the number of this entry and word wrapped as appropriate.
 */
void FASTCALL WriteToBlinkLog( char * pszText )
{
   if( fBlinkLogOpened )
      {
      char * pszBuf;

      INITIALISE_PTR(pszBuf);
      if( fNewMemory( &pszBuf, 512 ) )
         {
//       char szLine[ LOG_LINE_WIDTH + 6 ];
         char szLine[ LOG_LINE_WIDTH + LEN_INTERNETNAME + 6 ];
         AM_TIME ItemTime;

         /* Get the date/time for the blink line.
         */
         Amdate_GetCurrentTime( &ItemTime );

         /* Make a copy of the text.
          */
         strcpy( pszBuf, pszText );
         pszText = pszBuf;
   
         /* Handle lines longer than LOG_LINE_WIDTH characters by wrapping them.
          * The first line of each entry is prefixed by the number of
          * this entry.
          */
         wsprintf( szLine, "%-3u: ", ++cEntries );
         strcat( szLine, Amdate_FormatTime( &ItemTime, NULL ));
         strcat( szLine, ": " );

         while( strlen( pszText ) > LOG_LINE_WIDTH )
            {
            char * pszWordBreak;
   
            pszWordBreak = pszText + LOG_LINE_WIDTH;
            while( pszWordBreak > pszText && !isspace( *pszWordBreak ) )
               --pszWordBreak;
            if( pszWordBreak > pszText )
               *pszWordBreak++ = '\0';
            strncat( szLine, pszText, LOG_LINE_WIDTH );
            if( strlen( pszText ) > LOG_LINE_WIDTH )
               pszText = pszText + LOG_LINE_WIDTH;
            else
               pszText = pszWordBreak;
            WriteRawToBlinkLog( szLine );
            wsprintf( szLine, "     " );
            }
   
         /* Handle the last line, if there is one.
          */
         if( *pszText )
            {
            strcat( szLine, pszText );
            WriteRawToBlinkLog( szLine );
            }
         FreeMemory( &pszBuf );
         }
      } 
}

/* This function writes a line to the blink log.
 */
void FASTCALL WriteRawToBlinkLog( char * pszText )
{
   /* Cast to long to avoid wrap round because MAX_MSGSIZE is close to 0xFFFF
    */
   if( lpszLogText == NULL )
      {
      cbLogBuffer = 64000;
      cbLogText = 0;
      cbTrueLogText = 0;
      fNewMemory32( &lpszLogText, cbLogBuffer );
      }
   else if( cbLogText + (DWORD)strlen( pszText ) + 2L >= cbLogBuffer )
      {
      cbLogBuffer += 2048;
      fResizeMemory32( &lpszLogText, cbLogBuffer );
      }
   if( lpszLogText )
      {
      hmemcpy( lpszLogText + cbLogText, pszText, strlen( pszText ) );
      cbLogText += strlen( pszText );
      lpszLogText[ cbLogText++ ] = CR;
      lpszLogText[ cbLogText++ ] = LF;
      lpszLogText[ cbLogText ] = '\0';
      cbTrueLogText += strlen( pszText ) + 1;
      }
}

void EXPORT WINAPI Ameol2_WriteToBlinkLog( LPSTR lpszLogText )
{
   WriteToBlinkLog( lpszLogText );
}
