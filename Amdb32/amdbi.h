/* AMDBI.C - Internal Ameol database definitions
 *
 * Copyright 1993-1996 CIX, All Rights Reserved
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

#include <stdlib.h>
#include "amdb.h"

#define DB_ID               0x69BE          /* Current database format ID */
#define DB_VERSION          0x0105          /* Version 1.05 */

#define MH_ID               0xDE78          /* Identifies a valid message header ID */

#define STYP_TH             0x67            /* Identifies a message record */
#define STYP_TL             0x90            /* Identifies a topic record */
#define STYP_CL             0x85            /* Identifies a folder record */
#define STYP_CAT            0x43            /* Identifies a database record */

#define DEF_PURGE_TYPE      PO_DELETEONLY
#define DEF_PURGE_SIZE      100
#define DEF_PURGE_DATE      30

#define MAX_MASKLEVEL       4               /* Maximum masking levels */

/* Attachment linked list structure.
 */
typedef struct tagAH {
    struct tagAH FAR * pahNext;             /* Pointer to next attachment */
    ATTACHMENT atItem;                      /* Attachment */
} AH, FAR * PAH;

/* Message counts structure
 */
#pragma pack(1)
typedef struct tagMSGCOUNT {
    DWORD cUnRead;                          /* Number of un-read messages in folder */
    DWORD cMarked;                          /* Number of marked messages */
    DWORD cPriority;                        /* Number of priority messages in topic */
    DWORD cUnReadPriority;                  /* Number of unread priority messages in topic */
    DWORD cTagged;                          /* Number of headers tagged for full retrieval */
} MSGCOUNT;
#pragma pack()

/* Global message structure
 */
#pragma pack(1)
typedef struct tagTHITEM {
    char szTitle[ 80 ];                     /* Title of message */
    char szAuthor[ 80 ];                    /* Author of this message */
    DWORD dwComment;                        /* Message to which this is a comment */
    DWORD dwMsg;                            /* Message number */
    DWORD dwOffset;                         /* Offset of text in data file */
    DWORD dwSize;                           /* Length of text in data file */
    DWORD dwFlags;                          /* Message flags */
    DWORD dwMsgPtr;                         /* Offset of message in thread file */
    WORD wDate;                             /* Date of this message (packed) */
    WORD wTime;                             /* Time of this message (packed) */
    char szMessageId[ 80 ];                 /* Message ID (Mail/Usenet only) */
    DWORD dwActualMsg;                      /* Actual message number */
    DWORD dwRealSize;                       /* Actual message size */
} THITEM;
#pragma pack()

/* Msg linked list structure 
 */
typedef struct tagTH {
    BYTE wID;                               /* Structure ID word */
    struct tagTH FAR * pthNext;             /* Pointer to next message */
    struct tagTH FAR * pthPrev;             /* Pointer to previous message */
    struct tagTL FAR * ptl;                 /* Pointer to parent topic */
    struct tagTH FAR * pthRefNext;          /* Pointer to next reference */
    struct tagTH FAR * pthRefPrev;          /* Pointer to previous reference */
    struct tagTH FAR * pthRoot;             /* Pointer to root message (NULL if is root) */
    struct tagAH FAR * pahFirst;            /* Pointer to first associated attachment */
    DWORD dwTmpOffset;                      /* Saved offset for compact */
    BYTE wLevel;                            /* Thread level number */
    BYTE cPriority;                         /* Priority level number */
    WORD wComments;                         /* Number of comments to this message */
    WORD wUnRead;                           /* Number of unread in the thread */
    BOOL fExpanded;                         /* TRUE if thread expanded */
    int nSel;                               /* Selection in list box */
    BYTE nLockCount;                        /* Message lock count */
    DWORD dwThdMask[ MAX_MASKLEVEL ];       /* Thread mask */
    THITEM thItem;                          /* Global message structure */
} TH, FAR * PTH;

/*  Global topic structure
 */
