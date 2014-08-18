/* ADMMDI.C - The Ameol MDI dialog manager
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
#include "pdefs.h"
#include "amuser.h"
#include <memory.h>
#include <dos.h>
#include "..\Ameol232\main.h"

/* Note: HFONT is a 4-byte handle in Windows/NT
 */
#undef GWW_EXTRA
#define  GWL_DLGPROC             DLGWINDOWEXTRA
#define  GWW_HFONT               DLGWINDOWEXTRA+4
#define  GWW_EXTRA               DLGWINDOWEXTRA+8

#define MAXLEN_MENUNAME          64
#define MAXLEN_CLASSNAME         64
#define MAXLEN_CAPTIONTEXT       256
#define MAXLEN_TYPEFACE          64

#define MAX_MSGSIZE              0xFFF0

#define DLGTOCLIENTX( x, units )   MulDiv( x, units, 4 )
#define DLGTOCLIENTY( y, units )   MulDiv( y, units, 8 )

#define WIDTHSTRING "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"

static char dtMenuName[ MAXLEN_MENUNAME ];
static char dtClassName[ MAXLEN_CLASSNAME ];
static char dtCaptionText[ MAXLEN_CAPTIONTEXT ];
static char dtTypeFace[ MAXLEN_TYPEFACE ];

static char aADMWindInfo[] = "adm_windinfo";
static char szADMNFDClass[] = "admmndfclass";

typedef struct tagADM_WINDINFO {
   int xMDI;
   int yMDI;
   int xminMDI;
   int yminMDI;
   HWND hwndFocus;
} ADM_WINDINFO;

typedef ADM_WINDINFO FAR * LPADM_WINDINFO;

static BOOL FASTCALL DefADMMDIDlgProc_OnEraseBkgnd( HWND, HDC );
static void FASTCALL DefADMMDIDlgProc_OnGetMinMaxInfo( HWND, MINMAXINFO FAR * );
static void FASTCALL DefADMMDIDlgProc_OnSetFocus( HWND, HWND );
static void FASTCALL DefADMMDIDlgProc_OnDestroy( HWND );
static void FASTCALL DefADMMDIDlgProc_OnSize( HWND, UINT, int, int );

extern INTERFACE_PROPERTIES ip;
extern ATOM aADMLow;
extern ATOM aADMHigh;
extern HBRUSH hbrDlgBrush;
extern COLORREF rgbDialog;
extern WORD wWinVer;
extern HFONT hDlgFont;

static BOOL fIgnoreSizeMsg = FALSE;

/* This function creates an MDI client window.
 */
HWND EXPORT WINAPI Adm_CreateMDIWindow( LPCSTR lpcstrAddrWndName, LPCSTR lpcstrAddrWndClass,
   HINSTANCE hInst, LPRECT lprc, DWORD dwStyle, LPARAM lParam )
{
   MDICREATESTRUCT mcs;

   mcs.szTitle = lpcstrAddrWndName;
   mcs.szClass = lpcstrAddrWndClass;
   mcs.hOwner = hInst;
   mcs.x = lprc->left;
   mcs.y = lprc->top;
   mcs.cx = lprc->right - lprc->left;
   mcs.cy = lprc->bottom - lprc->top;
   mcs.style = dwStyle;
   mcs.lParam = lParam;
   return( (HWND)SendMessage( ip.hwndMDIClient, WM_MDICREATE, 0, (LPARAM)(LPMDICREATESTRUCT)&mcs ) );
}

/* This function destroys an MDI window.
 */
void EXPORT WINAPI Adm_DestroyMDIWindow( HWND hwnd )
{
   SendMessage( ip.hwndMDIClient, WM_MDIDESTROY, (WPARAM)hwnd, 0L );
}

/* This function returns the rectangle defining the MDI
 * client region.
 */
void EXPORT WINAPI Adm_GetMDIClientRect( LPRECT lprc )
{
   GetClientRect( ip.hwndMDIClient, lprc );
}

/* This function makes the MDI dialog window active.
 */
