/* ZMODEM.C - Zmodem Send/Receive
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

#include <windows.h>
#include <windowsx.h>
#include <stdlib.h>
#include "amlib.h"
#include "amuser.h"
#include <sys\types.h>
#include <sys\stat.h>
#include <string.h>
#include <time.h>
#include <dos.h>
#include "io.h"
#include "amcomms.h"
#include "zmodem.h"
#include <stdio.h>
#include <stdarg.h>

#define  THIS_FILE   __FILE__

BOOL fDefDlgEx = FALSE;       /* DefDlg recursion flag trap */

#define  MAX_CPS     20

/* ZMODEM file transfer structure.
 */
typedef struct tagZFT {
   LPCOMMDEVICE lpcdev;                /* COMM device associated with this transmission */
   LPFFILETRANSFERSTATUS lpffts;       /* Pointer to status procedure */
   FILETRANSFERINFO fts;               /* Status information */
   char szTargetFname[ _MAX_FNAME ];   /* Target file basename */
   char szTargetDir[ _MAX_PATH ];      /* Directory to which file is being downloaded */
   HNDFILE fh;                         /* Open file handle */
   HNDFILE pFile;                      /* Debug file handle */
   BOOL fUseCRC32;                     /* Flag set TRUE if using 32-bit CRC */
   BOOL fExpandLFtoCRLF;               /* TRUE if we do LF translation */
   WORD wFlags;                        /* Download flags */
   int cCancels;                       /* Number of ZCAN or cancels so far */
   BYTE tryzhdrtype;
   BYTE zconv;
   char attn[ ZATTNLEN + 1 ];
   char rxhdr[ 4 ];                    /* Receive header var */
   char secbuf[ ZMAXBUFLEN ];
   int nCpsIndex;                      /* Current index into CpsIndexArray */
   int CpsSum;                         /* Accumulated cps rate over MAX_CPS */
   int CpsIndexArray[ MAX_CPS ];       /* Most recent MAX_CPS cps rates logged */
   int nItemsInCpsArray;               /* Number of items in the array */
   char txbuf[ ZMAXSPLEN ];
   char txhdr[ 4 ];                    /* Transmit header var */
   LPSTR current_file;
   DWORD dwStartTick;
   DWORD dwTotal;
   DWORD frcvsize;
   int current_block_size;
   int error_recovery;
   int garbage_count;
   int header_type;
   int return_status;
   int rxcount;
   int rxframeind;
   int rxtimeout;                      /* Receive timeout value */
   int rxtype;
   int rx_flags;
   long filestart;
   long ftime;
   long rxpos;                         /* File position received from Z_GetHeader */
   UINT rx_buffer_size;
   UINT rx_frame_output_count;
} ZFT, FAR * LPZFT;

BOOL fZmodemAbort;                  /* Zmodem abort flag (Abort button pressed) */
char lpTmpBuf[ 256 ];               /* General purpose buffer */
LPSTR lpName;
LPZFT FASTCALL ZModemCreateFTS( void );
int FASTCALL OpenLink( LPZFT );
int FASTCALL OpenNextFile( LPZFT );
int FASTCALL CloseLink( LPZFT );
void FASTCALL _XferCleanup( LPCOMMDEVICE );
BOOL FASTCALL _XferCheckCarrier( LPZFT );
BOOL FASTCALL _XferAbortKeyPressed( LPZFT );
int FASTCALL _XferOpenFile( LPZFT );
int FASTCALL BackChannelActivity( LPZFT );
void FASTCALL SendDataFrame( LPZFT );
BOOL FASTCALL ReadBlock( LPZFT );
int FASTCALL StartFileTransfer( LPZFT );
void FASTCALL SendFileData( LPZFT );
int FASTCALL ReadFrameHeader( LPZFT, int  );
int FASTCALL GetPad( LPZFT );
int FASTCALL GetHeaderType( LPZFT );
int FASTCALL ReadHexFrameHeaderData( LPZFT );
int FASTCALL ReadBinaryHeaderData16( LPZFT );
int FASTCALL ReadBinaryHeaderData32( LPZFT );
int FASTCALL ReadHexByte( LPZFT );
int FASTCALL GetCookedChar( LPZFT );
void FASTCALL SendBinaryHeader( LPZFT, char, char * );
int FASTCALL SendHexHeader( LPZFT, char, BYTE * );
void FASTCALL SendBinaryPacket( LPZFT, int, char );
int FASTCALL EncodeCharacter( char *, BYTE);
int FASTCALL ReadBinaryByte( LPZFT );
WORD FASTCALL CalculateBlockCRC16( UINT, WORD, void * );
DWORD FASTCALL CalculateBlockCRC32( UINT, DWORD, void * );
int FASTCALL Z_GetByte( LPZFT, int );
int FASTCALL Z_qk_read( LPZFT );
int FASTCALL Z_TimedRead( LPZFT );
void FASTCALL Z_SendCan( LPZFT );
void FASTCALL Z_PutString( LPZFT, char * );
long FASTCALL Z_PullLongFromHeader( char * );
int FASTCALL Z_GetZDL( LPZFT );
int FASTCALL Z_GetHex( LPZFT );
int FASTCALL Z_GetHexHeader( LPZFT, char * );
int FASTCALL Z_GetBinaryHeader( LPZFT, char * );
int FASTCALL Z_GetBinaryHeader32( LPZFT, char * );
int FASTCALL Z_GetHeader( LPZFT, char *, BOOL );
void FASTCALL Z_PutLongIntoHeader( LPZFT, long );

int FASTCALL RZ_ReceiveDa32( LPZFT, LPSTR, int );
int FASTCALL RZ_ReceiveData( LPZFT, LPSTR, int );
void FASTCALL RZ_AckBibi( LPZFT );
int FASTCALL RZ_InitReceiver( LPZFT, BOOL );
int FASTCALL RZ_GetHeader( LPZFT );
int FASTCALL RZ_SaveToDisk( LPZFT, LONG * );
int FASTCALL RZ_ReceiveFile( LPZFT );
int FASTCALL RZ_ReceiveBatch( LPZFT );

void FASTCALL ZWriteFrame( LPZFT, int );
void FASTCALL ZMessage( LPZFT, char * );
long FASTCALL Z_FromUnixDate( char * );
WORD FASTCALL CalculateCharacterCRC16( WORD, BYTE );
WORD FASTCALL AltCalculateCharacterCRC16( WORD, BYTE );
DWORD FASTCALL CalculateCharacterCRC32( DWORD, BYTE );
DWORD FASTCALL AltCalculateCharacterCRC32( DWORD, BYTE );
void FASTCALL Z_SendBreak( void );
void FASTCALL Z_ShowLoc( LPZFT, long );
void FASTCALL SaveZDlgPos( LPZFT );
int FASTCALL ZSendStatus( LPZFT, int );
WORD FASTCALL FixupYear( WORD );
void FASTCALL GoToSleep( DWORD );
BOOL FASTCALL IsWild( char * );

#ifdef ENABLE_DEBUGGING_INFO
void FASTCALL OpenDebugFile( LPZFT );
void FASTCALL CloseDebugFile( LPZFT );
void FASTCALL WriteDebugTxhdrDump( LPZFT, char * );
void CDECL WriteDebugMessage( LPZFT, char *, ... );
#else
#define OpenDebugFile( z );
#define CloseDebugFile( z );
#define WriteDebugTxhdrDump( z, s );
#endif
#define  LEN_TEMPBUF          512


BOOL WINAPI Ameol2_MakeLocalFileName( HWND, LPSTR, LPSTR, BOOL );

/* swp_crctab calculated by Mark G. Mendel, Network Systems Corporation
 */
WORD swp_crctab[ 256 ] = {
   0x0000,  0x1021,  0x2042,  0x3063,  0x4084,  0x50a5,  0x60c6,  0x70e7,
   0x8108,  0x9129,  0xa14a,  0xb16b,  0xc18c,  0xd1ad,  0xe1ce,  0xf1ef,
   0x1231,  0x0210,  0x3273,  0x2252,  0x52b5,  0x4294,  0x72f7,  0x62d6,
   0x9339,  0x8318,  0xb37b,  0xa35a,  0xd3bd,  0xc39c,  0xf3ff,  0xe3de,
   0x2462,  0x3443,  0x0420,  0x1401,  0x64e6,  0x74c7,  0x44a4,  0x5485,
   0xa56a,  0xb54b,  0x8528,  0x9509,  0xe5ee,  0xf5cf,  0xc5ac,  0xd58d,
   0x3653,  0x2672,  0x1611,  0x0630,  0x76d7,  0x66f6,  0x5695,  0x46b4,
   0xb75b,  0xa77a,  0x9719,  0x8738,  0xf7df,  0xe7fe,  0xd79d,  0xc7bc,
   0x48c4,  0x58e5,  0x6886,  0x78a7,  0x0840,  0x1861,  0x2802,  0x3823,
   0xc9cc,  0xd9ed,  0xe98e,  0xf9af,  0x8948,  0x9969,  0xa90a,  0xb92b,
   0x5af5,  0x4ad4,  0x7ab7,  0x6a96,  0x1a71,  0x0a50,  0x3a33,  0x2a12,
   0xdbfd,  0xcbdc,  0xfbbf,  0xeb9e,  0x9b79,  0x8b58,  0xbb3b,  0xab1a,
   0x6ca6,  0x7c87,  0x4ce4,  0x5cc5,  0x2c22,  0x3c03,  0x0c60,  0x1c41,
   0xedae,  0xfd8f,  0xcdec,  0xddcd,  0xad2a,  0xbd0b,  0x8d68,  0x9d49,
   0x7e97,  0x6eb6,  0x5ed5,  0x4ef4,  0x3e13,  0x2e32,  0x1e51,  0x0e70,
   0xff9f,  0xefbe,  0xdfdd,  0xcffc,  0xbf1b,  0xaf3a,  0x9f59,  0x8f78,
   0x9188,  0x81a9,  0xb1ca,  0xa1eb,  0xd10c,  0xc12d,  0xf14e,  0xe16f,
   0x1080,  0x00a1,  0x30c2,  0x20e3,  0x5004,  0x4025,  0x7046,  0x6067,
   0x83b9,  0x9398,  0xa3fb,  0xb3da,  0xc33d,  0xd31c,  0xe37f,  0xf35e,
   0x02b1,  0x1290,  0x22f3,  0x32d2,  0x4235,  0x5214,  0x6277,  0x7256,
   0xb5ea,  0xa5cb,  0x95a8,  0x8589,  0xf56e,  0xe54f,  0xd52c,  0xc50d,
   0x34e2,  0x24c3,  0x14a0,  0x0481,  0x7466,  0x6447,  0x5424,  0x4405,
   0xa7db,  0xb7fa,  0x8799,  0x97b8,  0xe75f,  0xf77e,  0xc71d,  0xd73c,
   0x26d3,  0x36f2,  0x0691,  0x16b0,  0x6657,  0x7676,  0x4615,  0x5634,
   0xd94c,  0xc96d,  0xf90e,  0xe92f,  0x99c8,  0x89e9,  0xb98a,  0xa9ab,
   0x5844,  0x4865,  0x7806,  0x6827,  0x18c0,  0x08e1,  0x3882,  0x28a3,
   0xcb7d,  0xdb5c,  0xeb3f,  0xfb1e,  0x8bf9,  0x9bd8,  0xabbb,  0xbb9a,
   0x4a75,  0x5a54,  0x6a37,  0x7a16,  0x0af1,  0x1ad0,  0x2ab3,  0x3a92,
   0xfd2e,  0xed0f,  0xdd6c,  0xcd4d,  0xbdaa,  0xad8b,  0x9de8,  0x8dc9,
   0x7c26,  0x6c07,  0x5c64,  0x4c45,  0x3ca2,  0x2c83,  0x1ce0,  0x0cc1,
   0xef1f,  0xff3e,  0xcf5d,  0xdf7c,  0xaf9b,  0xbfba,  0x8fd9,  0x9ff8,
   0x6e17,  0x7e36,  0x4e55,  0x5e74,  0x2e93,  0x3eb2,  0x0ed1,  0x1ef0
   };

