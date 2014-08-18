/* INTCOMMS.H - Low level comms API and definitions
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

#ifndef _H_INTCOMMS

/* Dial type constants.
 */
#define  DTYPE_PULSE       0
#define  DTYPE_TONE        1

#define  cbMaxComBuf       32767

#define  IPPORT_NNTP       119
#define  IPPORT_POP3       110
#define  IPPORT_FINGER     79

#ifdef WIN32
#include <ras.h>
#include <raserror.h>
#include <tapi.h>
#endif

#include "winsock.h"
#include "amcomms.h"

#ifdef WIN32
typedef HANDLE HCOMM;
#else
typedef int HCOMM;
#endif

// TAPI handle types changed recently to be a DWORD
// Zero is now used to denote an invalid handle.
#define INVALID_TAPI_HANDLE 0

/* Message broadcast to disconnect a conn card.
 */
#define  WM_DISCONNECT           WM_USER+0x110

#define  XON                  17
#define  XOFF                 19

#define  LEN_SCRIPT        120
#define  LEN_PHONE         80
#define  LEN_ADDRESS       128
#define  LEN_MERCURY       40

typedef struct tagBUFFER {
   char achComBuf[ cbMaxComBuf ];            /* News line buffer */
   int cbComBuf;                             /* Pointer into line buffer */
   int cbInComBuf;                           /* Size of line buffer so far */
} BUFFER, FAR * LPBUFFER;

typedef struct tagDATAECHOSTRUCT {
   LPSTR lpBuffer;
   UINT cbBuffer;
   LPBUFFER lpComBuf;
} DATAECHOSTRUCT;

typedef struct tagMODEMDESCRIPTOR {
   char szModemName[ 100 ];                  /* Name of current modem */
   char szInitialise[ 80 ];                  /* Modem initialisation string */
   char szReset[ 40 ];                       /* Modem reset string */
   char szDialTone[ 40 ];                    /* Tone dial prefix */
   char szDialPulse[ 40 ];                   /* Pulse dial prefix */
   char szHangup[ 40 ];                      /* Modem hang-up command */
   char szSuffix[ 40 ];                      /* Modem command suffix */
   char szEscape[ 40 ];                      /* Modem on-line escape sequence */
   char szModemAck[ 40 ];                    /* Modem acknowledge string */
   char szModemNack[ 40 ];                   /* Modem error string */
   char szConnectMessage[ 40 ];              /* Connect OK string */
   char szConnectFail[ 40 ];                 /* Connect failure string */
   char szLineBusy[ 40 ];                    /* Line busy error string */
} MODEMDESCRIPTOR, FAR * LPMODEMDESCRIPTOR;

typedef struct tagIPCOMM {
   WORD wPort;                               /* IP port number */
   char szAddress[ LEN_ADDRESS ];            /* Address */
#ifdef WIN32
   RASDATA rd;                               /* RAS connection data */
#endif
} IPCOMM, FAR * LPIPCOMM;

typedef struct tagSERIALCOMM {
   DWORD dwBaud;                             /* Baud rate */
   int wDialType;                            /* Dial type (tone or pulse) */
   int wRetry;                               /* Number of times to retry */
   BOOL fCheckCTS;                           /* TRUE if we check CTS */
   BOOL fCheckDCD;                           /* TRUE if we check DCD */
   BOOL fOnline;                             /* TRUE if we're online */
   char szPhone[ LEN_PHONE ];                /* Phone number */
   char szMercury[ LEN_MERCURY ];            /* Mercury PIN number */
   char szPort[ 8 ];                         /* Access port */
#ifdef WIN32
   char szAreaCode[ 8 ];                     /* TAPI area code */
   char szCountryCode[ 8 ];                  /* TAPI country code */
   char szLocation[ 64 ];                    /* Name of location to use */
#endif
   MODEMDESCRIPTOR md;                       /* Modem descriptor */
} SERIALCOMM, FAR * LPSERIALCOMM;

#define  PROTOCOL_MODEM       0x0001
#define  PROTOCOL_TELEPHONY   0x0002
#define  PROTOCOL_SERIAL      (PROTOCOL_MODEM|PROTOCOL_TELEPHONY)
#define  PROTOCOL_NETWORK     0x0004

typedef struct tagCOMMDESCRIPTOR {
   char szTitle[LEN_CONNCARD+1];             /* Name of this connection card */
   char szScript[ 120 ];                     /* Path to startup script */
   int nTimeout;                             /* Inter-byte timeout */
   WORD wProtocol;                           /* Communications protocol */
   LPSERIALCOMM lpsc;                        /* Pointer to serial comm descriptor */
   LPIPCOMM lpic;                            /* Pointer to IP comm descriptor */
} COMMDESCRIPTOR, FAR * LPCOMMDESCRIPTOR;

