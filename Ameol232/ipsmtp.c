/* IPSMTP.C - Processes SMTP Mail functions
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

#include "main.h"
#include "palrc.h"
#include "resource.h"
#include "intcomms.h"
#include <memory.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "amlib.h"
#include "shared.h"
#include "cixip.h"
#include "amaddr.h"
#include "ameol2.h"
#include "rules.h"
#include "shrdob.h"
#include "mail.h"
#include "admin.h"
#include "log.h"
#include "blinkman.h"
#include "cookie.h"

#define  THIS_FILE   __FILE__

#define  NWS_MAILFROM            0x0100
#define  NWS_MAILTORCPT          0x0101
#define  NWS_MAILDATA            0x0102
#define  NWS_MAILQUIT            0x0103
#define  NWS_WAITINGQUIT            0x0104
#define  NWS_DISCONNECTED        0x0105
#define  NWS_MAILSENDDATA        0x0106
#define  NWS_MAILAUTH            0x0107
#define  NWS_MAILAUTHUSER        0x0108
#define  NWS_MAILAUTHPASS        0x0109
#define  NWS_ACTIONMASK          0x7FFF
#define SEND_PERCENTAGE_THRESHOLD   25000

#define  HPBUF_OFFSET            32

#define  MAX_PARTS               40
#define  cbLineBuf               1000

extern BYTE rgEncodeKey[8];

char * szPriorityDescription[ MAX_MAIL_PRIORITIES ] = { "5 (Lowest)", "4 (Low)", "3 (Normal)", "2 (High)", "1 (Highest)" };

char * szDayOfWeek[ 7 ] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
char * szMonth[ 12 ] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

int nSMTPState = 0;                       /* Mail state machine value */
LPCOMMDEVICE lpcdevSMTP = NULL;           /* Handle of mail comm device */

static BOOL fEntry = FALSE;               /* Re-entrancy flag */
static BOOL fNeedKickStart = FALSE;       /* Kickstart flag */

static char FAR szMsgId[ 256 ];           /* Message ID */

static HPSTR hpHdrText;                   /* Pointer to memory buffer */
static DWORD dwHdrLength;                 /* Length of message so far */
static DWORD dwBufSize;                   /* Size of buffer */
static DWORD dwMsgSentSoFar;                 /* Size of message sent so far */
static WORD wOldCount;                    /* Percentage count */

static HPSTR hpEncodedParts;              /* Pointer to current encoded part */
static HPSTR hpEncodedPartsPtr;           /* Pointer to encoded parts buffer */
static UINT cEncodedParts;                /* Number of encoded parts */
static UINT nThisEncodedPart;             /* Number of this encoded part */

typedef struct tagSMTPMAILHEADER {
   LPSTR lpStrTo;                      /* Pointer to To string */
   LPSTR lpStrReplyTo;                 /* Pointer to Reply To string */
   LPSTR lpStrCC;                      /* Pointer to CC string */
   LPSTR lpStrBCC;                     /* Pointer to BCC string */
   LPSTR lpStrSubject;                 /* Pointer to Subject string */
   LPSTR lpStrReply;                   /* Pointer to Reply string */
   LPSTR lpStrBoundary;                /* MIME encoding boundary */
   LPSTR lpStrMailAddress;
   LPSTR lpStrFullName;
   LPSTR lpStrExtraHeader;                /* Extra header text */
   int nPriority;                      /* Mail message priority */
   WORD wFlags;                        /* Mail flags */
} SMTPMAILHEADER;

static SMTPMAILHEADER smh;                /* SMTP mail header */

static LPSTR lpszTo;                      /* Compacted recipient string */
static LPSTR lpszCC;                      /* Compacted recipient string */
static LPSTR lpszBCC;                     /* Compacted BCC recipient */
static LPSTR lpszReplyTo;
static LPSTR lpszRecipients;              /* Full list of filtered recipients */
static LPSTR lpszRecipientsPtr;           /* Current recipient being processed */
static BOOL fSentHELO = FALSE;
static BOOL fCanSendHELO = FALSE; /*!!SM!!*/

char FASTCALL Encode64( char );
int FASTCALL b64encode(char *s);

BOOL EXPORT CALLBACK SMTPClientWndProc( LPCOMMDEVICE, int, DWORD, LPARAM );

void FASTCALL CreateMailHeader( SMTPMAILHEADER * );
BOOL FASTCALL EnsureSMTPWindow( void );
BOOL FASTCALL SendSMTPMail( LPCOMMDEVICE, char * );
void FASTCALL CleanUpSMTPState( void );
void FASTCALL SMTPError( char * );
void FASTCALL AppendToList( LPSTR, LPSTR, BOOL * );
void FASTCALL FillAddressList( LPSTR, LPSTR );
void FASTCALL WriteRecipientSocketLine( char * );
HPSTR FASTCALL FilterHeader( HPSTR );
LPSTR FASTCALL ExpandAndFilterRecipients( LPSTR, LPSTR, LPSTR );

static BOOL FASTCALL AppendToHeader( LPSTR );
static BOOL FASTCALL AppendMem32ToHeader( HPSTR, DWORD );

void FASTCALL WaitForReady(void)
{
  int t;
  t = 0;
  while (!fCanSendHELO && t < 50) /* !!SM!! */
  { 
    TaskYield();
    t++;
    Sleep(100);
  }
}

typedef BOOL (FAR PASCAL * WRAPTEXT)(LPSTR, LPSTR, DWORD, DWORD);

#define BUF_SIZ 5120

/* FS#76  Need to implement this to get round the wrap-on-save problem */
void FASTCALL FormatTextIP( HPSTR lprpSrc, HPSTR * lprpDest, DWORD pMax, DWORD iWordWrapColumn )   // 20060228 - 2.56.2049.067   // Need to wrap message body text here
{
   WRAPTEXT lpImpProc;
   
   if (hRegLib < (HINSTANCE)32)
      return;

   if( NULL == ( lpImpProc = (WRAPTEXT)GetProcAddress( hRegLib, "WrapText" ) ) )
      return;
   
//  PROCEDURE WrapText(pSource, pDest: PChar; pMaxLen: Integer; pLineLen: Integer); STDCALL;

   lpImpProc(lprpSrc, *lprpDest, pMax, iWordWrapColumn);

}

/* This function sends a mail to the SMTP server.
 */
