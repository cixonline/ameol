/* AMADDR.C - Ameol Address Book
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
#include "resource.h"
#include "palrc.h"
#include <memory.h>
#include <string.h>
#include "amaddr.h"
#include "amaddri.h"
#include "shared.h"
#include "rules.h"
#include "cix.h"
#include "ctype.h"
#include "lbf.h"
#include "print.h"

/* Intellimouse support.
 */
#ifdef WIN32
#include "zmouse.h"
#endif

#define  THIS_FILE   __FILE__

#define  AB_VERSION        0xF45F

#define  ADDR_FILE_FTYPE      0x3945
#define  ADDR_FILE_FVERSION   902
#define  LEN_FINDTEXT         32
#define ADDLOC_BCC            3
#define ADDLOC_CC          2
#define ADDLOC_TO          1
#define ADDLOC_NONE           0

extern BOOL fIsAmeol2Scratchpad;

static BOOL fDefDlgEx = FALSE;

char szAddrWndClass[] = "amctl_addrwndcls";
char szAddrWndName[] = "Address Book";
char szAddrBook[] = "Address Book";

int  fLastChar = 0; // !!SM!! 2.55
char fLastTyped[255];

HWND hwndAddrBook = NULL;                 /* Address picker window handle */

static BOOL fRegistered = FALSE;          /* TRUE if we've registered the address book window */
static UINT uMailMsg;                     /* Registered mail command */
static int hdrColumns[ 3 ];                  /* Header columns */
static BOOL fQuickUpdate = FALSE;            /* TRUE if we quick update the address book window */
static char szAddrBookPath[ _MAX_PATH ];     /* Path to address book files */
static UINT nSortColumn = 0;              /* Index of sort column */
static HFONT hAddrBookFontN;              /* Address book font */
static HFONT hAddrBookFontB;              /* Boldface version of address book font */
static WNDPROC lpProcListCtl;             /* Subclassed listbox window procedure */
static HBRUSH hbrAddrBook;                /* Brush used to paint listbox */
static HWND hwndTooltip;                  /* Tooltip in listbox */
static LPSTR lpGrpList = NULL;               /* List of groups for tooltip */

static HBITMAP hbmpUser;               /* User Bitmap */
static HBITMAP hbmpGroup;              /* Group Bitmap */

static char * szHeaderCaption[] = {
   "Name",
   "Address",
   "Comment"
   };

LPADDRBOOK lpAddrBookFirst = NULL;     /* First address book */
static BOOL fAddrBookLoaded = FALSE;         /* TRUE if address book loaded */

LPADDRBOOK lpGlbAddrBook = NULL;             /* Generic global addr book handle */

const char NEAR szAddrBookFile[] = "addrbook.dat";
const char NEAR szDefAddrBook[] = "addrbook.def";

BOOL FASTCALL AddrbookWnd_OnCreate( HWND, LPCREATESTRUCT );
void FASTCALL AddrbookWnd_OnClose( HWND );

void FASTCALL AddrbookWnd_OnSize( HWND, UINT, int, int );
void FASTCALL AddrbookWnd_OnMove( HWND, int, int );
void FASTCALL AddrbookWnd_MDIActivate( HWND, BOOL, HWND, HWND );
void FASTCALL AddrbookWnd_OnCommand( HWND, int, HWND, UINT );
void FASTCALL AddrbookWnd_OnTimer( HWND, UINT );
LRESULT FASTCALL AddrbookWnd_OnNotify( HWND, int, LPNMHDR );
void FASTCALL AddrbookWnd_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
void FASTCALL AddrbookWnd_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
int FASTCALL AddrbookWnd_OnCompareItem( HWND, const COMPAREITEMSTRUCT FAR * );
int FASTCALL AddrbookWnd_OnCharToItem( HWND, UINT, HWND, int );
int FASTCALL AddrbookWnd_OnKeyToItem( HWND, UINT, HWND, int );
void FASTCALL AddrbookWnd_OnAdjustWindows( HWND, int, int );
BOOL FASTCALL AddrbookWnd_OnEraseBkgnd( HWND, HDC );
HBRUSH FASTCALL AddrbookWnd_OnCtlColor( HWND, HDC, HWND, int );
void FASTCALL AddrbookWnd_OnSetFocus( HWND, HWND );

