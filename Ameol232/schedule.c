/* SCHEDULE.C - Implements the Scheduler
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
#include "resource.h"
#include "palrc.h"
#include "amlib.h"
#include "amevent.h"
#include <ctype.h>
#include "command.h"
#include "ini.h"
#include "rules.h"
#include "cix.h"
#include "admin.h"
#include <string.h>
#include "ameol2.h"

#define  THIS_FILE   __FILE__

#define  MWE_FOCUS         (DLGWINDOWEXTRA)
#define  MWE_EXTRA         (DLGWINDOWEXTRA+4)

char NEAR szSchedulerWndClass[] = "amctl_scheduler";
char NEAR szSchedulerWndName[] = "Scheduler";

static BOOL fDefDlgEx = FALSE;            /* DefDlg recursion flag trap */
static BOOL fRegistered = FALSE;          /* TRUE if we've registered the out-basket window class */
static HBITMAP hbmpSchd;                  /* Out-basket bitmaps */
static int hdrColumns[ 4 ];               /* Header columns */
static int nItemID = 1;                   /* Item ID */
static WNDPROC lpProcListCtl;             /* Subclassed listbox window procedure */
static HBRUSH hbrSchedulerWnd = NULL;     /* Handle of scheduler window brush */
static BOOL fQuickUpdate;                 /* TRUE if we don't paint background of listbox */

HWND hwndScheduler = NULL;                /* Scheduler window handle */

char * NameOfMonth[] = { "January", "February", "March", "April", "May", "June",
                       "July", "August", "September", "October", "November", "December" };