void EXPORT WINAPI Adm_MakeMDIWindowActive( HWND hwnd )
{
   BringWindowToTop( hwnd );
   if( IsIconic( hwnd ) )
      SendMessage( ip.hwndMDIClient, WM_MDIRESTORE, (WPARAM)hwnd, 0L );
}

/* This function creates an MDI dialog window.
 */
HWND EXPORT WINAPI Adm_MDIDialog( HWND hwnd, LPCSTR lpszDlgName, LPMDICREATESTRUCT lpMDICreateStruct )
{
   LPADM_WINDINFO lpADMWindInfo;
   HGLOBAL hADMWindInfo;
   DWORD dwSize;
   int dx, dy;

   /* Create the dialog and get the physical window size
    */
   if( ( dwSize = Adm_DrawDialog( hwnd, lpszDlgName ) ) == 0 )
      return( NULL );

   /* Now allocate an ADM_WINDINFO structure and store the pointer to
    * the structure in the window property list.
    */
   if( ( hADMWindInfo = GlobalAlloc( GHND, sizeof( ADM_WINDINFO ) ) ) == NULL )
      return( NULL );
   lpADMWindInfo = GlobalLock( hADMWindInfo );
   SetProp( hwnd, aADMWindInfo, hADMWindInfo );

   /* Compute the dialog client area size and save this in the
    * structure. Then resize the window to the size requested in
    * the MDICREATESTRUCT structure.
    */
   lpADMWindInfo->xMDI = 0;
   lpADMWindInfo->yMDI = 0;
   dx = lpMDICreateStruct->cx;
   dy = lpMDICreateStruct->cy;
   if( dx == 0 )
      dx = LOWORD( dwSize );
   if( dy == 0 )
      dy = HIWORD( dwSize );
   SetWindowPos( hwnd, NULL, 0, 0, dx, dy, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER );
   lpADMWindInfo->xminMDI = 40;
   lpADMWindInfo->yminMDI = 40;
   lpADMWindInfo->xMDI = LOWORD( dwSize ) - ( 2 * GetSystemMetrics( SM_CXFRAME ) );
   lpADMWindInfo->yMDI = HIWORD( dwSize ) - ( 2 * GetSystemMetrics( SM_CYFRAME ) + GetSystemMetrics( SM_CYCAPTION ) );

   /* If the window is not being opened minimised, send a WM_ADJUSTWINDOW message
    * to the window to allow it to adjust the position and size of its controls.
    */
   if( !IsIconic( hwnd ) )
      {
      dx -= lpADMWindInfo->xMDI;
      dy -= lpADMWindInfo->yMDI;
      SendMessage( hwnd, WM_ADJUSTWINDOWS, 0, MAKELPARAM( dx, dy ) );
      lpADMWindInfo->xMDI += dx;
      lpADMWindInfo->yMDI += dy;
      fIgnoreSizeMsg = FALSE;
      }
   else
      fIgnoreSizeMsg = TRUE;

   /* All done. Unlock the structure and
    * return the window handle.
    */
   GlobalUnlock( hADMWindInfo );
   return( hwnd );
}

/* This function computes the window size of a dialog from
 * the dialog resource.
 */
