/* MAIN.H - Main Ameol header file
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
#include "pdefs.h"
#include <stdlib.h>
#include "version.h"
#include <htmlhelp.h>
#include "amctrls.h"
#include "amuser.h"
#include "amdb.h"
#include "amob.h"
#include "amlib.h"
#include <commdlg.h>
#include "amcntxt.h"

#define  NOPENINFO
#define  NOVIRTEVENT
#define  NORCDICTIONARY
#define  NOCONFIGRECOG

/* Fix bugs in windowsx.h.
 */
#undef ComboBox_SetItemHeight
#define ComboBox_SetItemHeight(hwndCtl, index, cyItem)     ((int)(DWORD)SendMessage((hwndCtl), CB_SETITEMHEIGHT, (WPARAM)(int)(index), (LPARAM)(long)(cyItem)))

#ifdef WIN32
#define HANDLE_WM_COMMANDEX(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (int)(LOWORD(wParam)), (HWND)(lParam), (UINT)HIWORD(wParam)))
#define FORWARD_WM_COMMANDEX(hwnd, id, hwndCtl, codeNotify, fn) \
    (fn)((hwnd), WM_COMMAND, MAKELPARAM((UINT)(id),(codeNotify) ), (LPARAM)(hwndCtl))
#else
#define HANDLE_WM_COMMANDEX(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (int)(wParam), (HWND)LOWORD(lParam), (UINT)HIWORD(lParam)))
#define FORWARD_WM_COMMANDEX(hwnd, id, hwndCtl, codeNotify, fn) \
    (fn)((hwnd), WM_COMMAND, (WPARAM)(int)(id), MAKELPARAM((UINT)(hwndCtl), (codeNotify)))
#endif

/* void Cls_OnSetTopic(HWND hwnd, PTL ptl); */
#define HANDLE_WM_SETTOPIC(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (PTL)(lParam)),0L)
#define FORWARD_WM_SETTOPIC(hwnd, ptl) \
    (void)(fn)((hwnd), WM_DELETETOPIC, 0, LPARAM((ptl)))

/* void Cls_OnChangeFont(HWND hwnd, int type); */
#define HANDLE_WM_CHANGEFONT(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (int)(wParam)),0L)
#define FORWARD_WM_CHANGEFONT(hwnd, type) \
    (void)(fn)((hwnd), WM_CHANGEFONT, (WPARAM)(type), 0L)

#ifdef WIN32
/* void Cls_OnSettingChange(HWND hwnd, LPCTSTR lpszSectionName) */
#define HANDLE_WM_SETTINGCHANGE(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (int)(wParam), (LPCTSTR)(lParam)), 0L)
#define FORWARD_WM_SETTINGCHANGE(hwnd, wFlag, lpszSectionName, fn) \
    (void)(fn)((hwnd), WM_SETTINGCHANGE, (WPARAM)(wFlag), (LPARAM)(LPCTSTR)(lpszSectionName))
#endif

#ifndef WIN32
#define  Edit_ScrollCaret(hw)
#endif

/* Stuff to use WNDCLASSEX in Win32 and WNDCLASS in Win16.
 */
#ifdef WIN32
#define  APPWNDCLASS                   WNDCLASSEX
#define  AppWndClass_Init(w)           (w).cbSize = sizeof(WNDCLASSEX)
#define  AppWndClass_SetIcon16(w,h)    (w).hIconSm=(h)
#define  AppRegisterClass(w)           RegisterClassEx((w))
#else
#define  APPWNDCLASS                   WNDCLASS
#define  AppWndClass_Init(w)
#define  AppWndClass_SetIcon16(w,h)
#define  AppRegisterClass(w)           RegisterClass((w))
#endif
#define  AppWndClass_Style(w,d)        (w).style = (d)
#define  AppWndClass_WndProc(w,p)      (w).lpfnWndProc = (p)
#define  AppWndClass_ClsExtra(w,c)     (w).cbClsExtra = (c)
#define  AppWndClass_WndExtra(w,c)     (w).cbWndExtra = (c)
#define  AppWndClass_Instance(w,h)     (w).hInstance = (h)
#define  AppWndClass_SetIcon(w,h)      (w).hIcon=(h)
#define  AppWndClass_Cursor(w,c)       (w).hCursor=(c)
#define  AppWndClass_Background(w,b)   (w).hbrBackground=(HBRUSH)(b)
#define  AppWndClass_Menu(w,m)         (w).lpszMenuName=(m)
#define  AppWndClass_ClassName(w,cn)   (w).lpszClassName=(cn)

/* Character literals.
 */
#define  SOH                  1
#define  STX                  2
#define  BEL                  7
#define  BS                   '\b'
#define  TAB                  '\t'
#define  DLE                  16
#define  CAN                  24
#define  CR                   '\r'
#define  LF                   '\n'

/* Default (built-in) categories.
 */
enum {
CAT_FILE_MENU, 
CAT_EDIT_MENU, 
CAT_MAIL_MENU,
CAT_NEWS_MENU,
CAT_VIEW_MENU,
CAT_TOPIC_MENU,
CAT_MESSAGE_MENU,
CAT_SETTINGS_MENU,
CAT_WINDOW_MENU,
CAT_HELP_MENU
};

/* Categories with no default menu equivalents.
 */
#define  CAT_NON_MENU               100
#define  CAT_KEYBOARD               100
#define  CAT_SPARE                  101
#define  CAT_TERMINAL               102
#define  CAT_SAVEDSEARCH               103
#define  CAT_BLINKS                 104
#define  CAT_FORMS                  105
#define  CAT_EXTAPP                 106

/* Spelling checker options
 */
#define  SC_FL_KEEPSESSION       0x0001
#define  SC_FL_USELASTSESSION    0x0002
#define  SC_FL_DIALOG            0x0004

/* Customise Dialog pages.
 */
#define  CDU_KEYBOARD      0
#define  CDU_COLOURS       1
#define  CDU_FONTS         2

/* Preferences Dialog pages.
 */
#define  ODU_GENERAL       0
#define  ODU_READING       1
#define  ODU_EDITOR        2
#define  ODU_VIEWER        3
#define  ODU_ATTACHMENTS   4
#define  ODU_PURGE         5
#define  ODU_SPELLING      6
#define  ODU_BROWSER       7

/* Messages Dialog pages.
 */
#define  ODU_MAIL          0
#define  ODU_MAILHEADERS   1

/* Directories Dialog pages.
 */
#define  ODU_DIRECTORIES   0
#define  ODU_DIRASSOCIATE  1

/* Account types.
 */
#define ACTYP_CONFERENCING             'P' // !!SM!! 2038
#define ACTYP_CONFERENCING_AND_INTERNET      'X' // !!SM!! 2045

/* Properties Dialog pages.
 */
#define  IPP_SETTINGS      0
#define  IPP_STATISTICS    1
#define  IPP_PURGE         2
#define  IPP_SIGNATURE     3
#define  IPP_RULES         4

/* Advance options
 */
#define  ADVANCE_STAY            's'
#define  ADVANCE_NEXT            'n'
#define  ADVANCE_NEXTUNREAD      'r'

/* News flags
 */
#define  NEWS_DELIVER_IP            'I'
#define  NEWS_DELIVER_NONE          '0'

/* Mail flags.
 */
#define  MAIL_DELIVER_CIX           'X'
#define  MAIL_DELIVER_IP            'I'
#define  MAIL_DELIVER_AI            '$'

#define  MAIL_PRIORITY_LOWEST       -2
#define  MAIL_PRIORITY_LOW          -1
#define  MAIL_PRIORITY_NORMAL       0
#define  MAIL_PRIORITY_HIGH         1
#define  MAIL_PRIORITY_HIGHEST      2
#define  MAX_MAIL_PRIORITIES        5

/* Permission flags.
 */
#define  PF_ADMINISTRATE         0x00000001
#define  PF_CANPURGE             0x00000002
#define  PF_CANCREATEFOLDERS     0x00000004
#define  PF_CANUSECIXIP          0x00000008
#define  PF_CANUSECIX            0x00000010
#define  PF_CANCONFIGURE         0x00000020
#define  PF_ALL                  0xFFFFFFFF
#define  PF_BASIC                ((PF_ALL)&~(PF_ADMINISTRATE))

/* Compatibility flags
 */
#define  COMF_OLDMDICLOSESCHEME           0x0001
#define  COMF_NOJUMPAFTERMARKINGREAD         0x0002
#define  COMF_NOMARKUNREADONBACKSPACE     0x0004
#define  COMF_OLDPURGESIZE             0x0008
#define  COMF_DONTDOPROFILE               0x0010
#define COMF_NOTNEXTUNREAD             0x0020
#define COMF_COLLAPSECATEGORIES           0x0040
#define COMF_NO_X_AMEOL                0x0080

/* Services.
 */
