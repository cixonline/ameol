/* WINSOCK.C - The WINSOCK interface
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
#include "amlib.h"
#include "resource.h"
#include "intcomms.h"
#include "log.h"

#define  THIS_FILE   __FILE__

int (PASCAL FAR * fclosesocket)(SOCKET ) = NULL;
int (PASCAL FAR * fconnect)( SOCKET, const struct sockaddr FAR *, int ) = NULL;
u_short (PASCAL FAR * fhtons)( u_short ) = NULL;
int (PASCAL FAR * frecv)( SOCKET, char FAR *, int, int ) = NULL;
int (PASCAL FAR * fsend)( SOCKET, const char FAR *, int, int ) = NULL;
SOCKET (PASCAL FAR * fsocket)( int, int, int ) = NULL;
struct hostent FAR * (PASCAL FAR * fgethostbyname)( const char FAR * ) = NULL;
struct hostent FAR * (PASCAL FAR * fgethostbyaddr)( const char FAR *, int, int ) = NULL;
struct servent FAR * (PASCAL FAR * fgetservbyname)(const char FAR *, const char FAR * ) = NULL;
FARPROC (PASCAL FAR * fWSASetBlockingHook)( FARPROC ) = NULL;
int (PASCAL FAR * fWSAStartup)( WORD, LPWSADATA ) = NULL;
int (PASCAL FAR * fWSACleanup)( void ) = NULL;
int (PASCAL FAR * fWSAAsyncSelect)( SOCKET, HWND, u_int, long ) = NULL;
int (PASCAL FAR * fWSAGetLastError)( void ) = NULL;
int (PASCAL FAR * fioctlsocket)( SOCKET, long, u_long FAR * ) = NULL;
BOOL (PASCAL FAR * fWSAIsBlocking)( void ) = NULL;
int (PASCAL FAR * fWSACancelBlockingCall)( void ) = NULL;

#ifdef WIN32
DWORD (APIENTRY * fRasHangUp)( HRASCONN );
DWORD (APIENTRY * fRasDial)( LPRASDIALEXTENSIONS, LPSTR, LPRASDIALPARAMSA, DWORD, LPVOID, LPHRASCONN );
DWORD (APIENTRY * fRasGetErrorString)( UINT, LPSTR, DWORD );
DWORD (APIENTRY * fRasEnumConnections)( LPRASCONN, LPDWORD, LPDWORD );
DWORD (APIENTRY * fRasGetConnectStatus)( LPRASCONN, LPRASCONNSTATUS );
#endif

static int cSocketCount = 0;        /* Number of open sockets */
static HINSTANCE hLib = NULL;       /* Handle of WINSOCK.DLL library */

#ifdef WIN32
static HRASCONN hRasConn = NULL;    /* Ras connection handle */
static HINSTANCE hRasLib = NULL;    /* Handle of RASAPI32.DLL library */
#endif

#ifdef WIN32
static char szWinSockLib[] = "WSOCK32.DLL";
#else
static char szWinSockLib[] = "WINSOCK.DLL";
#endif

BOOL FASTCALL Pal_LoadWinsock( LPRASDATA );
void FASTCALL Pal_UnloadWinsock( void );
void FASTCALL UnloadLibraries( void );
BOOL fDoneConnectDevice;
BOOL fDoneDeviceConnected;

/* This function replaces the socket() call. We now load WINSOCK.DLL dynamically
 * if the socket count is zero.
 */
SOCKET FASTCALL Pal_socket( int af, int type, int protocol, LPRASDATA lprd )
{
   SOCKET s;

#ifdef WIN32
   if( Pal_IncrementSocketCount( lprd ) == 0 )
      return( INVALID_SOCKET );
#else
   if( Pal_IncrementSocketCount( NULL ) == 0 )
      return( INVALID_SOCKET );
#endif
   ASSERT( NULL != fsocket );
   if( ( s = fsocket( af, type, protocol ) ) != INVALID_SOCKET );
      return( s );
   Pal_DecrementSocketCount();
   return( INVALID_SOCKET );
}

/* This function replaces the closesocket() call. If the socket count decrements
 * to zero, we unload WINSOCK.DLL
 */
int FASTCALL Pal_closesocket( SOCKET s )
{
   int f;

   ASSERT( cSocketCount > 0 );
   ASSERT( NULL != fclosesocket );
   f = fclosesocket( s );
   Pal_DecrementSocketCount();
   return( f );
}

