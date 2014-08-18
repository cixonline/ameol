/* MAIN.C - Ameol main program
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
#include <ctype.h>
#include <time.h>
#include "mapi.h"
#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <io.h>
#include <direct.h>
#include <commdlg.h>
#include <string.h>
#include <htmlhelp.h>
#include "mnugrp.h"
#include "ssce.h"
#include <stdarg.h>
#include "intcomms.h"
#include "rules.h"
#include "shared.h"
#include "cixip.h"
#include "cix.h"
#include "local.h"
#include "amlib.h"
#include "toolbar.h"
#include "editmap.h"
#include "amaddr.h"
#include "ameol2.h"
#include "amevent.h"
#include "command.h"
#include <shellapi.h>
#include "blinkman.h"
#include "trace.h"
#include "news.h"
#include "ini.h"
#include "admin.h"
#include "common.bld"
#include "demo.h"
#include "lbf.h"
#include "log.h"
#include "amevent.h"
#include "winsparkle.h"

#ifndef USEBIGEDIT
#include "scintilla.h"
#include "scilexer.h"
#endif USEBIGEDIT

#include <richedit.h> // !!SM!!

int FASTCALL b64decodePartial(char *s);
LPSTR GetReadableText(PTL ptl, LPSTR pSource); // !!SM!! 2039

/* Intellimouse support.
 */
#include "zmouse.h"

#define  THIS_FILE   __FILE__

/* Build of DLLs required to run this version of Ameol.
 */
#define  REQ_AMCTRLS_BUILD       464

/* Build below which they must re-run Setup Wizard for
 * each new account.
 */
#define  MIN_VALID_SETUP_BUILD   1832

#define  MEB_HASMSG        0x0001
#define  MEB_CONF          0x0002
#define  MEB_MSGWND        0x0004
#define  MEB_READWRITE     0x0008
#define  MEB_NEWSTOPIC     0x0010
#define  MEB_MAILTOPIC     0x0020
#define  MEB_ATTACHMENT    0x0040
#define  MEB_HASFULLMSG    0x0080
#define  MEB_INBASKET      0x0100
#define  MEB_CIXTOPIC      0x0200
#define  MEB_RESIGNED      0x0400
#define  MEB_CIXCONF       0x0800
#define  MEB_TOPIC         0x1000

#define  BUTTONS_PER_SLOT  20
#define  IDMSB_MAX         6

#ifndef USEMUTEX // 2.55.2031 Create a lock file instead of useing a user based Mutex, works across networks / terminal sessions.
HANDLE fLockFile;
#endif USEMUTEX // 2.55.2031
HWND CheckFunc(LPSTR userName); //2.55.2032

//#define COLOR_MODERATED_FOLDERS      1

#define  WNNC_NET_TYPE              0x0002
#define  WNNC_NET_MULTINET          0x8000
#define  WNNC_SUBNET_WinWorkgroups  0x0004

static BOOL fDefDlgEx = FALSE;            /* DefDlg recursion flag trap */
static BOOL fInitTips = FALSE;                     /* TRUE if we have initted tips */

#pragma data_seg(".sdata")
LONG cInstances = 0L;            /* Usage count */
#pragma data_seg()

BOOL fIsAmeol2Scratchpad;
char fCurURL[255];               /* Current URL */ // 2.56.2051 FS#96

HWND fLastActiveWindow;          /*!!SM!! Last Active Window when switching Apps */

HMENU hPopupMenus;               /* Handle of popup menus */
HMENU hMoveMenu;                 /* Move menu added to In Basket popup menu */
HMENU hMainMenu;                 /* Handle of main menu */

HCURSOR hWaitCursor;             /* Handle of hour glass cursor */

HWND hwndMDIClient;              /* MDI client window handle */
HWND hwndActive;                 /* Currently active child window */
HWND hwndQuote;                  /* Handle of quote source window */
HWND hwndFrame;                  /* Handle of main window */
HWND hwndTopic;                  /* Handle of message viewer window */
HWND hwndToolBar;                /* Handle of toolbar window */
HWND hwndToolTips;               /* Handle of toolbar tooltips */
HWND hwndStatus;                 /* Handle of status window */
HWND hwndMDIDlg;                 /* Active MDI dialog window */
HWND hFindDlg;                   /* Handle of the current Find dialog */
HWND hSearchDlg;                 /* Handle of the current Search dialog */
HWND hwndMsg;                    /* Topmost message window */

BOOL fUseTooltips;               /* Flag set TRUE if tooltips are enabled */
BOOL fVisibleSeperators;         /* TRUE if seperators are visible */
BOOL fNewButtons;                /* TRUE if we use the new buttons */ 
BOOL f3DStyle;                   /* TRUE if we use 3D buttons, FALSE if flat buttons */
BOOL fNoAddons;                  /* TRUE if we remove addon support */
BOOL fForceNextUnread;           /* TRUE if we force next unread after parsing */
BOOL fRulesEnabled;              /* TRUE if Rules are enabled */
BOOL fLargeButtons;              /* Flag set TRUE if we're using large toolbar buttons */
BOOL fButtonLabels;              /* Flag set TRUE if toolbar button labels displayed */
BOOL fQuitting;                  /* Flag set when Palantir is being exited */
BOOL fInitialising;              /* Flag set when Palantir is being initialised */
BOOL fShowBlinkWindow;           /* Flag set TRUE if we show the Blink window */
BOOL fCCMailToSender;            /* TRUE if mail always echoed back to sender */
BOOL fThreadMailBySubject;       /* TRUE if we rethread implicit mail by subject */
BOOL fProtectMailFolders;        /* TRUE if we protect mail folders from deletion */
BOOL fBlankSubjectWarning;       /* TRUE if we warn before saving mail messages with no subject */
BOOL fNewMailAlert;              /* TRUE is we display an alert after new mail has arrived */
BOOL fNoisy;                     /* TRUE if we go beep for dialogs */
BOOL fMultiLineTabs;             /* TRUE if tabbed fialogs are multi-line */
BOOL fUseMButton;                /* TRUE if we go to the next unread when clicking the middle mouse button */
BOOL fBasicMode;                 /* TRUE if we work in basic mode */
BOOL fUseDictionary;             /* Set TRUE if we can use the dictionary */
BOOL fBeepAtEnd;                 /* TRUE if Palantir beeps at end of a topic */
BOOL fAutoQuoteNews;             /* TRUE if we autoquote a news article follow-up */
BOOL fAutoQuoteMail;             /* TRUE if we autoquote a mail reply */
BOOL fAutoDecode;                /* TRUE if we automatically decode articles */
BOOL fAutoIndent;                /* Enable auto-indent */
BOOL fQuitAfterConnect;          /* TRUE if we quit after connect */
BOOL fNoExit;                    /* TRUE if we don't exit after a Sage run */
BOOL fSageSettings;              /* TRUE if we only display our Preferences dialog */
BOOL fAutoConnect;               /* TRUE if we blink immediately after starting */
BOOL fShowNext;                  /* TRUE if we immediately show next un-read */
BOOL fShowFirst;                 /* TRUE if we immediately show the first unread message */
BOOL fColourQuotes;              /* TRUE if quoted text is coloured. */
BOOL fIsActive;                  /* Whether or not this app is active */
BOOL fEditAbort;                 /* TRUE if an edit action is aborted because of lack of memory */
BOOL fLaptopKeys;                /* TRUE if we reverse the function of the Pg and Ctrl+Pg keys */
BOOL fReadOnStartup;             /* TRUE if we read any scratchpad when we start */
BOOL fInitiateQuit;              /* TRUE if we're quitting Palantir */
BOOL fMainAbort;                 /* Main abort flag */
BOOL fDrawThreadLines;           /* TRUE if we draw thread lines */
BOOL fDeleteMailAfterReading;    /* TRUE if we delete mail from POP3 server after retriveing */
BOOL fEditAtTop;                 /* TRUE if we edit/sig position it at the top */
BOOL fPOP3Last;                  /* TRUE if we do POP3 last */
BOOL fOnline;                    /* TRUE if we're online */
BOOL fCopyRule;                  /* TRUE is we enable the copy rule command */
BOOL fBlinking;                  /* TRUE if we're blinking */
BOOL fKeyboardChanged;           /* TRUE if the keyboard has changed */
BOOL fStripHeaders;              /* TRUE if we don't display headers */
BOOL fMsgStyles;                 /* TRUE if we display message styles */
BOOL fShowTips;                  /* TRUE is we show the Tip of the day on startup */
BOOL fWordWrap;                  /* TRUE if we word wrap messages */
BOOL fHotLinks;                  /* TRUE if hotlinks are enabled */
BOOL fStartOnline;               /* TRUE if we start Palantir in online mode */
BOOL fWarnOnExit;                /* TRUE if we warn if outbasket not empty on exit */
BOOL fWarnAfterBlink;            /* TRUE if we warn if outbasket not empty after a blink */
BOOL fAlertOnFailure;            /* TRUE if we should check the blink log for error messages */
BOOL fNoLogo;                    /* TRUE if we don't show the startup logo */
BOOL fNoCMN;                     /* TRUE if we don't want to colour mail/news folders */
BOOL fNoOLT;                     /* TRUE if we don't run the online timeout code */
BOOL fPhysicalDelete;            /* TRUE if Delete Message command deletes message */
BOOL fRememberPassword;          /* TRUE if we remember the mail password */
BOOL fDisconnectTimeout;         /* TRUE if disconnect timer enabled */
BOOL fBrowserIsMSIE;             /* TRUE if MSIE is default browser */
BOOL fUseInternet;               /* TRUE if internet conferencing support installed */
BOOL fUseLegacyCIX;              /* TRUE if we support CIX mail */
BOOL fUseCIX;                    /* TRUE if CIX conferencing support installed */
BOOL fCIXViaInternet;            /* TRUE if we connect to CIX via internet */
BOOL fShowToolBar;               /* TRUE if toolbar is visible */
BOOL fShowStatusBar;             /* TRUE if status bar is visible */
BOOL fParsing;                   /* TRUE if scratchpad parse in progress */
BOOL fParseDuringBlink;          /* TRUE if we parse while blinking */
BOOL fRunningScript;             /* TRUE if we're running a script */
BOOL fCompletingSetupWizard;     /* TRUE if we have to complete Setup Wizard */
BOOL fAutoCollapse;              /* TRUE if we collapse folders as we move between them */
BOOL fLaunchAfterDecode;         /* TRUE if we launch file after decoding */
BOOL fSecureOpen;                /* TRUE if we're in secure mode */
BOOL fShowClock;                 /* TRUE if we display a clock on the status bar */
BOOL fShortHeaders;              /* TRUE if we display short headers */
BOOL fShortHeadersSubject;       /* TRUE if we display the Subject field */
BOOL fShortHeadersFrom;          /* TRUE if we display the From field */
BOOL fShortHeadersTo;            /* TRUE if we display the To field */
BOOL fShortHeadersCC;            /* TRUE if we display the CC field */
BOOL fShortHeadersDate;          /* TRUE if we display the CC field */ /*!!SM!!*/
BOOL fShowMugshot;               /* TRUE if we show mugshots */
BOOL fStretchMugshot;            /* TRUE if mugshot is stretched to fit window */
BOOL fQuitSeen;                  /* TRUE if WM_QUIT posted */
BOOL fUnreadAfterBlink;          /* TRUE if we show first unread after blink */
BOOL fShowTopicType;             /* TRUE if we show the folder type in the in-basket */
BOOL fSetup;                     /* TRUE if we run Setup Wizard after launching */
BOOL fEndBlinking;               /* TRUE if user terminates a blink. */
BOOL fUseCopiedMsgSubject;       /* TRUE if we pick the original subject for copied messages */
BOOL fViewCixTerminal;           /* TRUE if we view CIX terminal */
BOOL fFirstRun;                  /* TRUE if this is the first time Ameol was run */
BOOL fFirstTimeThisUser;         /* TRUE if this is the first time this user logged on */
BOOL fDebugMode;                 /* TRUE if we're running in debug mode */
BOOL fOldLogo;                   /* TRUE if we display the old logo */
BOOL fDNSCache;                  /* TRUE if we cache DNS lookups */
BOOL fLastStateWasMaximised;     /* Last non-iconic state of the main window */
BOOL fWorkspaceScrollbars;       /* TRUE if we show scroll bars in the MDI workspace */
BOOL fShortFilenames;            /* TRUE if OS only supports short filenames */
BOOL fFileDebug;
BOOL fAltMsgLayout;              /* TRUE if we use the new message window layout */
BOOL fNewMail = FALSE;           /* TRUE when new mail has arrived (plays new mail sound) */
BOOL fNewMailPlayed = FALSE;     /* Tracks whether the new mail sound has been played */
BOOL fYIA = FALSE;
BOOL fAutoCheck = FALSE;
BOOL fNoCheck;                   /* TRUE if the auto-check should not run */
BOOL fAlertTimedOut;             /* TRUE if the mail alert has timed out */
BOOL fWinNT;                     /* TRUE if we're under NT */
BOOL fSingleClickHotspots;       /* TRUE should we use a single click to activate a hotspot 2.55.2031 */
BOOL fShowAcronyms;              /* TRUE should we show popup acronyms in the editor 2.55.2033 */
BOOL fPageBeyondEnd;             /* TRUE Allow page down to go beyond end of edit window */
BOOL fShowLineNumbers;           /* FALSE show line numbers in message editor */  
BOOL fShowWrapCharacters;        /* FALSE show LF characters in wrapped lines */  
BOOL fMultiLineURLs;             /* TRUE ignore CR's in URL's */
BOOL fAntiSpamCixnews;           /* TRUE if we sent opt antispam-from to newsnet */
BOOL fRFC2822;                   /* Use Name <address> instead of address (Name) in mail headers*/ //!!SM!! 2.55.2035
DWORD fMaxNewsgroups;            /* Maximum number of newsgroups to base the progress gauge on 64000 !!SM!!*/
BOOL fBreakLinesOnSave;          /* When saving messages this flag turns wrapped lines with linefeeds into hard returns */
BOOL fUseINIfile;                /* Use INI Instead of Registry for setting storage */ // !!SM!! 2.56.2051
BOOL fUseU3;                     /* Ameol is being run from a U3 Device, so use INI and U3 variables*/ // !!SM!! 2.56.2053
BOOL fKillRunning;               /* U3 /Kill Command*/ // !!SM!! 2.56.2053
BOOL fUseWebServices;            /* Use Web Services instead of COSY interface */ // !!SM!! 2.56.2053
HFONT hToolbarFont = NULL;       /* Toolbar font */
BOOL fCheckForAttachments;       /* Check for "attach" in messages !!SM!! 2.55.2033 */

#ifdef _DEBUG
BOOL fUsingPathBuf;              /* TRUE if lpPathBuf is in active use */
#endif

BOOL fAutoSpellCheck;            /* TRUE if we do an automatic spelling check */
WORD wSpellSelStart;             /* Spell selection start */
WORD wSpellSelEnd;               /* Spell selection end */

WNDPROC lpfMdiClientProc;        /* Subclassed MDI client window procedure */
HBITMAP hBmpbackground;          /* Bitmap to be painted in MDI client window proc */
HPALETTE hPalbackground;         /* Palette for background */
BOOL fStretchBmp;                /* TRUE if we stretch the bitmap, FALSE if we tile */

HICON hAppIcon;                  /* Application icon */

UINT BF_IPFLAGS;                 /* All Internet flags */
UINT BF_CIXFLAGS;                /* All CIX flags */
UINT BF_POSTCIX;                 /* All CIX post flags */
UINT BF_GETCIX;                  /* All CIX get flags */
UINT BF_IPUSENET;                /* All Internet Usenet flags */
UINT BF_CIXUSENET;               /* All CIX Usenet flags */
UINT BF_USENET;                  /* All Usenet flags */
UINT BF_FULLCONNECT;             /* All full connect actions */
UINT BF_EVERYTHING;              /* All possible flags */

VIEWDATA vdDef;                  /* Default view data structure */
PURGEOPTIONS poDef;              /* Default purge options */

HINSTANCE hInst;                 /* Application instance handle */
HINSTANCE hSpellLib;             /* Spell checker library */
HINSTANCE hRscLib;               /* Handle of resources library */
HINSTANCE hRegLib;               /* Regular Expression and Mail Library */ // !!SM!! 2039
HINSTANCE hWebLib;               /* WebAPI DLL*/ // !!SM!! 2053 
HINSTANCE hSciLexer;             /* Scintilla Editor Library */ // !!SM!! 2041

UINT wSweepRate;                 /* Sweep timer rate */
UINT wDisconnectRate;            /* Disconnection idle timeout */
UINT uFindReplaceMsg;            /* Find/Replace dialog window message number */
UINT uMailMsg;                   /* Mail window message number */

PCAT pcatCIXDatabase = NULL;     /* CIX database cached handle */
PCL pclNewsFolder = NULL;        /* News folder */
PTL ptlPostmaster = NULL;        /* Postmaster folder */
PTL ptlMailFolder = NULL;        /* Outgoing mail folder */
PTL ptlLocalTopicName;
PTL ptlNewMailBox;

void (FASTCALL * fpParser)( void ); /* Pointer to current parsing function */

WORD wWinVer;                    /* Windows version number */
WORD fCompatFlag;                /* Compatibility flags */

HWND hwndWheel;                  /* Intellimouse wheel window */
UINT msgMouseWheel;              /* Mouse wheel messages */
UINT msg3DSupport;               /* 3D? Support */
UINT msgScrollLines;             /* Get scroll lines */
BOOL f3DSupport;                 /* TRUE if 3D? support */

DWORD dwMaxMsgSize;              /* Maximum CIX message size */
DWORD dwLastNonIdleTime;         /* Last non-idle time */
DWORD dwDefWindowState;          /* Default window state for new windows */
DWORD iActiveMode;               /* Active window mode */

BOOL fGetParList;                /* TRUE if we get participant list when we join new conferences */
BOOL fOpenTerminalLog;           /* TRUE if we log the connection to CONNECT.LOG */
BOOL fArcScratch;                /* TRUE if we compress scratchpad before downloading */
int iRecent;                     /* Number of recent messages to get when joining a new conference */
char szCIXConnCard[ 40 ];        /* Name of connection card to use with CIX */

int wInitState;                  /* How far we got thru initialisation so far */
int nMaxToArchive;               /* Maximum number of copies of a file to archive */
int nCIXMailClearCount;          /* Age of cleared CIX mail */
int nIPMailClearCount;           /* Age of cleared IP mail */
int cyInfoBar;                   /* Height of message viewer window info bar */
int iWrapCol;                    /* Word wrap column */ // FS#80
int irWrapCol;                   /* Word wrap column Reader */ // FS#80
int inWrapCol;                   /* Word wrap column Internet News*/ // 2.56.2055
int imWrapCol;                   /* Word wrap column Internet Mail */ // 2.56.2055
int iTabSpaces;                  /* Number of Spaces in a Tab */ 
int nToolbarPos;                 /* Toolbar orientation */
int nMugshotLatency;             /* Mugshot latency (how long it is visible) */
int nMailDelivery;               /* How we deliver mail */
int idmsbText;                   /* Text portion of status bar. */
int idmsbOnlineText;             /* Online text portion of status bar. */
int idmsbClock;                  /* Clock portion of status bar */
int idmsbOutbasket;              /* Outbasket notification portion of status bar */
int iMaxSubjectSize;             /* Maximum length of a Subject line */

//int idmsbMode;
int idmsbRow;
int idmsbColumn;
int nAccountType;                /* Type of account for which signup/setup is run */

char * pszAmeolDir = NULL;       /* Palantir home directory */
char * pszAppDataDir = NULL;     /* App Data Dir, normally Ameol base path unless Users= specified - VistaAlert */ 
char * pszModulePath = NULL;     /* Palantir module pathname */
char * pszUploadDir = NULL;      /* Directory for all uploads */
char * pszDownloadDir = NULL;    /* Directory for all downloads */
char * pszAttachDir = NULL;      /* Directory for all attachments */
char * pszDataDir = NULL;        /* Directory for all data files */
char * pszArchiveDir = NULL;     /* Directory for archived files */
char * pszScriptDir = NULL;      /* Directory for all script files */
char * pszResumesDir = NULL;     /* Directory for resumes */
char * pszScratchDir = NULL;     /* Directory for scratchpad downloads */
char * pszAddonsDir = NULL;      /* Directory for all addons */
char * pszMugshotDir = NULL;     /* Directory for mugshots */
char FAR szUsersDir[ _MAX_PATH]; /* Path to users directory */

#ifdef _DEBUG
BOOL fTraceOutputDebug;          /* TRUE if we trace to the debug terminal */
BOOL fTraceLogFile;              /* TRUE if we trace to the log file */
#endif

LPSTR lpTmpBuf;                  /* General purpose buffer */
LPSTR lpPathBuf;                 /* General purpose filename buffer */

static char szDictLoadMode[ 11 ];   /* How do we load the dictionary? */
static int cxLine;               /* Width of the line number field on the status bar */
static int cxCol;                /* Width of the column number field on the status bar */
static int cxMode;               /* Width of the mode field on the status bar */
static int cxClock;              /* Width of the clock field on the status bar */
static int iCachedButtonID;      /* Cached button ID */

int nRasConnect;              /* RAS connection status */
DWORD dwRasError;             /* Last RAS error */
UINT uRasMsg;                 /* RAS message */
RASDATA rdDef;                /* Default RAS data */

char szRasLib[] = "RASAPI32.DLL";   /* RAS DLL name */

const char NEAR szDefNews[] = "news.cix.co.uk";                /* Default news server */
const char NEAR szDefMail[] = "mail.cix.co.uk";                /* Default mail server */
const char NEAR szDefCIXTelnet[] = "cix.conferencing.co.uk";   /* Telnet to CIX address */
const char NEAR szDefDateFormat[] = "dd/mm/yy hh:nn";          /* Default date format */
const char NEAR szCixnews[] = "cixnews";                       /* CIX Usenet gateway name */
const char NEAR szPostmaster[] = "Messages";                   /* Messages folder */

const char NEAR szDefQuote[] = "In article %m, %a (%f) wrote:";
const char NEAR szDefQuoteDelim[] = ">|";

const char NEAR szCompulink[] = "cix.compulink.co.uk";         /* CIX domain */
const char NEAR szCixCoUk[] = "cix.co.uk";                     /* New CIX domain */

char NEAR szQuoteDelimiters[ LEN_QDELIM+1 ];                   /* Quote delimiter string */
char FAR szQuoteNewsHeader[ 100 ];                             /* Usenet quote header */
char FAR szQuoteMailHeader[ 100 ];                             /* Mail quote header */
char chQuote;                                                  /* Default quote character */

AMAPI_VERSION FAR amv;              /* Version information */
AMAPI_VERSION FAR amvLatest;        /* Latest Version information from COSY*/

BOOL fLogSMTP;                      /* Log SMTP activity */
BOOL fLogNNTP;                      /* Log NNTP activity */
BOOL fLogPOP3;                      /* Log POP3 activity */
BOOL fDisplayLockCount;             /* Display topic lock count */

char FAR szProvider[ 60 ];          /* Name of selected ISP */
char FAR szAccountName[ 64 ];       /* Account name */
char FAR szNewsServer[ 64 ];        /* News server */
char FAR szDomain[ 60 ];            /* Server domain */
char FAR szDefMailDomain[ 60 ];     /* Default mail domain */
char FAR szHostname[ 64 ];          /* Hostname (optional) */
char FAR szSMTPMailServer[ 64 ];    /* SMTP Mail server */
char FAR szMailServer[ 64 ];        /* POP3 Mail server */
char FAR szMailLogin[ 64 ];         /* Mail server login name */
char FAR szMailPassword[ 40 ];      /* Mail server login password */
char FAR szSMTPMailLogin[ 64 ];     /* SMTP Mail server login name */
char FAR szSMTPMailPassword[ 40 ];  /* SMTP Mail server login password */
char FAR szMailAddress[ 64 ];       /* User's mail address */
char FAR szMailReplyAddress[ 64 ];  /* User's reply address */
char FAR szFullName[ 64 ];          /* Full user name */
char FAR szMailBox[ 60 ];           /* Initial mailbox */
char FAR szBrowser[ _MAX_PATH ];    /* Browser */
char FAR szDateFormat[ 30 ];        /* Date format */
char FAR szAutoConnectMode[ 64 ];   /* Mode to use for /auto connect */
char FAR szCixProtocol[ 16 ];       /* Name of CIX file transfer protocol */

char FAR szCIXNickname[ 16 ];       /* CIX nickname */
char FAR szCIXPassword[ 40 ];       /* CIX password */
char FAR szOrganisation[ 64 ];      /* Organisation */
char FAR szUsenetGateway[ 10 ];     /* Name of Usenet gateway */

char szLoginUsername[ LEN_LOGINNAME ];       /* Login name entered at startup */
char szLoginPassword[ LEN_LOGINPASSWORD ];   /* Login name entered at startup */

BOOL fAutoPurge;                        /* TRUE if automatic purge enabled */
char FAR szAutoPurge[ 200 ];            /* Automatic purge conf/topic */

/* Hard coded filenames.
 * Might be a good idea to put these into the RC.
 */
const char NEAR szHelpFile[] =            "ameol2.chm";     /* On-line help file name */
const char NEAR szScriptList[] =       "scripts.ini";       /* Script list */
const char NEAR szNewmesScr[] =           "newmes.scr";     /* Main CIX script */
const char NEAR szModemsIni[] =        "modems.ini";        /* Modems configuration file */
const char NEAR szConnectLog[] =       "connect.log";       /* Connection log file name */
const char NEAR szUsersLstFile[] =        "users.lst";      /* Raw CIX users list */
const char NEAR szCixLstFile[] =       "cix.lst";           /* Raw CIX conferences list */
const char NEAR szUsenetLstFile[] =       "usenet.lst";     /* Raw CIX usenet list */
const char NEAR szMailDirFile[] =         "mail.lst";       /* Mail directory list file */
const char NEAR szCixInitScr[] =       "login.scr";         /* CIX initialisation script file */
const char NEAR szCixTermScr[] =       "exit.scr";          /* CIX termination script file */
const char NEAR szScratchPad[] =       "scratchp.dat";      /* Scratchpad file name */
const char NEAR szScratchPadRcv[] =       "scratchp.rcv";   /* Recovery scratchpad file name */
const char NEAR szUsenetScratchPad[] =    "newsnet.dat";    /* Usenet scratchpad file name */
const char NEAR szUsenetRcvScratchPad[] = "newsnet.rcv";    /* Usenet recovery scratchpad file name */
const char NEAR szLicenceRTF[] =       "licence.rtf";       /* Licence agreement !!SM!!*/
const char NEAR szLicence[] =          "licence.txt";       /* Licence agreement */
const char NEAR szConfigFile[] =       "custom.ini";        /* Custom configuration file */

const char szSSCELib[] =               "ssce32.dll";        /* Name of spell check DLL */
const char szMapiLib[] =               "mapi32.dll";        /* 32-bit MAPI */

char NEAR szAppName[ 64 ] = "Ameol";                        /* Application title */
char NEAR szAppLogo[ 64 ] = "";                             /* Application logo bitmap */
char NEAR szSplashLogo[ 64 ] = "";                          /* Splash logo bitmap */
char NEAR szSplashRGB[ 12 ] = "0,0,132";                    /* Splash text RGB values */
char NEAR szCIXPhoneNumber[ 64 ] = "08453502999";           /* CIX Phone number */
char NEAR szCountryCode[ 5 ] = "44";                        /* TAPI country code number */
char NEAR szAreaCode[ 10 ] = "0845";                        /* TAPI area code number */
char NEAR szPhoneNumber[ 30 ] = "3502999";                  /* TAPI phone number */

const char NEAR szMsgViewerWnd[] = "Message Viewer";        /* Section for message viewer */
const char NEAR szSettings[] = "Settings";                  /* Section for settings */
const char NEAR szWWW[] = "WWW";                            /* Section for WWW */
const char NEAR szColours[] = "Colours";                    /* Section for colours */
const char NEAR szPrinter[] = "Printer";                    /* Section for printer settings */
const char NEAR szSpelling[] = "Spelling";                  /* Section for spell checker */
const char NEAR szConnect[] = "Connect";                    /* Section for custom connections */
const char NEAR szFind[] = "Find";                          /* Section for search strings */
const char NEAR szFonts[] = "Fonts";                        /* Section for font definitions */
const char NEAR szFileFind[] = "File Find";                 /* Section for flist find strings */
const char NEAR szPreferences[] = "Preferences";            /* Section for preferences */
const char NEAR szNews[] = "News";                          /* Section for news */
const char NEAR szWindows[] = "Windows";                    /* Section for Windows */
const char NEAR szMail[] = "Mail";                          /* Section for mail */
const char NEAR szDirects[] = "Directories";                /* Section for directories */
const char NEAR szToolbar[] = "Toolbar";                    /* Section for toolbar */
const char NEAR szCIX[] = "CIX";                            /* Section for CIX */
const char NEAR szCIXIP[] = "Internet";                     /* Section for CIX Internet */
const char NEAR szDebug[] = "Debug";                        /* Section for debug information */
const char NEAR szAddons[] = "Addons";                      /* Section for addons */
const char NEAR szInfo[] = "User Info";                     /* Section for user info */
const char NEAR szTips[] = "Tips";                          /* Section for user info */
const char NEAR szKeyboard[] = "KeyMap";                    /* Section for keyboard configuration */
const char NEAR szCustomBitmaps[] = "Custom Bitmaps";       /* Section for custom bitmaps */
const char NEAR szImport[] = "Import";                      /* Section for import */
const char NEAR szExport[] = "Export";                      /* Section for export */
const char NEAR szTerminals[] = "Terminals";                /* Section for terminal connections */
const char NEAR szDatabases[] = "Databases";                /* Section for databases */
const char NEAR szConnections[] = "Connection Cards";       /* Section for Connection Cards */
const char NEAR szScrExportFiles[] = "Export Files";        /* Section for exported files */
const char NEAR szScrExportAuthors[] = "Export Authors";    /* Section for exported authors */
const char NEAR szExtensions[] = "Extensions";              /* Section for directory associations */
const char NEAR szDowntime[] = "Downtime";                  /* Section for CIX downtimes */
const char NEAR szBlinkman[] = "Blink Manager";             /* Section for Blink Manager */
const char NEAR szSMTPServers[] = "SMTP Servers";           /* Section for SMTP Servers */
const char NEAR szScheduler[] = "Scheduled Items";          /* Section for Scheduler */
const char NEAR szNewsServers[] = "News Servers";           /* Section for News servers */
const char NEAR szIdentities[] = "Identities";              /* Section for Identities */
const char NEAR szConfig[] = "Custom Setup";                /* Section for configuration */

const char NEAR szFrameWndClass[] =    "amctl_frame";       /* Frame window class name */
const char NEAR szClientWndClass[] =   "amctl_mdiclient";   /* Client window class name */

char szNewsgroup[ 80 ];                               /* Initial newsgroup to open */
char szMailtoAddress[ 80 ];                           /* E-mail address to e-mail */
char szCixLink[ 80 ];                                 /* CIX hotlink */

BOOL fInitNewsgroup = FALSE;                          /* Open initial newsgroup */
BOOL fMailto = FALSE;                                 /* Create a mail message */
BOOL fCixLink = FALSE;                                /* CIX hotlink */

/* Command line switch array.
 */
#define  MAX_CMDSWITCHLEN     8

#define  SWI_INTEGER          0
#define  SWI_BOOL             1
#define  SWI_STRING           2
#define  SWI_LONGINT          3

struct tagSWI {
   char szCmdSwitch[ MAX_CMDSWITCHLEN + 1 ];
   WORD wType;
   LPVOID lpBool;
   LPSTR lpStr;
   WORD wMax;
} FAR swi[] = {
   {  "account",  SWI_INTEGER,   &nAccountType,    NULL,                0     },
   {  "auto",     SWI_STRING,    &fAutoConnect,    szAutoConnectMode,   63    },
   {  "basic",    SWI_BOOL,      &fBasicMode,      NULL,                0     },
   {  "cix",      SWI_STRING,    &fCixLink,        szCixLink,           80    },
   {  "cixfile",  SWI_STRING,    &fCixLink,        szCixLink,           80    },
   {  "compat",   SWI_INTEGER,   &fCompatFlag,     NULL,                0     },
   {  "config",   SWI_BOOL,      &fSetup,          NULL,                0     },
   {  "dns",      SWI_BOOL,      &fDNSCache,       NULL,                0     },
   {  "debug2",   SWI_BOOL,      &fDebugMode,      NULL,                0     },
   {  "dict",     SWI_STRING,    NULL,             szDictLoadMode,      11    },
   {  "first",    SWI_BOOL,      &fShowFirst,      NULL,                0     },
   {  "isis",     SWI_BOOL,      &fOldLogo,        NULL,                0     },
   {  "lognntp",  SWI_BOOL,      &fLogNNTP,        NULL,                0     },
   {  "logpop3",  SWI_BOOL,      &fLogPOP3,        NULL,                0     },
   {  "logsmtp",  SWI_BOOL,      &fLogSMTP,        NULL,                0     },
   {  "mailto",   SWI_STRING,    &fMailto,         szMailtoAddress,     80    },
   {  "news",     SWI_STRING,    &fInitNewsgroup,  szNewsgroup,         80    },
   {  "next",     SWI_BOOL,      &fShowNext,       NULL,                0     },
   {  "noadds",   SWI_BOOL,      &fNoAddons,       NULL,                0     },
   {  "nocheck",  SWI_BOOL,      &fNoCheck,        NULL,                0     },
   {  "nocmn",    SWI_BOOL,      &fNoCMN,          NULL,                0     },
   {  "noexit",   SWI_BOOL,      &fNoExit,         NULL,                0     },
   {  "nologo",   SWI_BOOL,      &fNoLogo,         NULL,                0     },
   {  "noolt",    SWI_BOOL,      &fNoOLT,          NULL,                0     },
   {  "online",   SWI_BOOL,      &fStartOnline,    NULL,                0     },
   {  "purge",    SWI_STRING,    &fAutoPurge,      szAutoPurge,         200   },
   {  "quit",     SWI_BOOL,      &fQuitAfterConnect,  NULL,             0     },
   {  "sageset",  SWI_BOOL,      &fSageSettings,      NULL,             0     },
   {  "setup",    SWI_BOOL,      &fSetup,          NULL,                0     },
   {  "lite",     SWI_BOOL,      &fUseINIfile,     NULL,                0     }, // !!SM!! 2.56.2051
   {  "u3",       SWI_BOOL,      &fUseU3,          NULL,                0     }, // !!SM!! 2.56.2053
   {  "kill",     SWI_BOOL,      &fKillRunning,    NULL,                0     }, // !!SM!! 2.56.2053
   
   {  "yia",      SWI_BOOL,      &fYIA,            NULL,                0     },
#ifdef _DEBUG
   {  "traced",   SWI_BOOL,      &fTraceOutputDebug,  NULL,                0     },
   {  "tracef",   SWI_BOOL,      &fTraceLogFile,      NULL,                0     },
#endif
   {  "usenet",   SWI_STRING,    NULL,             szUsenetGateway,     9     }
   };

#define  MAX_CMDSWITCHES   (sizeof(swi)/sizeof(swi[0]))

/* Button bitmap arrays. One for small buttons and
 * another for large 'uns.
 */
static int nNormalButtonArray[] = {
   IDB_BITMAP1, IDB_BITMAP2, IDB_BITMAP3, IDB_BITMAP4,
   IDB_BITMAP5, IDB_BITMAP6, IDB_BITMAP7, IDB_BITMAP8,
   IDB_BITMAP9, IDB_BITMAP10, IDB_BITMAP11, IDB_BITMAP12
   };

static int nLargeButtonArray[] = {
   IDB_BIGBITMAP1, IDB_BIGBITMAP2, IDB_BIGBITMAP3, IDB_BIGBITMAP4,
   IDB_BIGBITMAP5, IDB_BIGBITMAP6, IDB_BIGBITMAP7, IDB_BIGBITMAP8,
   IDB_BIGBITMAP9, IDB_BIGBITMAP10, IDB_BIGBITMAP11, IDB_BIGBITMAP12
   };

static int nNewNormalButtonArray[] = {
   IDB_TBS2001, IDB_TBS2002, IDB_TBS2003, IDB_TBS2004,
   IDB_TBS2005, IDB_TBS2006, IDB_TBS2007, IDB_TBS2008,
   IDB_TBS2009, IDB_TBS2010, IDB_TBS2011, IDB_TBS2012
   };

static int nNewLargeButtonArray[] = {
   IDB_TBL2001, IDB_TBL2002, IDB_TBL2003, IDB_TBL2004,
   IDB_TBL2005, IDB_TBL2006, IDB_TBL2007, IDB_TBL2008,
   IDB_TBL2009, IDB_TBL2010, IDB_TBL2011, IDB_TBL2012
   };

#define cButtonArray (sizeof(nNormalButtonArray)/sizeof(nNormalButtonArray[0]))

/* The following array is used by the MenuHelp() function to display help
 * on the status bar when a popup menu is selected. The first elements of
 * the array is filled with the handle of the popup menu as soon as the
 * menu has been loaded.
 */
typedef struct tagPOPUPID {
   HMENU hpopupMenu;
   WORD wID;
} POPUPID;

static POPUPID wMainPopupIDs[] = {
   { 0,     IDS_FILEMENU      },       /* File menu */
   { 0,     IDS_EDITMENU      },       /* Edit menu */
   { 0,     IDS_MAILMENU      },       /* Mail menu */
   { 0,     IDS_NEWSMENU      },       /* News menu */
   { 0,     IDS_VIEWMENU      },       /* View menu */
   { 0,     IDS_TOPICMENU     },       /* Topic menu */
   { 0,     IDS_MESSAGEMENU      },       /* Message menu */
   { 0,     IDS_SETTINGSMENU  },       /* Tools menu */
   { 0,     IDS_WINDOWMENU    },       /* Window menu */
   { 0,     IDS_HELPMENU      },       /* Help menu */
   { 0,     0              }        /* Marks the end of the list */
   };

/* Some macros to save my fingers...
 */
#define  HK_ALT         (HOTKEYF_ALT)
#define  HK_SHIFT    (HOTKEYF_SHIFT)
#define  HK_CTRL        (HOTKEYF_CONTROL)
#define  HK_CTRLSHIFT   (HOTKEYF_SHIFT|HOTKEYF_CONTROL)
#define  MIR            MAKEINTRESOURCE

static MENUSELECTINFO FAR wIDs[] = {
   { "FileImport",               IDM_IMPORT,             MIR(IDS_IMPORT),           0,             MIR(IDS_TT_IMPORT),              TB_IMPORT,              0,                         WIN_ALL,                      CAT_FILE_MENU,    NSCHF_NONE },
   { "FileStopImport",           IDM_STOPIMPORT,            MIR(IDS_STOPIMPORT),       IDS_X_STOPIMPORT, MIR(IDS_TT_STOPIMPORT),          TB_STOPIMPORT,          0,                         WIN_ALL,                      CAT_FILE_MENU,    NSCHF_NONE },
   { "FileExport",               IDM_EXPORT,             MIR(IDS_EXPORT),           0,             MIR(IDS_TT_EXPORT),              TB_EXPORT,              0,                         WIN_ALL,                      CAT_FILE_MENU,    NSCHF_NONE },
   { "FileQuickExport",       IDM_QUICKEXPORT,        MIR(IDS_QUICKEXPORT),         0,             MIR(IDS_TT_QUICKEXPORT),         TB_QUICKEXPORT,            MAKEKEY( HK_CTRL, 'E' ),         WIN_ALL,                      CAT_FILE_MENU,    NSCHF_NONE },
   { "FileEventLog",          IDM_EVENTLOG,           MIR(IDS_EVENTLOG),            0,             MIR(IDS_TT_EVENTLOG),            TB_EVENTLOG,            MAKEKEY( HK_CTRL, 'L' ),         WIN_ALL,                      CAT_FILE_MENU,    NSCHF_NONE },
   { "FileSearch",               IDM_SEARCH,             MIR(IDS_SEARCH),           0,             MIR(IDS_TT_SEARCH),              TB_SEARCH,              MAKEKEY( 0, VK_F3 ),          WIN_ALL,                      CAT_FILE_MENU,    NSCHF_NONE },
   { "FileQuickPrint",           IDM_QUICKPRINT,            MIR(IDS_QUICKPRINT),       IDS_X_PAGESETUP,  MIR(IDS_TT_QUICKPRINT),          TB_PRINT,               MAKEKEY( HK_ALT, 'P' ),          WIN_ALL,                      CAT_FILE_MENU,    NSCHF_NONE },
   { "FilePrint",             IDM_PRINT,              MIR(IDS_PRINT),               IDS_X_PAGESETUP,  MIR(IDS_TT_PRINT),               TB_PRINT,               MAKEKEY( HK_CTRL, 'P' ),         WIN_ALL,                         CAT_FILE_MENU,    NSCHF_NONE },
   { "FilePageSetup",            IDM_PAGESETUP,          MIR(IDS_PAGESETUP),           IDS_X_PAGESETUP,  MIR(IDS_TT_PAGESETUP),           TB_PAGESETUP,           0,                         WIN_ALL,                      CAT_FILE_MENU,    NSCHF_NONE },
   { "FilePrintSetup",           IDM_PRINTSETUP,            MIR(IDS_PRINTSETUP),       IDS_X_PAGESETUP,  MIR(IDS_TT_PRINTSETUP),          TB_PRINTSETUP,          0,                         WIN_ALL,                      CAT_FILE_MENU,    NSCHF_NONE },
   { "FileSendReceiveAll",       IDM_SENDRECEIVEALL,        MIR(IDS_SENDRECEIVEALL),      IDS_X_BLINK,      MIR(IDS_TT_SENDRECEIVEALL),         TB_BLINK,               MAKEKEY( HK_CTRL, 'T' ),         WIN_ALL,                      CAT_FILE_MENU,    NSCHF_NONE },
   { "FileCustomBlink",       IDM_CUSTOMBLINK,        MIR(IDS_CUSTOMBLINK),         IDS_X_BLINK,      MIR(IDS_TT_CUSTOMBLINK),         TB_CUSTOMBLINK,            MAKEKEY( HK_CTRL, 'U' ),         WIN_ALL,                      CAT_FILE_MENU,    NSCHF_CANSCHEDULE },
   { "FileStop",              IDM_STOP,               MIR(IDS_STOP),             IDS_X_STOP,       MIR(IDS_TT_STOP),             TB_STOP,             0,                         WIN_ALL,                      CAT_FILE_MENU,    NSCHF_CANSCHEDULE },
   { "FileTerminal",          IDM_TERMINAL,           MIR(IDS_TERMINAL),            0,             MIR(IDS_TT_TERMINAL),            TB_TERMINAL,            0,                         WIN_ALL,                      CAT_FILE_MENU,    NSCHF_NONE },
   { "FileSend",              IDM_MAPISEND,           MIR(IDS_MAPISEND),            IDS_X_NOMSG,      MIR(IDS_TT_MAPISEND),            TB_MAPISEND,            0,                         WIN_THREAD|WIN_MESSAGE,             CAT_FILE_MENU,    NSCHF_NONE },
   { "FileExit",              IDM_EXIT,               MIR(IDS_EXIT),             0,             MIR(IDS_TT_EXIT),             TB_EXIT,             MAKEKEY( HK_ALT, VK_F4 ),        WIN_ALL,                      CAT_FILE_MENU,    NSCHF_CANSCHEDULE },
   { "EditUndo",              IDM_UNDO,               MIR(IDS_UNDO),             IDS_X_UNDO,       MIR(IDS_TT_UNDO),             TB_UNDO,             MAKEKEY( HK_CTRL, 'Z' ),         WIN_EDITS,                       CAT_EDIT_MENU,    NSCHF_NONE },
   { "EditRedo",              IDM_REDO,               MIR(IDS_REDO),             IDS_X_REDO,       MIR(IDS_TT_REDO),             TB_REDO,             0,                         WIN_EDITS,                       CAT_EDIT_MENU,    NSCHF_NONE }, // 20060302 - 2.56.2049.08
   { "EditCut",               IDM_CUT,             MIR(IDS_CUT),              IDS_X_CUT,        MIR(IDS_TT_CUT),              TB_CUT,                 MAKEKEY( HK_CTRL, 'X' ),         WIN_EDITS,                       CAT_EDIT_MENU,    NSCHF_NONE },
   { "EditCopy",              IDM_COPY,               MIR(IDS_COPY),             IDS_X_COPY,       MIR(IDS_TT_COPY),             TB_COPY,             MAKEKEY( HK_CTRL, 'C' ),         WIN_ALL,                      CAT_EDIT_MENU,    NSCHF_NONE },
   { "EditCopyAll",           IDM_COPYALL,            MIR(IDS_COPYALL),          IDS_X_COPYALL,    MIR(IDS_TT_COPYALL),          TB_COPYALL,             0,                         WIN_ALL,                      CAT_EDIT_MENU,    NSCHF_NONE },
   { "EditCopyCIXLink",       IDM_COPYCIXLINK,        MIR(IDS_COPYCIXLINK),         IDS_X_COPYCIXLINK,   MIR(IDS_TT_COPYCIXLINK),         TB_COPYCIXLINK,            0,                         WIN_ALL,                         CAT_EDIT_MENU,    NSCHF_NONE },
   { "EditBrowseLink",           IDM_BROWSE,             MIR(IDS_BROWSE),           IDS_X_BROWSELINK, MIR(IDS_TT_BROWSE),              TB_BROWSE,              0,                         WIN_ALL/*|WIN_NOMENU*/,             CAT_EDIT_MENU,    NSCHF_NONE },
   { "EditPaste",             IDM_PASTE,              MIR(IDS_PASTE),               IDS_X_PASTE,      MIR(IDS_TT_PASTE),               TB_PASTE,               MAKEKEY( HK_CTRL, 'V' ),         WIN_ALL,                      CAT_EDIT_MENU,    NSCHF_NONE },
   { "EditClear",             IDM_CLEAR,              MIR(IDS_CLEAR),               IDS_X_CLEAR,      MIR(IDS_TT_CLEAR),               TB_CLEAR,               0,                         WIN_ALL,                      CAT_EDIT_MENU,    NSCHF_NONE },
   { "EditSelectAll",            IDM_SELECTALL,          MIR(IDS_SELECTALL),           IDS_X_SELECTALL,  MIR(IDS_TT_SELECTALL),           TB_SELECTALL,           0,                         WIN_ALL,                      CAT_EDIT_MENU,    NSCHF_NONE },
   { "EditFind",              IDM_FIND,               MIR(IDS_FIND),             IDS_X_FIND,       MIR(IDS_TT_FIND),             TB_FIND,             MAKEKEY( HK_CTRL, 'F' ),         WIN_EDITS,                       CAT_EDIT_MENU,    NSCHF_NONE },
   { "EditReplace",           IDM_REPLACE,            MIR(IDS_REPLACE),          IDS_X_FIND,       MIR(IDS_TT_REPLACE),          TB_REPLACE,             MAKEKEY( HK_CTRL, 'H' ),         WIN_EDITS,                       CAT_EDIT_MENU,    NSCHF_NONE },
   { "EditSpellCheck",           IDM_SPELLCHECK,            MIR(IDS_SPELLCHECK),       IDS_X_NOSPELL,    MIR(IDS_TT_SPELLCHECK),          TB_SPELLCHECK,          MAKEKEY( 0, VK_F7 ),          WIN_EDITS,                       CAT_EDIT_MENU,    NSCHF_NONE },
   { "EditQuote",             IDM_QUOTE,              MIR(IDS_QUOTE),               IDS_X_COPY,       MIR(IDS_TT_QUOTE),               TB_QUOTE,               MAKEKEY( HK_CTRL, 'Q' ),         WIN_ALL,                      CAT_EDIT_MENU,    NSCHF_NONE },
   { "EditRot13",             IDM_ROT13,              MIR(IDS_ROT13),               IDS_X_ROT13,      MIR(IDS_TT_ROT13),               TB_ROT13,               0,                         WIN_THREAD|WIN_MESSAGE,             CAT_EDIT_MENU,    NSCHF_NONE },
   { "EditInsertFile",           IDM_INSERTFILE,            MIR(IDS_INSERTFILE),       IDS_X_FIND,       MIR(IDS_TT_INSERTFILE),          TB_INSERTFILE,          0,                         WIN_THREAD|WIN_MESSAGE,             CAT_EDIT_MENU,    NSCHF_NONE },
   { "ViewToolbar",           IDM_TOOLBAR,            MIR(IDS_TOOLBAR),          0,             MIR(IDS_TT_TOOLBAR),          TB_TOOLBAR,             0,                         WIN_ALL,                      CAT_VIEW_MENU,    NSCHF_NONE },
   { "ViewStatusbar",            IDM_STATUSBAR,          MIR(IDS_STATUSBAR),           0,             MIR(IDS_TT_STATUSBAR),           TB_STATUSBAR,           0,                         WIN_ALL,                      CAT_VIEW_MENU,    NSCHF_NONE },
   { "ViewOutBasket",            IDM_OUTBASKET,          MIR(IDS_OUTBASKET),           0,             MIR(IDS_TT_OUTBASKET),           TB_OUTBASKET,           0,                         WIN_ALL,                      CAT_VIEW_MENU,    NSCHF_NONE },
   { "ViewInBasket",          IDM_INBASKET,           MIR(IDS_INBASKET),            0,             MIR(IDS_TT_INBASKET),            TB_INBASKET,            MAKEKEY( HK_CTRL, 'J' ),         WIN_ALL,                      CAT_VIEW_MENU,    NSCHF_NONE },
   { "ViewInBasket2",            IDM_INBASKET2,          MIR(IDS_INBASKET2),           0,             MIR(IDS_TT_INBASKET2),           TB_INBASKET,            MAKEKEY( 0, 'J' ),               WIN_THREAD|WIN_MESSAGE,             CAT_VIEW_MENU,    NSCHF_NONE },
   { "ViewReference",            IDM_SORT_REFERENCE,        MIR(IDS_SORT_REFERENCE),      IDS_X_NOMSG,      MIR(IDS_TT_SORT_REFERENCE),         TB_SORT_REFERENCE,         0,                         WIN_THREAD|WIN_MESSAGE,             CAT_VIEW_MENU,    NSCHF_NONE },
   { "ViewRoots",             IDM_SORT_ROOTS,            MIR(IDS_SORT_ROOTS),       IDS_X_NOMSG,      MIR(IDS_TT_SORT_ROOTS),          TB_SORT_ROOTS,          0,                         WIN_THREAD|WIN_MESSAGE,             CAT_VIEW_MENU,    NSCHF_NONE },
   { "ViewNumber",               IDM_SORT_MSGNUM,        MIR(IDS_SORT_MSGNUM),         IDS_X_NOMSG,      MIR(IDS_TT_SORT_MSGNUM),         TB_SORT_MSGNUM,            0,                         WIN_THREAD|WIN_MESSAGE,             CAT_VIEW_MENU,    NSCHF_NONE },
   { "ViewSender",               IDM_SORT_SENDER,        MIR(IDS_SORT_SENDER),         IDS_X_NOMSG,      MIR(IDS_TT_SORT_SENDER),         TB_SORT_SENDER,            0,                         WIN_THREAD|WIN_MESSAGE,             CAT_VIEW_MENU,    NSCHF_NONE },
   { "ViewDateTime",          IDM_SORT_DATETIME,         MIR(IDS_SORT_DATETIME),       IDS_X_NOMSG,      MIR(IDS_TT_SORT_DATETIME),       TB_SORT_DATETIME,       0,                         WIN_THREAD|WIN_MESSAGE,             CAT_VIEW_MENU,    NSCHF_NONE },
   { "ViewSize",              IDM_SORT_MSGSIZE,       MIR(IDS_SORT_MSGSIZE),        IDS_X_NOMSG,      MIR(IDS_TT_SORT_MSGSIZE),        TB_SORT_MSGSIZE,        0,                         WIN_THREAD|WIN_MESSAGE,             CAT_VIEW_MENU,    NSCHF_NONE },
   { "ViewSubject",           IDM_SORT_SUBJECT,       MIR(IDS_SORT_SUBJECT),        IDS_X_NOMSG,      MIR(IDS_TT_SORT_SUBJECT),        TB_SORT_SUBJECT,        0,                         WIN_THREAD|WIN_MESSAGE,             CAT_VIEW_MENU,    NSCHF_NONE },
   { "ViewAscending",            IDM_ASCENDING,          MIR(IDS_ASCENDING),           IDS_X_NOMSG,      MIR(IDS_TT_ASCENDING),           TB_ASCENDING,           0,                         WIN_THREAD|WIN_MESSAGE,             CAT_VIEW_MENU,    NSCHF_NONE },
   { "ViewInBrowser",            IDM_VIEWINBROWSER,         MIR(IDS_VIEWINBROWSER),       IDS_X_NOMSG,      MIR(IDS_TT_VIEWINBROWSER),       TB_VIEWINBROWSER,       MAKEKEY( 0, VK_F8 ),          WIN_THREAD|WIN_MESSAGE,             CAT_VIEW_MENU,    NSCHF_NONE },
   { "ShowNumber",               IDM_SHOW_MSGNUM,        MIR(IDS_SHOW_MSGNUM),         IDS_X_NOMSG,      MIR(IDS_TT_SHOW_MSGNUM),         TB_SHOW_MSGNUM,            0,                         WIN_THREAD|WIN_MESSAGE,             CAT_VIEW_MENU,    NSCHF_NONE },
   { "ShowSender",               IDM_SHOW_SENDER,        MIR(IDS_SHOW_SENDER),         IDS_X_NOMSG,      MIR(IDS_TT_SHOW_SENDER),         TB_SHOW_SENDER,            0,                         WIN_THREAD|WIN_MESSAGE,             CAT_VIEW_MENU,    NSCHF_NONE },
   { "ShowDateTime",          IDM_SHOW_DATETIME,         MIR(IDS_SHOW_DATETIME),       IDS_X_NOMSG,      MIR(IDS_TT_SHOW_DATETIME),       TB_SHOW_DATETIME,       0,                         WIN_THREAD|WIN_MESSAGE,             CAT_VIEW_MENU,    NSCHF_NONE },
   { "ShowSize",              IDM_SHOW_MSGSIZE,       MIR(IDS_SHOW_MSGSIZE),        IDS_X_NOMSG,      MIR(IDS_TT_SHOW_MSGSIZE),        TB_SHOW_MSGSIZE,        0,                         WIN_THREAD|WIN_MESSAGE,             CAT_VIEW_MENU,    NSCHF_NONE },
   { "ShowSubject",           IDM_SHOW_SUBJECT,       MIR(IDS_SHOW_SUBJECT),        IDS_X_NOMSG,      MIR(IDS_TT_SHOW_SUBJECT),        TB_SHOW_SUBJECT,        0,                         WIN_THREAD|WIN_MESSAGE,             CAT_VIEW_MENU,    NSCHF_NONE },
   { "ViewNext",              IDM_NEXT,               MIR(IDS_NEXT),             0,             MIR(IDS_TT_NEXT),             TB_NEXT,             MAKEKEY( HK_CTRL, 'N' ),         WIN_ALL,                      CAT_VIEW_MENU,    NSCHF_NONE },
   { "ViewNextPriority",         IDM_NEXTPRIORITY,       MIR(IDS_NEXTPRIORITY),        0,             MIR(IDS_TT_NEXTPRIORITY),        TB_NEXTPRIORITY,        MAKEKEY( 0, VK_F5 ),          WIN_ALL,                      CAT_VIEW_MENU,    NSCHF_NONE },
   { "ViewNextMarked",           IDM_NEXTMARKED,            MIR(IDS_NEXTMARKED),       0,             MIR(IDS_TT_NEXTMARKED),          TB_NEXTMARKED,          MAKEKEY( 0, VK_F9 ),          WIN_ALL,                      CAT_VIEW_MENU,    NSCHF_NONE },
   { "ViewPrevMarked",           IDM_PREVMARKED,            MIR(IDS_PREVMARKED),       0,             MIR(IDS_TT_PREVMARKED),          TB_PREVMARKED,          MAKEKEY( HK_SHIFT, VK_F9 ),         WIN_ALL,                      CAT_VIEW_MENU,    NSCHF_NONE },
   { "ViewFixed",             IDM_FIXEDFONT,          MIR(IDS_FIXEDFONT),           IDS_X_NOMSG,      MIR(IDS_TT_FIXEDFONT),           TB_FIXEDFONT,           0,                         WIN_THREAD|WIN_MESSAGE,             CAT_VIEW_MENU,    NSCHF_NONE },
   { "ViewWordWrap",          IDM_WORDWRAP,           MIR(IDS_WORDWRAP),            IDS_X_NOMSG,      MIR(IDS_TT_WORDWRAP),            TB_WORDWRAP,            MAKEKEY( HK_CTRL, 'W' ),         WIN_ALL,                      CAT_VIEW_MENU,    NSCHF_NONE },
   { "ViewMessageStyles",        IDM_MSGSTYLES,          MIR(IDS_MSGSTYLES),           IDS_X_NOMSG,      MIR(IDS_TT_MSGSTYLES),           TB_MSGSTYLES,           0,                         WIN_THREAD|WIN_MESSAGE,             CAT_VIEW_MENU,    NSCHF_NONE },
   { "ViewHotLinks",          IDM_HOTLINKS,           MIR(IDS_HOTLINKS),            IDS_X_NOMSG,      MIR(IDS_TT_HOTLINKS),            TB_HOTLINKS,            0,                         WIN_THREAD|WIN_MESSAGE,             CAT_VIEW_MENU,    NSCHF_NONE },
   { "MultiLineHotLinks",        IDM_MLHOTLINKS,            MIR(IDS_MLHOTLINKS),       IDS_X_NOMSG,      MIR(IDS_TT_MLHOTLINKS),          TB_MLHOTLINKS,          0,                         WIN_THREAD|WIN_MESSAGE,             CAT_VIEW_MENU,    NSCHF_NONE },
   { "FolderNewCategory",        IDM_NEWCATEGORY,        MIR(IDS_NEWCATEGORY),         0,             MIR(IDS_TT_NEWCATEGORY),         TB_NEWCATEGORY,            0,                         WIN_ALL,                      CAT_TOPIC_MENU,      NSCHF_NONE },
   { "FolderNew",             IDM_NEWFOLDER,          MIR(IDS_NEWFOLDER),           0,             MIR(IDS_TT_NEWFOLDER),           TB_NEWFOLDER,           0,                         WIN_ALL,                         CAT_TOPIC_MENU,      NSCHF_NONE },
   { "FolderNewLocal",           IDM_NEWLOCALTOPIC,         MIR(IDS_NEWLOCALTOPIC),       0,             MIR(IDS_TT_NEWLOCALTOPIC),       TB_NEWLOCALTOPIC,       0,                         WIN_ALL,                         CAT_TOPIC_MENU,      NSCHF_NONE },
   { "FolderDelete",          IDM_DELETE,             MIR(IDS_DELETE),           IDS_X_INBASKET,      MIR(IDS_TT_DELETE),              TB_DELETE,              0,                         WIN_ALL,                         CAT_TOPIC_MENU,      NSCHF_NONE },
   { "FolderRename",          IDM_RENAME,             MIR(IDS_RENAME),           IDS_X_INBASKET,      MIR(IDS_TT_RENAME),              TB_RENAME,              0,                         WIN_ALL,                         CAT_TOPIC_MENU,      NSCHF_NONE },
   { "FolderProperties",         IDM_PROPERTIES,            MIR(IDS_PROPERTIES),       IDS_X_INBASKET,      MIR(IDS_TT_PROPERTIES),          TB_PROPERTIES,          MAKEKEY( HK_ALT, VK_RETURN ),    WIN_ALL,                         CAT_TOPIC_MENU,      NSCHF_NONE },
   { "FolderPurge",           IDM_PURGE,              MIR(IDS_PURGE),               IDS_X_INBASKET,      MIR(IDS_TT_PURGE),               TB_PURGE,               0,                         WIN_ALL,                         CAT_TOPIC_MENU,      NSCHF_NEEDFOLDER },
   { "FolderCheck",           IDM_CHECK,              MIR(IDS_CHECK),               IDS_X_INBASKET,      MIR(IDS_TT_CHECK),               TB_CHECK,               0,                         WIN_ALL,                         CAT_TOPIC_MENU,      NSCHF_NEEDFOLDER },
   { "FolderSort",               IDM_SORT,               MIR(IDS_SORT),             IDS_X_INBASKET,      MIR(IDS_TT_SORT),             TB_SORT,             0,                         WIN_ALL,                         CAT_TOPIC_MENU,      NSCHF_NONE },
// { "FolderRebuildIndex",       IDM_REBUILD_INDEX,         MIR(IDS_REBUILD_INDEX),       IDS_X_INBASKET,      MIR(IDS_TT_REBUILD_INDEX),       TB_REBUILD_INDEX,       0,                         WIN_ALL,                         CAT_TOPIC_MENU,      NSCHF_NONE },
   { "FolderBuild",           IDM_BUILD,              MIR(IDS_BUILD),               IDS_X_INBASKET,      MIR(IDS_TT_BUILD),               TB_BUILD,               0,                         WIN_ALL,                         CAT_TOPIC_MENU,      NSCHF_NONE },
   { "MarkTopicRead",            IDM_MARKTOPICREAD,         MIR(IDS_MARKTOPICREAD),       IDS_X_NOTOPIC,    MIR(IDS_TT_MARKTOPICREAD),       TB_MARKTOPICREAD,       MAKEKEY( HK_CTRL, 'K' ),         WIN_ALL,                         CAT_TOPIC_MENU,      NSCHF_NONE },
   { "MarkThreadRead",           IDM_MARKTHREADREAD,        MIR(IDS_MARKTHREADREAD),      IDS_X_NOMSG,      MIR(IDS_TT_MARKTHREADREAD),         TB_MARKTHREADREAD,         MAKEKEY( 0, 'Z' ),               WIN_THREAD|WIN_MESSAGE,             CAT_TOPIC_MENU,      NSCHF_NONE },
   { "RethreadArticles",         IDM_RETHREADARTICLES,      MIR(IDS_RETHREADARTICLES),    0,             MIR(IDS_TT_RETHREADARTICLES),    TB_RETHREADARTICLES,    0,                         WIN_THREAD|WIN_MESSAGE,             CAT_TOPIC_MENU,      NSCHF_NONE },
   { "KeepAtTop",             IDM_KEEPATTOP,          MIR(IDS_KEEPATTOP),           IDS_X_INBASKET,      MIR(IDS_TT_KEEPATTOP),           TB_KEEPATTOP,           0,                         WIN_THREAD|WIN_MESSAGE,             CAT_TOPIC_MENU,      NSCHF_NONE },
   { "Say",                IDM_SAY,             MIR(IDS_SAY),              IDS_X_NOSAYCOMMENT,  MIR(IDS_TT_SAY),              TB_SAY,                 MAKEKEY( 0, 'S' ),               WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "Comment",               IDM_COMMENT,            MIR(IDS_COMMENT),          IDS_X_NOSAYCOMMENT,  MIR(IDS_TT_COMMENT),          TB_COMMENT,             MAKEKEY( 0, 'C' ),               WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "Original",              IDM_ORIGINAL,           MIR(IDS_ORIGINAL),            IDS_X_NOMSG,      MIR(IDS_TT_ORIGINAL),            TB_ORIGINAL,            MAKEKEY( 0, 'O' ),               WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "Goto",                  IDM_GOTO,               MIR(IDS_GOTO),             IDS_X_NOMSG,      MIR(IDS_TT_GOTO),             TB_GOTO,             MAKEKEY( 0, 'G' ),               WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "CopyMsg",               IDM_COPYMSG,            MIR(IDS_COPYMSG),          IDS_X_NOMSG,      MIR(IDS_TT_COPYMSG),          TB_COPYMSG,             MAKEKEY( 0, 'Y' ),               WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "MoveMsg",               IDM_MOVEMSG,            MIR(IDS_MOVEMSG),          IDS_X_NOMSG,      MIR(IDS_TT_MOVEMSG),          TB_MOVEMSG,             MAKEKEY( HK_CTRL, 'Y' ),         WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "ForwardMsg",               IDM_FORWARDMSG,            MIR(IDS_FORWARDMSG),       IDS_X_NOMSG,      MIR(IDS_TT_FORWARDMSG),          TB_FORWARDMSG,          MAKEKEY( 0, 'V' ),               WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "ReadMsg",               IDM_READMSG,            MIR(IDS_READMSG),          IDS_X_NOMSG,      MIR(IDS_TT_READMSG),          TB_READMSG,             MAKEKEY( 0, 'R' ),               WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "ReadThread",               IDM_READTHREAD,            MIR(IDS_READTHREAD),       IDS_X_NOMSG,      MIR(IDS_TT_READTHREAD),          TB_READTHREAD,          MAKEKEY( HK_SHIFT, 'R' ),        WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "ReadLock",              IDM_READLOCK,           MIR(IDS_READLOCK),            IDS_X_NOMSG,      MIR(IDS_TT_READLOCK),            TB_READLOCK,            MAKEKEY( 0, 'L' ),               WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "ReadLockThread",           IDM_READLOCKTHREAD,        MIR(IDS_READLOCKTHREAD),      IDS_X_NOMSG,      MIR(IDS_TT_READLOCKTHREAD),         TB_READLOCKTHREAD,         MAKEKEY( HK_SHIFT, 'L' ),        WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "MarkMsg",               IDM_MARKMSG,            MIR(IDS_MARKMSG),          IDS_X_NOMSG,      MIR(IDS_TT_MARKMSG),          TB_MARKMSG,             MAKEKEY( 0, 'M' ),               WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "MarkThread",               IDM_MARKTHREAD,            MIR(IDS_MARKTHREAD),       IDS_X_NOMSG,      MIR(IDS_TT_MARKTHREAD),          TB_MARKTHREAD,          MAKEKEY( HK_SHIFT, 'M' ),        WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "KeepMsg",               IDM_KEEPMSG,            MIR(IDS_KEEPMSG),          IDS_X_NOMSG,      MIR(IDS_TT_KEEPMSG),          TB_KEEPMSG,             MAKEKEY( 0, 'K' ),               WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "KeepThread",               IDM_KEEPTHREAD,            MIR(IDS_KEEPTHREAD),       IDS_X_NOMSG,      MIR(IDS_TT_KEEPTHREAD),          TB_KEEPTHREAD,          MAKEKEY( HK_SHIFT, 'K' ),        WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "DeleteMsg",             IDM_DELETEMSG,          MIR(IDS_DELETEMSG),           IDS_X_NOMSG,      MIR(IDS_TT_DELETEMSG),           TB_DELETEMSG,           MAKEKEY( 0, 'D' ),               WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "KillMsg",               IDM_KILLMSG,            MIR(IDS_KILLMSG),          IDS_X_NOMSG,      MIR(IDS_TT_KILLMSG),          TB_KILLMSG,             MAKEKEY( 0, VK_DELETE ),         WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "DeleteThread",          IDM_DELETETHREAD,       MIR(IDS_DELETETHREAD),        IDS_X_NOMSG,      MIR(IDS_TT_DELETETHREAD),        TB_DELETETHREAD,        MAKEKEY( HK_SHIFT, 'D' ),        WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "KillThread",               IDM_KILLTHREAD,            MIR(IDS_KILLTHREAD),       IDS_X_NOMSG,      MIR(IDS_TT_KILLTHREAD),          TB_KILLTHREAD,          MAKEKEY( HK_SHIFT, VK_DELETE ),     WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "Ignore",                IDM_IGNORE,             MIR(IDS_IGNORE),           IDS_X_NOMSG,      MIR(IDS_TT_IGNORE),              TB_IGNORE,              MAKEKEY( 0, 'I' ),               WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "IgnoreThread",          IDM_IGNORETHREAD,       MIR(IDS_IGNORETHREAD),        IDS_X_NOMSG,      MIR(IDS_TT_IGNORETHREAD),        TB_IGNORETHREAD,        MAKEKEY( HK_SHIFT, 'I' ),        WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "Priority",              IDM_PRIORITY,           MIR(IDS_PRIORITY),            IDS_X_NOMSG,      MIR(IDS_TT_PRIORITY),            TB_PRIORITY,            MAKEKEY( 0, 'P' ),               WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "PriorityThread",           IDM_PRIORITYTHREAD,        MIR(IDS_PRIORITYTHREAD),      IDS_X_NOMSG,      MIR(IDS_TT_PRIORITYTHREAD),         TB_PRIORITYTHREAD,         MAKEKEY( HK_SHIFT, 'P' ),        WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "MarkTagged",               IDM_MARKTAGGED,            MIR(IDS_MARKTAGGED),       IDS_X_NONEWS,     MIR(IDS_TT_MARKTAGGED),          TB_MARKTAGGED,          MAKEKEY( 0, 'T' ),               WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "MarkThreadTagged",         IDM_MARKTHREADTAGGED,      MIR(IDS_MARKTHREADTAGGED),    IDS_X_NONEWS,     MIR(IDS_TT_MARKTHREADTAGGED),    TB_MARKTHREADTAGGED,    MAKEKEY( HK_SHIFT, 'T' ),        WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "Withdraw",              IDM_WITHDRAW,           MIR(IDS_WITHDRAW),            IDS_X_NOMSG,      MIR(IDS_TT_WITHDRAW),            TB_WITHDRAW,            MAKEKEY( 0, 'W' ),               WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "Watch",                 IDM_WATCH,              MIR(IDS_WATCH),               IDS_X_NONEWS,     MIR(IDS_TT_WATCH),               TB_WATCH,               MAKEKEY( 0, 'H' ),               WIN_THREAD|WIN_MESSAGE,             CAT_MESSAGE_MENU, NSCHF_NONE },
   { "Admin",                 IDM_ADMIN,              MIR(IDS_ADMIN),               IDS_X_ADMIN,      MIR(IDS_TT_ADMIN),               TB_ADMIN,               0,                         WIN_THREAD|WIN_MESSAGE,             CAT_SETTINGS_MENU,   NSCHF_NONE },
   { "Filters",               IDM_FILTERS,            MIR(IDS_FILTERS),          0,             MIR(IDS_TT_FILTERS),          TB_FILTERS,             0,                         WIN_ALL,                         CAT_SETTINGS_MENU,   NSCHF_NONE },
   { "Connections",           IDM_COMMUNICATIONS,        MIR(IDS_COMMUNICATIONS),      0,             MIR(IDS_TT_COMMUNICATIONS),         TB_COMMUNICATIONS,         0,                         WIN_ALL,                         CAT_SETTINGS_MENU,   NSCHF_NONE },
   { "Preferences",           IDM_PREFERENCES,        MIR(IDS_PREFERENCES),         0,             MIR(IDS_TT_PREFERENCES),         TB_PREFERENCES,            0,                         WIN_ALL,                         CAT_SETTINGS_MENU,   NSCHF_NONE },
   { "Directories",           IDM_DIRECTORIES,        MIR(IDS_DIRECTORIES),         0,             MIR(IDS_TT_DIRECTORIES),         TB_DIRECTORIES,            0,                         WIN_ALL,                         CAT_SETTINGS_MENU,   NSCHF_NONE },
   { "PurgeSettings",            IDM_PURGESETTINGS,         MIR(IDS_PURGESETTINGS),       0,             MIR(IDS_TT_PURGESETTINGS),       TB_PURGESETTINGS,       0,                         WIN_ALL,                         CAT_SETTINGS_MENU,   NSCHF_NONE },
   { "Customise",             IDM_CUSTOMISE,          MIR(IDS_CUSTOMISE),           0,             MIR(IDS_TT_CUSTOMISE),           TB_CUSTOMISE,           0,                         WIN_ALL,                         CAT_SETTINGS_MENU,   NSCHF_NONE },
   { "FontSettings",          IDM_FONTSETTINGS,       MIR(IDS_FONTSETTINGS),        0,             MIR(IDS_TT_FONTSETTINGS),        TB_FONTSETTINGS,        0,                         WIN_ALL|WIN_NOMENU,              CAT_SETTINGS_MENU,   NSCHF_NONE },
   { "ColourSettings",           IDM_COLOURSETTINGS,        MIR(IDS_COLOURSETTINGS),      0,             MIR(IDS_TT_COLOURSETTINGS),         TB_COLOURSETTINGS,         0,                         WIN_ALL|WIN_NOMENU,              CAT_SETTINGS_MENU,   NSCHF_NONE },
   { "Signatures",               IDM_SIGNATURES,            MIR(IDS_SIGNATURES),       0,             MIR(IDS_TT_SIGNATURES),          TB_SIGNATURES,          0,                         WIN_ALL,                         CAT_SETTINGS_MENU,   NSCHF_NONE },
   { "Scheduler",             IDM_SCHEDULER,          MIR(IDS_SCHEDULER),           0,             MIR(IDS_TT_SCHEDULER),           TB_SCHEDULER,           0,                         WIN_ALL,                         CAT_SETTINGS_MENU,   NSCHF_NONE },
   { "Addons",                IDM_ADDONS,             MIR(IDS_ADDONS),           IDS_X_ADDONS,     MIR(IDS_TT_ADDONS),              TB_ADDONS,              0,                         WIN_ALL,                         CAT_SETTINGS_MENU,   NSCHF_NONE },
   { "ConfigureAddons",       IDM_CONFIGADDONS,       MIR(IDS_CONFIGADDONS),        IDS_X_ADDONS,     MIR(IDS_TT_CONFIGADDONS),        TB_CONFIGADDONS,        0,                         WIN_ALL,                         CAT_SETTINGS_MENU,   NSCHF_NONE },
   { "CustomiseToolbar",         IDM_CUSTTOOLBAR,        MIR(IDS_CUSTTOOLBAR),         0,             MIR(IDS_TT_CUSTTOOLBAR),         TB_CUSTTOOLBAR,            0,                         WIN_ALL,                         CAT_SETTINGS_MENU,   NSCHF_NONE },
   { "WindowNewWindow",       IDM_NEWWINDOW,          MIR(IDS_NEWWINDOW),           0,             MIR(IDS_TT_NEWWINDOW),           TB_NEWWINDOW,           0,                         WIN_ALL,                         CAT_WINDOW_MENU,  NSCHF_NONE },
   { "WindowCloseAll",           IDM_CLOSEALL,           MIR(IDS_CLOSEALL),            0,             MIR(IDS_TT_CLOSEALL),            TB_CLOSEALL,            0,                         WIN_ALL,                         CAT_WINDOW_MENU,  NSCHF_NONE },
   { "WindowCascade",            IDM_CASCADE,            MIR(IDS_CASCADE),          0,             MIR(IDS_TT_CASCADE),          TB_CASCADE,             MAKEKEY( HK_SHIFT, VK_F5 ),         WIN_ALL,                      CAT_WINDOW_MENU,  NSCHF_NONE },
   { "WindowTileHorz",           IDM_TILEHORZ,           MIR(IDS_TILEHORZ),            0,             MIR(IDS_TT_TILEHORZ),            TB_TILEHORZ,            MAKEKEY( HK_SHIFT, VK_F4 ),         WIN_ALL,                      CAT_WINDOW_MENU,  NSCHF_NONE },
   { "WindowTileVert",           IDM_TILEVERT,           MIR(IDS_TILEVERT),            0,             MIR(IDS_TT_TILEVERT),            TB_TILEVERT,            0,                         WIN_ALL,                      CAT_WINDOW_MENU,  NSCHF_NONE },
   { "WindowArrangeIcons",       IDM_ARRANGEICONS,       MIR(IDS_ARRANGEICONS),        0,             MIR(IDS_TT_ARRANGEICONS),        TB_ARRANGEICONS,        0,                         WIN_ALL,                      CAT_WINDOW_MENU,  NSCHF_NONE },
   { "HelpContents",          IDM_HELPCONTENTS,       MIR(IDS_HELPCONTENTS),        0,             MIR(IDS_TT_HELPCONTENTS),        TB_HELPCONTENTS,        MAKEKEY( 0, VK_F1 ),          WIN_ALL,                      CAT_HELP_MENU,    NSCHF_NONE },
   { "HelpReport",               IDM_REPORT,             MIR(IDS_REPORT),           0,             MIR(IDS_TT_REPORT),              TB_REPORT,              0,                         WIN_ALL,                      CAT_HELP_MENU,    NSCHF_NONE },
   { "HelpAbout",             IDM_ABOUT,              MIR(IDS_ABOUT),               0,             MIR(IDS_TT_ABOUT),               TB_ABOUT,               0,                         WIN_ALL,                      CAT_HELP_MENU,    NSCHF_NONE },
   { "HelpShowTips",          IDM_SHOWTIPS,           MIR(IDS_SHOWTIPS),            0,             MIR(IDS_TT_SHOWTIPS),            TB_SHOWTIPS,            0,                         WIN_ALL,                      CAT_HELP_MENU,    NSCHF_NONE },
   { "HelpStartupWizard",        IDM_STARTWIZ,           MIR(IDS_STARTWIZ),            0,             MIR(IDS_TT_STARTWIZ),            TB_STARTWIZ,            0,                         WIN_ALL,                      CAT_HELP_MENU,    NSCHF_NONE },
   { "HelpCIXSupport",           IDM_CIXSUPPORT,            MIR(IDS_CIXSUPPORT),       0,             MIR(IDS_TT_CIXSUPPORT),          TB_CIXSUPPORT,          0,                         WIN_ALL,                      CAT_HELP_MENU,    NSCHF_NONE },
   { "HelpCheckForUpdates",      IDM_CHECKFORUPDATES,    MIR(IDS_CHECKFORUPDATES),     0,             MIR(IDS_TT_CHECKFORUPDATES),     TB_CHECKFORUPDATES,        0,                         WIN_ALL,                      CAT_HELP_MENU,    NSCHF_NONE },
   { "Backspace",             IDM_BACKSPACE,          MIR(IDS_BACKSPACE),           0,             MIR(IDS_TT_BACKSPACE),           TB_BACKSPACE,           MAKEKEY( 0, VK_BACK ),           WIN_THREAD|WIN_MESSAGE|WIN_NOMENU,     CAT_KEYBOARD,     NSCHF_NONE },
   { "ToggleThread",          IDM_TOGGLETHREAD,       MIR(IDS_TOGGLETHREAD),        0,             MIR(IDS_TT_TOGGLETHREAD),        TB_TOGGLETHREAD,        0,                         WIN_THREAD|WIN_MESSAGE|WIN_NOMENU,     CAT_KEYBOARD,     NSCHF_NONE },
   { "ExpandThread",          IDM_EXPANDTHREAD,       MIR(IDS_EXPANDTHREAD),        0,             MIR(IDS_TT_EXPANDTHREAD),        TB_EXPANDTHREAD,        MAKEKEY( 0, VK_ADD ),            WIN_THREAD|WIN_MESSAGE|WIN_NOMENU,     CAT_KEYBOARD,     NSCHF_NONE },
   { "ShrinkThread",          IDM_SHRINKTHREAD,       MIR(IDS_SHRINKTHREAD),        0,             MIR(IDS_TT_SHRINKTHREAD),        TB_SHRINKTHREAD,        MAKEKEY( 0, VK_SUBTRACT ),       WIN_THREAD|WIN_MESSAGE|WIN_NOMENU,     CAT_KEYBOARD,     NSCHF_NONE },
   { "PrevUnread",               IDM_PREVUNREAD,            MIR(IDS_PREVUNREAD),       0,             MIR(IDS_TT_PREVUNREAD),          TB_PREVUNREAD,          MAKEKEY( HK_SHIFT, VK_LEFT ),    WIN_THREAD|WIN_NOMENU,              CAT_KEYBOARD,     NSCHF_NONE },
   { "PrevRoot",              IDM_PREVROOT,           MIR(IDS_PREVROOT),            0,             MIR(IDS_TT_PREVROOT),            TB_PREVROOT,            MAKEKEY( 0, VK_LEFT ),           WIN_THREAD|WIN_NOMENU,              CAT_KEYBOARD,     NSCHF_NONE },
   { "PrevTopic",             IDM_PREVTOPIC,          MIR(IDS_PREVTOPIC),           0,             MIR(IDS_TT_PREVTOPIC),           TB_PREVTOPIC,           MAKEKEY( HK_CTRL, VK_LEFT ),     WIN_THREAD|WIN_MESSAGE|WIN_NOMENU,     CAT_KEYBOARD,     NSCHF_NONE },
   { "NextUnread",               IDM_NEXTUNREAD,            MIR(IDS_NEXTUNREAD),       0,             MIR(IDS_TT_NEXTUNREAD),          TB_NEXTUNREAD,          MAKEKEY( HK_SHIFT, VK_RIGHT ),      WIN_THREAD|WIN_NOMENU,              CAT_KEYBOARD,     NSCHF_NONE },
   { "NextRoot",              IDM_NEXTROOT,           MIR(IDS_NEXTROOT),            0,             MIR(IDS_TT_NEXTROOT),            TB_NEXTROOT,            MAKEKEY( 0, VK_RIGHT ),          WIN_THREAD|WIN_NOMENU,              CAT_KEYBOARD,     NSCHF_NONE },
   { "NextTopic",             IDM_NEXTTOPIC,          MIR(IDS_NEXTTOPIC),           0,             MIR(IDS_TT_NEXTTOPIC),           TB_NEXTTOPIC,           MAKEKEY( HK_CTRL, VK_RIGHT ),    WIN_THREAD|WIN_MESSAGE|WIN_NOMENU,     CAT_KEYBOARD,     NSCHF_NONE },
   { "Forward",               IDM_FORWARD,            MIR(IDS_FORWARD),          0,             MIR(IDS_TT_FORWARD),          TB_FORWARD,             0,                         WIN_THREAD|WIN_MESSAGE|WIN_NOMENU,     CAT_KEYBOARD,     NSCHF_NONE },
   { "Prev",                  IDM_PREV,               MIR(IDS_PREV),             0,             MIR(IDS_TT_PREV),             TB_PREV,             0,                         WIN_THREAD|WIN_MESSAGE|WIN_NOMENU,     CAT_KEYBOARD,     NSCHF_NONE },
   { "UpperCase",             IDM_UPPERCASE,          MIR(IDS_UPPERCASE),           0,             MIR(IDS_TT_UPPERCASE),           TB_UPPERCASE,           MAKEKEY( HK_CTRLSHIFT, 'U' ),    WIN_EDITS|WIN_NOMENU,               CAT_KEYBOARD,     NSCHF_NONE },
   { "LowerCase",             IDM_LOWERCASE,          MIR(IDS_LOWERCASE),           0,             MIR(IDS_TT_LOWERCASE),           TB_LOWERCASE,           MAKEKEY( HK_CTRLSHIFT, 'L' ),    WIN_EDITS|WIN_NOMENU,               CAT_KEYBOARD,     NSCHF_NONE },
   { "DocMax",                IDM_DOCMAX,             MIR(IDS_DOCMAX),           0,             MIR(IDS_TT_DOCMAX),              TB_DOCMAX,              MAKEKEY( HK_CTRL, VK_F10 ),         WIN_ALL|WIN_NOMENU,                 CAT_KEYBOARD,     NSCHF_NONE },
   { "AppMax",                IDM_APPMAX,             MIR(IDS_APPMAX),           0,             MIR(IDS_TT_APPMAX),              TB_APPMAX,              MAKEKEY( HK_ALT, VK_F10 ),       WIN_ALL|WIN_NOMENU,                 CAT_KEYBOARD,     NSCHF_NONE },
   { "DocRest",               IDM_DOCREST,            MIR(IDS_DOCREST),          0,             MIR(IDS_TT_DOCREST),          TB_DOCREST,             MAKEKEY( HK_CTRL, VK_F5 ),       WIN_ALL|WIN_NOMENU,                 CAT_KEYBOARD,     NSCHF_NONE },
   { "AppRest",               IDM_APPREST,            MIR(IDS_APPREST),          0,             MIR(IDS_TT_APPREST),          TB_APPREST,             MAKEKEY( HK_ALT, VK_F5 ),        WIN_ALL|WIN_NOMENU,                 CAT_KEYBOARD,     NSCHF_NONE },
   { "DocMin",                IDM_DOCMIN,             MIR(IDS_DOCMIN),           0,             MIR(IDS_TT_DOCMIN),              TB_DOCMIN,              MAKEKEY( HK_CTRL, VK_F9 ),       WIN_ALL|WIN_NOMENU,                 CAT_KEYBOARD,     NSCHF_NONE },
   { "AppMin",                IDM_APPMIN,             MIR(IDS_APPMIN),           0,             MIR(IDS_TT_APPMIN),              TB_APPMIN,              MAKEKEY( HK_ALT, VK_F9 ),        WIN_ALL|WIN_NOMENU,                 CAT_KEYBOARD,     NSCHF_NONE },
   { "DocSize",               IDM_DOCSIZE,            MIR(IDS_DOCSIZE),          0,             MIR(IDS_TT_DOCSIZE),          TB_DOCSIZE,             MAKEKEY( HK_CTRL, VK_F8 ),       WIN_ALL|WIN_NOMENU,                 CAT_KEYBOARD,     NSCHF_NONE },
   { "AppSize",               IDM_APPSIZE,            MIR(IDS_APPSIZE),          0,             MIR(IDS_TT_APPSIZE),          TB_APPSIZE,             MAKEKEY( HK_ALT, VK_F8 ),        WIN_ALL|WIN_NOMENU,                 CAT_KEYBOARD,     NSCHF_NONE },
   { "DocMove",               IDM_DOCMOVE,            MIR(IDS_DOCMOVE),          0,             MIR(IDS_TT_DOCMOVE),          TB_DOCMOVE,             MAKEKEY( HK_CTRL, VK_F7 ),       WIN_ALL|WIN_NOMENU,                 CAT_KEYBOARD,     NSCHF_NONE },
   { "AppMove",               IDM_APPMOVE,            MIR(IDS_APPMOVE),          0,             MIR(IDS_TT_APPMOVE),          TB_APPMOVE,             MAKEKEY( HK_ALT, VK_F7 ),        WIN_ALL|WIN_NOMENU,                 CAT_KEYBOARD,     NSCHF_NONE },
   { "RunApp",                IDM_EXTERNALAPP,        MIR(IDS_EXTERNALAPP),         0,             MIR(IDS_TT_EXTERNALAPP),         TB_EXTERNALAPP,            0,                         WIN_ALL|WIN_NOMENU,                 CAT_SPARE,        NSCHF_NONE },
   { "Separator",             IDM_SEP,             MIR(IDS_SEP),              0,             MIR(IDS_TT_SEP),              TB_SEP,                 0,                         WIN_ALL|WIN_NOMENU,                 CAT_SPARE,        NSCHF_NONE },
   { NULL,                    (UINT)-1,               MIR(-1),                (UINT)-1,         MIR(-1),                   0,                   0,                         0 }
   };

/* Toolbar string array
 */
UINT ToolStr[] = {
   IDS_TT_BLINK,
   IDS_TT_EXIT,
   IDS_TT_FORWARD,
   IDS_TT_NEXT,
   IDS_TT_ORIGINAL,
   IDS_TT_PREV,
   IDS_TT_PRINT,
   IDS_TT_PRINTSETUP,
   IDS_TT_CLEARCIXMAIL,
   IDS_TT_UNDO,
   IDS_TT_CUT,
   IDS_TT_COPY,
   IDS_TT_PASTE,
   IDS_TT_CLEAR,
   IDS_TT_QUOTE,
   IDS_TT_INSERTFILE,
   IDS_TT_MAIL,
   IDS_TT_NEXTPRIORITY,
   IDS_TT_NEXTMARKED,
   IDS_TT_PREVMARKED,
   IDS_TT_SEARCH,
   IDS_TT_PURGE,
   IDS_TT_SHOWALLGROUPS,
   IDS_TT_SUBSCRIBE,
   IDS_TT_SIGNATURES,
   IDS_TT_NEWFOLDER,
   IDS_TT_TOOLBAR,
   IDS_TT_STATUSBAR,
   IDS_TT_MARKTOPICREAD,
   IDS_TT_MARKTHREADREAD,
   IDS_TT_SCRIPTS,
   IDS_TT_COPYCIXLINK,
   IDS_TT_COPYMSG,
   IDS_TT_GOTO,
   IDS_TT_RETHREADARTICLES, 
   IDS_TT_MARKMSG,
   IDS_TT_KEEPMSG,
   IDS_TT_DELETEMSG,
   IDS_TT_IGNORE,
   IDS_TT_PRIORITY,
   IDS_TT_CASCADE,
   IDS_TT_TILEHORZ,
   IDS_TT_CLOSEALL,
   IDS_TT_HELPCONTENTS,
   IDS_TT_NULLSTRING,
   IDS_TT_READLOCK,
   IDS_TT_NULLSTRING,
   IDS_TT_ABOUT,
   IDS_TT_BACKSPACE,
   IDS_TT_PREVUNREAD,
   IDS_TT_NEXTUNREAD,
   IDS_TT_PREVTOPIC,
   IDS_TT_NEXTTOPIC,
   IDS_TT_PAGESETUP,
   IDS_TT_HEADERS,
   IDS_TT_STOP,
   IDS_TT_DECODE,
   IDS_TT_ONLINE,
   IDS_TT_OPENCLOSELOG,
   IDS_TT_PAUSERESUMELOG,
   IDS_TT_NULLSTRING, // IDS_TT_ZMODEMSEND,
   IDS_TT_NULLSTRING, // IDS_TT_ZMODEMRECEIVE,
   IDS_TT_NEWWINDOW,
   IDS_TT_FIXEDFONT,
   IDS_TT_SPELLCHECK,
   IDS_TT_REPLY,
   IDS_TT_REPLYTOALL,
   IDS_TT_RESIGN,
   IDS_TT_SEND,
   IDS_TT_SELECTALL,
   IDS_TT_CLEARBUF,
   IDS_TT_NULLSTRING, // IDS_TT_SHOWNEWGROUPS,
   IDS_TT_UPDCIXLIST,
   IDS_TT_UPDUSERSLIST,
   IDS_TT_OUTBASKET,
   IDS_TT_INBASKET,
   IDS_TT_GETARTICLES,
   IDS_TT_GETTAGGED,
   IDS_TT_PREFERENCES,
   IDS_TT_FILTERS,
   IDS_TT_TOGGLETHREAD,
   IDS_TT_EXPANDTHREAD,
   IDS_TT_SHRINKTHREAD,
   IDS_TT_MARKTAGGED,
   IDS_TT_READMSG,
   IDS_TT_PREVROOT,
   IDS_TT_NEXTROOT,
   IDS_TT_DOCMAX,
   IDS_TT_APPMAX,
   IDS_TT_DOCMIN,
   IDS_TT_APPMIN,
   IDS_TT_DOCREST,
   IDS_TT_APPREST,
   IDS_TT_DOCSIZE,
   IDS_TT_APPSIZE,
   IDS_TT_DOCMOVE,
   IDS_TT_APPMOVE,
   IDS_TT_CUSTOMISE,
   IDS_TT_MAPISEND,
   IDS_TT_GETNEWMAIL,
   IDS_TT_POSTARTICLE,
   IDS_TT_FOLLOWUPARTICLE,
   IDS_TT_DELETE,
   IDS_TT_UNSUBSCRIBE,
   IDS_TT_FORWARDMSG,
   IDS_TT_SORTMAILTO,
   IDS_TT_RUNATTACHMENT,
   IDS_TT_ROT13,
   IDS_TT_ADDRBOOK,
   IDS_TT_ADDTOADDRBOOK,
   IDS_TT_CUSTTOOLBAR,
   IDS_TT_FONTSETTINGS,
   IDS_TT_FAVOURITES,
   IDS_TT_FIND,
   IDS_TT_REPLACE,
   IDS_TT_RENAME,
   IDS_TT_PROPERTIES,
   IDS_TT_ADDONS,
   IDS_TT_TILEVERT,
   IDS_TT_ADMIN,
   IDS_TT_JOINCONFERENCE,
   IDS_TT_RESIGNCIX,
   IDS_TT_NULLSTRING,   //IDS_TT_CREATEFAX,
   IDS_TT_SHOWCIXCONFS,
   IDS_TT_GETCIXCONFS,
   IDS_TT_FILELIST,
   IDS_TT_DOWNLOAD,
   IDS_TT_UPLOADFILE,
   IDS_TT_PARTICIPANTS,
   IDS_TT_IMPORT,
   IDS_TT_EXPORT,
   IDS_TT_SHOWRESUME,
   IDS_TT_RESUMES,
   IDS_TT_SAY,
   IDS_TT_COMMENT,
   IDS_TT_MARKTHREAD,
   IDS_TT_MARKTHREADTAGGED,
   IDS_TT_READLOCKTHREAD,
   IDS_TT_KEEPTHREAD,
   IDS_TT_DELETETHREAD,
   IDS_TT_PRIORITYTHREAD,
   IDS_TT_IGNORETHREAD,
   IDS_TT_READTHREAD,
   IDS_TT_QUICKEXPORT,
   IDS_TT_STOPIMPORT,
   IDS_TT_SHOWALLUSERS,
   IDS_TT_SORT_REFERENCE,
   IDS_TT_SORT_ROOTS,
   IDS_TT_SORT_MSGNUM,
   IDS_TT_SORT_SENDER,
   IDS_TT_SORT_DATETIME,
   IDS_TT_SORT_MSGSIZE,
   IDS_TT_SORT_SUBJECT,
   IDS_TT_ASCENDING,
   IDS_TT_NEWMAILFOLDER,
   IDS_TT_EVENTLOG,
   IDS_TT_NULLSTRING, // IDS_TT_XMODEMSEND,
   IDS_TT_NULLSTRING, // IDS_TT_XMODEMRECEIVE,
   IDS_TT_TERMINAL,
   IDS_TT_WITHDRAW,
   IDS_TT_REBUILD_INDEX,
   IDS_TT_DELETEATTACHMENT,
   IDS_TT_NEWLOCALTOPIC,
   IDS_TT_CIXSETTINGS,
   IDS_TT_COMMUNICATIONS,
   IDS_TT_UPPERCASE,
   IDS_TT_LOWERCASE,
   IDS_TT_REPORT,
   IDS_TT_CHECK,
   IDS_TT_BROWSE,
   IDS_TT_OWNRESUME,
   IDS_TT_COLOURSETTINGS,
   IDS_TT_EXTERNALAPP,
   IDS_TT_CIXMAILDIRECTORY,
   IDS_TT_CIXMAILERASE,
   IDS_TT_CIXMAILRENAME,
   IDS_TT_CIXMAILEXPORT,
   IDS_TT_CIXMAILUPLOAD,
   IDS_TT_CIXMAILDOWNLOAD,
   IDS_TT_FILELISTFIND,
   IDS_TT_SHOW_MSGNUM,
   IDS_TT_SHOW_SENDER,
   IDS_TT_SHOW_DATETIME,
   IDS_TT_SHOW_MSGSIZE,
   IDS_TT_SHOW_SUBJECT,
   IDS_TT_ARRANGEICONS,
   IDS_TT_CUSTOMBLINK,
   IDS_TT_DIRECTORIES,
   IDS_TT_MAILSETTINGS,
   IDS_TT_SCHEDULER,
   IDS_TT_MOVEMSG,
   IDS_TT_KEEPATTOP,
   IDS_TT_SENDFILE,
   IDS_TT_CLEARIPMAIL,
   IDS_TT_WATCH,
   IDS_TT_USENET,
   IDS_TT_NULLSTRING,
   IDS_TT_UPDFILELIST,
   IDS_TT_UPDPARTLIST,
   IDS_TT_SORTMAILFROM,
   IDS_TT_WORDWRAP,
   IDS_TT_BUILD,
   IDS_TT_KILLTHREAD,
   IDS_TT_KILLMSG,
   IDS_TT_CONFIGADDONS,
   IDS_TT_NEWCATEGORY,
   IDS_TT_MSGSTYLES,
   IDS_TT_BLINKDDC,
   IDS_TT_VIEWINBROWSER,
   IDS_TT_FOLLOWUPARTICLETOALL,
   IDS_TT_REPOST,
   IDS_TT_PURGESETTINGS,
   IDS_TT_UPDCONFNOTES,
   IDS_TT_HOTLINKS,
   IDS_TT_SHOWTIPS,
   IDS_TT_STARTWIZ,
   IDS_TT_SEP,
   IDS_TT_READNEWS,
   IDS_TT_READMAIL,
   IDS_TT_SORT,
   IDS_TT_COPYALL,
   IDS_TT_NULLSTRING,
   IDS_TT_MLHOTLINKS, // 2.55.2031
   IDS_TT_REDO // 20060302 - 2.56.2049.08
   };
#define  maxToolStr  (sizeof(ToolStr)/sizeof(UINT))

/* This is the old default toolbar. We keep it here for existing users
 * who haven't customised their toolbar. New users have the new default
 * shown below.
 * Items marked TBSTYLE_SEP can have their iBitmap field set to
 * the width of the separator. 0 means that they're set to the
 * default separator width.
 */
TBBUTTON FAR aDefToolbar[] = {
   TB_INBASKET,         IDM_INBASKET,           TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_INBASKET,
   TB_OUTBASKET,        IDM_OUTBASKET,          TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_OUTBASKET,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_SHOWCIXCONFS,     IDM_SHOWCIXCONFS,       TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_SHOWCIXCONFS,
   TB_JOINCONFERENCE,      IDM_JOINCONFERENCE,        TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_JOINCONFERENCE,
   TB_SAY,              IDM_SAY,             TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_SAY,
   TB_COMMENT,          IDM_COMMENT,            TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_COMMENT,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_BLINK,            IDM_SENDRECEIVEALL,        TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_BLINK,
   TB_STOP,          IDM_STOP,               TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_STOP,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_SEARCH,           IDM_SEARCH,             TBSTATE_ENABLED,  TBSTYLE_BUTTON,      0, TB_SEARCH,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_BROWSE,           IDM_BROWSE,             TBSTATE_ENABLED,  TBSTYLE_BUTTON,      0, TB_BROWSE,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_PRINT,            IDM_QUICKPRINT,            TBSTATE_ENABLED,  TBSTYLE_BUTTON,      0, TB_PRINT,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_HELPCONTENTS,     IDM_HELPCONTENTS,       TBSTATE_ENABLED,  TBSTYLE_BUTTON,      0, TB_HELPCONTENTS,
   TB_REPORT,           IDM_REPORT,             TBSTATE_ENABLED,  TBSTYLE_BUTTON,      0, TB_REPORT,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_NEXT,          IDM_NEXT,               TBSTATE_ENABLED,  TBSTYLE_BUTTON,      0, TB_NEXT,
   TB_NEXTPRIORITY,     IDM_NEXTPRIORITY,       TBSTATE_ENABLED,  TBSTYLE_BUTTON,      0, TB_NEXTPRIORITY,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_EXIT,          IDM_EXIT,               TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_EXIT
   };
#define cToolsOnDefToolbar    (sizeof(aDefToolbar)/sizeof(aDefToolbar[0]))

/* This is the new default toolbar
 *

typedef struct _TBBUTTON {
    int iBitmap;
    int idCommand;
    BYTE fsState;
    BYTE fsStyle;
    DWORD dwData;
    int iString;
} TBBUTTON, NEAR* PTBBUTTON, FAR* LPTBBUTTON;

 */
TBBUTTON FAR aNewDefToolbar[] = {
   TB_INBASKET,         IDM_INBASKET,           TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_INBASKET,
   TB_OUTBASKET,        IDM_OUTBASKET,          TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_OUTBASKET,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_SHOWCIXCONFS,     IDM_SHOWCIXCONFS,       TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_SHOWCIXCONFS,
   TB_SAY,              IDM_SAY,             TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_SAY,
   TB_COMMENT,          IDM_COMMENT,            TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_COMMENT,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_BLINK,            IDM_SENDRECEIVEALL,        TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_BLINK,
   TB_STOP,          IDM_STOP,               TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_STOP,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_BROWSE,           IDM_BROWSE,             TBSTATE_ENABLED,  TBSTYLE_BUTTON,      0, TB_BROWSE,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_PRINT,            IDM_QUICKPRINT,            TBSTATE_ENABLED,  TBSTYLE_BUTTON,      0, TB_PRINT,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_HELPCONTENTS,     IDM_HELPCONTENTS,       TBSTATE_ENABLED,  TBSTYLE_BUTTON,      0, TB_HELPCONTENTS,
   TB_REPORT,           IDM_REPORT,             TBSTATE_ENABLED,  TBSTYLE_BUTTON,      0, TB_REPORT,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_EXIT,          IDM_EXIT,               TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_EXIT
   };
#define cToolsOnNewDefToolbar    (sizeof(aNewDefToolbar)/sizeof(aNewDefToolbar[0]))

/* This is the default IP Only toolbar.
 * Items marked TBSTYLE_SEP can have their iBitmap field set to
 * the width of the separator. 0 means that they're set to the
 * default separator width.
 */
TBBUTTON FAR aDefIPToolbar[] = {
   TB_INBASKET,         IDM_INBASKET,           TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_INBASKET,
   TB_OUTBASKET,        IDM_OUTBASKET,          TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_OUTBASKET,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_SAY,              IDM_SAY,             TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_SAY,
   TB_COMMENT,          IDM_COMMENT,            TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_COMMENT,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_SUBSCRIBE,        IDM_SUBSCRIBE,          TBSTATE_ENABLED,  TBSTYLE_BUTTON,      0, TB_SUBSCRIBE,
   TB_SHOWALLGROUPS,    IDM_SHOWALLGROUPS,         TBSTATE_ENABLED,  TBSTYLE_BUTTON,      0, TB_SHOWALLGROUPS,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_BLINK,            IDM_BLINKFULL,          TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_BLINK,
   TB_BLINKDDC,         IDM_BLINKDDC,           TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_BLINKDDC,
   TB_STOP,          IDM_STOP,               TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_STOP,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_SEARCH,           IDM_SEARCH,             TBSTATE_ENABLED,  TBSTYLE_BUTTON,      0, TB_SEARCH,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_BROWSE,           IDM_BROWSE,             TBSTATE_ENABLED,  TBSTYLE_BUTTON,      0, TB_BROWSE,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_PRINT,            IDM_QUICKPRINT,            TBSTATE_ENABLED,  TBSTYLE_BUTTON,      0, TB_PRINT,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_MAIL,          IDM_MAIL,               TBSTATE_ENABLED,  TBSTYLE_BUTTON,      0, TB_MAIL,
   TB_REPLY,            IDM_REPLY,              TBSTATE_ENABLED,  TBSTYLE_BUTTON,      0, TB_REPLY,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_HELPCONTENTS,     IDM_HELPCONTENTS,       TBSTATE_ENABLED,  TBSTYLE_BUTTON,      0, TB_HELPCONTENTS,
   TB_REPORT,           IDM_REPORT,             TBSTATE_ENABLED,  TBSTYLE_BUTTON,      0, TB_REPORT,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_NEXT,          IDM_NEXT,               TBSTATE_ENABLED,  TBSTYLE_BUTTON,      0, TB_NEXT,
   TB_NEXTPRIORITY,     IDM_NEXTPRIORITY,       TBSTATE_ENABLED,  TBSTYLE_BUTTON,      0, TB_NEXTPRIORITY,
   0,                0,                   TBSTATE_ENABLED,  TBSTYLE_SEP,      0, 0,
   TB_EXIT,          IDM_EXIT,               TBSTATE_ENABLED,  TBSTYLE_BUTTON,   0, TB_EXIT
   };
#define cToolsOnDefIPToolbar     (sizeof(aDefIPToolbar)/sizeof(aDefIPToolbar[0]))

BYTE rgEncodeKey[8] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef };

typedef int (CDECL * LPTRANSLATEPROC)( int );
int CDECL Rot13Function( int );


BOOL FAR PASCAL AmLoadAcronymList( void );

BOOL FASTCALL MainWnd_OnCreate( HWND, LPCREATESTRUCT );
void FASTCALL MainWnd_OnClose( HWND );
void FASTCALL MainWnd_OnEndSession( HWND, BOOL );
void FASTCALL MainWnd_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
LRESULT FASTCALL MainWnd_OnNotify( HWND, int, LPNMHDR );
void FASTCALL MainWnd_OnSize( HWND, UINT, int, int );
void FASTCALL MainWnd_OnMove( HWND, int, int );
void FASTCALL MainWnd_OnDestroy( HWND );
void FASTCALL MainWnd_OnCommand( HWND, int, HWND, UINT );
void FASTCALL MainWnd_OnMenuSelect( HWND, HMENU, int, HMENU, UINT );
void FASTCALL MainWnd_OnInitMenu( HWND, HMENU );
void FASTCALL MainWnd_OnActivate( HWND, UINT, HWND, BOOL );
void FASTCALL MainWnd_OnWinIniChange( HWND, LPCSTR );
void FASTCALL MainWnd_OnSettingChange( HWND, int, LPCSTR );
void FASTCALL MainWnd_OnDevModeChange( HWND, LPCSTR );
void FASTCALL MainWnd_OnFontChange( HWND );
void FASTCALL MainWnd_OnTimer( HWND, UINT );
void FASTCALL MainWnd_OnPaint( HWND );
void FASTCALL MainWnd_OnDestroy( HWND );
BOOL FASTCALL MainWnd_OnQueryOpen( HWND );
BOOL FASTCALL MainWnd_OnQueryEndSession( HWND );
void FASTCALL MainWnd_CommonCloseCode( HWND, BOOL );
void FASTCALL MainWnd_OnSysColorChange( HWND );

BOOL EXPORT FAR PASCAL OpenPassword( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL OpenPassword_DlgProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL OpenPassword_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL OpenPassword_OnCommand( HWND, int, HWND, UINT );

LRESULT EXPORT CALLBACK MainWndProc( HWND, UINT, WPARAM, LPARAM );
BOOL EXPORT CALLBACK MainWndEvents( int, LPARAM, LPARAM );

int FASTCALL RunApp( HINSTANCE, HINSTANCE, LPSTR, int );
int FASTCALL Run( void );
void FASTCALL FindDefaultBrowser( void );
BOOL FASTCALL LoadCommon( void );
void FASTCALL SetLastNonIdleTime( void );
void FASTCALL DoFirstTimeUserActions( void );
PCAT FASTCALL CreateCIXDatabase( void );
BOOL FASTCALL CustomTranslateAccelerator( HWND, LPMSG );
BOOL FASTCALL InitializeApplication( void );
char * FASTCALL ReadDirectoryPath( int, char * );
BOOL FASTCALL InitializeInstance( LPSTR, int );
BOOL FASTCALL ParseCommandLine( LPSTR );
void FASTCALL UpdateAppSize( void );
void FASTCALL SetMainWindowSizes( HWND, int, int );
void FASTCALL CmdShowToolbar( BOOL );
void FASTCALL CmdNextUnread( HWND );
BOOL FASTCALL MyIsDialogMessage( HWND, LPMSG );
void FASTCALL CmdShowStatusBar( BOOL );
void FASTCALL CmdOnline( void );
PTL FASTCALL CreateMailTopic( char * );
void FASTCALL Ameol2_MDIActivate( HWND, BOOL, HWND, HWND );
LRESULT FASTCALL DefFrameWindowProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL ExitAmeol( HWND, BOOL );
LPSTR FASTCALL FormatTimeString( LPSTR );
void FASTCALL LoadKeyboard( void );
void FASTCALL AssignKeynamesToMenu( void );
void FASTCALL ChangeMenuString( HMENU, int, char * );
void FASTCALL ForwardCommand( HWND, int, HWND, UINT );
void FASTCALL CmdMapiSend( HWND );
void FASTCALL CmdQuote( HWND );
void FASTCALL CmdInsertFile( HWND );
void FASTCALL CmdTranslateSelection( HWND, LPTRANSLATEPROC );
void FASTCALL DisplayCommandStatus( UINT );
BOOL FASTCALL QueryHasNetwork( void );
void FASTCALL GetWorkspaceRects( HWND, LPRECT );
void FASTCALL WriteCommandAssignment( int, char * );
void FASTCALL SaveDefaultWindows( void );
void FASTCALL TestForShortFilenames( void );
void FASTCALL CmdCIXSupport ( HWND );
void FASTCALL CmdCheckForUpdates ( HWND );

void FASTCALL Amaddr_Load( void );
void FASTCALL SyncSearchPtrs( LPVOID );
void FASTCALL InstallAllFileTransferProtocols( void );
void FASTCALL UninstallAllFileTransferProtocols( void );
BOOL EXPORT CALLBACK LicenceProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL LicenceProc_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL LicenceProc_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL LicenceProc_DlgProc( HWND, UINT, WPARAM, LPARAM );

// Tips things

void FASTCALL CmdShowTips( HWND );
BOOL EXPORT CALLBACK CmdTipsDlg( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL CmdTipsDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL CmdTipsDlg_OnCommand( HWND, int, HWND, UINT );
BOOL FASTCALL CmdTipsDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL SetTipText( HWND );

// Short header things

BOOL EXPORT CALLBACK ShortHeaderEdit( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL ShortHeaderEdit_DlgProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL ShortHeaderEdit_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL ShortHeaderEdit_OnCommand( HWND, int, HWND, UINT );

// Can't use the header because that causes a ton of conflicts.
#ifdef USE_HOME_FOLDER
#define CSIDL_PERSONAL           0x0005
EXTERN_C DECLSPEC_IMPORT BOOL STDAPICALLTYPE SHGetSpecialFolderPathA(HWND hwnd, LPSTR pszPath, int csidl, BOOL fCreate);
#endif

LRESULT EXPORT WINAPI MdiClientWndProc( HWND, UINT, WPARAM, LPARAM );

ULONG (FAR PASCAL * fMAPISendMail)( LHANDLE, ULONG, lpMapiMessage, FLAGS, ULONG );

void FASTCALL LocateMissedItems( void );

SELRANGE FASTCALL Amctl_CountFormatChars(HWND hwnd, SELRANGE lSelStart );

#ifdef USEMUTEX // 2.55.2031
HANDLE InstanceMutexHandle;
#endif USEMUTEX // 2.55.2031
HANDLE InitMutexHandle;
BOOL WINAPI IsAmeol2Loaded( LPSTR );

BOOL FASTCALL CheckForControlFile(char * pName)
{
   char lAppDir[_MAX_PATH];
   char lChkFile[_MAX_PATH];
   char * lDir;

   GetModuleFileName( hInst, (char *)&lAppDir, _MAX_PATH );

   lDir = strrchr( (char *)&lAppDir, '\\' );
   if( NULL != lDir )
      *++lDir = '\0';
   

   wsprintf( lChkFile, "%s%s", lAppDir, pName );

   return ( Amfile_QueryFile( lChkFile ) );  

}
/* This function loads a DLL from the App module path
 */
HMODULE WINAPI AmLoadLibrary( LPCSTR lpLibFileName )
{
   char szPath[ _MAX_PATH ];

   wsprintf( szPath, "%s%s", pszAmeolDir, lpLibFileName );
   return( LoadLibrary( szPath ) );
}

/* This function determines whether the specified library file exists.
 */
BOOL FASTCALL FindLibrary( LPCSTR lpszLibName )
{
   HMODULE h;
   BOOL r;

   h = AmLoadLibrary(lpszLibName);
   r = h != NULL;
   FreeLibrary( h );
   return ( r );
}

/* This function is called when Palantir is started.
 */
int PASCAL WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow )
{
   int fRetCode;
   int count;

   fRetCode = 0;

   /* Ensure that only one instance of this app is running.
    */
   for( count = 0; count < 10; count++ )
   {
      InitMutexHandle = CreateMutex( 0, TRUE, "UniqueAmeol2Name" );
      if( GetLastError() == 0 )
         break;
      Sleep( 100 );
   }
   InterlockedIncrement( &cInstances );
   fRetCode = RunApp( hInstance, hPrevInstance, lpszCmdLine, nCmdShow );
   InterlockedDecrement( &cInstances );

   if( !fRetCode )
      ReleaseMutex( InitMutexHandle );

   /* Return result to caller.
    */
   return( fRetCode );
}

/* This function is called an instance of Ameol is required to
 * be started.
 */
int FASTCALL RunApp( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow )
{
   int r;
   OSVERSIONINFO osversion;

   /* Save the application instance handle globally.
    */
   hInst = hInstance;

   /* Set some global variables.
    */
   fQuitting = FALSE;
   fShowNext = FALSE;
   fShowFirst = FALSE;
   fLaptopKeys = FALSE;
   fOnline = FALSE;
   fBlinking = FALSE;
   fEndBlinking = FALSE;
   fInitiateQuit = FALSE;
   hwndTopic = NULL;
   fOldLogo = FALSE;
   fDNSCache = FALSE;
   dwDefWindowState = 0;
   nAccountType = 0;
   fStripHeaders = FALSE;
   fMsgStyles = FALSE;
   fWordWrap = FALSE;
   fHotLinks = TRUE;
   hwndToolBar = NULL;
   fShowToolBar = FALSE;
   fShowStatusBar = FALSE;
   hwndStatus = NULL;
   hwndFrame = NULL;
   hwndQuote = NULL;
   hwndMsg = NULL;
   hwndMDIDlg = NULL;
   hFindDlg = NULL;
   hSpellLib = NULL;
   wInitState = 0;
   fNoLogo = FALSE;
   fNoCMN = FALSE;
   fNoOLT = FALSE;
   fKeyboardChanged = FALSE;
   hSearchDlg = NULL;
   fStartOnline = FALSE;
   fAutoConnect = FALSE;
   fQuitAfterConnect = FALSE;
   fSageSettings = FALSE;
   fNoExit = FALSE;
   fBasicMode = FALSE;
   fNewsErrorReported = FALSE;
   fCompletingSetupWizard = FALSE;
   iActiveMode = WIN_ALL;
   fParsing = FALSE;
   fRunningScript = FALSE;
   fUseINIfile = CheckForControlFile("lite");    // !!SM!! 2.56.2051
   fUseU3 = CheckForControlFile("u3");       //!!SM!! 2.56.2053   
   fKillRunning = FALSE;
   fSecureOpen = FALSE;
   nToolbarPos = IDM_TOOLBAR_TOP;
   fCompatFlag = 0;
   iCachedButtonID = -1;
   strcpy( szUsenetGateway, "newsnet" );
   strcpy( szAutoConnectMode, "Full" );
   fSetup = FALSE;
   fShortFilenames = TRUE;
   fLogSMTP = FALSE;
   fLogNNTP = FALSE;
   fLogPOP3 = FALSE;
   fDoneConnectDevice = FALSE;
   fDoneDeviceConnected = FALSE;

   fUseWebServices = CheckForControlFile("webapi"); // !!SM!! 2.56.2054

   fMaxNewsgroups = 32000;

   /* Set default trace options.
    */
#ifdef _DEBUG
   fTraceOutputDebug = FALSE;
   fTraceLogFile = FALSE;
   fDebugMode = FALSE;
#endif

   /* Set blink masks dynamically because we modify them later.
    */
   BF_POSTCIX = BF_POSTCIXMAIL|BF_POSTCIXMSGS|BF_POSTCIXNEWS;
   BF_GETCIX = BF_GETCIXMAIL|BF_GETCIXMSGS|BF_GETCIXNEWS;
   BF_IPFLAGS = BF_GETIPMAIL|BF_GETIPNEWS|BF_POSTIPMAIL|BF_POSTIPNEWS|BF_GETIPTAGGEDNEWS;
   BF_CIXFLAGS = BF_POSTCIX|BF_GETCIXNEWS|BF_GETCIXTAGGEDNEWS|BF_GETCIX|BF_CIXRECOVER|BF_STAYONLINE;
   BF_IPUSENET = BF_GETIPTAGGEDNEWS|BF_GETIPNEWS|BF_POSTIPNEWS;
   BF_CIXUSENET = BF_GETCIXTAGGEDNEWS|BF_GETCIXNEWS|BF_POSTCIXNEWS;
   BF_USENET = BF_CIXUSENET|BF_IPUSENET;
   BF_FULLCONNECT = BF_POSTCIX|BF_GETCIX|BF_IPFLAGS;
   BF_EVERYTHING = BF_CIXFLAGS|BF_IPFLAGS;

   /* Get and save Windows version number
    */
   wWinVer = LOWORD( GetVersion() );
   wWinVer = (( (WORD)(LOBYTE( wWinVer ) ) ) << 8 ) | ( (WORD)HIBYTE( wWinVer ) );

   /* Figure out which OS we're using. Windows NT 4.0 and
    * Windows 95/Windows NT 3.51 use different means of
    * finding out this information.
    */
   fWinNT = FALSE;
   memset( &osversion, 0, sizeof(OSVERSIONINFO) );
   osversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
   GetVersionEx( &osversion );

   /* Read registry directly for Windows 9x & WinNT3.51, if WinNT 4.0 
    * and above then use SystemParametersInfo
    */
   if( ( osversion.dwPlatformId == VER_PLATFORM_WIN32_NT ) && ( osversion.dwMajorVersion >= 4 ) )
      fWinNT = TRUE;

   /* Set the Palantir resources file.
    */
   hRscLib = hInst;

   /* Get version info.
    */
   amv.hInstance = hInst;
   GetPalantirVersion( &amv );

   /* Initialize the Latest version info
    */
   amvLatest.hInstance = hInst;
   GetPalantirVersion( &amvLatest );
   

   /* Get the current system time.
    */
   if( Amdate_GetPackedCurrentDate() == 65535 )
      {
      /* Don't put this string in the string table.
       */
      MessageBox( NULL, "Your system date is set to a date later than 2037.\r\nPlease correct your system date then restart Ameol.", szAppName, MB_OK|MB_ICONEXCLAMATION );
      return( FALSE );
      }

   if( Amdate_GetPackedCurrentDate() < 0x1D21 )
      {
      MessageBox( NULL, GS(IDS_STR621), szAppName, MB_OK|MB_ICONEXCLAMATION );
      return( FALSE );
      }


   /* Initialise Ameol Dialog Manager.
    */
   if( ( r = Adm_Init( hInst ) ) < 0 )
      switch( r )
         {
         default:
            return( FALSE );

         case ADM_INIT_ERROR:
            MessageBox( NULL, GS(IDS_STR158), szAppName, MB_OK|MB_ICONEXCLAMATION );
            return( FALSE );
         }

   /* Parse the command line.
    */
   szLoginUsername[ 0 ] = '\0';
   szLoginPassword[ 0 ] = '\0';
   strcpy( szDictLoadMode, "preload" );

   ParseCommandLine( lpszCmdLine );

   if ( fUseU3 )
   {
      SetINIMode(TRUE); // 2.56.2053
      SetU3Mode(TRUE); // 2.56.2053
   }
   else
      SetINIMode(fUseINIfile); // 2.56.2051 FS#121

   /* Create and register the window classes.
    */
   if( !InitializeApplication() )
      return( FALSE );

   /* Create the frame window and do other initialization
    */
   if( !InitializeInstance( lpszCmdLine, nCmdShow ) )
      return( fInitNewsgroup || fMailto || fCixLink );

   /* Check for default filters
    */
   CheckFilters();

   /* Now spin the main task loop
    */
   return( Run() );
}

/* This is the Palantir main message loop. The code
 * presented here allows for scheduling background activity
 * at times when the application is idle.
 */
int FASTCALL Run( void )
{
   BOOL fIdle;

   fQuitSeen = FALSE;
   while( !fQuitSeen )
      {
      fIdle = TRUE;
      while( TaskYield() )
         fIdle = FALSE;
      if( !fQuitSeen && fIdle )
         WaitMessage();
      }
   return( 0 );
}

BOOL FASTCALL AmCheckForNewVersion_Old( void ) /*!!SM!!*/
{
   AMAPI_VERSION lamv;
   char szBuf[20];
   char szTkn[20];
   char sz[255];
   static HNDFILE fh = HNDFILE_ERROR;

   memset(&szBuf,0,20);

   MakePath( sz, pszAmeolDir, "version.txt" );

   if( HNDFILE_ERROR != ( fh = Amfile_Open( sz, AOF_READ ) ) )
   {
      Amfile_Read( fh, &szBuf, 19 );
  
      ExtractToken((char *)&szBuf, (char *)&szTkn, 0, '.');
      lamv.nMaxima = atoi(szTkn);
      
      ExtractToken((char *)&szBuf, (char *)&szTkn, 1, '.');
      lamv.nMinima = atoi(szTkn);

      ExtractToken((char *)&szBuf, (char *)&szTkn, 2, '.');
      lamv.nBuild  = atoi(szTkn);

      Amfile_Close( fh );
           
      if ( (lamv.nMaxima > amv.nMaxima) || ((lamv.nMaxima == amv.nMaxima) && (lamv.nMinima > amv.nMinima)) || (lamv.nBuild > amv.nBuild))
      {
         UINT uRetCode;

         wsprintf( lpPathBuf, "%samupdwiz.exe", pszAmeolDir );
         wsprintf( sz, "/a %s", szBuf);
         if( ( uRetCode = (UINT)ShellExecute( hwndFrame, NULL, lpPathBuf, sz, pszAmeolDir, SW_SHOWNORMAL ) ) < 32 )
            DisplayShellExecuteError( hwndFrame, lpPathBuf, uRetCode );
         return FALSE;
      }
      else
         return FALSE;
   }
   else
      return FALSE;

}

BOOL FASTCALL AmCheckForNewVersion( void ) /*!!SM!!*/
{
   char lStr[100];
   char lpStr[200];

#ifndef TESTING   
   if ( (amvLatest.nMaxima > amv.nMaxima) || ((amvLatest.nMaxima == amv.nMaxima) && (amvLatest.nMinima > amv.nMinima)) || (amvLatest.nBuild > amv.nBuild))
#endif
   {
      int r;

      wsprintf(lStr, "%d.%d.%d", amvLatest.nMaxima, amvLatest.nMinima, amvLatest.nBuild);

      Amuser_GetLMString( szInfo, "AlreadyAsked", "", lpStr, 199 );
        if(_stricmp(lpStr,lStr) != 0)
      {
         EnsureMainWindowOpen();
         r = fDlgMessageBox( hwndFrame, 0, IDDLG_VERSIONCHECK, "New Versions Available", 30000, IDD_UPGRADENO );
         if( IDD_UPGRADEYES == r )
         {
            // They want to download it now
            wsprintf(lpStr, "cixfile:cix.support/files:a%d%d%d.exe", amvLatest.nMaxima, amvLatest.nMinima, amvLatest.nBuild);
            if( CommonHotspotHandler( hwndFrame, lpStr ) )
            {
               Amuser_WriteLMString( szInfo, "AlreadyAsked", lStr );
            }

         }
         else if ( IDD_UPGRADENEVER == r )
         {
            // They never want to download it
            Amuser_WriteLMString( szInfo, "AlreadyAsked", lStr );
         }
      }
   }
   return FALSE;
}

/* This function makes one call to Windows to check for anything
 * in the message queue. If there is nothing there, it returns
 * FALSE and the Run() function calls WaitMessage to allow other
 * applications the lion's share of the CPU time.
 */
BOOL FASTCALL TaskYield( void )
{
   MSG msg;

   /* Process one message.
    */
   if( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
      {
      /* If we quit, set a global flag
       */
      if( msg.message == WM_QUIT )
         {
         PostQuitMessage( msg.wParam );
         fQuitSeen = TRUE;
         return( FALSE );
         }

      /* Look for keyboard or mouse activities and clear
       * the idle flag. Otherwise set it.
       */
      if( msg.message >= WM_MOUSEFIRST && msg.message <= WM_MOUSELAST ||
          msg.message >= WM_KEYFIRST && msg.message <= WM_KEYLAST )
         SetLastNonIdleTime();

      /* Deal with the message now.
       */
      if( hSearchDlg == NULL || !IsDialogMessage( hSearchDlg, &msg ) )
         if( hFindDlg == NULL || !IsDialogMessage( hFindDlg, &msg ) )
            if( !( hwndToolBarOptions && IsDialogMessage( hwndToolBarOptions, &msg ) ) )
               if( !( hwndStartWiz && IsDialogMessage( hwndStartWiz, &msg ) ) )
                  if( !( hwndStartWizGuide && IsDialogMessage( hwndStartWizGuide, &msg ) ) )
                     if( !( hwndStartWizProf && IsDialogMessage( hwndStartWizProf, &msg ) ) )
                        if( !CustomTranslateAccelerator( hwndFrame, &msg ) && !TranslateMDISysAccel( hwndMDIClient, &msg ) )
                           if( hwndMDIDlg == NULL || !MyIsDialogMessage( hwndMDIDlg, &msg ) )
                              {
                              TranslateMessage( &msg );
                              DispatchMessage( &msg );
                              }
      if( !fParsing )
         return( TRUE );
      }
   if( fParsing && NULL != fpParser )
      {
      fpParser();
      return( TRUE );
      }
   return( FALSE );
}

/* This function sets the date and time when the user last moved
 * the mouse or pressed a key.
 */
void FASTCALL SetLastNonIdleTime( void )
{
   dwLastNonIdleTime = MAKELONG( Amdate_GetPackedCurrentDate(), Amdate_GetPackedCurrentTime() );
}

/* This function intercepts messages for MDI dialogs so we can
 * handle the special cases.
 */
BOOL FASTCALL MyIsDialogMessage( HWND hwndMDIDlg, LPMSG lpmsg )
{
   static BOOL fShiftUp;

   switch( lpmsg->message )
      {
      case WM_DROPFILES: {
/*       char szClassName[ 40 ];
         BOOL fEdit;
         BOOL fBigEdit;

         GetClassName( lpmsg->hwnd, szClassName, sizeof( szClassName ) );
         fEdit = _strcmpi( szClassName, "edit" ) == 0;
         fBigEdit = fEdit && ( GetWindowStyle( lpmsg->hwnd ) & ES_MULTILINE );
         if( fBigEdit )
*/          SendMessage( hwndMDIDlg, WM_DROPFILES, (WPARAM)lpmsg->wParam, (LPARAM)lpmsg->hwnd );

         }
         return( 0L );

      case WM_KEYUP:
         if( VK_SHIFT == lpmsg->wParam )
            {
            if( Editmap_Open( lpmsg->hwnd ) != ETYP_NOTEDIT )
               ToolbarState_CopyPaste();
            fShiftUp = TRUE;
            }
         break;

      case WM_LBUTTONUP:
         if( Editmap_Open( lpmsg->hwnd ) != ETYP_NOTEDIT )
            ToolbarState_CopyPaste();
         break;

      case WM_KEYDOWN: {
         char szClassName[ 40 ];
         BOOL fTreeview;
         BOOL fBigEdit;
         BOOL fButton;
         BOOL fEdit;

         /* Figure out which control we're under.
          */
         GetClassName( lpmsg->hwnd, szClassName, sizeof( szClassName ) );
         fEdit = (_strcmpi( szClassName, "edit" ) == 0) || (_strcmpi( szClassName, "Scintilla" ) == 0) || (_strcmpi( szClassName, WC_BIGEDIT ) == 0);
         fBigEdit = fEdit && ( GetWindowStyle( lpmsg->hwnd ) & ES_MULTILINE );
         fButton = _strcmpi( szClassName, "button" ) == 0;
         fTreeview = _strcmpi( szClassName, WC_TREEVIEW ) == 0;
         if( lpmsg->wParam == VK_TAB && !IsIconic( hwndMDIDlg ) )
            {
            HWND hwndFocus;

            if( GetAsyncKeyState( VK_SHIFT ) & 0x8000 )
               hwndFocus = GetNextDlgTabItem( hwndMDIDlg, lpmsg->hwnd, TRUE );
            else
               {
               if( fBigEdit && !( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) )
                  return( FALSE );
               hwndFocus = GetNextDlgTabItem( hwndMDIDlg, lpmsg->hwnd, FALSE );
               }
            SetFocus( hwndFocus );
            GetClassName( hwndFocus, szClassName, sizeof( szClassName ) );
            fEdit = _strcmpi( szClassName, "edit" ) == 0;
            fBigEdit = fEdit && ( GetWindowStyle( hwndFocus ) & ES_MULTILINE );
            if( fEdit && !fBigEdit )
               Edit_SetSel( hwndFocus, 0, 32767 );
            if( _strcmpi( szClassName, "button" ) == 0 )
               SendMessage( hwndMDIDlg, DM_SETDEFID, GetDlgCtrlID( hwndFocus ), 0L );
            else if( fButton )
               SendMessage( hwndMDIDlg, DM_SETDEFID, IDOK, 0L );
            ToolbarState_CopyPaste();
            return( TRUE );
            }

         /* Handle Cancel by ourselves.
          */
         if( lpmsg->wParam == VK_ESCAPE )
            {
            SendDlgCommand( hwndMDIDlg, IDCANCEL, TRUE );
            return( TRUE );
            }

         /* Handle left/up key for radio buttons.
          */
         if( lpmsg->wParam == VK_LEFT || lpmsg->wParam == VK_UP && !IsIconic( hwndMDIDlg ) )
            if( SendMessage( lpmsg->hwnd, WM_GETDLGCODE, 0, 0L ) & DLGC_RADIOBUTTON )
               {
               HWND hwnd;

               SendMessage( lpmsg->hwnd, BM_SETCHECK, 0, 0L );
               hwnd = lpmsg->hwnd;
               do
                  hwnd = GetNextDlgGroupItem( hwndMDIDlg, hwnd, TRUE );
               while( hwnd != lpmsg->hwnd && !IsWindowEnabled( hwnd ) );
               SetFocus( hwnd );
               SendMessage( hwnd, BM_SETCHECK, 1, 0L );
               return( TRUE );
               }

         /* Handle right/down key for radio buttons.
          */
         if( lpmsg->wParam == VK_RIGHT || lpmsg->wParam == VK_DOWN && !IsIconic( hwndMDIDlg ) )
            if( SendMessage( lpmsg->hwnd, WM_GETDLGCODE, 0, 0L ) & DLGC_RADIOBUTTON )
               {
               HWND hwnd;

               SendMessage( lpmsg->hwnd, BM_SETCHECK, 0, 0L );
               hwnd = lpmsg->hwnd;
               do
                  hwnd = GetNextDlgGroupItem( hwndMDIDlg, hwnd, FALSE );
               while( hwnd != lpmsg->hwnd && !IsWindowEnabled( hwnd ) );
               SetFocus( hwnd );
               SendMessage( hwnd, BM_SETCHECK, 1, 0L );
               return( TRUE );
               }

         /* Handle the RETURN key
          */
         if( lpmsg->wParam == VK_RETURN )
            {
            DWORD dwDef;

            /* If RETURN is pressed on a minimised window, restore it.
             */
            if( IsIconic( hwndMDIDlg ) )
               {
               SendMessage( hwndMDIClient, WM_MDIRESTORE, (WPARAM)hwndMDIDlg, 0L );
               return( TRUE );
               }

            /* Edit controls eat RETURNS for breakfast
             */
            if( fBigEdit || fTreeview )
               break;

            /* Find the default control and send it a command
             * if it is enabled.
             */
            dwDef = SendMessage( hwndMDIDlg, DM_GETDEFID, 0, 0L );
            if( HIWORD( dwDef ) == DC_HASDEFID )
               {
               int id;

               id = (int)LOWORD( dwDef );
               if( IsWindowEnabled( GetDlgItem( hwndMDIDlg, id ) ) )
                  {
                  SendDlgCommand( hwndMDIDlg, id, 1 );
                  return( TRUE );
                  }
               }
            }
         break;
         }

      case WM_SYSCHAR:
         /* Ignore Alt+ keystrokes if window is iconized.
          */
         if( IsIconic( hwndMDIDlg ) )
            return( FALSE );
         if( isalnum( (char)lpmsg->wParam ) && ( HIWORD( lpmsg->lParam ) & 0x2000 ) )
            {
            HWND hwnd;

            for( hwnd = GetWindow( hwndMDIDlg, GW_CHILD ); hwnd; hwnd = GetWindow( hwnd, GW_HWNDNEXT ) )
               {
               char szClassName[ 40 ];
               char sz[ 40 ];
               register int c;

               GetClassName( hwnd, szClassName, sizeof( szClassName ) );
               if( _strcmpi( szClassName, "edit" ) != 0 )
                  if( GetWindowText( hwnd, sz, sizeof( sz ) ) > 0 )
                     for( c = 0; sz[ c ]; ++c ) 
                        if( sz[ c ] == '&' && (sz[ c + 1 ] & 0xDF) == (char)(lpmsg->wParam & 0x00DF) )
                           {
                           int id;
      
                           id = GetDlgCtrlID( hwnd );
                           if( IsWindowEnabled( hwnd ) )
                              {
                              if( SendMessage( hwnd, WM_GETDLGCODE, 0, 0L ) & DLGC_BUTTON )
                                 {
                                 /* Simulate pressing button, or whatever...
                                  */
                                 SetFocus( hwnd );
                                 SendMessage( hwnd, BM_SETSTATE, TRUE, 0L );
                                 SendMessage( hwnd, BM_SETSTATE, FALSE, 0L );
                                 switch( GetWindowStyle( hwnd ) & 0x0F )
                                    {
                                    case BS_AUTORADIOBUTTON: {
                                       HWND hwnd2;

                                       SendMessage( hwnd, BM_SETCHECK, 1, 0L );
                                       hwnd2 = hwnd;
                                       hwnd2 = GetNextDlgGroupItem( hwndMDIDlg, hwnd2, TRUE );
                                       while( hwnd != hwnd2 )
                                          {
                                          SendMessage( hwnd2, BM_SETCHECK, 0, 0L );
                                          hwnd2 = GetNextDlgGroupItem( hwndMDIDlg, hwnd2, TRUE );
                                          }
                                       SendDlgCommand( hwndMDIDlg, id, BN_CLICKED );
                                       break;
                                       }

                                    case BS_AUTOCHECKBOX: {
                                       WPARAM sel;
      
                                       sel = LOWORD( SendMessage( hwnd, BM_GETCHECK, 0, 0L ) ) ? 0 : 1;
                                       SendMessage( hwnd, BM_SETCHECK, sel, 0L );
                                       SendDlgCommand( hwndMDIDlg, id, BN_CLICKED );
                                       break;
                                       }

                                    default:
                                       SendDlgCommand( hwndMDIDlg, id, BN_CLICKED );
                                       break;
                                    }
                                 return( TRUE );
                                 }
                              if( _strcmpi( szClassName, "static" ) == 0 )
                                 {
                                 for( ; hwnd; hwnd = GetWindow( hwnd, GW_HWNDNEXT ) )
                                    {
                                    GetClassName( hwnd, szClassName, sizeof( szClassName ) );
                                    if( _strcmpi( szClassName, "static" ) != 0 )
                                       break;
                                    }
                                 }
                              if( hwnd )
                                 {
                                 SetFocus( hwnd );
                                 if( _strcmpi( szClassName, "edit" ) == 0 )
                                    if( !( GetWindowStyle( hwnd ) & ES_MULTILINE ) )
                                       SendMessage( hwnd, EM_SETSEL, 0, 32767 );
                                 }
                              return( TRUE );
                              }
                           }
               }
            }
         break;
      }
   if( CallMsgFilter( lpmsg, MSGF_DIALOGBOX ) )
      return( TRUE );
   return( FALSE );
}

/* This function returns a flag that specifies whether or
 * not Ameol is quitting.
 */
BOOL EXPORT WINAPI Ameol2_IsAmeolQuitting( void )
{
   return( fQuitting );
}

/* Translate keystroke to the appropriate commands.
 */
BOOL FASTCALL CustomTranslateAccelerator( HWND hwnd, LPMSG lpmsg )
{
   WORD wModifier;
   CMDREC cmd;
   WORD wKey;

   /* If this is a system key AND the toolbar has captured
    * input, release the toolbar.
    */
   if( GetCapture() == hwndToolBar &&
       ( lpmsg->message == WM_SYSKEYDOWN || lpmsg->message == WM_SYSCOMMAND ) )
      SendMessage( hwndToolBar, WM_CANCELMODE, 0, 0L );

   /* Do nothing if this isn't a key down message.
    */
   if( lpmsg->message != WM_KEYDOWN && lpmsg->message != WM_SYSKEYDOWN )
      return( FALSE );

   /* Get the virtual key code. Assign the modifier
    * codes based on the state of the keys.
    */
   wKey = (WORD)lpmsg->wParam;
   wModifier = 0;
   if( GetKeyState( VK_CONTROL ) & 0x8000 )
      wModifier |= HK_CTRL;
   if( GetKeyState( VK_SHIFT ) & 0x8000 )
      wModifier |= HK_SHIFT;
   if( lpmsg->lParam & 0x20000000 )
      wModifier |= HK_ALT;
   wKey = MAKEKEY( wModifier, wKey );

   /* Handle Ctrl+F4
    */
   if( GetCapture() == hwndToolBar && wKey == 0x273 )
      SendMessage( hwndToolBar, WM_CANCELMODE, 0, 0L );

   /* Scan the command table looking for a command
    * matching the specified keystroke.
    */
   cmd.wKey = wKey;
   if( CTree_FindKey( &cmd ) )
      {
      /* Special hack for terminal.
       */
      if( ( wModifier == HK_CTRL || wModifier == HK_SHIFT || wModifier == 0 ) && Ameol2_GetWindowType() == WIN_TERMINAL )
         return( FALSE );
      if( WIN_ALL == cmd.iActiveMode || ( Ameol2_GetWindowType() & cmd.iActiveMode ) != 0 )
         {
         SendCommand( hwnd, cmd.iCommand, 0L );
         return( TRUE );
         }
      }
   return( FALSE );
}

/* This function registers the window classes that will be
 * used by Palantir,
 */
BOOL FASTCALL InitializeApplication( void )
{
   APPWNDCLASS wc;
   int ic16, ic32;

   /* Load the app icon.
    */
   ic32 = fOldLogo ? IDI_APPICON32_OLD : IDI_APPICON32;
   ic16 = fOldLogo ? IDI_APPICON16_OLD : IDI_APPICON16;
   hAppIcon = LoadIcon( hRscLib, MAKEINTRESOURCE(ic32) );

   /* Register the MDI frame window class
    */
   AppWndClass_Init( wc );
   AppWndClass_SetIcon( wc, hAppIcon );
   AppWndClass_SetIcon16( wc, LoadImage( hRscLib, MAKEINTRESOURCE(ic16), IMAGE_ICON, 16, 16, 0 ) );
   AppWndClass_Style( wc, CS_HREDRAW | CS_VREDRAW );
   AppWndClass_WndProc( wc, MainWndProc );
   AppWndClass_ClsExtra( wc, 0 );
   AppWndClass_WndExtra( wc, 0 );
   AppWndClass_Instance( wc, hInst );
   AppWndClass_Cursor( wc, LoadCursor( NULL, IDC_ARROW ) );
   AppWndClass_Background( wc, COLOR_APPWORKSPACE+1 );
   AppWndClass_Menu( wc, NULL );
   AppWndClass_ClassName( wc, szFrameWndClass );
   if( wWinVer < 0x35F )
      {
      /* Under Windows NT 3.51 or earlier, or Windows 3.11 or
       * earlier, call the old function.
       */
      if( !RegisterClass( (WNDCLASS *)&wc.style ) )
         return( FALSE );
      }
   if( wWinVer >= 0x35F )
      {
      /* Under Windows 95 or Windows NT 3.52 or later, call
       * the RegisterClassEx function.
       */
      if( !AppRegisterClass( &wc ) )
         return( FALSE );
      }
   return( TRUE );
}

/* This function performs general initialisation of Palantir.
 */
BOOL FASTCALL InitializeInstance( LPSTR lpszCmdLine, int nCmdShow )
{
   INTERFACE_PROPERTIES ip;
   BOOL fMustRunSetupWizard;
   char szDir[ _MAX_DIR ];
   int nHdrDivisions[ 5 ];
   BOOL fMigrateCookies;
   DWORD dwVersion;
   register int c;
   UINT iScrollLines;
   char * pszDir;
   DWORD dwState;
   RECT rc;

   /* Initialise some useful globals here.
    */
   hEditFont = NULL;
   fInitialising = TRUE;
   fForceNextUnread = FALSE;
   fMigrateCookies = FALSE;

   /* Allocate two buffers; one general purpose one and
    * one for filename expansions.
    */
   if( !fNewMemory( &lpTmpBuf, LEN_TEMPBUF ) )
      {
      ExitAmeol( NULL, 0 );
      return( FALSE );
      }
   if( !fNewMemory( &lpPathBuf, _MAX_PATH + 1 ) )
      {
      ExitAmeol( NULL, 0 );
      return( FALSE );
      }

   /* Get and save the module filename.
    */
   if( fNewMemory( &pszModulePath, _MAX_PATH ) )
      GetModuleFileName( hInst, pszModulePath, _MAX_PATH );

   /* Set the application directory.
    */
   strcpy( szDir, pszModulePath );
   pszDir = strrchr( szDir, '\\' );
   if( NULL != pszDir )
      *++pszDir = '\0';
   pszAmeolDir = CreateDirectoryPath( NULL, szDir ); // VistaAlert

   /* Set the app and user directory.
    */
   Amuser_SetAppDirectory( APDIR_APPDIR, pszAmeolDir );

   /* Read any custom details.
    */
   wsprintf( lpPathBuf, "%s%s", pszAmeolDir, szConfigFile );
   GetPrivateProfileString( szConfig, "AppName", szAppName, szAppName, sizeof( szAppName ), lpPathBuf );
   GetPrivateProfileString( szConfig, "AppLogo", szAppLogo, szAppLogo, sizeof( szAppLogo ), lpPathBuf );
   GetPrivateProfileString( szConfig, "SplashLogo", szSplashLogo, szSplashLogo, sizeof( szSplashLogo ), lpPathBuf );
   GetPrivateProfileString( szConfig, "SplashRGB", szSplashRGB, szSplashRGB, sizeof( szSplashRGB ), lpPathBuf );
   GetPrivateProfileString( szConfig, "FullPhoneNumber", szCIXPhoneNumber, szCIXPhoneNumber, sizeof( szCIXPhoneNumber ), lpPathBuf );
   GetPrivateProfileString( szConfig, "CountryCode", szCountryCode, szCountryCode, sizeof( szCountryCode ), lpPathBuf );
   GetPrivateProfileString( szConfig, "AreaCode", szAreaCode, szAreaCode, sizeof( szAreaCode ), lpPathBuf );
   GetPrivateProfileString( szConfig, "PhoneNumber", szPhoneNumber, szPhoneNumber, sizeof( szPhoneNumber ), lpPathBuf );

   /* Get the custom MDI background bitmap.
    */
   hBmpbackground = NULL;
   GetPrivateProfileString( szConfig, "BackgroundBmp", "", szDir, sizeof(szDir), lpPathBuf );
   if( *szDir )
      {
      hBmpbackground = Amuser_LoadBitmapFromDisk( szDir, &hPalbackground );
      fStretchBmp = GetPrivateProfileInt( szConfig, "StretchBmp", TRUE, lpPathBuf );
      }

   /* Set the current directory. Don't trust the calling
    * shell to do it for us!
    */
   strcpy( szDir, pszModulePath );
   pszDir = strrchr( szDir, '\\' );
   if( NULL != pszDir )
      *pszDir = '\0';
   if( !Amdir_SetCurrentDirectory( szDir ) )
      {
      wsprintf( lpTmpBuf, GS(IDS_STR1054), szDir );
      MessageBox( NULL, lpTmpBuf, NULL, MB_OK|MB_ICONEXCLAMATION );
      ExitAmeol( NULL, 0 );
      return( FALSE );
      }
   ++wInitState;

   /* Get the last build version. If this is not the same as the
    * previous, put up the licence agreement which must be accepted before
    * the user progresses.
    */
   fFirstRun = FALSE;
   Amuser_GetLMString( szInfo, "Version", "", lpTmpBuf, LEN_TEMPBUF );
   if( !*lpTmpBuf )
      {
      dwVersion = 0;
      fFirstRun = TRUE;
      }
   else
      {
      UINT nThisMaxVer;
      UINT nThisMinVer;
      UINT nThisBuild;

      sscanf( lpTmpBuf, "%u.%u.%u", &nThisMaxVer, &nThisMinVer, &nThisBuild );
      dwVersion = MAKEVERSION( nThisMaxVer, nThisMinVer, nThisBuild );
      }
   if( dwVersion != Ameol2_GetVersion() )
   {
      /*!!SM!!*/
      if ( (int)LoadLibrary( "RichEd32.dll" ) >= 32 )
      {
         if( !DialogBox( hInst, MAKEINTRESOURCE(IDDLG_LICENCE), NULL, LicenceProc ) )
         {
            ExitAmeol( NULL, 0 );
            return( FALSE );
         }
      }
      else
      {
         if( !DialogBox( hInst, MAKEINTRESOURCE(IDDLG_LICENCE_ORG), NULL, LicenceProc ) )
         {
            ExitAmeol( NULL, 0 );
            return( FALSE );
         }
      }
   }

   /* If this is a beta release and we've expired, then say so.
    */
   if( IS_BETA )
      {
      WORD w4DaysBeforeExpiryDate;
      WORD wExpiryDate;
      AM_DATE date;

      /* First check for actual expiry.
       */
      date.iDay = BETA_EXPIRY_DAY;
      date.iMonth = BETA_EXPIRY_MONTH;
      date.iYear = BETA_EXPIRY_YEAR;
      wExpiryDate = Amdate_PackDate( &date );
      if( Amdate_GetPackedCurrentDate() >= wExpiryDate )
         {
         MessageBox( NULL, GS(IDS_STR1125), szAppName, MB_OK|MB_ICONINFORMATION );
         ExitAmeol( NULL, 0 );
         return( FALSE );
         }

      /* 7 days before expiry.
       */
      Amdate_AdjustDate( &date, -7 );
      w4DaysBeforeExpiryDate = Amdate_PackDate( &date );
      if( Amdate_GetPackedCurrentDate() >= w4DaysBeforeExpiryDate )
         {
         /* Show a warning.
          */
         date.iDay = BETA_EXPIRY_DAY;
         date.iMonth = BETA_EXPIRY_MONTH;
         date.iYear = BETA_EXPIRY_YEAR;

         /* Hack to compute date.iDayOfWeek.
          */
         Amdate_UnpackDate( Amdate_PackDate( &date ), &date );
         wsprintf( lpTmpBuf, GS(IDS_STR1145), szAppName, Amdate_FormatLongDate( &date, NULL ) );
         MessageBox( NULL, lpTmpBuf, szAppName, MB_OK|MB_ICONINFORMATION );
         }
      else if( dwVersion != Ameol2_GetVersion() )
         {
         /* First time this version, show a warning.
          */
         date.iDay = BETA_EXPIRY_DAY;
         date.iMonth = BETA_EXPIRY_MONTH;
         date.iYear = BETA_EXPIRY_YEAR;

         /* Hack to compute date.iDayOfWeek.
          */
         Amdate_UnpackDate( Amdate_PackDate( &date ), &date );

         /* Show the message.
          */
         wsprintf( lpTmpBuf, GS(IDS_STR1146), szAppName, Amdate_FormatLongDate( &date, NULL ) );
         MessageBox( NULL, lpTmpBuf, szAppName, MB_OK|MB_ICONINFORMATION );
         }
      }

   /* Make this the current version.
    */
   wsprintf( lpTmpBuf, "%u.%2.2u.%3.3u", amv.nMaxima, amv.nMinima, amv.nBuild );
   Amuser_WriteLMString( szInfo, "Version", lpTmpBuf );

   /* Sort out the timezone difference. Because Microsoft think the USA
    * is the center of the Universe, the default timezone if TZ is not
    * specified is PST8PDT (!).
    */
   _tzset();

   /* Read the Users= setting from [Directories] in AMEOL2.INI for the
    * first time.
    */
   if( 0 == Amuser_GetLMString( szDirects, "Users", "", szUsersDir, _MAX_DIR ) ) // VistaAlert
   {
      if (!fFirstRun)
         wsprintf( szUsersDir, "%sUsers", pszAmeolDir );
      else
      {
         // For v2.6 and later, first time run creates the user folder in the app data
         // folder.
      #ifdef USE_HOME_FOLDER
         SHGetSpecialFolderPathA(NULL, szUsersDir, CSIDL_PERSONAL, TRUE);
         strcat(szUsersDir, "\\Ameol\\Users");
      #else
         wsprintf( szUsersDir, "%sUsers", pszAmeolDir );
      #endif
      }
      Amuser_WriteLMString( szDirects, "Users", szUsersDir);
   }

   /* If the user specified Admin mode OR this is a first time run of
    * Ameol on a network, put up the Admin dialog.
    */
   Amuser_SetAppDirectory( APDIR_USER, szUsersDir );

   /* Now open the users list.
    */
   if( !OpenUsersList() )
      {
      MessageBox( NULL, GS(IDS_STR1061), NULL, MB_OK|MB_ICONEXCLAMATION );
      ExitAmeol( NULL, 0 );
      return( FALSE );
      }
   ++wInitState;

   /* Now ensure that we have the login name and password.
    * If not, prompt for it.
    */
   {
      //!!SM!! 2029 restore Ameol window if user already logged on, but only if command line specifies username
      //char szName[255];
      HWND hwndCur;

      hwndCur = CheckFunc(szLoginUsername);                  // 2.55.2032
      if(IsWindow(hwndCur))            
      {
         if (fKillRunning)
         {
            PostMessage(hwndCur,WM_CLOSE,0,0L);
            ExitAmeol( NULL, 0 );
            return( FALSE );
         }
         else
         {
            SetForegroundWindow(hwndCur);
            if(IsMinimized(hwndCur))
               ShowWindow(hwndCur, SW_RESTORE);       
            ExitAmeol( NULL, 0 );
            return( FALSE );
         }
      }
      else
      {
         if( !Login( szLoginUsername, szLoginPassword ) )
         {
            ExitAmeol( NULL, 0 );
            return( FALSE );
         }
      }
   }
   strcpy( szLoginUsername, GetActiveUsername() );
   if( !IsAmeol2Loaded( szLoginUsername ) )
   {
      char szName[255];

      wsprintf(szName, "%s - %s", szLoginUsername, szAppName);
      ShowWindow(FindWindow("amctl_frame",   szName), SW_SHOW);
   #ifdef USEMUTEX // 2.55.2031
      ReleaseMutex( InstanceMutexHandle );
   #endif USEMUTEX // 2.55.2031
      return( FALSE );
   }

   /* Get the version number under which this user was last setup.
    * We'll need it to determine whether to run Setup again.
    */
   Amuser_GetPPString( szPreferences, "Version", "", lpTmpBuf, LEN_TEMPBUF );
   if( !*lpTmpBuf )
      {
      fMustRunSetupWizard = TRUE;
      fFirstTimeThisUser = TRUE;
      }
   else
      {
      UINT nThisMaxVer;
      UINT nThisMinVer;
      UINT nThisBuild;

      sscanf( lpTmpBuf, "%u.%u.%u", &nThisMaxVer, &nThisMinVer, &nThisBuild );
      dwVersion = MAKEVERSION( nThisMaxVer, nThisMinVer, nThisBuild );
      if( dwVersion != Ameol2_GetVersion() )
         {
         wsprintf( lpTmpBuf, "%u.%2.2u.%3.3u", amv.nMaxima, amv.nMinima, amv.nBuild );
         Amuser_WritePPString( szPreferences, "Version", lpTmpBuf );
         }
      if( nThisBuild < 1824 )
         fMigrateCookies = TRUE;
      if( nThisBuild >= MIN_VALID_SETUP_BUILD )
         fMustRunSetupWizard = fSetup;
      else if( nThisBuild < 1832 )
         {
         /* Upgrading from 2.01 to 2.10 requires us to re-run the
          * Setup Wizard again AND delete their configuration.
          */
         fMustRunSetupWizard = FALSE;
         }
      else
         {
         fMessageBox( NULL, 0, GS(IDS_STR1047), MB_OK|MB_ICONINFORMATION );
         fMustRunSetupWizard = TRUE;
         }
      }

   /* Now show introduction.
    */
   if( !fNoLogo && !fMustRunSetupWizard )
      ShowIntroduction();

   /* Load cursors. If any of the cursors failed to load,
    * we exit now.
    */
   hWaitCursor = LoadCursor( NULL, IDC_WAIT );
   if( hWaitCursor == NULL )
      {
      MessageBox( NULL, GS(IDS_STR7), szAppName, MB_OK|MB_ICONEXCLAMATION );
      ExitAmeol( NULL, 0 );
      return( FALSE );
      }
   ++wInitState;

   /* Get the message window info bar height.
    */
   cyInfoBar = GetSystemMetrics( SM_CYCAPTION ) - 1;

   /* Load the standard menus. If any of them failed to load,
    * we exit now.
    */
   hMainMenu = LoadMenu( hRscLib, MAKEINTRESOURCE( IDMNU_MAIN ) );
   hPopupMenus = LoadMenu( hRscLib, MAKEINTRESOURCE( IDMNU_POPUPS ) );
   if( hMainMenu == NULL || hPopupMenus == NULL )
      {
      HideIntroduction();
      MessageBox( NULL, GS(IDS_STR6), szAppName, MB_OK|MB_ICONEXCLAMATION );
      ExitAmeol( NULL, 0 );
      return( FALSE );
      }     
   ++wInitState;

   /* If MAPI enabled, add Send Mail... to the File menu.
    */
   if( GetProfileInt( "mail", "mapi", 0 ) == 1 )
      {
      HMENU hFileMenu;
      int count;

      /* Add it to the Mail menu.
       */
      hFileMenu = GetSubMenu( hMainMenu, 0 ); 
      count = GetMenuItemCount( hFileMenu );
      InsertMenu( hFileMenu, count - 2, MF_BYPOSITION|MF_SEPARATOR, 0, NULL );
      InsertMenu( hFileMenu, count - 1, MF_BYPOSITION, IDM_MAPISEND, GS(IDS_STR309) );
      }
   ++wInitState;

   /* Fill out the wPopupIDs array with the handle of the popup menus
    * for all the menus.
    */
   for( c = 0; wMainPopupIDs[ c ].wID; ++c )
      wMainPopupIDs[ c ].hpopupMenu = GetSubMenu( hMainMenu, c );

   /* Get the current printer state.
    */
   UpdatePrinterSelection( FALSE );
   PageSetupInit();
   wInitState = 7;

   /* Register the FindReplace message.
    */
   uFindReplaceMsg = RegisterWindowMessage( FINDMSGSTRING );

   /* Register a window message for RAS callback events.
    */
   if( 0 == ( uRasMsg = RegisterWindowMessageA( RASDIALEVENT ) ) )
      uRasMsg = WM_RASDIALEVENT;

   /* Add Intellimouse support.
    */
   hwndWheel = FindWindow( MSH_WHEELMODULE_CLASS, MSH_WHEELMODULE_TITLE );
   if( NULL != hwndWheel )
      HwndMSWheel( &msgMouseWheel, &msg3DSupport, &msgScrollLines, &f3DSupport, &iScrollLines );
   else
      SystemParametersInfo( SPI_GETWHEELSCROLLLINES, 0, &iScrollLines, 0 );
   Amctl_SetControlWheelScrollLines( iScrollLines );

   /* If this is a clean install, disable CIX Mail.
    */
   fUseLegacyCIX = Amuser_GetPPInt( szCIXIP, "UseCIXMail", TRUE );
   if (fFirstTimeThisUser)
   {
      fUseLegacyCIX = FALSE;
      Amuser_WritePPInt( szCIXIP, "UseCIXMail", FALSE );
   }

   /* By now we know if we're using CIX legacy services or not, so adjust the
    * blink flag values.
    */
   if (!fUseLegacyCIX)
   {
      BF_POSTCIX &= ~(BF_POSTCIXMAIL|BF_POSTCIXNEWS);
      BF_GETCIX &= ~(BF_GETCIXMAIL|BF_GETCIXNEWS|BF_GETCIXTAGGEDNEWS);
      BF_CIXFLAGS = BF_POSTCIX|BF_GETCIX|BF_CIXRECOVER|BF_STAYONLINE;
      BF_FULLCONNECT = BF_POSTCIX|BF_GETCIX|BF_IPFLAGS;
      BF_EVERYTHING = BF_CIXFLAGS|BF_IPFLAGS;
   }

   /* Some necessary flags for the message view mode and split bar position.
    */
   fRulesEnabled = Amuser_GetPPInt( szSettings, "RulesEnabled", TRUE );
   fDoBlinkAnimation = Amuser_GetPPInt( szSettings, "BlinkAnimation", TRUE );
   fShowTopicType = Amuser_GetPPInt( szSettings, "ShowFolderType", TRUE );
   fUseCopiedMsgSubject = Amuser_GetPPInt( szSettings, "UseCopiedMsgSubject", TRUE );
   fShowClock = Amuser_GetPPInt( szSettings, "ShowClock", TRUE );
   fShortHeaders = Amuser_GetPPInt( szSettings, "ShortHeaders", TRUE );
   fShortHeadersSubject = Amuser_GetPPInt( szSettings, "ShortHeadersSubject", FALSE );
   fShortHeadersFrom = Amuser_GetPPInt( szSettings, "ShortHeadersFrom", TRUE );
   fShortHeadersTo = Amuser_GetPPInt( szSettings, "ShortHeadersTo", TRUE );
   fShortHeadersCC = Amuser_GetPPInt( szSettings, "ShortHeadersCC", TRUE );
    fShortHeadersDate = Amuser_GetPPInt( szSettings, "ShortHeadersDate", TRUE );
   nMailDelivery = Amuser_GetPPInt( szSettings, "MailDelivery", MAIL_DELIVER_IP );
   fShowMugshot = Amuser_GetPPInt( szSettings, "ShowMugshots", TRUE );
   fStretchMugshot = Amuser_GetPPInt( szSettings, "StretchMugshots", TRUE );
   nMaxToArchive = Amuser_GetPPInt( szSettings, "MaxToArchive", 9 );
   nMugshotLatency = Amuser_GetPPInt( szSettings, "MugshotLatency", 5 );
   fDrawThreadLines = Amuser_GetPPInt( szSettings, "ThreadLines", TRUE );
   fDeleteMailAfterReading = Amuser_GetPPInt( szSettings, "DeleteMailAfterReading", FALSE );
   fEditAtTop = Amuser_GetPPInt( szSettings, "EditAtTop", FALSE );
   fPOP3Last = Amuser_GetPPInt( szSettings, "POP3Last", FALSE );
   fBeepAtEnd = Amuser_GetPPInt( szSettings, "BeepAtEnd", TRUE );
   fLaptopKeys = Amuser_GetPPInt( szSettings, "SwapPageKeyFunction", TRUE );
   fStripHeaders = Amuser_GetPPInt( szSettings, "ShowHeaders", TRUE );
   fAutoCheck = Amuser_GetPPInt( szSettings, "AutoCheck", FALSE );
   fMsgStyles = Amuser_GetPPInt( szSettings, "ShowMessageStyles", TRUE );
   fShowTips = Amuser_GetPPInt( szSettings, "ShowTips", FALSE );
   fWordWrap = Amuser_GetPPInt( szSettings, "ShowWordWrap", TRUE );
   fHotLinks = Amuser_GetPPInt( szSettings, "ShowHotLinks", TRUE );
   fAutoCollapse = Amuser_GetPPInt( szSettings, "AutoCollapseFolders", TRUE );
   fWarnOnExit = Amuser_GetPPInt( szSettings, "WarnOnExit", FALSE );
   fWarnAfterBlink = Amuser_GetPPInt( szSettings, "WarnAfterBlink", TRUE );
   fAlertOnFailure = Amuser_GetPPInt( szSettings, "AlertOnFailure", TRUE );
   iWrapCol = Amuser_GetPPInt( szSettings, "WordWrap", RIGHT_MARGIN );
   irWrapCol = Amuser_GetPPInt( szSettings, "WordWrapRead", RIGHT_MARGIN + 10 );
   inWrapCol = Amuser_GetPPInt( szSettings, "WordWrapNews", RIGHT_MARGIN ); // 2.56.2055
   imWrapCol = Amuser_GetPPInt( szSettings, "WordWrapMail", RIGHT_MARGIN ); // 2.56.2055
   iTabSpaces = Amuser_GetPPInt( szSettings, "TabSpaces", 8 ); //FS#80
   fSeparateEncodedCover = Amuser_GetPPInt( szSettings, "AttachmentCoverPage", FALSE );
   fSplitParts = Amuser_GetPPInt( szSettings, "SplitAttachment", FALSE );
   fAttachmentSizeWarning = Amuser_GetPPInt( szSettings, "AttachmentSizeWarning", TRUE );
   fAttachmentConvertToShortName = Amuser_GetPPInt( szSettings, "AttachmentConvertToShortName", TRUE );
   fCheckForAttachments = Amuser_GetPPInt( szSettings, "CheckForAttachments", TRUE ); // !!SM!! 2.55.2033
   dwLinesMax = Amuser_GetPPLong( szSettings, "AttachmentPartMax", 1000 );
   fLaunchAfterDecode = Amuser_GetPPInt( szSettings, "AutoLaunch", FALSE );
   fPromptForFilenames = Amuser_GetPPInt( szSettings, "FilenamePrompt", TRUE );
   fPhysicalDelete = Amuser_GetPPInt( szSettings, "DeleteIsEvil", FALSE );
   wDisconnectRate = (UINT)Amuser_GetPPLong( szSettings, "OnlineTimeoutRate", 60000 );
   fDisconnectTimeout = Amuser_GetPPInt( szSettings, "OnlineTimeout", TRUE );
   fColourQuotes = Amuser_GetPPInt( szSettings, "ColourQuotedText", TRUE );
   fShowBlinkWindow = Amuser_GetPPInt( szSettings, "ShowBlinkWindow", TRUE );
   fAutoIndent = Amuser_GetPPInt( szSettings, "AutoIndent", TRUE );
   fSecureOpen = Amuser_GetPPInt( szSettings, "SecureIcon", FALSE );
   fCopyRule = Amuser_GetPPInt( szSettings, "EnableCopyRule", FALSE );
   fParseDuringBlink = Amuser_GetPPInt( szSettings, "ParseDuringBlink", FALSE );
   fUnreadAfterBlink = Amuser_GetPPInt( szSettings, "ShowUnreadAfterBlink", TRUE );
   fAutoDecode = Amuser_GetPPInt( szSettings, "AutoDecode", FALSE );
   fStripHTML = Amuser_GetPPInt( szSettings, "StripHTML", FALSE );
   fReplaceAttachmentText = Amuser_GetPPInt( szSettings, "ReplaceAttachmentText", TRUE );
   fBackupAttachmentMail = Amuser_GetPPInt( szSettings, "BackupAttachmentMail", FALSE );
   fCCMailToSender = Amuser_GetPPInt( szSettings, "CCToSender", TRUE );
   fThreadMailBySubject = Amuser_GetPPInt( szSettings, "ThreadMailBySubject", FALSE );
   fProtectMailFolders = Amuser_GetPPInt( szSettings, "ProtectMailFolders", TRUE );
   fNewMailAlert = Amuser_GetPPInt( szSettings, "NewMailAlert", FALSE );
   fBlankSubjectWarning = Amuser_GetPPInt( szSettings, "BlankSubjectWarning", TRUE );
   fWorkspaceScrollbars = Amuser_GetPPInt( szSettings, "WorkspaceScrollBars", FALSE );
   fAltMsgLayout = Amuser_GetPPInt( szSettings, "AltMsgLayout", FALSE );
   fNoisy = Amuser_GetPPInt( szSettings, "MakeNoises", FALSE );
   fMultiLineTabs = Amuser_GetPPInt( szSettings, "MultiLineTabs", FALSE );
   fUseMButton = Amuser_GetPPInt( szSettings, "UnreadOnMiddleButton", FALSE );
   fFileDebug = Amuser_GetPPInt( szSettings, "FileDebug", FALSE );
   fNewButtons = Amuser_GetPPInt( szPreferences, "NewButtons", FALSE );
   fGetMailDetails = Amuser_GetPPInt( szSettings, "GetMailDetails", TRUE );
   wDefEncodingScheme = (WORD)Amuser_GetPPInt( szSettings, "EncodingScheme", fUseCIX ? ENCSCH_UUENCODE : ENCSCH_MIME );
   fMaxNewsgroups = Amuser_GetPPLong( szSettings, "MaxNewsgroups", 32000 );
   fAntiSpamCixnews = Amuser_GetPPInt( szSettings, "AntiSpamCixnews", FALSE );
   fRFC2822 = Amuser_GetPPInt( szSettings, "RFC2822", TRUE ); //!!SM!! 2.55.2035

   /* Lexer Specific Settings
    */
   fSingleClickHotspots = Amuser_GetPPInt( "Settings", "SingleClickHotspots", TRUE );
    fShowLineNumbers     = Amuser_GetPPInt( "Settings", "ShowLineNumbers", FALSE );
   fMultiLineURLs       = Amuser_GetPPInt( "Settings", "MultiLineURLs", FALSE );
   fShowWrapCharacters  = Amuser_GetPPInt( "Settings", "ShowWrapCharacters", FALSE );
   fShowAcronyms        = Amuser_GetPPInt( "Settings", "ShowAcronyms", FALSE );
   fBreakLinesOnSave    = Amuser_GetPPInt( "Settings", "BreakLinesOnSave", FALSE );
   fPageBeyondEnd       = Amuser_GetPPInt( "Settings", "PageBeyondEnd", TRUE ); // FS#57

   if( wDisconnectRate > 60000 )
      wDisconnectRate = 60000;
   iMaxSubjectSize = 240; /* magic number, derived from: 256 - len of largest header static text ('Newsgroups: ")  - 1 (null)  - 1 for fudge factor */

   /* Some options shouldn't override the command line
    */
   if( !fStartOnline )
      fStartOnline = Amuser_GetPPInt( szSettings, "StartOnline", FALSE );
   if( fNoCheck )
      fAutoCheck = FALSE;

   /* Browser.
    */
   fBrowserIsMSIE = Amuser_GetPPInt( szWWW, "MSIE", TRUE );
   if( !fBrowserIsMSIE )
      {
      Amuser_GetPPString( szWWW, "Path", "", szBrowser, _MAX_PATH );
      if( *szBrowser == '\0' )
         {
         FindDefaultBrowser();
         Amuser_WritePPString( szWWW, "Path", szBrowser );
         }
      }

   /* Default user 
    */
   Amuser_GetLMString( szInfo, "DefaultUser", "", lpTmpBuf, LEN_TEMPBUF );
   if( fFirstTimeThisUser && !*lpTmpBuf )
      Amuser_WriteLMString( szInfo, "DefaultUser", GetActiveUsername() );

   /* Quote stuff.
    */
   Amuser_GetPPString( szSettings, "DateFormat", szDefDateFormat, szDateFormat, sizeof(szDateFormat) );
   Amuser_GetPPString( szSettings, "QuoteDelimiters", szDefQuoteDelim, szQuoteDelimiters, LEN_QDELIM );
   Amuser_GetPPString( szSettings, "QuoteHeader", szDefQuote, szQuoteNewsHeader, sizeof(szQuoteNewsHeader) );
   Amuser_GetPPString( szSettings, "QuoteMailHeader", szDefQuote, szQuoteMailHeader, sizeof(szQuoteMailHeader) );
   fAutoQuoteNews = Amuser_GetPPInt( szSettings, "AutoQuote", TRUE );
   fAutoQuoteMail = Amuser_GetPPInt( szSettings, "AutoQuoteMail", FALSE );
   chQuote = (char)Amuser_GetPPInt( szSettings, "QuoteCharacter", '>' );

   /* Get logging information.
    */
   fLogSMTP = Amuser_GetPPInt( szDebug, "LogSMTP", fLogSMTP );
   fLogNNTP = Amuser_GetPPInt( szDebug, "LogNNTP", fLogNNTP );
   fLogPOP3 = Amuser_GetPPInt( szDebug, "LogPOP3", fLogPOP3 );
   fDisplayLockCount = Amuser_GetPPInt( szDebug, "ShowLockCount", FALSE );
   Amuser_WritePPInt( "Settings", "UseOldBitmaps", fOldLogo );

   hSciLexer = AmLoadLibrary("SciLexer.DLL");
   hRegLib = AmLoadLibrary( "REGEXP.DLL" ) ; // !!SM!! 2039
   hWebLib = AmLoadLibrary( "WEBAPI.DLL" ) ; // !!SM!! 2053

   if (hRegLib < (HINSTANCE)32)
      MessageBox( NULL, "Error Loading RegExp.DLL", szAppName, MB_OK|MB_ICONEXCLAMATION );

   /* Initialise the spell checker
    */
   fUseDictionary = FindLibrary( szSSCELib );
   if( fUseDictionary )
      {
      BOOL fPreloadDict;

      fPreloadDict = _strcmpi( szDictLoadMode, "preload" ) == 0;
      fPreloadDict = Amuser_GetPPInt( szSpelling, "Dictionary", fPreloadDict );
      InitSpelling();
      if( fPreloadDict )
         LoadDictionaries();
      }
   ++wInitState;

   /* Validate the directory names. If any are invalid, warn the user, but
    * replace the invalid names with the defaults anyway.
    * BUGBUG: No validation of returned values!
    */
   pszDataDir = ReadDirectoryPath( APDIR_DATA, "Data" );

   /* Quit if data directory is NULL, means we couldn't load or create a database.
    */
   if( pszDataDir == NULL )
      return( FALSE );
   pszDownloadDir = ReadDirectoryPath( APDIR_DOWNLOADS, "Download" );
   pszAttachDir = ReadDirectoryPath( APDIR_ATTACHMENTS, "Attach" );
   pszArchiveDir = ReadDirectoryPath( APDIR_ARCHIVES, "Archive" );
   pszUploadDir = ReadDirectoryPath( APDIR_UPLOADS, "Upload" );
   pszAddonsDir = ReadDirectoryPath( APDIR_ADDONS, "Addons" );
   pszScriptDir = ReadDirectoryPath( APDIR_SCRIPTS, "Script" );
   pszResumesDir = ReadDirectoryPath( APDIR_RESUMES, "Resumes" );
   pszScratchDir = ReadDirectoryPath( APDIR_SCRATCHPADS, "Scratch" );
   pszMugshotDir = ReadDirectoryPath( APDIR_MUGSHOTS, "Mugshot" );
   
   /* Is this OS limited to short filenames?
    */
   TestForShortFilenames();

   /* Load file transfer protocols.
    */
   InstallAllFileTransferProtocols();

   /* Now check the data subdirectories.
    */
   CheckDSD();

   /* Now we can safely report to the event log.
    */
   wsprintf( lpTmpBuf, GS(IDS_STR535), (LPSTR)GetActiveUsername() );
   Amevent_AddEventLogItem( ETYP_MESSAGE|ETYP_INITTERM, lpTmpBuf );

   /* Create the colours and fonts.
    */
   InitColourSettings();
   CreateAmeolSystemFonts();
   CreateAmeolFonts( FALSE );
   ++wInitState;

   /* Make sure we have a mugshot index.
    */
   EnsureMugshotIndex();

   /* Run Setup Wizard if we must.
    */
   if( fMustRunSetupWizard )
      {
      HideIntroduction();
      if( !SetupWizard( NULL ) )
         {
         ExitAmeol( NULL, 0 );
         return( FALSE );
         }
      }

   /* Time to set the data directory.
    */
   Amdb_SetDataDirectory( pszDataDir );

   /* Now get the settings.
    */
   fUseInternet = Amuser_GetPPInt( szCIXIP, "UseInternet", FALSE );
   nIPMailClearCount = Amuser_GetPPInt( szCIXIP, "MailClearCount", 3 );
   Amuser_GetPPString( szCIXIP, "NewsServer", szDefNews, szNewsServer, sizeof(szNewsServer) );
   Amuser_GetPPString( szCIXIP, "MailServer", szDefMail, szMailServer, sizeof(szMailServer) );
   Amuser_GetPPString( szCIXIP, "SMTPMailServer", szDefMail, szSMTPMailServer, sizeof(szSMTPMailServer) );
   Amuser_GetPPString( szCIXIP, "MailLogin", "", szMailLogin, sizeof(szMailLogin) );
   Amuser_GetPPString( szCIXIP, "MailAddress", "", szMailAddress, sizeof(szMailAddress) );
   Amuser_GetPPString( szCIXIP, "MailReplyAddress", szMailAddress, szMailReplyAddress, sizeof(szMailReplyAddress) );
   Amuser_GetPPString( szCIXIP, "MailFullName", "", szFullName, sizeof(szFullName) );
   Amuser_GetPPString( szCIXIP, "Domain", "", szDomain, sizeof(szDomain) );
   Amuser_GetPPString( szCIXIP, "Provider", "Internet", szProvider, sizeof(szProvider) );
   Amuser_GetPPString( szCIXIP, "Hostname", "", szHostname, sizeof(szHostname) );
   Amuser_GetPPString( szCIXIP, "SMTPMailLogin", "", szSMTPMailLogin, sizeof(szSMTPMailLogin) );
   Amuser_GetPPPassword( szCIXIP, "SMTPMailPassword", "", szSMTPMailPassword, sizeof(szSMTPMailPassword) );

   /* If we don't use CIXIP, news server is cixnews
    */
   if( !fUseInternet )
      strcpy( szNewsServer, szCixnews );

   /* Get RAS parameters (Win32 only)
    */
   rdDef.fUseRAS = Amuser_GetPPInt( szCIXIP, "Use RAS", FALSE );
   Amuser_GetPPString( szCIXIP, "RAS Entry Name", "", rdDef.szRASEntryName, sizeof(rdDef.szRASEntryName) );
   Amuser_GetPPString( szCIXIP, "RAS User Name", "", rdDef.szRASUserName, sizeof(rdDef.szRASUserName) );
   Amuser_GetPPPassword( szCIXIP, "RAS Password", "", rdDef.szRASPassword, sizeof(rdDef.szRASPassword) );

   /* Get the mail password and decode it.
    */
   fRememberPassword = Amuser_GetPPInt( szCIXIP, "RememberPassword", TRUE );
   if( fRememberPassword )
      Amuser_GetPPPassword( szCIXIP, "MailPassword", "", szMailPassword, LEN_PASSWORD );
   else
      *szMailPassword = '\0';

   /* Get CIX Conferencing details
    */
   fUseCIX = Amuser_GetPPInt( szCIX, "UseCIX", FALSE );
   fCIXViaInternet = Amuser_GetPPInt( szCIX, "CIXviaInternet", TRUE );
   Amuser_GetPPString( szCIX, "Nickname", "", szCIXNickname, sizeof(szCIXNickname) );
   Amuser_GetPPPassword( szCIX, "Password", "", szCIXPassword, LEN_PASSWORD );
   Amuser_GetPPString( szCIX, "ConnectionCard", "", szCIXConnCard, sizeof(szCIXConnCard) );
   Amuser_GetPPString( szCIX, "Protocol", "", szCixProtocol, sizeof(szCixProtocol) );
   if( szCixProtocol[ 0 ] == '\0' )
   {
      wsprintf( lpPathBuf, "%snwzmod32.afp", pszAmeolDir );
      if( Amfile_QueryFile( lpPathBuf ) )
         Amuser_GetPPString( szCIX, "Protocol", "ZModem II", szCixProtocol, sizeof(szCixProtocol) );
      else
         Amuser_GetPPString( szCIX, "Protocol", "ZMODEM", szCixProtocol, sizeof(szCixProtocol) );
   }
   fArcScratch = Amuser_GetPPInt( szCIX, "CompressScratchpad", FALSE );
   fGetParList = Amuser_GetPPInt( szCIX, "GetParticipantsList", TRUE );
   fOpenTerminalLog = Amuser_GetPPInt( szCIX, "CreateConnectLog", TRUE );
   dwMaxMsgSize = Amuser_GetPPLong( szCIX, "MaxMessageSize", 65535 );
   iRecent = Amuser_GetPPInt( szCIX, "Recent", 100 );
   nCIXMailClearCount = Amuser_GetPPInt( szCIX, "MailClearCount", 3 );
   fViewCixTerminal = Amuser_GetPPInt( szCIX, "CixTerminal", TRUE );

   /* Get the default e-mail domain.
    */
   Amuser_GetPPString( szSettings, "DefaultMailDomain", "", szDefMailDomain, sizeof(szDefMailDomain) );

   /* Adjust mail delivery depending on service.
    *
    * BUGFIX ID 121: Now if it's CIX Only installation or an Internet Only installation
    * and MAIL_DELIVER_AI is set, then it is reset to MAIL_DELIVER_CIX or
    * MAIL_DELIVER_IP depending on which it is. This is because previously
    * Mail that was intended for cix from an IP only installation was not
    * being sent because MAIL_DELIVER_AI means that cix mail will be sent
    * only via cix (which is not available.
    *
    * YH 21/05/96 for build 1651
   if( !fUseInternet )
      nMailDelivery = MAIL_DELIVER_CIX;
   if( !fUseCIX )
      nMailDelivery = MAIL_DELIVER_IP;
    */

   /* Make sure the default mail domain is valid.
    */
   if( !*szDefMailDomain )
      if( !fUseCIX )
         strcpy( szDefMailDomain, szSMTPMailServer );
      else
         strcpy( szDefMailDomain, szCixCoUk );

   /* Get the users account details. If they are not present, work
    * out what it is.
    */
   Amuser_GetPPString( szPreferences, "Account Type", "", szAccountName, sizeof(szAccountName) );
   if( *szAccountName == '\0' )
      {
      if( !fUseInternet )
         strcpy( szAccountName, GS(IDS_STR1150) );
      else if( fUseInternet && strcmp( szProvider, "CIX Internet" ) == 0 )
         strcpy( szAccountName, GS(IDS_STR1152) );
      else
         strcpy( szAccountName, GS(IDS_STR1151) );
      Amuser_WritePPString( szPreferences, "Account Type", szAccountName );
      }

   /* Create the default categories.
    */
   CTree_AddCategory( CAT_FILE_MENU, 0, "File" );
   CTree_AddCategory( CAT_EDIT_MENU, 1, "Edit" );
   CTree_AddCategory( CAT_VIEW_MENU, 2, "View" );
   CTree_AddCategory( CAT_TOPIC_MENU, 3, "Folder" );
   CTree_AddCategory( CAT_MESSAGE_MENU, 4, "Message" );
   CTree_AddCategory( CAT_SETTINGS_MENU, 5, "Settings" );
   CTree_AddCategory( CAT_WINDOW_MENU, 6, "Window" );
   CTree_AddCategory( CAT_HELP_MENU, 7, "Help" );
   CTree_AddCategory( CAT_KEYBOARD, 8, "Keyboard" );
   CTree_AddCategory( CAT_SPARE, 9, "Spare" );

   /* Load the services.
    */
   LoadCommon();
   if( fUseCIX )
      LoadCIX();
   if( fUseInternet )
      LoadCIXIP();
   LoadLocal();

   ++wInitState;

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
         ExitAmeol( NULL, 0 );
         return( FALSE );
         }
      }

   /* Delete some commands if we're not debugging.
    */
   if( !fDebugMode )
      {
      CTree_DeleteCommand( IDM_BUILD );
      DeleteMenu( GetSubMenu( hMainMenu, 5 ), IDM_BUILD, MF_BYCOMMAND );
      CTree_DeleteCommand( IDM_REBUILD_INDEX );
      DeleteMenu( GetSubMenu( hMainMenu, 5 ), IDM_REBUILD_INDEX, MF_BYCOMMAND );
//    CTree_DeleteCommand( IDM_COPYALL );
//    DeleteMenu( GetSubMenu( hMainMenu, 1 ), IDM_COPYALL, MF_BYCOMMAND );
      }

   /* Load the address book now.
    */
   Amaddr_Load();

   /* Get the frame window position settings.
    */
   GetClientRect( GetDesktopWindow(), &rc );
   dwState = WS_MAXIMIZE;

   /* Get the actual window size, position and state.
    */
   Amuser_ReadWindowState( "Main", &rc, &dwState );
   if( dwState == WS_MAXIMIZE )
      nCmdShow = SW_SHOWMAXIMIZED;
   if( dwState == WS_MINIMIZE )
      nCmdShow = SW_SHOWMINIMIZED;

   /* Initialise Blink Manager.
    */
   LoadBlinkList();

   /* Install forms, if any.
    */
   InstallMailForms();

   /* Create the main window.
    * Under Windows 95 or WinNT 4.0, show the username before the app
    * name so that multiple instances show up logically on the taskbar.
    */
   CreateAppCaption( lpTmpBuf, GetActiveUsername() );
   hwndFrame = CreateWindow( szFrameWndClass,
                             lpTmpBuf,
                             WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
                             rc.left, rc.top,
                             rc.right - rc.left,
                             rc.bottom - rc.top,
                             NULL,
                             ( fBasicMode ? NULL : hMainMenu ),
                             hInst,
                             NULL );
   if( !hwndFrame )
      {
      HideIntroduction();
      MessageBox( NULL, GS( IDS_STR10 ), szAppName, MB_OK|MB_ICONEXCLAMATION );
      ExitAmeol( NULL, 0 );
      return( FALSE );
      }
   if( NULL != hBmpbackground )
      lpfMdiClientProc = SubclassWindow( hwndMDIClient, MdiClientWndProc );
   ShowWindow( hwndFrame, nCmdShow );
   ShowWindow( hwndMDIClient, SW_SHOW );

   UpdateWindow( hwndFrame );

   /* Now set the interface properties.
    */
   ip.hwndFrame = hwndFrame;
   ip.hwndMDIClient = hwndMDIClient;
   Amuser_SetInterfaceProperties( &ip );

   /* Register events that we're interested in.
    */
   Amuser_RegisterEvent( AE_CLOSEDATABASE, MainWndEvents );
   Amuser_RegisterEvent( AE_DELETECATEGORY, MainWndEvents );
   Amuser_RegisterEvent( AE_DELETEFOLDER, MainWndEvents );
   Amuser_RegisterEvent( AE_DELETETOPIC, MainWndEvents );
   Amuser_RegisterEvent( AE_DELETEMSG, MainWndEvents );
   Amuser_RegisterEvent( AE_NEWCATEGORY, MainWndEvents );
   Amuser_RegisterEvent( AE_NEWFOLDER, MainWndEvents );
   Amuser_RegisterEvent( AE_NEWTOPIC, MainWndEvents );
   Amuser_RegisterEvent( AE_NEWMSG, MainWndEvents );
   Amuser_RegisterEvent( AE_TOPIC_CHANGED, MainWndEvents );
   Amuser_RegisterEvent( AE_FOLDER_CHANGED, MainWndEvents );
   Amuser_RegisterEvent( AE_CATEGORY_CHANGED, MainWndEvents );
   Amuser_RegisterEvent( AE_MDI_WINDOW_ACTIVATED, MainWndEvents );
   Amuser_RegisterEvent( AE_CONTEXTSWITCH, MainWndEvents );
   Amuser_RegisterEvent( AE_UNREADCHANGED, MainWndEvents );
   Amuser_RegisterEvent( AE_INSERTING_OUTBASKET_OBJECT, MainWndEvents );
   Amuser_RegisterEvent( AE_POPUP, MainWndEvents );
   Amuser_RegisterEvent( AE_INSERTED_OUTBASKET_OBJECT, MainWndEvents );
   Amuser_RegisterEvent( AE_DELETED_OUTBASKET_OBJECT, MainWndEvents );
   Amuser_RegisterEvent( AE_MOVEFOLDER, MainWndEvents );
   Amuser_RegisterEvent( AE_MOVETOPIC, MainWndEvents );
   Amuser_RegisterEvent( AE_MOVECATEGORY, MainWndEvents );

   /* Get and set the default viewdata info.
    */
   vdDef.nSortMode = (BYTE)Amuser_GetPPInt( szSettings, "SortMode", VSM_REFERENCE|VSM_ASCENDING );
   vdDef.nViewMode = (BYTE)( ( vdDef.nSortMode & 0x7F ) == VSM_REFERENCE ) ? VM_VIEWREF : ( ( vdDef.nSortMode & 0x7F ) == VSM_ROOTS ) ? VM_VIEWROOTS: VM_VIEWCHRON;
   vdDef.wHeaderFlags = (WORD)Amuser_GetPPInt( szSettings, "HeaderFlags", VH_MSGNUM|VH_SENDER|VH_SUBJECT );
   vdDef.nThreadIndentPixels = (WORD)Amuser_GetPPInt( szSettings, "ThreadIndentPixels", 6 );
   nHdrDivisions[ 0 ] = 120;
   nHdrDivisions[ 1 ] = 200;
   nHdrDivisions[ 2 ] = 136;
   nHdrDivisions[ 3 ] = 46;
   nHdrDivisions[ 4 ] = 600;
   if (!fUseLegacyCIX)
      vdDef.wHeaderFlags |= VH_DATETIME; // For new users, show the Date column on topics by default.
   Amuser_GetPPArray( szSettings, "HeaderColumns", nHdrDivisions, 5 );
   vdDef.nHdrDivisions[ 0 ] = (WORD)nHdrDivisions[ 0 ];
   vdDef.nHdrDivisions[ 1 ] = (WORD)nHdrDivisions[ 1 ];
   vdDef.nHdrDivisions[ 2 ] = (WORD)nHdrDivisions[ 2 ];
   vdDef.nHdrDivisions[ 3 ] = (WORD)nHdrDivisions[ 3 ];
   vdDef.nHdrDivisions[ 4 ] = (WORD)nHdrDivisions[ 4 ];
   Amdb_SetDefaultViewData( &vdDef );

   /* Get and set the default purge options.
    */
   poDef.wMaxDate = (WORD)Amuser_GetPPInt( szSettings, "PurgeMaxDate", 30 );
   poDef.wMaxSize = (WORD)Amuser_GetPPInt( szSettings, "PurgeMaxSize", 100 );
   poDef.wFlags = (WORD)Amuser_GetPPInt( szSettings, "PurgeFlags", PO_DELETEONLY );
   Amdb_SetDefaultPurgeOptions( &poDef );

   /* Load the databases now.
    */
   if( NULL == CreateCIXDatabase() )
      {
      HideIntroduction();
      ExitAmeol( NULL, 0 );
      return( FALSE );
      }

   /* Make sure we have the cached handles.
    */
   (void)GetCIXCategory();

   if (fUseInternet)
   {
      (void)GetPostmasterFolder();
      (void)GetNewsFolder();
   }

   /* Load the saved search commands.
    */
   InstallSavedSearchCommands();

   /* Load the terminals.
    */
   InstallTerminals();

   /* Show the status and toolbar windows.
    */
   fShowStatusBar = Amuser_GetPPInt( szSettings, "StatusBar", TRUE );
   fShowToolBar = Amuser_GetPPInt( szSettings, "ToolBar", TRUE );
   CmdShowStatusBar( fShowStatusBar );
   CmdShowToolbar( fShowToolBar );

   /* Load the rules.
    */
   InitRules();

   /* Show the status bar.
    */
   if( fShowStatusBar )
      {
      UpdateWindow( hwndStatus );
      SendMessage( hwndStatus, SB_SETTEXT, idmsbOutbasket|SBT_OWNERDRAW, 0L );
      }

   /* Done Setup Wizard.
    */
   if( fCompletingSetupWizard )
      fCompletingSetupWizard = FALSE;

   /* Ask the services to initialise their interfaces.
    */
   if( fUseCIX )
      InitialiseCIXInterface( hwndFrame );
   if( fUseInternet )
      InitialiseCIXIPInterface( hwndFrame );
   InitialiseLocalInterface( hwndFrame );

   /* Enable gallery if required.
    */
   EnableGallery();

   /* Load the Acronyms.lst from either app dir or addons dir
   */
   AmLoadAcronymList( );

   /* Clear old downtime entries
    */
   ClearOldPendingDowntimes();

   /* Migrate cookies.
    */
   if( fMigrateCookies )
      {
      OfflineStatusText( "Migrating data cookies... Please wait" );
      Amdb_MigrateCookies();
      ShowUnreadMessageCount();
      }

   /* First time ever run? Offer to import from
    * Ameol if Ameol is installed.
    */
   if( fFirstRun )
   {
      if( GetProfileString( "Ameol", "Setup", "", lpPathBuf, _MAX_PATH ) )
         AmdbImportWizard( hwndFrame );
   }

   /* Start the sweep timer.
    */
   KillSweepTimer();
   wSweepRate = Amuser_GetPPInt( szSettings, "sweep", -1 );
   SetSweepTimer();
   PostMessage( hwndFrame, WM_AMEOL_INIT1, 0, 0L );
   return( TRUE );
}

/* This function tests whether the OS uses short filenames.
 */
void FASTCALL TestForShortFilenames( void )
{
   char szRoot[ 64 ];
   DWORD dwCompLen;
   DWORD dwFlags;

   Amdir_ExtractRoot( pszResumesDir, szRoot, sizeof(szRoot) );
   if( GetVolumeInformation( szRoot, NULL, 0, NULL, &dwCompLen, &dwFlags, NULL, 0 ) )
      fShortFilenames = dwCompLen < 255;
}

/* This function restores all windows that were opened
 * when Ameol was last closed.
 */
void FASTCALL OpenDefaultWindows( void )
{
   WORD wOpenWindows;

   /* Get the open windows flag.
    */
   wOpenWindows = Amuser_GetPPInt( szWindows, "Layout", WIN_MESSAGE );

   /* If screen resolution 640 x 480 or less, set 
    * dwDefWindowState so that the windows are
    * opened maximised.
    */
   if( GetSystemMetrics( SM_CXSCREEN ) <= 640 )
      dwDefWindowState = WS_MAXIMIZE;

   /* Now test each flag and open the windows as appropriate.
    */
   if( wOpenWindows & WIN_SCHEDULE )
      CmdScheduler( hwndFrame );
   if( wOpenWindows & WIN_OUTBASK )
      CreateOutBasket( hwndFrame );
   if( wOpenWindows & WIN_INBASK )
      CreateInBasket( hwndFrame );
   if( wOpenWindows & WIN_GRPLIST )
      SendCommand( hwndFrame, IDM_SHOWALLGROUPS, 1 );
   if(   ( wOpenWindows & WIN_USERLIST ) && fUseCIX )
      SendCommand( hwndFrame, IDM_SHOWALLUSERS, 1 );
   if(   ( wOpenWindows & WIN_CIXLIST ) && fUseCIX )
      ReadCIXList( hwndFrame );
   if(   ( wOpenWindows & WIN_RESUMESLIST ) && fUseCIX )
      ShowResumesListWindow();
   if(   wOpenWindows & WIN_PURGESETTINGS )
      CmdPurgeSettings( hwndFrame );
   if(   wOpenWindows & WIN_ADDRBOOK )
      Amaddr_Editor();

   /* If we failed to close down correctly last time, don't open the message
    * window this time (it may cause another crash, e.g. if the topic we are
    * trying to open is corrupt)
    */
   if( fAutoCheck )
      fMessageBox( GetFocus(), 0, GS(IDS_STR1255), MB_OK|MB_ICONEXCLAMATION );
   else if( ( wOpenWindows & WIN_MESSAGE ) || fCixLink )
      {
      PCAT pcatFirst;
      PCL pclFirst;
      PTL ptlFirst;

      /* Find the most recent topic open in the message window and use
       * that. Otherwise display the first topic.
       */
      ptlFirst = NULL;
      pclFirst = NULL;
      pcatFirst = NULL;
      Amuser_GetPPString( szWindows, "LastOpenTopic", "", lpTmpBuf, LEN_TEMPBUF );
      if( *lpTmpBuf )
         {
         LPVOID pFolder;

         ParseFolderPathname( lpTmpBuf, &pFolder, FALSE, 0 );
         if( NULL != pFolder )
            if( Amdb_IsTopicPtr( pFolder ) )
               {
               ptlFirst = pFolder;
               pclFirst = Amdb_FolderFromTopic( ptlFirst );
               pcatFirst = Amdb_CategoryFromFolder( pclFirst );
               }
            else if( Amdb_IsFolderPtr( pFolder ) )
               {
               pclFirst = pFolder;
               pcatFirst = Amdb_CategoryFromFolder( pclFirst );
               }
            else if( Amdb_IsCategoryPtr( pFolder ) )
               pcatFirst = pFolder;
         }
      if( NULL == pcatFirst )
         pcatFirst = Amdb_GetFirstCategory();
      if( NULL == pclFirst && NULL != pcatFirst )
         pclFirst = Amdb_GetFirstFolder( pcatFirst );
      if( NULL == ptlFirst && NULL != pclFirst )
         ptlFirst = Amdb_GetFirstTopic( pclFirst );
      if( NULL != ptlFirst )
         OpenTopicViewerWindow( ptlFirst );
      }
}

/* This function saves a record of the open windows at the
 * current time.
 */
void FASTCALL SaveDefaultWindows( void )
{
   WORD wOpenWindows;

   /* If a message window is open, save the path to the
    * topic shown in the message window. Otherwise vape
    * that key from the registry.
    */
   if( NULL == hwndMsg )
      Amuser_WritePPString( szWindows, "LastOpenTopic", NULL );
   else
      {
      LPWINDINFO lpWindInfo;
      
      lpWindInfo = GetBlock( hwndMsg );
      WriteFolderPathname( lpTmpBuf, lpWindInfo->lpTopic );
      Amuser_WritePPString( szWindows, "LastOpenTopic", lpTmpBuf );
      }
      
   /* Now record the window layout.
    */
   wOpenWindows = 0;

   if( NULL != hwndInBasket )
      wOpenWindows |= WIN_INBASK;
   if( NULL != hwndOutBasket )
      wOpenWindows |= WIN_OUTBASK;
   if( NULL != hwndScheduler )
      wOpenWindows |= WIN_SCHEDULE;
   if( NULL != hwndGrpList )
      wOpenWindows |= WIN_GRPLIST;
   if( NULL != hwndMsg )
      wOpenWindows |= WIN_MESSAGE;
   if( NULL != hwndUserList )
      wOpenWindows |= WIN_USERLIST;
   if( NULL != hwndCIXList )
      wOpenWindows |= WIN_CIXLIST;
   if( NULL != hwndRsmPopup )
      wOpenWindows |= WIN_RESUMESLIST;
   if( NULL != hwndPurgeSettings )
      wOpenWindows |= WIN_PURGESETTINGS;
   if( NULL != hwndAddrBook )
      wOpenWindows |= WIN_ADDRBOOK;
   
   Amuser_WritePPInt( szWindows, "Layout", wOpenWindows );

}

/* This function returns the handle of the database to which new
 * CIX conferences are added.
 */
PCAT FASTCALL GetCIXCategory( void )
{
   if( NULL == pcatCIXDatabase )
   {
      PCL pcl;
      PTL ptl;

      if( NULL == ( pcatCIXDatabase = Amdb_GetFirstCategory() ) )
      {
         pcatCIXDatabase = CreateCIXDatabase();
         if (pcatCIXDatabase == NULL)
            return NULL;
      }
      if (NULL == ( pcl = Amdb_GetFirstFolder(pcatCIXDatabase) ) )
      {
         pcl = Amdb_CreateFolder( pcatCIXDatabase, "cixnews", CFF_SORT|CFF_KEEPATTOP );
         if (pcl == NULL)
            return NULL;
      }
      if (NULL == ( ptl = Amdb_GetFirstTopic( pcl ) ) )
      {
         Amdb_CreateTopic(pcl, "information", FTYPE_CIX);
      }
   }
   return( pcatCIXDatabase );
}

/* This function creates a default database if the active one
 * went missing for some reason.
 */
PCAT FASTCALL CreateCIXDatabase( void )
{
   char szActiveUser[ 40 ];
   DBCREATESTRUCT dbcs;
   int nRetCode;

   /* No databases loaded, so create (or load) a default one.
    */
   Amuser_GetActiveUser( szActiveUser, sizeof(szActiveUser) );
   wsprintf( dbcs.szFilename, "%s.ADB", szActiveUser );
   _strupr( dbcs.szFilename );
   while( AMDBERR_NOERROR != ( nRetCode = Amdb_LoadDatabase( dbcs.szFilename ) ) )
      {
      /* If user aborted load of database (such as when it is being
       * converted, stop now.
       */
      if( nRetCode == AMDBERR_USERABORT )
         return( NULL );

      /* If the database is not found, create it anew
       */
      if( fFirstTimeThisUser && ( AMDBERR_FILENOTFOUND == nRetCode ) )
         {
         wsprintf( dbcs.szName, "%s", szActiveUser );
         dbcs.szPassword[ 0 ] = '\0';
         dbcs.dwType = 0;
         Amdb_CreateDatabase( &dbcs );
         Amuser_WritePPString( szDatabases, dbcs.szName, dbcs.szFilename );
         return( Amdb_GetFirstCategory() );
         } 

      /* For any other situation, offer to locate the
       * database.
       */
      switch( fDlgMessageBox( hwndFrame, idsMISSINGDB, IDDLG_MISSINGDB, dbcs.szFilename, 0L, 0 ) )
         {
         case IDD_LOCATE: {
            static char strFilters[] = "Ameol Database (*.ADB)\0*.*\0\0";
            OPENFILENAME ofn;

            BEGIN_PATH_BUF;
            strcpy( lpPathBuf, dbcs.szFilename );
            ofn.lStructSize = sizeof( OPENFILENAME );
            ofn.hwndOwner = hwndFrame;
            ofn.hInstance = NULL;
            ofn.lpstrFilter = strFilters;
            ofn.lpstrCustomFilter = NULL;
            ofn.nMaxCustFilter = 0;
            ofn.nFilterIndex = 1;
            ofn.nMaxFile = _MAX_FNAME;
            ofn.lpstrFileTitle = NULL;
            ofn.nMaxFileTitle = 0;
            ofn.lpstrTitle = "Locate Database Index File";
            ofn.Flags = OFN_NOCHANGEDIR|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
            ofn.nFileOffset = 0;
            ofn.nFileExtension = 0;
            ofn.lpstrDefExt = NULL;
            ofn.lCustData = 0;
            ofn.lpfnHook = NULL;
            ofn.lpTemplateName = 0;
            ofn.lpstrFile = lpPathBuf;
            ofn.lpstrInitialDir = pszDataDir;
            if( GetOpenFileName( &ofn ) )
               {
               char *lpFilename;

               /* Extract the path and the filename. The path is used to
                * set the data directory.
                */
               lpFilename = GetFileBasename( lpPathBuf );
               if( _strcmpi( lpFilename, dbcs.szFilename ) == 0 )
                  if( lpFilename > lpPathBuf )
                     {
                     *(lpFilename - 1) = '\0';
                     Amuser_WritePPString( szDirects, "Data", lpPathBuf );
                     Amdb_SetDataDirectory( lpPathBuf );
                     }
               }
            END_PATH_BUF;
            break;
            }

         case IDCANCEL:
         case IDD_EXIT:
            return( NULL );

         case IDD_NEW:
            wsprintf( dbcs.szName, "%s", szActiveUser );
            dbcs.szPassword[ 0 ] = '\0';
            dbcs.dwType = 0;
            Amdb_CreateDatabase( &dbcs );
            Amuser_WritePPString( szDatabases, dbcs.szName, dbcs.szFilename );
            return( Amdb_GetFirstCategory() );
         }
      }
   return( Amdb_GetFirstCategory() );
}

/* This function returns the handle of the folder to which
 * all incoming mail is saved.
 */
PTL FASTCALL GetPostmasterFolder( void )
{
   if( NULL == ptlPostmaster )
      {
      Amuser_GetPPString( szCIXIP, "Postmaster", "", lpTmpBuf, LEN_TEMPBUF );
      if( *lpTmpBuf )
         ParseFolderPathname( lpTmpBuf, &ptlPostmaster, TRUE, FTYPE_MAIL );
      if( NULL == ptlPostmaster )
         {
         char sz[ 256 ];
         ptlPostmaster = CreateMailTopic( (char *)szPostmaster );
         WriteFolderPathname( sz, ptlPostmaster );
         Amuser_WritePPString( szCIXIP, "Postmaster", sz );
         }
      }
   return( ptlPostmaster );
}

/* This function ensures that the specified news folder
 * exists. It may have been deleted.
 */
PCL FASTCALL GetNewsFolder( void )
{
   if( NULL == pclNewsFolder )
      {
      Amuser_GetPPString( szPreferences, "NewsFolder", "", lpTmpBuf, LEN_TEMPBUF );
      if( *lpTmpBuf )
         ParseFolderPathname( lpTmpBuf, &pclNewsFolder, TRUE, FTYPE_NEWS );
      if( NULL == pclNewsFolder )
         if( NULL == ( pclNewsFolder = Amdb_OpenFolder( NULL, "News" ) ) )
         {
            PCAT pcat;

            if( pcat = GetCIXCategory() )
            {
               char sz[ 256 ];

               pclNewsFolder = Amdb_CreateFolder( pcat, "News", CFF_SORT|CFF_KEEPATTOP );
               WriteFolderPathname( sz, pclNewsFolder );
               Amuser_WritePPString( szPreferences, "NewsFolder", sz );
            }
         }
      }
   return( pclNewsFolder );
}

/* This function registers common out-basket objects used by
 * both services.
 */
BOOL FASTCALL LoadCommon( void )
{
   /* Register out-basket types used by the
    * all services.
    */
   if( !Amob_RegisterObjectClass( OBTYPE_GETNEWARTICLES, BF_GETIPNEWS, ObProc_GetNewArticles ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_GETTAGGED, BF_GETIPTAGGEDNEWS, ObProc_GetTagged ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_GETNEWSGROUPS, BF_POSTIPNEWS, ObProc_GetNewsgroups ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_GETNEWNEWSGROUPS, BF_POSTIPNEWS, ObProc_GetNewNewsgroups ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_GETARTICLE, BF_GETIPNEWS, ObProc_GetArticle ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_MAILMESSAGE, BF_POSTIPMAIL, ObProc_MailMessage ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_ARTICLEMESSAGE, BF_POSTIPNEWS, ObProc_ArticleMessage ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_CANCELARTICLE, BF_POSTIPNEWS, ObProc_CancelArticle ) )
      return( FALSE );
   if( !Amob_RegisterObjectClass( OBTYPE_FORWARDMAIL, BF_POSTIPMAIL, ObProc_ForwardMail ) )
      return( FALSE );
   return( TRUE );
}

/* This function retrieves a plaintext encoded password from the configuration
 * file.
 */
int FASTCALL Amuser_GetPPPassword( LPCSTR lpSection, LPSTR lpKey, LPCSTR lpDefBuf, LPSTR lpBuf, int cbBuf )
{
   int cbText;

   cbText = Amuser_GetPPString( lpSection, lpKey, lpDefBuf, lpTmpBuf, LEN_TEMPBUF );
   if( 0 != cbText )
      DecodeLine64( lpTmpBuf, lpBuf, cbBuf );
   else
      {
      memset( lpBuf, 0, cbBuf );
      Amuser_Encrypt( lpBuf, rgEncodeKey );
      }
   return( cbText );
}

/* This function writes a plaintext encoded password to the configuration file.
 */
int FASTCALL Amuser_WritePPPassword( LPCSTR lpSection, LPSTR lpKey, LPSTR lpBuf )
{
   if( NULL != lpBuf )
      {
      EncodeLine64( lpBuf, LEN_PASSWORD, lpTmpBuf );
      return( Amuser_WritePPString( lpSection, lpKey, lpTmpBuf ) );
      }
   return( Amuser_WritePPString( lpSection, lpKey, lpBuf ) );
}

/* This function creates the specified mailbox in the Mail folder,
 * whereever that might be.
 */
PTL FASTCALL CreateMailTopic( char * pszMailBox )
{
   PCL pcl;
   PTL ptl;

   ptl = NULL;
   if( NULL == ( pcl = Amdb_OpenFolder( NULL, "Mail" ) ) )
      {
      PCAT pcat;

      if( pcat = GetCIXCategory() )
         pcl = Amdb_CreateFolder( pcat, "Mail", CFF_SORT|CFF_KEEPATTOP );
      }
   if( NULL != pcl )
      if( NULL == ( ptl = Amdb_OpenTopic( pcl, pszMailBox ) ) )
         ptl = Amdb_CreateTopic( pcl, pszMailBox, FTYPE_MAIL );
   if( NULL == ptl )
      {
      wsprintf( lpTmpBuf, GS(IDS_STR845), pszMailBox );
      fMessageBox( hwndFrame, 0, lpTmpBuf, MB_OK|MB_ICONINFORMATION );
      }
   return( ptl );
}

/* This function loads all databases listed in the [Databases]
 * section of the user's configuration file.
 */
int FASTCALL LoadAllDatabases( void )
{
   LPSTR lpszDbList;
   int cLoaded;

   INITIALISE_PTR(lpszDbList);
   cLoaded = 0;
   if( fNewMemory( &lpszDbList, 8000 ) )
      {
      UINT c;

      Amuser_GetPPString( szDatabases, NULL, "", lpszDbList, 8000 );
      for( c = 0; lpszDbList[ c ]; ++c )
         {
         char sz[_MAX_PATH];

         /* Read the path for this database, then load it.
          */
         Amuser_GetPPString( szDatabases, &lpszDbList[ c ], "", sz, sizeof(sz) );
         c += strlen( &lpszDbList[ c ] );
         }
      FreeMemory( &lpszDbList );
      }
   return( cLoaded );
}

/* This function returns the type of the active
 * window.
 */
int EXPORT WINAPI Ameol2_GetWindowType( void )
{
   return( LOWORD(iActiveMode) );
}

/* This function returns an entry from the internal system
 * parameter store.
 */
int EXPORT WINAPI Ameol2_GetSystemParameter( char * pszParmString, char * pszBuf, int cbMax )
{
   char szService[ 20 ];
   char * pszService;

   /* Extract the service name.
    */
   pszService = szService;
   while( *pszParmString && *pszParmString != '\\' )
      *pszService++ = *pszParmString++;
   *pszService = '\0';

   /* Skip the path chr.
    */
   if( *pszParmString ==  '\\' )
      ++pszParmString;

   /* Call the appropriate service function.
    */
   memset( pszBuf, 0, cbMax );
   if( _strcmpi( szService, "CIX" ) == 0 )
   {
      return( GetCIXSystemParameter( pszParmString, pszBuf, cbMax ) );
   }
   else if( _strcmpi( szService, "IP" ) == 0 )
   {
      return( GetCIXIPSystemParameter( pszParmString, pszBuf, cbMax ) );
   }
   else if( _strcmpi( szService, "BASEPATH" ) == 0 )
   {  
      lstrcpyn( pszBuf, pszAmeolDir, cbMax );
      return( lstrlen( pszAmeolDir ) );
   }
   else
   {
      return( 0 );
   }
}

/* Subclassed MDI client window procedure.
 */
LRESULT EXPORT WINAPI MdiClientWndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   switch( msg )
      {
      case WM_ERASEBKGND: {
         HBITMAP hbmpOld;
         BITMAP bmp;
         HDC hdcMem;
         RECT rc;

         /* Paint the bitmap on the background. Either stretch or
          * tile as required.
          */
         if( NULL == hBmpbackground )
            return( FALSE );

         /* Prepare to attack.
          */
         hdcMem = CreateCompatibleDC( (HDC)wParam );
         SelectPalette( (HDC)wParam, hPalbackground, FALSE );
         RealizePalette( (HDC)wParam );
         SelectPalette( hdcMem, hPalbackground, FALSE );
         RealizePalette( hdcMem );
         hbmpOld = SelectBitmap( hdcMem, hBmpbackground );
         GetObject( hBmpbackground, sizeof( BITMAP ), &bmp );
         GetClientRect( hwnd, &rc );

         /* Draw the bitmap. Either stretch or tile as
          * appropriate.
          */
         if( fStretchBmp )
            StretchBlt( (HDC)wParam, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, bmp.bmWidth, bmp.bmHeight, SRCCOPY );
         else
            {
            RECT rcClip;
            int cx, cy;
            int x, y;

            GetClipBox( (HDC)wParam, &rcClip );
            cx = ( rc.right / bmp.bmWidth ) + 1;
            cy = ( rc.bottom / bmp.bmHeight ) + 1;
            for( x = 0; x < cx; ++x )
               for( y = 0; y < cy; ++y )
                  {
                  RECT rcOverlap;
                  RECT rcDraw;

                  rcDraw.left = x * bmp.bmWidth;
                  rcDraw.top = y * bmp.bmHeight;
                  rcDraw.right = rcDraw.left + bmp.bmWidth;
                  rcDraw.bottom = rcDraw.top + bmp.bmHeight;
                  if( IntersectRect( &rcOverlap, &rcDraw, &rcClip ) )
                     BitBlt( (HDC)wParam, rcDraw.left, rcDraw.top, bmp.bmWidth, bmp.bmHeight, hdcMem, 0, 0, SRCCOPY );
                  }
            }

         /* Clean up our wet dream.
          */
         SelectBitmap( hdcMem, hbmpOld );
         DeleteDC( hdcMem );
         return( TRUE );
         }
      }
   return( CallWindowProc( lpfMdiClientProc, hwnd, msg, wParam, lParam ) );
}

/* This function handles the default MDI dialog messages.
 */
LRESULT EXPORT WINAPI Ameol2_DefMDIDlgProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   switch( msg )
      {
      HANDLE_MSG( hwnd, WM_ERASEBKGND, Common_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, Ameol2_MDIActivate );

      default:
         break;
      }
   return( Adm_DefMDIDlgProc( hwnd, msg, wParam, lParam ) );
}

/* This function handles the WM_MDIACTIVATE message.
 */
void FASTCALL Ameol2_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_EDITS ), (LPARAM)(LPSTR)hwnd );
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, Adm_DefMDIDlgProc );
}

/* This function handles all main window events.
 */
BOOL EXPORT CALLBACK MainWndEvents( int wEvent, LPARAM lParam1, LPARAM lParam2 )
{
   switch( wEvent )
      {
      case AE_DELETED_OUTBASKET_OBJECT:
         SendMessage( hwndStatus, SB_SETTEXT, idmsbOutbasket|SBT_OWNERDRAW, 0L );
         break;

      case AE_INSERTED_OUTBASKET_OBJECT:
         /* A new item has been added to the out-basket, so make it
          * appear in the listbox.
          */
         if( fOnline )
            {
            OBINFO obinfo;

            Amob_GetObInfo( (LPOB)lParam1, &obinfo );
            if( !( obinfo.obHdr.wFlags & OBF_HOLD ) )
               {
               obinfo.obHdr.wFlags |= OBF_PENDING;
               Amob_SetObInfo( (LPOB)lParam1, &obinfo );
               }
            }
         SendMessage( hwndStatus, SB_SETTEXT, idmsbOutbasket|SBT_OWNERDRAW, 0L );
         break;

      case AE_POPUP: {
         LPAEPOPUP lpaep;

         /* Get the menu handle and the position at
          * which we're going to insert.
          */
         lpaep = (LPAEPOPUP)lParam1;
         switch( lpaep->wType )
            {
            case WIN_INBASK: {
               /* Add rethread commands only if current folder is a
                * mail or news folder.
                */
               if( Amdb_IsTopicPtr( lpaep->pFolder ) )
                  if( Amdb_GetTopicType( lpaep->pFolder ) == FTYPE_MAIL ||
                      Amdb_GetTopicType( lpaep->pFolder ) == FTYPE_NEWS )
                     InsertMenu( lpaep->hMenu, lpaep->nInsertPos++, MF_BYPOSITION, IDM_RETHREADARTICLES, "Rethread Messages" );
               break;
               }

            case WIN_OUTBASK:
            case WIN_THREAD:
            case WIN_EDITS: 
            case WIN_INFOBAR:
            case WIN_TERMINAL:
            case WIN_GRPLIST:
            case WIN_USERLIST:
            case WIN_CIXLIST:
            case WIN_PARLIST:
            case WIN_SCHEDULE:
            case WIN_PURGESETTINGS:
            case WIN_RESUMESLIST:
            case WIN_ADDRBOOK:
            case WIN_RESUMES:
            case WIN_MESSAGE:
               {
                  HWND hwndEdit;
                  fCurURL[0] = '\x0'; // 2.56.2051 FS#96
                  /* If the selected text starts with http://, add a Browse
                  * command to the menu. Eventually when the viewer control supports
                  * hotlinks, we can avoid this.
                  */
                  if( NULL != lpaep->pSelectedText )
                  {
                     //                char * pText;
                     //                int cbMatch;
                     
                     //                pText = lpaep->pSelectedText;
                     //                while( isspace( *pText ) )
                     //                   ++pText;
                     //                if( Amctl_IsHotlinkPrefix( pText, &cbMatch ) )
                     InsertMenu( lpaep->hMenu, lpaep->nInsertPos++, MF_BYPOSITION, IDM_BROWSE, "&Browse Selected Text" );
                  }
                  else // 2.56.2051 FS#96
                  {
                     hwndEdit = GetDlgItem( hwndMsg, IDD_MESSAGE );
                     if (IsWindow(hwndEdit) )
                     {
                        int sCur;
                        POINT lp;
                        
                        GetCursorPos(&lp);
                        ScreenToClient(hwndEdit, &lp);
                        
                        sCur = SendMessage(hwndEdit, SCI_GETSTYLEAT, SendMessage(hwndEdit, SCI_POSITIONFROMPOINT, lp.x, lp.y), 0);
                        if(sCur == SCE_CIX_HOTLINK || sCur == SCE_CIX_QUOTED_HOTLINK)
                        {
                           GetCurrentURL( hwndEdit, lp, fCurURL ); 
                           InsertMenu( lpaep->hMenu, lpaep->nInsertPos++, MF_BYPOSITION, IDM_BROWSE, "&Browse Link" );
                           InsertMenu( lpaep->hMenu, lpaep->nInsertPos++, MF_BYPOSITION, IDM_COPYLINK, "&Copy Link" );
                        }                       
                     }
                  }
                  
                  break;
               }
            }
         break;
         }

      case AE_INSERTING_OUTBASKET_OBJECT:
         return( fOnline && !fBlinking );

      case AE_CONTEXTSWITCH:
         SendAllWindows( WM_CONTEXTSWITCH, LOWORD(lParam1), lParam2 );
         break;

      case AE_MDI_WINDOW_ACTIVATED:
         /* An MDI dialog window has been made active, so set
          * hwndActive and hwndMDIDlg as appropriate.
          */
         if( LOWORD( lParam1 ) )
            {
            hwndActive = (HWND)lParam2;
            hwndMDIDlg = (HWND)lParam2;
            iActiveMode = HIWORD( lParam1 );
            }
         else
            {
            SendMessage( hwndStatus, SB_SETTEXT, idmsbRow, (LPARAM)(LPSTR)"" );
            SendMessage( hwndStatus, SB_SETTEXT, idmsbColumn, (LPARAM)(LPSTR)"" );
            hwndActive = NULL;
            hwndMDIDlg = NULL;
            iActiveMode = 0;
            }
         break;

      case AE_FOLDER_CHANGED:
         SendAllWindows( WM_FOLDERCHANGED, LOWORD(lParam2), lParam1 );
         return( TRUE );

      case AE_TOPIC_CHANGED:
         SendAllWindows( WM_TOPICCHANGED, LOWORD(lParam2), lParam1 );
         return( TRUE );

      case AE_CATEGORY_CHANGED:
         SendAllWindows( WM_CATEGORYCHANGED, LOWORD(lParam2), lParam1 );
         return( TRUE );

      case AE_NEWTOPIC:
         SendAllWindows( WM_INSERTTOPIC, 0, lParam1 );
         break;

      case AE_NEWMSG:
         SendAllWindows( WM_NEWMSG, 0, lParam1 );
         break;

      case AE_MOVETOPIC:
         SendAllWindows( WM_DELETETOPIC, (WPARAM)-1, lParam1 );
         break;

      case AE_MOVEFOLDER:
         SendAllWindows( WM_DELETEFOLDER, (WPARAM)-1, lParam1 );
         break;

      case AE_MOVECATEGORY:
         SendAllWindows( WM_DELETECATEGORY, (WPARAM)-1, lParam1 );
         break;

      case AE_DELETETOPIC:
         SyncSearchPtrs( (LPVOID)lParam1 );
         DeleteAllApplicableRules( (LPVOID)lParam1 );
         SendAllWindows( WM_DELETETOPIC, 0, lParam1 );
         if( (PTL)lParam1 == ptlPostmaster )
            ptlPostmaster = NULL;
         if( (PTL)lParam1 == ptlMailFolder )
            ptlMailFolder = NULL;
         break;

      case AE_NEWFOLDER:
         SendAllWindows( WM_INSERTFOLDER, 0, lParam1 );
         break;

      case AE_DELETEFOLDER:
         SyncSearchPtrs( (LPVOID)lParam1 );
         DeleteAllApplicableRules( (LPVOID)lParam1 );
         SendAllWindows( WM_DELETEFOLDER, 0, lParam1 );
         if( (PCL)lParam1 == pclNewsFolder )
            pclNewsFolder = NULL;
         break;

      case AE_NEWCATEGORY:
         SendAllWindows( WM_INSERTCATEGORY, 0, lParam1 );
         break;

      case AE_DELETECATEGORY:
         SyncSearchPtrs( (LPVOID)lParam1 );
         DeleteAllApplicableRules( (LPVOID)lParam1 );
         SendAllWindows( WM_DELETECATEGORY, 0, lParam1 );
         if( (PCAT)lParam1 == pcatCIXDatabase )
            pcatCIXDatabase = NULL;
         break;

      case AE_CLOSEDATABASE:
         SendAllWindows( WM_DELETECATEGORY, FALSE, lParam1 );
         break;

      case AE_UNREADCHANGED:
         SendMessage( hwndFrame, WM_UPDATE_UNREAD, 0, 0L );
         SendAllWindows( WM_UPDATE_UNREAD, 0, lParam1 );
         break;

      case AE_DELETEMSG:
         SyncSearchPtrs( (LPVOID)lParam1 );
         SendAllWindows( WM_DELETEMSG, LOWORD(lParam2), lParam1 );
         break;
      }
   return( TRUE );
}

/* This function checks whether the specified text is a hotlink
 * prefix.
 */
 /*
BOOL FASTCALL IsHotlinkPrefix( char * pszText, int * cbMatch )
{
// if( !fHotLinks )
//    return( FALSE );
   if( _strnicmp( pszText, "http://", *cbMatch = 7 ) == 0 )
      return( TRUE );
   if( _strnicmp( pszText, "file://", *cbMatch = 7 ) == 0 )
      return( TRUE );
   if( _strnicmp( pszText, "www.", *cbMatch = 4 ) == 0 && ( !*( pszText - 1 ) || ( *( pszText - 1 ) && ( !ispunct( *( pszText - 1 ) ) || *( pszText - 1 ) == '(' || *( pszText - 1 ) == '[' || *( pszText - 1 ) == '<' || *( pszText - 1 ) == ':' ) && !isalpha( *( pszText - 1 ) ) ) ) && isalnum( *( pszText + 4 ) ) )
      return( TRUE );
   if( _strnicmp( pszText, "https://", *cbMatch = 8 ) == 0 )
      return( TRUE );
   if( _strnicmp( pszText, "cix:", *cbMatch = 4 ) == 0 && ( !*( pszText - 1 ) || ( *( pszText - 1 ) && ( !ispunct( *( pszText - 1 ) ) || *( pszText - 1 ) == '(' || *( pszText - 1 ) == '[' || *( pszText - 1 ) == '<' || *( pszText - 1 ) == ':' ) && !isalpha( *( pszText - 1 ) ) ) ) )
      return( TRUE );
   if( _strnicmp( pszText, "**COPIED FROM: >>>", *cbMatch = 18 ) == 0 )
      return( TRUE );
   if( _strnicmp( pszText, "ftp://", *cbMatch = 6 ) == 0 )
      return( TRUE );
   if( _strnicmp( pszText, "cixfile:", *cbMatch = 8 ) == 0 )
      return( TRUE );
   if( _strnicmp( pszText, "mailto:", *cbMatch = 7 ) == 0 )
      return( TRUE );
   if( _strnicmp( pszText, "news:", *cbMatch = 5 ) == 0 &&  ( !*( pszText - 1 ) || ( *( pszText - 1 ) && ( !ispunct( *( pszText - 1 ) ) || *( pszText - 1 ) == '(' || *( pszText - 1 ) == '[' || *( pszText - 1 ) == '<' || *( pszText - 1 ) == ':' ) && !isalpha( *( pszText - 1 ) ) ) ) && ( !ispunct( *( pszText + 5 ) ) || *( pszText + 5 ) =='/' ) )
      return( TRUE );
   if( _strnicmp( pszText, "gopher://", *cbMatch = 9 ) == 0 )
      return( TRUE );
   if( _strnicmp( pszText, "callto:", *cbMatch = 7 ) == 0 )
      return( TRUE );
   if( _strnicmp( pszText, "telnet://", *cbMatch = 9 ) == 0 )
      return( TRUE );
   if( _strnicmp( pszText, "irc://", *cbMatch = 6 ) == 0 )
      return( TRUE );
   if( _strnicmp( pszText, "cixchat://", *cbMatch = 10 ) == 0 )
      return( TRUE );
   return( FALSE );
}
*/
/* This function loads the toolbar.
 */
BOOL FASTCALL LoadToolbar( HWND hwnd, BOOL fShow )
{
   int cButtonArraySize;
   TBBUTTON FAR * lpButtonArray;
   char szKey[ 10 ];
   char szData[ 200 ];
   BOOL fOk;

   INITIALISE_PTR(lpButtonArray);

   /* Load the toolbar, if it exists.
    */
   wsprintf( szKey, "Tool0" );
   cButtonArraySize = 0;
   if( Amuser_GetPPString( szToolbar, szKey, "", szData, sizeof(szData) ) )
      {
      int cAllocSize;
      int cTool;

      cAllocSize = 16;
      cTool = 0;
      if( fNewMemory( &lpButtonArray, cAllocSize * sizeof( TBBUTTON ) ) )
         do {
            char szCommand[ 40 ];
            BOOL fHasButton;
            char * pszData;
            register int c;
            TBBUTTON tb;
            CMDREC cmd;

            tb.idCommand = 0;
            tb.iBitmap = 0;

            /* Expand the array if needed.
             */
            if( cButtonArraySize >= cAllocSize )
               {
               cAllocSize += 16;
               if( !fResizeMemory( &lpButtonArray, cAllocSize * sizeof( TBBUTTON ) ) )
                  break;
               }

            /* Okay. szData is the command name, so extract it into the
             * szCommand buffer and move pszData forward to point to any
             * parameters.
             */
            pszData = szData;
            while( *pszData == ' ' )
               ++pszData;
            for( c = 0; *pszData && *pszData != ' ' && c < sizeof(szCommand) - 1; ++c )
               szCommand[ c ] = *pszData++;
            szCommand[ c ] = '\0';
            while( *pszData == ' ' )
               ++pszData;

            /* First check for special case Sep, which indicates a separator,
             * and the value following which is the separator width.
             */
            fHasButton = FALSE;
            if( _strcmpi( szCommand, "Sep" ) == 0 )
               {
               tb.idCommand = 0;
               tb.fsStyle = TBSTYLE_SEP;
               tb.iBitmap = atoi( pszData );
               fHasButton = TRUE;
               }
            else if( _strcmpi( szCommand, "FileBlink" ) == 0 )
               {
               tb.idCommand = IDM_BLINKMANFIRST;
               tb.fsStyle = TBSTYLE_BUTTON;
               fHasButton = TRUE;
               }
            else if( _strcmpi( szCommand, "TermBtn" ) == 0 )
               {
               tb.idCommand = IDM_TERMINALFIRST;
               tb.fsStyle = TBSTYLE_BUTTON;
               fHasButton = TRUE;
               }
            else if( CTree_FindCommandName( szCommand, &cmd ) )
               {
               tb.iBitmap = cmd.iToolButton;
               tb.idCommand = cmd.iCommand;
               tb.fsStyle = TBSTYLE_BUTTON;
               fHasButton = TRUE;
               }

            /* We have a button.
             */
            if( fHasButton )
               {
               /* Is it RunApp? Special case to deal with commands
                * not in the command table.
                * BUGBUG: Put the code that splits a command line into
                * a separate function.
                */
               if( IDM_EXTERNALAPP == tb.idCommand )
                  {
                  APPINFO appinfo;

                  pszData = OldExtractFilename( appinfo.szAppPath, pszData );
                  while( *pszData == ' ' )
                     ++pszData;
                  strcpy( appinfo.szCmdLine, pszData );
                  QuotifyFilename( appinfo.szCmdLine );
                  tb.idCommand = InstallExternalApp( &appinfo );
                  }
               /* Blink command. Can be the built-in one or one with
                * parameters.
                */
               else if( IDM_BLINKMANFIRST == tb.idCommand && *pszData )
                  {
                  if( 0 == ( tb.idCommand = GetBlinkCommandID( pszData ) ) )
                     {
                     if( strcmp( pszData, "CIX Forums Via Dial-Up" ) == 0 )
                        {
                        tb.idCommand = IDM_BLINKDDC;
                        tb.iBitmap = TB_BLINKDDC;
                        }
                     else
                        {
                        tb.idCommand = IDM_BLINKMANFIRST;
                        tb.iBitmap = TB_BLINK;
                        }
                     }
                  }

               /* Terminal buttons.
                */
               else if( IDM_TERMINALFIRST == tb.idCommand )
                  {
                  if( 0 == ( tb.idCommand = GetTerminalCommandID( pszData ) ) )
                     tb.idCommand = IDM_TERMINALFIRST;
                  }

               /* We've got the command data, so create the toolbar button.
                */
               tb.dwData = 0;
               tb.iString = tb.iBitmap;
               tb.fsState = TBSTATE_ENABLED;
               if( tb.idCommand >= IDM_DYNAMICCOMMANDS )
                  tb.fsStyle |= TBSTYLE_CALLBACK;
               lpButtonArray[ cButtonArraySize++ ] = tb;
               }
            wsprintf( szKey, "Tool%u", ++cTool );
            }
         while( Amuser_GetPPString( szToolbar, szKey, "", szData, sizeof(szData) ) );
      }

   /* If we couldn't load the stored toolbar, revert back to the
    * built-in one.
    */
   if( 0 == cButtonArraySize )
   {
      if (fUseInternet)
      {
         lpButtonArray = aDefIPToolbar;
         cButtonArraySize = cToolsOnDefIPToolbar;
      }
      else
      {
         if( fFirstRun )
         {
            lpButtonArray = aNewDefToolbar;
            cButtonArraySize = cToolsOnNewDefToolbar;
         }
         else
         {
            lpButtonArray = aDefToolbar;
            cButtonArraySize = cToolsOnDefToolbar;
         }
      }
   }

   /* Call ChangeToolbar to change the toolbar.
    */
   fOk = ChangeToolbar( hwnd, fShow, lpButtonArray, cButtonArraySize );

   /* Deallocate button array memory.
    * (Wouldn't a flag be safer?)
    */
   if( lpButtonArray != aDefToolbar && lpButtonArray != aDefIPToolbar && lpButtonArray != aNewDefToolbar )
      FreeMemory( &lpButtonArray );

   /* Return status to caller.
    */
   return( fOk );
}

/* This function changes or creates the toolbar.
 */
BOOL FASTCALL ChangeToolbar( HWND hwnd, BOOL fShow, TBBUTTON FAR * lpButtonArray, int cButtonArraySize )
{
   int * pButtonArray;
   TBADDBITMAP tba;
   BITMAP bmp;
   HBITMAP hbmp;
   char * pStrings;
   char * pStr;
   UINT cStrLen;
   register int c;
   int cxButton;
   int cyButton;
   int cxBitmap;
   int cyBitmap;
   DWORD dwStyle;

   INITIALISE_PTR(pStrings);

   /* Get toolbar settings.
    */
   f3DStyle = Amuser_GetPPInt( szPreferences, "3DToolbar", FALSE );
   fLargeButtons = Amuser_GetPPInt( szPreferences, "LargeButtons", FALSE );
   fUseTooltips = Amuser_GetPPInt( szPreferences, "Tooltips", TRUE );
   fButtonLabels = Amuser_GetPPInt( szPreferences, "ButtonLabels", FALSE );
   fVisibleSeperators = Amuser_GetPPInt( szPreferences, "VisibleSeperators", FALSE );
   fNewButtons = Amuser_GetPPInt( szPreferences, "NewButtons", FALSE );

   /* Select the appropriate array of toolbar bitmap IDs.
    */
   if( fNewButtons )
      pButtonArray = fLargeButtons ? nNewLargeButtonArray : nNewNormalButtonArray;
   else
      pButtonArray = fLargeButtons ? nLargeButtonArray : nNormalButtonArray;

   /* Reset to the default toolbar if no proper arguments supplied.
    */
   if( NULL == lpButtonArray || 0 == cButtonArraySize )
      {
         if ( fUseCIX )
         {
            lpButtonArray = aDefToolbar;
            cButtonArraySize = cToolsOnDefToolbar;
         }
         else
         {
            lpButtonArray = aDefIPToolbar;
            cButtonArraySize = cToolsOnDefIPToolbar;
         }
      }
   /* Replace IDM_BLINKFULL with the ID for a full blink and
    * IDM_BLINKDDC with the ID for a dial-up blink.
    */
   for( c = 0; c < cButtonArraySize; ++c )
      {
      if( lpButtonArray[ c ].idCommand == IDM_BLINKFULL )
         lpButtonArray[ c ].idCommand = GetBlinkCommandID( "Full" );
      if( lpButtonArray[ c ].idCommand == IDM_BLINKDDC )
         lpButtonArray[ c ].idCommand = GetBlinkCommandID( "CIX Forums Only" );
      }

   /* Count the number of buttons in the toolbar bitmap.
    */
   hbmp = LoadBitmap( hRscLib, MAKEINTRESOURCE(pButtonArray[0]) );
   GetObject( hbmp, sizeof( BITMAP ), &bmp );
   cxBitmap = bmp.bmWidth / BUTTONS_PER_SLOT;
   cyBitmap = bmp.bmHeight;
   DeleteBitmap( hbmp );

   /* Get the toolbar position.
    */
   dwStyle = TBSTYLE_TOOLTIPS|TBSTYLE_WRAPABLE|ACTL_CCS_NOPARENTALIGN;
   nToolbarPos = Amuser_GetPPInt( szToolbar, "Orientation", IDM_TOOLBAR_TOP );
   if( nToolbarPos == IDM_TOOLBAR_LEFT || nToolbarPos == IDM_TOOLBAR_RIGHT )
      dwStyle |= TBSTYLE_VERT;
   if( !f3DStyle )
      dwStyle |= TBSTYLE_FLAT;
   if( fVisibleSeperators )
      dwStyle |= TBSTYLE_VISSEP;
   if( fNewButtons )
      dwStyle |= TBSTYLE_NEWBUTTONS;

   /* Make adjustments for labels.
    */
   if( fButtonLabels )
      {
      cxButton = 60;
      cyButton = 60;
      }
   else
      {
      cxButton = cxBitmap + 8;
      cyButton = cyBitmap + 7;
      }

   /* Create the toolbar.
    */
   if( NULL != hwndToolBar )
      {
      DestroyWindow( hwndToolBar );
      hwndToolBar = NULL;
      }
   hwndToolBar = Amctl_CreateToolbar( hwnd,
                                  dwStyle,
                                  IDD_WINDOWTOOLBAR,
                                  BUTTONS_PER_SLOT,
                                  hInst,
                                  pButtonArray[ 0 ],
                                  lpButtonArray,
                                  cButtonArraySize,
                                  cxButton, cyButton,
                                  cxBitmap, cyBitmap,
                                  sizeof( TBBUTTON ) );
   if( hwndToolBar == NULL )
      return( FALSE );

   /* Set the toolbar font.
    */
   if( NULL == hToolbarFont )
      {
      HDC hdc;
      int cPixelsPerInch;
      int cyPixels;

      hdc = GetDC( NULL );
      ASSERT( hdc != NULL );
      cPixelsPerInch = GetDeviceCaps( hdc, LOGPIXELSY );
      cyPixels = -MulDiv( 8, cPixelsPerInch, 72 );
      hToolbarFont = CreateFont( cyPixels, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                              FALSE, ANSI_CHARSET, OUT_CHARACTER_PRECIS,
                              CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
                              VARIABLE_PITCH | FF_SWISS, "Arial" );
      ReleaseDC( NULL, hdc );
      }
   SetWindowFont( hwndToolBar, hToolbarFont, FALSE );

   /* Save the tooltips handle.
    */
   hwndToolTips = (HWND)SendMessage( hwndToolBar, TB_GETTOOLTIPS, 0, 0L );

   /* Add the other toolbar bitmaps.
    */
   for( c = 1; c < cButtonArray; ++c )
      {
      tba.hInst = hInst;
      tba.nID = pButtonArray[ c ];
      SendMessage( hwndToolBar, TB_ADDBITMAP, BUTTONS_PER_SLOT, (LPARAM)(LPSTR)&tba );
      }

   /* Create the toolbar strings. First count the length
    * then create a mega-block with them in it.
    */
   if( fButtonLabels )
      {
      register int i;

      cStrLen = 0;
      for( i = 0; i < maxToolStr; ++i )
         cStrLen += strlen( GS( ToolStr[ i ] ) ) + 1;
      if( fNewMemory( &pStrings, cStrLen + 1 ) )
         {
         pStr = pStrings;
         for( i = 0; i < maxToolStr; ++i )
            {
            LPCSTR pTTStr;
            int x;

            pTTStr = GS( ToolStr[ i ] );
            for( x = 0; pTTStr[ x ]; ++x )
               *pStr++ = pTTStr[ x ];
            *pStr++ = '\0';
            }
         *pStr = '\0';
         }
      SendMessage( hwndToolBar, TB_ADDSTRING, 0, (LPARAM)(LPSTR)pStrings );
      FreeMemory( &pStrings );
      }

   /* Do we show the toolbar?
    */
   if( fShow )
      {
      CmdShowToolbar( TRUE );
      UpdateAppSize();
      }
   return( TRUE );
}

/* This function saves the toolbar after it has been edited.
 */
BOOL FASTCALL SaveToolbar( HWND hwnd )
{
   TBBUTTON tb;
   register int c;

   Amuser_WritePPString( szToolbar, NULL, NULL );
   for( c = 0; SendMessage( hwndToolBar, TB_GETBUTTON, c, (LPARAM)(LPSTR)&tb ); ++c )
      {
      char szKey[ 10 ];

      /* For separators, write Sep followed by the separator
       * width.
       */
      wsprintf( szKey, "Tool%u", c );
      if( tb.fsStyle == TBSTYLE_SEP )
         wsprintf( lpTmpBuf, "Sep %u", tb.iBitmap );
      else
         WriteCommandAssignment( tb.idCommand, lpTmpBuf );
      Amuser_WritePPString( szToolbar, szKey, lpTmpBuf );
      }
   Amuser_WritePPInt( szToolbar, "Orientation", nToolbarPos );
   return( TRUE );
}

/* This function loads the CMDREC table from the [Keyboard]
 * section in the configuration file.
 */
void FASTCALL LoadKeyboard( void )
{
   register int c;
   int cbKeyBuf;
   char * psz;

   INITIALISE_PTR(psz);

   /* Look for and load the keyboard section.
    */
   cbKeyBuf = 32000;
   if( fNewMemory( &psz, cbKeyBuf ) )
      {
      fKeyboardChanged = FALSE;
      if( Amuser_GetPPString( szKeyboard, NULL, "", psz, 2048 ) )
         {
         for( c = 0; psz[ c ]; ++c )
            {
            char szCommand[ 40 ];
            register int i;
            char * pszData;
            char szData[ 60 ];
            int wKeyCode;
            CMDREC cmd;
      
            /* Decode the command and keycode.
             */
            Amuser_GetPPString( szKeyboard, psz + c, "", szData, sizeof(szData) );

            /* Read the key code.
             */
            pszData = szData;
            pszData = IniReadInt( pszData, &wKeyCode );

            /* Okay. szData is the command name, so extract it into the
             * szCommand buffer and move pszData forward to point to any
             * parameters.
             */
            while( *pszData == ' ' )
               ++pszData;
            for( i = 0; *pszData && *pszData != ' ' && i < sizeof(szCommand) - 1; ++i )
               szCommand[ i ] = *pszData++;
            szCommand[ i ] = '\0';
            while( *pszData == ' ' )
               ++pszData;


            /* Locate the command.
             * Now locate the command in the command list and
             * plug the key code.
             */
            if( _strcmpi( szCommand, "FileBlink" ) == 0 )
               {
               cmd.iCommand = GetBlinkCommandID( pszData );
               CTree_ChangeKey( cmd.iCommand, (WORD)wKeyCode );
               }
            else if( _strcmpi( szCommand, "TermBtn" ) == 0 )
               {
               cmd.iCommand = GetTerminalCommandID( pszData );
               CTree_ChangeKey( cmd.iCommand, (WORD)wKeyCode );
               }
            else if( _strcmpi( szCommand, "RunApp" ) == 0 )
               {
               cmd.iCommand = GetRunAppCommandID( pszData );
               CTree_ChangeKey( cmd.iCommand, (WORD)wKeyCode );
               }
            else if( CTree_FindCommandName( szCommand, &cmd ) )
               CTree_ChangeKey( cmd.iCommand, (WORD)wKeyCode );
            c += strlen( &psz[ c ] );
            }
         fKeyboardChanged = TRUE;
         }
      FreeMemory( &psz );
      }
}

/* This function saves the CMDREC table to the [Keyboard]
 * section in the configuration file.
 */
void FASTCALL SaveKeyboard( void )
{
   CMDREC cmd;
   HCMD hCmd;

   Amuser_WritePPString( szKeyboard, NULL, NULL );
   if( fKeyboardChanged )
      {
      register int c;

      hCmd = NULL;
      for( c = 0; NULL != ( hCmd = CTree_EnumTree( hCmd, &cmd ) ); ++c )
         {
         char szKey[ 10 ];

         /* Write the KeyMap entry.
          */
         wsprintf( szKey, "Key%u", c );
         wsprintf( lpTmpBuf, "%u,", cmd.wKey );
         WriteCommandAssignment( cmd.iCommand, lpTmpBuf + strlen( lpTmpBuf ) );
         Amuser_WritePPString( szKeyboard, szKey, lpTmpBuf );
         }
      }
}

/* This function loops through the commands in CMDREC and
 * assigns the key names to the appropriate commands.
 */
void FASTCALL AssignKeynamesToMenu( void )
{
   CMDREC cmd;
   HCMD hCmd;

   /* Loop for all commands.
    */
   hCmd = NULL;
   while( NULL != ( hCmd = CTree_EnumTree( hCmd, &cmd ) ) )
      if( cmd.wKey )
         {
         char sz[ 80 ];

         if( !( cmd.iActiveMode & WIN_NOMENU ) )
            if( GetMenuString( hMainMenu, cmd.iCommand, sz, sizeof( sz ), MF_BYCOMMAND ) )
               {
               WORD wKey;
               WORD wModifier;
               char * pTab;

               wModifier = GETMODIFIER( cmd.wKey );
               wKey = GETKEY( cmd.wKey );
               pTab = strchr( sz, '\t' );
               if( pTab )
                  *pTab = '\0';
               strcat( sz, "\t" );
               if( wModifier & HK_CTRL )
                  strcat( sz, "Ctrl+" );
               if( wModifier & HK_ALT )
                  strcat( sz, "Alt+" );
               if( wModifier & HK_SHIFT )
                  strcat( sz, "Shift+" );
               if( wKey )
                  Amctl_NewGetKeyNameText( wKey, sz + strlen( sz ), ( sizeof( sz ) -  strlen( sz ) ) - 1 );
               ModifyMenu( hMainMenu, cmd.iCommand, MF_BYCOMMAND|MF_STRING, cmd.iCommand, sz );
               }
         }
}

/* This function writes a command name to the specified registry section
 * and key.
 */
void FASTCALL WriteCommandAssignment( int idCommand, char * pBuf )
{
   if( idCommand >= IDM_EXTAPPFIRST && idCommand <= IDM_EXTAPPLAST )
      {
      APPINFO appinfo;

      /* For external applications, write a RunApp string followed
       * by the complete command name.
       */
      wsprintf( pBuf, "RunApp " );
      GetExternalApp( idCommand, &appinfo );
      QuotifyFilename( appinfo.szAppPath );
      strcat( pBuf, appinfo.szAppPath );
      if( *appinfo.szCmdLine )
         {
         strcat( pBuf, " " );
         strcat( pBuf, appinfo.szCmdLine );
         }
      }
   else if( idCommand >= IDM_BLINKMANFIRST && idCommand <= IDM_BLINKMANLAST )
      {
      /* Blink manager commands appear as Blink
       * followed by the command name.
       */
      wsprintf( pBuf, "FileBlink " );
      GetBlinkCommandName( idCommand, pBuf + strlen( pBuf ) );
      }
   else if( idCommand >= IDM_TERMINALFIRST && idCommand <= IDM_TERMINALLAST )
      {
      /* Terminal buttons appear as 'TermBtn' followed
       * by the terminal name.
       */
      wsprintf( pBuf, "TermBtn " );
      GetTerminalCommandName( idCommand, pBuf + strlen( pBuf ) );
      }
   else
      {
      CMDREC cmd;

      /* For anything else, extract the command name
       * and write that immediately.
       */
      cmd.iCommand = idCommand;
      if( CTree_GetCommand( &cmd ) )
         strcpy( pBuf, cmd.lpszCommand );
      }
}

/* This function deletes all custom button mappings.
 */
void FASTCALL DeleteCustomButtons( void )
{
   Amuser_WritePPString( szCustomBitmaps, NULL, NULL );
}

/* This function loads the information for a button's custom
 * toolbar bitmap.
 */
BOOL FASTCALL LoadCustomButtonBitmap( CMDREC FAR * lpCmd )
{
   char szCmdName[ 540 ];

   /* Write the custom bitmaps entry.
    */
   WriteCommandAssignment( lpCmd->iCommand, szCmdName );
   if( Amuser_GetPPString( szCustomBitmaps, szCmdName, "", lpTmpBuf, LEN_TEMPBUF ) )
      {
      char * pszFilename;
      char * pBuf;
      int index;

      /* Extract the filename, if there is one.
       */
      pBuf = lpTmpBuf;
      pszFilename = BTNBMP_GRID;
      if( *pBuf != ',' )
         {
         register int c;
         BOOL fInQuote;

         fInQuote = FALSE;
         for( c = 0; *pBuf && *pBuf != ','; ++pBuf )
            {
            if( *pBuf == ' ' && !fInQuote )
               break;
            if( *pBuf == '"' )
               fInQuote = !fInQuote;
            else if( c < _MAX_PATH - 1 )
               lpPathBuf[ c++ ] = *pBuf;
            }
         lpPathBuf[ c ] = '\0';
         pszFilename = lpPathBuf;
         }

      /* Extract the index
       */
      index = 0;
      if( *pBuf == ',' )
         {
         ++pBuf;
         index = atoi( pBuf );
         }

      /* Save these to the CMDREC structure.
       */
      SetCommandCustomBitmap( lpCmd, pszFilename, index );
      return( TRUE );
      }
   return( FALSE );
}

/* This function saves the custom bitmap entries to the
 * configuration store.
 */
void FASTCALL SaveCustomButtonBitmap( CMDREC FAR * lpCmd )
{
   LPSTR lpszCmdName;
   INITIALISE_PTR(lpszCmdName);

   /* Write the custom bitmaps entry.
    */
   if( fNewMemory( &lpszCmdName, 540 ) )
      {
      WriteCommandAssignment( lpCmd->iCommand, lpszCmdName );
      if( !lpCmd->btnBmp.fIsValid )
         Amuser_WritePPString( szCustomBitmaps, lpszCmdName, NULL );
      else
         {
         if( 0 == HIWORD( lpCmd->btnBmp.pszFilename ) )
            lpTmpBuf[ 0 ] = '\0';
         else
            {
            /* Write filename, suitable wrapped in quotes, and
             * the index value.
             */
            strcpy( lpTmpBuf, lpCmd->btnBmp.pszFilename );
            QuotifyFilename( lpTmpBuf );
            }
         strcat( lpTmpBuf, "," );
         _itoa( lpCmd->btnBmp.index, lpTmpBuf + strlen( lpTmpBuf ), 10 );
         Amuser_WritePPString( szCustomBitmaps, lpszCmdName, lpTmpBuf );
         }
      SaveToolbar( NULL );
      FreeMemory( &lpszCmdName );
      }
}

/* This function parses the Palantir command line.
 */
BOOL FASTCALL ParseCommandLine( LPSTR lpszCmdLine )
{
   while( *lpszCmdLine && *lpszCmdLine != 13 )
      {
      register int c;

      while( *lpszCmdLine == ' ' )
         ++lpszCmdLine;
      if( *lpszCmdLine == '/' || *lpszCmdLine == '-' )
         {
         char szCmdSwitch[ MAX_CMDSWITCHLEN + 1 ];
         BOOL fValue;

         /* The command line item is a switch. Parse off the switch into the szCmdSwitch[]
          * buffer, then set lpszCmdLine to point to either the switch value or the next
          * command line item.
          */
         ++lpszCmdLine;
         for( c = 0; *lpszCmdLine && *lpszCmdLine != '=' && *lpszCmdLine != ' '; ++lpszCmdLine )
            if( c < MAX_CMDSWITCHLEN )
               szCmdSwitch[ c++ ] = *lpszCmdLine;
         szCmdSwitch[ c ] = '\0';
         while( *lpszCmdLine == ' ' )
            ++lpszCmdLine;
         fValue = *lpszCmdLine == '=';
         if( fValue )
            ++lpszCmdLine;
         while( *lpszCmdLine == ' ' )
            ++lpszCmdLine;
         for( c = 0; c < MAX_CMDSWITCHES; ++c )
            if( _strcmpi( szCmdSwitch, swi[ c ].szCmdSwitch ) == 0 )
               {
               switch( swi[ c ].wType )
                  {
                  case SWI_STRING: {
                     if( fValue )
                        {
                        register UINT d;
                        char chEnd = ' ';

                        if( *lpszCmdLine == '\"' )
                           {
                           chEnd = '\"';
                           ++lpszCmdLine;
                           }
                        for( d = 0; *lpszCmdLine && *lpszCmdLine != chEnd; )
                           {
                           if( d < swi[ c ].wMax )
                              swi[ c ].lpStr[ d++ ] = *lpszCmdLine;
                           ++lpszCmdLine;
                           }
                        swi[ c ].lpStr[ d ] = '\0';
                        if( *lpszCmdLine == chEnd )
                           ++lpszCmdLine;
                        }
                     if( swi[ c ].lpBool )
                        *((BOOL FAR *)swi[ c ].lpBool) = TRUE;
                     fValue = FALSE;
                     break;
                     }

                  case SWI_BOOL: {
                     BOOL f;

                     /* Specifying the switch without any value is the same as specifying
                      * a '1' or 'true'.
                      */
                     f = 0;
                     if( !fValue || *lpszCmdLine == '1' || lstrcmpi( lpszCmdLine, "true" ) == 0 )
                        f = 1;
                     if( *lpszCmdLine == '0' || lstrcmpi( lpszCmdLine, "false" ) == 0 )
                        f = 0;
                     *((BOOL FAR *)swi[ c ].lpBool) = f;
                     break;
                     }

                  case SWI_LONGINT: {
                     DWORD dw;

                     for( dw = 0; isdigit( *lpszCmdLine ); ++lpszCmdLine )
                        dw = ( dw * 10 ) + ( *lpszCmdLine - '0' );
                     *((DWORD FAR *)swi[ c ].lpBool) = dw;
                     break;
                     }

                  case SWI_INTEGER: {
                     UINT w;

                     if( *lpszCmdLine == '\'' )
                        {
                        ++lpszCmdLine;
                        w = toupper( *lpszCmdLine );
                        ++lpszCmdLine;
                        }
                     else
                        for( w = 0; isdigit( *lpszCmdLine ); ++lpszCmdLine )
                           w = ( w * 10 ) + ( *lpszCmdLine - '0' );
                     *((UINT FAR *)swi[ c ].lpBool) = w;
                     break;
                     }
                  }
               if( fValue ) {
                  while( *lpszCmdLine && *lpszCmdLine != ' ' )
                     ++lpszCmdLine;
                  }
               break;
               }
         }
      else if( *szLoginUsername == '\0' )
         {
         char chTerm = ' ';

         /* Handle the login name.
          */
         if( *lpszCmdLine == '\"' )
            {
            ++lpszCmdLine;
            chTerm = '\"';
            }
         for( c = 0; c < LEN_LOGINNAME-1 && *lpszCmdLine && *lpszCmdLine != chTerm; ++c )
            szLoginUsername[ c ] = *lpszCmdLine++;
         szLoginUsername[ c ] = '\0';
         _strlwr( szLoginUsername );
         if( *lpszCmdLine == '\"' )
            ++lpszCmdLine;
         }
      else if( szLoginPassword[ 0 ] == '\0' )
         {
         char chTerm = ' ';

         if( *lpszCmdLine == '\"' )
            {
            ++lpszCmdLine;
            chTerm = '\"';
            }
         for( c = 0; c < LEN_LOGINPASSWORD && *lpszCmdLine && *lpszCmdLine != chTerm; ++c )
            {
            char ch;

            if( ( ch = *lpszCmdLine++ ) == '\\' )
               {
               switch( ch = *lpszCmdLine++ )
                  {
                  case '\0':  ch = '\\';  --lpszCmdLine; break;
                  case 't':   ch = '\t';  break;
                  case 'b':   ch = '\b';  break;
                  case 'e':   ch = 27;    break;
                  case '\\':  ch = '\\';  break;
                  }
               }
            szLoginPassword[ c ] = ch;
            }
         if( *lpszCmdLine == '\"' )
            ++lpszCmdLine;
         szLoginPassword[ c ] = '\0';
         }
      else ++lpszCmdLine;
      }
   return( TRUE );
}

/* This function reads a directory path from the users configuration
 * file, validates it and ensures that the directory exists.
 */
char * FASTCALL ReadDirectoryPath( int fnDirectory, char * npDirName )
{
   char szDefDir[ _MAX_DIR ];
   char szDir[ _MAX_DIR ];
   int length;

   /* Construct the default directory
    */
   // VistaAlert
   if( (_stricmp( npDirName, "Script" ) == 0 ) || ( _stricmp( npDirName, "Addons" ) == 0 ) || (_stricmp( npDirName, "Data" ) == 0 ) )
   {
      // !!SM!! 2.56.3053
      #ifdef USE_HOME_FOLDER
      if (!fUseU3 && fFirstRun && _stricmp( npDirName, "Data" ) == 0)
      {
         // For v2.6 and later, the Data directory is in the user's app
         // data folder
         SHGetSpecialFolderPathA(NULL, szDefDir, CSIDL_PERSONAL, TRUE);
         strcat(szDefDir, "\\Ameol\\");
         strcat(szDefDir, npDirName);
      }
      else
      #endif
      if (!fUseU3 || (_stricmp( npDirName, "Data" ) != 0))
         strcat( strcpy( szDefDir, pszAmeolDir ), npDirName );
      else
      {
         Amuser_GetActiveUserDirectory( szDefDir, _MAX_DIR );
         strcat( szDefDir, "\\" );
         strcat( szDefDir, npDirName );
      }
   }
   else
   {
      Amuser_GetActiveUserDirectory( szDefDir, _MAX_DIR );
      strcat( szDefDir, "\\" );
      strcat( szDefDir, npDirName );
   }

   if (!fUseU3)
   {
   /* Get the specified directory from the configuration file
      */
      if( 0 == Amuser_GetLMString( szDirects, npDirName, "", szDir, _MAX_DIR ) )
         if( 0 == Amuser_GetPPString( szDirects, npDirName, "", szDir, _MAX_DIR ) )
         {
            strcpy( szDir, szDefDir );
            Amuser_WritePPString( szDirects, npDirName, szDefDir );
         }
   }
   else
   {
      strcpy( szDir, szDefDir );
      Amuser_WritePPString( szDirects, npDirName, szDefDir );
   }
   length = strlen( szDir );
   if( length > 0 && szDir[ length - 1 ] == '\\' )
      szDir[ length - 1 ] = '\0';
   if( !( length > 0 && length <= 3 && szDir[ 1 ] == ':' ) )
      {
      if( !Amdir_QueryDirectory( szDir ) )
         if( !Amdir_Create( szDir ) )
            {
            if( _strcmpi( npDirName, "Data" ) == 0 )
            {
               /* Prompt if data path doesn't exist
                */
               if( IDNO == fMessageBox( NULL, 0, "Data directory not found - continue loading Ameol?", MB_YESNO|MB_ICONINFORMATION ) )
               {
                  ExitAmeol( NULL, 0 );
                  return( NULL );
               }
            }
            if( _strcmpi( szDefDir, szDir ) )
               {
               HideIntroduction();
               wsprintf( lpTmpBuf, GS( IDS_STR75 ), (LPSTR)szDir, (LPSTR)szDefDir );
               MessageBox( GetFocus(), lpTmpBuf, szAppName, MB_OK|MB_ICONEXCLAMATION );
               }  
            /* Sanity check - if even the default directory doesn't exist, create it!
             * If we can't create it - we've a problem!
             */
            if( !Amdir_QueryDirectory( szDefDir ) )
               if( !Amdir_Create( szDefDir ) )
                  {
                  wsprintf( lpTmpBuf, GS( IDS_STR123 ), (LPSTR)szDefDir );
                  MessageBox( GetFocus(), lpTmpBuf, szAppName, MB_OK|MB_ICONEXCLAMATION );
                  return( NULL );
                  }
            strcpy( szDir, szDefDir );
            Amuser_WritePPString( szDirects, npDirName, szDefDir );
            }
      }
   Amuser_SetAppDirectory( fnDirectory, szDir );
   return( CreateDirectoryPath( NULL, szDir ) );
}

/* This function handles all messages for the main window.
 */
LRESULT EXPORT FAR PASCAL MainWndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   /* First deal with any registered Find message.
    */
   if( msg == uFindReplaceMsg )
      {
      FINDREPLACE FAR * lpfr;

      lpfr = (FINDREPLACE FAR *)lParam;
      if( lpfr->Flags & FR_DIALOGTERM )
         hFindDlg = NULL;
      if( lpfr->Flags & FR_FINDNEXT )
         {
         if( !CmdFindNext( lpfr ) )
            fMessageBox( hFindDlg, 0, GS(IDS_STR345), MB_OK|MB_ICONEXCLAMATION );
         }
      if( (lpfr->Flags & FR_REPLACE) || (lpfr->Flags & FR_REPLACEALL) )
         {
         if( !CmdReplaceText( lpfr ) )
            fMessageBox( hFindDlg, 0, GS(IDS_STR345), MB_OK|MB_ICONEXCLAMATION );
         }
      if( lpfr->Flags & FR_REPLACEALL )
         while( CmdReplaceText( lpfr ) );
      return( 0 );
      }

   /* Handle mouse wheel messages.
    */
   if( msg == msgMouseWheel )
      {
         wParam = wParam << 16;
         return( SendMessage( GetFocus(), WM_MOUSEWHEEL, wParam, lParam ) );
      }

   /* Next, deal with any RAS messages.
    */
   if( msg == uRasMsg )
      {
      RASCONNSTATE rcs;
      DWORD dwError;

      /* Dereference the parameters.
       */
      rcs = (RASCONNSTATE)wParam;
      dwError = (DWORD)lParam;

      /* If an error occurred, abort the connection
       * and display the error string.
       */
      if( 0 != dwError )
         {
         nRasConnect = RCS_ERROR;
         dwRasError = dwError;
         return( TRUE );
         }

      /* Show progress messages.
       */
      switch( rcs )
         {
         case RASCS_Disconnected:
            nRasConnect = RCS_ABORTED;
            break;

         case RASCS_ConnectDevice:
            if( !fDoneConnectDevice )
               OnlineStatusText( GS(IDS_STR914), beCur.rd.szRASEntryName );
            fDoneConnectDevice = TRUE;
            break;

         case RASCS_DeviceConnected:
            if (!fDoneDeviceConnected )
               OnlineStatusText( GS(IDS_STR650), beCur.rd.szRASEntryName );
            fDoneDeviceConnected = TRUE;
            break;

         case RASCS_Connected:
            OnlineStatusText( GS(IDS_STR916) );
            nRasConnect = RCS_CONNECTED;
            fDoneConnectDevice = FALSE;
            fDoneDeviceConnected = FALSE;
            break;
         }
      return( TRUE );
      }

   switch( msg )
      {
      HANDLE_MSG( hwnd, WM_QUERYOPEN, MainWnd_OnQueryOpen );
      HANDLE_MSG( hwnd, WM_MOVE, MainWnd_OnMove );
      HANDLE_MSG( hwnd, WM_SIZE, MainWnd_OnSize );
      HANDLE_MSG( hwnd, WM_COMMAND, MainWnd_OnCommand );
      HANDLE_MSG( hwnd, WM_ACTIVATE, MainWnd_OnActivate );
      HANDLE_MSG( hwnd, WM_CREATE, MainWnd_OnCreate );
      HANDLE_MSG( hwnd, WM_ENDSESSION, MainWnd_OnEndSession );
      HANDLE_MSG( hwnd, WM_QUERYENDSESSION, MainWnd_OnQueryEndSession );
      HANDLE_MSG( hwnd, WM_MENUSELECT, MainWnd_OnMenuSelect );
      HANDLE_MSG( hwnd, WM_SETTINGCHANGE, MainWnd_OnSettingChange );
      HANDLE_MSG( hwnd, WM_DEVMODECHANGE, MainWnd_OnDevModeChange );
      HANDLE_MSG( hwnd, WM_SYSCOLORCHANGE, MainWnd_OnSysColorChange );
      HANDLE_MSG( hwnd, WM_FONTCHANGE, MainWnd_OnFontChange );
      HANDLE_MSG( hwnd, WM_TIMER, MainWnd_OnTimer );
      HANDLE_MSG( hwnd, WM_INITMENU, MainWnd_OnInitMenu );
      HANDLE_MSG( hwnd, WM_NOTIFY, MainWnd_OnNotify );
      
      
      case WM_POWERBROADCAST: /*!!SM!!*/
         {
            /* Machine is going into standby or suspend */
            if (( wParam == PBT_APMSUSPEND ) || (wParam == PBT_APMSTANDBY))
            {
               KillTimer( hwndFrame, TID_SCHEDULER );
               Amdb_SweepAndDiscard();
               Amuser_WriteWindowState( "Main", hwnd );
               SaveDefaultWindows();
            }
            /* Machine is resuming from standby or suspend */
            if (( wParam == PBT_APMRESUMESUSPEND ) || (wParam == PBT_APMRESUMESTANDBY))
            {
               if( !( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) )
                  LocateMissedItems( );

               if( 0 == SetTimer( hwndFrame, TID_SCHEDULER, (UINT)60000, NULL ) )
                  {
                  MessageBox( NULL, GS(IDS_STR1018), szAppName, MB_OK|MB_ICONEXCLAMATION );
                  Amevent_AddEventLogItem( ETYP_SEVERE|ETYP_INITTERM, GS(IDS_STR1019) );
                  }

            }
            return( TRUE );
         }           

      case WM_DROPFILES: {
/*       char szClassName[ 40 ];
         BOOL fEdit;
         BOOL fBigEdit;

         GetClassName( lpmsg->hwnd, szClassName, sizeof( szClassName ) );
         fEdit = _strcmpi( szClassName, "edit" ) == 0;
         fBigEdit = fEdit && ( GetWindowStyle( lpmsg->hwnd ) & ES_MULTILINE );
         if( fBigEdit )
*/          SendMessage( hwnd, WM_DROPFILES, wParam, 0L );

         }
         return( 0L );

      case WM_DESTROY:
         MainWnd_OnDestroy( hwnd );
         return( 0L );

      case WM_CLOSE:
         MainWnd_OnClose( hwnd );
         return( 0L );

      case WM_EXEC_CIX:
         Exec_CIX2();
         return( 0L );

      case WM_BLINK2:
         Blink( (OBCLSID)lParam );
         return( 0L );

      case WM_NEWSERROR:
         if( wParam > 0 )
            wsprintf( lpTmpBuf, GS(IDS_STR721), (int)wParam, (LPSTR)lParam );
         else
            strcpy( lpTmpBuf, (LPSTR)lParam );
         fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
         fNewsErrorReported = FALSE;
         return( 0L );

      case WM_DRAWITEM: {
         LPDRAWITEMSTRUCT lpdis;
         BITMAP bmp;
         HBITMAP hbmp;

         /* This function draws the 'letterbox' icon on the
          * outbasket icon part of the status bar. There's no
          * check to see if this is indeed what needs to be
          * drawn, so be careful if you add another ownerdraw
          * item to the status bar!
          */
         lpdis = (LPDRAWITEMSTRUCT)lParam;
         if( 0 != Amob_GetCount( OBF_UNFLAGGED ) )
         {
            hbmp = LoadBitmap( hRscLib, MAKEINTRESOURCE( IDB_OUTBASKET ) );
            GetObject( hbmp, sizeof( BITMAP ), &bmp );
            Amuser_DrawBitmap( lpdis->hDC,
                          lpdis->rcItem.left + ( ( lpdis->rcItem.right - lpdis->rcItem.left ) - bmp.bmWidth ) / 2,
                          lpdis->rcItem.top + ( ( lpdis->rcItem.bottom - lpdis->rcItem.top ) - bmp.bmHeight ) / 2,
                          bmp.bmWidth,
                          bmp.bmHeight,
                          hbmp, 0 );
            DeleteBitmap( hbmp );
         }
         else if( 0 != Amob_GetCount( 0 ) )
         {
            hbmp = LoadBitmap( hRscLib, MAKEINTRESOURCE( IDB_OUTBASKETLOCKED ) );
            GetObject( hbmp, sizeof( BITMAP ), &bmp );
            Amuser_DrawBitmap( lpdis->hDC,
                          lpdis->rcItem.left + ( ( lpdis->rcItem.right - lpdis->rcItem.left ) - bmp.bmWidth ) / 2,
                          lpdis->rcItem.top + ( ( lpdis->rcItem.bottom - lpdis->rcItem.top ) - bmp.bmHeight ) / 2,
                          bmp.bmWidth,
                          bmp.bmHeight,
                          hbmp, 0 );
            DeleteBitmap( hbmp );
         }
         return( 0L );
         }

      case WM_UPDATE_UNREAD: {
         DWORD dwTotalUnread;

         dwTotalUnread = Amdb_GetTotalUnread();
         ShowUnreadMessageCount();
         SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_NEXT, dwTotalUnread > 0 );
         SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_NEXTPRIORITY, Amdb_GetTotalUnreadPriority() > 0 );
         return( 0L );
         }

      case WM_COPYDATA: {
         PCOPYDATASTRUCT pCopyDataStruct;

         /* Under Win32, this is the means by which we fire up a
          * URL under another copy of Ameol.
          */
         pCopyDataStruct = (PCOPYDATASTRUCT)lParam;
         if( pCopyDataStruct->dwData == WM_PARSEURL )
            {
            BringWindowToTop( hwnd );
            SetForegroundWindow( hwnd );
            SendMessage( hwnd, WM_PARSEURL, 0, (LPARAM)pCopyDataStruct->lpData );
            }
         return( 0L );
         }

      case WM_PARSEURL:
         HideIntroduction();
         EnsureMainWindowOpen();
         CommonHotspotHandler( hwnd, (LPSTR)lParam );
         return( 0L );

      case WM_AMEOL_INIT1:
         /* Update the menu bar
          */
         ++wInitState;
         if( !fNoAddons )
            InstallAllAddons( hwnd );

         /* Do we start in online mode?
          */
         if( fStartOnline )
            CmdOnline();

         /* Load the databases
          */
         LoadScriptList();

         /* All done!
          */
         if( !fNoAddons )
            LoadAllAddons();

         SendMessage( hwnd, WM_UPDATE_UNREAD, 0, 0L );

         /* Load the list of scheduled items.
          */
         LoadScheduledItems();

         /* Start the schedule timer.
          */
         if( 0 == SetTimer( hwndFrame, TID_SCHEDULER, (UINT)60000, NULL ) )
            {
            HideIntroduction();
            MessageBox( NULL, GS(IDS_STR1018), szAppName, MB_OK|MB_ICONEXCLAMATION );
            Amevent_AddEventLogItem( ETYP_SEVERE|ETYP_INITTERM, GS(IDS_STR1019) );
            }

         /* Set default key sequences.
          */
         CTree_SetDefaultKey();

         /* Open the default windows.
          */
         if( fFirstTimeThisUser )
         {
            fAltMsgLayout = TRUE;
            Amuser_WritePPInt( szSettings, "AltMsgLayout", fAltMsgLayout );
         }
         OpenDefaultWindows();

         /* Reload the toolbar.
          */
         LoadToolbar( hwnd, fShowToolBar );
         SendMessage( hwnd, WM_UPDATE_UNREAD, 0, 0L );

         /* Assign key names to menu items with short-cuts.
          */
         LoadKeyboard();
         AssignKeynamesToMenu();
         DrawMenuBar( hwndFrame );

         /* Fixup toolbar buttons
          */
         SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_STOP, fOnline );
         SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_ONLINE, !fOnline );
         ToolbarState_Permissions();
         ToolbarState_Printer();
         ToolbarState_Import();
         ToolbarState_EditWindow();

         /* Run scheduled actions.
          */
         fInitialising = FALSE;
         SetLastNonIdleTime();
         Am_PlaySound( "Open" );
         if( !fAutoConnect && fAutoCheck )
            if( fMessageBox( hwnd, 0, GS(IDS_STR1252), MB_YESNO|MB_ICONINFORMATION ) == IDYES )
            {
               PCAT pcat;
               for( pcat = Amdb_GetFirstCategory(); pcat; pcat = Amdb_GetNextCategory( pcat ) )
                  Amdb_CheckAndRepair( hwnd, (LPVOID)pcat, FALSE );
            }

         Amuser_WritePPInt( szSettings, "AutoCheck", TRUE );

         /* Show Prefs dialog is running with /sageset
          * switch and this is not the first time running
          */
         if( fSageSettings && !fFirstRun )
         {
            PreferencesDialog( hwnd, 0 );
            fAutoConnect = FALSE;
            fQuitAfterConnect = FALSE;
         }

         /* Read in the Tutorial.
          */
         wsprintf( lpPathBuf, "%sTUTORIAL.SCR", pszAmeolDir );
         if( fFirstRun && Amfile_QueryFile( lpPathBuf ) )
            {
            UnarcFile( hwnd, lpPathBuf );
            if( CIXImportAndArchive( hwnd, lpPathBuf, PARSEFLG_IMPORT|PARSEFLG_NORULES ) )
               {
               while( fParsing && NULL != fpParser )
                  fpParser();
               }
            fForceNextUnread = TRUE;
            }

         /* Deal with unparsed scratchpads.
          */
         if( !fQuitAfterConnect && fUseCIX )
            HandleUnparsedScratchpads( hwnd, TRUE );

         if( !( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) )
            RunScheduledStartupActions();

         /* Blink at first run?
          */
         if (fFirstTimeThisUser)
         {
            DoFirstTimeUserActions();
            if ( IDYES == MessageBox( NULL, GS(IDS_STR1257), szAppName, MB_YESNO|MB_ICONQUESTION ) )
               fAutoConnect = TRUE;
         }

         /* We show next message when we start.
          */
         if( fInitNewsgroup )
            SendMessage( hwnd, WM_PARSEURL, 0, (LPARAM)(LPSTR)szNewsgroup );
         else if( fMailto )
            SendMessage( hwnd, WM_PARSEURL, 0, (LPARAM)(LPSTR)szMailtoAddress );
         else if( fCixLink )
            SendMessage( hwnd, WM_PARSEURL, 0, (LPARAM)(LPSTR)szCixLink );
         else if( fAutoConnect )
            {
            int id;

            /* Do an automatic blink.
             */
            id = GetBlinkCommandID( *szAutoConnectMode ? szAutoConnectMode : "Full" );
            PostCommand( hwnd, id, 1 );
            }
         else if( fShowNext && Amdb_GetTotalUnread() > 0 )
            SendCommand( hwnd, IDM_NEXT, 1 );
         else if( fShowFirst && Amdb_GetTotalUnread() > 0 )
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

         else {
            if( fAutoPurge )
               {
               LPVOID pFolder;

               ParseFolderPathname( szAutoPurge, &pFolder, FALSE, 0 );
               if( NULL == pFolder )
                  pFolder = Amdb_GetFirstCategory();
               if( NULL != pFolder )
                  {
                  HideIntroduction();
                  Purge( hwnd, pFolder, FALSE );
                  }
               if( fQuitAfterConnect && !fNoExit )
                  PostMessage( hwnd, WM_CLOSE, 0, 0L );
               fQuitAfterConnect = FALSE;
               }
            }

         /* If this is the first time for this user, display the
          * first connect dialog.
          */
         if( fFirstTimeThisUser )
            {
            fShowTips = TRUE;
            HideIntroduction();
            if (!fAutoConnect)
               CmdStartWiz( hwnd );
            }
         else
            HideIntroduction();
         if( fShowTips && !fFirstTimeThisUser )
            CmdShowTips( hwnd );
         return( 0L );

      default:
         break;
      }
   return( DefFrameProc( hwnd, hwndMDIClient, msg, wParam, lParam ) );
}

/* Do the first time new user actions.
 */
void FASTCALL DoFirstTimeUserActions( void )
{
   // Download full CIX list
   if( !Amob_Find( OBTYPE_GETCIXLIST, NULL ) )
   {
      Amob_New( OBTYPE_GETCIXLIST, NULL );
      Amob_Commit( NULL, OBTYPE_GETCIXLIST, NULL );
      Amob_Delete( OBTYPE_GETCIXLIST, NULL );
   }
}

/* This function ensures that the frame window is open by
 * restoring it to its last full view state.
 */
void FASTCALL EnsureMainWindowOpen( void )
{
   if( IsIconic( hwndFrame ) )
      if( fLastStateWasMaximised )
         ShowWindow( hwndFrame, SW_SHOWMAXIMIZED );
      else
         ShowWindow( hwndFrame, SW_RESTORE );
}

/* This function is called when an edit window is made active.
 * It adjusts the toolbar for the edit window.
 */
void FASTCALL ToolbarState_EditWindow( void )
{
   ToolbarState_MessageWindow();
   ToolbarState_TopicSelected();
   ToolbarState_CopyPaste();
}

/* This function disables/enables those toolbar buttons
 * that depend on an active topic.
 */
void FASTCALL ToolbarState_TopicSelected( void )
{
   CURMSG curmsg;
   BOOL fCixTopic;
   BOOL fNewsTopic;
   BOOL fMailTopic;
   BOOL fResigned;
   BOOL fHasMsgs;

   /* Set the topic type flag.
    */
   fCixTopic = FALSE;
   fNewsTopic = FALSE;
   fMailTopic = FALSE;
   fHasMsgs = FALSE;
   fResigned = FALSE;
   if( iActiveMode & (WIN_INBASK|WIN_MESSAGE|WIN_THREAD) )
      {
      Ameol2_GetCurrentTopic( &curmsg );
      if( NULL != curmsg.ptl )
         {
         /* Set topic type flags.
          */
         fCixTopic = Amdb_GetTopicType( curmsg.ptl ) == FTYPE_CIX;
         fNewsTopic = Amdb_GetTopicType( curmsg.ptl ) == FTYPE_NEWS;
         fMailTopic = Amdb_GetTopicType( curmsg.ptl ) == FTYPE_MAIL;
         fResigned = Amdb_IsResignedTopic( curmsg.ptl );
         }
      }

   /* Now set the button state.
    */
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_UPLOADFILE, fCixTopic );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_DOWNLOAD, fCixTopic );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_FILELIST, fCixTopic );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_RESIGN, fCixTopic );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_UNSUBSCRIBE, fNewsTopic && !fResigned );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_RETHREADARTICLES, fMailTopic|fNewsTopic );
}


void FASTCALL CheckMenuStates( void ) // !!SM!! 2.55
{

   HWND hwndEdit;
   BOOL fEnabled;
   char szClsName[ 16 ];
   
   fEnabled = FALSE;
   hwndEdit = GetFocus();
   GetClassName( hwndEdit, szClsName, 16 );
   if( _strcmpi( szClsName, "edit" ) == 0  || _strcmpi( szClsName, WC_BIGEDIT ) == 0 ) // !!SM!! 2.55.2032
      if( GetWindowStyle( hwndEdit ) & ES_MULTILINE )
         fEnabled = TRUE;

   MenuEnable( hMainMenu, IDM_INSERTFILE, fEnabled );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_INSERTFILE, fEnabled );
}
/* This function disables/enables those toolbar buttons
 * depending on whether Copy/Paste is available.
 */
void FASTCALL ToolbarState_CopyPaste( void )
{
   BOOL fHasSelection;
   BOOL fSelectAll;
   HWND hwndEdit;
   BOOL fCanUndo;
   BOOL fCanRedo;
   BOOL fRdOnly;
   BOOL fIsEdit; // !!SM!!2039
   HEDIT hedit;
   BOOL fFind;
   BOOL fPaste;

   /* First, set flags for each type of button group depending
    * on whether or not there is an edit window.
    */
   hwndEdit = GetFocus();
   hedit = Editmap_Open( hwndEdit );
   if( ETYP_NOTEDIT != hedit )
      {
      BEC_SELECTION bsel;

      Editmap_GetSelection( hedit, hwndEdit, &bsel );
      fHasSelection = bsel.lo != bsel.hi;
      fRdOnly = ( GetWindowStyle( hwndEdit ) & ES_READONLY ) == ES_READONLY;
      fCanUndo = Editmap_CanUndo( hedit, hwndEdit );
      fCanRedo = Editmap_CanRedo( hedit, hwndEdit );
      fFind = TRUE;
      fPaste = HasClipboardData();
      fSelectAll = TRUE;
      fIsEdit = TRUE; // !!SM!!2039
      }
   else
      {
      fCanUndo = FALSE;
      fCanRedo = FALSE;
      fHasSelection = FALSE;
      fFind = FALSE;
      fRdOnly = FALSE;
      fIsEdit = FALSE; // !!SM!!2039
      fPaste = FALSE;
      fSelectAll = ( hwndTopic != NULL ) || ( hwndMsg != NULL );
      }

   /* Now set the button state.
    */
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_SPELLCHECK, !fRdOnly && fUseDictionary );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_UNDO, fCanUndo );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_REDO, fCanRedo ); // 20060302 - 2.56.2049.08
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_CLEAR, fHasSelection && !fRdOnly );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_CUT, fHasSelection && !fRdOnly );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_COPY, fHasSelection );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_ROT13, fHasSelection );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_FIND, fFind );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_REPLACE, !fRdOnly );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_QUOTE, !fRdOnly && fIsEdit ); // !!SM!!2039
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_INSERTFILE, !fRdOnly );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_PASTE, fPaste && !fRdOnly );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_SELECTALL, fSelectAll );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_COPYALL, fSelectAll );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_FIXEDFONT, fSelectAll && ( hwndTopic != NULL ) );
    
   CheckMenuStates( ) ; // !!SM!! 2.55
}

/* This function disables/enables those toolbar buttons
 * depending on whether or not we're importing.
 */
void FASTCALL ToolbarState_Import( void )
{
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_IMPORT, !fParsing );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_STOPIMPORT, fParsing );
}

/* This function disables all those toolbar buttons which
 * are disabled throughout the session because of restricted
 * permissions.
 */
void FASTCALL ToolbarState_Permissions( void )
{
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_ADMIN, TestPerm(PF_ADMINISTRATE) );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_NEWFOLDER, TestPerm(PF_CANCREATEFOLDERS) );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_NEWMAILFOLDER, TestPerm(PF_CANCREATEFOLDERS) );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_USENET, TestPerm(PF_CANCONFIGURE) );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_PREFERENCES, TestPerm(PF_CANCONFIGURE) );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_DIRECTORIES, TestPerm(PF_CANCONFIGURE) );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_PURGESETTINGS, TestPerm(PF_CANCONFIGURE) );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_MAILSETTINGS, TestPerm(PF_CANCONFIGURE) );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_COLOURSETTINGS, TestPerm(PF_CANCONFIGURE) );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_CIXSETTINGS, TestPerm(PF_CANCONFIGURE) );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_ADDONS, TestPerm(PF_CANCONFIGURE) && !fNoAddons );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_CONFIGADDONS, TestPerm(PF_CANCONFIGURE) && !fNoAddons && CountInstalledAddons() );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_FILTERS, TestPerm(PF_CANCONFIGURE) );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_CUSTOMISE, TestPerm(PF_CANCONFIGURE) );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_CUSTTOOLBAR, TestPerm(PF_CANCONFIGURE) );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_SIGNATURES, TestPerm(PF_CANCONFIGURE) );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_SCHEDULER, TestPerm(PF_CANCONFIGURE) );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_COMMUNICATIONS, TestPerm(PF_CANCONFIGURE) );
}

/* This function enables or disables the toolbar buttons that
 * depend on the printer driver.
 */
void FASTCALL ToolbarState_Printer( void )
{
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_PRINT, IsPrinterDriver() );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_QUICKPRINT, IsPrinterDriver() );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_PRINTSETUP, IsPrinterDriver() );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_PAGESETUP, IsPrinterDriver() );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_PASTE, HasClipboardData() );
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL MainWnd_OnCreate( HWND hwnd, LPCREATESTRUCT lpCreateStruct )
{
   CLIENTCREATESTRUCT ccs;
   DWORD dwStyle;

   /* Create the MDI client window.
    */
   ccs.hWindowMenu = GetSubMenu( hMainMenu, GetMenuItemCount( hMainMenu ) - 2 );
   ccs.idFirstChild = IDM_WINDOWCHILD;
   dwStyle = WS_CHILD|WS_CLIPCHILDREN;
   if( fWorkspaceScrollbars )
      dwStyle |= WS_VSCROLL|WS_HSCROLL;
   hwndMDIClient = CreateWindow( "MDICLIENT",
                          "",
                          dwStyle,
                          0, 0,
                          0,
                          0,
                          hwnd,
                          (HMENU)0xCAC,
                          hInst,
                          (LPSTR)&ccs );
   if( hwndMDIClient == NULL )
      return( FALSE );

   /* Create the status bar.
    */
   hwndStatus = Amctl_CreateStatusbar( WS_CHILD, "", hwnd, IDD_STATUSWND );
   if( hwndStatus == NULL )
      return( FALSE );

   if( fFirstTimeThisUser )
      {
      fNewButtons = FALSE;
      Amuser_WritePPInt( szPreferences, "NewButtons", fNewButtons );
      }

   /* Load the toolbar.
    */
   if( !LoadToolbar( hwnd, FALSE ) )
      return( FALSE );

   /* Set the status bar font
    */
   SendMessage( hwndStatus, WM_SETFONT, (WPARAM)hStatusFont, 0L );

   /* Start the clock running.
    */
   if( 0 == SetTimer( hwnd, TID_CLOCK, 1000, NULL ) )
      Amevent_AddEventLogItem( ETYP_SEVERE|ETYP_INITTERM, GS(IDS_STR13) );

   /* Everything created okay.
    */
   return( TRUE );
}

/* Handle the WM_NOTIFY message.
 */
LRESULT FASTCALL MainWnd_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
{
   switch( lpNmHdr->code )
      {
      case TBN_CALLBACK: {
         LPTBCALLBACK lptbcb;

         lptbcb = (LPTBCALLBACK)lpNmHdr;
         switch( lptbcb->flag )
            {
            case TBCBF_WANTSTRING: {
               CMDREC cmd;

               /* If this is an external application, return the command name.
                */
               if( lptbcb->tb.idCommand >= IDM_EXTAPPFIRST && lptbcb->tb.idCommand <= IDM_EXTAPPLAST )
                  {
                  APPINFO appinfo;

                  GetExternalApp( lptbcb->tb.idCommand, &appinfo );
                  strcpy( lptbcb->szStr, appinfo.szCommandName );
                  lptbcb->pStr = lptbcb->szStr;
                  return( TRUE );
                  }
               /* Anything else will be in the command table.
                */
               cmd.iCommand = lptbcb->tb.idCommand;
               if( CTree_GetCommand( &cmd ) )
                  {
                  strcpy( lptbcb->szStr, GINTRSC(cmd.lpszTooltipString) );
                  lptbcb->pStr = lptbcb->szStr;
                  }
               return( TRUE );
               }

            case TBCBF_WANTBITMAP:
               /* If we have cached the button bitmaps for the
                * command, return the cached bitmaps.
                */
               lptbcb->hBmp = CTree_GetCommandBitmap( lptbcb->tb.idCommand );
               break;
            }
         return( 0L );
         }

      case TBN_NEEDRECT: {
         LPTBRECT lptbrc;
         RECT rc[ 3 ];

         lptbrc = (LPTBRECT)lpNmHdr;
         GetWorkspaceRects( hwnd, (LPRECT)&rc );
         lptbrc->rc = rc[ 2 ];
         return( 0L );
         }

      case NM_DBLCLK:
         if( lpNmHdr->idFrom == IDD_STATUSWND )
            {
            RECT rc;
            POINT pt;

            /* If double-click is on the Out Basket portion of
             * the status bar, open the out-basket window.
             */
            GetCursorPos( &pt );
            ASSERT( NULL != hwndStatus );
            SendMessage( hwndStatus, SB_GETPARTRECT, idmsbOutbasket, (LPARAM)(LPSTR)&rc );
            ScreenToClient( hwndStatus, &pt );
            if( PtInRect( &rc, pt ) )
               CreateOutBasket( hwnd );
            }
         return( 0L );

      case NM_RCLICK:
         if( lpNmHdr->idFrom == IDD_WINDOWTOOLBAR )
            {
            HMENU hPopupMenu;
            int iButton;
            POINT ptClnt;
            POINT pt;

            /* Right-click on the toolbar. First initialise the
             * different orientation commands.
             */
            GetCursorPos( &pt );
            hPopupMenu = GetPopupMenu( IPOP_TOOLMENU );
            CheckMenuGroup( hPopupMenu, IDM_TOOLBAR_TOP, IDM_TOOLBAR_BOTTOM, nToolbarPos );
            CheckMenuItem( hPopupMenu, IDM_TOOLBAR_TOP, ( nToolbarPos == IDM_TOOLBAR_TOP ) ? MF_CHECKED : MF_UNCHECKED );
            CheckMenuItem( hPopupMenu, IDM_TOOLBAR_LEFT, ( nToolbarPos == IDM_TOOLBAR_LEFT ) ? MF_CHECKED : MF_UNCHECKED );
            CheckMenuItem( hPopupMenu, IDM_TOOLBAR_RIGHT, ( nToolbarPos == IDM_TOOLBAR_RIGHT ) ? MF_CHECKED : MF_UNCHECKED );
            CheckMenuItem( hPopupMenu, IDM_TOOLBAR_BOTTOM, ( nToolbarPos == IDM_TOOLBAR_BOTTOM ) ? MF_CHECKED : MF_UNCHECKED );

            /* Next, find the button at the cursor position
             * and get the command ID.
             */
            ptClnt = pt;
            ScreenToClient( hwndToolBar, &ptClnt );
            iButton = (int)SendMessage( hwndToolBar, TB_HITTEST, 0, MAKELPARAM( ptClnt.x, ptClnt.y ) );
            if( iButton >= 0 )
               {
               TBBUTTON tb;

               /* It's a button (may be a separator if tb.idCommand == 0), so
                * check the command ID and pad out the menu as appropriate.
                */
               SendMessage( hwndToolBar, TB_GETBUTTON, iButton, (LPARAM)(LPSTR)&tb );
               if( tb.idCommand >= IDM_EXTAPPFIRST && tb.idCommand <= IDM_EXTAPPLAST )
                  AppendMenu( hPopupMenu, MF_BYCOMMAND|MF_STRING, IDM_EDITEXTAPP, "Properties" );
               else if( tb.idCommand >= IDM_TERMINALFIRST && tb.idCommand <= IDM_TERMINALLAST )
                  AppendMenu( hPopupMenu, MF_BYCOMMAND|MF_STRING, IDM_EDITTERMINAL, "Properties" );
               else if( tb.idCommand >= IDM_BLINKMANFIRST && tb.idCommand <= IDM_BLINKMANLAST || tb.idCommand == IDM_SENDRECEIVEALL )
                  AppendMenu( hPopupMenu, MF_BYCOMMAND|MF_STRING, IDM_EDITBLINK, "Properties" );
               if( tb.idCommand >= IDM_DYNAMICCOMMANDS )
                  AppendMenu( hPopupMenu, MF_BYCOMMAND|MF_STRING, IDM_BMPCHGBLINK, "Button" );

               /* Cache the command ID so we know which button is
                * to be affected.
                */
               iCachedButtonID = tb.idCommand;
               }
            TrackPopupMenu( hPopupMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL );
            }
         break;

      case TTN_POP:
         /* This message is sent by the tooltip control when the
          * tooltip is removed.
          */
         SendMessage( hwndStatus, SB_SIMPLE, FALSE, 0L );
         break;

      case TTN_SHOW:
         /* This message is sent by the tooltip control when the
          * tooltip is displayed.
          */
         if( !fParsing )
            DisplayCommandStatus( (UINT)idCode );
         return( 0L );

      case TBN_BUTTONSTATECHANGED:
         if( !fParsing )
            {
            TBNOTIFY FAR * lptbn;
   
            /* This notification is sent when the state of a button is changed
             * through the mouse. In our case, we detect when a button is pressed
             * and show the description on the status bar, and also when the same
             * button is released and remove the description.
             */
            lptbn = (TBNOTIFY FAR *)lpNmHdr;
            if( SendMessage( hwndToolBar, TB_ISBUTTONPRESSED, (UINT)lptbn->iItem, 0L ) )
               DisplayCommandStatus( (UINT)lptbn->iItem );
            else
               SendMessage( hwndStatus, SB_SIMPLE, FALSE, 0L );
            }
         break;

      case TTN_NEEDTEXT: {
         TOOLTIPTEXT FAR * lpttt;
         CMDREC cmd;

         lpttt = (TOOLTIPTEXT FAR *)lpNmHdr;
         if( fUseTooltips )
            {
            /* First deal with external applications which won't be
             * in the command table.
             */
            if( lpttt->hdr.idFrom >= IDM_EXTAPPFIRST && lpttt->hdr.idFrom <= IDM_EXTAPPLAST )
               {
               APPINFO appinfo;

               /* Get the external application command name.
                */
               GetExternalApp( lpttt->hdr.idFrom, &appinfo );
               strcpy( lpttt->szText, appinfo.szCommandName );
               lpttt->lpszText = lpttt->szText;
               lpttt->hinst = NULL;
               SendMessage( hwndStatus, SB_SETTEXT, 255|SBT_NOBORDERS, (LPARAM)(LPSTR)lpttt->szText );
               return( TRUE );
               }
            else
               {

               /* Anything else will be in the command table.
                */
               cmd.iCommand = lpttt->hdr.idFrom;
               if( CTree_GetCommand( &cmd ) )
                  {
                  strcpy( lpttt->szText, GINTRSC(cmd.lpszTooltipString) );
                  if( cmd.wKey )
                     {
                     int len;
            
                     strcat( lpttt->szText, " (" );
                     len = ( 80 - lstrlen( lpttt->szText ) ) - 3;
                     DescribeKey( cmd.wKey, lpttt->szText + lstrlen( lpttt->szText ), len );
                     strcat( lpttt->szText, ")" );
                     }
                  lpttt->lpszText = lpttt->szText;
                  lpttt->hinst = NULL;
                  DisplayCommandStatus( cmd.iCommand );
                  return( TRUE );
                  }
               }
            return( FALSE );
            }
         DisplayCommandStatus( lpttt->hdr.idFrom );
         return( FALSE );
         }
               
      case TBN_TOOLBARCHANGE:
      case TBN_QUERYINSERT:
      case TBN_QUERYDELETE:
         if( hwndToolBarOptions != NULL )
            SendMessage( hwndToolBarOptions, WM_NOTIFY, (WPARAM)idCode, (LPARAM)lpNmHdr );
         return( TRUE );
      }
   return( 0L );
}

/* This function handles the WM_ACTIVATE message.
 */
void FASTCALL MainWnd_OnActivate( HWND hwnd, UINT state, HWND hwndActDeact, BOOL fMinimized )
{
   /* !!SM!!
      Remember the last focused window
    */
/* if(state == WA_INACTIVE)
   {
      fLastActiveWindow = GetFocus();
   }
   else
   {
      if (IsWindow(fLastActiveWindow))
         SetFocus(fLastActiveWindow);
   }*/
}

/* This function displays the description of the specified command
 * button.
 */
void FASTCALL DisplayCommandStatus( UINT wID )
{
   CMDREC cmd;

   cmd.iCommand = wID;
   if( CTree_GetCommand( &cmd ) )
      {
      /* If the button is disabled, show the disabled string.
       */
      SendMessage( hwndStatus, SB_SIMPLE, TRUE, 0L );
      if( !SendMessage( hwndToolBar, TB_ISBUTTONENABLED, wID, 0L ) && cmd.iDisabledString )
         {
         strcpy( lpTmpBuf, GS(IDS_X_PREFIX) );
         if( wID == IDM_PRINT && !IsPrinterDriver() )
            strcat( lpTmpBuf, GS(IDS_X_PAGESETUP) );
         else
            strcat( lpTmpBuf, GS(cmd.iDisabledString) );
         SendMessage( hwndStatus, SB_SETTEXT, 255|SBT_NOBORDERS, (LPARAM)(LPSTR)lpTmpBuf );
         }
      else {
         LPCSTR lpsz;
      
         lpsz = cmd.lpszString;
         if( HIWORD( lpsz ) == 0 )
            lpsz = GS( (UINT)LOWORD( cmd.lpszString ) );
         SendMessage( hwndStatus, SB_SETTEXT, 255|SBT_NOBORDERS, (LPARAM)(LPSTR)lpsz );
         }
      }
}

/* This function extracts a popup menu from the list of
 * popups.
 */
HMENU FASTCALL GetPopupMenu( int iPopup )
{
   if( hPopupMenus )
      DestroyMenu( hPopupMenus );
   hPopupMenus = LoadMenu( hRscLib, MAKEINTRESOURCE( IDMNU_POPUPS ) );
   return( GetSubMenu( hPopupMenus, iPopup ) );
}

/* This function handles the WM_CLOSE message. It checks whether
 * the out-basket is empty and warns the user if not. Then it
 * initiates a sequence of close-downs, any of which could abort
 * the entire operation.
 */
void FASTCALL MainWnd_OnClose( HWND hwnd )
{
   if( RCS_CONNECTING == nRasConnect )
      {
      nRasConnect = RCS_ABORTED_DUE_TO_APP_QUIT;
      return;
      }
   MainWnd_CommonCloseCode( hwnd, TRUE );
}

/* This function handles the WM_QUERYENDSESSION message. It returns
 * FALSE if Palantir cannot quit for any reason.
 */
BOOL FASTCALL MainWnd_OnQueryEndSession( HWND hwnd )
{
   /* If the outbasket is not empty and we've got the warning option set
    * to warn before exiting.
    */
   if( fWarnOnExit && Amob_GetCount(OBF_UNFLAGGED) )
      {
      EnsureMainWindowOpen();
      if( fMessageBox( hwnd, 0, GS(IDS_STR194), MB_YESNO|MB_ICONINFORMATION ) == IDNO )
         return( FALSE );
      }

   /* If the server is busy, query whether to close Palantir.
    */
   if( IsServerBusy() || AreTerminalsActive() )
      {
      EnsureMainWindowOpen();
      if( fMessageBox( hwnd, 0, GS(IDS_STR680), MB_YESNO|MB_ICONEXCLAMATION ) == IDNO )
         return( FALSE );
      }

   /* For anything else, its okay to exit.
    */
   return( TRUE );
}

/* This function handles the WM_CLOSE message. It checks whether
 * the out-basket is empty and warns the user if not. Then it
 * initiates a sequence of close-downs, any of which could abort
 * the entire operation.
 */
void FASTCALL MainWnd_OnEndSession( HWND hwnd, BOOL fEnding )
{
   if( fEnding )
      MainWnd_CommonCloseCode( hwnd, FALSE );
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL MainWnd_OnMove( HWND hwnd, int x, int y )
{
   if( !IsIconic( hwnd ) )
      Amuser_WriteWindowState( "Main", hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, DefFrameWindowProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL MainWnd_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   if( fParsing )
      UpdateParsingStatusBar();
   Adm_DeferMDIResizing( hwnd, TRUE );
   SetMainWindowSizes( hwnd, cx, cy );
   Adm_DeferMDIResizing( hwnd, FALSE );
   //if ( state != SIZE_MINIMIZED ) //!!SM!! 2.55
   // FlashWindow( hwnd, 0 );   //!!SM!! 2.55

}

/* This function handles the WM_FONTCHANGE message. We recreate the
 * fonts used, then send a WM_CHANGEFONT to all windows.
 */
void FASTCALL MainWnd_OnFontChange( HWND hwnd )
{
   CreateAmeolFonts( FALSE );
   ComputeStatusBarWidth();
   UpdateStatusBarWidths();
   SendMessage( hwndStatus, WM_SETFONT, (WPARAM)hStatusFont, 1L );
   SendAllWindows( WM_CHANGEFONT, 0xFFFF, 0L ); 
}

/* This function handles the WM_SYSCOLORCHANGE message.
 */
void FASTCALL MainWnd_OnSysColorChange( HWND hwnd )
{
   Adm_SysColorChange();
}

/* This function handles the WM_SETTINGCHANGE message.
 * (Win32 only)
 */
void FASTCALL MainWnd_OnSettingChange( HWND hwnd, int wFlag, LPCSTR lpszSectionName )
{
   if( SPI_SETWHEELSCROLLLINES == wFlag )
      {
      UINT iScrollLines;
      if( NULL == hwndWheel )
         SystemParametersInfo( SPI_GETWHEELSCROLLLINES, 0, &iScrollLines, 0 );
      else
         iScrollLines = (int)SendMessage( hwndWheel, msgScrollLines, 0, 0 );
      Amctl_SetControlWheelScrollLines( iScrollLines );
      }

   /* Handle change to the date/time format.
    */
   if( lpszSectionName == NULL || lstrcmpi( lpszSectionName, "intl" ) == 0 )
      {
      Amdate_RefreshDateIniSettings( lpszSectionName );
      Amdate_RefreshTimeIniSettings( lpszSectionName );
      if( hwndTopic )
         SendAllWindows( WM_VIEWCHANGE, 0, 0L );
      UpdateStatusBarWidths();
      InvalidateRect( hwndStatus, NULL, TRUE );
      UpdateWindow( hwndStatus );
      }
   else
      {
      UpdatePrinterSelection( FALSE );
      ToolbarState_Printer();
      }
}

/* This function handles the WM_DEVMODECHANGE message.
 */
void FASTCALL MainWnd_OnDevModeChange( HWND hwnd, LPCSTR lpszSectionName )
{
   UpdatePrinterSelection( FALSE );
   ToolbarState_Printer();
}

/* This function handles the WM_MENUSELECT message.
 */
void FASTCALL MainWnd_OnMenuSelect( HWND hwnd, HMENU hmenu, int item, HMENU hmenuPopup, UINT flags )
{
   LPSTR lpszDesc;
   BOOL fClosed;
   WORD wStrID;
   char sz[ 144 ];

   lpszDesc = NULL;
   fClosed = ( flags == (UINT)-1 && hmenu == NULL );
   sz[ 0 ] = '\0';
   if( !fClosed )
      {
      register int c;

      if( flags & MF_POPUP )
         {
         if( hmenuPopup == GetSystemMenu( hwnd, FALSE ) )
            {
            wStrID = IDS_SYSMENU;
            VERIFY( LoadString( hRscLib, wStrID, sz, 144 ) );
            }
         else if( hwndActive && hmenuPopup == GetSystemMenu( hwndActive, FALSE ) )
            {
            wStrID = IDS_SYSMENU;
            VERIFY( LoadString( hRscLib, wStrID, sz, 144 ) );
            }
         else if( hmenuPopup == hCIXMenu )
            {
            /* Hack!
             */
            wStrID = IDS_CIXMENU;
            VERIFY( LoadString( hRscLib, wStrID, sz, 144 ) );
            }
         else
            {
            for( c = 0; wMainPopupIDs[ c ].hpopupMenu; ++c )
               if( hmenuPopup == wMainPopupIDs[ c ].hpopupMenu )
                  {
                  wStrID = wMainPopupIDs[ c ].wID;
                  VERIFY( LoadString( hRscLib, wStrID, sz, 144 ) );
                  break;
                  }
            }
         }
      else if( (UINT)item >= 0xF000 )
         {
         wStrID = 0;
         switch( (UINT)item )
            {
            case IDM_SPLIT:
               wStrID = IDS_SPLIT;
               break;

            case SC_NEXTWINDOW:
               wStrID = IDS_SC_NEXT;
               break;

            case SC_CLOSE:
               wStrID = (WORD)( ( hwnd == hwndFrame ) ? IDS_EXIT : IDS_SC_CLOSE );
               break;

            case SC_RESTORE:
               wStrID = IDS_SC_RESTORE;
               break;

            case SC_SIZE:
               wStrID = IDS_SC_SIZE;
               break;

            case SC_MOVE:
               wStrID = IDS_SC_MOVE;
               break;

            case SC_MAXIMIZE:
               wStrID = IDS_SC_MAXIMIZE;
               break;

            case SC_MINIMIZE:
               wStrID = IDS_SC_MINIMIZE;
               break;
            }
         if( wStrID )
            VERIFY( LoadString( hRscLib, wStrID, sz, 144 ) );
         }
      else
         {
         CMDREC cmd;

         cmd.iCommand = item;
         if( CTree_GetCommand( &cmd ) )
            {
            LPCSTR lpsz;

            /* If command is disabled, then the third field of the array contains the
             * ID of the message appropriate to the disabled command only if it is nonzero.
             */
            lpsz = cmd.lpszString;
            if( ( flags & MF_GRAYED ) && cmd.iDisabledString )
               {
               register int d = 0;
   
               wStrID = (WORD)cmd.iDisabledString;
               VERIFY( d = LoadString( hRscLib, IDS_X_PREFIX, sz, 144 ) );
               if( item == IDM_PRINT && !IsPrinterDriver() )
                  wStrID = IDS_X_PAGESETUP;
               VERIFY( LoadString( hRscLib, wStrID, sz + d, 144 ) );
               }
            else {
               if( HIWORD( lpsz ) == 0 )
                  LoadString( hRscLib, LOWORD(lpsz), sz, 144 );
               else
                  strcpy( sz, lpsz );
               }
            }
         }
      if( !fParsing )
         {
         SendMessage( hwndStatus, SB_SIMPLE, TRUE, 0L );
         SendMessage( hwndStatus, SB_SETTEXT, 255 | SBT_NOBORDERS, (LPARAM)(LPSTR)sz );
         }
      }
   else if( !fParsing )
      SendMessage( hwndStatus, SB_SIMPLE, FALSE, 0L );
}

/* This function handles the WM_DESTROY message.
 */
void FASTCALL MainWnd_OnDestroy( HWND hwnd )
{
   HMENU hMenu;

   hMenu = GetMenu( hwnd );
   if( hMainMenu != hMenu )
      DestroyMenu( hMainMenu );
   DestroyMenu( hPopupMenus );
   PostQuitMessage( 0 );
}

/* This function handles the WM_QUERYOPEN message.
 */
BOOL FASTCALL MainWnd_OnQueryOpen( HWND hwnd )
{
   static BOOL fQueryDlgOpen = FALSE;
   BOOL fOkToOpen;

   fOkToOpen = TRUE;
   if( fQueryDlgOpen )
      {
      BringWindowToTop( hwnd );
      return( FALSE );
      }
   if( fSecureOpen )
      {
      fQueryDlgOpen = TRUE;
      fOkToOpen = DialogBox( hRscLib, MAKEINTRESOURCE(IDDLG_OPENPASSWORD), hwnd, OpenPassword );
      fQueryDlgOpen = FALSE;
      }
   return( fOkToOpen );
}

/* This function handles the WM_TIMER message.
 */
void FASTCALL MainWnd_OnTimer( HWND hwnd, UINT id )
{
   switch( id )
      {
      case TID_MAILALERTTO: {
         char sz[ sizeof( szAppName ) ];
         if( !IsIconic( hwndFrame ) )
         {
            GetWindowText( GetParent( GetFocus() ), sz, sizeof( sz ) );
            if( strcmp( sz, szAppName ) == 0 )
               SendCommand( GetParent( GetFocus() ), IDOK, 0L );
         }
         fAlertTimedOut = TRUE;
         KillTimer( hwnd, TID_MAILALERT );
         KillTimer( hwnd, TID_MAILALERTTO );
         FlashWindow( hwndFrame, 0 );
         }
         break;

      case TID_MAILALERT:
         FlashWindow( hwnd, 1 );
         break;

/*    case TID_OLT:
         if( !fNoOLT && fBlinking && !fCIXSyncInProgress && ( iAddonsUsingConnection == 0 ) )
         {
            WriteToBlinkLog( "Internet timeout - disconnecting" );
            CmdOffline();
         }
         else if( fBlinking )
         {
            KillTimer( hwndFrame, TID_OLT );
            SetTimer( hwndFrame, TID_OLT, (UINT)60000, NULL );
         }
         else
            KillTimer( hwndFrame, TID_OLT );
         break;
*/
      case TID_SCHEDULER:
         InvokeScheduler();
         break;

      case TID_BLINKANIMATION:
         StepBlinkAnimation();
         break;

      case TID_DISCONNECT:
         HandleDisconnectTimeout();
         break;

      case TID_FLASHICON:
//       ASSERT( IsIconic( hwnd ) );
         if( IsIconic( hwnd ) ) // !!SM!! 2.54.2031
         {
            FlashWindow( hwnd, 1 );
         }
         break;

      case TID_SWEEPER: {
         if( fDisplayLockCount && fDebugMode )
            fMessageBox( hwnd, 0, "Starting sweep", MB_OK|MB_ICONEXCLAMATION );
         Amdb_SweepAndDiscard();
         if( fDisplayLockCount && fDebugMode )
            fMessageBox( hwnd, 0, "Ended sweep", MB_OK|MB_ICONEXCLAMATION );
         }
         break;
               
      case TID_CLOCK:
         ASSERT( IsWindow( hwndStatus ) );
         if( fShowClock && IsWindowVisible( hwndStatus ) )
            {
            LPSTR lpsz;

            lpsz = FormatTimeString( NULL );
            SendMessage( hwndStatus, SB_SETTEXT, idmsbClock, (LPARAM)lpsz );
            }
         break;
      }
}

/* This function processes the WM_INITMENU message.
 */
void FASTCALL MainWnd_OnInitMenu( HWND hwnd, HMENU hMenu )
{
   char * pszResignRejoin;
   BOOL fIsPrinterDriver;
   char * pszTopicType;
   BOOL fHasDatabase;
   BOOL fServerBusy;
   BOOL fIsAuthor;
   HWND hwndEdit;
   char sz[ 60 ];
   WORD wStatus;
   HEDIT hedit;
   int nSort;

   /* Cache some useful settings.
    */
   fServerBusy = IsServerBusy();
   fHasDatabase = NULL != pcatCIXDatabase;
   fIsPrinterDriver = IsPrinterDriver();
   fIsAuthor = FALSE;

   /* First do the global stuff.
    */
   MenuEnable( hMenu, IDM_EXPORT, fHasDatabase );
   MenuEnable( hMenu, IDM_SEARCH, fHasDatabase );
   MenuEnable( hMenu, IDM_FILELISTFIND, fHasDatabase );
   MenuEnable( hMenu, IDM_IMPORT, !fParsing );
   MenuEnable( hMenu, IDM_STOPIMPORT, fParsing );
   MenuEnable( hMenu, IDM_NEWFOLDER, TestPerm(PF_CANCREATEFOLDERS) && fHasDatabase );
   MenuEnable( hMenu, IDM_NEWCATEGORY, TestPerm(PF_CANCREATEFOLDERS) && fHasDatabase );
   MenuEnable( hMenu, IDM_NEXT, Amdb_GetTotalUnread() > 0 );
   MenuEnable( hMenu, IDM_NEXTPRIORITY, Amdb_GetTotalUnreadPriority() > 0 );
   MenuEnable( hMenu, IDM_NEWLOCALTOPIC, TestPerm(PF_CANCREATEFOLDERS) && fHasDatabase );
   MenuEnable( hMenu, IDM_NEWMAILFOLDER, TestPerm(PF_CANCREATEFOLDERS) && fHasDatabase );
   MenuEnable( hMenu, IDM_PRINT, fIsPrinterDriver && fHasDatabase );
   MenuEnable( hMenu, IDM_PRINTSETUP, fIsPrinterDriver );
   MenuEnable( hMenu, IDM_ADMIN, TestPerm(PF_ADMINISTRATE) );
   MenuEnable( hMenu, IDM_PAGESETUP, fIsPrinterDriver );
   MenuEnable( hMenu, IDM_STOP, fOnline || fBlinking );
   MenuEnable( hMenu, IDM_ONLINE, !fOnline && !fBlinking );
   MenuEnable( hMenu, IDM_USENET, TestPerm(PF_CANCONFIGURE) );
   MenuEnable( hMenu, IDM_PREFERENCES, TestPerm(PF_CANCONFIGURE) );
   MenuEnable( hMenu, IDM_DIRECTORIES, TestPerm(PF_CANCONFIGURE) );
   MenuEnable( hMenu, IDM_PURGESETTINGS, TestPerm(PF_CANCONFIGURE) );
   MenuEnable( hMenu, IDM_MAILSETTINGS, TestPerm(PF_CANCONFIGURE) );
   MenuEnable( hMenu, IDM_COLOURSETTINGS, TestPerm(PF_CANCONFIGURE) );
   MenuEnable( hMenu, IDM_ADDONS, TestPerm(PF_CANCONFIGURE) && !fNoAddons );
   MenuEnable( hMenu, IDM_CONFIGADDONS, TestPerm(PF_CANCONFIGURE) && !fNoAddons && CountInstalledAddons() );
   MenuEnable( hMenu, IDM_FILTERS, TestPerm(PF_CANCONFIGURE) && fHasDatabase );
   MenuEnable( hMenu, IDM_CUSTOMISE, TestPerm(PF_CANCONFIGURE) );
   MenuEnable( hMenu, IDM_CUSTTOOLBAR, TestPerm(PF_CANCONFIGURE) );
   MenuEnable( hMenu, IDM_SIGNATURES, TestPerm(PF_CANCONFIGURE) );
   MenuEnable( hMenu, IDM_SCHEDULER, TestPerm(PF_CANCONFIGURE) );
   MenuEnable( hMenu, IDM_COMMUNICATIONS, TestPerm(PF_CANCONFIGURE) );
   CheckMenuItem( hMenu, IDM_TOOLBAR, fShowToolBar ? MF_CHECKED : MF_UNCHECKED );
   CheckMenuItem( hMenu, IDM_STATUSBAR, fShowStatusBar ? MF_CHECKED : MF_UNCHECKED );
   CheckMenuItem( hMenu, IDM_HEADERS, fStripHeaders ? MF_UNCHECKED : MF_CHECKED );
   CheckMenuItem( hMenu, IDM_MSGSTYLES, fMsgStyles ? MF_CHECKED : MF_UNCHECKED );
   CheckMenuItem( hMenu, IDM_WORDWRAP, fWordWrap ? MF_CHECKED : MF_UNCHECKED );
   CheckMenuItem( hMenu, IDM_HOTLINKS, fHotLinks ? MF_CHECKED : MF_UNCHECKED );
   CheckMenuItem( hMenu, IDM_MLHOTLINKS, fMultiLineURLs ? MF_CHECKED : MF_UNCHECKED );
   

   /* Disable all blink commands
    */
   EnableBlinkMenu( hMenu, !fBlinking && !fServerBusy );

   /* Defaults, for when no appropriate folder is
    * visible.
    */
   pszResignRejoin = "Resign";
   pszTopicType = "";

   /* Set status to TRUE if the current control has a
    * selection.
    */
   hwndEdit = GetFocus();
   hedit = Editmap_Open( hwndEdit );
   if( ETYP_NOTEDIT != hedit )
      {
      BEC_SELECTION bsel;
      BOOL fRdOnly;
      int status;


      Editmap_GetSelection( hedit, hwndEdit, &bsel );
      status = bsel.lo != bsel.hi;
      fRdOnly = ( GetWindowStyle( hwndEdit ) & ES_READONLY ) == ES_READONLY;

      if (bsel.lo != bsel.hi)
      {
         HPSTR lpText;
         HPSTR lpText2;
//       int cbMatch;
         
         INITIALISE_PTR(lpText);

         if( fNewMemory32( &lpText, (bsel.hi - bsel.lo) + 3 ) )
         {
            Editmap_GetSel( hedit, hwndEdit, (bsel.hi - bsel.lo) + 3, lpText );
            lpText2 = lpText;
            while( isspace( *lpText2 ) )
               ++lpText2;
//          if( Amctl_IsHotlinkPrefix( lpText2, &cbMatch ) )
               MenuEnable( hMenu, IDM_BROWSE, TRUE );
            FreeMemory32( &lpText );
         }
      }

      MenuEnable( hMenu, IDM_COPY, status );
      MenuEnable( hMenu, IDM_CUT, status && !fRdOnly );
      MenuEnable( hMenu, IDM_CLEAR, status && !fRdOnly );
      MenuEnable( hMenu, IDM_UNDO, Editmap_CanUndo( hedit, hwndEdit ) );
      MenuEnable( hMenu, IDM_REDO, Editmap_CanRedo( hedit, hwndEdit ) ); // 20060302 - 2.56.2049.08
      MenuEnable( hMenu, IDM_FIND, !fRdOnly );
      MenuEnable( hMenu, IDM_FIXEDFONT, TRUE );
      MenuEnable( hMenu, IDM_REPLACE, !fRdOnly );
      MenuEnable( hMenu, IDM_ROT13, status );
      MenuEnable( hMenu, IDM_SPELLCHECK, !fRdOnly && fUseDictionary );
      MenuEnable( hMenu, IDM_QUOTE, !fRdOnly );
      MenuEnable( hMenu, IDM_INSERTFILE, !fRdOnly );
      MenuEnable( hMenu, IDM_PASTE, HasClipboardData() );
      MenuEnable( hMenu, IDM_SELECTALL, TRUE );
      MenuEnable( hMenu, IDM_COPYALL, TRUE );
      }
   else
      {
      MenuEnable( hMenu, IDM_SPELLCHECK, FALSE );
      MenuEnable( hMenu, IDM_COPY, FALSE );
      MenuEnable( hMenu, IDM_CUT, FALSE );
      MenuEnable( hMenu, IDM_CLEAR, FALSE );
      MenuEnable( hMenu, IDM_ROT13, FALSE );
      MenuEnable( hMenu, IDM_UNDO, FALSE );
      MenuEnable( hMenu, IDM_REDO, FALSE ); // 20060302 - 2.56.2049.08
      MenuEnable( hMenu, IDM_FIND, FALSE );
      MenuEnable( hMenu, IDM_FIXEDFONT, hwndTopic != NULL );
      MenuEnable( hMenu, IDM_REPLACE, FALSE );
      MenuEnable( hMenu, IDM_QUOTE, FALSE );
      MenuEnable( hMenu, IDM_INSERTFILE, FALSE );
      MenuEnable( hMenu, IDM_INSERT, FALSE );
      MenuEnable( hMenu, IDM_PASTE, FALSE );
      MenuEnable( hMenu, IDM_SELECTALL, hwndTopic != NULL );
      MenuEnable( hMenu, IDM_COPYALL, hwndTopic != NULL );
      MenuEnable( hMenu, IDM_BROWSE, FALSE );
      }


   /* Collect some statistics about the state of the message
    * window to save time.
    */
   wStatus = MEB_READWRITE;
   if( iActiveMode & (WIN_MESSAGE|WIN_THREAD) )
      {
      LPWINDINFO lpWindInfo;

      lpWindInfo = GetBlock( hwndTopic );
      if( lpWindInfo )
         {
         TOPICINFO topicinfo;
         PCL pcl;

         wStatus |= MEB_MSGWND|MEB_TOPIC;
         switch( Amdb_GetTopicType( lpWindInfo->lpTopic ) )
            {
            case FTYPE_CIX: wStatus |= MEB_CIXTOPIC; pszTopicType = "Topic"; break;
            case FTYPE_NEWS: wStatus |= MEB_NEWSTOPIC; break;
            case FTYPE_MAIL: wStatus |= MEB_MAILTOPIC; break;
            }
         if( lpWindInfo->lpMessage )
            {
            MSGINFO msginfo;

            Amdb_GetMsgInfo( lpWindInfo->lpMessage, &msginfo );
            if( IsLoginAuthor( msginfo.szAuthor ) )
               fIsAuthor = TRUE;
            wStatus |= MEB_HASMSG;
            if( !Amdb_IsHeaderMsg( lpWindInfo->lpMessage ) )
               wStatus |= MEB_HASFULLMSG;
            ChangeMenuString( hMenu, IDM_READMSG, Amdb_IsRead( lpWindInfo->lpMessage ) ? GS(IDS_STR316) : GS(IDS_STR317) );
            ChangeMenuString( hMenu, IDM_KEEPMSG, Amdb_IsKept( lpWindInfo->lpMessage ) ? GS(IDS_STR318) : GS(IDS_STR319) );
            ChangeMenuString( hMenu, IDM_READLOCK, Amdb_IsReadLock( lpWindInfo->lpMessage ) ? GS(IDS_STR320) : GS(IDS_STR321) );
            ChangeMenuString( hMenu, IDM_MARKMSG, Amdb_IsMarked( lpWindInfo->lpMessage ) ? GS(IDS_STR322) : GS(IDS_STR323) );
            ChangeMenuString( hMenu, IDM_DELETEMSG, Amdb_IsDeleted( lpWindInfo->lpMessage ) ? GS(IDS_STR324) : GS(IDS_STR325) );
            ChangeMenuString( hMenu, IDM_IGNORE, Amdb_IsIgnored( lpWindInfo->lpMessage ) ? GS(IDS_STR326) : GS(IDS_STR327) );
            ChangeMenuString( hMenu, IDM_PRIORITY, Amdb_IsPriority( lpWindInfo->lpMessage ) ? GS(IDS_STR446) : GS(IDS_STR445) );
            ChangeMenuString( hMenu, IDM_MARKTAGGED, Amdb_IsTagged( lpWindInfo->lpMessage ) ? GS(IDS_STR830) : GS(IDS_STR829) );
            if( Amdb_IsMsgHasAttachments( lpWindInfo->lpMessage ) )
               wStatus |= MEB_ATTACHMENT;
            }

         /* Is topic read-only?
          */
         pcl = Amdb_FolderFromTopic( lpWindInfo->lpTopic );
         Amdb_GetTopicInfo( lpWindInfo->lpTopic, &topicinfo );
         if( ( topicinfo.dwFlags & TF_READONLY ) && !Amdb_IsModerator( pcl, szCIXNickname ) )
            wStatus &= ~MEB_READWRITE;
         if( topicinfo.dwFlags & TF_RESIGNED )
            wStatus |= MEB_RESIGNED;
         CheckMenuItem( hMenu, IDM_FIXEDFONT, lpWindInfo->fFixedFont ? MF_CHECKED : MF_UNCHECKED );
         }
      }
   else if( iActiveMode == WIN_INBASK )
      {
      HWND hwndTreeCtl;
      HTREEITEM hSelItem;

      wStatus |= MEB_INBASKET;
      hwndTreeCtl = GetDlgItem( hwndActive, IDD_FOLDERLIST );
      hSelItem = TreeView_GetSelection( hwndTreeCtl );
      if( hSelItem )
         {
         TV_ITEM tv;

         /* Get the lParam of the selected item.
          */
         tv.hItem = hSelItem;
         tv.mask = TVIF_PARAM;
         VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
         if( Amdb_IsTopicPtr( (PTL)tv.lParam ) )
            {
            TOPICINFO topicinfo;
            PCL pcl;

            wStatus |= MEB_TOPIC;
            switch( Amdb_GetTopicType( (PTL)tv.lParam ) )
               {
               case FTYPE_NEWS: wStatus |= MEB_NEWSTOPIC; break;
               case FTYPE_CIX: wStatus |= MEB_CIXTOPIC; pszTopicType = "Topic"; break;
               case FTYPE_MAIL: wStatus |= MEB_MAILTOPIC; break;
               }

            /* Is topic read-only?
             */
            pcl = Amdb_FolderFromTopic( (PTL)tv.lParam );
            Amdb_GetTopicInfo( (PTL)tv.lParam, &topicinfo );
            if( ( topicinfo.dwFlags & TF_READONLY ) && !Amdb_IsModerator( pcl, szCIXNickname ) )
               wStatus &= ~MEB_READWRITE;
            if( topicinfo.dwFlags & TF_RESIGNED )
               {
               wStatus |= MEB_RESIGNED;
               pszResignRejoin = "Rejoin";
               }
            }
         else if( Amdb_IsFolderPtr( (PCL)tv.lParam ) )
            {
            BOOL fKeepAtTop;

            wStatus |= MEB_CONF;
            if( IsCIXFolder( (PCL)tv.lParam ) )
               {
               wStatus |= MEB_CIXCONF;
               if( Amdb_IsResignedFolder( (PCL)tv.lParam ) )
                  {
                  wStatus |= MEB_RESIGNED;
                  pszResignRejoin = "Rejoin";
                  }
               pszTopicType = "Forum";
               }
            fKeepAtTop = Amdb_GetFolderFlags( (PCL)tv.lParam, CF_KEEPATTOP ) == CF_KEEPATTOP;
            CheckMenuItem( hMenu, IDM_KEEPATTOP, fKeepAtTop ? MF_CHECKED : MF_UNCHECKED );
            }
         }
      }

   /* Change the Resign command.
    */
   if( hMenu == hMainMenu )
   {
      wsprintf( sz, "&%s %s", pszResignRejoin, pszTopicType );
      ChangeMenuString( hMenu, IDM_RESIGN, sz );
   }

   /* Show word wrap status.
    */
   if( NULL != hwndTopic )
      {
      VERIFY( hwndEdit = GetDlgItem( hwndTopic, IDD_MESSAGE ) );
      //CheckMenuItem( hMenu, IDM_WORDWRAP, SendMessage( hwndEdit, BEM_GETWORDWRAP, 0, 0L ) ? MF_CHECKED : MF_UNCHECKED );
      CheckMenuItem( hMenu, IDM_WORDWRAP, fWordWrap ? MF_CHECKED : MF_UNCHECKED ); // !!SM!! 2.55.2037      
      }

   /* Enable or disable menu commands as appropriate.
    */
   MenuEnable( hMenu, IDM_NEWWINDOW, fHasDatabase );
   MenuEnable( hMenu, IDM_DOWNLOAD, wStatus & MEB_CIXTOPIC );
   MenuEnable( hMenu, IDM_UPLOADFILE, wStatus & MEB_CIXTOPIC );
   MenuEnable( hMenu, IDM_REPOST, ( wStatus & MEB_CIXTOPIC ) && ( wStatus & MEB_HASMSG ) );
   MenuEnable( hMenu, IDM_FILELIST, wStatus & MEB_CIXTOPIC );
   MenuEnable( hMenu, IDM_COPYCIXLINK, wStatus & MEB_HASMSG );
   MenuEnable( hMenu, IDM_RESIGN, (wStatus & (MEB_CIXTOPIC|MEB_CIXCONF)) /* && !(wStatus & MEB_RESIGNED) */ );
   MenuEnable( hMenu, IDM_UNSUBSCRIBE, (wStatus & MEB_NEWSTOPIC) && !(wStatus & MEB_RESIGNED) );
   MenuEnable( hMenu, IDM_RETHREADARTICLES, wStatus & (MEB_MAILTOPIC|MEB_NEWSTOPIC) );
   MenuEnable( hMenu, IDM_RENAME, (wStatus & MEB_INBASKET) && fHasDatabase );
   MenuEnable( hMenu, IDM_DELETE, (wStatus & (MEB_INBASKET|MEB_MSGWND)) && fHasDatabase );
   MenuEnable( hMenu, IDM_KEEPATTOP, (wStatus & MEB_CONF) && fHasDatabase );
   MenuEnable( hMenu, IDM_PURGE, wStatus & (MEB_INBASKET|MEB_HASMSG) && TestPerm(PF_CANPURGE) && fHasDatabase );
   MenuEnable( hMenu, IDM_PROPERTIES, (wStatus & (MEB_INBASKET|MEB_MSGWND)) && fHasDatabase );
   MenuEnable( hMenu, IDM_CHECK, (wStatus & (MEB_INBASKET|MEB_MSGWND)) && fHasDatabase );
   MenuEnable( hMenu, IDM_SORT, ( ( wStatus & MEB_TOPIC) != MEB_TOPIC ) && (wStatus & (MEB_INBASKET|MEB_MSGWND)) && fHasDatabase );
   MenuEnable( hMenu, IDM_BUILD, (wStatus & (MEB_INBASKET|MEB_MSGWND)) && fHasDatabase );
   MenuEnable( hMenu, IDM_MARKTOPICREAD, (wStatus & (MEB_INBASKET|MEB_HASMSG)) && fHasDatabase );
   MenuEnable( hMenu, IDM_ADDTOADDRBOOK, wStatus & MEB_HASMSG );
   MenuEnable( hMenu, IDM_READMSG, wStatus & MEB_HASMSG );
   MenuEnable( hMenu, IDM_KEEPMSG, wStatus & MEB_HASMSG );
   MenuEnable( hMenu, IDM_READLOCK, wStatus & MEB_HASMSG );
   MenuEnable( hMenu, IDM_SORTMAILFROM, ( wStatus & (MEB_HASMSG|MEB_MAILTOPIC)) == (MEB_HASMSG|MEB_MAILTOPIC) );
   MenuEnable( hMenu, IDM_SORTMAILTO, ( wStatus & (MEB_HASMSG|MEB_MAILTOPIC)) == (MEB_HASMSG|MEB_MAILTOPIC) );
   MenuEnable( hMenu, IDM_WITHDRAW, (wStatus & MEB_HASMSG) && (wStatus & (MEB_CIXTOPIC|MEB_NEWSTOPIC)) );
   MenuEnable( hMenu, IDM_WATCH, (wStatus & MEB_HASMSG) && (wStatus & MEB_NEWSTOPIC) );
   MenuEnable( hMenu, IDM_MARKMSG, wStatus & MEB_HASMSG );
   MenuEnable( hMenu, IDM_PRIORITY, wStatus & MEB_HASMSG );
   MenuEnable( hMenu, IDM_MARKTAGGED, (wStatus & MEB_HASMSG) && (wStatus & MEB_NEWSTOPIC) );
   MenuEnable( hMenu, IDM_PREV, wStatus & MEB_MSGWND );
   MenuEnable( hMenu, IDM_DELETEMSG, wStatus & MEB_HASMSG );
   MenuEnable( hMenu, IDM_VIEWINBROWSER, wStatus & MEB_HASMSG );
   MenuEnable( hMenu, IDM_KILLMSG, wStatus & MEB_HASMSG );
   MenuEnable( hMenu, IDM_DECODE, ( wStatus & MEB_HASFULLMSG ) && !(wStatus & (MEB_CIXTOPIC)));
   MenuEnable( hMenu, IDM_RUNATTACHMENT, wStatus & MEB_ATTACHMENT );
   MenuEnable( hMenu, IDM_IGNORE, wStatus & MEB_HASMSG );
   MenuEnable( hMenu, IDM_MARKTHREADREAD, wStatus & MEB_HASMSG );
   MenuEnable( hMenu, IDM_FORWARD, wStatus & MEB_MSGWND );
   MenuEnable( hMenu, IDM_COMMENT, (wStatus & (MEB_MSGWND|MEB_HASMSG|MEB_READWRITE)) == (MEB_HASMSG|MEB_MSGWND|MEB_READWRITE) );
   MenuEnable( hMenu, IDM_SAY, (wStatus & (MEB_MSGWND|MEB_READWRITE)) == (MEB_MSGWND|MEB_READWRITE) );
   MenuEnable( hMenu, IDM_ORIGINAL, wStatus & MEB_HASMSG );
   MenuEnable( hMenu, IDM_COPYMSG, wStatus & MEB_HASMSG );
   MenuEnable( hMenu, IDM_MOVEMSG, (wStatus & (MEB_MSGWND|MEB_HASMSG|MEB_READWRITE)) == (MEB_MSGWND|MEB_HASMSG|MEB_READWRITE) || fIsAuthor );
   MenuEnable( hMenu, IDM_GOTO, wStatus & MEB_HASMSG );
   MenuEnable( hMenu, IDM_FORWARDMSG, (( wStatus & (MEB_HASMSG|MEB_MSGWND)) == (MEB_HASMSG|MEB_MSGWND)) && (ListBox_GetSelCount( GetDlgItem( hwndTopic, IDD_LIST ) ) == 1 )  ); // !!SM!! 2.56.2058
   
   MenuEnable( hMenu, IDM_SHOWRESUME, wStatus & MEB_HASMSG );
   MenuEnable( hMenu, IDM_MAPISEND, wStatus & MEB_HASMSG );
   MenuEnable( hMenu, IDM_REPLY, ( wStatus & (MEB_HASMSG|MEB_MAILTOPIC)) == (MEB_HASMSG|MEB_MAILTOPIC) );
   MenuEnable( hMenu, IDM_REPLYTOALL, ( wStatus & (MEB_HASMSG|MEB_MAILTOPIC)) == (MEB_HASMSG|MEB_MAILTOPIC) );
   MenuEnable( hMenu, IDM_CANCELARTICLE, (wStatus & (MEB_HASMSG|MEB_NEWSTOPIC)) == (MEB_HASMSG|MEB_NEWSTOPIC) );
   MenuEnable( hMenu, IDM_POSTARTICLE, wStatus & MEB_NEWSTOPIC );
   MenuEnable( hMenu, IDM_FOLLOWUPARTICLE, (wStatus & (MEB_HASMSG|MEB_NEWSTOPIC)) == (MEB_HASMSG|MEB_NEWSTOPIC) );
   MenuEnable( hMenu, IDM_FOLLOWUPARTICLETOALL, (wStatus & (MEB_HASMSG|MEB_NEWSTOPIC)) == (MEB_HASMSG|MEB_NEWSTOPIC) );
   MenuEnable( hMenu, IDM_SORT_REFERENCE, (wStatus & MEB_MSGWND) );
   MenuEnable( hMenu, IDM_SORT_ROOTS, (wStatus & MEB_MSGWND) );
   MenuEnable( hMenu, IDM_SORT_MSGNUM, (wStatus & MEB_MSGWND) );
   MenuEnable( hMenu, IDM_SORT_SENDER, (wStatus & MEB_MSGWND) );
   MenuEnable( hMenu, IDM_SORT_DATETIME, (wStatus & MEB_MSGWND) );
   MenuEnable( hMenu, IDM_SORT_MSGSIZE, (wStatus & MEB_MSGWND) );
   MenuEnable( hMenu, IDM_SORT_SUBJECT, (wStatus & MEB_MSGWND) );
   MenuEnable( hMenu, IDM_SHOW_MSGNUM, (wStatus & MEB_MSGWND) );
   MenuEnable( hMenu, IDM_SHOW_SENDER, (wStatus & MEB_MSGWND) );
   MenuEnable( hMenu, IDM_SHOW_DATETIME, (wStatus & MEB_MSGWND) );
   MenuEnable( hMenu, IDM_SHOW_MSGSIZE, (wStatus & MEB_MSGWND) );
   MenuEnable( hMenu, IDM_SHOW_SUBJECT, (wStatus & MEB_MSGWND) );
   MenuEnable( hMenu, IDM_ASCENDING, (wStatus & MEB_MSGWND) );
   

   /* Show a group button next to the current sort mode and a check
    * mark next to the appropriate view columns.
    */
   if( hwndTopic )
      {
      LPWINDINFO lpWindInfo;

      lpWindInfo = GetBlock( hwndTopic );
      if( lpWindInfo )
         {
         nSort = IDM_SORT_REFERENCE + ( (lpWindInfo->vd.nSortMode & 0x7F) - VSM_REFERENCE );
         CheckMenuGroup( hMenu, IDM_SORT_REFERENCE, IDM_SORT_SUBJECT, nSort );
         CheckMenuItem( hMenu, IDM_ASCENDING, (lpWindInfo->vd.nSortMode & VSM_ASCENDING) ? MF_CHECKED : MF_UNCHECKED );
         CheckMenuItem( hMenu, IDM_SHOW_MSGNUM, (lpWindInfo->vd.wHeaderFlags & VH_MSGNUM) ? MF_CHECKED : MF_UNCHECKED );
         CheckMenuItem( hMenu, IDM_SHOW_SENDER, (lpWindInfo->vd.wHeaderFlags & VH_SENDER) ? MF_CHECKED : MF_UNCHECKED );
         CheckMenuItem( hMenu, IDM_SHOW_DATETIME, (lpWindInfo->vd.wHeaderFlags & VH_DATETIME) ? MF_CHECKED : MF_UNCHECKED );
         CheckMenuItem( hMenu, IDM_SHOW_MSGSIZE, (lpWindInfo->vd.wHeaderFlags & VH_MSGSIZE) ? MF_CHECKED : MF_UNCHECKED );
         CheckMenuItem( hMenu, IDM_SHOW_SUBJECT, (lpWindInfo->vd.wHeaderFlags & VH_SUBJECT) ? MF_CHECKED : MF_UNCHECKED );
         MenuEnable( hMenu, IDM_ASCENDING, ( nSort != IDM_SORT_REFERENCE ) && ( nSort != IDM_SORT_ROOTS ) );
         }
      }

   CheckMenuStates( ); // !!SM!! 2.55

   /* Now let the registered event handlers have a say.
    */
   Amuser_CallRegistered( AE_INIT_MAIN_MENU, (LPARAM)(LPSTR)hMenu, 0L );
}

/* This function changes the menu string without affecting the name
 * of the keystroke assigned to it.
 */
void FASTCALL ChangeMenuString( HMENU hMenu, int wID, char * pszString )
{
   char sz[ 100 ];
   char szKey[ 20 ];
   char * pTab;

   if( GetMenuString( hMenu, wID, sz, sizeof( sz ), MF_BYCOMMAND ) )
      {
      pTab = strchr( sz, '\t' );
      if( pTab )
         {
         strcpy( szKey, ++pTab );
         strcpy( sz, pszString );
         strcat( sz, "\t" );
         strcat( sz, szKey );
         }
      else
         strcpy( sz, pszString );
      MenuString( hMenu, wID, sz );
      }
}

/* This function processes the WM_COMMAND message.
 *
 * Here we handle commands that are 'global' in the sense that they apply to
 * the Palantir environment rather than just the active windows. Commands whose
 * behaviour depends on the active window are passed to the active window procedure.
 */
void FASTCALL MainWnd_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   /* Let the event routines get first bite.
    */
   if( !Amuser_CallRegistered( AE_COMMAND, (LPARAM)(LPSTR)hwndCtl, MAKELPARAM(id,codeNotify) ) )
      return;

   /* Deal with replacement words selected form a popup
    * menu.
    */
   if( id >= IDM_SPELLWORD && id <= IDM_SPELLWORD + 10 )
      {
      char sz[ SSCE_MAX_WORD_SZ ];
      HWND hwndEdit;

      GetMenuString( hEditPopupMenu, id, sz, sizeof( sz ), MF_BYCOMMAND );
      hwndEdit = GetFocus();
      Edit_SetSel( hwndEdit, wSpellSelStart, wSpellSelEnd );
      Edit_ScrollCaret( hwndEdit );
      Edit_ReplaceSel( hwndEdit, sz );
      }

   /* Handle the custom search IDs.
    */
   if( id >= IDM_TERMINALFIRST && id <= IDM_TERMINALLAST )
      NewTerminalByCmd( hwnd, id );
   else if( id >= IDM_TOOLFIRST && id <= IDM_TOOLLAST )
      SendAddonCommand( id );
   else if( id >= IDM_FORMFIRST && id <= IDM_FORMLAST )
      ComposeMailForm( "NEWBUILD", TRUE, NULL, NULL );
   else if( id >= IDM_BLINKMANFIRST && id <= IDM_BLINKMANLAST )
      BlinkByCmd( id );
   else if( id >= IDM_EXTAPPFIRST && id <= IDM_EXTAPPLAST )
      RunExternalApp( id );
   else if( id >= IDM_MOVETOFIRST && id <= IDM_MOVETOLAST )
      {
      char szCategoryName[ LEN_CATNAME+1 ];
      PCAT pcat;

      /* Get the category being dragged.
       */
      GetMenuString( hMoveMenu, id, szCategoryName, sizeof(szCategoryName), MF_BYCOMMAND );
      StrStripPrefix( szCategoryName, szCategoryName, LEN_CATNAME+1 );
      pcat = Amdb_OpenCategory( szCategoryName );
      ASSERT( NULL != pcat );

      if( hwndActive == hwndInBasket )
         InBasket_MoveFolder( pcat, hwndInBasket );
      else if( hwndMsg && IsWindowVisible( GetDlgItem( hwndMsg, IDD_FOLDERLIST ) ) )
         InBasket_MoveFolder( pcat, hwndMsg );
   
      }
   else switch( id )
      {
      case IDM_TOOLBAR_BOTTOM:
      case IDM_TOOLBAR_TOP:
         ASSERT( IsWindow( hwndToolBar ) );
         nToolbarPos = id;
         SetWindowStyle( hwndToolBar, GetWindowStyle( hwndToolBar ) & ~TBSTYLE_VERT );
         Amuser_WritePPInt( szToolbar, "Orientation", nToolbarPos );
         UpdateAppSize();
         break;

      case IDM_TOOLBAR_LEFT:
      case IDM_TOOLBAR_RIGHT:
         ASSERT( IsWindow( hwndToolBar ) );
         nToolbarPos = id;
         SetWindowStyle( hwndToolBar, GetWindowStyle( hwndToolBar ) | TBSTYLE_VERT );
         Amuser_WritePPInt( szToolbar, "Orientation", nToolbarPos );
         UpdateAppSize();
         break;

      case IDM_ADDRBOOK:
         Amaddr_Editor();
         break;

      case IDM_SCRIPTS:
         CmdScripts( hwnd );
         break;

      case IDM_BMPCHGBLINK:
         /* This will only ever be invoked when the user
          * releases the right mouse button from the toolbar
          * popup menu.
          */
         ASSERT( iCachedButtonID != -1 );
         EditButtonBitmap( hwnd, iCachedButtonID );
         iCachedButtonID = -1;
         break;

      case IDM_EDITBLINK:
         /* This will only ever be invoked when the user
          * releases the right mouse button from the toolbar
          * popup menu.
          */
         ASSERT( iCachedButtonID != -1 );
         if (iCachedButtonID == IDM_SENDRECEIVEALL)
            iCachedButtonID = GetBlinkCommandID( "Full" );
         EditBlinkProperties( hwnd, iCachedButtonID );
         iCachedButtonID = -1;
         break;

      case IDM_EDITTERMINAL:
         /* This will only ever be invoked when the user
          * releases the right mouse button from the toolbar
          * popup menu.
          */
         ASSERT( iCachedButtonID != -1 );
         EditTerminal( hwnd, iCachedButtonID );
         iCachedButtonID = -1;
         break;

      case IDM_EDITEXTAPP:
         /* This will only ever be invoked when the user
          * releases the right mouse button from the toolbar
          * popup menu.
          */
         ASSERT( iCachedButtonID != -1 );
         EditExternalApp( hwnd, iCachedButtonID );
         iCachedButtonID = -1;
         break;

      case IDM_CUSTTOOLBAR:
         CmdCustomiseToolbar( hwnd );
         break;

      case IDM_COLOURSETTINGS:
         CustomiseDialog( hwnd, CDU_COLOURS );
         break;

      case IDM_FONTSETTINGS:
         CustomiseDialog( hwnd, CDU_FONTS );
         break;

      case IDM_COMMUNICATIONS:
         CmdConnections( hwnd );
         break;

      case IDM_SCHEDULER:
         CmdScheduler( hwnd );
         break;

      case IDM_SIGNATURES:
         CmdSignature( hwnd, "" );
         break;

      case IDM_ADDONS:
         if( !fNoAddons )
            CmdAddons( hwnd );
         break;

      case IDM_CONFIGADDONS:
         if( !fNoAddons )
            CmdConfigureAddons( hwnd );
         break;

      case IDM_MAPISEND:
         CmdMapiSend( hwnd );
         break;

      case IDM_NEXT:
         if( hwndTopic )
            ForwardCommand( hwndMsg, id, hwndCtl, codeNotify );
         else
            CmdNextUnread( hwnd );
         break;

      case IDM_NEXTPRIORITY: {
         PTH pth = NULL;
         PTL ptl = NULL;
         BOOL fUnread = FALSE;

         if( !hwndTopic ) {
            PCAT pcat;

            if( NULL != ( pcat = Amdb_GetFirstCategory() ) )
               {
               PCL pcl;
   
               if( NULL != ( pcl = Amdb_GetFirstFolder( pcat ) ) )
                  {
                  if( ( ptl = Amdb_GetFirstTopic( pcl ) ) == NULL )
                     {
                     /* No topics in the first folder. So search for the next
                      * folder which DOES have topics.
                      */
                     do {
                        if( ( pcl = Amdb_GetNextFolder( pcl ) ) == NULL )
                           break;
                        ptl = Amdb_GetFirstTopic( pcl );
                        }
                     while( ptl == NULL );
                     }
                  if( ptl )
                     pth = Amdb_GetFirstMsg( ptl, vdDef.nViewMode );
                  }
               }
            }
         else {
            LPWINDINFO lpWindInfo;

            lpWindInfo = (LPWINDINFO)GetBlock( hwndTopic );
            pth = lpWindInfo->lpMessage;
            ptl = lpWindInfo->lpTopic;
            }
         if( pth && Amdb_IsPriority( pth ) && !Amdb_IsRead( pth ) )
            {
            fUnread = TRUE;
            Amdb_MarkMsgRead( pth, TRUE );
            if( hwndTopic )
               UpdateCurrentMsgDisplay( hwndTopic );
            }
         if( ptl && ( NULL != ( pth = Amdb_GetPriorityUnRead( ptl, pth, vdDef.nViewMode, TRUE ) ) ) )
            {
            CURMSG unr;

            unr.pth = pth;
            unr.ptl = Amdb_TopicFromMsg( pth );
            unr.pcl = Amdb_FolderFromTopic( unr.ptl );
            unr.pcat = Amdb_CategoryFromFolder( unr.pcl );
            SetCurrentMsgEx( &unr, TRUE, fUnread, FALSE );
            }
         else
         {
            fMessageBox( hwnd, 0, GS( IDS_STR73 ), MB_OK|MB_ICONEXCLAMATION );
            SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_NEXTPRIORITY, FALSE );
            ShowUnreadMessageCount();
         }

         break;
         }
         
      case IDM_NEXTMARKED:
      case IDM_PREVMARKED: {
         PTH pth = NULL;
         PTL ptl = NULL;
         BOOL fMarked = FALSE;

         if( !hwndTopic ) {
            PCAT pcat;

            if( NULL != ( pcat = Amdb_GetFirstCategory() ) )
               {
               PCL pcl;

               if( NULL != ( pcl = Amdb_GetFirstFolder( pcat ) ) )
                  {
                  if( ( ptl = Amdb_GetFirstTopic( pcl ) ) == NULL )
                     {
                     /* No topics in the first folder. So search for the next
                      * folder which DOES have topics.
                      */
                     do {
                        if( ( pcl = Amdb_GetNextFolder( pcl ) ) == NULL )
                           break;
                        ptl = Amdb_GetFirstTopic( pcl );
                        }
                     while( ptl == NULL );
                     }
                  if( ptl )
                     {
                     Amdb_LockTopic( ptl );
                     pth = Amdb_GetFirstMsg( ptl, vdDef.nViewMode );
                     }
                  fMarked = pth && Amdb_IsMarked( pth );
                  }
               }
            }
         else {
            LPWINDINFO lpWindInfo;

            lpWindInfo = GetBlock( hwndTopic );
            pth = lpWindInfo->lpMessage;
            ptl = lpWindInfo->lpTopic;
            }
         if( !fMarked )
            fMarked = ptl && ( NULL != ( pth = Amdb_GetMarked( ptl, pth, vdDef.nViewMode, id == IDM_NEXTMARKED ) ) );
         if( fMarked )
            {
            CURMSG unr;

            unr.pth = pth;
            unr.ptl = Amdb_TopicFromMsg( pth );
            unr.pcl = Amdb_FolderFromTopic( unr.ptl );
            unr.pcat = Amdb_CategoryFromFolder( unr.pcl );
            SetCurrentMsg( &unr, TRUE, TRUE );
            }
         else
            fMessageBox( hwnd, 0, GS( IDS_STR212 ), MB_OK|MB_ICONEXCLAMATION );
         break;
         }

      case IDM_EVENTLOG:
         CmdEventLog( hwnd );
         break;

      case IDM_SEARCH:
         GlobalFind( hwnd );
         break;

      case IDM_STOPIMPORT:
         if( fParsing )
            {
            fParsing = FALSE; /* Suspend parsing while messagebox is up */
            fParsing = fMessageBox( hwnd, 0, GS(IDS_STR831), MB_YESNO|MB_ICONINFORMATION ) == IDNO;
            ToolbarState_Import();
            if( FALSE == fParsing )
               {
               /* MUST clear queue before calling CIXImportParseCompleted or
                * one more scratchpad will get parsed.
                */
               ClearScratchpadQueue();
               CIXImportParseCompleted();
               fpParser = NULL;
               }
            }
         break;

      case IDM_IMPORT:
         CmdImport( hwnd );
         break;

      case IDM_QUICKEXPORT:
         CIXExport( hwnd );
         break;

      case IDM_EXPORT:
         CmdExport( hwnd );
         break;

      case IDM_ROT13:
         CmdTranslateSelection( hwnd, Rot13Function );
         break;

      case IDM_UPPERCASE:
         CmdTranslateSelection( hwnd, toupper );
         break;

      case IDM_LOWERCASE:
         CmdTranslateSelection( hwnd, tolower );
         break;

      case IDM_INSERTFILE:
         CmdInsertFile( hwnd );
         break;

      case IDM_FIND:
         CmdFind( hwnd );
         break;

      case IDM_REPLACE:
         CmdReplace( hwnd );
         break;

      case IDM_QUOTE:
         if( hwndQuote )
            CmdQuote( hwndQuote );
         break;

      case IDM_KEEPATTOP:
         InBasket_KeepAtTop( hwnd );
         break;

      case IDM_DELETE:
         InBasket_DeleteCommand( hwnd );
         break;

      case IDM_RENAME:
         InBasket_RenameCommand( hwnd );
         break;

      case IDM_PURGE:
         InBasket_PurgeCommand( hwnd );
         break;

      case IDM_REBUILD_INDEX:
         if( fMessageBox( hwnd, 0, GS(IDS_STR495), MB_YESNO|MB_ICONQUESTION ) == IDYES )
            Amdb_RebuildDatabase( pcatCIXDatabase, hwnd );
         break;

      case IDM_BUILD:
         InBasket_BuildCommand( hwnd );
         break;

      case IDM_CHECK:
         InBasket_CheckCommand( hwnd );
         break;

      case IDM_DOWNLOAD:
         CmdCIXFileDownload( hwnd );
         break;

      case IDM_RESIGN_POPUP:
      case IDM_RESIGN:
      case IDM_UNSUBSCRIBE:
         CmdResign( hwnd, NULL, TRUE, TRUE );
         break;

      case IDM_GETARTICLES:
         InBasket_GetNewArticles( hwnd );
         break;

      case IDM_RETHREADARTICLES:
         InBasket_RethreadArticles( hwnd );
         break;

      case IDM_GETTAGGED:
         InBasket_GetTagged( hwnd );
         break;

      case IDM_SHOWALLGROUPS: {
         char szDefSrvr[ 64 ];

         Amuser_GetPPString( szNews, "Default Server", szNewsServer, szDefSrvr, 64 );
         ReadGroupsDat( hwnd, szDefSrvr );
         break;
         }

      case IDM_SUBSCRIBE:
         Subscribe( hwnd, "" );
         break;

      case IDM_READNEWS:
         if( NULL != pclNewsFolder )
         {
            PTL ptlFirst;

            ptlFirst = Amdb_GetFirstTopic( pclNewsFolder );
            if( NULL != ptlFirst )
               OpenTopicViewerWindow( ptlFirst );
            else
               fMessageBox( hwnd, 0, GS( IDS_STR1245 ), MB_OK|MB_ICONEXCLAMATION );
         }
         break;

      case IDM_READMAIL:
         if( NULL != ptlPostmaster )
            OpenTopicViewerWindow( ptlPostmaster );
         break;

      case IDM_TOOLBAR:
         /* Toggle whether or not the toolbar is visible.
          */
         ASSERT( IsWindow( hwndToolBar ) );
         CmdShowToolbar( !fShowToolBar );
         break;

      case IDM_STATUSBAR:
         /* Toggle whether or not the status bar is visible.
          */
         ASSERT( IsWindow( hwndStatus ) );
         CmdShowStatusBar( !IsWindowVisible( hwndStatus ) );
         break;

      case IDM_TERMINAL:
         /* Open another terminal session.
          */
         NewTerminal( hwnd, NULL, FALSE );
         break;

      case IDM_POSTARTICLE:
         CmdPostArticle( hwnd, NULL );
         break;

      case IDM_MAIL:
         CmdMailMessage( hwnd, NULL, NULL );
         break;

      case IDM_GETNEWMAIL:
         if( !Amob_Find( OBTYPE_GETNEWMAIL, NULL ) )
         {
            Amob_New( OBTYPE_GETNEWMAIL, NULL );
            Amob_Commit( NULL, OBTYPE_GETNEWMAIL, NULL );
            Amob_Delete( OBTYPE_GETNEWMAIL, NULL );
         }
         break;

      case IDM_HEADERS:
         fStripHeaders = !fStripHeaders;
         SendAllWindows( WM_TOGGLEHEADERS, 0, 0L );
         Amuser_WritePPInt( szSettings, "ShowHeaders", fStripHeaders );
         break;

      case IDM_FIXEDFONT:
         SendMessage( hwndActive, WM_TOGGLEFIXEDFONT, 0, 0L );
         break;

      case IDM_MSGSTYLES:
         fMsgStyles = !fMsgStyles;
         SendAllWindows( WM_TOGGLEHEADERS, 0, 0L );
         Amuser_WritePPInt( szSettings, "ShowMessageStyles", fMsgStyles );
         break;

      case IDM_WORDWRAP:
         fWordWrap = !fWordWrap;
         SendAllWindows( WM_TOGGLEHEADERS, 0, 0L );
         Amuser_WritePPInt( szSettings, "ShowWordWrap", fWordWrap );
         break;

      case IDM_MLHOTLINKS:  // 2.55.2031
         fMultiLineURLs = !fMultiLineURLs;
         SendAllWindows( WM_TOGGLEHEADERS, 0, 0L );
         Amuser_WritePPInt( szSettings, "MultiLineURLs", fMultiLineURLs );
         break;

      case IDM_HOTLINKS:
         fHotLinks = !fHotLinks;
         SendAllWindows( WM_TOGGLEHEADERS, 0, 0L );
         Amuser_WritePPInt( szSettings, "ShowHotLinks", fHotLinks );
         break;

      case IDM_NEWFOLDER:
         CreateNewFolder();
         break;

      case IDM_NEWLOCALTOPIC:
         CreateNewLocalTopic( hwnd );
         break;

      case IDM_NEWCATEGORY:
         CreateNewCategory();
         break;

      case IDM_NEWMAILFOLDER:
         CreateNewMailFolder( hwnd );
         break;

      case IDM_USENET:
         CmdUsenetSettings( hwnd );
         break;

      case IDM_PREFERENCES:
         PreferencesDialog( hwnd, nLastOptionsDialogPage );
         break;

      case IDM_PURGESETTINGS:
         CmdPurgeSettings( hwnd );
         break;

      case IDM_DIRECTORIES:
         DirectoriesDialog( hwnd, nLastDirectoriesDialogPage );
         break;

      case IDM_MAILSETTINGS:
         MailDialog( hwnd, nLastMailDialogPage );
         break;

      case IDM_CUSTOMISE:
         CustomiseDialog( hwnd, nLastCustomiseDialogPage );
         break;

      case IDM_FILTERS:
         CmdRules( hwnd );
         break;

      case IDM_ONLINE:
         CmdOnline();
         break;

      case IDM_STOP:
         CmdOffline();
         break;

      case IDM_SENDRECEIVEALL:
         BlinkByCmd( GetBlinkCommandID( "Full" ) );
         break;

      case IDM_CUSTOMBLINK:
         if( !fBlinking && !IsServerBusy() )
            CmdCustomBlink( hwnd );
         break;

      case IDM_APPMAX:
         /* Maximise the application window.
          */
         if( !IsZoomed( hwnd ) )
            ShowWindow( hwnd, SW_SHOWMAXIMIZED );
         break;

      case IDM_DOCMAX: {
         HWND hwndMDI;

         /* Maximise the current document window.
          */
         hwndMDI = (HWND)SendMessage( hwndMDIClient, WM_MDIGETACTIVE, 0, 0L );
         if( !IsZoomed( hwndMDI ) )
            ShowWindow( hwndMDI, SW_SHOWMAXIMIZED );
         break;
         }

      case IDM_APPREST:
         /* Restore the application window.
          */
         if( IsZoomed( hwnd ) || IsIconic( hwnd ) )
         {
            FlashWindow( hwnd, 0 );
            ShowWindow( hwnd, SW_SHOWNORMAL );
         }
         break;

      case IDM_DOCREST: {
         HWND hwndMDI;

         /* Restore the current document window.
          */
         hwndMDI = (HWND)SendMessage( hwndMDIClient, WM_MDIGETACTIVE, 0, 0L );
         if( IsZoomed( hwndMDI ) || IsIconic( hwndMDI ) )
            ShowWindow( hwndMDI, SW_SHOWNORMAL );
         break;
         }

      case IDM_APPMIN:
         /* Minimise the application window.
          */
         if( !IsIconic( hwnd ) )
            ShowWindow( hwnd, SW_SHOWMINIMIZED );
         break;

      case IDM_DOCMIN: {
         HWND hwndMDI;

         /* Minimise the current document window.
          */
         hwndMDI = (HWND)SendMessage( hwndMDIClient, WM_MDIGETACTIVE, 0, 0L );
         if( !IsIconic( hwndMDI ) )
            ShowWindow( hwndMDI, SW_SHOWMINIMIZED );
         break;
         }

      case IDM_APPSIZE:
         /* Resize the application window.
          */
         if( !IsZoomed( hwnd ) && !IsIconic( hwnd ) )
            SendMessage( hwnd, WM_SYSCOMMAND, SC_SIZE, 0L );
         break;

      case IDM_DOCSIZE: {
         HWND hwndMDI;

         /* Resize the current document window.
          */
         hwndMDI = (HWND)SendMessage( hwndMDIClient, WM_MDIGETACTIVE, 0, 0L );
         if( !IsZoomed( hwndMDI ) && !IsIconic( hwndMDI ) )
            SendMessage( hwndMDI, WM_SYSCOMMAND, SC_SIZE, 0L );
         break;
         }

      case IDM_APPMOVE:
         /* Move the application window.
          */
         if( !IsZoomed( hwnd ) && !IsIconic( hwnd ) )
            SendMessage( hwnd, WM_SYSCOMMAND, SC_MOVE, 0L );
         break;

      case IDM_DOCMOVE: {
         HWND hwndMDI;

         /* Move the current document window.
          */
         hwndMDI = (HWND)SendMessage( hwndMDIClient, WM_MDIGETACTIVE, 0, 0L );
         if( !IsZoomed( hwndMDI ) && !IsIconic( hwndMDI ) )
            SendMessage( hwndMDI, WM_SYSCOMMAND, SC_MOVE, 0L );
         break;
         }

      case IDM_UPDFILELIST:
         UpdateFileLists( NULL, TRUE );
         break;

      case IDM_UPDPARTLIST:
         UpdatePartLists( NULL, TRUE );
         break;

      case IDM_UPDCONFNOTES:
         UpdateConfNotes( NULL, TRUE );
         break;

      case IDM_UPDCIXLIST:
         UpdateCIXList();
         break;

      case IDM_UPDUSERSLIST:
         UpdateUsersList();
         break;

      case IDM_CIXFILELINK:
         break;

      case IDM_SORT:
         InBasket_SortCommand( hwnd );
         break;

      case IDM_COPYLINK:
         {
            HGLOBAL gHnd;
            if  (fCurURL[0] != '\x0')
            {
               
               if( NULL != ( gHnd = GlobalAlloc( GHND, strlen( fCurURL ) + 1 ) ) )
               {
                  char * gPtr;
                  
                  /* Copy it to the clipboard.
                  */
                  if( NULL != ( gPtr = GlobalLock( gHnd ) ) )
                  {
                     strcpy( gPtr, fCurURL );
                     GlobalUnlock( gHnd );
                     if( OpenClipboard( hwndFrame ) )
                     {
                        EmptyClipboard();
                        SetClipboardData( CF_TEXT, gHnd );
                        CloseClipboard();
                        break;
                     }
                  }
                  GlobalFree( gHnd );
               }
            }
            break;
         }

      case IDM_COPYCIXLINK: {
         MSGINFO msginfo;
         CURMSG curmsg;
         char sz[ 80 ];
         HGLOBAL gHnd;

         /* Create a CIX URL for the current message.
          */
         Ameol2_GetCurrentMsg( &curmsg );
         if( NULL != curmsg.pth )
            {
            Amdb_GetMsgInfo( curmsg.pth, &msginfo );
            if( GetAsyncKeyState( VK_SHIFT ) & 0x8000 )
               wsprintf( sz, "cix:%lu", msginfo.dwMsg );
            else
               wsprintf( sz, "cix:%s/%s:%lu", Amdb_GetFolderName( curmsg.pcl ), Amdb_GetTopicName( curmsg.ptl ), msginfo.dwMsg );
            if( NULL != ( gHnd = GlobalAlloc( GHND, strlen( sz ) + 1 ) ) )
               {
               char * gPtr;

               /* Copy it to the clipboard.
                */
               if( NULL != ( gPtr = GlobalLock( gHnd ) ) )
                  {
                  strcpy( gPtr, sz );
                  GlobalUnlock( gHnd );
                  if( OpenClipboard( hwndFrame ) )
                     {
                     EmptyClipboard();
                     SetClipboardData( CF_TEXT, gHnd );
                     CloseClipboard();
                     break;
                     }
                  }
               GlobalFree( gHnd );
               }
            }
         break;
         }

      case IDM_BROWSE: {
         int cchSelectedText;
         LPSTR pSelectedText;
         HWND hwndEdit;
         HEDIT hedit;

         INITIALISE_PTR(pSelectedText);

         /* BUGBUG: This code is duplicate in many places. Put it
          * in a shared function!
          */
         hwndEdit = GetFocus();
         hedit = Editmap_Open( hwndEdit );
         cchSelectedText = 0;
         if( ETYP_NOTEDIT != hedit )
            {
            BEC_SELECTION bsel;

            Editmap_GetSelection( hedit, hwndEdit, &bsel );
            if( bsel.lo != bsel.hi )
               cchSelectedText = LOWORD( (bsel.hi - bsel.lo) ) + 1;// + 3; /*!!SM!! +3??*/
            }

         /* If no selection, just run the browser.
          */
         if( 0 == cchSelectedText )
         {
            if (fCurURL[0] == '\x0')
            {
               CommonHotspotHandler( hwnd, "http://www.cix.co.uk" );
            }
            else
            {
               CommonHotspotHandler( hwnd, fCurURL ); // 2.56.2051 FS#96
            }
         }
         else if( fNewMemory( &pSelectedText, cchSelectedText + 1 ) )
            {
            LPSTR lpText = NULL;                   /* Pointer to entire text being checked */

            /* Get the selected text and invoke the browser. Strip
             * off any CR, LF and spaces at the start and end of
             * the string.
             */
            if( ETYP_BIGEDIT != hedit )
            {
               UINT cbText;
               WORD wStart, wEnd;
               DWORD dwSel;

               cbText = Edit_GetTextLength( hwndEdit );
               if( fNewMemory( &lpText, cbText + 1 ) )
                  {
                     Edit_GetText( hwndEdit, lpText, cbText + 1 );
                     dwSel = Edit_GetSel( hwndEdit );
                     wStart = LOWORD( dwSel );
                     wEnd = HIWORD( dwSel );
                     if( wStart != wEnd )
                        {
                        UINT lenSel;
                  
                        lenSel =  wEnd - wStart;
                        if( wStart )
                           _fmemcpy( lpText, lpText + wStart, lenSel );
                        lpText[ lenSel ] = '\0';
                        }
                     pSelectedText = lpText;
                  }
            }
            else if( cchSelectedText )
               Editmap_GetSel( hedit, hwndEdit, cchSelectedText, pSelectedText );
            pSelectedText[ cchSelectedText ] = '\0';
            StripLeadingTrailingChars( pSelectedText, '\r' );
            StripLeadingTrailingChars( pSelectedText, '\n' );
            StripLeadingTrailingChars( pSelectedText, ' ' );
            CommonHotspotHandler( hwnd, pSelectedText );
            FreeMemory( &pSelectedText );
            }
         break;
         }

      case IDM_CLOSE: {
         HWND hwndMDI;

         /* Close the current document window.
          */
         hwndMDI = (HWND)SendMessage( hwndMDIClient, WM_MDIGETACTIVE, 0, 0L );
         SendMessage( hwndMDI, WM_CLOSE, 0, 0L );
         break;
         }

      case IDM_UNDO:
         SendMessage( GetFocus(), WM_UNDO, 0, 0L );
         ToolbarState_CopyPaste();
         break;

#ifndef USEBIGEDIT
      case IDM_REDO: // 20060302 - 2.56.2049.08
         SendMessage( GetFocus(), SCI_REDO, 0, 0L );
         ToolbarState_CopyPaste();
         break;
#endif USEBIGEDIT

      case IDM_CUT:
         SendMessage( GetFocus(), WM_CUT, 0, 0L );
         ToolbarState_CopyPaste();
         break;

      case IDM_CLEAR:
         SendMessage( GetFocus(), WM_CLEAR, 0, 0L );
         ToolbarState_CopyPaste();
         break;

      case IDM_COPY:
         SendMessage( GetFocus(), WM_COPY, 0, 0L );
         ToolbarState_CopyPaste();
         break;

      case IDM_PASTE:
         SendMessage( GetFocus(), WM_PASTE, 0, 0L );
         break;

      case IDM_SELECTALL: {
         HWND hwndEdit;
         HEDIT hedit;

         if( NULL != hwndTopic )
            hwndEdit = GetDlgItem( hwndTopic, IDD_MESSAGE );
         else
            hwndEdit = GetFocus();
         hedit = Editmap_Open( hwndEdit );
         if( hedit != ETYP_NOTEDIT )
            {
//          BEC_SELECTION bsel;

//          bsel.lo = 0;
//          bsel.hi = 0xFFFFFFFF;
//          Editmap_SetSel( hedit, hwndEdit, &bsel );

            Editmap_SelectAll( hedit, hwndEdit );
            SetFocus( hwndEdit );
            }
         ToolbarState_CopyPaste();
         break;
         }

      case IDM_COPYALL: {
         HWND hwndEdit;
         HEDIT hedit;

         if( NULL != hwndTopic )
            hwndEdit = GetDlgItem( hwndTopic, IDD_MESSAGE );
         else
            hwndEdit = GetFocus();
         hedit = Editmap_Open( hwndEdit );
         if( hedit != ETYP_NOTEDIT )
            {
            BEC_SELECTION bsel;

            bsel.lo = 0;
            bsel.hi = 0xFFFFFFFF;
            Editmap_SetSel( hedit, hwndEdit, &bsel );
            SetFocus( hwndEdit );
            }
         SendMessage( GetFocus(), WM_COPY, 0, 0L );
         ToolbarState_CopyPaste();
         break;
         }

      case IDM_PRINTSETUP:
         PrintSetup( hwnd );
         break;

      case IDM_PAGESETUP:
         CmdPageSetup( hwnd );
         break;

      case IDM_HELPCONTENTS: {
         wsprintf( lpPathBuf, "%s%s", pszAmeolDir, szHelpFile );
         HtmlHelp( GetDesktopWindow(), lpPathBuf, HH_DISPLAY_TOC, 0 );
         break;
         }

      case IDM_REPORT:
         ReportWindow( hwnd );
         break;

      case IDM_ABOUT:
         CmdAbout( hwnd );
         break;

#ifdef USEUPDWIZ
      case IDM_UPDATEAMEOL: {
         UINT uRetCode;

         wsprintf( lpPathBuf, "%samupdwiz.exe", pszAmeolDir );
         if( ( uRetCode = (UINT)ShellExecute( hwndFrame, NULL, lpPathBuf, NULL, pszAmeolDir, SW_SHOWNORMAL ) ) < 32 )
            DisplayShellExecuteError( hwndFrame, lpPathBuf, uRetCode );
         break;
         }
#endif USEUPDWIZ

      case IDM_SHOWTIPS:
         CmdShowTips( hwnd );
         break;

      case IDM_CIXSUPPORT:
         CmdCIXSupport( hwnd );
         break;

      case IDM_CHECKFORUPDATES:
         CmdCheckForUpdates( hwnd );
         break;

      case IDM_STARTWIZ:
         CmdStartWiz( hwnd );
         break;

      case IDM_ADMIN:
         CmdAdmin( hwnd );
         break;

      case IDM_NEWWINDOW:
         CmdNewWindow( hwnd );
         break;

      case IDM_CASCADE:
         SendMessage( hwndMDIClient, WM_MDICASCADE, 0, 0L );
         break;

      case IDM_TILEHORZ:
         SendMessage( hwndMDIClient, WM_MDITILE, MDITILE_HORIZONTAL, 0L );
         break;

      case IDM_TILEVERT:
         SendMessage( hwndMDIClient, WM_MDITILE, MDITILE_VERTICAL, 0L );
         break;

      case IDM_ARRANGEICONS:
         SendMessage( hwndMDIClient, WM_MDIICONARRANGE, 0, 0L );
         break;

      case IDM_CLOSEALL:
         SendAllWindows( WM_CLOSE, 0, 0L );
         break;

      case IDM_OUTBASKET:
         CreateOutBasket( hwnd );
         break;

      case IDM_INBASKET:
      case IDM_INBASKET2:
         CreateInBasket( hwnd );
         break;

      case IDM_EXIT:
         SendMessage( hwnd, WM_CLOSE, 0, 0L );
         break;

      default: {
         HWND hwndMDI;

         hwndMDI = (HWND)SendMessage( hwndMDIClient, WM_MDIGETACTIVE, 0, 0L );
         if( hwndMDI != NULL )
            ForwardCommand( hwndMDI, id, hwndCtl, codeNotify );
         FORWARD_WM_COMMAND( hwnd, id, hwndCtl, codeNotify, DefFrameWindowProc );
         }
      }
}

/* This function places Ameol online.
 */
void FASTCALL CmdOnline( void )
{
   if( !fOnline && !fBlinking )
      {
      SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_ONLINE, FALSE );
      SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_STOP, TRUE );
      fOnline = TRUE;
      Amuser_CallRegistered( AE_CONTEXTSWITCH, AECSW_ONLINE, TRUE );
      }
}

/* This function places Ameol offline.
 */
void FASTCALL CmdOffline( void )
{
   if( fOnline || fBlinking || fInitiatingBlink )
      if( CloseOnlineWindows( hwndFrame, TRUE ) )
         {
         if( fBlinking )
            {
            EndOnlineBlink();
            DestroyBlinkWindow();
            }
         SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_ONLINE, TRUE );
         SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_STOP, FALSE );
         fInitiatingBlink = FALSE;
         fOnline = FALSE;
         }
}

/* This function handles the Next Unread command.
 */
void FASTCALL CmdNextUnread( HWND hwnd )
{
   CURMSG unr;

   unr.pcl = NULL;
   unr.ptl = NULL;
   unr.pth = NULL;
   unr.pcat = NULL;
   if( hwndActive == hwndInBasket )
      {
      HWND hwndTreeCtl;
      HTREEITEM hSelItem;
   
      hwndTreeCtl = GetDlgItem( hwndInBasket, IDD_FOLDERLIST );
      hSelItem = TreeView_GetSelection( hwndTreeCtl );
      if( hSelItem )
         {
         TV_ITEM tv;
   
         /* Get the lParam of the selected item.
          */
         tv.hItem = hSelItem;
         tv.mask = TVIF_PARAM;
         VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );

         /* Seek starting from whatever is the selected
          * database, folder or topic.
          */
         if( Amdb_IsTopicPtr( (PTL)tv.lParam ) )
            {
            unr.ptl = (PTL)tv.lParam;
            unr.pcl = Amdb_FolderFromTopic( unr.ptl );
            unr.pcat = Amdb_CategoryFromFolder( unr.pcl );
            }
         else if( Amdb_IsFolderPtr( (PCL)tv.lParam ) )
            {
            unr.pcl = (PCL)tv.lParam;
            unr.ptl = Amdb_GetFirstTopic( unr.pcl );
            unr.pcat = Amdb_CategoryFromFolder( unr.pcl );
            }
         else
            {
            unr.pcat = (PCAT)tv.lParam;
            unr.pcl = Amdb_GetFirstFolder( unr.pcat );
            unr.ptl = Amdb_GetFirstTopic( unr.pcl );
            }
         }
      }
   if( Amdb_GetNextUnReadMsg( &unr, vdDef.nViewMode ) )
      {
      OpenMsgViewerWindow( unr.ptl, unr.pth, FALSE );
      Amdb_InternalUnlockTopic( unr.ptl );
      }
   else
      fMessageBox( hwnd, 0, GS( IDS_STR81 ), MB_OK|MB_ICONEXCLAMATION );
}

/* This function handles the Send Mail command.
 */
void FASTCALL CmdMapiSend( HWND hwnd )
{
   HINSTANCE hMAPIDLL;
   ULONG f;

   /* Try to load the messaging DLL.
    */
   if( (int)( hMAPIDLL = LoadLibrary( szMapiLib ) ) >= 32 && hwndMsg != NULL )
      {
      LPWINDINFO lpWindInfo;

      /* Dereference the Send Mail entry point in the DLL.
       */
      (FARPROC)fMAPISendMail = GetProcAddress( hMAPIDLL, "MAPISendMail" );

      /* Get the current message viewer record.
       */
      lpWindInfo = GetBlock( hwndMsg );
      if( lpWindInfo->lpMessage )
         {
         HPSTR lpText;
         MSGINFO msginfo;

         /* Get the message details and the text of the message.
          */
         Amdb_GetMsgInfo( lpWindInfo->lpMessage,  &msginfo );
         if( NULL != ( lpText = Amdb_GetMsgText( lpWindInfo->lpMessage ) ) )
            {
            MapiRecipDesc sender;
            MapiMessage note;

            /* Prime the sending details.
             */
            sender.ulReserved = 0;
            sender.ulRecipClass = MAPI_ORIG;
            sender.lpszName = msginfo.szAuthor;
            sender.lpszAddress = NULL;
            sender.ulEIDSize = 0;
            sender.lpEntryID = NULL;
            note.ulReserved = 0;
            note.lpszSubject = msginfo.szTitle;
            note.lpszNoteText = lpText;
            note.lpszMessageType = NULL;
            note.lpszDateReceived = NULL;
            note.lpszConversationID = NULL;
            note.flFlags = 0;
            note.lpOriginator = &sender;
            note.nRecipCount = 0;
            note.lpRecips = NULL;
            note.nFileCount = 0;
            note.lpFiles = NULL;

            /* Now call the MAPI send mail function.
             */
            f = (*fMAPISendMail)( 0, 0L, &note, MAPI_DIALOG|MAPI_LOGON_UI, 0L );
            Amdb_FreeMsgTextBuffer( lpText );
            }
         }
      FreeLibrary( hMAPIDLL );
      }
}

/* This function handles the quote command. The hwndQuote parameter
 * specifies the edit window from which to quote. The hwndActive
 * variable must be set to the handle of the new edit window.
 */
void FASTCALL CmdQuote( HWND hwndQuote )
{
   HPSTR lpText;

   lpText = CreateQuotedMessage( hwndQuote, TRUE );
   if( NULL != lpText )
      {
      HWND hwndEdit;
      
      hwndEdit = GetDlgItem( hwndActive, IDD_EDIT );
      Edit_ReplaceSel( hwndEdit, lpText );

      FreeMemory32( &lpText );
      }
}

/* This function creates a quoted copy of the text for the
 * specified message.
 */
LPSTR FASTCALL CreateQuotedMessage( HWND hwndQuote, BOOL fWrap )
{
   LPWINDINFO lpWindInfo;
   HPSTR lpText;
   HPSTR lpDest;
   LPSTR lpszPreText;
   LPSTR lp2;

   lpszPreText = NULL;
   INITIALISE_PTR(lpDest);
   lpWindInfo = GetBlock( hwndQuote );
   if( lpWindInfo->lpMessage && ( NULL != ( lpText = Amdb_GetMsgText( lpWindInfo->lpMessage ) ) ) )
      {
      BOOL fWrappedOnLast;
      BOOL fCheckQuote;
      BOOL fInvalidSelection;
      int cQuotesSeen;
      BEC_SELECTION bsel;
      HWND hwndMsg;
      HEDIT hedit;
      DWORD cbText;
      PCL pcl;


      /* Get source and target edit windows.
       */
      hwndMsg = GetDlgItem( hwndQuote, IDD_MESSAGE );
      hedit = Editmap_Open( hwndMsg );
      fInvalidSelection = FALSE;

      /* Get the message we are going to quote.
       */
      if( fStripHeaders && ( Amdb_GetTopicType( Amdb_TopicFromMsg( lpWindInfo->lpMessage ) ) == FTYPE_MAIL || Amdb_GetTopicType( Amdb_TopicFromMsg( lpWindInfo->lpMessage ) ) == FTYPE_NEWS ) )
         lp2 = GetReadableText(Amdb_TopicFromMsg( lpWindInfo->lpMessage ), lpText ); //!!SM!! 2039
      else
         lp2 = GetTextBody( Amdb_TopicFromMsg( lpWindInfo->lpMessage ), lpText );

      Editmap_GetSelection( hedit, hwndMsg, &bsel );

      if( fShortHeaders && fStripHeaders && ( Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_MAIL || 
         Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_NEWS ) )
      {
         lpszPreText = GetShortHeaderText( lpWindInfo->lpMessage );
         if( NULL != lpszPreText && *lpszPreText )
         {
            bsel.lo -= strlen( lpszPreText );
            bsel.hi -= strlen( lpszPreText );
         }
      }

      if( bsel.lo != bsel.hi )
      {  
         SELRANGE lenSel;

         lenSel =  bsel.hi - bsel.lo + 1; //!!SM!! 2.55.2035

         INITIALISE_PTR( lp2 );
         if( fNewMemory32( &lp2, lenSel + 1) )
         {
            Editmap_GetSel( hedit, hwndMsg, lenSel, lp2 );
         }
      }
      else
      {
         LPSTR lpTmp;

         if( fShortHeaders && fStripHeaders && ( Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_MAIL || 
            Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_NEWS ) )
         {
            if( fStripHeaders && ( Amdb_GetTopicType( Amdb_TopicFromMsg( lpWindInfo->lpMessage ) ) == FTYPE_MAIL || Amdb_GetTopicType( Amdb_TopicFromMsg( lpWindInfo->lpMessage ) ) == FTYPE_NEWS ) )
               lpTmp = lpText; // !!SM!! 2039
            else
               lpTmp = GetTextBody( Amdb_TopicFromMsg( lpWindInfo->lpMessage ), lpText );
  
            lpTmp = lpText ;
            INITIALISE_PTR( lp2 );
            if( fNewMemory32( &lp2, hstrlen( lpText ) + strlen( lpszPreText ) + 1) )
            {
               strcpy(lp2, lpszPreText);
               hstrcat(lp2, lpTmp);
            }        
         }
      }
      
      /* Create a target buffer.
       */
      if( NULL != lpszPreText && *lpszPreText )
         cbText = hstrlen( lp2 ) + strlen(lpszPreText);
      else
         cbText = hstrlen( lp2 );
      if( !fInvalidSelection && fNewMemory32( &lpDest, ( ( cbText + 1 ) * 2 ) + 2 ) ) // 2.55.2034 Needs extra for small messages
         {
         LPSTR lpDestPtr;

         /* Initialise.
          */
         pcl = Amdb_FolderFromTopic( lpWindInfo->lpTopic );
         fEditAbort = FALSE;
         fWrappedOnLast = FALSE;
         fCheckQuote = TRUE;
         cQuotesSeen = 0;
         lpDestPtr = lpDest;
         while( *lp2 && !fEditAbort )
            {
            BOOL fWrapped;
            register int c;

            c = 0;
            lpDestPtr[ c++ ] = chQuote;
            lpDestPtr[ c++ ] = ' ';
            if( cQuotesSeen )
               {
               while( cQuotesSeen-- )
                  {
                  lpDestPtr[ c++ ] = chQuote;
                  lpDestPtr[ c++ ] = ' ';
                  }
               fCheckQuote = TRUE;
               }
            fWrapped = FALSE;
            cQuotesSeen = 0;
            while( c < iWrapCol-4 && *lp2 && c < ( MAX_WRAPCOLUMN - 3 ) )
            {
//          while( c < iWrapCol-2 && *lp2 && c < ( MAX_WRAPCOLUMN - 3 ) )
               if( *lp2 == CR || *lp2 == LF )
               {
                  LPSTR lp3;

                  if( !fWrapped )
                     break;
                  lp3 = lp2;
                  if( *lp2 == CR ) ++lp2;
                  if( *lp2 == LF ) ++lp2;
                  fWrapped = FALSE;
                  if( *lp2 == CR || *lp2 == LF || c + 3 >= iWrapCol )
                  {
                     lp2 = lp3;
                     break;
                  }
               }
               else if( fWrap && (*lp2 == chQuote && *(lp2+1) == ' ') && (( c > 0) && ( *(lp2-1) == CR || *(lp2-1) == LF )) ) // 20060505 2.56.2051 FS#109 
//             else if( fWrap && (*lp2 == chQuote && *(lp2+1) == ' '))
               {
                  if( fCheckQuote )
                  {
                     if( fWrappedOnLast )
                        lp2 += 2;
                     else
                        lpDestPtr[ c++ ] = *lp2++;
                  }
                  else
                     lpDestPtr[ c++ ] = *lp2++;
                  ++cQuotesSeen;
               }
               else 
               {
                  if( cQuotesSeen )
                     fCheckQuote = FALSE;
                  if( *lp2 == ' ' || *lp2 == '\t' )
                     if( *(lp2+1) == LF || *(lp2+1) == CR && fWrappedOnLast )
                        fWrapped = TRUE;
                  lpDestPtr[ c++ ] = *lp2++;
               }
            }
            fCheckQuote = FALSE;
            if( fWrap && (c == iWrapCol - 4) && *lp2 != CR && *lp2 != LF )
//          if( fWrap && (c == iWrapCol - 2) && *lp2 != CR && *lp2 != LF )
            {
               LPSTR lp3 = lp2;
               register int d = c;

               while( c > 2 && lpDestPtr[ c-1 ] != ' ' ) {
                  --lp2;
                  --c;
                  }
               if( c == 2 ) {
                  lp2 = lp3;
                  c = d;
                  }
               fCheckQuote = TRUE;
               fWrappedOnLast = TRUE;
               }
            else
               {
               fWrappedOnLast = FALSE;
               cQuotesSeen = 0;
               }
            if( *lp2 == CR )
               ++lp2;
            if( *lp2 == LF )
               ++lp2;
            lpDestPtr[ c++ ] = CR;
            lpDestPtr[ c++ ] = LF;
            lpDestPtr += c;
            }
         *lpDestPtr = '\0';
         }
      Amdb_FreeMsgTextBuffer( lpText );
   }
   if( NULL != lpszPreText && *lpszPreText )
      FreeMemory( &lpszPreText );
   return( lpDest );
}

/* This function handles translation of a selected stream of
 * characters.
 */
void FASTCALL CmdTranslateSelection( HWND hwnd, LPTRANSLATEPROC lfpTranslate )
{
   HWND hwndEdit;
   HEDIT hedit;

   /* Make sure there is a window with the
    * focus.
    */
   if( ( hwndEdit = GetFocus() ) != NULL )
      if( ( hedit = Editmap_Open( hwndEdit ) ) != ETYP_NOTEDIT )
         {
         BEC_SELECTION bsel;

         /* Get the selection. If no selection, can't do
          * anything!
          */
         Editmap_GetSelection( hedit, hwndEdit, &bsel );
         if( bsel.lo != bsel.hi )
            {
            HPSTR hpText;
            SELRANGE cbEdit;
            SELRANGE cbSel;

            INITIALISE_PTR(hpText);
            cbEdit = Editmap_GetTextLength( hedit, hwndEdit );
            cbSel =  bsel.hi - bsel.lo;
            if( fNewMemory32( &hpText, cbEdit + 1 ) )
               {
               register SELRANGE i;

               /* Get the selected text into an edit buffer and
                * NULL terminate it.
                */
               Editmap_GetText( hedit, hwndEdit, cbEdit + 1, hpText );
               if( 0 < bsel.lo )
                  hmemcpy( hpText, hpText + bsel.lo, cbSel + 1 );
               hpText[ cbSel ] = '\0';

               /* Now do ROT13 decoding on the selection.
                */
               for( i = 0; i < cbSel; ++i )
                  hpText[ i ] = lfpTranslate( hpText[ i ] );

               /* Replace the selection.
                */
               Editmap_ReplaceSel( hedit, hwndEdit, hpText );
               Editmap_SetSel( hedit, hwndEdit, &bsel );
               FreeMemory32( &hpText );
               }
            }
         }
}

/* This function performs Rot13 on the specified character.
 */
int CDECL Rot13Function( int ch )
{
   if( isupper( ch ) )
      ch = ( ( ( ch - 'A' ) + 13 ) % 26 ) + 'A';
   else if( islower( ch ) )
      ch = ( ( ( ch - 'a' ) + 13 ) % 26 ) + 'a';
   return( ch );
}

/* This function handles the Insert File command.
 */
void FASTCALL CmdInsertFile( HWND hwnd )
{
   HWND hwndEdit;
   char szClsName[ 17 ];
   static char strFilters[] = "Documents (*.doc;*.txt)\0*.doc;*.txt\0Log files (*.log)\0*.log\0All Files (*.*)\0*.*\0\0";

   hwndEdit = GetFocus();
   GetClassName( hwndEdit, szClsName, 16 );
// if( _strcmpi( szClsName, "edit" ) == 0 )
   if( _strcmpi( szClsName, "edit" ) == 0 || _strcmpi( szClsName, "amxctl_editplus" ) == 0)
      if( GetWindowStyle( hwndEdit ) & ES_MULTILINE )
         {
         char sz[ _MAX_PATH ];

         sz[ 0 ] = '\0';
         if( CommonGetOpenFilename( hwnd, sz, "Insert File", strFilters, "CachedInsertFileDir" ) )
            {
            HNDFILE fh;
      
            SetFocus( hwndEdit );
            ExtractFilename( sz, sz );
            if( ( fh = Amfile_Open( sz, AOF_READ ) ) == HNDFILE_ERROR )
               FileOpenError( hwnd, sz, FALSE, FALSE );
            else
               {
               LPSTR lpText;
               LONG lSize;

               INITIALISE_PTR(lpText);
               lSize = Amfile_GetFileSize( fh );
               if( 0L == lSize )
                  {
                  wsprintf( lpTmpBuf, GS(IDS_STR83), (LPSTR)sz );
                  fMessageBox( hwndEdit, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
                  }
               else if( lSize > 65535L )
                  fMessageBox( hwnd, 0, GS(IDS_STR77), MB_OK|MB_ICONEXCLAMATION);
               else if( !fNewMemory( &lpText, (WORD)lSize + 1 ) )
                  {
                  wsprintf( lpTmpBuf, GS(IDS_STR80), (LPSTR)sz );
                  fMessageBox( hwndEdit, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
                  }
               else
                  {
                  if( Amfile_Read( fh, lpText, (int)lSize ) != (UINT)lSize )
                     DiskReadError( hwnd, sz, FALSE, FALSE );
                  else
                     {
                     DWORD dwSel;

                     /* Strip off EOF from end if found.
                      */
                     if( lpText[ (int)lSize - 1 ] == 26 )
                        --lSize;
                     lpText[ (int)lSize ] = '\0';

                     /* Strip out any NULLs.
                      */
                     StripBinaryCharacter( lpText, (int)lSize, '\0' );

                     /* Make sure the line delimiters are in PC format.
                      */
                     lpText = ExpandUnixTextToPCFormat( lpText );

                     /* Now insert the text at the cursor position.
                      */
                     //SetWindowRedraw( hwndEdit, FALSE ); // 20060302 - 2.56.2049.09
                     dwSel = Edit_GetSel( hwndEdit );
                     Edit_ReplaceSel( hwndEdit, lpText );
                     Edit_SetSel( hwndEdit, LOWORD( dwSel ), HIWORD( dwSel ) );
                     Edit_ScrollCaret( hwndEdit );
                     //SetWindowRedraw( hwndEdit, TRUE ); // 20060302 - 2.56.2049.09
                     }
                  FreeMemory( &lpText );
                  }
               Amfile_Close( fh );
               }
            }
         else
            SetFocus( hwndEdit );
         }
}

/* This function returns whether or not the clipboard has any
 * valid data.
 */
BOOL FASTCALL HasClipboardData( void )
{
   BOOL status;

   status = FALSE;
   if( OpenClipboard( hwndFrame ) )
      {
      int wFmt;

      wFmt = 0;
      while( 0 != ( wFmt = EnumClipboardFormats( wFmt ) ) )
         if( wFmt == CF_TEXT )
            {
            status = TRUE;
            break;
            }
      CloseClipboard();
      }
   return( status );
}

/* This function forwards a command.
 */
void FASTCALL ForwardCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   WNDPROC lpMDIWndProc;

   lpMDIWndProc = (WNDPROC)GetWindowLong( hwnd, GWL_WNDPROC );
   if( lpMDIWndProc != NULL )
      CallWindowProc( lpMDIWndProc, hwnd, WM_COMMAND, MAKEWPARAM((UINT)id,(UINT)codeNotify), (LPARAM)hwndCtl );
}

/* This function acts as an intercept for the frame window
 * messages being forwarded via the FORWARD_WM_MSG macros. Since the
 * macros all assume DefWindowProc or DefDlgProc and DefFrameProc
 * requires the MDI client window handle, it doesn't work properly. So
 * this function helps to resolve this.
 */
LRESULT FASTCALL DefFrameWindowProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   return( DefFrameProc( hwnd, hwndMDIClient, message, wParam, lParam ) );
}

/* This function sets the widths of the various parts of the status
 * bar as the main window is resized.
 */
void FASTCALL UpdateStatusBarWidths( void )
{
   if( hwndStatus && hwndFrame && IsWindowVisible( hwndFrame ) )
      {
      int aOldParts[ IDMSB_MAX ];
      int aNewParts[ IDMSB_MAX ];
      int cOldParts;
      int cPart;

      ComputeStatusBarWidth();
      cOldParts = (int)SendMessage( hwndStatus, SB_GETPARTS, IDMSB_MAX, (LPARAM)(LPSTR)&aOldParts );

      /* Set the main text area.
       */
      cPart = 0;
      ASSERT( cPart < IDMSB_MAX );
      aNewParts[ idmsbText = cPart++ ] = -1;

      /* The following are no longer used in Ameol, but
       * are retained for now.
       */
      ASSERT( cPart < IDMSB_MAX )
      aNewParts[ idmsbRow = cPart++ ] = cxLine + 2;
      ASSERT( cPart < IDMSB_MAX )
      aNewParts[ idmsbColumn = cPart++ ] = cxCol + 2;
      aNewParts[ idmsbOutbasket = cPart++ ] = 30;

      /* Only show clock if asked to.
       */
      if( fShowClock )
         {
         ASSERT( cPart < IDMSB_MAX );
         aNewParts[ idmsbClock = cPart++ ] = cxClock + 2;
         }
      if( cOldParts != cPart || memcmp( aOldParts, aNewParts, cPart * sizeof(int) ) )
         {
         SendMessage( hwndStatus, SB_SETPARTS, cPart, (LPARAM)(LPSTR)&aNewParts );
         SendMessage( hwndStatus, SB_SETTEXT, idmsbOutbasket|SBT_OWNERDRAW, 0L );
         }
      }
}

/* This function computes the widths of each part of the status
 * bar depending on the active status bar font.
 */
void FASTCALL ComputeStatusBarWidth( void )
{
   char sz[ 40 ];
   HFONT hStatusFont;
   HFONT hOldFont;
   SIZE size;
   HDC hdc;

   hdc = GetDC( hwndStatus );
   hStatusFont = GetWindowFont( hwndStatus );
   hOldFont = SelectFont( hdc, hStatusFont );

   /* Compute the width of the line number field.
    */
   GetTextExtentPoint( hdc, "000000", 6, &size );
   cxLine = size.cx + 8;

   /* Compute the width of the column number field.
    */
   GetTextExtentPoint( hdc, "000", 3, &size );
   cxCol = size.cx + 8;

   /* Compute the width of the status field using the
    * longest text that can be selected there.
    */
   GetTextExtentPoint( hdc, "OFFLINE", 7, &size );
   cxMode = size.cx + 4;

   /* Compute the width of the clock field.
    */
   FormatTimeString( sz );
   GetTextExtentPoint( hdc, sz, strlen( sz ), &size );
   cxClock = size.cx + 8;

   /* All done.
    */
   SelectFont( hdc, hOldFont );
   ReleaseDC( hwndStatus, hdc );
}

LPSTR FASTCALL FormatTimeString( LPSTR lpsz )
{
   AM_TIME aTime;
   
   Amdate_GetCurrentTime( &aTime );
   return( Amdate_FormatTime( &aTime, lpsz ) );
}

/* This code is shared between the WM_CLOSE and WM_ENDSESSION handler. It
 * handles the closing down of Palantir.
 */
void FASTCALL MainWnd_CommonCloseCode( HWND hwnd, BOOL fPrompt )
{
   /* If parsing, warn user.
    */
   if( fParsing )
      {
      EnsureMainWindowOpen();
      if( fMessageBox( hwnd, 0, GS(IDS_STR826), MB_YESNO|MB_ICONINFORMATION ) == IDNO )
         return;
      CIXImportParseCompleted();
      }

   fMaxNewsgroups = Amuser_WritePPLong( szSettings, "MaxNewsgroups", fMaxNewsgroups );

   /* If out-basket not empty and warning enabled, warn
    * user.
    */
   if( fPrompt && fWarnOnExit && Amob_GetCount(OBF_UNFLAGGED) )
      {
      EnsureMainWindowOpen();
      if( fMessageBox( hwnd, 0, GS(IDS_STR194), MB_YESNO|MB_ICONINFORMATION ) == IDNO )
         return;
      }

   /* Okay, now do the actual close.
    */
   fInitiateQuit = FALSE;
   fQuitting = TRUE;
   if( hwndToolBarOptions )
      SendMessage( hwndToolBarOptions, WM_CLOSE, 0, 0L );
   if( hwndStartWiz )
      SendMessage( hwndStartWiz, WM_CLOSE, 0, 0L );
   if( hwndStartWizGuide )
      SendMessage( hwndStartWizGuide, WM_CLOSE, 0, 0L );
   if( hwndStartWizProf )
      SendMessage( hwndStartWizProf, WM_CLOSE, 0, 0L );
   if( CloseOnlineWindows( hwnd, fPrompt ) && CloseAllTerminals( hwnd, fPrompt ) )
      {
      if( iAddonsUsingConnection > 0 )
      {
         EndOnlineBlink();
         DestroyBlinkWindow();
         iAddonsUsingConnection = 0;
      }

      // Exit WinSparkle
      win_sparkle_cleanup();

      Amuser_WriteWindowState( "Main", hwnd );
      SaveDefaultWindows();
      SendAllWindows( WM_CLOSE, 0, 0L );
      if( fQuitting )
         {

         /* Delete any temp files we created when viewing in browser
          */
         register int n;
         FINDDATA ft;
         HFIND r;
         char szTempHTMLFileName[ _MAX_PATH ];

         GetTempPath( _MAX_PATH, lpPathBuf );
         wsprintf( szTempHTMLFileName, "%s~A2*.html", lpPathBuf );
         for( n = r = Amuser_FindFirst( szTempHTMLFileName, _A_NORMAL, &ft ); n != -1; n = Amuser_FindNext( r, &ft ) )
         {
            char szTempDel[ _MAX_PATH ];
            wsprintf( szTempDel, "%s%s", lpPathBuf, ft.name );
            Amfile_Delete( szTempDel );
         }
         Amuser_FindClose( r );

         wsprintf( szTempHTMLFileName, "%s~A2*.tmp", lpPathBuf );
         for( n = r = Amuser_FindFirst( szTempHTMLFileName, _A_NORMAL, &ft ); n != -1; n = Amuser_FindNext( r, &ft ) )
         {
            char szTempDel[ _MAX_PATH ];
            wsprintf( szTempDel, "%s%s", lpPathBuf, ft.name );
            Amfile_Delete( szTempDel );
         }
         Amuser_FindClose( r );

         Amuser_WritePPInt( szSettings, "AutoCheck", FALSE );
         Am_PlaySound( "Close" );

#ifdef USEMUTEX // 2.55.2031
         ReleaseMutex( InstanceMutexHandle );
#endif USEMUTEX // 2.55.2031
         /* Restore MDI client's old window proc.
          */
         if( NULL != lpfMdiClientProc )
            SubclassWindow( hwndMDIClient, lpfMdiClientProc );
         ShowWindow( hwnd, SW_HIDE );
         ExitAmeol( hwnd, TRUE );
         DestroyWindow( hwnd );
         }
      }
}

void FASTCALL SetSweepTimer( void )
{
   if( wSweepRate == -1 ) {
      DWORD dwFree;

      /* Determine the sweep frequency based on the amount of free RAM in
       * the computer. For systems with more than 8Mb free RAM, disable
       * the sweeper.
       */
      dwFree = GetFreeSpace( 0 ) / 1024;
      if( dwFree > 8 )        wSweepRate = 60000;
      else if( dwFree > 4 )   wSweepRate = 60000;
      else                    wSweepRate = 30000;
      }
   if( wSweepRate > 0 )
      if( 0 == SetTimer( hwndFrame, TID_SWEEPER, wSweepRate, NULL ) )
         Amevent_AddEventLogItem( ETYP_SEVERE|ETYP_INITTERM, GS(IDS_STR12) );
}

void FASTCALL KillSweepTimer( void )
{
   if( wSweepRate > 0 )
      KillTimer( hwndFrame, TID_SWEEPER );
}

/* Show or hide the toolbar.
 */
void FASTCALL CmdShowToolbar( BOOL fShow )
{
   if( fShow )
      {
      if( !IsWindowVisible( hwndToolBar ) )
         {
         fShowToolBar = TRUE;
         UpdateAppSize();
         ShowWindow( hwndToolBar, SW_SHOW );
         }
      }
   else
      {
      if( IsWindowVisible( hwndToolBar ) )
         {
         fShowToolBar = FALSE;
         ShowWindow( hwndToolBar, SW_HIDE );
         UpdateAppSize();
         }
      }
   Amuser_WritePPInt( szSettings, "ToolBar", fShowToolBar );
}

/* Show or hide the status bar.
 */
void FASTCALL CmdShowStatusBar( BOOL fShow )
{
   if( fShow )
      {
      if( !IsWindowVisible( hwndStatus ) )
         {
         fShowStatusBar = TRUE;
         UpdateAppSize();
         ShowWindow( hwndStatus, SW_SHOW );
         }
      }
   else
      {
      if( IsWindowVisible( hwndStatus ) )
         {
         fShowStatusBar = FALSE;
         ShowWindow( hwndStatus, SW_HIDE );
         UpdateAppSize();
         }
      }
   Amuser_WritePPInt( szSettings, "StatusBar", fShowStatusBar );
}

/* This function writes text onto the status bar.
 */
void CDECL StatusText( LPSTR lpszText, ... )
{
   va_list va_arg;

   va_start( va_arg, lpszText );
   wvsprintf( lpTmpBuf, lpszText, va_arg );
   va_end( va_arg );
   SendMessage( hwndStatus, SB_SETTEXT, idmsbOnlineText, (LPARAM)(LPSTR)lpTmpBuf );
}

/* Show the total number of unread messages
 * on the status bar.
 */
void EXPORT FASTCALL ShowUnreadMessageCount( void )
{
   if( hwndStatus && !fParsing )
      {
      DWORD dwTotalUnread;
      char sz[ 100 ];

      dwTotalUnread = Amdb_GetTotalUnread();
      if( 1 == dwTotalUnread )
         strcpy( sz, GS(IDS_STR1069) );
      else if( dwTotalUnread )
         wsprintf( sz, GS(IDS_STR76), dwTotalUnread );
      else
         strcpy( sz, GS(IDS_STR574) );
      OfflineStatusText( sz );
      }
}

/* This function writes text onto the status bar if and only
 * if Palantir is offline. Otherwise the message is ignored.
 */
void CDECL OfflineStatusText( LPSTR lpszText, ... )
{
   va_list va_arg;

   if( hwndStatus && !fParsing )
      {
      if( NULL == hwndBlink && fBlinking )
         return;
      va_start( va_arg, lpszText );
      wvsprintf( lpTmpBuf, lpszText, va_arg );
      va_end( va_arg );
      SendMessage( hwndStatus, SB_SETTEXT, idmsbText, (LPARAM)(LPSTR)lpTmpBuf );
      }
}

/* This function resizes the application MDI window, toolbar and
 * status bar.
 */
void FASTCALL UpdateAppSize( void )
{
   RECT rc;

   GetClientRect( hwndFrame, &rc );
   SetMainWindowSizes( hwndFrame, rc.right - rc.left, rc.bottom - rc.top );
}

/* This function resizes the application MDI window, toolbar and
 * status bar.
 */
void FASTCALL SetMainWindowSizes( HWND hwnd, int cx, int cy )
{
   HDWP hdwp;
   RECT rc[ 3 ];

   /* Get the control window sizes.
    */
   GetWorkspaceRects( hwnd, (LPRECT)&rc );
   cx = rc[ 2 ].right;
   cy = rc[ 2 ].bottom;

   /* Compute the MDI client window size.
    */
   hdwp = BeginDeferWindowPos( 2 );
   switch( nToolbarPos )
      {
      case IDM_TOOLBAR_LEFT:
      case IDM_TOOLBAR_RIGHT:
         DeferWindowPos( hdwp, hwndMDIClient, NULL, rc[ 2 ].left, rc[ 2 ].top, rc[ 2 ].right, rc[ 2 ].bottom, SWP_NOZORDER|SWP_NOACTIVATE );
         DeferWindowPos( hdwp, hwndToolBar, NULL, rc[ 1 ].left, rc[ 1 ].top, 0, 0, SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE );
         break;

      case IDM_TOOLBAR_TOP:
      case IDM_TOOLBAR_BOTTOM:
         DeferWindowPos( hdwp, hwndToolBar, NULL, rc[ 1 ].left, rc[ 1 ].top, 0, 0, SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE );
         DeferWindowPos( hdwp, hwndMDIClient, NULL, rc[ 2 ].left, rc[ 2 ].top, rc[ 2 ].right, rc[ 2 ].bottom, SWP_NOZORDER|SWP_NOACTIVATE );
         break;
      }
   EndDeferWindowPos( hdwp );

   /* If the toolbar bar is visible, send it a WM_SIZE message and
    * it will move to the correct position.
    */
   if( fShowToolBar )
      SendMessage( hwndToolBar, WM_SIZE, 0, MAKELONG( cx, cy ) );

   /* If the status bar is visible, send it a WM_SIZE message and
    * it will move to the correct position.
    */
   if( fShowStatusBar )
      {
      SendMessage( hwndStatus, WM_SIZE, 0, 0L );
      SendMessage( hwndStatus, WM_MOVE, 0, 0L );
      UpdateStatusBarWidths();
      }
   if( !IsIconic( hwnd ) )
      {
      Amuser_WriteWindowState( "Main", hwnd );
      fLastStateWasMaximised = IsMaximized( hwnd );
      }
}

/* This function returns the rectangles of various parts of the
 * Ameol workspace:
 *
 *  lprc[0] = rectangle of status bar offset from hwnd
 *  lprc[1] = rectangle of toolbar bar offset from hwnd
 *  lprc[2] = rectangle of MDI client offset from hwnd
 */
void FASTCALL GetWorkspaceRects( HWND hwnd, LPRECT lprc )
{
   int cyStatusWnd;
   int cyToolbarWnd;
   int cxToolbarWnd;
   RECT rcToolbarWnd;
   RECT rcStatusWnd;
   RECT rcFrameWnd;
   int cx, cy;

   /* First get the frame window dimensions.
    */
   GetClientRect( hwnd, &rcFrameWnd );
   cx = rcFrameWnd.right - rcFrameWnd.left;
   cy = rcFrameWnd.bottom - rcFrameWnd.top;

   /* Get the status window dimensions.
    */
   if( NULL == hwndStatus || !fShowStatusBar )
      SetRectEmpty( &rcStatusWnd );
   else
      GetClientRect( hwndStatus, &rcStatusWnd );

   /* Get the toolbar window dimensions.
    */
   if( NULL == hwndToolBar || !fShowToolBar )
      SetRectEmpty( &rcToolbarWnd );
   else
      {
      DWORD dwSize;

      dwSize = SendMessage( hwndToolBar, TB_AUTOSIZE, 0, MAKELONG( cx, cy ) );
      SetRect( &rcToolbarWnd, 0, 0, LOWORD(dwSize), HIWORD(dwSize) );
      }

   /* Compute the MDI client rectangle.
    */
   cyStatusWnd = rcStatusWnd.bottom - rcStatusWnd.top;
   cyToolbarWnd = rcToolbarWnd.bottom - rcToolbarWnd.top;
   cxToolbarWnd = rcToolbarWnd.right - rcToolbarWnd.left;
   switch( nToolbarPos )
      {
      case IDM_TOOLBAR_LEFT:
         SetRect( &lprc[ 0 ], cxToolbarWnd, cy - cyStatusWnd, cx - cxToolbarWnd, cyStatusWnd );
         SetRect( &lprc[ 1 ], 0, 0, cxToolbarWnd, cyToolbarWnd - cyStatusWnd );
         SetRect( &lprc[ 2 ], cxToolbarWnd, 0, cx - cxToolbarWnd, cy - cyStatusWnd );
         break;

      case IDM_TOOLBAR_RIGHT:
         SetRect( &lprc[ 0 ], 0, cy - cyStatusWnd, cx - cxToolbarWnd, cyStatusWnd );
         SetRect( &lprc[ 1 ], cx - cxToolbarWnd, 0, cxToolbarWnd, cyToolbarWnd - cyStatusWnd );
         SetRect( &lprc[ 2 ], 0, 0, cx - cxToolbarWnd, cy - cyStatusWnd );
         break;

      case IDM_TOOLBAR_TOP:
         SetRect( &lprc[ 0 ], 0, cy - cyStatusWnd, cx, cyStatusWnd );
         SetRect( &lprc[ 1 ], 0, 0, cxToolbarWnd, cyToolbarWnd );
         SetRect( &lprc[ 2 ], 0, cyToolbarWnd, cx, cy - ( cyStatusWnd + cyToolbarWnd ) );
         break;

      case IDM_TOOLBAR_BOTTOM:
         SetRect( &lprc[ 0 ], 0, cy - cyStatusWnd, cx, cyStatusWnd );
         SetRect( &lprc[ 1 ], 0, cy - ( cyStatusWnd + cyToolbarWnd ), cxToolbarWnd, cyToolbarWnd );
         SetRect( &lprc[ 2 ], 0, 0, cx, cy - ( cyStatusWnd + cyToolbarWnd ) );
         break;
      }
}

/* This function exits Palantir.
 */
void FASTCALL ExitAmeol( HWND hwnd, BOOL fSaveDb )
{
   switch( wInitState )
      {
      case 11:
      case 10:
         /* Save the database.
          */
         wsprintf( lpTmpBuf, GS(IDS_STR536), (LPSTR)GetActiveUsername() );
         Amevent_AddEventLogItem( ETYP_MESSAGE|ETYP_INITTERM, lpTmpBuf );
         if( fSaveDb )
            {
            register int r;
         
            do {
               if( Amdb_CommitDatabase( TRUE ) == AMDBERR_NOERROR )
                  break;
               r = fMessageBox( hwnd, 0, GS(IDS_STR84), MB_RETRYCANCEL|MB_ICONEXCLAMATION );
               }
            while( r == IDRETRY );
            }

         /* Save other stuff.
          */
         hwndActive = NULL;
         TermSpelling();
         DeleteCheckMenuItemGroupBitmap();
         HtmlHelp( NULL, NULL, HH_CLOSE_ALL, 0L );
         if( !fNoAddons )
            UnloadAllAddons();

         /* Close all open databases.
          */
         Amdb_CloseDatabase();
         Amob_SaveOutBasket( TRUE );

      case 9:
         if( fUseInternet)
            UnloadCIXIP( 0 );
         if( fUseCIX )
            UnloadCIX( 0 );
         UnloadLocal( 0 );

      case 8:
         CTree_DeleteAllCommands();
         CTree_DeleteAllCategories();
         
         DeleteAmeolFonts();
         DeleteAmeolSystemFonts();

         Amevent_FreeEventLog(); // 20060325 - 2.56.2049.20
         UnregisterAllEvents(); // 20060325 - 2.56.2049.20

      case 7:
         if( NULL != hBmpbackground )
            {
            DeleteBitmap( hBmpbackground );
            DeletePalette( hPalbackground );
            }

      case 6:
         PageSetupTerm();

      case 5:
      case 4:
      case 3:
         if( !hwnd )
            {
            DestroyMenu( hMainMenu );
            if( NULL != hFormsMenu )
               DestroyMenu( hFormsMenu );
            DestroyMenu( hPopupMenus );
            }

      case 2:
         CloseUsersList();

      case 1:
      case 0:
         UninstallAllFileTransferProtocols();
         if( NULL != hToolbarFont )
            DeleteFont( hToolbarFont );
         if( NULL != lpTmpBuf )
            FreeMemory( &lpTmpBuf );
         if( NULL != lpPathBuf )
            FreeMemory( &lpPathBuf );
         if( NULL != pszModulePath )
            FreeMemory( &pszModulePath );
         if( NULL != pszAmeolDir )
            FreeMemory( &pszAmeolDir );
         if( NULL != pszAppDataDir )       // VistaAlert
            FreeMemory( &pszAppDataDir );
         
         Adm_Exit( hInst );
         ASSERT( Pal_GetOpenSocketCount() == 0 );
         break;
      }

   if (hRegLib >= (HINSTANCE)32)
      FreeLibrary( hRegLib ); // !!SM!! 2041

   if (hSciLexer >= (HINSTANCE)32)
      FreeLibrary( hSciLexer ); // !!SM!! 2041

   if (hWebLib >= (HINSTANCE)32)
      FreeLibrary( hWebLib ); // !!SM!! 2053

}

/* This function returns a list of Ameol windows.
 */
void EXPORT WINAPI Ameol2_GetWindows( HWND FAR * lphwnd )
{
   *lphwnd++ = hwndFrame;
   *lphwnd++ = hwndMsg;
   *lphwnd++ = hwndOutBasket;
   *lphwnd++ = hwndMDIClient;
   *lphwnd++ = hwndInBasket;
   *lphwnd++ = hwndStatus;
   *lphwnd = hwndToolBar;
}

/* This function returns the current CIX user nickname.
 */
int EXPORT WINAPI Ameol2_GetCixNickname( char * pszNickname )
{
   strcpy( pszNickname, szCIXNickname );
   return( strlen( szCIXNickname ) );
}

/* This function handles the WM_ERASEBKGND message.
 */
BOOL FASTCALL Common_OnEraseBkgnd( HWND hwnd, HDC hdc )
{
   HBRUSH hbr;
   RECT rc;

   GetClipBox( hdc, &rc );
   hbr = CreateSolidBrush( GetSysColor( COLOR_BTNFACE ) );
   FillRect( hdc, &rc, hbr );
   DeleteBrush( hbr );
   return( TRUE );
}

/* This function handles the WM_CHANGEFONT message for edit
 * windows.
 */
void FASTCALL Common_ChangeFont( HWND hwnd, int type )
{
   if( type == FONTYP_EDIT || 0xFFFF == (WPARAM)type )
   {
#ifdef USEBIGEDIT
      SetWindowFont( GetDlgItem( hwnd, IDD_EDIT ), hEditFont, TRUE );
#else USEBIGEDIT
      SetEditorDefaults(GetDlgItem( hwnd, IDD_EDIT ), FALSE, lfEditFont, fWordWrap, fHotLinks , fMsgStyles, crEditText, crEditWnd );
#endif USEBIGEDIT
   }
}

/* This function detects whether or not a network is installed on
 * this machine.
 */
BOOL FASTCALL QueryHasNetwork( void )
{
   return( TRUE );
}

/* This function handles the Open Password dialog box.
 */
BOOL EXPORT CALLBACK OpenPassword( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = OpenPassword_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the OpenPassword
 *  dialog.
 */
LRESULT FASTCALL OpenPassword_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, OpenPassword_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, OpenPassword_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsOPENPASSWORD );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL OpenPassword_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndEdit;

   /* Test for no-password.
    */
   if( BlankPassword( GetActivePassword() ) )
      {
      EndDialog( hwnd, TRUE );
      return( FALSE );
      }

   /* Initialise the input field.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
   Edit_LimitText( hwndEdit, LEN_LOGINPASSWORD - 1 );
   EnableControl( hwnd, IDOK, FALSE );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL OpenPassword_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EDIT: {
         HWND hwndEdit;

         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndEdit ) > 0 );
         break;
         }

      case IDOK: {
         char szPassword[ LEN_LOGINPASSWORD ];

         memset( szPassword, 0, LEN_LOGINPASSWORD );
         Edit_GetText( GetDlgItem( hwnd, IDD_EDIT ), szPassword, LEN_LOGINPASSWORD );
         Amuser_Encrypt( szPassword, rgEncodeKey );
         if( memcmp( szPassword, GetActivePassword(), LEN_LOGINPASSWORD ) )
            {
            HighlightField( hwnd, IDD_EDIT );
            MessageBox( NULL, GS(IDS_STR526), NULL, MB_OK|MB_ICONEXCLAMATION );
            break;
            }
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* This function handles the Licence Agreement dialog.
 */
BOOL EXPORT CALLBACK LicenceProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, LicenceProc_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the LicenceProc
 * dialog.
 */
LRESULT FASTCALL LicenceProc_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, LicenceProc_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, LicenceProc_OnCommand );

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}


/*******************************************************************/
DWORD CALLBACK EditStreamCallback(
  DWORD dwCookie, // application-defined value
  LPBYTE pbBuff,  // pointer to a buffer
  LONG cb,        // number of bytes to read or write
  LONG *pcb       // pointer to number of bytes transferred
)
{
   ReadFile((HANDLE)dwCookie, (void *)pbBuff, cb, pcb, NULL);
   if(!IsBadWritePtr(&pbBuff[(*pcb)-1],1))
      pbBuff[(*pcb)-1] = '\x0';
   if(*pcb > 0)
      return 0;
   else
      return 1;
}
/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL LicenceProc_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   DWORD dwFileSize;
   DWORD dwRead;
   HWND hwndEdit;
   LPSTR lpText;
   HNDFILE fh;
   EDITSTREAM lpStream;

   INITIALISE_PTR(lpText);

   /* Open the licence agreement file.
    */
   if ( IsWindow(GetDlgItem( hwnd, IDD_EDIT_RTF ) ) )
   {
      hwndEdit = GetDlgItem( hwnd, IDD_EDIT_RTF ); /*!!SM!!*/
      wsprintf( lpPathBuf, "%s%s", pszAmeolDir, szLicenceRTF );
   }
   else
   {
      hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
      wsprintf( lpPathBuf, "%s%s", pszAmeolDir, szLicence );
   }

   fh = Amfile_Open( lpPathBuf, AOF_READ );
   if( HNDFILE_ERROR == fh )
      {
      FileOpenError( hwnd, (char *)lpPathBuf, FALSE, FALSE );
      EndDialog( hwnd, FALSE );
      return( TRUE );
      }

   if ( IsWindow(GetDlgItem( hwnd, IDD_EDIT_RTF ) ) )
   {
      SendMessage(hwndEdit, WM_SETTEXT, 0, (LPARAM)(LPSTR)"");
      SendMessage(hwndEdit, EM_SETTEXTMODE, TM_RICHTEXT, 0);
      lpStream.pfnCallback = EditStreamCallback;
      lpStream.dwError = 0;
      lpStream.dwCookie = (DWORD)(HANDLE)fh;
      SendMessage(hwndEdit, EM_STREAMIN, (WPARAM)(UINT)SF_RTF|SFF_PLAINRTF, (LPARAM)(EDITSTREAM FAR *)&lpStream);
      Amfile_Close( fh );
   }
   else
   {
      /* Allocate a block of memory for it and read it in.
       */
      dwFileSize = Amfile_GetFileSize( fh );
      ASSERT( dwFileSize < 0x0000F000 );
      if( !fNewMemory( &lpText, (WORD)dwFileSize + 1 ) )
         {
         OutOfMemoryError( hwnd, FALSE, FALSE );
         EndDialog( hwnd, FALSE );
         return( TRUE );
         }
      dwRead = Amfile_Read32( fh, lpText, (WORD)dwFileSize );
      if( dwRead != (WORD)dwFileSize )
         {
         DiskReadError( hwnd, (char *)lpPathBuf, FALSE, FALSE );
         EndDialog( hwnd, FALSE );
         return( TRUE );
         }
      Amfile_Close( fh );
      lpText[ dwRead ] = '\0';

      /* Fill the edit control.
       */
      Edit_SetText( hwndEdit, lpText );
      FreeMemory( &lpText );
   }
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL LicenceProc_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_ACCEPT:
      case IDD_NO_ACCEPT:
         EndDialog( hwnd, id == IDD_ACCEPT );
         break;
      }
}

/* This function tests whether the specified domain name is local
 * to CIX conferencing.
 */
BOOL FASTCALL IsCompulinkDomain( char * pszDomainName )
{
   if( _stricmp( pszDomainName, szCompulink ) == 0 )
      return( TRUE );
   return( _stricmp( pszDomainName, szCixCoUk ) == 0 );
}

/* This function locates the default browser under Win32 by looking
 * in the registry. Under Win16, it uses the default.
 */
void FASTCALL FindDefaultBrowser( void )
{
   HKEY hkResult;

   /* First look for the class entry associated with the .html
    * extension.
    */
   if( RegOpenKeyEx( HKEY_CLASSES_ROOT, ".html", 0L, KEY_READ, &hkResult ) == ERROR_SUCCESS )
      {
      char szValueName[ 10 ];
      char szClassKey[ 64 ];
      DWORD cbValueName;
      DWORD dwType;
      DWORD dwszInt;

      /* Now locate and open that class entry.
       */
      dwszInt = sizeof(szClassKey);
      cbValueName = sizeof(szValueName);
      RegEnumValue( hkResult, 0, szValueName, &cbValueName, NULL, NULL, 0, NULL );
      if( RegQueryValueEx( hkResult, szValueName, 0L, &dwType, szClassKey, &dwszInt ) == ERROR_SUCCESS )
         {
         HKEY hkResult2;

         /* In that class entry, we want the shell open command entry.
          */
         wsprintf( lpTmpBuf, "%s\\shell\\open\\command", szClassKey );
         if( RegOpenKeyEx( HKEY_CLASSES_ROOT, lpTmpBuf, 0L, KEY_READ, &hkResult2 ) == ERROR_SUCCESS )
            {
            dwszInt = LEN_TEMPBUF;
            RegEnumValue( hkResult2, 0, szValueName, &cbValueName, NULL, NULL, 0, NULL );
            if( RegQueryValueEx( hkResult2, szValueName, 0L, &dwType, lpTmpBuf, &dwszInt ) == ERROR_SUCCESS )
               {
               ExtractFilename( szBrowser, lpTmpBuf);
               QuotifyFilename( szBrowser );
               }
            RegCloseKey( hkResult2 );
            }
         }
      RegCloseKey( hkResult );
      }
}

/* Given a folder handle, this function sets two flags which specify whether
 * the folder contains any news or mail topics.
 */
void FASTCALL GetSupersetTopicTypes( LPVOID pFolder, BOOL * pfMail, BOOL * pfNews )
{
   *pfNews = FALSE;
   *pfMail = FALSE;
   if( Amdb_IsTopicPtr( pFolder ) )
      {
      if( Amdb_GetTopicType( (PTL)pFolder ) == FTYPE_NEWS )
         *pfNews = TRUE;
      else if( Amdb_GetTopicType( (PTL)pFolder ) == FTYPE_MAIL )
         *pfMail = TRUE;
      }
   else if( Amdb_IsFolderPtr( pFolder ) )
      {
      PTL ptl;

      for( ptl = Amdb_GetFirstTopic( (PCL)pFolder ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
         if( Amdb_GetTopicType( ptl ) == FTYPE_NEWS )
            *pfNews = TRUE;
         else if( Amdb_GetTopicType( ptl ) == FTYPE_MAIL )
            *pfMail = TRUE;
      }
   else if( Amdb_IsCategoryPtr( pFolder ) )
      {
      PCL pcl;
      PTL ptl;

      for( pcl = Amdb_GetFirstFolder( (PCAT)pFolder ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
         for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
            if( Amdb_GetTopicType( ptl ) == FTYPE_NEWS )
               *pfNews = TRUE;
            else if( Amdb_GetTopicType( ptl ) == FTYPE_MAIL )
               *pfMail = TRUE;
      }
}

BOOL WINAPI IsAmeol2Loaded( LPSTR userName )
{

#ifdef USEMUTEX // 2.55.2031
   char UniqueName[ 80 ];

   strcpy( UniqueName, "A Unique Ameol String For - " );
   strcat( UniqueName, *userName ? userName : "Administrator" );

   InstanceMutexHandle = OpenMutex( MUTEX_ALL_ACCESS, FALSE, UniqueName );
   if( InstanceMutexHandle == 0 )
   {
      InstanceMutexHandle = CreateMutex( 0, FALSE, UniqueName );
      if( InstanceMutexHandle == 0 )
         return FALSE;
      else
      {
         if ( GetLastError() == ERROR_ALREADY_EXISTS ) // !!SM!! 2.55
         {
            ReleaseMutex(InstanceMutexHandle);
            return FALSE;
         }
         else
            return TRUE;
      }
   }
   else
      return FALSE;
#else USEMUTEX // 2.55.2031
   char UniqueName[ 1024 ];

   pszDataDir = ReadDirectoryPath( APDIR_DATA, "Data" );
   wsprintf(UniqueName, "%s\\%s.lck", pszDataDir, userName);
   fLockFile = CreateFile(UniqueName, GENERIC_READ|GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_DELETE_ON_CLOSE, NULL);

   return (fLockFile != INVALID_HANDLE_VALUE);
#endif USEMUTEX // 2.55.2031
}   

void FASTCALL CmdShowTips ( HWND hwnd )
{
   Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_ATIPS), CmdTipsDlg, 0L );
}

void FASTCALL CmdCIXSupport ( HWND hwnd )
{
   UINT uRetCode;
   LPSTR pszLink = "http://www.cixonline.com/support.asp";
   if( ( uRetCode = (UINT)ShellExecute( hwnd, NULL, pszLink, "", pszAmeolDir, SW_SHOW ) ) < 32 )
   {
      DisplayShellExecuteError( hwnd, pszLink, uRetCode );
   }
}

void FASTCALL CmdCheckForUpdates ( HWND hwnd )
{
#ifdef IS_BETA
   win_sparkle_set_appcast_url("http://ameol2beta.cixhosting.co.uk/beta/appcast.xml");
#else
   win_sparkle_set_appcast_url("http://ameol2beta.cixhosting.co.uk/release/appcast.xml");
#endif
   win_sparkle_init();
   win_sparkle_check_update_with_ui();
}

/* Handles the Tips dialog.
 */
BOOL EXPORT CALLBACK CmdTipsDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, CmdTipsDlg_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the Tips dialog.
 */
LRESULT FASTCALL CmdTipsDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, CmdTipsDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, CmdTipsDlg_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsTOTD );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}


/* This function handles the WM_COMMAND message. Currently only one control
 * on the dialog, the OK button, dispatches this message.
 */ 
void FASTCALL CmdTipsDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {

      case IDD_NOTIPS: {
      
         /* User says they don't want tips any more */
         
         fShowTips = IsDlgButtonChecked( hwnd, IDD_NOTIPS );
         Amuser_WritePPInt( szSettings, "ShowTips", fShowTips );
         break;
         }

      case IDD_ANOTHER:

         /* User wants another tip */

         SetTipText( hwnd );
         break;

      case IDCANCEL:
      case IDOK:

         EndDialog( hwnd, 0 );
         break;
      }
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL CmdTipsDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{

   char szTipText[ 511 ] = "";


   Amuser_GetLMString( szTips, "Tip0", "", szTipText, LEN_TEMPBUF );

   /* If no tips installed, read defaults.
    */
   if( strcmp( szTipText, "" ) == 0 )
   {
      HNDFILE fh;
      char szDefTips[] = "tips.def";
      if( ( fh = Amfile_Open( szDefTips, AOF_READ ) ) != HNDFILE_ERROR )
            {
            LPLBF hlbf;

            fInitTips = TRUE;
            if( hlbf = Amlbf_Open( fh, AOF_READ ) )
               {
               char sz[ 512 ];
               int i = 0;

               /* Read each line from the default tips file and add it to
                * the registry.
                */
               while( Amlbf_Read( hlbf, sz, sizeof(sz), NULL, NULL, &fIsAmeol2Scratchpad ) )
                  {
                  if( strlen( sz ) > 0 )
                  {
                     wsprintf( lpTmpBuf, "Tip%d", i );
                     Amuser_WriteLMString( szTips, lpTmpBuf, sz );
                     i++;
                  }
               _itoa( i, lpTmpBuf, 10 );
               Amuser_WriteLMString( szTips, "TotalTips", lpTmpBuf );
               }
               Amlbf_Close( hlbf );
               }
            }
   }

   

   fShowTips = Amuser_GetPPInt( szSettings, "ShowTips", FALSE );
   CheckDlgButton( hwnd, IDD_NOTIPS, fShowTips );

   /* Seed the random number generator with the time 
    */
   srand( (unsigned)time( NULL ) );

   SetTipText( hwnd );

   return( TRUE );
}

void FASTCALL SetTipText( HWND hwnd )
{
   int i;
   HWND hwndText;
   char szTipNumber[ 6 ];
   char szTipText[ 511 ] = "";

   hwndText = GetDlgItem( hwnd, IDD_TIPTEXT );

   /* Try 50 times to get a tip from the tip database. If we fail,
    * display a standard text
    */
   for( i = 0; ( i < 50 && strcmp( szTipText, "" ) == 0 ); i++ )
   {
   wsprintf( szTipNumber, "Tip%d", ( rand() % 50 ));
   Amuser_GetLMString( szTips, szTipNumber, "", szTipText, LEN_TEMPBUF );
   }
   if( strcmp( szTipText, "" ) == 0 || fInitTips )
   {
      wsprintf( szTipText, "The Tip of the Day contains useful hints on using Ameol. Click the \"Another Tip\" button to see a new random tip. Keep tips turned on until you are more familiar with Ameol." );
      fInitTips = FALSE;
   }
   SetWindowText( hwndText, szTipText );
   SetWindowFont( hwndText, hHelv8Font, FALSE );

}

LPSTR FASTCALL GetShortHeaderText( PTH pth )
{
   char * lpBufPtr;
   LPSTR lpszMailHdrs;
   LPSTR lpszPreText;

   lpszPreText = NULL;
   lpszMailHdrs = NULL;
   lpBufPtr = NULL;

   if( fNewMemory( &lpszMailHdrs, 2048 ) )
   {
      char * lpEnd;

      /* Avoids a GPF if all the short header settings get set to FALSE
       * (should never happen now, but...) 
       */
      if( !fShortHeadersSubject && !fShortHeadersFrom && !fShortHeadersTo && !fShortHeadersCC && !fShortHeadersDate )
         fShortHeadersFrom = TRUE;
      *lpszMailHdrs = '\0';
      lpEnd = lpszMailHdrs;

      /* Collect each short header field if it's flag is set
       *
       */
      if( fShortHeadersSubject )
      {
         Amdb_GetMailHeaderField( pth, "Subject", lpTmpBuf, 500, TRUE );
         if( !*lpTmpBuf )
         {
            MSGINFO msginfo;
            Amdb_GetMsgInfo( pth, &msginfo );
            if( *msginfo.szTitle )
               strcpy( lpTmpBuf, msginfo.szTitle );
         }
         if( *lpTmpBuf )
         {
            if( fMsgStyles )
               strcpy( lpszMailHdrs, "*Subject:* " );
            else
               strcpy( lpszMailHdrs, "Subject: " );
                
            b64decodePartial(lpTmpBuf);
            lpBufPtr = lpszMailHdrs + strlen(lpszMailHdrs);
            strcat( lpBufPtr, lpTmpBuf );
            strcat( lpBufPtr, "\r\n" );
         }
         lpEnd = lpszMailHdrs + strlen(lpszMailHdrs);
      }
      if( fShortHeadersFrom )
      {
         Amdb_GetMailHeaderField( pth, "From", lpTmpBuf, 500, TRUE );
         if( !*lpTmpBuf )
         {
            MSGINFO msginfo;
            Amdb_GetMsgInfo( pth, &msginfo );
            if( *msginfo.szAuthor )
               strcpy( lpTmpBuf, msginfo.szAuthor );
         }
         if( *lpTmpBuf )
         {
            if( fMsgStyles )
               strcat( lpszMailHdrs, "*From:* " );
            else
               strcat( lpszMailHdrs, "From: " );

            b64decodePartial(lpTmpBuf);

            lpBufPtr = lpszMailHdrs + strlen(lpszMailHdrs);
            strcat( lpBufPtr, lpTmpBuf );
            strcat( lpBufPtr, "\r\n" );
         }

         lpEnd = lpszMailHdrs + strlen(lpszMailHdrs);
      }
      if( fShortHeadersTo )
      {
         Amdb_GetMailHeaderField( pth, "To", lpTmpBuf, 500, TRUE );
         if( *lpTmpBuf )
            {
            if( fMsgStyles )
               strcat( lpszMailHdrs, "*To:* " );
            else
               strcat( lpszMailHdrs, "To: " );

            b64decodePartial(lpTmpBuf);

            lpBufPtr = lpszMailHdrs + strlen(lpszMailHdrs);
            strcat( lpBufPtr, lpTmpBuf );
            strcat( lpBufPtr, "\r\n" );
            }

         lpEnd = lpszMailHdrs + strlen(lpszMailHdrs);
      }
      if( fShortHeadersCC )
      {
         Amdb_GetMailHeaderField( pth, "CC", lpTmpBuf, 500, TRUE );
         if( *lpTmpBuf )
         {
            if( fMsgStyles )
               strcat( lpszMailHdrs, "*CC:* " );
            else
               strcat( lpszMailHdrs, "CC: " );

            b64decodePartial(lpTmpBuf);

            lpBufPtr = lpszMailHdrs + strlen(lpszMailHdrs);
            strcat( lpBufPtr, lpTmpBuf );
            strcat( lpBufPtr, "\r\n" );
         }
      }
      if( fShortHeadersDate )
      {
         Amdb_GetMailHeaderField( pth, "Date", lpTmpBuf, 500, TRUE );
         if( *lpTmpBuf )
         {
            if( fMsgStyles )
               strcat( lpszMailHdrs, "*Date:* " );
            else
               strcat( lpszMailHdrs, "Date: " );

            b64decodePartial(lpTmpBuf);

            lpBufPtr = lpszMailHdrs + strlen(lpszMailHdrs);
            strcat( lpBufPtr, lpTmpBuf );
            strcat( lpBufPtr, "\r\n" );
         }
      }
      else
         *lpEnd = '\0';
      if( NULL != lpBufPtr && *lpBufPtr )
      {
         strcat( lpBufPtr, "\r\n" );
         lpszPreText = lpszMailHdrs;
      }
   }
   return( lpszPreText );
}

/* This function displays the Short Header dialog.
 */
void FASTCALL CmdShortHeaderEdit( HWND hwnd )
{
   Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_SHEDIT), ShortHeaderEdit, 0L );
}

/* This function handles the Short Header dialog box.
 */
BOOL EXPORT CALLBACK ShortHeaderEdit( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = ShortHeaderEdit_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the Short
 * header dialog.
 */
LRESULT FASTCALL ShortHeaderEdit_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, ShortHeaderEdit_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, ShortHeaderEdit_OnCommand );

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL ShortHeaderEdit_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{

   CheckDlgButton( hwnd, IDD_SUBJECT, fShortHeadersSubject );
   CheckDlgButton( hwnd, IDD_FROM, fShortHeadersFrom );
   CheckDlgButton( hwnd, IDD_TO, fShortHeadersTo );
   CheckDlgButton( hwnd, IDD_CC, fShortHeadersCC );
   CheckDlgButton( hwnd, IDD_DATE, fShortHeadersDate );  
   return( TRUE );

}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL ShortHeaderEdit_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDOK: {
         if( !IsDlgButtonChecked( hwnd, IDD_SUBJECT ) && !IsDlgButtonChecked( hwnd, IDD_FROM ) && !IsDlgButtonChecked( hwnd, IDD_TO ) && !IsDlgButtonChecked( hwnd, IDD_CC ) )
         {
            fMessageBox( hwnd, 0, "You must choose at least one option.", MB_OK|MB_ICONINFORMATION );
            break;
         }

         fShortHeadersSubject = IsDlgButtonChecked( hwnd, IDD_SUBJECT );
         fShortHeadersFrom = IsDlgButtonChecked( hwnd, IDD_FROM );
         fShortHeadersTo = IsDlgButtonChecked( hwnd, IDD_TO );
         fShortHeadersCC = IsDlgButtonChecked( hwnd, IDD_CC );
         fShortHeadersDate = IsDlgButtonChecked( hwnd, IDD_DATE );
         EndDialog( hwnd, TRUE );
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}

void WINAPI Ameol2_ShowUnreadMessageCount( void )
{
   ShowUnreadMessageCount();
}