BOOL EXPORT CALLBACK PickEntry( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL PickEntry_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL PickEntry_OnCommand( HWND, int, HWND, UINT );
BOOL FASTCALL PickEntry_OnInitDialog( HWND, HWND, LPARAM );
//int FASTCALL PickEntry_OnCompareItem( HWND, const COMPAREITEMSTRUCT FAR * );
//int FASTCALL PickEntry_OnCharToItem( HWND, UINT, HWND, int );
void FASTCALL PickEntry_InsertItem( HWND, HWND, LPADDRBOOK );
BOOL FASTCALL PickEntry_DeleteString( HWND, int );

void FASTCALL Amaddr_LoadOldFormat( HWND );
BOOL FASTCALL LoadTree( HNDFILE, UINT, char * );
void FASTCALL Amaddr_FillToolTips( HWND );
static BOOL fChangedGroup = FALSE;
static int cCopies;                    /* Number of copies to print */
static BOOL fPrintGroups = FALSE;               /* TRUE if we print groups in full */

BOOL EXPORT CALLBACK PrintAddressDlg( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL PrintAddressDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL PrintAddressDlg_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL PrintAddressDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL CmdPrintAddressBook( HWND hwnd );


BOOL EXPORT WINAPI Amaddr_Ameol2Import( HWND, char * );

/* Old A2 format address book header.
 */
#pragma pack(1)
typedef struct tagADDRBOOKHEADER {
   FILEINFO finfo;
   char szTitle[ 40 ];
   char szPassword[ 40 ];
   char szOwner[ 40 ];
} ADDRBOOKHEADER, FAR * LPADDRBOOKHEADER;
#pragma pack()

/* Old A2 format address book
 */
#pragma pack(1)
typedef struct tagENTRY {
   WORD wSize;                         /* Size of data block */
   WORD offDisplayname;                /* Offset of display name in data block */
   WORD offAddress;                    /* Offset of address in data block */
   WORD offComment;                    /* Offset of comment in data block */
   WORD offPGPStr;                     /* Offset of PGP string in data block */
   WORD offPicFilename;                /* Offset of picture filename in data block */
} ENTRY;
#pragma pack()

#pragma pack(1)
typedef struct tagA2OLDBK_GROUP {
   char szGroupname[ 64 ];             /* Group name */
   WORD cEntries;                      /* Number of entries in group */
} A2OLDBK_GROUP;
#pragma pack()

#pragma pack(1)
typedef struct tagA2OLDBK_HEADER {
   WORD fGroup;                        /* TRUE for a group, FALSE for an entry */
   union {
      A2OLDBK_GROUP gr;                /* This is a group */
      ENTRY en;                        /* This is an entry */
   };
} A2OLDBK_HEADER;
#pragma pack()

BOOL EXPORT CALLBACK AddrBookEdit( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL AddrBookEdit_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL AddrBookEdit_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL AddrBookEdit_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT CALLBACK AddrBookGroup( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL AddrBookGroup_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL AddrBookGroup_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL AddrBookGroup_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT CALLBACK GroupAdd( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL GroupAdd_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL GroupAdd_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL GroupAdd_DlgProc( HWND, UINT, WPARAM, LPARAM );

void FASTCALL MailBook_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR *, BOOL );
void FASTCALL AddrbookWnd_OnDrawItemListbox( HWND, const DRAWITEMSTRUCT FAR * );
void FASTCALL AddrbookWnd_OnDrawItemHeader( HWND, const DRAWITEMSTRUCT FAR * );

LRESULT EXPORT CALLBACK AddrEditorWndProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT FAR PASCAL AddrBookListProc( HWND, UINT, WPARAM, LPARAM );

/* This function implements the address book picker.
 */
int FASTCALL Amaddr_Editor( void )
{
   if( !fRegistered )
      {
      WNDCLASS wc;
   
      /* Create and register the address book window class
       */
      wc.style          = CS_HREDRAW | CS_VREDRAW;
      wc.lpfnWndProc    = AddrEditorWndProc;
      wc.hIcon          = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_ADDRBOOK) );
      wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
      wc.hInstance      = hInst;
      wc.cbWndExtra     = DLGWINDOWEXTRA;
      wc.cbClsExtra     = 0;
      wc.hbrBackground  = NULL;
      wc.lpszClassName  = szAddrWndClass;
      wc.lpszMenuName   = NULL;
      if( !RegisterClass( &wc ) )
         return( FALSE );
      fRegistered = TRUE;
      }

   /* If the address picker window is active, bring it to
    * the foreground.
    */
   if( hwndAddrBook )
      Adm_MakeMDIWindowActive( hwndAddrBook );
   else
      {
      DWORD dwState;
      RECT rc;

      /* The default position of the address book.
       */
      GetClientRect( hwndMDIClient, &rc );
      InflateRect( &rc, -10, -10 );
      dwState = 0;

      /* Save window state.
       */
      ReadProperWindowState( szAddrWndName, &rc, &dwState );

      /* Create the window.
       */
      hwndAddrBook = Adm_CreateMDIWindow( szAddrWndName, szAddrWndClass, hInst, &rc, dwState, 0L );
      if( NULL == hwndAddrBook )
         return( FALSE );

      /* Set the initial focus.
       */
      AddrbookWnd_OnSetFocus( hwndAddrBook, NULL );
      }
   return( TRUE );
}

/* This function implements the window procedure for the
 * address book picker.
 */
LRESULT EXPORT CALLBACK AddrEditorWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, AddrbookWnd_OnCreate );
      HANDLE_MSG( hwnd, WM_SIZE, AddrbookWnd_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, AddrbookWnd_OnMove );
      HANDLE_MSG( hwnd, WM_CLOSE, AddrbookWnd_OnClose );
      HANDLE_MSG( hwnd, WM_NOTIFY, AddrbookWnd_OnNotify );
      HANDLE_MSG( hwnd, WM_COMMAND, AddrbookWnd_OnCommand );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, AddrbookWnd_MDIActivate );
      HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, AddrbookWnd_OnAdjustWindows );
      HANDLE_MSG( hwnd, WM_DRAWITEM, AddrbookWnd_OnDrawItem );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, AddrbookWnd_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, AddrbookWnd_OnEraseBkgnd );
   #ifdef WIN32
      HANDLE_MSG( hwnd, WM_CTLCOLORLISTBOX, AddrbookWnd_OnCtlColor );
   #else
      HANDLE_MSG( hwnd, WM_CTLCOLOR, AddrbookWnd_OnCtlColor );
   #endif
      HANDLE_MSG( hwnd, WM_COMPAREITEM, AddrbookWnd_OnCompareItem );
      HANDLE_MSG( hwnd, WM_CHARTOITEM, AddrbookWnd_OnCharToItem );
      HANDLE_MSG( hwnd, WM_SETFOCUS, AddrbookWnd_OnSetFocus );
      HANDLE_MSG( hwnd, WM_TIMER, AddrbookWnd_OnTimer );

      case WM_CHANGEFONT:
         if( wParam == FONTYP_ADDRBOOK || 0xFFFF == wParam )
            {
            LOGFONT lfAddrBookBold;
            MEASUREITEMSTRUCT mis;
            HWND hwndList;

            /* Delete the current fonts and rebuild them.
             */
            DeleteFont( hAddrBookFontN );
            DeleteFont( hAddrBookFontB );
            hAddrBookFontN = CreateFontIndirect( &lfAddrBook );
            lfAddrBookBold = lfAddrBook;
            lfAddrBookBold.lfWeight = FW_BOLD;
            hAddrBookFontB = CreateFontIndirect( &lfAddrBookBold );

            /* T'user has changed the scheduler window text font.
             */
            hwndList = GetDlgItem( hwnd, IDD_LIST );
            SendMessage( hwnd, WM_MEASUREITEM, IDD_LIST, (LPARAM)(LPSTR)&mis );
            SendMessage( hwndList, LB_SETITEMHEIGHT, 0, MAKELPARAM( mis.itemHeight, 0) );
            InvalidateRect( hwndList, NULL, TRUE );
            UpdateWindow( hwndList );
            }
         return( 0L );

      case WM_APPCOLOURCHANGE:
         if( wParam == WIN_ADDRBOOK )
            {
            HWND hwndList;

            /* The user has changed the scheduler window or text
             * colour from the Customise dialog.
             */
            if( NULL != hbrAddrBook )
               DeleteBrush( hbrAddrBook );
            hbrAddrBook = CreateSolidBrush( crAddrBookWnd );
            hwndList = GetDlgItem( hwnd, IDD_LIST );
            InvalidateRect( hwndList, NULL, TRUE );
            UpdateWindow( hwndList );
            }
         return( 0L );

      case WM_GETMINMAXINFO: {
         MINMAXINFO FAR * lpMinMaxInfo;
         LRESULT lResult;

         /* Set the minimum tracking size so the client window can never
          * be resized below 24 lines.
          */
         lResult = Adm_DefMDIDlgProc( hwnd, message, wParam, lParam );
         lpMinMaxInfo = (MINMAXINFO FAR *)lParam;
         lpMinMaxInfo->ptMinTrackSize.x = 280;
         lpMinMaxInfo->ptMinTrackSize.y = 310;
         return( lResult );
         }

      default:
         break;
      }
   return( Adm_DefMDIDlgProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_ERASEBKGND message.
 */
BOOL FASTCALL AddrbookWnd_OnEraseBkgnd( HWND hwnd, HDC hdc )
{
   if( fQuickUpdate )
      return( TRUE );
   return( Common_OnEraseBkgnd( hwnd, hdc ) );
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL AddrbookWnd_OnCreate( HWND hwnd, LPCREATESTRUCT lpCreateStruct )
{
   LPMDICREATESTRUCT lpMDICreateStruct;
   static int aArray[ 3 ] = { 120, 100, 100 };
   LPADDRBOOK lpAddrBook;
   LOGFONT lfAddrBookBold;
   HD_ITEM hdi;
   HWND hwndHdr;
   HWND hwndList;
   int index;
   int count;

   /* Load the bitmaps.
    */
   hbmpUser  = LoadBitmap( hRscLib, MAKEINTRESOURCE(IDB_ADDRUSER) );
   hbmpGroup = LoadBitmap( hRscLib, MAKEINTRESOURCE(IDB_ADDRGROUP) );

   /* Create the address book fonts.
    */
   hAddrBookFontN = CreateFontIndirect( &lfAddrBook );
   lfAddrBookBold = lfAddrBook;
   lfAddrBookBold.lfWeight = FW_BOLD;
   hAddrBookFontB = CreateFontIndirect( &lfAddrBookBold );

   /* Create the dialog window.
    */
   lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
   Adm_MDIDialog( hwnd, MAKEINTRESOURCE(IDDLG_ADDRVIEW), lpMDICreateStruct );

   /* Read the current header settings.
    */
   Amuser_GetPPArray( szAddrBook, "Columns", aArray, 3 );

   /* Fill in the header fields.
    */
   hwndHdr = GetDlgItem( hwnd, IDD_LIST );
   hdi.mask = HDI_TEXT|HDI_WIDTH|HDI_FORMAT;
   hdi.cxy = hdrColumns[ 0 ] = aArray[ 0 ];
   hdi.fmt = HDF_STRING|HDF_LEFT|HDF_OWNERDRAW;
   Header_InsertItem( hwndHdr, 0, &hdi );
   
   hdi.cxy = hdrColumns[ 1 ] = aArray[ 1 ];
   hdi.fmt = HDF_STRING|HDF_LEFT|HDF_OWNERDRAW;
   Header_InsertItem( hwndHdr, 1, &hdi );

   hdi.cxy = hdrColumns[ 2 ] = aArray[ 2 ];
   hdi.fmt = HDF_STRING|HDF_LEFT|HDF_OWNERDRAW;
   Header_InsertItem( hwndHdr, 2, &hdi );
   SendMessage( hwndHdr, WM_SETTEXT, 2 | HBT_SPRING, 0L );

   /* Create the scheduler window handle.
    */
   hbrAddrBook = CreateSolidBrush( crAddrBookWnd );

   /* Common code to fill the address book list.
    */
   if( !fAddrBookLoaded )
      Amaddr_Load();
   Amaddr_FillList( hwnd, TRUE, "" );
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   Amaddr_FillToolTips( hwndList );

   /* Set the limit of the search text
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_FINDTEXT ), LEN_FINDTEXT );

   /* Subclass the list control.
    */
   VERIFY( hwndList = GetDlgItem( hwndList, 0x3000 ) );
   lpProcListCtl = SubclassWindow( hwndList, AddrBookListProc );

   /* Enable controls depending on whether anything selected
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   index = ListBox_GetCaretIndex( hwndList );
   ASSERT( index != LB_ERR );
   lpAddrBook = (LPADDRBOOK)ListBox_GetItemData( hwndList, index );
   ASSERT( NULL != lpAddrBook );
   count = ListBox_GetSelCount( hwndList );
   EnableControl( hwnd, IDD_RESUME, count > 0 && !lpAddrBook->fGroup );
   EnableControl( hwnd, IDD_MAIL, count > 0 );
   EnableControl( hwnd, IDD_EDIT, count == 1 );
   EnableControl( hwnd, IDD_REMOVE, count > 0 );
   return( TRUE );
}

/* This function fills the address book list window.
 */
void FASTCALL Amaddr_FillList( HWND hwnd, BOOL fTopicList, char * pszMatch )
{
   LPADDRBOOK lpAddrBook;
   LPADDRBOOK lpAddrSel;
   HWND hwndList;
   int index;

   /* If an item is selected, save it.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   lpAddrSel = lpAddrBookFirst;
   if( ListBox_GetSelCount( hwndList ) > 0 )
      {
      index = ListBox_GetCaretIndex( hwndList );
      lpAddrSel = (LPADDRBOOK)ListBox_GetItemData( hwndList, index );
      }

   /* Now empty and refill the list.
    */
   SetWindowRedraw( hwndList, FALSE );
   ListBox_ResetContent( hwndList );
   for( lpAddrBook = lpAddrBookFirst; lpAddrBook; lpAddrBook = lpAddrBook->lpNext )
      {
      if( *pszMatch == '\0' )
         ListBox_AddString( hwndList, (LPSTR)lpAddrBook );
      else if( lpAddrBook->fGroup && stristr( ((LPADDRBOOKGROUP)lpAddrBook)->szGroupName, pszMatch ) != NULL )
         ListBox_AddString( hwndList, (LPSTR)lpAddrBook );
      else {
         if( !lpAddrBook->fGroup &&
             stristr( lpAddrBook->szCixName, pszMatch ) != NULL ||
             stristr( lpAddrBook->szFullName, pszMatch ) != NULL ||
             stristr( lpAddrBook->szComment, pszMatch ) != NULL )
            ListBox_AddString( hwndList, (LPSTR)lpAddrBook );
         }
      }
   SetWindowRedraw( hwndList, TRUE );
   if( fTopicList )
      {
      InvalidateRect( hwndList, NULL, FALSE );
      UpdateWindow( hwndList );
      }

   /* Select the previously selected item, or the first one
    * if nothing was previously selected.
    */
   if( ListBox_GetCount( hwndList ) > 0 )
      {
      index = RealListBox_FindItemData( hwndList, -1, (DWORD)lpAddrSel );
      if( LB_ERR == index )
         index = 0;
      ListBox_SetSel( hwndList, TRUE, index );
      }
}

/* This function fills the tooltip control with a list of all
 * strings in the listbox.
 */
void FASTCALL Amaddr_FillToolTips( HWND hwndList )
{
   int count;
   RECT rc;
   int top;
   int c;

   /* Start from first visible item in listbox.
    */
   count = ListBox_GetCount( hwndList );
   top = ListBox_GetTopIndex( hwndList );
   GetClientRect( hwndList, &rc );

   /* Delete any existing tooltip.
    */
   if( NULL != hwndTooltip )
      DestroyWindow( hwndTooltip );
   hwndTooltip = CreateWindow( TOOLTIPS_CLASS, "", 0, 0, 0, 0, 0, GetParent( hwndList ), 0, hRscLib, 0L );
   SendMessage( hwndTooltip, TTM_SETMAXTIPWIDTH, 0, 2048 );
   for( c = top; c < count; ++c )
      {
      LPADDRBOOK lpAddrBook;

      /* Get the width of this string.
       */
      lpAddrBook = (LPADDRBOOK)ListBox_GetItemData( hwndList, c );
      if( lpAddrBook->fGroup )
         {
         TOOLINFO ti;

         /* Create a tooltip for this item.
          */
         ti.cbSize = sizeof(TOOLINFO);
         ti.uFlags = 0;
         ti.lpszText = LPSTR_TEXTCALLBACK;
         ti.hwnd = GetParent( hwndList );
         ti.uId = c;
         ti.hinst = hRscLib;
         ListBox_GetItemRect( hwndList, c, &ti.rect );
         if( IsRectEmpty( &ti.rect ) )
            break;
         SendMessage( hwndTooltip, TTM_ADDTOOL, 0, (LPARAM)(LPSTR)&ti );
         }
      }
}

 /* 
  This function handles the non-char keys in the address picker !!SM!! 2.55
  */
int FASTCALL AddrbookWnd_OnKeyToItem( HWND hwnd, UINT ch, HWND hwndList, int iCaret )
{
   int count;
   int index;

   count = ListBox_GetCount( hwndList );

   if ( ch == VK_HOME )
   {
      fLastChar = 0;
      memset((char *)&fLastTyped,0,254);
      ListBox_SetCurSel(hwndList, 0);
   }
   else if ( ch == VK_UP )
   {
      fLastChar = 0;
      memset((char *)&fLastTyped,0,254);
      ListBox_SetCurSel(hwndList, iCaret > 0 ? (iCaret - 1) : 0);
   }
   else if ( ch == VK_DOWN )
   {
      fLastChar = 0;
      memset((char *)&fLastTyped,0,254);
      ListBox_SetCurSel(hwndList, iCaret < count ? (iCaret + 1) : count);
   }
   else if ( ch == VK_END )
   {
      fLastChar = 0;
      memset((char *)&fLastTyped,0,254);
      ListBox_SetCurSel(hwndList, count);
   }
   else if( isprint( ch ) || (ch == VK_BACK) )
   {
      int start, n=0;
      LPADDRBOOK lpAddrBook;

      index = iCaret;
      start = index;
      
      if ( ch == VK_BACK )
      {
         fLastTyped[--fLastChar] = '\x0';
      }
      else
         fLastTyped[fLastChar++] = ch;
      do
      {
         lpAddrBook = (LPADDRBOOK)ListBox_GetItemData( hwndList, n );
         if( _strnicmp( fLastTyped, lpAddrBook->szFullName, fLastChar ) == 0)
         {
            return n;
         }
         else
            n++;

      }
      while (n < count);

/*    do {
         LPADDRBOOK lpAddrBook;

         if( ++index == count )
            index = 0;

         lpAddrBook = (LPADDRBOOK)ListBox_GetItemData( hwndList, index );
         if( fLastChar <= strlen(lpAddrBook->szFullName) )
         {
            int n=0;
            do
            {
               if( tolower( (char)fLastTyped[n] ) == tolower( lpAddrBook->szFullName[ n ] ) )
               {
                  n++;
               }
               else
               {
                  break;
               }
            }
            while (n < fLastChar);
         }
         else
            return( index );
      }
      while( index != start );
      return( -2 );
      */
   }
   return( -1 );
}

/* This function selects the next item in the address book that
 * begins with the specified character.
 */
int FASTCALL AddrbookWnd_OnCharToItem( HWND hwnd, UINT ch, HWND hwndList, int iCaret )
{
   if( isprint( ch ) )
      {
      int count;
      int index;
      int start;

      count = ListBox_GetCount( hwndList );
      index = iCaret;
      start = index;
      do {
         LPADDRBOOK lpAddrBook;

         if( ++index == count )
            index = 0;
         lpAddrBook = (LPADDRBOOK)ListBox_GetItemData( hwndList, index );
         if( tolower( (char)ch ) == tolower( lpAddrBook->szFullName[ 0 ] ) )
            return( index );
         }
      while( index != start );
      return( -2 );
      }
   return( -1 );
}

/* This function handles the WM_COMPAREITEM message
 */
int FASTCALL AddrbookWnd_OnCompareItem( HWND hwnd, const COMPAREITEMSTRUCT FAR * lpCompareItem )
{
   LPADDRBOOK lpAddr1;
   LPADDRBOOK lpAddr2;
   int nSortDirection;

   lpAddr1 = (LPADDRBOOK)lpCompareItem->itemData1;
   lpAddr2 = (LPADDRBOOK)lpCompareItem->itemData2;
   nSortDirection = ( nSortColumn & 0x80 ) ? -1 : 1;
   switch( nSortColumn & 0x7F )
      {
      case 0:
         /* Sort by full name.
          */
         return( _strcmpi( lpAddr1->szFullName, lpAddr2->szFullName ) * nSortDirection );

      case 1:
         /* Sort by internet address.
          */
         return( _strcmpi( lpAddr1->szCixName, lpAddr2->szCixName ) * nSortDirection );

      case 2:
         /* Sort by comment.
          */
         return( _strcmpi( lpAddr1->szComment, lpAddr2->szComment ) * nSortDirection );
      }

   /* Should never get here!
    */
   ASSERT( FALSE );
   return( 0 );
}

/* This function processes the WM_COMMAND message.
 */
void FASTCALL AddrbookWnd_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDM_QUICKPRINT:
      case IDM_PRINT: {
         CmdPrintAddressBook( hwnd );
         break;
         }

      case IDD_FINDTEXT:
         if( codeNotify == EN_CHANGE )
            {
            KillTimer( hwnd, IDD_FINDTEXT );
            SetTimer( hwnd, IDD_FINDTEXT, 500, NULL );
            }
         break;

      case IDD_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsADDRVIEW );
         break;

      case IDD_EDIT: {
         char szFullName[ AMLEN_INTERNETNAME+1 ];
         HWND hwndList;
         int index;

         /* Edit the selected item. You can only ever edit one
          * item at a time.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         ASSERT( 1 == ListBox_GetSelCount( hwndList ) );
         index = ListBox_GetCaretIndex( hwndList );
         lpGlbAddrBook = (LPADDRBOOK)ListBox_GetItemData( hwndList, index );

         /* Different edit actions depending on whether we're editing a
          * group or an individual item.
          */
         if( lpGlbAddrBook->fGroup )
            {
            LPADDRBOOK lpAddrBookNext;

            lpAddrBookNext = lpGlbAddrBook->lpNext;
            lstrcpy( szFullName, ((LPADDRBOOKGROUP)lpGlbAddrBook)->szGroupName );
            if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE( IDDLG_ADDRBOOKGROUP ), AddrBookGroup, (LPARAM)lpGlbAddrBook ) )
               if( lpGlbAddrBook )
                  {
                  ListBox_DeleteString( hwndList, index );
                  index = ListBox_AddString( hwndList, lpGlbAddrBook );
                  ListBox_SetSel( hwndList, TRUE, index );
                  }
               else
                  {
                  SetWindowRedraw( hwndList, FALSE );
                  Amaddr_FillList( hwnd, TRUE, "" );
                  Amaddr_FillToolTips( hwndList );
                  SetWindowRedraw( hwndList, TRUE );
                  }
            }
         else
            {
            /* We're editing an item. One thing to watch out for is when the
             * user changes the data, its position in the sorted list could
             * be changed!
             */
            lstrcpy( szFullName, lpGlbAddrBook->szFullName );
            if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE( IDDLG_ADDRBOOKEDIT ), AddrBookEdit, (LPARAM)lpGlbAddrBook ) )
               {
               ListBox_DeleteString( hwndList, index );
               index = ListBox_AddString( hwndList, lpGlbAddrBook );
               ListBox_SetSel( hwndList, TRUE, index );
               }
            }
         //pw 
         Amaddr_CommitChanges();
         SetFocus( hwndList );
         break;
         }

      case IDD_NEWGROUP: {
         HWND hwndList;

         /* Add a new group to the address book
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_ADDRBOOKGROUP), AddrBookGroup, 0L ) )
            {
            int index;

            index = ListBox_AddString( hwndList, lpGlbAddrBook );
            ListBox_SetSel( hwndList, FALSE, -1 );
            ListBox_SetSel( hwndList, TRUE, index );
            EnableControl( hwnd, IDD_RESUME, FALSE );
            EnableControl( hwnd, IDD_MAIL, TRUE );
            EnableControl( hwnd, IDD_EDIT, TRUE );
            EnableControl( hwnd, IDD_REMOVE, TRUE );
            Amaddr_FillToolTips( hwndList );
            }
         SetFocus( hwndList );
   //pw
         Amaddr_CommitChanges();
         break;
         }

      case IDD_REMOVE: {
         LPADDRBOOK lpAddrBook;
         HWND hwndList;
         int index;
         int count;
         int start;
         int retCode;
         WORD wType;

         /* Delete the selected entries.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         count = ListBox_GetCount( hwndList );
         start = -1;
         retCode = IDNO;
         wType = (WORD)( ListBox_GetSelCount( hwndList ) > 1 ? MB_YESNOALLCANCEL : MB_YESNO );
         for( index = 0; index < count; ++index )
            if( ListBox_GetSel( hwndList, index ) )
               {
               if( -1 == start )
                  start = index;
               lpAddrBook = (LPADDRBOOK)ListBox_GetItemData( hwndList, index );
               wsprintf( lpTmpBuf, GS(IDS_STR95), lpAddrBook->szFullName );
               if( IDALL != retCode )
                  retCode = fMessageBox( hwnd, 0, lpTmpBuf, wType|MB_ICONQUESTION );
               if( IDCANCEL == retCode )
                  break;
               if( IDYES == retCode || IDALL == retCode )
                  {
                  Amaddr_DeleteEntry( lpAddrBook );
                  ListBox_DeleteString( hwndList, index );
                  --count;
                  --index;
                  }
               }

         /* All done. If we have anything left, select something if all
          * selected items are deleted. Otherwise disable the buttons that
          * depend on a selection.
          */
         if( 0 != count )
            {
            if( 0 == ListBox_GetSelCount( hwndList ) )
               {
               if( start == count )
                  --start;
               ListBox_SetSel( hwndList, TRUE, start );
               }
            }
         else
            {
            EnableControl( hwnd, IDD_MAIL, FALSE );
            EnableControl( hwnd, IDD_RESUME, FALSE );
            EnableControl( hwnd, IDD_REMOVE, FALSE );
            }
         Amaddr_FillToolTips( hwndList );
//pw
         Amaddr_CommitChanges();
         SetFocus( hwndList );
         break;
         }

      case IDD_ADD: {
         HWND hwndList;

         /* Add a new entry to the address book
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_ADDRBOOKEDIT), AddrBookEdit, 0L ) )
            {
            int index;

            index = ListBox_AddString( hwndList, lpGlbAddrBook );
            ListBox_SetSel( hwndList, FALSE, -1 );
            ListBox_SetSel( hwndList, TRUE, index );
            EnableControl( hwnd, IDD_RESUME, TRUE );
            EnableControl( hwnd, IDD_MAIL, TRUE );
            EnableControl( hwnd, IDD_EDIT, TRUE );
            EnableControl( hwnd, IDD_REMOVE, TRUE );
            Amaddr_FillToolTips( hwndList );
            }
         SetFocus( hwndList );
         //pw
         Amaddr_CommitChanges();
         break;
         }

      case IDD_RESUME: {
         LPADDRBOOK lpAddrBook;
         HWND hwndList;
         int index;
         int count;

         /* Display the resume for the selected users
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         count = ListBox_GetCount( hwndList );
         for( index = 0; index < count; ++index )
            if( ListBox_GetSel( hwndList, index ) )
               {
               lpAddrBook = (LPADDRBOOK)ListBox_GetItemData( hwndList, index );
               ASSERT( NULL != lpAddrBook );
               if( !lpAddrBook->fGroup )
                  CmdShowResume( hwnd, lpAddrBook->szCixName, TRUE );
               }
         SetFocus( hwndList );
         break;
         }

      case IDD_LIST:
         if( codeNotify == LBN_SELCHANGE )
            {
            LPADDRBOOK lpAddrBook;
            int count;
            int index;

            /* First, the usual stuff.
             */
            index = ListBox_GetCaretIndex( hwndCtl );
            ASSERT( index != LB_ERR );
            lpAddrBook = (LPADDRBOOK)ListBox_GetItemData( hwndCtl, index );
            ASSERT( NULL != lpAddrBook );
            count = ListBox_GetSelCount( hwndCtl );
            EnableControl( hwnd, IDD_RESUME, count > 0 && !lpAddrBook->fGroup );
            EnableControl( hwnd, IDD_MAIL, count > 0 );
            EnableControl( hwnd, IDD_EDIT, count == 1 );
            EnableControl( hwnd, IDD_REMOVE, count > 0 );
            break;
            }
         if( codeNotify != LBN_DBLCLK )
            break;

      case IDD_MAIL: {
         LPADDRBOOK lpAddrBook;
         HWND hwndList;
         HPSTR lpToList;
         MAILOBJECT mo;
         DWORD cbToList;
         int index;
         int count;

         /* Create a mail object using the selected names as
          * the recipients.
          */
         Amob_New( OBTYPE_MAILMESSAGE, &mo );

         /* Set the Reply-To address.
          */
         if( fUseInternet )
            Amob_CreateRefString( &mo.recReplyTo, szMailReplyAddress );

         /* Set the mailbox.
          */
         if( NULL != GetPostmasterFolder() )
            {
            char sz[ 128 ];

            WriteFolderPathname( sz, ptlPostmaster );
            Amob_CreateRefString( &mo.recMailbox, sz );
            }

         /* Loop and build a string of recipient names. First
          * scan counts the size of each name.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         count = ListBox_GetCount( hwndList );
         cbToList = 1;
         for( index = 0; index < count; ++index )
            if( ListBox_GetSel( hwndList, index ) )
               {
               lpAddrBook = (LPADDRBOOK)ListBox_GetItemData( hwndList, index );
               ASSERT( NULL != lpAddrBook );
               if( !lpAddrBook->fGroup )
                  cbToList += strlen( lpAddrBook->szCixName ) + 1;
               else
                  {
                  /* Add 2 for quotes.
                   */
                  if( NULL != strchr( ((LPADDRBOOKGROUP)lpAddrBook)->szGroupName, ' ' ) )
                     cbToList += 2;
                  cbToList += strlen( ((LPADDRBOOKGROUP)lpAddrBook)->szGroupName ) + 1;
                  }
               }

         INITIALISE_PTR(lpToList);

         /* Allocate a buffer for each name.
          */
         if( !fNewMemory32( &lpToList, cbToList ) )
            OutOfMemoryError( hwnd, FALSE, FALSE );
         else
            {
            int cItems;

            /* Second scan fills the buffer
             */
            *lpToList = '\0';
            cItems = 0;
            for( index = 0; index < count; ++index )
               if( ListBox_GetSel( hwndList, index ) )
                  {
                  lpAddrBook = (LPADDRBOOK)ListBox_GetItemData( hwndList, index );
                  ASSERT( NULL != lpAddrBook );
                  if( cItems )
                     hstrcat( lpToList, " " );
                  if( !lpAddrBook->fGroup )
                     hstrcat( lpToList, lpAddrBook->szCixName );
                  else
                     {
                     BOOL fSpaces;

                     /* Wrap group names with spaces in quotes.
                      */
                     fSpaces = NULL != strchr( ((LPADDRBOOKGROUP)lpAddrBook)->szGroupName, ' ' );
                     if( fSpaces )
                        hstrcat( lpToList, "\"" );
                     hstrcat( lpToList, ((LPADDRBOOKGROUP)lpAddrBook)->szGroupName );
                     if( fSpaces )
                        hstrcat( lpToList, "\"" );
                     }
                  ++cItems;
                  }
            Amob_CreateRefString( &mo.recTo, lpToList );
            CreateMailWindow( hwndFrame, &mo, NULL );
            FreeMemory32( &lpToList );
            }
         Amob_Delete( OBTYPE_MAILMESSAGE, &mo );
         break;
         }

      case IDOK:
      case IDCANCEL:
//       if( GetFocus() != GetDlgItem( hwnd, IDD_FINDTEXT ) )
            {
            Amaddr_CommitChanges();
            AddrbookWnd_OnClose( hwnd );
            }
         break;
      }
}

/* This function handles the WM_TIMER message.
 */
void FASTCALL AddrbookWnd_OnTimer( HWND hwnd, UINT id )
{
   if( id == IDD_FINDTEXT )
   {
      char szFindText[ LEN_FINDTEXT ];
      HWND hwndList;
      LPADDRBOOK lpAddrBook;
      int index;
      int count;

      /* Get the new find text
       */
      KillTimer( hwnd, id );
      Edit_GetText( GetDlgItem( hwnd, IDD_FINDTEXT ), szFindText, sizeof(szFindText) );

      /* Build the new list.
       */
      Amaddr_FillList( hwnd, TRUE, szFindText );
      VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
      Amaddr_FillToolTips( hwndList );

      /* Enable controls depending on whether anything selected
       */
      index = ListBox_GetCaretIndex( hwndList );
      ASSERT( index != LB_ERR );
      lpAddrBook = (LPADDRBOOK)ListBox_GetItemData( hwndList, index );
      ASSERT( NULL != lpAddrBook );
      count = ListBox_GetSelCount( hwndList );
      EnableControl( hwnd, IDD_RESUME, count > 0 && !lpAddrBook->fGroup );
      EnableControl( hwnd, IDD_MAIL, count > 0 );
      EnableControl( hwnd, IDD_EDIT, count == 1 );
      EnableControl( hwnd, IDD_REMOVE, count > 0 );

   }
}

/* This function handles the WM_SETFOCUS message.
 */
void FASTCALL AddrbookWnd_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   HWND hwndList;
   BOOL fEnabled;

   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   fEnabled = ListBox_GetCount( hwndList ) > 0;
   SetFocus( GetDlgItem( hwnd, fEnabled ? IDD_LIST : IDOK ) );
}

/* This function handles the WM_MEASUREITEM message
 */
void FASTCALL AddrbookWnd_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   HFONT hOldFont;
   HDC hdc;

   hdc = GetDC( hwnd );
   hOldFont = SelectFont( hdc, hAddrBookFontB );
   GetTextMetrics( hdc, &tm );
   lpMeasureItem->itemHeight = tm.tmHeight + tm.tmExternalLeading + 2;
   SelectFont( hdc, hOldFont );
   ReleaseDC( hwnd, hdc );
}

/* This function handles the WM_DRAWITEM message
 */
void FASTCALL AddrbookWnd_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   switch( lpDrawItem->CtlID )
      {
      case IDD_LIST:
         switch( lpDrawItem->CtlType )
            {
            case ODT_HEADER:
               AddrbookWnd_OnDrawItemHeader( hwnd, lpDrawItem );
               break;

            case ODT_LISTBOX:
               AddrbookWnd_OnDrawItemListbox( hwnd, lpDrawItem );
               break;
            }
      }
}

/* This function handles the WM_DRAWITEM message for
 * the listbox.
 */
void FASTCALL AddrbookWnd_OnDrawItemListbox( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   if( lpDrawItem->itemID != -1 )
      {
      LPADDRBOOK lpAddrBook;
      HFONT hOldFont;
      HWND hwndList;
      COLORREF tmpT;
      COLORREF tmpB;
      COLORREF T;
      COLORREF B;
      HBRUSH hbr;
      RECT rc;
      TEXTMETRIC tm;

      /* Get the item we're drawing.
       */
      hwndList = GetDlgItem( hwnd, IDD_LIST );
      lpAddrBook = (LPADDRBOOK)ListBox_GetItemData( hwndList, lpDrawItem->itemID );
      if( NULL == lpAddrBook )
         return;

      /* Set up the painting tools.
       */
      rc = lpDrawItem->rcItem;
      if( lpDrawItem->itemState & ODS_SELECTED )
         {
         T = GetSysColor( COLOR_HIGHLIGHTTEXT );
         B = GetSysColor( COLOR_HIGHLIGHT );
         }
      else
         {
         T = crAddrBookText;
         B = crAddrBookWnd;
         }
      hbr = CreateSolidBrush( B );
      tmpT = SetTextColor( lpDrawItem->hDC, T );
      tmpB = SetBkColor( lpDrawItem->hDC, B );

      /* Show the type icon.
       */

      GetTextMetrics(lpDrawItem->hDC, &tm);

      /* If this is a user name, then fill in all three fields.
       */
      if( !lpAddrBook->fGroup )
      {
         Amuser_DrawBitmap( lpDrawItem->hDC, lpDrawItem->rcItem.left, lpDrawItem->rcItem.top, 16, tm.tmHeight - 1, hbmpUser, 0 );

         hOldFont = SelectFont( lpDrawItem->hDC, hAddrBookFontN );
         rc.left = 16;
         rc.right = hdrColumns[ 0 ] + 1;
         ExtTextOut( lpDrawItem->hDC, rc.left + 2, rc.top, ETO_OPAQUE|ETO_CLIPPED, &rc, lpAddrBook->szFullName, strlen(lpAddrBook->szFullName), NULL );
         rc.left = rc.right;
         rc.right = rc.left + hdrColumns[ 1 ] + 1;
         ExtTextOut( lpDrawItem->hDC, rc.left + 2, rc.top, ETO_OPAQUE|ETO_CLIPPED, &rc, lpAddrBook->szCixName, strlen(lpAddrBook->szCixName), NULL );
         rc.left = rc.right;
         rc.right = lpDrawItem->rcItem.right;
         ExtTextOut( lpDrawItem->hDC, rc.left + 2, rc.top, ETO_OPAQUE|ETO_CLIPPED, &rc, lpAddrBook->szComment, strlen(lpAddrBook->szComment), NULL );
      }
      else
      {
         int length;

         Amuser_DrawBitmap( lpDrawItem->hDC, lpDrawItem->rcItem.left, lpDrawItem->rcItem.top, 16, tm.tmHeight - 1, hbmpGroup, 0 );

         /* Show the group name in boldface.
          */
         hOldFont = SelectFont( lpDrawItem->hDC, hAddrBookFontB );
         rc.left = 16;
         length = strlen( ((LPADDRBOOKGROUP)lpAddrBook)->szGroupName );
         ExtTextOut( lpDrawItem->hDC, rc.left + 2, rc.top, ETO_OPAQUE|ETO_CLIPPED, &rc, ((LPADDRBOOKGROUP)lpAddrBook)->szGroupName, length, NULL );
      }

      /* Clean up.
       */
      SelectFont( lpDrawItem->hDC, hOldFont );
      SetTextColor( lpDrawItem->hDC, tmpT );
      SetBkColor( lpDrawItem->hDC, tmpB );
      DeleteBrush( hbr );
      }
}

/* This function handles the WM_DRAWITEM message for
 * the listbox.
 */
void FASTCALL AddrbookWnd_OnDrawItemHeader( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   char * pszText;
   RECT rc;
   int mode;

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
   if( lpDrawItem->itemID == (nSortColumn & 0x7F) )
      {
      HBITMAP hbmp;
      SIZE size;

      GetTextExtentPoint( lpDrawItem->hDC, pszText, strlen(pszText), &size );
      rc.left += size.cx + 3;
      rc.top += ( ( rc.bottom - rc.top ) - 10 ) / 2;
      if( nSortColumn & 0x80 )
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
}

/* This function processes the WM_NOTIFY message.
 */
LRESULT FASTCALL AddrbookWnd_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
{
   switch( lpNmHdr->code )
      {
      case TTN_NEEDTEXT: {
         TOOLTIPTEXT FAR * lpttt;
         LPADDRBOOKGROUP lpGrpAddrBook;
         HWND hwndList;
/*!!SM!! 2.56.?? BUG not displaying text here?? FS#118*/
         lpttt = (TOOLTIPTEXT FAR *)lpNmHdr;
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         lpGrpAddrBook = (LPADDRBOOKGROUP)ListBox_GetItemData( hwndList, lpttt->hdr.idFrom );
         if( NULL != lpGrpAddrBook )
            if( !lpGrpAddrBook->fGroup )
               return( FALSE );
            else
               {
               UINT cbSize;
               WORD c;

               /* First work out size of tip.
                */
               cbSize = strlen( lpGrpAddrBook->szGroupName ) + 1;
               for( c = 0; c < lpGrpAddrBook->cGroup; ++c )
                  {
                  cbSize += strlen( lpGrpAddrBook->lpAddrBook[ c ]->szCixName ) + 4;
                  cbSize += strlen( lpGrpAddrBook->lpAddrBook[ c ]->szFullName ) + 2;
                  }

               /* Free up any previous group list buffer
                */
               if( NULL != lpGrpList )
                  FreeMemory( &lpGrpList );

               /* Allocate a buffer for the list of group
                * names and fill it.
                */
               if( fNewMemory( &lpGrpList, cbSize + 1 ) )
                  {
                  strcpy( lpGrpList, lpGrpAddrBook->szGroupName );
                  strcat( lpGrpList, "\n" );
                  for( c = 0; c < lpGrpAddrBook->cGroup; ++c )
                     {
                     strcat( lpGrpList, "  " );
                     strcat( lpGrpList, lpGrpAddrBook->lpAddrBook[ c ]->szFullName );
                     strcat( lpGrpList, " (" );
                     strcat( lpGrpList, lpGrpAddrBook->lpAddrBook[ c ]->szCixName );
                     strcat( lpGrpList, ")" );
                     if( c < lpGrpAddrBook->cGroup - 1 )
                        strcat( lpGrpList, "\n" );
                     }
                  lpttt->lpszText = lpGrpList;
                  }
               }
         lpttt->hinst = NULL;
         return( TRUE );
         }

      case HDN_ITEMCLICK: {
         HD_NOTIFY FAR * lphdn;
         HWND hwndList;
         HWND hwndHdr;

         /* Resort the address book by the specified
          * column.
          */
         lphdn = (HD_NOTIFY FAR *)lpNmHdr;
         if( nSortColumn == (UINT)lphdn->iItem )
            nSortColumn ^= 0x80;
         else
            nSortColumn = lphdn->iItem;

         /* Kludge to redraw header.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         VERIFY( hwndHdr = GetDlgItem( hwndList, 0x2000 ) );
         InvalidateRect( hwndHdr, NULL, TRUE );
         UpdateWindow( hwndHdr );

         /* Now refill the list box.
          */
         Amaddr_FillList( hwnd, TRUE, "" );
         Amaddr_FillToolTips( hwndList );
         break;
         }

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
            Amuser_WritePPArray( szAddrBook, "Columns", hdrColumns, 3 );

         /* Update the listbox
          */
         fQuickUpdate = TRUE;
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         hwndList = GetDlgItem( hwndList, 0x3000 );
         InvalidateRect( hwndList, NULL, FALSE );
         UpdateWindow( hwndList );
         fQuickUpdate = FALSE;
         return( TRUE );
         }
      }
   return( 0L );
}

