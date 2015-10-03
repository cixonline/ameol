/* IPCOMMON.C - Common News and Mail IP stuff
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
#include "parse.h"
#include "intcomms.h"
#include <memory.h>
#include <ctype.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include "cixip.h"
#include "amlib.h"
#include "log.h"
#include "cookie.h"
#include "ameol2.h"

#define  THIS_FILE   __FILE__

/* The following are months as they appear in a mail or Usenet message header.
 * They are language independent and must not be translated.
 */
char * pszCixMonths[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

static char szMsgIdTmplt[] = "<memo.%02d%02d%02d%02d%02d%02d.%u%c@%s>";
static char szBoundaryTmplt[] = "==========_%02d%02d%02d%02d%02d%02d%u%c==_";

static BOOL fDisconnectTimerRunning = FALSE;    /* TRUE if disconnection timer is running */
static BOOL fCheckForXOLRField;                 /* TRUE if we have to check for X-Olr */
BOOL FASTCALL CheckForIPXOLRField( HEADER *, char * );
char * FASTCALL ParseTo( char *, HEADER * );
char * FASTCALL ParseCC( char *, HEADER * );
DWORD FASTCALL LookupReference( char *, HEADER * );

char FAR szNewsMsg[ 512 ];             /* Error message from server */
BOOL fNewsErrorReported;               /* TRUE if an error has been reported by Ameol */
static BOOL fParsedID = FALSE;

/* This function reports information to the user. If we're
 * online and not blinking, report via a message box. Else
 * report to the blink log.
 */
void FASTCALL ReportOnline( char * pInfoText )
{
   if( fBlinking )
      WriteToBlinkLog( pInfoText );
   else if( !fNewsErrorReported )
      {
      fNewsErrorReported = TRUE;
      strcpy( szNewsMsg, pInfoText );
      PostMessage( hwndFrame, WM_NEWSERROR, 0, (LPARAM)(LPSTR)szNewsMsg );
      }
}

/* This function suspends the disconnection timeout rate. It should
 * be called at the commencement of any IP activity which signals the
 * end of a no-activity period.
 */
void FASTCALL SuspendDisconnectTimeout( void )
{
   if( fDisconnectTimerRunning )
      {
      KillTimer( hwndFrame, TID_DISCONNECT );
      fDisconnectTimerRunning = FALSE;
      }
}

/* This function resets the disconnection timeout rate. It should be
 * called at the end of any IP activity and it signals the start of
 * a no-activity period.
 */
void FASTCALL ResetDisconnectTimeout( void )
{
   if( fDisconnectTimeout )
      {
      if( fDisconnectTimerRunning )
         KillTimer( hwndFrame, TID_DISCONNECT );
      SetTimer( hwndFrame, TID_DISCONNECT, wDisconnectRate, NULL );
      fDisconnectTimerRunning = TRUE;
      }
}

/* This function handles a disconnection timeout. It is called from
 * the WM_TIMER handler for the main window.
 */
void FASTCALL HandleDisconnectTimeout( void )
{
   ASSERT( TRUE == fDisconnectTimeout );
   if( fDisconnectTimerRunning && !fBlinking )
      {
      KillTimer( hwndFrame, TID_DISCONNECT );
      fDisconnectTimerRunning = FALSE;
      if( !IsServerBusy() )
         CmdOffline();
      else
         ResetDisconnectTimeout();
      }
}

/* Close the online connection when we go offline or we've finished
 * processing all out-basket items. If we're busy, we have to take
 * a brute-force approach to terminating the connection. Otherwise we
 * can do it the clean way.
 */
BOOL FASTCALL CloseOnlineWindows( HWND hwnd, BOOL fPrompt )
{
   /* If Ras dial in progress, cancel it.
    */
#ifdef WIN32
   if( nRasConnect == RCS_CONNECTING )
      {
      nRasConnect = RCS_ABORTED;
      return( FALSE );
      }
   if( fTapiDialing )
      {
      WriteToBlinkLog( GS(IDS_STR1101) );
      fTapiDialing = FALSE;
      fTapiAborted = TRUE;
      return( FALSE );
      }
#endif

   /* If we're blocking, stop blocking.
    */
   if( Pal_WSAIsBlocking() )
      {
      Pal_WSACancelBlockingCall();
      return( FALSE );
      }

   /* Broadcast WM_QUERYISONLINE to all child windows. If any
    * return TRUE, prompt to confirm that we can go off-line.
    */
   if( IsServerBusy() && fPrompt )
      {
      if( !fBlinking  || ( iAddonsUsingConnection > 0 ) )
         if( fMessageBox( hwnd, 0, GS(IDS_STR680), MB_YESNO|MB_ICONEXCLAMATION ) == IDNO )
            return( FALSE );
      }

   if( !fQuitting )
      iAddonsUsingConnection = 0;
   
   /* If a CIX synchronous blink is in progress, clear the flag so
    * that we'll unwind and eventually kill ourselves.
    */
   if( fCIXSyncInProgress )
      {
      if( lpcdevCIX )
         {
         /* By setting fEndBlinking to TRUE, next time Blink() is
          * called the blink is properly terminated.
          * YH 10/05/96
          */
         wCommErr = WCE_ABORT;
         fEndBlinking = TRUE;
         }
      return( FALSE );
      }

   /* Now broadcast WM_DISCONNECT to all child windows.
    */
   DisconnectAllNewsServers();
   if( lpcdevMail )
      SendMessage( lpcdevMail->hwnd, WM_DISCONNECT, 0, 0L );
   if( lpcdevSMTP )
      SendMessage( lpcdevSMTP->hwnd, WM_DISCONNECT, 0, 0L );
   return( TRUE );
}

/* This function closes all the open terminal windows.
 */
BOOL FASTCALL CloseAllTerminals( HWND hwnd, BOOL fPrompt )
{
   HWND hwndCheck;

   /* Broadcast WM_QUERYISONLINE to all child windows. If any
    * return TRUE, prompt to confirm that we can go off-line.
    */
   if( AreTerminalsActive() && fPrompt )
      {
      if( fMessageBox( hwnd, 0, GS(IDS_STR680), MB_YESNO|MB_ICONEXCLAMATION ) == IDNO )
         return( FALSE );
      }

   /* Loop and close all terminal windows.
    */
   hwndCheck = GetWindow( hwndMDIClient, GW_CHILD );
   while( hwndCheck )
      {
      HWND hwndNext;

      /* Get the handle of the next window in case hwndCheck goes invalid
       * during the SendMessage (because the window is destroyed)
       */
      hwndNext = GetWindow( hwndCheck, GW_HWNDNEXT );
      if( !GetWindow( hwndCheck, GW_OWNER ) )
         SendMessage( hwndCheck, WM_DISCONNECT, 0, 0L );
      hwndCheck = hwndNext;
      }
   return( TRUE );
}

/* This function returns TRUE if Palantir is currently busy. Because of the
 * way that some windows are children of hwndFrame and others are children
 * of hwndMDIClient, this code is a bit of a bodge. A cleaner implementation
 * would be desirable at some point!
 */
BOOL FASTCALL IsServerBusy( void )
{
   BOOL fServerBusy;

   fServerBusy = FALSE;
#ifdef WIN32
   if( nRasConnect == RCS_CONNECTING )
      fServerBusy = TRUE;
#endif
   if( !fServerBusy && CountOpenNewsServers() )
      fServerBusy = IsNewsServerBusy();
   if( !fServerBusy && lpcdevMail )
      fServerBusy = nMailState != NWS_IDLE;
   if( !fServerBusy && lpcdevSMTP )
      fServerBusy = nSMTPState != NWS_IDLE;
   if( !fServerBusy && ( iAddonsUsingConnection > 0 ) )
      fServerBusy = TRUE;
   return( fServerBusy );
}

/* This function returns whether or not any terminal
 * windows are active.
 */
BOOL FASTCALL AreTerminalsActive( void )
{
   HWND hwndCheck;
   BOOL fServerBusy;

   fServerBusy = FALSE;
   hwndCheck = GetWindow( hwndMDIClient, GW_CHILD );
   while( hwndCheck )
      {
      HWND hwndNext;

      /* Get the handle of the next window in case hwndCheck goes invalid
       * during the SendMessage (because the window is destroyed)
       */
      hwndNext = GetWindow( hwndCheck, GW_HWNDNEXT );
      if( !GetWindow( hwndCheck, GW_OWNER ) )
         if( fServerBusy = (BOOL)SendMessage( hwndCheck, WM_QUERYISONLINE, 0, 0L ) )
            break;
      hwndCheck = hwndNext;
      }
   return( fServerBusy );
}

/* Data has arrived from the socket. Read as much data as we can up to the end of
 * line terminator before attempting to parse the data. The first number on the line
 * will be the code indicating what the rest of the line means, so we can read the
 * numbers and ignore the rest of the line.
 */
void FASTCALL ReadAndProcessSocket( LPCOMMDEVICE lpcdev, LPBUFFER lpBuffer, FPPARSERPROC fpParserProc )
{

   // KillTimer( hwndFrame, TID_OLT );
   
   /* Loop until entire line is processed.
   */
   while( lpBuffer->cbInComBuf )
   {
      char * pLine;
      int iNewline;
      
      /* Have we got a line terminator yet? Note that we may get CR/LF which
      * is a '\r' before the actual '\n'.
      */
      
      if( ( pLine = memchr( lpBuffer->achComBuf, '\n', lpBuffer->cbInComBuf ) ) == NULL && lpBuffer->cbInComBuf < 4096 ) // FS#157 2.56.2053
         break;

      /*
        FS#157 2.56.2053
        Bit of a dirty hack here, ideally we'd 'insert' the \n, so as to preserve the actual message, 
        but it's a fixed buffer, so if already full, we'd loose the last character either way.
        At least it stops the hanging..
      */
      if( ( pLine = memchr( lpBuffer->achComBuf, '\n', lpBuffer->cbInComBuf ) ) == NULL ) 
      {
         lpBuffer->achComBuf[lpBuffer->cbInComBuf-1] = '\n';
         pLine = lpBuffer->achComBuf;
      }

      /* We've got a line terminator, so parse the line.
      */
      iNewline = pLine - lpBuffer->achComBuf;
      ASSERT( iNewline < lpBuffer->cbInComBuf );
      if( iNewline > 0 && lpBuffer->achComBuf[ iNewline - 1 ] == '\r' )
         --iNewline;
      lpBuffer->achComBuf[ iNewline ] = '\0';
      if( !fpParserProc( lpcdev, lpBuffer->achComBuf ) )
      {
         lpBuffer->cbInComBuf = 0;
         lpBuffer->cbComBuf = 0;
         return;
      }
      
      /* Now shift down the stuff to the right of the line
      * terminator.
      */
      ++pLine;
      lpBuffer->cbInComBuf -= pLine - lpBuffer->achComBuf;
      if( lpBuffer->cbInComBuf )
         memcpy( lpBuffer->achComBuf, pLine, lpBuffer->cbInComBuf );
   }
   lpBuffer->cbComBuf = 0;
   // SetTimer( hwndFrame, TID_OLT, (UINT)60000, NULL );

}

void FASTCALL ReadAndProcessSocket2( LPCOMMDEVICE lpcdev, LPBUFFER lpBuffer, FPPARSERPROC fpParserProc )
{
   LPSTR lpBuf;

   int i=0,j=0;
   
   INITIALISE_PTR(lpBuf);
   fNewMemory( &lpBuf, 5000 );

   while (j < lpBuffer->cbInComBuf - 1 )
   {
      lpBuf[i] = lpBuffer->achComBuf[j];
      if (lpBuf[i] == '\r' || lpBuf[i] == '\n')
      {
         if (lpBuffer->achComBuf[j+1] == '\r')
         {
            i++;
            j++;
            lpBuf[i] = '\n';
         }
         if (lpBuffer->achComBuf[j+1] == '\n')
         {
            i++;
            j++;
            lpBuf[i] = '\n';
         }
         lpBuf[i+1] = '\x0';
         lpBuffer->achComBuf[j] = '\x0';
         if( !fpParserProc( lpcdev, lpBuf ) )
         {
            lpBuffer->cbInComBuf = 0;
            lpBuffer->cbComBuf = 0;
            return;
         }
         i = 0;
         j++;
      }
      else if ( i > 4094 )
      {
         lpBuf[i+1] = '\r';
         lpBuf[i+2] = '\n';
         lpBuf[i+3] = '\0';
         if( !fpParserProc( lpcdev, lpBuf ) )
         {
            lpBuffer->cbInComBuf = 0;
            lpBuffer->cbComBuf = 0;
            return;
         }
         i = 0;
         j++;
      }
      else
      {
         i++;
         j++;
      }

   }
   lpBuffer->cbComBuf = lpBuffer->cbComBuf + j;
   FreeMemory( &lpBuf );

}

void FASTCALL ReadAndProcessSocketNew( LPCOMMDEVICE lpcdev, LPBUFFER lpBuffer, FPPARSERPROC fpParserProc )
{

// KillTimer( hwndFrame, TID_OLT );

   if( fDebugMode )
   {
      ReadAndProcessSocket( lpcdev, lpBuffer, fpParserProc );
   }
   else
   {

   /* Loop until entire line is processed.
    */
   while( lpBuffer->cbInComBuf && IsWindow(lpcdev->hwnd) /*!!SM!!*/ )
      {
      char * pLine;
      int iNewline;

      /* Have we got a line terminator yet? Note that we may get CR/LF which
       * is a '\r' before the actual '\n'.
       */
      if( ( pLine = memchr( lpBuffer->achComBuf, '\n', lpBuffer->cbInComBuf ) ) == NULL )
         break;

      /* We've got a line terminator, so parse the line.
       */
      iNewline = pLine - lpBuffer->achComBuf;
      ASSERT( iNewline < lpBuffer->cbInComBuf );
      if( iNewline > 0 && lpBuffer->achComBuf[ iNewline - 1 ] == '\r' )
         --iNewline;
      lpBuffer->achComBuf[ iNewline ] = '\0';
      if( !fpParserProc( lpcdev, lpBuffer->achComBuf ) )
      {
         if ( ( IsWindow(hwndBlink) || !fShowBlinkWindow ) && fBlinking )
         {
            /*!!SM!! - Possible news problem commenting these out???*/
            lpBuffer->cbInComBuf = 0;
            lpBuffer->cbComBuf = 0;
         }
         return;
      }

      /* Now shift down the stuff to the right of the line
       * terminator.
       */
      ++pLine;
      lpBuffer->cbInComBuf -= pLine - lpBuffer->achComBuf;
      if( lpBuffer->cbInComBuf )
         memcpy( lpBuffer->achComBuf, pLine, lpBuffer->cbInComBuf );
      }
   lpBuffer->cbComBuf = 0;
// SetTimer( hwndFrame, TID_OLT, (UINT)60000, NULL );
   }
}

/* This function both writes a NULL terminated string to the socket and
 * to the log file if it is enabled.
 */
BOOL FASTCALL WriteSocketLine( LPCOMMDEVICE lpcdev, LPSTR lpstr )
{
   int cBytesToWrite;

   cBytesToWrite = lstrlen( lpstr );
   if( NULL != lpcdev->lpLogFile )
      Amfile_Write( lpcdev->lpLogFile->fhLog, lpstr, cBytesToWrite );
   return( Amcomm_WriteData( lpcdev, lpstr, cBytesToWrite ) );
}

/* This function parses a line of text from the socket and fills global variables with
 * details from the header.
 */
BOOL FASTCALL ParseSocketLine( char * pLine, HEADER * pHeader )
{
   /* If not in header, check for In-Reply-To on
    * the first line.
    */
   if( !pHeader->fInHeader )
      {
      if( pHeader->fCheckForXOLRField )
         if( CheckForIPXOLRField( pHeader, pLine ) )
            return( TRUE );
      if( pHeader->fCheckForInReplyTo )
         if( CheckForInReplyTo( pHeader, pLine ) )
            return( TRUE );
      return( FALSE );
      }


   /* If blank line, end of header mode.
    */
   if( *pLine == '\0' )
      {
      pHeader->fInHeader = FALSE;
      return( FALSE );
      }

   /* Read the header type.
    */
   if( *pLine != ' ' && *pLine != '\t' )
      {
      register int c;

      for( c = 0; c < LEN_RFCTYPE && *pLine && *pLine != ':'; ++c )
         pHeader->szType[ c ] = *pLine++;
      pHeader->szType[ c ] = '\0';
      if( *pLine == ':' )
         ++pLine;
      }
   while( *pLine == ' ' )
      ++pLine;

   /* Inspect the type to determine how we parse it.
    */
   if( _strcmpi( pHeader->szType, "Date" ) == 0 )
      ParseMailDate( pLine, pHeader );
   else if( _strcmpi( pHeader->szType, "From" ) == 0 )
      ParseMailAuthor( pLine, pHeader );
   else if( _strcmpi( pHeader->szType, "To" ) == 0 )
      ParseTo( pLine, pHeader );
   else if( _strcmpi( pHeader->szType, "Apparently-To" ) == 0 && *pHeader->szTo == '\0' )
      ParseTo( pLine, pHeader );
   else if( _strcmpi( pHeader->szType, "CC" ) == 0 )
      ParseCC( pLine, pHeader );
   else if( _strcmpi( pHeader->szType, "References" ) == 0 && *pHeader->szReference == '\0' )
      ParseReference( pLine, pHeader );
   else if( _strcmpi( pHeader->szType, "Mime-Version" ) == 0 )
      pHeader->fMime = TRUE;
   else if( _strcmpi( pHeader->szType, "Disposition-Notification-To" ) == 0 )
      ParseDispositionNotification( pLine, pHeader );
   else if( _strcmpi( pHeader->szType, "X-Confirm-Reading-To" ) == 0 )
      ParseDispositionNotification( pLine, pHeader );
   else if( _strcmpi( pHeader->szType, "Content-Transfer-Encoding" ) == 0 )
      ParseContentTransferEncoding( pLine, pHeader );
   else if( _strcmpi( pHeader->szType, "Lines" ) == 0 )
      pHeader->cLines = atoi( pLine );
   else if( _strcmpi( pHeader->szType, "Message-Id" ) == 0 )
      ParseMessageId( pLine, pHeader, TRUE );
   else if( _strcmpi( pHeader->szType, "In-Reply-To" ) == 0 )
      ParseReplyNumber( pLine, pHeader );
   else if( _strcmpi( pHeader->szType, "X-Priority" ) == 0 )
      ParseXPriority( pLine, pHeader );
   else if( _strcmpi( pHeader->szType, "Xref" ) == 0 )
      ParseCrossReference( pLine, pHeader );
   else if( _strcmpi( pHeader->szType, "Subject" ) == 0 && *pHeader->szTitle == '\0' )
      ParseMailSubject( pLine, pHeader );
   else if( _strcmpi( pHeader->szType, "X-OLR" ) == 0 )
      ParseXOlr( pLine, pHeader );
   return( FALSE );
}

/* This function initialises the fields of a HEADER
 * structure.
 */
void FASTCALL InitialiseHeader( HEADER * pHeader )
{
   pHeader->szAuthor[ 0 ] = '\0';
   pHeader->szFrom[ 0 ] = '\0';
   pHeader->szTitle[ 0 ] = '\0';
   pHeader->szTo[ 0 ] = '\0';
   pHeader->szCC[ 0 ] = '\0';
   pHeader->szType[ 0 ] = '\0';
   pHeader->szMessageId[ 0 ] = '\0';
   pHeader->szReference[ 0 ] = '\0';
   pHeader->dwComment = 0L;
   pHeader->dwMsg = 0L;
   pHeader->fPossibleRefScanning = FALSE;
   pHeader->fCheckForInReplyTo = FALSE;
   pHeader->fCheckForXOLRField = TRUE;
   pHeader->fPriority = FALSE;
   pHeader->fInHeader = FALSE;
   pHeader->fOutboxMsg = FALSE;
   pHeader->fMime = FALSE;
   pHeader->fXref = FALSE;
   pHeader->fNotification = FALSE;
   pHeader->fCoverNote = FALSE;
   pHeader->cLines = 0;
   pHeader->nEncodingType = 0;
   Amdate_GetCurrentDate( &pHeader->date );
   Amdate_GetCurrentTime( &pHeader->time );
}

/* This function parses a disposition notification. We set a
 * flag on the message inviting the user to send a notification
 * back to the sender.
 */
char * FASTCALL ParseDispositionNotification( char * psz, HEADER * pHeader )
{
   pHeader->fNotification = TRUE;
   return( psz );
}

/* This function parses a cross-reference. All we do is set a flag
 * saying that this article is cross-referenced.
 */
char * FASTCALL ParseCrossReference( char * psz, HEADER * pHeader )
{
   pHeader->fXref = TRUE;
   return( psz );
}

/* This function parses an RFC specification date.
 */
char * FASTCALL ParseMailDate( char * psz, HEADER * pHeader )
{
   char szMonth[ 4 ];
   register int c;

   /* Initialise the structures.
    */
   pHeader->time.iHour = 0;
   pHeader->time.iMinute = 0;
   pHeader->time.iSecond = 0;
   pHeader->date.iDay = 0;
   pHeader->date.iMonth = 0;
   pHeader->date.iYear = 0;

   /* Skip the day name
    */
   if( !isdigit( *psz ) )
      {
      while( *psz && !isdigit( *psz ) )
         ++psz;
      if( *psz == '\0' )
         return( psz );
      }
   while( isdigit( *psz ) )
      pHeader->date.iDay = pHeader->date.iDay * 10 + ( *psz++ - '0' );
   while( *psz && !isalpha( *psz ) )
      ++psz;
   for( c = 0; *psz && isalpha( *psz ); )
      {
      if( c < 3 )
         szMonth[ c++ ] = *psz;
      ++psz;
      }
   szMonth[ c ] = '\0';
   if( *psz == '\0' )
      return( psz );
   for( c = 0; c < 12; ++c )
      if( strcmp( szMonth, pszCixMonths[ c ] ) == 0 ) {
         pHeader->date.iMonth = c + 1;
         break;
         }
   while( *psz && !isdigit( *psz ) )
      ++psz;
   while( isdigit( *psz ) )
      pHeader->date.iYear = pHeader->date.iYear * 10 + ( *psz++ - '0' );
   pHeader->date.iYear = FixupYear( pHeader->date.iYear );
   while( *psz == ' ' )
      ++psz;
   while( isdigit( *psz ) )
      pHeader->time.iHour = pHeader->time.iHour * 10 + ( *psz++ - '0' );
   if( *psz == ':' ) ++psz;
   while( isdigit( *psz ) )
      pHeader->time.iMinute = pHeader->time.iMinute * 10 + ( *psz++ - '0' );
   if( *psz == ':' ) 
   {
      ++psz;
      while( isdigit( *psz ) )
         pHeader->time.iSecond = pHeader->time.iSecond * 10 + ( *psz++ - '0' );
   }
   return( psz );
}

/* This function parses the X-Olr field which specifies that the
 * CIX mail message has an attachment.
 */
char * FASTCALL ParseXOlr( char * psz, HEADER * pHeader )
{
   if( _strnicmp( psz, "Binmail", 7 ) == 0 && _stricmp( pHeader->szAuthor, szCIXNickname ) && !pHeader->fOutboxMsg )
      {
      psz += 7;
      while( *psz == ' ' )
         ++psz;
      pHeader->fCoverNote = TRUE;
      pHeader->fCheckForXOLRField = FALSE;
      }
   return( psz );
}

/* This function parses a CONTENT-TRANSFER-ENCODING statement
 */
char * FASTCALL ParseContentTransferEncoding( char * psz, HEADER * pHeader )
{
   char szEncodingType[ 40 ];
   register int c;

   for( c = 0; *psz && c < 39; ++c )
      szEncodingType[ c ] = *psz++;
   szEncodingType[ c ] = 0;
   if( _strcmpi( szEncodingType, "QUOTED-PRINTABLE" ) == 0 )
      pHeader->nEncodingType = MIME_ET_QUOTEDPRINTABLE;
   else if( _strcmpi( szEncodingType, "7BIT" ) == 0 )
      pHeader->nEncodingType = MIME_ET_7BIT;
   else
      pHeader->nEncodingType = 0;
   return( psz );
}

/* This function preprocesses a line using the current encoding type
 * before it is added to the database.
 */
BOOL FASTCALL ApplyEncodingType( char * lpLinebuf, HEADER * pHeader )
{
   // 20060325 - 2.56.2049.21 
   BOOL fNewline;

   fNewline = TRUE;
   if( pHeader->nEncodingType == MIME_ET_QUOTEDPRINTABLE )
   {
      //register int s, d;

      // Don't 

      /* Quoted printable, so decode either the hex or the
       * newline at the end.
       */
      /*
      for( s = d = 0; lpLinebuf[ s ]; )
      {
         if( lpLinebuf[ s ] == '=' )
         {
            int nValue = 0;

            ++s;
            if( isxdigit( lpLinebuf[ s ] ) )
            {
               if( isalpha( lpLinebuf[ s ] ) )
                  nValue = 16 * ( ( tolower( lpLinebuf[ s ] ) - 'a' ) + 10 );
               else
                  nValue = 16 * ( lpLinebuf[ s ] - '0' );
               ++s;
               if( isxdigit( lpLinebuf[ s ] ) )
                  {
                  if( isalpha( lpLinebuf[ s ] ) )
                     nValue += ( ( tolower( lpLinebuf[ s ] ) - 'a' ) + 10 );
                  else
                     nValue += lpLinebuf[ s ] - '0';
                  ++s;
                  }
            }
            else if( lpLinebuf[ s ] )
            {
               nValue = lpLinebuf[ s++ ]; 
               //!!SM!! if it isn't a QP value, then put the equals back instead of stripping it???
               // nValue = '=';
            }
            else
            {
               fNewline = FALSE;
            }
            lpLinebuf[ d++ ] = (BYTE)nValue;
         }
         else
            lpLinebuf[ d++ ] = lpLinebuf[ s++ ];
      }
      lpLinebuf[ d ] = '\0';
      */
   }
   else if( pHeader->nEncodingType == MIME_ET_7BIT )
   {
//    register int s;

      /* 7-bit data format, so strip the top bit of every
       * character.
       */
   /* for( s = 0; lpLinebuf[ s ]; ++s )
         lpLinebuf[ s ] &= 0x7F;
    */
   }
   return( fNewline );
}

/* This function parses a To or Apparently-To field into the szTo
 * structure in the header.
 */
char * FASTCALL ParseTo( char * psz, HEADER * pHeader )
{
   char * pszTo;
   char szTmpName[ 64 ];
   int cbTo;

   /* Extract the entire To field to the szTo field in the
    * structure. We need it for the rules later.
    */
   cbTo = strlen( pHeader->szTo );
   pszTo = pHeader->szTo + cbTo;
   if( cbTo )
      *pszTo++ = ' ';
   psz = ParseToField( psz, pszTo, LEN_TOCCLIST - cbTo );

   /* Extract each address to szAddress and strip off the
    * compulink domain.
    */
   while( *pszTo )
      {
      char szAddress[ ( 10 * LEN_MAILADDR ) + 1 ];
      char * pszAddress;
      char szAuthorTmp[ 40 ];
      register int c;

      /* Extract one address
       */
      for( c = 0; pszTo[ c ] && pszTo[ c ] != ' '; ++c )
         if( c < ( 10 * LEN_MAILADDR ) )
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
         if( IsCompulinkDomain( pszAddress + 1 ) )
            *pszAddress = '\0';

      /* szAuthorTmp is used to hold the old-style CIX username for 
       * comparison
       */
      strcpy ( szAuthorTmp, szCIXNickname );
      strcat ( szAuthorTmp, "@cix.compulink.co.uk" );

      /* Is this the sender? If so, set the sender to be the author
       * of this message.
       */
      if( NULL != pHeader->ptl )
         Amdb_GetCookie( pHeader->ptl, MAIL_ADDRESS_COOKIE, szTmpName, szMailAddress, 64 );
      else
         strcpy( szTmpName, szMailAddress );
      if( _stricmp( pHeader->szAuthor, szMailAddress ) == 0 || _stricmp( pHeader->szAuthor, szAuthorTmp ) == 0 || _stricmp( pHeader->szAuthor, szTmpName ) == 0 )
         {
         pHeader->fPriority = TRUE;
         strcpy( pHeader->szAuthor, szAddress );
         pHeader->fOutboxMsg = TRUE;
         }
      }
   return( psz );
}

/* This function parses a CC field into the szCC structure in the
 * header.
 */
char * FASTCALL ParseCC( char * psz, HEADER * pHeader )
{
   int cbCC;

   cbCC = strlen(pHeader->szCC);
   if( cbCC )
      strcpy( pHeader->szCC + cbCC++, " " );
   psz = ParseToField( psz, pHeader->szCC + cbCC, LEN_TOCCLIST - cbCC );
   return( psz );
}

char * FASTCALL ParseMailAuthor( char * psz, HEADER * pHeader )
{
   char szFullName[ LEN_MAILADDR + 1 ];
   char szAddress[ LEN_MAILADDR + 1 ];

   *szFullName = '\0';
   *szAddress = '\0';
   psz = ParseFromField( psz, szAddress, szFullName );
   StripLeadingTrailingQuotes( szFullName );
   strncpy( pHeader->szFullname, *szFullName ? szFullName : szAddress, ( sizeof( pHeader->szFullname ) - 1 ));
   strncpy( pHeader->szAuthor, *szAddress ? szAddress : szFullName, ( sizeof( pHeader->szAuthor ) - 1 ) );
   strncpy( pHeader->szFrom, pHeader->szAuthor, ( sizeof( pHeader->szFrom ) - 1 ) );
   return( psz );
}

char * FASTCALL ParseXPriority( char * psz, HEADER * pHeader )
{
   switch( atoi( psz ) )
      {
      case 1:  /* Highest */
      case 2:  /* High */
         pHeader->fPriority = TRUE;
         break;
      }
   return( psz );
}

char * FASTCALL ParsePriority( char * psz, HEADER * pHeader )
{
   if( _strcmpi( psz, "urgent" ) == 0 )      
      pHeader->fPriority = TRUE;
   return( psz );
}

/* This function reads the elements of a To: or CC: field and extracts
 * the actual e-mail addresses to pszDest, each separated by a single
 * space.
 */
char * FASTCALL ParseToField( char * pszSrc, char * pszDest, int cbMax )
{
   int cbDest;

   if (strstr(pszSrc, "=?"))  //2.56.2051 FS#139
   {
      b64decodePartial(pszSrc);
   }
   cbDest = 0;
   while( *pszSrc )
      {
      register int c;
      register int d;
      char szName[ ( 10 * LEN_MAILADDR ) + 1 ];
      char * pszAddress;
      char chEnd = '\0';
      char chStart = '\0';
      BOOL fComment;
      int cBalance;

      while( *pszSrc == ' ' )
         ++pszSrc;
      fComment = FALSE;
      pszAddress = NULL;
      if( *pszSrc == '\"' ) {
         ++pszSrc;
         chStart = '\0';
         chEnd = '\"';
         if( NULL != strrchr( pszSrc, '<' ) )
            fComment = TRUE;
         }
      else if( *pszSrc == '<' ) {
         ++pszSrc;
         pszAddress = pszSrc;
         chStart = '<';
         chEnd = '>';
         }
      else if( *pszSrc == '(' ) {
         ++pszSrc;
         chStart = '(';
         chEnd = ')';
         fComment = TRUE;
         }
      cBalance = 0;
      for( d = c = 0; *pszSrc; ++c )
         {
         if( *pszSrc == chStart )
            --cBalance;
         else if( *pszSrc == chEnd )
            {
            if( cBalance )
               --cBalance;
            else
               break;
            }
         else if( *pszSrc == ',' && '\0' == chEnd )
            {
            ++pszSrc;
            break;
            }
         else if( chEnd == '\0' && ( *pszSrc == '\"' || *pszSrc == '(' || *pszSrc == '<' ) )
            {
            --c; /* Remove space at end of full name */
            break;
            }
         if( d < ( 10 * LEN_MAILADDR ) )
            szName[ d++ ] = *pszSrc;
         if( *pszSrc == ' ' && chEnd != '\0' )
            fComment = TRUE;
         if( *pszSrc == '@' && chEnd != ')' )
            pszAddress = &szName[ c ];
         ++pszSrc;
         }
      if( *pszSrc && *pszSrc == chEnd )
         ++pszSrc;
      while( d > 0 && ( szName[ d - 1 ] == ' ' || szName[ d - 1 ] == ',' ) )
         --d;
      szName[ d ] = '\0';
      if( pszAddress && !fComment )
         {
         int length;

         length = strlen( szName );
         if( cbDest + length < cbMax - 1 )
            {
            if( cbDest > 0 )
               pszDest[ cbDest++ ] = ' ';
            memcpy( pszDest + cbDest, szName, length + 1 );
            cbDest += length;
            }
         }
      }
   pszDest[ cbDest ] = '\0';
   return( pszSrc );
}

char * FASTCALL ParseFromField( char * psz, char * pszAuthor, char * pszFullName )
{
   if (strstr(psz, "=?")) //2.56.2051 FS#139
   {
      b64decodePartial(psz);
   }
   while( *psz )
      {
      register int c;
      register int d;
      char szName[ LEN_MAILADDR + 1 ];
      char * pszAddress;
      char chEnd = '\0';
      char chStart = '\0';
      int cBalance;

      while( *psz == ' ' )
         ++psz;
      pszAddress = NULL;
      if( *psz == '\"' ) {
         ++psz;
         chStart = '\0';
         chEnd = '\"';
         }
      else if( *psz == '<' ) {
         ++psz;
         pszAddress = psz;
         chStart = '<';
         chEnd = '>';
         }
      else if( *psz == '(' ) {
         ++psz;
         chStart = '(';
         chEnd = ')';
         }
      cBalance = 0;
      for( d = c = 0; *psz; ++c )
         {
         if( *psz == chStart )
            --cBalance;
         else if( *psz == chEnd )
            {
            if( ( cBalance % 2 ) != 0 )
               --cBalance;
            else
               break;
            }
         else if( chEnd == '\0' && ( *psz == '\"' || *psz == '(' || *psz == '<' ) )
            {
            --c; /* Remove space at end of full name */
            break;
            }
         if( d < LEN_MAILADDR )
            szName[ d++ ] = *psz;
         if( *psz == '@' && chEnd != ')' )
            pszAddress = &szName[ c ];
         ++psz;
         }
      if( *psz && *psz == chEnd )
         ++psz;
      while( d > 0 && ( szName[ d - 1 ] == ' ' || szName[ d - 1 ] == ',' ) )
         --d;
      szName[ d ] = '\0';
      if( pszAddress )
         {
         if( pszAuthor )
            strcpy( pszAuthor, szName );
         }
      else if( pszFullName )
         strcpy( pszFullName, szName );
      }
   return( psz );
}

char * FASTCALL ParseReference( char * psz, HEADER * pHeader )
{
   pHeader->dwComment = 0;
   while( *psz )
      {
      char szReference[ LEN_MESSAGEID ];
      register int c;
      DWORD dwPossibleComment;

      if( NULL != strrchr( psz, '<' ) )
         psz = strrchr( psz, '<' );
      else if( NULL != strrchr( psz, ' ' ) )
         psz = strrchr( psz, ' ' );

      if( *psz == '<' )
         ++psz;
      for( c = 0; *psz && *psz != '>'; )
         {
         if( c < ( LEN_MESSAGEID - 1 ) )
            szReference[ c++ ] = *psz;
         ++psz;
         }
      if( *psz == '>' )
         ++psz;
      szReference[ c ] = '\0';
      if( !fParsedID && !*pHeader->szReference )
         strcpy( pHeader->szReference, szReference );
      if( dwPossibleComment = Amdb_MessageIdToComment( pHeader->ptl, szReference ) )
         {
         pHeader->dwComment = dwPossibleComment;
         pHeader->fPossibleRefScanning = FALSE;
         }
      while( *psz == ' ' || *psz == ',' )
         ++psz;
      }
   return( psz );
}

char * FASTCALL ParseMessageId( char * psz, HEADER * pHeader, BOOL fMemoOverride )
{
   register int c;
   char chEnd = '\0';

   while( *psz == ' ' )
      ++psz;
   for( c = 0; c < LEN_MESSAGEID - 1 && *psz && *psz != chEnd; ++c )
      if( *psz == '<' ) {
         c = -1;
         chEnd = '>';
         ++psz;
         }
      else
         pHeader->szMessageId[ c ] = *psz++;
   pHeader->szMessageId[ c ] = '\0';
   if( _strnicmp( pHeader->szMessageId, "memo.", 5 ) == 0 && fMemoOverride )
      {
      char * psz2;

      psz2 = pHeader->szMessageId + 5;
      pHeader->dwMsg = strtoul( psz2, &psz2, 10 );
      }     
   return( psz );
}

char * FASTCALL ParseMailSubject( char * psz, HEADER * pHeader )
{
   register int c;

   if (strstr(psz, "=?")) //2.56.2051 FS#139
   {
      b64decodePartial(psz);
   }

   while( *psz == ' ' )
      ++psz;
   for( c = 0; c < LEN_TITLE - 1 && *psz; ++c )
      pHeader->szTitle[ c ] = *psz++;
   pHeader->szTitle[ c ] = '\0';
   if ( c == 0 )
      strcpy( pHeader->szTitle, "== No Subject ==" );
   if( !pHeader->fPossibleRefScanning && pHeader->dwComment == 0 )
      {
      if( _strnicmp( pHeader->szTitle, "Re:", 3 ) == 0 )
         pHeader->fPossibleRefScanning = TRUE;
      else if( _strnicmp( pHeader->szTitle, "Re[", 3 ) == 0 && isdigit( pHeader->szTitle[3] ) )
         pHeader->fPossibleRefScanning = TRUE;
      }
   return( psz );
}

char * FASTCALL ParseReplyNumber( char * psz, HEADER * pHeader )
{
   register int c;
   BOOL fInQuote;

   fInQuote = FALSE;
   while( *psz )
      {
      if( !fInQuote && *psz == '<' )
         break;
      if( *psz == '\"' )
         fInQuote = !fInQuote;
      ++psz;
      }
   if( *psz == '<' )
      {
      ++psz;
      for( c = 0; *psz && *psz != '>'; )
         {
         if( c < 79 )
            pHeader->szReference[ c++ ] = *psz;
         ++psz;
         }
      if( *psz == '>' )
         ++psz;
      pHeader->szReference[ c ] = '\0';

      /* The fPossibleRefScanning flag is set to false regardless of whether
       * a message was found to which this message is a comment to. This is
       * to avoid misthreading when a previous message has the same subject
       * as this one, but is not the message to which is this replies.
       * YH 19/04/96
       */
      pHeader->fPossibleRefScanning = FALSE;
      pHeader->fCheckForInReplyTo = FALSE;
      }
   fParsedID = TRUE;
   return( psz );
}

/* This function generates an unique message ID using the current
 * date and time, the current process task handle and a tag which
 * permits up to 52 unique message Ids per second.
 */
void FASTCALL CreateMessageId( char * pszMsgIdStr )
{
   char szAddress[ 130 ];
   static char chTag = 'A';
   AM_DATE aDate;
   AM_TIME aTime;
   WORD hTask;

   Amdate_GetCurrentTime( &aTime );
   Amdate_GetCurrentDate( &aDate );
   hTask = GetTaskHandle();
   if( *szHostname )
      wsprintf( szAddress, "%s.%s", szHostname, szDomain );
   else
      strcpy( szAddress, szDomain );
   wsprintf( pszMsgIdStr, szMsgIdTmplt, aDate.iYear, aDate.iMonth, aDate.iDay, aTime.iHour,
                       aTime.iMinute, aTime.iSecond, hTask, chTag, szAddress );
   chTag = (chTag == 'Z') ? 'a' : (chTag == 'z') ? 'A' : chTag + 1;
}

/* This function creates an unique MIME boundary divider using the current
 * date and time, the task handle and a separator character.
 */
LPSTR FASTCALL CreateMimeBoundary( void )
{
   LPSTR lpStr;

   INITIALISE_PTR(lpStr);
   if( fNewMemory( &lpStr, 60 ) )
      {
      static char chTag = 'A';
      AM_DATE aDate;
      AM_TIME aTime;
      WORD hTask;

      Amdate_GetCurrentTime( &aTime );
      Amdate_GetCurrentDate( &aDate );
      hTask = GetTaskHandle();
      wsprintf( lpStr, szBoundaryTmplt, aDate.iYear, aDate.iMonth, aDate.iDay, aTime.iHour,
                       aTime.iMinute, aTime.iSecond, hTask, chTag );
      chTag = (chTag == 'Z') ? 'a' : (chTag == 'z') ? 'A' : chTag + 1;
      }
   return( lpStr );
}

/* This function looks for and parses the In-Reply-To field which
 * specifies the message ID of the memo to which this is a reply.
 */
BOOL FASTCALL CheckForInReplyTo( HEADER * pHeader, char * psz )
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
      if( _strcmpi( szType, "In-Reply-To" ) == 0 )
         {
         ParseReplyNumber( psz, pHeader );
         fFound = TRUE;
         }
/*    else if( _strcmpi( szType, "References" ) == 0 )
         {
         ParseReference( psz, pHeader );
         fFound = TRUE;
         }
*/
      pHeader->fCheckForInReplyTo = FALSE;
      }
   return( fFound );
}

BOOL FASTCALL CheckForIPXOLRField( HEADER * pHeader, char * psz )
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
      if( _strcmpi( szType, "X-OLR" ) == 0 )
         {
         psz = ParseXOlr( psz, pHeader );
         fFound = TRUE;
         }
      pHeader->fCheckForXOLRField = FALSE;
      }
   return( fFound );
}