int FASTCALL Exec_MailMessage( LPVOID dwData )
{
   MAILOBJECT FAR * lpm;
   HPSTR hpAttachments;
   char sz[ 256 ];
   char szExtraHeaderText[ 100 ];
   char szThisMailAddress[ LEN_MAILADDR + 1 ];
   char szThisMailName[ LEN_MAILADDR + 1 ];
   PTL ptl = NULL;

   szThisMailAddress[ 0 ] = '\0';
   szThisMailName[0 ] = '\0';
   szExtraHeaderText[ 0 ] = '\0';

   /* Context switch into appropriate service.
    */
   SwitchService( SERVICE_IP );

   /* Dereference the data structure.
    */
   lpm = (MAILOBJECT FAR *)dwData;

   /* If we're busy, this request goes into the
    * out-basket.
    */
   if( nSMTPState != NWS_IDLE )
      return( POF_AWAITINGSERVICE );

   /* Start up the connection.
    */
   if( !EnsureSMTPWindow() )
      return( POF_CANNOTACTION );

   /* Initialise encoding variables.
    */
   cEncodedParts = 0;
   nThisEncodedPart = 1;
   hpEncodedParts = NULL;
   hpEncodedPartsPtr = NULL;
   smh.lpStrBoundary = NULL;

   /* Do we have an attachment? If so, encode the file into one big memory
    * block in hpAttachment and set the cEncodedParts and hpEncodedParts
    * variables.
    */
   hpAttachments = DRF(lpm->recAttachments);
   if( *hpAttachments )
      {
      /* Create the MIME boundary.
       */
      if( lpm->wEncodingScheme == ENCSCH_MIME )
         {
         smh.lpStrBoundary = CreateMimeBoundary();
         if( NULL == smh.lpStrBoundary )
            return( POF_CANNOTACTION );
         }
      hpEncodedParts = EncodeFiles( hwndFrame, hpAttachments, smh.lpStrBoundary, lpm->wEncodingScheme, &cEncodedParts );
      hpEncodedPartsPtr = hpEncodedParts;
      if( NULL == hpEncodedParts )
         return( POF_CANNOTACTION );
      if( fSeparateEncodedCover )
         nThisEncodedPart = 0;
      }

   /* Create local copies of the subject, to, cc and reply strings
    * for multi-part mail messages.
    */
   if( *DRF(lpm->recMailbox) )
      ParseFolderPathname( DRF(lpm->recMailbox), &ptl, FALSE, 0 );
   if( NULL != ptl )
      Amdb_GetCookie( ptl, EXTRA_HEADER_TEXT, szExtraHeaderText, "", 99 );
   smh.lpStrExtraHeader = HpLpStrCopy( szExtraHeaderText );
   smh.lpStrTo = HpLpStrCopy( Amob_DerefObject( &lpm->recTo ) );
   smh.lpStrReplyTo = HpLpStrCopy( Amob_DerefObject( &lpm->recReplyTo ) );
   smh.lpStrCC = HpLpStrCopy( Amob_DerefObject( &lpm->recCC ) );
   smh.lpStrBCC = HpLpStrCopy( Amob_DerefObject( &lpm->recBCC ) );
   smh.lpStrSubject = HpLpStrCopy( Amob_DerefObject( &lpm->recSubject ) );
   smh.lpStrReply = HpLpStrCopy( Amob_DerefObject( &lpm->recReply ) );
   smh.nPriority = lpm->nPriority;
   smh.wFlags = lpm->wFlags;

   if( strlen(smh.lpStrTo) == 0 && strlen(smh.lpStrCC) == 0 && strlen(smh.lpStrBCC) == 0 )
      return( POF_CANNOTACTION );

   if( DRF( lpm->recFrom ) )
      ParseFromField( Amob_DerefObject(&lpm->recFrom), szThisMailAddress, szThisMailName );
   if( !*szThisMailAddress )
      strcpy( szThisMailAddress, szMailAddress );
   if( !*szThisMailName )
      strcpy( szThisMailName, szFullName );
   smh.lpStrFullName = HpLpStrCopy( szThisMailName );
   smh.lpStrMailAddress = HpLpStrCopy( szThisMailAddress );

   /* Create the header text manually.
    */
   dwHdrLength = 0;
   dwBufSize = 0;
   INITIALISE_PTR( hpHdrText );

   /* Create the mail header.
    */
   if( !(lpm->wFlags & MOF_NOECHO ) )
      {
      LPSTR lpSender;
      LPSTR lpStrCC2;
      UINT length;

      /* If echo enabled, append our e-mail address to the CC
       * list.
       */
      INITIALISE_PTR(lpStrCC2);
      lpSender = *smh.lpStrReplyTo ? smh.lpStrReplyTo : szMailAddress;
      length = lstrlen( smh.lpStrCC ) + strlen( lpSender ) + 2;
      if( fNewMemory( &lpStrCC2, length ) )
         {
         hstrcpy( lpStrCC2, smh.lpStrCC );
         if( *smh.lpStrCC )
            hstrcat( lpStrCC2, " " );
         hstrcat( lpStrCC2, lpSender );
         FreeMemory( &smh.lpStrCC );
         smh.lpStrCC = lpStrCC2;
         }
      }
   CreateMailHeader( &smh );

   /* Also create a local copy of the message
    */
   if( !( lpm->wFlags & MOF_NOECHO ) && *hpAttachments )
   {
      CreateLocalCopyOfAttachmentMail( lpm, hpHdrText, cEncodedParts, szMsgId );
      smh.lpStrCC = HpLpStrCopy( Amob_DerefObject( &lpm->recCC ) );
      INITIALISE_PTR( hpHdrText );
      CreateMailHeader( &smh );
   }



   /* Append the text body
    */
   Amob_DerefObject( &lpm->recText );
   if( NULL != smh.lpStrBoundary )
      {
      char sz[ 80 ];

      wsprintf( sz, "--%s", smh.lpStrBoundary );
      AppendToHeader( sz );
      /*!!SM!!*/
//    AppendToHeader( "Content-Type: text/plain; charset=\"us-ascii\"" );
      AppendToHeader( "Content-Type: text/plain; charset=\"iso-8859-1\"" );
      if( !AppendToHeader( "" ) )
         {
         FreeMemory32( &hpHdrText );
         return( POF_CANNOTACTION );
         }
      }
   if( *lpm->recText.hpStr )
   {
      HPSTR lpStr ;
      HPSTR lpStrOut = NULL;
      DWORD lLen;

      lLen = (DWORD)(lstrlen(lpm->recText.hpStr) * 1.5);

      if (lLen <= 10)
         lLen = 10;

      INITIALISE_PTR(lpStrOut);
      
      fNewMemory32( &lpStrOut, lLen );
   
      //lpStr = lpm->recText.hpStr;

      // 20060228 - 2.56.2049.07
      // Need to wrap message body text here
      FormatTextIP( lpm->recText.hpStr, &lpStrOut, lLen - 1, imWrapCol > 4093 ? 4094 : imWrapCol ); // FS#76  

      lpStr = lpStrOut;

      while( *lpStr )
      {
         char * pTmpBuf;

         pTmpBuf = lpTmpBuf;
         while( *lpStr && *lpStr != 13 && *lpStr != 10 )
            *pTmpBuf++ = *lpStr++;
         *pTmpBuf = '\0';
         if( !AppendToHeader( lpTmpBuf ) )
         {
            FreeMemory32( &hpHdrText );
            return( POF_CANNOTACTION );
         }
         if( *lpStr == 13 ) ++lpStr;
         if( *lpStr == 13 ) ++lpStr;
         if( *lpStr == 10 ) ++lpStr;
      }
      FreeMemory32( &lpStrOut );

   }

   /* If we have an attachment and we attach to the first page, then
    * attach the first part to this page.
    */
   if( cEncodedParts )
      {
      if( !fSeparateEncodedCover )
         {
         AppendMem32ToHeader( hpEncodedParts, hstrlen( hpEncodedParts ) );
         hpEncodedParts += hstrlen( hpEncodedParts ) + 1;
         }
      ++nThisEncodedPart;
      }

   /* Status bar message.
    */
   OnlineStatusText( GS(IDS_STR730), lpm->recSubject.hpStr );

   /* All done! Say hello to the server.
    */

   if( !fSentHELO )
   {
      WaitForReady();
      if( beCur.szAuthInfoUser[0] != '\x0')
         wsprintf( sz, "EHLO %s\r\n", (LPSTR)szDomain );
      else
         wsprintf( sz, "HELO %s\r\n", (LPSTR)szDomain );
      if( WriteSocketLine( lpcdevSMTP, sz ) )
      {
         if( beCur.szAuthInfoUser[0] != '\x0')
            nSMTPState = NWS_MAILAUTH;
         else
            nSMTPState = NWS_MAILFROM;
         fSentHELO = TRUE;
         return( POF_PROCESSSTARTED );
      }
   }
   else
   {
      wsprintf( sz, "MAIL FROM:<%s>\r\n", smh.lpStrMailAddress );
      WriteSocketLine( lpcdevSMTP, sz );
      nSMTPState = NWS_MAILTORCPT;
      return( POF_PROCESSSTARTED );
   }

/* WaitForReady();

   if( !fSentHELO )
      wsprintf( sz, "HELO %s\r\n", (LPSTR)szDomain );
   else
      wsprintf( sz, "NOOP\r\n");

   if( WriteSocketLine( lpcdevSMTP, sz ) )
      {
      nSMTPState = NWS_MAILFROM;
      fSentHELO = TRUE;
      return( POF_PROCESSSTARTED );
      }
*/ 
   return( POF_CANNOTACTION );
}

