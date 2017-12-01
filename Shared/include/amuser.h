/* AMUSER.H - Ameol user module API and definitions
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

#ifndef _AMUSER_H

#ifndef RC_INVOKED

/* MUST start from zero */
#define  AE_FIRSTEVENT                          0

/* Main database events.
 * THE FOLLOWING EVENT NUMBERS (0..11) MUST NOT BE CHANGED IF
 * BACKWARD COMPATIBILITY WITH AMEOL IS TO BE MAINTAINED! swp
 */
#define  AE_CONNECTSTART                        AE_FIRSTEVENT
#define  AE_CONNECTEND                          AE_FIRSTEVENT+1
#define  AE_NEWFOLDER                           AE_FIRSTEVENT+2
#define  AE_NEWTOPIC                            AE_FIRSTEVENT+3
#define  AE_NEWMSG                              AE_FIRSTEVENT+4
#define  AE_DELETEFOLDER                        AE_FIRSTEVENT+5
#define  AE_DELETETOPIC                         AE_FIRSTEVENT+6
#define  AE_DELETEMSG                           AE_FIRSTEVENT+7
#define  AE_MSGCHANGE                           AE_FIRSTEVENT+8
#define  AE_ADDNEWMSG                           AE_FIRSTEVENT+10
#define AE_COMMAND                              AE_FIRSTEVENT+11
#define  AE_NEWCATEGORY                         AE_FIRSTEVENT+12
#define  AE_DELETECATEGORY                      AE_FIRSTEVENT+13
#define  AE_MOVEFOLDER                          AE_FIRSTEVENT+14
#define  AE_UNREADCHANGED                       AE_FIRSTEVENT+15
#define  AE_CLOSEDATABASE                       AE_FIRSTEVENT+16
#define  AE_MOVETOPIC                           AE_FIRSTEVENT+21
#define  AE_MOVECATEGORY                        AE_FIRSTEVENT+22
#define AE_PARSE_COMPLETE                       AE_FIRSTEVENT+56
#define AE_PARSED_ONE                           AE_FIRSTEVENT+57
#define AE_DISCONNECTWAITING                    AE_FIRSTEVENT+58

/* Newsgroup database events.
 */
#define  AE_GRPDB_DELETING                      AE_FIRSTEVENT+17
#define  AE_GRPDB_DELETED                       AE_FIRSTEVENT+18
#define  AE_GRPDB_INSERTING                     AE_FIRSTEVENT+19
#define  AE_GRPDB_INSERTED                      AE_FIRSTEVENT+20
#define  AE_GRPDB_COMMITTING                    AE_FIRSTEVENT+25
#define  AE_GRPDB_COMMITTED                     AE_FIRSTEVENT+26

/* UI events.
 */
#define  AE_INIT_MAIN_MENU                      AE_FIRSTEVENT+27
#define  AE_POPUP                               AE_FIRSTEVENT+28
#define  AE_MDI_WINDOW_ACTIVATED                AE_FIRSTEVENT+29
#define  AE_CONTEXTSWITCH                       AE_FIRSTEVENT+30
#define  AE_PREFSDIALOG                         AE_FIRSTEVENT+31
#define  AE_PROPERTIESDIALOG                    AE_FIRSTEVENT+32
#define  AE_INBASK_FOLDER_DELETING              AE_FIRSTEVENT+33
#define  AE_DIALOG_INIT                         AE_FIRSTEVENT+34
#define  AE_BLINKPROPERTIESDIALOG               AE_FIRSTEVENT+35
#define  AE_SWITCHING_SERVICE                   AE_FIRSTEVENT+36
#define  AE_SWITCHED_SERVICE                    AE_FIRSTEVENT+37
#define  AE_CONFIGADDONDLG                      AE_FIRSTEVENT+38
#define  AE_MAINPREFSDIALOG                     AE_FIRSTEVENT+59

/* Event log events.
 */
#define  AE_EVENTLOG_CHANGED                    AE_FIRSTEVENT+40
#define  AE_EVENTLOG_CLEARED                    AE_FIRSTEVENT+41

