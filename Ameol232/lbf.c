/* LBF.C - Line read/write I/O functions
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
#include "amuser.h"
#include "pdefs.h"
#include "amlib.h"
#include "lbf.h"

typedef struct tagLBF {
   LPSTR lpBuffer;         /* Points to start of input buffer */
   LPSTR lpBufPtr;         /* Current pointer into input buffer */
   HNDFILE fh;             /* File handle */
   WORD wType;             /* Buffer open mode */
   WORD szBuf;             /* Allocated size of buffer */
   DWORD lszBuf;           /* Size of file */
   DWORD dwAccum;          /* Count of characters read/written so far */
   WORD wTime;             /* Output file time */
   WORD wDate;             /* Output file date */
   WORD nError;            /* Any error code */
} LBF;

typedef LBF FAR * LPLBF;

/* This function initalises the input buffer to 0xFF00 bytes, and sets
 * the first byte of the buffer to NULL which triggers the next read of
 * the buffer to call Amlbf_Read().
 */
LPLBF FASTCALL Amlbf_Open( HNDFILE fh, int wType )
{
   LPLBF lpBuf;

   INITIALISE_PTR(lpBuf);
   if( fNewMemory( &lpBuf, sizeof( LBF ) ) )
      {
      INITIALISE_PTR(lpBuf->lpBuffer);
      if( wType == AOF_READ )
         {
         lpBuf->lszBuf = Amfile_GetFileSize( fh );
         lpBuf->szBuf = (unsigned)min( lpBuf->lszBuf, 0x0000FF00L ) + 1;
         }
      else if( wType == AOF_WRITE )
         {
         lpBuf->lszBuf = 0xFF00L;
         lpBuf->szBuf = (WORD)lpBuf->lszBuf;
         }
      if( fNewMemory( &lpBuf->lpBuffer, lpBuf->szBuf ) )
         {
         lpBuf->lpBufPtr = lpBuf->lpBuffer;
         *lpBuf->lpBuffer = '\0';
         }
      lpBuf->fh = fh;
      lpBuf->wType = wType;
      lpBuf->dwAccum = 0L;
      lpBuf->wDate = 0;
      lpBuf->wTime = 0;
      lpBuf->nError = LBF_NO_ERROR;
      }
   return( (LPLBF)lpBuf );
}

/* This function closes a line buffer
 */
void FASTCALL Amlbf_Close( LPLBF lpBufVoid )
{
   LPLBF lpBuf = (LPLBF)lpBufVoid;

   if( lpBuf->wType == AOF_WRITE )
      if( lpBuf->dwAccum )
         Amfile_Write( lpBuf->fh, (LPCSTR)lpBuf->lpBuffer, LOWORD( lpBuf->dwAccum ) );
   if( lpBuf->wTime || lpBuf->wDate )
      Amfile_SetFileTime( lpBuf->fh, lpBuf->wDate, lpBuf->wTime );
   Amfile_Close( lpBuf->fh );
   FreeMemory( &lpBuf->lpBuffer );
   FreeMemory( &lpBuf );
}

/* This function writes one line from user buffer to the output file.
 */
BOOL FASTCALL Amlbf_Write( LPLBF lpBufVoid, LPSTR lpUsrBuf, int cbMaxToWrite )
{
   LPLBF lpBuf = (LPLBF)lpBufVoid;

   if( lpBuf->dwAccum + cbMaxToWrite > lpBuf->szBuf )
      {
      if( Amfile_Write( lpBuf->fh, lpBuf->lpBuffer, (WORD)lpBuf->dwAccum ) != lpBuf->dwAccum )
         return( FALSE );
      lpBuf->dwAccum = 0;
      lpBuf->lpBufPtr = lpBuf->lpBuffer;
      }
   if( (UINT)cbMaxToWrite > lpBuf->szBuf )
      return( FALSE );
   while( cbMaxToWrite-- )
      {
      *lpBuf->lpBufPtr++ = *lpUsrBuf++;
      ++lpBuf->dwAccum;
      }
   return( TRUE );
}

/* This function reads one line from the scratchpad to a specified buffer.
 * A line is assumed to be delimited by CR, LF or CR followed by LF.
 */
