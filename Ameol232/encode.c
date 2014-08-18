/* ENCODE.C - Encode a uuencoded or MIME encoded message
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

#define  THIS_FILE   __FILE__

#include "main.h"
#include "palrc.h"
#include "resource.h"
#include <string.h>
#include "amlib.h"
#include "shrdob.h"

/* The intrinsic version of this function does not
 * properly handle huge pointers!
 */
#pragma function(_fmemcpy)

static HPSTR hpEncBufText;             /* Text buffer */
static DWORD dwEncBufLength;           /* Length of message so far */
static DWORD dwEncBufSize;             /* Size of buffer */
static DWORD dwGrowthRate;             /* Number of bytes by which buffer grows */

/* ENC is the basic 1-character encoding function to make a char printing 
*/
#define ENC(c) ((c) ? ((c) & 077) + ' ': '`')

BOOL fSeparateEncodedCover;         /* TRUE if we start the encoded stuff from the second page */
BOOL fSplitParts;                   /* Split parts when dwLinesMax reached per part */
BOOL fAttachmentSizeWarning;
BOOL fAttachmentConvertToShortName;         /* When saving attachment strings, convert ones with spaces to their shortname equivalent*/

DWORD dwLinesMax;                   /* Maximum number of lines per uuencoded message */

HPSTR FASTCALL EncodeUuencodeFiles( HWND, LPSTR, int * );
HPSTR FASTCALL EncodeMimeFiles( HWND, LPSTR, LPSTR, int * );
void FASTCALL EncodeLineUU( LPSTR, UINT, LPSTR );
LPSTR FASTCALL UUConvert( LPSTR, LPSTR );
static BOOL FASTCALL AppendToBuffer( LPSTR, UINT );
static BOOL FASTCALL PreallocateBuffer( DWORD );
char FASTCALL Encode64( char );
void FASTCALL GetFileTypeSubType( LPSTR, char *, char * );
int FASTCALL b64decode(char *s);

static char szAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* This function encodes the list of filenames pointed by lpFilenames
 * using the specified scheme.
 */
HPSTR FASTCALL EncodeFiles( HWND hwnd, LPSTR lpFilenames, LPSTR lpBoundary, int wScheme, int * piParts )
{
   switch( wScheme )
      {
      case ENCSCH_UUENCODE:
         return( EncodeUuencodeFiles( hwnd, lpFilenames, piParts ) );

      case ENCSCH_MIME:
         return( EncodeMimeFiles( hwnd, lpBoundary, lpFilenames, piParts ) );
      }
   return( NULL );
}

/* This function encodes the filename pointed by lpFilenames using
 * uuencoding.
 */
