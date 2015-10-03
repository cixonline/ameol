/* TIME.C - Useful time functions
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

#include "warnings.h"
#include "windows.h"
#include "pdefs.h"
#include <time.h>
#include "amuser.h"

static char sTime[ 10 ];
static char s1159[ 10 ];
static char s2359[ 10 ];
static int iTLZero;
static int iTime;
static BOOL fInited = FALSE;

/* This function re-reads the time settings from WIN.INI on receipt of a
 * WM_WININICHANGED message by the main window.
 */
void WINAPI EXPORT Amdate_RefreshTimeIniSettings( LPCSTR lpszSection )
{
   if( lpszSection == NULL || lstrcmpi( lpszSection, "intl" ) == 0 )
      {
      iTime = GetProfileInt( "intl", "iTime", 1 );
      iTLZero = GetProfileInt( "intl", "iTLZero", 0 );
      GetProfileString( "intl", "sTime", ":", sTime, 10 );
      GetProfileString( "intl", "s2359", "pm", s2359, 10 );
      GetProfileString( "intl", "s1159", "pm", s1159, 10 );
      fInited = TRUE;
      }
}

/* This function fills the DATE structure with the current day, month, year
 * and index of the current day of the week (where Sunday==0)
 */
void WINAPI EXPORT Amdate_GetCurrentTime( AM_TIME FAR * pTime )
{
   SYSTEMTIME st;

   GetLocalTime( &st );
   pTime->iHour = st.wHour;
   pTime->iMinute = st.wMinute;
   pTime->iSecond = st.wSecond;
   pTime->iHSeconds = 0;
}

/****************************************************************************
* GetPackedCurrentTime
*
* Description:
*  This function returns a 16-bit integer that contains the current time
*  in packed format. By packing the time this way, it can be more easily
*  manipulated and stored.
*
*  The format of a packed time is shown below:
*
*   15                               0
*    ÚÄÂÄÂÄÂÄÂÄÂÄÂÄÂÄÂÄÂÄÂÄÂÄÂÄÂÄÂÄÂÄ¿
*    ³h³h³h³h³h³m³m³m³m³m³m³x³x³x³x³x³
*    ÀÄÁÄÁÄÁÄÁÄÁÄÁÄÁÄÁÄÁÄÁÄÁÄÁÄÁÄÁÄÁÄÙ
*
*  Where:   hhhhh = hours, 0..23
*           mmmmmm = minutes, 0..59
*           xxxxx = seconds, 0..31 in 2-second increments
*
*  Note that owing to the method of storing the number of seconds in the
*  packed integer, the seconds resolution is reduced to 2-second increments.
*
*  The time also appears in this format at offsets 22 and 23 in the file
*  directory (See DOS Technical Reference, February 1985, p.5-12)
*
* Input:
*  None
*
* Return Value
*  Returns the packed time as a 16-bit integer.
****************************************************************************/

WORD WINAPI EXPORT Amdate_GetPackedCurrentTime( void )
{
   AM_TIME aTime;
   WORD nTime;

   Amdate_GetCurrentTime( &aTime );
   nTime = ( aTime.iSecond / 2 ) |
           ( aTime.iMinute << 5 ) |
           ( aTime.iHour << 11 );
   return( nTime );
}

WORD WINAPI EXPORT Amdate_PackTime( AM_TIME FAR * pTime )
{
   WORD nTime;

   nTime = ( pTime->iSecond / 2 ) |
           ( ( pTime->iMinute ) << 5 ) |
           ( ( pTime->iHour ) << 11 );
   return( nTime );
}

/****************************************************************************
* UnpackTime
*
* Description:
*  This function unpacks the time in nTime, which is assumed to have been
*  packed with GetPackedCurrentTime() or obtained from the DOS filing
*  system. The hundreths-seconds, which were not packed, are set to zero.
*
* Input:
*  nTime = packed time in 16-bit format (See GetPackedCurrentTime)
*  pTime = points to a TIME structure.
*
* Return Value
*  None
****************************************************************************/

void WINAPI EXPORT Amdate_UnpackTime( WORD nTime, AM_TIME FAR * pTime )
{
   pTime->iSecond = ( nTime & 31 ) * 2;
   pTime->iMinute = ( nTime >> 5 ) & 63;
   pTime->iHour = ( nTime >> 11 ) & 31;
   pTime->iHSeconds = 0;
}

/****************************************************************************
* FormatTime
*
* Description:
*  This function formats the time passed in the TIME structure using the
*  current country time formatting obtained from the [intl] section of
*  the WIN.INI file.
*
* Input:
*  pTime = pointer to TIME structure
*  pBuf = pointer to buffer to receive time string. If NULL, then
*         FormatTime uses it's own internal buffer.
*
* Output:
*  Returns pointer to formatted time string. If pBuf was NULL, then the
*  return value is a pointer to a static 40-character buffer that will be
*  overwritten by the next call to FormatTime with pBuf set to NULL.
****************************************************************************/

LPSTR WINAPI EXPORT Amdate_FormatTime( AM_TIME FAR * pTime, LPSTR pBuf )
{
   static char aBuf[ 40 ];

   if( pBuf == NULL )
      pBuf = aBuf;
   if( !fInited )
      Amdate_RefreshTimeIniSettings( NULL );
   if( iTime == 1 )
      {
      wsprintf( pBuf, iTLZero ? "%2.2d%s%2.2d%s%2.2d" : "%d%s%2.2d%s%2.2d",
                pTime->iHour, (LPSTR)sTime, pTime->iMinute, (LPSTR)sTime,
                pTime->iSecond );
      }
   else
      {
      wsprintf( pBuf, iTLZero ? "%2.2d%s%2.2d" : "%d%s%2.2d",
         pTime->iHour > 12 ? pTime->iHour - 12 : pTime->iHour, (LPSTR)sTime,
            pTime->iMinute );
      if ( *s2359 || *s1159 )
         {
         lstrcat( pBuf, " " );
         lstrcat( pBuf, pTime->iHour >= 12 ? s2359 : s1159 );
         }
      }
   return( pBuf );
}

/* Given a count of seconds, this function expands it into
 * a string.
 */
LPSTR WINAPI EXPORT Amdate_FormatLongTime( DWORD dwSeconds )
{
   static char szTime[ 50 ];
   int min, sec;
   char * pszTime;

   min = 0;
   sec = 0;
   min = (int)( dwSeconds / 60 );
   sec = (int)( dwSeconds % 60 );
   if( sec == 0 )
      sec++;
   pszTime = szTime;
   if( min )
      pszTime += wsprintf( pszTime, "%d min. ", min );
   if( sec )
      pszTime += wsprintf( pszTime, "%d sec.", sec );
   return( szTime );
}
