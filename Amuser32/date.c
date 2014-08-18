/* DATE.C - Useful date functions
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
#include "windows.h"
#include "pdefs.h"
#include <ctype.h>
#include <time.h>
#include "amuser.h"

char * sDay[ 7 ] = { "Sunday", "Monday", "Tuesday", "Wednesday",
      "Thursday", "Friday", "Saturday" };
char * sMonth[ 12 ] = { "January", "February", "March", "April",
      "May", "June", "July", "August", "September", "October",
      "November", "December" };

static char sShortDate[ 21 ];
static char sLongDate[ 21 ];
static BOOL fInited = FALSE;

/* This function re-reads the date settings from WIN.INI on receipt of a
 * WM_WININICHANGED message by the main window.
 */
void WINAPI EXPORT Amdate_RefreshDateIniSettings( LPCSTR lpszSection )
{
   if( lpszSection == NULL || lstrcmpi( lpszSection, "intl" ) == 0 )
      {
      ReadRegistryKey( HKEY_CURRENT_USER, "Control Panel\\International", "sLongDate", "", sLongDate, sizeof( sLongDate ) );
      ReadRegistryKey( HKEY_CURRENT_USER, "Control Panel\\International", "sShortDate", "", sShortDate, sizeof( sShortDate ) );
      if( !( *sLongDate ) )
         GetProfileString( "intl", "sLongDate", "dd mmmm yyyy", sLongDate, sizeof( sLongDate ) );
      if( !( *sShortDate ) )
         GetProfileString( "intl", "sShortDate", "dd/mm/yy", sShortDate, sizeof( sShortDate ) );
      AnsiLower( sLongDate );
      AnsiLower( sShortDate );
      fInited = TRUE;
      }
}

/* This function fills the DATE structure with the current day, month, year
 * and index of the current day of the week (where Sunday==0)
 */
void WINAPI EXPORT Amdate_GetCurrentDate( AM_DATE FAR * pDate )
{
#ifdef WIN32
   SYSTEMTIME st;

   GetLocalTime( &st );
   pDate->iDay = st.wDay;
   pDate->iMonth = st.wMonth;
   pDate->iYear = st.wYear;
   pDate->iDayOfWeek = st.wDayOfWeek;
#else
   time_t lTime;
   struct tm * ptmLocal;

   time( &lTime );
   ptmLocal = localtime( &lTime );
   pDate->iDay = ptmLocal->tm_mday;
   pDate->iMonth = ptmLocal->tm_mon + 1;
   pDate->iYear = ptmLocal->tm_year;
   pDate->iDayOfWeek = ptmLocal->tm_wday;
#endif
   if( pDate->iYear < 100 )
   {
      if( pDate->iYear < 80 )
         pDate->iYear += 2000;
      else
         pDate->iYear += 1900;
   }
   else if( pDate->iYear < 1000 )
      pDate->iYear += 1900;

}

/* This function returns time-zone information.
 */
void WINAPI EXPORT Amdate_GetTimeZoneInformation( AM_TIMEZONEINFO * pTZInfo )
{
   time_t lTime;
   struct tm * ptmLocal;

   _tzset();
   time( &lTime );
   ptmLocal = localtime( &lTime );
   if( ptmLocal->tm_isdst )
      {
      pTZInfo->diff = 1;
      if( strlen( _tzname[ 1 ] ) <= 3 )
         strcpy( pTZInfo->szZoneName, *_tzname[ 1 ] ? _tzname[ 1 ] : "BST" );
      else
         strcpy( pTZInfo->szZoneName, "BST" );
      }
   else
      {
      pTZInfo->diff = 0;
      strcpy( pTZInfo->szZoneName, _tzname[ 0 ] );
      }
}

