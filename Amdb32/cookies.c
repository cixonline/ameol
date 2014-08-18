/* COOKIES.C - Functions that handle cookies
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

#include "warnings.h"
#include <windows.h>
#include <windowsx.h>
#include <assert.h>
#include <string.h>
#include "amlib.h"
#include "amuser.h"
#include "amctrls.h"
#include "amdbi.h"
#include "cookie.h"

#define  THIS_FILE   __FILE__

#define  COOKIE_FILENAME   "cookie.ini"
#define  MAX_COOKIES       20

#pragma pack(1)
typedef struct tagCOOKIEHEADER {
   WORD cbIndex;                    /* Size of index */
   WORD cCookies;                   /* Number of cookies */
} COOKIEHEADER;
#pragma pack()

#pragma pack(1)
typedef struct tagCOOKIEINDEX {
   WORD dwSize;                     /* Size of cookie */
   DWORD dwOffset;                  /* Offset of cookie in file */
} COOKIEINDEX;
#pragma pack()

typedef struct tagCOOKIE {
   COOKIEHEADER hdr;                /* Header */
   COOKIEINDEX FAR * lpIndex;       /* Pointer to index */
   HNDFILE fh;                      /* File handle */
} COOKIE, FAR * LPCOOKIE;

LPCOOKIE FASTCALL OpenCookieFile( PTL, BOOL );
BOOL FASTCALL LoadCookie( LPCOOKIE, int, char *, int );
BOOL FASTCALL SaveCookie( LPCOOKIE, int, char *, int );
void FASTCALL CloseCookieFile( LPCOOKIE );

static BOOL fHasCookieFile = FALSE;
static char szCookieFile[ _MAX_PATH ];

/* As from Ameol 2.10, cookies are no longer binary files but a single
 * COOKIES.INI file. This function scans every topic and attempts to
 * migrate cookies from the *.CKE file to the COOKIES.INI file.
 */
BOOL WINAPI EXPORT Amdb_MigrateCookies( void )
{
   PCAT pcat;
   PCL pcl;
   PTL ptl;

   for( pcat = pcatFirst; pcat; pcat = pcat->pcatNext )
      for( pcl = pcat->pclFirst; pcl; pcl = pcl->pclNext )
         for( ptl = pcl->ptlFirst; ptl; ptl = ptl->ptlNext )
            {
            LPCOOKIE lpCookie;

            if( NULL != ( lpCookie = OpenCookieFile( ptl, FALSE ) ) )
               {
               char szFileName[ _MAX_FNAME ];
               char cBuf[ 256 ];

               /* Migrate the settings across.
                */
               if( LoadCookie( lpCookie, 0, cBuf, 256 ) )
                  Amdb_SetCookie( ptl, NEWS_SERVER_COOKIE, cBuf );
               if( LoadCookie( lpCookie, 1, cBuf, 256 ) )
                  Amdb_SetCookie( ptl, FDL_PATH_COOKIE, cBuf );
               if( LoadCookie( lpCookie, 2, cBuf, 256 ) )
                  Amdb_SetCookie( ptl, MAIL_REPLY_TO_COOKIE_1, cBuf );
               if( LoadCookie( lpCookie, 2, cBuf, 256 ) )
                  Amdb_SetCookie( ptl, NEWS_REPLY_TO_COOKIE, cBuf );
               if( LoadCookie( lpCookie, 3, cBuf, 256 ) )
                  Amdb_SetCookie( ptl, NEWS_FULLNAME_COOKIE, cBuf );
               if( LoadCookie( lpCookie, 4, cBuf, 256 ) )
                  Amdb_SetCookie( ptl, NEWS_EMAIL_COOKIE, cBuf );
               CloseCookieFile( lpCookie );

               /* Vape the cookie file.
                */
               wsprintf( szFileName, "%s.CKE", (LPSTR)ptl->tlItem.szFileName );
               Amdb_DeleteFile( szFileName );
               }
            }
   return( TRUE );
}

/* This function writes a string to the topic cookie file.
 */
BOOL WINAPI EXPORT Amdb_SetCookie( PTL ptl, LPSTR lpKey, LPSTR lpValue )
{
   char szSection[ 256 ];

   if( !fHasCookieFile )
      {
      Amuser_GetActiveUserDirectory( szCookieFile, _MAX_PATH );
      strcat( szCookieFile, "\\" COOKIE_FILENAME );
      fHasCookieFile = TRUE;
      }

   /*
   !!SM!! - Check we don't write out strings with just spaces in them
   */
   StripLeadingTrailingSpaces( lpValue );

   wsprintf( szSection, "%s/%s", ptl->pcl->clItem.szFolderName, ptl->tlItem.szTopicName );
   return( 0 != WritePrivateProfileString( szSection, lpKey, lpValue, szCookieFile ) );
}

/* This function retrieves a string from the topic cookie file.
 */
