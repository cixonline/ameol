/* AMEVNT.C - Handles the event log and the event log dialog
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

#include "warnings.h"
#include <windows.h>
#include <windowsx.h>
#include "amlib.h"
#include "amuser.h"
#include <string.h>
#include <stdlib.h>
#include "amevent.h"

#define  THIS_FILE   __FILE__

#define  DEF_MAXEVENTLOGLINES    100
#define  DEF_PERMITTEDEVENTS     (ETYP_DATABASE|ETYP_COMMS|ETYP_INITTERM|ETYP_DISKFILE|ETYP_MAINTENANCE|ETYP_SEVERE|ETYP_WARNING|ETYP_MESSAGE)

typedef struct tagEVENT {
   struct tagEVENT FAR * lpNext;          /* Pointer to next event log item. */
   struct tagEVENT FAR * lpPrev;          /* Pointer to previous event log item */
   EVENTLOGITEM elItem;                   /* Event log item structure */
} EVENT;

typedef EVENT FAR * LPEVENT;

#define  EVENT_LOG_FILE    "eventlog.dat"

static char szEventLogFile[ _MAX_PATH ];
static char szEventLog[] = "Event Log";

static LPEVENT lpFirstEvent = NULL;       /* Pointer to first event */
static LPEVENT lpLastEvent = NULL;        /* Pointer to last event */
static WORD wLineCount = 0;               /* Number of lines in event log */
static WORD wPermittedEvents = 0;         /* Permitted recordable events */
static WORD wMaxEventLogLines = 0;        /* Maximum number of lines in event log */
static BOOL fEventLogLoaded = FALSE;      /* TRUE if event log loaded */
static BOOL fSevereEventNotify;           /* TRUE if a severe event occurred */

BOOL FASTCALL LoadEventLog( void );
BOOL FASTCALL SaveEventLog( void );
BOOL FASTCALL CreateEventLogItem( EVENTLOGITEM *, BOOL );


/* This function frees the event log memory
 */
void WINAPI EXPORT Amevent_FreeEventLog( void ) //20060325 - 2.56.2049.20
{
   LPEVENT lpEvent;

   /* First clear the log.
    */
   lpEvent = lpFirstEvent;
   while( lpEvent )
      {
      LPEVENT lpNextEvent;

      lpNextEvent = lpEvent->lpNext;
      FreeMemory( &lpEvent );
      lpEvent = lpNextEvent;
      }
   lpFirstEvent = lpLastEvent = NULL;
   wLineCount = 0;
}

/* This function deletes all entries from the event log.
 */
void WINAPI EXPORT Amevent_ClearEventLog( void )
{
   LPEVENT lpEvent;

   /* First clear the log.
    */
   lpEvent = lpFirstEvent;
   while( lpEvent )
      {
      LPEVENT lpNextEvent;

      lpNextEvent = lpEvent->lpNext;
      FreeMemory( &lpEvent );
      lpEvent = lpNextEvent;
      }
   lpFirstEvent = lpLastEvent = NULL;
   wLineCount = 0;
   SaveEventLog();

   /* Now send the AE_EVENTLOG_CLEARED event
    */
   Amuser_CallRegistered( AE_EVENTLOG_CLEARED, 0, 0 );
}

/* This function returns the event log permissions word.
 */
WORD WINAPI EXPORT Amevent_GetPermittedEventsMask( void )
{
   if( !fEventLogLoaded )
      LoadEventLog();
   return( wPermittedEvents );
}

/* This function sets the event log permissions word.
 */
void WINAPI EXPORT Amevent_SetPermittedEventsMask( WORD wNewPermittedEvents )
{
   wPermittedEvents = wNewPermittedEvents;
   Amuser_WritePPInt( szEventLog, "EventLogAllowed", wPermittedEvents );
}

/* This function returns the maximum size of the event log.
 */
WORD WINAPI EXPORT Amevent_GetEventLogMaximumSize( void )
{
   if( !fEventLogLoaded )
      LoadEventLog();
   return( wMaxEventLogLines );
}

