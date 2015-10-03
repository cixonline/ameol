/* ADM.C - The Ameol dialog manager
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
#include <memory.h>
#include "pdefs.h"
#include "amuser.h"

#define LOADDS
#define  TASKHANDLE  DWORD

#define  MAX_HOOKS         20
#define  ADMHTYP_DLGHOOK   1
#define  ADMHTYP_CBTHOOK   2

typedef struct tagHOOKSTRUCT {
   TASKHANDLE hTask;
   HHOOK hHook;
   HHOOK hCBTHook;
} HOOKSTRUCT;

static char szADMLowAtom[] = "admlowproc";
static char szADMHighAtom[] = "admhighproc";

HBRUSH hbrDlgBrush;              /* Gray dialog background */
ATOM aADMLow;                 /* Property atom for low word of dialog address */
BOOL fSubclassDialog;            /* Whether or not dialogs are subclassed */
COLORREF rgbDialog;              /* Colour of dialog background */

static HINSTANCE hFrameInst;     /* Instance handle of the owner app frame window */
static char buf[ 512 ];          /* General purpose buffer */

static HOOKSTRUCT rgclihk[ MAX_HOOKS ];

extern INTERFACE_PROPERTIES ip;
extern HINSTANCE hLibInst;
extern WORD wWinVer;

BOOL FASTCALL ADMInstallHooks( TASKHANDLE, HINSTANCE );
BOOL FASTCALL ADMUninstallHooks( TASKHANDLE );
void FASTCALL ADMSubclassDialog( HWND );

LRESULT LOADDS WINAPI ADMDialogProc( HWND, UINT, WPARAM, LPARAM );
BOOL EXPORT WINAPI ADMBillboardProc( HWND, UINT, WPARAM, LPARAM );

void FASTCALL Amuser_SetDialogFont( HWND );

BOOL FASTCALL SendPreCtlColor( HWND, int, WPARAM, LPARAM, HBRUSH * );

/* This function initialises ADM on installation.
 */
void FASTCALL InternalInitADM( void )
{
   register int c;

   wWinVer = LOWORD( GetVersion() );
   wWinVer = (( (WORD)(LOBYTE( wWinVer ) ) ) << 8 ) | ( (WORD)HIBYTE( wWinVer ) );
   for( c = 0; c < MAX_HOOKS; ++c )
      rgclihk[ c ].hTask = (TASKHANDLE)NULL;
}

/* This function initialises the Ameol Dialog Manager
 */
int EXPORT WINAPI Adm_Init( HINSTANCE hInst )
{
   TASKHANDLE hTask;

   /* Add a dialog hook for all non-MDI dialogs.
    */
   hTask = GetCurrentThreadId();
   fSubclassDialog = TRUE;
   if( !ADMInstallHooks( hTask, hInst ) )
      return( ADM_INIT_ERROR );

   wWinVer = LOWORD( GetVersion() );
   wWinVer = (( (WORD)(LOBYTE( wWinVer ) ) ) << 8 ) | ( (WORD)HIBYTE( wWinVer ) );

   /* Create property atoms for dialog address
    */
   aADMLow = GlobalAddAtom( szADMLowAtom );

   /* Initialise some variables.
    */
   hFrameInst = hInst;

   /* Create dialog background brush
    */
   rgbDialog = wWinVer < 0x35F ? RGB(192,192,192) : GetSysColor( COLOR_3DFACE );
   hbrDlgBrush = CreateSolidBrush( rgbDialog );
   return( 0 );
}

/* This function terminates the Ameol Dialog Manager
 */
BOOL EXPORT WINAPI Adm_Exit( HINSTANCE hInst )
{
   TASKHANDLE hTask;

   /* Remove the main app from the hook table.
    */
   hTask = GetCurrentThreadId();
   ADMUninstallHooks( hTask );

   /* Remove the two atoms we created
    */
   GlobalDeleteAtom( aADMLow );

   /* Delete the resources we created.
    */
   DeleteBrush( hbrDlgBrush );
   return( TRUE );
}

/* This function is called in response to WM_SYSCOLORCHANGE
 * from the main app.
 */
void EXPORT WINAPI Adm_SysColorChange( void )
{
   if( wWinVer >= 0x35F )
      {
      DeleteBrush( hbrDlgBrush );
      rgbDialog = GetSysColor( COLOR_3DFACE );
      hbrDlgBrush = CreateSolidBrush( rgbDialog );
      }
}

/* This function enables or disables dialog subclassing.
 */
BOOL EXPORT WINAPI Adm_EnableDialogSubclassing( BOOL fEnable )
{
   BOOL fRet = fSubclassDialog;

   fSubclassDialog = fEnable;
   return( fRet );
}

/* This function traps all CBT messages for the current application.
 */