char * DayOfWeek[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
char * NameOfDay[] = { "First", "Second", "Third", "Fourth", "Fifth", "Sixth", "Seventh", "Eighth",
                       "Ninth", "Tenth", "Eleventh", "Twelfth", "Thirteenth", "Fourteenth",
                       "Fifteenth", "Sixteenth", "Seventeenth", "Eighteenth", "Nineteenth", "Twentieth",
                       "Twenty-first", "Twenty-second", "Twenty-third", "Twenty-fourth", "Twenty-fifth",
                       "Twenty-sixth", "Twenty-seventh", "Twenty-eighth", "Twenty-ninth", "Thirtieth",
                       "Thirty-first" };

/* List of controls that make up the schedule settings.
 * DON'T change the order!
 */
int nSchdCtrls[] = {
   IDD_TIME,            /* Time of the day edit field */
   IDD_PAD1,            /* Label */
   IDD_PAD2,            /* Label */
   IDD_MINUTES,         /* Minutes edit field */
   IDD_PAD3,            /* Label */
   IDD_PAD4,            /* Label */
   IDD_PAD5,            /* Label */
   IDD_DAYCOMBO2,       /* Day combo box */
   IDD_PAD6,            /* Label */
   IDD_PAD7,            /* Label */
   IDD_YEARCOMBO,       /* Combo box listing years (2011..2020) */
   IDD_MONTHCOMBO,      /* Combo box listing months (January..December) */
   IDD_DOWCOMBO,        /* Combo box listing days of the week */
   IDD_DAYCOMBO,        /* Combo box listing days (1..31) */
   IDD_DAYS,            /* Day edit field */
   IDD_PAD8             /* Label */
   };
#define cSchdCtrls (sizeof(nSchdCtrls)/sizeof(int))

/* For each type of scheduling, there is a list of controls that
 * are required to change the settings.
 */
typedef struct tagSCHDCTRLARRAY {
   int id;                          /* Radio control corresponding to action */
   int schType;                     /* Type of scheduled action */
   BOOL fCtlEnable[cSchdCtrls];     /* TRUE/FALSE array for each control */
} SCHDCTRLARRAY;  

SCHDCTRLARRAY schCtrlArray[] = {
   IDD_ONCE,      SCHTYPE_ONCE,     { TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, FALSE, TRUE, FALSE, FALSE },
   IDD_STARTUP,   SCHTYPE_STARTUP,  { FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE },
   IDD_EVERY,     SCHTYPE_EVERY,    { FALSE, FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE },
   IDD_HOURLY,    SCHTYPE_HOURLY,   { FALSE, FALSE, TRUE, TRUE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE },
   IDD_DAILY,     SCHTYPE_DAILY,    { TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE },
   IDD_WEEKLY,    SCHTYPE_WEEKLY,   { TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE },
   IDD_MONTHLY,   SCHTYPE_MONTHLY,  { TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE },
   IDD_DAYPERIOD, SCHTYPE_DAYPERIOD,{ FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, TRUE }
   };
#define cSchdCtrlArray (sizeof(schCtrlArray)/sizeof(int))

typedef struct tagSCHDITEM {
   struct tagSCHDITEM FAR * lpschdNext;      /* Pointer to next item */
   struct tagSCHDITEM FAR * lpschdPrev;      /* Pointer to previous item */
   int nItemID;                              /* Item ID */
   HCMD hCmd;                                /* Command handle */
   SCHEDULE sch;                             /* Schedule information */
   int cEfforts;                             /* Number of tries a failed run was retried */
   AM_DATE schNextDate;                      /* Date of next run */
   AM_TIME schNextTime;                      /* Time of next run */
   AM_DATE schLastDate;                      /* Date of last run */
   AM_TIME schLastTime;                      /* Time of last run */
   BOOL fRunOnce;                            /* TRUE if run once (schLastDate/Time are valid) */
   BOOL fActive;                             /* TRUE if action is active */
   BOOL fEnabled;                            /* TRUE if action is enabled */
   char * pszFolder;                         /* Path of folder */
} SCHDITEM, FAR * LPSCHDITEM;

LPSCHDITEM lpschdFirst = NULL;      /* First entry in the scheduler list */
LPSCHDITEM lpschdLast = NULL;       /* Last entry in the scheduler list */

typedef void (FASTCALL * SCHDINITFUNC)( HWND, LPSCHDITEM );

void FASTCALL FillDayComboBox( HWND, int );

void FASTCALL SetTimeField( HWND, LPSCHDITEM );
void FASTCALL SetDayField( HWND, LPSCHDITEM );
void FASTCALL SetMonthField( HWND, LPSCHDITEM );
void FASTCALL SetYearField( HWND, LPSCHDITEM );
void FASTCALL SetDayField2( HWND, LPSCHDITEM );
void FASTCALL SetDayField3( HWND, LPSCHDITEM );
void FASTCALL SetMinutesField( HWND, LPSCHDITEM );
void FASTCALL SetDayOfWeekField( HWND, LPSCHDITEM );

void FASTCALL GetDateField( HWND, AM_DATE * );
int FASTCALL GetDayField2( HWND );
BOOL FASTCALL GetDayField3( HWND, WORD * );
int FASTCALL GetMinutesField( HWND, WORD * );
int FASTCALL GetDayOfWeekField( HWND );

/* For each control above, a list of functions that initialise
 * the control.
 */
SCHDINITFUNC lpfCtlInit[] = {
   SetTimeField,
   NULL,
   NULL,
   SetMinutesField,
   NULL,
   NULL,
   NULL,
   SetDayField2,
   NULL,
   NULL,
   SetYearField,
   SetMonthField,
   SetDayOfWeekField,
   SetDayField,
   SetDayField3,
   NULL
   };

LRESULT EXPORT CALLBACK SchedulerWndProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL SchedulerDlg_OnCreate( HWND, LPCREATESTRUCT );
void FASTCALL SchedulerDlg_OnClose( HWND );
void FASTCALL SchedulerDlg_OnCommand( HWND, int, HWND, UINT );
void FASTCALL SchedulerDlg_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
void FASTCALL SchedulerDlg_OnSetFocus( HWND, HWND );
void FASTCALL SchedulerDlg_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
void FASTCALL SchedulerDlg_OnSize( HWND, UINT, int, int );
void FASTCALL SchedulerDlg_OnMove( HWND, int, int );
HBRUSH FASTCALL SchedulerDlg_OnCtlColor( HWND, HDC, HWND, int );
void FASTCALL SchedulerDlg_MDIActivate( HWND, BOOL, HWND, HWND );
void FASTCALL SchedulerDlg_OnAdjustWindows( HWND, int, int );
BOOL FASTCALL SchedulerDlg_OnEraseBkgnd( HWND, HDC );
LRESULT FASTCALL ScheduleDlg_OnNotify( HWND, int, LPNMHDR );

BOOL EXPORT CALLBACK EditSchedule( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL EditSchedule_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL EditSchedule_OnCommand( HWND, int, HWND, UINT );
BOOL FASTCALL EditSchedule_OnInitDialog( HWND, HWND, LPARAM );

BOOL EXPORT CALLBACK MissedScheduleDlg( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL MissedScheduleDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL MissedScheduleDlg_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL MissedScheduleDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );

LRESULT EXPORT FAR PASCAL SchedulerListProc( HWND, UINT, WPARAM, LPARAM );

#define  CPDATE_EARLIER    1
#define  CPDATE_EQUAL      2
#define  CPDATE_LATER      4

void FASTCALL LocateMissedItems( void );
void FASTCALL SetEnableDisable( HWND );
BOOL FASTCALL OkayToRunScheduled( void );
LPSTR FASTCALL SchShortDate( AM_DATE FAR * );
LPSTR FASTCALL SchShortTime( AM_TIME FAR * );
LPSCHDITEM FASTCALL SchdInstall( char *, SCHEDULE *, char * );
BOOL FASTCALL IsTimeEarlier( AM_TIME * );
BOOL FASTCALL CompareDate( AM_DATE *, int );
void FASTCALL SchdRemove( SCHDITEM FAR * );
void FASTCALL SaveScheduledItems( void );
BOOL FASTCALL SaveOneScheduledItem( LPSCHDITEM );
void FASTCALL UpdateSchedulerWindow( LPSCHDITEM );
void FASTCALL ScheduleFirstAction( SCHDITEM * );
void FASTCALL RescheduleNextAction( SCHDITEM * );
void FASTCALL ShowScheduleSettings( HWND, LPSCHDITEM );
void FASTCALL ShowScheduledCommandSettings( HWND, HCMD );
void FASTCALL RunScheduledCommand( LPSCHDITEM );
BOOL FASTCALL CanRunScheduledCommand( LPSCHDITEM );
void FASTCALL WriteScheduledCommandName( LPSCHDITEM, char * );

/* This function reads the scheduled items into a locally
 * linked list.
 */
void FASTCALL LoadScheduledItems( void )
{
   LPSTR lpsz;

   INITIALISE_PTR(lpsz);

   /* Get the list of scheduled items and
    * install them.
    */
   if( fNewMemory( &lpsz, 8000 ) )
      {
      UINT c;

      Amuser_GetPPString( szScheduler, NULL, "", lpsz, 8000 );
      for( c = 0; lpsz[ c ]; ++c )
         {
         BOOL fHasLastTimeDate;
         char szCmdName[ 60 ];
         char szFolder[ 192 ];
         AM_DATE schLastDate;
         AM_TIME schLastTime;
         AM_DATE schNextDate;
         AM_TIME schNextTime;
         LPSCHDITEM lpschd;
         BOOL fEnabled;
         BOOL fActive;
         char * pszFolder;
         SCHEDULE sch;
         LPSTR lp;

         /* Read one scheduled item
          */
         Amuser_GetPPString( szScheduler, &lpsz[ c ], "", lpTmpBuf, LEN_TEMPBUF );
         lp = lpTmpBuf;
         fHasLastTimeDate = FALSE;
         fEnabled = TRUE;
         fActive = TRUE;

         /* Read the command name.
          */
         lp = IniReadText( lp, szCmdName, sizeof(szCmdName) - 1 );

         /* Initialise the structure.
          */
         Amdate_GetCurrentDate( &sch.schDate );
         Amdate_GetCurrentTime( &sch.schTime );
         sch.reserved = 0;
         sch.dwSize = sizeof(SCHEDULE);

         /* Read the type of event.
          */
         switch( *lp++ )
            {
            case '\0':  --lp;                            break;
            case 'O':   sch.schType = SCHTYPE_ONCE;      break;
            case 'W':   sch.schType = SCHTYPE_WEEKLY;    break;
            case 'S':   sch.schType = SCHTYPE_STARTUP;   break;
            case 'E':   sch.schType = SCHTYPE_EVERY;     break;
            case 'H':   sch.schType = SCHTYPE_HOURLY;    break;
            case 'M':   sch.schType = SCHTYPE_MONTHLY;   break;
            case 'D':   sch.schType = SCHTYPE_DAILY;     break;
            case 'P':   sch.schType = SCHTYPE_DAYPERIOD; break;
            }

         /* Read the parameters to the event.
          */
         if( *lp )
            switch( sch.schType )
               {
               case SCHTYPE_STARTUP:
                  if( *lp == ',' )
                     ++lp;
                  break;

               case SCHTYPE_ONCE:
                  lp = IniReadDate( lp, &sch.schDate );
                  lp = IniReadTime( lp, &sch.schTime );
                  break;

               case SCHTYPE_WEEKLY: {
                  int iDayOfWeek;

                  lp = IniReadTime( lp, &sch.schTime );
                  lp = IniReadInt( lp, &iDayOfWeek );
                  sch.schDate.iDayOfWeek = iDayOfWeek;
                  break;
                  }

               case SCHTYPE_EVERY:
               case SCHTYPE_HOURLY: {
                  int iMinute;

                  lp = IniReadInt( lp, &iMinute );
                  sch.schTime.iMinute = iMinute;
                  break;
                  }

               case SCHTYPE_MONTHLY: {
                  int iDay;

                  lp = IniReadTime( lp, &sch.schTime );
                  lp = IniReadInt( lp, &iDay );
                  sch.schDate.iDay = iDay;
                  break;
                  }

               case SCHTYPE_DAYPERIOD: {
                  int iDayPeriod;

                  lp = IniReadInt( lp, &iDayPeriod );
                  sch.schDate.iDay = iDayPeriod;
                  break;
                  }

               case SCHTYPE_DAILY:
                  lp = IniReadTime( lp, &sch.schTime );
                  break;
               }

         /* Read folder name.
          */
         pszFolder = NULL;
         lp = IniReadText( lp, szFolder, sizeof(szFolder) );
         if( *szFolder )
            pszFolder = szFolder;

         /* Read the next scheduled time/date
          */
         lp = IniReadDate( lp, &schNextDate );
         lp = IniReadTime( lp, &schNextTime );

         /* All fields may have a last run date and time.
          */
         if( *lp != ',' )
            fHasLastTimeDate = TRUE;
         lp = IniReadDate( lp, &schLastDate );
         lp = IniReadTime( lp, &schLastTime );

         /* Get the enable and active flag.
          */
         lp = IniReadInt( lp, &fEnabled );
         lp = IniReadInt( lp, &fActive );

         /* Hack to change non-existent, obsolete FileBlink command
          * to Full.
          */
         if( _strcmpi( szCmdName, "FileBlink" ) == 0 )
            strcpy( szCmdName, "Full" );

         /* Install into the scheduler array.
          */
         lpschd = SchdInstall( szCmdName, &sch, pszFolder );
         if( NULL != lpschd )
            {
            /* Store the date/time of the last run, if
             * we have one.
             */
            if( fHasLastTimeDate )
               {
               lpschd->schLastTime = schLastTime;
               lpschd->schLastDate = schLastDate;
               lpschd->fRunOnce = TRUE;
               }

            /* Set the next run date/time.
             */
            lpschd->schNextTime = schNextTime;
            lpschd->schNextDate = schNextDate;

            /* Set the enable and active flags.
             */
            lpschd->fEnabled = fEnabled;
            lpschd->fActive = fActive;

            /* Set this item ID
             */
            lpschd->nItemID = atoi( &lpsz[ c ] + 2 );
            if( lpschd->nItemID >= nItemID )
               nItemID = lpschd->nItemID + 1;
            }
         else
            Amuser_WritePPString( szScheduler, &lpsz[ c ], NULL );
         c += strlen( &lpsz[ c ] );
         }
      FreeMemory( &lpsz );
      }

   /* If this is the first run, create default schedule entries.
    */
   if( NULL == lpschdFirst && ( fFirstTimeThisUser || fFirstRun ) )
      {
      SCHEDULE sch;

      /* Set default clear all mail entries.
       */
      memset( &sch, 0, sizeof(SCHEDULE) );
      sch.dwSize = sizeof(SCHEDULE);
      sch.schType = SCHTYPE_DAYPERIOD;
      Amdate_GetCurrentDate( &sch.schDate );
      Amdate_GetCurrentTime( &sch.schTime );
      sch.schDate.iDay = 1;
      sch.reserved = 0;
      if( fUseInternet )
         SchdInstall( "IPClearMail", &sch, NULL );
      if (fUseLegacyCIX)
         SchdInstall( "CixClearMail", &sch, NULL );
      }
}

/* This function saves the settings for one scheduled
 * item.
 */
BOOL FASTCALL SaveOneScheduledItem( LPSCHDITEM lpschd )
{
   char szEntry[ 10 ];
   int p;

   p = wsprintf( lpTmpBuf, "%s,", CTree_GetCommandName( lpschd->hCmd ) );
   switch( lpschd->sch.schType )
      {
      case SCHTYPE_STARTUP:
         p += wsprintf( lpTmpBuf + p, "S" );
         break;

      case SCHTYPE_ONCE:
         p += wsprintf( lpTmpBuf + p, "O%s,%s",
                        SchShortDate( &lpschd->sch.schDate ),
                        SchShortTime( &lpschd->sch.schTime ) );
         break;

      case SCHTYPE_WEEKLY:
         p += wsprintf( lpTmpBuf + p, "W%s,%u",
                        SchShortTime( &lpschd->sch.schTime ),
                        lpschd->sch.schDate.iDayOfWeek );
         break;

      case SCHTYPE_HOURLY:
         p += wsprintf( lpTmpBuf + p, "H%u", lpschd->sch.schTime.iMinute );
         break;

      case SCHTYPE_DAILY:
         p += wsprintf( lpTmpBuf + p, "D%s", SchShortTime( &lpschd->sch.schTime ) );
         break;

      case SCHTYPE_DAYPERIOD:
         p += wsprintf( lpTmpBuf + p, "P%u", lpschd->sch.schDate.iDay );
         break;

      case SCHTYPE_EVERY:
         p += wsprintf( lpTmpBuf + p, "E%u", lpschd->sch.schTime.iMinute );
         break;

      case SCHTYPE_MONTHLY:
         p += wsprintf( lpTmpBuf + p, "M%s,%u",
                        SchShortTime( &lpschd->sch.schTime ),
                        lpschd->sch.schDate.iDay );
         break;
      }

   /* Append folder name.
    */
   p += wsprintf( lpTmpBuf + p, ",%s", lpschd->pszFolder ? lpschd->pszFolder : "" );

   /* Save the date/time when this item will next run.
    */
   p += wsprintf( lpTmpBuf + p, ",%s", SchShortDate( &lpschd->schNextDate ) );
   p += wsprintf( lpTmpBuf + p, ",%s", SchShortTime( &lpschd->schNextTime ) );

   /* If this action has been run at least once, save the
    * last run date/time.
    */
   p += wsprintf( lpTmpBuf + p, "," );
   if( lpschd->fRunOnce )
      p += wsprintf( lpTmpBuf + p, "%s", SchShortDate( &lpschd->schLastDate ) );
   p += wsprintf( lpTmpBuf + p, "," );
   if( lpschd->fRunOnce )
      p += wsprintf( lpTmpBuf + p, "%s", SchShortTime( &lpschd->schLastTime ) );

   /* Add the enabled/disabled flag.
    */
   p += wsprintf( lpTmpBuf + p, ",%u", lpschd->fEnabled );
   p += wsprintf( lpTmpBuf + p, ",%u", lpschd->fActive );

   if( strncmp( lpTmpBuf, "FileExit,S", 10 ) == 0 )
   {
      fMessageBox( hwndFrame, 0, "Not a good idea...", MB_OK|MB_ICONINFORMATION );
      return( FALSE );
   }
   else
   {
   /* Write this entry.
    */
   wsprintf( szEntry, "SC%3.3u", lpschd->nItemID );
   Amuser_WritePPString( szScheduler, szEntry, lpTmpBuf );
   }

   return( TRUE );
   
}

/* Format a short date to a string.
 */
LPSTR FASTCALL SchShortDate( AM_DATE FAR * lpdate )
{
   static char szDate[ 40 ];

   wsprintf( szDate, "%u/%2.2u/%u", lpdate->iDay, lpdate->iMonth, lpdate->iYear );
   return( szDate );
}

/* Format a short time to a string.
 */
LPSTR FASTCALL SchShortTime( AM_TIME FAR * lptime )
{
   static char szTime[ 40 ];

   wsprintf( szTime, "%u:%2.2u", lptime->iHour, lptime->iMinute );
   return( szTime );
}

/* This function installs a new command into the scheduler
 * table.
 */
SCHDITEM FAR * FASTCALL SchdInstall( char * pszCmdName, SCHEDULE * psch, char * pszFolder )
{
   SCHDITEM FAR * lpschd;

   INITIALISE_PTR(lpschd);
   if( fNewMemory( &lpschd, sizeof(SCHDITEM) ) )
      {
      /* If folder specified, store folder
       * name.
       */
      lpschd->pszFolder = NULL;
      if( NULL != pszFolder )
         {
         if( !fNewMemory( &lpschd->pszFolder, strlen(pszFolder) + 1 ) )
            {
            FreeMemory( &lpschd );
            return( NULL );
            }
         strcpy( lpschd->pszFolder, pszFolder );
         }

      /* Fill out the structure and link it.
       */
      lpschd->hCmd = CTree_CommandToHandle( pszCmdName );
      if( NULL == lpschd->hCmd )
         {
         HideIntroduction();
         wsprintf( lpTmpBuf, GS(IDS_STR1133), pszCmdName );
         fMessageBox( hwndFrame, 0, lpTmpBuf, MB_OK|MB_ICONINFORMATION );
         FreeMemory( &lpschd );
         }
      else
         {
         lpschd->nItemID = nItemID++;
         lpschd->sch = *psch;
         lpschd->fActive = TRUE;
         lpschd->fEnabled = TRUE;
         lpschd->fRunOnce = FALSE;
         if( NULL == lpschdFirst )
            lpschdFirst = lpschd;
         else
            lpschdLast->lpschdNext = lpschd;
         lpschd->lpschdNext = NULL;
         lpschd->lpschdPrev = lpschdLast;
         lpschdLast = lpschd;

         /* Calculate the date/time when this action is
          * to be run.
          */
         ScheduleFirstAction( lpschd );
         }
      }
   return( lpschd );
}

/* This function deletes the specified scheduled item from the list.
 */
void FASTCALL SchdRemove( SCHDITEM FAR * lpschd )
{
   char szEntry[ 10 ];

   /* Remove this entry from the configuration file.
    */
   wsprintf( szEntry, "SC%3.3u", lpschd->nItemID );
   Amuser_WritePPString( szScheduler, szEntry, NULL );

   /* Free folder name.
    */
   if( NULL != lpschd->pszFolder )
      FreeMemory( &lpschd->pszFolder );

   /* Unlink it from the list then delete it.
    */
   if( NULL == lpschd->lpschdNext )
      lpschdLast = lpschd->lpschdPrev;
   else
      lpschd->lpschdNext->lpschdPrev = lpschd->lpschdPrev;
   if( NULL == lpschd->lpschdPrev )
      lpschdFirst = lpschd->lpschdNext;
   else
      lpschd->lpschdPrev->lpschdNext = lpschd->lpschdNext;

   /* Delete from the Scheduler list box.
    */
   if( NULL != hwndScheduler )
      {
      HWND hwndList;
      int index;
      int count;

      /* Locate item in the list box.
       */
      VERIFY( hwndList = GetDlgItem( hwndScheduler, IDD_SCHLIST ) );
      index = RealListBox_FindItemData( hwndList, -1, (LPARAM)lpschd );
      count = ListBox_DeleteString( hwndList, index );
      if( index == count )
         --index;

      /* Disable Remove and Edit if no items left.
       */
      if( 0 == count )
         {
         EnableControl( hwndScheduler, IDD_REMOVE, FALSE );
         EnableControl( hwndScheduler, IDD_DISABLE, FALSE );
         EnableControl( hwndScheduler, IDD_EDIT, FALSE );
         EnableControl( hwndScheduler, IDD_RUNNOW, FALSE );
         }
      }
   FreeMemory( &lpschd );
}

/* This function loops for each item in the scheduler and
 * checks to see if anything has been 'missed'. For example,
 * if an item was scheduled to be executed before the current
 * time, it should have been considered to have been missed.
 */
void FASTCALL LocateMissedItems( void )
{
   BOOL fRescheduleAll;
   LPSCHDITEM lpschd;
   BOOL fPrompt;

   fRescheduleAll = FALSE;
   fPrompt = !Amuser_GetPPInt( szSettings, "AutoScheduleMissed", FALSE );
   for( lpschd = lpschdFirst; lpschd; lpschd = lpschd->lpschdNext )
      if( lpschd->fActive && lpschd->fEnabled )
         {
         if( fRescheduleAll )
         {
            if (lpschd->sch.schType == SCHTYPE_ONCE )
               RescheduleNextAction( lpschd );
            else
               ScheduleFirstAction( lpschd );
            if( NULL != hwndScheduler )
               UpdateSchedulerWindow( lpschd );
            SaveOneScheduledItem( lpschd );

         }
         else if( CompareDate( &lpschd->schNextDate, CPDATE_EARLIER ) ||
                  CompareDate( &lpschd->schNextDate, CPDATE_EQUAL ) &&
                  IsTimeEarlier( &lpschd->schNextTime ) )
            {
            /* Prompt if so required.
             */
            if( fPrompt && ( NULL == strstr( CTree_GetCommandName( lpschd->hCmd ), "ClearMail" ) ) )
               {
               int r;

               /* Ask user if he/she wants to action this item now.
                * If so, reschedule to be carried out immediately.
                */
               HideIntroduction();
               r = Adm_Dialog( hRscLib, hwndFrame, MAKEINTRESOURCE(IDDLG_MISSEDSCHEDULE), MissedScheduleDlg, (LPARAM)lpschd );
               if( IDNO == r )
                  {
                  ScheduleFirstAction( lpschd );
                  if( NULL != hwndScheduler )
                     UpdateSchedulerWindow( lpschd );
                  SaveOneScheduledItem( lpschd );
                  continue;
                  }
               if( IDNOALL == r )
                  {
                  ScheduleFirstAction( lpschd );
                  fRescheduleAll = TRUE;
                  if( NULL != hwndScheduler )
                     UpdateSchedulerWindow( lpschd );
                  SaveOneScheduledItem( lpschd );
                  continue;
                  }
               fPrompt = !Amuser_GetPPInt( szSettings, "AutoScheduleMissed", FALSE );
               }

            /* Okay, have this item execute immediately.
             */
            RunScheduledCommand( lpschd );

            /* Reschedule for next run.
             */
            if (lpschd->sch.schType == SCHTYPE_ONCE )
               RescheduleNextAction( lpschd );
            else
               ScheduleFirstAction( lpschd );
            if( NULL != hwndScheduler )
               UpdateSchedulerWindow( lpschd );
            SaveOneScheduledItem( lpschd );
            }
         }
}

/* Scan the list of scheduled items and find one to be scheduled
 * to be run.
 */
void FASTCALL InvokeScheduler( void )
{
   SCHDITEM FAR * lpschd;
   AM_DATE schCurDate;
   AM_TIME schCurTime;

   /* Get the current date and time.
    */
   Amdate_GetCurrentDate( &schCurDate );
   Amdate_GetCurrentTime( &schCurTime );

   /* Now loop, looking for an action whose scheduled date and time
    * is prior to the current date and time.
    */
   for( lpschd = lpschdFirst; lpschd; lpschd = lpschd->lpschdNext )
      if( lpschd->fActive && lpschd->fEnabled )
         {
         if( schCurDate.iDay >= lpschd->schNextDate.iDay &&
             schCurDate.iMonth >= lpschd->schNextDate.iMonth &&
             schCurDate.iYear >= lpschd->schNextDate.iYear &&
             schCurTime.iHour >= lpschd->schNextTime.iHour &&
             schCurTime.iMinute >= lpschd->schNextTime.iMinute )
            {
            if( CanRunScheduledCommand( lpschd ) )
               {
               /* Run this command.
                */
               RunScheduledCommand( lpschd );

               /* Reschedule for next run.
                */
//             ScheduleFirstAction( lpschd );
               RescheduleNextAction( lpschd );
               if( NULL != hwndScheduler )
                  UpdateSchedulerWindow( lpschd );
               SaveOneScheduledItem( lpschd );
               }
            }
         }
}

/* This function returns scheduler information for the
 * specified command.
 */
int EXPORT WINAPI Ameol2_GetSchedulerInfo( char * pszCmdName, SCHEDULE * pschd )
{
   SCHDITEM FAR * lpschd;
   HCMD hCmd;

   hCmd = CTree_CommandToHandle( pszCmdName );
   for( lpschd = lpschdFirst; lpschd; lpschd = lpschd->lpschdNext )
      if( lpschd->hCmd == hCmd )
         {
         *pschd = lpschd->sch;
         return( SCHDERR_OK );
         }
   return( SCHDERR_NOSUCHCOMMAND );
}

/* This function displays the Edit dialog for the specified
 * scheduled command.
 */
int EXPORT WINAPI Ameol2_EditSchedulerInfo( HWND hwnd, char * pszCmdName )
{
   SCHDITEM FAR * lpschd;
   HCMD hCmd;

   /* First look for the entry in the table
    */
   hCmd = CTree_CommandToHandle( pszCmdName );
   for( lpschd = lpschdFirst; lpschd; lpschd = lpschd->lpschdNext )
      if( lpschd->hCmd == hCmd )
         {
         if( Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_SCHEDEDIT), EditSchedule, (LPARAM)lpschd ) )
            {
            /* Item has changed, so reschedule it and update
             * the list box.
             */
            ScheduleFirstAction( lpschd );
            if( NULL != hwndScheduler )
               UpdateSchedulerWindow( lpschd );
            SaveOneScheduledItem( lpschd );
            return( SCHDERR_OK );
            }
         return( SCHDERR_ABORTED );
         }
   return( SCHDERR_NOSUCHCOMMAND );
}

