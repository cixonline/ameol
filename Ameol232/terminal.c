/* TERMINAL.C - The Palantir terminal module
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
#include <commdlg.h>
#include <direct.h>
#include <io.h>
#include <ctype.h>
#include <string.h>
#include "amlib.h"
#include "telnet.h"
#include "command.h"
#include "toolbar.h"
#include "terminal.h"
#include "trace.h"

#pragma function(memset)

#define   THIS_FILE __FILE__

#define ZRQINIT          0         /* Request receive init */
#define ZRINIT           1         /* Receive init */
#define ZDLE             CAN
#define ZPAD             '*'

char NEAR szTerminalFrameWndClass[] = "amctl_terminalframewndcls";
char NEAR szTerminalWndClass[] = "amctl_terminalwndcls";
char NEAR szTerminalWndName[] = "Terminal";
char NEAR szTerminalLog[] = "TERMINAL.LOG";

COLORREF B4ToRGBTable[ 8 ] = {
     RGB( 0, 0, 0 ),
     RGB( 255, 0, 0 ),
     RGB( 0, 255, 0 ),
     RGB( 255, 255, 0 ),
     RGB( 0, 0, 255 ),
     RGB( 255, 0, 255 ),
     RGB( 0, 255, 255 ),
     RGB( 255, 255, 255 )
     };

typedef struct tagTERMENTRY {
     struct tagTERMENTRY FAR * lptrmNext;    /* Pointer to next entry */
     char szName[LEN_TERMNAME+1];                 /* Name of this entry */
     UINT iCommandID;                                       /* Command ID associated with entry */
} TERMENTRY, FAR * LPTERMENTRY;

LPTERMENTRY lptrmFirst = NULL;                    /* First terminal entry */

/* Make sure this is the right window.
 */
#define   MUST_BE_FRAME_WND(h)               ASSERT((GetDlgCtrlID((h)) != IDD_TERMINAL ))
#define   MUST_BE_CHILD_WND(h)               ASSERT((GetDlgCtrlID((h)) == IDD_TERMINAL ))

static BOOL fZModemUploadActive;             /* ZMODEM Send dialog is open */
static BOOL fRegistered = FALSE;             /* TRUE if we've registered the terminal window class */
static BOOL fLineDropped = TRUE;             /* TRUE if the line dropped */
static BOOL fDefDlgEx = FALSE;               /* DefDlg recursion flag trap */
static BOOL fKickStart = FALSE;              /* Kick start after connect */
static BOOL fInitialisingTerminal;      /* TRUE if we're initialising terminal */
static HWND hwndZStatus = NULL;              /* ZMODEM status window */

BOOL FASTCALL TerminalFrame_OnCreate( HWND, CREATESTRUCT FAR * );
void FASTCALL TerminalFrame_OnClose( HWND );
void FASTCALL TerminalFrame_OnDestroy( HWND );
void FASTCALL TerminalFrame_OnCommand( HWND, int, HWND, UINT );
void FASTCALL TerminalFrame_OnMove( HWND, int, int );
void FASTCALL TerminalFrame_MDIActivate( HWND, BOOL, HWND, HWND );
void FASTCALL TerminalFrame_OnSetFocus( HWND, HWND );
void FASTCALL TerminalFrame_OnAdjustWindows( HWND, int, int );

void FASTCALL Terminal_OnHScroll( HWND, HWND, UINT, int );
void FASTCALL Terminal_OnVScroll( HWND, HWND, UINT, int );
void FASTCALL Terminal_OnChar( HWND, UINT, int );
void FASTCALL Terminal_OnPaint( HWND );
void FASTCALL Terminal_OnSetFocus( HWND, HWND );
void FASTCALL Terminal_OnKillFocus( HWND, HWND );
void FASTCALL Terminal_OnKey( HWND, UINT, BOOL, int, UINT );
void FASTCALL Terminal_OnRButtonDown( HWND, BOOL, int, int, UINT );
void FASTCALL Terminal_OnLButtonDown( HWND, BOOL, int, int, UINT );
BOOL FASTCALL Terminal_OnEraseBkGnd( HWND, HDC );
void FASTCALL Terminal_OnSize( HWND, UINT, int, int );
void FASTCALL Terminal_OnNCPaint( HWND, HRGN );

BOOL FASTCALL Terminal_ReadProperties( LPSTR, TERMINALWND FAR * );
BOOL FASTCALL Terminal_CreateConnection( HWND, LPSTR, BOOL );
void FASTCALL TextBltToImageBitmap( HWND, LPRECT );
void FASTCALL Terminal_ChildWriteString( HWND, LPSTR, int, BOOL );
void FASTCALL Terminal_WriteChar( HWND, char, BOOL );
void FASTCALL Terminal_SetCursor( HWND );
void FASTCALL Terminal_WriteAnsiChar( HWND, char );
void FASTCALL Terminal_WriteTTYChar( HWND, char );
void FASTCALL Terminal_WriteRawChar( HWND, char );
void FASTCALL Terminal_NewLine( HWND );
void FASTCALL Terminal_RenderLine( HWND );
void FASTCALL Terminal_RenderScreen( HWND, int );
void FASTCALL Terminal_DrawLine( HWND, int, int, LPRECT, LPSTR, LPSTR, int );
void FASTCALL Terminal_SetScrollRange( HWND, int, int, int );
void FASTCALL Terminal_ShowOption( HWND, WORD );
void FASTCALL Terminal_ShowOptionString( HWND, char * );
void FASTCALL Terminal_DrawSelection( HWND, int, int, int, int );
HGLOBAL FASTCALL Terminal_CopySelection( HWND );
void FASTCALL Terminal_Properties( HWND, TERMINALWND FAR * );

BOOL FASTCALL OpenTerminalLog( HWND, LPSTR );
BOOL FASTCALL CloseTerminalLog( HWND );
BOOL FASTCALL ResumeTerminalLog( HWND );
BOOL FASTCALL PauseTerminalLog( HWND );

BOOL FASTCALL WriteCharToConnection( HWND, char );
BOOL FASTCALL WriteStringToConnection( HWND, char *, int );

void FASTCALL CmdTerminalPrint( HWND );
BOOL FASTCALL CharInRect( int, int, int, int, int, int );
void FASTCALL ShowTerminalLogFilename( HWND, LPLOGFILE );
void FASTCALL HandleCursorRepos( HWND );
void FASTCALL EnableDisableFXmitButtons( HWND, BOOL );
LPTERMENTRY FASTCALL AddTerminalToCommandTable( LPSTR );
BOOL FASTCALL RemoveTerminalFromCommandTable( LPSTR );
void FASTCALL Draw3DFrame( HDC, RECT FAR * );

BOOL FASTCALL NewTerminalConnect( HWND, TERMINALWND * );
BOOL EXPORT CALLBACK NewConnect_P1( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL NewConnect_P1_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL NewConnect_P1_OnNotify( HWND, int, LPNMHDR );
void FASTCALL NewConnect_P1_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK NewConnect_P2( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL NewConnect_P2_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL NewConnect_P2_OnNotify( HWND, int, LPNMHDR );
void FASTCALL NewConnect_P2_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT FAR PASCAL TerminalCallback( LPCOMMDEVICE, int, DWORD, LPARAM );

BYTE FASTCALL RGBToB4( COLORREF FAR *, COLORREF );
COLORREF FASTCALL FgB4ToRGB( TERMINALWND FAR *, BYTE );
COLORREF FASTCALL BgB4ToRGB( TERMINALWND FAR *, BYTE );

LRESULT EXPORT CALLBACK TerminalWndProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK TerminalFrameWndProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT CALLBACK TerminalConnectDlg( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL TerminalConnectDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL TerminalConnectDlg_OnCommand( HWND, int, HWND, UINT );
BOOL FASTCALL TerminalConnectDlg_OnInitDialog( HWND, HWND, LPARAM );

BOOL EXPORT CALLBACK ZStatus( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL ZStatus_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL ZStatus_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL ZStatus_DlgProc( HWND, UINT, WPARAM, LPARAM );

int EXPORT CALLBACK FileTransferStatus( LPFILETRANSFERINFO );

extern BOOL FASTCALL NewConnCard( HWND, char * );

/* This function installs all configured terminal
 * connections into the command table.
 */
void FASTCALL InstallTerminals( void )
{
     LPSTR lpsz;

     INITIALISE_PTR(lpsz);

     /* Get the list of installed terminal emulators and
      * install them into the command table.
      */
     if( fNewMemory( &lpsz, 8000 ) )
          {
          UINT c;

          Amuser_GetPPString( szTerminals, NULL, "", lpsz, 8000 );
          for( c = 0; lpsz[ c ]; ++c )
               {
               AddTerminalToCommandTable( &lpsz[ c ] );
               c += strlen( &lpsz[ c ] );
               }
          FreeMemory( &lpsz );
          }
}

/* This function installs the specified terminal connection
 * in the Ameol command table. Up to 1000 simultaneous commands
 * can be accommodated - enough for most.
 */
LPTERMENTRY FASTCALL AddTerminalToCommandTable( LPSTR lpszName )
{
     static UINT iCommandID = IDM_TERMINALFIRST;
     LPTERMENTRY lptrmNew;
     CMDREC cmd;

     INITIALISE_PTR(lptrmNew);

     /* First create an entry for this terminal in the list.
      */
     if( fNewMemory( &lptrmNew, sizeof(TERMENTRY ) ) )
          {
          if( iCommandID > IDM_TERMINALLAST )
               iCommandID = IDM_TERMINALFIRST;
          lptrmNew->lptrmNext = lptrmFirst;
          lptrmFirst = lptrmNew;
          strcpy( lptrmNew->szName, lpszName );
          lptrmNew->iCommandID = iCommandID++;

          /* Ensure we have a Terminals category.
           */
          if( !CTree_FindCategory( CAT_TERMINAL ) )
               CTree_AddCategory( CAT_TERMINAL, (WORD)-1, "Terminals" );

          /* Next, create an entry in the command table.
           */
          cmd.iCommand = lptrmNew->iCommandID;
          cmd.lpszString = lptrmNew->szName;
          cmd.lpszCommand = lptrmNew->szName;
          cmd.iDisabledString = 0;
          cmd.lpszTooltipString = lptrmNew->szName;
          cmd.iToolButton = TB_TERMINAL;
          cmd.wCategory = CAT_TERMINAL;
          cmd.wDefKey = 0;
          cmd.iActiveMode = WIN_ALL;
          cmd.hLib = NULL;
          cmd.wNewKey = 0;
          cmd.wKey = 0;
          cmd.nScheduleFlags = NSCHF_NONE;
          SetCommandCustomBitmap( &cmd, BTNBMP_GRID, fNewButtons? 35 : 15  );
          if( CTree_InsertCommand( &cmd ) )
               return( lptrmNew );
          }
     return( NULL );
}

/* Given the terminal command name, this function retrieves the
 * terminal command ID.
 */
UINT FASTCALL GetTerminalCommandID( LPSTR lpszName )
{
     LPTERMENTRY lptrm;

     for( lptrm = lptrmFirst; lptrm; lptrm = lptrm->lptrmNext )
          if( strcmp( lpszName, lptrm->szName ) == 0 )
               return( lptrm->iCommandID );
     return( 0 );
}

/* Given a terminal command ID, this function retrieves the
 * name of the terminal command.
 */
BOOL FASTCALL GetTerminalCommandName( UINT iCommand, char * lpUsrBuf )
{
     LPTERMENTRY lptrm;

     for( lptrm = lptrmFirst; lptrm; lptrm = lptrm->lptrmNext )
          if( iCommand == lptrm->iCommandID )
               {
               strcpy( lpUsrBuf, lptrm->szName );
               return( TRUE );
               }
     return( FALSE );
}

/* This function edits the terminal properties associated
 * with the specified command.
 */
void FASTCALL EditTerminal( HWND hwnd, int iCommand )
{
     CMDREC cmd;

     /* First find the terminal name
      */
     cmd.iCommand = iCommand;
     if( CTree_GetCommand( &cmd ) )
          {
        TERMINALWND tw;

          /* Read the properties for the specified
           * connection.
           */
          INITIALISE_PTR(tw.tp.lpcrBgMapTable);
          INITIALISE_PTR(tw.tp.lpcrFgMapTable);
          Terminal_ReadProperties( (LPSTR)cmd.lpszTooltipString, &tw );

          /* Setting the hwnd parameter of the TERMINALWND structure
           * to NULL will stop the properties dialog from attempting
           * to physically apply any changes.
           */
          tw.hwnd = NULL;
          Terminal_Properties( hwnd, &tw );

          /* If terminal name has changed, update the command
           * table.
           */
          if( strcmp( cmd.lpszTooltipString, tw.szName ) != 0 )
               {
               cmd.lpszTooltipString = tw.szName;
               cmd.lpszString = tw.szName;
               cmd.lpszCommand= tw.szName;
               CTree_PutCommand( &cmd );
               }
          }
}

/* This function removes the specified named terminal from
 * the command table.
 */
BOOL FASTCALL RemoveTerminalFromCommandTable( LPSTR lpszName )
{
     LPTERMENTRY FAR * lpptrm;

     for( lpptrm = &lptrmFirst; *lpptrm; lpptrm = &(*lpptrm)->lptrmNext )
          if( strcmp( (*lpptrm)->szName, lpszName ) == 0 )
               {
               LPTERMENTRY lptrm;

               lptrm = *lpptrm;
               CTree_DeleteCommand( (*lpptrm)->iCommandID );
               *lpptrm = lptrm->lptrmNext;
               FreeMemory( &lptrm );
               break;
               }
     return( TRUE );
}

/* This function locates the terminal connection assigned
 * to this command, then opens a window on that command.
 */
void FASTCALL NewTerminalByCmd( HWND hwndParent, UINT iCommand )
{
     CMDREC cmd;

     cmd.iCommand = iCommand;
     if( CTree_GetCommand( &cmd ) )
          NewTerminal( hwndParent, (LPSTR)cmd.lpszTooltipString, FALSE );
}

/* This function creates a terminal window.
 */
HWND FASTCALL NewTerminal( HWND hwndParent, LPSTR lpszConnectionCard, BOOL fDumbTerminal )
{
     char szSection[ LEN_TERMNAME+20+1 ];
     char szName[ LEN_TERMNAME+1 ];
     TERMINALWND FAR * ptwnd;
     HWND hwndTerminal;
     DWORD dwState;
     char sz[ 40 ];
     BOOL fConnect;
     int nRetCode;
     RECT rc;

     /* Register the group list window class if we have
      * not already done so.
      */
     if( !fRegistered )
          {
          WNDCLASS wc;

          wc.style                 = CS_HREDRAW | CS_VREDRAW;
          wc.lpfnWndProc      = TerminalFrameWndProc;
          wc.hIcon                 = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_TERMINAL) );
          wc.hCursor               = LoadCursor( NULL, IDC_ARROW );
          wc.lpszMenuName     = NULL;
          wc.cbWndExtra       = GWW_EXTRABYTES;
          wc.cbClsExtra       = DLGWINDOWEXTRA;
          wc.hbrBackground    = (HBRUSH)( COLOR_APPWORKSPACE + 1 );
          wc.lpszClassName    = szTerminalFrameWndClass;
          wc.hInstance      = hInst;
          if( !RegisterClass( &wc ) )
               return( FALSE );

          wc.style                 = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
          wc.lpfnWndProc      = TerminalWndProc;
          wc.hIcon                 = NULL;
          wc.hCursor               = LoadCursor( NULL, IDC_IBEAM );
          wc.lpszMenuName     = NULL;
          wc.cbWndExtra       = 0;
          wc.cbClsExtra       = 0;
          wc.hbrBackground    = (HBRUSH)( COLOR_APPWORKSPACE + 1 );
          wc.lpszClassName    = szTerminalWndClass;
          wc.hInstance      = hInst;
          if( !RegisterClass( &wc ) )
               return( FALSE );
          fRegistered = TRUE;
          }

     /* The window will appear in the left half of the
      * client area.
      */
     GetClientRect( hwndMDIClient, &rc );
     InflateRect( &rc, -10, -10 );
     dwState = 0;

     /* Fill out a connection record with details of what we
      * want to connect to.
      */
     if( NULL == lpszConnectionCard )
          {
          if( !Adm_Dialog( hRscLib, hwndFrame, MAKEINTRESOURCE(IDDLG_TERMINALCONNECT), TerminalConnectDlg, (LPARAM)(LPSTR)&szName ) )
               return( FALSE );
          lpszConnectionCard = szName;
          }

     /* Get the actual window size, position and state.
      */
     wsprintf( szSection, "%s Properties", lpszConnectionCard );
     if( Amuser_GetPPString( szSection, "Window", "", sz, sizeof( sz ) ) )
          Amuser_ParseWindowState( sz, &rc, &dwState );
     else
     {
          rc.right = 300;
          rc.bottom = 200;
     }
     if( hwndActive && IsMaximized( hwndActive ) )
          dwState = WS_MAXIMIZE;

     /* Ask if they want to connect or just open a
      * direct connection.
      */
     if( fDumbTerminal )
          fConnect = TRUE;
     else
          {
          wsprintf( lpTmpBuf, GS(IDS_STR1143), lpszConnectionCard );
          nRetCode = fMessageBox( hwndParent, 0, lpTmpBuf, MB_YESNOCANCEL|MB_ICONINFORMATION );
          if( IDCANCEL == nRetCode )
               return( NULL );
          fConnect = nRetCode == IDYES;
          }

     /* Create the window.
      */
     fInitialisingTerminal = FALSE;
     TRACE_DEBUG( "Creating CIX terminal window" );
     hwndTerminal = Adm_CreateMDIWindow( szTerminalWndName, szTerminalFrameWndClass, hInst, &rc, dwState, (LPARAM)lpszConnectionCard );

     /* Show the window.
      */
     if( NULL != hwndTerminal )
          {
          TRACE_DEBUG( "Created CIX terminal window" );
          ptwnd = GetTerminalWnd( hwndTerminal );
          SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndTerminal, 0L );
          UpdateWindow( hwndTerminal );
          SetFocus( ptwnd->hwndTerm );

          /* Finally create a connection.
           */
          if( !fDumbTerminal )
               {
               TRACE_DEBUG( "Opening connection in terminal window" );
               if( !Terminal_CreateConnection( hwndTerminal, lpszConnectionCard, fConnect ) )
                    {
                    FORWARD_WM_CLOSE( hwndTerminal, DefMDIChildProc );
                    hwndTerminal = NULL;
                    }
               }
          }
     return( hwndTerminal );
}

/* This function handles all messages for child windows.
 */
LRESULT EXPORT CALLBACK TerminalFrameWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
     switch( message )
          {
          HANDLE_MSG( hwnd, WM_CREATE, TerminalFrame_OnCreate );
          HANDLE_MSG( hwnd, WM_MDIACTIVATE, TerminalFrame_MDIActivate );
          HANDLE_MSG( hwnd, WM_MOVE, TerminalFrame_OnMove );
          HANDLE_MSG( hwnd, WM_SIZE, TerminalFrame_OnSize );
          HANDLE_MSG( hwnd, WM_CLOSE, TerminalFrame_OnClose );
          HANDLE_MSG( hwnd, WM_COMMAND, TerminalFrame_OnCommand );
          HANDLE_MSG( hwnd, WM_DESTROY, TerminalFrame_OnDestroy );
          HANDLE_MSG( hwnd, WM_ERASEBKGND, Common_OnEraseBkgnd );
          HANDLE_MSG( hwnd, WM_SETFOCUS, TerminalFrame_OnSetFocus );
          HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, TerminalFrame_OnAdjustWindows );

          case WM_CHANGEFONT:
               if( wParam == FONTYP_TERMINAL || 0xFFFF == wParam )
                    {
                    TERMINALWND FAR * ptwnd;
                    WORD wSize;
                    RECT rc;

                    /* Delete the current font and set the new one.
                     */
                    ptwnd = GetTerminalWnd( hwnd );
                    if( ptwnd->tp.hFont )
                         DeleteFont( ptwnd->tp.hFont );
                    ptwnd->tp.hFont = CreateFontIndirect( &lfTermFont );

                    /* Recalculate the terminal dimensions, etc.
                     */
                    Terminal_ComputeCellDimensions( ptwnd->hwndTerm );
                    GetClientRect( ptwnd->hwnd, &rc );
                    wSize = IsIconic( ptwnd->hwnd ) ? SIZE_MINIMIZED : IsZoomed( hwnd ) ? SIZE_MAXIMIZED : SIZE_RESTORED;
                    TerminalFrame_OnSize( ptwnd->hwnd, wSize, rc.right, rc.bottom );
                    InvalidateRect( ptwnd->hwnd, NULL, FALSE );
                    UpdateWindow( ptwnd->hwnd );
                    }
               return( 0L );

          case WM_APPCOLOURCHANGE:
               if( wParam == WIN_TERMINAL )
                    {
                    TERMINALWND FAR * ptwnd;

                    /* First change the configuration for this window.
                     */
                    ptwnd = GetTerminalWnd( hwnd );
                    if( strcmp( ptwnd->szName, "Dumb Terminal" ) == 0 )
                         {
                         /* Now update the terminal window.
                          */
                         if( ptwnd->hwnd )
                              {
                              if( ptwnd->tp.fAnsi )
                                   {
                                   SetFgAttr( ptwnd->crAttrByte, FgRGBToB4( ptwnd, crTermText ) );
                                   SetBgAttr( ptwnd->crAttrByte, BgRGBToB4( ptwnd, crTermWnd ) );
                                   ptwnd->crRealAttrByte = ptwnd->crAttrByte;
                                   memset( ptwnd->lpColourBuf, ptwnd->crAttrByte, ptwnd->tp.nBufferSize * ptwnd->tp.nLineWidth );
                                   }
                              InvalidateRect( ptwnd->hwnd, NULL, FALSE );
                              UpdateWindow( ptwnd->hwnd );
                              }
                         }
                    }
               return( 0L );

          case WM_GETMINMAXINFO: {
               MINMAXINFO FAR * lpMinMaxInfo;
               TERMINALWND FAR * ptwnd;
               LRESULT lResult;
               int cyClient;
               int cyBorder;
               RECT rcClient;
               RECT rcMain;

               /* Set the minimum tracking size so the client window can never
                * be resized below 24 lines.
                */
               lResult = Adm_DefMDIDlgProc( hwnd, message, wParam, lParam );
               ptwnd = GetTerminalWnd( hwnd );
               if( NULL != ptwnd )
                    if( NULL != ptwnd->hwndTerm && !ptwnd->fAttached )
                         {
                         lpMinMaxInfo = (MINMAXINFO FAR *)lParam;
                         cyClient = ( ptwnd->nLineHeight * 24 ) + 4;
                         GetWindowRect( hwnd, &rcMain );
                         GetClientRect( ptwnd->hwndTerm, &rcClient );
                         cyBorder = ( rcMain.bottom - rcMain.top ) - ( rcClient.bottom - rcClient.top );
                         lpMinMaxInfo->ptMinTrackSize.y = cyClient + cyBorder;
                         }
               return( lResult );
               }

          case DM_SETDEFID:
          case DM_GETDEFID:
               return( 0L );

          case WM_QUERYISONLINE:
               return( TRUE );

          case WM_DISCONNECT:
               TerminalFrame_OnClose( hwnd );
               return( 0L );

          default:
               break;
          }
     return( Adm_DefMDIDlgProc( hwnd, message, wParam, lParam ) );
}

