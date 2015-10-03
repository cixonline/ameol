/* BLINKMAN.H - Blink Manager properties
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

#ifndef _BLINKMAN_H

typedef struct tagBLINKENTRY {
   struct tagBLINKENTRY FAR * lpbeNext;         /* Pointer to next entry */
   struct tagBLINKENTRY FAR * lpbePrev;         /* Pointer to previous entry */
   char szName[ 40 ];                        /* Name of entry */
   DWORD dwBlinkFlags;                       /* Blink flags */
   char szConnCard[ LEN_CONNCARD+1];            /* Connection card for blink */
   UINT iCommandID;                       /* Command ID associated with entry */
   RASDATA rd;                            /* RAS data stuff */
   char szAuthInfoUser[ 64 ];                /* Authentication user name (blank if none) */
   char szAuthInfoPass[ 40 ];                /* Authentication user password */
} BLINKENTRY, FAR * LPBLINKENTRY;

/* Variables.
 */
extern BLINKENTRY beCur;                     /* Current blink entry */

/* Functions.
 */
BOOL FASTCALL BeginBlink( LPBLINKENTRY );
void FASTCALL SaveBlinkSMTPAuth( BLINKENTRY * lpbe );

#define _BLINKMAN_H
#endif