#define  SERVICE_CIX          0x63
#define  SERVICE_IP           0x65
#define  SERVICE_RAS          0x68

/* Scripts structure.
 */
#define  MAX_FILENAME         144
#define  MAX_DESCRIPTION      100

typedef struct tagSCRIPT {
   struct tagSCRIPT FAR * lpNext;
   struct tagSCRIPT FAR * lpPrev;
   char szFileName[ MAX_FILENAME ];
   char szDescription[ MAX_DESCRIPTION ];
} SCRIPT, FAR * LPSCRIPT;

/* What is being dropped?
 */
#define  WDD_MESSAGES      0
#define  WDD_USERLIST      1

/* Drag/drop structure.
 */
typedef struct tagDRAGDROPSTRUCT {
   HWND hwndDrop;                      /* Destination dialog or window */
   POINT pt;                           /* Coordinate of drop */
   UINT wDataType;                     /* Type of data being dropped */
   LPVOID lpData;                      /* Pointer to data */
} DRAGDROPSTRUCT, FAR * LPDRAGDROPSTRUCT;

/* void Cls_OnDragDrop(HWND hwnd, UINT wDragType, LPDRAGDROPSTRUCT lpdds); */
#define HANDLE_WM_DRAGDROP(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (UINT)(wParam), (LPDRAGDROPSTRUCT)(lParam)),0L)
#define FORWARD_WM_DRAGDROP(hwnd, uDragType, lpdds, fn) \
    (void)(fn)((hwnd), WM_DELETETOPIC, (WPARAM)(uDragType), (LPARAM)(lpdds))

typedef struct tagWINDINFO {
   PCAT lpCategory;                    /* Handle of topic category */
   PCL lpFolder;                       /* Handle of topic folder */
   PTL lpTopic;                        /* Handle of topic shown in message window */
   PTH lpMessage;                      /* Handle of current message being shown */
   int nSplitPos;                      /* Saved window split position */
   int nSplitCur;                      /* Current window split position */
   int nHSplitPos;                     /* Saved folder window split position */
   int nHSplitCur;                     /* Current folder window split position */
   BOOL fLockUpdates;                  /* TRUE if thread pane is not refreshed */
   BOOL fUpdatePending;                /* TRUE if a refresh is pending */
   BOOL fFixedFont;                    /* TRUE if we're showing a fixed font */
   HWND hwndFocus;                     /* Handle of sub-window that has the focus */
   VIEWDATA vd;                        /* View display information */
} WINDINFO;

typedef WINDINFO FAR * LPWINDINFO;

#define  GWW_EXTRA         4
#define  GetBlock(h)       (WINDINFO FAR *)GetWindowLong((h), 0 )
#define  PutBlock(h,b)     (WINDINFO FAR *)SetWindowLong((h), 0, (LONG)(b))

#define PORT_CIXCOM        2523

/* DrawCheckBox flags
 */
#define  CXB_NORMAL        0
#define  CXB_SELECTED      1
#define  CXB_ACTIVE        2

/* Popup menus
 */
#define  IPOP_EDITWINDOW      0
#define  IPOP_THDWINDOW       1
#define  IPOP_OBASKWINDOW     2
#define  IPOP_MSGWINDOW       3
#define  IPOP_MSGIDTITLE      4
#define  IPOP_GRPLISTWINDOW   5
#define  IPOP_IBASKWINDOW     6
#define  IPOP_ATTACHMENT      7
#define  IPOP_TOOLMENU        8
#define  IPOP_ULISTWINDOW     9
#define  IPOP_CLISTWINDOW     10
#define  IPOP_FILELIST        11
#define  IPOP_RESUMEWND       12
#define  IPOP_PARMPICKER      13
#define  IPOP_QUOTEHDRPICKER  14
#define  IPOP_DOWNLOAD        15
#define  IPOP_UPLOAD          16
#define  IPOP_SCHEDULER       17
#define  IPOP_PARLIST         18
#define  IPOP_MODPARLIST      19
#define  IPOP_SEARCHPARMS     20

/* Get/Set bits */
#define  BSB_CLRBIT           0
#define  BSB_SETBIT           1
#define  BSB_GETBIT           2

/* File types
 */
#define  DSD_PARLIST          0
#define  DSD_FLIST            1
#define  DSD_SIG              2
#define DSD_RESUME            3
#define  DSD_ROOT          254
#define  DSD_CURRENT          255

/* Some limitations.
 */
#define  LEN_PASSWORD         40
#define  LEN_TEMPBUF          512
#define  LEN_LOGINNAME        40
#define  LEN_MAILBOX          60
#define  LEN_LOGINPASSWORD    (LEN_PASSWORD)
#define  LEN_DIR              9
#define  LEN_TERMNAME         59

/* Data field size limits.
 * These do NOT include the terminating NULL character..!
 */
#define  LEN_COMMENT       63
#define  LEN_CIXNAME       14                /* A CIX user ID */
#define  LEN_MAILADDR      511                  /* A single e-mail address */
#define  LEN_DESCRIPTION      79
#define  LEN_MESSAGEID     511                  /* Name of an unique mail message number */
#define  LEN_FULLNAME      ((LEN_FOLDERNAME)+(LEN_TOPICNAME)+1)
#define  LEN_MODERATORS    80                /* List of moderators */
#define  LEN_AUTHORLIST    143               /* List of CIX users */
#define  LEN_CIXFILENAME   14                /* Maximum size of a CIX file name */
#define  LEN_FILENAME      259               /* Maximum size of a DOS file name (+extension) */
#define  LEN_DOSFILENAME   12                /* Maximum size of a DOS file name (+extension) */
#define  LEN_PATHNAME      260               /* Maximum size of full DOS pathname (directory+filename) */
#define  LEN_DIRNAME       259               /* Maximum size of a directory path */
#define  LEN_FULLCIXNAME   80                /* Full user name */
#define  LEN_COMMAND_LINE  80
#define  LEN_MENUTEXT      40
#define  LEN_HELPTEXT      160
#define  LEN_KEYNAME       20
#define  LEN_ALL           (LEN_COMMAND_LINE+LEN_MENUTEXT+LEN_HELPTEXT+LEN_KEYNAME)
#define  LEN_PORTNAME      40
#define  LEN_PHONENO       60
#define  LEN_QDELIM        10

#define  MAX_MSGSIZE       0xFFF0
#define  MAX_WRAPCOLUMN    200

#define  TID_CLOCK            1
#define  TID_SWEEPER          2
#define  TID_FLASHICON        3
#define  TID_DISCONNECT       4
#define  TID_BLINKANIMATION   5
#define  TID_SCHEDULER        6
#define TID_OLT               7 /* Online timeout */
#define TID_MAILALERT         8 /* Mail alert window flashy timer */
#define TID_MAILALERTTO       9 /* Mail alert dialog timeout */

typedef struct tagENCODINGSCHEMES {
   char * szDescription;               /* Description of scheme */
   UINT wID;                           /* ID of scheme */
} ENCODINGSCHEMES;

#define  RIGHT_MARGIN            74

/* Category folder view indexes
 */
#define  IBML_CATEGORY           0        /* Index of category icon */
#define  IBML_SELCATEGORY        1        /* Index of expanded category icon */
#define  IBML_FOLDER             2        /* Index of folder icon */
#define  IBML_SELFOLDER          3        /* Index of expanded folder icon */
#define  IBML_NEWS               4        /* Index of news folder icon */
#define  IBML_MAIL               5        /* Index of mail folder icon */
#define  IBML_UNTYPED            6        /* Index of untyped folder icon */
#define  IBML_CIX                7        /* Index of CIX topic icon */
#define  IBML_LOCAL              8        /* Index of local topic icon */
#define  IBML_MAX                9        /* Total number of icons */

