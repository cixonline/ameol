/* FILELIST.C - Handles CIX file lists.
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
#include "resource.h"
#include <dos.h>
#include <ctype.h>
#include <memory.h>
#include <string.h>
#include "print.h"
#include "amlib.h"
#include "cix.h"
#include <io.h>
#include "ameol2.h"
#include "parse.h"
#include "lbf.h"
#include "rules.h"
#include "webapi.h"

#define  THIS_FILE   __FILE__

extern BOOL fIsAmeol2Scratchpad;

char NEAR szFilelistWndClass[] = "amctl_Filelistwndcls";
char NEAR szFilelistWndName[] = "Filelist";
char NEAR szFlistSearchWndClass[] = "amctl_BunnyHug";
char NEAR szFlistSearchWndName[] = "FlistSearch";

#define  FLTYP_DESCRIPTION       0
#define  FLTYP_FILENAME          1
#define  FLTYP_COMMENT           2

#define  MAX_FIND_STRINGS        10

static BOOL fDefDlgEx = FALSE;

static BOOL fUp;                                /* TRUE if the last selection was with the Up key */
static BOOL fMouseClick;                        /* TRUE if the last selection was via a mouse click */
static int nLastSel;                            /* Index of last selected item */
static WNDPROC lpProcListCtl;                   /* Subclassed listbox window procedure */
static WNDPROC lpProcFListCtl;                  /* Subclassed file find listbox window procedure */
static BOOL fRegistered = FALSE;                /* TRUE if we've registered the comment window */
static int cCopies;                             /* Number of copies to print */
static BOOL fQuickUpdate;                       /* TRUE if we don't paint background of listbox */

HWND hwndFilelist = NULL;

static char * pszFindStr[ MAX_FIND_STRINGS ];   /* Pointers to search strings */
static BOOL fFindStringsLoaded = FALSE;         /* Set TRUE if we've loaded the find strings */
static int cFindStr = 0;                        /* Number of defined search strings */
static char szFindStr[ 100 ];                   /* CURRENT search strings */
static BOOL fCase;                              /* TRUE if search is case sensitive */
static BOOL fWordMatch;                         /* TRUE if we match full words only */
static HWND hwndFlistSearch;                    /* File List Find search result window */
static BOOL fScanning = FALSE;                  /* TRUE if we're scanning flists */
static BOOL fRegisteredFlist = FALSE;           /* TRUE if we've registered the flist search window */
static int hdrColumns[ 4 ];                     /* Header columns */
static int nFileListTab;                        /* Index of file list tab */
static int nFileDownTab;                        /* Index of previous downloads tab */
static int nFileDirTab;                         /* Index of file directory tab */
static BOOL fPrintFlist;                        /* TRUE if we print flist */
static BOOL fPrintFdir;                         /* TRUE if we print fdir */
static PTL ptlFlistWnd = NULL;                  /* Topic in which search originally happened */

BOOL fFileListWndActive;                        /* TRUE if a file list window is active */

typedef struct tagFILELISTWNDINFO {
   HWND hwndFocus;
   HFONT hTitleFont;
   PTL ptl;
} FILELISTWNDINFO, FAR * LPFILELISTWNDINFO;

#define  GetFilelistWndInfo(h)      (LPFILELISTWNDINFO)GetWindowLong((h),DWL_USER)
#define  SetFilelistWndInfo(h,p)    SetWindowLong((h),DWL_USER,(long)(p))

typedef struct tagFILELISTITEM {
   char szFileName[ 16 ];
   char szSize[ 10 ];
   char szDescription[ 100 ];
} FILELISTITEM;

BOOL FASTCALL FilelistWnd_OnCreate( HWND, LPCREATESTRUCT );
void FASTCALL FilelistWnd_OnClose( HWND );
void FASTCALL FilelistWnd_OnSize( HWND, UINT, int, int );
void FASTCALL FilelistWnd_OnMove( HWND, int, int );
void FASTCALL FilelistWnd_MDIActivate( HWND, BOOL, HWND, HWND );
void FASTCALL FilelistWnd_OnCommand( HWND, int, HWND, UINT );
void FASTCALL FilelistWnd_OnSetFocus( HWND, HWND );
void FASTCALL FilelistWnd_OnAdjustWindows( HWND, int, int );
void FASTCALL FilelistWnd_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
void FASTCALL FilelistWnd_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
LRESULT FASTCALL FilelistWnd_OnNotify( HWND, int, LPNMHDR );

BOOL FASTCALL FlistSearchWnd_OnCreate( HWND, LPCREATESTRUCT );
void FASTCALL FlistSearchWnd_OnClose( HWND );
void FASTCALL FlistSearchWnd_OnSize( HWND, UINT, int, int );
void FASTCALL FlistSearchWnd_OnMove( HWND, int, int );
void FASTCALL FlistSearchWnd_OnAdjustWindows( HWND, int, int );
void FASTCALL FlistSearchWnd_OnCommand( HWND, int, HWND, UINT );
void FASTCALL FlistSearchWnd_OnSetFocus( HWND, HWND );
void FASTCALL FlistSearchWnd_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
void FASTCALL FlistSearchWnd_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
BOOL FASTCALL FlistSearchWnd_OnEraseBkgnd( HWND, HDC );
LRESULT FASTCALL FlistSearchWnd_OnNotify( HWND, int, LPNMHDR );

BOOL EXPORT FAR PASCAL FileListFindDlg( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL FileListFindDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL FileListFindDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL FileListFindDlg_OnCommand( HWND, int, HWND, UINT );

LRESULT EXPORT CALLBACK FilelistWndProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK FileListListProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK FlistSearchWndProc( HWND, UINT, WPARAM, LPARAM );

LRESULT EXPORT FAR PASCAL FlistListProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT CALLBACK PrintFilelistDlg( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL PrintFilelistDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL PrintFilelistDlg_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL PrintFilelistDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );

void FASTCALL CmdFilelistPrint( HWND );
BYTE FASTCALL ParseFlistLine( char * );
BOOL FASTCALL FillFileList( HWND, PTL );
BOOL FASTCALL FillFileDirectory( HWND, PTL );
BOOL FASTCALL FillPreviousDownloads( HWND, PTL );
void FASTCALL ParseFileListItem( LPSTR, FILELISTITEM * );
void FASTCALL DoDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
void FASTCALL LoadFileFindStrings( void );

BOOL FASTCALL CreateFlistSearchWindow( HWND, PTL );
int FASTCALL FileListFindCategory( HWND, PCAT );
int FASTCALL FileListFindFolder( HWND, PCL );
int FASTCALL FileListFindTopic( HWND, PTL );

BOOL FASTCALL UpdateCategoryFileLists( PCAT, BOOL * );
BOOL FASTCALL UpdateFolderFileLists( PCL, BOOL * );
BOOL FASTCALL UpdateTopicFileLists( PTL, BOOL *, BOOL );
void FASTCALL UpdateTopicFileDirectory( PTL );

/* This function creates the Filelist window.
 */
BOOL FASTCALL CreateFilelistWindow( HWND hwnd, PTL ptl )
{
   char szFileName[ LEN_CIXFILENAME+1 ];
   LPFILELISTWNDINFO lpfli;
   HWND hwndFocus;
   DWORD dwState;
   int cxBorder;
   int cyBorder;
   RECT rc;

   /* Make sure this is a CIX topic.
    */
   if( Amdb_GetTopicType( ptl ) != FTYPE_CIX )
      return( FALSE );

   /* First check whether or not filelist is
    * available.
    */
   Amdb_GetInternalTopicName( ptl, szFileName );
   strcat( szFileName, ".FLS" );
   if( !Ameol2_QueryFileExists( szFileName, DSD_FLIST ) )
      {
      FILELISTOBJECT co;
      TOPICINFO topicinfo;
      PCL pcl;

      /* Local topic?
       */
      if( Amdb_GetTopicType( ptl ) == FTYPE_LOCAL )
         return( FALSE );
   
      /* Check whether topic is allowed to have a file list
       * If not, warn the user but allow them to override the internal flag. This
       * will be necessary if the topic was just created, but the user hasn't blinked
       * to download any messages yet.
       */
      Amdb_GetTopicInfo( ptl, &topicinfo );
      if( !( topicinfo.dwFlags & TF_HASFILES ) )
         if( fMessageBox( GetFocus(), 0, GS(IDS_STR425), MB_YESNO|MB_ICONQUESTION ) == IDNO )
            return( FALSE );

      /* Set the has-files flag for this topic if it wasn't
       * originally set.
       */
      if( !( topicinfo.dwFlags & TF_HASFILES ) )
         Amdb_SetTopicFlags( ptl, TF_HASFILES, TRUE );

      /* Okay. Create an out-basket object to get the file list.
       */
      pcl = Amdb_FolderFromTopic( ptl );
      Amob_New( OBTYPE_FILELIST, &co );
      Amob_CreateRefString( &co.recConfName, (LPSTR)Amdb_GetFolderName( pcl ) );
      Amob_CreateRefString( &co.recTopicName, (LPSTR)Amdb_GetTopicName( ptl ) );

      /* Commit the out-basket object.
       */
      if( Amob_Find( OBTYPE_FILELIST, &co ) )
         {
         wsprintf( lpTmpBuf, GS(IDS_STR421), Amdb_GetFolderName( pcl ), Amdb_GetTopicName( ptl ) );
         fMessageBox( GetFocus(), 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
         }
      else
         {
         wsprintf( lpTmpBuf, GS(IDS_STR420), Amdb_GetFolderName( pcl ), Amdb_GetTopicName( ptl ) );
         if( fMessageBox( GetFocus(), 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION ) == IDYES )
            Amob_Commit( NULL, OBTYPE_FILELIST, &co );
         }
      return( FALSE );
      }

   /* Register the group list window class if we have
    * not already done so.
    */
   if( !fRegistered )
      {
      WNDCLASS wc;

      wc.style          = CS_HREDRAW | CS_VREDRAW;
      wc.lpfnWndProc    = FilelistWndProc;
      wc.hIcon          = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_FILELIST) );
      wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
      wc.lpszMenuName   = NULL;
      wc.cbWndExtra     = DLGWINDOWEXTRA;
      wc.cbClsExtra     = 0;
      wc.hbrBackground  = (HBRUSH)( COLOR_APPWORKSPACE + 1 );
      wc.lpszClassName  = szFilelistWndClass;
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
   cxBorder = ( rc.right - rc.left ) / 6;
   cyBorder = ( rc.bottom - rc.top ) / 6;
   InflateRect( &rc, -cxBorder, -cyBorder );
   dwState = 0;

   /* Get the actual window size, position and state.
    */
   ReadProperWindowState( szFilelistWndName, &rc, &dwState );

   /* Create the window.
    */
   hwndFilelist = Adm_CreateMDIWindow( szFilelistWndName, szFilelistWndClass, hInst, &rc, dwState, (LPARAM)ptl );
   if( NULL == hwndFilelist )
      return( FALSE );

   /* Show the window.
    */
   SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndFilelist, 0L );

   /* Determine where we put the focus.
    */
   hwndFocus = GetDlgItem( hwndFilelist, IDD_LIST );
   SetFocus( hwndFocus );

   /* Store the handle of the focus window.
    */
   lpfli = GetFilelistWndInfo( hwndFilelist );
   lpfli->hwndFocus = hwndFocus;
   lpfli->ptl = ptl;
   return( TRUE );
}

/* This is the main window procedure.
 */
