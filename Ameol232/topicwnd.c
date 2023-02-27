/* TOPICWND.C - Handles the message viewer window
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
#include <stdio.h>
#include <ctype.h>
#include <memory.h>
#include "mnugrp.h"
#include "listview.h"
#include <dos.h>
#include <io.h>
#include <shellapi.h>
#include "shared.h"
#include "cixip.h"
#include "cix.h"
#include "local.h"
#include "amlib.h"
#include "amaddr.h"
#include "editmap.h"
#include "ameol2.h"
#include "command.h"
#include "news.h"
#include "rules.h"
#include "cookie.h"
#ifndef USEBIGEDIT
#include "scintilla.h"
#include "scilexer.h"
#endif USEBIGEDIT

#define  THIS_FILE   __FILE__

#define  MAX_DISPLAY_MSG_SIZE    30000
#define  BIG_MAX_DISPLAY_MSG_SIZE   500000
#define  STATUS_COLUMN_DEPTH     64

#ifndef USEBIGEDIT
extern LOGFONT FAR lfMsgFont;             /* Message window font structure */
#endif USEBIGEDIT

HWND fLastFocusPane;

typedef BOOL (FAR PASCAL * IMPORTPROC)(LPSTR, LPSTR);
typedef BOOL (FAR PASCAL * SAVEIMPORTPROC)(LPSTR, LPSTR, LPSTR);

LPSTR GetReadableText(PTL ptl, LPSTR pSource);
void FASTCALL SaveAttachment(LPSTR pSource, LPSTR pName, LPSTR pNum, PTH pth);
void FASTCALL OpenAttachment(LPSTR pSource, LPSTR pName, LPSTR pNum, PTH pth); // !!SM!! 2044


WINCOMMCTRLAPI void WINAPI InitCommonControls(void);

char szMsgViewWndClass[] = "amctl_msgviewcls";
char szInfoBarWndClass[] = "amctl_infobar";

char szMsgViewWndName[] = "Message Viewer";

static BOOL fDefDlgEx = FALSE;            /* DefDlg recursion flag trap */
static BOOL fRegistered = FALSE;          /* TRUE if we've registered the message viewer window class */

#define  GetSortMode(v)       (((v)->nSortMode) & 0x7F)
#define  SetViewMode(v,m)     ((v)->nViewMode = (GetSortMode((v))==VSM_REFERENCE)?VM_VIEWREF:(GetSortMode((v))==VSM_ROOTS)?VM_VIEWROOTS:VM_VIEWCHRON)

static char FAR str6[] = "Message \x007%lu\x006 to \x007%s\x006 ";
static char FAR str7[] = "Message \x007%lu\x006 from \x007%s\x006 ";
static char FAR str8[] = "on \x007%s\x006 at \x007%s\x006.";
static char FAR str9[] = "  Comment to message \x007%lu\x006.";
static char FAR str11[] = "  \x007 1\x006 unread message in folder.";
static char FAR str12[] = "  \x007%lu\x006 unread messages in folder.";
static char FAR str25[] = " (TOPIC FULL)";

static char * szHeaderCaption[] = {
   "Number",
   "Name",
   "Date",
   "Size",
   "Subject"
   };

static char FAR szRenamedFilename[ _MAX_FNAME ];   /* A filename */

static BOOL fInitialDisplay;           /* TRUE if this is the first display of the message window */
static BOOL fCreating;                 /* TRUE if we're creating the message window */
static HBITMAP hbmpThreadBitmaps;      /* Handle of thread window bitmaps */
static int cMsgWnds = 0;               /* Number of simultaneous open message windows */
static POINT ptLast;                   /* Last click coordinates */
static BOOL fQuickUpdate = FALSE;      /* Fast listbox update */
static WNDPROC lpProcListCtl = NULL;   /* Procedure address of list window */
static WNDPROC lpProcEditCtl = NULL;   /* Procedure address of edit window */
static WNDPROC lpProcTreeCtl = NULL;   /* Procedure address of folder window */
static BOOL fKeyboard = FALSE;         /* TRUE if the last action via on the keyboard */
static BOOL fMotionUp = FALSE;         /* TRUE if the cursor up key was pressed */
static HBRUSH hbrThreadWnd = NULL;     /* Handle of thread pane brush */
static HBRUSH hbrInBaskWnd;            /* Handle of in-basket window brush */

static BOOL fLastCopy = FALSE;         /* TRUE if ptlLastSrc/pclLastSrc are valid */
static PTL ptlLastSrc;                 /* Handle of last topic message copied to */
static PCL pclLastSrc;                 /* Handle of last conference message copied to */

BOOL fDragging = FALSE;                /* TRUE if we're drag/dropping from listbox */
BOOL fInBasket = FALSE;                /* TRUE if last drag was over In Basket */
BOOL fMoved = FALSE;                   /* TRUE if we moved while dragging */
HCURSOR hDragCursor;                   /* Handle of drag/drop cursor */
HCURSOR hNoNoCursor;                   /* Handle of stop cursor */
HCURSOR hCurDragCursor;                /* Handle of the current cursor */
HCURSOR hOldCursor;                    /* Handle of the original cursor */
HTREEITEM tvSelItem = 0L;              /* Selected item in folder list before drag */
BOOL fDragMove = FALSE;                /* TRUE if the drag action is a move rather than a copy */
BOOL fTopicClosed = FALSE;             /* Set TRUE if a topic is closed */
BOOL fKeepUnread = FALSE;              /* TRUE if IDM_NEXT does not mark current read */
LPSTR lpszPreText;

extern BOOL fWordWrap;

#define  MAX_BOOKMARKS     20

typedef struct tagBOOKMARK {
   PCL pcl;
   PTL ptl;
   DWORD dwMsg;
   BOOL fUnread;
   char * pszTitle;
} BOOKMARK;

int FASTCALL b64decodePartial(char *d); //!!SM!!

static BOOKMARK FAR cmBookMark[ MAX_BOOKMARKS ];   /* Bookmarks */
static int nBookPtr = 0;                           /* Index of current bookmark */

BOOL FASTCALL CheckThreadOpen( HWND hwnd );

BOOL FASTCALL MsgViewWnd_OnCreate( HWND, CREATESTRUCT FAR * );
void FASTCALL MsgViewWnd_OnSize( HWND, UINT, int, int );
void FASTCALL MsgViewWnd_OnMove( HWND, int, int );
void FASTCALL MsgViewWnd_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
void FASTCALL MsgViewWnd_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
int FASTCALL MsgViewWnd_OnCompareItem( HWND, const COMPAREITEMSTRUCT FAR * );
void FASTCALL MsgViewWnd_MDIActivate( HWND, BOOL, HWND, HWND );
void FASTCALL MsgViewWnd_OnDestroy( HWND );
BOOL FASTCALL MsgViewWnd_OnClose( HWND );
void FASTCALL MsgViewWnd_OnCommand( HWND, int, HWND, UINT );
void FASTCALL MsgViewWnd_OnSysCommand( HWND, UINT, int, int );
BOOL FASTCALL MsgViewWnd_OnEraseBkgnd( HWND, HDC );
LRESULT FASTCALL MsgViewWnd_OnNotify( HWND, int, LPNMHDR );
void FASTCALL MsgViewWnd_OnSetFocus( HWND, HWND );
void FASTCALL MsgViewWnd_OnKillFocus( HWND, HWND );
HBRUSH FASTCALL MsgViewWnd_OnCtlColor( HWND, HDC, HWND, int );
void FASTCALL MsgViewWnd_OnDeleteCategory( HWND, BOOL, PCAT );
void FASTCALL MsgViewWnd_OnDeleteFolder( HWND, BOOL, PCL );
void FASTCALL MsgViewWnd_OnDeleteTopic( HWND, BOOL, PTL );
void FASTCALL MsgViewWnd_OnCategoryChanged( HWND, int, PCAT );
void FASTCALL MsgViewWnd_OnFolderChanged( HWND, int, PCL );
void FASTCALL MsgViewWnd_OnTopicChanged( HWND, int, PTL );

LRESULT EXPORT CALLBACK MsgViewWndProc( HWND, UINT, WPARAM, LPARAM );

void FASTCALL SetMsgViewerWindow( HWND, PTL, int );
void FASTCALL RedrawInfoBar( HWND, HDC, PTH );
BOOL FASTCALL IsMsgCentered( HWND );
void FASTCALL CenterCurrentMsg( HWND );
BOOL FASTCALL InternalSetCurrentMsg( PTL, PTH, BOOL, BOOL );
void FASTCALL RemoveBookmark( PCL, PTL, PTH );
void FASTCALL RemoveAllBookmarks( void );
HWND FASTCALL FindMsgViewerWindow( HWND, PCL );
void FASTCALL SetMessageWindowTitle( HWND, PTL );
void FASTCALL InternalSetMsgBookmark( BOOL, BOOL );
void FASTCALL UpdateStatusDisplay( HWND );
void FASTCALL UpdateThreadDisplay( HWND, BOOL );
void FASTCALL ForceFocusToList( HWND );
void FASTCALL DownloadMsg( HWND, PTL, DWORD );
BOOL FASTCALL Common_KeyDown( HWND, WPARAM );
void FASTCALL SaveHeaderColumnWidths( HWND, HWND );
void FASTCALL UpdateAttachmentPane( HWND, PTH );
void FASTCALL ShowHeaderColumns( HWND );
void FASTCALL CopyMessageRange( HWND, PTH FAR *, BOOL, BOOL );
BOOL FASTCALL MoveToNextUnreadMessage( HWND, BOOL );
void FASTCALL MoveToNextMessage( HWND );
void FASTCALL ShowColumn( HWND, int );
void FASTCALL MoveToNextThread( HWND );
void FASTCALL CmdAddToAddrbook( HWND );
void FASTCALL CmdCoverNote( HWND );
PTH FAR * FASTCALL GetThreadHandles( HWND );
PTH FAR * FASTCALL CvtItemsToHandles( HWND, LPINT );
void FASTCALL CmdForwardMessage( HWND );
void FASTCALL SetSortMode( VIEWDATA *, int );
int FASTCALL GetNearestUndeleted( HWND, int );
void FASTCALL GetTopicFromTreeView( HWND, CURMSG FAR * );
BOOL FASTCALL SafeToOpen( HWND, char * );
void FASTCALL AdvanceAfterMarking( HWND, int );
void FASTCALL HandleNotification( HWND, PTH );
void FASTCALL CmdDisplayInBrowser( HWND, PTH );

void FASTCALL MarkMsgRange( HWND, LPINT );
void FASTCALL MarkTaggedRange( HWND, LPINT );
void FASTCALL MarkWatchRange( HWND, LPINT );
void FASTCALL MarkIgnoreRange( HWND, LPINT );
void FASTCALL MarkKeepRange( HWND, LPINT );
void FASTCALL MarkPriorityRange( HWND, LPINT );
void FASTCALL MarkReadLockRange( HWND, LPINT );
void FASTCALL MarkReadRange( HWND, LPINT );
void FASTCALL DeleteMsgRange( HWND, PTH FAR *, BOOL );
void FASTCALL MarkMsgWithdrawn( HWND, LPINT );
void FASTCALL MarkCIXMsgWithdrawn( HWND, PTH );
void FASTCALL MarkNewsMsgWithdrawn( HWND, PTH );

LRESULT EXPORT FAR PASCAL MessagePaneProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT FAR PASCAL ThreadPaneProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT FAR PASCAL FolderListProc( HWND, UINT, WPARAM, LPARAM );

LONG EXPORT CALLBACK InfoBarWndProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL InfoBarWnd_OnPaint( HWND );

BOOL EXPORT FAR PASCAL Goto( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL Goto_DlgProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL Goto_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL Goto_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT FAR PASCAL QueryOpenFile( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL QueryOpenFile_DlgProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL QueryOpenFile_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL QueryOpenFile_OnCommand( HWND, int, HWND, UINT );

void FASTCALL CreateReplyAddress( RECPTR *, PTL );


void FASTCALL StripLineBreaks(LPSTR pStr)
{
   DWORD i,j, len;
   LPSTR lTemp;

   len = strlen(pStr) + 1;

   INITIALISE_PTR(lTemp);
   if( !fNewMemory( &lTemp, len ) )
      return;

   i = 0;
   j = 0;
   while( (pStr[i]) && (i < len) )
   {     
      if( (pStr[i] != '\n') && (pStr[i] != '\r') )
      {
         lTemp[j] = pStr[i];
         j++;
      }
      i++;
   }
   lTemp[j] = '\0';
   strcpy(pStr, lTemp);

   FreeMemory( &lTemp );
}

void FASTCALL StripPercent20(LPSTR pStr)
{
   DWORD i,j, len;
   LPSTR lTemp;

   StripLineBreaks(pStr);

   len = strlen(pStr) + 1;

   if( _strnicmp( pStr, "file://", 7 ) == 0 )
   {
      INITIALISE_PTR(lTemp);
      if( !fNewMemory( &lTemp, len ) )
         return;

      i = 0;
      j = 0;
      while( (pStr[i]) && (i < len) )
      {     
         if( (i < len-2) && (pStr[i] == '%') && (pStr[i+1] == '2') && (pStr[i+2] == '0') )
         {
            lTemp[j] = ' ';
            i += 3;
         }
         else
         {
            lTemp[j] = pStr[i];
            i++;
         }
         j++;
      }
      strcpy(pStr, lTemp);

      FreeMemory( &lTemp );
   }
}

/* This function opens a window on the specified topic and
 * sets the cursor to the last thread OR the first unread
 * message in that topic.
 */
BOOL FASTCALL OpenTopicViewerWindow( PTL ptl )
{
   TOPICINFO topicinfo;
   CURMSG unr;

   Amdb_LockTopic( ptl );
   unr.ptl = ptl;
   unr.pcl = Amdb_FolderFromTopic( ptl );
   unr.pcat = Amdb_CategoryFromFolder( unr.pcl );
   unr.pth = Amdb_GetLastMsg( ptl, VM_VIEWREF );
   unr.pth = Amdb_ExGetRootMsg( unr.pth, TRUE, VM_VIEWREF );
   Amdb_GetTopicInfo( unr.ptl, &topicinfo );
   if( topicinfo.cUnRead )
      {
      unr.pth = NULL;
      Amdb_GetNextUnReadMsg( &unr, VM_VIEWREF );
      Amdb_InternalUnlockTopic( unr.ptl );
      }
   if( NULL == hwndTopic )
      OpenMsgViewerWindow( unr.ptl, unr.pth, FALSE );
   else
      {
      InternalSetMsgBookmark( FALSE, FALSE );
      SetMsgViewerWindow( hwndTopic, unr.ptl, 0 );
      InternalSetCurrentMsg( unr.ptl, unr.pth, TRUE, TRUE );
      }
   Amdb_UnlockTopic( unr.ptl );
   return( TRUE );
}

/* This function creates a new message viewer window.
 */
BOOL FASTCALL OpenMsgViewerWindow( PTL ptl, PTH pth, BOOL fNew )
{
   MDICREATESTRUCT mcs;
   DWORD dwState;
   RECT rc;

   /* Register the message viewer window class if we have
    * not already done so.
    */
   if( !fRegistered )
      {
      WNDCLASS wc;

      wc.style       = CS_HREDRAW | CS_VREDRAW;
      wc.lpfnWndProc    = MsgViewWndProc;
      wc.hIcon       = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_TOPIC) );
      wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
      wc.lpszMenuName      = NULL;
      wc.cbWndExtra     = GWW_EXTRA;
      wc.cbClsExtra     = 0;
      wc.hbrBackground  = (HBRUSH)( COLOR_APPWORKSPACE + 1 );
      wc.lpszClassName  = szMsgViewWndClass;
      wc.hInstance      = hInst;
      if( !RegisterClass( &wc ) )
         return( FALSE );

      /* Register the info bar window class
       */
      wc.style       = CS_HREDRAW | CS_VREDRAW;
      wc.lpfnWndProc    = InfoBarWndProc;
      wc.hIcon       = NULL;
      wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
      wc.lpszMenuName      = NULL;
      wc.cbWndExtra     = 0;
      wc.cbClsExtra     = 0;
      wc.hbrBackground  = (HBRUSH)( COLOR_APPWORKSPACE + 1 );
      wc.lpszClassName  = szInfoBarWndClass;
      wc.hInstance      = hInst;
      if( !RegisterClass( &wc ) )
         return( FALSE );

      fRegistered = TRUE;
      }

   /* If there is a message window already open and fNew is FALSE, then
    * re-use that window.
    */
   fInitialDisplay = FALSE;
   if( hwndMsg && !fNew )
      BringWindowToTop( hwndMsg );
   else {
      HWND hwnd;

      /* The default position of the message viewer window.
       */
      GetClientRect( hwndMDIClient, &rc );
//    rc.left = rc.right / 3;
      dwState = dwDefWindowState;

      /* Get the actual window size, position and state.
       */
      Amuser_ReadWindowState( szMsgViewWndName, &rc, &dwState );
      if( hwndActive && IsMaximized( hwndActive ) )
         dwState = WS_MAXIMIZE;

      /* Create the window.
       */
      mcs.szTitle = "";
      mcs.szClass = szMsgViewWndClass;
      mcs.hOwner = hInst;
      mcs.x = rc.left;
      mcs.y = rc.top;
      mcs.cx = rc.right - rc.left;
      mcs.cy = rc.bottom - rc.top;
      mcs.style = dwState;
      mcs.lParam = 0L;

      /* If we're opening a new window, offset this one from the existing
       * window.
       */
      if( fNew && hwndMsg )
         {
         RECT rc, rc2;
         int dx, dy;

         GetClientRect( hwndMDIClient, &rc );
         if( IsIconic( hwndMsg ) )
            SetRect( &rc2, 0, 0, rc.right, rc.bottom );
         else
            {
            GetWindowRect( hwndMsg, &rc2 );
            ScreenToClient( hwndMDIClient, (LPPOINT)&rc2 );
            ScreenToClient( hwndMDIClient, ((LPPOINT)&rc2)+1 );
            }
         dx = GetSystemMetrics( SM_CXSIZE ) + GetSystemMetrics( SM_CXFRAME ) + GetSystemMetrics( SM_CXBORDER );
         dy = GetSystemMetrics( SM_CYSIZE ) + GetSystemMetrics( SM_CYFRAME ) + GetSystemMetrics( SM_CYBORDER );
         mcs.x = rc2.left + dx;
         mcs.y = rc2.top + dy;
         if( mcs.x + mcs.cx > rc.right )
            mcs.cx -= dx;
         if( mcs.y + mcs.cy > rc.bottom )
            mcs.cy -= dy;
         if( mcs.cx < 100 || mcs.cy < 100 )
            {
            mcs.x = mcs.y = 0;
            mcs.cx = rc.right / 2;
            mcs.cy = rc.bottom - ( rc.bottom / 3 );
            }
         }

      /* Constrain the new window coordinates so that they don't
       * go off the edge of the MDI client window.
       */
      SetRect( &rc, mcs.x, mcs.y, mcs.x + mcs.cx, mcs.y + mcs.cy );
      ConstrainWindow( &rc );
      mcs.x = rc.left;
      mcs.y = rc.top;
      mcs.cx = rc.right - rc.left;
      mcs.cy = rc.bottom - rc.top;

      /* Create the message window.
       */
      fInitialDisplay = TRUE;
      fCreating = TRUE;
      hwnd = (HWND)SendMessage( hwndMDIClient, WM_MDICREATE, 0, (LPARAM)(LPMDICREATESTRUCT)&mcs );
      if( hwnd == NULL )
         return( FALSE );

      /* Set this as the current message window.
       */
      hwndMsg = hwnd;
      }

   /* Show the current topic.
    */
   InternalSetMsgBookmark( FALSE, FALSE );
   SetMsgViewerWindow( hwndMsg, ptl, 0 );
   InternalSetCurrentMsg( ptl, pth, TRUE, TRUE );
   
   /* Clear flags.
    */
   fCreating = FALSE;
   fInitialDisplay = FALSE;

   /* Show the window.
    */
   SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndMsg, 0L );
   return( TRUE );
}

/* This function handles all messages for the message viewer window.
 */
LRESULT EXPORT CALLBACK MsgViewWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, MsgViewWnd_OnCreate );
      HANDLE_MSG( hwnd, WM_COMMAND, MsgViewWnd_OnCommand );
      HANDLE_MSG( hwnd, WM_SIZE, MsgViewWnd_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, MsgViewWnd_OnMove );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, MsgViewWnd_MDIActivate );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, MsgViewWnd_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_DRAWITEM, MsgViewWnd_OnDrawItem );
      HANDLE_MSG( hwnd, WM_COMPAREITEM, MsgViewWnd_OnCompareItem );
      HANDLE_MSG( hwnd, WM_CLOSE, MsgViewWnd_OnClose );
      HANDLE_MSG( hwnd, WM_SYSCOMMAND, MsgViewWnd_OnSysCommand );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, MsgViewWnd_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_DESTROY, MsgViewWnd_OnDestroy );
      HANDLE_MSG( hwnd, WM_NOTIFY, MsgViewWnd_OnNotify );
      HANDLE_MSG( hwnd, WM_MENUSELECT, MainWnd_OnMenuSelect );
      HANDLE_MSG( hwnd, WM_SETFOCUS, MsgViewWnd_OnSetFocus );
      HANDLE_MSG( hwnd, WM_KILLFOCUS, MsgViewWnd_OnKillFocus );
   #ifdef WIN32
      HANDLE_MSG( hwnd, WM_CTLCOLORLISTBOX, MsgViewWnd_OnCtlColor );
      HANDLE_MSG( hwnd, WM_CTLCOLOREDIT, MsgViewWnd_OnCtlColor );
   #else
      HANDLE_MSG( hwnd, WM_CTLCOLOR, MsgViewWnd_OnCtlColor );
   #endif
      HANDLE_MSG( hwnd, WM_INSERTCATEGORY, InBasket_OnInsertCategory );
      HANDLE_MSG( hwnd, WM_INSERTFOLDER, InBasket_OnInsertFolder );
      HANDLE_MSG( hwnd, WM_INSERTTOPIC, InBasket_OnInsertTopic );
      HANDLE_MSG( hwnd, WM_DELETECATEGORY, MsgViewWnd_OnDeleteCategory );
      HANDLE_MSG( hwnd, WM_DELETEFOLDER, MsgViewWnd_OnDeleteFolder );
      HANDLE_MSG( hwnd, WM_DELETETOPIC, MsgViewWnd_OnDeleteTopic );
      HANDLE_MSG( hwnd, WM_DRAGDROP, InBasket_OnDragDrop );
      HANDLE_MSG( hwnd, WM_SETTOPIC, InBasket_OnSetTopic );
      HANDLE_MSG( hwnd, WM_CATEGORYCHANGED, MsgViewWnd_OnCategoryChanged );
      HANDLE_MSG( hwnd, WM_FOLDERCHANGED, MsgViewWnd_OnFolderChanged );
      HANDLE_MSG( hwnd, WM_TOPICCHANGED, MsgViewWnd_OnTopicChanged );
      HANDLE_MSG( hwnd, WM_UPDATE_UNREAD, InBasket_OnUpdateUnread );

      case WM_CLOSEFOLDER: {
         LPWINDINFO lpWindInfo;

         /* Close this window if the topic handle in
          * lParam1 matches.
          */
         lpWindInfo = GetBlock( hwnd );
         if( Amdb_IsTopicPtr( (PTL)lParam ) )
            {
            if( (PTL)lParam == lpWindInfo->lpTopic )
               {
               SendMessage( hwnd, WM_CLOSE, 0, 0L );
               fTopicClosed = TRUE;
               }
            }
         return( 0L );
         }

      case WM_REDRAWWINDOW:
         if( wParam & WIN_INBASK )
            {
            HWND hwndTreeCtl;

            hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
            Amdb_FillFolderTree( hwndTreeCtl, FTYPE_ALL|FTYPE_TOPICS, fShowTopicType );
            UpdateWindow( hwndTreeCtl );
            }
         return( 0L );

      case WM_LOCKUPDATES:
         if( WIN_THREAD & wParam )
            {
            LPWINDINFO lpWindInfo;

            lpWindInfo = GetBlock( hwnd );
            lpWindInfo->fLockUpdates = LOWORD( lParam );
            if( !lpWindInfo->fLockUpdates && lpWindInfo->fUpdatePending )
               {
               HWND hwndList;
               int nTopIndex;
               PTH pth;

               lpWindInfo->fUpdatePending = FALSE;
               VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
               nTopIndex = ListBox_GetTopIndex( hwndList );
               pth = lpWindInfo->lpMessage;
               SetMsgViewerWindow( hwnd, lpWindInfo->lpTopic, nTopIndex );
               ShowMsg( hwnd, pth, TRUE, TRUE, FALSE );
               }
            }
         if( WIN_INBASK & wParam )
            {
            HWND hwndTreeCtl;

            /* This message is broadcast whenever Ameol wants a particular
             * window to not update itself.
             */
            hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
            SetWindowRedraw( hwndTreeCtl, LOWORD( lParam ) );
            if( LOWORD( lParam ) )
               {
               InvalidateRect( hwndTreeCtl, NULL, TRUE );
               UpdateWindow( hwndTreeCtl );
               }
            }
         break;

      case WM_DELETEMSG: {
         LPWINDINFO lpWindInfo;

         /* Do nothing if this we're not showing the topic from which
          * this message is being deleted.
          */
         lpWindInfo = GetBlock( hwnd );
         if( Amdb_TopicFromMsg( (PTH)lParam ) == lpWindInfo->lpTopic )
            {
            HWND hwndList;
            int count;
            int index;
            CURMSG unr;

            /* Get the fred payne (c)Rikki handle.
             */
            VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
            if( (PTH)lParam == lpWindInfo->lpMessage )
               {
               /* We're deleting the highlighted message, so move
                * the highlight to the next message.
                */
               index = Amdb_GetMsgSel( (PTH)lParam );
               ASSERT( LB_ERR != index );
               unr.pth = NULL;
               unr.ptl = lpWindInfo->lpTopic;
               unr.pcl = Amdb_FolderFromTopic( unr.ptl );
               unr.pcat = Amdb_CategoryFromFolder( unr.pcl );
               index = GetNearestUndeleted( hwndList, index );
               if( index == LB_ERR )
                  {
                  /* All messages in topic vaped!
                   */
                  lpWindInfo->lpMessage = NULL;
                  }
               else
                  {
                  unr.pth = (PTH)ListBox_GetItemData( hwndList, index );
                  SetCurrentMsg( &unr, FALSE, wParam );
                  lpWindInfo->lpMessage = unr.pth;
                  }
               }

            /* Do nothing if updates locked out. Just set the
             * fUpdatePending flag which means that the thread
             * pane contains handles to messages now deleted.
             */
            if( lpWindInfo->fLockUpdates )
               {
               lpWindInfo->fUpdatePending = TRUE;

               /* Because we're not deleting the item from the list
                * box, we need some way of telling the rest of the
                * code that this item will no longer exist.
                * YH 21/06/96 12:50
                */
               index = Amdb_GetMsgSel( (PTH)lParam );
               ASSERT( LB_ERR != index );
               ListBox_SetItemData( hwndList, index, NULL );
               }
            else
               {
               /* Update.
                */
               index = Amdb_GetMsgSel( (PTH)lParam );
               if( LB_ERR != index )
                  {
                  /* If we're deleting from the middle of
                   * the list of messages, adjust the sel index for
                   * all subsequent messages.
                   */
                  count = ListBox_DeleteString( hwndList, index );
                  while( index < count )
                     {
                     unr.pth = (PTH)ListBox_GetItemData( hwndList, index );
                     Amdb_SetMsgSel( unr.pth, Amdb_GetMsgSel( unr.pth ) - 1 );
                     ++index;
                     }
                  }
               }
            }
         break;
         }

      case WM_HEADERSCHANGED: {
         HWND hwndList;

         ShowHeaderColumns( hwnd );
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         InvalidateRect( hwndList, NULL, TRUE );
         UpdateWindow( hwndList );
         return( 0L );
         }

      case WM_APPCOLOURCHANGE:
         if( wParam & WIN_THREAD )
            {
            HWND hwndList;

            /* Delete the thread window brush. This will cause it
             * to be reconstructed when the window is repainted.
             */
            if( hbrThreadWnd )
               {
               DeleteBrush( hbrThreadWnd );
               hbrThreadWnd = NULL;
               }

            /* Repaint the thread window.
             */
            VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
            InvalidateRect( hwndList, NULL, TRUE );
            UpdateWindow( hwndList );
            }
         if( wParam & WIN_MESSAGE )
            {
            HWND hwndEdit;
            LPWINDINFO lpWindInfo;

            lpWindInfo = GetBlock( hwnd );   

            /* Reset the colours.
             */
            VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_MESSAGE ) );
#ifdef USEBIGEDIT
            SendMessage( hwndEdit, BEM_SETTEXTCOLOUR, 0, (COLORREF)crMsgText );
            SendMessage( hwndEdit, BEM_SETBACKCOLOUR, 0, (COLORREF)crMsgWnd );
            SendMessage( hwndEdit, BEM_SETQUOTECOLOUR, TRUE, (COLORREF)crQuotedText );
            SendMessage( hwndEdit, BEM_SETHOTLINKCOLOUR, TRUE, (COLORREF)crHotlinks );

#else USEBIGEDIT
            if( lpWindInfo )
               SetEditorDefaults(hwndEdit, TRUE, lpWindInfo->fFixedFont ? lfFixedFont : lfMsgFont, fWordWrap, fHotLinks , fMsgStyles, crMsgText, crMsgWnd );
#endif USEBIGEDIT

            }
         if( wParam & WIN_INFOBAR )
            UpdateStatusDisplay( hwnd );
         if( wParam & WIN_INBASK )
            {
            HWND hwndList;

            if( NULL != hbrInBaskWnd )
               DeleteBrush( hbrInBaskWnd );
            hbrInBaskWnd = CreateSolidBrush( crInBaskWnd );
            VERIFY( hwndList = GetDlgItem( hwnd, IDD_FOLDERLIST ) );
            TreeView_SetTextColor( hwndList, crInBaskText );
            TreeView_SetBackColor( hwndList, crInBaskWnd );
            InvalidateRect( hwndList, NULL, TRUE );
            UpdateWindow( hwndList );
            }
         return( 0L );

      case WM_TOGGLEHEADERS: {
         LPWINDINFO lpWindInfo;
         HWND hwndEdit;
         HWND hwndResumeEdit = NULL;

         lpWindInfo = GetBlock( hwnd );
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_MESSAGE ) );
         if( hwndResumes )
            VERIFY( hwndResumeEdit = GetDlgItem( hwndResumes, IDD_EDIT ) );
         if( fMsgStyles )
            {
#ifdef USEBIGEDIT
            SetWindowStyle( hwndEdit, GetWindowStyle( hwndEdit ) | BES_USEATTRIBUTES );
            if( NULL != hwndResumeEdit )
               SetWindowStyle( hwndResumeEdit, GetWindowStyle( hwndResumeEdit ) | BES_USEATTRIBUTES );
            }
         else
            {
            SetWindowStyle( hwndEdit, GetWindowStyle( hwndEdit ) & ~BES_USEATTRIBUTES );
            if( NULL != hwndResumeEdit )
               SetWindowStyle( hwndResumeEdit, GetWindowStyle( hwndResumeEdit ) & ~BES_USEATTRIBUTES );
            }
#else USEBIGEDIT
            }
         if( lpWindInfo )
            SetEditorDefaults(hwndEdit, TRUE, lpWindInfo->fFixedFont ? lfFixedFont : lfMsgFont, fWordWrap, fHotLinks , fMsgStyles, crMsgText, crMsgWnd );
#endif USEBIGEDIT

#ifdef USEBIGEDIT
         SendMessage( hwndEdit, BEM_SETWORDWRAP, fWordWrap, 0L );
         SendMessage( hwndEdit, BEM_ENABLEHOTLINKS, fHotLinks, 0L );
#else USEBIGEDIT
  
#endif USEBIGEDIT

         if( NULL != hwndResumeEdit )
         {
#ifdef USEBIGEDIT
         SendMessage( hwndResumeEdit, BEM_SETWORDWRAP, fWordWrap, 0L );
         SendMessage( hwndResumeEdit, BEM_ENABLEHOTLINKS, fHotLinks, 0L );
         SendMessage( hwndResumeEdit, BEM_RESUMEDISPLAY, fHotLinks, 0L );
#else USEBIGEDIT
         if( lpWindInfo )
            SetEditorDefaults(hwndResumeEdit, TRUE, lpWindInfo->fFixedFont ? lfFixedFont : lfMsgFont, fWordWrap, fHotLinks , fMsgStyles, crMsgText, crMsgWnd );
#endif USEBIGEDIT
         }

         ShowMsg( hwnd, lpWindInfo->lpMessage, TRUE, TRUE, FALSE );
         break;
         }

      case WM_CHANGEMDIMENU: {
         HMENU hMenu;

         hMenu = GetSystemMenu( hwnd, FALSE );
         AppendMenu( hMenu, MF_SEPARATOR, 0, NULL );
         AppendMenu( hMenu, MF_BYCOMMAND|MF_STRING, IDM_SPLIT, GS(IDS_STR379) );
         if( wWinVer < 0x35F )
            DeleteMenu( hMenu, SC_TASKLIST, MF_BYCOMMAND );
         InsertMenu( hMenu, 8, MF_BYPOSITION, SC_NEXTWINDOW, "Nex&t\tCtrl+F6" );
         ModifyMenu( hMenu, SC_CLOSE, MF_BYCOMMAND, SC_CLOSE, "&Close\tCtrl+F4" );
         DrawMenuBar( hwnd );
         break;
         }

      case WM_NEWMSG: {
         LPWINDINFO lpWindInfo;
         PTH pth;

         /* Insert the message into the listbox
          */
         lpWindInfo = GetBlock( hwnd );
         pth = (PTH)lParam;
         ASSERT( pth != NULL );
         if( Amdb_TopicFromMsg( pth ) == lpWindInfo->lpTopic )
            {
            PTH pthNext;
            HWND hwndList;
            int index;

            /* Do nothing if updates locked out.
             */
            if( lpWindInfo->fLockUpdates || fParsing )
               {
               lpWindInfo->fUpdatePending = TRUE;
               break;
               }

            /* This goes in this topic, so find the message.
             */
            VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
            index = -1;
            if( pthNext = Amdb_GetNextMsg( pth, lpWindInfo->vd.nViewMode ) )
               index = Amdb_GetMsgSel( pthNext );
            index = ListBox_InsertString( hwndList, index, (LPARAM)(LPCSTR)pth );
            Amdb_SetMsgSel( pth, index );
            if( LB_ERR != index )
               {
               /* If we're inserting into the middle of
                * the list of messages, adjust the sel index for
                * all subsequent messages.
                */
               if( pthNext )
                  {
                  pthNext = Amdb_GetNextMsg( pthNext, lpWindInfo->vd.nViewMode );
                  while( pthNext )
                     {
                     Amdb_SetMsgSel( pthNext, Amdb_GetMsgSel( pthNext ) + 1 );
                     pthNext = Amdb_GetNextMsg( pthNext, lpWindInfo->vd.nViewMode );
                     }
                  }
               }
            }
         break;
         }

      case WM_UPDMSGFLAGS: {
         HWND hwndList;
         int index;
         RECT rc;

         /* Get the listbox handle.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );

         /* Get the index of the message in the listbox.
          */
         if( Amdb_IsTopicPtr( (PTL)lParam ) )
            {
            LPWINDINFO lpWindInfo;
            PTL ptl;

            /* If lParam is a topic handle, refresh the
             * entire thread pane.
             */
            ptl = (PTL)lParam;
            lpWindInfo = GetBlock( hwnd );
            if( lpWindInfo->lpTopic == ptl )
               {
               InvalidateRect( hwndList, NULL, FALSE );
               UpdateWindow( hwndList );
               }
            }
         else if( Amdb_IsMessagePtr( (PTH)lParam ) )
            {
            PTH pth;

            pth = (PTH)lParam;
            ASSERT( pth != NULL );
            index = Amdb_GetMsgSel( pth );
            ASSERT( index != LB_ERR );

            /* If this message is selected, update contents.
             */
            if( ListBox_GetCaretIndex( hwndList ) == index )
               ShowMsg( hwnd, pth, FALSE, TRUE, FALSE );

            /* Get the view rectangle of the message. If it is an
             * empty rectangle, do nothing. Otherwise invalidate it.
             */
            ListBox_GetItemRect( hwndList, index, &rc );
            if( !IsRectEmpty( &rc ) )
               {
               InvalidateRect( hwndList, &rc, FALSE );
               UpdateWindow( hwndList );
               }
            }
         UpdateStatusDisplay( hwnd );
         break;
         }

      case WM_TOGGLEFIXEDFONT: {
         LPWINDINFO lpWindInfo;
         HWND hwndEdit;

         /* Toggle fixed/normal font display.
          */
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_MESSAGE ) );
         lpWindInfo = GetBlock( hwnd );
         
#ifdef USEBIGEDIT
         if( lpWindInfo->fFixedFont )
            {
            SetWindowFont( hwndEdit, hMsgFont, TRUE );
            lpWindInfo->fFixedFont = FALSE;
            }
         else
            {
            SetWindowFont( hwndEdit, hFixedFont, TRUE );
            lpWindInfo->fFixedFont = TRUE;
            }
#else USEBIGEDIT
         if( lpWindInfo->fFixedFont )
            {
            SetEditorDefaults(hwndEdit, TRUE, lfMsgFont, fWordWrap, fHotLinks , fMsgStyles, crMsgText, crMsgWnd );
            lpWindInfo->fFixedFont = FALSE;
            }
         else
            {
            SetEditorDefaults(hwndEdit, TRUE, lfFixedFont, fWordWrap, fHotLinks , fMsgStyles, crMsgText, crMsgWnd );
            lpWindInfo->fFixedFont = TRUE;
            }
#endif USEBIGEDIT
         
         break;
         }

      case WM_CHANGEFONT:
         /* This message is sent when the user changes a font
          * from the Font Settings dialog. The wParam field is the
          * type of the font changed; we may receive a wParam that
          * doesn't apply to us!
          */
         if( wParam == FONTYP_THREAD || 0xFFFF == wParam )
            {
            MEASUREITEMSTRUCT mis;
            HWND hwndList;

            VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
            SendMessage( hwnd, WM_MEASUREITEM, IDD_LIST, (LPARAM)(LPSTR)&mis );
            SendMessage( hwndList, LB_SETITEMHEIGHT, 0, MAKELPARAM( mis.itemHeight, 0) );
            InvalidateRect( hwndList, NULL, TRUE );
            }
         if( wParam == FONTYP_MESSAGE || 0xFFFF == wParam || wParam == FONTYP_FIXEDFONT )
            {
            LPWINDINFO lpWindInfo;
            HWND hwndEdit;

            /* Only change message font if we're not currently
             * showing the fixed font version.
             */
            VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_MESSAGE ) );
            lpWindInfo = GetBlock( hwnd );