#define  WM_MDIMENUCHANGE        WM_USER+0x100
#define  WM_CHANGEMDIMENU        WM_USER+0x101
#define  WM_AMEOL_INIT1          WM_USER+0x102
#define  WM_CHANGEFONT           WM_USER+0x103
#define  WM_DELETETOPIC          WM_USER+0x104
#define  WM_DELETEFOLDER         WM_USER+0x105
#define  WM_VIEWCHANGE           WM_USER+0x106
#define  WM_NEWSERROR            WM_USER+0x107
#define  WM_QUERYISONLINE        WM_USER+0x108
#define  WM_TOGGLEHEADERS        WM_USER+0x109
#define  WM_TOPICCHANGED         WM_USER+0x10A
#define  WM_FOLDERCHANGED        WM_USER+0x10B
#define  WM_CATEGORYCHANGED      WM_USER+0x10C
#define  WM_CONTEXTSWITCH        WM_USER+0x10D
#define  WM_TASKBARNOTIFY        WM_USER+0x10E
#define  WM_EXEC_CIX             WM_USER+0x10F
#define  WM_PARSEURL             WM_USER+0x110
#define  WM_UPDATE_UNREAD        WM_USER+0x111
#define  WM_DELETEMSG            WM_USER+0x112
#define  WM_HEADERSCHANGED       WM_USER+0x113
#define  WM_INSERTTOPIC          WM_USER+0x114
#define  WM_INSERTFOLDER         WM_USER+0x115
#define  WM_APPCOLOURCHANGE      WM_USER+0x116
#define  WM_SETTOPIC             WM_USER+0x117
#define  WM_DRAGDROP             WM_USER+0x118
#define  WM_SELECTITEM           WM_USER+0x119
#define  WM_BLINK2               WM_USER+0x11A
#define  WM_REDRAWWINDOW         WM_USER+0x11B
#define  WM_CLOSEFOLDER          WM_USER+0x11C
#define  WM_TOGGLEFIXEDFONT      WM_USER+0x120
#define  WM_LOCKUPDATES          WM_USER+0x121
#define  WM_TOPICRESIGNED        WM_USER+0x122
#define  WM_FOLDERRESIGNED       WM_USER+0x123
#define  WM_SOCKMSG              WM_USER+0x124
#define  WM_INSERTCATEGORY       WM_USER+0x125
#define  WM_DELETECATEGORY       WM_USER+0x126
#define  WM_UPDMSGFLAGS          WM_USER+0x127
#define  WM_NEWMSG               WM_USER+0x128


// Common Hotspot Handler Return Codes

#define  AMCHH_NOCHANGE          0  // !!SM!! 2032
#define  AMCHH_MOVEDMESSAGE         1  // !!SM!! 2032
#define  AMCHH_OPENEDMDI            2  // !!SM!! 2032
#define  AMCHH_LAUNCHEXTERNAL    3  // !!SM!! 2032

#define  FONTYP_TERMINAL      0
#define  FONTYP_EDIT          1
#define  FONTYP_MESSAGE       2
#define  FONTYP_THREAD        3
#define  FONTYP_INBASKET      5
#define  FONTYP_FIXEDFONT     6
#define  FONTYP_GRPLIST       7
#define  FONTYP_OUTBASKET     8
#define  FONTYP_USERLIST      9
#define  FONTYP_CIXLIST       10
#define  FONTYP_RESUMES       11
#define  FONTYP_SCHEDULE      12
#define  FONTYP_ADDRBOOK      13

#define  WIN_OUTBASK          0x00000001
#define  WIN_THREAD           0x00000002
#define  WIN_MESSAGE          0x00000004
#define  WIN_EDITS            0x00000008
#define  WIN_INFOBAR          0x00000010
#define  WIN_INBASK           0x00000020
#define  WIN_TERMINAL         0x00000040
#define  WIN_GRPLIST          0x00000080
#define  WIN_USERLIST         0x00000100
#define  WIN_CIXLIST          0x00000200
#define  WIN_RESUMES          0x00000400
#define  WIN_PARLIST          0x00000800
#define  WIN_SCHEDULE         0x00001000
#define  WIN_PURGESETTINGS    0x00002000
#define  WIN_RESUMESLIST      0x00004000
#define  WIN_ADDRBOOK         0x00008000
#define  WIN_NOMENU           0x80000000
#define  WIN_MASK             0x7FFFFFFF
#define  WIN_ALL              (WIN_OUTBASK|WIN_THREAD|WIN_MESSAGE|WIN_EDITS|WIN_INFOBAR|\
                               WIN_INBASK|WIN_TERMINAL|WIN_GRPLIST|WIN_USERLIST|WIN_CIXLIST|\
                               WIN_RESUMES|WIN_PARLIST|WIN_SCHEDULE|WIN_PURGESETTINGS|\
                               WIN_RESUMESLIST|WIN_ADDRBOOK)

/* Custom headers.
 */
#define  MAX_HEADERS    16

typedef struct tagCUSTOMHEADERS {
   char szType[ LEN_RFCTYPE ];         /* RFC type name */
   char szValue[ 80 ];                 /* Value of type */
} CUSTOMHEADERS;

#define  ABMP_EXPANDED     0
#define  ABMP_HOLD         1
#define  ABMP_KEEP         2
#define  ABMP_DELETE       3
#define  ABMP_CLOSED       4
#define  ABMP_UNREAD       5
#define  ABMP_TAGGED       6
#define  ABMP_RETRIEVED    7
#define  ABMP_ATTACHMENT   8
#define  ABMP_BODY         9
#define  ABMP_USER         10
#define  ABMP_ADMIN        11
#define  ABMP_WITHDRAWN    12
#define  ABMP_NEWUSER      13
#define  ABMP_NEWADMIN     14
#define  ABMP_REMPAR       15
#define  ABMP_EXMOD        16
#define  ABMP_WATCH        17

#define  MAX_FILENAME         144
#define  MAX_DESCRIPTION      100

#define  SP_OK          0
#define  SP_CANCEL      -1
#define  SP_FINISH      1

typedef struct tagAPPINFO {
   char szAppPath[ 256 ];                 /* Path and filename of application */
   char szCmdLine[ 256 ];                 /* Full command line */
   char szCommandName[ 64 ];              /* Command name */
} APPINFO, FAR * LPAPPINFO;

extern AMAPI_VERSION amv;
extern BOOL f3DStyle;
extern BOOL fAlertTimedOut;
extern BOOL fAltMsgLayout;
extern BOOL fArcScratch;
extern BOOL fAttachmentSizeWarning;
extern BOOL fAttachmentConvertToShortName;
extern BOOL fCheckForAttachments;  // !!SM!! 2.55.2033
extern BOOL fAutoCollapse;
extern BOOL fAutoDecode;
extern BOOL fAutoIndent;
extern BOOL fAutoQuoteMail;
extern BOOL fAutoQuoteNews;
extern BOOL fAutoSpellCheck;
extern BOOL fBeepAtEnd;
extern BOOL fBlankSubjectWarning;
extern BOOL fBlinking;
extern BOOL fBrowserIsMSIE;
extern BOOL fButtonLabels;
extern BOOL fCCMailToSender;
extern BOOL fCixLink;
extern BOOL fCIXViaInternet;
extern BOOL fColourQuotes;
extern BOOL fCompletingSetupWizard;
extern BOOL fCopyRule;
extern BOOL fCTSDetect;
extern BOOL fDebugMode;
extern BOOL fDeleteMailAfterReading;
extern BOOL fDisconnectTimeout;
extern BOOL fDisplayLockCount;
extern BOOL fDNSCache;
extern BOOL fDoBlinkAnimation;
extern BOOL fDoneConnectDevice;
extern BOOL fDoneDeviceConnected;
extern BOOL fDrawThreadLines;
extern BOOL fEditAtTop;
extern BOOL fEndBlinking;
extern BOOL fExpandLFtoCRLF;
extern BOOL fFileDebug;
extern BOOL fFileListWndActive;
extern BOOL fFirstInstance;
extern BOOL fFirstRun;
extern BOOL fFirstTimeThisUser;
extern BOOL fForceNextUnread;
extern BOOL fGetMailDetails;
extern BOOL fGetParList;
extern BOOL fHotLinks;
extern BOOL fInitialising;
extern BOOL fInitiateQuit;
extern BOOL fInitiatingBlink;
extern BOOL fInitNewsgroup;
extern BOOL fIsActive;
extern BOOL fKeyboardChanged;
extern BOOL fLaptopKeys;
extern BOOL fLargeButtons;
extern BOOL fLaunchAfterDecode;
extern BOOL fMailto;
extern BOOL fMainAbort;
extern BOOL fMsgStyles;
extern BOOL fMultiLineTabs;
extern BOOL fNewMail;
extern BOOL fNewMailPlayed;
extern BOOL fNewsErrorReported;
extern BOOL fNNTPAuthInfo;
extern BOOL fNoCMN;
extern BOOL fNoisy;
extern BOOL fOldLogo;
extern BOOL fOnline;
extern BOOL fOpenTerminalLog;
extern BOOL fPageSetupDialogActive;
extern BOOL fParseDuringBlink;
extern BOOL fParsing;
extern BOOL fPOP3Last;
extern BOOL fPriorityFirst;
extern BOOL fPromptForFilenames;
extern BOOL fProtectMailFolders;
extern BOOL fNewMailAlert;
extern BOOL fQuitAfterConnect;
extern BOOL fQuitSeen;
extern BOOL fQuitting;
extern BOOL fRememberPassword;
extern BOOL fRulesEnabled;
extern BOOL fRunningScript;
extern BOOL fSecureOpen;
extern BOOL fSeparateEncodedCover;
extern BOOL fSetForward;
extern BOOL fShowBlinkWindow;
extern BOOL fShowClock;
extern BOOL fShortHeaders;
extern BOOL fShortHeadersSubject;
extern BOOL fShortHeadersFrom;
extern BOOL fShortHeadersTo;
extern BOOL fShortHeadersCC;
extern BOOL fShortHeadersDate;
extern BOOL fShowTopicType;
extern BOOL fShowMugshot;
extern BOOL fSignUpViaTelnet;
extern BOOL fSplitParts;
extern BOOL fStartOnline;
extern BOOL fStretchMugshot;
extern BOOL fStripHeaders;
extern BOOL fStripHTML;
extern BOOL fReplaceAttachmentText;
extern BOOL fBackupAttachmentMail;
extern BOOL fThreadMailBySubject;
extern BOOL fTmpShortHeadersSubject;
extern BOOL fTmpShortHeadersFrom;
extern BOOL fTmpShortHeadersTo;
extern BOOL fTmpShortHeadersCC;
extern BOOL fTopicClosed;
extern BOOL fTraceOutputDebug;
extern BOOL fTraceLogFile;
extern BOOL fUnreadAfterBlink;
extern BOOL fUseInternet;
extern BOOL fUseLegacyCIX;
extern BOOL fUseCIX;
extern BOOL fUseCopiedMsgSubject;
extern BOOL fUseDictionary;
extern BOOL fUseMButton;
extern BOOL fUseTooltips;
extern BOOL fVisibleSeperators;
extern BOOL fNewButtons;
extern BOOL fViewCixTerminal;
extern BOOL fWarnOnExit;
extern BOOL fWarnAfterBlink;
extern BOOL fAlertOnFailure;
extern BOOL fWinNT;
extern BOOL fWorkspaceScrollbars;
extern BOOL fYIA;
extern BOOL fZmodemActive;
extern BYTE rgEncodeKey[ 8 ];
extern BOOL fSingleClickHotspots;
extern BOOL fShowAcronyms;
extern BOOL fPageBeyondEnd;
extern BOOL fShowLineNumbers;
extern BOOL fMultiLineURLs;
extern BOOL fShowWrapCharacters;
extern BOOL fAntiSpamCixnews;
extern BOOL fRFC2822; //!!SM!! 2.55.2035
extern BOOL fBreakLinesOnSave;   // !!SM!! 2.55.2045
extern BOOL fUseWebServices;  // !!SM!! 2.56.2053

