/* REPORT.C - Ameol report
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
#include "amlib.h"
#include <memory.h>
#include <malloc.h>
#include "htmlhelp.h"
//#ifdef _DEBUG
#include <stdio.h>
#include <stdarg.h>
//#endif
#include <ctype.h>
#include <string.h>
#include <io.h>
#include "ameol2.h"
#include "intcomms.h"
#include "cixob.h"
#include "ssce.h"
#include "editmap.h"

#ifndef USEBIGEDIT
#include "scintilla.h"
#include "scilexer.h"
#endif USEBIGEDIT

#define  THIS_FILE   __FILE__

extern BOOL fWordWrap;                 /* TRUE if we word wrap messages */
extern BOOL fUseINIfile;

#pragma comment(lib, "User32.lib")

#define BUFSIZE 256

typedef void (WINAPI *PGNSI)(LPSYSTEM_INFO);
typedef BOOL (WINAPI *PGPI)(DWORD, DWORD, DWORD, DWORD, PDWORD);

#ifndef PROCESSOR_ARCHITECTURE_INTEL
#define PROCESSOR_ARCHITECTURE_INTEL 0
#endif
#ifndef PROCESSOR_ARCHITECTURE_MIPS
#define PROCESSOR_ARCHITECTURE_MIPS  1
#endif
#ifndef PROCESSOR_ARCHITECTURE_ALPHA
#define PROCESSOR_ARCHITECTURE_ALPHA 2
#endif
#ifndef PROCESSOR_ARCHITECTURE_PPC
#define PROCESSOR_ARCHITECTURE_PPC   3
#endif
#ifndef PROCESSOR_ARCHITECTURE_SHX  
#define PROCESSOR_ARCHITECTURE_SHX   4
#endif
#ifndef PROCESSOR_ARCHITECTURE_ARM
#define PROCESSOR_ARCHITECTURE_ARM   5
#endif
#ifndef PROCESSOR_ARCHITECTURE_IA64
#define PROCESSOR_ARCHITECTURE_IA64  6
#endif
#ifndef PROCESSOR_ARCHITECTURE_ALPHA64
#define PROCESSOR_ARCHITECTURE_ALPHA64 7
#endif
#ifndef PROCESSOR_ARCHITECTURE_MSIL
#define PROCESSOR_ARCHITECTURE_MSIL  8
#endif
#ifndef PROCESSOR_ARCHITECTURE_AMD64
#define PROCESSOR_ARCHITECTURE_AMD64 9
#endif
#ifndef PROCESSOR_ARCHITECTURE_IA32_ON_WIN64
#define PROCESSOR_ARCHITECTURE_IA32_ON_WIN64 10
#endif
#ifndef PROCESSOR_ARCHITECTURE_UNKNOWN
#define PROCESSOR_ARCHITECTURE_UNKNOWN 0xFFFF
#endif

#ifndef SM_SERVERR2
#define SM_SERVERR2 89
#endif

typedef BOOL (FAR PASCAL * VERSIONPROC)(LPSTR, DWORD, BOOL);

char szReportWndClass[] = "amctl_reportwndcls";
char szReportWndName[] = "Report";
char szReportIniFile[] = "Report.ini";

static BOOL fDefDlgEx = FALSE;

static BOOL fRegistered = FALSE;             /* TRUE if we've registered the mail window */
static HWND hwndReport = NULL;                  /* Handle of Report dialog */
static WNDPROC lpEditCtrlProc = NULL;           /* Edit control window procedure address */
static int nAddonsIndex;                     /* Index of addons in category list box */
static char szBugRepEmail[ LEN_INTERNETNAME+1 ];   /* Mail address */

#pragma data_seg("REPORT_XDATA")
static char FAR szAmVersion[ 60 ];        /* Ameol version number */
static char FAR szWinVer[ 100 ];       /* Windows version */ /*!!SM!!*/
static char FAR szCommDriver[ 90 ];       /* Communications driver */
static char FAR szProcessor[ 40 ];        /* Processor type */
static char FAR szToolList[ 1024 ];       /* List of installed tools */

/* Strings used when building report
 */
static char FAR sz1[ 90 ];                /* Priority type */
static char FAR sz2[ 90 ];                /* Ameol version number */
static char FAR sz3[ 120 ];                  /* Communications driver */
static char FAR sz4[ 90 ];                /* Windows version number */
static char FAR sz5[ 70 ];                /* Processor type */
static char FAR sz7[ 90 ];                /* DOS version */
static char FAR sz9[ 60 ];                /* 2nd Header */
static char FAR sz10[ 120 ];              /* Account name */
static char FAR sz11[ 120];                  /* Spell DLL version */

#pragma data_seg()

typedef DWORD (FAR PASCAL * LPVERSIONPROC)( void );

typedef struct tagREPORTWNDINFO {
   HWND hwndFocus;
} REPORTWNDINFO, FAR * LPREPORTWNDINFO;

#define  GetReportWndInfo(h)     (LPREPORTWNDINFO)GetWindowLong((h),DWL_USER)
#define  SetReportWndInfo(h,p)   SetWindowLong((h),DWL_USER,(long)(p))

LRESULT EXPORT CALLBACK ReportWndProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK ReportEditProc( HWND, UINT, WPARAM, LPARAM );

BOOL FASTCALL ReportWnd_OnCreate( HWND, LPCREATESTRUCT );
void FASTCALL ReportWnd_OnClose( HWND );
void FASTCALL ReportWnd_OnSize( HWND, UINT, int, int );
void FASTCALL ReportWnd_OnMove( HWND, int, int );
void FASTCALL ReportWnd_MDIActivate( HWND, BOOL, HWND, HWND );
void FASTCALL ReportWnd_OnCommand( HWND, int, HWND, UINT );
void FASTCALL ReportWnd_OnSetFocus( HWND, HWND );
void FASTCALL ReportWnd_OnAdjustWindows( HWND, int, int );
LRESULT FASTCALL ReportWnd_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr );

BOOL FASTCALL ReportWnd_CreateReport( HWND );
void FASTCALL LoadReportConfiguration( void );
BOOL FASTCALL FindTopic( HWND, PCL, char *, char * );
void FASTCALL ParseFolderTopic( char *, char *, char * );
void FASTCALL CreateCIXReportObject( char *, char *, char *, char * );
void FASTCALL CreateMailReportObject( char *, char *, char * );
BOOL FASTCALL UpdateOKButtonEnable( HWND );

S16 (WINAPI * fSSCE_Version)(S16 *, S16 *);
static WORD wSSCEVersion;                       /* Version number of SSCE library */
static char szSpellVer[ 20 ] = "(not found)";
static char szSpellDictDate[ 20 ] = "(not found)";


/* This function displays the Report window.
 */
BOOL FASTCALL ReportWindow( HWND hwnd )
{
   LPREPORTWNDINFO lpmwi;
   DWORD dwState;
   int cxBorder;
   int cyBorder;
   RECT rc;

   /* If report window already open, bring it to the
    * foreground.
    */
   if( hwndReport )
      {
      BringWindowToTop( hwndReport );
      return( TRUE );
      }

   /* Register the group list window class if we have
    * not already done so.
    */
   if( !fRegistered )
      {
      WNDCLASS wc;

      wc.style       = CS_HREDRAW | CS_VREDRAW;
      wc.lpfnWndProc    = ReportWndProc;
      wc.hIcon       = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_REPORT) );
      wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
      wc.lpszMenuName      = NULL;
      wc.cbWndExtra     = DLGWINDOWEXTRA;
      wc.cbClsExtra     = 0;
      wc.hbrBackground  = (HBRUSH)( COLOR_APPWORKSPACE + 1 );
      wc.lpszClassName  = szReportWndClass;
      wc.hInstance      = hInst;
      if( !RegisterClass( &wc ) )
         return( FALSE );
      fRegistered = TRUE;
      }

   /* The window will appear in the left half of the
    * client area.
    */
   GetClientRect( hwndMDIClient, &rc );
   rc.bottom -= GetSystemMetrics( SM_CYICON );
   cyBorder = ( rc.bottom - rc.top ) / 12;
   cxBorder = ( rc.right - rc.left ) / 12;
   InflateRect( &rc, -cxBorder, -cyBorder );
   dwState = 0;

   /* Get the actual window size, position and state.
    */
   ReadProperWindowState( szReportWndName, &rc, &dwState );

   /* Create the window.
    */
   hwndReport = Adm_CreateMDIWindow( szReportWndName, szReportWndClass, hInst, &rc, dwState, 0L );
   if( NULL == hwndReport )
      return( FALSE );

   /* Store the handle of the destination structure.
    */
   lpmwi = GetReportWndInfo( hwndReport );

   /* Determine where we put the focus.
    */
   lpmwi->hwndFocus = GetDlgItem( hwndReport, IDD_LIST );
   SetFocus( lpmwi->hwndFocus );
   return( TRUE );
}

/* This is the main window procedure.
 */