/* This function forwards a mail to the SMTP server.
 */
int FASTCALL Exec_ForwardMail( LPVOID dwData )
{
   FORWARDMAILOBJECT FAR * lpm;
   LPSTR lpszSubject;
   HPSTR hpAttachments;
   PTL ptl = NULL;
   char szExtraHeaderText[ 100 ];
   char sz[ 256 ];
   char szThisMailAddress[ LEN_MAILADDR + 1 ];
   char szThisMailName[ LEN_MAILADDR + 1 ];

   szExtraHeaderText[ 0 ] = '\0';

   /* Context switch into appropriate service.
    */
   SwitchService( SERVICE_IP );

   /* Dereference the data structure.
    */
   lpm = (FORWARDMAILOBJECT FAR *)dwData;

   /* If we're busy, this request goes into the
    * out-basket.
    */
   if( nSMTPState != NWS_IDLE )
      return( POF_AWAITINGSERVICE );

   /* Start up the connection.
    */
   if( !EnsureSMTPWindow() )
      return( POF_CANNOTACTION );

   /* Initialise encoding variables.
    */
   cEncodedParts = 0;
   nThisEncodedPart = 1;
   hpEncodedParts = NULL;
   hpEncodedPartsPtr = NULL;
   smh.lpStrBoundary = NULL;

//---
   /* Do we have an attachment? If so, encode the file into one big memory
    * block in hpAttachment and set the cEncodedParts and hpEncodedParts
    * variables.
    */
   hpAttachments = DRF(lpm->recAttachments);
   if( *hpAttachments )
      {
      /* Create the MIME boundary.
       */
      if( lpm->wEncodingScheme == ENCSCH_MIME )
         {
         smh.lpStrBoundary = CreateMimeBoundary();
         if( NULL == smh.lpStrBoundary )
            return( POF_CANNOTACTION );
         }
      hpEncodedParts = EncodeFiles( hwndFrame, hpAttachments, smh.lpStrBoundary, lpm->wEncodingScheme, &cEncodedParts );
      hpEncodedPartsPtr = hpEncodedParts;
      if( NULL == hpEncodedParts )
         return( POF_CANNOTACTION );
      if( fSeparateEncodedCover )
         nThisEncodedPart = 0;
      }

   /* Create local copies of the subject, to, cc and reply strings
    * for multi-part mail messages.
    */
   if( *DRF(lpm->recMailbox) )
      ParseFolderPathname( DRF(lpm->recMailbox), &ptl, FALSE, 0 );
   if( NULL != ptl )
      Amdb_GetCookie( ptl, EXTRA_HEADER_TEXT, szExtraHeaderText, "", 99 );
   smh.lpStrExtraHeader = HpLpStrCopy( szExtraHeaderText );
   smh.lpStrTo = HpLpStrCopy( Amob_DerefObject( &lpm->recTo ) );
   smh.lpStrReplyTo = HpLpStrCopy( Amob_DerefObject( &lpm->recReplyTo ) );
   smh.lpStrCC = HpLpStrCopy( Amob_DerefObject( &lpm->recCC ) );
   smh.lpStrBCC = HpLpStrCopy( Amob_DerefObject( &lpm->recBCC ) );
   smh.lpStrSubject = HpLpStrCopy( Amob_DerefObject( &lpm->recSubject ) );
   smh.nPriority = lpm->nPriority;
   smh.wFlags = lpm->wFlags;


   if( strlen(smh.lpStrTo) == 0 && strlen(smh.lpStrCC) == 0 && strlen(smh.lpStrBCC) == 0 )
      return( POF_CANNOTACTION );

   if( DRF( lpm->recFrom ) )
      ParseFromField( Amob_DerefObject(&lpm->recFrom), szThisMailAddress, szThisMailName );
   if( !*szThisMailAddress )
      strcpy( szThisMailAddress, szMailAddress );
   if( !*szThisMailName )
      strcpy( szThisMailName, szFullName );
   smh.lpStrFullName = HpLpStrCopy( szThisMailName );
   smh.lpStrMailAddress = HpLpStrCopy( szThisMailAddress );

   /* Create the header text manually.
    */
   dwHdrLength = 0;
   dwBufSize = 0;
   INITIALISE_PTR( hpHdrText );

   /* Create the mail header.
    */
   if( !(lpm->wFlags & MOF_NOECHO ) )
      {
      LPSTR lpSender;
      LPSTR lpStrCC2;
      UINT length;

      /* If echo enabled, append our e-mail address to the CC
       * list.
       */
      INITIALISE_PTR(lpStrCC2);
      lpSender = *smh.lpStrReplyTo ? smh.lpStrReplyTo : szMailAddress;
      length = lstrlen( smh.lpStrCC ) + strlen( lpSender ) + 2;
      if( fNewMemory( &lpStrCC2, length ) )
         {
         hstrcpy( lpStrCC2, smh.lpStrCC );
         if( *smh.lpStrCC )
            hstrcat( lpStrCC2, " " );
         hstrcat( lpStrCC2, lpSender );
         FreeMemory( &smh.lpStrCC );
         smh.lpStrCC = lpStrCC2;
         }
      }
   CreateMailHeader( &smh );

   /* Also create a local copy of the message
    */
   if( !( lpm->wFlags & MOF_NOECHO ) && *hpAttachments )
   {
      CreateLocalCopyOfForwardAttachmentMail( lpm, hpHdrText, cEncodedParts, szMsgId );
      smh.lpStrCC = HpLpStrCopy( Amob_DerefObject( &lpm->recCC ) );
      INITIALISE_PTR( hpHdrText );
      CreateMailHeader ( &smh );
   }

   /* Append the text body
    */
   Amob_DerefObject( &lpm->recText );
   if( NULL != smh.lpStrBoundary )
      {
      char sz[ 80 ];

      wsprintf( sz, "--%s", smh.lpStrBoundary );
      AppendToHeader( sz );
      /*!!SM!!*/
//    AppendToHeader( "Content-Type: text/plain; charset=\"us-ascii\"" );
      AppendToHeader( "Content-Type: text/plain; charset=\"iso-8859-1\"" );
      if( !AppendToHeader( "" ) )
         {
         FreeMemory32( &hpHdrText );
         return( POF_CANNOTACTION );
         }
      }
   if( *lpm->recText.hpStr )
      {
      HPSTR lpStr;

      lpStr = lpm->recText.hpStr;
      while( *lpStr )
         {
         char * pTmpBuf;

         pTmpBuf = lpTmpBuf;
         while( *lpStr && *lpStr != 13 && *lpStr != 10 )
            *pTmpBuf++ = *lpStr++;
         *pTmpBuf = '\0';
         if( !AppendToHeader( lpTmpBuf ) )
            {
            FreeMemory32( &hpHdrText );
            return( POF_CANNOTACTION );
            }
         if( *lpStr == 13 ) ++lpStr;
         if( *lpStr == 13 ) ++lpStr;
         if( *lpStr == 10 ) ++lpStr;
         }
      }

   /* If we have an attachment and we attach to the first page, then
    * attach the first part to this page.
    */
   if( cEncodedParts )
      {
      if( !fSeparateEncodedCover )
         {
         AppendMem32ToHeader( hpEncodedParts, hstrlen( hpEncodedParts ) );
         hpEncodedParts += hstrlen( hpEncodedParts ) + 1;
         }
      ++nThisEncodedPart;
      }

//---
      

   /* Now filters out certain lines from the header
    * Added by YH 22/04/96
    */
/* Amob_DerefObject( &lpm->recText );
   if( !AppendMem32ToHeader( lpm->recText.hpStr, hstrlen( lpm->recText.hpStr ) ) )
      {
      FreeMemory32( &hpHdrText );
      return( POF_CANNOTACTION );
      }
*/
   /* Create the message in the Mail folder.
    */
   lpszSubject = lpm->recSubject.hpStr;
   if( *lpm->recSubject.hpStr == '\0' )
      lpszSubject = GS(IDS_STR933);
   else
      lpszSubject = lpm->recSubject.hpStr;


   /* Status bar message.
    */
   OnlineStatusText( GS(IDS_STR749), lpszSubject );

   /* All done! Say hello to the server.
    */

   if( !fSentHELO )
   {
      WaitForReady();
      if( beCur.szAuthInfoUser[0] != '\x0')
         wsprintf( sz, "EHLO %s\r\n", (LPSTR)szDomain );
      else
         wsprintf( sz, "HELO %s\r\n", (LPSTR)szDomain );
      if( WriteSocketLine( lpcdevSMTP, sz ) )
      {
         if( beCur.szAuthInfoUser[0] != '\x0')
            nSMTPState = NWS_MAILAUTH;
         else
            nSMTPState = NWS_MAILFROM;
         fSentHELO = TRUE;
         return( POF_PROCESSSTARTED );
      }
   }
   else
   {
      wsprintf( sz, "MAIL FROM:<%s>\r\n", smh.lpStrMailAddress );
      WriteSocketLine( lpcdevSMTP, sz );
      nSMTPState = NWS_MAILTORCPT;
      return( POF_PROCESSSTARTED );
   }

/* WaitForReady();

   if( !fSentHELO )
      wsprintf( sz, "HELO %s\r\n", (LPSTR)szDomain );
   else
      wsprintf( sz, "NOOP\r\n");

   if( WriteSocketLine( lpcdevSMTP, sz ) )
      {
      nSMTPState = NWS_MAILFROM;
      fSentHELO = TRUE;
      return( POF_PROCESSSTARTED );
      }
*/
   return( POF_CANNOTACTION );

}