/* This function adds, modifies or deletes an entry from the
 * scheduler.
 */
int EXPORT WINAPI Ameol2_SetSchedulerInfo( char * pszCmdName, SCHEDULE * pschd )
{
   SCHDITEM FAR * lpschd;
   HCMD hCmd;

   /* First look for the entry in the table
    */
   hCmd = CTree_CommandToHandle( pszCmdName );
   for( lpschd = lpschdFirst; lpschd; lpschd = lpschd->lpschdNext )
      if( lpschd->hCmd == hCmd )
         {
         /* If pschd is NULL, we delete the entry. Otherwise
          * we update it.
          */
         if( NULL == pschd )
            SchdRemove( lpschd );
         else
            {
            lpschd->sch = *pschd;
            ScheduleFirstAction( lpschd );
            if( NULL != hwndScheduler )
               UpdateSchedulerWindow( lpschd );
            SaveOneScheduledItem( lpschd );
            }
         return( SCHDERR_OK );
         }

   /* Not there, so create it anew.
    */
   if( NULL == pschd )
      return( SCHDERR_OK );
   if( ( lpschd = SchdInstall( pszCmdName, pschd, NULL ) ) != NULL )
      {
      /* Save this.
       */
      SaveOneScheduledItem( lpschd );

      /* Insert the new item into the listbox and select
       * it.
       */
      if( NULL != hwndScheduler )
         {
         HWND hwndList;
         int index;

         VERIFY( hwndList = GetDlgItem( hwndScheduler, IDD_SCHLIST ) );
         index = ListBox_InsertString( hwndList, -1, lpschd );
         ListBox_SetSel( hwndList, FALSE, -1 );
         ListBox_SetSel( hwndList, TRUE, index );

         /* Enable the Edit and Remove buttons.
          */
         EnableControl( hwndScheduler, IDD_REMOVE, TRUE );
         EnableControl( hwndScheduler, IDD_DISABLE, TRUE );
         EnableControl( hwndScheduler, IDD_EDIT, TRUE );
         EnableControl( hwndScheduler, IDD_RUNNOW, TRUE );
         SetEnableDisable( hwndScheduler );
         }
      return( SCHDERR_OK );
      }
   return( SCHDERR_OUTOFMEMORY );
}

/* This function runs a scheduled command.
 */
void FASTCALL RunScheduledCommand( LPSCHDITEM lpschd )
{
   CMDREC msi;

   /* Time to schedule, so fire off the command then
    * reschedule the action for next time.
    */
   CTree_GetItem( lpschd->hCmd, &msi );

   /* Post the command.
    * HACK! Handle Check and Purge this way until we can come up with
    * a proper command interface.
    */
   switch( msi.iCommand )
      {
      case IDM_UPDFILELIST: {
         LPVOID pFolder;

         ParseFolderPathname( lpschd->pszFolder, &pFolder, FALSE, 0 );
         if( NULL != pFolder )
            UpdateFileLists( pFolder, FALSE );
         break;
         }

      case IDM_UPDPARTLIST: {
         LPVOID pFolder;

         ParseFolderPathname( lpschd->pszFolder, &pFolder, FALSE, 0 );
         if( NULL != pFolder )
            UpdatePartLists( pFolder, FALSE );
         break;
         }

      case IDM_UPDCONFNOTES: {
         LPVOID pFolder;

         ParseFolderPathname( lpschd->pszFolder, &pFolder, FALSE, 0 );
         if( NULL != pFolder )
            UpdateConfNotes( pFolder, FALSE );
         break;
         }

      case IDM_CHECK: {
         LPVOID pFolder;

         ParseFolderPathname( lpschd->pszFolder, &pFolder, FALSE, 0 );
         if( NULL != pFolder )
            Amdb_CheckAndRepair( hwndFrame, pFolder, FALSE );
         SetFocus( hwndFrame );
         break;
         }

      case IDM_PURGE: {
         LPVOID pFolder;

         ParseFolderPathname( lpschd->pszFolder, &pFolder, FALSE, 0 );
         if( NULL != pFolder )
            Purge( hwndFrame, pFolder, FALSE );
         SetFocus( hwndFrame );
         break;
         }

      default:
         PostCommand( hwndFrame, msi.iCommand, 0L );
         break;
      }

   /* Record when we were last run and never run
    * this action again.
    */
   Amdate_GetCurrentDate( &lpschd->schLastDate );
   Amdate_GetCurrentTime( &lpschd->schLastTime );
   lpschd->fRunOnce = TRUE;
   lpschd->cEfforts = 0;
   if (lpschd->sch.schType == SCHTYPE_ONCE )
      lpschd->fActive = FALSE;


   /* Report to the event log.
    */
   wsprintf( lpTmpBuf, GS(IDS_STR1024), GINTRSC(msi.lpszTooltipString) );
   Amevent_AddEventLogItem( ETYP_MESSAGE|ETYP_MAINTENANCE, lpTmpBuf );
}

/* This function determines whether Ameol is in an appropriate state
 * to run the scheduled command. The conditions are:
 *
 * 1. There should be no modal dialog displayed.
 * 2. Err...
 *
 * If the conditions are not correct, we reschedule the command to be
 * run in one minute later.
 */
BOOL FASTCALL CanRunScheduledCommand( LPSCHDITEM lpschd )
{
   BOOL fCanRun;

   fCanRun = OkayToRunScheduled();
   if( !fCanRun )
      {
      CMDREC msi;

      /* 5 tries at running? Give up.
       */
      if( ++lpschd->cEfforts == 60 )
         {
         lpschd->cEfforts = 0;
         return( FALSE );
         }

      /* Advance the time forward by one minute.
       */
      if( ++lpschd->schNextTime.iMinute == 60 )
         {
         lpschd->schNextTime.iMinute -= 60;
         if( ++lpschd->schNextTime.iHour == 24 )
            {
            lpschd->schNextTime.iHour = 0;
            Amdate_AdjustDate( &lpschd->schNextDate, 1 );
            }
         }

      /* Report to the event log.
       */
      CTree_GetItem( lpschd->hCmd, &msi );
      wsprintf( lpTmpBuf, GS(IDS_STR1025),
                GINTRSC(msi.lpszTooltipString),
                SchShortTime( &lpschd->schNextTime ) );
      Amevent_AddEventLogItem( ETYP_MESSAGE|ETYP_MAINTENANCE, lpTmpBuf );
      }
   return( fCanRun );
}

/* Determine whether or not to run the scheduled action.
 */
