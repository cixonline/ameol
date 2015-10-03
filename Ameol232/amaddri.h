/* AMADDRI.H - Internal header file for address book
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

#ifndef _AMADDRI_H

typedef struct tagFAXBOOK {
   struct tagFAXBOOK FAR * lpNext;        /* Pointer to next faxbook entry */
   FAXENTRY fe;                           /* Faxbook entry */
} FAXBOOK;

typedef FAXBOOK FAR * LPFAXBOOK;

typedef struct tagADDRBOOK {
   struct tagADDRBOOK FAR * lpNext;
   BOOL fGroup;
   char szFullName[ AMLEN_FULLCIXNAME + 1 ];
   char szCixName[ AMLEN_INTERNETNAME + 1 ];
   char szComment[ AMLEN_COMMENT + 1 ];
} ADDRBOOK;

typedef ADDRBOOK FAR * LPADDRBOOK;

typedef struct tagADDRBOOKGROUP {
   struct tagADDRBOOKGROUP FAR * lpNext;
   BOOL fGroup;
   char szGroupName[ AMLEN_INTERNETNAME+1 ];
   WORD cGroup;
   LPADDRBOOK lpAddrBook[ 1 ];
} ADDRBOOKGROUP;

typedef ADDRBOOKGROUP FAR * LPADDRBOOKGROUP;

extern LPADDRBOOK lpAddrBookFirst;

BOOL FASTCALL Amaddr_CreateEntry( LPSTR, LPSTR, LPSTR );
LPADDRBOOK FASTCALL Amaddr_DeleteEntry( LPADDRBOOK );
void FASTCALL Amaddr_CommitChanges( void );
void FASTCALL Amaddr_FillList( HWND, BOOL, char * );
BOOL FASTCALL Amaddr_Find( LPSTR, ADDRINFO FAR * );
void FASTCALL Amaddr_Load( void );

BOOL FASTCALL CreateFaxBookEntry( LPSTR, LPSTR );
BOOL FASTCALL RemoveFaxBookEntry( LPFAXBOOK );

#define _AMADDRI_H
#endif