int FASTCALL Pal_IncrementSocketCount( LPRASDATA lprd )
{
   if( 0 == cSocketCount )
      if( !Pal_LoadWinsock( lprd ) )
         return( 0 );
   return( ++cSocketCount );
}

int FASTCALL Pal_DecrementSocketCount( void )
{
   ASSERT( cSocketCount > 0 );
   if( --cSocketCount == 0 )
      Pal_UnloadWinsock();
   return( cSocketCount );
}

/* This function returns the number of open sockets.
 */
int FASTCALL Pal_GetOpenSocketCount( void )
{
   return( cSocketCount );
}

/* This function replaces the connect() call.
 */
int FASTCALL Pal_connect( SOCKET s, const struct sockaddr FAR *name, int namelen )
{
   ASSERT( NULL != fconnect );
   ASSERT( cSocketCount > 0 );
   return( fconnect( s, name, namelen ) );
}

/* This function replaces the ioctlsocket() call.
 */
int FASTCALL Pal_ioctlsocket( SOCKET s, long cmd, u_long FAR * argp )
{
   ASSERT( NULL != fioctlsocket );
   ASSERT( cSocketCount > 0 );
   return( fioctlsocket( s, cmd, argp ) );
}

/* This function replaces the recv() call.
 */
int FASTCALL Pal_recv( SOCKET s, char FAR * buf, int len, int flags )
{
   ASSERT( NULL != frecv );
   ASSERT( cSocketCount > 0 );
   return( frecv( s, buf, len, flags ) );
}

/* This function replaces the send() call.
 */
int FASTCALL Pal_send( SOCKET s, const char FAR * buf, int len, int flags )
{
   ASSERT( NULL != fsend );
   ASSERT( cSocketCount > 0 );
   return( fsend( s, buf, len, flags ) );
}

/* This function replaces WSASetBlockingHook
 */
FARPROC FASTCALL Pal_WSASetBlockingHook( FARPROC lpfHook )
{
   ASSERT( NULL != fWSASetBlockingHook );
   ASSERT( cSocketCount > 0 );
   return( fWSASetBlockingHook( lpfHook ) );
}

/* This function replaces the WSAGetLastError() call.
 */
int FASTCALL Pal_WSAGetLastError( void )
{
   ASSERT( NULL != fWSAGetLastError );
   ASSERT( cSocketCount > 0 );
   return( fWSAGetLastError() );
}

/* This function replaces the WSAAsyncSelect() call.
 */
int FASTCALL Pal_WSAAsyncSelect( SOCKET s, HWND hWnd, u_int wMsg, long lEvent )
{
   ASSERT( NULL != fWSAAsyncSelect );
   ASSERT( cSocketCount > 0 );
   return( fWSAAsyncSelect( s, hWnd, wMsg, lEvent ) );
}

/* This function replaces the WSACancelBlockingCall() call.
 */
int FASTCALL Pal_WSACancelBlockingCall( void )
{
   ASSERT( NULL != fWSACancelBlockingCall );
   ASSERT( cSocketCount > 0 );
   return( fWSACancelBlockingCall() );
}

/* This function replaces the WSAIsBlocking call.
 */
BOOL FASTCALL Pal_WSAIsBlocking( void )
{
   if( cSocketCount == 0 )
      return( FALSE );
   ASSERT( NULL != fWSAIsBlocking );
   return( fWSAIsBlocking() );
}

/* This function replaces the gethostbyname() call.
 */
struct hostent FAR * FASTCALL Pal_gethostbyname( const char FAR * name )
{
   ASSERT( NULL != fgethostbyname );
   ASSERT( cSocketCount > 0 );
   return( fgethostbyname( name ) );
}

/* This function replaces the getservbyname() call.
 */
struct servent FAR * FASTCALL Pal_getservbyname( const char FAR * name, const char FAR * proto )
{
   ASSERT( NULL != fgetservbyname );
   ASSERT( cSocketCount > 0 );
   return( fgetservbyname( name, proto ) );
}

/* This function replaces the gethostbyaddr() call.
 */
