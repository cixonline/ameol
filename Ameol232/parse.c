/* PARSE.C - CIX scratchpad parser
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

#include "main.h"
#include "palrc.h"
#include "resource.h"
#include <direct.h>
#include <commdlg.h>
#include <string.h>
#include <ctype.h>
#include <dos.h>
#include <io.h>
#include "amlib.h"
#include <sys\types.h>
#include <sys\stat.h>
#include "lbf.h"
#include "ameol2.h"
#include "rules.h"
#include "cix.h"
#include "amevent.h"
#include "cookie.h"
#include "news.h"

#define  THIS_FILE   __FILE__

#define  HDRTYP_VERBOSE       0
#define  HDRTYP_COMPACT       1
#define  HDRTYP_TERSE         2

/* State machine definitions
 */
#define  CIX_SM_START               0
#define  CIX_SM_CIXMSGHDR           1
#define  CIX_SM_TERSEHDR            2
#define  CIX_SM_TERSEDELIM          3
#define  CIX_SM_CIXMSGBODY          4
#define  CIX_SM_MAILHDR             5
#define  CIX_SM_MAILMSGHDR          6
#define  CIX_SM_VERBOSEHDR          7
#define  CIX_SM_RESUME              8
#define  CIX_SM_PARLIST             9
#define  CIX_SM_HELPLIST            10
#define  CIX_SM_HELP                11
#define  CIX_SM_FILELIST            12
#define  CIX_SM_SHOWCONF            13
#define  CIX_SM_SHOWCONFDATETIME    14
#define  CIX_SM_SHOWCONFTOPICLIST   15
#define  CIX_SM_FDIR                16
#define  CIX_SM_USENETHDRS          17
#define  CIX_SM_NEWSGROUPLIST       18
#define CIX_SM_SHOWCONFNOTE            19

#define  LEN_MSGLINE                2047

/* The number of tokens (separated by space) that appear in the first line of
** the flist data in the scratchpad. YH 04/04/95
*/
#define  NUM_TOKENS_IN_FLIST_HEAD   5

extern BOOL fIsAmeol2Scratchpad;

static BOOL fDefDlgEx = FALSE;      /* DefDlg recursion flag trap */
static PTL ptlPrev = NULL;
static WORD wUniqueID;

BOOL EXPORT CALLBACK ReadScratchpad( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL ReadScratchpad_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL ReadScratchpad_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL ReadScratchpad_DlgProc( HWND, UINT, WPARAM, LPARAM );

static char FAR szConfName[ LEN_CONFNAME + 1 ];       /* Name of conference to which message belongs */
static char FAR szTopicName[ LEN_TOPICNAME + 1 ];     /* Name of topic within the conference */
static char FAR szScratchFile[ _MAX_PATH ];           /* Scratchpad file name */
static char FAR szCurTopicFileName[ _MAX_PATH ];      /* Name of topic file for current flist being parsed (YH 04/04/96) */

char FAR szCompactHdrTmpl[] = ">>>%.14s/%.14s %lu %.14s(%lu)%d%s%2.2d %2.2d:%2.2d";

char szScratchpads[] = "Scratchpads";
char szCixHelpListSrc[] = "cixhelp.dat";        /* Download file for CIX help */

static HEADER FAR cixhdr;                       /* CIX message header */
static DWORD cNew;                              /* Count of new messages */
static DWORD cbMsg;                             /* Length of message (not really used) */
static DWORD cbsz;                              /* Cumulative length of message */
static DWORD cbTotal;                           /* True length of message */
static HPSTR hpBuf;                             /* Pointer to the actual message */
static WORD wFlags;                             /* Topic flags */
static BOOL fCheckForXOLRField;                 /* TRUE if we have to check for X-Olr */
static BOOL fMarkMsgUnread;                     /* TRUE if message is marked unread after importing */
static BOOL fMarkMsgMarked;                     /* TRUE if message is marked after importing */
static BOOL fMarkMsgLocked;                     /* TRUE if message is marked readlock after importing */
static BOOL fMarkMsgKeep;                       /* TRUE if message is marked kept after importing */
static BOOL fMarkMsgPriority;                   /* TRUE if message is priority after importing */
static BOOL fMarkMsgIgnore;                     /* TRUE if message is marked ignore after importing */
static BOOL fMarkMsgDelete;                     /* TRUE if message is marked delete after importing */
static BOOL fMarkMsgTagged;                     /* TRUE if message is marked tagged after importing */
static BOOL fMarkMsgWithdraw;                   /* TRUE if message is marked withdrawn after importing */
static BOOL fMarkMsgWatched;                    /* TRUE if message is marked watched after importing */
static BOOL fHasJoining;                        /* TRUE if we saw a 'Joining' message */
static BOOL fPreBody;                           /* TRUE if we're passing text between mail header and body */
static HNDFILE fh;                              /* Handle of file being parsed */
static LPLBF hBuffer;                           /* Handle of line buffer */
static LPSTR lpLineBuf;                         /* Pointer to line buffer */
static WORD wState;                             /* Current parse state */
static int nType;                               /* Type of CIX header being parsed */
static BOOL fCheckMaxMsgSize;                   /* TRUE if we must check maximum message size */
static BOOL fGetTitleFromMsgBody;               /* TRUE if message has a title */
static DWORD dwSize;                            /* Input scratchpad size */
static WORD wOldCount;                          /* Last parse percentage count */
static BOOL fPrependCompactHdr;                 /* TRUE if we prepend CIX header to message */
static BOOL fArticleSeen;                       /* TRUE if we saw the Article header field */
static WORD cUULines;                           /* Number of lines in Usenet message */
static WORD nUULine;                            /* Current line number */
static BOOL fHasBody;                           /* TRUE if the news article has a body */
static BOOL fMail;                              /* TRUE if we're parsing mail. */
static BOOL fUsenet;                            /* TRUE if we're parsing usenet. */
static int cbBlankLine;                         /* Size of blank line after message header */
static WORD wGlbParseFlags;                     /* Global parse flag */
static BOOL fTaggedStart;                       /* TRUE if we're handling tagged messages */
static BOOL fFirstNoteLine = TRUE;

#define  ArchiveAfterImporting()       (wGlbParseFlags & PARSEFLG_ARCHIVE)
#define  CountCR()                     (wGlbParseFlags & PARSEFLG_IGNORECR)
#define  MarkAllRead()                 (wGlbParseFlags & PARSEFLG_MARKREAD)
#define  Importing()                   (wGlbParseFlags & PARSEFLG_IMPORT)
#define  DontUseRules()                (wGlbParseFlags & PARSEFLG_NORULES)
#define  DontDeleteAfterImport()       (wGlbParseFlags & PARSEFLG_KEEPAFTERIMPORT)

static int wError;
static DWORD cbMaxMsgSize;
static BOOL fNoAddTopics;
static WORD wParsing;
static UINT cbLine;
static PCL pclShowConf;

static BOOL fBuilding = FALSE;                  /* TRUE if we're doing a build */

/* The following are language independent strings that can appear in a CIX
 * scratchpad.
 */
static char FAR scp_str1[] = "Checking for conference activity";
static char FAR scp_str2[] = "No unread messages";
static char FAR scp_str3[] = "There are comments to this message.";
static char FAR scp_str4[] = "There are more comments to ";
static char FAR scp_str5[] = "This is a comment to message ";
static char FAR scp_str6[] = "#! rnews ";
static char FAR scp_str7[] = " of ";
static char FAR scp_str8[] = "Moderator (s) \t";
static char FAR scp_str9[] = "Created:       ";
static char FAR scp_str10[] = "READ ONLY";
static char FAR scp_str11[] = "Not a member of";
static char FAR scp_str12[] = "Comment to ";
static char FAR scp_str13[] = "TOPIC FULL";
static char FAR scp_str14[] = "==========================";
static char FAR scp_str15[] = "==========";
static char FAR scp_str16[] = "Memo #";
static char FAR scp_str17[] = "Joining ";
static char FAR scp_str18[] = ">>>";
static char FAR scp_str19[] = "This topic is READ ONLY";
static char FAR scp_str20[] = "----------";
static char FAR scp_str21[] = "Comments.";
static char FAR scp_str22[] = "Date";
static char FAR scp_str23[] = "From";
static char FAR scp_str24[] = "X-Reply-Folder";
static char FAR scp_str25[] = "Message-Id";
static char FAR scp_str26[] = "In-Reply-To";
static char FAR scp_str27[] = "Subject";
static char FAR scp_str28[] = "Lines";
static char FAR scp_str29[] = "Article";
static char FAR scp_str30[] = "References";
static char FAR scp_str31[] = "Comment to ";
static char FAR scp_str32[] = "--------------------------";
static char FAR scp_str33[] = "X-OLR";
static char FAR scp_str35[] = "memo.";
static char FAR scp_str36[] = "Xref";
static char FAR scp_str37[] = "Newsgroups";
static char FAR scp_str38[] = "X-Priority";
static char FAR scp_str39[] = "!S4: ";
static char FAR scp_str40[] = "To";
static char FAR scp_str41[] = "Sender";
static char FAR scp_str42[] = "@!__hdr__!@";
static char FAR scp_str43[] = "CC";
static char FAR scp_str44[] = "@!__tagged__!@";
static char FAR scp_str45[] = "!MF:";
static char FAR scp_str46[] = "Mime-Version";
static char FAR scp_str47[] = "Content-Transfer-Encoding";
static char FAR scp_str48[] = "Disposition-Notification-To";
static char FAR scp_str49[] = "No details for conference";
static char FAR scp_str50[] = "(0)";
static char FAR scp_str51[] = "\x01Memo #";
static char FAR scp_str52[] = "X-Confirm-Reading-To";
static char FAR scp_str53[] = "Priority";

/* The following are used by Ameol to delimit blocks of text in the scratchpad.
 */
char FAR szUniqueIDEcho[] = "!*_ID_";
char FAR szResumeStartEcho[] = "!*_ResumeStart_";
char FAR szResumeEndEcho[] = "!*_ResumeEnd_";
char FAR szParListStartEcho[] = "!*_ParListStart_";
char FAR szParListEndEcho[] = "!*_ParListEnd_";
char FAR szHelpListStartEcho[] = "!*_HelpListStart_";
char FAR szHelpListEndEcho[] = "!*_HelpListEnd_";
char FAR szHelpStartEcho[] = "!*_HelpStart_";
char FAR szHelpEndEcho[] = "!*_HelpEnd_";
char FAR szFileListStartEcho[] = "!*_FileListStart_";
char FAR szFileListEndEcho[] = "!*_FileListEnd_";
char FAR szShowConfStartEcho[] = "!*_ShowConfStart_";
char FAR szShowConfEndEcho[] = "!*_ShowConfEnd_";
char FAR szNewsgroupStartEcho[] = "!*_NewsgroupListStart_";
char FAR szNewsgroupEndEcho[] = "!*_NewsgroupListEnd_";
char FAR szFdirStartEcho[] = "!*_FileDirStart_";
char FAR szFdirEndEcho[] = "!*_FileDirEnd_";
static char FAR szDefaultAuthor[] = "[No-author]\0";

static void FASTCALL AppendToMessage( char *, BOOL );
static void FASTCALL AppendLineToMessage( char *, BOOL );
static BOOL FASTCALL PutCurrentMessage( void );
static int FASTCALL PutMessage( void );

char * FASTCALL ParseConfTopic( char * );
char * FASTCALL ParseCIXMessageNumber( char * );
char * FASTCALL ParseCIXAuthor( char * );
char * FASTCALL ParseCIXMessageLength( char * );
char * FASTCALL ParseCIXDateTime( char * );
char * FASTCALL ParseExactCIXDateTime( char * );
char * FASTCALL ParseCIXMailAuthor( char *, BOOL );
BOOL FASTCALL ParseCompactHeader( char * );
BOOL FASTCALL CannotCreateFileError( HWND );
char * FASTCALL ParseConferenceTitle( char * );
char * FASTCALL ParseCIXFromField( char *, char *, char *, BOOL );
BOOL FASTCALL CheckForXOLRField( char * );
char * FASTCALL ParseCixMailToField( char * );
char * FASTCALL ParseCixMailCCField( char * );
char * FASTCALL ParseMailNewsgroup( char * );
char * FASTCALL ParseMailArticle( char * );
char * FASTCALL ParseReplyFolder( char * );
char * FASTCALL ParseCIXUsenetHeader( char * );

void FASTCALL CreateProgressGauge( LPSTR );
void FASTCALL PrependCompactHeader( void );
void FASTCALL UnarcScratchpad( HWND, char *, char * );
void FASTCALL CIXImportParse( void );
void FASTCALL EndOfCixnewsTagged( void );
void FASTCALL InitialiseCixHdr( void );
void FASTCALL ParseCixnewsAnnouncement( char * );
void FASTCALL CheckForPreBody( void );
BOOL FASTCALL FileListHeaderOK(LPCSTR lpszFileListHeader);

void FASTCALL ToolbarState_Import( void );

void FASTCALL ReportOnline( char * );

typedef struct tagIMPORTQUEUEITEM {
   struct tagIMPORTQUEUEITEM FAR * pimpqNext;
   char * lpszScratchFile;
   WORD wFlags;
} IMPORTQUEUEITEM, FAR * LPIMPORTQUEUEITEM;

LPIMPORTQUEUEITEM pimpqFirst = NULL;      /* Pointer to first item in import queue */
LPIMPORTQUEUEITEM pimpqLast = NULL;       /* Pointer to last item in import queue */

BOOL FASTCALL QueueScratchpad( LPSTR, WORD );

/* This function inspects the SCRATCH directory and
 * returns the number of files found.
 */
int FASTCALL CountUnparsedScratchpads( void )
{
   int cFilesFound;
   register int n;
   FINDDATA ft;
   HFIND r;

   cFilesFound = 0;
   BEGIN_PATH_BUF;
   wsprintf( lpPathBuf, "%s\\*.*", pszScratchDir );
   for( n = r = Amuser_FindFirst( lpPathBuf, _A_NORMAL, &ft ); n != -1; n = Amuser_FindNext( r, &ft ) )
      if( strcmp( ft.name, "." ) && strcmp( ft.name, ".." ) && !(ft.attrib & _A_SUBDIR) )
         {
         LPIMPORTQUEUEITEM pimpq;

         /* See if this is already being parsed. If so, don't
          * count it.
          */
         wsprintf( lpPathBuf, "%s\\%s", pszScratchDir, ft.name );
         for( pimpq = pimpqFirst; pimpq; pimpq = pimpq->pimpqNext )
            if( _strcmpi( pimpq->lpszScratchFile, lpPathBuf ) == 0 )
               break;
         if( NULL == pimpq )
            ++cFilesFound;
         }
   Amuser_FindClose( r );
   END_PATH_BUF;
   return( cFilesFound );
}

/* This function inspects the SCRATCH directory and parses all
 * files found therein.
 */
void FASTCALL HandleUnparsedScratchpads( HWND hwnd, BOOL fPrompt )
{
   /* Don't attempt to recover scratchpad if last connect failed!
    */
   if( !MustRecoverScratchpad() )
      if( CountUnparsedScratchpads() )
         {
         char sz[ _MAX_PATH ];

         /* Ask first...
          */
         if( fPrompt )
            if( IDNO == fMessageBox( hwnd, 0, GS(IDS_STR501), MB_YESNO|MB_ICONINFORMATION ) )
               return;

         /* Okay, enqueue all scratchpads therein.
          */
         wsprintf( sz, "%s\\*.*", pszScratchDir );
         CIXImportAndArchive( hwnd, sz, PARSEFLG_ARCHIVE|PARSEFLG_REPLACECR );
         }
}

/* This function queues the specified scratchpad for importing as 
 * soon as the current scratchpad has been done.
 */
BOOL FASTCALL QueueScratchpad( LPSTR lpszScratchFile, WORD wFlags )
{
   IMPORTQUEUEITEM FAR * pimpqNew;

   /* Add this entry to the queue.
    */
   INITIALISE_PTR(pimpqNew);
   if( fNewMemory( &pimpqNew, sizeof(IMPORTQUEUEITEM) ) )
      {
      /* Save the scratchpad name.
       */
      INITIALISE_PTR(pimpqNew->lpszScratchFile);
      if( fNewMemory( &pimpqNew->lpszScratchFile, strlen(lpszScratchFile) + 1 ) )
         {
         /* Save scratchpad filename.
          */
         strcpy( pimpqNew->lpszScratchFile, lpszScratchFile );

         /* Link this new entry to the queue.
          */
         if( NULL == pimpqFirst )
            pimpqFirst = pimpqNew;
         else
            pimpqLast->pimpqNext = pimpqNew;
         pimpqNew->wFlags = wFlags;
         pimpqNew->pimpqNext = NULL;
         pimpqLast = pimpqNew;
         return( TRUE );
         }
      }
   OutOfMemoryError( hwndFrame, FALSE, FALSE );
   return( FALSE );
}

/* This function pulls the next scratchpad out of the import
 * queue and processes it.
 */
BOOL FASTCALL DequeueScratchpad( void )
{
   if( pimpqFirst )
      {
      IMPORTQUEUEITEM FAR * pimpqThis;

      /* Import this entry and delete it from the
       * queue.
       */
      CIXImportAndArchive( hwndFrame, pimpqFirst->lpszScratchFile, pimpqFirst->wFlags );
      pimpqThis = pimpqFirst;
      pimpqFirst = pimpqFirst->pimpqNext;
      FreeMemory( &pimpqThis );
      return( TRUE );
      }
   return( FALSE );
}

/* This function empties the entire scratchpad
 * queue.
 */
void FASTCALL ClearScratchpadQueue( void )
{
   IMPORTQUEUEITEM FAR * pimpqNext;
   IMPORTQUEUEITEM FAR * pimpq;

   for( pimpq = pimpqFirst; pimpq; pimpq = pimpqNext )
      {
      pimpqNext = pimpq->pimpqNext;
      FreeMemory( &pimpq );
      }
   pimpqFirst = NULL;
}

/* This function parses a CIX scratchpad.
 */
BOOL EXPORT WINAPI CIXImport( HWND hwnd, LPSTR lpszScratchFile )
{
   return( CIXImportAndArchive( hwnd, lpszScratchFile, PARSEFLG_REPLACECR ) );
}

/* This function parses an Ameol scratchpad.
 */
BOOL EXPORT WINAPI AmeolImport( HWND hwnd, LPSTR lpszScratchFile )
{
   return( CIXImportAndArchive( hwnd, lpszScratchFile, PARSEFLG_IGNORECR ) );
}

/* This function rebuilds the specified topic by re-importing
 * the TXT file for that topic.
 */
BOOL FASTCALL BuildImport( HWND hwnd, PTL ptl )
{
   char szOldPath[ _MAX_PATH ];
   char szNewPath[ _MAX_PATH ];
   TOPICINFO topicinfo;
   BOOL fOk;

   /* Assume we're not done.
    */
   fOk = FALSE;

   /* Broadcast a message to close all windows on this
    * topic.
    */
   fTopicClosed = FALSE;
   fBuilding = TRUE;
   SendAllWindows( WM_CLOSEFOLDER, 0, (LPARAM)ptl );
   if( fTopicClosed )
      OnlineStatusText( GS(IDS_STR1108) );

   /* Close all message windows which are open on this topic
    * and discard it.
    */
   Amdb_DiscardTopic( ptl );

   /* Get the topic base filename and rename
    * it to <filename>.BLD
    */
   Amdb_GetTopicInfo( ptl, &topicinfo );
   wsprintf( szOldPath, "%s\\%s.TXT", pszDataDir, topicinfo.szFileName );
   wsprintf( szNewPath, "%s\\%s.BLD", pszDataDir, topicinfo.szFileName );
   if( Amfile_Rename( szOldPath, szNewPath ) )
      {
      /* Renamed okay, so delete the THD file.
       */
      if( !Amdb_EmptyTopic( ptl ) )
         {
         /* Failed to delete THD file! Serious error, so put
          * the TXT file back and exit now.
          */
         wsprintf( szOldPath, "%s\\%s.BLD", pszDataDir, topicinfo.szFileName );
         wsprintf( szNewPath, "%s\\%s.TXT", pszDataDir, topicinfo.szFileName );
         Amfile_Rename( szOldPath, szNewPath );
         }
      else
         {
         /* Otherwise import the BLD file. We sit in a loop until the
          * parser completes in order to avoid running it in the
          * background. Might be better to have a no-async flag input
          * to the parser though.
          */
         if( NULL != hwndInBasket )
            SendMessage( hwndInBasket, WM_LOCKUPDATES, WIN_INBASK, (LPARAM)FALSE );
         cixhdr.ptl = ptl;
         CIXImportAndArchive( hwnd, szNewPath, PARSEFLG_IMPORT|PARSEFLG_MARKREAD|PARSEFLG_NORULES );
         while( fParsing )
            {
            MSG msg;

            fpParser();
            if( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
               if( msg.message == WM_QUIT )
                  {
                  PostQuitMessage( msg.wParam );
                  break;
                  }
               else
                  {
                  TranslateMessage( &msg );
                  DispatchMessage( &msg );
                  }
            }
         if( NULL != hwndInBasket )
            SendMessage( hwndInBasket, WM_LOCKUPDATES, WIN_INBASK, (LPARAM)TRUE );
         fBuilding = FALSE;
         fOk = TRUE;
         }
      }
   return( fOk );
}

/* This function parses a scratchpad with optional
 * archiving.
 */
BOOL FASTCALL CIXImportAndArchive( HWND hwnd, LPSTR lpszScratchFile, WORD wParseFlags )
{
   char szFile[ _MAX_PATH ];

   /* Save the options. These can be overriden by the
    * ReadScratchpad dialog.
    */
   wGlbParseFlags = wParseFlags;

   /* If no scratchpad name passed as a parameter, put up the dialog which prompts
    * the user for the scratchpad name.
    */
   if( lpszScratchFile )
      strcpy( szFile, lpszScratchFile );
   else
      {
      Amuser_GetPPString( szSettings, "Scratchpad", "", szFile, sizeof( szFile ) );
      if( !Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_READSCRATCHPAD), ReadScratchpad, (LPARAM)(LPSTR)szFile ) )
         return( FALSE );
      Amuser_WritePPString( szSettings, "Scratchpad", szFile );
      }

   /* If the filename contains wildcards, extract each individual
    * filename and queue all except the first.
    */
   if( IsWild( szFile ) )
      {
      register int n;
      FINDDATA ft;
      char * pExt;      
      HFIND r;

      BEGIN_PATH_BUF;
      strcpy( lpPathBuf, szFile );
      pExt = GetFileBasename( lpPathBuf );
      for( n = r = Amuser_FindFirst( szFile, _A_NORMAL, &ft ); n != -1; n = Amuser_FindNext( r, &ft ) )
         {
         strcpy( pExt, ft.name );
         if( strcmp( ft.name, "." ) && strcmp( ft.name, ".." ) && !(ft.attrib & _A_SUBDIR) )
            QueueScratchpad( lpPathBuf, wGlbParseFlags );
         }
      Amuser_FindClose( r );
      END_PATH_BUF;
      return( DequeueScratchpad() );
      }

   /* If already parsing, add this scratchpad to the
    * import queue.
    */
   if( fParsing || (!fParseDuringBlink && fBlinking) )
      return( QueueScratchpad( szFile, wGlbParseFlags ) );

   /* Set the name of the scratchpad we're currently
    * parsing.
    */
   strcpy( szScratchFile, szFile );

   /* Allocate a line buffer
    */
   if( !fNewMemory( &lpLineBuf, LEN_MSGLINE + 1 ) )
      return( FALSE );

   /* Uncompress the file.
    */
   UnarcFile( hwnd, szScratchFile );

   /* Initialise some globals.
    */
   ptlPrev = NULL;
   cNew = 0;

   /* Switch off the sweep timer, otherwise performance will be very seriously
    * degraded!
    */
   KillSweepTimer();

   /* Try and open the scratchpad, and keep trying...
    */
   do {
      if( ( fh = Amfile_Open( szScratchFile, AOF_READ ) ) == HNDFILE_ERROR )
         {
         char sz[ _MAX_DIR ];
         int nRet;

         wsprintf( sz, GS(IDS_STR553), (LPSTR)szScratchFile );
         nRet = fMessageBox( hwnd, 0, sz, MB_RETRYCANCEL|MB_ICONEXCLAMATION );
         if( nRet == IDCANCEL )
            {
            if( !Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_READSCRATCHPAD), ReadScratchpad, (LPARAM)(LPSTR)szScratchFile ) )
               {
               FreeMemory( &lpLineBuf );
               return( FALSE );
               }
            UnarcFile( hwnd, szScratchFile );
            Amuser_WritePPString( szSettings, "Scratchpad", szScratchFile );
            }
         }
   } while( fh == HNDFILE_ERROR );

   /* Initialise the line buffer.
    */
   hBuffer = Amlbf_Open( fh, AOF_READ );
   cbMaxMsgSize = MAX_MSGSIZE;
   fNewMemory32( &hpBuf, cbMaxMsgSize );
   if( !hBuffer || !hpBuf )
      {
      OutOfMemoryError( hwndFrame, FALSE, FALSE );
      FreeMemory( &lpLineBuf );
      return( FALSE );
      }

   /* Initialise variables
    */
   wState = CIX_SM_START;
   wFlags = 0;
   cbsz = 0;
   cUULines = 0;
   fGetTitleFromMsgBody = FALSE;
   fArticleSeen = FALSE;
   nType = HDRTYP_COMPACT;
   wError = AMDBERR_NOERROR;
   fHasJoining = FALSE;
   fCheckMaxMsgSize = FALSE;
   fMarkMsgUnread = !MarkAllRead();
   fMarkMsgMarked = FALSE;
   fMarkMsgLocked = FALSE;
   fMarkMsgKeep = FALSE;
   fMarkMsgDelete = FALSE;
   fMarkMsgTagged = FALSE;
   fMarkMsgWithdraw = FALSE;
   fMarkMsgWatched = FALSE;
   fMarkMsgPriority = FALSE;
   fMarkMsgIgnore = FALSE;
   fTaggedStart = FALSE;
   wOldCount = 0;
   InitialiseCixHdr();
   dwSize = Amlbf_GetSize( hBuffer );

