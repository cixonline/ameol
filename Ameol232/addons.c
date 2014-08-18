/* ADDONS.C - Support for loading and running addons
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
#include <stdlib.h>
#include <ctype.h>
#include <commdlg.h>
#include <direct.h>
#include <io.h>
#include <dos.h>
#include <string.h>
#include <shellapi.h>
#include "command.h"
#include "ameol2.h"
#include "cix.h"
#include "rules.h"

#pragma pack(1)   /* Structures passed to addons must have this packing */

#define  THIS_FILE   __FILE__

static BOOL fDefDlgEx = FALSE;               /* DefDlg recursion flag trap */

#define  LEN_ADDONNAME           32

typedef struct tagADDON {
   struct tagADDON FAR * lpadnNext;          /* Pointer to next addon */
   char szCommandLine[ LEN_COMMAND_LINE ];   /* Addon command line */
   char szAddonName[ LEN_ADDONNAME ];        /* Addon name */
   struct {
      unsigned fInstalled:1;                 /* TRUE if addon was just installed */
      unsigned fEnabled:1;                   /* TRUE if addon is enabled */
      };
   HINSTANCE hLib;                           /* Handle of addon */
   HICON hIcon;                              /* Icon displayed in properties */
   VERSION ver;                              /* Addon version information */
} ADDON;

typedef ADDON FAR * LPADDON;
typedef VERSION FAR * LPVERSION;

typedef int (FAR PASCAL * ADN_EXECPROC)( HWND );
typedef int (FAR PASCAL * ADN_LOADPROC)( HWND );
typedef int (FAR PASCAL * ADN_UNLOADPROC)( int );
typedef int (FAR PASCAL * ADN_COMMANDPROC)( HWND, int );
typedef BOOL (FAR PASCAL * ADN_QUERYADDONPROC)( int, LPVOID );
typedef LPVERSION (FAR PASCAL * ADN_VERSIONPROC)( void );

LPADDON lpadnFirst = NULL;          /* First Tools library in list */
LPADDON lpadnLast = NULL;           /* Last Tools library in list */
int cInstalledAddons;               /* Number of installed addons */
HINSTANCE hAmeolLib;                /* Handle of AMEOL.DLL library */
WORD wNextCmdID = IDM_TOOLFIRST;    /* Addon command ID */

static AMEOLMENU amlast;            /* Last structure passed to Ameol2_AddMenu */
static BOOL fHasLast = FALSE;       /* TRUE if amlast is valid */

extern BOOL fUseU3;                 /* Ameol is being run from a U3 Device, so use INI and U3 variables*/ // !!SM!! 2.56.2053

#define A2_FILE_MENU       0
#define A2_EDIT_MENU       1
#define A2_MAIL_MENU       2
#define A2_CIX_MENU        3
#define A2_NEWS_MENU       4
#define A2_VIEW_MENU       5
#define A2_FOLDER_MENU     6
#define A2_MESSAGE_MENU    7
#define A2_SETTINGS_MENU   8
#define A2_WINDOW_MENU     9
#define A2_HELP_MENU       10

#define  MAX_MAP_ELEMENTS  10

/* Map Ameol v1 menus to Ameol v2.
 */
int pFullMenuMap[ MAX_MAP_ELEMENTS ] = {
   A2_FILE_MENU,           /* A1 File menu -> A2 File menu */
   A2_EDIT_MENU,           /* A1 Edit menu -> A2 Edit menu */
   A2_MAIL_MENU,           /* A1 Mail menu -> A2 Mail menu */
   A2_SETTINGS_MENU,       /* A1 Global menu -> A2 Settings menu */
   A2_SETTINGS_MENU,       /* A1 Settings menu -> A2 Settings menu */
   A2_FOLDER_MENU,         /* A1 Topic menu -> A2 Folder menu */
   A2_MESSAGE_MENU,        /* A1 Message menu -> A2 Message menu */
   A2_FILE_MENU,           /* A1 Utilities menu -> N/A */
   A2_WINDOW_MENU,         /* A1 Window menu -> A2 Window menu */
   A2_HELP_MENU            /* A1 Help menu -> A2 Help menu */
   };

int pConfMenuMap[ MAX_MAP_ELEMENTS ] = {
   A2_FILE_MENU,           /* A1 File menu -> A2 File menu */
   A2_MAIL_MENU,           /* A1 Mail menu -> A2 Mail menu */
   A2_SETTINGS_MENU,       /* A1 Global menu -> A2 Settings menu */
   A2_SETTINGS_MENU,       /* A1 Settings menu -> A2 Settings menu */
   A2_VIEW_MENU,           /* A1 View menu -> A2 View menu */
   A2_FOLDER_MENU,         /* A1 Topic menu -> A2 Folder menu */
   A2_FILE_MENU,           /* A1 Utilities menu -> N/A */
   A2_WINDOW_MENU,         /* A1 Window menu -> A2 Window menu */
   A2_HELP_MENU            /* A1 Help menu -> A2 Help menu */
   };

int pTermMenuMap[ MAX_MAP_ELEMENTS ] = {
   A2_FILE_MENU,           /* A1 File menu -> A2 File menu */
   A2_EDIT_MENU,           /* A1 Edit menu -> A2 Edit menu */
   A2_MAIL_MENU,           /* A1 Mail menu -> A2 Mail menu */
   A2_SETTINGS_MENU,       /* A1 Global menu -> A2 Settings menu */
   A2_SETTINGS_MENU,       /* A1 Settings menu -> A2 Settings menu */
   A2_FOLDER_MENU,         /* A1 Terminal menu -> N/A */
   A2_FILE_MENU,           /* A1 Utilities menu -> N/A */
   A2_WINDOW_MENU,         /* A1 Window menu -> A2 Window menu */
   A2_HELP_MENU            /* A1 Help menu -> A2 Help menu */
   };

/* Map menus to categories.
 */
static int pCategoryMap[] = {
   CAT_FILE_MENU,
   CAT_EDIT_MENU,
   CAT_MAIL_MENU,
   CAT_CIX_MENU,
   CAT_NEWS_MENU,
   CAT_VIEW_MENU,
   CAT_TOPIC_MENU,
   CAT_MESSAGE_MENU,
   CAT_SETTINGS_MENU,
   CAT_WINDOW_MENU,
   CAT_HELP_MENU
   };

/* Map menus to categories.
 */
static int pCIXOnlyCategoryMap[] = {
   CAT_FILE_MENU,
   CAT_EDIT_MENU,
   CAT_CIX_MENU,
   CAT_VIEW_MENU,
   CAT_TOPIC_MENU,
   CAT_MESSAGE_MENU,
   CAT_SETTINGS_MENU,
   CAT_WINDOW_MENU,
   CAT_HELP_MENU
   };

/* Map menus to categories.
 */
static int pCIXMailOnlyCategoryMap[] = {
   CAT_FILE_MENU,
   CAT_EDIT_MENU,
   CAT_MAIL_MENU,
   CAT_CIX_MENU,
   CAT_VIEW_MENU,
   CAT_TOPIC_MENU,
   CAT_MESSAGE_MENU,
   CAT_SETTINGS_MENU,
   CAT_WINDOW_MENU,
   CAT_HELP_MENU
   };

/* Addon function entry-points
 */
static char szAddonVersion[] = "AddonVersion";
static char szExec[] = "Exec";
static char szLoad[] = "Load";
static char szUnload[] = "Unload";
static char szCommand[] = "Command";
static char szQueryAddon[] = "QueryAddon";

static char szAmeolDll[] = "AMEOL32.EXE";

LPADDON FASTCALL InstallAddon( HWND, LPSTR );
BOOL FASTCALL UninstallAddon( LPADDON );
int FASTCALL MapMenuForA260AndLater(int wMenu);
BOOL FASTCALL LoadAddon( LPADDON );
BOOL FASTCALL UnloadAddon( LPADDON );
LPSTR FASTCALL ExpandVersion( DWORD );
void FASTCALL SaveInstalledAddons( void );
HICON FASTCALL BitmapToIcon( HBITMAP );
HBITMAP FASTCALL GetAddonBitmap( HINSTANCE, int, int, int );
HBITMAP FASTCALL GetAddonToolbarBitmap( CMDREC FAR * );

