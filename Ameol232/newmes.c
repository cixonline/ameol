/* NEWMES.C - Creates the CIX script
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
#include "resource.h"
#include <string.h>
#include "intcomms.h"
#include "amlib.h"
#include "amaddr.h"
#include <stdarg.h>
#include <ctype.h>
#include "cix.h"
#include "shared.h"
#include "lbf.h"
#include "nfl.h"
#include "rules.h"
#include "news.h"
#include "mail.h"
#include "hxfer.h"
#include "webapi.h"

#define  THIS_FILE   __FILE__

#define  CIX_MAIL_THRESHOLD         40
#define  CIX_DEF_RIGHT_MARGIN    77
#define  CIX_DEF_USENET_WINDOW      8192

extern BOOL fIsAmeol2Scratchpad;

extern char FAR szUniqueIDEcho[];
extern char FAR szShowConfStartEcho[];
extern char FAR szShowConfEndEcho[];
extern char FAR szParListStartEcho[];
extern char FAR szParListEndEcho[];
extern char FAR szFileListStartEcho[];
extern char FAR szFileListEndEcho[];
extern char FAR szFdirStartEcho[];
extern char FAR szFdirEndEcho[];
extern char FAR szResumeStartEcho[];
extern char FAR szResumeEndEcho[];
extern char FAR szNewsgroupStartEcho[];
extern char FAR szNewsgroupEndEcho[];

static UINT uZWindowSize;                          /* Usenet ZMODEM download window size */
static WORD wUniqueID;                             /* Unique ID */
static BOOL fNeedCopy = FALSE;                     /* TRUE if need a copy command in script */
static int cMailMsgs = 0;                          /* Number of mail messages encountered */
static UINT cEncodedParts;                         /* Number of encoded parts */
static UINT nThisEncodedPart;                      /* Index of this encoded part */

/* List of CIX script commands.
 */
static char FAR szCIXInit1[] = "opt file s y terse unix y %s comp y term pag 0 ref n auto y edit q q";
static char FAR szCIXInit2[] = "read forward q";
static char FAR szCIXInit3[] = "opt term width %u recent %u messagesize %lu q";
static char FAR szCIXInit4[] = "opt recent %d messagesize %lu q";
static char FAR szPutBinmail[] = "binmail %s %s";
static char FAR szClearMail[] = "mail cl in quick cl out quick q";
static char FAR szSetRecent[] = "opt recent %u q";
static char FAR szQuitMode[] = "q";
static char FAR szQuitModeCR[] = "q";
static char FAR szBye[] = "bye";
static char FAR szEnterMailMode[] = "mail";
static char FAR szSend[] = "send";
static char FAR szJoinConf[] = "j %s";
static char FAR szJoinConfTopic[] = "j %s/%s";
static char FAR szResignConf[] = "resi %s";
static char FAR szFileEcho0[] = "file echo";
static char FAR szFileEcho2[] = "file echo %s %s";
static char FAR szFileEchoTopic[] = "file echo %s %s/%s";
static char FAR szFileEchoID[] = "file echo %s %u";
static char FAR szFileEchoPling[] = "file echo %s !%s/%s";
static char FAR szEraFile[] = "era %s";
static char FAR szEnterModMode[] = "mod %s";
static char FAR szKillscratch[] = "killscratch";
static char FAR szFileShowAll[] = "go showallla";
static char FAR szArcscratch[] = "arcscratch";
static char FAR szFileMailDir[] = "mail file dir q";
static char FAR szDownload[] = "download";
static char FAR szEnterMail[] = "mail";
static char FAR szBatchArticle[] = "batch %s %lu endbatch";
static char FAR szBatchNewsgroup[] = "batch %s all endbatch";
static char FAR szQuickClearMail[] = "mail cl in %u days quick cl out %u days quick q";
static char FAR szSetBatchsize[] = "opt batchsize %lu";

static char FAR szGoAmeolUpdate[] = "go ameolupdate"; /*!!SM!!*/

/* List of Ameol script commands.
 */
static char FAR szWaitforMain[] = "waitfor \"M:\"";
static char FAR szWaitforMail[] = "waitfor \"Ml:\"";
static char FAR szWaitforNewsnet[] = "waitfor \"Newsnet:\"";
static char FAR szWaitforMod[] = "waitfor \"Mod:\"";
static char FAR szNoRecover[] = "set recover 0";
static char FAR szSetRecover[] = "set recover 1";
static char FAR szSuccess[] = "success %u";
static char FAR szDeleteFile[] = "delete \"%s\"";
static char FAR szEndWhile[] = "endwhile";
static char FAR szEndif[] = "endif";
static char FAR szTestLasterrorFalse[] = "if lasterror == 0";
static char FAR szTestLasterrorTrue[] = "if lasterror != 0";
static char FAR szDownloadFile[] = "download \"%s\\%s\"";
static char FAR szLog[] = "log \"%s\"";
static char FAR szUpload[] = "upload \"%s\"";
static char FAR szStatus[] = "status \"%s\"";
static char FAR szRecord[] = "record \"%s/%s\" \"%s\\%s\"";
static char szScriptScrPath[ _MAX_PATH ];

#define  FMT_TXT_CIX          0x001
#define  FMT_TXT_USENET       0x002
#define  FMT_TXT_CMD          0x004
#define  FMT_TXT_QUOTEFROM    0x008
#define  FMT_FORCED           0x800

void FASTCALL FormatText( HNDFILE, HPSTR, UINT, int );
void FASTCALL WriteStatusLine( HNDFILE, LPSTR );
void CDECL WriteScriptLine( HNDFILE, LPSTR, ... );
void FASTCALL WriteIDHeader( HNDFILE );
BOOL FASTCALL SendMailFile( HNDFILE, MAILOBJECT FAR *, int );
BOOL FASTCALL SendBinmailFile( HNDFILE, BINMAILOBJECT FAR *, int );
BOOL FASTCALL SendBinaryFile( HNDFILE, BINMAILOBJECT FAR *, int );
BOOL FASTCALL SendEncodedFile( HNDFILE, MAILOBJECT FAR *, int );
void FASTCALL MakeCixName( char * );
LPSTR FASTCALL UUConvert( LPSTR, char * );
void FASTCALL CreateMailObject( HNDFILE, MAILOBJECT FAR *, LPSTR, BOOL, BOOL );
void FASTCALL CreateForwardMailObject( HNDFILE, FORWARDMAILOBJECT FAR * );
void FASTCALL ExtractCIXAddress( LPSTR, LPSTR );
BOOL FASTCALL InsertScript( HNDFILE, LPCSTR );
void FASTCALL Mail_UpdateDirectory( HNDFILE );
void FASTCALL GenerateCIXUsenetScript( HNDFILE, DWORD );
BOOL FASTCALL IsCixnewsActions( void );
BOOL FASTCALL IsCixnewsInFolderList( HPSTR );
void FASTCALL SendNewsArticle( HNDFILE, ARTICLEOBJECT FAR * );
void FASTCALL CreateCIXNewsnetHeader( HNDFILE, ARTICLEOBJECT FAR *, char *, char * );

/* This function generates the CIX script that writes new messages and
 * comments to conferences.
 */