/* This function sets the maximum size of the event log.
 */
void WINAPI EXPORT Amevent_SetEventLogMaximumSize( WORD wNewMaxEventLogLines )
{
   wMaxEventLogLines = wNewMaxEventLogLines;
   Amuser_WritePPInt( szEventLog, "EventLogMax", wMaxEventLogLines );
}

/* This function creates a new event log item given a specified
 * type and description.
 */
BOOL WINAPI EXPORT Amevent_AddEventLogItem( WORD wType, LPSTR lpszText )
{
   EVENTLOGITEM elItem;
   register int c;

   /* Make sure the event log is loaded.
    */
   if( !fEventLogLoaded )
      LoadEventLog();

   /* Reject event if it is filtered out.
    */
   if( ( (wType & ETYP_MASK) & (wPermittedEvents & ETYP_MASK) ) == 0 )
      return( FALSE );
   if( ( (wType & ETYP_MASK2) & (wPermittedEvents & ETYP_MASK2) ) == 0 )
      return( FALSE );

   /* Create and store the event structure.
    */
   elItem.wType = wType;
   elItem.wDate = Amdate_GetPackedCurrentDate();
   elItem.wTime = Amdate_GetPackedCurrentTime();
   for( c = 0; lpszText[ c ] && c < ELOG_DESCRIPTION; ++c )
      elItem.szDescription[ c ] = lpszText[ c ];
   elItem.szDescription[ c ] = '\0';
   if( !CreateEventLogItem( &elItem, TRUE ) )
      return( FALSE );

   /* For severe events, set a flag.
    * BUGBUG: Not used yet.
    */
   if( wType == ETYP_SEVERE )
      fSevereEventNotify = TRUE;
   return( SaveEventLog() );
}

/* This function returns the handle of the next event.
 */
LPEVENT EXPORT WINAPI Amevent_GetEvent( LPEVENT lpEvent )
{
   /* Make sure the event log is loaded.
    */
   if( !fEventLogLoaded )
      LoadEventLog();
   return( lpEvent ? lpEvent->lpNext : lpFirstEvent );
}

/* This function returns the handle of the next event.
 */
void EXPORT WINAPI Amevent_GetEventInfo( LPEVENT lpEvent, LPEVENTLOGITEM lpeli )
{
   *lpeli = lpEvent->elItem;
}

/* This function returns information about the specified, next or next matching item in the
 * event log.
 */
int EXPORT WINAPI Amevent_GetNextEventLogItem( WORD wType, int index, EVENTLOGITEM FAR * lpeli )
{
   LPEVENT lpEvent;
   register int c;

   if( !fEventLogLoaded )
      LoadEventLog();
   switch( wType )
      {
      case GNE_FIRST:
         *lpeli = lpFirstEvent->elItem;
         return( 0 );

      case GNE_LAST:
         *lpeli = lpLastEvent->elItem;
         return( wLineCount - 1 );

      case GNE_BYINDEX:
         lpEvent = lpFirstEvent;
         for( c = 0; c < index && lpEvent; ++c )
            lpEvent = lpEvent->lpNext;
         if( lpEvent )
            *lpeli = lpEvent->elItem;
         return( lpEvent ? c : -1 );

      case GNE_BYTYPE:
         lpEvent = lpFirstEvent;
         for( c = 0; c < index && lpEvent; ++c )
            lpEvent = lpEvent->lpNext;
         for( ; lpEvent; ++c )
            if( lpEvent->elItem.wType & lpeli->wType )
               {
               *lpeli = lpEvent->elItem;
               break;
               }
         return( lpEvent ? c : -1 );
      }
   return( -1 );
}

/* This function creates a new event log item from the specified
 * event structure.
 */