BOOL EXPORT CALLBACK Addons( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL Addons_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL Addons_OnCommand( HWND, int, HWND, UINT );
void FASTCALL Addons_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
LRESULT FASTCALL Addons_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT CALLBACK AddonProperties( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL AddonProperties_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL AddonProperties_OnNotify( HWND, int, LPNMHDR );
void FASTCALL AddonProperties_OnCommand( HWND, int, HWND, UINT );
BOOL FASTCALL IsAddonLoaded( LPSTR );
void FASTCALL SaveCustomButtonBitmap( CMDREC FAR * );

typedef struct tagEXTAPPENTRY {
   struct tagEXTAPPENTRY FAR * lpeaNext;  /* Pointer to next entry */
   int iCommandID;                        /* Command ID */
   APPINFO appinfo;                       /* Application info */
} EXTAPPENTRY, FAR * LPEXTAPPENTRY;

LPEXTAPPENTRY lpeaFirst = NULL;           /* First external app */

UINT CALLBACK EXPORT ConfigAddonCallbackFunc( HWND, UINT, LPARAM );

/* This function installs the specified external application
 * into the command table.
 */
int FASTCALL InstallExternalApp( LPAPPINFO lpAppInfo )
{
   static UINT iCommandID = IDM_EXTAPPFIRST;
   LPEXTAPPENTRY lpeaNew;
   LPEXTAPPENTRY lpea;

   INITIALISE_PTR(lpeaNew);

   /* Look for an existing command.
    */
   for( lpea = lpeaFirst; lpea; lpea = lpea->lpeaNext )
      if( ( strcmp( lpAppInfo->szAppPath, lpea->appinfo.szAppPath ) == 0 ) && ( strcmp( lpAppInfo->szCmdLine, lpea->appinfo.szCmdLine ) == 0 ) )
         return( lpea->iCommandID );

   /* First create an entry for this application.
    */
   if( fNewMemory( &lpeaNew, sizeof(EXTAPPENTRY) ) )
      {
      char szCommandName[ 64 ];
      char * pBasename;
      char * pExt;
      CMDREC cmd;

      /* Add a new entry into the structure.
       */
      if( iCommandID > IDM_EXTAPPLAST )
         iCommandID = IDM_EXTAPPFIRST;
      lpeaNew->lpeaNext = lpeaFirst;
      lpeaFirst = lpeaNew;
      lpeaNew->iCommandID = iCommandID++;
      lpeaNew->appinfo = *lpAppInfo;

      /* Create a decorated command name.
       */
      strcpy( lpTmpBuf, lpAppInfo->szAppPath );
      pBasename = GetFileBasename( lpTmpBuf );
      pExt = strchr( pBasename, '.' );
      if( NULL != pExt )
         *pExt = '\0';

      /* Decorate the name.
       */
      _strlwr( pBasename );
      pBasename[ 0 ] = (char)toupper( pBasename[ 0 ] );

      /* Finally create the tooltip string and show it on
       * the command line.
       */
      wsprintf( lpeaNew->appinfo.szCommandName, "Run %s", pBasename );
      wsprintf( szCommandName, "Run%s", pBasename );

      /* Ensure we have a Terminals category.
       */
      if( !CTree_FindCategory( CAT_EXTAPP ) )
         CTree_AddCategory( CAT_EXTAPP, (WORD)-1, "Other Apps" );

      /* Next, create an entry in the command table.
       */
      cmd.iCommand = lpeaNew->iCommandID;
      cmd.lpszString = lpeaNew->appinfo.szCommandName;
      cmd.lpszCommand = szCommandName;
      cmd.iDisabledString = 0;
      cmd.lpszTooltipString = lpeaNew->appinfo.szCommandName;
      cmd.iToolButton = 0;
      cmd.wCategory = CAT_EXTAPP;
      cmd.wDefKey = 0;
      cmd.iActiveMode = WIN_ALL;
      cmd.hLib = NULL;
      cmd.wNewKey = 0;
      cmd.wKey = 0;
      cmd.nScheduleFlags = NSCHF_NONE;
      cmd.btnBmp.hbmpSmall = NULL;
      cmd.btnBmp.hbmpLarge = NULL;
      CTree_InsertCommand( &cmd );
      return( lpeaNew->iCommandID );
      }
   return( -1 );
}

/* This function runs the specified external application
 * given it's command ID.
 */
void FASTCALL RunExternalApp( int iCommand )
{
   LPEXTAPPENTRY lpea;

   for( lpea = lpeaFirst; lpea; lpea = lpea->lpeaNext )
      if( iCommand == lpea->iCommandID )
         {
         LPSTR lpAppPath;
         LPSTR lpCmdLine;

         INITIALISE_PTR(lpAppPath);
         INITIALISE_PTR(lpCmdLine);

         /* Allocate a couple of buffers.
          */
         if( fNewMemory( &lpAppPath, _MAX_PATH ) )
            {
            if( fNewMemory( &lpCmdLine, _MAX_PATH ) )
               {
               char * pTmpBuf;
               UINT uRetCode;

               pTmpBuf = lpCmdLine;
               if( *lpea->appinfo.szCmdLine )
                  {
                  char * pCmdLine;

                  /* Copy the command line, expanding any
                   * special characters.
                   */
                  for( pCmdLine = lpea->appinfo.szCmdLine; *pCmdLine; ++pCmdLine )
                     if( *pCmdLine != '%' )
                        *pTmpBuf++ = *pCmdLine;
                     else switch( *++pCmdLine )
                        {
                        case 0:
                           --pCmdLine;
                           break;

                        case '%':
                           *pTmpBuf++ = '%';
                           break;

                        case 's':
                           if( hwndTopic )
                              {
                              GetMarkedName( hwndTopic, pTmpBuf, (LEN_TEMPBUF - strlen(pTmpBuf)) - 1, FALSE );
                              pTmpBuf += strlen( pTmpBuf );
                              }
                           break;

                        case 'h':
                           if( hwndTopic )
                              {
                              GetMarkedName( hwndTopic, pTmpBuf, (LEN_TEMPBUF - strlen(pTmpBuf)) - 1, FALSE );
                              pTmpBuf += strlen( pTmpBuf );
                              }

                        case 'a':
                           if( hwndTopic )
                              {
                              CURMSG curmsg;

                              /* Get the current message if there is one, and locate
                               * the author of that message.
                               */
                              Ameol2_GetCurrentMsg( &curmsg );
                              if( NULL != curmsg.pth )
                                 {
                                 MSGINFO msginfo;

                                 Amdb_GetMsgInfo( curmsg.pth, &msginfo );
                                 strcpy( pTmpBuf, msginfo.szAuthor );
                                 pTmpBuf += strlen( pTmpBuf );
                                 }
                              }
                           break;
                        }
                  }
               *pTmpBuf = '\0';

               /* Strip quotes from the application path, then run it.
                */
               strcpy( lpAppPath, lpea->appinfo.szAppPath );
               StripCharacter( lpAppPath, '"' );
               if( ( uRetCode = (UINT)ShellExecute( hwndFrame, NULL, lpAppPath, lpCmdLine, pszAmeolDir, SW_SHOWNORMAL ) ) < 32 )
                  DisplayShellExecuteError( hwndFrame, lpAppPath, uRetCode );
               FreeMemory( &lpCmdLine );
               }
            FreeMemory( &lpAppPath );
            }
         break;
         }
}

/* This function retrieves the external application pathname
 * given a command ID.
 */
void FASTCALL GetExternalApp( int iCommand, LPAPPINFO lpAppPath )
{
   LPEXTAPPENTRY lpea;

   for( lpea = lpeaFirst; lpea; lpea = lpea->lpeaNext )
      if( iCommand == lpea->iCommandID )
         {
         *lpAppPath = lpea->appinfo;
         break;
         }
}

/* Given the runapp command name, this function retrieves the
 * runapp command ID.
 */
UINT FASTCALL GetRunAppCommandID( LPSTR lpszName )
{
   LPEXTAPPENTRY lpea;
   char szAppPathAndCmdLine[ 512 ];                /* Path and filename of application */
// char szQuotedAppPathAndCmdLine[ 512 ];                /* Path and filename of application */

   for( lpea = lpeaFirst; lpea; lpea = lpea->lpeaNext )
   {
      QuotifyFilename( lpea->appinfo.szAppPath );
//    QuotifyFilename( lpszName );
      if( *lpea->appinfo.szCmdLine )
         wsprintf( szAppPathAndCmdLine, "%s %s", lpea->appinfo.szAppPath , lpea->appinfo.szCmdLine );
      else
         wsprintf( szAppPathAndCmdLine, "%s", lpea->appinfo.szAppPath );
      if( strcmp( szAppPathAndCmdLine, lpszName ) == 0 ) 
         return( lpea->iCommandID );
   }
   return( 0 );
}

/* This function edits the attributes for the specified
 * toolbar command.
 */
void FASTCALL EditExternalApp( HWND hwnd, int iCommand )
{
   LPEXTAPPENTRY lpea;

   for( lpea = lpeaFirst; lpea; lpea = lpea->lpeaNext )
      if( iCommand == lpea->iCommandID )
         {
         CmdExternalApp( hwnd, &lpea->appinfo );
         break;
         }
}

/* This function returns the number of addons installed.
 */
int FASTCALL CountInstalledAddons( void )
{
   return( cInstalledAddons );
}

/* This function loads all addons when Ameol starts.
 */
void FASTCALL InstallAllAddons( HWND hwnd )
{
   char sz[ LEN_COMMAND_LINE ];
   BOOL fRenameSubsequent;
   char szText[ 10 ];
   int cTool;

   /* Load an old format addon section.
    */
   cTool = 1;
   lpadnFirst = NULL;
   lpadnLast = NULL;
   fRenameSubsequent = FALSE;
   wsprintf( szText, "tool%d", cTool++ );
   while( Amuser_GetPPString( szAddons, szText, "", sz, sizeof( sz ) ) )
      {
      LPADDON lpadnNew;

      if( NULL == ( lpadnNew = InstallAddon( hwnd, sz ) ) )
         fRenameSubsequent = TRUE;
      else
         {
         if( NULL == lpadnFirst )
            lpadnFirst = lpadnNew;
         else
            lpadnLast->lpadnNext = lpadnNew;
         lpadnLast = lpadnNew;
         ++cInstalledAddons;
         }
      wsprintf( szText, "tool%d", cTool++ );
      }

   /* If any old format addon section found, convert it to the new
    * format.
    */
   if( NULL != lpadnFirst )
      {
      LPADDON lpadn;

      Amuser_WritePPString( szAddons, NULL, NULL );
      for( lpadn = lpadnFirst; lpadn; lpadn = lpadn->lpadnNext )
         Amuser_WritePPString( szAddons, lpadn->szAddonName, lpadn->szCommandLine );
      }
   else
      {
      LPSTR lpsz;

      /* Look for the new format section. Read the whole list of
       * key names.
       */
      INITIALISE_PTR(lpsz);
      if( fNewMemory( &lpsz, 8000 ) )
         {
         UINT c;

         Amuser_GetPPString( szAddons, NULL, "", lpsz, 8000 );
         for( c = 0; lpsz[ c ]; ++c )
            {
            LPADDON lpadnNew;

            Amuser_GetPPString( szAddons, &lpsz[ c ], "", lpPathBuf, _MAX_PATH );

            if( fUseU3 )
            {
               char szPath[_MAX_PATH];

               if ( GetEnvironmentVariable("U3_DEVICE_DOCUMENT_PATH", (LPSTR)&szPath, _MAX_PATH - 1) > 0 )
               {
                  lpPathBuf[0] = szPath[0];
               }
            }
            if( NULL != ( lpadnNew = InstallAddon( hwnd, lpPathBuf ) ) )
               {
               if( NULL == lpadnFirst )
                  lpadnFirst = lpadnNew;
               else
                  lpadnLast->lpadnNext = lpadnNew;
               lpadnLast = lpadnNew;
               ++cInstalledAddons;
               }
            c += strlen( &lpsz[ c ] );
            }
         FreeMemory( &lpsz );
         }
      }

   /* This ensures that addons not installed are vaped from the
    * registry and the remaining are renamed.
    */
   if( fRenameSubsequent )
      SaveInstalledAddons();
}

/* This function saves the list of installed addons.
 */
void FASTCALL SaveInstalledAddons( void )
{
   LPADDON lpadn;

   lpadn = lpadnFirst;
   Amuser_WritePPString( szAddons, NULL, NULL );
   while( lpadn )
      {
      Amuser_WritePPString( szAddons, lpadn->szAddonName, lpadn->szCommandLine );
      lpadn = lpadn->lpadnNext;
      }
}

/* This function calls the Load interface for all addons. If
 * any fail, the addon is unloaded from memory.
 */
void FASTCALL LoadAllAddons( void )
{
   LPADDON lpadn;

   lpadn = lpadnFirst;
   while( lpadn )
      {
      LPADDON lpadnNext;

      lpadnNext = lpadn->lpadnNext;
      if( !LoadAddon( lpadn ) )
         lpadn->fEnabled = FALSE;
      lpadn = lpadnNext;
      }
}

/* This function is called before Ameol exits. It removes all addons
 * from memory.
 */
void FASTCALL UnloadAllAddons( void )
{
   LPADDON lpadn;

   lpadn = lpadnFirst;
   while( lpadn )
      {
      LPADDON lpadnNext;

      lpadnNext = lpadn->lpadnNext;
      if( lpadn->fEnabled )
         UnloadAddon( lpadn );
      UninstallAddon( lpadn );
      --cInstalledAddons;
      lpadn = lpadnNext;
      }
   lpadnFirst = lpadnLast = NULL;
}

/* Given the full path and filename of an addon, this function
 * creates a record for the addon and returns a pointer to the
 * record.
 */
LPADDON FASTCALL InstallAddon( HWND hwnd, LPSTR lpszFilename )
{
   LPADDON lpadnNew;

   INITIALISE_PTR(lpadnNew);
   if( !fNewMemory( &lpadnNew, sizeof(ADDON) ) )
      OutOfMemoryError( hwnd, FALSE, FALSE );
   else
      {
      char szAddonName[ LEN_ADDONNAME ];
      char * pszAddonBasename;
      LPVERSION lpVersion;
      HINSTANCE hAdnLib;
      register int c;

      /* Create the addon name.
       */
      pszAddonBasename = GetFileBasename( lpszFilename );
      for( c = 0; c < LEN_ADDONNAME - 1 && *pszAddonBasename != '.'; ++c )
         szAddonName[ c ] = *pszAddonBasename++;
      szAddonName[ c ] = '\0';

      /* Fill out the new structure.
       */
      StatusText( GS(IDS_STR554), GetFileBasename( lpszFilename ) );
      lpadnNew->lpadnNext = NULL;
      lpadnNew->hLib = NULL;
      lpadnNew->hIcon = NULL;
      lpadnNew->fEnabled = TRUE;
      lpadnNew->fInstalled = FALSE;
      lstrcpy( lpadnNew->szCommandLine, lpszFilename );
      lstrcpy( lpadnNew->szAddonName, szAddonName );
      if( IsAddonLoaded( lpszFilename ) )
      {
         fMessageBox( hwnd, 0, "This addon is already loaded.", MB_OK|MB_ICONINFORMATION );
         FreeMemory( &lpadnNew );
         return( lpadnNew );
      }

      if( NULL == ( hAdnLib = LoadLibrary( lpszFilename ) ) )
      {
         wsprintf( lpTmpBuf, GS(IDS_STR811), GetFileBasename( lpszFilename ) );
         fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONINFORMATION );
         FreeMemory( &lpadnNew );
      }
      else
      {
         ADN_LOADPROC lpLoadProc;
         ADN_VERSIONPROC lpVerProc;

         lpadnNew->hLib = hAdnLib;
         if( ( lpLoadProc = (ADN_LOADPROC)GetProcAddress( hAdnLib, szLoad ) ) == NULL )
         {
            wsprintf( lpTmpBuf, GS(IDS_STR810), GetFileBasename( lpszFilename ) );
            fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONINFORMATION );
            FreeLibrary( hAdnLib );
            FreeMemory( &lpadnNew );
         }
         else if( ( lpVerProc = (ADN_VERSIONPROC)GetProcAddress( hAdnLib, szAddonVersion ) ) != NULL )
         {
            lpVersion = (*lpVerProc)();
            lpadnNew->ver = *lpVersion;
         }
      }
   }
   return( lpadnNew );
}