/* This function handles all messages for child windows.
 */
LRESULT EXPORT CALLBACK TerminalWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
     switch( message )
          {
          case WM_PASTE: {
               HGLOBAL hMem;

               OpenClipboard( hwnd );
               hMem = GetClipboardData( CF_TEXT );
               if( hMem )
                    {
                    LPSTR lpMem;
                    TERMINALWND FAR * ptwnd;
               
                    ptwnd = GetTerminalWnd( GetParent( hwnd ) );
                    lpMem = GlobalLock( hMem );
                    while( *lpMem )
                         {
                         WriteCharToConnection( hwnd, (char)*lpMem );
                         if( ptwnd->tp.fLocalEcho )
                              Terminal_WriteChar( hwnd, (char)*lpMem, FALSE );
                         ++lpMem;
                         }
                    if( ptwnd->tp.fLocalEcho )
                         {
                         Terminal_RenderLine( hwnd );
                         UpdateWindow( hwnd );
                         Terminal_SetCursor( hwnd );
                         }
                    GlobalUnlock( hMem );
                    }
               CloseClipboard();
               break;
               }

          case WM_COPY: {
               HGLOBAL hMem;

               hMem = Terminal_CopySelection( GetParent(hwnd) );
               if( hMem && OpenClipboard( hwndFrame ) )
                    {
                    EmptyClipboard();
                    SetClipboardData( CF_TEXT, hMem );
                    CloseClipboard();
                    }
               return( 0L );
               }

          case WM_GETDLGCODE:
               return( DLGC_WANTCHARS );

          HANDLE_MSG( hwnd, WM_ERASEBKGND, Terminal_OnEraseBkGnd );
          HANDLE_MSG( hwnd, WM_KEYDOWN, Terminal_OnKey );
          HANDLE_MSG( hwnd, WM_CHAR, Terminal_OnChar );
          HANDLE_MSG( hwnd, WM_RBUTTONDOWN, Terminal_OnRButtonDown );
          HANDLE_MSG( hwnd, WM_RBUTTONDBLCLK, Terminal_OnRButtonDown );
          HANDLE_MSG( hwnd, WM_LBUTTONDOWN, Terminal_OnLButtonDown );
          HANDLE_MSG( hwnd, WM_LBUTTONDBLCLK, Terminal_OnLButtonDown );
          HANDLE_MSG( hwnd, WM_HSCROLL, Terminal_OnHScroll );
          HANDLE_MSG( hwnd, WM_PAINT, Terminal_OnPaint );
          HANDLE_MSG( hwnd, WM_SETFOCUS, Terminal_OnSetFocus );
          HANDLE_MSG( hwnd, WM_KILLFOCUS, Terminal_OnKillFocus );
          HANDLE_MSG( hwnd, WM_VSCROLL, Terminal_OnVScroll );
          HANDLE_MSG( hwnd, WM_SIZE, Terminal_OnSize );
          HANDLE_MSG( hwnd, WM_NCPAINT, Terminal_OnNCPaint );

          default:
               break;
          }
     return( DefWindowProc( hwnd, message, wParam, lParam ) );
}

/* Paint the non-client portion of the terminal window.
 */
void FASTCALL Terminal_OnNCPaint( HWND hwnd, HRGN hrgn )
{
     RECT rc;
     HDC hdc;

     /* Since we're doing NC painting, client things do not
      * apply to us.
      */
     hdc = GetDC( GetParent( hwnd ) );
     GetWindowRect( hwnd, &rc );
     MapWindowPoints( NULL, GetParent( hwnd ), (LPPOINT)&rc, 2 );
     rc.right += 2;
     rc.bottom += 2;
     rc.left -= 2;
     rc.top -= 2;
     Draw3DFrame( hdc, &rc );
     ReleaseDC( GetParent( hwnd ), hdc );
     FORWARD_WM_NCPAINT( hwnd, hrgn, DefWindowProc );
}

/* This function handles erasing the terminal window
 * background.
 */
BOOL FASTCALL Terminal_OnEraseBkGnd( HWND hwnd, HDC hdc )
{
     return( TRUE );
}

void FASTCALL Terminal_OnKey( HWND hwnd, UINT vk, BOOL fDown, int cRepeat, UINT flags )
{
     if( fDown )
          switch( vk )
               {
               case VK_UP:
                    WriteStringToConnection( hwnd, "\x01B[A", 3 );
                    break;

               case VK_DOWN:
                    WriteStringToConnection( hwnd, "\x01B[B", 3 );
                    break;

               case VK_LEFT:
                    WriteStringToConnection( hwnd, "\x01B[D", 3 );
                    break;

               case VK_RIGHT:
                    WriteStringToConnection( hwnd, "\x01B[C", 3 );
                    break;

               case VK_HOME:
                    PostMessage( hwnd, WM_VSCROLL, SB_TOP, 0L );
                    break;

               case VK_NEXT:
                    PostMessage( hwnd, WM_VSCROLL, SB_PAGEDOWN, 0L );
                    break;

               case VK_PRIOR:
                    PostMessage( hwnd, WM_VSCROLL, SB_PAGEUP, 0L );
                    break;

               case VK_END:
                    PostMessage( hwnd, WM_VSCROLL, SB_BOTTOM, 0L );
                    break;
               }
}

/* This function handles the WM_MDIACTIVATE message.
 */
void FASTCALL TerminalFrame_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
     if( hwndActivate == hwnd )
          {
          TERMINALWND FAR * ptwnd;

          /* Enable or disable toolbar buttons depending on the
           * current context.
           */
          ptwnd = GetTerminalWnd( hwnd );
          if( ptwnd )
               {
               SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_COPY, ptwnd->fSelection );
               SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_SEND, ptwnd->fSelection );
               }
          }
     else
          SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_SEND, FALSE );
     Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_TERMINAL ), (LPARAM)(LPSTR)hwnd );
     FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, Adm_DefMDIDlgProc );
}

void FASTCALL Terminal_OnRButtonDown( HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags )
{
     TERMINALWND FAR * ptwnd;

     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     if( fDoubleClick )
          Terminal_OnLButtonDown( hwnd, fDoubleClick, x, y, keyFlags );
     if( ptwnd->fSelection )
          SendMessage( GetParent(hwnd), WM_COMMAND, IDM_SEND, 0L );
}

void FASTCALL Terminal_OnLButtonDown( HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags )
{
     TERMINALWND FAR * ptwnd;
     int col, row;
     int vButton;
     POINT pt;

     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     pt.x = x;
     pt.y = y;
     col = pt.x / ptwnd->nCharWidth;
     row = pt.y / ptwnd->nLineHeight;
     if( col > ptwnd->nSBQXSize || row > ptwnd->nSBQYSize )
          return;
     if( row + ptwnd->nSBQYShift >= ptwnd->tp.nBufferSize )
          return;
     if( ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) && ptwnd->fSelection )
          {
          col = ptwnd->nSBQX2Anchor;
          row = ptwnd->nSBQY2Anchor;
          }
     else {
          if( ptwnd->fSelection )
               {
               Terminal_DrawSelection( hwnd, ptwnd->nSBQX1Anchor,
                                                                 ptwnd->nSBQY1Anchor,
                                                                 ptwnd->nSBQX2Anchor,
                                                                 ptwnd->nSBQY2Anchor );
               ptwnd->fSelection = FALSE;
               }
          ptwnd->nSBQX1Anchor = col += ptwnd->nSBQXShift;
          ptwnd->nSBQY1Anchor = row += ptwnd->nSBQYShift;
          }

     if( fDoubleClick )
          {
          LPSTR lpText;

          lpText = ptwnd->lpScrollBuf + ( ptwnd->nSBQY1Anchor * ptwnd->tp.nLineWidth );
          ptwnd->nSBQX2Anchor = ptwnd->nSBQX1Anchor;
          while( ptwnd->nSBQX1Anchor && lpText[ ptwnd->nSBQX1Anchor ] == ' ' )
               --ptwnd->nSBQX1Anchor;
          while( ptwnd->nSBQX1Anchor > 0 && lpText[ ptwnd->nSBQX1Anchor - 1 ] != ' ' )
               --ptwnd->nSBQX1Anchor;
          while( ptwnd->nSBQX2Anchor < ptwnd->tp.nLineWidth && lpText[ ptwnd->nSBQX2Anchor ] != ' ' )
               ++ptwnd->nSBQX2Anchor;
          ptwnd->nSBQY2Anchor = ptwnd->nSBQY1Anchor;
          ptwnd->fSelection = TRUE;
          Terminal_DrawSelection( hwnd, ptwnd->nSBQX1Anchor,
                                                            ptwnd->nSBQY1Anchor,
                                                            ptwnd->nSBQX2Anchor,
                                                            ptwnd->nSBQY2Anchor );
          return;
          }

     if( ptwnd->fWndHasCaret )
          HideCaret( hwnd );
     SetCapture( hwnd );
     vButton = GetSystemMetrics( SM_SWAPBUTTON ) ? VK_RBUTTON : VK_LBUTTON;
     while( GetAsyncKeyState( vButton ) & 0x8000 )
          {
          int colTmp;
          int rowTmp;

          GetCursorPos( &pt );
          ScreenToClient( hwnd, &pt );
          colTmp = pt.x / ptwnd->nCharWidth;
          rowTmp = pt.y / ptwnd->nLineHeight;
          if( colTmp < 0 )
               colTmp = 0;
          if( rowTmp < 0 )
               rowTmp = 0;
          if( colTmp >= ptwnd->nSBQXSize )
               colTmp = ptwnd->nSBQXSize - 1;
          if( rowTmp >= ptwnd->nSBQYSize )
               rowTmp = ptwnd->nSBQYSize - 1;
          colTmp += ptwnd->nSBQXShift;
          rowTmp += ptwnd->nSBQYShift;
          if( colTmp != col || rowTmp != row )
               {
               int x1, y1;
               int x2, y2;

               x1 = col; x2 = colTmp;
               y1 = row; y2 = rowTmp;
               if( y2 < y1 ) {
                    register int n;

                    n = y1; y1 = y2; y2 = n;
                    n = x1; x1 = x2; x2 = n;
                    }
               Terminal_DrawSelection( hwnd, x1, y1, x2, y2 );
               col = colTmp;
               row = rowTmp;
               }
          }

     ptwnd->nSBQX2Anchor = col;
     ptwnd->nSBQY2Anchor = row;
     if( ptwnd->nSBQX2Anchor != ptwnd->nSBQX1Anchor || ptwnd->nSBQY2Anchor != ptwnd->nSBQY1Anchor )
          ptwnd->fSelection = TRUE;
     ReleaseCapture();
     if( ptwnd->fWndHasCaret )
          ShowCaret( hwnd );
}

/* This function updates the selection in the terminal window.
 */
void FASTCALL Terminal_DrawSelection( HWND hwnd, int x1, int y1, int x2, int y2 )
{
     TERMINALWND FAR * ptwnd;
     RECT rc;
     HDC hdc;

     /* Get this window's terminal record.
      */
     ptwnd = GetTerminalWnd( GetParent(hwnd) );

     /* Swap, if necessary */
     if( y2 < y1 ) {
          register int n;

          n = y1;
          y1 = y2;
          y2 = n;
          n = x1;
          x1 = x2;
          x2 = n;
          }

     /* Convert logical coords to physical
      */
     y1 -= ptwnd->nSBQYShift;
     y2 -= ptwnd->nSBQYShift;
     x1 -= ptwnd->nSBQXShift;
     x2 -= ptwnd->nSBQXShift;

     /* Constrain coordinates
      */
     if( y1 < 0 )                            y1 = 0;
     if( y2 < 0 )                            return;
     if( y1 >= ptwnd->nSBQYSize )  return;
     if( x1 < 0 )                            x1 = 0;
     if( x2 >= ptwnd->nSBQXSize )  x2 = ptwnd->nSBQXSize - 1;

     hdc = GetDC( hwnd );
     while( y1 < y2 )
          {
          if( y1 >= ptwnd->nSBQYSize )
               break;
          SetRect( &rc, x1 * ptwnd->nCharWidth,
                                y1 * ptwnd->nLineHeight,
                                ( ptwnd->nSBQXSize - 1 ) * ptwnd->nCharWidth,
                                ( y1 + 1 ) * ptwnd->nLineHeight );
          InvertRect( hdc, &rc );
          x1 = 0;
          ++y1;
          }
     if( y1 == y2 )
          {
          SetRect( &rc, x1 * ptwnd->nCharWidth,
                                y1 * ptwnd->nLineHeight,
                                x2 * ptwnd->nCharWidth,
                                ( y1 + 1 ) * ptwnd->nLineHeight );
          InvertRect( hdc, &rc );
          }
     ReleaseDC( hwnd, hdc );
}

HGLOBAL FASTCALL Terminal_CopySelection( HWND hwnd )
{
     TERMINALWND FAR * ptwnd;

     MUST_BE_FRAME_WND(hwnd);
     ptwnd = GetTerminalWnd( hwnd );
     if( ptwnd->fSelection )
          {
          LPSTR lpText;
          LPSTR lpMem;
          WORD wCopySize;
          HGLOBAL hg;
          register int y1;
          register int y2;
          register int x1;
          register int x2;

          /* Get the coordinates
           */
          x1 = ptwnd->nSBQX1Anchor;
          x2 = ptwnd->nSBQX2Anchor;
          y1 = ptwnd->nSBQY1Anchor;
          y2 = ptwnd->nSBQY2Anchor;

          /* Swap, if necessary
           */
          if( y2 < y1 || ( y2 == y1 && x2 < x1 ) ) {
               register int n;
     
               n = y1;
               y1 = y2;
               y2 = n;
               n = x1;
               x1 = x2;
               x2 = n;
               }

          /* Compute the size of the block of memory needed
           */
          wCopySize = ( ( y2 - y1 ) + 1 ) * ( ptwnd->nSBQXSize + 2 );
          if( ( hg = GlobalAlloc( GHND, (DWORD)wCopySize ) ) == NULL )
               return( NULL );
          if( ( lpMem = GlobalLock( hg ) ) == NULL )
               return( NULL );
          lpText = ptwnd->lpScrollBuf + ( y1 * ptwnd->tp.nLineWidth );
          for( ; y1 <= y2; ++y1 )
               {
               BOOL fLastLine;
               register int n;
               int ex2;

               fLastLine = y1 == y2;
               ex2 = fLastLine ? x2 : ptwnd->nSBQXSize - 1;
               for( n = ex2 - 1; n > x1; --n )
                    if( lpText[ n ] != ' ' )
                         break;
               if( n >= x1 ) {
                    _fmemcpy( lpMem, lpText + x1, ( n - x1 ) + 1 );
                    lpMem += ( n - x1 ) + 1;
                    }
               lpText += ptwnd->tp.nLineWidth;
               if( !fLastLine ) {
                    *lpMem++ = CR;
                    *lpMem++ = LF;
                    }
               x1 = 0;
               }
          *lpMem = '\0';
          GlobalUnlock( hg );
          return( hg );
          }
     return( NULL );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL TerminalFrame_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
     TERMINALWND FAR * ptwnd;

     ptwnd = GetTerminalWnd( hwnd );
     switch( id )
          {
          case IDM_PROPERTIES:
               Terminal_Properties( hwndFrame, ptwnd );
               break;

          case IDM_QUICKPRINT:
          case IDM_PRINT:
               CmdTerminalPrint( hwnd );
               break;
 
          case IDM_CLEARBUF:
               Terminal_WriteChar( ptwnd->hwndTerm, 12, FALSE );
               Terminal_SetCursor( ptwnd->hwndTerm );
               break;

          case IDM_ZMODEMSEND:
               if( NULL != ptwnd->lpcdev )
                    if( NULL != ( ptwnd->hxfer = GetFileTransferProtocolHandle( "ZMODEM" ) ) )
                         {
                         char sz[ _MAX_PATH ];
                         OPENFILENAME ofn;
                         static char strFilters[] = "Archive Files (*.zip;*.lzh;*.arc)\0*.zip;*.lzh;*.arc\0All Files (*.*)\0*.*\0\0";
          
                         fZModemUploadActive = TRUE;
                         strcpy( sz, pszUploadDir );
                         ofn.lStructSize = sizeof( OPENFILENAME );
                         ofn.hwndOwner = hwnd;
                         ofn.hInstance = NULL;
                         ofn.lpstrFilter = strFilters;
                         ofn.lpstrCustomFilter = NULL;
                         ofn.nMaxCustFilter = 0;
                         ofn.nFilterIndex = 1;
                         ofn.lpstrFile = sz;
                         ofn.nMaxFile = sizeof( sz );
                         ofn.lpstrFileTitle = NULL;
                         ofn.nMaxFileTitle = 0;
                         ofn.lpstrInitialDir = pszUploadDir;
                         ofn.lpstrTitle = "Binary Send";
                         ofn.Flags = OFN_NOCHANGEDIR|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST;
                         ofn.nFileOffset = 0;
                         ofn.nFileExtension = 0;
                         ofn.lpstrDefExt = NULL;
                         ofn.lCustData = 0;
                         ofn.lpfnHook = NULL;
                         ofn.lpTemplateName = 0;
                         if( GetOpenFileName( &ofn ) )
                              {
                              EnableDisableFXmitButtons( hwnd, FALSE );
                              KillSweepTimer();
                              fZModemUploadActive = FALSE;
                              SendFileViaProtocol( ptwnd->hxfer, ptwnd->lpcdev, FileTransferStatus, hwnd, sz, FALSE );
                              SetSweepTimer();
                              EnableDisableFXmitButtons( hwnd, TRUE );
                              }
                         else
                              CancelTransfer( ptwnd->hxfer, ptwnd->lpcdev );
                         }
               break;

          case IDM_ZMODEMRECEIVE:
               if( NULL != ptwnd->lpcdev )
                    if( NULL != ( ptwnd->hxfer = GetFileTransferProtocolHandle( "ZMODEM" ) ) )
                         {
                         EnableDisableFXmitButtons( hwnd, FALSE );
                         KillSweepTimer();
                         ReceiveFileViaProtocol( ptwnd->hxfer, ptwnd->lpcdev, FileTransferStatus, hwnd, NULL, FALSE, ZDF_PROMPT );
                         SetSweepTimer();
                         EnableDisableFXmitButtons( hwnd, TRUE );
                         }
               break;

#ifdef DOKERMIT
          case IDM_KERMITSEND:
               if( NULL != ptwnd->lpcdev )
                    if( NULL != ( ptwnd->hxfer = GetFileTransferProtocolHandle( "KERMIT" ) ) )
                         {
                         char sz[ _MAX_PATH ];
                         OPENFILENAME ofn;
                         static char strFilters[] = "Archive Files (*.zip;*.lzh;*.arc)\0*.zip;*.lzh;*.arc\0All Files (*.*)\0*.*\0\0";
          
                         fZModemUploadActive = TRUE;
                         strcpy( sz, pszUploadDir );
                         ofn.lStructSize = sizeof( OPENFILENAME );
                         ofn.hwndOwner = hwnd;
                         ofn.hInstance = NULL;
                         ofn.lpstrFilter = strFilters;
                         ofn.lpstrCustomFilter = NULL;
                         ofn.nMaxCustFilter = 0;
                         ofn.nFilterIndex = 1;
                         ofn.lpstrFile = sz;
                         ofn.nMaxFile = sizeof( sz );
                         ofn.lpstrFileTitle = NULL;
                         ofn.nMaxFileTitle = 0;
                         ofn.lpstrInitialDir = pszUploadDir;
                         ofn.lpstrTitle = "Binary Send";
                         ofn.Flags = OFN_NOCHANGEDIR|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST;
                         ofn.nFileOffset = 0;
                         ofn.nFileExtension = 0;
                         ofn.lpstrDefExt = NULL;
                         ofn.lCustData = 0;
                         ofn.lpfnHook = NULL;
                         ofn.lpTemplateName = 0;
                         if( GetOpenFileName( &ofn ) )
                              {
                              EnableDisableFXmitButtons( hwnd, FALSE );
                              KillSweepTimer();
                              fZModemUploadActive = FALSE;
                              SendFileViaProtocol( ptwnd->hxfer, ptwnd->lpcdev, FileTransferStatus, hwnd, sz, FALSE );
                              SetSweepTimer();
                              EnableDisableFXmitButtons( hwnd, TRUE );
                              }
                         else
                              CancelTransfer( ptwnd->hxfer, ptwnd->lpcdev );
                         }
               break;

          case IDM_KERMITRECEIVE:
               if( NULL != ptwnd->lpcdev )
                    if( NULL != ( ptwnd->hxfer = GetFileTransferProtocolHandle( "KERMIT" ) ) )
                         {
                         EnableDisableFXmitButtons( hwnd, FALSE );
                         KillSweepTimer();
                         ReceiveFileViaProtocol( ptwnd->hxfer, ptwnd->lpcdev, FileTransferStatus, hwnd, NULL, FALSE, ZDF_PROMPT );
                         SetSweepTimer();
                         EnableDisableFXmitButtons( hwnd, TRUE );
                         }
               break;
#endif
          case IDM_DOWNLOAD: {
               HMENU hPopupMenu;
               RECT rc;

               /* If the file upload popup menu only has one entry then
                * do an immediate download.
                */
               GetWindowRect( GetDlgItem( hwnd, id ), &rc );
               hPopupMenu = GetSubMenu( hPopupMenus, IPOP_DOWNLOAD );
               if( GetMenuItemCount( hPopupMenu ) < 2 )
                    PostCommand( hwnd, IDM_ZMODEMRECEIVE, 0L );
               else
                    TrackPopupMenu( hPopupMenu, TPM_LEFTALIGN, rc.left, rc.bottom, 0, hwnd, NULL );
               break;
               }              

          case IDM_UPLOAD: {
               HMENU hPopupMenu;
               RECT rc;

               /* If the file upload popup menu only has one entry then
                * do an immediate ZMODEM upload.
                */
               GetWindowRect( GetDlgItem( hwnd, id ), &rc );
               hPopupMenu = GetSubMenu( hPopupMenus, IPOP_UPLOAD );
               if( GetMenuItemCount( hPopupMenu ) < 2 )
                    PostCommand( hwnd, IDM_ZMODEMSEND, 0L );
               else
                    TrackPopupMenu( hPopupMenu, TPM_LEFTALIGN, rc.left, rc.bottom, 0, hwnd, NULL );
               break;
               }              

          case IDM_PAUSERESUMELOG:
               if( ptwnd->lpLogFile->fPaused )
                    ResumeTerminalLog( hwnd );
               else
                    PauseTerminalLog( hwnd );
               break;

          case IDM_OPENCLOSELOG:
               if( ptwnd->lpLogFile == NULL ) {
                    char sz[ _MAX_PATH ];
                    OPENFILENAME ofn;
                    static char strFilters[] = "Log Files (*.log)\0*.log\0All Files (*.*)\0*.*\0\0";

                    strcpy( sz, szTerminalLog );
                    ofn.lStructSize = sizeof( OPENFILENAME );
                    ofn.hwndOwner = hwnd;
                    ofn.hInstance = NULL;
                    ofn.lpstrFilter = strFilters;
                    ofn.lpstrCustomFilter = NULL;
                    ofn.nMaxCustFilter = 0;
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFile = sz;
                    ofn.nMaxFile = sizeof( sz );
                    ofn.lpstrFileTitle = NULL;
                    ofn.nMaxFileTitle = 0;
                    ofn.lpstrInitialDir = NULL;
                    ofn.lpstrTitle = "Open Log";
                    ofn.Flags = OFN_OVERWRITEPROMPT|OFN_CREATEPROMPT|OFN_NOCHANGEDIR|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST;
                    ofn.nFileOffset = 0;
                    ofn.nFileExtension = 0;
                    ofn.lpstrDefExt = NULL;
                    ofn.lCustData = 0;
                    ofn.lpfnHook = NULL;
                    ofn.lpTemplateName = 0;
                    if( GetSaveFileName( &ofn ) )
                         OpenTerminalLog( hwnd, sz );
                    }
               else
                    CloseTerminalLog( hwnd );
               break;

          case IDM_SELECTALL:
               ptwnd->nSBQX1Anchor = 0;
               ptwnd->nSBQY1Anchor = 0;
               ptwnd->nSBQX2Anchor = ptwnd->nSBQXCaret;
               ptwnd->nSBQY2Anchor = ptwnd->nSBQYCaret;
               ptwnd->fSelection = TRUE;
               InvalidateRect( hwnd, NULL, FALSE );
               UpdateWindow( hwnd );
               break;

          case IDM_SEND: {
               HGLOBAL hMem;

               hMem = Terminal_CopySelection( hwnd );
               if( hMem ) {
                    LPSTR lpMem;

                    lpMem = GlobalLock( hMem );
                    while( *lpMem )
                         {
                         WriteCharToConnection( ptwnd->hwndTerm, *lpMem );
                         if( ptwnd->tp.fLocalEcho )
                              Terminal_WriteChar( ptwnd->hwndTerm, (char)*lpMem, FALSE );
                         ++lpMem;
                         }
                    GlobalUnlock( hMem );
                    GlobalFree( hMem );
                    }
               break;
               }
          }
}