#ifdef USEBIGEDIT
            SendMessage( hwndEdit, BEM_COLOURQUOTES, (WPARAM)fColourQuotes, (LPARAM)(LPSTR)&szQuoteDelimiters );
            if( wParam == FONTYP_MESSAGE || 0xFFFF == wParam )
            {
               if( !lpWindInfo->fFixedFont )
                  SetWindowFont( hwndEdit, hMsgFont, FALSE );
            }
            else if( wParam == FONTYP_FIXEDFONT )
            {
               if( lpWindInfo->fFixedFont )
                  SetWindowFont( hwndEdit, hFixedFont, FALSE );
            }
#else USEBIGEDIT
            if( lpWindInfo )
               SetEditorDefaults(hwndEdit, TRUE, lpWindInfo->fFixedFont ? lfFixedFont : lfMsgFont, fWordWrap, fHotLinks , fMsgStyles, crMsgText, crMsgWnd );
#endif USEBIGEDIT
            InvalidateRect( hwndEdit, NULL, TRUE );
            UpdateWindow( hwndEdit );
            }
         if( wParam == FONTYP_INBASKET || 0xFFFF == wParam )
            {
            HWND hwndTreeCtl;

            VERIFY( hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST ) );
            SetWindowFont( hwndTreeCtl, hInBasketFont, TRUE );
            }
         return( 0L );

      case WM_VIEWCHANGE: {
         LPWINDINFO lpWindInfo;
         HWND hwndList;
         PTL ptl;
         PTH pth;

         lpWindInfo = GetBlock( hwnd );
         ptl = lpWindInfo->lpTopic;
         fInitialDisplay = TRUE;
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         pth = lpWindInfo->lpMessage;
         SetMsgViewerWindow( hwnd, ptl, ListBox_GetTopIndex( hwndList ) );
         if( pth )
            pth = Amdb_GetNearestMsg( pth, lpWindInfo->vd.nViewMode );
         else
            pth = Amdb_GetFirstMsg( ptl, lpWindInfo->vd.nViewMode );
         ShowMsg( hwnd, pth, TRUE, TRUE, FALSE );
         break;
         }

      default:
         break;
      }
   return( DefMDIChildProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_CATEGORYCHANGED message.
 */
void FASTCALL MsgViewWnd_OnCategoryChanged( HWND hwnd, int type, PCAT pcat )
{
   if( type == AECHG_NAME );
      InbasketCategoryNameChanged( hwnd, pcat );
}

/* This function handles the WM_FOLDERCHANGED message.
 */
void FASTCALL MsgViewWnd_OnFolderChanged( HWND hwnd, int type, PCL pcl )
{
   if( type == AECHG_NAME )
      {
      LPWINDINFO lpWindInfo;

      /* Update the caption bar on the message viewer window.
       */
      lpWindInfo = GetBlock( hwnd );
      if( pcl == Amdb_FolderFromTopic( lpWindInfo->lpTopic ) )
         SetMessageWindowTitle( hwnd, lpWindInfo->lpTopic );

      /* Now change the folder label.
       */
      InbasketFolderNameChanged( hwnd, pcl );
      }
}

/* This function handles the WM_TOPICCHANGED message.
 */
void FASTCALL MsgViewWnd_OnTopicChanged( HWND hwnd, int type, PTL ptl )
{
   if( type == AECHG_NAME || type == AECHG_FLAGS )
      {
      LPWINDINFO lpWindInfo;

      /* Update the caption bar on the message viewer window.
       */
      lpWindInfo = GetBlock( hwnd );
      if( ptl == lpWindInfo->lpTopic )
         SetMessageWindowTitle( hwnd, lpWindInfo->lpTopic );

      /* Now change the folder label.
       */
      InbasketTopicNameChanged( hwnd, ptl );
      }
}
/* This function updates the category label in the inbasket with
 * the new label following a WM_TOPICCHANGED event.
 */
void FASTCALL InbasketCategoryNameChanged( HWND hwnd, PCAT pcat )
{
   HWND hwndTreeCtl;
   HTREEITEM hTreeItem;

   hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );           
   if( 0L != ( hTreeItem = Amdb_FindCategoryItem( hwndTreeCtl, pcat ) ) )
      {
      TV_ITEM tv;

      /* The category name is changed, so update the
       * folder window.
       */
      tv.hItem = hTreeItem;
      tv.pszText = (LPSTR)Amdb_GetCategoryName( pcat );
      tv.mask = TVIF_TEXT;
      TreeView_SetItem( hwndTreeCtl, &tv );
      }
   SaveRules();
}

/* This function updates the folder label in the inbasket with
 * the new label following a WM_TOPICCHANGED event.
 */
void FASTCALL InbasketFolderNameChanged( HWND hwnd, PCL pcl )
{
   HWND hwndTreeCtl;
   HTREEITEM hTreeItem;

   /* Update the folder list window.
    */
   hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );           
   if( 0L != ( hTreeItem = Amdb_FindFolderItem( hwndTreeCtl, pcl ) ) )
      {
      TV_ITEM tv;

      /* Update the folder window.
       */
      tv.hItem = hTreeItem;
      tv.pszText = (LPSTR)Amdb_GetFolderName( pcl );
      tv.mask = TVIF_TEXT;
      TreeView_SetItem( hwndTreeCtl, &tv );
      }
   SaveRules();
}

/* This function updates the topic label in the inbasket with
 * the new label following a WM_TOPICCHANGED event.
 */
void FASTCALL InbasketTopicNameChanged( HWND hwnd, PTL ptl )
{
   HWND hwndTreeCtl;
   HTREEITEM hTreeItem;

   /* Update the folder list window.
    */
   hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );           
   if( 0L != ( hTreeItem = Amdb_FindTopicItem( hwndTreeCtl, ptl ) ) )
      {
      TV_ITEM tv;

      /* Update the folder window.
       */
      tv.hItem = hTreeItem;
      tv.pszText = (LPSTR)Amdb_GetTopicName( ptl );
      tv.mask = TVIF_TEXT;
      TreeView_SetItem( hwndTreeCtl, &tv );
      }
   SaveRules();
}

/* This function handles the WM_DELETECATEGORY message.
 */
void FASTCALL MsgViewWnd_OnDeleteCategory( HWND hwnd, BOOL fMove, PCAT pcat )
{
   LPWINDINFO lpWindInfo;
   PCL pcl;
   PTL ptl;

   lpWindInfo = GetBlock( hwnd );
   ptl = lpWindInfo->lpTopic;
   pcl = Amdb_FolderFromTopic( ptl );
   if( !fMove && Amdb_CategoryFromFolder( pcl ) == pcat )
      SendMessage( hwnd, WM_CLOSE, 0, 0L );
   else
      {
      InBasket_OnDeleteCategory( hwnd, fMove, pcat );
      if( !fMove )
         RemoveBookmark( pcl, NULL, NULL );
      fTopicClosed = TRUE;
      }
}

/* This function handles the WM_DELETEFOLDER message.
 */
void FASTCALL MsgViewWnd_OnDeleteFolder( HWND hwnd, BOOL fMove, PCL pcl )
{
   PTL ptl;
   LPWINDINFO lpWindInfo;

   lpWindInfo = GetBlock( hwnd );
   ptl = lpWindInfo->lpTopic;
   if( !fMove && Amdb_FolderFromTopic( ptl ) == pcl )
      SendMessage( hwnd, WM_CLOSE, 0, 0L );
   else
      {
      InBasket_OnDeleteFolder( hwnd, fMove, pcl );
      if( !fMove )
         RemoveBookmark( pcl, NULL, NULL );
      }
}

/* This function handles the WM_DELETETOPIC message.
 */
void FASTCALL MsgViewWnd_OnDeleteTopic( HWND hwnd, BOOL fMove, PTL ptl )
{
   LPWINDINFO lpWindInfo;

   lpWindInfo = GetBlock( hwnd );
   if( !fMove && lpWindInfo->lpTopic == ptl )
      SendMessage( hwnd, WM_CLOSE, 0, 0L );
   else
      {
      InBasket_OnDeleteTopic( hwnd, fMove, ptl );
      if( !fMove )
         RemoveBookmark( NULL, ptl, NULL );
      }
}

/* This function handles the WM_CTLCOLOR message.
 */
HBRUSH FASTCALL MsgViewWnd_OnCtlColor( HWND hwnd, HDC hdc, HWND hwndChild, int type )
{
   switch( GetDlgCtrlID( hwndChild ) )
      {
      case IDD_FOLDERLIST:
         if( !hbrInBaskWnd )
            hbrInBaskWnd = CreateSolidBrush( crInBaskWnd );
         SetBkColor( hdc, crInBaskWnd );
         SetTextColor( hdc, crInBaskText );
         return( hbrInBaskWnd );

      case IDD_LIST:
         if( !hbrThreadWnd )
            hbrThreadWnd = CreateSolidBrush( crThreadWnd );
         SetBkColor( hdc, crThreadWnd );
         SetTextColor( hdc, crNormalMsg );
         return( hbrThreadWnd );
      }
   return( NULL );
}

/* This function handles the WM_ERASEBKGND message.
 */
BOOL FASTCALL MsgViewWnd_OnEraseBkgnd( HWND hwnd, HDC hdc )
{
   return( TRUE );
}


/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL MsgViewWnd_OnCreate( HWND hwnd, CREATESTRUCT FAR * lpCreateStruct )
{
   LPWINDINFO lpWindInfo;

   INITIALISE_PTR(lpWindInfo);
   if( fNewMemory( &lpWindInfo, sizeof( WINDINFO ) ) )
      {
      static int aArray[ 5 ] = { 90, 200, 136, 46, 600 };
      int aSplits[ 2 ];
      HWND hwndFolderList;
      HWND hwndEdit;
      HIMAGELIST himl;
      HWND hwndHdr;
      HD_ITEM hdi;
      DWORD dwAttr;

      /* Read the current header settings.
       */
      Amuser_GetPPArray( szMsgViewerWnd, "ThreadPaneColumns", aArray, sizeof(aArray) );

      /* Create the header control window.
       */
      hwndHdr = CreateWindow( WC_HEADER, "", WS_CHILD|WS_VISIBLE|ACTL_CCS_NOPARENTALIGN|HDS_BUTTONS|
                              ACTL_CCS_NORESIZE, 0, 0, 0, 0, hwnd, (HMENU)IDD_MSGIDTITLE, hInst, NULL );

      /* Fill in the controls.
       */
      hdi.mask = HDI_TEXT|HDI_WIDTH|HDI_FORMAT;
      hdi.cxy = aArray[ 0 ];
      hdi.pszText = GS(IDS_STR380);
      hdi.fmt = HDF_STRING|HDF_LEFT|HDF_OWNERDRAW;
      Header_InsertItem( hwndHdr, 0, &hdi );
      
      hdi.cxy = aArray[ 1 ];
      hdi.pszText = GS(IDS_STR381);
      hdi.fmt = HDF_STRING|HDF_LEFT|HDF_OWNERDRAW;
      Header_InsertItem( hwndHdr, 1, &hdi );

      hdi.cxy = aArray[ 2 ];
      hdi.pszText = GS(IDS_STR907);
      hdi.fmt = HDF_STRING|HDF_LEFT|HDF_OWNERDRAW;
      Header_InsertItem( hwndHdr, 2, &hdi );

      hdi.cxy = aArray[ 3 ];
      hdi.pszText = GS(IDS_STR906);
      hdi.fmt = HDF_STRING|HDF_LEFT|HDF_OWNERDRAW;
      Header_InsertItem( hwndHdr, 3, &hdi );

      hdi.cxy = aArray[ 4 ];
      hdi.pszText = GS(IDS_STR382);
      hdi.fmt = HDF_STRING|HDF_LEFT|HDF_OWNERDRAW;
      Header_InsertItem( hwndHdr, 4, &hdi );
      SendMessage( hwndHdr, WM_SETTEXT, 4 | HBT_SPRING, 0L );
      SetWindowFont( hwndHdr, hHelv8Font, FALSE );

      /* If this is the first message window, load the thread bitmaps.
       * (MUST be done before we create the listbox!)
       */
      if( cMsgWnds++ == 0 )
         hbmpThreadBitmaps = LoadBitmap( hRscLib, MAKEINTRESOURCE(IDB_THREADBITMAPS) );
      ASSERT( NULL != hbmpThreadBitmaps );

      /* Create the attachments window, which will remain invisible until
       * we display a message with attachments.
       */
      // 20060325 - 2.56.2051.22
      CreateWindow( LISTVIEW_CLASS, "", WS_CHILD|WS_HSCROLL, 0, 0, 0, 0, hwnd, (HMENU)IDD_ATTACHMENT, hInst, NULL );

      //    SetWindowStyle( hwndEdit, GetWindowStyle( hwndEdit ) | BES_USEATTRIBUTES );

      /* Create the listbox window which will display the threads.
       */
      CreateWindow( "listbox", "", LBS_OWNERDRAWFIXED | WS_VISIBLE | LBS_EXTENDEDSEL |
                       WS_CHILD | LBS_NOTIFY | LBS_DISABLENOSCROLL | LBS_SORT | WS_VSCROLL,
                       0, 0, 0, 0, hwnd, (HMENU)IDD_LIST, hInst, NULL );

      /* Create the treeview window which will display the threads.
       */
      hwndFolderList = CreateWindow( WC_TREEVIEW, "", WS_VSCROLL | WS_VISIBLE | WS_CHILD |
                       TVS_OWNERDRAW|TVS_HASBUTTONS|TVS_HASLINES|TVS_DISABLEDRAGDROP,
                       0, 0, 0, 0, hwnd, (HMENU)IDD_FOLDERLIST, hInst, NULL );
      SetWindowFont( hwndFolderList, hInBasketFont, FALSE );

      /* Create the split windows.
       */
      Amctl_CreateSplitterWindow( hwnd, IDD_HSPLIT, TRUE );
      Amctl_CreateSplitterWindow( hwnd, IDD_VSPLIT, FALSE );
      
      /* Create the info bar window which will display status about the selected
       * message.
       */
      CreateWindow( szInfoBarWndClass, "", WS_VISIBLE | WS_CHILD, 0, 0, 0, 0,
                       hwnd, (HMENU)IDD_STATUS, hInst, NULL );
   
      /* Create an edit control which can display large messages
       * Use this for the message window
       */
      dwAttr = WS_VISIBLE|WS_CHILD|WS_VSCROLL|WS_HSCROLL|ES_READONLY;
      if( fMsgStyles )
         dwAttr |= BES_USEATTRIBUTES;

#ifdef USEBIGEDIT
      hwndEdit = CreateWindow( WC_BIGEDIT, "", dwAttr,
               0, 0, 0, 0, hwnd, (HMENU)IDD_MESSAGE, (HINSTANCE)hInst, NULL );

      SendMessage( hwndEdit, BEM_SETTEXTCOLOUR, 0, (COLORREF)crMsgText );
      SendMessage( hwndEdit, BEM_SETBACKCOLOUR, 0, (COLORREF)crMsgWnd );
      SendMessage( hwndEdit, BEM_SETQUOTECOLOUR, 0, (COLORREF)crQuotedText );
      SendMessage( hwndEdit, BEM_SETHOTLINKCOLOUR, 0, (COLORREF)crHotlinks );
      SendMessage( hwndEdit, BEM_SETWORDWRAP, fWordWrap, 0L );

      /* Set quoting in the edit control.
       */

      SendMessage( hwndEdit, BEM_COLOURQUOTES, (WPARAM)fColourQuotes, (LPARAM)(LPSTR)&szQuoteDelimiters );
      SendMessage( hwndEdit, BEM_ENABLEHOTLINKS, (WPARAM)fHotLinks, 0L );
      
      SetWindowFont( hwndEdit, hMsgFont, FALSE );

#else USEBIGEDIT
#ifdef GNU
      hwndEdit = CreateWindow( "Scintilla", "", dwAttr,
               0, 0, 0, 0, hwnd, (HMENU)IDD_MESSAGE, (HINSTANCE)hInst, NULL );
#else GNU
      hwndEdit = CreateWindow( WC_BIGEDIT, "", dwAttr,
               0, 0, 0, 0, hwnd, (HMENU)IDD_MESSAGE, (HINSTANCE)hInst, NULL );
#endif GNU

      if( lpWindInfo )
         SetEditorDefaults(hwndEdit, TRUE, lpWindInfo->fFixedFont ? lfFixedFont : lfMsgFont, fWordWrap, fHotLinks , fMsgStyles, crMsgText, crMsgWnd );
#endif USEBIGEDIT

      /* Subclass the thread and message panes.
       */
      lpProcEditCtl = SubclassWindow( GetDlgItem( hwnd, IDD_MESSAGE ), MessagePaneProc );
      lpProcListCtl = SubclassWindow( GetDlgItem( hwnd, IDD_LIST ), ThreadPaneProc );
      lpProcTreeCtl = SubclassWindow( GetDlgItem( hwnd, IDD_FOLDERLIST ), FolderListProc );

   
      /* Set the horizontal split
       */   
      aSplits[ 0 ] = 40;
      aSplits[ 1 ] = 40;
      Amuser_GetPPArray( szMsgViewerWnd, "SplitBarPosition", aSplits, 2 );
      lpWindInfo->nSplitPos = aSplits[ 0 ];
      lpWindInfo->nSplitCur = aSplits[ 1 ];

      /* Set the vertical split
       */   
      aSplits[ 0 ] = 25;
      aSplits[ 1 ] = 25;
      Amuser_GetPPArray( szMsgViewerWnd, "FolderSplitBarPosition", aSplits, 2 );
      lpWindInfo->nHSplitPos = aSplits[ 0 ];
      lpWindInfo->nHSplitCur = aSplits[ 1 ];

      /* Initialise the WINDINFO structure.
       */
      lpWindInfo->lpTopic = NULL;
      lpWindInfo->lpMessage = NULL;
      lpWindInfo->hwndFocus = GetDlgItem( hwnd, IDD_LIST );
      lpWindInfo->fFixedFont = FALSE;
      lpWindInfo->fLockUpdates = FALSE;
      lpWindInfo->fUpdatePending = FALSE;
      PutBlock( hwnd, lpWindInfo );

      /* Create the image list.
       */
      himl = Amdb_CreateFolderImageList();
      if( !himl )
         return( FALSE );
      TreeView_SetImageList( hwndFolderList, himl, 0 );
      TreeView_SetTextColor( hwndFolderList, crInBaskText );
      TreeView_SetBackColor( hwndFolderList, crInBaskWnd );

      /* Fill the treeview control.
       */
      Amdb_FillFolderTree( hwndFolderList, FTYPE_ALL|FTYPE_TOPICS, fShowTopicType );

      /* Set the message window font and size.
       */
      PostMessage( hwnd, WM_CHANGEMDIMENU, 0, 0L );
      fLastFocusPane = GetDlgItem( hwnd, IDD_LIST );
      PostMessage( hwnd, WM_TOGGLEHEADERS, 0, 0L );
      return( TRUE );
      }
   return( FALSE );
}

/* This function intercepts messages for the out-basket list box window
 * so that we can handle the right mouse button.
 */
LRESULT EXPORT FAR PASCAL FolderListProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      case WM_MOUSEWHEEL: 
      {
         HWND hwndFocused;
         POINT lp;

         /* Pass on mouse wheel messages to the
          * message pane.
          */
         GetCursorPos(&lp);
         hwndFocused = WindowFromPoint(lp);
         if (IsWindow(hwndFocused) && hwnd != hwndFocused)
            return( SendMessage( hwndFocused, message, wParam, lParam ) );
         break;
      }
      case WM_SETFOCUS:
         iActiveMode = WIN_INBASK;
         if ( SendMessage(GetDlgItem( hwnd, IDD_MESSAGE ), SCI_CALLTIPACTIVE, 0, 0) == 1 )
            SendMessage(GetDlgItem( hwnd, IDD_MESSAGE ), SCI_CALLTIPCANCEL, 0, 0);
         break;

      case WM_KEYDOWN:
         if( wParam == VK_F6 || wParam == VK_TAB )
         {
            HWND hwndParent;
            HWND hwndFocus;

            hwndParent = GetParent( hwnd );
            VERIFY( hwndFocus = GetDlgItem( hwndParent, IDD_LIST ) );
            SetFocus( hwndFocus );
            return( TRUE );
         }
         break;

      case WM_ERASEBKGND: {
         RECT rc;

         GetClientRect( hwnd, &rc );
         SetBkColor( (HDC)wParam, crInBaskWnd );
         ExtTextOut( (HDC)wParam, rc.left, rc.top, ETO_OPAQUE, &rc, "", 0, NULL );
         return( TRUE );
         }
      }
   return( CallWindowProc( lpProcTreeCtl, hwnd, message, wParam, lParam ) );
}

/* This function shows or hides the relevant header columns.
 */
void FASTCALL ShowHeaderColumns( HWND hwnd )
{
   LPWINDINFO lpWindInfo;
   HWND hwndHdr;

   lpWindInfo = GetBlock( hwnd );
   hwndHdr = GetDlgItem( hwnd, IDD_MSGIDTITLE );
   Header_ShowItem( hwndHdr, 0, lpWindInfo->vd.wHeaderFlags & VH_MSGNUM );
   Header_ShowItem( hwndHdr, 1, lpWindInfo->vd.wHeaderFlags & VH_SENDER );
   Header_ShowItem( hwndHdr, 2, lpWindInfo->vd.wHeaderFlags & VH_DATETIME );
   Header_ShowItem( hwndHdr, 3, lpWindInfo->vd.wHeaderFlags & VH_MSGSIZE );
   Header_ShowItem( hwndHdr, 4, lpWindInfo->vd.wHeaderFlags & VH_SUBJECT );
   Header_SetItemWidth( hwndHdr, 0, lpWindInfo->vd.nHdrDivisions[ 0 ] );
   Header_SetItemWidth( hwndHdr, 1, lpWindInfo->vd.nHdrDivisions[ 1 ] );
   Header_SetItemWidth( hwndHdr, 2, lpWindInfo->vd.nHdrDivisions[ 2 ] );
   Header_SetItemWidth( hwndHdr, 3, lpWindInfo->vd.nHdrDivisions[ 3 ] );
   Header_SetItemWidth( hwndHdr, 4, lpWindInfo->vd.nHdrDivisions[ 4 ] );
}

/* This function handles the WM_CLOSE message.
 */
BOOL FASTCALL MsgViewWnd_OnClose( HWND hwnd )
{
   PTL ptlTmp;
   LPWINDINFO lpWindInfo;
   HWND hwndTreeCtl;
   HIMAGELIST himl;

   /* Handle notification first.
    */
   lpWindInfo = GetBlock( hwnd );
   if( NULL != lpWindInfo->lpMessage )
      HandleNotification( hwnd, lpWindInfo->lpMessage );

   /* Switch the message window to the other message window
    * if one is available.
    */
   hwndMsg = hwndQuote = FindMsgViewerWindow( hwnd, NULL );
   if( hwndMsg == NULL )
      {
      RemoveAllBookmarks();
      if( hbrThreadWnd )
         {
         DeleteBrush( hbrThreadWnd );
         hbrThreadWnd = NULL;
         }
      if( hbrInBaskWnd )
         {
         DeleteBrush( hbrInBaskWnd );
         hbrInBaskWnd = NULL;
         }
      }

   /* Delete the folder pane treeview image list.
    */
   VERIFY( hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST ) );
   himl = TreeView_GetImageList( hwndTreeCtl, 0 );
   Amctl_ImageList_Destroy( himl );

   /* Delete the window.
    */
   ptlTmp = lpWindInfo->lpTopic;
   SendMessage( hwndMDIClient, WM_MDIDESTROY, (WPARAM)hwnd, 0L );
   
   
   /* If autocollapsing of folders is on, close the folder 
    */
   if( !fQuitting && fAutoCollapse && hwndInBasket )
   {
   HTREEITEM hOldTopicItem;
   HTREEITEM hOldFolderItem;

   VERIFY( hwndTreeCtl = GetDlgItem( hwndInBasket, IDD_FOLDERLIST ) );
   if( 0L != ( hOldTopicItem = TreeView_GetSelection( hwndTreeCtl ) ) )
      if( 0L != ( hOldFolderItem = TreeView_GetParent( hwndTreeCtl, hOldTopicItem ) ) )
         TreeView_Expand( hwndTreeCtl, hOldFolderItem, TVE_COLLAPSE );
   }

   /* Unlock the topic that was being displayed.
    */
   if( ptlTmp )
      Amdb_InternalUnlockTopic( ptlTmp );
   Amdb_SetTopicSel( ptlTmp, NULL );
   return( TRUE );
}

/* This function handles the WM_DESTROY message.
 */
void FASTCALL MsgViewWnd_OnDestroy( HWND hwnd )
{
   LPWINDINFO lpWindInfo;

   /* Free the bitmaps if we're the last message window to close.
    */
   ASSERT( cMsgWnds != 0 );
   if( --cMsgWnds == 0 )
      DeleteBitmap( hbmpThreadBitmaps );

   /* Free the message record.
    */
   lpWindInfo = GetBlock( hwnd );
   FreeMemory( &lpWindInfo );
}

/* This function handles the WM_SYSCOMMAND message.
 */
void FASTCALL MsgViewWnd_OnSysCommand( HWND hwnd, UINT cmd, int x, int y )
{
   if( cmd == IDM_SPLIT )
      {
      SendMessage( GetDlgItem( hwnd, IDD_HSPLIT ), SPM_ADJUST, 0, 0L );
      return;
      }
   FORWARD_WM_SYSCOMMAND( hwnd, cmd, x, y, DefMDIChildProc );
}

void FASTCALL MsgViewWnd_OnKillFocus( HWND hwnd, HWND hwndOldFocus )
{
}
/* This function handles the WM_SETFOCUS message. Whenever the message
 * viewer window gets the focus, we transfer it to the thread pane.
 */
void FASTCALL MsgViewWnd_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   /* Set the focus.
    */
   iActiveMode = WIN_THREAD;

   if ( SendMessage(GetDlgItem( hwnd, IDD_MESSAGE ), SCI_CALLTIPACTIVE, 0, 0) == 1 )
      SendMessage(GetDlgItem( hwnd, IDD_MESSAGE ), SCI_CALLTIPCANCEL, 0, 0);
   
   if (IsWindow(fLastFocusPane)) // !!SM!!
      SetFocus(fLastFocusPane); // !!SM!!
   else                   // !!SM!!
      SetFocus( GetDlgItem( hwnd, IDD_LIST ) ); 
}


/* This function handles notification messages from the
 * treeview control.
 */
LRESULT FASTCALL MsgViewWnd_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
{

   if( lpNmHdr->idFrom == IDD_FOLDERLIST )
      {
      /* Don't process right-button popup in the folder list in the
       * same way as in the in-basket. At least not yet!
       */
      if( lpNmHdr->code == TVN_SELCHANGED )
         {
         LPNM_TREEVIEW lpnmtv;

         lpnmtv = (LPNM_TREEVIEW)lpNmHdr;
         if( lpnmtv->action != TVC_UNKNOWN )
            {
            InBasket_OpenCommand( hwnd, FALSE );
            if( lpnmtv->action == TVC_BYKEYBOARD )
               SetFocus( lpNmHdr->hwndFrom );
            }
         }
      InBasket_OnNotify( hwnd, idCode, lpNmHdr );
      }
   else if( lpNmHdr->idFrom == IDD_ATTACHMENT )
      {
      switch( lpNmHdr->code )
         {
         case NM_RCLICK: {
            HMENU hPopupMenu;
            POINT pt;

            if( hwndActive != hwnd )
               SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwnd, 0L );
            hPopupMenu = GetPopupMenu( IPOP_ATTACHMENT );
            GetCursorPos( &pt );
            TrackPopupMenu( hPopupMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndFrame, NULL );
            break;
            }
   
         case NM_DBLCLK:
            SendCommand( hwnd, IDM_RUNATTACHMENT, 0L );
            break;
         }
      }
   else if( lpNmHdr->idFrom == IDD_MESSAGE )
      {
#ifdef USEBIGEDIT
      if( BEMN_CLICK == lpNmHdr->code )
         {
         BEMN_HOTSPOT FAR * lpbemnHotspot;

         /* User clicked over a hotspot in the edit window, so
          * fire up the browser.
          */
         lpbemnHotspot = (BEMN_HOTSPOT FAR *)lpNmHdr;
         return( CommonHotspotHandler( hwnd, lpbemnHotspot->szText ) );
         }

#else USEBIGEDIT
         return Lexer_OnNotify( hwnd, idCode, lpNmHdr );
#endif USEBIGEDIT

      }
   else if( lpNmHdr->idFrom == IDD_MSGIDTITLE )
      {
      switch( lpNmHdr->code )
         {
         case NM_RCLICK: {
            HMENU hPopupMenu;
            POINT pt;

            if( hwndActive != hwnd )
               SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwnd, 0L );
            hPopupMenu = GetPopupMenu( IPOP_MSGIDTITLE );
            GetCursorPos( &pt );
            TrackPopupMenu( hPopupMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndFrame, NULL );
            break;
            }

         case HDN_ITEMCLICK: 
            {
               if( ( GetKeyState( VK_SHIFT ) & 0x8000 ) == 0x8000 )
               {
                  LPWINDINFO lpWindInfo;
                  HD_NOTIFY FAR * lphdn;

                  /* User clicked the column header buttons, so resort the
                   * column in chronological mode.
                   */
                  lphdn = (HD_NOTIFY FAR *)lpNmHdr;
                  lpWindInfo = GetBlock( hwnd );
                  SetSortMode( &lpWindInfo->vd, lphdn->iItem + VSM_MSGNUM );
                  lpWindInfo->vd.nViewMode = VM_VIEWCHRON;
                  Amdb_SetTopicViewData( lpWindInfo->lpTopic, &lpWindInfo->vd );
                  SendMessage( hwnd, WM_VIEWCHANGE, lpWindInfo->vd.nViewMode, 0L );
               }
               return( TRUE );
            }


         case HDN_BEGINTRACK:
            return( TRUE );
   
         case HDN_TRACK:
         case HDN_ENDTRACK: {
            LPWINDINFO lpWindInfo;
            HD_NOTIFY FAR * lpnf;
            HWND hwndList;
            int diff;
            RECT rc;

            /* Save the old offset of the column being
             * dragged.
             */
            lpnf = (HD_NOTIFY FAR *)lpNmHdr;
            lpWindInfo = GetBlock( hwnd );
            diff = lpnf->pitem->cxy - lpWindInfo->vd.nHdrDivisions[ lpnf->iItem ];
            lpWindInfo->vd.nHdrDivisions[ lpnf->iItem ] = lpnf->pitem->cxy;
            
            /* If we're done, save new position
             */
            if( HDN_ENDTRACK == lpNmHdr->code )
               {
               BOOL fGlobal;

               /* If the Ctrl key is down, apply changes to the current topic
                * otherwise apply changes globally.
                */
               fGlobal = ( GetKeyState( VK_CONTROL ) & 0x8000 ) == 0x8000;
               if( !fGlobal )
                  Amdb_SetTopicViewData( lpWindInfo->lpTopic, &lpWindInfo->vd );
               else
                  {
                  PCAT pcat;
                  PCL pcl;
                  PTL ptl;

                  for( pcat = Amdb_GetFirstCategory(); pcat; pcat = Amdb_GetNextCategory( pcat ) )
                     for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
                        for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
                           {
                           TOPICINFO topicinfo;

                           Amdb_GetTopicInfo( ptl, &topicinfo );
                           topicinfo.vd.nHdrDivisions[ 0 ] = lpWindInfo->vd.nHdrDivisions[ 0 ];
                           topicinfo.vd.nHdrDivisions[ 1 ] = lpWindInfo->vd.nHdrDivisions[ 1 ];
                           topicinfo.vd.nHdrDivisions[ 2 ] = lpWindInfo->vd.nHdrDivisions[ 2 ];
                           topicinfo.vd.nHdrDivisions[ 3 ] = lpWindInfo->vd.nHdrDivisions[ 3 ];
                           topicinfo.vd.nHdrDivisions[ 4 ] = lpWindInfo->vd.nHdrDivisions[ 4 ];
                           Amdb_SetTopicViewData( ptl, &topicinfo.vd );
                           }
                  }
               }

            /* Invalidate from the column being dragged.
             */
            VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
            GetClientRect( hwndList, &rc );
            rc.left = lpnf->cxyUpdate + 1;
            if( diff > 0 )
               rc.left -= diff;

            /* Now use ScrollWindow.
             */
            fQuickUpdate = TRUE;
            ScrollWindow( hwndList, diff, 0, &rc, &rc );
            if( 0 == lpnf->iItem )
               {
               rc.right = rc.left;
               rc.left = 0;
               InvalidateRect( hwndList, &rc, FALSE );
               }
            UpdateWindow( hwndList );
            fQuickUpdate = FALSE;
            return( TRUE );
            }
         }
      }
   return( FALSE );
}

/* This function handles the BEMN_CLICK notification in an
 * edit window.
 */