/* This function invokes the addon Load procedure.
 */
BOOL FASTCALL LoadAddon( LPADDON lpadn )
{
   ADN_LOADPROC lpLoadProc;

   /* Report to the status bar.
    */
   wsprintf( lpTmpBuf, GS(IDS_STR541), GetFileBasename( lpadn->szCommandLine ) );
   OfflineStatusText( lpTmpBuf );

   /* Check that the minimum version required is equal to or
    * greater than the current version.
    */
   if( ( lpadn->ver.dwReqVersion & 0x00FFFFFF ) > CvtToAmeolVersion(Ameol2_GetVersion()) )
      {
      wsprintf( lpTmpBuf, GS(IDS_STR823), lpadn->szCommandLine, ExpandVersion( lpadn->ver.dwReqVersion ) );
      fMessageBox( hwndFrame, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
      return( FALSE );
      }

   /* Now call the Load interface.
    */
   if( ( lpLoadProc = (ADN_LOADPROC)GetProcAddress( lpadn->hLib, szLoad ) ) != NULL )
      if( !(*lpLoadProc)( hwndFrame ) )
         {
         wsprintf( lpTmpBuf, GS(IDS_STR824), lpadn->szCommandLine );
         fMessageBox( hwndFrame, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
         return( FALSE );
         }
   return( TRUE );
}

/* This function invokes the addon Unload procedure.
 */
BOOL FASTCALL UnloadAddon( LPADDON lpadn )
{
   ADN_UNLOADPROC lpUnloadProc;

   /* Now call the Unload interface.
    */
   if( ( lpUnloadProc = (ADN_UNLOADPROC)GetProcAddress( lpadn->hLib, szUnload ) ) != NULL )
      (*lpUnloadProc)( 0 );
   return( TRUE );
}

/* This function uninstalls an addon.
 */
BOOL FASTCALL UninstallAddon( LPADDON lpadn )
{
   LPADDON lpadnPtr;

   if( lpadn == lpadnFirst )
      lpadnFirst = lpadn->lpadnNext;
   else
      for( lpadnPtr = lpadnFirst; lpadnPtr; lpadnPtr = lpadnPtr->lpadnNext )
         if( lpadnPtr->lpadnNext == lpadn )
            {
            lpadnPtr->lpadnNext = lpadn->lpadnNext;
            --cInstalledAddons;
            break;
            }
   if( NULL != lpadn->hIcon )
      DestroyIcon( lpadn->hIcon );
   FreeLibrary( lpadn->hLib );
   FreeMemory( &lpadn );
   return( TRUE );
}

/* This function invokes the addon command handler.
 */
void FASTCALL SendAddonCommand( int iCommand )
{
   CMDREC cmd;

   cmd.iCommand = iCommand;
   if( CTree_GetCommand( &cmd ) )
      {
      ADN_COMMANDPROC lpProc;

      if( NULL != ( lpProc = (ADN_COMMANDPROC)GetProcAddress( cmd.hLib, szCommand ) ) )
         (*lpProc)( hwndFrame, cmd.iCommand );
      }
}

/* This function handles the Addons command.
 */
void FASTCALL CmdAddons( HWND hwnd )
{
   Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_ADDONS), Addons, 0L );
}

