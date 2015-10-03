/* AMCOMMS.H - Communications API header file
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

#ifndef _INC_COMMS

#ifdef WIN32
#include <ras.h>
#include <raserror.h>
#include <tapi.h>
#endif

#define  LEN_CONNCARD         79

#define  USE_XONXOFF          1
#define  USE_RTSCTS           2
#define  USE_DSRDTR           3

/* Communications status codes.
 */
#define  WCE_NOERROR                0
#define  WCE_ABORT                  1
#define  WCE_TIMEOUT                2
#define  WCE_NOCONNECT              3
#define  WCE_SCRIPTERR              4
#define  WCE_NOSCRIPTFILE           5
#define  WCE_NOCARRIER              6
#define  WCE_CTSLOW                 7
#define  WCE_TERMINAL_INIT_ERROR    8
#define  WCE_BADLOGIN               9
#define  WCE_LINEBUSY               10
#define  WCE_NONICKNAME             11
#define  WCE_BUSY                   12

/* LPCOMMQUERYSTRUCT
 */
typedef struct tagCOMMQUERYSTRUCT {
   UINT cbStructSize;            /* Size of structure */
   char szCommDevice[ 60 ];      /* Communications device title */
} COMMQUERYSTRUCT, FAR * LPCOMMQUERYSTRUCT;

/* Callback constants.
 */
#define  CCB_DISCONNECTED     0
#define  CCB_DATAREADY        1
#define  CCB_STATUSTEXT       2
#define  CCB_DATAECHO         3

typedef struct tagCOMMDEVICE FAR * LPCOMMDEVICE;
typedef BOOL (FAR PASCAL * COMMCALLBACK)(LPCOMMDEVICE, int, DWORD, LPARAM);

#ifndef LPBOOL
typedef BOOL FAR * LPBOOL;
#endif

/* Remote Access Services data
 */
#ifndef WIN32
#define  LPRASDATA      void FAR *
#else
typedef struct tagRASDATA {
   BOOL fUseRAS;                                /* Connect using RAS */
   char szRASEntryName[ RAS_MaxEntryName+1 ];   /* RAS entry name */
   char szRASUserName[ UNLEN+1 ];               /* RAS user name */
   char szRASPassword[ PWLEN+1 ];               /* RAS password */
} RASDATA, FAR * LPRASDATA;
#endif

BOOL WINAPI Amcomm_Open( LPCOMMDEVICE FAR *, LPSTR, COMMCALLBACK, DWORD, char *, HWND, LPRASDATA, BOOL );
BOOL WINAPI Amcomm_OpenCard( HWND, LPCOMMDEVICE FAR *, LPVOID, COMMCALLBACK, DWORD, char *, HWND, BOOL );
void WINAPI Amcomm_Close( LPCOMMDEVICE );
BOOL WINAPI Amcomm_QueryCharReady( LPCOMMDEVICE );
UINT WINAPI Amcomm_ReadChar( LPCOMMDEVICE, char * );
UINT WINAPI Amcomm_ReadCharTimed( LPCOMMDEVICE, char *, long );
UINT WINAPI Amcomm_ReadString( LPCOMMDEVICE, LPSTR, UINT );
UINT WINAPI Amcomm_ReadStringTimed( LPCOMMDEVICE, LPSTR, UINT, long );
BOOL WINAPI Amcomm_WriteChar( LPCOMMDEVICE, char );
BOOL WINAPI Amcomm_WriteCharTimed( LPCOMMDEVICE, char, UINT );
BOOL WINAPI Amcomm_WriteString( LPCOMMDEVICE, LPSTR, UINT );
BOOL WINAPI Amcomm_WriteStringTimed( LPCOMMDEVICE, LPSTR, UINT, long );
BOOL WINAPI Amcomm_WriteData( LPCOMMDEVICE, LPSTR, UINT );
UINT WINAPI Amcomm_Query( LPCOMMDEVICE, LPCOMMQUERYSTRUCT );
BOOL WINAPI Amcomm_SetCallback( LPCOMMDEVICE, BOOL );
BOOL WINAPI Amcomm_SetDataLogging( LPCOMMDEVICE, BOOL );
BOOL WINAPI Amcomm_Delay( LPCOMMDEVICE, DWORD );
BOOL WINAPI Amcomm_IsCTSActive( LPCOMMDEVICE );
BOOL WINAPI Amcomm_IsCarrierActive( LPCOMMDEVICE );
BOOL WINAPI Amcomm_ReadCommBuffer( LPCOMMDEVICE );
BOOL WINAPI Amcomm_FlowControl( LPCOMMDEVICE, UINT );
void WINAPI Amcomm_ClearOutbound( LPCOMMDEVICE );
void WINAPI Amcomm_ClearInbound( LPCOMMDEVICE );
int WINAPI Amcomm_WaitForString( LPCOMMDEVICE, int, char *[], int );
BOOL WINAPI Amcomm_WaitFor( LPCOMMDEVICE, char );
BOOL WINAPI Amcomm_Get( LPCOMMDEVICE, char *, int );
BOOL WINAPI Amcomm_CheckCarrier( LPCOMMDEVICE );
void WINAPI Amcomm_Destroy( LPCOMMDEVICE );
BOOL WINAPI Amcomm_OpenLogFile( LPCOMMDEVICE, char * );
BOOL WINAPI Amcomm_CloseLogFile( LPCOMMDEVICE );
BOOL WINAPI Amcomm_ResumeLogFile( LPCOMMDEVICE );
BOOL WINAPI Amcomm_PauseLogFile( LPCOMMDEVICE );
char * WINAPI Amcomm_GetLastError( void );
int WINAPI Amcomm_GetCommStatus( void );
BOOL WINAPI Amcomm_GetBinary( LPCOMMDEVICE );

#ifdef WIN32
void FASTCALL Amcomm_CloseTAPI( LPCOMMDEVICE );
#endif

#define _INC_COMMS
#endif
