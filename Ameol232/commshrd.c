/* COMMSHRD.C - Comms routines shared by COMM16.C and COMM32.C
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
#include "amlib.h"
#include <ctype.h>
#include "intcomms.h"
#include <string.h>
#include <memory.h>
#include "telnet.h"
#include "log.h"
#include "crypto.h"

#define  THIS_FILE   __FILE__

#define  MAX_RINGS         3

static BOOL fRegisteredComms = FALSE;     /* TRUE if comms window registered */

char szCommsClientWndClass[] = "amctl_commsclntwndcls";

#define  CLE_NOERROR             0x0000
#define  CLE_TIMEOUT             0x8001
#define  CLE_BAD_IP_ADDRESS      0x8002
#define  CLE_BAD_HOSTNAME        0x8003
#define  CLE_CANNOT_CONNECT      0x8004

static WORD wLastError;       /* Last error */

int FASTCALL Amcomm_DialNumber( LPCOMMDEVICE, LPSTR );
BOOL WINAPI EXPORT Amcomm_DefaultBlockingHook( void );
void CALLBACK Amcomm_TimerProc( HWND, UINT, UINT, DWORD );

/* DNS cache to speed up blinks.
 */
typedef struct tagA2DNSCACHE {
   char name[ 64 ];
   struct in_addr sin_addr;
} A2DNSCACHE;

#define  MAX_DNS_CACHE     10

static A2DNSCACHE DNSCache[ MAX_DNS_CACHE ]; /* Cached DNS lookups */
static int cInCache = 0;                        /* Number of items in cache */

/* This function opens a comms connection using the specified
 * connection card. On return, the comms handle or socket port in
 * the card is set to the open connection and, thus, should be
 * used in subsequent calls to other Amcomm functions.
 */
BOOL EXPORT WINAPI Amcomm_Open( LPCOMMDEVICE FAR * lppcdev, LPSTR lpConnectionCard, COMMCALLBACK lpfCallback, 
                        DWORD dwAppData, char * pszLogFile, HWND hwndLog, LPRASDATA lprd, BOOL pSendInit /*2.56.2052 FS#148*/, BOOL useTls )
{
   LPCOMMDESCRIPTOR lpcd;
   LPCOMMDEVICE lpcdev;

   INITIALISE_PTR(lpcd);

   // TestCall();

   /* Get the specified connection card.
    */
   if( !fNewMemory( &lpcd, sizeof(COMMDESCRIPTOR) ) )
      {
      if( fDebugMode )
         WriteToBlinkLog( "NewMemory failed in Amcomm_Open (commshrd.c)" );
      return( FALSE );
      }
   strcpy( lpcd->szTitle, lpConnectionCard );
   INITIALISE_PTR(lpcd->lpsc );
   INITIALISE_PTR(lpcd->lpic );
   if( !LoadCommDescriptor( lpcd ) )
      {
      if( fDebugMode )
         WriteToBlinkLog( "LoadCommDescriptor failed in Amcomm_Open (commshrd.c)" );
      return( FALSE );
      }

   /* Plug in RAS data.
    */
#ifdef WIN32
   if( lpcd->wProtocol == PROTOCOL_NETWORK )
      lpcd->lpic->rd = *lprd;
#endif

   
   /* Open this connection card.
    */
   if( Amcomm_OpenCard( hwndFrame, lppcdev, lpcd, lpfCallback, dwAppData, pszLogFile, hwndLog, pSendInit, useTls) )
      {
      lpcdev = *lppcdev;
      lpcdev->fOwnDescriptors = TRUE;
      return( TRUE );
      }
   if( fDebugMode )
      WriteToBlinkLog( "Amcomm_OpenCard failed in Amcomm_Open (commshrd.c)" );
   return( FALSE );
}

/* This function opens the specified connection card.
 */
BOOL WINAPI Amcomm_OpenCard( HWND hwnd, LPCOMMDEVICE FAR * lppcdev, LPCOMMDESCRIPTOR lpcd, COMMCALLBACK lpfCallback, 
                     DWORD dwAppData, char * pszLogFile, HWND hwndLog, BOOL pSendInit/*2.56.2052 FS#148*/, BOOL useTls )
{
   LPCOMMDEVICE lpcdev;

   INITIALISE_PTR(lpcdev);

   /* Create an invisible window for notification
    * messages.
    */
   if( !fRegisteredComms )
      {
      WNDCLASS wc;

      wc.style          = 0;
      wc.lpfnWndProc    = CommsClientWndProc;
      wc.hIcon          = NULL;
      wc.hCursor        = NULL;
      wc.lpszMenuName   = NULL;
      wc.cbWndExtra     = sizeof(LPCOMMDEVICE);
      wc.cbClsExtra     = 0;
      wc.hbrBackground  = 0;
      wc.lpszClassName  = szCommsClientWndClass;
      wc.hInstance      = hInst;
      if( !RegisterClass( &wc ) )
         return( FALSE );
      fRegisteredComms = TRUE;
      }

   /* Create a new comm device descriptor.
    */
   if( !fNewMemory( &lpcdev, sizeof(COMMDEVICE) ) )
      {
      if( fDebugMode )
         WriteToBlinkLog( "NewMemory failed in Amcomm_OpenCard (commshrd.c)" );
      return( FALSE );
      }
   *lppcdev = lpcdev;

   /* Create a window for this device.
    */
   lpcdev->hwnd = CreateWindow( szCommsClientWndClass, "", WS_OVERLAPPED, 0, 0, 0, 0, HWND_DESKTOP, NULL, hInst, 0L );
   if( NULL == lpcdev->hwnd )
      {
      FreeMemory( &lpcdev );
      *lppcdev = NULL;
      if( fDebugMode )
         WriteToBlinkLog( "CreateWindow failed Amcomm_OpenCard (commshrd.c)" );
      return( FALSE );
      }

   /* Save the descriptor in the window extra stuff.
    */
   SetWindowLong( lpcdev->hwnd, 0, (LPARAM)lpcdev );
   lpcdev->fOpen = FALSE;
   lpcdev->fEntry = FALSE;
   lpcdev->fNeedKickStart = FALSE;
   lpcdev->lpcd = lpcd;
   lpcdev->lpfCallback = lpfCallback;
   lpcdev->fCallback = lpfCallback != NULL;
   lpcdev->fAsyncComms = TRUE;
   lpcdev->dwAppData = dwAppData;
   lpcdev->fOwnDescriptors = FALSE;
   lpcdev->lpLogFile = NULL;
   lpcdev->fDataLogging = FALSE;
#ifdef WIN32
   lpcdev->fUseTAPI = FALSE;
#endif
   
   lpcdev->fSendInit = pSendInit;

   /* If a log file is specified, open it.
    */
   if( NULL != pszLogFile )
      Amcomm_OpenLogFile( lpcdev, pszLogFile );

   /* Initialise the comm buffer pointers.
    */
   lpcdev->buf.cbInComBuf = 0;
   lpcdev->buf.cbComBuf = 0;

   /* No error.
    */
   wCommErr = WCE_NOERROR;

   /* If this is a telnet connection, create the connection.
    */
   if( lpcd->wProtocol == PROTOCOL_NETWORK )
      {
      if( Amcomm_OpenSocket( lpcdev, lpcd->lpic, hwndLog, useTls ) )
         return( TRUE );
      else
         if( fDebugMode )
         {
            wsprintf( lpTmpBuf, "Amcomm_OpenSocket failed in Amcomm_OpenCard (commshrd.c)");
            WriteToBlinkLog( lpTmpBuf );
         }
      }
   else
      {
      if( Amcomm_OpenModem( lpcdev, lpcd->lpsc, hwndLog ) )
         return( TRUE );
      else
         if( fDebugMode )
            WriteToBlinkLog( "Amcomm_OpenModem failed in Amcomm_OpenCard (commshrd.c)" );
      }

   /* Failure, so deallocate structure and free
    * memory.
    */
   if( NULL != pszLogFile ) 
      Amcomm_Destroy( lpcdev );

// DestroyWindow( lpcdev->hwnd ); // !!SM!! 2.54
   *lppcdev = NULL;
   return( FALSE );
}