DWORD EXPORT WINAPI ADMCBTFilterHook( int nCode, WPARAM wParam, LPARAM lParam )
{
   static HWND hwndHookDlg = NULL;
   TASKHANDLE hTask;
   register int c;

   if( nCode == HCBT_CREATEWND )
      {
      LPCREATESTRUCT lpcs;

      lpcs = ((LPCBT_CREATEWND)lParam)->lpcs;
      if( lpcs->lpszClass == WC_DIALOG )
         hwndHookDlg = (HWND)wParam;
      else if( hwndHookDlg != NULL && fSubclassDialog )
         {
         ADMSubclassDialog( hwndHookDlg );
         hwndHookDlg = NULL;
         }
      }
   hTask = GetCurrentThreadId();
   for( c = 0; c < MAX_HOOKS; ++c )
      if( rgclihk[ c ].hTask == hTask )
         break;
   if( c == MAX_HOOKS )
      return( 0L );
   return( CallNextHookEx( rgclihk[ c ].hCBTHook, nCode, wParam, lParam ) );
}

/* This function subclasses the specified dialog.
 */
void FASTCALL ADMSubclassDialog( HWND hDlg )
{
   WNDPROC lpfOldDlgProc;

   lpfOldDlgProc = SubclassWindow( hDlg, (FARPROC)ADMDialogProc );
   SetProp( hDlg, (LPCSTR)aADMLow, (HANDLE)lpfOldDlgProc );
}

/* This function is the prologue dialog procedure for all dialog boxes
 * subclassed by the ADM manager.
 */
LRESULT LOADDS EXPORT WINAPI ADMDialogProc( HWND hwnd, UINT wm, WPARAM wParam, LPARAM lParam )
{
   WNDPROC lpfOldDlgProc;

   lpfOldDlgProc = (WNDPROC)GetProp( hwnd, (LPCSTR)aADMLow );
   switch( wm )
      {
      case WM_INITDIALOG: {
         RECT rc;
         RECT rcScreen;
         int x, y;

         /* Center the dialog box on the screen.
          */
         if( NULL == ip.hwndFrame || IsIconic( ip.hwndFrame ) )
            SetRect( &rcScreen, 0, 0, GetSystemMetrics( SM_CXSCREEN ), GetSystemMetrics( SM_CYSCREEN ) );
         else
            GetWindowRect( ip.hwndFrame, &rcScreen );
         GetWindowRect( hwnd, &rc );
         x = ( ( rcScreen.right - rcScreen.left ) - ( rc.right - rc.left ) ) / 2;
         y = ( ( rcScreen.bottom - rcScreen.top ) - ( rc.bottom - rc.top ) ) / 3;
         SetWindowPos( hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE );
         break;
         }

      case WM_CTLCOLOREDIT:
      case WM_CTLCOLORLISTBOX: {
         HBRUSH hbr;

         if( SendPreCtlColor( hwnd, wm, wParam, lParam, &hbr ) )
            return( (LRESULT)hbr );
         break;
         }

      case WM_CTLCOLORMSGBOX:
      case WM_CTLCOLORBTN:
      case WM_CTLCOLORDLG:
      case WM_CTLCOLORSCROLLBAR:
      case WM_CTLCOLORSTATIC: {
         HBRUSH hbr;

         if( SendPreCtlColor( hwnd, wm, wParam, lParam, &hbr ) )
            return( (LRESULT)hbr );
         if( hbrDlgBrush )
            {
            SetBkColor( (HDC)wParam, rgbDialog );
            return( (LRESULT)(LPVOID)hbrDlgBrush );
            }
         break;
         }

      case WM_COMMAND:
         if( wParam == IDD_HELP )
            {
            PostMessage( hwnd, WM_ADMHELP, 0, 0L );
            return( 0L );
            }
         break;
      }
   return( CallWindowProc( lpfOldDlgProc, hwnd, wm, wParam, lParam ) );
}

/* This function sends WM_PRECTLCOLOR to the dialog to allow it the
 * option to override the default WM_CTLCOLOR stuff.
 */
BOOL FASTCALL SendPreCtlColor( HWND hwnd, int wm, WPARAM wParam, LPARAM lParam, HBRUSH * phbr )
{
   PRECTLCOLORSTRUCT pcc;

   pcc.wm = wm;
   pcc.wParam = wParam;
   pcc.lParam = lParam;
   pcc.fOverride = FALSE;
   *phbr = (HBRUSH)SendMessage( hwnd, WM_PRECTLCOLOR, 0, (LPARAM)(LPSTR)&pcc );
   return( pcc.fOverride );
}

/* This function traps dialog box messages to handle the F1 key.
 */
