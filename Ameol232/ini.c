/* INI.C - Functions that read configuration data
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
#include "palrc.h"
#include <ctype.h>
#include <string.h>
#include "ini.h"

char * FASTCALL RawIniReadInt( char *, int * );

/* This function reads some text from an INI string field
 * then skips any separating comma.
 */
char * FASTCALL IniReadText( char * psz, char * pStore, int cbStore )
{
   register int i;
   char chEnd;

   chEnd = ',';
   if( *psz == '"' )
      chEnd = *psz++;
   for( i = 0; *psz && *psz != chEnd; )
      {
      if( i < cbStore )
         pStore[ i++ ] = *psz;
      ++psz;
      }
   pStore[ i ] = '\0';
   if( chEnd == '"' && *psz == '"' )
      ++psz;
   if( *psz == ',' )
      ++psz;
   return( psz );
}

/* This function reads an integer value from an INI value field
 * then skips any separating comma.
 */
char * FASTCALL IniReadInt( char * psz, int * pInt )
{
   psz = RawIniReadInt( psz, pInt );
   if( *psz == ',' )
      ++psz;
   return( psz );
}

/* This function reads an integer value from an INI value field.
 */
char * FASTCALL RawIniReadInt( char * psz, int * pInt )
{
   *pInt = 0;
   while( isdigit( *psz ) )
      *pInt = *pInt * 10 + ( *psz++ - '0' );
   return( psz );
}

/* This function reads a long integer value from an INI value field
 * then skips any separating comma.
 */
char * FASTCALL IniReadLong( char * psz, long * pLong )
{
   *pLong = 0;
   while( isdigit( *psz ) )
      *pLong = *pLong * 10 + ( *psz++ - '0' );
   if( *psz == ',' )
      ++psz;
   return( psz );
}

/* This function reads a date from an INI value field
 * then skips any separating comma.
 */
char * FASTCALL IniReadDate( char * psz, AM_DATE * pDate )
{
   int iMonth;
   int iYear;
   int iDay;

   iMonth = iYear = iDay = 0;
   psz = RawIniReadInt( psz, &iDay );
   if( *psz == '/' )
      {
      ++psz;
      psz = RawIniReadInt( psz, &iMonth );
      if( *psz == '/' )
         {
         ++psz;
         psz = RawIniReadInt( psz, &iYear );
         }
      }
   if( *psz == ',' )
      ++psz;
   pDate->iDay = iDay;
   pDate->iMonth = iMonth;
   pDate->iYear = iYear;
   return( psz );
}

/* This function reads a time from an INI value field
 * then skips any separating comma.
 */
char * FASTCALL IniReadTime( char * psz, AM_TIME * pTime )
{
   int iMinute;
   int iSecond;
   int iHour;

   iMinute = iSecond = iHour = 0;
   psz = RawIniReadInt( psz, &iHour );
   if( *psz == ':' )
      {
      ++psz;
      psz = RawIniReadInt( psz, &iMinute );
      if( *psz == ':' )
         {
         ++psz;
         psz = RawIniReadInt( psz, &iSecond );
         }
      }
   if( *psz == ',' )
      ++psz;
   pTime->iHour = iHour;
   pTime->iMinute = iMinute;
   pTime->iSecond = iSecond;
   return( psz );
}

/* This function writes a text string to an INI string field. If
 * the string contains commas, it is embedded in quotes.
 */
char * FASTCALL IniWriteText( char * psz, char * pszText )
{
   if( strchr( pszText, ',' ) )
      {
      *psz++ = '"';
      while( *pszText )
         *psz++ = *pszText++;
      *psz++ = '"';
      }
   else
      {
      while( *pszText )
         *psz++ = *pszText++;
      }
   *psz = '\0';
   return( psz );
}

/* This function writes an unsigned value to an INI string field.
 * This function accepts a LONG argument and can be used for any
 * value.
 */
char * FASTCALL IniWriteUnsignedValue( char * psz, DWORD nValue )
{
   _ultoa( nValue, psz, 10 );
   psz += strlen( psz );
   return( psz );
}

/* This function writes a signed value to an INI string field.
 * This function accepts a LONG argument and can be used for any
 * value.
 */
char * FASTCALL IniWriteValue( char * psz, LONG nValue )
{
   _ltoa( nValue, psz, 10 );
   psz += strlen( psz );
   return( psz );
}
