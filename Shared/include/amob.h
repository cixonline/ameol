/* AMOB.H - Ameol Out-Basket API definitions
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

#ifndef _OUTBASK_H_INC

/* Constants for scheduling a single OB item. Not
 * used at the moment.
 */
#define  OBSCHED_RUNONCE            0
#define  OBSCHED_HOURLY             1
#define  OBSCHED_DAILY              2
#define  OBSCHED_WEEKLY             3

/* Blink manager constants.
 */
#define  BF_GETIPMAIL            0x00000001
#define  BF_GETIPNEWS            0x00000002
#define  BF_POSTIPMAIL           0x00000004
#define  BF_POSTIPNEWS           0x00000008
#define  BF_GETIPTAGGEDNEWS      0x00000010
#define  BF_POSTCIXMAIL          0x00000020
#define  BF_POSTCIXNEWS          0x00000040
#define  BF_POSTCIXMSGS          0x00000080
#define  BF_GETCIXMAIL           0x00000100
#define  BF_GETCIXNEWS           0x00000200
#define  BF_GETCIXMSGS           0x00000400
#define  BF_CIXRECOVER           0x00000800
#define  BF_STAYONLINE           0x00001000
#define  BF_GETCIXTAGGEDNEWS     0x00002000

/* Exec return codes.
 */
#define  POF_CANNOTACTION        0
#define  POF_PROCESSCOMPLETED    1
#define  POF_PROCESSSTARTED      2
#define  POF_AWAITINGSERVICE     3
#define  POF_HELDBYSCRIPT        4

/* Object class ID */
typedef DWORD OBCLSID;

/* Outbasket flags
 */
#define  OBF_NORMAL     0x0000            /* Unmodified item */
#define  OBF_HOLD       0x0001            /* Item is being held */
#define  OBF_ACTIVE     0x0002            /* This item is being carried out */
#define  OBF_KEEP       0x0004            /* Item is processed on every blink */
#define  OBF_PENDING    0x0008            /* Item is waiting for service to complete */
#define  OBF_ERROR      0x0010            /* Item failed */
#define  OBF_OPEN       0x0020            /* Item is being edited */
#define  OBF_SCRIPT     0x0040            /* Item is held by a script */

/* Only used by Amob_GetCount
 */
#define  OBF_UNFLAGGED  0x8000            /* Unflagged (not kept or held) items */

#pragma pack(1)
typedef struct tagOBHEADER {
   WORD wStartSig;               /* Object header start signature */
   OBCLSID clsid;                /* Object class */
   DWORD dwType;                 /* Type of object */
   WORD wFlags;                  /* Object flags */
   WORD id;                      /* Unique out-basket object ID */
   WORD wDate;                   /* Date when object was posted */
   WORD wTime;                   /* Time when object was posted */
   WORD wActionDate;             /* Date when object will be activated */
   WORD wActionTime;             /* Time when object will be activated */
   WORD wLastActionDate;         /* Date when object was last activated */
   WORD wLastActionTime;         /* Time when object was last activated */
   WORD wActionFlags;            /* Scheduler flags */
   WORD wActionFreq;             /* Action frequency */
   WORD wEndSig;                 /* Object header end signature */
   WORD wCRC;                    /* Header checksum */
} OBHEADER;
#pragma pack()

typedef struct tagOBINFO {
   OBHEADER obHdr;               /* Outbasket header */
   DWORD dwClsType;              /* Class type */
   LPVOID lpObData;              /* Class specific data handle */
} OBINFO;

/* Out-basket events.
 */
#define  OBEVENT_LOAD            0
#define  OBEVENT_SAVE            1
#define  OBEVENT_DELETE          2
#define  OBEVENT_NEW             3
#define  OBEVENT_EXEC            4
#define  OBEVENT_EDIT            5
#define  OBEVENT_DESCRIBE        6
#define  OBEVENT_FIND            7
#define  OBEVENT_COPY            8
#define  OBEVENT_PERSISTENCE     9
#define  OBEVENT_EDITABLE        10
#define  OBEVENT_REMOVE          11
#define  OBEVENT_GETCLSID        12
#define  OBEVENT_GETTYPE         13

typedef LRESULT (EXPORT CALLBACK FAR * LPFOBPROC)( UINT, HNDFILE, LPVOID, LPVOID );

/* A record pointer implements a paged data structure
 * scheme.
 */
#pragma pack(1)
typedef struct tagRECPTR {
   DWORD dwOffset;                     /* Offset of data in database */
   DWORD dwSize;                       /* Size of data in database and memory */
   HPSTR hpStr;                        /* Pointer to data in memory */
} RECPTR;
#pragma pack()

typedef struct tagOB FAR * LPOB;       /* Out-basket handle */

#define  LPOB_PROCESSED    (LPOB)0xFFFFFFFE

BOOL WINAPI Amob_RegisterObjectClass( OBCLSID, DWORD, LPFOBPROC );
HPSTR WINAPI Amob_DerefObject( RECPTR FAR * );
BOOL WINAPI Amob_FreeRefObject( RECPTR FAR * );
BOOL WINAPI Amob_CreateRefString( RECPTR FAR *, HPSTR );
BOOL WINAPI Amob_CreateRefObject( RECPTR FAR *, DWORD );
BOOL WINAPI Amob_SaveRefObject( RECPTR FAR * );
BOOL WINAPI Amob_CopyRefObject( RECPTR FAR *, RECPTR FAR * );
void WINAPI Amob_ClearMemory( void );
void WINAPI Amob_RemoveObject( LPOB, BOOL );
LPOB WINAPI Amob_Commit( LPOB, OBCLSID, LPVOID );
BOOL WINAPI Amob_Uncommit( LPOB );
LPOB WINAPI Amob_Find( OBCLSID, LPVOID );
LPOB WINAPI Amob_FindNext( LPOB, OBCLSID, LPVOID );
BOOL WINAPI Amob_Delete( OBCLSID, LPVOID );
BOOL WINAPI Amob_New( OBCLSID, LPVOID );
void WINAPI Amob_ResyncObjectIDs( void );
BOOL WINAPI Amob_Edit( LPOB );
BOOL WINAPI Amob_Exec( OBCLSID, LPVOID );
BOOL WINAPI Amob_Describe( LPOB, LPSTR );
LPOB WINAPI Amob_GetOb( LPOB );
int WINAPI Amob_GetCount( WORD );
void WINAPI Amob_GetObInfo( LPOB, OBINFO FAR * );
void WINAPI Amob_SetObInfo( LPOB, OBINFO FAR * );
BOOL WINAPI Amob_IsEditable( OBCLSID );
DWORD WINAPI Amob_GetObjectType( LPOB );
BOOL WINAPI Amob_SaveDataObject( HNDFILE, LPVOID, UINT );
BOOL WINAPI Amob_LoadDataObject( HNDFILE, LPVOID, UINT );
LPOB WINAPI Amob_GetByID( LPOB, WORD );
WORD WINAPI Amob_GetNextObjectID( void );
HWND WINAPI Amob_GetEditWindow( LPOB );
void WINAPI Amob_SetEditWindow( LPOB, HWND );
void WINAPI Amob_SaveOutBasket( BOOL );

/* Quick macro around Amob_DerefObject.
 */
#define DRF(p)    Amob_DerefObject( &(p) )

#define  _OUTBASK_H_INC
#endif
