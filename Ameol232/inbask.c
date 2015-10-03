/* INBASK.C - Implements the Palantir in-basket
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
#include <ctype.h>
#include <memory.h>
#include <string.h>
#include <direct.h>
#include <io.h>
#include "print.h"
#include "cix.h"
#include "cixip.h"
#include "amlib.h"
#include "ameol2.h"
#include "rules.h"
#include "admin.h"
#include "cookie.h"
#include "..\Amdb32\amdbi.h"

#define  THIS_FILE   __FILE__

#define  MAX_SCANLEN       30
#define SORT_FOLDER_LIMIT  1
#define  ID_DRAGTIMER      89

#define  RR_YES               'y'
#define  RR_NO             'n'
#define  RR_DEFAULT           'd'

struct tagRR_OPTS {
   char * pszName;
   int nMode;
} szRROpts[] = {
   "Always send",       RR_YES,
   "Always refuse",     RR_NO,
   "Ask",               RR_DEFAULT
   };

#define  cRROpts  (sizeof(szRROpts)/sizeof(szRROpts[0]))


char NEAR szInBaskWndClass[] = "amctl_inbaskcls";
char NEAR szInBaskWndName[] = "In Basket";

HWND hwndInBasket = NULL;              /* Handle of in-basket window */

static BOOL fDefDlgEx = FALSE;

static BOOL fRegistered = FALSE;       /* TRUE if we've registered the in-basket window class */
static HBRUSH hbrInBaskWnd = NULL;     /* Handle of in-basket window brush */
static WNDPROC lpProcListCtl;          /* Subclassed listbox window procedure */
static HIMAGELIST himl;                /* In-basket image list */
static SIZE sizeStat;                  /* Statistics field width */
static BOOL fDragging = FALSE;         /* TRUE if we're dragging a label */
static HTREEITEM hDragItem;            /* Item being dragged */
static HFONT hDlg6Font = NULL;         /* 6pt dialog font */
static HFONT hTitleFont = NULL;        /* 28pt dialog font */
static LPVOID pPropFolder;             /* Property sheet folder handle */
static BOOL fUpdatesBlocked;           /* TRUE if in-basket updates are blocked */
static DWORD dwLastScrollTime;         /* Ticks since the last scroll */
static PCL pclOriginalFolder;          /* Folder of topic being moved */
static PCAT pcatOriginalCategory;      /* Category of folder being moved */
static BOOL fMoving = FALSE;           /* TRUE if we're moving a topic */
static BOOL fKeepAtTopFolder = FALSE;
static int cCopies;                    /* Number of copies to print */
static int fPrintConfsOnly = TRUE;              /* TRUE if we only print confs */

HFONT hAboutFont;
static PTL ptlLastTopic = NULL;

int nLastPropertiesPage = IPP_SETTINGS;   /* Last properties page */

typedef struct tagTOPICFLAGSTATE {
   unsigned fResigned;
   unsigned fReadOnly;
   unsigned fIgnored;
   unsigned fHasFiles;
   unsigned fReadFull;
   unsigned fMailingList;
   unsigned fListFolder;
} TOPICFLAGSTATE;

BOOL FASTCALL InBasket_OnCreate( HWND, LPCREATESTRUCT );
void FASTCALL InBasket_OnClose( HWND );
void FASTCALL InBasket_OnSize( HWND, UINT, int, int );
void FASTCALL InBasket_OnMove( HWND, int, int );
void FASTCALL InBasket_OnCommand( HWND, int, HWND, UINT );
void FASTCALL InBasket_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
void FASTCALL InBasket_MDIActivate( HWND, BOOL, HWND, HWND );
void FASTCALL InBasket_OnSetFocus( HWND, HWND );
HBRUSH FASTCALL InBasket_OnCtlColor( HWND, HDC, HWND, int );
void FASTCALL InBasket_OnMouseMove( HWND, int, int, UINT );
void FASTCALL InBasket_OnLButtonUp( HWND, int, int, UINT );
void FASTCALL InBasket_OnCancelMode( HWND );
void FASTCALL InBasket_RightClickMenu( HWND, HWND );
void FASTCALL InBasket_OnCategoryChanged( HWND, int, PCAT );
void FASTCALL InBasket_OnFolderChanged( HWND, int, PCL );
void FASTCALL InBasket_OnTopicChanged( HWND, int, PTL );
void FASTCALL InBasket_OnAdjustWindows( HWND, int, int );

BOOL EXPORT CALLBACK InBasket_Prop1( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL InBasket_Prop1_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL InBasket_Prop1_OnNotify( HWND, int, LPNMHDR );
void FASTCALL InBasket_Prop1_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT CALLBACK InBasket_Prop3( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL InBasket_Prop3_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL InBasket_Prop3_OnNotify( HWND, int, LPNMHDR );
void FASTCALL InBasket_Prop3_OnCommand( HWND, int, HWND, UINT );
void FASTCALL Signature_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
void FASTCALL PaintSignature( HWND, HDC, const RECT FAR * );
void FASTCALL UpdateSignature( HWND );

BOOL FASTCALL EnumRethreadArticles( PTL, LPARAM );
BOOL FASTCALL EnumRebuild( PTL, LPARAM );

BOOL EXPORT CALLBACK InBasket_Prop5( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL InBasket_Prop5_OnInitDialog( HWND, HWND, LPARAM );
LRESULT FASTCALL InBasket_Prop5_OnNotify( HWND, int, LPNMHDR );
void FASTCALL InBasket_Prop5_OnCommand( HWND, int, HWND, UINT );
void FASTCALL InBasket_Prop5_ChangeTopicFlags( PTL, DWORD );

BOOL EXPORT FAR PASCAL BuildDlgProc( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL BuildDlgProc_DlgProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL BuildDlgProc_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL BuildDlgProc_OnCommand( HWND, int, HWND, UINT );

DWORD FASTCALL InBasket_ReadFlags( HWND );
HWND FASTCALL GetActiveFolderWindow( void );
int FASTCALL GetTopicTypeImage( PTL );
BOOL FASTCALL InBasket_ViewFolder( PTL, BOOL );
void FASTCALL CheckTopicFlagButton( HWND, int, unsigned );
void FASTCALL ReadTopicFlags( PTL, TOPICFLAGSTATE * );
void FASTCALL SetTopicFlagState( unsigned *, BOOL );
void FASTCALL StoreTopicFlags( HWND, PTL );
void FASTCALL DobbleSetClrFlags( HWND, int, DWORD *, DWORD *, DWORD );
void FASTCALL UpdateStatistics( HWND, LPVOID );

UINT CALLBACK EXPORT InbasketPropertiesCallbackFunc( HWND, UINT, LPARAM );

void FASTCALL InBasket_DeleteTopic( HWND, PTL, HTREEITEM );
void FASTCALL InBasket_DeleteFolder( HWND, PCL, HTREEITEM );
void FASTCALL InBasket_DeleteCategory( HWND, PCAT, HTREEITEM );

LRESULT EXPORT CALLBACK InBasketWndProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT FAR PASCAL InBaskListProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL InBasket_SortCommand( HWND );

void FASTCALL GetRROption( HWND, PTL );
void FASTCALL SetRROption( HWND, PTL );

BOOL EXPORT CALLBACK PrintInbasketDlg( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL PrintInbasketDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL PrintInbasketDlg_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL PrintInbasketDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL CmdPrintInbasket( HWND );


/* This function creates the in-basket window.
 */
BOOL FASTCALL CreateInBasket( HWND hwnd )
{
   MDICREATESTRUCT mcs;
   DWORD dwState;
   RECT rc;

   /* If outbasket window already open, bring it to the front
    * and display it.
    */
   if( hwndInBasket )
      {
      SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndInBasket, 0L );
      return( TRUE );
      }

   /* Register the in-basket window class if we have
    * not already done so.
    */
   if( !fRegistered )
      {
      WNDCLASS wc;

      wc.style       = CS_HREDRAW | CS_VREDRAW;
      wc.lpfnWndProc    = InBasketWndProc;
      wc.hIcon       = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_INBASKET) );
      wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
      wc.lpszMenuName      = NULL;
      wc.cbWndExtra     = DLGWINDOWEXTRA;
      wc.cbClsExtra     = 0;
      wc.hbrBackground  = (HBRUSH)( COLOR_APPWORKSPACE + 1 );
      wc.lpszClassName  = szInBaskWndClass;
      wc.hInstance      = hInst;
      if( !RegisterClass( &wc ) )
         return( FALSE );
      fRegistered = TRUE;
      }

   /* The default position of the in-basket.
    */
   GetClientRect( hwndMDIClient, &rc );
   rc.right = rc.right / 3;
   dwState = 0;

   /* Get the actual window size, position and state.
    */
   ReadProperWindowState( szInBaskWndName, &rc, &dwState );
   if( hwndActive && IsMaximized( hwndActive ) )
      dwState = WS_MAXIMIZE;

   /* Create the window.
    */
   mcs.szTitle = szInBaskWndName;
   mcs.szClass = szInBaskWndClass;
   mcs.hOwner = hInst;
   mcs.x = rc.left;
   mcs.y = rc.top;
   mcs.cx = rc.right - rc.left;
   mcs.cy = rc.bottom - rc.top;
   mcs.style = dwState;
   mcs.lParam = 0L;
   hwndInBasket = (HWND)SendMessage( hwndMDIClient, WM_MDICREATE, 0, (LPARAM)(LPMDICREATESTRUCT)&mcs );
   if( hwndInBasket == NULL )
      return( FALSE );

   /* Show the window.
    */
   SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndInBasket, 0L );
   UpdateWindow( hwndInBasket );
   return( TRUE );
}

/* This function handles all messages for the in-basket window.
 */
LRESULT EXPORT CALLBACK InBasketWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, InBasket_OnCreate );
      HANDLE_MSG( hwnd, WM_CLOSE, InBasket_OnClose );
      HANDLE_MSG( hwnd, WM_SIZE, InBasket_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, InBasket_OnMove );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, Common_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_COMMAND, InBasket_OnCommand );
      HANDLE_MSG( hwnd, WM_NOTIFY, InBasket_OnNotify );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, InBasket_MDIActivate );
      HANDLE_MSG( hwnd, WM_MENUSELECT, MainWnd_OnMenuSelect );
      HANDLE_MSG( hwnd, WM_DRAWITEM, InBasket_OnDrawItem );
      HANDLE_MSG( hwnd, WM_SETFOCUS, InBasket_OnSetFocus );
      HANDLE_MSG( hwnd, WM_INSERTCATEGORY, InBasket_OnInsertCategory );
      HANDLE_MSG( hwnd, WM_INSERTFOLDER, InBasket_OnInsertFolder );
      HANDLE_MSG( hwnd, WM_INSERTTOPIC, InBasket_OnInsertTopic );
      HANDLE_MSG( hwnd, WM_DELETECATEGORY, InBasket_OnDeleteCategory );
      HANDLE_MSG( hwnd, WM_DELETEFOLDER, InBasket_OnDeleteFolder );
      HANDLE_MSG( hwnd, WM_DELETETOPIC, InBasket_OnDeleteTopic );
      HANDLE_MSG( hwnd, WM_DRAGDROP, InBasket_OnDragDrop );
      HANDLE_MSG( hwnd, WM_MOUSEMOVE, InBasket_OnMouseMove );
      HANDLE_MSG( hwnd, WM_CANCELMODE, InBasket_OnCancelMode );
      HANDLE_MSG( hwnd, WM_LBUTTONUP, InBasket_OnLButtonUp );
      HANDLE_MSG( hwnd, WM_SETTOPIC, InBasket_OnSetTopic );
   #ifdef WIN32
      HANDLE_MSG( hwnd, WM_CTLCOLORLISTBOX, InBasket_OnCtlColor );
   #else
      HANDLE_MSG( hwnd, WM_CTLCOLOR, InBasket_OnCtlColor );
   #endif
      HANDLE_MSG( hwnd, WM_CATEGORYCHANGED, InBasket_OnCategoryChanged );
      HANDLE_MSG( hwnd, WM_FOLDERCHANGED, InBasket_OnFolderChanged );
      HANDLE_MSG( hwnd, WM_TOPICCHANGED, InBasket_OnTopicChanged );
      HANDLE_MSG( hwnd, WM_UPDATE_UNREAD, InBasket_OnUpdateUnread );
      HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, InBasket_OnAdjustWindows );

      case WM_REDRAWWINDOW:
         if( wParam & WIN_INBASK )
            {
            HWND hwndTreeCtl;

            hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
            Amdb_FillFolderTree( hwndTreeCtl, FTYPE_ALL|FTYPE_TOPICS, fShowTopicType );
            UpdateWindow( hwndTreeCtl );
            }
         return( 0L );

      case WM_TIMER: {
         POINT pt;

         /* The timer procedure feeds the parent window WM_MOUSEMOVE
          * messages even when the mouse isn't moving. This keeps the list
          * scrolling up or down when the mouse is outside the list box.
          */
         GetCursorPos( &pt );
         ScreenToClient( hwnd, &pt );
         SendMessage( hwnd, WM_MOUSEMOVE, 0, MAKELPARAM( pt.x, pt.y ) );
         return( 0L );
         }

      case WM_LOCKUPDATES:
         if( WIN_INBASK & wParam )
            {
            HWND hwndTreeCtl;

            /* This message is broadcast whenever Ameol wants a particular
             * window to not update itself.
             */
            hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
            SetWindowRedraw( hwndTreeCtl, LOWORD( lParam ) );
            fUpdatesBlocked = !LOWORD( lParam );
            if( LOWORD( lParam ) )
               {
               InvalidateRect( hwndTreeCtl, NULL, TRUE );
               UpdateWindow( hwndTreeCtl );
               }
            }
         return( 0L );

      case WM_CHANGEFONT:
         if( wParam == FONTYP_INBASKET || 0xFFFF == wParam )
            {
            HWND hwndTreeCtl;

            VERIFY( hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST ) );
            SetWindowFont( hwndTreeCtl, hInBasketFont, TRUE );
            }
         return( 0L );

      case WM_APPCOLOURCHANGE:
         InBasket_OnAppColourChange( hwnd, wParam );
         return( 0L );

      default:
         break;
      }
   return( Adm_DefMDIDlgProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_UPDATE_UNREAD message.
 */
void FASTCALL InBasket_OnUpdateUnread( HWND hwnd, PTL ptl )
{
   HWND hwndTreeCtl;
   HTREEITEM hTopicItem;
   TV_ITEM tv;

   /* If lParam is NULL, we update the unread count for the
    * selected item (fastest), otherwise lParam must be the
    * handle of the topic whose unread count we update.
    */
   if( fUpdatesBlocked )
      return;
   hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
   if( NULL == ptl )
      hTopicItem = TreeView_GetSelection( hwndTreeCtl );
   else
      hTopicItem = Amdb_FindTopicItem( hwndTreeCtl, ptl );
   if( 0 != hTopicItem )
      {
      /* Get the visible rectangle for the item we're updating.
       * If this is empty, then the item is collapsed or off-screen.
       */
      tv.hItem = hTopicItem;
      tv.mask = TVIF_RECT;
      VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
      if( !IsRectEmpty( &tv.rcItem ) )
         InvalidateRect( hwndTreeCtl, &tv.rcItem, TRUE );
      else
         {
         /* Is the folder visible?
          */
         hTopicItem = TreeView_GetParent( hwndTreeCtl, hTopicItem );
         tv.hItem = hTopicItem;
         tv.mask = TVIF_RECT|TVIF_STATE;
         tv.stateMask = 0x00FF;
         VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
         if( !IsRectEmpty( &tv.rcItem ) )
            {
            if( !(tv.state & TVIS_EXPANDED) )
               InvalidateRect( hwndTreeCtl, &tv.rcItem, TRUE );
            }
         else
            {
            /* Is the category visible?
             */
            tv.hItem = TreeView_GetParent( hwndTreeCtl, hTopicItem );
            tv.mask = TVIF_RECT|TVIF_STATE;
            tv.stateMask = 0x00FF;
            VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
            if( !IsRectEmpty( &tv.rcItem ) )
               {
               if( !(tv.state & TVIS_EXPANDED) )
                  InvalidateRect( hwndTreeCtl, &tv.rcItem, TRUE );
               }
            }
         }
      UpdateWindow( hwndTreeCtl );
      }
}

/* This function handles the WM_CATEGORYCHANGED message.
 */
void FASTCALL InBasket_OnCategoryChanged( HWND hwnd, int type, PCAT pcat )
{
   if( type == AECHG_NAME );
      InbasketCategoryNameChanged( hwnd, pcat );
}

/* This function handles the WM_FOLDERCHANGED message.
 */
void FASTCALL InBasket_OnFolderChanged( HWND hwnd, int type, PCL pcl )
{
   if( type == AECHG_NAME );
      InbasketFolderNameChanged( hwnd, pcl );
}

/* This function handles the WM_TOPICCHANGED message.
 */
void FASTCALL InBasket_OnTopicChanged( HWND hwnd, int type, PTL ptl )
{
   if( type == AECHG_FLAGS )
      {
      HTREEITEM hTopicItem;
      HWND hwndTreeCtl;

      /* Is the topic visible?
       */
      hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
      hTopicItem = Amdb_FindTopicItem( hwndTreeCtl, ptl );
      if( 0 != hTopicItem )
         {
         TV_ITEM tv;

         /* Get the topic visible rectangle and if it isn't
          * empty, update it.
          */
         tv.hItem = hTopicItem;
         tv.mask = TVIF_RECT;
         VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
         if( !IsRectEmpty( &tv.rcItem ) )
            {
            InvalidateRect( hwndTreeCtl, &tv.rcItem, TRUE );
            UpdateWindow( hwndTreeCtl );
            }
         }
      }
   else if( type == AECHG_NAME )
      InbasketTopicNameChanged( hwnd, ptl );
}

/* This function handles the WM_APPCOLOURCHANGE message.
 */
void FASTCALL InBasket_OnAppColourChange( HWND hwnd, int id )
{
   if( id == WIN_INBASK )
      {
      HWND hwndList;

      if( NULL != hbrInBaskWnd )
         DeleteBrush( hbrInBaskWnd );
      hbrInBaskWnd = CreateSolidBrush( crInBaskWnd );
      hwndList = GetDlgItem( hwnd, IDD_FOLDERLIST );
      TreeView_SetTextColor( hwndList, crInBaskText );
      TreeView_SetBackColor( hwndList, crInBaskWnd );
      InvalidateRect( hwndList, NULL, TRUE );
      UpdateWindow( hwndList );
      }
}

/* This function handles the WM_INSERTCATEGORY message.
 */
void FASTCALL InBasket_OnInsertCategory( HWND hwnd, PCAT pcat )
{
   HTREEITEM htreeCatItem;
   HWND hwndTreeCtl;
   PCAT pcatPrev;
   PCL pcl;
   PTL ptl;

   /* Disable updates in the control.
    */
   hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
   SetWindowRedraw( hwndTreeCtl, FALSE );

   /* Insert the folder into the tree.
    */
   ASSERT( 0 != pcat );
   if( pcatPrev = Amdb_GetPreviousCategory( pcat ) )
      {
      htreeCatItem = Amdb_FindCategoryItem( hwndTreeCtl, pcatPrev );
      ASSERT( 0 != htreeCatItem );
      }
   else
      htreeCatItem = TVI_LAST;
   InsertCategory( hwndTreeCtl, pcat, ( Amdb_GetCategoryFlags( pcat, DF_EXPANDED ) == DF_EXPANDED ), htreeCatItem );

   /* Insert all folders and topics for this category.
    */
   for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
      {
      HTREEITEM htreeConfItem;

      htreeConfItem = InsertFolder( hwndTreeCtl, htreeCatItem, pcl, ( Amdb_GetFolderFlags( pcl, CF_EXPANDED ) == CF_EXPANDED ), TVI_LAST );
      for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
         InsertTopic( hwndTreeCtl, htreeConfItem, ptl, TVI_LAST );
      }

   /* Now update the control.
    */
   SetWindowRedraw( hwndTreeCtl, !fUpdatesBlocked );
}

/* This function handles the WM_DELETECATEGORY message.
 */
void FASTCALL InBasket_OnDeleteCategory( HWND hwnd, BOOL fMove, PCAT pcat )
{
   HWND hwndTreeCtl;
   HTREEITEM htreeCatItem;

   hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
   ASSERT( NULL != pcat );
   htreeCatItem = Amdb_FindCategoryItem( hwndTreeCtl, pcat );
   SetWindowRedraw( hwndTreeCtl, FALSE );
   if( 0 != htreeCatItem )
      TreeView_DeleteItem( hwndTreeCtl, htreeCatItem );
   if( fMove )
      {
      HTREEITEM htreeCatItem;
      PCAT pcatPrev;
      PCL pcl;
      PTL ptl;

      /* Insert the category in its new position.
       */
      if( pcatPrev = Amdb_GetPreviousCategory( pcat ) )
         {
         htreeCatItem = Amdb_FindCategoryItem( hwndTreeCtl, pcatPrev );
         ASSERT( 0 != htreeCatItem );
         }
      else
         htreeCatItem = TVI_LAST;
      htreeCatItem = InsertCategory( hwndTreeCtl, pcat, ( Amdb_GetCategoryFlags( pcat, DF_EXPANDED ) == DF_EXPANDED ), htreeCatItem );

      /* Insert all folders and topics for this category.
       */
      for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
         {
         HTREEITEM htreeConfItem;

         htreeConfItem = InsertFolder( hwndTreeCtl, htreeCatItem, pcl, ( Amdb_GetFolderFlags( pcl, CF_EXPANDED ) == CF_EXPANDED ), TVI_LAST );
         for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
            InsertTopic( hwndTreeCtl, htreeConfItem, ptl, TVI_LAST );
         }
      }
   SaveRules();
   SetWindowRedraw( hwndTreeCtl, TRUE );

}

/* This function handles the WM_INSERTFOLDER message.
 */
void FASTCALL InBasket_OnInsertFolder( HWND hwnd, PCL pcl )
{
   HWND hwndTreeCtl;
   HTREEITEM htreeCatItem;
   HTREEITEM htreeConfItem;
   HTREEITEM hInsertAfter;
   PCAT pcat;
   PCL pclPrev;
   PTL ptl;

   /* Disable updates in the control.
    */
   hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
   SetWindowRedraw( hwndTreeCtl, FALSE );

   /* Insert the folder into the tree.
    */
   ASSERT( 0 != pcl );
   pcat = Amdb_CategoryFromFolder( pcl );
   htreeCatItem = Amdb_FindCategoryItem( hwndTreeCtl, pcat );
   ASSERT( 0 != htreeCatItem );
   if( pclPrev = Amdb_GetPreviousFolder( pcl ) )
      {
      hInsertAfter = Amdb_FindFolderItem( hwndTreeCtl, pclPrev );
      ASSERT( 0 != hInsertAfter );
      }
   else
      hInsertAfter = TVI_FIRST;
   htreeConfItem = InsertFolder( hwndTreeCtl, htreeCatItem, pcl, ( Amdb_GetFolderFlags( pcl, CF_EXPANDED ) == CF_EXPANDED ), hInsertAfter );

   /* Insert all topics for this folder.
    */
   for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
      InsertTopic( hwndTreeCtl, htreeConfItem, ptl, TVI_LAST );

   /* Now update the control.
    */
   SetWindowRedraw( hwndTreeCtl, !fUpdatesBlocked );
}

/* This function handles the WM_DELETEFOLDER message.
 */
void FASTCALL InBasket_OnDeleteFolder( HWND hwnd, BOOL fMove, PCL pcl )
{
   HWND hwndTreeCtl;
   HTREEITEM hConfItem;

   hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
   ASSERT( NULL != pcl );
   if( fMoving || fMove && !fKeepAtTopFolder )
      hConfItem = Amdb_FindFolderItemInCategory( hwndTreeCtl, pcatOriginalCategory, pcl );
   else
      hConfItem = Amdb_FindFolderItem( hwndTreeCtl, pcl );
   SetWindowRedraw( hwndTreeCtl, FALSE );
   if( 0 != hConfItem )
      TreeView_DeleteItem( hwndTreeCtl, hConfItem );
   if( fMove )
      {
      HTREEITEM htreeCatItem;
      HTREEITEM htreeConfItem;
      HTREEITEM hInsertAfter;
      PCL pclPrev;
      PCAT pcat;
      PTL ptl;

      pcat = Amdb_CategoryFromFolder( pcl );
      htreeCatItem = Amdb_FindCategoryItem( hwndTreeCtl, pcat );
      ASSERT( 0 != htreeCatItem );
      if( pclPrev = Amdb_GetPreviousFolder( pcl ) )
         {
         hInsertAfter = Amdb_FindFolderItem( hwndTreeCtl, pclPrev );
         ASSERT( 0 != hInsertAfter );
         }
      else
         hInsertAfter = TVI_FIRST;
      htreeConfItem = InsertFolder( hwndTreeCtl, htreeCatItem, pcl, ( Amdb_GetFolderFlags( pcl, CF_EXPANDED ) == CF_EXPANDED ), hInsertAfter );

      /* If this is the folder we are storing new newsgroups in, 
       * update and save the new setting
       */
      if( pcl == pclNewsFolder )
         {
         WriteFolderPathname( lpTmpBuf, pcl );
         Amuser_WritePPString( szPreferences, "NewsFolder", lpTmpBuf );
         pclNewsFolder = pcl;
         }

      /* Insert all topics for this folder.
       */
      for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
         {
         InsertTopic( hwndTreeCtl, htreeConfItem, ptl, TVI_LAST );

         /* If moving the postmaster folder, update and save the 
          * new postmaster setting
          */
         if( ptl == ptlPostmaster )
            {
            char sz[ 256 ];

            WriteFolderPathname( sz, ptl );
            Amuser_WritePPString( szCIXIP, "Postmaster", sz );
            ptlPostmaster = ptl;
            }
         }
      }
   fKeepAtTopFolder = FALSE;
   SaveRules();
   SetWindowRedraw( hwndTreeCtl, TRUE );
}

/* This function handles the WM_INSERTTOPIC message.
 */
void FASTCALL InBasket_OnInsertTopic( HWND hwnd, PTL ptl )
{
   HWND hwndTreeCtl;
   HTREEITEM hConfItem;
   HTREEITEM hInsertAfter;
   PTL ptlPrev;
   
   hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
   ASSERT( NULL != ptl );
   hConfItem = Amdb_FindFolderItem( hwndTreeCtl, Amdb_FolderFromTopic( ptl ) );
   ASSERT( 0 != hConfItem );
   if( ptlPrev = Amdb_GetPreviousTopic( ptl ) )
      {
      hInsertAfter = Amdb_FindTopicItem( hwndTreeCtl, ptlPrev );
      ASSERT( 0 != hInsertAfter );
      }
   else
      hInsertAfter = TVI_FIRST;
   InsertTopic( hwndTreeCtl, hConfItem, ptl, hInsertAfter );
}

/* This function handles the WM_DELETETOPIC message.
 */
void FASTCALL InBasket_OnDeleteTopic( HWND hwnd, BOOL fMove, PTL ptl )
{
   HWND hwndTreeCtl;
   HTREEITEM hTopicItem;

   hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
   ASSERT( NULL != ptl );
   if( fMoving || fMove)
      hTopicItem = Amdb_FindTopicItemInFolder( hwndTreeCtl, pclOriginalFolder, ptl );
   else
      hTopicItem = Amdb_FindTopicItem( hwndTreeCtl, ptl );
   SetWindowRedraw( hwndTreeCtl, FALSE );
   if( 0 != hTopicItem )
      TreeView_DeleteItem( hwndTreeCtl, hTopicItem );
   if( fMove )
      {
      HTREEITEM hInsertAfter;
      HTREEITEM hConfItem;
      PTL ptlPrev;

      /* We're moving the topic, so find the new position and
       * insert it into the tree.
       */
      hConfItem = Amdb_FindFolderItem( hwndTreeCtl, Amdb_FolderFromTopic( ptl ) );
      ASSERT( 0 != hConfItem );
      if( ptlPrev = Amdb_GetPreviousTopic( ptl ) )
         {
         hInsertAfter = Amdb_FindTopicItem( hwndTreeCtl, ptlPrev );
         ASSERT( 0 != hInsertAfter );
         }
      else
         hInsertAfter = TVI_FIRST;
      InsertTopic( hwndTreeCtl, hConfItem, ptl, hInsertAfter );
      }
   SaveRules();
   SetWindowRedraw( hwndTreeCtl, TRUE );
}

/* This function handles the WM_SETTOPIC message.
 */
void FASTCALL InBasket_OnSetTopic( HWND hwnd, PTL ptl )
{
   HWND hwndTreeCtl;
   HTREEITEM hNewTopicItem;
   HTREEITEM hOldTopicItem;
   HTREEITEM hNewFolderItem;
   HTREEITEM hOldFolderItem;
   HTREEITEM hNewCategoryItem;
   HTREEITEM hOldCategoryItem;
   
   /* Get list box handle.
    */   
   hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );

   /* Select the folder with the new topic. The folder
    * will automatically be expanded.
    */
   ASSERT( NULL != ptl );
   hNewTopicItem = Amdb_FindTopicItem( hwndTreeCtl, ptl );
   ASSERT( 0 != hNewTopicItem );

   if( NULL == ptlLastTopic )
      ptlLastTopic = ptl;

   if( ptl != ptlLastTopic )
      hOldTopicItem = Amdb_FindTopicItem( hwndTreeCtl, ptlLastTopic );
   else
      hOldTopicItem = TreeView_GetSelection( hwndTreeCtl );


   /* If auto collapse enabled AND the new topic folder is
    * different from the old topic folder, collapse the old
    * topic folder.
    */
   if( !fInitialising )
   if( fAutoCollapse )
      if( fCompatFlag & COMF_COLLAPSECATEGORIES )
      {
      if( 0L != hOldTopicItem )
         if( 0L != ( hOldFolderItem = TreeView_GetParent( hwndTreeCtl, hOldTopicItem ) ) )
            if( 0L != ( hOldCategoryItem = TreeView_GetParent( hwndTreeCtl, hOldFolderItem ) ) )
               if( 0L != ( hNewFolderItem = TreeView_GetParent( hwndTreeCtl, hNewTopicItem ) ) )
                  if( 0L != ( hNewCategoryItem = TreeView_GetParent( hwndTreeCtl, hNewFolderItem ) ) )
                     if( hNewCategoryItem != hOldCategoryItem )
                        TreeView_Expand( hwndTreeCtl, hOldCategoryItem, TVE_COLLAPSE );
                     else  if( hNewFolderItem != hOldFolderItem )
                        TreeView_Expand( hwndTreeCtl, hOldFolderItem, TVE_COLLAPSE );
      }
      else
      {

      if( 0L != hOldTopicItem )
         if( 0L != ( hOldFolderItem = TreeView_GetParent( hwndTreeCtl, hOldTopicItem ) ) )
               if( 0L != ( hNewFolderItem = TreeView_GetParent( hwndTreeCtl, hNewTopicItem ) ) )
                  if( hNewFolderItem != hOldFolderItem )
                        TreeView_Expand( hwndTreeCtl, hOldFolderItem, TVE_COLLAPSE );
      }


   /* Select the new topic.
    */
   TreeView_SelectItem( hwndTreeCtl, hNewTopicItem );
   if( NULL == hwndInBasket || hwnd == hwndInBasket )
      ptlLastTopic = ptl;

}

