/* PURGE.C - Implements the purge dialogs
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
#include "palrc.h"
#include "resource.h"
#include "amlib.h"
#include <memory.h>
#include <string.h>
#include <ctype.h>
#include "admin.h"
#include "editmap.h"

#define  THIS_FILE   __FILE__

char szPurgeSettingsWndClass[] = "amctl_PurgeSettingsWndcls";
char szPurgeSettingsWndName[] = "Purge Settings";

static BOOL fDefDlgEx = FALSE;         /* DefDlg recursion flag trap */
static BOOL fRegistered = FALSE;       /* TRUE if we've registered the dialog window */
static BOOL fAbortPurge;               /* TRUE if abort button hit during purging */
HWND hwndPurgeSettings = NULL;         /* Purge settings window procedure */
static BOOL fPurgeCommand;
static HWND hPurgeDlg;
static BOOL fInitingDialog = FALSE;
static BOOL fPurgeChanged;

typedef struct tagPURGESETTINGSWNDINFO {
   HWND hwndFocus;
} PURGESETTINGSWNDINFO, FAR * LPPURGESETTINGSWNDINFO;

#define  GetPurgeSettingsWndInfo(h)       (LPPURGESETTINGSWNDINFO)GetWindowLong((h),DWL_USER)
#define  SetPurgeSettingsWndInfo(h,p)     SetWindowLong((h),DWL_USER,(long)(p))

BOOL FASTCALL DefaultPurgeOptions_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL DefaultPurgeOptions_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL DefaultPurgeOptions_OnNotify( HWND, int, LPNMHDR );
LRESULT FASTCALL DefaultPurgeOptions_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL FASTCALL PurgeOptions_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL PurgeOptions_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL PurgeOptions_OnNotify( HWND, int, LPNMHDR );
LRESULT FASTCALL PurgeOptions_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT FAR PASCAL Purging( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL Purging_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL Purging_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL Purging_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL FASTCALL PurgeSettings_OnCreate( HWND, LPCREATESTRUCT );
void FASTCALL PurgeSettings_OnClose( HWND );
void FASTCALL PurgeSettings_OnSize( HWND, UINT, int, int );
void FASTCALL PurgeSettings_OnMove( HWND, int, int );
void FASTCALL PurgeSettings_MDIActivate( HWND, BOOL, HWND, HWND );
void FASTCALL PurgeSettings_OnCommand( HWND, int, HWND, UINT );
void FASTCALL PurgeSettings_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
LRESULT FASTCALL PurgeSettings_OnNotify( HWND, int, LPNMHDR );
void FASTCALL PurgeSettings_OnAdjustWindows( HWND, int, int );
void FASTCALL PurgeSettings_Update( HWND );

LRESULT EXPORT CALLBACK PurgeSettingsWndProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT CALLBACK PurgeCallback( int, PURGECALLBACKINFO FAR * );

void FASTCALL UpdateLastPurgeInfo( HWND, LPVOID );
void FASTCALL ShowLastPurgeInfo( HWND, char *, WORD, WORD );
BOOL FASTCALL ApplyPurgeChanges( HWND, LPVOID, BOOL );
BOOL FASTCALL ReadPurgeOptions( HWND, PURGEOPTIONS *, BOOL );
void FASTCALL WritePurgeOptions( HWND, PURGEOPTIONS *, BOOL );
void FASTCALL PurgeOptions_ShowSettings( HWND, LPVOID );
void FASTCALL ExplainFolderPurgeSettings( LPVOID, char * );
void FASTCALL ExplainPurgeSettings( PURGEOPTIONS *, char * );

BOOL EXPORT CALLBACK CompactCallback( int, PURGECALLBACKINFO FAR * );
BOOL EXPORT CALLBACK PurgeMsgCallback( int, PURGECALLBACKINFO FAR * );

/* Description of function goes here
 */
BOOL EXPORT CALLBACK DefaultPurgeOptions( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, DefaultPurgeOptions_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the PurgeOptions
 * dialog.
 */
LRESULT FASTCALL DefaultPurgeOptions_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, DefaultPurgeOptions_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, DefaultPurgeOptions_OnCommand );
      HANDLE_MSG( hwnd, WM_NOTIFY, DefaultPurgeOptions_OnNotify );

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL DefaultPurgeOptions_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   WritePurgeOptions( hwnd, &poDef, TRUE );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL DefaultPurgeOptions_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_MAINTAIN:
      case IDD_OLDER:
      case IDD_STORE:
      case IDD_DELETEONLY:
      case IDD_IGNOREDONLY:
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_MAINTAIN_EDIT:
      case IDD_OLDER_EDIT:
/*    
         if( !fInitingDialog && ( codeNotify == EN_CHANGE ) ) //!!SM!! 2.56.2061
         {
            CheckDlgButton( hwnd, IDD_MAINTAIN, id == IDD_MAINTAIN_EDIT );
            CheckDlgButton( hwnd, IDD_OLDER, id == IDD_OLDER_EDIT );
            CheckDlgButton( hwnd, IDD_DELETEONLY, FALSE );
            CheckDlgButton( hwnd, IDD_IGNOREDONLY, FALSE );
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         }
*/
         if( codeNotify == EN_UPDATE  && !fInitingDialog && (  EditMap_GetModified( GetDlgItem( hwnd, IDD_MAINTAIN_EDIT ) ) || EditMap_GetModified( GetDlgItem( hwnd, IDD_OLDER_EDIT ) ) ) )
         {
            CheckDlgButton( hwnd, IDD_MAINTAIN, id == IDD_MAINTAIN_EDIT );
            CheckDlgButton( hwnd, IDD_OLDER, id == IDD_OLDER_EDIT );
            CheckDlgButton( hwnd, IDD_DELETEONLY, FALSE );
            CheckDlgButton( hwnd, IDD_IGNOREDONLY, FALSE );
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         }
         break;
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL DefaultPurgeOptions_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPREF_PURGE );
         break;

      case PSN_SETACTIVE:
         nLastOptionsDialogPage = ODU_PURGE;
         break;

      case PSN_APPLY: {
         /* Get and save the default options.
          */
         memset( &poDef, 0, sizeof(PURGEOPTIONS) );
         if( !ReadPurgeOptions( hwnd, &poDef, TRUE ) )
            return( PSNRET_INVALID_NOCHANGEPAGE );
         Amdb_SetDefaultPurgeOptions( &poDef );

         /* Save global settings.
          */
         Amuser_WritePPInt( szSettings, "PurgeMaxDate", poDef.wMaxDate );
         Amuser_WritePPInt( szSettings, "PurgeMaxSize", poDef.wMaxSize );
         Amuser_WritePPInt( szSettings, "PurgeFlags", poDef.wFlags );

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* Description of function goes here
 */
BOOL EXPORT CALLBACK PurgeOptions( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, PurgeOptions_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the PurgeOptions
 * dialog.
 */
LRESULT FASTCALL PurgeOptions_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, PurgeOptions_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, PurgeOptions_OnCommand );
      HANDLE_MSG( hwnd, WM_NOTIFY, PurgeOptions_OnNotify );

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL PurgeOptions_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPCAMPROPSHEETPAGE psp;
   LPVOID pData;

   /* Dereference and save the handle of the database, folder
    * or topic whose purge settings we're showing.
    */
   psp = (LPCAMPROPSHEETPAGE)lParam;
   pData = (LPVOID)psp->lParam;
   SetWindowLong( hwnd, DWL_USER, (LPARAM)pData );
   InBasket_Prop_ShowTitle( hwnd, pData );

   /* If pData is a topic, get and show the topic purge
    * settings directly.
    */
   PurgeOptions_ShowSettings( hwnd, pData );
   fPurgeChanged = FALSE;
   return( FALSE );
}

/* This function shows the purge settings for the specified
 * folder.
 */