LRESULT EXPORT CALLBACK FilelistWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, FilelistWnd_OnCreate );
      HANDLE_MSG( hwnd, WM_CLOSE, FilelistWnd_OnClose );
      HANDLE_MSG( hwnd, WM_SIZE, FilelistWnd_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, FilelistWnd_OnMove );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, Common_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, FilelistWnd_MDIActivate );
      HANDLE_MSG( hwnd, WM_COMMAND, FilelistWnd_OnCommand );
      HANDLE_MSG( hwnd, WM_NOTIFY, FilelistWnd_OnNotify );
      HANDLE_MSG( hwnd, WM_MENUSELECT, MainWnd_OnMenuSelect );
      HANDLE_MSG( hwnd, WM_SETFOCUS, FilelistWnd_OnSetFocus );
      HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, FilelistWnd_OnAdjustWindows );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, FilelistWnd_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_DRAWITEM, FilelistWnd_OnDrawItem );

      case WM_GETMINMAXINFO: {
         MINMAXINFO FAR * lpMinMaxInfo;
         LRESULT lResult;

         /* Set the minimum tracking size so the client window can never
          * be resized below 24 lines.
          */
         lResult = Adm_DefMDIDlgProc( hwnd, message, wParam, lParam );
         lpMinMaxInfo = (MINMAXINFO FAR *)lParam;
         lpMinMaxInfo->ptMinTrackSize.x = 340;
         lpMinMaxInfo->ptMinTrackSize.y = 250;
         return( lResult );
         }

      default:
         break;
      }
   return( Adm_DefMDIDlgProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_SETFOCUS message.
 */
void FASTCALL FilelistWnd_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   LPFILELISTWNDINFO lpfli;
   HWND hwndFocus;

   lpfli = GetFilelistWndInfo( hwnd );
   if( hwndFocus = lpfli->hwndFocus )
      SetFocus( hwndFocus );
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL FilelistWnd_OnCreate( HWND hwnd, LPCREATESTRUCT lpCreateStruct )
{
   LPMDICREATESTRUCT lpMDICreateStruct;
   LPFILELISTWNDINFO lpfli;
   HWND hwndList;
   HWND hwndTab;
   int nTabIndex;
   TC_ITEM tie;
   PTL ptl;
   PCL pcl;

   INITIALISE_PTR(lpfli);

   /* Create the dialog window.
    */
   lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
   Adm_MDIDialog( hwnd, MAKEINTRESOURCE(IDDLG_FILELIST), lpMDICreateStruct );

   /* Get the topic handle.
    */
   ptl = (PTL)lpMDICreateStruct->lParam;

   /* Create a structure to store the mail window info.
    */
   if( !fNewMemory( &lpfli, sizeof(FILELISTWNDINFO) ) )
      return( FALSE );
   SetFilelistWndInfo( hwnd, lpfli );
   lpfli->hwndFocus = NULL;

   /* Set the tab control.
    */
   VERIFY( hwndTab = GetDlgItem( hwnd, IDD_TABS ) );
   nTabIndex = 0;
   tie.mask = TCIF_TEXT;
   tie.pszText = "File List";
   nFileListTab = nTabIndex;
   TabCtrl_InsertItem( hwndTab, nTabIndex++, &tie );

   if( Amdb_IsModerator( Amdb_FolderFromTopic( ptl ), szCIXNickname ) )
      {
      tie.pszText = "Directory";
      nFileDirTab = nTabIndex;
      TabCtrl_InsertItem( hwndTab, nTabIndex++, &tie );
      }

   tie.pszText = "Previous Downloads";
   nFileDownTab = nTabIndex;
   TabCtrl_InsertItem( hwndTab, nTabIndex++, &tie );
   SetWindowFont( hwndTab, hHelvB8Font, FALSE );

   /* Create the font we'll use for the title.
    */
   lpfli->hTitleFont = CreateFont( 18, 0, 0, 0, FW_BOLD, FALSE, FALSE,
                           FALSE, ANSI_CHARSET, OUT_CHARACTER_PRECIS,
                           CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
                           VARIABLE_PITCH | FF_SWISS, "Times New Roman" );
   SetWindowFont( GetDlgItem( hwnd, IDD_TITLE ), lpfli->hTitleFont, FALSE );

   /* Set the title bar of the file list window to show the conference/topic
    * to which the file list belongs.
    */
   pcl = Amdb_FolderFromTopic( ptl );
   wsprintf( lpTmpBuf, GS(IDS_STR426), Amdb_GetFolderName( pcl ), Amdb_GetTopicName ( ptl ) );
   SetDlgItemText( hwnd, IDD_TITLE, lpTmpBuf );

   /* Start by filling the list with the file list.
    */
   if( !FillFileList( hwnd, ptl ) )
      return( FALSE );

   /* Subclass the listbox so we can handle cursor keys.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   SetWindowFont( hwndList, hHelv10Font, FALSE );
   lpProcListCtl = SubclassWindow( hwndList, FileListListProc );
   return( TRUE );
}

/* This function fills the list box with the file directory for
 * the specified topic.
 */
BOOL FASTCALL FillFileDirectory( HWND hwnd, PTL ptl )
{
   char szFileName[ LEN_CIXFILENAME+1 ];
   HWND hwndList;
   HNDFILE fh;

   /* Clear the list.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   ListBox_ResetContent( hwndList );

   /* Change the final column description.
    */
   SetDlgItemText( hwnd, IDD_PAD2, "Time Created/Modified" );

   /* Get the topic file directory file name by concatenating .FLD to the
    * internal topic name.
    */
   Amdb_GetInternalTopicName( ptl, szFileName );
   strcat( szFileName, ".FLD" );
   if( ( fh = Ameol2_OpenFile( szFileName, DSD_FLIST, AOF_READ ) ) != HNDFILE_ERROR )
      {
      HCURSOR hOldCursor;
      LPLBF hBuffer;

      /* This topic has a file list.
       */
      Amdb_SetTopicFlags( ptl, TF_HASFILES, TRUE );

      /* Initialise for parsing the file list.
       */
      hOldCursor = SetCursor( hWaitCursor );
      hBuffer = Amlbf_Open( fh, AOF_READ );

      /* Read and discard the first three line
       */
      Amlbf_Read( hBuffer, lpTmpBuf, LEN_TEMPBUF-1, NULL, NULL, &fIsAmeol2Scratchpad );
      Amlbf_Read( hBuffer, lpTmpBuf, LEN_TEMPBUF-1, NULL, NULL, &fIsAmeol2Scratchpad );
      Amlbf_Read( hBuffer, lpTmpBuf, LEN_TEMPBUF-1, NULL, NULL, &fIsAmeol2Scratchpad );

      /* Loop and read the file list file one line at a time.
       */
      while( Amlbf_Read( hBuffer, lpTmpBuf, LEN_TEMPBUF-1, NULL, NULL, &fIsAmeol2Scratchpad ) )
         {
         LPSTR lpText;

         INITIALISE_PTR(lpText);
         if( !fNewMemory( &lpText, strlen( lpTmpBuf ) + 2 ) )
            break;
         lpText[ 0 ] = FLTYP_FILENAME;
         strcpy( lpText + 1, lpTmpBuf );
         ListBox_InsertString( hwndList, -1, lpText );
         }
      Amlbf_Close( hBuffer );
      if( ListBox_GetCount( hwndList ) > 0 )
         {
         ListBox_SetCurSel( hwndList, 0 );
         fUp = fMouseClick = FALSE;
         }
      else
         {
         EnableControl( hwnd, IDD_FIND, FALSE );
         EnableControl( hwnd, IDM_DOWNLOAD, FALSE );
         }
      SetCursor( hOldCursor );
      }

   /* Enable buttons that apply.
    */
   EnableControl( hwnd, IDD_UPDATE, TRUE );
   return( fh != HNDFILE_ERROR );
}

/* This function fills the list box with the file list for
 * the specified topic.
 */
BOOL FASTCALL FillFileList( HWND hwnd, PTL ptl )
{
   char szFileName[ LEN_CIXFILENAME+1 ];
   HWND hwndList;
   HNDFILE fh;

   /* Clear the list.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   ListBox_ResetContent( hwndList );

   /* Change the final column description.
    */
   SetDlgItemText( hwnd, IDD_PAD2, "Description" );

   /* Get the topic file list file name by concatenating .FLS to the
    * internal topic name.
    */
   Amdb_GetInternalTopicName( ptl, szFileName );
   strcat( szFileName, ".FLS" );
   if( ( fh = Ameol2_OpenFile( szFileName, DSD_FLIST, AOF_READ ) ) != HNDFILE_ERROR )
      {
      HCURSOR hOldCursor;
      char szDate[ 80 ];
      register int c;
      LPLBF hBuffer;
      int nFirstSel;
      BOOL fFirst;

      /* This topic has an flist.
       */
      Amdb_SetTopicFlags( ptl, TF_HASFILES, TRUE );

      /* Initialise for parsing the file list.
       */
      hOldCursor = SetCursor( hWaitCursor );
      hBuffer = Amlbf_Open( fh, AOF_READ );
      nLastSel = 0;
      fFirst = TRUE;
      nFirstSel = LB_ERR;

      /* Read and discard the first line
       */
      Amlbf_Read( hBuffer, lpTmpBuf, LEN_TEMPBUF-1, NULL, NULL, &fIsAmeol2Scratchpad );
      for( c = 0; lpTmpBuf[ c ] && lpTmpBuf[ c ] != ' '; ++c );
      ParseCIXDate( lpTmpBuf + c, szDate );
      wsprintf( lpTmpBuf, GS(IDS_STR427), (LPSTR)szDate );
      SetDlgItemText( hwnd, IDD_LASTUPDATED, lpTmpBuf );

      /* Loop and read the file list file one line at a time.
       */
      while( Amlbf_Read( hBuffer, lpTmpBuf, LEN_TEMPBUF-1, NULL, NULL, &fIsAmeol2Scratchpad ) )
         {
         BYTE chType;
         LPSTR lpText;
         int nSel;

         INITIALISE_PTR(lpText);
         chType = ParseFlistLine( lpTmpBuf );
         if( !fNewMemory( &lpText, strlen( lpTmpBuf ) + 2 ) )
            break;
         lpText[ 0 ] = chType;
         lstrcpy( lpText + 1, lpTmpBuf );
         nSel = ListBox_InsertString( hwndList, -1, lpText );

         /* Remember the position of the first filename in the list.
          */
         if( fFirst && chType == FLTYP_FILENAME )
            {
            nFirstSel = nSel;
            fFirst = FALSE;
            }
         }
      Amlbf_Close( hBuffer );
      if( nFirstSel != LB_ERR )
         {
         ListBox_SetCurSel( hwndList, nFirstSel );
         fUp = fMouseClick = FALSE;
         PostDlgCommand( hwnd, IDD_LIST, LBN_SELCHANGE);
         }
      else
         {
         EnableControl( hwnd, IDD_FIND, FALSE );
         EnableControl( hwnd, IDM_DOWNLOAD, FALSE );
         }
      SetCursor( hOldCursor );
      }

   /* Enable buttons that apply.
    */
   EnableControl( hwnd, IDD_UPDATE, TRUE );
   return( fh != HNDFILE_ERROR );
}

/* Analyse the line to determine whether it is a comment, blank or a filename
 * description. In general, anything not fitting a filename description is
 * assumed to be a comment.
 */
BYTE FASTCALL ParseFlistLine( char * pszSrc )
{
   if( *pszSrc == ' ' || *pszSrc == '\t' )
      return( FLTYP_DESCRIPTION );
   while( *pszSrc && *pszSrc != ' ' && *pszSrc != '\t' )
      {
      if( *pszSrc != '.' && !ValidFileNameChr( *pszSrc ) )
         return( FLTYP_COMMENT );
      ++pszSrc;
      }
   while( *pszSrc == ' ' || *pszSrc == '\t' )
      ++pszSrc;
   if( *pszSrc == '\0' )
      return( FLTYP_COMMENT );
   while( *pszSrc && *pszSrc != ' ' && *pszSrc != '\t' )
      {
      if( !isdigit( *pszSrc ) )
         return( FLTYP_COMMENT );
      ++pszSrc;
      }
   while( *pszSrc == ' ' || *pszSrc == '\t' )
      ++pszSrc;
   return( FLTYP_FILENAME );
}

/* This function fills the list box with a list of previous
 * downloads from the specified topic.
 */
BOOL FASTCALL FillPreviousDownloads( HWND hwnd, PTL ptl )
{
   char szFileName[ LEN_CIXFILENAME+1 ];
   HWND hwndList;
   HNDFILE fh;

   /* Clear the list.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   ListBox_ResetContent( hwndList );

   /* Change the final column description.
    */
   SetDlgItemText( hwnd, IDD_PAD2, "Downloaded To" );

   /* Get the previous downloads file list file name by concatenating .FPR
    * to the internal topic name.
    */
   Amdb_GetInternalTopicName( ptl, szFileName );
   strcat( szFileName, ".FPR" );
   if( ( fh = Ameol2_OpenFile( szFileName, DSD_FLIST, AOF_READ ) ) != HNDFILE_ERROR )
      {
      HCURSOR hOldCursor;
      LPLBF hBuffer;

      /* Initialise for parsing the file list.
       */
      hOldCursor = SetCursor( hWaitCursor );
      hBuffer = Amlbf_Open( fh, AOF_READ );

      /* Loop and read the file list file one line at a time.
       */
      while( Amlbf_Read( hBuffer, lpTmpBuf, LEN_TEMPBUF-1, NULL, NULL, &fIsAmeol2Scratchpad ) )
         {
         register int c;
         LPSTR lpText;

         /* Allocate a buffer for this entry.
          */
         INITIALISE_PTR(lpText);
         if( !fNewMemory( &lpText, strlen( lpTmpBuf ) + 2 ) )
            break;
         lpText[ 0 ] = FLTYP_FILENAME;

         /* Replace tabs with single spaces.
          */
         for( c = 0; lpTmpBuf[ c ]; ++c )
            if( lpTmpBuf[ c ] == '\t' )
               lpTmpBuf[ c ] = ' ';

         /* Insert the file details. To ensure that the most recent
          * downloads appear first, we insert at the beginning.
          */
         strcpy( lpText+1, lpTmpBuf );
         ListBox_InsertString( hwndList, 0, lpText );
         }
      Amlbf_Close( hBuffer );
      SetCursor( hOldCursor );
      if( ListBox_GetCount( hwndList ) > 0 )
         {
         ListBox_SetCurSel( hwndList, 0 );
         fUp = fMouseClick = FALSE;
         }
      else
         {
         EnableControl( hwnd, IDD_FIND, FALSE );
         EnableControl( hwnd, IDM_DOWNLOAD, FALSE );
         }
      }

   /* Disable buttons that don't apply.
    */
   EnableControl( hwnd, IDD_UPDATE, FALSE );
   return( fh != HNDFILE_ERROR );
}

/* This is our subclassed listbox procedure. We trap keystrokes and
 * mouse actions to keep track of which direction the user is moving.
 */
LRESULT EXPORT CALLBACK FileListListProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   switch( msg )
      {
      case WM_CONTEXTMENU: /*!!SM!!*/
      case WM_RBUTTONDOWN: {
      HMENU hPopupMenu;
      int index;
      POINT pt;

      /* Select the item under the cursor only if nothing is
       * currently selected.
       */
      index = ItemFromPoint( hwnd, (short)LOWORD(lParam), (short)HIWORD(lParam) );
      if( ListBox_GetSel( hwnd, index ) == 0 )
         {
         CallWindowProc( lpProcListCtl, hwnd, WM_LBUTTONDOWN, wParam, lParam );
         CallWindowProc( lpProcListCtl, hwnd, WM_LBUTTONUP, wParam, lParam );
         }

      /* Display the popup menu.
       */
      hPopupMenu = GetPopupMenu( IPOP_FILELIST );
      GetCursorPos( &pt );
      TrackPopupMenu( hPopupMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndFilelist, NULL );
      break;
      }

      case WM_LBUTTONDOWN:
         fMouseClick = TRUE;
         break;

      case WM_KEYDOWN:
         fMouseClick = FALSE;
         switch( wParam )
            {
            case VK_UP:
            case VK_PRIOR:
            case VK_END:
               fUp = TRUE;
               break;

            case VK_DOWN:
            case VK_NEXT:
            case VK_HOME:
               fUp = FALSE;
               break;
            }
         break;
      }
   return( CallWindowProc( lpProcListCtl, hwnd, msg, wParam, lParam ) );
}

/* This function processes the WM_COMMAND message.
 */
void FASTCALL FilelistWnd_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsFILELIST );
         break;

      case IDM_QUICKPRINT:
      case IDM_PRINT:
         CmdFilelistPrint( hwnd );
         break;

      case IDD_FIND: {
         LPFILELISTWNDINFO lpfli;

         lpfli = GetFilelistWndInfo( hwnd );
         CmdFileListFind( hwnd, lpfli->ptl );
         break;
         }

      case IDD_LIST:
         /* All this, because the Microsoft API doesn't allow you to have
          * disabled items in list boxes!!
          */
         if( codeNotify == LBN_SELCHANGE )
            {
            HWND hwndList;
            int nSel;

            hwndList = GetDlgItem( hwnd, IDD_LIST );
            if( ( nSel = ListBox_GetCurSel( hwndList ) ) != LB_ERR )
               {
               LPSTR lpText;

               lpText = (LPSTR)ListBox_GetItemData( hwndList, nSel );
               if( *lpText != FLTYP_FILENAME )
                  {
                  int count;

                  count = ListBox_GetCount( hwndList );
                  if( fMouseClick )
                     nSel = nLastSel;
                  else if( fUp )
                     while( --nSel >= 0 )
                        {
                        lpText = (LPSTR)ListBox_GetItemData( hwndList, nSel );
                        if( *lpText == FLTYP_FILENAME )
                           break;
                        }
                  else
                     while( ++nSel < count )
                        {
                        lpText = (LPSTR)ListBox_GetItemData( hwndList, nSel );
                        if( *lpText == FLTYP_FILENAME )
                           break;
                        }
                  if( nSel < 0 || nSel >= count )
                     nSel = nLastSel;
                  ListBox_SetCurSel( hwndList, nSel );
                  }
               nLastSel = nSel;
               }
            break;
            }
         if( codeNotify != LBN_DBLCLK )
            break;

      case IDD_DOWNLOAD: 
      case IDM_DOWNLOAD: 
         {
         HWND hwndList;
         int nSel;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         if( ( nSel = ListBox_GetCurSel( hwndList ) ) != LB_ERR )
            {
            LPSTR lpText;

            lpText = (LPSTR)ListBox_GetItemData( hwndList, nSel );
            if( *lpText == FLTYP_FILENAME )
               {
               char szCIXFileName[ LEN_CIXFILENAME+1 ];
               char szDirectory[ _MAX_PATH ];
               LPFILELISTWNDINFO lpfli;
               register int c;
               FDLOBJECT fd;
               PCL pcl;

               /* Get the file list window record.
                */
               lpfli = GetFilelistWndInfo( hwnd );

               /* Extract the filename.
                */
               ++lpText;
               while( *lpText == ' ' || *lpText == '\t' )
                  ++lpText;
               for( c = 0; c <= LEN_CIXFILENAME && lpText[ c ] != ' '; ++c )
                  szCIXFileName[ c ] = lpText[ c ];
               szCIXFileName[ c ] = '\0';

               /* Get the directory associated with this
                * filename.
                */
               GetAssociatedDir( lpfli->ptl, szDirectory, szCIXFileName );

               /* Create a new FDLOBJECT type object.
                */
               pcl = Amdb_FolderFromTopic( lpfli->ptl );
               Amob_New( OBTYPE_DOWNLOAD, &fd );
               Amob_CreateRefString( &fd.recCIXFileName, szCIXFileName );
               Amob_CreateRefString( &fd.recDirectory, szDirectory );
               Amob_CreateRefString( &fd.recConfName, (LPSTR)Amdb_GetFolderName( pcl ) );
               Amob_CreateRefString( &fd.recTopicName, (LPSTR)Amdb_GetTopicName( lpfli->ptl ) );
               if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_FDL), Download, (LPARAM)(LPSTR)&fd ) )
                  {
                  if( Amob_Find( OBTYPE_DOWNLOAD, &fd ) )
                     {
                     wsprintf( lpTmpBuf, GS(IDS_STR424), szCIXFileName );
                     fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
                     }
                  else
                     Amob_Commit( NULL, OBTYPE_DOWNLOAD, &fd );
                  }
               Amob_Delete( OBTYPE_DOWNLOAD, &fd );
               SetFocus( hwndList );
               }
            }
         break;
         }

         case IDM_CIXFILELINK: {
         HWND hwndList;
         int nSel;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         if( ( nSel = ListBox_GetCurSel( hwndList ) ) != LB_ERR )
            {
            LPSTR lpText;

            lpText = (LPSTR)ListBox_GetItemData( hwndList, nSel );
            if( *lpText == FLTYP_FILENAME )
               {
               char szCIXFileName[ LEN_CIXFILENAME+1 ];
               char sz[ 80 ];
               LPFILELISTWNDINFO lpfli;
               register int c;
               PCL pcl;
               HGLOBAL gHnd;

               /* Get the file list window record.
                */
               lpfli = GetFilelistWndInfo( hwnd );

               /* Extract the filename.
                */
               ++lpText;
               while( *lpText == ' ' || *lpText == '\t' )
                  ++lpText;
               for( c = 0; c <= LEN_CIXFILENAME && lpText[ c ] != ' '; ++c )
                  szCIXFileName[ c ] = lpText[ c ];
               szCIXFileName[ c ] = '\0';

               /* Get this folder and topic.
                */
               pcl = Amdb_FolderFromTopic( lpfli->ptl );
               wsprintf( sz, "cixfile:%s/%s:%s", Amdb_GetFolderName( pcl ), Amdb_GetTopicName( lpfli->ptl ), szCIXFileName );

               if( NULL != ( gHnd = GlobalAlloc( GHND, strlen( sz ) + 1 ) ) )
               {
               char * gPtr;


               /* Copy it to the clipboard.
                */
               if( NULL != ( gPtr = GlobalLock( gHnd ) ) )
                  {
                  strcpy( gPtr, sz );
                  GlobalUnlock( gHnd );
                  if( OpenClipboard( hwndFrame ) )
                     {
                     EmptyClipboard();
                     SetClipboardData( CF_TEXT, gHnd );
                     CloseClipboard();
                     break;
                     }
                  }
               GlobalFree( gHnd );
               }

               SetFocus( hwndList );
               }
            }
         break;
         }


      case IDD_UPDATE: {
         LPFILELISTWNDINFO lpfli;

         /* Get the file list window record.
          */
         lpfli = GetFilelistWndInfo( hwnd );
         UpdateFileLists( lpfli->ptl, TRUE );
         break;
         }

      case IDM_COLOURSETTINGS:
         CustomiseDialog( hwnd, CDU_COLOURS );
         break;

      case IDM_FONTSETTINGS:
         CustomiseDialog( hwnd, CDU_FONTS );
         break;


      case IDOK:
      case IDCANCEL:
         /* Close the file list window.
          */
         FilelistWnd_OnClose( hwnd );
         break;
      }
}

