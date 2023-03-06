/* CIXIP.C - CIX Internet driver main program
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

#define  THIS_FILE   __FILE__

#include "main.h"
#include <ctype.h>
#include <string.h>
#include "resource.h"
#include "amlib.h"
#include "intcomms.h"
#include "cookie.h"
#include "shared.h"
#include "cixip.h"
#include "command.h"
#include "blinkman.h"
#include "toolbar.h"
#include "news.h"
#include "ameol2.h"

static int nCIXIPPrefsPage;         /* Index of internet preferences page */
static int nCIXIPPropertiesPage;    /* Index of internet properties page */
static int nSMTPPrefsPage;          /* Index of internet SMTP page */

extern char * pClrList[ 9 ];
extern int nClrRate[ 9 ];
extern char * pCountList[ 8 ];
extern int nClearCount[ 8 ];
extern BYTE rgEncodeKey[8];

void FASTCALL CommonScheduleDialog( HWND, char * );
void FASTCALL Common_FillClearRateList( HWND, int, int );
void FASTCALL Common_FillClearCountList( HWND, int, int );

BOOL WINAPI CIXIP_Popup( int, LPARAM, LPARAM );
BOOL WINAPI CIXIP_PrefsDialog( int, LPARAM, LPARAM );
BOOL WINAPI CIXIP_PropertiesDialog( int, LPARAM, LPARAM );
BOOL WINAPI CIXIP_Command( int, LPARAM, LPARAM );
BOOL WINAPI CIXIP_BlinkPropertiesDialog( int, LPARAM, LPARAM );