long swp_crc_32_tab[ 256 ] = {
   0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
   0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
   0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
   0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
   0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
   0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
   0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
   0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
   0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
   0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
   0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
   0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
   0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
   0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
   0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
   0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
   0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
   0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
   0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
   0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
   0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
   0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
   0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
   0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
   0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
   0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
   0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
   0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
   0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
   0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
   0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
   0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
   };

WORD FASTCALL AltCalculateCharacterCRC16( WORD crc, BYTE c )
{
   return( swp_crctab[ ( crc >> 8 ) & 0xFF ] ^ ( crc << 8 ) ^ c );
}

WORD FASTCALL CalculateCharacterCRC16( WORD crc, BYTE c )
{
   return( ( crc << 8 ) ^ swp_crctab[ ( crc >> 8 ) ^ c ] );
}

DWORD FASTCALL AltCalculateCharacterCRC32( DWORD crc, BYTE c )
{
   return( swp_crc_32_tab[ (BYTE)( crc ^ (long)c ) ] ^ ( ( crc >> 8 ) & 0x00FFFFFF ) );
}

DWORD FASTCALL CalculateCharacterCRC32( DWORD crc, BYTE c )
{
   DWORD temp1;
   DWORD temp2;

   temp1 = ( crc >> 8 ) & 0x00FFFFFFL;
   temp2 = swp_crc_32_tab[ ( (WORD)crc ^ c ) & 0xff ];
   return( temp1 ^ temp2  );
}

WORD FASTCALL CalculateBlockCRC16( UINT count, WORD crc, void * buffer )
{
   BYTE * p = buffer;

   while( count-- != 0 )
      crc = ( crc << 8 ) ^ swp_crctab[ ( crc >> 8 ) ^ *p++ ];
   return( crc );
}

DWORD FASTCALL CalculateBlockCRC32( UINT count, DWORD crc, void * buffer )
{
   BYTE * p = buffer;
   DWORD temp1;
   DWORD temp2;

   while( count-- != 0 )
      {
      temp1 = ( crc >> 8 ) & 0x00FFFFFFL;
      temp2 = swp_crc_32_tab[ ( (WORD) crc ^ *p++ ) & 0xff ];
      crc = temp1 ^ temp2;
      }
   return( crc );
}

/* This function creates a new, empty, FTS for a ZMODEM
 * file transmission session.
 */
LPZFT FASTCALL ZModemCreateFTS( void )
{
   LPZFT lpzft;

   INITIALISE_PTR(lpzft);
   if( fNewMemory( &lpzft, sizeof(ZFT) ) )
      memset( lpzft, 0, sizeof(ZFT) );
   return( lpzft );
}

BOOL WINAPI EXPORT Aftp_Send( LPCOMMDEVICE lpcdev, LPFFILETRANSFERSTATUS lpffts, HWND hwnd, LPSTR lpszFileName, BOOL fAuto )
{
   LPZFT lpzft;
   BOOL fOk;

   /* First create the ZMODEM file transfer
    * structure.
    */
   if( NULL == ( lpzft = ZModemCreateFTS() ) )
      return( FALSE );
   fNewMemory( &lpName, LEN_TEMPBUF );
   lpzft->lpcdev = lpcdev;
   lpzft->lpffts = lpffts;
   lpzft->fts.hwndOwner = hwnd;
   lpzft->fh = HNDFILE_ERROR;
   fZmodemAbort = FALSE;

   /* Open the debug file.
    */
   OpenDebugFile( lpzft );

   /* Initialise.
    */
   ZSendStatus( lpzft, FTST_YIELD );

   /* Disable the callback for this comms session.
    */
   Amcomm_SetDataLogging( lpzft->lpcdev, FALSE );
   Amcomm_SetCallback( lpzft->lpcdev, FALSE );
   lpzft->fExpandLFtoCRLF = Amcomm_GetBinary( lpzft->lpcdev );

   /* Initialise variables.
    */
   lpzft->dwStartTick = GetTickCount();
   lpzft->current_file = lpszFileName;
   lpzft->return_status = XFER_RETURN_SUCCESS;
   lpzft->current_block_size = -1;
   lpzft->fts.dwSizeSoFar = 0xFFFFFFFF;
   lpzft->fts.dwBytesRead = 0xFFFFFFFF;
   lpzft->fts.szLastMessage[ 0 ] = '\0';
   lpzft->garbage_count = -1;
   lpzft->cCancels = -1;
   lpzft->fts.cErrors = 0;
   lpzft->fts.wPercentage = 0;

   /* Main loop starts here.
    */
   if( OpenLink( lpzft ) )
   {
      ZSendStatus( lpzft, FTST_UPLOAD );

      while( OpenNextFile( lpzft ) )
         {
         Z_ShowLoc( lpzft, 0 );
         if( StartFileTransfer( lpzft ) )
            SendFileData( lpzft );
         Amfile_Close( lpzft->fh );
         lpzft->fh = HNDFILE_ERROR;
         if( lpzft->return_status >= ASSUCCESS )
            {
            if( lpzft->fts.cps == 0 )
               lpzft->fts.cps = (int)lpzft->fts.dwBytesRead;
            lpzft->fts.wPercentage = 100;
            ZSendStatus( lpzft, FTST_UPDATE );
            CloseLink( lpzft );
            }
         else
            {
            _XferCleanup( lpzft->lpcdev );
            break;
            }
         }
   }
   fOk = lpzft->return_status == ASSUCCESS;

   /* Send the final status report.
    */
   ZSendStatus( lpzft, fOk ? FTST_COMPLETED : FTST_FAILED );

   /* Close the debug file.
    */
   CloseDebugFile( lpzft );

   /* Restore the callback for this comms session.
    */
   Amcomm_SetCallback( lpzft->lpcdev, TRUE );
   Amcomm_SetDataLogging( lpzft->lpcdev, TRUE );
   FreeMemory( &lpzft );
   if( NULL != lpName )
      FreeMemory( &lpName );
   return( fOk );
}

BOOL WINAPI EXPORT Aftp_Cancel( LPCOMMDEVICE lpcdev )
{
   _XferCleanup( lpcdev );
   return( TRUE );
}

/* This function returns a string that describes this file
 * transfer protocol.
 */
void WINAPI EXPORT Aftp_Describe( LPSTR lpBuf, int cbMax )
{
   strcpy( lpBuf, "ZMODEM" );
}

/* This function returns the command strings necessary to
 * configure the conferencing system to use this protocol.
 */
void WINAPI EXPORT Aftp_Commands( LPSTR lpBuf, int cbMax )
{
   strcpy( lpBuf, "d z u z" );
}

int FASTCALL BackChannelActivity( LPZFT lpzft )
{
   char ch;

   if( !_XferCheckCarrier( lpzft ) )
      return( 1 );
   while( Amcomm_ReadChar( lpzft->lpcdev, &ch ) )
      {
      if( ch == ZCAN || ch == ZPAD )
         {
         SendBinaryPacket( lpzft, 0, ZCRCE );
         return( 1 );
         }
      else if( ch != XON && ch != XOFF )
         if( lpzft->garbage_count++ > 100 )
            {
            SendBinaryPacket( lpzft, 0, ZCRCE );
            return( 1 );
            }
         else ++lpzft->garbage_count;
      if( !_XferCheckCarrier( lpzft ) )
         return( 1 );
      }
   return( 0 );
}

BOOL FASTCALL _XferCheckCarrier( LPZFT lpzft )
{
   if( !Amcomm_CheckCarrier( lpzft->lpcdev ) )
      {
      lpzft->return_status = ASNOCARRIER;
      return( FALSE );
      }
   return( TRUE );
}

void FASTCALL _XferCleanup( LPCOMMDEVICE lpcdev )
{
   static char *abort_string = "\x18\x18\x18\x18\x18\x18\x18\x18\x18\x18\x08\x08\x08\x08\x08\x08\x08\x08\x08\x08";
   char ch;

   Amcomm_WriteString( lpcdev, abort_string, strlen( abort_string ) );
   while( Amcomm_QueryCharReady( lpcdev ) )
      Amcomm_ReadChar( lpcdev, &ch );
   fZmodemAbort = TRUE;
}

int FASTCALL _XferOpenFile( LPZFT lpzft )
{
   lpzft->fh = Amfile_Open( lpzft->fts.szFilename, AOF_SHARE_READ );
   if( lpzft->fh != HNDFILE_ERROR )
      {
      lpzft->fts.dwSizeSoFar = lpzft->fts.dwBytesRead = 0;
      lpzft->fts.dwTotalSize = Amfile_GetFileSize( lpzft->fh );
      lpzft->frcvsize = lpzft->fts.dwTotalSize;
      return( TRUE );
      }
   lpzft->return_status = XFER_RETURN_CANT_OPEN_FILE;
   _XferCleanup( lpzft->lpcdev );
   return( FALSE );
}

BOOL FASTCALL _XferAbortKeyPressed( LPZFT lpzft )
{
   if( 0 == GetWindowText( GetFocus() , lpName, 18 ) )
      wsprintf( lpName, "%s", "Blank" );
   ZSendStatus( lpzft, FTST_YIELD );
   if( Amcomm_GetCommStatus() == WCE_ABORT )
      fZmodemAbort = TRUE;
   if( fZmodemAbort || ( ( GetAsyncKeyState( VK_ESCAPE ) & 0x8000 )  && lstrcmp( lpName, "Connection Status" ) == 0 ) )
      {
      lpzft->return_status = ASABORT;
      return( TRUE );
      }
   return( FALSE );
}

void FASTCALL SendDataFrame( LPZFT lpzft )
{
   register int i;
   BOOL last_subpacket_in_frame;
   char header[ 4 ];

   lpzft->garbage_count = 0;
   if( BackChannelActivity( lpzft ) )
      return;
   for( i = 0; i < 4; ++i )
      header[ i ] = (char)( ( lpzft->fts.dwSizeSoFar >> ( i * 8 ) ) & 0xFF );
   lpzft->rx_frame_output_count = 0;
   SendBinaryHeader( lpzft, ZDATA, header );
   if( lpzft->return_status < ASSUCCESS )
      return;
   do {
      if( _XferAbortKeyPressed( lpzft ) )
         return;
      if( BackChannelActivity( lpzft ) )
         return;
      last_subpacket_in_frame = ReadBlock( lpzft );
      if( lpzft->error_recovery )
         last_subpacket_in_frame = 1;
      SendBinaryPacket( lpzft, lpzft->current_block_size, (char)( last_subpacket_in_frame ? ZCRCW : ZCRCG ) );
      if( lpzft->return_status < ASSUCCESS )
         return;
      }
   while( last_subpacket_in_frame == 0 );
}

BOOL FASTCALL ReadBlock( LPZFT lpzft )
{
   int bytes_to_read;

   if( lpzft->rx_buffer_size > 0 )
      bytes_to_read = lpzft->rx_buffer_size - lpzft->rx_frame_output_count;
   else
      bytes_to_read = ZMAXSPLEN;
   if( bytes_to_read > ZMAXSPLEN )
      bytes_to_read = ZMAXSPLEN;
   Amfile_SetPosition( lpzft->fh, lpzft->fts.dwSizeSoFar, ASEEK_BEGINNING );
   lpzft->current_block_size = Amfile_Read( lpzft->fh, lpzft->txbuf, bytes_to_read );
   if( lpzft->current_block_size <= 0 )
      {
      lpzft->return_status = XFER_RETURN_CANT_GET_BUFFER;
      _XferCleanup( lpzft->lpcdev );
      return( TRUE );
      }
   lpzft->fts.dwSizeSoFar += lpzft->current_block_size;
   lpzft->fts.dwBytesRead += lpzft->current_block_size;
   Z_ShowLoc( lpzft, lpzft->fts.dwSizeSoFar );
   lpzft->rx_frame_output_count += lpzft->current_block_size;
   if( lpzft->rx_buffer_size == 0 ) {
      if( ( lpzft->rx_flags & CANOVIO ) == 0 )
         return( TRUE );
      }
   else
      {
      if( lpzft->rx_frame_output_count >= lpzft->rx_buffer_size )
         return( TRUE );
      }
   if( lpzft->fts.dwSizeSoFar >= lpzft->fts.dwTotalSize )
      return( TRUE );
   return( FALSE );
}