/* Out-basket events.
 */
#define  AE_INSERTED_OUTBASKET_OBJECT           AE_FIRSTEVENT+43
#define  AE_INSERTING_OUTBASKET_OBJECT          AE_FIRSTEVENT+44
#define  AE_DELETED_OUTBASKET_OBJECT            AE_FIRSTEVENT+45
#define  AE_DELETING_OUTBASKET_OBJECT           AE_FIRSTEVENT+46
#define  AE_EDITING_OUTBASKET_OBJECT            AE_FIRSTEVENT+47
#define  AE_EXECUTING_OUTBASKET_OBJECT          AE_FIRSTEVENT+48
#define  AE_EDITED_OUTBASKET_OBJECT             AE_FIRSTEVENT+49

/* Database modification events.
 */
#define  AE_TOPIC_CHANGED                       AE_FIRSTEVENT+50
#define  AE_FOLDER_CHANGED                      AE_FIRSTEVENT+51
#define  AE_CATEGORY_CHANGED                    AE_FIRSTEVENT+52
#define  AE_TOPIC_CHANGING                      AE_FIRSTEVENT+53
#define  AE_FOLDER_CHANGING                     AE_FIRSTEVENT+54
#define  AE_CATEGORY_CHANGING                   AE_FIRSTEVENT+55

#define  AE_LASTEVENT                           AE_FIRSTEVENT+60
#define  MAX_EVENTS                             ((AE_LASTEVENT-AE_FIRSTEVENT)+1)

/* If the AE_xxx_CHANGED event is sent, the following flags
 * are passed in the lParam2 to specify WHAT changed.
 */
#define  AECHG_NAME           0x0001
#define  AECHG_FLAGS          0x0002

/* If the AE_CONTEXTSWITCH event is sent, the following flags are
 * passed in the lParam1 to specify the context.
 */
#define  AECSW_ONLINE         0x0001

/* AE_POPUP structure.
 */
typedef struct tagAEPOPUP {
   WORD wType;                /* Type of window invoking popup */
   LPVOID pFolder;            /* Folder handle (may be NULL) */
   LPSTR pSelectedText;       /* Pointer to a copy of any selected text (may be NULL) */
   int cchSelectedText;       /* Length of any selected text */
   HMENU hMenu;               /* Handle of popup menu */
   int nInsertPos;            /* Insertion position */
} AEPOPUP, FAR * LPAEPOPUP;

typedef BOOL (WINAPI * LPFNEEVPROC)( int, LPARAM, LPARAM );

void WINAPI UnregisterAllEvents( void );
BOOL WINAPI Amuser_RegisterEvent( int, LPFNEEVPROC );
BOOL WINAPI Amuser_UnregisterEvent( int, LPFNEEVPROC );
BOOL WINAPI Amuser_CallRegistered( int, LPARAM, LPARAM );

/* Mugshot functions.
 */
HBITMAP WINAPI Amuser_GetMugshot( char *, HPALETTE FAR * );
int WINAPI Amuser_MapUserToMugshotFilename( char *, char *, int );
void WINAPI Amuser_DeleteUserMugshot( char * );
void WINAPI Amuser_AssignUserMugshot( char *, char * );

/* Graphic functions.
 */
COLORREF WINAPI Amuser_SetTransparentColour( COLORREF );
void WINAPI Amuser_DrawBitmap( HDC, int, int, int, int, HBITMAP, int );
HBITMAP WINAPI Amuser_LoadBitmapFromDisk( LPSTR, HPALETTE FAR * );
HBITMAP WINAPI Amuser_LoadGIFFromDisk( LPSTR, HPALETTE FAR * );

/* Structure for ChangeDirectory()
 */
#pragma pack(1)
typedef struct tagCHGDIR {
   HWND hwnd;                    // Parent window handle
   char szTitle[ 40 ];           // Window title
   char szPrompt[ 40 ];          // Prompt for directory edit field
   char szPath[ 144 ];           // Pathname
} CHGDIR;
#pragma pack()