/* This function places a command in the out-basket to update the file
 * lists for the specified folder.
 */
void FASTCALL UpdateFileLists( LPVOID pFolder, BOOL fPrompt )
{
   /* If no folder specified, request one visually.
    */
   if( NULL == pFolder )
      {
      CURMSG unr;
      PICKER cp;

      Ameol2_GetCurrentTopic( &unr );
      cp.wType = CPP_TOPICS;
      cp.ptl = unr.ptl;
      if( NULL != cp.ptl )
         cp.pcl = Amdb_FolderFromTopic( cp.ptl );
      strcpy( cp.szCaption, "Update File Lists" );
      if( !Amdb_ConfPicker( hwndFrame, &cp ) )
         return;
      pFolder = NULL != cp.ptl ? (LPVOID)cp.ptl : (LPVOID)cp.pcl;

      /* Check whether topic is allowed to have a file list
       * If not, warn the user but allow them to override the internal flag. This
       * will be necessary if the topic was just created, but the user hasn't blinked
       * to download any messages yet.
       */
      if( Amdb_IsTopicPtr( pFolder ) )
         {
         TOPICINFO topicinfo;

         Amdb_GetTopicInfo( (PTL)pFolder, &topicinfo );
         if( !( topicinfo.dwFlags & TF_HASFILES ) )
            if( fMessageBox( GetFocus(), 0, GS(IDS_STR425), MB_YESNO|MB_ICONQUESTION ) == IDNO )
               return;
         }
      }

   /* Now go deal with the folder.
    */
   if( Amdb_IsCategoryPtr( (PCAT)pFolder ) )
      UpdateCategoryFileLists( (PCAT)pFolder, &fPrompt );
   else if( Amdb_IsFolderPtr( (PCL)pFolder ) )
      UpdateFolderFileLists( (PCL)pFolder, &fPrompt );
   else
      UpdateTopicFileLists( (PTL)pFolder, &fPrompt, TRUE );
}

