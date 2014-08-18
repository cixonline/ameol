/* AMEOLOB.C - The old Ameol out-basket interface
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

#include "warnings.h"
#include <windows.h>
#include <windowsx.h>
#include <assert.h>
#include <stdlib.h>
#include "amuser.h"
#include "amlib.h"
#include "amctrls.h"
#include <string.h>
#include "amob.h"
#include "amdb.h"
#include "cixob.h"
#include "ameol2.h"

#define  THIS_FILE   __FILE__

#define  OT_MAIL           0
#define  OT_COMMENT        1
#define  OT_SAY            2
#define  OT_FAXMAIL        3
#define  OT_RESUME         4
#define  OT_CIXLIST        5
#define  OT_FILELIST       6
#define  OT_FDL            7
#define  OT_JOIN           8
#define  OT_RESIGN         9
#define  OT_COPYMSG        10
#define  OT_PUTRESUME      11
#define  OT_WITHDRAW       12
#define  OT_BINMAIL        13
#define  OT_MAILUPLOAD     14
#define  OT_MAILDOWNLOAD   15
#define  OT_MAILERASE      16
#define  OT_MAILLIST       17
#define  OT_INCLUDE        18
#define  OT_FUL            19
#define  OT_JOINUNET       20
#define  OT_RESIGNUNET     21
#define  OT_INLINE         22
#define  OT_OLRINDEX       23
#define  OT_INDEXDOWNLOAD  24
#define  OT_FILEMSG        25
#define  OT_EXPORT         26
#define  OT_GETPARLIST     27
#define  OT_ADDPAR         28
#define  OT_REMPAR         29
#define  OT_UUNETSAY       30
#define  OT_CIXHELPLIST    32
#define  OT_CIXHELP        33
#define  OT_EXMOD          34
#define  OT_COMOD          35
#define  OT_UUNETLIST      36
#define  OT_USERLIST       37
#define  OT_MAILRENAME     38
#define  OT_PREINCLUDE     39
#define  OT_CLRMAIL        40
#define  OT_CLRMAILIMM     41
#define  OT_GETNEWSGROUPS  42
#define  OT_FDIR           43

/* Old object flags passed in OBHEADER in
 * PutObject.
 */
#define  OLD_OBF_HOLD      0x0001            /* Item is being held */
#define  OLD_OBF_KEEP      0x0004            /* Item is processed on every blink */

#define  LEN_INTERNETNAME  99                /* A full Internet address */
#define  LEN_CONFNAME      14                /* A CIX conference name */
#define  LEN_TOPICNAME     77                /* A CIX topic or Usenet newsgroup name */
#define  LEN_TITLE         79                /* Say subject */
#define  LEN_FAXNO         20                /* Length of a fax number */
#define  LEN_FAXTO         31                /* Length of a fax recipient name */
#define  LEN_FAXFROM       31                /* Length of a fax sender name */
#define  LEN_FAXSUBJECT    60                /* Length of a fax subject */
#define  LEN_AUTHORLIST    143               /* List of CIX users */
#define  LEN_REPLY         80                /* Length of a mail reply address */
#define  LEN_CIXFILENAME   14                /* Maximum size of a CIX file name */
#define  LEN_PLATFORM      39                /* Name of a CIX index platform */
#define  LEN_INDEXSEARCH   99                /* Name of a CIX index search string */
#define  LEN_FILENAME      259               /* Maximum size of a DOS file name (+extension) */
#define  LEN_DIRNAME       259               /* Maximum size of a directory path */
#define  LEN_PATHNAME      260               /* Maximum size of full DOS pathname (directory+filename) */
#define  LEN_DESCRIPTION   79                /* Length of a script description */
#define  LEN_DOSFILENAME   12                /* Maximum size of a DOS file name (+extension) */
#define  LEN_SUBJECT       80                /* Mail subject */
#define  LEN_MAILADDR      511               /* Length of a mail address */
#define  LEN_FULLNAME      ((LEN_CONFNAME)+(LEN_TOPICNAME)+1)

typedef LPVOID OLD_LPOB;

#pragma pack(1)
typedef struct tagOLD_OBHEADER {
   BYTE type;                    /* Type of object */
   WORD wSize;                   /* Size of structure */
   WORD wFlags;                  /* Object flags */
   WORD wDate;                   /* Date when object was posted */
   WORD wTime;                   /* Time when object was posted */
   DWORD dwOffset;               /* Offset of message in out-basket */
   DWORD dwSize;                 /* Size of message in out-basket */
} OLD_OBHEADER;

typedef struct tagOLD_GETPARLISTOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szConfName[ LEN_CONFNAME+1 ];        /* Conference name */
} OLD_GETPARLISTOBJECT;

typedef struct tagOLD_ADDPAROBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szConfName[ LEN_CONFNAME+1 ];        /* Conference name */
   char szUserName[ LEN_AUTHORLIST+1 ];      /* User names */
} OLD_ADDPAROBJECT;

typedef struct tagOLD_REMPAROBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szConfName[ LEN_CONFNAME+1 ];        /* Conference name */
   char szUserName[ LEN_AUTHORLIST+1 ];      /* User names */
} OLD_REMPAROBJECT;

typedef struct tagOLD_COMODOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szConfName[ LEN_CONFNAME+1 ];        /* Conference name */
   char szUserName[ LEN_AUTHORLIST+1 ];      /* User names */
} OLD_COMODOBJECT;

typedef struct tagOLD_EXMODOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szConfName[ LEN_CONFNAME+1 ];        /* Conference name */
   char szUserName[ LEN_AUTHORLIST+1 ];      /* User names */
} OLD_EXMODOBJECT;

typedef struct tagOLD_CLRMAILOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
} OLD_CLRMAILOBJECT;

typedef struct tagGETNEWSGROUPSOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
} GETNEWSGROUPSOBJECT;

typedef struct tagOLD_EXPORTOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szConfName[ LEN_CONFNAME+1 ];        /* Conference name */
   char szTopicName[ LEN_TOPICNAME+1 ];      /* Topic name */
   char szFileName[ LEN_CIXFILENAME+1 ];     /* Name of file to export */
   WORD wFlags;                              /* Export flags */
} OLD_EXPORTOBJECT;

typedef struct tagOLD_FILEMSGOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   DWORD dwStart;
   DWORD dwEnd;
   char szConfName[ LEN_CONFNAME+1 ];        /* Conference name */
   char szTopicName[ LEN_TOPICNAME+1 ];      /* Topic name */
} OLD_FILEMSGOBJECT;

typedef struct tagOLRINDEXOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szPlatform[ LEN_PLATFORM+1 ];        /* Platform name */
   char szSearchText[ LEN_INDEXSEARCH+1 ];   /* Search text */
   WORD wLimit;                              /* Limit of search */
} OLRINDEXOBJECT;

typedef struct tagINDEXDOWNLOADOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szPlatform[ LEN_PLATFORM+1 ];        /* Platform name */
   char szCIXFileName[ LEN_CIXFILENAME+1 ];  /* Name of file to download as on CIX */
   char szFileName[ LEN_FILENAME+1 ];        /* Name of file to download */
   char szDirectory[ LEN_DIRNAME+1 ];        /* Directory to which to download */
} INDEXDOWNLOADOBJECT;

typedef struct tagOLD_INCLUDEOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szFileName[ LEN_PATHNAME+1 ];        /* File name */
   char szDescription[ LEN_DESCRIPTION+1 ];  /* Description */
} OLD_INCLUDEOBJECT;

typedef struct tagOLD_INLINEOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szFileName[ LEN_PATHNAME+1 ];        /* File name */
   char szDescription[ LEN_DESCRIPTION+1 ];  /* Description */
} OLD_INLINEOBJECT;

typedef struct tagOLD_WITHDRAWOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   DWORD dwMsg;                              /* Message number */
   char szConfName[ LEN_CONFNAME+1 ];        /* Conference name */
   char szTopicName[ LEN_TOPICNAME+1 ];      /* Topic name */
} OLD_WITHDRAWOBJECT;