BOOL EXPORT WINAPI Adm_GetMDIDialogRect( HINSTANCE hInst, LPCSTR lpszDlgName, LPRECT lprc )
{
   HRSRC hDlgRes;
   HANDLE hRes;
   WORD FAR * lpDlgRes;
   DWORD dwExtendedStyle;
   WORD dtX, dtY, dtCX, dtCY;
   WORD dtItemCount;
   HFONT hOldFont;
   WORD wUnitsX, wUnitsY;
   DWORD dtStyle;
   TEXTMETRIC tm;
   SIZE size;
   HDC hDC;

   /* Load the dialog resource into memory.
    */
   if( !( hDlgRes = FindResource( hInst, lpszDlgName, RT_DIALOG ) ) )
      return( FALSE );
   hRes = LoadResource( hInst, hDlgRes );
   lpDlgRes = LockResource( hRes );

   /* Get the dialog style and position information.
    */
   dtStyle = *((DWORD FAR *)lpDlgRes)++;
   dwExtendedStyle = *((DWORD FAR *)lpDlgRes)++;
   dtItemCount = *((WORD FAR *) lpDlgRes)++;
   dtX = *((WORD FAR *) lpDlgRes)++;
   dtY = *((WORD FAR *) lpDlgRes)++;
   dtCX = *((WORD FAR *) lpDlgRes)++;
   dtCY = *((WORD FAR *) lpDlgRes)++;

   /* Get the font size.
    */
   hDC = GetDC( NULL );
   hOldFont = SelectObject( hDC, hDlgFont );
   GetTextMetrics( hDC, &tm );
   GetTextExtentPoint( hDC, WIDTHSTRING, 52, &size );
   wUnitsX = (WORD)(size.cx  / 52);
   wUnitsY = (WORD)tm.tmHeight;
   SelectObject( hDC, hOldFont );
   ReleaseDC( NULL, hDC );

   /* Unlock the resource and compute the size.
    */
   UnlockResource( hRes );
   FreeResource( hRes );
   dtCX = DLGTOCLIENTX( dtCX, wUnitsX ) + 2 * GetSystemMetrics( SM_CXFRAME );
   dtCY = DLGTOCLIENTY( dtCY, wUnitsY ) + 2 * GetSystemMetrics( SM_CYFRAME ) + GetSystemMetrics( SM_CYCAPTION );
   SetRect( lprc, 0, 0, dtCX, dtCY );
   return( TRUE );
}

/* This function creates an MDI dialog window.
 */