extern char * pszAddonsDir;
extern char * pszAmeolDir;
extern char * pszAppDataDir;
extern char * pszArchiveDir;
extern char * pszAttachDir;
extern char * pszCixMonths[12];
extern char * pszDataDir;
extern char * pszDownloadDir;
extern char * pszModulePath;
extern char * pszMugshotDir;
extern char * pszResumesDir;
extern char * pszScratchDir;
extern char * pszScriptDir;
extern char * pszUploadDir;
extern char * szPriorityDescription[ MAX_MAIL_PRIORITIES ];
extern char chQuote;
extern char FAR szAccountName[ 64 ];
extern char FAR szAuthInfoPass[ 40 ];
extern char FAR szAuthInfoUser[ 64 ];
extern char FAR szBrowser[];
extern char FAR szCompactHdrTmpl[];
extern char FAR szDateFormat[ 30 ];
extern char FAR szDefMailDomain[ 60 ];
extern char FAR szDomain[ 60 ];
extern char FAR szFullName[ 64 ];
extern char FAR szHostname[ 64 ];
extern char FAR szMailAddress[ 64 ];
extern char FAR szMailBox[ LEN_MAILBOX ];
extern char FAR szMailLogin[ 64 ];
extern char FAR szMailPassword[ 40 ];
extern char FAR szMailClearPassword[ 40 ];
extern char FAR szSMTPMailLogin[ 64 ];
extern char FAR szSMTPMailPassword[ 40 ];
extern char FAR szMailReplyAddress[ 64 ];
extern char FAR szMailServer[ 64 ];
extern char FAR szNewsMsg[ 512 ];
extern char FAR szNewsServer[ 64 ];
extern char FAR szProvider[ 60 ];
extern char FAR szQuoteMailHeader[ 100 ];
extern char FAR szQuoteNewsHeader[ 100 ];
extern char FAR szExpandedQuote[ 15 ];
extern char FAR szExpandedQuoteMailHeader[ 15 ];
extern char FAR szExpandedQuoteNewsHeader[ 15 ];
extern char FAR szSMTPMailServer[ 64 ];
extern char FAR szUsenetGateway[ 10 ];
extern char FAR szUsersDir[];
extern char FAR szProf1[ LEN_TEMPBUF ];
extern char FAR szProf2[ LEN_TEMPBUF ];
extern char FAR szProf3[ LEN_TEMPBUF ];
extern char FAR szProf4[ LEN_TEMPBUF ];
extern char FAR szProf5[ LEN_TEMPBUF ];
extern char NEAR szLoginUsername[ LEN_LOGINNAME ];    
extern char NEAR szAppName[ 64 ];
extern char NEAR szAppLogo[ 64 ];
extern char NEAR szSplashLogo[ 64 ];
extern char NEAR szSplashRGB[ 12 ];
extern char NEAR szAreaCode[ 10 ];
extern char NEAR szCIXPhoneNumber[ 64 ];
extern char NEAR szCountryCode[ 5 ];
extern char NEAR szDatabaseFile[];
extern char NEAR szErrLogClass[];
extern char NEAR szGlobalSig[];
extern char NEAR szIniSystem[];
extern char NEAR szOrganization[];
extern char NEAR szPhoneNumber[ 30 ];
extern char NEAR szQuoteDelimiters[];
extern char NEAR szRegRscFile[];
extern char NEAR szSignatureFile[];
extern char szNewsgroup[ 80 ];
extern char szMailtoAddress[ 80 ];
extern char szCixLink[ 80 ];
extern char szCIXConnCard[ 40 ];
extern char szCIXGateway[ 64 ];
extern char szCIXNickname[ 16 ];
extern char szCIXPassword[ 40 ];
extern char szCIXClearPassword[ 40 ];
extern char szCixProtocol[ 16 ];
extern char szLoginPassword[ LEN_LOGINPASSWORD ];
extern char szInitialise[ 80 ];
extern COLORREF crAddrBookText;
extern COLORREF crAddrBookWnd;
extern COLORREF crCIXListText;
extern COLORREF crCIXListWnd;
extern COLORREF crEditWnd;
extern COLORREF crEditText;
extern COLORREF crGrpListText;
extern COLORREF crGrpListWnd;
extern COLORREF crHighlight;
extern COLORREF crHighlightText;
extern COLORREF crHotlinks;
extern COLORREF crIgnoredMsg;
extern COLORREF crIgnoredTopic;
extern COLORREF crInBaskText;
extern COLORREF crInBaskWnd;
extern COLORREF crInfoBar;
extern COLORREF crKeepAtTop;
extern COLORREF crLoginAuthor;
extern COLORREF crMarkedMsg;
extern COLORREF crModerated;
extern COLORREF crMail;
extern COLORREF crNews;
extern COLORREF crMsgText;
extern COLORREF crMsgWnd;
extern COLORREF crNormalMsg;
extern COLORREF crOutBaskText;
extern COLORREF crOutBaskWnd;
extern COLORREF crPriorityMsg;
extern COLORREF crQuotedText;
extern COLORREF crResignedTopic;
extern COLORREF crResumeText;
extern COLORREF crResumeWnd;
extern COLORREF crSchedulerText;
extern COLORREF crSchedulerWnd;
extern COLORREF crThreadWnd;
extern COLORREF crTermWnd;
extern COLORREF crTermText;
extern COLORREF crUnReadMsg;
extern COLORREF crUserListText;
extern COLORREF crUserListWnd;
extern const char NEAR szAddons[];
extern const char NEAR szBlinkman[];
extern const char NEAR szSMTPServers[];
extern const char NEAR szCIX[];
extern const char NEAR szCixInitScr[];
extern const char NEAR szCIXIP[];
extern const char NEAR szCixLstFile[];
extern const char NEAR szCixnews[];
extern const char NEAR szCixTermScr[];
extern const char NEAR szColours[];
extern const char NEAR szCompulink[];
extern const char NEAR szConfig[];
extern const char NEAR szConfigFile[];
extern const char NEAR szCixCoUk[];
extern const char NEAR szConnect[];
extern const char NEAR szConnections[];
extern const char NEAR szConnectLog[];
extern const char NEAR szDatabases[];
extern const char NEAR szDefCIXGateway[ 64 ];
extern const char NEAR szDefCIXTelnet[];
extern const char NEAR szDefMail[];
extern const char NEAR szDefNews[];
extern const char NEAR szDirects[];
extern const char NEAR szDowntime[];
extern const char NEAR szExport[];
extern const char NEAR szExtensions[];
extern const char NEAR szFileFind[];
extern const char NEAR szFind[];
extern const char NEAR szFrameWndClass[];
extern const char NEAR szFonts[];
extern const char NEAR szHelpFile[];
extern const char NEAR szImport[];
extern const char NEAR szInfo[];
extern const char NEAR szMail[];
extern const char NEAR szMailDirFile[];
extern const char NEAR szModemsIni[];
extern const char NEAR szMsgViewerWnd[];
extern const char NEAR szNewmesScr[];
extern const char NEAR szNews[];
extern const char NEAR szNewsServers[];
extern const char NEAR szIdentities[];
extern const char NEAR szPostmaster[];
extern const char NEAR szPreferences[];
extern const char NEAR szPrinter[];
extern const char NEAR szScheduler[];
extern const char NEAR szScratchPad[];
extern const char NEAR szScratchPadRcv[];
extern const char NEAR szScrExportFiles[];
extern const char NEAR szScrExportAuthors[];
extern const char NEAR szScriptList[];
extern const char NEAR szSettings[];
extern const char NEAR szSpelling[];
extern const char NEAR szSSCELib[];
extern const char NEAR szTerminals[];
extern const char NEAR szToolbar[];
extern const char NEAR szUsenetLstFile[];
extern const char NEAR szUsenetRcvScratchPad[];
extern const char NEAR szUsenetScratchPad[];
extern const char NEAR szUsersLstFile[];
extern const char NEAR szWWW[];
extern CUSTOMHEADERS uMailHeaders[ MAX_HEADERS ];
extern CUSTOMHEADERS uNewsHeaders[ MAX_HEADERS ];
extern DWORD dwLinesMax;
extern DWORD dwMaxMsgSize;
extern DWORD iActiveMode;
extern DWORD dwDefWindowState;
extern DWORD dwLastNonIdleTime;
extern ENCODINGSCHEMES NEAR EncodingScheme[];
extern float nBottomMargin;
extern float nLeftMargin;
extern float nRightMargin;
extern float nTopMargin;
extern HCURSOR hWaitCursor;
extern HFONT hAddrBookFont;
extern HFONT hCIXListFont;
extern HFONT hEditFont;
extern HFONT hFixedFont;
extern HFONT hGrpListFont;
extern HFONT hHelv10Font;
extern HFONT hHelv8Font;
extern HFONT hHelvB10Font;
extern HFONT hHelvB8Font;
extern HFONT hInBasketBFont;
extern HFONT hInBasketFont;
extern HFONT hMsgFont;
extern HFONT hOutBasketFont;
extern HFONT hPrinterFont;
extern HFONT hResumesFont;
extern HFONT hSchedulerFont;
extern HFONT hStatusFont;
extern HFONT hSys10Font;
extern HFONT hSys8Font;
extern HFONT hTermFont;
extern HFONT hThreadBFont;
extern HFONT hThreadFont;
extern HFONT hUserListFont;
extern HFONT hAboutFont;
extern HICON hAppIcon;
extern HINSTANCE hInst;
extern HINSTANCE hRscLib;
extern HINSTANCE hRegLib; // !!SM!! 2039
extern HINSTANCE hWebLib; // !!SM!! 2053
extern HINSTANCE hSciLexer; // !!SM!! 2041
extern HINSTANCE hSpellLib;
extern HMENU hCIXMenu;
extern HMENU hMoveMenu;
extern HMENU hFormsMenu;
extern HMENU hEditPopupMenu;
extern HMENU hMainMenu;
extern HMENU hPopupMenus;
extern HWND hFindDlg;
extern HWND hSplashScreen;
extern HWND hSearchDlg;
extern HWND hwndActive;
extern HWND hwndBlink;
extern HWND hwndCIXList;
extern HWND hwndRsmPopup;
extern HWND hwndPurgeSettings;
extern HWND hwndFrame;
extern HWND hwndAddrBook;
extern HWND hwndGallery;
extern HWND hwndGrpList;
extern HWND hwndParlist;
extern HWND hwndInBasket;
extern HWND hwndMDIClient;
extern HWND hwndMDIDlg;
extern HWND hwndMsg;
extern HWND hwndOutBasket;
extern HWND hwndQuote;
extern HWND hwndResumes;
extern HWND hwndScheduler;
extern HWND hwndStatus;
extern HWND hwndToolBar;
extern HWND hwndToolBarOptions;
extern HWND hwndStartWiz;
extern HWND hwndStartWizGuide;
extern HWND hwndStartWizProf;
extern HWND hwndToolTips;
extern HWND hwndTopic;
extern HWND hwndUserList;
extern int cMailFormsInstalled;
extern int cMailHeaders;
extern int cNewsHeaders;
extern int cyInfoBar;
extern int nLastError;
extern int fNoExit;
extern int fNewsFilterType;
extern int nLastCommsDialogPage;
extern int idmsbClock;
extern int idmsbOnlineText;
extern int idmsbOutbasket;
extern int idmsbRow;
extern int idmsbColumn;
extern int idmsbText;
extern int iRecent;
extern int iWrapCol;
extern int irWrapCol;
extern int inWrapCol; // 2.56.2055
extern int imWrapCol; // 2.56.2055
extern int iTabSpaces;
extern int iMaxSubjectSize;
extern int nAccountType;
extern int nCIXMailClearCount;
extern int nFontType;
extern int nIPMailClearCount;
extern int nLastCustomiseDialogPage;
extern int nLastDirectoriesDialogPage;
extern int nLastMailDialogPage;
extern int nNewsDialogPage;
extern int nLastOptionsDialogPage;
extern int nLastPropertiesPage;
extern int nMailDelivery;
extern int nMaxToArchive;
extern int nMugshotLatency;
extern int nNewsDelivery;
extern int nOrientation;
extern int nPrinterFont;
extern int nThreadIndentPixels;
extern int nUsenetActive;
extern int wID;
extern LOGFONT lfAddrBook;
extern LOGFONT lfCIXListFont;
extern LOGFONT lfEditFont;
extern LOGFONT lfFixedFont;
extern LOGFONT lfGrpListFont;
extern LOGFONT lfInBasketBFont;
extern LOGFONT lfInBasketFont;
extern LOGFONT lfMsgFont;
extern LOGFONT lfOutBasketFont;
extern LOGFONT lfPrinterFont;
extern LOGFONT lfResumesFont;
extern LOGFONT lfSchedulerFont;
extern LOGFONT lfStatusFont;
extern LOGFONT lfTermFont;
extern LOGFONT lfThreadBFont;
extern LOGFONT lfThreadFont;
extern LOGFONT lfUserListFont;
extern LOGFONT lfUsrPrtFont;
extern LPSTR lpCenterFooter;
extern LPSTR lpCenterHeader;
extern LPSTR lpLeftFooter;
extern LPSTR lpLeftHeader;
extern LPSTR lpPathBuf;
extern LPSTR lpRightFooter;
extern LPSTR lpRightHeader;
extern LPSTR lpTmpBuf;
extern PTL ptlPostmaster;
extern PTL ptlLocalTopicName;
extern PTL ptlNewMailBox;
extern PCL pclNewsFolder;
extern PURGEOPTIONS poDef;
extern UINT wDisconnectRate;
extern UINT wSweepRate;
extern VIEWDATA vdDef;
extern void (FASTCALL * fpParser)( void );
extern WORD fCompatFlag;
extern WORD wDefEncodingScheme;
extern WORD wSpellSelEnd;
extern WORD wSpellSelStart;
extern WORD wWinVer;
extern UINT BF_IPFLAGS;
extern UINT BF_CIXFLAGS;
extern UINT BF_POSTCIX;
extern UINT BF_GETCIX;
extern UINT BF_IPUSENET;
extern UINT BF_CIXUSENET;
extern UINT BF_USENET;
extern UINT BF_FULLCONNECT;
extern UINT BF_EVERYTHING;
extern DWORD fMaxNewsgroups;