/* This function opens the socket device specified by the
 * IPCOMM connection card entry.
 */
LPCOMMDEVICE FASTCALL Amcomm_OpenSocket( LPCOMMDEVICE lpcdev, IPCOMM FAR * lpic, HWND hwndLog, BOOL useTls )
{
   SOCKADDR_IN dest_sin;
   LPHOSTENT phe;
   LPSTR lpszPort;
   char buf[ 7 ];
   int idTimer;
   
   /* Clear last error.
    */
   wLastError = CLE_NOERROR;

   /* Start a timer. If the timer counts down to zero,
    * we unblock and fail any operations.
    */
   idTimer = SetTimer( NULL, 0, 60000, (TIMERPROC)Amcomm_TimerProc );

   /* Open a socket.
    */
   wLastError = CLE_CANNOT_CONNECT;
#ifdef WIN32
   lpcdev->sock = Pal_socket( AF_INET, SOCK_STREAM, 0, &lpic->rd );
#else
   lpcdev->sock = Pal_socket( AF_INET, SOCK_STREAM, 0, NULL );
#endif
   if( lpcdev->sock == INVALID_SOCKET )
      {
      KillTimer( NULL, idTimer );
      if( fDebugMode )
         WriteToBlinkLog( "lpcdev->sock returned INVALID_SOCKET in Amcomm_OpenSocket (commshrd.c)" );
      return( NULL );
      }

   /* Restart the timer.
    */
   KillTimer( NULL, idTimer );
   idTimer = SetTimer( NULL, 0, 60000, (TIMERPROC)Amcomm_TimerProc );

   /* Resolve the IP address into a port number.
    */
   lpszPort = lpic->szAddress;
   while( *lpszPort == ' ' )
      ++lpszPort;
   if( isdigit( *lpszPort ) )
      {
      BOOL fError;
      LPSTR p;

      p = lpszPort;
      fError = FALSE;
      dest_sin.sin_addr.S_un.S_un_b.s_b1 = ReadIPNumber( &p, &fError );
      dest_sin.sin_addr.S_un.S_un_b.s_b2 = ReadIPNumber( &p, &fError );
      dest_sin.sin_addr.S_un.S_un_b.s_b3 = ReadIPNumber( &p, &fError );
      dest_sin.sin_addr.S_un.S_un_b.s_b4 = ReadIPNumber( &p, &fError );
      if( fError )
         {
         KillTimer( NULL, idTimer );
         wLastError = CLE_BAD_IP_ADDRESS;
         Pal_closesocket( lpcdev->sock );
         return( NULL );
         }
      }

  
   else {
      int c;

      /* First look for name in cache.
       */
      for( c = 0; c < cInCache; ++c )
         if( strcmp( DNSCache[ c ].name, lpszPort ) == 0 )
            {
            dest_sin.sin_addr = DNSCache[ c ].sin_addr;
            break;
            }

      /* Not in cache, so have to query DNS.
       */
      if( c == cInCache )
         {
         int count = 0;
         do
         {
            if( count > 0 )
               Sleep( 1000 );
            phe = Pal_gethostbyname( lpszPort );
            count++;
         }
         while( ( phe == NULL ) && ( count < 5 ) );

         if( phe == NULL )
         {
            if( fDebugMode )
            {
               wsprintf( lpTmpBuf, "Pal_gethostbyname == NULL in AmComm_OpenSocket, commshrd.c, port %s, %d", lpszPort, Pal_WSAGetLastError() );
               WriteToBlinkLog( lpTmpBuf );
            }
            KillTimer( NULL, idTimer );
            wLastError = CLE_BAD_HOSTNAME;
            Pal_closesocket( lpcdev->sock );
            return( NULL );
         }
         _fmemcpy( (LPSTR)&dest_sin.sin_addr, phe->h_addr, phe->h_length );

         /* Save this to the cache.
          */
         if( fDNSCache )
         {
            if( cInCache < MAX_DNS_CACHE )
            {
            strcpy( DNSCache[ cInCache ].name, lpszPort );
            DNSCache[ cInCache ].sin_addr = dest_sin.sin_addr;
            ++cInCache;
            }
         }
         }
      }

   /* Restart the timer.
    */
   KillTimer( NULL, idTimer );
   idTimer = SetTimer( NULL, 0, 60000, (TIMERPROC)Amcomm_TimerProc );

   /* Set our own blocking hook.
    */
   Pal_WSASetBlockingHook( Amcomm_DefaultBlockingHook );

   /* Connect to the IP port.
    */
   wLastError = CLE_CANNOT_CONNECT;
   dest_sin.sin_family = AF_INET;
   dest_sin.sin_port = Pal_htons( lpic->wPort );
   if( Pal_connect( lpcdev->sock, (LPSOCKADDR)&dest_sin, sizeof( dest_sin ) ) < 0 )
      {
      KillTimer( NULL, idTimer );
      Pal_closesocket( lpcdev->sock );
      if( fDebugMode )
         WriteToBlinkLog( "Pal_connect failed in AmComm_OpenSocket (commshrd,c)" );
      return( NULL );
      }

   /* Set an asynchronous callback.
    */
   if( fCIXSyncInProgress )
      lpcdev->fAsyncComms = FALSE;
   else
   {
      if( Pal_WSAAsyncSelect( lpcdev->sock, lpcdev->hwnd, WM_SOCKMSG, FD_READ|FD_CLOSE ) == SOCKET_ERROR )
      {
         KillTimer( NULL, idTimer );
         Pal_closesocket( lpcdev->sock );
         if( fDebugMode )
         {                  
            wsprintf( lpTmpBuf, "Pal_WSAAsyncSelect returned SOCKET_ERROR in AmComm_OpenSocket (commshrd.c) port %s, %d", lpszPort, Pal_WSAGetLastError());
            WriteToBlinkLog( lpTmpBuf );
         }
         return( NULL );
      }
      lpcdev->fAsyncComms = TRUE;
   }

   /* Kill the timer 'cos we're done.
    */
   KillTimer( NULL, idTimer );
   wLastError = CLE_NOERROR;

   /* Send the telnet initial negotiations sequence.
    */
   
   lpcdev->fOpen = TRUE;
   lpcdev->fExpandLFtoCRLF = FALSE;
   if (lpcdev->fSendInit == TRUE)
// if( IPPORT_TELNET == lpic->wPort ) // 2.56.2052 FS#148
   {
      wsprintf( buf, "%c%c%c", IAC, DOTEL, ECHO );       /* Enable echo */
      Pal_send( lpcdev->sock, buf, 3, 0 );
      wsprintf( buf, "%c%c%c", IAC, DOTEL, SGA );        /* Suppress goahead */
      Pal_send( lpcdev->sock, buf, 3, 0 );
      wsprintf( buf, "%c%c%c", IAC, DOTEL, NAWS );       /* Set window size */
      Pal_send( lpcdev->sock, buf, 3, 0 );
      wsprintf( buf, "%c%c%c", IAC, DOTEL, BINARY );     /* Set binary */
      Pal_send( lpcdev->sock, buf, 3, 0 );
      wsprintf( buf, "%c%c%c", IAC, WILLTEL, TERMTYPE ); /* Will send termtype */
      Pal_send( lpcdev->sock, buf, 3, 0 );
   }

   if (useTls) {
	   tls_connect(lpcdev->sock, &lpcdev->tlsState);
   }
   

   /* Attach this connection to the debug terminal
    */
   if( NULL != hwndLog )
      {
      Terminal_AttachConnection( hwndLog, lpcdev );
      lpcdev->fDataLogging = TRUE;
      }

   /* Finally, if we've a script, run it.
    */
   if( *lpcdev->lpcd->szScript )
      ExecuteScript( lpcdev, lpcdev->lpcd->szScript, TRUE );
   return( lpcdev );
}

