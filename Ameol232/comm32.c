/* COMM32.C - The Palantir 32-bit communications driver
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

#ifdef _DLL
#include "warnings.h"
#include <windows.h>
#include <windowsx.h>
#include "amlib.h"
#include "amuser.h"
#include "amob.h"
#else
#include "main.h"
#include "resource.h"
#include "amlib.h"
#endif
#include <ctype.h>
#include <process.h>
#include <memory.h>
#include "telnet.h"
#include "intcomms.h"
#include "cixip.h"
#include "log.h"
#include <stdarg.h>

#define  THIS_FILE   __FILE__

/* Comment out the next statement to revert back to the
 * Win32 thread API.
 */
#define  USE_CLIB_THREAD_API

#define  ASCII_XON            0x11
#define  ASCII_XOFF           0x13
#define  CN_EVENT             0x04

#define  TXBUFFERSIZE         4096
#define  RXBUFFERSIZE         4096

#define  MAX_PERMITTED_OVERRUNS     10

static char szTapiLib[] = "tapi32.dll";   /* TAPI DLL name */
static char szTapiLog[] = "tapi.log";     /* TAPI log file */
static HINSTANCE hTapiLib = NULL;         /* TAPI library handle */
static HLINEAPP hTAPI;                    /* TAPI instance handle */
static DWORD cLines;                      /* Number of available TAPI lines */
static int cTapiCount = 0;                /* Count of usage of the hTAPI handle */

BOOL fTapiDialing;                        /* TRUE when dialling via TAPI */
BOOL fTapiAborted;                        /* TRUE if TAPI call aborted */
BOOL fBusy;                               /* TRUE if remote is busy */
int cOverruns;                            /* Overrun errors */

LONG (WINAPI * flineInitialize)( LPHLINEAPP, HINSTANCE, LINECALLBACK, LPCSTR, LPDWORD );
LONG (WINAPI * flineNegotiateAPIVersion)( HLINEAPP, DWORD, DWORD, DWORD, LPDWORD, LPLINEEXTENSIONID );
LONG (WINAPI * flineGetDevCaps)( HLINEAPP, DWORD, DWORD, DWORD, LINEDEVCAPS * );
LONG (WINAPI * flineOpen)( HLINEAPP, DWORD, LPHLINE, DWORD, DWORD, DWORD, DWORD, DWORD, LPLINECALLPARAMS const );
LONG (WINAPI * flineGetID)( HLINE, DWORD, HCALL, DWORD, LPVARSTRING, LPCSTR );
LONG (WINAPI * flineMakeCall)( HLINE, LPHCALL, LPCSTR, DWORD, LPLINECALLPARAMS const );
LONG (WINAPI * flineTranslateAddress)( HLINEAPP, DWORD, DWORD, LPCSTR, DWORD, DWORD, LPLINETRANSLATEOUTPUT );
LONG (WINAPI * flineClose)( HLINE );
LONG (WINAPI * flineShutdown)( HLINEAPP );
LONG (WINAPI * flineDrop)( HCALL, LPCSTR, DWORD );
LONG (WINAPI * flineDeallocateCall)( HCALL );
LONG (WINAPI * ftapiGetLocationInfo)( LPSTR, LPSTR );
LONG (WINAPI * flineConfigDialog)( DWORD, HWND, LPCSTR );
LONG (WINAPI * flineGetTranslateCaps)( HLINEAPP, DWORD, LPLINETRANSLATECAPS );
LONG (WINAPI * flineSetCurrentLocation)( HLINEAPP, DWORD );
LONG (WINAPI * flineGetCallInfo)( HCALL, LPLINECALLINFO );
LONG (WINAPI * flineTranslateDialog)( HLINEAPP, DWORD, DWORD, HWND, LPCSTR );
LONG (WINAPI * flineGetCountry)( DWORD, DWORD, LPLINECOUNTRYLIST );

static DWORD cbQueueSize;           /* Size of the queue */
static char * chQueue = NULL;       /* Pointer to queue */
static DWORD nQueueHead;            /* Index of head of queue */
static DWORD nQueueTail;            /* Index of tail of queue */

BOOL FASTCALL Amcomm_ReadFromQueue32( char *, int, int * );
BOOL FASTCALL Amcomm_SetupConnection( LPCOMMDEVICE );

LPCOMMDEVICE FASTCALL Amcomm_OpenTAPIModem( LPCOMMDEVICE, SERIALCOMM FAR *, HWND );
BOOL FASTCALL Amcomm_TapiDial( LPCOMMDEVICE, SERIALCOMM FAR * );
void EXPORT CALLBACK Amcomm_TapiCallback( DWORD, DWORD, DWORD, DWORD, DWORD, DWORD );
DWORD FAR PASCAL CommWatchProc( LPSTR );
HCOMM FASTCALL GetModemHandle( LPCOMMDEVICE );
void FASTCALL Amcomm_WriteToTerminal( LPCOMMDEVICE, char * );
LINEDEVCAPS * FASTCALL GetLineDeviceCaps( HLINEAPP, DWORD, DWORD );
BOOL FASTCALL Amcomm_TapiPassthrough( LPCOMMDEVICE, SERIALCOMM FAR * );
void CDECL Amcomm_DebugTAPIMessage( char *msg, ... );

/* This function returns information about the active connection
 * and the curren settings.
 */
UINT EXPORT FAR PASCAL Amcomm_Query( LPCOMMDEVICE lpcdev, LPCOMMQUERYSTRUCT lpcqs )
{
   if( NULL != lpcqs )
      if( lpcqs->cbStructSize == sizeof(COMMQUERYSTRUCT) )
         lstrcpy( lpcqs->szCommDevice, "Ameol Unified 32-Bit Driver 1.2" );
   return( sizeof(COMMQUERYSTRUCT) );
}

/* TAPI Debug function (Hopefully to show any IPF's that still occur?)
 * Added 29/9/96
 */
void CDECL Amcomm_DebugTAPIMessage( char *msg, ... )
{
   char szOutLF[ 512 ];
   HNDFILE pFile;

   ASSERT( strlen(msg) + 2 < sizeof(szOutLF) );

   /* Open the trace log file for appending.
    */
   pFile = Amfile_Open( szTapiLog, AOF_WRITE );

   /* I've put this code in because I'm assuming that you can
    * only open a file for appending if it already exists.
    */ 
   if( pFile == HNDFILE_ERROR )
      pFile = Amfile_Create( szTapiLog, 0 );
   else
      Amfile_SetPosition( pFile, 0L, ASEEK_END );

   /* If we've got a trace log file handle, write the string
    * to the file.
    */
   if( pFile != HNDFILE_ERROR )
      {
      va_list argptr;

      va_start(argptr, msg);
      wvsprintf( szOutLF, msg, argptr );
      va_end(argptr);
      Amfile_Write( pFile, szOutLF, strlen(szOutLF) );
      Amfile_Write( pFile, "\r\n", 2 );
      Amfile_Close( pFile );
      }
}

/* This function opens the serial device specified by the
 * SERIALCOMM connection card entry.
 */
