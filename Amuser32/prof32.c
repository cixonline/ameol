/* PROF32.C - Interface to the Win32 registry
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
#include <windows.h>
#include <windowsx.h>
#include "amlib.h"
#include "amuser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define  THIS_FILE   __FILE__

#define  MAX_VALUE_NAME    128
#define  MAX_SUBKEY_NAME   128

extern BOOL fUseINIfile; // 2.56.2051 FS#121

extern char szActiveUser[ 40 ];
extern char * szdirUser;

int WINAPI INI_Amuser_GetPPInt( LPCSTR lpSection, LPSTR lpKey, int nDefault );
int WINAPI INI_Amuser_GetPrivateProfileInt( LPCSTR lpSection, LPSTR lpKey, int nDefault, LPCSTR lpszIniFile );
float WINAPI INI_Amuser_GetPPFloat( LPCSTR lpSection, LPSTR lpKey, float nDefault );
float WINAPI INI_Amuser_GetPrivateProfileFloat( LPCSTR lpSection, LPSTR lpKey, float nDefault, LPCSTR lpszIniFile );
LONG WINAPI INI_Amuser_GetPPLong( LPCSTR lpSection, LPSTR lpKey, long lDefault );
LONG WINAPI INI_Amuser_GetPrivateProfileLong( LPCSTR lpSection, LPSTR lpKey, long lDefault, LPCSTR lpszIniFile );
int WINAPI INI_Amuser_GetPPString( LPCSTR lpSection, LPSTR lpKey, LPCSTR lpDefBuf, LPSTR lpBuf, int cbBuf );
int WINAPI INI_Amuser_GetPrivateProfileString( LPCSTR lpSection, LPSTR lpKey, LPCSTR lpDefBuf, LPSTR lpBuf, int cbBuf, LPCSTR lpszIniFile );
int WINAPI INI_Amuser_GetLMString( LPCSTR lpSection, LPSTR lpKey, LPCSTR lpDefBuf, LPSTR lpBuf, int cbBuf );
int WINAPI INI_Amuser_GetLMInt( LPCSTR lpSection, LPSTR lpKey, int nDefault );
int WINAPI INI_Amuser_GetPPArray( LPCSTR lpSection, LPSTR lpKey, LPINT lpint, int size );
int WINAPI INI_Amuser_DeleteUser( LPSTR lpUser );
int WINAPI INI_Amuser_WritePPInt( LPCSTR lpSection, LPSTR lpKey, int nValue );
BOOL WINAPI INI_Amuser_WritePrivateProfileInt( LPCSTR lpSection, LPSTR lpKey, int nValue, LPCSTR lpszIniFile );
int WINAPI INI_Amuser_WritePPLong( LPCSTR lpSection, LPSTR lpKey, long nValue );
int WINAPI INI_Amuser_WritePrivateProfileLong( LPCSTR lpSection, LPSTR lpKey, long nValue, LPCSTR lpszIniFile );
int WINAPI INI_Amuser_WriteLMString( LPCSTR lpSection, LPSTR lpKey, LPSTR lpBuf );
int WINAPI INI_Amuser_WritePPString( LPCSTR lpSection, LPSTR lpKey, LPSTR lpBuf );
int WINAPI INI_Amuser_WritePrivateProfileString( LPCSTR lpSection, LPSTR lpKey, LPSTR lpBuf, LPCSTR lpszIniFile );
int WINAPI INI_Amuser_WritePPFloat( LPCSTR lpSection, LPSTR lpKey, float nValue );
int WINAPI INI_Amuser_WritePrivateProfileFloat( LPCSTR lpSection, LPSTR lpKey, float nValue, LPCSTR lpszIniFile );
int WINAPI INI_Amuser_WritePPArray( LPCSTR lpSection, LPSTR lpKey, LPINT lpArray, int cArraySize );

/* Dump the Ameol registry to INI files for the specified user
 */