void FASTCALL PurgeOptions_ShowSettings( HWND hwnd, LPVOID pData )
{
   TOPICINFO ti;

   ti.po.wMaxSize = 0;
   ti.po.wMaxDate = 0;
   if( Amdb_IsTopicPtr( pData ) )
      {
      HWND hwndCheck;

      /* Make sure the enable purge and backup checkboxes are NOT tri-state.
       */
      hwndCheck = GetDlgItem( hwnd, IDD_PURGE_ON );
      SetWindowStyle( hwndCheck, ( ( GetWindowStyle( hwndCheck ) & ~0xFF ) & ~BS_AUTO3STATE ) | BS_AUTOCHECKBOX );
      hwndCheck = GetDlgItem( hwnd, IDD_STORE );
      SetWindowStyle( hwndCheck, ( ( GetWindowStyle( hwndCheck ) & ~0xFF ) & ~BS_AUTO3STATE ) | BS_AUTOCHECKBOX );

      /* Get and show purge options.
       */
      Amdb_GetTopicInfo( (PTL)pData, &ti );
      WritePurgeOptions( hwnd, &ti.po, FALSE );
      }
   else if( Amdb_IsFolderPtr( pData ) )
      {
      FOLDERINFO fi;
      HWND hwndCheck;

      /* Make sure the enable purge and backup checkboxes are NOT tri-state.
       */
      hwndCheck = GetDlgItem( hwnd, IDD_PURGE_ON );
      SetWindowStyle( hwndCheck, ( ( GetWindowStyle( hwndCheck ) & ~0xFF ) & ~BS_AUTO3STATE ) | BS_AUTOCHECKBOX );
      hwndCheck = GetDlgItem( hwnd, IDD_STORE );
      SetWindowStyle( hwndCheck, ( ( GetWindowStyle( hwndCheck ) & ~0xFF ) & ~BS_AUTO3STATE ) | BS_AUTOCHECKBOX );

      /* Get and show purge options.
       */
      Amdb_GetFolderInfo( (PCL)pData, &fi );
      WritePurgeOptions( hwnd, &fi.po, FALSE );
      }
   else
      {
      HWND hwndCheck;
      HWND hwndSpin;
      int nEnable;
      int nSizeLimit;
      int nDateLimit;
      int nDeleteOnly;
      int nIgnoredOnly;
      int nStore;
      int count;
      PCL pcl;
      PTL ptl;

      /* Count the settings for each topic.
       */
      nEnable = 0;
      nSizeLimit = 0;
      nDateLimit = 0;
      nDeleteOnly = 0;
      nIgnoredOnly = 0;
      nStore = 0;
      count = 0;

      /* Make the enable purge and backup checkboxes tri-state.
       */
      hwndCheck = GetDlgItem( hwnd, IDD_PURGE_ON );
      SetWindowStyle( hwndCheck, ( GetWindowStyle( hwndCheck ) & ~0xFF ) | BS_AUTO3STATE );
      hwndCheck = GetDlgItem( hwnd, IDD_STORE );
      SetWindowStyle( hwndCheck, ( GetWindowStyle( hwndCheck ) & ~0xFF ) | BS_AUTO3STATE );

      /* Get settings
       */
      for( pcl = Amdb_GetFirstFolder( (PCAT)pData ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
         for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
            {
            Amdb_GetTopicInfo( ptl, &ti );
            nEnable |= !( ti.po.wFlags & PO_NOPURGE ) ? 2 : 1;
            nSizeLimit += ( ti.po.wFlags & PO_SIZEPRUNE ) ? 1 : 0;
            nDateLimit += ( ti.po.wFlags & PO_DATEPRUNE ) ? 1 : 0;
            nDeleteOnly += ( ti.po.wFlags & PO_DELETEONLY ) ? 1 : 0;
            nIgnoredOnly += ( ti.po.wFlags & PO_PURGE_IGNORED ) ? 1 : 0;
            nStore |= ( ti.po.wFlags & PO_BACKUP ) ? 2 : 1;
            ++count;
            }
      fInitingDialog = TRUE;
      CheckDlgButton( hwnd, IDD_PURGE_ON, nEnable - 1 );
      CheckDlgButton( hwnd, IDD_MAINTAIN, FALSE );
      CheckDlgButton( hwnd, IDD_OLDER, FALSE );
      CheckDlgButton( hwnd, IDD_DELETEONLY, FALSE );
      CheckDlgButton( hwnd, IDD_IGNOREDONLY, FALSE );
      if( nSizeLimit == count )
         {
         SetDlgItemInt( hwnd, IDD_MAINTAIN_EDIT, ti.po.wMaxSize, FALSE );
         CheckDlgButton( hwnd, IDD_MAINTAIN, TRUE );
         SetFocus( GetDlgItem( hwnd, IDD_MAINTAIN ) );
         }
      else if( nDateLimit == count )
         {
         SetDlgItemInt( hwnd, IDD_OLDER_EDIT, ti.po.wMaxDate, FALSE );
         CheckDlgButton( hwnd, IDD_OLDER, TRUE );
         SetFocus( GetDlgItem( hwnd, IDD_OLDER ) );
         }
      else if( nDeleteOnly == count )
         {
         CheckDlgButton( hwnd, IDD_DELETEONLY, TRUE );
         SetFocus( GetDlgItem( hwnd, IDD_DELETEONLY ) );
         }
      else if( nIgnoredOnly == count )
         {
         CheckDlgButton( hwnd, IDD_IGNOREDONLY, TRUE );
         SetFocus( GetDlgItem( hwnd, IDD_IGNOREDONLY ) );
         }
      CheckDlgButton( hwnd, IDD_STORE, nStore - 1 );
      Edit_LimitText( GetDlgItem( hwnd, IDD_MAINTAIN_EDIT ), 4 );
      Edit_LimitText( GetDlgItem( hwnd, IDD_OLDER_EDIT ), 3 );

      hwndSpin = GetDlgItem( hwnd, IDD_SPIN );
      SendMessage( hwndSpin, UDM_SETBUDDY, (WPARAM)GetDlgItem( hwnd, IDD_MAINTAIN_EDIT ), 0L );
      SendMessage( hwndSpin, UDM_SETRANGE, 0, MAKELPARAM( 9999, 0 ) );
      SendMessage( hwndSpin, UDM_SETPOS, 0, MAKELPARAM( ti.po.wMaxSize, 0 ) );

      hwndSpin = GetDlgItem( hwnd, IDD_SPIN2 );
      SendMessage( hwndSpin, UDM_SETBUDDY, (WPARAM)GetDlgItem( hwnd, IDD_OLDER_EDIT ), 0L );
      SendMessage( hwndSpin, UDM_SETRANGE, 0, MAKELPARAM( 999, 0 ) );
      SendMessage( hwndSpin, UDM_SETPOS, 0, MAKELPARAM( ti.po.wMaxDate, 0 ) );
      fInitingDialog = FALSE;
      }

   /* Update the 'Last purged on...' info.
    */
   UpdateLastPurgeInfo( hwnd, pData );
}

/* This function updates the 'last purged on' information in
 * the dialog.
 */
void FASTCALL UpdateLastPurgeInfo( HWND hwnd, LPVOID pFolder )
{
   if( Amdb_IsTopicPtr( (PTL)pFolder ) )
      {
      TOPICINFO ti;

      Amdb_GetTopicInfo( (PTL)pFolder, &ti );
      ShowLastPurgeInfo( hwnd, "topic", ti.wLastPurgeDate, ti.wLastPurgeTime );
      }
   else if( Amdb_IsFolderPtr( (PCL)pFolder ) )
      {
      FOLDERINFO ci;

      Amdb_GetFolderInfo( (PCL)pFolder, &ci );
      ShowLastPurgeInfo( hwnd, "folder", ci.wLastPurgeDate, ci.wLastPurgeTime );
      }
}

/* This function writes a description of when the topic was last
 * purged.
 */
void FASTCALL ShowLastPurgeInfo( HWND hwnd, char * pDesc, WORD wLastPurgeDate, WORD wLastPurgeTime )
{
   char sz[ 100 ];

   if( 0 == wLastPurgeDate )
      wsprintf( sz, GS(IDS_STR1156), pDesc );
   else
      wsprintf( sz, GS(IDS_STR1157), pDesc, Amdate_FriendlyDate( NULL, wLastPurgeDate, wLastPurgeTime ) );
   SetDlgItemText( hwnd, IDD_LASTPURGED, sz );
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL PurgeOptions_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsIBP_PURGE );
         break;

      case PSN_SETACTIVE:
         nLastPropertiesPage = IPP_PURGE;
         break;

      case PSN_APPLY: {
         LPVOID pFolder;

         pFolder = (LPVOID)GetWindowLong( hwnd, DWL_USER );
         if( fPurgeChanged )
            if( !ApplyPurgeChanges( hwnd, pFolder, TRUE ) )
               return( PSNRET_INVALID_NOCHANGEPAGE );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This function applies the changes to the Purge properties.
 */
BOOL FASTCALL ApplyPurgeChanges( HWND hwnd, LPVOID pFolder, BOOL fOfferForFolders )
{
   PCL pcl;
   PTL ptl;

   if( Amdb_IsTopicPtr( (PTL)pFolder ) )
      {
      PURGEOPTIONS po;

      /* Set purge options for an individual topic.
       */
      memset( &po, 0, sizeof(PURGEOPTIONS) );
      if( !ReadPurgeOptions( hwnd, &po, FALSE ) )
         return( FALSE );
      Amdb_SetTopicPurgeOptions( (PTL)pFolder, &po );
      }
   else if( Amdb_IsFolderPtr( (PCL)pFolder ) )
      {
      FOLDERINFO fi;
      PURGEOPTIONS po;

      /* Set purge options for an individual topic.
       */
      memset( &po, 0, sizeof(PURGEOPTIONS) );
      if( !ReadPurgeOptions( hwnd, &po, FALSE ) )
         return( FALSE );
      Amdb_GetFolderInfo( (PCL)pFolder, &fi );
      Amdb_SetFolderPurgeOptions( (PCL)pFolder, &po );
      if( memcmp( &fi.po, &po, sizeof(PURGEOPTIONS) ) != 0 )
         {
         BOOL fOk;

         fOk = TRUE;
         if( fOfferForFolders )
            fOk = fMessageBox( hwnd, 0, GS(IDS_STR1139), MB_YESNO|MB_ICONQUESTION ) == IDYES;
         if( fOk )
            {
            for( ptl = Amdb_GetFirstTopic( (PCL)pFolder ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
               {
               Amdb_GetTopicPurgeOptions( ptl, &po );
               ReadPurgeOptions( hwnd, &po, FALSE );
               Amdb_SetTopicPurgeOptions( ptl, &po );
               }
            }
         }
      }
   else
      {
      PURGEOPTIONS po;

      /* Set purge options for all topics in all folders
       * in the selected database.
       */
      memset( &po, 0, sizeof(PURGEOPTIONS) );
      if( fOfferForFolders )
         if( IDNO == fMessageBox( hwnd, 0, GS(IDS_STR1185), MB_YESNO|MB_ICONQUESTION ) )
            return( FALSE );
      if( !ReadPurgeOptions( hwnd, &po, FALSE ) )
         return( FALSE );
      for( pcl = Amdb_GetFirstFolder( (PCAT)pFolder ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
         {
         Amdb_GetFolderPurgeOptions( pcl, &po );
         ReadPurgeOptions( hwnd, &po, FALSE );
         Amdb_SetFolderPurgeOptions( pcl, &po );
         for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
            {
            Amdb_GetTopicPurgeOptions( ptl, &po );
            ReadPurgeOptions( hwnd, &po, FALSE );
            Amdb_SetTopicPurgeOptions( ptl, &po );
            }
         }
      }

   /* Force the Apply button to be disabled.
    */
   fPurgeChanged = FALSE;
   PropSheet_UnChanged( GetParent( hwnd ), hwnd );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL PurgeOptions_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_DEFAULT:
         WritePurgeOptions( hwnd, &poDef, TRUE );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         fPurgeChanged = TRUE;
         break;

      case IDD_PURGE: {
         LPVOID pFolder;

         pFolder = (LPVOID)GetWindowLong( hwnd, DWL_USER );
         if( ApplyPurgeChanges( hwnd, pFolder, TRUE ) )
            {
            Purge( GetParent( hwnd ), pFolder, FALSE );
            UpdateLastPurgeInfo( hwnd, pFolder );
            }
         break;
         }

      case IDD_MAINTAIN:
      case IDD_OLDER:
      case IDD_STORE:
      case IDD_DELETEONLY:
      case IDD_IGNOREDONLY:
         fPurgeChanged = TRUE;
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_PURGE_ON: {
         LPVOID ptl;
         PURGEOPTIONS po;

            
         ptl = (LPVOID)GetWindowLong( hwnd, DWL_USER );
         if( Amdb_IsTopicPtr( ptl ) || Amdb_IsFolderPtr( ptl ) )
            {
            if( !ReadPurgeOptions( hwnd, &po, FALSE ) )
               break;
            WritePurgeOptions( hwnd, &po, FALSE );
            }
         else if( Amdb_IsCategoryPtr( ptl ) )
            {
            if( !ReadPurgeOptions( hwnd, &po, TRUE ) )
               break;
            WritePurgeOptions( hwnd, &po, TRUE );
            }
         fPurgeChanged = TRUE;
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
         }

      case IDD_MAINTAIN_EDIT:
      case IDD_OLDER_EDIT:
/*
         if( !fInitingDialog && ( codeNotify == EN_CHANGE ) ) //!!SM!! 2.56.2061
         {
            CheckDlgButton( hwnd, IDD_MAINTAIN, id == IDD_MAINTAIN_EDIT );
            CheckDlgButton( hwnd, IDD_OLDER, id == IDD_OLDER_EDIT );
            CheckDlgButton( hwnd, IDD_DELETEONLY, FALSE );
            CheckDlgButton( hwnd, IDD_IGNOREDONLY, FALSE );
            fPurgeChanged = TRUE;
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         }
         if( !fInitingDialog && ( codeNotify == EN_UPDATE )  && 
            (  EditMap_GetModified( GetDlgItem( hwnd, IDD_MAINTAIN_EDIT ) ) || EditMap_GetModified( GetDlgItem( hwnd, IDD_OLDER_EDIT ) ) ) )
*/
         if( codeNotify == EN_UPDATE && !fInitingDialog && (  EditMap_GetModified( GetDlgItem( hwnd, IDD_MAINTAIN_EDIT ) ) || EditMap_GetModified( GetDlgItem( hwnd, IDD_OLDER_EDIT ) ) ) )
            {
            CheckDlgButton( hwnd, IDD_MAINTAIN, id == IDD_MAINTAIN_EDIT );
            CheckDlgButton( hwnd, IDD_OLDER, id == IDD_OLDER_EDIT );
            CheckDlgButton( hwnd, IDD_DELETEONLY, FALSE );
            CheckDlgButton( hwnd, IDD_IGNOREDONLY, FALSE );
            fPurgeChanged = TRUE;
            PropSheet_Changed( GetParent( hwnd ), hwnd );
            }
         break;
      }
}

/* This function reads the purge settings from the dialog box and
 * stores them in the PURGEOPTIONS structure.
 */
BOOL FASTCALL ReadPurgeOptions( HWND hwnd, PURGEOPTIONS * npo, BOOL fGlobal )
{
   int wMaxDate;
   int wMaxSize;

   if( !fGlobal )
      if( IsDlgButtonChecked( hwnd, IDD_PURGE_ON ) != 2 )
         {
         if( IsDlgButtonChecked( hwnd, IDD_PURGE_ON ) )
            npo->wFlags &= ~PO_NOPURGE;
         else
            npo->wFlags |= PO_NOPURGE;
         }
   if( !GetDlgInt( hwnd, IDD_MAINTAIN_EDIT, &wMaxSize, 0, 9999 ) )
      return( FALSE );
   if( !GetDlgInt( hwnd, IDD_OLDER_EDIT, &wMaxDate, 0, 999 ) )
      return( FALSE );
   npo->wMaxDate = wMaxDate;
   npo->wMaxSize = wMaxSize;
   if( IsDlgButtonChecked( hwnd, IDD_DELETEONLY ) == 1 )
      {
      npo->wFlags &= ~( PO_DELETEONLY|PO_DATEPRUNE|PO_SIZEPRUNE|PO_PURGE_IGNORED );
      if( IsDlgButtonChecked( hwnd, IDD_DELETEONLY ) )
         npo->wFlags |= PO_DELETEONLY;
      else
         npo->wFlags &= ~PO_DELETEONLY;
      }
   if( IsDlgButtonChecked( hwnd, IDD_IGNOREDONLY ) == 1 )
      {
      npo->wFlags &= ~( PO_DELETEONLY|PO_DATEPRUNE|PO_SIZEPRUNE|PO_PURGE_IGNORED );
      if( IsDlgButtonChecked( hwnd, IDD_IGNOREDONLY ) )
         npo->wFlags |= PO_PURGE_IGNORED ;
      else
         npo->wFlags &= ~PO_PURGE_IGNORED;
      }
   if( IsDlgButtonChecked( hwnd, IDD_OLDER ) == 1 )
      {
      npo->wFlags &= ~( PO_DELETEONLY|PO_DATEPRUNE|PO_SIZEPRUNE|PO_PURGE_IGNORED );
      if( IsDlgButtonChecked( hwnd, IDD_OLDER ) )
         npo->wFlags |= PO_DATEPRUNE;
      else
         npo->wFlags &= ~PO_DATEPRUNE;
      }
   if( IsDlgButtonChecked( hwnd, IDD_MAINTAIN ) == 1 )
      {
      npo->wFlags &= ~( PO_DELETEONLY|PO_DATEPRUNE|PO_SIZEPRUNE|PO_PURGE_IGNORED );
      if( IsDlgButtonChecked( hwnd, IDD_MAINTAIN ) )
         npo->wFlags |= PO_SIZEPRUNE;
      else
         npo->wFlags &= ~PO_SIZEPRUNE;
      }
   if( IsDlgButtonChecked( hwnd, IDD_STORE ) != 2 )
      {
      if( IsDlgButtonChecked( hwnd, IDD_STORE ) )
         npo->wFlags |= PO_BACKUP;
      else
         npo->wFlags &= ~PO_BACKUP;
      }
   return( TRUE );
}

void FASTCALL WritePurgeOptions( HWND hwnd, PURGEOPTIONS * npo, BOOL fGlobal )
{
   BOOL fPurge;
   HWND hwndSpin;

   fInitingDialog = TRUE;
   if( !fGlobal )
      CheckDlgButton( hwnd, IDD_PURGE_ON, !( npo->wFlags & PO_NOPURGE ) );
   CheckDlgButton( hwnd, IDD_MAINTAIN, npo->wFlags & PO_SIZEPRUNE );
   CheckDlgButton( hwnd, IDD_OLDER, npo->wFlags & PO_DATEPRUNE );
   CheckDlgButton( hwnd, IDD_DELETEONLY, npo->wFlags & PO_DELETEONLY );
   CheckDlgButton( hwnd, IDD_IGNOREDONLY, npo->wFlags & PO_PURGE_IGNORED );
   SetDlgItemInt( hwnd, IDD_MAINTAIN_EDIT, npo->wMaxSize, FALSE );
   SetDlgItemInt( hwnd, IDD_OLDER_EDIT, npo->wMaxDate, FALSE );
   Edit_LimitText( GetDlgItem( hwnd, IDD_MAINTAIN_EDIT ), 4 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_OLDER_EDIT ), 3 );
   hwndSpin = GetDlgItem( hwnd, IDD_SPIN );
   SendMessage( hwndSpin, UDM_SETBUDDY, (WPARAM)GetDlgItem( hwnd, IDD_MAINTAIN_EDIT ), 0L );
   SendMessage( hwndSpin, UDM_SETRANGE, 0, MAKELPARAM( 9999, 0 ) );
   SendMessage( hwndSpin, UDM_SETPOS, 0, MAKELPARAM( npo->wMaxSize, 0 ) );
   hwndSpin = GetDlgItem( hwnd, IDD_SPIN2 );
   SendMessage( hwndSpin, UDM_SETBUDDY, (WPARAM)GetDlgItem( hwnd, IDD_OLDER_EDIT ), 0L );
   SendMessage( hwndSpin, UDM_SETRANGE, 0, MAKELPARAM( 999, 0 ) );
   SendMessage( hwndSpin, UDM_SETPOS, 0, MAKELPARAM( npo->wMaxDate, 0 ) );
   CheckDlgButton( hwnd, IDD_STORE, npo->wFlags & PO_BACKUP );

   /* Disable the purge controls if purging is disabled.
    */
   if( !fGlobal )
      {
      fPurge = !( npo->wFlags & PO_NOPURGE );
      EnableControl( hwnd, IDD_MAINTAIN, fPurge );
      EnableControl( hwnd, IDD_OLDER, fPurge );
      EnableControl( hwnd, IDD_DELETEONLY, fPurge );
      EnableControl( hwnd, IDD_IGNOREDONLY, fPurge );
      EnableControl( hwnd, IDD_MAINTAIN_EDIT, fPurge );
      EnableControl( hwnd, IDD_OLDER_EDIT, fPurge );
      EnableControl( hwnd, IDD_STORE, fPurge );
      }
   fInitingDialog = FALSE;
}

/* This function carries out the interface to the database PurgeOne()
 * function.
 */
BOOL FASTCALL Purge( HWND hwnd, LPVOID plv, BOOL fPrompt )
{
   PCL pcl;
   PCL pclNext;
   HWND hwndGauge;
   int cErrors;

   /* Prime the variables we'll be using,
    */
   fAbortPurge = FALSE;
   fPurgeCommand = TRUE;
   cErrors = 0;

   /* Exit if purging prohibited.
    */
   if( !TestPerm(PF_CANPURGE) )
   {
      fMessageBox( hwnd, 0, "This user does not have permission to purge (see Users on the Settings menu)", MB_OK|MB_ICONINFORMATION );
      return( FALSE );
   }

   /* Get go-ahead confirmation BEFORE we put up the dialog box.
    */
   if( fPrompt )
      if( Amdb_IsCategoryPtr( plv ) )
         {
         char sz[ 128 ];

         ExplainFolderPurgeSettings( plv, sz );
         wsprintf( lpTmpBuf, GS(IDS_STR729), Amdb_GetCategoryName( plv ), sz );
         if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONINFORMATION ) == IDNO )
            return( FALSE );
         }
      else if( Amdb_IsTopicPtr( plv ) )
         {
         char sz[ 128 ];
         TOPICINFO ti;

         Amdb_GetTopicInfo( (PTL)plv, &ti );
         if( ti.po.wFlags & PO_NOPURGE )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR1134), MB_OK|MB_ICONINFORMATION ) ;
            return( FALSE );
            }
         ExplainPurgeSettings( &ti.po, sz );
         wsprintf( lpTmpBuf, GS(IDS_STR1243), Amdb_GetFolderName( Amdb_FolderFromTopic( (PTL)plv ) ), Amdb_GetTopicName( plv ), sz );
         if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONINFORMATION ) == IDNO )
            return( FALSE );
         }
      else
         {
         char sz[ 128 ];
         BOOL fDisabled;
         PTL ptl;

         /* See if all topics in the folder have purge disabled.
          */
         fDisabled = TRUE;
         for( ptl = Amdb_GetFirstTopic( (PCL)plv ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
            {
            TOPICINFO ti;

            Amdb_GetTopicInfo( ptl, &ti );
            if( !( ti.po.wFlags & PO_NOPURGE ) )
               fDisabled = FALSE;
            }

         /* If so, warn and don't go any further.
          */
         if( fDisabled )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR1134), MB_OK|MB_ICONINFORMATION ) ;
            return( FALSE );
            }
         ExplainFolderPurgeSettings( plv, sz );
         wsprintf( lpTmpBuf, GS(IDS_STR729), Amdb_GetFolderName( plv ), sz );
         if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONINFORMATION ) == IDNO )
            return( FALSE );
         }

   /* Create and show the purge modeless dialog.
    */
   hPurgeDlg = Adm_CreateDialog( hRscLib, hwnd, MAKEINTRESOURCE( IDDLG_PURGING ), (DLGPROC)Purging, 0L );
   ShowWindow( hPurgeDlg, SW_SHOW );
   UpdateWindow( hPurgeDlg );
   EnableWindow( hwnd, FALSE );

   /* Initialise the gauge dialog that keeps track of the
    * progress.
    */
   hwndGauge = GetDlgItem( hPurgeDlg, IDD_GAUGE );
   SendMessage( hwndGauge, PBM_SETPOS, 0, 0L );
   SendMessage( hwndGauge, PBM_SETSTEP, 1, 0L );

   /* Prevent the thread panes from being redrawn.
    */
   SendAllWindows( WM_LOCKUPDATES, WIN_THREAD, (LPARAM)TRUE );

   /* If we're purging a database, then the purge is through every
    * topic in every folder in the database.
    */
   if( Amdb_IsCategoryPtr( plv ) )
      {
      SendMessage( hwndGauge, PBM_SETRANGE, 0, MAKELPARAM( 0, CountTopics( plv ) * 2 ) );
      for( pcl = Amdb_GetFirstFolder( (PCAT)plv ); pcl; pcl = pclNext )
         {
         pclNext = Amdb_GetNextFolder( pcl );
         cErrors += Amdb_PurgeFolder( pcl, PurgeCallback );
         if( fAbortPurge )
            break;
         }
      }

   /* If we're purging a topic, then set the gauge counter to be the
    * number of messages in the topic.
    */
   else if( Amdb_IsTopicPtr( plv ) )
      {
      TOPICINFO topicinfo;
      WPARAM cRange;

      Amdb_GetTopicInfo( plv, &topicinfo );
      cRange = LOWORD( topicinfo.cMsgs ) * 2;
      SendMessage( hwndGauge, PBM_SETRANGE, 0, MAKELPARAM( 0, cRange ) );
      cErrors += Amdb_PurgeTopic( plv, PurgeMsgCallback );
      }

   /* Otherwise we're purging a folder, so set the gauge counter to
    * be the number of topics in the folder.
    */
   else
      {
      SendMessage( hwndGauge, PBM_SETRANGE, 0, MAKELPARAM( 0, CountTopics( plv ) * 2 ) );
      cErrors += Amdb_PurgeFolder( plv, PurgeCallback );
      }

   /* Just in case the counter stops short...
    */
   SendMessage( hwndGauge, PBM_SETPOS, 100, 0L );

   /* All done, kill our progress dialog.
    */
   EnableWindow( hwnd, TRUE );
   DestroyWindow( hPurgeDlg );

   /* Restore the thread pane refresh.
    */
   SendAllWindows( WM_LOCKUPDATES, WIN_THREAD, (LPARAM)FALSE );

   /* Update any open message and inbasket windows, then update
    * the status bar.
    */
   Amuser_CallRegistered( AE_UNREADCHANGED, 0L, 0L );
   if( fNoisy )
      Am_PlaySound( "Purge Complete" );
   return( TRUE );
}