struct hostent FAR * FASTCALL Pal_gethostbyaddr( const char FAR * addr, int len, int type )
{
   ASSERT( NULL != fgethostbyaddr );
   ASSERT( cSocketCount > 0 );
   return( fgethostbyaddr( addr, len, type ) );
}

/* This function replaces the htons() call.
 */
u_short FASTCALL Pal_htons( u_short hostshort )
{
   ASSERT( NULL != fhtons );
   ASSERT( cSocketCount > 0 );
   return( fhtons( hostshort ) );
}

#ifdef WIN32
/* This function returns the open RAS handle.
 */
HANDLE FASTCALL Pal_GetRasHandle( void )
{
   return( hRasConn );
}
#endif

/* This function loads the WINSOCK DLL and patches up the dynamic references to
 * the exported functions.
 */
BOOL FASTCALL Pal_LoadWinsock( LPRASDATA lprd )
{
   WSADATA wsaData;

   /* First try and load the library.
    */
   ASSERT( NULL == hLib );
   if( ( hLib = LoadLibrary( szWinSockLib ) ) == NULL )
      {
      fMessageBox( GetFocus(), 0, GS(IDS_STR517), MB_OK|MB_ICONEXCLAMATION );
      return( FALSE );
      }

   /* Next, dereference access to the exported entrypoints that we'll be using.
    */
   (FARPROC)fsend = GetProcAddress( hLib, "send" );
   (FARPROC)frecv = GetProcAddress( hLib, "recv" );
   (FARPROC)fhtons = GetProcAddress( hLib, "htons" );
   (FARPROC)fWSAStartup = GetProcAddress( hLib, "WSAStartup" );
   (FARPROC)fsocket = GetProcAddress( hLib, "socket" );
   (FARPROC)fWSACleanup = GetProcAddress( hLib, "WSACleanup" );
   (FARPROC)fWSAGetLastError = GetProcAddress( hLib, "WSAGetLastError" );
   (FARPROC)fWSASetBlockingHook = GetProcAddress( hLib, "WSASetBlockingHook" );
   (FARPROC)fWSAAsyncSelect = GetProcAddress( hLib, "WSAAsyncSelect" );
   (FARPROC)fgethostbyname = GetProcAddress( hLib, "gethostbyname" );
   (FARPROC)fclosesocket = GetProcAddress( hLib, "closesocket" );
   (FARPROC)fconnect = GetProcAddress( hLib, "connect" );
   (FARPROC)fgethostbyaddr = GetProcAddress( hLib, "gethostbyaddr" );
   (FARPROC)fioctlsocket = GetProcAddress( hLib, "ioctlsocket" );
   (FARPROC)fWSAIsBlocking = GetProcAddress( hLib, "WSAIsBlocking" );
   (FARPROC)fWSACancelBlockingCall = GetProcAddress( hLib, "WSACancelBlockingCall" );
   (FARPROC)fgetservbyname = GetProcAddress( hLib, "getservbyname" );

   /* Make sure none of them are NULL. Ideally we should be able to tell the user
    * which ones are not supported, but it's sufficient to return FALSE for now.
    */
   if( fsend == NULL || frecv == NULL || fhtons == NULL || fWSAStartup == NULL ||
       fsocket == NULL || fWSACleanup == NULL || fgethostbyname == NULL ||
       fclosesocket == NULL || fconnect == NULL || fgethostbyaddr == NULL ||
       fioctlsocket == NULL || fWSAGetLastError == NULL || fWSAAsyncSelect == NULL ||
       fgetservbyname == NULL || fWSASetBlockingHook == NULL )
      {
      UnloadLibraries();
      fMessageBox( GetFocus(), 0, GS(IDS_STR628), MB_OK|MB_ICONEXCLAMATION );
      return( FALSE );
      }

   /* If Ras enabled, wait for Ras connection.
    */
#ifdef WIN32
   if( lprd->fUseRAS )
      {
      BOOL fHasExistingRASConn;
      LPRASCONN lpRasConn;
      RASDIALPARAMS rdp;
      DWORD cbItems;
      DWORD cb;

      INITIALISE_PTR(lpRasConn);

      /* Switch to the RAS service
       */
      SwitchService( SERVICE_RAS );

      /* First try and load the RAS DLL.
       */
      ASSERT( NULL == hRasLib );
      if( ( hRasLib = LoadLibrary( szRasLib ) ) == NULL )
         {
         UnloadLibraries();
         fMessageBox( GetFocus(), 0, GS(IDS_STR517), MB_OK|MB_ICONEXCLAMATION );
         return( FALSE );
         }

      /* Get the RAS functions.
       */
      (FARPROC)fRasDial = GetProcAddress( hRasLib, "RasDialA" );
      (FARPROC)fRasHangUp = GetProcAddress( hRasLib, "RasHangUpA" );
      (FARPROC)fRasEnumConnections = GetProcAddress( hRasLib, "RasEnumConnectionsA" );
      (FARPROC)fRasGetConnectStatus = GetProcAddress( hRasLib, "RasGetConnectStatusA" );
      (FARPROC)fRasGetErrorString = GetProcAddress( hRasLib, "RasGetErrorStringA" );

      /* Make sure that the functions exist.
       */
      if( NULL == fRasDial || NULL == fRasHangUp || NULL == fRasEnumConnections ||
          NULL == fRasGetConnectStatus )
         {
         UnloadLibraries();
         fMessageBox( GetFocus(), 0, GS(IDS_STR628), MB_OK|MB_ICONEXCLAMATION );
         return( FALSE );
         }

      /* First check whether or not this connection is already active. If so,
       * do nothing.
       */
      cb = 10 * sizeof(RASCONN);
      fHasExistingRASConn = FALSE;
      hRasConn = NULL;
      if( fNewMemory32( &lpRasConn, cb ) )
         {
         DWORD i;

         lpRasConn->dwSize = sizeof(RASCONN);
         if( 0 == fRasEnumConnections( lpRasConn, &cb, &cbItems ) )
            for( i = 0; i < cbItems; ++i )
               if( _strcmpi( lpRasConn[ i ].szEntryName, lprd->szRASEntryName ) == 0 )
                  {
                  fHasExistingRASConn = TRUE;
                  break;
                  }
         FreeMemory32( &lpRasConn );
         }

      /* Dial via RAS.
       */
      if( !fHasExistingRASConn )
         {
         register int c;

         /* Retry RAS dialling up to 5 times.
          */
         for( c = 0; c < 5; ++c )
            {
            /* Dial one loop.
             */
            memset( &rdp, 0, sizeof(RASDIALPARAMS) );
            rdp.dwSize = sizeof(RASDIALPARAMS);
            strcpy( rdp.szEntryName, lprd->szRASEntryName );
            strcpy( rdp.szUserName, lprd->szRASUserName );
            Amuser_Decrypt( lprd->szRASPassword, rgEncodeKey );
            strcpy( rdp.szPassword, lprd->szRASPassword );
            Amuser_Encrypt( lprd->szRASPassword, rgEncodeKey );
            if( 0 != fRasDial( NULL, NULL, &rdp, 0xFFFFFFFF, (LPVOID)hwndFrame, &hRasConn ) )
               {
               ReportOnline( GS(IDS_STR820) );
               UnloadLibraries();
               return( FALSE );
               }

            /* Spin the message loop until either RAS connects or
             * an error occurs.
             */
            nRasConnect = RCS_CONNECTING;
            while( nRasConnect == RCS_CONNECTING )
               {
               BOOL fIdle;

               fIdle = TRUE;
               while( TaskYield() )
                  fIdle = FALSE;
               if( fIdle )
                  WaitMessage();
               }

            /* Handle BUSY and redial up to 5 times.
             */
            if( nRasConnect != RCS_ERROR )
               break;
            if( ERROR_LINE_BUSY != dwRasError &&
                ERROR_NO_ANSWER != dwRasError &&
                ERROR_NO_DIALTONE != dwRasError )
               break;
            if( c == 4 )
               break;

            /* If we come here, we're going to try and dial
             * again. Hang up the line first.
             */
            fRasHangUp( hRasConn );
            if( ERROR_LINE_BUSY == dwRasError)
            {
            OnlineStatusText( GS(IDS_STR1087) );
            WriteToBlinkLog( GS(IDS_STR1087) );
            }
            if ( ERROR_NO_ANSWER == dwRasError)
            {
            OnlineStatusText( GS(IDS_STR1207) );
            WriteToBlinkLog( GS(IDS_STR1207) );
            }
            if ( ERROR_NO_DIALTONE == dwRasError)
            {
            OnlineStatusText( GS(IDS_STR1209) );
            WriteToBlinkLog( GS(IDS_STR1209) );
            }
            
            Sleep( 3000 );
            }

         /* Handle the case where the connect failed due to an error.
          */
         if( nRasConnect == RCS_ERROR )
            {
            char szErrStr[ 150 ];

            fRasGetErrorString( dwRasError, szErrStr, sizeof(szErrStr) );
            UnloadLibraries();
            ReportOnline( szErrStr );
            return( FALSE );
            }

         /* Handle the case where the connect failed because the
          * user terminated Palantir.
          */
         else if( nRasConnect == RCS_ABORTED_DUE_TO_APP_QUIT )
            {
            ReportOnline( GS(IDS_STR817) );
            UnloadLibraries();
            PostMessage( hwndFrame, WM_CLOSE, 0, 0L );
            return( FALSE );
            }

         /* Handle the case where the connect failed due to an error.
          */
         else if( nRasConnect == RCS_ABORTED )
            {
            UnloadLibraries();
            ReportOnline( GS(IDS_STR818) );
            return( FALSE );
            }

         /* Minimise the connect window if Windows 95.
          */
         if( wWinVer >= 0x35F )
            {
            register int c;
            char sz[ 100 ];

            wsprintf( sz, "Connected to %s", lprd->szRASEntryName );
            for( c = 0; c < 100; ++c )
               {
               HWND hwnd;

               hwnd = FindWindow( "#32770", sz );
               if( IsWindow( hwnd ) )
                  {
                  ShowWindow( hwnd, SW_MINIMIZE );
                  break;
                  }
               TaskYield();
               }
            }
         }
      }
#endif

   /* Next, start the WINSOCK driver going.
    */
   if( fWSAStartup( 0x0101, &wsaData ) )
      {
      UnloadLibraries();
      ReportOnline( GS(IDS_STR628) );
      return( FALSE );
      }
   if( LOBYTE( wsaData.wVersion ) != 1 || HIBYTE( wsaData.wVersion ) != 1 )
      {
      fWSACleanup();
      UnloadLibraries();
      ReportOnline( GS(IDS_STR629) );
      return( FALSE );
      }

   return( TRUE );
}