HPSTR FASTCALL EncodeUuencodeFiles( HWND hwnd, LPSTR lpFilename, int * piParts )
{
   BOOL fOk;
   BOOL fFileOpenError = FALSE;

   hpEncBufText = NULL;
   dwEncBufLength = 0;
   dwEncBufSize = 0;
   *piParts = 1;
   fOk = TRUE;
   while( *lpFilename )
      {
      HNDFILE fh;

      lpFilename = ExtractFilename( lpPathBuf, lpFilename );
      if( ( fh = Amfile_Open( lpPathBuf, AOF_SHARE_READ|AOF_SHARE_WRITE ) ) == HNDFILE_ERROR )
         if( ( fh = Amfile_Open( lpPathBuf, AOF_SHARE_READ ) ) == HNDFILE_ERROR )
         {
            FileOpenError( hwnd, lpPathBuf, FALSE, TRUE );
            fFileOpenError = TRUE;
         }
      if( !fFileOpenError )
         {
         LPSTR lpFileBuf;
         UINT cbFileBuf;

         /* Initialise buffer variables.
          */
         INITIALISE_PTR(lpFileBuf);

         /* Allocate a file buffer of about 45K for reading the
          * file in chunks.
          */
         cbFileBuf = 45u * 1000u;
         if( fNewMemory( &lpFileBuf, cbFileBuf ) )
            {
            DWORD dwFileSize;
            DWORD dwBytesSoFar;
            DWORD dwEstimatedLines;
            DWORD dwLines;
            UINT cRead;
            UINT cEncoded;
            char sz[ 100 ];
            HWND hwndGauge = NULL;
            HWND hDlg = NULL;
            WORD wCount;
            WORD wOldCount = 0;


            /* Try to preallocate the buffer
             */
            dwFileSize = Amfile_GetFileSize( fh );
            dwEstimatedLines = ( dwFileSize / 45 ) + 1;
            if( NULL == hpEncBufText )
               {
               DWORD dwSizeWanted;

               if( fSplitParts )
                  dwSizeWanted = ( ( dwEstimatedLines / dwLinesMax ) + 1 ) * 64;
               else
                  dwSizeWanted = dwEstimatedLines * 64;
               PreallocateBuffer( dwSizeWanted + 64 );
               }

            /* Big file (>200K)? Use a status gauge
             */
            if( dwFileSize > 200000 )
               {
               EnableWindow( hwndFrame, FALSE );
               hDlg = Adm_Billboard( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_ENCODINGMSG) );
               hwndGauge = GetDlgItem( hDlg, IDD_GAUGE );
               SendMessage( hwndGauge, PBM_SETSTEP, 1, 0L );
               SetDlgItemText( hDlg, IDD_STATUS, lpPathBuf );
               if( fSplitParts )
                  {
                  wsprintf( lpTmpBuf, GS(IDS_STR1202), ( (dwEstimatedLines / dwLinesMax) > 0 ? (dwEstimatedLines / dwLinesMax) : 1 ), dwLinesMax );
                  SetDlgItemText( hDlg, IDD_STATUS3, lpTmpBuf );
                  }
               }

            /* Loop for every byte in the file and
             * encode it.
             */
            dwLines = 0;
            cRead = 0;
            dwBytesSoFar = 0L;
            while( fOk && ( cRead = Amfile_Read( fh, lpFileBuf, cbFileBuf ) ) != 0 && (HNDFILE)cRead != HNDFILE_ERROR )
               {
               LPSTR lpFilePtr;

               lpFilePtr = lpFileBuf;
               while( fOk && cRead )
                  {
                  TaskYield();
                  cEncoded = cRead > 45 ? 45 : cRead;

                  /* Bump the gauge.
                   */
                  dwBytesSoFar += cEncoded;
                  if( dwFileSize > 200000 )
                     {
                     wCount = (WORD)( ( (DWORD)dwBytesSoFar * 100 ) / (DWORD)dwFileSize );
                     if( wCount != wOldCount )
                        {
                        SendMessage( hwndGauge, PBM_STEPIT, 0, 0L );
                        wOldCount = wCount;
                        }
                     }
                  if( 0 == dwLines )
                     {
                     wsprintf( sz, "\r\nbegin 666 %s\r\n", GetFileBasename( lpPathBuf ) );
                     fOk = AppendToBuffer( sz, strlen( sz ) );
                     }
                  else if( fSplitParts && dwLines == dwLinesMax )
                     {
                     fOk = AppendToBuffer( "\0", 1 );
                     dwLines = 0;
                     *piParts = *piParts + 1;
                     }
                  if( fOk )
                     {
                     EncodeLineUU( lpFilePtr, cEncoded, sz );
                     AppendToBuffer( sz, strlen( sz ) );
                     ++dwLines;
                     }
                  lpFilePtr += cEncoded;
                  cRead -= cEncoded;
                  }
               }
            
            SendMessage( hwndGauge, PBM_SETPOS, 100, 0L );

            /* Delete any progress window.
             */
            if( hwndGauge != NULL )
               {
               EnableWindow( hwndFrame, TRUE );
               DestroyWindow( hDlg );
               }

            /* If tout va bien, then append an end record to the
             * end of the last block.
             */
            if( fOk )
               {
               wsprintf( sz, "`\r\nend\r\n" );
               AppendToBuffer( sz, strlen( sz ) );
               if( *lpFilename )
                  {
                  /* If more attachments, separate each one with
                   * a newline.
                   */
                  wsprintf( sz, "\r\n" );
                  AppendToBuffer( sz, strlen( sz ) );
                  }
               }
            else
               {
               wsprintf( lpTmpBuf, GS(IDS_STR752), lpPathBuf );
               fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
               }

            /* Read error while reading attachment. Definitely NOT safe to
             * send the attachment!
             */
            if( (HNDFILE)cRead == HNDFILE_ERROR )
               {
               wsprintf( lpTmpBuf, GS(IDS_STR752), lpPathBuf );
               fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
               fOk = FALSE;
               }

            /* Failure? Destroy the memory block.
             */
            if( !fOk && hpEncBufText )
               {
               FreeMemory32( &hpEncBufText );
               hpEncBufText = NULL;
               }
            FreeMemory( &lpFileBuf );
            }
         else
            {
            wsprintf( lpTmpBuf, GS(IDS_STR752), lpPathBuf );
            fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
            }
         Amfile_Close( fh );
         }
      }
   if( fOk )
      fOk = AppendToBuffer( "\0", 1 );
   return( fOk ? hpEncBufText : NULL );
}

/* This function encodes the filename pointed by lpFilenames using
 * MIME encoding.
 */