typedef struct tagOLD_COPYMSGOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   DWORD dwMsg;                              /* Message number */
   char szConfName[ LEN_CONFNAME+1 ];        /* Conference name */
   char szTopicName[ LEN_TOPICNAME+1 ];      /* Topic name */
   char szCurConfName[ LEN_CONFNAME+1 ];     /* Conference name */
   char szCurTopicName[ LEN_TOPICNAME+1 ];   /* Topic name */
} OLD_COPYMSGOBJECT;

typedef struct tagOLD_JOINOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szConfName[ LEN_FULLNAME+1 ];        /* Conference name */
   WORD wRecent;                             /* Recent messages on join */
} OLD_JOINOBJECT;

typedef struct tagOLD_JOINUNETOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szGroupName[ LEN_TOPICNAME+1 ];      /* Conference name */
   WORD wRecent;                             /* Recent messages on join */
} OLD_JOINUNETOBJECT;

typedef struct tagOLD_RESIGNUNETOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szGroupName[ LEN_TOPICNAME+1 ];      /* Conference name */
} OLD_RESIGNUNETOBJECT;

typedef struct tagOLD_BINMAILOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szFileName[ LEN_PATHNAME+1 ];        /* File name */
   char szUserName[ LEN_AUTHORLIST+1 ];      /* User name */
   DWORD dwSize;                             /* Size of binmail object */
   WORD wFlags;                              /* Binmail flags */
} OLD_BINMAILOBJECT;

typedef struct tagOLD_MAILERASEOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szFileName[ LEN_CIXFILENAME+1 ];     /* File name */
} OLD_MAILERASEOBJECT;

typedef struct tagOLD_MAILRENAMEOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szOldName[ LEN_CIXFILENAME+1 ];      /* Old file name */
   char szNewName[ LEN_CIXFILENAME+1 ];      /* New file name */
} OLD_MAILRENAMEOBJECT;

typedef struct tagOLD_MAILLISTOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
} OLD_MAILLISTOBJECT;

typedef struct tagOLD_MAILUPLOADOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szFileName[ LEN_PATHNAME+1 ];        /* Local file name, including path */
} OLD_MAILUPLOADOBJECT;

typedef struct tagOLD_MAILDOWNLOADOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szCIXFileName[ LEN_CIXFILENAME+1];   /* CIX file name */
   char szFileName[ LEN_FILENAME+1 ];        /* File name, including path */
   char szDirectory[ LEN_DIRNAME+1 ];        /* Directory to which to download */
   WORD wFlags;                              /* Download flags */
} OLD_MAILDOWNLOADOBJECT;

typedef struct tagOLD_RESIGNOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szConfName[ LEN_CONFNAME+1 ];        /* Conference name */
   char szTopicName[ LEN_TOPICNAME+1 ];      /* Topic name */
} OLD_RESIGNOBJECT;

typedef struct tagOLD_FDLOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szConfName[ LEN_CONFNAME+1 ];        /* Conference name */
   char szTopicName[ LEN_TOPICNAME+1 ];      /* Topic name */
   char szCIXFileName[ LEN_CIXFILENAME+1 ];  /* CIX file name */
   char szFileName[ LEN_FILENAME+1 ];        /* Name of file to download */
   char szDirectory[ LEN_DIRNAME+1 ];        /* Directory to which to download */
   WORD wFlags;                              /* Download flags */
} OLD_FDLOBJECT;

typedef struct tagOLD_FULOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szConfName[ LEN_CONFNAME+1 ];        /* Conference name */
   char szTopicName[ LEN_TOPICNAME+1 ];      /* Topic name */
   char szFileName[ LEN_PATHNAME+1 ];        /* Full pathname of file to upload */
} OLD_FULOBJECT;

typedef struct tagOLD_FILELISTOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szConfName[ LEN_CONFNAME+1 ];        /* Conference name */
   char szTopicName[ LEN_TOPICNAME+1 ];      /* Topic name */
   char szFileName[ LEN_DOSFILENAME+1 ];     /* Filename */
} OLD_FILELISTOBJECT;

typedef struct tagOLD_FDIROBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szConfName[ LEN_CONFNAME+1 ];        /* Conference name */
   char szTopicName[ LEN_TOPICNAME+1 ];      /* Topic name */
} OLD_FDIROBJECT;

typedef struct tagCIXLISTOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
} CIXLISTOBJECT;

typedef struct tagUUNETLISTOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
} UUNETLISTOBJECT;

typedef struct tagUSERLISTOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
} USERLISTOBJECT;

typedef struct tagCIXHELPLISTOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
} CIXHELPLISTOBJECT;

typedef struct tagCIXHELPOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szHelp[ 40 ];                        /* Help subject */
} CIXHELPOBJECT;

typedef struct tagOLD_RESUMEOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szCixName[ LEN_INTERNETNAME+1 ];     /* Name of user */
   char szFileName[ LEN_DOSFILENAME+1 ];     /* Filename */
} OLD_RESUMEOBJECT;

typedef struct tagOLD_SAYOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szConfName[ LEN_CONFNAME+1 ];        /* Conference name */
   char szTopicName[ LEN_TOPICNAME+1 ];      /* Topic name */
   char szTitle[ LEN_TITLE+1 ];              /* Title of message */
} OLD_SAYOBJECT;

typedef struct tagOLD_COMMENTOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szConfName[ LEN_CONFNAME+1 ];        /* Conference name */
   char szTopicName[ LEN_TOPICNAME+1 ];      /* Topic name */
   DWORD dwComment;                          /* Number of message commented to */
} OLD_COMMENTOBJECT;

typedef struct tagOLD_MAILOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szTo[ LEN_MAILADDR+1 ];              /* To whom message is addressed */
   char szCC[ LEN_MAILADDR+1 ];              /* Carbon-copy list */
   char szSubject[ LEN_SUBJECT+1 ];          /* Subject title */
   char szReply[ LEN_REPLY+1 ];              /* Path and ID of original message */
   WORD wFlags;                              /* Mail flags */
} OLD_MAILOBJECT;

typedef struct tagOLD_UUNETSAYOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   char szTo[ LEN_INTERNETNAME+1 ];          /* To whom message is addressed */
   char szSubject[ LEN_SUBJECT+1 ];          /* Subject title */
   char szReply[ LEN_REPLY+1 ];              /* Path and ID of original message */
   WORD wFlags;                              /* Mail flags */
} OLD_UUNETSAYOBJECT;

typedef struct tagOLD_FAXOBJECT {
   OLD_OBHEADER obHdr;                       /* Object header */
   WORD wFaxFlags;                           /* Fax flags */
   char szNumber[ LEN_FAXNO+1 ];             /* Fax destination number */
   char szTo[ LEN_FAXTO+1 ];                 /* Name of recipient */
   char szFrom[ LEN_FAXFROM+1 ];             /* Name of sender */
   char szSubject[ LEN_FAXSUBJECT+1 ];       /* Subject title */
} OLD_FAXOBJECT;
#pragma pack()

OLD_LPOB PASCAL PutMailListObject( OLD_LPOB, OLD_MAILLISTOBJECT FAR *, LPSTR );
OLD_LPOB PASCAL PutFileMsgObject( OLD_LPOB, OLD_FILEMSGOBJECT FAR *, LPSTR );
OLD_LPOB PASCAL PutSayObject( OLD_LPOB, OLD_SAYOBJECT FAR *, LPSTR );
OLD_LPOB PASCAL PutCommentObject( OLD_LPOB, OLD_COMMENTOBJECT FAR *, LPSTR );
OLD_LPOB PASCAL PutMailObject( OLD_LPOB, OLD_MAILOBJECT FAR *, LPSTR );
OLD_LPOB PASCAL PutFaxMailObject( OLD_LPOB, OLD_FAXOBJECT FAR *, LPSTR );
OLD_LPOB PASCAL PutArticleObject( OLD_LPOB, OLD_UUNETSAYOBJECT FAR *, LPSTR );
OLD_LPOB PASCAL PutIncludeObject( int, OLD_LPOB, OLD_INCLUDEOBJECT FAR *, LPSTR );
OLD_LPOB PASCAL PutInlineObject( OLD_LPOB, OLD_INLINEOBJECT FAR *, LPSTR );
OLD_LPOB PASCAL PutFilelistObject( OLD_LPOB, OLD_FILELISTOBJECT FAR *, LPSTR );
OLD_LPOB PASCAL PutFdirObject( OLD_LPOB, OLD_FDIROBJECT FAR *, LPSTR );
OLD_LPOB PASCAL PutFileDownloadObject( OLD_LPOB, OLD_FDLOBJECT FAR *, LPSTR );
OLD_LPOB PASCAL PutFileUploadObject( OLD_LPOB, OLD_FULOBJECT FAR *, LPSTR );
OLD_LPOB PASCAL PutJoinObject( OLD_LPOB, OLD_JOINOBJECT FAR *, LPSTR );
OLD_LPOB PASCAL PutClrMailObject( OLD_LPOB, OLD_CLRMAILOBJECT FAR *, LPSTR );
OLD_LPOB PASCAL PutGetParListObject( OLD_LPOB, OLD_GETPARLISTOBJECT FAR *, LPSTR );
OLD_LPOB PASCAL PutAddParObject( OLD_LPOB, OLD_ADDPAROBJECT FAR *, LPSTR );
OLD_LPOB PASCAL PutResumeObject( OLD_LPOB , OLD_RESUMEOBJECT FAR *, LPSTR );
LPVOID FASTCALL GetObjectFromLPOB( LPOB );
void FASTCALL SplitTopic( LPSTR, LPSTR, LPSTR );