BOOL FASTCALL OkayToRunScheduled( void )
{
   DWORD dwCurTime;

   if( !IsWindowEnabled( hwndFrame ) )
      return( FALSE );
   if( fParsing || fInitiatingBlink || fBlinking )
      return( FALSE );
   dwCurTime = MAKELONG( Amdate_GetPackedCurrentDate(), Amdate_GetPackedCurrentTime() );
   if( dwLastNonIdleTime - dwCurTime < 3000 )
      return( FALSE );
   return( TRUE );
}

/* This function runs all scheduled actions defined to run when
 * Ameol starts.
 */
void FASTCALL RunScheduledStartupActions( void )
{
   SCHDITEM FAR * lpschd;

   LocateMissedItems();
   for( lpschd = lpschdFirst; lpschd; lpschd = lpschd->lpschdNext )
      if( lpschd->sch.schType == SCHTYPE_STARTUP )
         if( lpschd->fActive && lpschd->fEnabled )
            {
            /* Run the command.
             */
            RunScheduledCommand( lpschd );

            /* Save new state of command.
             */
            SaveOneScheduledItem( lpschd );
            lpschd->fActive = FALSE;
            }
}

/* This function computes the date and time when the specified
 * action is to be run next.
 */
void FASTCALL ScheduleFirstAction( SCHDITEM * lpschd )
{
   switch( lpschd->sch.schType )
      {
      case SCHTYPE_DAYPERIOD:
         /* Every x days. Run the first time that Ameol is
          * started on that day, so ignore the time field.
          */
         Amdate_GetCurrentDate( &lpschd->schNextDate );
         Amdate_AdjustDate( &lpschd->schNextDate, lpschd->sch.schDate.iDay );
         break;

      case SCHTYPE_DAILY:
         /* At a specified time every day.
          */
         Amdate_GetCurrentDate( &lpschd->schNextDate );
         if( IsTimeEarlier( &lpschd->sch.schTime ) )
            Amdate_AdjustDate( &lpschd->schNextDate, 1 );
         lpschd->schNextTime = lpschd->sch.schTime;
         break;

      case SCHTYPE_EVERY:
         /* Every xx minutes.
          */
         Amdate_GetCurrentDate( &lpschd->schNextDate );
         Amdate_GetCurrentTime( &lpschd->schNextTime );
         if( ( lpschd->schNextTime.iMinute += lpschd->sch.schTime.iMinute ) >= 60 )
            {
            lpschd->schNextTime.iMinute -= 60;
            if( ++lpschd->schNextTime.iHour == 24 )
               {
               lpschd->schNextTime.iHour = 0;
               Amdate_AdjustDate( &lpschd->schNextDate, 1 );
               }
            }
         break;

      case SCHTYPE_WEEKLY: {
         int diff;

         /* Run this event weekly. If we're after the event day (0=Sunday), then
          * advance into next week.
          */
         Amdate_GetCurrentDate( &lpschd->schNextDate );
         lpschd->schNextTime = lpschd->sch.schTime;
         lpschd->schNextTime.iSecond = 0;
         diff = lpschd->sch.schDate.iDayOfWeek - lpschd->schNextDate.iDayOfWeek;
         if( 0 > diff || ( 0 == diff && IsTimeEarlier( &lpschd->schNextTime ) ) )
            diff += 7;
         Amdate_AdjustDate( &lpschd->schNextDate, diff );
         break;
         }

      case SCHTYPE_MONTHLY:
         /* We run monthly on a certain day. If we've passed that day,
          * advance to the next month.
          */
         Amdate_GetCurrentDate( &lpschd->schNextDate );
         lpschd->schNextTime = lpschd->sch.schTime;
         if( lpschd->schNextDate.iDay > lpschd->sch.schDate.iDay || IsTimeEarlier( &lpschd->schNextTime ) )
            {
            /* Advance to next month. If we're in December, advance
             * to next year.
             */
            if( ++lpschd->schNextDate.iMonth == 13 )
               {
               lpschd->schNextDate.iMonth = 1;
               ++lpschd->schNextDate.iYear;
               }
            }
         lpschd->schNextDate.iDay = lpschd->sch.schDate.iDay;
         break;            

      case SCHTYPE_HOURLY:
         /* Run this action hourly. If minutes_past_hour is earlier
          * than now, schedule it for the next hour.
          */
         Amdate_GetCurrentDate( &lpschd->schNextDate );
         Amdate_GetCurrentTime( &lpschd->schNextTime );
         if( lpschd->schNextTime.iMinute >= lpschd->sch.schTime.iMinute )
            if( ++lpschd->schNextTime.iHour == 24 )
               {
               lpschd->schNextTime.iHour = 0;
               Amdate_AdjustDate( &lpschd->schNextDate, 1 );
               }
         lpschd->schNextTime.iMinute = lpschd->sch.schTime.iMinute;
         lpschd->schNextTime.iSecond = 0;
         break;

      case SCHTYPE_ONCE:
         /* One off schedule.
          */
         lpschd->schNextDate = lpschd->sch.schDate;
         lpschd->schNextTime = lpschd->sch.schTime;
         lpschd->schNextTime.iSecond = 0;
         break;

      }
   lpschd->schNextTime.iSecond = 0;
   lpschd->fActive = TRUE;
}

/* Returns TRUE if the specified time is earlier or the same
 * as the current time.
 */
BOOL FASTCALL IsTimeEarlier( AM_TIME * pTime )
{
   WORD wTime;

   wTime = Amdate_PackTime( pTime );
   return( wTime <= Amdate_GetPackedCurrentTime() );
}

/* Returns TRUE if the specified date is earlier or the same
 * as the current date.
 */
BOOL FASTCALL CompareDate( AM_DATE * pDate, int nCompare )
{
   WORD wDate;

   wDate = Amdate_PackDate( pDate );
   if( nCompare == CPDATE_EARLIER )
      return( wDate < Amdate_GetPackedCurrentDate() );
   if( nCompare == CPDATE_EQUAL )
      return( wDate == Amdate_GetPackedCurrentDate() );
   return( wDate > Amdate_GetPackedCurrentDate() );
}

/* This function computes the date and time when the specified
 * action is to be run next.
 */
void FASTCALL RescheduleNextAction( SCHDITEM * lpschd )
{
   switch( lpschd->sch.schType )
      {
      case SCHTYPE_EVERY:
         /* Every xx minutes.
          */
         Amdate_GetCurrentDate( &lpschd->schNextDate );
         Amdate_GetCurrentTime( &lpschd->schNextTime );
         if( ( lpschd->schNextTime.iMinute += lpschd->sch.schTime.iMinute ) >= 60 )
            {
            lpschd->schNextTime.iMinute -= 60;
            if( ++lpschd->schNextTime.iHour == 24 )
               {
               lpschd->schNextTime.iHour = 0;
               Amdate_AdjustDate( &lpschd->schNextDate, 1 );
               }
            }
         lpschd->schNextTime.iSecond = 0;
         break;

      case SCHTYPE_DAYPERIOD:
         /* Advance by a given number of days.
          */
         Amdate_GetCurrentDate( &lpschd->schNextDate );
         Amdate_AdjustDate( &lpschd->schNextDate, lpschd->sch.schDate.iDay );
         break;

      case SCHTYPE_DAILY:
         /* At a specified time every day.
          */
         Amdate_GetCurrentDate( &lpschd->schNextDate );
         if( IsTimeEarlier( &lpschd->sch.schTime ) )
            Amdate_AdjustDate( &lpschd->schNextDate, 1 );
         lpschd->schNextTime = lpschd->sch.schTime; // 2.56.2055

         break;

      case SCHTYPE_HOURLY:
         /* Advance forward one hour.
          */
         Amdate_GetCurrentDate( &lpschd->schNextDate );
         Amdate_GetCurrentTime( &lpschd->schNextTime );
         if( ++lpschd->schNextTime.iHour == 24 )
            {
            lpschd->schNextTime.iHour = 0;
            Amdate_AdjustDate( &lpschd->schNextDate, 1 );
            }
         lpschd->schNextTime.iMinute = lpschd->sch.schTime.iMinute;
         lpschd->schNextTime.iSecond = 0;
         break;

      case SCHTYPE_MONTHLY:
         /* Advance to next month. If we're in December, advance
          * to next year.
          */
         if( ++lpschd->schNextDate.iMonth == 13 )
            {
            lpschd->schNextDate.iMonth = 1;
            ++lpschd->schNextDate.iYear;
            }
         lpschd->schNextDate.iDay = lpschd->sch.schDate.iDay;
         break;            

      case SCHTYPE_WEEKLY:
         /* Advance to same time next week.
          */
         Amdate_GetCurrentDate( &lpschd->schNextDate );
         Amdate_AdjustDate( &lpschd->schNextDate, 7 );
         break;         

      case SCHTYPE_ONCE:
         /* A once only action has already been actioned for this
          * session, so never run it again.
          */
         lpschd->fActive = FALSE;
         break;
      }
   if( NULL != hwndScheduler )
      UpdateSchedulerWindow( lpschd );
   SaveOneScheduledItem( lpschd );
}

/* This function displays the Scheduler dialog.
 */
BOOL FASTCALL CmdScheduler( HWND hwnd )
{
   if( TestPerm(PF_CANCONFIGURE) )
      {
      DWORD dwState;
      RECT rc;

      /* If scheduler window already open, bring it to the front
       * and display it.
       */
      if( hwndScheduler )
         {
         Adm_MakeMDIWindowActive( hwndScheduler );
         return( TRUE );
         }

      /* Register the out-basket window class if we have
       * not already done so.
       */
      if( !fRegistered )
         {
         WNDCLASS wc;

         wc.style          = CS_HREDRAW | CS_VREDRAW;
         wc.lpfnWndProc    = SchedulerWndProc;
         wc.hIcon          = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_SCHEDULER) );
         wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
         wc.lpszMenuName   = NULL;
         wc.cbWndExtra     = MWE_EXTRA;
         wc.cbClsExtra     = 0;
         wc.hbrBackground  = NULL;
         wc.lpszClassName  = szSchedulerWndClass;
         wc.hInstance      = hInst;
         if( !RegisterClass( &wc ) )
            return( FALSE );
         fRegistered = TRUE;
         }

      /* The default position of the out-basket.
       */
      GetClientRect( hwndMDIClient, &rc );
      InflateRect( &rc, -10, -10 );
      dwState = 0;

      /* Load the out-basket bitmaps.
       */
      hbmpSchd = LoadBitmap( hRscLib, MAKEINTRESOURCE(IDB_OUTBASKETBMPS) );

      /* Get the actual window size, position and state.
       */
      ReadProperWindowState( szSchedulerWndName, &rc, &dwState );

      /* Create the window.
       */
      hwndScheduler = Adm_CreateMDIWindow( szSchedulerWndName, szSchedulerWndClass, hInst, &rc, dwState, 0L );
      if( NULL == hwndScheduler )
         return( FALSE );

      /* Set the initial focus.
       */
      SchedulerDlg_OnSetFocus( hwndScheduler, NULL );
      return( TRUE );
      }
   return( FALSE );
}

/* This function dispatches messages for the Scheduler dialog.
 */