// StripBinaryCharacter( (char *)hBuffer, dwSize, '\0' );

   /* Create a gauge window on the status bar.
    */
   if( !fBuilding )
      {
      wsprintf( lpTmpBuf, GS(IDS_STR869), GetFileBasename(szScratchFile) );
      CreateProgressGauge( lpTmpBuf );
      }

   /* Update frame window first.
    */
   UpdateWindow( hwndFrame );

   /* Set the parse state machine.
    */
   fParsing = TRUE;
   ToolbarState_Import();
   fpParser = CIXImportParse;
   return( TRUE );
}

/* This function creates a progress gauge on the status bar.
 * The specified text appears first, followed by the gauge:
 */
void FASTCALL CreateProgressGauge( LPSTR lpszText )
{
   if( hwndStatus && IsWindowVisible( hwndStatus ) )
      SendMessage( hwndStatus, SB_BEGINSTATUSGAUGE, 0, (LPARAM)(LPSTR)lpszText );
}

/* This function updates the status bar during parsing. It is also
 * called when the main window is restored or resized.
 */
void FASTCALL UpdateParsingStatusBar( void )
{
   wsprintf( lpTmpBuf, GS(IDS_STR869), GetFileBasename(szScratchFile) );
   SendMessage( hwndStatus, SB_STEPSTATUSGAUGE, wOldCount, (LPARAM)(LPSTR)lpTmpBuf );
}

/* Clean up at the end of the parse.
 */
void FASTCALL CIXImportParseCompleted( void )
{
   /* Finished! Tidy up at the end.
    */
   if( wError != AMDBERR_USERABORT && cbsz )
      if( wState == CIX_SM_CIXMSGBODY || wState == CIX_SM_START )
         PutCurrentMessage();
   fParsing = FALSE;
   fpParser = NULL;
   ToolbarState_Import();
   Amlbf_Close( hBuffer );
   FreeMemory32( &hpBuf );
   FreeMemory( &lpLineBuf );

   /* Delete the gauge window.
    */
   if( !fBuilding )
      SendMessage( hwndStatus, SB_ENDSTATUSGAUGE, 0, 0L );

   /* If needed, update last topic parsed.
    */
   if( ptlPrev )
      {
      SendAllWindows( WM_LOCKUPDATES, WIN_THREAD, (LPARAM)FALSE );
      Amuser_CallRegistered( AE_UNREADCHANGED, (LPARAM)ptlPrev, 0L );
      Amdb_DiscardTopic( ptlPrev );
      }
   else
      SendMessage( hwndFrame, WM_UPDATE_UNREAD, 0, 0L );


   /* Turn the sweep timer back on.
    */
   SetSweepTimer();

   /* If we're not in batch mode, try and save the database file now.
    */
   if( wError == AMDBERR_NOERROR )
      while( Amdb_CommitDatabase( FALSE ) != AMDBERR_NOERROR )
         {
         wError = AMDBERR_OUTOFDISKSPACE;
         if( !DiskSaveError( hwndFrame, pszDataDir, TRUE, FALSE ) )
            break;
         wError = AMDBERR_NOERROR;
         }

   /* Report back to the user.
    */
   if( wError != AMDBERR_NOERROR )
      switch( wError )
         {
         case AMDBERR_USERABORT:
            fMessageBox( hwndFrame, 0, GS(IDS_STR185), MB_OK|MB_ICONEXCLAMATION );
            break;
   
         case AMDBERR_BADSCRATCHPAD:
            fMessageBox( hwndFrame, 0, GS(IDS_STR184), MB_OK|MB_ICONEXCLAMATION );
            break;

         default: {
            char szTmp[200];

            wsprintf(szTmp, GS(IDS_STR183), wError);
            fMessageBox( hwndFrame, 0, szTmp, MB_OK|MB_ICONEXCLAMATION );
            break;
            }
         }

   /* Archive this scratchpad if needed.
    */
   if( ArchiveAfterImporting() )
      Ameol2_Archive( szScratchFile, TRUE );

   /* If importing, delete the scratchpad.
    */
   if( Importing() && !DontDeleteAfterImport() )
      Amfile_Delete( szScratchFile );

   /* Spread the word, we've parsed a scratchpad
    */
   Amuser_CallRegistered( AE_PARSED_ONE, (LPARAM)(LPSTR)szScratchFile, 0 );
   

   /* Pull any other scratchpad out of the queue. If none, display
    * the first unread message.
    */
   if( !DequeueScratchpad() )
      {
      Amuser_CallRegistered( AE_PARSE_COMPLETE, 0, 0 );
      if( fNoisy )
         Am_PlaySound( "Parse Complete" );
      if( fQuitAfterConnect && !fNoExit )
         PostMessage( hwndFrame, WM_CLOSE, 0, 0L );
      else if( fForceNextUnread || ( fUnreadAfterBlink && !Importing() ) )
         if( !( fCompatFlag & COMF_NOTNEXTUNREAD  && !( NULL == hwndTopic ) ) )
         {
         CURMSG unr;

         unr.pcat = NULL;
         unr.pcl = NULL;
         unr.ptl = NULL;
         unr.pth = NULL;
         if( Amdb_GetNextUnReadMsg( &unr, VM_VIEWREF ) )
            {
            OpenMsgViewerWindow( unr.ptl, unr.pth, FALSE );
            Amdb_InternalUnlockTopic( unr.ptl );
            }
         fForceNextUnread = FALSE;
         }
      }
   if( !fQuitting && fNewMail && !fNewMailPlayed )
   {
      if( fNoisy )
         Am_PlaySound( "New Mail" );
      if( fNewMailAlert )
      {
         fAlertTimedOut = FALSE;
         if( !IsIconic( hwndFrame ) )
         {
            SetTimer( hwndFrame, TID_MAILALERTTO, 120000, NULL );
            SetTimer( hwndFrame, TID_MAILALERT, 500, NULL );
            fMessageBox( hwndFrame, 0, GS(IDS_STR1254), MB_OK|MB_ICONINFORMATION );
         }
         else
         {
            StatusText( GS(IDS_STR1254) );
            SetTimer( hwndFrame, TID_MAILALERTTO, 3000, NULL );
            SetTimer( hwndFrame, TID_MAILALERT, 250, NULL );
         }
         if( !IsIconic( hwndFrame ) && !fAlertTimedOut )
         {
            KillTimer( hwndFrame, TID_MAILALERTTO );
            KillTimer( hwndFrame, TID_MAILALERT );
            FlashWindow( hwndFrame, 0 );
         }
      }
      fNewMail = FALSE;
      fNewMailPlayed = TRUE;
   }
}

/* This function performs one iteration of the parse state machine.
 */
