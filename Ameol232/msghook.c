/* MSGHOOK.C - Handles the Ameol messagebox
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

#include "main.h"
#include "palrc.h"
#include "resource.h"
#include <string.h>

static int nHelpCode;
static char FAR sz[ 300 ];

LRESULT EXPORT CALLBACK MsgBoxHook( HWND, UINT, WPARAM, LPARAM );
static void MsgBoxHook_OnCommand( HWND, int, HWND, UINT );

/* This function handles the new style Ameol message box, complete with
 * Help button.
 */
int FASTCALL fMessageBox( HWND hwnd, int nContext, LPCSTR lpszStr, int wType )
{
   int wStyle = 0;
   char szName[255];

   wsprintf(szName, "%s - %s", szLoginUsername, szAppName);

   if( !fQuitting && IsIconic( hwndFrame ) ) {
      /* Make a local copy of the string, because it may have been
       * cached by GS() and the cache could be overwritten when Ameol
       * restores itself from an icon.
       */
      strcpy( sz, lpszStr );
      SetTimer( hwndFrame, TID_FLASHICON, 500, NULL );
      while( IsIconic( hwndFrame ) )
         TaskYield();
      KillTimer( hwndFrame, TID_FLASHICON );
      lpszStr = sz;
      }
   nHelpCode = nContext;
   if( nHelpCode )
      wStyle = MB_EX_HELP;
   HideIntroduction();
   if( fNoisy )
      MessageBeep( wType & 0xF0 );
   return( Adm_MsgBox( hwnd, (LPSTR)lpszStr, (LPSTR)szName, wType, wStyle, (WNDPROC)MsgBoxHook ) );
}

/* This function processes certain messages intended for the new
 * style Ameol message boxes.
 */
LRESULT EXPORT CALLBACK MsgBoxHook( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      case WM_ADMHELP:
         if( 0 != nHelpCode )
            HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, (DWORD)nHelpCode );
         break;

      HANDLE_MSG( hwnd, WM_COMMAND, MsgBoxHook_OnCommand );
      }  
   return( 0L );
}

/* This function handles the WM_COMMAND message.
 */
static void MsgBoxHook_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   if( id == IDEXHELP && 0 != nHelpCode )
      HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, (DWORD)nHelpCode );
}
