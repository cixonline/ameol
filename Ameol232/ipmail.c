/* IPMAIL.C - The Ameol POP3 client interface
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
#include "intcomms.h"
#include <memory.h>
#include <ctype.h>
#include "cixip.h"
#include "cix.h"
#include <stdio.h>
#include <string.h>
#include "cookie.h"
#include "rules.h"
#include "amlib.h"
#include "md5.h"
#include "admin.h"
#include "log.h"
#include "ameol2.h"

#define  THIS_FILE   __FILE__

#define  NWS_MAILLOGIN           0x000F
#define  NWS_MAILPASS            0x0010
#define  NWS_MAILREADY           0x0011
#define  NWS_MAILSTATS           0x0012
#define  NWS_READMSG             0x0013
#define  NWS_READMSGBODY         0x0014
#define  NWS_NEXTMSG             0x0015
#define  NWS_QUITTING            0x0016
#define  NWS_READUIDL            0x0018
#define  NWS_CLEARMSG            0x0019
#define  NWS_READTOP             0x001A
#define  NWS_READTOPBODY         0x001B
#define  NWS_ACTIONMASK          0x7FFF

#define  HPBUF_OFFSET            32
#define  MAX_HEADER              16000
#define  PERCENTAGE_THRESHOLD    10000

#define  UIDL_DAT_ID             0x5593

static BOOL fDefDlgEx = FALSE;            /* DefDlg recursion flag trap */

int nMailState = 0;                       /* Mail state machine value */
LPCOMMDEVICE lpcdevMail = NULL;           /* Handle of mail comm device */

static HEADER FAR mailhdr;                /* Mail message header */
static BOOL fEntry = FALSE;               /* Re-entrancy flag */

static char szTmpMailPassword[ 64 ];      /* Store for mail password */

static int cTotalSize;                    /* Total size of mail messages on POP3 server */
static int cNewMsgs;                      /* Number of new messages waiting */
static int cMsgsRead;                     /* Total number of mail messages read so far */
static int cMsgNumber;                    /* Number of message being read */
static int cLastReadMsg;                  /* Number of message last read */
static int cMessagesDeleted;              /* Number of messages deleted by rules */
static BOOL fClearingMail = FALSE;        /* TRUE if we're clearing mail */
static LPSTR FAR * lpuidl;                /* Array of UIDL pointers */
static int cUidl;                         /* Index into array */
static LPSTR lpuidlDat = NULL;            /* Table of user IDs read in the past */
#define BIGUIDL
#ifndef BIGUIDL
static UINT cbuidlDat;                    /* Size of the table */
#else BIGUIDL
static DWORD cbuidlDat;                   /* Size of the table */ // !!SM!!
#endif BIGUIDL
static char * pszHeader;                  /* Header collected via TOP */
static UINT cbHeader;                     /* Size of header collected via TOP */

static HPSTR hpMailBuf;                   /* Pointer to memory buffer */
static DWORD dwMailMsgLength;             /* Length of message so far */
static DWORD dwBufSize;                   /* Size of buffer */
static DWORD dwMsgSize;                   /* Actual size of message (returned by POP3) */
static DWORD dwMsgSoFar;                  /* Size of message collected so far */
static WORD wOldCount;                    /* Percentage count */
static BOOL fBlankLine = FALSE;           /* Flag used to fold In-Reply-To into header */
char szTmpName[ 64 ];

/* List of POP3 commands.
 */
static char szPCMD_Quit[] = "QUIT\r\n";
static char szPCMD_Retr[] = "RETR %u\r\n";
static char szPCMD_Stat[] = "STAT\r\n";
static char szPCMD_Uidl[] = "UIDL\r\n";
static char szPCMD_User[] = "USER %s\r\n";
static char szPCMD_Pass[] = "PASS %s\r\n";
static char szPCMD_Dele[] = "DELE %u\r\n";
static char szPCMD_Top[] = "TOP %u %u\r\n";

#define  TOP_PARAM      0

typedef struct tagUIDLSTRUCT {
   WORD wDate;
   WORD wTime;
   char szUidl[ 64 ];
} UIDLSTRUCT;

/* UIDL database.
 */
static char szUidlDataFile[] = "UIDL.DAT";
static char szUidlTempFile[] = "UIDL.$$$";

static char FAR szDefaultAuthor[] = "[No-author]\0";

BOOL EXPORT CALLBACK MailClientWndProc( LPCOMMDEVICE, int, DWORD, LPARAM );

