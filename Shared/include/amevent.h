/* AMEVENT.H - Event Manager APIs and definitions
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

#ifndef _H_AMEVENT

/* Event log definitions.
 */
#define  ETYP_DATABASE        0x0001
#define  ETYP_COMMS           0x0002
#define  ETYP_INITTERM        0x0004
#define  ETYP_DISKFILE        0x0008
#define  ETYP_MAINTENANCE     0x0010
#define  ETYP_MASK            0x0FFF

#define  ETYP_WARNING         0x1000
#define  ETYP_SEVERE          0x2000
#define  ETYP_MESSAGE         0x4000
#define  ETYP_MASK2           0xF000

#define  GNE_FIRST            0
#define  GNE_LAST             1
#define  GNE_BYINDEX          2
#define  GNE_BYTYPE           3

#define  DEF_MAXEVENTLOGLINES    100
#define  DEF_PERMITTEDEVENTS     (ETYP_DATABASE|ETYP_COMMS|ETYP_INITTERM|ETYP_DISKFILE|ETYP_MAINTENANCE|ETYP_SEVERE|ETYP_WARNING|ETYP_MESSAGE)

#define  ELOG_DESCRIPTION     79

#pragma pack(1)
typedef struct tagEVENTLOGITEM {
   WORD wDate;
   WORD wTime;
   WORD wType;
   char szDescription[ ELOG_DESCRIPTION+1 ];
} EVENTLOGITEM;
#pragma pack()

typedef struct tagEVENTLOGITEM FAR * LPEVENTLOGITEM;
typedef struct tagEVENT FAR * LPEVENT;

BOOL WINAPI Amevent_AddEventLogItem( WORD, LPSTR );
int WINAPI Amevent_GetNextEventLogItem( WORD, int, LPEVENTLOGITEM );
void WINAPI EXPORT Amevent_FreeEventLog( void ); //20060325 - 2.56.2049.20
void WINAPI Amevent_ClearEventLog( void );
WORD WINAPI Amevent_GetPermittedEventsMask( void );
WORD WINAPI Amevent_GetEventLogMaximumSize( void );
void WINAPI Amevent_SetPermittedEventsMask( WORD );
void WINAPI Amevent_SetEventLogMaximumSize( WORD );
LPEVENT WINAPI Amevent_GetEvent( LPEVENT );
void WINAPI Amevent_GetEventInfo( LPEVENT, LPEVENTLOGITEM );

#define _H_AMEVENT
#endif