LRESULT EXPORT CALLBACK SchedulerWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, SchedulerDlg_OnCreate );
      HANDLE_MSG( hwnd, WM_CLOSE, SchedulerDlg_OnClose );
      HANDLE_MSG( hwnd, WM_SIZE, SchedulerDlg_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, SchedulerDlg_OnMove );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, SchedulerDlg_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_COMMAND, SchedulerDlg_OnCommand );
      HANDLE_MSG( hwnd, WM_DRAWITEM, SchedulerDlg_OnDrawItem );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, SchedulerDlg_MDIActivate );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, SchedulerDlg_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, SchedulerDlg_OnAdjustWindows );
   #ifdef WIN32
      HANDLE_MSG( hwnd, WM_CTLCOLORLISTBOX, SchedulerDlg_OnCtlColor );
   #else
      HANDLE_MSG( hwnd, WM_CTLCOLOR, SchedulerDlg_OnCtlColor );
   #endif
      HANDLE_MSG( hwnd, WM_NOTIFY, ScheduleDlg_OnNotify );

      case WM_SETFOCUS:
         SetFocus( GetDlgItem( hwnd, IDD_SCHLIST ) );
         return( 0L );

      case WM_CHANGEFONT:
         if( wParam == FONTYP_SCHEDULE || 0xFFFF == wParam )
            {
            MEASUREITEMSTRUCT mis;
            HWND hwndList;

            /* T'user has changed the scheduler window text font.
             */
            hwndList = GetDlgItem( hwnd, IDD_SCHLIST );
            SendMessage( hwnd, WM_MEASUREITEM, IDD_SCHLIST, (LPARAM)(LPSTR)&mis );
            SendMessage( hwndList, LB_SETITEMHEIGHT, 0, MAKELPARAM( mis.itemHeight, 0) );
            SetWindowFont( hwndList, hSchedulerFont, TRUE );
            InvalidateRect( hwndList, NULL, TRUE );
            UpdateWindow( hwndList );
            }
         return( 0L );

      case WM_APPCOLOURCHANGE:
         if( wParam == WIN_SCHEDULE )
            {
            HWND hwndList;

            /* The user has changed the scheduler window or text
             * colour from the Customise dialog.
             */
            if( NULL != hbrSchedulerWnd )
               DeleteBrush( hbrSchedulerWnd );
            hbrSchedulerWnd = CreateSolidBrush( crSchedulerWnd );
            hwndList = GetDlgItem( hwnd, IDD_SCHLIST );
            InvalidateRect( hwndList, NULL, TRUE );
            UpdateWindow( hwndList );
            }
         return( 0L );

      case WM_GETMINMAXINFO: {
         MINMAXINFO FAR * lpMinMaxInfo;
         LRESULT lResult;

         /* Set the minimum tracking size so the client window can never
          * be resized below 24 lines.
          */
         lResult = Adm_DefMDIDlgProc( hwnd, message, wParam, lParam );
         lpMinMaxInfo = (MINMAXINFO FAR *)lParam;
         lpMinMaxInfo->ptMinTrackSize.x = 175;
         lpMinMaxInfo->ptMinTrackSize.y = 250;
         return( lResult );
         }

      default:
         break;
      }
   return( Adm_DefMDIDlgProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_ERASEBKGND message.
 */
BOOL FASTCALL SchedulerDlg_OnEraseBkgnd( HWND hwnd, HDC hdc )
{
   if( fQuickUpdate )
      return( TRUE );
   return( Common_OnEraseBkgnd( hwnd, hdc ) );
}

/* This function handles the WM_CTLCOLOR message.
 */
HBRUSH FASTCALL SchedulerDlg_OnCtlColor( HWND hwnd, HDC hdc, HWND hwndChild, int type )
{
   switch( GetDlgCtrlID( hwndChild ) )
      {
      case IDD_SCHLIST:
         SetBkColor( hdc, crSchedulerWnd );
         SetTextColor( hdc, crSchedulerText );
         return( hbrSchedulerWnd );
      }
#ifdef WIN32
   return( FORWARD_WM_CTLCOLORLISTBOX( hwnd, hdc, hwndChild, Adm_DefMDIDlgProc ) );
#else
   return( FORWARD_WM_CTLCOLOR( hwnd, hdc, hwndChild, type, Adm_DefMDIDlgProc ) );
#endif
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL SchedulerDlg_OnCreate( HWND hwnd, LPCREATESTRUCT lpCreateStruct )
{
   static int aArray[ 4 ] = { 100, 165, 125, 125 };
   LPMDICREATESTRUCT lpMDICreateStruct;
   LPSCHDITEM lpschd;
   HWND hwndList;
   HWND hwndHdr;
   HD_ITEM hdi;
   int count;

   /* Create the dialog window.
    */
   lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
   Adm_MDIDialog( hwnd, MAKEINTRESOURCE(IDDLG_SCHEDULER), lpMDICreateStruct );

   /* Read the current header settings.
    */
   Amuser_GetPPArray( szSettings, "SchedulerColumns", aArray, 4 );

   /* Fill in the header fields.
    */
   hwndHdr = GetDlgItem( hwnd, IDD_SCHLIST );
   hdi.mask = HDI_TEXT|HDI_WIDTH|HDI_FORMAT;
   hdi.cxy = hdrColumns[ 0 ] = aArray[ 0 ];
   hdi.pszText = GS(IDS_STR1026);
   hdi.fmt = HDF_STRING|HDF_LEFT;
   Header_InsertItem( hwndHdr, 0, &hdi );
   
   hdi.cxy = hdrColumns[ 1 ] = aArray[ 1 ];
   hdi.pszText = GS(IDS_STR1027);
   hdi.fmt = HDF_STRING|HDF_LEFT;
   Header_InsertItem( hwndHdr, 1, &hdi );

   hdi.cxy = hdrColumns[ 2 ] = aArray[ 2 ];
   hdi.pszText = GS(IDS_STR1028);
   hdi.fmt = HDF_STRING|HDF_LEFT;
   Header_InsertItem( hwndHdr, 2, &hdi );

   hdi.cxy = hdrColumns[ 3 ] = aArray[ 3 ];
   hdi.pszText = GS(IDS_STR1029);
   hdi.fmt = HDF_STRING|HDF_LEFT;
   Header_InsertItem( hwndHdr, 3, &hdi );
   SendMessage( hwndHdr, WM_SETTEXT, 3 | HBT_SPRING, 0L );
   SetWindowFont( hwndHdr, hHelv8Font, FALSE );

   /* Subclass the listbox so we can handle the right mouse
    * button. NOTE: Must subclass the listbox within the
    * topic list window.
    */
   hwndList = GetDlgItem( hwnd, IDD_SCHLIST );
   fQuickUpdate = FALSE;
   hwndList = GetDlgItem( hwndList, 0x3000 );
   lpProcListCtl = SubclassWindow( hwndList, SchedulerListProc );

   /* Create the scheduler window handle.
    */
   hbrSchedulerWnd = CreateSolidBrush( crSchedulerWnd );

   /* Fill the scheduler dialog.
    */
   hwndList = GetDlgItem( hwnd, IDD_SCHLIST );
   for( lpschd = lpschdFirst; lpschd; lpschd = lpschd->lpschdNext )
      ListBox_InsertString( hwndList, -1, lpschd );

   /* Disable the controls as appropriate.
    */
   count = ListBox_GetCount( hwndList );
   if( count > 0 )
      ListBox_SetSel( hwndList, TRUE, 0 );
   EnableControl( hwnd, IDD_REMOVE, count > 0 );
   EnableControl( hwnd, IDD_EDIT, count > 0 );
   EnableControl( hwnd, IDD_DISABLE, count > 0 );
   EnableControl( hwnd, IDD_RUNNOW, count > 0 );
   CheckDlgButton( hwnd, IDD_AUTORUN, Amuser_GetPPInt( szSettings, "AutoScheduleMissed", FALSE ) );


   /* Set the Disable button as appropriate.
    */
   SetEnableDisable( hwnd );
   return( TRUE );
}

/* This function handles the WM_CLOSE message.
 */
void FASTCALL SchedulerDlg_OnClose( HWND hwnd )
{
   /* Clean up
    */
   Amuser_WritePPInt( szSettings, "AutoScheduleMissed", IsDlgButtonChecked( hwnd, IDD_AUTORUN ) );
   Adm_DestroyMDIWindow( hwnd );
   DeleteBitmap( hbmpSchd );
   DeleteBrush( hbrSchedulerWnd );
   hwndScheduler = NULL;
}

/* This function handles the WM_MDIACTIVATE message.
 */
void FASTCALL SchedulerDlg_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_SCHEDULE ), (LPARAM)(LPSTR)hwnd );
   ToolbarState_EditWindow();
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, Adm_DefMDIDlgProc );
}

/* This function intercepts messages for the scheduler list box window
 * so that we can handle the right mouse button.
 */