/* This function returns a 16-bit integer that contains the current date
 * in packed format. By packing the date this way, it can be more easily
 * manipulated and stored.
 *
 * The format of a packed date is shown below:
 *
 *  15                               0
 *   ÚÄÂÄÂÄÂÄÂÄÂÄÂÄÂÄÂÄÂÄÂÄÂÄÂÄÂÄÂÄÂÄ¿
 *   ³y³y³y³y³y³y³y³m³m³m³m³d³d³d³d³d³
 *   ÀÄÁÄÁÄÁÄÁÄÁÄÁÄÁÄÁÄÁÄÁÄÁÄÁÄÁÄÁÄÁÄÙ
 *
 * Where:   yyyyyyy = year, 0..19 (1980-2099)
 *          mmmm = month, 1..12
 *          ddddd = day, 1..31
 *
 * The date also appears in this format at offsets 24 and 25 in the file
 * directory (See DOS Technical Reference, February 1985, p.513)
 *
 * NOTE: The day-of-week field is not packed, and thus must be recomputed
 * when unpacking into a DATE structure. The UnpackDate function does this
 * automatically.
 */
WORD WINAPI EXPORT Amdate_GetPackedCurrentDate( void )
{
   time_t lTime;
   struct tm * ptmLocal;
   WORD nDate;

   time( &lTime );
   if( lTime == -1 )
      return( -1 );
   ptmLocal = localtime( &lTime );
   nDate = ptmLocal->tm_mday |
           ( ( ptmLocal->tm_mon + 1 ) << 5 ) |
           ( ( ptmLocal->tm_year - 80 ) << 9 );
   return( nDate );
}

/* This function adjusts the date by the given number of days. If wDays is
 * negative, it returns the date wDays before the date in lpDate. If wDays
 * is positive, it returns the date wDays after the date in lpDate.
 */
void WINAPI EXPORT Amdate_AdjustDate( AM_DATE FAR * lpDate, int wDays )
{
   static int DaysInMonth[ 12 ] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

   if( wDays > 0 ) {
      while( wDays-- ) {
         WORD iDaysInMonth;

         iDaysInMonth = DaysInMonth[ lpDate->iMonth - 1 ];
         if( lpDate->iMonth == 2 && ( lpDate->iYear % 200 == 0 || lpDate->iYear % 4 == 0 ) )
            ++iDaysInMonth;
         if( lpDate->iDay < iDaysInMonth )
            ++lpDate->iDay;
         else {
            if( lpDate->iMonth < 12 )
               ++lpDate->iMonth;
            else {
               ++lpDate->iYear;
               lpDate->iMonth = 1;
               }
            lpDate->iDay = 1;
            }
         }
      }
   else if( wDays < 0 ) {
      while( wDays++ )
         if( lpDate->iDay > 1 )
            --lpDate->iDay;
         else {
            if( lpDate->iMonth > 1 )
               --lpDate->iMonth;
            else {
               --lpDate->iYear;
               lpDate->iMonth = 12;
               }
            lpDate->iDay = DaysInMonth[ lpDate->iMonth - 1 ];
            if( 2 == lpDate->iMonth && ( lpDate->iYear % 200 == 0 || lpDate->iYear % 4 == 0 ) )
               ++lpDate->iDay;
            }
      }
}

/* This function unpacks the date in nDate, which is assumed to have been
 * packed with GetPackedCurrentDate() or obtained from the DOS filing
 * system. It uses Zeller's Congruence to compute the day of the week
 * given just the day, month and year.
 */
WORD WINAPI EXPORT Amdate_PackDate( AM_DATE FAR * pDate )
{
   WORD nDate;

   nDate = pDate->iDay |
           ( ( pDate->iMonth ) << 5 ) |
           ( ( pDate->iYear - 1980 ) << 9 );
   return( nDate );
}

/* This function unpacks the date in nDate, which is assumed to have been
 * packed with GetPackedCurrentDate() or obtained from the DOS filing
 * system. It uses Zeller's Congruence to compute the day of the week
 * given just the day, month and year.
 */