#pragma optimize("", off)
BOOL FASTCALL GenerateScript( DWORD dwBlinkFlags )
{
   LPOB lpob;
   char sz[ 200 ];
   HNDFILE fhIn;
   HNDFILE fh;
   BOOL fNeedDownload;
   BOOL fNeedMailDir;
   BOOL fHasIDHeader;
   BOOL fInMail;
   WORD wRecent;
   int count;

   /* Create the new script file.
    */
   BEGIN_PATH_BUF;
   Amuser_GetActiveUserDirectory( lpPathBuf, _MAX_PATH );
   strcat( strcat( lpPathBuf, "\\" ), szNewmesScr );
   if( ( fh = Amfile_Create( lpPathBuf, 0 ) ) == HNDFILE_ERROR )
      return( FALSE );
   END_PATH_BUF;

   /* Copy the contents of CIXINIT.SCR at this position.
    */
   if( !InsertScript( fh, szCixInitScr ) )
      {
      char szCmds[ 40 ];
      HXFER pftp;

      pftp = GetFileTransferProtocolHandle( szCixProtocol );
      GetFileTransferProtocolCommands( pftp, szCmds, sizeof(szCmds) );
      FmtWriteToScriptFile( fh, TRUE, szCIXInit1, szCmds );
      WriteScriptLine( fh, szWaitforMain );
      WriteToScriptFile( fh, TRUE, szCIXInit2 );
      WriteScriptLine( fh, szWaitforMain );
      }
   if( !( fCompatFlag & COMF_DONTDOPROFILE ) )
      {
      /* If the editor wrap column was set pass the default CIX
       * wrap column, extend the right hand margin on CIX so that
       * it won't wrap our text.
       */
      if( iWrapCol > CIX_DEF_RIGHT_MARGIN )
         FmtWriteToScriptFile( fh, TRUE, szCIXInit3, iWrapCol + 2, iRecent, dwMaxMsgSize );
      else
         FmtWriteToScriptFile( fh, TRUE, szCIXInit4, iRecent, dwMaxMsgSize );
      WriteScriptLine( fh, szWaitforMain );
      }
   wRecent = iRecent;

   if( ( strlen( szProf1 ) > 0 ) || ( strlen( szProf2 ) > 0 ) || ( strlen( szProf3 ) > 0 ) || ( strlen( szProf4 ) > 0 ) ||( strlen( szProf5 ) > 0 ) )
   {
      WriteToScriptFile( fh, TRUE, "go confprofile" );
      WriteScriptLine( fh, "waitfor \"Category:\"" );
      if( strlen( szProf1 ) > 0 )
         {
            wsprintf( lpTmpBuf, "put \"join %s\"", szProf1 );
            WriteScriptLine( fh, lpTmpBuf );
            WriteScriptLine( fh, "waitfor \"Category:\"" );
         }
      if( strlen( szProf2 ) > 0 )
         {
            wsprintf( lpTmpBuf, "put \"join %s\"", szProf2 );
            WriteScriptLine( fh, lpTmpBuf );
            WriteScriptLine( fh, "waitfor \"Category:\"" );
         }
      if( strlen( szProf3 ) > 0 )
         {
            wsprintf( lpTmpBuf, "put \"join %s\"", szProf3 );
            WriteScriptLine( fh, lpTmpBuf );
            WriteScriptLine( fh, "waitfor \"Category:\"" );
         }
      if( strlen( szProf4 ) > 0 )
         {
            wsprintf( lpTmpBuf, "put \"join %s\"", szProf4 );
            WriteScriptLine( fh, lpTmpBuf );
            WriteScriptLine( fh, "waitfor \"Category:\"" );
         }
      if( strlen( szProf5 ) > 0 )
         {
            wsprintf( lpTmpBuf, "put \"join %s\"", szProf5 );
            WriteScriptLine( fh, lpTmpBuf );
            WriteScriptLine( fh, "waitfor \"Category:\"" );
         }

      WriteToScriptFile( fh, TRUE, "quit" );
      WriteScriptLine( fh, szWaitforMain );
      WriteToScriptFile( fh, TRUE, "script" );
      WriteScriptLine( fh, szWaitforMain );
      szProf1[ 0 ] = '\0';
      szProf2[ 0 ] = '\0';
      szProf3[ 0 ] = '\0';
      szProf4[ 0 ] = '\0';
      szProf5[ 0 ] = '\0';
   }

   /* Get Usenet ZMODEM window size.
    */
   uZWindowSize = Amuser_GetPPInt( szSettings, "UsenetZWindow", CIX_DEF_USENET_WINDOW );

   /* If the recovery flags is set, include a command to download the
    * scratchpad left over from the last connection.
    */
   if( !TestF( dwBlinkFlags, BF_CIXRECOVER ) )
      {
      WriteToScriptFile( fh, TRUE, szKillscratch );
      WriteScriptLine( fh, szWaitforMain );
      }
   else {
      WriteScriptLine( fh, szSetRecover );
      WriteToScriptFile( fh, TRUE, szDownload );
      WriteScriptLine( fh, szDownloadFile, (LPSTR)pszScratchDir, (LPSTR)szScratchPad );
      WriteScriptLine( fh, szTestLasterrorTrue );
      WriteScriptLine( fh, szLog, GS(IDS_STR1092) );
      WriteToScriptFile( fh, TRUE, szBye );
      WriteScriptLine( fh, "end" );
      WriteScriptLine( fh, szEndif );
      WriteScriptLine( fh, szNoRecover );
      WriteScriptLine( fh, "rename \"%s\\%s\" \"%s\\%s\"", (LPSTR)pszScratchDir, (LPSTR)szScratchPad, (LPSTR)pszScratchDir, (LPSTR)szScratchPadRcv );
      WriteScriptLine( fh, "import \"%s\\%s\"", (LPSTR)pszScratchDir, (LPSTR)szScratchPadRcv );
      }
   fNeedMailDir = FALSE;
   fHasIDHeader = FALSE;

   /* Create an unique ID to identify 'wrappers'.
    */
   wUniqueID = Amdate_GetPackedCurrentDate() + Amdate_GetPackedCurrentTime();

   /* Handle moderator functions Add Par and Rem Par before the main
    * script.
    */
   if( dwBlinkFlags & (BF_POSTCIXMSGS|BF_GETCIXMAIL) )
      for( lpob = Amob_GetOb( NULL ); lpob; lpob = Amob_GetOb( lpob ) )
         {
         OBINFO obinfo;

         Amob_GetObInfo( lpob, &obinfo );
         if( !( obinfo.obHdr.wFlags & (OBF_HOLD|OBF_OPEN) ) )
            {
            switch( obinfo.obHdr.clsid )
               {
               case OBTYPE_BINMAIL:
                  if( TestF( dwBlinkFlags, BF_POSTCIXMAIL ) )
                     {
                     BINMAILOBJECT FAR * lpbmo;

                     /* We're binmailing a file.
                      */
                     lpbmo = (BINMAILOBJECT FAR *)obinfo.lpObData;
                     if( lpbmo->wEncodingScheme == ENCSCH_BINARY )
                        {
                        if( lpbmo->wFlags & BINF_UPLOAD )
                           fNeedMailDir = TRUE;
                        SendBinaryFile( fh, lpbmo, obinfo.obHdr.id );
                        }
                     }
                  break;

               case OBTYPE_MAILMESSAGE:
                  if( TestF( dwBlinkFlags, BF_POSTCIXMAIL ) && obinfo.obHdr.id != 2 )
                     {
                     MAILOBJECT FAR * lpmo;

                     /* Handle the case where the user has attached a binary file
                      * to a mail message. This is pretty much the same as binmailing
                      * a file via the Send File command, so expect this code to become
                      * obsolete at some point.
                      */
                     lpmo = (MAILOBJECT FAR *)obinfo.lpObData;
                     if( *DRF(lpmo->recAttachments) )
                        if( lpmo->wEncodingScheme == ENCSCH_BINARY )
                           {
                           SendMailFile( fh, lpmo, obinfo.obHdr.id );
                           fNeedMailDir = TRUE;
                           }
                     }
                  break;

               case OBTYPE_CLEARCIXMAIL:
                  if( TestF( dwBlinkFlags, BF_POSTCIXMAIL ) )
                     {
                     if( nCIXMailClearCount )
                        FmtWriteToScriptFile( fh, TRUE, szQuickClearMail, nCIXMailClearCount, nCIXMailClearCount );
                     else
                        WriteToScriptFile( fh, TRUE, szClearMail );
                     WriteScriptLine( fh, szWaitforMain );
                     WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                     }
                  break;

               case OBTYPE_PREINCLUDE: {
                  INCLUDEOBJECT FAR * lpio;

                  lpio = (INCLUDEOBJECT FAR *)obinfo.lpObData;
                  WriteScriptLine( fh, "include \"%s\"", DRF(lpio->recFileName) );
                  if( !TestF( obinfo.obHdr.wFlags, OBF_KEEP ) )
                     WriteScriptLine( fh, szDeleteFile, DRF(lpio->recFileName) );
                  WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                  break;
                  }

               case OBTYPE_UNBANPAR: 
               case OBTYPE_BANPAR: {
                  if( TestF( dwBlinkFlags, BF_POSTCIXMSGS ) )
                     {
                     BANPAROBJECT FAR * lpbpo;

                     LPSTR pszBanCmd = (obinfo.obHdr.clsid == OBTYPE_BANPAR) ? "banadd" : "bandel";

                     lpbpo = (BANPAROBJECT FAR *)obinfo.lpObData;
                     FmtWriteToScriptFile( fh, TRUE, szJoinConf, DRF(lpbpo->recConfName) );
                     WriteScriptLine( fh, "if waitfor( \"Topic?\", \"No conference\" ) == 0" );
                     WriteToScriptFile( fh, TRUE, "" );
                     WriteScriptLine( fh, "waitfor \"R:\"" );
                     FmtWriteToScriptFile( fh, TRUE, "go %s", pszBanCmd );
                     WriteScriptLine( fh, "if waitfor( \"%s:\", \"cannot continue\", \"R:\" ) == 0", pszBanCmd );
                     WriteToScriptFile( fh, TRUE, DRF( lpbpo->recUserName ) );
                     WriteScriptLine( fh, szEndif );
                     WriteScriptLine( fh, szEndif );
                     WriteToScriptFile( fh, TRUE, szQuitMode );
                     WriteScriptLine( fh, szWaitforMain );
                     WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                  }
                  break;
                  }

               case OBTYPE_ADDPAR:
                  if( TestF( dwBlinkFlags, BF_POSTCIXMSGS ) )
                     {
                     ADDPAROBJECT FAR * lpapo;

                     lpapo = (ADDPAROBJECT FAR *)obinfo.lpObData;
                     FmtWriteToScriptFile( fh, TRUE, szEnterModMode, DRF( lpapo->recConfName ) );
                     WriteScriptLine( fh, szWaitforMod );
                     FmtWriteToScriptFile( fh, TRUE, "add par %s", DRF( lpapo->recUserName ) );
                     WriteScriptLine( fh, szWaitforMod );
                     WriteToScriptFile( fh, TRUE, szQuitMode );
                     WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                     }
                  break;

               case OBTYPE_COMOD:
                  if( TestF( dwBlinkFlags, BF_POSTCIXMSGS ) )
                     {
                     COMODOBJECT FAR * lpcmo;
                     char szModList[ LEN_AUTHORLIST+1 ];
                     char szName[ LEN_INTERNETNAME+1 ];
                     LPSTR np;
   
                     lpcmo = (COMODOBJECT FAR *)obinfo.lpObData;
                     FmtWriteToScriptFile( fh, TRUE, szEnterModMode, DRF( lpcmo->recConfName ) );
                     WriteScriptLine( fh, szWaitforMod );
   
                     lstrcpy( np = szModList, DRF( lpcmo->recUserName ) );
                     np = ExtractMailAddress( np, szName, LEN_INTERNETNAME );
                     while( *szName )
                        {
                        FmtWriteToScriptFile( fh, TRUE, "comod %s", (LPSTR)szName );
                        WriteScriptLine( fh, szWaitforMod );
                        np = ExtractMailAddress( np, szName, LEN_INTERNETNAME );
                        }
                     WriteToScriptFile( fh, TRUE, szQuitMode );
                     WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                     }
                  break;

               case OBTYPE_REMPAR:
                  if( TestF( dwBlinkFlags, BF_POSTCIXMSGS ) )
                     {
                     REMPAROBJECT FAR * lprpo;

                     lprpo = (REMPAROBJECT FAR *)obinfo.lpObData;
                     FmtWriteToScriptFile( fh, TRUE, szEnterModMode, DRF( lprpo->recConfName ) );
                     WriteScriptLine( fh, szWaitforMod );
                     FmtWriteToScriptFile( fh, TRUE, "rem par %s", DRF( lprpo->recUserName ) );
                     WriteScriptLine( fh, szWaitforMod );
                     WriteToScriptFile( fh, TRUE, szQuitMode );
                     WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                     }
                  break;

               case OBTYPE_EXMOD:
                  if( TestF( dwBlinkFlags, BF_POSTCIXMSGS ) )
                     {
                     EXMODOBJECT FAR * lpemo;
                     char szModList[ LEN_AUTHORLIST+1 ];
                     char szName[ LEN_INTERNETNAME+1 ];
                     LPSTR np;
   
                     lpemo = (EXMODOBJECT FAR *)obinfo.lpObData;
                     FmtWriteToScriptFile( fh, TRUE, szEnterModMode, DRF(lpemo->recConfName) );
                     WriteScriptLine( fh, szWaitforMod );
   
                     lstrcpy( np = szModList, DRF(lpemo->recUserName) );
                     np = ExtractMailAddress( np, szName, LEN_INTERNETNAME );
                     while( *szName )
                        {
                        FmtWriteToScriptFile( fh, TRUE, "exmod %s", (LPSTR)szName );
                        WriteScriptLine( fh, szWaitforMod );
                        np = ExtractMailAddress( np, szName, LEN_INTERNETNAME );
                        }
                     WriteToScriptFile( fh, TRUE, szQuitMode );
                     WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                     }
                  break;
               }
            }
         }

   /* Create the contents of the SCRIPT.SCR file, which is generated during
    * the upload.
    */
   fNeedCopy = TRUE;
   fNeedDownload = ( dwBlinkFlags & (BF_GETCIXMSGS|BF_GETCIXMAIL) ) != 0;
   count = 0;
   cMailMsgs = 0;
   fInMail = FALSE;
   for( lpob = Amob_GetOb( NULL ); lpob; lpob = Amob_GetOb( lpob ) )
      {
      OBINFO obinfo;

      Amob_GetObInfo( lpob, &obinfo );
      if( !( obinfo.obHdr.wFlags & (OBF_HOLD|OBF_OPEN) ) )
         {
         switch( obinfo.obHdr.clsid )
            {
            case OBTYPE_BINMAIL:
               if( TestF( dwBlinkFlags, BF_POSTCIXMAIL ) )
                  {
                  BINMAILOBJECT FAR * lpbmo;

                  /* Ensure we've entered the mail subsystem.
                   */
                  if( !fInMail )
                     {
                     WriteToScriptFile( fh, FALSE, szEnterMailMode );
                     fInMail = TRUE;
                     }

                  /* We're binmailing a file.
                   */
                  lpbmo = (BINMAILOBJECT FAR *)obinfo.lpObData;
                  if( lpbmo->wEncodingScheme != ENCSCH_BINARY )
                     SendBinmailFile( fh, lpbmo, 2 );
                  }
               break;

            case OBTYPE_FORWARDMAIL:
               if( TestF( dwBlinkFlags, BF_POSTCIXMAIL ) && obinfo.obHdr.id == 2 )
                  {
                  FORWARDMAILOBJECT FAR * lpfmo;

                  /* Ensure we've entered the mail subsystem.
                   */
                  if( !fInMail )
                     {
                     WriteToScriptFile( fh, FALSE, szEnterMailMode );
                     fInMail = TRUE;
                     }

                  /* When forwarding via CIX, create an ordinary mail
                   * message with the prefix Fw:
                   */
                  lpfmo = (FORWARDMAILOBJECT FAR *)obinfo.lpObData;
                  CreateForwardMailObject( fh, lpfmo );
                  ++count;
                  }
               break;

            case OBTYPE_MAILMESSAGE:
               if( TestF( dwBlinkFlags, BF_POSTCIXMAIL ) && obinfo.obHdr.id == 2 )
                  {
                  MAILOBJECT FAR * lpmo;

                  /* Do nothing if not sending uuencoded.
                   */
                  lpmo = (MAILOBJECT FAR *)obinfo.lpObData;
                  if( *DRF(lpmo->recAttachments ) && lpmo->wEncodingScheme != ENCSCH_UUENCODE )
                     break;

                  /* Ensure we've entered the mail subsystem.
                   */
                  if( !fInMail )
                     {
                     WriteToScriptFile( fh, FALSE, szEnterMailMode );
                     fInMail = TRUE;
                     }

                  /* If we've any attachments and they're being send encoded,
                   * then send 'em now.
                   */
                  cEncodedParts = 0;
                  if( *DRF(lpmo->recAttachments ) )
                     {
                     if( lpmo->wEncodingScheme != ENCSCH_BINARY )
                        SendMailFile( fh, lpmo, 2 );
                     }
                  else
                     {
                     BOOL fEcho;

                     fEcho = !(lpmo->wFlags & MOF_NOECHO );
                     CreateMailObject( fh, lpmo, DRF(lpmo->recText), fEcho, FALSE );
                     }
                  ++count;
                  }
               break;
            }
         }
      }
   if( fInMail )
      WriteToScriptFile( fh, FALSE, szQuitModeCR );

   /* Create the contents of the SCRIPT.SCR file, which is generated during
    * the upload.
    */
   for( lpob = Amob_GetOb( NULL ); lpob; lpob = Amob_GetOb( lpob ) )
      {
      OBINFO obinfo;

      Amob_GetObInfo( lpob, &obinfo );
      if( !( obinfo.obHdr.wFlags & (OBF_HOLD|OBF_OPEN) ) )
         {
         switch( obinfo.obHdr.clsid )
            {
            case OBTYPE_WITHDRAW:
               if( TestF( dwBlinkFlags, BF_POSTCIXMSGS ) )
                  {
                  WITHDRAWOBJECT FAR * lpwo;

                  lpwo = (WITHDRAWOBJECT FAR *)obinfo.lpObData;
                  FmtWriteToScriptFile( fh, FALSE, szJoinConf, DRF(lpwo->recTopicName) );
                  FmtWriteToScriptFile( fh, FALSE, "withdraw %lu", lpwo->dwMsg );
                  WriteToScriptFile( fh, FALSE, szQuitModeCR );
                  ++count;
                  }
               break;

            case OBTYPE_RESTORE:
               if( TestF( dwBlinkFlags, BF_POSTCIXMSGS ) )
                  {
                  RESTOREOBJECT FAR * lpro;

                  lpro = (RESTOREOBJECT FAR *)obinfo.lpObData;
                  FmtWriteToScriptFile( fh, FALSE, szJoinConf, DRF(lpro->recTopicName) );
                  FmtWriteToScriptFile( fh, FALSE, "restore %lu", lpro->dwMsg );
                  WriteToScriptFile( fh, FALSE, szQuitModeCR );
                  ++count;
                  }
               break;

            case OBTYPE_COPYMSG:
               if( TestF( dwBlinkFlags, BF_POSTCIXMSGS ) )
                  {
                  COPYMSGOBJECT FAR * lpcmo;

                  lpcmo = (COPYMSGOBJECT FAR *)obinfo.lpObData;
                  FmtWriteToScriptFile( fh, FALSE, szJoinConfTopic, DRF(lpcmo->recConfName), DRF(lpcmo->recTopicName) );
                  WriteToScriptFile( fh, FALSE, szQuitModeCR );
                  FmtWriteToScriptFile( fh, FALSE, szJoinConfTopic, DRF(lpcmo->recCurConfName), DRF(lpcmo->recCurTopicName) );
                  FmtWriteToScriptFile( fh, FALSE, "copy %lu to %s/%s", lpcmo->dwMsg, DRF(lpcmo->recConfName), DRF(lpcmo->recTopicName) );
                  WriteToScriptFile( fh, FALSE, szQuitModeCR );
                  ++count;
                  }
               break;

            case OBTYPE_JOINCONFERENCE:
               if( TestF( dwBlinkFlags, BF_POSTCIXMSGS ) )
                  {
                  JOINOBJECT FAR * lpjo;

                  lpjo = (JOINOBJECT FAR *)obinfo.lpObData;
                  if( wRecent != lpjo->wRecent )
                     {
                     wRecent = lpjo->wRecent;
                     FmtWriteToScriptFile( fh, FALSE, szSetRecent, wRecent );
                     }
                  FmtWriteToScriptFile( fh, FALSE, szJoinConf, DRF(lpjo->recConfName) );
                  WriteToScriptFile( fh, FALSE, szQuitModeCR );
                  ++count;
                  }
               break;

            case OBTYPE_CIXRESIGN:
               if( TestF( dwBlinkFlags, BF_POSTCIXMSGS ) )
                  {
                  RESIGNOBJECT FAR * lpro;

                  lpro = (RESIGNOBJECT FAR *)obinfo.lpObData;
                  FmtWriteToScriptFile( fh, FALSE, szResignConf, DRF(lpro->recConfName) );
                  ++count;
                  }
               break;

               
            case OBTYPE_SAYMESSAGE:
               if( TestF( dwBlinkFlags, BF_POSTCIXMSGS ) )
                  {
                  SAYOBJECT FAR * lpso;
                  LPSTR lpszTitle;

                  lpso = (SAYOBJECT FAR *)obinfo.lpObData;
                  FmtWriteToScriptFile( fh, FALSE, szJoinConfTopic, DRF(lpso->recConfName), DRF(lpso->recTopicName) );
                  WriteToScriptFile( fh, FALSE, "say" );
                  lpszTitle = DRF(lpso->recSubject);
                  if( lpszTitle[ 0 ] == '.' && lpszTitle[ 1 ] == '\0' )
                     FmtWriteToScriptFile( fh, FALSE, "%s ", lpszTitle );
                  else
                     FmtWriteToScriptFile( fh, FALSE, "%s", lpszTitle );
                  FormatText( fh, DRF(lpso->recText), FMT_TXT_CIX, iWrapCol );
                  WriteToScriptFile( fh, FALSE, szQuitModeCR );
                  ++count;
                  }
               break;

            case OBTYPE_COMMENTMESSAGE:
               if( TestF( dwBlinkFlags, BF_POSTCIXMSGS ) )
                  {
                  COMMENTOBJECT FAR * lpco;

                  lpco = (COMMENTOBJECT FAR *)obinfo.lpObData;
                  FmtWriteToScriptFile( fh, FALSE, szJoinConfTopic, DRF(lpco->recConfName), DRF(lpco->recTopicName) );
                  FmtWriteToScriptFile( fh, FALSE, "com %lu", lpco->dwReply );
                  FormatText( fh, DRF(lpco->recText), FMT_TXT_CIX, iWrapCol );
                  WriteToScriptFile( fh, FALSE, szQuitModeCR );
                  ++count;
                  }
               break;
            }
         }
      }

   /* Handle stuff that needs to come at the end
      */
   if( TestF( dwBlinkFlags, BF_GETCIXMSGS ) )
      for( lpob = Amob_GetOb( NULL ); lpob; lpob = Amob_GetOb( lpob ) )
         {
         OBINFO obinfo;
   
         Amob_GetObInfo( lpob, &obinfo );
         if( !( obinfo.obHdr.wFlags & (OBF_HOLD|OBF_OPEN) ) )
            {
            switch( obinfo.obHdr.clsid )
               {
               case OBTYPE_GETMESSAGE: {
                  GETMESSAGEOBJECT FAR * lpgmo;

                  lpgmo = (GETMESSAGEOBJECT FAR *)obinfo.lpObData;
                  wsprintf( sz, szJoinConf, DRF(lpgmo->recTopicName) );
                  WriteToScriptFile( fh, FALSE, sz );
                  if( lpgmo->dwStartMsg == lpgmo->dwEndMsg )
                     FmtWriteToScriptFile( fh, FALSE, "file %lu", lpgmo->dwStartMsg );
                  else
                     FmtWriteToScriptFile( fh, FALSE, "file %lu to %lu", lpgmo->dwStartMsg, lpgmo->dwEndMsg );
                  WriteToScriptFile( fh, FALSE, szQuitModeCR );
                  fNeedDownload = TRUE;
                  ++count;
                  break;
                  }

               case OBTYPE_INLINE: {
                  INLINEOBJECT FAR * lpino;

                  lpino = (INLINEOBJECT FAR *)obinfo.lpObData;
                  if( ( fhIn = Amfile_Open( DRF(lpino->recFileName), AOF_READ ) ) != HNDFILE_ERROR )
                     {
                     char sz2[ 80 ];
                     LPLBF hBuffer;
   
                     hBuffer = Amlbf_Open( fhIn, AOF_READ );
                     while( Amlbf_Read( hBuffer, sz2, sizeof( sz2 ) - 1, NULL, NULL, &fIsAmeol2Scratchpad ) )
                        {
                        if( CompileInline( fh, sz2 ) )
                           fNeedDownload = TRUE;
                        }
                     Amlbf_Close( hBuffer );
                     ++count;
                     }
                  break;
                  }
               }
            }
         }

   /* Handle batching resume downloads. */
   fHasIDHeader = FALSE;
   if( TestF( dwBlinkFlags, BF_GETCIXMSGS ) )
      for( lpob = Amob_GetOb( NULL ); lpob; lpob = Amob_GetOb( lpob ) )
         {
         OBINFO obinfo;

         Amob_GetObInfo( lpob, &obinfo );
         if( !( obinfo.obHdr.wFlags & (OBF_HOLD|OBF_OPEN) ) )
            switch( obinfo.obHdr.clsid )
               {
               case OBTYPE_JOINCONFERENCE: {
                  char szConfName[ LEN_CONFNAME+1 ];
                  JOINOBJECT FAR * lpjo;
                  register int c;
                  LPSTR lpsz;

                  if( !fHasIDHeader )
                     WriteIDHeader( fh );
                  lpjo = (JOINOBJECT FAR *)obinfo.lpObData;
                  lpsz = DRF(lpjo->recConfName);
                  for( c = 0; *lpsz && *lpsz != '/' && c < LEN_CONFNAME+1; ++c ) //FS#154 2054
                     szConfName[ c ] = *lpsz++;
                  if( *lpsz != '/' )
                     {
                     szConfName[ c ] = '\0';
                     FmtWriteToScriptFile( fh, FALSE, "file echo %s %s 0", (LPSTR)szShowConfStartEcho, (LPSTR)szConfName );
                     FmtWriteToScriptFile( fh, FALSE, "file show %s", (LPSTR)szConfName );
                     FmtWriteToScriptFile( fh, FALSE, szFileEchoID, (LPSTR)szShowConfEndEcho, wUniqueID );
                     }
                  if( fGetParList )
                     {
                     szConfName[ c ] = '\0';
                     FmtWriteToScriptFile( fh, FALSE, szFileEcho2, (LPSTR)szParListStartEcho, (LPSTR)szConfName );
                     FmtWriteToScriptFile( fh, FALSE, "file show par %s", (LPSTR)szConfName );
                     FmtWriteToScriptFile( fh, FALSE, szFileEchoID, (LPSTR)szParListEndEcho, wUniqueID );
                     }
                  break;
                  }

               case OBTYPE_FILELIST: {
                  FILELISTOBJECT FAR * lpflo;

                  if( !fHasIDHeader )
                     WriteIDHeader( fh );
                  lpflo = (FILELISTOBJECT FAR *)obinfo.lpObData;
                  FmtWriteToScriptFile( fh, FALSE, szFileEchoPling, (LPSTR)szFileListStartEcho, DRF(lpflo->recConfName), DRF(lpflo->recTopicName) );
                  FmtWriteToScriptFile( fh, FALSE, szJoinConfTopic, DRF(lpflo->recConfName), DRF(lpflo->recTopicName) );
                  WriteToScriptFile( fh, FALSE, "file time flist" );
                  WriteToScriptFile( fh, FALSE, szQuitModeCR );
                  WriteToScriptFile( fh, FALSE, szFileEcho0 );
                  FmtWriteToScriptFile( fh, FALSE, szFileEchoID, (LPSTR)szFileListEndEcho, wUniqueID );
                  fNeedDownload = TRUE;
                  break;
                  }

               case OBTYPE_FDIR: {
                  FDIROBJECT FAR * lpfdr;

                  if( !fHasIDHeader )
                     WriteIDHeader( fh );
                  lpfdr = (FDIROBJECT FAR *)obinfo.lpObData;
                  FmtWriteToScriptFile( fh, FALSE, szFileEchoTopic, (LPSTR)szFdirStartEcho, DRF(lpfdr->recConfName), DRF(lpfdr->recTopicName) );
                  FmtWriteToScriptFile( fh, FALSE, szJoinConfTopic, DRF(lpfdr->recConfName), DRF(lpfdr->recTopicName) );
                  WriteToScriptFile( fh, FALSE, "file fdir" );
                  WriteToScriptFile( fh, FALSE, szQuitModeCR );
                  WriteToScriptFile( fh, FALSE, szFileEcho0 );
                  FmtWriteToScriptFile( fh, FALSE, szFileEchoID, (LPSTR)szFdirEndEcho, wUniqueID );
                  fNeedDownload = TRUE;
                  break;
                  }

               case OBTYPE_GETPARLIST: {
                  GETPARLISTOBJECT FAR * lpgpo;

                  lpgpo = (GETPARLISTOBJECT FAR *)obinfo.lpObData;
                  if( !fHasIDHeader )
                     WriteIDHeader( fh );
                  FmtWriteToScriptFile( fh, FALSE, szFileEcho2, (LPSTR)szParListStartEcho, DRF(lpgpo->recConfName) );
                  FmtWriteToScriptFile( fh, FALSE, "file show par %s", DRF(lpgpo->recConfName) );
                  FmtWriteToScriptFile( fh, FALSE, szFileEchoID, (LPSTR)szParListEndEcho, wUniqueID );
                  FmtWriteToScriptFile( fh, FALSE, "file echo %s %s 1", (LPSTR)szShowConfStartEcho, DRF(lpgpo->recConfName) );
                  FmtWriteToScriptFile( fh, FALSE, "file show %s", DRF(lpgpo->recConfName) );
                  FmtWriteToScriptFile( fh, FALSE, szFileEchoID, (LPSTR)szShowConfEndEcho, wUniqueID );
                  fNeedDownload = TRUE;
                  break;
                  }

               case OBTYPE_UPDATENOTE: {
                  UPDATENOTEOBJECT FAR * lpucn;
                  LPSTR lpsz;


                  lpucn = (UPDATENOTEOBJECT FAR *)obinfo.lpObData;
                  if( !fHasIDHeader )
                     WriteIDHeader( fh );
                  lpsz = DRF(lpucn->recConfName);
                  FmtWriteToScriptFile( fh, FALSE, "file echo %s %s 0", (LPSTR)szShowConfStartEcho, (LPSTR)lpsz );
                  FmtWriteToScriptFile( fh, FALSE, "file show %s", (LPSTR)lpsz );
                  FmtWriteToScriptFile( fh, FALSE, szFileEchoID, (LPSTR)szShowConfEndEcho, wUniqueID );

                  fNeedDownload = TRUE;
                  break;
                  }

               case OBTYPE_RESUME: {
                  RESUMEOBJECT FAR * lpro;

                  lpro = (RESUMEOBJECT FAR *)obinfo.lpObData;
                  if( !fHasIDHeader )
                     WriteIDHeader( fh );
                  FmtWriteToScriptFile( fh, FALSE, szFileEcho2, (LPSTR)szResumeStartEcho, DRF(lpro->recUserName) );
                  FmtWriteToScriptFile( fh, FALSE, "file show resume %s", DRF(lpro->recUserName) );
                  WriteToScriptFile( fh, FALSE, szFileEcho0 );
                  FmtWriteToScriptFile( fh, FALSE, szFileEchoID, (LPSTR)szResumeEndEcho, wUniqueID );
                  fNeedDownload = TRUE;
                  break;
                  }
               }
            }

   /* If no SCRIPT.SCR created, issue commands online.
    */
   if( fNeedCopy )
      {
      if( ( dwBlinkFlags & (BF_GETCIXMSGS|BF_GETCIXMAIL) ) || fNeedDownload )
         {
         if( TestF( dwBlinkFlags, BF_GETCIXMSGS ) )
            {
            WriteToScriptFile( fh, TRUE, "file read all" );
            WriteScriptLine( fh, "if waitfor(\"R:\", \"M:\") == 0" );
            WriteScriptLine( fh, "put \"q\"" );
            WriteScriptLine( fh, szWaitforMain );
            WriteScriptLine( fh, "endif" );
            }
         if( TestF( dwBlinkFlags, BF_GETCIXMAIL ) )
            {
            WriteToScriptFile( fh, TRUE, szEnterMailMode );
            WriteScriptLine( fh, szWaitforMail );
            WriteToScriptFile( fh, TRUE, "file all" );
            WriteScriptLine( fh, szWaitforMail );
            WriteToScriptFile( fh, TRUE, szQuitModeCR );
            WriteScriptLine( fh, szWaitforMain );
            }
         if( fArcScratch )
            {
            WriteToScriptFile( fh, TRUE, "arcscratch" );
            WriteScriptLine( fh, szWaitforMain );
            }
         WriteScriptLine( fh, szSetRecover );
         fNeedDownload = TRUE;
         count = 0;
         }
      }

   /* Generate the commands to read all unread messages
    */
   else if( ( dwBlinkFlags & (BF_GETCIXMSGS|BF_GETCIXMAIL) ) || fNeedDownload )
      {
      if( fNeedCopy )
         {
         Amuser_GetActiveUserDirectory( szScriptScrPath, _MAX_FNAME );
         strcat( szScriptScrPath, "\\" );
         strcat( szScriptScrPath, "script.scr" );
         wsprintf( lpTmpBuf, "copy \"%s\" <<", szScriptScrPath );
         WriteScriptLine( fh, lpTmpBuf );
         fNeedCopy = FALSE;
         }
      if( TestF( dwBlinkFlags, BF_GETCIXMSGS ) )
         WriteToScriptFile( fh, FALSE, "file read all" );
      if( TestF( dwBlinkFlags, BF_GETCIXMAIL ) )
         {
         WriteToScriptFile( fh, FALSE, szEnterMailMode );
         WriteToScriptFile( fh, FALSE, "file all" );
         WriteToScriptFile( fh, FALSE, szQuitModeCR );
         }
      if( fArcScratch )
         WriteToScriptFile( fh, FALSE, "arcscratch" );
      fNeedDownload = TRUE;
      count = 1;
      }
   if( count )
      {
      WriteScriptLine( fh, "<<" );
      WriteToScriptFile( fh, TRUE, "upl" );
      Amuser_GetActiveUserDirectory( szScriptScrPath, _MAX_FNAME );
      strcat( szScriptScrPath, "\\" );
      strcat( szScriptScrPath, "script.scr" );
      wsprintf( lpTmpBuf, "upload \"%s\"", szScriptScrPath );
      WriteScriptLine( fh, lpTmpBuf );
      WriteScriptLine( fh, szTestLasterrorTrue );
      WriteScriptLine( fh, szLog, GS(IDS_STR1093) );
      WriteToScriptFile( fh, TRUE, szBye );
      WriteScriptLine( fh, "end" );
      WriteScriptLine( fh, szEndif );
      if( fNeedDownload )
         WriteScriptLine( fh, szSetRecover );
      WriteToScriptFile( fh, TRUE, "scput script" );
      if( fNeedDownload )
         WriteStatusLine( fh, GS(IDS_STR1057) );
      else
         WriteStatusLine( fh, GS(IDS_STR1169) );
      WriteScriptLine( fh, szWaitforMain );
      WriteToScriptFile( fh, TRUE, "script" );
      }
   if( TestF( dwBlinkFlags, BF_POSTCIXMSGS ) && count )
      WriteScriptLine( fh, "success 0" );
   if( TestF( dwBlinkFlags, BF_POSTCIXMAIL ) && count )
      WriteScriptLine( fh, "success 2" );
   if( count )
      WriteScriptLine( fh, szWaitforMain );
   if( fNeedDownload )
      {
      WriteToScriptFile( fh, TRUE, szDownload );
      WriteScriptLine( fh, szDownloadFile, (LPSTR)pszScratchDir, (LPSTR)szScratchPad );
      }
   if( count )
      {
      Amuser_GetActiveUserDirectory( szScriptScrPath, _MAX_FNAME );
      strcat( szScriptScrPath, "\\" );
      strcat( szScriptScrPath, "script.scr" );
      wsprintf( lpTmpBuf, "delete \"%s\"", szScriptScrPath );
      WriteScriptLine( fh, lpTmpBuf );
      }
   if( fNeedDownload )
      {
      WriteScriptLine( fh, szTestLasterrorTrue );
      WriteScriptLine( fh, szLog, GS(IDS_STR1094) );
      WriteToScriptFile( fh, TRUE, szBye );
      WriteScriptLine( fh, "end" );
      WriteScriptLine( fh, szEndif );
      WriteScriptLine( fh, szNoRecover );
      WriteScriptLine( fh, "import \"%s\\%s\"", (LPSTR)pszScratchDir, (LPSTR)szScratchPad );
      }
   if( dwBlinkFlags & (BF_POSTCIXMAIL|BF_POSTCIXMSGS) )
      {
      fNeedCopy = FALSE;
      for( lpob = Amob_GetOb( NULL ); lpob; lpob = Amob_GetOb( lpob ) )
         {
         OBINFO obinfo;

         Amob_GetObInfo( lpob, &obinfo );
         if( !( obinfo.obHdr.wFlags & (OBF_HOLD|OBF_OPEN) ) )
            switch( obinfo.obHdr.clsid )
               {
               case OBTYPE_INCLUDE: {
                  INCLUDEOBJECT FAR * lpinc;

                  lpinc = (INCLUDEOBJECT FAR *)obinfo.lpObData;
                  WriteScriptLine( fh, "include \"%s\"", DRF(lpinc->recFileName) );
                  WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                  break;
                  }

               case OBTYPE_MAILLIST:
                  if( dwBlinkFlags & BF_POSTCIXMAIL )
                     {
                     Mail_UpdateDirectory( fh );
                     fNeedMailDir = FALSE;
                     WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                     }
                  break;
   
               case OBTYPE_MAILUPLOAD:
                  if( dwBlinkFlags & BF_POSTCIXMAIL )
                     {
                     MAILUPLOADOBJECT FAR * lpmul;
                     char szFileName[ 144 ];
                     HNDFILE fh2;

                     lpmul = (MAILUPLOADOBJECT FAR *)obinfo.lpObData;
                     ExtractFilename( szFileName, DRF(lpmul->recFileName) );
                     if( ( fh2 = Amfile_Open( szFileName, AOF_SHARE_READ ) ) == HNDFILE_ERROR )
                        {
                        int r = IDNO;

                        if( !fQuitAfterConnect && !fQuitting )
                           {
                           wsprintf( lpTmpBuf, GS(IDS_STR197), szFileName );
                           r = fMessageBox( GetFocus(), 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION );
                           }
                        if( r == IDNO )
                           {
                           Amfile_Close( fh );
                           return( FALSE );
                           }
                        obinfo.obHdr.wFlags |= OBF_HOLD;
                        Amob_SetObInfo( lpob, &obinfo );
                        }
                     else
                        {
                        WriteToScriptFile( fh, TRUE, "mail ful" );
                        WriteScriptLine( fh, szUpload, szFileName );
                        WriteToScriptFile( fh, TRUE, szQuitMode );
                        WriteScriptLine( fh, szTestLasterrorFalse );
                        WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                        fNeedMailDir = TRUE;
                        WriteScriptLine( fh, szEndif );
                        Amfile_Close( fh2 );
                        }
                     }
                  break;
   
               case OBTYPE_MAILDOWNLOAD:
                  if( dwBlinkFlags & BF_POSTCIXMAIL )
                     {
                     MAILDOWNLOADOBJECT FAR * lpmdl;
                        
                     lpmdl = (MAILDOWNLOADOBJECT FAR *)obinfo.lpObData;
                     FmtWriteToScriptFile( fh, TRUE, "mail fdl %s", DRF(lpmdl->recCIXFileName) );
                     if( lpmdl->wFlags & FDLF_OVERWRITE )
                        WriteScriptLine( fh, "download \"%s\\%s\",overwrite", DRF(lpmdl->recDirectory), DRF(lpmdl->recFileName) );
                     else if( lpmdl->wFlags & FDLF_RENAME )
                        WriteScriptLine( fh, "download \"%s\\%s\",rename", DRF(lpmdl->recDirectory), DRF(lpmdl->recFileName) );
                     else
                        WriteScriptLine( fh, szDownloadFile, DRF(lpmdl->recDirectory), DRF(lpmdl->recFileName) );
                     WriteScriptLine( fh, szTestLasterrorFalse );
                     if( lpmdl->wFlags & FDLF_DELETE )
                        {
                        FmtWriteToScriptFile( fh, TRUE, szEraFile, DRF(lpmdl->recCIXFileName) );
                        WriteScriptLine( fh, szWaitforMail );
                        fNeedMailDir = TRUE;
                        }
                     WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                     wsprintf( lpPathBuf, GS(IDS_STR1112), DRF(lpmdl->recCIXFileName) );
                     WriteScriptLine( fh, szLog, lpPathBuf );
                     WriteScriptLine( fh, szEndif );
                     WriteToScriptFile( fh, TRUE, szQuitMode );
                     WriteScriptLine( fh, szWaitforMain );
                     }
                  break;

               case OBTYPE_MAILEXPORT:
                  if( dwBlinkFlags & BF_POSTCIXMAIL )
                     {
                     MAILEXPORTOBJECT FAR * lpmexp;

                     lpmexp = (MAILEXPORTOBJECT FAR *)obinfo.lpObData;
                     FmtWriteToScriptFile( fh, TRUE, "j %s/%s", DRF(lpmexp->recConfName), DRF(lpmexp->recTopicName) );
                     WriteScriptLine( fh, "if waitfor( \"R:\", \"M:\" ) == 0" );
                     FmtWriteToScriptFile( fh, TRUE, "mail export %s", DRF(lpmexp->recFileName) );
                     WriteScriptLine( fh, szWaitforMail );
                     if( lpmexp->wFlags & BINF_DELETE )
                        {
                        FmtWriteToScriptFile( fh, TRUE, szEraFile, DRF(lpmexp->recFileName ) );
                        WriteScriptLine( fh, szWaitforMail );
                        }
                     WriteToScriptFile( fh, TRUE, szQuitMode );   /* Exit to topic */
                     WriteToScriptFile( fh, TRUE, szQuitMode );   /* Exit to Main: prompt */
                     WriteScriptLine( fh, szWaitforMain );
                     WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                     WriteScriptLine( fh, szEndif );
                     }
                  break;

               case OBTYPE_MAILERASE:
                  if( dwBlinkFlags & BF_POSTCIXMAIL )
                     {
                     MAILERASEOBJECT FAR * lpmeo;

                     lpmeo = (MAILERASEOBJECT FAR *)obinfo.lpObData;
                     FmtWriteToScriptFile( fh, TRUE, "mail era %s", DRF(lpmeo->recFileName) );
                     WriteScriptLine( fh, "while waitfor(\"(y/n)? \", \"Ml:\") == 0" );
                     WriteToScriptFile( fh, TRUE, "y" );
                     WriteScriptLine( fh, szEndWhile );
                     WriteToScriptFile( fh, TRUE, szQuitMode );
                     WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                     fNeedMailDir = TRUE;
                     }
                  break;

               case OBTYPE_MAILRENAME:
                  if( dwBlinkFlags & BF_POSTCIXMAIL )
                     {
                     MAILRENAMEOBJECT FAR * lpmro;

                     lpmro = (MAILRENAMEOBJECT FAR *)obinfo.lpObData;
                     FmtWriteToScriptFile( fh, TRUE, "mail ren %s %s q", DRF(lpmro->recOldName), DRF(lpmro->recNewName) );
                     WriteScriptLine( fh, szWaitforMain );
                     WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                     fNeedMailDir = TRUE;
                     }
                  break;

               case OBTYPE_PUTRESUME:
                  if( dwBlinkFlags & BF_POSTCIXMSGS )
                     {
                     RESUMEOBJECT FAR * lpro;
                     char szOldName[ _MAX_PATH ];
                     char szNewName[ _MAX_PATH ];
                     HNDFILE fhOld;
                     HNDFILE fhNew;

                     /* Start with an upload.
                      */
                     WriteToScriptFile( fh, TRUE, "upl" );

                     /* Make a copy of the resumes file into
                      * <filename>.RSU and strip off the first line.
                      */
                     lpro = (RESUMEOBJECT FAR *)obinfo.lpObData;
                     strcpy( szOldName, DRF(lpro->recFileName) );
                     strcpy( szNewName, DRF(lpro->recFileName) );
                     ChangeExtension( szNewName, "RSU" );

                     /* Copy the files, stripping off the first line.
                      */
                     if( ( fhOld = Amfile_Open( szOldName, AOF_READ ) ) != HNDFILE_ERROR )
                        {
                        if( ( fhNew = Amfile_Create( szNewName, 0 ) ) != HNDFILE_ERROR )
                           {
                           LPLBF hBuffer;

                           /* Read the first line and look for the ' was last on '
                            * string. If found, skip this and the next line.
                            */
                           hBuffer = Amlbf_Open( fhOld, AOF_READ );
                           Amlbf_Read( hBuffer, lpTmpBuf, LEN_TEMPBUF-1, NULL, NULL, &fIsAmeol2Scratchpad );
                           if( NULL != strstr( lpTmpBuf, " was last on " ) )
                              Amlbf_Read( hBuffer, lpTmpBuf, LEN_TEMPBUF-1, NULL, NULL, &fIsAmeol2Scratchpad );
                           else
                              WriteScriptLine( fhNew, lpTmpBuf );

                           /* Copy the rest of the file.
                            */
                           while( Amlbf_Read( hBuffer, lpTmpBuf, LEN_TEMPBUF-1, NULL, NULL, &fIsAmeol2Scratchpad ) )
                              {
                              strcat( lpTmpBuf, "\n" );
                              Amfile_Write( fhNew, lpTmpBuf, strlen(lpTmpBuf) );
                              }
                           Amlbf_Close( hBuffer );
                           Amfile_Close( fhNew );
                           WriteScriptLine( fh, szUpload, szNewName );
                           }
                        else
                           {
                           /* Can't copy. Must regretfully upload
                            * the original file.
                            */
                           Amfile_Close( fhOld );
                           WriteScriptLine( fh, szUpload, szOldName );
                           }

                        /* Now upload the copy.
                         */
                        WriteToScriptFile( fh, TRUE, "scput res" );
                        WriteScriptLine( fh, szWaitforMain );
                        WriteScriptLine( fh, szDeleteFile, szNewName );
                        WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                        }
                     }
                  break;
   
               case OBTYPE_GETCIXLIST:
                  if( dwBlinkFlags & BF_POSTCIXMSGS )
                     {
                     WriteToScriptFile( fh, TRUE, szKillscratch );
                     WriteScriptLine( fh, szWaitforMain );
                     WriteStatusLine( fh, "Getting CIX Forums list..." );
                     WriteToScriptFile( fh, TRUE, szFileShowAll );
                     WriteScriptLine( fh, szWaitforMain );
                     if( fArcScratch )
                        {
                        WriteStatusLine( fh, "Compressing CIX Forums list..." );
                        WriteToScriptFile( fh, TRUE, szArcscratch );
                        WriteScriptLine( fh, szWaitforMain );
                        }
                     WriteToScriptFile( fh, TRUE, szDownload );
                     WriteScriptLine( fh, "download \"%s%s\"", (LPSTR)pszAmeolDir, (LPSTR)szCixLstFile );  // VistaAlert
                     WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                     }
                  break;
   
               case OBTYPE_GETUSERLIST:
                  if( dwBlinkFlags & BF_POSTCIXMSGS )
                     {
                     WriteToScriptFile( fh, TRUE, szKillscratch );
                     WriteScriptLine( fh, szWaitforMain );
                     WriteStatusLine( fh, "Getting CIX Subscribers list..." );
                     WriteToScriptFile( fh, TRUE, "file show par" );
                     WriteScriptLine( fh, szWaitforMain );
                     if( fArcScratch )
                        {
                        WriteStatusLine( fh, "Compressing CIX Subscribers list..." );
                        WriteToScriptFile( fh, TRUE, szArcscratch );
                        WriteScriptLine( fh, szWaitforMain );
                        }
                     WriteToScriptFile( fh, TRUE, szDownload );
                     WriteScriptLine( fh, "download \"%s%s\"", (LPSTR)pszAmeolDir, (LPSTR)szUsersLstFile ); // VistaAlert
                     WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                     }
                  break;

               case OBTYPE_FUL:
                  if( dwBlinkFlags & BF_POSTCIXMSGS )
                     {
                     FULOBJECT FAR * lpful;
                     char szFileName[ 144 ];
                     HNDFILE fh2;
                     PCL pcl;
   
                     lpful = (FULOBJECT FAR *)obinfo.lpObData;
                     ExtractFilename( szFileName, DRF(lpful->recFileName) );
                     if( ( fh2 = Amfile_Open( szFileName, AOF_SHARE_READ ) ) == HNDFILE_ERROR )
                        {
                        int r = IDNO;
   
                        if( !fQuitAfterConnect && !fQuitting )
                           {
                           wsprintf( lpTmpBuf, GS(IDS_STR197), DRF(lpful->recFileName) );
                           r = fMessageBox( GetFocus(), 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION );
                           }
                        if( r == IDNO )
                           {
                           Amfile_Close( fh );
                           return( FALSE );
                           }
                        obinfo.obHdr.wFlags |= OBF_HOLD;
                        Amob_SetObInfo( lpob, &obinfo );
                        }
                     else
                        {
                        DWORD dwSize;
                        AM_DATE date;

                        WriteToScriptFile( fh, TRUE, szKillscratch );
                        WriteScriptLine( fh, szWaitforMain );
                        FmtWriteToScriptFile( fh, TRUE, "j %s/%s", DRF(lpful->recConfName), DRF(lpful->recTopicName) );
                        WriteToScriptFile( fh, TRUE, "ful" );
                        WriteScriptLine( fh, szUpload, szFileName );
                        WriteScriptLine( fh, szTestLasterrorFalse );
                        pcl = Amdb_OpenFolder( NULL, DRF(lpful->recConfName) );
                        if( !Amdb_IsModerator( pcl, szCIXNickname ) )
                           {
                           WriteScriptLine( fh, "while waitfor(\" (y/n)? \", \"subject: \") == 0" );
                           WriteScriptLine( fh, "put \"y\"" );
                           WriteScriptLine( fh, szEndWhile );
                           }
                        else
                           {
                           LPSTR lpModList;
                           UINT wSize;

                           INITIALISE_PTR( lpModList );
                           wSize = Amdb_GetModeratorList( pcl, NULL, 0xFFFF );
                           if( wSize > 0 )
                              if( fNewMemory( &lpModList, wSize ) )
                              {
   
                              /* Get the list again.
                               */
                              Amdb_GetModeratorList( pcl, lpModList, wSize );
                              WriteToScriptFile( fh, TRUE, szEnterMail );
                              FmtWriteToScriptFile( fh, TRUE, "to %s", lpModList );
                              }
                           }
   
                           wsprintf( sz, "Upload to cix:%s/%s", DRF(lpful->recConfName), DRF(lpful->recTopicName) );
                           WriteToScriptFile( fh, TRUE, sz );
      
                           /* Include the file name */
                           WriteToScriptFile( fh, TRUE, "" );
                           wsprintf( sz, "   Filename: %s", (LPSTR)GetFileBasename( szFileName ) );
                           WriteToScriptFile( fh, TRUE, sz );

                           /* Hotlink */
                           wsprintf( sz, "    Hotlink: cixfile:%s/%s:%s", DRF(lpful->recConfName), DRF(lpful->recTopicName) ,(LPSTR)GetFileBasename( szFileName ) );
                           WriteToScriptFile( fh, TRUE, sz );

                           /* Include the file size in the description */
                           dwSize = Amfile_SetPosition( fh2, 0L, 2 );
                           wsprintf( sz, "  File size: %s bytes", FormatThousands( dwSize, NULL ) );
                           WriteToScriptFile( fh, TRUE, sz );
      
                           /* Include your own name */
                           wsprintf( sz, "Contributor: %s", (LPSTR)szCIXNickname );
                           WriteToScriptFile( fh, TRUE, sz );
      
                           /* Include the upload date */
                           Amdate_GetCurrentDate( &date );
                           wsprintf( sz, "       Date: %s", Amdate_FormatLongDate( &date, NULL ) );
                           WriteToScriptFile( fh, TRUE, sz );
      
                           /* Finally, include the description */
                           if( *DRF(lpful->recText) )
                              {
                              WriteToScriptFile( fh, TRUE, "" );
                              WriteToScriptFile( fh, TRUE, "Description:" );
                              FormatText( fh, DRF(lpful->recText), FMT_TXT_CIX|FMT_TXT_CMD|FMT_FORCED, 69 );
                              }
                           else
                              WriteToScriptFile( fh, TRUE, "." );
                           if( fCCMailToSender && !Amdb_IsModerator( pcl, szCIXNickname ) )
                              {
                              wsprintf( sz, "to %s", (LPSTR)szCIXNickname );
                              WriteToScriptFile( fh, TRUE, sz );
                              }
                           WriteToScriptFile( fh, TRUE, "send" );
                           WriteScriptLine( fh, szWaitforMail );
                           WriteToScriptFile( fh, TRUE, szQuitMode );
                           }
                        WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                        WriteScriptLine( fh, szEndif );
                        Amfile_Close( fh2 );
                        WriteToScriptFile( fh, TRUE, szQuitMode );
                        WriteScriptLine( fh, szWaitforMain );
                        }
                  break;
   
               case OBTYPE_DOWNLOAD:
                  if( dwBlinkFlags & BF_POSTCIXMSGS )
                     {
                     FDLOBJECT FAR * lpfdl;

                     lpfdl = (FDLOBJECT FAR *)obinfo.lpObData;
                     FmtWriteToScriptFile( fh, TRUE, "j %s/%s", DRF(lpfdl->recConfName), DRF(lpfdl->recTopicName) );
                     if( lpfdl->wFlags & FDLF_JOINANDRESIGN )
                        {
                        WriteScriptLine( fh, "if waitfor(\"(y/n)? \", \"R:\") == 0" );
                        WriteToScriptFile( fh, TRUE, "y" );
                        WriteScriptLine( fh, szEndif );
                        }
                     else
                        WriteScriptLine( fh, "if waitfor(\"R:\", \"M:\") == 0" );
                     FmtWriteToScriptFile( fh, TRUE, "fdl %s", DRF(lpfdl->recCIXFileName) );
                     if( lpfdl->wFlags & FDLF_OVERWRITE )
                        WriteScriptLine( fh, "download \"%s\\%s\",overwrite", DRF(lpfdl->recDirectory), DRF(lpfdl->recFileName) );
                     else if( lpfdl->wFlags & FDLF_RENAME )
                        WriteScriptLine( fh, "download \"%s\\%s\",rename", DRF(lpfdl->recDirectory), DRF(lpfdl->recFileName) );
                     else
                        WriteScriptLine( fh, szDownloadFile, DRF(lpfdl->recDirectory), DRF(lpfdl->recFileName) );
                     WriteToScriptFile( fh, TRUE, szQuitMode );
                     if( lpfdl->wFlags & FDLF_JOINANDRESIGN )
                        {
                        FmtWriteToScriptFile( fh, TRUE, "resi %s/%s", DRF(lpfdl->recConfName), DRF(lpfdl->recTopicName) );
                        WriteScriptLine( fh, szWaitforMain );
                        }
                     WriteScriptLine( fh, szTestLasterrorFalse );
                     WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                     WriteScriptLine( fh, szRecord, DRF(lpfdl->recConfName), DRF(lpfdl->recTopicName), DRF(lpfdl->recDirectory), DRF(lpfdl->recCIXFileName) );
                     WriteScriptLine( fh, szEndif );
                     if( !( lpfdl->wFlags & FDLF_JOINANDRESIGN ) )
                        WriteScriptLine( fh, szEndif );
                     }
                  break;
               }
            }
         }

   /* We need to refresh the local copy of the mail directory
    * if we uploaded, renamed or erased in the CIX mail directory
    */
   if( fNeedMailDir )
      Mail_UpdateDirectory( fh );

   /* Process the Usenet gateway
    */
   GenerateCIXUsenetScript( fh, dwBlinkFlags );

   /* Clean up in the end
    */
   if( !TestF( dwBlinkFlags, BF_STAYONLINE ) )
      {
      InsertScript( fh, szCixTermScr );
      WriteStatusLine( fh, GS(IDS_STR1015) );
      WriteToScriptFile( fh, TRUE, szBye );
      }
   Amfile_Close( fh );
   return( TRUE );
}
#pragma optimize("", on)

