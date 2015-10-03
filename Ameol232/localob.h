/* LOCALOB.H - Local folder out-basket objects
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

#ifndef _LOCALOB_H_

#include "shrdob.h"

#define  OBTYPE_LOCALSAYMESSAGE     0x300
#define  OBTYPE_LOCALCMTMESSAGE     0x301

#pragma pack(1)
typedef struct tagLOCALSAYOBJECT {
   RECPTR recConfName;                 /* Folder name */
   RECPTR recTopicName;                /* Topic name */
   RECPTR recSubject;                  /* Subject of message */
   RECPTR recText;                     /* The text of the message */
} LOCALSAYOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagLOCALCMTOBJECT {
   RECPTR recConfName;                 /* Folder name */
   RECPTR recTopicName;                /* Topic name */
   RECPTR recText;                     /* The text of the message */
   DWORD dwReply;                      /* Number of original message */
} LOCALCMTOBJECT;
#pragma pack()

#define _LOCALOB_H_
#endif