/* This function handles the WM_CLOSE message.
 */
void FASTCALL AddrbookWnd_OnClose( HWND hwnd )
{
   /* Clean up before we exit.
    */
   hwndAddrBook = NULL;
   if( NULL != lpGrpList )
      FreeMemory( &lpGrpList );
   DeleteFont( hAddrBookFontN );
   DeleteFont( hAddrBookFontB );
   DeleteBitmap( hbmpUser );
   DeleteBitmap( hbmpGroup );

   Adm_DestroyMDIWindow( hwnd );
}

/* This function intercepts messages for the scheduler list box window
 * so that we can handle the right mouse button.
 */
LRESULT EXPORT FAR PASCAL AddrBookListProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
   #ifdef WIN32
      case WM_MOUSEWHEEL:
   #endif
      case WM_VSCROLL:
         CallWindowProc( lpProcListCtl, hwnd, message, wParam, lParam );
         Amaddr_FillToolTips( hwnd );
         return( 0L );

      case WM_MOUSEMOVE:
      case WM_LBUTTONDOWN:
      case WM_LBUTTONUP: {
         MSG msg;

         msg.lParam = lParam;
         msg.wParam = wParam;
         msg.message = (int)message;
         msg.hwnd = hwnd;
         SendMessage( hwndTooltip, TTM_RELAYEVENT, 0, (LPARAM)(LPSTR)&msg );
         break;
         }

      case WM_ERASEBKGND:
         if( !fQuickUpdate )
            {
            RECT rc;

            GetClientRect( hwnd, &rc );
            FillRect( (HDC)wParam, &rc, hbrAddrBook );
            }
         return( TRUE );

      case WM_CONTEXTMENU: /*!!SM!!*/
      case WM_RBUTTONDOWN:
         {
         POINT pt;
         HMENU hPopupMenu;
         int count;
         int index;

         /* Make sure this window is active and select the item
          * at the cursor.
          */
         if( hwndActive != GetParent( hwnd ) )
            SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndAddrBook, 0L );

         /* Select the item under the cursor only if nothing is
          * currently selected.
          */
         index = ItemFromPoint( hwnd, (short)LOWORD(lParam), (short)HIWORD(lParam) );
         if( ListBox_GetSel( hwnd, index ) == 0 )
            {
            CallWindowProc( lpProcListCtl, hwnd, WM_LBUTTONDOWN, wParam, lParam );
            CallWindowProc( lpProcListCtl, hwnd, WM_LBUTTONUP, wParam, lParam );
            }

         /* Get the popup menu.
          */
         hPopupMenu = GetSubMenu( hPopupMenus, 20 );

         /* If we have any selected items, use the attributes
          * of the first item to determine the menu items.
          */
         count = ListBox_GetSelCount( hwnd );
         MenuEnable( hPopupMenu, IDD_REMOVE, count > 0 );
         MenuEnable( hPopupMenu, IDD_EDIT, count == 1 );

         /* Pop up!
          */
         GetCursorPos( &pt );
         TrackPopupMenu( hPopupMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndFrame, NULL );
         return( 0 );
         }

      case WM_KEYDOWN:
         /* Trap the Delete and key in the list box to
          * handle the Remove command.
          */
         if( wParam == VK_DELETE ) 
            {
            PostDlgCommand( hwndAddrBook, IDD_REMOVE, 0 );
            return( 0 );
            }

         /* Trap the Insert key in the list box to
          * add a entry.
          */
         if( wParam == VK_INSERT )
            {
            PostDlgCommand( hwndAddrBook, IDD_ADD, 1 );
            return( 0 );
            }
         break;
      }
   return( CallWindowProc( lpProcListCtl, hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL AddrbookWnd_OnAdjustWindows( HWND hwnd, int dx, int dy )
{
   Adm_MoveWnd( GetDlgItem( hwnd, IDOK ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDCANCEL ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_HELP ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_MAIL ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_RESUME ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_REMOVE ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_ADD ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_EDIT ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_NEWGROUP ), dx, 0 );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_LIST ), dx, dy );
}

/* This function handles the WM_CTLCOLOR message.
 */
HBRUSH FASTCALL AddrbookWnd_OnCtlColor( HWND hwnd, HDC hdc, HWND hwndChild, int type )
{
   switch( GetDlgCtrlID( hwndChild ) )
      {
      case IDD_LIST:
         SetBkColor( hdc, crAddrBookWnd );
         SetTextColor( hdc, crAddrBookText );
         return( hbrAddrBook );
      }
#ifdef WIN32
   return( FORWARD_WM_CTLCOLORLISTBOX( hwnd, hdc, hwndChild, Adm_DefMDIDlgProc ) );
#else
   return( FORWARD_WM_CTLCOLOR( hwnd, hdc, hwndChild, type, Adm_DefMDIDlgProc ) );
#endif
}

/* This function handles the WM_MDIACTIVATE message.
 * We replace the current menu bar with the newsgroups window
 * menu.
 */
void FASTCALL AddrbookWnd_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_ADDRBOOK ), (LPARAM)(LPSTR)hwnd );
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, Adm_DefMDIDlgProc );
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL AddrbookWnd_OnMove( HWND hwnd, int x, int y )
{
   /* Save window state.
    */
   Amuser_WriteWindowState( szAddrWndName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL AddrbookWnd_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   /* Save window state.
    */
   Amuser_WriteWindowState( szAddrWndName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* This function adds a new entry to the address book.
 */
BOOL WINAPI EXPORT Amaddr_AddEntry( HWND hwnd, LPSTR lpszCixName, LPSTR lpszFullName )
{
   LPADDRBOOK lpAddrBook;
   ADDRBOOK addrbook;

   /* First determine whether or not the address is already in
    * the address book.
    */
   if( !fAddrBookLoaded )
      Amaddr_Load();
   lpAddrBook = lpAddrBookFirst;
   while( lpAddrBook )
      {
      if( lstrcmpi( lpAddrBook->szCixName, lpszCixName ) == 0 )
         {
         wsprintf( lpTmpBuf, GS(IDS_STR14), lpszCixName );
         if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION ) == IDNO )
            return( FALSE );
         if( *lpszFullName )
            lstrcpy( lpAddrBook->szFullName, lpszFullName );
         Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_ADDRBOOKEDIT), AddrBookEdit, (LPARAM)lpAddrBook );
         return( TRUE );
         }
      lpAddrBook = lpAddrBook->lpNext;
      }
   lstrcpy( addrbook.szFullName, lpszFullName );
   lstrcpy( addrbook.szCixName, lpszCixName );
   addrbook.szComment[ 0 ] = '\0';
   if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_ADDRBOOKEDIT), AddrBookEdit, (LPARAM)(LPSTR)&addrbook ) )
      {
      /* Create the new entry. If the address book window is
       * open, add the entry to the window.
       */
      Amaddr_CreateEntry( addrbook.szCixName, addrbook.szFullName, addrbook.szComment );
      if( hwndAddrBook )
         {
         HWND hwndList;

         VERIFY( hwndList = GetDlgItem( hwndAddrBook, IDD_LIST ) );
         ListBox_AddString( hwndList, lpGlbAddrBook );
         }
         
      }
   //pw 
   Amaddr_CommitChanges();
   return( TRUE );
}

/* This function updates the specified entry.
 */
BOOL WINAPI EXPORT Amaddr_SetEntry( LPSTR lpszFullName, ADDRINFO FAR * lpAddrInfo )
{
   LPADDRBOOK lpAdr1;

   lpAdr1 = lpAddrBookFirst;
   if( lpAddrInfo )
      lpszFullName = lpAddrInfo->szFullName;
   while( lpAdr1 )
      {
      if( lstrcmpi( lpszFullName, lpAdr1->szFullName ) == 0 ) {
         if( !lpAddrInfo )
            Amaddr_DeleteEntry( lpAdr1 );
         else
            {
            lstrcpy( lpAdr1->szCixName, lpAddrInfo->szCixName );
            lstrcpy( lpAdr1->szComment, lpAddrInfo->szComment );
            lpGlbAddrBook = lpAdr1;
            }
         //pw
         Amaddr_CommitChanges();
         return( TRUE );
         }
      lpAdr1 = lpAdr1->lpNext;
      }
   if( lpAddrInfo )
   {
      Amaddr_CreateEntry( lpAddrInfo->szCixName, lpszFullName, lpAddrInfo->szComment );
      //pw
      Amaddr_CommitChanges();
   }
   return( FALSE );
}

/* This function updates the details for the specified entry.
 */
BOOL EXPORT FAR PASCAL Amaddr_GetEntry( LPSTR lpszCixName, ADDRINFO FAR * lpAddrInfo )
{
   LPADDRBOOK lpAdr1;

   if( !fAddrBookLoaded )
      Amaddr_Load();
   lpAdr1 = lpAddrBookFirst;
   while( lpAdr1 )
      {
      if( !lpAdr1->fGroup )
         if( lstrcmpi( lpszCixName, lpAdr1->szCixName ) == 0 )
            {
            lstrcpy( lpAddrInfo->szCixName, lpAdr1->szCixName );
            lstrcpy( lpAddrInfo->szFullName, lpAdr1->szFullName );
            lstrcpy( lpAddrInfo->szComment, lpAdr1->szComment );
            //pw
            Amaddr_CommitChanges();
            return( TRUE );
            }
      lpAdr1 = lpAdr1->lpNext;
      //pw
      Amaddr_CommitChanges();
      }
   return( FALSE );
}

/* This function creates a new address book entry using the specified data.
 */
BOOL FASTCALL Amaddr_CreateEntry( LPSTR lpszCixName, LPSTR lpszFullName, LPSTR lpszComment )
{
   if( !fAddrBookLoaded )
      Amaddr_Load();
   INITIALISE_PTR(lpGlbAddrBook);
   if( fNewMemory( &lpGlbAddrBook, sizeof( ADDRBOOK ) ) )
      {
      LPADDRBOOK lpAddr;
      LPADDRBOOK lpAddr2;

      lpAddr2 = NULL;
      for( lpAddr = lpAddrBookFirst; lpAddr; lpAddr = lpAddr->lpNext )
         {
         if( lpAddr->fGroup )
            {
            if( lstrcmpi( ((LPADDRBOOKGROUP)lpAddr)->szGroupName, lpszFullName ) > 0 )
               break;
            }
         else if( *lpszFullName )
            {
            if( lstrcmpi( lpAddr->szFullName, lpszFullName ) > 0 )
               break;
            }
         else if( *lpszCixName )
            {
            if( lstrcmpi( lpAddr->szCixName, lpszCixName ) > 0 )
               break;
            }
         lpAddr2 = lpAddr;
         }
      if( !lpAddr2 )
         lpAddrBookFirst = lpGlbAddrBook;
      else
         lpAddr2->lpNext = lpGlbAddrBook;
      lpGlbAddrBook->lpNext = lpAddr;
      lpGlbAddrBook->fGroup = FALSE;
      strncpy( lpGlbAddrBook->szCixName, lpszCixName, AMLEN_INTERNETNAME+1 );
      strncpy( lpGlbAddrBook->szFullName, lpszFullName, AMLEN_FULLCIXNAME+1 );
      strncpy( lpGlbAddrBook->szComment, lpszComment, AMLEN_COMMENT+1 );
      lpGlbAddrBook->szCixName[ AMLEN_INTERNETNAME ] = '\0';
      lpGlbAddrBook->szFullName[ AMLEN_FULLCIXNAME ] = '\0';
      lpGlbAddrBook->szComment[ AMLEN_COMMENT ] = '\0';
      //pw
      Amaddr_CommitChanges();
      return( TRUE );
      }
   return( FALSE );
}

/* This function expands and returns a list of group names
 * to the specified array.
 */
int EXPORT WINAPI Amaddr_GetGroup( LPSTR lpszGroupName, char FAR * FAR lpGroupList[] )
{
   LPADDRBOOKGROUP lpAdr1;

   if( !fAddrBookLoaded )
      Amaddr_Load();
   lpAdr1 = (LPADDRBOOKGROUP)lpAddrBookFirst;
   while( lpAdr1 )
      {
      if( lpAdr1->fGroup )
         if( lstrcmpi( lpszGroupName, lpAdr1->szGroupName ) == 0 )
            {
            if( lpGroupList )
               {
               register WORD c;

               for( c = 0; c < lpAdr1->cGroup; ++c )
                  lstrcpy( lpGroupList[ c ], lpAdr1->lpAddrBook[ c ]->szCixName );
               }
            return( lpAdr1->cGroup );
            }
      lpAdr1 = lpAdr1->lpNext;
      }
   return( 0 );
}

/* This function returns whether or not the specified name
 * is an address book group.
 */
BOOL EXPORT WINAPI Amaddr_IsGroup( LPSTR lpszCixName )
{
   LPADDRBOOKGROUP lpAdr1;

   if( !fAddrBookLoaded )
      Amaddr_Load();
   lpAdr1 = (LPADDRBOOKGROUP)lpAddrBookFirst;
   while( lpAdr1 )
      {
      if( lpAdr1->fGroup )
         if( lstrcmpi( lpszCixName, lpAdr1->szGroupName ) == 0 )
            return( TRUE );
      lpAdr1 = lpAdr1->lpNext;
      }
   return( FALSE );
}

/* This function expands a group into its component addresses.
 */
