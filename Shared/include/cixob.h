/* CIXOB.H - CIX out-basket object definitions
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

#ifndef _CIXOB_H_

#include "shrdob.h"

#define  OBTYPE_JOINCONFERENCE      0x200
#define  OBTYPE_SAYMESSAGE          0x201
#define  OBTYPE_COMMENTMESSAGE      0x202
#define  OBTYPE_GETUSERLIST         0x203
#define  OBTYPE_GETCIXLIST          0x204
#define  OBTYPE_WITHDRAW            0x205
#define  OBTYPE_DOWNLOAD            0x206
#define  OBTYPE_FAXMAILMESSAGE      0x207
#define  OBTYPE_GETMESSAGE          0x208
#define  OBTYPE_CIXRESIGN           0x209
#define  OBTYPE_INCLUDE             0x210
#define  OBTYPE_INLINE              0x211
#define  OBTYPE_PREINCLUDE          0x212
#define  OBTYPE_FILELIST            0x213
#define  OBTYPE_ADDPAR              0x215
#define  OBTYPE_REMPAR              0x216
#define  OBTYPE_COMOD               0x217
#define  OBTYPE_EXMOD               0x218
#define  OBTYPE_GETPARLIST          0x219
#define  OBTYPE_RESUME              0x220
#define  OBTYPE_PUTRESUME           0x221
#define  OBTYPE_CLEARCIXMAIL        0x224
#define  OBTYPE_COPYMSG             0x225
#define  OBTYPE_FUL                 0x226
#define  OBTYPE_FDIR                0x227
#define  OBTYPE_MAILLIST            0x228
#define  OBTYPE_MAILUPLOAD          0x229
#define  OBTYPE_MAILDOWNLOAD        0x230
#define  OBTYPE_MAILEXPORT          0x231
#define  OBTYPE_MAILERASE           0x232
#define  OBTYPE_MAILRENAME          0x233
#define  OBTYPE_EXECCIXSCRIPT       0x234
#define  OBTYPE_CIXARTICLEMESSAGE   0x235
#define  OBTYPE_BINMAIL             0x236
#define OBTYPE_UPDATENOTE           0x239
#define OBTYPE_RESTORE              0x240
#define  OBTYPE_EXECVERSCRIPT       0x241
#define OBTYPE_BANPAR               0x242
#define OBTYPE_UNBANPAR             0x243

/* Fax mail flags.
 */
#define  FFX_NOCOVER          0x0001

/* Download flags
 */
#define  FDLF_JOINANDRESIGN      1
#define  FDLF_DELETE          2
#define  FDLF_OVERWRITE       4
#define  FDLF_RENAME          8

/* Binmail flags
 */
#define  BINF_SENDCOVERNOTE      1
#define  BINF_DELETE          4
#define  BINF_UPLOAD          8

