/* AMUSER.C - The Palantir user engine
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
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "amlib.h"
#include "amuser.h"
#include "common.bld"

#define  THIS_FILE   __FILE__

char * szdirAppDir = NULL;          /* Full path to application directory */
char * szdirMugshots = NULL;        /* Full path to mugshot directory */
char * szdirAddons = NULL;          /* Full path to addons directory */
char * szdirArchive = NULL;            /* Full path to archive directory */
char * szdirScripts = NULL;            /* Full path to scripts directory */
char * szdirResumes = NULL;            /* Full path to resumes directory */
char * szdirScratchpads = NULL;        /* Full path to scratchpad download directory */
char * szdirUploads = NULL;            /* Full path to uploads directory */
char * szdirDownloads = NULL;       /* Full path to downloads directory */
char * szdirAttach = NULL;          /* Full path to attachments directory */
char * szdirData = NULL;            /* Full path to data directory */
char * szdirUser = NULL;            /* Full path to user directory */
char * szdirActiveUser = NULL;         /* Full path to active user directory (computed) */
char * szdirActiveUserFile = NULL;     /* Full path to active user configuration file (computed) */

BOOL fUseINIfile = FALSE; // 2.56.2051 FS#121
BOOL fUseU3 = FALSE; // !!SM!! 2.56.2053

HINSTANCE hLibInst = NULL;          /* Handle of library instance */
HINSTANCE hRegLib;               /* Regular Expression and Mail Library */
INTERFACE_PROPERTIES ip;            /* Current interface properties */
HFONT hDlgFont = NULL;              /* Dialog font */
WORD wWinVer;                    /* Current windows version */
BOOL fAppDirSet = FALSE;            /* TRUE if application directory set */
BOOL fActiveUserSet = FALSE;        /* TRUE if active user directory set */
char szActiveUser[ 40 ];            /* Name of active user */
LOGFONT lfSysFont;                  /* Dialog font attributes */

#define  ROP_DSPDxax          0x00E20746
#define  RGB_TRANSPARENT         RGB( 255, 0, 255 )

COLORREF crTransparent = RGB_TRANSPARENT;                /* Transparent colour */
const char NEAR szCompulink[] = "cix.compulink.co.uk";         /* CIX domain */
const char NEAR szCixCoUk[] = "cix.co.uk";                  /* New CIX domain */

void FASTCALL CreateDialogFont( void );
void FASTCALL InternalInitADM( void );
int FASTCALL ReadInteger( char ** );
void FASTCALL ParseWindowState( char *, LPRECT, DWORD * );
void FASTCALL CreateWindowState( char *, HWND );
BOOL FASTCALL ValidFileNameChr( char );
void FASTCALL StoreString( char **, char * );
void FASTCALL SetActiveUserHomeDirectory( void );
BOOL FASTCALL IsCompulinkDomain( char * );

void WINAPI EXPORT SetINIMode(BOOL pUseINIfile);       /* Shoulw we read INI via file or registry // 2.56.2051 FS#121*/

void FASTCALL FreeStoredDirecrories( void ); // 20060325 - 2.56.2049.20

/* This is the database DLL entry point. Two separate functions are
 * provided: one for Windows 32 and one for 16-bit Windows.
 */
#ifdef WIN32
#if defined(__BORLANDC__)
BOOL WINAPI EXPORT DllEntryPoint(HINSTANCE hInstance, DWORD fdwReason, LPVOID lpvReserved)
#else
BOOL WINAPI EXPORT DllMain( HANDLE hInstance, DWORD fdwReason, LPVOID lpvReserved )
#endif
{
   switch( fdwReason )
      {
      case DLL_PROCESS_ATTACH:

         InternalInitADM();
         CreateDialogFont();
         hRegLib = NULL;
         hLibInst = hInstance;
         return( 1 );

      case DLL_PROCESS_DETACH:
         if( hDlgFont )
            DeleteFont( hDlgFont );
         FreeStoredDirecrories(); // 20060325 - 2.56.2049.20
         return( 1 );

      default:
         return( 1 );
      }
}
#else
int WINAPI EXPORT LibMain( HANDLE hInstance, WORD wDataSeg, WORD cbHeapSize, LPSTR lpszCmdLine )
{
   hLibInst = hInstance;
   InternalInitADM();
   CreateDialogFont();
   return( 1 );
}