/* Stuff specific to Win32
 */
#ifdef WIN32
extern BOOL fTapiDialing;
extern BOOL fTapiAborted;
#endif

/* Some debug stuff to ensure that we don't reuse a shared
 * global inadvertedly.
 */
#ifdef _DEBUG

extern BOOL fUsingPathBuf;

#define  BEGIN_PATH_BUF       { ASSERT(!fUsingPathBuf); fUsingPathBuf = TRUE; }
#define  END_PATH_BUF         { fUsingPathBuf = FALSE; }
#else
#define  BEGIN_PATH_BUF
#define  END_PATH_BUF
#endif

/* Folder enumeration procedure.
 */
typedef (FASTCALL * ENUMFOLDERPROC)( PTL, LPARAM );

/* DATADIR.C */
char * FASTCALL CreateDirectoryPath( char *, char * );
void FASTCALL WriteDirectoryPath( int, char **, HWND, int, char * );
BOOL FASTCALL ValidateChangedDir( HWND, char * );
BOOL FASTCALL QueryDirectoryExists( char * );
BOOL FASTCALL CheckDSD( void );
HFIND FASTCALL AmFindFirst( LPCSTR, WORD, UINT, FINDDATA * );

/* ABOUT.C */
void FASTCALL CmdAbout( HWND );
void FASTCALL About_GetUserName( char *, int );
void FASTCALL About_GetSiteName( char *, int );