UINT EXPORT WINAPI Amaddr_ExpandGroup( LPSTR lpszCixName, int index, LPSTR lpOutBuf )
{
   LPADDRBOOKGROUP lpAdr1;

   if( !fAddrBookLoaded )
      Amaddr_Load();
   lpAdr1 = (LPADDRBOOKGROUP)lpAddrBookFirst;
   while( lpAdr1 )
      {
      if( lpAdr1->fGroup )
         if( lstrcmpi( lpszCixName, lpAdr1->szGroupName ) == 0 )
            {
            if( index >= (int)lpAdr1->cGroup )
               return( FALSE );
            lstrcpy( lpOutBuf, lpAdr1->lpAddrBook[ index ]->szCixName );
            return( TRUE );
            }
      lpAdr1 = lpAdr1->lpNext;
      }
   return( FALSE );
}

/* This function locates any existing address book entry
 * based on the full name.
 */
BOOL FASTCALL Amaddr_Find( LPSTR lpszFullName, ADDRINFO FAR * lpAddrInfo )
{
   LPADDRBOOK lpAdr1;

   lpAdr1 = lpAddrBookFirst;
   while( lpAdr1 ) {
      if( !lpAdr1->fGroup )
         if( lstrcmpi( lpszFullName, lpAdr1->szFullName ) == 0 )
            {
            if( lpAddrInfo )
               {
               lstrcpy( lpAddrInfo->szCixName, lpAdr1->szCixName );
               lstrcpy( lpAddrInfo->szFullName, lpAdr1->szFullName );
               lstrcpy( lpAddrInfo->szComment, lpAdr1->szComment );
               }
            return( TRUE );
            }
      lpAdr1 = lpAdr1->lpNext;
      }
   return( FALSE );
}

/* This function locates any existing address book entry
 * based on the full name.
 */
LPADDRBOOK FASTCALL Amaddr_FindAddress( LPSTR lpszCixName )
{
   char szAddress[ AMLEN_INTERNETNAME+3 ];
   LPADDRBOOK lpAdr1;

   strcpy( szAddress, lpszCixName );
   StripLeadingTrailingQuotes( szAddress );
   lpAdr1 = lpAddrBookFirst;
   while( lpAdr1 ) {
      if( lpAdr1->fGroup )
         {
         if( lstrcmpi( szAddress, lpAdr1->szFullName ) == 0 )
            return( lpAdr1 );
         }
      else
         if( lstrcmpi( lpszCixName, lpAdr1->szCixName ) == 0 )
            return( lpAdr1 );
      lpAdr1 = lpAdr1->lpNext;
      }
   return( NULL );
}

/* This function deletes the specified address book group.
 */
BOOL EXPORT WINAPI Amaddr_DeleteGroup( char * lpszGroupName )
{
   LPADDRBOOKGROUP lpAdr1;
   LPADDRBOOKGROUP lpAdr2;

   if( !fAddrBookLoaded )
      Amaddr_Load();
   lpAdr1 = (LPADDRBOOKGROUP)lpAddrBookFirst;
   lpAdr2 = NULL;
   while( lpAdr1 )
      {
      if( lpAdr1->fGroup )
         if( lstrcmpi( lpszGroupName, lpAdr1->szGroupName ) == 0 )
            {
            if( !lpAdr2 )
               lpAddrBookFirst = (LPADDRBOOK)lpAdr1->lpNext;
            else
               lpAdr2->lpNext = lpAdr1->lpNext;
            FreeMemory( &lpAdr1 );
            return( TRUE );
            }
      lpAdr2 = lpAdr1;
      lpAdr1 = lpAdr1->lpNext;
      }
   return( FALSE );
}

/* This function returns the next entry in the address
 * book by handle.
 */
HADDRBOOK EXPORT WINAPI Amaddr_GetNextEntry( HADDRBOOK hAddr, ADDRINFO FAR * lpAddrInfo )
{
   LPADDRBOOK lpAdr1;

   if( !fAddrBookLoaded )
      Amaddr_Load();
   if( hAddr == NULL )
      lpAdr1 = (LPADDRBOOK)lpAddrBookFirst;
   else
      lpAdr1 = ((LPADDRBOOK)hAddr)->lpNext;
   while( lpAdr1 )
      {
      if( !lpAdr1->fGroup )
         {
         lstrcpy( lpAddrInfo->szCixName, lpAdr1->szCixName );
         lstrcpy( lpAddrInfo->szFullName, lpAdr1->szFullName );
         lstrcpy( lpAddrInfo->szComment, lpAdr1->szComment );
         break;
         }
      lpAdr1 = lpAdr1->lpNext;
      }
   return( (HADDRBOOK)lpAdr1 );
}

/* This function returns the next group in the address
 * book by handle.
 */
HADDRBOOK EXPORT WINAPI Amaddr_GetNextGroup( HADDRBOOK hAddr, LPSTR lpszGroupName )
{
   LPADDRBOOKGROUP lpAdr1;

   if( !fAddrBookLoaded )
      Amaddr_Load();
   if( hAddr == NULL )
      lpAdr1 = (LPADDRBOOKGROUP)lpAddrBookFirst;
   else
      lpAdr1 = ((LPADDRBOOKGROUP)hAddr)->lpNext;
   while( lpAdr1 )
      {
      if( lpAdr1->fGroup )
         {
         lstrcpy( lpszGroupName, lpAdr1->szGroupName );
         break;
         }
      lpAdr1 = lpAdr1->lpNext;
      }
   return( (HADDRBOOK)lpAdr1 );
}

/* This function deletes the specified address book entry.
 */
LPADDRBOOK FASTCALL Amaddr_DeleteEntry( LPADDRBOOK lpAddrBook )
{
   LPADDRBOOK lpAdr1;
   LPADDRBOOK lpAdr2;

   if( !fAddrBookLoaded )
      Amaddr_Load();
   lpAdr1 = lpAddrBookFirst;
   lpAdr2 = NULL;
   while( lpAdr1 )
      {
      LPADDRBOOK lpAddrNext;
      BOOL fFound;

      lpAddrNext = lpAdr1->lpNext;
      fFound = FALSE;
      if( lpAdr1 == lpAddrBook )
         {
         if( !lpAdr2 )
            lpAdr2 = lpAddrBookFirst = lpAddrNext;
         else
            lpAdr2->lpNext = lpAddrNext;
         FreeMemory( &lpAdr1 );
         fFound = TRUE;
         }
      else if( lpAdr1->fGroup )
         {
         LPADDRBOOKGROUP lpAdrGroup = (LPADDRBOOKGROUP)lpAdr1;
         register WORD c;

         /* If this is a group, find the entry in the group.
          */
         for( c = 0; c < lpAdrGroup->cGroup; ++c )
            if( lpAdrGroup->lpAddrBook[ c ] == lpAddrBook )
               {
               for( ;c < lpAdrGroup->cGroup; ++c )
                  lpAdrGroup->lpAddrBook[ c ] = lpAdrGroup->lpAddrBook[ c + 1 ];
               if( --lpAdrGroup->cGroup == 0 )
                  {
                  /* If the group is now empty, delete the group.
                   */
                  if( !lpAdr2 )
                     lpAdr2 = lpAddrBookFirst = lpAddrNext;
                  else
                     lpAdr2->lpNext = lpAddrNext;
                  FreeMemory( &lpAdr1 );
                  }
               else
                  lpAdr2 = lpAdr1;
               lpAdr1 = lpAddrNext;
               fFound = TRUE;
               break;
               }
         }
      if( !fFound )
         lpAdr2 = lpAdr1;
      lpAdr1 = lpAddrNext;
      }
   return( lpAdr2 );
}

/* This function deletes an address book.
 */
BOOL WINAPI EXPORT Amaddr_DeleteAddressBook( void )
{
   LPADDRBOOK lpa;
   LPADDRBOOK lpaNext;

   if( !fAddrBookLoaded )
      Amaddr_Load();
   for( lpa = lpAddrBookFirst; lpa; lpa = lpaNext )
      {
      lpaNext = lpa->lpNext;
      FreeMemory( &lpa );
      }
   lpAddrBookFirst = NULL;
   return( TRUE );
}

/* This function loads the address book if it is not
 * already loaded.
 */
void FASTCALL Amaddr_Load( void )
{
   static BOOL fLoading = FALSE;

   if( fLoading )
      return;
   fLoading = TRUE;
   if( !fAddrBookLoaded )
   {
      HNDFILE fh;

      /* Look for old format address book first.
       */
      Amaddr_LoadOldFormat( GetFocus() );

      /* Load new format address book.
       */
      Amuser_GetActiveUserDirectory( szAddrBookPath, _MAX_FNAME );
      strcat( szAddrBookPath, "\\" );
      strcat( szAddrBookPath, szAddrBookFile );
      if( ( fh = Amfile_Open( szAddrBookPath, AOF_READ ) ) == HNDFILE_ERROR )
         {
         /* No address book, so create one
          */
         if( ( fh = Amfile_Open( szDefAddrBook, AOF_READ ) ) != HNDFILE_ERROR )
            {
            LPLBF hlbf;

            if( hlbf = Amlbf_Open( fh, AOF_READ ) )
               {
               char sz[ 128 ];

               /* Read each line from the default book and add it to
                * the address book.
                */
               while( Amlbf_Read( hlbf, sz, sizeof(sz), NULL, NULL, &fIsAmeol2Scratchpad ) )
                  {
                  char * pszCixName;
                  char * pszFullName;
                  char * pszComment;

                  if( NULL != ( pszCixName = MakeCompactString( sz ) ) )
                     {
                     pszFullName = pszCixName + strlen( pszCixName ) + 1;
                     pszComment = pszFullName + strlen( pszFullName ) + 1;
                     StripLeadingTrailingQuotes( pszFullName );
                     StripLeadingTrailingQuotes( pszComment );
                     if (lstrlen(pszCixName) >= AMLEN_INTERNETNAME) 
                        pszCixName[ AMLEN_INTERNETNAME ] = '\0'; // !!SM 2044
                     if (lstrlen(pszFullName) >= AMLEN_FULLCIXNAME) 
                        pszFullName[ AMLEN_FULLCIXNAME ] = '\0'; // !!SM 2044
                     if (lstrlen(pszComment) >= AMLEN_COMMENT) 
                        pszComment[AMLEN_COMMENT] = '\0';        // !!SM 2044

                     if( *pszCixName && *pszFullName )
                        {
                        ADDRINFO addrinfo;

                        strcpy( addrinfo.szCixName, pszCixName );
                        strcpy( addrinfo.szFullName, pszFullName );
                        strcpy( addrinfo.szComment, pszComment );
                        Amaddr_SetEntry( NULL, &addrinfo );
                        }
                     FreeMemory( &pszCixName );
                     }
                  }
               Amlbf_Close( hlbf );
               Amaddr_CommitChanges();
               }
            }
         }
      else
         {
         WORD wVersion;
         WORD fGroup;
         BOOL fBook21;

         fBook21 = FALSE;
         if( Amfile_Read( fh, (LPSTR)&wVersion, sizeof( WORD ) ) == sizeof( WORD ) )
            {
            if( wVersion == AB_VERSION )
               fBook21 = TRUE;
            else
               Amfile_SetPosition( fh, 0L, ASEEK_BEGINNING );
            }
         while( Amfile_Read( fh, (LPSTR)&fGroup, sizeof( WORD ) ) == sizeof( WORD ) )
            if( fGroup )
               {
               char szGroupName[ AMLEN_INTERNETNAME+1 ];
               register WORD c;
               WORD cGroup;

               Amfile_Read( fh, szGroupName, AMLEN_INTERNETNAME );
               szGroupName[ AMLEN_INTERNETNAME ] = '\0'; // !!SM 2.55
               Amfile_Read( fh, &cGroup, sizeof( WORD ) );
               Amaddr_CreateGroup( szGroupName );
               for( c = 0; c < cGroup; ++c )
                  {
                  char szFullName[ AMLEN_INTERNETNAME+1 ];

                  Amfile_Read( fh, szFullName, AMLEN_INTERNETNAME );
                  szFullName[ AMLEN_INTERNETNAME ] = '\0'; // !!SM 2.55
                  Amaddr_AddToGroup( szGroupName, szFullName );
                  }
               }
            else
               {
               ADDRINFO addrinfo;
               char szFaxNo[ AMLEN_FAXNO+1 ];

               Amfile_Read( fh, addrinfo.szCixName, AMLEN_INTERNETNAME );
               Amfile_Read( fh, addrinfo.szFullName, AMLEN_FULLCIXNAME );
               addrinfo.szCixName[ AMLEN_INTERNETNAME ] = '\0';
               addrinfo.szFullName[ AMLEN_FULLCIXNAME ] = '\0';
               if( fBook21 )
               {
                  Amfile_Read( fh, addrinfo.szComment, AMLEN_COMMENT );
               }
               else
               {
                  Amfile_Read( fh, szFaxNo, AMLEN_FAXNO );
                  addrinfo.szComment[ 0 ] = '\0';
               }
               addrinfo.szComment[AMLEN_COMMENT] = '\0'; // !!SM 2.55
               if( *addrinfo.szFullName == '\0' )
                  strcpy( addrinfo.szFullName, addrinfo.szCixName );
               Amaddr_SetEntry( NULL, &addrinfo );
               }
         Amfile_Close( fh );
         if( !fBook21 )
            Amaddr_CommitChanges();
         }
      fAddrBookLoaded = TRUE;
   }
   fLoading = FALSE;
}

/* This function loads the old format address book.
 */
void FASTCALL Amaddr_LoadOldFormat( HWND hwnd )
{
   LPSTR lpBuf;
   int cbBuf;

   INITIALISE_PTR(lpBuf );

   /* Get list of address books.
    */
   cbBuf = 30000;
   if( fNewMemory( &lpBuf, cbBuf ) )
      {
      LPSTR lpPtr;

      Amuser_GetPPString( szAddrWndName, NULL, "", lpBuf, cbBuf );
      lpPtr = lpBuf;
      while( *lpPtr )
         {
         if( Amuser_GetPPString( szAddrWndName, lpPtr, "", szAddrBookPath, _MAX_FNAME ) )
            {
            if( Amaddr_Ameol2Import( hwnd, szAddrBookPath ) )
               {
               /* Don't delete the old file. Just rename the extension
                * so that it can be recovered if anything goes wrong.
                */
               strcpy( lpPathBuf, szAddrBookPath );
               ChangeExtension( lpPathBuf, "ABK" );
               Amfile_Rename( szAddrBookPath, lpPathBuf );
               }
            }
         lpPtr += lstrlen(lpPtr) + 1;
         }
      FreeMemory( &lpBuf );
      }
}

/* Found an old A2 style address book, so offer to
 * read it
 */
BOOL EXPORT WINAPI Amaddr_Ameol2Import( HWND hwnd, char * pszPath )
{
   HNDFILE fh;
   BOOL fOk;

   fOk = FALSE;
   if( ( fh = Amfile_Open( pszPath, AOF_READ ) ) != HNDFILE_ERROR )
      {
      ADDRBOOKHEADER hdr;

      if( IDNO == fMessageBox( GetFocus(), 0, GS(IDS_STR1177), MB_YESNO|MB_ICONQUESTION ) )
         return( FALSE );
      Amfile_Read( fh, &hdr, sizeof(ADDRBOOKHEADER) );
      if( hdr.finfo.wType != ADDR_FILE_FTYPE )
         fMessageBox( GetFocus(), 0, "Not an Ameol address book", MB_OK|MB_ICONINFORMATION );
      else if( hdr.finfo.wVersion != ADDR_FILE_FVERSION )
         fMessageBox( GetFocus(), 0, "Invalid Ameol address book version", MB_OK|MB_ICONINFORMATION );
      else
         fOk = LoadTree( fh, 65535U, "" );
      Amfile_Close( fh );
      }
   return( fOk );
}

