/* AMCTRLS.C - Initialise Ameol common controls
 *
 * This module contains the initialisation and termination code for
 * the Ameol common controls DLL. The initialisation code automatically
 * registers all window classes used in the DLL.
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
#include "amctrls.h"
#include "amctrli.h"
#include "common.bld"
#include "listview.h"

#define  THIS_FILE   __FILE__

BOOL fInited = FALSE;         /* TRUE if common controls initialised */
HINSTANCE hLibInst;           /* Handle of library */
HINSTANCE hRegLib;            /* Regular Expression and Mail Library */
BOOL fNewShell;               /* TRUE if we're hosted under Windows 95 or Windows NT 4.0 */
BOOL fWin95;                  /* TRUE if we're hosted under Windows 95 */
BOOL fWinNT4;                 /* TRUE if we're hosted under Windows NT 4.0 */
BOOL fCtlAnimation;           /* TRUE if we've enabled control animation */
int iScrollLines = 3;         /* Number of lines scrolling */

BOOL FASTCALL Amctl_Term( void );

#define  MAKEOLDVERSION(m,n,b,r)    (DWORD)MAKELONG((((n)<<8)|(r)),(((b)<<8)|(m)))

/* This is the common controls entry point. Two separate functions are
 * provided: one for Windows 32 and one for 16-bit Windows.
 */
#ifdef WIN32
#if defined(__BORLANDC__)
BOOL WINAPI DllEntryPoint(HINSTANCE hInstance, DWORD fdwReason, LPVOID lpvReserved)
#else
BOOL WINAPI DllMain( HANDLE hInstance, DWORD fdwReason, LPVOID lpvReserved )
#endif
{
   static int nAttached = 0;

   switch( fdwReason )
      {
      case DLL_PROCESS_ATTACH:
         hLibInst = hInstance;
         hRegLib = NULL;
         ++nAttached;
         return( Amctl_Init() );

      case DLL_PROCESS_DETACH:
         if( --nAttached > 0 )
            return(1);
         else
            return( Amctl_Term() );

      default:
         return( 1 );
      }
}
#else
int EXPORT FAR PASCAL LibMain( HINSTANCE hInstance, WORD wDataSeg, WORD cbHeapSize, LPSTR lpszCmdLine )
{
   hLibInst = hInstance;
   return( Amctl_Init() );
}

int EXPORT FAR PASCAL WEP( int nRetType )
{
   return( Amctl_Term() );
}
#endif

/* This function returns the version number of Amctrls.
 * OBSOLETE: Provided for backward compatibility. Please call
 * GetBuildVersion instead.
 */
WINCOMMCTRLAPI DWORD WINAPI EXPORT Amctl_GetAmctrlsVersion( void )
{
   return( 0xFFFFFFFF );
}

/* This function returns the version number of Amctrls.
 */
WINCOMMCTRLAPI DWORD WINAPI EXPORT GetBuildVersion( void )
{
   return( MAKEVERSION( PRODUCT_MAX_VER, PRODUCT_MIN_VER, PRODUCT_BUILD ) );
}

/* Initialises the common controls DLL. You need to call this function to both
 * include the DLL library in with the linker and to load the DLL at the start of your
 * application.
 */
WINCOMMCTRLAPI int WINAPI EXPORT Amctl_Init( void )
{
   if( !fInited )
      {
   #ifdef WIN32
      OSVERSIONINFO osversion;
   #endif
      WORD wWinVer;

      /* Figure out which OS we're using. Windows NT 4.0 and
       * Windows 95/Windows NT 3.51 use different means of
       * finding out this information.
       */
      fWinNT4 = fWin95 = FALSE;
   #ifdef WIN32
      memset( &osversion, 0, sizeof(OSVERSIONINFO) );
      osversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
      GetVersionEx( &osversion );

      /* Read registry directly for Windows 9x & WinNT3.51, if WinNT 4.0 
       * and above then use SystemParametersInfo
       */
      if( osversion.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS )
         fWin95 = TRUE;
      if( ( osversion.dwPlatformId == VER_PLATFORM_WIN32_NT ) && ( osversion.dwMajorVersion >= 4 ) )
         fWinNT4 = TRUE;
   #endif

      /* Which version of Windows? This code looks for Windows 95
       * and sets an internal flag because Windows 95 provides a
       * rather different UI, and we should attempt to emulate that
       * UI rather than the standard Windows UI if possible.
       */
      wWinVer = LOWORD( GetVersion() );
      wWinVer = (( (WORD)(LOBYTE( wWinVer ) ) ) << 8 ) | ( (WORD)HIBYTE( wWinVer ) );
      fNewShell = wWinVer >= 0x35F;

      /* Enable control animation.
       */
      fCtlAnimation = TRUE;

      /* Register control classes.
       */
      if( RegisterHeaderClass( hLibInst ) )
         if( RegisterHotkeyClass( hLibInst ) )
            if( RegisterStatusClass( hLibInst ) )
               if( RegisterTabClass( hLibInst ) )
                  if( RegisterUpDownClass( hLibInst ) )
                     if( RegisterProgressClass( hLibInst ) )
                        if( RegisterListviewClass( hLibInst ) )
                           if( RegisterPropertySheetClass( hLibInst ) )
                              if( RegisterToolbarClass( hLibInst ) )
                                 if( RegisterTooltipClass( hLibInst ) )
                                    if( RegisterTreeviewClass( hLibInst ) )
                                       if( RegisterWellClass( hLibInst ) )
                                          if( RegisterPicButtonClass( hLibInst ) )
                                             if( RegisterSplitterClass( hLibInst ) )
//                                              if( RegisterBigEditClass( hLibInst ) )
                                                   if( RegisterPicCtrlClass( hLibInst ) )
                                                      if( RegisterTopicListClass( hLibInst ) )
                                                         if( RegisterVListBox( hLibInst ) )
                                                            if( Register3DLineControl( hLibInst ) )
                                                               fInited = TRUE;

      }
   return( fInited ? 1 : 0 );
}