int WINAPI EXPORT WEP( int nRetType )
{
   if( hDlgFont )
      DeleteFont( hDlgFont );
   return( 1 );
}
#endif

void FASTCALL UnStoreString( char ** ppstrDest ) // 20060325 - 2.56.2049.20
{ 
   if( *ppstrDest )
      FreeMemory( ppstrDest );
    *ppstrDest = NULL;
}

void FASTCALL FreeStoredDirecrories( void ) // 20060325 - 2.56.2049.20
{                      
   UnStoreString( &szdirAppDir );
   UnStoreString( &szdirUser );
   UnStoreString( &szdirMugshots );
   UnStoreString( &szdirAddons );
   UnStoreString( &szdirUploads );
   UnStoreString( &szdirDownloads );
   UnStoreString( &szdirScripts );
   UnStoreString( &szdirResumes );
   UnStoreString( &szdirScratchpads );
   UnStoreString( &szdirArchive );
   UnStoreString( &szdirData );
   UnStoreString( &szdirAttach );
   UnStoreString( &szdirUser );
   UnStoreString( &szdirActiveUser );
   UnStoreString( &szdirActiveUserFile );

}
/* This function returns the version number of Amctrls.
 */
DWORD WINAPI EXPORT GetBuildVersion( void )
{
   return( MAKEVERSION( PRODUCT_MAX_VER, PRODUCT_MIN_VER, PRODUCT_BUILD ) );
}

/* This function creates the dialog font used by calls to the
 * Amuser_SetDialogFont function.
 */
void FASTCALL CreateDialogFont( void )
{
   int cPixelsPerInch;
   HDC hdc;

   /* Get the system variable font dimensions.
    */
   hdc = GetDC( NULL );
   cPixelsPerInch = GetDeviceCaps( hdc, LOGPIXELSY );
   GetObject( GetStockFont( ANSI_VAR_FONT ), sizeof( LOGFONT ), &lfSysFont );
   ReleaseDC( NULL, hdc );

   /* Create the font. If we're running under Windows 95 or the Win95 shell
    * under Windows NT, use a non-bold font. Otherwise use a bold font.
    */
   lfSysFont.lfWeight = FW_BOLD;
#ifdef WIN32
   if( wWinVer >= 0x35F )
      lfSysFont.lfWeight = FW_NORMAL;
#endif
   lfSysFont.lfHeight = -MulDiv( 8, cPixelsPerInch, 72 );
   hDlgFont = CreateFontIndirect( &lfSysFont );
}

/* This function sets all text controls in the specified dialog to use a
 * non-bold font.
 */
void FASTCALL Amuser_SetDialogFont( HWND hwnd )
{
   HWND hwndCtl;

   hwndCtl = GetWindow( hwnd, GW_CHILD );
   while( hwndCtl )
      {
      SetWindowFont( hwndCtl, hDlgFont, FALSE );
      hwndCtl = GetWindow( hwndCtl, GW_HWNDNEXT );
      }
}

/* This function returns the dialog box font.
 */
HFONT WINAPI EXPORT Amuser_GetDialogFont( void )
{
   return( hDlgFont );
}

/* This function sets the current UI properties.
 */
void WINAPI EXPORT Amuser_SetInterfaceProperties( LPINTERFACE_PROPERTIES lpip )
{
   ip = *lpip;
   fActiveUserSet = TRUE;
}

/* This function sets the current UI properties.
 */