void FASTCALL CIXImportParse( void )
{
   BOOL fContinuedLine;
   DWORD dwAccum;
   WORD wCount;

   /* Handle the end of the parse, which occurs
    * if the user aborts OR the end of the scratchpad
    * is detected.
    */
   if( wError == AMDBERR_USERABORT || !Amlbf_Read( hBuffer, lpLineBuf, LEN_MSGLINE, &cbLine, &fContinuedLine, &fIsAmeol2Scratchpad ) ) // FS#83 2.56.2051  
      {
      if( LBF_READ_ERROR == Amlbf_GetLastError( hBuffer ) )
         DiskReadError( GetFocus(), szScratchFile, FALSE, FALSE );
      CIXImportParseCompleted();
      return;
      }

   /* Update progress gauge.
    * BUGBUG: Overflow occurs if dwAccum is larger than 0x28F5C28
    * (42949672). Need to fix this approach to computing percents
    * without using floats or fractions.
    */
   if( !fBuilding )
      {
      dwAccum = Amlbf_GetCount( hBuffer );
      wCount = (WORD)( ( dwAccum * 100 ) / dwSize );
      if( wCount != wOldCount )
         {
         wOldCount = wCount;
         UpdateParsingStatusBar();
         }
      }

   /* Still parsing, so invoke the state
    * machine.
    */
   switch( wState )
      {
      case CIX_SM_START:
         if( ( strncmp( lpLineBuf, scp_str16, strlen(scp_str16) ) == 0 ) || ( strncmp( lpLineBuf, scp_str51, strlen(scp_str51) ) == 0 ) ) {
            char * psz;

PPMail:        if( strstr( lpLineBuf, scp_str50 ) != NULL )
               break;

            if( !PutCurrentMessage() )
               break;
            cbMsg = cbsz = cbTotal = 0;
            AppendLineToMessage( lpLineBuf, TRUE );
            psz = ParseCIXMessageNumber( lpLineBuf + ( *lpLineBuf == '\x01' ? 6 : 5 ) );
            if( *psz == '(' ) ++psz;
            while( isdigit( *psz ) )
               cbMsg = cbMsg * 10 + ( *psz++ - '0' );
            if( cbMsg == 0 )
               break;
            strcpy( szConfName, "Mail" );
            strcpy( szTopicName, szPostmaster );
            if( !fBuilding )
               cixhdr.ptl = GetPostmasterFolder();
            cixhdr.dwMsg = (DWORD)-1;
            cixhdr.dwComment = 0;
            fHasBody = FALSE;
            wState = CIX_SM_MAILHDR;
            fMail = TRUE;
            wFlags = 0;
            cixhdr.fPriority = FALSE;
            cixhdr.szMessageId[ 0 ] = '\0';
            cixhdr.szTitle[ 0 ] = '\0';
            cixhdr.szAuthor[ 0 ] = '\0';
            cixhdr.szFrom[ 0 ] = '\0';
            cixhdr.szTo[ 0 ] = '\0';
            cUULines = 0;
            cixhdr.fCheckForInReplyTo = TRUE;
            fCheckForXOLRField = TRUE;
            fGetTitleFromMsgBody = TRUE;
            fPrependCompactHdr = FALSE;
            fArticleSeen = FALSE;
            }
         else if( strncmp( lpLineBuf, scp_str44, strlen(scp_str44) ) == 0 ) {
            register int c;
            char * psz;

            /* Save any current message.
             */
            if( !PutCurrentMessage() )
               break;

            /* Collect the newsgroup name to szTopicName.
             */
            psz = lpLineBuf + strlen( scp_str44 );
            while( *psz == ' ' )
               ++psz;
            for( c = 0; c < LEN_TOPICNAME && *psz && *psz != ' '; ++psz, ++c )
               szTopicName[ c ] = *psz;
            szTopicName[ c ] = '\0';

            /* Begin to gather tagged articles.
             */
            cixhdr.fPriority = FALSE;
            cixhdr.szMessageId[ 0 ] = '\0';
            cixhdr.szTitle[ 0 ] = '\0';
            cixhdr.szAuthor[ 0 ] = '\0';
            cixhdr.szFrom[ 0 ] = '\0';
            cixhdr.szTo[ 0 ] = '\0';
            cixhdr.dwComment = 0;
            cUULines = 0;
            cixhdr.dwMsg = (DWORD)-1;
            fUsenet = TRUE;
            fHasBody = FALSE;
            wState = CIX_SM_START;
            fTaggedStart = TRUE;
            }
         else if( strncmp( lpLineBuf, scp_str42, strlen(scp_str42) ) == 0 ) {
            if( !PutCurrentMessage() )
               break;
            strcpy( szConfName, Amdb_GetFolderName( GetNewsFolder() ) );
            cixhdr.fPriority = FALSE;
            cixhdr.szMessageId[ 0 ] = '\0';
            cixhdr.szTitle[ 0 ] = '\0';
            cixhdr.szAuthor[ 0 ] = '\0';
            cixhdr.szFrom[ 0 ] = '\0';
            cixhdr.szTo[ 0 ] = '\0';
            cixhdr.dwComment = 0;
            cUULines = 0;
            cixhdr.dwMsg = (DWORD)-1;
            fUsenet = TRUE;
            fHasBody = FALSE;
            wState = CIX_SM_USENETHDRS;
            }
         else if( strncmp( lpLineBuf, szNewsgroupStartEcho, strlen( szNewsgroupStartEcho ) ) == 0 )
            wState = CIX_SM_NEWSGROUPLIST;
         else if( strncmp( lpLineBuf, scp_str6, strlen( scp_str6 ) ) == 0 ) {
            char * psz;

PPUsenet:   if( !PutCurrentMessage() )
               break;
            cbMsg = cbsz = cbTotal = 0;
            AppendLineToMessage( lpLineBuf, TRUE );
//          AppendToMessage( lpLineBuf, TRUE );
            psz = lpLineBuf + strlen( scp_str6 );
            while( isdigit( *psz ) )
               cbMsg = cbMsg * 10 + ( *psz++ - '0' );
            if( cbMsg == 0 )
               break;
            strcpy( szConfName, Amdb_GetFolderName( GetNewsFolder() ) );
            cixhdr.fPriority = FALSE;
            cixhdr.szMessageId[ 0 ] = '\0';
            cixhdr.szTitle[ 0 ] = '\0';
            cixhdr.szAuthor[ 0 ] = '\0';
            cixhdr.szFrom[ 0 ] = '\0';
            cixhdr.szTo[ 0 ] = '\0';
            cixhdr.dwComment = 0;
            cUULines = 0;
            cixhdr.dwMsg = (DWORD)-1;
            fUsenet = TRUE;
            fHasBody = FALSE;
            wState = CIX_SM_MAILHDR;
            fArticleSeen = FALSE;
            }
         else if( strncmp( lpLineBuf, scp_str39, strlen( scp_str39 ) ) == 0 ) {
            char * psz;

            /* Parse Semaphore 4 scratchpad format. This is an extra line
             * before each message which specify the message attributes.
             */
PPS4:       if( !PutCurrentMessage() )
               break;
            psz = lpLineBuf + 5;
            while( *psz )
               switch( *psz++ )
                  {
                  case 'U': fMarkMsgUnread = *psz++ == '1'; break;
                  case 'M': fMarkMsgMarked = *psz++ == '1'; break;
                  case 'L': fMarkMsgLocked = *psz++ == '1'; break;
                  }
            wState = CIX_SM_START;
            }
         else if( strncmp( lpLineBuf, scp_str45, strlen( scp_str45 ) ) == 0 ) {
            char * psz;

            /* Parse MF message flags introduced by Ameol v2.01 and used
             * by other OLRs.
             */
PPMF:       if( !PutCurrentMessage() )
               break;
            psz = lpLineBuf + strlen(scp_str45);
            while( *psz )
               switch( *psz++ )
                  {
                  case 'I': fMarkMsgIgnore = TRUE; break;
                  case 'A': fMarkMsgPriority = TRUE; break;
                  case 'D': fMarkMsgDelete = TRUE; break;
                  case 'G': fMarkMsgTagged = TRUE; break;
                  case 'W': fMarkMsgWithdraw = TRUE; break;
                  case 'H': fMarkMsgWatched = TRUE; break;
                  case 'K': fMarkMsgKeep = TRUE; break;
                  case 'U': fMarkMsgUnread = TRUE; break;
                  case 'M': fMarkMsgMarked = TRUE; break;
                  case 'L': fMarkMsgLocked = TRUE; break;
                  }
            wState = CIX_SM_START;
            }
         else if( strncmp( lpLineBuf + 1, szShowConfStartEcho, strlen( szShowConfStartEcho ) ) == 0 ) {
            register int c;
            char * psz;

PPShowConf: if( !PutCurrentMessage() )
               break;
            psz = lpLineBuf + 1 + strlen( szShowConfStartEcho );
            while( *psz == ' ' )
               ++psz;
            for( c = 0; c < LEN_CONFNAME+1 && *psz && *psz != ' '; ++psz, ++c ) //FS#154 2054
               szConfName[ c ] = *psz;
            szConfName[ c ] = '\0';
            while( *psz == ' ' )
               ++psz;
            fNoAddTopics = FALSE;
            if( *psz == '1' )
               fNoAddTopics = TRUE;
            cbMsg = cbsz = cbTotal = 0;
            wState = CIX_SM_SHOWCONF;
            }
         else if( strncmp( lpLineBuf + 1, szHelpListStartEcho, strlen( szHelpListStartEcho ) ) == 0 ) {
PPHelpList: if( !PutCurrentMessage() )
               break;
            cbMsg = cbsz = cbTotal = 0;
            wState = CIX_SM_HELPLIST;
            }
         else if( strncmp( lpLineBuf + 1, szFileListStartEcho, strlen( szFileListStartEcho ) ) == 0 ) {
            register int c;
            char * psz;

PPFileList: if( !PutCurrentMessage() )
               break;
            psz = lpLineBuf + 1 + strlen( szFileListStartEcho );
            while( *psz == ' ' )
               ++psz;
            if( *psz == '!' )
               {
               PCL pcl;
               PTL ptl;

               ++psz;
               for( c = 0; c < LEN_CONFNAME+1 && *psz && *psz != '/'; ++psz, ++c ) //FS#154 2054
                  szConfName[ c ] = *psz;
               szConfName[ c ] = '\0';
               if( *psz == '/' )
                  ++psz;
               for( c = 0; c < LEN_TOPICNAME && *psz; ++psz, ++c )
                  szTopicName[ c ] = *psz;
               szTopicName[ c ] = '\0';
               if( pcl = Amdb_OpenFolder( NULL, szConfName ) )
                  if( ptl = Amdb_OpenTopic( pcl, szTopicName ) )
                     {
                     TOPICINFO topicinf;

                     Amdb_GetTopicInfo( ptl, &topicinf );
                     /* Changed to using szCurTopicFileName instead of szConfName
                     ** Because we need to keep the conf name proper during flist
                     ** parsing. YH 04/04/96
                     */
                     wsprintf( szCurTopicFileName, "%s.FLS", (LPSTR)topicinf.szFileName );
                     }
               }
            else
               {
               for( c = 0; c < LEN_CONFNAME+1 && *psz; ++psz, ++c ) //FS#154 2054
                  szConfName[ c ] = *psz;
               szConfName[ c ] = '\0';
               }
            wState = CIX_SM_FILELIST;
            cbMsg = cbsz = cbTotal = 0;
            }
         else if( strncmp( lpLineBuf + 1, szFdirStartEcho, strlen( szFdirStartEcho ) ) == 0 ) {
            register int c;
            char * psz;

PPFdir:     if( !PutCurrentMessage() )
               break;
            psz = lpLineBuf + 1 + strlen( szFdirStartEcho );
            while( *psz == ' ' )
               ++psz;
            for( c = 0; c < LEN_CONFNAME+1 && *psz && *psz != '/'; ++psz, ++c ) //FS#154 2054
               szConfName[ c ] = *psz;
            szConfName[ c ] = '\0';
            if( *psz == '/' )
               ++psz;
            for( c = 0; c < LEN_TOPICNAME && *psz && *psz != '/'; ++psz, ++c )
               szTopicName[ c ] = *psz;
            szTopicName[ c ] = '\0';
            cbMsg = cbsz = cbTotal = 0;
            wState = CIX_SM_FDIR;
            }
         else if( strncmp( lpLineBuf + 1, szUniqueIDEcho, strlen( szUniqueIDEcho ) ) == 0 )
PPID:       wUniqueID = atoi( lpLineBuf + 1 + strlen( szUniqueIDEcho ) );
         else if( strncmp( lpLineBuf + 1, szHelpStartEcho, strlen( szHelpStartEcho ) ) == 0 ) {
            register int c;
            char * psz;

PPHelp:     if( !PutCurrentMessage() )
               break;
            psz = lpLineBuf + 1 + strlen( szHelpStartEcho );
            while( *psz == ' ' )
               ++psz;
            for( c = 0; c < 40 && *psz; ++psz, ++c )
               szTopicName[ c ] = *psz;
            szTopicName[ c ] = '\0';
            cbMsg = cbsz = cbTotal = 0;
            wState = CIX_SM_HELP;
            }
         else if( strncmp( lpLineBuf + 1, szParListStartEcho, strlen( szParListStartEcho ) ) == 0 ) {
            register int c;
            char * psz;

PPParList:  if( !PutCurrentMessage() )
               break;
            psz = lpLineBuf + 1 + strlen( szParListStartEcho );
            while( *psz == ' ' )
               ++psz;
            for( c = 0; c < LEN_CONFNAME+1 && *psz; ++psz, ++c )//FS#154 2054
               szConfName[ c ] = *psz;
            szConfName[ c ] = '\0';
            cbMsg = cbsz = cbTotal = 0;
            wState = CIX_SM_PARLIST;
            }
         else if( strncmp( lpLineBuf + 1, szResumeStartEcho, strlen( szResumeStartEcho ) ) == 0 ) {
            register int c;
            char * psz;

PPResume:   if( !PutCurrentMessage() )
               break;
            psz = lpLineBuf + 1 + strlen( szResumeStartEcho );
            while( *psz == ' ' )
               ++psz;
            for( c = 0; c < 14 && *psz; ++psz, ++c )
               cixhdr.szAuthor[ c ] = *psz;
            cixhdr.szAuthor[ c ] = '\0';
            cbMsg = cbsz = cbTotal = 0;
            wState = CIX_SM_RESUME;
            }
         else if( strncmp( lpLineBuf, scp_str17, 8 ) == 0 )
            {
            if( !PutCurrentMessage() )
               break;
            ParseConferenceTitle( lpLineBuf + 8 );
            wState = CIX_SM_START;
            fHasJoining = TRUE;
            }
         else if( strncmp( lpLineBuf, scp_str18, 3 ) == 0 )
            {
            nType = HDRTYP_COMPACT;
            if( !PutCurrentMessage() )
               break;
            cixhdr.szMessageId[ 0 ] = '\0';
            cixhdr.dwComment = 0;
            cixhdr.fPriority = FALSE;
            ParseCompactHeader( lpLineBuf + 3 );
            fGetTitleFromMsgBody = TRUE;
            cixhdr.szTitle[ 0 ] = '\0';
            if( !fBuilding )
               cixhdr.ptl = NULL;
            cbsz = cbTotal = 0;
            fCheckMaxMsgSize = TRUE;
            fPrependCompactHdr = TRUE;
            fPreBody = FALSE;
            wState = CIX_SM_CIXMSGBODY;
            }
         else if( strcmp( lpLineBuf, scp_str19 ) == 0 )
            {
            wFlags |= TF_READONLY;
            wState = CIX_SM_START;
            }
         else if( strcmp( lpLineBuf, scp_str10 ) == 0 )
            {
            wFlags |= TF_READONLY;
            wState = CIX_SM_START;
            }
         else if( strcmp( lpLineBuf, scp_str13 ) == 0 )
            {
            wFlags |= TF_TOPICFULL;
            wState = CIX_SM_START;
            }
         else if( strcmp( lpLineBuf, scp_str14 ) == 0 )
            {
            nType = HDRTYP_VERBOSE;
            fPrependCompactHdr = TRUE;
            wState = CIX_SM_CIXMSGHDR;
            }
         else if( strcmp( lpLineBuf, scp_str15 ) == 0 )
            {
            nType = HDRTYP_TERSE;
            fPrependCompactHdr = TRUE;
            wState = CIX_SM_CIXMSGHDR;
            }
         else if( strncmp( lpLineBuf, scp_str1, strlen( scp_str1 ) ) == 0 )
            wState = CIX_SM_START;
         else if( strncmp( lpLineBuf, scp_str2, strlen( scp_str2 ) ) == 0 )
            wState = CIX_SM_START;
         else
            wState = CIX_SM_START;
         break;

      case CIX_SM_CIXMSGHDR: {
         char * psz;

         if( !PutCurrentMessage() )
            break;
         psz = ParseConfTopic( lpLineBuf );
         psz = ParseCIXMessageNumber( psz );
         psz = ParseCIXAuthor( psz );
         psz = ParseCIXMessageLength( psz );
         if( _strcmpi( szConfName, "mail" ) == 0 )
            {
            cixhdr.dwComment = 0;
            cbsz = cbTotal = 0;
            wState = CIX_SM_MAILMSGHDR;
            wFlags = 0;
            cixhdr.dwMsg = (DWORD)-1;
            cixhdr.szMessageId[ 0 ] = '\0';
            cixhdr.fCheckForInReplyTo = TRUE;
            fCheckForXOLRField = TRUE;
            if( !fBuilding )
               cixhdr.ptl = GetPostmasterFolder();
            break;
            }
         psz = ParseCIXDateTime( psz );
         cbsz = cbTotal = 0;
         fGetTitleFromMsgBody = FALSE;
         cixhdr.szMessageId[ 0 ] = '\0';
         cixhdr.dwComment = 0;
         cixhdr.szTitle[ 0 ] = '\0';
         if( !fBuilding )
            cixhdr.ptl = NULL;
         wState = ( nType == HDRTYP_TERSE ) ? CIX_SM_TERSEHDR : CIX_SM_VERBOSEHDR;
         break;
         }

      case CIX_SM_USENETHDRS:
         /* Parse a header downloaded from the CIX Usenet gateway.
          */
         if( strncmp( lpLineBuf, scp_str42, strlen(scp_str42) ) != 0 )
            ParseCIXUsenetHeader( lpLineBuf );
         else
            {
            wState = CIX_SM_START;
            cbsz = 0;
            }
         break;

      case CIX_SM_TERSEHDR:
         if( strcmp( lpLineBuf, scp_str20 ) == 0 && nType == HDRTYP_TERSE )
            {
            fGetTitleFromMsgBody = TRUE;
            fCheckMaxMsgSize = TRUE;
            fPreBody = FALSE;
//          cixhdr.fPriority = FALSE;
            wState = CIX_SM_CIXMSGBODY;
            }
         else if( strcmp( lpLineBuf, scp_str21 ) == 0 )
            {
            fGetTitleFromMsgBody = TRUE;
            wState = CIX_SM_TERSEDELIM;
            }
         else if( strncmp( lpLineBuf, scp_str12, strlen( scp_str12 ) ) == 0 ) {
            char * psz;

            psz = lpLineBuf + strlen( scp_str12 );
            cixhdr.dwComment = 0;
            while( isdigit( *psz ) )
               cixhdr.dwComment = cixhdr.dwComment * 10 + ( *psz++ - '0' );
            fGetTitleFromMsgBody = TRUE;
            wState = CIX_SM_TERSEDELIM;
            break;
            }
         break;

      case CIX_SM_TERSEDELIM:
         fCheckMaxMsgSize = TRUE;
         fPreBody = FALSE;
         cixhdr.fPriority = FALSE;
         wState = CIX_SM_CIXMSGBODY;
         break;

      case CIX_SM_CIXMSGBODY: {
         BOOL fInMsg;

         /* If message size is larger than the maxmessage size,
          * then there is a likelihood that the message body will
          * not have been filed. In this case, skip to the start
          * of the next message header.
          */
         if( fCheckMaxMsgSize && cbMsg > dwMaxMsgSize )
            {
            AppendLineToMessage( GS(IDS_STR653), TRUE );
            wState = CIX_SM_START;
            break;
            }

         /* If message size within total, append line to message.
          */
         if( cUULines > 0 )
            {
            if( fInMsg = nUULine > 1 )
               --nUULine;
            }
         else if( cbTotal + 1 == cbMsg && fPreBody )
            {
            CheckForPreBody();
            fInMsg = FALSE;
            }
         else
            fInMsg = cbTotal < cbMsg;
         if( fInMsg )
            {
            unsigned char * pszNonSpc;
            BOOL fNewline;
            UINT cbBefore;

            /* Find the first non-space.
             */
            fNewline = TRUE;
            for( pszNonSpc = lpLineBuf; *pszNonSpc && isspace(*pszNonSpc); ++pszNonSpc );

            /* If prepending compact header, construct it.
             */
            if( fPrependCompactHdr )
               {
               PrependCompactHeader();
               fPrependCompactHdr = FALSE;
               }

            /* Handle the typical CIX hacks of storing In-Reply-To and
             * X-Olr in the message body.
             */
            if( cixhdr.fCheckForInReplyTo )
               if( CheckForInReplyTo( &cixhdr, lpLineBuf ) )
                  {
                  fHasBody = TRUE;
                  AppendToMessage( lpLineBuf, TRUE );
                  CheckForPreBody();
                  break;
                  }

            /* Check for X-Olr field. If found, append to the header
             */
            if( fCheckForXOLRField )
               if( CheckForXOLRField( lpLineBuf ) )
                  {
                  fHasBody = TRUE;
                  AppendToMessage( lpLineBuf, TRUE );
                  CheckForPreBody();
                  break;
                  }

            /* If we haven't got a message title (which will be the case if
             * we're parsing a CIX conference message), then use the first line
             * of the message as the title.
             */
            if( fGetTitleFromMsgBody && *pszNonSpc )
               {
               if( !fUseCopiedMsgSubject ||
                   ( ( strncmp( lpLineBuf, "**COPIED FROM: >>>", 18 ) != 0 
                   && strncmp( lpLineBuf, "**REPOST:", 9 ) != 0 ) && fUseCopiedMsgSubject ) )
                  {
                  register int c;

                  for( c = 0; lpLineBuf[ c ] && c < LEN_TITLE; ++c )
                     cixhdr.szTitle[ c ] = lpLineBuf[ c ];
                  cixhdr.szTitle[ c ] = '\0';
                  fGetTitleFromMsgBody = FALSE;
                  }
               }

            /* Neither X-Olr nor In-Reply-To were found, so
             * disable pre-body scanning and append the body text.
             */
            CheckForPreBody();
            if( *lpLineBuf != '\0' )
               fHasBody = TRUE;

            /* Handle quoted printable or 7-bit data.
             */
            cbBefore = strlen( lpLineBuf ) + 1;
            fNewline = ApplyEncodingType( lpLineBuf, &cixhdr );

            /* Now add to the message.
             */
            AppendLineToMessage( lpLineBuf, fNewline );
//          cbTotal += ( cbLine ); // FS#83 2.56.2051  
            cbTotal += ( !fIsAmeol2Scratchpad || CountCR() ? cbLine : cbBefore); // !!SM!! 2.56.2059
//          cbTotal += ( CountCR() ? cbLine : cbBefore ); // !!SM!! 2.56.2059
//          cbTotal += ( CountCR() ? cbLine : ( ( strchr( lpLineBuf, '\x13' ) != NULL ) ? ( cbLine + 2 ) : cbBefore ) );
            break;
            }

         /* If >>> seen, start parsing a new compact header.
          */
         if( ( strncmp( lpLineBuf, scp_str18, 3 ) == 0 ) )
            {
            if( !PutCurrentMessage() )
               break;
            cixhdr.dwComment = 0;
            cixhdr.szMessageId[ 0 ] = '\0';
            ParseCompactHeader( lpLineBuf + 3 );
            cixhdr.szTitle[ 0 ] = '\0';
            cbsz = cbTotal = 0;
            if( !fBuilding )
               cixhdr.ptl = NULL;
            nType = HDRTYP_COMPACT;
            fGetTitleFromMsgBody = TRUE;
            fPrependCompactHdr = TRUE;
            }
         else if( strncmp( lpLineBuf, scp_str39, strlen( scp_str39 ) ) == 0 )
            goto PPS4;
         else if( strncmp( lpLineBuf, scp_str45, strlen( scp_str45 ) ) == 0 )
            goto PPMF;
         else if( strncmp( lpLineBuf + 1, szParListStartEcho, strlen( szParListStartEcho ) ) == 0 )
            goto PPParList;
         else if( strncmp( lpLineBuf + 1, szShowConfStartEcho, strlen( szShowConfStartEcho ) ) == 0 )
            goto PPShowConf;
         else if( strncmp( lpLineBuf + 1, szResumeStartEcho, strlen( szResumeStartEcho ) ) == 0 )
            goto PPResume;
         else if( strncmp( lpLineBuf + 1, szHelpListStartEcho, strlen( szHelpListStartEcho ) ) == 0 )
            goto PPHelpList;
         else if( strncmp( lpLineBuf + 1, szHelpStartEcho, strlen( szHelpStartEcho ) ) == 0 )
            goto PPHelp;
         else if( strncmp( lpLineBuf + 1, szUniqueIDEcho, strlen( szUniqueIDEcho ) ) == 0 )
            goto PPID;
         else if( strncmp( lpLineBuf + 1, szFileListStartEcho, strlen( szFileListStartEcho ) ) == 0 )
            goto PPFileList;
         else if( strncmp( lpLineBuf + 1, szFdirStartEcho, strlen( szFdirStartEcho ) ) == 0 )
            goto PPFdir;
         else if( strncmp( lpLineBuf, scp_str16, strlen(scp_str16) ) == 0 )
            goto PPMail;
         else if( strncmp( lpLineBuf, scp_str51, strlen(scp_str51) ) == 0 )
            goto PPMail;
         else if( strncmp( lpLineBuf, scp_str6, strlen( scp_str6 ) ) == 0 )
            goto PPUsenet;
         else if( strncmp( lpLineBuf, scp_str1, strlen( scp_str1 ) ) == 0 )
            wState = CIX_SM_START;
         else if( strncmp( lpLineBuf, scp_str44, strlen( scp_str44 ) ) == 0 )
            {
            /* Finished! Tidy up at the end.
             */
            if( wError != AMDBERR_USERABORT && cbsz )
               if( wState == CIX_SM_CIXMSGBODY || wState == CIX_SM_START )
                  PutCurrentMessage();
            EndOfCixnewsTagged();
            wState = CIX_SM_START;
            }
         else if( strcmp( lpLineBuf, scp_str14 ) == 0 )
            {
            nType = HDRTYP_VERBOSE;
            wState = CIX_SM_CIXMSGHDR;
            fPrependCompactHdr = TRUE;
            }
         else if( strcmp( lpLineBuf, scp_str15 ) == 0 )
            {
            nType = HDRTYP_TERSE;
            wState = CIX_SM_CIXMSGHDR;
            fPrependCompactHdr = TRUE;
            }
         else if( strncmp( lpLineBuf, scp_str17, 7 ) == 0 )
            {
            if( !PutCurrentMessage() )
               break;
            ParseConferenceTitle( lpLineBuf + 8 );
            wState = CIX_SM_START;
            fHasJoining = TRUE;
            }
         else if( strncmp( lpLineBuf, scp_str2, strlen( scp_str2 ) ) == 0 )
            wState = CIX_SM_START;
         break;
         }

      case CIX_SM_MAILHDR:
         if( fContinuedLine )
            AppendToMessage( lpLineBuf, TRUE );
         else if( strncmp( lpLineBuf, scp_str44, strlen( scp_str44 ) ) == 0 )
            {
            EndOfCixnewsTagged();
            wState = CIX_SM_START;
            }
         else
            {
            register int c;
            BOOL fSpaces;
            char * psz;

            psz = lpLineBuf;
            fSpaces = *psz == ' ' || *psz == '\t';
            while( *psz == ' ' || *psz == '\t' )
               ++psz;
            if( *psz == '\0' ) 
               {
               /* If we haven't seen a subject header, then use
                * (No Subject) as the subject header.
                */
               if( fGetTitleFromMsgBody )
                  {
                  strcpy( cixhdr.szTitle, GS(IDS_STR558) );
                  fGetTitleFromMsgBody = FALSE;
                  }
               fCheckMaxMsgSize = FALSE;
               fPreBody = TRUE;
               cbBlankLine = cbLine;
               wState = CIX_SM_CIXMSGBODY;
               }
            else {
               AppendToMessage( lpLineBuf, TRUE );
               if( !fSpaces )
                  {
                  for( c = 0; c < LEN_RFCTYPE && *psz && *psz != ':'; ++c )
                     cixhdr.szType[ c ] = *psz++;
                  cixhdr.szType[ c ] = '\0';
                  if( *psz == ':' ) ++psz;
                  while( *psz == ' ' )
                     ++psz;
                  }
               if( _strcmpi( cixhdr.szType, scp_str22 ) == 0 )
                  ParseMailDate( psz, &cixhdr );
               else if( _strcmpi( cixhdr.szType, scp_str28 ) == 0 && !fArticleSeen )
                  nUULine = cUULines = atoi( psz ) + 1;
               else if( _strcmpi( cixhdr.szType, scp_str29 ) == 0 )
                  ParseMailArticle( psz );
               else if( _strcmpi( cixhdr.szType, scp_str36 ) == 0 && !fArticleSeen )
                  ParseCrossReference( psz, &cixhdr );
               else if( _strcmpi( cixhdr.szType, scp_str37 ) == 0 && !fMail && !fArticleSeen )
                  ParseMailNewsgroup( psz );
               else if( _strcmpi( cixhdr.szType, scp_str23 ) == 0 && !cixhdr.szAuthor[ 0 ] )
                  {
//                ParseCIXMailAuthor( psz, TRUE );
                  ParseCIXMailAuthor( psz, FALSE ); // !!SM!!
                  cixhdr.fPriority = IsLoginAuthor( cixhdr.szAuthor );
                  }
               else if( _strcmpi( cixhdr.szType, scp_str40 ) == 0 )
                  ParseCixMailToField( psz );
               else if( _strcmpi( cixhdr.szType, scp_str43 ) == 0 )
                  ParseCixMailCCField( psz );
               else if( _strcmpi( cixhdr.szType, scp_str25 ) == 0 )
                  ParseMessageId( psz, &cixhdr, !fUsenet );
               else if( _strcmpi( cixhdr.szType, scp_str24 ) == 0 )
                  {
                  ParseReplyFolder( psz );
                  cixhdr.fPriority = IsLoginAuthor( cixhdr.szAuthor );
                  }
               else if( _strcmpi( cixhdr.szType, scp_str47 ) == 0 )
                  ParseContentTransferEncoding( psz, &cixhdr );
               else if( _strcmpi( cixhdr.szType, scp_str48 ) == 0 )
                  ParseDispositionNotification( psz, &cixhdr );
               else if( _strcmpi( cixhdr.szType, scp_str52 ) == 0 )
                  ParseDispositionNotification( psz, &cixhdr );
               else if( _strcmpi( cixhdr.szType, scp_str30 ) == 0 )
                  ParseReference( psz, &cixhdr );
               else if( _strcmpi( cixhdr.szType, scp_str33 ) == 0 )
                  ParseXOlr( psz, &cixhdr );
               else if( _strcmpi( cixhdr.szType, scp_str38 ) == 0 )
                  ParseXPriority( psz, &cixhdr );
               else if( _strcmpi( cixhdr.szType, scp_str53 ) == 0 )
                  ParsePriority( psz, &cixhdr );
               else if( _strcmpi( cixhdr.szType, scp_str26 ) == 0 )
                  ParseReplyNumber( psz, &cixhdr );
               else if( _strcmpi( cixhdr.szType, scp_str27 ) == 0 && !cixhdr.szTitle[ 0 ] )
                  {
                  ParseMailSubject( psz, &cixhdr );
                  fGetTitleFromMsgBody = FALSE;
                  }
//             fParsedID = FALSE;
               wState = CIX_SM_MAILHDR;
               }
            }
         break;

      case CIX_SM_MAILMSGHDR:
         if( _strnicmp( lpLineBuf, scp_str31, 11 ) )
            {
            fHasBody = FALSE;
            wState = CIX_SM_MAILHDR;
            }
         break;

      case CIX_SM_VERBOSEHDR:
         if( strcmp( lpLineBuf, scp_str32 ) == 0 && nType == HDRTYP_VERBOSE )
            {
            fGetTitleFromMsgBody = TRUE;
            fCheckMaxMsgSize = TRUE;
            fPreBody = FALSE;
//          cixhdr.fPriority = FALSE;
            wState = CIX_SM_CIXMSGBODY;
            }
         else if( strncmp( lpLineBuf, scp_str4, strlen( scp_str4 ) ) == 0 )
            {
            fGetTitleFromMsgBody = TRUE;
            wState = CIX_SM_VERBOSEHDR;
            }
         else if( strcmp( lpLineBuf, scp_str3 ) == 0 )
            {
            fGetTitleFromMsgBody = TRUE;
            wState = CIX_SM_VERBOSEHDR;
            }
         else if( strncmp( lpLineBuf, scp_str5, strlen( scp_str5 ) ) == 0 )
            {
            char * psz;

            psz = lpLineBuf + strlen( scp_str5 );
            cixhdr.dwComment = 0;
            while( isdigit( *psz ) )
               cixhdr.dwComment = cixhdr.dwComment * 10 + ( *psz++ - '0' );
            wState = CIX_SM_VERBOSEHDR;
            fGetTitleFromMsgBody = TRUE;
            break;
            }
         break;

      case CIX_SM_RESUME:
         /* We're parsing a resume, so add lines to the current resume body
          * until we see a resume end marker with our end ID.
          */
         if( strncmp( lpLineBuf + 1, szResumeEndEcho, strlen( szResumeEndEcho ) ) == 0 )
            if( (WORD)atoi( lpLineBuf + 1 + strlen( szResumeEndEcho ) ) == wUniqueID )
               {
               /* End of resume, so add resume to the
                * resume database.
                */
               CreateNewResume( cixhdr.szAuthor, hpBuf );
               wState = CIX_SM_START;
               cbsz = 0;
               break;
               }
         AppendToMessage( lpLineBuf, TRUE );
         break;

      case CIX_SM_PARLIST:
         if( strncmp( lpLineBuf + 1, szParListEndEcho, strlen( szParListEndEcho ) ) == 0 )
            {
            PCL pcl;

            if( pcl = Amdb_OpenFolder( NULL, szConfName ) )
               {
               FOLDERINFO confinfo;
               char szFileName[ _MAX_FNAME ];
               HNDFILE fh;

               Amdb_GetFolderInfo( pcl, &confinfo );
               wsprintf( szFileName, "%s.PAR", (LPSTR)confinfo.szFileName );
               if( ( fh = Ameol2_CreateFile( szFileName, DSD_PARLIST, 0 ) ) != HNDFILE_ERROR )
                  {
                  BOOL fWriteFailed;

                  /* Write the participants list file. If it failed, report and
                   * delete the file.
                   * BUGBUG: If this happens during a scheduled event, there won't
                   * be a user around to action the response!!
                   */
                  while( fWriteFailed = ( Amfile_Write32( fh, hpBuf, cbsz ) != cbsz ) )
                     if( !DiskWriteError( hwndFrame, szFileName, TRUE, FALSE ) )
                        break;
                  Amfile_Close( fh );
                  if( fWriteFailed )
                     Amfile_Delete( szFileName );
                  }
               }
            wState = CIX_SM_START;
            cbsz = 0;
            }
         else if( strncmp( lpLineBuf, scp_str11, strlen( scp_str11 ) ) != 0 )
            AppendToMessage( lpLineBuf, TRUE );
         break;

      case CIX_SM_NEWSGROUPLIST:
         if( strncmp( lpLineBuf, szNewsgroupEndEcho, strlen( szNewsgroupEndEcho ) ) == 0 )
            wState = CIX_SM_START;
         else
            {
            char szNewsgroupName[LEN_TOPICNAME+1];
            register int c;
            PCL pcl;
            PTL ptl;

            /* Extract the newsgroup name.
             */
            for( c = 0; c < LEN_TOPICNAME && lpLineBuf[ c ] && lpLineBuf[ c ] != ':'; ++c )
               szNewsgroupName[ c ] = lpLineBuf[ c ];
            szNewsgroupName[ c ] = '\0';

            /* Locate it in the messagebase and if not found, then
             * create it.
             */
            if( lpLineBuf[ c ] == ':' )
               {
               PCAT pcat;

               ptl = NULL;
               for( pcat = Amdb_GetFirstCategory(); pcat; pcat = Amdb_GetNextCategory( pcat ) )
                  for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
                     if( ptl = Amdb_OpenTopic( pcl, szNewsgroupName ) )
                        break;
               if( NULL == ptl )
                  if( NULL != ( pcl = GetNewsFolder() ) )
                     {
                     ptl = Amdb_CreateTopic( pcl, szNewsgroupName, FTYPE_NEWS );
                     Amdb_SetCookie( ptl, NEWS_SERVER_COOKIE, (char *)szCixnews );
                     }
               }
            }
         break;

      case CIX_SM_HELPLIST:
         if( strncmp( lpLineBuf + 1, szHelpListEndEcho, strlen( szHelpListEndEcho ) ) == 0 )
            {
            wState = CIX_SM_START;
            cbsz = 0;
            }
         else
            AppendToMessage( lpLineBuf, TRUE );
         break;

      case CIX_SM_HELP:
         if( strncmp( lpLineBuf + 1, szHelpEndEcho, strlen( szHelpEndEcho ) ) == 0 )
            {
            wState = CIX_SM_START;
            cbsz = 0;
            }
         else
            AppendToMessage( lpLineBuf, TRUE );
         break;

      case CIX_SM_FILELIST: {
         /* I had to put the initialisations back into the declarations
          * because they are static variables whose values need to persist
          * outside the function scope.
          */
         static BOOL fFileListIsOK = FALSE;
         static int nLinesParsedInFileList = 0;

         /* The body of this case statement has been changed by YH on 04/04/96
          * This was done to fix a bug, where if an flist was not downloaded
          * properly then it still created an .fls. When the user came to 
          * view this corrupt .fls file it caused a GPF because the header
          * is corrupt (the first characters "AF/AFFF" or similar are missing
          * the date parsing code breaks down because it expects those characters
          * to be there and strips the month string off before parsing the date).
          *
          * The way I check for a corrupt header is to make sure there are the
          * correct number of tokens in the first line, Tokens in this case
          * are delimeted by spaces.
          */
         if( strncmp( lpLineBuf + 1, szFileListEndEcho, strlen( szFileListEndEcho ) ) == 0 )
            {
            HNDFILE fh;

            /* Only create .fls file if this flist data seemed O.K.
             */
            if( fFileListIsOK )
               {
               /* Changed from szConfName to szCurTopicFileName because
                * szConfName is now being used for the conf name proper, not
                * a file name. YH 04/04/96
                */
               if( ( fh = Ameol2_CreateFile( szCurTopicFileName, DSD_FLIST, 0 ) ) != HNDFILE_ERROR )
                  {
                  Amfile_Write32( fh, hpBuf, cbsz );
                  Amfile_Close( fh );
                  }
               }
            else
               {
               /* File List not downloaded properly so inform the user in the event log
                */
               char szEvent[256];

               wsprintf( szEvent, GS(IDS_STR1106), szConfName, szTopicName );
               Amevent_AddEventLogItem( ETYP_WARNING | ETYP_DISKFILE, szEvent );
               }
            wState = CIX_SM_START;
            cbsz = 0;

            /* Reset these local static variables for the next flist we might
             * find in the scratchpad.
             */
            nLinesParsedInFileList = 0;
            fFileListIsOK = FALSE;
            }
         else
            {
            /* If we're looking at the first line of the flist, then
             * that it's O.K.
             */
            if( nLinesParsedInFileList == 0 )
               fFileListIsOK = FileListHeaderOK( lpLineBuf );
            AppendToMessage( lpLineBuf, TRUE );
            nLinesParsedInFileList++;
            }
         break;
         }

      case CIX_SM_SHOWCONF:
         pclShowConf = NULL;
         if( strncmp( lpLineBuf + 1, szShowConfEndEcho, strlen( szShowConfEndEcho ) ) == 0 )
            {
            wState = CIX_SM_START;
            cbsz = 0;
            }

         else if( strncmp( lpLineBuf, scp_str49, strlen( scp_str49 ) ) == 0 )
            {
            wState = CIX_SM_START;
            cbsz = 0;
            break;
            }

         else if( strncmp( lpLineBuf, scp_str8, strlen( scp_str8 ) ) == 0 )
            {
            if( !( pclShowConf = Amdb_OpenFolder( NULL, szConfName ) ) )
               {
               PCAT pcat;

               pcat = GetCIXCategory();
               pclShowConf = Amdb_CreateFolder( pcat, szConfName, CFF_SORT );
               }
            if( pclShowConf )
               {
               char * psz;

               psz = lpLineBuf + strlen( scp_str8 );
               if( psz[ strlen( psz ) - 1 ] == '.' )
                  psz[ strlen( psz ) - 1 ] = '\0';
               Amdb_SetModeratorList( pclShowConf, psz );
               }
            wState = CIX_SM_SHOWCONFDATETIME;
            }
         else
            wState = CIX_SM_SHOWCONFTOPICLIST;
         break;

      case CIX_SM_SHOWCONFDATETIME:
         if( strncmp( lpLineBuf, scp_str9, strlen( scp_str9 ) ) == 0 )
            {
            if( !( pclShowConf = Amdb_OpenFolder( NULL, szConfName ) ) )
               {
               PCAT pcat;

               pcat = GetCIXCategory();
               pclShowConf = Amdb_CreateFolder( pcat, szConfName, CFF_SORT );
               }
            if( pclShowConf )
               {
               ParseCIXDateTime( lpLineBuf + strlen( scp_str9 ) );
               Amdb_SetFolderDateTime( pclShowConf, Amdate_PackDate( &cixhdr.date ), Amdate_PackTime( &cixhdr.time ) );
               }
            wState = CIX_SM_SHOWCONFTOPICLIST;
            }
         else
            wState = CIX_SM_SHOWCONFTOPICLIST;
         break;

      case CIX_SM_SHOWCONFTOPICLIST: {
         static BOOL fSkipping = FALSE;

         if( strncmp( lpLineBuf + 1, szShowConfEndEcho, strlen( szShowConfEndEcho ) ) == 0 )
            {
            /* FIX: The next line takes care of conferences on CIX that
             * have no topics.
             */
            if( pclShowConf && Amdb_GetFirstTopic( pclShowConf ) == NULL )
               {
               RESIGNOBJECT ro;
         
               Amob_New( OBTYPE_CIXRESIGN, &ro );
               Amob_CreateRefString( &ro.recConfName, szConfName );
               if( !Amob_Find( OBTYPE_CIXRESIGN, &ro ) )
                  Amob_Commit( NULL, OBTYPE_CIXRESIGN, &ro );
               Amob_Delete( OBTYPE_CIXRESIGN, &ro );
               Amdb_DeleteFolder( pclShowConf );
               wsprintf( lpTmpBuf, GS(IDS_STR994), (LPSTR)szConfName );
               ReportOnline( lpTmpBuf );
               }
            fSkipping = FALSE;
            wState = CIX_SM_START;
            cbsz = 0;
            }
         else if( *lpLineBuf == '\0' || *lpLineBuf == ' ' )
         {
            fSkipping = TRUE;
            wState = CIX_SM_SHOWCONFNOTE;
            fFirstNoteLine = TRUE;
         }

         else if( *lpLineBuf && *lpLineBuf != '-' && !fSkipping && !fNoAddTopics )
            {
            register int c;
            char * psz;
            PTL ptl;

            psz = lpLineBuf;
            for( c = 0; *psz && *psz != ' ' && c < LEN_TOPICNAME; ++c )
               szTopicName[ c ] = *psz++;
            szTopicName[ c ] = '\0';
            if( !( pclShowConf = Amdb_OpenFolder( NULL, szConfName ) ) )
               {
               PCAT pcat;

               pcat = GetCIXCategory();
               pclShowConf = Amdb_CreateFolder( pcat, szConfName, CFF_SORT );
               }
            if( pclShowConf )
               if( !( ptl = Amdb_OpenTopic( pclShowConf, szTopicName ) ) )
                  ptl = Amdb_CreateTopic( pclShowConf, szTopicName, FTYPE_CIX );
            }
            wState = CIX_SM_SHOWCONFNOTE;
            fFirstNoteLine = TRUE;
         break;
         }

      case CIX_SM_SHOWCONFNOTE:
         if( strncmp( lpLineBuf + 1, szShowConfEndEcho, strlen( szShowConfEndEcho ) ) == 0 )
         {
            PCL pcl;

            if( pcl = Amdb_OpenFolder( NULL, szConfName ) )
               {
               FOLDERINFO confinfo;
               char szFileName[ _MAX_FNAME ];
               HNDFILE fh;

               Amdb_GetFolderInfo( pcl, &confinfo );
               wsprintf( szFileName, "%s.CXN", (LPSTR)confinfo.szFileName );
               if( ( fh = Ameol2_CreateFile( szFileName, DSD_CURRENT, 0 ) ) != HNDFILE_ERROR )
                  {
                  BOOL fWriteFailed;

                  /* Write the participants list file. If it failed, report and
                   * delete the file.
                   * BUGBUG: If this happens during a scheduled event, there won't
                   * be a user around to action the response!!
                   */
                  while( fWriteFailed = ( Amfile_Write32( fh, hpBuf, cbsz ) != cbsz ) )
                     if( !DiskWriteError( hwndFrame, szFileName, TRUE, FALSE ) )
                        break;
                  Amfile_Close( fh );
                  if( fWriteFailed )
                     Amfile_Delete( szFileName );
                  }
               }
            wState = CIX_SM_START;
            cbsz = 0;
            }
         else
         {
            if( fFirstNoteLine )
            {
               AppendToMessage( "Padding line", TRUE );
               fFirstNoteLine = FALSE;
            }
            AppendToMessage( lpLineBuf, TRUE );
         }

         break;

      case CIX_SM_FDIR:
         if( strncmp( lpLineBuf + 1, szFdirEndEcho, strlen( szFdirEndEcho ) ) == 0 )
            {
            PCL pcl;
            PTL ptl;

            if( pcl = Amdb_OpenFolder( NULL, szConfName ) )
               if( ptl = Amdb_OpenTopic( pcl, szTopicName ) )
                  {
                  TOPICINFO topicinfo;
                  char szFileName[ _MAX_FNAME ];
                  HNDFILE fh;
   
                  Amdb_GetTopicInfo( ptl, &topicinfo );
                  wsprintf( szFileName, "%s.FLD", (LPSTR)topicinfo.szFileName );
                  if( ( fh = Ameol2_CreateFile( szFileName, DSD_FLIST, 0 ) ) != HNDFILE_ERROR )
                     {
                     BOOL fDiskWriteFailed;

                     /* If the write failed, delete the file and report
                      * an error.
                      */
                     while( fDiskWriteFailed = ( Amfile_Write32( fh, hpBuf, cbsz ) != cbsz ) )
                        if( !DiskWriteError( hwndFrame, szFileName, TRUE, FALSE ) )
                           break;
                     Amfile_Close( fh );
                     if( fDiskWriteFailed )
                        Amfile_Delete( szFileName );
                     }
               }
            wState = CIX_SM_START;
            cbsz = 0;
            }
         else
            AppendToMessage( lpLineBuf, TRUE );
         break;
      }

   /* Stop parsing if error
    */
   if( wError != AMDBERR_NOERROR )
      CIXImportParseCompleted();
}