BOOL EXPORT CALLBACK Prefs_CixIP( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL Prefs_CixIP_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL Prefs_CixIP_OnNotify( HWND, int, LPNMHDR );
void FASTCALL Prefs_CixIP_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK Prefs_IPSMTP( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL Prefs_IPSMTP_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL Prefs_IPSMTP_OnNotify( HWND, int, LPNMHDR );
void FASTCALL Prefs_IPSMTP_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK InBasket_Server( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL InBasket_Server_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL InBasket_Server_OnNotify( HWND, int, LPNMHDR );
void FASTCALL InBasket_Server_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK InBasket_Server_Mail( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL InBasket_Server_Mail_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL InBasket_Server_Mail_OnNotify( HWND, int, LPNMHDR );
void FASTCALL InBasket_Server_Mail_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK CIXIPBlinkProperties( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL CIXIPBlinkProperties_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL CIXIPBlinkProperties_OnNotify( HWND, int, LPNMHDR );
void FASTCALL CIXIPBlinkProperties_OnCommand( HWND, int, HWND, UINT );

#define GETAECOMMANDID(l1,l2)    (LOWORD((l2)))

/* Some macros to save my fingers...
 */
#define  HK_ALT         (HOTKEYF_ALT)
#define  HK_SHIFT       (HOTKEYF_SHIFT)
#define  HK_CTRL        (HOTKEYF_CONTROL)
#define  HK_CTRLSHIFT   (HOTKEYF_SHIFT|HOTKEYF_CONTROL)
#define  MIR            MAKEINTRESOURCE

#pragma data_seg("CMDTABLE")
static MENUSELECTINFO FAR wIDs[] = {
   { "MailGetNew",               IDM_GETNEWMAIL,            MIR(IDS_GETNEWMAIL),       0,             MIR(IDS_TT_GETNEWMAIL),          TB_GETNEWMAIL,          0,                WIN_ALL,          CAT_MAIL_MENU,    NSCHF_CANSCHEDULE },
   { "MailReadMail",          IDM_READMAIL,           MIR(IDS_READMAIL),            IDS_X_NOMAIL,     MIR(IDS_TT_READMAIL),            TB_READMAIL,            MAKEKEY( 0, 'B' ),      WIN_THREAD|WIN_MESSAGE, CAT_MAIL_MENU,    NSCHF_CANSCHEDULE },
   { "MailCreate",               IDM_MAIL,               MIR(IDS_MAIL),             0,             MIR(IDS_TT_MAIL),             TB_MAIL,             MAKEKEY( HK_CTRL, 'M' ),WIN_ALL,          CAT_MAIL_MENU,    NSCHF_NONE },
   { "MailReply",             IDM_REPLY,              MIR(IDS_REPLY),               IDS_X_NOMAIL,     MIR(IDS_TT_REPLY),               TB_REPLY,               0,                WIN_THREAD|WIN_MESSAGE, CAT_MAIL_MENU,    NSCHF_NONE },
   { "MailReplyAll",          IDM_REPLYTOALL,            MIR(IDS_REPLYTOALL),       IDS_X_NOMAIL,     MIR(IDS_TT_REPLYTOALL),          TB_REPLYTOALL,          MAKEKEY( 0, 'A' ),      WIN_THREAD|WIN_MESSAGE, CAT_MAIL_MENU,    NSCHF_NONE },
   { "MailAddToAddressBook",     IDM_ADDTOADDRBOOK,         MIR(IDS_ADDTOADDRBOOK),       IDS_X_NOMSG,      MIR(IDS_TT_ADDTOADDRBOOK),       TB_ADDTOADDRBOOK,       MAKEKEY( HK_CTRL, 'A' ),WIN_THREAD|WIN_MESSAGE, CAT_MAIL_MENU,    NSCHF_NONE },
   { "MailAddressBook",       IDM_ADDRBOOK,           MIR(IDS_ADDRBOOK),            0,             MIR(IDS_TT_ADDRBOOK),            TB_ADDRBOOK,            0,                WIN_ALL,          CAT_MAIL_MENU,    NSCHF_NONE },
   { "MailRedirect",          IDM_SORTMAILFROM,       MIR(IDS_SORTMAILFROM),        IDS_X_NOMAIL,     MIR(IDS_TT_SORTMAILFROM),        TB_SORTMAILFROM,        MAKEKEY( 0, 'Q' ),      WIN_THREAD|WIN_MESSAGE, CAT_MAIL_MENU,    NSCHF_NONE },
   { "MailSortMailTo",           IDM_SORTMAILTO,            MIR(IDS_SORTMAILTO),       IDS_X_NOMAIL,     MIR(IDS_TT_SORTMAILTO),          TB_SORTMAILTO,          0,                WIN_THREAD|WIN_MESSAGE, CAT_MAIL_MENU,    NSCHF_NONE },
   { "FileOnline",               IDM_ONLINE,             MIR(IDS_ONLINE),           IDS_X_ONLINE,     MIR(IDS_TT_ONLINE),              TB_ONLINE,              0,                WIN_ALL,          CAT_FILE_MENU,    NSCHF_CANSCHEDULE },
   { "NewsReadNews",          IDM_READNEWS,           MIR(IDS_READNEWS),            0,             MIR(IDS_TT_READNEWS),            TB_READNEWS,            0,                WIN_ALL,          CAT_NEWS_MENU,    NSCHF_NONE },
   { "NewsSubscribe",            IDM_SUBSCRIBE,          MIR(IDS_SUBSCRIBE),           0,             MIR(IDS_TT_SUBSCRIBE),           TB_SUBSCRIBE,           0,                WIN_ALL,          CAT_NEWS_MENU,    NSCHF_NONE },
   { "NewsUnsubscribe",       IDM_UNSUBSCRIBE,        MIR(IDS_UNSUBSCRIBE),         IDS_X_NONEWSGRP,  MIR(IDS_TT_UNSUBSCRIBE),         TB_UNSUBSCRIBE,            0,                WIN_ALL,          CAT_NEWS_MENU,    NSCHF_NONE },
   { "NewsGetArticles",       IDM_GETARTICLES,        MIR(IDS_GETARTICLES),         0,             MIR(IDS_TT_GETARTICLES),         TB_GETARTICLES,            0,                WIN_ALL,          CAT_NEWS_MENU,    NSCHF_CANSCHEDULE },
   { "NewsGetTagged",            IDM_GETTAGGED,          MIR(IDS_GETTAGGED),           0,             MIR(IDS_TT_GETTAGGED),           TB_GETTAGGED,           0,                WIN_ALL,          CAT_NEWS_MENU,    NSCHF_CANSCHEDULE },
   { "NewsShowAllGroups",        IDM_SHOWALLGROUPS,         MIR(IDS_SHOWALLGROUPS),       0,             MIR(IDS_TT_SHOWALLGROUPS),       TB_SHOWALLGROUPS,       0,                WIN_ALL,          CAT_NEWS_MENU,    NSCHF_NONE },
   { "NewsPostArticle",       IDM_POSTARTICLE,        MIR(IDS_POSTARTICLE),         IDS_X_NONEWSGRP,  MIR(IDS_TT_POSTARTICLE),         TB_POSTARTICLE,            0,                WIN_ALL,          CAT_NEWS_MENU,    NSCHF_NONE },
   { "NewsFollowupArticle",      IDM_FOLLOWUPARTICLE,    MIR(IDS_FOLLOWUPARTICLE),     IDS_X_NONEWS,     MIR(IDS_TT_FOLLOWUPARTICLE),     TB_FOLLOWUPARTICLE,        0,                WIN_THREAD|WIN_MESSAGE, CAT_NEWS_MENU,    NSCHF_NONE },
   { "NewsFollowupArticleToAll", IDM_FOLLOWUPARTICLETOALL,  MIR(IDS_FOLLOWUPARTICLETOALL),   IDS_X_NONEWS,     MIR(IDS_TT_FOLLOWUPARTICLETOALL),   TB_FOLLOWUPARTICLETOALL,   MAKEKEY( 0, 'X' ),      WIN_THREAD|WIN_MESSAGE, CAT_NEWS_MENU,    NSCHF_NONE },
   { "IPClearMail",           IDM_CLEARIPMAIL,        MIR(IDS_CLEARIPMAIL),         0,             MIR(IDS_TT_CLEARIPMAIL),         TB_CLEARIPMAIL,            0,                WIN_ALL,          CAT_MAIL_MENU,    NSCHF_CANSCHEDULE },
   { "UsenetSettings",           IDM_USENET,             MIR(IDS_USENET),           0,             MIR(IDS_TT_USENET),              TB_USENET,              0,                WIN_ALL,             CAT_SETTINGS_MENU,   NSCHF_NONE },
   { "ViewHeaders",           IDM_HEADERS,            MIR(IDS_HEADERS),          0,             MIR(IDS_TT_HEADERS),          TB_HEADERS,             0,                WIN_THREAD|WIN_MESSAGE, CAT_VIEW_MENU,    NSCHF_NONE },
   { "FolderNewMailbox",         IDM_NEWMAILFOLDER,         MIR(IDS_NEWMAILFOLDER),       IDS_X_NOMSG,      MIR(IDS_TT_NEWMAILFOLDER),       TB_NEWMAILFOLDER,       0,                WIN_THREAD|WIN_MESSAGE, CAT_TOPIC_MENU,      NSCHF_NONE },
   { "Decode",                IDM_DECODE,             MIR(IDS_DECODE),           IDS_X_NOFULLMSG,  MIR(IDS_TT_DECODE),              TB_DECODE,              MAKEKEY( 0, 'U' ),      WIN_THREAD|WIN_MESSAGE, CAT_MESSAGE_MENU, NSCHF_NONE },
   { "RunAttachment",            IDM_RUNATTACHMENT,         MIR(IDS_RUNATTACHMENT),       IDS_X_NOATTACH,      MIR(IDS_TT_RUNATTACHMENT),       TB_RUNATTACHMENT,       0,                WIN_THREAD|WIN_MESSAGE, CAT_MESSAGE_MENU, NSCHF_NONE },
   { "DeleteAttachment",         IDM_DELETEATTACHMENT,      MIR(IDS_DELETEATTACHMENT),    IDS_X_NOATTACH,      MIR(IDS_TT_DELETEATTACHMENT),    TB_DELETEATTACHMENT,    0,                WIN_THREAD|WIN_MESSAGE, CAT_MESSAGE_MENU, NSCHF_NONE },
   { NULL,                    (UINT)-1,               MIR(-1),                (UINT)-1,         MIR(-1),                   0,                   0,                0 }
   };
#pragma data_seg()

/* This function registers all out-basket object types.
 */
BOOL FASTCALL LoadCIXIP( void )
{
   register int c;

   /* First, register some events.
    */
   Amuser_RegisterEvent( AE_POPUP, CIXIP_Popup );
   Amuser_RegisterEvent( AE_PREFSDIALOG, CIXIP_PrefsDialog );
   Amuser_RegisterEvent( AE_PROPERTIESDIALOG, CIXIP_PropertiesDialog );
   Amuser_RegisterEvent( AE_BLINKPROPERTIESDIALOG, CIXIP_BlinkPropertiesDialog );
   Amuser_RegisterEvent( AE_COMMAND, CIXIP_Command );

   /* Add a category for IP commands.
    */
   if (!fUseLegacyCIX)
      CTree_AddCategory( CAT_MAIL_MENU, 2, "Mail" );
   CTree_AddCategory( CAT_NEWS_MENU, 3, "News" );

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

   /* Register some private OB classes.
    */
   if( !Amob_RegisterObjectClass( OBTYPE_CLEARIPMAIL, BF_POSTIPMAIL, ObProc_ClearIPMail ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_GETNEWMAIL, BF_GETIPMAIL, ObProc_GetNewMail ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_USENETRESIGN, BF_GETCIXNEWS, ObProc_UsenetResign ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_USENETJOIN, BF_GETCIXNEWS, ObProc_UsenetJoin ) )
      return( FALSE );
   return( TRUE );
}

/* This is the Clear IP Mail out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_ClearIPMail( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   switch( uType )
      {
      case OBEVENT_EXEC:
         return( Exec_ClearIPMail( dwData ) );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR1020), nIPMailClearCount );
         return( TRUE );

      case OBEVENT_PERSISTENCE:
      case OBEVENT_FIND:
      case OBEVENT_DELETE:
         return( TRUE );
      }
   return( 0L );
}

/* Initialise the CIX Internet service interface.
 */
BOOL FASTCALL InitialiseCIXIPInterface( HWND hwnd )
{
   HMENU hMenu;
   HMENU hNewMenu;
   HMENU hViewMenu;
   HMENU hFileMenu;
   HMENU hFolderMenu;
   HMENU hMessageMenu;
   HMENU hSettingsMenu;

   /* Insert the News and Mail menus.
    */
   InsertMailMenu();
   hMenu = LoadMenu( hRscLib, MAKEINTRESOURCE(IDMNU_USENET) );
   hNewMenu = GetSubMenu( hMenu, 1 );
   InsertMenu( hMainMenu, 4, MF_BYPOSITION|MF_POPUP, (UINT)hNewMenu, "&News" );

   /* Add View Mail/Usenet headers
    */
   hViewMenu = GetSubMenu( hMainMenu, 5 );
   InsertMenu( hViewMenu, 2, MF_BYPOSITION|MF_STRING, IDM_HEADERS, "Mail/Usenet &Headers" );

   /* Add Decode Attachments
    */
   hMessageMenu = GetSubMenu( hMainMenu, 7 );
   InsertMenu( hMessageMenu, -1, MF_BYPOSITION|MF_SEPARATOR, 0, NULL);
   InsertMenu( hMessageMenu, -1, MF_BYPOSITION|MF_STRING, IDM_DECODE, "Decode &Binary Attachment" );
   InsertMenu( hMessageMenu, -1, MF_BYPOSITION|MF_STRING, IDM_RUNATTACHMENT, "Open Selected &Attachment" );

   /* Add New Mailbox command
    */
   hFolderMenu = GetSubMenu( hMainMenu, 6 );
   InsertMenu( hFolderMenu, 3, MF_BYPOSITION|MF_STRING, IDM_NEWMAILFOLDER, "Ne&w Mailbox..." );

   /* Add Settings to the Settings menu.
    */
   hSettingsMenu = GetSubMenu( hMainMenu, 8 );
   if (!fUseLegacyCIX)
      InsertMenu( hSettingsMenu, 5, MF_BYPOSITION|MF_STRING, IDM_MAILSETTINGS, "&Mail..." );
   InsertMenu( hSettingsMenu, 5, MF_BYPOSITION|MF_STRING, IDM_USENET, "Use&net..." );

   /* Add Online command
    */
   hFileMenu = GetSubMenu( hMainMenu, 0 );
   InsertMenu( hFileMenu, 15, MF_BYPOSITION|MF_STRING, IDM_ONLINE, "&Online" );
   return( TRUE );
}

void FAR PASCAL InsertMailMenu(void)
{
   static BOOL fMailMenuInserted = FALSE;
   if (!fMailMenuInserted)
   {
      HMENU hMenu = LoadMenu( hRscLib, MAKEINTRESOURCE(IDMNU_USENET) );
      HMENU hNewMenu = GetSubMenu( hMenu, 0 );
      if (!fUseInternet)
      {
         DeleteMenu( hNewMenu, IDM_GETNEWMAIL, MF_BYCOMMAND );
         DeleteMenu( hNewMenu, IDM_CLEARIPMAIL, MF_BYCOMMAND );
      }
      InsertMenu( hMainMenu, 2, MF_BYPOSITION|MF_POPUP, (UINT)hNewMenu, "&Mail" );
      fMailMenuInserted = TRUE;
   }
}

/* This callback is called when the service is unloaded.
 */
void FAR PASCAL UnloadCIXIP( int nWhyAmIBeingKilled )
{
   Amuser_UnregisterEvent( AE_POPUP, CIXIP_Popup );
   Amuser_UnregisterEvent( AE_PREFSDIALOG, CIXIP_PrefsDialog );
   Amuser_UnregisterEvent( AE_PROPERTIESDIALOG, CIXIP_PropertiesDialog );
   Amuser_UnregisterEvent( AE_BLINKPROPERTIESDIALOG, CIXIP_BlinkPropertiesDialog );
   Amuser_UnregisterEvent( AE_COMMAND, CIXIP_Command );
}

/* This function returns the parameter for the specified
 * service.
 */
int FASTCALL GetCIXIPSystemParameter( char * pszKey, char * pszBuf, int cbMax )
{
   if( lstrcmpi( pszKey, "ReplyAddress" ) == 0 )
      {
      lstrcpyn( pszBuf, szMailReplyAddress, cbMax );
      return( lstrlen( pszBuf ) );
      }
   return( 0 );
}

/* This callback function is called when the user right-clicks on
 * a folder belonging to this service in the in-basket. We append
 * any commands specific to this service to the menu.
 */
BOOL WINAPI EXPORT CIXIP_Popup( int wEvent, LPARAM lParam1, LPARAM lParam2 )
{
   LPAEPOPUP lpaep;

   /* Get the menu handle and the position at
    * which we're going to insert.
    */
   lpaep = (LPAEPOPUP)lParam1;
   switch( lpaep->wType )
      {
      case WIN_INBASK: {
         BOOL fMailFolder;
         BOOL fNewsFolder;

         /* Decode the folder handle to see if it is one which
          * we can handle.
          */
         GetSupersetTopicTypes( lpaep->pFolder, &fMailFolder, &fNewsFolder );
         if( fMailFolder )
            InsertMenu( lpaep->hMenu, lpaep->nInsertPos++, MF_BYPOSITION, IDM_GETNEWMAIL, "Get New Mail" );
         break;
         }
      }
   return( TRUE );
}

/* This callback function is called when the user chooses a
 * menu command or presses a keystroke corresponding to a menu
 * command.
 */
BOOL WINAPI EXPORT CIXIP_Command( int wEvent, LPARAM lParam1, LPARAM lParam2 )
{
   switch( GETAECOMMANDID(lParam1, lParam2) )
      {
      case IDM_CLEARIPMAIL:
         Amob_New( OBTYPE_CLEARIPMAIL, NULL );
         if( !Amob_Find( OBTYPE_CLEARIPMAIL, NULL ) )
            Amob_Commit( NULL, OBTYPE_CLEARIPMAIL, NULL );
         Amob_Delete( OBTYPE_CLEARIPMAIL, NULL );
         break;
      }
   return( TRUE );
}

/* This callback function is called when the user chooses the Properties
 * dialog. We add pages for the News server service.
 */
BOOL WINAPI EXPORT CIXIP_PropertiesDialog( int wEvent, LPARAM lParam1, LPARAM lParam2 )
{
   if( Amdb_IsTopicPtr( (PTL)lParam2 ) )
      if( Amdb_GetTopicType( (PTL)lParam2 ) == FTYPE_NEWS )
         {
         AMPROPSHEETPAGE psp;

         psp.dwSize = sizeof( AMPROPSHEETPAGE );
         psp.dwFlags = PSP_USETITLE;
         psp.hInstance = hInst;
         psp.pszTemplate = MIR( IDDLG_IBP_SERVER );
         psp.pszIcon = NULL;
         psp.pfnDlgProc = InBasket_Server;
         psp.pszTitle = "Usenet";
         psp.lParam = lParam2;
         nCIXIPPropertiesPage = (int)PropSheet_AddPage( (HWND)lParam1, &psp );
         }
      else if( fUseInternet && Amdb_GetTopicType( (PTL)lParam2 ) == FTYPE_MAIL )
         {
         AMPROPSHEETPAGE psp;

         psp.dwSize = sizeof( AMPROPSHEETPAGE );
         psp.dwFlags = PSP_USETITLE;
         psp.hInstance = hInst;
         psp.pszTemplate = MIR( IDDLG_IBP_SERVER_MAIL );
         psp.pszIcon = NULL;
         psp.pfnDlgProc = InBasket_Server_Mail;
         psp.pszTitle = "Mail";
         psp.lParam = lParam2;
         nCIXIPPropertiesPage = (int)PropSheet_AddPage( (HWND)lParam1, &psp );
         }

   return( TRUE );
}

/* This callback function is called when the user chooses the Properties
 * command in the Blink Manager dialog.
 */
BOOL WINAPI EXPORT CIXIP_BlinkPropertiesDialog( int wEvent, LPARAM lParam1, LPARAM lParam2 )
{
   AMPROPSHEETPAGE psp;

   psp.dwSize = sizeof( AMPROPSHEETPAGE );
   psp.dwFlags = PSP_USETITLE;
   psp.hInstance = hInst;
   psp.pszTemplate = MIR( IDDLG_BLINK_IP );
   psp.pszIcon = NULL;
   psp.pfnDlgProc = CIXIPBlinkProperties;
   psp.pszTitle = "Internet";
   psp.lParam = lParam2;
   PropSheet_AddPage( (HWND)lParam1, &psp );
   return( TRUE );
}

/* This callback function is called when the user chooses the Preferences
 * dialog. We add pages for the internet service.
 */
BOOL WINAPI EXPORT CIXIP_PrefsDialog( int wEvent, LPARAM lParam1, LPARAM lParam2 )
{
   AMPROPSHEETPAGE psp;

   psp.dwSize = sizeof( AMPROPSHEETPAGE );
   psp.dwFlags = PSP_USETITLE;
   psp.hInstance = hInst;
   psp.pszTemplate = MIR(IDDLG_PREF_INTERNET);
   psp.pszIcon = NULL;
   psp.pfnDlgProc = Prefs_CixIP;
   psp.pszTitle = "POP3";
   psp.lParam = 0L;
   nCIXIPPrefsPage = (int)PropSheet_AddPage( (HWND)lParam1, &psp );

   psp.dwSize = sizeof( AMPROPSHEETPAGE );
   psp.dwFlags = PSP_USETITLE;
   psp.hInstance = hInst;
   psp.pszTemplate = MIR(IDDLG_PREF_SMTP);
   psp.pszIcon = NULL;
   psp.pfnDlgProc = Prefs_IPSMTP;
   psp.pszTitle = "SMTP";
   psp.lParam = lParam2;
   nSMTPPrefsPage = (int)PropSheet_AddPage( (HWND)lParam1, &psp );
   return( TRUE );
}

/* This function dispatches messages for the Incoming Mail page of the
 * Preferences dialog.
 */
BOOL EXPORT CALLBACK Prefs_CixIP( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, Prefs_CixIP_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, Prefs_CixIP_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, Prefs_CixIP_OnNotify );
      HANDLE_DLGMSG( hwnd, WM_MEASUREITEM, FolderCombo_OnMeasureItem );
      HANDLE_DLGMSG( hwnd, WM_DRAWITEM, FolderCombo_OnDrawItem );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL Prefs_CixIP_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   int nMailClearRate;
   HWND hwndEdit;
   SCHEDULE sch;
   char sz[ 10 ];

   /* Set the current POP3 server name.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_MAILSERVER );
   Edit_SetText( hwndEdit, szMailServer );
   Edit_LimitText( hwndEdit, sizeof(szMailServer) - 1 );

   /* Set the current POP3 port value.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_MAILPORT );
   wsprintf( lpTmpBuf, "%d", Amuser_GetPPLong( szSettings, "POP3Port", 110 ) );
   Edit_SetText( hwndEdit, lpTmpBuf );
   Edit_LimitText( hwndEdit, 5 );

   /* Set the mail login name.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_LOGIN );
   Edit_SetText( hwndEdit, szMailLogin );
   Edit_LimitText( hwndEdit, sizeof(szMailLogin) - 1 );

   /* Set the mail login password.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_PASSWORD );
   if( fRememberPassword )
      {
      Amuser_Decrypt( szMailPassword, rgEncodeKey );
      Edit_SetText( hwndEdit, szMailPassword );
      Edit_LimitText( hwndEdit, sizeof(szMailPassword) - 1 );
      Amuser_Encrypt( szMailPassword, rgEncodeKey );
      CheckDlgButton( hwnd, IDD_SAVEPASSWORD, TRUE );
      }
   else
      {
      EnableControl( hwnd, IDD_PAD1, FALSE );
      EnableWindow( hwndEdit, FALSE );
      CheckDlgButton( hwnd, IDD_SAVEPASSWORD, FALSE );
      }

   /* Get mail clear rate from scheduler.
    */
   nMailClearRate = 0;
   if( SCHDERR_OK == Ameol2_GetSchedulerInfo( "IPClearMail", &sch ) )
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

   /* Show current clear count
    */
   hwndEdit = GetDlgItem( hwnd, IDD_COUNTLIST );
   Edit_LimitText( hwndEdit, 2 );
   wsprintf( sz, "%d", nIPMailClearCount );
   Edit_SetText( hwndEdit, sz );

   /* Set options.
    */
   CheckDlgButton( hwnd, IDD_DELETEAFTER, fDeleteMailAfterReading );
   CheckDlgButton( hwnd, IDD_POP3LAST, fPOP3Last );

   if( !fUseCIX )
      ShowEnableControl( hwnd, IDD_POP3LAST, FALSE );

   /* Set Scheduler button bitmap.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_SCHEDULER ), hInst, IDB_SCHEDULE );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL Prefs_CixIP_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_SCHEDULER:
         CommonScheduleDialog( hwnd, "IPClearMail" );
         break;

      case IDD_CLRLIST:
         if( codeNotify == CBN_SELCHANGE )
            {
            if( 255 == nClrRate[ ComboBox_GetCurSel( hwndCtl ) ] )
               ShowEnableControl( hwnd, IDD_SCHEDULER, TRUE );
            else
               ShowEnableControl( hwnd, IDD_SCHEDULER, FALSE );
            PropSheet_Changed( GetParent( hwnd ), hwnd );
            }
         break;

      case IDD_COUNTLIST:
         if( codeNotify == EN_CHANGE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_LOGIN:
      case IDD_PASSWORD:
      case IDD_MAILSERVER:
      case IDD_MAILPORT:
         if( codeNotify == EN_UPDATE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_SAVEPASSWORD: {
         BOOL fChecked;

         fChecked = IsDlgButtonChecked( hwnd, id );
         EnableControl( hwnd, IDD_PAD1, fChecked );
         EnableControl( hwnd, IDD_PASSWORD, fChecked );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
         }

      case IDD_POP3LAST:
      case IDD_DELETEAFTER:
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL Prefs_CixIP_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPREF_INTERNET );
         break;

      case PSN_SETACTIVE:
         nLastMailDialogPage = nCIXIPPrefsPage;
         break;

      case PSN_APPLY: {
         int nCIXMailClearRate;
         char sz[ 256 ];
         HWND hwndList;
         HWND hwndEdit;
         int nPort;

         /* Read and store the POP3 mail server name.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_MAILSERVER );
         if( Edit_GetTextLength( hwndEdit ) == 0 )
            {
            PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, nCIXIPPrefsPage );
            fMessageBox( hwnd, 0, GS(IDS_STR779), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_MAILSERVER );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }
         Edit_GetText( hwndEdit, sz, sizeof(szMailServer) );
         if( lstrcmpi( sz, szMailServer ) != 0 )
            {
            strcpy( szMailServer, sz );
            }

         /* Save the port numbers
          */
         hwndEdit = GetDlgItem( hwnd, IDD_MAILPORT );
         Edit_GetText( hwndEdit, sz, sizeof(sz) );
         nPort = atoi( sz );
         if (nPort < 1 || nPort > 65535)
         {
            PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, nCIXIPPrefsPage );
            wsprintf( lpTmpBuf, GS(IDS_STR1263), sz );
            fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_MAILPORT );
            return( PSNRET_INVALID_NOCHANGEPAGE );
         }
         Amuser_WritePPLong( szSettings, "POP3Port", nPort );

         /* Read and store the mail login name.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_LOGIN );
         if( Edit_GetTextLength( hwndEdit ) == 0 )
            {
            PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, nCIXIPPrefsPage );
            fMessageBox( hwnd, 0, GS(IDS_STR780), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_LOGIN );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }
         Edit_GetText( hwndEdit, szMailLogin, sizeof(szMailLogin) );

         /* Read and store the mail password.
          */
         fRememberPassword = IsDlgButtonChecked( hwnd, IDD_SAVEPASSWORD );
         if( fRememberPassword )
            {
            hwndEdit = GetDlgItem( hwnd, IDD_PASSWORD );
            if( Edit_GetTextLength( hwndEdit ) == 0 )
               {
               PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, nCIXIPPrefsPage );
               fMessageBox( hwnd, 0, GS(IDS_STR781), MB_OK|MB_ICONEXCLAMATION );
               HighlightField( hwnd, IDD_PASSWORD );
               return( PSNRET_INVALID_NOCHANGEPAGE );
               }
            Edit_GetText( hwndEdit, szMailPassword, sizeof(szMailPassword) );
            Amuser_Encrypt( szMailPassword, rgEncodeKey );
            Amuser_WritePPPassword( szCIXIP, "MailPassword", szMailPassword );
            Amuser_WritePPInt( szCIXIP, "RememberPassword", TRUE );
            }
         else
            {
            *szMailPassword = '\0';
            Edit_SetText( GetDlgItem( hwnd, IDD_PASSWORD ), "" );
            Amuser_WritePPPassword( szCIXIP, "MailPassword", NULL );
            Amuser_WritePPInt( szCIXIP, "RememberPassword", FALSE );
            }

         /* Get mail clear count.
          */
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_COUNTLIST ) );
         Edit_GetText( hwndEdit, sz, sizeof(sz) );
         nIPMailClearCount = atoi( sz );
         if (nIPMailClearCount < 1 || nIPMailClearCount > 90)
         {
            PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, nCIXIPPrefsPage );
            wsprintf( lpTmpBuf, GS(IDS_STR1265), sz );
            fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_COUNTLIST );
            return( PSNRET_INVALID_NOCHANGEPAGE );
         }
         Amuser_WritePPInt( szCIXIP, "MailClearCount", nIPMailClearCount );

         /* Get mail clear frequency.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_CLRLIST ) );
         nCIXMailClearRate = nClrRate[ ComboBox_GetCurSel( hwndList ) ];

         /* Update (or add) the Clear CIX mail command with the
          * new frequency.
          */
         if( nCIXMailClearRate == 0 )
            Ameol2_SetSchedulerInfo( "IPClearMail", NULL );
         else if( nCIXMailClearRate != 255 )
            {
            SCHEDULE sch;

            Amdate_GetCurrentDate( &sch.schDate );
            Amdate_GetCurrentTime( &sch.schTime );
            sch.reserved = 0;
            sch.dwSize = sizeof(SCHEDULE);
            sch.schType = SCHTYPE_DAYPERIOD;
            sch.schDate.iDay = nCIXMailClearRate;
            Ameol2_SetSchedulerInfo( "IPClearMail", &sch );
            }

         /* Get options.
          */
         fDeleteMailAfterReading = IsDlgButtonChecked( hwnd, IDD_DELETEAFTER );
         fPOP3Last = IsDlgButtonChecked( hwnd, IDD_POP3LAST );

         /* Save the mail settings
          */
         Amuser_WritePPString( szCIXIP, "MailServer", szMailServer );
         Amuser_WritePPString( szCIXIP, "MailLogin", szMailLogin );
         Amuser_WritePPInt( szSettings, "DeleteMailAfterReading", fDeleteMailAfterReading );
         Amuser_WritePPInt( szSettings, "POP3Last", fPOP3Last );

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This function dispatches messages for the SMTP page of the
 * Preferences dialog.
 */