void WINAPI EXPORT Amuser_SaveRegistry( LPCSTR szUser )
{
   char achKey[200];
   DWORD cbName;
   char achClass[MAX_PATH] = TEXT("");
   char szIniFile[200];
   DWORD cchClassName = MAX_PATH;
   DWORD cSubKeys=0;
   DWORD cbMaxSubKey;
   DWORD cchMaxClass;
   DWORD cValues;
   DWORD cchMaxValue;
   DWORD cbMaxValueData;
   DWORD cbSecurityDescriptor;
   FILETIME ftLastWriteTime;
   DWORD i, n, retCode; 

   // Get the class name and the value count.
   HKEY hkResult;
   char sz[ 100 ];

   wsprintf( szIniFile, "%s\\%s\\%s.INI", szdirUser, szUser, szUser );

   wsprintf( sz, "SOFTWARE\\CIX\\Ameol2\\%s", szUser );
   if( RegOpenKeyEx(HKEY_CURRENT_USER, sz, 0L, KEY_READ, &hkResult ) == ERROR_SUCCESS )
   {
      retCode = RegQueryInfoKey(
         hkResult,                // key handle 
         achClass,                // buffer for class name 
         &cchClassName,           // size of class string 
         NULL,                    // reserved 
         &cSubKeys,               // number of subkeys 
         &cbMaxSubKey,            // longest subkey size 
         &cchMaxClass,            // longest class string 
         &cValues,                // number of values for this key 
         &cchMaxValue,            // longest value name 
         &cbMaxValueData,         // longest value data 
         &cbSecurityDescriptor,   // security descriptor 
         &ftLastWriteTime);       // last write time 

      // Enumerate the subkeys.
      for (i=0; i<cSubKeys; i++) 
      {
         HKEY hkSubKey;

         cbName = 200;
         retCode = RegEnumKeyEx(hkResult, i,
                     achKey, 
                     &cbName, 
                     NULL, 
                     NULL, 
                     NULL, 
                     &ftLastWriteTime);

         // Enumerate values under the subkey
         wsprintf( sz, "SOFTWARE\\CIX\\Ameol2\\%s\\%s", szUser, achKey );
         if( RegOpenKeyEx(HKEY_CURRENT_USER, sz, 0L, KEY_READ, &hkSubKey ) == ERROR_SUCCESS )
         {
            retCode = RegQueryInfoKey(
               hkSubKey,                // key handle 
               achClass,                // buffer for class name 
               &cchClassName,           // size of class string 
               NULL,                    // reserved 
               NULL,                    // number of subkeys 
               NULL,                    // longest subkey size 
               &cchMaxClass,            // longest class string 
               &cValues,                // number of values for this key 
               &cchMaxValue,            // longest value name 
               &cbMaxValueData,         // longest value data 
               &cbSecurityDescriptor,   // security descriptor 
               &ftLastWriteTime);       // last write time

            for (n=0, retCode=ERROR_SUCCESS; n<cValues; n++) 
            {
               char achKeyName[200];
               union {
                  char achValue[200];
                  DWORD dwValue;
               } unValue;
               DWORD cchValue = 200; 
               DWORD cchKeyName = 200;
               DWORD dwType;

               cchValue = MAX_VALUE_NAME; 
               unValue.achValue[0] = '\0'; 
               retCode = RegEnumValue(hkSubKey, n, 
                   achKeyName, 
                   &cchKeyName, 
                   NULL, 
                   &dwType,
                   (LPBYTE)&unValue,
                   &cchValue);
               if (retCode == ERROR_SUCCESS ) 
               {
                  switch (dwType)
                  {
                     case REG_SZ:
                        INI_Amuser_WritePrivateProfileString( achKey, achKeyName, unValue.achValue, szIniFile );
                        break;

                     case REG_DWORD:
                        INI_Amuser_WritePrivateProfileLong( achKey, achKeyName, unValue.dwValue, szIniFile );
                        break;
                  }
               }
            }
         }
      }
   }
}

/* Opens the Ameol registry and reads the specified string.
*/
int WINAPI EXPORT ReadRegistryKey( HKEY hk, LPCSTR pKeyPath, LPSTR lpKey, LPCSTR lpDefBuf, LPSTR lpBuf, int cbBuf )
{
   HKEY hkResult;
   
   if( RegOpenKeyEx( hk, pKeyPath, 0L, KEY_READ, &hkResult ) == ERROR_SUCCESS )
   {
      if( !lpKey )
      {
         char szValueName[ MAX_VALUE_NAME ];
         DWORD index = 0;
         DWORD cbValueName;
         int cbInBuf;
         
         cbValueName = sizeof( szValueName );
         cbInBuf = 0;
         index = 0;
         while( RegEnumValue( hkResult, index++, szValueName, &cbValueName, NULL, NULL, 0, NULL ) == ERROR_SUCCESS )
         {
            if( cbInBuf + lstrlen( szValueName ) + 1 >= cbBuf )
               break;
            lstrcpy( lpBuf + cbInBuf, szValueName );
            cbInBuf += lstrlen( szValueName ) + 1;
            cbValueName = sizeof( szValueName );
         }
         *(lpBuf + cbInBuf) = '\0';
         RegCloseKey( hkResult );
      }
      else
      {
         DWORD dwType;
         DWORD dwszInt;
         
         dwszInt = (DWORD)cbBuf;
         if( RegQueryValueEx( hkResult, lpKey, 0L, &dwType, lpBuf, &dwszInt ) != ERROR_SUCCESS )
            lstrcpy( lpBuf, lpDefBuf );
         RegCloseKey( hkResult );
      }
   }
   else
      lstrcpy( lpBuf, lpDefBuf );
   return( lstrlen( lpBuf ) );
}

void FASTCALL IniFileToLocalKey( LPCSTR, char * );
LPCSTR FASTCALL GetIniFileBasename( LPCSTR );
int FASTCALL ReadRegistryInt( HKEY, LPSTR, LPCSTR, LPSTR, int );
long FASTCALL ReadRegistryLong( HKEY, LPSTR, LPCSTR, LPSTR, long );
int FASTCALL WriteRegistryInt( HKEY, LPSTR, LPCSTR, LPSTR, int );
int FASTCALL WriteRegistryLong( HKEY, LPSTR, LPCSTR, LPSTR, long );
int FASTCALL WriteRegistryString( HKEY, LPSTR, LPCSTR, LPSTR, LPSTR );
int FASTCALL WriteRegistryKey( HKEY, LPCSTR, LPSTR, LPSTR );