LRESULT EXPORT CALLBACK ReportWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, ReportWnd_OnCreate );
      HANDLE_MSG( hwnd, WM_CLOSE, ReportWnd_OnClose );
      HANDLE_MSG( hwnd, WM_SIZE, ReportWnd_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, ReportWnd_OnMove );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, Common_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, ReportWnd_MDIActivate );
      HANDLE_MSG( hwnd, WM_COMMAND, ReportWnd_OnCommand );
      HANDLE_MSG( hwnd, WM_MENUSELECT, MainWnd_OnMenuSelect );
      HANDLE_MSG( hwnd, WM_SETFOCUS, ReportWnd_OnSetFocus );
      HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, ReportWnd_OnAdjustWindows );
      HANDLE_MSG( hwnd, WM_CHANGEFONT, Common_ChangeFont );
      HANDLE_MSG( hwnd, WM_NOTIFY, ReportWnd_OnNotify );

      case WM_APPCOLOURCHANGE:
      case WM_TOGGLEHEADERS: {  // 2.25.2031
#ifdef USEBIGEDIT
         SetWindowFont( GetDlgItem( hwnd, IDD_EDIT ), hEditFont, FALSE );
#else USEBIGEDIT
         SetEditorDefaults(GetDlgItem( hwnd, IDD_EDIT ), FALSE, lfEditFont, fWordWrap, fHotLinks , fMsgStyles, crEditText, crEditWnd );
#endif USEBIGEDIT
         break;
         }
      case WM_GETMINMAXINFO: {
         MINMAXINFO FAR * lpMinMaxInfo;
         LRESULT lResult;

         /* Set the minimum tracking size so the client window can never
          * be resized below 24 lines.
          */
         lResult = Adm_DefMDIDlgProc( hwnd, message, wParam, lParam );
         lpMinMaxInfo = (MINMAXINFO FAR *)lParam;
         lpMinMaxInfo->ptMinTrackSize.x = 435;
         lpMinMaxInfo->ptMinTrackSize.y = 295;
         return( lResult );
         }

      default:
         break;
      }
   return( Adm_DefMDIDlgProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_SETFOCUS message.
 */
void FASTCALL ReportWnd_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   LPREPORTWNDINFO lpmwi;
   HWND hwndFocus;

   lpmwi = GetReportWndInfo( hwnd );
   hwndFocus = lpmwi->hwndFocus;
   if( hwndFocus )
      SetFocus( hwndFocus );
   iActiveMode = WIN_EDITS;
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL ReportWnd_OnCreate( HWND hwnd, LPCREATESTRUCT lpCreateStruct )
{
   LPMDICREATESTRUCT lpMDICreateStruct;
   LPREPORTWNDINFO lpmwi;
   LPVOID lpTool;
   HWND hwndList;
   ADDONINFO toolinfo;
   LPSTR lpReportCats;
   UINT cbReportCats;
   LPSTR lp;

   INITIALISE_PTR(lpReportCats);
   INITIALISE_PTR(lpmwi);

   /* Create the dialog window.
    */
   lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
   Adm_MDIDialog( hwnd, MAKEINTRESOURCE(IDDLG_REPORT_NEW), lpMDICreateStruct );

   /* Create a structure to store the mail window info.
    */
   if( !fNewMemory( &lpmwi, sizeof(REPORTWNDINFO) ) )
      return( FALSE );
   SetReportWndInfo( hwnd, lpmwi );
   lpmwi->hwndFocus = NULL;

   /* Set the edit window font.
    */
#ifdef USEBIGEDIT
   SetWindowFont( GetDlgItem( hwnd, IDD_EDIT ), hEditFont, FALSE );
#else USEBIGEDIT
   SetEditorDefaults(GetDlgItem( hwnd, IDD_EDIT ), FALSE, lfEditFont, fWordWrap, fHotLinks , fMsgStyles, crEditText, crEditWnd );
#endif USEBIGEDIT

   /* Read list of categories from REPORT.INI
    */
   strcat( strcpy( lpPathBuf, pszAmeolDir ), szReportIniFile );

   /* Allocate some global memory. Start from 32K.
    */
   cbReportCats = 32768;
   while( cbReportCats > 1024 && !fNewMemory( &lpReportCats, cbReportCats ) )
      cbReportCats -= 1024;

   /* Read list of scripts from SCRIPTS.INI file. We open the file and
    * read each line by line rather than GetProfileString
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   GetPrivateProfileString( "Categories", NULL, "", lp = lpReportCats, cbReportCats, lpPathBuf );
   while( *lp )
      {
      ComboBox_InsertString( hwndList, -1, lp );
      lp += strlen( lp ) + 1;
      }

   /* Append list of installed addons
    */
   lpTool = NULL;
   nAddonsIndex = ComboBox_InsertString( hwndList, -1, "----------------------" );
   while( NULL != ( lpTool = Ameol2_EnumAllTools( lpTool, &toolinfo ) ) )
      if( toolinfo.dwVersion )
         {
         register int c;
         int index;

         lpTmpBuf[ 0 ] = toupper( toolinfo.szFileName[ 0 ] );
         for( c = 1; toolinfo.szFileName[ c ] && toolinfo.szFileName[ c ] != '.'; ++c )
            lpTmpBuf[ c ] = tolower( toolinfo.szFileName[ c ] );
         lpTmpBuf[ c ] = '\0';
         strcat( lpTmpBuf, " Addon" );
         index = ComboBox_InsertString( hwndList, -1, lpTmpBuf );
         ComboBox_SetItemData( hwndList, index, (LPARAM)(LPSTR)toolinfo.hLib );
         }

   /* Make sure nothing is selected!
    */
   ComboBox_SetCurSel( hwndList, CB_ERR );

   /* Read list of severities.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_SEVERITY ) );
   GetPrivateProfileString( "Severity", NULL, "", lp = lpReportCats, cbReportCats, lpPathBuf );
   while( *lp )
      {
      ComboBox_InsertString( hwndList, -1, lp );
      lp += strlen( lp ) + 1;
      }

   /* Make sure nothing is selected!
    */
   ComboBox_SetCurSel( hwndList, CB_ERR );
   FreeMemory( &lpReportCats );

   /* Set steps bitmaps.
    */
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_NUMBER1 ), hRscLib, IDB_NUMBER1 );
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_NUMBER2 ), hRscLib, IDB_NUMBER2 );
   PicCtrl_SetBitmap( GetDlgItem( hwnd, IDD_NUMBER3 ), hRscLib, IDB_NUMBER3 );

   /* Limit the report title field.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_TITLE ), LEN_TITLE-1 );

   /* Subclass the edit control so we can do indents.
    */
   lpEditCtrlProc = SubclassWindow( GetDlgItem( hwnd, IDD_EDIT ), ReportEditProc );
   
   /* Setup the WordWrap function !!SM!!
   */
#ifdef SMWORDBREAK
   SendDlgItemMessage(hwnd, IDD_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)(EDITWORDBREAKPROC)WordBreakProc) ;
#endif SMWORDBREAK

#ifndef USEBIGEDIT
   SendMessage( GetDlgItem( hwnd, IDD_EDIT ), SCI_SETSAVEPOINT,  (WPARAM)0, (LPARAM)0L ); 
#endif USEBIGEDIT
   EnableControl( hwnd, IDOK, FALSE );
   return( TRUE );
}

/* This function handles indents in the edit window.
 */
LRESULT EXPORT CALLBACK ReportEditProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   return( EditCtrlIndent( lpEditCtrlProc, hwnd, msg, wParam, lParam ) );
}

LRESULT FASTCALL ReportWnd_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
{
#ifndef USEBIGEDIT
   if( lpNmHdr->idFrom == IDD_EDIT )
   {
      return Lexer_OnNotify( hwnd, idCode, lpNmHdr );
   }
#endif USEBIGEDIT
   return( FALSE );
}

/* This function processes the WM_COMMAND message.
 */
void FASTCALL ReportWnd_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   LPREPORTWNDINFO lpmwi;

   lpmwi = GetReportWndInfo( hwnd );
   switch( id )
      {
      case IDD_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsREPORT );
         break;

      case IDD_LIST:
      case IDD_SEVERITY:
         if( codeNotify == CBN_SELCHANGE )
            UpdateOKButtonEnable( hwnd );
         break;

      case IDD_EDIT:
      case IDD_TITLE:
         if( codeNotify == EN_UPDATE || codeNotify == EN_CHANGE)
            UpdateOKButtonEnable( hwnd );
         lpmwi->hwndFocus = hwndCtl;
         break;

      case IDM_SPELLCHECK:
         if( Ameol2_SpellCheckDocument( hwnd, GetDlgItem( hwnd, IDD_TITLE ), SC_FL_KEEPSESSION ) == SP_OK )
            Ameol2_SpellCheckDocument( hwnd, GetDlgItem( hwnd, IDD_EDIT ), SC_FL_USELASTSESSION|SC_FL_DIALOG );
         break;

      case IDOK:
         if( !ReportWnd_CreateReport( hwnd ) )
            break;
         Adm_DestroyMDIWindow( hwnd );
         hwndReport = NULL;
         break;

      case IDCANCEL:
         ReportWnd_OnClose( hwnd );
         break;
      }
}