/* This function handles the WM_DRAGDROP message.
 */
void FASTCALL InBasket_OnDragDrop( HWND hwnd, UINT uDataType, LPDRAGDROPSTRUCT lpdds )
{
   TV_HITTESTINFO tvhi;
   TV_ITEM tv;

   /* Locate the drop folder.
    */
   tvhi.pt = lpdds->pt;
   if( tv.hItem = TreeView_HitTest( lpdds->hwndDrop, &tvhi ) )
      {
      HWND hwndTreeCtl;

      /* Now get the handle of the folder at the
       * hit test location.
       */
      tv.mask = TVIF_PARAM;
      hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
      TreeView_GetItem( hwndTreeCtl, &tv );
      if( Amdb_IsTopicPtr( (PTL)tv.lParam ) )
         {
         PTH FAR * lppth;
         BOOL fMove;
         int c;

         /* Count how many messages we're copying.
          */
         lppth = (PTH FAR *)lpdds->lpData;
         for( c = 0; lppth[ c ] != NULL; ++c );

         /* Call the copy range function to do the actual copy or
          * move.
          */
         fMove = ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) ? FALSE : TRUE;
         DoCopyMsgRange( hwndTopic, lppth, c, (PTL)tv.lParam, fMove, FALSE, FALSE );
         }
      }
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL InBasket_OnCreate( HWND hwnd, LPCREATESTRUCT lpCreateStruct )
{
   LPMDICREATESTRUCT lpMDICreateStruct;
   HWND hwndTreeCtl;
   HFONT hOldFont;
   HDC hdc;

   /* Create the dialog window.
    */
   lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
   Adm_MDIDialog( hwnd, MAKEINTRESOURCE(IDDLG_INBASKET), lpMDICreateStruct );

   /* Set the window font. This is needed, despite the fact that
    * we draw the labels, in order to set the item height. Later,
    * replace this with a WM_MEASUREITEM call.
    */
   VERIFY( hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST ) );
   SetWindowFont( hwndTreeCtl, hInBasketFont, 0L );

   /* Need some icons to go with it.
    */
   himl = Amdb_CreateFolderImageList();
   if( !himl )
      return( FALSE );
   TreeView_SetImageList( hwndTreeCtl, himl, 0 );

   /* Compute the width of the statistics field.
    */
   hdc = GetDC( hwnd );
   hOldFont = SelectFont( hdc, hInBasketBFont );
   GetTextExtentPoint( hdc, "00000/00000", 11, &sizeStat );
   SelectFont( hdc, hOldFont );
   ReleaseDC( hwnd, hdc );

   /* Subclass the listbox so we can handle the right mouse
    * button.
    */
   lpProcListCtl = SubclassWindow( GetDlgItem( hwnd, IDD_FOLDERLIST ), InBaskListProc );

   /* Create the out-basket window handle.
    */
   hbrInBaskWnd = CreateSolidBrush( crInBaskWnd );
   TreeView_SetTextColor( hwndTreeCtl, crInBaskText );
   TreeView_SetBackColor( hwndTreeCtl, crInBaskWnd );

   /* Now fill from the category.
    */
   Amdb_FillFolderTree( hwndTreeCtl, FTYPE_ALL|FTYPE_TOPICS, fShowTopicType );
   SetFocus( hwndTreeCtl );
   return( TRUE );
}

/* This function intercepts messages for the out-basket list box window
 * so that we can handle the right mouse button.
 */
LRESULT EXPORT FAR PASCAL InBaskListProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   switch( msg )
      {
      case WM_ERASEBKGND: {
         COLORREF bkTmp;
         RECT rc;

         GetClientRect( hwnd, &rc );
         bkTmp = SetBkColor( (HDC)wParam, crInBaskWnd );
         ExtTextOut( (HDC)wParam, rc.left, rc.top, ETO_OPAQUE, &rc, "", 0, NULL );
         SetBkColor( (HDC)wParam, bkTmp );
         return( TRUE );
         }
      }
   return( CallWindowProc( lpProcListCtl, hwnd, msg, wParam, lParam ) );
}

/* This function handles the WM_MDIACTIVATE message.
 */
void FASTCALL InBasket_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_INBASK ), (LPARAM)(LPSTR)hwnd );
   if( fActive )
      ToolbarState_EditWindow();
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SETFOCUS message. Whenever the message
 * viewer window gets the focus, we transfer it to the thread pane.
 */
void FASTCALL InBasket_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   SetFocus( GetDlgItem( hwnd, IDD_FOLDERLIST ) );
}

/* This function inserts a category into the in-basket.
 */
HTREEITEM FASTCALL InsertCategory( HWND hwndTreeCtl, PCAT pcat, BOOL fExpanded, HTREEITEM htreeInsertAfter )
{
   TV_INSERTSTRUCT tvins;
   CATEGORYINFO catinfo;

   Amdb_GetCategoryInfo( pcat, &catinfo );
   tvins.hParent = 0;
   tvins.hInsertAfter = htreeInsertAfter;
   tvins.item.pszText = (LPSTR)Amdb_GetCategoryName( pcat );
   tvins.item.mask = TVIF_TEXT|TVIF_IMAGE|TVIF_STATE;
   tvins.item.state = fExpanded ? TVIS_EXPANDED : 0;
   tvins.item.iImage = I_IMAGECALLBACK;
   tvins.item.iSelectedImage = I_IMAGECALLBACK;
   tvins.item.lParam = (LPARAM)pcat;
   tvins.item.cChildren = catinfo.cFolders;
   return( TreeView_InsertItem( hwndTreeCtl, &tvins ) );
}

/* This function inserts a folder into the in-basket.
 */
HTREEITEM FASTCALL InsertFolder( HWND hwndTreeCtl, HTREEITEM htreeConfItem, PCL pcl, BOOL fExpanded, HTREEITEM htreeInsertAfter )
{
   TV_INSERTSTRUCT tvins;
   FOLDERINFO foldinfo;

   Amdb_GetFolderInfo( pcl, &foldinfo );
   tvins.hInsertAfter = htreeInsertAfter;
   tvins.hParent = htreeConfItem;
   tvins.item.pszText = (LPSTR)Amdb_GetFolderName( pcl );
   tvins.item.mask = TVIF_TEXT|TVIF_IMAGE|TVIF_STATE;
   tvins.item.state = fExpanded ? TVIS_EXPANDED : 0;
   tvins.item.iImage = I_IMAGECALLBACK;
   tvins.item.iSelectedImage = I_IMAGECALLBACK;
   tvins.item.lParam = (LPARAM)pcl;
   tvins.item.cChildren = foldinfo.cTopics;
   return( TreeView_InsertItem( hwndTreeCtl, &tvins ) );
}

/* This function inserts a topic into the in-basket.
 */
HTREEITEM FASTCALL InsertTopic( HWND hwndTreeCtl, HTREEITEM htreeTopicItem, PTL ptl, HTREEITEM htreeInsertAfter )
{
   TV_INSERTSTRUCT tvins;

   tvins.hInsertAfter = htreeInsertAfter;
   tvins.hParent = htreeTopicItem;
   tvins.item.pszText = (LPSTR)Amdb_GetTopicName( ptl );
   tvins.item.mask = TVIF_TEXT|TVIF_IMAGE;
   tvins.item.state = 0;
   tvins.item.iImage = fShowTopicType ? GetTopicTypeImage( ptl ) : I_NOIMAGE;
   tvins.item.iSelectedImage = tvins.item.iImage;
   tvins.item.lParam = (LPARAM)ptl;
   tvins.item.cChildren = 0;
   return( TreeView_InsertItem( hwndTreeCtl, &tvins ) );
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL InBasket_OnAdjustWindows( HWND hwnd, int dx, int dy )
{
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_FOLDERLIST ), dx, dy );
}

/* This function handles the WM_CLOSE message.
 */
void FASTCALL InBasket_OnClose( HWND hwnd )
{
   Amctl_ImageList_Destroy( himl );
   DeleteBrush( hbrInBaskWnd );
   Adm_DestroyMDIWindow( hwnd );
   hwndInBasket = NULL;
   himl = NULL;
}

/* This function draws a treeview label.
 */
void FASTCALL InBasket_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpdis )
{
   TV_DRAWITEM FAR * lptvdi;
   LPVOID pFolder;
   COLORREF crText;
   COLORREF crBack;
   COLORREF T;
   TOPICINFO ti;
   char szLabel[ 100 ];
   char sz[ 20 ];
   HFONT hOldFont;
   RECT rcLabel;
   RECT rc;
   SIZE size;
   int x, y;

   /* Shut compiler up */
   ti.dwFlags = 0;
   ti.cUnRead = 0;
   ti.cMsgs = 0;
   rc.left = 0;

   /* Get a pointer to the TV_DRAWITEM struct that
    * has more details than the DRAWITEMSTRUCT structure.
    */
   lptvdi = (TV_DRAWITEM FAR *)lpdis->itemData;
   pFolder = (LPVOID)lptvdi->tv.lParam;

   /* If this is a topic, get the topic information.
    */
   if( Amdb_IsTopicPtr( (PTL)pFolder ) )
      Amdb_GetTopicInfo( (PTL)pFolder, &ti );

   /* Compute the position of the text.
    */
   hOldFont = SelectFont( lpdis->hDC, hInBasketFont );

   /* Set the label colours depending on whether it is selected or
    * unselected, a conference, category or topic and, if a topic,
    * the topic status.
    */
   if( lpdis->itemState & ODS_SELECTED )
      {
      crText = SetTextColor( lpdis->hDC, crHighlightText );
      crBack = SetBkColor( lpdis->hDC, crHighlight );

//    Looks ugly! pw 16/11/98
//    if( !( lpdis->itemState & ODS_FOCUS ) )
//       crBack = SetBkColor( lpdis->hDC, GetSysColor( COLOR_BTNFACE ) );
      }
   else
      {
      COLORREF crNewText;

      crNewText = crInBaskText;
      if( Amdb_IsTopicPtr( (PTL)pFolder ) )
         {
         WORD wTopicType = 3;

         if( fDisplayLockCount )
         {
            if( Amdb_GetLockCount( (PTL)pFolder ) > 0 )
               crNewText = crResumeText;
         }
         else
         {



         if( !fNoCMN )
            wTopicType = Amdb_GetTopicType( (PTL)pFolder );

         if( ti.dwFlags & TF_RESIGNED )
            crNewText = crResignedTopic;
         else if( ti.dwFlags & TF_IGNORED )
            crNewText = crIgnoredTopic;
         else if( wTopicType == FTYPE_MAIL )
            crNewText = crMail;
         else if( wTopicType == FTYPE_NEWS )
            crNewText = crNews;
         }
         }
      else if( Amdb_IsFolderPtr( (PCL)pFolder ) )
         {
         if( Amdb_IsResignedFolder( (PCL)pFolder ) )
            crNewText = crResignedTopic;
         else if( !fNoCMN && (PCL)pFolder == pclNewsFolder )
            crNewText = crNews;
         else if( !fNoCMN && NULL != ptlPostmaster && Amdb_FolderFromTopic( ptlPostmaster ) == (PCL)pFolder )
            crNewText = crMail;
         else if( !fNoCMN && fUseCIX && Amdb_IsModerator( (PCL)pFolder, szCIXNickname ) )
            crNewText = crModerated;
         else if( Amdb_GetFolderFlags( (PCL)pFolder, CF_KEEPATTOP ) == CF_KEEPATTOP )
            crNewText = crKeepAtTop;
         }
      crText = SetTextColor( lpdis->hDC, crNewText );
      crBack = SetBkColor( lpdis->hDC, crInBaskWnd );
      }
   T = crInBaskText;

   /* Compute the initial label width.
    */
   GetTextExtentPoint( lpdis->hDC, lptvdi->pszText, lstrlen( lptvdi->pszText ), &size );
   rcLabel = lpdis->rcItem;
   x = rcLabel.left + 3;
   y = rcLabel.top + ( ( rcLabel.bottom - rcLabel.top ) - size.cy ) / 2;

   /* If we're drawing the drag label, stop now.
    */
   if( lpdis->itemState & 0x8000 )
      sz[ 0 ] = '\0';
   else
      {
      register int c;

      /* Initialise.
       */
      rc = lpdis->rcItem;
      c = 0;

      /* Display the flags.
       */
      if( Amdb_IsTopicPtr( (PTL)pFolder ) )
         {
         if( fDisplayLockCount )
            c += wsprintf( sz, "%u ", Amdb_GetLockCount( (PTL)pFolder ) );
         if( Amdb_GetTopicType( (PTL)pFolder ) == FTYPE_LOCAL || (ti.dwFlags & (TF_HASFILES|TF_READONLY)) )
            {
            sz[ c++ ] = '(';
            if( Amdb_GetTopicType( (PTL)pFolder ) == FTYPE_LOCAL )
               sz[ c++ ] = 'L';
            if( ti.dwFlags & TF_HASFILES )
               sz[ c++ ] = 'F';
            if( ti.dwFlags & TF_READONLY )
               sz[ c++ ] = 'R';
            sz[ c++ ] = ')';
            sz[ c++ ] = ' ';
            sz[ c++ ] = ' ';
            }
         }
      sz[ c ] = '\0';

      /* If this is the lowest visible level item, then
       * display extra statistics.
       */
      if( Amdb_IsCategoryPtr( (LPVOID)pFolder ) && !( lptvdi->tv.state & TVIS_EXPANDED ) )
         {
         CATEGORYINFO cai;
   
         Amdb_GetCategoryInfo( (PCAT)pFolder, &cai );
         wsprintf( sz + c, "%lu/%lu", min( cai.cUnRead, 9999999 ), min( cai.cMsgs, 9999999 ) );
         if( cai.cUnRead )
            T = crUnReadMsg;
         }
      else if( Amdb_IsFolderPtr( (LPVOID)pFolder ) && !( lptvdi->tv.state & TVIS_EXPANDED ) )
         {
         FOLDERINFO ci;
   
         Amdb_GetFolderInfo( (PCL)pFolder, &ci );
         wsprintf( sz + c, "%lu/%lu", min( ci.cUnRead, 9999999 ), min( ci.cMsgs, 9999999 ) );
         if( ci.cUnRead )
            T = crUnReadMsg;
         }
      else if( Amdb_IsTopicPtr( (LPVOID)pFolder ) )
         {
         wsprintf( sz + c, "%lu/%lu", min( ti.cUnRead, 9999999 ), min( ti.cMsgs, 9999999 ) );
         if( ti.cUnRead )
            T = crUnReadMsg;
         }

      /* Compute the statistic width.
       */
      GetTextExtentPoint( lpdis->hDC, sz, strlen(sz), &size );
      rc.right = lpdis->rcItem.right;
      rc.left = rc.right - ( size.cx + 10 );
      }

   /* Adjust the label to fit, if needed.
    * Don't bother if dragging label.
    */
   strcpy( szLabel, lptvdi->pszText );
   if( !( lpdis->itemState & 0x8000 ) )
      AdjustNameToFit( lpdis->hDC, szLabel, ( rc.left - x ) - 10, TRUE, &size );

   /* Draw the label.
    */
   rcLabel.right = rcLabel.left + size.cx + 6;
   ExtTextOut( lpdis->hDC, x, y, ETO_OPAQUE, &rcLabel, szLabel, strlen(szLabel), NULL );

   /* Draw the statistics.
    */
   if( *sz )
      {
      /* If the treeview control has the focus, then draw
       * a focus rectangle around the selected item.
       */
      if( lpdis->itemState & ODS_FOCUS )
         DrawFocusRect( lpdis->hDC, &rcLabel );
   
      /* Restore the background colour.
       */
      SetTextColor( lpdis->hDC, T );
      SetBkColor( lpdis->hDC, crInBaskWnd );
      ExtTextOut( lpdis->hDC, rc.left, y, ETO_OPAQUE, &rc, sz, strlen( sz ), NULL );
      }

   /* Clean up before we leave.
    */
   SetBkColor( lpdis->hDC, crBack );
   SetTextColor( lpdis->hDC, crText );
   SelectFont( lpdis->hDC, hOldFont );
}