/* This function loads the address book tree.
 */
BOOL FASTCALL LoadTree( HNDFILE fh, UINT cChildren, char * lpszGroupName )
{
   register UINT c;

   /* Loop for each entry in the current group.
    */
   for( c = 0; c < cChildren; ++c )
      {
      A2OLDBK_HEADER hr;

      /* Read one header item.
       */
      if( Amfile_Read( fh, &hr, sizeof(A2OLDBK_HEADER) ) != sizeof(A2OLDBK_HEADER) )
         break;

      /* Create the new group.
       */
      if( hr.fGroup )
         {
         char szGroupName[ 64 ];
         int index;
         int nSub;

         /* Resolve group name clashes by appending (x) to the
          * group name.
          */
         strcpy( szGroupName, hr.gr.szGroupname );
         index = min( strlen( szGroupName ), 60 );
         for( nSub = 2; Amaddr_IsGroup( szGroupName ) && nSub < 10; ++nSub )
            wsprintf( szGroupName + index, " (%d)", nSub++ );
         if( 10 == nSub )
            return( FALSE );

         /* Now try to create the group.
          */
         if( !Amaddr_CreateGroup( szGroupName ) )
            return( FALSE );
         if( !LoadTree( fh, hr.gr.cEntries, szGroupName ) )
            return( FALSE );
         }
      else
         {
         LPSTR lpen;

         /* Compute the size of the rest of the entry (the variable
          * sized data block), allocate memory to contain it and read
          * the data.
          */
         INITIALISE_PTR(lpen);
         if( fNewMemory( &lpen, hr.en.wSize ) )
            {
            if( Amfile_Read( fh, lpen, hr.en.wSize ) == hr.en.wSize )
               {
               char * lpszFullName;
               char * lpszCixName;
               char * lpszComment;

               lpszFullName = lpen + hr.en.offDisplayname;
               lpszCixName = lpen + hr.en.offAddress;
               lpszComment = lpen + hr.en.offComment;
               Amaddr_CreateEntry( lpszCixName, lpszFullName, lpszComment );
               if( *lpszGroupName )
                  Amaddr_AddToGroup( lpszGroupName, lpszFullName );
               }
            FreeMemory( &lpen );
            }
         }
      }
   return( TRUE );
}

/* This function creates an address book group.
 */
BOOL WINAPI EXPORT Amaddr_CreateGroup( LPSTR lpszGroupName )
{
   if( !fAddrBookLoaded )
      Amaddr_Load();
   INITIALISE_PTR(lpGlbAddrBook);
   if( fNewMemory( &lpGlbAddrBook, sizeof( ADDRBOOKGROUP ) ) )
      {
      LPADDRBOOKGROUP lpAddr;
      LPADDRBOOKGROUP lpAddr2 = NULL;

      for( lpAddr = (LPADDRBOOKGROUP)lpAddrBookFirst; lpAddr; lpAddr = lpAddr->lpNext )
         {
         if( lstrcmpi( lpAddr->szGroupName, lpszGroupName ) > 0 )
            break;
         lpAddr2 = lpAddr;
         }
      if( !lpAddr2 )
         lpAddrBookFirst = lpGlbAddrBook;
      else
         lpAddr2->lpNext = (LPADDRBOOKGROUP)lpGlbAddrBook;
      ((LPADDRBOOKGROUP)lpGlbAddrBook)->lpNext = lpAddr;
      ((LPADDRBOOKGROUP)lpGlbAddrBook)->fGroup = TRUE;
      lstrcpy( ((LPADDRBOOKGROUP)lpGlbAddrBook)->szGroupName, lpszGroupName );
      ((LPADDRBOOKGROUP)lpGlbAddrBook)->cGroup = 0;
      return( TRUE );
      }
   return( FALSE );
}

/* This function adds the specified entry to the group.
 */
BOOL WINAPI EXPORT Amaddr_AddToGroup( LPSTR lpszGroupName, LPSTR lpszFullName )
{
   LPADDRBOOKGROUP lpAddr;
   LPADDRBOOKGROUP lpAddr2;

   if( !fAddrBookLoaded )
      Amaddr_Load();
   lpAddr2 = NULL;
   for( lpAddr = (LPADDRBOOKGROUP)lpAddrBookFirst; lpAddr; lpAddr = lpAddr->lpNext )
      {
      if( lpAddr->fGroup )
         if( lstrcmpi( lpAddr->szGroupName, lpszGroupName ) == 0 )
            break;
      lpAddr2 = lpAddr;
      }
   if( lpAddr )
      {
      register WORD c;
      LPADDRBOOK lpAdr1;
      LPADDRBOOK lpAdr2;
      LPADDRBOOK lpAddrNew;

      INITIALISE_PTR(lpAddrNew);

      for( c = 0; c < lpAddr->cGroup; ++c )
         if( lstrcmpi( lpAddr->lpAddrBook[ c ]->szFullName, lpszFullName ) == 0 )
            return( TRUE );      // Already there...
      if( lpAddr->cGroup > 0 )
         {
         LPADDRBOOKGROUP lpAddr4;

         lpAddr4 = lpAddr->lpNext;
         if( !fResizeMemory( &lpAddr, sizeof( ADDRBOOKGROUP ) + lpAddr->cGroup * sizeof( LPADDRBOOK ) ) )
            return( FALSE );
         if( !lpAddr2 )
            lpAddrBookFirst = (LPADDRBOOK)lpAddr;
         else
            lpAddr2->lpNext = lpAddr;
         lpAddr->lpNext = lpAddr4;
         lpGlbAddrBook = (LPADDRBOOK)lpAddr;
         }
      lpAdr1 = lpAddrBookFirst;
      lpAdr2 = NULL;
      while( lpAdr1 )
         {
         register int r;

         if( !lpAdr1->fGroup )
            if( ( r = lstrcmpi( lpszFullName, lpAdr1->szFullName ) ) == 0 )
               {
               lpAddr->lpAddrBook[ lpAddr->cGroup++ ] = lpAdr1;
               return( TRUE );
               }
            else if( r < 0 )
               break;
         lpAdr2 = lpAdr1;
         lpAdr1 = lpAdr1->lpNext;
         }

      /* Wasn't there, so create an entry for this CIX name
       * If we're loading the address book, and a group appears before one or more of
       * the names in the group, then this will happen. In this case, when we find the
       * name later during the load, we'll simply be updating the record created here.
       */
      if( fNewMemory( &lpAddrNew, sizeof( ADDRBOOK ) ) )
         {
         if( !lpAdr2 )
            lpAddrBookFirst = lpAddrNew;
         else
            lpAdr2->lpNext = lpAddrNew;
         lpAddrNew->lpNext = lpAdr1;
         lpAddrNew->fGroup = FALSE;
         lstrcpy( lpAddrNew->szFullName, lpszFullName );
         lpAddrNew->szCixName[ 0 ] = '\0';
         lpAddrNew->szComment[ 0 ] = '\0';
         lpAddr->lpAddrBook[ lpAddr->cGroup++ ] = lpAddrNew;
         return( TRUE );
         }
      return( FALSE );
      }
   return( TRUE );
}

/* From v2.60, this is a stub and no longer does anything.
 */
BOOL WINAPI EXPORT Amaddr_CreateFaxBookEntry( LPSTR lpszFullName, LPSTR lpszFaxNo )
{
   return FALSE;
}

/* From v2.60, this is a stub and no longer does anything.
 */
BOOL WINAPI EXPORT Amaddr_DeleteFaxBookEntry( LPSTR lpszFullName, LPSTR lpszFaxNo )
{
   return FALSE;
}

/* This function saves the address book.
 */
void FASTCALL Amaddr_CommitChanges( void )
{
   HNDFILE fh;

   /* Save the address book only if it has been loaded.
    */
   if( fAddrBookLoaded )
      {
      Amuser_GetActiveUserDirectory( szAddrBookPath, _MAX_FNAME );
      strcat( szAddrBookPath, "\\" );
      strcat( szAddrBookPath, szAddrBookFile );
      if( !lpAddrBookFirst )
         {
         if( Amfile_QueryFile( szAddrBookPath ) )
            Amfile_Delete( szAddrBookPath );
         }
      else if( ( fh = Amfile_Create( szAddrBookPath, 0 ) ) != HNDFILE_ERROR )
         {
         LPADDRBOOK lpAddrBook;
         WORD wVersion = AB_VERSION;

         Amfile_Write( fh, (LPCSTR)&wVersion, sizeof( WORD ) );
         for( lpAddrBook = lpAddrBookFirst; lpAddrBook; lpAddrBook = lpAddrBook->lpNext )
            {
            Amfile_Write( fh, (LPCSTR)&lpAddrBook->fGroup, sizeof( WORD ) );
            if( lpAddrBook->fGroup )
               {
               LPADDRBOOKGROUP lpGroup;
               register WORD c;

               lpGroup = (LPADDRBOOKGROUP)lpAddrBook;
               Amfile_Write( fh, lpGroup->szGroupName, AMLEN_INTERNETNAME );
               Amfile_Write( fh, (LPCSTR)&lpGroup->cGroup, sizeof( WORD ) );
               for( c = 0; c < lpGroup->cGroup; ++c )
                  Amfile_Write( fh, lpGroup->lpAddrBook[ c ]->szFullName, AMLEN_INTERNETNAME );
               }
            else
               {
               Amfile_Write( fh, lpAddrBook->szCixName, AMLEN_INTERNETNAME );
               Amfile_Write( fh, lpAddrBook->szFullName, AMLEN_FULLCIXNAME );
               Amfile_Write( fh, lpAddrBook->szComment, AMLEN_COMMENT );
               }
            }
         Amfile_Close( fh );
         }
   }
}

/* This function handles the address book editor
 */
BOOL EXPORT CALLBACK AddrBookEdit( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, AddrBookEdit_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the AddrBookEdit
 * dialog.
 */
LRESULT FASTCALL AddrBookEdit_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, AddrBookEdit_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, AddrBookEdit_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsADDREDIT );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL AddrBookEdit_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   BOOL fOk = FALSE;
   LPADDRBOOK lpAddrBook;

   /* Fill the dialog from any existing entry.
    */
   lpAddrBook = (LPADDRBOOK)lParam;
   if( lParam )
      {
      SetDlgItemText( hwnd, IDD_CIXNAME, lpAddrBook->szCixName );
      SetDlgItemText( hwnd, IDD_FULLNAME, lpAddrBook->szFullName );
      SetDlgItemText( hwnd, IDD_COMMENT, lpAddrBook->szComment );
      fOk = *lpAddrBook->szCixName && *lpAddrBook->szFullName;
      }

   /* Set edit field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_FULLNAME ), AMLEN_FULLCIXNAME );
   Edit_LimitText( GetDlgItem( hwnd, IDD_CIXNAME ), AMLEN_INTERNETNAME );
   Edit_LimitText( GetDlgItem( hwnd, IDD_COMMENT ), AMLEN_COMMENT );

   /* If any entries are prefilled then set the focus to
    * the first blank entry.
    */
   if( lParam )
      if( *lpAddrBook->szCixName )
         hwndFocus = GetDlgItem( hwnd, IDD_FULLNAME );

   /* Disable OK button if not all the entries are there
    */
   EnableControl( hwnd, IDOK, fOk );
   SetFocus( hwndFocus );
   SetWindowLong( hwnd, DWL_USER, lParam );
   return( FALSE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL AddrBookEdit_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_FULLNAME:
      case IDD_CIXNAME:
         if( codeNotify == EN_UPDATE )
            EnableControl( hwnd, IDOK,
                  Edit_GetTextLength( GetDlgItem( hwnd, IDD_CIXNAME ) ) &&
                  Edit_GetTextLength( GetDlgItem( hwnd, IDD_FULLNAME ) ) );
         break;

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;

      case IDOK: {
         LPADDRBOOK lpAddrBook;
         BOOL fOk;

         /* This same code handles creating a new and editing an
          * existing address book entry.
          */
         lpAddrBook = (LPADDRBOOK)GetWindowLong( hwnd, DWL_USER );
         if( lpAddrBook )
            {
            GetDlgItemText( hwnd, IDD_FULLNAME, lpAddrBook->szFullName, AMLEN_FULLCIXNAME+1 );
            GetDlgItemText( hwnd, IDD_CIXNAME, lpAddrBook->szCixName, AMLEN_INTERNETNAME+1 );
            GetDlgItemText( hwnd, IDD_COMMENT, lpAddrBook->szComment, AMLEN_COMMENT+1 );
            StripLeadingTrailingSpaces( lpAddrBook->szCixName );
            lpGlbAddrBook = lpAddrBook;
            fOk = TRUE;
            }
         else {
            char szCixName[ AMLEN_INTERNETNAME+1 ];
            char szFullName[ AMLEN_FULLCIXNAME+1 ];
            char szComment[ AMLEN_COMMENT+1 ];

            /* Get the new entries.
             */
            GetDlgItemText( hwnd, IDD_CIXNAME, szCixName, AMLEN_INTERNETNAME+1 );
            GetDlgItemText( hwnd, IDD_FULLNAME, szFullName, AMLEN_FULLCIXNAME+1 );
            GetDlgItemText( hwnd, IDD_COMMENT, szComment, AMLEN_COMMENT+1 );

            /* First make sure that this entry isn't already
             * present.
             */
            if( Amaddr_Find( szFullName, NULL ) )
               {
               wsprintf( lpTmpBuf, GS(IDS_STR444), (LPSTR)szFullName );
               fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
               break;
               }
            StripLeadingTrailingSpaces( szCixName );
            fOk = Amaddr_CreateEntry( szCixName, szFullName, szComment );
            }
         Amaddr_CommitChanges();
         EndDialog( hwnd, fOk );
         break;
         }
      }
}

/* This function handles adding names to a group
 */