/* This function places a command in the out-basket to update the file
 * lists for the specified database.
 */
BOOL FASTCALL UpdateCategoryFileLists( PCAT pFolder, BOOL * pfPrompt )
{
   PCL pcl;

   for( pcl = Amdb_GetFirstFolder( (PCAT)pFolder ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
      if( !UpdateFolderFileLists( pcl, pfPrompt ) )
         return( FALSE );
   return( TRUE );
}

/* This function places a command in the out-basket to update the file
 * lists for the specified folder.
 */
BOOL FASTCALL UpdateFolderFileLists( PCL pFolder, BOOL * pfPrompt )
{
   PTL ptlNext;
   PTL ptl;

   for( ptl = Amdb_GetFirstTopic( (PCL)pFolder ); ptl; ptl = ptlNext )
      {
      ptlNext = Amdb_GetNextTopic( ptl );
      if( !UpdateTopicFileLists( ptl, pfPrompt, NULL == ptlNext ) )
         return( FALSE );
      }
   return( TRUE );
}

/* This function places a command in the out-basket to update the file
 * lists for the specified topic.
 */
BOOL FASTCALL UpdateTopicFileLists( PTL ptl, BOOL * pfPrompt, BOOL fLast )
{
   TOPICINFO topicinfo;

   /* Make sure topic has a file list.
    */
   Amdb_GetTopicInfo( ptl, &topicinfo );
/* if( topicinfo.dwFlags & TF_RESIGNED )
      {     
      PCL pcl;
      int retCode;
      HWND hwnd;

      pcl = Amdb_FolderFromTopic( ptl );
      wsprintf( lpTmpBuf, "Topic resigned, rejoin %s/%s?", Amdb_GetFolderName( pcl ), Amdb_GetTopicName( ptl ) );
      retCode = fMessageBox( GetFocus(), 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION );
      if( IDYES == retCode )
         CmdResign( hwnd, NULL, TRUE, TRUE );
      else
         return( TRUE );
      }
*/
   if( topicinfo.dwFlags & TF_HASFILES )  
      {
      FILELISTOBJECT co;
      PCL pcl;

      /* Get folder handle.
       */
      pcl = Amdb_FolderFromTopic( ptl );

      if( topicinfo.dwFlags & TF_RESIGNED )
         {     
         int retCode;
//       HWND hwnd;

         if( *pfPrompt )
            return( TRUE );
         wsprintf( lpTmpBuf, "Topic resigned, rejoin %s/%s?", Amdb_GetFolderName( pcl ), Amdb_GetTopicName( ptl ) );
         retCode = fMessageBox( GetFocus(), 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION );
         if( IDYES == retCode )
            CmdResign( GetFocus(), NULL, TRUE, TRUE );
         else
            return( TRUE );
         }

      Amdb_GetTopicInfo( ptl, &topicinfo );
      if( topicinfo.dwFlags & TF_RESIGNED )
         return( TRUE );



      /* Create an out-basket object to get the file list.
       */
      Amob_New( OBTYPE_FILELIST, &co );
      Amob_CreateRefString( &co.recConfName, (LPSTR)Amdb_GetFolderName( pcl ) );
      Amob_CreateRefString( &co.recTopicName, (LPSTR)Amdb_GetTopicName( ptl ) );

      /* Commit the out-basket object.
       */
      if( Amob_Find( OBTYPE_FILELIST, &co ) )
         {
         if( *pfPrompt )
            {
            wsprintf( lpTmpBuf, GS(IDS_STR423), Amdb_GetFolderName( pcl ), Amdb_GetTopicName( ptl ) );
            fMessageBox( GetFocus(), 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
            }
         }
      else
         {
         if( *pfPrompt )
            {
            int retCode;

            wsprintf( lpTmpBuf, GS(IDS_STR422), Amdb_GetFolderName( pcl ), Amdb_GetTopicName( ptl ) );
            if( fLast )
               retCode = fMessageBox( GetFocus(), 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION );
            else
               retCode = fMessageBox( GetFocus(), 0, lpTmpBuf, MB_YESNOALLCANCEL|MB_ICONQUESTION );
            if( IDNO == retCode )
               {
               Amob_Delete( OBTYPE_FILELIST, &co );
               return( TRUE );
               }
            if( IDCANCEL == retCode )
               {
               Amob_Delete( OBTYPE_FILELIST, &co );
               return( FALSE );
               }
            if( IDALL == retCode )
               *pfPrompt = FALSE;
            }
         Amob_Commit( NULL, OBTYPE_FILELIST, &co );
         if( Amdb_IsModerator( pcl, szCIXNickname ) )
            UpdateTopicFileDirectory( ptl );
         }
      Amob_Delete( OBTYPE_FILELIST, &co );
      }
   return( TRUE );
}

/* This function updates the topic file directory.
 */
void FASTCALL UpdateTopicFileDirectory( PTL ptl )
{
   FDIROBJECT co;
   PCL pcl;

   /* Get folder handle.
    */
   pcl = Amdb_FolderFromTopic( ptl );

   /* Create an out-basket object to get the file directory.
    */
   Amob_New( OBTYPE_FDIR, &co );
   Amob_CreateRefString( &co.recConfName, (LPSTR)Amdb_GetFolderName( pcl ) );
   Amob_CreateRefString( &co.recTopicName, (LPSTR)Amdb_GetTopicName( ptl ) );

   /* Commit the out-basket object.
    */
   if( !Amob_Find( OBTYPE_FDIR, &co ) )
      Amob_Commit( NULL, OBTYPE_FDIR, &co );
   Amob_Delete( OBTYPE_FDIR, &co );
}

/* This function handles notification messages from the
 * tab control.
 */
LRESULT FASTCALL FilelistWnd_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
{
   switch( lpNmHdr->code )
      {
      case TCN_SELCHANGE: {
         LPFILELISTWNDINFO lpfli;
         int nSelection;

         /* Switching between file list and file
          * directory view.
          */
         lpfli = GetFilelistWndInfo( hwnd );
         nSelection = TabCtrl_GetCurSel( lpNmHdr->hwndFrom );
         if( nFileListTab == nSelection )
            FillFileList( hwnd, lpfli->ptl );
         else if( nFileDirTab == nSelection )
            FillFileDirectory( hwnd, lpfli->ptl );
         else if( nFileDownTab == nSelection )
            FillPreviousDownloads( hwnd, lpfli->ptl );
         return( 0L );
         }
      }
   return( FALSE );
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL FilelistWnd_OnAdjustWindows( HWND hwnd, int dx, int dy )
{
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_LIST ), dx, dy );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_TABS ), dx, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDOK ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_DOWNLOAD ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_UPDATE ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_FIND ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_HELP ), dx, 0 );
}

/* This function handles the WM_CLOSE message.
 */
void FASTCALL FilelistWnd_OnClose( HWND hwnd )
{
   LPFILELISTWNDINFO lpfli;
   register int c;
   HWND hwndList;
   int cTotal;

   hwndList = GetDlgItem( hwnd, IDD_LIST );
   cTotal = ListBox_GetCount( hwndList );
   for( c = 0; c < cTotal; ++c )
      {
      LPSTR lpText;

      lpText = (LPSTR)ListBox_GetItemData( hwndList, c );
      FreeMemory( &lpText );
      ListBox_SetItemData( hwndList, c, NULL );
      }
   lpfli = GetFilelistWndInfo( hwnd );
   DeleteFont( lpfli->hTitleFont );
   Adm_DestroyMDIWindow( hwnd );
   hwndFilelist = NULL;

}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL FilelistWnd_OnMove( HWND hwnd, int x, int y )
{
   if( !IsIconic( hwnd ) )
      Amuser_WriteWindowState( szFilelistWndName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL FilelistWnd_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   /* Save the new window state to the configuration file.
    */
   if( !IsIconic( hwnd ) )
      Amuser_WriteWindowState( szFilelistWndName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* This function handles the WM_MDIACTIVATE message.
 */
void FASTCALL FilelistWnd_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   fFileListWndActive = fActive;
   Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_EDITS ), (LPARAM)(LPSTR)hwnd );
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, Adm_DefMDIDlgProc );
}

/* This function handles the WM_MEASUREITEM message.
 */
void FASTCALL FilelistWnd_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   HFONT hOldFont;
   HDC hdc;

   hdc = GetDC( hwnd );
   hOldFont = SelectFont( hdc, hHelv8Font );
   GetTextMetrics( hdc, &tm );
   lpMeasureItem->itemHeight = tm.tmHeight + tm.tmExternalLeading + 2;
   SelectFont( hdc, hOldFont );
   ReleaseDC( hwnd, hdc );
}

/* This function handles the WM_DRAWITEM message.
 */
void FASTCALL FilelistWnd_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   if( lpDrawItem->itemID != -1 )
      DoDrawItem( hwnd, lpDrawItem );
}

/* This function draws one line of the file list box.
 */