/* This function enables or disables the Download and Upload
 * buttons.
 */
void FASTCALL EnableDisableFXmitButtons( HWND hwnd, BOOL fState )
{
     EnableControl( hwnd, IDM_DOWNLOAD, fState );
     EnableControl( hwnd, IDM_UPLOAD, fState );
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL TerminalFrame_OnCreate( HWND hwnd, CREATESTRUCT FAR * lpCreateStruct )
{
     HGLOBAL hScrollBuf;
     TERMINALWND FAR * ptwnd;

     /* Allocate a data structure for this window.
      */
     INITIALISE_PTR(ptwnd);
     if( fNewMemory( &ptwnd, sizeof(TERMINALWND) ) )
          {
          LPMDICREATESTRUCT lpMDICreateStruct;
          LPSTR lpszName;

          /* Dereference the CONNECT structure.
           */
          lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
          lpszName = (LPSTR)lpMDICreateStruct->lParam;
          PutTerminalWnd( hwnd, ptwnd );
          ptwnd->lpcdev = NULL;
          ptwnd->hxfer = NULL;
          ptwnd->hwndTerm = NULL;

          /* Create the background window.
           */
          Adm_MDIDialog( hwnd, MAKEINTRESOURCE(IDDLG_TERMINAL), lpMDICreateStruct );
          ptwnd->hwndTerm = GetDlgItem( hwnd, IDD_TERMINAL );
          if( NULL == ptwnd->hwndTerm )
               return( FALSE );

          /* Set the default foreground and background colours
           */
          B4ToRGBTable[ 0 ] = GetSysColor( COLOR_WINDOWTEXT );
          B4ToRGBTable[ 7 ] = GetSysColor( COLOR_WINDOW );
          INITIALISE_PTR(ptwnd->tp.lpcrBgMapTable);
          INITIALISE_PTR(ptwnd->tp.lpcrFgMapTable);

          /* Disable the Pause/Resume log commands.
           */
          EnableControl( hwnd, IDM_PAUSERESUMELOG, FALSE );

          /* Now read the properties for this connection.
           */
          Terminal_ReadProperties( lpszName, ptwnd );

          /* Initialise other flags used by the window.
           */
          ptwnd->fHasCaret = FALSE;
          ptwnd->fWndHasCaret = FALSE;
          ptwnd->fOriginMode = FALSE;
          ptwnd->fSelection = FALSE;
          ptwnd->lpLogFile = NULL;
          ptwnd->nMode = 0;
          ptwnd->nWriteState = 0;
          ptwnd->fReverseOn = FALSE;
          ptwnd->fCurrentLineRendered = FALSE;
          ptwnd->hwnd = hwnd;

          /* Create a scroll buffer */
          hScrollBuf = GlobalAlloc( GHND, (WORD)ptwnd->tp.nBufferSize * (WORD)ptwnd->tp.nLineWidth );
          if( !hScrollBuf )
               return( FALSE );
          ptwnd->lpScrollBuf = GlobalLock( hScrollBuf );
          if( !ptwnd->lpScrollBuf )
               return( FALSE );
          memset( ptwnd->lpScrollBuf, ' ', ptwnd->tp.nBufferSize * ptwnd->tp.nLineWidth );
     
          /* If ANSI mode, create a colour buffer */
          ptwnd->crAttrByte = 0;
          SetFgAttr( ptwnd->crAttrByte, FgRGBToB4( ptwnd, ptwnd->tp.crTermText ) );
          SetBgAttr( ptwnd->crAttrByte, BgRGBToB4( ptwnd, ptwnd->tp.crTermBack ) );
          ptwnd->crRealAttrByte = ptwnd->crAttrByte;
          if( ptwnd->tp.fAnsi )
               {
               HGLOBAL hColourBuf;
     
               hColourBuf = GlobalAlloc( GHND, (WORD)ptwnd->tp.nBufferSize * (WORD)ptwnd->tp.nLineWidth );
               if( !hColourBuf )
                    return( FALSE );
               ptwnd->lpColourBuf = GlobalLock( hColourBuf );
               if( ptwnd->lpColourBuf )
                    memset( ptwnd->lpColourBuf, ptwnd->crAttrByte, ptwnd->tp.nBufferSize * ptwnd->tp.nLineWidth );
               }
          else
               ptwnd->lpColourBuf = NULL;
     
          /* Initialise pointers, etc */
          ptwnd->nSBQX = 0;
          ptwnd->nSBQY = 0;
          ptwnd->nSBQYTop = 0;
          ptwnd->nSBQYBottom = 0;
          ptwnd->nSBQXSize = 0;
          ptwnd->nSBQYSize = 0;
          ptwnd->nSBQXShift = 0;
          ptwnd->nSBQYShift = 0;
          ptwnd->nSBQYExtent = 0;
          ptwnd->nSBQXExtent = 0;
          ptwnd->nSBQXCaret = 0;
          ptwnd->nSBQYCaret = 0;
     
          /* Compute the character cell dimensions */
          Terminal_ComputeCellDimensions( ptwnd->hwndTerm );
          return( TRUE );
          }
     return( FALSE );
}

/* This function is called by the file transfer functions to report
 * status information.
 */
int EXPORT CALLBACK FileTransferStatus( LPFILETRANSFERINFO lpftinfo )
{
     TERMINALWND FAR * ptwnd;

     /* Get the handle of the terminal structure.
      */
     MUST_BE_FRAME_WND(lpftinfo->hwndOwner);
     ptwnd = GetTerminalWnd( lpftinfo->hwndOwner );

     /* Determine how to handle the status bar gauge depending on what
      * we're actually doing at this stage.
      */
     switch( lpftinfo->wStatus )
          {
          case FTST_YIELD:
               TaskYield();
               break;

          case FTST_QUERYEXISTING: {
               LPSTR lpStr;
               int cStrLen;
               WORD wFlags;

               /* Make a copy of the path before we work
                * on it.
                */
               cStrLen = strlen( lpftinfo->szFilename ) + 1;
               lpStr = NULL;
               if( !fNewMemory( &lpStr, cStrLen ) )
                    wFlags = ZDF_DEFAULT;
               else
                    {
                    HDC hdc;

                    /* Shrink the string to fit the dialog.
                     */
                    strcpy( lpStr, lpftinfo->szFilename );
                    _strupr( lpStr );
                    hdc = GetDC( hwndFrame );
                    FitPathString( hdc, lpStr, 300 );
                    ReleaseDC( hwndFrame, hdc );
                    switch( fDlgMessageBox( GetFocus(), idsFDLPROMPT, IDDLG_FDLPROMPT, lpStr, 15000, IDD_RENAME ) )
                         {
                         case IDD_SKIP:           wFlags = ZDF_DEFAULT; break;
                         case IDD_OVERWRITE: wFlags = ZDF_OVERWRITE; break;
                         case IDD_RENAME:         wFlags = ZDF_RENAME; break;
                         default:                      wFlags = 0; break;
                         }
                    FreeMemory( &lpStr );
                    }
               return( wFlags );
               }

          case FTST_BEGINTRANSMIT:
          case FTST_BEGINRECEIVE:
               if( NULL == hwndZStatus )
                    {
                    char * pszStatus;
                    HWND hwndGauge;

                    /* Display the status dialog and reset the gauge.
                     */
                    hwndZStatus = Adm_CreateDialog( hRscLib, ptwnd->hwndTerm, MAKEINTRESOURCE(IDDLG_ZMODEMSTATUS), ZStatus, (LPARAM)ptwnd );
                    VERIFY( hwndGauge = GetDlgItem( hwndZStatus, IDD_GAUGE ) );
                    SendMessage( hwndGauge, PBM_SETRANGE, 0, MAKELPARAM( 0, 100 ) );
                    SendMessage( hwndGauge, PBM_SETPOS, 0, 0L );
                    SetDlgItemText( hwndZStatus, IDD_PERCENTAGE, "0%" );

                    /* Set the title.
                     */
                    pszStatus = ( lpftinfo->wStatus == FTST_BEGINTRANSMIT ) ? "ZModem Send" : "ZModem Receive";
                    SetWindowText( hwndZStatus, pszStatus );
                    }
               break;

          case FTST_UPDATE:
               if( NULL != hwndZStatus )
                    {
                    char szTotalSize[ 40 ];
                    HWND hwndGauge;
                    char sz[ 10 ];

                    /* Fill out the statistics stuff.
                     */
                    FormatThousands( lpftinfo->dwTotalSize, szTotalSize ),
                    SetDlgItemText( hwndZStatus, IDD_FILENAME, GetFileBasename(lpftinfo->szFilename) );
                    SetDlgItemText( hwndZStatus, IDD_FILESIZE, szTotalSize );
                    SetDlgItemInt( hwndZStatus, IDD_CPS, lpftinfo->cps, FALSE );
                    SetDlgItemInt( hwndZStatus, IDD_ERRORCOUNT, lpftinfo->cErrors, FALSE );
                    SetDlgItemText( hwndZStatus, IDD_DEBUGMSG, lpftinfo->szLastMessage );

                    /* Show time remaining and elapsed.
                     */
                    SetDlgItemText( hwndZStatus, IDD_TIMEREMAINING, Amdate_FormatLongTime( lpftinfo->dwTimeRemaining ) );
                    SetDlgItemText( hwndZStatus, IDD_TRANSFERTIME, Amdate_FormatLongTime( lpftinfo->dwTimeSoFar ) );

                    /* Update the status gauge.
                     */
                    hwndGauge = GetDlgItem( hwndZStatus, IDD_GAUGE );
                    SendMessage( hwndGauge, PBM_SETPOS, lpftinfo->wPercentage, 0L );
                    wsprintf( sz, "%u%%", lpftinfo->wPercentage );
                    SetDlgItemText( hwndZStatus, IDD_PERCENTAGE, sz );
                    }
               break;

          case FTST_COMPLETED:
          case FTST_FAILED:
               if( NULL != hwndZStatus )
                    {
                    DestroyWindow( hwndZStatus );
                    hwndZStatus = NULL;
                    }
               break;
          }
     return( 0 );
}

/* This function loads the properties record of the specified terminal record
 * from the configuration file.
 */
BOOL FASTCALL Terminal_ReadProperties( LPSTR lpszConnectName, TERMINALWND FAR * ptwnd )
{
     int nMaxBufferSize;
     char szSection[ LEN_TERMNAME+20+1 ];
     register int c;

     /* Derive the configuration section for this connection.
      */
     wsprintf( szSection, "%s Properties", lpszConnectName );
     strcpy( ptwnd->szName, lpszConnectName );

     /* Load the properties for this window from the
      * configuration file.
      */
     ptwnd->tp.fNewline = Amuser_GetPPInt( szSection, "Newline", FALSE );
     ptwnd->tp.fOutCRtoCRLF = Amuser_GetPPInt( szSection, "OutCRtoCRLF", FALSE );
     ptwnd->tp.fLocalEcho = Amuser_GetPPInt( szSection, "LocalEcho", FALSE );
     ptwnd->tp.fShowNegotiation = Amuser_GetPPInt( szSection, "ShowNegotiations", FALSE );
     ptwnd->tp.fTelnetBinary = Amuser_GetPPInt( szSection, "TelnetBinary", TRUE );
     ptwnd->tp.fAutoWrap = Amuser_GetPPInt( szSection, "AutoWrap", TRUE );
     ptwnd->tp.fSound = Amuser_GetPPInt( szSection, "Sound", FALSE );
     ptwnd->tp.fStripHigh = Amuser_GetPPInt( szSection, "StripHigh", TRUE );
     ptwnd->tp.fAnsi = Amuser_GetPPInt( szSection, "Ansi", TRUE );
     ptwnd->tp.nBufferSize = Amuser_GetPPInt( szSection, "BufferSize", DEF_BUFFERSIZE );
     ptwnd->tp.nLineWidth = Amuser_GetPPInt( szSection, "LineWidth", 80 );
     ptwnd->tp.crTermText = Amuser_GetPPLong( szSection, "TextColor", crTermText );
     ptwnd->tp.crTermBack = Amuser_GetPPLong( szSection, "BackColor", crTermWnd );
     Amuser_GetPPString( szSection, "ConnectionCard", "", ptwnd->tp.szConnCard, LEN_CONNCARD );

     /* Read colour mappings.
      */
     if( NULL != ptwnd->tp.lpcrBgMapTable )
          FreeMemory( &ptwnd->tp.lpcrBgMapTable );
     if( fNewMemory( &ptwnd->tp.lpcrBgMapTable, sizeof( COLORREF ) * 8 ) )
          for( c = 0; c < 8; ++c )
               {
               char sz[ 20 ];

               wsprintf( sz, "BackColourMap%u", c );
               ptwnd->tp.lpcrBgMapTable[ c ] = Amuser_GetPPLong( szSection, sz, B4ToRGBTable[ c ] );
               }
     if( NULL != ptwnd->tp.lpcrFgMapTable )
          FreeMemory( &ptwnd->tp.lpcrFgMapTable );
     if( fNewMemory( &ptwnd->tp.lpcrFgMapTable, sizeof( COLORREF ) * 8 ) )
          for( c = 0; c < 8; ++c )
               {
               char sz[ 20 ];

               wsprintf( sz, "ForeColourMap%u", c );
               ptwnd->tp.lpcrFgMapTable[ c ] = Amuser_GetPPLong( szSection, sz, B4ToRGBTable[ c ] );
               }

     /* Create a copy of the terminal font
      */
     Terminal_SetDefaultFont( ptwnd, szSection );

     /* Sanity check the line width to ensure that it isn't
      * outside the legal range
      */
     if( ptwnd->tp.nLineWidth < 80 )
          ptwnd->tp.nLineWidth = 80;
     else if( ptwnd->tp.nLineWidth > 132 )
          ptwnd->tp.nLineWidth = 132;
     nMaxBufferSize = ( ptwnd->tp.nLineWidth == 80 ) ? 800 : 480;
     
     /* Sanity check the buffer size to ensure that it isn't
      * outside the legal range
      */
     if( ptwnd->tp.nBufferSize < 10 )
          ptwnd->tp.nBufferSize = 10;
     else if( ptwnd->tp.nBufferSize > nMaxBufferSize )
          ptwnd->tp.nBufferSize = nMaxBufferSize;
     return( TRUE );
}

void FASTCALL Terminal_ResizeBuffer( HWND hwnd )
{
     HGLOBAL hScrollBuf;
     HGLOBAL hColourBuf;
     TERMINALWND FAR * ptwnd;

     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     hScrollBuf = GlobalPtrHandle( ptwnd->lpScrollBuf );
     GlobalFree( hScrollBuf );
     if( ptwnd->lpColourBuf )
          {
          hColourBuf = GlobalPtrHandle( ptwnd->lpColourBuf );
          GlobalFree( hColourBuf );
          }
     hScrollBuf = GlobalAlloc( GHND, (WORD)ptwnd->tp.nBufferSize * (WORD)ptwnd->tp.nLineWidth );
     if( !hScrollBuf )
          return;
     ptwnd->lpScrollBuf = GlobalLock( hScrollBuf );
     if( ptwnd->lpScrollBuf )
          {
          RECT rc;
          int state;

          if( ptwnd->tp.fAnsi )
               {
               hColourBuf = GlobalAlloc( GHND, (WORD)ptwnd->tp.nBufferSize * (WORD)ptwnd->tp.nLineWidth );
               if( !hColourBuf )
                    return;
               ptwnd->lpColourBuf = GlobalLock( hColourBuf );
               if( ptwnd->lpColourBuf )
                    memset( ptwnd->lpColourBuf, ptwnd->crAttrByte, ptwnd->tp.nBufferSize * ptwnd->tp.nLineWidth );
               }
          else
               ptwnd->lpColourBuf = NULL;
          memset( ptwnd->lpScrollBuf, ' ', ptwnd->tp.nBufferSize * ptwnd->tp.nLineWidth );
          ptwnd->nSBQX = ptwnd->nSBQY = 0;
          ptwnd->nSBQYTop = ptwnd->nSBQYBottom = 0;
          ptwnd->nSBQXSize = ptwnd->nSBQYSize = 0;
          ptwnd->nSBQXShift = ptwnd->nSBQYShift = 0;
          ptwnd->nSBQYExtent = 0;
          ptwnd->nSBQXExtent = 0;
          ptwnd->nSBQXCaret = 0;
          ptwnd->nSBQYCaret = 0;
          Terminal_SetScrollRange( ptwnd->hwndTerm, SB_HORZ, 0, 0 );
          Terminal_SetScrollRange( ptwnd->hwndTerm, SB_VERT, 0, 0 );
          TextBltToImageBitmap( ptwnd->hwndTerm, NULL );
          Terminal_SetCursor( ptwnd->hwndTerm );
          state = IsZoomed( ptwnd->hwnd ) ? SIZE_MAXIMIZED : IsIconic( ptwnd->hwnd ) ? SIZE_MINIMIZED : SIZE_RESTORED;
          GetClientRect( ptwnd->hwnd, &rc );
          FORWARD_WM_SIZE( ptwnd->hwndTerm, state, rc.right - rc.left, rc.bottom - rc.top, TerminalWndProc );
          }
     else
          TerminalFrame_OnClose( ptwnd->hwndTerm );
}

/* This function sets or removes ANSI support from the terminal session.
 */
BOOL FASTCALL Terminal_SetAnsi( HWND hwnd, BOOL fNewAnsi )
{
     HGLOBAL hColourBuf;
     TERMINALWND FAR * ptwnd;

     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     if( !fNewAnsi && ptwnd->tp.fAnsi && ptwnd->lpColourBuf )
          {
          RECT rc;

          hColourBuf = GlobalPtrHandle( ptwnd->lpColourBuf );
          GlobalFree( hColourBuf );
          ptwnd->lpColourBuf = NULL;
          GetClientRect( ptwnd->hwnd, &rc );
          ptwnd->nSBQYSize = ( rc.bottom - rc.top ) / ptwnd->nLineHeight;
          ptwnd->nSBQXSize = ( rc.right - rc.left ) / ptwnd->nCharWidth;

          ptwnd->nSBQYTop = 0;
          ptwnd->nSBQYBottom = ptwnd->nSBQYSize - 1;
          }
     if( fNewAnsi && !ptwnd->tp.fAnsi )
          {
          RECT rc;

          hColourBuf = GlobalAlloc( GHND, (WORD)ptwnd->tp.nBufferSize * (WORD)ptwnd->tp.nLineWidth );
          if( !hColourBuf )
               return( FALSE );
          ptwnd->lpColourBuf = GlobalLock( hColourBuf );
          if( ptwnd->lpColourBuf )
               {
               memset( ptwnd->lpColourBuf, ptwnd->crAttrByte, ptwnd->tp.nBufferSize * ptwnd->tp.nLineWidth );
               ptwnd->crAttrByte = 0;
               SetFgAttr( ptwnd->crAttrByte, FgRGBToB4( ptwnd, ptwnd->tp.crTermText ) );
               SetBgAttr( ptwnd->crAttrByte, BgRGBToB4( ptwnd, ptwnd->tp.crTermBack ) );
               ptwnd->crRealAttrByte = ptwnd->crAttrByte;
               }
          GetClientRect( hwnd, &rc );
          ptwnd->nSBQYSize = ( rc.bottom - rc.top ) / ptwnd->nLineHeight;
          ptwnd->nSBQXSize = min( ( rc.right - rc.left ) / ptwnd->nCharWidth, ptwnd->tp.nLineWidth );

          ptwnd->nSBQYTop = 0;
          ptwnd->nSBQYBottom = ptwnd->nSBQYSize - 1;

          /* May need to scroll image up to keep cursor in view */
          if( ptwnd->nSBQY >= ptwnd->nSBQYSize )
               {
               ptwnd->nSBQYShift += ( ptwnd->nSBQY - ptwnd->nSBQYSize ) + 1;
               if( ptwnd->nSBQYShift > ptwnd->nSBQYExtent )
                    ptwnd->nSBQYExtent = ptwnd->nSBQYShift;
               ptwnd->nSBQY = ptwnd->nSBQYSize - 1;
     
               /* Update the vertical scroll range */
               Terminal_SetScrollRange( hwnd, SB_VERT, 0, ptwnd->nSBQYExtent );
               SetScrollPos( hwnd, SB_VERT, ptwnd->nSBQYShift, TRUE );
               }
          }
     return( TRUE );
}

/* Attach an open comms device to the terminal.
 */
BOOL FASTCALL Terminal_AttachConnection( HWND hwnd, LPCOMMDEVICE lpcdev )
{
     TERMINALWND FAR * ptwnd;

     /* Open the communications device.
      */
     MUST_BE_FRAME_WND(hwnd);
     ptwnd = GetTerminalWnd( hwnd );
     if( NULL == ptwnd->lpcdev )
          {
          ptwnd->lpcdev = lpcdev;
          ptwnd->fAttached = TRUE;
          }
     return( TRUE );
}

/* Open the communications device.
 */
BOOL FASTCALL Terminal_CreateConnection( HWND hwnd, LPSTR lpszName, BOOL fConnect )
{
     TERMINALWND FAR * ptwnd;

     /* Open the communications device.
      */
     MUST_BE_FRAME_WND(hwnd);
     ptwnd = GetTerminalWnd( hwnd );
     ptwnd->lpcdev = NULL;
     ptwnd->fAttached = FALSE;
     fInitialisingTerminal = TRUE;
     if( !fConnect )
          {
          LPCOMMDESCRIPTOR lpcd;

          INITIALISE_PTR(lpcd);

          /* Get the specified connection card.
           */
          if( !fNewMemory( &lpcd, sizeof(COMMDESCRIPTOR) ) )
               return( FALSE );
          strcpy( lpcd->szTitle, ptwnd->tp.szConnCard);
          INITIALISE_PTR(lpcd->lpsc );
          INITIALISE_PTR(lpcd->lpic );
          if( !LoadCommDescriptor( lpcd ) )
               return( FALSE );

          /* Vape the phone number and script fields.
           */
          if( lpcd->wProtocol & PROTOCOL_SERIAL )
               *lpcd->lpsc->szPhone = '\0';
          *lpcd->szScript = '\0';

          /* Open this connection card.
           */
          if( !Amcomm_OpenCard( hwndFrame, &ptwnd->lpcdev, lpcd, TerminalCallback, (DWORD)(LPSTR)hwnd, NULL, NULL, TRUE, FALSE ) )
               {
               fMessageBox( hwnd, 0, GS(IDS_STR630), MB_ICONEXCLAMATION|MB_OK );
               return( FALSE );
               }
          ptwnd->lpcdev->fOwnDescriptors = TRUE;
          }
#ifdef WIN32
     else if( !Amcomm_Open( &ptwnd->lpcdev, ptwnd->tp.szConnCard, TerminalCallback, (DWORD)(LPSTR)hwnd, NULL, NULL, &rdDef, TRUE, FALSE ) )
#else
     else if( !Amcomm_Open( &ptwnd->lpcdev, ptwnd->tp.szConnCard, TerminalCallback, (DWORD)(LPSTR)hwnd, NULL, NULL, NULL ) )
#endif
          {
          fMessageBox( hwnd, 0, GS(IDS_STR630), MB_ICONEXCLAMATION|MB_OK );
          return( FALSE );
          }
     ShowUnreadMessageCount();
     fInitialisingTerminal = FALSE;

     /* Kick start to get to the prompt.
      */
     if( fKickStart )
          Amcomm_WriteChar( ptwnd->lpcdev, '\n' );

     /* Successful connect, so show host name in the
      * title bar.
      */
     GetWindowText( hwnd, lpTmpBuf, 20 );
     strcat( lpTmpBuf, " - " );
     strcat( lpTmpBuf, lpszName );
     SetWindowText( hwnd, lpTmpBuf );
     return( TRUE );
}

/* This is the terminal callback function. It is called when
 * new data arrives or the connection is cut off.
 */
BOOL EXPORT FAR PASCAL TerminalCallback( LPCOMMDEVICE lpcdev, int nCode, DWORD dwAppData, LPARAM lParam )
{
     HWND hwnd;

     hwnd = (HWND)dwAppData;
     switch( nCode )
          {
          case CCB_STATUSTEXT: {
               TERMINALWND FAR * ptwnd;

               /* Write status information somewhere.
                */
               MUST_BE_FRAME_WND(hwnd);
               ptwnd = GetTerminalWnd( hwnd );
               if( NULL != ptwnd->lpcdev )
                    {
                    LPSTR lpString;

                    lpString = (LPSTR)lParam;
                    Terminal_ChildWriteString( ptwnd->hwndTerm, lpString, strlen(lpString), TRUE );
                    }
               return( TRUE );
               }

          case CCB_DISCONNECTED:
               PostMessage( hwnd, WM_CLOSE, 0, 0L );
               fMessageBox( hwnd, 0, GS(IDS_STR958), MB_OK|MB_ICONINFORMATION );
               return( TRUE );

          case CCB_DATAECHO: {
               TERMINALWND FAR * ptwnd;

               MUST_BE_FRAME_WND(hwnd);
               ptwnd = GetTerminalWnd( hwnd );
               if( NULL != ptwnd->lpcdev )
                    {
                    DATAECHOSTRUCT FAR * lpdes;

                    lpdes = (DATAECHOSTRUCT FAR *)lParam;
                    Terminal_ChildWriteString( ptwnd->hwndTerm, lpdes->lpBuffer, lpdes->cbBuffer, TRUE );
                    if( !fInitialisingTerminal )
                         {
                         lpdes->lpComBuf->cbInComBuf = 0;
                         lpdes->lpComBuf->cbComBuf = 0;
                         }
                    }
               return( TRUE );
               }

          case CCB_DATAREADY: {
               TERMINALWND FAR * ptwnd;

               MUST_BE_FRAME_WND(hwnd);
               ptwnd = GetTerminalWnd( hwnd );
               if( NULL != ptwnd->lpcdev )
                    {
                    LPBUFFER lpBuffer;

                    lpBuffer = (LPBUFFER)lParam;
                    Terminal_ChildWriteString( ptwnd->hwndTerm, lpBuffer->achComBuf, lpBuffer->cbInComBuf, TRUE );
                    if( !fInitialisingTerminal )
                         {
                         lpBuffer->cbInComBuf = 0;
                         lpBuffer->cbComBuf = 0;
                         }
                    }
               return( TRUE );
               }
          }
     return( FALSE );
}

void FASTCALL Terminal_ShowOption( HWND hwnd, WORD wID )
{
     TERMINALWND FAR * ptwnd;

     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     if( ptwnd->tp.fShowNegotiation )
          {
          Terminal_ChildWriteString( hwnd, GS( wID ), -1, TRUE );
          Terminal_WriteChar( hwnd, 13, TRUE );
          Terminal_WriteChar( hwnd, 10, TRUE );
          }
}

void FASTCALL Terminal_ShowOptionString( HWND hwnd, char * pStr )
{
     TERMINALWND FAR * ptwnd;

     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     if( ptwnd->tp.fShowNegotiation )
          {
          Terminal_ChildWriteString( hwnd, pStr, -1, TRUE );
          Terminal_WriteChar( hwnd, 13, TRUE );
          Terminal_WriteChar( hwnd, 10, TRUE );
          }
}

BYTE FASTCALL FgRGBToB4( TERMINALWND FAR * ptwnd, COLORREF cr )
{
     return( RGBToB4( ptwnd->tp.lpcrFgMapTable, cr ) );
}

BYTE FASTCALL BgRGBToB4( TERMINALWND FAR * ptwnd, COLORREF cr )
{
     return( RGBToB4( ptwnd->tp.lpcrBgMapTable, cr ) );
}

BYTE FASTCALL RGBToB4( COLORREF FAR * lpcrMapTable, COLORREF cr )
{
     BYTE b4;
     register int c;

     /* First use lookup table.
      */
     for( c = 0; c < 8; ++c )
          if( lpcrMapTable[ c ] == cr )
               return( c );

     /* If not in table, synthesize it.
      */
     b4 = 0;
     if( GetRValue( cr ) )    b4 |= 0x1;
     if( GetGValue( cr ) )    b4 |= 0x2;
     if( GetBValue( cr ) )    b4 |= 0x4;
     return( b4 );
}

COLORREF FASTCALL FgB4ToRGB( TERMINALWND FAR * ptwnd, BYTE b4 )
{
     ASSERT( b4 >= 0 && b4 <= 7 );
     return( ptwnd->tp.lpcrFgMapTable[ b4 ] );
}

COLORREF FASTCALL BgB4ToRGB( TERMINALWND FAR * ptwnd, BYTE b4 )
{
     ASSERT( b4 >= 0 && b4 <= 7 );
     return( ptwnd->tp.lpcrBgMapTable[ b4 ] );
}

BOOL FASTCALL WriteCharToConnection( HWND hwnd, char ch )
{
     TERMINALWND FAR * ptwnd;

     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     if( NULL != ptwnd->lpcdev )
          return( Amcomm_WriteChar( ptwnd->lpcdev, ch ) );
     return( FALSE );
}

BOOL FASTCALL WriteStringToConnection( HWND hwnd, char * npsz, int length )
{
     TERMINALWND FAR * ptwnd;

     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     if( NULL != ptwnd->lpcdev )
          return( Amcomm_WriteString( ptwnd->lpcdev, npsz, length ) );
     return( FALSE );
}

/* This function processes the WM_HSCROLL message.
 */
void FASTCALL Terminal_OnHScroll( HWND hwnd, HWND hwndCtl, UINT code, int pos )
{
     TERMINALWND FAR * ptwnd;
     int nSBQXShiftTmp;
     int cShift;

     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     nSBQXShiftTmp = ptwnd->nSBQXShift;
     switch( code )
          {
          case SB_TOP:
               ptwnd->nSBQXShift = 0;
               break;

          case SB_PAGEUP:
               if( ( ptwnd->nSBQXShift -= ptwnd->nSBQXSize ) < 0 )
                    ptwnd->nSBQXShift = 0;
               break;

          case SB_LINEUP:
               if( ptwnd->nSBQXShift > 0 )
                    --ptwnd->nSBQXShift;
               break;

          case SB_LINEDOWN:
               if( ptwnd->nSBQXShift < ptwnd->nSBQXExtent )
                    ++ptwnd->nSBQXShift;
               break;

          case SB_PAGEDOWN:
               if( ( ptwnd->nSBQXShift += ptwnd->nSBQXSize ) > ptwnd->nSBQXExtent )
                    ptwnd->nSBQXShift = ptwnd->nSBQXExtent;
               break;

          case SB_BOTTOM:
               ptwnd->nSBQXShift = ptwnd->nSBQXExtent;
               break;

          case SB_THUMBPOSITION:
               ptwnd->nSBQXShift = pos;
               break;
          }
     cShift = ptwnd->nSBQXShift - nSBQXShiftTmp;
     if( cShift )
          TextBltToImageBitmap( hwnd, NULL );
     SetScrollPos( hwnd, SB_HORZ, ptwnd->nSBQXShift, TRUE );
     Terminal_SetCursor( hwnd );
}

/* This function processes the WM_VSCROLL message.
 */
void FASTCALL Terminal_OnVScroll( HWND hwnd, HWND hwndCtl, UINT code, int pos )
{
     TERMINALWND FAR * ptwnd;
     int nSBQYShiftTmp;
     int cShift;

     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     nSBQYShiftTmp = ptwnd->nSBQYShift;
     switch( code )
          {
          case SB_TOP:
               ptwnd->nSBQYShift = 0;
               break;

          case SB_PAGEUP:
               if( ( ptwnd->nSBQYShift -= ptwnd->nSBQYSize ) < 0 )
                    ptwnd->nSBQYShift = 0;
               break;

          case SB_LINEUP:
               if( ptwnd->nSBQYShift > 0 )
                    --ptwnd->nSBQYShift;
               break;

          case SB_LINEDOWN:
               if( ptwnd->nSBQYShift < ptwnd->nSBQYExtent )
                    ++ptwnd->nSBQYShift;
               break;

          case SB_PAGEDOWN:
               if( ( ptwnd->nSBQYShift += ptwnd->nSBQYSize ) > ptwnd->nSBQYExtent )
                    ptwnd->nSBQYShift = ptwnd->nSBQYExtent;
               break;

          case SB_BOTTOM:
               ptwnd->nSBQYShift = ptwnd->nSBQYExtent;
               break;

          case SB_THUMBPOSITION:
               ptwnd->nSBQYShift = pos;
               break;
          }
     cShift = ptwnd->nSBQYShift - nSBQYShiftTmp;
     if( cShift < 0 )
          {
          RECT rc;

          /* Code to scroll image down. If the scroll is less than 1 screenful, we
           * scroll the image bitmap down and repaint the uncovered area. Otherwise
           * we repaint the whole area from the text buffer.
           */
          if( -cShift < ptwnd->nSBQYSize ) {
               /* Scroll the memory image by the shift factor */
               GetClientRect( hwnd, &rc );
               rc.bottom = ptwnd->nLineHeight * ptwnd->nSBQYSize;
               ScrollWindow( hwnd, 0, -( ptwnd->nLineHeight * cShift ), &rc, &rc );
               }
          else {
               TextBltToImageBitmap( hwnd, NULL );
               if( ptwnd->fSelection )
                    Terminal_DrawSelection( hwnd, ptwnd->nSBQX1Anchor,
                                                                      ptwnd->nSBQY1Anchor,
                                                                      ptwnd->nSBQX2Anchor,
                                                                      ptwnd->nSBQY2Anchor );
               }
          }
     else if( cShift > 0 )
          {
          RECT rc;

          /* Code to scroll image up. If the scroll is less than 1 screenful, we
           * scroll the image bitmap up and repaint the uncovered area. Otherwise
           * we repaint the whole area from the text buffer.
           */
          if( cShift < ptwnd->nSBQYSize ) {
               /* Scroll the memory image by the shift factor */
               GetClientRect( hwnd, &rc );
               rc.bottom = ptwnd->nLineHeight * ptwnd->nSBQYSize;
               ScrollWindow( hwnd, 0, ptwnd->nLineHeight * -cShift, &rc, &rc );
               }
          else {
               TextBltToImageBitmap( hwnd, NULL );
               if( ptwnd->fSelection )
                    Terminal_DrawSelection( hwnd, ptwnd->nSBQX1Anchor,
                                                                      ptwnd->nSBQY1Anchor,
                                                                      ptwnd->nSBQX2Anchor,
                                                                      ptwnd->nSBQY2Anchor );
               }
          }
     SetScrollPos( hwnd, SB_VERT, ptwnd->nSBQYShift, TRUE );
     Terminal_SetCursor( hwnd );
}

/* This function processes the WM_CHAR message.
 */
void FASTCALL Terminal_OnChar( HWND hwnd, UINT ch, int cRepeat )
{
     TERMINALWND FAR * ptwnd;

     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     if( ch == VK_RETURN && IsIconic( hwnd ) )
          SendMessage( hwndMDIClient, WM_MDIRESTORE, (WPARAM)hwnd, 0L );
     else {
          if( ptwnd->fSelection )
               {
               ptwnd->fSelection = FALSE;
               InvalidateRect( hwnd, NULL, FALSE );
               UpdateWindow( hwnd );
               }
          if( ptwnd->tp.fStripHigh )
               ch &= 0x7F;
          if( NULL != ptwnd->lpcdev )
               WriteCharToConnection( hwnd, (char)ch );
          if( ptwnd->tp.fLocalEcho )
               Terminal_WriteChar( hwnd, (char)ch, TRUE );
          if( (char)ch == 13 && ptwnd->tp.fOutCRtoCRLF )
               Terminal_WriteChar( hwnd, 10, TRUE );
          }
}

/* This function processes the WM_PAINT message.
 */
void FASTCALL Terminal_OnPaint( HWND hwnd )
{
     PAINTSTRUCT ps;
     TERMINALWND FAR * ptwnd;
     RECT rc;

     MUST_BE_CHILD_WND(hwnd);
     BeginPaint( hwnd, &ps );
     rc = ps.rcPaint;
     TextBltToImageBitmap( hwnd, &rc );

     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     if( ptwnd->fSelection )
          Terminal_DrawSelection( hwnd, ptwnd->nSBQX1Anchor,
                                                            ptwnd->nSBQY1Anchor,
                                                            ptwnd->nSBQX2Anchor,
                                                            ptwnd->nSBQY2Anchor );
     EndPaint( hwnd, &ps );
     Terminal_SetCursor( hwnd );
}

/* This function handles the WM_SETFOCUS message.
 */
void FASTCALL TerminalFrame_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
     TERMINALWND FAR * ptwnd;

     MUST_BE_FRAME_WND(hwnd);
     ptwnd = GetTerminalWnd( hwnd );
     SetFocus( ptwnd->hwndTerm );
}