BOOL FASTCALL Amlbf_Read( LPLBF lpBufVoid, LPSTR lpUsrBuf, int cbMaxToRead, int * npiLen, BOOL * npfContinued, BOOL * pIsAmeol2Scratchpad )
{
   register int c;
   register int length;
   LPLBF lpBuf = (LPLBF)lpBufVoid;
   static BOOL fSeenEOL = TRUE;
   BOOL fEof = FALSE;
   BOOL fBof = FALSE;

   c = 0;
   if( npfContinued )
      *npfContinued = !fSeenEOL;
   fSeenEOL = FALSE;
   fBof = Amfile_GetPosition( lpBuf->fh ) == 0;
   while( *lpBuf->lpBufPtr != 13 && *lpBuf->lpBufPtr != 10 && c < cbMaxToRead - 1 )
   {
      if( *lpBuf->lpBufPtr == '\0' )
      {
         register int cRead;

         if( ( cRead = Amfile_Read( lpBuf->fh, lpBuf->lpBuffer, lpBuf->szBuf - 1 ) ) == -1 )
         {
         /* An error occurred when reading from the file.
            */
            lpBuf->lpBuffer[ 0 ] = '\0';
            lpBuf->nError = LBF_READ_ERROR;
            fEof = TRUE;
            break;
         }
         if( 0 == cRead && c > 0 )
            break;
         lpBuf->lpBufPtr = lpBuf->lpBuffer;
         lpBuf->lpBufPtr[ cRead ] = '\0';
         if( *lpBuf->lpBufPtr == '\0' )
         {
            fEof = TRUE;
            break;
         }
         continue;
      }
      if( c < cbMaxToRead - 1 )
         lpUsrBuf[ c++ ] = *lpBuf->lpBufPtr;
      ++lpBuf->lpBufPtr;
      ++lpBuf->dwAccum;
   }
   length = c;

   if( fBof ) 
   {
      *pIsAmeol2Scratchpad = ( *lpBuf->lpBufPtr == 13 );
   }

   if( *pIsAmeol2Scratchpad ) // !!SM!! 2.55.2059  This is an Ameol2 scratchpad / topic file, CR/LF on each line
   {
      if( *lpBuf->lpBufPtr == 13 )
      {
         if( *( lpBuf->lpBufPtr + 1 ) == 10 ) {
            ++lpBuf->lpBufPtr;
            ++lpBuf->dwAccum;
            ++length;
            fSeenEOL = TRUE;
         }
      }
      if( *lpBuf->lpBufPtr == 13 || *lpBuf->lpBufPtr == 10 ) {
         ++lpBuf->lpBufPtr;
         ++lpBuf->dwAccum;
         ++length;
         fSeenEOL = TRUE;
      }
   }
   else // !!SM!! 2.55.2059  This is a fresh CIX scratchpad, No CR's, just LF's
   {
      // !!SM!! 2.55.2059 If there is a CR, it shouldn't be here, so swap it for a LF, but count it.
      if( *lpBuf->lpBufPtr == 13 )
      {
         *lpBuf->lpBufPtr = 10; 
         ++lpBuf->lpBufPtr;
         ++lpBuf->dwAccum;
         ++length;
      }
      if( *lpBuf->lpBufPtr == 10 ) {
         ++lpBuf->lpBufPtr;
         ++lpBuf->dwAccum;
         ++length;
         fSeenEOL = TRUE;
      }
   }

/*
   if(pParseFlags & PARSEFLG_IGNORECR) // Ameol Scratchpad 
   {
      // Since we've added the CR, ignore it.
      if( *lpBuf->lpBufPtr == 13 )
      {
         if( *( lpBuf->lpBufPtr + 1 ) == 10 ) {
            ++lpBuf->lpBufPtr;
            ++lpBuf->dwAccum;
            ++length;
            fSeenEOL = TRUE;
         }
      }
      if( *lpBuf->lpBufPtr == 13 || *lpBuf->lpBufPtr == 10 ) {
         ++lpBuf->lpBufPtr;
         ++lpBuf->dwAccum;
         ++length;
         fSeenEOL = TRUE;
      }
   }
   else // CIX Scratchpad // FS#83 2.55.2051  
   {
      // CIX Scratchpads shouldn't have CR's in them, but if they do, we need to count the CR's.
      if( ( *lpBuf->lpBufPtr == 13 ) && ( pParseFlags & PARSEFLG_REPLACECR ) )
      {
         *lpBuf->lpBufPtr = ' '; // !!SM!! 2.55.2059 Invalid CR in CIX Scratchpad - Should only contain LF's
         ++lpBuf->lpBufPtr;
         ++lpBuf->dwAccum;
         ++length;
         fSeenEOL = TRUE;
      }
      if( *lpBuf->lpBufPtr == 10 ) {
         ++lpBuf->lpBufPtr;
         ++lpBuf->dwAccum;
         ++length;
         fSeenEOL = TRUE;
      }
   }
*/
   if( npiLen )
      *npiLen = length;
   lpUsrBuf[ c ] = '\0';
   return( !fEof );
}

/* This function returns the last error
 */
int FASTCALL Amlbf_GetLastError( LPLBF lpBuf )
{
   return( lpBuf->nError );
}

/* This function returns the total number of characters in the
 * input file. For optimum performance, it simply returns the value
 * calculated earlier by InitBuffer.
 */
DWORD FASTCALL Amlbf_GetSize( LPLBF lpBuf )
{
   return( lpBuf->lszBuf );
}

/* This function returns the total number of characters read
 * from the input file so far, including any end-of-line delimiters.
 */

DWORD FASTCALL Amlbf_GetCount( LPLBF lpBuf )
{
   return( lpBuf->dwAccum );
}

/* This function returns our current read position within
 * the buffer.
 */
LONG FASTCALL Amlbf_GetPos( LPLBF lpBuf )
{
   return( lpBuf->dwAccum );
}

/* This function adjusts our read position within the
 * buffer.
 */
void FASTCALL Amlbf_SetPos( LPLBF lpBuf, LONG pos )
{
   Amfile_SetPosition( lpBuf->fh, pos, ASEEK_BEGINNING );
   lpBuf->lpBufPtr = lpBuf->lpBuffer;
   *lpBuf->lpBufPtr = '\0';
}

/* This function sets the date and time of the
 * buffer file. This is the date/time assigned to the
 * file when we close it.
 */
void FASTCALL Amlbf_SetFileDateTime( LPLBF lpBuf, WORD wDate, WORD wTime )
{
   lpBuf->wTime = wTime;
   lpBuf->wDate = wDate;
}
