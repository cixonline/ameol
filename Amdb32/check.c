/* CHECK.C - Check and Repair a folder
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
#include "amcntxt.h"
#include "amlib.h"
#include "amuser.h"
#include "amctrls.h"
#include "htmlhelp.h"
#include <string.h>
#include "amdbi.h"
#include "amdbrc.h"
#include <io.h>

#define  THIS_FILE   __FILE__

static BOOL fDefDlgEx = FALSE;

#define  LEN_MESSAGEID     80                /* Name of an unique mail message number */

#define  IDD_VIEWLOG       0x7567

BOOL EXPORT CALLBACK CheckAndRepairDlg( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL CheckAndRepair_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL CheckAndRepair_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL CheckAndRepair_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL CheckAndRepair_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
void FASTCALL CheckAndRepair_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );

static BOOL fAutoFix;                        /* Automatically fix any errors found */
static BOOL fGenReport;                      /* Generate a status report */
static BOOL fForceBuild;                     /* Rebuild topic files */
static BOOL fMainAbort;                      /* Set TRUE if check is aborted */
static BOOL fChecking;                       /* TRUE if check in progress */
static BOOL fViewLog;
static BOOL fOldLogo;                        /* TRUE if we display old logo */
static char chTmpBuf[ 256 ];                 /* General purpose buffer */

typedef struct tagAMDBCHECKSTRUCT {
   LPVOID pFolder;            /* Handle of folder to check */
   WORD wFlags;               /* Flags */
   BOOL fPrompt;              /* TRUE if we check without prompting */
} AMDBCHECKSTRUCT;

BOOL FASTCALL CheckTypedFolder( HWND, AMDBCHECKSTRUCT FAR * );
int FASTCALL PostCheckFolder( HNDFILE, PCL );
int FASTCALL PostCheckCategory( HNDFILE, PCAT );
int FASTCALL CheckFolder( HNDFILE, PCL );
int FASTCALL CheckTopic( HNDFILE, PTL, HWND );
void FASTCALL LogRepair( HNDFILE, LPVOID, WORD );
BOOL FASTCALL RetryableDiskSaveError( HWND, char * );
BOOL FASTCALL RebuildThreadFile( PTL );
void FASTCALL ChangeDefaultButton( HWND, int );

static char szRepairLog[] = "repair.log";    /* Check and Repair log file */
static char szRepairLogPath[ _MAX_PATH ];    /* Where the file is stored */

const char NEAR szHelpFile[] = "ameol2.chm";

BOOL EXPORT CALLBACK ResultDlgBoxProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL ResultDlgBoxProc_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL ResultDlgBoxProc_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL ResultDlgBoxProc_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL Am_PlaySound( LPSTR );

void FASTCALL Am_PlaySound( LPSTR lpSoundName )
{
      char szSound[ _MAX_DIR ];
      char szPath[ _MAX_PATH ];

      szSound[ 0 ] = '\0';
      wsprintf( szPath, "AppEvents\\Schemes\\Apps\\Ameol2\\%s\\.Current", lpSoundName );
      ReadRegistryKey( HKEY_CURRENT_USER, szPath, "", "", szSound, sizeof( szSound ) );
      PlaySound( szSound, NULL, SND_FILENAME|SND_NODEFAULT );
}

/* This function checks and repairs the specified folder.
 */
BOOL EXPORT WINAPI Amdb_CheckAndRepair( HWND hwnd, LPVOID pFolder, BOOL fPrompt )
{
   AMDBCHECKSTRUCT acs;

   acs.pFolder = pFolder;
   acs.fPrompt = fPrompt;
   acs.wFlags = 0;
   fOldLogo = Amuser_GetPPInt( "Settings", "UseOldBitmaps", FALSE );
   return( Adm_Dialog( hLibInst, hwnd, MAKEINTRESOURCE(IDDLG_CHECKANDREPAIR), CheckAndRepairDlg, (LPARAM)(LPSTR)&acs ) );
}

/* Check the specified folder. The hwnd handle is the handle of the
 * Check dialog box.
 */