#pragma pack(1)
typedef struct tagTLITEM {
    char szTopicName[ 78 ];                 /* Topic name */
    char szFileName[ 9 ];                   /* Unique topic file name */
    WORD timeCreated;                       /* Time when topic was created */
    WORD dateCreated;                       /* Date when topic was created */
    DWORD dwFlags;                          /* Topic flags */
    DWORD dwMinMsg;                         /* Smallest message in this topic */
    DWORD dwMaxMsg;                         /* Largest message in this topic */
    DWORD cMsgs;                            /* Number of messages in this folder */
    WORD wTopicType;                        /* Topic type */
    WORD wLastPurgeDate;                    /* Date of last purge */
    WORD wLastPurgeTime;                    /* Time of last purge */
    DWORD dwUpperMsg;                       /* Upper message count */
    char szSigFile[ 9 ];                    /* Signature file */
    PURGEOPTIONS po;                        /* Topic purge options */
    VIEWDATA vd;                            /* Topic display information */
    MSGCOUNT msgc;                          /* Message counts */
} TLITEM;
#pragma pack()

typedef struct tagCNL;

/* Topic linked list structure 
 */
typedef struct tagTL {
    BYTE wID;                               /* Structure ID word */
    struct tagTL FAR * ptlNext;             /* Pointer to next topic */
    struct tagTL FAR * ptlPrev;             /* Pointer to previous topic */
    struct tagTH FAR * pthFirst;            /* Pointer to first message in topic */
    struct tagTH FAR * pthLast;             /* Pointer to last message in topic */
    struct tagTH FAR * pthRefFirst;         /* Pointer to first message in ref list */
    struct tagTH FAR * pthRefLast;          /* Pointer to last message in ref list */
    struct tagCNL FAR * pcl;                /* Pointer to parent folder */
    struct tagTH FAR * pthCreateLast;       /* Pointer to last created message in topic */
    struct tagAH FAR * pahFirst;            /* Pointer to attachments */
    WORD wLockCount;                        /* Topic lock count */
    HWND hwnd;                              /* Handle of topic window */
    TLITEM tlItem;                          /* Global topic structure */
} TL, FAR * PTL;

/* Global folder structure 
 */
#pragma pack(1)
typedef struct tagCLITEM {
    char szFolderName[ 80 ];                /* Folder name */
    char szFileName[ 9 ];                   /* Unique folder file name */
    WORD timeCreated;                       /* Time when folder was created */
    WORD dateCreated;                       /* Date when folder was created */
    WORD cTopics;                           /* Number of topics in this folder */
    DWORD cMsgs;                            /* Number of messages in this folder */
    DWORD dwFlags;                          /* Global folder flags */
    WORD wLastPurgeDate;                    /* Date of last purge */
    WORD wLastPurgeTime;                    /* Time of last purge */
    char szSigFile[ 9 ];                    /* Signature file */
    PURGEOPTIONS po;                        /* Folder purge options */
} CLITEM;
#pragma pack()

typedef struct tagCAT;

/*  Folder linked list structure 
 */
typedef struct tagCNL {
    BYTE wID;                               /* Structure ID word */
    struct tagCNL FAR * pclNext;            /* Pointer to next folder */
    struct tagCNL FAR * pclPrev;            /* Pointer to previous folder */
    struct tagTL FAR * ptlFirst;            /* Pointer to first topic in folder*/
    struct tagTL FAR * ptlLast;             /* Pointer to last topic in folder */
    struct tagCAT FAR * pcat;               /* Pointer to parent database */
    CLITEM clItem;                          /* Global folder structure */
    MSGCOUNT msgc;                          /* Message counts */
    LPSTR lpModList;                        /* List of conference moderators */
} CNL;

/* Global category structure.
 */
#pragma pack(1)
typedef struct tagCATITEM {
    char szCategoryName[ 80 ];              /* Category name */
    WORD timeCreated;                       /* Time when folder was created */
    WORD dateCreated;                       /* Date when folder was created */
    WORD cFolders;                          /* Number of folders in this category */
    DWORD cMsgs;                            /* Number of messages in this category */
    DWORD dwFlags;                          /* Category flags */
    WORD wLastPurgeDate;                    /* Date of last purge */
    WORD wLastPurgeTime;                    /* Time of last purge */
} CATITEM;  
#pragma pack()