LRESULT FASTCALL CommonHotspotHandler( HWND hwnd, char * pszHotspot ) // !!SM!! 2032 BOOL->LRESULT
{
   int uRetCode;

   uRetCode = AMCHH_NOCHANGE;

   /* User clicked over a hotspot in the edit window, so
    * fire up the browser.
    */
   if( _strnicmp( pszHotspot, "news:", 5 )  == 0 || _strnicmp( pszHotspot, "nntp:", 5 )  == 0 )
   {
      char szServerName[ 64 ];
      PTL ptl;

      /* Strip off news: prefix.
       */
      pszHotspot += 5;
      while( isspace( *pszHotspot ) )
         ++pszHotspot;

      /* Check for news server designation.
       */
      strcpy( szServerName, szNewsServer );
      if( *pszHotspot == '/' && *(pszHotspot+1) == '/' )
      {
         int c;

         /* Gather the news server name.
          */
         pszHotspot += 2;
         for( c = 0; *pszHotspot && *pszHotspot != '/'; ++c )
            {
            if( c < 63 )
               szServerName[ c ] = *pszHotspot;
            ++pszHotspot;
            }
         szServerName[ c ] = '\0';
         if( *pszHotspot == '/' )
            ++pszHotspot;
         CmdCreateNewsServer( szServerName );
      }

      /* Open a message window on the specified newsgroup.
       * Subscribe to it if it isn't found.
       */
      InternalSetMsgBookmark( FALSE, FALSE );
      ptl = Amdb_OpenTopic( pclNewsFolder, pszHotspot );
      if( NULL == ptl )
      {
         SUBSCRIBE_INFO si;

         si.pszNewsgroup = pszHotspot;
         si.pszNewsServer = szServerName;
         CmdSubscribe( hwnd, &si );
         ptl = Amdb_OpenTopic( pclNewsFolder, pszHotspot );
      }

      if( NULL != ptl )
      {
         OpenTopicViewerWindow( ptl );
         uRetCode = AMCHH_MOVEDMESSAGE; // !!SM!! 2032
      }
      else
         uRetCode = AMCHH_NOCHANGE; // !!SM!! 2032

      /* If we're online, get some more articles
       */
      if( fOnline && NULL != ptl )
      {
         NEWARTICLESOBJECT nh;
         char sz[ 100 ];

         Amob_New( OBTYPE_GETNEWARTICLES, &nh );
         WriteFolderPathname( sz, (LPVOID)ptl );
         Amob_CreateRefString( &nh.recFolder, sz );
         Amob_Commit( NULL, OBTYPE_GETNEWARTICLES, &nh );
         Amob_Delete( OBTYPE_GETNEWARTICLES, &nh );
      }
   }
   else if( _strnicmp( pszHotspot, "mailto:", 7 ) == 0 )
      {
      MAILOBJECT mo;
      char * pSubject;
      char sz[ 80 ];
      int count;
      CURMSG curmsg;

      /* Strip off mailto: prefix.
       */
      pszHotspot += 7;
      while( isspace( *pszHotspot ) )
         ++pszHotspot;

      /* Open a mail editor window.
       */

      Amob_New( OBTYPE_MAILMESSAGE, &mo );
      if( NULL != ( pSubject = strrchr( pszHotspot, '?' ) ) )
         {
            if( _strnicmp( pSubject + 1 , "Subject=", 8 ) == 0 )
            {
               Amob_CreateRefString( &mo.recSubject, pSubject + 9 );
               for( count = 0; *pszHotspot && *pszHotspot != '?' && count < 80; count++ )
               {
                  sz[ count ] = *pszHotspot;
                  pszHotspot++;
               }
               sz[ count ] = '\0';
               Amob_CreateRefString( &mo.recTo, sz );
            }
            else
               Amob_CreateRefString( &mo.recTo, pszHotspot );
         }
      else
         Amob_CreateRefString( &mo.recTo, pszHotspot );


      Ameol2_GetCurrentMsg( &curmsg );
      if( NULL != curmsg.ptl )
      {
         char szMailbox[ LEN_TOPICNAME + LEN_FOLDERNAME + LEN_CATNAME + 3 ];
         WriteFolderPathname( szMailbox, curmsg.ptl);
         Amob_CreateRefString( &mo.recMailbox, szMailbox );
         CreateReplyAddress( &mo.recReplyTo, curmsg.ptl );
      }
      else if( fUseInternet )
         Amob_CreateRefString( &mo.recReplyTo, szMailReplyAddress );
      SetFocus(CreateMailWindow( hwndFrame, &mo, NULL ));
      Amob_Delete( OBTYPE_MAILMESSAGE, &mo );
      uRetCode = AMCHH_OPENEDMDI; // !!SM!! 2032
   }

   else if( _strnicmp( pszHotspot, "cixfile:", 8 ) == 0 )
   {
      char szDirectory[ _MAX_PATH ];
      char szConfName[ LEN_CONFNAME+1 ];
      char szTopicName[ LEN_TOPICNAME+1 ];
      register int c;
      FDLOBJECT fd;
      char * pText;
      PCL pcl;
      PTL ptl;
      CURMSG curmsg;

      /* Parse off conference, topic and filename.
       */
      pText = pszHotspot + 8;
      StripLeadingTrailingChars( pText, '.' ); // !!SM!! 2.55
      StripLeadingTrailingChars( pText, ',' ); // !!SM!! 2.55
      if( strchr( pText, '/' ) != NULL )
      {
         for( c = 0; *pText && *pText != '/'; ++c )
         {
            if( c < LEN_CONFNAME+1 ) //FS#154 2054
               szConfName[ c ] = *pText;
            else                          // !!SM!! 2.55 2031
               szConfName[ LEN_CONFNAME ] = '\0'; 
            ++pText;
         }
         if( c < LEN_CONFNAME+1 )                     // !!SM!! 2.55 2031 //FS#154 2054
            szConfName[ c ] = '\0';                 
         else
            szConfName[ LEN_CONFNAME ] = '\0';

         if( szConfName [ 0 ]  == '\0' )
         {
            Ameol2_GetCurrentMsg( &curmsg );
            if( NULL != curmsg.pcl )
               strcpy( szConfName, Amdb_GetFolderName( curmsg.pcl ) );
         }
         if( *pText == '/' )
            ++pText;
         for( c = 0; *pText && *pText != ':'; ++c )
         {
            if( c < LEN_TOPICNAME )
               szTopicName[ c ] = *pText;
            else                                     // !!SM!! 2.55 2031
               szTopicName[ LEN_TOPICNAME ] = '\0';
            ++pText;
         }
         if( c < LEN_TOPICNAME )                 // !!SM!! 2.55 2031
            szTopicName[ c ] = '\0';
         else
            szTopicName[ LEN_TOPICNAME ] = '\0';
         
         if( *pText == ':' )
            ++pText;

         /* Try and get an associated directory for this
          * filename.
          */
         if( NULL != ( pcl = Amdb_OpenFolder( NULL, szConfName ) ) )
            ptl = Amdb_OpenTopic( pcl, szTopicName );
         else
            ptl = NULL;
      }
      else
      {
         CURMSG curmsg;

         /* Only filename is provided.
          */
         Ameol2_GetCurrentMsg( &curmsg );
         if( NULL != curmsg.pcl )
            strcpy( szConfName, Amdb_GetFolderName( curmsg.pcl ) );
         if( NULL != curmsg.ptl )
            strcpy( szTopicName, Amdb_GetTopicName( curmsg.ptl ) );
         ptl = curmsg.ptl;
      }

      /* Parse off filename.
       */
      for( c = 0; *pText; ++c )
      {
         if( c < _MAX_PATH )
            lpPathBuf[ c ] = *pText;
         ++pText;
      }
      lpPathBuf[ c ] = '\0';
      if( NULL != ptl )
         pcl = Amdb_FolderFromTopic( ptl );

      /* Get the directory associated with this
       * filename.
       */
      GetAssociatedDir( ptl, szDirectory, lpPathBuf );

      /* Create a new FDLOBJECT type object.
       */
      if( fUseCIX )
      {
         Amob_New( OBTYPE_DOWNLOAD, &fd );
         Amob_CreateRefString( &fd.recCIXFileName, lpPathBuf );
         Amob_CreateRefString( &fd.recDirectory, szDirectory );
         Amob_CreateRefString( &fd.recConfName, szConfName );
         Amob_CreateRefString( &fd.recTopicName, szTopicName );
         if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_FDL), Download, (LPARAM)(LPSTR)&fd ) )
         {
            if( !Amob_Find( OBTYPE_DOWNLOAD, &fd ) )
            {
               Amob_Commit( NULL, OBTYPE_DOWNLOAD, &fd );
            }
            Amob_Delete( OBTYPE_DOWNLOAD, &fd );
         }
         else
         {
            Amob_Delete( OBTYPE_DOWNLOAD, &fd );
            return FALSE;
         }
         uRetCode = AMCHH_NOCHANGE; // !!SM!! 2032
      }
      else
      {
         fMessageBox( hwnd, 0, "CIX service not installed.\n\nCould not download file.", MB_OK|MB_ICONEXCLAMATION );
         return FALSE;
      }
   }
   else if( _strnicmp( pszHotspot, "am2file:", 8 ) == 0 )  // !!SM!! 2040
   {
      char * pText;
      char pNum[10];
      HPSTR lpText;
      CURMSG curmsg;
      int i;

      /* Parse off conference, topic and filename.
       */
      pText = pszHotspot + 8;
      StripLeadingTrailingChars( pText, '.' ); 
      StripLeadingTrailingChars( pText, ',' ); 

      i = 0;
      while (i < 9 && pText[i] !='-')
      {
         pNum[i] = pText[i];
         i++;
      }
      pNum[i] = '\x0';

      pText = pText + i + 1;

      Ameol2_GetCurrentMsg( &curmsg );
      if( NULL != curmsg.pth  )
      {
         if( lpText = Amdb_GetMsgText( curmsg.pth ) )
         {
            if(  GetKeyState( VK_CONTROL ) & 0x8000 )
               OpenAttachment(lpText, pText, pNum, curmsg.pth);
            else
               SaveAttachment(lpText, pText, pNum, curmsg.pth);
         }
      }
      return TRUE;
   }

   else if( _strnicmp( pszHotspot, "cix:", 4 ) == 0 )
   {
      DWORD dwMsgNumber;
      char * pText;
      PTL ptl;
      PCL pcl;
      char lConf [ LEN_CONFNAME + 1];
      char lTopic[ LEN_TOPICNAME + 1];
      char lsz[ LEN_CONFNAME + LEN_TOPICNAME + 10];
      int i,j;                                                 // !!SM!! 2.55.2033

      /* Skip any '//' after the cix:
       */
      pText = pszHotspot + 4;
      StripLeadingTrailingChars( pText, '.' ); // !!SM!! 2.55
      StripLeadingTrailingChars( pText, ',' ); // !!SM!! 2.55
      StripLeadingTrailingChars( pText, ' ' ); // !!SM!! 2.56.2058
      StripTrailingChars( pText, '/' ); // !!SM!! 2.55.2032
      if( *pText == '/' && *(pText + 1) == '/' )
         pText += 2;
      strcpy( lpTmpBuf, pText );
      //strlwr( lpTmpBuf );
      lConf[0] = '\x0';
      lTopic[0] = '\x0';
      ParseFolderTopicMessage( lpTmpBuf, &ptl, &dwMsgNumber );
      if( NULL == ptl )
      {
         LPWINDINFO lpWindInfo;
         char * pMsgNumber;
         BOOL fAllNumbers = TRUE;
         int iLen;
         int count;

         /* Create a Join command.
          */
         strcpy( lpTmpBuf, pText );
         if( NULL != ( pMsgNumber = strrchr( lpTmpBuf, ':' ) ) )
            *pMsgNumber = '\0';
         else if( NULL != ( pMsgNumber = strrchr( lpTmpBuf, '#' ) ) )
            *pMsgNumber = '\0';
         else if( hwndTopic )
         {
            /* Check for cix:<number> syntax. If no '/' found AND
             * the entire lpTmpBuf is a number, then treat it as
             * a number.
             */
            lpWindInfo = GetBlock( hwndTopic );
            ptl = lpWindInfo->lpTopic;
            pMsgNumber = lpTmpBuf;
            while( isdigit( *pMsgNumber ) )
               ++pMsgNumber;
            if( *pMsgNumber == '\0' )
               {
               pMsgNumber = lpTmpBuf;
               dwMsgNumber = atoi( pMsgNumber );
               }
            else if( *pMsgNumber != '/' )
               while( *pMsgNumber )
                  ++pMsgNumber;
         }
         iLen = ( strlen( lpTmpBuf ) ) - 1;
         for( count = 0; count < iLen;count++ )
         {
            if( !isdigit( *( lpTmpBuf+count ) ) )
               fAllNumbers = FALSE;
         }
         if( lpTmpBuf <= pMsgNumber && !fAllNumbers )
         {
            if( *lpTmpBuf == '/' && hwndTopic )
            {
               char sz[ 120 ];
               PCL pcl;
               /* If no folder name specified, use the current
                * folder name.
                */
               lpWindInfo = GetBlock( hwndTopic );
               pcl = Amdb_FolderFromTopic( lpWindInfo->lpTopic );
               wsprintf( sz, "%s/%s", Amdb_GetFolderName( pcl ), lpTmpBuf );
               strcpy( lpTmpBuf, sz );
            }
            else if( *lpTmpBuf == '/' && IsWindow(GetDlgItem(hwnd, IDD_CONFNAME)) )         // !!SM!! 2.55.2032
            {                                                           // !!SM!! 2.55.2032
               Edit_GetText(GetDlgItem(hwnd, IDD_CONFNAME), (LPSTR)&lConf, LEN_CONFNAME); // !!SM!! 2.55.2032
               i = 0;                                                      // !!SM!! 2.55.2032
               j = 0;                                                      // !!SM!! 2.55.2032
               while(pText[i] == '/') i++;                                       // !!SM!! 2.55.2032
               while(pText[i] != ':' && pText[i] != '#' && pText[i] != '\x0')          // !!SM!! 2.55.2032
                  lTopic[j++] = pText[i++];                                   // !!SM!! 2.55.2032
               lTopic[j++] = '\x0';                                        // !!SM!! 2.55.2032
            }                                                           // !!SM!! 2.55.2032

            if ( !fUseCIX )
               fMessageBox( hwnd, 0, "CIX service not installed.\n\nCould not join forum.", MB_OK|MB_ICONEXCLAMATION );
            else if(lConf[0] != '\x0' )
            {
               if( NULL != ( pcl = Amdb_OpenFolder( NULL, lConf ) ) )
                  ptl = Amdb_OpenTopic( pcl, lTopic );
            }
            else
               ptl = CmdJoinConference( hwnd, lpTmpBuf );
         }
         else if( hwndTopic )
         {
            lpWindInfo = GetBlock( hwndTopic );
            ptl = lpWindInfo->lpTopic;
         }
         else if(IsWindow(GetDlgItem(hwnd, IDD_CONFNAME)) && IsWindow(GetDlgItem(hwnd, IDD_TOPICNAME))) // !!SM!! 2.55.2032
         {
            Edit_GetText(GetDlgItem(hwnd, IDD_CONFNAME), (LPSTR)&lConf, LEN_CONFNAME);
            Edit_GetText(GetDlgItem(hwnd, IDD_TOPICNAME), (LPSTR)&lTopic, LEN_TOPICNAME);

            pMsgNumber = lpTmpBuf;
            while( isdigit( *pMsgNumber ) )
               ++pMsgNumber;
            if( *pMsgNumber == '\0' )
               {
               pMsgNumber = lpTmpBuf;
               dwMsgNumber = atoi( pMsgNumber );
               }
            else if( *pMsgNumber != '/' )
               while( *pMsgNumber )
                  ++pMsgNumber;

            if (pText && pText[0] == '/')
               wsprintf( lsz, "%s%s", lConf, pText);
            else if(fAllNumbers && pText)                                                 // 20060228 - 2.56.2049.04
               wsprintf( lsz, "%s/%s:%s", lConf, lTopic, pText);
            else if(pText)
               wsprintf( lsz, "%s/%s", lConf, pText);
            else
               wsprintf( lsz, "%s/%s", lConf, lTopic);
            if( NULL != ( pcl = Amdb_OpenFolder( NULL, lConf ) ) )
            {
               if (pText && pText[0] == '/')
                  ptl = Amdb_OpenTopic( pcl, pText+1 );
               else if(pText && !fAllNumbers)                                             // 20060228 - 2.56.2049.04
                  ptl = Amdb_OpenTopic( pcl, pText );
               else
                  ptl = Amdb_OpenTopic( pcl, lTopic );
            }
            else
               ptl = CmdJoinConference( hwnd, lsz );
         }                                                              // !!SM!! 2.55.2032
         else                                                                                            
         {                                                              // !!SM!! 2.55.2033
            i = 0;                                                      
            j = 0;                                                      
            while(pText[i] != '/' && pText[i] != '\x0')
            {
               lConf[j] = pText[i];
               i++;
               j++;
            }
            lConf[j] = '\x0';
            j = 0; 
            if (pText[i] != '/') i++;
            while(pText[i] != ':' && pText[i] != '#' && pText[i] != '\x0')          
               lTopic[j++] = pText[i++];
            lTopic[j++] = '\x0';

            wsprintf( lsz, "%s/%s", lConf, lTopic);
            if( NULL != ( pcl = Amdb_OpenFolder( NULL, lConf ) ) )
               ptl = Amdb_OpenTopic( pcl, lTopic );
            else
               ptl = CmdJoinConference( hwnd, lsz );
         }                                                              // !!SM!! 2.55.2033
      }  


      /* Okay, we've got a topic handle and possibly
       * a message number.
       */
      if( NULL != ptl )
      {
         Amdb_LockTopic( ptl );
         if( 0L != dwMsgNumber && NULL == Amdb_OpenMsg( ptl, dwMsgNumber ) )
            if( fUseCIX )
               DownloadMsg( hwnd, ptl, dwMsgNumber );
            else
               fMessageBox( hwnd, 0, "CIX service not installed.\n\nCould not retrieve message.", MB_OK|MB_ICONEXCLAMATION );
         else
            {
            BOOL fNew;

            fNew = ( GetKeyState( VK_CONTROL ) & 0x8000 ) == 0x8000;
            InternalSetMsgBookmark( FALSE, FALSE );
            OpenOrJoinTopic( ptl, dwMsgNumber, fNew );
            }
         Amdb_UnlockTopic( ptl );
         uRetCode = AMCHH_MOVEDMESSAGE; // !!SM!! 2032
      }
      else
         uRetCode = AMCHH_NOCHANGE; // !!SM!! 2032
   }
   else if( _strnicmp( pszHotspot, "**COPIED FROM: >>>", 18 ) == 0 )
   {
      DWORD dwMsgNumber;
      char * pText;
      PTL ptl;
      UINT k;

      /* Skip any '//' after the cix:
       */
      pText = pszHotspot + 18;
      if( *pText == '/' && *(pText + 1) == '/' )
         pText += 2;

      /* Strip any invalid cix: prefixes
       */
      if( strlen( pText ) > 4 && pText[3] == ':') // 20060313 - 2.56.2049.14
         pText += 4;

      for( k = 0; k < strlen( pText ); k++ )
      {
         if( pText[ k ] == ' ' )
            pText[ k ] = ':'; 
      }

      strcpy( lpTmpBuf, pText );

      //strlwr( lpTmpBuf );
      ParseFolderTopicMessage( lpTmpBuf, &ptl, &dwMsgNumber );
      if( NULL == ptl )
         {
         LPWINDINFO lpWindInfo;
         char * pMsgNumber;
         BOOL fAllNumbers = TRUE;
         int iLen;
         int count;

         /* Create a Join command.
          */
         strcpy( lpTmpBuf, pText );
         if( NULL != ( pMsgNumber = strrchr( lpTmpBuf, ':' ) ) )
            *pMsgNumber = '\0';
         else if( NULL != ( pMsgNumber = strrchr( lpTmpBuf, '#' ) ) )
            *pMsgNumber = '\0';
         else if( hwndTopic )
            {
            /* Check for cix:<number> syntax. If no '/' found AND
             * the entire lpTmpBuf is a number, then treat it as
             * a number.
             */
            lpWindInfo = GetBlock( hwndTopic );
            ptl = lpWindInfo->lpTopic;
            pMsgNumber = lpTmpBuf;
            while( isdigit( *pMsgNumber ) )
               ++pMsgNumber;
            if( *pMsgNumber == '\0' )
               {
               pMsgNumber = lpTmpBuf;
               dwMsgNumber = atoi( pMsgNumber );
               }
            else if( *pMsgNumber != '/' )
               while( *pMsgNumber )
                  ++pMsgNumber;
            }
            iLen = ( strlen( lpTmpBuf ) ) - 1;
            for( count = 0; count < iLen;count++ )
            {
               if( !isdigit( *( lpTmpBuf+count ) ) )
                  fAllNumbers = FALSE;
            }
            if( lpTmpBuf <= pMsgNumber && !fAllNumbers )
            {
            if( *lpTmpBuf == '/' && hwndTopic )
               {
               char sz[ 120 ];
               PCL pcl;

               /* If no folder name specified, use the current
                * folder name.
                */
               lpWindInfo = GetBlock( hwndTopic );
               pcl = Amdb_FolderFromTopic( lpWindInfo->lpTopic );
               wsprintf( sz, "%s/%s", Amdb_GetFolderName( pcl ), lpTmpBuf );
               strcpy( lpTmpBuf, sz );
               }
               if ( !fUseCIX )
                  fMessageBox( hwnd, 0, "CIX service not installed.\n\nCould not join forum.", MB_OK|MB_ICONEXCLAMATION );
               else
                  ptl = CmdJoinConference( hwnd, lpTmpBuf );
            }
         else if( hwndTopic )
            {
            lpWindInfo = GetBlock( hwndTopic );
            ptl = lpWindInfo->lpTopic;
            }
         }

      /* Okay, we've got a topic handle and possibly
       * a message number.
       */
      if( NULL != ptl )
      {
         Amdb_LockTopic( ptl );
         if( 0L != dwMsgNumber && NULL == Amdb_OpenMsg( ptl, dwMsgNumber ) )
            if( fUseCIX )
               DownloadMsg( hwnd, ptl, dwMsgNumber );
            else
               fMessageBox( hwnd, 0, "CIX service not installed.\n\nCould not retrieve message.", MB_OK|MB_ICONEXCLAMATION );
         else
            {
            BOOL fNew;

            fNew = ( GetKeyState( VK_CONTROL ) & 0x8000 ) == 0x8000;
            InternalSetMsgBookmark( FALSE, FALSE );
            OpenOrJoinTopic( ptl, dwMsgNumber, fNew );
            }
         Amdb_UnlockTopic( ptl );
         uRetCode = AMCHH_MOVEDMESSAGE; // !!SM!! 2032
      }
      else
         uRetCode = AMCHH_NOCHANGE; // !!SM!! 2032

   }
   else            
   {
      /* Under Windows 95, open the link as a document first. This will use any
       * active browser rather than firing up a new one.
       */
      if( wWinVer >= 0x35F && fBrowserIsMSIE )
      {
         StripPercent20(pszHotspot);
         if( ( uRetCode = (UINT)ShellExecute( hwndFrame, NULL, pszHotspot, "", pszAmeolDir, SW_SHOW ) ) < 32 )
         {
            DisplayShellExecuteError( hwnd, pszHotspot, uRetCode );
            uRetCode = AMCHH_NOCHANGE;       // !!SM!! 2032
//          return( FALSE );              // !!SM!! 2032
         }
         else
            uRetCode = AMCHH_LAUNCHEXTERNAL; // !!SM!! 2032
      }
      else
      {
         LPSTR lpFullCmdLine;
         char * pCmdLine;
         int cbCmdLine;

         INITIALISE_PTR(lpFullCmdLine);

         /* szBrowser might contain a command line. So we have to
          * extract that and pass it separately with the URL
          * text.
          */
         pCmdLine = ExtractFilename( lpPathBuf, szBrowser );
         cbCmdLine = strlen( pCmdLine ) + strlen( pszHotspot ) + 2;
         if( fNewMemory( &lpFullCmdLine, cbCmdLine ) )
         {
            strcpy( lpFullCmdLine, pCmdLine );
            strcat( lpFullCmdLine, " " );
            strcat( lpFullCmdLine, pszHotspot );
            if( ( uRetCode = (UINT)ShellExecute( hwndFrame, NULL, lpPathBuf, lpFullCmdLine, pszAmeolDir, SW_SHOW ) ) < 32 )
            {
               DisplayShellExecuteError( hwnd, szBrowser, uRetCode );
               uRetCode = AMCHH_NOCHANGE;       // !!SM!! 2032
            }
            else
               uRetCode = AMCHH_LAUNCHEXTERNAL; // !!SM!! 2032
            FreeMemory( &lpFullCmdLine );
         }
      }
   }
   return( uRetCode );                                 // !!SM!! 2032
}


/* This function prompts for a replacement name for the file
 * being decoded.
 */
BOOL FASTCALL RenameAttachment( HWND hwnd )
{
   OPENFILENAME ofn;
   static char strFilters[] = "All Files (*.*)\0*.*\0\0";
   static char FAR szOutBuf[ _MAX_PATH ];    /* Full path to filename */
   char lszAttachDir[_MAX_PATH];

    strcpy(szOutBuf, szRenamedFilename);
   ofn.lStructSize = sizeof( OPENFILENAME );
   ofn.hwndOwner = hwnd;
   ofn.hInstance = NULL;
   ofn.lpstrFilter = strFilters;
   ofn.lpstrCustomFilter = NULL;
   ofn.nMaxCustFilter = 0;
   ofn.nFilterIndex = 1;
   ofn.lpstrFile = szOutBuf;
   ofn.nMaxFile = sizeof( szOutBuf );
   ofn.lpstrFileTitle = NULL;
   ofn.nMaxFileTitle = 0;
   ofn.lpstrInitialDir = lszAttachDir;
   ofn.lpstrTitle = "Rename Decoded Attachment";
   ofn.Flags = OFN_NOCHANGEDIR|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST|OFN_OVERWRITEPROMPT;
   ofn.nFileOffset = 0;
   ofn.nFileExtension = 0;
   ofn.lpstrDefExt = NULL;
   ofn.lCustData = 0;
   ofn.lpfnHook = NULL;
   ofn.lpTemplateName = 0;
   if( GetSaveFileName( &ofn ) )
      {
      strcpy( szRenamedFilename, szOutBuf );
      return( TRUE );
      }
   return( FALSE );
}

BOOL FASTCALL Am_OpenWith(HWND hwnd, LPSTR pFileName)
{
   void (WINAPI * SHOpenWithProc)( HWND, HINSTANCE, LPSTR, int );
   HINSTANCE hLib;

   /* Load SHELL32.DLL
    */
   if( NULL == ( hLib = LoadLibrary( "SHELL32.DLL" ) ) )
      return FALSE;

   (FARPROC)SHOpenWithProc = GetProcAddress( hLib, "OpenAs_RunDLL" );

   /* If any of the functions are NULL, can't use the new IF
    */
   if( NULL == SHOpenWithProc )
   {
      (FARPROC)SHOpenWithProc = GetProcAddress( hLib, "OpenAs_RunDLLA" );
      if( NULL == SHOpenWithProc )
         return FALSE;
   }

   /* Save current path in a global for BrowseCallback
    */

   SHOpenWithProc( hwnd, hInst, pFileName, SW_SHOWNORMAL );

   FreeLibrary( hLib );

   return TRUE;
// return FALSE;

}


void FASTCALL ShowLastError(void)
{
   LPVOID lpMsgBuf;
   FormatMessage( 
      FORMAT_MESSAGE_ALLOCATE_BUFFER | 
      FORMAT_MESSAGE_FROM_SYSTEM | 
      FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      GetLastError(),
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
      (LPTSTR) &lpMsgBuf,
      0,
      NULL 
   );
   // Process any inserts in lpMsgBuf.
   // ...
   // Display the string.
   MessageBox( NULL, (LPCTSTR)lpMsgBuf, "Error", MB_OK | MB_ICONINFORMATION );
   // Free the buffer.
   LocalFree( lpMsgBuf );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL MsgViewWnd_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   LPWINDINFO lpWindInfo;
   PTH pth;

   lpWindInfo = GetBlock( hwnd );
   if( id >= IDM_BACKMENU && id < IDM_BACKMENULAST )
      {
      PTH pth;

      /* Jump to the message specified by the selection
       * in the popup menu.
       */
      id = ( nBookPtr - ( id - IDM_BACKMENU ) ) - 1;
      Amdb_LockTopic( cmBookMark[ id ].ptl );
      if( pth = Amdb_OpenMsg( cmBookMark[ id ].ptl, cmBookMark[ id ].dwMsg ) )
         {
         InternalSetMsgBookmark( FALSE, TRUE );
         InternalSetCurrentMsg( cmBookMark[ id ].ptl, pth, TRUE, TRUE );
         }
      Amdb_UnlockTopic( cmBookMark[ id ].ptl );
      return;
      }
   switch( id )
      {
      case IDD_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsMESSAGEWINDOW );
         break;

      case IDCANCEL:
         SendMessage( hwnd, WM_CLOSE, 0, 0L );
         break;

      case IDM_QUICKPRINT:
         PrintMessage( hwnd, TRUE );
         ForceFocusToList( hwnd );
         break;

      case IDM_PRINT:
         PrintMessage( hwnd, FALSE );
         ForceFocusToList( hwnd );
         break;

      case IDM_OPEN:
         InBasket_OpenCommand( hwnd, FALSE );
         break;

      case IDM_PROPERTIES:
         /* Display the properties dialog for
          * this topic.
          */
         InBasket_Properties( hwnd );
         break;

      case IDM_SORT:
         InBasket_SortCommand( hwnd );
         break;


      case IDM_PURGE:
         InBasket_PurgeCommand( hwnd );
         break;

      case IDM_CHECK:
         InBasket_CheckCommand( hwnd );
         break;

      case IDM_ASCENDING: {
         BOOL fGlobal;
         lpWindInfo->vd.nSortMode ^= VSM_ASCENDING;
         fGlobal = ( GetKeyState( VK_CONTROL ) & 0x8000 ) == 0x8000;
         if( !fGlobal )
            Amdb_SetTopicViewData( lpWindInfo->lpTopic, &lpWindInfo->vd );
         else
            {
            PCAT pcat;
            PCL pcl;
            PTL ptl;

            for( pcat = Amdb_GetFirstCategory(); pcat; pcat = Amdb_GetNextCategory( pcat ) )
               for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
                  for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
                        Amdb_SetTopicViewData( ptl, &lpWindInfo->vd );
            }
         SendMessage( hwnd, WM_VIEWCHANGE, lpWindInfo->vd.nViewMode, 0L );
         break;
         }

      case IDM_SHOW_MSGNUM:      ShowColumn( hwnd, VH_MSGNUM );   break;
      case IDM_SHOW_SENDER:      ShowColumn( hwnd, VH_SENDER );   break;
      case IDM_SHOW_DATETIME:    ShowColumn( hwnd, VH_DATETIME ); break;
      case IDM_SHOW_MSGSIZE:     ShowColumn( hwnd, VH_MSGSIZE );  break;
      case IDM_SHOW_SUBJECT:     ShowColumn( hwnd, VH_SUBJECT );  break;

      case IDM_SORT_REFERENCE:
      case IDM_SORT_ROOTS:
      case IDM_SORT_MSGNUM:
      case IDM_SORT_SENDER:
      case IDM_SORT_DATETIME:
      case IDM_SORT_MSGSIZE:
      case IDM_SORT_SUBJECT: {
         int nNewSortMode;
         BOOL fGlobal;

         /* If the Ctrl key is down, apply changes to the current topic
          * otherwise apply changes globally.
          */
         fGlobal = ( GetKeyState( VK_CONTROL ) & 0x8000 ) == 0x8000;
         nNewSortMode = VSM_REFERENCE + ( id - IDM_SORT_REFERENCE );
         SetSortMode( &lpWindInfo->vd, nNewSortMode );
         SetViewMode( &lpWindInfo->vd, id );
         if( !fGlobal )
            Amdb_SetTopicViewData( lpWindInfo->lpTopic, &lpWindInfo->vd );
         else
            {
            PCAT pcat;
            PCL pcl;
            PTL ptl;

            for( pcat = Amdb_GetFirstCategory(); pcat; pcat = Amdb_GetNextCategory( pcat ) )
               for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
                  for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
                     {
                     TOPICINFO topicinfo;

                     Amdb_GetTopicInfo( ptl, &topicinfo );
                     SetSortMode( &topicinfo.vd, nNewSortMode );
                     SetViewMode( &topicinfo.vd, id );
                     Amdb_SetTopicViewData( ptl, &topicinfo.vd );
                     }
            }
         SendMessage( hwnd, WM_VIEWCHANGE, lpWindInfo->vd.nViewMode, 0L );
         break;
         }

      case IDM_RENAMEATTACHMENT: {
         HWND hwndAttach;
         register int c;
         int cItems;

         /* Loop for all selected items and delete them.
          */
         VERIFY( hwndAttach = GetDlgItem( hwnd, IDD_ATTACHMENT ) );
         cItems = (int)SendMessage( hwndAttach, LVM_GETCOUNT, 0, 0L );
         for( c = 0; c < cItems; ++c )
         {
            if( SendMessage( hwndAttach, LVM_GETSEL, c, 0L ) )
            {
               LVIEW_ITEM lvi;
               ATTACHMENT at;
//             register int r;
//             UINT uFlags;

               SendMessage( hwndAttach, LVM_GETICONITEM, c, (LPARAM)(LPSTR)&lvi );
               Amdb_GetMsgAttachment( NULL, (PAH)lvi.dwData, &at );
//             wsprintf( lpTmpBuf, GS(IDS_STRING245), at.szPath );
               strcpy( szRenamedFilename, at.szPath );
//             uFlags = ( c < cItems - 1 ) ? MB_YESNOCANCEL : MB_YESNO;
//             if( ( r = fMessageBox( hwnd, 0, lpTmpBuf, uFlags|MB_ICONINFORMATION ) ) == IDYES )
                  {
                     if ( RenameAttachment( hwnd ) )
                     {
                        Amfile_Rename(at.szPath, szRenamedFilename);
                        Amdb_RenameAttachment( lpWindInfo->lpMessage, &at, szRenamedFilename );
                     }
                  }
//             else if( r == IDCANCEL )
//                break;

            }
         }
         UpdateAttachmentPane( hwnd, lpWindInfo->lpMessage );
         break;
                           }

      case IDM_OPENATTACHMENTWITH:
      {
         HWND hwndAttach;
         register int c;
         int cItems;

         /* Make sure message has attachment.
          */
         ASSERT( NULL != lpWindInfo );
         ASSERT( NULL != lpWindInfo->lpMessage );
         if( !Amdb_IsMsgHasAttachments( lpWindInfo->lpMessage ) )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR791), MB_OK|MB_ICONEXCLAMATION );
            break;
            }

         /* Loop for all selected items and activate them.
          */
         VERIFY( hwndAttach = GetDlgItem( hwnd, IDD_ATTACHMENT ) );
         cItems = (int)SendMessage( hwndAttach, LVM_GETCOUNT, 0, 0L );
         for( c = 0; c < cItems; ++c )
            if( SendMessage( hwndAttach, LVM_GETSEL, c, 0L ) )
               {
               LVIEW_ITEM lvi;
               ATTACHMENT at;

               SendMessage( hwndAttach, LVM_GETICONITEM, c, (LPARAM)(LPSTR)&lvi );
               Amdb_GetMsgAttachment( NULL, (PAH)lvi.dwData, &at );
               if(!Am_OpenWith(hwnd, at.szPath))
                  ShowLastError();
               }
         break;
      }

      case IDM_RUNATTACHMENT: {
         HWND hwndAttach;
         register int c;
         int cItems;

         /* Make sure message has attachment.
          */
         ASSERT( NULL != lpWindInfo );
         ASSERT( NULL != lpWindInfo->lpMessage );
         if( !Amdb_IsMsgHasAttachments( lpWindInfo->lpMessage ) )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR791), MB_OK|MB_ICONEXCLAMATION );
            break;
            }

         /* Loop for all selected items and activate them.
          */
         VERIFY( hwndAttach = GetDlgItem( hwnd, IDD_ATTACHMENT ) );
         cItems = (int)SendMessage( hwndAttach, LVM_GETCOUNT, 0, 0L );
         for( c = 0; c < cItems; ++c )
            if( SendMessage( hwndAttach, LVM_GETSEL, c, 0L ) )
               {
               LVIEW_ITEM lvi;
               ATTACHMENT at;
               UINT uRetCode;

               SendMessage( hwndAttach, LVM_GETICONITEM, c, (LPARAM)(LPSTR)&lvi );
               Amdb_GetMsgAttachment( NULL, (PAH)lvi.dwData, &at );
               if( SafeToOpen( hwnd, at.szPath ) )
                  if( ( uRetCode = (UINT)ShellExecute( hwndFrame, NULL, at.szPath, "", pszAmeolDir, SW_SHOW ) ) < 32 )
                     DisplayShellExecuteError( hwnd, at.szPath, uRetCode );
               }
         break;
         }

      case IDM_DELETEATTACHMENT: {
         HWND hwndAttach;
         register int c;
         int cItems;

         /* Loop for all selected items and delete them.
          */
         VERIFY( hwndAttach = GetDlgItem( hwnd, IDD_ATTACHMENT ) );
         cItems = (int)SendMessage( hwndAttach, LVM_GETCOUNT, 0, 0L );
         for( c = 0; c < cItems; ++c )
            if( SendMessage( hwndAttach, LVM_GETSEL, c, 0L ) )
               {
               LVIEW_ITEM lvi;
               ATTACHMENT at;
               register int r;
               UINT uFlags;

               SendMessage( hwndAttach, LVM_GETICONITEM, c, (LPARAM)(LPSTR)&lvi );
               Amdb_GetMsgAttachment( NULL, (PAH)lvi.dwData, &at );
               wsprintf( lpTmpBuf, GS(IDS_STR733), at.szPath );
               uFlags = ( c < cItems - 1 ) ? MB_YESNOCANCEL : MB_YESNO;
               if( ( r = fMessageBox( hwnd, 0, lpTmpBuf, uFlags|MB_ICONINFORMATION ) ) == IDYES )
                  {
                  Amdb_DeleteAttachment( lpWindInfo->lpMessage, &at, ( cItems == 1) );
                  Amfile_Delete( at.szPath );
                  }
               else if( r == IDCANCEL )
                  break;
               }
         UpdateAttachmentPane( hwnd, lpWindInfo->lpMessage );
         break;
         }

      case IDM_SAY:
         switch( Amdb_GetTopicType( lpWindInfo->lpTopic ) )
            {
            case FTYPE_NEWS:  CmdPostArticle( hwnd, NULL ); break;
            case FTYPE_MAIL:  CmdMailMessage( hwnd, NULL, NULL ); break;
            case FTYPE_CIX:   CmdSay( hwnd ); break;
            case FTYPE_LOCAL: CmdLocalSay( hwnd ); break;
            }
         break;

      case IDM_COMMENT:
         if( NULL != lpWindInfo->lpMessage )
            switch( Amdb_GetTopicType( lpWindInfo->lpTopic ) )
               {
               case FTYPE_NEWS:  CmdFollowUpArticle( hwnd, FALSE ); break;
               case FTYPE_MAIL:  CmdMailReply( hwnd, FALSE, TRUE ); break;
               case FTYPE_CIX:   CmdComment( hwnd ); break;
               case FTYPE_LOCAL: CmdLocalCmt( hwnd ); break;
               }
         break;

      case IDM_FOLLOWUPARTICLE:
         CmdFollowUpArticle( hwnd, FALSE );
         break;

      case IDM_FOLLOWUPARTICLETOALL:
         CmdFollowUpArticle( hwnd, TRUE );
         break;

      case IDM_SHOWRESUME: {
         if( lpWindInfo->lpMessage )
            {
            MSGINFO msginfo;

            Amdb_GetMsgInfo( lpWindInfo->lpMessage, &msginfo );
            CmdShowResume( hwnd, msginfo.szAuthor, TRUE );
            }
         break;
         }

      case IDM_VIEWINBROWSER:
         if( lpWindInfo->lpMessage )
            CmdDisplayInBrowser( hwnd, lpWindInfo->lpMessage );
         break;

      case IDM_SORTMAILFROM:
         CreateSortMailFrom( hwnd );
         break;

      case IDM_SORTMAILTO:
         CreateSortMailTo( hwnd );
         break;

      case IDM_FORWARDMSG:
         CmdForwardMessage( hwnd );
         break;

      case IDM_REPLYTOALL:
         CmdMailReply( hwnd, TRUE, TRUE );
         break;

      case IDM_REPLY:
         CmdMailReply( hwnd, FALSE, TRUE );
         break;

      case IDM_ADDTOADDRBOOK:
         CmdAddToAddrbook( hwnd );
         break;

      case IDM_DECODE:
         if( lpWindInfo->lpMessage )
            if( ( NULL != lpWindInfo->lpTopic ) && Amdb_GetTopicType( lpWindInfo->lpTopic ) != FTYPE_CIX )
               if( !Amdb_IsHeaderMsg( lpWindInfo->lpMessage ) )
                  if( DecodeMessage( hwnd, FALSE ) )
                     {
                     ShowMsg( hwnd, lpWindInfo->lpMessage, FALSE, TRUE, FALSE );
                     if( Amdb_IsMsgHasAttachments( lpWindInfo->lpMessage ) && fLaunchAfterDecode )
                        {
                        HWND hwndAttach;
         
                        VERIFY( hwndAttach = GetDlgItem( hwnd, IDD_ATTACHMENT ) );
                        SendMessage( hwndAttach, LVM_SETSEL, 0, 0L );
                        PostCommand( hwnd, IDM_RUNATTACHMENT, 0L );
                        }
                     }
         break;

      case IDM_COVERNOTE:
         CmdCoverNote( hwnd );
         break;

      case IDD_HSPLIT: {
         HWND hwndFolderList;
         HWND hwndList;
         HWND hwndHdr;
         int aSplits[ 2 ];
         int wState;
         RECT rc;
         int cy;

         GetClientRect( hwnd, &rc );
         cy = 0;
         switch( codeNotify )
            {
            default:
               ASSERT(FALSE);
               break;

            case SPN_SPLITDBLCLK:
               if( GetKeyState( VK_SHIFT ) & 0x8000 )
               {
               cy = lpWindInfo->nSplitCur;
               cy = ( cy == 0 ) ? lpWindInfo->nSplitPos : 0;
               }
               else
                  cy = lpWindInfo->nSplitCur;
               break;

            case SPN_SPLITSET:
               cy = (int)hwndCtl;
               cy = ( cy < 0 ) ? 0 : ( cy > rc.bottom ) ? 0 : cy;
               if( cy )
                  lpWindInfo->nSplitPos = cy;
               break;
            }
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         VERIFY( hwndFolderList = GetDlgItem( hwnd, IDD_FOLDERLIST ) );
         VERIFY( hwndHdr = GetDlgItem( hwnd, IDD_MSGIDTITLE ) );
         if( cy == 0 )
            {
            ShowWindow( hwndList, SW_HIDE );
            if( !fAltMsgLayout )
               if( 0 != lpWindInfo->nHSplitCur )
                  ShowWindow( hwndFolderList, SW_HIDE );
            if( hwndHdr )
               ShowWindow( hwndHdr, SW_HIDE );
            }
         else if( ( GetWindowStyle( hwndList ) & WS_VISIBLE ) == 0 )
            {
            ShowWindow( hwndList, SW_SHOW );
            if( !fAltMsgLayout )
               if( 0 != lpWindInfo->nHSplitCur )
                  ShowWindow( hwndFolderList, SW_SHOW );
            if( hwndHdr )
               ShowWindow( hwndHdr, SW_SHOW );
            }
         lpWindInfo->nSplitCur = cy;
         aSplits[ 0 ] = lpWindInfo->nSplitPos;
         aSplits[ 1 ] = lpWindInfo->nSplitCur;
         Amuser_WritePPArray( szMsgViewerWnd, "SplitBarPosition", aSplits, 2 );
         wState = IsMaximized( hwnd ) ? SIZE_MAXIMIZED : SIZE_RESTORED;
         SendMessage( hwnd, WM_SIZE, wState, MAKELONG( rc.right, rc.bottom ) );
         break;
         }

      case IDD_VSPLIT: {
         HWND hwndFolderList;
         int wState;
         int aSplits[ 2 ];
         RECT rc;
         int cx;

         GetClientRect( hwnd, &rc );
         cx = 0;
         switch( codeNotify )
            {
            default:
               ASSERT(FALSE);
               break;

            case SPN_SPLITDBLCLK:
               if( GetKeyState( VK_SHIFT ) & 0x8000 )
               {
               cx = lpWindInfo->nHSplitCur;
               cx = ( cx == 0 ) ? lpWindInfo->nHSplitPos : 0;
               }
               else
                  cx = lpWindInfo->nHSplitCur;
               break;

            case SPN_SPLITSET:
               cx = (int)hwndCtl;
               cx = ( cx < 0 ) ? 0 : ( cx > rc.bottom ) ? 0 : cx;
               if( cx )
                  lpWindInfo->nHSplitPos = cx;
               break;
            }
         VERIFY( hwndFolderList = GetDlgItem( hwnd, IDD_FOLDERLIST ) );
         if( cx == 0 )
            {
            if( IsWindowVisible( hwndFolderList ) )
               ShowWindow( hwndFolderList, SW_HIDE );
            }
         else 
            {
            if( !IsWindowVisible( hwndFolderList ) )
               ShowWindow( hwndFolderList, SW_SHOW );
            }
         lpWindInfo->nHSplitCur = cx;
         aSplits[ 0 ] = lpWindInfo->nHSplitPos;
         aSplits[ 1 ] = lpWindInfo->nHSplitCur;
         Amuser_WritePPArray( szMsgViewerWnd, "FolderSplitBarPosition", aSplits, 2 );
         wState = IsMaximized( hwnd ) ? SIZE_MAXIMIZED : SIZE_RESTORED;
         SendMessage( hwnd, WM_SIZE, wState, MAKELONG( rc.right, rc.bottom ) );
         break;
         }

      case IDM_MARKTHREADREAD:
         if( pth = lpWindInfo->lpMessage )
            {
            Amdb_MarkThreadRead( pth );
            UpdateThreadDisplay( hwnd, TRUE );
            UpdateStatusDisplay( hwnd );
            if( !( fCompatFlag & COMF_NOJUMPAFTERMARKINGREAD ) )
               SendCommand( hwnd, IDM_NEXT, 0 );
            }
         break;

      case IDM_WATCH:
         if( Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_NEWS )
            {
            LPINT lpi;

            if( lpi = GetSelectedItems( hwnd ) )
               MarkWatchRange( hwnd, lpi );
            }
         break;

      case IDM_IGNORE: {
         LPINT lpi;

         if( lpi = GetSelectedItems( hwnd ) )
            MarkIgnoreRange( hwnd, lpi );
         break;
         }

      case IDM_IGNORETHREAD: {
         LPINT lpi;

         if( lpi = GetThread( hwnd ) )  
         {
            MarkIgnoreRange( hwnd, lpi );          
         }
         break;
         }

      case IDM_PRIORITY: {
         LPINT lpi;

         if( lpi = GetSelectedItems( hwnd ) )
            MarkPriorityRange( hwnd, lpi );
         break;
         }

      case IDM_PRIORITYTHREAD: {
         LPINT lpi;

         if( lpi = GetThread( hwnd ) )
            MarkPriorityRange( hwnd, lpi );
         break;
         }

      case IDM_KEEPTHREAD: {
         LPINT lpi;

         if( lpi = GetThread( hwnd ) )
            MarkKeepRange( hwnd, lpi );
         break;
         }

      case IDM_KEEPMSG: {
         LPINT lpi;

         if( lpi = GetSelectedItems( hwnd ) )
            MarkKeepRange( hwnd, lpi );
         break;
         }

      case IDM_WITHDRAW: {
         LPINT lpi;

         if( lpi = GetSelectedItems( hwnd ) )
            MarkMsgWithdrawn( hwnd, lpi );
         break;
         }

      case IDM_READLOCK: {
         LPINT lpi;

         if( lpi = GetSelectedItems( hwnd ) )
            MarkReadLockRange( hwnd, lpi );
         break;
         }

      case IDM_READLOCKTHREAD: {
         LPINT lpi;

         if( lpi = GetThread( hwnd ) )
            MarkReadLockRange( hwnd, lpi );
         break;
         }

      case IDM_DELETEMSG: {
         PTH FAR * lppth;

         if( lppth = GetSelectedItemsHandles( hwnd ) )
            DeleteMsgRange( hwnd, lppth, FALSE );
         break;
         }

      case IDM_DELETETHREAD: {
         PTH FAR * lppth;

         if( lppth = GetThreadHandles( hwnd ) )
            DeleteMsgRange( hwnd, lppth, FALSE );
         break;
         }

      case IDM_KILLMSG: {
         PTH FAR * lppth;

         if( lppth = GetSelectedItemsHandles( hwnd ) )
         {
            DeleteMsgRange( hwnd, lppth, TRUE );
            UpdateCurrentMsgDisplay( hwnd );
         }
         break;
         }

      case IDM_KILLTHREAD: {
         PTH FAR * lppth;

         if( lppth = GetThreadHandles( hwnd ) )
            DeleteMsgRange( hwnd, lppth, TRUE );
         break;
         }

      case IDM_COPYMSG: {
         PTH FAR * lppth;

         if( lppth = GetSelectedItemsHandles( hwnd ) )
            CopyMessageRange( hwnd, lppth, FALSE, FALSE );
         break;
         }
      
      case IDM_MOVEMSG: {
         PTH FAR * lppth;

         if( lppth = GetSelectedItemsHandles( hwnd ) )
            CopyMessageRange( hwnd, lppth, TRUE, FALSE );
         break;
         }

      case IDM_REPOST: {
         PTH FAR * lppth;

         if( lppth = GetSelectedItemsHandles( hwnd ) )
            CopyMessageRange( hwnd, lppth, FALSE, TRUE );
         break;
         }


      case IDM_BACKSPACE:
         if( nBookPtr ) {
            PTH pth;

            --nBookPtr;
            FreeMemory( &cmBookMark[ nBookPtr ].pszTitle );
            Amdb_LockTopic( cmBookMark[ nBookPtr ].ptl );
            if( pth = Amdb_OpenMsg( cmBookMark[ nBookPtr ].ptl, cmBookMark[ nBookPtr ].dwMsg ) )
               {
               if( !( fCompatFlag & COMF_NOMARKUNREADONBACKSPACE ) )
                  if( cmBookMark[ nBookPtr ].fUnread )
                     {
                     Amdb_MarkMsgRead( pth, FALSE );
                     UpdateCurrentMsgDisplay( hwnd );
                     UpdateStatusDisplay( hwnd );
                     }
               InternalSetCurrentMsg( cmBookMark[ nBookPtr ].ptl, pth, TRUE, TRUE );
               }
            Amdb_UnlockTopic( cmBookMark[ nBookPtr ].ptl );
            }
         break;

      case IDM_TOGGLETHREAD:
         if( pth = lpWindInfo->lpMessage )
            if( Amdb_IsExpanded( pth ) )
               goto Shrink;
         /* Fall through to expand thread */

      case IDM_EXPANDTHREAD:
         if( pth = lpWindInfo->lpMessage )
            if( Amdb_ExpandThread( pth, TRUE ) )
               {
               HWND hwndList;
               int nSel;

               VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
               nSel = ListBox_GetTopIndex( hwndList );
               SetMsgViewerWindow( hwnd, lpWindInfo->lpTopic, nSel );
               fQuickUpdate = TRUE;
               ShowMsg( hwnd, pth, TRUE, TRUE, FALSE );
               fQuickUpdate = FALSE;
               }
         break;

      case IDM_SHRINKTHREAD:
Shrink:  if( pth = lpWindInfo->lpMessage )
            if( Amdb_ExpandThread( pth, FALSE ) )
               {
               HWND hwndList;
               int nSel;

               VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
               nSel = ListBox_GetTopIndex( hwndList );
               SetMsgViewerWindow( hwnd, lpWindInfo->lpTopic, nSel );
               pth = Amdb_GetRootMsg( pth, TRUE );
               fQuickUpdate = TRUE;
               ShowMsg( hwnd, pth, TRUE, TRUE, FALSE );
               fQuickUpdate = FALSE;
               }
         break;

      case IDM_MARKTAGGED: {
         LPINT lpi;

         if( Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_NEWS )
            if( lpi = GetSelectedItems( hwnd ) )
               MarkTaggedRange( hwnd, lpi );
         break;
         }

      case IDM_MARKTHREADTAGGED: {
         LPINT lpi;

         if( Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_NEWS )
            if( lpi = GetThread( hwnd ) )
               MarkTaggedRange( hwnd, lpi );
         break;
         }

      case IDM_MARKTHREAD: {
         LPINT lpi;

         if( lpi = GetThread( hwnd ) )
            MarkMsgRange( hwnd, lpi );
         break;
         }

      case IDM_MARKMSG: {
         LPINT lpi;

         if( lpi = GetSelectedItems( hwnd ) )
            MarkMsgRange( hwnd, lpi );
         break;
         }

      case IDM_READMSG: {
         LPINT lpi;

         if( lpi = GetSelectedItems( hwnd ) )
            MarkReadRange( hwnd, lpi );
         break;
         }

      case IDM_READTHREAD: {
         LPINT lpi;

         if( lpi = GetThread( hwnd ) )
            MarkReadRange( hwnd, lpi );
         break;
         }

      case IDM_MARKTOPICREAD:
         CmdMarkTopicRead( hwnd );
         break;

      case IDM_PREVUNREAD: {
         CURMSG unr;

         Ameol2_GetCurrentMsg( &unr );
         if( NULL != unr.pth )
            while( unr.pth = Amdb_GetPreviousMsg( unr.pth, VM_VIEWREF ) )
               if( !Amdb_IsRead( unr.pth ) )
                  {
                  SetCurrentMsg( &unr, TRUE, TRUE );
                  break;
                  }
         break;
         }

      case IDM_PREVROOT: {
         CURMSG unr;

         Ameol2_GetCurrentMsg( &unr );
         if( NULL != unr.pth )
            if( unr.pth = Amdb_GetPreviousMsg( unr.pth, VM_VIEWREF ) )
               if( unr.pth = Amdb_ExGetRootMsg( unr.pth, TRUE, lpWindInfo->vd.nViewMode ) )
                  SetCurrentMsg( &unr, TRUE, TRUE );
         break;
         }

      case IDM_PREVTOPIC: {
         CURMSG unr;

         unr.ptl = lpWindInfo->lpTopic;
         unr.pcl = Amdb_FolderFromTopic( unr.ptl );
         unr.pcat = Amdb_CategoryFromFolder( unr.pcl );
         if( ( unr.ptl = Amdb_GetPreviousTopic( unr.ptl ) ) == NULL )
            {
            do {
               if( ( unr.pcl = Amdb_GetPreviousFolder( unr.pcl ) ) == NULL )
                  do {
                     if( ( unr.pcat = Amdb_GetPreviousCategory( unr.pcat ) ) == NULL )
                        unr.pcat = Amdb_GetLastCategory();
                     unr.pcl = Amdb_GetLastFolder( unr.pcat );
                     }
                  while( unr.pcl == NULL );
               unr.ptl = Amdb_GetLastTopic( unr.pcl );
               }
            while( unr.ptl == NULL );
            }
         if( unr.ptl != lpWindInfo->lpTopic )
            {
            InternalSetMsgBookmark( FALSE, FALSE );
            OpenTopicViewerWindow( unr.ptl );
            }
         break;
         }

      case IDM_NEXTTOPIC: {
         CURMSG unr;

         unr.ptl = lpWindInfo->lpTopic;
         unr.pcl = Amdb_FolderFromTopic( unr.ptl );
         unr.pcat = Amdb_CategoryFromFolder( unr.pcl );
         if( ( unr.ptl = Amdb_GetNextTopic( unr.ptl ) ) == NULL )
            {
            do {
               if( ( unr.pcl = Amdb_GetNextFolder( unr.pcl ) ) == NULL )
                  do {
                     if( ( unr.pcat = Amdb_GetNextCategory( unr.pcat ) ) == NULL )
                        unr.pcat = Amdb_GetFirstCategory();
                     unr.pcl = Amdb_GetFirstFolder( unr.pcat );
                     }
                  while( unr.pcl == NULL );
               unr.ptl = Amdb_GetFirstTopic( unr.pcl );
               }
            while( unr.ptl == NULL );
            }
         if( unr.ptl != lpWindInfo->lpTopic )
            {
            InternalSetMsgBookmark( FALSE, FALSE );
            OpenTopicViewerWindow( unr.ptl );
            }
         break;
         }

      case IDM_NEXTUNREAD: {
         CURMSG unr;

         Ameol2_GetCurrentMsg( &unr );
         if( NULL != unr.pth )
            while( unr.pth = Amdb_GetNextMsg( unr.pth, VM_VIEWREF ) )
               if( !Amdb_IsRead( unr.pth ) )
                  {
                  SetCurrentMsg( &unr, TRUE, TRUE );
                  break;
                  }
         break;
         }

      case IDM_NEXTROOT: {
         CURMSG unr;

         Ameol2_GetCurrentMsg( &unr );
         if( NULL != unr.pth )
            if( unr.pth = Amdb_GetNextMsg( unr.pth, VM_VIEWREF ) )
               if( unr.pth = Amdb_ExGetRootMsg( unr.pth, FALSE, lpWindInfo->vd.nViewMode ) )
                  SetCurrentMsg( &unr, TRUE, TRUE );
         break;
         }

      case IDM_NEXTALL:
      case IDM_NEXT: {
         BOOL fUnread = FALSE;
         CURMSG unr;
         PTL ptl;

         /* Mark the current message read.
          */
         ptl = lpWindInfo->lpTopic;
         pth = lpWindInfo->lpMessage;
         if( pth && !Amdb_IsRead( pth ) && !fKeepUnread )
            {
            Amdb_MarkMsgRead( pth, TRUE );
            fUnread = TRUE;
            }

         /* If we're not in roots mode, use the MoveToNextUnreadMessage
          * function to advance to the next unread message in this
          * topic.
          */
         if( lpWindInfo->vd.nViewMode != VM_VIEWROOTS )
//       if( lpWindInfo->vd.nViewMode == VM_VIEWREF )
            {
            if( MoveToNextUnreadMessage( hwnd, fUnread ) )
               break;
            }

         /* Otherwise use Amdb_GetNextUnReadMsg which is slower and
          * more complex but which handles roots mode properly.
          */
         unr.pcl = Amdb_FolderFromTopic( ptl );
         unr.pcat = Amdb_CategoryFromFolder( unr.pcl );
         unr.ptl = ptl;
         unr.pth = pth;
         if( Amdb_GetNextUnReadMsg( &unr, VM_USE_ORIGINAL ) )
            {
            if( !( id == IDM_NEXTALL && unr.ptl != ptl ) || ( pth == NULL ) )
               {
               if( lpWindInfo->vd.nViewMode == VM_VIEWROOTS && pth && Amdb_GetRootMsg( unr.pth, TRUE ) != Amdb_GetRootMsg( pth, TRUE ) )
                  unr.pth = Amdb_GetRootMsg( unr.pth, TRUE );
               SetCurrentMsgEx( &unr, TRUE, fUnread, FALSE );
               }
            else {
               if( fBeepAtEnd )
                  AmMessageBeep();
               InvalidateRect( GetDlgItem( hwnd, IDD_LIST ), NULL, TRUE );
               UpdateStatusDisplay( hwnd );
               }
            Amdb_InternalUnlockTopic( unr.ptl );
            }
         else
            {
            if( NULL != lpWindInfo->lpMessage )
               HandleNotification( hwnd, pth );
            if( !fNoisy && fBeepAtEnd )
               AmMessageBeep();
            fMessageBox( hwnd, 0, GS(IDS_STR58), MB_OK|MB_ICONEXCLAMATION );
            InvalidateRect( GetDlgItem( hwnd, IDD_LIST ), NULL, TRUE );
            UpdateStatusDisplay( hwnd );
            SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_NEXT, FALSE );
            ShowUnreadMessageCount();
            }
         ForceFocusToList( hwnd );
         break;
         }

      case IDM_FORWARD: {
         CURMSG unr;
         int nMode;

         Ameol2_GetCurrentMsg( &unr );
         nMode = ( lpWindInfo->vd.nViewMode == VM_VIEWCHRON ) ? VM_VIEWCHRON : VM_VIEWREFEX;
         if( unr.pth )
            unr.pth = Amdb_GetNextMsg( unr.pth, nMode );
         while( unr.pth == NULL )
            {
            Amdb_UnlockTopic( unr.ptl );
            if( ( unr.ptl = Amdb_GetNextTopic( unr.ptl ) ) == NULL )
               {
               do {
                  if( ( unr.pcl = Amdb_GetNextFolder( unr.pcl ) ) == NULL )
                     unr.pcl = Amdb_GetFirstFolder( unr.pcat );
                  unr.ptl = Amdb_GetFirstTopic( unr.pcl );
                  }
               while( unr.ptl == NULL );
               }
            if( unr.ptl )
               {
               Amdb_LockTopic( unr.ptl );
               unr.pth = Amdb_GetFirstMsg( unr.ptl, nMode );
               }
            }
         SetCurrentMsg( &unr, TRUE, TRUE );
         ForceFocusToList( hwnd );
         break;
         }

      case IDM_PREV: {
         CURMSG unr;
         int nMode;

         Ameol2_GetCurrentMsg( &unr );
         nMode = ( lpWindInfo->vd.nViewMode == VM_VIEWCHRON ) ? VM_VIEWCHRON : VM_VIEWREFEX;
         if( unr.pth )
            unr.pth = Amdb_GetPreviousMsg( unr.pth, nMode );
         while( unr.pth == NULL )
            {
            Amdb_UnlockTopic( unr.ptl );
            if( ( unr.ptl = Amdb_GetPreviousTopic( unr.ptl ) ) == NULL )
               {
               if( ( unr.pcl = Amdb_GetPreviousFolder( unr.pcl ) ) == NULL )
                  unr.pcl = Amdb_GetFirstFolder( unr.pcat );
               unr.ptl = Amdb_GetLastTopic( unr.pcl );
               }
            if( unr.ptl )
               {
               Amdb_LockTopic( unr.ptl );
               unr.pth = Amdb_GetLastMsg( unr.ptl, nMode );
               }
            }
         SetCurrentMsg( &unr, TRUE, TRUE );
         ForceFocusToList( hwnd );
         break;
         }

      case IDM_ORIGINAL: {
         PTH pth;

         pth = lpWindInfo->lpMessage;
         if( pth ) {
            MSGINFO msginfo;

            Amdb_GetMsgInfo( pth, &msginfo );
            if( msginfo.wLevel == 0 )
               {
               if( msginfo.dwComment )
                  DownloadMsg( hwnd, lpWindInfo->lpTopic, msginfo.dwComment );
               else
                  fMessageBox( hwnd, 0, GS(IDS_STR60), MB_OK|MB_ICONEXCLAMATION );
               }
            else if( pth = Amdb_GetMsg( pth, msginfo.dwComment ) ) {
               CURMSG unr;

               unr.pth = pth;
               unr.ptl = lpWindInfo->lpTopic;
               unr.pcl = Amdb_FolderFromTopic( unr.ptl );
               unr.pcat = Amdb_CategoryFromFolder( unr.pcl );
               SetCurrentMsg( &unr, TRUE, TRUE );
               }
            else
               fMessageBox( hwnd, 0, GS(IDS_STR62), MB_OK|MB_ICONEXCLAMATION );
            }
         ForceFocusToList( hwnd );
         break;
         }

      case IDM_GOTO: {
         char dwGotoMsg[255];
         
         //       if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_GOTO), Goto, (LPARAM)(LPSTR)&dwGotoMsg ) )
         if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_GOTO), Goto, (LPARAM)(LPSTR)&dwGotoMsg ) )
         {
            if ( strstr(dwGotoMsg, ":") != NULL)
            {
               CommonHotspotHandler( hwnd, dwGotoMsg );
            }
            else
            {
               CURMSG unr;
               DWORD lGotoMsg=0, i;
               
               i = 0;
               while( isdigit( dwGotoMsg[i] ) )
                  lGotoMsg = lGotoMsg * 10 + ( dwGotoMsg[i++] - '0' );
               
               pth = lpWindInfo->lpMessage;
               Ameol2_GetCurrentMsg( &unr );
               if( NULL == unr.pth || ( unr.pth = Amdb_GetMsg( pth, lGotoMsg ) ) == NULL )
                  DownloadMsg( hwnd, unr.ptl, lGotoMsg );
               else
                  SetCurrentMsg( &unr, TRUE, TRUE );
            }
         }
         break;
         }

      case IDD_LIST:
         if( codeNotify == LBN_SELCHANGE )
            {
            HWND hwndList;
            int nSel;

            VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
            if( ( nSel = ListBox_GetCaretIndex( hwndList ) ) != LB_ERR )
               {
               PTH pth;

               pth = (PTH)ListBox_GetItemData( hwndList, nSel );
               if( pth != lpWindInfo->lpMessage )
                  {
                  InternalSetMsgBookmark( FALSE, FALSE );
                  if( fKeyboard )
                     CenterCurrentMsg( hwnd );
                  InternalSetCurrentMsg( lpWindInfo->lpTopic, pth, FALSE, FALSE );
                  }
               }
            }
         break;
      }
   ToolbarState_MessageWindow( ); // !!SM!! 2.56.2058
   ToolbarState_CopyPaste(); //!!SM!! 2039
   FORWARD_WM_COMMAND( hwnd, id, hwndCtl, codeNotify, DefMDIChildProc );
}

/* This function adjusts the sort mode for the specified topic.
 */
void FASTCALL SetSortMode( VIEWDATA * pvd, int nNewMode )
{
   if( GetSortMode( pvd ) == nNewMode )
      pvd->nSortMode ^= VSM_ASCENDING;
   else
      {
      BOOL fIsAscending;

      fIsAscending = pvd->nSortMode & VSM_ASCENDING;
      pvd->nSortMode = nNewMode;
      if( fIsAscending )
         pvd->nSortMode |= VSM_ASCENDING;
      }
}

/* This function displays the current message in a WWW browser.
 */
void FASTCALL CmdDisplayInBrowser( HWND hwnd, PTH pth )
{
   HPSTR lpStrBody;
   HPSTR lpStr;
   HNDFILE fh;

   /* Get the message itself and remove the header.
    */
   lpStr = Amdb_GetMsgText( pth );
   lpStrBody = GetTextBody( Amdb_TopicFromMsg( pth ), lpStr );

   /* Create a temporary file for saving the message.
    */
#ifdef WIN32
   GetTempPath( _MAX_PATH, lpPathBuf );
   GetTempFileName( lpPathBuf, "~A2", 0, lpPathBuf );
   ChangeExtension( lpPathBuf, "html" );
#else
   GetTempFileName( GetTempDrive( 0 ), "~A2", 0, lpPathBuf );
   ChangeExtension( lpPathBuf, "htm" );
#endif
   if( HNDFILE_ERROR != ( fh = Amfile_Create( lpPathBuf, 0 ) ) )
      {
      /* Write the file.
       */
      Amfile_Write32( fh, lpStrBody, hstrlen(lpStrBody) );
      Amfile_Close( fh );

      /* Now open the file in the browser as a local file.
       */
//    if( NULL != strstr( szBrowser, "Opera" ) || NULL != strstr( szBrowser, "opera" ) )
//       wsprintf( lpTmpBuf, "file://localhost/%s", lpPathBuf );
//    else
         wsprintf( lpTmpBuf, "file://%s", lpPathBuf );
      CommonHotspotHandler( hwnd, lpTmpBuf );
      }
   Amdb_FreeMsgTextBuffer( lpStr );
}

/* This function handles the Forward Message command.
 */
void FASTCALL CmdForwardMessage( HWND hwnd )
{
   LPWINDINFO lpWindInfo;
         
   lpWindInfo = GetBlock( hwnd );
   if( lpWindInfo->lpMessage )
      CmdForwardMailMessage( hwnd, lpWindInfo->lpMessage );
}

/* This function shows or hides the specified column in the
 * message viewer window.
 */
void FASTCALL ShowColumn( HWND hwnd, int idCol )
{
   LPWINDINFO lpWindInfo;
   BOOL fGlobal;

   /* Get and toggle the current state.
    */
   lpWindInfo = GetBlock( hwnd );
   lpWindInfo->vd.wHeaderFlags ^= idCol;

   /* If the Ctrl key is down, apply changes to the current topic
    * otherwise apply changes globally.
    */
   fGlobal = ( GetKeyState( VK_CONTROL ) & 0x8000 ) == 0x8000;
   if( fGlobal )
      {
      PCAT pcat;
      PCL pcl;
      PTL ptl;

      for( pcat = Amdb_GetFirstCategory(); pcat; pcat = Amdb_GetNextCategory( pcat ) )
         for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
            for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
               {
               TOPICINFO topicinfo;

               Amdb_GetTopicInfo( ptl, &topicinfo );
               topicinfo.vd.wHeaderFlags = lpWindInfo->vd.wHeaderFlags;
               Amdb_SetTopicViewData( ptl, &topicinfo.vd );
               }
      }
   Amdb_SetTopicViewData( lpWindInfo->lpTopic, &lpWindInfo->vd );
   SendMessage( hwnd, WM_HEADERSCHANGED, 0, 0L );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL MsgViewWnd_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   /* If the message window has been minimised then just
    * disable the Split command on its system menu.
    */
   if( state == SIZE_MINIMIZED ) {
      HMENU hMenu;

      hMenu = GetSystemMenu( hwnd, FALSE );
      EnableMenuItem( hMenu, IDM_SPLIT, MF_GRAYED );
      }
   else {
      HWND hwndHdr;
      HWND hwndList;
      HWND hwndFolderList;
      HWND hwndHSplit;
      HWND hwndVSplit;
      HWND hwndInfoBar;
      HWND hwndEdit;
      HWND hwndAttach;
      LPWINDINFO lpWindInfo;
      TEXTMETRIC tm;
      HFONT hOldFont;
      HDC hdc;
      int cy2;
      int cySplit;
      int cxSplit;
      int cyList;
      int cxFolderList;
      int cyHdr;
      int cyAttachment;
      RECT rcMain;

      /* If the message window has been restored or maximised, then
       * enable the Split command on the system menu.
       */
      if( state == SIZE_RESTORED || state == SIZE_MAXIMIZED )
         {
         HMENU hMenu;

         hMenu = GetSystemMenu( hwnd, FALSE );
         EnableMenuItem( hMenu, IDM_SPLIT, MF_ENABLED );
         }

      /* Get the handles of each message window child window.
       */
      hwndHdr = GetDlgItem( hwnd, IDD_MSGIDTITLE );
      hwndList = GetDlgItem( hwnd, IDD_LIST );
      hwndInfoBar = GetDlgItem( hwnd, IDD_STATUS );
      hwndEdit = GetDlgItem( hwnd, IDD_MESSAGE );
      hwndHSplit = GetDlgItem( hwnd, IDD_HSPLIT );
      hwndVSplit = GetDlgItem( hwnd, IDD_VSPLIT );
      hwndAttach = GetDlgItem( hwnd, IDD_ATTACHMENT );
      hwndFolderList = GetDlgItem( hwnd, IDD_FOLDERLIST );

      /* Compute the height of the info bar based on the 8-point Helv
       * font height plus some extra for the borders.
       */
      hdc = GetDC( hwnd );
      hOldFont = SelectFont( hdc, hHelv8Font );
      GetTextMetrics( hdc, &tm );
      cyInfoBar = tm.tmHeight + 8;
      SelectFont( hdc, hOldFont );
      ReleaseDC( hwnd, hdc );

      /* Predefined heights.
       */
      cySplit = 6;
      cxSplit = 6;
      cyAttachment = 0;
      cyHdr = 0;

      GetClientRect( hwnd, &rcMain );

      /* Compute the height and width of the treeview and list window. This will help us to
       * compute in turn the height and position of the message window.
       */
      lpWindInfo = GetBlock( hwnd );
      if( GetWindowStyle( hwndFolderList ) & WS_VISIBLE )
         {
         int nSplit;

         if( lpWindInfo )
            nSplit = lpWindInfo->nHSplitCur;
         else
            nSplit = 30;
         cxFolderList = (int)( ( (float)nSplit / 100.0 ) * cx );
         }
      else
         cxFolderList = 0;
      if( GetWindowStyle( hwndList ) & WS_VISIBLE )
      {
         RECT rc;
         int nSplit;

         /* Compute the height of the thread list. The extra
          * code that calls GetWindowRect is needed because the
          * list is an integral height.
          */
         if( lpWindInfo )
            nSplit = lpWindInfo->nSplitCur;
         else
            nSplit = 40;
         cyList = (int)( ( (float)nSplit / 100.0 ) * cy );
         if( cyList )
            {
            GetWindowRect( hwndHdr, &rc );
            cyHdr = 18; //rc.bottom - rc.top;
            cyList -= cyHdr;
            }

         /* Resize the header control.
          */
         SetWindowPos( hwndHdr, NULL, cxSplit + cxFolderList, 0, cx - ( cxSplit + cxFolderList ), cyHdr, SWP_NOZORDER|SWP_NOACTIVATE );

         /* Resize the listbox control.
          */
         SetWindowPos( hwndList, NULL, cxSplit + cxFolderList, cyHdr, cx - ( cxSplit + cxFolderList ), cyList, SWP_NOZORDER|SWP_NOACTIVATE );

         /* Because the listbox is a integral height type, the size we specify
          * in cyList won't necessarily be the final size, so use GetWindowRect
          * again to retrieve the actual listbox height.
          */
         GetWindowRect( hwndList, &rc );
         cyList = rc.bottom - rc.top;

         /* Now size the treeview control.
          */
         if( fAltMsgLayout )
            SetWindowPos( hwndFolderList, NULL, 0, 0, cxFolderList, rcMain.bottom, SWP_NOZORDER|SWP_NOACTIVATE );
         else
            SetWindowPos( hwndFolderList, NULL, 0, 0, cxFolderList, cyList + cyHdr, SWP_NOZORDER|SWP_NOACTIVATE );

         SetEditorDefaults(hwndEdit, TRUE, lpWindInfo->fFixedFont ? lfFixedFont : lfMsgFont, fWordWrap, fHotLinks , fMsgStyles, crMsgText, crMsgWnd );
      }
      else
         cyList = 0;

      /* Set the header extent.
       */
      Header_SetExtent( hwndHdr, cyList + cyHdr );
   
      /* Compute the height of the original window as a percentage of the space occupied
       * by the bottom part of the message window.
       */
      cy2 = cyList + cyHdr;
      if( fAltMsgLayout )
      {
         SetWindowPos( hwndHSplit, NULL, cxFolderList + cxSplit, cy2, cx, cySplit, SWP_NOZORDER|SWP_NOACTIVATE );
         SetWindowPos( hwndVSplit, NULL, cxFolderList, 0, cxSplit, rcMain.bottom, SWP_NOZORDER|SWP_NOACTIVATE );
      }
      else
      {
         SetWindowPos( hwndHSplit, NULL, 0, cy2, cx, cySplit, SWP_NOZORDER|SWP_NOACTIVATE );
         SetWindowPos( hwndVSplit, NULL, cxFolderList, 0, cxSplit, cy2, SWP_NOZORDER|SWP_NOACTIVATE );
      }


      /* Resize the info bar control.
       */
      cy2 += cySplit;
      if( fAltMsgLayout )
         SetWindowPos( hwndInfoBar, NULL, cxFolderList + cxSplit, cy2, cx - cxFolderList - cxSplit, cyInfoBar + 1, SWP_NOZORDER|SWP_NOACTIVATE );
      else
         SetWindowPos( hwndInfoBar, NULL, 0, cy2, cx, cyInfoBar + 1, SWP_NOZORDER|SWP_NOACTIVATE );

      /* If the attachment window is visible, show it.
       */
      cy2 += cyInfoBar + 1;
      if( GetWindowStyle( hwndAttach ) & WS_VISIBLE )
         {
         cyAttachment = GetSystemMetrics( SM_CYICONSPACING ) + GetSystemMetrics( SM_CXVSCROLL ) ;
         if( fAltMsgLayout )
//          SetWindowPos( hwndAttach, NULL, cxFolderList + cxSplit, cy2, cx, cyAttachment, SWP_NOZORDER|SWP_NOACTIVATE );
            SetWindowPos( hwndAttach, NULL, cxFolderList + cxSplit, cy2, cx - cxFolderList - cxSplit, cyAttachment, SWP_NOZORDER|SWP_NOACTIVATE ); // 2.56.2056
         else
            SetWindowPos( hwndAttach, NULL, 0, cy2, cx, cyAttachment, SWP_NOZORDER|SWP_NOACTIVATE );
         }

      /* Resize the edit control.
       */
      cy2 += cyAttachment;
      if( fAltMsgLayout )
         SetWindowPos( hwndEdit, NULL, cxFolderList + cxSplit, cy2, cx - cxFolderList - cxSplit, cy - cy2, SWP_NOZORDER|SWP_NOACTIVATE );
      else
         SetWindowPos( hwndEdit, NULL, 0, cy2, cx, cy - cy2, SWP_NOZORDER|SWP_NOACTIVATE );

      /* All done!
       */
      CenterCurrentMsg( hwnd );
      if( hwnd == hwndMsg )
         Amuser_WriteWindowState( szMsgViewWndName, hwnd );
      }
   FORWARD_WM_SIZE( hwnd, state, cx, cy, DefMDIChildProc );
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL MsgViewWnd_OnMove( HWND hwnd, int x, int y )
{
   Amuser_WriteWindowState( szMsgViewWndName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, DefMDIChildProc );
}

/* This function handles the WM_MEASUREITEM message.
 */
void FASTCALL MsgViewWnd_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   HFONT hOldFont;
   BITMAP bmp;
   HDC hdc;

   hdc = GetDC( hwnd );
   hOldFont = SelectFont( hdc, hThreadBFont );
   GetTextMetrics( hdc, &tm );
   GetObject( hbmpThreadBitmaps, sizeof( BITMAP ), &bmp );
   lpMeasureItem->itemHeight = max( tm.tmHeight, bmp.bmHeight );
   SelectFont( hdc, hOldFont );
   ReleaseDC( hwnd, hdc );
}

/* This function handles the WM_COMPAREITEM message
 */
int FASTCALL MsgViewWnd_OnCompareItem( HWND hwnd, const COMPAREITEMSTRUCT FAR * lpCompareItem )
{
   LPWINDINFO lpWindInfo;
   PTH pth1;
   PTH pth2;

   pth1 = (PTH)lpCompareItem->itemData1;
   pth2 = (PTH)lpCompareItem->itemData2;
   lpWindInfo = GetBlock( hwnd );
   switch( lpWindInfo->vd.nViewMode )
      {
      case VM_VIEWREF:
      case VM_VIEWROOTS:
         return( -1 );

      case VM_VIEWCHRON: {
         MSGINFO msginfo1;
         MSGINFO msginfo2;
         int nSortDirection;
         int nSort;

         Amdb_GetMsgInfo( pth1, &msginfo1 );
         Amdb_GetMsgInfo( pth2, &msginfo2 );
         nSort = 0;
         switch( lpWindInfo->vd.nSortMode & 0x7F )
            {
            default:
               ASSERT(FALSE);
               break;

            case VSM_MSGNUM:
               nSort = msginfo1.dwMsg < msginfo2.dwMsg ? -1 : 1;
               break;

            case VSM_SENDER:
               nSort = lstrcmpi( msginfo1.szAuthor, msginfo2.szAuthor );
               if (nSort == 0)
               {
                  nSort = msginfo1.dwMsg < msginfo2.dwMsg ? -1 : 1;
               }
               break;

            case VSM_DATETIME:
               if( msginfo1.wDate != msginfo2.wDate )
                  nSort = msginfo1.wDate <= msginfo2.wDate ? -1 : 1;
               else
                  nSort = msginfo1.wTime <= msginfo2.wTime ? -1 : 1;
               break;

            case VSM_MSGSIZE:
               nSort = (msginfo1.dwSize & 0x7FFFFFFF) < (msginfo2.dwSize & 0x7FFFFFFF) ? -1 : 1;
               if (nSort == 0)
               {
                  nSort = msginfo1.dwMsg < msginfo2.dwMsg ? -1 : 1;
               }
               break;

            case VSM_SUBJECT: {
               char * psz1;
               char * psz2;

               psz1 = msginfo1.szTitle;
               psz2 = msginfo2.szTitle;
               if( ( *((DWORD *)psz1) & 0x00DFDFDF ) == 0x1A4552 )
                  psz1 += 3;
               if( ( *((DWORD *)psz2) & 0x00DFDFDF ) == 0x1A4552 )
                  psz2 += 3;
               if( ( *((DWORD *)psz1) & 0x00DFDFDF ) == 0x1A5746 )
                  psz1 += 3;
               if( ( *((DWORD *)psz2) & 0x00DFDFDF ) == 0x1A5746 )
                  psz2 += 3;
               while( *psz1 == ' ' ) ++psz1;
               while( *psz2 == ' ' ) ++psz2;
               nSort = lstrcmpi( psz1, psz2 );
               if (nSort == 0)
               {
                  nSort = msginfo1.dwMsg < msginfo2.dwMsg ? -1 : 1;
               }
               break;
               }
            }
         nSortDirection = (lpWindInfo->vd.nSortMode & VSM_ASCENDING) ? 1 : -1;
         return( nSortDirection * nSort );
         }
      }
   return( 0 );
}

/* This function handles the WM_DRAWITEM message.
 */