BOOL FASTCALL CheckTypedFolder( HWND hwndDlg, AMDBCHECKSTRUCT FAR * lpacs )
{
   WORD wTotal;
   WORD wCount;
   HWND hwndGauge;
   int cRepaired;
   HNDFILE fhLog;
   BOOL fNoisy;

   /* Initialise variables.
    */
   wCount = 0;
   cRepaired = 0;
   fViewLog = FALSE;
   fhLog = HNDFILE_ERROR;
   SetDlgItemText( hwndDlg, IDD_CHECKING, GS(IDS_STR435) );
   fNoisy = Amuser_GetPPInt( "Settings", "MakeNoises", FALSE );

   /* Create the repair log file.
    */
   if( fGenReport )
      {
      Amuser_GetActiveUserDirectory( szRepairLogPath, _MAX_PATH );
      strcat( strcat( szRepairLogPath, "\\" ), szRepairLog );
      if( ( fhLog = Amfile_Open( szRepairLogPath, AOF_WRITE ) ) != HNDFILE_ERROR )
         {
         DWORD dwSize;

         /* Seek to the end, because we're appending. This has the bonus of
          * returning the total file size.
          */
         dwSize = Amfile_SetPosition( fhLog, 0L, ASEEK_END );
         if( dwSize > 60000 && lpacs->fPrompt )
            {
            wsprintf( chTmpBuf, GS(IDS_STR443), (LPSTR)szRepairLogPath );
            if( MessageBox( hwndDlg, chTmpBuf, "Warning", MB_YESNO|MB_ICONQUESTION ) == IDYES )
               {
               Amfile_Close( fhLog );
               fhLog = Amfile_Create( szRepairLogPath, 0 );
               }
            }
         }
      else if( ( fhLog = Amfile_Create( szRepairLogPath, 0 ) ) == HNDFILE_ERROR )
         {
         if( lpacs->fPrompt )
            {
            wsprintf( chTmpBuf, GS(IDS_STR161), (LPSTR)szRepairLogPath );
            MessageBox( hwndDlg, chTmpBuf, NULL, MB_OK|MB_ICONEXCLAMATION );
            }
         }
      }

   /* Write the date of this check to the start of the log.
    */
   if( fhLog != HNDFILE_ERROR )
      {
      AM_DATE date;
      AM_TIME time;

      Amdate_UnpackDate( Amdate_GetPackedCurrentDate(), &date );
      Amdate_UnpackTime( Amdate_GetPackedCurrentTime(), &time );
      wsprintf( chTmpBuf, GS(IDS_STR160), Amdate_FormatShortDate( &date, NULL ), Amdate_FormatTime( &time, NULL ) );
      Amfile_Write( fhLog, chTmpBuf, strlen( chTmpBuf ) );
      }

   /* Report options.
    */
   if( fAutoFix )
      {
      strcpy( chTmpBuf, GS(IDS_STR519) );
      Amfile_Write( fhLog, chTmpBuf, strlen( chTmpBuf ) );
      }

   /* Close any open cached file handles
    */
   if( fhCachedThd != HNDFILE_ERROR )
      {
      Amfile_Close( fhCachedThd );
      fhCachedThd = HNDFILE_ERROR;
      ptlCachedThd = NULL;
      }
   if( fhCachedTxt != HNDFILE_ERROR )
      {
      Amfile_Close( fhCachedTxt );
      fhCachedTxt = HNDFILE_ERROR;
      ptlCachedTxt = NULL;
      }

   /* Now check pFolder and take the appropriate action.
    */
   hwndGauge = GetDlgItem( hwndDlg, IDD_GAUGE );
   SendMessage( hwndGauge, PBM_SETSTEP, 1, 0L );
   wTotal = 0;
   if( Amdb_IsTopicPtr( lpacs->pFolder ) )
      {
      /* Check a topic.
       */
      SendMessage( hwndGauge, PBM_SETRANGE, 0, MAKELPARAM( 0, 100 ) );
      SetDlgItemText( hwndDlg, IDD_TOPICNAME, Amdb_GetTopicName( lpacs->pFolder ) );
      SetDlgItemText( hwndDlg, IDD_FOLDERNAME, Amdb_GetFolderName( Amdb_FolderFromTopic( lpacs->pFolder ) ) );
      SetDlgItemText( hwndDlg, IDD_CATEGORYNAME, Amdb_GetCategoryName ( Amdb_CategoryFromFolder ( Amdb_FolderFromTopic( lpacs->pFolder ) ) ) );
      cRepaired += CheckTopic( fhLog, (PTL)lpacs->pFolder, hwndGauge );
      }
   else if( Amdb_IsFolderPtr( lpacs->pFolder ) )
      {
      PTL ptl;

      /* Check a folder and all topics in the folder.
       */
      SendMessage( hwndGauge, PBM_SETRANGE, 0, MAKELPARAM( 0, ((PCL)lpacs->pFolder)->clItem.cTopics ) );
      cRepaired += CheckFolder( fhLog, (PCL)lpacs->pFolder );
      for( ptl = ((PCL)lpacs->pFolder)->ptlFirst; !fMainAbort && ptl; ptl = ptl->ptlNext )
         {
         SetDlgItemText( hwndDlg, IDD_TOPICNAME, Amdb_GetTopicName( ptl ) );
         SetDlgItemText( hwndDlg, IDD_FOLDERNAME, Amdb_GetFolderName( Amdb_FolderFromTopic( ptl ) ) );
         SetDlgItemText( hwndDlg, IDD_CATEGORYNAME, Amdb_GetCategoryName ( Amdb_CategoryFromFolder ( Amdb_FolderFromTopic( ptl ) ) ) );
         cRepaired += CheckTopic( fhLog, ptl, NULL );
         SendMessage( hwndGauge, PBM_STEPIT, 0, 0L );
         }
      cRepaired += PostCheckFolder( fhLog, (PCL)lpacs->pFolder );
      }
   else
      {
      PCL pcl;
      PTL ptl;

      /* Check a category, all folders in the category and all topics
       * in each folder.
       */
      for( pcl = ((PCAT)lpacs->pFolder)->pclFirst; pcl; pcl = pcl->pclNext )
         wTotal += pcl->clItem.cTopics;
      SendMessage( hwndGauge, PBM_SETRANGE, 0, MAKELPARAM( 0, wTotal ) );
      for( pcl = ((PCAT)lpacs->pFolder)->pclFirst; !fMainAbort && pcl; pcl = pcl->pclNext )
         {
         cRepaired += CheckFolder( fhLog, pcl );
         for( ptl = pcl->ptlFirst; !fMainAbort && ptl; ptl = ptl->ptlNext )
            {
            SetDlgItemText( hwndDlg, IDD_TOPICNAME, Amdb_GetTopicName( ptl ) );
            SetDlgItemText( hwndDlg, IDD_FOLDERNAME, Amdb_GetFolderName( Amdb_FolderFromTopic( ptl ) ) );
            SetDlgItemText( hwndDlg, IDD_CATEGORYNAME, Amdb_GetCategoryName ( Amdb_CategoryFromFolder ( Amdb_FolderFromTopic( ptl ) ) ) );
            cRepaired += CheckTopic( fhLog, ptl, NULL );
            SendMessage( hwndGauge, PBM_STEPIT, 0, 0L );
            }
         cRepaired += PostCheckFolder( fhLog, pcl );
         }
      cRepaired += PostCheckCategory( fhLog, (PCAT)lpacs->pFolder );
      }

   /* Close the repair log file
    */
   if( fhLog != HNDFILE_ERROR )
      {
      AM_DATE date;
      AM_TIME time;

      Amdate_UnpackDate( Amdate_GetPackedCurrentDate(), &date );
      Amdate_UnpackTime( Amdate_GetPackedCurrentTime(), &time );
      wsprintf( chTmpBuf, GS(IDS_STR162), Amdate_FormatShortDate( &date, NULL ), Amdate_FormatTime( &time, NULL ) );
      Amfile_Write( fhLog, chTmpBuf, strlen( chTmpBuf ) );
      Amfile_Close( fhLog );
      }

   /* If we get this far, then no problems!
    */
   if( cRepaired )
      {
      /* Report the result back to the user. Show different report
       * message depending on whether or not we fixed any errors.
       */
      if( lpacs->fPrompt )
         {
         char sz[ 200 ];
         int idDlg;

         if( fAutoFix )
            {
            strcpy( sz, GS(IDS_STR168) );
            wsprintf( chTmpBuf, GS(IDS_STR434), cRepaired );
            }
         else
            {
            strcpy( sz, GS(IDS_STR194) );
            wsprintf( chTmpBuf, GS(IDS_STR433), cRepaired );
            }
         if( fGenReport )
            strcat( sz, GS(IDS_STR176) );
         SetDlgItemText( hwndDlg, IDD_CHECKING, chTmpBuf );
         idDlg = fGenReport ? IDDLG_CHECKREPORTRESULT : IDDLG_CHECKRESULT;
         if( IDD_VIEW == Adm_Dialog( hRscLib, hwndDlg, MAKEINTRESOURCE(idDlg), ResultDlgBoxProc, (LPARAM)(LPSTR)sz ) )
            fViewLog = TRUE;
         else
            fViewLog = FALSE;
         }

      /* Save changes if we made any repairs.
       */
      if( fAutoFix )
         Amdb_CommitDatabase( FALSE );
      }
   else
   {
      SetDlgItemText( hwndDlg, IDD_CHECKING, GS(IDS_STR195) );
   }
   if( fNoisy )
      Am_PlaySound( "Check Complete" );
   if( Amdb_IsCategoryPtr( lpacs->pFolder ) )
      {
         SetDlgItemText( hwndDlg, IDD_CATEGORYNAME, Amdb_GetCategoryName( lpacs->pFolder ) );
         SetDlgItemText( hwndDlg, IDD_FOLDERNAME, "" );
         SetDlgItemText( hwndDlg, IDD_TOPICNAME, "" );
      }

   if( Amdb_IsFolderPtr( lpacs->pFolder ) )
      {
         SetDlgItemText( hwndDlg, IDD_FOLDERNAME, Amdb_GetFolderName( lpacs->pFolder ) );
         SetDlgItemText( hwndDlg, IDD_CATEGORYNAME, Amdb_GetCategoryName ( Amdb_CategoryFromFolder ( lpacs->pFolder ) ) );
         SetDlgItemText( hwndDlg, IDD_TOPICNAME, "" );
      }

   if( Amdb_IsTopicPtr( lpacs->pFolder ) )
      {
         SetDlgItemText( hwndDlg, IDD_TOPICNAME, Amdb_GetTopicName( lpacs->pFolder ) );
         SetDlgItemText( hwndDlg, IDD_FOLDERNAME, Amdb_GetFolderName( Amdb_FolderFromTopic( lpacs->pFolder ) ) );
         SetDlgItemText( hwndDlg, IDD_CATEGORYNAME, Amdb_GetCategoryName ( Amdb_CategoryFromFolder ( Amdb_FolderFromTopic( lpacs->pFolder ) ) ) );
      }

   return( cRepaired > 0 );
}