void WINAPI EXPORT Amuser_GetInterfaceProperties( LPINTERFACE_PROPERTIES lpip )
{
   *lpip = ip;
}

/* This function returns the handle of the New popup menu
 * in the file menu.
 */
HMENU WINAPI EXPORT Amuser_GetNewPopupMenu( void )
{
   HMENU hMainMenu;
   HMENU hFileMenu;
   HMENU hNewMenu;

   hMainMenu = GetMenu( ip.hwndFrame );
   hFileMenu = GetSubMenu( hMainMenu, 0 );
   hNewMenu = GetSubMenu( hFileMenu, 0 );
   return( hNewMenu );
}

/* This function loads the specified services DLL.
 */
HINSTANCE WINAPI EXPORT Amuser_LoadService( char * pszService )
{
   char szPath[ _MAX_PATH ];

   wsprintf( szPath, "%s\\%sSRVC.DLL", szdirAppDir, pszService );
   return( LoadLibrary( szPath ) );
}

/* This function returns the active user name.
 */
int WINAPI EXPORT Amuser_GetActiveUser( LPSTR lpBuf, int cbMax )
{
   if( lstrcpyn( lpBuf, szActiveUser, cbMax ) )
      return( lstrlen( szActiveUser ) );
   return( 0 );
}

/* This function sets the active user name.
 */
void WINAPI EXPORT Amuser_SetActiveUser( LPSTR lpBuf )
{
   ASSERT( TRUE == fAppDirSet );

   /* Store the active user name.
    */
   lstrcpyn( szActiveUser, lpBuf, sizeof(szActiveUser)-1 );

   /* Set the user's home directory and config file.
    */
   SetActiveUserHomeDirectory();
}

/* This function set the user's home directory and config file.
 */
void FASTCALL SetActiveUserHomeDirectory( void )
{
   char szPath[ _MAX_PATH ];

   if (fUseU3)
   {
      DWORD lRet;
      char szUserPath[ _MAX_PATH ];
      
      fUseINIfile = TRUE;
      lRet = GetEnvironmentVariable("U3_DEVICE_DOCUMENT_PATH", (LPSTR)&szPath, _MAX_PATH - 1);
      if (lRet > 0 && szPath[0] !='\x0')
      {
         wsprintf( szUserPath, "%s\\Ameol2\\Users", szPath);
      }
      else
      {
         wsprintf( szUserPath, "%s", szdirUser);
      }
      /* Store the path to the active user's home directory.
      */
      wsprintf( szPath, "%s\\%s", szUserPath, szActiveUser );
      StoreString( &szdirActiveUser, szPath );
      Amuser_WritePPString( "Settings", "UserDir", szdirActiveUser);
      
      /* Store the full pathname of the active user's INI file.
      */
      wsprintf( szPath, "%s\\%s\\%s.INI", szUserPath, szActiveUser, szActiveUser );
      StoreString( &szdirActiveUserFile, szPath );
   }
   else
   {
      /* Store the path to the active user's home directory.
      */
      wsprintf( szPath, "%s\\%s", szdirUser, szActiveUser );
      StoreString( &szdirActiveUser, szPath );
      Amuser_WritePPString( "Settings", "UserDir", szdirActiveUser);
      
      /* Store the full pathname of the active user's INI file.
      */
      wsprintf( szPath, "%s\\%s\\%s.INI", szdirUser, szActiveUser, szActiveUser );
      StoreString( &szdirActiveUserFile, szPath );
   }

   fActiveUserSet = TRUE;
}

/* This function returns a directory from the directory
 * properties.
 */