/* This function handles the WM_CTLCOLOR message.
 * BUGBUG: This code never seems to be called.
 */
HBRUSH FASTCALL InBasket_OnCtlColor( HWND hwnd, HDC hdc, HWND hwndChild, int type )
{
   switch( GetDlgCtrlID( hwndChild ) )
      {
      case IDD_FOLDERLIST:
         SetBkColor( hdc, crInBaskWnd );
         SetTextColor( hdc, crInBaskText );
         return( hbrInBaskWnd );
      }
   return( NULL );
}

/* This function handles the WM_MOUSEMOVE message.
 */
void FASTCALL InBasket_OnMouseMove( HWND hwnd, int x, int y, UINT keyFlags )
{
   if( fDragging )
      {
      TV_HITTESTINFO tv;
      HWND hwndTreeCtl;
      HTREEITEM hSelHit;

      /* Update the position of the cursor.
       */
      hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
      tv.pt.x = x;
      tv.pt.y = y;
      MapWindowPoints( hwnd, hwndTreeCtl, &tv.pt, 1 );
      if( ( hSelHit = TreeView_HitTest( hwndTreeCtl, &tv ) ) != 0 )
         {
         Amctl_ImageList_DragLeave( hwndTreeCtl );
         TreeView_SelectDropTarget( hwndTreeCtl, hSelHit );
         }
      else if( tv.flags & (TVHT_ABOVE|TVHT_BELOW) )
         {
         DWORD dwTicksSinceLastScroll;

         dwTicksSinceLastScroll = GetTickCount() - dwLastScrollTime;
         if( dwTicksSinceLastScroll >= 50 )
            {
            Amctl_ImageList_DragLeave( hwndTreeCtl );

            if( tv.flags & TVHT_ABOVE )
               SendMessage( hwndTreeCtl, WM_VSCROLL, SB_LINEUP, 0L );
            else
               SendMessage( hwndTreeCtl, WM_VSCROLL, SB_LINEDOWN, 0L );

            dwLastScrollTime = GetTickCount();
            }
         }
      Amctl_ImageList_DragMove( tv.pt.x, tv.pt.y );
      }
}

/* This function handles the WM_CANCELMODE message.
 * If we're dragging, we just cancel dragging and do nothing
 * with the image we're dragging.
 */
void FASTCALL InBasket_OnCancelMode( HWND hwnd )
{
   if( fDragging )
      {
      HWND hwndTreeCtl;

      /* End dragging.
       */
      KillTimer( hwnd, ID_DRAGTIMER );
      Amctl_ImageList_EndDrag();
      ReleaseCapture();
      ShowCursor( TRUE );
      fDragging = FALSE;

      /* Select the original folder.
       */
      hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
      TreeView_SelectItem( hwndTreeCtl, hDragItem );
      }
}

/* This function handles the WM_LBUTTONUP message.
 */
void FASTCALL InBasket_OnLButtonUp( HWND hwnd, int x, int y, UINT keyFlags )
{
   if( fDragging )
      {
      TV_HITTESTINFO tv;
      HWND hwndTreeCtl;
      HTREEITEM hSelHit;

      /* First clean up...
       */
      KillTimer( hwnd, ID_DRAGTIMER );
      Amctl_ImageList_EndDrag();
      ReleaseCapture();
      ShowCursor( TRUE );
      fDragging = FALSE;

      /* Now where did we drop the selection?
       */
      hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
      tv.pt.x = x;
      tv.pt.y = y;
      MapWindowPoints( hwnd, hwndTreeCtl, &tv.pt, 1 );
      if( ( hSelHit = TreeView_HitTest( hwndTreeCtl, &tv ) ) != 0 )
         {
         TV_ITEM tvDest;
         TV_ITEM tvSrc;

         /* Did we drop back on ourselves? If so, exit now
          * since nothing is supposed to happen.
          */
         if( hDragItem == hSelHit )
            return;

         /* Get some information about the item at the
          * drop position.
          */
         tvDest.hItem = hSelHit;
         tvDest.mask = TVIF_PARAM;
         VERIFY( TreeView_GetItem( hwndTreeCtl, &tvDest ) );

         /* Get some information about the item being dragged
          */
         tvSrc.hItem = hDragItem;
         tvSrc.mask = TVIF_PARAM;
         VERIFY( TreeView_GetItem( hwndTreeCtl, &tvSrc ) );

         /* Handle moving categories.
          */
         if( Amdb_IsCategoryPtr( (PCAT)tvSrc.lParam ) )
            {
            int iMoveError;

            /* No error.
             */
            iMoveError = AMDBERR_NOERROR;

            /* Did we drop over a topic? Insert the category
             * after the category belonging to the topic.
             */
            if( Amdb_IsTopicPtr( (PTL)tvDest.lParam ) )
               {
               PCAT pcat;
               PCL pcl;

               pcl = Amdb_FolderFromTopic( (PTL)tvDest.lParam );
               pcat = Amdb_CategoryFromFolder( pcl );
               if( pcat != (PCAT)tvSrc.lParam )
                  iMoveError = Amdb_MoveCategory( (PCAT)tvSrc.lParam, pcat );
               }
            else if( Amdb_IsFolderPtr( (PCL)tvDest.lParam ) )
               {
               PCAT pcat;
   
               pcat = Amdb_CategoryFromFolder( (PCL)tvDest.lParam );
               if( pcat != (PCAT)tvSrc.lParam )
                  iMoveError = Amdb_MoveCategory( (PCAT)tvSrc.lParam, pcat );
               }
            else
               iMoveError = Amdb_MoveCategory( (PCAT)tvSrc.lParam, (PCAT)tvDest.lParam );

            /* Report error if this is an abuse of privileges!
             */
            if( AMDBERR_NOPERMISSION == iMoveError )
               fMessageBox( hwnd, 0, GS(IDS_STR852), MB_OK|MB_ICONEXCLAMATION );
            else if( AMDBERR_NOERROR == iMoveError )
               hDragItem = Amdb_FindCategoryItem( hwndTreeCtl, (PCAT)tvSrc.lParam );
            ASSERT( hDragItem != 0 );
            TreeView_SelectItem( hwndTreeCtl, hDragItem );
            }

         /* Handle moving folders.
          */
         else if( Amdb_IsFolderPtr( (PCL)tvSrc.lParam ) )
            {
            int iMoveError;

            /* No error.
             */
            iMoveError = AMDBERR_NOERROR;

            /* Save the parent folder of the topic being
             * moved.
             */
            fMoving = TRUE;
            pcatOriginalCategory = Amdb_CategoryFromFolder( (PCL)tvSrc.lParam );

            /* Did we drop over a topic? This is an error - we can't
             * insert folders within folders!
             */
            if( Amdb_IsTopicPtr( (PTL)tvDest.lParam ) )
               {
               fMessageBox( hwnd, 0, GS(IDS_STR851), MB_OK|MB_ICONEXCLAMATION );
               TreeView_SelectItem( hwndTreeCtl, hDragItem );
               return;
               }

            /* If we dropped over a folder, insert the new folder after that
             * folder.
             */
            else if( Amdb_IsFolderPtr( (PCL)tvDest.lParam ) )
               {
               PCAT pcat;
   
               pcat = Amdb_CategoryFromFolder( (PCL)tvDest.lParam );
               iMoveError = Amdb_MoveFolder( (PCL)tvSrc.lParam, pcat, (PCL)tvDest.lParam );
               }

            /* If we dropped over a category, move the topic to that category.
             */
            else if( Amdb_IsCategoryPtr( (PCAT)tvDest.lParam ) )
               iMoveError = Amdb_MoveFolder( (PCL)tvSrc.lParam, (PCAT)tvDest.lParam, NULL );

            /* Report error if this is an abuse of privileges!
             */
            fMoving = FALSE;
            if( AMDBERR_NOPERMISSION == iMoveError )
               fMessageBox( hwnd, 0, GS(IDS_STR852), MB_OK|MB_ICONEXCLAMATION );
            else
               hDragItem = Amdb_FindFolderItem( hwndTreeCtl, (PCL)tvSrc.lParam );
            ASSERT( hDragItem != 0 );
            TreeView_SelectItem( hwndTreeCtl, hDragItem );
            }

         /* Handle moving topics.
          */
         else
            {
            int iMoveError;
            PCL pcl;

            /* No error.
             */
            iMoveError = AMDBERR_NOERROR;

            /* Get the folder handle.
             */
            if( Amdb_IsTopicPtr( (PTL)tvDest.lParam ) )
               pcl = Amdb_FolderFromTopic( (PTL)tvDest.lParam );
            else if( Amdb_IsFolderPtr( (PCL)tvDest.lParam ) )
               pcl = (PCL)tvDest.lParam;
            else
               return;

            /* Prohibit moving CIX topics outside their folder!
             */
            if( Amdb_GetTopicType( (PTL)tvSrc.lParam ) == FTYPE_CIX )
               if( pcl != Amdb_FolderFromTopic( (PTL)tvSrc.lParam ) )
                  {
                  fMessageBox( hwnd, 0, GS(IDS_STR537), MB_OK|MB_ICONINFORMATION );
                  TreeView_SelectItem( hwndTreeCtl, hDragItem );
                  return;
                  }

            /* Save the parent folder of the topic being
             * moved.
             */
            fMoving = TRUE;
            pclOriginalFolder = Amdb_FolderFromTopic( (PTL)tvSrc.lParam );

            /* Did we drop over a topic? Insert the new item after that
             * topic.
             */
            if( Amdb_IsTopicPtr( (PTL)tvDest.lParam ) )
               iMoveError = Amdb_MoveTopic( (PTL)tvSrc.lParam, pcl, (PTL)tvDest.lParam );

            /* If we dropped over a folder, move the topic to that folder.
             */
            else if( Amdb_IsFolderPtr( (PCL)tvDest.lParam ) )
               iMoveError = Amdb_MoveTopic( (PTL)tvSrc.lParam, (PCL)tvDest.lParam, NULL );

            /* Report error if this is an abuse of privileges!
             */
            fMoving = FALSE;
            if( AMDBERR_NOPERMISSION == iMoveError )
               fMessageBox( hwnd, 0, GS(IDS_STR853), MB_OK|MB_ICONEXCLAMATION );
            else
               hDragItem = Amdb_FindTopicItem( hwndTreeCtl, (PTL)tvSrc.lParam );
            ASSERT( hDragItem != 0 );
            TreeView_SelectItem( hwndTreeCtl, hDragItem );

            /* Moving a topic may change rules.
             */
            SaveRules();
            }
         }
      }
}

/* This function moves the selected folder to the specified
 * category.
 */
void FASTCALL InBasket_MoveFolder( LPVOID pFolder, HWND hwnd )
{
   HTREEITEM hSelItem;
   HWND hwndTreeCtl;
   
   hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );

   if( hSelItem = TreeView_GetSelection( hwndTreeCtl ) )
      {
      TV_ITEM tv;

      /* Get the lParam of the selected item.
       */
      tv.hItem = hSelItem;
      tv.mask = TVIF_PARAM;
      VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );

      /* Handle moving a folder to another category.
       * (Make sure we don't move it to its own category).
       */
      if( Amdb_IsCategoryPtr( (PCAT)pFolder ) )
         {
         if( Amdb_IsFolderPtr( (PCL)tv.lParam ) )
            if( Amdb_CategoryFromFolder( (PCL)tv.lParam ) != (PCAT)pFolder )
               {
               HTREEITEM hNextItem;
               int iMoveError;

               /* Save the parent folder of the folder being
                * moved.
                */
               fMoving = TRUE;
               pcatOriginalCategory = Amdb_CategoryFromFolder( (PCL)tv.lParam );

               /* Find the next or previous folder.
                */
               hNextItem = TreeView_GetNextSibling( hwndTreeCtl, hSelItem );
               if( 0L == hNextItem )
                  {
                  hNextItem = TreeView_GetPrevSibling( hwndTreeCtl, hSelItem );
                  if( 0L == hNextItem )
                     hNextItem = TreeView_GetParent( hwndTreeCtl, hSelItem );
                  }

               /* Move it then select the next folder.
                */
               fMoving = FALSE;
               iMoveError = Amdb_MoveFolder( (PCL)tv.lParam, (PCAT)pFolder, NULL );
               if( AMDBERR_NOPERMISSION == iMoveError )
                  fMessageBox( hwndFrame, 0, GS(IDS_STR853), MB_OK|MB_ICONEXCLAMATION );
               else
                  TreeView_SelectItem( hwndTreeCtl, hNextItem );
               }
         }
      }
}

/* This function handles notification messages from the
 * treeview control.
 */