DWORD EXPORT WINAPI Adm_DrawDialog( HWND hwnd, LPCSTR lpszDlgName )
{
   HRSRC hDlgRes;
   HANDLE hRes;
   WORD FAR * lpDlgRes;
   DWORD dwExtendedStyle;
   char ch;
   WORD dtX, dtY, dtCX, dtCY;
   WORD dtItemCount;
   DWORD dtStyle;
   WORD dtPointSize;
   register int i;
   register WORD nCtl;
   HINSTANCE hInst;
   WORD wUnitsX, wUnitsY;
   HDC hDC;
   HFONT hOldFont;
   TEXTMETRIC tm;
   SIZE size;

   /* Load the dialog resource into memory.
    */
   hInst = GetWindowInstance( hwnd );
   if( !( hDlgRes = FindResource( hInst, lpszDlgName, RT_DIALOG ) ) )
      return( 0L );
   hRes = LoadResource( hInst, hDlgRes );
   lpDlgRes = LockResource( hRes );

   /* Get the dialog style and position information.
    */
   dtStyle = *((DWORD FAR *)lpDlgRes)++;
   dwExtendedStyle = *((DWORD FAR *)lpDlgRes)++;
   dtItemCount = *((WORD FAR *) lpDlgRes)++;
   dtX = *((WORD FAR *) lpDlgRes)++;
   dtY = *((WORD FAR *) lpDlgRes)++;
   dtCX = *((WORD FAR *) lpDlgRes)++;
   dtCY = *((WORD FAR *) lpDlgRes)++;

   /* Get menu name
    */
   for( i = 0; ch = (char)*lpDlgRes++; ++i )
      dtMenuName[ i ] = ch;
   dtMenuName[ i ] = '\0';

   /* Get class name
    */
   for( i = 0; ch = (char)*lpDlgRes++; ++i )
      dtClassName[ i ] = ch;
   dtClassName[ i ] = '\0';

   /* Get caption text
    */
   for( i = 0; ch = (char)*lpDlgRes++; ++i )
      dtCaptionText[ i ] = ch;
   dtCaptionText[ i ] = '\0';
   if( dtCaptionText[ 0 ] )
      SetWindowText( hwnd, dtCaptionText );

   /* Get point size
    */
   if( dtStyle & DS_SETFONT ) {
      dtPointSize = *((WORD FAR *)lpDlgRes)++;
      for( i = 0; ch = (char)*lpDlgRes++; ++i )
         dtTypeFace[ i ] = ch;
      dtTypeFace[ i ] = '\0';
      }
   while( (DWORD)lpDlgRes & 0x0003 )
      ++lpDlgRes;

   /* Using the point size and face name, create the font to be used by
    * the dialog box.
    */
   hDC = GetDC( NULL );
   hOldFont = SelectObject( hDC, hDlgFont );
   GetTextMetrics( hDC, &tm );
   GetTextExtentPoint( hDC, WIDTHSTRING, 52, &size );
   wUnitsX = (WORD)(size.cx  / 52);
   wUnitsY = (WORD)tm.tmHeight;
   SelectObject( hDC, hOldFont );
   ReleaseDC( NULL, hDC );

   /* Now create the controls in the dialog box.
    */
   for( nCtl = 0; nCtl < dtItemCount; nCtl++ ) {
      WORD dtX, dtY;
      WORD dtCX, dtCY;
      char dtClassName[ MAXLEN_CLASSNAME ];
      char dtCaptionText[ MAXLEN_CAPTIONTEXT ];
      BYTE dtInfoSize;
      int dtID;
      DWORD dwExtendedStyle = 0L;
      DWORD dwStyle;
      HINSTANCE hCtlInst;
      HWND hCtl;

      dwStyle = *((DWORD FAR *)lpDlgRes)++;
      dwExtendedStyle = *((DWORD FAR *)lpDlgRes)++;
      dtX = *((WORD FAR *)lpDlgRes)++;
      dtY = *((WORD FAR *)lpDlgRes)++;
      dtCX = *((WORD FAR *)lpDlgRes)++;
      dtCY = *((WORD FAR *)lpDlgRes)++;
      dtID = *((WORD FAR *)lpDlgRes)++;
      hCtlInst = hInst;

      /* Get the control class name
       */
      if( *lpDlgRes == 0xFFFF )
         ++lpDlgRes;
      for( i = 0; ch = (char)*lpDlgRes++; ++i ) {
         dtClassName[ i ] = ch;
         if( ch & 0x80 )
            break;
         }
      if( ch & 0x80 )
         switch( ch & 0x7F ) {
            case 0:
               lstrcpy( dtClassName, "BUTTON" );
               break;

            case 1:
               lstrcpy( dtClassName, "EDIT" );
               if( wWinVer >= 0x035F )
                  {
                  /* Set the edge style to be 3D and adjust the
                   * offset/size to compensate.
                   */
                  dwExtendedStyle |= WS_EX_CLIENTEDGE;
                  --dtX;
                  --dtY;
                  dtCY += 2;
                  dtCX += 2;
                  }
               break;

            case 2:
               lstrcpy( dtClassName, "STATIC" );
               break;

            case 3:
               lstrcpy( dtClassName, "LISTBOX" );
               if( wWinVer >= 0x035F )
                  dwExtendedStyle |= WS_EX_CLIENTEDGE;
               break;

            case 4:
               lstrcpy( dtClassName, "SCROLLBAR" );
               break;

            case 5:
               lstrcpy( dtClassName, "COMBOBOX" );
               break;
            }
      else
         dtClassName[ i ] = '\0';

      /* Get the caption text
       */
      if( *lpDlgRes == 0xFFFF )
         {
         ++lpDlgRes;
         wsprintf( dtCaptionText, "#%d", *((WORD FAR *)lpDlgRes)++ );
         }
      else
         {
         for( i = 0; ch = (char)*lpDlgRes++; ++i )
            dtCaptionText[ i ] = ch;
         dtCaptionText[ i ] = '\0';
         }

      /* Handle the BigEdit class.
       * Do nothing for Win32 - edit controls can be larger than 64K.
       */
      if( lstrcmpi( dtClassName, "bigedit" ) == 0 ) {
#ifdef USEBIGEDIT
         lstrcpy( dtClassName, "EDIT" );
         if( wWinVer >= 0x035F )
            dwExtendedStyle |= WS_EX_CLIENTEDGE;
#else USEBIGEDIT
#ifdef GNU
         lstrcpy( dtClassName, "Scintilla" );         
#else GNU
         lstrcpy( dtClassName, "amxctl_editplus" );         
#endif GNU
#endif  USEBIGEDIT
         hCtlInst = hInst;
         }

      dtInfoSize = *((BYTE FAR *)lpDlgRes)++;
      dwStyle |= WS_CHILD;
      hCtl = CreateWindowEx( dwExtendedStyle, dtClassName, dtCaptionText, dwStyle,
                           DLGTOCLIENTX( dtX, wUnitsX ),
                           DLGTOCLIENTY( dtY, wUnitsY ),
                           DLGTOCLIENTX( dtCX, wUnitsX ),
                           DLGTOCLIENTY( dtCY, wUnitsY ),
                           hwnd, (HMENU)dtID, hCtlInst,
                           ( dtInfoSize == 0 ) ? NULL : (VOID FAR *)lpDlgRes );
      SendMessage( hCtl, WM_SETFONT, (WPARAM)hDlgFont, (LPARAM)FALSE );
      if( lstrcmpi( dtClassName, "BUTTON" ) == 0 )
         if( dwStyle & BS_DEFPUSHBUTTON )
            SendMessage( hwnd, DM_SETDEFID, dtID, 0L );
      if( lstrcmpi( dtClassName, "EDIT" ) == 0 && (dwStyle & ES_MULTILINE) )
         SendMessage( hCtl, EM_SETLIMITTEXT, 0xFFFFFFFF, 0L );

      /* Bounce over child info structure
       */
      lpDlgRes += dtInfoSize;
      while( (DWORD)lpDlgRes & 0x0003 )
         ++(LPSTR)lpDlgRes;
      }

   /* Unlock the resource structure and remove it from memory.
    */
   UnlockResource( hRes );
   FreeResource( hRes );
   dtCX = DLGTOCLIENTX( dtCX, wUnitsX ) + 2 * GetSystemMetrics( SM_CXFRAME );
   dtCY = DLGTOCLIENTY( dtCY, wUnitsY ) + 2 * GetSystemMetrics( SM_CYFRAME ) + GetSystemMetrics( SM_CYCAPTION );
   return( MAKELONG( dtCX, dtCY ) );
}