/* This function returns the last comms error message.
 */
char * WINAPI Amcomm_GetLastError( void )
{
   switch( wLastError )
      {
      case CLE_NOERROR:          return( "No error" );
      case CLE_TIMEOUT:          return( "Connection timed out" );
      case CLE_BAD_IP_ADDRESS:   return( "Badly formed IP address" );
      case CLE_BAD_HOSTNAME:     return( "Cannot resolve host name" );
      case CLE_CANNOT_CONNECT:   return( "Connection refused by server" );
      }
   return( "Unknown error" );
}

/* This is the timer procedure.
 */
void CALLBACK Amcomm_TimerProc( HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime )
{
   if( Pal_WSAIsBlocking() )
      {
      wLastError = CLE_TIMEOUT;
      Pal_WSACancelBlockingCall();
      }
} 

/* This function destroys the connection card device.
 */
void WINAPI Amcomm_Destroy( LPCOMMDEVICE lpcdev )
{
   /* Close TAPI
    */
#ifdef WIN32
   Amcomm_CloseTAPI( lpcdev );
#endif

   /* Close any log file.
    */
   Amcomm_CloseLogFile( lpcdev );

   /* Destroy the client window and deallocate all the
    * memory structures.
    */
   if( lpcdev->fOwnDescriptors )
      {
      if( NULL != lpcdev->lpcd->lpsc )
         FreeMemory( &lpcdev->lpcd->lpsc );
      if( NULL != lpcdev->lpcd->lpic )
         FreeMemory( &lpcdev->lpcd->lpic );
      FreeMemory( &lpcdev->lpcd );
      lpcdev->fOpen = FALSE;
      }
   if( IsWindow( lpcdev->hwnd ) )
      DestroyWindow( lpcdev->hwnd );
   FreeMemory( &lpcdev );
}

/* Our own blocking hook which calls TaskYield.
 */
BOOL WINAPI EXPORT Amcomm_DefaultBlockingHook( void )
{
   BOOL retCode;

   /* Call TaskYield
    */
   retCode = TaskYield();

   /* If Quit flag now set, cancel the blocking
    * hook.
    */
   if( fQuitSeen )
      Pal_WSACancelBlockingCall();
   return( retCode );
}

/* This function dials the phone number specified by the serial
 * device.
 */