int WINAPI EXPORT Amuser_GetAppDirectory( int fnDirectory, LPSTR lpBuf, int cbMax )
{
   char szPath[_MAX_PATH];

   switch( fnDirectory )
      {
      case APDIR_APPDIR:      lstrcpyn( lpBuf, szdirAppDir, cbMax - 1 ); break;
      case APDIR_MUGSHOTS: lstrcpyn( lpBuf, szdirMugshots, cbMax - 1 ); break;
      case APDIR_RESUMES:     lstrcpyn( lpBuf, szdirResumes, cbMax - 1 ); break;
      case APDIR_ADDONS:      lstrcpyn( lpBuf, szdirAddons, cbMax - 1 ); break;
      case APDIR_UPLOADS:     lstrcpyn( lpBuf, szdirUploads, cbMax - 1 ); break;
      case APDIR_DOWNLOADS:   lstrcpyn( lpBuf, szdirDownloads, cbMax - 1 ); break;
      case APDIR_ARCHIVES: lstrcpyn( lpBuf, szdirArchive, cbMax - 1 ); break;
      case APDIR_SCRIPTS:     lstrcpyn( lpBuf, szdirScripts, cbMax - 1 ); break;
      case APDIR_SCRATCHPADS: lstrcpyn( lpBuf, szdirScratchpads, cbMax - 1 ); break;
      case APDIR_DATA:     lstrcpyn( lpBuf, szdirData, cbMax - 1 ); break;
      case APDIR_USER:     lstrcpyn( lpBuf, szdirUser, cbMax - 1 ); break;
      case APDIR_ACTIVEUSER:  lstrcpyn( lpBuf, szdirActiveUser, cbMax - 1 ); /*
         Amuser_GetPPString( "Settings", "UserDir", "", lpBuf, _MAX_PATH );*/
         break;
      default:                lpBuf[ 0 ] = '\0'; break;
      }
   
   //if (fUseINIfile)
   // lpBuf[0] = szdirAppDir[0];

   if (fUseU3)
   {
      DWORD lRet;
      
      fUseINIfile = TRUE;
      lRet = GetEnvironmentVariable("U3_DEVICE_PATH", (LPSTR)&szPath, _MAX_PATH - 1);
      if (lRet > 0 && szPath[0] !='\x0')
      {
         lpBuf[0] = szPath[0];
      }
      else
      {
         lpBuf[0] = szdirAppDir[0];
      }
   }
   return( lstrlen( lpBuf ) );
}

/* This function sets a directory.
 */
BOOL WINAPI EXPORT Amuser_SetAppDirectory( int fnDirectory, LPSTR lpBuf )
{
   switch( fnDirectory )
      {
      case APDIR_APPDIR: {
         char sz[ _MAX_PATH ];

         ASSERT( FALSE == fAppDirSet );
         StoreString( &szdirAppDir, lpBuf );
         wsprintf( sz, "%sUSERS", szdirAppDir );
         StoreString( &szdirUser, sz );
         fAppDirSet = TRUE;
         break;
         }

      case APDIR_MUGSHOTS:
         ASSERT( TRUE == fAppDirSet );
         StoreString( &szdirMugshots, lpBuf );
         break;

      case APDIR_ADDONS:
         ASSERT( TRUE == fAppDirSet );
         StoreString( &szdirAddons, lpBuf );
         break;

      case APDIR_UPLOADS:
         ASSERT( TRUE == fAppDirSet );
         StoreString( &szdirUploads, lpBuf );
         break;

      case APDIR_DOWNLOADS:
         ASSERT( TRUE == fAppDirSet );
         StoreString( &szdirDownloads, lpBuf );
         break;

      case APDIR_SCRIPTS:
         ASSERT( TRUE == fAppDirSet );
         StoreString( &szdirScripts, lpBuf );
         break;

      case APDIR_RESUMES:
         ASSERT( TRUE == fAppDirSet );
         StoreString( &szdirResumes, lpBuf );
         break;

      case APDIR_SCRATCHPADS:
         ASSERT( TRUE == fAppDirSet );
         StoreString( &szdirScratchpads, lpBuf );
         break;

      case APDIR_ARCHIVES:
         ASSERT( TRUE == fAppDirSet );
         StoreString( &szdirArchive, lpBuf );
         break;

      case APDIR_DATA:
         ASSERT( TRUE == fAppDirSet );
         StoreString( &szdirData, lpBuf );
         break;
      
      case APDIR_ATTACHMENTS:
         ASSERT( TRUE == fAppDirSet );
         StoreString( &szdirAttach, lpBuf );
         break;
      
      case APDIR_USER:
         ASSERT( TRUE == fAppDirSet );
         StoreString( &szdirUser, lpBuf );
         if( fActiveUserSet )
            SetActiveUserHomeDirectory();
         break;

      default:
         ASSERT( FALSE );
      }
   return( TRUE );
}