/* This function handles the WM_SETFOCUS message.
 */
void FASTCALL Terminal_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
     TERMINALWND FAR * ptwnd;

     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     if( !ptwnd->fWndHasCaret )
          {
          CreateCaret( hwnd, NULL, ptwnd->nCharWidth, ptwnd->nLineHeight );
          ShowCaret( hwnd );
          ptwnd->fHasCaret = TRUE;
          ptwnd->fWndHasCaret = TRUE;
          }
     Terminal_SetCursor( hwnd );
     SetFocus( ptwnd->hwndTerm );
}

/* This function handles the WM_KILLFOCUS message.
 */
void FASTCALL Terminal_OnKillFocus( HWND hwnd, HWND hwndNewFocus )
{
     TERMINALWND FAR * ptwnd;

     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent( hwnd ) );
     if( ptwnd->fWndHasCaret )
          {
          HideCaret( hwnd );
          DestroyCaret();
          ptwnd->fHasCaret = FALSE;
          ptwnd->fWndHasCaret = FALSE;
          }
}

/* This function handles the WM_CLOSE message.
 */
void FASTCALL TerminalFrame_OnClose( HWND hwnd )
{
     TERMINALWND FAR * ptwnd;

     MUST_BE_FRAME_WND(hwnd);
     ptwnd = GetTerminalWnd( hwnd );
     if( fInitialisingTerminal )
          {
          wCommErr = WCE_ABORT;
          return;
          }

     /* If ZMODEM activity going on, kill it.
      */
     if( IsTransferActive() && hwnd != hwndCixTerminal )
          if( NULL != ptwnd->lpcdev && NULL != ptwnd->hxfer )
               {
               CancelTransfer( ptwnd->hxfer, ptwnd->lpcdev );
               return;
               }

     /* Close the comm port if not already closed.
      */
     if( NULL != ptwnd->lpcdev && !ptwnd->fAttached )
          if( ptwnd->lpcdev->fOpen )
               Amcomm_Close( ptwnd->lpcdev );

     /* Destroy the window.
      */
     Adm_DestroyMDIWindow( hwnd );
}

/* This function handles the WM_DESTROY message.
 */