LRESULT EXPORT FAR PASCAL SchedulerListProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   switch( msg )
      {
      case WM_ERASEBKGND:
         if( !fQuickUpdate )
            {
            RECT rc;

            GetClientRect( hwnd, &rc );
            FillRect( (HDC)wParam, &rc, hbrSchedulerWnd );
            }
         return( TRUE );

      case WM_CONTEXTMENU: /*!!SM!!*/
      case WM_RBUTTONDOWN:
         {
         POINT pt;
         HMENU hPopupMenu;
         int count;
         int index;

         /* Make sure this window is active and select the item
          * at the cursor.
          */
         if( hwndActive != GetParent( hwnd ) )
            SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndScheduler, 0L );

         /* Select the item under the cursor only if nothing is
          * currently selected.
          */
         index = ItemFromPoint( hwnd, (short)LOWORD(lParam), (short)HIWORD(lParam) );
         if( ListBox_GetSel( hwnd, index ) == 0 )
            {
            CallWindowProc( lpProcListCtl, hwnd, WM_LBUTTONDOWN, wParam, lParam );
            CallWindowProc( lpProcListCtl, hwnd, WM_LBUTTONUP, wParam, lParam );
            }

         /* Get the scheduler menu.
          */
         hPopupMenu = GetSubMenu( hPopupMenus, IPOP_SCHEDULER );

         /* If we have any selected items, use the attributes
          * of the first item to determine the menu items.
          */
         count = ListBox_GetCount( hwnd );
         MenuEnable( hPopupMenu, IDD_RUNNOW, count > 0 );
         MenuEnable( hPopupMenu, IDD_RESCHEDULE, count > 0 );
         MenuEnable( hPopupMenu, IDD_REMOVE, count > 0 );
         MenuEnable( hPopupMenu, IDD_DISABLE, count > 0 );

         /* Rename Disable to Enable if selected item is
          * disabled.
          */
         if( LB_ERR != ( index = ListBox_GetCaretIndex( hwnd ) ) && count > 0 )
            {
            LPSCHDITEM lpschd;

            /* Change the button label as appropriate.
             */
            lpschd = (LPSCHDITEM)ListBox_GetItemData( hwnd, index );
            if( !lpschd->fEnabled )
               MenuString( hPopupMenu, IDD_DISABLE, "Enable" );
            else
               MenuString( hPopupMenu, IDD_DISABLE, "Disable" );
            }
         GetCursorPos( &pt );
         TrackPopupMenu( hPopupMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndFrame, NULL );
         return( 0 );
         }

      case WM_KEYDOWN:
         /* Trap the Delete and key in the scheduler list box to
          * handle the Remove command.
          */
         if( wParam == VK_DELETE ) 
            {
            PostDlgCommand( hwndScheduler, IDD_REMOVE, 0 );
            return( 0 );
            }

         /* Trap the Insert key in the scheduler list box to
          * add a new event.
          */
         if( wParam == VK_INSERT )
            {
            PostDlgCommand( hwndScheduler, IDD_ADD, 1 );
            return( 0 );
            }
         break;
      }
   return( CallWindowProc( lpProcListCtl, hwnd, msg, wParam, lParam ) );
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL SchedulerDlg_OnAdjustWindows( HWND hwnd, int dx, int dy )
{
   Adm_MoveWnd( GetDlgItem( hwnd, IDCANCEL ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_ADD ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_REMOVE ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_DISABLE ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_RUNNOW ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_EDIT ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_HELP ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_AUTORUN ), 0, dy );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_SCHLIST ), dx, dy );
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL SchedulerDlg_OnMove( HWND hwnd, int x, int y )
{
   Amuser_WriteWindowState( szSchedulerWndName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL SchedulerDlg_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   HWND hwndList;
   HWND hwndHdr;
   RECT rc;

   /* Update the header extent.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_SCHLIST ) );
   VERIFY( hwndHdr = GetDlgItem( hwnd, IDD_SCHLIST ) );
   GetClientRect( hwndList, &rc );
   Header_SetExtent( hwndHdr, rc.bottom );

   /* Save the new window state to the configuration file.
    */
   Amuser_WriteWindowState( szSchedulerWndName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SETFOCUS message.
 */
void FASTCALL SchedulerDlg_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   BOOL fEnabled;

   fEnabled = IsWindowEnabled( GetDlgItem( hwnd, IDD_SCHLIST ) );
   SetFocus( GetDlgItem( hwnd, fEnabled ? IDD_SCHLIST : IDOK ) );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL SchedulerDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSCHEDULER );
         break;

      case IDD_SCHLIST:
         if( codeNotify == LBN_SELCHANGE )
            {
            EnableControl( hwnd, IDD_REMOVE, TRUE );
            EnableControl( hwnd, IDD_EDIT, TRUE );
            SetEnableDisable( hwnd );
            break;
            }
         if( codeNotify != LBN_DBLCLK )
            break;

      case IDD_RESCHEDULE:
      case IDD_EDIT: {
         HWND hwndList;
         int index;
         int count;

         VERIFY( hwndList = GetDlgItem( hwnd, IDD_SCHLIST ) );
         index = ListBox_GetCaretIndex( hwndList );
         count = ListBox_GetCount( hwndList );
         if( LB_ERR != index && count > 0 )
            {
            LPSCHDITEM lpschd;

            lpschd = (LPSCHDITEM)ListBox_GetItemData( hwndList, index );
            ASSERT( NULL != lpschd );
            if( Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_SCHEDEDIT), EditSchedule, (LPARAM)lpschd ) )
               {
               /* Item has changed, so reschedule it and update
                * the list box.
                */
               ScheduleFirstAction( lpschd );
               UpdateSchedulerWindow( lpschd );
               SaveOneScheduledItem( lpschd );
               }
            }
         SetFocus( hwndList );
         break;
         }

      case IDD_ADD: {
         LPSCHDITEM lpschd;
         SCHDITEM schd;
         HWND hwndList;

         /* Initialise the schd structure.
          */
         schd.hCmd = CTree_CommandToHandle( "Full" );
         schd.sch.schType = SCHTYPE_ONCE;
         schd.pszFolder = NULL;
         Amdate_GetCurrentDate( &schd.sch.schDate );
         Amdate_GetCurrentTime( &schd.sch.schTime );

         /* Get the list box handle.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_SCHLIST ) );

         /* Display the dialog that the user completes to create a new
          * scheduled item.
          */
         if( Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_SCHEDEDIT), EditSchedule, (LPARAM)(LPSTR)&schd ) )
            if( NULL != ( lpschd = SchdInstall( (char *)CTree_GetCommandName( schd.hCmd ), &schd.sch, schd.pszFolder ) ) )
               {
               int index;

               /* Save this.
                */
               if ( SaveOneScheduledItem( lpschd ) )
               {

               /* Insert the new item into the listbox and select
                * it.
                */
               index = ListBox_InsertString( hwndList, -1, lpschd );
               ListBox_SetSel( hwndList, FALSE, -1 );
               ListBox_SetSel( hwndList, TRUE, index );

               /* Enable the Edit and Remove buttons.
                */
               EnableControl( hwnd, IDD_REMOVE, TRUE );
               EnableControl( hwnd, IDD_DISABLE, TRUE );
               EnableControl( hwnd, IDD_EDIT, TRUE );
               EnableControl( hwnd, IDD_RUNNOW, TRUE );
               SetEnableDisable( hwnd );
               }
               }

         /* If folder was specified, free up memory allocated
          * for its name.
          */
         if( NULL != schd.pszFolder )
            FreeMemory( &schd.pszFolder );
         SetFocus( hwndList );
         break;
         }

      case IDD_RUNNOW: {
         HWND hwndList;
         int index;
         int count;

         /* Run the specified item
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_SCHLIST ) );
         count = ListBox_GetCount( hwndList );
         for( index = 0; index < count; ++index )
            if( ListBox_GetSel( hwndList, index ) )
               {
               LPSCHDITEM lpschd;

               lpschd = (LPSCHDITEM)ListBox_GetItemData( hwndList, index );
               ASSERT( NULL != lpschd );
               RunScheduledCommand( lpschd );

               /* Reschedule for next run.
                */
               if( lpschd->sch.schType != SCHTYPE_ONCE )
                  ScheduleFirstAction( lpschd );
               SaveOneScheduledItem( lpschd );
               UpdateSchedulerWindow( lpschd );
               }
         break;
         }

      case IDD_DISABLE: {
         HWND hwndList;
         int index;
         int count;

         /* Enable or disable the selected item.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_SCHLIST ) );
         count = ListBox_GetCount( hwndList );
         for( index = 0; index < count; ++index )
            if( ListBox_GetSel( hwndList, index ) )
               {
               LPSCHDITEM lpschd;

               lpschd = (LPSCHDITEM)ListBox_GetItemData( hwndList, index );
               ASSERT( NULL != lpschd );
               lpschd->fEnabled = !lpschd->fEnabled;
               if( lpschd->fEnabled )
                  ScheduleFirstAction( lpschd );
               UpdateSchedulerWindow( lpschd );
               SaveOneScheduledItem( lpschd );
               }
         SetEnableDisable( hwnd );
         SetFocus( hwndList );
         break;
         }

      case IDD_REMOVE: {
         LPSCHDITEM lpschd;
         HWND hwndList;
         CMDREC msi;
         int index;
         int count;
         int start;

         /* Remove the selected item.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_SCHLIST ) );
         count = ListBox_GetCount( hwndList );
         start = -1;
         for( index = 0; index < count; ++index )
         {
            if( ListBox_GetSel( hwndList, index ) > 0 )
            {
               if( -1 == start )
                  start = index;
               lpschd = (LPSCHDITEM)ListBox_GetItemData( hwndList, index );
               ASSERT( NULL != lpschd );
               CTree_GetItem( lpschd->hCmd, &msi );
               wsprintf( lpTmpBuf, GS(IDS_STR988), GINTRSC(msi.lpszTooltipString) );
               if( IDYES == fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONINFORMATION ) ) // 2.56.2053 FS#152
               {
                  SchdRemove( lpschd );
                  --count;
                  --index;
               }
            }
         }
         SetFocus( hwndList );
         ListBox_SetSel( hwndList, TRUE, 0 );

         break;
         }

      case IDCANCEL:
         SendMessage( hwnd, WM_CLOSE, 0, 0L );
         break;
      }
}

/* Change the Disable button to Enable if the selected
 * item is disabled. And visa versa.
 */
void FASTCALL SetEnableDisable( HWND hwnd )
{
   HWND hwndList;
   int index;
   int count;

   /* Get the selected item.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_SCHLIST ) );
   index = ListBox_GetCaretIndex( hwndList );
   count = ListBox_GetCount( hwndList );
   if( LB_ERR != index && count > 0 )
      {
      LPSCHDITEM lpschd;

      /* Change the button label as appropriate.
       */
      lpschd = (LPSCHDITEM)ListBox_GetItemData( hwndList, index );
      if( lpschd->fEnabled )
         SetDlgItemText( hwnd, IDD_DISABLE, "Disa&ble" );
      else
         SetDlgItemText( hwnd, IDD_DISABLE, "Ena&ble" );
      }
}

/* This function handles notification messages from the
 * treeview control.
 */
LRESULT FASTCALL ScheduleDlg_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
{
   switch( lpNmHdr->code )
      {
      case HDN_ITEMCLICK:
         return( TRUE );

      case HDN_BEGINTRACK:
         return( TRUE );

      case HDN_TRACK:
      case HDN_ENDTRACK: {
         HD_NOTIFY FAR * lpnf;
         HWND hwndList;
         int diff;
         RECT rc;

         /* Save the old offset of the column being
          * dragged.
          */
         lpnf = (HD_NOTIFY FAR *)lpNmHdr;
         diff = lpnf->pitem->cxy - hdrColumns[ lpnf->iItem ];
         hdrColumns[ lpnf->iItem ] = lpnf->pitem->cxy;

         /* Invalidate from the column being dragged.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_SCHLIST ) );
         VERIFY( hwndList = GetDlgItem( hwndList, 0x3000 ) );
         GetClientRect( hwndList, &rc );
         rc.left = lpnf->cxyUpdate;
         if( diff > 0 )
            rc.left -= diff; // + 1;

         /* Now use ScrollWindow.
          */
         fQuickUpdate = TRUE;
         ScrollWindow( hwndList, diff, 0, &rc, &rc );
         UpdateWindow( hwndList );
         fQuickUpdate = FALSE;

         /* Save updates.
          */
         if( HDN_ENDTRACK == lpNmHdr->code )
            Amuser_WritePPArray( szSettings, "SchedulerColumns", hdrColumns, 4 );
         return( TRUE );
         }
      }
   return( FALSE );
}

/* This function handles the WM_MEASUREITEM message
 */
void FASTCALL SchedulerDlg_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   HFONT hOldFont;
   BITMAP bmp;
   HDC hdc;

   hdc = GetDC( hwnd );
   hOldFont = SelectFont( hdc, hSchedulerFont );
   GetTextMetrics( hdc, &tm );
   GetObject( hbmpSchd, sizeof( BITMAP ), &bmp );
   lpMeasureItem->itemHeight = max( tm.tmHeight + tm.tmExternalLeading, bmp.bmHeight ) + 2;
   SelectFont( hdc, hOldFont );
   ReleaseDC( hwnd, hdc );
}

/* This function handles the WM_DRAWITEM message
 */
void FASTCALL SchedulerDlg_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   if( lpDrawItem->itemID != -1 )
      {
      LPSCHDITEM lpschd;
      HFONT hOldFont;
      HWND hwndList;
      COLORREF T;
      COLORREF tmpT;
      COLORREF tmpB;
      HBRUSH hbr;
      SIZE size;
      RECT rc;
      int y;

      /* Get the item we're drawing.
       */
      hwndList = GetDlgItem( hwnd, IDD_SCHLIST );
      lpschd = (LPSCHDITEM)ListBox_GetItemData( hwndList, lpDrawItem->itemID );
      if( NULL == lpschd )
         return;

      /* Set up the painting tools.
       */
      rc = lpDrawItem->rcItem;
      hOldFont = SelectFont( lpDrawItem->hDC, hSchedulerFont );
      hbr = CreateSolidBrush( crSchedulerWnd );
      T = crSchedulerText;
      tmpT = SetTextColor( lpDrawItem->hDC, T );
      tmpB = SetBkColor( lpDrawItem->hDC, crSchedulerWnd );

      /* Get text extent to compute Y offset of all text on the line
       * (so that they are vertically centered).
       */
      WriteScheduledCommandName( lpschd, lpTmpBuf );
      GetTextExtentPoint( lpDrawItem->hDC, lpTmpBuf, strlen(lpTmpBuf), &size );
      y = rc.top + ( ( rc.bottom - rc.top ) - size.cy ) / 2;

      /* If item selected, show command name in selected
       * colours.
       */
      rc.left = 1;
      rc.right = min( rc.left + size.cx + 4, hdrColumns[ 0 ] );
      if( !( lpDrawItem->itemState & ODS_SELECTED ) )
         ExtTextOut( lpDrawItem->hDC, rc.left + 2, y, ETO_OPAQUE|ETO_CLIPPED, &rc, lpTmpBuf, strlen(lpTmpBuf), NULL );
      else
         {
         SetTextColor( lpDrawItem->hDC, crHighlightText );
         SetBkColor( lpDrawItem->hDC, crHighlight );
         ExtTextOut( lpDrawItem->hDC, rc.left + 2, y, ETO_OPAQUE|ETO_CLIPPED, &rc, lpTmpBuf, strlen(lpTmpBuf), NULL );
         SetTextColor( lpDrawItem->hDC, T );
         SetBkColor( lpDrawItem->hDC, crSchedulerWnd );
         }
      if( lpDrawItem->itemState & ODS_FOCUS )
         DrawFocusRect( lpDrawItem->hDC, &rc );
      rc.left = rc.right;
      rc.right = hdrColumns[ 0 ] + 1;
      FillRect( lpDrawItem->hDC, &rc, hbr );

      /* Only do these if repainting entire line.
       */
      if( lpDrawItem->itemAction & ODA_DRAWENTIRE )
         {
         /* Show the frequency
          */
         rc.left = hdrColumns[ 0 ];
         rc.right = rc.left + hdrColumns[ 1 ] + 1;
         if( !lpschd->fEnabled )
            strcpy( lpTmpBuf, "Disabled" );
         else switch( lpschd->sch.schType )
            {
            case SCHTYPE_STARTUP:
               wsprintf( lpTmpBuf, GS(IDS_STR1030) );
               break;

            case SCHTYPE_ONCE:
               wsprintf( lpTmpBuf, GS(IDS_STR1031),
                  SchShortDate( &lpschd->sch.schDate ),
                  SchShortTime( &lpschd->sch.schTime ) );
               break;

            case SCHTYPE_DAYPERIOD:
               wsprintf( lpTmpBuf, GS(IDS_STR1141), lpschd->sch.schDate.iDay );
               break;

            case SCHTYPE_DAILY:
               wsprintf( lpTmpBuf, GS(IDS_STR1038), SchShortTime( &lpschd->sch.schTime ) );
               break;

            case SCHTYPE_WEEKLY:
               wsprintf( lpTmpBuf, GS(IDS_STR1032),
                  DayOfWeek[ lpschd->sch.schDate.iDayOfWeek ],
                  SchShortTime( &lpschd->sch.schTime ) );
               break;

            case SCHTYPE_HOURLY:
               wsprintf( lpTmpBuf, GS(IDS_STR1033), lpschd->sch.schTime.iMinute );
               break;

            case SCHTYPE_EVERY:
               wsprintf( lpTmpBuf, GS(IDS_STR1034), lpschd->sch.schTime.iMinute );
               break;

            case SCHTYPE_MONTHLY:
               wsprintf( lpTmpBuf, GS(IDS_STR1035),
                  NameOfDay[ lpschd->sch.schDate.iDay - 1 ],
                  SchShortTime( &lpschd->sch.schTime ) );
               break;
            }
         ExtTextOut( lpDrawItem->hDC, rc.left, y, ETO_OPAQUE|ETO_CLIPPED, &rc, lpTmpBuf, strlen(lpTmpBuf), NULL );

         /* Show the time when this command was last run.
          */
         rc.left = rc.right;
         rc.right = rc.left + hdrColumns[ 2 ] + 1;
         if( lpschd->fRunOnce )
            wsprintf( lpTmpBuf, GS(IDS_STR1036), SchShortTime(&lpschd->schLastTime), SchShortDate(&lpschd->schLastDate) );
         else
            strcpy( lpTmpBuf, GS(IDS_STR1037) );
         ExtTextOut( lpDrawItem->hDC, rc.left, y, ETO_OPAQUE|ETO_CLIPPED, &rc, lpTmpBuf, strlen(lpTmpBuf), NULL );

         /* Show the time when this command will next run.
          */
         rc.left = rc.right;
         rc.right = lpDrawItem->rcItem.right;
         lpTmpBuf[ 0 ] = '\0';
         if( SCHTYPE_STARTUP == lpschd->sch.schType )
            strcpy( lpTmpBuf, GS(IDS_STR1142) );
         else if( !lpschd->fActive )
            strcpy( lpTmpBuf, GS(IDS_STR1037) );
         else if( SCHTYPE_DAYPERIOD == lpschd->sch.schType )
            wsprintf( lpTmpBuf, "%s", SchShortDate(&lpschd->schNextDate) );
         else if( SCHTYPE_STARTUP != lpschd->sch.schType )
            wsprintf( lpTmpBuf, GS(IDS_STR1036), SchShortTime(&lpschd->schNextTime), SchShortDate(&lpschd->schNextDate) );
         ExtTextOut( lpDrawItem->hDC, rc.left, y, ETO_OPAQUE|ETO_CLIPPED, &rc, lpTmpBuf, strlen(lpTmpBuf), NULL );
         }

      /* Clean up.
       */
      SelectFont( lpDrawItem->hDC, hOldFont );
      SetTextColor( lpDrawItem->hDC, tmpT );
      SetBkColor( lpDrawItem->hDC, tmpB );
      DeleteBrush( hbr );
      }
}