/* This function scans the out-basket and determines whether there
 * are any CIX Usenet actions. No point in going into the CIX
 * Usenet service otherwise.
 */
BOOL FASTCALL IsCixnewsActions( void )
{
   BOOL fUseCixnews;
   LPOB lpob;

   fUseCixnews = FALSE;
   for( lpob = Amob_GetOb( NULL ); !fUseCixnews && lpob; lpob = Amob_GetOb( lpob ) )
      {
      OBINFO obinfo;

      Amob_GetObInfo( lpob, &obinfo );
      if( !( obinfo.obHdr.wFlags & (OBF_HOLD|OBF_OPEN) ) )
         switch( obinfo.obHdr.clsid )
            {
            case OBTYPE_USENETJOIN:
            case OBTYPE_USENETRESIGN:
               fUseCixnews = TRUE;
               break;

            case OBTYPE_CANCELARTICLE: {
               CANCELOBJECT FAR * lpco;

               /* Dereference the object data structure
                */
               lpco = (CANCELOBJECT FAR *)obinfo.lpObData;
               fUseCixnews = IsCixnewsNewsgroup( DRF(lpco->recNewsgroup) );
               break;
               }

            case OBTYPE_ARTICLEMESSAGE: {
               ARTICLEOBJECT FAR * lpao;

               /* Dereference the object data structure
                */
               lpao = (ARTICLEOBJECT FAR *)obinfo.lpObData;
               fUseCixnews = IsCixnewsNewsgroup( DRF(lpao->recNewsgroup) );
               break;
               }

            case OBTYPE_GETNEWSGROUPS: {
               NEWSGROUPSOBJECT FAR * lpng;

               lpng = (NEWSGROUPSOBJECT FAR *)obinfo.lpObData;
               fUseCixnews = strcmp( DRF(lpng->recServerName), szCixnews ) == 0;
               break;
               }

            case OBTYPE_GETARTICLE: {
               GETARTICLEOBJECT FAR * lpga;

               lpga = (GETARTICLEOBJECT FAR *)obinfo.lpObData;
               fUseCixnews = IsCixnewsNewsgroup( DRF(lpga->recNewsgroup) );
               break;
               }

            case OBTYPE_GETTAGGED: {
               GETTAGGEDOBJECT FAR * lpgt;
   
               lpgt = (GETTAGGEDOBJECT FAR *)obinfo.lpObData;
               fUseCixnews = IsCixnewsInFolderList( DRF(lpgt->recFolder) );
               break;
               }

            case OBTYPE_GETNEWARTICLES: {
               NEWARTICLESOBJECT FAR * lprh;

               /* Derefrence the data structure.
                */
               lprh = (NEWARTICLESOBJECT FAR *)obinfo.lpObData;
               fUseCixnews = IsCixnewsInFolderList( DRF(lprh->recFolder) );
               break;
               }
            }
      }
   return( fUseCixnews );
}