void FASTCALL TerminalFrame_OnDestroy( HWND hwnd )
{
     HGLOBAL hScrollBuf;
     HGLOBAL hColourBuf;
     TERMINALWND FAR * ptwnd;

     MUST_BE_FRAME_WND(hwnd);
     ptwnd = GetTerminalWnd( hwnd );

     /* If this is the CIX debug terminal window, zap the
      * handle to show that it doesn't exist no more.
       */
     if( hwnd == hwndCixTerminal )
          hwndCixTerminal = NULL;

     /* Close the device and delete the PTWND structure.
      */
     else if( NULL != ptwnd->lpcdev )
          {
          Amcomm_Close( ptwnd->lpcdev );
          Amcomm_Destroy( ptwnd->lpcdev );
          }

     /* Delete the scroll buffer.
      */
     hScrollBuf = GlobalPtrHandle( ptwnd->lpScrollBuf );
     GlobalUnlock( hScrollBuf );
     GlobalFree( hScrollBuf );
     if( ptwnd->lpColourBuf )
          {
          hColourBuf = GlobalPtrHandle( ptwnd->lpColourBuf );
          GlobalUnlock( hColourBuf );
          GlobalFree( hColourBuf );
          }
     DeleteFont( ptwnd->tp.hFont );

     /* Delete the colour mapping tables.
      */
     if( ptwnd->tp.lpcrBgMapTable )
          FreeMemory( &ptwnd->tp.lpcrBgMapTable );
     if( ptwnd->tp.lpcrFgMapTable )
          FreeMemory( &ptwnd->tp.lpcrFgMapTable );

     /* Close any open log.
      */
     CloseTerminalLog( hwnd );
     FreeMemory( &ptwnd );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL Terminal_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
     TERMINALWND FAR * ptwnd;

     /* Adjust the child window size.
      */
     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     if( NULL != ptwnd->hwndTerm )
          if( 0 != ptwnd->nLineHeight )
               {
               if( ptwnd->lpColourBuf )
                    {
                    ptwnd->nSBQYSize = cy / ptwnd->nLineHeight;
                    ptwnd->nSBQXSize = min( cx / ptwnd->nCharWidth, ptwnd->tp.nLineWidth );
                    }
               else
                    {
                    ptwnd->nSBQYSize = cy / ptwnd->nLineHeight;
                    ptwnd->nSBQXSize = cx / ptwnd->nCharWidth;
                    }
               if( !ptwnd->fOriginMode )
                    ptwnd->nSBQYBottom = ptwnd->nSBQYSize - 1;
          
               /* May need to scroll image up to keep cursor in view
                */
               if( ptwnd->nSBQY >= ptwnd->nSBQYSize )
                    {
                    ptwnd->nSBQYShift += ( ptwnd->nSBQY - ptwnd->nSBQYSize ) + 1;
                    if( ptwnd->nSBQYShift > ptwnd->nSBQYExtent )
                         ptwnd->nSBQYExtent = ptwnd->nSBQYShift;
                    ptwnd->nSBQY = ptwnd->nSBQYSize - 1;
                    }
          
               /* Update the vertical scroll range
                */
               Terminal_SetScrollRange( hwnd, SB_VERT, 0, ptwnd->nSBQYExtent );
               SetScrollPos( hwnd, SB_VERT, ptwnd->nSBQYShift, TRUE );
          
               /* Update the horizontal scroll range
                */
               Terminal_SetScrollRange( hwnd, SB_HORZ, 0, ptwnd->nSBQXExtent );
               SetScrollPos( hwnd, SB_HORZ, ptwnd->nSBQXShift, TRUE );
               }
     FORWARD_WM_SIZE( hwnd, state, cx, cy, DefWindowProc );
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL TerminalFrame_OnAdjustWindows( HWND hwnd, int dx, int dy )
{
     Adm_InflateWnd( GetDlgItem( hwnd, IDD_TERMINAL ), dx, dy );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL TerminalFrame_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
     TERMINALWND FAR * ptwnd;

     /* Adjust the child window size.
      */
     MUST_BE_FRAME_WND(hwnd);
     ptwnd = GetTerminalWnd( hwnd );
     if( NULL != ptwnd->hwndTerm )
          {
          char szSection[ LEN_TERMNAME+20+1 ];
          char sz[ 40 ];

          /* Save the new window state to the configuration file.
           */
          if (Amuser_CreateWindowState( sz, hwnd ))
              {
              wsprintf( szSection, "%s Properties", ptwnd->szName );
              Amuser_WritePPString( szSection, "Window", sz );
              }
          }
     FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* This function handles the WM_MOVE message.
 */
void FASTCALL TerminalFrame_OnMove( HWND hwnd, int x, int y )
{
     TERMINALWND FAR * ptwnd;
     char szSection[ LEN_TERMNAME+20+1 ];
     char sz[ 40 ];

     ptwnd = GetTerminalWnd( hwnd );
     if (Amuser_CreateWindowState( sz, hwnd ))
         {
         wsprintf( szSection, "%s Properties", ptwnd->szName );
         Amuser_WritePPString( szSection, "Window", sz );
         }
     FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function computes the new character cell dimensions, in pixels,
 * based on the current terminal font.
 */

void FASTCALL Terminal_ComputeCellDimensions( HWND hwnd )
{
     TERMINALWND FAR * ptwnd;
     TEXTMETRIC tm;
     HDC hdc;

     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     hdc = GetDC( hwnd );
     SelectFont( hdc, ptwnd->tp.hFont );
     GetTextMetrics( hdc, &tm );
     ptwnd->nLineHeight = tm.tmHeight + tm.tmExternalLeading;
     ptwnd->nCharWidth = tm.tmAveCharWidth;
     ReleaseDC( hwnd, hdc );
}

/* This code does nothing to the display, so it can be called without
 * worrying about performance hit.
 */
void FASTCALL Terminal_WriteString( HWND hwnd, LPSTR lpstr, int count, BOOL fUpdate )
{
     TERMINALWND FAR * ptwnd;

     MUST_BE_FRAME_WND(hwnd);
     ptwnd = GetTerminalWnd( hwnd );
     Terminal_ChildWriteString( ptwnd->hwndTerm, lpstr, count, fUpdate );
}

/* This code does nothing to the display, so it can be called without
 * worrying about performance hit.
 */
void FASTCALL Terminal_ChildWriteString( HWND hwnd, LPSTR lpstr, int count, BOOL fUpdate )
{
     register int c;

     MUST_BE_CHILD_WND(hwnd);
     if( count == -1 )
          count = lstrlen( lpstr );
     for( c = 0; c < count; ++c )
          Terminal_WriteChar( hwnd, lpstr[ c ], FALSE );
     if( fUpdate )
          {
          Terminal_RenderLine( hwnd );
          Terminal_SetCursor( hwnd );
          }
}

/* This function interprets the character depending on the active
 * terminal emulator mode.
 */
void FASTCALL Terminal_WriteChar( HWND hwnd, char ch, BOOL fUpdate )
{
     TERMINALWND FAR * ptwnd;

     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     if( ptwnd->lpColourBuf )
          Terminal_WriteAnsiChar( hwnd, ch );
     else
          Terminal_WriteTTYChar( hwnd, ch );
     if( fUpdate )
          {
          Terminal_RenderLine( hwnd );
          Terminal_SetCursor( hwnd );
          }
}

/* This function interprets the character using VT100 display mode.
 */
void FASTCALL Terminal_WriteAnsiChar( HWND hwnd, char ch )
{
     TERMINALWND FAR * ptwnd;
     BOOL fCixTerminal;

     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     fCixTerminal = GetParent(hwnd) == hwndCixTerminal;
     switch( ptwnd->nWriteState )
          {
          case 0:
               if( ch == 27 )
                    ptwnd->nWriteState = 1;
               else if( ch != 14 && ch != 15 )
                    Terminal_WriteTTYChar( hwnd, ch );
               break;

          case 4:
          case 5:
               /* Character set select - ignore. */
               ptwnd->nWriteState = 0;
               break;

          case 1:
               if( ch == 'c' )
                    {
                    ptwnd->nAnsiVal = 0;
                    ptwnd->nWriteState = 0;
                    }
               else if( ch == '(' )
                    ptwnd->nWriteState = 4;
               else if( ch == ')' )
                    ptwnd->nWriteState = 5;
               else if( ch == '7' )
                    goto AN2;
               else if( ch == '8' )
                    goto AN3;
               else if( ch == 'M' )
                    {
                    if( !fCixTerminal )
                         {
AN5:                Terminal_RenderLine( hwnd );
                         ptwnd->nSBQY = ptwnd->nSBQYCaret - ptwnd->nSBQYShift;
                         if( ptwnd->nSBQY > ptwnd->nSBQYTop )
                              {
                              --ptwnd->nSBQY;
                              --ptwnd->nSBQYCaret;
                              }
                         else
                              {
                              LPSTR lpstr;
                              LPSTR lpAttr;

                              ASSERT( ptwnd->nSBQYTop + ptwnd->nSBQYShift < ptwnd->tp.nBufferSize );
                              ASSERT( ( ( ( ptwnd->nSBQYBottom - ptwnd->nSBQYTop ) - 1 ) + ptwnd->nSBQYTop + ptwnd->nSBQYShift ) < ptwnd->tp.nBufferSize );
                              lpstr = ptwnd->lpScrollBuf + ( ptwnd->tp.nLineWidth * ( ptwnd->nSBQYTop + ptwnd->nSBQYShift ) );
                              _fmemmove( lpstr + ptwnd->tp.nLineWidth, lpstr, ( ( ptwnd->nSBQYBottom - ptwnd->nSBQYTop ) - 1 ) * ptwnd->tp.nLineWidth );
                              if( ptwnd->lpColourBuf )
                                   {
                                   lpAttr = ptwnd->lpColourBuf + ( ptwnd->tp.nLineWidth * ( ptwnd->nSBQYTop + ptwnd->nSBQYShift ) );
                                   _fmemmove( lpAttr + ptwnd->tp.nLineWidth, lpAttr, ( ( ptwnd->nSBQYBottom - ptwnd->nSBQYTop ) - 1 ) * ptwnd->tp.nLineWidth );
                                   }
                              lpstr = ptwnd->lpScrollBuf + ( ptwnd->tp.nLineWidth * ( ptwnd->nSBQYTop + ptwnd->nSBQYShift ) );
                              memset( lpstr, ' ', ptwnd->tp.nLineWidth );
                              if( ptwnd->lpColourBuf )
                                   {
                                   lpAttr = ptwnd->lpColourBuf + ( ptwnd->tp.nLineWidth * ( ptwnd->nSBQYTop + ptwnd->nSBQYShift ) );
                                   memset( lpAttr, ptwnd->crAttrByte, ptwnd->tp.nLineWidth );
                                   }
                              Terminal_RenderScreen( hwnd, 1 );
                              }
                         }
                    ptwnd->nWriteState = 0;
                    }
               else if( ch == 'D' )
                    {
                    if( !fCixTerminal )
                         {
                         Terminal_RenderLine( hwnd );
                         ptwnd->nSBQY = ptwnd->nSBQYCaret - ptwnd->nSBQYShift;
                         if( ptwnd->nSBQY < ptwnd->nSBQYBottom ) {
                              ++ptwnd->nSBQY;
                              ++ptwnd->nSBQYCaret;
                              ASSERT( ptwnd->nSBQYCaret < ptwnd->tp.nBufferSize );
                              ASSERT( ptwnd->nSBQY + ptwnd->nSBQYShift < ptwnd->tp.nBufferSize );
                              }
                         else
                              {
                              LPSTR lpstr;
                              LPSTR lpAttr;

                              ASSERT( ptwnd->nSBQYTop + ptwnd->nSBQYShift < ptwnd->tp.nBufferSize );
                              lpstr = ptwnd->lpScrollBuf + ( ptwnd->tp.nLineWidth * ( ptwnd->nSBQYTop + ptwnd->nSBQYShift ) );
                              _fmemmove( lpstr, lpstr + ptwnd->tp.nLineWidth, ( ( ptwnd->nSBQYBottom - ptwnd->nSBQYTop ) - 1 ) * ptwnd->tp.nLineWidth );
                              if( ptwnd->lpColourBuf )
                                   {
                                   lpAttr = ptwnd->lpColourBuf + ( ptwnd->tp.nLineWidth * ( ptwnd->nSBQYTop + ptwnd->nSBQYShift ) );
                                   _fmemmove( lpAttr, lpAttr + ptwnd->tp.nLineWidth, ( ( ptwnd->nSBQYBottom - ptwnd->nSBQYTop ) - 1 ) * ptwnd->tp.nLineWidth );
                                   }
                              ASSERT( ptwnd->nSBQYBottom + ptwnd->nSBQYShift < ptwnd->tp.nBufferSize );
                              lpstr = ptwnd->lpScrollBuf + ( ptwnd->tp.nLineWidth * ( ptwnd->nSBQYBottom + ptwnd->nSBQYShift ) );
                              memset( lpstr, ' ', ptwnd->tp.nLineWidth );
                              if( ptwnd->lpColourBuf )
                                   {
                                   lpAttr = ptwnd->lpColourBuf + ( ptwnd->tp.nLineWidth * ( ptwnd->nSBQYBottom + ptwnd->nSBQYShift ) );
                                   memset( lpAttr, ptwnd->crAttrByte, ptwnd->tp.nLineWidth );
                                   }
                              Terminal_RenderScreen( hwnd, -1 );
                              }
                         }
                    ptwnd->nWriteState = 0;
                    }
               else if( ch == 'E' )
                    {
                    if( fCixTerminal )
                         ptwnd->nWriteState = 0;
                    else
                         {
                         ptwnd->nSBQXCaret = ptwnd->nSBQX = 0;
                         goto AN5;
                         }
                    }
               else if( ch == '=' )
                    ptwnd->nWriteState = 0;
               else if( ch == 'C' )
                    ptwnd->nWriteState = 0;
               else if( ch == '>' )
                    ptwnd->nWriteState = 0;
               else if( ch != '[' ) {
                    if( !fCixTerminal )
                         Terminal_WriteTTYChar( hwnd, ch );
                    ptwnd->nWriteState = 0;
                    }
               else {
                    ptwnd->nWriteState = 2;
                    ptwnd->nAnsiVal = 0;
                    }
               break;

          case 2:
AN1:      if( ch == 's' )
                    {
AN2:           ptwnd->nSaveCol = ptwnd->nSBQXCaret;
                    ptwnd->nSaveRow = ptwnd->nSBQYCaret;
                    ptwnd->nWriteState = 0;
                    }
               else if( ch == '?' )
                    ptwnd->nWriteState = 6;
               else if( ch == 'A' ) {
                    if( !fCixTerminal )
                         {
                         int cMotion;

                         Terminal_RenderLine( hwnd );
                         cMotion = ( ptwnd->nAnsiVal < 1 ) ? 1 : ptwnd->AnsiParmVal[ 0 ];
                         ptwnd->nSBQY = ptwnd->nSBQYCaret - ptwnd->nSBQYShift;
                         while( cMotion-- && ptwnd->nSBQY > ptwnd->nSBQYTop )
                              {
                              --ptwnd->nSBQY;
                              --ptwnd->nSBQYCaret;
                              }
                         }
                    ptwnd->nWriteState = 0;
                    }
               else if( ch == 'B' ) {
                    if( !fCixTerminal )
                         {
                         int cMotion;

                         Terminal_RenderLine( hwnd );
                         cMotion = ( ptwnd->nAnsiVal < 1 ) ? 1 : ptwnd->AnsiParmVal[ 0 ];
                         ptwnd->nSBQY = ptwnd->nSBQYCaret - ptwnd->nSBQYShift;
                         while( cMotion-- && ptwnd->nSBQY < ptwnd->nSBQYBottom )
                              Terminal_NewLine( hwnd );
                         }
                    ptwnd->nWriteState = 0;
                    }
               else if( ch == 'C' ) {
                    if( !fCixTerminal )
                         {
                         int cMotion;

                         cMotion = ( ptwnd->nAnsiVal < 1 ) ? 1 : ptwnd->AnsiParmVal[ 0 ];
                         ptwnd->nSBQX = ptwnd->nSBQXCaret - ptwnd->nSBQXShift;
                         while( cMotion-- && ptwnd->nSBQX < ptwnd->tp.nLineWidth - 1 )
                              {
                              ++ptwnd->nSBQX;
                              ++ptwnd->nSBQXCaret;
                              }
                         }
                    ptwnd->nWriteState = 0;
                    }
               else if( ch == 'D' ) {
                    if( !fCixTerminal )
                         {
                         int cMotion;

                         cMotion = ( ptwnd->nAnsiVal < 1 ) ? 1 : ptwnd->AnsiParmVal[ 0 ];
                         ptwnd->nSBQX = ptwnd->nSBQXCaret - ptwnd->nSBQXShift;
                         while( cMotion-- && ptwnd->nSBQX )
                              {
                              --ptwnd->nSBQX;
                              --ptwnd->nSBQXCaret;
                              }
                         }
                    ptwnd->nWriteState = 0;
                    }
               else if( ch == 'f' || ch == 'H' )
                    {
                    if( !fCixTerminal )
                         HandleCursorRepos( hwnd );
                    ptwnd->nWriteState = 0;
                    }
               else if( ch == 'r' )
                    {
                    if( !fCixTerminal )
                         {
                         Terminal_RenderLine( hwnd );
                         ptwnd->nSBQY = ptwnd->nSBQYCaret - ptwnd->nSBQYShift;
                         if( ptwnd->nSBQY >= 24 )
                              if( ( ptwnd->nSBQYShift = ptwnd->nSBQYExtent + ptwnd->nSBQYSize ) == ptwnd->tp.nBufferSize )
                                   {
                                   _fmemmove( ptwnd->lpScrollBuf, ptwnd->lpScrollBuf + ptwnd->tp.nLineWidth, ptwnd->tp.nLineWidth * ( ptwnd->tp.nBufferSize - 1 ) );
                                   memset( ptwnd->lpScrollBuf + ptwnd->tp.nLineWidth * ( ptwnd->tp.nBufferSize - 1 ), ' ', ptwnd->tp.nLineWidth );
                                   if( ptwnd->lpColourBuf )
                                        {
                                        _fmemmove( ptwnd->lpColourBuf, ptwnd->lpColourBuf + ptwnd->tp.nLineWidth, ptwnd->tp.nLineWidth * ( ptwnd->tp.nBufferSize - 1 ) );
                                        memset( ptwnd->lpColourBuf + ptwnd->tp.nLineWidth * ( ptwnd->tp.nBufferSize - 1 ), ' ', ptwnd->tp.nLineWidth );
                                        }
                                   --ptwnd->nSBQYShift;
                                   TextBltToImageBitmap( hwnd, NULL );
                                   }
                         ptwnd->nSBQYTop = ( ptwnd->nAnsiVal < 1 ) ? 0 : ptwnd->AnsiParmVal[ 0 ] - 1;
                         ptwnd->nSBQYBottom = ( ptwnd->nAnsiVal < 2 ) ? ptwnd->nSBQYSize - 1 : ptwnd->AnsiParmVal[ 1 ] - 1;
                         }
                    ptwnd->nWriteState = 0;
                    }
               else if( ch == 'J' ) {
                    if( !fCixTerminal )
                         {
                         LPSTR lpstr;
                         LPSTR lpAttr = NULL;

                         Terminal_RenderLine( hwnd );
                         ptwnd->nSBQY = ptwnd->nSBQYCaret - ptwnd->nSBQYShift;
                         ASSERT( ptwnd->nSBQY < ptwnd->tp.nBufferSize );
                         ASSERT( ptwnd->nSBQX < ptwnd->tp.nLineWidth );
                         ASSERT( ptwnd->nSBQYShift < ptwnd->tp.nBufferSize );
                         ptwnd->crAttrByte = ptwnd->crRealAttrByte;
                         lpstr = ptwnd->lpScrollBuf + ( ptwnd->nSBQYShift * ptwnd->tp.nLineWidth );
                         if( ptwnd->lpColourBuf )
                              lpAttr = ptwnd->lpColourBuf + ( ptwnd->nSBQYShift * ptwnd->tp.nLineWidth );
                         switch( ( ptwnd->nAnsiVal < 1 ) ? 0 : ptwnd->AnsiParmVal[ 0 ] )
                              {
                              case 0:
                                   ASSERT( ptwnd->nSBQY < ptwnd->tp.nBufferSize );
                                   lpstr += ptwnd->tp.nLineWidth * ptwnd->nSBQY;
                                   memset( lpstr + ptwnd->nSBQX, ' ', ptwnd->tp.nLineWidth - ptwnd->nSBQX );
                                   if( lpAttr )
                                        {
                                        lpAttr += ptwnd->tp.nLineWidth * ptwnd->nSBQY;
                                        memset( lpAttr + ptwnd->nSBQX, ptwnd->crAttrByte, ptwnd->tp.nLineWidth - ptwnd->nSBQX );
                                        }
                                   if( ptwnd->nSBQY < ptwnd->nSBQYSize - 1 )
                                        {
                                        ASSERT( ( ptwnd->nSBQYSize - ptwnd->nSBQY ) - 1 < ptwnd->tp.nBufferSize );
                                        memset( lpstr + ptwnd->tp.nLineWidth, ' ', ( ( ptwnd->nSBQYSize - ptwnd->nSBQY ) - 1 ) * ptwnd->tp.nLineWidth );
                                        if( lpAttr )
                                             memset( lpAttr + ptwnd->tp.nLineWidth, ptwnd->crAttrByte, ( ( ptwnd->nSBQYSize - ptwnd->nSBQY ) - 1 ) * ptwnd->tp.nLineWidth );
                                        }
                                   TextBltToImageBitmap( hwnd, NULL );
                                   break;

                              case 1:
                                   if( ptwnd->nSBQY )
                                        {
                                        ASSERT( ptwnd->nSBQY - 1 < ptwnd->tp.nBufferSize );
                                        memset( lpstr, ' ', ( ptwnd->nSBQY - 1 ) * ptwnd->tp.nLineWidth );
                                        if( lpAttr )
                                             memset( lpAttr, ptwnd->crAttrByte, ( ptwnd->nSBQY - 1 ) * ptwnd->tp.nLineWidth );
                                        }
                                   if( ptwnd->nSBQX )
                                        {
                                        ASSERT( ptwnd->nSBQY < ptwnd->tp.nBufferSize );
                                        lpstr += ptwnd->nSBQY * ptwnd->tp.nLineWidth;
                                        memset( lpstr, ' ', ptwnd->nSBQX );
                                        if( lpAttr )
                                             {
                                             lpAttr += ptwnd->nSBQY * ptwnd->tp.nLineWidth;
                                             memset( lpAttr, ptwnd->crAttrByte, ptwnd->nSBQX );
                                             }
                                        }
                                   TextBltToImageBitmap( hwnd, NULL );
                                   break;

                              case 2:
                                   memset( lpstr, ' ', ptwnd->tp.nLineWidth * ptwnd->nSBQYSize );
                                   if( lpAttr )
                                        memset( lpAttr, ptwnd->crAttrByte, ptwnd->tp.nLineWidth * ptwnd->nSBQYSize );
                                   TextBltToImageBitmap( hwnd, NULL );
                                   break;
                              }
                         }
                    ptwnd->nWriteState = 0;
                    }
               else if( ch == '?' )
                    ptwnd->nWriteState = 2;
               else if( ch == '=' )
                    ptwnd->nWriteState = 2;
               else if( ch == 'h' )
                    ptwnd->nWriteState = 0;
               else if( ch == 'c' )
                    {
                    if( !fCixTerminal )
                         switch( ( ptwnd->nAnsiVal < 1 ) ? 0 : ptwnd->AnsiParmVal[ 0 ] )
                              {
                              case 0: {
                                   char sz[ 20 ];

                                   strcpy( sz, "\x01B[?1;2c" );
                                   WriteStringToConnection( hwnd, sz, strlen( sz ) );
                                   break;
                                   }
                              }
                    ptwnd->nWriteState = 0;
                    }
               else if( ch == 'n' )
                    {
                    if( !fCixTerminal )
                         switch( ( ptwnd->nAnsiVal < 1 ) ? 0 : ptwnd->AnsiParmVal[ 0 ] )
                              {
                              case 5: {
                                   char sz[ 20 ];

                                   strcpy( sz, "\x01B[0n" );
                                   WriteStringToConnection( hwnd, sz, strlen( sz ) );
                                   break;
                                   }

                              case 6: {
                                   char sz[ 20 ];
                                   int nYAdjust;
               
                                   nYAdjust = max( ptwnd->nSBQYSize - 24, 0 );
                                   wsprintf( sz, "\x01B[%d;%dR", ptwnd->nSBQY + 1 + nYAdjust, ptwnd->nSBQX + 1 );
                                   WriteStringToConnection( hwnd, sz, strlen( sz ) );
                                   break;
                                   }
                              }
                    ptwnd->nWriteState = 0;
                    }
               else if( ch == 'l' )
                    ptwnd->nWriteState = 0;
               else if( ch == 'm' )
                    {
                    register int c;

                    if( ptwnd->nAnsiVal == 0 )
                         ptwnd->AnsiParmVal[ ptwnd->nAnsiVal++ ] = 0;
                    if( !fCixTerminal )
                         for( c = 0; c < ptwnd->nAnsiVal; ++c )
                              switch( ptwnd->AnsiParmVal[ c ] )
                                   {
                                   case 0:
                                        if( ptwnd->fReverseOn )
                                             {
                                             int nBgAttr = GetBgAttr( ptwnd->crAttrByte );
                                             int nFgAttr = GetFgAttr( ptwnd->crAttrByte );
          
                                             SetFgAttr( ptwnd->crAttrByte, nBgAttr );
                                             SetBgAttr( ptwnd->crAttrByte, nFgAttr );
                                             ptwnd->fReverseOn = FALSE;
                                             }
                                        ClearUnderline();
                                        ClearBold();
                                        SetFgAttr( ptwnd->crAttrByte, FgRGBToB4( ptwnd, ptwnd->tp.crTermText ) );
                                        SetBgAttr( ptwnd->crAttrByte, BgRGBToB4( ptwnd, ptwnd->tp.crTermBack ) );
                                        break;
          
                                   case 1:
                                        SetBold();
                                        break;

                                   case 2:
                                        ClearBold();
                                        break;

                                   case 4:
                                        SetUnderline();
                                        break;

                                   case 7:
                                        if( !ptwnd->fReverseOn )
                                             {
                                             int nBgAttr = GetBgAttr( ptwnd->crAttrByte );
                                             int nFgAttr = GetFgAttr( ptwnd->crAttrByte );
          
                                             SetFgAttr( ptwnd->crAttrByte, nBgAttr );
                                             SetBgAttr( ptwnd->crAttrByte, nFgAttr );
                                             ptwnd->fReverseOn = TRUE;
                                             }
                                        break;

                                   case 30:       SetFgAttr( ptwnd->crAttrByte, BLACK );       ptwnd->crRealAttrByte = ptwnd->crAttrByte;   break;
                                   case 31:       SetFgAttr( ptwnd->crAttrByte, RED );         ptwnd->crRealAttrByte = ptwnd->crAttrByte;   break;
                                   case 32:       SetFgAttr( ptwnd->crAttrByte, GREEN );       ptwnd->crRealAttrByte = ptwnd->crAttrByte;   break;
                                   case 33:       SetFgAttr( ptwnd->crAttrByte, CYAN );        ptwnd->crRealAttrByte = ptwnd->crAttrByte;   break;
                                   case 34:       SetFgAttr( ptwnd->crAttrByte, BLUE );        ptwnd->crRealAttrByte = ptwnd->crAttrByte;   break;
                                   case 35:       SetFgAttr( ptwnd->crAttrByte, MAGENTA );     ptwnd->crRealAttrByte = ptwnd->crAttrByte;   break;
                                   case 36:       SetFgAttr( ptwnd->crAttrByte, YELLOW );      ptwnd->crRealAttrByte = ptwnd->crAttrByte;   break;
                                   case 37:       SetFgAttr( ptwnd->crAttrByte, WHITE );       ptwnd->crRealAttrByte = ptwnd->crAttrByte;   break;

                            case 40:        SetBgAttr( ptwnd->crAttrByte, BLACK );        ptwnd->crRealAttrByte = ptwnd->crAttrByte;  break;
                                   case 41:       SetBgAttr( ptwnd->crAttrByte, RED );         ptwnd->crRealAttrByte = ptwnd->crAttrByte;   break;
                                   case 42:       SetBgAttr( ptwnd->crAttrByte, GREEN );       ptwnd->crRealAttrByte = ptwnd->crAttrByte;   break;
                                   case 43:       SetBgAttr( ptwnd->crAttrByte, CYAN );        ptwnd->crRealAttrByte = ptwnd->crAttrByte;   break;
                                   case 44:       SetBgAttr( ptwnd->crAttrByte, BLUE );        ptwnd->crRealAttrByte = ptwnd->crAttrByte;   break;
                                   case 45:       SetBgAttr( ptwnd->crAttrByte, MAGENTA );     ptwnd->crRealAttrByte = ptwnd->crAttrByte;   break;
                                   case 46:       SetBgAttr( ptwnd->crAttrByte, YELLOW );      ptwnd->crRealAttrByte = ptwnd->crAttrByte;   break;
                                   case 47:       SetBgAttr( ptwnd->crAttrByte, WHITE );       ptwnd->crRealAttrByte = ptwnd->crAttrByte;   break;
                                   }
                    ptwnd->nWriteState = 0;
                    }
               else if( ch == 'u' ) {
AN3:           if( !fCixTerminal )
                         {
                         Terminal_RenderLine( hwnd );
                         ptwnd->nSBQXCaret = ptwnd->nSaveCol;
                         ptwnd->nSBQYCaret = ptwnd->nSaveRow;
                         ptwnd->nSBQX = ptwnd->nSBQXCaret - ptwnd->nSBQXShift;
                         ptwnd->nSBQY = ptwnd->nSBQYCaret - ptwnd->nSBQYShift;
                         }
                    ptwnd->nWriteState = 0;
                    }
               else if( ch == 'K' ) {
                    if( !fCixTerminal )
                         {
                         LPSTR lpstr;
                         LPSTR lpAttr = NULL;

                         Terminal_RenderLine( hwnd );
                         ptwnd->nSBQY = ptwnd->nSBQYCaret - ptwnd->nSBQYShift;
                         ASSERT( ptwnd->nSBQY < ptwnd->tp.nBufferSize );
                         ASSERT( ptwnd->nSBQYShift < ptwnd->tp.nBufferSize );
                         ASSERT( ptwnd->nSBQX < ptwnd->tp.nLineWidth );
                         lpstr = ptwnd->lpScrollBuf + ( ptwnd->nSBQYShift * ptwnd->tp.nLineWidth );
                         if( ptwnd->lpColourBuf )
                              lpAttr = ptwnd->lpColourBuf + ( ptwnd->nSBQYShift * ptwnd->tp.nLineWidth );
                         switch( ( ptwnd->nAnsiVal < 1 ) ? 0 : ptwnd->AnsiParmVal[ 0 ] )
                              {
                              case 0:
                                   if( ptwnd->tp.nLineWidth - ptwnd->nSBQX )
                                        {
                                        lpstr += ptwnd->nSBQY * ptwnd->tp.nLineWidth;
                                        memset( lpstr + ptwnd->nSBQX, ' ', ptwnd->tp.nLineWidth - ptwnd->nSBQX );
                                        if( lpAttr )
                                             {
                                             lpAttr += ptwnd->nSBQY * ptwnd->tp.nLineWidth;
                                             memset( lpAttr + ptwnd->nSBQX, ptwnd->crAttrByte, ptwnd->tp.nLineWidth - ptwnd->nSBQX );
                                             }
                                        ptwnd->fCurrentLineRendered = FALSE;
                                        }
                                   break;

                              case 1:
                                   if( ptwnd->nSBQX )
                                        {
                                        lpstr += ptwnd->nSBQY * ptwnd->tp.nLineWidth;
                                        memset( lpstr, ' ', ptwnd->nSBQX );
                                        if( lpAttr )
                                             {
                                             lpAttr += ptwnd->nSBQY * ptwnd->tp.nLineWidth;
                                             memset( lpAttr, ptwnd->crAttrByte, ptwnd->nSBQX );
                                             }
                                        ptwnd->fCurrentLineRendered = FALSE;
                                        }
                                   break;

                              case 2:
                                   lpstr += ptwnd->nSBQY * ptwnd->tp.nLineWidth;
                                   memset( lpstr, ' ', ptwnd->tp.nLineWidth );
                                   if( lpAttr )
                                        {
                                        lpAttr += ptwnd->nSBQY * ptwnd->tp.nLineWidth;
                                        memset( lpAttr, ptwnd->crAttrByte, ptwnd->tp.nLineWidth );
                                        }
                                   ptwnd->fCurrentLineRendered = FALSE;
                                   break;
                              }
                         }
                    ptwnd->nWriteState = 0;
                    }
               else if( isdigit( ch ) ) {
                    ptwnd->AnsiParmVal[ ptwnd->nAnsiVal ] = ch - '0';
                    ptwnd->nWriteState = 3;
                    }
               break;

          case 3:
               if( isdigit( ch ) )
                    {
                    if( ptwnd->nAnsiVal >= 2 )
                         ptwnd->nAnsiVal = 1;
                    ptwnd->AnsiParmVal[ ptwnd->nAnsiVal ] = ( ptwnd->AnsiParmVal[ ptwnd->nAnsiVal ] * 10 ) + ch - '0';
                    if( ptwnd->AnsiParmVal[ ptwnd->nAnsiVal ] > 255 )
                         ptwnd->AnsiParmVal[ ptwnd->nAnsiVal ] = 0;
                    }
               else {
                    ++ptwnd->nAnsiVal;
                    if( ch == ';' )
                         ptwnd->nWriteState = 2;
                    else
                         goto AN1;
                    }
               break;

          case 6:
               ptwnd->nMode = ch - '0';
               ptwnd->nWriteState = 7;
               break;

          case 7:
               if( ch == 'h' )
                    switch( ptwnd->nMode )
                         {
                         case 6: ptwnd->fOriginMode = TRUE; break;
                         }
               else if( ch == 'l' )
                    switch( ptwnd->nMode )
                         {
                         case 6: ptwnd->fOriginMode = FALSE; break;
                         }
               ptwnd->nWriteState = 0;
               break;
          }
}

void FASTCALL HandleCursorRepos( HWND hwnd )
{
     TERMINALWND FAR * ptwnd;
     int nYAdjust;
     int top;

     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     nYAdjust = 0;
     Terminal_RenderLine( hwnd );
     top = ptwnd->fOriginMode ? ptwnd->nSBQYTop : 0;
     if( ptwnd->nAnsiVal > 0 )
          nYAdjust = max( ptwnd->nSBQYSize - 24, 0 );
     ptwnd->nSBQYCaret = ptwnd->nSBQYShift + ( ( ptwnd->nAnsiVal < 1 ) ? top: ptwnd->AnsiParmVal[ 0 ] - 1 ) + nYAdjust;
     ptwnd->nSBQXCaret = ptwnd->nSBQXShift + ( ptwnd->nAnsiVal < 2 ) ? 0: ptwnd->AnsiParmVal[ 1 ] - 1;
     if( ptwnd->nSBQYCaret >= ptwnd->tp.nBufferSize )
          {
          int diff;

          diff = ( ptwnd->nSBQYCaret - ptwnd->tp.nBufferSize ) + 1;
          _fmemmove( ptwnd->lpScrollBuf, ptwnd->lpScrollBuf + ( ptwnd->tp.nLineWidth * diff ), ptwnd->tp.nLineWidth * ( ptwnd->tp.nBufferSize - diff ) );
          memset( ptwnd->lpScrollBuf + ptwnd->tp.nLineWidth * ( ptwnd->tp.nBufferSize - diff ), ' ', ptwnd->tp.nLineWidth * diff );
          if( ptwnd->lpColourBuf )
               {
               _fmemmove( ptwnd->lpColourBuf, ptwnd->lpColourBuf + ( ptwnd->tp.nLineWidth * diff ), ptwnd->tp.nLineWidth * ( ptwnd->tp.nBufferSize - diff ) );
               memset( ptwnd->lpColourBuf + ptwnd->tp.nLineWidth * ( ptwnd->tp.nBufferSize - diff ), ptwnd->crAttrByte, ptwnd->tp.nLineWidth * diff );
               }
          ptwnd->nSBQYShift -= diff;
          ptwnd->nSBQYCaret -= diff;
          ptwnd->nSBQX = ptwnd->nSBQXCaret - ptwnd->nSBQXShift;
          ptwnd->nSBQY = ptwnd->nSBQYCaret - ptwnd->nSBQYShift;
          TextBltToImageBitmap( hwnd, NULL );
          }
     else
          {
          ptwnd->nSBQX = ptwnd->nSBQXCaret - ptwnd->nSBQXShift;
          ptwnd->nSBQY = ptwnd->nSBQYCaret - ptwnd->nSBQYShift;
          }
}

/* This function interprets the character using TTY display mode.
 *
 * This code does nothing to the display, so it can be called without
 * worrying about performance hit.
 */
BOOL gotK1;
BOOL gotK2;
BOOL gotK3;
BOOL gotK4;
BOOL gotK5;

void FASTCALL Terminal_WriteTTYChar( HWND hwnd, char ch )
{
     static char ch1 = 0;
     static char ch2 = 0;
     TERMINALWND FAR * ptwnd;
     static BOOL fZModemFrameTypeSeen = FALSE;
     static BOOL fZModemInitSeen = FALSE;

     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     switch( ch )
          {
          case 0x7F:     /* Delete character is ignored */
          case 0:        /* NUL character is ignored */
               break;

          case 12:
               if( ptwnd->tp.fAnsi )
                    {
                    if( ptwnd->nSBQYExtent + ptwnd->nSBQY + 1 >= ptwnd->tp.nBufferSize )
                         {
                         int nDepth;

                         nDepth = ptwnd->tp.nLineWidth * ( ptwnd->tp.nBufferSize - ptwnd->nSBQY );
                         ASSERT( ptwnd->nSBQY < ptwnd->tp.nBufferSize );
                         _fmemmove( ptwnd->lpScrollBuf, ptwnd->lpScrollBuf + ptwnd->tp.nLineWidth * ptwnd->nSBQY, nDepth );
                         memset( ptwnd->lpScrollBuf + nDepth, ' ', ptwnd->tp.nLineWidth * ptwnd->nSBQY );
                         if( ptwnd->lpColourBuf )
                              {
                              _fmemmove( ptwnd->lpColourBuf, ptwnd->lpColourBuf + ptwnd->tp.nLineWidth * ptwnd->nSBQY, nDepth );
                              memset( ptwnd->lpColourBuf + nDepth, ptwnd->crAttrByte, ptwnd->tp.nLineWidth * ptwnd->nSBQY );
                              }
                         }
                    else {
                         ptwnd->nSBQYExtent += ptwnd->nSBQY;
                         ptwnd->nSBQYShift += ptwnd->nSBQY;
                         }
                    ptwnd->nSBQX = ptwnd->nSBQY = 0;
                    ptwnd->nSBQXCaret = ptwnd->nSBQXShift;
                    ptwnd->nSBQYCaret = ptwnd->nSBQYShift;
                    fZModemInitSeen = FALSE;
                    TextBltToImageBitmap( hwnd, NULL );
                    Terminal_SetScrollRange( hwnd, SB_VERT, 0, ptwnd->nSBQYExtent );
                    SetScrollPos( hwnd, SB_VERT, ptwnd->nSBQYShift, TRUE );
                    }
               else
                    {
                    Terminal_WriteRawChar( hwnd, ch );
                    if( ptwnd->lpLogFile && !ptwnd->lpLogFile->fPaused )
                         Amfile_Write( ptwnd->lpLogFile->fhLog, &ch, 1 );
                    }
               break;

          case BEL:
               if( ptwnd->tp.fSound )
                    AmMessageBeep();
               fZModemInitSeen = FALSE;
               if( ptwnd->lpLogFile && !ptwnd->lpLogFile->fPaused )
                    Amfile_Write( ptwnd->lpLogFile->fhLog, &ch, 1 );
               break;

          case TAB:
               do {
                    ++ptwnd->nSBQXCaret;
                    if( ++ptwnd->nSBQX == ptwnd->nSBQXSize )
                         {
                         Terminal_NewLine( hwnd );
                         ptwnd->nSBQX = 0;
                         ptwnd->nSBQXCaret = 0;
                         }
                    }
               while( ptwnd->nSBQX % 8 );
               fZModemInitSeen = FALSE;
               if( ptwnd->lpLogFile && !ptwnd->lpLogFile->fPaused )
                    Amfile_Write( ptwnd->lpLogFile->fhLog, &ch, 1 );
               break;

          case BS:
               if( ptwnd->nSBQX > 0 ) {
                    --ptwnd->nSBQX;
                    --ptwnd->nSBQXCaret;
                    }
               fZModemInitSeen = FALSE;
               if( ptwnd->lpLogFile && !ptwnd->lpLogFile->fPaused )
                    Amfile_Write( ptwnd->lpLogFile->fhLog, &ch, 1 );
               break;

          case CR:
               ptwnd->nSBQX = 0;
               ptwnd->nSBQXCaret = 0;
               fZModemInitSeen = FALSE;
               if( ptwnd->lpLogFile && !ptwnd->lpLogFile->fPaused )
                    Amfile_Write( ptwnd->lpLogFile->fhLog, &ch, 1 );
               if( !ptwnd->tp.fNewline )
                    break;
               ch = LF;

          case LF:
               /* Added for force an explicit CR with every LF seen.
                * Is this intentional?
                */
               ptwnd->nSBQX = 0;
               ptwnd->nSBQXCaret = 0;
               fZModemInitSeen = FALSE;
               Terminal_NewLine( hwnd );
               if( ptwnd->lpLogFile && !ptwnd->lpLogFile->fPaused )
                    Amfile_Write( ptwnd->lpLogFile->fhLog, &ch, 1 );
               break;
          
#ifdef DOKERMIT
          case 0x1:
               if (gotK1)
               {
                    gotK1 = FALSE;
                    gotK2 = FALSE;
                    gotK3 = FALSE;
                    gotK4 = FALSE;
                    gotK5 = FALSE;
               }else
                    gotK1 = TRUE;
               break;
          case 0x38:
               if (!gotK2 && gotK1)
               {
                    gotK2 = TRUE;
               }else{
                    gotK1 = FALSE;
                    gotK2 = FALSE;
                    gotK3 = FALSE;
                    gotK4 = FALSE;
                    gotK5 = FALSE;
               }
          case 0x20:
               if (!gotK3 && gotK2)
               {
                    gotK3 = TRUE;
               }else{
                    gotK1 = FALSE;
                    gotK2 = FALSE;
                    gotK3 = FALSE;
                    gotK4 = FALSE;
                    gotK5 = FALSE;
               }

          case 0x53:
               if (!gotK4 && gotK3)
               {
                    PostCommand( GetParent(hwnd), IDM_KERMITRECEIVE, 0L );
//                  gotK4 = TRUE;
               }else{
                    gotK1 = FALSE;
                    gotK2 = FALSE;
                    gotK3 = FALSE;
                    gotK4 = FALSE;
                    gotK5 = FALSE;
               }
          case 0x7E:
               if (!gotK5 && gotK4)
               {
                    gotK5 = TRUE;
               }else{
                    gotK1 = FALSE;
                    gotK2 = FALSE;
                    gotK3 = FALSE;
                    gotK4 = FALSE;
                    gotK5 = FALSE;
               }
          case 0x2F:
               if (gotK5)
               {
                    PostCommand( GetParent(hwnd), IDM_KERMITRECEIVE, 0L );
               }else{
                    gotK1 = FALSE;
                    gotK2 = FALSE;
                    gotK3 = FALSE;
                    gotK4 = FALSE;
                    gotK5 = FALSE;
               }
#endif DOKERMIT

          case ZRQINIT + '0':
               if( fZModemFrameTypeSeen )
                    {
                    PostCommand( GetParent(hwnd), IDM_ZMODEMRECEIVE, 0L );
                    fZModemFrameTypeSeen = FALSE;
                    break;
                    }
               else if( fZModemInitSeen )
                    {
                    fZModemInitSeen = FALSE;
                    fZModemFrameTypeSeen = TRUE;
                    }
               goto DF;

          case ZRINIT + '0':
               if( fZModemFrameTypeSeen )
                    {
                    if( !fZModemUploadActive )
                         PostCommand( GetParent(hwnd), IDM_ZMODEMSEND, 0L );
                    fZModemFrameTypeSeen = FALSE;
                    break;
                    }
               goto DF;

          case ZDLE:
               if( !fBlinking ) {
                    if( ch1 == ZPAD && ch2 == ZPAD )
                         fZModemInitSeen = TRUE;
                         break;
                    break;
                    }
               fZModemInitSeen = FALSE;
               goto DF;

          default:
DF:       Terminal_WriteRawChar( hwnd, ch );
               if( ptwnd->lpLogFile && !ptwnd->lpLogFile->fPaused )
                    Amfile_Write( ptwnd->lpLogFile->fhLog, &ch, 1 );
               break;
          }
     ch2 = ch1;
     ch1 = ch;
}

/* This function writes a raw character to the current cursor pos. It does
 * not attempt to 'interpret' control characters, but will automatically
 * wrap round at the end of the line if asked to do so.
 *
 * This code does nothing to the display, so it can be called without
 * worrying about performance hit.
 */
void FASTCALL Terminal_WriteRawChar( HWND hwnd, char ch )
{
     TERMINALWND FAR * ptwnd;

     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     if( ptwnd->nSBQX < ptwnd->tp.nLineWidth )
          {
          LPSTR lpchr;

          /* Pop the character into the scroll buffer. fEnd is set if we're
           * inserting at the end of the line in which case we need to pop a
           * NULL afterwards
           */
          ASSERT( ptwnd->nSBQYCaret < ptwnd->tp.nBufferSize );
          ASSERT( ptwnd->nSBQXCaret < ptwnd->tp.nLineWidth );
          lpchr = ptwnd->lpScrollBuf + ( ptwnd->tp.nLineWidth * ptwnd->nSBQYCaret ) + ptwnd->nSBQXCaret;
          *lpchr = ch;

          /* If in Ansi mode, set the attribute byte too.
           */
          if( ptwnd->lpColourBuf )
               {
               LPSTR lpchr2;

               lpchr2 = ptwnd->lpColourBuf + ( ptwnd->tp.nLineWidth * ptwnd->nSBQYCaret ) + ptwnd->nSBQXCaret;
               *lpchr2 = ptwnd->crAttrByte;
               }

          /* Only need to render line if it is visible */
          if( CharInRect( ptwnd->nSBQXCaret, ptwnd->nSBQYCaret, ptwnd->nSBQXShift, ptwnd->nSBQYShift, ptwnd->nSBQXSize, ptwnd->nSBQYSize ) )
               {
               HDC hdc;
               int x, y;

               x = ( ptwnd->nSBQXCaret - ptwnd->nSBQXShift ) * ptwnd->nCharWidth;
               y = ( ptwnd->nSBQYCaret - ptwnd->nSBQYShift ) * ptwnd->nLineHeight;
               if( ptwnd->fWndHasCaret )
                    HideCaret( hwnd );
               hdc = GetDC( hwnd );
               SelectFont( hdc, ptwnd->tp.hFont );
               SetBkColor( hdc, BgB4ToRGB( ptwnd, GetBgAttr(ptwnd->crAttrByte) ) );
               SetTextColor( hdc, FgB4ToRGB( ptwnd, GetFgAttr(ptwnd->crAttrByte) ) );
               TextOut( hdc, x, y, lpchr, 1 );
               ReleaseDC( hwnd, hdc );
               if( ptwnd->fWndHasCaret )
                    ShowCaret( hwnd );
               ptwnd->fCurrentLineRendered = TRUE;
               }
          else
               ptwnd->fCurrentLineRendered = FALSE;

          /* Move the cursor forward */
          ++ptwnd->nSBQXCaret;
          if( ++ptwnd->nSBQX == ptwnd->tp.nLineWidth )
               if( ptwnd->tp.fAutoWrap )
                    {
                    ptwnd->nSBQX = 0;
                    ptwnd->nSBQXCaret = 0;
                    Terminal_NewLine( hwnd );
                    }
               else {
                    --ptwnd->nSBQX;
                    --ptwnd->nSBQXCaret;
                    }

          /* Handle the horizontal extent */
          if( ptwnd->nSBQX > ptwnd->nSBQXSize )
               if( ptwnd->nSBQXCaret > ptwnd->nSBQXExtent )
                    {
                    ptwnd->nSBQXExtent = ptwnd->nSBQXCaret;
                    Terminal_SetScrollRange( hwnd, SB_HORZ, 0, ptwnd->nSBQXExtent );
                    SetScrollPos( hwnd, SB_HORZ, ptwnd->nSBQXShift, TRUE );
                    }
          }
}

void FASTCALL Terminal_RenderScreen( HWND hwnd, int nScroll )
{
     RECT rc;
     BOOL fOffScreen;
     TERMINALWND FAR * ptwnd;

     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent(hwnd) );

     /* Set a flag if the current cursor pos if off-screen.
      */
     fOffScreen = !CharInRect( ptwnd->nSBQXCaret, ptwnd->nSBQYCaret, ptwnd->nSBQXShift, ptwnd->nSBQYShift, ptwnd->nSBQXSize, ptwnd->nSBQYSize );

     /* Now, since we're scrolling, the memory image needs to be adjusted.
      * Scroll the pixels up and invalidate the entire display area.
      */
     GetClientRect( hwnd, &rc );
     rc.top = ptwnd->nSBQYTop * ptwnd->nLineHeight;
     rc.bottom = ( ptwnd->nSBQYBottom + 1 ) * ptwnd->nLineHeight;
     if( ptwnd->fWndHasCaret )
          HideCaret( hwnd );
     ScrollWindow( hwnd, 0, ptwnd->nLineHeight * nScroll, &rc, &rc );
     UpdateWindow( hwnd );
     if( ptwnd->fWndHasCaret )
          ShowCaret( hwnd );
}

void FASTCALL Terminal_DrawLine( HWND hwnd, int x, int y, LPRECT lprc, LPSTR lpLine, LPSTR lpAttr, int nLength )
{
     HDC hdc;
     TERMINALWND FAR * ptwnd;

     MUST_BE_CHILD_WND(hwnd);
     hdc = GetDC( hwnd );
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     SelectFont( hdc, ptwnd->tp.hFont );
     if( !lpAttr )
          {
          SetTextColor( hdc, FgB4ToRGB( ptwnd, GetFgAttr( ptwnd->crRealAttrByte ) ) );
          SetBkColor( hdc, BgB4ToRGB( ptwnd, GetBgAttr( ptwnd->crRealAttrByte ) ) );
          ExtTextOut( hdc, x, y, ETO_OPAQUE, lprc, lpLine, nLength, NULL );
          }
     else
          {
          BYTE crAttr;
          RECT rc;
          register int s = 0;

          rc = *lprc;
          while( s < nLength )
               {
               register int c = 0;

               crAttr = lpAttr[ s ];
               c = 1;
               while( c + s < nLength && lpAttr[ c + s ] == crAttr )
                    ++c;
               SetBkColor( hdc, BgB4ToRGB( ptwnd, (BYTE)( ( crAttr >> 4 ) & 0x07 ) ) );
               SetTextColor( hdc, FgB4ToRGB( ptwnd, (BYTE)( crAttr & 0x07 ) ) );
               rc.right = rc.left + ( c * ptwnd->nCharWidth );
               ExtTextOut( hdc, x, y, ETO_OPAQUE, &rc, lpLine + s, c, NULL );
               rc.left = rc.right;
               x += c * ptwnd->nCharWidth;
               s += c;
               }
          rc.right = lprc->right;
          ExtTextOut( hdc, x, y, ETO_OPAQUE, &rc, "", 0, NULL );
          }
     ReleaseDC( hwnd, hdc );
}

/* This function renders the current line to the image buffer. It
 * should be called before moving the cursor off the current line
 * or before painting the image buffer to the screen.
 */
void FASTCALL Terminal_RenderLine( HWND hwnd )
{
     TERMINALWND FAR * ptwnd;

     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     if( !ptwnd->fCurrentLineRendered ) {
          LPSTR lpLine;
          LPSTR lpAttr = NULL;
          RECT rc;

          /* For blank lines, replace with a ptr to an empty string
           */
          ASSERT( ptwnd->nSBQYCaret < ptwnd->tp.nBufferSize );
          ASSERT( ptwnd->nSBQXShift < ptwnd->tp.nLineWidth );
          lpLine = ptwnd->lpScrollBuf + ( ptwnd->tp.nLineWidth * ptwnd->nSBQYCaret ) + ptwnd->nSBQXShift;
          if( ptwnd->lpColourBuf )
               lpAttr = ptwnd->lpColourBuf + ( ptwnd->tp.nLineWidth * ptwnd->nSBQYCaret ) + ptwnd->nSBQXShift;

          /* Associate the image buffer with a DC
           */
          GetClientRect( hwnd, &rc );
          rc.top = ( ptwnd->nSBQYCaret - ptwnd->nSBQYShift ) * ptwnd->nLineHeight;
          rc.bottom = rc.top + ptwnd->nLineHeight;
          if( ptwnd->fWndHasCaret )
               HideCaret( hwnd );
          Terminal_DrawLine( hwnd, rc.left, rc.top, &rc, lpLine, lpAttr, ptwnd->tp.nLineWidth - ptwnd->nSBQXShift );
          if( ptwnd->fWndHasCaret )
               ShowCaret( hwnd );
     
          /* Add this line to the update region.
           */
          ptwnd->fCurrentLineRendered = TRUE;
          }
}

/* This function handles the newline case. The cursor is moved to
 * the start of the next line, and the display is scrolled up if
 * the cursor is at the end of the line.
 */
void FASTCALL Terminal_NewLine( HWND hwnd )
{
     TERMINALWND FAR * ptwnd;

     /* Render the line we're leaving if we haven't already done so
      */
     MUST_BE_CHILD_WND(hwnd);
     Terminal_RenderLine( hwnd );

     /* Move to the next line. If this goes off the bottom
      * of the window, scroll the scroll buffer up and adjust the scroll
      * bars.
      */
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     ++ptwnd->nSBQYCaret;
     ++ptwnd->nSBQY;
     if( ptwnd->nSBQY == ptwnd->nSBQYSize || ptwnd->nSBQYCaret == ptwnd->tp.nBufferSize )
          {
          BOOL fScroll;
     
          /* If we hit the bottom of the scroll buffer, its time to lose
           * the first line of the buffer and scroll everything up.
           * HINT: Could replace this stuff with a queue.
           */
          fScroll = ptwnd->nSBQY == ptwnd->nSBQYSize;
          if( ptwnd->nSBQYCaret == ptwnd->tp.nBufferSize )
               {
               _fmemmove( ptwnd->lpScrollBuf, ptwnd->lpScrollBuf + ptwnd->tp.nLineWidth, ptwnd->tp.nLineWidth * ( ptwnd->tp.nBufferSize - 1 ) );
               memset( ptwnd->lpScrollBuf + ptwnd->tp.nLineWidth * ( ptwnd->tp.nBufferSize - 1 ), ' ', ptwnd->tp.nLineWidth );
               if( ptwnd->lpColourBuf )
                    {
                    _fmemmove( ptwnd->lpColourBuf, ptwnd->lpColourBuf + ptwnd->tp.nLineWidth, ptwnd->tp.nLineWidth * ( ptwnd->tp.nBufferSize - 1 ) );
                    memset( ptwnd->lpColourBuf + ptwnd->tp.nLineWidth * ( ptwnd->tp.nBufferSize - 1 ), ptwnd->crAttrByte, ptwnd->tp.nLineWidth );
                    }
               --ptwnd->nSBQYCaret;
               fScroll = TRUE;
               }

          /* Now scroll up the image DC and render the bitmap.
           */
          else if( ptwnd->nSBQY == ptwnd->nSBQYSize && ptwnd->nSBQYTop == 0 )
               ++ptwnd->nSBQYShift;

          if( fScroll )
               {
               BOOL fOffScreen;

               /* Update the extent */
               if( ( ptwnd->nSBQYCaret + 1 ) - ptwnd->nSBQYSize > ptwnd->nSBQYExtent )
                    ptwnd->nSBQYExtent = ( ptwnd->nSBQYCaret + 1 ) - ptwnd->nSBQYSize;
               --ptwnd->nSBQY;

               /* Set a flag if the current cursor pos if off-screen.
                */
               fOffScreen = !CharInRect( ptwnd->nSBQXCaret, ptwnd->nSBQYCaret, 0, ptwnd->nSBQYShift, ptwnd->tp.nLineWidth, ptwnd->nSBQYSize );
               if( !fOffScreen )
                    Terminal_RenderScreen( hwnd, -1 );

               /* Adjust the scroll bars */
               Terminal_SetScrollRange( hwnd, SB_VERT, 0, ptwnd->nSBQYExtent );
               if( !fOffScreen )
                    SetScrollPos( hwnd, SB_VERT, ptwnd->nSBQYShift, TRUE );
               }
          }
}

/* This function adjusts the caret position in the terminal window.
 */
void FASTCALL Terminal_SetCursor( HWND hwnd )
{
     TERMINALWND FAR * ptwnd;

     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     if( ptwnd->fWndHasCaret )
          {
          BOOL fCaretInView;
     
          fCaretInView = CharInRect( ptwnd->nSBQXCaret, ptwnd->nSBQYCaret, ptwnd->nSBQXShift, ptwnd->nSBQYShift, ptwnd->nSBQXSize, ptwnd->nSBQYSize );
          if( fCaretInView && !ptwnd->fHasCaret )
               {
               CreateCaret( hwnd, NULL, ptwnd->nCharWidth, ptwnd->nLineHeight );
               if( ptwnd->fWndHasCaret )
                    ShowCaret( hwnd );
               ptwnd->fHasCaret = TRUE;
               }
          else if( !fCaretInView && ptwnd->fHasCaret )
               {
               if( ptwnd->fWndHasCaret )
                    HideCaret( hwnd );
               DestroyCaret();
               ptwnd->fHasCaret = FALSE;
               }
          if( ptwnd->fHasCaret )
               {
               ptwnd->nSBQX = ptwnd->nSBQXCaret - ptwnd->nSBQXShift;
               ptwnd->nSBQY = ptwnd->nSBQYCaret - ptwnd->nSBQYShift;
               SetCaretPos( ptwnd->nSBQX * ptwnd->nCharWidth, ptwnd->nSBQY * ptwnd->nLineHeight );
               }
          }
}

BOOL FASTCALL CharInRect( int x, int y, int left, int top, int width, int height )
{
     return( x >= left && y >= top && x < left + width && y < top + height );
}

/* This function sets the scroll bar range and automatically disables the
 * scroll bars if the range is set to zero.
 */
void FASTCALL Terminal_SetScrollRange( HWND hwnd, int wType, int min, int max )
{
     if( min == 0 && max == 0 ) {
          SetScrollRange( hwnd, wType, 0, 2000, FALSE );
          EnableScrollBar( hwnd, wType, ESB_DISABLE_BOTH );
          }
     else {
          SetScrollRange( hwnd, wType, min, max, FALSE );
          EnableScrollBar( hwnd, wType, ESB_ENABLE_BOTH );
          }
}

/* This function draws the visible portion of the scroll buffer to the
 * image bitmap. All unused portions are erased to the terminal background
 * colour.
 */
void FASTCALL TextBltToImageBitmap( HWND hwnd, LPRECT lprc )
{
     TERMINALWND FAR * ptwnd;
     register int c;
     RECT rc;
     RECT rcClip;
     LPSTR lpstr;
     LPSTR lpAttr;
     HDC hdc;

     /* We'll be rendering the lot, anyway */
     MUST_BE_CHILD_WND(hwnd);
     ptwnd = GetTerminalWnd( GetParent(hwnd) );
     ptwnd->fCurrentLineRendered = TRUE;
     lpAttr = NULL;

     /* For each line, blt the text into the image buffer */
     GetClientRect( hwnd, &rc );
     if( !lprc )
          {
          CopyRect( &rcClip, &rc );
          lprc = &rcClip;
          }
     rc.top = 0;
     rc.bottom = rc.top + ptwnd->nLineHeight;
     lpstr = ( ptwnd->lpScrollBuf + ( ptwnd->tp.nLineWidth * ptwnd->nSBQYShift ) ) + ptwnd->nSBQXShift;
     if( ptwnd->lpColourBuf )
          lpAttr = ( ptwnd->lpColourBuf + ( ptwnd->tp.nLineWidth * ptwnd->nSBQYShift ) ) + ptwnd->nSBQXShift;
     if( ptwnd->fWndHasCaret )
          HideCaret( hwnd );
     hdc = GetDC( hwnd );
     SetBkColor( hdc, BgB4ToRGB( ptwnd, GetBgAttr( ptwnd->crRealAttrByte ) ) );
     SelectFont( hdc, ptwnd->tp.hFont );
     for( c = ptwnd->nSBQYShift; c < ptwnd->nSBQYSize + ptwnd->nSBQYShift; ++c )
          {
          if( rc.bottom >= lprc->top && rc.top <= lprc->bottom )
               {
               if( c < ptwnd->tp.nBufferSize )
                    Terminal_DrawLine( hwnd, rc.left, rc.top, &rc, lpstr, lpAttr, ptwnd->tp.nLineWidth - ptwnd->nSBQXShift );
               else
                    ExtTextOut( hdc, rc.left, rc.top, ETO_OPAQUE, &rc, "", 0, NULL );
               }
          if( c < ptwnd->tp.nBufferSize )
               {
               lpstr += ptwnd->tp.nLineWidth;
               if( lpAttr )
                    lpAttr += ptwnd->tp.nLineWidth;
               }
          rc.top = rc.bottom;
          rc.bottom = rc.top + ptwnd->nLineHeight;
          }
     if( rc.top < lprc->bottom )
          {
          rc.bottom = lprc->bottom;
          ExtTextOut( hdc, rc.left, rc.top, ETO_OPAQUE, &rc, "", 0, NULL );
          }
     ReleaseDC( hwnd, hdc );
     if( ptwnd->fWndHasCaret )
          ShowCaret( hwnd );
}

BOOL FASTCALL OpenTerminalLog( HWND hwnd, LPSTR lpszLogFile )
{
     HNDFILE fhLog;

     if( ( fhLog = Amfile_Create( lpszLogFile, 0 ) ) == HNDFILE_ERROR )
          FileCreateError( hwnd, lpszLogFile, FALSE, TRUE );
     else
          {
          LPLOGFILE lpNewLogFile;

          INITIALISE_PTR(lpNewLogFile);
          if( fNewMemory( &lpNewLogFile, sizeof( LOGFILE ) ) )
               {
               TERMINALWND FAR * ptwnd;

               ptwnd = GetTerminalWnd( hwnd );
               lpNewLogFile->fPaused = FALSE;
               lpNewLogFile->fhLog = fhLog;
               lpNewLogFile->lpNext = ptwnd->lpLogFile;
               strcpy( lpNewLogFile->szFileName, lpszLogFile );
               ptwnd->lpLogFile = lpNewLogFile;
               SetWindowText( GetDlgItem( hwnd, IDM_OPENCLOSELOG ), GS(IDS_STR597) );
               EnableControl( hwnd, IDM_PAUSERESUMELOG, TRUE );
               ShowTerminalLogFilename( hwnd, ptwnd->lpLogFile );
               return( TRUE );
               }
          Amfile_Close( fhLog );
          }
     return( FALSE );
}

/* This function shows the name of the terminal window log file
 * in the terminal window caption.
 */
void FASTCALL ShowTerminalLogFilename( HWND hwnd, LPLOGFILE lpLogFile )
{
     strcat( strcat( strcpy( lpTmpBuf, GS(IDS_STR259) ), " - " ), lpLogFile->szFileName );
     if( lpLogFile->fPaused )
          strcat( strcat( lpTmpBuf, " " ), GS(IDS_STR612) );
     SetWindowText( hwnd, lpTmpBuf );
}

BOOL FASTCALL CloseTerminalLog( HWND hwnd )
{
     LPLOGFILE lpNextLogFile;
     TERMINALWND FAR * ptwnd;

     ptwnd = GetTerminalWnd( hwnd );
     if( !ptwnd->lpLogFile )
          return( FALSE );
     if( ptwnd->lpLogFile->fPaused )
          SetWindowText( GetDlgItem( hwnd, IDM_PAUSERESUMELOG ), GS(IDS_STR595) );
     EnableControl( hwnd, IDM_PAUSERESUMELOG, FALSE );
     Amfile_Close( ptwnd->lpLogFile->fhLog );
     lpNextLogFile = ptwnd->lpLogFile->lpNext;
     FreeMemory( &ptwnd->lpLogFile );
     ptwnd->lpLogFile = lpNextLogFile;
     SetWindowText( GetDlgItem( hwnd, IDM_OPENCLOSELOG ), GS(IDS_STR598) );
     SetWindowText( hwnd, GS(IDS_STR259) );
     return( TRUE );
}

BOOL FASTCALL ResumeTerminalLog( HWND hwnd )
{
     TERMINALWND FAR * ptwnd;

     ptwnd = GetTerminalWnd( hwnd );
     if( ptwnd->lpLogFile == NULL )
          return( FALSE );
     if( ptwnd->lpLogFile->fPaused )
          {
          SetWindowText( GetDlgItem( hwnd, IDM_PAUSERESUMELOG ), GS(IDS_STR595) );
          ptwnd->lpLogFile->fPaused = FALSE;
          ShowTerminalLogFilename( hwnd, ptwnd->lpLogFile );
          }
     return( TRUE );
}

BOOL FASTCALL PauseTerminalLog( HWND hwnd )
{
     TERMINALWND FAR * ptwnd;

     ptwnd = GetTerminalWnd( hwnd );
     if( ptwnd->lpLogFile == NULL )
          return( FALSE );
     if( !ptwnd->lpLogFile->fPaused )
          {
          SetWindowText( GetDlgItem( hwnd, IDM_PAUSERESUMELOG ), GS(IDS_STR596) );
          ptwnd->lpLogFile->fPaused = TRUE;
          ShowTerminalLogFilename( hwnd, ptwnd->lpLogFile );
          }
     return( TRUE );
}

/* Handles the Terminal connection dialog.
 */
BOOL EXPORT CALLBACK TerminalConnectDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
     CheckDefDlgRecursion( &fDefDlgEx );
     return( SetDlgMsgResult( hwnd, message, TerminalConnectDlg_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the Terminal connection dialog.
 */
LRESULT FASTCALL TerminalConnectDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
     switch( message )
          {
          HANDLE_MSG( hwnd, WM_INITDIALOG, TerminalConnectDlg_OnInitDialog );
          HANDLE_MSG( hwnd, WM_COMMAND, TerminalConnectDlg_OnCommand );

          case WM_ADMHELP:
               HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsTERMINALCONNECT );
               break;

          default:
               return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
          }
     return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL TerminalConnectDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
     LPTERMENTRY lptrm;
     HWND hwndList;
     register int index;
     LPSTR lpsz;

     INITIALISE_PTR(lpsz);

     /* Save the pointer to the connect record.
      */
     ASSERT( lParam != 0 );
     SetWindowLong( hwnd, DWL_USER, lParam );

     /* Fill The listbox with everything from the Terminal Connections
      * section of the configuration file.
      */
     hwndList = GetDlgItem( hwnd, IDD_LIST );
     ASSERT( IsWindow( hwndList ) );
     for( lptrm = lptrmFirst; lptrm; lptrm = lptrm->lptrmNext )
          {
          index = ListBox_AddString( hwndList, lptrm->szName );
          ListBox_SetItemData( hwndList, index, (DWORD)lptrm );
          }

     /* If there is anything in the list, set the selection
      * to the first item. Otherwise disable the Connect, Delete
      * and Edit buttons.
      */
     if( NULL != lptrmFirst )
          ListBox_SetCurSel( hwndList, 0 );
     else
          {
          EnableControl( hwnd, IDOK, FALSE );
          EnableControl( hwnd, IDD_EDIT, FALSE );
          EnableControl( hwnd, IDD_DELETE, FALSE );
          }
     return( TRUE );
}

/* This function handles the WM_COMMAND message. Currently only one control
 * on the dialog, the OK button, dispatches this message.
 */
void FASTCALL TerminalConnectDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
     switch( id )
          {
          case IDD_NEW: {
               TERMINALWND tw;

               /* Create a new entry in the terminal connection
                * settings.
                */
               if( NewTerminalConnect( hwnd, &tw ) )
                    {
                    char szSection[ LEN_TERMNAME+20+1 ];
                    LPTERMENTRY lptrm;
                    HWND hwndList;
                    int index;

                    /* Create a section for this entry.
                     */
                    wsprintf( szSection, "%s Properties", tw.szName );
                    Amuser_WritePPString( szTerminals, tw.szName, szSection );
                    Amuser_WritePPString( szSection, "ConnectionCard", tw.tp.szConnCard );

                    /* Add it to the command table.
                     */
                    lptrm = AddTerminalToCommandTable( tw.szName );

                    /* Add to the listbox.
                     */
                    hwndList = GetDlgItem( hwnd, IDD_LIST );
                    index = ListBox_AddString( hwndList, tw.szName );
                    ListBox_SetItemData( hwndList, index, (DWORD)lptrm );
                    ListBox_SetCurSel( hwndList, index );

                    /* Enable controls that expect one item.
                     */
                    EnableControl( hwnd, IDOK, TRUE );
                    EnableControl( hwnd, IDD_EDIT, TRUE );
                    EnableControl( hwnd, IDD_DELETE, TRUE );
                    }
               break;
               }

          case IDD_EDIT: {
         TERMINALWND tw;
               char szConnect[ LEN_TERMNAME+1 ];
               HWND hwndList;
               int index;

               /* There must be a selection.
                */
               hwndList = GetDlgItem( hwnd, IDD_LIST );
               index = ListBox_GetCurSel( hwndList );
               ASSERT( index != LB_ERR );

               /* Read the connection name
                */
               memset( szConnect, '\0', sizeof( szConnect ) );
               ListBox_GetText( hwndList, index, szConnect );
               ASSERT( szConnect[ sizeof( szConnect ) - 1 ] == '\0' );

               /* Read the properties for the specified
                * connection.
                */
               INITIALISE_PTR(tw.tp.lpcrBgMapTable);
               INITIALISE_PTR(tw.tp.lpcrFgMapTable);
               Terminal_ReadProperties( szConnect, &tw );

               /* Setting the hwnd parameter of the TERMINALWND structure
                * to NULL will stop the properties dialog from attempting
                * to physically apply any changes.
                */
               tw.hwnd = NULL;
               Terminal_Properties( hwnd, &tw );

               /* If the name has changed, redraw the name.
                */
               if( strcmp( tw.szName, szConnect ) != 0 )
                    {
                    char szSection[ LEN_TERMNAME+20+1 ];
                    LPTERMENTRY lptrm;
                    CMDREC cmd;

                    /* Get the pointer to the TERMENTRY for this item.
                     */
                    lptrm = (LPTERMENTRY)ListBox_GetItemData( hwndList, index );
                    strcpy( lptrm->szName, tw.szName );

                    /* Save the renamed entry in the configuration file.
                     */
                    wsprintf( szSection, "%s Properties", tw.szName );
                    Amuser_WritePPString( szTerminals, szConnect, NULL );
                    Amuser_WritePPString( szTerminals, tw.szName, szSection );

                    /* Remove and add back the item from the list box.
                     */
                    ListBox_DeleteString( hwndList, index );
                    index = ListBox_AddString( hwndList, tw.szName );
                    ListBox_SetItemData( hwndList, index, (DWORD)lptrm );
                    ListBox_SetCurSel( hwndList, index );

                    /* Update the command table.
                     */
                    if( CTree_FindCommandName( szConnect, &cmd ) )
                         {
                         cmd.lpszTooltipString = tw.szName;
                         cmd.lpszString = tw.szName;
                         cmd.lpszCommand = tw.szName;
                         CTree_PutCommand( &cmd );
                         }
                    }
               break;
               }

          case IDD_DELETE: {
               char sz[ 200 ];
               char szConnect[ 80 ];
               HWND hwndList;
               int index;

               /* There must be a selection.
                */
               hwndList = GetDlgItem( hwnd, IDD_LIST );
               index = ListBox_GetCurSel( hwndList );
               ASSERT( index != LB_ERR );

               /* Read the connection name
                */
               memset( szConnect, '\0', sizeof( szConnect ) );
               ListBox_GetText( hwndList, index, szConnect );
               ASSERT( szConnect[ sizeof( szConnect ) - 1 ] == '\0' );

               /* Get confirmation.
                */
               wsprintf( sz, GS(IDS_STR685), (LPSTR)szConnect );
               if( fMessageBox( hwnd, 0, sz, MB_YESNO|MB_ICONINFORMATION ) == IDNO )
                    break;

               /* Remove it from the command table.
                */
               RemoveTerminalFromCommandTable( szConnect );

               /* Delete the corresponding entry in the configuration
                * file for this connection.
                */
               Amuser_WritePPString( szTerminals, szConnect, NULL );

               /* Delete it from the listbox.
                */
               if( ListBox_DeleteString( hwndList, index ) == index )
                    --index;
               if( index != LB_ERR )
                    ListBox_SetCurSel( hwndList, index );
               else
                    {
                    EnableControl( hwnd, IDOK, FALSE );
                    EnableControl( hwnd, IDD_EDIT, FALSE );
                    EnableControl( hwnd, IDD_DELETE, FALSE );
                    PostMessage( hwnd, WM_NEXTDLGCTL, (WPARAM)GetDlgItem( hwnd, IDD_NEW ), (LPARAM)TRUE );
                    }
               fCancelToClose( hwnd, IDCANCEL );
               break;
               }

          case IDD_LIST:
               if( codeNotify != LBN_DBLCLK )
                    break;

          case IDOK: {
               char sz[ 200 ];
               char szConnect[ 80 ];
               char * lpsz;
               HWND hwndList;
               int index;

               /* Get the pointer to the user's connect record
                * which we'll fill in with details about the
                * connection.
                */
               lpsz = (LPSTR)GetWindowLong( hwnd, DWL_USER );
               ASSERT( lpsz != NULL );

               /* There must be a selection.
                */
               hwndList = GetDlgItem( hwnd, IDD_LIST );
               index = ListBox_GetCurSel( hwndList );
               ASSERT( index != LB_ERR );

               /* Read the connection name
                */
               memset( szConnect, '\0', sizeof( szConnect ) );
               ListBox_GetText( hwndList, index, szConnect );
               ASSERT( szConnect[ sizeof( szConnect ) - 1 ] == '\0' );

               /* Find the corresponding entry in the configuration
                * file for this connection.
                */
               Amuser_GetPPString( szTerminals, szConnect, "", sz, sizeof(szConnect) );

               /* Fill the connect record structure.
                */
               strcpy( lpsz, szConnect );
               EndDialog( hwnd, TRUE );
               break;
               }

          case IDCANCEL:
               EndDialog( hwnd, FALSE );
               break;
          }
}

/* This function creates a new connection entry.
 */
BOOL FASTCALL NewTerminalConnect( HWND hwnd, TERMINALWND * ptw )
{
     AMPROPSHEETPAGE psp[ 2 ];
     AMPROPSHEETHEADER psh;

     psp[ 0 ].dwSize = sizeof( AMPROPSHEETPAGE );
     psp[ 0 ].dwFlags = PSP_USETITLE;
     psp[ 0 ].hInstance = hInst;
     psp[ 0 ].pszTemplate = MAKEINTRESOURCE( IDDLG_NEWCONNECT_P1 );
     psp[ 0 ].pszIcon = NULL;
     psp[ 0 ].pfnDlgProc = NewConnect_P1;
     psp[ 0 ].pszTitle = "Select a Title For the Connection";
     psp[ 0 ].lParam = (LPARAM)(LPSTR)ptw;

     psp[ 1 ].dwSize = sizeof( AMPROPSHEETPAGE );
     psp[ 1 ].dwFlags = PSP_USETITLE;
     psp[ 1 ].hInstance = hInst;
     psp[ 1 ].pszTemplate = MAKEINTRESOURCE( IDDLG_NEWCONNECT_P2 );
     psp[ 1 ].pszIcon = NULL;
     psp[ 1 ].pfnDlgProc = NewConnect_P2;
     psp[ 1 ].pszTitle = "Choose a Connection Card";
     psp[ 1 ].lParam = (LPARAM)(LPSTR)ptw;

     psh.dwSize = sizeof( AMPROPSHEETHEADER );
     psh.dwFlags = PSH_PROPSHEETPAGE|PSH_WIZARD|PSH_HASHELP;
     if( fMultiLineTabs )
          psh.dwFlags |= PSH_MULTILINETABS;
     psh.hwndParent = hwnd;
     psh.hInstance = hInst;
     psh.pszIcon = NULL;
     psh.pszCaption = "";
     psh.nPages = sizeof( psp ) / sizeof( AMPROPSHEETPAGE );
     psh.nStartPage = 0;
     psh.ppsp = (LPCAMPROPSHEETPAGE)&psp;
     return( Amctl_PropertySheet(&psh ) != -1 );
}

/* This function dispatches messages for the first page of the
 * New Connect wizard.
 */
BOOL EXPORT CALLBACK NewConnect_P1( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
     switch( message )
          {
          HANDLE_DLGMSG( hwnd, WM_INITDIALOG, NewConnect_P1_OnInitDialog );
          HANDLE_DLGMSG( hwnd, WM_COMMAND, NewConnect_P1_OnCommand );
          HANDLE_DLGMSG( hwnd, WM_NOTIFY, NewConnect_P1_OnNotify );
          }
     return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL NewConnect_P1_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
     TERMINALWND * ptw;
     LPCAMPROPSHEETPAGE psp;

     /* Dereference to get the CONNECT structure
      */
     psp = (LPCAMPROPSHEETPAGE)lParam;
     ptw = (TERMINALWND *)psp->lParam;
     SetWindowLong( hwnd, DWL_USER, (LPARAM)ptw );

     /* Set the picture bitmap.
      */
     PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_PIX ), hInst, IDB_NC_PIX );

     /* Prepare the input fields.
      */
     Edit_LimitText( GetDlgItem( hwnd, IDD_CONNECTNAME ), LEN_TERMNAME );
     PropSheet_SetWizButtons( GetParent( hwnd ), 0 );
     return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL NewConnect_P1_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
     switch( id )
          {
          case IDD_CONNECTNAME:
               if( codeNotify == EN_CHANGE )
                    {
                    if( Edit_GetTextLength( hwndCtl ) > 0 )
                         PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_NEXT );
                    else
                         PropSheet_SetWizButtons( GetParent( hwnd ), 0 );
                    }
               break;
          }
}