int FASTCALL StartFileTransfer( LPZFT lpzft )
{
   int frame_type;

   lpzft->error_recovery = 0;
   for( ;; )
      {
      SendBinaryHeader( lpzft, ZFILE, "\0\0\0\0" );
      if( lpzft->return_status < ASSUCCESS )
         return( lpzft->return_status );
      SendBinaryPacket( lpzft, lpzft->current_block_size, ZCRCW );
      if( lpzft->return_status < ASSUCCESS )
         return( lpzft->return_status );
      frame_type = ReadFrameHeader( lpzft, FALSE );
      ZWriteFrame( lpzft, frame_type );
      switch( frame_type )
         {
         case ZRPOS:
            lpzft->fts.dwSizeSoFar =
               ( (DWORD)lpzft->txhdr[ 3 ] << 24 ) +
               ( (DWORD)lpzft->txhdr[ 2 ] << 16 ) +
               ( (DWORD)lpzft->txhdr[ 1 ] << 8 ) +
               ( (DWORD)lpzft->txhdr[ 0 ] );
            return( TRUE );

         case ZSKIP:
            lpzft->return_status = XFER_RETURN_REMOTE_ABORT;
            return( FALSE );

         default:
            if( lpzft->return_status < ASSUCCESS )
               return( lpzft->return_status );
            ++lpzft->fts.cErrors;
            ZSendStatus( lpzft, FTST_UPDATE );
            if( lpzft->fts.cErrors > ZMODEM_MAX_ERRORS )
               {
               lpzft->return_status = XFER_RETURN_TOO_MANY_ERRORS;
               return( lpzft->return_status );
               }
            break;
         }
      }
}

void FASTCALL SendFileData( LPZFT lpzft )
{
   register int i;
   char header[ 4 ];
   BOOL done;
   int frame_type;

   for( ;; )
      {
      SendDataFrame( lpzft );
      if( lpzft->return_status < ASSUCCESS )
         return;
      done = 0;
      while( !done )
         {
         frame_type = ReadFrameHeader( lpzft, FALSE );
         ZWriteFrame( lpzft, frame_type );
         switch( frame_type )
            {
            case ZRPOS:
               lpzft->fts.dwSizeSoFar = 
                  ( (DWORD)lpzft->txhdr[ 3 ] << 24 ) +
                  ( (DWORD)lpzft->txhdr[ 2 ] << 16 ) +
                  ( (DWORD)lpzft->txhdr[ 1 ] << 8 ) +
                  ( (DWORD)lpzft->txhdr[ 0 ] );
               Amfile_SetPosition( lpzft->fh, lpzft->fts.dwSizeSoFar, ASEEK_BEGINNING );
               lpzft->error_recovery = 1;
               done = 1;
               break;

            case ZACK:
               lpzft->error_recovery = 0;
               done = 1;
               break;

            case ZRINIT:
            case ZSKIP:
               lpzft->return_status = XFER_RETURN_REMOTE_ABORT;
               done = 1;
               break;

            default:
               if( lpzft->return_status < ASSUCCESS )
                  return;
               break;
            }
         }
      if( lpzft->fts.dwSizeSoFar >= lpzft->fts.dwTotalSize )
         break;
      }
   for( i = 0; i < 4; ++i )
      header[ i ] = (char)( ( lpzft->fts.dwSizeSoFar >> ( i * 8 ) ) & 0xFF );
   SendBinaryHeader( lpzft, ZEOF, header );
   if( lpzft->return_status < ASSUCCESS )
      return;
   ReadFrameHeader( lpzft, FALSE );
}

int FASTCALL ReadFrameHeader( LPZFT lpzft, int immediate_return )
{
   int frame_type;

   lpzft->garbage_count = 0;
   for( ;; ) 
      {
      if( !GetPad( lpzft ) )
         {
         lpzft->header_type = ASBUFREMPTY;
         if( lpzft->return_status < ASSUCCESS )
            return( lpzft->return_status );
         }
      else
         lpzft->header_type = GetHeaderType( lpzft );
      switch( lpzft->header_type )
         {
         case 'B':
            frame_type = ReadHexFrameHeaderData( lpzft );
            break;

         case 'A':
            frame_type = ReadBinaryHeaderData16( lpzft );
            break;

         case 'C':
            frame_type = ReadBinaryHeaderData32( lpzft );
            break;

         case 0:
            if( lpzft->garbage_count >= 2048 )
               frame_type = ASGENERALERROR;
            else
               frame_type = ASSUCCESS;
            break;

         default:
            frame_type = ASGENERALERROR;
            break;
         }
      if( frame_type < ASSUCCESS ) {
         if( lpzft->return_status < ASSUCCESS )
            return( lpzft->return_status );
         ++lpzft->fts.cErrors;
         ZSendStatus( lpzft, FTST_UPDATE );
         if( lpzft->fts.cErrors > ZMODEM_MAX_ERRORS )
            {
            lpzft->return_status = XFER_RETURN_TOO_MANY_ERRORS;
            return( lpzft->return_status );
            }
         if( immediate_return )
            return( frame_type );
         }
      else if( frame_type >= ASSUCCESS )
         return( frame_type );
      }
}

int FASTCALL GetPad( LPZFT lpzft )
{
   int timeout_timer;
   char c;

   lpzft->cCancels = 0;
   timeout_timer = 0;
   for(;;)
      {
      if( !Amcomm_QueryCharReady( lpzft->lpcdev ) )
         if( _XferAbortKeyPressed( lpzft ) )
            return( FALSE );
      if( !_XferCheckCarrier( lpzft ) )
         return( FALSE );
      if( !Amcomm_ReadCharTimed( lpzft->lpcdev, &c, 1000 ) )
         c = ASBUFREMPTY;
      switch( c )
         {
         case ZPAD:
            return( TRUE );

         case ASBUFREMPTY:
            timeout_timer++;
            if( timeout_timer >= 10 )
               return( FALSE );
            if( _XferAbortKeyPressed( lpzft ) )
               return( FALSE );
            break;

         case CAN:
            lpzft->cCancels++;
            timeout_timer = 0;
            if( lpzft->cCancels >= 5 )
               {
               lpzft->return_status = XFER_RETURN_REMOTE_ABORT;
               _XferCleanup( lpzft->lpcdev );
               return( FALSE );
               }
            else {
               if( _XferAbortKeyPressed( lpzft ) )
                  return( FALSE );
               if( !_XferCheckCarrier( lpzft ) )
                  return( FALSE );
               if( !Amcomm_ReadCharTimed( lpzft->lpcdev, &c, 120 ) )
                  c = ASBUFREMPTY;
               switch( c )
                  {
                  case CAN:
                     lpzft->cCancels++;
                     break;

                  default:
                     lpzft->garbage_count++;
                     break;
                  }
               }
            break;

         default:
            timeout_timer = 0;
            lpzft->cCancels = 0;
            lpzft->garbage_count++;
            if( ( lpzft->garbage_count & 0xff ) == 0 )
               {
               if( _XferAbortKeyPressed( lpzft ) )
                  return( FALSE );
               break;
               }
            if( lpzft->garbage_count >= 2048 )
               return( FALSE );
        }
    }
}

int FASTCALL GetHeaderType( LPZFT lpzft )
{
   int c;

   for( ;; )
      {
      c = GetCookedChar( lpzft );
      if( c == ZPAD )
         continue;
      if( c == ZDLE )
         break;
      if( c < ASSUCCESS )
         return( c );
      lpzft->garbage_count++;
      return( 0 );
      }
   return( GetCookedChar( lpzft ) );
}

int FASTCALL ReadHexFrameHeaderData( LPZFT lpzft )
{
   int c;
   int frame_type;
   WORD crc;
   int i;

   frame_type = ReadHexByte( lpzft );
   if ( frame_type < 0 )
      return( frame_type );
   ZWriteFrame( lpzft, frame_type );
   crc = CalculateCharacterCRC16( 0U, (BYTE) frame_type );
   for ( i = 0 ; i < 4 ; i++ )
      {
      c = ReadHexByte( lpzft );
      if ( c < 0 )
         return( c );
      lpzft->txhdr[ i ] = (BYTE) c;
      crc = CalculateCharacterCRC16( crc, (BYTE) c );
      }
   c = ReadHexByte( lpzft );
   if ( c < 0 )
      return( c );
   crc = CalculateCharacterCRC16( crc, (BYTE) c );
   c = ReadHexByte( lpzft );
   if ( c < 0 )
      return( c );
   crc = CalculateCharacterCRC16( crc, (BYTE) c );
   if ( crc != 0 )
      return( ASGENERALERROR );

   for ( i = 0 ; i < 5 ; i ++ )
      {
      if ( !Amcomm_QueryCharReady( lpzft->lpcdev ) )
         break;
      if( !_XferCheckCarrier( lpzft ) )
         return( ASGENERALERROR );
      Amcomm_ReadChar( lpzft->lpcdev, (BYTE *)&c );
      c &= 0x7f;
      if ( c != CR && c != LF && c != XON )
         return( ASGENERALERROR );
      }
   return( frame_type );
}

int FASTCALL ReadBinaryHeaderData16( LPZFT lpzft )
{
   int c;
   int frame_type;
   WORD crc;
   int i;

   frame_type = ReadBinaryByte( lpzft );
   if ( frame_type < 0 )
      return( frame_type );
   ZWriteFrame( lpzft, frame_type );
   crc = CalculateCharacterCRC16( 0, (BYTE) frame_type );
   for ( i = 0 ; i < 4 ; i++ )
      {
      c = ReadBinaryByte( lpzft );
      if ( c < 0 )
         return( c );
      lpzft->txhdr[ i ] = (BYTE) c;
      crc = CalculateCharacterCRC16( crc, (BYTE) c );
      }
   c = ReadBinaryByte( lpzft );
   if ( c < 0 )
      return( c );
   crc = CalculateCharacterCRC16( crc, (BYTE) c );
   c = ReadBinaryByte( lpzft );
   if ( c < 0 )
      return( c );
   crc = CalculateCharacterCRC16( crc, (BYTE) c );
   if ( crc != 0 )
      return( ASGENERALERROR );
   return( frame_type );
}

int FASTCALL ReadBinaryHeaderData32( LPZFT lpzft )
{
   int c;
   int frame_type;
   DWORD crc;
   int i;

   frame_type = ReadBinaryByte( lpzft );
   if( frame_type < 0 )
      return( frame_type );
   ZWriteFrame( lpzft, frame_type );
   crc = CalculateCharacterCRC32( 0xFFFFFFFFL, (BYTE)frame_type );
   for ( i = 0 ; i < 4 ; i++ )
      {
      c = ReadBinaryByte( lpzft );
      if ( c < 0 )
         return( c );
      lpzft->txhdr[ i ] = (BYTE)c;
      crc = CalculateCharacterCRC32( crc, (BYTE) c );
      }
   c = ReadBinaryByte( lpzft );
   if ( c < 0 )
      return( c );
   crc = CalculateCharacterCRC32( crc, (BYTE) c );
   c = ReadBinaryByte( lpzft );
   if ( c < 0 )
      return( c );
   crc = CalculateCharacterCRC32( crc, (BYTE) c );
   c = ReadBinaryByte( lpzft );
   if ( c < 0 )
      return( c );
   crc = CalculateCharacterCRC32( crc, (BYTE) c );
   c = ReadBinaryByte( lpzft );
   if ( c < 0 )
      return( c );
   crc = CalculateCharacterCRC32( crc, (BYTE) c );
   if ( crc != 0xDEBB20E3L )
      return( ASGENERALERROR );
   return( frame_type );
}

int FASTCALL ReadHexByte( LPZFT lpzft )
{
   int c;
   int value;

   c = GetCookedChar( lpzft );
   if ( c >= '0' && c <= '9' )
      value = c - '0';
   else if ( c >= 'a' && c <= 'f' )
      value = c -'a' + 10;
   else
      return( ASGENERALERROR );
   value <<= 4;
   c = GetCookedChar( lpzft );
   if ( c >= '0' && c <= '9' )
      return( value + c - '0' );
   else if ( c >= 'a' && c <= 'f' )
      return( value + c -'a' + 10 );
   return( ASGENERALERROR );
}