/* This function checks the specified folder data structure
 * for validity.
 */
int FASTCALL CheckFolder( HNDFILE fhLog, PCL pcl )
{
   char sz[ _MAX_FNAME ];
   int cRepaired;

   /* Check that the conference filename is valid.
    * Check for overwritten conference filename caused by moderator list bug
    */
   cRepaired = 0;
   if( _fmemchr( pcl->clItem.szSigFile, 0, sizeof(pcl->clItem.szSigFile) ) == NULL )
      {
      if( fAutoFix )
         pcl->clItem.szSigFile[ 0 ] = '\0';
      LogRepair( fhLog, pcl, IDS_STR655 );
      ++cRepaired;
      }
   if( pcl->clItem.szFileName[ 0 ] == '\0' )
      {
      if( fAutoFix )
         CreateUniqueFile( pcl->clItem.szFolderName, "CNF", TRUE, pcl->clItem.szFileName );
      LogRepair( fhLog, pcl, IDS_STR164 );
      ++cRepaired;
      }
   if( lstrlen( pcl->clItem.szFileName ) > 8 )
      {
      if( fAutoFix )
         CreateUniqueFile( pcl->clItem.szFolderName, "CNF", TRUE, pcl->clItem.szFileName );
      ++cRepaired;
      }
   wsprintf( sz, "%s.cnf", (LPSTR)pcl->clItem.szFileName );
   if( !Amdb_QueryFileExists( sz ) )
      {
      if( fAutoFix )
         {
         HNDFILE fh;
         
         fh = Amdb_CreateFile( sz, 0 );
         Amfile_Close( fh );
         }
      LogRepair( fhLog, pcl, IDS_STR166 );
      ++cRepaired;
      }
   return( cRepaired );
}

/* This function checks a folder after all topics have been checked
 * and possibly repaired.
 */
int FASTCALL PostCheckFolder( HNDFILE fhLog, PCL pcl )
{
   int cRepaired;
   DWORD cMsgs;
   long diff;
   PTL ptl;

   cMsgs = 0;
   cRepaired = 0;
   for( ptl = pcl->ptlFirst; ptl; ptl = ptl->ptlNext )
      cMsgs += ptl->tlItem.cMsgs;
   if( diff = cMsgs - pcl->clItem.cMsgs )
      {
      if( fAutoFix )
         pcl->clItem.cMsgs += diff;
      LogRepair( fhLog, pcl, IDS_STR438 );
      ++cRepaired;
      }
   return( cRepaired );
}

/* This function checks a category after all folders have been checked
 * and possibly repaired. It MUST be called after PostCheckFolder!
 */
int FASTCALL PostCheckCategory( HNDFILE fhLog, PCAT pcat )
{
   int cRepaired;
   DWORD cMsgs;
   long diff;
   PCL pcl;

   cMsgs = 0;
   cRepaired = 0;
   for( pcl = pcat->pclFirst; pcl; pcl = pcl->pclNext )
      cMsgs += pcl->clItem.cMsgs;
   if( diff = cMsgs - pcat->catItem.cMsgs )
      {
      if( fAutoFix )
         pcat->catItem.cMsgs += diff;
      LogRepair( fhLog, pcat, IDS_STR438 );
      ++cRepaired;
      }
   return( cRepaired );
}

/* This function checks one topic.
 */