/* Enables or disables the Save button depending on whether all
 * the appropriate information is entered on the report.
 */
BOOL FASTCALL UpdateOKButtonEnable( HWND hwnd )
{
   int index;

   if( (index = ComboBox_GetCurSel( GetDlgItem( hwnd, IDD_SEVERITY ) ) ) == CB_ERR )
      {
      EnableControl( hwnd, IDOK, FALSE );
      return( FALSE );
      }
   else if( (index = ComboBox_GetCurSel( GetDlgItem( hwnd, IDD_LIST ) ) ) == CB_ERR || index == nAddonsIndex )
      {
      EnableControl( hwnd, IDOK, FALSE );
      return( FALSE );
      }
   else if( 0 == Edit_GetTextLength( GetDlgItem( hwnd, IDD_TITLE ) ) )
      {
      EnableControl( hwnd, IDOK, FALSE );
      return( FALSE );
      }
   else if( 0 == Edit_GetTextLength( GetDlgItem( hwnd, IDD_EDIT ) ) )
      {
      EnableControl( hwnd, IDOK, FALSE );
      return( FALSE );
      }
   EnableControl( hwnd, IDOK, TRUE );
   return( TRUE );
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL ReportWnd_OnAdjustWindows( HWND hwnd, int dx, int dy )
{
   Adm_MoveWnd( GetDlgItem( hwnd, IDOK ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDCANCEL ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_HELP ), dx, 0 );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_EDIT ), dx, dy );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_TITLE ), dx, 0 );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_LIST ), dx, 0 );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_LIST_GROUP ), dx, 0 );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_SEVERITY ), dx, 0 );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_SEVERITY_GROUP ), dx, 0 );
   SetEditWindowWidth( GetDlgItem( hwnd, IDD_EDIT ), iWrapCol );
}

/* This function handles the WM_CLOSE message.
 */
void FASTCALL ReportWnd_OnClose( HWND hwnd )
{
   register int n;
   HWND hwndTitle;
   HWND hwndDscp;
   BOOL fReportQuitting;

   fReportQuitting = Ameol2_IsAmeolQuitting();

   hwndTitle = GetDlgItem( hwnd, IDD_TITLE );
   hwndDscp = GetDlgItem( hwnd, IDD_EDIT );
   if( EditMap_GetModified( hwndTitle ) || EditMap_GetModified( hwndDscp ) )
   {
      if( !fReportQuitting && !UpdateOKButtonEnable( hwnd ) )
      {
         n = fMessageBox( hwnd, 0, GS(IDS_STR1240), MB_YESNO|MB_ICONQUESTION );
         if( n == IDNO )
            return;
      }
      else
      {
         n = fMessageBox( hwnd, 0, GS(IDS_STR884), fReportQuitting? MB_YESNO|MB_ICONQUESTION : MB_YESNOCANCEL|MB_ICONQUESTION );
         if( n == IDYES )
            if( !ReportWnd_CreateReport( hwnd ) )
               return;
         if( !fReportQuitting && n == IDCANCEL )
            return;
      }
   }
   Adm_DestroyMDIWindow( hwnd );
   hwndReport = NULL;
}


void FASTCALL fDebugMsg( HWND hwnd, LPCSTR lpszStr, ... )
{
#ifdef _DEBUGREPORT
va_list argptr;
HANDLE   h;
LPSTR buffer;

   h = GlobalAlloc(GHND, 1024);
   buffer = (LPSTR)GlobalLock(h);
   if(buffer == NULL){
      return;
   }
   va_start(argptr, lpszStr);
   vsprintf((char *)buffer, lpszStr, argptr);
   va_end(argptr);
   fMessageBox( hwnd, 0, buffer, MB_OK|MB_ICONEXCLAMATION );
   GlobalUnlock(h);
   GlobalFree(h);
#endif
}

/* This function creates a report.
 */