/* This function converts the purge settings for the specified
 * folder into plain English.
 */
void FASTCALL ExplainFolderPurgeSettings( LPVOID pFolder, char * pszBuf )
{
   TOPICINFO topicInfo;

   if( Amdb_IsTopicPtr( (PTL)pFolder ) )
      Amdb_GetTopicInfo( (PTL)pFolder, &topicInfo );
   else if( Amdb_IsFolderPtr( (PCL)pFolder ) )
      {
      PTL ptl;

      /* For folders, look at the purge options for each individual
       * topic and if they are all the same, describe the purge
       * options. Otherwise use a generic message.
       */
      ptl = Amdb_GetFirstTopic( (PCL)pFolder );
      Amdb_GetTopicInfo( ptl, &topicInfo );
      while( ptl = Amdb_GetNextTopic( ptl ) )
         {
         TOPICINFO topicInfo2;

         Amdb_GetTopicInfo( ptl, &topicInfo2 );
         if( memcmp( &topicInfo2.po, &topicInfo.po, sizeof(PURGEOPTIONS) ) != 0 )
            break;
         }
      if( NULL != ptl )
         {
         strcpy( pszBuf, GS(IDS_STR1138) );
         return;
         }
      }
   else
      {
      /* Database always shows a generic message.
       */
      strcpy( pszBuf, GS(IDS_STR1138) );
      return;
      }
   ExplainPurgeSettings( &topicInfo.po, pszBuf );
}