int FASTCALL CheckTopic( HNDFILE fhLog, PTL ptl, HWND hwndGauge )
{
   PTH pth;
   DWORD cMarked;
   DWORD cPriority;
   DWORD cUnReadPriority;
   DWORD cUnRead;
   DWORD dwMinMsg;
   DWORD dwMaxMsg;
   DWORD cMsgs;
   WORD cInitRepaired;
   int cRepaired;
   WORD wMask;
   long diff;

   cRepaired = 0;
   if( !CommitTopicMessages( ptl, FALSE ) )
      {
      LogRepair( fhLog, ptl, IDS_STR613 );
      ++cRepaired;
      }
   cMarked = 0;
   cPriority = 0;
   cUnReadPriority = 0;
   cUnRead = 0;
   dwMinMsg = 0xFFFFFFFF;
   dwMaxMsg = 0L;
   cInitRepaired = cRepaired;
   Amdb_LockTopic( ptl );
   cMsgs = 0;
   if( ( ptl->tlItem.po.wFlags & (PO_PURGE_IGNORED|PO_SIZEPRUNE|PO_DATEPRUNE|PO_DELETEONLY) ) == 0 )
      {
      if( fAutoFix )
         {
         ptl->tlItem.po.wFlags = poDefault.wFlags;
         ptl->tlItem.po.wMaxSize = poDefault.wMaxSize;
         ptl->tlItem.po.wMaxDate = poDefault.wMaxDate;
         }
      LogRepair( fhLog, ptl, IDS_STR451 );
      ++cRepaired;
      }
   wMask = ptl->tlItem.po.wFlags & (PO_PURGE_IGNORED|PO_SIZEPRUNE|PO_DATEPRUNE|PO_DELETEONLY);
   if( wMask != PO_SIZEPRUNE && wMask != PO_DATEPRUNE && wMask != PO_DELETEONLY && wMask != PO_PURGE_IGNORED)
      {
      if( fAutoFix ) {
         ptl->tlItem.po.wFlags = poDefault.wFlags;
         ptl->tlItem.po.wMaxSize = poDefault.wMaxSize;
         ptl->tlItem.po.wMaxDate = poDefault.wMaxDate;
         }
      LogRepair( fhLog, ptl, IDS_STR451 );
      ++cRepaired;
      }

   /* Loop for each message in the topic and accumulate a running
    * count of messages and their attributes.
    */
   for( pth = ptl->pthFirst; pth && !fMainAbort; pth = pth->pthNext )
      {
      YieldToSystem();
      if( !( pth->thItem.dwFlags & MF_READ ) )
         ++cUnRead;
      if( pth->thItem.dwFlags & MF_PRIORITY )
         {
         ++cPriority;
         if( !( pth->thItem.dwFlags & MF_READ ) )
            ++cUnReadPriority;
         }
      if( pth->thItem.dwFlags & MF_MARKED )
         ++cMarked;
      if( pth->thItem.dwMsg < dwMinMsg )
         dwMinMsg = pth->thItem.dwMsg;
      if( pth->thItem.dwMsg > dwMaxMsg )
         dwMaxMsg = pth->thItem.dwMsg;
      ++cMsgs;
      if( NULL != hwndGauge && ptl->tlItem.cMsgs )
         {
         WORD wCount;

         wCount = (WORD)( ( (DWORD)cMsgs * 100 ) / (DWORD)ptl->tlItem.cMsgs );
         SendMessage( hwndGauge, PBM_SETPOS, wCount, 0L );
         }
      }
   if( cMsgs > 0 && !fMainAbort ) {
      if( dwMinMsg != ptl->tlItem.dwMinMsg )
         {
         if( fAutoFix )
            ptl->tlItem.dwMinMsg = dwMinMsg;
         LogRepair( fhLog, ptl, IDS_STR180 );
         ++cRepaired;
         }
      if( dwMaxMsg != ptl->tlItem.dwMaxMsg )
         {
         if( fAutoFix )
            ptl->tlItem.dwMaxMsg = dwMaxMsg;
         LogRepair( fhLog, ptl, IDS_STR181 );
         ++cRepaired;
         }
      if( diff = cMsgs - ptl->tlItem.cMsgs )
         {
         if( fAutoFix )
            {
            ptl->tlItem.cMsgs += diff;
            ptl->pcl->clItem.cMsgs += diff;
            ptl->pcl->pcat->catItem.cMsgs += diff;
            }
         LogRepair( fhLog, ptl, IDS_STR438 );
         ++cRepaired;
         }
      if( diff = cMarked - ptl->tlItem.msgc.cMarked )
         {
         if( fAutoFix )
            {
            ptl->tlItem.msgc.cMarked += diff;
            ptl->pcl->msgc.cMarked += diff;
            ptl->pcl->pcat->msgc.cMarked += diff;
            cTotalMsgc.cMarked += diff;
            }
         LogRepair( fhLog, ptl, IDS_STR173 );
         ++cRepaired;
         }
      if( diff = cUnRead - ptl->tlItem.msgc.cUnRead )
         {
         if( fAutoFix )
            {
            ptl->tlItem.msgc.cUnRead += diff;
            ptl->pcl->msgc.cUnRead += diff;
            ptl->pcl->pcat->msgc.cUnRead += diff;
            cTotalMsgc.cUnRead += diff;
            }
         LogRepair( fhLog, ptl, IDS_STR174 );
         ++cRepaired;
         Amuser_CallRegistered( AE_UNREADCHANGED, (LPARAM)ptl, 0 );
         }
      if( diff = cPriority - ptl->tlItem.msgc.cPriority )
         {
         if( fAutoFix )
            {
            ptl->tlItem.msgc.cPriority += diff;
            ptl->pcl->msgc.cPriority += diff;
            ptl->pcl->pcat->msgc.cPriority += diff;
            cTotalMsgc.cPriority += diff;
            }
         LogRepair( fhLog, ptl, IDS_STR175 );
         ++cRepaired;
         }
      if( diff = cUnReadPriority - ptl->tlItem.msgc.cUnReadPriority )
         {
         if( fAutoFix )
            {
            ptl->tlItem.msgc.cUnReadPriority += diff;
            ptl->pcl->msgc.cUnReadPriority += diff;
            ptl->pcl->pcat->msgc.cUnReadPriority += diff;
            cTotalMsgc.cUnReadPriority += diff;
            }
         LogRepair( fhLog, ptl, IDS_STR518 );
         ++cRepaired;
         }

      /* If any errors were found or the user requested that the index file
       * be rebuilt, then rebuild the index file.
       */
      if( ( cRepaired - cInitRepaired && fAutoFix ) || fForceBuild )
         if( RebuildThreadFile( ptl ) )
            LogRepair( fhLog, ptl, (WORD)( cRepaired ? IDS_STR436 : IDS_STR439) );
         else
            LogRepair( fhLog, ptl, IDS_STR437 );
      }
   Amdb_InternalUnlockTopic( ptl );
   Amdb_DiscardTopic( ptl );
   return( cRepaired );
}