typedef CHGDIR FAR * LPCHGDIR;
                              
BOOL WINAPI Amuser_ChangeDirectory( LPCHGDIR );

typedef long HFIND;
typedef struct _finddata32_t FINDDATA;

void WINAPI Amuser_WriteWindowState( LPSTR, HWND );
void WINAPI Amuser_ReadWindowState( LPSTR, LPRECT, DWORD FAR * );
void WINAPI Amuser_ParseWindowState( LPSTR, LPRECT, DWORD FAR * );
BOOL WINAPI Amuser_CreateWindowState( LPSTR, HWND );

HFIND WINAPI Amuser_FindFirst( LPSTR, UINT, FINDDATA FAR * );
HFIND WINAPI Amuser_FindNext( HFIND, FINDDATA FAR * );
HFIND WINAPI Amuser_FindClose( HFIND );

typedef struct tagINTERFACE_PROPERTIES {
   HWND hwndFrame;                  /* Handle of frame window */
   HWND hwndMDIClient;              /* Handle of MDI client window */
} INTERFACE_PROPERTIES;

typedef INTERFACE_PROPERTIES FAR * LPINTERFACE_PROPERTIES;

void WINAPI SetINIMode(BOOL pUseINIfile);       /* Shoulw we read INI via file or registry // 2.56.2051 FS#121*/
void WINAPI SetU3Mode(BOOL pUseU3); // 2.56.2053

void WINAPI Amuser_SetInterfaceProperties( LPINTERFACE_PROPERTIES );
void WINAPI Amuser_GetInterfaceProperties( LPINTERFACE_PROPERTIES );
HMENU WINAPI Amuser_GetNewPopupMenu( void );
int WINAPI Amuser_GetActiveUser( LPSTR, int );
void WINAPI Amuser_SetActiveUser( LPSTR );
int WINAPI Amuser_DeleteUser( LPSTR );

typedef struct tagPRECTLCOLORSTRUCT {
   int wm;
   WPARAM wParam;
   LPARAM lParam;
   BOOL fOverride;
} PRECTLCOLORSTRUCT;

#define  WM_ADMHELP           WM_USER+201
#define  WM_PRECTLCOLOR       WM_USER+202
#define  WM_ADJUSTWINDOWS     WM_USER+203

/* void Cls_OnAdmHelp(HWND hwnd); */
#define HANDLE_WM_ADMHELP(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd)), 0L)
#define FORWARD_WM_ADMHELP(hwnd, fn) \
    (void)(fn)((hwnd), WM_ADMHELP, 0, 0L)

/* void Cls_OnAdjustWindows(HWND hwnd, int dx, int dy); */
#define HANDLE_WM_ADJUSTWINDOWS(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam)), 0L)
#define FORWARD_WM_ADJUSTWINDOWS(hwnd, dx, dy, fn) \
    (void)(fn)((hwnd), WM_ADJUSTWINDOWS, 0, MAKELPARAM((dx),(dy)))

#define  WBH_SETFONT          0x0001

BOOL WINAPI Adm_Init( HINSTANCE );
BOOL WINAPI Adm_Exit( HINSTANCE );

int WINAPI Adm_Dialog( HINSTANCE, HWND, LPCSTR, DLGPROC, LPARAM );
HWND WINAPI Adm_CreateDialog( HINSTANCE, HWND, LPCSTR, DLGPROC, LPARAM );
HWND WINAPI Adm_Billboard( HINSTANCE, HWND, LPCSTR );
void WINAPI Adm_SysColorChange( void );
DWORD WINAPI Adm_DrawDialog( HWND, LPCSTR );
BOOL WINAPI Adm_EnableDialogSubclassing( BOOL );
void WINAPI Adm_InflateWnd( HWND, int, int );
void WINAPI Adm_MoveWnd( HWND, int, int );

/* MDI dialog functions.
 */