void FASTCALL DoDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   if( lpDrawItem->itemID != -1 )
      {
      RECT rcListWnd;
      RECT rcCol1Wnd;
      RECT rcCol2Wnd;
      FILELISTITEM fli;
      HWND hwndList;
      HFONT hOldFont;
      LPSTR lpText;
      RECT rc;
      COLORREF T;
      COLORREF B;
      HBRUSH hbr;

      rcCol1Wnd.left = 0;
      rcCol2Wnd.left = 0;
      hwndList = GetDlgItem( hwnd, IDD_LIST );
      rc = lpDrawItem->rcItem;
      lpText = (LPSTR)ListBox_GetItemData( hwndList, lpDrawItem->itemID );
      if( !lpText )
         return;
      ParseFileListItem( lpText, &fli );
      if( ( lpDrawItem->itemState & ODS_SELECTED ) && *lpText == FLTYP_FILENAME )
         {
         T = GetSysColor( COLOR_HIGHLIGHTTEXT );
         B = GetSysColor( COLOR_HIGHLIGHT );
         }
      else
         {
         T = GetSysColor( COLOR_WINDOWTEXT );
         B = GetSysColor( COLOR_WINDOW );
         }
      hbr = CreateSolidBrush( B );
      SetTextColor( lpDrawItem->hDC, T );
      SetBkColor( lpDrawItem->hDC, B );
      if( lpDrawItem->itemAction == ODA_DRAWENTIRE || lpDrawItem->itemAction == ODA_SELECT )
         {
         char * p;

         /* Get the left coordinate of the list box
          */
         GetWindowRect( hwndList, &rcListWnd );
         ScreenToClient( hwnd, (LPPOINT)&rcListWnd );
         ScreenToClient( hwnd, ((LPPOINT)&rcListWnd) + 1 );

         /* Get the coordinates of the left edge of the second column
          */
         GetWindowRect( GetDlgItem( hwnd, IDD_PAD1 ), &rcCol1Wnd );
         ScreenToClient( hwndList, (LPPOINT)&rcCol1Wnd );
         ScreenToClient( hwndList, ((LPPOINT)&rcCol1Wnd)+1 );

         /* Get the coordinates of the left edge of the third column
          */
         GetWindowRect( GetDlgItem( hwnd, IDD_PAD2 ), &rcCol2Wnd );
         ScreenToClient( hwndList, (LPPOINT)&rcCol2Wnd );
         ScreenToClient( hwndList, ((LPPOINT)&rcCol2Wnd)+1 );

         /* Draw the filename, if we have a filename.
          * Hey! This reminds me of an old joke. GB Shaw was once sent
          * a letter from a person who apologised for being unable to
          * make the premier of his newest play, but offered "to attend
          * the second night, if there is one". Unfazed, Shaw sent by
          * return two tickets for the second night. "One for yourself
          * and another for a friend, if you have one".
          */
         if( *lpText != FLTYP_COMMENT )
            p = fli.szFileName;
         else
            p = fli.szDescription;
         hOldFont = SelectFont( lpDrawItem->hDC, hSys8Font );
         DrawText( lpDrawItem->hDC, p, -1, &rc, DT_NOPREFIX|DT_SINGLELINE|DT_VCENTER|DT_CALCRECT );
         rc.right += 2;
         rc.left += 2;
         DrawText( lpDrawItem->hDC, p, -1, &rc, DT_NOPREFIX|DT_SINGLELINE|DT_VCENTER );
         rc.left -= 2;
         if( lpDrawItem->itemState & ODS_FOCUS )
            DrawFocusRect( lpDrawItem->hDC, &rc );
         SelectFont( lpDrawItem->hDC, hOldFont );
         }

      /* Draw the size and/or description fields. Skip if entire
       * line is a comment.
       */
      if( lpDrawItem->itemAction == ODA_DRAWENTIRE && *lpText != FLTYP_COMMENT )
         {
         hOldFont = SelectFont( lpDrawItem->hDC, hHelv8Font );
         SetTextColor( lpDrawItem->hDC, GetSysColor( COLOR_WINDOWTEXT ) );
         SetBkColor( lpDrawItem->hDC, GetSysColor( COLOR_WINDOW ) );

         /* Draw the file size column
          */
         rc.left = rcCol1Wnd.left;
         rc.right = rcCol2Wnd.left;
         DrawText( lpDrawItem->hDC, fli.szSize, -1, &rc, DT_NOPREFIX|DT_SINGLELINE|DT_VCENTER );

         /* Draw the description column
          */
         rc.left = rcCol2Wnd.left;
         rc.right = lpDrawItem->rcItem.right;
         DrawText( lpDrawItem->hDC, fli.szDescription, -1, &rc, DT_NOPREFIX|DT_SINGLELINE|DT_VCENTER );        
         SelectFont( lpDrawItem->hDC, hOldFont );
         }
      DeleteBrush( hbr );
      }
}

/* This function parses a file list line into its filename, size
 * and description components.
 */
void FASTCALL ParseFileListItem( LPSTR lpText, FILELISTITEM * pfli )
{
   register int c;
   BYTE bType;

   /* Clear the fields.
    */
   pfli->szFileName[ 0 ] = '\0';
   pfli->szSize[ 0 ] = '\0';
   pfli->szDescription[ 0 ] = '\0';

   /* The first byte of the line is the type of line.
    */
   switch( bType = *lpText++ )
      {
      case FLTYP_FILENAME:
         while( *lpText == ' ' || *lpText == '\t' )
            ++lpText;
         for( c = 0; *lpText && *lpText != ' ' && *lpText != '\t'; )
            {
            if( c < 16 )
               pfli->szFileName[ c++ ] = *lpText;
            ++lpText;
            }
         pfli->szFileName[ c ] = '\0';
   
         /* Skip spaces between filename and size
          */
         while( *lpText == ' ' || *lpText == '\t' )
            ++lpText;
   
         /* Copy the size field into szSize.
          */
         for( c = 0; *lpText && *lpText != ' ' && *lpText != '\t'; )
            {
            if( c < 10 )
               pfli->szSize[ c++ ] = *lpText;
            ++lpText;
            }
         pfli->szSize[ c ] = '\0';

      case FLTYP_DESCRIPTION:
         /* Skip spaces between size and description
          */
         while( *lpText == ' ' || *lpText == '\t' )
            ++lpText;

      case FLTYP_COMMENT:
         /* Copy the description into szDescription
          */
         for( c = 0; *lpText; )
            {
            if( c < 100 )
               pfli->szDescription[ c++ ] = *lpText;
            ++lpText;
            }
         pfli->szDescription[ c ] = '\0';
         break;
      }
}

/* This is the Filelist out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_Filelist( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   FILELISTOBJECT FAR * lpflo;

   lpflo = (FILELISTOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_EXEC:
/*       if (fUseWebServices){ // !!SM!! 2.56.2053
            char lPath[255];
            wsprintf(lPath, "%s\\%s", (LPSTR)pszAmeolDir, (LPSTR)"cix.dat" ); // VistaAlert
            return (Am2GetFileList(szCIXNickname, szCIXPassword, DRF(lpflo->recConfName ), DRF(lpflo->recTopicName ), (char*)&lPath));
         }
         else*/
            return( POF_HELDBYSCRIPT );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR240), DRF(lpflo->recConfName), DRF(lpflo->recTopicName) );
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( 0 );

      case OBEVENT_NEW:
         _fmemset( lpflo, 0, sizeof( FILELISTOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         FILELISTOBJECT flo;
      
         INITIALISE_PTR(lpflo);
         Amob_LoadDataObject( fh, &flo, sizeof( FILELISTOBJECT ) );
         if( fNewMemory( &lpflo, sizeof( FILELISTOBJECT ) ) )
            {
            flo.recConfName.hpStr = NULL;
            flo.recTopicName.hpStr = NULL;
            *lpflo = flo;
            }
         return( (LRESULT)lpflo );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpflo->recConfName );
         Amob_SaveRefObject( &lpflo->recTopicName );
         return( Amob_SaveDataObject( fh, lpflo, sizeof( FILELISTOBJECT ) ) );

      case OBEVENT_COPY: {
         FILELISTOBJECT FAR * lpcoNew;
      
         INITIALISE_PTR( lpcoNew );
         lpflo = (FILELISTOBJECT FAR *)dwData;
         if( fNewMemory( &lpcoNew, sizeof( FILELISTOBJECT ) ) )
            {
            INITIALISE_PTR( lpcoNew->recConfName.hpStr );
            INITIALISE_PTR( lpcoNew->recTopicName.hpStr );
            Amob_CopyRefObject( &lpcoNew->recConfName, &lpflo->recConfName );
            Amob_CopyRefObject( &lpcoNew->recTopicName, &lpflo->recTopicName );
            }
         return( (LRESULT)lpcoNew );
         }

      case OBEVENT_FIND: {
         FILELISTOBJECT FAR * lpflo1;
         FILELISTOBJECT FAR * lpflo2;

         lpflo1 = (FILELISTOBJECT FAR *)dwData;
         lpflo2 = (FILELISTOBJECT FAR *)lpBuf;
         if( strcmp( DRF(lpflo1->recConfName), DRF(lpflo2->recConfName) ) != 0 )
            return( FALSE );
         return( strcmp( DRF(lpflo1->recTopicName), DRF(lpflo2->recTopicName) ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpflo );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpflo->recConfName );
         Amob_FreeRefObject( &lpflo->recTopicName );
         return( TRUE );
      }
   return( 0L );
}

/* This is the File directory out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_Fdir( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   FDIROBJECT FAR * lpfdr;

   lpfdr = (FDIROBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_EXEC:
/*       if (fUseWebServices){ // !!SM!! 2.56.2053
            char lPath[255];
            wsprintf(lPath, "%s\\%s", (LPSTR)pszAmeolDir, (LPSTR)"cix.dat" ); // VistaAlert
            return (Am2GetFDir(szCIXNickname, szCIXPassword, DRF(lpfdr->recConfName ), DRF(lpfdr->recTopicName ), (char *)&lPath));
         }
         else*/
            return( POF_HELDBYSCRIPT );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR675), DRF(lpfdr->recConfName), DRF(lpfdr->recTopicName) );
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( 0 );

      case OBEVENT_NEW:
         _fmemset( lpfdr, 0, sizeof( FDIROBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         FDIROBJECT fdr;
      
         INITIALISE_PTR(lpfdr);
         Amob_LoadDataObject( fh, &fdr, sizeof( FDIROBJECT ) );
         if( fNewMemory( &lpfdr, sizeof( FDIROBJECT ) ) )
            {
            fdr.recConfName.hpStr = NULL;
            fdr.recTopicName.hpStr = NULL;
            *lpfdr = fdr;
            }
         return( (LRESULT)lpfdr );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpfdr->recConfName );
         Amob_SaveRefObject( &lpfdr->recTopicName );
         return( Amob_SaveDataObject( fh, lpfdr, sizeof( FDIROBJECT ) ) );

      case OBEVENT_COPY: {
         FDIROBJECT FAR * lpcoNew;
      
         INITIALISE_PTR( lpcoNew );
         lpfdr = (FDIROBJECT FAR *)dwData;
         if( fNewMemory( &lpcoNew, sizeof( FDIROBJECT ) ) )
            {
            INITIALISE_PTR( lpcoNew->recConfName.hpStr );
            INITIALISE_PTR( lpcoNew->recTopicName.hpStr );
            Amob_CopyRefObject( &lpcoNew->recConfName, &lpfdr->recConfName );
            Amob_CopyRefObject( &lpcoNew->recTopicName, &lpfdr->recTopicName );
            }
         return( (LRESULT)lpcoNew );
         }

      case OBEVENT_FIND: {
         FDIROBJECT FAR * lpfdr1;
         FDIROBJECT FAR * lpfdr2;

         lpfdr1 = (FDIROBJECT FAR *)dwData;
         lpfdr2 = (FDIROBJECT FAR *)lpBuf;
         if( strcmp( DRF(lpfdr1->recConfName), DRF(lpfdr2->recConfName) ) != 0 )
            return( FALSE );
         return( strcmp( DRF(lpfdr1->recTopicName), DRF(lpfdr2->recTopicName) ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpfdr );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpfdr->recConfName );
         Amob_FreeRefObject( &lpfdr->recTopicName );
         return( TRUE );
      }
   return( 0L );
}

/* This function handles the File List find
 * command.
 */
void FASTCALL CmdFileListFind( HWND hwnd, PTL ptl )
{
   Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_FILELISTFIND), FileListFindDlg, (LPARAM)ptl );
}

/* This function handles the File List Find dialog box.
 */
BOOL EXPORT CALLBACK FileListFindDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = FileListFindDlg_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the Admin
 *  dialog.
 */