void WINAPI EXPORT Amdate_UnpackDate( WORD nDate, AM_DATE FAR * pDate )
{
   int iCentury, iYear, iMonth, iDay;
   int iZeller;

   pDate->iDay = nDate & 31;
   pDate->iMonth = ( nDate >> 5 ) & 15;
   pDate->iYear = 1980 + ( ( nDate >> 9 ) & 127 );
   iYear = pDate->iYear;
   iDay = pDate->iDay;
   iMonth = pDate->iMonth;
   if( iMonth < 3 ) {
      --iYear;
      iMonth += 12;
      }
   iCentury = iYear / 100;
   iYear = iYear % 100;
   iZeller = iDay + ( ( ( iMonth + 1 ) * 26 ) / 10 ) + iYear + ( iYear / 4 );
   iZeller = iZeller + ( iCentury / 4 ) + 5 * iCentury;
   pDate->iDayOfWeek = ( iZeller - 1 ) % 7;
}

/* This function formats the date passed in the DATE structure using the
 * current country date formatting obtained from the [intl] section of
 * the WIN.INI file.
 *
 * Caveats: Weekday and month names are in English - no provision for
 * foreign language names. (Could pull these in from a external file.)
 */
LPSTR WINAPI EXPORT Amdate_FormatLongDate( AM_DATE FAR * pDate, LPSTR pBuf )
{
   return( Amdate_FormatFullDate( pDate, NULL, pBuf, sLongDate ) );
}

LPSTR WINAPI EXPORT Amdate_FormatShortDate( AM_DATE FAR * pDate, LPSTR pBuf )
{
   return( Amdate_FormatFullDate( pDate, NULL, pBuf, sShortDate ) );
}

LPSTR WINAPI EXPORT Amdate_FormatFullDate( AM_DATE FAR * pDate, AM_TIME FAR * pTime, LPSTR pBuf, LPSTR psDate )
{
   static char aBuf[ 40 ];
   register int c;
   LPSTR pBuf2;
   BOOL fQuote;

   if( pBuf == NULL )
      pBuf = aBuf;
   pBuf2 = pBuf;
   if( !fInited )
      Amdate_RefreshDateIniSettings( NULL );
   fQuote = 0;
   while( *psDate )
      if( *psDate == '\'' )
         {
         fQuote ^= 1;
         ++psDate;
         }
      else if( !fQuote && *psDate == 'h' && NULL != pTime )
         {
         for( c = 0; *psDate == 'h'; ++psDate, ++c );
         if( c == 1 )
            wsprintf( pBuf, "%d", pTime->iHour );
         else if( c == 2 )
            wsprintf( pBuf, "%2.2d", pTime->iHour );
         pBuf += lstrlen( pBuf );
         }
      else if( !fQuote && *psDate == 'n' && NULL != pTime )
         {
         for( c = 0; *psDate == 'n'; ++psDate, ++c );
         if( c == 1 )
            wsprintf( pBuf, "%d", pTime->iMinute );
         else if( c == 2 )
            wsprintf( pBuf, "%2.2d", pTime->iMinute );
         pBuf += lstrlen( pBuf );
         }
      else if( !fQuote && *psDate == 's' && NULL != pTime )
         {
         for( c = 0; *psDate == 's'; ++psDate, ++c );
         if( c == 1 )
            wsprintf( pBuf, "%d", pTime->iSecond );
         else if( c == 2 )
            wsprintf( pBuf, "%2.2d", pTime->iSecond );
         pBuf += lstrlen( pBuf );
         }
      else if( !fQuote && *psDate == 'd' )
         {
         for( c = 0; *psDate == 'd'; ++psDate, ++c );
         if (pDate->iDayOfWeek > 7)
         {
            wsprintf( pBuf, "%d", pDate->iDayOfWeek );
         }
         else
         {
            if( c == 1 )
               wsprintf( pBuf, "%d", pDate->iDay );
            else if( c == 2 )
               wsprintf( pBuf, "%2.2d", pDate->iDay );
            else if( c == 3 )
               wsprintf( pBuf, "%3.3s", (LPSTR)sDay[ pDate->iDayOfWeek ] );
            else
               wsprintf( pBuf, "%s", (LPSTR)sDay[ pDate->iDayOfWeek ] );
         }
         pBuf += lstrlen( pBuf );
         }
      else if( !fQuote && *psDate == 'm' )
         {
         if( pDate->iMonth == 0 )
            pDate->iMonth = 1;

         for( c = 0; *psDate == 'm'; ++psDate, ++c );
         if (pDate->iMonth > 12)
         {
            wsprintf( pBuf, "%d", pDate->iMonth );
         }
         else
         {
            if( c == 1 )
               wsprintf( pBuf, "%d", pDate->iMonth );
            else if( c == 2 )
               wsprintf( pBuf, "%2.2d", pDate->iMonth );
            else if( c == 3 )
               wsprintf( pBuf, "%3.3s", (LPSTR)sMonth[ pDate->iMonth - 1 ] );
            else
               wsprintf( pBuf, "%s", (LPSTR)sMonth[ pDate->iMonth - 1 ] );
         }
         pBuf += lstrlen( pBuf );
         }
      else if( !fQuote && *psDate == '"' )
         {
         ++psDate;
         while( *psDate && *psDate != '"' )
            *pBuf++ = *psDate;
         if( *psDate == '"' )
            ++psDate;
         }
      else if( !fQuote && *psDate == 'y' )
         {
         int iYear;

         for( c = 0; *psDate == 'y'; ++psDate, ++c );
         if( ( iYear = pDate->iYear ) < 100 )
            {
            if( iYear < 80 )
               iYear += 2000;
            else
               iYear += 1900;
            }
         else if( iYear < 1000 )
            iYear += 1900;
         if( c == 2 )
            wsprintf( pBuf, "%2.2d", iYear % 100 );
         else
            wsprintf( pBuf, "%d", iYear );
         pBuf += lstrlen( pBuf );
         }
      else *pBuf++ = *psDate++;
   *pBuf = '\0';
   return( pBuf2 );
}