/* This function converts the purge settings in ppo into
 * plain English.
 */
void FASTCALL ExplainPurgeSettings( PURGEOPTIONS * ppo, char * pszBuf )
{
   if( ppo->wFlags & PO_DELETEONLY )
      strcpy( pszBuf, GS(IDS_STR1135) );
   else if( ppo->wFlags & PO_SIZEPRUNE )
      wsprintf( pszBuf, GS(IDS_STR1136), ppo->wMaxSize );
   else if( ppo->wFlags & PO_PURGE_IGNORED )
      strcpy( pszBuf, GS(IDS_STR1228) );
   else
      wsprintf( pszBuf, GS(IDS_STR1137), ppo->wMaxDate );
}

/* This function computes the total number of topics in
 * a folder.
 */
WORD FASTCALL CountTopics( LPVOID pFolder )
{
   if( Amdb_IsCategoryPtr( (PCAT)pFolder ) )
      return( CountTopicsInCategory( (PCAT)pFolder ) );
   if( Amdb_IsFolderPtr( (PCL)pFolder ) )
      return( CountTopicsInFolder( (PCL)pFolder ) );
   ASSERT( Amdb_IsTopicPtr( (PTL)pFolder ) );
   return( 1 );
}

/* This function computes the total number of topics in
 * a category.
 */