int FASTCALL GetCookedChar( LPZFT lpzft )
{
   char c;

   for( ;; )
      {
      if( !Amcomm_QueryCharReady( lpzft->lpcdev ) )
         if ( _XferAbortKeyPressed( lpzft ) )
            return( lpzft->return_status );
      if( !_XferCheckCarrier( lpzft ) )
         return( lpzft->return_status );
      if( !Amcomm_ReadCharTimed( lpzft->lpcdev, &c, 2500 ) )
         c = ASBUFREMPTY;
      if ( c < 0 )
         return( c );
      c &= 0x7f;
      if ( c != XOFF && c != XON )
         return( c );
      }
}

void FASTCALL SendBinaryHeader( LPZFT lpzft, char frame_type, char *header_data )
{
   WORD crc;
   DWORD crc32;
   int length = 4;
   int i;

   lpzft->secbuf[ 0 ] = ZPAD;
   lpzft->secbuf[ 1 ] = ZDLE;
   if( lpzft->fUseCRC32 )
      {
      lpzft->secbuf[ 2 ] = ZBIN32;
      crc32 = CalculateCharacterCRC32( 0xFFFFFFFFL, frame_type );
      crc32 = CalculateBlockCRC32( length, crc32, header_data );
      crc32 = ~crc32;
      crc = 0;
      }
   else {
      lpzft->secbuf[ 2 ] = ZBIN;
      crc = CalculateCharacterCRC16( 0, frame_type );
      crc = CalculateBlockCRC16( length, crc, header_data );
      crc32 = 0;
      }
   i = 3;
   i += EncodeCharacter( lpzft->secbuf + i, frame_type );
   while ( length-- > 0 )
      i += EncodeCharacter( lpzft->secbuf + i, *header_data++ );
   if( lpzft->fUseCRC32 )
      {
      i += EncodeCharacter( lpzft->secbuf + i, (BYTE)( crc32 & 0xff ) );
      i += EncodeCharacter( lpzft->secbuf + i, (BYTE)( crc32 >>  8 ) );
      i += EncodeCharacter( lpzft->secbuf + i, (BYTE)( crc32 >> 16 ) );
      i += EncodeCharacter( lpzft->secbuf + i, (BYTE)( crc32 >> 24 ) );
      }
   else {
      i += EncodeCharacter( lpzft->secbuf + i, (BYTE)( crc >> 8 ) );
      i += EncodeCharacter( lpzft->secbuf + i, (BYTE)( crc & 0xff ) );
      }
   lpzft->return_status = Amcomm_WriteString( lpzft->lpcdev, lpzft->secbuf, i ) ? ASSUCCESS : ASGENERALERROR;
}

int FASTCALL SendHexHeader( LPZFT lpzft, char frame_type, BYTE *header_data )
{
   WORD crc;
   char *buf;
   int length = 4;

   crc = CalculateCharacterCRC16( 0, frame_type );
   crc = CalculateBlockCRC16( length, crc, header_data );

   lpzft->secbuf[ 0 ] = ZPAD;
   lpzft->secbuf[ 1 ] = ZPAD;
   lpzft->secbuf[ 2 ] = ZDLE;
   lpzft->secbuf[ 3 ] = ZHEX;
   wsprintf( lpzft->secbuf + 4, "%02x", frame_type );
   buf = lpzft->secbuf + 6;

   while( length-- > 0 )
      {
      wsprintf( buf, "%02x", *header_data++ );
      buf += 2;
      }
   if( lpzft->fExpandLFtoCRLF )
      {
      wsprintf( buf, "%04x\n", crc );
      buf += 5;
      }
   else
      {
      wsprintf( buf, "%04x\r\n", crc );
      buf += 6;
      }
   if( frame_type != ZFIN && frame_type != ZACK )
      {
      wsprintf( buf, "%c", XON );
      buf += 1;
      }
   *buf = '\0';
   return( Amcomm_WriteString( lpzft->lpcdev, lpzft->secbuf, (int)( buf - lpzft->secbuf ) ) );
}

void FASTCALL SendBinaryPacket( LPZFT lpzft, int length, char terminator )
{
   WORD crc;
   DWORD crc32;
   int i, x;

   crc = 0;
   crc32 = 0;
   if( lpzft->fUseCRC32 )
      {
      crc32 = CalculateBlockCRC32( length, 0xFFFFFFFFL, lpzft->txbuf );
      crc32 = CalculateCharacterCRC32( crc32, terminator );
      crc32 = ~crc32;
      }
   else {
      crc = CalculateBlockCRC16( length, 0, lpzft->txbuf );
      crc = CalculateCharacterCRC16( crc, terminator );
      }
   i = 0;
   for( x = 0; x < length; x++ )
      i += EncodeCharacter( lpzft->secbuf + i, lpzft->txbuf[ x ] );
   lpzft->secbuf[ i++ ] = ZDLE;
   i += EncodeCharacter( lpzft->secbuf + i, terminator );
   if( lpzft->fUseCRC32 )
      {
      for( x = 0; x < 4; x++ )
         {
         i += EncodeCharacter( lpzft->secbuf + i, (BYTE)( crc32 & 0xff) );
         crc32 >>= 8;
         }
      } 
   else {
      for( x = 0; x < 2; x++ )
         {
         i += EncodeCharacter( lpzft->secbuf + i, (BYTE)( ( crc >> 8 ) & 0xff ) );
         crc <<= 8;
         }
      }
   if( terminator == ZCRCW )
      lpzft->secbuf[ i++ ] = XON;
   lpzft->return_status = Amcomm_WriteString( lpzft->lpcdev, lpzft->secbuf, i ) ? ASSUCCESS : ASGENERALERROR;
}

int FASTCALL EncodeCharacter( char * buf, BYTE c )
{
   switch ( c )
      {
      case ZDLE:
      case CR:
      case CR | 0x80:
      case XON:
      case XON | 0x80:
      case XOFF:
      case XOFF | 0x80:
      case DLE:
      case DLE | 0x80:
         *buf++ = ZDLE;
         *buf = (BYTE)( c ^ 0x40 );
         return( 2 );

      default:
         *buf = c;
         return( 1 );
      }
}

int FASTCALL ReadBinaryByte( LPZFT lpzft )
{
   int c;

   lpzft->cCancels = 0;
   if( !Amcomm_QueryCharReady( lpzft->lpcdev ) )
      if( _XferAbortKeyPressed( lpzft ) )
         return( lpzft->return_status );
   if( !_XferCheckCarrier( lpzft ) )
      return( lpzft->return_status );
   if( !Amcomm_ReadCharTimed( lpzft->lpcdev, (BYTE *)&c, 2500 ) )
      c = ASBUFREMPTY;
   c &= 0x00FF;
   if ( c < 0 )
      return( c );
   if ( c == ZDLE )
      {
      lpzft->cCancels++;
      for( ;; )
         {
         if( !Amcomm_QueryCharReady( lpzft->lpcdev ) )
            if( _XferAbortKeyPressed( lpzft ) )
               return( lpzft->return_status );
         if( !_XferCheckCarrier( lpzft ) )
            return( lpzft->return_status );
         if( !Amcomm_ReadCharTimed( lpzft->lpcdev, (BYTE *)&c, 2500 ) )
            c = ASBUFREMPTY;
         c &= 0x00FF;
         if ( c < 0 )
            return( c );
         switch( c )
            {
            case ZDLE:
               lpzft->cCancels++;
               break;

            case 'h':
            case 'i':
            case 'j':
            case 'k':
               return( c + 0x100 );

            default:
               return( c ^ 0x40 );
            }
         if( lpzft->cCancels >= 5 )
            {
            lpzft->return_status = XFER_RETURN_REMOTE_ABORT;
            return( lpzft->return_status );
            }
         }
      }
   else
      return( c );
}

int FASTCALL OpenLink( LPZFT lpzft )
{
   int frame_type;

   if( SendHexHeader( lpzft, ZRQINIT, (BYTE *)"\0\0\0\0" ) < ASSUCCESS )
      return( FALSE );
   for( ;; )
      {
      frame_type = ReadFrameHeader( lpzft, FALSE );
      ZWriteFrame( lpzft, frame_type );
      switch( frame_type )
         {
         case ZRINIT:
            lpzft->rx_buffer_size = ( lpzft->txhdr[ 0 ] & 0xff ) + ( ( lpzft->txhdr[ 1 ] & 0xff ) << 8  );
//          lpzft->rx_buffer_size = ( lpzft->txhdr[ 0 ] & 0xff ) + ( ( lpzft->txhdr[ 1 ] << 8 ) & 0xff );
            lpzft->rx_flags = lpzft->txhdr[ 3 ];
            lpzft->fUseCRC32 = lpzft->rx_flags & CANFC32;
            return( TRUE );

         default:
            ++lpzft->fts.cErrors;
            ZSendStatus( lpzft, FTST_UPDATE );
            if( lpzft->fts.cErrors > ZMODEM_MAX_ERRORS )
               lpzft->return_status = XFER_RETURN_TOO_MANY_ERRORS;
            if( lpzft->return_status < ASSUCCESS )
               return( FALSE );
         }
      }
}

int FASTCALL OpenNextFile( LPZFT lpzft )
{
   int i;
   char * name_to_send;

   i = 0;
   while( *lpzft->current_file )
      {
      if( *lpzft->current_file == '"' )
         ++lpzft->current_file;
      else
         lpzft->fts.szFilename[ i++ ] = *lpzft->current_file++;
      }
   if ( i == 0 )
      return( FALSE );
   lpzft->fts.szFilename[ i ] = '\0';
   if( !_XferOpenFile( lpzft ) )
      return( FALSE );
   name_to_send = strrchr( lpzft->fts.szFilename, '\\' );
   if( !name_to_send )
      name_to_send = strrchr( lpzft->fts.szFilename, ':' );
   if( name_to_send )
      name_to_send++;
   if ( name_to_send == NULL )
      name_to_send = lpzft->fts.szFilename;
   lpzft->current_block_size = wsprintf( lpzft->txbuf, "%s%c%ld%c", (LPSTR)name_to_send, 0, lpzft->fts.dwTotalSize, 0 );
   ZSendStatus( lpzft, FTST_BEGINTRANSMIT );
   return( TRUE );
}

int FASTCALL CloseLink( LPZFT lpzft )
{
   int frame_type;

   for ( ;; )
      {
      SendHexHeader( lpzft, ZFIN, (BYTE *)"\0\0\0\0" );
      if( lpzft->return_status < ASSUCCESS )
         break;
      frame_type = ReadFrameHeader( lpzft, FALSE );
      ZWriteFrame( lpzft, frame_type );
      if( frame_type == ZFIN )
         break;
      if( lpzft->return_status < ASSUCCESS )
         break;
      ++lpzft->fts.cErrors;
      ZSendStatus( lpzft, FTST_UPDATE );
      if( lpzft->fts.cErrors > ZMODEM_MAX_ERRORS )
         lpzft->return_status = XFER_RETURN_TOO_MANY_ERRORS;
      }
   Amcomm_WriteChar( lpzft->lpcdev, 'O' );
   Amcomm_WriteChar( lpzft->lpcdev, 'O' );
   return( lpzft->return_status );
}

/* Update the transfer statistics and call the callback function
 * if required.
 */