/* This function determines whether any newsgroup in the folder
 * list use Cixnews as the news server.
 */
BOOL FASTCALL IsCixnewsInFolderList( HPSTR hpszFolderList )
{
   NEWSFOLDERLIST nfl;

   /* Build a list of PTLs for each folder referenced then
    * scan the list.
    */
   if( BuildFolderList( hpszFolderList, &nfl ) )
      {
      register int c;

      /* Loop for each newsgroup in the list
       */
      for( c = 0; c < nfl.cTopics; ++c )
         {
         TOPICINFO topicinfo;

         Amdb_GetTopicInfo( nfl.ptlTopicList[ c ], &topicinfo );
         if( IsCixnewsNewsgroup( topicinfo.szTopicName ) )
            {
            FreeMemory( (LPVOID)&nfl.ptlTopicList );
            return( TRUE );
            }
         }
      FreeMemory( (LPVOID)&nfl.ptlTopicList );
      }
   return( FALSE );
}

/* This function creates the script commands that process CIX Usenet
 * access
 */
void FASTCALL GenerateCIXUsenetScript( HNDFILE fh, DWORD dwBlinkFlags )
{
   if( (dwBlinkFlags & BF_CIXUSENET) && OkayUsenetAccess( (char *)szCixnews ) && IsCixnewsActions() )
      {
      BOOL fIssuedOptZWnd;

      /* Open the gate.
       */
      fIssuedOptZWnd = FALSE;
      WriteStatusLine( fh, GS(IDS_STR1010) );
      FmtWriteToScriptFile( fh, TRUE, "go %s", (LPSTR)szUsenetGateway );
      WriteScriptLine( fh, "if waitfor(\"^JNewsnet:\", \"M:\") == 0" );

      /* Deal with recovery
       */
      if( TestF( dwBlinkFlags, BF_CIXRECOVER ) )
         {
         WriteScriptLine( fh, szSetRecover );
         WriteToScriptFile( fh, TRUE, "download messages" );
         WriteScriptLine( fh, szDownloadFile, (LPSTR)pszScratchDir, (LPSTR)szUsenetScratchPad );
         WriteScriptLine( fh, szTestLasterrorTrue );
         WriteToScriptFile( fh, TRUE, "quit" );
         WriteScriptLine( fh, "if waitfor( \"(Y/N) \", \"M:\" ) == 0" );
         WriteScriptLine( fh, "put \"N\"" );
         WriteScriptLine( fh, "waitfor \"M:\"" );
         WriteScriptLine( fh, szEndif );
         WriteScriptLine( fh, szLog, GS(IDS_STR1095) );
         WriteToScriptFile( fh, TRUE, szBye );
         WriteScriptLine( fh, "end" );
         WriteScriptLine( fh, szEndif );
         WriteScriptLine( fh, szNoRecover );
         WriteScriptLine( fh, "rename \"%s\\%s\" \"%s\\%s\"", (LPSTR)pszScratchDir, (LPSTR)szUsenetScratchPad, (LPSTR)pszScratchDir, (LPSTR)szUsenetRcvScratchPad );
         WriteScriptLine( fh, "import \"%s\\%s\"", (LPSTR)pszScratchDir, (LPSTR)szUsenetRcvScratchPad );
         }

      /* Vape any existing scratchpad.
       */
      WriteToScriptFile( fh, TRUE, szKillscratch );
      WriteScriptLine( fh, szWaitforNewsnet );

      /* Process Usenet commands.
       */
      if( dwBlinkFlags & BF_POSTCIXNEWS )
         {
         LPOB lpob;

         /* First pass - deal with posting new messages, replied and
          * cancellations.
          */
         fNeedCopy = TRUE;
         for( lpob = Amob_GetOb( NULL ); lpob; lpob = Amob_GetOb( lpob ) )
            {
            OBINFO obinfo;

            Amob_GetObInfo( lpob, &obinfo );
            if( !( obinfo.obHdr.wFlags & (OBF_HOLD|OBF_OPEN) ) )
               switch( obinfo.obHdr.clsid )
                  {
                  case OBTYPE_CANCELARTICLE: {
                     CANCELOBJECT FAR * lpco;
                     
                     /* Dereference the object data structure
                      */
                     lpco = (CANCELOBJECT FAR *)obinfo.lpObData;
                     if( IsCixnewsNewsgroup( DRF(lpco->recNewsgroup) ) )
                        {
                        WriteToScriptFile( fh, FALSE, "#! rnews 666" );
                        WriteToScriptFile( fh, FALSE, "From:" );
                        WriteToScriptFile( fh, FALSE, "Path:" );
                        FmtWriteToScriptFile( fh, FALSE, "Newsgroups: %s", DRF(lpco->recNewsgroup) );
                        FmtWriteToScriptFile( fh, FALSE, "Control: cancel <%s>", DRF(lpco->recReply) );
                        WriteToScriptFile( fh, FALSE, "Message-ID:" );
                        FmtWriteToScriptFile( fh, FALSE, "Subject: cmsg cancel <%s>", DRF(lpco->recReply) );
                        WriteToScriptFile( fh, FALSE, "Date:" );
                        WriteToScriptFile( fh, FALSE, "" );
                        WriteToScriptFile( fh, FALSE, "Article cancelled" );
                        }
                     break;
                     }

                  case OBTYPE_ARTICLEMESSAGE: {
                     ARTICLEOBJECT FAR * lpao;

                     /* Dereference the object data structure
                      */
                     lpao = (ARTICLEOBJECT FAR *)obinfo.lpObData;
                     if( IsCixnewsNewsgroup( DRF(lpao->recNewsgroup) ) )
                        SendNewsArticle( fh, lpao );
                     break;
                     }
                  }
            }

         /* If we put any messages then upload them to the
          * CIX Usenet gateway.
          */
         if( !fNeedCopy )
            {
            WriteScriptLine( fh, "<<" );
            if( fAntiSpamCixnews )
            {
               WriteToScriptFile( fh, TRUE, "opt antispam-from" );
               WriteScriptLine( fh, szWaitforNewsnet );
            }
            WriteToScriptFile( fh, TRUE, "upload messages" );
            Amuser_GetActiveUserDirectory( szScriptScrPath, _MAX_FNAME );
            strcat( szScriptScrPath, "\\" );
            strcat( szScriptScrPath, "script.scr" );
            wsprintf( lpTmpBuf, "upload \"%s\"", szScriptScrPath );
            WriteScriptLine( fh, lpTmpBuf );
            WriteScriptLine( fh, szSuccess, 3 );
            }
         fNeedCopy = FALSE;

         /* Second pass - deal with newsgroup downloads.
          */
         for( lpob = Amob_GetOb( NULL ); lpob; lpob = Amob_GetOb( lpob ) )
            {
            OBINFO obinfo;

            Amob_GetObInfo( lpob, &obinfo );
            if( !( obinfo.obHdr.wFlags & (OBF_HOLD|OBF_OPEN) ) )
               switch( obinfo.obHdr.clsid )
                  {
                  case OBTYPE_USENETJOIN: {
                     JOINUNETOBJECT FAR * lpruo;

                     lpruo = (JOINUNETOBJECT FAR *)obinfo.lpObData;
                     FmtWriteToScriptFile( fh, TRUE, "join %s %u", DRF(lpruo->recGroupName), lpruo->wArticles );
                     WriteScriptLine( fh, szWaitforNewsnet );
                     WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                     break;
                     }

                  case OBTYPE_USENETRESIGN: {
                     RESIGNUNETOBJECT FAR * lpruo;

                     lpruo = (RESIGNUNETOBJECT FAR *)obinfo.lpObData;
                     FmtWriteToScriptFile( fh, TRUE, "resign %s", DRF(lpruo->recGroupName) );
                     WriteScriptLine( fh, szWaitforNewsnet );
                     WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                     break;
                     }

                  case OBTYPE_GETNEWSGROUPS: {
                     NEWSGROUPSOBJECT FAR * lpng;

                     lpng = (NEWSGROUPSOBJECT FAR *)obinfo.lpObData;
                     if( strcmp( DRF(lpng->recServerName), szCixnews ) == 0 )
                        {
                        /* Retrieve the full CIX Usenet newsgroup list.
                         */
                        WriteToScriptFile( fh, TRUE, szKillscratch );
                        WriteScriptLine( fh, szWaitforNewsnet );
                        WriteToScriptFile( fh, TRUE, "file newsgrouplist" );
                        WriteScriptLine( fh, szWaitforNewsnet );
                        if( fArcScratch )
                           {
                           WriteToScriptFile( fh, TRUE, "arcbatch" );
                           WriteScriptLine( fh, szWaitforNewsnet );
                           }
                        if( !fIssuedOptZWnd )
                           {
                           FmtWriteToScriptFile( fh, TRUE, "opt zwindow %u", uZWindowSize );
                           WriteScriptLine( fh, szWaitforNewsnet );
                           fIssuedOptZWnd = TRUE;
                           }
                        WriteToScriptFile( fh, TRUE, "download messages" );
                        WriteScriptLine( fh, "download \"%s%s\"", (LPSTR)pszAmeolDir, (LPSTR)szUsenetLstFile ); // VistaAlert
                        WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                        }
                     break;
                     }
                  }
            }
         }

      /* Process commands that file to the Usenet scratchpad.
       */
      if( dwBlinkFlags & (BF_GETCIXNEWS) )
         {
         NEWSSERVERINFO nsi;
         int cHeaders;
         LPOB lpob;

         /* If a limit to the size of the Usenet scratchpad
          * has been set, convey that limit.
          */
         GetUsenetServerDetails( (char *)szCixnews, &nsi );
         if( nsi.dwUsenetBatchsize )
            {
            FmtWriteToScriptFile( fh, TRUE, szSetBatchsize, nsi.dwUsenetBatchsize );
            WriteScriptLine( fh, szWaitforNewsnet );
            }

         /* First step, process any out-basket actions that add new
          * messages to the Usenet scratchpad.
          */
         cHeaders = 0;
         if( dwBlinkFlags & BF_GETCIXNEWS )
            for( lpob = Amob_GetOb( NULL ); lpob; lpob = Amob_GetOb( lpob ) )
               {
               OBINFO obinfo;

               Amob_GetObInfo( lpob, &obinfo );
               if( !( obinfo.obHdr.wFlags & (OBF_HOLD|OBF_OPEN) ) )
                  switch( obinfo.obHdr.clsid )
                     {
                     case OBTYPE_GETNEWARTICLES: {
                        NEWARTICLESOBJECT FAR * lprh;
                        NEWSFOLDERLIST nfl;

                        /* Derefrence the data structure.
                         */
                        lprh = (NEWARTICLESOBJECT FAR *)obinfo.lpObData;
                        Amob_DerefObject( &lprh->recFolder );
                        if( BuildFolderList( lprh->recFolder.hpStr, &nfl ) )
                           {
                           TOPICINFO topicinfo;
                           int cFullArticles;
                           int cTotalCixnews;
                           register int c;

                           /* Optimisation. Do a one-pass and if ALL cixnews newsgroups
                            * are marked for retrieving all headers, do it the quick way.
                            */
                           cHeaders = 0;
                           cFullArticles = 0;
                           cTotalCixnews = 0;
                           for( c = 0; c < nfl.cTopics; ++c )
                              {
                              Amdb_GetTopicInfo( nfl.ptlTopicList[ c ], &topicinfo );
                              if( IsCixnewsNewsgroup( topicinfo.szTopicName ) )
                                 {
                                 if( !( topicinfo.dwFlags & TF_READFULL ) )
                                    ++cHeaders;
                                 else
                                    ++cFullArticles;
                                 ++cTotalCixnews;
                                 }
                              }

                           /* The FAST route!
                            */
                           if( cHeaders > 0 )
                              {
                              WriteStatusLine( fh, GS(IDS_STR1011) );
                              WriteToScriptFile( fh, TRUE, "echo @!__hdr__!@" );
                              WriteScriptLine( fh, szWaitforNewsnet );
                              WriteToScriptFile( fh, TRUE, "opt format header" );
                              WriteScriptLine( fh, szWaitforNewsnet );
                              if( cHeaders == cTotalCixnews )
                                 {
                                 /* Take the fast route.
                                  */
                                 WriteToScriptFile( fh, TRUE, "batch all endbatch" );
                                 WriteScriptLine( fh, szWaitforNewsnet );
                                 }
                              else
                                 {
                                 /* First deal with all newsgroups for which we only
                                  * retrieve new headers.
                                  */
                                 for( c = 0; c < nfl.cTopics; ++c )
                                    {
                                    Amdb_GetTopicInfo( nfl.ptlTopicList[ c ], &topicinfo );
                                    if( IsCixnewsNewsgroup( topicinfo.szTopicName ) )
                                       if( !( topicinfo.dwFlags & TF_READFULL ) )
                                          {
                                          FmtWriteToScriptFile( fh, TRUE, szBatchNewsgroup, topicinfo.szTopicName );
                                          WriteScriptLine( fh, szWaitforNewsnet );
                                          }
                                    }
                                 }

                              /* Clean up at the end
                               */
                              WriteToScriptFile( fh, TRUE, "echo @!__hdr__!@" );
                              WriteScriptLine( fh, szWaitforNewsnet );
                              }

                           /* Do full articles, if any.
                            */
                           if( cFullArticles > 0 )
                              {
                              /* Switch to full format if we were previously
                               * in header format.
                               */
                              if( cHeaders )
                                 {
                                 WriteToScriptFile( fh, TRUE, "opt format rnews" );
                                 WriteScriptLine( fh, szWaitforNewsnet );
                                 }

                              /* Again, take the fast route if we download
                               * full articles for every newsgroup.
                               */
                              if( cFullArticles == cTotalCixnews )
                                 {
                                 WriteToScriptFile( fh, TRUE, "batch all endbatch" );
                                 WriteScriptLine( fh, szWaitforNewsnet );
                                 }
                              else
                                 {
                                 /* First deal with all newsgroups for which we only
                                  * retrieve new headers.
                                  */
                                 for( c = 0; c < nfl.cTopics; ++c )
                                    {
                                    TOPICINFO topicinfo;

                                    Amdb_GetTopicInfo( nfl.ptlTopicList[ c ], &topicinfo );
                                    if( IsCixnewsNewsgroup( topicinfo.szTopicName ) )
                                       if( topicinfo.dwFlags & TF_READFULL )
                                          {
                                          FmtWriteToScriptFile( fh, TRUE, szBatchNewsgroup, topicinfo.szTopicName );
                                          WriteScriptLine( fh, szWaitforNewsnet );
                                          }
                                    }
                                 }
                              }
                           FreeMemory( (LPVOID)&nfl.ptlTopicList );
                           WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                           }
                        break;
                        }
                     }
               }

         /* Next step, look through the list of subscribed newsgroups for any
          * which contain tagged articles.
          */
         if( dwBlinkFlags & BF_GETCIXTAGGEDNEWS )
            for( lpob = Amob_GetOb( NULL ); lpob; lpob = Amob_GetOb( lpob ) )
               {
               OBINFO obinfo;

               Amob_GetObInfo( lpob, &obinfo );
               if( !( obinfo.obHdr.wFlags & (OBF_HOLD|OBF_OPEN) ) )
                  switch( obinfo.obHdr.clsid )
                     {
                  #ifdef OBTYPE_GETCIXNEWSLIST_IMPLEMENTED
                     case OBTYPE_GETCIXNEWSLIST:
                        WriteToScriptFile( fh, TRUE, "echo %s", (LPSTR)szNewsgroupStartEcho );
                        WriteScriptLine( fh, szWaitforNewsnet );
                        WriteToScriptFile( fh, TRUE, "file newsgroups" );
                        WriteScriptLine( fh, szWaitforNewsnet );
                        WriteToScriptFile( fh, TRUE, "echo %s", (LPSTR)szNewsgroupEndEcho );
                        WriteScriptLine( fh, szWaitforNewsnet );
                        WriteScriptLine( fh, szSuccess, lpob->id );
                        break;
                  #endif

                     case OBTYPE_GETARTICLE: {
                        GETARTICLEOBJECT FAR * lpga;
                        PTL ptl;

                        lpga = (GETARTICLEOBJECT FAR *)obinfo.lpObData;
                        Amob_DerefObject( &lpga->recNewsgroup );
                        if( IsCixnewsNewsgroup( lpga->recNewsgroup.hpStr ) )
                           {
                           ptl = PtlFromNewsgroup( lpga->recNewsgroup.hpStr );
                           if( NULL != ptl )
                              {
                              TOPICINFO topicinfo;

                              /* Get just one article.
                               */
                              if( cHeaders )
                                 {
                                 WriteToScriptFile( fh, TRUE, "opt format rnews" );
                                 WriteScriptLine( fh, szWaitforNewsnet );
                                 cHeaders = 0;
                                 }
                              Amdb_GetTopicInfo( ptl, &topicinfo );
                              FmtWriteToScriptFile( fh, TRUE, szBatchArticle, topicinfo.szTopicName, lpga->dwMsg );
                              WriteScriptLine( fh, szWaitforNewsnet );
                              }
                           WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                           }
                        break;
                        }

                     case OBTYPE_GETTAGGED: {
                        GETTAGGEDOBJECT FAR * lpgt;
                        NEWSFOLDERLIST nfl;

                        lpgt = (GETTAGGEDOBJECT FAR *)obinfo.lpObData;
                        Amob_DerefObject( &lpgt->recFolder );
                        if( BuildFolderList( lpgt->recFolder.hpStr, &nfl ) )
                           {
                           register int c;
                           BOOL fHasHeader;

                           /* Loop for each newsgroup in the list
                            */
                           WriteStatusLine( fh, GS(IDS_STR1012) );
                           fHasHeader = FALSE;
                           for( c = 0; c < nfl.cTopics; ++c )
                              {
                              TOPICINFO topicinfo;
                              PTH pthTagged;

                              Amdb_GetTopicInfo( nfl.ptlTopicList[ c ], &topicinfo );
                              if( IsCixnewsNewsgroup( topicinfo.szTopicName ) )
                                 {
                                 pthTagged = NULL;
                                 Amdb_LockTopic( nfl.ptlTopicList[ c ] );
                                 while( pthTagged = Amdb_GetTagged( nfl.ptlTopicList[ c ], pthTagged ) )
                                    {
                                    MSGINFO msginfo;

                                    if( !fHasHeader )
                                       {
                                       if( cHeaders )
                                          {
                                          WriteToScriptFile( fh, TRUE, "opt format rnews" );
                                          WriteScriptLine( fh, szWaitforNewsnet );
                                          cHeaders = 0;
                                          }
                                       FmtWriteToScriptFile( fh, TRUE, "echo @!__tagged__!@ %s", topicinfo.szTopicName );
                                       WriteScriptLine( fh, szWaitforNewsnet );
                                       fHasHeader = TRUE;
                                       }
                                    Amdb_GetMsgInfo( pthTagged, &msginfo );
                                    FmtWriteToScriptFile( fh, TRUE, szBatchArticle, topicinfo.szTopicName, msginfo.dwMsg );
                                    WriteScriptLine( fh, szWaitforNewsnet );
                                    }
                                 Amdb_UnlockTopic( nfl.ptlTopicList[ c ] );
                                 if( fHasHeader )
                                    {
                                    WriteToScriptFile( fh, TRUE, "echo @!__tagged__!@" );
                                    WriteScriptLine( fh, szWaitforNewsnet );
                                    fHasHeader = FALSE;
                                    }
                                 }
                              }
                           FreeMemory( (LPVOID)&nfl.ptlTopicList );
                           }
                        WriteScriptLine( fh, szSuccess, obinfo.obHdr.id );
                        break;
                        }
                     }
               }

         /* Archive the scratchpad if required.
          */
         if( fArcScratch )
            {
            WriteStatusLine( fh, GS(IDS_STR1013) );
            WriteToScriptFile( fh, TRUE, "arcbatch" );
            WriteScriptLine( fh, szWaitforNewsnet );
            }

         /* Issue opt zwindow if required.
          */
         if( !fIssuedOptZWnd )
            {
            FmtWriteToScriptFile( fh, TRUE, "opt zwindow %u", uZWindowSize );
            WriteScriptLine( fh, szWaitforNewsnet );
            fIssuedOptZWnd = TRUE;
            }

         /* Finally download the scratchpad
          */
         WriteScriptLine( fh, szSetRecover );
         WriteToScriptFile( fh, TRUE, "download messages" );
         WriteScriptLine( fh, "download \"%s\\%s\"", (LPSTR)pszScratchDir, (LPSTR)szUsenetScratchPad );
         WriteScriptLine( fh, szTestLasterrorFalse );
         WriteScriptLine( fh, szNoRecover );
         WriteScriptLine( fh, szEndif );
         WriteScriptLine( fh, "import \"%s\\%s\"", (LPSTR)pszScratchDir, (LPSTR)szUsenetScratchPad );
         }

      /* Here, we exit from the CIX Usenet
       * subsystem.
       */
      WriteStatusLine( fh, GS(IDS_STR1014) );
      WriteToScriptFile( fh, TRUE, "quit" );
      WriteScriptLine( fh, "if waitfor( \"(Y/N) \", \"M:\" ) == 0" );
      WriteScriptLine( fh, "put \"N\"" );
      WriteScriptLine( fh, "waitfor \"M:\"" );
      WriteScriptLine( fh, szEndif );
      Amuser_GetActiveUserDirectory( szScriptScrPath, _MAX_FNAME );
      strcat( szScriptScrPath, "\\" );
      strcat( szScriptScrPath, "script.scr" );
      wsprintf( lpTmpBuf, "delete \"%s\"", szScriptScrPath );
      WriteScriptLine( fh, lpTmpBuf );
      WriteScriptLine( fh, szEndif );
      }
}