HPSTR FASTCALL EncodeMimeFiles( HWND hwnd, LPSTR lpBoundary, LPSTR lpFilename, int * piParts )
{
   LPSTR lpFileBuf;
   char sz[ 300 ];
   HNDFILE fh;
   BOOL fOk;
   BOOL fFileOpenError = FALSE;

   hpEncBufText = NULL;
   *piParts = 1;
   fOk = FALSE;

   /* Initialise buffer variables.
    */
   INITIALISE_PTR(lpFileBuf);
   dwEncBufLength = 0;
   dwEncBufSize = 0;
   fOk = TRUE;

   /* Loop for each filename.
    */
   while( *lpFilename && fOk )
      {
      /* Extract one filename.
       */
      lpFilename = ExtractFilename( lpPathBuf, lpFilename );

      /* For multiple selection files, the first entry will not have
       * a basename, so save it as the pathname for further filenames
       * which will be relative to this one.
       */
      if( ( fh = Amfile_Open( lpPathBuf, AOF_SHARE_READ|AOF_SHARE_WRITE ) ) == HNDFILE_ERROR )
         if( ( fh = Amfile_Open( lpPathBuf, AOF_SHARE_READ ) ) == HNDFILE_ERROR )
         {
         FileOpenError( hwnd, lpPathBuf, FALSE, TRUE );
         fOk = FALSE;
         fFileOpenError = TRUE;
         }
      if( !fFileOpenError )
         {
         char szType[ 20 ];
         char szSubType[ 20 ];
         UINT cbFileBuf;
   
         /* Get the type and subtype of the file.
          */
         GetFileTypeSubType( lpPathBuf, szType, szSubType );
   
         /* Allocate a file buffer of about 45K for reading the
          * file in chunks.
          */
         cbFileBuf = 45u * 1000u;
         if( fNewMemory( &lpFileBuf, cbFileBuf ) )
            {
            DWORD dwFileSize;
            DWORD dwBytesSoFar;
            DWORD dwEstimatedLines;
            DWORD dwLines;
            int cRead;
            HWND hDlg = NULL;
            HWND hwndGauge = NULL;
            LPSTR lpFilePtr;
            unsigned long accum;
            int save_shift;
            BOOL fQuit;
            BOOL fEof;
            int shift;
            int index;
            WORD wCount;
            WORD wOldCount = 0;

            /* Initalise variables.
             */
            shift = 0;
            accum = 0;
            index = 0;
            fQuit = FALSE;
            save_shift = 0;
   
            /* Try to preallocate the buffer
             */
            dwFileSize = Amfile_GetFileSize( fh );
            dwEstimatedLines = ( dwFileSize + ( dwFileSize / 4 ) ) / 64;
            if( NULL == hpEncBufText )
               {
               DWORD dwSizeWanted;

               if( fSplitParts )
                  dwSizeWanted = ( dwEstimatedLines / dwLinesMax ) * 64;
               else
                  dwSizeWanted = dwEstimatedLines * 64;
               PreallocateBuffer( dwSizeWanted + 64 );
               }

            /* Big file (>200K)? Use a status gauge
             */
            if( dwFileSize > 200000 )
               {
               EnableWindow( hwndFrame, FALSE );
               hDlg = Adm_Billboard( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_ENCODINGMSG) );
               hwndGauge = GetDlgItem( hDlg, IDD_GAUGE );
               SetDlgItemText( hDlg, IDD_STATUS, lpPathBuf );
               SendMessage( hwndGauge, PBM_SETSTEP, 10, 0L );
               if( fSplitParts )
                  {
                  wsprintf( lpTmpBuf, GS(IDS_STR1202), ( (dwEstimatedLines / dwLinesMax) > 0 ? (dwEstimatedLines / dwLinesMax) : 1 ), dwLinesMax );
                  SetDlgItemText( hDlg, IDD_STATUS3, lpTmpBuf );
                  }
               }

            /* Loop for every byte in the file and
             * encode it.
             */
            dwLines = 0;
            dwBytesSoFar = 0L;
            fEof = FALSE;
            cRead = 0;
            lpFilePtr = NULL;
            while( ( !fEof || ( shift != 0 ) ) && fOk )
               {
               unsigned long value;
               unsigned char blivit;
   
               /* If we're allowed to split into parts and we've gone over
                * the maximum, terminate the current block with a NULL and
                * start a new one.
                */
               if( fSplitParts && dwLines >= dwLinesMax )
                  {
                  fOk = AppendToBuffer( "\0", 1 );
                  dwLines = 0;
                  *piParts = *piParts + 1;
                  }

               /* Start a new block header.
                */
               if( dwLines == 0 )
                  {
                  if( NULL != lpBoundary )
                     {
                     wsprintf( sz, "--%s\r\n", lpBoundary );
                     fOk = AppendToBuffer( sz, strlen( sz ) );
                     wsprintf( sz, "Content-Type: %s/%s; name=\"%s\"\r\n", (LPSTR)szType, (LPSTR)szSubType, GetFileBasename( lpPathBuf ) );
                     fOk = AppendToBuffer( sz, strlen( sz ) );
                     wsprintf( sz, "Content-Transfer-Encoding: base64\r\n" );
                     fOk = AppendToBuffer( sz, strlen( sz ) );
                     wsprintf( sz, "Content-Disposition: attachment; filename=\"%s\"\r\n", GetFileBasename( lpPathBuf ) );
                     fOk = AppendToBuffer( sz, strlen( sz ) );
                     fOk = AppendToBuffer( "\r\n", 2 );
                     dwLines = 5;
                     }
                  }

               /* Here's where we read bytes from the buffer.
                */
               if( !fEof && !fQuit )
                  {
                  if( cRead == 0 )
                     {
                     cRead = Amfile_Read( fh, lpFileBuf, cbFileBuf );
                     lpFilePtr = lpFileBuf;
                     TaskYield();

                     /* Bump the gauge.
                      */
                     dwBytesSoFar += cRead;
                     if( dwFileSize > 200000 )
                        {
                        wCount = (WORD)( ( (DWORD)dwBytesSoFar * 10 ) / (DWORD)( dwFileSize + 1 ) );
                        if( wCount != wOldCount )
                           {
                           SendMessage( hwndGauge, PBM_STEPIT, 0, 0L );
                           wOldCount = wCount;
                           }
                        }
                     }
                  if( cRead > 0 )
                     {
                     blivit = *lpFilePtr++;
                     --cRead;
                     }
                  else
                     {
                     fQuit = TRUE;
                     save_shift = shift;
                     blivit = 0;
                     fEof = TRUE;
                     }
                  }
               else
                  {
                  fQuit = TRUE;
                  save_shift = shift;
                  blivit = 0;
                  }
               if( !fQuit || ( shift != 0 ) )
                  {
                  value = (unsigned long)blivit;
                  accum <<= 8;
                  shift += 8;
                  accum |= value;
                  }
               while( shift >= 6 )
                  {
                  shift -= 6;
                  value = (accum >> shift) & 0x3Fl;
                  blivit = szAlphabet[ value ];
                  sz[ index++ ] = blivit;
                  if ( index >= 60 )
                     {
                     sz[ index++ ] = '\r';
                     sz[ index++ ] = '\n';
                     sz[ index ] = '\0';
                     AppendToBuffer( sz, strlen( sz ) );
                     ++dwLines;
                     index = 0;
                     }
                  if( fQuit )
                     shift = 0;
                  }
               }

            /* Handle straggling bytes.
             */   
            if( save_shift == 2 )
               {
               sz[ index++ ] = '=';
               if( index >= 60 )
                  {
                  sz[ index++ ] = '\r';
                  sz[ index++ ] = '\n';
                  sz[ index ] = '\0';
                  AppendToBuffer( sz, strlen( sz ) );
                  ++dwLines;
                  index = 0;
                  }
               sz[ index++ ] = '=';
               if( index >= 60 )
                  {
                  sz[ index++ ] = '\r';
                  sz[ index++ ] = '\n';
                  sz[ index ] = '\0';
                  AppendToBuffer( sz, strlen( sz ) );
                  ++dwLines;
                  index = 0;
                  }
               }
            else if( save_shift == 4 )
               {
               sz[ index++ ] = '=';
               if( index >= 60 )
                  {
                  sz[ index++ ] = '\r';
                  sz[ index++ ] = '\n';
                  sz[ index ] = '\0';
                  AppendToBuffer( sz, strlen( sz ) );
                  ++dwLines;
                  index = 0;
                  }
               }

            /* Write out any partially filled line.
             */
            if( index != 0 )
               {
               sz[ index++ ] = '\r';
               sz[ index++ ] = '\n';
               sz[ index ] = '\0';
               AppendToBuffer( sz, strlen( sz ) );
               ++dwLines;
               }


            SendMessage( hwndGauge, PBM_SETPOS, 100, 0L );

            /* Delete any progress window.
             */
            if( hwndGauge != NULL )
               {
               EnableWindow( hwndFrame, TRUE );
               DestroyWindow( hDlg );
               }

            /* If tout va bien, then append an end record to the
             * end of the last block.
             */
            if( !fOk )
               {
               wsprintf( lpTmpBuf, GS(IDS_STR752), lpPathBuf );
               fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
               }
   
            /* Read error while reading attachment. Definitely NOT safe to
             * send the attachment!
             */
            if( cRead == -1 )
               {
               wsprintf( lpTmpBuf, GS(IDS_STR752), lpPathBuf );
               fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
               fOk = FALSE;
               }
   
            /* Failure? Destroy the memory block.
             */
            if( !fOk && hpEncBufText )
               {
               FreeMemory32( &hpEncBufText );
               hpEncBufText = NULL;
               }
            FreeMemory( &lpFileBuf );
            }
         else
            {
            wsprintf( lpTmpBuf, GS(IDS_STR752), lpPathBuf );
            fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
            fOk = FALSE;
            }
         Amfile_Close( fh );
         }
      }

   /* If all OK, append a boundary terminator to the end.
    */
   if( fOk && NULL != lpBoundary )
      {
      wsprintf( sz, "--%s--\r\n", lpBoundary );
      AppendToBuffer( sz, strlen( sz ) + 1 );
      }
   return( fOk ? hpEncBufText : NULL );
}