/* This function reads an array of integers from a private initialisation file.
*  It searches the file for the key named by the lpKey parameter
*  under the application heading specified by lpSection parameter.
*/
int WINAPI EXPORT Amuser_GetPPArray( LPCSTR lpSection, LPSTR lpKey, LPINT lpint, int size )
{
   if (fUseINIfile)
   {
      return ( INI_Amuser_GetPPArray( lpSection, lpKey, lpint, size ) );
   }
   else
   {
      char buf[ 80 ];
      
      if( Amuser_GetPPString( lpSection, lpKey, "", buf, sizeof( buf ) ) )
      {
         register int c, n;
         
         for( n = c = 0; buf[ n ] && c < size; ++c )
         {
            lpint[ c ] = atoi( buf + n );
            while( buf[ n ] && buf[ n ] != ' ' )
               ++n;
            if( buf[ n ] == ' ' )
               ++n;
         }
         return( c );
      }
      return( 0 );
   }
}

/* This function writes an array of integers to a private initialisation file.
*  It searches the file for the key named by the lpKey parameter
*  under the application heading specified by lpSection parameter.
*/
int WINAPI EXPORT Amuser_WritePPArray( LPCSTR lpSection, LPSTR lpKey, LPINT lpint, int size )
{
   if (fUseINIfile)
   {
      return(INI_Amuser_WritePPArray( lpSection, lpKey, lpint, size ));
   }
   else
   {
      char buf[ 80 ];
      register int c, n;

      INI_Amuser_WritePPArray( lpSection, lpKey, lpint, size );
      for( n = c = 0; c < size && n < sizeof( buf ) - 4; ++c )
      {
         if( c )
            buf[ n++ ] = ' ';
         _itoa( lpint[ c ], buf + n, 10 );
         n += strlen( buf + n );
      }
      return( Amuser_WritePPString( lpSection, lpKey, buf ) );
   }
}

/* This function reads the long value from a private initalization file.
*  It searches the file for the key named by the lpKey parameter
*  under the application heading specified by lpSection parameter.
*/
float WINAPI EXPORT Amuser_GetPPFloat( LPCSTR lpSection, LPSTR lpKey, float nDefault )
{
   if (fUseINIfile)
   {
      return (INI_Amuser_GetPPFloat( lpSection, lpKey, nDefault ));
   }
   else
   {
      static char buf[ 20 ];
      float nValue;
      
      if( Amuser_GetPPString( lpSection, lpKey, "", buf, sizeof( buf ) ) )
         nValue = (float)atof( buf );
      else
         nValue = nDefault;
      return( nValue );
   }
}

/* This function returns an integer key setting from the specified
* private profile file.
*/
float EXPORT WINAPI Amuser_GetPrivateProfileFloat( LPCSTR lpSection, LPSTR lpKey, float nDefault, LPCSTR lpszIniFile )
{
   if (fUseINIfile)
   {
      return(INI_Amuser_GetPrivateProfileFloat( lpSection, lpKey, nDefault, lpszIniFile ));
   }
   else
   {
      static char buf[ 20 ];
      float nValue;
      
      if( Amuser_GetPrivateProfileString( lpSection, lpKey, "", buf, sizeof( buf ), lpszIniFile ) )
         nValue = (float)atof( buf );
      else
         nValue = nDefault;
      return( nValue );
   }
}

/* This function writes the floating point value to a private initalization file.
*  It searches the file for the key named by the lpKey parameter
*  under the application heading specified by lpSection parameter.
*/
BOOL WINAPI EXPORT Amuser_WritePPFloat( LPCSTR lpSection, LPSTR lpKey, float nValue )
{
   if (fUseINIfile)
   {
      return(INI_Amuser_WritePPFloat( lpSection, lpKey, nValue ));
   }
   else
   {
      static char buf[ 20 ];

      INI_Amuser_WritePPFloat( lpSection, lpKey, nValue );
      sprintf( buf, "%.3f", nValue );
      return( Amuser_WritePPString( lpSection, lpKey, buf ) );
   }
}

/* This function writes the floating point value to a private initalization file.
*  It searches the file for the key named by the lpKey parameter
*  under the application heading specified by lpSection parameter.
*/
BOOL WINAPI EXPORT Amuser_WritePrivateProfileFloat( LPCSTR lpSection, LPSTR lpKey, float nValue, LPCSTR lpszIniFile )
{
   if (fUseINIfile)
   {
      return(INI_Amuser_WritePrivateProfileFloat( lpSection, lpKey, nValue, lpszIniFile ));
   }
   else
   {
      static char buf[ 20 ];
      
      INI_Amuser_WritePrivateProfileFloat( lpSection, lpKey, nValue, lpszIniFile );
      sprintf( buf, "%.3f", nValue );
      return( Amuser_WritePrivateProfileString( lpSection, lpKey, buf, lpszIniFile ) );
   }
}

/* Opens the Ameol registry and reads the specified integer value.
*/
int WINAPI EXPORT Amuser_GetPPInt( LPCSTR lpSection, LPSTR lpKey, int nDefInt )
{
   if (fUseINIfile)
   {
      return(INI_Amuser_GetPPInt( lpSection, lpKey, nDefInt ));
   }
   else
   {
      return( ReadRegistryInt( HKEY_CURRENT_USER, szActiveUser, lpSection, lpKey, nDefInt ) );
   }
}

/* Opens the Ameol registry and reads the specified integer value.
*/
int WINAPI EXPORT Amuser_GetPrivateProfileInt( LPCSTR lpSection, LPSTR lpKey, int nDefInt, LPCSTR lpszIniFile )
{
   if (fUseINIfile)
   {
      return(INI_Amuser_GetPrivateProfileInt( lpSection, lpKey, nDefInt, lpszIniFile ));
   }
   else
   {
      char szKey[ 9 ];
      
      IniFileToLocalKey( lpszIniFile, szKey );
      return( ReadRegistryInt( HKEY_CURRENT_USER, szKey, lpSection, lpKey, nDefInt ) );
   }
}