/* This function constructs a compact header from the information
 * we've got so far and appends it to the message.
 */
void FASTCALL PrependCompactHeader( void )
{
   wsprintf( lpTmpBuf, szCompactHdrTmpl,
            szConfName,
            szTopicName,
            cixhdr.dwMsg,
            cixhdr.szAuthor,
            cbMsg,
            cixhdr.date.iDay,
            (LPSTR)pszCixMonths[ cixhdr.date.iMonth - 1 ],
            cixhdr.date.iYear % 100,
            cixhdr.time.iHour,
            cixhdr.time.iMinute );
   if( cixhdr.dwComment )
      wsprintf( lpTmpBuf + strlen( lpTmpBuf ), " c%lu*", cixhdr.dwComment );
   AppendLineToMessage( lpTmpBuf, TRUE );
}

/* This function checks for the blank line between the header
 * and body and adds it if required.
 */
void FASTCALL CheckForPreBody( void )
{
   if( fPreBody )
      {
      fPreBody = FALSE;
      AppendLineToMessage( "", TRUE );
      cbTotal += cbBlankLine;
      }
}

/* This function handles the end of a tagged section
 * of cixnews articles.
 */
void FASTCALL EndOfCixnewsTagged( void )
{
   PTL ptl;

   /* End of tagged headers. Now mark any messages which are still
    * 'tagged' in the newsgroup as unavailable.
    */
   if( NULL != ( ptl = PtlFromNewsgroup( szTopicName ) ) )
      {
      PTH pth;

      pth = NULL;
      Amdb_LockTopic( ptl );
      while( pth = Amdb_GetTagged( ptl, pth ) )
         {
         Amdb_MarkMsgTagged( pth, FALSE );
         Amdb_MarkMsgUnavailable( pth, TRUE );
         }
      Amdb_UnlockTopic( ptl );
      }
}

