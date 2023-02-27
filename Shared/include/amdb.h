/* AMDB.H - Database header file.
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

#ifndef  _AMDB_INCLUDED

typedef struct tagCAT FAR * PCAT;
typedef struct tagCNL FAR * PCL;
typedef struct tagTL FAR * PTL;
typedef struct tagTH FAR * PTH;
typedef struct tagAH FAR * PAH;

/* Create database structure and definitions.
 */
typedef struct tagDBCREATESTRUCT {
   char szName[ 60 ];                     /* Database name */
   char szFilename[ 260 ];                /* Filename */
   char szPassword[ 40 ];                 /* Password */
   DWORD dwType;                          /* Type of database */
} DBCREATESTRUCT, FAR * LPDBCREATESTRUCT;

#define  VM_VIEWREF           0
#define  VM_VIEWROOTS         1
#define  VM_VIEWCHRON         2
#define  VM_VIEWREFEX         3
#define  VM_USE_ORIGINAL      255

/* Column sort mode flags.
 */
#define  VSM_REFERENCE        0
#define  VSM_ROOTS            1
#define  VSM_MSGNUM           2
#define  VSM_SENDER           3
#define  VSM_DATETIME         4
#define  VSM_MSGSIZE          5
#define  VSM_SUBJECT          6
#define  VSM_ASCENDING        0x80

/* Amdb_CreateFolder constants.
 */
#define  CFF_FIRST      0x0001
#define  CFF_LAST       0x0002
#define  CFF_SORT       0x0003
#define  CFF_KEEPATTOP  0x8000

/* Message viewer header flags.
 */
#define  VH_MSGNUM            1
#define  VH_SENDER            2
#define  VH_DATETIME          4
#define  VH_MSGSIZE           8
#define  VH_SUBJECT           16

typedef struct tagVIEWDATA {
   BYTE nSortMode;                        /* Sort mode */
   BYTE nViewMode;                        /* View mode */
   WORD wHeaderFlags;                     /* Header flags */
   WORD nThreadIndentPixels;              /* Thread indentation depth */
   WORD nHdrDivisions[ 5 ];               /* Header control columns */
} VIEWDATA;

typedef struct tagNEWMSGSTRUCT {
   PTL ptl;                               /* Handle of topic to which message belongs */
   DWORD dwMsg;                           /* Message number */
   DWORD dwComment;                       /* Number of comment to message */
   LPCSTR lpszTitle;                      /* Pointer to title of message */
   LPCSTR lpszAuthor;                     /* Pointer to name of author of message */
   LPCSTR lpszMessageId;                  /* Pointer to message ID */
   AM_DATE FAR * lpDate;                  /* Pointer to message data structure */
   AM_TIME FAR * lpTime;                  /* Pointer to message time structure */
   HPSTR lpszMsg;                         /* Pointer to message body */
   DWORD cbsz;                            /* Size of message body */
   WORD dwFlags;                          /* Flags */
} NEWMSGSTRUCT;

#define  LEN_FOLDERNAME          14       /* A CIX folder name */
#define  LEN_CONFNAME            14       /* A CIX conference name */
#define  LEN_TOPICNAME           77       /* A CIX topic or Usenet newsgroup name */
#define  LEN_CATNAME             79       /* Database */
#define  LEN_TITLE               79       /* Say subject */
#define  LEN_INTERNETNAME        99       /* A full Internet address */
#define  LEN_REPLY               80
#define  LEN_RFCTYPE             30

/* Error codes.
 */
#define  AMDBERR_NOERROR               0
#define  AMDBERR_BADFORMAT             -1
#define  AMDBERR_BADVERSION            -2
#define  AMDBERR_OUTOFMEMORY           -3
#define  AMDBERR_BADPASSWORD           -4
#define  AMDBERR_FILENOTFOUND          -5
#define  AMDBERR_NOPERMISSION          -6
#define  AMDBERR_OUTOFDISKSPACE        -7
#define  AMDBERR_CANNOTCREATEFILE      -8
#define  AMDBERR_BADSCRATCHPAD         -9
#define  AMDBERR_CANTHANDLEVERBOSE     -10
#define  AMDBERR_IGNOREDERROR          -11
#define  AMDBERR_CANNOTOPENFILE        -12
#define  AMDBERR_BUFTOOSMALL           -13
#define  AMDBERR_USERABORT             -14
#define  AMDBERR_DISKREADERROR         -15
#define  AMDBERR_DISKWRITEERROR        -16
#define  AMDBERR_TOPICLOCKED           -17
#define  AMDBERR_CANNOTBACKUP          -18