/* This function handles the Addons dialog box
 */
BOOL EXPORT CALLBACK Addons( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, Addons_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the Login
 * dialog.
 */
LRESULT FASTCALL Addons_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, Addons_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_DRAWITEM, Addons_OnDrawItem );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, Addons_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsADDONS );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL Addons_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPADDON lpadn;
   HWND hwndList;

   /* Fill the listbox with a list of all installed
    * addons.
    */
   lpadn = lpadnFirst;
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   while( lpadn )
      {
      ListBox_InsertString( hwndList, -1, lpadn );
      lpadn = lpadn->lpadnNext;
      }
   if( lpadnFirst )
      {
      ListBox_SetCurSel( hwndList, 0 );
      PostDlgCommand( hwnd, IDD_LIST, LBN_SELCHANGE );
      }
   else
      {
      EnableControl( hwnd, IDD_REMOVE, FALSE );
      EnableControl( hwnd, IDD_PROPERTIES, FALSE );
      }

   /* Load the AMEOL.DLL library to guard against legacy addons
    * which need the old API.
    */
   hAmeolLib = NULL;
   if( FindLibrary( szAmeolDll ) )
      hAmeolLib = LoadLibrary( szAmeolDll );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL Addons_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_PROPERTIES: {
         AMPROPSHEETPAGE psp;
         AMPROPSHEETHEADER psh;
         HWND hwndList;
         int index;
         LPADDON lpadn;

         /* Get the selected addon.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         index = ListBox_GetCurSel( hwndList );
         ASSERT( index != LB_ERR );
         lpadn = (LPADDON)ListBox_GetItemData( hwndList, index );

         /* Create the addons property page
          */
         psp.dwSize = sizeof( AMPROPSHEETPAGE );
         psp.dwFlags = PSP_USETITLE;
         psp.hInstance = hInst;
         psp.pszTemplate = MAKEINTRESOURCE( IDDLG_ADDON_PROPERTIES );
         psp.pszIcon = NULL;
         psp.pfnDlgProc = AddonProperties;
         psp.pszTitle = "Properties";
         psp.lParam = (LPARAM)lpadn;

         /* Create the properties dialog.
          */
         psh.dwSize = sizeof( AMPROPSHEETHEADER );
         psh.dwFlags = PSH_PROPSHEETPAGE|PSH_PROPTITLE|PSH_HASHELP|PSH_NOAPPLYNOW;
         psh.hwndParent = hwnd;
         psh.hInstance = hInst;
         psh.pszIcon = NULL;
         psh.pszCaption = GetFileBasename( lpadn->szCommandLine );
         psh.nPages = sizeof( psp ) / sizeof( AMPROPSHEETPAGE );
         psh.nStartPage = 0;
         psh.ppsp = (LPCAMPROPSHEETPAGE)&psp;
         Amctl_PropertySheet(&psh );
         break;
         }

      case IDD_REMOVE: {
         HWND hwndList;
         int index;

         /* Remove the selected addon.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         if( ( index = ListBox_GetCurSel( hwndList ) ) != LB_ERR )
            {
            char * pszBasename;
            LPADDON lpadn;
            int count;

            lpadn = (LPADDON)ListBox_GetItemData( hwndList, index );

            /* If this is MAILSORT, offer to disable the built-in
             * rules if not already disabled.
             */
            pszBasename = GetFileBasename( lpadn->szCommandLine );

            /* Unload and uninstall the addon from memory. Just in case
             * the addon had commands on the toolbar, save the toolbar.
             */
            if( !lpadn->fInstalled )
               UnloadAddon( lpadn );
            UninstallAddon( lpadn );
            SaveToolbar( hwndFrame );

            /* Remove it from the listbox.
             */
            count = ListBox_DeleteString( hwndList, index );
            if( index == count )
               --index;
            if( index >= 0 )
               ListBox_SetCurSel( hwndList, index );
            else
               {
               EnableControl( hwnd, IDD_REMOVE, FALSE );
               EnableControl( hwnd, IDD_PROPERTIES, FALSE );
               }
            }
         break;
         }

      case IDD_ADD: {
         static char FAR strFilters[] = "Ameol Addons (*.ADN)\0*.ADN\0All Files (*.*)\0*.*\0\0";
         char szCommandLine[ LEN_COMMAND_LINE ];

         Amuser_GetAddonsDirectory( szCommandLine, _MAX_DIR );
         if( CommonGetOpenFilename( hwnd, szCommandLine, "Select Addon", strFilters, "CachedAddonsDir" ) )
            {
            HWND hwndList;
            LPADDON lpadn;
            int index;

            /* Create a new addon record and add to the listbox.
             */
            hwndList = GetDlgItem( hwnd, IDD_LIST );
            ExtractFilename( szCommandLine, szCommandLine );
            if( NULL != ( lpadn = InstallAddon( hwnd, szCommandLine ) ) )
               {
               /* Mark this addon installed and add it to
                * the listbox.
                */
               lpadn->fInstalled = TRUE;
               index = ListBox_InsertString( hwndList, -1, lpadn );
               ListBox_SetCurSel( hwndList, index );
               EnableControl( hwnd, IDD_REMOVE, TRUE );
               EnableControl( hwnd, IDD_PROPERTIES, TRUE );
               }
            ShowUnreadMessageCount();
            }
         break;
         }

      case IDOK: {
         register int i;
         int count;
         HWND hwndList;

         /* Loop for each addon in the list and link them
          * together.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         count = ListBox_GetCount( hwndList );
         lpadnFirst = NULL;
         lpadnLast = NULL;
         for( i = 0; i < count; ++i )
            {
            LPADDON lpadn;

            lpadn = (LPADDON)ListBox_GetItemData( hwndList, i );
            if( NULL == lpadnFirst )
               lpadnFirst = lpadn;
            else
               lpadnLast->lpadnNext = lpadn;
            if( lpadn->fInstalled )
               {
               if( !LoadAddon( lpadn ) )
                  break;
               lpadn->fInstalled = FALSE;
               }
            lpadnLast = lpadn;
            }
         if( hAmeolLib )
            FreeLibrary( hAmeolLib );
         SaveInstalledAddons();
         ShowUnreadMessageCount();
         EndDialog( hwnd, 0 );
         break;
         }

      case IDCANCEL: {
         register int i;
         int count;
         HWND hwndList;

         /* Any changes made must be undone. Uninstall any
          * addons added and restore any that were removed.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         count = ListBox_GetCount( hwndList );
         for( i = 0; i < count; ++i )
            {
            LPADDON lpadn;

            lpadn = (LPADDON)ListBox_GetItemData( hwndList, i );
            if( lpadn->fInstalled )
               UninstallAddon( lpadn );
            }
         if( hAmeolLib )
            FreeLibrary( hAmeolLib );
         EndDialog( hwnd, 0 );
         break;
         }
      }
}

/* This function handles the WM_DRAWITEM message.
 */
void FASTCALL Addons_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   /* If there is an item to draw, draw it.
    */
   if( lpDrawItem->itemID != -1 )
      {
      HFONT hOldFont;
      LPADDON lpadn;
      COLORREF tmpT;
      COLORREF tmpB;
      COLORREF T;
      COLORREF B;
      HBRUSH hbr;
      RECT rc;

      /* Set the brush and pen colours and save the
       * current text and background colours.
       */
      if( lpDrawItem->itemState & ODS_SELECTED )
         {
         T = GetSysColor( COLOR_HIGHLIGHTTEXT );
         B = GetSysColor( COLOR_HIGHLIGHT );
         }
      else
         {
         T = GetSysColor( COLOR_WINDOWTEXT );
         B = GetSysColor( COLOR_WINDOW );
         }
      hbr = CreateSolidBrush( B );
      rc = lpDrawItem->rcItem;
   
      /* Show the description string.
       */
      lpadn = (LPADDON)ListBox_GetItemData( lpDrawItem->hwndItem, lpDrawItem->itemID );

      /* Erase the entire drawing area
       */
      FillRect( lpDrawItem->hDC, &rc, hbr );
      tmpT = SetTextColor( lpDrawItem->hDC, T );
      tmpB = SetBkColor( lpDrawItem->hDC, B );
   
      /* Display the addon name
       */
      hOldFont = SelectFont( lpDrawItem->hDC, hHelvB8Font );
      rc.left += 4;
      DrawText( lpDrawItem->hDC, lpadn->ver.szDescription, -1, &rc, DT_VCENTER|DT_LEFT|DT_SINGLELINE|DT_NOPREFIX );

      /* If this item has the focus, draw the focus box.
       */
      if( lpDrawItem->itemState & ODS_FOCUS )
         DrawFocusRect( lpDrawItem->hDC, &lpDrawItem->rcItem );

      /* Restore things back to normal.
       */
      SelectFont( lpDrawItem->hDC, hOldFont );
      SetTextColor( lpDrawItem->hDC, tmpT );
      SetBkColor( lpDrawItem->hDC, tmpB );
      DeleteBrush( hbr );
      }
}

