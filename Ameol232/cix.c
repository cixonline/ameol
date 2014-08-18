/* CIX.C - CIX driver main program
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

#define  THIS_FILE   __FILE__

#include "main.h"
#include "resource.h"
#include "amlib.h"
#include <ctype.h>
#include "shared.h"
#include "cix.h"
#include "command.h"
#include "ameol2.h"
#include <string.h>
#include "toolbar.h"
#include "intcomms.h"
#include "hxfer.h"
#include "blinkman.h"
#include "news.h"
#include "editmap.h"

static BOOL fDefDlgEx = FALSE;

static int nCIXPrefsPage;        /* Index of CIX preferences page */
static int nCIXPropertiesPage;   /* Index of CIX properties page */
static int nCIXPropertiesPage2;  /* Index of 2nd CIX properties page */

HMENU hCIXMenu;                  /* Handle of CIX popup menu */

HFONT hAboutFont;

#define GETAECOMMANDID(l1,l2)    (LOWORD((l2)))

/* Some macros to save my fingers...
 */
#define  HK_ALT         (HOTKEYF_ALT)
#define  HK_SHIFT    (HOTKEYF_SHIFT)
#define  HK_CTRL        (HOTKEYF_CONTROL)
#define  HK_CTRLSHIFT   (HOTKEYF_SHIFT|HOTKEYF_CONTROL)
#define  MIR            MAKEINTRESOURCE

#pragma data_seg("CMDTABLE")
static MENUSELECTINFO FAR wIDs[] = {
   { "FileScripts",        IDM_SCRIPTS,         MIR(IDS_SCRIPTS),       0,             MIR(IDS_TT_SCRIPTS),       TB_SCRIPTS,          0,                WIN_ALL,          CAT_FILE_MENU,    NSCHF_NONE },
   { "CixJoin",            IDM_JOINCONFERENCE,     MIR(IDS_JOINCONFERENCE),   0,             MIR(IDS_TT_JOINCONFERENCE),      TB_JOINCONFERENCE,      0,                WIN_ALL,          CAT_CIX_MENU,     NSCHF_NONE },
   { "FolderResign",       IDM_RESIGN,          MIR(IDS_RESIGN),        IDS_X_RESIGN,     MIR(IDS_TT_RESIGN),           TB_RESIGNCIX,        MAKEKEY( HK_CTRL,'S'),  WIN_ALL,          CAT_CIX_MENU,     NSCHF_NONE },
   { "CixShowConferences",    IDM_SHOWCIXCONFS,    MIR(IDS_SHOWCIXCONFS),     0,             MIR(IDS_TT_SHOWCIXCONFS),     TB_SHOWCIXCONFS,     0,                WIN_ALL,          CAT_CIX_MENU,     NSCHF_NONE },
   { "CixGetConferences",     IDM_GETCIXCONFS,     MIR(IDS_GETCIXCONFS),      0,             MIR(IDS_TT_GETCIXCONFS),      TB_GETCIXCONFS,         0,                WIN_ALL,          CAT_CIX_MENU,     NSCHF_NONE },
   { "CixShowUsers",       IDM_SHOWALLUSERS,    MIR(IDS_SHOWALLUSERS),     0,             MIR(IDS_TT_SHOWALLUSERS),     TB_SHOWALLUSERS,     0,                WIN_ALL,          CAT_CIX_MENU,     NSCHF_NONE },
   { "CixFileList",        IDM_FILELIST,        MIR(IDS_FILELIST),         0,             MIR(IDS_TT_FILELIST),         TB_FILELIST,         MAKEKEY(0,'F'),         WIN_THREAD|WIN_MESSAGE, CAT_CIX_MENU,     NSCHF_NONE },
   { "CixFileListFind",    IDM_FILELISTFIND,    MIR(IDS_FILELISTFIND),     0,             MIR(IDS_TT_FILELISTFIND),     TB_FILELISTFIND,     0,                WIN_ALL,          CAT_FILE_MENU,    NSCHF_NONE },
   { "CixDownloadFile",    IDM_DOWNLOAD,        MIR(IDS_DOWNLOAD),         0,             MIR(IDS_TT_DOWNLOAD),         TB_DOWNLOAD,         MAKEKEY(HK_CTRL,'D'),   WIN_ALL,          CAT_CIX_MENU,     NSCHF_NONE },
   { "CixUploadFile",         IDM_UPLOADFILE,         MIR(IDS_UPLOADFILE),    0,             MIR(IDS_TT_UPLOADFILE),       TB_UPLOADFILE,       0,                WIN_ALL,          CAT_CIX_MENU,     NSCHF_NONE },
   { "CixParticipants",    IDM_PARTICIPANTS,    MIR(IDS_PARTICIPANTS),     0,             MIR(IDS_TT_PARTICIPANTS),     TB_PARTICIPANTS,     MAKEKEY(0,VK_F4),    WIN_ALL,          CAT_CIX_MENU,     NSCHF_NONE },
   { "CixEditResume",         IDM_OWNRESUME,       MIR(IDS_OWNRESUME),        0,             MIR(IDS_TT_OWNRESUME),        TB_OWNRESUME,        0,                WIN_ALL,          CAT_CIX_MENU,     NSCHF_NONE },
   { "CixResumes",            IDM_RESUMES,         MIR(IDS_RESUMES),       0,             MIR(IDS_TT_RESUMES),       TB_RESUMES,          0,                WIN_ALL,          CAT_CIX_MENU,     NSCHF_NONE },
   { "CixShowResume",         IDM_SHOWRESUME,         MIR(IDS_SHOWRESUME),    0,             MIR(IDS_TT_SHOWRESUME),       TB_SHOWRESUME,       MAKEKEY(HK_CTRL,'R'),   WIN_ALL,          CAT_MESSAGE_MENU, NSCHF_NONE },
   { "CixRepost",          IDM_REPOST,          MIR(IDS_REPOST),        0,             MIR(IDS_TT_REPOST),           TB_REPOST,           MAKEKEY(0,'E'),         WIN_THREAD|WIN_MESSAGE, CAT_MESSAGE_MENU, NSCHF_NONE },
   { "CixMailDirectory",      IDM_MAILDIRECTORY,      MIR(IDS_MAILDIRECTORY),    0,             MIR(IDS_TT_CIXMAILDIRECTORY), TB_CIXMAILDIRECTORY, 0,                WIN_ALL,          CAT_MAIL_MENU,    NSCHF_NONE },
   { "CixMailRename",         IDM_MAILRENAME,         MIR(IDS_MAILRENAME),    0,             MIR(IDS_TT_CIXMAILRENAME),    TB_CIXMAILRENAME,    0,                WIN_ALL,          CAT_MAIL_MENU,    NSCHF_NONE },
   { "CixMailExport",         IDM_MAILEXPORT,         MIR(IDS_MAILEXPORT),    0,             MIR(IDS_TT_CIXMAILEXPORT),    TB_CIXMAILEXPORT,    0,                WIN_ALL,          CAT_MAIL_MENU,    NSCHF_NONE },
   { "CixMailSendFile",    IDM_SENDFILE,        MIR(IDS_SENDFILE),         0,             MIR(IDS_TT_SENDFILE),         TB_SENDFILE,         0,                WIN_ALL,          CAT_MAIL_MENU,    NSCHF_NONE },
   { "CixMailErase",       IDM_MAILERASE,       MIR(IDS_MAILERASE),        0,             MIR(IDS_TT_CIXMAILERASE),     TB_CIXMAILERASE,     0,                WIN_ALL,          CAT_MAIL_MENU,    NSCHF_NONE },
   { "CixMailUpload",         IDM_MAILUPLOAD,         MIR(IDS_MAILUPLOAD),    0,             MIR(IDS_TT_CIXMAILUPLOAD),    TB_CIXMAILUPLOAD,    0,                WIN_ALL,          CAT_MAIL_MENU,    NSCHF_NONE },
   { "CixMailDownload",    IDM_MAILDOWNLOAD,    MIR(IDS_MAILDOWNLOAD),     0,             MIR(IDS_TT_CIXMAILDOWNLOAD),  TB_CIXMAILDOWNLOAD,     0,                WIN_ALL,          CAT_MAIL_MENU,    NSCHF_NONE },
   { "CixClearMail",       IDM_CLEARCIXMAIL,    MIR(IDS_CLEARCIXMAIL),     0,             MIR(IDS_TT_CLEARCIXMAIL),     TB_CLEARCIXMAIL,     0,                WIN_ALL,          CAT_MAIL_MENU,    NSCHF_CANSCHEDULE },
   { "CixSettings",        IDM_CIXSETTINGS,     MIR(IDS_CIXSETTINGS),      0,             MIR(IDS_TT_CIXSETTINGS),      TB_CIXSETTINGS,         0,                WIN_ALL,          CAT_SETTINGS_MENU,   NSCHF_NONE },
   { "MailSettings",       IDM_MAILSETTINGS,    MIR(IDS_MAILSETTINGS),     0,             MIR(IDS_TT_MAILSETTINGS),     TB_MAILSETTINGS,     0,                WIN_ALL,             CAT_SETTINGS_MENU,   NSCHF_NONE },
   { "UpdateFileLists",    IDM_UPDFILELIST,     MIR(IDS_UPDFILELIST),      0,             MIR(IDS_TT_UPDFILELIST),      TB_UPDFILELIST,         0,                WIN_ALL|WIN_NOMENU,     CAT_KEYBOARD,     NSCHF_NEEDFOLDER },
   { "UpdatePartLists",    IDM_UPDPARTLIST,     MIR(IDS_UPDPARTLIST),      0,             MIR(IDS_TT_UPDPARTLIST),      TB_UPDPARTLIST,         0,                WIN_ALL|WIN_NOMENU,     CAT_KEYBOARD,     NSCHF_NEEDFOLDER },
   { "UpdateCixList",         IDM_UPDCIXLIST,         MIR(IDS_UPDCIXLIST),    0,             MIR(IDS_TT_UPDCIXLIST),       TB_UPDCIXLIST,       0,                WIN_ALL|WIN_NOMENU,     CAT_KEYBOARD,     NSCHF_CANSCHEDULE },
   { "UpdateUsersList",    IDM_UPDUSERSLIST,    MIR(IDS_UPDUSERSLIST),     0,             MIR(IDS_TT_UPDUSERSLIST),     TB_UPDUSERSLIST,     0,                WIN_ALL|WIN_NOMENU,     CAT_KEYBOARD,     NSCHF_CANSCHEDULE },
   { "UpdateConfNotes",    IDM_UPDCONFNOTES,    MIR(IDS_UPDCONFNOTES),     0,             MIR(IDS_TT_UPDCONFNOTES),     TB_UPDCONFNOTES,     0,                WIN_ALL|WIN_NOMENU,     CAT_KEYBOARD,     NSCHF_NEEDFOLDER },
   { NULL,                 (UINT)-1,            MIR(-1),             (UINT)-1,         MIR(-1),                0,                0,                0 }
   };