#pragma pack(1)
typedef struct tagFDLOBJECT {
   RECPTR recConfName;                 /* Conference name */
   RECPTR recTopicName;                /* Topic name */
   RECPTR recCIXFileName;              /* CIX file name */
   RECPTR recFileName;                 /* Local file name */
   RECPTR recDirectory;                /* Directory to which to download */
   WORD wFlags;                        /* Download flags */
} FDLOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagFULOBJECT {
   RECPTR recConfName;                 /* Conference name */
   RECPTR recTopicName;                /* Topic name */
   RECPTR recFileName;                 /* Full pathname of file to upload */
   RECPTR recText;                     /* Description of file being uploaded */
} FULOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagBINMAILOBJECT {
   RECPTR recFileName;                 /* File name */
   RECPTR recUserName;                 /* User name */
   RECPTR recText;                     /* Cover note text */
   DWORD dwSize;                       /* Size of binmail object */
   WORD wFlags;                        /* Binmail flags */
   WORD wEncodingScheme;               /* Encoding scheme (uuencode or MIME) */
} BINMAILOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagINCLUDEOBJECT {
   RECPTR recFileName;                 /* File name */
   RECPTR recDescription;              /* Description */
} INCLUDEOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagINLINEOBJECT {
   RECPTR recFileName;                 /* File name */
   RECPTR recDescription;              /* Description */
} INLINEOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagWITHDRAWOBJECT {
   DWORD dwMsg;                        /* Message number */
   RECPTR recTopicName;                /* Topic from which to withdraw */
} WITHDRAWOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagRESTOREOBJECT {
   DWORD dwMsg;                        /* Message number */
   RECPTR recTopicName;                /* Topic from which to restore */
} RESTOREOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagJOINOBJECT {
   RECPTR recConfName;                 /* Conference to join */
   WORD wRecent;                       /* Recent messages on join */
} JOINOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagRESIGNOBJECT {
   RECPTR recConfName;                 /* Conference or topic to resign */
} RESIGNOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagRESIGNUNETOBJECT {
   RECPTR recGroupName;                /* Newsgroup to resign */
} RESIGNUNETOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagJOINUNETOBJECT {
   RECPTR recGroupName;                /* Newsgroup to resign */
   WORD wArticles;                     /* Number of articles */
} JOINUNETOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagCOPYMSGOBJECT {
   DWORD dwMsg;                        /* Message number */
   RECPTR recConfName;                 /* Conference name */
   RECPTR recTopicName;                /* Topic name */
   RECPTR recCurConfName;              /* Conference name */
   RECPTR recCurTopicName;             /* Topic name */
} COPYMSGOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagSAYOBJECT {
   RECPTR recConfName;                 /* Conference name */
   RECPTR recTopicName;                /* Topic name */
   RECPTR recSubject;                  /* Subject of message */
   RECPTR recText;                     /* The text of the message */
} SAYOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagCOMMENTOBJECT {
   DWORD dwReply;                      /* Number of original message */
   RECPTR recConfName;                 /* Conference name */
   RECPTR recTopicName;                /* Topic name */
   RECPTR recText;                     /* The text of the message */
} COMMENTOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagFAXMAILOBJECT {
   WORD wFaxFlags;                     /* Fax flags */
   RECPTR recNumber;                   /* Fax destination number */
   RECPTR recTo;                       /* Name of recipient */
   RECPTR recFrom;                     /* Name of sender */
   RECPTR recSubject;                  /* Subject title */
   RECPTR recText;                     /* The text of the message */
} FAXMAILOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagGETMESSAGEOBJECT {
   DWORD dwStartMsg;                   /* First message number */
   DWORD dwEndMsg;                     /* Last message number */
   RECPTR recTopicName;                /* Topic from which to get message */
} GETMESSAGEOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagFILELISTOBJECT {
   RECPTR recConfName;                 /* Conference name */
   RECPTR recTopicName;                /* Topic name */
} FILELISTOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagFDIROBJECT {
   RECPTR recConfName;                 /* Conference name */
   RECPTR recTopicName;                /* Topic name */
} FDIROBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagADDPAROBJECT {
   RECPTR recConfName;                 /* Conference name */
   RECPTR recUserName;                 /* User names */
} ADDPAROBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagREMPAROBJECT {
   RECPTR recConfName;                 /* Conference name */
   RECPTR recUserName;                 /* User names */
} REMPAROBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagCOMODOBJECT {
   RECPTR recConfName;                 /* Conference name */
   RECPTR recUserName;                 /* User names */
} COMODOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagEXMODOBJECT {
   RECPTR recConfName;                 /* Conference name */
   RECPTR recUserName;                 /* User names */
} EXMODOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagGETPARLISTOBJECT {
   RECPTR recConfName;                 /* Conference name */
} GETPARLISTOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagUPDATENOTEOBJECT {
   RECPTR recConfName;                 /* Conference name */
} UPDATENOTEOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagRESUMEOBJECT {
   RECPTR recUserName;                 /* Name of user */
   RECPTR recFileName;                 /* Filename */
} RESUMEOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagBANPAROBJECT {
   RECPTR recUserName;                 /* Name of user */
   RECPTR recConfName;                 /* Conference name */
   RECPTR recTopicName;             /* Topic name */
} BANPAROBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagMAILUPLOADOBJECT {
   RECPTR recFileName;                 /* Local file name, including path */
} MAILUPLOADOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagMAILDOWNLOADOBJECT {
   RECPTR recCIXFileName;              /* CIX file name */
   RECPTR recFileName;                 /* File name, including path */
   RECPTR recDirectory;                /* Directory to which to download */
   WORD wFlags;                        /* Download flags */
} MAILDOWNLOADOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagMAILEXPORTOBJECT {
   RECPTR recConfName;                 /* Conference name */
   RECPTR recTopicName;                /* Topic name */
   RECPTR recFileName;                 /* Name of file to export */
   WORD wFlags;                        /* Export flags */
} MAILEXPORTOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagMAILERASEOBJECT {
   RECPTR recFileName;                 /* File name */
} MAILERASEOBJECT;
#pragma pack()

#pragma pack(1)
typedef struct tagMAILRENAMEOBJECT {
#pragma pack()
   RECPTR recOldName;                  /* Old file name */
   RECPTR recNewName;                  /* New file name */
} MAILRENAMEOBJECT;
#pragma pack()

#define _CIXOB_H_
#endif