/* This function updates the scheduler window for the specified
 * entry.
 */
void FASTCALL UpdateSchedulerWindow( LPSCHDITEM lpschd )
{
   HWND hwndList;
   int index;

   VERIFY( hwndList = GetDlgItem( hwndScheduler, IDD_SCHLIST ) );
   hwndList = GetDlgItem( hwndList, 0x3000 );
   index = RealListBox_FindItemData( hwndList, -1, (DWORD)lpschd );
   if( LB_ERR != index )
      {
      RECT rc;

      /* Found the entry, so redraw it.
       */
      ListBox_GetItemRect( hwndList, index, &rc );
      InvalidateRect( hwndList, &rc, TRUE );
      UpdateWindow( hwndList );
      }
}

/* Handles the Schedule Editor dialog.
 */
BOOL EXPORT CALLBACK EditSchedule( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, EditSchedule_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the Schedule Editor dialog.
 */
LRESULT FASTCALL EditSchedule_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, EditSchedule_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, EditSchedule_OnCommand );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, FolderCombo_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_DRAWITEM, FolderCombo_OnDrawItem );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSCHEDEDIT );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL EditSchedule_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPSCHDITEM lpschd;
   register int c;
   HWND hwndList;
   CMDREC msi;
   HCMD hSelCmd;
   HCMD hCmd;
   int index;

   /* Get ptr to scheduled item we're editing.
    */
   lpschd = (LPSCHDITEM)lParam;
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* Fill the list of commands that can be scheduled.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_COMMANDS ) );
   hCmd = hSelCmd = NULL;
   while( NULL != ( hCmd = CTree_EnumTree( hCmd, &msi ) ) )
      {  
      if( msi.nScheduleFlags & NSCHF_CANSCHEDULE )
         {
         /* Insert this command into the list.
          */
         index = ListBox_AddString( hwndList, GINTRSC(msi.lpszTooltipString) );
         ListBox_SetItemData( hwndList, index, (LPARAM)hCmd );
         }
      }

   /* Select our command.
    */
   if( NULL != lpschd )
      {
      if( CB_ERR != ( index = RealListBox_FindItemData( hwndList, -1, (LPARAM)lpschd->hCmd ) ) )
         ListBox_SetCurSel( hwndList, index );
      else
         EnableWindow( hwndList, FALSE );
      }
   else
      EnableWindow( hwndList, FALSE );

   /* Fill the list of folders.
    */
   FillListWithTopics( hwnd, IDD_FOLDERLIST, FTYPE_ALL|FTYPE_TOPICS );

   /* If folder is specified, parse it.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_FOLDERLIST ) );
   index = 0;
   if( NULL != lpschd )
      if( NULL != lpschd->pszFolder )
         {
         LPVOID pFolder;

         ParseFolderPathname( lpschd->pszFolder, &pFolder, FALSE, 0 );
         if( ( index = RealComboBox_FindItemData( hwndList, -1, (LPARAM)pFolder ) ) == CB_ERR )
            index = 0;
         }
   ComboBox_SetCurSel( hwndList, index );

   /* Fill the year combo box.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_YEARCOMBO ) );
   for( c = 2013; c <= 2020; ++c )
      {
      char sz[ 6 ];

      wsprintf( sz, "%u", c );
      ComboBox_InsertString( hwndList, -1, sz );
      }

   /* Fill the month combo box.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_MONTHCOMBO ) );
   for( c = 0; c < 12; ++c )
      ComboBox_InsertString( hwndList, -1, NameOfMonth[ c ] );

   /* Fill the Day Of Week combo box.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_DOWCOMBO ) );
   for( c = 0; c < 7; ++c )
      ComboBox_InsertString( hwndList, -1, DayOfWeek[ c ] );

   /* Fill the other day combo boxes.
    */
   FillDayComboBox( hwnd, IDD_DAYCOMBO );
   FillDayComboBox( hwnd, IDD_DAYCOMBO2 );

   /* Show the action type.
    */
   if( NULL != lpschd )
      {
      switch( lpschd->sch.schType )
         {
         case SCHTYPE_ONCE:      CheckDlgButton( hwnd, IDD_ONCE, TRUE ); break;
         case SCHTYPE_EVERY:     CheckDlgButton( hwnd, IDD_EVERY, TRUE ); break;
         case SCHTYPE_HOURLY:    CheckDlgButton( hwnd, IDD_HOURLY, TRUE ); break;
         case SCHTYPE_DAILY:     CheckDlgButton( hwnd, IDD_DAILY, TRUE ); break;
         case SCHTYPE_DAYPERIOD: CheckDlgButton( hwnd, IDD_DAYPERIOD, TRUE ); break;
         case SCHTYPE_WEEKLY:    CheckDlgButton( hwnd, IDD_WEEKLY, TRUE ); break;
         case SCHTYPE_MONTHLY:   CheckDlgButton( hwnd, IDD_MONTHLY, TRUE ); break;
         case SCHTYPE_STARTUP:   CheckDlgButton( hwnd, IDD_STARTUP, TRUE ); break;
         }
      ShowScheduleSettings( hwnd, lpschd );
      ShowScheduledCommandSettings( hwnd, lpschd->hCmd );
      }

   /* Fill the other day combo boxes.
    */
   FillDayComboBox( hwnd, IDD_DAYCOMBO );
   FillDayComboBox( hwnd, IDD_DAYCOMBO2 );
   return( TRUE );
}

/* This function fills the days combo box based on the
 * number of days in the month. It attempts to preserve
 * any existing days selection unless the day is out of
 * range for the new month.
 */
void FASTCALL FillDayComboBox( HWND hwnd, int id )
{
   static int DaysInMonth[ 12 ] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
   HWND hwndListOfMonths;
   HWND hwndListOfYears;
   HWND hwndListOfDays;
   int cDaysInMonth;
   register int c;
   int iCurDay;
   int iMonth;
   int iYear;

   /* Preserve the existing selection.
    */
   VERIFY( hwndListOfDays = GetDlgItem( hwnd, id ) );
   iCurDay = ComboBox_GetCurSel( hwndListOfDays );
   if( CB_ERR == iCurDay )
      iCurDay = 0;
   ComboBox_ResetContent( hwndListOfDays );

   /* Get the selected month.
    */
   VERIFY( hwndListOfMonths = GetDlgItem( hwnd, IDD_MONTHCOMBO ) );
   iMonth = ComboBox_GetCurSel( hwndListOfMonths );
   if( CB_ERR == iMonth )
      iMonth = 0;
   cDaysInMonth = DaysInMonth[ iMonth ];

   /* Adjust cDaysInMonth if month is February and the year is a
    * leap year.
    */
   VERIFY( hwndListOfYears = GetDlgItem( hwnd, IDD_YEARCOMBO ) );
   iYear = ComboBox_GetCurSel( hwndListOfYears );
   if( CB_ERR == iYear )
      iYear = 0;
   iYear += 2013;
   if( 1 == iMonth && ( iYear % 200 == 0 || iYear % 4 == 0 ) )
      ++cDaysInMonth;

   /* Fill the days combo box.
    */
   for( c = 1; c <= cDaysInMonth; ++c )
      {
      char sz[ 6 ];

      wsprintf( sz, "%u", c );
      ComboBox_InsertString( hwndListOfDays, -1, sz );
      }

   /* Set the original selection.
    */
   if( iCurDay >= DaysInMonth[ iMonth ] )
      iCurDay = DaysInMonth[ iMonth ] - 1;
   ComboBox_SetCurSel( hwndListOfDays, iCurDay );
}

/* This function handles the WM_COMMAND message.
 */ 