typedef struct tagLOGFILE {
   struct tagLOGFILE FAR * lpNext;           /* Pointer to next logfile */
   char szFileName[ _MAX_FNAME ];            /* Log file name */
   BOOL fPaused;                             /* TRUE if log file has been held */
   HNDFILE fhLog;                            /* Handle of the terminal log file */
} LOGFILE;

typedef LOGFILE FAR * LPLOGFILE;

typedef struct tagCOMMDEVICE {
   HWND hwnd;                                /* Handle of notification window */
   LPLOGFILE lpLogFile;                      /* Pointer to active log file */
   BUFFER buf;                               /* Buffer for data */
   SOCKET sock;                              /* For IP connection, socket handle */
   HCOMM hComm;                              /* Comms handle */
   DWORD dwAppData;                          /* Application data */
   struct {
      unsigned int fCallback:1;              /* TRUE if callback is active */
      unsigned int fAsyncComms:1;            /* TRUE if this connection uses async comms */
      unsigned int fDataLogging:1;           /* TRUE if we output to the terminal window */
      unsigned int fExpandLFtoCRLF:1;        /* TRUE if we do text translations */
      unsigned int fNeedKickStart:1;         /* Kickstart flag */
      unsigned int fOpen:1;                  /* TRUE if this descriptor is open */
      unsigned int fOwnDescriptors:1;        /* TRUE if we own the comm descriptor */
      unsigned int fEntry:1;                 /* Re-entrancy flag */
      unsigned int fSendInit:1;                 /* Send Telnet Init Codes 2.56.2052 FS#148 */
      };
   COMMCALLBACK lpfCallback;                 /* Callback function (NULL if none) */
   LPCOMMDESCRIPTOR lpcd;                    /* Pointer to associated comm descriptor */
#ifdef WIN32
   BOOL fConnected;                          /* TRUE if we're on-line */
   DWORD dwThreadID;                         /* Comm notification thread ID */
   HANDLE hCommThread;                       /* Comm notification thread */
   HANDLE hPostEvent;                        /* Post event handle */
   OVERLAPPED overlappedRead;                /* Overlapped write I/O flag */
   OVERLAPPED overlappedWrite;               /* Overlapped write I/O flag */
   OVERLAPPED osCommEvent;                   /* Overlapped write I/O flag */
   BOOL fUseTAPI;                            /* TRUE if we use TAPI */
   HLINE hLine;                              /* TAPI line handle */
   HLINEAPP hTAPI;                           /* TAPI handle */
   HCALL hCall;                              /* TAPI call handle */
   DWORD dwVersionToUse;                     /* TAPI version in use */
   DWORD dwLineToUse;                        /* TAPI line in use */
#endif
} COMMDEVICE, FAR * LPCOMMDEVICE;

BOOL FASTCALL SaveCommDescriptor( LPCOMMDESCRIPTOR );
BOOL FASTCALL LoadCommDescriptor( LPCOMMDESCRIPTOR );
BOOL FASTCALL CreateCommDescriptor( LPCOMMDESCRIPTOR );
void FASTCALL LoadModemDescriptor( MODEMDESCRIPTOR * );

BOOL FASTCALL EditConnectionCard( HWND, LPCOMMDESCRIPTOR );

SOCKET FASTCALL Pal_socket( int, int, int, LPRASDATA );
int FASTCALL Pal_closesocket( SOCKET );
int FASTCALL Pal_GetOpenSocketCount( void );
int FASTCALL Pal_ioctlsocket( SOCKET, long, u_long FAR * );
int FASTCALL Pal_recv( SOCKET, char FAR *, int, int );
int FASTCALL Pal_connect( SOCKET, const struct sockaddr FAR *, int );
int FASTCALL Pal_send( SOCKET, const char FAR *, int, int );
int FASTCALL Pal_WSAAsyncSelect( SOCKET, HWND, u_int, long );
struct hostent FAR * FASTCALL Pal_gethostbyname( const char FAR * );
int FASTCALL Pal_WSACancelBlockingCall( void );
u_short FASTCALL Pal_htons( u_short );
struct hostent FAR * FASTCALL Pal_gethostbyaddr( const char FAR *, int, int );
struct servent FAR * FASTCALL Pal_getservbyname( const char FAR *, const char FAR * );
BOOL FASTCALL Pal_WSAIsBlocking( void );
int FASTCALL Pal_IncrementSocketCount( LPRASDATA );
int FASTCALL Pal_DecrementSocketCount( void );
int FASTCALL Pal_WSAGetLastError( void );
FARPROC FASTCALL Pal_WSASetBlockingHook( FARPROC );