#define  DF_EXPANDED             0x00000001

#define  CF_EXPANDED             0x0001
#define  CF_DELETED              0x0004
#define  CF_KEEPATTOP            0x0008

/* Topic flags.
 */
#define  TF_ONDISK               0x0001
#define  TF_CHANGED              0x0002
#define  TF_HASFILES             0x0004
#define  TF_READONLY             0x0008
#define  TF_RESIGNED             0x0010
#define  TF_IGNORED              0x0020
#define  TF_MAILINGLIST          0x0040
#define  TF_TOPICFULL            0x0080
#define  TF_BUILDTHREADLINES     0x0100
#define  TF_ATTACHCHANGED        0x0200
#define  TF_READ50               0x0400
#define  TF_MARKALLREAD          0x0800
#define  TF_THREADINTOPIC        0x1000
#define  TF_DELETED              0x2000
#define  TF_READFULL             0x4000
#define  TF_TOPICINUSE           0x8000
#define TF_LISTFOLDER            0x10000
#define  TF_USER_FLAGS           (TF_RESIGNED|TF_IGNORED|TF_READ50|TF_MARKALLREAD|TF_READFULL)

/* Message flags
 */
#define  MF_READ                 0x00000001
#define  MF_KEEP                 0x00000002
#define  MF_MARKED               0x00000004
#define  MF_DELETED              0x00000008
#define  MF_IGNORE               0x00000010
#define  MF_PRIORITY             0x00000020
#define  MF_DESTROYED            0x00000040
#define  MF_WITHDRAWN            0x00000080
#define  MF_COVERNOTE            0x00000100
#define  MF_MAILMSG              0x00000200
#define  MF_READLOCK             0x00000400
#define  MF_PURGED               0x00000800
#define  MF_OUTBOXMSG            0x00001000
#define  MF_ATTACHMENTS          0x00002000
#define  MF_XPOSTED              0x00004000
#define  MF_HDRONLY              0x00008000
#define  MF_BEINGRETRIEVED       0x00010000
#define  MF_TAGGED               0x00020000
#define  MF_UNAVAILABLE          0x00040000
#define  MF_WATCH                0x00080000
#define  MF_NOTIFICATION         0x00100000
#define	 MF_MSGDECODED           0x00200000

#define  FTYPE_NEWS              0
#define  FTYPE_MAIL              1
#define  FTYPE_CIX               2
#define  FTYPE_UNTYPED           3
#define  FTYPE_LOCAL             5
#define  FTYPE_ALL               15
#define  FTYPE_TYPEMASK          15

#define  FTYPE_CATEGORIES        0x0100
#define  FTYPE_CONFERENCES       0x0200
#define  FTYPE_TOPICS            0x0400
#define  FTYPE_GROUPMASK         0xFF00

#define  PO_SIZEPRUNE            0x0001
#define  PO_DATEPRUNE            0x0002
#define  PO_BACKUP               0x0008
#define  PO_DELETEONLY           0x0020
#define  PO_PURGE_IGNORED        0x0040
#define  PO_NOPURGE              0x8000

typedef struct tagPURGEOPTIONS {
   WORD wFlags;                           /* Purge options flags */
   WORD wMaxSize;                         /* Maximum topic size if PO_SIZEPRUNE */
   WORD wMaxDate;                         /* Maximum days to keep if PO_DATEPRUNE */
} PURGEOPTIONS;

typedef struct tagCATEGORYINFO {
   char szCategoryName[ 60 ];             /* Category name */
   DWORD cMsgs;                           /* Number of messages in this category */
   DWORD cUnRead;                         /* Number of un-read messages */
   WORD cFolders;                         /* Number of folders in this category */
   WORD wCreationTime;                    /* Time when category created */
   WORD wCreationDate;                    /* Date when category created */
   DWORD dwFlags;                         /* Category flags */
} CATEGORYINFO;