LRESULT FASTCALL InBasket_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
{
   switch( lpNmHdr->code )
      {
      case TVN_SELCHANGED:
         ToolbarState_TopicSelected();
         break;

      case TVN_BEGINDRAG: {
         NM_TREEVIEW FAR * lpnmtv;
         HIMAGELIST himl;
         TV_ITEM tv;
         int xDiff;
         int yDiff;

         /* Save the handle of the item being dragged.
          */
         lpnmtv = (NM_TREEVIEW FAR *)lpNmHdr;
         hDragItem = lpnmtv->itemNew.hItem;

         /* We're dragging a label. Get the drag image and work out
          * from where we start dragging.
          */
         himl = TreeView_CreateDragImage( lpNmHdr->hwndFrom, lpnmtv->itemNew.hItem );
         tv.hItem = lpnmtv->itemNew.hItem;
         tv.mask = TVIF_RECT;
         VERIFY( TreeView_GetItem( lpNmHdr->hwndFrom, &tv ) );

         /* Initiate the drag action.
          */
         yDiff = tv.rcItem.top - lpnmtv->ptDrag.y;
         xDiff = ( tv.rcItem.left - lpnmtv->ptDrag.x ) - 16;
         Amctl_ImageList_BeginDrag( himl, 0, xDiff, yDiff );
         Amctl_ImageList_DragEnter( lpNmHdr->hwndFrom, lpnmtv->ptDrag.x, lpnmtv->ptDrag.y );
         ShowCursor( FALSE );
         SetCapture( hwnd );
         fDragging = TRUE;
         SetTimer( hwnd, ID_DRAGTIMER, 10, NULL );
         dwLastScrollTime = GetTickCount();
         return( 0L );
         }

      case TVN_ENDLABELEDIT: {
         TV_DISPINFO FAR * lptvdi;

         lptvdi = (TV_DISPINFO FAR *)lpNmHdr;
         if( lptvdi->item.pszText )
            {
            BOOL fExists;
            BOOL fOk;

            /* Make sure the new name is valid.
             */
            StripLeadingTrailingSpaces( lptvdi->item.pszText );
            if( !IsValidFolderName( hwnd, lptvdi->item.pszText ) )
               break;
            if( strchr( lptvdi->item.pszText, ' ' ) != NULL )
            {
               wsprintf( lpTmpBuf, GS(IDS_STR1154), ' ' );
               fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONINFORMATION );
               break;
            }

            /* For each type of folder, check that the name isn't already
             * in use before accepting it.
             */
            fExists = fOk = FALSE;
            if( *lptvdi->item.pszText )
               if( Amdb_IsTopicPtr( (PTL)lptvdi->item.lParam ) )
                  {
                  PTL ptl = NULL;
                  char szTopicName[ LEN_TOPICNAME + 1 ];
                  ptl = (PTL)lptvdi->item.lParam;
                  strcpy( szTopicName, Amdb_GetTopicName( ptl ) );
                  if( strcmp( lptvdi->item.pszText, szTopicName ) != 0 )
                  {
                     if( !( fExists = Amdb_OpenTopic( Amdb_FolderFromTopic( ptl ), lptvdi->item.pszText ) != NULL ) )
                        fOk = Amdb_SetTopicName( ptl, lptvdi->item.pszText ) == AMDBERR_NOERROR;
                     if( fOk )
                     {
                        Amdb_RenameCookies( szTopicName, ptl, TRUE );
                        if( ptlPostmaster == ptl )
                        {
                           WriteFolderPathname( lpTmpBuf, ptl );
                           Amuser_WritePPString( szCIXIP, "Postmaster", lpTmpBuf );
                        }
                     }
                  }

                  }
               else if( Amdb_IsFolderPtr( (PCL)lptvdi->item.lParam ) )
                  {
                  PCL pcl = NULL;
                  char szFolderName[ LEN_FOLDERNAME + 1 ];
                  pcl = (PCL)lptvdi->item.lParam;
                  strcpy( szFolderName, Amdb_GetFolderName( pcl ) );
                  if( strcmp( lptvdi->item.pszText, szFolderName ) != 0 )
                  {
                     if( !( fExists = Amdb_OpenFolder( NULL, lptvdi->item.pszText ) != NULL ) )
                        fOk = Amdb_SetFolderName( pcl, lptvdi->item.pszText ) == AMDBERR_NOERROR;
                     if( fOk )
                     {
                        PTL newptl;
                        newptl = pcl->ptlFirst;
                        while( newptl )
                        {
                        PTL ptlNext;

                        ptlNext = newptl->ptlNext;
                        Amdb_RenameCookies( szFolderName, newptl, FALSE );
                        newptl = ptlNext;
                        }

//                      for( first topic to last topic )
                        if( pclNewsFolder == pcl )
                        {
                           WriteFolderPathname( lpTmpBuf, pcl );
                           Amuser_WritePPString( szPreferences, "NewsFolder", lpTmpBuf );
                        }
                        if( ptlPostmaster != NULL && Amdb_FolderFromTopic( ptlPostmaster ) == pcl )
                        {
                           WriteFolderPathname( lpTmpBuf, ptlPostmaster );
                           Amuser_WritePPString( szCIXIP, "Postmaster", lpTmpBuf );
                        }
                     }
                  }
                  }
               else if( Amdb_IsCategoryPtr( (PCAT)lptvdi->item.lParam ) )
                  {
                  PCAT pcat;

                  pcat = (PCAT)lptvdi->item.lParam;
                  if( strcmp( lptvdi->item.pszText, Amdb_GetCategoryName( pcat ) ) != 0 )
                  {
                     if( !( fExists = Amdb_OpenCategory( lptvdi->item.pszText ) != NULL ) )
                        fOk = Amdb_SetCategoryName( pcat, lptvdi->item.pszText ) == AMDBERR_NOERROR;
                     if( fOk && pclNewsFolder != NULL && ( Amdb_CategoryFromFolder( pclNewsFolder ) == pcat ) )
                     {
                        WriteFolderPathname( lpTmpBuf, pclNewsFolder );
                        Amuser_WritePPString( szPreferences, "NewsFolder", lpTmpBuf );
                     }
                     if( fOk && ptlPostmaster != NULL && ( Amdb_CategoryFromFolder( Amdb_FolderFromTopic( ptlPostmaster ) ) == pcat ) )
                     {
                        WriteFolderPathname( lpTmpBuf, ptlPostmaster );
                        Amuser_WritePPString( szCIXIP, "Postmaster", lpTmpBuf );
                     }
                  }

                  }

            /* Throw up an error if the name was used, otherwise change the
             * label in the control.
             */
            if( fExists )
               fMessageBox( hwnd, 0, GS(IDS_STR801), MB_OK|MB_ICONEXCLAMATION );
            else if( fOk )
               TreeView_SetItem( lpNmHdr->hwndFrom, &lptvdi->item );
            }
         SaveRules();
         return( FALSE );
         }

      case NM_RCLICK:
         InBasket_RightClickMenu( hwnd, lpNmHdr->hwndFrom );
         return( TRUE );

      case TVN_ITEMEXPANDED: {
         NM_TREEVIEW FAR * lpnmtv;

         /* If a folder is expanded or closed, save the
          * state in the category.
          */
         lpnmtv = (NM_TREEVIEW FAR *)lpNmHdr;
         if( Amdb_IsFolderPtr( (PCL)lpnmtv->itemNew.lParam ) )
            Amdb_SetFolderFlags( (PCL)lpnmtv->itemNew.lParam, CF_EXPANDED, lpnmtv->action == TVE_EXPAND );
         else
            {
            Amdb_SetCategoryFlags( (PCAT)lpnmtv->itemNew.lParam, DF_EXPANDED, lpnmtv->action == TVE_EXPAND );
            if( lpnmtv->action == TVE_EXPAND )
               {
               PCL pcl;

               for( pcl = Amdb_GetFirstFolder( (PCAT)lpnmtv->itemNew.lParam ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
                  Amdb_SetFolderFlags( pcl, CF_EXPANDED, FALSE );
               }
            }
         return( TRUE );
         }

      case TVN_KEYDOWN: {
         TV_KEYDOWN FAR * lptvkd;

         lptvkd = (TV_KEYDOWN FAR *)lpNmHdr;
         switch( lptvkd->wVKey )
            {
            case VK_MENU:
               if( GetKeyState( VK_SHIFT ) & 0x8000 )
                  InBasket_RightClickMenu( hwnd, lpNmHdr->hwndFrom );
               break;

            case VK_LEFT: {
               HTREEITEM hSelItem;

               /* Get the previous visible item.
                */
               if( hSelItem = TreeView_GetSelection( lpNmHdr->hwndFrom ) )
                  {
                  hSelItem = TreeView_GetPrevVisible( lpNmHdr->hwndFrom, hSelItem );
                  while( hSelItem )
                     {
                     TV_ITEM tv;

                     /* Get the folder handle of the previous item. If it is not a
                      * sub-folder, then select it.
                      */
                     tv.hItem = hSelItem;
                     tv.mask = TVIF_PARAM;
                     VERIFY( TreeView_GetItem( lpNmHdr->hwndFrom, &tv ) );
                     if( !Amdb_IsTopicPtr( (PTL)tv.lParam ) )
                        {
                        TreeView_SelectItem( lpNmHdr->hwndFrom, hSelItem );
                        break;
                        }
                     hSelItem = TreeView_GetPrevVisible( lpNmHdr->hwndFrom, hSelItem );
                     }
                  }
               break;
               }

            case VK_RIGHT: {
               HTREEITEM hSelItem;

               /* Get the next visible item.
                */
               if( hSelItem = TreeView_GetSelection( lpNmHdr->hwndFrom ) )
                  {
                  hSelItem = TreeView_GetNextVisible( lpNmHdr->hwndFrom, hSelItem );
                  while( hSelItem )
                     {
                     TV_ITEM tv;

                     /* Get the folder handle of the next item. If it is not a
                      * sub-folder, then select it.
                      */
                     tv.hItem = hSelItem;
                     tv.mask = TVIF_PARAM;
                     VERIFY( TreeView_GetItem( lpNmHdr->hwndFrom, &tv ) );
                     if( !Amdb_IsTopicPtr( (PTL)tv.lParam ) )
                        {
                        TreeView_SelectItem( lpNmHdr->hwndFrom, hSelItem );
                        break;
                        }
                     hSelItem = TreeView_GetNextVisible( lpNmHdr->hwndFrom, hSelItem );
                     }
                  }
               break;
               }

            case VK_INSERT:
               if( TestPerm(PF_CANCREATEFOLDERS) )
                  CreateNewFolder();
               break;

            case VK_DELETE:
               InBasket_DeleteCommand( hwnd );
               break;
            }
         return( TRUE );
         }

      case NM_RETURN:
      case NM_DBLCLK:
         if( GetKeyState( VK_CONTROL ) & 0x8000 )
            return( TRUE );
         return( InBasket_OpenCommand( hwnd, FALSE ) );

      case NM_SETFOCUS:
         return( TRUE );

      case TVN_GETDISPINFO: {
         TV_DISPINFO FAR * lptvi;

         lptvi = (TV_DISPINFO FAR *)lpNmHdr;
         if( Amdb_IsCategoryPtr( (PCAT)lptvi->item.lParam ) )
            {
            if( lptvi->item.state & TVIS_EXPANDED )
               {
               lptvi->item.iImage = IBML_SELCATEGORY;
               lptvi->item.iSelectedImage = IBML_SELCATEGORY;
               }
            else
               {
               lptvi->item.iImage = IBML_CATEGORY;
               lptvi->item.iSelectedImage = IBML_CATEGORY;
               }
            }
         else
            {
            if( lptvi->item.state & TVIS_EXPANDED )
               {
               lptvi->item.iImage = IBML_SELFOLDER;
               lptvi->item.iSelectedImage = IBML_SELFOLDER;
               }
            else
               {
               lptvi->item.iImage = IBML_FOLDER;
               lptvi->item.iSelectedImage = IBML_FOLDER;
               }
            }
         return( TRUE );
         }
      }
   return( FALSE );
}

/* This function handles the right-mouse button popup in the
 * in-basket.
 */
void FASTCALL InBasket_RightClickMenu( HWND hwnd, HWND hwndTreeCtl )
{
   TV_HITTESTINFO tvhti;
   BOOL fNewsFolder;
   BOOL fMailFolder;
   HMENU hPopupMenu;
   AEPOPUP aep;
   TV_ITEM tv;
   PCAT pcat;
   POINT pt;
   int pos;
   BOOL fHit;

   tv.lParam = 0L;
   /* Make sure this window is the active one!
    */
   if( hwndActive != hwnd )
      SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwnd, 0L );
   SetFocus( hwndTreeCtl );

   /* Determine where to popup the menu and which folder we've
    * selected at the time.
    */
   GetCursorPos( &tvhti.pt );
   ScreenToClient( hwndTreeCtl, &tvhti.pt );
   if( TreeView_HitTest( hwndTreeCtl, &tvhti ) )
      fHit = TRUE;
   else
      fHit = FALSE;
   GetCursorPos( &pt );
   pos = 6;
   hPopupMenu = GetPopupMenu( IPOP_IBASKWINDOW );

   if( fHit )
      {
      TreeView_SelectItem( hwndTreeCtl, tvhti.hItem );

      /* Get the lParam of the selected item.
       */
      tv.hItem = tvhti.hItem;
      tv.mask = TVIF_PARAM;
      VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );

      /* Fill list of categories if selected item is a
       * folder and there are multiple categories.
       */
      pcat = Amdb_GetFirstCategory();
      if( Amdb_IsFolderPtr( (PCL)tv.lParam ) && NULL != Amdb_GetNextCategory( pcat ) )
         {
         int index;

         hMoveMenu = CreateMenu();
         InsertMenu( hPopupMenu, 1, MF_BYPOSITION|MF_POPUP, (UINT)hMoveMenu, "Move To" );
         index = 0;
         for( ; pcat; pcat = Amdb_GetNextCategory( pcat ) )
            if( pcat != Amdb_CategoryFromFolder( (PCL)tv.lParam ) )
               {
               InsertMenu( hMoveMenu, index, MF_BYPOSITION, IDM_MOVETOFIRST + index, Amdb_GetCategoryName( pcat ) );
               ++index;
               }
         ++pos;
         }

      /* If this is a folder, insert Keep At Top command
       * pos is the offset at which we allow addons to insert
       * their own items. Move it forward if we insert this
       * command before it.
       */
      if( Amdb_IsFolderPtr( (PCL)tv.lParam ) )
         {
         InsertMenu( hPopupMenu, 2, MF_BYPOSITION, IDM_KEEPATTOP, GS(IDS_TT_KEEPATTOP) );
         ++pos;
         }

      /* Add the command to create a new mailbox if we support this
       */
      if (fUseLegacyCIX || fUseInternet)
      {
         HMENU hNewMenu = GetSubMenu( hPopupMenu, 0 );
         InsertMenu( hNewMenu, 0, MF_BYPOSITION, IDM_NEWMAILFOLDER, "Mailbox" );
      }

      /* Decode the folder handle to see if it is one which
       * we can handle.
       */
      GetSupersetTopicTypes( (LPVOID)tv.lParam, &fMailFolder, &fNewsFolder );

      /* Now insert the appropriate commands depending on
       * what we've got in the selected folder.
       */
      if( fNewsFolder )
         {
         if( Amdb_IsTopicPtr( (PTL)(LPVOID)tv.lParam ) )
            if( Amdb_IsResignedTopic( (PTL)(LPVOID)tv.lParam ) )
               InsertMenu( hPopupMenu, pos++, MF_BYPOSITION, IDM_SUBSCRIBE, "Rejoin Newsgroup" );
            else
               InsertMenu( hPopupMenu, pos++, MF_BYPOSITION, IDM_UNSUBSCRIBE, "Resign Newsgroup" );
         InsertMenu( hPopupMenu, pos++, MF_BYPOSITION, IDM_GETARTICLES, "Get New Articles" );
         InsertMenu( hPopupMenu, pos++, MF_BYPOSITION, IDM_GETTAGGED, "Get Tagged Articles" );
         InsertMenu( hPopupMenu, pos++, MF_BYPOSITION|MF_SEPARATOR, 0, NULL );
         }

      if( Amdb_IsTopicPtr( (PTL)(LPVOID)tv.lParam ) )
         MenuEnable( hPopupMenu, IDM_SORT, FALSE );

      }

      /* Call the AE_POPUP event.
       */
      aep.wType = WIN_INBASK;
      aep.pFolder = (LPVOID)tv.lParam;
      aep.pSelectedText = NULL;
      aep.cchSelectedText = 0;
      aep.hMenu = hPopupMenu;
      aep.nInsertPos = pos;
      if( fHit )
         Amuser_CallRegistered( AE_POPUP, (LPARAM)(LPSTR)&aep, 0L );

      /* Add a separator if any extra commands were added to the
       * menu.
       */
      if( pos != aep.nInsertPos )
         InsertMenu( hPopupMenu, aep.nInsertPos, MF_BYPOSITION|MF_SEPARATOR, 0, NULL );

//    if( !fHit )


      /* Now display the menu.
       */
      TrackPopupMenu( hPopupMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndFrame, NULL );
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL InBasket_OnMove( HWND hwnd, int x, int y )
{
   Amuser_WriteWindowState( szInBaskWndName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL InBasket_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   /* Save the new window state to the configuration file.
    */
   Amuser_WriteWindowState( szInBaskWndName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* This function processes the WM_COMMAND message.
 */
void FASTCALL InBasket_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsINBASKET );
         break;

      case IDM_QUICKPRINT:
      case IDM_PRINT:
         CmdPrintInbasket( hwnd );
         break;

      case IDCANCEL:
         /* Fixes bug where pressing escape while dragging item causes window to close
          * What should happen is that the drag should be cancelled only.
          * YH 14/06/96 10:00
          */
         if( fDragging )
            InBasket_OnCancelMode( hwnd );
         else {
            HWND hwndTreeCtl;

            hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
            if( TreeView_GetEditControl( hwndTreeCtl ) )
               TreeView_EndEditLabelNow( hwndTreeCtl, TRUE );
            else
               SendMessage( hwnd, WM_CLOSE, 0, 0L );
            }
         break;

      case IDM_OPEN:
         InBasket_OpenCommand( hwnd, FALSE );
         break;

      case IDM_PROPERTIES:
         InBasket_Properties( hwnd );
         break;

      case IDM_GETARTICLES:
         InBasket_GetNewArticles( hwnd );
         break;

      case IDM_GETTAGGED:
         InBasket_GetTagged( hwnd );
         break;

      case IDM_MARKTOPICREAD:
         CmdMarkTopicRead( hwnd );
         break;

      case IDM_SORT:
         InBasket_SortCommand( hwnd );
         break;

      }
}

/* This function marks the topic selected in the folder
 * list as read.
 */
LPVOID FASTCALL CmdMarkTopicRead( HWND hwnd )
{
   HWND hwndTreeCtl;
   HTREEITEM hSelItem;
   LPVOID pFolder;

   pFolder = NULL;
   hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
   if( hSelItem = TreeView_GetSelection( hwndTreeCtl ) )
      {
      TV_ITEM tv;

      /* Get the lParam of the selected item.
       */
      tv.hItem = hSelItem;
      tv.mask = TVIF_PARAM|TVIF_RECT;
      VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
      pFolder = (LPVOID)tv.lParam;

      /* If we're marking an entire topic...
       */
      if( Amdb_IsTopicPtr( (PTL)tv.lParam ) )
         {
         if( MarkTopicRead( hwnd, (PTL)tv.lParam, TRUE ) > 0 && !IsRectEmpty( &tv.rcItem ) )
            {
            InvalidateRect( hwndTreeCtl, &tv.rcItem, TRUE );
            UpdateWindow( hwndTreeCtl );
            }
         }
      else if( Amdb_IsFolderPtr( (PCL)tv.lParam ) )
         {
         HCURSOR hOldCursor;
         HTREEITEM hItem;
         PTL ptlNext;
         PTL ptl;

         hOldCursor = SetCursor( hWaitCursor );
         hItem = TreeView_GetChild( hwndTreeCtl, hSelItem );
         for( ptl = Amdb_GetFirstTopic( (PCL)tv.lParam ); ptl; ptl = ptlNext )
            {
            ptlNext = Amdb_GetNextTopic( ptl );
            if( MarkTopicRead( hwnd, ptl, ptlNext == NULL ) > 0 )
               {
               /* Get the handle of the treeview item corresponding
                * to this item.
                */
               tv.hItem = hItem;
               tv.mask = TVIF_RECT;
               VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );

               /* If this item is collapsed, the rectangle will
                * be empty.
                */
               if( !IsRectEmpty( &tv.rcItem ) )
                  InvalidateRect( hwndTreeCtl, &tv.rcItem, TRUE );
               }
            hItem = TreeView_GetNextSibling( hwndTreeCtl, hItem );
            }
         UpdateWindow( hwndTreeCtl );
         SetCursor( hOldCursor );
         }
      else  
         {
         HCURSOR hOldCursor;
         HTREEITEM hFolderItem;
         HTREEITEM hItem;
         PCL pclNext;
         PCL pcl;

         hOldCursor = SetCursor( hWaitCursor );
         hFolderItem = TreeView_GetChild( hwndTreeCtl, hSelItem );
         for( pcl = Amdb_GetFirstFolder( (PCAT)tv.lParam ); pcl; pcl = pclNext )
            {
            PTL ptlNext;
            PTL ptl;

            pclNext = Amdb_GetNextFolder( pcl );
            hItem = TreeView_GetChild( hwndTreeCtl, hFolderItem );
            for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = ptlNext )
               {
               ptlNext = Amdb_GetNextTopic( ptl );
               if( MarkTopicRead( hwnd, ptl, ptlNext == NULL ) > 0 )
                  {
                  /* Get the handle of the treeview item corresponding
                   * to this item.
                   */
                  tv.hItem = hItem;
                  tv.mask = TVIF_RECT;
                  VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );

                  /* If this item is collapsed, the rectangle will
                   * be empty.
                   */
                  if( !IsRectEmpty( &tv.rcItem ) )
                     InvalidateRect( hwndTreeCtl, &tv.rcItem, TRUE );
                  }
               hItem = TreeView_GetNextSibling( hwndTreeCtl, hItem );
               }

            /* Deal with collapsed folders.
             */
            if( Amdb_GetFolderFlags( pcl, CF_EXPANDED ) != CF_EXPANDED )
               {
               tv.hItem = hFolderItem;
               tv.mask = TVIF_RECT;
               VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
               InvalidateRect( hwndTreeCtl, &tv.rcItem, TRUE );
               }
            hFolderItem = TreeView_GetNextSibling( hwndTreeCtl, hFolderItem );
            }

         /* Deal with collapsed categories.
          */
         if( Amdb_GetCategoryFlags( (PCAT)tv.lParam, DF_EXPANDED ) != DF_EXPANDED )
            {
            tv.hItem = hSelItem;
            tv.mask = TVIF_RECT;
            VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
            InvalidateRect( hwndTreeCtl, &tv.rcItem, TRUE );
            }
         UpdateWindow( hwndTreeCtl );
         SetCursor( hOldCursor );
         }
      }
   return( pFolder );
}

/* This function returns the handle of the active folder
 * window. It is hwndInBasket if the In Basket window is
 * active, or hwndTopic if the message window is active.
 * It is NULL otherwise.
 */
HWND FASTCALL GetActiveFolderWindow( void )
{
   if( hwndActive == hwndInBasket )
      return( hwndInBasket );
   if( hwndMsg && IsWindowVisible( GetDlgItem( hwndMsg, IDD_FOLDERLIST ) ) )
      return( hwndMsg );
   return( NULL );
}

/* This function handles the Rethread Articles command.
 */
void FASTCALL InBasket_RethreadArticles( HWND hwnd )
{
   HWND hwndFolder;

   hwndFolder = GetActiveFolderWindow();
   if( NULL == hwndFolder && hwndMsg )
      hwndFolder = hwndMsg;
   if( hwndFolder )
      {
      HWND hwndTreeCtl;
      HTREEITEM hSelItem;

      hwndTreeCtl = GetDlgItem( hwndFolder, IDD_FOLDERLIST );
      if( hSelItem = TreeView_GetSelection( hwndTreeCtl ) )
         {
         BOOL fUpdate;
         TV_ITEM tv;
   
         /* Get the lParam of the selected item. Then, depending
          * on what sort of folder it is, call the rethread usenet
          * command to sort things out.
          */
         fUpdate = FALSE;
         tv.hItem = hSelItem;
         tv.mask = TVIF_PARAM;
         VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
         EnumerateFolders( (LPVOID)tv.lParam, EnumRethreadArticles, 0L );
         }
      }
}

/* This function iterates for all folders and subfolders within pFolder and
 * passes their handle to lpfProc with lParam.
 */
void FASTCALL EnumerateFolders( LPVOID pFolder, ENUMFOLDERPROC lpfProc, LPARAM lParam )
{
   if( Amdb_IsTopicPtr( (PTL)pFolder ) )
      lpfProc( (PTL)pFolder, lParam );
   else if( Amdb_IsFolderPtr( (PCL)pFolder ) )
      {
      PTL ptl;

      for( ptl = Amdb_GetFirstTopic( (PCL)pFolder ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
         if( !lpfProc( ptl, lParam ) )
            break;
      }
   else
      {
      PCL pcl;
      PTL ptl;

      for( pcl = Amdb_GetFirstFolder( (PCAT)pFolder ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
         for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
            if( !lpfProc( ptl, lParam ) )
               break;
      }
}

/* This function is called from EnumerateFolders for the
 * folders to be rethreaded.
 */
BOOL FASTCALL EnumRethreadArticles( PTL ptl, LPARAM lParam )
{
   HWND hwndTopic2;

   if( Amdb_RethreadUsenet( hwndActive, ptl ) )
      if( hwndTopic2 = Amdb_GetTopicSel( ptl ) )
         SendMessage( hwndTopic2, WM_VIEWCHANGE, 0, 0L );
   return( TRUE );
}

/* This function handles the Get Tagged Articles command.
 */
void FASTCALL InBasket_GetTagged( HWND hwnd )
{
   GETTAGGEDOBJECT gt;
   BOOL fPrompt;
   HWND hwndFolder;
   char sz[ 100 ];

   hwndFolder = GetActiveFolderWindow();
   if( NULL == hwndFolder && hwndMsg )
      hwndFolder = hwndMsg;
   fPrompt = hwndFolder != NULL;
   Amob_New( OBTYPE_GETTAGGED, &gt );
   if( hwndFolder )
      {
      HWND hwndTreeCtl;
      HTREEITEM hSelItem;

      hwndTreeCtl = GetDlgItem( hwndFolder, IDD_FOLDERLIST );
      if( hSelItem = TreeView_GetSelection( hwndTreeCtl ) )
         {
         TV_ITEM tv;
   
         /* Get the lParam of the selected item.
          */
         tv.hItem = hSelItem;
         tv.mask = TVIF_PARAM;
         VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
         WriteFolderPathname( sz, (LPVOID)tv.lParam );
         }
      }
   else
      {
      PCL pcl;

      pcl = GetNewsFolder();
      WriteFolderPathname( sz, (LPVOID)pcl );
      }
   Amob_CreateRefString( &gt.recFolder, sz );

   /* If we have to prompt to confirm the choice of folder, put up the
    * dialog box now.
    */
   if( fPrompt )
      if( !Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_GETTAGGEDARTICLES), GetTaggedDlg, (LPARAM)(LPSTR)&gt ) )
         {
         Amob_Delete( OBTYPE_GETTAGGED, &gt );
         return;
         }

   /* Create and action the out-basket object.
    */
   if( !Amob_Find( OBTYPE_GETTAGGED, &gt ) )
      Amob_Commit( NULL, OBTYPE_GETTAGGED, &gt );
   Amob_Delete( OBTYPE_GETTAGGED, &gt );
}

/* This function handles the Get New Articles command.
 */
void FASTCALL InBasket_GetNewArticles( HWND hwnd )
{
   NEWARTICLESOBJECT nh;
   HWND hwndFolder;
   char sz[ 100 ];

   Amob_New( OBTYPE_GETNEWARTICLES, &nh );
   hwndFolder = GetActiveFolderWindow();
   if( NULL == hwndFolder && hwndMsg )
      hwndFolder = hwndMsg;
   if( NULL != hwndFolder )
      {
      HWND hwndTreeCtl;
      HTREEITEM hSelItem;
   
      hwndTreeCtl = GetDlgItem( hwndFolder, IDD_FOLDERLIST );
      if( hSelItem = TreeView_GetSelection( hwndTreeCtl ) )
         {
         TV_ITEM tv;

         /* Get the lParam of the selected item.
          */
         tv.hItem = hSelItem;
         tv.mask = TVIF_PARAM;
         VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
         WriteFolderPathname( sz, (LPVOID)tv.lParam );
         }
      }
   else
      {
      PCL pcl;

      pcl = GetNewsFolder();
      WriteFolderPathname( sz, (LPVOID)pcl );
      }
   Amob_CreateRefString( &nh.recFolder, sz );

   /* Confirm the choice of folder - put up the dialog box now.
    */
   if( !Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_GETNEWHEADERS), GetNewArticlesDlg, (LPARAM)(LPSTR)&nh ) )
      {
      Amob_Delete( OBTYPE_GETNEWARTICLES, &nh );
      return;
      }

   /* Create and action the out-basket object.
    */
   if( !Amob_Find( OBTYPE_GETNEWARTICLES, &nh ) )
      Amob_Commit( NULL, OBTYPE_GETNEWARTICLES, &nh );
   Amob_Delete( OBTYPE_GETNEWARTICLES, &nh );
}