/* This function formats a CIX Usenet news article.
 */
void FASTCALL SendNewsArticle( HNDFILE fh, ARTICLEOBJECT FAR * lpao )
{
   if( *DRF(lpao->recAttachments) )
      {
      HPSTR hpEncodedParts;
      LPSTR lpStrBoundary;
      char szSubject[ 100 ];

      /* Create the MIME boundary.
       */
      if( lpao->wEncodingScheme == ENCSCH_UUENCODE )
         lpStrBoundary = NULL;
      else
         {
         lpStrBoundary = CreateMimeBoundary();
         if( NULL == lpStrBoundary )
            return;
         }

      /* Encode the file(s) and return now if any error.
       */
      hpEncodedParts = EncodeFiles( hwndFrame, DRF(lpao->recAttachments), lpStrBoundary, lpao->wEncodingScheme, &cEncodedParts );
      if( NULL == hpEncodedParts )
         return;
      nThisEncodedPart = 0;
      if( !fSeparateEncodedCover )
         ++nThisEncodedPart;

      /* Insert cover text.
       */
      wsprintf( szSubject, "%s (%u/%u)", DRF(lpao->recSubject), nThisEncodedPart, cEncodedParts );
      CreateCIXNewsnetHeader( fh, lpao, szSubject, lpStrBoundary );
      if( NULL != lpStrBoundary )
         {
         char sz[ 80 ];

         wsprintf( sz, "--%s", lpStrBoundary );
         WriteToScriptFile( fh, FALSE, sz );
         /*!!SM!!*/
//       WriteToScriptFile( fh, FALSE, "Content-Type: text/plain; charset=\"us-ascii\"" );
         WriteToScriptFile( fh, FALSE, "Content-Type: text/plain; charset=\"iso-8859-1\"" );
         WriteToScriptFile( fh, FALSE, "" );
         }
      FormatText( fh, DRF(lpao->recText), FMT_TXT_USENET, iWrapCol );

      /* If we encode on the first page, append the first block of
       * encoded text.
       */
      if( !fSeparateEncodedCover )
         {
         FormatText( fh, hpEncodedParts, FMT_TXT_USENET, iWrapCol );
         hpEncodedParts += strlen(hpEncodedParts) + 1;
         }
      ++nThisEncodedPart;

      /* Now loop for each part and send a mail message.
       * At this point, hpEncodedParts points to a huge (>64K) memory
       * block of encoded file, each part delimited by a NULL.
       */
      while( nThisEncodedPart <= cEncodedParts )
         {
         wsprintf( szSubject, "%s (%u/%u)", DRF(lpao->recSubject), nThisEncodedPart, cEncodedParts );
         CreateCIXNewsnetHeader( fh, lpao, szSubject, lpStrBoundary );
         FormatText( fh, hpEncodedParts, FMT_TXT_USENET, iWrapCol );
         hpEncodedParts += strlen(hpEncodedParts) + 1;
         ++nThisEncodedPart;
         }
      }
   else
      {
      CreateCIXNewsnetHeader( fh, lpao, DRF(lpao->recSubject), NULL );
      FormatText( fh, DRF(lpao->recText), FMT_TXT_USENET, iWrapCol );
      }
}

