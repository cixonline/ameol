/* VERSION.C - Version information functions
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
#include "resource.h"
#include "amlib.h"
#include <string.h>
#ifdef WIN32
#include <winver.h>
#else
#include <ver.h>
#endif
#include "common.bld"

#ifndef MAKEWORD
#define MAKEWORD(low, high) ((WORD)(((BYTE)(low)) | (((WORD)((BYTE)(high))) << 8)))
#endif

#define  CODENAME    "Legolas"

typedef struct tagLANGANDCP {
   WORD wLanguage;
   WORD wCodePage;
} LANGANDCP;

char szVarFileInfo[] = "\\VarFileInfo\\Translation";

/* This function returns the current version number,
 * packed into a 32-bit longword value.
 */
DWORD EXPORT WINAPI Ameol2_GetVersion( void )
{
   return( MAKEVERSION( amv.nMaxima, amv.nMinima, amv.nBuild ) );
}

/* This function returns a pointer to the Ameol codename.
 */
LPSTR EXPORT WINAPI Ameol2_GetAmeolCodename( void )
{
   return( CODENAME );
}

/* Given a 32-bit version number, this function expands it into a full
 * version string.
 */
LPSTR EXPORT WINAPI Ameol2_ExpandVersion( DWORD dwVersion )
{
   static char sz[ 40 ];

   wsprintf( sz, "%u.%2.2u", HIWORD( dwVersion ) >> 8, HIWORD( dwVersion ) & 0xFF );
   if( LOWORD( dwVersion ) )
      wsprintf( sz + strlen( sz ), ".%3.3u", LOWORD(dwVersion) );
   return( sz );
}

/* This function expands an addon's version number.
 */
LPSTR EXPORT WINAPI Ameol2_ExpandAddonVersion( DWORD dwVersion )
{
   static char sz[ 40 ];
   int nRelease;
   int nBeta;

   nRelease = LOWORD( dwVersion ) & 0xFF;
   nBeta = ( HIWORD( dwVersion ) >> 8 ) & 0xFF;
   wsprintf( sz, "%d.%2.2d", HIWORD( dwVersion ) & 0xFF, LOWORD( dwVersion ) >> 8 );
   if( nRelease > 100 && nRelease < 200 )
      wsprintf( sz + strlen( sz ), ".%2.2d", nRelease - 100 );
   else if( nRelease >= 200 )
      {
      if( nRelease > 200 )
         wsprintf( sz + strlen( sz ), ".%2.2d", nRelease - 200 );
      strcat( sz, " Evaluation Release" );
      }
   if( nBeta & 0x7F )
      wsprintf( sz + strlen( sz ), GS(IDS_STR365), nBeta & 0x7F );
   return( sz );
}

/* This function converts between an Ameol version number and an Ameol
 * version number.
 */
DWORD WINAPI EXPORT CvtToAmeolVersion( DWORD dwVersion )
{
   BYTE nMaxima;
   BYTE nMinima;
   BYTE nRelease;
   BYTE nBeta;

   nMaxima = (BYTE)( HIWORD( dwVersion ) >> 8 );
   nMinima = (BYTE)( HIWORD( dwVersion) & 0xFF );
   nRelease = 100;
   if( IS_BETA )
      nBeta = 1;
   else
      nBeta = 0;
   return( MAKELONG( MAKEWORD( nRelease, nMinima ), MAKEWORD( nMaxima, nBeta ) ) );
}

/* Fills the user supplied AMAPI_VERSION structure with information about the
 * specified file. The hInstance field of the structure must be instantiated on entry
 * with the handle of the calling application, as this function will use this to
 * extract the version information from the application resource block.
 */
BOOL CDECL GetPalantirVersion( LPAMAPI_VERSION lpapiVersion )
{
   char szFilename[ _MAX_PATH ];
   BOOL fOk;

   fOk = FALSE;
   if( GetModuleFileName( lpapiVersion->hInstance, szFilename, _MAX_PATH ) )
      {
      DWORD dwBufSize;
      DWORD dwHandle;

      if( ( dwBufSize = GetFileVersionInfoSize( szFilename, &dwHandle ) ) != 0L )
         {
         HGLOBAL hBuf;

         if( ( hBuf = GlobalAlloc( GHND, dwBufSize ) ) != NULL )
            {
            LPVOID lpvData;

            lpvData = GlobalLock( hBuf );
            if( GetFileVersionInfo( szFilename, 0L, dwBufSize, lpvData ) != 0 )
               {
               VS_FIXEDFILEINFO FAR * lpvsffi;
               char szStrFileInfoPath[ 40 ];
               char szStrFileInfo[ 60 ];
               LANGANDCP FAR * lpBuffer;
               LPSTR lpstr;
               UINT cbVerBuf;

               if( VerQueryValue( lpvData, "\\", (void FAR* FAR*)&lpvsffi, &cbVerBuf ) && cbVerBuf > 0 )
                  {
                  lpapiVersion->nMaxima = HIWORD( lpvsffi->dwFileVersionMS );
                  lpapiVersion->nMinima = LOWORD( lpvsffi->dwFileVersionMS );
                  lpapiVersion->nBuild = HIWORD( lpvsffi->dwFileVersionLS );
                  lpapiVersion->nRelease = LOWORD( lpvsffi->dwFileVersionLS );
                  lpapiVersion->fPrivate = ( lpvsffi->dwFileFlags & VS_FF_PRIVATEBUILD ) != 0;
                  lpapiVersion->fDebug = ( lpvsffi->dwFileFlags & VS_FF_DEBUG ) != 0;
                  lpapiVersion->fPatched = ( lpvsffi->dwFileFlags & VS_FF_PATCHED ) != 0;
                  lpapiVersion->fPrerelease = ( lpvsffi->dwFileFlags & VS_FF_PRERELEASE ) != 0;
                  fOk = TRUE;
                  }
               if( VerQueryValue( lpvData, szVarFileInfo, (void FAR* FAR*)&lpBuffer, &cbVerBuf ) && cbVerBuf > 0 )
                  {
                  *lpapiVersion->szLegalCopyright = '\0';
                  *lpapiVersion->szFileDescription = '\0';
                  wsprintf( szStrFileInfoPath, "\\StringFileInfo\\%04x%04x\\", lpBuffer->wLanguage, lpBuffer->wCodePage );

                  /* Get the file description.
                   */
                  wsprintf( szStrFileInfo, "%s\\FileDescription", (LPSTR)szStrFileInfoPath  );
                  VerQueryValue( lpvData, szStrFileInfo, &lpstr, &cbVerBuf );
                  if( lpstr )
                     lstrcpy( lpapiVersion->szFileDescription, lpstr );

                  /* Get the legal copyright string.
                   */
                  wsprintf( szStrFileInfo, "%s\\LegalCopyright", (LPSTR)szStrFileInfoPath  );
                  VerQueryValue( lpvData, szStrFileInfo, &lpstr, &cbVerBuf );
                  if( lpstr )
                     lstrcpy( lpapiVersion->szLegalCopyright, lpstr );

                  /* Get the product name.
                   */
                  wsprintf( szStrFileInfo, "%s\\ProductName", (LPSTR)szStrFileInfoPath  );
                  VerQueryValue( lpvData, szStrFileInfo, &lpstr, &cbVerBuf );
                  if( lpstr )
                     lstrcpy( szAppName, lpstr );
                  }
               }
            GlobalUnlock( hBuf );
            GlobalFree( hBuf );
            }
         }
      }
   return( fOk );
}