/* This function rebuilds the thread file.
 */
BOOL FASTCALL RebuildThreadFile( PTL ptl )
{
   char szOldName[ 13 ];
   char szWorkName[ 13 ];
   BOOL fError;
   HNDFILE fh;
   PTH pth;

   fError = FALSE;
   wsprintf( szOldName, "%s.THD", (LPSTR)ptl->tlItem.szFileName );
   wsprintf( szWorkName, "%s.WRK", (LPSTR)ptl->tlItem.szFileName );
   if( ( fh = Amdb_CreateFile( szWorkName, 0 ) ) != HNDFILE_ERROR ) {
      for( pth = ptl->pthFirst; pth; pth = pth->pthNext )
         {
         pth->thItem.dwMsgPtr = Amfile_SetPosition( fh, 0L, ASEEK_CURRENT );
         while( !fError && Amfile_Write( fh, (LPCSTR)&pth->thItem, sizeof( THITEM ) ) != sizeof( THITEM ) )
            if( !RetryableDiskSaveError( GetFocus(), szWorkName ) )
               fError = TRUE;
         }
      Amfile_Close( fh );
      if( fError )
         Amdb_DeleteFile( szWorkName );
      else
         {
         Amdb_DeleteFile( szOldName );
         Amdb_RenameFile( szWorkName, szOldName );
         }
      }
   return( !fError );
}

/* This function writes out an informational message to the
 * repair log file.
 */
void FASTCALL LogRepair( HNDFILE fh, LPVOID pFolder, WORD wID )
{
   if( fh != HNDFILE_ERROR )
      {
      register int c;

      c = 0;
      if( NULL != pFolder )
         if( Amdb_IsCategoryPtr( (PCAT)pFolder ) )
            c += wsprintf( chTmpBuf, "Category %s", Amdb_GetCategoryName( (PCAT)pFolder ) );
         else if( Amdb_IsFolderPtr( (PCL)pFolder ) )
            c += wsprintf( chTmpBuf, "%s", Amdb_GetFolderName( (PCL)pFolder ) );
         else
            {
            PCL pcl;

            pcl = Amdb_FolderFromTopic( (PTL)pFolder );
            c += wsprintf( chTmpBuf + c, "%s/%s", Amdb_GetFolderName( pcl ), Amdb_GetTopicName( (PTL)pFolder ) );
            }
      while( c < 24 )
         chTmpBuf[ c++ ] = ' ';
      c += wsprintf( chTmpBuf + c, " : %s", (LPSTR)GS(wID ) );
      c += wsprintf( chTmpBuf + c, "\r\n" );
      Amfile_Write( fh, chTmpBuf, strlen( chTmpBuf ) );
      }
}

/* This function handles the Check and Repair dialog.
 */
BOOL EXPORT CALLBACK CheckAndRepairDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = CheckAndRepair_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the CheckAndRepair
 * dialog.
 */
LRESULT FASTCALL CheckAndRepair_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, CheckAndRepair_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, CheckAndRepair_OnCommand );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, CheckAndRepair_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_DRAWITEM, CheckAndRepair_OnDrawItem );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsCHECKANDREPAIR );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL CheckAndRepair_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   AMDBCHECKSTRUCT FAR * lpacs;
   HWND hwndList;
   int index;
   PCAT pcat;
   PCL pcl;
   PTL ptl;

   /* Get pointer to AMDBCHECKSTRUCT structure.
    */
   lpacs = (AMDBCHECKSTRUCT FAR *)lParam;
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* Fill list of folders.
    */
   hwndList = GetDlgItem( hwnd, IDD_FOLDERLIST );
   for( pcat = pcatFirst; pcat; pcat = pcat->pcatNext )
      {
      ComboBox_InsertString( hwndList, -1, pcat );
      for( pcl = pcat->pclFirst; pcl; pcl = pcl->pclNext )
         {
         ComboBox_InsertString( hwndList, -1, pcl );
         for( ptl = pcl->ptlFirst; ptl; ptl = ptl->ptlNext )
            ComboBox_InsertString( hwndList, -1, ptl );
         }
      }

   /* Find and select the input folder.
    */
   if( CB_ERR != ( index = ComboBox_FindStringExact( hwndList, -1, lpacs->pFolder ) ) )
   {
      ComboBox_SetCurSel( hwndList, index );
      if( Amdb_IsTopicPtr( lpacs->pFolder ) )
      {
         SetDlgItemText( hwnd, IDD_TOPICNAME, Amdb_GetTopicName( lpacs->pFolder ) );
         SetDlgItemText( hwnd, IDD_FOLDERNAME, Amdb_GetFolderName( Amdb_FolderFromTopic( lpacs->pFolder ) ) );
         SetDlgItemText( hwnd, IDD_CATEGORYNAME, Amdb_GetCategoryName ( Amdb_CategoryFromFolder ( Amdb_FolderFromTopic( lpacs->pFolder ) ) ) );
      }

      if( Amdb_IsFolderPtr( lpacs->pFolder ) )
      {
         SetDlgItemText( hwnd, IDD_FOLDERNAME, Amdb_GetFolderName( lpacs->pFolder ) );
         SetDlgItemText( hwnd, IDD_CATEGORYNAME, Amdb_GetCategoryName ( Amdb_CategoryFromFolder ( lpacs->pFolder ) ) );
      }
      if( Amdb_IsCategoryPtr( lpacs->pFolder ) )
         SetDlgItemText( hwnd, IDD_CATEGORYNAME, Amdb_GetCategoryName( lpacs->pFolder ) );
   }

   /* Disable 'Checking folder...' caption.
    */