void FASTCALL Z_ShowLoc( LPZFT lpzft, long l )
{
   int wCount;
   int seconds;

   wCount = 0;
   if( lpzft->fts.dwTotalSize )
      wCount = (WORD)( ( (DWORD)l * 100 ) / lpzft->fts.dwTotalSize );
   if( wCount != lpzft->fts.wPercentage )
      lpzft->fts.wPercentage = wCount;
   seconds = (int)( ( GetTickCount() - lpzft->dwStartTick ) / 1000 );
   if( seconds == 0 ) // !!SM!!
      seconds = 1;
// if( seconds = (int)( ( GetTickCount() - lpzft->dwStartTick ) / 1000 ) ) // !!SM!!
      if( lpzft->fts.cps = (int)( lpzft->fts.dwBytesRead / seconds ) )
         {
         /* Do cps smoothing
          */
      #ifdef DO_CPS_SMOOTHING
         if( lpzft->nCpsIndex < MAX_CPS )
            {
            if( lpzft->nItemsInCpsArray < MAX_CPS )
               ++lpzft->nItemsInCpsArray;
            }
         else
            lpzft->nCpsIndex = 0;
         lpzft->CpsSum -= lpzft->CpsIndexArray[ lpzft->nCpsIndex ];
         lpzft->CpsIndexArray[ lpzft->nCpsIndex++ ] = lpzft->fts.cps;
         lpzft->CpsSum += lpzft->fts.cps;
         lpzft->fts.cps = lpzft->CpsSum / lpzft->nItemsInCpsArray;
      #endif

         /* Compute time remaining and elapsed.
          */
         lpzft->fts.dwTimeRemaining = ( lpzft->frcvsize - lpzft->fts.dwBytesRead ) / lpzft->fts.cps;
         lpzft->fts.dwTimeSoFar = lpzft->frcvsize / lpzft->fts.cps;
         }
   lpzft->dwTotal = 0;
   ZSendStatus( lpzft, FTST_UPDATE );
}

/* Sends a break command to the remote system.
 * Currently does nothing.
 */
void FASTCALL Z_SendBreak( void )
{
}

long FASTCALL Z_FromUnixDate( char * npstr )
{
   AM_DATE date;
   AM_TIME time;
   struct tm * ptm;
   time_t secs = 0;

   while( *npstr )
      secs = ( secs * 8 ) + ( *npstr++ - '0' );
   if( secs == 0 )
      return( MAKELPARAM( Amdate_GetPackedCurrentTime(), Amdate_GetPackedCurrentDate() ) );
   /* OK. Currently CIX always timestamps downloaded files with GMT time. 
    * We convert to local time within each section, i.e. cixlist.c takes 
    * the GMT time and converts to local time. If CIX ever moves to Solaris
    * and time stamps files properly (i.e. according to the right TZ) you can
    * comment out the next line and use the localtime one instead, and then
    * remove all the local conversions elsewhere.
    */
   ptm = gmtime( &secs );
// ptm = localtime( &secs );
   date.iYear = FixupYear( (WORD)ptm->tm_year );
   date.iMonth = ptm->tm_mon + 1;
   date.iDay = ptm->tm_mday;
   date.iDayOfWeek = 0;             // Ignored for file I/O
   time.iHour = ptm->tm_hour;
   time.iMinute = ptm->tm_min;
   time.iSecond = ptm->tm_sec;
   time.iHSeconds = 0;
   return( MAKELONG( Amdate_PackTime( &time ), Amdate_PackDate( &date ) ) );
}

/* Read one byte from the communications port. If no byte is available,
 * delay for 100 milliseconds before retrying. The tenths parameter
 * specifies how long to wait before giving up, where 1 = 100 milliseconds
 * (one tenth of a second).
 */
int FASTCALL Z_GetByte( LPZFT lpzft, int tenths )
{
   char ch;

   do {
      int wCommErr;

      if( Amcomm_ReadChar( lpzft->lpcdev, &ch ) )
         return( (int)(unsigned char)ch );
      wCommErr = Amcomm_GetCommStatus();
      if( wCommErr == WCE_ABORT || wCommErr == WCE_NOCARRIER )
         fZmodemAbort = TRUE;
      if( fZmodemAbort )
         return( ZCAN );
      GoToSleep( 100 );
      ZSendStatus( lpzft, FTST_YIELD );
      }
   while( --tenths );
   return( ZTIMEOUT );
}

/* Reads one byte from the communication port. If no byte is available,
 * delay for lpzft->rxtimeout * 100 milliseconds.
 */
int FASTCALL Z_qk_read( LPZFT lpzft )
{
   return( Z_GetByte( lpzft, lpzft->rxtimeout ) );
}

int FASTCALL Z_TimedRead( LPZFT lpzft )
{
   int c;

retry:
   c = Z_qk_read( lpzft );
   if( ( c & 0x7F ) == 17 || ( c & 0x7F ) == 19 )
      goto retry;
   return( c );
}

void FASTCALL Z_SendCan( LPZFT lpzft )
{
   register int c;

   Amcomm_ClearOutbound( lpzft->lpcdev );
   for( c = 0; c < 10; ++c )
      {
      Amcomm_WriteChar( lpzft->lpcdev, CAN );
      Amcomm_Delay( lpzft->lpcdev, 100 );
      }
   for( c = 0; c < 10; ++c )
      Amcomm_WriteChar( lpzft->lpcdev, 8 );
}

void FASTCALL Z_PutString( LPZFT lpzft, char * p )
{
   while( *p ) {
      switch( *p ) {
         case 221:   Z_SendBreak(); break;
         case 222:   Amcomm_Delay( lpzft->lpcdev, 2000 ); break;
         default:    Amcomm_WriteChar( lpzft->lpcdev, *p ); break;
         }
      ++p;
      }
}

long FASTCALL Z_PullLongFromHeader( char * nphdr )
{
   long l;

   l = nphdr[ ZP3 ] & 0xFF;
   l = ( l << 8 ) | ( nphdr[ ZP2 ] & 0xFF );
   l = ( l << 8 ) | ( nphdr[ ZP1 ] & 0xFF );
   l = ( l << 8 ) | ( nphdr[ ZP0 ] & 0xFF );
   return( l );
}

void FASTCALL Z_PutLongIntoHeader( LPZFT lpzft, long l )
{
   lpzft->txhdr[ ZP0 ] = (BYTE)( l & 0xFF );
   lpzft->txhdr[ ZP1 ] = (BYTE)( l >> 8 );
   lpzft->txhdr[ ZP2 ] = (BYTE)( l >> 16 );
   lpzft->txhdr[ ZP3 ] = (BYTE)( l >> 24 );
}

int FASTCALL Z_GetZDL( LPZFT lpzft )
{
   int c;

   if( ( c = Z_qk_read( lpzft ) ) != ZDLE )
      return( c );
   if( ( c = Z_qk_read( lpzft ) ) < 0 )
      return( c );
   if( c == CAN && ( c = Z_qk_read( lpzft ) ) < 0 )
      return( c );
   if( c == CAN && ( c = Z_qk_read( lpzft ) ) < 0 )
      return( c );
   if( c == CAN && ( c = Z_qk_read( lpzft ) ) < 0 )
      return( c );
   switch( c ) {
      case CAN:
         return( GOTCAN );

      case ZCRCE: case ZCRCG:
      case ZCRCQ: case ZCRCW:
         return( c | GOTOR );

      case ZRUB0:
         return( 0x007F );

      case ZRUB1:
         return( 0x00FF );

      default:
         if( ( c & 0x60 ) == 0x40 )
            return( c ^ 0x40 );
         return( ZERROR );
      }
}

int FASTCALL Z_GetHex( LPZFT lpzft )
{
   register int n, c;

   if( ( n = Z_TimedRead( lpzft ) ) < 0 )
      return( n );
   if( ( n -= 0x30 ) > 9 )
      n -= 39;
   if( ( n & 0xFFF0 ) != 0 )
      return( ZERROR );
   if( ( c = Z_TimedRead( lpzft ) ) < 0 )
      return( c );
   if( ( c -= 0x30 ) > 9 )
      c -= 39;
   if( ( c & 0xFFF0 ) != 0 )
      return( ZERROR );
   return( ( n << 4 ) | c );
}

int FASTCALL Z_GetHexHeader( LPZFT lpzft, char * nphdr )
{
   WORD crc;
   register int n, c;

   if( ( c = Z_GetHex( lpzft ) ) < 0 )
      return( c );
   lpzft->rxtype = c;
   crc = AltCalculateCharacterCRC16( 0, (BYTE)lpzft->rxtype );
   for( n = 0; n < 4; ++n )
      {
      if( ( c = Z_GetHex( lpzft ) ) < 0 )
         return( c );
      nphdr[ n ] = (BYTE)( c & 0xFF );
      crc = AltCalculateCharacterCRC16( crc, nphdr[ n ] );
      }
   if( ( c = Z_GetHex( lpzft ) ) < 0 )
      return( c );
   crc = AltCalculateCharacterCRC16( crc, (BYTE)( c & 0xFF ) );
   if( ( c = Z_GetHex( lpzft ) ) < 0 )
      return( c );
   crc = AltCalculateCharacterCRC16( crc, (BYTE)( c & 0xFF ) );
   if( crc != 0 )
      {
      ++lpzft->fts.cErrors;
      ZSendStatus( lpzft, FTST_UPDATE );
      return( ZERROR );
      }
   if( Z_GetByte( lpzft, 1 ) == 13 )
      c = Z_GetByte( lpzft, 1 );
   return( lpzft->rxtype );
}

int FASTCALL Z_GetBinaryHeader( LPZFT lpzft, char * nphdr )
{
   WORD crc;
   register int n, c;

   if( ( c = Z_GetZDL( lpzft ) ) < 0 )
      return( c );
   lpzft->rxtype = c;
   crc = AltCalculateCharacterCRC16( 0, (BYTE)lpzft->rxtype );
   for( n = 0; n < 4; ++n )
      {
      if( ( c = Z_GetZDL( lpzft ) ) & 0xFF00 )
         return( c );
      nphdr[ n ] = c & 0xFF;
      crc = AltCalculateCharacterCRC16( crc, nphdr[ n ] );
      }
   if( ( c = Z_GetZDL( lpzft ) ) & 0xFF00 )
      return( c );
   crc = AltCalculateCharacterCRC16( crc, (BYTE)( c & 0xFF ) );
   if( ( c = Z_GetZDL( lpzft ) ) & 0xFF00 )
      return( c );
   crc = AltCalculateCharacterCRC16( crc, (BYTE)( c & 0xFF ) );
   if( crc != 0 )
      {
      ++lpzft->fts.cErrors;
      ZSendStatus( lpzft, FTST_UPDATE );
      }
   return( lpzft->rxtype );
}

int FASTCALL Z_GetBinaryHeader32( LPZFT lpzft, char * nphdr )
{
   DWORD crc;
   register int n, c;

   if( ( c = Z_GetZDL( lpzft ) ) < 0 )
      return( c );
   lpzft->rxtype = c;
   crc = AltCalculateCharacterCRC32( 0xFFFFFFFF, (BYTE)lpzft->rxtype );
   for( n = 0; n < 4; ++n )
      {
      if( ( c = Z_GetZDL( lpzft ) ) & 0xFF00 )
         return( c );
      nphdr[ n ] = c & 0xFF;
      crc = AltCalculateCharacterCRC32( crc, nphdr[ n ] );
      }
   for( n = 0; n < 4; ++n )
      {
      if( ( c = Z_GetZDL( lpzft ) ) & 0xFF00 )
         return( c );
      crc = AltCalculateCharacterCRC32( crc, (BYTE)( c & 0xFF ) );
      }
   if( crc != 0xDEBB20E3 )
      {
      ++lpzft->fts.cErrors;
      ZSendStatus( lpzft, FTST_UPDATE );
      return( ZERROR );
      }
   return( lpzft->rxtype );
}