/* This function allocates memory for a string and stores that
 * string in the specified location.
 */
void FASTCALL StoreString( char ** ppstrDest, char * pstrSrc )
{
   if( *ppstrDest )
      FreeMemory( ppstrDest );
   if( fNewMemory( ppstrDest, strlen(pstrSrc)+1 ) )
      strcpy( *ppstrDest, pstrSrc );
}

/* This function retrieves the specified window size, position and state
 * from the configuration file.
 */
void WINAPI EXPORT Amuser_ReadWindowState( char * pszWndName, LPRECT lprc, DWORD * pdw )
{
   char sz[ 40 ];

   if( Amuser_GetPPString( "Windows", pszWndName, "", sz, sizeof( sz ) ) )
      Amuser_ParseWindowState( sz, lprc, pdw );
}

/* This function parses a window state string.
 */
void WINAPI EXPORT Amuser_ParseWindowState( char * pszStateStr, LPRECT lprc, DWORD * pdw )
{
   static DWORD wTodwWndState[] = { 0, WS_MINIMIZE, WS_MAXIMIZE };
   int wState;
   int left;
   int top;
   int width;
   int height;

   wState = ReadInteger( &pszStateStr );
   left = ReadInteger( &pszStateStr );
   top = ReadInteger( &pszStateStr );
   width = ReadInteger( &pszStateStr );
   height = ReadInteger( &pszStateStr );
   if( wState < 0 || wState >= sizeof( wTodwWndState ) )
      wState = 0;
   *pdw = wTodwWndState[ wState ];
   lprc->left = left;
   lprc->top = top;
   lprc->right = left + width;
   lprc->bottom = top + height;
}

/* This function reads an integer from the string.
 */
int FASTCALL ReadInteger( char ** ppszStr )
{
   BOOL isNegative = FALSE;
   char * pszStr;
   int nValue;

   pszStr = *ppszStr;
   nValue = 0;
   if (*pszStr == '-')
      {
      isNegative = TRUE;
      ++pszStr;
      }
   while( isdigit( *pszStr ) )
      nValue = nValue * 10 + ( *pszStr++ - '0' );
   if( *pszStr == ',' )
      ++pszStr;
   *ppszStr = pszStr;
   if (isNegative)
       nValue = -nValue;
   return( nValue );
}

/* This function saves the specified window size, position and state to
 * the configuration file.
 */
void WINAPI EXPORT Amuser_WriteWindowState( char * pszWndName, HWND hwnd )
{
   char sz[ 40 ];

   if (Amuser_CreateWindowState( sz, hwnd ))
       Amuser_WritePPString( "Windows", pszWndName, sz );
}

/* This function compiles a window state string for the
 * specified window.
 */
BOOL WINAPI EXPORT Amuser_CreateWindowState( char * pszStateStr, HWND hwnd )
{
   WINDOWPLACEMENT wndpl;
   int wState;
   int left;
   int top;
   int width;
   int height;

   wndpl.length = sizeof(WINDOWPLACEMENT);
   if (GetWindowPlacement( hwnd, &wndpl ))
       {
       if( wndpl.showCmd == SW_SHOWMINIMIZED )
          wState = 1;
       else if( wndpl.showCmd == SW_SHOWMAXIMIZED )
          wState = 2;
       else
          wState = 0;
       left = wndpl.rcNormalPosition.left;
       top = wndpl.rcNormalPosition.top;
       width = wndpl.rcNormalPosition.right - wndpl.rcNormalPosition.left;
       height = wndpl.rcNormalPosition.bottom - wndpl.rcNormalPosition.top;
       wsprintf( pszStateStr, "%d,%d,%d,%d,%u", wState, left, top, width, height );
       return TRUE;
       }
   return FALSE;
}