/* Linked list of categories
 */
typedef struct tagCAT {
    BYTE wID;                               /* Structure ID word */
    struct tagCAT FAR * pcatNext;           /* Pointer to next open database */
    struct tagCAT FAR * pcatPrev;           /* Pointer previous open database */
    struct tagCNL FAR * pclFirst;           /* Pointer to first item in database */
    struct tagCNL FAR * pclLast;            /* Pointer to last item in database */
    CATITEM catItem;                        /* Global category structure */
    MSGCOUNT msgc;                          /* Message counts */
} CAT, FAR * PCAT;

/* Database structure
 */
#pragma pack(1)
typedef struct tagDBHEADER {
    WORD wID;                               /* Identifies this file */
    WORD wVersion;                          /* Version number of file format */
    char szName[ 60 ];                      /* Database name */
    char szPassword[ 40 ];                  /* Password */
    char szOwner[ 40 ];                     /* Owner of this database */
    WORD timeCreated;                       /* Time when db was created */
    WORD dateCreated;                       /* Date when db was created */
    DWORD dwFlags;                          /* Database flags */
    WORD cbFolder;                          /* Size of folder header */
    WORD cbTopic;                           /* Size of topic header */
    WORD cbMsg;                             /* Size of a message record */
    WORD cCategories;                       /* Number of categories in database */
} DBHEADER;
#pragma pack()

extern HINSTANCE hLibInst;
extern HINSTANCE hRscLib;
extern MSGCOUNT cTotalMsgc;
extern PTL ptlCachedThd;
extern HNDFILE fhCachedThd;
extern PTL ptlCachedTxt;
extern HNDFILE fhCachedTxt;
extern BOOL fHasShare;
extern char szDataDir[ _MAX_DIR ];
extern char szUserDir[ _MAX_DIR ];
extern PCAT pcatFirst;
extern PCAT pcatLast;
extern BOOL fInitDataDirectory;
extern PURGEOPTIONS poDefault;
extern WORD wBufferUpperLimit;
extern INTERFACE_PROPERTIES ip;
extern BOOL fDataChanged;
extern DBHEADER dbHdr;
extern LPSTR lpszGlbDatabase;

BOOL FASTCALL fNewCategory( PCAT *, CATITEM * );
BOOL FASTCALL fNewFolder( PCAT, PCL *, char *, CLITEM * );
BOOL FASTCALL fNewTopic( PTL *, PCL, TLITEM * );
BOOL FASTCALL fDeleteFolder( PCL );
BOOL FASTCALL fDeleteTopic( PTL );
BOOL FASTCALL SaveAttachments( PTL );
void FASTCALL UpdateMessageCounts( MSGCOUNT *, MSGCOUNT * );
void FASTCALL ReduceMessageCounts( MSGCOUNT *, MSGCOUNT * );
BOOL FASTCALL YieldToSystem( void );
BOOL FASTCALL CreateUniqueFile( LPSTR, LPSTR, BOOL, LPSTR );
BOOL FASTCALL CreateUniqueFileMulti( LPSTR, LPSTR, LPSTR, BOOL, LPSTR);
BOOL FASTCALL CommitTopicMessages( PTL, BOOL );
PTH FASTCALL CreateMsgIndirect( PTL, THITEM *, DWORD *, BOOL * );

HNDFILE FASTCALL Amdb_OpenFile( LPCSTR, int );
HNDFILE FASTCALL Amdb_CreateFile( LPCSTR, int );
BOOL FASTCALL Amdb_QueryFileExists( LPCSTR );
BOOL FASTCALL Amdb_DeleteFile( LPCSTR );
BOOL FASTCALL Amdb_RenameFile( LPCSTR, LPCSTR );
BOOL FASTCALL Amdb_CopyFile( LPCSTR, LPCSTR );

HNDFILE FASTCALL Amdb_UserOpenFile( LPCSTR, int );
BOOL FASTCALL Amdb_UserDeleteFile( LPCSTR );

void FASTCALL Amdb_MoveCookies( PTL, PCL );