/* This function uncompresses the specified file. It first checks that the
 * file is arced, and does nothing otherwise. If it is arced, it renames
 * the file extension to BYT and uncompresses the file to its original
 * name.
 */
BOOL FASTCALL UnarcFile( HWND hwnd, char * npFileName )
{
   HCURSOR hOldCursor;
   hOldCursor = SetCursor( LoadCursor( NULL, IDC_WAIT ) );
   if( Amcmp_IsArcedFile( npFileName ) )
      {

      char sz[ _MAX_DIR ];

      /* Change the extension and make sure that
       * any existing file of that name does not exist.
       */
      strcpy( sz, npFileName );
      ChangeExtension( sz, "BYT" );
      Amfile_Delete( sz );

      /* Rename the input file.
       */
      if( !Amfile_Rename( npFileName, sz ) )
      {
         SetCursor( hOldCursor );
         return( FALSE );
      }

      /* Finally uncompress the file.
       */
      UnarcScratchpad( hwnd, sz, npFileName );
      }
   SetCursor( hOldCursor );
   return( TRUE );
}

/* This function uncompresses the specified input file to the
 * output file.
 */
void FASTCALL UnarcScratchpad( HWND hwnd, char * npArcFile, char * npOutFile )
{

   /* Create a gauge window on the status bar.
    */
   if( hwndStatus && IsWindowVisible( hwndStatus ) )
      {
      wsprintf( lpTmpBuf, GS(IDS_STR354), GetFileBasename(szScratchFile) );
      SendMessage( hwndStatus, SB_BEGINSTATUSGAUGE, 0, (LPARAM)(LPSTR)lpTmpBuf );
      }

   /* Call the uncompress function. If it fails, delete the
    * output file. Otherwise delete the input file.
    */
   if( !Amcmp_UncompressFile( hwndStatus, npArcFile, npOutFile ) )
      {
      fMessageBox( hwnd, 0, GS( IDS_STR50 ), MB_OK|MB_ICONEXCLAMATION );
      Amfile_Delete( npOutFile );
      }
   else
      Amfile_Delete( npArcFile );

   /* Delete the gauge window.
    */
   if( hwndStatus && IsWindowVisible( hwndStatus ) )
      SendMessage( hwndStatus, SB_ENDSTATUSGAUGE, 0, 0L );
}

static BOOL FASTCALL PutCurrentMessage( void )
{
   if( cbsz )
      {
      while( ( wError = PutMessage() ) != AMDBERR_NOERROR )
         switch( wError )
            {
            case AMDBERR_OUTOFDISKSPACE:
               if( !DiskSaveError( hwndFrame, pszDataDir, TRUE, FALSE ) )
                  return( FALSE );
               break;

            case AMDBERR_OUTOFMEMORY:
               if( !OutOfMemoryError( hwndFrame, TRUE, FALSE ) )
                  return( FALSE );
               break;

            case AMDBERR_CANNOTCREATEFILE:
               if( !CannotCreateFileError( hwndFrame ) )
                  return( FALSE );
               break;
            }
      InitialiseCixHdr();
      }
   return( wError == AMDBERR_NOERROR );
}

/* This function initialises the cixhdr structure ready
 * for parsing a new message.
 */