LPCOMMDEVICE FASTCALL Amcomm_OpenModem( LPCOMMDEVICE lpcdev, SERIALCOMM FAR * lpsc, HWND hwndLog )
{
   COMMTIMEOUTS ct;
   char sz[ 20 ];
   DWORD dwFlags;

   /* Initialise handles.
    */
   cOverruns = 0;
   lpcdev->dwThreadID = 0L;
   lpcdev->hCommThread = NULL;
   lpcdev->overlappedRead.hEvent = NULL;
   lpcdev->overlappedWrite.hEvent = NULL;

   /* Is RAS open? Need to close down RAS unless it was spawned from
    * outside Ameol or we can't use the modem port.
    * NOTE: No check to see if we're opening the same COM port as is
    * used by RAS!
    */
   if( NULL != Pal_GetRasHandle() )
      {
      if( CountOpenNewsServers() )
         DisconnectAllNewsServers();
      if( lpcdevMail )
         SendMessage( lpcdevMail->hwnd, WM_DISCONNECT, 0, 0L );
      if( lpcdevSMTP )
         SendMessage( lpcdevSMTP->hwnd, WM_DISCONNECT, 0, 0L );
      Pal_DecrementSocketCount();
      }

   /* Use TAPI if specified AND there's a phone number ('cos we can't
    * handle passthrough stuff yet).
    */
   if( lpcdev->lpcd->wProtocol == PROTOCOL_TELEPHONY )
      {
      LPCOMMDEVICE lpcdev2;

      lpcdev2 = Amcomm_OpenTAPIModem( lpcdev, lpsc, hwndLog );
      if( lpcdev->fUseTAPI )
         return( lpcdev2 );
      Amcomm_DebugTAPIMessage( "Could not load TAPI - using ordinary comms" );
      }

   /* Use async if we're in terminal mode, sync otherwise.
    */
   lpcdev->fAsyncComms = !fCIXSyncInProgress;

   /* Open the comm port.
    */
   wsprintf( sz, "\\\\.\\%s", lpsc->szPort );
   dwFlags = 0;
   if( lpcdev->fAsyncComms )
      dwFlags |= FILE_FLAG_OVERLAPPED;
   lpcdev->hComm = CreateFile( sz, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, dwFlags, NULL );
   if( INVALID_HANDLE_VALUE == lpcdev->hComm )
      {
      wsprintf( lpTmpBuf, GS(IDS_STR976), lpsc->szPort );
      ReportOnline( lpTmpBuf );
      return( NULL );
      }

   /* Setup state values. The ReadIntervalTimeout is set
    * to -1 to tell it to return immediately as soon as
    * a new character is received.
    */
   SetupComm( lpcdev->hComm, RXBUFFERSIZE, TXBUFFERSIZE );
   ct.ReadIntervalTimeout = (DWORD)-1;
   ct.ReadTotalTimeoutMultiplier = 0;
   ct.ReadTotalTimeoutConstant = 0;
   ct.WriteTotalTimeoutMultiplier = 0;
   ct.WriteTotalTimeoutConstant = 0;
   SetCommTimeouts( lpcdev->hComm, &ct );

   /* Setup the connection.
    */
   if( !Amcomm_SetupConnection( lpcdev ) )
      return( FALSE );

   /* We're now connected.
    */
   lpcdev->fConnected = TRUE;

   /* Create a thread to watch for comm events.
    */
   if( lpcdev->fAsyncComms )
      {
   #ifdef USE_CLIB_THREAD_API
      lpcdev->hCommThread = (HANDLE)_beginthreadex( NULL, 0, CommWatchProc, lpcdev, 0, &lpcdev->dwThreadID );
   #else
      lpcdev->hCommThread = CreateThread( NULL, 0, CommWatchProc, lpcdev, 0, &lpcdev->dwThreadID );
   #endif
      if( lpcdev->hCommThread == NULL )
         {
         ReportOnline( GS(IDS_STR978) );
         CloseHandle( lpcdev->hComm );
         return( NULL );
         }
      SetThreadPriority( lpcdev->hCommThread, THREAD_PRIORITY_BELOW_NORMAL );
      ResumeThread( lpcdev->hCommThread );

      /* Create an overlapped I/O event for writing.
       */
      memset( &lpcdev->overlappedWrite, 0, sizeof(OVERLAPPED) );
      lpcdev->overlappedWrite.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
      if( NULL == lpcdev->overlappedWrite.hEvent )
         {
         TerminateThread( lpcdev->hCommThread, 0 );
         CloseHandle( lpcdev->hComm );
         ReportOnline( GS(IDS_STR979) );
         return( NULL );
         }

      /* Create an overlapped I/O event for reading.
       */
      memset( &lpcdev->overlappedRead, 0, sizeof(OVERLAPPED) );
      lpcdev->overlappedRead.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
      if( NULL == lpcdev->overlappedRead.hEvent )
         {
         TerminateThread( lpcdev->hCommThread, 0 );
         CloseHandle( lpcdev->overlappedWrite.hEvent );
         CloseHandle( lpcdev->hComm );
         ReportOnline( GS(IDS_STR980) );
         return( NULL );
         }
      }

   /* We never expand when connecting via modem.
    */
   lpcdev->fOpen = TRUE;
   lpcdev->fExpandLFtoCRLF = FALSE;
   lpcdev->lpcd->lpsc->fOnline = FALSE;

   /* Do we have a phone number? If so, dial the number. If not,
    * run any script.
    */
   if( !*lpsc->szPhone )
      {
      if( *lpcdev->lpcd->szScript )
         ExecuteScript( lpcdev, lpcdev->lpcd->szScript, TRUE );
      return( lpcdev );
      }
   else if( Amcomm_Dial( lpcdev, lpsc, hwndLog ) )
      return( lpcdev );

   /* Dialling failed, so close down neatly.
    */
   if( lpcdev->fAsyncComms )
      {
      lpcdev->fConnected = FALSE;
      if( lpcdev->fAsyncComms )
         if( lpcdev->dwThreadID )
            {
            SetCommMask( lpcdev->hComm, 0 );
            while( lpcdev->dwThreadID != 0 );
            }
      CloseHandle( lpcdev->hCommThread );
      if( NULL != lpcdev->overlappedRead.hEvent )
         CloseHandle( lpcdev->overlappedRead.hEvent );
      if( NULL != lpcdev->overlappedWrite.hEvent )
         CloseHandle( lpcdev->overlappedWrite.hEvent );
      }
   CloseHandle( lpcdev->hComm );

   /* Zap the handles. Some tests up the line expect
    * these to be NULL if they're closed.
    */
   lpcdev->hCommThread = NULL;
   lpcdev->overlappedRead.hEvent = NULL;
   lpcdev->overlappedWrite.hEvent = NULL;
   lpcdev->hComm = NULL;
   return( NULL );
}

/* This function sets the baud rate and other stuff for the
 * specified serial port.
 */
BOOL FASTCALL Amcomm_SetupConnection( LPCOMMDEVICE lpcdev )
{
   DCB dcb;

   /* Clear the mask
    */
   SetCommMask( lpcdev->hComm, 0 );

   /* Now set up the port protocol.
    */
   GetCommState( lpcdev->hComm, &dcb );
   dcb.BaudRate = lpcdev->lpcd->lpsc->dwBaud;
   dcb.ByteSize = 8;
   dcb.Parity = NOPARITY;
   dcb.StopBits = ONESTOPBIT;

   /* Setup hardware flow control
    */
   dcb.fOutxCtsFlow = 1;
   dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;

   /* Setup software flow control
    */
   dcb.fInX = dcb.fOutX = FALSE;
   dcb.XonChar = XON;
   dcb.XoffChar = XOFF;
   dcb.XonLim = 38;
   dcb.XoffLim = 38;

   /* Other various settings
    */
   dcb.fBinary = 1;
   dcb.fNull = 0;
   dcb.fParity = 0;
   dcb.EofChar = 0;
   dcb.EvtChar = 0;
   if( SetCommState( lpcdev->hComm, &dcb ) < 0 )
      {
      ReportOnline( GS(IDS_STR977) );
      CloseHandle( lpcdev->hComm );
      return( FALSE );
      }
   return( TRUE );
}

/* This function opens the serial device specified by the
 * SERIALCOMM connection card entry using TAPI.
 */
LPCOMMDEVICE FASTCALL Amcomm_OpenTAPIModem( LPCOMMDEVICE lpcdev, SERIALCOMM FAR * lpsc, HWND hwndLog )
{
   LINEEXTENSIONID extensions;
   DWORD dwVersionToUse;
   LONG lrc;

   /* Vape TAPI.LOG
    */
   Amfile_Delete( szTapiLog );

   /* Assume no TAPI until libraries loaded.
    */
   lpcdev->fUseTAPI = FALSE;

   /* Always drive the TAPI modem in async mode.
    */
   lpcdev->fAsyncComms = TRUE;

   /* Load the TAPI library.
    */
   Amcomm_DebugTAPIMessage( "Loading TAPI" );
   lpcdev->hTAPI = Amcomm_LoadTAPI( Amcomm_TapiCallback );
   if( INVALID_TAPI_HANDLE == lpcdev->hTAPI )
      {
      ReportOnline( GS(IDS_STR1048) );
      return( NULL );
      }

   /* Test each line to find one capable of supporting a data/modem
    * device.
    */
   Amcomm_DebugTAPIMessage( "Looking for TAPI Modem" );
   if( !Amcomm_GetDeviceLine( lpcdev->hTAPI, lpcdev->lpcd->lpsc->md.szModemName, &lpcdev->dwLineToUse ) )
      {
      ReportOnline( GS(IDS_STR1050) );
      Amcomm_UnloadTAPI( lpcdev->hTAPI );
      return( NULL );
      }
   Amcomm_DebugTAPIMessage( "Located TAPI Modem");

   /* Open the line.
    */
   lrc = flineNegotiateAPIVersion( hTAPI, lpcdev->dwLineToUse, 0x00010003, 0x00010004, &dwVersionToUse, &extensions );
   if( lrc < 0 )
      {
      ReportOnline( GS(IDS_STR1051) );
      Amcomm_UnloadTAPI( lpcdev->hTAPI );
      return( NULL );
      }
   lpcdev->dwVersionToUse = dwVersionToUse;
   lrc = flineOpen( lpcdev->hTAPI, lpcdev->dwLineToUse, &lpcdev->hLine, lpcdev->dwVersionToUse, 0, (DWORD)lpcdev, LINECALLPRIVILEGE_OWNER, LINEMEDIAMODE_DATAMODEM, NULL );
   if( lrc )
      {
      ReportOnline( GS(IDS_STR1051) );
      Amcomm_UnloadTAPI( lpcdev->hTAPI );
      return( NULL );
      }

   /* Now have TAPI.
    */
   lpcdev->fUseTAPI = TRUE;

   /* Attach this connection to the debug terminal
    */
   if( NULL != hwndLog )
      {
      Terminal_AttachConnection( hwndLog, lpcdev );
      lpcdev->fDataLogging = TRUE;
      }

   /* No phone number, so open in pass-through mode.
    */
   if( *lpsc->szPhone == '\0' )
      {
      if( !Amcomm_TapiPassthrough( lpcdev, lpsc ) )
         return( NULL );
      return( lpcdev );
      }

   /* Try to dial.
    */
   Amcomm_DebugTAPIMessage( "Starting TAPI Dial");
   if( !Amcomm_TapiDial( lpcdev, lpsc ) )
      {
      Amcomm_DebugTAPIMessage( "TAPI Dial failed" );
      if( INVALID_TAPI_HANDLE != lpcdev->hLine )
         {
         Amcomm_DebugTAPIMessage( "Closing TAPI line" );
         flineClose( lpcdev->hLine );
         }
      Amcomm_UnloadTAPI( lpcdev->hTAPI );
      return( NULL );
      }

   /* Return the handle.
    */
   return( lpcdev );
}

/* This function extracts the line devcaps for the specified
 * TAPI device. The returned handle is either NULL or a calloc'd
 * memory block handle.
 */
LINEDEVCAPS * FASTCALL GetLineDeviceCaps( HLINEAPP hTAPI, DWORD dwDeviceID, DWORD dwVersionToUse )
{
   LINEDEVCAPS * pldc;
   LONG lrc;

   pldc = (LINEDEVCAPS *)calloc( sizeof(LINEDEVCAPS), 1 );
   pldc->dwTotalSize = sizeof(LINEDEVCAPS);
   while( ( lrc = flineGetDevCaps( hTAPI, dwDeviceID, dwVersionToUse, 0, pldc ) ) == 0 )
      if( pldc->dwNeededSize > pldc->dwTotalSize )
         {
         DWORD dwNeededSize;

         dwNeededSize = pldc->dwNeededSize;
         free( pldc );
         pldc = (LINEDEVCAPS *)calloc( dwNeededSize, 1 );
         pldc->dwTotalSize = dwNeededSize;
         }
      else
         break;
   if( lrc )
      {
      free( pldc );
      return( NULL );
      }
   return( pldc );
}

/* This function retrieves the modem handle when using TAPI
 */