/* This function converts an input filename into one valid for the current
 * operating system.
 */
BOOL EXPORT WINAPI Amuser_CreateCompatibleFilename( LPCSTR lpszHostFName, LPSTR lpszOSFName )
{
   register int c;

   /* Skip to the first valid chr in the name.
    */
   for( c = 0; *lpszHostFName && c < 8; ++c )
      if( ValidFileNameChr( *lpszHostFName ) )
         break;
      else
         ++lpszHostFName;

   /* Now gather all valid chrs up to 8 chrs or the first '.'
    */
   for( c = 0; *lpszHostFName && *lpszHostFName != '.' && c < 8; ++c )
      {
      if( ValidFileNameChr( *lpszHostFName ) )
         *lpszOSFName++ = *lpszHostFName;
      ++lpszHostFName;
      }
   *lpszOSFName++ = '.';

   /* If we stopped at a period, skip it. Otherwise find the first period
    * and skip to after it.
    */
   if( *lpszHostFName == '.' )
      ++lpszHostFName;
   else {
      for( c = 0; lpszHostFName[ c ]; ++c )
         if( lpszHostFName[ c ] == '.' )
            {
            lpszHostFName += c + 1;
            break;
            }
         }

   /* Skip to the first valid chr in the extension
    */
   for( c = 0; *lpszHostFName && c < 3; ++c )
      if( ValidFileNameChr( *lpszHostFName ) )
         break;
      else
         ++lpszHostFName;

   /* Now gather all valid chrs up to 3 chrs or the end of the name
    */
   for( c = 0; *lpszHostFName && c < 3; ++c )
      {
      if( ValidFileNameChr( *lpszHostFName ) )
         *lpszOSFName++ = *lpszHostFName;
      ++lpszHostFName;
      }
   *lpszOSFName++ = '\0';
   AnsiLower( lpszOSFName );
   return( lstrlen( lpszOSFName ) > 0 );
}

/* Returns whether or not ch is a valid character in a DOS filename.
 * Knows nothing about Windows/NT filenames, though.
 */
BOOL FASTCALL ValidFileNameChr( char ch )
{
   if( isalnum( ch ) || ch == '_' || ch == '^' || ch == '$' || ch == '~'
      || ch == '!' || ch == '#' || ch == '%' || ch == '&' || ch == '-'
      || ch == '{' || ch == '}' || ch == '(' || ch == ')' || ch == '@'
      || ch == '\'' || ch == '`' )
      return( TRUE );
   return( FALSE );
}

/* This function returns the mugshot bitmap for the specified
 * user.
 */
HBITMAP EXPORT WINAPI Amuser_GetMugshot( char * pszUsername, HPALETTE FAR * phPal )
{
   char szPath[ _MAX_PATH ];
   char * pszExt;

   /* Convert the username to a filename.
    */
   *phPal = NULL;
   wsprintf( szPath, "%s\\", szdirMugshots);
   if( 0 == Amuser_MapUserToMugshotFilename( pszUsername, szPath + strlen(szPath), _MAX_PATH ) )
      return( NULL );

   /* Find the file extension.
    */
   if( NULL != ( pszExt = strrchr( szPath, '.' ) ) )
      ++pszExt;

   /* Try looking for the BMP file first.
    */
   if( pszExt && _strcmpi( pszExt, "bmp" ) == 0 )
      return( Amuser_LoadBitmapFromDisk( szPath, phPal ) );

   /* Not there, so try the GIF file.
    */
   if( pszExt && _strcmpi( pszExt, "gif" ) == 0 )
      return( Amuser_LoadGIFFromDisk( szPath, phPal ) );

   /* No picture, so fail.
    */
   return( NULL );
}