BOOL EXPORT CALLBACK AddrBookGroup( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, AddrBookGroup_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the AddrBookGroup
 * dialog.
 */
LRESULT FASTCALL AddrBookGroup_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, AddrBookGroup_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, AddrBookGroup_OnCommand );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, AddrbookWnd_OnMeasureItem );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsEDITGROUP );
         break;

      case WM_DRAWITEM:
         MailBook_OnDrawItem( hwnd, (const DRAWITEMSTRUCT FAR *)lParam, TRUE );
         return( 0L );

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL AddrBookGroup_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndEdit;
   HWND hwndList;

   /* If an existing group handle is supplied, fill the list
    * box with the group entries.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   if( lParam )
      {
      LPADDRBOOKGROUP lpGroup = (LPADDRBOOKGROUP)lParam;
      register WORD c;

      for( c = 0; c < lpGroup->cGroup; ++c )
         ListBox_AddString( hwndList, lpGroup->lpAddrBook[ c ] );
      Edit_SetText( hwndEdit, lpGroup->szGroupName );
      ListBox_SetSel( hwndList, TRUE, 0 );
      }
   Edit_LimitText( hwndEdit, AMLEN_INTERNETNAME );
   EnableControl( hwnd, IDD_DELETE, ListBox_GetCount( hwndList ) > 0 );
   EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndEdit ) > 0 );
   SetWindowLong( hwnd, DWL_USER, lParam );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL AddrBookGroup_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EDIT:
         if( codeNotify == EN_UPDATE )
            {
            HWND hwndEdit;
            HWND hwndList;
         
            hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
            hwndList = GetDlgItem( hwnd, IDD_LIST );
            EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndEdit ) > 0 && ListBox_GetCount( hwndList ) > 0 );
            }
         break;

      case IDD_ADD: {
         HWND hwndList;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_GROUPADD), GroupAdd, (LPARAM)(LPSTR)hwndList ) )
            {
            HWND hwndEdit;

            hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
            EnableControl( hwnd, IDD_DELETE, TRUE );
            EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndEdit ) > 0 );
            }
         break;
         }

      case IDD_DELETE: {
         HWND hwndList;
         int index;
         int count;

         /* Delete the selected entries from the group.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         count = ListBox_GetCount( hwndList );
         for( index = 0; index < count; ++index )
            if( ListBox_GetSel( hwndList, index ) )
               {
               ListBox_DeleteString( hwndList, index );
               fChangedGroup = TRUE;
               --index;
               --count;
               }

         /* Select next item, if any, or disable Delete
          * button if all gone.
          */
         if( 0 != count )
            {
            if( index == count )
               --index;
            ListBox_SetSel( hwndList, TRUE, index );
            }
         else
            {
            EnableControl( hwnd, IDOK, FALSE );
            SetFocus( GetDlgItem( hwnd, IDD_ADD ) );
            EnableControl( hwndList, IDD_DELETE, FALSE );
            }
         break;
         }

      case IDOK: {
         char szGroupName[ AMLEN_INTERNETNAME+1 ];
         HWND hwndList;
         HWND hwndEdit;
         register int c;
         int count;

         /* All done. If we have an existing group then 
          */
         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         Edit_GetText( hwndEdit, szGroupName, AMLEN_INTERNETNAME+1 );
         count = ListBox_GetCount( hwndList );
         lpGlbAddrBook = (LPADDRBOOK)GetWindowLong( hwnd, DWL_USER );
         if( Amaddr_GetGroup( szGroupName, NULL ) )
            {
            if( count )
               {
                  if (fChangedGroup)
                     {
                        wsprintf( lpTmpBuf, GS(IDS_STR72), (LPSTR)szGroupName );
                        if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION ) == IDNO )
                           break;
                     }
               }
            else
               {
               wsprintf( lpTmpBuf, GS(IDS_STR435), (LPSTR)szGroupName );
               if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION ) == IDNO )
                  break;
               lpGlbAddrBook = NULL;
               }
            Amaddr_DeleteGroup( szGroupName );
            }

         /* We're adding names to the group, so take the quick approach
          * and simply vape and recreate the group from scratch.
          */
         if( count )
            {
            if( lpGlbAddrBook )
               Amaddr_DeleteGroup( ((LPADDRBOOKGROUP)lpGlbAddrBook)->szGroupName );
            if( !Amaddr_CreateGroup( szGroupName ) )
               break;
            for( c = 0; c < count; ++c )
               {
               LPADDRBOOK lpAddr1;
   
               lpAddr1 = (LPADDRBOOK)ListBox_GetItemData( hwndList, c );
               Amaddr_AddToGroup( szGroupName, lpAddr1->szFullName );
               }
            }
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* This function handles the Add Group dialog.
 */
BOOL EXPORT CALLBACK GroupAdd( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, GroupAdd_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the GroupAdd
 * dialog.
 */
LRESULT FASTCALL GroupAdd_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, GroupAdd_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, GroupAdd_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsEDITADDRBOOK );
         break;

      case WM_DRAWITEM:
         MailBook_OnDrawItem( hwnd, (const DRAWITEMSTRUCT FAR *)lParam, TRUE );
         return( 0L );

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL GroupAdd_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   char szGroupName[ AMLEN_INTERNETNAME+1 ];
   LPADDRBOOK lpAddrBook;
   HWND hwndDlg;
   HWND hwndList;
   HWND hwndList2;

   /* Fill the list with all member names.
    */
   lpAddrBook = lpAddrBookFirst;
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   hwndList2 = (HWND)lParam;
   while( lpAddrBook )
      {
      if( !lpAddrBook->fGroup )
         {
         if( ListBox_FindStringExact( hwndList2, -1, lpAddrBook ) == LB_ERR )
            ListBox_AddString( hwndList, lpAddrBook );
         }
      lpAddrBook = lpAddrBook->lpNext;
      }
   ListBox_SetSel( hwndList, TRUE, 0 );

   /* Get the group name from the parent dialog. This is important because
    * the group may not exist yet so we have to use whatever the user has
    * entered as a group name.
    */
   hwndDlg = GetParent( hwndList2 );
   Edit_GetText( GetDlgItem( hwndDlg, IDD_EDIT ), szGroupName, AMLEN_INTERNETNAME );
   SetWindowText( GetDlgItem( hwnd, IDD_GROUPNAME ), szGroupName );

   /* Save handle and disable OK because nothing has been
    * selected yet.
    */
   SetWindowLong( hwnd, DWL_USER, lParam );
   EnableControl( hwnd, IDOK, ListBox_GetCount( hwndList ) > 0 );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL GroupAdd_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_LIST:
         if( codeNotify == LBN_SELCHANGE )
            EnableControl( hwnd, IDOK, ListBox_GetSelCount( hwndCtl ) > 0 );
         break;

      case IDOK: {
         HWND hwndList2;
         HWND hwndList;
         register int c;
         BOOL fFirst = TRUE;
         hwndList2 = (HWND)GetWindowLong( hwnd, DWL_USER );
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         for( c = 0; c < ListBox_GetCount( hwndList ); ++c )
            if( ListBox_GetSel( hwndList, c ) )
               {
               LPARAM lpAddrBook;
               int n;

               lpAddrBook = ListBox_GetItemData( hwndList, c );
               if( ListBox_FindStringExact( hwndList2, -1, lpAddrBook ) == LB_ERR )
                  {
                  n = ListBox_AddString( hwndList2, lpAddrBook );
                  if( fFirst )
                     {
                     ListBox_SetCurSel( hwndList2, n );
                     fChangedGroup = TRUE;
                     fFirst = FALSE;
                     }
                  }
               }
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* This function handles the WM_DRAWITEM message
 */
void FASTCALL MailBook_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem, BOOL fFullNames )
{
   if( lpDrawItem->itemID != -1 )
      {
      char szAddress[ AMLEN_INTERNETNAME+1 ];
      LPADDRBOOK lpAddrBook;
      HFONT hOldFont;
      HWND hwndList;
      COLORREF tmpT;
      COLORREF tmpB;
      COLORREF T;
      COLORREF B;
      HBRUSH hbr;
      LPSTR lpsz;
      RECT rc;

      /* Get the stuff we're supposed to be painting.
       */
      hwndList = lpDrawItem->hwndItem;
      lpAddrBook = (LPADDRBOOK)ListBox_GetItemData( hwndList, lpDrawItem->itemID );
      ListBox_GetText( hwndList, lpDrawItem->itemID, szAddress );
      rc = lpDrawItem->rcItem;
      GetOwnerDrawListItemColours( lpDrawItem, &T, &B );
      hbr = CreateSolidBrush( B );

      /* Erase the entire drawing area
       */
      FillRect( lpDrawItem->hDC, &rc, hbr );
      tmpT = SetTextColor( lpDrawItem->hDC, T );
      tmpB = SetBkColor( lpDrawItem->hDC, B );

      /* Draw the bitmap first
       */
      rc.left += 18;

      /* Draw the CIX name
       */
      if( NULL == lpAddrBook )
         {
         hOldFont = SelectFont( lpDrawItem->hDC, hHelv8Font );
         lpsz = szAddress;
         }
      else
         {
         hOldFont = SelectFont( lpDrawItem->hDC, lpAddrBook->fGroup ? hSys8Font : hHelv8Font );
         if( ( fFullNames && *lpAddrBook->szFullName ) || lpAddrBook->fGroup )
            lpsz = lpAddrBook->szFullName;
         else
            lpsz = lpAddrBook->szCixName;
         }
      ExtTextOut( lpDrawItem->hDC, rc.left, rc.top, ETO_OPAQUE, &rc, lpsz, lstrlen( lpsz ), NULL );
      SelectFont( lpDrawItem->hDC, hOldFont );
      SetTextColor( lpDrawItem->hDC, tmpT );
      SetBkColor( lpDrawItem->hDC, tmpB );
      DeleteBrush( hbr );
      }
}

/* This function implements the address book picker.
 */
BOOL WINAPI EXPORT Amaddr_PickEntry( HWND hwndParent, LPAMADDRPICKLIST lpPickList )
{
   if( lpPickList->wFlags & AMPCKFLG_NOCC )
      return( Adm_Dialog( hRscLib, hwndParent, MAKEINTRESOURCE(IDDLG_SMALLADDRPICKER), PickEntry, (LPARAM)lpPickList ) );
   return( Adm_Dialog( hRscLib, hwndParent, MAKEINTRESOURCE(IDDLG_ADDRPICKER), PickEntry, (LPARAM)lpPickList ) );
}

/* Handles the address book picker dialog.
 */
BOOL EXPORT CALLBACK PickEntry( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, PickEntry_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the Edit Group dialog.
 */
LRESULT FASTCALL PickEntry_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, PickEntry_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, PickEntry_OnCommand );
      HANDLE_MSG( hwnd, WM_COMPAREITEM, AddrbookWnd_OnCompareItem );
      HANDLE_MSG( hwnd, WM_CHARTOITEM, AddrbookWnd_OnCharToItem );

      case WM_VKEYTOITEM:
         return AddrbookWnd_OnKeyToItem( hwnd, LOWORD(wParam), (HWND)lParam, HIWORD(wParam) );

      case WM_DRAWITEM:
         MailBook_OnDrawItem( hwnd, (const DRAWITEMSTRUCT FAR *)lParam, TRUE );
         return( 0L );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsADDRPICKER );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL PickEntry_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPAMADDRPICKLIST lpPickList;
   HWND hwndToList;
   HWND hwndList;
   LPSTR lpsz;

   /* Dereference the picker structure and save it
    */
   lpPickList = (LPAMADDRPICKLIST)lParam;
   SetWindowLong( hwnd, DWL_USER, (LPARAM)lpPickList );

   /* Fill the listbox.
    */
   if( !fAddrBookLoaded )
      Amaddr_Load();
   Amaddr_FillList( hwnd, FALSE, "" );

   /* Fill the To list.
    */
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   VERIFY( hwndToList = GetDlgItem( hwnd, IDD_TOLIST ) );
   if( lpsz = lpPickList->lpszTo )
      while( *lpsz )
         {
         LPADDRBOOK lpAddrBook;
         int index;

         index = ListBox_AddString( hwndToList, lpsz );
         lpAddrBook = Amaddr_FindAddress( lpsz );
         ListBox_SetItemData( hwndToList, index, lpAddrBook );
         if( NULL != lpAddrBook )
            {
            index = RealListBox_FindItemData( hwndList, -1, (LPARAM)lpAddrBook );
            ListBox_DeleteString( hwndList, index );
            }
         lpsz += lstrlen( lpsz ) + 1;
         }
   EnableControl( hwnd, IDD_REMOVETO, ListBox_GetCount( hwndToList ) > 0 );

   /* Fill the CC list.
    */
   if( !( lpPickList->wFlags & AMPCKFLG_NOCC ) )
      {
      HWND hwndCCList;

      VERIFY( hwndCCList = GetDlgItem( hwnd, IDD_CCLIST ) );
      if( lpsz = lpPickList->lpszCC )
         while( *lpsz )
            {
            LPADDRBOOK lpAddrBook;
            int index;

            index = ListBox_AddString( hwndCCList, lpsz );
            lpAddrBook = Amaddr_FindAddress( lpsz );
            ListBox_SetItemData( hwndCCList, index, lpAddrBook );
            if( NULL != lpAddrBook )
               {
               index = RealListBox_FindItemData( hwndList, -1, (LPARAM)lpAddrBook );
               ListBox_DeleteString( hwndList, index );
               }
            lpsz += lstrlen( lpsz ) + 1;
            }
      EnableControl( hwnd, IDD_REMOVECC, ListBox_GetCount( hwndCCList ) > 0 );
      }

   /* Fill the BCC list.
    */
   if( !( lpPickList->wFlags & AMPCKFLG_NOCC ) )
      {
      HWND hwndBCCList;

      VERIFY( hwndBCCList = GetDlgItem( hwnd, IDD_BCCLIST ) );
      if( lpsz = lpPickList->lpszBCC )
         while( *lpsz )
            {
            LPADDRBOOK lpAddrBook;
            int index;

            index = ListBox_AddString( hwndBCCList, lpsz );
            lpAddrBook = Amaddr_FindAddress( lpsz );
            ListBox_SetItemData( hwndBCCList, index, lpAddrBook );
            if( NULL != lpAddrBook )
               {
               index = RealListBox_FindItemData( hwndList, -1, (LPARAM)lpAddrBook );
               ListBox_DeleteString( hwndList, index );
               }
            lpsz += lstrlen( lpsz ) + 1;
            }
      EnableControl( hwnd, IDD_REMOVEBCC, ListBox_GetCount( hwndBCCList ) > 0 );
      }

   /* Might need to reset selection in main list.
    */
   if( 0 == ListBox_GetSelCount( hwndList ) )
      ListBox_SetSel( hwndList, TRUE, 0 );
   return( TRUE );
}

/* This function handles the WM_COMMAND message. Currently only one control
 * on the dialog, the OK button, dispatches this message.
 */ 