LRESULT FASTCALL FileListFindDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, FileListFindDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, FileListFindDlg_OnCommand );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, FolderCombo_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_DRAWITEM, FolderCombo_OnDrawItem );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsFILELISTFIND );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL FileListFindDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   register int c;
   HWND hwndEdit;
   HWND hwndList;
   int index;

   /* Fill the list of folders.
    */
   FillListWithTopics( hwnd, IDD_FOLDER, FTYPE_CIX|FTYPE_TOPICS );

   /* Fill the list of previous search strings.
    */
   LoadFileFindStrings();
   VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_EDIT ) );
   ComboBox_LimitText( hwndEdit, sizeof(szFindStr)-1 );
   for( c = 0; c < cFindStr; ++c )
      ComboBox_InsertString( hwndEdit, -1, pszFindStr[ c ] );

   /* Select the current folder.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_FOLDER ) );
   index = 0;
   if( 0 != lParam )
      {
      if( CB_ERR != ( index = RealComboBox_FindItemData( hwndList, -1, lParam ) ) )
         ComboBox_SetCurSel( hwndList, index );
      }
   ComboBox_SetCurSel( hwndList, index );

   /* Disable Reset List option if window not open.
    */
   EnableControl( hwnd, IDD_RESETLIST, NULL != hwndFlistSearch );

   /* Set the original options.
    */
   CheckDlgButton( hwnd, IDD_CASE, fCase );
   CheckDlgButton( hwnd, IDD_WORDMATCH, fWordMatch );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL FileListFindDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDOK: {
         LPVOID pFolder;
         HWND hwndGauge;
         HWND hwndList;
         HWND hwndEdit;
         register int c;
         int cFound;
         int index;

         /* Get the selected search folder.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_FOLDER ) );
         index = ComboBox_GetCurSel( hwndList );
         ASSERT( CB_ERR != index );
         pFolder = (LPVOID)ComboBox_GetItemData( hwndList, index );
         ASSERT( NULL != pFolder );

         /* Get the options.
          */
         fCase = IsDlgButtonChecked( hwnd, IDD_CASE );
         fWordMatch = IsDlgButtonChecked( hwnd, IDD_WORDMATCH );

         /* If reset list option checked and window open,
          * empty the window.
          */
         if( IsDlgButtonChecked( hwnd, IDD_RESETLIST ) && NULL != hwndFlistSearch )
            {
            HWND hwndList;

            VERIFY( hwndList = GetDlgItem( hwndFlistSearch, IDD_LIST ) );
            ListBox_ResetContent( hwndList );
            }

         /* Get the search string.
          */
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_EDIT ) );
         ComboBox_GetText( hwndEdit, szFindStr, sizeof(szFindStr) );

         /* Strip out duplicates.
          */
         for( c = 0; c < cFindStr; ++c )
            if( strcmp( szFindStr, pszFindStr[ c ] ) == 0 )
               {
               free( pszFindStr[ c ] );
               for( ; c < cFindStr - 1; ++c )
                  pszFindStr[ c ] = pszFindStr[ c + 1 ];
               --cFindStr;
               pszFindStr[ c ] = NULL;
               break;
               }

         /* Add it to the list.
          */
         if( cFindStr++ == MAX_FIND_STRINGS )
            {
            --cFindStr;
            free( pszFindStr[ cFindStr - 1 ] );
            }
         for( c = cFindStr - 1; c > 0; --c )
            pszFindStr[ c ] = pszFindStr[ c - 1 ];
         pszFindStr[ 0 ] = _strdup( szFindStr );

         /* Disable controls while searching.
          */
         EnableControl( hwnd, IDOK, FALSE );
         EnableControl( hwnd, IDD_PAD1, FALSE );
         EnableControl( hwnd, IDD_PAD2, FALSE );
         EnableControl( hwnd, IDD_PAD3, FALSE );
         EnableControl( hwnd, IDD_EDIT, FALSE );
         EnableControl( hwnd, IDD_FOLDER, FALSE );
         EnableControl( hwnd, IDD_CASE, FALSE );
         EnableControl( hwnd, IDD_WORDMATCH, FALSE );
         EnableControl( hwnd, IDD_RESETLIST, FALSE );
         EnableControl( hwnd, IDD_HELP, FALSE );

         /* Rename Close to Stop.
          */
         SetWindowText( GetDlgItem( hwnd, IDCANCEL ), "&Stop" );

         /* Initialise gauge.
          */
         VERIFY( hwndGauge = GetDlgItem( hwnd, IDD_GAUGE ) );
         SendMessage( hwndGauge, PBM_SETSTEP, 1, 0L );
         SendMessage( hwndGauge, PBM_SETPOS, 0, 0L );
         SendMessage( hwndGauge, PBM_SETRANGE, 0, MAKELPARAM( 0, CountTopics( pFolder ) ) );

         /* Call the appropriate search routine depending
          * on what folder was selected.
          */
         fScanning = TRUE;
         fMainAbort = FALSE;
         if( Amdb_IsCategoryPtr( (PCAT)pFolder ) )
            cFound = FileListFindCategory( hwnd, (PCAT)pFolder );
         else if( Amdb_IsFolderPtr( (PCL)pFolder ) )
            cFound = FileListFindFolder( hwnd, (PCL)pFolder );
         else
            cFound = FileListFindTopic( hwnd, (PTL)pFolder );
         fScanning = FALSE;
         fMainAbort = FALSE;
         if( 0 == cFound )
            {
            /* Bewail and bemoan.
             */
            fMessageBox( hwnd, 0, GS(IDS_STR345), MB_OK|MB_ICONINFORMATION );

            /* Reset status gauge.
             */
            SendMessage( hwndGauge, PBM_SETPOS, 0, 0L );

            /* Rename Stop to Close
             */
            SetWindowText( GetDlgItem( hwnd, IDCANCEL ), "&Close" );

            /* Enable controls while searching.
             */
            EnableControl( hwnd, IDOK, TRUE );
            EnableControl( hwnd, IDD_PAD1, TRUE );
            EnableControl( hwnd, IDD_PAD2, TRUE );
            EnableControl( hwnd, IDD_PAD3, TRUE );
            EnableControl( hwnd, IDD_EDIT, TRUE );
            EnableControl( hwnd, IDD_FOLDER, TRUE );
            EnableControl( hwnd, IDD_CASE, TRUE );
            EnableControl( hwnd, IDD_WORDMATCH, TRUE );
            EnableControl( hwnd, IDD_RESETLIST, NULL != hwndFlistSearch );
            EnableControl( hwnd, IDD_HELP, TRUE );
            break;
            }
         }

      case IDCANCEL: {
         register int c;
         register int n;

         /* If we're scanning, just set a flag.
          */
         if( fScanning )
            {
            fMainAbort = TRUE;
            break;
            }

         /* Save the list of find strings.
          */
         Amuser_WritePPString( szFileFind, NULL, NULL );
         for( n = 0, c = 1; c <= MAX_FIND_STRINGS; ++n, ++c )
            {
            char sz[ 10 ];

            wsprintf( sz, "FileFind%u", c );
            Amuser_WritePPString( szFileFind, sz, pszFindStr[ n ] );
            }

         /* Delete the list of previous strings.
          * Bug fix: Now checks that c < MAX_FIND_STRINGS
          * YH 02/05/96
          */
         for( c = 0; pszFindStr[ c ] != NULL && c < MAX_FIND_STRINGS; ++c)
            free( pszFindStr[ c ] );
         EndDialog( hwnd, id == IDOK );
         break;
         }
      }
}

/* This function loads the file find search string history
 * list from the configuration file.
 */
void FASTCALL LoadFileFindStrings( void )
{
   register int c;
   char sz[ 16 ];

   /* Load the strings from the [Find] section.
    */
   cFindStr = 0;
   for( c = 0; c < MAX_FIND_STRINGS; ++c )
      {
      wsprintf( sz, "FileFind%u", c + 1 );
      if( !Amuser_GetPPString( szFileFind, sz, "", szFindStr, sizeof( szFindStr ) ) )
         break;
      pszFindStr[ c ] = _strdup( szFindStr );
      ++cFindStr;
      }

   /* Pad out the unused history items to NULL.
    */
   while( c < MAX_FIND_STRINGS )
      pszFindStr[ c++ ] = NULL;

   /* Make the first string the active string.
    */
   if( cFindStr )
      strcpy( szFindStr, pszFindStr[ 0 ] );
}

/* This function searches a database for a matching file or
 * description.
 */
int FASTCALL FileListFindCategory( HWND hwnd, PCAT pcat )
{
   int cFound;
   PCL pcl;

   cFound = 0;
   for( pcl = Amdb_GetFirstFolder( pcat ); !fMainAbort && pcl; pcl = Amdb_GetNextFolder( pcl ) )
      cFound += FileListFindFolder( hwnd, pcl );
   return( cFound );
}

/* This function searches a folder for a matching file or
 * description.
 */
int FASTCALL FileListFindFolder( HWND hwnd, PCL pcl )
{
   int cFound;
   PTL ptl;

   cFound = 0;
   for( ptl = Amdb_GetFirstTopic( pcl ); !fMainAbort && ptl; ptl = Amdb_GetNextTopic( ptl ) )
      cFound += FileListFindTopic( hwnd, ptl );
   return( cFound );
}

/* This function searches a topic for a matching file or
 * description.
 */
int FASTCALL FileListFindTopic( HWND hwnd, PTL ptl )
{
   HWND hwndGauge;
   TOPICINFO ti;
   int cFound;

   /* One step for each topic.
    */
   VERIFY( hwndGauge = GetDlgItem( hwnd, IDD_GAUGE ) );
   SendMessage( hwndGauge, PBM_STEPIT, 0, 0L );

   /* Initialise count of found items.
    */
   cFound = 0;

   /* Skip topic if it doesn't have a file list.
    */
   Amdb_GetTopicInfo( ptl, &ti );
   if( ti.dwFlags & TF_HASFILES )
      {
      char szFileName[ _MAX_FNAME ];
      HNDFILE fh;

      /* Get the filelist topic name and open the 
       * file if we can.
       */
      Amdb_GetInternalTopicName( ptl, szFileName );
      strcat( szFileName, ".FLS" );
      if( ( fh = Ameol2_OpenFile( szFileName, DSD_FLIST, AOF_READ ) ) != HNDFILE_ERROR )
         {
         LPLBF hBuffer;

         /* Set the file for line buffered reads.
          */
         hBuffer = Amlbf_Open( fh, AOF_READ );

         /* Read and discard first line.
          */
         Amlbf_Read( hBuffer, lpTmpBuf, LEN_TEMPBUF-1, NULL, NULL, &fIsAmeol2Scratchpad );

         /* Now loop for each line and locate szFindStr on that line. If
          * found, add the information to the listbox.
          */
         while( !fMainAbort && Amlbf_Read( hBuffer, lpTmpBuf + 1, LEN_TEMPBUF-2, NULL, NULL, &fIsAmeol2Scratchpad ) )
            {
            TaskYield();
            lpTmpBuf[ 0 ] = ParseFlistLine( lpTmpBuf + 1 );
            if( FLTYP_FILENAME == lpTmpBuf[ 0 ] )
               if( -1 != FStrMatch( lpTmpBuf + 1, szFindStr, fCase, fWordMatch ) )
                  {
                  /* Got a match, so add this entry to the listbox.
                   */
                  if( CreateFlistSearchWindow( hwndFrame, ptl ) )
                     {
                     HWND hwndList;
                     int index;

                     VERIFY( hwndList = GetDlgItem( hwndFlistSearch, IDD_LIST ) );
                     index = ListBox_InsertString( hwndList, -1, lpTmpBuf );
                     if( LB_ERR == index )
                        break;
                     ListBox_SetItemData( hwndList, index, (LPARAM)ptl );
                     ++cFound;
                     }
                  }
            }
         Amlbf_Close( hBuffer );
         }
      }
   return( cFound );
}

/* This command handles the Print command in the file list window.
 */