#pragma data_seg()

extern int idRasTab;
void FASTCALL EnableDisableRASTab( HWND );

void FASTCALL AppendPopup( HMENU, HMENU );

extern char * pClrList[ 9 ];
extern int nClrRate[ 9 ];
extern char * pCountList[ 8 ];
extern int nClearCount[ 8 ];
extern char szCixProtocol[ 16 ];

char szIllegalCixFilenameChrs[] = "{}\"'$;&()|^<> \t";

void FASTCALL CommonScheduleDialog( HWND, char * );
void FASTCALL Common_FillClearRateList( HWND, int, int );
void FASTCALL Common_FillClearCountList( HWND, int, int );

BOOL WINAPI CIX_Popup( int, LPARAM, LPARAM );
BOOL WINAPI CIX_PropertiesDialog( int, LPARAM, LPARAM );
BOOL WINAPI CIX_BlinkPropertiesDialog( int, LPARAM, LPARAM );
BOOL WINAPI CIX_Command( int, LPARAM, LPARAM );
BOOL WINAPI CIX_DeletingFolder( int, LPARAM, LPARAM );

BOOL EXPORT CALLBACK CIXSettings( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL CIXSettings_DlgProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL CIXSettings_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL CIXSettings_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK InBasket_CIX( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL InBasket_CIX_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL InBasket_CIX_OnNotify( HWND, int, LPNMHDR );
void FASTCALL InBasket_CIX_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK InBasket_CIX2( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL InBasket_CIX2_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL InBasket_CIX2_OnNotify( HWND, int, LPNMHDR );
void FASTCALL InBasket_CIX2_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK CIXBlinkProperties( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL CIXBlinkProperties_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL CIXBlinkProperties_OnNotify( HWND, int, LPNMHDR );
void FASTCALL CIXBlinkProperties_OnCommand( HWND, int, HWND, UINT );

BOOL FASTCALL UpdateConfNote( HWND, PCL, BOOL *, BOOL );
BOOL FASTCALL UpdateDatabaseConfNotes( PCAT, BOOL * );
BOOL FASTCALL UpdateFolderConfNotes( PCL, BOOL * );

/* This function registers all out-basket object types.
 */
BOOL FASTCALL LoadCIX( void )
{
   register int c;

   /* First, register some events.
    */
   Amuser_RegisterEvent( AE_POPUP, CIX_Popup );
   Amuser_RegisterEvent( AE_PROPERTIESDIALOG, CIX_PropertiesDialog );
   Amuser_RegisterEvent( AE_BLINKPROPERTIESDIALOG, CIX_BlinkPropertiesDialog );
   Amuser_RegisterEvent( AE_INBASK_FOLDER_DELETING, CIX_DeletingFolder );
   Amuser_RegisterEvent( AE_COMMAND, CIX_Command );

   /* Add categories for CIX commands.
    */
   if (fUseLegacyCIX)
      CTree_AddCategory( CAT_MAIL_MENU, 2, "Mail" );
   CTree_AddCategory( CAT_CIX_MENU, 3, "CIX Forums" );

   /* Install default commands into the command table.
    */
   for( c = 0; wIDs[ c ].iCommand != -1; ++c )
      {
      CMDREC cmd;

      /* Set CMDREC record from the MENUSELECTINFO table.
       */
      cmd.lpszCommand = wIDs[ c ].lpszCommand;
      cmd.iCommand = wIDs[ c ].iCommand;
      cmd.lpszString = wIDs[ c ].lpszString;
      cmd.iDisabledString = wIDs[ c ].iDisabledString;
      cmd.lpszTooltipString = wIDs[ c ].lpszTooltipString;
      cmd.iToolButton = wIDs[ c ].iToolButton;
      cmd.wDefKey = wIDs[ c ].wDefKey;
      cmd.iActiveMode = wIDs[ c ].iActiveMode;
      cmd.wCategory = wIDs[ c ].wCategory;
      cmd.nScheduleFlags = wIDs[ c ].nScheduleFlags;
      cmd.fCustomBitmap = FALSE;

      /* Insert this command. If we can't, then fail
       */
      if( !CTree_InsertCommand( &cmd ) )
         {
         OutOfMemoryError( NULL, FALSE, FALSE );
         return( FALSE );
         }
      }

   /* Next, register out-basket types used by the
    * CIX service.
    */
   if( !Amob_RegisterObjectClass( OBTYPE_JOINCONFERENCE, BF_POSTCIXMSGS, ObProc_JoinConference ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_WITHDRAW, BF_POSTCIXMSGS, ObProc_Withdraw ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_RESTORE, BF_POSTCIXMSGS, ObProc_Restore ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_SAYMESSAGE, BF_POSTCIXMSGS, ObProc_SayMessage ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_COMMENTMESSAGE, BF_POSTCIXMSGS, ObProc_CommentMessage ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_GETUSERLIST, BF_POSTCIXMSGS, ObProc_GetUserList ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_GETCIXLIST, BF_POSTCIXMSGS, ObProc_GetCIXList ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_DOWNLOAD, BF_POSTCIXMSGS, ObProc_Download ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_GETMESSAGE, BF_GETCIXMSGS, ObProc_GetCIXMessage ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_CIXRESIGN, BF_POSTCIXMSGS, ObProc_CIXResign ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_INCLUDE, BF_POSTCIXMSGS, ObProc_Include ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_PREINCLUDE, BF_GETCIXMSGS, ObProc_Include ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_INLINE, BF_GETCIXMSGS, ObProc_Inline ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_FILELIST, BF_GETCIXMSGS, ObProc_Filelist ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_FDIR, BF_GETCIXMSGS, ObProc_Fdir ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_GETPARLIST, BF_GETCIXMSGS, ObProc_GetParList ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_BANPAR, BF_POSTCIXMSGS, ObProc_BanPar ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_UNBANPAR, BF_POSTCIXMSGS, ObProc_UnBanPar ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_ADDPAR, BF_POSTCIXMSGS, ObProc_AddPar ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_REMPAR, BF_POSTCIXMSGS, ObProc_RemPar ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_COMOD, BF_POSTCIXMSGS, ObProc_CoMod ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_EXMOD, BF_POSTCIXMSGS, ObProc_ExMod ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_RESUME, BF_GETCIXMSGS, ObProc_GetResume ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_PUTRESUME, BF_POSTCIXMSGS, ObProc_PutResume ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_EXECCIXSCRIPT, BF_CIXFLAGS, ObProc_ExecCIXScript ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_FUL, BF_POSTCIXMSGS, ObProc_FileUpload ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_COPYMSG, BF_POSTCIXMSGS, ObProc_CopyMsg ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_MAILLIST, BF_POSTCIXMAIL, ObProc_MailDirectory ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_MAILERASE, BF_POSTCIXMAIL, ObProc_MailErase ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_MAILRENAME, BF_POSTCIXMAIL, ObProc_MailRename ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_MAILDOWNLOAD, BF_POSTCIXMAIL, ObProc_MailDownload ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_MAILEXPORT, BF_POSTCIXMAIL, ObProc_MailExport ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_MAILUPLOAD, BF_POSTCIXMAIL, ObProc_MailUpload ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_CLEARCIXMAIL, BF_POSTCIXMAIL, ObProc_ClearCIXMail ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_BINMAIL, BF_POSTCIXMAIL, ObProc_Binmail ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_UPDATENOTE, BF_GETCIXMSGS, ObProc_UpdateNote ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_EXECVERSCRIPT, BF_CIXFLAGS, ObProc_ExecVerScript ) )
      return( FALSE );
   
   return( TRUE );
}

/* Initialise the CIX Conferencing service interface.
 */
BOOL FASTCALL InitialiseCIXInterface( HWND hwnd )
{
   HMENU hFileMenu;
   HMENU hMsgMenu;
   HMENU hMailMenu;
   HMENU hMenu;
   int nOffset = 0;

   /* Insert the CIX menu.
    */
   hMenu = LoadMenu( hRscLib, MAKEINTRESOURCE(IDMNU_CIX) );
   hCIXMenu = GetSubMenu( hMenu, 0 );
   InsertMenu( hMainMenu, 2, MF_BYPOSITION|MF_POPUP, (UINT)hCIXMenu, "CIX Fo&rums" );

   /* Insert the CIX mail commands to the end of the
    * Conferencing menu.
    */
   if (fUseLegacyCIX)
   {
      InsertMailMenu();
      hMenu = LoadMenu( hRscLib, MAKEINTRESOURCE(IDMNU_CIXMAILDIR) );
      hMailMenu = GetSubMenu( hMainMenu, 2 );
      if (fUseInternet)
         AppendMenu( hMailMenu, MF_SEPARATOR, -1, NULL );
      AppendPopup( hMailMenu, GetSubMenu( hMenu, 0 ) );
      DestroyMenu( hMenu );
      AppendMenu( hMailMenu, MF_STRING, IDM_CLEARCIXMAIL, "C&lear CIX Mail" );
      nOffset = 1;  // Offset to account for Mail menu added.
   }

   /* Add CIX specific commands to the Message menu.
    */
   hMsgMenu = GetSubMenu( hMainMenu, 5 + nOffset );
   InsertMenu( hMsgMenu, 6, MF_BYPOSITION|MF_STRING, IDM_REPOST, "R&e-Post" );
   AppendMenu( hMsgMenu, MF_SEPARATOR, 0, NULL );
   AppendMenu( hMsgMenu, MF_STRING, IDM_SHOWRESUME, "S&how Resume..." );

   /* Delete the Forward Message command if no internet.
    */
   if (!fUseInternet)
      DeleteMenu( hMsgMenu, IDM_FORWARDMSG, MF_BYCOMMAND);

   /* Add CIX Settings to the Settings menu.
    */
   hMenu = GetSubMenu( hMainMenu, 6 + nOffset );
   InsertMenu( hMenu, 8, MF_BYPOSITION|MF_STRING, IDM_CIXSETTINGS, "&Forums..." );
   if (fUseLegacyCIX)
      InsertMenu( hMenu, 5, MF_BYPOSITION|MF_STRING, IDM_MAILSETTINGS, "&Mail..." );

   /* Add File List Find
    */
   hFileMenu = GetSubMenu( hMainMenu, 0 );
   InsertMenu( hFileMenu, 5, MF_BYPOSITION|MF_STRING, IDM_SCRIPTS, "&Scripts..." );
   InsertMenu( hFileMenu, 7, MF_BYPOSITION|MF_STRING, IDM_FILELISTFIND, "Fin&d in File Lists..." );

   return( TRUE );
}

/* This function appends all commands from the specified popup menu
 * to the end of the destination menu.
 */
void FASTCALL AppendPopup( HMENU hDstMenu, HMENU hSrcMenu )
{
   register int c;
   int count;

   count = GetMenuItemCount( hSrcMenu );
   for( c = 0; c < count; ++c )
      {
      char sz[ 100 ];

      GetMenuString( hSrcMenu, c, sz, sizeof(sz), MF_BYPOSITION );
      if( '\0' == *sz )
         AppendMenu( hDstMenu, MF_SEPARATOR, 999, NULL );
      else
         {
         UINT wID;

         wID = GetMenuItemID( hSrcMenu, c );
         AppendMenu( hDstMenu, MF_STRING, wID, sz );
         }
      }
}

/* This function returns the parameter for the specified
 * service.
 */
int FASTCALL GetCIXSystemParameter( char * pszKey, char * pszBuf, int cbMax )
{
   if( lstrcmpi( pszKey, "User" ) == 0 )
      {
      lstrcpyn( pszBuf, szCIXNickname, cbMax );
      return( lstrlen( pszBuf ) );
      }
   return( 0 );
}

/* This callback is called when the service is unloaded.
 */
void FAR PASCAL UnloadCIX( int nWhyAmIBeingKilled )
{
   Amuser_UnregisterEvent( AE_POPUP, CIX_Popup );
   Amuser_UnregisterEvent( AE_PROPERTIESDIALOG, CIX_PropertiesDialog );
   Amuser_UnregisterEvent( AE_BLINKPROPERTIESDIALOG, CIX_BlinkPropertiesDialog );
   Amuser_UnregisterEvent( AE_INBASK_FOLDER_DELETING, CIX_DeletingFolder );
   Amuser_UnregisterEvent( AE_COMMAND, CIX_Command );
}

/* This callback function is called when the user chooses a
 * menu command or presses a keystroke corresponding to a menu
 * command.
 */
BOOL WINAPI EXPORT CIX_Command( int wEvent, LPARAM lParam1, LPARAM lParam2 )
{
   switch( GETAECOMMANDID(lParam1, lParam2) )
      {
      case IDM_CIXSETTINGS:
         Adm_Dialog( hInst, hwndFrame, MAKEINTRESOURCE(IDDLG_CIXSETTINGS), CIXSettings, 0L );
         break;

      case IDM_CLEARCIXMAIL:
         Amob_New( OBTYPE_CLEARCIXMAIL, NULL );
         if( !Amob_Find( OBTYPE_CLEARCIXMAIL, NULL ) )
            Amob_Commit( NULL, OBTYPE_CLEARCIXMAIL, NULL );
         Amob_Delete( OBTYPE_CLEARCIXMAIL, NULL );
         break;

      case IDM_MAILDIRECTORY:
         CmdMailDirectory( hwndFrame );
         break;

      case IDM_SENDFILE:
         CmdBinmail( hwndFrame, NULL );
         break;

      case IDM_MAILRENAME:
         CmdMailRename( hwndFrame, "" );
         break;

      case IDM_MAILEXPORT:
         CmdMailExport( hwndFrame, "" );
         break;

      case IDM_MAILERASE:
         CmdMailErase( hwndFrame, "" );
         break;

      case IDM_MAILUPLOAD:
         CmdMailUpload( hwndFrame );
         break;

      case IDM_MAILDOWNLOAD:
         CmdMailDownload( hwndFrame, "" );
         break;

      case IDM_UPLOADFILE:
         CmdFileUpload( hwndFrame );
         break;

      case IDM_OWNRESUME:
         CmdEditResume( hwndFrame );
         break;

      case IDM_PARTICIPANTS: {
         CURMSG curmsg;

         Ameol2_GetCurrentTopic( &curmsg );
         CreateParlistWindow( hwndFrame, curmsg.pcl );
         break;
         }

      case IDM_FILELISTFIND:
         CmdFileListFind( hwndFrame, NULL );
         break;

      case IDM_FILELIST: {
         CURMSG curmsg;

         Ameol2_GetCurrentTopic( &curmsg );
         if( NULL != curmsg.ptl )
            if( Amdb_GetTopicType( curmsg.ptl ) == FTYPE_MAIL )
               CmdMailDirectory( hwndFrame );
            else
               CreateFilelistWindow( hwndFrame, curmsg.ptl );
         break;
         }

      case IDM_JOINCONFERENCE_POPUP:
      case IDM_JOINCONFERENCE:
         CmdJoinConference( hwndFrame, "" );
         break;

      case IDM_SHOWALLUSERS:
         ReadUsersList( hwndFrame );
         break;

      case IDM_SHOWCIXCONFS:
         ReadCIXList( hwndFrame );
         break;

      case IDM_RESUMES:
         ShowResumesListWindow();
         return( FALSE );

      }
   return( TRUE );
}

/* This callback function is called when the user tries to delete a
 * folder or topic from the in-basket.
 */
BOOL WINAPI EXPORT CIX_DeletingFolder( int wEvent, LPARAM lParam1, LPARAM lParam2 )
{
   CmdResign( (HWND)lParam1, (LPVOID)lParam2, FALSE, FALSE );
   return( TRUE );
}

/* This callback function is called when the user chooses the Properties
 * dialog. We add pages for the CIX Conferencing service.
 */
BOOL WINAPI EXPORT CIX_PropertiesDialog( int wEvent, LPARAM lParam1, LPARAM lParam2 )
{
   if( IsCIXFolder( (PCL)lParam2 ) )
      {
      AMPROPSHEETPAGE psp;

      psp.dwSize = sizeof( AMPROPSHEETPAGE );
      psp.dwFlags = PSP_USETITLE;
      psp.hInstance = hInst;
      psp.pszTemplate = MAKEINTRESOURCE( IDDLG_IBP_MODERATORS );
      psp.pszIcon = NULL;
      psp.pfnDlgProc = InBasket_CIX;
      psp.pszTitle = "Moderators";
      psp.lParam = lParam2;
      nCIXPropertiesPage = (int)PropSheet_AddPage( (HWND)lParam1, &psp );

      psp.dwSize = sizeof( AMPROPSHEETPAGE );
      psp.dwFlags = PSP_USETITLE;
      psp.hInstance = hInst;
      psp.pszTemplate = MAKEINTRESOURCE( IDDLG_IBP_ABOUT );
      psp.pszIcon = NULL;
      psp.pfnDlgProc = InBasket_CIX2;
      psp.pszTitle = "About";
      psp.lParam = lParam2;
      nCIXPropertiesPage2 = (int)PropSheet_AddPage( (HWND)lParam1, &psp );
      }
   return( TRUE );
}

/* This callback function is called when the user chooses the Properties
 * command in the Blink Manager dialog.
 */
BOOL WINAPI EXPORT CIX_BlinkPropertiesDialog( int wEvent, LPARAM lParam1, LPARAM lParam2 )
{
   AMPROPSHEETPAGE psp;

   psp.dwSize = sizeof( AMPROPSHEETPAGE );
   psp.dwFlags = PSP_USETITLE;
   psp.hInstance = hInst;
   psp.pszTemplate = MAKEINTRESOURCE(fUseLegacyCIX ? IDDLG_BLINK_CIX_WITH_MAIL : IDDLG_BLINK_CIX );
   psp.pszIcon = NULL;
   psp.pfnDlgProc = CIXBlinkProperties;
   psp.pszTitle = "CIX Forums";
   psp.lParam = lParam2;
   PropSheet_AddPage( (HWND)lParam1, &psp );
   return( TRUE );
}


/* This function handles the Open Password dialog box.
 */
BOOL EXPORT CALLBACK CIXSettings( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, CIXSettings_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the Incoming Mail page of the
 * Preferences dialog.
 */
LRESULT FASTCALL CIXSettings_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, CIXSettings_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, CIXSettings_OnCommand );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, FolderCombo_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_DRAWITEM, FolderCombo_OnDrawItem );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsCIXSETTINGS );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL CIXSettings_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   int nMailClearRate;
   HWND hwndCombo;
   HWND hwndSpin;
   HWND hwndEdit;
   SCHEDULE sch;
   HXFER pftp;
   int index;

   /* Set the CIX nickname
    */
   hwndEdit = GetDlgItem( hwnd, IDD_CIXNICKNAME );
   Edit_SetText( hwndEdit, szCIXNickname );
   Edit_LimitText( hwndEdit, sizeof(szCIXNickname) - 1 );

   /* Set the CIX password
    */
   hwndEdit = GetDlgItem( hwnd, IDD_CIXPASSWORD );
   Amuser_Decrypt( szCIXPassword, rgEncodeKey );
   Edit_SetText( hwndEdit, szCIXPassword );
   Edit_LimitText( hwndEdit, sizeof(szCIXPassword)-1 );
   Amuser_Encrypt( szCIXPassword, rgEncodeKey );

   /* Show the settings.
    */
   CheckDlgButton( hwnd, IDD_GETPARLIST, fGetParList );
   CheckDlgButton( hwnd, IDD_CONNECTLOG, fOpenTerminalLog );
   CheckDlgButton( hwnd, IDD_ARCSCRATCH, fArcScratch );
   CheckDlgButton( hwnd, IDD_CIXTERMINAL, fViewCixTerminal );

   /* Maximum message size.
    */
   SetDlgItemLongInt( hwnd, IDD_MAXMSGSIZE, dwMaxMsgSize );
   SendDlgItemMessage( hwnd, IDD_MAXMSGSIZE, EM_LIMITTEXT, 8, 0L );

   /* Get mail clear rate from scheduler.
    */
   if (fUseLegacyCIX)
   {
      nMailClearRate = 0;
      if( SCHDERR_OK == Ameol2_GetSchedulerInfo( "CixClearMail", &sch ) )
         {
         nMailClearRate = 255;
         if( sch.schType == SCHTYPE_DAYPERIOD )
            if( sch.schDate.iDay <= 7 )
               nMailClearRate = sch.schDate.iDay;
         if( nMailClearRate == 255 )
            ShowEnableControl( hwnd, IDD_SCHEDULER, TRUE );
         }

      /* Fill Clear list
       */
      Common_FillClearRateList( hwnd, IDD_CLRLIST, nMailClearRate );
      Common_FillClearCountList( hwnd, IDD_COUNTLIST, nCIXMailClearCount );
   }
   else
   {
      RECT rcDialog;

      GetWindowRect( hwnd, &rcDialog );
      rcDialog.bottom -= 80;  // Hacky guess.
       SetWindowPos( hwnd, NULL, 0, 0, rcDialog.right - rcDialog.left, rcDialog.bottom - rcDialog.top, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE ); 

      // Resize the dialog to hide the CIX mail options and disable them
      // so you can't tab to them.
      ShowEnableControl( hwnd, IDD_CLRLIST, FALSE);
      ShowEnableControl( hwnd, IDD_COUNTLIST, FALSE);
   }

   /* List file transfer protocols.
    */
   VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_PROTOCOL ) );
   EnumerateFileTransferProtocols( hwndCombo );
   if( NULL == ( pftp = GetFileTransferProtocolHandle( szCixProtocol ) ) )
      index = 0;
   else
      index = RealComboBox_FindItemData( hwndCombo, -1, (LPARAM)pftp );
   ComboBox_SetCurSel( hwndCombo, index );

   /* Recent setting.
    */
   SetDlgItemInt( hwnd, IDD_EDIT, iRecent, FALSE );
   SendDlgItemMessage( hwnd, IDD_EDIT, EM_LIMITTEXT, 4, 0L );
   hwndSpin = GetDlgItem( hwnd, IDD_SPIN );
   SetWindowStyle( hwndSpin, GetWindowStyle( hwndSpin ) | UDS_NOTHOUSANDS ); 
   SendMessage( hwndSpin, UDM_SETBUDDY, (WPARAM)GetDlgItem( hwnd, IDD_EDIT ), 0L );
   SendMessage( hwndSpin, UDM_SETRANGE, 0, MAKELPARAM( 9999, 1 ) );
   SendMessage( hwndSpin, UDM_SETPOS, 0, MAKELPARAM( iRecent, 0 ) );

   /* Set Scheduler button bitmap.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_SCHEDULER ), hInst, IDB_SCHEDULE );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL CIXSettings_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_SCHEDULER:
         CommonScheduleDialog( hwnd, "CixClearMail" );
         break;

      case IDD_CLRLIST:
         if( codeNotify == CBN_SELCHANGE )
            if( 255 == nClrRate[ ComboBox_GetCurSel( hwndCtl ) ] )
               ShowEnableControl( hwnd, IDD_SCHEDULER, TRUE );
            else
               ShowEnableControl( hwnd, IDD_SCHEDULER, FALSE );
         break;

      case IDOK: {
         int nCIXMailClearRate;
         HWND hwndEdit;
         HWND hwndList;

         /* Read and store the recent setting.
          */
         if( !GetDlgInt( hwnd, IDD_EDIT, &iRecent, 1, 9999 ) )
            break;

         /* Read and store the maximum message size
          */
         if( !GetDlgLongInt( hwnd, IDD_MAXMSGSIZE, &dwMaxMsgSize, 50, 99999999 ) )
            break;

         /* Read and store the mail address.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_CIXNICKNAME );
         if( Edit_GetTextLength( hwndEdit ) == 0 )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR797), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_CIXNICKNAME );
            break;
            }
         Edit_GetText( hwndEdit, szCIXNickname, sizeof(szCIXNickname) );
         if( !IsValidCIXNickname( szCIXNickname ) )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR904), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_CIXNICKNAME );
            break;
            }

         /* Read and store the full name.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_CIXPASSWORD );
         if( Edit_GetTextLength( hwndEdit ) == 0 )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR798), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_CIXPASSWORD );
            break;
            }
         Edit_GetText( hwndEdit, szCIXPassword, sizeof(szCIXPassword) );
         if( !IsValidCIXPassword( szCIXPassword ) )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR905), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_CIXPASSWORD );
            break;
            }

         /* Get the connection details.
          */
         fGetParList = IsDlgButtonChecked( hwnd, IDD_GETPARLIST );
         fOpenTerminalLog = IsDlgButtonChecked( hwnd, IDD_CONNECTLOG );
         fArcScratch = IsDlgButtonChecked( hwnd, IDD_ARCSCRATCH );
         fViewCixTerminal = IsDlgButtonChecked( hwnd, IDD_CIXTERMINAL );

         /* Get mail clear count.
          */
         if (fUseLegacyCIX)
         {
            VERIFY( hwndList = GetDlgItem( hwnd, IDD_COUNTLIST ) );
            nCIXMailClearCount = nClearCount[ ComboBox_GetCurSel( hwndList ) ];
            Amuser_WritePPInt( szCIX, "MailClearCount", nCIXMailClearCount );

            /* Get mail clear frequency.
             */
            VERIFY( hwndList = GetDlgItem( hwnd, IDD_CLRLIST ) );
            nCIXMailClearRate = nClrRate[ ComboBox_GetCurSel( hwndList ) ];

            /* Update (or add) the Clear CIX mail command with the
             * new frequency.
             */
            if( nCIXMailClearRate == 0 )
               Ameol2_SetSchedulerInfo( "CixClearMail", NULL );
            else if( nCIXMailClearRate != 255 )
               {
               SCHEDULE sch;

               Amdate_GetCurrentDate( &sch.schDate );
               Amdate_GetCurrentTime( &sch.schTime );
               sch.reserved = 0;
               sch.dwSize = sizeof(SCHEDULE);
               sch.schType = SCHTYPE_DAYPERIOD;
               sch.schDate.iDay = nCIXMailClearRate;
               Ameol2_SetSchedulerInfo( "CixClearMail", &sch );
               }
         }

         /* Get the file transfer protocol.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_PROTOCOL ) );
         ComboBox_GetText( hwndList, szCixProtocol, sizeof(szCixProtocol) );
         Amuser_WritePPString( szCIX, "Protocol", szCixProtocol );

         /* Save the CIX settings
          */
         Amuser_WritePPString( szCIX, "Nickname", szCIXNickname );
         Amuser_WritePPInt( szCIX, "CompressScratchpad", fArcScratch );
         Amuser_WritePPInt( szCIX, "GetParticipantsList", fGetParList );
         Amuser_WritePPInt( szCIX, "CreateConnectLog", fOpenTerminalLog );
         Amuser_WritePPLong( szCIX, "MaxMessageSize", dwMaxMsgSize );
         Amuser_WritePPInt( szCIX, "Recent", iRecent );
         Amuser_WritePPInt( szCIX, "CixTerminal", fViewCixTerminal );

         /* Encode the password in DES format before saving.
          */
         Amuser_Encrypt( szCIXPassword, rgEncodeKey );
         Amuser_WritePPPassword( szCIX, "Password", szCIXPassword );

         /* Exit
          */
         EndDialog( hwnd, TRUE );
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}