void FASTCALL InitialiseCixHdr( void )
{
   cbsz = 0;
   fMarkMsgUnread = !MarkAllRead();
   fMarkMsgMarked = FALSE;
   fMarkMsgLocked = FALSE;
   fMarkMsgKeep = FALSE;
   fMarkMsgDelete = FALSE;
   fMarkMsgTagged = FALSE;
   fMarkMsgWithdraw = FALSE;
   fMarkMsgWatched = FALSE;
   fMarkMsgPriority = FALSE;
   fMarkMsgIgnore = FALSE;
   fMail = FALSE;
   fUsenet = FALSE;
   InitialiseHeader( &cixhdr );
   if( !fBuilding )
      cixhdr.ptl = NULL;
}

/* The current conference file cannot be created.
 */
BOOL FASTCALL CannotCreateFileError( HWND hwnd )
{
   register int r;
   char szFileName[ _MAX_FNAME ];
   char sz[ 100 ];
   PTL ptl;
   PCL pcl;

   szFileName[ 0 ] = '\0';
   if( pcl = Amdb_OpenFolder( NULL, szConfName ) )
      if( ptl = Amdb_OpenTopic( pcl, szTopicName ) )
         {
         Amdb_GetInternalTopicName( ptl, szFileName );
         strcat( szFileName, ".TXT" );
         AnsiUpper( szFileName );
         }
   wsprintf( sz, GS(IDS_STR691), (LPSTR)szFileName );
   r = fMessageBox( hwnd, 0, sz, MB_RETRYCANCEL|MB_ICONEXCLAMATION );
   return( r == IDRETRY );
}

/* This function appends the input line to the current
 * message being composed.
 */
static void FASTCALL AppendToMessage( char * sz, BOOL fNewline )
{
   AppendLineToMessage( sz, fNewline );
// cbTotal += ( CountCR() ? cbLine : strlen( sz ) + 1 ); // !!SM!! 2.56.2059
   cbTotal += ( !fIsAmeol2Scratchpad || CountCR() ? cbLine : strlen( sz ) + 1 ); // !!SM!! 2.56.2059  
// cbTotal += ( cbLine ); // FS#83 2.56.2051  
}

/* Thus function appends the specified line to the current
 * message being composed.
 */
static void FASTCALL AppendLineToMessage( char * sz, BOOL fNewline )
{
   /* Cast to long to avoid wrap round because MAX_MSGSIZE is close to 0xFFFF
    */
   if( cbsz + (DWORD)strlen( sz ) + 3L >= cbMaxMsgSize )
      {
      if( cbMsg > cbMaxMsgSize )
      {
         cbMaxMsgSize = cbMsg + 2048;
         if( !fResizeMemory32( &hpBuf, cbMaxMsgSize ) )
            {
            wError = AMDBERR_OUTOFMEMORY;
            OutOfMemoryError( hwndFrame, FALSE, FALSE );
            wsprintf( lpTmpBuf, "%u", GetLastError() );
            fMessageBox( hwndFrame, 0, lpTmpBuf, MB_OK|MB_ICONINFORMATION );
            wsprintf( lpTmpBuf, "%u", cbMaxMsgSize );
            fMessageBox( hwndFrame, 0, lpTmpBuf, MB_OK|MB_ICONINFORMATION );
            return;
            }
      }
      else
      {
         cbMaxMsgSize += 0xFFFF;
         if( !fResizeMemory32( &hpBuf, cbMaxMsgSize ) )
            {
            wError = AMDBERR_OUTOFMEMORY;
            OutOfMemoryError( hwndFrame, FALSE, FALSE );
            wsprintf( lpTmpBuf, "%u", GetLastError() );
            fMessageBox( hwndFrame, 0, lpTmpBuf, MB_OK|MB_ICONINFORMATION );
            wsprintf( lpTmpBuf, "%u", cbMaxMsgSize );
            fMessageBox( hwndFrame, 0, lpTmpBuf, MB_OK|MB_ICONINFORMATION );
            return;
            }
         }
      }
   hmemcpy( hpBuf + cbsz, sz, strlen( sz ) );
   cbsz += strlen( sz );
   if( fNewline )
      {
      hpBuf[ cbsz++ ] = CR;
      hpBuf[ cbsz++ ] = LF;
      }
   hpBuf[ cbsz ] = '\0';
}

/* This function stores the current message in the message database.
 */
static int FASTCALL PutMessage( void )
{
   HWND hwndTopic2;
   DWORD dwLclFlags;
   PTL ptlReal;
   int nMatch;
   PTH pth;
   int r;
   char szTempAuthor[ LEN_INTERNETNAME + 1 ];
   char szTempAuthor2[ LEN_INTERNETNAME + 1 ];
   char szTmpName[ 64 ];

   /* If buffer is NULL, must have been an out-of-memory condition
    */
   if( !hpBuf )
      return( AMDBERR_OUTOFMEMORY );

   /* If no folder handle predefined.
    */
   if( NULL == cixhdr.ptl )
      {
      PCL pcl;

      /* Get the conference handle, and create it if it does not exist
       */
      if( !( pcl = Amdb_OpenFolder( NULL, szConfName ) ) )
         {
         int position;
         PCAT pcat;

         position = CFF_SORT;
         if( strcmp( szConfName, "cixnews" ) == 0 || fMail || fUsenet )
            position |= CFF_KEEPATTOP;
         pcat = GetCIXCategory();
         if( !( pcl = Amdb_CreateFolder( pcat, szConfName, position ) ) )
            return( AMDBERR_OUTOFMEMORY );
         }
   
      /* Get the topic handle, and create it if it does not exist.
       */
      if( !( cixhdr.ptl = Amdb_OpenTopic( pcl, szTopicName ) ) )
         {
         WORD wType;

         if( fMail )
            wType = FTYPE_MAIL;
         else if( fUsenet )
            wType = FTYPE_NEWS;
         else if( isupper( *szTopicName ) )
            wType = FTYPE_LOCAL;
         else
            wType = FTYPE_CIX;
         if( !( cixhdr.ptl = Amdb_CreateTopic( pcl, szTopicName, wType ) ) )
            return( AMDBERR_OUTOFMEMORY );
         }
      }
   if( fHasJoining )
      {
      Amdb_SetTopicFlags( cixhdr.ptl, TF_READONLY|TF_HASFILES|TF_TOPICFULL, FALSE );
      Amdb_SetTopicFlags( cixhdr.ptl, wFlags, TRUE );
      }
   if( fMail )
      Amdb_SetTopicType( cixhdr.ptl, FTYPE_MAIL );
   else if( fUsenet )
      Amdb_SetTopicType( cixhdr.ptl, FTYPE_NEWS );

   /* If the topic handle is different from the previous one, discard the previous
    * topic. Scratchpads tend to have topics ordered in groups, so this code is
    * unlikely to cause thrashing. On the plus side, it improves performance and
    * memory consumption when parsing large scratchpads or importing from a large
    * set of message bases.
    */
   if( ptlPrev && cixhdr.ptl != ptlPrev )
      {
      if( hwndTopic2 = Amdb_GetTopicSel( ptlPrev ) )
         SendMessage( hwndTopic2, WM_LOCKUPDATES, WIN_THREAD, (LPARAM)FALSE );
      Amuser_CallRegistered( AE_UNREADCHANGED, (LPARAM)ptlPrev, 0L );
      Amdb_DiscardTopic( ptlPrev );
      }
   ptlPrev = cixhdr.ptl;
   if( hwndTopic2 = Amdb_GetTopicSel( ptlPrev ) )
      SendMessage( hwndTopic2, WM_LOCKUPDATES, WIN_THREAD, (LPARAM)TRUE );

   /* If no body, add a blank line.
    */
   if( !fHasBody )
      AppendLineToMessage( "", TRUE );

   /* Create the new message.
    * CAUTION: On return from InternalCreateMsg, ptl is NOT guaranteed to be the topic to which
    * the message was added, as an addon trapping AE_ADDNEWMSG may alter the destination
    * topic, amongst other things. Always use TopicFromMsg() to get the true topic handle.
    * Be careful not to overwrite ptl which must be unchanged for InternalUnlockTopic.
    */
   Amdb_LockTopic( cixhdr.ptl );
   ptlReal = cixhdr.ptl;
   nMatch = 0;
   Amdb_GetCookie( cixhdr.ptl, MAIL_ADDRESS_COOKIE, szTmpName, szMailAddress, 64 );
   if( _stricmp( szTmpName, cixhdr.szAuthor ) == 0 )
      {
      char szAddress[ LEN_MAILADDR + 1 ];
      register int c;
      char * pszAddress;
      char szMyAddress[ LEN_MAILADDR ];
                  
      ParseToField( cixhdr.szTo, szMyAddress, LEN_MAILADDR );

      /* Extract one address
       */
      for( c = 0; szMyAddress[ c ] && szMyAddress[ c ] != ' '; ++c )
         if( c < LEN_MAILADDR )
            szAddress[ c ] = szMyAddress[ c ];
      szAddress[ c ] = '\0';
      if( NULL != ( pszAddress = strchr( szAddress, '@' ) ) )
         if( IsCompulinkDomain( pszAddress + 1 ) )
            *pszAddress = '\0';
      strcpy( cixhdr.szAuthor, szAddress );
      cixhdr.fPriority = TRUE;
      cixhdr.fOutboxMsg = TRUE;
      }
   if( cixhdr.fOutboxMsg )
      {
      strncpy( szTempAuthor, cixhdr.szFrom, LEN_INTERNETNAME );
      strncpy( szTempAuthor2, cixhdr.szAuthor, LEN_INTERNETNAME );
      strncpy( cixhdr.szAuthor, szTempAuthor, LEN_INTERNETNAME );
      }
   if( strlen( cixhdr.szAuthor ) == 0 )
      strcpy( cixhdr.szAuthor, szDefaultAuthor );
   if( !DontUseRules() )
      nMatch = MatchRule( &ptlReal, hpBuf, &cixhdr, TRUE );
   if( !( nMatch & FLT_MATCH_COPY ) && ptlReal != cixhdr.ptl )
      {
      Amdb_InternalUnlockTopic( cixhdr.ptl );
      cixhdr.ptl = ptlReal;
      Amdb_LockTopic( cixhdr.ptl );
      }

   if( cixhdr.fOutboxMsg )
      strncpy( cixhdr.szAuthor, szTempAuthor2, LEN_INTERNETNAME );

   if( nMatch & FLT_MATCH_DELETE )
      r = AMDBERR_NOERROR;
   else
      {
      /* Compute local flags.
       */
      char szSubject[ 80 ];
      if( Amdb_GetTopicType( cixhdr.ptl ) == FTYPE_MAIL )
      {
         Amdb_GetCookie( cixhdr.ptl, SUBJECT_IGNORE_TEXT, szSubject, "", 80 );
         if( *szSubject )
            cixhdr.fPossibleRefScanning = TRUE;
      }

      dwLclFlags = 0;
      if( ( nMatch & FLT_MATCH_PRIORITY ) || cixhdr.fPriority || fMarkMsgPriority )
         dwLclFlags |= MF_PRIORITY;
      if( cixhdr.fCoverNote && !cixhdr.fPriority )
         dwLclFlags |= MF_COVERNOTE;
      if( cixhdr.fOutboxMsg )
         dwLclFlags |= MF_OUTBOXMSG;
      if( cixhdr.fNotification && !cixhdr.fOutboxMsg )
         dwLclFlags |= MF_NOTIFICATION;

      /* If this is the cixnews conference, check for a Downtime
       * announcement in the title.
       */
      if( strcmp( szConfName, "cixnews" ) == 0 )
         ParseCixnewsAnnouncement( cixhdr.szTitle );
      if( *cixhdr.szReference )
         {
         DWORD dwPossibleComment;

         if( dwPossibleComment = Amdb_MessageIdToComment( cixhdr.ptl, cixhdr.szReference ) )
            cixhdr.dwComment = dwPossibleComment;
         }
      else if( cixhdr.fPossibleRefScanning && fThreadMailBySubject )
      {
         cixhdr.dwComment = Amdb_LookupSubjectReference( cixhdr.ptl, cixhdr.szTitle, szSubject );
      }
      r = Amdb_CreateMsg( cixhdr.ptl, &pth, cixhdr.dwMsg, cixhdr.dwComment, cixhdr.szTitle, cixhdr.szAuthor, 
                         cixhdr.szMessageId, &cixhdr.date, &cixhdr.time, hpBuf, cbsz, dwLclFlags );
      if( r == AMDBERR_NOERROR )
         {
         /* Add this message to the main index
          */
         ++cNew;
         if( nMatch & FLT_MATCH_COPY && ( ptlReal != cixhdr.ptl ) )
            {
            PTH FAR * lppth;
            /* Okay, message got moved so move the message after
             * we've applied all the above.
             */
            INITIALISE_PTR(lppth);
            if( fNewMemory( (PTH FAR *)&lppth, 2 * sizeof( PTH ) ) )
               {
               lppth[ 0 ] = pth;
               lppth[ 1 ] = NULL;
               DoCopyMsgRange( hwndFrame, lppth, 1, ptlReal, FALSE, FALSE, FALSE );
               }
            }
         if( ( nMatch & FLT_MATCH_PRIORITY ) || cixhdr.fPriority || fMarkMsgPriority )
            Amdb_MarkMsgPriority( pth, TRUE, FALSE );
         if( ( nMatch & FLT_MATCH_READ ) || !fMarkMsgUnread )
            Amdb_MarkMsgRead( pth, TRUE );
         if( ( nMatch & FLT_MATCH_IGNORE ) || fMarkMsgIgnore )
            Amdb_MarkMsgIgnore( pth, TRUE, FALSE );
         if( nMatch & FLT_MATCH_WATCH )
         {
         /* Only get article if DESTINATION topic is a newsgroup
          * and we've currently only got the header.
          */
         if( Amdb_GetTopicType( cixhdr.ptl ) == FTYPE_NEWS )
            Amdb_MarkMsgWatch( pth, TRUE, TRUE );
         }
         if( fUsenet && !fHasBody && ( nMatch & FLT_MATCH_GETARTICLE || Amdb_IsWatched( pth ) ) )
            Amdb_MarkMsgTagged( pth, TRUE );
         else
            Amdb_MarkMsgTagged( pth, FALSE );
         if( nMatch & FLT_MATCH_READLOCK )
            Amdb_MarkMsgReadLock( pth, TRUE );
         if( nMatch & FLT_MATCH_KEEP )
            Amdb_MarkMsgKeep( pth, TRUE );
         if( nMatch & FLT_MATCH_MARK )
            Amdb_MarkMsg( pth, TRUE );
         if( fMarkMsgMarked )
            Amdb_MarkMsg( pth, TRUE );
         if( fMarkMsgLocked )
            Amdb_MarkMsgReadLock( pth, TRUE );
         if( fMarkMsgKeep )
            Amdb_MarkMsgKeep( pth, TRUE );
         if( fMarkMsgDelete )
            Amdb_MarkMsgDelete( pth, TRUE );
         if( fMarkMsgTagged )
            Amdb_MarkMsgTagged( pth, TRUE );
         if( fMarkMsgWithdraw )
            Amdb_MarkMsgWithdrawn( pth, TRUE );
         if( fMarkMsgWatched )
            Amdb_MarkMsgWatch( pth, TRUE, FALSE );
         if( cixhdr.fCoverNote )
            Amdb_GetSetCoverNoteBit( pth, BSB_SETBIT );
         if( cixhdr.fOutboxMsg )
            Amdb_SetOutboxMsgBit( pth, TRUE );
         if( fUsenet )
         {
            if( Amdb_GetTopicFlags( cixhdr.ptl, TF_READFULL ) )
               fHasBody = TRUE;
            Amdb_MarkHdrOnly( pth, !fHasBody );
         }
         if( cixhdr.fNotification && !cixhdr.fOutboxMsg )
            Amdb_GetSetNotificationBit( pth, BSB_SETBIT );
         Amdb_MarkMsgUnavailable( pth, FALSE );
         fHasBody = FALSE;
         }
      else if( r == AMDBERR_IGNOREDERROR )
         r = AMDBERR_NOERROR;
      if( fMail )
         fNewMail = TRUE;
      }
   Amdb_InternalUnlockTopic( cixhdr.ptl );
   return( r );
}

/* Parse a possible downtime announcement in the CIX cixnews
 * informations conference.
 */