/* OUTBASK.C */
BOOL FASTCALL CreateOutBasket( HWND );
void FASTCALL RemoveObject( LPOB );
void FASTCALL UpdateOutbasketItem( LPOB, BOOL );
void FASTCALL UpdateOutBasketStatus( void );
int FASTCALL ItemFromPoint( HWND, short, short );

/* INTRO.C */
void FASTCALL ShowIntroduction( void );
void FASTCALL HideIntroduction( void );

/* WINDOWS.C */
void FASTCALL CleanAddresses( RECPTR FAR * pAddr, DWORD pLen ); // !!SM!! 2.55.2032
BOOL FASTCALL CheckForAttachment(HWND pEdit, HWND pAttach); /*!!SM!!*/
BOOL FASTCALL CheckValidMailAddress(HWND pEdit);
int FASTCALL b64decodePartial(char *d);

int FAR PASCAL IsAscii(int ch);
int FAR PASCAL WordBreakProc(LPSTR lpszEditText, int ichCurrent, int chEditText, int wActionCode);
void FASTCALL SetEditWindowWidth( HWND, int );
LRESULT FASTCALL EditCtrlIndent( WNDPROC, HWND, UINT, WPARAM, LPARAM );
void FASTCALL SetEditText( HWND, PTL, int, HPSTR, BOOL );
void FASTCALL ReadProperWindowState( char *, RECT *, DWORD * );
void FASTCALL ConstrainWindow( RECT * );
LRESULT CALLBACK GeneralEditProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL ShowEnableControl( HWND, int, BOOL );
BOOL FASTCALL IsEditWindow( HWND );

/* TERMINAL.C */
void FASTCALL EditTerminal( HWND, int );
void FASTCALL InstallTerminals( void );
void FASTCALL NewTerminalByCmd( HWND, UINT );
HWND FASTCALL NewTerminal( HWND, LPSTR, BOOL );
UINT FASTCALL GetTerminalCommandID( LPSTR );
BOOL FASTCALL GetTerminalCommandName( UINT, char * );
void FASTCALL Terminal_WriteString( HWND, LPSTR, int, BOOL );
void FASTCALL UpdateTerminalBitmaps( void );

/* MAIN.C */
HMODULE WINAPI AmLoadLibrary( LPCSTR );
void FASTCALL OpenDefaultWindows( void );
void FASTCALL EnsureMainWindowOpen( void );
void FASTCALL CmdShortHeaderEdit( HWND );
LPSTR FASTCALL GetShortHeaderText( PTH );
LPSTR FASTCALL CreateQuotedMessage( HWND, BOOL );
HMENU FASTCALL GetPopupMenu( int );
BOOL FASTCALL FindLibrary( LPCSTR );
BOOL FASTCALL TaskYield( void );
void FASTCALL SaveKeyboard( void );
void FASTCALL MainWnd_OnMenuSelect( HWND, HMENU, int, HMENU, UINT );
void FASTCALL SetSweepTimer( void );
void FASTCALL KillSweepTimer( void );
void FASTCALL CmdOffline( void );
void CDECL StatusText( LPSTR, ... );
void CDECL OnlineStatusText( LPSTR, ... );
void CDECL OfflineStatusText( LPSTR, ... );
void FASTCALL ShowUnreadMessageCount( void );
BOOL FASTCALL HasClipboardData( void );
BOOL FASTCALL ChangeToolbar( HWND, BOOL, TBBUTTON FAR *, int );
BOOL FASTCALL LoadToolbar( HWND, BOOL );
BOOL FASTCALL SaveToolbar( HWND );
void FASTCALL ComputeStatusBarWidth( void );
void FASTCALL UpdateStatusBarWidths( void );
BOOL FASTCALL Common_OnEraseBkgnd( HWND, HDC );
void FASTCALL Common_ChangeFont( HWND, int );
int FASTCALL Amuser_GetPPPassword( LPCSTR, LPSTR, LPCSTR, LPSTR, int );
int FASTCALL Amuser_WritePPPassword( LPCSTR, LPSTR, LPSTR );
PCAT FASTCALL GetCIXCategory( void );
PTL FASTCALL GetPostmasterFolder( void );
PCL FASTCALL GetNewsFolder( void );
void FASTCALL SetResignRejoin( LPVOID, BOOL );
BOOL FASTCALL IsCompulinkDomain( char * );
void FASTCALL GetSupersetTopicTypes( LPVOID, BOOL *, BOOL * );
//BOOL FASTCALL IsHotlinkPrefix( char *, int * );
BOOL FASTCALL Amctl_IsHotlinkPrefix( char * pszText, int * cbMatch );

/* TBSTATE.C */
void FASTCALL ToolbarState_Permissions( void );
void FASTCALL ToolbarState_EditWindow( void );
void FASTCALL ToolbarState_Printer( void );
void FASTCALL ToolbarState_Import( void );
void FASTCALL ToolbarState_CopyPaste( void );
void FASTCALL ToolbarState_TopicSelected( void );
void FASTCALL ToolbarState_MessageWindow( void );

void FASTCALL CheckMenuStates( void ); // !!SM!! 2.55

/* PAGSETUP.C */
BOOL FASTCALL PageSetupInit( void );
void FASTCALL PageSetupTerm( void );
void FASTCALL CmdPageSetup( HWND );

/* INBASK.C */
BOOL FASTCALL CreateInBasket( HWND );
void FASTCALL CreateNewFolder( void );
void FASTCALL CreateNewCategory( void );
void FASTCALL InBasket_Prop_ShowTitle( HWND, LPVOID );
BOOL FASTCALL InBasket_OpenCommand( HWND, BOOL );
void FASTCALL InBasket_GetNewArticles( HWND );
void FASTCALL InBasket_GetTagged( HWND );
void FASTCALL InBasket_DeleteCommand( HWND );
void FASTCALL InBasket_KeepAtTop( HWND );
void FASTCALL InBasket_PurgeCommand( HWND );
void FASTCALL InBasket_CheckCommand( HWND );
void FASTCALL InBasket_RethreadArticles( HWND );
void FASTCALL InBasket_OnMouseMove( HWND, int, int, UINT );
void FASTCALL InBasket_OnLButtonUp( HWND, int, int, UINT );
void FASTCALL InBasket_OnCancelMode( HWND );
void FASTCALL InBasket_MoveFolder( LPVOID, HWND );
void FASTCALL InBasket_BuildCommand( HWND );
void FASTCALL InBasket_RenameCommand( HWND );
void FASTCALL InBasket_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
void FASTCALL InBasket_OnDragDrop( HWND, UINT, LPDRAGDROPSTRUCT );
void FASTCALL InBasket_OnInsertCategory( HWND, PCAT );
void FASTCALL InBasket_OnInsertFolder( HWND, PCL );
void FASTCALL InBasket_OnInsertTopic( HWND, PTL );
void FASTCALL InBasket_OnDeleteCategory( HWND, BOOL, PCAT );
void FASTCALL InBasket_OnDeleteFolder( HWND, BOOL, PCL );
void FASTCALL InBasket_OnDeleteTopic( HWND, BOOL, PTL );
void FASTCALL InBasket_OnAppColourChange( HWND, int );
void FASTCALL InBasket_OnSetTopic( HWND, PTL );
void FASTCALL InBasket_OnUpdateUnread( HWND, PTL );
LRESULT FASTCALL InBasket_OnNotify( HWND, int, LPNMHDR );
void FASTCALL CmdNewWindow( HWND );
void FASTCALL InBasket_DisplayProperties( HWND, LPVOID, int );
void FASTCALL InBasket_Properties( HWND );
void FASTCALL InBasket_SortCommand( HWND );
HTREEITEM FASTCALL InsertCategory( HWND, PCAT, BOOL, HTREEITEM );
HTREEITEM FASTCALL InsertFolder( HWND, HTREEITEM, PCL, BOOL, HTREEITEM );
HTREEITEM FASTCALL InsertTopic( HWND, HTREEITEM, PTL, HTREEITEM );
BOOL FASTCALL IsCIXFolder( LPVOID );
BOOL FASTCALL IsLocalFolder( LPVOID );
LPVOID FASTCALL CmdMarkTopicRead( HWND );
void FASTCALL OpenOrJoinTopic( PTL, DWORD, BOOL );
void FASTCALL EnumerateFolders( LPVOID, ENUMFOLDERPROC, LPARAM );
BOOL FASTCALL ParseFolderTopicMessage( char *, PTL *, DWORD * );