/* This function dispatches messages for the General page of the
 * In Basket properties dialog.
 */
BOOL EXPORT CALLBACK AddonProperties( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, AddonProperties_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, AddonProperties_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, AddonProperties_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL AddonProperties_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPCAMPROPSHEETPAGE psp;
   LPADDON lpadn;

   /* Dereference the addon record.
    */
   psp = (LPCAMPROPSHEETPAGE)lParam;
   lpadn = (LPVOID)psp->lParam;

   /* Fill out the dialog.
    */
   GetFilePathname( lpadn->szCommandLine, lpPathBuf );
   SetDlgItemText( hwnd, IDD_ADDON, lpadn->ver.szDescription );
   SetDlgItemText( hwnd, IDD_FILENAME, GetFileBasename( lpadn->szCommandLine ) );
   SetDlgItemPath( hwnd, IDD_PATHNAME, lpPathBuf );
   SetDlgItemText( hwnd, IDD_VERSION, ExpandVersion( lpadn->ver.dwVersion ) );
   SetDlgItemText( hwnd, IDD_AUTHOR, lpadn->ver.szAuthor );
   wsprintf( lpTmpBuf, GS(IDS_STR918), ExpandVersion( lpadn->ver.dwReqVersion ) );
   SetDlgItemText( hwnd, IDD_REQVERSION, lpTmpBuf );

   /* Display the addon icon.
    */
   if( NULL != lpadn->hIcon )
      DestroyIcon( lpadn->hIcon );
   if( ( lpadn->hIcon = ExtractIcon( hInst, lpadn->szCommandLine, 0 ) ) != NULL && lpadn->hIcon != (HICON)1 )
      SendDlgItemMessage( hwnd, IDD_ICON, STM_SETICON, (WPARAM)lpadn->hIcon, 0 );
   else
      {
      HBITMAP hbmp;

      /* No icon, so create one from the bitmap if one exists.
       */
      if( NULL != ( hbmp = LoadBitmap( lpadn->hLib, MAKEINTRESOURCE( 6000 ) ) ) )
         {
         lpadn->hIcon = BitmapToIcon( hbmp );
         DeleteBitmap( hbmp );
         }
      }
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL AddonProperties_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL AddonProperties_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsADDON_PROPERTIES );
         break;

      case PSN_APPLY: {
         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This function expands a packed 32-bit version number into
 * its version number string.
 */
LPSTR FASTCALL ExpandVersion( DWORD dwVersion )
{
   static char sz[ 40 ];
   int nRelease;
   int nBeta;

   nRelease = LOWORD( dwVersion ) & 0xFF;
   nBeta = ( HIWORD( dwVersion ) >> 8 ) & 0xFF;
   wsprintf( sz, "%d.%2.2d", HIWORD( dwVersion ) & 0xFF, LOWORD( dwVersion ) >> 8 );
   if( nRelease > 100 && nRelease < 200 )
      wsprintf( sz + strlen( sz ), ".%2.2d", nRelease - 100 );
   else if( nRelease >= 200 )
      {
      if( nRelease > 200 )
         wsprintf( sz + strlen( sz ), ".%2.2d", nRelease - 200 );
      strcat( sz, " Evaluation Release" );
      }
   if( nBeta & 0x7F )
      wsprintf( sz + strlen( sz ), GS(IDS_STR365), nBeta & 0x7F );
   if( nBeta & 0x80 )
      strcat( sz, " [PV]" );
   return( sz );
}

/* This function converts a bitmap to an icon.
 */
HICON FASTCALL BitmapToIcon( HBITMAP hBitmap )
{
   HBITMAP hXorBitmap, hAndBitmap, hOldBmp1, hOldBmp2, hOldBmp3;
   BITMAP XorBitmap, AndBitmap;
   LPSTR lpAndBits, lpXorBits;
   HANDLE hAndBits, hXorBits;
   HDC hdcSrc, hdcDst, hdc, hdcTmp;
   int iIconWidth, iIconHeight;
   HICON hIcon;
   DWORD dwNumBytes;

   /* 1) Obtain a handle to the bitmap's DIB.
    */
   if( hBitmap == NULL )
      return( NULL );

   /* Get the icon's dimensions
    */
   iIconWidth = GetSystemMetrics( SM_CXICON );
   iIconHeight = GetSystemMetrics( SM_CYICON );

   /* Create two DCs to shrink/expand the bitmap to fit the Icon's dimensions
    * We need one DC to hold our source bitmap, and one to hold our 
    * destination bitmap (which is the size of our icon).
    */
   hdc = CreateDC( "DISPLAY", NULL, NULL, NULL );
   hdcSrc = CreateCompatibleDC( hdc );
   hOldBmp1 = SelectObject( hdcSrc, hBitmap );
   hdcDst = CreateCompatibleDC( hdc );
   hXorBitmap = CreateCompatibleBitmap( hdc, iIconWidth, iIconHeight );
   hOldBmp2 = SelectObject( hdcDst, hXorBitmap );
   DeleteObject( hOldBmp1 );
   DeleteObject( hOldBmp2 );
   GetObject( hBitmap, sizeof(BITMAP), (VOID FAR *)&XorBitmap );

   /* 2) Shrink / expand the bitmap to create an XOR bitmap as the same 
    *    size as that of an icon supported by the system.
    */
   StretchBlt( hdcDst, 0, 0, iIconWidth, iIconHeight,
               hdcSrc, 0, 0, XorBitmap.bmWidth, XorBitmap.bmHeight, SRCCOPY );
   DeleteDC( hdcSrc );

   /* Get the expanded/shrunk XorBitmap
    */
   GetObject( hXorBitmap, sizeof(BITMAP), (VOID FAR *)&XorBitmap );

   /* Create storage for the set of bytes to be used as the XOR mask for the icon
    */
   dwNumBytes = (DWORD)( XorBitmap.bmWidthBytes * XorBitmap.bmHeight * XorBitmap.bmPlanes );
   hXorBits = GlobalAlloc( GHND, dwNumBytes );
   if( hXorBits == NULL )
      {
      /* clean up before quitting
       */
      DeleteDC( hdc );      
      DeleteObject( hBitmap );
      DeleteDC( hdcDst );            
      DeleteObject( hXorBitmap );      
      return( NULL );
      }
   lpXorBits = GlobalLock( hXorBits );

   /* 3) Save the XOR bits in memory.
    */
   GetBitmapBits( hXorBitmap, dwNumBytes, lpXorBits );

   /* 4) Create a monochrome bitmap for the AND mask and fill it with a 
    *    pattern of your choice.
    */
   hAndBitmap = CreateBitmap( iIconWidth, iIconHeight, 1, 1, NULL );
   GetObject( hAndBitmap, sizeof(BITMAP), (VOID FAR *)&AndBitmap );
   dwNumBytes = (DWORD)( AndBitmap.bmWidthBytes * AndBitmap.bmHeight * AndBitmap.bmPlanes );
   hdcTmp = CreateCompatibleDC( hdc );
   hOldBmp3 = SelectObject( hdcTmp, hAndBitmap );
   DeleteObject( hOldBmp3 );
   DeleteDC( hdc );

   /* Fill the And bitmap with all 0s.  This will make all non-zero
    * colors in our XOR bitmap show through in the original color,
    * and all zero values (black) will show through as black.  Note
    * that since we don't know which part of the bitmap to make 
    * transparent, our AND mask dosen't represent any transparency.
    */
   PatBlt( hdcTmp, 0, 0, iIconWidth, iIconHeight, BLACKNESS );
   hAndBits = GlobalAlloc( GHND, dwNumBytes );
   if( hAndBits == NULL )
      {
      /* clean up before quitting
       */
      GlobalUnlock( hXorBits );
      GlobalFree( hXorBits );
      DeleteDC( hdcDst );
      DeleteDC( hdcTmp );            
      DeleteObject( hBitmap );
      DeleteObject( hAndBitmap );
      DeleteObject( hXorBitmap );      
      return( NULL );
      }
   lpAndBits = GlobalLock( hAndBits );

   /* 5) Save the AND bits in memory.
    */
   GetBitmapBits( hAndBitmap, dwNumBytes, lpAndBits );

   /* 6) Use the XOR and AND masks and create an icon using CreateIcon.
    */
   hIcon = CreateIcon( hInst, iIconWidth, iIconHeight, (BYTE)XorBitmap.bmPlanes, (BYTE)XorBitmap.bmBitsPixel, lpAndBits, lpXorBits );

   /* Clean up before exiting.
    */
   GlobalUnlock( hXorBits );
   GlobalFree( hXorBits );
   GlobalUnlock( hAndBits );
   GlobalFree( hAndBits );
   DeleteDC( hdcDst );
   DeleteDC( hdcTmp );
   DeleteObject( hAndBitmap );
   DeleteObject( hBitmap );
   DeleteObject( hXorBitmap );
   return( hIcon );
}

/* This function inserts a command onto the menu bar.
 */
void EXPORT WINAPI Ameol2_InsertMenu( AMEOLMENU FAR * lpam )
{
   char szCmdLabel[ 128 ];
   char szCmdName[ 40 ];
   char * pszMenuText;
   WORD wCategory;
   register int i;
   CMDREC cmd;
   int wMenu;
   WORD wKey;
   WORD wModifier;
   HMENU hMenu;
   int (*pMenuMap)[ MAX_MAP_ELEMENTS ];

   /* Is this command identical to the one added thru the last
    * call to this function?
    */
   if( fHasLast )
      {
      if( lpam->hInst == amlast.hInst &&
          lpam->wKey == amlast.wKey &&
          strcmp( lpam->lpMenuText, amlast.lpMenuText ) == 0 &&
          strcmp( lpam->lpMenuHelpText, amlast.lpMenuHelpText ) == 0 )
         {
         lpam->hMenu = amlast.hMenu;
         lpam->wID = amlast.wID;
         return;
         }
      }

   /* Ensure that the menu command ID is valid.
    */
   if( wNextCmdID > IDM_TOOLLAST )
      wNextCmdID = IDM_TOOLFIRST;

   /* Clear the return values.
    */
   lpam->hMenu = 0;
   lpam->wID = 0;

   /* Convert from old-style key to new style, while
    * also handling the new style directly (although
    * unlikely!)
    */
   if( ( wKey = lpam->wKey ) >= 0x1000 )
      {
      wModifier = wKey & 0xFF00;
      wKey &= 0x00FF;
      if( wModifier & 0x1000 )   wKey |= (HOTKEYF_CONTROL << 8);
      if( wModifier & 0x2000 )   wKey |= (HOTKEYF_SHIFT << 8);
      if( wModifier & 0x4000 )   wKey |= (HOTKEYF_ALT << 8);
      }

   /* Map the menu positions.
    */
   switch( HIBYTE( lpam->wPopupMenu ) )
      {
      case 0:     pMenuMap = &pFullMenuMap;  break;
      case 1:     pMenuMap = &pConfMenuMap;  break;
      case 2:     pMenuMap = &pTermMenuMap;  break;
      case 3:     pMenuMap = NULL;        break;
      default: pMenuMap = NULL;        break;
      }
   wMenu = LOBYTE( lpam->wPopupMenu );
   if( pMenuMap )
      wMenu = (*pMenuMap)[ wMenu ];

   // With Ameol 2.60 and later, the Mail and News menus are dropped
   // when in CIX only mode so remap commands aimed at the old location
   // of the Conf menu to the new one (pos 3 to pos 2)
   if (!fUseInternet)
      wMenu = MapMenuForA260AndLater(wMenu);
   if (fUseLegacyCIX)
      wCategory = pCIXMailOnlyCategoryMap[ wMenu ];
   else
      wCategory = pCIXOnlyCategoryMap[ wMenu ];

   /* Construct a command name based from the menu name, sans
    * spaces and short cut codes.
    */
   pszMenuText = (char *)lpam->lpMenuText;
   for( i = 0; i < 39 && *pszMenuText && *pszMenuText != '\t' && *pszMenuText != '\a'; )
      if( *pszMenuText == '&' && *(pszMenuText+1) != '&' || *pszMenuText == ' ' )
         pszMenuText++;
      else if( *pszMenuText == '.' && *(pszMenuText+1) == '.' && *(pszMenuText+2) == '.' )
         break;
      else
         szCmdName[ i++ ] = *pszMenuText++;
   szCmdName[ i ] = '\0';

   /* Convert menu name to something prettier. Any '&'s which prefix
    * short cut keys are stripped out, everything after a \t or \a is
    * deleted and any trailing '...'s are removed.
    */
   pszMenuText = (char *)lpam->lpMenuText;
   for( i = 0; i < 127 && *pszMenuText && *pszMenuText != '\t' && *pszMenuText != '\a'; )
      if( *pszMenuText == '&' && *(pszMenuText+1) != '&' )
         pszMenuText++;
      else if( *pszMenuText == '.' && *(pszMenuText+1) == '.' && *(pszMenuText+2) == '.' )
         break;
      else
         szCmdLabel[ i++ ] = *pszMenuText++;
   szCmdLabel[ i ] = '\0';

   /* Fill out a CMDREC structure for this
    * command.
    */
   cmd.iCommand = wNextCmdID++;
   cmd.lpszString = lpam->lpMenuHelpText;
   cmd.iDisabledString = 0;
   cmd.lpszTooltipString = szCmdLabel;
   cmd.lpszCommand = szCmdName;
   cmd.iToolButton = 0;
   cmd.wDefKey = wKey;
   cmd.hLib = lpam->hInst;
   cmd.iActiveMode = WIN_ALL;
   cmd.wNewKey = 0;
   cmd.wKey = wKey;
   cmd.wCategory = wCategory;
   cmd.nScheduleFlags = NSCHF_CANSCHEDULE;
   cmd.fCustomBitmap = FALSE;
   cmd.btnBmp.hbmpSmall = NULL;
   cmd.btnBmp.hbmpLarge = NULL;

   /* Insert into the tree.
    */
   if( CTree_InsertCommand( &cmd ) )
      {
      char sz[ 80 ];

      /* Add the command to the menu bar.
       */
      if( hwndActive && IsZoomed( hwndActive ) )
         ++wMenu;
      hMenu = GetSubMenu( hMainMenu, wMenu );

      /* Append the key description.
       */
      lstrcpy( sz, lpam->lpMenuText );
      wModifier = GETMODIFIER( cmd.wKey );
      wKey = GETKEY( cmd.wKey );
      strcat( sz, "\t" );
      if( wModifier & HOTKEYF_CONTROL )
         strcat( sz, "Ctrl+" );
      if( wModifier & HOTKEYF_ALT )
         strcat( sz, "Alt+" );
      if( wModifier & HOTKEYF_SHIFT )
         strcat( sz, "Shift+" );
      if( wKey )
         Amctl_NewGetKeyNameText( wKey, sz + strlen( sz ), ( sizeof( sz ) -  strlen( sz ) ) - 1 );
      InsertMenu( hMenu, lpam->wMenuPos, MF_BYPOSITION, cmd.iCommand, sz );

      /* Only redraw the menu bar if we're not initialising,
       * or terminating. Makes for a smoother initialisation.
       */
      if( !fInitialising && !fQuitting )
         DrawMenuBar( hwndFrame );
      lpam->hMenu = hMenu;
      lpam->wID = cmd.iCommand;

      /* Save this structure.
       */
      amlast = *lpam;
      fHasLast = TRUE;
      }
}

int FASTCALL MapMenuForA260AndLater(int wMenu)
{
   if (wMenu == 3 && !fUseLegacyCIX)
      return 2;
   if (wMenu == 2 && !fUseLegacyCIX)
   {
      // Trying to add to the Mail menu but CIX Mail isn't
      // available, so just return.
      return wMenu;
   }
   if (wMenu == 2 || wMenu == 4)
   {
      // Trying to add to the Mail or News menus in CIX mode
      // but they don't exist. So just return.
      return wMenu;
   }
   if (wMenu > 4)
      return wMenu - ((fUseLegacyCIX) ? 1 : 2);
   return wMenu;
}

/* This function extracts the bitmap for an addon command. The bitmap
 * is scaled to the appropriate size if one is not already supplied
 * to specification.
 */
HBITMAP FASTCALL GetAddonToolbarBitmap( CMDREC FAR * pmsi )
{
   HBITMAP hBmp;

   if( fLargeButtons )
      {
      if( NULL != pmsi->btnBmp.hbmpLarge )
         DeleteBitmap( pmsi->btnBmp.hbmpLarge );
      hBmp = pmsi->btnBmp.hbmpLarge = GetAddonBitmap( pmsi->hLib, pmsi->iCommand, 24, 24 );
      }
   else
      {
      if( NULL != pmsi->btnBmp.hbmpSmall )
         DeleteBitmap( pmsi->btnBmp.hbmpSmall );
      hBmp = pmsi->btnBmp.hbmpSmall = GetAddonBitmap( pmsi->hLib, pmsi->iCommand, 16, 16 );
      }
   pmsi->fCustomBitmap = TRUE;
   return( hBmp );
}

/* This function calls the QUERY_SUPPORT_URL interface in the
 * addon.
 */
BOOL FASTCALL GetAddonSupportURL( HINSTANCE hInst, char * lpBuffer, int cbMax )
{
   ADN_QUERYADDONPROC lpQueryAddonProc;

   /* Call the QueryAddon interface.
    */
   if( NULL != ( lpQueryAddonProc = (ADN_QUERYADDONPROC)GetProcAddress( hInst, szQueryAddon ) ) )
      {
      SUPPORT_URL su;

      su.fCIXService = fUseCIX;
      su.fIPService = fUseInternet;
      su.lpURLStr = lpBuffer;
      su.cchURLStr = cbMax;
      if( (*lpQueryAddonProc)( QUERY_SUPPORT_URL, &su ) )
         return( TRUE );
      }
   return( FALSE );
}

/* This function extracts the bitmap for an addon command.
 */
HBITMAP FASTCALL GetAddonBitmap( HINSTANCE hInst, int iCommand, int cxButton, int cyButton )
{
   ADN_QUERYADDONPROC lpQueryAddonProc;
   HBITMAP hbmpApp;

   /* First try the QueryAddon interface.
    */
   if( NULL != ( lpQueryAddonProc = (ADN_QUERYADDONPROC)GetProcAddress( hInst, szQueryAddon ) ) )
      {
      CMDBUTTONBITMAP btnBmp;

      btnBmp.wID = (WORD)iCommand;
      btnBmp.hLib = NULL;
      btnBmp.cxRequired = cxButton;
      btnBmp.cyRequired = cyButton;
      if( (*lpQueryAddonProc)( QUERY_CMD_BUTTON_BITMAP, &btnBmp ) )
         {
         if( NULL != btnBmp.hLib )
            hbmpApp = LoadBitmap( btnBmp.hLib, MAKEINTRESOURCE( btnBmp.uBitmap ) );
         else
            hbmpApp = (HBITMAP)btnBmp.uBitmap;
         return( ExpandBitmap( hbmpApp, cxButton, cyButton ) );
         }
      }

   /* First look for resource ID 6000 in the addon bitmap
    * table. This will be the wrong size, but it's a start...
    */
   hbmpApp = LoadBitmap( hInst, MAKEINTRESOURCE( 6000 ) );

   /* Get the first icon in the addon. If none found, use
    * the default addon icon instead.
    */
   if( NULL == hbmpApp )
      return( NULL );

   /* Convert the icon to a bitmap.
    */
   return( ExpandBitmap( hbmpApp, cxButton, cyButton ) );
}

/* This function expands a bitmap to the required size.
 */
HBITMAP FASTCALL ExpandBitmap( HBITMAP hbmpApp, int cxButton, int cyButton )
{
   HBITMAP hOldBmp1;
   HBITMAP hOldBmp2;
   BITMAP bmp;
   HBITMAP hbmp;
   HDC hdc2;
   HDC hdc3;
   HDC hdc;

   /* Return bitmap now if it is the correct size.
    */
   GetObject( hbmpApp, sizeof(BITMAP), &bmp );
   if( bmp.bmHeight == cyButton && bmp.bmWidth == cxButton )
      return( hbmpApp );

   /* Now we convert the icon to a bitmap. Note that
    * the icons are assumed to be 32x32.
    */
   hdc = GetDC( hwndFrame );

   /* Create two DCs for the source and target bitmap which are
    * compatible with the current DC.
    */
   hdc2 = CreateCompatibleDC( hdc );
   hdc3 = CreateCompatibleDC( hdc );
   hbmp = CreateCompatibleBitmap( hdc, cxButton, cyButton );

   /* This is where we do a simple stretchblt between the old
    * and new toolbar bitmaps.
    */
   hOldBmp1 = SelectBitmap( hdc2, hbmpApp );
   hOldBmp2 = SelectBitmap( hdc3, hbmp );
   StretchBlt( hdc3, 0, 0, cxButton, cyButton, hdc2, 0, 0, bmp.bmWidth, bmp.bmHeight, SRCCOPY );
   SelectBitmap( hdc2, hOldBmp1 );
   SelectBitmap( hdc3, hOldBmp2 );
   DeleteBitmap( hbmpApp );

   /* Clean up for all.
    */
   DeleteDC( hdc2 );
   DeleteDC( hdc3 );
   ReleaseDC( hwndFrame, hdc );
   return( hbmp );
}

/* This function converts an icon to a bitmap. The icon is
 * destroyed after conversion.
 */
HBITMAP FASTCALL IconToBitmap( HICON hIcon, int cxButton, int cyButton )
{
   HBITMAP hOldBmp1;
   HBITMAP hOldBmp2;
   HBITMAP hbmp2;
   HBITMAP hbmp;
   HBRUSH hbr;
   HDC hdc2;
   HDC hdc3;
   RECT rc;
   HDC hdc;

   /* Now we convert the icon to a bitmap. Note that
    * the icons are assumed to be 32x32.
    */
   hdc = GetDC( hwndFrame );

   /* Create two DCs for the source and target bitmap which are
    * compatible with the current DC.
    */
   hdc2 = CreateCompatibleDC( hdc );
   hdc3 = CreateCompatibleDC( hdc );
   hbmp = CreateCompatibleBitmap( hdc, cxButton, cyButton );
   hbmp2 = CreateCompatibleBitmap( hdc, 32, 32 );
   hOldBmp1 = SelectBitmap( hdc2, hbmp2 );
   hOldBmp2 = SelectBitmap( hdc3, hbmp );

   /* Set the target bitmap background to grey which is treated as
    * transparent by the toolbar control.
    */
   hbr = CreateSolidBrush( RGB(192, 192, 192) );
   SetRect( &rc, 0, 0, 32, 32 );
   FillRect( hdc2, &rc, hbr );
   DeleteBrush( hbr );

   /* Draw the icon on the button and stretch it to the target
    * bitmap to fit.
    */
   DrawIcon( hdc2, 0, 0, hIcon );
   StretchBlt( hdc3, 0, 0, cxButton, cyButton, hdc2, 0, 0, 32, 32, SRCCOPY );

   /* Clean up.
    */
   SelectBitmap( hdc2, hOldBmp1 );
   SelectBitmap( hdc3, hOldBmp2 );
   DeleteBitmap( hbmp2 );
   DestroyIcon( hIcon );

   /* Clean up for all.
    */
   DeleteDC( hdc2 );
   DeleteDC( hdc3 );
   ReleaseDC( hwndFrame, hdc );
   return( hbmp );
}

/* This function removes a command from the Ameol menu bar.
 */
void EXPORT WINAPI Ameol2_DeleteMenu( HMENU hMenu, WORD iCommand )
{
   CMDREC cmd;

   cmd.iCommand = (UINT)iCommand;
   if( CTree_GetCommand( &cmd ) )
      {
      RemoveMenu( hMenu, iCommand, MF_BYCOMMAND );
      if( !fInitialising && !fQuitting )
         DrawMenuBar( hwndFrame );
      CTree_DeleteCommand( iCommand );
      }
}

/* This function enumerates all installed addons, filling the
 * lpAddonInfo structure with details about the addon.
 */
LPADDON EXPORT WINAPI Ameol2_EnumAllTools( LPADDON lpAddon, LPADDONINFO lpAddonInfo )
{
   ADN_VERSIONPROC lpVerProc;

   lpAddon = ( NULL == lpAddon ) ? lpadnFirst : lpAddon->lpadnNext;
   lpAddonInfo->dwVersion = 0;
   if( lpAddon )
      if( ( lpVerProc = (ADN_VERSIONPROC)GetProcAddress( lpAddon->hLib, szAddonVersion ) ) != NULL )
         {
         LPVERSION lpVersion;
         LPSTR lpFileName;
         LPSTR lpText;
         register int c;

         lpVersion = (*lpVerProc)();
         lpAddonInfo->dwVersion = lpVersion->dwVersion;
         lpAddonInfo->hLib = lpAddon->hLib;
         lpText = lpAddon->szCommandLine;
         lpFileName = lpText;
         while( *lpText )
            {
            if( *lpText == ':' || *lpText == '\\' )
               lpFileName = lpText + 1;
            ++lpText;
            }
         for( c = 0; *lpFileName && c < 12; ++c )
            lpAddonInfo->szFileName[ c ] =  *lpFileName++;
         lpAddonInfo->szFileName[ c ] = '\0';
         lstrcpy( lpAddonInfo->szAuthor, lpVersion->szAuthor );
         lstrcpy( lpAddonInfo->szDescription, lpVersion->szDescription );
         }
   return( lpAddon );
}

/* This function displays the Configure Addons dialog.
 */
BOOL FASTCALL CmdConfigureAddons( HWND hwnd )
{
   AMPROPSHEETHEADER psh;

   psh.dwSize = sizeof( AMPROPSHEETHEADER );
   psh.dwFlags = PSH_PROPSHEETPAGE|PSH_USECALLBACK|PSH_HASHELP;
   if( fMultiLineTabs )
      psh.dwFlags |= PSH_MULTILINETABS;
   psh.hwndParent = hwnd;
   psh.hInstance = hInst;
   psh.pszIcon = NULL;
   psh.pszCaption = "Configure Addons";
   psh.nPages = 0;
   psh.nStartPage = 0;
   psh.pfnCallback = ConfigAddonCallbackFunc;
   psh.ppsp = NULL;
   if( Amctl_PropertySheet(&psh ) == -2 )
      {
      fMessageBox( hwnd, 0, GS(IDS_STR1163), MB_OK|MB_ICONINFORMATION );
      return( FALSE );
      }
   return( TRUE );
}

/* This function is called as soon as the property sheet has
 * finished initialising. We raise a AE_PREFSDIALOG event to allow
 * addons to attach their own property pages.
 */
UINT CALLBACK EXPORT ConfigAddonCallbackFunc( HWND hwnd, UINT uMsg, LPARAM lParam )
{
   if( PSCB_INITIALIZED == uMsg )
      Amuser_CallRegistered( AE_CONFIGADDONDLG, (LPARAM)(LPSTR)hwnd, 0L );
   PropSheet_SetCurSel( hwnd, 0L, 0 );
   return( 0 );
}

BOOL EXPORT WINAPI Ameol2_UnloadAddon( LPADDON lpaddon )
{
   UnloadAddon( lpaddon );
   return( UninstallAddon( lpaddon ) );
}

LPADDON EXPORT WINAPI Ameol2_LoadAddon( LPSTR lpszFilename )
{
   LPADDON lpaddon;
   LPADDON lpReturnAddon;
   LPADDON lpadnFirst;
   LPADDON lpadnLast;
   int count = 0;
   ADDONINFO amti;

   /* Load the AMEOL.DLL library to guard against legacy addons
    * which need the old API.
    */
   hAmeolLib = NULL;
   if( FindLibrary( szAmeolDll ) )
      hAmeolLib = LoadLibrary( szAmeolDll );

   if( NULL != ( lpReturnAddon = InstallAddon( hwndFrame, lpszFilename ) ) )
   {
      lpReturnAddon->fInstalled = TRUE;
      lpadnFirst = NULL;
      lpadnLast = NULL;
      for( lpaddon = (LPADDON)Ameol2_EnumAllTools( NULL, &amti); lpaddon; lpaddon = (LPADDON)Ameol2_EnumAllTools( (LPVOID)lpaddon, &amti ) )
      {
         count++;
         lpadnLast = lpaddon;
      }
      if( count == 0 )
         lpadnFirst = lpReturnAddon;
      else
         lpadnLast->lpadnNext = lpReturnAddon;
      if( !LoadAddon( lpReturnAddon ) )
      {
         if( hAmeolLib )
            FreeLibrary( hAmeolLib );
         ShowUnreadMessageCount();
         return( NULL );
      }
      lpReturnAddon->fInstalled = FALSE;
      lpadnLast = lpReturnAddon;
   }
   if( hAmeolLib )
      FreeLibrary( hAmeolLib );
   SaveInstalledAddons();
   ShowUnreadMessageCount();
   return( lpReturnAddon );
}

BOOL FASTCALL IsAddonLoaded( LPSTR lpszFilename )
{
   LPADDON lpaddon;
   ADDONINFO amti;

   for( lpaddon = (LPADDON)Ameol2_EnumAllTools( NULL, &amti); lpaddon; lpaddon = (LPADDON)Ameol2_EnumAllTools( (LPVOID)lpaddon, &amti ) )
   {
      if( _strcmpi( lpaddon->szCommandLine, lpszFilename ) == 0 )
         return( TRUE );
   }
   return( FALSE );
}