BOOL EXPORT FAR PASCAL POP3Login( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL POP3Login_DlgProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL POP3Login_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL POP3Login_OnCommand( HWND, int, HWND, UINT );

BOOL FASTCALL ParseMail( LPCOMMDEVICE, char * );
BOOL FASTCALL EnsurePOP3Password( void );
BOOL FASTCALL EnsureMailWindow( void );
void FASTCALL ClearMailMessage( void );
void FASTCALL NextMailMessage( void );
void FASTCALL FreeUidlArray( void );
void FASTCALL AppendToMailMessage( char *, BOOL );

BOOL FASTCALL ReadUidlDataFile( void );
BOOL FASTCALL SaveUidlDataFile( void );
void FASTCALL AddToUidlDataFile( char * );
BOOL FASTCALL LocateInUidlDataFile( char *, UIDLSTRUCT * );
void FASTCALL DeleteUidlDataFile( void );

void FASTCALL AddToUidlDataFileWithID( char * pszUidl, UIDLSTRUCT * puidls ); /*!!SM!!*/
void FASTCALL RemoveExtraUIDLs(void); /*!!SM!!*/

static BOOL FASTCALL PreallocateBuffer( DWORD );

/* This function clears mail from the POP3 server.
 */
int FASTCALL Exec_ClearIPMail( LPVOID dwData )
{
   /* Context switch into appropriate service.
    */
   SwitchService( SERVICE_IP );

   /* If we're busy, this request goes into the
    * out-basket.
    */
   if( nMailState != NWS_IDLE )
      return( POF_AWAITINGSERVICE );

   /* Do we need the password?
    */
   if( !EnsurePOP3Password() )
      return( POF_CANNOTACTION );

   /* Clearing mail.
    */
   fClearingMail = TRUE;

   /* Start up the connection.
    */
   nMailState = NWS_MAILLOGIN;
   if( !EnsureMailWindow() )
      return( POF_CANNOTACTION );

   /* And that's all we have to do! :-)
    */
   return( POF_PROCESSSTARTED );
}

/* This function ensures that we have a POP3 password.
 */
BOOL FASTCALL EnsurePOP3Password( void )
{
   if( !fRememberPassword )
      {
      if( !Adm_Dialog( hRscLib, hwndFrame, MAKEINTRESOURCE(IDDLG_POP3LOGIN), POP3Login, 0L ) )
         return( FALSE );
      }
   else
      memcpy( szTmpMailPassword, szMailPassword, sizeof(szTmpMailPassword) );
   return( TRUE );
}

/* This function reads new mail from the POP3 server.
 */
int FASTCALL Exec_GetNewMail( LPVOID dwData )
{
   cMessagesDeleted = 0; // !!SM!! 2042

   /* Context switch into appropriate service.
    */
   SwitchService( SERVICE_IP );

   /* If we're busy, this request goes into the
    * out-basket.
    */
   if( nMailState != NWS_IDLE )
      return( POF_AWAITINGSERVICE );

   /* Do we need the password?
    */
   if( !EnsurePOP3Password() )
      return( POF_CANNOTACTION );

   /* Not clearing mail.
    */
   fClearingMail = FALSE;

   /* Start up the connection.
    */
   nMailState = NWS_MAILLOGIN;
   if( !EnsureMailWindow() )
      return( POF_CANNOTACTION );

   /* And that's all we have to do! :-)
    */
   return( POF_PROCESSSTARTED );
}

/* This function ensures that a mail window is open for processing.
 */
BOOL FASTCALL EnsureMailWindow( void )
{
   LPCOMMDESCRIPTOR lpcdMail;
   char * pszLogFile;

   INITIALISE_PTR(lpcdMail);

   /* Go no further if not allowed to use CIX internet.
    */
   if( !TestPerm(PF_CANUSECIXIP) )
      return( FALSE );

   /* Return now if lpcdevMail is not NULL, indicating that
    * a mail server device is already open.
    */
   if( NULL != lpcdevMail )
      {
      SuspendDisconnectTimeout();
      return( TRUE );
      }

   /* Create a temporary connection card.
    */
   if( !fNewMemory( &lpcdMail, sizeof(COMMDESCRIPTOR) ) )
      return( FALSE );

   memset( lpcdMail, 0, sizeof(COMMDESCRIPTOR) );
   strcpy( lpcdMail->szTitle, "$POP3" );

   /* Create an internet section in the CC.
    */
   if( !fNewMemory( &lpcdMail->lpic, sizeof(IPCOMM) ) )
      {
      FreeMemory( &lpcdMail );
      return( FALSE );
      }

   lpcdMail->wProtocol = PROTOCOL_NETWORK;
   lpcdMail->lpic->wPort = (UINT)Amuser_GetPPLong( szSettings, "POP3Port", 110 );
   lpcdMail->nTimeout = 60;
#ifdef WIN32
   lpcdMail->lpic->rd = rdDef;
#endif
   strcpy( lpcdMail->lpic->szAddress, szMailServer );
   
   /* Create a logging file.
    */
   pszLogFile = NULL;
   if( fLogPOP3 )
   {
      char szTemp[_MAX_PATH];
      BEGIN_PATH_BUF;
      Amuser_GetActiveUserDirectory( lpPathBuf, _MAX_PATH );
      strcat( strcat( lpPathBuf, "\\" ), "pop3.log" );
      pszLogFile = (char *)lpPathBuf;
      if(GetShortPathName( pszLogFile, (char *)&szTemp, _MAX_PATH ) > 0 )
         wsprintf( lpTmpBuf, GS(IDS_STR1117), "POP3", (char *)szTemp );
      else
         wsprintf( lpTmpBuf, GS(IDS_STR1117), "POP3", (char *)pszLogFile );
      WriteToBlinkLog( lpTmpBuf );
      END_PATH_BUF;
   }

   /* Open the POP3 client.
    */
   OnlineStatusText( GS(IDS_STR709) );
   if( fLogPOP3 )
      WriteToBlinkLog( GS(IDS_STR709) );
   if( Amcomm_OpenCard( hwndFrame, &lpcdevMail, lpcdMail, MailClientWndProc, 0L, pszLogFile, NULL, FALSE, FALSE ) )
      {
      SuspendDisconnectTimeout();
      OnlineStatusText( GS(IDS_STR713) );
      if( fLogPOP3 )
         WriteToBlinkLog( GS(IDS_STR713) );

      }
   else
      {
      wsprintf( lpTmpBuf, GS(IDS_STR1082), szMailServer, Amcomm_GetLastError() );
      ReportOnline( lpTmpBuf );
      WriteToBlinkLog( lpTmpBuf );
      nMailState = NWS_IDLE;
      }
   return( lpcdevMail != NULL );
}

/* This function handles all messages for the mail server.
 */
BOOL EXPORT CALLBACK MailClientWndProc( LPCOMMDEVICE lpcdev, int nCode, DWORD dwAppData, LPARAM lParam )
{
   switch( nCode )
      {
      case CCB_DISCONNECTED:
         OnlineStatusText( GS(IDS_STR714) );
         if( fLogPOP3 )
            WriteToBlinkLog( GS(IDS_STR714) );
         nMailState = NWS_IDLE;
         SaveUidlDataFile();
         FreeUidlArray();
         if( NULL != lpuidlDat )
            FreeMemory( &lpuidlDat );
         Amcomm_Close( lpcdevMail );
         Amcomm_Destroy( lpcdevMail );
         if( fLogPOP3 )
            {
            Amuser_GetActiveUserDirectory( lpTmpBuf, _MAX_PATH );
            strcat( strcat( lpTmpBuf, "\\" ), "pop3.log" );
            Ameol2_Archive( lpTmpBuf, FALSE );
            }
         lpcdevMail = NULL;
         EndProcessing( 0xFFFF );
         return( TRUE );

      case CCB_DATAREADY:
         ReadAndProcessSocket( lpcdev, (LPBUFFER)lParam, ParseMail );
         break;
      }
   return( FALSE );
}

/* Parse a complete line read from the POP3 mail server.
 */
BOOL FASTCALL ParseMail( LPCOMMDEVICE lpcdev, char * pLine )
{
   BOOL fServiceActive;
   char sz[ 60 ];
   char redactedPass[ 60 ];
   BOOL fOk;

   /* This code must not be re-entrant!
    */
   ASSERT( FALSE == fEntry );
   fEntry = TRUE;
   fServiceActive = TRUE;

   /* Set fOk if the +OK string is detected.
    */
   fOk = TRUE;
   if( !( nMailState & NWS_ISBUSY ) )
      if( strncmp( pLine, "+OK", 3 ) != 0 )
         fOk = FALSE;
      else
         pLine += 3;

   /* Jump into the state machine.
    */
   switch( nMailState & NWS_ACTIONMASK )
      {
      case NWS_QUITTING:
         nMailState = NWS_IDLE;
         ResetDisconnectTimeout();
         SendMessage( hwndFrame, WM_UPDATE_UNREAD, 0, 0L );
         fServiceActive = FALSE;
         break;

      case NWS_MAILLOGIN:
         /* Logged into POP3 server OK, so send user name.
          */
         OnlineStatusText( GS(IDS_STR1187) );
         if( fLogPOP3 )
            WriteToBlinkLog( GS(IDS_STR1187) );
         wsprintf( sz, szPCMD_User, (LPSTR)szMailLogin );
         WriteSocketLine( lpcdevMail, sz );
         nMailState = NWS_MAILPASS;
         break;

      case NWS_MAILPASS:
         if( !fOk )
            {
            /* Invalid user name, so quit the server.
             */
            FailedExec();
            ReportOnline( pLine );
            wsprintf( sz, szPCMD_Quit );
            WriteSocketLine( lpcdevMail, sz );
            nMailState = NWS_QUITTING;
            OnlineStatusText( GS(IDS_STR719) );
            }
         else
            {
            /* Valid user name, so send password.
             */
            OnlineStatusText( GS(IDS_STR1188) );
            if( fLogPOP3 )
               WriteToBlinkLog( GS(IDS_STR1188) );
            Amuser_Decrypt( szTmpMailPassword, rgEncodeKey );
            wsprintf( sz, szPCMD_Pass, (LPSTR)szTmpMailPassword );
            wsprintf( redactedPass, szPCMD_Pass, "****************" );
            memset( szTmpMailPassword, 0, sizeof(szTmpMailPassword) );
            WriteSocketLineRedacted( lpcdevMail, sz, redactedPass );
            nMailState = NWS_MAILREADY;
            }
         break;

      case NWS_MAILREADY: {
         if( !fOk )
            {
            /* Invalid password, so quit server.
             */
            FailedExec();
            ReportOnline( pLine );
            WriteSocketLine( lpcdevMail, szPCMD_Quit );
            nMailState = NWS_QUITTING;
            OnlineStatusText( GS(IDS_STR1227) );
            }
         else
            {
            if( fClearingMail )
            {
               OnlineStatusText( GS(IDS_STR1056) );
               ReportOnline( GS(IDS_STR1056) );
            }
            else
               OnlineStatusText( GS(IDS_STR1055) );
            WriteSocketLine( lpcdevMail, szPCMD_Stat );
            nMailState = NWS_MAILSTATS;
            }
         break;
         }

      case NWS_MAILSTATS:
         if( !fOk )
            {
            /* Error, for some reason. (Is this really
             * possible?). Quit the server.
             */
            FailedExec();
            ReportOnline( GS(IDS_STR860) );
            WriteSocketLine( lpcdevMail, szPCMD_Quit );
            nMailState = NWS_QUITTING;
            }
         else
            {
            sscanf( pLine, "%u %u", &cNewMsgs, &cTotalSize );
            if( cNewMsgs == 0 )
               {
               /* No new messages, so quit now. Delete any
                * UIDL.DAT file.
                */
               if( !fClearingMail )
               {
               OnlineStatusText( GS(IDS_STR1078) );
               ReportOnline( GS(IDS_STR1078) );
               }
               DeleteUidlDataFile();
               WriteSocketLine( lpcdevMail, szPCMD_Quit );
               nMailState = NWS_QUITTING;
               break;
               }

            /* Allocate an array for storing the unique IDs
             * of the messages.
             */
            INITIALISE_PTR(lpuidl);
            if( !fNewMemory( (LPVOID)&lpuidl, cNewMsgs * sizeof(LPSTR) ) )
               {
               WriteSocketLine( lpcdevMail, szPCMD_Quit );
               ReportOnline( GS(IDS_STR861) );
               nMailState = NWS_QUITTING;
               }
            else
               {
               cUidl = 0;

               /* Issue the UIDL command to get a list of
                * unique messages IDs.
                */
               WriteSocketLine( lpcdevMail, szPCMD_Uidl );
               nMailState = NWS_READUIDL;
               cMsgsRead = 0;
               }
            }
         break;

      case NWS_READUIDL:
         if( !fOk )
            {
            /* UIDL not supported. Revert to different algorithm
             * using TOP and MD5 instead.
             */
            WriteToBlinkLog( GS(IDS_STR859) );
            cMsgNumber = 0;
            pszHeader = NULL;
            wsprintf( sz, szPCMD_Top, cUidl + 1, TOP_PARAM );
            WriteSocketLine( lpcdevMail, sz );
            nMailState = NWS_READTOP;
            }
         else if( !( nMailState & NWS_ISBUSY ) )
            nMailState |= NWS_ISBUSY;
         else if( cUidl == cNewMsgs || pLine[ 0 ] == '.' && pLine[ 1 ] == '\0' )
            {
            /* Okay, we've now got all the uidls we need. Compare
             * the array against UIDL.DAT and do a RETR for all
             * IDs which don't appear. Start from the most recent
             * for performance reasons.
             */
            cMsgNumber = 0;
            if( !ReadUidlDataFile() )
               FreeMemory( (LPVOID)&lpuidl );
            if( fClearingMail )
               ClearMailMessage();
            else
               NextMailMessage();
            }
         else if( NULL != ( pLine = strchr( pLine, ' ' ) ) )
            {
            ASSERT( cUidl < cNewMsgs );
            lpuidl[ cUidl++ ] = _strdup( ++pLine );
            }
         break;

      case NWS_READTOP:
         if( !fOk )
            {
            /* Okay, TOP doesn't work, so start from the first
             * message.
             */
            cMsgNumber = 0;
            if( !fDeleteMailAfterReading )
               {
               fDeleteMailAfterReading = TRUE;
               ReportOnline( GS(IDS_STR1132) );
               }
            NextMailMessage();
            }
         else
            nMailState = NWS_READTOPBODY|NWS_ISBUSY;
         break;

      case NWS_READTOPBODY:
         if( pLine[ 0 ] == '.' && pLine[ 1 ] == '\0' )
            {
            /* Here we do the MD5 stuff.
             */
            if( NULL != pszHeader )
               {
               char szMDDigest[ 50 ];
               BYTE digest[ 16 ];
               register int i;
               MD5_CTX context;

               /* Make MD5 from header.
                */
               MD5Init( &context );
               MD5Update( &context, pszHeader, strlen(pszHeader) );
               MD5Final( digest, &context );

               /* Make a printable digest string.
                */
               for( i = 0; i < 12; ++i )
                  wsprintf( szMDDigest + ( i * 2 ), "%02x", digest[ i ] );

               /* Got all of TOP header, so store UIDL created so
                * far in lpuidl array. If all headers retrieved,
                * scan the array.
                */
               ASSERT( cUidl < cNewMsgs );
               lpuidl[ cUidl++ ] = _strdup( szMDDigest );

               /* Finish with that header string.
                */
               FreeMemory( &pszHeader );
               }

            /* All done?
             */
            if( cUidl == cNewMsgs )
               {
               cMsgNumber = 0;
               if( !ReadUidlDataFile() )
                  FreeMemory( (LPVOID)&lpuidl );
               if( fClearingMail )
                  ClearMailMessage();
               else
                  NextMailMessage();
               }
            else
               {
               wsprintf( sz, szPCMD_Top, cUidl + 1, TOP_PARAM );
               WriteSocketLine( lpcdevMail, sz );
               nMailState = NWS_READTOP;
               }
            }
         else
            {
            /* Add this line to a local buffer of
             * lines collected so far.
             */
            if( NULL == pszHeader )
               {
               fNewMemory( &pszHeader, MAX_HEADER );
               pszHeader[ 0 ] = '\0';
               cbHeader = 0;
               }
            if( NULL != pszHeader )
               {
               /* DON'T store the Status line, because the POP3 mailer
                * adds this line itself after we've done a RETR on a 
                * message. This confuses the MD5 code royally!
                */
               if( memcmp( pLine, "Status:", 7 ) != 0 )
                  {
                  int length;

                  length = strlen( pLine );
                  if( length + cbHeader < MAX_HEADER )
                     {
                     strcat( pszHeader, pLine );
                     cbHeader += length;
                     }
                  }
               }
            }
         break;

      case NWS_CLEARMSG:
         ClearMailMessage();
         break;

      case NWS_READMSG:
         if( fOk )
            {
            /* Get size of this message.
             */
            dwMsgSize = atol( pLine );
            dwMsgSoFar = 0L;

            /* Preallocate a buffer for the message
             */
            PreallocateBuffer( dwMsgSize + 1 );

            /* Confirmed reading mail message, so get the body of the
             * message.
             */
            InitialiseHeader( &mailhdr );
            mailhdr.fInHeader = TRUE;
            mailhdr.fCheckForInReplyTo = TRUE;
            fBlankLine = FALSE;
            nMailState = NWS_READMSGBODY|NWS_ISBUSY;
            hpMailBuf = NULL;
            dwMailMsgLength = HPBUF_OFFSET;
            mailhdr.ptl = GetPostmasterFolder();

            /* Feedback on the status bar.
             */
            wOldCount = 0;
            if( dwMsgSize < PERCENTAGE_THRESHOLD )
               OnlineStatusText( GS(IDS_STR1148), cMsgsRead + 1 );
            else
               {
               if ( NULL !=hwndBlink )
               {
               ResetBlinkGauge();
//             OnlineStatusText( GS(IDS_STR986), cMsgsRead + 1, wOldCount );
               OnlineStatusText( GS(IDS_STR1148), cMsgsRead + 1 );
               }
               else
                  OnlineStatusText( GS(IDS_STR715), cMsgsRead + 1, wOldCount );
               }
            break;
            }

         /* Fall thru to code that tries next message
          * number.
          */
         NextMailMessage();
         break;

      case NWS_NEXTMSG:
         NextMailMessage();
         break;

      case NWS_READMSGBODY:
         if( pLine[ 0 ] == '.' && pLine[ 1 ] == '\0' )
            {
            char sz[ 20 ];
            HEADER temphdr;
            char szTempAuthor[ LEN_INTERNETNAME + 1   ];
            DWORD dwLclFlags;

            /* Show final percentage.
             */
            if( wOldCount > 0 )
               {
               if ( NULL !=hwndBlink )
               {
                  StepBlinkGauge( 100 );
                  HideBlinkGauge();
               }
               else
                  OnlineStatusText( GS(IDS_STR715), cMsgsRead + 1, 100 );
               }

            /* We're all done. Add this message to the database, then
             * skip to the next message, if any.
             */
            if( dwMailMsgLength )
               {
               int nMatch;
               PTL ptl;

               ptl = mailhdr.ptl;
               nMatch = MatchRule( &ptl, hpMailBuf + HPBUF_OFFSET, &mailhdr, FALSE );
               if( !( nMatch & FLT_MATCH_COPY ) && NULL != ptl && ptl != mailhdr.ptl )
                  {
                     char szTmpName[ 64 ];

                     Amdb_GetCookie( ptl, MAIL_ADDRESS_COOKIE, szTmpName, szMailAddress, 64 );
                     if( _stricmp( szTmpName, mailhdr.szAuthor ) == 0 )
                     {
                        char szAddress[ LEN_MAILADDR + 1 ];
                        register int c;
                        char * pszAddress;
                        char szMyAddress[ LEN_MAILADDR ];
                        
                        ParseToField( mailhdr.szTo, szMyAddress, LEN_TOCCLIST );

                        /* Extract one address
                         */
                        for( c = 0; szMyAddress[ c ] && szMyAddress[ c ] != ' '; ++c )
                           if( c < LEN_MAILADDR )
                              szAddress[ c ] = szMyAddress[ c ];
                        szAddress[ c ] = '\0';
                        if( NULL != ( pszAddress = strchr( szAddress, '@' ) ) )
                           if( IsCompulinkDomain( pszAddress + 1 ) )
                              *pszAddress = '\0';

                        strcpy( mailhdr.szAuthor, szAddress );
                        mailhdr.fPriority = TRUE;
                        mailhdr.fOutboxMsg = TRUE;
                     }


                  }
               if( mailhdr.fOutboxMsg )
                  {
                     InitialiseHeader( &temphdr );
                     ParseMailAuthor( mailhdr.szFrom, &temphdr );
                     strncpy( szTempAuthor, mailhdr.szAuthor, LEN_INTERNETNAME );
                     strncpy( mailhdr.szAuthor, temphdr.szAuthor, LEN_INTERNETNAME );
                  }

               /* Add a blank line.
                */
               AppendToMailMessage( "", TRUE );

               /* Apply the filter.
                */
               ptl = mailhdr.ptl;
               nMatch = MatchRule( &ptl, hpMailBuf + HPBUF_OFFSET, &mailhdr, TRUE );
               if( !( nMatch & FLT_MATCH_COPY ) && ( ptl != mailhdr.ptl ) )
                  mailhdr.ptl = ptl;
               if( mailhdr.fOutboxMsg )
                  strcpy( mailhdr.szAuthor, szTempAuthor);
               if( !( nMatch & FLT_MATCH_DELETE ) )
                  {
                  TOPICINFO topicinfo;
                  DWORD dwMsgNumber;
                  int cbHdr;
                  PTH pth;
                  int r;

                  /* Compute local flags.
                   */
                  char szSubject[ 80 ];
                  Amdb_GetCookie( mailhdr.ptl, SUBJECT_IGNORE_TEXT, szSubject, "", 80 );
                  if( *szSubject )
                     mailhdr.fPossibleRefScanning = TRUE;
                  dwLclFlags = 0;
                  if( ( nMatch & FLT_MATCH_PRIORITY ) || mailhdr.fPriority )
                     dwLclFlags |= MF_PRIORITY;
                  if( mailhdr.fCoverNote && !mailhdr.fPriority )
                     dwLclFlags |= MF_COVERNOTE;
                  if( mailhdr.fOutboxMsg )
                     dwLclFlags |= MF_OUTBOXMSG;
                  if( mailhdr.fNotification && !mailhdr.fOutboxMsg )
                     dwLclFlags |= MF_NOTIFICATION;

                  /* If this message had an Re: and no matching message ID, scan for the
                   * original by doing Re: pattern matching.
                   */
                  Amdb_LockTopic( mailhdr.ptl );
                  if( *mailhdr.szReference )
                     {
                     DWORD dwPossibleComment;

                     if( dwPossibleComment = Amdb_MessageIdToComment( mailhdr.ptl, mailhdr.szReference ) )
                        {
                           mailhdr.dwComment = dwPossibleComment;
                           mailhdr.fPossibleRefScanning = FALSE;
                        }
                     }
                  if( mailhdr.dwComment == 0L && mailhdr.fPossibleRefScanning && fThreadMailBySubject )
                     mailhdr.dwComment = Amdb_LookupSubjectReference( mailhdr.ptl, mailhdr.szTitle, szSubject );

                  /* Set a title if there wasn't one.
                   */
                  if( *mailhdr.szTitle == '\0' )
                     strcpy( mailhdr.szTitle, GS(IDS_STR933) );

                  /* Get the topic info and use that to
                   * create a mail header.
                   */
                  Amdb_GetTopicInfo( mailhdr.ptl, &topicinfo );
                  dwMsgNumber = topicinfo.dwMaxMsg + 1;
                  cbHdr = wsprintf( hpMailBuf, "Memo #%lu (%lu)\r\n", dwMsgNumber, dwMailMsgLength - HPBUF_OFFSET );
                  ASSERT( cbHdr <= 32 );
                  hmemcpy( hpMailBuf + cbHdr, hpMailBuf + HPBUF_OFFSET, dwMailMsgLength - HPBUF_OFFSET );
                  dwMailMsgLength -= HPBUF_OFFSET - cbHdr;
                  if( strlen (mailhdr.szAuthor ) == 0 )
                     strcpy( mailhdr.szAuthor, szDefaultAuthor );
                  r = Amdb_CreateMsg( mailhdr.ptl, &pth, dwMsgNumber, mailhdr.dwComment, mailhdr.szTitle, mailhdr.szAuthor, mailhdr.szMessageId, &mailhdr.date, &mailhdr.time, hpMailBuf, dwMailMsgLength, dwLclFlags );
                  fNewMail = TRUE;


                  if( r == AMDBERR_NOERROR )
                  {
                  /* Take post storage action based on rules.
                   */
                  if( nMatch & FLT_MATCH_COPY && ( ptl != mailhdr.ptl ) )
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
                        DoCopyMsgRange( hwndFrame, lppth, 1, ptl, FALSE, FALSE, FALSE );
                        }
                     }
                  if( ( nMatch & FLT_MATCH_PRIORITY ) || mailhdr.fPriority )
                     Amdb_MarkMsgPriority( pth, TRUE, TRUE );
                  if( nMatch & FLT_MATCH_READ )
                     Amdb_MarkMsgRead( pth, TRUE );
                  if( nMatch & FLT_MATCH_IGNORE )
                     Amdb_MarkMsgIgnore( pth, TRUE, TRUE );
                  if( nMatch & FLT_MATCH_READLOCK )
                     Amdb_MarkMsgReadLock( pth, TRUE );
                  if( nMatch & FLT_MATCH_KEEP )
                     Amdb_MarkMsgKeep( pth, TRUE );
                  if( nMatch & FLT_MATCH_MARK )
                     Amdb_MarkMsg( pth, TRUE );
                  if( mailhdr.fOutboxMsg )
                     Amdb_SetOutboxMsgBit( pth, TRUE );
                  if( mailhdr.fCoverNote )
                     Amdb_GetSetCoverNoteBit( pth, BSB_SETBIT );
                  if( mailhdr.fNotification && !mailhdr.fOutboxMsg )
                     Amdb_GetSetNotificationBit( pth, BSB_SETBIT );

                  }

                  SendAllWindows( WM_UPDATE_UNREAD, 0, (LPARAM)mailhdr.ptl );
                  
                  /* For POP3 mail, be cautious and commit the topic changes.
                   */
                  Amdb_InternalUnlockTopic( mailhdr.ptl );
                  Amdb_CommitCachedFiles();
                  /* Report to blinks log.
                   */
                  wsprintf( lpTmpBuf, GS(IDS_STR1161), mailhdr.szTitle, mailhdr.szFrom );
                  WriteToBlinkLog( lpTmpBuf );
                  }
               if( ( nMatch & FLT_MATCH_DELETE ) )
                  ++cMessagesDeleted;
               FreeMemory32( &hpMailBuf );
               dwMailMsgLength = HPBUF_OFFSET;
               hpMailBuf = NULL;
               mailhdr.dwComment = 0L;
               }

            /* Bump count.
             */
            ++cMsgsRead;
            ASSERT( cNewMsgs != 0 );

            /* Move onto the next message.
             */
            if( fDeleteMailAfterReading )
               {
//             WriteSocketLine( lpcdevMail, "\r\n" ); // !!SM!!
               wsprintf( sz, szPCMD_Dele, cMsgNumber );
               WriteSocketLine( lpcdevMail, sz );
               nMailState = NWS_NEXTMSG;
               }
            else
               {
               if( NULL != lpuidl )
                  AddToUidlDataFile( lpuidl[ cMsgNumber - 1 ] );
               NextMailMessage();
               }
            }
         else
            {
            WORD wCount;

            /* First, add this line to the memory buffer.
             */
            if( *pLine == '\0' && mailhdr.fCheckForInReplyTo )
               fBlankLine = TRUE;
            if( !fBlankLine )
               {
               BOOL fNewline;

               fNewline = ApplyEncodingType( pLine, &mailhdr );
               AppendToMailMessage( pLine, fNewline );
               }

            /* Update percentage.
             */
            dwMsgSoFar += strlen( pLine ) + 1;
            if( dwMsgSize >= PERCENTAGE_THRESHOLD )
               {
               wCount = (WORD)( ( (DWORD)dwMsgSoFar * 100 ) / (DWORD)dwMsgSize );
               if( wCount != wOldCount )
                  {
                  if ( NULL !=hwndBlink )
                  {
//                   OnlineStatusText( GS(IDS_STR986), cMsgsRead + 1, wOldCount );
                     StepBlinkGauge( wOldCount );
                  }
                  else
                     OnlineStatusText( GS(IDS_STR715), cMsgsRead + 1, wOldCount );
                  wOldCount = wCount;
                  }
               }

            /* Next, parse the contents.
             */
            if( ParseSocketLine( pLine, &mailhdr ) )
               {
               AppendToMailMessage( pLine, TRUE );
               AppendToMailMessage( "", TRUE );
               fBlankLine = FALSE;
               }
            else if( fBlankLine && *pLine != '\0' )
               {
               BOOL fNewline;

               AppendToMailMessage( "", TRUE );
               fNewline = ApplyEncodingType( pLine, &mailhdr );
               AppendToMailMessage( pLine, fNewline );
               fBlankLine = FALSE;
               }
            }
         break;
      }
   fEntry = FALSE;
   return( fServiceActive );

}