/* Opens the Ameol registry and reads the specified integer value.
*/
int FASTCALL ReadRegistryInt( HKEY hk, LPSTR lpUser, LPCSTR lpSection, LPSTR lpKey, int nDefInt )
{
   char sz[ 100 ];
   HKEY hkResult;
   
   wsprintf( sz, "SOFTWARE\\CIX\\Ameol2\\%s\\%s", lpUser, lpSection );
   if( RegOpenKeyEx( hk, sz, 0L, KEY_READ, &hkResult ) == ERROR_SUCCESS )
   {
      DWORD dwType;
      DWORD dwszInt;
      
      dwszInt = sizeof( int );
      RegQueryValueEx( hkResult, lpKey, NULL, &dwType, (LPBYTE)&nDefInt, &dwszInt );
      RegCloseKey( hkResult );
   }
   return( nDefInt );
}

/* Opens the Ameol registry and reads the specified integer value.
*/
long WINAPI EXPORT Amuser_GetPPLong( LPCSTR lpSection, LPSTR lpKey, long lDefLong )
{
   if (fUseINIfile)
   {
      return(INI_Amuser_GetPPLong( lpSection, lpKey, lDefLong ));
   }
   else
   {
      return( ReadRegistryLong( HKEY_CURRENT_USER, szActiveUser, lpSection, lpKey, lDefLong ) );
   }
}

/* Opens the Ameol registry and reads the specified integer value.
*/
long WINAPI EXPORT Amuser_GetPrivateProfileLong( LPCSTR lpSection, LPSTR lpKey, long nDefLong, LPCSTR lpszIniFile )
{
   if (fUseINIfile)
   {
      return(INI_Amuser_GetPrivateProfileLong( lpSection, lpKey, nDefLong, lpszIniFile ));
   }
   else
   {
      char szKey[ 9 ];
      
      IniFileToLocalKey( lpszIniFile, szKey );
      return( ReadRegistryLong( HKEY_CURRENT_USER, szKey, lpSection, lpKey, nDefLong ) );
   }
}

/* Opens the Ameol registry and reads the specified integer value.
*/
long FASTCALL ReadRegistryLong( HKEY hk, LPSTR lpUser, LPCSTR lpSection, LPSTR lpKey, long lDefLong )
{
   char sz[ 100 ];
   HKEY hkResult;
   
   wsprintf( sz, "SOFTWARE\\CIX\\Ameol2\\%s\\%s", lpUser, lpSection );
   if( RegOpenKeyEx( hk, sz, 0L, KEY_READ, &hkResult ) == ERROR_SUCCESS )
   {
      DWORD dwType;
      DWORD dwszLong;
      
      dwszLong = sizeof( long );
      RegQueryValueEx( hkResult, lpKey, NULL, &dwType, (LPBYTE)&lDefLong, &dwszLong );
      RegCloseKey( hkResult );
   }
   return( lDefLong );
}

/* Opens the Ameol registry and reads the specified string from the
* current user section.
*/
int WINAPI EXPORT Amuser_GetPPString( LPCSTR lpSection, LPSTR lpKey, LPCSTR lpDefBuf, LPSTR lpBuf, int cbBuf )
{
   if (fUseINIfile)
   {
      return(INI_Amuser_GetPPString( lpSection, lpKey, lpDefBuf, lpBuf, cbBuf ));
   }
   else
   {
      char sz[ 100 ];
      
      wsprintf( sz, "SOFTWARE\\CIX\\Ameol2\\%s\\%s", szActiveUser, lpSection );
      return( ReadRegistryKey( HKEY_CURRENT_USER, sz, lpKey, lpDefBuf, lpBuf, cbBuf ) );
   }
}

/* Opens the Ameol registry and reads the specified string from the
* current user section.
*/
int WINAPI EXPORT Amuser_GetPrivateProfileString( LPCSTR lpSection, LPSTR lpKey, LPCSTR lpDefBuf, LPSTR lpBuf, int cbBuf, LPCSTR lpszIniFile )
{
   if (fUseINIfile)
   {
      return(INI_Amuser_GetPrivateProfileString( lpSection, lpKey, lpDefBuf, lpBuf, cbBuf, lpszIniFile ));
   }
   else
   {
      char szKey[ 9 ];
      char sz[ 100 ];
      
      IniFileToLocalKey( lpszIniFile, szKey );
      wsprintf( sz, "SOFTWARE\\CIX\\Ameol2\\%s\\%s", szKey, lpSection );
      return( ReadRegistryKey( HKEY_CURRENT_USER, sz, lpKey, lpDefBuf, lpBuf, cbBuf ) );
   }
}

/* Opens the Ameol registry and reads the specified string from the
* local machine section.
*/
int WINAPI EXPORT Amuser_GetLMString( LPCSTR lpSection, LPSTR lpKey, LPCSTR lpDefBuf, LPSTR lpBuf, int cbBuf )
{
   if (fUseINIfile)
   {
      return(INI_Amuser_GetLMString( lpSection, lpKey, lpDefBuf, lpBuf, cbBuf ));
   }
   else
   {
      char sz[ 100 ];
      
      wsprintf( sz, "SOFTWARE\\CIX\\Ameol2\\%s", lpSection );
      return( ReadRegistryKey( HKEY_LOCAL_MACHINE, sz, lpKey, lpDefBuf, lpBuf, cbBuf ) );
   }
}