/* This function dispatches messages for the General page of the
 * In Basket properties dialog.
 */
BOOL EXPORT CALLBACK InBasket_CIX( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, InBasket_CIX_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, InBasket_CIX_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, InBasket_CIX_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL InBasket_CIX_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPCAMPROPSHEETPAGE psp;
   HWND hwndList;
   LPVOID pData;
   LPSTR lpModList;
   UINT wSize;

   INITIALISE_PTR(lpModList);

   /* Dereference and save the handle of the database, folder
    * or topic whose properties we're showing.
    */
   psp = (LPCAMPROPSHEETPAGE)lParam;
   pData = (LPVOID)psp->lParam;
   SetWindowLong( hwnd, DWL_USER, (LPARAM)pData );
   InBasket_Prop_ShowTitle( hwnd, pData );

   /* Fill out the page.
    */
   ASSERT( Amdb_IsFolderPtr( pData ) );
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   wSize = Amdb_GetModeratorList( (PCL)pData, NULL, 0xFFFF );
   if( wSize > 0 )
      if( fNewMemory( &lpModList, wSize ) )
         {
         /* Get the list again.
          */
         Amdb_GetModeratorList( (PCL)pData, lpModList, wSize );
         Edit_SetText( hwndList, lpModList );
         FreeMemory( &lpModList );
         }
   SetFocus( GetDlgItem( hwnd, IDD_UPDATE ) );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL InBasket_CIX_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_LIST:
         if( codeNotify == EN_CHANGE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
      case IDD_UPDATE: {
         BOOL fPrompt;
         PCL pcl;

         fPrompt = TRUE;
         pcl = (PCL)GetWindowLong( hwnd, DWL_USER );
         if( !UpdateParList( hwnd, pcl, &fPrompt, FALSE ) )
            break;
         break;
         }

      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL InBasket_CIX_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsIBP_MODERATORS );
         break;

      case PSN_SETACTIVE:
         nLastPropertiesPage = nCIXPropertiesPage;
         SetFocus( GetDlgItem( hwnd, IDD_UPDATE ) );
         break;

      case PSN_APPLY: {
         HWND hwndEdit;
         PCL pcl;

         /* Get the folder handle.
          */
         pcl = (PCL)GetWindowLong( hwnd, DWL_USER );

         /* Get the list of moderators and if it has been
          * changed, update it.
          */
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_LIST ) );
         if( EditMap_GetModified( hwndEdit ) )
            {
            LPSTR lpModList;
            UINT cbEdit;

            INITIALISE_PTR(lpModList);
            cbEdit = Edit_GetTextLength( hwndEdit ) + 1;
            if( fNewMemory( &lpModList, cbEdit ) )
               {
               Edit_GetText( hwndEdit, lpModList, cbEdit );
               Amdb_SetModeratorList( pcl, lpModList );
               FreeMemory( &lpModList );
               SendAllWindows( WM_APPCOLOURCHANGE, WIN_INBASK, 0L );
               }
            }

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This function dispatches messages for the General page of the
 * In Basket properties dialog.
 */