/* This function attempts to allocate a BIG buffer of the
 * requested size to avoid calls to fResizeMemory.
 */
static BOOL FASTCALL PreallocateBuffer( DWORD dwSizeWanted )
{
   if( fNewMemory32( &hpEncBufText, dwSizeWanted ) )
      {
      dwEncBufSize = dwSizeWanted;
      dwEncBufLength = 0;
      dwGrowthRate = dwSizeWanted;
      return( TRUE );
      }
   dwGrowthRate = 64000;
   return( FALSE );
}

/* This function appends the specified line to the memory buffer.
 */
static BOOL FASTCALL AppendToBuffer( LPSTR lpstr, UINT wSize )
{
   /* Cast to long to avoid wrap round because MAX_MSGSIZE is close to 0xFFFF
    */
   if( hpEncBufText == NULL )
      {
      dwEncBufSize = dwGrowthRate;
      dwEncBufLength = 0;
      fNewMemory32( &hpEncBufText, dwEncBufSize );
      }
   else if( dwEncBufLength + (DWORD)wSize + 2L >= dwEncBufSize )
      {
      dwEncBufSize += dwGrowthRate;
      fResizeMemory32( &hpEncBufText, dwEncBufSize );
      }
   if( hpEncBufText )
      {
      _fmemcpy( hpEncBufText + dwEncBufLength, lpstr, wSize );
      dwEncBufLength += wSize;
      return( TRUE );
      }
   return( FALSE );
}

/* This function encodes the wCount bytes from the source buffer to
 * the destination buffer and appends a CR/LF and NULL to the end.
 */
void FASTCALL EncodeLineUU( LPSTR lpSrcBuf, UINT wCount, LPSTR lpDestBuf )
{
   register UINT c;

   *lpDestBuf++ = ENC( wCount );
   for( c = 0; c < wCount; c += 3 )
      lpDestBuf = UUConvert( lpDestBuf, &lpSrcBuf[ c ] );
   *lpDestBuf++ = CR;
   *lpDestBuf++ = LF;
   *lpDestBuf = '\0';
}