/* This function writes the header for a Usenet news article.
 */
void FASTCALL CreateCIXNewsnetHeader( HNDFILE fh, ARTICLEOBJECT FAR * lpao, char * pszSubject, char * lpStrBoundary )
{
   register int c;

   WriteToScriptFile( fh, FALSE, "#! rnews 666" );
   WriteToScriptFile( fh, FALSE, "From:" );
   WriteToScriptFile( fh, FALSE, "Path:" );
   FmtWriteToScriptFile( fh, FALSE, "Newsgroups: %s", DRF(lpao->recNewsgroup) );
   WriteToScriptFile( fh, FALSE, "Message-ID:" );
   LoadUsenetHeaders();
   for( c = 0; c < cNewsHeaders; ++c )
      FmtWriteToScriptFile( fh, FALSE, "%s: %s", uNewsHeaders[ c ].szType, uNewsHeaders[ c ].szValue );
   if( 0L != lpao->dwReply )
      FmtWriteToScriptFile( fh, FALSE, "References: <%s>", DRF(lpao->recReply) );
   FmtWriteToScriptFile( fh, FALSE, "Subject: %s", pszSubject );
   WriteToScriptFile( fh, FALSE, "Date:" );
   if( lpStrBoundary != NULL )
      {
      WriteToScriptFile( fh, FALSE, "Mime-Version: 1.0" );
      wsprintf( lpTmpBuf, "Content-Type: multipart/mixed; boundary=\"%s\"", lpStrBoundary );
      WriteToScriptFile( fh, FALSE, lpTmpBuf );
      }
   WriteToScriptFile( fh, FALSE, "" );
}

/* This function takes a block of text and formats it, wrapping
 * paragraphs as required, so that it fits within the specified
 * word wrap column.
 */
void FASTCALL FormatText( HNDFILE fh, HPSTR lpstr, UINT wFormatType, int iWordWrapColumn )   // 20060228 - 2.56.2049.067   // Need to wrap message body text here
{
   static char szCrlf[] = "\r\n";
   BOOL fWrappedOnLast;

   fWrappedOnLast = FALSE;
   if( lpstr )
      while( *lpstr )
      {
         register int c, n;
         register int d, e;
         char szCpy[ MAX_WRAPCOLUMN+3 ];

         fWrappedOnLast = FALSE; // 2.56.2056
         for( n = c = d = e = 0; lpstr[ c ] && n <= iWordWrapColumn; ++c )
         {
            if( lpstr[ c ] == CR || lpstr[ c ] == LF )
            {
               int t;

               if( !fWrappedOnLast )
                  break;
               t = c;
               if( lpstr[ c ] == CR ) ++c;
               if( lpstr[ c ] == CR ) ++c;
               if( lpstr[ c ] == LF ) ++c;
               fWrappedOnLast = FALSE;
               if( lpstr[ c ] == CR || lpstr[ c ] == LF )
               {
                  c = t;
                  break;
               }
               --c;
               continue;
            }
            if( lpstr[ c ] == ' ' || lpstr[ c ] == '\t' ) 
            {
               d = c;
               e = n;
            }
            if( lpstr[ c ] ==  '\t' )
            {
               do
                  szCpy[ n++ ] = ' ';
               while( n & 0x07 );
            }
            else 
            {
               if( lpstr[ c ] == '"' && ( FMT_TXT_CMD & wFormatType ) )
                  szCpy[ n++ ] = '"';
               szCpy[ n++ ] = lpstr[ c ];
            }
         }
         if( lpstr[ c ] != '\0' && lpstr[ c ] != CR && d > 0 )
         {
            c = d + 1;
            n = e + 1;
            fWrappedOnLast = TRUE;
         }
         else
            fWrappedOnLast = FALSE;
         if( FMT_TXT_CMD & wFormatType )
            Amfile_Write( fh, "put \"", 5 );
         else
            Amfile_Write( fh, " ", 1 );

         /* Prefix any line beginning with 'From' with a
          * quote.
          */
         if( FMT_TXT_QUOTEFROM & wFormatType )
            if( strncmp( szCpy, "From ", 5 ) == 0 )
            {
               memmove( szCpy + 1, szCpy, ++n );
               szCpy[ 0 ] = '>';
            }

         /* Now write the text.
          */
         Amfile_Write( fh, szCpy, n );
         if( szCpy[ 0 ] == '.' && n == 1 )
            Amfile_Write( fh, " ", 1 );
         if( FMT_TXT_CMD & wFormatType )
            Amfile_Write( fh, "\"", 1 );
         Amfile_Write( fh, szCrlf, 2 );

         /* Skip to the next line.
          */
         lpstr += c;
         if( *lpstr == CR )
            ++lpstr;
         if( *lpstr == CR )
            ++lpstr;
         if( *lpstr == LF )
            ++lpstr;
      }
   if( FMT_TXT_CMD & wFormatType )
      WriteToScriptFile( fh, TRUE, "." );
   else if( FMT_TXT_CIX & wFormatType )
      {
      char sz[ 7 ];

      strcpy( sz, szCrlf );
      strcat( sz, " ." );
      strcat( sz, szCrlf );
      Amfile_Write( fh, sz, strlen( sz ) );
      }
}