BOOL FASTCALL Amcomm_Dial( LPCOMMDEVICE lpcdev, SERIALCOMM FAR * lpsc, HWND hwndLog )
{
   /* If we check for CTS before dialling and it is low, then
    * offer to abort or switch to XON/XOFF flow control.
    */
   if( lpsc->fCheckCTS ) {
      do {
         register int r;

         /* Quit loop now if CTS active.
          */
         if( Amcomm_IsCTSActive( lpcdev ) )
            break;

         /* If this is an unmonitored blink, no prompt is possible so
          * exit now.
          */
         if( fQuitAfterConnect )
            {
            ReportOnline( GS(IDS_STR666) );
            return( FALSE );
            }

         /* Now ask the user what we do next.
          */
         EnsureMainWindowOpen();
         r = fDlgMessageBox( hwndFrame, idsCTSLOWERROR, IDDLG_CTSLOWERROR, NULL, 15000, IDCANCEL );
         if( r == IDCANCEL )
            return( FALSE );
         if( r == IDD_XONXOFF )
            {
            if( !Amcomm_FlowControl( lpcdev, USE_XONXOFF ) )
               return( FALSE );
            ReportOnline( GS(IDS_STR982) );
            break;
            }
         }
      while( TRUE );
      }

   /* Attach this connection to the debug terminal
    */
   if( NULL != hwndLog )
      {
      Terminal_AttachConnection( hwndLog, lpcdev );
      lpcdev->fDataLogging = TRUE;
      }

   /* Delay 1000 before sending reset string to allow modem
    * to 'warm' up.
    */
   OnlineStatusText( "Resetting modem" );
   if( Amcomm_Delay( lpcdev, 1000 ) )
      {
      /* Clear the I/O buffers then send reset.
       */
      Amcomm_ClearInbound( lpcdev );
      Amcomm_ClearOutbound( lpcdev );
      if( Amcomm_Put( lpcdev, lpsc->md.szReset ) )
         {
         char * wstr[ 2 ];

         /* That worked fine, so wait up to 3 seconds for
          * a response.
          */
         wstr[ 0 ] = lpsc->md.szModemAck;
         wstr[ 1 ] = lpsc->md.szModemNack;
         if( Amcomm_WaitForString( lpcdev, 2, wstr, lpcdev->lpcd->nTimeout ) == 0 )
            {
            /* Wait for linefeed from modem
             */
            if( Amcomm_WaitFor( lpcdev, '\n' ) )
               {
               /* Send initialisation string and wait for OK
                */
               OnlineStatusText( "Initialising modem" );
               if( Amcomm_Delay( lpcdev, 1000 ) && Amcomm_Put( lpcdev, lpsc->md.szInitialise ) )
                  {
                  Amcomm_WaitForString( lpcdev, 2, wstr, lpcdev->lpcd->nTimeout );

                  /* Wait for linefeed from modem
                   */
                  if( Amcomm_WaitFor( lpcdev, '\n' ) && Amcomm_Delay( lpcdev, 200 ) )
                     {
                     register int c = 0;

                     /* Dial the phone number.
                      */
                     do
                     {
                        if( Amcomm_DialNumber( lpcdev, lpsc->szPhone ) )
                           {
                           if( *lpcdev->lpcd->szScript )
                              ExecuteScript( lpcdev, lpcdev->lpcd->szScript, TRUE );
                           return( TRUE );
                           }
                        else
                        {

                        /* Reset error because Amcomm_Delay expects it to be
                         * WCE_NOERROR!
                         */
                        if( wCommErr == WCE_ABORT )
                           break;
                        if( c + 1 <= lpsc->wRetry )
                           {
                           wCommErr = WCE_NOERROR;
                           if( !Amcomm_Delay( lpcdev, 2000 ) )
                              break;
                           }
                        c++;
                        }
                     }
                     while( c <= lpsc->wRetry );
                     

                     /* If we get here, we couldn't connect.
                      */
                     ReportOnline( GS(IDS_STR1088) );
                     }
                  }
               }
            }
         }
      }

   /* Return an error.
    */
   SendMessage( hwndFrame, WM_UPDATE_UNREAD, 0, 0L );
   return( FALSE );
}

/* This function dials the specified phone number.
 */
int FASTCALL Amcomm_DialNumber( LPCOMMDEVICE lpcdev, LPSTR lpszNumber )
{
   char * wstr2[ 5 ];
   char sz[ 100 ];
   int cRings;

   /* Create the appropriate dial string depending on whether we're
    * dialing tone or pulse. Then insert any mercury ID.
    */
   if( lpcdev->lpcd->lpsc->wDialType == DTYPE_PULSE )
      wsprintf( sz, "%s%s", (LPSTR)lpcdev->lpcd->lpsc->md.szDialPulse, lpszNumber );
   else
      wsprintf( sz, "%s%s", (LPSTR)lpcdev->lpcd->lpsc->md.szDialTone, lpszNumber );
   InsertMercuryPin( lpcdev, sz );

   /* Send it to the modem and await the modem's response.
    */
   wsprintf( lpTmpBuf, "Dialling %s", lpszNumber );
   OnlineStatusText( lpTmpBuf );
   if( Amcomm_Put( lpcdev, sz ) )
      {
      wstr2[ 0 ] = lpcdev->lpcd->lpsc->md.szConnectMessage;
      wstr2[ 1 ] = lpcdev->lpcd->lpsc->md.szConnectFail;
      wstr2[ 2 ] = lpcdev->lpcd->lpsc->md.szLineBusy;
      wstr2[ 3 ] = "VOICE";
      wstr2[ 4 ] = "RINGING";
      while( TRUE )
         {
         cRings = 0;
         switch( Amcomm_WaitForString( lpcdev, 5, wstr2, lpcdev->lpcd->nTimeout ) )
            {
            case 4:
               /* Allow up to MAX_RINGS ringings before
                * we give up.
                */
               if( ++cRings > MAX_RINGS )
                  {
                  Amcomm_Put( lpcdev, "" );
                  break;
                  }
               break;
      
            case -1:
               /* Timed out.
                */
               return( FALSE );

            case 0:
               /* Success! :-)
                */
               lpcdev->lpcd->lpsc->fOnline = TRUE;
               return( TRUE );

            case 3:
               /* VOICE detected, so fail
                * instantly.
                */
               OnlineStatusText( "Voice detected" );
               return( FALSE );

            case 1:
               /* Failed (NO CARRIER?), so wait 1 second
                * before retrying.
                */
               OnlineStatusText( "Error" );
               return( FALSE );

            case 2:
               /* Busy
                */
               wCommErr = WCE_BUSY;
               OnlineStatusText( "Modem is busy..." );
               return( FALSE );
            }
         }
      }
   return( FALSE );
}

/* This function inserts the designated mercury PIN number in
 * the specified phone number where an ^# appears in the
 * number.
 */
