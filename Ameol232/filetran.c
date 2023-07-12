/* FILETRAN.C - File transfer protocol interface
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

#define  FILETRAN_INCLUDED

#include "main.h"
#include "palrc.h"
#include "resource.h"
#include "amlib.h"
#include "intcomms.h"
#include "transfer.h"
#include <string.h>
#include <dos.h>
#include <io.h>

typedef BOOL (WINAPI * AFTPSEND)( LPCOMMDEVICE, LPFFILETRANSFERSTATUS, HWND, LPSTR, BOOL );
typedef BOOL (WINAPI * AFTPRECEIVE)( LPCOMMDEVICE, LPFFILETRANSFERSTATUS, HWND, LPSTR, BOOL, WORD );
typedef BOOL (WINAPI * AFTPDESCRIBE)( LPSTR, int );
typedef BOOL (WINAPI * AFTPCOMMAND)( LPSTR, int );
typedef BOOL (WINAPI * AFTPCANCEL)( LPCOMMDEVICE );

typedef struct tagFTPROTOCOL {
   struct tagFTPROTOCOL * pftpNext;       /* Pointer to next protocol */
   char szName[ 16 ];                     /* Name of protocol */
   HINSTANCE hLib;                        /* Handle of library */
   AFTPSEND lpfSend;                      /* Pointer to Send function */
   AFTPRECEIVE lpfReceive;                /* Pointer to Receive function */
   AFTPDESCRIBE lpfDescribe;              /* Pointer to Describe function */
   AFTPCANCEL lpfCancel;                  /* Pointer to Cancel function */
   AFTPCOMMAND lpfCommand;                /* Pointer to Command function */
} FTPROTOCOL;

static FTPROTOCOL * pftpFirst = NULL;        /* First installed protocol */
static FTPROTOCOL * pftpLast = NULL;         /* Last installed protocol */

static BOOL fTransferActive = FALSE;         /* TRUE if a transfer is active */

/* This function scans the Ameol base directory and installs
 * all protocol files found.
 */
void FASTCALL InstallAllFileTransferProtocols( void )
{
   FINDDATA ft;
   register int n;
   HFIND r;
   UINT fuErrorMode;

   fuErrorMode = SetErrorMode( SEM_FAILCRITICALERRORS );
   for( n = r = Amuser_FindFirst( "*.afp", _A_NORMAL, &ft ); n != -1; n = Amuser_FindNext( r, &ft ) )
      {
      HINSTANCE hLib;

      wsprintf( lpPathBuf, "%s%s", pszAmeolDir, ft.name );
      if( ( hLib = LoadLibrary( lpPathBuf ) ) >= (HINSTANCE)32 )
         {
         FTPROTOCOL * pftpNew;

         /* File found, so allocate a new structure to store
          * details.
          */
         INITIALISE_PTR(pftpNew);
         if( fNewMemory( &pftpNew, sizeof(FTPROTOCOL ) ) )
            {
            if( NULL == pftpFirst )
               pftpFirst = pftpNew;
            else
               pftpLast->pftpNext = pftpNew;
            pftpNew->pftpNext = NULL;
            pftpNew->hLib = hLib;
            pftpNew->lpfSend = (AFTPSEND)GetProcAddress( hLib, "Aftp_Send" );
            pftpNew->lpfReceive = (AFTPRECEIVE)GetProcAddress( hLib, "Aftp_Receive" );
            pftpNew->lpfDescribe = (AFTPDESCRIBE)GetProcAddress( hLib, "Aftp_Describe" );
            pftpNew->lpfCancel = (AFTPCANCEL)GetProcAddress( hLib, "Aftp_Cancel" );
            pftpNew->lpfCommand = (AFTPCOMMAND)GetProcAddress( hLib, "Aftp_Commands" );
            pftpLast = pftpNew;

            /* If Describe entry point supported, call it for
             * the name of this protocol.
             */
            if( NULL != pftpNew->lpfDescribe )
               pftpNew->lpfDescribe( pftpNew->szName, sizeof(pftpNew->szName) );
            else
               {
               int c;

               /* Otherwise use the basename portion of the filename.
                */
               for( c = 0; ft.name[ c ] && ft.name[ c ] != '.'; ++c )
                  pftpNew->szName[ c ] = ft.name[ c ];
               pftpNew->szName[ c ] = '\0';
               }
            }
         }
      }
   Amuser_FindClose( r );
   SetErrorMode( fuErrorMode );
}