/* TOPICWND.C */
BOOL FASTCALL OpenTopicViewerWindow( PTL );
BOOL FASTCALL OpenMsgViewerWindow( PTL, PTH, BOOL );
BOOL FASTCALL SetCurrentMsgEx( CURMSG FAR *, BOOL, BOOL, BOOL );
DWORD FASTCALL MarkTopicRead( HWND, PTL, BOOL );
void FASTCALL UpdateCurrentMsgDisplay( HWND );
void FASTCALL UpdateStatusDisplay( HWND );
void FASTCALL ShowMsg( HWND, PTH, BOOL, BOOL, BOOL, char* );
BOOL FASTCALL SetCurrentMsg( CURMSG FAR *, BOOL, BOOL );
LPINT FASTCALL GetThread( HWND );
LPINT FASTCALL GetSelectedItems( HWND );
PTH FAR * FASTCALL GetSelectedItemsHandles( HWND );
void FASTCALL GetMarkedFilename( HWND, LPSTR, int );
void FASTCALL GetMarkedName( HWND, LPSTR, int, BOOL );
void FASTCALL StripOutName( char *, char * );
void FASTCALL DisplayShellExecuteError( HWND, char *, UINT );
BOOL FASTCALL IsLoginAuthor( char * );
LPSTR FASTCALL GetTextBody( PTL, LPSTR );
void FASTCALL InbasketTopicNameChanged( HWND, PTL );
void FASTCALL InbasketFolderNameChanged( HWND, PCL );
void FASTCALL InbasketCategoryNameChanged( HWND, PCAT );
PCL FASTCALL FillConfList( HWND, LPSTR );
PTL FASTCALL FillTopicList( HWND, LPSTR, LPSTR );
PCL FASTCALL FillLocalConfList( HWND, LPSTR );
PTL FASTCALL FillLocalTopicList( HWND, LPSTR, LPSTR );
LRESULT FASTCALL CommonHotspotHandler( HWND, char * ); // !!SM!! 2032 BOOL->LRESULT

/* MAIL.C */
BOOL FASTCALL CheckAttachmentSize( HWND, LPSTR, int );
BOOL FASTCALL CheckValidCixFilenames( HWND, LPSTR );

/* NEWS.C */
BOOL FASTCALL CmdUsenetSettings( HWND );

/* PREFS.C */
BOOL FASTCALL PreferencesDialog( HWND, int );
BOOL FASTCALL MailDialog( HWND, int );
BOOL FASTCALL DirectoriesDialog( HWND, int );
void FASTCALL DrawCheckBox( HDC, int, int, int, SIZE * );
void FASTCALL CheckList_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
BOOL FASTCALL FillRasConnections( HWND, int );

/* SPELLING.C */
void FASTCALL InitSpelling( void );
BOOL EXPORT CALLBACK SpellOpts( HWND, UINT, WPARAM, LPARAM );
void FASTCALL LoadDictionaries( void );
void FASTCALL TermSpelling( void );
int FASTCALL DoSpellingPopup( HWND, HMENU );

/* CUSTOM.C */
BOOL FASTCALL CustomiseDialog( HWND, int );
void FASTCALL DescribeKey( WORD, char *, int );

/* IPCOMMON.C */
BOOL FASTCALL CloseOnlineWindows( HWND, BOOL );
BOOL FASTCALL CloseAllTerminals( HWND, BOOL );
char * FASTCALL ParseFromField( char *, char *, char * );
void FASTCALL CompactToAddress( char * );
BOOL FASTCALL IsServerBusy( void );
BOOL FASTCALL AreTerminalsActive( void );

/* SEARCH.C */
void FASTCALL InstallSavedSearchCommands( void );
void FASTCALL RunSavedSearchCommand( int );
void FASTCALL GlobalFind( HWND );
void FASTCALL FolderListCombo_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
void FASTCALL FolderCombo_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
void FASTCALL FolderCombo_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );

/* PRINTMSG.C */
BOOL FASTCALL PrintMessage( HWND, BOOL );
void FASTCALL UpdatePrinterName( HWND );