/* Given a username, this function returns the corresponding
 * filename.
 */
int EXPORT WINAPI Amuser_MapUserToMugshotFilename( char * pszUsername, char * pszFilename, int cbFilename )
{
   char szPath[ _MAX_PATH ];
   char szUsername8[ 9 ];
   char * pszBasename;
   char * pszAddress;

   /* Empty the return string.
    */
   *pszFilename = '\0';

   /* Create a pathname that we'll be working with.
    */
   wsprintf( szPath, "%s\\", szdirMugshots );
   pszBasename = szPath + strlen(szPath);

   /* First consult the MUGSHOT.INI file for the salacious
    * details.
    */
   strcpy( pszBasename, "MUGSHOT.INI" );
   if( 0 < GetPrivateProfileString( "Index", pszUsername, "", pszFilename, cbFilename, szPath ) )
      return( strlen( pszFilename ) );

   /* Not found. Try to synthesize the filename from the user name
    * using both BMP and GIF formats.
    */
   wsprintf( pszBasename, "%s.bmp", pszUsername );
   if( !Amfile_QueryFile( szPath ) )
      {
      /* Try GIF
       */
      wsprintf( pszBasename, "%s.gif", pszUsername );
      if( !Amfile_QueryFile( szPath ) )
         {
         /* Not found. If the username is longer than 8
          * characters, it's possible that only the first
          * 8 were specified in the filename. Try it.
          */
         if( strlen( pszUsername ) <= 8 )
            return( 0 );

         pszAddress = strchr( pszUsername, '@' );
         if( NULL != pszAddress )
            if( !( IsCompulinkDomain( pszAddress + 1 ) ) )
               return( 0 );
         memcpy( szUsername8, pszUsername, 8 );
         szUsername8[ 8 ] = '\0';

         /* Try BMP first
          */
         wsprintf( pszBasename, "%s.bmp", szUsername8 );
         if( !Amfile_QueryFile( szPath ) )
            {
            /* Try GIF
             */
            wsprintf( pszBasename, "%s.gif", szUsername8 );
            if( !Amfile_QueryFile( szPath ) )
               /* Okay, give up. It isn't there!
                */
               return( 0 );
            }
         }
      }

   /* We succeeded so add an entry to the MUGSHOT.INI file so
    * that subsequent lookups are faster.
    */
   strcpy( pszFilename, pszBasename );
   strcpy( pszBasename, "MUGSHOT.INI" );
   WritePrivateProfileString( "Index", pszUsername, pszFilename, szPath  );
   return( strlen( pszFilename ) );
}

/* This function deletes all references to this user from
 * the mugshot configuration file.
 */
void WINAPI EXPORT Amuser_DeleteUserMugshot( char * pszUsername )
{
   char szPath[ _MAX_PATH ];

   wsprintf( szPath, "%s\\%s", szdirMugshots, "MUGSHOT.INI" );
   WritePrivateProfileString( "Index", pszUsername, NULL, szPath  );
}

/* This function assigns the specified mugshot to the user.
 */
void WINAPI EXPORT Amuser_AssignUserMugshot( char * pszUsername, char * pszFilename )
{
   char szPath[ _MAX_PATH ];

   wsprintf( szPath, "%s\\%s", szdirMugshots, "MUGSHOT.INI" );
   WritePrivateProfileString( "Index", pszUsername, pszFilename, szPath  );
}

/* This function computes a CRC over a set of bytes.
 */
WORD WINAPI EXPORT Amuser_ComputeCRC( LPBYTE lpbytes, UINT cbBytes )
{
   WORD wCRC;

   wCRC = 0;
   while( cbBytes-- )
      wCRC += *lpbytes++;
   return( wCRC ^ 0xFFFF );
}