BOOL WINAPI Adm_DeferMDIResizing( HWND, BOOL );
void WINAPI Adm_MakeMDIWindowActive( HWND );
void WINAPI Adm_GetMDIClientRect( LPRECT );
HWND WINAPI Adm_CreateMDIWindow( LPCSTR, LPCSTR, HINSTANCE, LPRECT, DWORD, LPARAM );
void WINAPI Adm_DestroyMDIWindow( HWND );
HWND WINAPI Adm_MDIDialog( HWND, LPCSTR, LPMDICREATESTRUCT );
LRESULT WINAPI Adm_DefMDIDlgProc( HWND, UINT, WPARAM, LPARAM );
BOOL WINAPI Adm_GetMDIDialogRect( HINSTANCE, LPCSTR, LPRECT );

#define  ADM_INIT_ERROR             -4

#define  MB_YESNOALL       0x0006
#define  MB_YESNOALLCANCEL 0x0007
#define  MB_EX_HELP        1
#define  MB_EX_DEFBUTTON4  0x0100
#define  IDALL             8
#define  IDEXHELP          300

int WINAPI Adm_MsgBox( HWND, LPSTR, LPSTR, UINT, UINT, WNDPROC );

/* Directory API.
 */
#define  APDIR_ADDONS         0
#define  APDIR_MUGSHOTS       1
#define  APDIR_UPLOADS        2
#define  APDIR_DOWNLOADS         3
#define  APDIR_DATA           4
#define  APDIR_USER           5
#define  APDIR_APPDIR         6
#define  APDIR_ACTIVEUSER     7
#define  APDIR_ARCHIVES       8
#define  APDIR_SCRIPTS        9
#define  APDIR_SCRATCHPADS    10
#define  APDIR_RESUMES        11
#define  APDIR_ATTACHMENTS    13

#define  Amuser_GetAddonsDirectory(lpbuf,max)         Amuser_GetAppDirectory(APDIR_ADDONS,(lpbuf),(max))
#define  Amuser_GetMugshotsDirectory(lpbuf,max)       Amuser_GetAppDirectory(APDIR_MUGSHOTS,(lpbuf),(max))
#define  Amuser_GetUploadsDirectory(lpbuf,max)        Amuser_GetAppDirectory(APDIR_UPLOADS,(lpbuf),(max))
#define  Amuser_GetDownloadsDirectory(lpbuf,max)      Amuser_GetAppDirectory(APDIR_DOWNLOADS,(lpbuf),(max))
#define  Amuser_GetDataDirectory(lpbuf,max)           Amuser_GetAppDirectory(APDIR_DATA,(lpbuf),(max))
#define  Amuser_GetUserDirectory(lpbuf,max)           Amuser_GetAppDirectory(APDIR_USER,(lpbuf),(max))
#define  Amuser_GetActiveUserDirectory(lpbuf,max)     Amuser_GetAppDirectory(APDIR_ACTIVEUSER,(lpbuf),(max))
#define  Amuser_GetArchiveDirectory(lpbuf,max)        Amuser_GetAppDirectory(APDIR_ARCHIVES,(lpbuf),(max))
#define  Amuser_GetScriptsDirectory(lpbuf,max)        Amuser_GetAppDirectory(APDIR_SCRIPTS,(lpbuf),(max))
#define  Amuser_GetScratchpadsDirectory(lpbuf,max)    Amuser_GetAppDirectory(APDIR_SCRATCHPADS,(lpbuf),(max))
#define  Amuser_GetResumesDirectory(lpbuf,max)        Amuser_GetAppDirectory(APDIR_RESUMES,(lpbuf),(max))
#define  Amuser_GetAttachmentsDirectory(lpbuf,max)    Amuser_GetAppDirectory(APDIR_ATTACHMENTS,(lpbuf),(max))