int FASTCALL Z_GetHeader( LPZFT lpzft, char * nphdr, BOOL fAuto )
{
   register int c;
   DWORD n;
   int cancount;

   if( 0 == GetWindowText( GetFocus() , lpName, 18 ) )
      wsprintf( lpName, "%s", "Blank" );
   n = 1000;
   cancount = 5;
   lpzft->fUseCRC32 = FALSE;
again:
   if( fZmodemAbort || ( ( GetAsyncKeyState( VK_ESCAPE ) & 0x8000 ) && lstrcmp( lpName, "Connection Status" ) == 0 ) )
      {
      Z_SendCan( lpzft );
      return( ZCAN );
      }
   lpzft->rxframeind = lpzft->rxtype = 0;
   if( fAuto )
      goto from_auto;
   c = Z_TimedRead( lpzft );
   switch( c )
      {
      case ZPAD:
         break;

      case RCDO: case ZTIMEOUT:
      case ZCAN:
         goto done;

      case CAN:
         if( --cancount <= 0 ) {
            c = ZCAN;
            goto done;
            }

agn2: default:
         if( --n == 0 )
            {
            ++lpzft->fts.cErrors;
            ZSendStatus( lpzft, FTST_UPDATE );
            return( ZERROR );
            }
         if( c != CAN )
            cancount = 5;
         goto again;
      }
   cancount = 5;
splat:
   c = Z_TimedRead( lpzft );
   switch( c )
      {
      case ZDLE:  break;
      case ZPAD:  goto splat;
      case RCDO: case ZTIMEOUT: case ZCAN: goto done;
      default:    goto agn2;
      }
from_auto:
   fAuto = FALSE;
   c = Z_TimedRead( lpzft );
   switch( c )
      {
      case ZBIN32:
         lpzft->rxframeind = ZBIN32;
         c = Z_GetBinaryHeader32( lpzft, nphdr );
         break;
      case ZBIN:
         lpzft->rxframeind = ZBIN;
         c = Z_GetBinaryHeader( lpzft, nphdr );
         break;
      case ZHEX:
         lpzft->rxframeind = ZHEX;
         c = Z_GetHexHeader( lpzft, nphdr );
         break;
      case CAN:
         if( --cancount <= 0 ) {
            c = CAN;
            goto done;
            }
         goto agn2;
      case RCDO: case ZTIMEOUT: case ZCAN: goto done;
      default: goto agn2;
      }
   lpzft->rxpos = Z_PullLongFromHeader( nphdr );
done:
   return( c );
}

int FASTCALL RZ_ReceiveDa32( LPZFT lpzft, LPSTR buf, int blength )
{
   register int c, d, n;
   long crc;

   lpzft->fUseCRC32 = TRUE;
   crc = 0xFFFFFFFF;
   lpzft->rxcount = 0;
   while( TRUE )
      {
      if( ( c = Z_GetZDL( lpzft ) ) & 0xFF00 )
crcfoo:  switch( c )
            {
            case GOTCRCE: case GOTCRCG:
            case GOTCRCQ: case GOTCRCW:
               d = c;
               crc = AltCalculateCharacterCRC32( crc, (BYTE)( c & 0xFF ) );
               for( n = 0; n < 4; ++n ) {
                  if( ( c = Z_GetZDL( lpzft ) ) & 0xFF00 )
                     goto crcfoo;
                  crc = AltCalculateCharacterCRC32( crc, (BYTE)( c & 0xFF ) );
                  }
               if( crc != 0xDEBB20E3 )
                  {
               #ifdef ENABLE_DEBUGGING_INFO
                  WriteDebugMessage( lpzft, "Invalid CRC32 in %c frame (got 0x%x)", d, crc );
               #endif
                  ++lpzft->fts.cErrors;
                  ZSendStatus( lpzft, FTST_UPDATE );
                  return( ZERROR );
                  }
               return( d );

            case GOTCAN:
               return( ZCAN );

            case ZTIMEOUT:
            case RCDO:
               return( c );

            default:
               return( c );
            }
      if( --blength < 0 )
         return( ZERROR );
      buf[ lpzft->rxcount++ ] = (BYTE)( c & 0xFF );
      crc = AltCalculateCharacterCRC32( crc, (BYTE)( c & 0xFF ) );
      }
}

int FASTCALL RZ_ReceiveData( LPZFT lpzft, LPSTR buf, int blength )
{
   register int c, d;
   WORD crc;

   if( lpzft->rxframeind == ZBIN32 )
      return( RZ_ReceiveDa32( lpzft, buf, blength ) );
   crc = lpzft->rxcount = 0;
   while( TRUE ) {
      if( ( c = Z_GetZDL( lpzft ) ) & 0xFF00 )
crcfoo:  switch( c )
            {
            case GOTCRCE: case GOTCRCG:
            case GOTCRCQ: case GOTCRCW:
               d = c;
               crc = AltCalculateCharacterCRC16( crc, (BYTE)( c & 0xFF ) );
               if( ( c = Z_GetZDL( lpzft ) ) & 0xFF00 )
                  goto crcfoo;
               crc = AltCalculateCharacterCRC16( crc, (BYTE)( c & 0xFF ) );
               if( ( c = Z_GetZDL( lpzft ) ) & 0xFF00 )
                  goto crcfoo;
               crc = AltCalculateCharacterCRC16( crc, (BYTE)( c & 0xFF ) );
               if( crc != 0 )
                  {
               #ifdef ENABLE_DEBUGGING_INFO
                  WriteDebugMessage( lpzft, "Invalid CRC in %c frame (got 0x%x)", c, crc );
               #endif
                  ++lpzft->fts.cErrors;
                  ZSendStatus( lpzft, FTST_UPDATE );
                  return( ZERROR );
                  }
               return( d );

            case GOTCAN:
               return( ZCAN );

            case ZTIMEOUT:
            case RCDO:
               return( c );

            default:
               return( c );
            }
      if( --blength < 0 )
         return( ZERROR );
      buf[ lpzft->rxcount++ ] = (BYTE)( c & 0xFF );
      crc = AltCalculateCharacterCRC16( crc, (BYTE)( c & 0xFF ) );
      }
}

void FASTCALL RZ_AckBibi( LPZFT lpzft )
{
   register int n;

   Z_PutLongIntoHeader( lpzft, 0L );
   for( n = 0; n < 4; ++n )
      {
      SendBinaryHeader( lpzft, ZFIN, lpzft->txhdr );
      WriteDebugTxhdrDump( lpzft, "Sending ZFIN" );
      if( lpzft->return_status < ASSUCCESS )
         return;
      while( TRUE )
         {
         switch( Z_GetByte( lpzft, 100 ) )
            {
            case 'o':
            case 'O':
            #ifdef ENABLE_DEBUGGING_INFO
               WriteDebugMessage( lpzft, "Got and skipped OO sequence" );
            #endif
               Z_GetByte( lpzft, 1 );

            case ZTIMEOUT:
               return;

            default:
               break;
            }
         }
      }
}

int FASTCALL RZ_InitReceiver( LPZFT lpzft, BOOL fAuto )
{
   register int c, n;

   memset( lpzft->attn, 0, sizeof( lpzft->attn ) );
   lpzft->fts.cErrors = 0;
   for( n = 10; --n >= 0; )
      {
      Z_PutLongIntoHeader( lpzft, 0L );
      lpzft->txhdr[ ZF0 ] = CANFDX | CANOVIO | CANFC32 | CANBRK;
      WriteDebugTxhdrDump( lpzft, "Sending ZRINIT to host" );
      SendHexHeader( lpzft, lpzft->tryzhdrtype, lpzft->txhdr );
      if( lpzft->tryzhdrtype == ZSKIP )
         lpzft->tryzhdrtype = ZRINIT;
again:
      c = Z_GetHeader( lpzft, lpzft->rxhdr, fAuto );
      fAuto = FALSE;
      ZWriteFrame( lpzft, c );
      switch( c )
         {
         case ZFILE:
            lpzft->zconv = lpzft->rxhdr[ ZF0 ] | ZCRESUM;
            lpzft->tryzhdrtype = ZRINIT;
            c = RZ_ReceiveData( lpzft, lpzft->secbuf, ZMAXSPLEN );
            if( c == GOTCRCW )
               return( ZFILE );
            WriteDebugTxhdrDump( lpzft, "ZFILE received, responding with ZNAK." );
            SendHexHeader( lpzft, ZNAK, lpzft->txhdr );
            goto again;

         case ZSINIT:
            c = RZ_ReceiveData( lpzft, lpzft->attn, ZATTNLEN );
            if( c == GOTCRCW )
               {
               WriteDebugTxhdrDump( lpzft, "ZSINIT received, responding with ZACK." );
               SendHexHeader( lpzft, ZACK, lpzft->txhdr );
               }
            else
               {
               WriteDebugTxhdrDump( lpzft, "ZSINIT received, responding with ZNAK." );
               SendHexHeader( lpzft, ZNAK, lpzft->txhdr );
               }
            goto again;

         case ZFREECNT:
            Z_PutLongIntoHeader( lpzft, Amdir_FreeDiskSpace( lpzft->szTargetDir ) );
            WriteDebugTxhdrDump( lpzft, "ZFREECNT received." );
            SendHexHeader( lpzft, ZACK, lpzft->txhdr );
            goto again;

         case ZCOMMAND:
            c = RZ_ReceiveData( lpzft, lpzft->secbuf, ZMAXSPLEN );
            if( c = GOTCRCW )
               {
               int errors;

               errors = 0;
               Z_PutLongIntoHeader( lpzft, 0 );
               do {
                  WriteDebugTxhdrDump( lpzft, "ZCOMMAND received, responding with ZCOMPL." );
                  SendHexHeader( lpzft, ZCOMPL, lpzft->txhdr );
                  ++errors;
                  }
               while( errors <= 10 && Z_GetHeader( lpzft, lpzft->rxhdr, FALSE ) != ZFIN );
               RZ_AckBibi( lpzft );
               return( ZOK );
               }
            WriteDebugTxhdrDump( lpzft, "ZCOMMAND received, responding with ZNAK." );
            SendHexHeader( lpzft, ZNAK, lpzft->txhdr );
            goto again;

         case ZCOMPL:
         case ZFIN:
            return( ZCOMPL );

         case ZCAN:
         case RCDO:
            return( c );
         }
      }
   ZMessage( lpzft, "Timeout" );
   return( ZERROR );
}