/* This function shows the packed date/time field in a friendly
 * format.
 */
char * WINAPI EXPORT Amdate_FriendlyDate( char * pBuf, WORD wDate, WORD wTime )
{
   static char sBuf[ 100 ];
   AM_DATE curDate;
   AM_TIME curTime;
   AM_DATE theDate;
   AM_TIME theTime;

   /* Use local buffer.
    */
   if( NULL == pBuf )
      pBuf = sBuf;

   /* Get dates in expanded format.
    */
   Amdate_GetCurrentDate( &curDate );
   Amdate_GetCurrentTime( &curTime );
   Amdate_UnpackDate( wDate, &theDate );
   Amdate_UnpackTime( wTime, &theTime );

   /* Was it today? Just show the time
    */
   *pBuf = '\0';
   if( memcmp( &curDate, &theDate, sizeof(AM_DATE) ) == 0 )
      strcat( pBuf, "today" );
   else if( curDate.iMonth == theDate.iMonth && curDate.iYear == theDate.iYear )
      {
      int diff;

      /* Same month, so prettify the date by saying something
       * like 'yesterday' or 'x days ago'.
       */
      diff = curDate.iDay - theDate.iDay;
      if( 1 == diff )
         strcat( pBuf, "yesterday" );
      else
         wsprintf( pBuf + strlen( pBuf ), "%u days ago", diff );
      }
   else
      wsprintf( pBuf + strlen( pBuf ), "on %s", Amdate_FormatLongDate( &theDate, NULL ) );

   /* Concatenate the time
    */
   strcat( pBuf, " at " );
   strcat( pBuf, Amdate_FormatTime( &theTime, NULL ) );
   return( pBuf );
}