BOOL FASTCALL ReportWnd_CreateReport( HWND hwnd )
{
   char szTopicName[ LEN_TOPICNAME+1 ];
   char szConfName[ LEN_CONFNAME+1 ];
   char szTitle[ LEN_TITLE ];
   char szAccountName[64];
   ADDONINFO toolinfo;
   int index;
   DWORD dwVersion;
   HWND hwndTitle;
   HWND hwndList;
   register int r;
   char sz[ 200 ];
   HWND hwndDscp;
   LPADDON lpTool;
   UINT cbExtra;
   UINT cchText;
   HGLOBAL gHnd;
   PCL pcl;
   int lUseCIX;

   /* Make sure we're showing the report page
    */
   VERIFY( hwndTitle = GetDlgItem( hwnd, IDD_TITLE ) );
   VERIFY( hwndDscp = GetDlgItem( hwnd, IDD_EDIT ) );


   /* Get the report category.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   index = ComboBox_GetCurSel( hwndList );
   if (index != CB_ERR ) // 20060307 - 2.56.2049.12
   {

   lUseCIX = fUseCIX;
   /* Get the CIX database.
    */
   pcl = NULL;

   /* Load the report configuration.
    */
   LoadReportConfiguration();

   /* Spell check the report
    */
   Edit_SetSel( hwndTitle, 0, 0 );
   Edit_SetSel( hwndDscp, 0, 0 );
   if( fAutoSpellCheck && ( EditMap_GetModified( hwndTitle ) || EditMap_GetModified( hwndDscp ) ) )
   {
      if( ( r = Ameol2_SpellCheckDocument( hwnd, hwndTitle, SC_FL_KEEPSESSION ) ) == SP_CANCEL )
         return( FALSE );
      if( r != SP_FINISH )
         if( Ameol2_SpellCheckDocument( hwnd, hwndDscp, SC_FL_USELASTSESSION ) == SP_CANCEL )
            return( FALSE );
   }

   /* Default mail address.
    */
   strcat( strcpy( lpPathBuf, pszAmeolDir ), szReportIniFile );
   GetPrivateProfileString( "SupportEmail", "Address", "support@cix.uk", szBugRepEmail, sizeof(szBugRepEmail), lpPathBuf );

   /* Decide to which conference to post, depending on whether this is a
    * beta, release or evaluation version.
    */
   dwVersion = CvtToAmeolVersion(Ameol2_GetVersion());

   /* Path to the REPORT.INI file
    */
   strcat( strcpy( lpPathBuf, pszAmeolDir ), szReportIniFile );

   ASSERT( index != CB_ERR );
   if( index > nAddonsIndex )
   {
      HINSTANCE hLib;
      int len;

      fDebugMsg( hwnd, "1 - Addon Report" );

      /* Get default addons conference and topic.
       */
      GetPrivateProfileString( "Addons", "Path", "", lpTmpBuf, LEN_TEMPBUF, lpPathBuf );
      StripLeadingTrailingSpaces( lpTmpBuf );
      ParseFolderTopic( lpTmpBuf, szConfName, szTopicName );

      /* Get the addon's library handle and call the QueryAddon
       * procedure for the support URL.
       */
      hLib = (HINSTANCE)ComboBox_GetItemData( hwndList, index );
      if( GetAddonSupportURL( hLib, lpTmpBuf, LEN_TEMPBUF ) )
      {
         /* If URL is mailto:, then we send the report by e-mail.
          * If it is cix:, we send the report via CIX.
          */
         if( strncmp( lpTmpBuf, "mailto:", 7 ) == 0 )
            strcpy( szBugRepEmail, lpTmpBuf + 7 );
         else if( strncmp( lpTmpBuf, "cix:", 4 ) == 0 )
         {
            char * pTmpBuf;
            register int c;

            /* Extract conference/topic name. If the
             * report is to be posted to the default topic
             * in the support conference, the conference
             * name is omitted. E.g:
             *
             *   cix:/report_addon
             */
            pTmpBuf = lpTmpBuf + 4;
            for( c = 0; *pTmpBuf && *pTmpBuf != '/'; ++pTmpBuf )
               if( c < LEN_CONFNAME+1 ) //FS#154 2054
                  szConfName[ c++ ] = *pTmpBuf;
            if( c > 0 )
               szConfName[ c ] = '\0';
            if( *pTmpBuf == '/' )
               ++pTmpBuf;
            for( c = 0; *pTmpBuf; ++pTmpBuf )
               if( c < LEN_TOPICNAME )
                  szTopicName[ c++ ] = *pTmpBuf;
            szTopicName[ c ] = '\0';

            if (szConfName[0] != '\x0') // 20060307 - 2.56.2049.12
            {
               /* If the conference is present in the msgbase, look
                * for the topic.
                */
               if( NULL != ( pcl = Amdb_OpenFolder( NULL, szConfName ) ) )
               {
                     if ( !FindTopic( hwnd, pcl, szTopicName, szTopicName ) )
                        return FALSE;
               }
            }
         }
      }

      /* Get the addon's name
       */
      ComboBox_GetText( hwndList, szTitle, 9 );
      strcat( szTitle, ": " );
      len = strlen( szTitle );
      GetDlgItemText( hwnd, IDD_TITLE, szTitle + len, LEN_TITLE - len );
   }
   else if (index != CB_ERR) // 20060307 - 2.56.2049.12
   {
      char sz[ 128 ];

      fDebugMsg( hwnd, "2 - Category Report" );

      /* The user selected a category.
       */
      ComboBox_GetLBText( hwndList, index, sz );
      GetPrivateProfileString( "Categories", sz, "", lpTmpBuf, LEN_TEMPBUF, lpPathBuf );
      StripLeadingTrailingSpaces( lpTmpBuf );

      /* !!SM!! 140304
       */
      if( strncmp( lpTmpBuf, "mailto:", 7 ) == 0 )
      {
            fDebugMsg( hwnd, "2a - Mail Report" );
            strcpy( szBugRepEmail, lpTmpBuf + 7 );
            lUseCIX = FALSE;
      }
      else
         ParseFolderTopic( lpTmpBuf, szConfName, szTopicName );
      GetDlgItemText( hwnd, IDD_TITLE, szTitle, LEN_TITLE );
   }

   /* Does the conference exist. If not, we might have to defer to
    * a different one.
    */
   if( lUseCIX )
   {
      fDebugMsg( hwnd, "3 - Sending via CIX" );
      if( NULL == ( pcl = Amdb_OpenFolder( NULL, szConfName ) ) )
      {
         JOINOBJECT jo;
         PCAT pcat;

         Amob_New( OBTYPE_JOINCONFERENCE, &jo );
         jo.wRecent = 10;
         Amob_CreateRefString( &jo.recConfName, szConfName );
         if( !Amob_Find( OBTYPE_JOINCONFERENCE, &jo ) )
            {
            wsprintf( sz, GS(IDS_STR885), (LPSTR)szConfName, (LPSTR)szConfName );
            if( IDNO == fMessageBox( hwnd, 0, sz, MB_YESNO|MB_ICONEXCLAMATION ) )
               return( FALSE );
                // Nick Crashes here
            Amob_Commit( NULL, OBTYPE_JOINCONFERENCE, &jo );
            }
         Amob_Delete( OBTYPE_JOINCONFERENCE, &jo );
         pcat = GetCIXCategory();
         if (!pcat)
         {
            fMessageBox( hwnd, 0, "Error Finding CIX Category", MB_OK|MB_ICONEXCLAMATION );
            return(FALSE); 
         }
         pcl = Amdb_CreateFolder( pcat, szConfName, CFF_SORT );
         fDebugMsg( hwnd, "4 - Created Folder" );
      }

      /* Use the most recent topics.
      */
      if( pcl )
         if( !FindTopic( hwnd, pcl, szTopicName, szTopicName ))
         {
            fDebugMsg( hwnd, "5a - Found Topic %s Rejected", szTopicName);
            return FALSE;
         }
         else
            fDebugMsg( hwnd, "5b - Found Topic %s Accepted", szTopicName );

   }
   
   /* Create the topic in the local message base.
    */
   if( lUseCIX )
   {
      if( pcl )
      {
         if( NULL == Amdb_OpenTopic( pcl, szTopicName ) )
         {
            fDebugMsg( hwnd, "6 - Topic %s not found creating", szTopicName );
            Amdb_CreateTopic( pcl, szTopicName, FTYPE_CIX );
            fDebugMsg( hwnd, "7a - Created Topic %s", szTopicName );
         }
         else
            fDebugMsg( hwnd, "7b - Opening Topic %s", szTopicName );
      }
   }

   fDebugMsg( hwnd, "8 - Formatting Config text" );

   /* Determine account name
    */
   if( !fUseInternet )
      strcpy(szAccountName, GS(IDS_STR1150));
   else if( fUseInternet && strcmp( szProvider, "CIX Internet" ) == 0 )
      strcpy(szAccountName, GS(IDS_STR1152));
   else
      strcpy(szAccountName, GS(IDS_STR1151));

   /* Format configuration strings
    */
   cbExtra = 0;
   cbExtra += wsprintf( sz2, GS(IDS_STR888), (LPSTR)szAmVersion, (LPSTR)(fUseINIfile ? " (Lite)" : "") );
   //cbExtra += wsprintf( sz3, GS(IDS_STR889), (LPSTR)szCommDriver );
   cbExtra += wsprintf( sz4, GS(IDS_STR890), (LPSTR)szWinVer );
   //cbExtra += wsprintf( sz5, GS(IDS_STR891), (LPSTR)szProcessor );
   cbExtra += wsprintf( sz10, GS(IDS_STR1153), (LPSTR)szAccountName );

   fDebugMsg( hwnd, "9 - Enumerating Addons" );
   /* List addons
    */
   lpTool = NULL;
   szToolList[ 0 ] = '\0';
   while( NULL != ( lpTool = Ameol2_EnumAllTools( lpTool, &toolinfo ) ) )
   {
      if( toolinfo.dwVersion )
      {
         if( strlen( szToolList ) + 50 > sizeof( szToolList ) )
         {
            break;
         }
         if( *szToolList == '\0' )
         {
            strcat( szToolList, GS(IDS_STR894) );
         }
         wsprintf( szToolList + strlen( szToolList ), "    %-15s %s\r\n",
                (LPSTR)toolinfo.szFileName,
                Ameol2_ExpandAddonVersion( toolinfo.dwVersion ) );
      }
   }
   cbExtra += strlen( szToolList );

   fDebugMsg( hwnd, "10 - Setting Priority" );
   /* Set the priority
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_SEVERITY ) );
   index = ComboBox_GetCurSel( hwndList );
   ASSERT( index != CB_ERR );
   ComboBox_GetLBText( hwndList, index, lpTmpBuf );

   /* Format the section headers
    */
   cbExtra += wsprintf( sz1, GS(IDS_STR895), lpTmpBuf );
   cbExtra += wsprintf( sz9, GS(IDS_STR896) );

   fDebugMsg( hwnd, "11 - Building O/B Object" );
   /* Now put it altogether and pop into the out-basket
    */
   cchText = Edit_GetTextLength( hwndDscp );
   gHnd = GlobalAlloc( GHND, cchText + cbExtra + 1 );
   if( gHnd )
      {
      LPSTR lpszText;

      lpszText = GlobalLock( gHnd );
      if( lpszText )
         {
         strcpy( lpszText, sz1 );
         Edit_GetText( hwndDscp, lpszText + strlen( lpszText ), cchText + 1 );
         strcat( lpszText, sz9 );
         strcat( lpszText, sz2 );
         strcat( lpszText, sz7 );
         strcat( lpszText, sz4 );
         strcat( lpszText, sz5 );
         strcat( lpszText, sz3 );
         strcat( lpszText, sz10 );
         strcat( lpszText, sz11 );
         strcat( lpszText, szToolList );
         if ( lUseCIX )
            CreateCIXReportObject( szConfName, szTopicName, szTitle, lpszText );
         else
            CreateMailReportObject( szBugRepEmail, szTitle, lpszText );
         GlobalUnlock( gHnd );
         }
      GlobalFree( gHnd );
      }
   }
   else
      fMessageBox( hwnd, 0, GS(IDS_STRING204), MB_OK|MB_ICONEXCLAMATION );

   return( TRUE );
}