typedef struct tagFOLDERINFO {
   char szFolderName[ 80 ];               /* Folder name */
   char szFileName[ 9 ];                  /* Folder file name */
   DWORD cMsgs;                           /* Number of messages in this folder */
   DWORD cUnRead;                         /* Number of un-read messages */
   WORD cTopics;                          /* Number of topics in folder */
   WORD wCreationTime;                    /* Time when folder created */
   WORD wCreationDate;                    /* Date when folder created */
   char szSigFile[ 9 ];                   /* Folder signature file */
   PURGEOPTIONS po;                       /* Folder purge options */
   WORD wLastPurgeDate;                   /* Date of last purge */
   WORD wLastPurgeTime;                   /* Time of last purge */
} FOLDERINFO;

typedef struct tagTOPICINFO {
   char szTopicName[ LEN_TOPICNAME+1 ];   /* Topic name */
   char szFileName[ 9 ];                  /* Unique topic file name */
   DWORD dwFlags;                         /* Topic flags */
   DWORD cMsgs;                           /* Number of messages in this topic */
   DWORD cUnRead;                         /* Number of un-read messages */
   DWORD cPriority;                       /* Number of priority messages */
   DWORD cMarked;                         /* Number of marked messages */
   DWORD dwMinMsg;                        /* Smallest message in this topic */
   DWORD dwMaxMsg;                        /* Largest message in this topic */
   DWORD dwUpperMsg;                      /* Upper message count */
   char szSigFile[ 9 ];                   /* Topic signature file */
   WORD wCreationTime;                    /* Time when topic created */
   WORD wCreationDate;                    /* Date when topic created */
   PURGEOPTIONS po;                       /* Topic purge options */
   VIEWDATA vd;                           /* Topic view data */
   WORD wLastPurgeDate;                   /* Date of last purge */
   WORD wLastPurgeTime;                   /* Time of last purge */
} TOPICINFO;

typedef struct tagCURMSG {
   LPVOID pFolder;                        /* Active folder */
   PCAT pcat;                             /* Database */
   PCL pcl;                               /* Folder */
   PTL ptl;                               /* Topic */
   PTH pth;                               /* Message */
} CURMSG;

typedef struct tagMSGINFO {
   DWORD dwMsg;                           /* Message number */
   DWORD dwComment;                       /* Message to which this is a comment */
   char szTitle[ LEN_TITLE+1 ];           /* Title of this message */
   char szAuthor[ LEN_INTERNETNAME+1 ];   /* Author of this message */
   char szReply[ LEN_REPLY+1 ];           /* Message identification */
   DWORD dwFlags;                         /* Message flags (MF_xxx) */
   WORD wDate;                            /* Date of this message (packed) */
   WORD wTime;                            /* Time of this message (packed) */
   DWORD cUnRead;                         /* Number of un-read messages */
   BYTE wLevel;                           /* Level number of message */
   BYTE cPriority;                        /* Non-zero if thread has priority messages */
   WORD wComments;                        /* Number of comments to message */
   DWORD cUnReadInThread;                 /* Number of un-read messages in thread */
   DWORD dwActual;                        /* Physical mail message number */
   DWORD dwSize;                          /* Message size, in bytes/lines */
} MSGINFO;

/* Attachment structure.
 */
typedef struct tagATTACHMENT {
   DWORD dwMsg;                           /* Message to which attachment applies */
   char szPath[ 260 ];                    /* Full path to attachment */
} ATTACHMENT;

#define  PCI_PURGING             1
#define  PCI_BACKUP              2
#define  PCI_YIELD               3
#define  PCI_COMPACTING          4
#define  PCI_PURGINGFINISHED     5

typedef struct tagPURGECALLBACKINFO {
   PCL pcl;                               /* Folder handle */
   PTL ptl;                               /* Topics handle */
   PTH pth;                               /* Message handle */
   DWORD dwMsg;                           /* Message number */
   DWORD cMsgs;                           /* Total number of msgs in topic */
} PURGECALLBACKINFO;

typedef int (WINAPI *PURGEPROC)( int, PURGECALLBACKINFO FAR * );