/* This function creates the mail header using the MAILOBJECT data and
 * stores it in hpHdrText.
 */
void FASTCALL CreateMailHeader( SMTPMAILHEADER * psmh )
{
   AM_TIMEZONEINFO tzi;
   char sz[ 512 ];
   register int c;
   AM_DATE date;
   AM_TIME time;

   /* MIME Boundary?
    */
   AppendToHeader( "Mime-Version: 1.0" );
   if( psmh->lpStrBoundary != NULL )
      {
      wsprintf( sz, "Content-Type: multipart/mixed; boundary=\"%s\"", psmh->lpStrBoundary );
      AppendToHeader( sz );
      }
   else
      /*!!SM!!*/
//    AppendToHeader( "Content-Type: text/plain; charset=\"us-ascii\"" );
      AppendToHeader( "Content-Type: text/plain; charset=\"iso-8859-1\"" );

   /* Add a Date line
    */
   Amdate_GetCurrentDate( &date );
   Amdate_GetCurrentTime( &time );
   wsprintf( sz, "Date: %s, %u %s %u %02u:%2.2u ",
               szDayOfWeek[ date.iDayOfWeek ],
               date.iDay,
               szMonth[ date.iMonth - 1 ],
               date.iYear,
               time.iHour,
               time.iMinute );
   Amdate_GetTimeZoneInformation( &tzi );
   wsprintf( sz + strlen( sz ), "+%02u00 (%s)", tzi.diff, tzi.szZoneName );
   AppendToHeader( sz );

   /* Say who this is from.
    */
   if(fRFC2822) //!!SM!! 2.55.2035
      wsprintf( sz, "From: \"%s\" <%s>", psmh->lpStrFullName, psmh->lpStrMailAddress );
   else
      wsprintf( sz, "From: %s (%s)", psmh->lpStrMailAddress, psmh->lpStrFullName );

   AppendToHeader( sz );

   /* And the subject.
    */
   wsprintf( sz, "Subject: %s", psmh->lpStrSubject );
   if( cEncodedParts > 1 ) /*!!SM!!*/
      wsprintf( sz + strlen( sz ), " (%u/%u)", nThisEncodedPart, cEncodedParts );
   AppendToHeader( sz );

   /* Save the recipient address.
    */
   lpszTo = MakeCompactString( psmh->lpStrTo );
   lpszCC = MakeCompactString( psmh->lpStrCC );
   lpszBCC = MakeCompactString( psmh->lpStrBCC );
   lpszReplyTo = MakeCompactString( psmh->lpStrReplyTo );


   /* Need to make sure that only a maximum of 1 CIX address recieves
    * a RCPT command. This is because CoSy duplicates mails when there
    * are multiple RCPT and Resent-To fields.
    * YH 10/06/96 14:40
    */
   if( !( lpszRecipients = ExpandAndFilterRecipients( lpszTo, lpszCC, lpszBCC ) ) )
      {
      ReportOnline( GS(IDS_STR1086) );
      return;
      }
   else
      lpszRecipientsPtr = lpszRecipients;

   /* Fill out the To fields.
    */
   wsprintf( sz, "To: " );
   FillAddressList( sz, lpszTo );

   /* Fill out the CC fields.
    */
   if( lpszCC && *lpszCC )
      {
      wsprintf( sz, "CC: " );
      FillAddressList( sz, lpszCC );
      }

   /* Is this a reply?
    */
   if( psmh->lpStrReply && *psmh->lpStrReply )
      {
      wsprintf( sz, "In-Reply-To: <%s>", psmh->lpStrReply );
      AppendToHeader( sz );
      }

   /* Add Reply-To
    */
   if( lpszReplyTo && *lpszReplyTo )
      {
      wsprintf( sz, "Reply-To: " );
      FillAddressList( sz, lpszReplyTo );
/*    wsprintf( sz, "Reply-To: %s", (LPSTR)psmh->lpStrReplyTo );

      if( NULL == strchr( sz, '@' ) )
      {
      if( *szDefMailDomain != '@' )
         strcat( sz, "@" );
      strcat( sz, szDefMailDomain );
      }
      AppendToHeader( sz );
*/    }

   /* Create and store our own message ID.
    */
   CreateMessageId( szMsgId );
   wsprintf( sz, "Message-Id: %s", (LPSTR)szMsgId );
   AppendToHeader( sz );

   /* Return receipt?
    */
   if( psmh->wFlags & MOF_RETURN_RECEIPT )
      {
      wsprintf( sz, "Disposition-Notification-To: <%s>", (LPSTR)psmh->lpStrReplyTo );
      AppendToHeader( sz );
      }

   /* Extra header */
   if( *psmh->lpStrExtraHeader )
      {
      AppendToHeader( (LPSTR)psmh->lpStrExtraHeader );
      }

   /* Insert the custom headers.
    */
   LoadMailHeaders();
   for( c = 0; c < cMailHeaders; ++c )
      {
      wsprintf( sz, "%s: %s", uMailHeaders[ c ].szType, uMailHeaders[ c ].szValue );
      AppendToHeader( sz );
      }

   /* Add X-Priority for non-normal priority messages.
    */
   if( psmh->nPriority != MAIL_PRIORITY_NORMAL )
      {
      wsprintf( sz, "X-Priority: %s", szPriorityDescription[ psmh->nPriority - MAIL_PRIORITY_LOWEST ] );
      AppendToHeader( sz );
      }

   /* Now append a blank line.
    */
   AppendToHeader( "" );
}

/* This function ensures that a mail window is open for processing.
 */