/* This function converts the three characters at lpSrc into
 * four characters at lpDst.
 */
LPSTR FASTCALL UUConvert( LPSTR lpDest, LPSTR lpSrc )
{
   int c1, c2, c3, c4;

   c1 = lpSrc[0] >> 2;
   c2 = (lpSrc[0] << 4) & 060 | (lpSrc[1] >> 4) & 017;
   c3 = (lpSrc[1] << 2) & 074 | (lpSrc[2] >> 6) & 03;
   c4 = lpSrc[2] & 077;
   *lpDest++ = ENC(c1);
   *lpDest++ = ENC(c2);
   *lpDest++ = ENC(c3);
   *lpDest++ = ENC(c4);
   return( lpDest );
}

/* This function does Base 64 encoding of the NULL terminated line at
 * lpszSrc to the buffer at lpszDest and returns the number of
 * characters encoded.
 *
 * NOTE: DON'T use this for encoding files into base 64. It's broken!
 */
int FASTCALL EncodeLine64( LPSTR lpszSrc, UINT wCount, LPSTR lpszDest )
{
   register int c;
   register UINT d;

   for( c = d = 0; d < wCount; d += 3 )
      {
      char cTempHigh;
      char cTempLow;
      BYTE cByte1, cByte2, cByte3, cByte4;
      char cLetter1, cLetter2, cLetter3;

      cLetter1 = lpszSrc[ d ];
      cLetter2 = lpszSrc[ d + 1 ];
      cLetter3 = lpszSrc[ d + 2 ];
      cByte1 = (BYTE)cLetter1 >> 2;
      cTempHigh = ( cLetter1 << 4 ) & 0x30;
      cTempLow = (BYTE)cLetter2 >> 4;
      cByte2 = cTempHigh | cTempLow;
      cTempHigh = ( cLetter2 << 2 ) & 0x3C;
      cTempLow = (BYTE)cLetter3 >> 6;
      cByte3 = cTempHigh | cTempLow;
      cByte4 = cLetter3 & 0x3F;
      lpszDest[ c++ ] = Encode64( cByte1 );
      lpszDest[ c++ ] = Encode64( cByte2 );
      lpszDest[ c++ ] = Encode64( cByte3 );
      lpszDest[ c++ ] = Encode64( cByte4 );
      }
   lpszDest[ c ] = '\0';
   return( c );
}

/* This function converts an ASCII letter into a Base 64
 * 6-bit code.
 */
char FASTCALL Encode64( char ch )
{
   static char EncodeTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

   if( ch >= 0 && ch <= 63 )
      return( EncodeTable[ ch ] );
   return( '=' );
}

/* Given a filename, this function determines the type and subtype of that file.
 * Currently, all this information is hard-coded in. Later we should be able to
 * read this from a file.
 */
void FASTCALL GetFileTypeSubType( LPSTR lpFilename, char * pszType, char * pszSubType )
{
   LPSTR lpExt;

   /* Get the extension and compare with the known
    * types.
    */
   if( lpExt = GetFileBasename( lpFilename ) )
      {
      while( *lpExt && *lpExt != '.' )
         ++lpExt;
      if( *lpExt == '.' )
         {
         ++lpExt;
         if( _strcmpi( lpExt, "mpg" ) == 0 )
            {
            strcpy( pszType, "video" );
            strcpy( pszSubType, "mpeg" );
            return;
            }
         if( _strcmpi( lpExt, "jpg" ) == 0 )
            {
            strcpy( pszType, "image" );
            strcpy( pszSubType, "jpeg" );
            return;
            }
         if( _strcmpi( lpExt, "gif" ) == 0 )
            {
            strcpy( pszType, "image" );
            strcpy( pszSubType, "gif" );
            return;
            }
         }
      }

   /* The default types if none of the above.
    */
   strcpy( pszType, "application" );
   strcpy( pszSubType, "octet-stream" );
}

typedef
unsigned
char    uchar;                              // Define unsigned char as uchar.

#define b64is7bit(c)  ((c) > 0x7f ? 0 : 1)  // Valid 7-Bit ASCII character?
#define b64blocks(l) (((l) + 2) / 3 * 4 + 1)// Length rounded to 4 byte block.
#define b64octets(l)  ((l) / 4  * 3 + 1)    // Length rounded to 3 byte octet.

char   *b64buffer(char *, BOOL);            // Alloc. encoding/decoding buffer.

// Note:    Tables are in hex to support different collating sequences

static  
const                                       // Base64 Index into encoding
uchar  pIndex[]     =   {                   // and decoding table.
                        0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
                        0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
                        0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
                        0x59, 0x5a, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
                        0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e,
                        0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
                        0x77, 0x78, 0x79, 0x7a, 0x30, 0x31, 0x32, 0x33,
                        0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2b, 0x2f
                        };

static
const                                       // Base64 encoding and decoding
uchar   pBase64[]   =   {                   // table.
                        0x3e, 0x7f, 0x7f, 0x7f, 0x3f, 0x34, 0x35, 0x36,
                        0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x7f,
                        0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x00, 0x01,
                        0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
                        0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11,
                        0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
                        0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x1a, 0x1b,
                        0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
                        0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
                        0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33
                        };