/* Opens the Ameol registry and reads the specified integer from the
* local machine section.
*/
int WINAPI EXPORT Amuser_GetLMInt( LPCSTR lpSection, LPSTR lpKey, int nDefInt )
{
   if (fUseINIfile)
   {
      return(INI_Amuser_GetLMInt( lpSection, lpKey, nDefInt ));
   }
   else
   {
      char sz[ 100 ];
      HKEY hkResult;
      
      wsprintf( sz, "SOFTWARE\\CIX\\Ameol2\\%s", lpSection );
      if( RegOpenKeyEx( HKEY_LOCAL_MACHINE, sz, 0L, KEY_READ, &hkResult ) == ERROR_SUCCESS )
      {
         DWORD dwType;
         DWORD dwszInt;
         
         dwszInt = sizeof( int );
         RegQueryValueEx( hkResult, lpKey, NULL, &dwType, (LPBYTE)&nDefInt, &dwszInt );
         RegCloseKey( hkResult );
      }
      return( nDefInt );
   }
}

/* Opens the Ameol registry and writes the specified string.
*/
int WINAPI EXPORT Amuser_WritePPString( LPCSTR lpSection, LPSTR lpKey, LPSTR lpBuf )
{
   if (fUseINIfile)
   {
      return(INI_Amuser_WritePPString( lpSection, lpKey, lpBuf ));
   }
   else
   {
      INI_Amuser_WritePPString( lpSection, lpKey, lpBuf );
      return( WriteRegistryString( HKEY_CURRENT_USER, szActiveUser, lpSection, lpKey, lpBuf ) );
   }
}

/* Opens the Ameol registry and writes the specified string to
* the local machine section.
*/
int WINAPI EXPORT Amuser_WriteLMString( LPCSTR lpSection, LPSTR lpKey, LPSTR lpBuf)
{
   if (fUseINIfile)
   {
      return(INI_Amuser_WriteLMString( lpSection, lpKey, lpBuf));
   }
   else
   {
      char sz[ 100 ];

      INI_Amuser_WriteLMString( lpSection, lpKey, lpBuf);
      wsprintf( sz, "SOFTWARE\\CIX\\Ameol2\\%s", lpSection );
      return( WriteRegistryKey( HKEY_LOCAL_MACHINE, sz, lpKey, lpBuf ) );
   }
}

/* Opens the Ameol registry and writes the specified string.
*/
int WINAPI EXPORT Amuser_WritePrivateProfileString( LPCSTR lpSection, LPSTR lpKey, LPSTR lpBuf, LPCSTR lpszIniFile )
{
   if (fUseINIfile)
   {
      return(INI_Amuser_WritePrivateProfileString( lpSection, lpKey, lpBuf, lpszIniFile ));
   }
   else
   {
      char szKey[ 9 ];

      INI_Amuser_WritePrivateProfileString( lpSection, lpKey, lpBuf, lpszIniFile );
      IniFileToLocalKey( lpszIniFile, szKey );
      return( WriteRegistryString( HKEY_CURRENT_USER, szKey, lpSection, lpKey, lpBuf ) );
   }
}

/* This function writes to the specified registry key.
*/
int FASTCALL WriteRegistryString( HKEY hk, LPSTR lpUser, LPCSTR lpSection, LPSTR lpKey, LPSTR lpBuf )
{
   char sz[ 100 ];
   
   wsprintf( sz, "SOFTWARE\\CIX\\Ameol2\\%s\\%s", lpUser, lpSection );
   return( WriteRegistryKey( hk, sz, lpKey, lpBuf ) );
}

/* Delete the specified user from the registry.
*/
int WINAPI Amuser_DeleteUser( LPSTR lpUser )
{
   if (fUseINIfile)
   {
      return(INI_Amuser_DeleteUser( lpUser ));
   }
   else
   {
      char sz[ 100 ];

      INI_Amuser_DeleteUser( lpUser );
      wsprintf( sz, "SOFTWARE\\CIX\\Ameol2\\%s", lpUser );
      return( WriteRegistryKey( HKEY_CURRENT_USER, sz, NULL, NULL ) );
   }
}

/* This function deletes all keys and values within the specified
* registry key.
*/
void FASTCALL DeleteRegistryKey( HKEY hk, LPCSTR pKeyPath )
{
   char szSubKeyName[ MAX_SUBKEY_NAME ];
   char szValueName[ MAX_VALUE_NAME ];
   DWORD cbSubKeyName;
   DWORD cbValueName;
   HKEY hkResult;
   FILETIME ft;
   
   /* Open this key.
   */
   if( RegOpenKeyEx( hk, pKeyPath, 0L, KEY_ALL_ACCESS, &hkResult ) != ERROR_SUCCESS )
      return;
   
      /* First look for and delete all keys. Caution: if there's a key whose length
      * is greater than cbSubKeyName, the RegEnumKeyEx function will FAIL!
   */
   cbSubKeyName = sizeof( szSubKeyName );
   while( RegEnumKeyEx( hkResult, 0, szSubKeyName, &cbSubKeyName, NULL, NULL, NULL, &ft ) == ERROR_SUCCESS )
   {
      DeleteRegistryKey( hkResult, szSubKeyName );
      cbSubKeyName = sizeof( szSubKeyName );
   }
   
   /* Then look for and delete all values. Actually, you can't mix
   * keys and values in the same subkey, but this code won't care.
   */
   cbValueName = sizeof( szValueName );
   while( RegEnumValue( hkResult, 0, szValueName, &cbValueName, NULL, NULL, 0, NULL ) == ERROR_SUCCESS )
   {
      RegDeleteValue( hkResult, szValueName );
      cbValueName = sizeof( szValueName );
   }
   
   /* Finally delete the key.
   */
   RegCloseKey( hkResult );
   RegDeleteKey( hk, pKeyPath );
}