BOOL FASTCALL EnsureSMTPWindow( void )
{
   LPCOMMDESCRIPTOR lpcdSMTP;
   char * pszLogFile;
   char szBlinkSMTP[ 64 ];
   char szAuthInfoPass[ 80 ];

    fCanSendHELO = FALSE; //!!SM!!

   INITIALISE_PTR(lpcdSMTP);

   /* Go no further if not allowed to use CIX internet.
    */
   if( !TestPerm(PF_CANUSECIXIP) )
      return( FALSE );

   /* Return now if lpcdevNNTP is not NULL, indicating that
    * a SMTP server device is already open.
    */
   if( NULL != lpcdevSMTP )
      {
      SuspendDisconnectTimeout();
      return( TRUE );
      }

   /* Create a temporary connection card.
    */
   if( !fNewMemory( &lpcdSMTP, sizeof(COMMDESCRIPTOR) ) )
      return( FALSE );
   memset( lpcdSMTP, 0, sizeof(COMMDESCRIPTOR) );
   strcpy( lpcdSMTP->szTitle, "$SMTP" );
   INITIALISE_PTR(lpcdSMTP->lpic );

   /* Create an internet section in the CC.
    */
   if( !fNewMemory( &lpcdSMTP->lpic, sizeof(IPCOMM) ) )
      {
      FreeMemory( &lpcdSMTP );
      return( FALSE );
      }

   lpcdSMTP->wProtocol = PROTOCOL_NETWORK;
   lpcdSMTP->lpic->wPort = (UINT)Amuser_GetPPLong( szSettings, "SMTPPort", IPPORT_SMTP );
   lpcdSMTP->nTimeout = 60;
#ifdef WIN32
   lpcdSMTP->lpic->rd = rdDef;
#endif
   strcpy( lpTmpBuf, beCur.szName );
   strcat( lpTmpBuf, "-SMTP" );
   Amuser_GetPPString( szSMTPServers, lpTmpBuf, "", szBlinkSMTP, sizeof( szBlinkSMTP ) );
   if( strlen( szBlinkSMTP ) > 0 )
      strcpy( lpcdSMTP->lpic->szAddress, szBlinkSMTP );
   else
      strcpy( szBlinkSMTP, szSMTPMailServer );
   
   strcpy( lpcdSMTP->lpic->szAddress, szBlinkSMTP );

   
   memset( beCur.szAuthInfoUser, 0, sizeof(beCur.szAuthInfoUser) );
   memset( beCur.szAuthInfoPass, 0, sizeof(beCur.szAuthInfoPass) );

   strcpy( lpTmpBuf, beCur.szName );
   strcat( lpTmpBuf, "-SMTP User" );
   Amuser_GetPPString( szSMTPServers, lpTmpBuf, "", beCur.szAuthInfoUser, sizeof( beCur.szAuthInfoUser ) ); 
   
   strcpy( lpTmpBuf, beCur.szName );
   strcat( lpTmpBuf, "-SMTP Password" );
   Amuser_GetPPString( szSMTPServers, lpTmpBuf, "", szAuthInfoPass, sizeof( szAuthInfoPass ) );
   DecodeLine64( szAuthInfoPass, beCur.szAuthInfoPass, sizeof( beCur.szAuthInfoPass)-1 );
   
   
   /* Create a logging file.
    */
   pszLogFile = NULL;
   if( fLogSMTP )
   {
      char szTemp[_MAX_PATH];
      BEGIN_PATH_BUF;
      Amuser_GetActiveUserDirectory( lpPathBuf, _MAX_PATH );
      strcat( strcat( lpPathBuf, "\\" ), "smtp.log" );
      pszLogFile = (char *)lpPathBuf;
      if(GetShortPathName( pszLogFile, (char *)&szTemp, _MAX_PATH ) > 0 )
         wsprintf( lpTmpBuf, GS(IDS_STR1117), "SMTP", (char *)szTemp );
      else
         wsprintf( lpTmpBuf, GS(IDS_STR1117), "SMTP", (char *)pszLogFile );
      WriteToBlinkLog( lpTmpBuf );
      END_PATH_BUF;
   }

   /* Open the SMTP device.
    */
   nSMTPState = NWS_IDLE;
   OnlineStatusText( GS(IDS_STR1021) );
   if( Amcomm_OpenCard( hwndFrame, &lpcdevSMTP, lpcdSMTP, SMTPClientWndProc, 0L, pszLogFile, NULL, FALSE ) )
      {
      SuspendDisconnectTimeout();
      OnlineStatusText( GS(IDS_STR1022) );
      }
   else
      {
      wsprintf( lpTmpBuf, GS(IDS_STR1082), szBlinkSMTP, Amcomm_GetLastError() );
      ReportOnline( lpTmpBuf );
      WriteToBlinkLog( lpTmpBuf );
      nSMTPState = 0;
      }
   return( lpcdevSMTP != NULL );
}

/* This function handles all messages for the mail server.
 */
BOOL EXPORT CALLBACK SMTPClientWndProc( LPCOMMDEVICE lpcdev, int nCode, DWORD dwAppData, LPARAM lParam )
{
   switch( nCode )
      {
      case CCB_DISCONNECTED:
         OnlineStatusText( GS(IDS_STR1023) );
         nSMTPState = 0;
         CleanUpSMTPState();
         Amcomm_Close( lpcdevSMTP );
         Amcomm_Destroy( lpcdevSMTP );
         if( fLogSMTP )
            {
            Amuser_GetActiveUserDirectory( lpTmpBuf, _MAX_PATH );
            strcat( strcat( lpTmpBuf, "\\" ), "smtp.log" );
            Ameol2_Archive( lpTmpBuf, FALSE );
            }
         lpcdevSMTP = NULL;
         fSentHELO = FALSE;
         return( TRUE );

      case CCB_DATAREADY:
         ReadAndProcessSocket( lpcdev, (LPBUFFER)lParam, SendSMTPMail );
         break;
      }
   return( FALSE );
}

/* This function cleans up at the end of an SMTP session.
 */
void FASTCALL CleanUpSMTPState( void )
{
   if( hpHdrText )
      {
      FreeMemory32( &hpHdrText );
      hpHdrText = NULL;
      }
   if( lpszTo )
      {
      FreeMemory( &lpszTo );
      lpszTo = NULL;
      }
   if( lpszCC )
      {
      FreeMemory( &lpszCC );
      lpszCC = NULL;
      }
   if( lpszRecipients )
      {
      FreeMemory( &lpszRecipients );
      lpszRecipients = NULL;
      }
}

/* This function is called if the current message could not be delivered.
 */
void FASTCALL SMTPError( char * pLine )
{
   ReportOnline( pLine );
   FailedExec();
   WriteSocketLine( lpcdevSMTP, "RSET\r\n" );
   nSMTPState = NWS_WAITINGQUIT;
   CleanUpSMTPState();
}

/* Parse a complete line read from the SMTP mail server.
 */