/* This function deletes mail from the POP3 server.
 */
void FASTCALL ClearMailMessage( void )
{
   char sz[ 20 ];

   if( cMsgNumber < cNewMsgs )
      {
      UIDLSTRUCT uidls;

      /* Get the next message number and locate it in the
       * UIDL.DAT data file. Repeat until we find one that
       * doesn't occur in the file.
       */
      while( cMsgNumber < cNewMsgs )
         if( LocateInUidlDataFile( lpuidl[ cMsgNumber++ ], &uidls ) )
            {
            AM_DATE date;
            WORD wClearDate;

            Amdate_GetCurrentDate( &date );
            Amdate_AdjustDate( &date, -nIPMailClearCount );
            wClearDate = Amdate_PackDate( &date );
            if( uidls.wDate <= wClearDate )
               {
               //WriteSocketLine( lpcdevMail, "\r\n" );
               wsprintf( sz, szPCMD_Dele, cMsgNumber ); // !!SM!!
               WriteSocketLine( lpcdevMail, sz );
               nMailState = NWS_CLEARMSG;
               return;
               }
            }
      }
   if( cMsgNumber == cNewMsgs )
      {
      /* Report
       */
      if( 0 != cMsgsRead )
         {
         wsprintf( lpTmpBuf, GS(IDS_STR1080), cMsgsRead );
         WriteToBlinkLog( lpTmpBuf );
         }

      /* Send QUIT to the server.
       */
      WriteSocketLine( lpcdevMail, szPCMD_Quit );
      nMailState = NWS_QUITTING;

      /* Deallocate the UIDL array.
       */
      SaveUidlDataFile();
      if( NULL != lpuidlDat )
         FreeMemory( &lpuidlDat );
      FreeUidlArray();
      }
}