HCOMM FASTCALL GetModemHandle( LPCOMMDEVICE lpcdev )
{
   VARSTRING * lpVarString;
   LONG lrc;

   ASSERT(lpcdev->fUseTAPI);

   /* Now get the modem handle.
    */
   lpVarString = (VARSTRING *)malloc( sizeof(VARSTRING) );
   if( NULL == lpVarString )
      return( NULL );
   lpVarString->dwTotalSize = sizeof(VARSTRING);
   do {
      if( ( lrc = flineGetID( 0, 0L, lpcdev->hCall, LINECALLSELECT_CALL, lpVarString, "comm/datamodem" ) ) == 0 &&
         ( lpVarString->dwTotalSize < lpVarString->dwNeededSize ) )
         {
         DWORD dwSize;

         dwSize = lpVarString->dwNeededSize;
         free( lpVarString );
         lpVarString = (VARSTRING *)malloc( dwSize );
         if( NULL == lpVarString )
            return( NULL );
         lpVarString->dwTotalSize = dwSize;
         }
      else if( lrc < 0 )
         return( NULL );
      else
         break;
      }
   while( TRUE );
   lpcdev->hComm = *((LPHANDLE)((LPBYTE)lpVarString + lpVarString->dwStringOffset));
   free( lpVarString );
   return( lpcdev->hComm );
}

/* Put the modem online using TAPI passthrough mode.
 */
BOOL FASTCALL Amcomm_TapiPassthrough( LPCOMMDEVICE lpcdev, SERIALCOMM FAR * lpsc )
{
   LINECALLPARAMS * pCallParams;
   LONG lResult;

   /* Make the call.
    */
   Amcomm_DebugTAPIMessage( "TAPI Initialising for Passthrough...");
   pCallParams = (LINECALLPARAMS *)calloc( sizeof(LINECALLPARAMS), 1 );
   pCallParams->dwTotalSize = sizeof(LINECALLPARAMS);
   pCallParams->dwBearerMode = LINEBEARERMODE_PASSTHROUGH;
   pCallParams->dwMediaMode = LINEMEDIAMODE_DATAMODEM;
   pCallParams->dwCallParamFlags = LINECALLPARAMFLAGS_IDLE;
   pCallParams->dwAddressMode = LINEADDRESSMODE_ADDRESSID;
   lResult = flineMakeCall( lpcdev->hLine, &lpcdev->hCall, NULL, 0, pCallParams );
   if( lResult < 0 )
      {
      ReportOnline( GS(IDS_STR1053) );
      free( pCallParams );
      return( FALSE );
      }
   free( pCallParams );
   return( TRUE );
}

/* Dial using TAPI
 */
BOOL FASTCALL Amcomm_TapiDial( LPCOMMDEVICE lpcdev, SERIALCOMM FAR * lpsc )
{
   char szDisplayableNumber[ 256 ];
   LINECALLPARAMS * pCallParams;
   LINETRANSLATEOUTPUT * lto = NULL;
   char szNumber[ 256 ];
   DWORD dwLocation;
   register int c;
   char * pNumber;
   LONG lResult;

   /* Make sure number is prefixed with '+'
    * Dunno why.
    */
   Amcomm_DebugTAPIMessage( "TAPI Initalise Dialing...");
   szNumber[ 0 ] = '+';
   szNumber[ 1 ] = '\0';

   /* Set the location details
    */
   strcat( szNumber, lpsc->szCountryCode );
   strcat( szNumber, "(" );
   strcat( szNumber, lpsc->szAreaCode );
   strcat( szNumber, ")" );

   /* Finally insert our number.
    */
   pNumber = szNumber + strlen(szNumber);
   strcat( szNumber, lpsc->szPhone );

   /* Set the current location.
    */
   if( *lpsc->szLocation )
      if( _strcmpi( lpsc->szLocation, "(Default)" ) != 0 )
         {
         dwLocation = Amcomm_GetLocationID( lpcdev->hTAPI, lpsc->szLocation );
         if( (DWORD)-1 == dwLocation )
            {
            free( lto );
            wsprintf( lpTmpBuf, GS(IDS_STR1186), lpsc->szLocation );
            ReportOnline( lpTmpBuf );
            return( FALSE );
            }
         flineSetCurrentLocation( lpcdev->hTAPI, dwLocation );
         }

   /* Translate the number into a dial string.
    */
   lto = (LINETRANSLATEOUTPUT *)calloc( sizeof(LINETRANSLATEOUTPUT) + 5000 , 1 );
   lto->dwTotalSize = sizeof(LINETRANSLATEOUTPUT) + 5000;
   lResult = flineTranslateAddress( lpcdev->hTAPI, lpcdev->dwLineToUse, lpcdev->dwVersionToUse, szNumber, 0, 0, lto);
   if( lResult != 0 )
      {
      free( lto );
      ReportOnline( GS(IDS_STR1052) );
      return( FALSE );
      }
   memcpy( szNumber, (LPSTR)((DWORD)lto + lto->dwDialableStringOffset), lto->dwDialableStringSize );
   szNumber[ lto->dwDialableStringSize ] = 0;
   memcpy( szDisplayableNumber, (LPSTR)((DWORD)lto + lto->dwDisplayableStringOffset), lto->dwDisplayableStringSize );
   szDisplayableNumber[ lto->dwDisplayableStringSize ] = 0;
   free( lto );

   /* Show status
    */
   c = 0;
   do
   {
      wsprintf( lpTmpBuf, GS(IDS_STR1090), szDisplayableNumber );
      OnlineStatusText( lpTmpBuf );

      /* Also show in terminal window.
       */
      Amcomm_WriteToTerminal( lpcdev, GS(IDS_STR1063) );

      /* Make the call.
       */
      pCallParams = (LINECALLPARAMS *)calloc( sizeof(LINECALLPARAMS), 1 );
      pCallParams->dwTotalSize = sizeof(LINECALLPARAMS);
      pCallParams->dwBearerMode = LINEBEARERMODE_VOICE;
      pCallParams->dwMediaMode = LINEMEDIAMODE_DATAMODEM;
      pCallParams->dwCallParamFlags = LINECALLPARAMFLAGS_IDLE;
      pCallParams->dwAddressMode = LINEADDRESSMODE_ADDRESSID;
//    lpcdev->hLine = NULL; // !!SM!! 2.55
      lResult = flineMakeCall( lpcdev->hLine, &lpcdev->hCall, szNumber, 0, pCallParams );
      if( lResult < 0 )
         {
         ReportOnline( GS(IDS_STR1053) );
         free( pCallParams );
         return( FALSE );
         }
      free( pCallParams );

      /* Now wait until the call is established.
       */
      fTapiDialing = TRUE;
      fTapiAborted = FALSE;
      fBusy = FALSE;
      while( fTapiDialing )
         {
         BOOL fIdle;

         fIdle = TRUE;
         while( TaskYield() )
            fIdle = FALSE;
         if( fIdle )
            WaitMessage();
         }
      fTapiDialing = FALSE;

      /* If we got a busy, pause and try again.
       */
      if( !fBusy || ( fBusy && c + 1 <= lpsc->wRetry ) )
         break;
      Amcomm_WriteToTerminal( lpcdev, GS(IDS_STR1087) );
      OnlineStatusText( GS(IDS_STR1087) );
      WriteToBlinkLog( GS(IDS_STR1087) );
      Sleep( 2000 );
      c++;
      }
      while( c <= lpsc->wRetry );


   /* If aborted, fail now.
    */
   ShowUnreadMessageCount();
   if( fTapiAborted )
      {
      Amcomm_DebugTAPIMessage( "TAPI: Connect aborted");
      return( FALSE );
      }

   /* Run any script specified.
    */
   if( *lpcdev->lpcd->szScript )
      ExecuteScript( lpcdev, lpcdev->lpcd->szScript, TRUE );
   return( TRUE );
}

/* This function writes to the terminal window.
 */
void FASTCALL Amcomm_WriteToTerminal( LPCOMMDEVICE lpcdev, char * pszStr )
{
   if( NULL != hwndCixTerminal && lpcdev->fDataLogging )
      Terminal_WriteString( hwndCixTerminal, pszStr, strlen(pszStr), TRUE );
   else if( NULL != lpcdev->lpfCallback && lpcdev->fCallback )
      lpcdev->lpfCallback( lpcdev, CCB_STATUSTEXT, lpcdev->dwAppData, (LPARAM)pszStr );
}

/* TAPI callback function.
 */