void CDECL WriteScriptLine( HNDFILE fh, LPSTR lpszText, ... )
{
   char szvBuf[ 200 ];
   char szBuf[ 180 ];
   va_list argPtr;

   va_start( argPtr, lpszText );
   wvsprintf( szBuf, lpszText, argPtr );
   wsprintf( szvBuf, "%s\r\n", (LPSTR)szBuf );
   Amfile_Write( fh, szvBuf, strlen( szvBuf ) );
}

/* This function writes text to the command line. If fCommand is
 * TRUE, the text is written as a put "xxx" command otherwise it
 * is written raw prefixed with one space.
 */
void CDECL FmtWriteToScriptFile( HNDFILE fh, BOOL fCommand, LPSTR lpszText, ... )
{
   char szBuf[ 180 ];
   va_list argPtr;

   va_start( argPtr, lpszText );
   wvsprintf( szBuf, lpszText, argPtr );
   WriteToScriptFile( fh, fCommand, szBuf );
}

/* This function writes text to the command line. If fCommand is
 * TRUE, the text is written as a put "xxx" command otherwise it
 * is written raw prefixed with one space.
 */
void FASTCALL WriteToScriptFile( HNDFILE fh, BOOL fCommand, LPSTR lpszText )
{
   char szvBuf[ 200 ];

   if( fCommand )
      {
      register int c;
      register int d;

      strcpy( szvBuf, "put \"" );
      for( d = strlen( szvBuf ), c = 0; lpszText[ c ]; ++c, ++d )
         {
         if( lpszText[ c ] == '"' )
            szvBuf[ d++ ] = '"';
         szvBuf[ d ] = lpszText[ c ];
         }
      szvBuf[ d++ ] = '\"';
      szvBuf[ d++ ] = CR;
      szvBuf[ d++ ] = LF;
      szvBuf[ d ] = '\0';
      Amfile_Write( fh, szvBuf, strlen( szvBuf ) );
      }
   else
      {
      if( fNeedCopy )
      {
         Amuser_GetActiveUserDirectory( szScriptScrPath, _MAX_FNAME );
         strcat( szScriptScrPath, "\\" );
         strcat( szScriptScrPath, "script.scr" );
         wsprintf( lpTmpBuf, "copy \"%s\" <<", szScriptScrPath );
         WriteScriptLine( fh, lpTmpBuf );
      }
      wsprintf( szvBuf, " %s\r\n", lpszText );
      Amfile_Write( fh, szvBuf, strlen( szvBuf ) );
      fNeedCopy = FALSE;
      }
}

/* This function extracts a CIX name.
 */
void FASTCALL MakeCixName( char * npszName )
{
   while( *npszName && *npszName != ' ' )
      ++npszName;
   *npszName = '\0';
}

/* This function sends a mail attachment. It creates a BINMAIL
 * object for this MAILOBJECT and calls SendBinaryFile to do
 * all the hard work.
 */
BOOL FASTCALL SendBinmailFile( HNDFILE fh, BINMAILOBJECT FAR * lpbmo, int obid )
{
   BOOL fOk;

   if( lpbmo->wEncodingScheme == ENCSCH_BINARY )
      fOk = SendBinaryFile( fh, lpbmo, obid );
   else
      {
      MAILOBJECT mob;

      /* Create a MAILOBJECT corresponding to this BINMAIL
       * object thingy.
       */
      Amob_New( OBTYPE_MAILMESSAGE, &mob );
      Amob_CreateRefString( &mob.recAttachments, DRF(lpbmo->recFileName) );
      Amob_CreateRefString( &mob.recTo, DRF(lpbmo->recUserName) );
      Amob_CreateRefString( &mob.recText, DRF(lpbmo->recText) );
      Amob_CreateRefString( &mob.recSubject, "Attachment" );
      mob.wEncodingScheme = lpbmo->wEncodingScheme;
      fOk = SendEncodedFile( fh, &mob, obid );
      Amob_Delete( OBTYPE_MAILMESSAGE, &mob );
      }
   return( fOk );
}

/* This function sends a mail attachment. It creates a BINMAIL
 * object for this MAILOBJECT and calls SendBinaryFile to do
 * all the hard work.
 */
BOOL FASTCALL SendMailFile( HNDFILE fh, MAILOBJECT FAR * lpmo, int obid )
{
   BOOL fOk;

   if( lpmo->wEncodingScheme != ENCSCH_BINARY )
      fOk = SendEncodedFile( fh, lpmo, obid );
   else
      {
      BINMAILOBJECT bmo;
      LPSTR lpUsername;
      UINT cbUsername;

      INITIALISE_PTR(lpUsername);

      /* Need to create a BINMAIL object type from a MAILOBJECT
       * type for the SendBinaryFile function to work.
       */
      Amob_New( OBTYPE_BINMAIL, &bmo );
      Amob_CreateRefString( &bmo.recFileName, DRF(lpmo->recAttachments) );
      Amob_CreateRefString( &bmo.recText, DRF(lpmo->recText) );

      /* Combine both To and CC fields into one.
       */
      cbUsername = strlen( DRF(lpmo->recTo) ) + strlen( DRF(lpmo->recCC) ) + 2;
      if( !fNewMemory( &lpUsername, cbUsername ) )
         fOk = FALSE;
      else
         {
         strcpy( lpUsername, DRF(lpmo->recTo) );
         if( *DRF(lpmo->recCC) )
            {
            strcat( lpUsername, " " );
            strcat( lpUsername, DRF(lpmo->recCC) );
            }
         Amob_CreateRefString( &bmo.recUserName, lpUsername );
         FreeMemory( &lpUsername );

         /* Set flags and file size.
          */
         bmo.wFlags |= BINF_SENDCOVERNOTE|BINF_DELETE|BINF_UPLOAD;
         bmo.wEncodingScheme = lpmo->wEncodingScheme;
         bmo.dwSize = 0L;
         fOk = SendBinaryFile( fh, &bmo, obid );
         }
      Amob_Delete( OBTYPE_BINMAIL, &bmo );
      }
   return( fOk );
}

/* This function handles the code to binmail a file to a user. The
 * most significant change from Ameol v1 is that we now detect for
 * a successful binmail and send the cover note there and then.
 *
 * New for v2.0! Can binmail multiple files in one go. Each filename
 * in recFileName must be delimited by spaces. Embedded spaces must be
 * placed in quotes.
 */
BOOL FASTCALL SendBinaryFile( HNDFILE fh, BINMAILOBJECT FAR * lpbmo, int obid )
{
   LPSTR lpFilename;
   HPSTR lpText;

   INITIALISE_PTR(lpText);

   /* Enter the mail system.
    */
   WriteToScriptFile( fh, TRUE, szEnterMail );
   WriteScriptLine( fh, szWaitforMail );
   cEncodedParts = 0;

   /* May be multiple file names.
    */
   lpFilename = DRF(lpbmo->recFileName);
   while( *lpFilename )
      {
      char szName[ LEN_INTERNETNAME+1 ];
      char szText[ 100 ];
      MAILOBJECT mob;
      LPSTR lpszTo;

      /* Extract one filename.
       */
      lpFilename = ExtractFilename( lpPathBuf, lpFilename );

      /* If we're uploading, upload the file now and ensure
       * we were successful.
       */
      if( lpbmo->wFlags & BINF_UPLOAD )
         {
         HNDFILE fhFile;

         /* For uploaded files, set the file size now.
          */
         lpbmo->dwSize = 0L;
         if( HNDFILE_ERROR != ( fhFile = Amfile_Open( lpPathBuf, AOF_SHARE_READ ) ) )
            {
            lpbmo->dwSize = Amfile_GetFileSize( fhFile );
            Amfile_Close( fhFile );
            }
         WriteToScriptFile( fh, TRUE, "ful" );
         WriteScriptLine( fh, szUpload, lpPathBuf );
         WriteScriptLine( fh, szTestLasterrorFalse );
         }

      /* This code is embedded within a successful upload. Binmail
       * the file to every user.
       */
      lpszTo = DRF(lpbmo->recUserName);
      while( *lpszTo )
         {
         char szFileName[ LEN_PATHNAME+1 ];
         char szAddress[ LEN_CIXNAME+1 ];

         lpszTo = ExtractMailAddress( lpszTo, szName, sizeof( szName ) );
         lstrcpy( szFileName, lpPathBuf );
         StripLeadingTrailingQuotes( szName );
         if( Amaddr_IsGroup( szName ) )
            {
            int index;

            for( index = 0; Amaddr_ExpandGroup( szName, index, szAddress ); ++index )
               {
               ExtractCIXAddress( szAddress, szAddress );
               FmtWriteToScriptFile( fh, TRUE, szPutBinmail, (LPSTR)szAddress, (LPSTR)GetFileBasename( szFileName ) );
               }
            }
         else
            {
            ExtractCIXAddress( szName, szAddress );
            FmtWriteToScriptFile( fh, TRUE, szPutBinmail, (LPSTR)szAddress, (LPSTR)GetFileBasename( szFileName ) );
            }
         WriteScriptLine( fh, szWaitforMail );
         }

      /* All sent, so create an e-mail message for all the recipients.
       */
      if( lpbmo->wFlags & BINF_SENDCOVERNOTE )
         {
         Amob_New( OBTYPE_MAILMESSAGE, &mob );
         Amob_CreateRefString( &mob.recAttachments, lpPathBuf );
         Amob_CreateRefString( &mob.recTo, DRF(lpbmo->recUserName) );

         /* Create the subject line.
          */
         wsprintf( szText, GS(IDS_STR51), GetFileBasename( lpPathBuf ), lpbmo->dwSize );
         Amob_CreateRefString( &mob.recSubject, szText );

         /* Create the standard header.
          */
         wsprintf( szText, "%s%s", GS(IDS_STR974), GetFileBasename( lpPathBuf ) );
         if( 0L != lpbmo->dwSize )
            wsprintf( szText + strlen(szText), " %lu", lpbmo->dwSize );
         strcat( szText, "\r\n\r\n" );

         /* Allocate space for the header and the text.
          */
         if( fNewMemory32( &lpText, lpbmo->recText.dwSize + strlen(szText) + 1 ) )
            {
            strcpy( lpText, szText );
            hstrcat( lpText, DRF(lpbmo->recText) );
            CreateMailObject( fh, &mob, lpText, fCCMailToSender, TRUE );
            FreeMemory32( &lpText );
            }
         Amob_Delete( OBTYPE_MAILMESSAGE, &mob );
         }

      /* If we delete after uploading, then delete the
       * binmailed file now.
       */
      if( lpbmo->wFlags & BINF_DELETE )
         {
         char szFileName[ LEN_PATHNAME+1 ];

         lstrcpy( szFileName, lpPathBuf );
         FmtWriteToScriptFile( fh, TRUE, szEraFile, (LPSTR)GetFileBasename( szFileName ) );
         WriteScriptLine( fh, szWaitforMail );
         }

      /* Done!
       */
      WriteScriptLine( fh, szSuccess, obid );

      /* End of conditional block for when upload is not
       * successful.
       */
      if( lpbmo->wFlags & BINF_UPLOAD )
         WriteScriptLine( fh, szEndif );
      }

   /* Barf out of mail subsystem.
    */
   WriteToScriptFile( fh, TRUE, szQuitMode );
   return( TRUE );
}

/* This function creates one or more mail messages consisting of uuencoded forms
 * of the file specified in the BINMAILOBJECT object.
 */
BOOL FASTCALL SendEncodedFile( HNDFILE fh, MAILOBJECT FAR * lpmo, int obid )
{
   HPSTR hpEncodedParts;
   LPSTR lpStrBoundary;

   /* Create the MIME boundary.
    */
   if( lpmo->wEncodingScheme == ENCSCH_UUENCODE )
      lpStrBoundary = NULL;
   else
      {
      lpStrBoundary = CreateMimeBoundary();
      if( NULL == lpStrBoundary )
         return( FALSE );
      }

   /* Encode the file(s) and return now if any error.
    */
   hpEncodedParts = EncodeFiles( hwndFrame, DRF(lpmo->recAttachments), lpStrBoundary, lpmo->wEncodingScheme, &cEncodedParts );
   if( NULL == hpEncodedParts )
      return( FALSE );
   nThisEncodedPart = 0;
   if( fSeparateEncodedCover )
      CreateMailObject( fh, lpmo, DRF(lpmo->recText), FALSE, FALSE );
   else
      {
      HPSTR hpFirstPage;
      DWORD dwFirstPage;

      INITIALISE_PTR(hpFirstPage);

      /* Create the first page by combining the subject text and the
       * first encoded part.
       */
      dwFirstPage = hstrlen(hpEncodedParts) + lstrlen(DRF(lpmo->recText)) + 3;
      if( fNewMemory32( &hpFirstPage, dwFirstPage ) )
         {
         hstrcpy( hpFirstPage, DRF(lpmo->recText) );
         hstrcat( hpFirstPage, "\r\n" );
         hstrcat( hpFirstPage, hpEncodedParts );
         ++nThisEncodedPart;
         CreateMailObject( fh, lpmo, hpFirstPage, FALSE, FALSE );
         FreeMemory32( &hpFirstPage );
         hpEncodedParts += hstrlen(hpEncodedParts) + 1;
         }
      }
   ++nThisEncodedPart;

   /* Now loop for each part and send a mail message.
    * At this point, hpEncodedParts points to a huge (>64K) memory
    * block of encoded file, each part delimited by a NULL.
    */
   while( nThisEncodedPart <= cEncodedParts )
      {
      CreateMailObject( fh, lpmo, hpEncodedParts, FALSE, FALSE );
      hpEncodedParts += hstrlen(hpEncodedParts) + 1;
      ++nThisEncodedPart;
      }

   /* Create a local copy of the attachment message.
    */
   CreateLocalCopyOfAttachmentMail( lpmo, NULL, cEncodedParts, NULL );
   return( TRUE );
}

/* This function creates a forward mail message.
 */
void FASTCALL CreateForwardMailObject( HNDFILE fh, FORWARDMAILOBJECT FAR * lpfmo )
{
   MAILOBJECT mob;
   BOOL fEcho;

   /* Create a MAILOBJECT to send the message.
    */
   Amob_New( OBTYPE_MAILMESSAGE, &mob );
   Amob_CreateRefString( &mob.recSubject, DRF(lpfmo->recSubject) );
   Amob_CreateRefString( &mob.recTo, DRF(lpfmo->recTo) );
   Amob_CreateRefString( &mob.recCC, DRF(lpfmo->recCC) );
   Amob_CreateRefString( &mob.recBCC, DRF(lpfmo->recBCC) );
   Amob_CreateRefString( &mob.recReplyTo, DRF(lpfmo->recReplyTo) );
   mob.nPriority = lpfmo->nPriority;
   mob.wFlags = lpfmo->wFlags;
   fEcho = !(mob.wFlags & MOF_NOECHO );
   CreateMailObject( fh, &mob, DRF(lpfmo->recText), fEcho, FALSE );
   Amob_Delete( OBTYPE_MAILMESSAGE, &mob );
}