void FASTCALL ParseCixnewsAnnouncement( char * psz )
{
   char szMsgType[ 20 ];
   register int c;

   for( c = 0; c < 19 && *psz != ':'; ++c )
      szMsgType[ c ] = *psz++;
   szMsgType[ c ] = '\0';
   if( *psz == ':' )
      ++psz;
   while( *psz == ' ' )
      ++psz;
   if( strcmp( szMsgType, "DOWNTIME" ) == 0 )
      {
      char szTime[ 40 ];
      char szDate[ 40 ];
      char * pszTime;
      AM_DATE date;
      AM_TIME startTime;
      AM_TIME endTime;

      /* Extract the downtime date
       */
      date.iDay = 0;
      date.iMonth = 0;
      date.iYear = 0;
      while( isdigit( *psz ) )
         date.iDay = date.iDay * 10 + ( *psz++ - '0' );
      if( *psz == '/' )
         ++psz;
      while( isdigit( *psz ) )
         date.iMonth = date.iMonth * 10 + ( *psz++ - '0' );
      if( *psz == '/' )
         ++psz;
      while( isdigit( *psz ) )
         date.iYear = date.iYear * 10 + ( *psz++ - '0' );
      date.iYear = FixupYear( date.iYear );
      while( *psz == ' ' )
         ++psz;

      /* Extract the start of the downtime
       */
      if( strcmp( psz, "CANCELLED" ) == 0 )
         pszTime = NULL;
      else
         {
         startTime.iSecond = 0;
         startTime.iMinute = 0;
         startTime.iHour = 0;
         while( isdigit( *psz ) )
            startTime.iHour = startTime.iHour * 10 + ( *psz++ - '0' );
         if( *psz == ':' )
            ++psz;
         while( isdigit( *psz ) )
            startTime.iMinute = startTime.iMinute * 10 + ( *psz++ - '0' );
         while( *psz == ' ' )
            ++psz;
         if( *psz == '-' )
            ++psz;
         while( *psz == ' ' )
            ++psz;

         /* Extract the end of the downtime
          */
         endTime.iSecond = 0;
         endTime.iMinute = 0;
         endTime.iHour = 0;
         while( isdigit( *psz ) )
            endTime.iHour = endTime.iHour * 10 + ( *psz++ - '0' );
         if( *psz == ':' )
            ++psz;
         while( isdigit( *psz ) )
            endTime.iMinute = endTime.iMinute * 10 + ( *psz++ - '0' );
         wsprintf( pszTime = szTime, "%d:%2.2d %d:%2.2d", startTime.iHour, startTime.iMinute, endTime.iHour, endTime.iMinute );
         }

      /* Now write this information to the configuration file.
       */
      wsprintf( szDate, "%d/%d/%d", date.iDay, date.iMonth, date.iYear );
      Amuser_WritePPString( szDowntime, szDate, pszTime );
      }
}

/* This function converts a year from a 2-digit to a 4-digit
 * value. (ie. 80 to 1980, 99 to 1999, 04 to 2004)
 * Amended by pw 6/5/1999 as gmtime/localtime etc return
 * ( year - 1900 ) i.e. '101' for 2001.
 */
WORD FASTCALL FixupYear( WORD iYear )
{
   if( iYear < 100 )
      {
      if( iYear < 80 )
         iYear += 2000;
      else
         iYear += 1900;
      }
   else if( iYear < 1000 )
      iYear += 1900;
   return( iYear );
}

/* This function parses a CIX conference and topic name
 * from the message header.
 */
char * FASTCALL ParseConfTopic( char * psz )
{
   register int c;

   for( c = 0; *psz && *psz != '/' && c < LEN_CONFNAME+1; ++c ) //FS#154 2054
      szConfName[ c ] = *psz++;
   szConfName[ c ] = '\0';
   while( *psz && *psz != '/' )
      ++psz;
   if( *psz == '\0' )
      return( psz );
   ++psz;
   for( c = 0; *psz && *psz != ' ' && c < LEN_TOPICNAME; ++c )
      szTopicName[ c ] = *psz++;
   szTopicName[ c ] = '\0';
   if( strlen( szTopicName ) == 0 )
      strcpy( szTopicName, "CIXBug" );
   while( *psz == ' ' )
      ++psz;
   return( psz );
}

/* This function parses a CIX message number.
 */
char * FASTCALL ParseCIXMessageNumber( char * psz )
{
   if( *psz == '#' ) ++psz;
   cixhdr.dwMsg = 0;
   while( isdigit( *psz ) )
      cixhdr.dwMsg = cixhdr.dwMsg * 10 + ( *psz++ - '0' );
   if( *psz == ',' ) ++psz;
   while( *psz == ' ' )
      ++psz;
   return( psz );
}

/* This function parses a CIX author name.
 */
char * FASTCALL ParseCIXAuthor( char * psz )
{
   register int c;

   while( *psz && *psz != ' ' )
      ++psz;
   if( *psz == '\0' )
      return( psz );
   while( *psz == ' ' )
      ++psz;
   for( c = 0; *psz && *psz != ',' && c < LEN_INTERNETNAME; ++c )
      cixhdr.szAuthor[ c ] = *psz++;
   cixhdr.szAuthor[ c ] = '\0';
   strcpy( cixhdr.szFrom, cixhdr.szAuthor );
   cixhdr.fPriority = IsLoginAuthor( cixhdr.szAuthor );
   if( *psz == '\0' )
      return( psz );
   ++psz;
   while( *psz == ' ' )
      ++psz;
   return( psz );
}

/* This function parses a CIX message length.
 */
char * FASTCALL ParseCIXMessageLength( char * psz )
{
   cbMsg = 0;
   while( isdigit( *psz ) )
      cbMsg = cbMsg * 10 + ( *psz++ - '0' );
   while( *psz && *psz != ',' )
      ++psz;
   if( *psz == '\0' )
      return( psz );
   ++psz;
   while( *psz == ' ' )
      ++psz;
   return( psz );
}

/* This function parses the Newsgroup line.
 */
char * FASTCALL ParseMailNewsgroup( char * psz )
{
   register int c;

   /* Get the newsgroup name.
    */
   while( *psz == ' ' )
      ++psz;
   for( c = 0; *psz && *psz != ',' && c < LEN_TOPICNAME; ++c )
      szTopicName[ c ] = *psz++;
   szTopicName[ c ] = '\0';

   /* Get the topic handle, and create it if it does not exist.
    */
   if( !fBuilding )
      if( NULL == ( cixhdr.ptl = Amdb_OpenTopic( NULL, szTopicName ) ) )
         {
         PCL pcl;

         pcl = GetNewsFolder();
         cixhdr.ptl = Amdb_CreateTopic( pcl, szTopicName, FTYPE_NEWS );
         }
   return( psz );
}

/* This function parses a CIX date format.
 */
char * FASTCALL ParseCIXDate( char * npsz, char * npDate )
{
   while( *npsz == ' ' )
      ++npsz;
   ParseCIXDateTime( npsz );
   Amdate_UnpackDate( Amdate_PackDate( &cixhdr.date ), &cixhdr.date );
   wsprintf( npDate, GS(IDS_STR348), Amdate_FormatLongDate( &cixhdr.date, NULL ), Amdate_FormatTime( &cixhdr.time, NULL ) );
   return( npsz );
}

/* This function parses a CIX time format.
 */
char * FASTCALL ParseCIXTime( char * npsz, AM_DATE FAR * lpDate, AM_TIME FAR * lpTime )
{
   while( *npsz == ' ' )
      ++npsz;
   npsz = ParseCIXDateTime( npsz );
   *lpDate = cixhdr.date;
   *lpTime = cixhdr.time;
   return( npsz );
}

/* This function parses a CIX conference style date and time.
 */
char * FASTCALL ParseCIXDateTime( char * psz )
{
   char szMonth[ 4 ];
   int iCentury;
   int iZeller;
   int iYear;
   register int c;

   cixhdr.time.iHour = 0;
   cixhdr.time.iMinute = 0;
   cixhdr.time.iSecond = 0;
   cixhdr.date.iDay = 0;
   cixhdr.date.iMonth = 0;
   cixhdr.date.iYear = 0;
   for( c = 0; *psz && *psz != ' '; )
      {
      if( c < 3 )
         szMonth[ c++ ] = *psz;
      ++psz;
      }
   if( *psz == '\0' )
      return( psz );
   szMonth[ c ] = '\0';
   while( *psz == ' ' )
      ++psz;
   for( c = 0; c < 12; ++c )
      if( strcmp( szMonth, pszCixMonths[ c ] ) == 0 ) {
         cixhdr.date.iMonth = c + 1;
         break;
         }
   while( isdigit( *psz ) )
      cixhdr.date.iDay = cixhdr.date.iDay * 10 + ( *psz++ - '0' );
   while( *psz == ' ' )
      ++psz;
   while( isdigit( *psz ) )
      cixhdr.time.iHour = cixhdr.time.iHour * 10 + ( *psz++ - '0' );
   if( *psz == ':' ) ++psz;
   while( isdigit( *psz ) )
      cixhdr.time.iMinute = cixhdr.time.iMinute * 10 + ( *psz++ - '0' );
   while( *psz == ' ' )
      ++psz;
   while( isdigit( *psz ) )
      cixhdr.date.iYear = cixhdr.date.iYear * 10 + ( *psz++ - '0' );
   cixhdr.date.iYear = FixupYear( cixhdr.date.iYear );

   /* Compute the day of the week.
    */
   iCentury = cixhdr.date.iYear / 100;
   iYear = cixhdr.date.iYear % 100;
   iZeller = cixhdr.date.iDay + ( ( ( cixhdr.date.iMonth + 1 ) * 26 ) / 10 ) + cixhdr.date.iYear + ( cixhdr.date.iYear / 4 );
   iZeller = iZeller + ( iCentury / 4 ) + 5 * iCentury;
   cixhdr.date.iDayOfWeek = ( iZeller - 1 ) % 7;
   return( psz );
}

/* This function parses a CIX time format including seconds 
 * (from run datetime).
 */
char * FASTCALL ParseExactCIXTime( char * npsz, AM_DATE FAR * lpDate, AM_TIME FAR * lpTime )
{
   while( *npsz == ' ' )
      ++npsz;
   npsz = ParseExactCIXDateTime( npsz );
   *lpDate = cixhdr.date;
   *lpTime = cixhdr.time;
   return( npsz );
}

/* This function parses a CIX conference style date and time.
 */
char * FASTCALL ParseExactCIXDateTime( char * psz )
{
   char szMonth[ 4 ];
   int iCentury;
   int iZeller;
   int iYear;
   register int c;

   cixhdr.time.iHour = 0;
   cixhdr.time.iMinute = 0;
   cixhdr.time.iSecond = 0;
   cixhdr.date.iDay = 0;
   cixhdr.date.iMonth = 0;
   cixhdr.date.iYear = 0;
   psz++;
   psz++;
   psz++;
   psz++;
   for( c = 0; *psz && *psz != ' '; )
      {
      if( c < 3 )
         szMonth[ c++ ] = *psz;
      ++psz;
      }
   if( *psz == '\0' )
      return( psz );
   szMonth[ c ] = '\0';
   while( *psz == ' ' )
      ++psz;
   for( c = 0; c < 12; ++c )
      if( strcmp( szMonth, pszCixMonths[ c ] ) == 0 ) {
         cixhdr.date.iMonth = c + 1;
         break;
         }
   while( isdigit( *psz ) )
      cixhdr.date.iDay = cixhdr.date.iDay * 10 + ( *psz++ - '0' );
   while( *psz == ' ' )
      ++psz;
   while( isdigit( *psz ) )
      cixhdr.time.iHour = cixhdr.time.iHour * 10 + ( *psz++ - '0' );
   if( *psz == ':' ) ++psz;
   while( isdigit( *psz ) )
      cixhdr.time.iMinute = cixhdr.time.iMinute * 10 + ( *psz++ - '0' );
   if( *psz == ':' ) ++psz;
   while( isdigit( *psz ) )
      cixhdr.time.iSecond = cixhdr.time.iSecond * 10 + ( *psz++ - '0' );
   while( *psz == ' ' )
      ++psz;
   psz++;
   psz++;
   psz++;
   psz++;
   while( isdigit( *psz ) )
      cixhdr.date.iYear = cixhdr.date.iYear * 10 + ( *psz++ - '0' );
// cixhdr.date.iYear = FixupYear( cixhdr.date.iYear );

   /* Compute the day of the week.
    */
   iCentury = cixhdr.date.iYear / 100;
   iYear = cixhdr.date.iYear % 100;
   iZeller = cixhdr.date.iDay + ( ( ( cixhdr.date.iMonth + 1 ) * 26 ) / 10 ) + cixhdr.date.iYear + ( cixhdr.date.iYear / 4 );
   iZeller = iZeller + ( iCentury / 4 ) + 5 * iCentury;
   cixhdr.date.iDayOfWeek = ( iZeller - 1 ) % 7;
   return( psz );
}

/* This function parses the Article command.
 */
char * FASTCALL ParseMailArticle( char * psz )
{
   register int c;

   while( *psz == ' ' )
      ++psz;
   cixhdr.dwMsg = 0;
   while( isdigit( *psz ) )
      cixhdr.dwMsg = cixhdr.dwMsg * 10 + ( *psz++ - '0' );
   if( strncmp( psz, scp_str7, strlen( scp_str7 ) ) == 0 )
      {
      PCL pcl;

      psz += strlen( scp_str7 );
      for( c = 0; *psz && *psz != ' ' && c < LEN_TOPICNAME; ++c )
         szTopicName[ c ] = *psz++;
      szTopicName[ c ] = '\0';

      /* Get the topic handle, and create it if it does not exist.
       */
      pcl = GetNewsFolder();
      if( !fBuilding )
         if( NULL == ( cixhdr.ptl = Amdb_OpenTopic( pcl, szTopicName ) ) )
            cixhdr.ptl = Amdb_CreateTopic( pcl, szTopicName, FTYPE_NEWS );
      fArticleSeen = TRUE;
      }
   return( psz );
}

/* This function parses the X-Reply-Folder field.
 */
char * FASTCALL ParseReplyFolder( char * psz )
{
   register int c;

   while( *psz == ' ' )
      ++psz;
   for( c = 0; *psz && *psz != ' ' && c < LEN_TOPICNAME; ++c )
      szTopicName[ c ] = *psz++;
   szTopicName[ c ] = '\0';
   return( psz );
}

/* Thus function parses a CIX conference title message
 */
char * FASTCALL ParseConferenceTitle( char * psz )
{
   wFlags = 0;
   while( *psz == ' ' )
      ++psz;
   while( *psz && *psz != ' ' )
      ++psz;
   while( *psz == ' ' )
      ++psz;
   if( *psz == '(' ) {
      ++psz;
      if( *psz++ == 'F' )
         wFlags |= TF_HASFILES;
      ++psz;
      }
   while( *psz == ' ' )
      ++psz;
   while( *psz && isdigit( *psz ) )
      ++psz;      /* Skip count of new messages */
   while( *psz && *psz != '.' )
      ++psz;
   if( *psz == '.' )
      ++psz;
   if( strncmp( psz, scp_str10, strlen( scp_str10 ) ) == 0 )
      {
      wFlags |= TF_READONLY;
      psz += strlen( scp_str10 ) + 1;
      }
   if( strncmp( psz, scp_str13, strlen( scp_str13 ) ) == 0 )
      wFlags |= TF_TOPICFULL;
   return( psz );
}

/* Parse one line of a CIX Usenet header
 */
char * FASTCALL ParseCIXUsenetHeader( char * psz )
{
   char szFullName[ LEN_INTERNETNAME + 1 ];
   char szAuthor[ LEN_INTERNETNAME+1 ];
   char szDateStr[ 80 ];
   DWORD dwMsgSize;
   register int c;

   /* Extract topic name
    */
   for( c = 0; *psz && *psz != '|' && c < LEN_TOPICNAME; ++c )
      szTopicName[ c ] = *psz++;
   szTopicName[ c ] = '\0';
   if( *psz == '|' )
      ++psz;

   /* Extract message number.
    */
   cixhdr.dwMsg = 0;
   while( isdigit( *psz ) )
      cixhdr.dwMsg = cixhdr.dwMsg * 10 + ( *psz++ - '0' );
   if( *psz == '|' )
      ++psz;

   /* Extract the author
    */
   for( c = 0; *psz && *psz != '|' && c < LEN_INTERNETNAME; ++c )
      szAuthor[ c ] = *psz++;
   szAuthor[ c ] = '\0';
   ParseFromField( szAuthor, cixhdr.szAuthor, szFullName );
   if( *cixhdr.szAuthor == '\0' )
      strcpy( cixhdr.szAuthor, szFullName );
   strcpy( cixhdr.szFrom, cixhdr.szAuthor );
   if( *psz == '|' )
      ++psz;

   /* Extract the subject line
    */
   for( c = 0; *psz && *psz != '|' && c < LEN_TITLE; ++c )
      cixhdr.szTitle[ c ] = *psz++;
   cixhdr.szTitle[ c ] = '\0';
   if( *psz == '|' )
      ++psz;

   /* This is a usenet message with no body.
    */
   fUsenet = TRUE;
   fHasBody = FALSE;

   /* Fake the date/time because Usenet doesn't do it for
    * us.
    */
   Amdate_GetCurrentDate( &cixhdr.date );
   Amdate_GetCurrentTime( &cixhdr.time );
   wsprintf( szDateStr, "%u %s %u %u:%u:%u",
             cixhdr.date.iDay, 
             pszCixMonths[ cixhdr.date.iMonth - 1 ],
             cixhdr.date.iYear,
             cixhdr.time.iHour,
             cixhdr.time.iMinute );

   /* Create the header.
    */
   dwMsgSize = 0L;
   for( c = 0; c < 2; ++c )
      {
      cbMsg = cbTotal = cbsz = 0;
      if( c > 0 )
         {
         wsprintf( lpTmpBuf, "#! rnews %lu", dwMsgSize );
         AppendLineToMessage( lpTmpBuf, TRUE );
         }
      wsprintf( lpTmpBuf, "Article: %lu of %s", cixhdr.dwMsg, szTopicName );
      AppendLineToMessage( lpTmpBuf, TRUE );
      wsprintf( lpTmpBuf, "From: %s", cixhdr.szAuthor );
      AppendLineToMessage( lpTmpBuf, TRUE );
      wsprintf( lpTmpBuf, "Newsgroups: %s", szTopicName );
      AppendLineToMessage( lpTmpBuf, TRUE );
      wsprintf( lpTmpBuf, "Subject: %s", cixhdr.szTitle );
      AppendLineToMessage( lpTmpBuf, TRUE );
      wsprintf( lpTmpBuf, "Date: %s", szDateStr );
      dwMsgSize = cbsz - 5;
      }

   /* Okay, now put this message.
    */
   cixhdr.fPossibleRefScanning = TRUE;
   cixhdr.dwComment = 0L;
   PutCurrentMessage();
   return( psz );
}