void FASTCALL InsertMercuryPin( LPCOMMDEVICE lpcdev, char * psz )
{
   char sz[ 100 ];
   register int c;

   strcpy( sz, psz );
   for( c = 0; sz[ c ]; ++c )
      if( sz[ c ] == '^' && sz[ c + 1 ] == '#' )
         {
         char szPassword[ 40 ];

         memcpy( szPassword, lpcdev->lpcd->lpsc->szMercury, 40 );
         Amuser_Decrypt( szPassword, rgEncodeKey );
         strcpy( psz, szPassword );
         psz += strlen( szPassword );
         ++c;
         }
      else
         *psz++ = sz[ c ];
   *psz = '\0';
}

/* Returns whether or not the data is being subject to binary
 * to TEXT translation.
 */
BOOL EXPORT WINAPI Amcomm_GetBinary( LPCOMMDEVICE lpcdev )
{
   return( lpcdev->fExpandLFtoCRLF ? TRUE : FALSE );
}

/* This function hangs up a call.
 */
void FASTCALL Amcomm_DisconnectFromHost( LPCOMMDEVICE lpcdev )
{
   if( lpcdev->lpcd->wProtocol & PROTOCOL_SERIAL )
      {
      if( lpcdev->lpcd->lpsc->fOnline )
         {
         if( lpcdev->lpcd->lpsc->fCheckDCD )
            Amcomm_FastHangUp( lpcdev );
         else
            Amcomm_SlowHangUp( lpcdev );
         }
      lpcdev->lpcd->lpsc->fOnline = FALSE;
      }
}

/* This function hangs up the modem by first sending the escape sequence,
 * then the hang-up sequence. The escape sequence places the modem into
 * command mode, and the hang-up sequence ensures that the modem disconnects
 * from the remote computer.
 */
void FASTCALL Amcomm_SlowHangUp( LPCOMMDEVICE lpcdev )
{
   if( lpcdev->lpcd->lpsc->fOnline )
      if( Amcomm_Delay( lpcdev, 1500 ) )
         {
         LPSERIALCOMM lpsc;

         lpsc = lpcdev->lpcd->lpsc;
         Amcomm_WriteStringTimed( lpcdev, lpsc->md.szEscape, strlen( lpsc->md.szEscape ), lpcdev->lpcd->nTimeout * 1000 );
         if( Amcomm_Delay( lpcdev, 1500 ) )
            {
            Amcomm_Put( lpcdev, lpsc->md.szHangup );
            Amcomm_Delay( lpcdev, 1000 );
            }
         }
}

/* This function writes a string to the remote. The string is
 * terminated by the modem command suffix.
 */
BOOL FASTCALL Amcomm_Put( LPCOMMDEVICE lpcdev, char * pszStr )
{
   char sz[ 200 ];

   if( !lpcdev->fOpen )
      return( FALSE );
   wsprintf( sz, "%s%s", (LPSTR)pszStr, (LPSTR)lpcdev->lpcd->lpsc->md.szSuffix );
   Amcomm_TranslateString( sz, FALSE );
   return( Amcomm_WriteStringTimed( lpcdev, sz, lstrlen( sz ), lpcdev->lpcd->nTimeout * 1000 ) );
}

/* This function translates between a modem encoded string and a
 * raw modem decoded strings. A modem encoded string substitutes
 * control characters with the sequence ^x where x is the control
 * character ORed with 64.
 */
void FASTCALL Amcomm_TranslateString( LPSTR lpszStr, BOOL fHow )
{
   if( fHow ) {   /* Translate control codes to ^x sequence */
      LPSTR lpszDst;
      LPSTR lpszSrc;
      char szBuf[ 40 ];

      lpszDst = szBuf;
      lpszSrc = lpszStr;
      while( *lpszSrc )
         if( *lpszSrc < ' ' ) {
            *lpszDst++ = '^';
            *lpszDst++ = *lpszSrc++ + '@';
            }
         else
            *lpszDst++ = *lpszSrc++;
      *lpszDst = '\0';
      strcpy( lpszStr, szBuf );
      }
   else {
      LPSTR lpszDst;

      lpszDst = lpszStr;
      while( *lpszStr )
         if( *lpszStr == '^' ) {
            ++lpszStr;
            if( *lpszStr == '^' )
               *lpszDst++ = *lpszStr++;
            else
               *lpszDst++ = *lpszStr++ - '@';
            }
         else
            *lpszDst++ = *lpszStr++;
      *lpszDst = '\0';
      }
}

/* This function delays for the specified time period,
 * yielding to the system in the meanwhile.
 */
BOOL EXPORT WINAPI Amcomm_Delay( LPCOMMDEVICE lpcdev, DWORD dwMilliSecs )
{
   DWORD dwTicks;

   dwTicks = GetTickCount();
   while( !fQuitSeen && ( GetTickCount() - dwTicks ) < dwMilliSecs )
      {
      if( wCommErr != WCE_NOERROR )
         return( FALSE );
      if( !lpcdev->fOpen )
         return( FALSE );
      TaskYield();
      }
   return( TRUE );
}

/* This function waits for one or more strings to be output by the host system.
 * The return value is the index of the matching string in the array, or -1 if no
 * matching string was detected within the timeout period.
 */
int EXPORT WINAPI Amcomm_WaitForString( LPCOMMDEVICE lpcdev, int count, char * npstr[], int wTime )
{
   size_t max_len;
   char buf[ 100 ];
   register int c;
   BOOL fTimedOutOnce;
   char ch;

   /* Find the longest string of those passed to us.
    */
   max_len = 0;
   for( c = 0; c < count; ++c )
      if( strlen( npstr[ c ] ) > max_len )
         max_len = strlen( npstr[ c ] );

   /* Now loop until we either timeout or we've got
    * a match.
    */
   fTimedOutOnce = FALSE;
   do {
      Amcomm_Get( lpcdev, &ch, wTime );
      while( wCommErr == WCE_NOERROR )
         {
         for( c = max_len - 2; c >= 0; --c )
            buf[ c + 1 ] = buf[ c ];
         buf[ 0 ] = ch;
         for( c = 0; c < count; ++c )
            {
            register int d;
            int lastcol;
            lastcol = strlen( npstr[ c ] ) - 1;
            for( d = lastcol; d >= 0; --d )
               if( npstr[ c ][ d ] != buf[ lastcol - d ] )
                  break;
            if( d < 0 )
               return( c );
            }
         Amcomm_Get( lpcdev, &ch, wTime );
         }

      /* If this is the first timeout, nudge the
       * remote using a CR and try again.
       */
      if( wCommErr == WCE_TIMEOUT && !fTimedOutOnce )
         {
         fTimedOutOnce = TRUE;
         Amcomm_WriteChar( lpcdev, 13 );
         }
      else
         break;
      }
   while( !fTimedOutOnce );
   return( -1 );
}