/* This function retrieves the next message from the POP3 server.
 * If all mail has been retrieved, it quits the server.
 */
void FASTCALL NextMailMessage( void )
{
   char sz[ 20 ];

   if( cMsgNumber < cNewMsgs )
      {
      UIDLSTRUCT uidl;

      if( NULL == lpuidl )
         {
         /* Send RETR with next message number.
          */
         wsprintf( sz, szPCMD_Retr, ++cMsgNumber );
         WriteSocketLine( lpcdevMail, sz );
         nMailState = NWS_READMSG;
         return;
         }

      /* Get the next message number and locate it in the
       * UIDL.DAT data file. Repeat until we find one that
       * doesn't occur in the file.
       */
      while( cMsgNumber < cNewMsgs )
         if( !LocateInUidlDataFile( lpuidl[ cMsgNumber++ ], &uidl ) )
            {
            wsprintf( sz, szPCMD_Retr, cMsgNumber );
            WriteSocketLine( lpcdevMail, sz );
            nMailState = NWS_READMSG;
            return;
            }
      }
   if( cMsgNumber == cNewMsgs )
      {
      /* Report
       */
      if( 0 == cMsgsRead )
      {
         OnlineStatusText( GS(IDS_STR1078) );
         WriteToBlinkLog( GS(IDS_STR1078) );
      }
      else
         {
         wsprintf( lpTmpBuf, GS(IDS_STR1079), cMsgsRead );
         WriteToBlinkLog( lpTmpBuf );
         OnlineStatusText( lpTmpBuf );
         }
      if( cMessagesDeleted )
      {
         wsprintf( lpTmpBuf, "%u message(s) deleted by rules", cMessagesDeleted );
         WriteToBlinkLog( lpTmpBuf );
      }
      /* Send QUIT to the server.
       */
      WriteSocketLine( lpcdevMail, szPCMD_Quit );
      nMailState = NWS_QUITTING;

      /* Deallocate the UIDL array.
       */
      SaveUidlDataFile();
      if( NULL != lpuidlDat )
         FreeMemory( &lpuidlDat );
      FreeUidlArray();
      }
}