/* This function implements the PutObject API call.
 */
OLD_LPOB EXPORT FAR PASCAL PutObject( OLD_LPOB lpOldObj, OLD_OBHEADER FAR * lpNewObj, LPSTR lpszMsg )
{
   OLD_LPOB lpo;

   /* First deal with the actual
    * object.
    */
   lpo = NULL;
   switch( lpNewObj->type )
      {
      case OT_RESUME:
         lpo = PutResumeObject( lpOldObj, (OLD_RESUMEOBJECT FAR *)lpNewObj, lpszMsg );
         break;

      case OT_ADDPAR:
         lpo = PutAddParObject( lpOldObj, (OLD_ADDPAROBJECT FAR *)lpNewObj, lpszMsg );
         break;

      case OT_CLRMAIL:
         lpo = PutClrMailObject( lpOldObj, (OLD_CLRMAILOBJECT FAR *)lpNewObj, lpszMsg );
         break;

      case OT_GETPARLIST:
         lpo = PutGetParListObject( lpOldObj, (OLD_GETPARLISTOBJECT FAR *)lpNewObj, lpszMsg );
         break;

      case OT_JOIN:
         lpo = PutJoinObject( lpOldObj, (OLD_JOINOBJECT FAR *)lpNewObj, lpszMsg );
         break;

      case OT_MAILLIST:
         lpo = PutMailListObject( lpOldObj, (OLD_MAILLISTOBJECT FAR *)lpNewObj, lpszMsg );
         break;
         
      case OT_FILEMSG:
         lpo = PutFileMsgObject( lpOldObj, (OLD_FILEMSGOBJECT FAR *)lpNewObj, lpszMsg );
         break;

      case OT_FDL:
         lpo = PutFileDownloadObject( lpOldObj, (OLD_FDLOBJECT FAR *)lpNewObj, lpszMsg );
         break;

      case OT_FUL:
         lpo = PutFileUploadObject( lpOldObj, (OLD_FULOBJECT FAR *)lpNewObj, lpszMsg );
         break;

      case OT_FILELIST:
         lpo = PutFilelistObject( lpOldObj, (OLD_FILELISTOBJECT FAR *)lpNewObj, lpszMsg );
         break;

      case OT_FDIR:
         lpo = PutFdirObject( lpOldObj, (OLD_FDIROBJECT FAR *)lpNewObj, lpszMsg );
         break;

      case OT_INLINE:
         lpo = PutInlineObject( lpOldObj, (OLD_INLINEOBJECT FAR *)lpNewObj, lpszMsg );
         break;

      case OT_INCLUDE:  
         lpo = PutIncludeObject( OBTYPE_INCLUDE, lpOldObj, (OLD_INCLUDEOBJECT FAR *)lpNewObj, lpszMsg );
         break;

      case OT_PREINCLUDE:
         lpo = PutIncludeObject( OBTYPE_PREINCLUDE, lpOldObj, (OLD_INCLUDEOBJECT FAR *)lpNewObj, lpszMsg );
         break;

      case OT_SAY:
         lpo = PutSayObject( lpOldObj, (OLD_SAYOBJECT FAR *)lpNewObj, lpszMsg );
         break;

      case OT_COMMENT:
         lpo = PutCommentObject( lpOldObj, (OLD_COMMENTOBJECT FAR *)lpNewObj, lpszMsg );
         break;

      case OT_UUNETSAY:
         lpo = PutArticleObject( lpOldObj, (OLD_UUNETSAYOBJECT FAR *)lpNewObj, lpszMsg );
         break;

      case OT_MAIL:
         lpo = PutMailObject( lpOldObj, (OLD_MAILOBJECT FAR *)lpNewObj, lpszMsg );
         break;

      case OT_FAXMAIL:
         lpo = PutFaxMailObject( lpOldObj, (OLD_FAXOBJECT FAR *)lpNewObj, lpszMsg );
         break;

      }

   /* Now set any flags passed in the OLD_OBHEADER header.
    */
   if( lpNewObj->wFlags )
      {
      OBINFO obinf;
      BOOL fKeep;
      BOOL fHold;

      fKeep = lpNewObj->wFlags & OLD_OBF_KEEP;
      fHold = lpNewObj->wFlags & OLD_OBF_HOLD;
      Amob_GetObInfo( lpo, &obinf );
      if( fKeep )
         obinf.obHdr.wFlags |= OBF_KEEP;
      if( fHold )
         obinf.obHdr.wFlags |= OBF_HOLD;
      Amob_SetObInfo( lpo, &obinf );
      }
   return( lpo );
}

/* This function returns an object handle from its
 * LPOB.
 */
LPVOID FASTCALL GetObjectFromLPOB( LPOB lpob )
{
   OBINFO obinfo;

   if( NULL == lpob )
      return( NULL );
   Amob_GetObInfo( lpob, &obinfo );
   return( obinfo.lpObData );
}

/* This function implements the PutObject API call for the OT_GETPARLIST
 * out-basket object type.
 */
OLD_LPOB PASCAL PutGetParListObject( OLD_LPOB lpOldObj, OLD_GETPARLISTOBJECT FAR * lpGetParListObj, LPSTR lpszMsg )
{
   GETPARLISTOBJECT FAR * lpgplo;
   GETPARLISTOBJECT gplo;

   lpgplo = GetObjectFromLPOB( lpOldObj );
   if( NULL == lpgplo )
      {
      Amob_New( OBTYPE_GETPARLIST, &gplo );
      lpgplo = &gplo;
      }
   Amob_CreateRefString( &lpgplo->recConfName, lpGetParListObj->szConfName );
   return( (OLD_LPOB)Amob_Commit( lpOldObj, OBTYPE_GETPARLIST, lpgplo ) );
}

/* This function implements the PutObject API call for the OT_JOINOBJECT
 * out-basket object type.
 */
OLD_LPOB PASCAL PutJoinObject( OLD_LPOB lpOldObj, OLD_JOINOBJECT FAR * lpJoinObj, LPSTR lpszMsg )
{
   JOINOBJECT FAR * lpjo;
   JOINOBJECT jo;

   lpjo = GetObjectFromLPOB( lpOldObj );
   if( NULL == lpjo )
      {
      Amob_New( OBTYPE_JOINCONFERENCE, &jo );
      lpjo = &jo;
      }
   lpjo->wRecent = lpJoinObj->wRecent;
   Amob_CreateRefString( &lpjo->recConfName, lpJoinObj->szConfName );
   return( (OLD_LPOB)Amob_Commit( lpOldObj, OBTYPE_JOINCONFERENCE, lpjo ) );
}

/* This function implements the PutObject API call for the OT_FILEMSG
 * out-basket object type.
 */