#define  Amdb_IsRead(p)                Amdb_TestMsgFlags((p),MF_READ)
#define  Amdb_IsReadLock(p)            Amdb_TestMsgFlags((p),MF_READLOCK)
#define  Amdb_IsPriority(p)            Amdb_TestMsgFlags((p),MF_PRIORITY)
#define  Amdb_IsIgnored(p)             Amdb_TestMsgFlags((p),MF_IGNORE)
#define  Amdb_IsDeleted(p)             Amdb_TestMsgFlags((p),MF_DELETED)
#define  Amdb_IsMarked(p)              Amdb_TestMsgFlags((p),MF_MARKED)
#define  Amdb_IsBeingRetrieved(p)      Amdb_TestMsgFlags((p),MF_BEINGRETRIEVED)
#define  Amdb_IsHeaderMsg(p)           Amdb_TestMsgFlags((p),MF_HDRONLY)
#define  Amdb_IsKept(p)                Amdb_TestMsgFlags((p),MF_KEEP)
#define  Amdb_IsTagged(p)              Amdb_TestMsgFlags((p),MF_TAGGED)
#define  Amdb_IsMsgHasAttachments(p)   Amdb_TestMsgFlags((p),MF_ATTACHMENTS)
#define  Amdb_IsMsgUnavailable(p)      Amdb_TestMsgFlags((p),MF_UNAVAILABLE)
#define  Amdb_IsXPosted(p)             Amdb_TestMsgFlags((p),MF_XPOSTED)
#define  Amdb_IsWithdrawn(p)           Amdb_TestMsgFlags((p),MF_WITHDRAWN)
#define  Amdb_IsWatched(p)             Amdb_TestMsgFlags((p),MF_WATCH)
#define  Amdb_NeedNotification(p)      Amdb_TestMsgFlags((p),MF_NOTIFICATION)
#define  Amdb_IsDecoded(p)			   Amdb_TestMsgFlags((p),MF_MSGDECODED)

/* Get/Set bits */
#define  BSB_CLRBIT           0
#define  BSB_SETBIT           1
#define  BSB_GETBIT           2

typedef struct tagDATABASEINFO {
   char szFilename[ 260 ];                /* Database file name */
   char szName[ 60 ];                     /* Database name */
   char szOwner[ 40 ];                    /* Owner of this database */
   UINT cCategories;                      /* Number of categories */
   WORD timeCreated;                      /* Time when db was created */
   WORD dateCreated;                      /* Date when db was created */
} DATABASEINFO;

typedef BOOL (FAR PASCAL * DATABASEENUMPROC)( DATABASEINFO FAR *, LPARAM );

/* Functions that work on databases.
 */
BOOL WINAPI Amdb_CreateDatabase( LPDBCREATESTRUCT );
BOOL WINAPI Amdb_CommitDatabase( BOOL );
void WINAPI Amdb_PhysicalDeleteDatabase( char * );
int WINAPI Amdb_CloseDatabase( void );
BOOL WINAPI Amdb_EnumDatabases( DATABASEENUMPROC, LPARAM );
void WINAPI Amdb_SetDefaultPurgeOptions( PURGEOPTIONS FAR * );
void WINAPI Amdb_SetDefaultViewData( VIEWDATA FAR * );
void WINAPI Amdb_SweepAndDiscard( void );
DWORD WINAPI Amdb_GetTotalUnread( void );
DWORD WINAPI Amdb_GetTotalUnreadPriority( void );
DWORD WINAPI Amdb_GetTotalTagged( void );
void WINAPI Amdb_SetDataDirectory( LPSTR );
int WINAPI Amdb_LoadDatabase( LPSTR );
BOOL WINAPI Amdb_RebuildDatabase( PCAT, HWND );

/* Functions that work on categories.
 */
PCAT WINAPI Amdb_CreateCategory( LPSTR );
PCAT WINAPI Amdb_OpenCategory( LPSTR );
int WINAPI Amdb_DeleteCategory( PCAT );
PCAT WINAPI Amdb_GetNextCategory( PCAT );
PCAT WINAPI Amdb_GetPreviousCategory( PCAT );
PCAT WINAPI Amdb_GetFirstCategory( void );
PCAT WINAPI Amdb_GetLastCategory( void );
BOOL WINAPI Amdb_IsCategoryPtr( PCAT );
PCAT WINAPI Amdb_CategoryFromFolder( PCL );
void WINAPI Amdb_GetCategoryInfo( PCAT, CATEGORYINFO FAR * );
LPCSTR WINAPI Amdb_GetCategoryName( PCAT );
BOOL WINAPI Amdb_SetCategoryName( PCAT, LPCSTR );
void WINAPI Amdb_SetCategoryFlags( PCAT, DWORD, BOOL );
DWORD WINAPI Amdb_GetCategoryFlags( PCAT, DWORD );
int WINAPI Amdb_MoveCategory( PCAT, PCAT );