/* This function writes to the specified registry key.
*/
int FASTCALL WriteRegistryKey( HKEY hk, LPCSTR pKeyPath, LPSTR lpKey, LPSTR lpBuf )
{
   DWORD dwDisposition;
   HKEY hkResult;
   
   if( !lpKey )
      DeleteRegistryKey( hk, pKeyPath );
   else if( !lpBuf )
   {
      if( RegOpenKeyEx( hk, pKeyPath, 0L, KEY_SET_VALUE, &hkResult ) == ERROR_SUCCESS )
      {
         RegDeleteValue( hkResult, lpKey );
         RegCloseKey( hkResult );
      }
   }
   else {
      if( RegCreateKeyEx( hk, pKeyPath, 0L, "", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkResult, &dwDisposition ) == ERROR_SUCCESS )
      {
         RegSetValueEx( hkResult, lpKey, 0L, REG_SZ, lpBuf, lstrlen( lpBuf ) + 1 );
         RegCloseKey( hkResult );
      }
   }
   return( TRUE );
}

/* Opens the Ameol registry and writes the specified integer.
*/
int WINAPI EXPORT Amuser_WritePPInt( LPCSTR lpSection, LPSTR lpKey, int nValue )
{
   if (fUseINIfile)
   {
      return(INI_Amuser_WritePPInt( lpSection, lpKey, nValue ));
   }
   else
   {
      INI_Amuser_WritePPInt( lpSection, lpKey, nValue );
      return( WriteRegistryInt( HKEY_CURRENT_USER, szActiveUser, lpSection, lpKey, nValue ) );
   }
}

/* Opens the Ameol registry and writes the specified integer.
*/
int WINAPI EXPORT Amuser_WritePrivateProfileInt( LPCSTR lpSection, LPSTR lpKey, int nValue, LPCSTR lpszIniFile )
{
   if (fUseINIfile)
   {
      return(INI_Amuser_WritePrivateProfileInt( lpSection, lpKey, nValue, lpszIniFile ));
   }
   else
   {
      char szKey[ 9 ];

      INI_Amuser_WritePrivateProfileInt( lpSection, lpKey, nValue, lpszIniFile );
      IniFileToLocalKey( lpszIniFile, szKey );
      return( WriteRegistryInt( HKEY_CURRENT_USER, szKey, lpSection, lpKey, nValue ) );
   }
}

/* Opens the Ameol registry and writes the specified integer.
*/
int FASTCALL WriteRegistryInt( HKEY hk, LPSTR lpUser, LPCSTR lpSection, LPSTR lpKey, int nValue )
{
   char sz[ 100 ];
   DWORD dwDisposition;
   HKEY hkResult;
   
   wsprintf( sz, "SOFTWARE\\CIX\\Ameol2\\%s\\%s", lpUser, lpSection );
   if( RegCreateKeyEx( HKEY_CURRENT_USER, sz, 0L, "", REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hkResult, &dwDisposition ) == ERROR_SUCCESS )
   {
      DWORD dw = (DWORD)nValue;
      
      RegSetValueEx( hkResult, lpKey, 0L, REG_DWORD, (LPBYTE)&dw, sizeof(DWORD) );
      RegCloseKey( hkResult );
   }
   return( TRUE );
}

/* Opens the Ameol registry and writes the specified long integer.
*/
int WINAPI EXPORT Amuser_WritePPLong( LPCSTR lpSection, LPSTR lpKey, long lValue )
{
   if (fUseINIfile)
   {
      return(INI_Amuser_WritePPLong( lpSection, lpKey, lValue ));
   }
   else
   {
      INI_Amuser_WritePPLong( lpSection, lpKey, lValue );
      return( WriteRegistryLong( HKEY_CURRENT_USER, szActiveUser, lpSection, lpKey, lValue ) );
   }
}

/* Opens the Ameol registry and writes the specified integer.
*/
int WINAPI EXPORT Amuser_WritePrivateProfileLong( LPCSTR lpSection, LPSTR lpKey, long nValue, LPCSTR lpszIniFile )
{
   if (fUseINIfile)
   {
      return(INI_Amuser_WritePrivateProfileLong( lpSection, lpKey, nValue, lpszIniFile ));
   }
   else
   {
      char szKey[ 9 ];

      INI_Amuser_WritePrivateProfileLong( lpSection, lpKey, nValue, lpszIniFile );
      IniFileToLocalKey( lpszIniFile, szKey );
      return( WriteRegistryLong( HKEY_CURRENT_USER, szKey, lpSection, lpKey, nValue ) );
   }
}