/* This function handles the WM_NOTIFY message.
 */
LRESULT FASTCALL NewConnect_P1_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
     switch( lpnmhdr->code )
          {
          case PSN_HELP:
               HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsNEWCONNECT_P1 );
               break;

          case PSN_WIZNEXT: {
               TERMINALWND * ptw;
               char sz[ 10 ];

               /* Get the pointer to the properties record.
                */
               ptw = (TERMINALWND *)GetWindowLong( hwnd, DWL_USER );

               /* Retrieve the text to the record and check that
                * it hasn't been used.
                */
               Edit_GetText( GetDlgItem( hwnd, IDD_CONNECTNAME ), ptw->szName, LEN_TERMNAME+1 );
               if( 0 != Amuser_GetPPString( szTerminals, ptw->szName, "", sz, sizeof(sz) ) )
                    {
                    fMessageBox( hwnd, 0, GS(IDS_STR833), MB_OK|MB_ICONEXCLAMATION );
                    HighlightField( hwnd, IDD_CONNECTNAME );
                    return( PSNRET_INVALID_NOCHANGEPAGE );
                    }

               /* Force the Apply button to be disabled.
                */
               return( IDDLG_NEWCONNECT_P2 );
               }
          }
     return( FALSE );
}

/* This function dispatches messages for the second page of the
 * New Connect wizard.
 */