#ifdef WIN32
HANDLE FASTCALL Pal_GetRasHandle( void );

extern RASDATA rdDef;
extern char szRasLib[];
extern int nRasConnect;
extern DWORD dwRasError;
#endif

#define  NWS_ISBUSY                 0x8000
#define  NWS_IDLE                   0x0000

typedef BOOL (FASTCALL * FPPARSERPROC )( LPCOMMDEVICE, char * );

extern int nMailState;
extern int nNewsState;
extern int nSMTPState;
extern BOOL fLogSMTP;
extern BOOL fLogNNTP;
extern BOOL fLogPOP3;
extern BOOL fCanDoXover;
extern BOOL fCIXSyncInProgress;
extern LPCOMMDEVICE lpcdevNNTP;
extern LPCOMMDEVICE lpcdevMail;
extern LPCOMMDEVICE lpcdevSMTP;
extern LPCOMMDEVICE lpcdevCIX;
extern HWND hwndCixTerminal;
extern int wCommErr;

void FASTCALL ReadAndProcessSocket( LPCOMMDEVICE, LPBUFFER, FPPARSERPROC );
BOOL FASTCALL WriteSocketLine( LPCOMMDEVICE, LPSTR );
void FASTCALL CreateMessageId( char * );
BOOL FASTCALL ResolveHostNames( void );
LPSTR FASTCALL CreateMimeBoundary( void );
void FASTCALL UnresolveHostNames( void );
void FASTCALL ResetDisconnectTimeout( void );
void FASTCALL SuspendDisconnectTimeout( void );
void FASTCALL HandleDisconnectTimeout( void );
void FASTCALL ReportOnline( char * );

/* TAPI functions available to
 * external procedures.
 */
#ifdef WIN32
HLINEAPP FASTCALL Amcomm_LoadTAPI( LINECALLBACK );
void FASTCALL Amcomm_UnloadTAPI( HLINEAPP );
void FASTCALL Amcomm_FillDevices( HLINEAPP, HWND, int );
BOOL FASTCALL Amcomm_LineConfigDialog( HLINEAPP, HWND, char * );
BOOL FASTCALL Amcomm_LocationConfigDialog( HLINEAPP, HWND, char *, char * );
BOOL FASTCALL Amcomm_GetDeviceLine( HLINEAPP, char *, DWORD * );
void FASTCALL Amcomm_FillLocations( HLINEAPP, HWND );
DWORD FASTCALL Amcomm_GetLocationID( HLINEAPP, char * );
void FASTCALL Amcomm_FillCountries( HWND );
DWORD FASTCALL Amcomm_GetDefaultAreaCode( HLINEAPP );
#endif

/* RAS functions available to
 * external procedures.
 */
#ifdef WIN32
#define  RCS_ABORTED_DUE_TO_APP_QUIT      0
#define  RCS_ABORTED                      1
#define  RCS_CONNECTED                    2
#define  RCS_CONNECTING                   3
#define  RCS_ERROR                        4

BOOL FASTCALL ConnectViaRas( void );
#endif

/* COMMSHRD.C */
LRESULT EXPORT CALLBACK CommsClientWndProc( HWND, UINT, WPARAM, LPARAM );
LPCOMMDEVICE FASTCALL Amcomm_OpenSocket( LPCOMMDEVICE, IPCOMM FAR *, HWND );
LPCOMMDEVICE FASTCALL Amcomm_OpenModem( LPCOMMDEVICE, SERIALCOMM FAR *, HWND );
BOOL FASTCALL Amcomm_Dial( LPCOMMDEVICE, SERIALCOMM FAR *, HWND );
void FASTCALL Amcomm_TranslateString( LPSTR, BOOL );
void FASTCALL Amcomm_FastHangUp( LPCOMMDEVICE );
void FASTCALL Amcomm_SlowHangUp( LPCOMMDEVICE );
void FASTCALL Amcomm_DisconnectFromHost( LPCOMMDEVICE );
BOOL FASTCALL Amcomm_Put( LPCOMMDEVICE, char * );
void FASTCALL InsertMercuryPin( LPCOMMDEVICE, char * );
int FASTCALL ReadCommSocket( LPCOMMDEVICE );

/* RUNSCRPT.C */
BOOL FASTCALL ExecuteScript( LPCOMMDEVICE, LPSTR, BOOL );
BOOL FASTCALL CompileInline( HNDFILE, char * );
BOOL FASTCALL IsInlineToken( char * );

/* TERMINAL.C */
BOOL FASTCALL Terminal_AttachConnection( HWND, LPCOMMDEVICE );

#define  _H_INTCOMMS
#endif