/* Functions that work on folders.
 */
PCL WINAPI Amdb_GetFirstFolder( PCAT );
PCL WINAPI Amdb_GetLastFolder( PCAT );
PCL WINAPI Amdb_GetFirstRealFolder( PCAT );
PCL WINAPI Amdb_GetLastRealFolder( PCAT );
PCL WINAPI Amdb_GetNextFolder( PCL );
PCL WINAPI Amdb_GetPreviousFolder( PCL );
PCL WINAPI Amdb_CreateFolder( PCAT, LPSTR, int );
PCL WINAPI Amdb_OpenFolder( PCAT, LPSTR );
int WINAPI Amdb_DeleteFolder( PCL );
void WINAPI Amdb_SetFolderFlags( PCL, DWORD, BOOL );
DWORD WINAPI Amdb_GetFolderFlags( PCL, DWORD );
BOOL WINAPI Amdb_IsFolderPtr( PCL );
LPCSTR WINAPI Amdb_GetFolderName( PCL );
BOOL WINAPI Amdb_SetFolderName( PCL, LPCSTR );
BOOL WINAPI Amdb_IsResignedFolder( PCL );
PCL WINAPI Amdb_GetNextUnconstrainedFolder( PCL );
PCL WINAPI Amdb_GetPreviousUnconstrainedFolder( PCL );
int WINAPI Amdb_MoveFolder( PCL, PCAT, PCL );
LPSTR WINAPI Amdb_SetModeratorList( PCL, LPSTR );
UINT WINAPI Amdb_GetModeratorList( PCL, LPSTR, UINT );
void WINAPI Amdb_GetFolderInfo( PCL, FOLDERINFO FAR * );
void WINAPI Amdb_SetFolderSignature( PCL, LPSTR );
void WINAPI Amdb_GetFolderSignature( PCL, LPSTR );
void WINAPI Amdb_SetFolderDateTime( PCL, WORD, WORD );
void WINAPI Amdb_SetFolderPurgeOptions( PCL, PURGEOPTIONS FAR * );
void WINAPI Amdb_GetFolderPurgeOptions( PCL, PURGEOPTIONS FAR * );
int WINAPI Amdb_PurgeFolder( PCL, PURGEPROC );
BOOL WINAPI Amdb_IsModerator( PCL, LPSTR );

/* Functions that work on topics.
 */
void WINAPI Amdb_DrawThreadLines( HDC, PTH, int, int, int, int, int );
int WINAPI Amdb_DeleteTopic( PTL );
BOOL WINAPI Amdb_IsTopicPtr( PTL );
PTL WINAPI Amdb_OpenTopic( PCL, LPSTR );
void WINAPI Amdb_LockTopic( PTL );
void WINAPI Amdb_UnlockTopic( PTL );
int WINAPI Amdb_GetLockCount( PTL );
void WINAPI Amdb_InternalUnlockTopic( PTL );
void WINAPI Amdb_SetTopicFlags( PTL, DWORD, BOOL );
DWORD WINAPI Amdb_GetTopicFlags( PTL, DWORD );
BOOL WINAPI Amdb_IsResignedTopic( PTL );
PTL WINAPI Amdb_CreateTopic( PCL, LPSTR, WORD );
int WINAPI Amdb_MoveTopic( PTL, PCL, PTL );
BOOL WINAPI Amdb_EmptyTopic( PTL );
LPCSTR WINAPI Amdb_GetTopicName( PTL );
BOOL WINAPI Amdb_SetTopicName( PTL, LPCSTR );
void WINAPI Amdb_GetInternalTopicName( PTL, LPSTR );
PTL WINAPI Amdb_GetFirstTopic( PCL );
PTL WINAPI Amdb_GetLastTopic( PCL );
PTL WINAPI Amdb_GetNextTopic( PTL );
PTL WINAPI Amdb_GetPreviousTopic( PTL );
void WINAPI Amdb_SetTopicSignature( PTL, LPSTR );
void WINAPI Amdb_GetTopicInfo( PTL, TOPICINFO FAR * );
void WINAPI Amdb_SetTopicPurgeOptions( PTL, PURGEOPTIONS FAR * );
void WINAPI Amdb_GetTopicPurgeOptions( PTL, PURGEOPTIONS FAR * );
void WINAPI Amdb_SetTopicViewData( PTL, VIEWDATA FAR * );
void WINAPI Amdb_DiscardTopic( PTL );
PCL WINAPI Amdb_FolderFromTopic( PTL );
int WINAPI Amdb_PurgeTopic( PTL, PURGEPROC );
int WINAPI Amdb_CompactTopic( PTL, PURGEPROC );
DWORD WINAPI Amdb_MarkTopicRead( PTL );
DWORD WINAPI Amdb_MarkTopicDateRead( PTL, WORD );