BOOL FASTCALL CreateEventLogItem( EVENTLOGITEM * pelItem, BOOL fNotify )
{
   LPEVENT lpDeletedEvent;
   LPEVENT lpEvent;

   INITIALISE_PTR(lpEvent);
   INITIALISE_PTR(lpDeletedEvent);
   if( fNewMemory( &lpEvent, sizeof( EVENT ) ) )
      {
      /* We've reached the maximum size of the log, so delete the
       * last event. We don't actually delete it now because doing so
       * would destablise any addon which stored the event handle. So
       * we delete it AFTER we've sent AE_EVENTLOG_CHANGED.
       */
      if( ++wLineCount > wMaxEventLogLines )
         {
         LPEVENT lpLastEvent2;

         lpLastEvent2 = lpLastEvent->lpPrev;
         lpDeletedEvent = lpLastEvent;
         lpLastEvent = lpLastEvent2;
         lpLastEvent->lpNext = NULL;
         --wLineCount;
         }

      /* Link in the new event.
       */
      lpEvent->lpPrev = NULL;
      lpEvent->lpNext = lpFirstEvent;
      if( lpEvent->lpNext == NULL )
         lpLastEvent = lpEvent;
      if( lpFirstEvent )
         lpFirstEvent->lpPrev = lpEvent;
      lpFirstEvent = lpEvent;
      lpEvent->elItem = *pelItem;

      /* All done. Send event notification.
       */
      if( fNotify )
         Amuser_CallRegistered( AE_EVENTLOG_CHANGED, (LPARAM)lpFirstEvent, (LPARAM)lpDeletedEvent );

      /* If we deleted an event, delete it now.
       */
      if( NULL != lpDeletedEvent )
         FreeMemory( &lpDeletedEvent );
      return( TRUE );
      }
   return( FALSE );
}

/* This function loads the event log from disk.
 */
BOOL FASTCALL LoadEventLog( void )
{
   HNDFILE fh;
   BOOL fOk = FALSE;

   /* Read config info.
    */
   wLineCount = 0;
   wMaxEventLogLines = Amuser_GetPPInt( szEventLog, "EventLogMax", DEF_MAXEVENTLOGLINES );
   wPermittedEvents = Amuser_GetPPInt( szEventLog, "EventLogAllowed", DEF_PERMITTEDEVENTS );

   /* Construct the path to the event log file.
    */
   Amuser_GetActiveUserDirectory( szEventLogFile, _MAX_PATH );
   strcat( strcat( szEventLogFile, "\\" ), EVENT_LOG_FILE );
   if( ( fh = Amfile_Open( szEventLogFile, AOF_READ ) ) != HNDFILE_ERROR )
      {
      EVENTLOGITEM elItem;

      while( Amfile_Read( fh, &elItem, sizeof( EVENTLOGITEM ) ) == sizeof( EVENTLOGITEM ) )
         CreateEventLogItem( &elItem, FALSE );
      Amfile_Close( fh );
      fOk = TRUE;
      }
   fEventLogLoaded = TRUE; /* Even if it didn't exist... */
   return( fOk );
}

/* This function saves the event log to disk.
 */
BOOL FASTCALL SaveEventLog( void )
{
   HNDFILE fh;
   BOOL fOk = FALSE;

   Amuser_GetActiveUserDirectory( szEventLogFile, _MAX_PATH );
   strcat( strcat( szEventLogFile, "\\" ), EVENT_LOG_FILE );
   if( lpFirstEvent == NULL )
      {
      Amfile_Delete( szEventLogFile );
      fOk = TRUE;
      }
   else 
      if( ( fh = Amfile_Create( szEventLogFile, 0 ) ) != HNDFILE_ERROR )
         {
         LPEVENT lpEvent;
         BOOL fError = FALSE;

         for( lpEvent = lpLastEvent; !fError && lpEvent; lpEvent = lpEvent->lpPrev )
            if( Amfile_Write( fh, (LPCSTR)&lpEvent->elItem, sizeof( EVENTLOGITEM ) ) != sizeof( EVENTLOGITEM ) )
               fError = TRUE;
         Amfile_Close( fh );
         fOk = !fError;
         }
   return( fOk );
}