/* Split a folder/topic name into its component parts.
 */
void FASTCALL ParseFolderTopic( char * pQualName, char * pConfName, char * pTopicName )
{
   while( *pQualName && *pQualName != '/' )
      *pConfName++ = *pQualName++;
   *pConfName = '\0';
   if( *pQualName == '/' )
      ++pQualName;
   while( *pQualName )
      *pTopicName++ = *pQualName++;
   *pTopicName = '\0';
}

/* This function creates an out-basket object for this report.
 */
void FASTCALL CreateCIXReportObject( char * pszConf, char * pszTopic, char * pszTitle, char * pszText )
{
   SAYOBJECT sob;

   /* We have access to CIX, so send this report via
    * CIX.
    */
   Amob_New( OBTYPE_SAYMESSAGE, &sob );
   Amob_CreateRefString( &sob.recConfName, pszConf );
   Amob_CreateRefString( &sob.recTopicName, pszTopic );
   Amob_CreateRefString( &sob.recSubject, pszTitle );
   Amob_CreateRefString( &sob.recText, pszText );
   Amob_Commit( NULL, OBTYPE_SAYMESSAGE, &sob );
   Amob_Delete( OBTYPE_SAYMESSAGE, &sob );
}

/* This function creates an out-basket object for this report.
 */