/* Opens the Ameol registry and writes the specified integer.
*/
int FASTCALL WriteRegistryLong( HKEY hk, LPSTR lpUser, LPCSTR lpSection, LPSTR lpKey, long nValue )
{
   char sz[ 100 ];
   DWORD dwDisposition;
   HKEY hkResult;
   
   wsprintf( sz, "SOFTWARE\\CIX\\Ameol2\\%s\\%s", lpUser, lpSection );
   if( RegCreateKeyEx( HKEY_CURRENT_USER, sz, 0L, "", REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hkResult, &dwDisposition ) == ERROR_SUCCESS )
   {
      DWORD dw = (DWORD)nValue;
      
      RegSetValueEx( hkResult, lpKey, 0L, REG_DWORD, (LPBYTE)&dw, sizeof(DWORD) );
      RegCloseKey( hkResult );
   }
   return( TRUE );
}

/* Converts a INI filename path to an 8-character local key name.
*/
void FASTCALL IniFileToLocalKey( LPCSTR lpszIniFile, char * pszKey )
{
   register int c;
   LPCSTR p;
   
   for( c = 0, p = GetIniFileBasename( lpszIniFile ); *p && *p != '.' && c < 8; ++c, ++p )
      if( c == 0 )
         *pszKey++ = toupper(*p);
      else
         *pszKey++ = tolower(*p);
   *pszKey = '\0';
}

/* Returns a pointer to the basename of an INI file path.
*/
LPCSTR FASTCALL GetIniFileBasename( LPCSTR pszFile )
{
   LPCSTR pszFileT;
   
   pszFileT = pszFile;
   while( *pszFileT )
   {
      if( *pszFileT == ':' || *pszFileT == '\\' || *pszFileT == '/' )
         pszFile = pszFileT + 1;
      ++pszFileT;
   }
   return( pszFile );
}

extern char * szdirActiveUserFile;
extern char * szdirAppDir;
extern char * szdirUser;

/* This function returns an integer key setting from the user's
* configuration file.
*/
int WINAPI INI_Amuser_GetPPInt( LPCSTR lpSection, LPSTR lpKey, int nDefault )
{
   return( INI_Amuser_GetPrivateProfileInt( lpSection, lpKey, nDefault, szdirActiveUserFile ) );
}

/* This function returns an integer key setting from the specified
* private profile file.
*/
int WINAPI INI_Amuser_GetPrivateProfileInt( LPCSTR lpSection, LPSTR lpKey, int nDefault, LPCSTR lpszIniFile )
{
   return( GetPrivateProfileInt( lpSection, lpKey, nDefault, lpszIniFile ) );
}

/* This function returns an floating point key setting from the user's
* configuration file.
*/
float WINAPI INI_Amuser_GetPPFloat( LPCSTR lpSection, LPSTR lpKey, float nDefault )
{
   return( INI_Amuser_GetPrivateProfileFloat( lpSection, lpKey, nDefault, szdirActiveUserFile ) );
}

/* This function returns an integer key setting from the specified
* private profile file.
*/
float WINAPI INI_Amuser_GetPrivateProfileFloat( LPCSTR lpSection, LPSTR lpKey, float nDefault, LPCSTR lpszIniFile )
{
   static char buf[ 20 ];
   float nValue;
   
   if( GetPrivateProfileString( lpSection, lpKey, "", buf, sizeof( buf ), lpszIniFile ) )
      nValue = (float)atof( buf );
   else
      nValue = nDefault;
   return( nValue );
}

/* This function returns a long integer key setting from the user's
* configuration file.
*/
LONG WINAPI INI_Amuser_GetPPLong( LPCSTR lpSection, LPSTR lpKey, long lDefault )
{
   return( INI_Amuser_GetPrivateProfileLong( lpSection, lpKey, lDefault, szdirActiveUserFile ) );
}

/* This function returns an integer key setting from the specified
* private profile file.
*/
LONG WINAPI INI_Amuser_GetPrivateProfileLong( LPCSTR lpSection, LPSTR lpKey, long lDefault, LPCSTR lpszIniFile )
{
   static char buf[ 20 ];
   LONG lValue;
   
   if( GetPrivateProfileString( lpSection, lpKey, "", buf, sizeof( buf ), lpszIniFile ) )
      lValue = atol( buf );
   else
      lValue = lDefault;
   return( lValue );
}

/* This function returns an string key setting from the user's
* configuration file.
*/
int WINAPI INI_Amuser_GetPPString( LPCSTR lpSection, LPSTR lpKey, LPCSTR lpDefBuf, LPSTR lpBuf, int cbBuf )
{
   return( INI_Amuser_GetPrivateProfileString( lpSection, lpKey, lpDefBuf, lpBuf, cbBuf, szdirActiveUserFile ) );
}

/* This function returns an integer key setting from the specified
* private profile file.
*/
int WINAPI INI_Amuser_GetPrivateProfileString( LPCSTR lpSection, LPSTR lpKey, LPCSTR lpDefBuf, LPSTR lpBuf, int cbBuf, LPCSTR lpszIniFile )
{
   return( GetPrivateProfileString( lpSection, lpKey, lpDefBuf, lpBuf, cbBuf, lpszIniFile ) );
}

/* This function returns an string key setting from the local machine settings.
*/
int WINAPI INI_Amuser_GetLMString( LPCSTR lpSection, LPSTR lpKey, LPCSTR lpDefBuf, LPSTR lpBuf, int cbBuf )
{
   char szFile[254];
   wsprintf(szFile, "%sAmeol2.ini", szdirAppDir);
   return( GetPrivateProfileString( lpSection, lpKey, lpDefBuf, lpBuf, cbBuf, szFile ) );
}