BOOL EXPORT CALLBACK NewConnect_P2( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
     switch( message )
          {
          HANDLE_DLGMSG( hwnd, WM_INITDIALOG, NewConnect_P2_OnInitDialog );
          HANDLE_DLGMSG( hwnd, WM_COMMAND, NewConnect_P2_OnCommand );
          HANDLE_DLGMSG( hwnd, WM_NOTIFY, NewConnect_P2_OnNotify );
          }
     return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL NewConnect_P2_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
     TERMINALWND * ptw;
     LPCAMPROPSHEETPAGE psp;

     /* Dereference to get the CONNECT structure
      */
     psp = (LPCAMPROPSHEETPAGE)lParam;
     ptw = (TERMINALWND *)psp->lParam;
     SetWindowLong( hwnd, DWL_USER, (LPARAM)ptw );

     /* Set the picture bitmap.
      */
     PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_PIX ), hInst, IDB_NC_PIX );

     /* Prepare the input fields.
      */
     FillConnectionCards( hwnd, IDD_LIST, "" );
     return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL NewConnect_P2_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
     switch( id )
          {
          case IDD_CREATENEW: {
               char sz[ LEN_CONNCARD+1 ];

               /* Fire up the New Connection Card wizard to create a
                * new connection card.
                */
               if( NewConnCard( GetParent( hwnd ), sz ) )
                    {
                    HWND hwndList;
                    int index;

                    /* If the listbox is disabled, then delete the
                     * message saying that there are no connection
                     * cards available.
                     */
                    hwndList = GetDlgItem( hwnd, IDD_LIST );
                    index = ComboBox_AddString( hwndList, sz );
                    ComboBox_SetCurSel( hwndList, index );
                    }
               break;
               }
          }
}