void FASTCALL CreateMailReportObject( char * pszMail, char * pszTitle, char * pszText )
{
   MAILOBJECT mob;

   /* No CIX access, so send this report via Internet.
    */
   Amob_New( OBTYPE_MAILMESSAGE, &mob );
   Amob_CreateRefString( &mob.recTo, pszMail );
   Amob_CreateRefString( &mob.recSubject, pszTitle );
   Amob_CreateRefString( &mob.recText, pszText );
   Amob_CreateRefString( &mob.recReplyTo, szMailReplyAddress );
   Amob_Commit( NULL, OBTYPE_MAILMESSAGE, &mob );
   Amob_Delete( OBTYPE_MAILMESSAGE, &mob );
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL ReportWnd_OnMove( HWND hwnd, int x, int y )
{
   Amuser_WriteWindowState( szReportWndName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL ReportWnd_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   /* Save the new window state to the configuration file.
    */
   Amuser_WriteWindowState( szReportWndName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* This function handles the WM_MDIACTIVATE message.
 */
void FASTCALL ReportWnd_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_EDITS ), (LPARAM)(LPSTR)hwnd );
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, Adm_DefMDIDlgProc );
}

/* This function inspects the machine environment and fills
 * out the various globals with details of versions, CPU, etc
 */
void FASTCALL LoadReportConfiguration( void )
{
   SYSTEM_INFO si;
   COMMQUERYSTRUCT cqs;
   DWORD version;
   HNDFILE fh;
   AM_DATE date;
   AM_TIME time;
   WORD uDate;
   WORD uTime;

   /* Get Ameol version number
    */
   version = Ameol2_GetVersion();
   strcpy( szAmVersion, Ameol2_ExpandVersion( version ) );

   /* Get COMM driver version number
    */
   cqs.cbStructSize = sizeof(COMMQUERYSTRUCT);
   Amcomm_Query( NULL, &cqs );
   strcpy( szCommDriver, cqs.szCommDevice );

   /* Get the spell DLL version number and dict date */
   if( !hSpellLib )
      hSpellLib = AmLoadLibrary( szSSCELib );
   if( NULL != hSpellLib )
      {
      S16 maxSSCEVer;
      S16 minSSCEVer;

      /* Start with some version numbering stuff.
       */
      (FARPROC)fSSCE_Version = GetProcAddress( hSpellLib, "SSCE_Version" );

      /* Get the SSCE library version.
       */
      if( NULL != fSSCE_Version )
         {
         fSSCE_Version( &maxSSCEVer, &minSSCEVer );
         wsprintf( szSpellVer, "%d.%d", maxSSCEVer, minSSCEVer );
         }
      
      }

   BEGIN_PATH_BUF;
   wsprintf( lpPathBuf, "%s%sSSCEBRM.CLX", (LPSTR)pszAmeolDir, (LPSTR)"DICT\\" );
   if( ( fh = Amfile_Open( lpPathBuf, AOF_READ ) ) != HNDFILE_ERROR )
      {
      Amfile_GetFileTime( fh, &uDate, &uTime );
      Amdate_UnpackDate( uDate, &date );
      Amdate_UnpackTime( uTime, &time );
      wsprintf( lpTmpBuf, "%s", Amdate_FormatShortDate( &date, NULL ) );
      wsprintf( szSpellDictDate, lpTmpBuf );
      Amfile_Close( fh );
      }
   END_PATH_BUF;

   /* Get Windows, DOS version, processor type and mode
    */
   if (!GetOSDisplayString(szWinVer))
      strcpy(szWinVer, "Unknown");

   /* wProcessorArchitecture

   PROCESSOR_ARCHITECTURE_UNKNOWN
   PROCESSOR_ARCHITECTURE_INTEL
   PROCESSOR_ARCHITECTURE_IA64
   PROCESSOR_ARCHITECTURE_AMD64

   If wProcessorArchitecture is PROCESSOR_ARCHITECTURE_INTEL, wProcessorLevel is defined by the CPU vendor.

   If wProcessorArchitecture is PROCESSOR_ARCHITECTURE_IA64, wProcessorLevel is set to 1.


   wProcessorRevision 

   Architecture-dependent processor revision. The following table shows how the revision value is assembled for each type of processor architecture. 
   Processor Value 
   Intel Pentium, 
   Cyrix, or 
   NextGen 586 

   The high byte is the model and the low byte is the stepping. For example, if the value is xxyy, the model number and stepping can be displayed 
   as follows: 

   Model xx, Stepping yy
    
   Intel 80386 or 80486 A value of the form xxyz. 

   If xx is equal to 0xFF, y - 0xA is the model number, and z is the stepping identifier.
   If xx is not equal to 0xFF, xx + 'A' is the stepping letter and yz is the minor stepping

   */
   GetSystemInfo(&si);
   switch( si.wProcessorArchitecture )
   {
   case PROCESSOR_ARCHITECTURE_INTEL:
   case PROCESSOR_ARCHITECTURE_MIPS:
   case PROCESSOR_ARCHITECTURE_ALPHA:
      {
         strcpy( szProcessor,
            ( si.dwProcessorType == PROCESSOR_INTEL_386 ) ? "Intel 386" :
            ( si.dwProcessorType == PROCESSOR_INTEL_486 ) ? "Intel 486" :
            ( si.dwProcessorType == PROCESSOR_INTEL_PENTIUM ) ? "Intel Pentium" :
            ( si.dwProcessorType == PROCESSOR_MIPS_R4000 ) ? "MIPS R4000" :
            ( si.dwProcessorType == PROCESSOR_ALPHA_21064 ) ? "Alpha 21064" :
            "Unknown" );
         break;
      }
   case PROCESSOR_ARCHITECTURE_PPC:
      {
         strcpy( szProcessor, "PPC");
         break;
      }
   case PROCESSOR_ARCHITECTURE_SHX:
      {
         strcpy( szProcessor, "SHX");
         break;
      }
   case PROCESSOR_ARCHITECTURE_ARM:
      {
         strcpy( szProcessor, "ARM");
         break;
      }
   case PROCESSOR_ARCHITECTURE_IA64:
      {
         strcpy( szProcessor, "Native 64 bit process on IA64");
         break;
      }
   case PROCESSOR_ARCHITECTURE_ALPHA64:
      {
         strcpy( szProcessor, "Alpha 64");
         break;
      }
   case PROCESSOR_ARCHITECTURE_MSIL:
      {
         strcpy( szProcessor, "MSIL");
         break;
      }
   case PROCESSOR_ARCHITECTURE_AMD64:
      {
         strcpy( szProcessor, "Native 64 bit process on AMD64");
         break;
      }
   case PROCESSOR_ARCHITECTURE_IA32_ON_WIN64:
      {
         strcpy( szProcessor, "Intel Architecture 64bit");
         break;
      }
   case PROCESSOR_ARCHITECTURE_UNKNOWN:
      {
         strcpy( szProcessor, "Unknown");
         break;
      }
   }
}