/* This function handles the Open command.
 */
BOOL FASTCALL InBasket_OpenCommand( HWND hwnd, BOOL fNew )
{
   HTREEITEM hSelItem;
   HWND hwndTreeCtl;
   TV_ITEM tv;

   /* Get the selected item.
    */
   hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
   if( hSelItem = TreeView_GetSelection( hwndTreeCtl ) )
      {
      PTL ptl;

      /* Get the lParam of the selected item.
       */
      tv.hItem = hSelItem;
      tv.mask = TVIF_PARAM;
      VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );

      /* If Alt key is down, bring up properties.
       */
      if( GetKeyState( VK_MENU ) & 0x8000 )
         {
         InBasket_DisplayProperties( hwnd, (LPVOID)tv.lParam, nLastPropertiesPage );
         return( FALSE );
         }

      /* If this is a topic handle, open the topic window for the
       * selected topic. Otherwise this is a double-click to open a
       * category/folder node, so always return TRUE.
       */
      ptl = NULL;
      if( Amdb_IsCategoryPtr( (PCAT)tv.lParam ) )
         {
         PCL pcl;

         if( NULL != ( pcl = Amdb_GetFirstFolder( (PCAT)tv.lParam ) ) )
            ptl = Amdb_GetFirstTopic( pcl );
         }
      else if( Amdb_IsFolderPtr( (PCL)tv.lParam ) )
         ptl = Amdb_GetFirstTopic( (PCL)tv.lParam );
      else
         ptl = (PTL)tv.lParam;
      if( ptl != NULL )
         return( InBasket_ViewFolder( ptl, fNew ) );
      }
   return( TRUE );
}

/* This function handles the purge command.
 */
void FASTCALL InBasket_PurgeCommand( HWND hwnd )
{
   HWND hwndFolder;

   hwndFolder = GetActiveFolderWindow();
   if( NULL == hwndFolder && hwndMsg )
      hwndFolder = hwndMsg;
   if( NULL != hwndFolder )
      {
      HWND hwndTreeCtl;
      HTREEITEM hSelItem;

      hwndTreeCtl = GetDlgItem( hwndFolder, IDD_FOLDERLIST );
      if( TestPerm(PF_CANPURGE) )
         if( hSelItem = TreeView_GetSelection( hwndTreeCtl ) )
            {
            TV_ITEM tv;
      
            /* Get the lParam of the selected item.
             */
            tv.hItem = hSelItem;
            tv.mask = TVIF_PARAM;
            VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
      
            /* Call the generic purge function.
             */
            Purge( hwnd, (LPVOID)tv.lParam, TRUE );
            }
      }
}

/* This function handles the Build command.
 */
void FASTCALL InBasket_BuildCommand( HWND hwnd )
{
   CURMSG curmsg;

   /* Get the selected folder.
    */
   Ameol2_GetCurrentTopic( &curmsg );
   if( NULL != curmsg.pFolder )
      {
      HWND hBuildDlg;
      HWND hwndGauge;
      UINT cTopics;

      /* Display build dialog.
       */
      hBuildDlg = Adm_CreateDialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_BUILD), (DLGPROC)BuildDlgProc, 0L );
      EnableWindow( hwndFrame, FALSE );

      /* Initialise gauge.
       */
      cTopics = CountTopics( curmsg.pFolder );
      hwndGauge = GetDlgItem( hBuildDlg, IDD_GAUGE );
      SendMessage( hwndGauge, PBM_SETRANGE, 0, MAKELPARAM( 0, cTopics ) );
      SendMessage( hwndGauge, PBM_SETSTEP, 1, 0L );

      /* Enumerate and rebuild folders.
       */
      EnumerateFolders( (LPVOID)curmsg.pFolder, EnumRebuild, (LPARAM)(LPSTR)hBuildDlg );
      EnableWindow( hwndFrame, TRUE );
      DestroyWindow( hBuildDlg );
      }
}

/* This function is called from EnumerateFolders for the
 * folders to be rethreaded.
 */
BOOL FASTCALL EnumRebuild( PTL ptl, LPARAM lParam )
{
   HWND hBuildDlg;
   HWND hwndGauge;
   PCL pcl;

   /* Show the topic name.
    */
   hBuildDlg = (HWND)lParam;
   pcl = Amdb_FolderFromTopic( ptl );
   wsprintf( lpTmpBuf, "%s/%s", Amdb_GetFolderName( pcl ), Amdb_GetTopicName( ptl ) );
   SetDlgItemText( hBuildDlg, IDD_PAD2, lpTmpBuf );

   /* Build the topic.
    */
   BuildImport( hBuildDlg, ptl );

   /* Step the gauge.
    */
   hwndGauge = GetDlgItem( hBuildDlg, IDD_GAUGE );
   SendMessage( hwndGauge, PBM_STEPIT, 0, 0L );
   return( TRUE );
}

/* This function handles the Check command.
 */
void FASTCALL InBasket_CheckCommand( HWND hwnd )
{
   HWND hwndFolder;

   hwndFolder = GetActiveFolderWindow();
   if( NULL == hwndFolder && hwndMsg )
      hwndFolder = hwndMsg;
   if( NULL != hwndFolder )
      {
      HWND hwndTreeCtl;
      HTREEITEM hSelItem;

      hwndTreeCtl = GetDlgItem( hwndFolder, IDD_FOLDERLIST );
      if( hSelItem = TreeView_GetSelection( hwndTreeCtl ) )
         {
         TV_ITEM tv;
      
         /* Get the lParam of the selected item.
          */
         tv.hItem = hSelItem;
         tv.mask = TVIF_PARAM;
         VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
   
         /* Call the generic check and repair function.
          */
         Amdb_CheckAndRepair( hwnd, (LPVOID)tv.lParam, TRUE );
         }
      }
}

/* This function handles the Keep At Top command.
 */
void FASTCALL InBasket_KeepAtTop( HWND hwnd )
{
   HWND hwndFolder;

   hwndFolder = GetActiveFolderWindow();
   if( NULL == hwndFolder && hwndMsg )
      hwndFolder = hwndMsg;
   if( NULL != hwndFolder )
      {
      HWND hwndTreeCtl;
      HTREEITEM hSelItem;

      hwndTreeCtl = GetDlgItem( hwndFolder, IDD_FOLDERLIST );
      if( hSelItem = TreeView_GetSelection( hwndTreeCtl ) )
         {
         TV_ITEM tv;
      
         /* Get the lParam of the selected item.
          */
         tv.hItem = hSelItem;
         tv.mask = TVIF_PARAM;
         VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
   
         /* Make sure it is a folder.
          */
         if( Amdb_IsFolderPtr( (PCL)tv.lParam ) )
            {
            int iMoveError;
            PCAT pcat;

            /* If already kept at top, clear the flag.
             */
            if( Amdb_GetFolderFlags( (PCL)tv.lParam, CF_KEEPATTOP ) )
               Amdb_SetFolderFlags( (PCL)tv.lParam, CF_KEEPATTOP, FALSE );
            else
               Amdb_SetFolderFlags( (PCL)tv.lParam, CF_KEEPATTOP, TRUE );

            /* Need to move it because it may be in the wrong position.
             */
            fKeepAtTopFolder = TRUE;
            pcat = Amdb_CategoryFromFolder( (PCL)tv.lParam );
            iMoveError = Amdb_MoveFolder( (PCL)tv.lParam, pcat, NULL );
            if( AMDBERR_NOPERMISSION == iMoveError )
               fMessageBox( hwnd, 0, GS(IDS_STR852), MB_OK|MB_ICONEXCLAMATION );
            else
               {
               /* Finally, select the folder we just dragged.
                */
               hSelItem = Amdb_FindFolderItem( hwndTreeCtl, (PCL)tv.lParam );
               ASSERT( hSelItem != 0 );
               TreeView_SelectItem( hwndTreeCtl, hSelItem );
               }
            }
         }
      }
}

/* This function handles the delete command.
 */
void FASTCALL InBasket_DeleteCommand( HWND hwnd )
{
   HWND hwndFolder;
   HCURSOR hOldCursor;

   hOldCursor = SetCursor( LoadCursor( NULL, IDC_WAIT ) );

   hwndFolder = GetActiveFolderWindow();
   if( NULL == hwndFolder && hwndMsg )
      hwndFolder = hwndMsg;
   if( NULL != hwndFolder )
      {
      HWND hwndTreeCtl;
      HTREEITEM hSelItem;

      hwndTreeCtl = GetDlgItem( hwndFolder, IDD_FOLDERLIST );
      if( hSelItem = TreeView_GetSelection( hwndTreeCtl ) )
         {
         HTREEITEM hNextItem;
         LPVOID pSelItem;
         LPVOID pNextItem;
         TV_ITEM tv;
   
         /* Highlight the next or previous item. If none,
          * highlight the parent.
          */
         hNextItem = TreeView_GetNextSibling( hwndTreeCtl, hSelItem );
         if( 0L == hNextItem )
            {
            hNextItem = TreeView_GetPrevSibling( hwndTreeCtl, hSelItem );
            if( 0L == hNextItem )
               hNextItem = TreeView_GetParent( hwndTreeCtl, hSelItem );
            }
   
         /* Get the lParam of the selected item.
          */
         tv.hItem = hSelItem;
         tv.mask = TVIF_PARAM;
         VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
         pSelItem = (LPVOID)tv.lParam;

         /* Get the lParam of the next item.
          */
         pNextItem = NULL;
         if( 0L != hNextItem )
            {
            tv.hItem = hNextItem;
            tv.mask = TVIF_PARAM;
            VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
            pNextItem = (LPVOID)tv.lParam;
            }

         /* What are we deleting?
          */
         if( Amdb_IsTopicPtr( (PTL)pSelItem ) )
               InBasket_DeleteTopic( hwndFolder, (PTL)pSelItem, hNextItem );
         else if( Amdb_IsFolderPtr( (PCL)pSelItem ) )
            InBasket_DeleteFolder( hwndFolder, (PCL)pSelItem, hNextItem );
         else
            InBasket_DeleteCategory( hwndFolder, (PCAT)pSelItem, hNextItem );

         /* If we're not deleting from the In-Basket then need to
          * update the selection in the In-Basket
          */
         if( hwndFolder != hwndInBasket && 0L != hNextItem )
            {
            hwndTreeCtl = GetDlgItem( hwndInBasket, IDD_FOLDERLIST );
            if( Amdb_IsTopicPtr( (PTL)pNextItem ) )
               hSelItem = Amdb_FindTopicItem( hwndTreeCtl, (PTL)pNextItem );
            else if( Amdb_IsFolderPtr( (PCL)pNextItem ) )
               hSelItem = Amdb_FindFolderItem( hwndTreeCtl, (PCL)pNextItem );
            else
               hSelItem = Amdb_FindCategoryItem( hwndTreeCtl, (PCAT)pNextItem );
            TreeView_SelectItem( hwndTreeCtl, hSelItem );
            }
         }
      }
   if( hwndParlist )
         ParlistWndResetContent( hwndParlist );
   SetCursor( hOldCursor );
}

/* This function handles the Rename command.
 */
void FASTCALL InBasket_RenameCommand( HWND hwnd )
{
   HWND hwndFolder;

   hwndFolder = GetActiveFolderWindow();
   if( NULL == hwndFolder && hwndMsg )
      hwndFolder = hwndMsg;
   if( NULL != hwndFolder )
      {
      HWND hwndTreeCtl;
      HTREEITEM hSelItem;

      hwndTreeCtl = GetDlgItem( hwndFolder, IDD_FOLDERLIST );
      if( hSelItem = TreeView_GetSelection( hwndTreeCtl ) )
         {
         TV_ITEM tv;

         /* Don't allow editing category name.
          */
         tv.hItem = hSelItem;
         tv.mask = TVIF_PARAM;
         VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
         TreeView_EditLabel( hwndTreeCtl, hSelItem );
         }
      }
}

/* This function creates a new category.
 */
void FASTCALL CreateNewCategory( void )
{
   HWND hwndFolder;

   hwndFolder = GetActiveFolderWindow();
   if( NULL == hwndFolder )
      {
      CreateInBasket( hwndFrame );
      hwndFolder = GetActiveFolderWindow();
      }
   if( hwndFolder && TestPerm(PF_CANCREATEFOLDERS) )
      {
      char szFolderName[ 64 ];
      int cFolder;
      PCAT pcat;

      /* Create an unique name for New Folder
       */
      strcpy( szFolderName, "New_Category" );
      cFolder = 1;
      while( cFolder < 10 && Amdb_OpenCategory( szFolderName ) )
         {
         ++cFolder;
         wsprintf( szFolderName, "%s (%u)", "New Category", cFolder );
         }

      /* No space for new folder?
       */
      if( 10 == cFolder )
         {
         fMessageBox( hwndFrame, 0, GS(IDS_STR1115), MB_OK|MB_ICONINFORMATION );
         return;
         }

      /* Create a new folder.
       */
      pcat = Amdb_CreateCategory( szFolderName );
      if( NULL == pcat )
         fMessageBox( hwndFrame, 0, GS(IDS_STR844), MB_OK|MB_ICONINFORMATION );
      else
         {
         HWND hwndTreeCtl;
         HTREEITEM hSelItem;

         /* Initiate editing of the new folder name.
          */
         VERIFY( hwndTreeCtl = GetDlgItem( hwndFolder, IDD_FOLDERLIST ) );
         hSelItem = Amdb_FindCategoryItem( hwndTreeCtl, pcat );
         TreeView_SelectItem( hwndTreeCtl, hSelItem );
         SetFocus( hwndTreeCtl );
         TreeView_EditLabel( hwndTreeCtl, hSelItem );
         }
      }
}

/* This function creates a new folder.
 */
void FASTCALL CreateNewFolder( void )
{
   HWND hwndFolder;

   hwndFolder = GetActiveFolderWindow();
   if( NULL == hwndFolder )
      {
      CreateInBasket( hwndFrame );
      hwndFolder = GetActiveFolderWindow();
      }
   if( hwndFolder && TestPerm(PF_CANCREATEFOLDERS) )
      {
      HWND hwndTreeCtl;
      HTREEITEM hSelItem;
      char szFolderName[ 64 ];
      TV_ITEM tv;
      int cFolder;
      PCAT pFolder;
      PCL pcl;

      /* Is there a selected folder?
       */
      VERIFY( hwndTreeCtl = GetDlgItem( hwndFolder, IDD_FOLDERLIST ) );
      hSelItem = TreeView_GetSelection( hwndTreeCtl );
      ASSERT( 0 != hSelItem );

      /* Get the lParam of the selected item.
       */
      tv.hItem = hSelItem;
      tv.mask = TVIF_PARAM;
      VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
      if( Amdb_IsFolderPtr( (PCL)tv.lParam ) )
         pFolder = Amdb_CategoryFromFolder( (PCL)tv.lParam );
      else if( Amdb_IsTopicPtr( (PTL)tv.lParam ) )
         {
         PCL pcl;

         pcl = Amdb_FolderFromTopic( (PTL)tv.lParam );
         pFolder = Amdb_CategoryFromFolder( pcl );
         }
      else
         pFolder = (PCAT)tv.lParam;

      /* Create an unique name for New Folder
       */
      strcpy( szFolderName, GS(IDS_STR1105) );
      cFolder = 1;
      while( cFolder < 10 && Amdb_OpenFolder( NULL, szFolderName ) )
         {
         ++cFolder;
         wsprintf( szFolderName, "%s (%u)", GS(IDS_STR1105), cFolder );
         }

      /* No space for new folder?
       */
      if( 10 == cFolder )
         {
         fMessageBox( hwndFrame, 0, GS(IDS_STR1115), MB_OK|MB_ICONINFORMATION );
         return;
         }

      /* Create a new folder.
       */
      pcl = Amdb_CreateFolder( pFolder, szFolderName, CFF_SORT );
      if( NULL == pcl )
         fMessageBox( hwndFrame, 0, GS(IDS_STR844), MB_OK|MB_ICONINFORMATION );
      else
         {
         /* Initiate editing of the new folder name.
          */
         hSelItem = Amdb_FindFolderItem( hwndTreeCtl, pcl );
         TreeView_SelectItem( hwndTreeCtl, hSelItem );
         SetFocus( hwndTreeCtl );
         TreeView_EditLabel( hwndTreeCtl, hSelItem );
         }
      }
}

/* This function deletes a topic from the in-basket.
 */
void FASTCALL InBasket_DeleteTopic( HWND hwnd, PTL ptl, HTREEITEM hNextItem )
{
   /* Call events registered with deleting an in-basket
    * topic.
    */
   if( !Amuser_CallRegistered( AE_INBASK_FOLDER_DELETING, (LPARAM)(LPSTR)hwnd, (LPARAM)ptl ) )
      return;

   /* Can't delete topic if it is in use.
    */
   if( Amdb_GetTopicFlags( ptl, TF_TOPICINUSE ) )
      {
      fMessageBox( hwnd, 0, "Topic cannot be deleted as it is currently in use.", MB_OK|MB_ICONEXCLAMATION );
      return;
      }
   /* Can't delete a topic if protection enabled 
    */
   if(( Amdb_GetTopicType( ptl )  == FTYPE_MAIL) && fProtectMailFolders )
      {
      fMessageBox( hwnd, 0, GS(IDS_STR1212), MB_OK|MB_ICONEXCLAMATION );
      return;
      }
   /* Offer to delete the topic.
    */
   wsprintf( lpTmpBuf, GS(IDS_STR1246), Amdb_GetTopicName( ptl ) );
   if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONINFORMATION|MB_DEFBUTTON2 ) == IDYES )
      {
      HWND hwndTreeCtl;
      int error;

      if( ptlPostmaster == ptl )
      {
         fMessageBox( hwnd, 0, "You are deleting your main mailbox.\r\n\r\nPlease choose a new mailbox to which new mail messages will added.", MB_OK|MB_ICONEXCLAMATION );
         MailDialog( hwnd, 0 );
         if( ptlPostmaster == ptl )
            return;
      }
      error = Amdb_DeleteTopic( ptl );
      if( AMDBERR_NOPERMISSION == error )
         fMessageBox( hwnd, 0, GS(IDS_STR850), MB_OK|MB_ICONEXCLAMATION );
      else if( AMDBERR_TOPICLOCKED == error )
         fMessageBox( hwnd, 0, GS(IDS_STR1123), MB_OK|MB_ICONEXCLAMATION );
      else
         {
         if( ptlLastTopic == ptl )
            ptlLastTopic = NULL;
         hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
         if( hNextItem )
            TreeView_SelectItem( hwndTreeCtl, hNextItem );
         }
      }
}

/* This function deletes a folder from the in-basket.
 */
void FASTCALL InBasket_DeleteFolder( HWND hwnd, PCL pcl, HTREEITEM hNextItem )
{
   PTL ptl;

   /* Call events registered with deleting an in-basket
    * folder.
    */
   if( !Amuser_CallRegistered( AE_INBASK_FOLDER_DELETING, (LPARAM)(LPSTR)hwnd, (LPARAM)pcl ) )
      return;

   /* Can't delete topic if it is in use.
    */
   for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
      if( Amdb_GetTopicFlags( ptl, TF_TOPICINUSE ) )
         {
         fMessageBox( hwnd, 0, "This folder cannot be deleted as some topics in it are currently in use.", MB_OK|MB_ICONEXCLAMATION );
         return;
         }
      else
         if(( Amdb_GetTopicType( ptl )  == FTYPE_MAIL) && fProtectMailFolders )
         {
         fMessageBox( hwnd, 0, GS(IDS_STR1213), MB_OK|MB_ICONEXCLAMATION );
         return;
         }
   
   /* Offer to delete the folder.
    */
   wsprintf( lpTmpBuf, GS(IDS_STR727), Amdb_GetFolderName( pcl ) );
   if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONINFORMATION|MB_DEFBUTTON2 ) == IDYES )
      {
      HWND hwndTreeCtl;
      int error;

      if( ptlPostmaster != NULL && Amdb_FolderFromTopic( ptlPostmaster ) == pcl )
      {
         fMessageBox( hwnd, 0, "You are deleting the folder containing your main mailbox.\r\n\r\nPlease choose a new mailbox to which new mail messages will added.", MB_OK|MB_ICONEXCLAMATION );
         MailDialog( hwnd, 0 );
         if( Amdb_FolderFromTopic( ptlPostmaster ) == pcl )
            return;
      }
      if( pclNewsFolder == pcl )
      {
         fMessageBox( hwnd, 0, "You are deleting the folder to which new newsgroups are added.\r\n\r\nPlease choose a new folder to which new newsgroups will added.", MB_OK|MB_ICONEXCLAMATION );
         nNewsDialogPage = 0;
         CmdUsenetSettings( hwnd );
         if( pclNewsFolder == pcl )
            return;
      }
      if( NULL != ptlLastTopic )
         if( Amdb_FolderFromTopic( ptlLastTopic ) == pcl )
            ptlLastTopic = NULL;
      error = Amdb_DeleteFolder( pcl );
      if( AMDBERR_NOPERMISSION == error )
         fMessageBox( hwnd, 0, GS(IDS_STR849), MB_OK|MB_ICONEXCLAMATION );
      else if( AMDBERR_TOPICLOCKED == error )
         fMessageBox( hwnd, 0, GS(IDS_STR1123), MB_OK|MB_ICONEXCLAMATION );
      else
         {
         hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
         if( hNextItem )
            TreeView_SelectItem( hwndTreeCtl, hNextItem );
         }
      }
}