WORD FASTCALL CountTopicsInCategory( PCAT pcat )
{
   WORD cTopics;
   PCL pcl;

   cTopics = 0;
   for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
      {
      FOLDERINFO folderinfo;

      Amdb_GetFolderInfo( pcl, &folderinfo );
      cTopics += folderinfo.cTopics;
      }
   return( cTopics );
}

/* This function returns the number of topics in a folder.
 */
WORD FASTCALL CountTopicsInFolder( PCL pcl )
{
   FOLDERINFO folderinfo;

   Amdb_GetFolderInfo( pcl, &folderinfo );
   return( folderinfo.cTopics );
}

/* This function handles the dialog box messages passed to the Purging
 * dialog.
 */
BOOL EXPORT FAR PASCAL Purging( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, Purging_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the Purging
 * dialog.
 */
LRESULT FASTCALL Purging_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, Purging_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, Purging_OnCommand );

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL Purging_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   if( id == IDCANCEL )
      fAbortPurge = TRUE;
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL Purging_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   return( TRUE );
}

/* This function is called repeatedly during purging so that we can keep
 * the user posted on events.
 */
BOOL EXPORT CALLBACK PurgeCallback( int iType, PURGECALLBACKINFO FAR * lpPci )
{
   switch( iType )
      {
      case PCI_COMPACTING:
      case PCI_PURGING: {
         HWND hwndGauge;

         hwndGauge = GetDlgItem( hPurgeDlg, IDD_GAUGE );
         SendMessage( hwndGauge, PBM_STEPIT, 0, 0L );
         SetDlgItemText( hPurgeDlg, IDD_FOLDERNAME, Amdb_GetTopicName( lpPci->ptl ) );
         SetDlgItemText( hPurgeDlg, IDD_CATEGORYNAME, Amdb_GetCategoryName ( Amdb_CategoryFromFolder ( Amdb_FolderFromTopic( lpPci->ptl ) ) ) );
         SetDlgItemText( hPurgeDlg, IDD_FOLDER, Amdb_GetFolderName( Amdb_FolderFromTopic( lpPci->ptl ) ) );
         SendMessage( hwndInBasket, WM_UPDATE_UNREAD, 0, (LPARAM)lpPci->ptl );
         SendAllWindows( WM_UPDATE_UNREAD, 0, (LPARAM)lpPci->ptl );
         break;
         }

      case PCI_YIELD:
         TaskYield();
         if( fAbortPurge )
            return( FALSE );
         break;
      }
   return( TRUE );
}