/* This function opens the UIDL.DAT file and reads it into
 * memory. The lpuidlDat points to the file in memory and
 * cbuidlDat is the size of the file in bytes.
 */
BOOL FASTCALL ReadUidlDataFile( void )
{
   HNDFILE fh;

   /* Open the file.
    */
   Amuser_GetActiveUserDirectory( lpTmpBuf, _MAX_PATH );
   strcat( strcat( lpTmpBuf, "\\" ), szUidlDataFile );
   fh = Amfile_Open( lpTmpBuf, AOF_READWRITE );
   if( HNDFILE_ERROR != fh )
      {
      /* Get the file size.
       */
#ifndef BIGUIDL
      cbuidlDat = (int)Amfile_GetFileSize( fh ); //!!SM!!
#else BIGUIDL
      cbuidlDat = (DWORD)Amfile_GetFileSize( fh );
#endif BIGUIDL

      /* Allocate a block of memory to contain it, then
       * read in the data. Check that the first two bytes
       * contain the version ID.
       */
      if( fNewMemory( &lpuidlDat, cbuidlDat ) )
#ifndef BIGUIDL
         if( cbuidlDat != Amfile_Read( fh, lpuidlDat, cbuidlDat ) ) // !!SM!!
#else BIGUIDL
         if( cbuidlDat != Amfile_Read32( fh, lpuidlDat, cbuidlDat ) )
#endif BIGUIDL
            {
            DiskReadError( hwndFrame, lpTmpBuf, FALSE, TRUE );
            FreeMemory( &lpuidlDat );
            }
         else if( *(WORD *)lpuidlDat != UIDL_DAT_ID )
         {
            /* Error Reading the file, so reset the internal list !!SM!!
             */
            cbuidlDat = 2;
            if( fResizeMemory( &lpuidlDat, cbuidlDat ) )
               *(WORD *)lpuidlDat = UIDL_DAT_ID;
//          FreeMemory( &lpuidlDat );
         }
      Amfile_Close( fh );
      }
   else
      {
      /* File doesn't exist, so create it and set the
       * first two bytes to the version ID.
       */
      cbuidlDat = 2;
      if( fNewMemory( &lpuidlDat, cbuidlDat ) )
         *(WORD *)lpuidlDat = UIDL_DAT_ID;
      }
   return( lpuidlDat != NULL );
}