void EXPORT CALLBACK Amcomm_TapiCallback( DWORD dwDevice, DWORD dwMessage, DWORD dwInst, DWORD dwP1, DWORD dwP2, DWORD dwP3 )
{
   switch( dwMessage )
      {
      case LINE_REPLY:
         switch( dwP2 )
            {
            case LINEERR_OPERATIONFAILED:
               if( fTapiDialing )
                  {
                  fTapiDialing = FALSE;
                  fTapiAborted = TRUE;
                  ReportOnline( "*ERROR* : Dialling failed" );
                  Amcomm_DebugTAPIMessage( "TAPI: LINEERR_OPERATIONFAILED!");
                  }
               break;
               
            case LINEERR_CALLUNAVAIL:
            case LINEERR_DIALDIALTONE:
               if( fTapiDialing )
                  {
                  /* We actually get LINEERR_CALLUNAVAIL if there's no dial tone,
                   * but I've no way of telling whether this is guaranteed or not!
                   */
                  fTapiDialing = FALSE;
                  fTapiAborted = TRUE;
                  ReportOnline( "*ERROR* : Dialling failed - No Dial Tone" );
                  Amcomm_DebugTAPIMessage( "TAPI: LINEERR_CALLUNAVAIL/LINEERR_DIALDIALTONE!");
                  }
               break;
            }
         break;

      case LINE_CALLSTATE:
         switch( dwP1 )
            {
            case LINECALLSTATE_BUSY:
               if( fTapiDialing )
                  {
                  fTapiDialing = FALSE;
                  fTapiAborted = TRUE;
                  fBusy = TRUE;
                  ReportOnline( GS(IDS_STR1088) );
                  WriteToBlinkLog( GS(IDS_STR1088) );
                  }
               break;

            case LINECALLSTATE_DISCONNECTED: {
               LPCOMMDEVICE lpcdev;
               
               /* For some obscure reason, if a line busy occurs it seems to 
                * come through to here, not LINECALLSTATE_BUSY
                * I know there's no logic to it, but..... [RJP Feb '98]
                */
               if ((dwP2==LINEDISCONNECTMODE_BUSY) && (fTapiDialing))
                  {
                  fTapiDialing = FALSE;
                  fTapiAborted = TRUE;
                  fBusy = TRUE;
                  ReportOnline( GS(IDS_STR1088) );
                  WriteToBlinkLog( GS(IDS_STR1088) );
                  return;
                  }

               /* If the remote service disconnects for us then
                * hang up.
                */
               lpcdev = (LPCOMMDEVICE)dwInst;
               Amcomm_WriteToTerminal( lpcdev, GS(IDS_STR1065) );
               WriteToBlinkLog( GS(IDS_STR1065) );
               lpcdev->lpcd->lpsc->fOnline = FALSE;
               Amcomm_Close( lpcdev );
               if( fTapiDialing )
                  {
                  fTapiDialing = FALSE;
                  fTapiAborted = TRUE;
                  ReportOnline( GS(IDS_STR1089) );
                  WriteToBlinkLog( GS(IDS_STR1089) );
                  }
               break;
               }

            case LINECALLSTATE_CONNECTED: {
               LINECALLINFO * lplci;
               LPCOMMDEVICE lpcdev;
               LONG lrc;

               /* Got a connection!
                */
               fTapiDialing = FALSE;

               /* Set modem handle.
                */
               lpcdev = (LPCOMMDEVICE)dwInst;
               lpcdev->fConnected = TRUE;
               lpcdev->hComm = GetModemHandle( lpcdev );

               /* Report on our success.
                */
               lplci = (LINECALLINFO *)calloc( sizeof(LINECALLINFO), 1 );
               lplci->dwTotalSize = sizeof(LINECALLINFO);
               while( ( lrc = flineGetCallInfo( lpcdev->hCall, lplci ) ) == 0 )
                  if( lplci->dwNeededSize > lplci->dwTotalSize )
                     {
                     DWORD dwNeededSize;

                     dwNeededSize = lplci->dwNeededSize;
                     free( lplci );
                     lplci = (LINECALLINFO *)calloc( dwNeededSize, 1 );
                     lplci->dwTotalSize = dwNeededSize;
                     }
                  else
                     break;
               wsprintf( lpTmpBuf, GS(IDS_STR1064), lplci->dwRate );
               Amcomm_WriteToTerminal( lpcdev, lpTmpBuf );

               /* We never expand when connecting via modem.
                */
               lpcdev->fOpen = TRUE;
               lpcdev->fExpandLFtoCRLF = FALSE;
               lpcdev->lpcd->lpsc->fOnline = FALSE;
               Amcomm_DebugTAPIMessage( "TAPI Connected!");

               /* Now create a thread for reading data.
                * Created inactive since if it is triggered before everything has
                * initalised, nasty things could happen?
                */
               if( lpcdev->fAsyncComms )
                  {
               #ifdef USE_CLIB_THREAD_API
                  lpcdev->hCommThread = (HANDLE)_beginthreadex( NULL, 0, CommWatchProc, lpcdev, CREATE_SUSPENDED, &lpcdev->dwThreadID );
               #else
                  lpcdev->hCommThread = CreateThread( NULL, 0, CommWatchProc, lpcdev, CREATE_SUSPENDED, &lpcdev->dwThreadID );
               #endif
                  if( lpcdev->hCommThread == NULL )
                     {
                     ReportOnline( GS(IDS_STR978) );
                     CloseHandle( lpcdev->hComm );
                     fTapiAborted = TRUE;
                     break;
                     }

                  /* Create an overlapped I/O event for writing.
                   */
                  Amcomm_DebugTAPIMessage( "Creating Write Event");
                  memset( &lpcdev->overlappedWrite, 0, sizeof(OVERLAPPED) );
                  lpcdev->overlappedWrite.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
                  if( NULL == lpcdev->overlappedWrite.hEvent )
                     {
                     TerminateThread( lpcdev->hCommThread, 0 );
                     CloseHandle( lpcdev->hComm );
                     ReportOnline( GS(IDS_STR979) );
                     fTapiAborted = TRUE;
                     break;
                     }

                  /* Create an overlapped I/O event for reading.
                   */
                  Amcomm_DebugTAPIMessage( "Creating Read Event");
                  memset( &lpcdev->overlappedRead, 0, sizeof(OVERLAPPED) );
                  lpcdev->overlappedRead.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
                  if( NULL == lpcdev->overlappedRead.hEvent )
                     {
                     TerminateThread( lpcdev->hCommThread, 0 );
                     CloseHandle( lpcdev->overlappedWrite.hEvent );
                     CloseHandle( lpcdev->hComm );
                     ReportOnline( GS(IDS_STR980) );
                     fTapiAborted = TRUE;
                     break;
                     }
                  Amcomm_DebugTAPIMessage( "TAPI: Setting Procedures");
                  
                  /*Comm threads should to have a higher base priority than the UI thread.
                   * If they don't, then any temporary priority boost the UI thread gains
                   * could cause the COMM threads to loose data.
                   * Well, that's what MSDN has to say anyway -- I'm sure they know what 
                   * they are talking abut :-)
                   */
                  SetThreadPriority( lpcdev->hCommThread, THREAD_PRIORITY_HIGHEST );
                  Amcomm_DebugTAPIMessage( "TAPI: Setting Priorities");
                  ResumeThread( lpcdev->hCommThread );  
                  }
               Amcomm_DebugTAPIMessage( "TAPI: Connection Completed!");
               break;
               }
            }
         break;
      }
}

/* Close TAPI and free up the TAPI comms driver
 * library handle.
 */
void FASTCALL Amcomm_CloseTAPI( LPCOMMDEVICE lpcdev )
{
   if( lpcdev->fUseTAPI )
   {
      /* Report to the terminal window.
       */
      Amcomm_WriteToTerminal( lpcdev, GS(IDS_STR1066) );

      if ( hTapiLib != NULL /*!!SM!! 2.55*/)
      {
         /* Hang up the line
          */
         if( INVALID_TAPI_HANDLE != lpcdev->hCall )
            {
            flineDrop( lpcdev->hCall, NULL, 0 );
            flineDeallocateCall( lpcdev->hCall );
            }
         if( INVALID_TAPI_HANDLE != lpcdev->hLine )
            flineClose( lpcdev->hLine );
      }
      Amcomm_UnloadTAPI( lpcdev->hTAPI );
      lpcdev->fUseTAPI = FALSE;
   }
}

/* Close the specified comms port.
 */
void EXPORT WINAPI Amcomm_Close( LPCOMMDEVICE lpcdev )
{
   if( lpcdev->fOpen )
   {
      if( lpcdev->lpcd->wProtocol & PROTOCOL_SERIAL )
      {
         /* Disconnect from the host
          */
         Amcomm_DisconnectFromHost( lpcdev );

         /* Set the fConnected flag to FALSE. This will be detected
          * by the comms watcher thread which will then break out of
          * its loop and terminate itself.
          */
         lpcdev->fConnected = FALSE;
         if( lpcdev->fAsyncComms )
            if( lpcdev->dwThreadID )
               {
               SetCommMask( lpcdev->hComm, 0 );
               if( lpcdev->hCommThread )
                  {
                  /* Signal the event to close the worker threads.
                   */
                  SetEvent( &lpcdev->osCommEvent );

                  /* Purge all outstanding reads
                   */
                  PurgeComm( lpcdev->hComm, PURGE_RXABORT | PURGE_RXCLEAR );

                  /* Wait 10 seconds for it to exit.  Shouldn't happen.
                   */
                  if( WaitForSingleObject( lpcdev->hCommThread, 10000 ) == WAIT_TIMEOUT )
                     {
                     /* Thread didn't close -- we will have to kill it...
                      */
                     TerminateThread(lpcdev->hCommThread, 0);

                     /* The ReadThread cleans up these itself if it terminates
                      * normally.
                      */
                     FreeMemory( &chQueue );
                     CloseHandle( lpcdev->osCommEvent.hEvent );
                     }
                  }
               }

         /* Clear DTR and the input/output buffers.
          */
         PurgeComm( lpcdev->hComm, PURGE_TXABORT|PURGE_RXABORT|PURGE_TXCLEAR|PURGE_RXCLEAR );

         /* Close the various comms handles.
          */
         if( lpcdev->fAsyncComms )
            {
            if( NULL != lpcdev->overlappedRead.hEvent )
               CloseHandle( lpcdev->overlappedRead.hEvent );
            if( NULL != lpcdev->overlappedWrite.hEvent )
               CloseHandle( lpcdev->overlappedWrite.hEvent );
            CloseHandle( lpcdev->hCommThread );
            }
         CloseHandle( lpcdev->hComm );
         lpcdev->fOpen = FALSE;

         /* Report if any overruns.
          */
         if( cOverruns )
            ReportOnline( GS(IDS_STR1129) );
      }
      else 
      {
         Pal_closesocket( lpcdev->sock );
         lpcdev->sock = INVALID_SOCKET;
         lpcdev->fOpen = FALSE;
      }
   }
}

/* This function hangs up the modem by dropping the DTR line.
 */
void FASTCALL Amcomm_FastHangUp( LPCOMMDEVICE lpcdev )
{
   if( lpcdev->lpcd->lpsc->fOnline )
      {
      EscapeCommFunction( lpcdev->hComm, CLRRTS );
      EscapeCommFunction( lpcdev->hComm, CLRDTR );
      if( Amcomm_Delay( lpcdev, 500 ) )
         {
         EscapeCommFunction( lpcdev->hComm, SETDTR );
         Amcomm_Delay( lpcdev, 500 );
         }
      EscapeCommFunction( lpcdev->hComm, CLRDTR );
      lpcdev->lpcd->lpsc->fOnline = FALSE;
      }
}