/* This function unregisters the global classes registered by
 * this DLL.
 */
BOOL FASTCALL Amctl_Term( void )
{
   UnregisterClass( THREED_CLASSNAME, hLibInst );
   UnregisterClass( VLIST_CLASSNAME, hLibInst );
   UnregisterClass( WC_TOPICLST, hLibInst );
   UnregisterClass( WC_PICCTRL, hLibInst );
// UnregisterClass( WC_BIGEDIT, hLibInst );
   UnregisterClass( WC_SPLITTER, hLibInst );
   UnregisterClass( WC_PICBUTTON, hLibInst );
   UnregisterClass( WC_WELL, hLibInst );
   UnregisterClass( WC_TREEVIEW, hLibInst );
   UnregisterClass( TOOLTIPS_CLASS, hLibInst );
   UnregisterClass( TOOLBARCLASSNAME, hLibInst );
   UnregisterClass( WC_PROPSHEET, hLibInst );
   UnregisterClass( LISTVIEW_CLASS, hLibInst );
   UnregisterClass( PROGRESS_CLASS, hLibInst );
   UnregisterClass( UPDOWN_CLASS, hLibInst );
   UnregisterClass( WC_TABCONTROL, hLibInst );
   UnregisterClass( STATUSCLASSNAME, hLibInst );
   UnregisterClass( HOTKEY_CLASS, hLibInst );
   UnregisterClass( WC_HEADER, hLibInst );
   return( TRUE );
}

/* Sends a notification message to the parent window. Common controls communicate
 * with the parent window via the WM_NOTIFY message. The lParam parameter of the message
 * points to a notification message header, typically based on NMHDR, which contains
 * values specific to the control and the message.
 */
WINCOMMCTRLAPI LRESULT WINAPI EXPORT Amctl_SendNotify( HWND hwndTo, HWND hwndFrom, int code, NMHDR FAR * pnmhdr )
{
   pnmhdr->hwndFrom = hwndFrom;
   pnmhdr->idFrom = GetDlgCtrlID( hwndFrom );
   pnmhdr->code = code;
   return( SendMessage( hwndTo, WM_NOTIFY, pnmhdr->idFrom, (LPARAM)pnmhdr ) );
}

/* Our replacement assertion macro. Unlike Window's default assertion macro, we
 * provide the option to break back into the debugger so we can trace where the
 * problem occurs.
 */
BOOL AssertMsg( void * pExp, void * pFilename, unsigned nLine )
{
   char sz[ 200 ]; /* BUGBUG: This really isn't enough! */

   wsprintf( sz, "%s in %s line %u\n\nBreak to debugger ?", (LPSTR)pExp, (LPSTR)pFilename, nLine );
   return( IDYES == MessageBox( NULL, sz, "Assertion Error", MB_YESNO|MB_ICONEXCLAMATION ) );
}

/* This function enables or disables control animation.
 */
WINCOMMCTRLAPI BOOL WINAPI EXPORT Amctl_EnableControlsAnimation( BOOL fAnimate )
{
   BOOL fOldCtlAnimation;

   fOldCtlAnimation = fCtlAnimation;
   fCtlAnimation = fAnimate;
   return( fOldCtlAnimation );
}

/* This function sets the number of lines by which the bigedit and treeview
 * controls scroll when the WM_MOUSEWHEEL message is received.
 */
WINCOMMCTRLAPI void WINAPI EXPORT Amctl_SetControlWheelScrollLines( int iLines )
{
   iScrollLines = iLines;
}

WINCOMMCTRLAPI int WINAPI EXPORT Amctl_GetControlWheelScrollLines()
{
   return iScrollLines;
}