BOOL WINAPI EXPORT Amdb_GetCookie( PTL ptl, LPSTR lpKey, LPSTR lpBuf, LPSTR lpDef, int cbMax )
{
   char szSection[ 256 ];
   BOOL ret;
   
   if( !fHasCookieFile )
   {
      Amuser_GetActiveUserDirectory( szCookieFile, _MAX_PATH );
      strcat( szCookieFile, "\\" COOKIE_FILENAME );
      fHasCookieFile = TRUE;
   }
   /* It turns out that ParseFolderPathname doesn't always return a PTL, it can return
      a PCL or PCAT. So we have to check and if it's not a PTL then return the default.
   */
   if( Amdb_IsTopicPtr( ptl ) )
      wsprintf( szSection, "%s/%s", ptl->pcl->clItem.szFolderName, ptl->tlItem.szTopicName );
   else
      wsprintf( szSection, "%s/%s", "Oh_I_Dont_Know", "Think_Of_Something_Amusing" );
   
   ret = (  0 != GetPrivateProfileString( szSection, lpKey, lpDef, lpBuf, cbMax, szCookieFile ));
   
   if (lpBuf[0]=='\x0')
      strcpy(lpBuf,lpDef);
   
   return( ret );
}

/* This function moves cookies for a topic to another folder.
 */
void FASTCALL Amdb_MoveCookies( PTL ptl, PCL pclOld )
{
   char szSectionOld[ 256 ];
   LPSTR lpBuf;

   INITIALISE_PTR(lpBuf);
   if( !fHasCookieFile )
      {
      Amuser_GetActiveUserDirectory( szCookieFile, _MAX_PATH );
      strcat( szCookieFile, "\\" COOKIE_FILENAME );
      fHasCookieFile = TRUE;
      }

   /* If pclOld is NULL, delete the section.
    */
   if( NULL == pclOld )
      {
      wsprintf( szSectionOld, "%s/%s", ptl->pcl->clItem.szFolderName, ptl->tlItem.szTopicName );
      WritePrivateProfileString( szSectionOld, NULL, NULL, szCookieFile );
      return;
      }

   /* Copy across cookies to the new folder, if
    * one was specified.
    */
   wsprintf( szSectionOld, "%s/%s", pclOld->clItem.szFolderName, ptl->tlItem.szTopicName );
   if( fNewMemory( &lpBuf, 32767 ) )
      {
      if( GetPrivateProfileString( szSectionOld, NULL, "", lpBuf, 32767, szCookieFile ) )
         {
         char szSection[ 256 ];
         LPSTR lpBufPtr;

         wsprintf( szSection, "%s/%s", ptl->pcl->clItem.szFolderName, ptl->tlItem.szTopicName );
         for( lpBufPtr = lpBuf; *lpBufPtr; lpBufPtr += strlen( lpBufPtr ) + 1 )
            {
            char szData[ 256 ];

            GetPrivateProfileString( szSectionOld, lpBufPtr, "", szData, 256, szCookieFile );
            WritePrivateProfileString( szSection, lpBufPtr, szData, szCookieFile );
            }
         }
      FreeMemory( &lpBuf );
      }
   WritePrivateProfileString( szSectionOld, NULL, NULL, szCookieFile );
}

/* This function opens the cookie file for the specified
 * topic. If it doesn't exist and fCreate is TRUE, then we
 * create it.
 */
