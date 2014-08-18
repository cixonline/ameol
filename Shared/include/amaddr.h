/* AMADDR.H - Header file for address book
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

#ifndef _AMADDR_H

/* Needed for FILEINFO structure
 */
#include "amuser.h"

#define  AMLEN_FAXNO             31
#define  AMLEN_FAXTO             31
#define  AMLEN_FULLCIXNAME       80
#define  AMLEN_INTERNETNAME         99
#define  AMLEN_COMMENT           63

typedef struct tagADDRINFO {
   char szCixName[ AMLEN_INTERNETNAME+1 ];
   char szFullName[ AMLEN_FULLCIXNAME+1 ];
   char szComment[ AMLEN_COMMENT+1 ];
} ADDRINFO;

typedef LPVOID HADDRBOOK;

typedef struct tagFAXENTRY {
   char szFullName[ AMLEN_FAXTO+1 ];
   char szFaxNo[ AMLEN_FAXNO+1 ];
} FAXENTRY, FAR * LPFAXENTRY;

#define  AMPCKFLG_SETCC    0x0001
#define  AMPCKFLG_NOCC     0x0002
#define  AMPCKFLG_SETBCC      0x0004

typedef struct tagAMADDRPICKLIST {
   LPSTR lpszTo;
   LPSTR lpszCC;
   LPSTR lpszBCC;
   WORD wFlags;
} AMADDRPICKLIST, FAR * LPAMADDRPICKLIST;

/* Non exported address book functions.
 */
int FASTCALL Amaddr_Editor( void );
BOOL FASTCALL Amaddr_PickFaxEntry( HWND, LPFAXENTRY );

/* Address book details.
 */
BOOL WINAPI Amaddr_IsGroup( LPSTR );
UINT WINAPI Amaddr_ExpandGroup( LPSTR, int, LPSTR );
BOOL WINAPI Amaddr_SetEntry( LPSTR, ADDRINFO FAR * );
BOOL WINAPI Amaddr_GetEntry( LPSTR, ADDRINFO FAR * );
int WINAPI Amaddr_GetGroup( LPSTR, char FAR * FAR [] );
BOOL WINAPI Amaddr_CreateGroup( LPSTR );
BOOL WINAPI Amaddr_DeleteGroup( char * );
BOOL WINAPI Amaddr_AddToGroup( LPSTR, LPSTR );
HADDRBOOK WINAPI Amaddr_GetNextEntry( HADDRBOOK, ADDRINFO FAR * );
HADDRBOOK WINAPI Amaddr_GetNextGroup( HADDRBOOK, LPSTR );
BOOL WINAPI Amaddr_AddEntry( HWND, LPSTR, LPSTR );
BOOL WINAPI Amaddr_PickEntry( HWND, LPAMADDRPICKLIST );

/* Fax address book details.
 */
BOOL WINAPI Amaddr_CreateFaxBookEntry( LPSTR, LPSTR );
BOOL WINAPI Amaddr_DeleteFaxBookEntry( LPSTR, LPSTR );

#define _AMADDR_H
#endif