/* This function handles the WM_NOTIFY message.
 */
LRESULT FASTCALL NewConnect_P2_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
     switch( lpnmhdr->code )
          {
          case PSN_HELP:
               HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsNEWCONNECT_P2 );
               break;

          case PSN_SETACTIVE:
               PropSheet_SetWizButtons( GetParent( hwnd ), PSWIZB_FINISH|PSWIZB_BACK );
               break;

          case PSN_WIZBACK:
               return( IDDLG_NEWCONNECT_P1 );

          case PSN_WIZFINISH: {
               TERMINALWND * ptw;
               HWND hwndCombo;

               /* Get the pointer to the properties record.
                */
               ptw = (TERMINALWND *)GetWindowLong( hwnd, DWL_USER );

               /* Get the connection card name.
                */
               hwndCombo = GetDlgItem( hwnd, IDD_LIST );
               ComboBox_GetText( hwndCombo, ptw->tp.szConnCard, LEN_CONNCARD+1 );
               return( 0 );
               }
          }
     return( FALSE );
}


/* This function handles the Licence Agreement dialog.
 */
BOOL EXPORT CALLBACK ZStatus( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
     CheckDefDlgRecursion( &fDefDlgEx );
     return( SetDlgMsgResult( hwnd, message, ZStatus_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the ZStatus
 * dialog.
 */
LRESULT FASTCALL ZStatus_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
     switch( message )
          {
          HANDLE_MSG( hwnd, WM_INITDIALOG, ZStatus_OnInitDialog );
          HANDLE_MSG( hwnd, WM_COMMAND, ZStatus_OnCommand );

          default:
               return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
          }
     return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL ZStatus_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
     SetWindowLong( hwnd, DWL_USER, lParam );
     return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL ZStatus_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
     switch( id )
          {
          case IDD_STOP: {
               TERMINALWND FAR * ptwnd;

               ptwnd = (TERMINALWND *)GetWindowLong( hwnd, DWL_USER );
               if( NULL != ptwnd->hxfer )
                    CancelTransfer( ptwnd->hxfer, ptwnd->lpcdev );
               break;
               }
          }
}

/* This function draws a standard 3D frame. It's used by other
 * functions in this DLL, so it has to be as generic as possible.
 * On return, it adjusts the rectangle coordinates so that they
 * point into the frame.
 */
void FASTCALL Draw3DFrame( HDC hdc, RECT FAR * lprc )
{
     HPEN hpen;
     HPEN hpenGray;
     HPEN hpenBlack;
     HPEN hpenLtGray;
     HPEN hpenWhite;

     /* Create some pens.
      */
     hpenGray = CreatePen( PS_SOLID, 0, GetSysColor( COLOR_BTNSHADOW ) );
     hpenBlack = CreatePen( PS_SOLID, 0, RGB( 0, 0, 0 ) );
     hpenLtGray = CreatePen( PS_SOLID, 0, GetSysColor( COLOR_BTNFACE ) );
     hpenWhite = CreatePen( PS_SOLID, 0, GetSysColor( COLOR_BTNHIGHLIGHT ) );
#ifndef WIN32
     ASSERT( NULL != hpenGray );
     ASSERT( NULL != hpenBlack );
     ASSERT( NULL != hpenLtGray );
     ASSERT( NULL != hpenWhite );
#endif

     /* Draw the left and bottom outside edge.
      */
     hpen = SelectPen( hdc, hpenGray );
     MoveToEx( hdc, lprc->left, lprc->bottom - 2, NULL );
     LineTo( hdc, lprc->left, lprc->top );
     LineTo( hdc, lprc->right - 1, lprc->top );

     /* Draw the left and bottom inside edge.
      */
     SelectPen( hdc, hpenBlack );
     MoveToEx( hdc, lprc->left + 1, lprc->bottom - 3, NULL );
     LineTo( hdc, lprc->left + 1, lprc->top + 1 );
     LineTo( hdc, lprc->right - 2, lprc->top + 1 );

     /* Draw the top and right outside edge.
      */
     SelectPen( hdc, hpenWhite );
     MoveToEx( hdc, lprc->left, lprc->bottom - 1, NULL );
     LineTo( hdc, lprc->right - 1, lprc->bottom - 1 );
     LineTo( hdc, lprc->right - 1, lprc->top - 1 );

     /* Draw the top and right inside edge.
      */
     SelectPen( hdc, hpenLtGray );
     MoveToEx( hdc, lprc->left + 1, lprc->bottom - 2, NULL );
     LineTo( hdc, lprc->right - 2, lprc->bottom - 2 );
     LineTo( hdc, lprc->right - 2, lprc->top );

     /* Clean up when we've finished.
      */
     SelectPen( hdc, hpen );
     DeletePen( hpenGray );
     DeletePen( hpenBlack );
     DeletePen( hpenLtGray );
     DeletePen( hpenWhite );
     lprc->left += 2;
     lprc->top += 2;
     lprc->right -= 2;
     lprc->bottom -= 2;
}

void FASTCALL UpdateTerminalBitmaps( void )
{

     LPTERMENTRY lpptrm;
     CMDREC cmd;
     WORD wMyKey = 0;

     for( lpptrm = lptrmFirst; lpptrm; lpptrm = lpptrm->lptrmNext )
     {

     /* Next, create an entry in the command table.
      */
     cmd.iCommand = lpptrm->iCommandID;
     cmd.lpszString = lpptrm->szName;
     cmd.lpszCommand = lpptrm->szName;
     cmd.iDisabledString = 0;
     cmd.lpszTooltipString = lpptrm->szName;
     cmd.iToolButton = TB_TERMINAL;
     cmd.wCategory = CAT_TERMINAL;
     if( CTree_GetCommandKey( lpptrm->szName, &wMyKey ) )
          cmd.wDefKey = wMyKey;
     else
          cmd.wDefKey = 0;
     cmd.iActiveMode = WIN_ALL;
     cmd.hLib = NULL;
     cmd.wNewKey = 0;
     cmd.wKey = 0;
     cmd.nScheduleFlags = NSCHF_NONE;
     SetCommandCustomBitmap( &cmd, BTNBMP_GRID, fNewButtons? 35 : 15  );
     CTree_PutCommand( &cmd );
     }

}