/* This handles MDI dialog window messages.
 */
LRESULT WINAPI EXPORT Adm_DefMDIDlgProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   switch( msg )
      {
      HANDLE_MSG( hwnd, WM_ERASEBKGND, DefADMMDIDlgProc_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_GETMINMAXINFO, DefADMMDIDlgProc_OnGetMinMaxInfo );
      HANDLE_MSG( hwnd, WM_DESTROY, DefADMMDIDlgProc_OnDestroy );
      HANDLE_MSG( hwnd, WM_SETFOCUS, DefADMMDIDlgProc_OnSetFocus );
      HANDLE_MSG( hwnd, WM_SIZE, DefADMMDIDlgProc_OnSize );

#ifndef USEBIGEDIT
      case WM_APPCOLOURCHANGE: // 2.55.2031
      case WM_TOGGLEFIXEDFONT: 
      case WM_TOGGLEHEADERS: 
      case WM_CHANGEFONT:
         {
            HWND hwndEdit;

            /* Reset the colours.
             */
            hwndEdit = GetDlgItem( hwnd, 6031 );
            SendMessage(hwndEdit,  msg, wParam, lParam);
         }
         return( 0L );
#endif USEBIGEDIT
   #ifdef WIN32
      case WM_CTLCOLOREDIT:
      case WM_CTLCOLORLISTBOX:
         break;

      case WM_CTLCOLORMSGBOX:
      case WM_CTLCOLORBTN:
      case WM_CTLCOLORDLG:
      case WM_CTLCOLORSCROLLBAR:
      case WM_CTLCOLORSTATIC:
         if( hbrDlgBrush )
            {
            SetBkColor( (HDC)wParam, rgbDialog );
            return( (LRESULT)(LPVOID)hbrDlgBrush );
            }
         break;
   #else
      case WM_CTLCOLOR:
         if( f3dInited )
            return( (LRESULT)(LPVOID)Ctl3dCtlColorEx( msg, wParam, lParam ) );
         if( hbrDlgBrush )
            {
            SetBkColor( (HDC)wParam, rgbDialog );
            return( (LRESULT)(LPVOID)hbrDlgBrush );
            }
         break;
   #endif

      case DM_GETDEFID:
      case DM_SETDEFID:
         return( DefDlgProc( hwnd, msg, wParam, lParam ) );
      }
   return( DefMDIChildProc( hwnd, msg, wParam, lParam ) );
}