int FASTCALL RZ_GetHeader( LPZFT lpzft )
{
   char szNewName[ _MAX_PATH ];
   char sz[ _MAX_PATH ];
   char s[ 255 ];
   BOOL fReplace;
   FINDDATA ft;
   HFIND hFind;
   char * ps;
   int p, n;

   /* Read the input buffer into a local buffer.
    */
   ps = s;
   for( p = 0; p < 255 && lpzft->secbuf[ p ]; ++p )
      *ps++ = toupper( lpzft->secbuf[ p ] );
   *ps = '\0';
   ++p;

   /* Scan back to the file basename.
    */
   for( n = p - 1; n >= 0; --n )
      if( s[ n ] == ':' || s[ n ] == '\\' )
         break;
   strcpy( lpzft->fts.szFilename, s + ( n + 1 ) );

   /* Read off the file size.
    */
   lpzft->fts.dwTotalSize = 0;
   while( p < ZMAXSPLEN && lpzft->secbuf[ p ] != ' ' && lpzft->secbuf[ p ] )
      {
      lpzft->fts.dwTotalSize = ( lpzft->fts.dwTotalSize * 10 ) + ( lpzft->secbuf[ p ] - 0x30 );
      ++p;
      }
   lpzft->frcvsize = lpzft->fts.dwTotalSize;
   ++p;

   /* Read off the Unix date of the time.
    */
   ps = s;
   while( p < ZMAXSPLEN && lpzft->secbuf[ p ] >= 0x30 && lpzft->secbuf[ p ] <= 0x37 )
      {
      *ps++ = lpzft->secbuf[ p ];
      ++p;
      }
   *ps = '\0';
   ++p;
   lpzft->ftime = Z_FromUnixDate( s );

   /* Create the local filename. If a local target name is supplied, use
    * that. Otherwise use the name that the remote sent us.
    */
   strcpy( sz, lpzft->szTargetDir );
   if( lpzft->szTargetFname[ 0 ] && !IsWild( lpzft->szTargetFname ) )
      {
      strcat( sz, lpzft->szTargetFname );
      strcpy( lpzft->fts.szFilename, lpzft->szTargetFname );
      }
   else
      {
      if( !Ameol2_MakeLocalFileName( GetFocus(), lpzft->fts.szFilename, sz + strlen( sz ), TRUE ) )
         {
      #ifdef ENABLE_DEBUGGING_INFO
         WriteDebugMessage( lpzft, "Sending ZSKIP because filename could not be converted" );
      #endif
         return( ZSKIP );
         }
      }
#ifdef ENABLE_DEBUGGING_INFO
   WriteDebugMessage( lpzft, "Received file details: Filename=%s, size=%lu", sz, lpzft->frcvsize );
#endif

   /* Reinitialise the status dialog.
    */
   ZSendStatus( lpzft, FTST_BEGINRECEIVE );
   fReplace = TRUE;
   if( ( hFind = Amuser_FindFirst( sz, _A_NORMAL, &ft ) ) != -1 )
      {
      LONG dtime = 0L;
      WORD date;
      WORD time;

      if( ( lpzft->fh = Amfile_Open( sz, AOF_WRITE ) ) != HNDFILE_ERROR )
         {
      #ifdef ENABLE_DEBUGGING_INFO
         WriteDebugMessage( lpzft, "File exists, but can't open file!" );
      #endif
         Amfile_GetFileTime( lpzft->fh, &date, &time );
         dtime = MAKELONG( time, date );
         Amfile_Close( lpzft->fh );
         lpzft->fh = HNDFILE_ERROR;
         }
      if( ( lpzft->zconv & ZCRESUM ) && lpzft->fts.dwTotalSize > (DWORD)ft.size && lpzft->ftime == dtime )
         {
         lpzft->filestart = ft.size;
         if( ( lpzft->fh = Amfile_Open( sz, AOF_WRITE ) ) == HNDFILE_ERROR )
            {
            ZMessage( lpzft, "Error opening file" );
            return( ZERROR );
            }
         if( Amfile_SetPosition( lpzft->fh, ft.size, ASEEK_BEGINNING ) == -1L )
            {
            ZMessage( lpzft, "Error positioning file" );
            return( ZERROR );
            }
         ZMessage( lpzft, "Recovering" );
         lpzft->frcvsize = lpzft->fts.dwTotalSize - lpzft->filestart;
         lpzft->fts.dwSizeSoFar = lpzft->filestart;
         fReplace = FALSE;
      #ifdef ENABLE_DEBUGGING_INFO
         WriteDebugMessage( lpzft, "Attempting to recover file: New offset=%lu", lpzft->filestart );
      #endif
         }
      else if( lpzft->ftime == dtime && lpzft->fts.dwTotalSize == (DWORD)ft.size )
         {
         if( lpzft->wFlags == ZDF_PROMPT )
            lpzft->wFlags = ZSendStatus( lpzft, FTST_QUERYEXISTING );
         if( lpzft->wFlags & ZDF_OVERWRITE )
            fReplace = TRUE;
         else if( lpzft->wFlags & ZDF_RENAME )
            {
            char * pExt;
            char ch;

            strcpy( szNewName, sz );
            pExt = szNewName + strlen( szNewName ) - 1;
            if( *pExt == '.' )
               --pExt;
            for( ch = '0'; ch <= '9'; ++ch )
               {
               *pExt = ch;
               if( !Amfile_QueryFile( szNewName ) )
                  break;
               }
            Amfile_Rename( sz, szNewName );
            fReplace = TRUE;
            }
         else
            {
            ZMessage( lpzft, "File Complete" );
            Z_ShowLoc( lpzft, lpzft->fts.dwTotalSize );
            return( ZSKIP );
            }
         }
      Amuser_FindClose( hFind );
      }
   if( fReplace )
      {
      lpzft->filestart = 0;
      if( ( lpzft->fh = Amfile_Create( sz, 0 ) ) == HNDFILE_ERROR )
         {
         if( !Amdir_Create( lpzft->szTargetDir ) )
            return( ZERROR );
         if( ( lpzft->fh = Amfile_Create( sz, 0 ) ) == HNDFILE_ERROR )
            return( ZERROR );
         }
      Amfile_SetFileTime( lpzft->fh, HIWORD( lpzft->ftime ), LOWORD( lpzft->ftime ) );
      }
   Z_ShowLoc( lpzft, 0 );
   return( ZOK );
}

/* This function writes the receive buffer to disk.
 */
int FASTCALL RZ_SaveToDisk( LPZFT lpzft, LONG * nprxbytes )
{
   UINT cWritten;

   if( 0 == GetWindowText( GetFocus() , lpName, 18 ) )
      wsprintf( lpName, "%s", "Blank" );
   if( fZmodemAbort || ( ( GetAsyncKeyState( VK_ESCAPE ) & 0x8000 ) && lstrcmp( lpName, "Connection Status" ) == 0 ) )
      {
      ZMessage( lpzft, "Aborted from keyboard" );
      Z_SendCan( lpzft );
      return( ZERROR );
      }
   if( ( cWritten = Amfile_Write( lpzft->fh, lpzft->secbuf, lpzft->rxcount ) ) != (UINT)lpzft->rxcount )
      {
   #ifdef ENABLE_DEBUGGING_INFO
      WriteDebugMessage( lpzft, "Error writing data to disk: Size = %lu, Written=%lu", lpzft->rxcount, cWritten );
   #endif
      ZMessage( lpzft, "Disk write error" );
      return( ZERROR );
      }
   *nprxbytes = *nprxbytes + lpzft->rxcount;
   lpzft->fts.dwBytesRead += lpzft->rxcount;
   lpzft->fts.dwSizeSoFar += lpzft->rxcount;
   lpzft->dwTotal += lpzft->rxcount;
   return( ZOK );
}

int FASTCALL RZ_ReceiveFile( LPZFT lpzft )
{
   register int c, n;
   long rxbytes;

   lpzft->fts.cErrors = 0;
   ZSendStatus( lpzft, FTST_UPDATE );
   lpzft->dwStartTick = GetTickCount();
   lpzft->fts.dwBytesRead = 0;
   lpzft->fts.dwSizeSoFar = 0;
   lpzft->dwTotal = 0;
   if( ( c = RZ_GetHeader( lpzft ) ) != ZOK )
      {
      if( c == ZSKIP )
         lpzft->tryzhdrtype = ZSKIP;
      WriteDebugTxhdrDump( lpzft, "Couldn't get header. File may be skipped" );
      return( c );
      }
   c = ZOK;
   n = 10;
   rxbytes = lpzft->rxpos = lpzft->filestart;
   do {
      Z_PutLongIntoHeader( lpzft, rxbytes );
      SendHexHeader( lpzft, ZRPOS, lpzft->txhdr );
      WriteDebugTxhdrDump( lpzft, "Sending ZRPOS to remote." );
nxthdr:
      c = Z_GetHeader( lpzft, lpzft->rxhdr, FALSE );
      ZWriteFrame( lpzft, c );
      switch( c ) {
         case ZDATA:
            if( lpzft->rxpos != rxbytes ) {
               --n;
               ++lpzft->fts.cErrors;
               ZSendStatus( lpzft, FTST_UPDATE );
               if( n < 0 )
                  {
               #ifdef ENABLE_DEBUGGING_INFO
                  WriteDebugMessage( lpzft, "ZDATA with bad position received. Aborting this file." );
               #endif
                  goto err;
                  }
               ZMessage( lpzft, "Bad position" );
               Z_PutString( lpzft, lpzft->attn );
            #ifdef ENABLE_DEBUGGING_INFO
               WriteDebugMessage( lpzft, "ZDATA with bad position received. Sending attention string." );
            #endif
               }
            else {
moredata:      c = RZ_ReceiveData( lpzft, lpzft->secbuf, ZMAXSPLEN );
               switch( c ) {
                  case ZCAN:
                  case RCDO:
                     WriteDebugTxhdrDump( lpzft, "ZCAN/RCDO during data receipt. Aborting this file." );
                     goto err;

                  case ZERROR:
                     ++lpzft->fts.cErrors;
                     ZSendStatus( lpzft, FTST_UPDATE );
                     if( --n < 0 )
                        goto err;
                     Z_PutString( lpzft, lpzft->attn );
                     WriteDebugTxhdrDump( lpzft, "ZERROR during data receipt. Sending attention string." );
                     break;

                  case ZTIMEOUT:
                     if( --n < 0 )
                        {
                        WriteDebugTxhdrDump( lpzft, "ZTIMEOUT during data receipt. Aborting this file." );
                        goto err;
                        }
                     break;

                  case GOTCRCW:
                     n = 10;
                     c = RZ_SaveToDisk( lpzft, &rxbytes );
                     if( c != ZOK )
                        return( c );
                     Z_ShowLoc( lpzft, rxbytes );
                     Z_PutLongIntoHeader( lpzft, rxbytes );
                     WriteDebugTxhdrDump( lpzft, "GOTCRCW received. Sending next position." );
                     SendHexHeader( lpzft, ZACK, lpzft->txhdr );
                     goto nxthdr;

                  case GOTCRCQ:
                     n = 10;
                     c = RZ_SaveToDisk( lpzft, &rxbytes );
                     if( c != ZOK )
                        return( c );
                     Z_ShowLoc( lpzft, rxbytes );
                     Z_PutLongIntoHeader( lpzft, rxbytes );
                     WriteDebugTxhdrDump( lpzft, "GOTCRCQ received. Sending next position." );
                     SendHexHeader( lpzft, ZACK, lpzft->txhdr );
                     goto moredata;

                  case GOTCRCG:
                     n = 10;
                     c = RZ_SaveToDisk( lpzft, &rxbytes );
                     if( c != ZOK )
                        return( c );
                     Z_ShowLoc( lpzft, rxbytes );
                  #ifdef ENABLE_DEBUGGING_INFO
                     WriteDebugMessage( lpzft, "GOTCRCG received." );
                  #endif
                     goto moredata;

                  case GOTCRCE:
                     n = 10;
                     c = RZ_SaveToDisk( lpzft, &rxbytes );
                     if( c != ZOK )
                        return( c );
                  #ifdef ENABLE_DEBUGGING_INFO
                     WriteDebugMessage( lpzft, "GOTCRCE received." );
                  #endif
                     Z_ShowLoc( lpzft, rxbytes );
                     goto nxthdr;
                  }
               }
            break;

         case ZNAK:
         case ZTIMEOUT:
            if( --n < 0 )
               {
            #ifdef ENABLE_DEBUGGING_INFO
               WriteDebugMessage( lpzft, "ZNAK/ZTIMEOUT received. Aborting this file." );
            #endif
               goto err;
               }
            Z_ShowLoc( lpzft, rxbytes );
         #ifdef ENABLE_DEBUGGING_INFO
            WriteDebugMessage( lpzft, "ZNAK/ZTIMEOUT received. Still waiting..." );
         #endif
            break;

         case ZFILE:
         #ifdef ENABLE_DEBUGGING_INFO
            WriteDebugMessage( lpzft, "ZFILE received." );
         #endif
            c = RZ_ReceiveData( lpzft, lpzft->secbuf, ZMAXSPLEN );
            break;

         case ZEOF:
            if( lpzft->rxpos == rxbytes )
               {
            #ifdef ENABLE_DEBUGGING_INFO
               WriteDebugMessage( lpzft, "ZEOF received and all done" );
            #endif
               return( c );
               }
         #ifdef ENABLE_DEBUGGING_INFO
            WriteDebugMessage( lpzft, "ZEOF received but not all done yet" );
         #endif
            goto nxthdr;

         case ZERROR:
            if( --n < 0 )
               {
            #ifdef ENABLE_DEBUGGING_INFO
               WriteDebugMessage( lpzft, "ZERROR received. Aborting this file." );
            #endif
               goto err;
               }
            Z_ShowLoc( lpzft, rxbytes );
            Z_PutString( lpzft, lpzft->attn );
         #ifdef ENABLE_DEBUGGING_INFO
            WriteDebugMessage( lpzft, "ZERROR received. Sending attention string." );
         #endif
            break;

         default:
         #ifdef ENABLE_DEBUGGING_INFO
            WriteDebugMessage( lpzft, "Unknown frame type received (%u). Aborting this file.", c );
         #endif
            c = ZERROR;
            goto err;
         }
      }
   while( TRUE );
err:
   return( ZERROR );
}