/* This function creates a mail message.
 */
void FASTCALL CreateMailObject( HNDFILE fh, MAILOBJECT FAR * lpmo, LPSTR lpszText, BOOL fEcho, BOOL fCommand )
{
   char szName[ LEN_INTERNETNAME+1 ];
   LPSTR lpszGrpPtr;
   LPSTR lpszReply;
   register int c;
   char sz[ 200 ];
   LPSTR lpszTo;
   LPSTR lpszCC;
   LPSTR lpszBCC;
   BOOL fGroup;
   BOOL fSelf;
   BOOL fBCCGroup;

   /* Dereference the mail fields.
    */
   lpszTo = DRF(lpmo->recTo);
   lpszCC = DRF(lpmo->recCC);
   lpszBCC = DRF(lpmo->recBCC);
   lpszReply = DRF(lpmo->recReply);

   /* Get the first address in the list.
    */
   lpszTo = ExtractMailAddress( lpszTo, szName, sizeof( szName ) );
   lpszGrpPtr = NULL;
   fGroup = FALSE;
   fSelf = FALSE;

   /* This code gets round a bug in CIX where sending more than
    * CIX_MAIL_THRESHOLD messages in the system causes a failure.
    * The solution is to quit the mail system and re-enter it.
    */
   if( ++cMailMsgs > CIX_MAIL_THRESHOLD )
      {
      WriteToScriptFile( fh, fCommand, szQuitModeCR );
      WriteToScriptFile( fh, fCommand, szEnterMailMode );
      cMailMsgs = 0;
      }

   /* If the first name is a group name, get the first
    * group entry.
    */
   StripLeadingTrailingQuotes( szName );
   if( Amaddr_IsGroup( szName ) )
      {
      char szGrpEntry[ AMLEN_INTERNETNAME+1 ];

      Amaddr_ExpandGroup( szName, 0, szGrpEntry );
      fGroup = TRUE;
      fSelf = strcmp( szGrpEntry, szCIXNickname ) == 0;
      wsprintf( sz, "to %s", (LPSTR)szGrpEntry );
      }
   else
      {
      if( szName[ 1 ] == '@' )
            wsprintf( sz, "to \"%s\"", (LPSTR)szName );
      else
         wsprintf( sz, "to %s", (LPSTR)szName );
      fSelf = strcmp( szName, szCIXNickname ) == 0;
      }
   WriteToScriptFile( fh, fCommand, sz );

   /* Write the subject field.
    */
   wsprintf( sz, "%s", DRF(lpmo->recSubject) );
   if( cEncodedParts )
      wsprintf( sz + strlen( sz ), " (%u/%u)", nThisEncodedPart, cEncodedParts );
   WriteToScriptFile( fh, fCommand, sz );

   /* If this is a reply, prefix the reply text with
    * In-Reply-To.
    */
   if( *lpszReply )
      FmtWriteToScriptFile( fh, fCommand, "In-Reply-To: <%s>", lpszReply );

   /* Write the text.
    */
   if( fCommand )
      FormatText( fh, lpszText, FMT_TXT_CMD|FMT_TXT_QUOTEFROM, 69 );
   else
      FormatText( fh, lpszText, FMT_TXT_CIX|FMT_TXT_QUOTEFROM, iWrapCol );

   /* If we echo a copy back to ourselves, include our
    * CIX nickname in the CC field.
    */
   if( fEcho && !fSelf )
      FmtWriteToScriptFile( fh, fCommand, "cc %s", (LPSTR)szCIXNickname );

   /* Next, fill out the remainder of the To fields
    * from the group, if there was one.
    */
   if( fGroup )
      {
      char szGrpEntry[ AMLEN_INTERNETNAME+1 ];
      int index;

      for( index = 1; Amaddr_ExpandGroup( szName, index, szGrpEntry ); ++index )
         FmtWriteToScriptFile( fh, fCommand, "to %s", (LPSTR)szGrpEntry );
      }

   /* The rest of the To field is sent using CC
    */
   while( *lpszTo )
      {
      lpszTo = ExtractMailAddress( lpszTo, szName, sizeof( szName ) );
      StripLeadingTrailingQuotes( szName );
      if( Amaddr_IsGroup( szName ) )
         {
         char szGrpEntry[ AMLEN_INTERNETNAME+1 ];
         int index;

         for( index = 0; Amaddr_ExpandGroup( szName, index, szGrpEntry ); ++index )
            FmtWriteToScriptFile( fh, fCommand, "to %s", (LPSTR)szGrpEntry );
         }
      else
         FmtWriteToScriptFile( fh, fCommand, "to %s", (LPSTR)szName );
      }

   /* Now send the CC list.
    */
   while( *lpszCC )
      {
      lpszCC = ExtractMailAddress( lpszCC, szName, sizeof( szName ) );
      StripLeadingTrailingQuotes( szName );
      if( Amaddr_IsGroup( szName ) )
         {
         char szGrpEntry[ AMLEN_INTERNETNAME+1 ];
         int index;

         for( index = 0; Amaddr_ExpandGroup( szName, index, szGrpEntry ); ++index )
            FmtWriteToScriptFile( fh, fCommand, "cc %s", (LPSTR)szGrpEntry );
         }
      else
         FmtWriteToScriptFile( fh, fCommand, "cc %s", (LPSTR)szName );
      }

   /* Add custom fields that begin with X-
    */
   LoadMailHeaders();
   for( c = 0; c < cMailHeaders; ++c )
      {
      if( strlen( uMailHeaders[ c ].szType ) > 2 && memcmp( uMailHeaders[ c ].szType, "X-", 2 ) == 0 )
         FmtWriteToScriptFile( fh, fCommand, "xtra %s %s", uMailHeaders[ c ].szType + 2, uMailHeaders[ c ].szValue );
      }

   /* Add priority information, if required.
    */
   if( lpmo->nPriority != MAIL_PRIORITY_NORMAL )
      FmtWriteToScriptFile( fh, fCommand, "xtra Priority %s", szPriorityDescription[ lpmo->nPriority - MAIL_PRIORITY_LOWEST ] );

   /* Finally, send the mail message itself.
    */
   WriteToScriptFile( fh, fCommand, szSend );

   while( *lpszBCC )
      {
      fBCCGroup = FALSE;
      lpszBCC = ExtractMailAddress( lpszBCC, szName, sizeof( szName ) );
      StripLeadingTrailingQuotes( szName );
      if( Amaddr_IsGroup( szName ) )
         {
         char szGrpEntry[ AMLEN_INTERNETNAME+1 ];
         int index;
         fBCCGroup = TRUE;


         for( index = 0; Amaddr_ExpandGroup( szName, index, szGrpEntry ); ++index )
            {
               FmtWriteToScriptFile( fh, fCommand, "to %s", (LPSTR)szGrpEntry );

               /* Write the subject field.
                */
               wsprintf( sz, "%s", DRF(lpmo->recSubject) );
               if( cEncodedParts )
                  wsprintf( sz + strlen( sz ), " (%u/%u)", nThisEncodedPart, cEncodedParts );
               WriteToScriptFile( fh, fCommand, sz );

               /* If this is a reply, prefix the reply text with
                * In-Reply-To.
                */
               if( *lpszReply )
                  FmtWriteToScriptFile( fh, fCommand, "In-Reply-To: <%s>", lpszReply );

               /* Write the text.
                */
               if( fCommand )
                  FormatText( fh, lpszText, FMT_TXT_CMD|FMT_TXT_QUOTEFROM, 69 );
               else
                  FormatText( fh, lpszText, FMT_TXT_CIX|FMT_TXT_QUOTEFROM, iWrapCol );

               /* Add custom fields that begin with X-
                */
               for( c = 0; c < cMailHeaders; ++c )
               {
                  if( strlen( uMailHeaders[ c ].szType ) > 2 && memcmp( uMailHeaders[ c ].szType, "X-", 2 ) == 0 )
                     FmtWriteToScriptFile( fh, fCommand, "xtra %s %s", uMailHeaders[ c ].szType + 2, uMailHeaders[ c ].szValue );
               }

               /* Add priority information, if required.
                */
               if( lpmo->nPriority != MAIL_PRIORITY_NORMAL )
                  FmtWriteToScriptFile( fh, fCommand, "xtra Priority %s", szPriorityDescription[ lpmo->nPriority - MAIL_PRIORITY_LOWEST ] );

               /* Finally, send the mail message itself.
                */
               WriteToScriptFile( fh, fCommand, szSend );

            }
         }
      else
         FmtWriteToScriptFile( fh, fCommand, "to %s", (LPSTR)szName );

   /* Write the subject field.
    */
   if( !fBCCGroup )
   {
   wsprintf( sz, "%s", DRF(lpmo->recSubject) );
   if( cEncodedParts )
      wsprintf( sz + strlen( sz ), " (%u/%u)", nThisEncodedPart, cEncodedParts );
   WriteToScriptFile( fh, fCommand, sz );

   /* If this is a reply, prefix the reply text with
    * In-Reply-To.
    */
   if( *lpszReply )
      FmtWriteToScriptFile( fh, fCommand, "In-Reply-To: <%s>", lpszReply );

   /* Write the text.
    */
   if( fCommand )
      FormatText( fh, lpszText, FMT_TXT_CMD|FMT_TXT_QUOTEFROM, 69 );
   else
      FormatText( fh, lpszText, FMT_TXT_CIX|FMT_TXT_QUOTEFROM, iWrapCol );

   /* Add custom fields that begin with X-
    */
   for( c = 0; c < cMailHeaders; ++c )
      {
      if( strlen( uMailHeaders[ c ].szType ) > 2 && memcmp( uMailHeaders[ c ].szType, "X-", 2 ) == 0 )
         FmtWriteToScriptFile( fh, fCommand, "xtra %s %s", uMailHeaders[ c ].szType + 2, uMailHeaders[ c ].szValue );
      }

   /* Add priority information, if required.
    */
   if( lpmo->nPriority != MAIL_PRIORITY_NORMAL )
      FmtWriteToScriptFile( fh, fCommand, "xtra Priority %s", szPriorityDescription[ lpmo->nPriority - MAIL_PRIORITY_LOWEST ] );

   /* Finally, send the mail message itself.
    */
   WriteToScriptFile( fh, fCommand, szSend );
   }
   
   }



}

/* This function extracts the CIX portion of a full e-mail
 * address.
 */
void FASTCALL ExtractCIXAddress( LPSTR lpName, LPSTR lpBuf )
{
   while( *lpName && *lpName != '@' )
      *lpBuf++ = *lpName++;
   *lpBuf = '\0';
}

/* This function adds the contents of the specified
 * script file at the current position in the script. The
 * script file is assumed to be in the script directory.
 * Blank lines are stripped out.
 */
BOOL FASTCALL InsertScript( HNDFILE fh, LPCSTR npScriptFileName )
{
   char sz[ _MAX_PATH ];
   HNDFILE fh2;

   wsprintf( sz, "%s\\%s", (LPSTR)pszScriptDir, (LPSTR)npScriptFileName );
   if( ( fh2 = Amfile_Open( sz, AOF_READ ) ) != HNDFILE_ERROR )
      {
      LPLBF hBuffer;

      hBuffer = Amlbf_Open( fh2, AOF_READ );
      while( Amlbf_Read( hBuffer, sz, sizeof( sz ) - 1, NULL, NULL, &fIsAmeol2Scratchpad ) )
         {
         if( *sz )
            WriteScriptLine( fh, sz );
         }
      Amlbf_Close( hBuffer );
      }
   return( fh2 != HNDFILE_ERROR );
}

/* This function writes the status command to the script
 * file.
 */
void FASTCALL WriteStatusLine( HNDFILE fh, LPSTR lpszText )
{
   WriteScriptLine( fh, szStatus, lpszText );
}

/* This function writes the unique ID header echo.
 */
void FASTCALL WriteIDHeader( HNDFILE fh )
{
   FmtWriteToScriptFile( fh, FALSE, szFileEchoID, (LPSTR)szUniqueIDEcho, wUniqueID );
}

/* This function adds commands to the script to download
 * the mail directory
 */
void FASTCALL Mail_UpdateDirectory( HNDFILE fh )
{
   char szUserDir[ _MAX_PATH ];

   WriteToScriptFile( fh, TRUE, szKillscratch );
   WriteStatusLine( fh, GS(IDS_STR1016) );
   WriteToScriptFile( fh, TRUE, szFileMailDir );
   WriteScriptLine( fh, szWaitforMain );
   WriteToScriptFile( fh, TRUE, szDownload );
   Amuser_GetActiveUserDirectory( szUserDir, _MAX_PATH );
   WriteScriptLine( fh, szDownloadFile, (LPSTR)szUserDir, (LPSTR)szMailDirFile );
}

LRESULT EXPORT CALLBACK ObProc_ExecVerScript( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   switch( uType )
      {
      case OBEVENT_EXEC:
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_DESCRIBE:
         strcpy( lpBuf, GS(IDS_STRING718) );
         return( TRUE );

      case OBEVENT_PERSISTENCE:
      case OBEVENT_FIND:
      case OBEVENT_DELETE:
         return( TRUE );
      }
   return( 0L );
}

/* This is the Exec CIX Script out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_ExecCIXScript( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   LRESULT i;

   switch( uType )
      {
      case OBEVENT_EXEC:
         if (fUseWebServices){ // !!SM!! 2.56.2053
            char lPath[255];
            char szPassword[ 40 ];

            memcpy( szPassword, szCIXPassword, sizeof(szCIXPassword) );
            Amuser_Decrypt( szPassword, rgEncodeKey );
                  
            wsprintf(lPath, "%s\\%s", (LPSTR)pszScratchDir, (LPSTR)szScratchPad );
            i = Am2DownloadScratch(szCIXNickname, szPassword, lPath);
            return i;
         }
         else
            return( Exec_CIX() );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_DESCRIBE:
         strcpy( lpBuf, GS(IDS_STR987) );
         return( TRUE );

      case OBEVENT_PERSISTENCE:
      case OBEVENT_FIND:
      case OBEVENT_DELETE:
         return( TRUE );
      }
   return( 0L );
}

BOOL FASTCALL CreateMailScript( void )
{
   HNDFILE fh;

   BEGIN_PATH_BUF;
   Amuser_GetActiveUserDirectory( lpPathBuf, _MAX_PATH );
   strcat( strcat( lpPathBuf, "\\" ), "ugetmail.scr" );
   if( ( fh = Amfile_Create( lpPathBuf, 0 ) ) == HNDFILE_ERROR )
      return( FALSE );
   END_PATH_BUF;

   WriteStatusLine( fh, "Collecting CIX Mail..." );
   WriteToScriptFile( fh, TRUE, szEnterMailMode );
   WriteScriptLine( fh, szWaitforMail );
   WriteToScriptFile( fh, TRUE, "file all" );
   WriteScriptLine( fh, szWaitforMail );
   WriteToScriptFile( fh, TRUE, szQuitModeCR );
   WriteScriptLine( fh, szWaitforMain );
   
   if( fArcScratch )
   {
   WriteToScriptFile( fh, TRUE, "arcscratch" );
   WriteScriptLine( fh, szWaitforMain );
   }
   WriteScriptLine( fh, szSetRecover );
   WriteToScriptFile( fh, TRUE, szDownload );
   WriteScriptLine( fh, szDownloadFile, (LPSTR)pszScratchDir, (LPSTR)"rmail.dat" );
   WriteScriptLine( fh, szTestLasterrorTrue );
   WriteScriptLine( fh, szLog, GS(IDS_STR1094) );
   WriteToScriptFile( fh, TRUE, szBye );
   WriteScriptLine( fh, "end" );
   WriteScriptLine( fh, szEndif );
   WriteScriptLine( fh, szNoRecover );
   WriteScriptLine( fh, "import \"%s\\%s\"", (LPSTR)pszScratchDir, (LPSTR)"rmail.dat" );
   WriteStatusLine( fh, GS(IDS_STR1015) );
   WriteToScriptFile( fh, TRUE, szBye );

   Amfile_Close( fh );
   return( TRUE );

}