/*---------------------------------------------------------------------------*/
/* b64buffer - Allocate the decoding or encoding buffer.                     */
/* =====================================================                     */
/*                                                                           */
/* Call this routine to allocate an encoding buffer in 4 byte blocks or a    */
/* decoding buffer in 3 byte octets.  We use "calloc" to initialize the      */
/* buffer to 0x00's for strings.                                             */
/*                                                                           */
/* Call with:   char *  - Pointer to the string to be encoded or decoded.    */
/*              bool    - True (!0) to allocate an encoding buffer.          */
/*                        False (0) to allocate a decoding buffer.           */
/*                                                                           */
/* Returns:     char *  - Pointer to the buffer or NULL if the buffer        */
/*                        could not be allocated.                            */
/*---------------------------------------------------------------------------*/
char *b64buffer(char *s, BOOL f)
{
    int     l = strlen(s);                  // String size to encode or decode.
    char   *b;                              // String pointers.

    if  (!l)                                // If the string size is 0...
        return  NULL;                       // ...return null.

   if (!(b = (char *) calloc((f ? b64blocks(l) : b64octets(l)),
               sizeof(char))))
      return NULL;
//        printf("Insufficient real memory to %s \"%s\".\n",(f ? "encode" : "decode"), s);
    return  b;                              // Return the pointer or null.
}

/*---------------------------------------------------------------------------*/
/* b64valid - validate the character to decode.                              */
/* ============================================                              */
/*                                                                           */
/* Checks whether the character to decode falls within the boundaries of the */
/* Base64 decoding table.                                                    */
/*                                                                           */
/* Call with:   char    - The Base64 character to decode.                    */
/*                                                                           */
/* Returns:     bool    - True (!0) if the character is valid.               */
/*                        False (0) if the character is not valid.           */
/*---------------------------------------------------------------------------*/
BOOL b64valid(uchar *c)
{
    if ((*c < 0x2b) || (*c > 0x7a))         // If not within the range of...
        return FALSE;                       // ...the table, return false.
    
    if ((*c = pBase64[*c - 0x2b]) == 0x7f)  // If it falls within one of...
        return FALSE;                       // ...the gaps, return false.

    return TRUE;                            // Otherwise, return true.
}

/*---------------------------------------------------------------------------*/
/* b64isnot - Display an error message and clean up.                         */
/* =================================================                         */
/*                                                                           */
/* Call this routine to display a message indicating that the string being   */
/* decoded is an invalid Base64 string and de-allocate the decoding buffer.  */
/*                                                                           */
/* Call with:   char *  - Pointer to the Base64 string being decoded.        */
/*              char *  - Pointer to the decoding buffer or NULL if it isn't */
/*                        allocated and doesn't need to be de-allocated.     */
/*                                                                           */
/* Returns:     bool    - True (!0) if the character is valid.               */
/*                        False (0) if the character is not valid.           */
/*---------------------------------------------------------------------------*/
BOOL b64isnot(char *p, char *b)
{
//    printf("\"%s\" is not a Base64 encoded string.\n", p);

    if  (b)                                 // If the buffer pointer is not...
        free(b);                            // ...NULL, de-allocate it.

    return  FALSE;                          // Return false for main.
}

/*---------------------------------------------------------------------------*/
/* b64encode - Encode a 7-Bit ASCII string to a Base64 string.               */
/* ===========================================================               */
/*                                                                           */
/* Call with:   char *  - The 7-Bit ASCII string to encode.                  */
/*                                                                           */
/* Returns:     bool    - True (!0) if the operation was successful.         */
/*                        False (0) if the operation was unsuccessful.       */
/*---------------------------------------------------------------------------*/
int FASTCALL b64encode(char *s)
{
    int     l   = strlen(s);                // Get the length of the string.
    int     x   = 0;                        // General purpose integers.
    char   *b, *p;                          // Encoded buffer pointers.

    while   (x < l)                         // Validate each byte of the string
    {                                       // ...to ensure that it's 7-Bit...
        if (!b64is7bit((uchar) *(s + x)))   // ...ASCII.
        {
//            printf("\"%s\" is not a 7-Bit ASCII string.\n", s);
            return FALSE;                   // Return false if it's not.
        }
        x++;                                // Next byte.
    }

    if (!(b = b64buffer(s, TRUE)))          // Allocate an encoding buffer.
        return FALSE;                       // Can't allocate encoding buffer.

    memset(b, 0x3d, b64blocks(l) - 1);      // Initialize it to "=". 

    p = b;                                  // Save the buffer pointer.
    x = 0;                                  // Initialize string index.

    while   (x < (l - (l % 3)))             // encode each 3 byte octet.
    {
        *b++   = pIndex[  s[x]             >> 2];
        *b++   = pIndex[((s[x]     & 0x03) << 4) + (s[x + 1] >> 4)];
        *b++   = pIndex[((s[x + 1] & 0x0f) << 2) + (s[x + 2] >> 6)];
        *b++   = pIndex[  s[x + 2] & 0x3f];
         x    += 3;                         // Next octet.
    }

    if (l - x)                              // Partial octet remaining?
    {
        *b++        = pIndex[s[x] >> 2];    // Yes, encode it.

        if  (l - x == 1)                    // End of octet?
            *b      = pIndex[ (s[x] & 0x03) << 4];
        else                            
        {                                   // No, one more part.
            *b++    = pIndex[((s[x]     & 0x03) << 4) + (s[x + 1] >> 4)];
            *b      = pIndex[ (s[x + 1] & 0x0f) << 2];
        }
    }

//    b64stats(s, p, TRUE);                   // Display some encoding stats.
   strcpy(s,p);
    free(p);                                // De-allocate the encoding buffer.
//    printf("Base64 encoding complete.\n");
    return TRUE;                            // Return to caller with success.
}