OLD_LPOB PASCAL PutFileMsgObject( OLD_LPOB lpOldObj, OLD_FILEMSGOBJECT FAR * lpFileMsgObj, LPSTR lpszMsg )
{
   GETMESSAGEOBJECT FAR * lpgm;
   char sz[ 128 ];
   GETMESSAGEOBJECT gm;

   lpgm = GetObjectFromLPOB( lpOldObj );
   if( NULL == lpgm )
      {
      Amob_New( OBTYPE_GETMESSAGE, &gm );
      lpgm = &gm;
      }
   lpgm->dwStartMsg = lpFileMsgObj->dwStart;
   lpgm->dwEndMsg = lpFileMsgObj->dwEnd;
   wsprintf( sz, "%s/%s", lpFileMsgObj->szConfName, lpFileMsgObj->szTopicName );
   Amob_CreateRefString( &lpgm->recTopicName, sz );
   return( (OLD_LPOB)Amob_Commit( lpOldObj, OBTYPE_GETMESSAGE, lpgm ) );
}

/* This function implements the PutObject API call for the OT_MAILLIST
 * out-basket object type.
 */
OLD_LPOB PASCAL PutMailListObject( OLD_LPOB lpOldObj, OLD_MAILLISTOBJECT FAR * lpNewMaillistObj, LPSTR lpszMsg )
{
   Amob_New( OBTYPE_MAILLIST, NULL );
   return( (OLD_LPOB)Amob_Commit( lpOldObj, OBTYPE_MAILLIST, NULL ) );
}

/* This function implements the PutObject API call for the OT_FDL
 * out-basket object type.
 */
OLD_LPOB PASCAL PutFileDownloadObject( OLD_LPOB lpOldObj, OLD_FDLOBJECT FAR * lpNewFdlObj, LPSTR lpszMsg )
{
   FDLOBJECT FAR * lpfdl;
   FDLOBJECT fdl;

   lpfdl = GetObjectFromLPOB( lpOldObj );
   if( NULL == lpfdl )
      {
      Amob_New( OBTYPE_DOWNLOAD, &fdl );
      lpfdl = &fdl;
      }
   Amob_CreateRefString( &lpfdl->recConfName, lpNewFdlObj->szConfName );
   Amob_CreateRefString( &lpfdl->recTopicName, lpNewFdlObj->szTopicName );
   Amob_CreateRefString( &lpfdl->recFileName, lpNewFdlObj->szFileName );
   Amob_CreateRefString( &lpfdl->recCIXFileName, lpNewFdlObj->szCIXFileName );
   Amob_CreateRefString( &lpfdl->recDirectory, lpNewFdlObj->szDirectory );
   lpfdl->wFlags = lpNewFdlObj->wFlags;
   return( (OLD_LPOB)Amob_Commit( lpOldObj, OBTYPE_DOWNLOAD, lpfdl ) );
}

/* This function implements the PutObject API call for the OT_FUL
 * out-basket object type.
 */
OLD_LPOB PASCAL PutFileUploadObject( OLD_LPOB lpOldObj, OLD_FULOBJECT FAR * lpNewFulObj, LPSTR lpszMsg )
{
   FULOBJECT FAR * lpful;
   FULOBJECT ful;

   lpful = GetObjectFromLPOB( lpOldObj );
   if( NULL == lpful )
      {
      Amob_New( OBTYPE_FUL, &ful );
      lpful = &ful;
      }
   Amob_CreateRefString( &lpful->recConfName, lpNewFulObj->szConfName );
   Amob_CreateRefString( &lpful->recTopicName, lpNewFulObj->szTopicName );
   Amob_CreateRefString( &lpful->recFileName, lpNewFulObj->szFileName );
   Amob_CreateRefString( &lpful->recText, lpszMsg );
   return( (OLD_LPOB)Amob_Commit( lpOldObj, OBTYPE_FUL, lpful ) );
}

/* This function implements the PutObject API call for the OT_FILELIST
 * out-basket object type.
 */
OLD_LPOB PASCAL PutFilelistObject( OLD_LPOB lpOldObj, OLD_FILELISTOBJECT FAR * lpNewFilelistObj, LPSTR lpszMsg )
{
   FILELISTOBJECT FAR * lpflo;
   FILELISTOBJECT flo;

   lpflo = GetObjectFromLPOB( lpOldObj );
   if( NULL == lpflo )
      {
      Amob_New( OBTYPE_FILELIST, &flo );
      lpflo = &flo;
      }
   Amob_CreateRefString( &lpflo->recConfName, lpNewFilelistObj->szConfName );
   Amob_CreateRefString( &lpflo->recTopicName, lpNewFilelistObj->szTopicName );
   return( (OLD_LPOB)Amob_Commit( lpOldObj, OBTYPE_FILELIST, lpflo ) );
}

/* This function implements the PutObject API call for the OT_FILELIST
 * out-basket object type.
 */
OLD_LPOB PASCAL PutFdirObject( OLD_LPOB lpOldObj, OLD_FDIROBJECT FAR * lpNewFdirObj, LPSTR lpszMsg )
{
   FDIROBJECT FAR * lpfdr;
   FDIROBJECT fdr;

   lpfdr = GetObjectFromLPOB( lpOldObj );
   if( NULL == lpfdr )
      {
      Amob_New( OBTYPE_FDIR, &fdr );
      lpfdr = &fdr;
      }
   Amob_CreateRefString( &lpfdr->recConfName, lpNewFdirObj->szConfName );
   Amob_CreateRefString( &lpfdr->recTopicName, lpNewFdirObj->szTopicName );
   return( (OLD_LPOB)Amob_Commit( lpOldObj, OBTYPE_FDIR, lpfdr ) );
}

/* This function implements the PutObject API call for the OT_INCLUDE
 * out-basket object type.
 */
OLD_LPOB PASCAL PutInlineObject( OLD_LPOB lpOldObj, OLD_INLINEOBJECT FAR * lpNewInlineObj, LPSTR lpszMsg )
{
   INLINEOBJECT FAR * lpso;
   INLINEOBJECT so;

   lpso = GetObjectFromLPOB( lpOldObj );
   if( NULL == lpso )
      {
      Amob_New( OBTYPE_INLINE, &so );
      lpso = &so;
      }
   Amob_CreateRefString( &lpso->recFileName, lpNewInlineObj->szFileName );
   Amob_CreateRefString( &lpso->recDescription, lpNewInlineObj->szDescription );
   return( (OLD_LPOB)Amob_Commit( lpOldObj, OBTYPE_INLINE, lpso ) );
}

/* This function implements the PutObject API call for the OT_INCLUDE and
 * OT_PREINCLUDE out-basket object type.
 */
OLD_LPOB PASCAL PutIncludeObject( int nType, OLD_LPOB lpOldObj, OLD_INCLUDEOBJECT FAR * lpNewIncludeObj, LPSTR lpszMsg )
{
   INCLUDEOBJECT FAR * lpso;
   INCLUDEOBJECT so;

   lpso = GetObjectFromLPOB( lpOldObj );
   if( NULL == lpso )
      {
      Amob_New( nType, &so );
      lpso = &so;
      }
   Amob_CreateRefString( &lpso->recFileName, lpNewIncludeObj->szFileName );
   Amob_CreateRefString( &lpso->recDescription, lpNewIncludeObj->szDescription );
   return( (OLD_LPOB)Amob_Commit( lpOldObj, nType, lpso ) );
}

/* This function implements the PutObject API call for the OT_SAY
 * out-basket object type.
 */
OLD_LPOB PASCAL PutSayObject( OLD_LPOB lpOldObj, OLD_SAYOBJECT FAR * lpNewSayObj, LPSTR lpszMsg )
{
   SAYOBJECT FAR * lpso;
   SAYOBJECT so;

   lpso = GetObjectFromLPOB( lpOldObj );
   if( NULL == lpso )
      {
      Amob_New( OBTYPE_SAYMESSAGE, &so );
      lpso = &so;
      }
   Amob_CreateRefString( &lpso->recConfName, lpNewSayObj->szConfName );
   Amob_CreateRefString( &lpso->recTopicName, lpNewSayObj->szTopicName );
   Amob_CreateRefString( &lpso->recSubject, lpNewSayObj->szTitle );
   Amob_CreateRefString( &lpso->recText, lpszMsg );
   return( (OLD_LPOB)Amob_Commit( lpOldObj, OBTYPE_SAYMESSAGE, lpso ) );
}