/* This function returns a flag which specifies whether or
 * not there is data ready.
 */
BOOL EXPORT WINAPI Amcomm_QueryCharReady( LPCOMMDEVICE lpcdev )
{
   unsigned long count;
   int status;

   if( lpcdev->buf.cbComBuf < lpcdev->buf.cbInComBuf )
      return( TRUE );
   if( lpcdev->lpcd->wProtocol & PROTOCOL_SERIAL )
      {
      DWORD dwError;
      COMSTAT cStat;

      if( lpcdev->fAsyncComms )
         return( nQueueHead != nQueueTail );
      ClearCommError( lpcdev->hComm, &dwError, &cStat );
      return( cStat.cbInQue );
      }
   else
      {
      if( lpcdev->fAsyncComms )
         TaskYield();
      if( ( status = Pal_ioctlsocket( lpcdev->sock, (long)FIONREAD, (unsigned long FAR *)&count ) ) == SOCKET_ERROR )
         {
         lpcdev->fOpen = FALSE;
         return( FALSE );
         }
      }
   return( count > 0 );
}

/* This function reads data from the serial port.
 */
BOOL EXPORT WINAPI Amcomm_ReadCommBuffer( LPCOMMDEVICE lpcdev )
{
   DWORD dwBytesRead;

   /* Deal with async comms.
    */
   dwBytesRead = 0;
   if( lpcdev->fAsyncComms )
      {
      /* Error if carrier down.
       */
      if( !Amcomm_CheckCarrier( lpcdev ) )
         {
         wCommErr = WCE_NOCARRIER;
         return( FALSE );
         }

      /* Read the data.
       */
      Amcomm_ReadFromQueue32( lpcdev->buf.achComBuf, cbMaxComBuf, &dwBytesRead );
      }
   else
      {
      DWORD dwCommError;
      COMSTAT comstat;

      /* Error if carrier down.
       */
      if( !Amcomm_CheckCarrier( lpcdev ) )
         {
         wCommErr = WCE_NOCARRIER;
         return( FALSE );
         }

      /* Clear any pending error.
       */
      if( !ClearCommError( lpcdev->hComm, &dwCommError, &comstat ) )
         return( FALSE );
 
      /* Read the data.
       */
      if( 0 == comstat.cbInQue )
         dwBytesRead = 0;
      else
         {
         DWORD dwBytesToRead;

         dwBytesToRead = ( comstat.cbInQue < cbMaxComBuf ) ? comstat.cbInQue : cbMaxComBuf;
         if( !ReadFile( lpcdev->hComm, lpcdev->buf.achComBuf, dwBytesToRead, &dwBytesRead, NULL ) )
            {
            DWORD dwLastError;

            dwLastError = GetLastError();
            ClearCommError( lpcdev->hComm, &dwCommError, &comstat );
            }
         }
      }

   /* NULL terminate the buffer.
    */
   lpcdev->buf.cbInComBuf = LOWORD( dwBytesRead );
   lpcdev->buf.achComBuf[ lpcdev->buf.cbInComBuf ] = 0;
   lpcdev->buf.cbComBuf = 0;

   /* We've got data, so call hooks.
    */
   if( 0 < lpcdev->buf.cbInComBuf )
      {
      if( lpcdev->lpLogFile && !lpcdev->lpLogFile->fPaused )
         Amfile_Write( lpcdev->lpLogFile->fhLog, lpcdev->buf.achComBuf, lpcdev->buf.cbInComBuf );
      if( NULL != hwndCixTerminal && lpcdev->fDataLogging )
         Terminal_WriteString( hwndCixTerminal, lpcdev->buf.achComBuf, lpcdev->buf.cbInComBuf, TRUE );
      if( NULL != lpcdev->lpfCallback && lpcdev->fCallback )
         lpcdev->lpfCallback( lpcdev, CCB_DATAREADY, lpcdev->dwAppData, (LPARAM)&lpcdev->buf );
      }
   return( lpcdev->buf.cbInComBuf != 0 ); 
}

/* This function writes the specified string to the
 * communications device.
 */
BOOL EXPORT WINAPI Amcomm_WriteStringTimed( LPCOMMDEVICE lpcdev, LPSTR lpStr, UINT cBytes, long wTime )
{
   UINT count;
   UINT cBytesToWrite;

   count = 0;
   cBytesToWrite = cBytes;
   if( lpcdev->lpcd->wProtocol & PROTOCOL_SERIAL )
      {
      if( !lpcdev->fAsyncComms )
         {
         DWORD dwBytesWritten;
         DWORD dwStartTime;

         /* Send data using synchronous write. Loop and try to write the
          * data as much at a time until all data is written out.
          */
         count = 0;
         dwStartTime = GetTickCount();
         while( ( GetTickCount() - dwStartTime ) < (DWORD)wTime && 0 < cBytes )
            {
            COMSTAT cStat;
            DWORD dwError;
            DWORD cBytesToWrite;

            ClearCommError( lpcdev->hComm, &dwError, &cStat );
            cBytesToWrite = TXBUFFERSIZE - cStat.cbOutQue;
            if( cBytes < cBytesToWrite )
               cBytesToWrite = cBytes;
            if( cBytesToWrite )
               {
               if( !WriteFile( lpcdev->hComm, lpStr + count, cBytesToWrite, &dwBytesWritten, NULL ) )
                  {
                  ClearCommError( lpcdev->hComm, &dwError, &cStat );
                  break;
                  }
               cBytes -= (UINT)dwBytesWritten;
               count += (UINT)dwBytesWritten;
               }
            }
         }
      else
         {
         DWORD dwBytesWritten;
         BOOL fWriteStat;
         COMSTAT cStat;
         DWORD dwError;

         /* Send data using asynchronous write. Send all the data then wait until
          * the driver tells us that all is done.
          */
         ClearCommError( lpcdev->hComm, &dwError, &cStat );
         fWriteStat = WriteFile( lpcdev->hComm, lpStr, cBytes, &dwBytesWritten, &lpcdev->overlappedWrite );
         if( !fWriteStat && ( GetLastError() == ERROR_IO_PENDING ) )
            {
            if( WaitForSingleObject( lpcdev->overlappedWrite.hEvent, wTime ) )
               dwBytesWritten = 0;
            else
               {
               GetOverlappedResult( lpcdev->hComm, &lpcdev->overlappedWrite, &dwBytesWritten, FALSE );
               lpcdev->overlappedWrite.Offset = 0;
               lpcdev->overlappedWrite.OffsetHigh = 0;
               }
            }
         count = (UINT)dwBytesWritten;
         }
      }
   else
      {
      register UINT c;
      register UINT d;
      register UINT e;

      /* Loop through string, looking for the longest run which does
       * not include an IAC. For each IAC seen, send it followed by another
       * which effectively translates into ASCII 255 on the server.
       */
      for( d = c = e = 0; c < cBytes; ++c )
         if( ++e, (BYTE)lpStr[ c ] == IAC )
            {
            if( !Amcomm_WriteData( lpcdev, lpStr + d, e ) )
               return( FALSE );
            count += ( e - 1 );
            d = c;
            e = 1;
            }
      if( e )
         {
         if( !Amcomm_WriteData( lpcdev, lpStr + d, e ) )
            return( FALSE );
         count += e;
         }
      }
   return( count == cBytesToWrite );
}

/* This function handles all messages from the comms server.
 */
LRESULT EXPORT CALLBACK CommsClientWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      case WM_DISCONNECT:
         {
            if( fDebugMode )
            {                  
               WriteToBlinkLog( "CommsClientWndProc: WM_DISCONNECT" );
            }

            DestroyWindow( hwnd );
            fEndBlinking = TRUE;
            return( 0L );
         }

      case WM_COMMNOTIFY: {
         LPCOMMDEVICE lpcdev;

         if( 0 != ( lpcdev = (LPCOMMDEVICE)GetWindowLong( hwnd, 0 ) ) )
            if( LOWORD(lParam) & CN_EVENT )
               {
               int nLength;
               int cbComBufPtr;
               int count;

               /* Figure out how much space we've got for reading
                * any data.
                */
               count = cbMaxComBuf - lpcdev->buf.cbInComBuf;
               ASSERT( count >= 0 );
               cbComBufPtr = lpcdev->buf.cbInComBuf;

               /* Read from our internal queue.
                */
               Amcomm_ReadFromQueue32( lpcdev->buf.achComBuf + cbComBufPtr, count, &nLength );

               /* If we got any data, use it.
                */
               if( nLength )
                  {
                  lpcdev->buf.cbInComBuf += nLength;
                  if( lpcdev->lpLogFile && !lpcdev->lpLogFile->fPaused )
                     Amfile_Write( lpcdev->lpLogFile->fhLog, lpcdev->buf.achComBuf + cbComBufPtr, nLength );
                  if( NULL != hwndCixTerminal && lpcdev->fDataLogging )
                     Terminal_WriteString( hwndCixTerminal, lpcdev->buf.achComBuf + cbComBufPtr, nLength, TRUE );
                  if( NULL != lpcdev->lpfCallback && lpcdev->fCallback )
                     {
                     DATAECHOSTRUCT des;
                     
                     des.lpBuffer = lpcdev->buf.achComBuf + cbComBufPtr;
                     des.cbBuffer = nLength;
                     des.lpComBuf = &lpcdev->buf;
                     lpcdev->lpfCallback( lpcdev, CCB_DATAECHO, lpcdev->dwAppData, (LPARAM)&des );
                     }
                  }
               }
         return( TRUE );
         }

      case WM_DESTROY: 
         {
            LPCOMMDEVICE lpcdev;

            lpcdev = (LPCOMMDEVICE)GetWindowLong( hwnd, 0 );
            ASSERT( lpcdev != 0L );
            if( lpcdev->fOpen )
            {
               if( fDebugMode )
               {                  
                  WriteToBlinkLog( "CommsClientWndProc: WM_DESTROY" );
               }
               if( NULL != lpcdev->lpfCallback && lpcdev->fCallback )
                  lpcdev->lpfCallback( lpcdev, CCB_DISCONNECTED, lpcdev->dwAppData, 0L );
               if( !IsWindow( hwnd ) )
                  return( 0L );
            }
            SetWindowLong( hwnd, 0, 0L );
            break;
         }

      case WM_SOCKMSG: {
         LPCOMMDEVICE lpcdev;

         if( 0L != ( lpcdev = (LPCOMMDEVICE)GetWindowLong( hwnd, 0 ) ) )
            if( lpcdev->sock != INVALID_SOCKET )
               switch( WSAGETSELECTEVENT( lParam ) )
                  {
                  case FD_CLOSE:
                     if( fDebugMode )
                     {                  
                        WriteToBlinkLog( "CommsClientWndProc: FD_CLOSE" );
                     }
                     DestroyWindow( hwnd );
                     return( 0L );
   
                  case FD_READ:
                     if( lpcdev->fEntry )
                        lpcdev->fNeedKickStart = TRUE;
                     else if( !ReadCommSocket( lpcdev ) )
                        if( WSAEINPROGRESS == Pal_WSAGetLastError() )
                           PostMessage( lpcdev->hwnd, WM_SOCKMSG, 0, WSAMAKESELECTREPLY( FD_READ, 0 ) );
                     return( 0L );
                  }
         return( 0L );
         }
      }
   return( DefWindowProc( hwnd, message, wParam, lParam ) );
}