/* This function scans the Ameol base directory and installs
 * all protocol files found.
 */
void FASTCALL UninstallAllFileTransferProtocols( void )
{
   FTPROTOCOL * pftpNext;
   FTPROTOCOL * pftp;

   for( pftp = pftpFirst; pftp; pftp = pftpNext )
      {
      FreeLibrary( pftp->hLib );
      pftpNext = pftp->pftpNext;
      FreeMemory( &pftp );
      }
}

/* This function enumerates all installed protocols and their
 * handles into a combo box.
 */
BOOL FASTCALL EnumerateFileTransferProtocols( HWND hwndCombo )
{
   FTPROTOCOL * pftp;

   ComboBox_ResetContent( hwndCombo );
   for( pftp = pftpFirst; pftp; pftp = pftp->pftpNext )
      {
      int index;

      index = ComboBox_AddString( hwndCombo, pftp->szName );
      ComboBox_SetItemData( hwndCombo, index, pftp );
      }
   return( ComboBox_GetCount( hwndCombo ) > 0 );
}

/* This function returns a handle to the named protocol.
 */
FTPROTOCOL * FASTCALL GetFileTransferProtocolHandle( char * pszName )
{
   FTPROTOCOL * pftp;

   for( pftp = pftpFirst; pftp; pftp = pftp->pftpNext )
      if( _strcmpi( pftp->szName, pszName ) == 0 )
         return( pftp );
   return( NULL );
}

/* This function returns the command strings necessary to
 * configure the conferencing system to use this protocol.
 */
BOOL FASTCALL GetFileTransferProtocolCommands( FTPROTOCOL * pftp, LPSTR lpBuf, int cbMax )
{
   if( NULL != pftp && NULL != pftp->lpfCommand )
      return( pftp->lpfCommand( lpBuf, cbMax ) );
   return( 0 );
}

/* This function returns a flag that specifies whether or
 * not a file transfer is occurring.
 */
BOOL FASTCALL IsTransferActive( void )
{
   return( fTransferActive );
}

/* This function calls the interface to interrupt the current
 * file transfer.
 */
void FASTCALL CancelTransfer( FTPROTOCOL * pftp, LPCOMMDEVICE lpcdev )
{
   if( fTransferActive )
      pftp->lpfCancel( lpcdev );
}

/* This function calls the Send interface on the specified file
 * transfer protocol.
 */
BOOL FASTCALL SendFileViaProtocol( FTPROTOCOL * pftp, LPCOMMDEVICE lpcdev, LPFFILETRANSFERSTATUS lpffts, HWND hwnd, LPSTR lpszFileName, BOOL fAuto )
{
   BOOL fResult;

   fTransferActive = TRUE;
   fResult = pftp->lpfSend( lpcdev, lpffts, hwnd, lpszFileName, fAuto );
   fTransferActive = FALSE;
   return( fResult );
}

/* This function calls the Receive interface on the specified file
 * transfer protocol.
 */
BOOL FASTCALL ReceiveFileViaProtocol( FTPROTOCOL * pftp, LPCOMMDEVICE lpcdev, LPFFILETRANSFERSTATUS lpffts, HWND hwnd, LPSTR lpszPath, BOOL fAuto, WORD wDownloadFlags )
{
   BOOL fResult;

   fTransferActive = TRUE;
   fResult = pftp->lpfReceive( lpcdev, lpffts, hwnd, lpszPath, fAuto, wDownloadFlags );
   fTransferActive = FALSE;
   return( fResult );
}