/* This function implements the PutObject API call for the OT_COMMENT
 * out-basket object type.
 */
OLD_LPOB PASCAL PutCommentObject( OLD_LPOB lpOldObj, OLD_COMMENTOBJECT FAR * lpNewCommentObj, LPSTR lpszMsg )
{
   COMMENTOBJECT FAR * lpco;
   COMMENTOBJECT co;

   lpco = GetObjectFromLPOB( lpOldObj );
   if( NULL == lpco )
      {
      Amob_New( OBTYPE_COMMENTMESSAGE, &co );
      lpco = &co;
      }
   Amob_CreateRefString( &lpco->recConfName, lpNewCommentObj->szConfName );
   Amob_CreateRefString( &lpco->recTopicName, lpNewCommentObj->szTopicName );
   Amob_CreateRefString( &lpco->recText, lpszMsg );
   lpco->dwReply = lpNewCommentObj->dwComment;
   return( (OLD_LPOB)Amob_Commit( lpOldObj, OBTYPE_COMMENTMESSAGE, lpco ) );
}

/* This function implements the PutObject API call for the OT_MAIL
 * out-basket object type.
 */
OLD_LPOB PASCAL PutMailObject( OLD_LPOB lpOldObj, OLD_MAILOBJECT FAR * lpNewMailObj, LPSTR lpszMsg )
{
   MAILOBJECT FAR * lpmo;
   char szReplyAddress[ 80 ];
   MAILOBJECT mo;

   lpmo = GetObjectFromLPOB( lpOldObj );
   if( NULL == lpmo )
      {
      Amob_New( OBTYPE_MAILMESSAGE, &mo );
      lpmo = &mo;
      }
   Ameol2_GetSystemParameter( "IP\\ReplyAddress", szReplyAddress, sizeof(szReplyAddress) );
   Amob_CreateRefString( &lpmo->recTo, lpNewMailObj->szTo );
   Amob_CreateRefString( &lpmo->recCC, lpNewMailObj->szCC );
   Amob_CreateRefString( &lpmo->recSubject, lpNewMailObj->szSubject );
   Amob_CreateRefString( &lpmo->recReply, lpNewMailObj->szReply );
   Amob_CreateRefString( &lpmo->recReplyTo, szReplyAddress );
   Amob_CreateRefString( &lpmo->recText, lpszMsg );
   lpmo->wFlags |= lpNewMailObj->wFlags;
   if( !( lpNewMailObj->wFlags & MOF_NOECHO ) )
      lpmo->wFlags &= ~MOF_NOECHO;
   return( (OLD_LPOB)Amob_Commit( lpOldObj, OBTYPE_MAILMESSAGE, lpmo ) );
}

/* This function implements the PutObject API call for the OT_UUNETSAY
 * out-basket object type.
 */
OLD_LPOB PASCAL PutArticleObject( OLD_LPOB lpOldObj, OLD_UUNETSAYOBJECT FAR * lpNewMailObj, LPSTR lpszMsg )
{
   ARTICLEOBJECT FAR * lpmo;
   ARTICLEOBJECT mo;

   lpmo = GetObjectFromLPOB( lpOldObj );
   if( NULL == lpmo )
      {
      Amob_New( OBTYPE_ARTICLEMESSAGE, &mo );
      lpmo = &mo;
      }
   Amob_CreateRefString( &lpmo->recNewsgroup, lpNewMailObj->szTo );
   Amob_CreateRefString( &lpmo->recSubject, lpNewMailObj->szSubject );
   Amob_CreateRefString( &lpmo->recReply, lpNewMailObj->szReply );
   Amob_CreateRefString( &lpmo->recText, lpszMsg );
   return( (OLD_LPOB)Amob_Commit( lpOldObj, OBTYPE_ARTICLEMESSAGE, lpmo ) );
}

/* This function implements the PutObject API call for the OT_FAXMAIL
 * out-basket object type.
 */
OLD_LPOB PASCAL PutFaxMailObject( OLD_LPOB lpOldObj, OLD_FAXOBJECT FAR * lpNewFaxMailObj, LPSTR lpszMsg )
{
   FAXMAILOBJECT FAR * lpmo;
   FAXMAILOBJECT mo;

   lpmo = GetObjectFromLPOB( lpOldObj );
   if( NULL == lpmo )
      {
      Amob_New( OBTYPE_FAXMAILMESSAGE, &mo );
      lpmo = &mo;
      }
   Amob_CreateRefString( &lpmo->recTo, lpNewFaxMailObj->szTo );
   Amob_CreateRefString( &lpmo->recFrom, lpNewFaxMailObj->szFrom );
   Amob_CreateRefString( &lpmo->recSubject, lpNewFaxMailObj->szSubject );
   Amob_CreateRefString( &lpmo->recNumber, lpNewFaxMailObj->szNumber );
   Amob_CreateRefString( &lpmo->recText, lpszMsg );
   lpmo->wFaxFlags = lpNewFaxMailObj->wFaxFlags;
   return( (OLD_LPOB)Amob_Commit( lpOldObj, OBTYPE_FAXMAILMESSAGE, lpmo ) );
}

/* This function implements the PutObject API call for the OT_CLRMAIL
 * out-basket object type.
 */
OLD_LPOB PASCAL PutClrMailObject( OLD_LPOB lpOldObj, OLD_CLRMAILOBJECT FAR * lpNewMailObj, LPSTR lpszMsg )
{
   Amob_New( OBTYPE_CLEARCIXMAIL, NULL );
   return( (OLD_LPOB)Amob_Commit( lpOldObj, OBTYPE_CLEARCIXMAIL, NULL ) );
}

/* This function implements the PutObject API call for the OT_ADDPAR
 * out-basket object type.
 */
OLD_LPOB PASCAL PutAddParObject( OLD_LPOB lpOldObj, OLD_ADDPAROBJECT FAR * lpNewAddParObj, LPSTR lpszMsg )
{
   ADDPAROBJECT FAR * lpmo;
   ADDPAROBJECT mo;

   lpmo = GetObjectFromLPOB( lpOldObj );
   if( NULL == lpmo )
      {
      Amob_New( OBTYPE_ADDPAR, &mo );
      lpmo = &mo;
      }
   Amob_CreateRefString( &lpmo->recConfName, lpNewAddParObj->szConfName );
   Amob_CreateRefString( &lpmo->recUserName, lpNewAddParObj->szUserName );
   return( (OLD_LPOB)Amob_Commit( lpOldObj, OBTYPE_ADDPAR, lpmo ) );
}

/* This function implements the PutObject API call for the OT_RESUME
 * out-basket object type.
 */
OLD_LPOB PASCAL PutResumeObject( OLD_LPOB lpOldObj, OLD_RESUMEOBJECT FAR * lpNewResumeObj, LPSTR lpszMsg )
{
   RESUMEOBJECT FAR * lpro;
   RESUMEOBJECT ro;

   lpro = GetObjectFromLPOB( lpOldObj );
   if( NULL == lpro )
      {
      Amob_New( OBTYPE_RESUME, &ro );
      lpro = &ro;
      }
   Amob_CreateRefString( &lpro->recUserName, lpNewResumeObj->szCixName );
   Amob_CreateRefString( &lpro->recFileName, lpNewResumeObj->szFileName );
   return( (OLD_LPOB)Amob_Commit( lpOldObj, OBTYPE_RESUME, lpro ) );
}


/* This function implements the FindObject API call.
 */
OLD_LPOB EXPORT FAR PASCAL FindObject( OLD_OBHEADER FAR * lpObjHdr )
{
   OLD_LPOB lpob;

   lpob = NULL;
   switch( lpObjHdr->type )
      {
      case OT_INLINE: {
         INLINEOBJECT io;

         Amob_New( OBTYPE_INLINE, &io );
         Amob_CreateRefString( &io.recFileName, ((OLD_INLINEOBJECT FAR *)lpObjHdr)->szFileName );
         Amob_CreateRefString( &io.recDescription, ((OLD_INLINEOBJECT FAR *)lpObjHdr)->szDescription );
         lpob = (OLD_LPOB)Amob_Find( OBTYPE_INLINE, &io );
         Amob_Delete( OBTYPE_INLINE, &io );
         break;
         }

      case OT_INCLUDE: {
         INCLUDEOBJECT io;

         Amob_New( OBTYPE_INCLUDE, &io );
         Amob_CreateRefString( &io.recFileName, ((OLD_INCLUDEOBJECT FAR *)lpObjHdr)->szFileName );
         Amob_CreateRefString( &io.recDescription, ((OLD_INCLUDEOBJECT FAR *)lpObjHdr)->szDescription );
         lpob = (OLD_LPOB)Amob_Find( OBTYPE_INCLUDE, &io );
         Amob_Delete( OBTYPE_INCLUDE, &io );
         break;
         }
      }
   return( lpob );
}