BOOL FASTCALL SendSMTPMail( LPCOMMDEVICE lpcdev, char * pLine )
{
   static BOOL fContinue = FALSE;
   int iCode;

   /* This code must not be re-entrant!
    */
   ASSERT( FALSE == fEntry );
   fEntry = TRUE;
   if( !( nSMTPState & NWS_ISBUSY ) && isdigit( *pLine ) )
      {
      /* Read error code.
       */
      iCode = 0;
      while( isdigit( *pLine ) )
         iCode = ( iCode * 10 ) + ( *pLine++ - '0' );

      /* Check for continuity flag. If found, disregard all
       * except the last one.
       */
      if( *pLine == '-' )
         {
         fEntry = FALSE;
         return( TRUE );
         }

      /* Connection rejected by remote server./ Too many bad commands
       */
      if( iCode == 421 )
         {
         fEntry = FALSE;
         ReportOnline( pLine );
         }

      /* Insufficient system storage. !!SM!!
       */
      if( iCode == 452 )
         {
         fEntry = FALSE;
         ReportOnline( pLine );
         }

      /* Connection accepted.
       */
      if( iCode == 220 )
         {
         fCanSendHELO = TRUE; /* !!SM!! */
         fEntry = FALSE;
         return( TRUE );
         }

//    if( iCode == 334 ) // Authentication method OK

      /* Synchronization Error !!SM!!
       */
      if( iCode == 554 )
         {
         fEntry = FALSE;
         ReportOnline( pLine );
         }        

      }
   else
      iCode = 0;

   /* Jump into the state machine.
    */
   switch( nSMTPState & NWS_ACTIONMASK )
      {

      case NWS_MAILAUTH: {
         char sz[ 255 ];

         if( iCode >= 400 ) /*!!SM!!*/
            SMTPError( pLine );
         else
            {
            wsprintf( sz, "AUTH LOGIN\r\n" );
            WriteSocketLine( lpcdevSMTP, sz );
            nSMTPState = NWS_MAILAUTHUSER;
            }
         break;
         }

      case NWS_MAILAUTHUSER: {
         char sz[ 255 ];
         char szTemp[ 255 ];
         
         if( iCode >= 400 ) /*!!SM!!*/
            SMTPError( pLine );
         else
            {
            strcpy(szTemp, beCur.szAuthInfoUser );

            b64encode( (char *)&szTemp );
            wsprintf( sz, "%s\r\n", szTemp);
            WriteSocketLine( lpcdevSMTP, sz );
            nSMTPState = NWS_MAILAUTHPASS;
            }
         break;
         }

      case NWS_MAILAUTHPASS: {
         char sz[ 255 ];
//       char szTemp[ 255 ];
         char szPassword[ 40 ];

         if( iCode >= 400 ) /*!!SM!!*/
            SMTPError( pLine );
         else
            {

            memcpy( szPassword, beCur.szAuthInfoPass, sizeof(beCur.szAuthInfoPass) );
            Amuser_Decrypt( szPassword, rgEncodeKey );
            
            b64encode( (char *)&szPassword );
            
            wsprintf( sz, "%s\r\n", (LPSTR)szPassword );
            WriteSocketLine( lpcdevSMTP, sz );
            memset( szPassword, 0, sizeof(beCur.szAuthInfoPass) );
            nSMTPState = NWS_MAILFROM;
            }
         break;
         }

      case NWS_MAILFROM: {
         char sz[ 80 ];

         if( iCode >= 400 ) /*!!SM!!*/
            SMTPError( pLine );
         else
            {
            wsprintf( sz, "MAIL FROM:<%s>\r\n", smh.lpStrMailAddress );
            WriteSocketLine( lpcdevSMTP, sz );
            nSMTPState = NWS_MAILTORCPT;
            }
         break;
         }

      case NWS_MAILTORCPT: {
         if( iCode >= 400 )
            SMTPError( pLine );
         else
            {
            WriteRecipientSocketLine( lpszRecipientsPtr );
            lpszRecipientsPtr += lstrlen( lpszRecipientsPtr ) + 1;
            nSMTPState = ( *lpszRecipientsPtr != '\0' ) ? NWS_MAILTORCPT : NWS_MAILDATA;
            }
         break;
         }

      case NWS_MAILDATA:
         if( iCode >= 400 )
            SMTPError( pLine );
         else {
            char sz[ 80 ];

            wsprintf( sz, "DATA\r\n" );
            WriteSocketLine( lpcdevSMTP, sz );
            nSMTPState = NWS_MAILSENDDATA;
            }
         break;

      case NWS_MAILSENDDATA: 
         if( iCode >= 400 ) // !!SM!!
            SMTPError( pLine );
         else 
         {
            HPSTR hpMsg;
            LPSTR lpszLineBuf;
            BOOL fInitProgress = FALSE;
            WORD wCount;

            wOldCount = 0;
            hpMsg = hpHdrText;
            INITIALISE_PTR( lpszLineBuf );
            dwMsgSentSoFar = 0L;
            if( fNewMemory( &lpszLineBuf, cbLineBuf ) )
            {
               while( *hpMsg && lpcdevSMTP != NULL )
               {
                  register int c;
      
                  /* Yield.
                   */
                  TaskYield();
                  
                  /* BUGBUG: If the user clicks on Stop, lpcdevSMTP is NULL
                   * at this point. I haven't managed to cleanly close the
                   * connection, below are my attempts. Currently when you change
                   * to another message or exit Ameol, it crashes. If you fix it please
                   * mail me to tell me how you did it!
                   */

                  /* Oops! Our SMTP device has disappeared. 
                   * User probably aborted the blink. 
                   * Cleanup and return.
                   */
   /*             if( NULL == lpcdevSMTP )
                  {
                  if( smh.lpStrTo )             FreeMemory( &smh.lpStrTo );
                  if( smh.lpStrReplyTo )        FreeMemory( &smh.lpStrReplyTo );
                  if( smh.lpStrCC )             FreeMemory( &smh.lpStrCC );
                  if( smh.lpStrSubject )        FreeMemory( &smh.lpStrSubject );
                  if( smh.lpStrReply )          FreeMemory( &smh.lpStrReply );
                  if( smh.lpStrMailAddress )          FreeMemory( &smh.lpStrMailAddress );
                  if( smh.lpStrFullName )          FreeMemory( &smh.lpStrFullName );
                  if( hpEncodedPartsPtr )    FreeMemory32( &hpEncodedPartsPtr );
                  if( hpHdrText )      FreeMemory32( &hpHdrText );
                  FreeMemory( &lpszLineBuf );
                  nSMTPState = NWS_IDLE;
                  ResetDisconnectTimeout();
                  EndProcessing( 0xFFFF );
                  fEntry = FALSE;
                  return( FALSE );
                  }
   */
                  /* Find the end of the line and NULL terminate it. This
                   * makes working with strings much easier.
                   */

                  if (lpcdevSMTP != NULL) /*!!SM!!*/
                  {
                     c = 0;
                     if( *hpMsg == '.' )
                        lpszLineBuf[ c++ ] = '.';
                     while( c < cbLineBuf - 3 && *hpMsg && *hpMsg != '\r' && *hpMsg != '\n' )
                        lpszLineBuf[ c++ ] = *hpMsg++;
      //             while( c > 1 && lpszLineBuf[ c - 1 ] == ' ' )
      //                --c;
                     lpszLineBuf[ c++ ] = '\r';
                     lpszLineBuf[ c++ ] = '\n';
                     lpszLineBuf[ c ] = '\0';

                     WriteSocketLine( lpcdevSMTP, lpszLineBuf );
                     dwMsgSentSoFar += strlen( lpszLineBuf ) + 1;
                     if( dwHdrLength > SEND_PERCENTAGE_THRESHOLD )
                     {
                        if( !fInitProgress && hwndBlink )
                           {
                           ResetBlinkGauge();
                           fInitProgress = TRUE;
                           }

                        /* Update percentage.
                         */
                        wCount = (WORD)( ( (DWORD)dwMsgSentSoFar * 100 ) / (DWORD)dwHdrLength );
                        if( wCount > 100 )
                           wCount = 100;
                        if( wCount != wOldCount )
                           {
                           if ( NULL !=hwndBlink )
                              StepBlinkGauge( wOldCount );
                           else
                              OnlineStatusText( GS(IDS_STR1233), smh.lpStrSubject, wOldCount );
                           wOldCount = wCount;
                           }
                     }

                     /* Move hpMsg forward to start at the next line. Note the
                      * check for CR/CR/LF which is a soft line break under Windows.
                      */
                     if( *hpMsg == '\r' )
                        ++hpMsg;
                     if( *hpMsg == '\r' )
                        ++hpMsg;
                     if( *hpMsg == '\n' )
                        ++hpMsg;
                  }
                  else /*!!SM!!*/
                  {
                     if( smh.lpStrTo )          FreeMemory( &smh.lpStrTo );
                     if( smh.lpStrReplyTo )        FreeMemory( &smh.lpStrReplyTo );
                     if( smh.lpStrCC )          FreeMemory( &smh.lpStrCC );
                     if( smh.lpStrSubject )        FreeMemory( &smh.lpStrSubject );
                     if( smh.lpStrReply )       FreeMemory( &smh.lpStrReply );
                     if( smh.lpStrMailAddress )    FreeMemory( &smh.lpStrMailAddress );
                     if( smh.lpStrFullName )       FreeMemory( &smh.lpStrFullName );
                     if( hpEncodedPartsPtr )       FreeMemory32( &hpEncodedPartsPtr );
                     if( hpHdrText        )     FreeMemory32( &hpHdrText );
                     if( lpszLineBuf)           FreeMemory( &lpszLineBuf );
                     if( hwndBlink )
                        HideBlinkGauge();
                     nSMTPState = NWS_IDLE;
                     ResetDisconnectTimeout();
                     EndProcessing( 0xFFFF );
                     fEntry = FALSE;
                     return( FALSE );
                  }
               }
               FreeMemory( &lpszLineBuf );
               if( hwndBlink )
                  HideBlinkGauge();
               if (lpcdevSMTP != NULL)
                  WriteSocketLine( lpcdevSMTP, ".\r\n" );
               nSMTPState = NWS_MAILQUIT;
            }
         }
         break;

      case NWS_MAILQUIT:
         CleanUpSMTPState();
         if( iCode >= 400 )
            SMTPError( pLine );
         else {
            /* Report that the last message went OK
             */
            if( nThisEncodedPart <= cEncodedParts )
               {
               char sz[ 200 ];

               /* Create the header text manually.
                */
               dwHdrLength = 0;
               dwBufSize = 0;
               INITIALISE_PTR( hpHdrText );

               /* Create the next header.
                */
               CreateMailHeader( &smh );

               /* Append the next block of encoded data.
                */
               AppendMem32ToHeader( hpEncodedParts, hstrlen( hpEncodedParts ) );
               hpEncodedParts += hstrlen( hpEncodedParts ) + 1;
               ++nThisEncodedPart;

               /* Now start with the From line.
                */   
               wsprintf( sz, "MAIL FROM:<%s>\r\n", smh.lpStrMailAddress, smh.lpStrFullName );
               WriteSocketLine( lpcdevSMTP, sz );
               nSMTPState = NWS_MAILTORCPT;
               }
            else
               {
               /* Successful, so write to the event log.
                */
               wsprintf( lpTmpBuf, GS(IDS_STR1160), smh.lpStrSubject, smh.lpStrTo );
               WriteToBlinkLog( lpTmpBuf );

               /* Clean up buffers we allocated.
                */
               if( smh.lpStrTo )          FreeMemory( &smh.lpStrTo );
               if( smh.lpStrReplyTo )        FreeMemory( &smh.lpStrReplyTo );
               if( smh.lpStrCC )          FreeMemory( &smh.lpStrCC );
               if( smh.lpStrSubject )        FreeMemory( &smh.lpStrSubject );
               if( smh.lpStrReply )       FreeMemory( &smh.lpStrReply );
               if( smh.lpStrMailAddress )    FreeMemory( &smh.lpStrMailAddress );
               if( smh.lpStrFullName )       FreeMemory( &smh.lpStrFullName );
               if( hpEncodedPartsPtr )       FreeMemory32( &hpEncodedPartsPtr );
               nSMTPState = NWS_IDLE;
               if( !Blink( OBTYPE_MAILMESSAGE ) && !Blink( OBTYPE_FORWARDMAIL ) )
                  {
                  WriteSocketLine( lpcdevSMTP, "quit\r\n" );
                  nSMTPState = NWS_WAITINGQUIT;
                  fSentHELO = FALSE;
                  }
               else
                  {
                  fEntry = FALSE;
                  return( FALSE );
                  }
               }
            }
         break;

      case NWS_WAITINGQUIT:
         nSMTPState = NWS_IDLE;
         ResetDisconnectTimeout();
         EndProcessing( 0xFFFF );
         fEntry = FALSE;
         return( FALSE );
      }
   fEntry = FALSE;
   return( TRUE );
}