DWORD EXPORT WINAPI ADMMsgFilterHook( int nCode, WPARAM wParam, LPARAM lParam )
{
   register int c;
   TASKHANDLE hTask;

   if( nCode == MSGF_DIALOGBOX )
      if( lParam != 0 )
         {
         LPMSG lpMsg;

         lpMsg = (LPMSG)lParam;
         switch( lpMsg->message )
            {
            case WM_KEYDOWN:
               if( lpMsg->wParam == VK_F1 )
                  {
                  PostMessage( GetParent( lpMsg->hwnd ), WM_ADMHELP, 0, 0L );
                  return( TRUE );
                  }
               break;
            }
         }
   hTask = GetCurrentThreadId();
   for( c = 0; c < MAX_HOOKS; ++c )
      if( rgclihk[ c ].hTask == hTask )
         break;
   if( c == MAX_HOOKS )
      return( 0L );
   return( CallNextHookEx( rgclihk[ c ].hHook, nCode, wParam, lParam ) );
}

/* This function hooks into the DialogBoxParam function.
 */
int EXPORT WINAPI Adm_Dialog( HINSTANCE hInst, HWND hwnd, LPCSTR psDlgName, DLGPROC lfpDlgProc, LPARAM lParam )
{
   return( DialogBoxParam( hInst, psDlgName, hwnd, lfpDlgProc, lParam ) );
}

/* This function hooks into the CreateDialogParam function.
 */
HWND EXPORT WINAPI Adm_CreateDialog( HINSTANCE hInst, HWND hwnd, LPCSTR psDlgName, DLGPROC lfpDlgProc, LPARAM lParam )
{
   return( CreateDialogParam( hInst, psDlgName, hwnd, lfpDlgProc, lParam ) );
}

/* This function creates a billboard. A billboard is a modeless dialog with no
 * no 'feedback' controls which is simply used to show information.
 */
HWND EXPORT WINAPI Adm_Billboard( HINSTANCE hInst, HWND hwnd, LPCSTR psDlgName )
{
   HWND hwndDlg;

   if( hwndDlg = Adm_CreateDialog( hInst, hwnd, psDlgName, ADMBillboardProc, 0L ) )
      {
      ShowWindow( hwndDlg, SW_SHOW );
      UpdateWindow( hwndDlg );
      }
   return( hwndDlg );
}

/* This function is a simple dialog procedure for billboard windows.
 */
BOOL EXPORT WINAPI ADMBillboardProc( HWND hwnd, UINT wm, WPARAM wParam, LPARAM lParam )
{
   return( FALSE );
}

/* This function adds the specified task to the array of hooked tasks that
 * are monitored by ADM. Up to MAX_HOOKS tasks can be hooked.
 */
BOOL FASTCALL ADMInstallHooks( TASKHANDLE hTask, HINSTANCE hInst )
{
   register int c;

   for( c = 0; c < MAX_HOOKS; ++c )
      {
      if( rgclihk[ c ].hTask == (TASKHANDLE)NULL )
         {
         rgclihk[ c ].hTask = hTask;
         rgclihk[ c ].hHook = SetWindowsHookEx( WH_MSGFILTER, ADMMsgFilterHook, hInst, hTask );
         rgclihk[ c ].hCBTHook = SetWindowsHookEx( WH_CBT, ADMCBTFilterHook, hInst, hTask );
         break;
         }
      }
   return( c < MAX_HOOKS );
}

/* This function removes the specified task from the hook table.
 */
BOOL FASTCALL ADMUninstallHooks( TASKHANDLE hTask )
{
   register int c;

   for( c = 0; c < MAX_HOOKS; ++c )
      if( rgclihk[ c ].hTask == hTask )
         {
         rgclihk[ c ].hTask = (TASKHANDLE)NULL;
         UnhookWindowsHookEx( rgclihk[ c ].hHook );
         UnhookWindowsHookEx( rgclihk[ c ].hCBTHook );
         break;
         }
   return( c < MAX_HOOKS );
}

/* This function expands the specified window by the given amounts,
 * where negative values reduce the window size.
 */
void EXPORT WINAPI Adm_InflateWnd( HWND hwnd, int dx, int dy )
{
   RECT rc;

   GetWindowRect( hwnd, &rc );
   rc.right -= rc.left;
   rc.bottom -= rc.top;
   SetWindowPos( hwnd, NULL, 0, 0, rc.right + dx, rc.bottom + dy, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER );
}

/* This function moves the specified window by the given amounts,
 * where negative values are to the left and up.
 */
void EXPORT WINAPI Adm_MoveWnd( HWND hwnd, int dx, int dy )
{
   RECT rc;

   GetWindowRect( hwnd, &rc );
   ScreenToClient( GetParent( hwnd ), (LPPOINT)&rc );
   SetWindowPos( hwnd, NULL, rc.left + dx, rc.top + dy, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER );
}