LPCOOKIE FASTCALL OpenCookieFile( PTL ptl, BOOL fCreate )
{
   char szFileName[ 13 ];
   LPCOOKIE lpCookie;
   HNDFILE fh;

   INITIALISE_PTR(lpCookie);

   /* If cookie loaded, just reopen the file.
    */
   wsprintf( szFileName, "%s.CKE", (LPSTR)ptl->tlItem.szFileName );

   /* Open the cookie file.
    */
   if( HNDFILE_ERROR == ( fh = Amdb_OpenFile( szFileName, AOF_READWRITE ) ) )
      {
      if( fCreate )
         {
         /* Create the file and initialise it.
          */
         fh = Amdb_CreateFile( szFileName, 0 );
         if( HNDFILE_ERROR != fh )
            {
            COOKIEINDEX FAR * lpidxCookie;
            COOKIEHEADER cookieHdr;

            INITIALISE_PTR(lpidxCookie);

            cookieHdr.cCookies = 0;
            cookieHdr.cbIndex = MAX_COOKIES * sizeof(COOKIEINDEX);
            if( (COOKIEINDEX FAR *)fNewMemory( &lpidxCookie, cookieHdr.cbIndex ) )
               {
               memset( lpidxCookie, 0, cookieHdr.cbIndex );
               if( !fNewMemory( &lpCookie, sizeof(COOKIE) ) )
                  FreeMemory( &lpidxCookie );
               else
                  {
                  lpCookie->hdr = cookieHdr;
                  lpCookie->lpIndex = lpidxCookie;
                  lpCookie->fh = fh;

                  /* Write to the cookie file.
                   */
                  Amfile_Write( fh, (LPSTR)&cookieHdr, sizeof(COOKIEHEADER) );
                  Amfile_Write( fh, (LPSTR)lpidxCookie, cookieHdr.cbIndex );
                  }
               }

            /* Failed? Close file.
             */
            if( NULL == lpCookie )
               Amfile_Close( fh );
            }
         }
      return( lpCookie );
      }

   /* If we got our cookie file, read the
    * fixed-size header.
    */
   if( HNDFILE_ERROR != fh )
      {
      COOKIEHEADER cookieHdr;
      int cRead;

      cRead = Amfile_Read( fh, &cookieHdr, sizeof(COOKIEHEADER) );
      if( cRead == sizeof(COOKIEHEADER) )
         {
         COOKIEINDEX FAR * lpidxCookie;

         INITIALISE_PTR(lpidxCookie);

         /* Allocate memory for the index.
          */
         if( (COOKIEINDEX FAR *)fNewMemory( &lpidxCookie, cookieHdr.cbIndex ) )
            {
            cRead = Amfile_Read( fh, lpidxCookie, cookieHdr.cbIndex );
            if( cRead == (int)cookieHdr.cbIndex )
               {
               /* Allocate a cookie header.
                */
               if( !fNewMemory( &lpCookie, sizeof(COOKIE) ) )
                  FreeMemory( &lpidxCookie );
               else
                  {
                  lpCookie->hdr = cookieHdr;
                  lpCookie->lpIndex = lpidxCookie;
                  lpCookie->fh = fh;
                  }
               }
            }
         }

      /* Failed? Close file.
       */
      if( NULL == lpCookie )
         Amfile_Close( fh );
      }

   /* All okay?
    */
   return( lpCookie );
}

/* This function closes the open cookie file handle.
 */
void FASTCALL CloseCookieFile( LPCOOKIE lpCookie )
{
   if( HNDFILE_ERROR != lpCookie->fh )
      {
      Amfile_Close( lpCookie->fh );
      lpCookie->fh = HNDFILE_ERROR;
      }
}

/* This function loads a cookie.
 */
BOOL FASTCALL LoadCookie( LPCOOKIE lpCookie, int nCookie, char * lpstr, int cbMax )
{
   /* No cookie if index is out of range or the size of the
    * entry is zero.
    */
   if( nCookie < MAX_COOKIES )
      if( 0L != lpCookie->lpIndex[ nCookie ].dwSize )
         {
         /* Seek to the file and load the cookie.
          */
         if( lpCookie->lpIndex[ nCookie ].dwSize < (DWORD)cbMax )
            cbMax = lpCookie->lpIndex[ nCookie ].dwSize;
         Amfile_SetPosition( lpCookie->fh, lpCookie->lpIndex[ nCookie ].dwOffset, ASEEK_BEGINNING );
         if( (UINT)cbMax == Amfile_Read( lpCookie->fh, lpstr, cbMax ) )
            return( TRUE );
         }
   return( FALSE );
}
/* This function handles renaming of a topic/folder with cookies..
 */
BOOL WINAPI EXPORT Amdb_RenameCookies( LPSTR lpOldName, LPVOID lpNewPtr, BOOL fIsTopic )
{
   char szSectionOld[ 8192 ];
   LPSTR lpBuf;
   PTL newptl;

   INITIALISE_PTR(lpBuf);
   if( !fHasCookieFile )
      {
      Amuser_GetActiveUserDirectory( szCookieFile, _MAX_PATH );
      strcat( szCookieFile, "\\" COOKIE_FILENAME );
      fHasCookieFile = TRUE;
      }

   newptl = (PTL)lpNewPtr;

   if( fIsTopic )
      wsprintf( szSectionOld, "%s/%s", newptl->pcl->clItem.szFolderName, lpOldName );
   else
      wsprintf( szSectionOld, "%s/%s", lpOldName, newptl->tlItem.szTopicName );

   /* Copy across cookies to the new folder, if
    * one was specified.
    */
   if( fNewMemory( &lpBuf, 32767 ) )
      {
      if( GetPrivateProfileString( szSectionOld, NULL, "", lpBuf, 32767, szCookieFile ) )
         {
         char szSection[ 256 ];

         LPSTR lpBufPtr;

         wsprintf( szSection, "%s/%s", newptl->pcl->clItem.szFolderName, newptl->tlItem.szTopicName );
         for( lpBufPtr = lpBuf; *lpBufPtr; lpBufPtr += strlen( lpBufPtr ) + 1 )
            {
            char szData[ 256 ];

            GetPrivateProfileString( szSectionOld, lpBufPtr, "", szData, 256, szCookieFile );
            WritePrivateProfileString( szSection, lpBufPtr, szData, szCookieFile );
            WritePrivateProfileString( szSectionOld, NULL, NULL, szCookieFile );
            }
         }
      FreeMemory( &lpBuf );
      }


return( TRUE );
}