/* This function writes a RCPT to line to the SMTP server. If no
 * domain is specified for the recipient, the default is added.
 */
void FASTCALL WriteRecipientSocketLine( char * lpstr )
{
   char sz[ 600 ];

   if (lpstr != NULL && *lpstr) // !!SM!!
   {
      strcpy( sz, "RCPT TO:<" );
      strcat( sz, lpstr );
      if( NULL == strchr( lpstr, '@' ) )
         {
         if( *szDefMailDomain != '@' )
            strcat( sz, "@" );
         strcat( sz, szDefMailDomain );
         }
      strcat( sz, ">\r\n" );
      WriteSocketLine( lpcdevSMTP, sz );
   }
}

/* This function appends the specified line to the memory buffer.
 */
static BOOL FASTCALL AppendToHeader( LPSTR lpstr )
{
   /* Cast to long to avoid wrap round because MAX_MSGSIZE is close to 0xFFFF
    */
   if( hpHdrText == NULL )
      {
      dwBufSize = 64000;
      dwHdrLength = 0;
      fNewMemory32( &hpHdrText, dwBufSize );
      }
   else if( dwHdrLength + (DWORD)lstrlen( lpstr ) + 2L >= dwBufSize )
      {
      dwBufSize += 64000;
      fResizeMemory32( &hpHdrText, dwBufSize );
      }
   if( hpHdrText )
      {
      hmemcpy( hpHdrText + dwHdrLength, lpstr, lstrlen( lpstr ) );
      dwHdrLength += lstrlen( lpstr );
      hpHdrText[ dwHdrLength++ ] = CR;
      hpHdrText[ dwHdrLength++ ] = LF;
      hpHdrText[ dwHdrLength ] = '\0';
      return( TRUE );
      }
   return( FALSE );
}

/* This function appends a huge memory block onto the end of the
 * one we currently have.
 */
static BOOL FASTCALL AppendMem32ToHeader( HPSTR hpstr, DWORD dwSize )
{
   if( hpHdrText == NULL )
      {
      dwBufSize = dwSize + 1;
      dwHdrLength = 0;
      fNewMemory32( &hpHdrText, dwBufSize );
      }
   else if( dwHdrLength + dwSize >= dwBufSize )
      {
      dwBufSize += dwSize;
      fResizeMemory32( &hpHdrText, dwBufSize );
      }
   if( hpHdrText )
      {
      hmemcpy( hpHdrText + dwHdrLength, hpstr, dwSize );
      dwHdrLength += dwSize;
      hpHdrText[ dwHdrLength ] = '\0';
      return( TRUE );
      }
   return( FALSE );
}

/* This function appends the specified address to a list of
 * addresses making up the To: or CC: header field. Before
 * calling this function for the first time, ensure that lpszList
 * is set to To: or CC:
 */
void FASTCALL AppendToList( LPSTR lpszList, LPSTR lpszAddress, BOOL * pfFirstOnLine )
{
   BOOL fHasDomain;
   int cbRequired;
   LPSTR lpszPos;
   BOOL fWhiteSpace;

   /* Check whether we have a domain marker. If not, we
    * have to add one.
    */
   fHasDomain = NULL != strchr( lpszAddress, '@' );

   /* Compute space needed on the line.
    */
   cbRequired = lstrlen(lpszList) + lstrlen(lpszAddress);
   if( !fHasDomain )
      cbRequired += strlen( szDefMailDomain ) + 1;
   ASSERT(lstrlen(lpszList) >= 4 );

   /* If no space, then terminate this line and start
    * another.
    */
   if( cbRequired > 72 && !*pfFirstOnLine )
      {
      strcat( lpszList, "," );
      AppendToHeader( lpszList );
      strcpy( lpszList, "  " );
      }

   /* Add the new address. Append the default mail
    * domain if required.
    *
    * If this is not the first recipient in the field, then append
    * a comma. Scan from the begining of the field, look for a colon.
    * If no colon is found then line must be one that comes after the
    * the first line, so just set the pos to the first character in the
    * the line. Then look for any non-white space characters in the
    * line following pos, and if any are found we need to append a coma.
    * YH 07/06/96 10:30
    */
   if( ( lpszPos = strchr( lpszList, ':' ) ) == NULL )
      lpszPos = lpszList;
   else
      lpszPos++;
   fWhiteSpace = TRUE;
   while( fWhiteSpace && *lpszPos )
      {
      fWhiteSpace =  *lpszPos == ' ' || *lpszPos == '\r' || *lpszPos == '\n' || *lpszPos == '\t';
      lpszPos++;
      }
   if( !fWhiteSpace )
      strcat( lpszList, ", " );
   strcat( lpszList, lpszAddress );
   if( !fHasDomain )
      {
      if( *szDefMailDomain != '@' )
         strcat( lpszList, "@" );
      strcat( lpszList, szDefMailDomain );
      }
   *pfFirstOnLine = FALSE;
}