BOOL EXPORT CALLBACK InBasket_CIX2( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, InBasket_CIX2_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, InBasket_CIX2_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, InBasket_CIX2_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL InBasket_CIX2_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPCAMPROPSHEETPAGE psp;
   FOLDERINFO confinfo;
   char szFileName[ 13 ];
   LPVOID pData;
   HNDFILE fh;

   /* Dereference and save the handle of the database, folder
    * or topic whose properties we're showing.
    */
   psp = (LPCAMPROPSHEETPAGE)lParam;
   pData = (LPVOID)psp->lParam;
   SetWindowLong( hwnd, DWL_USER, (LPARAM)pData );
   InBasket_Prop_ShowTitle( hwnd, pData );

   ASSERT( NULL == hAboutFont );
   hAboutFont = CreateFont( 14, 0, 0, 0, 400, FALSE, FALSE,
                           FALSE, ANSI_CHARSET, OUT_CHARACTER_PRECIS,
                           CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
                           FIXED_PITCH | FF_MODERN, "Courier New" );
   
   /* Fill out the page.
    */
   ASSERT( Amdb_IsFolderPtr( pData ) );
   Amdb_GetFolderInfo( (PCL)pData, &confinfo );
   wsprintf( szFileName, "%s.CXN", (LPSTR)confinfo.szFileName );
   if( ( fh = Ameol2_OpenFile( szFileName, DSD_CURRENT, AOF_READ ) ) != HNDFILE_ERROR )
      {
      WORD wSize;
      LPSTR lpBuf;
      WORD uDate;
      WORD uTime;


      INITIALISE_PTR(lpBuf);

      wSize = (WORD)Amfile_GetFileSize( fh );
      if( fNewMemory( &lpBuf, wSize + 1 ) )
         {
         /* Read the note, make sure the text is in PC format
          * and skip the spurious date at the start.
          */
         Amfile_Read( fh, lpBuf, wSize );
         lpBuf[ wSize ] = '\0';
         lpBuf = ExpandUnixTextToPCFormat( lpBuf );
         Edit_SetText( GetDlgItem( hwnd, IDD_EDIT ), AdvanceLine( lpBuf ) );
         FreeMemory( &lpBuf );
         }
      Amfile_GetFileTime( fh, &uDate, &uTime );
      wsprintf( lpTmpBuf, GS(IDS_STR336), Amdate_FriendlyDate( NULL, uDate, uTime ) );
      SetDlgItemText( hwnd, IDD_LASTUPDATED, lpTmpBuf );
      Amfile_Close( fh );
      }
   else
      SetDlgItemText( hwnd, IDD_LASTUPDATED, GS(IDS_STR1236) );
   SetWindowFont( GetDlgItem( hwnd, IDD_EDIT ), hAboutFont, FALSE );
   return( TRUE );
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL InBasket_CIX2_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsIBP_ABOUT );
         break;

      case PSN_SETACTIVE:
         nLastPropertiesPage = nCIXPropertiesPage2;
         break;

      case PSN_APPLY:
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
      }
   return( FALSE );
}