/* This function sets the 'transparent' colour for Amuser_DrawBitmap
 * funciton.
 */
COLORREF WINAPI EXPORT Amuser_SetTransparentColour( COLORREF crTrans )
{
   COLORREF crOldTrans;

   crOldTrans = crTransparent;
   crTransparent = crTrans;
   return( crOldTrans );
}

/* This function draws a bitmap from a group.
 */
void WINAPI EXPORT Amuser_DrawBitmap( HDC hdc, int x, int y, int cx, int cy, HBITMAP hbmpGrp, int pos )
{
   HBITMAP hBmpT;
   HBITMAP hBmpMono;
   COLORREF crBack, crText;
   HBRUSH hBr, hBrT;
   HDC hMemDC;
   HDC hDCSrc;
   HDC hDCMid;

   /* Get two intermediate DCs.
    */
   hDCSrc = CreateCompatibleDC( hdc );
   hDCMid = CreateCompatibleDC( hdc );
   hMemDC = CreateCompatibleDC( hdc );

   SelectBitmap( hDCSrc, hbmpGrp );

   /* Create a monochrome bitmap for masking.
    */
   hBmpMono = CreateCompatibleBitmap( hDCMid, cx, cy );
   SelectBitmap( hDCMid, hBmpMono );

   /* Create a middle bitmap.
    */
   hBmpT = CreateCompatibleBitmap( hdc, cx, cy );
   SelectBitmap( hMemDC, hBmpT );

   /* Create a monochrome mask where we have zeroes in the image, ones
    * elsewhere.
    */
   crBack = SetBkColor( hDCSrc, crTransparent );
   BitBlt( hDCMid, 0, 0, cx, cy, hDCSrc, pos * cx, 0, SRCCOPY );
   SetBkColor( hDCSrc, crBack );

   /* Put the unmodified image in the temporary bitmap
    */
   BitBlt( hMemDC, 0, 0, cx, cy, hDCSrc, pos * cx, 0, SRCCOPY );

   /* Create and select a brush of the background colour.
    */
   hBr = CreateSolidBrush( GetBkColor( hdc ) );
   hBrT = SelectBrush( hMemDC, hBr );

   /* Force conversion of the monochrome to stay black and white.
    */
   crText = SetTextColor( hMemDC, 0L );
   crBack = SetBkColor( hMemDC, RGB( 255, 255, 255 ) );

   /* Where the monochrome mask is one, Blt the brush; where the mono mask
    * is zero, leave the destination untouched. This results in painting
    * around the image with the background brush.
    */
   BitBlt( hMemDC, 0, 0, cx, cy, hDCMid, 0, 0, ROP_DSPDxax );
   BitBlt( hdc, x, y, cx, cy, hMemDC, 0, 0, SRCCOPY );

   SetTextColor( hMemDC, crText );
   SetBkColor( hMemDC, crBack );

   SelectBrush( hMemDC, hBrT );
   DeleteBrush( hBr );

   DeleteDC( hDCSrc );
   DeleteDC( hDCMid );
   DeleteDC( hMemDC );
   DeleteBitmap( hBmpT );
   DeleteBitmap( hBmpMono );
}

/* This function tests whether the specified domain name is local
 * to CIX conferencing.
 */
BOOL FASTCALL IsCompulinkDomain( char * pszDomainName )
{
   if( _stricmp( pszDomainName, szCompulink ) == 0 )
      return( TRUE );
   return( _stricmp( pszDomainName, szCixCoUk ) == 0 );
}

void WINAPI EXPORT SetINIMode(BOOL pUseINIfile) // 2.56.2051 FS#121
{
   fUseINIfile = pUseINIfile;
}

void WINAPI EXPORT SetU3Mode(BOOL pUseU3) // 2.56.2053
{
   fUseU3 = pUseU3;
}