/* This function returns the comm status.
 */
int EXPORT WINAPI Amcomm_GetCommStatus( void )
{
   return( wCommErr );
}

/* This function waits for a specified character.
 */
BOOL EXPORT WINAPI Amcomm_WaitFor( LPCOMMDEVICE lpcdev, char chWait )
{
   char ch;

   ch = 0;
   if( Amcomm_Get( lpcdev, &ch, lpcdev->lpcd->nTimeout ) )
      while( ch && ch != chWait )
         {
         if( !Amcomm_Get( lpcdev, &ch, lpcdev->lpcd->nTimeout ) )
            break;
         }
   return( ch == chWait );
}

/* This function gets a single character from the remote.
 */
BOOL EXPORT WINAPI Amcomm_Get( LPCOMMDEVICE lpcdev, char * npch, int wTime )
{
   DWORD dwCurTime;
   DWORD dwTime;

   dwCurTime = GetTickCount();
   dwTime = wTime * 1000;
   while( wCommErr == WCE_NOERROR )
      {
      if( wTime && ( GetTickCount() - dwCurTime ) > dwTime )
         break;
      TaskYield();
      if( wCommErr == WCE_ABORT )
         return( FALSE );
      if( Amcomm_ReadChar( lpcdev, npch ) )
         return( TRUE );
      if( !Amcomm_CheckCarrier( lpcdev ) )
         {
         wCommErr = WCE_NOCARRIER;
         return( FALSE );
         }
      if( fQuitSeen )
         return( FALSE );
      }
   if( wCommErr == WCE_NOERROR )
      wCommErr = WCE_TIMEOUT;
   return( FALSE );
}

/* This function enables or disables callback for the specified
 * session.
 */
BOOL EXPORT WINAPI Amcomm_SetCallback( LPCOMMDEVICE lpcdev, BOOL fEnable )
{
   lpcdev->fCallback = fEnable;
   return( TRUE );
}

/* This function reads one character from the input
 * device.
 */
UINT EXPORT WINAPI Amcomm_ReadChar( LPCOMMDEVICE lpcdev, char * pch )
{
   UINT cBytesRead;

   cBytesRead = 0;
   if( lpcdev->buf.cbComBuf == lpcdev->buf.cbInComBuf )
      if( lpcdev->lpcd->wProtocol & PROTOCOL_SERIAL )
         {
         /* We're doing synchronous comms and the buffer is
          * empty, so need to wait until it fills up again.
          */
         if( !Amcomm_ReadCommBuffer( lpcdev ) )
            return( 0 );
         }
      else
         {
         if( !lpcdev->fAsyncComms )
            {
            if( !ReadCommSocket( lpcdev ) )
               return( 0 );
            }
         }
   if( lpcdev->buf.cbComBuf < lpcdev->buf.cbInComBuf )
      {
      *pch = lpcdev->buf.achComBuf[ lpcdev->buf.cbComBuf++ ];
      ++cBytesRead;
      }
   if( lpcdev->buf.cbComBuf == lpcdev->buf.cbInComBuf )
      {
      lpcdev->buf.cbComBuf = 0;
      lpcdev->buf.cbInComBuf = 0;
      }
   return( cBytesRead );
}

/* This function checks that the carrier is active if carrier
 * detect has been enabled.
 */
BOOL EXPORT WINAPI Amcomm_CheckCarrier( LPCOMMDEVICE lpcdev )
{
   if( lpcdev->lpcd->wProtocol & PROTOCOL_SERIAL )
      {
      if( lpcdev->lpcd->lpsc->fCheckDCD && lpcdev->lpcd->lpsc->fOnline )
         if( !Amcomm_IsCarrierActive( lpcdev ) )
            {
            lpcdev->lpcd->lpsc->fOnline = FALSE;
            return( FALSE );
            }
      }
   if( lpcdev->fOpen == FALSE )
      return( FALSE );
   return( TRUE );
}

/* This function reads one character from the input
 * device within the specified time frame.
 */
UINT EXPORT WINAPI Amcomm_ReadCharTimed( LPCOMMDEVICE lpcdev, char * pch, long wTime )
{
   return( Amcomm_ReadStringTimed( lpcdev, pch, 1, wTime ) );
}

/* This function reads a block of data from the input
 * device.
 */
UINT EXPORT WINAPI Amcomm_ReadString( LPCOMMDEVICE lpcdev, LPSTR lpStr, UINT cBytes )
{
   return( Amcomm_ReadStringTimed( lpcdev, lpStr, cBytes, -1L ) );
}

/* This function reads a block of data from the input
 * device.
 */
UINT EXPORT WINAPI Amcomm_ReadStringTimed( LPCOMMDEVICE lpcdev, LPSTR lpStr, UINT cBytes, long dwTime )
{
   DWORD dwStartTime;
   UINT cBytesRead;

   ASSERT( cBytes > 0 && cBytes <= cbMaxComBuf );
   cBytesRead = 0;
   dwStartTime = GetTickCount();
   while( ( GetTickCount() - dwStartTime ) < (DWORD)dwTime && cBytes )
      {
      if( !Amcomm_CheckCarrier( lpcdev ) )
         {
         wCommErr = WCE_NOCARRIER;
         break;
         }
      else if( lpcdev->buf.cbComBuf < lpcdev->buf.cbInComBuf )
         {
         lpStr[ cBytesRead++ ] = lpcdev->buf.achComBuf[ lpcdev->buf.cbComBuf++ ];
         --cBytes;
         }
      else if( dwTime )
         {
         lpcdev->buf.cbComBuf = 0;
         lpcdev->buf.cbInComBuf = 0;
         if( lpcdev->lpcd->wProtocol & PROTOCOL_SERIAL )
            {
            /* We're doing synchronous comms and the buffer is
             * empty, so need to wait until it fills up again.
             */
            if( Amcomm_QueryCharReady( lpcdev ) )
               if( !Amcomm_ReadCommBuffer( lpcdev ) )
                  break;
            }
         else
            {
            if( !lpcdev->fAsyncComms )
               {
               if( Amcomm_QueryCharReady( lpcdev ) )
                  if( !ReadCommSocket( lpcdev ) )
                     break;
               }
            }
         }
      else
         break;
      }
   return( cBytesRead );
}