// ShowWindow( GetDlgItem( hwnd, IDD_CHECKING ), SW_HIDE );
   SetDlgItemText( hwnd, IDD_CHECKING, GS(IDS_STR1240) );


   /* Initialise flags and variables.
    */
   fAutoFix = TRUE;
   fGenReport = TRUE;

   /* Prompt whether to go ahead
    */
   CheckDlgButton( hwnd, IDD_AUTOFIX, fAutoFix );
   CheckDlgButton( hwnd, IDD_GENREPORT, fGenReport );

   /* Shoot if fPrompt is false.
    */
   if( !lpacs->fPrompt )
      PostDlgCommand( hwnd, IDOK, 0L );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL CheckAndRepair_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDOK: {
         AMDBCHECKSTRUCT FAR * lpacs;
         char szExec[ 64 ];
         HWND hwndGauge;
         HWND hwndList;
         int index;

         /* Get the options.
          */
         fAutoFix = IsDlgButtonChecked( hwnd, IDD_AUTOFIX );
         fGenReport = IsDlgButtonChecked( hwnd, IDD_GENREPORT );
         fMainAbort = FALSE;

         /* Get the check structure.
          */
         lpacs = (AMDBCHECKSTRUCT FAR *)GetWindowLong( hwnd, DWL_USER );

         /* Get the folder to check.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_FOLDERLIST ) );
         index = ComboBox_GetCurSel( hwndList );
         ASSERT( CB_ERR != index );
         lpacs->pFolder = (LPVOID)ComboBox_GetItemData( hwndList, index );
         ASSERT( NULL != lpacs->pFolder );

         /* Disable options.
          */
         EnableControl( hwnd, IDD_AUTOFIX, FALSE );
         EnableControl( hwnd, IDD_GENREPORT, FALSE );
         EnableControl( hwnd, IDD_AUTOFIX, FALSE );
         EnableControl( hwnd, IDD_OPTIONS, FALSE );
         EnableControl( hwnd, IDD_TITLE, FALSE );
         EnableControl( hwnd, IDD_FOLDERLIST, FALSE );
         EnableControl( hwnd, IDOK, FALSE );
//       ShowWindow( GetDlgItem( hwnd, IDD_CHECKING ), SW_SHOW );

         /* Call the checking routine.
          */
         fChecking = TRUE;
         CheckTypedFolder( hwnd, lpacs );
         fChecking = FALSE;

         /* Done! Enable options
          */
         hwndGauge = GetDlgItem( hwnd, IDD_GAUGE );
         SendMessage( hwndGauge, PBM_SETPOS, 0, 0L );
         EnableControl( hwnd, IDD_AUTOFIX, TRUE );
         EnableControl( hwnd, IDD_GENREPORT, TRUE );
         EnableControl( hwnd, IDD_AUTOFIX, TRUE );
         EnableControl( hwnd, IDD_OPTIONS, TRUE );
         EnableControl( hwnd, IDD_TITLE, TRUE );
         EnableControl( hwnd, IDD_FOLDERLIST, TRUE );
         EnableControl( hwnd, IDOK, TRUE );

         /* Done check and no prompt? Exit now.
          */
         if( !lpacs->fPrompt )
            {
            EndDialog( hwnd, TRUE );
            break;
            }

         /* Rename Cancel to close and make it active.
          */
         SetDlgItemText( hwnd, IDCANCEL, "&Close" );
         ChangeDefaultButton( hwnd, IDCANCEL );
         SetFocus( GetDlgItem( hwnd, IDCANCEL ) );

         if( fViewLog )
            {
            wsprintf( szExec, "notepad %s", szRepairLogPath );
            WinExec( szExec, SW_SHOWNORMAL );
            }

         break;
         }

      case IDCANCEL:
         if( fChecking )
            fMainAbort = TRUE;
         else
            EndDialog( hwnd, TRUE );
         break;

      case IDD_FOLDERLIST: {

         AMDBCHECKSTRUCT FAR * lpacs;
         int index;
         HWND hwndList;

         
         /* Get the check structure.
          */
         lpacs = (AMDBCHECKSTRUCT FAR *)GetWindowLong( hwnd, DWL_USER );

         /* Get the folder to check.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_FOLDERLIST ) );
         index = ComboBox_GetCurSel( hwndList );
         ASSERT( CB_ERR != index );
         lpacs->pFolder = (LPVOID)ComboBox_GetItemData( hwndList, index );
         ASSERT( NULL != lpacs->pFolder );

         if( Amdb_IsCategoryPtr( lpacs->pFolder ) )
            SetDlgItemText( hwnd, IDD_CATEGORYNAME, Amdb_GetCategoryName( lpacs->pFolder ) );
         else
            SetDlgItemText( hwnd, IDD_CATEGORYNAME, "" );

         if( Amdb_IsFolderPtr( lpacs->pFolder ) )
         {
            SetDlgItemText( hwnd, IDD_FOLDERNAME, Amdb_GetFolderName( lpacs->pFolder ) );
            SetDlgItemText( hwnd, IDD_CATEGORYNAME, Amdb_GetCategoryName ( Amdb_CategoryFromFolder ( lpacs->pFolder ) ) );
         }
         else
            SetDlgItemText( hwnd, IDD_FOLDERNAME, "" );

         if( Amdb_IsTopicPtr( lpacs->pFolder ) )
         {
            SetDlgItemText( hwnd, IDD_TOPICNAME, Amdb_GetTopicName( lpacs->pFolder ) );
            SetDlgItemText( hwnd, IDD_FOLDERNAME, Amdb_GetFolderName( Amdb_FolderFromTopic( lpacs->pFolder ) ) );
            SetDlgItemText( hwnd, IDD_CATEGORYNAME, Amdb_GetCategoryName ( Amdb_CategoryFromFolder ( Amdb_FolderFromTopic( lpacs->pFolder ) ) ) );
         }
         else
            SetDlgItemText( hwnd, IDD_TOPICNAME, "" );

                      }

      }
}

/* This function changes the default push button on a dialog
 * without changing the focus.
 */
void FASTCALL ChangeDefaultButton( HWND hwnd, int id )
{
   DWORD dwDefId;

   /* Get rid of current default push button.
    */
   dwDefId = SendMessage( hwnd, DM_GETDEFID, 0, 0L );
   if( HIWORD( dwDefId ) == DC_HASDEFID )
      SendDlgItemMessage( hwnd, (int)LOWORD( dwDefId ), BM_SETSTYLE, (WPARAM)BS_PUSHBUTTON, (LPARAM)TRUE );

   /* Set new default push button.
    */
   SendMessage( hwnd, DM_SETDEFID, id, 0L);
   SendDlgItemMessage( hwnd, id, BM_SETSTYLE, (WPARAM)BS_DEFPUSHBUTTON, (LPARAM)TRUE );
}

/* This function yields to the system to allow other tasks to
 * run. It returns TRUE if okay, or FALSE if a WM_QUIT was seen.
 */
BOOL FASTCALL YieldToSystem( void )
{
   MSG msg;

   if( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
      if( msg.message == WM_QUIT )
         {
         PostQuitMessage( msg.wParam );
         return( FALSE );
         }
      else
         {
         TranslateMessage( &msg );
         DispatchMessage( &msg );
         }
   return( TRUE );
}

/* This function reports an error saving to the disk. The user is
 * offered the opportunity to retry the action that raised
 * the error condition.
 */
BOOL FASTCALL RetryableDiskSaveError( HWND hwnd, char * pszDir )
{
   char szErrMsgBuf[ 256 ];
   char chDrive;

   chDrive = Amdir_GetDrive( pszDir );
   wsprintf( szErrMsgBuf, GS(IDS_STR188), chDrive );
   return( MessageBox( hwnd, szErrMsgBuf, NULL, MB_RETRYCANCEL|MB_ICONEXCLAMATION ) == IDRETRY );
}

/* This function handles the WM_MEASUREITEM message
 */
void FASTCALL CheckAndRepair_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   HBITMAP hbmp;
   BITMAP bmp;
   HDC hdc;

   hdc = GetDC( hwnd );
   GetTextMetrics( hdc, &tm );
   hbmp = LoadBitmap( hLibInst, MAKEINTRESOURCE(IDB_DATABASE) );
   GetObject( hbmp, sizeof( BITMAP ), &bmp );
   DeleteBitmap( hbmp );
   ReleaseDC( hwnd, hdc );
   lpMeasureItem->itemHeight = max( tm.tmHeight + tm.tmExternalLeading, bmp.bmHeight + 2 );
}

/* This function handles the WM_DRAWITEM message
 */
void FASTCALL CheckAndRepair_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   HBRUSH hbr;
   HBITMAP hbmp;
   LPCSTR lpsz;
   LPVOID pData;
   int offset;
   int width;
   COLORREF tmpT, tmpB;
   COLORREF T, B;
   RECT rc;

   /* Get the folder handle.
    */
   if( lpDrawItem->itemID == -1 )
      return;
   pData = (LPVOID)ComboBox_GetItemData( lpDrawItem->hwndItem, lpDrawItem->itemID );
   if( NULL == pData )
      return;
   rc = lpDrawItem->rcItem;

   /* Set fSelect if this item is selected in the edit field.
    */
   if( rc.left > 0 )
      width = 0;
   else
      width = 16;

   /* Set the colours we're going to use.
    */
   if( lpDrawItem->itemState & ODS_SELECTED ) {
      T = GetSysColor( COLOR_HIGHLIGHTTEXT );
      B = GetSysColor( COLOR_HIGHLIGHT );
      }
   else {
      T = GetSysColor( COLOR_WINDOWTEXT );
      B = GetSysColor( COLOR_WINDOW );
      }
   hbr = CreateSolidBrush( B );
   FillRect( lpDrawItem->hDC, &rc, hbr );
   tmpT = SetTextColor( lpDrawItem->hDC, T );
   tmpB = SetBkColor( lpDrawItem->hDC, B );
   hbmp = NULL;

   /* Set the bitmap, offset and text for the
    * selected item.
    */
   if( Amdb_IsCategoryPtr( pData ) )
      {
      hbmp = LoadBitmap( hLibInst, MAKEINTRESOURCE(IDB_DATABASESELECTED) );
      offset = 0;
      lpsz = Amdb_GetCategoryName( (PCAT)pData );
      }
   else if( Amdb_IsFolderPtr( pData ) )
      {
      hbmp = LoadBitmap( hLibInst, MAKEINTRESOURCE(IDB_FOLDERSELECTED) );
      offset = width;
      lpsz = Amdb_GetFolderName( (PCL)pData );
      }
   else
      {
      PCL pcl;

      switch( Amdb_GetTopicType( (PTL)pData ) )
         {
         case FTYPE_NEWS:     hbmp = LoadBitmap( hLibInst, MAKEINTRESOURCE(IDB_NEWSFOLDER) ); break;
         case FTYPE_MAIL:     hbmp = LoadBitmap( hLibInst, MAKEINTRESOURCE(IDB_MAILFOLDER) ); break;
         case FTYPE_UNTYPED:  hbmp = LoadBitmap( hLibInst, MAKEINTRESOURCE(IDB_UNTYPEDFOLDER) ); break;
         case FTYPE_CIX:      hbmp = LoadBitmap( hLibInst, MAKEINTRESOURCE(fOldLogo ? IDB_CIXFOLDER_OLD : IDB_CIXFOLDER) ); break;
         case FTYPE_LOCAL:    hbmp = LoadBitmap( hLibInst, MAKEINTRESOURCE(IDB_LOCALTOPIC) ); break;
         default:             ASSERT(FALSE); break;
         }
      offset = 2 * width;
      pcl = Amdb_FolderFromTopic( (PTL)pData );
      if( 0 == width )
         {
         wsprintf( chTmpBuf, "%s/%s", Amdb_GetFolderName( pcl ), Amdb_GetTopicName( (PTL)pData ) );
         lpsz = chTmpBuf;
         }
      else
         lpsz = Amdb_GetTopicName( (PTL)pData );
      }

   /* Draw the bitmap.
    */
   rc.left += offset;
   if( NULL != hbmp )
      {
      Amuser_DrawBitmap( lpDrawItem->hDC, rc.left, rc.top + 1, 16, 15, hbmp, 0 );
      DeleteBitmap( hbmp );
      }

   /* Draw the text.
    */
   rc.left += 22;
   DrawText( lpDrawItem->hDC, lpsz, -1, &rc, DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX );

   /* Clean up afterwards.
    */
   SetTextColor( lpDrawItem->hDC, tmpT );
   SetBkColor( lpDrawItem->hDC, tmpB );
   DeleteBrush( hbr );
}