/* This function returns an integer setting from the local machine settings.
*/
int WINAPI INI_Amuser_GetLMInt( LPCSTR lpSection, LPSTR lpKey, int nDefault )
{
   char szFile[254];
   wsprintf(szFile, "%sAmeol2.ini", szdirAppDir);
   return( GetPrivateProfileInt( lpSection, lpKey, nDefault, szFile ) );
}

/* This function returns an integer array setting from the user's
* configuration file.
*/
int WINAPI INI_Amuser_GetPPArray( LPCSTR lpSection, LPSTR lpKey, LPINT lpint, int size )
{
   char buf[ 80 ];
   
   if( GetPrivateProfileString( lpSection, lpKey, "", buf, sizeof( buf ), szdirActiveUserFile ) )
   {
      register int c, n;
      
      for( n = c = 0; buf[ n ] && c < size; ++c )
      {
         lpint[ c ] = atoi( buf + n );
         while( buf[ n ] && buf[ n ] != ' ' )
            ++n;
         if( buf[ n ] == ' ' )
            ++n;
      }
      return( c );
   }
   return( 0 );
}

/* Delete the specified user configuration file.
*/
int WINAPI INI_Amuser_DeleteUser( LPSTR lpUser )
{
   char sz[ _MAX_PATH ];
   
   wsprintf( sz, "%s\\%s\\%s.INI", szdirUser, lpUser, lpUser );
   return( Amfile_Delete( sz ) );
}

/* This function writes an integer key setting to the user's
* configuration file.
*/
int WINAPI INI_Amuser_WritePPInt( LPCSTR lpSection, LPSTR lpKey, int nValue )
{
   return( INI_Amuser_WritePrivateProfileInt( lpSection, lpKey, nValue, szdirActiveUserFile ) );
}

/* This function writes an integer key setting to the specified
* configuration file.
*/
BOOL WINAPI INI_Amuser_WritePrivateProfileInt( LPCSTR lpSection, LPSTR lpKey, int nValue, LPCSTR lpszIniFile )
{
   static char sBuf[ 7 ];
   
   _itoa( nValue, sBuf, 10 );
   return( WritePrivateProfileString( lpSection, lpKey, sBuf, lpszIniFile ) );
}

/* This function writes a long integer key setting to the user's
* configuration file.
*/
int WINAPI INI_Amuser_WritePPLong( LPCSTR lpSection, LPSTR lpKey, long nValue )
{
   return( INI_Amuser_WritePrivateProfileLong( lpSection, lpKey, nValue, szdirActiveUserFile ) );
}

/* This function writes an integer key setting to the specified
* configuration file.
*/
int WINAPI INI_Amuser_WritePrivateProfileLong( LPCSTR lpSection, LPSTR lpKey, long nValue, LPCSTR lpszIniFile )
{
   static char sBuf[ 15 ];
   
   _ltoa( nValue, sBuf, 10 );
   return( WritePrivateProfileString( lpSection, lpKey, sBuf, lpszIniFile ) );
}

/* This function writes a string key setting to the local machine settings.
*/
int WINAPI INI_Amuser_WriteLMString( LPCSTR lpSection, LPSTR lpKey, LPSTR lpBuf )
{
   char szFile[254];
   wsprintf(szFile, "%sAmeol2.ini", szdirAppDir);
   return( WritePrivateProfileString( lpSection, lpKey, lpBuf, szFile ) );
}

/* This function writes an string key setting to the user's
* configuration file.
*/
int WINAPI INI_Amuser_WritePPString( LPCSTR lpSection, LPSTR lpKey, LPSTR lpBuf )
{
   return( INI_Amuser_WritePrivateProfileString( lpSection, lpKey, lpBuf, szdirActiveUserFile ) );
}

/* This function writes an integer key setting to the specified
* configuration file.
*/
int WINAPI INI_Amuser_WritePrivateProfileString( LPCSTR lpSection, LPSTR lpKey, LPSTR lpBuf, LPCSTR lpszIniFile )
{
   return( WritePrivateProfileString( lpSection, lpKey, lpBuf, lpszIniFile ) );
}

/* This function writes a floating point key setting to the user's
* configuration file.
*/
int WINAPI INI_Amuser_WritePPFloat( LPCSTR lpSection, LPSTR lpKey, float nValue )
{
   return( INI_Amuser_WritePrivateProfileFloat( lpSection, lpKey, nValue, szdirActiveUserFile ) );
}

/* This function writes an integer key setting to the specified
* configuration file.
*/
int WINAPI INI_Amuser_WritePrivateProfileFloat( LPCSTR lpSection, LPSTR lpKey, float nValue, LPCSTR lpszIniFile )
{
   static char buf[ 20 ];
   
   sprintf( buf, "%.3f", nValue );
   return( WritePrivateProfileString( lpSection, lpKey, buf, lpszIniFile ) );
}

/* This function writes an integer array setting to the user's
* configuration file.
*/
int WINAPI INI_Amuser_WritePPArray( LPCSTR lpSection, LPSTR lpKey, LPINT lpArray, int cArraySize )
{
   char buf[ 80 ];
   register int c, n;

   for( n = c = 0; c < cArraySize && n < sizeof( buf ) - 4; ++c )
   {
      if( c )
         buf[ n++ ] = ' ';
      _itoa( lpArray[ c ], buf + n, 10 );
      n += strlen( buf + n );
   }
   return( WritePrivateProfileString( lpSection, lpKey, buf, szdirActiveUserFile ) );
}

// #endif USEREGISTRY