void FASTCALL MsgViewWnd_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   switch( lpDrawItem->CtlID )
      {
      case IDD_FOLDERLIST:
         InBasket_OnDrawItem( hwnd, lpDrawItem );
         break;

      case IDD_MSGIDTITLE: {
         LPWINDINFO lpWindInfo;
         char * pszText;
         int mode;
         RECT rc;

         /* First draw the name.
          */      
         rc = lpDrawItem->rcItem;
         InflateRect( &rc, -2, 0 );
         mode = SetBkMode( lpDrawItem->hDC, TRANSPARENT );
         pszText = szHeaderCaption[ lpDrawItem->itemID ];
         DrawText( lpDrawItem->hDC, pszText, -1, &rc, DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_LEFT );

         /* Look at the sort mode for this topic. If it matches the one
          * we're drawing, show the sort icon next to it.
          */
         lpWindInfo = GetBlock( hwnd );
         if( lpDrawItem->itemID == (UINT)( (lpWindInfo->vd.nSortMode & 0x7F) - VSM_MSGNUM ) )
            {
            HBITMAP hbmp;
            SIZE size;

            GetTextExtentPoint( lpDrawItem->hDC, pszText, strlen(pszText), &size );
            rc.left += size.cx + 3;
            rc.top += ( ( rc.bottom - rc.top ) - 10 ) / 2;
            if( lpWindInfo->vd.nSortMode & VSM_ASCENDING )
               {
               hbmp = LoadBitmap( hRscLib, MAKEINTRESOURCE(IDB_SORT_ASCEND) );
               Amuser_DrawBitmap( lpDrawItem->hDC, rc.left, rc.top, 10, 10, hbmp, 0 );
               }
            else
               {
               hbmp = LoadBitmap( hRscLib, MAKEINTRESOURCE(IDB_SORT_DESCEND) );
               Amuser_DrawBitmap( lpDrawItem->hDC, rc.left, rc.top, 10, 10, hbmp, 0 );
               }
            DeleteBitmap( hbmp );
            }
         SetBkMode( lpDrawItem->hDC, mode );
         break;
         }

      case IDD_LIST:
         if( lpDrawItem->itemID != -1 )
            if( lpDrawItem->itemAction == ODA_FOCUS )
               /*DrawFocusRect( lpDrawItem->hDC, &lpDrawItem->rcItem )*/;
            else
               {
               LPWINDINFO lpWindInfo;
               BOOL fViewRefMode;
               PTH pth;
               char sz[ 100 ];
               COLORREF tmpT;
               COLORREF tmpB;
               COLORREF T;
               COLORREF T2;
               COLORREF B;
               HBRUSH hbr;
               HFONT hOldFont;
               MSGINFO msginfo;
               BOOL fDrawnUnRead;
               BYTE wLevel;
               int wLen;
               SIZE size;
               RECT rc;
               WORD wType;

               /* Get the handle of the message we're going to draw.
                */
               pth = (PTH)ListBox_GetItemData( lpDrawItem->hwndItem, lpDrawItem->itemID );
               if( !pth )
                  return;
               rc = lpDrawItem->rcItem;

               /* Get the pointer to the window info structure.
                */
               lpWindInfo = GetBlock( hwnd );

               /* If update pending, abort draw because pth
                * could well be invalid.
                */
               if( lpWindInfo->fUpdatePending )
                  return;

               /* Select the font we'll use in the listbox window.
                */
               Amdb_GetMsgInfo( pth, &msginfo );
               wLevel = 0;
               fViewRefMode = FALSE;
               if( lpWindInfo->vd.nViewMode != VM_VIEWCHRON )
                  {
                  wLevel = msginfo.wLevel;
                  fViewRefMode = TRUE;
                  }
               hOldFont = SelectFont( lpDrawItem->hDC, wLevel > 0 ? hThreadFont : hThreadBFont );

               /* Determine the colours to use in the listbox window.
                */
               if( lpDrawItem->itemState & ODS_SELECTED )
                  {
                  T2 = T = crHighlightText;
                  B = crHighlight;
                  }
               else {
                  T2 = T = crNormalMsg;
                  B = crThreadWnd;
                  }

               /* Lots of initialisation.
                */
               B = GetNearestColor( lpDrawItem->hDC, B );
               hbr = CreateSolidBrush( B );
               if( ( msginfo.dwFlags & MF_PRIORITY ) )
                  T = crPriorityMsg;
               if( msginfo.dwFlags & MF_IGNORE )
                  T = T2 = crIgnoredMsg;
               if( IsLoginAuthor( msginfo.szAuthor ) )
                  T = crLoginAuthor;
               if( msginfo.dwFlags & MF_MARKED )
                  T = crMarkedMsg;
               tmpT = SetTextColor( lpDrawItem->hDC, T );
               tmpB = SetBkColor( lpDrawItem->hDC, B );

               /* Display the message number and status column.
                */
               rc.left = rc.right = 0;
               if( lpWindInfo->vd.wHeaderFlags & VH_MSGNUM )
                  {
                  int pos;

                  rc.right = STATUS_COLUMN_DEPTH;
                  if( fViewRefMode )
                     rc.right += vdDef.nThreadIndentPixels * wLevel;
                  if( rc.right < STATUS_COLUMN_DEPTH )
                     rc.right = STATUS_COLUMN_DEPTH;
                  FillRect( lpDrawItem->hDC, &rc, hbr );
         
                  /* Draw the message flags bitmaps.
                   */
                  fDrawnUnRead = FALSE;
                  pos = 0;
                  if( msginfo.dwFlags & MF_BEINGRETRIEVED )
                     Amuser_DrawBitmap( lpDrawItem->hDC, rc.left, rc.top, 16, 16, hbmpThreadBitmaps, ABMP_RETRIEVED );
                  else if( msginfo.dwFlags & MF_DELETED )
                     Amuser_DrawBitmap( lpDrawItem->hDC, rc.left, rc.top, 16, 16, hbmpThreadBitmaps, ABMP_DELETE );
                  else if( msginfo.dwFlags & MF_READLOCK )
                     Amuser_DrawBitmap( lpDrawItem->hDC, rc.left, rc.top, 16, 16, hbmpThreadBitmaps, ABMP_HOLD );
                  else if( msginfo.dwFlags & MF_KEEP )
                     Amuser_DrawBitmap( lpDrawItem->hDC, rc.left, rc.top, 16, 16, hbmpThreadBitmaps, ABMP_KEEP );
                  else if( !( msginfo.dwFlags & MF_READ ) )
                     {
                     Amuser_DrawBitmap( lpDrawItem->hDC, rc.left, rc.top, 16, 16, hbmpThreadBitmaps, ABMP_UNREAD );
                     fDrawnUnRead = TRUE;
                     }
                  else if( msginfo.dwFlags & MF_WITHDRAWN )
                     Amuser_DrawBitmap( lpDrawItem->hDC, rc.left, rc.top, 16, 16, hbmpThreadBitmaps, ABMP_WITHDRAWN );
                  else if( msginfo.dwFlags & MF_WATCH )
                     Amuser_DrawBitmap( lpDrawItem->hDC, rc.left, rc.top, 16, 16, hbmpThreadBitmaps, ABMP_WATCH );
                  else if( msginfo.dwFlags & MF_TAGGED )
                     {
                     Amuser_DrawBitmap( lpDrawItem->hDC, rc.left, rc.top, 16, 16, hbmpThreadBitmaps, ABMP_TAGGED );
                     fDrawnUnRead = TRUE;
                     }
                  pos += 16;

                  /* Show an icon for attachments.
                   */
                  if( Amdb_GetTopicType( lpWindInfo->lpTopic ) != FTYPE_CIX )
                     {
                     if( msginfo.dwFlags & MF_ATTACHMENTS )
                        Amuser_DrawBitmap( lpDrawItem->hDC, rc.left + pos, rc.top, 16, 16, hbmpThreadBitmaps, ABMP_ATTACHMENT );
                     pos += 16;
                     }
         
                  /* Draw an icon showing whether or not the message has a header.
                   */
                  if( Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_NEWS )
                     {
                     if( !( msginfo.dwFlags & MF_HDRONLY ) )
                        Amuser_DrawBitmap( lpDrawItem->hDC, rc.left + pos, rc.top, 16, 16, hbmpThreadBitmaps, ABMP_BODY );
                     pos += 16;
                     }

                  /* For level 0 messages with comments in reference/roots mode, draw the
                   * open/closed folder. For closed threads with unread messages, draw the
                   * unread message bitmap next to it.
                   */
                  if( wLevel == 0 && msginfo.wComments && fViewRefMode )
                     {
                     wType = Amdb_IsExpanded( pth ) ? ABMP_EXPANDED : ABMP_CLOSED;
                     Amuser_DrawBitmap( lpDrawItem->hDC, rc.left + pos, rc.top, 16, 16, hbmpThreadBitmaps, wType );
                     if( wType == ABMP_CLOSED )
                        {
                        if( msginfo.cUnReadInThread && !fDrawnUnRead )
                           Amuser_DrawBitmap( lpDrawItem->hDC, rc.left, rc.top, 16, 16, hbmpThreadBitmaps, ABMP_UNREAD );
                        if( msginfo.cPriority && !( msginfo.dwFlags & MF_MARKED ) )
                           SetTextColor( lpDrawItem->hDC, T = crPriorityMsg );
                        }
                     }
                  pos += 16;

                  /* Set the right hand column.
                   */
                  rc.right = rc.left + pos;
                  if( fViewRefMode )
                     rc.right += vdDef.nThreadIndentPixels * wLevel;

                  /* Draw the thread lines if we've been asked to do so. This code is
                   * optimised to draw them as fast as possible. If you can optimise it
                   * any further, then be my guest. :-)
                   */
                  if( fDrawThreadLines )
                     {
                     HPEN hPen;
                     HPEN hOldPen;
         
                     hPen = CreatePen( PS_SOLID, 1, T2 );
                     hOldPen = SelectPen( lpDrawItem->hDC, hPen );
                     rc.left = pos;
                     Amdb_DrawThreadLines( lpDrawItem->hDC, pth, rc.left, lpWindInfo->vd.nHdrDivisions[0] - 1, rc.top, rc.bottom, vdDef.nThreadIndentPixels );
                     SelectPen( lpDrawItem->hDC, hOldPen );
                     DeletePen( hPen );
                     }

                  /* Draw the message number. For orphan root messages, prefix the
                   * message number with '...' to indicate that its an orphan.
                   */
                  rc.left = rc.right;
                  rc.right = lpWindInfo->vd.nHdrDivisions[0] + 1;
                  if( wLevel == 0 && msginfo.dwComment && fViewRefMode )
                     {
                     SelectFont( lpDrawItem->hDC, hThreadBFont );
                     wsprintf( sz, "...%lu", msginfo.dwMsg );
                     }
                  else
                     wsprintf( sz, "%lu", msginfo.dwMsg );
                  GetTextExtentPoint( lpDrawItem->hDC, sz, strlen( sz ), &size );
                  wLen = size.cx;
                  if( rc.left + wLen > rc.right - (int)vdDef.nThreadIndentPixels )
                     rc.left = ( rc.right - vdDef.nThreadIndentPixels ) - wLen;
                  ExtTextOut( lpDrawItem->hDC, rc.left, rc.top, ETO_OPAQUE|ETO_CLIPPED, &rc, sz, strlen( sz ), NULL );
                  }

               /* Draw the message author name. For outgoing e-mail, show the name
                * prefixed with 'To:'.
                */
               if( lpWindInfo->vd.wHeaderFlags & VH_SENDER )
                  {
                  rc.left = rc.right;
                  rc.right = rc.left + lpWindInfo->vd.nHdrDivisions[1] + 1;
                  sz[ 0 ] = '\0';
                  if( msginfo.dwFlags & MF_OUTBOXMSG )
                     strcpy( sz, "To: " );
                  
                  strncat( sz, msginfo.szAuthor, sizeof( sz ) );
                  ExtTextOut( lpDrawItem->hDC, rc.left + 2, rc.top, ETO_OPAQUE|ETO_CLIPPED, &rc, sz, strlen( sz ), NULL );
                  }

               /* Draw the message date/time if required.
                */
               if( lpWindInfo->vd.wHeaderFlags & VH_DATETIME )
                  {
                  AM_DATE date;
                  AM_TIME time;

                  rc.left = rc.right;
                  rc.right = rc.left + lpWindInfo->vd.nHdrDivisions[2] + 1;
                  Amdate_UnpackDate( msginfo.wDate, &date );
                  Amdate_UnpackTime( msginfo.wTime, &time );
                  Amdate_FormatFullDate( &date, &time, sz, szDateFormat );
                  ExtTextOut( lpDrawItem->hDC, rc.left + 2, rc.top, ETO_OPAQUE|ETO_CLIPPED, &rc, sz, strlen( sz ), NULL );
                  }

               /* Show the message size.
                */
               if( lpWindInfo->vd.wHeaderFlags & VH_MSGSIZE )
                  {
                  rc.left = rc.right;
                  rc.right = rc.left + lpWindInfo->vd.nHdrDivisions[3] - 1;
                  if( msginfo.dwSize & 0x80000000 )
                     wsprintf( sz, "%lu line(s)", ( msginfo.dwSize & 0x7FFFFFFF ) );
                  else
                     wsprintf( sz, "%luK", (msginfo.dwSize + 1023L) / 1024L );
                  ExtTextOut( lpDrawItem->hDC, rc.left + 2, rc.top, ETO_OPAQUE|ETO_CLIPPED, &rc, sz, strlen( sz ), NULL );
                  }

               /* Finally draw the subject information.
                */
               if( lpWindInfo->vd.wHeaderFlags & VH_SUBJECT )
                  {
                  rc.left = rc.right;
                  rc.right = lpDrawItem->rcItem.right;
                  SetTextColor( lpDrawItem->hDC, T );
         
//#ifdef THDDECODE
                  if((strstr(msginfo.szTitle, "=?") && ( Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_NEWS || Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_MAIL )) )
                  {
                     Amdb_GetMailHeaderField( pth, "Subject", lpTmpBuf, 500, TRUE ); // !!SM!!
                     if (*lpTmpBuf)
                     {
                        b64decodePartial(lpTmpBuf); // !!SM!!
                        ExtTextOut( lpDrawItem->hDC, rc.left + 2, rc.top, ETO_OPAQUE|ETO_CLIPPED, &rc, lpTmpBuf, strlen( lpTmpBuf ), NULL ); // !!SM!!
                     }
                     else
                        ExtTextOut( lpDrawItem->hDC, rc.left + 2, rc.top, ETO_OPAQUE|ETO_CLIPPED, &rc, msginfo.szTitle, strlen( msginfo.szTitle ), NULL );
                  }
                  else
//#endif THDDECODE
                     ExtTextOut( lpDrawItem->hDC, rc.left + 2, rc.top, ETO_OPAQUE|ETO_CLIPPED, &rc, msginfo.szTitle, strlen( msginfo.szTitle ), NULL );
                  }

               /* Show the focus rectangle.
                */
               //if( lpDrawItem->itemState & ODS_FOCUS )
               // DrawFocusRect( lpDrawItem->hDC, &lpDrawItem->rcItem );

               /* Clean up afterwards and have a cigarette.
                */
               SelectFont( lpDrawItem->hDC, hOldFont );
               SetTextColor( lpDrawItem->hDC, tmpT );
               SetBkColor( lpDrawItem->hDC, tmpB );
               DeleteBrush( hbr );
               }
         break;
      }
}

/* This function returns TRUE if lpszAuthor matches the
 * CIX nickname or mail source address.
 */
BOOL FASTCALL IsLoginAuthor( char * pszAuthor )
{
   register int c;

   for( c = 0; pszAuthor[ c ] && pszAuthor[ c ] != '@'; ++c )
      if( szCIXNickname[ c ] != pszAuthor[ c ] )
         {
         /* Not CIX nickname, so try mail.
          */
         return( FALSE );
         //return( _strcmpi( pszAuthor, szFullName ) == 0 );
         }
   if (c != 0)
   {
   if( pszAuthor[ c ] == '@' )
      return( IsCompulinkDomain( &pszAuthor[ c + 1 ] ) );
   }
   else if( !fUseCIX )
      return( FALSE );
   return( szCIXNickname[ c ] == '\0' );
}

/* This function handles the WM_MDIACTIVATE message.
 */
void FASTCALL MsgViewWnd_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   if( hwndActivate == hwnd )
      {
      LPWINDINFO lpWindInfo;

      /* Set some handles.
       */
      hwndActive = hwnd;
      hwndTopic = hwnd;
      hwndQuote = hwnd;
      hwndMsg = hwnd;

      /* Invoke the AE_MSGCHANGE event.
       */
      lpWindInfo = GetBlock( hwnd );
      if( lpWindInfo->lpMessage )
         Amuser_CallRegistered( AE_MSGCHANGE, (LPARAM)lpWindInfo->lpMessage, (LPARAM)lpWindInfo->lpTopic );
      Amuser_WriteWindowState( szMsgViewWndName, hwnd );

      /* Update the toolbar to reflect this window state.
       */
      ToolbarState_EditWindow();
      }
   else
      {
      if ( ( GetDlgItem( hwnd, IDD_FOLDERLIST ) == GetFocus()) ||
             GetDlgItem( hwnd, IDD_LIST ) == GetFocus() ||
             GetDlgItem( hwnd, IDD_MESSAGE ) == GetFocus() )
         fLastFocusPane = GetFocus();
      hwndTopic = NULL;
      Amuser_CallRegistered( AE_MSGCHANGE, (LPARAM)-1, 0 );
      }
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, DefMDIChildProc );
}

/* This function adjusts the toolbar buttons depending on the state of
 * the current message window.
 */
void FASTCALL ToolbarState_MessageWindow( void )
{
   LPWINDINFO lpWindInfo;
   TOPICINFO topicinfo;
   BOOL fIsReadWrite;
   BOOL fHasMessage;
   BOOL fMailTopic;
   BOOL fNewsTopic;
   BOOL fMultiSelect;

   /* Initialise.
    */
   fIsReadWrite = FALSE;
   fMailTopic = FALSE;
   fNewsTopic = FALSE;
   fHasMessage = FALSE;
   fMultiSelect = FALSE;

   /* Set a flag if we have a message.
    */
   if( NULL != hwndTopic )
      {
      if( lpWindInfo = GetBlock( hwndTopic ) )
         if( lpWindInfo->lpTopic )
            {
            if( lpWindInfo->lpMessage )
               fHasMessage = TRUE;

            /* Is this a mail topic?
             */
            fMailTopic = Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_MAIL;
            fNewsTopic = Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_NEWS; 
            fMultiSelect = ListBox_GetSelCount( GetDlgItem( hwndTopic, IDD_LIST ) ) > 1; // !!SM!! 2.56.2058
            /* Set a flag if the current topic is
             * read-only.
             */
            Amdb_GetTopicInfo( lpWindInfo->lpTopic, &topicinfo );
            fIsReadWrite = !(topicinfo.dwFlags & TF_READONLY);
            if( !fIsReadWrite && Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_CIX )
               {
               PCL pcl;

               pcl = Amdb_FolderFromTopic( lpWindInfo->lpTopic );
               fIsReadWrite = Amdb_IsModerator( pcl, szCIXNickname );
               }
            }
      }

   /* Now update the toolbar buttons.
    */
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_SORTMAILFROM, fMailTopic && fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_SORTMAILTO, fMailTopic && fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_REPLY, fMailTopic && fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_REPLYTOALL, fMailTopic && fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_ADDTOADDRBOOK, fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_READMSG, fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_KEEPMSG, fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_READLOCK, fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_MARKMSG, fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_PRIORITY, fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_MARKTAGGED, fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_PREV, TRUE );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_DELETEMSG, fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_DECODE, fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_IGNORE, fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_MARKTHREADREAD, fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_READTHREAD, fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_FORWARD, TRUE );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_FORWARDMSG, fIsReadWrite && fHasMessage && !fMultiSelect );  
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_ORIGINAL, fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_MAPISEND, fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_COPYMSG, fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_SAY, fIsReadWrite );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_COMMENT, fIsReadWrite && fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_POSTARTICLE, fNewsTopic );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_COPYCIXLINK, fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_VIEWINBROWSER, fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_FOLLOWUPARTICLE, fNewsTopic && fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_FOLLOWUPARTICLETOALL, fNewsTopic && fHasMessage );
   SendMessage( hwndToolBar, TB_ENABLEBUTTON, IDM_CANCELARTICLE, fNewsTopic && fHasMessage );
}

/* This function handles the Add To Address Book command.
 */
void FASTCALL CmdAddToAddrbook( HWND hwnd )
{
   LPWINDINFO lpWindInfo;

   lpWindInfo = GetBlock( hwnd );
   if( lpWindInfo->lpMessage )
      {
      MSGINFO msginfo;
      char szFromField[ LEN_MAILADDR + 1 ];
      char szFullName[ LEN_FULLCIXNAME + 1 ];
      char szReplyField[ LEN_MAILADDR + 1 ];

      Amdb_GetMsgInfo( lpWindInfo->lpMessage, &msginfo );
      *szFullName = '\0';
      if( Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_MAIL )
         {
         if( msginfo.dwFlags & MF_OUTBOXMSG )
            Amdb_GetMailHeaderField( lpWindInfo->lpMessage, "To", szFromField, sizeof( szFromField ), FALSE );
         else
            Amdb_GetMailHeaderField( lpWindInfo->lpMessage, "From", szFromField, sizeof( szFromField ), FALSE );
         if( *szFromField )
         {
            char szFromAuthor[ LEN_MAILADDR + 1 ];

            ParseFromField( szFromField, szFromAuthor, szFullName );
            strncpy( msginfo.szAuthor, szFromAuthor, sizeof( msginfo.szAuthor) );
         }
         Amdb_GetMailHeaderField( lpWindInfo->lpMessage, "Reply-To", szReplyField, sizeof( szReplyField ), FALSE );
         if( *szReplyField && !( msginfo.dwFlags & MF_OUTBOXMSG ) )
            {
            ParseFromField( szReplyField, szReplyField, NULL );
            if( _strcmpi( msginfo.szAuthor, szReplyField ) != 0 )
               {
               wsprintf( lpTmpBuf, "Add Reply-To: address (%s)\r\n instead of From: address (%s) ?", szReplyField, msginfo.szAuthor );
               if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION ) == IDYES )
                  Amaddr_AddEntry( hwnd, szReplyField, szFullName );
               else
                  Amaddr_AddEntry( hwnd, msginfo.szAuthor, szFullName );
               }
            else
               Amaddr_AddEntry( hwnd, msginfo.szAuthor, szFullName );
            }
         else
            Amaddr_AddEntry( hwnd, msginfo.szAuthor, szFullName );
         }
      else
         Amaddr_AddEntry( hwnd, msginfo.szAuthor, szFullName );
      }
}

/* This is the subclassed message pane procedure.
 */
LRESULT EXPORT FAR PASCAL MessagePaneProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   LRESULT dwRet;

   dwRet = FALSE;
   switch( msg )
      {
      case WM_MOUSEWHEEL: 
      {
         HWND hwndFocused;
         POINT lp;

         /* Pass on mouse wheel messages to the
          * message pane.
          */
         GetCursorPos(&lp);
         hwndFocused = WindowFromPoint(lp);
         if (IsWindow(hwndFocused) && hwnd != hwndFocused)
            return( SendMessage( hwndFocused, msg, wParam, lParam ) );
         break;
      }
      case WM_KEYUP:
         if( VK_SHIFT == wParam )
            ToolbarState_CopyPaste();
         break;

      case WM_LBUTTONUP:
         ToolbarState_CopyPaste();
         break;

      case WM_MBUTTONUP:
         if( fUseMButton )
            Common_KeyDown( hwnd, VK_RETURN);
         break;

/*    case WM_ERASEBKGND: {
         RECT rc;

         GetClientRect( hwnd, &rc );
         SetBkColor( (HDC)wParam, crMsgWnd );
         ExtTextOut( (HDC)wParam, rc.left, rc.top, ETO_OPAQUE, &rc, "", 0, NULL );
         return( TRUE );
         }
*/
      case WM_SETFOCUS:
         iActiveMode = WIN_MESSAGE;

         break;

      case WM_CONTEXTMENU: // !!SM!! 2.54
      case WM_RBUTTONDOWN: {
            BEC_SELECTION bsel;
            HMENU hPopupMenu;
            HMENU hBackMenu;
            HEDIT hedit;
            POINT pt;
            AEPOPUP aep;
            LPWINDINFO lpWindInfo;
            LPSTR pSelectedText;
            int cchSelectedText;
            int b, id;
            

            if( hwndActive != GetParent( hwnd ) )
               SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)GetParent( hwnd ), 0L );
            
            INITIALISE_PTR(pSelectedText);

            /* Get WINDINFO structure.
             */
            lpWindInfo = GetBlock( GetParent( hwnd ) );

            /* The popup menu we're going to use.
             */
            if( hPopupMenus )
               DestroyMenu( hPopupMenus );
            
            hPopupMenus = LoadMenu( hRscLib, MAKEINTRESOURCE( IDMNU_POPUPS ) );
            hPopupMenu = GetPopupMenu( IPOP_MSGWINDOW );

            /* Fill list of backspaced messages
             */
            hBackMenu = GetSubMenu( hPopupMenu, 6 );
            DeleteMenu( hBackMenu, 0, MF_BYPOSITION );
            id = IDM_BACKMENU;
            for( b = nBookPtr - 1; b >= 0; --b )
               AppendMenu( hBackMenu, MF_STRING, id++, cmBookMark[ b ].pszTitle );

            /* If there is any selection, get that.
             */
            hedit = Editmap_Open( hwnd );
            Editmap_GetSelection( hedit, hwnd, &bsel );
            if( bsel.lo != bsel.hi )
               {
//             cchSelectedText = LOWORD( (bsel.hi - bsel.lo) );
               cchSelectedText = (bsel.hi - bsel.lo) + 1; // !!SM!! 2.55.2035
               if( fNewMemory( &pSelectedText, cchSelectedText + 1) )
                  Editmap_GetSel( hedit, hwnd, cchSelectedText, pSelectedText );
               else
                  cchSelectedText = 0;
               }
            else
               {
               cchSelectedText = 0;
               pSelectedText = 0;
               }

            /* Call the AE_POPUP event.
             */
            aep.wType = WIN_MESSAGE;
            aep.pFolder = lpWindInfo->lpTopic;
            aep.pSelectedText = pSelectedText;
            aep.cchSelectedText = cchSelectedText;
            aep.hMenu = hPopupMenu;
            aep.nInsertPos = 3;
            Amuser_CallRegistered( AE_POPUP, (LPARAM)(LPSTR)&aep, 0L );

            /* Free up selected text buffer
             */
            if( NULL != pSelectedText )
               FreeMemory( &pSelectedText );

            /* If there is some selected text, show a different popup menu that
             * has commands that work on the selected text.
             */
            SetFocus( hwnd );
            GetCursorPos( &pt );
            TrackPopupMenu( hPopupMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndFrame, NULL );
            return( TRUE );
            }
         break;

      case WM_KEYDOWN:
         if( hwndTopic )
            {
            /* The F6 key in the message pane switches focus to the thread pane
             * or the list pane (if the thread pane is closed).
             */
            if( wParam == VK_F6 || wParam == VK_TAB )
               {
               LPWINDINFO lpWindInfo;
               HWND hwndParent;
               HWND hwndFocus;

               /* Get WINDINFO structure.
                */
               lpWindInfo = GetBlock( GetParent( hwnd ) );
               hwndParent = GetParent( hwnd );
               hwndFocus = GetDlgItem( hwndParent, IDD_FOLDERLIST );
               if( !IsWindowVisible( hwndFocus ) || 0 == lpWindInfo->nHSplitCur )
                  hwndFocus = GetDlgItem( hwndParent, IDD_LIST );
               SetFocus( hwndFocus );
               return( TRUE );
               }

            /* Handle keys that are common to both the message and the
             * thread pane.
             */
            if( Common_KeyDown( hwnd, wParam ) )
               return( TRUE );
            }
         break;
      }

   /* Eat any actions that would alter the text in the message pane. This
    * is our way of making the message pane read-only.
    */
   if( msg != WM_CHAR && msg != WM_PASTE && msg != WM_CUT && msg != WM_CLEAR )
      dwRet = CallWindowProc( lpProcEditCtl, hwnd, msg, wParam, lParam );
   return( dwRet );
}

/* This is the subclassed thread pane procedure.
 */
LRESULT EXPORT FAR PASCAL ThreadPaneProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   switch( msg )
      {

      case WM_MOUSEWHEEL: 
      {
         HWND hwndFocused;
         POINT lp;

         /* Pass on mouse wheel messages to the
          * message pane.
          */
         GetCursorPos(&lp);
         hwndFocused = WindowFromPoint(lp);
         if (IsWindow(hwndFocused) && hwnd != hwndFocused)
            return( SendMessage( hwndFocused, msg, wParam, lParam ) );
         break;
      }

      case WM_SETFOCUS:
         iActiveMode = WIN_THREAD;
         if ( SendMessage(GetDlgItem( hwnd, IDD_MESSAGE ), SCI_CALLTIPACTIVE, 0, 0) == 1 )
            SendMessage(GetDlgItem( hwnd, IDD_MESSAGE ), SCI_CALLTIPCANCEL, 0, 0);
         break;

      case WM_LBUTTONDOWN: {
         int cyItems;

         /* Any mouse selection clears the keyboard flag so that
          * a message selected with the mouse is NOT autocentered.
          */
         fKeyboard = FALSE;

         /* Dereference the cursor position.
          */
         ptLast.x = (short)LOWORD( lParam );
         ptLast.y = (short)HIWORD( lParam );

         /* Is this item selected? If not, pass onto the default window
          * proc which will select it. If it is selected, initiate the
          * drag procedure.
          */
         cyItems = ListBox_GetItemHeight( hwnd, 0 );
         if( cyItems != LB_ERR && cyItems > 0 )
            {
            int index;
            int count;

            index = ( ptLast.y / cyItems ) + ListBox_GetTopIndex( hwnd );
            count = ListBox_GetCount( hwnd );
            if( index < count && ListBox_GetSel( hwnd, index ) )
               {
               /* Okay, we now start dragging. Load the
                * appropriate cursor for this drag.
                */
               if( fDragMove = ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) ? FALSE : TRUE )
                  if( ListBox_GetSelCount( hwnd ) > 1 )
                     hDragCursor = LoadCursor( hRscLib, MAKEINTRESOURCE(IDC_MULTIPLEDRAG)  );
                  else
                     hDragCursor = LoadCursor( hRscLib, MAKEINTRESOURCE(IDC_SINGLEDRAG) );
               else
                  if( ListBox_GetSelCount( hwnd ) > 1 )
                     hDragCursor = LoadCursor( hRscLib, MAKEINTRESOURCE(IDC_MULTIPLEDRAGCOPY)  );
                  else
                     hDragCursor = LoadCursor( hRscLib, MAKEINTRESOURCE(IDC_SINGLEDRAGCOPY) );
               hNoNoCursor = LoadCursor( hRscLib, MAKEINTRESOURCE(IDC_NONODROP) );
               hCurDragCursor = hDragCursor;
               hOldCursor = GetCursor();
               fDragging = TRUE;
               fInBasket = FALSE;
               fMoved = FALSE;
               tvSelItem = 0L;
               SetCapture( hwnd );
               return( 0L );
               }
            }
         fDragging = FALSE;
         break;
         }

      case WM_ERASEBKGND:
         if( fQuickUpdate )
            return( TRUE );
         break;

      case WM_SETCURSOR:
         if( fDragging )
            {
            /* Just update the cursor while we drag.
             */
            SetCursor( hCurDragCursor );
            return( 0L );
            }
         break;

      case WM_MOUSEMOVE:
         if( fDragging )
         {
            POINT pt;
            HWND hwndIBList;
            HWND hwndFLList;
            HWND hwndPt;

            /* Get the current coordinates and remember it.
             */
            pt.x = (short)LOWORD( lParam );
            pt.y = (short)HIWORD( lParam );
            if( !fMoved )
               if( abs( pt.x - ptLast.x ) < ( GetSystemMetrics( SM_CXDOUBLECLK ) / 2 ) &&
                   abs( pt.y - ptLast.y ) < ( GetSystemMetrics( SM_CYDOUBLECLK ) / 2 ) )
                  return( 0L );

            /* Where is the cursor now? If it's over the
             * thread pane or the in-basket, then fine. Otherwise
             * pop up the no-no cursor.
             */
            ClientToScreen( hwnd, &pt );
            hwndPt = WindowFromPoint( pt );
            hwndIBList = GetDlgItem( hwndInBasket, IDD_FOLDERLIST );
            hwndFLList = GetDlgItem( GetParent( hwnd ), IDD_FOLDERLIST );
            if( hwndPt == hwnd )
                  hCurDragCursor = hDragCursor;
            else if( hwndPt == hwndIBList || hwndPt == hwndFLList )
               {
               TV_HITTESTINFO tvhi;
               HTREEITEM hItem;
               TV_ITEM tvDest; // 2.56.2052 FS#150

               /* Find the tree item under the cursor.
                */
               tvhi.pt = pt;
               MapWindowPoints( NULL, hwndPt, &tvhi.pt, 1 );
               if( hItem = TreeView_HitTest( hwndPt, &tvhi ) )
               {
                  if( 0L == tvSelItem )
                     tvSelItem = TreeView_GetSelection( hwndPt );
                  if( TreeView_GetSelection( hwndPt ) != hItem )
                     TreeView_SelectDropTarget( hwndPt, hItem );

                  // 2.56.2052 FS#150
                  tvDest.hItem = hItem;
                  tvDest.mask = TVIF_PARAM;
                  VERIFY( TreeView_GetItem( hwndPt, &tvDest ) );                    
                  
                  if( Amdb_IsTopicPtr( (PTL)tvDest.lParam ) )
                  {
                     PCAT pcat;
                     PCL pcl;
                     
                     pcl = Amdb_FolderFromTopic( (PTL)tvDest.lParam );
                     pcat = Amdb_CategoryFromFolder( pcl );
                  

                     if( (Amdb_GetTopicType( (PTL)tvDest.lParam ) == FTYPE_CIX) && !Amdb_IsModerator( pcl, szCIXNickname ) )
                     {
                        if( ListBox_GetSelCount( hwnd ) > 1 )
                           hDragCursor = LoadCursor( hRscLib, MAKEINTRESOURCE(IDC_MULTIPLEDRAGCOPY)  );
                        else
                           hDragCursor = LoadCursor( hRscLib, MAKEINTRESOURCE(IDC_SINGLEDRAGCOPY) );
                     }

                     hCurDragCursor = hDragCursor;
                  }
                  else if( Amdb_IsFolderPtr( (PCL)tvDest.lParam ) )
                  {
                     hCurDragCursor = hNoNoCursor;
                  }              
                  // 2.56.2052 FS#150

               }
               else if( tvhi.flags & (TVHT_TOLEFT|TVHT_TORIGHT|TVHT_ABOVE|TVHT_BELOW) )
               {
                  if( 0L == tvSelItem )
                     tvSelItem = TreeView_GetSelection( hwndPt );
                  else
                  {
                     if( TreeView_GetSelection( hwndPt ) != tvSelItem )
                        TreeView_SelectDropTarget( hwndPt, tvSelItem );
                  }
                  hCurDragCursor = hDragCursor;  // 2.56.2052 FS#150
               }
               fInBasket = hwndPt == hwndIBList;
               }
            else if( IsEditWindow( hwndPt ) )
                  hCurDragCursor = hDragCursor;
            else
               {
               if( 0L == tvSelItem )
                  tvSelItem = TreeView_GetSelection( hwndPt );
               else
               {
                  HWND hwndLoc;

                  hwndLoc = fInBasket ? hwndIBList : hwndFLList;
                  if( TreeView_GetSelection( hwndLoc ) != tvSelItem )
                     TreeView_SelectDropTarget( hwndLoc, tvSelItem );
               }
                  hCurDragCursor = hNoNoCursor;
               }
            SetCursor( hCurDragCursor );
            if( !fMoved )
               OfflineStatusText( fDragMove ? GS(IDS_STR815) : GS(IDS_STR816) );
            ptLast = pt;
            fMoved = TRUE;
            return( 0L );
            }
         break;

      case WM_CANCELMODE:
      case WM_LBUTTONUP:
         if( fDragging )
            {
            POINT pt;

            /* First of all, switch off all the drag stuff so we're
             * clean if we have to early-exit this code.
             */
            SetCursor( hOldCursor );
            DestroyCursor( hDragCursor );
            DestroyCursor( hNoNoCursor );
            fDragging = FALSE;
            ReleaseCapture();

            /* Did we even move? If not, make this a button down/up
             * sequence and select the item under the cursor.
             */
            if( !fMoved )
               {
               CallWindowProc( lpProcListCtl, hwnd, WM_LBUTTONDOWN, wParam, lParam );
               CallWindowProc( lpProcListCtl, hwnd, WM_LBUTTONUP, wParam, lParam );
               }
            else
               {
               HWND hwndIBList;
               HWND hwndFLList;
               HWND hwndPt;

               /* Now where did we drop? If it wasn't the
                * in-basket, then we're not interested. Otherwise post
                * a WM_DRAGDROP message to the inbasket telling it to
                * get on with the job of copying the messages.
                */
               ShowUnreadMessageCount();
               pt.x = (short)LOWORD( lParam );
               pt.y = (short)HIWORD( lParam );
               ClientToScreen( hwnd, &pt );
               hwndPt = WindowFromPoint( pt );
               hwndIBList = GetDlgItem( hwndInBasket, IDD_FOLDERLIST );
               hwndFLList = GetDlgItem( GetParent( hwnd ), IDD_FOLDERLIST );
               if( hwndPt == hwndIBList || hwndPt == hwndFLList )
                  {
                  DRAGDROPSTRUCT dds;

                  /* Fill in a DRAGDROPSTRUCT structure for the
                   * destination window.
                   */
                  dds.lpData = GetSelectedItemsHandles( GetParent(hwnd) );
                  if( NULL != dds.lpData )
                     {
                     dds.hwndDrop = hwndPt;
                     dds.pt = pt;
                     dds.wDataType = WDD_MESSAGES;
                     MapWindowPoints( NULL, dds.hwndDrop, &dds.pt, 1 );
                     SetWindowRedraw( hwnd, FALSE );
                        SendMessage( GetParent( hwndPt ), WM_DRAGDROP, WDD_MESSAGES, (LPARAM)(LPSTR)&dds );
                     FreeMemory( (LPVOID)&dds.lpData );
                     SetWindowRedraw( hwnd, TRUE );
                     UpdateWindow( hwnd );

                     /* Reselect original folder.
                      */
                     if( 0L != tvSelItem )
                        TreeView_SelectItem( hwndPt, tvSelItem );
                     return( 0L );
                     }
                  }
               else if( IsEditWindow( hwndPt ) )
                  {
                  MSGINFO msginfo;
                  PTH * lpData;
                  char sz[ 80 ];
                  PCL pcl;
                  PTL ptl;
                  PTH pth;

                  /* It's an edit window, so add the URL for the
                   * message at the cursor position.
                   */
                  lpData = GetSelectedItemsHandles( GetParent(hwnd) );;
                  pth = lpData[ 0 ];
                  ptl = Amdb_TopicFromMsg( pth );
                  pcl = Amdb_FolderFromTopic( ptl );
                  Amdb_GetMsgInfo( pth, &msginfo );
                  if( GetAsyncKeyState( VK_SHIFT ) & 0x8000 )
                     wsprintf( sz, "cix:%lu", msginfo.dwMsg );
                  else
                     wsprintf( sz, "cix:%s/%s:%lu", Amdb_GetFolderName( pcl ), Amdb_GetTopicName( ptl ), msginfo.dwMsg );
                  Edit_ReplaceSel( hwndPt, sz );
                  FreeMemory( (LPVOID)&lpData );
                  }
               if( 0L != tvSelItem )
                  TreeView_SelectItem( hwndPt, tvSelItem );
               }
            return( 0L );
            }
         break;

      case WM_CONTEXTMENU: // !!SM!! 2.54
      case WM_RBUTTONDOWN: {
         LPWINDINFO lpWindInfo;
         int index;
         PTH pth;
         AEPOPUP aep;


         /* Convert this message to a left button up/down so that we actually
          * select the item at the cursor.
          */

         fKeyboard = FALSE;
         if( GetParent( hwnd ) != hwndActive )
            SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)GetParent( hwnd ), 0L );