void FASTCALL CmdFilelistPrint( HWND hwnd )
{
   LPFILELISTWNDINFO lpfli;
   HPRINT hPr;
   BOOL fOk;

   /* Get file list window handle.
    */
   lpfli = GetFilelistWndInfo( hwnd );

   /* Show and get the print terminal dialog box.
    */
   if( !Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_PRINTFILELIST), PrintFilelistDlg, (LPARAM)lpfli ) )
      return;

   /* Start printing.
    */
   GetWindowText( hwnd, lpTmpBuf, LEN_TEMPBUF );
   fOk = TRUE;
   if( NULL != ( hPr = BeginPrint( hwnd, lpTmpBuf, &lfFixedFont ) ) )
      {
      register int c;
      register int d;

      for( c = 0; fOk && c < cCopies; ++c )
         {
         HWND hwndList;
         int count;

         /* Print the header.
          */
         ResetPageNumber( hPr );
         fOk = BeginPrintPage( hPr );
         wsprintf( lpTmpBuf, "%-14s   %10s    %s", (LPSTR)"Filename", (LPSTR)"Size", (LPSTR)"Description" );
         if( !PrintLine( hPr, lpTmpBuf ) )
            break;
         wsprintf( lpTmpBuf, "%-14s   %10s    %s", (LPSTR)"--------", (LPSTR)"----", (LPSTR)"-----------" );
         if( !PrintLine( hPr, lpTmpBuf ) )
            break;

         /* Now print each file description under the
          * header.
          */
         fOk = TRUE;
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         count = ListBox_GetCount( hwndList );
         for( d = 0; fOk && d < count; ++d )
            {
            FILELISTITEM fli;
            LPSTR lpText;

            lpText = (LPSTR)ListBox_GetItemData( hwndList, d );
            ParseFileListItem( lpText, &fli );
            if( *lpText == FLTYP_COMMENT )
               wsprintf( lpTmpBuf, "%s", (LPSTR)fli.szDescription );
            else
               wsprintf( lpTmpBuf, "%-14s   %10s    %s", (LPSTR)fli.szFileName, (LPSTR)fli.szSize, (LPSTR)fli.szDescription );
            fOk = PrintLine( hPr, lpTmpBuf );
            }
         if( fOk )
            EndPrintPage( hPr );
         }
      EndPrint( hPr );
      }
}

/* This function handles the Print dialog.
 */
BOOL EXPORT CALLBACK PrintFilelistDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = PrintFilelistDlg_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the Print Message
 * dialog.
 */
LRESULT FASTCALL PrintFilelistDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, PrintFilelistDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, PrintFilelistDlg_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPRINTFILELIST );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL PrintFilelistDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPFILELISTWNDINFO lpfli;
   HWND hwndSpin;

   /* Get file list window handle.
    */
   lpfli = (LPFILELISTWNDINFO)lParam;

   /* Set the input field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_COPIES ), 2 );

   /* Set number of copies.
    */
   SetDlgItemInt( hwnd, IDD_COPIES, 1, FALSE );
   hwndSpin = GetDlgItem( hwnd, IDD_SPIN );
   SendMessage( hwndSpin, UDM_SETBUDDY, (WPARAM)GetDlgItem( hwnd, IDD_COPIES ), 0L );
   SendMessage( hwndSpin, UDM_SETRANGE, 0, MAKELPARAM( 99, 1 ) );
   SendMessage( hwndSpin, UDM_SETPOS, 0, MAKELPARAM( 1, 0 ) );

   /* Disable moderator option if user is not a moderator
    * of the conference.
    */
   if( !Amdb_IsModerator( Amdb_FolderFromTopic( lpfli->ptl ), szCIXNickname ) )
      EnableControl( hwnd, IDD_FDIR, FALSE );
   CheckDlgButton( hwnd, IDD_FLIST, TRUE );

   /* Display current printer name.
    */
   UpdatePrinterName( hwnd );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL PrintFilelistDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_SETUP:
         PrintSetup( hwnd );
         UpdatePrinterName( hwnd );
         break;

      case IDOK:
         /* Get number of copies.
          */
         if( !GetDlgInt( hwnd, IDD_COPIES, &cCopies, 1, 99 ) )
            break;
         fPrintFlist = IsDlgButtonChecked( hwnd, IDD_FLIST );
         fPrintFdir = IsDlgButtonChecked( hwnd, IDD_FDIR );
         EndDialog( hwnd, TRUE );
         break;

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}

/* This function creates the window which will be filled with the
 * results of a search in the file lists.
 */
BOOL FASTCALL CreateFlistSearchWindow( HWND hwnd, PTL ptl )
{
   /* If window already visible, bring it to the front.
    */
   if( NULL == hwndFlistSearch )
      {
      HWND hwndFocus;
      DWORD dwState;
      int cxBorder;
      int cyBorder;
      RECT rc;

      /* Register the group list window class if we have
       * not already done so.
       */
      if( !fRegisteredFlist )
         {
         WNDCLASS wc;

         wc.style          = CS_HREDRAW | CS_VREDRAW;
         wc.lpfnWndProc    = FlistSearchWndProc;
         wc.hIcon          = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_FILELISTFIND) );
         wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
         wc.lpszMenuName   = NULL;
         wc.cbWndExtra     = DLGWINDOWEXTRA;
         wc.cbClsExtra     = 0;
         wc.hbrBackground  = (HBRUSH)( COLOR_APPWORKSPACE + 1 );
         wc.lpszClassName  = szFlistSearchWndClass;
         wc.hInstance      = hInst;
         if( !RegisterClass( &wc ) )
            return( FALSE );
         fRegisteredFlist = TRUE;
         }

      /* The window will appear in the left half of the
       * client area.
       */
      GetClientRect( hwndMDIClient, &rc );
      rc.bottom -= GetSystemMetrics( SM_CYICON );
      cxBorder = ( rc.right - rc.left ) / 6;
      cyBorder = ( rc.bottom - rc.top ) / 6;
      InflateRect( &rc, -cxBorder, -cyBorder );
      dwState = 0;

      /* Get the actual window size, position and state.
       */
      ReadProperWindowState( szFlistSearchWndName, &rc, &dwState );

      /* Create the window.
       */
      hwndFlistSearch = Adm_CreateMDIWindow( szFlistSearchWndName, szFlistSearchWndClass, hInst, &rc, dwState, 0L );
      if( NULL == hwndFlistSearch )
         return( FALSE );

      /* Show the window.
       */
      SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndFlistSearch, 0L );
      UpdateWindow( hwndFlistSearch );

      /* Determine where we put the focus.
       */
      hwndFocus = GetDlgItem( hwndFlistSearch, IDD_LIST );
      SetFocus( hwndFocus );
      }
   ptlFlistWnd = ptl;
   return( TRUE );
}

/* This is the File List find window procedure.
 */
LRESULT EXPORT CALLBACK FlistSearchWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, FlistSearchWnd_OnCreate );
      HANDLE_MSG( hwnd, WM_CLOSE, FlistSearchWnd_OnClose );
      HANDLE_MSG( hwnd, WM_SIZE, FlistSearchWnd_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, FlistSearchWnd_OnMove );
      HANDLE_MSG( hwnd, WM_NOTIFY, FlistSearchWnd_OnNotify );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, FlistSearchWnd_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, FilelistWnd_MDIActivate );
      HANDLE_MSG( hwnd, WM_COMMAND, FlistSearchWnd_OnCommand );
      HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, FlistSearchWnd_OnAdjustWindows );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, FlistSearchWnd_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_DRAWITEM, FlistSearchWnd_OnDrawItem );
      HANDLE_MSG( hwnd, WM_SETFOCUS, FlistSearchWnd_OnSetFocus );

      case WM_GETMINMAXINFO: {
         MINMAXINFO FAR * lpMinMaxInfo;
         LRESULT lResult;

         /* Set the minimum tracking size so the client window can never
          * be resized below 24 lines.
          */
         lResult = Adm_DefMDIDlgProc( hwnd, message, wParam, lParam );
         lpMinMaxInfo = (MINMAXINFO FAR *)lParam;
         lpMinMaxInfo->ptMinTrackSize.x = 220;
         lpMinMaxInfo->ptMinTrackSize.y = 150;
         return( lResult );
         }

      default:
         break;
      }
   return( Adm_DefMDIDlgProc( hwnd, message, wParam, lParam ) );

}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL FlistSearchWnd_OnCreate( HWND hwnd, LPCREATESTRUCT lpCreateStruct )
{
   static int aArray[ 4 ] = { 120, 160, 80, 200 };
   LPMDICREATESTRUCT lpMDICreateStruct;
   HWND hwndTopLst;
   HWND hwndList;
   HD_ITEM hdi;

   /* Create the dialog window.
    */
   lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
   Adm_MDIDialog( hwnd, MAKEINTRESOURCE(IDDLG_FLISTFIND), lpMDICreateStruct );

   /* Read the current header settings.
    */
   Amuser_GetPPArray( szFileFind, "Columns", aArray, 4 );

   /* Subclass the listbox so we can handle the right mouse
    * button. NOTE: Must subclass the listbox within the
    * topic list window.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   fQuickUpdate = FALSE;
   lpProcFListCtl = SubclassWindow( GetDlgItem( hwndList, 0x3000 ), FlistListProc );

   /* Initialise the header control.
    */
   VERIFY( hwndTopLst = GetDlgItem( hwnd, IDD_LIST ) );
   hdi.mask = HDI_TEXT|HDI_WIDTH|HDI_FORMAT;
   hdi.cxy = hdrColumns[ 0 ] = aArray[ 0 ];
   hdi.pszText = "Filename";
   hdi.fmt = HDF_STRING|HDF_LEFT;
   Header_InsertItem( hwndTopLst, 0, &hdi );
   
   hdi.cxy = hdrColumns[ 1 ] = aArray[ 1 ];
   hdi.pszText = "Forum/Topic";
   hdi.fmt = HDF_STRING|HDF_LEFT;
   Header_InsertItem( hwndTopLst, 1, &hdi );

   hdi.cxy = hdrColumns[ 2 ] = aArray[ 2 ];
   hdi.pszText = "Size";
   hdi.fmt = HDF_STRING|HDF_LEFT;
   Header_InsertItem( hwndTopLst, 2, &hdi );

   hdi.cxy = hdrColumns[ 3 ] = aArray[ 3 ];
   hdi.pszText = "Description";
   hdi.fmt = HDF_STRING|HDF_LEFT;
   Header_InsertItem( hwndTopLst, 3, &hdi );
   SendMessage( hwndTopLst, WM_SETTEXT, 3 | HBT_SPRING, 0L );
   SetWindowFont( hwndTopLst, hHelv8Font, FALSE );

   return( TRUE );
}

/* This function intercepts messages for the file find list box window
 * so that we can handle the right mouse button.
 */
LRESULT EXPORT FAR PASCAL FlistListProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   switch( msg )
      {
      case WM_CONTEXTMENU: /*!!SM!!*/
      case WM_RBUTTONDOWN: {
      HMENU hPopupMenu;
      int index;
      POINT pt;
      HWND hwndList;

      hwndList = GetDlgItem( hwnd, IDD_LIST );

      /* Select the item under the cursor only if nothing is
       * currently selected.
       */
      index = ItemFromPoint( hwnd, (short)LOWORD(lParam), (short)HIWORD(lParam) );
      if( ListBox_GetSel( hwnd, index ) == 0 )
         {
         CallWindowProc( lpProcFListCtl, hwnd, WM_LBUTTONDOWN, wParam, lParam );
         CallWindowProc( lpProcFListCtl, hwnd, WM_LBUTTONUP, wParam, lParam );
         }

      /* Display the popup menu.
       */
      hPopupMenu = GetPopupMenu( IPOP_FILELIST );
      GetCursorPos( &pt );
      TrackPopupMenu( hPopupMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndFlistSearch, NULL );
      break;
      }
      
      case WM_ERASEBKGND:
         if( fQuickUpdate )
            return( TRUE );
         break;

   }
   return( CallWindowProc( lpProcFListCtl, hwnd, msg, wParam, lParam ) );
}

/* This function processes the WM_COMMAND message.
 */