#define  Amuser_SetAddonsDirectory(lpbuf)             Amuser_SetAppDirectory(APDIR_ADDONS,(lpbuf))
#define  Amuser_SetMugshotsDirectory(lpbuf)           Amuser_SetAppDirectory(APDIR_MUGSHOTS,(lpbuf))
#define  Amuser_SetUploadsDirectory(lpbuf)            Amuser_SetAppDirectory(APDIR_UPLOADS,(lpbuf))
#define  Amuser_SetDownloadsDirectory(lpbuf)          Amuser_SetAppDirectory(APDIR_DOWNLOADS,(lpbuf))
#define  Amuser_SetDataDirectory(lpbuf)               Amuser_SetAppDirectory(APDIR_DATA,(lpbuf))
#define  Amuser_SetUserDirectory(lpbuf)               Amuser_SetAppDirectory(APDIR_USER,(lpbuf))
#define  Amuser_SetArchiveDirectory(lpbuf)            Amuser_SetAppDirectory(APDIR_ARCHIVES,(lpbuf))
#define  Amuser_SetScriptsDirectory(lpbuf)            Amuser_SetAppDirectory(APDIR_SCRIPTS,(lpbuf))
#define  Amuser_SetScratchpadsDirectory(lpbuf)        Amuser_SetAppDirectory(APDIR_SCRATCHPADS,(lpbuf))
#define  Amuser_SetResumesDirectory(lpbuf)            Amuser_SetAppDirectory(APDIR_RESUMES,(lpbuf))
#define  Amuser_SetAttachmentsDirectory(lpbuf)        Amuser_SetAttachmentsDirectory(APDIR_ATTACHMENTS,(lpbuf))

int WINAPI Amuser_GetAppDirectory( int, LPSTR, int );
BOOL WINAPI Amuser_SetAppDirectory( int, LPSTR );

/* Memory allocation - moved from Amlib
 */
BOOL WINAPI Amuser_AllocMemory( LPVOID FAR *, UINT );
BOOL WINAPI Amuser_ReallocMemory( LPVOID FAR *, UINT );
void WINAPI Amuser_FreeMemory( LPVOID FAR * );

#define fNewMemory            Amuser_AllocMemory
#define fResizeMemory         Amuser_ReallocMemory
#define FreeMemory            Amuser_FreeMemory
#define fNewSharedMemory      Amuser_AllocMemory
#define fResizeSharedMemory      Amuser_ReallocMemory
#define fNewMemory32       Amuser_AllocMemory
#define fNewSharedMemory32    Amuser_AllocMemory
#define fResizeMemory32       Amuser_ReallocMemory
#define FreeMemory32       Amuser_FreeMemory

/* Profile API.
 */

/* Functions to read from user configuration.
 */
int WINAPI Amuser_GetPPInt( LPCSTR, LPSTR, int );
LONG WINAPI Amuser_GetPPLong( LPCSTR, LPSTR, long );
float WINAPI Amuser_GetPPFloat( LPCSTR, LPSTR, float );
int WINAPI Amuser_GetPPString( LPCSTR, LPSTR, LPCSTR, LPSTR, int );
int WINAPI Amuser_GetPPArray( LPCSTR, LPSTR, LPINT, int );

/* Functions to write to user configuration.
 */
int WINAPI Amuser_WritePPInt( LPCSTR, LPSTR, int );
int WINAPI Amuser_WritePPLong( LPCSTR, LPSTR, long );
int WINAPI Amuser_WritePPFloat( LPCSTR, LPSTR, float );
int WINAPI Amuser_WritePPString( LPCSTR, LPSTR, LPSTR );
int WINAPI Amuser_WritePPArray( LPCSTR, LPSTR, LPINT, int );

/* Functions to read from local machine configuration.
 */
int WINAPI Amuser_GetLMString( LPCSTR, LPSTR, LPCSTR, LPSTR, int );
int WINAPI Amuser_GetLMInt( LPCSTR, LPSTR, int );
int WINAPI Amuser_WriteLMString( LPCSTR, LPSTR, LPSTR );

/* Functions to read private profile or registry.
 */
float WINAPI Amuser_GetPrivateProfileFloat( LPCSTR, LPSTR, float, LPCSTR );
int WINAPI Amuser_GetPrivateProfileInt( LPCSTR, LPSTR, int, LPCSTR );
LONG WINAPI Amuser_GetPrivateProfileLong( LPCSTR, LPSTR, long, LPCSTR );
int WINAPI Amuser_GetPrivateProfileString( LPCSTR, LPSTR, LPCSTR, LPSTR, int, LPCSTR );