int axtoi( LPTSTR pch) 
{     
   register TCHAR ch;     
   register INT n = 0;      
   while ((ch = *pch++) != 0) 
   {         
      if (iswdigit(ch))             
         ch -= '0';
      else if (ch >= 'A' && ch <= 'F')             
         ch += (TCHAR)(10 - 'A');         
      else if (ch >= 'a' && ch <= 'f')             
         ch += (TCHAR)(10 - 'a');         
      else             
         ch = (TCHAR)0;          
      n = 16 * n + ch;     
   }      
   return n; 
}  


int awtoi(LPWSTR string) 
{     
   CHAR szAnsi[17];     
   BOOL fDefCharUsed;      
   
   WideCharToMultiByte(CP_ACP, 0, string, -1, szAnsi, 17, NULL, &fDefCharUsed);      
   return atoi(szAnsi); 
} 
 

int valtoi( LPTSTR pszValue) 
{     
   if ( (char)pszValue[0] == '0' && ((char)pszValue[1] == 'X' || (char)pszValue[1] == 'x'))
      return (axtoi((LPTSTR)&pszValue[2])) ;
   else
      return awtoi((LPWSTR)pszValue); 
}  


void DecodePrintedQuotable(char * d)
{
   LPSTR b;
   char    buf[5];
   int j, k, l;

// c = d;

   INITIALISE_PTR(b);

   l = strlen(d)+1;

   if( !fNewMemory( &b, l ) )
       return ;

   j = 0;
   k = 0;
   while( d[k] )
   {
      if (d[k] == '?')
      {
         if (d[k+1] = '=')
            break;
      }
      else if (d[k] == '=')
      {
         buf[0]='0';
         buf[1]='x';
         k++;
         buf[2]=d[k];
         k++;
         buf[3]=d[k];
         buf[4]='\x0';
         b[j] = valtoi( buf ) ;
      }
      else
      {
         b[j] = d[k];
      }
      k++;
      j++;           
   }
   b[j] = '\x0';
   strcpy(d,b);
   FreeMemory( &b );
}

int FASTCALL b64decodePartial1(char *d)
{
   LPSTR  b, o;
   char * s, *t;
   int isB64, isQP, len, i, j;

   len = strlen(d);
   isB64 = 0;
   isQP = 0;
   s = strstr(_strupr(d), "ISO-8859-1?B?");
   if(s == NULL)
   {
      s = strstr(_strupr(d), "ISO-8859-1?Q?");
      if (s != NULL)
      {
         isQP = 1;
         isB64 = 0;
      }
   }
   else
   {
      isQP = 0;
      isB64 = 1;
   }

   if (s != NULL)
   {
      INITIALISE_PTR(b);
      INITIALISE_PTR(o);

      if( !fNewMemory( &b, len ) )
          return FALSE;
      if( !fNewMemory( &o, len ) )
          return FALSE;

      s += 13; 
      while (*s)
      {
         i = 0;
         j = 0;
         while (*s)
         {
            if(*s == '?' && *(s+1) == '=')
               break;
            else
            {
               b[i++] = *s;
               s++;
            }
         }

         if (isB64)
         {
            b64decode(b);
         }
         if (isQP)
         {
            DecodePrintedQuotable(b);
         }

         strcat(o, b);

         if (*s)
         {
            t = strstr(_strupr(s), "ISO-8859-1?B?");
            if(t != NULL)
            {
               isQP = 0;
               isB64 = 1;
            }
            else
            {
               t = strstr(_strupr(s), "ISO-8859-1?Q?");
               if (t != NULL)
               {
                  isQP = 1;
                  isB64 = 0;
               }
            }
            if(t != NULL)
            {
               s = t;
               s += 13; 
            }
            b[0] = '\x0';
         }
         if(*s == '?' && *(s+1) == '=')
            s+=2;
      }
      strcpy(d, o);
      FreeMemory( &b );
      FreeMemory( &o );
      return TRUE;
   }
   return TRUE;

}

typedef void (FAR PASCAL * CONVERTCHARSETPROC)(LPSTR);