/* This function handles the WM_ERASEBKGND message.
 */
static BOOL FASTCALL DefADMMDIDlgProc_OnEraseBkgnd( HWND hwnd, HDC hdc )
{
   FORWARD_WM_ERASEBKGND( hwnd, hdc, DefMDIChildProc );
   return( TRUE );
}

/* This function handles the WM_GETMINMAXINFO message.
 */
static void FASTCALL DefADMMDIDlgProc_OnGetMinMaxInfo( HWND hwnd, MINMAXINFO FAR * lpMinMaxInfo )
{
   HGLOBAL hADMWindInfo;
   LPADM_WINDINFO lpADMWindInfo;

   if( hADMWindInfo = GetProp( hwnd, aADMWindInfo ) )
      if( lpADMWindInfo = GlobalLock( hADMWindInfo ) )
         {
         lpMinMaxInfo->ptMinTrackSize.x = lpADMWindInfo->xminMDI;
         lpMinMaxInfo->ptMinTrackSize.y = lpADMWindInfo->yminMDI;
         GlobalUnlock( hADMWindInfo );
         }
   FORWARD_WM_GETMINMAXINFO( hwnd, lpMinMaxInfo, DefMDIChildProc );
}

/* This function handles the WM_DESTROY message.
 */
static void FASTCALL DefADMMDIDlgProc_OnDestroy( HWND hwnd )
{
   HGLOBAL hADMWindInfo;
   LPADM_WINDINFO lpADMWindInfo;

   if( hADMWindInfo = GetProp( hwnd, aADMWindInfo ) )
      if( lpADMWindInfo = GlobalLock( hADMWindInfo ) )
         {
         GlobalUnlock( hADMWindInfo );
         GlobalFree( hADMWindInfo );
         RemoveProp( hwnd, aADMWindInfo );
         }
}

/* This function handles the WM_SETFOCUS message.
 */
static void FASTCALL DefADMMDIDlgProc_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   HGLOBAL hADMWindInfo;
   LPADM_WINDINFO lpADMWindInfo;
   
   if( hADMWindInfo = GetProp( hwnd, aADMWindInfo ) )
      if( lpADMWindInfo = GlobalLock( hADMWindInfo ) )
         {
         SetFocus( lpADMWindInfo->hwndFocus );
         GlobalUnlock( hADMWindInfo );
         }
}

/* This function instructs the MDI client windows whether or not
 * to resize themselves. It's a hack provided to fix the bug where
 * minimising then restoring the main window breaks any maximised
 * MDI client windows within it.
 */
BOOL EXPORT WINAPI Adm_DeferMDIResizing( HWND hwnd, BOOL fDefer )
{
   BOOL fDeferOld;

   fDeferOld = fIgnoreSizeMsg;
   fIgnoreSizeMsg = fDefer;
   return( fDeferOld );
}

/* This function handles the WM_SIZE message. We post a message to the
 * dialog showing the relative offset by which the window size changed.
 */
static void FASTCALL DefADMMDIDlgProc_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   if( state != SIZE_MINIMIZED && !fIgnoreSizeMsg )
      {
      HGLOBAL hADMWindInfo;
      LPADM_WINDINFO lpADMWindInfo;

      if( hADMWindInfo = GetProp( hwnd, aADMWindInfo ) )
         if( lpADMWindInfo = GlobalLock( hADMWindInfo ) )
            {
            int dx, dy;
   
            dx = cx - lpADMWindInfo->xMDI;
            dy = cy - lpADMWindInfo->yMDI;
            SendMessage( hwnd, WM_ADJUSTWINDOWS, 0, MAKELPARAM( dx, dy ) );
            lpADMWindInfo->xMDI += dx;
            lpADMWindInfo->yMDI += dy;
            GlobalUnlock( hADMWindInfo );
            }
      }
   fIgnoreSizeMsg = FALSE;
   FORWARD_WM_SIZE( hwnd, state, cx, cy, DefMDIChildProc );
}