/* This function scans the messagebase and attempts to locate the
 * topic with the highest part number (eg. 3problems, 2problems, problems).
 */
BOOL FASTCALL FindTopic( HWND hwnd, PCL pcl, char * pszTopicName, char * pszOut )
{
   char szTopic[ LEN_TOPICNAME + 1];
   char szTemp[1000];
   register int c;
   DWORD dwMask;
   PTL ptl;
   BOOL lFound;
   TOPICINFO pTopicInfo;
   BOOL lResult;

   lResult = TRUE;
   lFound = FALSE;
   ptl = NULL;

   if( pcl )
   {
      for( c = 999; c >= 1; --c ) /*!!SM!!*/
      { 
//       wsprintf( szTopic, "%c%s", c + '0', (LPSTR)pszTopicName );
         wsprintf( szTopic, "%d%s", c, (LPSTR)pszTopicName );    /*!!SM!!*/
         if( Amdb_OpenTopic( pcl, szTopic ) )
            {
            strcpy( pszOut, szTopic );
            lFound = TRUE;
            }
         }
   }
   if( !lFound && (_stricmp(pszOut, pszTopicName) != 0) )
      strcpy( pszOut, pszTopicName );

   dwMask = 0;
   ptl = Amdb_OpenTopic( pcl, pszOut );
// ptl = Amdb_OpenTopic( pcl, pszTopicName );


   if (ptl)
   {
      Amdb_GetTopicInfo( ptl, &pTopicInfo );

      
      if ( pTopicInfo.dwFlags & TF_READONLY )
      {
         wsprintf(szTemp, "Error: Report Topic %s/%s is marked Read-Only.\n\r\n\rMake sure you are joined to all the latest support topics", Amdb_GetFolderName(pcl), pszOut),
         fMessageBox( hwnd, 0, szTemp, MB_OK|MB_ICONEXCLAMATION );
         lResult = FALSE;
      }
      else if ( pTopicInfo.dwFlags & TF_TOPICFULL )
      {
         wsprintf(szTemp, "Error: Report Topic %s/%s is marked Full.\n\r\n\rMake sure you are joined to all the latest support topics", Amdb_GetFolderName(pcl), pszOut),
         fMessageBox( hwnd, 0, szTemp, MB_OK|MB_ICONEXCLAMATION );
         lResult = FALSE;
      }
      else if ( pTopicInfo.dwFlags & TF_RESIGNED )
      {
         wsprintf(szTemp, "Error: Report Topic %s/%s is marked Resigned.\n\r\n\rYou will not be able to see the report you are posting", Amdb_GetFolderName(pcl), pszOut),
         fMessageBox( hwnd, 0, szTemp, MB_OK|MB_ICONEXCLAMATION );
         lResult = FALSE;
      }
      else if ( pTopicInfo.dwFlags & TF_DELETED )
      {
         wsprintf(szTemp, "Error: Report Topic %s/%s is marked Deleted.\n\r\n\rYou will not be able to see the report you are posting", Amdb_GetFolderName(pcl), pszOut),
         fMessageBox( hwnd, 0, szTemp, MB_OK|MB_ICONEXCLAMATION );
         lResult = FALSE;
      }
   }
   else
   {
      wsprintf(szTemp, "Warning: You are not a member of %s/%s so Ameol cannot determine if this topic is Read/Write or not.\n\rWe recommend that you blink and join %s to download the most recent topics before submitting this report.\n\r\n\rDo you wish to continue to post this report and run the risk of posting to a read-only topic?", 
         Amdb_GetFolderName(pcl), pszOut, Amdb_GetFolderName(pcl));
      if ( fMessageBox( hwnd, 0, szTemp, MB_YESNO|MB_ICONEXCLAMATION ) == IDYES )
         lResult = TRUE;
      else
         lResult = FALSE;
   }
   return lResult;
}

typedef LONG NTSTATUS, *PNTSTATUS;
#define STATUS_SUCCESS (0x00000000)

typedef NTSTATUS (WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

BOOL FASTCALL GetOSDisplayString( LPTSTR pszOS)
{
    HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
    if (hMod) {
        RtlGetVersionPtr fxPtr = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
        if (fxPtr != NULL) {
            RTL_OSVERSIONINFOW rovi = { 0 };
            rovi.dwOSVersionInfoSize = sizeof(rovi);
            if ( STATUS_SUCCESS == fxPtr(&rovi) ) {
                int maxVersion = rovi.dwMajorVersion;
                if (rovi.dwBuildNumber > 22000)
                    maxVersion = 11;
                sprintf(pszOS, "Windows %d.%d build %d", maxVersion, rovi.dwMinorVersion, rovi.dwBuildNumber);
                return TRUE;
            }
        }
    }
    return FALSE;
}