/* This function is called repeatedly during compacting so that we can keep
 * the user posted on events.
 */
BOOL EXPORT CALLBACK CompactCallback( int iType, PURGECALLBACKINFO FAR * lpPci )
{
   switch( iType )
      {
      case PCI_COMPACTING: {
         HWND hwndGauge;

         hwndGauge = GetDlgItem( hPurgeDlg, IDD_GAUGE );
         SendMessage( hwndGauge, PBM_STEPIT, 0, 0L );
         SetDlgItemText( hPurgeDlg, IDD_FOLDERNAME, Amdb_GetTopicName( lpPci->ptl ) );
         SetDlgItemText( hPurgeDlg, IDD_CATEGORYNAME, Amdb_GetCategoryName ( Amdb_CategoryFromFolder ( Amdb_FolderFromTopic( lpPci->ptl ) ) ) );
         SetDlgItemText( hPurgeDlg, IDD_FOLDER, Amdb_GetFolderName( Amdb_FolderFromTopic( lpPci->ptl ) ) );
         SendMessage( hwndInBasket, WM_UPDATE_UNREAD, 0, (LPARAM)lpPci->ptl );
         SendAllWindows( WM_UPDATE_UNREAD, 0, (LPARAM)lpPci->ptl );
         break;
         }

      case PCI_YIELD:
         TaskYield();
         if( fAbortPurge )
            return( FALSE );
         break;
      }
   return( TRUE );
}

/* This function is called repeatedly during purging so that we can keep
 * the user posted on events.
 */
BOOL EXPORT CALLBACK PurgeMsgCallback( int iType, PURGECALLBACKINFO FAR * lpPci )
{
   static DWORD cMsgsAtStart;

   switch( iType )
      {
      case PCI_COMPACTING:
      case PCI_PURGING:
         cMsgsAtStart = lpPci->cMsgs;
         SetDlgItemText( hPurgeDlg, IDD_FOLDERNAME, Amdb_GetTopicName( lpPci->ptl ) );
         SetDlgItemText( hPurgeDlg, IDD_CATEGORYNAME, Amdb_GetCategoryName ( Amdb_CategoryFromFolder ( Amdb_FolderFromTopic( lpPci->ptl ) ) ) );
         SetDlgItemText( hPurgeDlg, IDD_FOLDER, Amdb_GetFolderName( Amdb_FolderFromTopic( lpPci->ptl ) ) );
         break;

      case PCI_YIELD: {
         HWND hwndGauge;

         hwndGauge = GetDlgItem( hPurgeDlg, IDD_GAUGE );
         SendMessage( hwndGauge, PBM_STEPIT, 0, 0L );
         TaskYield();
         if( fAbortPurge )
            return( FALSE );
         break;
         }
      }
   return( TRUE );
}

/* This function displays the Purge Settings dialog.
 */
BOOL FASTCALL CmdPurgeSettings( HWND hwnd )
{
   LPPURGESETTINGSWNDINFO lppli;
   HWND hwndFocus;
   DWORD dwState;
   RECT rc;

   /* Get the current purge settings window for this folder
    * if one exists.
    */
   if( NULL == hwndPurgeSettings )
      {
      /* Register the group list window class if we have
       * not already done so.
       */
      if( !fRegistered )
         {
         WNDCLASS wc;

         wc.style          = CS_HREDRAW | CS_VREDRAW;
         wc.lpfnWndProc    = PurgeSettingsWndProc;
         wc.hIcon          = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_PURGESETTINGS) );
         wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
         wc.lpszMenuName   = NULL;
         wc.cbWndExtra     = DLGWINDOWEXTRA;
         wc.cbClsExtra     = 0;
         wc.hbrBackground  = (HBRUSH)( COLOR_APPWORKSPACE + 1 );
         wc.lpszClassName  = szPurgeSettingsWndClass;
         wc.hInstance      = hInst;
         if( !RegisterClass( &wc ) )
            return( FALSE );
         fRegistered = TRUE;
         }

      /* The window will appear in the left half of the
       * client area.
       */
      GetClientRect( hwndMDIClient, &rc );
      InflateRect( &rc, -10, -10 );
      dwState = 0;

      /* Get the actual window size, position and state.
       */
      ReadProperWindowState( szPurgeSettingsWndName, &rc, &dwState );

      /* Create the window.
       */
      hwndPurgeSettings = Adm_CreateMDIWindow( szPurgeSettingsWndName, szPurgeSettingsWndClass, hInst, &rc, dwState, 0L );
      if( NULL == hwndPurgeSettings )
         return( FALSE );
      }

   /* Show the window.
    */
   if( IsIconic( hwndPurgeSettings ) )
      SendMessage( hwndMDIClient, WM_MDIRESTORE, (WPARAM)hwndPurgeSettings, 0L );
   SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndPurgeSettings, 0L );
   UpdateWindow( hwndPurgeSettings );

   /* Determine where we put the focus.
    */
   VERIFY( hwndFocus = GetDlgItem( hwndPurgeSettings, IDD_FOLDERLIST ) );
   SetFocus( hwndFocus );

   /* Store the handle of the focus window.
    */
   lppli = GetPurgeSettingsWndInfo( hwndPurgeSettings );
   lppli->hwndFocus = hwndFocus;
   return( TRUE );
}

/* This is the main window procedure.
 */