/* This function deletes a category from the in-basket.
 */
void FASTCALL InBasket_DeleteCategory( HWND hwnd, PCAT pcat, HTREEITEM hNextItem )
{
   PTL ptl;
   PCL pcl;

   if( NULL == Amdb_GetNextCategory( pcat ) && NULL == Amdb_GetPreviousCategory( pcat ) )
      return;

   for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
   {
      for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
      {
         if( Amdb_GetTopicFlags( ptl, TF_TOPICINUSE ) )
         {
            fMessageBox( hwnd, 0, "This folder cannot be deleted as some topics in it are currently in use.", MB_OK|MB_ICONEXCLAMATION );
            return;
         }
         else
         if(( Amdb_GetTopicType( ptl )  == FTYPE_MAIL) && fProtectMailFolders )
         {
            fMessageBox( hwnd, 0, GS(IDS_STR1213), MB_OK|MB_ICONEXCLAMATION );
            return;
         }
      }
   }

   wsprintf( lpTmpBuf, GS(IDS_STR1223), Amdb_GetCategoryName( pcat ) );
   if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONINFORMATION|MB_DEFBUTTON2 ) == IDYES )
      {
      CATEGORYINFO cai;
      HWND hwndTreeCtl;

      if( ptlPostmaster != NULL && Amdb_CategoryFromFolder( Amdb_FolderFromTopic( ptlPostmaster ) ) == pcat )
      {
         fMessageBox( hwnd, 0, "You are deleting the category containing your main mailbox.\r\n\r\nPlease choose a new mailbox to which new mail messages will added.", MB_OK|MB_ICONEXCLAMATION );
         MailDialog( hwnd, 0 );
         if( Amdb_CategoryFromFolder( Amdb_FolderFromTopic( ptlPostmaster ) ) == pcat )
            return;
      }
      if( pclNewsFolder != NULL && Amdb_CategoryFromFolder( pclNewsFolder ) == pcat )
      {
         fMessageBox( hwnd, 0, "You are deleting the category containing the folder to which new newsgroups are added.\r\n\r\nPlease choose a new folder to which new newsgroups will added.", MB_OK|MB_ICONEXCLAMATION );
         nNewsDialogPage = 0;
         CmdUsenetSettings( hwnd );
         if( Amdb_CategoryFromFolder( pclNewsFolder ) == pcat )
            return;
      }

      /* Need info about this category.
       */
      Amdb_GetCategoryInfo( pcat, &cai );

      /* Clear the caches.
       */
      if( NULL != ptlLastTopic )
         if( Amdb_CategoryFromFolder( Amdb_FolderFromTopic( ptlLastTopic ) ) == pcat )
            ptlLastTopic = NULL;
      if( AMDBERR_NOPERMISSION == Amdb_DeleteCategory( pcat ) )
         fMessageBox( hwnd, 0, GS(IDS_STR837), MB_OK|MB_ICONINFORMATION );
      else
         {
         /* Select the next folder.
          */
         hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
         if( hNextItem )
            TreeView_SelectItem( hwndTreeCtl, hNextItem );
         }
      }
}

/* This function handles the New Message Window command. To be
 * backward compatible with Ameol, we have to handle several special
 * cases.
 */
void FASTCALL CmdNewWindow( HWND hwnd )
{
   BOOL fGetMarked = FALSE;

   if( NULL != hwndTopic || NULL != hwndInBasket )
   {
      if( NULL != GetActiveFolderWindow() )
         InBasket_OpenCommand( GetActiveFolderWindow(), TRUE );
      else
      {
         if( NULL != hwndTopic )
            fGetMarked = TRUE;
         else
            OpenOrJoinTopic( Amdb_GetFirstTopic( Amdb_GetFirstRealFolder( Amdb_GetFirstCategory() ) ), 0L, TRUE );
      }

   }

   else if( NULL == hwndInBasket && NULL == hwndTopic )
      OpenOrJoinTopic( Amdb_GetFirstTopic( Amdb_GetFirstRealFolder( Amdb_GetFirstCategory() ) ), 0L, TRUE );

   else
      fGetMarked = TRUE;

   if( fGetMarked )
   {
      char szName[ LEN_FULLNAME+1 ];
      DWORD dwMsgNumber;
      PTL ptl;
      BOOL fLastResort;

      /* Has the user highlighted a folder/topic name in the current
       * message window? If so, read it off and use it to determine
       * the destination topic.
       */
      GetMarkedName( hwndTopic, szName, LEN_FULLNAME, TRUE );
      ptl = NULL;
      dwMsgNumber = 0L;
      if( szName[ 0 ] )
         ParseFolderTopicMessage( szName, &ptl, &dwMsgNumber );

      /* If no valid topic name highlighted, then take the current
       * topic and display the next one in the sequence.
       */
      if( szName[ 0 ] == '\0' )
         {
         LPWINDINFO lpWindInfo;

         lpWindInfo = GetBlock( hwndTopic );
         ptl = lpWindInfo->lpTopic;
         if( ( ptl = Amdb_GetNextTopic( ptl ) ) == NULL )
            {
            PCL pcl;

            pcl = Amdb_FolderFromTopic( lpWindInfo->lpTopic );
            do {
               PCAT pcat;

               pcat = Amdb_CategoryFromFolder( pcl );
               if( ( pcl = Amdb_GetNextFolder( pcl ) ) == NULL )
                  pcl = Amdb_GetFirstFolder( pcat );
               ptl = Amdb_GetFirstTopic( pcl );
               }
            while( ptl == NULL );
            }
            fLastResort = TRUE;
         }
      else
         fLastResort = FALSE;

      /* If we now have a valid topic, open it in a new window and
       * display the first unread message in that topic.
       */
      if( fLastResort )
         {
            if( TreeView_GetSelection( GetDlgItem( hwndTopic, IDD_FOLDERLIST ) ) )
            {
            InBasket_OpenCommand( hwndTopic, TRUE );
            return;
            }
         }
      else
         OpenOrJoinTopic( ptl, dwMsgNumber, TRUE );
      }
}

/* This function opens a message window on topic ptl and sets the
 * current message to the numbered message.
 */
void FASTCALL OpenOrJoinTopic( PTL ptl, DWORD dwMsgNumber, BOOL fNew )
{
   TOPICINFO topicinfo;
   PTH pth;

   Amdb_GetTopicInfo( ptl, &topicinfo );
   Amdb_LockTopic( ptl );
   if( 0L != dwMsgNumber )
      pth = Amdb_OpenMsg( ptl, dwMsgNumber );
   else if( 0 == topicinfo.cUnRead )
      pth = Amdb_GetLastMsg( ptl, vdDef.nViewMode );
   else
      {
      CURMSG curmsg;

      curmsg.pcl = Amdb_FolderFromTopic( ptl );
      curmsg.pcat = Amdb_CategoryFromFolder( curmsg.pcl );
      curmsg.ptl = ptl;
      curmsg.pth = NULL;
      if( Amdb_GetNextUnReadMsg( &curmsg, vdDef.nViewMode ) )
         {
         Amdb_InternalUnlockTopic( ptl );
         pth = curmsg.pth;
         }
      else
         pth = Amdb_GetLastMsg( ptl, vdDef.nViewMode );
      }
   OpenMsgViewerWindow( ptl, pth, fNew );
   Amdb_UnlockTopic( ptl );
   SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndMsg, 0L );
}

/* Given a string of the format:
 *    conference\topic:message
 * This function returns the PTL handle of the topic and the
 * message number.
 */
BOOL FASTCALL ParseFolderTopicMessage( char * pszInput, PTL * pptl, DWORD * pdw )
{
   char * npszConfName;
   char * npszTopicName;
   char * npszMsgNumber;
   PCL pcl;

   *pdw = 0L;
   *pptl = NULL;

   /* Extract folder/topic name. Either or both
    * may be blank.
    */
   npszConfName = npszTopicName = pszInput;
   if( *pszInput != ':' && *pszInput != '#' )
      {
      while( *npszTopicName && *npszTopicName != '/' )
         ++npszTopicName;
      if( *npszTopicName == '/' )
         *npszTopicName++ = '\0';
      npszMsgNumber = npszTopicName;
      while( *npszMsgNumber && *npszMsgNumber != ':' && *npszMsgNumber != '#' )
         ++npszMsgNumber;
      }
   else
      npszMsgNumber = pszInput;

   /* Extract any message number.
    */
   if( *npszMsgNumber == ':' || *npszMsgNumber == '#' )
      {
      *npszMsgNumber++ = '\0';
      *pdw = atol( npszMsgNumber );
      }

   /* Now get the ptl.
    */
   if( *npszConfName == '\0' && hwndTopic )
      {
      LPWINDINFO lpWindInfo;

      lpWindInfo = GetBlock( hwndTopic );
      pcl = Amdb_FolderFromTopic( lpWindInfo->lpTopic );
      }
   else
      pcl = Amdb_OpenFolder( NULL, npszConfName );
   if( NULL != pcl )
      {
      if( *npszTopicName )
         *pptl = Amdb_OpenTopic( pcl, npszTopicName );
      else
         *pptl = Amdb_GetFirstTopic( pcl );
      if( NULL == *pptl )
         pszInput[ 0 ] = '\0';
      }
   else if (*pszInput) 
   {
      pszInput[ 0 ] = '\0';
   }
   return( *pszInput != '\0' );
}

/* This function opens the message viewer window on the specified
 * topic.
 */
BOOL FASTCALL InBasket_ViewFolder( PTL ptl, BOOL fNew )
{
   TOPICINFO topicinfo;
   CURMSG curmsg;

   curmsg.ptl = ptl;
   curmsg.pcl = Amdb_FolderFromTopic( curmsg.ptl );
   curmsg.pcat = Amdb_CategoryFromFolder( curmsg.pcl );
   Amdb_LockTopic( curmsg.ptl );
   curmsg.pth = Amdb_GetLastMsg( curmsg.ptl, VM_VIEWREF );
   curmsg.pth = Amdb_ExGetRootMsg( curmsg.pth, TRUE, vdDef.nViewMode );
   Amdb_GetTopicInfo( curmsg.ptl, &topicinfo );
   if( topicinfo.cUnRead )
      {
      curmsg.pth = NULL;
      Amdb_GetNextUnReadMsg( &curmsg, vdDef.nViewMode );
      Amdb_InternalUnlockTopic( curmsg.ptl );
      }
   SetCurrentMsgEx( &curmsg, TRUE, TRUE, fNew );
   Amdb_UnlockTopic( curmsg.ptl );
   return( FALSE );
}

/* This function returns the index of an image for the specified
 * folder type.
 */
int FASTCALL GetTopicTypeImage( PTL ptl )
{
   switch( Amdb_GetTopicType( ptl ) )
      {
      case FTYPE_NEWS:     return( IBML_NEWS );
      case FTYPE_MAIL:     return( IBML_MAIL );
      case FTYPE_UNTYPED:  return( IBML_UNTYPED );
      case FTYPE_CIX:      return( IBML_CIX );
      case FTYPE_LOCAL:    return( IBML_LOCAL );
      }
   return( -1 );
}

/* This function displays the properties dialog for the
 * specified in-basket item.
 */
void FASTCALL InBasket_Properties( HWND hwnd )
{
   HWND hwndTreeCtl;
   HTREEITEM hSelItem;

   hwndTreeCtl = GetDlgItem( hwnd, IDD_FOLDERLIST );
   if( hSelItem = TreeView_GetSelection( hwndTreeCtl ) )
      {
      TV_ITEM tv;

      /* Get the lParam of the selected item and display
       * the Properties dialog.
       */
      tv.hItem = hSelItem;
      tv.mask = TVIF_PARAM;
      VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );
      InBasket_DisplayProperties( hwnd, (LPVOID)tv.lParam, nLastPropertiesPage );
      }
}

/* This function displays the property dialog for the specified
 * folder.
 */
void FASTCALL InBasket_DisplayProperties( HWND hwnd, LPVOID pFolder, int nPage )
{
   AMPROPSHEETPAGE psp[ 6 ];
   AMPROPSHEETHEADER psh;
   LPCSTR lpszTitle;
   LPCSTR pszDlg;
   int cPage;


   if( Amdb_IsCategoryPtr( (PCAT)pFolder ) )
      {
      lpszTitle = Amdb_GetCategoryName( (PCAT)pFolder );
      pszDlg = MAKEINTRESOURCE( IDDLG_IBP_DATABASE );
      }
   else if( Amdb_IsFolderPtr( (PCL)pFolder ) )
      {
      lpszTitle = Amdb_GetFolderName( (PCL)pFolder );
      pszDlg = MAKEINTRESOURCE( IDDLG_IBP_FOLDER );
      }
   else
      {
      lpszTitle = Amdb_GetTopicName( (PTL)pFolder );
      pszDlg = MAKEINTRESOURCE( IDDLG_IBP_TOPIC );
      }
   cPage = 0;
                           
   /* Create the Settings page.
    */
   ASSERT( cPage < (sizeof( psp ) / sizeof( AMPROPSHEETPAGE )) );
   psp[ IPP_SETTINGS ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ IPP_SETTINGS ].dwFlags = PSP_USETITLE;
   psp[ IPP_SETTINGS ].hInstance = hInst;
   psp[ IPP_SETTINGS ].pszTemplate = MAKEINTRESOURCE( IDDLG_IBP_SETTINGS );
   psp[ IPP_SETTINGS ].pszIcon = NULL;
   psp[ IPP_SETTINGS ].pfnDlgProc = InBasket_Prop5;
   psp[ IPP_SETTINGS ].pszTitle = "Settings";
   psp[ IPP_SETTINGS ].lParam = (LPARAM)pFolder;
   ++cPage;

   /* Create the general properties page.
    */
   ASSERT( cPage < (sizeof( psp ) / sizeof( AMPROPSHEETPAGE )) );
   psp[ IPP_STATISTICS ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ IPP_STATISTICS ].dwFlags = PSP_USETITLE;
   psp[ IPP_STATISTICS ].hInstance = hInst;
   psp[ IPP_STATISTICS ].pszTemplate = pszDlg;
   psp[ IPP_STATISTICS ].pszIcon = NULL;
   psp[ IPP_STATISTICS ].pfnDlgProc = InBasket_Prop1;
   psp[ IPP_STATISTICS ].pszTitle = "Statistics";
   psp[ IPP_STATISTICS ].lParam = (LPARAM)pFolder;
   ++cPage;

   /* Create the purge properties page.
    */
   ASSERT( cPage < (sizeof( psp ) / sizeof( AMPROPSHEETPAGE )) );
   psp[ IPP_PURGE ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ IPP_PURGE ].dwFlags = PSP_USETITLE;
   psp[ IPP_PURGE ].hInstance = hInst;
   psp[ IPP_PURGE ].pszTemplate = MAKEINTRESOURCE( IDDLG_IBP_PURGE );
   psp[ IPP_PURGE ].pszIcon = NULL;
   psp[ IPP_PURGE ].pfnDlgProc = PurgeOptions;
   psp[ IPP_PURGE ].pszTitle = "Purge";
   psp[ IPP_PURGE ].lParam = (LPARAM)pFolder;
   ++cPage;

   /* Create the Signature properties page.
    */
   ASSERT( cPage < (sizeof( psp ) / sizeof( AMPROPSHEETPAGE )) );
   psp[ IPP_SIGNATURE ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ IPP_SIGNATURE ].dwFlags = PSP_USETITLE;
   psp[ IPP_SIGNATURE ].hInstance = hInst;
   psp[ IPP_SIGNATURE ].pszTemplate = MAKEINTRESOURCE( IDDLG_IBP_SIGNATURE );
   psp[ IPP_SIGNATURE ].pszIcon = NULL;
   psp[ IPP_SIGNATURE ].pfnDlgProc = InBasket_Prop3;
   psp[ IPP_SIGNATURE ].pszTitle = "Signature";
   psp[ IPP_SIGNATURE ].lParam = (LPARAM)pFolder;
   ++cPage;

   /* Create the Rules properties page.
    */
   ASSERT( cPage < (sizeof( psp ) / sizeof( AMPROPSHEETPAGE )) );
   psp[ IPP_RULES ].dwSize = sizeof( AMPROPSHEETPAGE );
   psp[ IPP_RULES ].dwFlags = PSP_USETITLE;
   psp[ IPP_RULES ].hInstance = hInst;
   psp[ IPP_RULES ].pszTemplate = MAKEINTRESOURCE( IDDLG_IBP_RULES );
   psp[ IPP_RULES ].pszIcon = NULL;
   psp[ IPP_RULES ].pfnDlgProc = FolderPropRules;
   psp[ IPP_RULES ].pszTitle = "Rules";
   psp[ IPP_RULES ].lParam = (LPARAM)pFolder;
   ++cPage;

   /* Create the font we'll use for the category, folder and topic
    * names at the top of each page.
    */
   ASSERT( NULL == hTitleFont );
   hTitleFont = CreateFont( 18, 0, 0, 0, FW_BOLD, FALSE, FALSE,
                           FALSE, ANSI_CHARSET, OUT_CHARACTER_PRECIS,
                           CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
                           VARIABLE_PITCH | FF_SWISS, "Times New Roman" );

   /* Create a 6pt font for the Signature preview page.
    */
   ASSERT( NULL == hDlg6Font );
   hDlg6Font = CreateFont( 12, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                           FALSE, ANSI_CHARSET, OUT_CHARACTER_PRECIS,
                           CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
                           VARIABLE_PITCH | FF_SWISS, "Arial" );

   /* Create the properties dialog.
    */
   psh.dwSize = sizeof( AMPROPSHEETHEADER );
   psh.dwFlags = PSH_PROPSHEETPAGE|PSH_PROPTITLE|PSH_USECALLBACK|PSH_HASHELP;
   if( fMultiLineTabs )
      psh.dwFlags |= PSH_MULTILINETABS;
   psh.hwndParent = hwndFrame;
   psh.hInstance = hInst;
   psh.pszIcon = NULL;
   psh.pszCaption = lpszTitle;
   psh.nPages = cPage;
   psh.nStartPage = nPage;
   psh.pfnCallback = InbasketPropertiesCallbackFunc;
   psh.ppsp = (LPCAMPROPSHEETPAGE)&psp;
   nLastPropertiesPage = nPage;
   pPropFolder = pFolder;
   Amctl_PropertySheet(&psh );

   /* Added so that rules code can tidy up stuff when the rules dialog
    * closes. YH 17/04/96
    */
   PropertySheetClosingRules();

   /* Delete the font.
    */
   DeleteFont( hDlg6Font );
   DeleteFont( hTitleFont );
   DeleteFont( hAboutFont );
   hDlg6Font = NULL;
   hTitleFont = NULL;
   hAboutFont = NULL;
}

/* This function is called as soon as the property sheet has
 * finished initialising. We raise a AE_PROPERTIESDIALOG event to allow
 * addons to attach their own property pages.
 */
UINT CALLBACK EXPORT InbasketPropertiesCallbackFunc( HWND hwnd, UINT uMsg, LPARAM lParam )
{
   if( PSCB_INITIALIZED == uMsg )
      Amuser_CallRegistered( AE_PROPERTIESDIALOG, (LPARAM)(LPSTR)hwnd, (LPARAM)pPropFolder );
   PropSheet_SetCurSel( hwnd, 0L, nLastPropertiesPage );
   return( 0 );
}

/* This function returns TRUE if this is a folder and there are CIX
 * topics within that folder.
 */