void FASTCALL InBasket_CIX2_OnCommand( HWND hwnd, int id , HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_UPDATENOTE: {
   
         LPVOID pFolder;
      
         pFolder = (LPVOID)GetWindowLong( hwnd, DWL_USER );
         UpdateConfNotes( pFolder, TRUE );
         break;
         }
      }
}

/* This callback function is called when the user right-clicks on
 * a folder belonging to this service in the in-basket. We append
 * any commands specific to this service to the menu.
 */
BOOL WINAPI EXPORT CIX_Popup( int wEvent, LPARAM lParam1, LPARAM lParam2 )
{
   LPAEPOPUP lpaep;

   lpaep = (LPAEPOPUP)lParam1;
   switch( lpaep->wType )
      {
      case WIN_MESSAGE:
         /* Add two extra commands to every message window popup.
          */
         if( ( Amdb_GetTopicType( (PTL)lpaep->pFolder ) == FTYPE_CIX || Amdb_GetTopicType( (PTL)lpaep->pFolder ) == FTYPE_MAIL ) && lpaep->cchSelectedText > 0 )
            {
            InsertMenu( lpaep->hMenu, lpaep->nInsertPos++, MF_BYPOSITION, IDM_JOINCONFERENCE, "Join Conference" );
            if( Amdb_GetTopicType( (PTL)lpaep->pFolder ) == FTYPE_CIX )
               InsertMenu( lpaep->hMenu, lpaep->nInsertPos++, MF_BYPOSITION, IDM_DOWNLOAD, "Download File" );
            else if( Amdb_GetTopicType( (PTL)lpaep->pFolder ) == FTYPE_MAIL )
               InsertMenu( lpaep->hMenu, lpaep->nInsertPos++, MF_BYPOSITION, IDM_MAILDOWNLOAD, "Download Binmail" );
            }
         break;

      case WIN_INBASK: {
         BOOL fCIXFolder;

         fCIXFolder = FALSE;
      
         /* Decode the folder handle to see if it is one which
          * we can handle.
          */
         if( Amdb_IsTopicPtr( lpaep->pFolder ) )
            {
            if( Amdb_GetTopicType( (PTL)lpaep->pFolder ) == FTYPE_CIX )
               fCIXFolder = TRUE;
            }
         else if( Amdb_IsFolderPtr( lpaep->pFolder ) )
            {
            PTL ptl;
      
            for( ptl = Amdb_GetFirstTopic( (PCL)lpaep->pFolder ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
               if( Amdb_GetTopicType( ptl ) == FTYPE_CIX )
                  fCIXFolder = TRUE;
            }
         else if( Amdb_IsCategoryPtr( lpaep->pFolder ) )
            {
            PCL pcl;
            PTL ptl;
      
            for( pcl = Amdb_GetFirstFolder( (PCAT)lpaep->pFolder ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
               for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
                  if( Amdb_GetTopicType( ptl ) == FTYPE_CIX )
                     fCIXFolder = TRUE;
            }

         /* If this is a mail topic, then allow people to view the Mail Directory
          * via the popup menu.
          */
         if( Amdb_IsTopicPtr( (PTL)lpaep->pFolder ) )
            if( Amdb_GetTopicType( (PTL)lpaep->pFolder ) == FTYPE_MAIL )
               InsertMenu( lpaep->hMenu, lpaep->nInsertPos++, MF_BYPOSITION, IDM_MAILDIRECTORY, "Mail Directory" );

         /* Now insert the appropriate commands depending on
          * what we've got in the selected folder.
          */
         if( fCIXFolder )
            {
            if( Amdb_IsTopicPtr( (PTL)lpaep->pFolder ) )
               {
               TOPICINFO topicinfo;

               /* If it is a CIX topic, add a command to display the
                * File List window.
                */
               Amdb_GetTopicInfo( (PTL)lpaep->pFolder, &topicinfo );
               if( topicinfo.dwFlags & TF_HASFILES )
                  InsertMenu( lpaep->hMenu, lpaep->nInsertPos++, MF_BYPOSITION, IDM_FILELIST, "File List" );
               if( topicinfo.dwFlags & TF_RESIGNED )
                  InsertMenu( lpaep->hMenu, lpaep->nInsertPos++, MF_BYPOSITION, IDM_JOINCONFERENCE, "Rejoin Topic" );
               else
                  InsertMenu( lpaep->hMenu, lpaep->nInsertPos++, MF_BYPOSITION, IDM_RESIGN, "Resign Topic" );
               }
            else if( Amdb_IsFolderPtr( (PCL)lpaep->pFolder ) )
               {
               /* If it is a CIX folder, add Resign or Rejoin as
                * appropriate.
                */
               if( Amdb_IsResignedFolder( (PCL)lpaep->pFolder ) )
                  InsertMenu( lpaep->hMenu, lpaep->nInsertPos++, MF_BYPOSITION, IDM_JOINCONFERENCE, "Rejoin Forum" );
               else
                  InsertMenu( lpaep->hMenu, lpaep->nInsertPos++, MF_BYPOSITION, IDM_RESIGN, "Resign Forum" );
               }
            }
         break;
         }
      }
   return( TRUE );
}

/* This function checks whether or not the specified CIX password
 * is valid.
 */
BOOL FASTCALL IsValidCIXPassword( char * pszPassword )
{
   return( TRUE );
}

/* This function checks whether or not the specified CIX nickname
 * is valid.
 */
BOOL FASTCALL IsValidCIXNickname( char * pszNickname )
{
   register int c;

   for( c = 0; c < 14 && pszNickname[ c ]; ++c )
      if( !( isalpha( pszNickname[ c ] ) || ( strchr( "0123456789_", pszNickname[ c ] ) && c > 0 ) ) )
         break;
   return( pszNickname[ c ] == '\0' );
}

/* This function checks that the specified filename is valid on
 * CIX.
 */
BOOL FASTCALL CheckValidCixFilename( HWND hwnd, char * pszFilename )
{
   if( strlen( GetFileBasename( pszFilename ) ) > 14 )
      {
      fMessageBox( hwnd, 0, GS(IDS_STR454), MB_OK|MB_ICONEXCLAMATION );
      return( FALSE );
      }
   if( strpbrk( GetFileBasename( pszFilename ), szIllegalCixFilenameChrs ) != NULL )
      {
      fMessageBox( hwnd, 0, GS(IDS_STR454), MB_OK|MB_ICONEXCLAMATION );
      return( FALSE );
      }
   return( TRUE );
}

/* This function dispatches messages for the General page of the
 * In Basket properties dialog.
 */
BOOL EXPORT CALLBACK CIXBlinkProperties( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, CIXBlinkProperties_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, CIXBlinkProperties_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, CIXBlinkProperties_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL CIXBlinkProperties_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPCAMPROPSHEETPAGE psp;
   LPBLINKENTRY lpbe;

   /* Dereference and save the handle of the database, folder
    * or topic whose properties we're showing.
    */
   psp = (LPCAMPROPSHEETPAGE)lParam;
   lpbe = (LPVOID)psp->lParam;
   SetWindowLong( hwnd, DWL_USER, (LPARAM)lpbe );

   /* Set the various options.
    */
   CheckDlgButton( hwnd, IDD_DOWNLOADCIX, TestF(lpbe->dwBlinkFlags, BF_GETCIXMSGS) );
   CheckDlgButton( hwnd, IDD_DOWNLOADUSENET, TestF(lpbe->dwBlinkFlags, BF_GETCIXNEWS) );
   CheckDlgButton( hwnd, IDD_DOWNLOADTAGGED, TestF(lpbe->dwBlinkFlags, BF_GETCIXTAGGEDNEWS) );
   CheckDlgButton( hwnd, IDD_CIXCONF, TestF(lpbe->dwBlinkFlags, BF_POSTCIXMSGS) );
   CheckDlgButton( hwnd, IDD_USENET, TestF(lpbe->dwBlinkFlags, BF_POSTCIXNEWS) );
   CheckDlgButton( hwnd, IDD_RECOVER, TestF(lpbe->dwBlinkFlags, BF_CIXRECOVER) );
   CheckDlgButton( hwnd, IDD_STAYONLINE, TestF(lpbe->dwBlinkFlags, BF_STAYONLINE) );

   /* Only used if CIX Mail is enabled.
    */
   if (fUseLegacyCIX)
   {
      CheckDlgButton( hwnd, IDD_DOWNLOADMAIL, TestF(lpbe->dwBlinkFlags, BF_GETCIXMAIL) );
      CheckDlgButton( hwnd, IDD_MAIL, TestF(lpbe->dwBlinkFlags, BF_POSTCIXMAIL) );
   }

   /* Set picture button.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_EDITCONNCARD ), hInst, IDB_EDITCONNCARD );

   /* How do we connect to CIX?
    */
   FillConnectionCards( hwnd, IDD_LIST, lpbe->szConnCard );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL CIXBlinkProperties_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EDITCONNCARD: {
         COMMDESCRIPTOR cd;
         HWND hwndList;

         INITIALISE_PTR( cd.lpsc );
         INITIALISE_PTR( cd.lpic );

         /* Get the selected card name.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         ComboBox_GetText( hwndList, cd.szTitle, LEN_CONNCARD+1 );
         EditConnectionCard( GetParent( hwnd ), &cd );
         break;
         }

      case IDD_LIST:
         if( codeNotify == CBN_SELCHANGE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_DOWNLOADUSENET:
      case IDD_DOWNLOADTAGGED:
      case IDD_DOWNLOADCIX:
      case IDD_DOWNLOADMAIL:
      case IDD_USENET:
      case IDD_CIXCONF:
      case IDD_MAIL:
      case IDD_RECOVER:
      case IDD_STAYONLINE:
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
      }
}

/* This function enables or disables the RAS tab
 */
void FASTCALL EnableDisableRASTab( HWND hwnd )
{
   COMMDESCRIPTOR cd;
   HWND hwndList;

   INITIALISE_PTR( cd.lpsc );
   INITIALISE_PTR( cd.lpic );

   /* Get the type of connection choosen. If it is
    * serial, then disable any RAS.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   ComboBox_GetText( hwndList, cd.szTitle, LEN_CONNCARD+1 );
   if( LoadCommDescriptor( &cd ) )
      {
      HWND hwndTab;

      /* Enable or disable RAS tab.
       */
      hwndTab = PropSheet_GetTabControl( GetParent( hwnd ) );
      TabCtrl_EnableTab( hwndTab, idRasTab, !(cd.wProtocol & PROTOCOL_SERIAL) );
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL CIXBlinkProperties_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsBLINK_CIX );
         break;

      case PSN_APPLY: {
         LPBLINKENTRY lpbe;
         HWND hwndList;

         /* Update the blink entry structure.
          */
         lpbe = (LPBLINKENTRY)GetWindowLong( hwnd, DWL_USER );
         lpbe->dwBlinkFlags &= ~BF_CIXFLAGS;
         if( IsDlgButtonChecked( hwnd, IDD_DOWNLOADCIX ) )
            lpbe->dwBlinkFlags |= BF_GETCIXMSGS;
         if( IsDlgButtonChecked( hwnd, IDD_DOWNLOADUSENET ) )
            lpbe->dwBlinkFlags |= BF_GETCIXNEWS;
         if( IsDlgButtonChecked( hwnd, IDD_DOWNLOADTAGGED ) )
            lpbe->dwBlinkFlags |= BF_GETCIXTAGGEDNEWS;
         if( IsDlgButtonChecked( hwnd, IDD_CIXCONF ) )
            lpbe->dwBlinkFlags |= BF_POSTCIXMSGS;
         if( IsDlgButtonChecked( hwnd, IDD_USENET ) )
            lpbe->dwBlinkFlags |= BF_POSTCIXNEWS;
         if( IsDlgButtonChecked( hwnd, IDD_RECOVER ) )
            lpbe->dwBlinkFlags |= BF_CIXRECOVER;
         if( IsDlgButtonChecked( hwnd, IDD_STAYONLINE ) )
            lpbe->dwBlinkFlags |= BF_STAYONLINE;
         if (fUseLegacyCIX)
         {
            if( IsDlgButtonChecked( hwnd, IDD_DOWNLOADMAIL ) )
               lpbe->dwBlinkFlags |= BF_GETCIXMAIL;
            if( IsDlgButtonChecked( hwnd, IDD_MAIL ) )
               lpbe->dwBlinkFlags |= BF_POSTCIXMAIL;
         }

         /* Get the connection card.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         ComboBox_GetText( hwndList, lpbe->szConnCard, LEN_CONNCARD+1 );

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This is the Clear CIX Mail out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_ClearCIXMail( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   switch( uType )
      {
      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR622), nCIXMailClearCount );
         return( TRUE );

      case OBEVENT_PERSISTENCE:
      case OBEVENT_FIND:
      case OBEVENT_DELETE:
         return( TRUE );
      }
   return( 0L );
}

/* This is the Update Conference Note out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_UpdateNote( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   UPDATENOTEOBJECT FAR * lpucn;

   lpucn = (UPDATENOTEOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_EXEC:
            return( POF_HELDBYSCRIPT );

      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR1225), DRF(lpucn->recConfName) );
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( 0 );

      case OBEVENT_NEW:
         _fmemset( lpucn, 0, sizeof( UPDATENOTEOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         UPDATENOTEOBJECT ucn;
      
         INITIALISE_PTR(lpucn);
         Amob_LoadDataObject( fh, &ucn, sizeof( UPDATENOTEOBJECT ) );
         if( fNewMemory( &lpucn, sizeof( UPDATENOTEOBJECT ) ) )
            {
            ucn.recConfName.hpStr = NULL;
            *lpucn = ucn;
            }
         return( (LRESULT)lpucn );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpucn->recConfName );
         return( Amob_SaveDataObject( fh, lpucn, sizeof( UPDATENOTEOBJECT ) ) );

      case OBEVENT_COPY: {
         UPDATENOTEOBJECT FAR * lpcoNew;
      
         INITIALISE_PTR( lpcoNew );
         lpucn = (UPDATENOTEOBJECT FAR *)dwData;
         if( fNewMemory( &lpcoNew, sizeof( UPDATENOTEOBJECT ) ) )
            {
            INITIALISE_PTR( lpcoNew->recConfName.hpStr );
            Amob_CopyRefObject( &lpcoNew->recConfName, &lpucn->recConfName );
            }
         return( (LRESULT)lpcoNew );
         }

      case OBEVENT_FIND: {
         UPDATENOTEOBJECT FAR * lpucn1;
         UPDATENOTEOBJECT FAR * lpucn2;

         lpucn1 = (UPDATENOTEOBJECT FAR *)dwData;
         lpucn2 = (UPDATENOTEOBJECT FAR *)lpBuf;
         return( strcmp( DRF(lpucn1->recConfName), DRF(lpucn2->recConfName) ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpucn );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpucn->recConfName );
         return( TRUE );
      }
   return( 0L );
}

void FASTCALL UpdateConfNotes( LPVOID pFolder, BOOL fPrompt )
{
   /* If no folder specified, request one visually.
    */
   if( NULL == pFolder )
      {
      CURMSG unr;
      PICKER cp;

      Ameol2_GetCurrentTopic( &unr );
      cp.wType = CPP_CONFERENCES;
      cp.ptl = NULL;
      cp.pcl = unr.pcl;
      strcpy( cp.szCaption, "Update Forum Notes" );
      if( !Amdb_ConfPicker( hwndFrame, &cp ) )
         return;
      pFolder = (LPVOID)cp.pcl;
      }

   /* Now go deal with the folder.
    */
   if( Amdb_IsCategoryPtr( (PCAT)pFolder ) )
      UpdateDatabaseConfNotes( (PCAT)pFolder, &fPrompt );
   else
      UpdateFolderConfNotes( pFolder, &fPrompt );
}

/* This function places a command in the out-basket to update the
 * conf notes for the specified database.
 */
BOOL FASTCALL UpdateDatabaseConfNotes( PCAT pFolder, BOOL * pfPrompt )
{
   PCL pcl;

   for( pcl = Amdb_GetFirstFolder( (PCAT)pFolder ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
      if( !UpdateFolderConfNotes( pcl, pfPrompt ) )
         return( FALSE );
   return( TRUE );
}

/* This function places a command in the out-basket to update the
 * participants lists for the specified topic.
 */
BOOL FASTCALL UpdateFolderConfNotes( PCL pcl, BOOL * pfPrompt )
{
   /* If pcl is a topic, make it a folder. This is a
    * horrible hack and should be removed.
    */
   if( Amdb_IsTopicPtr( (PTL)pcl ) )
      pcl = Amdb_FolderFromTopic( (PTL)pcl );

   /* Make sure this is a CIX folder.
    */
   if( !IsCIXFolder( pcl ) )
      return( TRUE );

   /* Make sure topic has a file list.
    */
   return( UpdateConfNote( hwndFrame, pcl, pfPrompt, FALSE ) );
}

/* This function creates an out-basket object to update the
 * participant list for the selected folder.
 */
BOOL FASTCALL UpdateConfNote( HWND hwnd, PCL pcl, BOOL * pfPrompt, BOOL fAll )
{
   UPDATENOTEOBJECT ucn;

   Amob_New( OBTYPE_UPDATENOTE, &ucn );
   Amob_CreateRefString( &ucn.recConfName, (LPSTR)Amdb_GetFolderName( pcl ) );

   if( Amob_Find( OBTYPE_UPDATENOTE, &ucn ) )
      {
      if( *pfPrompt )
         {
         wsprintf( lpTmpBuf, GS(IDS_STR1226) );
         fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
         }
      }
   else
      {
      if( *pfPrompt )
         {
         int wButtonType;
         int retCode;

         wsprintf( lpTmpBuf, GS(IDS_STR1231), Amdb_GetFolderName( pcl ) );
         wButtonType = fAll ? MB_YESNOALLCANCEL|MB_ICONQUESTION : MB_YESNO|MB_ICONQUESTION;
         retCode = fMessageBox( hwnd, 0, lpTmpBuf, wButtonType );
         if( IDNO == retCode )
            {
            Amob_Delete( OBTYPE_UPDATENOTE, &ucn );
            return( TRUE );
            }
         if( IDCANCEL == retCode )
            {
            Amob_Delete( OBTYPE_UPDATENOTE, &ucn );
            return( FALSE );
            }
         if( IDALL == retCode )
            *pfPrompt = FALSE;
         }
         Amob_Commit( NULL, OBTYPE_UPDATENOTE, &ucn );
      }
      Amob_Delete( OBTYPE_UPDATENOTE, &ucn );
   return( TRUE );
}
