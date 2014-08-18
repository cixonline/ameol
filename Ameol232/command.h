/* COMMAND.H - Ameol command dispatcher definitions
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

#ifndef _COMMAND_H

typedef struct tagBUTTONBMP {
   BOOL fIsValid;                      /* TRUE if structure is valid */
   LPSTR pszFilename;                  /* Pointer to filename */
   int index;                          /* Index of bitmap in file */
   HBITMAP hbmpSmall;                  /* Cached small bitmap handle */
   HBITMAP hbmpLarge;                  /* Cached small bitmap handle */
} BUTTONBMP;

/* Predefined bitmaps.
 */
#define  BTNBMP_GRID          (LPSTR)MAKEINTRESOURCE(0x0001)

typedef struct tagMENUSELECTINFO {
   LPCSTR lpszCommand;                 /* Command name */
   UINT iCommand;                      /* Command value */
   LPCSTR lpszString;                  /* String resource ID */
   UINT iDisabledString;               /* String resource ID when command is disabled */
   LPCSTR lpszTooltipString;           /* Tooltip string resource ID */
   UINT iToolButton;                   /* Index of associated toolbar button bitmap */
   WORD wDefKey;                       /* Default key */
   DWORD iActiveMode;                  /* Window mode in which key is valid */
   WORD wCategory;                     /* Command category */
   WORD nScheduleFlags;                /* Scheduler options */
} MENUSELECTINFO;

typedef struct tagCMDREC {
   LPCSTR lpszCommand;                 /* Command name */
   UINT iCommand;                      /* Command value */
   LPCSTR lpszString;                  /* String resource ID */
   UINT iDisabledString;               /* String resource ID when command is disabled */
   LPCSTR lpszTooltipString;           /* Tooltip string resource ID */
   UINT iToolButton;                   /* Index of associated toolbar button bitmap */
   BOOL fCustomBitmap;                 /* TRUE if this command has a custom bitmap */
   BUTTONBMP btnBmp;                   /* Button bitmap */
   WORD wDefKey;                       /* Default key */
   DWORD iActiveMode;                  /* Window mode in which key is valid */
   HINSTANCE hLib;                     /* Handle of library that owns command */
   WORD wNewKey;                       /* Store for wKey during keyboard editing */
   WORD wKey;                          /* Current key code */
   WORD wCategory;                     /* Command category */
   WORD nScheduleFlags;                /* Scheduler options */
} CMDREC;

typedef struct tagCMD FAR * HCMD;

#define  NSCHF_NONE                    0x0000
#define  NSCHF_CANSCHEDULE             0x0001
#define  NSCHF_NEEDFOLDER              (0x0002|(NSCHF_CANSCHEDULE))

#define  MAKEKEY(ctrl,vkey)         (WORD)((((WORD)ctrl)<<8)|(WORD)(vkey))
#define  GETMODIFIER(key)           (WORD)((key)>>8)
#define  GETKEY(key)                (WORD)((key)&0xFF)

typedef struct tagCATEGORYITEM {
   WORD wCategory;                     /* Category number */
   char * pCategory;                   /* Category description */
} CATEGORYITEM, FAR * LPCATEGORYITEM;

/* Functions that work on commands.
 */
BOOL FASTCALL CTree_AddCategory( WORD, WORD, char * );
BOOL FASTCALL CTree_GetCategory( WORD, LPCATEGORYITEM );
BOOL FASTCALL CTree_FindCategory( WORD );
BOOL FASTCALL CTree_GetCommand( CMDREC FAR * );
BOOL FASTCALL CTree_PutCommand( CMDREC FAR * );
BOOL FASTCALL CTree_FindCommandName( char *, CMDREC FAR * );
BOOL FASTCALL CTree_InsertCommand( CMDREC FAR * );
BOOL FASTCALL CTree_FindKey( CMDREC FAR * );
BOOL FASTCALL CTree_FindNewKey( CMDREC FAR * );
void FASTCALL CTree_SetDefaultKey( void );
void FASTCALL CTree_SaveKey( void );
BOOL FASTCALL CTree_ChangeKey( UINT, WORD );
void FASTCALL CTree_ResetKey( void );
HCMD FASTCALL CTree_EnumTree( HCMD, CMDREC FAR * );
void FASTCALL CTree_GetItem( HCMD, CMDREC FAR * );
void FASTCALL CTree_SetItem( HCMD, CMDREC FAR * );
BOOL FASTCALL CTree_DeleteCommand( UINT );
HCMD FASTCALL CTree_CommandToHandle( char * );
LPCSTR FASTCALL CTree_GetCommandName( HCMD );
void FASTCALL CTree_DeleteAllCategories( void );
void FASTCALL CTree_DeleteAllCommands( void );
BOOL FASTCALL CTree_GetCommandKey( char *, WORD * );

/* Functions that work on button bitmaps.
 */
HBITMAP FASTCALL GetButtonBitmapEntry( BUTTONBMP * );
void FASTCALL SetCommandCustomBitmap( CMDREC FAR *, LPSTR, int );
BOOL FASTCALL CmdButtonBitmap( HWND, BUTTONBMP * );
HBITMAP FASTCALL CTree_GetCommandBitmap( UINT );
void FASTCALL EditButtonBitmap( HWND, UINT );

#define _COMMAND_H
#endif