/* This function saves the memory image of the user id
 * lists back to the data file.
 */
BOOL FASTCALL SaveUidlDataFile( void )
{
   if( NULL != lpuidlDat )
      {
      HNDFILE fh;

      /* Create a temporary file first.
       */
      Amuser_GetActiveUserDirectory( lpTmpBuf, _MAX_PATH );
      strcat( strcat( lpTmpBuf, "\\" ), szUidlTempFile );
      fh = Amfile_Create( lpTmpBuf, 0 );
      if( HNDFILE_ERROR != fh )
         RemoveExtraUIDLs();
#ifndef BIGUIDL
         if( cbuidlDat == Amfile_Write( fh, lpuidlDat, cbuidlDat ) ) //!!SM!!
#else BIGUIDL
         if( cbuidlDat == Amfile_Write32( fh, lpuidlDat, cbuidlDat ) ) 
#endif BIGUIDL
            {
            char chNewName[ _MAX_PATH ];

            Amfile_Close( fh );
            Amuser_GetActiveUserDirectory( chNewName, _MAX_PATH );
            strcat( strcat( chNewName, "\\" ), szUidlDataFile );
            Amfile_Delete( chNewName );
            Amfile_Rename( lpTmpBuf, chNewName );
            }
         else if( !DiskWriteError( hwndFrame, lpTmpBuf, FALSE, TRUE ) )
            {
            Amfile_Close( fh );
            Amfile_Delete( lpTmpBuf );
            }
      }
   return( lpuidlDat != NULL );
}