DWORD WINAPI Amdb_GetDiskSpaceUsed( PTL );
BOOL WINAPI Amdb_RethreadUsenet( HWND, PTL );
WORD WINAPI Amdb_GetTopicType( PTL );
void WINAPI Amdb_SetTopicType( PTL, WORD );
void WINAPI Amdb_SetTopicSel( PTL, HWND );
HWND WINAPI Amdb_GetTopicSel( PTL );
void WINAPI Amdb_MarkTopicInUse( PTL, BOOL );
DWORD WINAPI Amdb_GetTopicTextFileSize( PTL );
DWORD WINAPI Amdb_GetTextFileThreshold( void );

/* Functions that work on messages.
 */
BOOL WINAPI Amdb_IsMessagePtr( PTH );
BOOL WINAPI Amdb_GetSetCoverNoteBit( PTH, int );
PTL WINAPI Amdb_TopicFromMsg( PTH );
int WINAPI Amdb_CreateMsg( PTL, PTH FAR *, DWORD, DWORD, LPSTR, LPSTR, LPSTR, AM_DATE FAR *, AM_TIME FAR *, HPSTR, DWORD, DWORD );
BOOL WINAPI Amdb_FastRealFastLoadListBox( HWND, PTL, int, BOOL );
BOOL WINAPI Amdb_GetMailHeaderFieldInText( HPSTR, LPSTR, LPSTR, int, BOOL );
BOOL WINAPI Amdb_GetMailHeaderField( PTH, LPSTR, LPSTR, int, BOOL );
void WINAPI Amdb_MarkMsgWatch( PTH, BOOL, BOOL );
BOOL WINAPI Amdb_MarkMsgRead( PTH, BOOL );
BOOL WINAPI Amdb_MarkMsgReadLock( PTH, BOOL );
void WINAPI Amdb_MarkMsg( PTH, BOOL );
void WINAPI Amdb_MarkMsgTagged( PTH, BOOL );
BOOL WINAPI Amdb_MarkMsgWithdrawn( PTH, BOOL );
void WINAPI Amdb_MarkMsgDelete( PTH, BOOL );
void WINAPI Amdb_MarkMsgIgnore( PTH, BOOL, BOOL );
void WINAPI Amdb_MarkMsgKeep( PTH, BOOL );
void WINAPI Amdb_SetLines( PTH, DWORD );
void WINAPI Amdb_MarkMsgPriority( PTH, BOOL, BOOL );
BOOL WINAPI Amdb_MarkMsgCrossReferenced( PTH, BOOL );
void WINAPI Amdb_UpdateThreading( PTL );
BOOL WINAPI Amdb_GetNextUnReadMsg( CURMSG FAR *, int );
BOOL WINAPI Amdb_GetSetNotificationBit( PTH, int );
PTH WINAPI Amdb_GetMarked( PTL, PTH, int, BOOL );
PTH WINAPI Amdb_GetTagged( PTL, PTH );
PTH WINAPI Amdb_GetMsg( PTH, DWORD );
PTH WINAPI Amdb_GetNearestMsg( PTH, int );
PTH WINAPI Amdb_OpenMsg( PTL, DWORD );
PTH WINAPI Amdb_GetFirstMsg( PTL, int );
PTH WINAPI Amdb_GetLastMsg( PTL, int );
PTH WINAPI Amdb_GetNextMsg( PTH, int );
PTH WINAPI Amdb_GetPreviousMsg( PTH, int );
PTH WINAPI Amdb_GetPriorityUnRead( LPVOID, PTH, int, BOOL );
PTH WINAPI Amdb_GetRootMsg( PTH, BOOL );
PTH WINAPI Amdb_GotoMsg( PTH, DWORD );
PTH WINAPI Amdb_ExGetRootMsg( PTH, BOOL, int );
BOOL WINAPI Amdb_IsExpanded( PTH );
BOOL WINAPI Amdb_IsRootMessage( PTH );
void WINAPI Amdb_ExpandMessage( PTH, BOOL );
PTH WINAPI Amdb_ExpandThread( PTH, BOOL );
void WINAPI Amdb_DeleteMsg( PTH, BOOL );
DWORD WINAPI Amdb_GetMsgNumber( PTH );
void WINAPI Amdb_GetMsgInfo( PTH, MSGINFO FAR * );
HPSTR WINAPI Amdb_GetMsgText( PTH );
DWORD WINAPI Amdb_GetMsgTextLength( PTH );
void WINAPI Amdb_FreeMsgTextBuffer( HPSTR );
void WINAPI Amdb_CommitCachedFiles( void );
void WINAPI Amdb_MarkThreadRead( PTH );
void WINAPI Amdb_SetMsgSel( PTH, int );
int WINAPI Amdb_GetMsgSel( PTH );
void WINAPI Amdb_MarkHdrOnly( PTH, BOOL );
DWORD WINAPI Amdb_MessageIdToComment( PTL, LPSTR );
void WINAPI Amdb_MarkMsgBeingRetrieved( PTH, BOOL );
void WINAPI Amdb_MarkMsgUnavailable( PTH, BOOL );
PAH WINAPI Amdb_GetMsgAttachment( PTH, PAH, ATTACHMENT FAR * );
BOOL WINAPI Amdb_CreateAttachment( PTH, LPSTR );
BOOL WINAPI Amdb_ReplaceMsgText( PTH, HPSTR, DWORD );
DWORD WINAPI Amdb_GetMessageNumber( PTH );
DWORD WINAPI Amdb_LookupSubjectReference( PTL, LPSTR, LPSTR );
BOOL WINAPI Amdb_RenameAttachment( PTH pth, ATTACHMENT FAR * lpatItem, LPSTR lpszFilename ); /*!!SM!!*/
BOOL WINAPI Amdb_DeleteAttachment( PTH, ATTACHMENT FAR *, BOOL );
BOOL WINAPI Amdb_TestMsgFlags( PTH, DWORD );
BOOL WINAPI Amdb_SetOutboxMsgBit( PTH, BOOL );
BOOL WINAPI Amdb_GetOutboxMsgBit( PTH );
void WINAPI Amdb_MarkMsgDecoded( PTH );