//          break;

         if( WM_CONTEXTMENU != msg) // !!SM!! 2.55 - don't do the simulated mouse click if it's a keyboard menu
         {
            /* Select the item under the cursor only if nothing is
             * currently selected.
             */
            index = ItemFromPoint( hwnd, (short)LOWORD(lParam), (short)HIWORD(lParam) );
            if( ListBox_GetSel( hwnd, index ) == 0 )
            {
               CallWindowProc( lpProcListCtl, hwnd, WM_LBUTTONDOWN, wParam, lParam );
               CallWindowProc( lpProcListCtl, hwnd, WM_LBUTTONUP, wParam, lParam );
            }
         }
         /* If we have a message, construct a popup menu with commands appropriate
          * to the selected message.
          */
         lpWindInfo = GetBlock( GetParent( hwnd ) );
         if( ( pth = lpWindInfo->lpMessage ) && hwndActive == GetParent( hwnd ) )
            {
            HMENU hPopupMenu;
            POINT pt;

            hPopupMenu = GetPopupMenu( IPOP_THDWINDOW );
            MenuString( hPopupMenu, IDM_READMSG, Amdb_IsRead( pth ) ? GS(IDS_STR368) : GS(IDS_STR369) );
            MenuString( hPopupMenu, IDM_KEEPMSG, Amdb_IsKept( pth ) ? GS(IDS_STR386) : GS(IDS_STR385) );
            MenuString( hPopupMenu, IDM_READLOCK, Amdb_IsReadLock( pth ) ? GS(IDS_STR392) : GS(IDS_STR391) );
            MenuString( hPopupMenu, IDM_MARKMSG, Amdb_IsMarked( pth ) ? GS(IDS_STR384) : GS(IDS_STR383) );
            MenuString( hPopupMenu, IDM_DELETEMSG, Amdb_IsDeleted( pth ) ? GS(IDS_STR388) : GS(IDS_STR387) );
            MenuString( hPopupMenu, IDM_IGNORE, Amdb_IsIgnored( pth ) ? GS(IDS_STR390) : GS(IDS_STR389) );
            MenuString( hPopupMenu, IDM_PRIORITY, Amdb_IsPriority( pth ) ? GS(IDS_STR448) : GS(IDS_STR447) );

            if( Amdb_GetTopicType( Amdb_TopicFromMsg ( pth ) ) == FTYPE_NEWS || Amdb_GetTopicType( Amdb_TopicFromMsg ( pth ) ) == FTYPE_CIX )
               InsertMenu( hPopupMenu, IDM_IGNORE, MF_BYCOMMAND, IDM_WITHDRAW, "&Withdraw" );

            if( Amdb_GetTopicType( Amdb_TopicFromMsg ( pth ) ) == FTYPE_MAIL )
                  {
                  InsertMenu( hPopupMenu, IDM_COPYMSG, MF_BYCOMMAND, IDM_REPLYTOALL, "Reply to All..." );
                  MenuString( hPopupMenu, IDM_SAY, "New Message..." );
                  MenuString( hPopupMenu, IDM_COMMENT, "Reply..." );
                  }
            if( Amdb_GetTopicType( Amdb_TopicFromMsg ( pth ) ) == FTYPE_NEWS )
                  {
                  InsertMenu( hPopupMenu, IDM_COPYMSG, MF_BYCOMMAND, IDM_FOLLOWUPARTICLETOALL, "Follow Up To All..." );
                  InsertMenu( hPopupMenu, IDM_IGNORE, MF_BYCOMMAND, IDM_WATCH, "Watch" );
                  InsertMenu( hPopupMenu, IDM_IGNORE, MF_BYCOMMAND, IDM_MARKTAGGED, "&Tag" );
                  MenuString( hPopupMenu, IDM_SAY, "Post Article..." );
                  MenuString( hPopupMenu, IDM_COMMENT, "Follow Up Article..." );
                  }

            /* Call the AE_POPUP event.
             */
            aep.wType = WIN_THREAD;
            aep.pFolder = lpWindInfo->lpTopic;
            aep.pSelectedText = NULL;
            aep.cchSelectedText = 0;
            aep.hMenu = hPopupMenu;
            aep.nInsertPos = 3;
            Amuser_CallRegistered( AE_POPUP, (LPARAM)(LPSTR)&aep, 0L );

            /* Pop up and track the menu.
             */
            GetCursorPos( &pt );
            TrackPopupMenu( hPopupMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndFrame, NULL );
            }
         break;
         }

      case WM_LBUTTONDBLCLK: {
         LPWINDINFO lpWindInfo;

         /* Left button double-click toggles between expanding and
          * shrinking a thread.
          */
         fKeyboard = FALSE;
         lpWindInfo = GetBlock( GetParent( hwnd ) );
         if( lpWindInfo->vd.nViewMode != VM_VIEWCHRON )
            SendCommand( GetParent( hwnd ), IDM_TOGGLETHREAD, 1 );
         break;
         }

      case WM_MBUTTONUP:
         if( fUseMButton )
            Common_KeyDown( hwnd, VK_RETURN);
         break;

      case WM_KEYDOWN:
         if( fLaptopKeys )
            {
            HWND hwndEdit;
   
            /* The Page Up key in the thread pane scrolls the message
             * up one page.
             */
            hwndEdit = GetDlgItem( GetParent( hwnd ), IDD_MESSAGE );
            if( wParam == VK_PRIOR )
               {
               if( !( GetKeyState( VK_CONTROL ) & 0x8000 ) )
                  {
                  SendMessage( hwndEdit, WM_VSCROLL, SB_PAGEUP, 0L );
                  return( TRUE );
                  }
               break;
               }
   
            /* The Page Down key in the thread pane scrolls the message
             * down one page.
             */
            if( wParam == VK_NEXT )
               {
               if( !( GetKeyState( VK_CONTROL ) & 0x8000 ) )
                  {
                  SendMessage( hwndEdit, WM_VSCROLL, SB_PAGEDOWN, 0L );
                  return( TRUE );
                  }
               break;
               }
            }

         /* Handle the Control and arrow keys which scroll the thread
          * pane list up or down one line or page without changing the
          * current selection.
          */
         if( GetKeyState( VK_CONTROL ) & 0x8000 )
            if( fDragging )
            {              
               if( fDragMove = ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) ? FALSE : TRUE )
                  if( ListBox_GetSelCount( hwnd ) > 1 )
                     hDragCursor = LoadCursor( hRscLib, MAKEINTRESOURCE(IDC_MULTIPLEDRAG)  );
                  else
                     hDragCursor = LoadCursor( hRscLib, MAKEINTRESOURCE(IDC_SINGLEDRAG) );
               else
                  if( ListBox_GetSelCount( hwnd ) > 1 )
                     hDragCursor = LoadCursor( hRscLib, MAKEINTRESOURCE(IDC_MULTIPLEDRAGCOPY)  );
                  else
                     hDragCursor = LoadCursor( hRscLib, MAKEINTRESOURCE(IDC_SINGLEDRAGCOPY) );
               hNoNoCursor = LoadCursor( hRscLib, MAKEINTRESOURCE(IDC_NONODROP) );
               hCurDragCursor = hDragCursor;
               SetCursor( hCurDragCursor );
            }
            else
            {
            HWND hwndEdit;

            hwndEdit = GetDlgItem( GetParent( hwnd ), IDD_MESSAGE );
            if( wParam == VK_UP ) {
               SendMessage( hwndEdit, WM_VSCROLL, SB_LINEUP, 0L );
               return( TRUE );
               }
            if( wParam == VK_DOWN ) {
               SendMessage( hwndEdit, WM_VSCROLL, SB_LINEDOWN, 0L );
               return( TRUE );
               }
            if( wParam == VK_PRIOR ) {
               SendMessage( hwndEdit, WM_VSCROLL, SB_PAGEUP, 0L );
               return( TRUE );
               }
            if( wParam == VK_NEXT ) {
               SendMessage( hwndEdit, WM_VSCROLL, SB_PAGEDOWN, 0L );
               return( TRUE );
               }
            }

         /* The listbox will handle the up key, but we set a
          * flag to indicate that the listbox LBN_SELCHANGE handler
          * must re-center the selected message.
          */
         if( wParam == VK_UP )
            {
            fKeyboard = TRUE;
            fMotionUp = TRUE;
            break;
            }

         /* The listbox will handle the down key, but we set a
          * flag to indicate that the listbox LBN_SELCHANGE handler
          * must re-center the selected message.
          */
         if( wParam == VK_DOWN )
            {
            fKeyboard = TRUE;
            fMotionUp = FALSE;
            break;
            }

         /* ESC cancels dragging
          */
         if( wParam == VK_ESCAPE && fDragging )
            SendMessage( hwnd, WM_CANCELMODE, 0, 0L );

         /* F6 in the thread pane. switches to the message pane.
          */
         if( wParam == VK_F6 || wParam == VK_TAB )
            {
            SetFocus( GetDlgItem( GetParent( hwnd ), IDD_MESSAGE ) );
            return( TRUE );
            }

         /* Handle keys that are common to both the message and the
          * thread pane.
          */
         if( Common_KeyDown( hwnd, wParam ) )
            return( TRUE );
         break;

         case WM_KEYUP:
         if( fDragging )
            {              
               if( fDragMove = ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) ? FALSE : TRUE )
                  if( ListBox_GetSelCount( hwnd ) > 1 )
                     hDragCursor = LoadCursor( hRscLib, MAKEINTRESOURCE(IDC_MULTIPLEDRAG)  );
                  else
                     hDragCursor = LoadCursor( hRscLib, MAKEINTRESOURCE(IDC_SINGLEDRAG) );
               else
                  if( ListBox_GetSelCount( hwnd ) > 1 )
                     hDragCursor = LoadCursor( hRscLib, MAKEINTRESOURCE(IDC_MULTIPLEDRAGCOPY)  );
                  else
                     hDragCursor = LoadCursor( hRscLib, MAKEINTRESOURCE(IDC_SINGLEDRAGCOPY) );
               hNoNoCursor = LoadCursor( hRscLib, MAKEINTRESOURCE(IDC_NONODROP) );
               hCurDragCursor = hDragCursor;
               SetCursor( hCurDragCursor );

            }
         break;

      }
   return( CallWindowProc( lpProcListCtl, hwnd, msg, wParam, lParam ) );
}

/* This function handles keys that are common to both the thread
 * and the message panes.
 */
BOOL FASTCALL Common_KeyDown( HWND hwnd, WPARAM id )
{
   switch( id )
      {
      case VK_RETURN: {
         PTH pth;
         LPWINDINFO lpWindInfo;

         lpWindInfo = GetBlock( GetParent( hwnd ) );
         pth = lpWindInfo->lpMessage;
         SendCommand( GetParent( hwnd ), IDM_NEXT, 1 );
         return( TRUE );
         }
         
      case VK_SPACE: {
         PTH pth;
         LPWINDINFO lpWindInfo;

         lpWindInfo = GetBlock( GetParent( hwnd ) );
         pth = lpWindInfo->lpMessage;
         if( lpWindInfo->vd.nViewMode == VM_VIEWROOTS && pth && Amdb_IsRootMessage( pth ) )
            SendCommand( GetParent( hwnd ), IDM_MARKTHREADREAD, 1 );
         else
            SendCommand( GetParent( hwnd ), IDM_NEXTALL, 1 );
         return( TRUE );
         }
      }
   return( FALSE );
}

/* This function shows the specified topic in the current
 * message window. The nTopIndex parameter is the index of the
 * message to be shown at the top of the thread list.
 */
void FASTCALL SetMsgViewerWindow( HWND hwnd, PTL ptl, int nTopIndex )
{
   LPWINDINFO lpWindInfo;
   TOPICINFO topicinfo;
   HWND hwndList;
   HWND hwndHdr;
   PTL ptlTmp;

   /* Make sure the message viewer window is full-sized.
    */
   if( IsIconic( hwnd ) )
      SendMessage( hwndMDIClient, WM_MDIRESTORE, (WPARAM)hwnd, 0L );

   /* First handle message notifications
    */
   lpWindInfo = GetBlock( hwnd );
   if( NULL != lpWindInfo->lpMessage )
      HandleNotification( hwnd, lpWindInfo->lpMessage );

   /* If the message viewer window is showing a different topic, unlock it.
    */
   if( ( ptlTmp = lpWindInfo->lpTopic ) && ptl != ptlTmp )
      {
      Amdb_InternalUnlockTopic( ptlTmp );
      fInitialDisplay = TRUE;
      }
   lpWindInfo->lpTopic = ptl;
   lpWindInfo->lpMessage = NULL;
   if( ptl != ptlTmp )
      Amdb_LockTopic( ptl );
   SendMessage( hwnd, WM_SETTOPIC, 0, (LPARAM)ptl );
   if( hwndInBasket )
      SendMessage( hwndInBasket, WM_SETTOPIC, 0, (LPARAM)ptl );
   ToolbarState_TopicSelected();

   /* Set the topic header positions.
    */
   Amdb_GetTopicInfo( ptl, &topicinfo );
   lpWindInfo->vd = topicinfo.vd;

   /* Hide or show each column as appropriate.
    */
   hwndHdr = GetDlgItem( hwnd, IDD_MSGIDTITLE );
   SetWindowRedraw( hwndHdr, FALSE );
   ShowHeaderColumns( hwnd );
   SetWindowRedraw( hwndHdr, TRUE );
   InvalidateRect( hwndHdr, NULL, FALSE );

   /* Fill the thread window with the messages in this topic.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   ListBox_ResetContent( hwndList );
   if( ptl )
      {
      HCURSOR hOldCursor;

      SetWindowRedraw( hwndList, FALSE );
      hOldCursor = SetCursor( hWaitCursor );
      Amdb_SetTopicSel( ptl, hwnd );

      /* Call a special API function in AMDB to load the list
       * box for us much faster than if we did it here.
       */
      Amdb_FastRealFastLoadListBox( hwndList, ptl, lpWindInfo->vd.nViewMode, fInitialDisplay );

      /* Ensure that threading is applied
       */
      SetCursor( hOldCursor );
      ListBox_SetTopIndex( hwndList, nTopIndex );
      SetMessageWindowTitle( hwnd, ptl );
      UpdateWindow( hwnd );
      }
   fInitialDisplay = FALSE;
}

/* This function displays the name of the folder and topic in the
 * message window.
 */
void FASTCALL SetMessageWindowTitle( HWND hwnd, PTL ptl )
{
   PCL pcl;
   TOPICINFO topicinfo;

   pcl = Amdb_FolderFromTopic( ptl );
   Amdb_GetTopicInfo( ptl, &topicinfo );
   wsprintf( lpTmpBuf, "%s/%s", Amdb_GetFolderName( pcl ), (LPSTR)topicinfo.szTopicName );
   if( Amdb_GetTopicType( ptl ) == FTYPE_LOCAL || (topicinfo.dwFlags & (TF_HASFILES|TF_READONLY ) ) )
      {
      strcat( lpTmpBuf, " (" );
      if( Amdb_GetTopicType( ptl ) == FTYPE_LOCAL )
         strcat( lpTmpBuf, "L" );
      if( topicinfo.dwFlags & TF_HASFILES )
         strcat( lpTmpBuf, "F" );
      if( topicinfo.dwFlags & TF_READONLY )
         strcat( lpTmpBuf, "R" );
      strcat( lpTmpBuf, ")" );
      }
   if( topicinfo.dwFlags & TF_TOPICFULL )
      strcat( lpTmpBuf, str25 );
   SetWindowText( hwnd, lpTmpBuf );
}

BOOL FASTCALL ShouldDecode(LPSTR msg) {
	LPSTR line = msg;
	char* needle = "Content-Type: multipart";
	size_t len = strlen(needle);

	while (_strnicmp(line, needle, len) != 0) {
		// Reached the end of the headers without finding content-type
		if (strncmp(line, "\r\n", 2) == 0) {
			return FALSE;
		}

		line = _fstrstr(line, "\r\n");
		if (line == NULL) {
			return FALSE;
		}
		line += 2;
	}

	return TRUE;
}


/* This function shows the specified message in the message window.
 */
void FASTCALL ShowMsg( HWND hwnd, PTH pth, BOOL fSetSel, BOOL fForcedShow, BOOL fCenter )
{
   LPWINDINFO lpWindInfo;
   LPSTR lpszPostText;
   LPSTR lpszPreText;
   HWND hwndInfoBar;
   HWND hwndTreeCtl;
   HWND hwndEdit;
   HWND hwndList;
   char sz[ 80 ];
   HPSTR lpText;
   LPSTR lpszMailHdrs;
   HDC hdc;
   DWORD dwMsgLength = 0;
   HWND hwndBill = NULL;
   BOOL shouldDecode = FALSE;
   

   if ( pth )
      {
      dwMsgLength = Amdb_GetMsgTextLength( pth );
      if( dwMsgLength > MAX_DISPLAY_MSG_SIZE )
         OfflineStatusText( "Displaying message, please wait..." );
      if( dwMsgLength > BIG_MAX_DISPLAY_MSG_SIZE )
         hwndBill = Adm_Billboard( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_DISPLAYINGMESSAGE ) );
      }
      
   /* Return if message not different and we're now forcing a redisplay
    * of the current message.
    */
   lpWindInfo = GetBlock( hwnd );
   if( pth && lpWindInfo->lpMessage == pth && !fForcedShow )
   {
      if( dwMsgLength > MAX_DISPLAY_MSG_SIZE )
         ShowUnreadMessageCount();
      if( dwMsgLength > BIG_MAX_DISPLAY_MSG_SIZE )
         DestroyWindow( hwndBill );
      return;
   }

   /* Handle notification
    */
   if( NULL != lpWindInfo->lpMessage )
      HandleNotification( hwnd, lpWindInfo->lpMessage );

   /* Set new message handle.
    */
   lpWindInfo->lpMessage = pth;

   /* Update the toolbar.
    */
   ToolbarState_MessageWindow();

   /* If this message has associated attachments, then show the
    * attachments window. Otherwise hide it.
    */
   UpdateAttachmentPane( hwnd, pth );
   lpszMailHdrs = NULL;
   lpszPostText = NULL;
   lpszPreText = NULL;
   hwndEdit = GetDlgItem( hwnd, IDD_MESSAGE );

   if( !pth )
   {
      Editmap_SetWindowText(hwndEdit, ""); // !!SM!! 2.55.2032
//    SetWindowText( hwndEdit, "" );
   }
   else {
      /* If only header is available, then set lpsz to point
       * to a string describing the header attributes,
       */
      if( Amdb_IsHeaderMsg( pth ) )
         {
         if( Amdb_IsMsgUnavailable( pth ) && Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_NEWS )
            lpszPostText = GS(IDS_STR756);
         else if( Amdb_IsBeingRetrieved( pth ) )
            lpszPostText = GS(IDS_STR757);
         else if( Amdb_IsTagged( pth ) )
            lpszPostText = GS(IDS_STR758);
         else
            {
            char szKey[ 32 ];
            WORD wKey;

            /* For an article not retrieved, get the key associated
             * with the Tagged command and explain how to retrieve
             * the article.
             */
            if( !CTree_GetCommandKey( "MarkTagged", &wKey ) )
               lpszPostText = GS(IDS_STR1140);
            else
               {
               DescribeKey( wKey, szKey, sizeof(szKey) );
               wsprintf( sz, GS(IDS_STR759), szKey );
               lpszPostText = sz;
               }
            }
         if( fShortHeaders && fStripHeaders && ( Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_MAIL || Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_NEWS ) )
            lpszPreText = GetShortHeaderText( pth );
         }
      else if( fShortHeaders && fStripHeaders && ( Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_MAIL || Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_NEWS ) )
      {
         lpszPreText = GetShortHeaderText( pth );
      }
      if( lpText = Amdb_GetMsgText( pth ) )
         {
         DWORD dwLength;
         HPSTR lpText2;
         PTL ptl;
         PCL pcl;

         /* Add lpszPreText to start of message and lpszPostText to
          * end of message.
          */
         dwLength = Amdb_GetMsgTextLength( pth );
         ptl = Amdb_TopicFromMsg( pth );
         pcl = Amdb_FolderFromTopic( ptl );
         lpText2 = lpText;
         
         if( fStripHeaders && ( Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_MAIL || Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_NEWS ) )
		 {
			shouldDecode = ShouldDecode(lpText2);
            lpText = GetReadableText(ptl, lpText2);
		 }
		 else
		 {
            lpText = GetTextBody( ptl, lpText2 );
		 }

         if( ( lpszPreText || lpszPostText ) && dwLength )
            {
            UINT cbPostText = 0;
            UINT cbPreText = 0;

            if( lpszPreText )
               cbPreText = strlen(lpszPreText);
            if( lpszPostText )
               cbPostText = strlen(lpszPostText);
            if( fResizeMemory32( &lpText2, dwLength + cbPreText + cbPostText + 1 ) )
               {
               //lpText = GetTextBody( ptl, lpText2 ); 

               if( fStripHeaders && ( Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_MAIL || Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_NEWS ) )
                  lpText = lpText2;
               else
                  lpText = GetTextBody( ptl, lpText2 );

               dwLength -= (DWORD)( lpText - lpText2 );
               if( lpszPreText )
               {
                  hmemmove( lpText + cbPreText, lpText, dwLength + 1 );
                  memcpy( lpText, lpszPreText, cbPreText );
               }
               if( lpszPostText )
                  strcat( lpText, lpszPostText );
               dwLength += cbPreText + cbPostText;
               }
            }
         else
            dwLength -= (DWORD)( lpText - lpText2 );
   
         /* Only tidy up the ends for short messages.
          */
         if( dwLength < 32000 )
            {
            register int c;

            c = lstrlen( lpText );
   
            for( c = lstrlen( lpText ) - 1; c > 0 && ( lpText[ c ] == CR || lpText[ c ] == LF ); --c );
            if( c < 0 )
               lpText = "";
            else
               lpText[ c + 1 ] = '\0';
            }
         Editmap_SetWindowText(hwndEdit, lpText); // !!SM!! 2.55.2032
//       SetWindowText( hwndEdit, lpText );
         Amdb_FreeMsgTextBuffer( lpText2 );
         if( Amdb_GetSetCoverNoteBit( pth, BSB_GETBIT ) )
            {
            Amdb_GetSetCoverNoteBit( pth, BSB_CLRBIT );
            PostCommand( hwnd, IDM_COVERNOTE, 0L );
            }
         }
      else if( Amdb_GetMsgTextLength( pth ) )
      {
         Editmap_SetWindowText(hwndEdit, GS(IDS_STR55)); // !!SM!! 2.55.2032
//       SetWindowText( hwndEdit, GS(IDS_STR55) );
      }
      else
         Editmap_SetWindowText(hwndEdit, ""); // !!SM!! 2.55.2032
//       SetWindowText( hwndEdit, "" );
      if( NULL != lpszMailHdrs )
         FreeMemory( &lpszMailHdrs );
      }

   /* Update the info bar.
    */
   hwndInfoBar = GetDlgItem( hwnd, IDD_STATUS );
   hdc = GetDC( hwndInfoBar );
   RedrawInfoBar( hwndInfoBar, hdc, pth );
   ReleaseDC( hwndInfoBar, hdc );

   /* Set the selection if requested to do so.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   if( pth && fSetSel )
      {
      int index;

      ListBox_SetSel( hwndList, FALSE, -1 );
      if( LB_ERR != ( index = Amdb_GetMsgSel( pth ) ) )
         {
         ListBox_SetSel( hwndList, TRUE, index );
         ListBox_SetCaretIndex( hwndList, index ); // needed for WINE in Linux
         }
      }
   if( fCenter )
      CenterCurrentMsg( hwndTopic );
   SetWindowRedraw( hwndList, TRUE );
   if( fSetSel && hwndTopic == hwndActive )
      SetFocus( GetDlgItem( hwndTopic, IDD_LIST ) );

   /* Update caret
    */
   if( !fForcedShow && ( NULL != hwndTopic ) )
      {
      hwndTreeCtl = GetDlgItem( hwndTopic, IDD_FOLDERLIST );
      ASSERT( hwndTreeCtl );
      if( IsWindowVisible( hwndTreeCtl ) )
         {
         HTREEITEM hTreeItemCur;
         HTREEITEM hTreeItemNeeded;

         hTreeItemNeeded = Amdb_FindTopicItem( hwndTreeCtl, lpWindInfo->lpTopic );
         hTreeItemCur = TreeView_GetSelection( hwndTreeCtl );
         if( hTreeItemCur != hTreeItemNeeded )
            TreeView_Select( hwndTreeCtl, hTreeItemNeeded, TVGN_CARET );
         }
      }        
   if( !fForcedShow || ( dwMsgLength > MAX_DISPLAY_MSG_SIZE ) )
      ShowUnreadMessageCount();
   if( dwMsgLength > BIG_MAX_DISPLAY_MSG_SIZE )
      DestroyWindow( hwndBill );
   Amuser_CallRegistered( AE_MSGCHANGE, (LPARAM)pth, (LPARAM)lpWindInfo->lpTopic );

   if (shouldDecode) {
	   DecodeMessage(hwnd, TRUE);
   }
}

/* This function updates the attachment pane by showing or hiding any
 * attachments depending on the current message.
 */
void FASTCALL UpdateAttachmentPane( HWND hwnd, PTH pth )
{
   BOOL fResize;
   HWND hwndAttach;

   hwndAttach = GetDlgItem( hwnd, IDD_ATTACHMENT );
   fResize = FALSE;
   if( pth )
      {
      if( Amdb_IsMsgHasAttachments( pth ) )
         {
         ATTACHMENT at;
         LVIEW_ITEM lvi;
         PAH pah;

         pah = NULL;
         SendMessage( hwndAttach, LVM_RESETCONTENT, 0, 0L );
         while( pah = Amdb_GetMsgAttachment( pth, pah, &at ) )
            {
               lvi.hIcon = NULL;
//             if( Amfile_QueryFile( at.szPath ) ) /*!!SM!!*/
               {
                  if( FindExecutable( at.szPath, pszAmeolDir, lpPathBuf ) > (HINSTANCE)32 )
                     lvi.hIcon = ExtractIcon( hInst, lpPathBuf, 0 );
                  if( !lvi.hIcon )
                     lvi.hIcon = LoadIcon( NULL, IDI_APPLICATION );
                  lvi.dwData = (DWORD)pah;
                  lvi.lpszText = GetFileBasename( at.szPath );
                  SendMessage( hwndAttach, LVM_ADDICON, 0, (LPARAM)(LPSTR)&lvi );
               }
/*
               else
               {
                  Amdb_DeleteAttachment( pth, &at, FALSE );
                  pah = NULL;
               }
*/
            }
         if ((int)SendMessage( hwndAttach, LVM_GETCOUNT, 0, 0L ) > 0) /*!!SM!!*/
         {
            if( !IsWindowVisible( hwndAttach ) )
               {
               ShowWindow( hwndAttach, SW_SHOW );
               fResize = TRUE;
               }
            else
               {
               InvalidateRect( hwndAttach, NULL, TRUE );
               UpdateWindow( hwndAttach );
               }
         }
         else
            {
               if( IsWindowVisible( hwndAttach ) )
               {
                  ShowWindow( hwndAttach, SW_HIDE );
                  fResize = TRUE;
               }
            }
         }
      else
         {
         if( IsWindowVisible( hwndAttach ) )
            {
            ShowWindow( hwndAttach, SW_HIDE );
            fResize = TRUE;
            }
         }
      }
   else
      {
      if( IsWindowVisible( hwndAttach ) )
         {
         ShowWindow( hwndAttach, SW_HIDE );
         fResize = TRUE;
         }
      }

   /* If we have to resize the message viewer window because we're
    * showing or hiding an attachment window, then send a WM_SIZE message
    * now so that the edit window is the correct size later.
    */
   if( fResize )
      {
      WORD wState;
      RECT rc;

      GetClientRect( hwnd, &rc );
      wState = IsMaximized( hwnd ) ? SIZE_MAXIMIZED : SIZE_RESTORED;
      SendMessage( hwnd, WM_SIZE, wState, MAKELONG( rc.right, rc.bottom ) );
      }
}

/* This function centers the selected message in the middle of
 * the listbox.
 */
void FASTCALL CenterCurrentMsg( HWND hwnd )
{
   HWND hwndList;
   int top;
   RECT rc;
   int index;
   int count;

   hwndList = GetDlgItem( hwnd, IDD_LIST );
   index = ListBox_GetCaretIndex( hwndList );
   top = ListBox_GetTopIndex( hwndList );
   GetClientRect( hwndList, &rc );
   count = ( rc.bottom - rc.top ) / ListBox_GetItemHeight( hwndList, 0 );
   if( index != top + ( count / 2 ) && ( index - ( count / 2 ) ) >= 0 )
      ListBox_SetTopIndex( hwndList, index - ( count / 2 ) );
}

/* This function returns a flag which specifies whether or not the
 * selected message is centered in the listbox.
 */
BOOL FASTCALL IsMsgCentered( HWND hwnd )
{
   HWND hwndList;
   int top;
   RECT rc;
   int index;
   int count;

   hwndList = GetDlgItem( hwnd, IDD_LIST );
   index = ListBox_GetCaretIndex( hwndList );
   top = ListBox_GetTopIndex( hwndList );
   GetClientRect( hwndList, &rc );
   count = ( rc.bottom - rc.top ) / ListBox_GetItemHeight( hwndList, 0 );
   return( ( ( index - top ) - ( count / 2 ) ) == 0 );
}

/* This function finds a message viewer window associated with the
 * specified folder.
 */
HWND FASTCALL FindMsgViewerWindow( HWND hwnd, PCL pcl )
{
   if( !hwnd )
      hwnd = GetWindow( hwndMDIClient, GW_CHILD );
   else
      hwnd = GetWindow( hwnd, GW_HWNDNEXT );
   while( NULL != hwnd )
      {
      char szWndClass[ 40 ];

      GetClassName( hwnd, szWndClass, 40 );
      if( _strcmpi( szWndClass, szMsgViewWndClass ) == 0 )
         {
         LPWINDINFO lpWindInfo;

         if( !pcl )
            break;
         if( lpWindInfo = GetBlock( hwnd ) )
            if( Amdb_FolderFromTopic( lpWindInfo->lpTopic ) == pcl )
               break;
         }
      hwnd = GetWindow( hwnd, GW_HWNDNEXT );
      }
   return( hwnd );
}

/* This function paints the info bar.
 */
void FASTCALL RedrawInfoBar( HWND hwnd, HDC hdc, PTH pth )
{
   char sz[ 200 ];
   RECT rc;
   TEXTMETRIC tm;
   HRGN hrgn;
   HBRUSH hbr;
   HPEN hpenGray;
   HPEN hOldPen;
   HFONT hfontT;
   COLORREF crText;
   int x, y;
   int nAlign;
   register int n, m, c;
   int mode;

   if( pth )
      {
      MSGINFO msginfo;
      char szDate[ 40 ];
//    char szTime[ 40 ];
      AM_DATE date;
      AM_TIME time;

      Amdb_GetMsgInfo( pth, &msginfo );
      if( msginfo.dwFlags & MF_OUTBOXMSG )
         wsprintf( sz, str6, msginfo.dwMsg, (LPSTR)msginfo.szAuthor );
      else
         wsprintf( sz, str7, msginfo.dwMsg, (LPSTR)msginfo.szAuthor );
      Amdate_UnpackDate( msginfo.wDate, &date );
      Amdate_UnpackTime( msginfo.wTime, &time );
      Amdate_FormatShortDate( &date, szDate );
      wsprintf( sz + strlen( sz ), "\x006on \x007" );
      Amdate_FormatFullDate( &date, &time, sz + strlen( sz ), szDateFormat );
      wsprintf( sz + strlen( sz ), "\x006" );
      
//    wsprintf( szTime, "%2.2d:%2.2d:%2.2d", time.iHour, time.iMinute, time.iSecond );
//    wsprintf( sz + strlen( sz ), str8, (LPSTR)szDate, (LPSTR)szTime );
      if( msginfo.dwComment )
         wsprintf( sz + strlen( sz ), str9, msginfo.dwComment );
      if( msginfo.cUnRead == 0 )
         strcat( sz, GS(IDS_STR56) );
      else if( msginfo.cUnRead == 1 )
         strcat( sz, str11 );
      else
         wsprintf( sz + strlen( sz ), str12, msginfo.cUnRead );
      }
   else
      strcpy( sz, GS(IDS_STR57) );

   /* Fill the info bar with the current button colour.
    */
   GetClientRect( hwnd, &rc );
   hbr = CreateSolidBrush( GetSysColor( COLOR_BTNFACE ) );
   FillRect( hdc, &rc, hbr );
   DeleteBrush( hbr );

   /* Make the info bar 3D.
    */
   hOldPen = SelectPen( hdc, GetStockPen( BLACK_PEN ) );
   MoveToEx( hdc, 0, rc.bottom - 1, NULL );
   LineTo( hdc, rc.right, rc.bottom - 1 );
   SelectPen( hdc, GetStockPen( WHITE_PEN ) );
   MoveToEx( hdc, rc.right - 3, 2, NULL );
   LineTo( hdc, 2, 2 );
   LineTo( hdc, 2, rc.bottom - 3 );
   hpenGray = CreatePen( PS_SOLID, 1, GetSysColor( COLOR_BTNSHADOW ) );
   SelectPen( hdc, hpenGray );
   MoveToEx( hdc, 3, rc.bottom - 3, NULL );
   LineTo( hdc, rc.right - 3, rc.bottom - 3 );
   LineTo( hdc, rc.right - 3, 2 );
   SelectPen( hdc, hOldPen );
   DeletePen( hpenGray );

   /* If there are no messages in this topic, then show a warning
    * on the info bar.
    */
   mode = SetBkMode( hdc, TRANSPARENT );
   hfontT = SelectFont( hdc, hHelv8Font );
   GetTextMetrics( hdc, &tm );
   y = rc.bottom - ( ( rc.bottom - rc.top ) - tm.tmHeight ) / 2 - 1;
   x = rc.left + 20;
   hrgn = CreateRectRgn( 3, 3, rc.right - 5, rc.bottom - 3 );
   SelectClipRgn( hdc, hrgn );

   /* Display the text on the info bar.
    */
   nAlign = SetTextAlign( hdc, TA_BOTTOM );
   crText = SetTextColor( hdc, crInfoBar );
   for( n = m = c = 0; sz[ c ]; ++c )
      {
      if( sz[ c ] == '\x007' || sz[ c ] == '\x006' ) {
         if( n ) {
            SIZE size;

            ExtTextOut( hdc, x, y, 0, &rc, sz + m, n, NULL );
            GetTextExtentPoint( hdc, sz + m, n, &size );
            x += size.cx;
            }
         if( sz[ c ] == '\x007' )
            SelectFont( hdc, hSys8Font );
         else
            SelectFont( hdc, hHelv8Font );
         n = -1;
         m = c + 1;
         }
      ++n;
      }
   if( n )
      ExtTextOut( hdc, x, y, 0, &rc, sz + m, n, NULL );
   SetTextColor( hdc, crText );
   SetTextAlign( hdc, nAlign );

   DeleteRgn( hrgn );
   SelectFont( hdc, hfontT );
   SetBkMode( hdc, mode );
}

/* This function retrieves a list of all messages in the
 * current thread.
 */
LPINT FASTCALL GetThread( HWND hwnd )
{
   LPWINDINFO lpWindInfo;
   PTH pth;

   CheckThreadOpen( hwnd );

   lpWindInfo = GetBlock( hwnd );
   if( pth = lpWindInfo->lpMessage )
      if( pth = Amdb_GetRootMsg( pth, TRUE ) )
         {
         MSGINFO msginfo;
         PTH pthStart;
         LPINT lpi;
         int count;

         /* Count the number of messages in the
          * thread.
          */
         count = 1;
         pthStart = pth;
         while( pth = Amdb_GetNextMsg( pth, lpWindInfo->vd.nViewMode/*VM_VIEWREF*/ ) )
            {
            Amdb_GetMsgInfo( pth, &msginfo );
            if( msginfo.wLevel == 0 )
               break;
            ++count;
            }

         /* Allocate space for these message indices.
          */
         INITIALISE_PTR(lpi);
         if( fNewMemory( &lpi, ( count + 1 ) * sizeof( int ) ) )
            {
            register int c;

            /* Loop and fill the array with the message
             * numbers.
             */
            pth = pthStart;
            for( c = 0; c < count; ++c )
               {
               lpi[ c ] = Amdb_GetMsgSel( pth );
               pth = Amdb_GetNextMsg( pth, lpWindInfo->vd.nViewMode/*VM_VIEWREF*/ );
               }
            lpi[ c ] = LB_ERR;
            return( lpi );
            }
         OutOfMemoryError( hwnd, FALSE, FALSE );
         }
   return( NULL );
}

/* This function retrieves a list of selected messages and allocates
 * and returns a pointer to the PTHs corresponding to the messages.
 */
LPINT FASTCALL GetSelectedItems( HWND hwnd )
{
   HWND hwndList;
   int count;

   hwndList = GetDlgItem( hwnd, IDD_LIST );
   if( count = ListBox_GetSelCount( hwndList ) )
      {
      LPINT lpi;

      INITIALISE_PTR(lpi);
      if( fNewMemory( &lpi, ( count + 1 ) * sizeof( int ) ) )
         {
         ListBox_GetSelItems( hwndList, count, lpi );
         lpi[ count ] = LB_ERR;
         return( lpi );
         }
      OutOfMemoryError( hwnd, FALSE, FALSE );
      }
   return( NULL );
}

/* This function retrieves a list of selected messages and allocates
 * and returns a pointer to the PTHs corresponding to the messages.
 */
PTH FAR * FASTCALL GetSelectedItemsHandles( HWND hwnd )
{
   LPINT lpi;

   if( lpi = GetSelectedItems( hwnd ) )
      return( CvtItemsToHandles( hwnd, lpi ) );
   return( NULL );
}

/* This function retrieves a list of selected messages and allocates
 * and returns a pointer to the PTHs corresponding to the messages.
 */
PTH FAR * FASTCALL GetThreadHandles( HWND hwnd )
{
   LPINT lpi;

   if( lpi = GetThread( hwnd ) )
      return( CvtItemsToHandles( hwnd, lpi ) );
   return( NULL );
}

/* This function converts an array of message indexes to their
 * corresponding message handles.
 */
PTH FAR * FASTCALL CvtItemsToHandles( HWND hwnd, LPINT lpi )
{
   PTH FAR * lppth;
   int count;

   INITIALISE_PTR(lppth);
   for( count = 0; lpi[ count ] != LB_ERR; ++count );
   if( fNewMemory( (PTH FAR *)&lppth, ( count + 1 ) * sizeof( PTH ) ) )
      {
      HWND hwndList;
      register int c;

      hwndList = GetDlgItem( hwnd, IDD_LIST );
      for( c = 0; c < count; ++c )
         lppth[ c ] = (PTH)ListBox_GetItemData( hwndList, lpi[ c ] );
      lppth[ c ] = NULL;
      FreeMemory( &lpi );
      return( lppth );
      }
   FreeMemory( &lpi );
   OutOfMemoryError( hwnd, FALSE, FALSE );
   return( NULL );
}

/* This function makes the specified message the current message.
 */