/* Functions to write private profile or registry.
 */
int WINAPI Amuser_WritePrivateProfileFloat( LPCSTR, LPSTR, float, LPCSTR );
int WINAPI Amuser_WritePrivateProfileInt( LPCSTR, LPSTR, int, LPCSTR );
int WINAPI Amuser_WritePrivateProfileLong( LPCSTR, LPSTR, long, LPCSTR );
int WINAPI Amuser_WritePrivateProfileString( LPCSTR, LPSTR, LPSTR, LPCSTR );

/* Functions to read the general registry.
 */
void WINAPI Amuser_SaveRegistry( LPCSTR );
int WINAPI ReadRegistryKey( HKEY, LPCSTR, LPSTR, LPCSTR, LPSTR, int );

/* Encyption/decryption functions.
 */
BOOL WINAPI Amuser_Encrypt( LPSTR, LPBYTE );
BOOL WINAPI Amuser_Decrypt( LPSTR, LPBYTE );

/* Multi-monitor functions.
 */
void MakeSureWindowIsVisible(HWND hwnd);
BOOL IsWindowOnScreen(HWND hwnd);
void ClipWindowToMonitor(HWND hwndP, HWND hwnd, BOOL fWork);
void CenterWindowToMonitor(HWND hwndP, HWND hwnd, BOOL fWork);
void CenterRectToMonitor(HWND hwnd, RECT *prc, BOOL fWork);
void ClipRectToMonitor(HWND hwnd, RECT *prc, BOOL fWork);
void GetMonitorRect(HWND hwnd, LPRECT prc, BOOL fWork);

/* File info functions.
 */
#pragma pack(1)
typedef struct tagFILEINFO {
   WORD wType;
   WORD wVersion;
} FILEINFO;
#pragma pack()

/* File I/O functions.
 */
#define  AOF_READ                0x0001
#define  AOF_WRITE               0x0002
#define  AOF_READWRITE           (AOF_READ|AOF_WRITE)
#define  AOF_SHARE_READ          0x0010
#define  AOF_SHARE_WRITE         0x0020
#define  AOF_SHARE_READWRITE     (AOF_SHARE_READ|AOF_SHARE_WRITE)
#define  AOF_SEQUENTIAL_IO       0x0100
#define  AOF_RANDOM_IO           0x0200

#define  ASEEK_CURRENT     0
#define  ASEEK_BEGINNING   1
#define  ASEEK_END         2

typedef HANDLE HNDFILE;
#define HNDFILE_ERROR   INVALID_HANDLE_VALUE

HNDFILE WINAPI Amfile_Open( LPCSTR, int );
HNDFILE WINAPI Amfile_Create( LPCSTR, int );
BOOL WINAPI Amfile_Delete( LPCSTR );
BOOL WINAPI Amfile_Rename( LPCSTR, LPCSTR );
void WINAPI Amfile_Close( HNDFILE );
UINT WINAPI Amfile_Read( HNDFILE, LPVOID, UINT );
DWORD WINAPI Amfile_GetPosition( HNDFILE );
DWORD WINAPI Amfile_Read32( HNDFILE, LPVOID, DWORD );
UINT WINAPI Amfile_Write( HNDFILE, LPCSTR, UINT );
DWORD WINAPI Amfile_Write32( HNDFILE, LPCSTR, DWORD );
LONG WINAPI Amfile_SetPosition( HNDFILE, LONG, int );
LONG WINAPI Amfile_GetFileSize( HNDFILE );
BOOL WINAPI Amfile_Lock( HNDFILE, DWORD, DWORD );
BOOL WINAPI Amfile_Unlock( HNDFILE, DWORD, DWORD );
void WINAPI Amfile_SetFileTime( HNDFILE, WORD, WORD );
void WINAPI Amfile_GetFileTime( HNDFILE, WORD FAR *, WORD FAR * );
void WINAPI Amfile_GetFileLocalTime( HNDFILE, WORD FAR *, WORD FAR * );