/* This function writes the specified character to the
 * communications device.
 */
BOOL EXPORT WINAPI Amcomm_WriteChar( LPCOMMDEVICE lpcdev, char ch )
{
   return( Amcomm_WriteCharTimed( lpcdev, ch, 3000 ) );
}

/* This function writes the specified character to the
 * communications device.
 */
BOOL EXPORT WINAPI Amcomm_WriteCharTimed( LPCOMMDEVICE lpcdev, char ch, UINT wTime )
{
   return( Amcomm_WriteStringTimed( lpcdev, &ch, 1, wTime ) );
}

/* This function writes the specified string to the
 * communications device.
 */
BOOL EXPORT WINAPI Amcomm_WriteString( LPCOMMDEVICE lpcdev, LPSTR lpStr, UINT cBytes )
{
   return( Amcomm_WriteStringTimed( lpcdev, lpStr, cBytes, -1L ) );
}

/* This function writes a block of data to the specified
 * socket.
 */
BOOL EXPORT WINAPI Amcomm_WriteData( LPCOMMDEVICE lpcdev, LPSTR lpData, UINT cBytesToWrite )
{
   int offset;

   offset = 0;
   if( lpcdev->fOpen )
      while( cBytesToWrite )
         {
         int cBytesWritten;

         cBytesWritten = Pal_send( lpcdev->sock, lpData + offset, cBytesToWrite, 0 );
         if( SOCKET_ERROR == cBytesWritten )
            {
            int nError;

            nError = Pal_WSAGetLastError();
            if( WSAEINPROGRESS != nError && WSAEWOULDBLOCK != nError )
               {
               if( WSAECONNABORTED == nError )
                  {
                  lpcdev->fOpen = FALSE;
                  ReportOnline( GS(IDS_STR1091) );
                  break;
                  }
               if( WSAECONNRESET == nError )
                  {
                  lpcdev->fOpen = FALSE;
                  ReportOnline( GS(IDS_STR1058) );
                  return( FALSE );
                  }
               }
            cBytesWritten = 0;
            }
         if( cBytesWritten < (int)cBytesToWrite )
            {
            /* Palantir is too fast for the mail server, so
             * stop to allow the buffers to flush then carry
             * on.
             */
            GoToSleep( 200 );
            }
         offset += cBytesWritten;
         cBytesToWrite -= cBytesWritten;
         }
   return( cBytesToWrite == 0 );
}

/* Data has arrived from the socket.
 */