void FASTCALL InternalSetMsgBookmark( BOOL fUnread, BOOL fBack )
{
   if( hwndTopic )
      {
         LPWINDINFO lpWindInfo;
         MSGINFO msginfo;
         BOOKMARK unr;

         lpWindInfo = GetBlock( hwndTopic );
         if( !lpWindInfo->lpMessage )
            return;
         Amdb_GetMsgInfo( lpWindInfo->lpMessage, &msginfo );
         unr.dwMsg = msginfo.dwMsg;
         unr.ptl = lpWindInfo->lpTopic;
         unr.pcl = Amdb_FolderFromTopic( unr.ptl );
         unr.fUnread = fUnread;
         unr.pszTitle = NULL;

         if (strstr(msginfo.szTitle, "=?"))
         {
            Amdb_GetMailHeaderField( lpWindInfo->lpMessage, "Subject", lpTmpBuf, 500, TRUE );
            if( *lpTmpBuf )
            {
               b64decodePartial(lpTmpBuf);
               if( fNewMemory( &unr.pszTitle, strlen(lpTmpBuf) + 1 ) )
                  strcpy( unr.pszTitle, lpTmpBuf );
            }
         }
         else
         {
            if( fNewMemory( &unr.pszTitle, strlen(msginfo.szTitle) + 1 ) )
               strcpy( unr.pszTitle, msginfo.szTitle );
         }
         if( !fBack )
         {
            if( nBookPtr == MAX_BOOKMARKS )
               {
               register int c;

               for( c = 1; c < MAX_BOOKMARKS; ++c )
                  cmBookMark[ c - 1 ] = cmBookMark[ c ];
               --nBookPtr;
               }
            cmBookMark[ nBookPtr++ ] = unr;
         }
      }
}

/* This function handles the notification that the current message
 * has been read.
 */
void FASTCALL HandleNotification( HWND hwnd, PTH pth )
{
   if( Amdb_NeedNotification( pth ) )
      {
      MSGINFO msginfo;
      int nWhen = 0;
      char szThisMsgId[ 256 ];
      char szRR[ 4 ];
      PTL ptl;


      ptl = Amdb_TopicFromMsg( pth);
      Amdb_GetMsgInfo( pth, &msginfo );
      Amdb_GetCookie( ptl, RR_TYPE, szRR, "", 4 );
      if( strlen( szRR ) == 0 || strcmp( szRR, "100" ) == 0 )
         nWhen = fDlgMessageBox( hwnd, 0, IDDLG_NOTIFICATION, msginfo.szAuthor, 60000, IDD_LATER );
      else if( strcmp( szRR, "121" ) == 0 )
         nWhen = IDD_NOW;
      else if( strcmp( szRR, "110" ) == 0 )
         nWhen = IDD_NEVER;
      if( IDD_NOW == nWhen )
         {
         MAILOBJECT mo;
         char sz[ 128 ];
         AM_DATE date;
         HEADER FAR header;
         char szThisMailAddress[ 64 ];
         char szThisMailName[ 64 ];

         /* Create the mail message and pop it into the
          * Out Basket.
          * (Might be worth writing a function that creates the Out
          * Basket message given just a few parameters...)
          */
         InitialiseHeader( &header );
         Amob_New( OBTYPE_MAILMESSAGE, &mo );
         WriteFolderPathname( sz, GetPostmasterFolder() );
         Amob_CreateRefString( &mo.recMailbox, sz );
         Amdb_GetCookie( ptl, MAIL_ADDRESS_COOKIE, szThisMailAddress, szMailAddress, sizeof(szThisMailAddress) );
         Amdb_GetCookie( ptl, MAIL_NAME_COOKIE, szThisMailName, szFullName, sizeof(szThisMailName) );
         wsprintf( lpTmpBuf, "%s (%s)", (LPSTR)szThisMailAddress, (LPSTR)szThisMailName );
         Amob_CreateRefString( &mo.recFrom, lpTmpBuf );
         Amob_CreateRefString( &mo.recTo, msginfo.szAuthor );
         mo.wEncodingScheme = ENCSCH_NONE;
         mo.wFlags = MOF_NOECHO;
         mo.nPriority = MAIL_PRIORITY_NORMAL;
         if( fUseInternet )
            CreateReplyAddress( &mo.recReplyTo, Amdb_TopicFromMsg( pth ) );
         wsprintf( lpTmpBuf, GS(IDS_STR1205), msginfo.szTitle );
         Amob_CreateRefString( &mo.recSubject, lpTmpBuf );
         Amdate_UnpackDate( msginfo.wDate, &date );
         Amdb_GetMailHeaderField( pth, "Message-Id", szThisMsgId, 256, FALSE );
         wsprintf( lpTmpBuf, GS(IDS_STR1204),
                   msginfo.szTitle,
                   Amdate_FormatLongDate( &date, NULL ),
                   szThisMsgId );
         Amob_CreateRefString( &mo.recText, lpTmpBuf );
         ParseReplyNumber( szThisMsgId, &header );
         Amob_CreateRefString( &mo.recReply, header.szReference );
         Amob_Commit( NULL, OBTYPE_MAILMESSAGE, &mo );
         Amob_Delete( OBTYPE_MAILMESSAGE, &mo );
         Amdb_GetSetNotificationBit( pth, BSB_CLRBIT );
         }
      else if( IDD_NEVER == nWhen )
         Amdb_GetSetNotificationBit( pth, BSB_CLRBIT );
      }
}

/* This function makes the specified message the current message.
 */
BOOL FASTCALL InternalSetCurrentMsg( PTL ptl, PTH pth, BOOL fSetSel, BOOL fCenter )
{
   if( hwndTopic )
      {
      TOPICINFO topicinfo;
      LPWINDINFO lpWindInfo;
      BOOL fForcedShow;
      int nTopIndex;
      HWND hwndList;
      BOOL fExpand;
      PTH pth2;

      /* Initialise variables.
       */
      lpWindInfo = GetBlock( hwndTopic );
      hwndList = GetDlgItem( hwndTopic, IDD_LIST );
      nTopIndex = ListBox_GetTopIndex( hwndList );
      pth2 = lpWindInfo->lpMessage;
      fForcedShow = FALSE;
      fExpand = FALSE;
      topicinfo.vd.nViewMode = 0;

      /* Get info about the topic in which the new message
       * appears.
       */
      if( NULL != pth )
         Amdb_GetTopicInfo( Amdb_TopicFromMsg( pth ), &topicinfo );

      /* If we're viewing in roots mode, decide whether or not we have
       * to expand the thread in which the new message appears. We may also
       * have to contract the thread in which the current message appears
       * unless it is in the same thread as the new message.
       */
      if( NULL != pth && topicinfo.vd.nViewMode == VM_VIEWROOTS && pth2 != pth )
         {
         PTL ptl2;

         ptl2 = lpWindInfo->lpTopic;
         Amdb_LockTopic( ptl2 );
         if( ptl != lpWindInfo->lpTopic )
            SetMsgViewerWindow( hwndTopic, ptl, nTopIndex );
         if( pth2 && ( Amdb_GetRootMsg( pth2, TRUE ) != Amdb_GetRootMsg( pth, TRUE ) ) )
            {
            if( Amdb_IsExpanded( pth2 ) )
               {
               Amdb_ExpandThread( pth2, FALSE );
               fExpand = TRUE;
               }
            if( pth && !Amdb_IsRootMessage( pth ) )
               {
               Amdb_ExpandThread( pth, TRUE );
               fExpand = TRUE;
               }
            }
         else if( pth2 && Amdb_IsRootMessage( pth2 ) && !Amdb_IsExpanded( pth2 ) )
            {
            Amdb_ExpandThread( pth2, TRUE );
            fExpand = TRUE;
            }
         else if( pth2 == NULL )
            {
            if( !Amdb_IsExpanded( pth ) )
               {
               Amdb_ExpandThread( pth, TRUE );
               fExpand = TRUE;
               }
            }
         Amdb_UnlockTopic( ptl2 );
         }
      else if( pth && !Amdb_IsExpanded( pth ) )
         {
         Amdb_ExpandThread( pth, TRUE );
         fExpand = TRUE;
         }
      if( ptl != lpWindInfo->lpTopic || fExpand )
         {
         SetMsgViewerWindow( hwndTopic, ptl, nTopIndex );
         fForcedShow = TRUE;
         }
      if( fExpand )
         fSetSel = TRUE;
      ShowMsg( hwndTopic, pth, fSetSel, fForcedShow, fCenter );
      }
   return( hwndTopic != NULL );
}

void FASTCALL ForceFocusToList( HWND hwnd )
{
   if( GetFocus() != GetDlgItem( hwnd, IDD_LIST ) )
      SetFocus( GetDlgItem( hwnd, IDD_LIST ) );
}

void FASTCALL UpdateCurrentMsgDisplay( HWND hwnd )
{
   HWND hwndList;
   int index;
   RECT rect;

   hwndList = GetDlgItem( hwnd, IDD_LIST );
   if( ( index = ListBox_GetCaretIndex( hwndList ) ) != LB_ERR )
      {
      ListBox_GetItemRect( hwndList, index, &rect );
      InvalidateRect( hwndList, &rect, FALSE );
      UpdateWindow( hwndList );
      }
}

void FASTCALL UpdateThreadDisplay( HWND hwnd, BOOL fQuick )
{
   fQuickUpdate = fQuick;
   InvalidateRect( GetDlgItem( hwnd, IDD_LIST ), NULL, FALSE );
   UpdateWindow( GetDlgItem( hwnd, IDD_LIST ) );
   fQuickUpdate = FALSE;
}

void FASTCALL UpdateStatusDisplay( HWND hwnd )
{
   HWND hwndInfoBar;
   PTH pth;
   HDC hdc;
   LPWINDINFO lpWindInfo;

   lpWindInfo = GetBlock( hwnd );
   pth = lpWindInfo->lpMessage;
   hwndInfoBar = GetDlgItem( hwnd, IDD_STATUS );
   hdc = GetDC( hwndInfoBar );
   RedrawInfoBar( hwndInfoBar, hdc, pth );
   ReleaseDC( hwndInfoBar, hdc );
}

/* This function creates a GETARTICLEOBJECT for the specified
 * message and offers to download the article.
 */
void FASTCALL DownloadMsg( HWND hwnd, PTL ptl, DWORD dwMsg )
{
   switch( Amdb_GetTopicType( ptl ) )
      {
      case FTYPE_NEWS: {
         GETARTICLEOBJECT gao;
      
         Amob_New( OBTYPE_GETARTICLE, &gao );
         gao.dwMsg = dwMsg;
         Amob_CreateRefString( &gao.recNewsgroup, (char *)Amdb_GetTopicName( ptl ) );
         if( Amob_Find( OBTYPE_GETARTICLE, &gao ) )
            {
            wsprintf( lpTmpBuf, GS(IDS_STR65), dwMsg );
            fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
            }
         else
            {
            wsprintf( lpTmpBuf, GS(IDS_STR63), dwMsg );
            if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION ) == IDYES )
               Amob_Commit( NULL, OBTYPE_GETARTICLE, &gao );
            }
         Amob_Delete( OBTYPE_GETARTICLE, &gao );
         break;
         }

      case FTYPE_CIX: {
         GETMESSAGEOBJECT gao;
         char sz[ 80 ];
         PCL pcl;

         Amob_New( OBTYPE_GETMESSAGE, &gao );
         gao.dwStartMsg = dwMsg;
         gao.dwEndMsg = dwMsg;
         pcl = Amdb_FolderFromTopic( ptl );
         wsprintf( sz, "%s/%s", Amdb_GetFolderName(pcl), Amdb_GetTopicName(ptl) );
         Amob_CreateRefString( &gao.recTopicName, sz );
         if( Amob_Find( OBTYPE_GETMESSAGE, &gao ) )
            {
            wsprintf( lpTmpBuf, GS(IDS_STR65), dwMsg );
            fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
            }
         else
            {
            wsprintf( lpTmpBuf, GS(IDS_STR63), dwMsg );
            if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION ) == IDYES )
               Amob_Commit( NULL, OBTYPE_GETMESSAGE, &gao );
            }
         Amob_Delete( OBTYPE_GETMESSAGE, &gao );
         break;
         }

      default:
         fMessageBox( hwnd, 0, GS(IDS_STR62), MB_OK|MB_ICONEXCLAMATION );
         break;
      }
}

/* This function marks all messages in the specified topic
 * read.
 */
DWORD FASTCALL MarkTopicRead( HWND hwnd, PTL ptl, BOOL fUpdate )
{
   DWORD dwMarkedRead;

   if( dwMarkedRead = Amdb_MarkTopicRead( ptl ) )
      if( hwndMsg )
         {
         LPWINDINFO lpWindInfo;
   
         lpWindInfo = GetBlock( hwndMsg );
         if( lpWindInfo->lpTopic == ptl )
            {
            UpdateThreadDisplay( hwndMsg, TRUE );
            UpdateStatusDisplay( hwndMsg );
            }
         }
   return( dwMarkedRead );
}

/* This function returns the handles of the active message,
 * topic, folder and category in the CURMSG structure.
 */
BOOL EXPORT WINAPI Ameol2_GetCurrentMsg( CURMSG FAR * lpunr )
{
   HWND hwndTreeCtl;
   LPWINDINFO lpWindInfo;

   if( hwndTopic )
         {
         lpWindInfo = GetBlock( hwndTopic );
         lpunr->pFolder = lpWindInfo->lpTopic;
         lpunr->ptl = lpWindInfo->lpTopic;
         lpunr->pth = lpWindInfo->lpMessage;
         lpunr->pcl = Amdb_FolderFromTopic( lpunr->ptl );
         lpunr->pcat = Amdb_CategoryFromFolder( lpunr->pcl );
         }
   else if( hwndInBasket )
      {
      hwndTreeCtl = GetDlgItem( hwndInBasket, IDD_FOLDERLIST );
      GetTopicFromTreeView( hwndTreeCtl, lpunr );
      }
   else
      {
      lpunr->pFolder = NULL;
      lpunr->ptl = NULL;
      lpunr->pcl = NULL;
      lpunr->pcat = NULL;
      lpunr->pth = NULL;
      }
   return( hwndTopic != NULL );
}

/* This function returns the handles of the active message,
 * topic, folder and category in the CURMSG structure.
 */
BOOL EXPORT WINAPI Ameol2_GetCurrentTopic( CURMSG FAR * lpunr )
{
   HWND hwndTreeCtl;
   LPWINDINFO lpWindInfo;

   /* Initialise structure.
    */
   lpunr->pFolder = NULL;
   lpunr->ptl = NULL;
   lpunr->pcl = NULL;
   lpunr->pcat = NULL;
   lpunr->pth = NULL;

   /* If topic window open, use that.
    */
   if( hwndTopic )
      {
      /* Get the topic info from the folder pane if it's visible, otherwise use
       * the thread pane YH 25/06/96 12:30
       */
      VERIFY( hwndTreeCtl = GetDlgItem( hwndTopic, IDD_FOLDERLIST ) );
      if( IsWindowVisible( hwndTreeCtl ) )
         GetTopicFromTreeView( hwndTreeCtl, lpunr );
      else
         {
         lpWindInfo = GetBlock( hwndTopic );
         lpunr->pFolder = lpWindInfo->lpTopic;
         lpunr->pth = NULL;
         lpunr->ptl = lpWindInfo->lpTopic;
         lpunr->pcl = Amdb_FolderFromTopic( lpunr->ptl );
         lpunr->pcat = Amdb_CategoryFromFolder( lpunr->pcl );
         }
      }
   else if( hwndInBasket )
      {
      hwndTreeCtl = GetDlgItem( hwndInBasket, IDD_FOLDERLIST );
      GetTopicFromTreeView( hwndTreeCtl, lpunr );
      }
   return( hwndTopic != NULL );
}

/* Given an treeview of an inbasket, whether it is from the inbasket window or the folder
 * pane of the message window, this function fill the CURMSG structure with info on
 * which folder/topic is chosen.
 * YH 25/06/96 12:30 
 */
void FASTCALL GetTopicFromTreeView( HWND hwndTreeCtl, CURMSG FAR * lpunr )
{
   HTREEITEM hSelItem;

   if( hSelItem = TreeView_GetSelection( hwndTreeCtl ) )
      {
      TV_ITEM tv;

      /* Get the lParam of the selected item.
       */
      tv.hItem = hSelItem;
      tv.mask = TVIF_PARAM;
      VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );

      /* Fill the CURMSG structure.
       */
      lpunr->pth = NULL;
      if( Amdb_IsCategoryPtr( (PCAT)tv.lParam ) )
         {
         lpunr->ptl = NULL;
         lpunr->pcl = NULL;
         lpunr->pcat = (PCAT)tv.lParam;
         lpunr->pFolder = lpunr->pcat;
         }
      else if( Amdb_IsFolderPtr( (PCL)tv.lParam ) )
         {
         lpunr->ptl = NULL;
         lpunr->pcl = (PCL)tv.lParam;
         lpunr->pcat = Amdb_CategoryFromFolder( lpunr->pcl );
         lpunr->pFolder = lpunr->pcl;
         }
      else
         {
         lpunr->ptl = (PTL)tv.lParam;
         lpunr->pcl = Amdb_FolderFromTopic( lpunr->ptl );
         lpunr->pcat = Amdb_CategoryFromFolder( lpunr->pcl );
         lpunr->pFolder = lpunr->ptl;
         }
      }
}

/* This function removes the bookmark for the specified message or
 * topic from the bookmark list.
 */
void FASTCALL RemoveBookmark( PCL pcl, PTL ptl, PTH pth )
{
   register int c;
   MSGINFO msginfo;

   msginfo.dwMsg = 0;
   if( pth )
      Amdb_GetMsgInfo( pth, &msginfo );
   for( c = 0; c < nBookPtr; ++c )
      if( pcl == cmBookMark[ c ].pcl || ptl == cmBookMark[ c ].ptl || ( pth && msginfo.dwMsg == cmBookMark[ c ].dwMsg ) )
         {
         register int d;

         FreeMemory( &cmBookMark[ c ].pszTitle );
         for( d = c; d < nBookPtr - 1; ++d )
            cmBookMark[ d ] = cmBookMark[ d + 1 ];
         --nBookPtr;
         --c;
         }
}

void FASTCALL RemoveAllBookmarks( void )
{
   while( --nBookPtr >= 0 )
      FreeMemory( &cmBookMark[ nBookPtr ].pszTitle );
   nBookPtr = 0;
}

BOOL FASTCALL SetCurrentMsgEx( CURMSG FAR * lpunr, BOOL fCenter, BOOL fUnread, BOOL fNew )
{
   if( !hwndTopic || fNew )
      OpenMsgViewerWindow( lpunr->ptl, lpunr->pth, fNew );
   else
      {
      InternalSetMsgBookmark( fUnread, FALSE );
      InternalSetCurrentMsg( lpunr->ptl, lpunr->pth, TRUE, fCenter );
      SetFocus( GetDlgItem( hwndTopic, IDD_LIST ) );
      }
   return( TRUE );
}

BOOL WINAPI EXPORT Ameol2_SetCurrentMsg( CURMSG FAR * lpunr )
{
   return( SetCurrentMsg( lpunr, TRUE, TRUE ) );
}

BOOL FASTCALL SetCurrentMsg( CURMSG FAR * lpunr, BOOL fCenter, BOOL fUpdate )
{
   if( !fUpdate )
      {
      LPWINDINFO lpWindInfo;

      if( !hwndTopic )
         return( FALSE );
      lpWindInfo = GetBlock( hwndTopic );
      lpWindInfo->lpMessage = lpunr->pth;
      }
   else
      {
      if( !hwndTopic )
         OpenMsgViewerWindow( lpunr->ptl, lpunr->pth, FALSE );
      else
         {
         InternalSetMsgBookmark( FALSE, FALSE );
         InternalSetCurrentMsg( lpunr->ptl, lpunr->pth, TRUE, fCenter );
         }
      SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndTopic, 0L );
      }
   return( TRUE );
}

/* This is the window procedure for the info bar window.
 */
LRESULT EXPORT CALLBACK InfoBarWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_PAINT, InfoBarWnd_OnPaint );

      default:
         break;
      }
   return( DefWindowProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_PAINT message for the info bar
 * window.
 */
void FASTCALL InfoBarWnd_OnPaint( HWND hwnd )
{
   LPWINDINFO lpWindInfo;
   PAINTSTRUCT ps;
   PTH pth;
   HDC hdc;

   hdc = BeginPaint( hwnd, (LPPAINTSTRUCT)&ps );
   lpWindInfo = GetBlock( GetParent( hwnd ) );
   pth = lpWindInfo->lpMessage;
   RedrawInfoBar( hwnd, hdc, pth );
   EndPaint( hwnd, (LPPAINTSTRUCT)&ps );
}

/* This function handles the Goto dialog box.
 */
BOOL EXPORT CALLBACK Goto( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = Goto_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the Goto
 *  dialog.
 */
LRESULT FASTCALL Goto_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, Goto_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, Goto_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsGOTO );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL Goto_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndEdit;

   SetWindowLong( hwnd, DWL_USER, lParam );
   hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
   //Edit_LimitText( hwndEdit, 11 );    // 2.56.2051 FS#138 
   EnableControl( hwnd, IDOK, FALSE );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL Goto_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   HWND hwndEdit;

   switch( id )
      {
      case IDD_EDIT: {

         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndEdit ) > 0 );
         break;
         }

      case IDOK: {
         // 2.56.2051 FS#138 
         LPSTR lpdwGotoMsg;
/*       DWORD dwGotoMsg;
         DWORD FAR * lpdwGotoMsg;


         if( !GetDlgLongInt( hwnd, IDD_EDIT, &dwGotoMsg, 1, 99999999 ) )
            break;
         if( dwGotoMsg == 0 )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR436), MB_OK|MB_ICONEXCLAMATION );
            break;
            }
         lpdwGotoMsg = (DWORD FAR *)GetWindowLong( hwnd, DWL_USER );
         *lpdwGotoMsg = dwGotoMsg;
*/
         lpdwGotoMsg = (LPSTR)GetWindowLong( hwnd, DWL_USER );
         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         Edit_GetText( hwndEdit, lpdwGotoMsg, 254 );
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

void FASTCALL StripOutName( char * pszSrc, char * pszNameToStrip )
{
   LPSTR pCopy;
   LPSTR pCopy2;

   pCopy = pCopy2 = MakeCompactString( pszSrc );
   *pszSrc = '\0';
   while( *pCopy )
      {
      if( lstrcmpi( pCopy, pszNameToStrip ) )
         {
         if( *pszSrc )
            strcat( pszSrc, " " );
         strcat( pszSrc, pCopy );
         }
      pCopy += lstrlen( pCopy ) + 1;
      }
   FreeMemory( &pCopy2 );
}

/* This function converts the return code from ShellExecute into
 * an error message.
 */
void FASTCALL DisplayShellExecuteError( HWND hwnd, char * pszFilename, UINT uRetCode )
{
   char * pStrError;

   switch( uRetCode )
      {
      case 0:  pStrError = GS(IDS_STR761); break;
      case 2:  pStrError = GS(IDS_STR762); break;
      case 3:  pStrError = GS(IDS_STR763); break;
      case 5:  pStrError = GS(IDS_STR764); break; 
      case 6:  pStrError = GS(IDS_STR765); break;
      case 8:  pStrError = GS(IDS_STR766); break;
      case 10: pStrError = GS(IDS_STR767); break;
      case 11: pStrError = GS(IDS_STR768); break;
      case 12: pStrError = GS(IDS_STR769); break;
      case 13: pStrError = GS(IDS_STR770); break;
      case 14: pStrError = GS(IDS_STR771); break;
      case 15: pStrError = GS(IDS_STR772); break;
      case 16: pStrError = GS(IDS_STR773); break;
      case 19: pStrError = GS(IDS_STR774); break;
      case 20: pStrError = GS(IDS_STR775); break;
      case 21: pStrError = GS(IDS_STR776); break;
      case 31: pStrError = GS(IDS_STR760); break;

      default:
         wsprintf( lpTmpBuf, GS(IDS_STR870), pszFilename, uRetCode );
         fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
         return;
      }
   wsprintf( lpTmpBuf, "%s\n\n%s", pszFilename, pStrError );
   fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
}

/* This function advances automatically to the next message.
 */
void FASTCALL MoveToNextMessage( HWND hwnd )
{
   HWND hwndList;
   CURMSG unr;
   int index;

   hwndList = GetDlgItem( hwnd, IDD_LIST );
   Ameol2_GetCurrentMsg( &unr );
   index = Amdb_GetMsgSel( unr.pth );
   if( index != LB_ERR )
      if( index < ListBox_GetCount( hwndList ) - 1 )
         {
         unr.pth = (PTH)ListBox_GetItemData( hwndList, index + 1 );
         if( unr.pth != NULL )
            {
            SetCurrentMsg( &unr, TRUE, TRUE );
            ForceFocusToList( hwnd );
            }
         }
}

/* This function advances automatically to the next unread message.
 */
BOOL FASTCALL MoveToNextUnreadMessage( HWND hwnd, BOOL fUnread )
{
   HWND hwndList;
   CURMSG unr;
   int index;
   int count;

   hwndList = GetDlgItem( hwnd, IDD_LIST );
   Ameol2_GetCurrentMsg( &unr );
   if( NULL != unr.pth )
      {
      index = Amdb_GetMsgSel( unr.pth );
      if( index != LB_ERR )
         {
         count = ListBox_GetCount( hwndList );
         if( index < ListBox_GetCount( hwndList ) - 1 )
            {
            unr.pth = (PTH)ListBox_GetItemData( hwndList, index + 1 );
            while( unr.pth != NULL && index < count )
               {
               if( !Amdb_IsRead( unr.pth ) )
                  {
                  SetCurrentMsgEx( &unr, TRUE, fUnread, FALSE );
                  ForceFocusToList( hwnd );
                  return( TRUE );
                  }
               unr.pth = (PTH)ListBox_GetItemData( hwndList, ++index );
               }
            }
         }
      }
   return( FALSE );
}

/* This function advances automatically to the start of 
 * the next thread.
 */
void FASTCALL MoveToNextThread( HWND hwnd )
{
   HWND hwndList;
   CURMSG unr;
   int count;
   int index;

   hwndList = GetDlgItem( hwnd, IDD_LIST );
   Ameol2_GetCurrentMsg( &unr );
   index = Amdb_GetMsgSel( unr.pth );
   if( index != LB_ERR )
      {
      count = ListBox_GetCount( hwndList );
      if( index < count - 1 )
         {
         unr.pth = (PTH)ListBox_GetItemData( hwndList, ++index );
         while( unr.pth && index < count )
            {
            MSGINFO msginfo;

            Amdb_GetMsgInfo( unr.pth, &msginfo );
            if( msginfo.wLevel == 0 )
               {
               SetCurrentMsg( &unr, TRUE, TRUE );
               ForceFocusToList( hwnd );
               break;
               }
            unr.pth = (PTH)ListBox_GetItemData( hwndList, ++index );
            }
         }
      }
}

/* This function bookmarks all messages specified by
 * the lpi array of message indexes.
 */
void FASTCALL MarkMsgRange( HWND hwnd, LPINT lpi )
{
   HWND hwndList;
   register int c;
   BOOL fStatus;

   fStatus = FALSE;
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   for( c = 0; lpi[ c ] != LB_ERR; ++c )
      {
      RECT rect;
      PTH pth;

      pth = (PTH)ListBox_GetItemData( hwndList, lpi[ c ] );
      if( 0 == c )
         fStatus = !Amdb_IsMarked( pth );
      Amdb_MarkMsg( pth, fStatus );
      ListBox_GetItemRect( hwndList, lpi[ c ], &rect );
      InvalidateRect( hwndList, &rect, FALSE );
      }
   UpdateWindow( hwndList );
   FreeMemory( &lpi );
   AdvanceAfterMarking( hwnd, IDM_MARKMSG );
}

/* This function tags all messages specified by
 * the lpi array of message indexes.
 */
void FASTCALL MarkTaggedRange( HWND hwnd, LPINT lpi )
{
   LPWINDINFO lpWindInfo;
   HWND hwndList;
   register int c;
   BOOL fMove;
   PTH pth;

   hwndList = GetDlgItem( hwnd, IDD_LIST );
   if( fOnline )
      {
      for( c = 0; lpi[ c ] != LB_ERR; ++c )
         {
         MSGINFO msginfo;
         RECT rect;
   
         pth = (PTH)ListBox_GetItemData( hwndList, lpi[ c ] );
         if( Amdb_IsTagged( pth ) )
            Amdb_MarkMsgTagged( pth, FALSE );
         else
            {
            GETARTICLEOBJECT ga;

            Amdb_GetMsgInfo( pth, &msginfo );
            if( msginfo.dwFlags & MF_BEINGRETRIEVED )
               continue;
            Amob_New( OBTYPE_GETARTICLE, &ga );
            ga.dwMsg = msginfo.dwMsg;
            Amob_CreateRefString( &ga.recNewsgroup, (char *)Amdb_GetTopicName( Amdb_TopicFromMsg( pth ) ) );
            if( !Amob_Find( OBTYPE_GETARTICLE, &ga ) )
               Amob_Commit( NULL, OBTYPE_GETARTICLE, &ga );
            Amob_Delete( OBTYPE_GETARTICLE, &ga );
            Amdb_MarkMsgRead( pth, TRUE );
            }
         ListBox_GetItemRect( hwndList, lpi[ c ], &rect );
         InvalidateRect( hwndList, &rect, FALSE );
         }
      fMove = c == 1;
      }
   else
      {
      for( c = 0; lpi[ c ] != LB_ERR; ++c )
         {
         RECT rect;

         pth = (PTH)ListBox_GetItemData( hwndList, lpi[ c ] );
         Amdb_MarkMsgRead( pth, TRUE );
         Amdb_MarkMsgBeingRetrieved( pth, FALSE );
         Amdb_MarkMsgTagged( pth, !Amdb_IsTagged( pth ) );
         ListBox_GetItemRect( hwndList, lpi[ c ], &rect );
         InvalidateRect( hwndList, &rect, FALSE );
         }
      fMove = TRUE;
      }
   UpdateWindow( hwndList );
   FreeMemory( &lpi );
   lpWindInfo = GetBlock( hwnd );
   if( fMove )
      {
      PTH pthCurrent;

      pthCurrent = lpWindInfo->lpMessage;
      AdvanceAfterMarking( hwnd, IDM_MARKTAGGED );
      fMove = lpWindInfo->lpMessage != pthCurrent;
      }
   if( !fMove )
      ShowMsg( hwnd, lpWindInfo->lpMessage, TRUE, TRUE, FALSE );
}

/* This function ignores all messages specified by the
 * lpi array of message indexes.
 */
void FASTCALL MarkIgnoreRange( HWND hwnd, LPINT lpi )
{
   HWND hwndList;
   register int c;
   BOOL fStatus;
   PTH pth;

   fStatus = FALSE;
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   for( c = 0; lpi[ c ] != LB_ERR; ++c )
      {
      BOOL fLastOne;

      pth = (PTH)ListBox_GetItemData( hwndList, lpi[ c ] );
      if( 0 == c )
         fStatus = !Amdb_IsIgnored( pth );
      fLastOne = lpi[ c + 1 ] == LB_ERR;
      Amdb_MarkMsgIgnore( pth, fStatus, fLastOne );
      }
   UpdateThreadDisplay( hwnd, TRUE );
   UpdateStatusDisplay( hwnd );
   FreeMemory( &lpi );
   AdvanceAfterMarking( hwnd, IDM_IGNORE );
}

/* This function watches all messages specified by the
 * lpi array of message indexes.
 */
void FASTCALL MarkWatchRange( HWND hwnd, LPINT lpi )
{
   HWND hwndList;
   register int c;
   BOOL fStatus;
   PTH pth;

   fStatus = FALSE;
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   for( c = 0; lpi[ c ] != LB_ERR; ++c )
      {
      pth = (PTH)ListBox_GetItemData( hwndList, lpi[ c ] );
      if( 0 == c )
         fStatus = !Amdb_IsWatched( pth );
      Amdb_MarkMsgWatch( pth, fStatus, TRUE );
      }
   UpdateThreadDisplay( hwnd, TRUE );
   UpdateStatusDisplay( hwnd );
   FreeMemory( &lpi );
   AdvanceAfterMarking( hwnd, IDM_WATCH );
   if( Amuser_GetPPInt( szSettings, "AdvanceWatch", ADVANCE_STAY ) == ADVANCE_STAY )
   {
         LPWINDINFO lpWindInfo;

         lpWindInfo = GetBlock( hwnd );
         ShowMsg( hwnd, lpWindInfo->lpMessage, TRUE, TRUE, FALSE );
   }

}

/* This function copies all messages specified by the
 * lpi array of message indexes.
 */
void FASTCALL CopyMessageRange( HWND hwnd, PTH FAR * lppth, BOOL fMove, BOOL fRepost )
{
   LPWINDINFO lpWindInfo;
   PCL pclSrc;
   PTL ptlSrc;
   PICKER cp;
   WORD wFType;

   /* First ask for the destination.
    */
   lpWindInfo = GetBlock( hwnd );
   cp.wType = CPP_TOPICS;
   if( fRepost )
      strcpy( cp.szCaption, GS(IDS_STR1219) );
   else
      strcpy( cp.szCaption, fMove ? GS(IDS_STR347) : GS(IDS_STR346) );
   ptlSrc = lpWindInfo->lpTopic;
   pclSrc = Amdb_FolderFromTopic( ptlSrc );
   if( !fLastCopy )
      {
      pclLastSrc = pclSrc;
      ptlLastSrc = ptlSrc;
      }
   cp.pcl = pclLastSrc;
   cp.ptl = ptlLastSrc;
   fLastCopy = TRUE;
   wFType = Amdb_GetTopicType( ptlSrc );
   if( fMove && !( wFType == FTYPE_CIX || wFType == FTYPE_MAIL || wFType == FTYPE_LOCAL ) )
      fMessageBox( hwnd, 0, GS(IDS_STR508), MB_OK|MB_ICONEXCLAMATION );
   else
      {
      if( fRepost )
         cp.wHelpID = idsCOPYMSG;
      else
         cp.wHelpID = fMove ? idsMOVEMSG : idsCOPYMSG;
      if( Amdb_ConfPicker( hwnd, &cp ) )
         {
         HWND hwndList;
         int c;

         /* Count how many messages we're copying.
          */
         for( c = 0; lppth[ c ] != NULL; ++c );

         /* Call the copy range function to do the actual copy or
          * move.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         if( DoCopyMsgRange( hwnd, lppth, c, cp.ptl, fMove, FALSE, fRepost ) )
            if( !fMove )
               for( c = 0; lppth[ c ] != NULL; ++c )
                  {
                  RECT rect;
                  int index;

                  /* Update each item moved.
                   */
                  index = Amdb_GetMsgSel( lppth[ c ] );
                  if( LB_ERR != index )
                     {
                     ListBox_GetItemRect( hwndList, index, &rect );
                     InvalidateRect( hwndList, &rect, FALSE );
                     }
                  }
         ptlLastSrc = cp.ptl;
         pclLastSrc = cp.pcl;
         }
      }
   FreeMemory( (LPVOID)&lppth );
}


/* This function keeps all messages specified by the
 * lpi array of message indexes.
 */
void FASTCALL MarkKeepRange( HWND hwnd, LPINT lpi )
{
   HWND hwndList;
   register int c;
   BOOL fStatus;
   PTH pth;

   fStatus = FALSE;
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   for( c = 0; lpi[ c ] != LB_ERR; ++c )
      {
      RECT rect;

      pth = (PTH)ListBox_GetItemData( hwndList, lpi[ c ] );
      if( 0 == c )
         fStatus = !Amdb_IsKept( pth );
      Amdb_MarkMsgKeep( pth, fStatus );
      ListBox_GetItemRect( hwndList, lpi[ c ], &rect );
      InvalidateRect( hwndList, &rect, FALSE );
      }
   UpdateWindow( hwnd );
   FreeMemory( &lpi );
   AdvanceAfterMarking( hwnd, IDM_KEEPMSG );
}

/* This function either stops at the current message, moves to
 * the next message or to the next unread after marking the
 * current message.
 */
void FASTCALL AdvanceAfterMarking( HWND hwnd, int id )
{
   int nAdvanceMode;
   LPWINDINFO lpWindInfo;
   PTH pth;
         
   lpWindInfo = GetBlock( hwnd );
   pth = lpWindInfo->lpMessage;

   nAdvanceMode = ADVANCE_NEXTUNREAD;
   switch( id )
      {
      case IDM_IGNORE:  nAdvanceMode = Amuser_GetPPInt( szSettings, "AdvanceIgnore", nAdvanceMode ); break;
      case IDM_KEEPMSG: nAdvanceMode = Amuser_GetPPInt( szSettings, "AdvanceKeep", nAdvanceMode ); break;
      case IDM_PRIORITY:   nAdvanceMode = Amuser_GetPPInt( szSettings, "AdvancePriority", nAdvanceMode ); break;
      case IDM_MARKMSG: nAdvanceMode = Amuser_GetPPInt( szSettings, "AdvanceBookmark", nAdvanceMode ); break;
      case IDM_MARKTAGGED:nAdvanceMode = Amuser_GetPPInt( szSettings, "AdvanceTagged", nAdvanceMode ); break;
      case IDM_WATCH:      nAdvanceMode = Amuser_GetPPInt( szSettings, "AdvanceWatch", nAdvanceMode ); break;
      case IDM_READLOCK:   nAdvanceMode = Amuser_GetPPInt( szSettings, "AdvanceReadLock", nAdvanceMode ); break;
      case IDM_READMSG: nAdvanceMode = Amuser_GetPPInt( szSettings, "AdvanceRead", nAdvanceMode ); break;
      case IDM_KILLMSG:
      case IDM_DELETEMSG:  nAdvanceMode = Amuser_GetPPInt( szSettings, "AdvanceDelete", nAdvanceMode ); break;
      case IDM_WITHDRAW:   nAdvanceMode = Amuser_GetPPInt( szSettings, "AdvanceWithdraw", nAdvanceMode ); break;
      }
   if( nAdvanceMode != ADVANCE_STAY )
      if( nAdvanceMode == ADVANCE_NEXT )
      {
         if( id != IDM_KILLMSG )
            MoveToNextMessage( hwnd );
      }
      else
         {
         fKeepUnread = ( id == IDM_READMSG || id == IDM_KILLMSG || id == IDM_DELETEMSG );
         if( ( id == IDM_IGNORE || id == IDM_KEEPMSG || id == IDM_PRIORITY || id == IDM_MARKMSG 
            || id == IDM_MARKTAGGED || id == IDM_WATCH || id == IDM_READLOCK || id == IDM_READMSG 
               || id == IDM_WITHDRAW ) || ( ( id == IDM_KILLMSG || id == IDM_DELETEMSG ) && pth 
                  && Amdb_IsRead( pth ) ) )
            SendCommand( hwnd, IDM_NEXT, 0 );
         fKeepUnread = FALSE;
         }
}

/* This function marks priority all messages specified by
 * the lpi array of message indexes.
 */
void FASTCALL MarkPriorityRange( HWND hwnd, LPINT lpi )
{
   HWND hwndList;
   register int c;
   BOOL fStatus;
   PTH pth;

   fStatus = FALSE;
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   for( c = 0; lpi[ c ] != LB_ERR; ++c )
      {
      BOOL fLastOne;

      pth = (PTH)ListBox_GetItemData( hwndList, lpi[ c ] );
      if( 0 == c )
         fStatus = !Amdb_IsPriority( pth );
      fLastOne = lpi[ c + 1 ] == LB_ERR;
      Amdb_MarkMsgPriority( pth, fStatus, fLastOne );
      }
   UpdateThreadDisplay( hwnd, TRUE );
// UpdateWindow( hwndList );
   UpdateStatusDisplay( hwnd );
   FreeMemory( &lpi );
   AdvanceAfterMarking( hwnd, IDM_PRIORITY );
}