/* This function implements the GetObjectText API call. Under Ameol, only
 * a certain set of out-basket items had associated text.
 */
LPSTR EXPORT FAR PASCAL GetObjectText( OLD_LPOB lpob )
{
   OBINFO obinfo;

   Amob_GetObInfo( lpob, &obinfo );
   switch( obinfo.obHdr.clsid )
      {
      case OBTYPE_FUL: {
         FULOBJECT FAR * lpful;

         lpful = (FULOBJECT FAR *)obinfo.lpObData;
         return( Amob_DerefObject( &lpful->recText ) );
         }

      case OBTYPE_ARTICLEMESSAGE: {
         ARTICLEOBJECT FAR * lpao;

         lpao = (ARTICLEOBJECT FAR *)obinfo.lpObData;
         return( Amob_DerefObject( &lpao->recText ) );
         }

      case OBTYPE_MAILMESSAGE: {
         MAILOBJECT FAR * lpmo;

         lpmo = (MAILOBJECT FAR *)obinfo.lpObData;
         return( Amob_DerefObject( &lpmo->recText ) );
         }

      case OBTYPE_SAYMESSAGE: {
         SAYOBJECT FAR * lpso;

         lpso = (SAYOBJECT FAR *)obinfo.lpObData;
         return( Amob_DerefObject( &lpso->recText ) );
         }

      case OBTYPE_COMMENTMESSAGE: {
         COMMENTOBJECT FAR * lpco;

         lpco = (COMMENTOBJECT FAR *)obinfo.lpObData;
         return( Amob_DerefObject( &lpco->recText ) );
         }

      case OBTYPE_FAXMAILMESSAGE: {
         FAXMAILOBJECT FAR * lpfmo;

         lpfmo = (FAXMAILOBJECT FAR *)obinfo.lpObData;
         return( Amob_DerefObject( &lpfmo->recText ) );
         }
      }
   return( NULL );
}

/* This function implements the RemoveObject API call.
 */
void EXPORT FAR PASCAL RemoveObject( OLD_LPOB lpob )
{
   Amob_RemoveObject( lpob, TRUE );
}

/* This function implements the GetOutBasketObjectType API call.
 */
int EXPORT FAR PASCAL GetOutBasketObjectType( OLD_LPOB lpob )
{
   OBINFO obinfo;

   Amob_GetObInfo( lpob, &obinfo );
   switch( obinfo.obHdr.clsid )
      {
      case OBTYPE_SAYMESSAGE:    return( OT_SAY );
      case OBTYPE_MAILMESSAGE:   return( OT_MAIL );
      case OBTYPE_COMMENTMESSAGE:   return( OT_COMMENT );
      case OBTYPE_FAXMAILMESSAGE:   return( OT_FAXMAIL );
      case OBTYPE_RESUME:        return( OT_RESUME );
      case OBTYPE_GETCIXLIST:    return( OT_CIXLIST );
      case OBTYPE_FILELIST:      return( OT_FILELIST );
      case OBTYPE_DOWNLOAD:      return( OT_FDL );
      case OBTYPE_JOINCONFERENCE:   return( OT_JOIN );
      case OBTYPE_CIXRESIGN:     return( OT_RESIGN );
      case OBTYPE_COPYMSG:    return( OT_COPYMSG );
      case OBTYPE_PUTRESUME:     return( OT_PUTRESUME );
      case OBTYPE_WITHDRAW:      return( OT_WITHDRAW );
      case OBTYPE_BINMAIL:    return( OT_BINMAIL );
      case OBTYPE_MAILUPLOAD:    return( OT_MAILUPLOAD );
      case OBTYPE_MAILDOWNLOAD:  return( OT_MAILDOWNLOAD );
      case OBTYPE_MAILERASE:     return( OT_MAILERASE );
      case OBTYPE_MAILLIST:      return( OT_MAILLIST );
      case OBTYPE_INCLUDE:    return( OT_INCLUDE );
      case OBTYPE_FUL:        return( OT_FUL );
      //return( OT_JOINUNET );
      case OBTYPE_USENETRESIGN:  return( OT_RESIGNUNET );
      case OBTYPE_INLINE:        return( OT_INLINE );
      case OBTYPE_GETMESSAGE:    return( OT_FILEMSG );
      case OBTYPE_MAILEXPORT:    return( OT_EXPORT );
      case OBTYPE_GETPARLIST:    return( OT_GETPARLIST );
      case OBTYPE_ADDPAR:        return( OT_ADDPAR );
      case OBTYPE_REMPAR:        return( OT_REMPAR );
      case OBTYPE_ARTICLEMESSAGE:   return( OT_UUNETSAY );
      case OBTYPE_EXMOD:         return( OT_EXMOD );
      case OBTYPE_COMOD:         return( OT_COMOD );
      case OBTYPE_GETNEWSGROUPS: return( OT_UUNETLIST );
      case OBTYPE_GETUSERLIST:   return( OT_USERLIST );
      case OBTYPE_MAILRENAME:    return( OT_MAILRENAME );
      case OBTYPE_PREINCLUDE:    return( OT_PREINCLUDE );
      case OBTYPE_CLEARCIXMAIL:  return( OT_CLRMAIL );
      case OBTYPE_FDIR:       return( OT_FDIR );
      }                    
   return( -1 );
}

/* This function implements the GetObjectCount API call.
 */
int EXPORT FAR PASCAL GetObjectCount( void )
{
   return( Amob_GetCount( 0 ) );
}

/* This function implements the FreeObjectTextBuffer API call.
 * Does nothing, as text buffers are dynamically deallocated.
 */
void EXPORT FAR PASCAL FreeObjectTextBuffer( LPSTR lpText )
{
}

/* This function implements the GetOutBasketObject API call.
 */
OLD_LPOB EXPORT FAR PASCAL GetOutBasketObject( OLD_LPOB lpob )
{
   return( (OLD_LPOB)Amob_GetOb( lpob ) );
}

/* This function implements the GetOutBasketObjectHeader API call.
 */
void EXPORT FAR PASCAL GetOutBasketObjectHeader( OLD_LPOB lpob, OLD_OBHEADER FAR * lpoobh )
{
   OBINFO obinfo;

   Amob_GetObInfo( lpob, &obinfo );
   lpoobh->wFlags = obinfo.obHdr.wFlags;
   lpoobh->wDate = obinfo.obHdr.wDate;
   lpoobh->wTime = obinfo.obHdr.wTime;
}

/* This function implements the GetOutBasketObjectData API call.
 */
