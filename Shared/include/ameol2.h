/* AMEOL2.H - APIs exported by Ameol main program
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

#ifndef _AMEOL2_H

#include "amdb.h"

/* Old Ameol v1 menu structure.
 */
#pragma pack(1)
typedef struct tagAMEOLMENU {
   HINSTANCE hInst;                       /* Addon instance handle */
   short wPopupMenu;                      /* Offset of popup menu */
   short wMenuPos;                        /* Offset on menu */
   LPCSTR lpMenuText;                     /* Pointer to string for menu */
   LPCSTR lpMenuHelpText;                 /* Pointer to string for menu description */
   WORD wKey;                             /* Encoded key */
   HMENU hMenu;                           /* Output: Ameol menu bar handle */
   WORD wID;                              /* Output: Ameol command ID */
} AMEOLMENU;
#pragma pack()

#pragma pack(1)
typedef struct tagADDONINFO {
   HINSTANCE hLib;                        /* Addon library handle */
   DWORD dwVersion;                       /* Version number, in Ameol format */
   char szFileName[ 13 ];                 /* File name */
   char szAuthor[ 40 ];                   /* Author's identification field */
   char szDescription[ 100 ];             /* Product description field */
} ADDONINFO;
#pragma pack()

#pragma pack(1)
typedef struct tagSUPPORT_URL {
   BOOL fCIXService;                      /* TRUE if CIX service installed */
   BOOL fIPService;                       /* TRUE if IP service installed */
   LPSTR lpURLStr;                        /* Pointer to URL buffer */
   UINT cchURLStr;                        /* Size of URL buffer */
} SUPPORT_URL;
#pragma pack()

/* QueryAddon interface
 */
#define  QUERY_CMD_BUTTON_BITMAP       0x0001
#define  QUERY_SUPPORT_URL             0x0002

#pragma pack(1)
typedef struct tagCMDBUTTONBITMAP {
   WORD wID;                           /* ID of command */
   HINSTANCE hLib;                     /* Library handle */
   UINT uBitmap;                       /* Bitmap handle or ID */
   int cxRequired;                     /* Width of bitmap required */
   int cyRequired;                     /* Height of bitmap required */
} CMDBUTTONBITMAP;
#pragma pack()

typedef CMDBUTTONBITMAP FAR * LPCMDBUTTONBITMAP;
   
typedef ADDONINFO FAR * LPADDONINFO;
typedef struct tagADDON FAR * LPADDON;

typedef struct tagVERSION {
   DWORD dwVersion;                          /* Version number, in Ameol format */
   DWORD dwReqVersion;                       /* Earliest version of Ameol required for this product */
   char szAuthor[ 40 ];                      /* Author's identification field */
   char szDescription[ 100 ];                /* Product description field */
} VERSION;                   

int WINAPI Ameol2_SpellCheckDocument( HWND, HWND, BOOL );
BOOL WINAPI Ameol2_IsAmeolQuitting( void );
void WINAPI Ameol2_InsertMenu( AMEOLMENU FAR * );
void WINAPI Ameol2_DeleteMenu( HMENU, WORD );
void WINAPI Ameol2_GetWindows( HWND FAR * );
BOOL WINAPI Ameol2_GetCurrentTopic( CURMSG FAR * );
BOOL WINAPI Ameol2_GetCurrentMsg( CURMSG FAR * );
BOOL WINAPI Ameol2_SetCurrentMsg( CURMSG FAR * );
LPADDON WINAPI Ameol2_EnumAllTools( LPADDON, LPADDONINFO );
DWORD WINAPI Ameol2_GetVersion( void );
LPSTR WINAPI Ameol2_GetAmeolCodename( void );
LPSTR WINAPI Ameol2_ExpandVersion( DWORD );
LPSTR WINAPI Ameol2_ExpandAddonVersion( DWORD );
HFONT WINAPI Ameol2_GetStockFont( int );
BOOL WINAPI Ameol2_DeleteFile( LPCSTR, WORD );
BOOL WINAPI Ameol2_QueryFileExists( LPCSTR, WORD );
BOOL WINAPI Ameol2_RenameFile( LPCSTR, LPCSTR, WORD );
BOOL WINAPI Ameol2_Archive( LPCSTR, BOOL );
HNDFILE WINAPI Ameol2_CreateFile( LPCSTR, WORD, WORD );
HNDFILE WINAPI Ameol2_OpenFile( LPCSTR, WORD, WORD );
DWORD WINAPI CvtToAmeolVersion( DWORD );
int WINAPI Ameol2_GetWindowType( void );
LRESULT WINAPI Ameol2_DefMDIDlgProc( HWND, UINT, WPARAM, LPARAM );
int WINAPI Ameol2_GetCixNickname( char * );
int WINAPI Ameol2_GetSystemParameter( char *, char *, int );
BOOL WINAPI Ameol2_MakeLocalFileName( HWND, LPSTR, LPSTR, BOOL );
BOOL WINAPI Ameol2_UnloadAddon( LPADDON );
LPADDON WINAPI Ameol2_LoadAddon( LPSTR );
void WINAPI Ameol2_OnlineStatusMessage( LPSTR );
void WINAPI Ameol2_ConnectionInUse( void );
void WINAPI Ameol2_ConnectionFinished( void );
void WINAPI Ameol2_ShowUnreadMessageCount( void );
void WINAPI Ameol2_WriteToBlinkLog( LPSTR );
extern int iAddonsUsingConnection;

/* Scheduler API
 */
#define  SCHTYPE_ONCE         'O'
#define  SCHTYPE_WEEKLY       'W'
#define  SCHTYPE_STARTUP      'S'
#define  SCHTYPE_HOURLY       'H'
#define  SCHTYPE_MONTHLY      'M'
#define  SCHTYPE_DAILY        'D'
#define  SCHTYPE_EVERY        'E'
#define  SCHTYPE_DAYPERIOD    'P'

#define  SCHDERR_OK              0
#define  SCHDERR_ABORTED         1
#define  SCHDERR_NOSUCHCOMMAND   2
#define  SCHDERR_OUTOFMEMORY     3

typedef struct tagSCHEDULE {
   DWORD dwSize;              /* Version info data */
   int schType;               /* Type of scheduled action */
   AM_DATE schDate;           /* Date of one-off action */
   AM_TIME schTime;           /* Time of one-off action */
   int reserved;              /* Reserved for now */
} SCHEDULE, FAR * LPSCHEDULE;

int WINAPI Ameol2_GetSchedulerInfo( char *, SCHEDULE * );
int WINAPI Ameol2_SetSchedulerInfo( char *, SCHEDULE * );
int WINAPI Ameol2_EditSchedulerInfo( HWND, char * );

#define _AMEOL2_H
#endif