BOOL EXPORT CALLBACK Prefs_IPSMTP( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, Prefs_IPSMTP_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, Prefs_IPSMTP_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, Prefs_IPSMTP_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL Prefs_IPSMTP_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   BOOL fSMTPAuthInfo;
   HWND hwndEdit;

   /* Set the current SMTP server name.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_SMTPSERVER );
   Edit_SetText( hwndEdit, szSMTPMailServer );
   Edit_LimitText( hwndEdit, sizeof(szSMTPMailServer) - 1 );

   /* Set the current SMTP server name.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_SMTPPORT );
   wsprintf( lpTmpBuf, "%d", Amuser_GetPPLong( szSettings, "SMTPPort", IPPORT_SMTP ) );
   Edit_SetText( hwndEdit, lpTmpBuf );
   Edit_LimitText( hwndEdit, 5 );

   fSMTPAuthInfo = szSMTPMailLogin != '\0';
   if( fSMTPAuthInfo )
      {
      Edit_SetText( GetDlgItem( hwnd, IDD_AUTHINFOUSER ), szSMTPMailLogin );
      Amuser_Decrypt( szSMTPMailPassword, rgEncodeKey );
      Edit_SetText( GetDlgItem( hwnd, IDD_AUTHINFOPASS ), szSMTPMailPassword );
      Amuser_Encrypt( szSMTPMailPassword, rgEncodeKey );
      }
   Edit_LimitText( GetDlgItem( hwnd, IDD_AUTHINFOUSER ), sizeof(szSMTPMailLogin) - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_AUTHINFOPASS ), sizeof(szSMTPMailPassword) - 1 );

   /* Set the authinfo stuff.
    */
   CheckDlgButton( hwnd, IDD_SMTPAUTH, fSMTPAuthInfo );
   EnableControl( hwnd, IDD_AUTHINFOUSER, fSMTPAuthInfo );
   EnableControl( hwnd, IDD_AUTHINFOPASS, fSMTPAuthInfo );
   EnableControl( hwnd, IDD_PAD2, fSMTPAuthInfo );
   EnableControl( hwnd, IDD_PAD4, fSMTPAuthInfo );


   /* Set the mail login address.
    */
   VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_EMAILADDRESS ) );
   Edit_SetText( hwndEdit, szMailAddress );
   Edit_LimitText( hwndEdit, sizeof(szMailAddress) - 1 );
   EnableWindow( hwndEdit, fUseInternet );

   /* Set the mail full name.
    */
   VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_FULLNAME ) );
   Edit_SetText( hwndEdit, szFullName );
   Edit_LimitText( hwndEdit, sizeof(szFullName) - 1 );
   EnableWindow( hwndEdit, fUseInternet );

   /* Set the mail reply address.
    */
   VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_REPLYADDRESS ) );
   Edit_SetText( hwndEdit, szMailReplyAddress );
   Edit_LimitText( hwndEdit, sizeof(szMailReplyAddress) - 1 );
   EnableWindow( hwndEdit, fUseInternet );

   CheckDlgButton( hwnd, IDD_RFC2822, fRFC2822 ); //!!SM!! 2.55.2035
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL Prefs_IPSMTP_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_SMTPAUTH: {
         BOOL fSelected;

         fSelected = IsDlgButtonChecked( hwnd, IDD_SMTPAUTH );
         EnableControl( hwnd, IDD_AUTHINFOUSER, fSelected );
         EnableControl( hwnd, IDD_AUTHINFOPASS, fSelected );
         EnableControl( hwnd, IDD_PAD2, fSelected );
         EnableControl( hwnd, IDD_PAD4, fSelected );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
         }

      case IDD_EMAILADDRESS:
      case IDD_FULLNAME:
      case IDD_REPLYADDRESS:
      case IDD_SMTPSERVER:
      case IDD_SMTPPORT:
         if( codeNotify == EN_UPDATE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_RFC2822:
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL Prefs_IPSMTP_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPREF_SMTP );
         break;

      case PSN_SETACTIVE:
         nLastMailDialogPage = nSMTPPrefsPage;
         break;

      case PSN_APPLY: {
         BOOL fSMTPAuthInfo;
         char sz[ 256 ];
         HWND hwndEdit;
         int nPort;

         /* Read and store the SMTP mail server name.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_SMTPSERVER );
         if( Edit_GetTextLength( hwndEdit ) == 0 )
            {
            PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, nSMTPPrefsPage );
            fMessageBox( hwnd, 0, GS(IDS_STR779), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_SMTPSERVER );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }
         Edit_GetText( hwndEdit, sz, sizeof(szSMTPMailServer) );
         if( lstrcmpi( sz, szSMTPMailServer ) != 0 )
            {
            strcpy( szSMTPMailServer, sz );
            }

         Amuser_WritePPString( szCIXIP, "SMTPMailServer", szSMTPMailServer );

         /* Save the SMTP mail port
          */
         hwndEdit = GetDlgItem( hwnd, IDD_SMTPPORT );
         Edit_GetText( hwndEdit, sz, sizeof(sz) );
         nPort = atoi( sz );
         if (nPort < 1 || nPort > 65535)
         {
            PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, nSMTPPrefsPage );
            wsprintf( lpTmpBuf, GS(IDS_STR1263), sz );
            fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_SMTPPORT );
            return( PSNRET_INVALID_NOCHANGEPAGE );
         }
         Amuser_WritePPLong( szSettings, "SMTPPort", nPort );

         /* Use authentication info?
          */
         fSMTPAuthInfo = IsDlgButtonChecked( hwnd, IDD_SMTPAUTH );
         if( fSMTPAuthInfo )
         {
            char szAuthInfoPass[ 64 ];

            /* Set the user name.
             */
            hwndEdit = GetDlgItem( hwnd, IDD_AUTHINFOUSER );
            Edit_GetText( hwndEdit, szSMTPMailLogin, sizeof(szSMTPMailLogin) );
            Amuser_WritePPString( szCIXIP, "SMTPMailLogin", szSMTPMailLogin);

            /* Set the password.
             */
            memset( szAuthInfoPass, 0, sizeof(szAuthInfoPass) );
            hwndEdit = GetDlgItem( hwnd, IDD_AUTHINFOPASS );
            Edit_GetText( hwndEdit, szSMTPMailPassword, sizeof(szSMTPMailPassword) );
            Amuser_Encrypt( szSMTPMailPassword, rgEncodeKey );
            EncodeLine64( szSMTPMailPassword, LEN_PASSWORD, szAuthInfoPass );
            Amuser_WritePPString( szCIXIP, "SMTPMailPassword", szAuthInfoPass);
         }
         else
         {
            Amuser_WritePPString( szCIXIP, "SMTPMailLogin", "");
            Amuser_WritePPString( szCIXIP, "SMTPMailPassword", "");
         }

         /* Read and store the mail address.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_EMAILADDRESS );
         if( Edit_GetTextLength( hwndEdit ) == 0 )
            {
            PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, nSMTPPrefsPage );
            fMessageBox( hwnd, 0, GS(IDS_STR783), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_EMAILADDRESS );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }
         Edit_GetText( hwndEdit, szMailAddress, sizeof(szMailAddress) );
         Amuser_WritePPString( szCIXIP, "MailAddress", szMailAddress );

         /* Read and store the mail reply address.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_REPLYADDRESS );
         if( Edit_GetTextLength( hwndEdit ) == 0 )
            {
            PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, nSMTPPrefsPage );
            fMessageBox( hwnd, 0, GS(IDS_STR1217), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_REPLYADDRESS );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }
         Edit_GetText( hwndEdit, szMailReplyAddress, sizeof(szMailReplyAddress) );
         Amuser_WritePPString( szCIXIP, "MailReplyAddress", szMailReplyAddress );

         /* Read and store the full name.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_FULLNAME );
         if( Edit_GetTextLength( hwndEdit ) == 0 )
            {
            PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, nSMTPPrefsPage );
            fMessageBox( hwnd, 0, GS(IDS_STR784), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_FULLNAME );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }
         Edit_GetText( hwndEdit, szFullName, sizeof(szFullName) );
         Amuser_WritePPString( szCIXIP, "MailFullName", szFullName );

         /* Save the mail settings
          */
         fRFC2822 = IsDlgButtonChecked( hwnd, IDD_RFC2822 ); //!!SM!! 2.55.2035
         Amuser_WritePPInt( szSettings, "RFC2822", fRFC2822 ); //!!SM!! 2.55.2035 

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
BOOL EXPORT CALLBACK CIXIPBlinkProperties( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, CIXIPBlinkProperties_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, CIXIPBlinkProperties_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, CIXIPBlinkProperties_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL CIXIPBlinkProperties_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPCAMPROPSHEETPAGE psp;
   LPBLINKENTRY lpbe;
   BOOL fSMTPAuthInfo; // !!SM!!
   
   char sz[ 60 ];
   char szBlinkSMTP[ 64 ];
   char szAuthInfoPass[ 80 ];

   /* Dereference and save the handle of the database, folder
    * or topic whose properties we're showing.
    */
   psp = (LPCAMPROPSHEETPAGE)lParam;
   lpbe = (LPVOID)psp->lParam;
   SetWindowLong( hwnd, DWL_USER, (LPARAM)lpbe );

   /* Set the frame title.
    */
   GetDlgItemText( hwnd, IDD_PAD1, sz, sizeof(sz) );
   wsprintf( lpTmpBuf, sz, szProvider );
   SetDlgItemText( hwnd, IDD_PAD1, lpTmpBuf );

   /* Set the various options.
    */
   CheckDlgButton( hwnd, IDD_DOWNLOADNEWS, TestF(lpbe->dwBlinkFlags, BF_GETIPNEWS) );
   CheckDlgButton( hwnd, IDD_DOWNLOADTAGGED, TestF(lpbe->dwBlinkFlags, BF_GETIPTAGGEDNEWS) );
   CheckDlgButton( hwnd, IDD_POSTNEWS, TestF(lpbe->dwBlinkFlags, BF_POSTIPNEWS) );
   CheckDlgButton( hwnd, IDD_GETMAIL, TestF(lpbe->dwBlinkFlags, BF_GETIPMAIL) );
   CheckDlgButton( hwnd, IDD_POSTMAIL, TestF(lpbe->dwBlinkFlags, BF_POSTIPMAIL) );

   strcpy( lpTmpBuf, lpbe->szName );
   strcat( lpTmpBuf, "-SMTP" );
   Amuser_GetPPString( szSMTPServers, lpTmpBuf, "", szBlinkSMTP, sizeof( szBlinkSMTP ) );
   if( strlen( szBlinkSMTP ) == 0 )
      strcpy( szBlinkSMTP, szSMTPMailServer );
   Edit_SetText( GetDlgItem( hwnd, IDD_BLINKSMTP ), szBlinkSMTP );

   strcpy( lpTmpBuf, lpbe->szName );
   strcat( lpTmpBuf, "-SMTP User" );
   Amuser_GetPPString( szSMTPServers, lpTmpBuf, "", lpbe->szAuthInfoUser, sizeof( lpbe->szAuthInfoUser ) );

   strcpy( lpTmpBuf, lpbe->szName );
   strcat( lpTmpBuf, "-SMTP Password" );
   Amuser_GetPPString( szSMTPServers, lpTmpBuf, "", szAuthInfoPass, sizeof(szAuthInfoPass) );

   DecodeLine64( szAuthInfoPass, lpbe->szAuthInfoPass, sizeof(lpbe->szAuthInfoPass)-1 );

   fSMTPAuthInfo = *lpbe->szAuthInfoUser != '\0';
   if( fSMTPAuthInfo )
      {
      Edit_SetText( GetDlgItem( hwnd, IDD_AUTHINFOUSER ), lpbe->szAuthInfoUser );
      Amuser_Decrypt( lpbe->szAuthInfoPass, rgEncodeKey );
      Edit_SetText( GetDlgItem( hwnd, IDD_AUTHINFOPASS ), lpbe->szAuthInfoPass );
      Amuser_Encrypt( lpbe->szAuthInfoPass, rgEncodeKey );
      }
   Edit_LimitText( GetDlgItem( hwnd, IDD_AUTHINFOUSER ), sizeof(lpbe->szAuthInfoUser) - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_AUTHINFOPASS ), sizeof(lpbe->szAuthInfoPass) - 1 );

   /* Set the authinfo stuff.
    */
   CheckDlgButton( hwnd, IDD_SMTPAUTH, fSMTPAuthInfo );
   EnableControl( hwnd, IDD_AUTHINFOUSER, fSMTPAuthInfo );
   EnableControl( hwnd, IDD_AUTHINFOPASS, fSMTPAuthInfo );
   EnableControl( hwnd, IDD_PAD2, fSMTPAuthInfo );
   EnableControl( hwnd, IDD_PAD4, fSMTPAuthInfo );

   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL CIXIPBlinkProperties_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_LIST:
      if( codeNotify == CBN_SELCHANGE )
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_SMTPAUTH: {
         BOOL fSelected;

         fSelected = IsDlgButtonChecked( hwnd, IDD_SMTPAUTH );
         EnableControl( hwnd, IDD_AUTHINFOUSER, fSelected );
         EnableControl( hwnd, IDD_AUTHINFOPASS, fSelected );
         EnableControl( hwnd, IDD_PAD2, fSelected );
         EnableControl( hwnd, IDD_PAD4, fSelected );
//       PropSheet_Changed( GetParent( hwnd ), hwnd );
//       break;
         }
      case IDD_AUTHINFOUSER:
      case IDD_AUTHINFOPASS:           
      case IDD_DOWNLOADNEWS:
      case IDD_DOWNLOADTAGGED:
      case IDD_POSTNEWS:
      case IDD_GETMAIL:
      case IDD_POSTMAIL:
      case IDD_BLINKSMTP:
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL CIXIPBlinkProperties_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsBLINK_IP );
         break;

      case PSN_APPLY: {
         LPBLINKENTRY lpbe;
         HWND hwndEdit;
         BOOL fSMTPAuthInfo; // !!SM!!
         char sz[ 256 ];


         /* Update the blink entry structure.
          */
         lpbe = (LPBLINKENTRY)GetWindowLong( hwnd, DWL_USER );
         lpbe->dwBlinkFlags &= ~BF_IPFLAGS;
         if( IsDlgButtonChecked( hwnd, IDD_DOWNLOADNEWS ) )
            lpbe->dwBlinkFlags |= BF_GETIPNEWS;
         if( IsDlgButtonChecked( hwnd, IDD_DOWNLOADTAGGED ) )
            lpbe->dwBlinkFlags |= BF_GETIPTAGGEDNEWS;
         if( IsDlgButtonChecked( hwnd, IDD_POSTNEWS ) )
            lpbe->dwBlinkFlags |= BF_POSTIPNEWS;
         if( IsDlgButtonChecked( hwnd, IDD_GETMAIL ) )
            lpbe->dwBlinkFlags |= BF_GETIPMAIL;
         if( IsDlgButtonChecked( hwnd, IDD_POSTMAIL ) )
            lpbe->dwBlinkFlags |= BF_POSTIPMAIL;
         
         /* Read and store the mail address.
          */

         hwndEdit = GetDlgItem( hwnd, IDD_BLINKSMTP );
         if( Edit_GetTextLength( hwndEdit ) == 0 )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR779), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_BLINKSMTP );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }
         Edit_GetText( hwndEdit, sz, sizeof(sz) );

         strcpy( lpTmpBuf, lpbe->szName );
         strcat( lpTmpBuf, "-SMTP" );
         Amuser_WritePPString( szSMTPServers, lpTmpBuf, sz );

         //!!SM!!
         memset( &lpbe->szAuthInfoUser, 0, sizeof(lpbe->szAuthInfoUser) );
         memset( &lpbe->szAuthInfoPass, 0, sizeof(lpbe->szAuthInfoPass) );

         /* Use authentication info?
          */
         fSMTPAuthInfo = IsDlgButtonChecked( hwnd, IDD_SMTPAUTH );
         if( fSMTPAuthInfo )
         {
            /* Set the user name.
             */
            hwndEdit = GetDlgItem( hwnd, IDD_AUTHINFOUSER );
            Edit_GetText( hwndEdit, lpbe->szAuthInfoUser, sizeof(lpbe->szAuthInfoUser) );

            /* Set the password.
             */
            hwndEdit = GetDlgItem( hwnd, IDD_AUTHINFOPASS );
            Edit_GetText( hwndEdit, lpbe->szAuthInfoPass, sizeof(lpbe->szAuthInfoPass) );
            Amuser_Encrypt( lpbe->szAuthInfoPass, rgEncodeKey );

            SaveBlinkSMTPAuth( lpbe );
         }
         else
         {
            strcpy( lpTmpBuf, lpbe->szName );
            strcat( lpTmpBuf, "-SMTP User" );
            Amuser_WritePPString( szSMTPServers, lpTmpBuf, "");

            strcpy( lpTmpBuf, lpbe->szName );
            strcat( lpTmpBuf, "-SMTP Password" );
            Amuser_WritePPString( szSMTPServers, lpTmpBuf, "");
         }

         // !!SM!!
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
BOOL EXPORT CALLBACK InBasket_Server( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, InBasket_Server_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, InBasket_Server_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, InBasket_Server_OnNotify );
      HANDLE_DLGMSG( hwnd, WM_DRAWITEM, UsenetServers_OnDrawItem );
      HANDLE_DLGMSG( hwnd, WM_MEASUREITEM, UsenetServers_OnMeasureItem );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL InBasket_Server_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   char szSubNewsServer[ 64 ];
   char szNewsReplyTo[ 64 ];
   char szNewsFullName[ 64 ];
   char szNewsMailAddress[ 64 ];
   char szNewsAttachDir[ _MAX_PATH ];

   LPCAMPROPSHEETPAGE psp;
   LPVOID pData;

   /* Dereference and save the handle of the database, folder
    * or topic whose properties we're showing.
    */
   psp = (LPCAMPROPSHEETPAGE)lParam;
   pData = (LPVOID)psp->lParam;
   SetWindowLong( hwnd, DWL_USER, (LPARAM)pData );
   InBasket_Prop_ShowTitle( hwnd, pData );

   /* Get the news server for this topic.
    */
   Amdb_GetCookie( (PTL)pData, NEWS_SERVER_COOKIE, szSubNewsServer, szNewsServer, sizeof(szSubNewsServer) );
   Amdb_GetCookie( (PTL)pData, NEWS_REPLY_TO_COOKIE, szNewsReplyTo, szMailReplyAddress, sizeof(szNewsReplyTo) );
   Amdb_GetCookie( (PTL)pData, NEWS_FULLNAME_COOKIE, szNewsFullName, szFullName, sizeof(szNewsFullName) );
   Amdb_GetCookie( (PTL)pData, NEWS_EMAIL_COOKIE, szNewsMailAddress, szMailAddress, sizeof(szNewsMailAddress) );
    Amdb_GetCookie( (PTL)pData, NEWS_ATTACHMENTS_COOKIE, szNewsAttachDir, pszAttachDir, sizeof(szNewsAttachDir) );
    
   /* Fill the input fields.
    */
   Edit_SetText( GetDlgItem( hwnd, IDD_EMAILADDRESS ), szNewsMailAddress );
   Edit_SetText( GetDlgItem( hwnd, IDD_REPLYADDRESS ), szNewsReplyTo );
   Edit_SetText( GetDlgItem( hwnd, IDD_FULLNAME ), szNewsFullName );
   Edit_SetText( GetDlgItem( hwnd, IDD_MAILATTACHMENTS ), szNewsAttachDir );
   
   /* Set edit field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_EMAILADDRESS ), 63 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_REPLYADDRESS ), 63 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_FULLNAME ), 63 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_MAILATTACHMENTS ), _MAX_PATH - 1);

   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_BROWSE ), hInst, IDB_FOLDERBUTTON );

   /* Fill the list box with the list of news servers.
    */
   FillUsenetServersCombo( hwnd, IDD_LIST, szSubNewsServer );

   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL InBasket_Server_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
   {
      case IDD_EMAILADDRESS:
      case IDD_FULLNAME:
      case IDD_REPLYADDRESS:
      case IDD_MAILATTACHMENTS:
         if( codeNotify == EN_UPDATE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_LIST:
         if( codeNotify == CBN_SELCHANGE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
      case IDD_BROWSE: 
      {
         CHGDIR chgdir;
         HWND hwndEdit;

         hwndEdit = GetDlgItem( hwnd, IDD_MAILATTACHMENTS );
         Edit_GetText( hwndEdit, chgdir.szPath, 144 );
         strcpy( chgdir.szTitle, GS(IDS_STR522) );
         strcpy( chgdir.szPrompt, "Select Download Path For Attachments"/*GS(IDS_STR524)*/ );
         chgdir.hwnd = hwnd;
         if( Amuser_ChangeDirectory( &chgdir ) )
            Edit_SetText( hwndEdit, chgdir.szPath );
         SetFocus( hwndEdit );
         break;
      }
   }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL InBasket_Server_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsIBP_SERVER );
         break;

      case PSN_SETACTIVE:
         nLastPropertiesPage = nCIXIPPropertiesPage;
         break;

      case PSN_APPLY: {
         char szNewsReplyTo[ 64 ];
         char szNewsFullName[ 64 ];
         char szNewsMailAddress[ 64 ];
         char szSubNewsServer[ 64 ];
         char szNewsAttachDir[_MAX_PATH];
         HWND hwndList;
         int index;
         PTL ptl;
         HWND hwndEdit;

         /* Get the newsgroup topic handle.
          */
         ptl = (PTL)GetWindowLong( hwnd, DWL_USER );

         /* Get the selected news server.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         index = ComboBox_GetCurSel( hwndList );
         ASSERT( CB_ERR != index );
         ComboBox_GetLBText( hwndList, index, szSubNewsServer );

         Edit_GetText( GetDlgItem( hwnd, IDD_MAILATTACHMENTS ), szNewsAttachDir, _MAX_PATH );
         if( !ValidateChangedDir( hwnd, szNewsAttachDir ) )
            break;

         /* Save cookie.
          */
         Amdb_SetCookie( ptl, NEWS_SERVER_COOKIE, szSubNewsServer );

         hwndEdit = GetDlgItem( hwnd, IDD_EMAILADDRESS );
         if( Edit_GetTextLength( hwndEdit ) == 0 )
            {
            PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, nCIXIPPropertiesPage );
            fMessageBox( hwnd, 0, GS(IDS_STR783), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_EMAILADDRESS );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }

         hwndEdit = GetDlgItem( hwnd, IDD_FULLNAME );
         if( Edit_GetTextLength( hwndEdit ) == 0 )
            {
            PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, nCIXIPPropertiesPage );
            fMessageBox( hwnd, 0, GS(IDS_STR784), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_FULLNAME );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }

         hwndEdit = GetDlgItem( hwnd, IDD_REPLYADDRESS );
         if( Edit_GetTextLength( hwndEdit ) == 0 )
            {
            PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, nCIXIPPropertiesPage );
            fMessageBox( hwnd, 0, GS(IDS_STR1217), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_REPLYADDRESS );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }

         /* Get poster details.
          */
         Edit_GetText( GetDlgItem( hwnd, IDD_EMAILADDRESS ), szNewsMailAddress, 64 );
         Edit_GetText( GetDlgItem( hwnd, IDD_REPLYADDRESS ), szNewsReplyTo, 64 );
         Edit_GetText( GetDlgItem( hwnd, IDD_FULLNAME ), szNewsFullName, 64 );

         /* Save cookies for poster details.
          */
         Amdb_SetCookie( ptl, NEWS_REPLY_TO_COOKIE, szNewsReplyTo );
         Amdb_SetCookie( ptl, NEWS_FULLNAME_COOKIE, szNewsFullName );
         Amdb_SetCookie( ptl, NEWS_EMAIL_COOKIE, szNewsMailAddress );
         Amdb_SetCookie( ptl, NEWS_ATTACHMENTS_COOKIE, szNewsAttachDir);

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
BOOL EXPORT CALLBACK InBasket_Server_Mail( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, InBasket_Server_Mail_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, InBasket_Server_Mail_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, InBasket_Server_Mail_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL InBasket_Server_Mail_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   char szMailReplyTo[ 64 ];
   char szMailFullName[ 64 ];
   char szMailMailAddress[ 64 ];
   char szMailAttachDir[_MAX_PATH];

   LPCAMPROPSHEETPAGE psp;
   LPVOID pData;

   /* Dereference and save the handle of the database, folder
    * or topic whose properties we're showing.
    */
   psp = (LPCAMPROPSHEETPAGE)lParam;
   pData = (LPVOID)psp->lParam;
   SetWindowLong( hwnd, DWL_USER, (LPARAM)pData );
   InBasket_Prop_ShowTitle( hwnd, pData );

   /* Get the news server for this topic.
    */
   Amdb_GetCookie( (PTL)pData, MAIL_REPLY_TO_COOKIE_1, szMailReplyTo, szMailReplyAddress, sizeof(szMailReplyTo) );
   Amdb_GetCookie( (PTL)pData, MAIL_NAME_COOKIE, szMailFullName, szFullName, sizeof(szMailFullName) );
   Amdb_GetCookie( (PTL)pData, MAIL_ADDRESS_COOKIE, szMailMailAddress, szMailAddress, sizeof(szMailMailAddress) );
    Amdb_GetCookie( (PTL)pData, MAIL_ATTACHMENTS_COOKIE, szMailAttachDir, pszAttachDir, sizeof(szMailAttachDir) );

   /* Fill the input fields.
    */
   Edit_SetText( GetDlgItem( hwnd, IDD_EMAILADDRESS ), szMailMailAddress );
   Edit_SetText( GetDlgItem( hwnd, IDD_REPLYADDRESS ), szMailReplyTo );
   Edit_SetText( GetDlgItem( hwnd, IDD_FULLNAME ), szMailFullName );
   Edit_SetText( GetDlgItem( hwnd, IDD_MAILATTACHMENTS ), szMailAttachDir );
   
   /* Set edit field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_EMAILADDRESS ), 63 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_REPLYADDRESS ), 63 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_FULLNAME ), 63 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_MAILATTACHMENTS ), _MAX_PATH - 1);

   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_BROWSE ), hInst, IDB_FOLDERBUTTON );

   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL InBasket_Server_Mail_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EMAILADDRESS:
      case IDD_FULLNAME:
      case IDD_MAILATTACHMENTS:
      case IDD_REPLYADDRESS:
         if( codeNotify == EN_UPDATE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
      case IDD_BROWSE: {
         CHGDIR chgdir;
         HWND hwndEdit;

         hwndEdit = GetDlgItem( hwnd, IDD_MAILATTACHMENTS );
         Edit_GetText( hwndEdit, chgdir.szPath, 144 );
         strcpy( chgdir.szTitle, GS(IDS_STR522) );
         strcpy( chgdir.szPrompt, "Select Download Path For Attachments"/*GS(IDS_STR524)*/ );
         chgdir.hwnd = hwnd;
         if( Amuser_ChangeDirectory( &chgdir ) )
            Edit_SetText( hwndEdit, chgdir.szPath );
         SetFocus( hwndEdit );
         break;
         }
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL InBasket_Server_Mail_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsIBP_MAIL );
         break;

      case PSN_SETACTIVE:
         nLastPropertiesPage = nCIXIPPropertiesPage;
         break;

      case PSN_APPLY: {
         char szMailReplyTo[ 64 ];
         char szMailFullName[ 64 ];
         char szMailMailAddress[ 64 ];
         char szMailAttachDir[_MAX_PATH];
         PTL ptl;
         HWND hwndEdit;

         /* Get the newsgroup topic handle.
          */
         ptl = (PTL)GetWindowLong( hwnd, DWL_USER );

         /*
         !!SM!!
         */
         Edit_GetText( GetDlgItem( hwnd, IDD_FULLNAME ), szMailFullName, 64 );
         Edit_GetText( GetDlgItem( hwnd, IDD_EMAILADDRESS ), szMailMailAddress, 64 );
         Edit_GetText( GetDlgItem( hwnd, IDD_REPLYADDRESS ), szMailReplyTo, 64 );
         Edit_GetText( GetDlgItem( hwnd, IDD_MAILATTACHMENTS ), szMailAttachDir, _MAX_PATH );

         Edit_GetText( GetDlgItem( hwnd, IDD_MAILATTACHMENTS ), szMailAttachDir, _MAX_DIR );
         if( !ValidateChangedDir( hwnd, szMailAttachDir ) )
            break;
         
         /* Get poster details.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_EMAILADDRESS );
         if( Edit_GetTextLength( hwndEdit ) == 0 || strstr( szMailMailAddress, "@") == NULL  )
            {
            PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, nCIXIPPropertiesPage );
            fMessageBox( hwnd, 0, GS(IDS_STR783), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_EMAILADDRESS );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }

         hwndEdit = GetDlgItem( hwnd, IDD_FULLNAME );
         if( Edit_GetTextLength( hwndEdit ) == 0 )
            {
            PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, nCIXIPPropertiesPage );
            fMessageBox( hwnd, 0, GS(IDS_STR784), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_FULLNAME );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }

         hwndEdit = GetDlgItem( hwnd, IDD_REPLYADDRESS );
         if( Edit_GetTextLength( hwndEdit ) == 0 || strstr( szMailReplyTo, "@") == NULL)
            {
            PropSheet_SetCurSel( lpnmhdr->hwndFrom, 0L, nCIXIPPropertiesPage );
            fMessageBox( hwnd, 0, GS(IDS_STR1217), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_REPLYADDRESS );
            return( PSNRET_INVALID_NOCHANGEPAGE );
            }

         /*
         !!SM!!
         Edit_GetText( GetDlgItem( hwnd, IDD_FULLNAME ), szMailFullName, 64 );
         Edit_GetText( GetDlgItem( hwnd, IDD_EMAILADDRESS ), szMailMailAddress, 64 );
         Edit_GetText( GetDlgItem( hwnd, IDD_REPLYADDRESS ), szMailReplyTo, 64 );
         */
         
         /* Save cookies for poster details.
          */
         Amdb_SetCookie( ptl, MAIL_REPLY_TO_COOKIE_1, szMailReplyTo );
         Amdb_SetCookie( ptl, MAIL_NAME_COOKIE, szMailFullName );
         Amdb_SetCookie( ptl, MAIL_ADDRESS_COOKIE, szMailMailAddress );
         Amdb_SetCookie( ptl, MAIL_ATTACHMENTS_COOKIE, szMailAttachDir);

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}