LRESULT EXPORT CALLBACK PurgeSettingsWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, PurgeSettings_OnCreate );
      HANDLE_MSG( hwnd, WM_CLOSE, PurgeSettings_OnClose );
      HANDLE_MSG( hwnd, WM_SIZE, PurgeSettings_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, PurgeSettings_OnMove );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, Common_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, PurgeSettings_MDIActivate );
      HANDLE_MSG( hwnd, WM_COMMAND, PurgeSettings_OnCommand );
      HANDLE_MSG( hwnd, WM_NOTIFY, PurgeSettings_OnNotify );
      HANDLE_MSG( hwnd, WM_DRAWITEM, PurgeSettings_OnDrawItem );
      HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, PurgeSettings_OnAdjustWindows );
      HANDLE_MSG( hwnd, WM_INSERTCATEGORY, InBasket_OnInsertCategory );
      HANDLE_MSG( hwnd, WM_INSERTFOLDER, InBasket_OnInsertFolder );
      HANDLE_MSG( hwnd, WM_INSERTTOPIC, InBasket_OnInsertTopic );
      HANDLE_MSG( hwnd, WM_DELETECATEGORY, InBasket_OnDeleteCategory );
      HANDLE_MSG( hwnd, WM_DELETEFOLDER, InBasket_OnDeleteFolder );
      HANDLE_MSG( hwnd, WM_DELETETOPIC, InBasket_OnDeleteTopic );

      case WM_GETMINMAXINFO: {
         MINMAXINFO FAR * lpMinMaxInfo;
         LRESULT lResult;

         /* Set the minimum tracking size so the client window can never
          * be resized below 24 lines.
          */
         lResult = Adm_DefMDIDlgProc( hwnd, message, wParam, lParam );
         lpMinMaxInfo = (MINMAXINFO FAR *)lParam;
         lpMinMaxInfo->ptMinTrackSize.x = 420;
         lpMinMaxInfo->ptMinTrackSize.y = 300;
         return( lResult );
         }

      default:
         break;
      }
   return( Adm_DefMDIDlgProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL PurgeSettings_OnCreate( HWND hwnd, LPCREATESTRUCT lpCreateStruct )
{
   LPMDICREATESTRUCT lpMDICreateStruct;
   LPPURGESETTINGSWNDINFO lppli;
   HWND hwndTreeCtl;
   HIMAGELIST himl;

   INITIALISE_PTR(lppli);

   /* Create the dialog window.
    */
   lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
   Adm_MDIDialog( hwnd, MAKEINTRESOURCE(IDDLG_PURGESETTINGS), lpMDICreateStruct );

   /* Create a structure to store the mail window info.
    */
   if( !fNewMemory( &lppli, sizeof(PURGESETTINGSWNDINFO) ) )
      return( FALSE );
   SetPurgeSettingsWndInfo( hwnd, lppli );

   /* Create the image list.
    */
   hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
   himl = Amdb_CreateFolderImageList();
   if( himl )
      TreeView_SetImageList( hwndTreeCtl, himl, 0 );

   /* Fill list with in-basket contents.
    */
   Amdb_FillFolderTree( hwndTreeCtl, FTYPE_ALL|FTYPE_TOPICS, TRUE );
   fPurgeChanged = FALSE;
   return( TRUE );
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL PurgeSettings_OnAdjustWindows( HWND hwnd, int dx, int dy )
{
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_FOLDERLIST ), dx, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_PURGE ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDOK ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_HELP ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_PAD1 ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_PAD2 ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_PAD3 ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_PURGE_ON ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_DEFAULT ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_MAINTAIN ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_STORE ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_OLDER ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_DELETEONLY ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_IGNOREDONLY ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_MAINTAIN_EDIT ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_OLDER_EDIT ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_SPIN ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_SPIN2 ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_LASTPURGED ), 0, dy );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL PurgeSettings_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPURGESETTINGS );
         break;

      case IDD_DEFAULT:
         WritePurgeOptions( hwnd, &poDef, TRUE );
         fPurgeChanged = TRUE;
         PurgeSettings_Update( hwnd );
         break;

      case IDD_PURGE: {
         HTREEITEM hSelItem;
         HWND hwndTreeCtl;
         TV_ITEM tv;

         VERIFY( hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST ) );
         hSelItem = TreeView_GetSelection( hwndTreeCtl );
         ASSERT( 0L != hSelItem );
         tv.hItem = hSelItem;
         tv.mask = TVIF_PARAM;
         VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
         if( fPurgeChanged )
            if( !ApplyPurgeChanges( hwnd, (LPVOID)tv.lParam, TRUE ) )
               break;
         Purge( GetParent( hwnd ), (LPVOID)tv.lParam, FALSE );
         UpdateLastPurgeInfo( hwnd, (LPVOID)tv.lParam );
         break;
         }

      case IDD_PURGE_ON: {
         HTREEITEM hSelItem;
         HWND hwndTreeCtl;
         TV_ITEM tv;
         PURGEOPTIONS po;

         VERIFY( hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST ) );
         hSelItem = TreeView_GetSelection( hwndTreeCtl );
         ASSERT( 0L != hSelItem );
         tv.hItem = hSelItem;
         tv.mask = TVIF_PARAM;
         VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
         if( Amdb_IsTopicPtr( (PTL)tv.lParam ) )
            {
            if( !ReadPurgeOptions( hwnd, &po, FALSE ) )
               break;
            WritePurgeOptions( hwnd, &po, FALSE );
            }
         fPurgeChanged = TRUE;
         PurgeSettings_Update( hwnd );
         break;
         }
      case IDD_STORE:
      case IDD_MAINTAIN:
      case IDD_OLDER:
      case IDD_DELETEONLY:
      case IDD_IGNOREDONLY:
         fPurgeChanged = TRUE;
         PurgeSettings_Update( hwnd );
         break;

      case IDD_MAINTAIN_EDIT:
      case IDD_OLDER_EDIT:
/*    
         if( !fInitingDialog && ( codeNotify == EN_CHANGE ) ) //!!SM!! 2.56.2061
         {
            CheckDlgButton( hwnd, IDD_MAINTAIN, id == IDD_MAINTAIN_EDIT );
            CheckDlgButton( hwnd, IDD_OLDER, id == IDD_OLDER_EDIT );
            CheckDlgButton( hwnd, IDD_DELETEONLY, FALSE );
            CheckDlgButton( hwnd, IDD_IGNOREDONLY, FALSE );
            fPurgeChanged = TRUE;
            PurgeSettings_Update( hwnd );
         }
*/
         if( codeNotify == EN_UPDATE && !fInitingDialog && (  EditMap_GetModified( GetDlgItem( hwnd, IDD_MAINTAIN_EDIT ) ) || EditMap_GetModified( GetDlgItem( hwnd, IDD_OLDER_EDIT ) ) ) )
         {
            CheckDlgButton( hwnd, IDD_MAINTAIN, id == IDD_MAINTAIN_EDIT );
            CheckDlgButton( hwnd, IDD_OLDER, id == IDD_OLDER_EDIT );
            CheckDlgButton( hwnd, IDD_DELETEONLY, FALSE );
            CheckDlgButton( hwnd, IDD_IGNOREDONLY, FALSE );
            fPurgeChanged = TRUE;
            PurgeSettings_Update( hwnd );
         }
         break;

      case IDOK:
      case IDCANCEL:
         /* Close the purge settings window.
          */
         PurgeSettings_OnClose( hwnd );
         break;
      }
}

/* This function handles the WM_CLOSE message.
 */
void FASTCALL PurgeSettings_OnClose( HWND hwnd )
{
   HTREEITEM hSelItem;
   HWND hwndTreeCtl;
   TV_ITEM tv;
   HIMAGELIST himl;

   /* Get the selected folder.
    */
   VERIFY( hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST ) );
   hSelItem = TreeView_GetSelection( hwndTreeCtl );
   ASSERT( 0L != hSelItem );
   tv.hItem = hSelItem;
   tv.mask = TVIF_PARAM;
   VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );

   /* Apply any outstanding changes.
    */
   if( fPurgeChanged )
      if( !ApplyPurgeChanges( hwnd, (LPVOID)tv.lParam, TRUE ) )
         return;

   himl = TreeView_GetImageList(hwndTreeCtl, 0); // 20060325 - 2.56.2049.20
   if ( himl )
      Amctl_ImageList_Destroy( himl );

   /* Destroy window.
    */
   hwndPurgeSettings = NULL;
   Adm_DestroyMDIWindow( hwnd );
}

/* The purge settings have been changed so read the settings
 * and update the treeview control.
 */
void FASTCALL PurgeSettings_Update( HWND hwnd )
{
   HTREEITEM hSelItem;
   HWND hwndTreeCtl;
   TV_ITEM tv;

   /* Get the selected folder.
    */
   VERIFY( hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST ) );
   hSelItem = TreeView_GetSelection( hwndTreeCtl );
   ASSERT( 0L != hSelItem );
   tv.hItem = hSelItem;
   tv.mask = TVIF_PARAM|TVIF_RECT;
   VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );

   /* Apply any outstanding changes.
    */
   if( ApplyPurgeChanges( hwnd, (LPVOID)tv.lParam, FALSE ) )
      if( Amdb_IsTopicPtr( (PTL)tv.lParam ) )
         {
         if( !IsRectEmpty( &tv.rcItem ) )
            {
            InvalidateRect( hwndTreeCtl, &tv.rcItem, TRUE );
            UpdateWindow( hwndTreeCtl );
            }
         }
      else if( Amdb_IsFolderPtr( (PCL)tv.lParam ) )
         {
         HTREEITEM hChildItem;

         /* Update all expanded child items of this
          * folder.
          */         
         hChildItem = TreeView_GetChild( hwndTreeCtl, hSelItem );
         while( hChildItem )
            {
            tv.hItem = hChildItem;
            tv.mask = TVIF_PARAM|TVIF_RECT;
            VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
            if( !IsRectEmpty( &tv.rcItem ) )
               {
               InvalidateRect( hwndTreeCtl, &tv.rcItem, TRUE );
               UpdateWindow( hwndTreeCtl );
               }
            hChildItem = TreeView_GetNextSibling( hwndTreeCtl, hChildItem );
            }
         }
      else
         {
         InvalidateRect( hwndTreeCtl, NULL, TRUE );
         UpdateWindow( hwndTreeCtl );
         }
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL PurgeSettings_OnMove( HWND hwnd, int x, int y )
{
   if( !IsIconic( hwnd ) )
      Amuser_WriteWindowState( szPurgeSettingsWndName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL PurgeSettings_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   /* Save the new window state to the configuration file.
    */
   if( !IsIconic( hwnd ) )
      Amuser_WriteWindowState( szPurgeSettingsWndName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* This function handles the WM_MDIACTIVATE message.
 */
void FASTCALL PurgeSettings_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_PURGESETTINGS ), (LPARAM)(LPSTR)hwnd );
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, Adm_DefMDIDlgProc );
}