/* This function handles the Message Box dialog.
 */
BOOL EXPORT CALLBACK ResultDlgBoxProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, ResultDlgBoxProc_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the ResultDlgBoxProc
 * dialog.
 */
LRESULT FASTCALL ResultDlgBoxProc_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, ResultDlgBoxProc_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, ResultDlgBoxProc_OnCommand );

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL ResultDlgBoxProc_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   if( lParam )
      SetWindowText( GetDlgItem( hwnd, IDD_TEXT ), (LPSTR)lParam );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL ResultDlgBoxProc_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_VIEW:
      case IDCANCEL:
         EndDialog( hwnd, id );
         break;
      }
}

/* This function attempts to repair usenet threading.
 */
BOOL WINAPI EXPORT Amdb_RethreadUsenet( HWND hwnd, PTL ptl )
{
   HCURSOR hCursor;
   HWND hwndGauge;
   char sz[ 200 ];
   BOOL fChanged;
   HNDFILE fhLog;
   DWORD cMsgs;
   HWND hDlg;
   PTH pth;

   /* Exit now if topic isn't a mailbox or news folder.
    */
   if( Amdb_GetTopicType( ptl ) != FTYPE_MAIL && Amdb_GetTopicType( ptl ) != FTYPE_NEWS )
      return( FALSE );

   /* Open a file to report rethreading
    */
   fhLog = Amfile_Create( "THREAD.LOG", 0 );

   /* Display dialog.
    */
   hDlg = Adm_CreateDialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_RETHREAD), (DLGPROC)ResultDlgBoxProc, 0L );
   YieldToSystem();

   /* Show the topic title.
    */
   wsprintf( sz, GS(IDS_STR533), ptl->pcl->clItem.szFolderName, ptl->tlItem.szTopicName );
   SetDlgItemText( hDlg, IDD_TOPIC, sz );

   /* Initialise the gauge in the
    * dialog.
    */
   hwndGauge = GetDlgItem( hDlg, IDD_GAUGE );
   SendMessage( hwndGauge, PBM_SETRANGE, 0, MAKELPARAM( 0, 100 ) );
   SendMessage( hwndGauge, PBM_SETSTEP, 1, 0L );

   /* Initialise.
    */
   fChanged = FALSE;
   hCursor = SetCursor( LoadCursor( NULL, IDC_WAIT ) );
   CommitTopicMessages( ptl, TRUE );
   Amdb_LockTopic( ptl );
   cMsgs = 0;
   for( pth = ptl->pthFirst; pth; pth = pth->pthNext )
      {
      HPSTR lpszText;
      HPSTR lpszText2;
      WORD wCount;

      if( lpszText = lpszText2 = Amdb_GetMsgText( pth ) )
         {
         while( *lpszText )
            {
            register int c;
            char szType[ LEN_RFCTYPE + 1 ];

            while( *lpszText == ' ' || *lpszText == '\t' )
               ++lpszText;
            if( *lpszText == '\r' || *lpszText == '\n' || *lpszText == '\0' )
               break;
            for( c = 0; c < LEN_RFCTYPE && *lpszText && *lpszText != ':'; ++c )
               szType[ c ] = *lpszText++;
            szType[ c ] = '\0';
            if( *lpszText == ':' )
               ++lpszText;
            while( *lpszText == ' ' )
               ++lpszText;
            if( _strcmpi( szType, "Message-Id" ) == 0 )
               {
               char chEnd = '\0';
               char szMessageId[ LEN_MESSAGEID + 1 ];

               for( c = 0; c < LEN_MESSAGEID && *lpszText && *lpszText != '\r' && *lpszText != chEnd; ++c )
                  if( *lpszText == '<' )
                     {
                     c = -1;
                     chEnd = '>';
                     ++lpszText;
                     }
                  else
                     szMessageId[ c ] = *lpszText++;
               szMessageId[ c ] = '\0';
               if( lstrcmp( pth->thItem.szMessageId, szMessageId ) )
                  {
                  /* Report repair to the log file
                   */
                  if( HNDFILE_ERROR != fhLog )
                     {
                     char sz[ 120 ];

                     wsprintf( sz, GS(IDS_STR527), szMessageId );
                     Amfile_Write( fhLog, sz, strlen( sz ) );
                     }
                  lstrcpy( pth->thItem.szMessageId, szMessageId );
                  fChanged = TRUE;
                  }
               }
            else if( _strcmpi( szType, "References" ) == 0 || _strcmpi( szType, "In-Reply-To" ) == 0 )
               {
               DWORD dwComment;
               char szReference[ 80 ];
               PTH pth2;

               dwComment = 0;
               while( *lpszText && *lpszText != '\r' && *lpszText != '\n' )
                  {
                  while( *lpszText == ' ' )
                     ++lpszText;
                  if( *lpszText == '<' )
                     ++lpszText;
                  for( c = 0; *lpszText && *lpszText != '\r' && *lpszText != '>'; )
                     {
                     if( c < 79 )
                        szReference[ c++ ] = *lpszText;
                     ++lpszText;
                     }
                  if( *lpszText == '>' )
                     ++lpszText;
                  szReference[ c ] = '\0';
                  for( pth2 = ptl->pthFirst; pth2; pth2 = pth2->pthNext )
                     if( lstrcmpi( pth2->thItem.szMessageId, szReference ) == 0 )
                        {
                        dwComment = pth2->thItem.dwMsg;
                        break;
                        }
                  while( *lpszText == ' ' )
                     ++lpszText;
                  }
               if( dwComment != pth->thItem.dwComment )
                  {
                  /* Report repair to the log file
                   */
                  if( HNDFILE_ERROR != fhLog )
                     {
                     char sz[ 120 ];

                     wsprintf( sz, GS(IDS_STR526), pth->thItem.dwMsg, dwComment );
                     Amfile_Write( fhLog, sz, strlen( sz ) );
                     }
                  pth->thItem.dwComment = dwComment;
                  fChanged = TRUE;
                  }
               break;
               }
            while( *lpszText != '\r' && *lpszText != '\n' && *lpszText )
               ++lpszText;
            if( *lpszText == '\r' )
               if( *++lpszText == '\n' )
                  ++lpszText;
            }
         Amdb_FreeMsgTextBuffer( lpszText2 );
         }

      /* Update gauge.
       */
      ++cMsgs;
      wCount = (WORD)( ( (DWORD)cMsgs * 100 ) / (DWORD)ptl->tlItem.cMsgs );
      SendMessage( hwndGauge, PBM_SETPOS, wCount, 0L );
      }

   /* All done!
    */
   if( !fChanged )
      Amdb_UnlockTopic( ptl );
   else
      {
      RebuildThreadFile( ptl );
      Amdb_UnlockTopic( ptl );
      Amdb_DiscardTopic( ptl );
      }
   SetCursor( hCursor );
   DestroyWindow( hDlg );
   Amfile_Close( fhLog );
   return( fChanged );
}