/* This function implements the comms watcher thread. It runs in
 * the background waiting for data and reads the data into a queue.
 */
DWORD FAR PASCAL CommWatchProc( LPSTR lpData )
{
   LPCOMMDEVICE lpcdev;
   COMMTIMEOUTS ct;
   DCB dcb;
   BOOL fDataPending;
   COMSTAT cStat;
   DWORD dwError;

   /* Get the comm device data structure.
    */
   lpcdev = (LPCOMMDEVICE)lpData;
   Amcomm_DebugTAPIMessage( "Comm32 Thread Started" );

   /* Create and event which will be set by WM_COMMNOTIFY after it
    * has been processed. This prevents us from sending more than
    * one WM_COMMNOTIFY message at once.
    */
   Amcomm_DebugTAPIMessage( "Creating event notification" );
   memset( &lpcdev->osCommEvent, 0, sizeof( OVERLAPPED ) );
   lpcdev->osCommEvent.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
   if( lpcdev->osCommEvent.hEvent == NULL )
      {
      Amcomm_DebugTAPIMessage( "Failed to create event notification!" );
      return ( FALSE );
      }

   /* Set the comm timeout.
    */
   Amcomm_DebugTAPIMessage( "Setting comm timeouts" );
   memset( &ct, 0, sizeof(ct) );
   ct.ReadIntervalTimeout = MAXDWORD;
   SetCommTimeouts( lpcdev->hComm, &ct );

   /* fAbortOnError is the only DCB dependancy in TapiComm.
    * Can't guarentee that the SP will set this to what we expect.
    */
   Amcomm_DebugTAPIMessage( "Fixing DCB" );
   GetCommState( lpcdev->hComm, &dcb );
   Amcomm_DebugTAPIMessage( "fAbortOnError was %u, is now %u", dcb.fAbortOnError, FALSE );
   Amcomm_DebugTAPIMessage( "fOutxCtsFlow was %u, is now %u", dcb.fOutxCtsFlow, 1 );
   Amcomm_DebugTAPIMessage( "fRtsControl was %u, is now %u", dcb.fOutxCtsFlow, RTS_CONTROL_HANDSHAKE );
   Amcomm_DebugTAPIMessage( "fInX was %u, is now %u", dcb.fInX, FALSE );
   Amcomm_DebugTAPIMessage( "fOutX was %u, is now %u", dcb.fOutX, FALSE );
   Amcomm_DebugTAPIMessage( "XonChar was %u, is now %u", dcb.XonChar, XON );
   Amcomm_DebugTAPIMessage( "XoffChar was %u, is now %u", dcb.XoffChar, XOFF );
   Amcomm_DebugTAPIMessage( "XonLim was %u, is now %u", dcb.XonLim, 38 );
   Amcomm_DebugTAPIMessage( "XoffLim was %u, is now %u", dcb.XoffLim, 38 );
   Amcomm_DebugTAPIMessage( "fBinary was %u, is now %u", dcb.fBinary, 1 );
   Amcomm_DebugTAPIMessage( "fNull was %u, is now %u", dcb.fNull, 0 );
   dcb.fAbortOnError = FALSE;
   dcb.fOutxCtsFlow = 1;
   dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
   dcb.fInX = dcb.fOutX = FALSE;
   dcb.XonChar = XON;
   dcb.XoffChar = XOFF;
   dcb.XonLim = 38;
   dcb.XoffLim = 38;
   dcb.fBinary = 1;
   dcb.fNull = 0;
   SetCommState( lpcdev->hComm, &dcb );

   /* Allocate a queue.
    */
   Amcomm_DebugTAPIMessage( "Creating internal comms queue" );
   cbQueueSize = 32000;
   if( !fNewMemory( &chQueue, cbQueueSize ) )
      return( FALSE );
   nQueueHead = 0;
   nQueueTail = 0;

   /* I rather suspect that if there is any data in the input BEFORE we call
    * SetCommMask, we won't get the event? Right? This would explain those
    * people who are getting Ameol hanging after a TAPI connect until they
    * press the RETURN key. I'm having a spot of difficulty confirming this
    * so I've added this extra code to be on the safe side.
    */
   Amcomm_DebugTAPIMessage( "Clearing pending data errors" );
   fDataPending = FALSE;
   ClearCommError( lpcdev->hComm, &dwError, &cStat );
   if( cStat.cbInQue != 0 )
      {
      Amcomm_DebugTAPIMessage( "Data pending - will be picked up immediately" );
      fDataPending = TRUE;
      }

   /* Tell the comms driver that we want to know when the EV_RXCHAR
    * event is triggered.
    */
   if( !SetCommMask( lpcdev->hComm, EV_RXCHAR ) )
      {
      Amcomm_DebugTAPIMessage( "Failed to set EV_RXCHAR comm mask" );
      return( FALSE );
      }

   /* Keep count of overrun errors.
    */
   cOverruns = 0;

   /* Now loop forever or until the connection is closed.
    */
   Amcomm_DebugTAPIMessage( "Comm32 watch loop started" );
   while( lpcdev->fConnected )
      {
      /* Skip wait if more data waiting to be read (because of a full
       * queue). I'm pretty sure that if we don't do this, then we won't
       * get a EV_RXCHAR event for the unread data.
       */
      if( !fDataPending )
         {
         DWORD dwEvtMask;

         dwEvtMask = 0;
         if( !WaitCommEvent( lpcdev->hComm, &dwEvtMask, &lpcdev->osCommEvent ) )
            if( GetLastError() == ERROR_IO_PENDING )
               {
               DWORD dwTransfer;
               
               /* This error is to be expected. It means that the
                * event is triggered and waiting. We won't come back
                * from GetOverlappedResult until the event occurs.
                */
               GetOverlappedResult( lpcdev->hComm, &lpcdev->osCommEvent, &dwTransfer, TRUE );
               }
         if( ( dwEvtMask & EV_RXCHAR ) == EV_RXCHAR )
            fDataPending = TRUE;
         }

      /* Test whether the event was the one we wanted. If so,
       * wait for any pending WM_COMMNOTIFY to finish before posting
       * another.
       */
      if( fDataPending )
         {
         DWORD cbBytesInQueue;
         DWORD cBytesRead;

         /* How many bytes in queue?
          */
         fDataPending = FALSE;
         cbBytesInQueue = cbQueueSize - nQueueHead;
         if( 0 == cbBytesInQueue )
            {
            cbBytesInQueue = nQueueTail;
            nQueueHead = 0;
            }

         /* How much to read?
          */
         ClearCommError( lpcdev->hComm, &dwError, &cStat );
         if( dwError & CE_OVERRUN )
            ++cOverruns;

         /* Space in queue and data to transfer.
          */
         if( cbBytesInQueue && cStat.cbInQue )
            {
            cbBytesInQueue = min( cStat.cbInQue, cbBytesInQueue );
            if( !ReadFile( lpcdev->hComm, chQueue + nQueueHead, cbBytesInQueue, &cBytesRead, &lpcdev->overlappedRead ) )
               {
               cBytesRead = 0;
               if( GetLastError() == ERROR_IO_PENDING )
                  {
                  if( WaitForSingleObject( lpcdev->overlappedRead.hEvent, 1000 * lpcdev->lpcd->nTimeout ) )
                     cBytesRead = 0;
                  else
                     {
                     GetOverlappedResult( lpcdev->hComm, &lpcdev->overlappedRead, &cBytesRead, FALSE );
                     lpcdev->overlappedRead.Offset = 0;
                     lpcdev->overlappedRead.OffsetHigh = 0;
                     }
                  }
               else
                  {
                  /* We got an error, so clear the error
                   */
                  ClearCommError( lpcdev->hComm, &dwError, &cStat );
                  }
               }
            if( cBytesRead )
               nQueueHead += cBytesRead;
            if( NULL != lpcdev->lpfCallback && lpcdev->fCallback )
               SendMessage( lpcdev->hwnd, WM_COMMNOTIFY, (WPARAM)lpcdev->hComm, MAKELONG(CN_EVENT, 0) );
            if( cBytesRead < cStat.cbInQue )
               fDataPending = TRUE;
            }
         ResetEvent( lpcdev->osCommEvent.hEvent );
         }
      }
   Amcomm_DebugTAPIMessage( "Comm32 watch loop finished" );

   /* Destroy the queue.
    */
   Amcomm_DebugTAPIMessage( "Deleting internal comm queue" );
   FreeMemory( &chQueue );

   /* Close the event handle and kill the thread.
    */
   Amcomm_DebugTAPIMessage( "Closing event handle" );
   CloseHandle( lpcdev->osCommEvent.hEvent );
   lpcdev->dwThreadID = 0;
   
#ifdef USE_CLIB_THREAD_API
   Amcomm_DebugTAPIMessage( "Ending thread" );
   _endthreadex( TRUE );
#endif
   Amcomm_DebugTAPIMessage( "Comm32 Thread Stopped" );
   return( TRUE );
}