/* This function unloads the loaded libraries.
 */
void FASTCALL UnloadLibraries( void )
{
   if( NULL != hLib )
      {
      FreeLibrary( hLib );
      hLib = NULL;
      }
#ifdef WIN32
   if( NULL != hRasLib )
      {
      /* If Ras enabled, hang up Ras.
       */
      if( 0 != hRasConn )
         {
         OnlineStatusText( GS(IDS_STR867) );
         if( 0 != fRasHangUp( hRasConn ) )
            ReportOnline( GS(IDS_STR819) );
         else
            {
         #ifdef MS_WAY_TO_SHUTDOWN_RAS
            RASCONNSTATUS rcs;
            DWORD dwTickStart;

            /* Wait for RAS connection to really
             * terminate.
             */
            rcs.dwSize = sizeof(RASCONNSTATUS);
            dwTickStart = GetTickCount();
            fRasGetConnectStatus( hRasConn, &rcs );
            while( rcs.dwError != ERROR_INVALID_HANDLE && ( GetTickCount() - dwTickStart ) < 30000 )
               {
               Sleep( 0 );
               fRasGetConnectStatus( hRasConn, &rcs );
               }
            if( rcs.dwError != ERROR_INVALID_HANDLE )
               {
               wsprintf( lpTmpBuf, "Warning: RASGetConnectStatus returned unexpected code %lu", rcs.dwError );
               WriteToBlinkLog( lpTmpBuf );
               }
         #else
            Sleep( 3000 );
         #endif
            }
         OnlineStatusText( GS(IDS_STR868) );
         hRasConn = NULL;
         }
      FreeLibrary( hRasLib );
      hRasLib = NULL;
      fDoneConnectDevice = FALSE;
      fDoneDeviceConnected = FALSE;

      }
#endif
}

/* This function unloads the WINSOCK.DLL library.
 */
void FASTCALL Pal_UnloadWinsock( void )
{
   /* Clean up.
    */
   ASSERT( NULL != hLib );
   ASSERT( cSocketCount == 0 );
   fWSACleanup();

   /* Unload the libraries.
    */
   UnloadLibraries();
}