/* This function fills a To or CC list with the specified compact
 * string, expanding any group names in the process.
 */
void FASTCALL FillAddressList( LPSTR lpszList, LPSTR lpszAddressList )
{
   BOOL fFirstOnLine;

   fFirstOnLine = TRUE;
   while( *lpszAddressList )
      {
      char szLastName[ LEN_MAILADDR+1 ];
      char szName[ LEN_MAILADDR+1 ];

      /* Group name?
       */
      lpszAddressList = ExtractMailAddress( lpszAddressList, szName, LEN_MAILADDR );
      lstrcpy( szLastName, szName );
      StripLeadingTrailingQuotes( szName );
      StripCharacter( szName, '<' );
      StripCharacter( szName, '>' );
      if( Amaddr_IsGroup( szName ) )
         {
         char szGrpEntry[ AMLEN_INTERNETNAME+1 ];
         int index;

         for( index = 0; Amaddr_ExpandGroup( szName, index, szGrpEntry ); ++index )
            AppendToList( lpszList, szGrpEntry, &fFirstOnLine );
         }
      else if (*szLastName != '"')
         AppendToList( lpszList, szName, &fFirstOnLine );
      lpszAddressList += strlen( lpszAddressList ) + 1;
      }

   /* Anything left must be flushed.
    */
   if( lpszList[ 4 ] )
      AppendToHeader( lpszList );
}

/* This function filters certain lines from mail headers that are being resent
 * because of a forward command. Added by YH
 */
HPSTR FASTCALL FilterHeader( HPSTR hpszText )
{
   HPSTR    hpszBody;
   BOOL     fLineRejected;
   HPSTR    hpszTok;

   hpszBody = hpszText;

   do
      {
      hpszTok = "From ";
      fLineRejected = ( _fstrnicmp( hpszBody, hpszTok, _fstrlen( hpszTok ) ) == 0 );
      if( !fLineRejected )
         {
         hpszTok = "Received:";
         fLineRejected = ( _fstrnicmp( hpszBody, hpszTok, _fstrlen( hpszTok ) ) == 0 );
         }
      if( !fLineRejected )
         {
         hpszTok = "Memo ";
         fLineRejected = ( _fstrnicmp( hpszBody, hpszTok, _fstrlen( hpszTok ) ) == 0 );
         }

      /* Get rid of lines that start with white space */
      if( !fLineRejected )
         fLineRejected = ( hpszBody[0] == ' ' || hpszBody[0] == '\t' );

      /* Get rid of lines containing extended mailer information */
      if( !fLineRejected )
         fLineRejected = ( hpszBody[0] == 'X' && hpszBody[1] == '-' );
      
      if( fLineRejected )
         {  
         /* skip to next line */
         hpszBody = _fstrstr( hpszBody, "\r\n" ) + 2;
         }
      }
   while( fLineRejected );

   return( hpszBody );
}

/* This function will Create a compact string of addresses from groups
 * that will contain a maximum of 1 CIX addresses.
 * YH 10/06/96 14:45
 */
LPSTR FASTCALL ExpandAndFilterRecipients( LPSTR lpszTo, LPSTR lpszCC, LPSTR lpszBCC )
{
   char szName[ LEN_MAILADDR+1 ];
   BOOL fCIXMailSentThisSession;
   UINT cbFilteredListSize;
   LPSTR lpszFilteredList;
   LPSTR lpszGroupPtr;
   LPSTR lpszFiltPtr;
   LPSTR alpszLists[ 3 ];
   BOOL fCIXAddress;
   LPSTR lpszGroup;
   int nNumLists;
   LPSTR lpszPtr;
   BOOL fError;
   int i;

   /* Initialise.
    */
   lpszGroup = NULL;
   lpszGroupPtr = NULL;
   lpszPtr = NULL;
   cbFilteredListSize = 0;
   fError = FALSE;
   fCIXMailSentThisSession = FALSE;
   alpszLists[ 0 ] = lpszTo;
   alpszLists[ 1 ] = lpszCC;
   alpszLists[ 2 ] = lpszBCC;

   /* Determine the number of lists to scan.
    */
   ASSERT( lpszTo );
   nNumLists = 3;

   /* Go though To and CC field and calculate the maxmimum space
    * needed to store all addressess
    */
   for( i = 0; i < nNumLists; i++ )
      {
      lpszPtr = alpszLists[ i ];
      if( NULL == lpszPtr )
         continue;
      while( *lpszPtr )
         {
         /* If this is a group name...
          */
         lpszPtr = ExtractMailAddress( lpszPtr, szName, LEN_MAILADDR );
         StripLeadingTrailingQuotes( szName );
         StripCharacter( szName, '<' );
         StripCharacter( szName, '>' );
         if( Amaddr_IsGroup( szName ) )
            {
            char szGrpEntry[ AMLEN_INTERNETNAME+1 ];
            int index;

            for( index = 0; Amaddr_ExpandGroup( szName, index, szGrpEntry ); ++index )
               cbFilteredListSize += strlen( szGrpEntry ) + 1;
            }
         else
            cbFilteredListSize += strlen( szName ) + 1;
         lpszPtr += strlen( lpszPtr ) + 1;
         }
      }

// if (cbFilteredListSize  == 0)
//    return NULL;

   /* Now go through both To and CC fields and add all mail names
    * such that all groups are expanded but that the final list
    * contains a maximum of 1 CIX address.
    * YH 10/06/96 14:35
    */
   INITIALISE_PTR(lpszFilteredList);
   if( !fNewMemory( &lpszFilteredList, cbFilteredListSize  + 2 ) )
      fError = TRUE;
   else
      {
      lpszFiltPtr = lpszFilteredList;
      for( i = 0; i < nNumLists; i++ )
         {
         lpszPtr = alpszLists[ i ];
         if( NULL == lpszPtr )
            continue;
         while( *lpszPtr && !fError )
            {
            char szName[ LEN_MAILADDR+1 ];

            /* If this is a group name...
             */
            lpszPtr = ExtractMailAddress( lpszPtr, szName, LEN_MAILADDR );
            StripLeadingTrailingQuotes( szName );
            StripCharacter( szName, '<' );
            StripCharacter( szName, '>' );
            if( Amaddr_IsGroup( szName ) )
               {
               char szGrpEntry[ AMLEN_INTERNETNAME+1 ];
               int index;

               for( index = 0; Amaddr_ExpandGroup( szName, index, szGrpEntry ); ++index )
                  {
                  fCIXAddress = IsCixAddress( szGrpEntry );
                  if( !fCIXAddress || !fCIXMailSentThisSession )
                     {
                     strcpy( lpszFiltPtr, szGrpEntry );
                     lpszFiltPtr += strlen( szGrpEntry ) + 1;
                     }
                  }
               }
            else
               {
               /* Get here if it's a normal email address (not a group)
                */
               fCIXAddress = IsCixAddress( szName );
               if( !fCIXAddress || !fCIXMailSentThisSession )
                  {
                  strcpy( lpszFiltPtr, szName );
                  lpszFiltPtr += strlen( szName ) + 1;
                  }
               }
            lpszPtr += strlen( lpszPtr ) + 1;
            }
         }
      *lpszFiltPtr = '\0';
      }
   if( fError )
      lpszFilteredList = NULL;
   lpszFilteredList[cbFilteredListSize+1] = '\x0';
   return( lpszFilteredList );
}