BOOL FASTCALL IsCIXFolder( LPVOID pFolder )
{
   PTL ptl;

   if( Amdb_IsFolderPtr( (PCL)pFolder ) )
      for( ptl = Amdb_GetFirstTopic( (PCL)pFolder ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
         if( Amdb_GetTopicType( ptl ) == FTYPE_CIX )
            return( TRUE );
   return( FALSE );
}

/* This function returns TRUE if this is a folder and there are local
 * topics within that folder.
 */
BOOL FASTCALL IsLocalFolder( LPVOID pFolder )
{
   PTL ptl;

   if( Amdb_IsFolderPtr( (PCL)pFolder ) )
      for( ptl = Amdb_GetFirstTopic( (PCL)pFolder ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
         if( Amdb_GetTopicType( ptl ) == FTYPE_LOCAL )
            return( TRUE );
   return( FALSE );
}

/* This function displays the folder name in 28-pt Times New Roman at
 * the top of each in-basket properties page.
 */
void FASTCALL InBasket_Prop_ShowTitle( HWND hwnd, LPVOID pFolder )
{
   char szName[ 100 ];
   HFONT hOldFont;
   HWND hwndFName;
   SIZE size;
   RECT rc;
   HDC hDC;

   /* Get the folder name.
    */
   ASSERT( hTitleFont != NULL );
   if( Amdb_IsCategoryPtr( (PCAT)pFolder ) )
      strcpy( szName, Amdb_GetCategoryName( (PCAT)pFolder ) );
   else if( Amdb_IsFolderPtr( (PCL)pFolder ) )
      strcpy( szName, Amdb_GetFolderName( (PCL)pFolder ) );
   else
      strcpy( szName, Amdb_GetTopicName( (PTL)pFolder ) );

   /* Adjust name to fit.
    */
   VERIFY( hwndFName = GetDlgItem( hwnd, IDD_FOLDERNAME ) );
   hDC = GetDC( hwndFName );
   hOldFont = SelectFont( hDC, hTitleFont );
   GetClientRect( hwndFName, &rc );
   AdjustNameToFit( hDC, szName, rc.right - rc.left, TRUE, &size );
   SelectFont( hDC, hOldFont );
   ReleaseDC( hwndFName, hDC );

   /* Set the font and the text.
    */
   SetWindowFont( hwndFName, hTitleFont, FALSE );
   SetWindowText( hwndFName, szName );
}

/* This function dispatches messages for the General page of the
 * In Basket properties dialog.
 */
BOOL EXPORT CALLBACK InBasket_Prop1( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, InBasket_Prop1_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, InBasket_Prop1_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, InBasket_Prop1_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL InBasket_Prop1_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPCAMPROPSHEETPAGE psp;
   LPVOID pFolder;

   /* Dereference and save the handle of the category, folder
    * or topic whose properties we're showing.
    */
   psp = (LPCAMPROPSHEETPAGE)lParam;
   pFolder = (LPVOID)psp->lParam;
   SetWindowLong( hwnd, DWL_USER, (LPARAM)pFolder );
   InBasket_Prop_ShowTitle( hwnd, pFolder );

   /* Fill out the page.
    */
   UpdateStatistics( hwnd, pFolder );
   return( TRUE );
}

/* Update the statistics pane of the property sheet.
 */
void FASTCALL UpdateStatistics( HWND hwnd, LPVOID pFolder )
{
   if( Amdb_IsCategoryPtr( (PCAT)pFolder ) )
      {
      CATEGORYINFO cai;
      AM_DATE date;
      AM_TIME time;
      DWORD cPriority;
      DWORD cMarked;
      DWORD cTopics;
      PCL pcl;
      PTL ptl;

      /* Fill out the statistics.
       */
      Amdb_GetCategoryInfo( (PCAT)pFolder, &cai );
      SetDlgItemLongInt( hwnd, IDD_TOTAL, cai.cMsgs );
      SetDlgItemLongInt( hwnd, IDD_UNREAD, cai.cUnRead );
      SetDlgItemLongInt( hwnd, IDD_FOLDERS, cai.cFolders );

      /* List of priority and bookmarks have to be computed by
       * iterating over all folders.
       */
      cPriority = 0;
      cMarked = 0;
      cTopics = 0;
      for( pcl = Amdb_GetFirstFolder( (PCAT)pFolder ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
         for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
            {
            TOPICINFO ti;

            Amdb_GetTopicInfo( ptl, &ti );
            cPriority += ti.cPriority;
            cMarked += ti.cMarked;
            cTopics++;
            }
      SetDlgItemLongInt( hwnd, IDD_BOOKMARKS, cMarked );
      SetDlgItemLongInt( hwnd, IDD_PRIORITY, cPriority );
      SetDlgItemLongInt( hwnd, IDD_TOPICS, cTopics );

      /* Show the creation date and time.
       */
      Amdate_UnpackDate( cai.wCreationDate, &date );
      Amdate_UnpackTime( cai.wCreationTime, &time );
      wsprintf( lpTmpBuf, GS(IDS_STR348), Amdate_FormatShortDate( &date, NULL ), Amdate_FormatTime( &time, NULL ) );
      SetDlgItemText( hwnd, IDD_CREATED, lpTmpBuf );
      }
   else if( Amdb_IsFolderPtr( (PCL)pFolder ) )
      {
      char szFilename[ 13 ];
      FOLDERINFO ci;
      AM_DATE date;
      AM_TIME time;
      DWORD cPriority;
      DWORD cMarked;
      DWORD cTopics;
      PTL ptl;

      /* Fill out the statistics.
       */
      Amdb_GetFolderInfo( (PCL)pFolder, &ci );
      SetDlgItemLongInt( hwnd, IDD_TOTAL, ci.cMsgs );
      SetDlgItemLongInt( hwnd, IDD_UNREAD, ci.cUnRead );

      /* List of priority and bookmarks have to be computed by
       * iterating over all topics.
       */
      cPriority = 0;
      cMarked = 0;
      cTopics = 0;
      for( ptl = Amdb_GetFirstTopic( (PCL)pFolder ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
         {
         TOPICINFO ti;

         Amdb_GetTopicInfo( ptl, &ti );
         cPriority += ti.cPriority;
         cMarked += ti.cMarked;
         cTopics++;
         }
      SetDlgItemLongInt( hwnd, IDD_BOOKMARKS, cMarked );
      SetDlgItemLongInt( hwnd, IDD_PRIORITY, cPriority );
      SetDlgItemLongInt( hwnd, IDD_TOPICS, cTopics );

      /* Show the index file name.
       */
      wsprintf( szFilename, "%s.CNF", (LPSTR)ci.szFileName );
      _strupr( szFilename );
      SetDlgItemText( hwnd, IDD_DATFILENAME, szFilename );

      /* Show the creation date and time.
       */
      Amdate_UnpackDate( ci.wCreationDate, &date );
      Amdate_UnpackTime( ci.wCreationTime, &time );
      wsprintf( lpTmpBuf, GS(IDS_STR348), Amdate_FormatShortDate( &date, NULL ), Amdate_FormatTime( &time, NULL ) );
      SetDlgItemText( hwnd, IDD_CREATED, lpTmpBuf );
      }
   else
      {
      TOPICINFO ti;
      AM_DATE date;
      AM_TIME time;
      DWORD dwSpaceUsed;
      char * pTopicType;
      char szFilename[ 13 ];

      /* Fill out the statistics.
       */
      Amdb_GetTopicInfo( (PTL)pFolder, &ti );
      SetDlgItemLongInt( hwnd, IDD_TOTAL, ti.cMsgs );
      SetDlgItemLongInt( hwnd, IDD_UNREAD, ti.cUnRead );
      SetDlgItemLongInt( hwnd, IDD_BOOKMARKS, ti.cMarked );
      SetDlgItemLongInt( hwnd, IDD_PRIORITY, ti.cPriority );

      /* Show folder type
       */
      switch( Amdb_GetTopicType( (PTL)pFolder ) )
         {
         case FTYPE_NEWS:     pTopicType = GS(IDS_STR873); break;
         case FTYPE_MAIL:     pTopicType = GS(IDS_STR872); break;
         case FTYPE_CIX:      pTopicType = GS(IDS_STR874); break;
         case FTYPE_UNTYPED:  pTopicType = GS(IDS_STR875); break;
         case FTYPE_LOCAL:    pTopicType = GS(IDS_STR876); break;
         default:             pTopicType = "";              break;
         }
      SetDlgItemText( hwnd, IDD_FOLDERTYPE, pTopicType );

      /* Get and show disk space used.
       */
      dwSpaceUsed = Amdb_GetDiskSpaceUsed( (PTL)pFolder );
      wsprintf( lpTmpBuf, "%uK", ( dwSpaceUsed + 1023 ) / 1024 );
      SetDlgItemText( hwnd, IDD_DISKSPACE, lpTmpBuf );

      /* Show the creation date and time.
       */
      Amdate_UnpackDate( ti.wCreationDate, &date );
      Amdate_UnpackTime( ti.wCreationTime, &time );
      wsprintf( lpTmpBuf, GS(IDS_STR348), Amdate_FormatShortDate( &date, NULL ), Amdate_FormatTime( &time, NULL ) );
      SetDlgItemText( hwnd, IDD_CREATED, lpTmpBuf );

      /* Show the filenames.
       */
      wsprintf( szFilename, "%s.THD", (LPSTR)ti.szFileName );
      _strupr( szFilename );
      SetDlgItemText( hwnd, IDD_IDXFILENAME, szFilename );
      wsprintf( szFilename, "%s.TXT", (LPSTR)ti.szFileName );
      _strupr( szFilename );
      SetDlgItemText( hwnd, IDD_DATFILENAME, szFilename );
      }
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL InBasket_Prop1_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_CHECK: {
         LPVOID pFolder;

         pFolder = (LPVOID)GetWindowLong( hwnd, DWL_USER );
         Amdb_CheckAndRepair( GetParent( hwnd ), pFolder, FALSE );
         break;
         }
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL InBasket_Prop1_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP: {
         LPVOID pFolder;

         pFolder = (LPVOID)GetWindowLong( hwnd, DWL_USER );
         if( Amdb_IsCategoryPtr( (PCAT)pFolder ) )
            HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsIBP_DATABASE );
         else if( Amdb_IsFolderPtr( (PCL)pFolder ) )
            HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsIBP_FOLDER );
         else
            HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsIBP_TOPIC );
         break;
         }

      case PSN_SETACTIVE: {
         LPVOID pFolder;

         pFolder = (LPVOID)GetWindowLong( hwnd, DWL_USER );
         if( NULL != pFolder )
            UpdateStatistics( hwnd, pFolder );
         nLastPropertiesPage = IPP_STATISTICS;
         break;
         }

      case PSN_APPLY: {
         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This function dispatches messages for the Signature page of the
 * In Basket properties dialog.
 */
BOOL EXPORT CALLBACK InBasket_Prop3( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, InBasket_Prop3_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, InBasket_Prop3_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, InBasket_Prop3_OnNotify );
      HANDLE_DLGMSG( hwnd, WM_DRAWITEM, Signature_OnDrawItem );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL InBasket_Prop3_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPCAMPROPSHEETPAGE psp;
   HWND hwndList;
   int index;
   LPVOID pFolder;

   /* Dereference and save the handle of the category, folder
    * or topic whose properties we're showing.
    */
   psp = (LPCAMPROPSHEETPAGE)lParam;
   pFolder = (LPVOID)psp->lParam;
   SetWindowLong( hwnd, DWL_USER, (LPARAM)pFolder );
   InBasket_Prop_ShowTitle( hwnd, pFolder );

   /* Fill list of signatures.
    */
   FillSignatureList( hwnd, IDD_LIST );

   /* Select the default for this folder.
    */
   if( Amdb_IsTopicPtr( (PTL)pFolder ) )
      {
      TOPICINFO topicinfo;

      Amdb_GetTopicInfo( (PTL)pFolder, &topicinfo );
      VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
      if( ( index = ComboBox_FindStringExact( hwndList, -1, topicinfo.szSigFile ) ) == CB_ERR )
         index = -1;
      ComboBox_SetCurSel( hwndList, index );
      }
   else if( Amdb_IsFolderPtr( (PCL)pFolder ) )
      {
      FOLDERINFO folderinfo;

      Amdb_GetFolderInfo( (PCL)pFolder, &folderinfo );
      VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
      if( ( index = ComboBox_FindStringExact( hwndList, -1, folderinfo.szSigFile ) ) == CB_ERR )
         index = -1;
      ComboBox_SetCurSel( hwndList, index );
      }
   else
      {
      FOLDERINFO folderinfo;
      char szSigFile[ 9 ];
      PCL pcl;

      /* Scan all folder signatures and if they match, select that signature
       * from the list. Otherwise don't select anything.
       */
      VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
      szSigFile[ 0 ] = '\0';
      for( pcl = Amdb_GetFirstFolder( (PCAT)pFolder ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
         {
         Amdb_GetFolderInfo( pcl, &folderinfo );
         if( *szSigFile && strcmp( folderinfo.szSigFile, szSigFile ) )
            break;
         strcpy( szSigFile, folderinfo.szSigFile );
         }
      if( NULL == pcl )
         {
         if( ( index = ComboBox_FindStringExact( hwndList, -1, szSigFile ) ) == CB_ERR )
            index = -1;
         ComboBox_SetCurSel( hwndList, index );
         }
      }
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL InBasket_Prop3_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_LIST:
         if( codeNotify == CBN_SELCHANGE )
         {
            UpdateSignature( hwnd );
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         }

         break;

      case IDD_EDIT: {
         HWND hwndList;
         char sz[ 13 ];
         int index;

         /* Edit the selected signature.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         if( CB_ERR != ( index = ComboBox_GetCurSel( hwndList ) ) )
            ComboBox_GetLBText( hwndList, index, sz );
         else
            *sz = '\0';
         CmdSignature( GetParent( hwnd ), sz );

         /* Refill the signature list again.
          */
         FillSignatureList( hwnd, IDD_LIST );
         if( ( index = ComboBox_FindStringExact( hwndList, -1, sz ) ) == CB_ERR )
            index = -1;
         ComboBox_SetCurSel( hwndList, index );
         UpdateSignature( hwnd );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;
         }
      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL InBasket_Prop3_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsIBP_SIGNATURE );
         break;

      case PSN_SETACTIVE:
         nLastPropertiesPage = IPP_SIGNATURE;
         break;

      case PSN_APPLY: {
         HWND hwndList;
         int index;

         /* Get the selected signature.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         if( CB_ERR != ( index = ComboBox_GetCurSel( hwndList ) ) )
            {
            char sz[ 13 ];
            LPVOID pFolder;

            /* Get the new signature.
             */
            ComboBox_GetLBText( hwndList, index, sz );

            /* Get the folder to which signature applies.
             */
            pFolder = (LPVOID)GetWindowLong( hwnd, DWL_USER );
            if( Amdb_IsTopicPtr( (PTL)pFolder ) )
               Amdb_SetTopicSignature( (PTL)pFolder, sz );
            else if( Amdb_IsCategoryPtr( (PCAT)pFolder) )
               {
               PCL pcl;
               PTL ptl;

               /* Set the signature for all folders.
                */
               for( pcl = Amdb_GetFirstFolder( (PCAT)pFolder ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
                  {
                  for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
                     Amdb_SetTopicSignature( ptl, sz );
                  Amdb_SetFolderSignature( pcl, sz );
                  }
               }
            else if( Amdb_IsFolderPtr( (PCL)pFolder ) )
               {
               char szOldFolderSig[ 13 ];

               /* When changing the folder signature, offer to
                * change the signature for all topics too.
                */
               Amdb_GetFolderSignature( (PCL)pFolder, szOldFolderSig );
               if( strcmp( sz, szOldFolderSig ) != 0 )
                  {
                  if( fMessageBox( hwnd, 0, GS(IDS_STR623), MB_YESNO|MB_ICONQUESTION ) == IDYES )
                     {
                     PTL ptl;

                     for( ptl = Amdb_GetFirstTopic( (PCL)pFolder ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
                        Amdb_SetTopicSignature( ptl, sz );
                     }
                  Amdb_SetFolderSignature( (PCL)pFolder, sz );
                  }
               }
            }

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This function handles drawing the selected signature.
 */
void FASTCALL Signature_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItemStruct )
{
   PaintSignature( hwnd, lpDrawItemStruct->hDC, &lpDrawItemStruct->rcItem );
}

/* This function updates the signature preview.
 */
void FASTCALL UpdateSignature( HWND hwnd )
{
   HWND hwndSig;
   HDC hdc;
   RECT rc;

   hwndSig = GetDlgItem( hwnd, IDD_SIGPICTURE );
   GetClientRect( hwndSig, &rc );
   hdc = GetDC( hwndSig );
   PaintSignature( hwnd, hdc, &rc );
   ReleaseDC( hwndSig, hdc );
}

/* This function paints a signature.
 */
void FASTCALL PaintSignature( HWND hwnd, HDC hdc, const RECT FAR * lprc )
{
   HPEN hpenBlack;
   char sz[ 13 ];
   HWND hwndList;
   HPEN hpenOld;
   HBRUSH hbr;
   LPSTR lpsz;
   int index;
   RECT rc;

   /* Get the name of the selected signature.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   if( CB_ERR != ( index = ComboBox_GetCurSel( hwndList ) ) )
      ComboBox_GetLBText( hwndList, index, sz );
   else
      *sz = '\0';
   lpsz = GetEditSignature( sz );

   /* Draw the frame.
    */
   rc = *lprc;
   hpenBlack = CreatePen( PS_SOLID, 0, GetSysColor( COLOR_WINDOWTEXT ) );
   hpenOld = SelectPen( hdc, hpenBlack );
   MoveToEx( hdc, 0, rc.top, NULL );
   LineTo( hdc, 0, rc.bottom - 2 );
   LineTo( hdc, rc.right - 2, rc.bottom - 2 );
   LineTo( hdc, rc.right - 2, rc.top - 1 );
   MoveToEx( hdc, rc.right - 1, rc.top + 1, NULL );
   LineTo( hdc, rc.right - 1, rc.bottom - 1 );
   LineTo( hdc, rc.left + 1, rc.bottom - 1 );

   /* Fill the sample area with the window colour.
    */
   rc.left += 1;
   rc.bottom -= 2;
   rc.right -= 2;
   hbr = CreateSolidBrush( GetSysColor( COLOR_WINDOW ) );
   FillRect( hdc, &rc, hbr );
   DeleteBrush( hbr );
   rc.left += 3;
   rc.right -= 3;
   rc.bottom -= 6;

   /* Draw the signature text if we have one.
    */
   if( NULL != lpsz )
      {
      HFONT hOldFont;

      /* Select the 6pt font into the DC.
       */
      hOldFont = SelectFont( hdc, hDlg6Font );
      SetTextColor( hdc, GetSysColor( COLOR_WINDOWTEXT ) );
      SetBkColor( hdc, GetSysColor( COLOR_WINDOW ) );
      DrawText( hdc, lpsz, -1, &rc, DT_LEFT|DT_WORDBREAK|DT_NOPREFIX );

      /* Clean up after drawing signature text.
       */
      SelectFont( hdc, hOldFont );
      FreeMemory( &lpsz );
      }

   /* Clean up afterwards.
    */
   SelectPen( hdc, hpenOld );
   DeletePen( hpenBlack );
}

/* This function dispatches messages for the General page of the
 * In Basket properties dialog.
 */
BOOL EXPORT CALLBACK InBasket_Prop5( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_DLGMSG( hwnd, WM_INITDIALOG, InBasket_Prop5_OnInitDialog );
      HANDLE_DLGMSG( hwnd, WM_COMMAND, InBasket_Prop5_OnCommand );
      HANDLE_DLGMSG( hwnd, WM_NOTIFY, InBasket_Prop5_OnNotify );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL InBasket_Prop5_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPCAMPROPSHEETPAGE psp;
   TOPICFLAGSTATE tfs;
   LPVOID pFolder;
   char szListMailAddress[ LEN_MAILADDR ];
   char szExtraHeaderText[ 100 ];

   /* Dereference and save the handle of the category, folder
    * or topic whose properties we're showing.
    */
   psp = (LPCAMPROPSHEETPAGE)lParam;
   pFolder = (LPVOID)psp->lParam;
   SetWindowLong( hwnd, DWL_USER, (LPARAM)pFolder );
   InBasket_Prop_ShowTitle( hwnd, pFolder );

   /* Initialise the structure.
    */
   tfs.fResigned = 3;
   tfs.fReadOnly = 3;
   tfs.fIgnored = 3;
   tfs.fHasFiles = 3;
   tfs.fReadFull = 3;
   tfs.fMailingList = 3;
   tfs.fListFolder = 3;

   /* Fill out the page.
    */
   if( Amdb_IsTopicPtr( (PTL)pFolder ) )
      ReadTopicFlags( (PTL)pFolder, &tfs );
   else if( Amdb_IsFolderPtr( (PCL)pFolder ) )
      {
      PTL ptl;

      for( ptl = Amdb_GetFirstTopic( (PCL)pFolder ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
         ReadTopicFlags( ptl, &tfs );
      }
   else
      {
      PCL pcl;
      PTL ptl;

      for( pcl = Amdb_GetFirstFolder( (PCAT)pFolder ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
         for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
            ReadTopicFlags( ptl, &tfs );
      }

   /* Set the various options.
    */
   CheckTopicFlagButton( hwnd, IDD_RESIGNED, tfs.fResigned );
   CheckTopicFlagButton( hwnd, IDD_READONLY, tfs.fReadOnly );
   CheckTopicFlagButton( hwnd, IDD_IGNORED, tfs.fIgnored );
   CheckTopicFlagButton( hwnd, IDD_HASFILES, tfs.fHasFiles );
   CheckTopicFlagButton( hwnd, IDD_FULLARTICLES, tfs.fReadFull );
   CheckTopicFlagButton( hwnd, IDD_MAILINGLIST, tfs.fMailingList );
   if( tfs.fListFolder == 1 )
   {
   CheckTopicFlagButton( hwnd, IDD_LISTFOLDER, tfs.fListFolder );
   EnableControl( hwnd, IDD_LISTFOLDERADDRESS, tfs.fListFolder );
   Amdb_GetCookie( (PTL)pFolder, MAIL_LIST_ADDRESS, szListMailAddress, "", LEN_MAILADDR );
   Edit_SetText( GetDlgItem( hwnd, IDD_LISTFOLDERADDRESS ), szListMailAddress );
   Edit_LimitText( GetDlgItem( hwnd, IDD_LISTFOLDERADDRESS ), LEN_MAILADDR - 1 );
   }
   else
   {
      EnableControl( hwnd, IDD_LISTFOLDERADDRESS, FALSE );
      if( Amdb_IsTopicPtr( (PTL)pFolder ) &&    Amdb_GetTopicType( (PTL)pFolder ) == FTYPE_MAIL )
         CheckTopicFlagButton( hwnd, IDD_LISTFOLDER, 0 );
      else
      {
      CheckTopicFlagButton( hwnd, IDD_LISTFOLDER, 3 );
      ShowWindow( GetDlgItem( hwnd, IDD_LISTFOLDER ), SW_HIDE );
      ShowWindow( GetDlgItem( hwnd, IDD_LISTFOLDERADDRESS ), SW_HIDE );
      ShowWindow( GetDlgItem( hwnd, 65532 ), SW_HIDE );
      ShowWindow( GetDlgItem( hwnd, 65534 ), SW_HIDE );
      ShowWindow( GetDlgItem( hwnd, 65533 ), SW_HIDE );
      }
         

   }
   if( Amdb_IsTopicPtr( (PTL)pFolder ) &&    Amdb_GetTopicType( (PTL)pFolder ) == FTYPE_MAIL )
   {
      Amdb_GetCookie( (PTL)pFolder, SUBJECT_IGNORE_TEXT, szListMailAddress, "", 79 );
      Edit_SetText( GetDlgItem( hwnd, IDD_SUBIGNORE ), szListMailAddress );
      Edit_LimitText( GetDlgItem( hwnd, IDD_SUBIGNORE ), 79 );
      Amdb_GetCookie( (PTL)pFolder, EXTRA_HEADER_TEXT, szExtraHeaderText, "", 99 );
      Edit_SetText( GetDlgItem( hwnd, IDD_EXTRAHEADER ), szExtraHeaderText );
      Edit_LimitText( GetDlgItem( hwnd, IDD_EXTRAHEADER ), 99 );
      SetRROption( GetDlgItem( hwnd, IDD_RR), (PTL)pFolder );
   }
   else
   {
      ShowWindow( GetDlgItem( hwnd, IDD_RR ), SW_HIDE );
      ShowWindow( GetDlgItem( hwnd, IDD_SUBIGNORE ), SW_HIDE );
      ShowWindow( GetDlgItem( hwnd, IDD_EXTRAHEADER ), SW_HIDE );
      ShowWindow( GetDlgItem( hwnd, IDD_EXTRAHEADERTEXT ), SW_HIDE );
//    EnableControl( hwnd, IDD_RR, FALSE );
//    EnableControl( hwnd, IDD_SUBIGNORE, FALSE );
   }


   return( TRUE );
}

/* This function sets the topic flag based on the specified state. If
 * the state is 3, the flag is not used and is disabled.
 */
void FASTCALL CheckTopicFlagButton( HWND hwnd, int id, unsigned fState )
{
   if( 3 == fState )
      EnableControl( hwnd, id, FALSE );
   else {
      if( fState == 2 )
         {
         HWND hwndCtl;

         /* Make the control tri-state.
          */
         hwndCtl = GetDlgItem( hwnd, id );
         SetWindowStyle( hwndCtl, ( GetWindowStyle( hwndCtl ) & ~0xFF ) | BS_AUTO3STATE );
         }
      CheckDlgButton( hwnd, id, fState );
      }
}

/* This function adjusts the flags in the specified topic flag
 * state structure based on the topic flags.
 */
void FASTCALL ReadTopicFlags( PTL ptl, TOPICFLAGSTATE * ptfs )
{
   TOPICINFO topicinfo;

   Amdb_GetTopicInfo( ptl, &topicinfo );
   if( Amdb_GetTopicType( ptl ) == FTYPE_CIX )
      SetTopicFlagState( &ptfs->fHasFiles, TestF(topicinfo.dwFlags, TF_HASFILES) );
   if( Amdb_GetTopicType( ptl ) == FTYPE_NEWS )
      SetTopicFlagState( &ptfs->fReadFull, TestF(topicinfo.dwFlags, TF_READFULL) );
   if( Amdb_GetTopicType( ptl ) == FTYPE_MAIL )
      SetTopicFlagState( &ptfs->fMailingList, TestF(topicinfo.dwFlags, TF_MAILINGLIST) );
   if( Amdb_GetTopicType( ptl ) == FTYPE_MAIL )
      SetTopicFlagState( &ptfs->fListFolder, TestF(topicinfo.dwFlags, TF_LISTFOLDER) );
   if( Amdb_GetTopicType( ptl ) != FTYPE_MAIL )
      {
      SetTopicFlagState( &ptfs->fResigned, TestF(topicinfo.dwFlags, TF_RESIGNED) );
      SetTopicFlagState( &ptfs->fReadOnly, TestF(topicinfo.dwFlags, TF_READONLY) );
      }
   SetTopicFlagState( &ptfs->fIgnored, TestF(topicinfo.dwFlags, TF_IGNORED) );
}

/* This function adjusts the topic flag state based on the
 * new boolean state.
 */
void FASTCALL SetTopicFlagState( unsigned * pfFlag, BOOL fNewState )
{
   static int TopicFlagTable[] = { 0, 2, 2, 1, 2, 2, 0, 1 };
   int index;

   ASSERT( *pfFlag <= 3 );
   index = fNewState ? 1 : 0;
   *pfFlag = TopicFlagTable[ *pfFlag * 2 + index ];
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL InBasket_Prop5_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_RESIGNED:
      case IDD_READONLY:
      case IDD_IGNORED:
      case IDD_HASFILES:
      case IDD_FULLARTICLES:
      case IDD_MAILINGLIST:
      case IDD_LISTFOLDERADDRESS:
      case IDD_RR:
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_EXTRAHEADER:
      case IDD_SUBIGNORE:
         if( codeNotify == EN_UPDATE )
            PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      case IDD_LISTFOLDER:
         EnableControl( hwnd, IDD_LISTFOLDERADDRESS, IsDlgButtonChecked( hwnd, IDD_LISTFOLDER ) );
         PropSheet_Changed( GetParent( hwnd ), hwnd );
         break;

      }
}

/* This function handles the WM_NOTIFY message.
 */ 
LRESULT FASTCALL InBasket_Prop5_OnNotify( HWND hwnd, int code, LPNMHDR lpnmhdr )
{
   switch( lpnmhdr->code )
      {
      case PSN_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsIBP_SETTINGS );
         break;

      case PSN_SETACTIVE:
         nLastPropertiesPage = IPP_SETTINGS;
         break;

      case PSN_APPLY: {
         LPVOID pFolder;
         PTL ptl;
         char szListMailAddress[ 120 ];
         char szExtraHeaderText[ 100 ];

         /* Get the folder to which this applies.
          */
         pFolder = (LPVOID)GetWindowLong( hwnd, DWL_USER );
         if( Amdb_IsTopicPtr( (PTL)pFolder ) )
            StoreTopicFlags( hwnd, (PTL)pFolder );
         else if( Amdb_IsFolderPtr( (PCL)pFolder ) )
            {
            PTL ptl;

            for( ptl = Amdb_GetFirstTopic( (PCL)pFolder ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
               StoreTopicFlags( hwnd, ptl );
            }
         else
            {
            PCL pcl;
            PTL ptl;

            for( pcl = Amdb_GetFirstFolder( (PCAT)pFolder ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
               /* Bug fix: GPF when changing ADMIN properties.
                * YH 21/06/96 9:50
                */
               for( ptl = Amdb_GetFirstTopic( (PCL)pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
                  StoreTopicFlags( hwnd, ptl );
            }


         /* Get the newsgroup topic handle.
          */
         ptl = (PTL)GetWindowLong( hwnd, DWL_USER );

         if( Amdb_IsTopicPtr( ptl ) )
         {
         Edit_GetText( GetDlgItem( hwnd, IDD_LISTFOLDERADDRESS ), szListMailAddress, LEN_MAILADDR );
         if( Amdb_GetTopicFlags( ptl, TF_LISTFOLDER ) && strlen( szListMailAddress ) == 0 )
         {
            fMessageBox( hwnd, 0, "You must enter an address", MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_LISTFOLDERADDRESS );
            return( PSNRET_INVALID_NOCHANGEPAGE );
         }
         /* Save cookies for poster details.
          */
         if( strlen( szListMailAddress ) >  0 )
            Amdb_SetCookie( ptl, MAIL_LIST_ADDRESS, szListMailAddress );
         if( Amdb_GetTopicType( (PTL)pFolder ) == FTYPE_MAIL )
            GetRROption( GetDlgItem( hwnd, IDD_RR), (PTL)pFolder );
         Edit_GetText( GetDlgItem( hwnd, IDD_SUBIGNORE ), szListMailAddress, 80 );
         Amdb_SetCookie( ptl, SUBJECT_IGNORE_TEXT, szListMailAddress );
         Edit_GetText( GetDlgItem( hwnd, IDD_EXTRAHEADER ), szExtraHeaderText, 100 );
         Amdb_SetCookie( ptl, EXTRA_HEADER_TEXT, szExtraHeaderText );
         }

         /* Force the Apply button to be disabled.
          */
         PropSheet_UnChanged( GetParent( hwnd ), hwnd );
         return( PSNRET_NOERROR );
         }
      }
   return( FALSE );
}

/* This function updates the topic flags from the settings
 * dialog page.
 */
void FASTCALL StoreTopicFlags( HWND hwnd, PTL ptl )
{
   TOPICINFO topicinfo;
   DWORD dwSetFlag;
   DWORD dwClrFlag;

   /* First collect the new flag settings
    */
   Amdb_GetTopicInfo( ptl, &topicinfo );
   dwSetFlag = 0L;
   dwClrFlag = 0L;
   DobbleSetClrFlags( hwnd, IDD_RESIGNED, &dwSetFlag, &dwClrFlag, TF_RESIGNED );
   DobbleSetClrFlags( hwnd, IDD_READONLY, &dwSetFlag, &dwClrFlag, TF_READONLY );
   DobbleSetClrFlags( hwnd, IDD_IGNORED, &dwSetFlag, &dwClrFlag, TF_IGNORED );
   DobbleSetClrFlags( hwnd, IDD_HASFILES, &dwSetFlag, &dwClrFlag, TF_HASFILES );
   DobbleSetClrFlags( hwnd, IDD_FULLARTICLES, &dwSetFlag, &dwClrFlag, TF_READFULL );
   DobbleSetClrFlags( hwnd, IDD_MAILINGLIST, &dwSetFlag, &dwClrFlag, TF_MAILINGLIST );
   DobbleSetClrFlags( hwnd, IDD_LISTFOLDER, &dwSetFlag, &dwClrFlag, TF_LISTFOLDER );

   /* If the flags have changed, compute the delta for those
    * flags which have been cleared and those which have been
    * set.
    */
   if( dwSetFlag || dwClrFlag )
      {
      if( dwClrFlag )   Amdb_SetTopicFlags( ptl, dwClrFlag, FALSE );
      if( dwSetFlag )   Amdb_SetTopicFlags( ptl, dwSetFlag, TRUE );
      }
}

/* I've invented a new word. To dobble means to test the state of a flag
 * and set a bit in one or more bitflags to represent the flag state.
 */
void FASTCALL DobbleSetClrFlags( HWND hwnd, int id, DWORD * pdwSetFlag, DWORD * pdwClrFlag, DWORD dwFlag )
{
   switch( IsDlgButtonChecked( hwnd, id ) )
      {
      case 0:  *pdwClrFlag |= dwFlag; break;
      case 1:  *pdwSetFlag |= dwFlag; break;
      }
}

/* This function handles the Open Password dialog box.
 */
BOOL EXPORT CALLBACK BuildDlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = BuildDlgProc_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the BuildDlgProc
 *  dialog.
 */
LRESULT FASTCALL BuildDlgProc_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, BuildDlgProc_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, BuildDlgProc_OnCommand );

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL BuildDlgProc_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL BuildDlgProc_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDCANCEL:
      case IDOK:
         fParsing = FALSE;
         break;
      }
}

void FASTCALL InBasket_SortCommand( HWND hwnd )
{
   HWND hwndFolder;

   hwndFolder = GetActiveFolderWindow();
   if( NULL == hwndFolder && hwndMsg )
      hwndFolder = hwndMsg;
   if( NULL != hwndFolder )
      {

   HTREEITEM hSelItem;
   HWND hwndTreeCtl;
   
   OfflineStatusText( "Sorting in basket, please wait..." );
   hwndTreeCtl = GetDlgItem( hwndFolder, IDD_FOLDERLIST );

   if( hSelItem = TreeView_GetSelection( hwndTreeCtl ) )
   {
      TV_ITEM tv;

      TreeView_SortChildren( hwndTreeCtl, hSelItem, FALSE );

      /* Get the lParam of the selected item and display
       * the Properties dialog.
       */
      tv.hItem = hSelItem;
      tv.mask = TVIF_PARAM;
      VERIFY( TreeView_GetItem( hwndTreeCtl, &tv ) );

      if( Amdb_IsCategoryPtr( (PCAT)tv.lParam ) )
      {
         PCL pcl1;
         PCL pcl2;
         LPCSTR pszText1;
         LPCSTR pszText2;
         BOOL fSwapped;
         CATEGORYINFO ci;
         HWND hwndBill = NULL;
         HWND hwndProgress = NULL;

         Amdb_GetCategoryInfo( (PCAT)tv.lParam, &ci );
         if( ci.cFolders > SORT_FOLDER_LIMIT )
         {
            hwndBill = Adm_Billboard( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_SORTINGFOLDERS) );
            hwndProgress = GetDlgItem( hwndBill, IDD_PROGRESS );
         }
         fSwapped = TRUE;
         pcatOriginalCategory = (PCAT)tv.lParam;
         while ( fSwapped )
         {
            pcl1 = Amdb_GetFirstFolder( (PCAT)tv.lParam );
            if( NULL == pcl1 )
               break;
            fSwapped = FALSE;
            while( Amdb_GetNextFolder( pcl1 ) && Amdb_CategoryFromFolder( Amdb_GetNextFolder( pcl1 ) ) == (PCAT)tv.lParam )
            {
               if( NULL != hwndProgress && ( ( GetTickCount() % 125 ) == 0 ) )
                  SetWindowText( hwndProgress, "*" );
               pcl2 = Amdb_GetNextFolder( pcl1 );
               pszText1 = Amdb_GetFolderName( pcl1 );
               pszText2 = Amdb_GetFolderName( pcl2 );
               if( ( lstrcmpi( pszText1, pszText2 ) > 0 && Amdb_GetFolderFlags( pcl1, CF_KEEPATTOP ) != CF_KEEPATTOP ) )
               {
                  Amdb_MoveFolder( pcl1, (PCAT)tv.lParam, pcl2 );
                  fSwapped = TRUE;
                  if( NULL != hwndProgress && ( ( GetTickCount() % 250 ) == 0 ) )
                     SetWindowText( hwndProgress, " " );
               }
            pcl1 = pcl2;
            }
         }
         if( ci.cFolders > SORT_FOLDER_LIMIT )
            DestroyWindow( hwndBill );
      }
      else if( Amdb_IsFolderPtr( (PCL)tv.lParam ) )
      {
         PTL ptl1;
         PTL ptl2;
         LPCSTR pszText1;
         LPCSTR pszText2;
         BOOL fSwapped;
         fSwapped = TRUE;
         pclOriginalFolder = (PCL)tv.lParam;
         while ( fSwapped )
         {
            ptl1 = Amdb_GetFirstTopic( (PCL)tv.lParam );
            if( NULL == ptl1 )
               break;
            fSwapped = FALSE;
            while( Amdb_GetNextTopic( ptl1 ) && Amdb_FolderFromTopic( Amdb_GetNextTopic( ptl1 ) ) == (PCL)tv.lParam )
            {
               ptl2 = Amdb_GetNextTopic( ptl1 );
               pszText1 = Amdb_GetTopicName( ptl1 );
               pszText2 = Amdb_GetTopicName( ptl2 );
               if( lstrcmpi( pszText1, pszText2 ) > 0 )
               {
                  Amdb_MoveTopic( ptl1, (PCL)tv.lParam, ptl2 );
                  fSwapped = TRUE;
               }
            ptl1 = ptl2;
            }
         }
      }
      SendAllWindows( WM_REDRAWWINDOW, WIN_INBASK, 0L );
   }
   ShowUnreadMessageCount();
   }
}


void FASTCALL SetRROption( HWND hwnd, PTL ptl )
{
   int nRRMode;
   register int c;
   int index;
   char szRR[ 20];

   Amdb_GetCookie( ptl, RR_TYPE, szRR, "", 20 );
   if( strlen( szRR ) == 0 )
      strcpy( szRR, "100" );
   nRRMode = atoi( szRR );
      for( c = 0; c < cRROpts; ++c )
      {
      index = ComboBox_AddString( hwnd, szRROpts[ c ].pszName );
      ComboBox_SetItemData( hwnd, index, szRROpts[ c ].nMode );
      }
   index = RealComboBox_FindItemData( hwnd, -1, nRRMode );
   ComboBox_SetCurSel( hwnd, index );
}

/* This function retrieves and sets the advance option.
 */
void FASTCALL GetRROption( HWND hwnd, PTL ptl )
{
   int nRRMode;
   int index;
   char szRR[ 20];

   index = ComboBox_GetCurSel( hwnd );
   ASSERT( index != CB_ERR );
   nRRMode = (int)ComboBox_GetItemData( hwnd, index );
   _itoa( nRRMode, szRR, 10 );
   Amdb_SetCookie( ptl, RR_TYPE, szRR );
}

/* This function prints one of the various lists
 */
void FASTCALL CmdPrintInbasket( HWND hwnd )
{
   HPRINT hPr;
   BOOL fOk;

   /* Show and get the print terminal dialog box.
    */
   if( !Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_PRINTINBASKET), PrintInbasketDlg, 0L ) )
      return;

   /* Start printing.
    */
   GetWindowText( hwnd, lpTmpBuf, LEN_TEMPBUF );
   fOk = TRUE;
   if( NULL != ( hPr = BeginPrint( hwnd, lpTmpBuf, &lfFixedFont ) ) )
      {
      register int c;

      for( c = 0; fOk && c < cCopies; ++c )
         {
         PCAT pcat;
         PCL pcl;
         PTL ptl;
         BOOL fMod = FALSE;
         BOOL fResigned = FALSE;

         /* Print the header.
          */
         ResetPageNumber( hPr );
         fOk = BeginPrintPage( hPr );
         for( pcat = Amdb_GetFirstCategory(); fOk && pcat; pcat = Amdb_GetNextCategory( pcat ) )
         {
            char szPrintBuf[ 100 ];
            strcpy( szPrintBuf, (LPSTR)Amdb_GetCategoryName( pcat ) );
            if( fOk )
               fOk = PrintLine( hPr, szPrintBuf );
            for( pcl = Amdb_GetFirstFolder( pcat ); fOk && pcl; pcl = Amdb_GetNextFolder( pcl ) )
            {
               char szPrintBuf2[ 200 ];
               strcpy( szPrintBuf2, "   " );
               strcat( szPrintBuf2, (LPSTR)Amdb_GetFolderName( pcl ) );
               fResigned = Amdb_IsResignedFolder( pcl );
               fMod = Amdb_IsModerator( pcl, szCIXNickname );
               if( fPrintConfsOnly && ( fResigned || fMod ) )
               {
               strcat( szPrintBuf2, " (" );
               if( fResigned )
                  strcat( szPrintBuf2, "X" );
               if( fMod )
                  strcat( szPrintBuf2, "M" );
               strcat( szPrintBuf2, ")" );
               }
               else if( fMod )
                  strcat( szPrintBuf2, " (M)" );
               if( fOk )
                  fOk = PrintLine( hPr, szPrintBuf2 );
               if( !fPrintConfsOnly )
               {
                  TOPICINFO ti;
                  for( ptl = Amdb_GetFirstTopic( pcl ); fOk && ptl; ptl = Amdb_GetNextTopic( ptl ) )
                  {
                     char szPrintBuf3[ 1000 ];
                     strcpy( szPrintBuf3, "    " );
                     strcat( szPrintBuf3, (LPSTR)Amdb_GetTopicName( ptl ) );
                     Amdb_GetTopicInfo( ptl, &ti );
                     if( Amdb_GetTopicType( ptl ) == FTYPE_LOCAL || (ti.dwFlags & (TF_HASFILES|TF_READONLY|TF_RESIGNED ) ) )
                     {
                     strcat( szPrintBuf3, " (" );
                     if( Amdb_GetTopicType( ptl ) == FTYPE_LOCAL )
                        strcat( szPrintBuf3, "L" );
                     if( ti.dwFlags & TF_HASFILES )
                        strcat( szPrintBuf3, "F" );
                     if( ti.dwFlags & TF_READONLY )
                        strcat( szPrintBuf3, "R" );
                     if( ti.dwFlags & TF_RESIGNED )
                        strcat( szPrintBuf3, "X" );
                     strcat( szPrintBuf3, ")" );
                     }

                     if( fOk )
                        fOk = PrintLine( hPr, szPrintBuf3 );
                  }
               }
            }
         }
         if( fOk )
            EndPrintPage( hPr );
         }
      EndPrint( hPr );
      }
}

/* This function handles the Print dialog.
 */
BOOL EXPORT CALLBACK PrintInbasketDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = PrintInbasketDlg_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the Print Message
 * dialog.
 */
LRESULT FASTCALL PrintInbasketDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, PrintInbasketDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, PrintInbasketDlg_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPRINTINBASKET );
         break;
      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL PrintInbasketDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndSpin;

   /* Set the input field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_COPIES ), 2 );

   /* Set option as appropriate.
    */
   CheckDlgButton( hwnd, IDD_CONFS, fPrintConfsOnly );
   CheckDlgButton( hwnd, IDD_TOPICS, !fPrintConfsOnly );

   /* Set number of copies.
    */
   SetDlgItemInt( hwnd, IDD_COPIES, 1, FALSE );
   hwndSpin = GetDlgItem( hwnd, IDD_SPIN );
   SendMessage( hwndSpin, UDM_SETBUDDY, (WPARAM)GetDlgItem( hwnd, IDD_COPIES ), 0L );
   SendMessage( hwndSpin, UDM_SETRANGE, 0, MAKELPARAM( 99, 1 ) );
   SendMessage( hwndSpin, UDM_SETPOS, 0, MAKELPARAM( 1, 0 ) );

   /* Display current printer name.
    */
   UpdatePrinterName( hwnd );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL PrintInbasketDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
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
         fPrintConfsOnly = IsDlgButtonChecked( hwnd, IDD_CONFS );
         EndDialog( hwnd, TRUE );
         break;

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}