void FASTCALL FlistSearchWnd_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsFLISTFIND );
         break;

      case IDD_FIND:
         CmdFileListFind( hwnd, ptlFlistWnd );
         break;

      case IDD_LIST:
         if( codeNotify != LBN_DBLCLK )
            break;

      case IDM_DOWNLOAD:
      case IDD_DOWNLOAD: 
         {
         HWND hwndList;
         int nSel;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         hwndList = GetDlgItem( hwndList, 0x3000 );
         if( ( nSel = ListBox_GetCurSel( hwndList ) ) != LB_ERR )
            {
            char sz[ 120 ];

            ListBox_GetText( hwndList, nSel, sz );
            if( sz[ 0 ] == FLTYP_FILENAME )
               {
               char szCIXFileName[ LEN_CIXFILENAME+1 ];
               char szDirectory[ _MAX_PATH ];
               register int c;
               FDLOBJECT fd;
               LPSTR lpText;
               PCL pcl;
               PTL ptl;

               /* Extract the filename.
                */
               lpText = &sz[ 1 ];
               while( *lpText == ' ' || *lpText == '\t' )
                  ++lpText;
               for( c = 0; c <= LEN_CIXFILENAME && lpText[ c ] != ' '; ++c )
                  szCIXFileName[ c ] = lpText[ c ];
               szCIXFileName[ c ] = '\0';

               /* Get this folder and topic.
                */
               ptl = (PTL)ListBox_GetItemData( hwndList, nSel );
               pcl = Amdb_FolderFromTopic( ptl );

               /* Get the directory associated with this
                * filename.
                */
               GetAssociatedDir( ptl, szDirectory, szCIXFileName );

               /* Create a new FDLOBJECT type object.
                */
               Amob_New( OBTYPE_DOWNLOAD, &fd );
               Amob_CreateRefString( &fd.recCIXFileName, szCIXFileName );
               Amob_CreateRefString( &fd.recDirectory, szDirectory );
               Amob_CreateRefString( &fd.recConfName, (LPSTR)Amdb_GetFolderName( pcl ) );
               Amob_CreateRefString( &fd.recTopicName, (LPSTR)Amdb_GetTopicName( ptl ) );
               if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_FDL), Download, (LPARAM)(LPSTR)&fd ) )
                  {
                  if( !Amob_Find( OBTYPE_DOWNLOAD, &fd ) )
                     Amob_Commit( NULL, OBTYPE_DOWNLOAD, &fd );
                  }
               Amob_Delete( OBTYPE_DOWNLOAD, &fd );
               SetFocus( hwndList );
                  }
               }
            }
         break;

         case IDM_CIXFILELINK: {
         HWND hwndList;
         int nSel;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         hwndList = GetDlgItem( hwndList, 0x3000 );
         if( ( nSel = ListBox_GetCurSel( hwndList ) ) != LB_ERR )
            {
            char sz[ 120 ];

            ListBox_GetText( hwndList, nSel, sz );

            if( sz[ 0 ]== FLTYP_FILENAME )
               {
               char szCIXFileName[ LEN_CIXFILENAME+1 ];
               register int c;
               PCL pcl;
               PTL ptl;
               HGLOBAL gHnd;
               LPSTR lpText;


               /* Extract the filename.
                */
               lpText = &sz[ 1 ];
               while( *lpText == ' ' || *lpText == '\t' )
                  ++lpText;
               for( c = 0; c <= LEN_CIXFILENAME && lpText[ c ] != ' '; ++c )
                  szCIXFileName[ c ] = lpText[ c ];
               szCIXFileName[ c ] = '\0';

               /* Get this folder and topic.
                */
               ptl = (PTL)ListBox_GetItemData( hwndList, nSel );
               pcl = Amdb_FolderFromTopic( ptl );

               wsprintf( sz, "cixfile:%s/%s:%s", Amdb_GetFolderName( pcl ), Amdb_GetTopicName( ptl ), szCIXFileName );

               if( NULL != ( gHnd = GlobalAlloc( GHND, strlen( sz ) + 1 ) ) )
               {
               char * gPtr;


               /* Copy it to the clipboard.
                */
               if( NULL != ( gPtr = GlobalLock( gHnd ) ) )
                  {
                  strcpy( gPtr, sz );
                  GlobalUnlock( gHnd );
                  if( OpenClipboard( hwndFrame ) )
                     {
                     EmptyClipboard();
                     SetClipboardData( CF_TEXT, gHnd );
                     CloseClipboard();
                     break;
                     }
                  }
               GlobalFree( gHnd );
               }

               SetFocus( hwndList );
               }
            }
         break;
         }

      case IDOK:
      case IDCANCEL:
         /* Close the file list window.
          */
         FlistSearchWnd_OnClose( hwnd );
         break;
      }
}

/* This function handles the WM_ERASEBKGND message.
 */
BOOL FASTCALL FlistSearchWnd_OnEraseBkgnd( HWND hwnd, HDC hdc )
{
   if( fQuickUpdate )
      return( TRUE );
   return( Common_OnEraseBkgnd( hwnd, hdc ) );
}

/* This function handles notification messages from the
 * treeview control.
 */
LRESULT FASTCALL FlistSearchWnd_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
{
   switch( lpNmHdr->code )
      {
      case HDN_ITEMCLICK:
         return( TRUE );

      case HDN_BEGINTRACK:
         return( TRUE );

      case HDN_TRACK:
      case HDN_ENDTRACK: {
         register int c;
         HWND hwndList;

         /* Read the new column settings into the
          * global hdrColumns array.
          */
         for( c = 0; c < 3; ++c )
            {
            HD_ITEM hdi;

            SendMessage( lpNmHdr->hwndFrom, HDM_GETITEM, c, (LPARAM)(LPSTR)&hdi );
            hdrColumns[ c ] = hdi.cxy;
            }
         if( HDN_ENDTRACK == lpNmHdr->code )
            Amuser_WritePPArray( szFileFind, "Columns", hdrColumns, 4 );

         /* Update the listbox
          */
         fQuickUpdate = TRUE;
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         InvalidateRect( hwndList, NULL, FALSE );
         UpdateWindow( hwndList );
         fQuickUpdate = FALSE;
         return( TRUE );
         }
      }
   return( FALSE );
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL FlistSearchWnd_OnAdjustWindows( HWND hwnd, int dx, int dy )
{
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_LIST ), dx, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDOK ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_DOWNLOAD ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_FIND ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_HELP ), dx, 0 );
}

/* This function handles the WM_CLOSE message.
 */
void FASTCALL FlistSearchWnd_OnClose( HWND hwnd )
{
   Adm_DestroyMDIWindow( hwnd );
   hwndFlistSearch = NULL;
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL FlistSearchWnd_OnMove( HWND hwnd, int x, int y )
{
   if( !IsIconic( hwnd ) )
      Amuser_WriteWindowState( szFlistSearchWndName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL FlistSearchWnd_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   /* Save the new window state to the configuration file.
    */
   if( !IsIconic( hwnd ) )
      Amuser_WriteWindowState( szFlistSearchWndName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* This function handles the WM_DRAWITEM message
 */
void FASTCALL FlistSearchWnd_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   if( lpDrawItem->itemID != -1 )
      {
      COLORREF tmpT, tmpB;
      FILELISTITEM fli;
      HFONT hFont;
      HBRUSH hbr;
      SIZE size;
      PTL ptl;
      PCL pcl;
      RECT rc;
      int y;

      /* Get the folder handle.
       */
      ptl = (PTL)ListBox_GetItemData( lpDrawItem->hwndItem, lpDrawItem->itemID );
      if( NULL == ptl )
         return;

      /* Set the default painting colours.
       */
      ASSERT( Amdb_IsTopicPtr(ptl) );
      rc = lpDrawItem->rcItem;
      hbr = CreateSolidBrush( GetSysColor( COLOR_WINDOW ) );
      tmpT = SetTextColor( lpDrawItem->hDC, GetSysColor( COLOR_WINDOWTEXT ) );
      tmpB = SetBkColor( lpDrawItem->hDC, GetSysColor( COLOR_WINDOW ) );

      /* Start by filling in the icons.
       */
      if( lpDrawItem->itemAction & ODA_DRAWENTIRE )
         {
         rc.left = 0;
         rc.right = 19;
         FillRect( lpDrawItem->hDC, &rc, hbr );
         }

      /* Get the file list text
       */
      ListBox_GetText( lpDrawItem->hwndItem, lpDrawItem->itemID, lpTmpBuf );
      ParseFileListItem( lpTmpBuf, &fli );

      /* Draw the folder/topic name.
       */
      hFont = SelectFont( lpDrawItem->hDC, hHelvB8Font );
      GetTextExtentPoint( lpDrawItem->hDC, fli.szFileName, strlen(fli.szFileName), &size );
      y = rc.top + ( ( rc.bottom - rc.top ) - size.cy ) / 2;
      rc.left = 19;
      rc.right = rc.left + size.cx + 4;
      if( !( lpDrawItem->itemState & ODS_SELECTED ) )
         ExtTextOut( lpDrawItem->hDC, rc.left + 2, y, ETO_OPAQUE, &rc, fli.szFileName, strlen(fli.szFileName), NULL );
      else
         {
         SetTextColor( lpDrawItem->hDC, crHighlightText );
         SetBkColor( lpDrawItem->hDC, crHighlight );
         ExtTextOut( lpDrawItem->hDC, rc.left + 2, y, ETO_OPAQUE, &rc, fli.szFileName, strlen(fli.szFileName), NULL );
         SetTextColor( lpDrawItem->hDC, GetSysColor( COLOR_WINDOWTEXT ) );
         SetBkColor( lpDrawItem->hDC, GetSysColor( COLOR_WINDOW ) );
         }
      if( lpDrawItem->itemState & ODS_FOCUS )
         DrawFocusRect( lpDrawItem->hDC, &rc );
      rc.left = rc.right;
      rc.right = hdrColumns[ 0 ];
      FillRect( lpDrawItem->hDC, &rc, hbr );

      /* Only do these if repainting entire line.
       */
      if( lpDrawItem->itemAction & ODA_DRAWENTIRE )
         {
         /* Draw the folder/topic name.
          */
         rc.left = hdrColumns[ 0 ];
         rc.right = rc.left + hdrColumns[ 1 ];
         pcl = Amdb_FolderFromTopic( ptl );
         wsprintf( lpTmpBuf, "%s/%s", Amdb_GetFolderName( pcl ), Amdb_GetTopicName( ptl ) );
         ExtTextOut( lpDrawItem->hDC, rc.left + 2, y, ETO_OPAQUE, &rc, lpTmpBuf, strlen(lpTmpBuf), NULL );

         /* Draw the size.
          */
         rc.left = rc.right;
         rc.right = rc.left + hdrColumns[ 2 ];
         ExtTextOut( lpDrawItem->hDC, rc.left + 2, y, ETO_OPAQUE, &rc, fli.szSize, strlen(fli.szSize), NULL );

         /* Draw the description.
          */
         rc.left = rc.right;
         rc.right = rc.left + hdrColumns[ 3 ];
         ExtTextOut( lpDrawItem->hDC, rc.left + 2, y, ETO_OPAQUE, &rc, fli.szDescription, strlen(fli.szDescription), NULL );
         }

      /* Clean up afterwards.
       */
      SelectFont( lpDrawItem->hDC, hFont );
      SetTextColor( lpDrawItem->hDC, tmpT );
      SetBkColor( lpDrawItem->hDC, tmpB );
      DeleteBrush( hbr );
      }
}

/* This function handles the WM_DRAWITEM message
 */
void FASTCALL FlistSearchWnd_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   HFONT hFont;
   HBITMAP hbmp;
   BITMAP bmp;
   HDC hdc;

   hdc = GetDC( hwnd );
   hFont = SelectFont( hdc, hHelvB8Font );
   GetTextMetrics( hdc, &tm );
   hbmp = LoadBitmap( hInst, MAKEINTRESOURCE(IDB_DATABASE) );
   GetObject( hbmp, sizeof( BITMAP ), &bmp );
   DeleteBitmap( hbmp );
   SelectFont( hdc, hFont );
   ReleaseDC( hwnd, hdc );
   lpMeasureItem->itemHeight = max( tm.tmHeight + tm.tmExternalLeading, bmp.bmHeight + 2 );
}

void FASTCALL FlistSearchWnd_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   HWND hwndFocus;
   hwndFocus = GetDlgItem( hwndFlistSearch, IDD_LIST );
   SetFocus( hwndFocus );
}