/* This function looks for and parses the X-Olr field which specifies
 * that the CIX mail message has an attachment.
 */
BOOL FASTCALL CheckForXOLRField( char * psz )
{
   char szType[ LEN_RFCTYPE ];
   BOOL fFound;

   fFound = FALSE;
   while( *psz == ' ' || *psz == '\t' )
      ++psz;
   if( *psz != '\0' )
      {
      register int c;

      for( c = 0; c < LEN_RFCTYPE - 1 && *psz && *psz != ':'; ++c )
         szType[ c ] = *psz++;
      szType[ c ] = '\0';
      if( *psz == ':' ) ++psz;
      while( *psz == ' ' )
         ++psz;
      if( _strcmpi( szType, scp_str33 ) == 0 )
         {
         psz = ParseXOlr( psz, &cixhdr );
         fFound = TRUE;
         }
      fCheckForXOLRField = FALSE;
      }
   return( fFound );
}

/* This function parses the To field of an e-mail message.
 *
 * This code gets round a long-standing criticism with Ameol's e-mail
 * system. Namely that all e-mail sent from Ameol are shown in the
 * Message window with the name of the sender, not the recipient. So
 * what we do is, when parsing an e-mail header, check whether the
 * sender's name matches the current login user, and, if so, replace
 * it with the name in the To field instead.
 *
 * However, for clarity, we also need to document the fact in the
 * Message window. So we set the fOutboxMsg flag which forces a call to
 * SetOutboxMsgBit() in the database, to set the MF_OUTBOXMSG flag. This
 * forces the name to be prefixed with 'To:' in the message window.
 */
char * FASTCALL ParseCixMailToField( char * psz )
{
   char * szTemp;
   char * pszTo;
   int cbTo;

   /* Extract the entire To field to the szTo field in the
    * structure. We need it for the rules later.
    */
   cbTo = strlen( cixhdr.szTo );
   pszTo = cixhdr.szTo + cbTo;
   if( cbTo > 0 )
      *pszTo++ = ' ';
   psz = ParseToField( psz, pszTo, LEN_TOCCLIST - cbTo );

   /* Extract each address to szAddress and strip off the
    * compulink domain.
    */
   while( *pszTo )
      {
      char szAddress[ ( 5 * LEN_INTERNETNAME ) + 1 ];
      char * pszAddress;
      register int c;

      /* Extract one address.
       */
      for( c = 0; pszTo[ c ] && pszTo[ c ] != ' '; ++c )
         if( c < ( 5 * LEN_INTERNETNAME ) )
            szAddress[ c ] = pszTo[ c ];
      szAddress[ c ] = '\0';

      /* Skip spaces, commas and whatever.
       */
      pszTo += c;
      while( *pszTo == ' ' || *pszTo == ',' )
         ++pszTo;

      /* Look for the domain and check whether it is within Compulink. If
       * so, strip off the domain (for clarity).
       */
      if( NULL != ( pszAddress = strchr( szAddress, '@' ) ) )
         if( IsCompulinkDomain( pszAddress + 1 ) && IsCompulinkDomain( szDefMailDomain ) )
            *pszAddress = '\0';

      /* !!SM!! 2.55
         As we no longer strip the @cix from the author, so that rules can match @cix.co.uk
         we have to strip it here before doing the check, so that it works for conferencing only
         users who can't set their internet domain.
       */

   
      INITIALISE_PTR(szTemp);
      if( fNewMemory( &szTemp, LEN_INTERNETNAME ) )
      {
         char * pszAddress;
         
         strcpy(szTemp, cixhdr.szAuthor);

         if( NULL != ( pszAddress = strchr( szTemp, '@' ) ) )
            if( IsCompulinkDomain( pszAddress + 1 ) && IsCompulinkDomain( szDefMailDomain ) )
               *pszAddress = '\0';

         if( _stricmp( szTemp, szCIXNickname ) == 0 ||
             ( fUseInternet && _stricmp( szTemp, szMailAddress ) == 0 ) )
         {
            cixhdr.fPriority = TRUE;
            strncpy( cixhdr.szAuthor, szAddress, LEN_INTERNETNAME );
            cixhdr.fOutboxMsg = TRUE;
         }
      }
      /* Is this the sender? If so, set the sender to be the author
       * of this message.
       */
      /* // !!SM!! 2.55
      if( _stricmp( cixhdr.szAuthor, szCIXNickname ) == 0 ||                  
          ( fUseInternet && _stricmp( cixhdr.szAuthor, szMailAddress ) == 0 ) )     
      {
         cixhdr.fPriority = TRUE;
         strncpy( cixhdr.szAuthor, szAddress, LEN_INTERNETNAME );          
         cixhdr.fOutboxMsg = TRUE;
      }
      */
   }
   return( psz );
}

/* This function parses the CC field of an e-mail message.
 */
char * FASTCALL ParseCixMailCCField( char * psz )
{
   char * pszCc;
   int cbCc;

   cbCc = strlen( cixhdr.szCC );
   pszCc = cixhdr.szCC + cbCc;
   if( cbCc > 0 )
      *pszCc++ = ' ';
   psz = ParseToField( psz, pszCc, LEN_TOCCLIST - cbCc );
// psz = ParseToField( psz, cixhdr.szCC, LEN_TOCCLIST );
   return( psz );
}

/* This function parses a CIX mail message. We deliberately strip
 * off the @cix.compulink.co.uk from the sender name.
 */
char * FASTCALL ParseCIXMailAuthor( char * psz, BOOL fStripCompulink )
{
   char * pszAddress;

   ParseMailAuthor( psz, &cixhdr );
   if( NULL != ( pszAddress = strchr( cixhdr.szAuthor, '@' ) ) )
      if( IsCompulinkDomain( pszAddress + 1 ) && fStripCompulink && IsCompulinkDomain( szDefMailDomain ) )
         *pszAddress = '\0';
   if( NULL != ( pszAddress = strchr( cixhdr.szFrom, '@' ) ) )
      if( IsCompulinkDomain( pszAddress + 1 ) && fStripCompulink && IsCompulinkDomain( szDefMailDomain ) )
         *pszAddress = '\0';
   return( psz );
}

/* This function parses a CIX compact header.
 */
BOOL FASTCALL ParseCompactHeader( char * psz )
{
   char szMonth[ 4 ];
   register int c;

   psz = ParseConfTopic( psz );
   psz = ParseCIXMessageNumber( psz );

   /* Parse the author's name. The name is terminated by a '(' which
    * delimits the message size.
    */
   while( *psz == ' ' )
      ++psz;
   for( c = 0; *psz && *psz != '(' && c < LEN_INTERNETNAME; ++c )
      cixhdr.szAuthor[ c ] = *psz++;
   cixhdr.szAuthor[ c ] = '\0';
   strcpy( cixhdr.szFrom, cixhdr.szAuthor );
   cixhdr.fPriority = IsLoginAuthor( cixhdr.szAuthor );
   while( *psz && *psz != '(' )
      ++psz;
   if( *psz ==  '\0' )
      return( FALSE );

   /* Parse the message size. The message size is wrapped in parenthesis.
    */
   ++psz;
   cbMsg = 0;
   while( isdigit( *psz ) )
      cbMsg = cbMsg * 10 + ( *psz++ - '0' );
   ++psz;

   /* Parse the date and time. This is a more compact method than the
    * terse header.
    */
   cixhdr.time.iHour = 0;
   cixhdr.time.iMinute = 0;
   cixhdr.time.iSecond = 0;
   cixhdr.date.iDay = 0;
   cixhdr.date.iMonth = 0;
   cixhdr.date.iYear = 0;
   while( isdigit( *psz ) )
      cixhdr.date.iDay = cixhdr.date.iDay * 10 + ( *psz++ - '0' );
   for( c = 0; !isdigit( *psz ); )
      {
      if( c < 3 )
         szMonth[ c++ ] = *psz;
      ++psz;
      }
   szMonth[ c ] = '\0';
   for( c = 0; c < 12; ++c )
      if( _strcmpi( szMonth, pszCixMonths[ c ] ) == 0 ) {
         cixhdr.date.iMonth = c + 1;
         break;
         }
   while( isdigit( *psz ) )
      cixhdr.date.iYear = cixhdr.date.iYear * 10 + ( *psz++ - '0' );
   cixhdr.date.iYear = FixupYear( cixhdr.date.iYear );
   while( *psz == ' ' )
      ++psz;
   while( isdigit( *psz ) )
      cixhdr.time.iHour = cixhdr.time.iHour * 10 + ( *psz++ - '0' );
   if( *psz == ':' ) ++psz;
   while( isdigit( *psz ) )
      cixhdr.time.iMinute = cixhdr.time.iMinute * 10 + ( *psz++ - '0' );
   while( *psz == ' ' )
      ++psz;
   if( *psz == '\0' )
      return( TRUE );
   if( *psz != 'c' )
      return( TRUE );
   ++psz;
   cixhdr.dwComment = 0;
   while( isdigit( *psz ) )
      cixhdr.dwComment = cixhdr.dwComment * 10 + ( *psz++ - '0' );
   return( FALSE );
}

/* This is the Parse Init dialog handler
 */
BOOL EXPORT CALLBACK ReadScratchpad( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = ReadScratchpad_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the ReadScratchpad
 * dialog.
 */
LRESULT FASTCALL ReadScratchpad_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, ReadScratchpad_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, ReadScratchpad_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsREADSCRATCHPAD );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL ReadScratchpad_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   register int c;
   HWND hwndCombo;

   /* Set the input field.
    */
   hwndCombo = GetDlgItem( hwnd, IDD_EDIT );
   if( lParam )
      ComboBox_SetText( hwndCombo, (LPSTR)lParam );
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* Fill the combo box with previous scratchpads.
    */
   for( c = 0; c < 10; ++c )
      {
      char szScratch[ _MAX_DIR ];
      char sz[ 30 ];

      wsprintf( sz, "File%u", c + 1 );
      if( !Amuser_GetPPString( szScratchpads, sz, "", szScratch, sizeof( szScratch ) ) )
         break;
      ComboBox_InsertString( hwndCombo, -1, szScratch );
      }

   /* Disable Start button unless there's a filename
    * present.
    */
   EnableControl( hwnd, IDOK, ComboBox_GetTextLength( hwndCombo ) > 0 );

   /* Set the default options.
    */
   CheckDlgButton( hwnd, IDD_MARKREAD, MarkAllRead() );
   CheckDlgButton( hwnd, IDD_ARCHIVE, ArchiveAfterImporting() );

   /* Set the picture buttons.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_BROWSE ), hInst, IDB_FILEBUTTON );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL ReadScratchpad_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EDIT:
         if( codeNotify == CBN_EDITCHANGE )
            EnableControl( hwnd, IDOK, ComboBox_GetTextLength( hwndCtl ) > 0 );
         else if( codeNotify == CBN_SELCHANGE )
            EnableControl( hwnd, IDOK, ComboBox_GetCurSel( hwndCtl ) != CB_ERR );
         break;

      case IDD_BROWSE: {
         static char strFilters[] = "Scratchpads (*.scr;*.dat;*.txt)\0*.scr;*.dat;*.txt\0Archives (scratchp.*)\0scratchp.*\0All Files (*.*)\0*.*\0\0";
         char sz[ _MAX_PATH ];
         OPENFILENAME ofn;
         HWND hwndCombo;

         hwndCombo = GetDlgItem( hwnd, IDD_EDIT );
         ComboBox_GetText( hwndCombo, sz, sizeof( sz ) );
         if( ( strlen( sz) > 1 ) && ( sz[ ( strlen( sz ) - 1 ) ] == '\\' ) && ( sz[ ( strlen( sz ) - 2 ) ] != ':' ) )
            sz[ ( strlen( sz ) - 1 ) ] = '\0';
         ofn.lStructSize = sizeof( OPENFILENAME );
         ofn.hwndOwner = hwnd;
         ofn.hInstance = NULL;
         ofn.lpstrFilter = strFilters;
         ofn.lpstrCustomFilter = NULL;
         ofn.nMaxCustFilter = 0;
         ofn.nFilterIndex = 1;
         ofn.lpstrFile = sz;
         ofn.nMaxFile = sizeof( sz );
         ofn.lpstrFileTitle = NULL;
         ofn.nMaxFileTitle = 0;
         ofn.lpstrInitialDir = "";
         ofn.lpstrTitle = "Scratchpad File";
         ofn.Flags = OFN_NOCHANGEDIR|OFN_OVERWRITEPROMPT|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST;
         ofn.nFileOffset = 0;
         ofn.nFileExtension = 0;
         ofn.lpstrDefExt = NULL;
         ofn.lCustData = 0;
         ofn.lpfnHook = NULL;
         ofn.lpTemplateName = 0;
         if( GetOpenFileName( &ofn ) )
            {
            ComboBox_SetText( hwndCombo, sz );
            EnableControl( hwnd, IDOK, *sz != '\0' );
            }
         SetFocus( hwndCombo );
         ComboBox_SetEditSel( hwndCombo, 32767, 32767 );
         break;
         }

      case IDOK: {
         LPSTR lpFile;
         HWND hwndCombo;
         register int n;
         register int c;

         /* Get the pointer to the target file buffer.
          * Default to the szScratchFile[] global array if NULL.
          */
         lpFile = (LPSTR)GetWindowLong( hwnd, DWL_USER );
         if( NULL == lpFile )
            lpFile = szScratchFile;

         /* First get and save the selected scratchpad filename.
          */
         hwndCombo = GetDlgItem( hwnd, IDD_EDIT );
         ComboBox_GetText( hwndCombo, lpFile, _MAX_PATH );

         /* Now save the entire list.
          */
         Amuser_WritePPString( szScratchpads, NULL, NULL );
         Amuser_WritePPString( szScratchpads, "File1", lpFile );
         for( n = 0, c = 2; c <= 10; ++n )
            {
            char szScratchpad[ _MAX_DIR ];
            if( CB_ERR == ComboBox_GetLBText( hwndCombo, n, szScratchpad ) )
               break;
            if( strcmp( lpFile, szScratchpad ) )
               {
               char sz[ 10 ];

               wsprintf( sz, "File%u", c++ );
               Amuser_WritePPString( szScratchpads, sz, szScratchpad );
               }
            }

         /* Get and save the options.
          */
         wGlbParseFlags &= ~(PARSEFLG_MARKREAD|PARSEFLG_ARCHIVE);
         if( IsDlgButtonChecked( hwnd, IDD_MARKREAD ) )
            wGlbParseFlags |= PARSEFLG_MARKREAD;
         if( IsDlgButtonChecked( hwnd, IDD_ARCHIVE ) )
            wGlbParseFlags |= PARSEFLG_ARCHIVE;
         EndDialog( hwnd, TRUE );
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}

/* This functions will return TRUE if the File List header is O.K.
 * An O.K. file list header will contain exactly the right number of
 * tokens (NUM_TOKENS_IN_FLIST_HEAD). Tokens are delimited by ' ' and '\t'.
 *
 * An example of an O.K. header would be
 *
 *          AF/AFFH   Apr   4 12:32   96
 *
 * A corrupt version would be
 *
 *          Apr   4 12:32   96
 *
 * YH 04/04/96
 */
BOOL FASTCALL FileListHeaderOK( LPCSTR lpszFileListHeader )
{
   int nNumTokens;
   BOOL fIsSpace;
   BOOL fTokenCounted;

   ASSERT( lpszFileListHeader );

   /* Skip any space characters that might be present
    * before the first token.
    */
   nNumTokens = 0;
   fIsSpace = TRUE;
   while( *lpszFileListHeader && fIsSpace )
      {
      if( *lpszFileListHeader == ' ' || *lpszFileListHeader == '\t' )
         lpszFileListHeader++;
      else
         fIsSpace = FALSE;
      }

   /* Go through the header counting the number of tokens there are
    */
   fTokenCounted = FALSE;
   while( *lpszFileListHeader )
      {
      /* This bit skips through the current token until
       * it finds a space character
       */
      while( *lpszFileListHeader && !fIsSpace )
         {
         if( *lpszFileListHeader == ' ' || *lpszFileListHeader == '\t' )
            {
            fIsSpace = TRUE;
            fTokenCounted = FALSE;
            }
         else
            {
            if( !fTokenCounted )
               {
               fTokenCounted = TRUE;
               nNumTokens++;
               }
            lpszFileListHeader++;
            }
         }

      /* This between skips over the spaces between the tokens
       */
      while( *lpszFileListHeader && fIsSpace )
         {
         if( *lpszFileListHeader == ' ' || *lpszFileListHeader == '\t' )
            lpszFileListHeader++;
         else
            fIsSpace = FALSE;
         }
      }
   return( nNumTokens == NUM_TOKENS_IN_FLIST_HEAD );
}