/* This function marks read lock all messages specified by
 * the lpi array of message indexes.
 */
void FASTCALL MarkReadLockRange( HWND hwnd, LPINT lpi )
{
   HWND hwndList;
   register int c;
   BOOL fStatus;
   PTH pth;

   fStatus = FALSE;
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   for( c = 0; lpi[ c ] != LB_ERR; ++c )
      {
      RECT rect;

      pth = (PTH)ListBox_GetItemData( hwndList, lpi[ c ] );
      if( 0 == c )
         fStatus = !Amdb_IsReadLock( pth );
      if( !fStatus )
         {
         Amdb_MarkMsgReadLock( pth, FALSE );
         Amdb_MarkMsgRead( pth, TRUE );
         }
      else
         {
         Amdb_MarkMsgRead( pth, FALSE );
         Amdb_MarkMsgReadLock( pth, TRUE );
         }
      ListBox_GetItemRect( hwndList, lpi[ c ], &rect );
      InvalidateRect( hwndList, &rect, FALSE );
      }
   UpdateWindow( hwnd );
   UpdateStatusDisplay( hwnd );
   FreeMemory( &lpi );
   AdvanceAfterMarking( hwnd, IDM_READLOCK );
}

/* This function marks read all messages specified by
 * the lpi array of message indexes.
 */
void FASTCALL MarkReadRange( HWND hwnd, LPINT lpi )
{
   HWND hwndList;
   register int c;
   BOOL fStatus;
   PTH pth;

   fStatus = FALSE;
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   for( c = 0; lpi[ c ] != LB_ERR; ++c )
      {
      RECT rect;

      pth = (PTH)ListBox_GetItemData( hwndList, lpi[ c ] );
      if( 0 == c )
         fStatus = !Amdb_IsRead( pth );
      Amdb_MarkMsgRead( pth, fStatus );
      ListBox_GetItemRect( hwndList, lpi[ c ], &rect );
      InvalidateRect( hwndList, &rect, FALSE );
      }
   UpdateWindow( hwndList );
   UpdateStatusDisplay( hwnd );
   FreeMemory( &lpi );
   AdvanceAfterMarking( hwnd, IDM_READMSG );
}

/* This function deletes or marks deleted all messages specified by
 * the lppth array of message handles.
 */
void FASTCALL DeleteMsgRange( HWND hwnd, PTH FAR * lppth, BOOL fPhysicalDelete )
{
   HWND hwndList;
   register int c;
   BOOL fMove;
   BOOL fPrompt;
   BOOL fStatus;
   BOOL fDeleted;
   int iSelItem;
   int retCode;

   hwndList = GetDlgItem( hwnd, IDD_LIST );
   fMove = FALSE;
   fStatus = FALSE;
   fPrompt = TRUE;
   fDeleted = FALSE;
   ASSERT( NULL != lppth[ 0 ] );
   iSelItem = LB_ERR;
   for( c = 0; lppth[ c ] != NULL; ++c )
      {
      RECT rect;

      if( fPhysicalDelete )
         {

         if( !fPrompt )
            retCode = IDYES;
         else
            {
            MSGINFO msginfo;
            UINT uFlags;
   
            Amdb_GetMsgInfo( lppth[ c ], &msginfo );

            /* If we did some deletes without refreshing the thread pane,
             * then refresh it now.
             */
            wsprintf( lpTmpBuf, GS(IDS_STR778), (LPSTR)msginfo.szTitle );
            uFlags = ( lppth[ c + 1 ] != NULL ) ? MB_YESNOALLCANCEL : MB_YESNO;
            retCode = fMessageBox( hwnd, 0, lpTmpBuf, uFlags|MB_ICONINFORMATION );
            if( retCode == IDCANCEL )
               break;
            if( retCode == IDALL )
               {
               fPrompt = FALSE;
               retCode = IDYES;
               }
            }
         if( ( c == 0 && lppth[ c + 1 ] == NULL ) || retCode != IDNO || ( retCode == IDNO && lppth[ c + 1 ] != NULL ) )
            iSelItem = Amdb_GetMsgSel( lppth[ c ] );
         if( retCode == IDYES )
         {
            Amdb_DeleteMsg( lppth[ c ], fPrompt );
            fDeleted = TRUE;
         }


         }
      else
         {
         int index;

         if( 0 == c )
            fStatus = !Amdb_IsDeleted( lppth[ c ] );
         Amdb_MarkMsgDelete( lppth[ c ], fStatus );
         fDeleted = TRUE;
         index = Amdb_GetMsgSel( lppth[ c ] );
         if( LB_ERR != index )
            {
            ListBox_GetItemRect( hwndList, index, &rect );
            InvalidateRect( hwndList, &rect, FALSE );
            }
         fMove = TRUE;
         }
      }
   UpdateWindow( hwndList );
   UpdateStatusDisplay( hwnd );
   if( fMove )
      AdvanceAfterMarking( hwnd, fPhysicalDelete ? IDM_KILLMSG: IDM_DELETEMSG );
   else if( fDeleted )
      {
      CURMSG unr;

      /* If we did some deletes without refreshing the thread pane,
       * then refresh it now.
       */
      Ameol2_GetCurrentMsg( &unr );
      
      /*
       !!SM!! Not sure why the below is needed, it messes up going to the next message.
       */
      
//    unr.pth = NULL;
//    if( iSelItem == ListBox_GetCount( hwndList ) )
//       --iSelItem;
//    if( iSelItem != -1 )
         {
//       unr.pth = (PTH)ListBox_GetItemData( hwndList, iSelItem );
         if( unr.pth != NULL )
            {
            LPWINDINFO lpWindInfo;

            lpWindInfo = GetBlock( hwnd );
            lpWindInfo->lpMessage = NULL;
            }
         }

      SetCurrentMsg( &unr, TRUE, TRUE );
      ForceFocusToList( hwnd );
      AdvanceAfterMarking( hwnd, fPhysicalDelete ? IDM_KILLMSG: IDM_DELETEMSG );
      }
   FreeMemory( (LPVOID)&lppth );
}

/* This function withdraws the selected message.
 */
void FASTCALL MarkMsgWithdrawn( HWND hwnd, LPINT lpi )
{
   LPWINDINFO lpWindInfo;
   HWND hwndList;
   register int c;
   BOOL fStatus;
   int wType;
   PTH pth;

   lpWindInfo = GetBlock( hwnd );
   fStatus = FALSE;
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   wType = Amdb_GetTopicType( lpWindInfo->lpTopic );
   for( c = 0; lpi[ c ] != LB_ERR; ++c )
      {
      RECT rect;

      pth = (PTH)ListBox_GetItemData( hwndList, lpi[ c ] );
      if( wType == FTYPE_CIX )
         MarkCIXMsgWithdrawn( hwnd, pth );
      else if( wType == FTYPE_NEWS )
         MarkNewsMsgWithdrawn( hwnd, pth );
      ListBox_GetItemRect( hwndList, lpi[ c ], &rect );
      InvalidateRect( hwndList, &rect, FALSE );
      }
   UpdateWindow( hwndList );
   UpdateStatusDisplay( hwnd );
   FreeMemory( &lpi );
   SetFocus( hwndMsg );
   AdvanceAfterMarking( hwnd, IDM_WITHDRAW );
}

/* This function withdraws a message from Usenet.
 */
void FASTCALL MarkNewsMsgWithdrawn( HWND hwnd, PTH pth )
{
   LPWINDINFO lpWindInfo;
   BOOL fCanWithdraw;
   MSGINFO msginfo;
   BOOL fState;
   PCL pcl;
   PTL ptl;

   /* Is the message already withdrawn? If so, just mark it
    * unwithdrawn. No feedback needed.
    */
   lpWindInfo = GetBlock( hwnd );
   Amdb_GetMsgInfo( pth, &msginfo );
   fState = !Amdb_IsWithdrawn( pth );

   /* Check we can legally withdraw this message.
    */
   ptl = lpWindInfo->lpTopic;
   pcl = Amdb_FolderFromTopic( ptl );
   fCanWithdraw = _strcmpi( szMailAddress, msginfo.szAuthor ) == 0;
   if( !fCanWithdraw )
      fCanWithdraw = _strcmpi( szCIXNickname, msginfo.szAuthor ) == 0;
   if( !fCanWithdraw )
   {
      Amdb_GetCookie( ptl, NEWS_EMAIL_COOKIE, lpTmpBuf, szMailAddress, 64 );
      fCanWithdraw = _strcmpi( lpTmpBuf, msginfo.szAuthor ) == 0;
   }
   if( fCanWithdraw )
      {
      CANCELOBJECT co;
      LPOB lpob;

      Amob_New( OBTYPE_CANCELARTICLE, &co );
      Amob_CreateRefString( &co.recReply, msginfo.szReply );
      Amob_CreateRefString( &co.recNewsgroup, (HPSTR)Amdb_GetTopicName( lpWindInfo->lpTopic ) );
      Amob_CreateRefString( &co.recFrom, (HPSTR)msginfo.szAuthor );
      Amdb_GetCookie( ptl, NEWS_FULLNAME_COOKIE, lpTmpBuf, szFullName, 64 );
      Amob_CreateRefString( &co.recRealName, (HPSTR)lpTmpBuf );
      if( fState )
         {
         if( Amob_Find( OBTYPE_CANCELARTICLE, &co ) )
            fMessageBox( hwnd, 0, GS(IDS_STR792), MB_OK|MB_ICONEXCLAMATION );
         else
            {
            wsprintf( lpTmpBuf, GS(IDS_STR1046), msginfo.szTitle );
            if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION ) == IDYES )
               {
               Amob_Commit( NULL, OBTYPE_CANCELARTICLE, &co );
               Amdb_MarkMsgWithdrawn( pth, TRUE );
               }
            }
         }
      else {
         if( lpob = Amob_Find( OBTYPE_CANCELARTICLE, &co ) )
            Amob_RemoveObject( lpob, TRUE );
         Amdb_MarkMsgWithdrawn( pth, FALSE );
         }
      Amob_Delete( OBTYPE_CANCELARTICLE, &co );

      /* Update the selected item.
       */
      UpdateCurrentMsgDisplay( hwnd );

      /* If the withdrawn message was originally marked unread,
       * it'll be marked read. So update the unread counters.
       */
      if( !( msginfo.dwFlags & MF_READ ) )
         {
         UpdateStatusDisplay( hwnd );
         Amuser_CallRegistered( AE_UNREADCHANGED, (LPARAM)ptl, 0L );
         }
      }
   else
      fMessageBox( hwnd, 0, GS(IDS_STR654), MB_OK|MB_ICONINFORMATION );
}

/* This function withdraws a message from CIX.
 */
void FASTCALL MarkCIXMsgWithdrawn( HWND hwnd, PTH pth )
{
   LPWINDINFO lpWindInfo;
   MSGINFO msginfo;
   PCL pcl;
   PTL ptl;
   BOOL fState;

   /* Is the message already withdrawn? If so, just mark it
    * unwithdrawn. No feedback needed.
    */
   lpWindInfo = GetBlock( hwnd );
   Amdb_GetMsgInfo( pth, &msginfo );
   fState = !Amdb_IsWithdrawn( pth );

   /* Check we can legally withdraw this message.
    */
   ptl = lpWindInfo->lpTopic;
   pcl = Amdb_FolderFromTopic( ptl );
   if( _strcmpi( szCIXNickname, msginfo.szAuthor ) == 0 || Amdb_IsModerator( pcl, szCIXNickname ) )
      {
      WITHDRAWOBJECT wo;
      LPOB lpob;
      RESTOREOBJECT ro;

      Amob_New( OBTYPE_RESTORE, &ro );
      ro.dwMsg = msginfo.dwMsg;
      wsprintf( lpTmpBuf, "%s/%s", Amdb_GetFolderName( pcl ), Amdb_GetTopicName( ptl ) );
      Amob_CreateRefString( &ro.recTopicName, lpTmpBuf );

      Amob_New( OBTYPE_WITHDRAW, &wo );
      wo.dwMsg = msginfo.dwMsg;
      wsprintf( lpTmpBuf, "%s/%s", Amdb_GetFolderName( pcl ), Amdb_GetTopicName( ptl ) );
      Amob_CreateRefString( &wo.recTopicName, lpTmpBuf );
      if( fState )
         {
         if( lpob = Amob_Find( OBTYPE_RESTORE, &ro ) )
            {
               Amob_RemoveObject( lpob, TRUE );
               Amdb_MarkMsgWithdrawn( pth, TRUE );
            }

         else if( Amob_Find( OBTYPE_WITHDRAW, &wo ) )
            fMessageBox( hwnd, 0, GS(IDS_STR52), MB_OK|MB_ICONEXCLAMATION );
         else
            if( fMessageBox( hwnd, 0, GS(IDS_STR53), MB_YESNO|MB_ICONQUESTION ) == IDYES )
               {
               Amob_Commit( NULL, OBTYPE_WITHDRAW, &wo );
               Amdb_MarkMsgWithdrawn( pth, TRUE );
               }
         }
      else 
         {
         if( lpob = Amob_Find( OBTYPE_WITHDRAW, &wo ) )
            {
               Amob_RemoveObject( lpob, TRUE );
               Amdb_MarkMsgWithdrawn( pth, FALSE );
            }
         else
         {
            if( fMessageBox( hwnd, 0, GS(IDS_STR1238), MB_YESNO|MB_ICONQUESTION ) == IDYES )
               {
               Amob_Commit( NULL, OBTYPE_RESTORE, &ro );
               Amdb_MarkMsgWithdrawn( pth, FALSE );
               }

         }

         }
      Amob_Delete( OBTYPE_WITHDRAW, &wo );
      Amob_Delete( OBTYPE_RESTORE, &ro );

      /* Update the selected item.
       */
      UpdateCurrentMsgDisplay( hwnd );

      /* If the withdrawn message was originally marked unread,
       * it'll be marked read. So update the unread counters.
       */
      if( !( msginfo.dwFlags & MF_READ ) )
         {
         UpdateStatusDisplay( hwnd );
         Amuser_CallRegistered( AE_UNREADCHANGED, (LPARAM)ptl, 0L );
         }
      }
   else
      fMessageBox( hwnd, 0, GS(IDS_STR654), MB_OK|MB_ICONINFORMATION );
}

/* This function handles retrieving a binmail as a result of a
 * cover note being detected in the incoming mail message.
 */
void FASTCALL CmdCoverNote( HWND hwnd )
{
   LPWINDINFO lpWindInfo;
   PCL pcl;

   lpWindInfo = GetBlock( hwnd );
   pcl = Amdb_FolderFromTopic( lpWindInfo->lpTopic );
   if( Amdb_GetTopicType( lpWindInfo->lpTopic ) == FTYPE_MAIL )
      if( NULL != lpWindInfo->lpMessage )
         {
         char sz[ 200 ];
   
         /* Locate the X-Olr field in the header.
          */
         Amdb_GetMailHeaderField( lpWindInfo->lpMessage, "X-OLR", sz, sizeof(sz), FALSE );
         if( *sz )
            {
            char szFileName[ LEN_CIXFILENAME + 1 ];
            register int c;
            DWORD dwSize;
            char * psz;
   
            /* Extract the filename portion.
             */
            psz = sz;
            while( *psz && *psz != ' ' )
               ++psz;
            while( *psz == ' ' )
               ++psz;
            for( c = 0; *psz != CR && *psz != LF && *psz != ' '; ++c )
               {
               if( c < LEN_CIXFILENAME )
                  szFileName[ c ] = *psz;
               ++psz;
               }
            szFileName[ c ] = '\0';
   
            /* Extract the size field. This may be absent if the message
             * is sent from a less superior OLR (!)
             */
            dwSize = 0;
            while( *psz == ' ' )
               ++psz;
            if( isdigit( *psz ) )
               {
               while( isdigit( *psz ) )
                  dwSize = dwSize * 10 + (*psz++ - '0');
               wsprintf( lpTmpBuf, GS(IDS_STR85), (LPSTR)szFileName, dwSize );
               }
            else
               wsprintf( lpTmpBuf, GS(IDS_STR455), (LPSTR)szFileName );
   
            /* Now ask the user whether or not they want to grab the binmail. If
             * so, fire off a Mail Download to get the file.
             */
            if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION ) == IDYES )
            {
               if( fUseCIX )
                  CmdMailDownload( hwnd, szFileName );
               else
                  fMessageBox( hwnd, 0, "CIX service not installed.\n\nCould not download file.", MB_OK|MB_ICONEXCLAMATION );
            }
   
            /* Clear the cover note bit.
             */
            Amdb_GetSetCoverNoteBit( lpWindInfo->lpMessage, BSB_CLRBIT );
            }
         }
}

/* This function extracts a highlighted filename from the
 * edit window.
 */
void FASTCALL GetMarkedFilename( HWND hwnd, LPSTR lpszFileName, int cbMax )
{
   BEC_SELECTION bsel;
   HWND hwndEdit;
   HEDIT hedit;

   /* Assume there isn't a selection.
    */
   lpszFileName[ 0 ] = '\0';

   /* Open the message viewer control and get the
    * selection.
    */
   VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_MESSAGE ) );
   hedit = Editmap_Open( hwndEdit );
   Editmap_GetSelection( hedit, hwndEdit, &bsel );
   if( bsel.lo != bsel.hi )
   {
      LPSTR lpszMsg;
      UINT cbEdit;

      INITIALISE_PTR(lpszMsg);
//    cbEdit = (UINT)( bsel.hi - bsel.lo );
      cbEdit = ( bsel.hi - bsel.lo ) + 1; // !!SM!! 2.55.2035
      if( fNewMemory( &lpszMsg, cbEdit + 1 ) )
      {
         LPSTR lpszMsg2;
         int count;
         int c;
         int d;
   
         Editmap_GetSel( hedit, hwndEdit, cbEdit, lpszMsg );
         lpszMsg2 = lpszMsg;
         d = 0;
         while( lpszMsg[ d ] && !( lpszMsg[ d ] == '.' || ValidFileNameChr( lpszMsg[ d ] ) ) )
            ++d;
         count = (int)( cbEdit - d );
         if( count > 1 && cbMax > 0 )
         {
            count = min( count, min( cbMax - 1, LEN_CIXFILENAME ) );
            for( c = 0; c < count; ++d )
            {
               if( !( lpszMsg[ d ] == '.' || ValidFileNameChr( lpszMsg[ d ] ) ) )
                  break;
               lpszFileName[ c++ ] = lpszMsg[ d ];
            }
            lpszFileName[ c ] = '\0';
         }
         FreeMemory( &lpszMsg );
      }
   }
}

/* This function retrieves any marked text from the specified
 * message window.
 */
void FASTCALL GetMarkedName( HWND hwnd, LPSTR lpszName, int cbMax, BOOL doCheck )
{
   BEC_SELECTION bsel;
   HWND hwndEdit;
   HEDIT hedit;

   /* Assume there isn't a selection.
    */
   lpszName[ 0 ] = '\0';

   /* Open the message viewer control and get the
    * selection.
    */
   VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_MESSAGE ) );
   hedit = Editmap_Open( hwndEdit );

   Editmap_GetSelection( hedit, hwndEdit, &bsel );
   if( bsel.lo != bsel.hi )
   {
      LPSTR lpszMsg;
      UINT cbEdit;

      if ( !doCheck || fMessageBox( hwnd, 0, "Use Selected Text as To: Address?", MB_YESNO|MB_ICONINFORMATION ) == IDYES ) 
      {
         INITIALISE_PTR(lpszMsg);
         cbEdit = ( bsel.hi - bsel.lo ) + 1; // !!SM!! 2.55.2037
         if( fNewMemory( &lpszMsg, cbEdit + 1 ) )
         {
            LPSTR lpszMsg2;
            int count;
            int d;
            int c;
      
            Editmap_GetSel( hedit, hwndEdit, cbEdit, lpszMsg );
            lpszMsg2 = lpszMsg;
            d = 0;
            while( lpszMsg[ d ] && lpszMsg[ d ] == ' ' )
               ++d;
            count = (int)( cbEdit - d );
            if( count > 1 && cbMax > 0 )
            {        
               count = min( count, cbMax - 1 );
               for( c = 0; c < count && lpszMsg[ d ] != CR && lpszMsg[ d ] != LF /*&& lpszMsg[ d ] != ' '*/; ++c, ++d ) //!!SM!! 2045
               {
                  if (lpszMsg[ d ] == ' ')
                  {
                     if ( ( d > 0 && lpszMsg[ d - 1 ] != ',') && ( d < count && lpszMsg[ d + 1 ] != ',') && 
                         ( d > 0 && lpszMsg[ d - 1 ] != ';') && ( d < count && lpszMsg[ d + 1 ] != ';'))
                        lpszName[ c ] = ',';                                                      //!!SM!! 2045
                     else
                        c--;
                  }
                  else if (lpszMsg[ d ] == ',')
                  {
                     if ( ( d > 0 && lpszMsg[ d - 1 ] != ',') && ( d < count && lpszMsg[ d + 1 ] != ','))
                        lpszName[ c ] = ' ';                                                      //!!SM!! 2045
                     else
                        c--;
                  }
                  else if (lpszMsg[ d ] == ';')
                  {
                     if ( ( d > 0 && lpszMsg[ d - 1 ] != ',') && ( d < count && lpszMsg[ d + 1 ] != ','))
                        lpszName[ c ] = ' ';                                                      //!!SM!! 2045
                     else
                        c--;
                  }
                  else
                     lpszName[ c ] = lpszMsg[ d ];                                                  //!!SM!! 2045
               }
               lpszName[ c ] = '\0';
            }
            QuickReplace(lpszName, lpszName,"mailto:", ""); // FS#155
            FreeMemory( &lpszMsg );
         }
      }
   }
}

/* Given a pointer to the text for a message, this function
 * returns a pointer to the body of the text. If we're hiding
 * headers in Mail and Usenet messages, it skips those
 * headers.
 */
LPSTR FASTCALL GetTextBody( PTL ptl, LPSTR lpText2 )
{
   LPSTR lpText;
   WORD wType;

   lpText = lpText2;
   while( *lpText && *lpText != '\r' )
      ++lpText;
   if( *lpText == '\r' )
      ++lpText;
   if( *lpText == '\n' )
      ++lpText;
   wType = Amdb_GetTopicType( ptl );
   if( fStripHeaders && ( wType == FTYPE_NEWS || ( wType == FTYPE_MAIL && NULL != strstr( lpText2, "Date: " ) ) ) )
      {
      if( lpText2 = (HPSTR)_fstrstr( (LPSTR)lpText, "\r\n\r\n" ) )
         lpText = lpText2 + 4;
      }
   return( lpText );
}


LPSTR GetReadableText(PTL ptl, LPSTR pSource)
{
   strcpy(pSource, GetTextBody( ptl, pSource ));

   return( pSource );
}

void FASTCALL OpenAttachment(LPSTR pSource, LPSTR pName, LPSTR pNum, PTH pth) // !!SM!! 2044
{
   SAVEIMPORTPROC lpImpProc;
   char lszAttachDir[_MAX_PATH];
   char lszTemp[_MAX_PATH];
   int uRetCode;

   if (hRegLib < (HINSTANCE)32)
      return;

   if( NULL == ( lpImpProc = (SAVEIMPORTPROC)GetProcAddress( hRegLib, "SaveAttachment" ) ) )
      return;

   GetTempPath( _MAX_PATH, lpPathBuf );

   if ( Amdir_QueryDirectory( lpPathBuf ) )
   {
      if (lpPathBuf[strlen(lpPathBuf)-1] != '\\')
         strcat(lpPathBuf,"\\");

      wsprintf(lszAttachDir, "%s%s", lpPathBuf, pName);                                                                            // !!SM!! 2043

      if( SafeToOpen( hwndFrame, lszAttachDir ) )
      {
         if( lpImpProc( pSource, lszAttachDir, pNum ) )
         {
            if( ( uRetCode = (UINT)ShellExecute( hwndFrame, NULL, lszAttachDir, "", pszAmeolDir, SW_SHOW ) ) < 32 )
               DisplayShellExecuteError( hwndFrame, lszAttachDir, uRetCode );
         }
         else
         {                                                                                              // !!SM!! 2047
            wsprintf(lszTemp, "Error Saving: %s", lszAttachDir);                                                                 
            fMessageBox( hwndFrame, 0, lszTemp, MB_OK|MB_ICONEXCLAMATION );
         }
      }
   }
   else
   {
      wsprintf(lszAttachDir, "Directory: \"%s\" does not exist", lszAttachDir);                                                                 
      fMessageBox( hwndFrame, 0, lszAttachDir, MB_OK|MB_ICONEXCLAMATION );
   }
}

void FASTCALL SaveAttachment(LPSTR pSource, LPSTR pName, LPSTR pNum, PTH pth) // !!SM!! 2040
{
   SAVEIMPORTPROC lpImpProc;
   OPENFILENAME ofn;
   static char strFilters[] = "All Files (*.*)\0*.*\0\0";
   static char FAR szOutBuf[ _MAX_PATH ];    /* Full path to filename */
   char lszAttachDir[_MAX_PATH];

   if (hRegLib < (HINSTANCE)32)
      return;

   if( NULL == ( lpImpProc = (SAVEIMPORTPROC)GetProcAddress( hRegLib, "SaveAttachment" ) ) )
      return;

   if( Amdb_GetTopicType( Amdb_TopicFromMsg(pth) ) == FTYPE_MAIL )                                                            // !!SM!! 2041
      Amdb_GetCookie( Amdb_TopicFromMsg(pth), MAIL_ATTACHMENTS_COOKIE, (LPSTR)&lszAttachDir, pszAttachDir, _MAX_PATH - 1 );  // !!SM!! 2041
   else if (Amdb_GetTopicType( Amdb_TopicFromMsg(pth) ) == FTYPE_NEWS )                                                       // !!SM!! 2041
      Amdb_GetCookie( Amdb_TopicFromMsg(pth), NEWS_ATTACHMENTS_COOKIE, (LPSTR)&lszAttachDir, pszAttachDir, _MAX_PATH - 1 );  // !!SM!! 2041
   else                                                                                                                       // !!SM!! 2041
      wsprintf(lszAttachDir, "%s", pszAttachDir);                                                                            // !!SM!! 2041

                                                                                                /* If we get here, we've got an import procedure entry point, so
    * call it and wait for it to do the biz.
    */

    strcpy(szOutBuf, pName);
   ofn.lStructSize = sizeof( OPENFILENAME );
   ofn.hwndOwner = hwndFrame;
   ofn.hInstance = NULL;
   ofn.lpstrFilter = strFilters;
   ofn.lpstrCustomFilter = NULL;
   ofn.nMaxCustFilter = 0;
   ofn.nFilterIndex = 1;
   ofn.lpstrFile = szOutBuf;
   ofn.nMaxFile = sizeof( szOutBuf );
   ofn.lpstrFileTitle = NULL;
   ofn.nMaxFileTitle = 0;
   ofn.lpstrInitialDir = lszAttachDir;
   ofn.lpstrTitle = "Save Attachment";
   ofn.Flags = OFN_NOCHANGEDIR|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST|OFN_OVERWRITEPROMPT;
   ofn.nFileOffset = 0;
   ofn.nFileExtension = 0;
   ofn.lpstrDefExt = NULL;
   ofn.lCustData = 0;
   ofn.lpfnHook = NULL;
   ofn.lpTemplateName = 0;
   if( GetSaveFileName( &ofn ) )
   {
      strcpy( pName, szOutBuf );
      if(!lpImpProc( pSource, pName, pNum ))
      {
         wsprintf(lszAttachDir, "Error Saving: %s", pName);                                                                 
         fMessageBox( hwndFrame, 0, lszAttachDir, MB_OK|MB_ICONEXCLAMATION );
      }
   }
}



/* This function fills the IDD_CONFNAME combo box with the
 * list of conference names.
 */
PCL FASTCALL FillConfList( HWND hwnd, LPSTR lpszConfName )
{
   HWND hwndList;
   int index;
   PCAT pcat;
   PCL pcl2;
   PCL pcl;

   hwndList = GetDlgItem( hwnd, IDD_CONFNAME );
   ComboBox_ResetContent( hwndList );
   SetWindowRedraw( hwndList, FALSE );
   pcl2 = lpszConfName ? Amdb_OpenFolder( NULL, lpszConfName ) : NULL;
   for( pcat = Amdb_GetFirstCategory(); pcat; pcat = Amdb_GetNextCategory( pcat ) )
      for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
         if( IsCIXFolder( pcl ) )
            ComboBox_AddString( hwndList, Amdb_GetFolderName( pcl ) );
   SetWindowRedraw( hwndList, TRUE );
   if( CB_ERR != ( index = ComboBox_FindStringExact( hwndList, -1, lpszConfName ) ) )
      ComboBox_SetCurSel( hwndList, index );
   else {
      /* Conference name isn't in the list. If this is a drop downlist
       * combo, select the first item otherwise fill the edit control
       * with the conference name.
       */
      if( ( GetWindowStyle( hwndList ) & 0x0003 ) == CBS_DROPDOWNLIST )
         ComboBox_SetCurSel( hwndList, 0 );
      else
         ComboBox_SetText( hwndList, lpszConfName );
      }
   ComboBox_SetExtendedUI( hwndList, TRUE );
   return( pcl2 );
}

/* This function fills the IDD_TOPICNAME combo box with the
 * list of topic names.
 */
PTL FASTCALL FillTopicList( HWND hwnd, LPSTR lpszConfName, LPSTR lpszTopicName )
{
   HWND hwndList;
   int index;
   PTL ptl2;
   PCL pcl;
   PTL ptl;

   hwndList = GetDlgItem( hwnd, IDD_TOPICNAME );
   ComboBox_ResetContent( hwndList );
   ptl2 = NULL;
   if( ( pcl = Amdb_OpenFolder( NULL, lpszConfName ) ) != NULL )
      {
      ptl2 = Amdb_OpenTopic( pcl, lpszTopicName );
      SetWindowRedraw( hwndList, FALSE );
      for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
         if( Amdb_GetTopicType( ptl ) == FTYPE_CIX )
            {
            ComboBox_AddString( hwndList, Amdb_GetTopicName( ptl ) );
            }
      SetWindowRedraw( hwndList, TRUE );
      }
   if( CB_ERR != ( index = ComboBox_FindStringExact( hwndList, -1, lpszTopicName ) ) )
      ComboBox_SetCurSel( hwndList, index );
   else
      {
      /* Topic name isn't in the list. If this is a drop downlist
       * combo, select the first item otherwise fill the edit control
       * with the topic name.
       */
      if( ( GetWindowStyle( hwndList ) & 0x0003 ) == CBS_DROPDOWNLIST )
         ComboBox_SetCurSel( hwndList, 0 );
      else
         ComboBox_SetText( hwndList, lpszTopicName );
      }
   ComboBox_SetExtendedUI( hwndList, TRUE );
   return( ptl2 );
}

/* This function fills the IDD_CONFNAME combo box with the
 * list of conference names.
 */
PCL FASTCALL FillLocalConfList( HWND hwnd, LPSTR lpszConfName )
{
   HWND hwndList;
   int nCurSel;
   PCAT pcat;
   PCL pcl2;
   PCL pcl;

   hwndList = GetDlgItem( hwnd, IDD_CONFNAME );
   ComboBox_ResetContent( hwndList );
   SetWindowRedraw( hwndList, FALSE );
   pcl2 = lpszConfName ? Amdb_OpenFolder( NULL, lpszConfName ) : NULL;
   nCurSel = 0;
   for( pcat = Amdb_GetFirstCategory(); pcat; pcat = Amdb_GetNextCategory( pcat ) )
      for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
         if( IsLocalFolder( pcl ) )
            {
            int index;

            index = ComboBox_AddString( hwndList, Amdb_GetFolderName( pcl ) );
            if( pcl == pcl2 )
               nCurSel = index;
            }
   SetWindowRedraw( hwndList, TRUE );
   ComboBox_SetCurSel( hwndList, nCurSel );
   ComboBox_SetExtendedUI( hwndList, TRUE );
   return( pcl2 );
}

/* This function fills the IDD_TOPICNAME combo box with the
 * list of topic names.
 */
PTL FASTCALL FillLocalTopicList( HWND hwnd, LPSTR lpszConfName, LPSTR lpszTopicName )
{
   HWND hwndList;
   int nCurSel;
   PCAT pcat;
   PTL ptl2;
   PCL pcl;
   PTL ptl;

   hwndList = GetDlgItem( hwnd, IDD_TOPICNAME );
   pcat = Amdb_GetFirstCategory();
   if( ( pcl = Amdb_OpenFolder( NULL, lpszConfName ) ) == NULL )
      pcl = Amdb_GetFirstFolder( pcat );
   ptl2 = lpszTopicName ? Amdb_OpenTopic( pcl, lpszTopicName ) : NULL;
   nCurSel = 0;
   ComboBox_ResetContent( hwndList );
   SetWindowRedraw( hwndList, FALSE );
   for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
      if( Amdb_GetTopicType( ptl ) == FTYPE_LOCAL )
         {
         int index;
      
         index = ComboBox_AddString( hwndList, Amdb_GetTopicName( ptl ) );
         if( ptl2 == ptl )
            nCurSel = index;
         }
   SetWindowRedraw( hwndList, TRUE );
   ComboBox_SetCurSel( hwndList, nCurSel );
   ComboBox_SetExtendedUI( hwndList, TRUE );
   return( ptl2 );
}

/* Returns the index to the nearest undeleted item from the one just
 * deleted. Returns -1 if no such item was found. YH 21/06/96 12:55
 */
int FASTCALL GetNearestUndeleted( HWND hwndList, int iJustDeleted )
{
   int   cListSize;
   int   iRet;
   BOOL  fFound;

   cListSize = ListBox_GetCount( hwndList );

   ASSERT( iJustDeleted >= 0 && iJustDeleted < cListSize );

   fFound = FALSE;

   /* Search forward
    */
   iRet = iJustDeleted + 1;
   while( iRet < cListSize && !fFound )
      if( (PTH)ListBox_GetItemData( hwndList, iRet ) != NULL )
         fFound = TRUE;
      else
         iRet++;

   /* If nothing was found then search backwards
    */
   if( !fFound )
      {
      iRet = iJustDeleted - 1;
      while( iRet >= 0 && !fFound )
         if( (PTH)ListBox_GetItemData( hwndList, iRet ) != NULL )
            fFound = TRUE;
         else
            iRet--;
      }

   if( !fFound )
      iRet = -1;
   else
      ASSERT( iJustDeleted >= 0 && iJustDeleted < cListSize );

   return iRet;
}

/* This function checks whether or not it is safe to open
 * this file. It looks for the file extension in the config
 * file under [Safe To Open] and, if found, returns TRUE.
 * Otherwise it puts up a warning dialog and seeks confirmation
 * from the user.
 */
BOOL FASTCALL SafeToOpen( HWND hwnd, char * pszFilename )
{
   char szYesNo[ 4 ];
   char * pExt;

   /* Look for extension first.
    */
   pExt = strrchr( pszFilename, '.' );
   if( NULL != pExt )
      if( Amuser_GetPPString( "Safe To Open", pExt + 1, "", szYesNo, sizeof(szYesNo) ) )
         if( _strcmpi( szYesNo, "Yes" ) == 0 )
            return( TRUE );

   /* Not found, so throw up dialog box.
    */
   return( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_QUERYOPENFILE), QueryOpenFile, (LPARAM)pszFilename ) );
}

/* This function handles the QueryOpenFile dialog box.
 */
BOOL EXPORT CALLBACK QueryOpenFile( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = QueryOpenFile_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the QueryOpenFile
 *  dialog.
 */
LRESULT FASTCALL QueryOpenFile_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, QueryOpenFile_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, QueryOpenFile_OnCommand );

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL QueryOpenFile_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   char * pExt;
   SIZE size;
   RECT rc;
   HDC hdc;

   /* Default to Yes for this file.
    */
   GetClientRect( GetDlgItem( hwnd, IDD_FILENAME ), &rc );
   strcpy( lpPathBuf, (LPSTR)lParam );
   hdc = GetDC( hwnd );
   AdjustNameToFit( hdc, lpPathBuf, rc.right, TRUE, &size );
   ReleaseDC( hwnd, hdc );
   SetDlgItemText( hwnd, IDD_FILENAME, lpPathBuf );
   CheckDlgButton( hwnd, IDD_YES, TRUE );

   /* Disable option to ignore files of this type if file has
    * no extension.
    */
   if( NULL == ( pExt = strrchr( (LPSTR)lParam, '.' ) ) )
      EnableControl( hwnd, IDD_NOPROMPT, FALSE );
   else
      {
      /* For certain extensions, don't allow people to register
       * them as safe.
       */
      ++pExt;
      if( _strcmpi( pExt, "exe" ) == 0 ||
          _strcmpi( pExt, "com" ) == 0 ||
          _strcmpi( pExt, "btm" ) == 0 ||
          _strcmpi( pExt, "lnk" ) == 0 ||
          _strcmpi( pExt, "reg" ) == 0 ||
          _strcmpi( pExt, "bat" ) == 0 )
         {
         EnableControl( hwnd, IDD_NOPROMPT, FALSE );
         CheckDlgButton( hwnd, IDD_YES, FALSE );
         CheckDlgButton( hwnd, IDD_NO, TRUE );
         }
      }
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL QueryOpenFile_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDOK: {
         BOOL fYes;

         /* Do we register this file extension as safe?
          */
         fYes = IsDlgButtonChecked( hwnd, IDD_YES );
         if( IsDlgButtonChecked( hwnd, IDD_NOPROMPT ) )
            {
            char * pExt;

            GetDlgItemText( hwnd, IDD_FILENAME, lpPathBuf, _MAX_PATH );
            pExt = strrchr( lpPathBuf, '.' );
            Amuser_WritePPString( "Safe To Open", pExt + 1, fYes ? "Yes" : "No" );
            }

         /* Return what the user asked us to do.
          */
         EndDialog( hwnd, fYes );
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}

/* Opens a thread before performing thread commands */

BOOL FASTCALL CheckThreadOpen( HWND hwnd )
{
   LPWINDINFO lpWindInfo;
   PTH pth;
   BOOL lRet;
   
   lRet = FALSE;

   lpWindInfo = GetBlock( hwnd );
   if ( lpWindInfo )
   {
      if( !Amdb_IsExpanded( lpWindInfo->lpMessage ) && lpWindInfo->vd.nViewMode != VM_VIEWCHRON )
      {
         if( pth = lpWindInfo->lpMessage )
         {
            if( Amdb_ExpandThread( pth, TRUE ) )
            {
               HWND hwndList;
               int nSel;

               VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
               nSel = ListBox_GetTopIndex( hwndList );
               SetMsgViewerWindow( hwnd, lpWindInfo->lpTopic, nSel );
               fQuickUpdate = TRUE;
               ShowMsg( hwnd, pth, TRUE, TRUE, FALSE );
               fQuickUpdate = FALSE;
               lRet = TRUE;
            }
         }
      }
   }
   return lRet;
}