void FASTCALL EditSchedule_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_YEARCOMBO:
      case IDD_MONTHCOMBO:
         if( codeNotify == CBN_SELCHANGE )
            FillDayComboBox( hwnd, IDD_DAYCOMBO );
         break;

      case IDD_COMMANDS:
         if( codeNotify == LBN_SELCHANGE )
            {
            HWND hwndList;
            int index;
            HCMD hCmd;

            /* A new command has been selected from the list, so
             * update the dialog.
             */
            VERIFY( hwndList = GetDlgItem( hwnd, IDD_COMMANDS ) );
            index = ListBox_GetCurSel( hwndList );
            ASSERT( CB_ERR != index );
            hCmd = (HCMD)ListBox_GetItemData( hwndList, index );
            ASSERT( 0L != hCmd );

            /* Show or hide controls depending on the
             * command.
             */
            ShowScheduledCommandSettings( hwnd, hCmd );
            }
         break;

      case IDD_ONCE:
      case IDD_EVERY:
      case IDD_HOURLY:
      case IDD_DAILY:
      case IDD_WEEKLY:
      case IDD_STARTUP:
      case IDD_DAYPERIOD:
      case IDD_MONTHLY: {
         LPSCHDITEM lpschd;

         /* Rearrange the dialog to reflect the settings for
          * the specified item.
          */
         lpschd = (LPSCHDITEM)GetWindowLong( hwnd, DWL_USER );
         ShowScheduleSettings( hwnd, lpschd );
         break;
         }

      case IDOK: {
         LPSCHDITEM lpschd;
         HWND hwndList;
         int index;

         /* Set the new settings for this item.
          */
         lpschd = (LPSCHDITEM)GetWindowLong( hwnd, DWL_USER );
         Amdate_GetCurrentDate( &lpschd->sch.schDate );
         Amdate_GetCurrentTime( &lpschd->sch.schTime );
         if( IsDlgButtonChecked( hwnd, IDD_ONCE ) )
            {
            /* Retrieve the once-off date and time.
             */
            lpschd->sch.schType = SCHTYPE_ONCE;
            if( !BreakTime( hwnd, IDD_TIME, &lpschd->sch.schTime ) )
               break;
            GetDateField( hwnd, &lpschd->sch.schDate );
            }
         else if( IsDlgButtonChecked( hwnd, IDD_STARTUP ) )
            {
            /* Schedule to run at startup, so no
             * parameters.
             */
            lpschd->sch.schType = SCHTYPE_STARTUP;
            }
         else if( IsDlgButtonChecked( hwnd, IDD_EVERY ) )
            {
            /* Retrieve the minutes frequency.
             */
            lpschd->sch.schType = SCHTYPE_EVERY;
            if( !GetMinutesField( hwnd, &lpschd->sch.schTime.iMinute ) )
               break;
            }
         else if( IsDlgButtonChecked( hwnd, IDD_HOURLY ) )
            {
            /* Retrieve the minutes past the hour.
             */
            lpschd->sch.schType = SCHTYPE_HOURLY;
            if( !GetMinutesField( hwnd, &lpschd->sch.schTime.iMinute ) )
               break;
            }
         else if( IsDlgButtonChecked( hwnd, IDD_DAILY ) )
            {
            /* Retrieve the time of the day.
             */
            lpschd->sch.schType = SCHTYPE_DAILY;
            if( !BreakTime( hwnd, IDD_TIME, &lpschd->sch.schTime ) )
               break;
            }
         else if( IsDlgButtonChecked( hwnd, IDD_DAYPERIOD ) )
            {
            /* Retrieve the day frequency
             */
            lpschd->sch.schType = SCHTYPE_DAYPERIOD;
            if( !GetDayField3( hwnd, &lpschd->sch.schDate.iDay ) )
               break;
            }
         else if( IsDlgButtonChecked( hwnd, IDD_WEEKLY ) )
            {
            /* Retrieve the time and day of the week.
             */
            if( !BreakTime( hwnd, IDD_TIME, &lpschd->sch.schTime ) )
               break;
            lpschd->sch.schDate.iDayOfWeek = GetDayOfWeekField( hwnd );
            lpschd->sch.schType = SCHTYPE_WEEKLY;
            }
         else if( IsDlgButtonChecked( hwnd, IDD_MONTHLY ) )
            {
            /* Retrieve the day and time of the month.
             */
            if( !BreakTime( hwnd, IDD_TIME, &lpschd->sch.schTime ) )
               break;
            lpschd->sch.schDate.iDay = GetDayField2( hwnd );
            lpschd->sch.schType = SCHTYPE_MONTHLY;
            }

         /* Save the command name.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_COMMANDS ) );
         index = ListBox_GetCurSel( hwndList );
         ASSERT( CB_ERR != index );
         lpschd->hCmd = (HCMD)ListBox_GetItemData( hwndList, index );

         /* Get the folder name, if required.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_FOLDERLIST ) );
         if( NULL != lpschd->pszFolder )
            FreeMemory( &lpschd->pszFolder );
         if( IsWindowVisible( hwndList ) )
            if( CB_ERR != ( index = ComboBox_GetCurSel( hwndList ) ) )
               {
               char szFolder[ 192 ];
               LPVOID pFolder;

               /* Expand the folder name into a path.
                */
               pFolder = (LPVOID)ComboBox_GetItemData( hwndList, index );
               WriteFolderPathname( szFolder, pFolder );

               /* Save the folder name.
                */
               if( !fNewMemory( &lpschd->pszFolder, strlen(szFolder) + 1 ) )
                  {
                  OutOfMemoryError( hwnd, FALSE, FALSE );
                  break;
                  }
               strcpy( lpschd->pszFolder, szFolder );
               }
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* This function shows the settings for the specified scheduled
 * command in the dialog.
 */
void FASTCALL ShowScheduledCommandSettings( HWND hwnd, HCMD hCmd )
{
   CMDREC msi;
   BOOL fStatus;

   /* Get command details.
    */
   CTree_GetItem( hCmd, &msi );
   fStatus = ( msi.nScheduleFlags & NSCHF_NEEDFOLDER ) == NSCHF_NEEDFOLDER;
   ShowEnableControl( hwnd, IDD_PAGE, fStatus );
   ShowEnableControl( hwnd, IDD_FOLDERLIST, fStatus );
}

/* This function shows the settings for the specified schedule
 * item in the dialog.
 */
void FASTCALL ShowScheduleSettings( HWND hwnd, LPSCHDITEM lpschd )
{
   register int c;

   /* Hide or show the date/time fields.
    */
   for( c = 0; c < cSchdCtrlArray; ++c )
      if( IsDlgButtonChecked( hwnd, schCtrlArray[ c ].id ) )
         {
         register int i;

         /* For each control, show or hide it as appropriate
          * for this schedule type.
          */
         for( i = 0; i < cSchdCtrls; ++i )
            if( schCtrlArray[ c ].fCtlEnable[ i ] )
               {
               if( lpfCtlInit[ i ] )
                  lpfCtlInit[ i ]( hwnd, lpschd );
               ShowEnableControl( hwnd, nSchdCtrls[ i ], TRUE );
               }
            else
               ShowEnableControl( hwnd, nSchdCtrls[ i ], FALSE );
         break;
         }
}

/* This function fills out the Minutes field in the dialog.
 */
void FASTCALL SetMinutesField( HWND hwnd, LPSCHDITEM lpschd )
{
   char sz[ 20 ];

   /* Now fill out the time field with the specified time.
    */
   wsprintf( sz, "%u", lpschd->sch.schTime.iMinute );
   Edit_SetText( GetDlgItem( hwnd, IDD_MINUTES ), sz );
}

/* This function retrieves the Minutes field from the dialog.
 */
int FASTCALL GetMinutesField( HWND hwnd, WORD * pMinutes )
{
   int iMinute;

   if( GetDlgInt( hwnd, IDD_MINUTES, &iMinute, 0, 59 ) )
      {
      *pMinutes = (WORD)iMinute;
      return( TRUE );
      }
   return( FALSE );
}

/* This function fills out the Time field in the dialog.
 */
void FASTCALL SetTimeField( HWND hwnd, LPSCHDITEM lpschd )
{
   char sz[ 20 ];

   /* Now fill out the time field with the specified time.
    */
   wsprintf( sz, "%u:%2.2u", lpschd->sch.schTime.iHour, lpschd->sch.schTime.iMinute );
   Edit_SetText( GetDlgItem( hwnd, IDD_TIME ), sz );
}

/* This function fills out the Day Of Week combo box in
 * the dialog.
 */
void FASTCALL SetDayOfWeekField( HWND hwnd, LPSCHDITEM lpschd )
{
   HWND hwndList;

   /* Set the day of week.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_DOWCOMBO ) );
   ComboBox_SetCurSel( hwndList, lpschd->sch.schDate.iDayOfWeek );
}

/* This function retrieves the Day Of Week from
 * the dialog.
 */
int FASTCALL GetDayOfWeekField( HWND hwnd )
{
   HWND hwndList;

   /* Set the day of week.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_DOWCOMBO ) );
   return( ComboBox_GetCurSel( hwndList ) );
}

/* This function fills out the Day field in the dialog.
 */
void FASTCALL SetDayField( HWND hwnd, LPSCHDITEM lpschd )
{
   HWND hwndList;

   /* Set the day.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_DAYCOMBO ) );
   ComboBox_SetCurSel( hwndList, lpschd->sch.schDate.iDay - 1 );
}

/* This function fills out the Month field in the dialog.
 */
void FASTCALL SetMonthField( HWND hwnd, LPSCHDITEM lpschd )
{
   HWND hwndList;

   /* Set the month.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_MONTHCOMBO ) );
   ASSERT( lpschd->sch.schDate.iMonth >= 1 && lpschd->sch.schDate.iMonth <= 12 );
   ComboBox_SetCurSel( hwndList, lpschd->sch.schDate.iMonth - 1 );
}

/* This function fills out the Year field in the dialog.
 */
void FASTCALL SetYearField( HWND hwnd, LPSCHDITEM lpschd )
{
   HWND hwndList;

   /* Set the year.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_YEARCOMBO ) );
   ComboBox_SetCurSel( hwndList, lpschd->sch.schDate.iYear - 2013 );
}

/* This function retrieves the Date field from the dialog.
 */
void FASTCALL GetDateField( HWND hwnd, AM_DATE * pDate )
{
   HWND hwndList;

   /* Get the day.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_DAYCOMBO ) );
   pDate->iDay = ComboBox_GetCurSel( hwndList ) + 1;

   /* Set the month.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_MONTHCOMBO ) );
   pDate->iMonth = ComboBox_GetCurSel( hwndList ) + 1;

   /* Set the year.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_YEARCOMBO ) );
   pDate->iYear = ComboBox_GetCurSel( hwndList ) + 2013;
}

/* This function retrieves the selected day from the day
 * combo box.
 */
int FASTCALL GetDayField2( HWND hwnd )
{
   HWND hwndList;

   VERIFY( hwndList = GetDlgItem( hwnd, IDD_DAYCOMBO2 ) );
   return( ComboBox_GetCurSel( hwndList ) + 1 );
}

/* This function sets the day field.
 */
void FASTCALL SetDayField2( HWND hwnd, LPSCHDITEM lpschd )
{
   HWND hwndList;

   /* Set the day.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_DAYCOMBO2 ) );
   ComboBox_SetCurSel( hwndList, lpschd->sch.schDate.iDay - 1 );
}

/* This function retrieves the inputted day field.
 */
BOOL FASTCALL GetDayField3( HWND hwnd, WORD * pDays )
{
   int iDays;

   if( GetDlgInt( hwnd, IDD_DAYS, &iDays, 1, 60 ) )
      {
      *pDays = (WORD)iDays;
      return( TRUE );
      }
   return( FALSE );
}

/* This function sets the day field.
 */
void FASTCALL SetDayField3( HWND hwnd, LPSCHDITEM lpschd )
{
   char sz[ 20 ];

   /* Fill out the day field
    */
   wsprintf( sz, "%u", lpschd->sch.schDate.iDay );
   Edit_SetText( GetDlgItem( hwnd, IDD_DAYS ), sz );
}

/* This function handles the Message Box dialog.
 */
BOOL EXPORT CALLBACK MissedScheduleDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, MissedScheduleDlg_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the MissedScheduleDlg
 * dialog.
 */
LRESULT FASTCALL MissedScheduleDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, MissedScheduleDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, MissedScheduleDlg_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsMISSEDSCHEDULE );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL MissedScheduleDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   char szBuf[ 100 ];
   LPSCHDITEM lpschd;

   /* Say which command got missed.
    */
   lpschd = (LPSCHDITEM)lParam;
   WriteScheduledCommandName( lpschd, szBuf );
   wsprintf( lpTmpBuf, GS(IDS_STR1041),
             szBuf,
             SchShortTime(&lpschd->schNextTime),
             SchShortDate(&lpschd->schNextDate) );
   SetDlgItemText( hwnd, IDD_PAD1, lpTmpBuf );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL MissedScheduleDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDYES:
      case IDNOALL:
      case IDNO:
         Amuser_WritePPInt( szSettings, "AutoScheduleMissed", IsDlgButtonChecked( hwnd, IDD_ALWAYS ) );
         if( NULL != hwndScheduler )
            CheckDlgButton( hwndScheduler, IDD_AUTORUN, Amuser_GetPPInt( szSettings, "AutoScheduleMissed", FALSE ) );
         EndDialog( hwnd, id );
         break;
      }
}

/* This function writes the name of the scheduled command
 * to pBuf.
 */
void FASTCALL WriteScheduledCommandName( LPSCHDITEM lpschd, char * pBuf )
{
   CMDREC msi;

   CTree_GetItem( lpschd->hCmd, &msi );
   strcpy( pBuf, GINTRSC(msi.lpszTooltipString) );
   if( lpschd->pszFolder )
      {
      strcat( pBuf, " " );
      strcat( pBuf, lpschd->pszFolder );
      }
}

void FASTCALL UpdateSchItemFolder( LPSTR lpszOldItemName, LPSTR lpszNewItemName )
{
   LPSCHDITEM lpschd;

   for( lpschd = lpschdFirst; lpschd; lpschd = lpschd->lpschdNext )
   {
      if( _strcmpi( lpschd->pszFolder, lpszOldItemName ) == 0 )
      {
         strcpy( lpschd->pszFolder, lpszNewItemName );
         SaveOneScheduledItem( lpschd );
      }
   }



}