/* Cookie functions.
 */
BOOL WINAPI Amdb_SetCookie( PTL, LPSTR, LPSTR );
BOOL WINAPI Amdb_GetCookie( PTL, LPSTR, LPSTR, LPSTR, int );
BOOL WINAPI Amdb_MigrateCookies( void );
BOOL WINAPI Amdb_RenameCookies( LPSTR, LPVOID, BOOL );

/* void Cls_OnInsertDatabase(HWND hwnd, PCAT pcat); */
#define HANDLE_WM_INSERTCATEGORY(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (PCAT)(lParam)), 0L)
#define FORWARD_WM_INSERTCATEGORY(hwnd, pcat, fn) \
    (void)(fn)((hwnd), WM_INSERTCATEGORY, 0, (LPARAM)(PCAT)(pcat))

/* void Cls_OnCategoryChanged(HWND hwnd, int type, PCAT pcat); */
#define HANDLE_WM_CATEGORYCHANGED(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (int)(wParam), (PCAT)(lParam)), 0L)
#define FORWARD_WM_CATEGORYCHANGED(hwnd, type, pcat, fn) \
    (void)(fn)((hwnd), WM_CATEGORYCHANGED, (WPARAM)(type), (LPARAM)(PCAT)(pcat))

/* void Cls_OnInsertFolder(HWND hwnd, PCL pcl); */
#define HANDLE_WM_INSERTFOLDER(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (PCL)(lParam)), 0L)
#define FORWARD_WM_INSERTFOLDER(hwnd, pcl, fn) \
    (void)(fn)((hwnd), WM_INSERTFOLDER, 0, (LPARAM)(PCL)(pcl))

/* void Cls_OnFolderChanged(HWND hwnd, int type, PCL pcl); */
#define HANDLE_WM_FOLDERCHANGED(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (int)(wParam), (PCL)(lParam)), 0L)
#define FORWARD_WM_FOLDERCHANGED(hwnd, type, pcl, fn) \
    (void)(fn)((hwnd), WM_FOLDERCHANGED, (WPARAM)(type), (LPARAM)(PCL)(pcl))