/* This function deletes the UIDL.DAT data file, if
 * one exists.
 */
void FASTCALL DeleteUidlDataFile( void )
{
   char chNewName[ _MAX_PATH ];

   Amuser_GetActiveUserDirectory( chNewName, _MAX_PATH );
   strcat( strcat( chNewName, "\\" ), szUidlDataFile );
   Amfile_Delete( chNewName );
}

/* This function adds the specified user ID string to
 * the UIDL.DAT file with an exiting ID.
 */
void FASTCALL AddToUidlDataFileWithID( char * pszUidl, UIDLSTRUCT * puidls )
{

   if( NULL != lpuidlDat )
   {
      UINT cbNewSize;
      cbNewSize = cbuidlDat + ( strlen( pszUidl ) + 1 ) + sizeof(DWORD);
      if( fResizeMemory( &lpuidlDat, cbNewSize ) )
      {
         WORD * pw;

         /* Store the date/time in the first 32-bits
          */
         pw = (WORD *)&lpuidlDat[ cbuidlDat ];

         *pw++ = puidls->wDate;
         *pw++ = puidls->wTime;
         
         /* Store the string in the remainder.
          */
         strcpy( (LPSTR)pw, pszUidl );
         cbuidlDat = cbNewSize;
      }
   }
}


void FASTCALL RemoveExtraUIDLs( void )
{
   LPSTR localUIDLList;
   LPSTR lpuid;
   UIDLSTRUCT uidl;
   UINT i;
   UINT lbuidlDat;

   lbuidlDat = cbuidlDat;

   INITIALISE_PTR(localUIDLList);

   if( fNewMemory( &localUIDLList, lbuidlDat ) )
   {

      i = 0;
      while ( i < lbuidlDat )
      {
         localUIDLList[i] = lpuidlDat[i];
         i++;
      }

      if( NULL != lpuidlDat )
      {
         cbuidlDat = 2;
         fResizeMemory( &lpuidlDat, cbuidlDat );
         
      }
      /* Skip the version number.
       */
      lpuid = localUIDLList + sizeof(WORD);
      while( lpuid < localUIDLList + lbuidlDat )
      {
         uidl.wDate = *(WORD *)lpuid++;
         uidl.wTime = *(WORD *)lpuid++;
         lpuid += sizeof(WORD);
         ASSERT( lpuid < localUIDLList + lbuidlDat );
         i = 0;
         while ((int)i < cNewMsgs) 
         {
            if( strcmp( lpuidl[ i++ ], lpuid ) == 0 )
            {
               AddToUidlDataFileWithID( lpuid, &uidl );
               break;
            }
         }
         lpuid += strlen(lpuid) + 1;
         ASSERT( lpuid <= localUIDLList + lbuidlDat );
      }
      FreeMemory( &localUIDLList );
   }
}

/* This function adds the specified user ID string to
 * the UIDL.DAT file.
 */