BOOL FASTCALL ReadCommSocket( LPCOMMDEVICE lpcdev )
{
   static BOOL fIAC = FALSE;
   unsigned long count;
   int status;
   char buf[ 30 ];
   register int c;
   register int d;
   int nErrCount;
   int cbComBufPtr;
   BYTE ch;

   /* First count how many bytes are waiting for us.
    */
   if( ( status = Pal_ioctlsocket( lpcdev->sock, (long)FIONREAD, (unsigned long FAR *)&count ) ) == SOCKET_ERROR  )
      {
      lpcdev->fOpen = FALSE;
      return( FALSE );
      }

   /* Nothing? Return now.
    */
   if( count == 0 )
      return( FALSE );

   /* Maximum cbMaxComBuf bytes.
    */
   if( lpcdev->buf.cbInComBuf + count >= cbMaxComBuf )
      count = cbMaxComBuf - lpcdev->buf.cbInComBuf;

   /* No room in buffer. We'll have to try again later.
    */
   if( count == 0 )
      {
      if( lpcdev->fAsyncComms )
         PostMessage( lpcdev->hwnd, WM_SOCKMSG, 0, WSAMAKESELECTREPLY( FD_READ, 0 ) );
      return( FALSE );
      }

   /* If we're missing an IAC, plug it in.
    */
   cbComBufPtr = lpcdev->buf.cbInComBuf;
   c = lpcdev->buf.cbInComBuf;
   if( fIAC )
      {
      lpcdev->buf.achComBuf[ c++ ] = (BYTE)IAC;
      --count;
      }

   /* Now read in that many bytes. We can handle up to cbMaxComBuf bytes at a time before we
    * expect a line terminator.
    */
   nErrCount = 0;
   if( ( status = Pal_recv( lpcdev->sock, lpcdev->buf.achComBuf + c, LOWORD( count ), 0 ) ) == SOCKET_ERROR )
      if( Pal_WSAGetLastError() == WSAEINPROGRESS && nErrCount < 6 )
         {
         /* Another action is in progress. Delay 200ms and try again.
          */
         GoToSleep( 200 );
         ++nErrCount;
         }
      else
         {
         fIAC = FALSE;
         return( FALSE );
         }
   if( status == 0 )
      {
      lpcdev->fOpen = FALSE;
      return( FALSE );
      }
   lpcdev->buf.cbInComBuf += status;
   if( fIAC )
      {
      ++lpcdev->buf.cbInComBuf;
      fIAC = FALSE;
      }

   /* Now go thru the input buffer and handle IAC stuff. Also
    * strip them out.
    */
   d = c = cbComBufPtr;
   while( c < lpcdev->buf.cbInComBuf )
      switch( ch = (BYTE)lpcdev->buf.achComBuf[ c++ ] )
         {
         case IAC:
            if( c == lpcdev->buf.cbInComBuf )
               {
               /* The IAC is split at the end of the buffer, so we can't
                * parse it. Set the fIAC flag so that we plug an IAC at the
                * start of the next combuf reload.
                */
               fIAC = TRUE;
               break;
               }
            switch( (BYTE)lpcdev->buf.achComBuf[ c++ ] )
               {
               case IAC:
                  lpcdev->buf.achComBuf[ d++ ] = (BYTE)255;
                  break;

               case SB: {
                  BYTE parsedat[ 256 ];
                  register int n;

                  /* Read negotiation substring up to next IAC
                   */
                  for( n = 0; n < sizeof(parsedat) && c < lpcdev->buf.cbInComBuf; ++n, ++c )
                     {
                     if( ( ch = lpcdev->buf.achComBuf[ c ] ) == IAC )
                        break;
                     parsedat[ n ] = ch;
                     }
                  if( IAC != ch )
                     {
                     /* Reached end of buffer without full substring!
                      */
                     ASSERT(FALSE);
                     }

                  /* Server wants to know our terminal type. Say VT100
                   */
                  if( parsedat[ 0 ] == TERMTYPE && parsedat[ 1 ] == SEND )
                     {
                     wsprintf( buf, "%c%c%c%cvt100%c%c", IAC, SB, TERMTYPE, 0, IAC, SE );
                     Pal_send( lpcdev->sock, buf, 11, 0 );
                     }
                  break;
                  }

               case WONTTEL:
                  ++c;
                  break;

               case WILLTEL:
                  switch( (BYTE)lpcdev->buf.achComBuf[ c++ ] )
                     {
                     default:
                        wsprintf( buf, "%c%c%c", IAC, WILLTEL, lpcdev->buf.achComBuf[ c-1 ] );
                        Pal_send( lpcdev->sock, buf, 3, 0 );
                        break;

                     case BINARY:
                        lpcdev->fExpandLFtoCRLF = TRUE;
                        break;

                     case ECHO:
                        wsprintf( buf, "%c%c%c", IAC, DOTEL, ECHO );
                        Pal_send( lpcdev->sock, buf, 3, 0 );
                        break;
                     }
                  break;

               case DOTEL:
                  switch( (BYTE)lpcdev->buf.achComBuf[ c++ ] )
                     {
                     default:
                        wsprintf( buf, "%c%c%c", IAC, WONTTEL, lpcdev->buf.achComBuf[ c-1 ] );
                        Pal_send( lpcdev->sock, buf, 3, 0 );
                        break;

                     case ECHO:
                        wsprintf( buf, "%c%c%c", IAC, WONTTEL, ECHO );
                        Pal_send( lpcdev->sock, buf, 3, 0 );
                        break;

                     case SGA:
                        wsprintf( buf, "%c%c%c", IAC, WILLTEL, SGA );
                        Pal_send( lpcdev->sock, buf, 3, 0 );
                        break;
                     }
                  break;

               case DONTTEL:
                  wsprintf( buf, "%c%c%c", IAC, WONTTEL, lpcdev->buf.achComBuf[ c++ ] );
                  Pal_send( lpcdev->sock, buf, 3, 0 );
                  break;
               }
            break;

         default:
            lpcdev->buf.achComBuf[ d++ ] = ch;
            break;
         }

   /* Terminate the buffer.
    */
   lpcdev->buf.achComBuf[ d ] = 0;
   if( ( lpcdev->buf.cbInComBuf = d ) == 0 )
      return( FALSE );
   if( lpcdev->lpLogFile && !lpcdev->lpLogFile->fPaused )
      Amfile_Write( lpcdev->lpLogFile->fhLog, lpcdev->buf.achComBuf + cbComBufPtr, d - cbComBufPtr );
   if( NULL != hwndCixTerminal && lpcdev->fDataLogging )
      Terminal_WriteString( hwndCixTerminal, lpcdev->buf.achComBuf + cbComBufPtr, d - cbComBufPtr, TRUE );
   if( NULL != lpcdev->lpfCallback && lpcdev->fCallback )
      lpcdev->lpfCallback( lpcdev, CCB_DATAREADY, lpcdev->dwAppData, (LPARAM)&lpcdev->buf );
   return( TRUE );
}

/* This function opens the specified log file and redirects
 * all incoming data to that file.
 */
BOOL WINAPI Amcomm_OpenLogFile( LPCOMMDEVICE lpcdev, char * lpszLogFile )
{
   HNDFILE fhLog;

   if( ( fhLog = Amfile_Create( lpszLogFile, 0 ) ) != HNDFILE_ERROR )
      {
      LPLOGFILE lpNewLogFile;

      INITIALISE_PTR(lpNewLogFile);
      if( fNewMemory( &lpNewLogFile, sizeof( LOGFILE ) ) )
         {
         lpNewLogFile->fPaused = FALSE;
         lpNewLogFile->fhLog = fhLog;
         lpNewLogFile->lpNext = lpcdev->lpLogFile;
         lstrcpy( lpNewLogFile->szFileName, lpszLogFile );
         lpcdev->lpLogFile = lpNewLogFile;
         return( TRUE );
         }
      Amfile_Close( fhLog );
      }
   return( FALSE );
}

/* This function closes the current log file.
 */
BOOL WINAPI Amcomm_CloseLogFile( LPCOMMDEVICE lpcdev )
{
   LPLOGFILE lpNextLogFile;

   if( !lpcdev->lpLogFile )
      return( FALSE );
   Amfile_Close( lpcdev->lpLogFile->fhLog );
   lpNextLogFile = lpcdev->lpLogFile->lpNext;
   FreeMemory( &lpcdev->lpLogFile );
   lpcdev->lpLogFile = lpNextLogFile;
   return( TRUE );
}

/* This function enables or disables logging of incoming data for
 * the specified session.
 */
BOOL EXPORT WINAPI Amcomm_SetDataLogging( LPCOMMDEVICE lpcdev, BOOL fEnable )
{
   if( fEnable )
      Amcomm_ResumeLogFile( lpcdev );
   else
      Amcomm_PauseLogFile( lpcdev );
   lpcdev->fDataLogging = fEnable;
   return( TRUE );
}

/* This function resumes logging to the current log
 * file.
 */
BOOL WINAPI Amcomm_ResumeLogFile( LPCOMMDEVICE lpcdev )
{
   if( lpcdev->lpLogFile == NULL )
      return( FALSE );
   if( lpcdev->lpLogFile->fPaused )
      lpcdev->lpLogFile->fPaused = FALSE;
   return( TRUE );
}

/* This function pauses logging to the current log
 * file.
 */
BOOL WINAPI Amcomm_PauseLogFile( LPCOMMDEVICE lpcdev )
{
   if( lpcdev->lpLogFile == NULL )
      return( FALSE );
   if( !lpcdev->lpLogFile->fPaused )
      lpcdev->lpLogFile->fPaused = TRUE;
   return( TRUE );
}