BOOL WINAPI Amfile_QueryFile( LPCSTR );
BOOL WINAPI Amfile_IsSharingLoaded( void );
BOOL WINAPI Amfile_Copy( LPSTR, LPSTR );

/* Uncompression functions.
 */
BOOL WINAPI Amcmp_IsArcedFile( LPSTR );
BOOL WINAPI Amcmp_UncompressFile( HWND, LPSTR, LPSTR );

/* Directory functions.
 */
DWORD WINAPI Amdir_FreeDiskSpace( char * );
BOOL WINAPI Amdir_Create( LPCSTR );
BOOL WINAPI Amdir_Remove( LPCSTR );
char WINAPI Amdir_GetDrive( LPSTR );
BOOL WINAPI Amdir_SetCurrentDrive( char );
BOOL WINAPI Amdir_QueryDirectory( LPCSTR );
DWORD WINAPI Amdir_GetCurrentDirectory( LPSTR, DWORD );
BOOL WINAPI Amdir_SetCurrentDirectory( char *);
void WINAPI Amdir_ExtractRoot( char *, char *, int );

/* Filename conversion functions.
 */
BOOL WINAPI Amuser_CreateCompatibleFilename( LPCSTR, LPSTR );

/* Date and time functions.
 */
typedef struct tagAM_DATE {
   WORD iYear;             /* Current year (1980..2099) */
   WORD iMonth;            /* Current month 1..12 */
   WORD iDay;              /* Current day of month 1..31 */
   WORD iDayOfWeek;        /* Day of week (0=Sunday, 6=Saturday), 0..6 */
} AM_DATE;

typedef struct tagAM_TIME {
   WORD iHour;             /* Hour, in 24-hour format (00 = 12.00am) */
   WORD iMinute;           /* Minutes, 0..59 */
   WORD iSecond;           /* Seconds, 0..59 */
   WORD iHSeconds;         /* Hundreths of a second, 0..99 */
} AM_TIME;

typedef struct tagAM_TIMEZONEINFO {
   char szZoneName[ 32 ];  /* Zone name (BST, GMT, etc) */
   UINT diff;              /* Difference from UTC */
} AM_TIMEZONEINFO;

void WINAPI Amdate_RefreshDateIniSettings( LPCSTR );
void WINAPI Amdate_GetCurrentDate( AM_DATE FAR * );
WORD WINAPI Amdate_GetPackedCurrentDate( void );
WORD WINAPI Amdate_PackDate( AM_DATE FAR * );
void WINAPI Amdate_UnpackDate( WORD, AM_DATE FAR * );
LPSTR WINAPI Amdate_FormatLongDate( AM_DATE FAR *, LPSTR );
LPSTR WINAPI Amdate_FormatShortDate( AM_DATE FAR *, LPSTR );
LPSTR WINAPI Amdate_FormatFullDate( AM_DATE FAR *, AM_TIME FAR *, LPSTR, LPSTR );
void WINAPI Amdate_AdjustDate( AM_DATE FAR *, int );
void WINAPI Amdate_GetTimeZoneInformation( AM_TIMEZONEINFO * );

void WINAPI Amdate_RefreshTimeIniSettings( LPCSTR );
void WINAPI Amdate_GetCurrentTime( AM_TIME FAR *);
WORD WINAPI Amdate_GetPackedCurrentTime( void );
WORD WINAPI Amdate_PackTime( AM_TIME FAR * );
void WINAPI Amdate_UnpackTime( WORD, AM_TIME FAR * );
LPSTR WINAPI Amdate_FormatTime( AM_TIME FAR *, LPSTR );
LPSTR WINAPI Amdate_FormatLongTime( DWORD );
char * WINAPI Amdate_FriendlyDate( char *, WORD, WORD );

/* Compute CRC function.
 */
WORD WINAPI Amuser_ComputeCRC( LPBYTE, UINT );

/* Load a service DLL.
 */
HINSTANCE WINAPI Amuser_LoadService( char * );

#endif

#define  IDD_HELP             6002

#define _AMUSER_H
#endif