void FASTCALL AddToUidlDataFile( char * pszUidl )
{
   if( NULL != lpuidlDat )
      {
      UINT cbNewSize;

      cbNewSize = cbuidlDat + ( strlen( pszUidl ) + 1 ) + sizeof(DWORD);
      if( fResizeMemory( &lpuidlDat, cbNewSize ) )
         {
         WORD * pw;
         AM_DATE date;
         AM_DATE adjdate;
         AM_TIME time;
         WORD wPackedHeaderDate;
         WORD wPackedAdjDate;

         /* Store the date/time in the first 32-bits
          */
         pw = (WORD *)&lpuidlDat[ cbuidlDat ];

         /* Right. If the date in the mail header is more
          * than one day into the future we store the 
          * *current* date and time instead. This avoids 
          * the problem where someone sends an email with 
          * an invalid date which is then never cleared.
          * We adjust forward by one day to to allow for 
          * time zone differences.
          */
         Amdate_GetCurrentDate( &date );
         Amdate_GetCurrentTime( &time );
         adjdate = date;
         Amdate_AdjustDate( &adjdate, 1 );

         wPackedAdjDate = Amdate_PackDate( &adjdate );
         wPackedHeaderDate = Amdate_PackDate( &mailhdr.date );
         if( wPackedHeaderDate > wPackedAdjDate )
         {
            *pw++ = Amdate_PackDate( &date );
            *pw++ = Amdate_PackTime( &time );
         }
         else
         {
            *pw++ = Amdate_PackDate( &mailhdr.date );
            *pw++ = Amdate_PackTime( &mailhdr.time );
         }

         /* Store the string in the remainder.
          */
         strcpy( (LPSTR)pw, pszUidl );
         cbuidlDat = cbNewSize;
         }
      }
}

/* This function locates the specified user ID in the
 * UIDL.DAT database array.
 */
BOOL FASTCALL LocateInUidlDataFile( char * pszUidl, UIDLSTRUCT * puidls )
{
   if( NULL != lpuidlDat )
      {
      LPSTR lpuid;

      /* Skip the version number.
       */
      lpuid = lpuidlDat + sizeof(WORD);
      while( lpuid < lpuidlDat + cbuidlDat )
         {
         /* Skip the date/time.
          */
         puidls->wDate = *(WORD *)lpuid;
         puidls->wTime = *(WORD *)lpuid;
         lpuid += sizeof(WORD);
         ASSERT( lpuid < lpuidlDat + cbuidlDat );
         lpuid += sizeof(WORD);
         ASSERT( lpuid < lpuidlDat + cbuidlDat );

         /* Match against the string and return TRUE
          * if found.
          */
         strcpy( puidls->szUidl, lpuid );
         if( strcmp( pszUidl, lpuid ) == 0 )
            return( TRUE );

         /* Skip to the next one.
          */
         lpuid += strlen(lpuid) + 1;
         ASSERT( lpuid <= lpuidlDat + cbuidlDat );
         }
      }
   return( FALSE );
}

/* This function deallocates the UIDL array.
 */
void FASTCALL FreeUidlArray( void )
{
   if( NULL != lpuidl )
      {
      register int i;

      for( i = 0; i < cUidl; ++i )
         free( lpuidl[ i ] );
      cUidl = 0;
      FreeMemory( (LPVOID)&lpuidl );
      }
}

/* This function attempts to allocate a BIG buffer of the
 * requested size to avoid calls to fResizeMemory.
 */
static BOOL FASTCALL PreallocateBuffer( DWORD dwSizeWanted )
{
   if( fNewMemory32( &hpMailBuf, dwSizeWanted ) )
      {
      dwBufSize = dwSizeWanted;
      return( TRUE );
      }
   return( FALSE );
}

/* This function appends the specified line to the memory buffer.
 */
void FASTCALL AppendToMailMessage( char * pstr, BOOL fNewline )
{
   /* Cast to long to avoid wrap round because MAX_MSGSIZE is close to 0xFFFF
    */
   if( hpMailBuf == NULL )
      {
      dwBufSize = 64000;
      fNewMemory32( &hpMailBuf, dwBufSize );
      }
   else if( dwMailMsgLength + (DWORD)strlen( pstr ) + 2L >= dwBufSize )
      {
      dwBufSize += 64000;
      fResizeMemory32( &hpMailBuf, dwBufSize );
      }
   if( hpMailBuf )
      {
      if( *pstr == '.' )
         ++pstr;
      hmemcpy( hpMailBuf + dwMailMsgLength, pstr, strlen( pstr ) );
      dwMailMsgLength += strlen( pstr );
      if( fNewline )
         {
         hpMailBuf[ dwMailMsgLength++ ] = CR;
         hpMailBuf[ dwMailMsgLength++ ] = LF;
         }
      hpMailBuf[ dwMailMsgLength ] = '\0';
      }
}

/* This function handles the POP3Login dialog box.
 */
BOOL EXPORT CALLBACK POP3Login( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = POP3Login_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the POP3Login
 *  dialog.
 */
LRESULT FASTCALL POP3Login_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, POP3Login_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, POP3Login_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPOP3LOGIN );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL POP3Login_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndEdit;

   /* Set the login name, but leave password field
    * blank.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_MAILLOGIN );
   Edit_LimitText( hwndEdit, sizeof(szMailLogin) - 1 );
   Edit_SetText( hwndEdit, szMailLogin );
   SetFocus( GetDlgItem( hwnd, IDD_MAILPASSWORD ) );

   /* Disable OK button.
    */
   EnableControl( hwnd, IDOK, FALSE );
   return( FALSE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL POP3Login_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_MAILPASSWORD:
      case IDD_MAILLOGIN:
         if( codeNotify == EN_CHANGE )
            EnableControl( hwnd, IDOK,
               Edit_GetTextLength( GetDlgItem( hwnd, IDD_MAILLOGIN ) ) > 0 &&
               Edit_GetTextLength( GetDlgItem( hwnd, IDD_MAILPASSWORD ) ) > 0 );
            break;

      case IDOK:
         /* Get the login name and password. Save the password
          * if the IDD_SAVEPASSWORD button is checked.
          */
         Edit_GetText( GetDlgItem( hwnd, IDD_MAILLOGIN ), szMailLogin, sizeof(szMailLogin) );
         Edit_GetText( GetDlgItem( hwnd, IDD_MAILPASSWORD ), szTmpMailPassword, sizeof(szTmpMailPassword) );
         Amuser_Encrypt( szTmpMailPassword, rgEncodeKey );
         if( IsDlgButtonChecked( hwnd, IDD_SAVEPASSWORD ) )
            {
            memcpy( szMailPassword, szTmpMailPassword, sizeof(szTmpMailPassword) );
            Amuser_WritePPPassword( szCIXIP, "MailPassword", szMailPassword );
            Amuser_WritePPInt( szCIXIP, "RememberPassword", fRememberPassword = TRUE );
            }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}