/* PURGE.C */
BOOL EXPORT CALLBACK PurgeOptions( HWND, UINT, WPARAM, LPARAM );
BOOL EXPORT CALLBACK DefaultPurgeOptions( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL Purge( HWND, LPVOID, BOOL );
WORD FASTCALL CountTopics( LPVOID );
WORD FASTCALL CountTopicsInCategory( PCAT );
WORD FASTCALL CountTopicsInFolder( PCL );
BOOL FASTCALL CmdPurgeSettings( HWND );

/* DECODE.C */
BOOL FASTCALL DecodeMessage( HWND, BOOL, HPSTR* );
int FASTCALL DecodeLine64( LPSTR, LPSTR, int );

/* SIG.C */
void FASTCALL CmdSignature( HWND, char * );
void FASTCALL FillSignatureList( HWND, int );
void FASTCALL SetEditSignature( HWND, PTL );
LPSTR FASTCALL GetEditSignature( char * );
void FASTCALL ReplaceEditSignature( HWND, char *, char *, BOOL );

/* MSGHOOK.C */
int FASTCALL fMessageBox( HWND, int, LPCSTR, int );

/* MSGDGPRC.C */
int FASTCALL fDlgMessageBox( HWND, int, int, LPSTR, DWORD, int );

/* MKSELVS.C */
void FASTCALL MakeEditSelectionVisible( HWND, HWND );

/* DLGPROCS.C */
void FASTCALL GoToSleep( DWORD );
BOOL FASTCALL WritePrivateProfileInt( LPCSTR, LPSTR, int, LPCSTR );
BOOL FASTCALL fCancelToClose( HWND, int );
void FASTCALL ChangeDefaultButton( HWND, int );
void FASTCALL SendAllWindows( UINT, WPARAM, LPARAM );
BOOL FASTCALL GetDlgInt( HWND, WORD, LPINT, int, int );
BOOL FASTCALL GetDlgLongInt( HWND, WORD, LPLONG, LONG, LONG );
void FASTCALL SetDlgItemFloat( HWND, int, float );
float FASTCALL GetDlgItemFloat( HWND, int );
void FASTCALL SetDlgItemLongInt( HWND, int, DWORD );
LONG FASTCALL GetDlgItemLongInt( HWND, int, BOOL *, BOOL );
void FASTCALL HighlightField( HWND, int );
void FASTCALL AmMessageBeep( void );
int FASTCALL ComboBox_RealFindStringExact( HWND, LPCSTR );
int FASTCALL RealListBox_FindItemData( HWND, int, LPARAM );
BOOL FASTCALL BreakDate( HWND, int, AM_DATE * );
BOOL FASTCALL BreakTime( HWND, int, AM_TIME * );
BOOL EXPORT CALLBACK MsgDlgBoxProc( HWND, UINT, WPARAM, LPARAM );
LPSTR FASTCALL ExpandUnixTextToPCFormat( LPSTR );
int FASTCALL RealComboBox_FindItemData( HWND, int, LPARAM );
void FASTCALL GetOwnerDrawListItemColours( const DRAWITEMSTRUCT *, COLORREF *, COLORREF * );
void FASTCALL SetDlgItemPath( HWND, int, char * );
void FASTCALL AdjustNameToFit( HDC, LPSTR, int, BOOL, SIZE * );
void FASTCALL CreateFollowUpQuote( PTH, RECPTR *, LPSTR );

/* PRINT.C */
HDC FASTCALL CreateCurrentPrinterDC( void );
BOOL FASTCALL IsPrinterDriver( void );
void FASTCALL PrintSetup( HWND );
BOOL FASTCALL UpdatePrinterSelection( BOOL );

/* FONTS.C */
void FASTCALL DeleteAmeolFonts( void );
void FASTCALL CreateAmeolFonts( BOOL );
void FASTCALL DeleteAmeolSystemFonts( void );
void FASTCALL CreateAmeolSystemFonts( void );
void FASTCALL ReadFontData( char *, LOGFONT FAR * );
void FASTCALL WriteFontData( char *, LOGFONT FAR * );

/* SETUPWIZ.C */
BOOL FASTCALL SetupWizard( HWND );
void FASTCALL MakeAmeol2DefaultMailClient( void );
void FASTCALL MakeAmeol2DefaultNewsClient( void );

/* STARTUPWIZ.C */
BOOL FASTCALL CmdStartWiz( HWND );

/* COLOURS.C */
BOOL EXPORT CALLBACK Customise_Colour( HWND, UINT, WPARAM, LPARAM );
void FASTCALL InitColourSettings( void );

/* GRPFILTR.C */
void FASTCALL CmdNewsgroupsFilter( HWND, char * );

/* ENCODE.C */
HPSTR FASTCALL EncodeFiles( HWND, LPSTR, LPSTR, int, int * );
int FASTCALL EncodeLine64( LPSTR, UINT, LPSTR );

/* CUSTTOOL.C */
BOOL FASTCALL CmdCustomiseToolbar( HWND );
BOOL FASTCALL CmdExternalApp( HWND, LPAPPINFO );

/* CUSTFONT.C */
BOOL FASTCALL CmdCustomiseFonts( HWND );
BOOL EXPORT CALLBACK FontSettings( HWND, UINT, WPARAM, LPARAM );
void FASTCALL FillFontStylesAndSizes( HWND );

/* FIND.C */
void FASTCALL CmdFind( HWND );
void FASTCALL CmdReplace( HWND );
BOOL FASTCALL CmdFindNext( FINDREPLACE FAR * );
BOOL FASTCALL CmdReplaceText( FINDREPLACE FAR * );

/* MAILBOX.C */
BOOL FASTCALL CreateMailbox( PTL, char * );
void FASTCALL CreateNewMailFolder( HWND );
void FASTCALL CreateMailAddress( char *, char *, char *, char * );
BOOL FASTCALL IsValidFolderName( HWND, char * );

/* ADDONS.C */
BOOL FASTCALL CmdConfigureAddons( HWND );
int FASTCALL CountInstalledAddons( void );
void FASTCALL RunExternalApp( int );
void FASTCALL GetExternalApp( int, LPAPPINFO );
void FASTCALL EditExternalApp( HWND, int );
int FASTCALL InstallExternalApp( LPAPPINFO );
void FASTCALL CmdAddons( HWND );
void FASTCALL UnloadAllAddons( void );
void FASTCALL InstallAllAddons( HWND );
void FASTCALL LoadAllAddons( void );
void FASTCALL SendAddonCommand( int );
HBITMAP FASTCALL ExpandBitmap( HBITMAP, int, int );
HBITMAP FASTCALL IconToBitmap( HICON, int, int );
BOOL FASTCALL GetAddonSupportURL( HINSTANCE, char *, int );
UINT FASTCALL GetRunAppCommandID( LPSTR );

/* IMPORT.C */
BOOL FASTCALL CmdImport( HWND );

/* PARSE.C */
BOOL EXPORT WINAPI CIXImport( HWND, LPSTR );
BOOL FASTCALL CIXImportAndArchive( HWND, LPSTR, WORD );
void FASTCALL CIXImportParseCompleted( void );
int FASTCALL CountUnparsedScratchpads( void );
void FASTCALL HandleUnparsedScratchpads( HWND, BOOL );
void FASTCALL UpdateParsingStatusBar( void );
BOOL FASTCALL DequeueScratchpad( void );
void FASTCALL ClearScratchpadQueue( void );
BOOL FASTCALL BuildImport( HWND, PTL );
BOOL FASTCALL UnarcFile( HWND, char * );

/* EXPORT.C */
BOOL FASTCALL CmdExport( HWND );

/* SCREXPRT.C */
BOOL EXPORT WINAPI CIXExport( HWND );
void FASTCALL EnableScrExportOptions( HWND, LPVOID );

/* CIXIP.C */
void FAR PASCAL InsertMailMenu(void);

/* EVENT.C */
BOOL FASTCALL CmdEventLog( HWND );

/* COMMS.C */
void FASTCALL FillModemList( HWND, int );
void FASTCALL FillPortsList( HWND, int );
void FASTCALL FillSpeedsList( HWND, int );
void FASTCALL FillConnectionCards( HWND, int, char * );
int FASTCALL ReadIPNumber( LPSTR *, BOOL * );
BOOL EXPORT CALLBACK ModemSettings( HWND, UINT, WPARAM, LPARAM );

/* CONNMAN.C */
void FASTCALL CmdConnections( HWND );
BOOL FASTCALL AutoDetect( HWND, int, int, int );

/* FILENAME.C */
BOOL FASTCALL ValidFileNameChr( char );
void FASTCALL QuotifyFilename( char * );
char * FASTCALL ExtractFilename( char *, char * );
char * FASTCALL OldExtractFilename( char *, char * );
BOOL FASTCALL IsWild( char * );
BOOL FASTCALL IsValidDOSFileName( LPSTR );
BOOL FASTCALL IsValidCIXFileName( LPSTR );
LPSTR FASTCALL GetFileBasename( LPSTR );
void FASTCALL GetFilePathname( LPSTR, LPSTR );
int FASTCALL ChangeExtension( LPSTR, LPSTR );
BOOL FASTCALL CommonGetOpenFilename( HWND, char *, char *, char *, char * );

/* DIRASSOC.C */
BOOL EXPORT CALLBACK DirAssociate( HWND, UINT, WPARAM, LPARAM );
void FASTCALL GetAssociatedDir( PTL, char *, char * );

/* DIRECTRY.C */
BOOL EXPORT CALLBACK Directories( HWND, UINT, WPARAM, LPARAM );

/* SCRIPT.C */
void FASTCALL LoadScriptList( void );
void FASTCALL CmdScripts( HWND );
void FASTCALL FillScriptsList( HWND, int );

/* PRNTCOMP.C */
void FASTCALL CmdComposedPrint( HWND );

/* BUILDSCR.C */
BOOL FASTCALL CompileScript( HWND, LPSCRIPT );
void FASTCALL MakePath( LPSTR, LPSTR, LPSTR );

/* ERRORS.C */
BOOL FASTCALL OutOfMemoryError( HWND, BOOL, BOOL );
BOOL FASTCALL DiskWriteError( HWND, char *, BOOL, BOOL );
BOOL FASTCALL DiskReadError( HWND, char *, BOOL, BOOL );
BOOL FASTCALL FileOpenError( HWND, char *, BOOL, BOOL );
BOOL FASTCALL FileCreateError( HWND, char *, BOOL, BOOL );
BOOL FASTCALL DiskSaveError( HWND, char *, BOOL, BOOL );
void FASTCALL NotImplementedYet( HWND );

/* REPORT.C */
BOOL FASTCALL ReportWindow( HWND );
BOOL FASTCALL GetOSDisplayString( LPTSTR pszOS);

/* GALLERY.C */
void FASTCALL EnableGallery( void );
void FASTCALL CmdMugshotEdit( HWND );
void FASTCALL EnsureMugshotIndex( void );

/* BLINK.C */
BOOL FASTCALL EndBlink( void );
void FASTCALL EndProcessing( OBCLSID );
BOOL FASTCALL Blink( OBCLSID );
void FASTCALL EndOnlineBlink( void );
void FASTCALL DestroyBlinkWindow( void );
void FASTCALL FailedExec( void );
void FASTCALL StartBlinkAnimation( void );
void FASTCALL EndBlinkAnimation( void );
void FASTCALL StepBlinkAnimation( void );
void FASTCALL FinishStepingBlinkGauge( int pCurrent ); /*!!SM!!*/
void FASTCALL ResetBlinkGauge( void );
void FASTCALL HideBlinkGauge( void );
void FASTCALL StepBlinkGauge( int );
void FASTCALL ShowDownloadCount( DWORD pAmount, LPSTR pText );// !!SM!!
void FASTCALL ClearOldPendingDowntimes( void );
BOOL FASTCALL CheckForDowntime( HWND );
void FASTCALL SwitchService( int );
void FASTCALL RemoveObItem( int );

/* BLINKMAN.C */
void FASTCALL CmdCustomBlink( HWND );
void FASTCALL LoadBlinkList( void );
int FASTCALL PutUniqueAccelerator( HMENU, char * );
BOOL FASTCALL BlinkByCmd( UINT );
void FASTCALL CmdBlinkMan( HWND );
UINT FASTCALL GetBlinkCommandID( LPSTR );
BOOL FASTCALL GetBlinkCommandName( UINT, char * );
void FASTCALL EnableBlinkButtons( BOOL );
void FASTCALL EnableBlinkMenu( HMENU, BOOL );
void FASTCALL EditBlinkProperties( HWND, UINT );
void FASTCALL UpdateBlinkBitmaps( void );

/* SCHEDULE.C */
BOOL FASTCALL CmdScheduler( HWND );
void FASTCALL LoadScheduledItems( void );
void FASTCALL InvokeScheduler( void );
void FASTCALL RunScheduledStartupActions( void );

/* SORTFROM.C */
void FASTCALL CreateSortMailFrom( HWND );

/* SORTTO.C */
void FASTCALL CreateSortMailTo( HWND );

/* AMDBIMP.C */
void EXPORT WINAPI AmdbImportWizard( HWND );

/* NEWREG.C */
int FASTCALL RegisterWizard( void );

/* CHKFILT.C */
void FASTCALL CheckFilters( void );

/* SOUND.C */
void FASTCALL Am_PlaySound( LPSTR );