int FASTCALL b64decodePartial(char *d)
{
   LPSTR  b, o;
   int j, e, l, k, len;
   int isB64, isQP;
   CONVERTCHARSETPROC lpConvertCharProc;

   // 20060505 - 2.56.2051 FS#111
   if (hRegLib >= (HINSTANCE)32)
   {
      if( NULL != ( lpConvertCharProc = (CONVERTCHARSETPROC)GetProcAddress( hRegLib, "ConvertCharSet" ) ) )
      {
         lpConvertCharProc( d );
         return TRUE;
      }
   }

   if (strstr(d, "?=") || strstr(d, "=?") /*|| strstr(_strupr(d), "ISO-8859-1")*/ )
   {
      len = strlen(d) + 1;

      INITIALISE_PTR(b);
      INITIALISE_PTR(o);

      if( !fNewMemory( &b, len ) )
          return FALSE;
      if( !fNewMemory( &o, len ) )
          return FALSE;

      l = 0;
      o[0] = '\x0';
      k = 0;
      while (d[k])
      {
         if ( (d[k] == '=') && ((d[k+1]) == '?')) // Begin
         {
            j = 0;
            e = 1;
            k += 2;
            isB64 = 0;
            isQP = 0;
            while(d[k] && e)
            {
               if (d[k] == '?')
               {
                  if ((d[k+1]) == '=') // End
                  {
                     b[j] = '\x0';
                     k+=2;
                     e = 0;
                  }
                  else if (toupper(d[k+1]) == 'Q' && (d[k+2]) == '?') 
                  {
                     b[0] = '\x0';
                     k+=3;
                     j=0;
                     isQP = 1;
                  }
                  else if (toupper(d[k+1]) == 'B' && (d[k+2]) == '?') 
                  {
                     b[0] = '\x0';
                     k+=3;
                     j=0;
                     isB64 = 1;
                  }
                  else
                  {
                     b[j] = d[k];
                     j++;
                     k++;
                  }
               }
               else
               {
                  b[j] = d[k];
                  j++;
                  k++;
               }
            }
            if ((stristr(b,"?B?") != NULL) || (stristr(b,"?b?") != NULL)) // Base 64
            {
               j = 0;
               while ( (b[j] != '?') && (toupper(b[j+1]) != 'B') && (b[j+2] != '?') )
                  j++;
               j += 5;
               memcpy(b, b+j,strlen(b));
               b64decode(b);
            }
            else if ((strstr(b,"?Q?") != NULL) || (strstr(b,"?q?") != NULL)) // Quoted Printable
            {
               j = 0;
               while ( (b[j] != '?') && (toupper(b[j+1]) != 'Q') && (b[j+2] != '?') )
                  j++;
               j += 5;
               memcpy(b, b+j,strlen(b));
               DecodePrintedQuotable(b);
            }
            else if (isB64)
            {
               b64decode(b);
            }
            else if (isQP)
            {
               DecodePrintedQuotable(b);
            }
            strcat(o, b);
            l = strlen(o);
            k++;
         }
         else
         {
            o[l] =  d[k];
            k++;
            l++;
         }
      }
      strcpy(d, o);
      FreeMemory( &b );
      FreeMemory( &o );
      return TRUE;
   }
   else
   {
      return TRUE;
   }
}

/*---------------------------------------------------------------------------*/
/* b64decode - Decode a Base64 string to a 7-Bit ASCII string.               */
/* ===========================================================               */
/*                                                                           */
/* Call with:   char *  - The Base64 string to decode.                       */
/*                                                                           */
/* Returns:     bool    - True (!0) if the operation was successful.         */
/*                        False (0) if the operation was unsuccessful.       */
/*---------------------------------------------------------------------------*/
int FASTCALL b64decode(char *s)
{
    int     l = strlen(s);                  // Get length of Base64 string.
    char   *b, *p;                          // Decoding buffer pointers.
    uchar   c = 0;                          // Character to decode.
    int     x = 0;                          // General purpose integers.
    int     y = 0;
    char *  len;

    static                                  // Collating sequence...
    const                                   // ...independant "===".
    char    pPad[]  =   {0x3d, 0x3d, 0x3d, 0x00};


    l = strlen(s);                  
   
   len = s;

    if  (l % 4)                             // If it's not modulo 4, then it...
        return b64isnot(s, NULL);           // ...can't be a Base64 string.

    if  (b = strchr(s, pPad[0]))            // Only one, two or three equal...
    {                                       // ...'=' signs are allowed at...
        if  ((b - s) < (l - 3))             // ...the end of the Base64 string.
            return b64isnot(s, NULL);       // Any other equal '=' signs are...
        else                                // ...invalid.
            if  (strncmp(b, (char *) pPad + 3 - (s + l - b), s + l - b))
                return b64isnot(s, NULL);
    }

    if  (!(b = b64buffer(s, FALSE)))        // Allocate a decoding buffer.
        return FALSE;                       // Can't allocate decoding buffer.

    p = s;                                  // Save the encoded string pointer.
    x = 0;                                  // Initialize index.

    while ((c = *s++))                      // Decode every byte of the...
    {                                       // Base64 string.
        if  (c == pPad[0])                  // Ignore "=".
            break;

        if (!b64valid(&c))                  // Valid Base64 Index?
            return b64isnot(s, b);          // No, return false.
        
        switch(x % 4)                       // Decode 4 byte words into...
        {                                   // ...3 byte octets.
        case    0:                          // Byte 0 of word.
            b[y]    =  c << 2;
            break;                          
        case    1:                          // Byte 1 of word.
            b[y]   |=  c >> 4;

            if (!b64is7bit((uchar) b[y++])) // Is 1st byte of octet valid?
            break;
  //              return b64isnot(s, b);      // No, return false.

            b[y]    = (c & 0x0f) << 4;
            break;
        case    2:                          // Byte 2 of word.
            b[y]   |=  c >> 2;

            if (!b64is7bit((uchar) b[y++])) // Is 2nd byte of octet valid?
            break;
//                return b64isnot(s, b);      // No, return false.

            b[y]    = (c & 0x03) << 6;
            break;
        case    3:                          // Byte 3 of word.
            b[y]   |=  c;

            if (!b64is7bit((uchar) b[y++])) // Is 3rd byte of octet valid?
            break;
//                return b64isnot(s, b);      // No, return false.
        }
        x++;                                // Increment word byte.
    }

//    b64stats(p, b, FALSE);                  // Display some decoding stats.
   s = len;
   strcpy(s,b);
    free(b);                                // De-allocate decoding buffer.
//    printf("Base64 decoding complete.\n");
    return TRUE;                            // Return to caller with success.
}