/* This function reads up to cbUsrBufMax bytes from the comm queue.
 */
BOOL FASTCALL Amcomm_ReadFromQueue32( char * lpUsrBuf, int cbUsrBufMax, int * pcbUsrBuf )
{
   *pcbUsrBuf = 0;
   if( NULL == chQueue )
      return( FALSE );
   while( nQueueTail != nQueueHead && *pcbUsrBuf < cbUsrBufMax )
      {
      *lpUsrBuf++ = chQueue[ nQueueTail++ ];
      if( nQueueTail == cbQueueSize )
         nQueueTail = 0;
      ++*pcbUsrBuf;
      }
   *lpUsrBuf = '\0';
   return( *pcbUsrBuf > 0 );
}

/* This function returns whether or not the CTS
 * signal is active.
 */
BOOL WINAPI EXPORT Amcomm_IsCTSActive( LPCOMMDEVICE lpcdev )
{
   if( lpcdev->lpcd->wProtocol & PROTOCOL_SERIAL )
      {
      DWORD dwModemStat;

      GetCommModemStatus( lpcdev->hComm, &dwModemStat );
      return( dwModemStat & MS_CTS_ON );
      }
   return( FALSE );
}

/* This function returns whether or not the carrier
 * signal is active.
 */
BOOL WINAPI EXPORT Amcomm_IsCarrierActive( LPCOMMDEVICE lpcdev )
{
   if( lpcdev->lpcd->wProtocol & PROTOCOL_SERIAL )
      {
      DWORD dwModemStat;

      GetCommModemStatus( lpcdev->hComm, &dwModemStat );
      return( dwModemStat & MS_RLSD_ON );
      }
   return( TRUE );
}

/* This function sets the requested flow control.
 */
BOOL EXPORT WINAPI Amcomm_FlowControl( LPCOMMDEVICE lpcdev, UINT wFlowCtrl )
{
   DCB dcb;

   ASSERT( lpcdev->lpcd->wProtocol & PROTOCOL_SERIAL );

   /* Get the current DCB and set the flow control
    * options as appropriate.
    */
   GetCommState( lpcdev->hComm, &dcb );
   dcb.fOutxDsrFlow = (wFlowCtrl & USE_DSRDTR) ? TRUE : FALSE;
   dcb.fOutxCtsFlow = (wFlowCtrl & USE_RTSCTS) ? TRUE : FALSE;
   dcb.fInX = dcb.fOutX = (wFlowCtrl & USE_XONXOFF) ? TRUE : FALSE;;

   /* For each defined flow control, set their
    * associated parameters.
    */
   if( wFlowCtrl & USE_XONXOFF )
      {
      dcb.XonChar = ASCII_XON;
      dcb.XoffChar = ASCII_XOFF;
      dcb.XonLim = 256;
      dcb.XoffLim = 256;
      }
   return( SetCommState( lpcdev->hComm, &dcb ) );
}

/* This function clears the output serial buffer.
 */
void EXPORT WINAPI Amcomm_ClearOutbound( LPCOMMDEVICE lpcdev )
{
   if( lpcdev->lpcd->wProtocol & PROTOCOL_SERIAL )
      PurgeComm( lpcdev->hComm, PURGE_TXCLEAR|PURGE_TXABORT );
}

/* This function clears the input serial buffer.
 */
void EXPORT WINAPI Amcomm_ClearInbound( LPCOMMDEVICE lpcdev )
{
   if( lpcdev->lpcd->wProtocol & PROTOCOL_SERIAL )
      PurgeComm( lpcdev->hComm, PURGE_RXCLEAR|PURGE_RXABORT );
   lpcdev->buf.cbComBuf = lpcdev->buf.cbInComBuf = 0;
}

/* Fill a combo listbox with a list of TAPI modems given
 * the handle of an existing TAPI connection.
 */
void FASTCALL Amcomm_FillDevices( HLINEAPP hTAPI, HWND hwnd, int wID )
{
   LINEDEVCAPS * pldc;
   LPSTR lpszLineName;
   DWORD dwDeviceID;
   HWND hwndCombo;
   int index;

   /* Initialise.
    */
   pldc = NULL;
   index = CB_ERR;
   hwndCombo = GetDlgItem( hwnd, wID );

   /* Make sure the control is empty.  If it isn't,
    * hold onto the currently selected ID and then reset it.
    */
   if( ComboBox_GetCount( hwndCombo ) )
      {
      index = ComboBox_GetCurSel( hwndCombo );
      ComboBox_ResetContent( hwndCombo );
      }

   /* Loop for every device. 
    */
   Amcomm_DebugTAPIMessage( "Number of lines found = %u", cLines );
   for( dwDeviceID = 0; dwDeviceID < cLines; dwDeviceID++ )
      {
      LINEEXTENSIONID extensions;
      DWORD dwVersionToUse;
      LONG lrc;

      /* Negotiate a version to use for this device.
       */
      lrc = flineNegotiateAPIVersion( hTAPI, dwDeviceID, 0x00010003, 0x00010004, &dwVersionToUse, &extensions );
      if( lrc < 0 )
         continue;
      if( dwVersionToUse )
         {
         /* Get the device capabilities of this line.
          */
         pldc = GetLineDeviceCaps( hTAPI, dwDeviceID, dwVersionToUse );
         if( pldc )
            if( pldc->dwMediaModes & LINEMEDIAMODE_DATAMODEM )
               if( pldc->dwLineNameSize && pldc->dwLineNameOffset && pldc->dwStringFormat == STRINGFORMAT_ASCII )
                  {
                  /* This is the name of the device.
                   */
                  lpszLineName = ((char *)pldc) + pldc->dwLineNameOffset;
                  if( *lpszLineName )
                     {
                     lpszLineName[ pldc->dwLineNameSize - 1 ] = '\0';
                     ComboBox_AddString( hwndCombo, lpszLineName );
                     }
                  }
         if( pldc )
            free( pldc );
         }
      }
   if( index == CB_ERR )
      index = 0;
   ComboBox_SetCurSel( hwndCombo, index );
}

/* This function loads the TAPI library, gets the addresses of
 * various functions we're interested in and initialises TAPI
 * to get the line count.
 */
HLINEAPP FASTCALL Amcomm_LoadTAPI( LINECALLBACK lpfTapiCallback )
{
   DWORD dwTickCount;
   LONG lrc;

   if( 0 == cTapiCount++ )
      {
      if( NULL == hTapiLib )
         {
         if( ( hTapiLib = LoadLibrary( szTapiLib ) ) == NULL )
            return( INVALID_TAPI_HANDLE );
         (FARPROC)flineInitialize = GetProcAddress( hTapiLib, "lineInitialize" );
         (FARPROC)flineNegotiateAPIVersion = GetProcAddress( hTapiLib, "lineNegotiateAPIVersion" );
         (FARPROC)flineGetDevCaps = GetProcAddress( hTapiLib, "lineGetDevCaps" );
         (FARPROC)flineOpen = GetProcAddress( hTapiLib, "lineOpen" );
         (FARPROC)flineGetID = GetProcAddress( hTapiLib, "lineGetID" );
         (FARPROC)flineMakeCall = GetProcAddress( hTapiLib, "lineMakeCall" );
         (FARPROC)flineTranslateAddress = GetProcAddress( hTapiLib, "lineTranslateAddress" );
         (FARPROC)flineClose = GetProcAddress( hTapiLib, "lineClose" );
         (FARPROC)flineShutdown = GetProcAddress( hTapiLib, "lineShutdown" );
         (FARPROC)flineDrop = GetProcAddress( hTapiLib, "lineDrop" );
         (FARPROC)flineDeallocateCall = GetProcAddress( hTapiLib, "lineDeallocateCall" );
         (FARPROC)ftapiGetLocationInfo = GetProcAddress( hTapiLib, "tapiGetLocationInfo" );
         (FARPROC)flineConfigDialog = GetProcAddress( hTapiLib, "lineConfigDialog" );
         (FARPROC)flineGetTranslateCaps = GetProcAddress( hTapiLib, "lineGetTranslateCaps" );
         (FARPROC)flineSetCurrentLocation = GetProcAddress( hTapiLib, "lineSetCurrentLocation" );
         (FARPROC)flineGetCallInfo = GetProcAddress( hTapiLib, "lineGetCallInfo" );
         (FARPROC)flineTranslateDialog = GetProcAddress( hTapiLib, "lineTranslateDialog" );
         (FARPROC)flineGetCountry = GetProcAddress( hTapiLib, "lineGetCountry" );
         }

      /* Look for an unclosed TAPI handle.
       */
      hTAPI = (HLINEAPP)Amuser_GetPPLong( szSettings, "TAPIHandle", (long)NULL );
      if( INVALID_TAPI_HANDLE != hTAPI )
         {
         flineShutdown( hTAPI );
         Amuser_WritePPString( szSettings, "TAPIHandle", NULL );
         }

      /* First initialise the line.
       */
      dwTickCount = GetTickCount();
      if( NULL == lpfTapiCallback )
         lpfTapiCallback = Amcomm_TapiCallback;
      while( LINEERR_REINIT == ( lrc = flineInitialize( &hTAPI, hInst, lpfTapiCallback, "AMEOL2", &cLines ) ) )
         if( ( GetTickCount() - dwTickCount ) >= 5000 )
            return( INVALID_TAPI_HANDLE );
      if( lrc < 0 )
         return( INVALID_TAPI_HANDLE );

      /* Save the TAPI handle in case we crash and have to
       * close it later.
       */
      Amuser_WritePPLong( szSettings, "TAPIHandle", (long)hTAPI );
      }
   return( hTAPI );
}

/* Unload the TAPI library.
 */
void FASTCALL Amcomm_UnloadTAPI( HLINEAPP hTAPI )
{
   if( 0 == --cTapiCount )
      {
      Amcomm_DebugTAPIMessage( "Shutting down TAPI line" );
      flineShutdown( hTAPI );
      if( NULL != hTapiLib )
         {
         Amcomm_DebugTAPIMessage( "Unloading TAPI library" );
         FreeLibrary( hTapiLib );
         hTapiLib = NULL;
         hTAPI = INVALID_TAPI_HANDLE;
         }
      Amuser_WritePPString( szSettings, "TAPIHandle", NULL );
      }
}