/* This function handles notification messages from the
 * treeview control.
 */
LRESULT FASTCALL PurgeSettings_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
{
   switch( lpNmHdr->code )
      {
      case TVN_SELCHANGING: {
         LPNM_TREEVIEW lpnmtv;
         TV_ITEM tv;

         /* Get the old item and apply the selected purge
          * settings.
          */
         lpnmtv = (LPNM_TREEVIEW)lpNmHdr;
         if( 0L != lpnmtv->itemOld.hItem )
            {
            tv.hItem = lpnmtv->itemOld.hItem;
            tv.mask = TVIF_PARAM;
            VERIFY( TreeView_GetItem( lpNmHdr->hwndFrom, &tv ) );
            if( Amdb_IsTopicPtr( (PTL)tv.lParam ) && fPurgeChanged )
               if( !ApplyPurgeChanges( hwnd, (LPVOID)tv.lParam, FALSE ) )
                  return( TRUE );
            }
         break;
         }

      case TVN_SELCHANGED: {
         LPNM_TREEVIEW lpnmtv;
         TV_ITEM tv;

         /* Get the old item and apply the selected purge
          * settings.
          */
         lpnmtv = (LPNM_TREEVIEW)lpNmHdr;
         tv.hItem = lpnmtv->itemNew.hItem;
         tv.mask = TVIF_PARAM;
         VERIFY( TreeView_GetItem( lpNmHdr->hwndFrom, &tv ) );
         PurgeOptions_ShowSettings( hwnd, (LPVOID)tv.lParam );
         SetFocus( lpNmHdr->hwndFrom );
         break;
         }

      case TVN_GETDISPINFO: {
         TV_DISPINFO FAR * lptvi;

         lptvi = (TV_DISPINFO FAR *)lpNmHdr;
         if( Amdb_IsCategoryPtr( (PCAT)lptvi->item.lParam ) )
            {
            if( lptvi->item.state & TVIS_EXPANDED )
               {
               lptvi->item.iImage = IBML_SELCATEGORY;
               lptvi->item.iSelectedImage = IBML_SELCATEGORY;
               }
            else
               {
               lptvi->item.iImage = IBML_CATEGORY;
               lptvi->item.iSelectedImage = IBML_CATEGORY;
               }
            }
         else
            {
            if( lptvi->item.state & TVIS_EXPANDED )
               {
               lptvi->item.iImage = IBML_SELFOLDER;
               lptvi->item.iSelectedImage = IBML_SELFOLDER;
               }
            else
               {
               lptvi->item.iImage = IBML_FOLDER;
               lptvi->item.iSelectedImage = IBML_FOLDER;
               }
            }
         return( TRUE );
         }
      }
   return( FALSE );
}

/* This function draws a treeview label.
 */
void FASTCALL PurgeSettings_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpdis )
{
   TV_DRAWITEM FAR * lptvdi;
   LPVOID pFolder;
   COLORREF crText;
   COLORREF crBack;
   COLORREF T;
   TOPICINFO ti;
   char szLabel[ 100 ];
   char sz[ 100 ];
   RECT rcLabel;
   RECT rc;
   SIZE size;
   int x, y;

   ti.dwFlags = 0;
   ti.po.wFlags = 0;
   ti.po.wMaxSize = 0;
   ti.po.wMaxDate = 0;

   /* Get a pointer to the TV_DRAWITEM struct that
    * has more details than the DRAWITEMSTRUCT structure.
    */
   lptvdi = (TV_DRAWITEM FAR *)lpdis->itemData;
   pFolder = (LPVOID)lptvdi->tv.lParam;

   /* If this is a topic, get the topic information.
    */
   if( Amdb_IsTopicPtr( (PTL)pFolder ) )
      Amdb_GetTopicInfo( (PTL)pFolder, &ti );

   /* Set the label colours depending on whether it is selected or
    * unselected, a conference, database or topic and, if a topic,
    * the topic status.
    */
   if( lpdis->itemState & ODS_SELECTED )
      {
      crText = SetTextColor( lpdis->hDC, crHighlightText );
      crBack = SetBkColor( lpdis->hDC, crHighlight );
      }
   else
      {
      COLORREF crNewText;

      crNewText = crInBaskText;
      if( Amdb_IsTopicPtr( (PTL)pFolder ) )
         {
         if( ti.dwFlags & TF_RESIGNED )
            crNewText = crResignedTopic;
         else if( ti.dwFlags & TF_IGNORED )
            crNewText = crIgnoredTopic;
         }
      else if( Amdb_IsFolderPtr( (PCL)pFolder ) )
         {
         if( Amdb_IsResignedFolder( (PCL)pFolder ) )
            crNewText = crResignedTopic;
         }
      crText = SetTextColor( lpdis->hDC, crNewText );
      crBack = SetBkColor( lpdis->hDC, GetSysColor( COLOR_WINDOW ) );
      }
   T = crInBaskText;

   /* Compute the initial label width.
    */
   GetTextExtentPoint( lpdis->hDC, lptvdi->pszText, lstrlen( lptvdi->pszText ), &size );
   rcLabel = lpdis->rcItem;
   x = rcLabel.left + 3;
   y = rcLabel.top + ( ( rcLabel.bottom - rcLabel.top ) - size.cy ) / 2;

   /* Init for to show purge options
    */
   rc = lpdis->rcItem;
   sz[ 0 ] = '\0';

   /* Get the purge options
    */
   if( Amdb_IsTopicPtr( (PTL)pFolder ) )
      {
      if( ti.po.wFlags & PO_NOPURGE )
         {
         strcpy( sz, GS(IDS_STR549) );
         SetTextColor( lpdis->hDC, crResignedTopic );
         }
      else if( ti.po.wFlags & PO_DELETEONLY )
         strcpy( sz, GS(IDS_STR550) );
      else if( ti.po.wFlags & PO_PURGE_IGNORED )
         strcpy( sz, GS(IDS_STR1229) );
      else if( ti.po.wFlags & PO_SIZEPRUNE )
         wsprintf( sz, GS(IDS_STR551), ti.po.wMaxSize );
      else if( ti.po.wFlags & PO_DATEPRUNE )
         wsprintf( sz, GS(IDS_STR552), ti.po.wMaxDate );
      }

   /* Compute the purge options width.
    */
   GetTextExtentPoint( lpdis->hDC, sz, strlen(sz), &size );
   rc.right = lpdis->rcItem.right;
   rc.left = rc.right - ( size.cx + 10 );

   /* Adjust the label to fit, if needed.
    */
   strcpy( szLabel, lptvdi->pszText );
   AdjustNameToFit( lpdis->hDC, szLabel, ( rc.left - x ) - 10, TRUE, &size );

   /* Draw the label.
    */
   rcLabel.right = rcLabel.left + size.cx + 6;
   ExtTextOut( lpdis->hDC, x, y, ETO_OPAQUE, &rcLabel, szLabel, strlen(szLabel), NULL );

   /* Draw the statistics.
    */
   if( *sz )
      {
      /* If the treeview control has the focus, then draw
       * a focus rectangle around the selected item.
       */
      if( lpdis->itemState & ODS_FOCUS )
         DrawFocusRect( lpdis->hDC, &rcLabel );
   
      /* Restore the background colour.
       */
      SetTextColor( lpdis->hDC, T );
      SetBkColor( lpdis->hDC, GetSysColor( COLOR_WINDOW ) );
      ExtTextOut( lpdis->hDC, rc.left, y, ETO_OPAQUE, &rc, sz, strlen( sz ), NULL );
      }

   /* Clean up before we leave.
    */
   SetBkColor( lpdis->hDC, crBack );
   SetTextColor( lpdis->hDC, crText );
}