/* void Cls_OnInsertTopic(HWND hwnd, PTL ptl); */
#define HANDLE_WM_INSERTTOPIC(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (PTL)(lParam)), 0L)
#define FORWARD_WM_INSERTTOPIC(hwnd, pcl, fn) \
    (void)(fn)((hwnd), WM_INSERTTOPIC, 0, (LPARAM)(PTL)(ptl))

/* void Cls_OnTopicChanged(HWND hwnd, int type, PTL ptl); */
#define HANDLE_WM_TOPICCHANGED(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (int)(wParam), (PTL)(lParam)), 0L)
#define FORWARD_WM_TOPICCHANGED(hwnd, type, ptl, fn) \
    (void)(fn)((hwnd), WM_TOPICCHANGED, (WPARAM)(type), (LPARAM)(PTL)(ptl))

/* void Cls_OnDeleteCategory(HWND hwnd, BOOL fMove, PCAT pcat); */
#define HANDLE_WM_DELETECATEGORY(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (BOOL)(wParam), (PCAT)(lParam)), 0L)
#define FORWARD_WM_DELETECATEGORY(hwnd, fMove, pcat, fn) \
    (void)(fn)((hwnd), WM_DELETECATEGORY, (WPARAM)(fMove), (LPARAM)(PCAT)(pcat))

/* void Cls_OnDeleteFolder(HWND hwnd, BOOL fMove, PCL pcl); */
#define HANDLE_WM_DELETEFOLDER(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (BOOL)(wParam), (PCL)(lParam)), 0L)
#define FORWARD_WM_DELETEFOLDER(hwnd, fMove, pcl, fn) \
    (void)(fn)((hwnd), WM_DELETEFOLDER, (WPARAM)(fMove), (LPARAM)(PCL)(pcl))

/* void Cls_OnDeleteTopic(HWND hwnd, BOOL fMove, PTL ptl); */
#define HANDLE_WM_DELETETOPIC(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (BOOL)(wParam), (PTL)(lParam)), 0L)
#define FORWARD_WM_DELETETOPIC(hwnd, fMove, pcl, fn) \
    (void)(fn)((hwnd), WM_DELETETOPIC, (WPARAM)(fMove), (LPARAM)(PTL)(ptl))

/* void Cls_OnUpdateUnread(HWND hwnd, PTL ptl); */
#define HANDLE_WM_UPDATE_UNREAD(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (PTL)(lParam)), 0L)
#define FORWARD_WM_UPDATE_UNREAD(hwnd, pcl, fn) \
    (void)(fn)((hwnd), WM_UPDATE_UNREAD, 0, (LPARAM)(PTL)(ptl))

/* The folder picker dialog definitions
 */
#define  CPP_TOPICS        0x0001
#define  CPP_CONFERENCES   0x0002
#define  CPP_NOMAIL        0x8000

typedef struct tagPICKER {
   WORD wType;                   /* Type of folder being displayed (CP_xxx) */
   PCL pcl;                      /* Output: Filled with the selected folder */
   PTL ptl;                      /* Output: Filled with the selected topic */
   char szCaption[ 40 ];         /* User supplied caption for conf picker dialog */
   WORD wHelpID;                 /* Dialog help ID */
} PICKER;

typedef PICKER FAR * LPPICKER;

BOOL WINAPI Amdb_ConfPicker( HWND, LPPICKER );
HIMAGELIST WINAPI Amdb_CreateFolderImageList( void );
void WINAPI Amdb_FillFolderTree( HWND, WORD, BOOL );
HTREEITEM WINAPI Amdb_FindCategoryItem( HWND, PCAT );
HTREEITEM WINAPI Amdb_FindFolderItem( HWND, PCL );
HTREEITEM WINAPI Amdb_FindTopicItem( HWND, PTL );
HTREEITEM WINAPI Amdb_FindTopicItemInFolder( HWND, PCL, PTL );
HTREEITEM WINAPI Amdb_FindFolderItemInCategory( HWND, PCAT, PCL );

/* Check and repair.
 */
BOOL WINAPI Amdb_CheckAndRepair( HWND, LPVOID, BOOL );

#define  _AMDB_INCLUDED
#endif