int FASTCALL RZ_ReceiveBatch( LPZFT lpzft )
{
   register int c;

   while( TRUE )
      {
      /* Receive the data for the file that has been opened.
       */
      c = RZ_ReceiveFile( lpzft );
      ZWriteFrame( lpzft, c );

      /* Close the file if it was opened.
       */
      if( HNDFILE_ERROR != lpzft->fh )
         {
         Amfile_SetFileTime( lpzft->fh, HIWORD( lpzft->ftime ), LOWORD( lpzft->ftime ) );
         Amfile_Close( lpzft->fh );
         lpzft->fh = HNDFILE_ERROR;
         }

      /* Deal with the next block.
       */
      switch( c ) {
         case ZEOF:
         case ZSKIP:
            c = RZ_InitReceiver( lpzft, FALSE );
            ZWriteFrame( lpzft, c );
            switch( c )
               {
               case ZFILE:
               #ifdef ENABLE_DEBUGGING_INFO
                  WriteDebugMessage( lpzft, "ZFILE received. Gearing up for next file." );
               #endif
                  break;

               case ZCOMPL:
               #ifdef ENABLE_DEBUGGING_INFO
                  WriteDebugMessage( lpzft, "ZCOMPL received. Tidying up and saying bye-bye" );
               #endif
                  RZ_AckBibi( lpzft );
                  return( ZOK );

               default:
               #ifdef ENABLE_DEBUGGING_INFO
                  WriteDebugMessage( lpzft, "Unknown frame (%u) received by RZ_ReceiveBatch", c );
               #endif
                  return( ZERROR );
               }
            break;

         default:
            return( c );
         }
      }
}

/* Handles a ZModem file reception.
 */
BOOL WINAPI EXPORT Aftp_Receive( LPCOMMDEVICE lpcdev, LPFFILETRANSFERSTATUS lpffts, HWND hwnd, LPSTR lpszPath, BOOL fAuto, WORD wDownloadFlags )
{
   LPZFT lpzft;
   register int i;
   BOOL fOk;

   /* First create the ZMODEM file transfer
    * structure.
    */
   if( NULL == ( lpzft = ZModemCreateFTS() ) )
      return( FALSE );
   fNewMemory( &lpName, LEN_TEMPBUF );
   lpzft->lpcdev = lpcdev;
   lpzft->lpffts = lpffts;
   lpzft->fts.hwndOwner = hwnd;
   lpzft->fh = HNDFILE_ERROR;
   fZmodemAbort = FALSE;

   /* Open the debug file.
    */
   OpenDebugFile( lpzft );

   /* Suspend the callback for this comms session.
    */
   Amcomm_SetDataLogging( lpzft->lpcdev, FALSE );
   Amcomm_SetCallback( lpzft->lpcdev, FALSE );
   lpzft->fExpandLFtoCRLF = Amcomm_GetBinary( lpzft->lpcdev );

   /* The default download directory and filename. Setting szTargetFname to an
    * empty string tells us to use the host system filename.
    */
   lpzft->wFlags = wDownloadFlags;
   lpzft->fts.cErrors = 0;
   lpzft->fts.wPercentage = 0;
   lpzft->fts.szLastMessage[ 0 ] = '\0';

   /* Check whether the input parameter lpszPath specifies just a
    * directory or a complete filename. If it's a directory, then the
    * sending device supplies the filename. If it is a filename, then
    * this name overrides the one supplied by the sending device.
    */
   Amuser_GetDownloadsDirectory( lpzft->szTargetDir, _MAX_PATH );
   lpzft->szTargetFname[ 0 ] = '\0';
   if( lpszPath )
      {
      BOOL fIsDirectory;
      struct _stat buf;

      /* Test whether lpszPath is a directory.
       */
      fIsDirectory = FALSE;
      if( _stat( lpszPath, &buf ) == 0 )
         if( buf.st_mode & _S_IFDIR )
            {
            strcpy( lpzft->szTargetDir, lpszPath );
            fIsDirectory = TRUE;
            }

      /* lpszPath was a filename and may include a pathname (and a
       * drive letter). Strip off the filename into szTargetFname, and
       * the pathname into szTargetDir.
       */
      if( !fIsDirectory )
         {
         register int c;

         for( c = strlen( lpszPath ) - 1; c >= 0; --c )
            if( lpszPath[ c ] == '\\' || lpszPath[ c ] == ':' )
               break;
         strcpy( lpzft->szTargetFname, &lpszPath[ c + 1 ] );
         if( c > 0 )
            {
            strncpy( lpzft->szTargetDir, lpszPath, c );
            lpzft->szTargetDir[ c ] = '\0';
            }
         }
      }

   /* If a target directory was supplied, ensure it ends with '\'
    */
   if( lpzft->szTargetDir[ 0 ] && lpzft->szTargetDir[ strlen( lpzft->szTargetDir ) - 1 ] != '\\' )
      strcat( lpzft->szTargetDir, "\\" );
#ifdef ENABLE_DEBUGGING_INFO
   WriteDebugMessage( lpzft, "Target directory is %s", lpzft->szTargetDir );
#endif

   /* Ready to receive.
    */
   ZSendStatus( lpzft, FTST_DOWNLOAD );
   lpzft->rxtimeout = 100;
   lpzft->tryzhdrtype = ZRINIT;
   i = RZ_InitReceiver( lpzft, fAuto );
   ZWriteFrame( lpzft, i );
   fOk = FALSE;
   if( i == ZCOMPL )
      fOk = TRUE;
   else if( i == ZFILE )
      if( ( i = RZ_ReceiveBatch( lpzft ) ) == ZOK )
         fOk = TRUE;
   if( !fOk )
      {
   #ifdef ENABLE_DEBUGGING_INFO
      WriteDebugMessage( lpzft, "Receive not AOK, so sending cancel string" );
   #endif
      Z_SendCan( lpzft );
      if( i == ZSKIP )
         fOk = TRUE;
      }

   /* Send the final status report.
    */
   ZSendStatus( lpzft, fOk ? FTST_COMPLETED : FTST_FAILED );

   /* Close the debug file.
    */
   CloseDebugFile( lpzft );

   /* Restore the callback for this comms session.
    */
   Amcomm_SetCallback( lpzft->lpcdev, TRUE );
   Amcomm_SetDataLogging( lpzft->lpcdev, TRUE );
   FreeMemory( &lpzft );
   if( NULL != lpName )
      FreeMemory( &lpName );
   return( fOk );
}


/* This function sends the specified message to the status
 * callback function.
 */
void FASTCALL ZMessage( LPZFT lpzft, char * pszMessage )
{
   strcpy( lpzft->fts.szLastMessage, pszMessage );
   ZSendStatus( lpzft, FTST_UPDATE );
}

/* This function writes the description of the specified frame type to
 * the frame type field of the ZModem status dialog.
 */
void FASTCALL ZWriteFrame( LPZFT lpzft, int n )
{
   register char * psz;
   char sz[ 40 ];

   psz = "";
   if( n >= ZCRCE && n <= ZCRCW ) {
      switch( n ) {
         case ZCRCE: psz = "ZCRCE"; break;
         case ZCRCG: psz = "ZCRCG"; break;
         case ZCRCQ: psz = "ZCRCQ"; break;
         case ZCRCW: psz = "ZCRCW"; break;
         }
      }
   else if( n < -3 || n > 20 )
      wsprintf( psz = sz, "ZUNKNOWN (%d)", n );
   else switch( n ) {
      case -3:    psz = "ZNOCARRIER";  break;
      case -2:    psz = "ZTIMEOUT"; break;
      case -1:    psz = "ZERROR"; break;
      case 0:     psz = "ZRQINIT"; break;
      case 1:     psz = "ZRINIT"; break;
      case 2:     psz = "ZSINIT"; break;
      case 3:     psz = "ZACK"; break;
      case 4:     psz = "ZFILE"; break;
      case 5:     psz = "ZSKIP"; break;
      case 6:     psz = "ZNAK"; break;
      case 7:     psz = "ZABORT"; break;
      case 8:     psz = "ZFIN"; break;
      case 9:     psz = "ZRPOS"; break;
      case 10:    psz = "ZDATA"; break;
      case 11:    psz = "ZEOF"; break;
      case 12:    psz = "ZFERR"; break;
      case 13:    psz = "ZCRC"; break;
      case 14:    psz = "ZCHALLENGE"; break;
      case 15:    psz = "ZCOMPL"; break;
      case 16:    psz = "ZCAN"; break;
      case 17:    psz = "ZFREECNT"; break;
      case 18:    psz = "ZCOMMAND"; break;
      case 19:    psz = "ZSTDERR"; break;
      default:    psz = "ZUNKNOWN"; break;
      }
   strcpy( lpzft->fts.szLastMessage, psz );
   ZSendStatus( lpzft, FTST_UPDATE );
}

/* This function calls the callback with the specified status
 * information.
 */
int FASTCALL ZSendStatus( LPZFT lpzft, int wStatus )
{
   lpzft->fts.wStatus = wStatus;
   return( lpzft->lpffts( &lpzft->fts ) );
}

/* This function converts a year from a 2-digit to a 4-digit
 * value. (ie. 80 to 1980, 99 to 1999, 04 to 2004)
 * Amended by pw 6/5/1999 as gmtime/localtime etc return
 * ( year - 1900 ) i.e. '101' for 2001.
 */
WORD FASTCALL FixupYear( WORD iYear )
{
   if( iYear < 100 )
      {
      if( iYear < 80 )
         iYear += 2000;
      else
         iYear += 1900;
      }
   else if( iYear < 1000 )
      iYear += 1900;
   return( iYear );
}

/* This function pauses for the specified number of
 * milliseconds.
 */
void FASTCALL GoToSleep( DWORD dwMilliseconds )
{
#ifdef WIN32
   Sleep( dwMilliseconds );
#else
   DWORD dwDelay;

   dwDelay = GetTickCount();
   while( ( GetTickCount() - dwDelay ) < dwMilliseconds );
#endif
}

/* This function checks whether the specified filename
 * contains wildcard characters.
 */
BOOL FASTCALL IsWild( char * pFilename )
{
   while( *pFilename )
      {
      if( *pFilename == '?' || *pFilename == '*' )
         return( TRUE );
      ++pFilename;
      }
   return( FALSE );
}

/* This function outputs the contents of the txhdr buffer after the
 * specified message.
 */
#ifdef ENABLE_DEBUGGING_INFO
void FASTCALL WriteDebugTxhdrDump( LPZFT lpzft, char * p )
{
   char sz[ 160 ];

   wsprintf( sz, "%s: TxHdr=%2.2x %2.2x %2.2x %2.2x", p, lpzft->txhdr[ 0 ], lpzft->txhdr[ 1 ],
             lpzft->txhdr[ 2 ], lpzft->txhdr[ 3 ] );
   WriteDebugMessage( lpzft, sz );
}

/* ZMODEM Debug function. Formats and prints the specified
 * message to the debug file.
 */
void CDECL WriteDebugMessage( LPZFT lpzft, char *msg, ... )
{
   char szOutLF[ 512 ];

   ASSERT( strlen(msg) + 2 < sizeof(szOutLF) );

   /* If we've got a trace log file handle, write the string
    * to the file.
    */
   if( lpzft->pFile != HNDFILE_ERROR )
      {
      va_list argptr;

      va_start(argptr, msg);
      wvsprintf( szOutLF, msg, argptr );
      va_end(argptr);
      Amfile_Write( lpzft->pFile, szOutLF, strlen(szOutLF) );
      Amfile_Write( lpzft->pFile, "\r\n", 2 );
      }
}

/* Open the debug log file.
 */
void FASTCALL OpenDebugFile( LPZFT lpzft )
{
   if( HNDFILE_ERROR != ( lpzft->pFile = Amfile_Open( "ZMODEM.LOG", AOF_WRITE ) ) )
      Amfile_SetPosition( lpzft->pFile, 0L, ASEEK_END );
   else if( HNDFILE_ERROR != ( lpzft->pFile = Amfile_Create( "ZMODEM.LOG", 0 ) ) )
      WriteDebugMessage( lpzft, "** ZMODEM log file opened" );
}

/* Close the debug log file
 */
void FASTCALL CloseDebugFile( LPZFT lpzft )
{
   if( HNDFILE_ERROR != lpzft->pFile )
      {
      WriteDebugMessage( lpzft, "** ZMODEM log file closed\r\n" );
      Amfile_Close( lpzft->pFile );
      lpzft->pFile = HNDFILE_ERROR;
      }
}
#endif