void FASTCALL PickEntry_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_ADD: {
         HWND hwndList;

         /* Add a new entry to the address book
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_ADDRBOOKEDIT), AddrBookEdit, 0L ) )
            {
            int index;

            index = ListBox_AddString( hwndList, lpGlbAddrBook );
            ListBox_SetSel( hwndList, FALSE, -1 );
            ListBox_SetSel( hwndList, TRUE, index );
            }
         SetFocus( hwndList );
         break;
         }

      case IDD_EDIT: {
         char szFullName[ AMLEN_INTERNETNAME+1 ];
         HWND hwndList;
         int index;

         /* Edit the selected item. You can only ever edit one
          * item at a time.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         ASSERT( 1 == ListBox_GetSelCount( hwndList ) );
         index = ListBox_GetCaretIndex( hwndList );
         lpGlbAddrBook = (LPADDRBOOK)ListBox_GetItemData( hwndList, index );

         /* Different edit actions depending on whether we're editing a
          * group or an individual item.
          */
         if( lpGlbAddrBook->fGroup )
            {
            LPADDRBOOK lpAddrBookNext;

            lpAddrBookNext = lpGlbAddrBook->lpNext;
            lstrcpy( szFullName, ((LPADDRBOOKGROUP)lpGlbAddrBook)->szGroupName );
            if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE( IDDLG_ADDRBOOKGROUP ), AddrBookGroup, (LPARAM)lpGlbAddrBook ) )
               if( lpGlbAddrBook )
                  {
                  ListBox_DeleteString( hwndList, index );
                  index = ListBox_AddString( hwndList, lpGlbAddrBook );
                  ListBox_SetSel( hwndList, TRUE, index );
                  }
               else
                  {
                  SetWindowRedraw( hwndList, FALSE );
                  Amaddr_FillList( hwnd, FALSE, "" );
                  SetWindowRedraw( hwndList, TRUE );
                  }
            }
         else
            {
            /* We're editing an item. One thing to watch out for is when the
             * user changes the data, its position in the sorted list could
             * be changed!
             */
            lstrcpy( szFullName, lpGlbAddrBook->szFullName );
            if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE( IDDLG_ADDRBOOKEDIT ), AddrBookEdit, (LPARAM)lpGlbAddrBook ) )
               {
               ListBox_DeleteString( hwndList, index );
               index = ListBox_AddString( hwndList, lpGlbAddrBook );
               ListBox_SetSel( hwndList, TRUE, index );
               }
            }
         SetFocus( hwndList );
         break;
         }

      case IDD_LIST:
         if( codeNotify != LBN_DBLCLK )
            break;

      case IDD_ADDBCC:
      case IDD_ADDCC:
      case IDD_ADDTO: {
         LPAMADDRPICKLIST lpPickList;
         HWND hwndList;
         int addloc = ADDLOC_NONE;
         int count;
         int index;
         int start;

         /* Loop for all selected items.
          */
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         count = ListBox_GetCount( hwndList );
         start = -1;
         lpPickList = (LPAMADDRPICKLIST)GetWindowLong( hwnd, DWL_USER );
         if( id == IDD_LIST )
            addloc = ( ( lpPickList->wFlags & AMPCKFLG_SETBCC ) ?  ADDLOC_BCC : ( lpPickList->wFlags & AMPCKFLG_SETCC ) ?  ADDLOC_CC : ADDLOC_TO );
         else
            addloc = ( ( id == IDD_ADDTO ) ?  ADDLOC_TO : ( id == IDD_ADDCC ) ?  ADDLOC_CC : ADDLOC_BCC ) ;
         for( index = 0; index < count; ++index )
            if( ListBox_GetSel( hwndList, index ) )
               {
               LPADDRBOOK lpAddrBook;
               int idDest;

               /* Add this to the To field.
                */
               if( -1 == start )
                  start = index;
               lpAddrBook = (LPADDRBOOK)ListBox_GetItemData( hwndList, index );
               idDest = ( addloc == ADDLOC_TO ) ? IDD_TOLIST : ( addloc == ADDLOC_CC ) ? IDD_CCLIST : IDD_BCCLIST ;
               PickEntry_InsertItem( hwnd, GetDlgItem( hwnd, idDest ), lpAddrBook );
               idDest = ( addloc == ADDLOC_TO ) ? IDD_REMOVETO : ( addloc == ADDLOC_CC ) ? IDD_REMOVECC : IDD_REMOVEBCC;
               EnableControl( hwnd, idDest, TRUE );
               ListBox_DeleteString( hwndList, index );
               --count;
               --index;
               }

         /* All done. If we have anything left, select something if all
          * selected items are deleted. Otherwise disable the buttons that
          * depend on a selection.
          */
         if( 0 != count )
            {
            if( 0 == ListBox_GetSelCount( hwndList ) )
               {
               if( start == count )
                  --start;
               ListBox_SetSel( hwndList, TRUE, start );
               }
            }
         else
            {
            EnableControl( hwnd, IDD_ADDTO, FALSE );
            EnableControl( hwnd, IDD_ADDCC, FALSE );
            EnableControl( hwnd, IDD_ADDBCC, FALSE );
            }
         SetFocus( hwndList );
         break;
         }

      case IDD_BCCLIST:
      case IDD_CCLIST:
      case IDD_TOLIST:
         if( codeNotify != LBN_DBLCLK )
            break;
         /* Fall thru to IDD_REMOVETO logic.
          */

      case IDD_REMOVEBCC:
      case IDD_REMOVECC:
      case IDD_REMOVETO: {
         HWND hwndList;
         int remloc = ADDLOC_NONE;
         int idDest;
         int index;
         int idBtn;

         /* Remove the selected item from the listbox.
          */
         remloc = ( ( id == IDD_REMOVETO || id == IDD_TOLIST ) ?  ADDLOC_TO : ( id == IDD_REMOVECC || id == IDD_CCLIST ) ?  ADDLOC_CC : ADDLOC_BCC ) ;
         idDest = ( remloc == ADDLOC_TO ) ? IDD_TOLIST : ( remloc == ADDLOC_CC ) ? IDD_CCLIST : IDD_BCCLIST ;
         idBtn = ( remloc == ADDLOC_TO ) ? IDD_REMOVETO : ( remloc == ADDLOC_CC ) ? IDD_REMOVECC : IDD_REMOVEBCC ;

         hwndList = GetDlgItem( hwnd, idDest );
         if( LB_ERR != ( index = ListBox_GetCurSel( hwndList ) ) )
            {
            LPADDRBOOK lpAddrBook;

            /* Remove the selected string and highlight the next
             * one, or disable the Remove button if all strings
             * removed.
             */
            lpAddrBook = (LPADDRBOOK)ListBox_GetItemData( hwndList, index );
            if( !PickEntry_DeleteString( hwndList, index ) )
               EnableControl( hwnd, idBtn, FALSE );

            /* Add item back to the full list.
             */
            if( NULL != lpAddrBook )
               {
               VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
               ListBox_AddString( hwndList, (LPSTR)lpAddrBook );
               EnableControl( hwnd, IDD_ADDTO, TRUE );
               EnableControl( hwnd, IDD_ADDCC, TRUE );
               EnableControl( hwnd, IDD_ADDBCC, TRUE );
               }
            SetFocus( hwndList );
            }
         break;
         }

      case IDOK: {
         LPAMADDRPICKLIST lpPickList;
         register int c;
         HWND hwndList;
         int cbList;
         int count;

         /* Get ths user's picklist structure.
          */
         lpPickList = (LPAMADDRPICKLIST)GetWindowLong( hwnd, DWL_USER );
         INITIALISE_PTR(lpPickList->lpszTo);
         INITIALISE_PTR(lpPickList->lpszCC);
         INITIALISE_PTR(lpPickList->lpszBCC);

         /* Loop once for the To list and compute the length of the
          * selected names.
          */
         hwndList = GetDlgItem( hwnd, IDD_TOLIST );
         if( count = ListBox_GetCount( hwndList ) )
            {
            UINT p;

            cbList = 1;
            for( c = 0; c < count; ++c )
               cbList += ListBox_GetTextLen( hwndList, c ) + 1;
   
            /* Allocate memory for the To list and store entries.
             */
            if( fNewMemory( &lpPickList->lpszTo, cbList ) )
               {
               p = 0;
               for( c = 0; c < count; ++c )
                  p += ListBox_GetText( hwndList, c, lpPickList->lpszTo + p ) + 1;
               lpPickList->lpszTo[ p ] = '\0';
               }
            }

         /* Loop once for the CC list and compute the length of the
          * selected names.
          */
         if( !( lpPickList->wFlags & AMPCKFLG_NOCC ) )
            {
            hwndList = GetDlgItem( hwnd, IDD_CCLIST );
            if( count = ListBox_GetCount( hwndList ) )
               {
               UINT p;

               cbList = 1;
               for( c = 0; c < count; ++c )
                  cbList += ListBox_GetTextLen( hwndList, c ) + 1;
      
               /* Allocate memory for the CC list and store entries.
                */
               if( fNewMemory( &lpPickList->lpszCC, cbList ) )
                  {
                  p = 0;
                  for( c = 0; c < count; ++c )
                     p += ListBox_GetText( hwndList, c, lpPickList->lpszCC + p ) + 1;
                  lpPickList->lpszCC[ p ] = '\0';
                  }
               }
            }

         /* Loop once for the BCC list and compute the length of the
          * selected names.
          */
         if( !( lpPickList->wFlags & AMPCKFLG_NOCC ) )
            {
            hwndList = GetDlgItem( hwnd, IDD_BCCLIST );
            if( count = ListBox_GetCount( hwndList ) )
               {
               UINT p;

               cbList = 1;
               for( c = 0; c < count; ++c )
                  cbList += ListBox_GetTextLen( hwndList, c ) + 1;
      
               /* Allocate memory for the CC list and store entries.
                */
               if( fNewMemory( &lpPickList->lpszBCC, cbList ) )
                  {
                  p = 0;
                  for( c = 0; c < count; ++c )
                     p += ListBox_GetText( hwndList, c, lpPickList->lpszBCC + p ) + 1;
                  lpPickList->lpszBCC[ p ] = '\0';
                  }
               }
            }

         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* This function deletes a string from the specified list box
 * and highlights the next one.
 */
BOOL FASTCALL PickEntry_DeleteString( HWND hwndList, int index )
{
   int count;

   index = ListBox_DeleteString( hwndList, index );
   count = ListBox_GetCount( hwndList );
   if( index == count )
      --index;
   if( LB_ERR != index )
      ListBox_SetCurSel( hwndList, index );
   return( index != LB_ERR );
}

/* This function inserts the specified group or entry into the
 * To or CC field.
 */
void FASTCALL PickEntry_InsertItem( HWND hwnd, HWND hwndList, LPADDRBOOK lpAddrBook )
{
   char szGroupname[ AMLEN_INTERNETNAME+3 ];
   LPSTR lpsz;
   int index;

   /* Insert this item if it doesn't already appear in the list.
    */
   if( lpAddrBook->fGroup )
      wsprintf( lpsz = szGroupname, "\"%s\"", ((LPADDRBOOKGROUP)lpAddrBook)->szGroupName );
   else
      lpsz = lpAddrBook->szCixName;
   if( LB_ERR == ( index = ListBox_FindStringExact( hwndList, -1, lpsz ) ) )
      {
      HWND hwndToList;
      HWND hwndCCList;
      HWND hwndBCCList;

      /* Need window handles.
       */
      hwndToList = GetDlgItem( hwnd, IDD_TOLIST );
      hwndCCList = GetDlgItem( hwnd, IDD_CCLIST );
      hwndBCCList = GetDlgItem( hwnd, IDD_BCCLIST );

      /* Remove it from the To list.
       */
      if( LB_ERR != ( index = ListBox_FindStringExact( hwndToList, -1, lpsz ) ) )
         if( !PickEntry_DeleteString( hwndToList, index ) )
            EnableControl( hwnd, IDD_REMOVETO, FALSE );

      /* Remove it from the CC list.
       */
      if( LB_ERR != ( index = ListBox_FindStringExact( hwndCCList, -1, lpsz ) ) )
         if( !PickEntry_DeleteString( hwndCCList, index ) )
            EnableControl( hwnd, IDD_REMOVECC, FALSE );

      /* Remove it from the BCC list.
       */
      if( LB_ERR != ( index = ListBox_FindStringExact( hwndBCCList, -1, lpsz ) ) )
         if( !PickEntry_DeleteString( hwndBCCList, index ) )
            EnableControl( hwnd, IDD_REMOVEBCC, FALSE );

      /* Add it to the destination list
       */
      index = ListBox_AddString( hwndList, lpsz );
      ListBox_SetItemData( hwndList, index, (LPARAM)lpAddrBook );
      }
   ListBox_SetCurSel( hwndList, index );
}

void FASTCALL CmdPrintAddressBook( HWND hwnd )
{
   HPRINT hPr;
   BOOL fOk;
// LPSTR lpAddrPrintBuf;

   /* Show and get the print terminal dialog box.
    */
   if( !Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_PRINTADDRESSBOOK), PrintAddressDlg, 0L ) )
      return;

   GetWindowText( hwnd, lpTmpBuf, LEN_TEMPBUF );
// GetWindowText( hwnd, lpAddrPrintBuf, LEN_TEMPBUF );
   fOk = TRUE;
   if( NULL != ( hPr = BeginPrint( hwnd, lpTmpBuf, &lfFixedFont ) ) )
      {
      register int c;
      int count;
      int index;
      HWND hwndList;
      LPADDRBOOK lpAddrBook;

      VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );

      for( c = 0; fOk && c < cCopies; ++c )
         {

         /* Print the header.
          */
         ResetPageNumber( hPr );
         fOk = BeginPrintPage( hPr );
         wsprintf( lpTmpBuf, "%s   %s   %s", (LPSTR)"Name", (LPSTR)"Address", (LPSTR)"Comment" );
         if( !PrintLine( hPr, lpTmpBuf ) )
            break;
         wsprintf( lpTmpBuf, "%s   %s   %s", (LPSTR)"----", (LPSTR)"-------", (LPSTR)"-------" );
         if( !PrintLine( hPr, lpTmpBuf ) )
            break;
         wsprintf( lpTmpBuf, "%s", (LPSTR)" " );
         if( !PrintLine( hPr, lpTmpBuf ) )
            break;
         count = ListBox_GetCount( hwndList );
         for( index = 0; fOk && index < count; ++index )
         {

            lpAddrBook = (LPADDRBOOK)ListBox_GetItemData( hwndList, index );
            ASSERT( NULL != lpAddrBook );
            if( !lpAddrBook->fGroup )
            {
               wsprintf( lpTmpBuf, "%s, %s, %s", (LPSTR)lpAddrBook->szFullName, (LPSTR)lpAddrBook->szCixName, (LPSTR)lpAddrBook->szComment );
               if( fOk );
                  fOk = PrintLine( hPr, lpTmpBuf );
            }
            else
            {
               if( fPrintGroups )
               {
                  char szName[ LEN_INTERNETNAME+1 ];
                  int index;

                  wsprintf( lpTmpBuf, "**Group: %s", ((LPADDRBOOKGROUP)lpAddrBook)->szGroupName );
                  if( fOk );
                     fOk = PrintLine( hPr, lpTmpBuf );
                  for( index = 0; Amaddr_ExpandGroup( ((LPADDRBOOKGROUP)lpAddrBook)->szGroupName , index, szName ); ++index )
                  {
                  wsprintf( lpTmpBuf, "   %s", szName );
                  if( fOk );
                     fOk = PrintLine( hPr, lpTmpBuf );
                  }
               }
               else
               {
                  wsprintf( lpTmpBuf, "**Group: %s", ((LPADDRBOOKGROUP)lpAddrBook)->szGroupName );
               if( fOk );
                  fOk = PrintLine( hPr, lpTmpBuf );
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
BOOL EXPORT CALLBACK PrintAddressDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = PrintAddressDlg_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the Print Message
 * dialog.
 */
LRESULT FASTCALL PrintAddressDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, PrintAddressDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, PrintAddressDlg_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsPRINTLIST );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL PrintAddressDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndSpin;

   /* Set the input field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_COPIES ), 2 );
   CheckDlgButton( hwnd, IDD_PRINTENTRIES, !fPrintGroups );
   CheckDlgButton( hwnd, IDD_PRINTGROUPS, fPrintGroups );

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
void FASTCALL PrintAddressDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
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
         fPrintGroups = IsDlgButtonChecked( hwnd, IDD_PRINTGROUPS );
         EndDialog( hwnd, TRUE );
         break;

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}