/* This function fills the specified combo box with a list of
 * connection cards.
 */
void FASTCALL Amcomm_FillLocations( HLINEAPP hTAPI, HWND hwndCombo )
{
   LPLINELOCATIONENTRY lplle;
   LPLINETRANSLATECAPS pldc;
   LONG lrc;
   DWORD c;

   /* Get the structure.
    */
   pldc = (LINETRANSLATECAPS *)calloc( sizeof(LINETRANSLATECAPS), 1 );
   pldc->dwTotalSize = sizeof(LINETRANSLATECAPS);
   while( ( lrc = flineGetTranslateCaps( INVALID_TAPI_HANDLE, 0x00010004, pldc ) ) == 0 )
      if( pldc->dwNeededSize > pldc->dwTotalSize )
         {
         DWORD dwNeededSize;

         dwNeededSize = pldc->dwNeededSize;
         free( pldc );
         pldc = (LINETRANSLATECAPS *)calloc( dwNeededSize, 1 );
         pldc->dwTotalSize = dwNeededSize;
         }
      else
         break;

   /* Fill the combo box with the list of
    * locations.
    */
   lplle = (LPLINELOCATIONENTRY)( ((char *)pldc) + pldc->dwLocationListOffset );
   ComboBox_ResetContent( hwndCombo );
   for( c = 0; c < pldc->dwNumLocations; ++c )
      {
      char * lpLocName;

      lpLocName = ((char *)pldc) + lplle->dwLocationNameOffset;
      ComboBox_AddString( hwndCombo, lpLocName );
      ++lplle;
      }
   ComboBox_InsertString( hwndCombo, 0, "(Default)" );
   ComboBox_SetCurSel( hwndCombo, 0 );
}

/* This function fills a combo box with a list of country names
 * and sets the item data for each entry to the country code for
 * each name.
 */
void FASTCALL Amcomm_FillCountries( HWND hwndCombo )
{
   LPLINECOUNTRYENTRY lplce;
   LPLINECOUNTRYLIST pldc;
   LONG lrc;
   DWORD c;

   /* Get the structure.
    */
   pldc = (LINECOUNTRYLIST *)calloc( sizeof(LINECOUNTRYLIST), 1 );
   pldc->dwTotalSize = sizeof(LINECOUNTRYLIST);
   while( ( lrc = flineGetCountry( 0, 0x00010004, pldc ) ) == 0 )
      if( pldc->dwNeededSize > pldc->dwTotalSize )
         {
         DWORD dwNeededSize;

         dwNeededSize = pldc->dwNeededSize;
         free( pldc );
         pldc = (LINECOUNTRYLIST *)calloc( dwNeededSize, 1 );
         pldc->dwTotalSize = dwNeededSize;
         }
      else
         break;

   /* Fill the combo box.
    */
   lplce = (LPLINECOUNTRYENTRY)( ((char *)pldc) + pldc->dwCountryListOffset );
   for( c = 0; c < pldc->dwNumCountries; ++c )
      {
      char * lpszCountryName;
      int index;

      lpszCountryName = (char *)pldc + lplce->dwCountryNameOffset;
      wsprintf( lpTmpBuf, "%s (%u)", lpszCountryName, lplce->dwCountryCode );
      index = ComboBox_AddString( hwndCombo, lpTmpBuf );
      ComboBox_SetItemData( hwndCombo, index, lplce->dwCountryCode );
      ++lplce;
      }
   ComboBox_SetCurSel( hwndCombo, 0 );
}

/* This function returns the ID for the specified named location, or
 * -1 if the location is invalid.
 */
DWORD FASTCALL Amcomm_GetLocationID( HLINEAPP hTAPI, char * pszLocation )
{
   LPLINELOCATIONENTRY lplle;
   LPLINETRANSLATECAPS pldc;
   LONG lrc;
   DWORD c;

   /* Get the structure.
    */
   pldc = (LINETRANSLATECAPS *)calloc( sizeof(LINETRANSLATECAPS), 1 );
   pldc->dwTotalSize = sizeof(LINETRANSLATECAPS);
   while( ( lrc = flineGetTranslateCaps( INVALID_TAPI_HANDLE, 0x00010004, pldc ) ) == 0 )
      if( pldc->dwNeededSize > pldc->dwTotalSize )
         {
         DWORD dwNeededSize;

         dwNeededSize = pldc->dwNeededSize;
         free( pldc );
         pldc = (LINETRANSLATECAPS *)calloc( dwNeededSize, 1 );
         pldc->dwTotalSize = dwNeededSize;
         }
      else
         break;

   /* Fill the combo box with the list of
    * locations.
    */
   lplle = (LPLINELOCATIONENTRY)( ((char *)pldc) + pldc->dwLocationListOffset );
   for( c = 0; c < pldc->dwNumLocations; ++c )
      {
      char * lpLocName;

      lpLocName = ((char *)pldc) + lplle->dwLocationNameOffset;
      if( strcmp( lpLocName, pszLocation ) == 0 )
         return( lplle->dwPermanentLocationID );
      ++lplle;
      }
   return( (DWORD)-1 );
}

/* This function returns the ID for the specified named location, or
 * -1 if the location is invalid.
 */
DWORD FASTCALL Amcomm_GetDefaultAreaCode( HLINEAPP hTAPI )
{
   LPLINELOCATIONENTRY lplle;
   LPLINETRANSLATECAPS pldc;
   LONG lrc;
   DWORD c;

   /* Get the structure.
    */
   pldc = (LINETRANSLATECAPS *)calloc( sizeof(LINETRANSLATECAPS), 1 );
   pldc->dwTotalSize = sizeof(LINETRANSLATECAPS);
   while( ( lrc = flineGetTranslateCaps( INVALID_TAPI_HANDLE, 0x00010004, pldc ) ) == 0 )
      if( pldc->dwNeededSize > pldc->dwTotalSize )
         {
         DWORD dwNeededSize;

         dwNeededSize = pldc->dwNeededSize;
         free( pldc );
         pldc = (LINETRANSLATECAPS *)calloc( dwNeededSize, 1 );
         pldc->dwTotalSize = dwNeededSize;
         }
      else
         break;

   /* Fill the combo box with the list of
    * locations.
    */
   lplle = (LPLINELOCATIONENTRY)( ((char *)pldc) + pldc->dwLocationListOffset );
   for( c = 0; c < pldc->dwNumLocations; ++c )
      {
      if( lplle->dwPermanentLocationID == pldc->dwCurrentLocationID )
         {
         char * lpCityCode;

         lpCityCode = ((char *)pldc) + lplle->dwCityCodeOffset;
         return( atoi( lpCityCode ) );
         }
      ++lplle;
      }
   return( (DWORD)-1 );
}

/* This function displays the line config dialog for the
 * specified modem.
 */
BOOL FASTCALL Amcomm_LineConfigDialog( HLINEAPP hTAPI, HWND hwnd, char * pszModemName )
{
   DWORD dwLineToUse;

   /* Get the line for the specified modem.
    */
   if( !Amcomm_GetDeviceLine( hTAPI, pszModemName, &dwLineToUse ) )
      return( FALSE );

   /* Call the lineConfig dialog.
    */
   flineConfigDialog( dwLineToUse, hwnd, NULL );
   return( TRUE );
}

/* This function displays the location configuration
 * dialog.
 */
BOOL FASTCALL Amcomm_LocationConfigDialog( HLINEAPP hTAPI, HWND hwnd, char * pszModemName, char * pszPhoneNumber )
{
   DWORD dwLineToUse;

   /* Get the line for the specified modem.
    */
   if( !Amcomm_GetDeviceLine( hTAPI, pszModemName, &dwLineToUse ) )
      return( FALSE );

   /* Call the lineTranslate dialog.
    */
   if( '\0' == *pszPhoneNumber )
      pszPhoneNumber = NULL;
   flineTranslateDialog( hTAPI, dwLineToUse, 0x00010004, hwnd, pszPhoneNumber );
   return( TRUE );
}

/* This function returns the line ID for the specified TAPI
 * device.
 */
BOOL FASTCALL Amcomm_GetDeviceLine( HLINEAPP hTAPI, char * pszModemName, DWORD * pdwLineToUse )
{
   BOOL fGotDataModem;
   LONG lrc;
   DWORD i;

   /* Test each line to find one capable of supporting a data/modem
    * device.
    */
   fGotDataModem = FALSE;
   for( i = 0; i < cLines; ++i )
      {
      LINEEXTENSIONID extensions;
      DWORD dwVersionToUse;
      LINEDEVCAPS * pldc;

      /* Negotiate an API for the line
       */
      lrc = flineNegotiateAPIVersion( hTAPI, i, 0x00010003, 0x00010004, &dwVersionToUse, &extensions );
      if( lrc < 0 )
         return( FALSE );
      pldc = GetLineDeviceCaps( hTAPI, i, dwVersionToUse );
      if( NULL != pldc )
         if( pldc->dwMediaModes & LINEMEDIAMODE_DATAMODEM )
            {
            /* Compare this line with the configured one. If found, set
             * fGotDataModem and breakout now.
             */
            if( pldc->dwLineNameSize && pldc->dwLineNameOffset && pldc->dwStringFormat == STRINGFORMAT_ASCII )
               {
               char * lpszLineName;

               lpszLineName = ((char *)pldc) + pldc->dwLineNameOffset;
               if( *lpszLineName )
                  lpszLineName[ pldc->dwLineNameSize - 1 ] = '\0';
               if( strcmp( lpszLineName, pszModemName ) == 0 )
                  fGotDataModem = TRUE;
               }
            }
      free( pldc );
      if( fGotDataModem )
         break;
      }

   /* Not found a line? Fail.
    */
   if( !fGotDataModem )
      return( FALSE );
   *pdwLineToUse = i;
   return( TRUE );
}
