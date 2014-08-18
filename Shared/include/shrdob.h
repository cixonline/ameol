/* SHRDOB.H - Shared CIX and CIX Internet outbasket definitions
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

#ifndef _H_SHDRDOB

#define  OBTYPE_GETNEWARTICLES         0x100
#define  OBTYPE_GETTAGGED           0x101
#define  OBTYPE_GETNEWSGROUPS       0x102
#define  OBTYPE_GETNEWNEWSGROUPS       0x103
#define  OBTYPE_GETARTICLE          0x105
#define  OBTYPE_MAILMESSAGE            0x107
#define  OBTYPE_ARTICLEMESSAGE         0x108
#define  OBTYPE_CANCELARTICLE       0x109
#define  OBTYPE_FORWARDMAIL            0x110
#define  OBTYPE_USENETRESIGN           0x237
#define  OBTYPE_USENETJOIN          0x238

/* Encoding schemes
 */
#define  ENCSCH_NONE          0
#define  ENCSCH_UUENCODE      1
#define  ENCSCH_MIME          2
#define  ENCSCH_BINARY        3

#define  W_SERVICE_CIX              0x0001
#define  W_SERVICE_IP               0x0002
#define  W_SERVICE_BOTH             (W_SERVICE_CIX|W_SERVICE_IP)

#define  MOF_NOECHO                 0x0001
#define  MOF_RETURN_RECEIPT         0x0002
#define  MOF_READ_RECEIPT           0x0004

#pragma pack(1)
typedef struct tagNEWARTICLESOBJECT {
   RECPTR recFolder;                   /* Folder name */
   WORD wService;                      /* Service to use */
} NEWARTICLESOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagGETTAGGEDOBJECT {
   RECPTR recFolder;                   /* Folder name */
   WORD wService;                      /* Service to use */
} GETTAGGEDOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagNEWNEWSGROUPSOBJECT {
   RECPTR recServerName;               /* News server name */
   AM_DATE date;                       /* Date */
   AM_TIME time;                       /* Time */
} NEWNEWSGROUPSOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagNEWSGROUPSOBJECT {
   RECPTR recServerName;               /* News server name */
} NEWSGROUPSOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagGETARTICLEOBJECT {
   RECPTR recNewsgroup;                /* Newsgroup from which article is to be retrieved */
   DWORD dwMsg;                        /* Message to retrieve */
} GETARTICLEOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagCANCELOBJECT {
   RECPTR recNewsgroup;                /* Newsgroup from which article is to be cancelled */
   RECPTR recReply;                    /* Message ID of article to be cancelled */
   RECPTR recFrom;                  /* Reply to address */
   RECPTR recRealName;                 /* Real Name address */
} CANCELOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagMAILOBJECT {
   DWORD dwReply;                      /* Number of message to which this is a reply */
   RECPTR recFrom;                     /* Sender details */
   RECPTR recTo;                       /* To recipients */
   RECPTR recCC;                       /* CC recipients */
   RECPTR recBCC;                      /* Blind CC recipients */
   RECPTR recSubject;                  /* Subject */
   RECPTR recReply;                    /* Reply message number */
   RECPTR recReplyTo;                  /* Reply to address */
   RECPTR recText;                     /* The text of the message */
   RECPTR recAttachments;              /* Attachments */
   RECPTR recMailbox;                  /* Name of mailbox */
   short nPriority;                    /* Priority */
   WORD wEncodingScheme;               /* Encoding scheme (uuencode or MIME) */
   WORD wFlags;                        /* Mail flags */
} MAILOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagARTICLEOBJECT {
   DWORD dwReply;                      /* Number of message to which this is a reply */
   RECPTR recNewsgroup;                /* Newsgroup to which article is posted */
   RECPTR recSubject;                  /* Subject */
   RECPTR recReply;                    /* Reply message number */
   RECPTR recReplyTo;                  /* Reply to address */
   RECPTR recText;                     /* The text of the message */
   RECPTR recAttachments;              /* Attachments */
   RECPTR recFolder;                   /* Folder from which article was created */
   WORD wEncodingScheme;               /* Encoding scheme (uuencode or MIME) */
} ARTICLEOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagFORWARDMAILOBJECT {
   RECPTR recTo;                       /* To recipients */
   RECPTR recCC;                       /* CC recipients */
   RECPTR recSubject;                  /* Subject */
   RECPTR recBCC;                      /* Blind CC recipients */
   RECPTR recReplyTo;                  /* Reply to address */
   RECPTR recText;                     /* The text of the message */
   RECPTR recAttachments;              /* Attachments */
   RECPTR recMailbox;                  /* Name of mailbox */
   RECPTR recFrom;                     /* Sender details */
   short nPriority;                    /* Priority */
   WORD wEncodingScheme;               /* Encoding scheme (uuencode or MIME) */
   WORD wFlags;                        /* Mail flags */
} FORWARDMAILOBJECT;
#pragma pack()

#define _H_SHDRDOB
#endif