void EXPORT FAR PASCAL GetOutBasketObjectData( OLD_LPOB lpob, OLD_OBHEADER FAR * lpoobh )
{
   OBINFO obinfo;

   Amob_GetObInfo( lpob, &obinfo );
   lpoobh->wFlags = obinfo.obHdr.wFlags;
   lpoobh->wDate = obinfo.obHdr.wDate;
   lpoobh->wTime = obinfo.obHdr.wTime;
   switch( obinfo.obHdr.clsid )
      {
      case OBTYPE_BINMAIL: {
         OLD_BINMAILOBJECT FAR * lpobo;
         BINMAILOBJECT FAR * lpbo;

         lpbo = (BINMAILOBJECT FAR *)obinfo.lpObData;
         lpobo = (OLD_BINMAILOBJECT FAR *)lpoobh;
         strcpy( lpobo->szFileName, DRF(lpbo->recFileName) );
         strcpy( lpobo->szUserName, DRF(lpbo->recUserName) );
         lpobo->dwSize = lpbo->dwSize;
         lpobo->wFlags = lpbo->wFlags;
         break;
         }

      case OBTYPE_FDIR: {
         OLD_FDIROBJECT FAR * lpofdo;
         FDIROBJECT FAR * lpfdo;

         lpfdo = (FDIROBJECT FAR *)obinfo.lpObData;
         lpofdo = (OLD_FDIROBJECT FAR *)lpoobh;
         strcpy( lpofdo->szConfName, DRF(lpfdo->recConfName) );
         strcpy( lpofdo->szTopicName, DRF(lpfdo->recTopicName) );
         break;
         }

      case OBTYPE_FILELIST: {
         OLD_FILELISTOBJECT FAR * lpoflo;
         FILELISTOBJECT FAR * lpflo;

         lpflo = (FILELISTOBJECT FAR *)obinfo.lpObData;
         lpoflo = (OLD_FILELISTOBJECT FAR *)lpoobh;
         strcpy( lpoflo->szConfName, DRF(lpflo->recConfName) );
         strcpy( lpoflo->szTopicName, DRF(lpflo->recTopicName) );
         break;
         }

      case OBTYPE_PUTRESUME:
      case OBTYPE_RESUME: {
         OLD_RESUMEOBJECT FAR * lporo;
         RESUMEOBJECT FAR * lpro;

         lpro = (RESUMEOBJECT FAR *)obinfo.lpObData;
         lporo = (OLD_RESUMEOBJECT FAR *)lpoobh;
         strcpy( lporo->szFileName, DRF(lpro->recFileName) );
         strcpy( lporo->szCixName, DRF(lpro->recUserName) );
         break;
         }

      case OBTYPE_INCLUDE: {
         OLD_INCLUDEOBJECT FAR * lpoio;
         INCLUDEOBJECT FAR * lpio;

         lpio = (INCLUDEOBJECT FAR *)obinfo.lpObData;
         lpoio = (OLD_INCLUDEOBJECT FAR *)lpoobh;
         strcpy( lpoio->szFileName, DRF(lpio->recFileName) );
         strcpy( lpoio->szDescription, DRF(lpio->recDescription) );
         break;
         }

      case OBTYPE_INLINE: {
         OLD_INLINEOBJECT FAR * lpoio;
         INLINEOBJECT FAR * lpio;

         lpio = (INLINEOBJECT FAR *)obinfo.lpObData;
         lpoio = (OLD_INLINEOBJECT FAR *)lpoobh;
         strcpy( lpoio->szFileName, DRF(lpio->recFileName) );
         strcpy( lpoio->szDescription, DRF(lpio->recDescription) );
         break;
         }

      case OBTYPE_JOINCONFERENCE: {
         OLD_JOINOBJECT FAR * lpojo;
         JOINOBJECT FAR * lpjo;

         lpjo = (JOINOBJECT FAR *)obinfo.lpObData;
         lpojo = (OLD_JOINOBJECT FAR *)lpoobh;
         strcpy( lpojo->szConfName, DRF(lpjo->recConfName) );
         lpojo->wRecent = lpjo->wRecent;
         break;
         }

      case OBTYPE_FUL: {
         OLD_FULOBJECT FAR * lpofuo;
         FULOBJECT FAR * lpfuo;

         lpfuo = (FULOBJECT FAR *)obinfo.lpObData;
         lpofuo = (OLD_FULOBJECT FAR *)lpoobh;
         strcpy( lpofuo->szConfName, DRF(lpfuo->recConfName) );
         strcpy( lpofuo->szTopicName, DRF(lpfuo->recTopicName) );
         strcpy( lpofuo->szFileName, DRF(lpfuo->recFileName) );
         break;
         }

      case OBTYPE_GETPARLIST: {
         OLD_GETPARLISTOBJECT FAR * lpoplo;
         GETPARLISTOBJECT FAR * lpplo;

         lpplo = (GETPARLISTOBJECT FAR *)obinfo.lpObData;
         lpoplo = (OLD_GETPARLISTOBJECT FAR *)lpoobh;
         strcpy( lpoplo->szConfName, DRF(lpplo->recConfName) );
         break;
         }

      case OBTYPE_ADDPAR: {
         OLD_ADDPAROBJECT FAR * lpoap;
         ADDPAROBJECT FAR * lpap;

         lpap = (ADDPAROBJECT FAR *)obinfo.lpObData;
         lpoap = (OLD_ADDPAROBJECT FAR *)lpoobh;
         strcpy( lpoap->szConfName, DRF(lpap->recConfName) );
         strcpy( lpoap->szUserName, DRF(lpap->recUserName) );
         break;
         }

      case OBTYPE_REMPAR: {
         OLD_REMPAROBJECT FAR * lporp;
         REMPAROBJECT FAR * lprp;

         lprp = (REMPAROBJECT FAR *)obinfo.lpObData;
         lporp = (OLD_REMPAROBJECT FAR *)lpoobh;
         strcpy( lporp->szConfName, DRF(lprp->recConfName) );
         strcpy( lporp->szUserName, DRF(lprp->recUserName) );
         break;
         }

      case OBTYPE_COMOD: {
         OLD_COMODOBJECT FAR * lporp;
         COMODOBJECT FAR * lprp;

         lprp = (COMODOBJECT FAR *)obinfo.lpObData;
         lporp = (OLD_COMODOBJECT FAR *)lpoobh;
         strcpy( lporp->szConfName, DRF(lprp->recConfName) );
         strcpy( lporp->szUserName, DRF(lprp->recUserName) );
         break;
         }

      case OBTYPE_EXMOD: {
         OLD_EXMODOBJECT FAR * lporp;
         EXMODOBJECT FAR * lprp;

         lprp = (EXMODOBJECT FAR *)obinfo.lpObData;
         lporp = (OLD_EXMODOBJECT FAR *)lpoobh;
         strcpy( lporp->szConfName, DRF(lprp->recConfName) );
         strcpy( lporp->szUserName, DRF(lprp->recUserName) );
         break;
         }

      case OBTYPE_CIXRESIGN: {
         OLD_RESIGNOBJECT FAR * lpocr;
         RESIGNOBJECT FAR * lpcr;

         lpcr = (RESIGNOBJECT FAR *)obinfo.lpObData;
         lpocr = (OLD_RESIGNOBJECT FAR *)lpoobh;
         strcpy( lpocr->szConfName, DRF(lpcr->recConfName) );
         break;
         }

      case OBTYPE_MAILUPLOAD: {
         OLD_MAILUPLOADOBJECT FAR * lpomu;
         MAILUPLOADOBJECT FAR * lpmu;

         lpmu = (MAILUPLOADOBJECT FAR *)obinfo.lpObData;
         lpomu = (OLD_MAILUPLOADOBJECT FAR *)lpoobh;
         strcpy( lpomu->szFileName, DRF(lpmu->recFileName) );
         break;
         }

      case OBTYPE_MAILERASE: {
         OLD_MAILERASEOBJECT FAR * lpome;
         MAILERASEOBJECT FAR * lpme;

         lpme = (MAILERASEOBJECT FAR *)obinfo.lpObData;
         lpome = (OLD_MAILERASEOBJECT FAR *)lpoobh;
         strcpy( lpome->szFileName, DRF(lpme->recFileName) );
         break;
         }

      case OBTYPE_MAILRENAME: {
         OLD_MAILRENAMEOBJECT FAR * lpomn;
         MAILRENAMEOBJECT FAR * lpmn;

         lpmn = (MAILRENAMEOBJECT FAR *)obinfo.lpObData;
         lpomn = (OLD_MAILRENAMEOBJECT FAR *)lpoobh;
         strcpy( lpomn->szOldName, DRF(lpmn->recOldName) );
         strcpy( lpomn->szNewName, DRF(lpmn->recNewName) );
         break;
         }

      case OBTYPE_MAILEXPORT: {
         OLD_EXPORTOBJECT FAR * lpomn;
         MAILEXPORTOBJECT FAR * lpmn;

         lpmn = (MAILEXPORTOBJECT FAR *)obinfo.lpObData;
         lpomn = (OLD_EXPORTOBJECT FAR *)lpoobh;
         strcpy( lpomn->szConfName, DRF(lpmn->recConfName) );
         strcpy( lpomn->szTopicName, DRF(lpmn->recTopicName) );
         strcpy( lpomn->szFileName, DRF(lpmn->recFileName) );
         lpomn->wFlags = lpmn->wFlags;
         break;
         }

      case OBTYPE_MAILDOWNLOAD: {
         OLD_MAILDOWNLOADOBJECT FAR * lpomd;
         MAILDOWNLOADOBJECT FAR * lpmd;

         lpmd = (MAILDOWNLOADOBJECT FAR *)obinfo.lpObData;
         lpomd = (OLD_MAILDOWNLOADOBJECT FAR *)lpoobh;
         strcpy( lpomd->szCIXFileName, DRF(lpmd->recCIXFileName) );
         strcpy( lpomd->szFileName, DRF(lpmd->recFileName) );
         strcpy( lpomd->szDirectory, DRF(lpmd->recDirectory) );
         lpomd->wFlags = lpmd->wFlags;
         break;
         }

      case OBTYPE_GETMESSAGE: {
         OLD_FILEMSGOBJECT FAR * lpogm;
         GETMESSAGEOBJECT FAR * lpgm;

         lpgm = (GETMESSAGEOBJECT FAR *)obinfo.lpObData;
         lpogm = (OLD_FILEMSGOBJECT FAR *)lpoobh;
         SplitTopic( DRF(lpgm->recTopicName), lpogm->szConfName, lpogm->szTopicName );
         lpogm->dwStart = lpgm->dwStartMsg;
         lpogm->dwEnd = lpgm->dwEndMsg;
         break;
         }

      case OBTYPE_DOWNLOAD: {
         OLD_FDLOBJECT FAR * lpofdl;
         FDLOBJECT FAR * lpfdl;

         lpfdl = (FDLOBJECT FAR *)obinfo.lpObData;
         lpofdl = (OLD_FDLOBJECT FAR *)lpoobh;
         strcpy( lpofdl->szTopicName, DRF(lpfdl->recTopicName) );
         strcpy( lpofdl->szConfName, DRF(lpfdl->recConfName) );
         strcpy( lpofdl->szFileName, DRF(lpfdl->recFileName) );
         strcpy( lpofdl->szCIXFileName, DRF(lpfdl->recCIXFileName) );
         strcpy( lpofdl->szDirectory, DRF(lpfdl->recDirectory) );
         lpofdl->wFlags = lpfdl->wFlags;
         break;
         }

      case OBTYPE_FAXMAILMESSAGE: {
         OLD_FAXOBJECT FAR * lpofmo;
         FAXMAILOBJECT FAR * lpfmo;

         lpfmo = (FAXMAILOBJECT FAR *)obinfo.lpObData;
         lpofmo = (OLD_FAXOBJECT FAR *)lpoobh;
         strcpy( lpofmo->szTo, DRF(lpfmo->recTo) );
         strcpy( lpofmo->szFrom, DRF(lpfmo->recFrom) );
         strcpy( lpofmo->szSubject, DRF(lpfmo->recSubject) );
         strcpy( lpofmo->szNumber, DRF(lpfmo->recNumber) );
         lpofmo->wFaxFlags = lpfmo->wFaxFlags;
         break;
         }

      case OBTYPE_MAILMESSAGE: {
         OLD_MAILOBJECT FAR * lpomo;
         MAILOBJECT FAR * lpmo;

         lpmo = (MAILOBJECT FAR *)obinfo.lpObData;
         lpomo = (OLD_MAILOBJECT FAR *)lpoobh;
         strcpy( lpomo->szTo, DRF(lpmo->recTo) );
         strcpy( lpomo->szCC, DRF(lpmo->recCC) );
         strcpy( lpomo->szSubject, DRF(lpmo->recSubject) );
         strcpy( lpomo->szReply, DRF(lpmo->recReply) );
         lpomo->wFlags = lpmo->wFlags;
         break;
         }

      case OBTYPE_ARTICLEMESSAGE: {
         OLD_UUNETSAYOBJECT FAR * lpoao;
         ARTICLEOBJECT FAR * lpao;

         lpao = (ARTICLEOBJECT FAR *)obinfo.lpObData;
         lpoao = (OLD_UUNETSAYOBJECT FAR *)lpoobh;
         strcpy( lpoao->szTo, DRF(lpao->recNewsgroup) );
         strcpy( lpoao->szSubject, DRF(lpao->recSubject) );
         strcpy( lpoao->szReply, DRF(lpao->recReply) );
         lpoao->wFlags = 0;
         break;
         }

      case OBTYPE_USENETRESIGN: {
         OLD_RESIGNUNETOBJECT FAR * lpoco;
         RESIGNUNETOBJECT FAR * lpco;

         lpco = (RESIGNUNETOBJECT FAR *)obinfo.lpObData;
         lpoco = (OLD_RESIGNUNETOBJECT FAR *)lpoobh;
         strcpy( lpoco->szGroupName, DRF(lpco->recGroupName) );
         break;
         }

      case OBTYPE_COMMENTMESSAGE: {
         OLD_COMMENTOBJECT FAR * lpoco;
         COMMENTOBJECT FAR * lpco;

         lpco = (COMMENTOBJECT FAR *)obinfo.lpObData;
         lpoco = (OLD_COMMENTOBJECT FAR *)lpoobh;
         strcpy( lpoco->szConfName, DRF(lpco->recConfName) );
         strcpy( lpoco->szTopicName, DRF(lpco->recTopicName) );
         lpoco->dwComment = lpco->dwReply;
         break;
         }

      case OBTYPE_COPYMSG: {
         OLD_COPYMSGOBJECT FAR * lpocpy;
         COPYMSGOBJECT FAR * lpcpy;

         lpcpy = (COPYMSGOBJECT FAR *)obinfo.lpObData;
         lpocpy = (OLD_COPYMSGOBJECT FAR *)lpoobh;
         strcpy( lpocpy->szConfName, DRF(lpcpy->recConfName) );
         strcpy( lpocpy->szTopicName, DRF(lpcpy->recTopicName) );
         strcpy( lpocpy->szCurConfName, DRF(lpcpy->recCurConfName) );
         strcpy( lpocpy->szCurTopicName, DRF(lpcpy->recCurTopicName) );
         lpocpy->dwMsg = lpcpy->dwMsg;
         break;
         }

      case OBTYPE_WITHDRAW: {
         OLD_WITHDRAWOBJECT FAR * lpowo;
         WITHDRAWOBJECT FAR * lpwo;

         lpwo = (WITHDRAWOBJECT FAR *)obinfo.lpObData;
         lpowo = (OLD_WITHDRAWOBJECT FAR *)lpoobh;
         SplitTopic( DRF(lpwo->recTopicName), lpowo->szConfName, lpowo->szTopicName );
         lpowo->dwMsg = lpwo->dwMsg;
         break;
         }

      case OBTYPE_SAYMESSAGE: {
         OLD_SAYOBJECT FAR * lposo;
         SAYOBJECT FAR * lpso;

         lpso = (SAYOBJECT FAR *)obinfo.lpObData;
         lposo = (OLD_SAYOBJECT FAR *)lpoobh;
         strcpy( lposo->szConfName, DRF(lpso->recConfName) );
         strcpy( lposo->szTopicName, DRF(lpso->recTopicName) );
         strcpy( lposo->szTitle, DRF(lpso->recSubject)  );
         break;
         }
      }
}

/* This function implements the GetObjectInfo API call.
 */
void EXPORT FAR PASCAL GetObjectInfo( OLD_LPOB lpob, OLD_OBHEADER FAR * lpoobh )
{
   GetOutBasketObjectData( lpob, lpoobh );
}

/* This function splits a combined conference/topic name into separate
 * data fields.
 */
void FASTCALL SplitTopic( LPSTR lpTopic, LPSTR lpConfName, LPSTR lpTopicName )
{
   while( *lpTopic && *lpTopic != '/' )
      *lpConfName++ = *lpTopic++;
   *lpConfName = '\0';
   if( *lpTopic == '/' )
      ++lpTopic;
   while( *lpTopic )
      *lpTopicName++ = *lpTopic++;
   *lpTopicName = '\0';
}
