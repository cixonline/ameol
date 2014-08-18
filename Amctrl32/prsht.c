/* PRSHT.C - Implements property sheets
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
#include "kludge.h"
#include "amctrls.h"
#include "amctrli.h"
#include <string.h>
#include "multimon.h"

#define  MAX_BUTTONS       5
#define  ID_TABCTRL        20
#define  WM_KICKIDLE       WM_USER+9450
#define  PSBTN_BASE        0x3000

#define  THIS_FILE   __FILE__

#define  GWW_EXTRA            4
#define  GetBlock(h)          ((PROPHDR FAR *)GetWindowLong((h),0))
#define  PutBlock(h,b)        (SetWindowLong((h),0,(LONG)(b)))

static int nRetCode;          /* Return code from PropertySheet */

/* Defines one page of a property sheet.
 */
typedef struct tagPROPPAGE {
   struct tagPROPPAGE FAR * pNextPropPage;   /* Pointer to next property page record */
   struct tagPROPPAGE FAR * pPrevPropPage;   /* Pointer to previous property page record */
   HWND hwnd;                                /* Handle of window page */
   LPSTR pszCaption;                         /* Pointer to page caption (Wizards only) */
   AMPROPSHEETPAGE psp;                   /* Copy of the property sheet page */
} PROPPAGE;

/* Defines the header record of a property sheet.
 */
typedef struct tagPROPHDR {
   struct tagPROPPAGE FAR * pFirstPropPage;  /* Pointer to first property page record */
   struct tagPROPPAGE FAR * pLastPropPage;   /* Pointer to last property page record */
   UINT nCurPage;                            /* Index of active page */
   UINT cPages;                              /* Total number of pages */
   int cxButtonGap;                          /* Gap between buttons, in pixels */
   SIZE sizeButton;                          /* Size of the buttons, in pixels */
   SIZE sizeTabMargin;                       /* Size of margin around tab control */
   HFONT hFont;                              /* Handle of font used by property sheet */
   HFONT hButtonFont;                        /* Handle of font used by property sheet buttons */
   HWND hwndTab;                             /* Handle of tab control (not used for Wizards) */
   HWND hwndDefault;                         /* Handle of default button */
   HWND hwndFocus;                           /* Handle of window with the focus */
   int cButtons;                             /* Number of buttons */
   RECT rectLargest;                         /* Dimensions of the largest page */
   HWND rghwndButton[ MAX_BUTTONS ];         /* Array of button IDs */
   AMPROPSHEETHEADER psh;                    /* Copy of the property sheet header */
} PROPHDR;

LRESULT EXPORT CALLBACK PropDlgProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK PropSheetProc( HWND, UINT, WPARAM, LPARAM );

static BOOL FASTCALL PreTranslateMessage( HWND, MSG * );
static BOOL FASTCALL ProcessChars( HWND, MSG * );
static BOOL FASTCALL ProcessTab( HWND, MSG * );
static void FASTCALL RecalcLayout( HWND, PROPHDR FAR * );
static BOOL FASTCALL SetActivePage( HWND, PROPHDR FAR *, UINT );
static BOOL FASTCALL CreateStandardButtons( HWND, PROPHDR FAR * );
static PROPPAGE FAR * FASTCALL GetActivePage( PROPHDR FAR * );
static PROPPAGE FAR * FASTCALL GetPage( PROPHDR FAR *, int );
static BOOL FASTCALL CreatePropertyPages( HWND, PROPHDR FAR * );
static void FASTCALL CenterWindow( HWND );
static LPVOID FASTCALL LockDlgResource( HINSTANCE, LPCSTR );
static void FASTCALL UnlockDlgResource( LPCSTR );
static void FASTCALL GetCaptionFromDialog( LPVOID, char *, int );
static PROPPAGE FAR * FASTCALL AddPropertyPage( HWND, PROPHDR FAR *, LPCAMPROPSHEETPAGE );
static void FASTCALL HelpButton( HWND );
static BOOL FASTCALL CancelButton( HWND );
static BOOL FASTCALL ApplyButton( HWND );
static void FASTCALL ClosePropertySheet( HWND, int );
static void FASTCALL WizardBackButton( HWND );
static void FASTCALL WizardNextButton( HWND );
static BOOL FASTCALL WizardFinishButton( HWND );
static void FASTCALL SetPropertySheetTitle( HWND, HINSTANCE, DWORD, LPCSTR );
static void FASTCALL CreatePropertySheetFont( PROPHDR FAR * );
static void FASTCALL SetDefaultButton( HWND, PROPHDR FAR * );
static HWND FASTCALL FindButton( PROPHDR FAR *, int );
static void FASTCALL CheckDefaultButton( HWND, PROPHDR FAR *, HWND );
static void FASTCALL CheckFocusChange( HWND, PROPHDR FAR * );
static HWND FASTCALL FindNextControl( HWND, PROPHDR FAR *, HWND, char );
static void FASTCALL GotoControl( HWND, PROPHDR FAR *, HWND, char );
static HWND FASTCALL FindControlWithChar( HWND, char );
static BOOL FASTCALL IsCharAfterAmpersand( char *, char );
static void FASTCALL SetCtrlFocus( HWND );
void FASTCALL SetFocusOnFirstControl( PROPHDR FAR *, PROPPAGE FAR * );
static int FASTCALL ResourceIDToPage( PROPHDR FAR *, LPCSTR );

/* This function registers the property sheet dialog window class.
 */
BOOL FASTCALL RegisterPropertySheetClass( HINSTANCE hInst )
{
   WNDCLASS wc;

   wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
   wc.hIcon          = NULL;
   wc.lpszMenuName   = NULL;
   wc.lpszClassName  = WC_PROPSHEET;
   wc.hbrBackground  = (HBRUSH)( COLOR_BTNFACE + 1 );
   wc.hInstance      = hInst;
   wc.style          = CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS;
   wc.lpfnWndProc    = PropDlgProc;
   wc.cbClsExtra     = 0;
   wc.cbWndExtra     = GWW_EXTRA;
   return( RegisterClass( &wc ) );
}

/* This function displays and handles interaction with the property sheet.
 * The lppsph parameter points to a property sheet header record that defines the
 * layout and attributes for the entire property sheet, and points to individual
 * records for each of the property pages.
 */
WINCOMMCTRLAPI int WINAPI EXPORT Amctl_PropertySheet( LPCAMPROPSHEETHEADER lppsph )
{
   HWND hwndDialog;
   DWORD dwExStyle;
   DWORD dwStyle;
   BOOL fCancel;
   BOOL fShown;
   static MSG msg;

   /* Create the property sheet window. The window must be a popup with both
    * a system menu and a caption to make it resemble a dialog box.
    */
   dwStyle = WS_POPUPWINDOW|WS_CAPTION;
   dwExStyle = WS_EX_DLGMODALFRAME|
            WS_EX_TOPMOST;                                                  //!!SM!! 2045
   hwndDialog = CreateWindowEx( dwExStyle, WC_PROPSHEET, "", dwStyle, 0, 0, 400, 200, lppsph->hwndParent, (HMENU)0, hLibInst, (LPVOID)lppsph );
   if( !hwndDialog )
      return( -2 );

   /* Because our property sheet dialogs are modal, disable the parent
    * window.
    */
   if( NULL != lppsph->hwndParent && IsWindowEnabled( lppsph->hwndParent ) )
      EnableWindow( lppsph->hwndParent, FALSE );
   SetWindowPos(hwndDialog, HWND_NOTOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE);  //!!SM!! 2045
   SetActiveWindow( hwndDialog );

   /* Here, we spin a message loop dispatching messages around the system
    * and those for the property sheet dialog until the dialog is closed or
    * a system exit message is posted.
    */
   fCancel = FALSE;
   fShown = ( GetWindowStyle( hwndDialog ) & WS_VISIBLE ) != 0;
   nRetCode = -1;
   while( !fCancel && IsWindow( hwndDialog ) )
      {
      /* If no messages are queued and the parent window wants idle messages, then
       * send idle messages to the parent window. This keeps any background tasks
       * spinning while the property sheet dialog is waiting for input.
       */
      if( !PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE ) )
         {
         if( !( GetWindowStyle( lppsph->hwndParent ) & DS_NOIDLEMSG ) )
            SendMessage( lppsph->hwndParent, WM_ENTERIDLE, MSGF_DIALOGBOX, (LPARAM)(UINT)hwndDialog );
         if( !fShown )
            {
            /* Display of the dialog (assuming it wasn't created with the WS_VISIBLE style)
             * is deferred until all creation messages have been processed.
             */
            ShowWindow( hwndDialog, SW_SHOW );
            fShown = TRUE;
            }
         }

      /* A message has been received. If it's the WM_QUIT message, we post it back again so
       * that the application main message handler will see it, then we shut down the property
       * sheet dialog as fast as we can. Otherwise process the message as normal.
       */
      do {
         if( !GetMessage( &msg, NULL, 0, 0 ) )
            {
            PostQuitMessage( (int)msg.wParam );
            fCancel = TRUE;
            }
         if( !CallMsgFilter( &msg, MSGF_DIALOGBOX ) )
            if( !PreTranslateMessage( hwndDialog, &msg ) )
               {
               TranslateMessage( &msg );
               DispatchMessage( &msg );
               }
         }
      while( !fCancel && PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE ) );
      } 
   return( nRetCode );
}

static BOOL FASTCALL PreTranslateMessage( HWND hwnd, MSG * pmsg )
{
   BOOL fResult;

   fResult = FALSE;
   if( IsWindow( hwnd ) )
      {
      if( pmsg->message != WM_KICKIDLE )
         {
         MSG msg;
   
         PeekMessage( &msg, NULL, WM_KICKIDLE, WM_KICKIDLE, PM_REMOVE );
         PostMessage( hwnd, WM_KICKIDLE, 0, 0L );
         }
      if( ProcessChars( hwnd, pmsg ) )
         fResult = TRUE;
      else if( ProcessTab( hwnd, pmsg ) )
         fResult = TRUE;
      else
         {
         PROPHDR FAR * pPropHdr;
         PROPPAGE FAR * pPropPage;
   
         pPropHdr = GetBlock( hwnd );
         if( pPropHdr )
            {
            pPropPage = GetActivePage( pPropHdr );
            if( pPropPage )
               if( (IsChild( pPropPage->hwnd, pmsg->hwnd ) && IsDialogMessage( pPropPage->hwnd, pmsg )) || IsDialogMessage( hwnd, pmsg ) )
                  fResult = TRUE;
            }
         }
      if( WM_KICKIDLE == pmsg->message )
         {
         PROPHDR FAR * pPropHdr;
   
         pPropHdr = GetBlock( hwnd );
         if( pPropHdr )
            CheckFocusChange( hwnd, pPropHdr );
         }
      }
   return( fResult );
}

static void FASTCALL CheckFocusChange( HWND hwnd, PROPHDR FAR * pPropHdr )
{
   HWND hWndFocus;

   hWndFocus = GetFocus();
   if( hWndFocus != pPropHdr->hwndFocus && hWndFocus )
      {
      CheckDefaultButton( hwnd, pPropHdr, hWndFocus );               
      pPropHdr->hwndFocus = hWndFocus;
      }
}

static void FASTCALL CheckDefaultButton( HWND hwnd, PROPHDR FAR * pPropHdr, HWND hFocusAfter )
{
   HWND hOldDefault = NULL;
   HWND hwndDefault = NULL;
   DWORD dwOldDefault = 0;
   DWORD dwDefault = 0;

   if( pPropHdr->hwndFocus && IsChild( hwnd, pPropHdr->hwndFocus ) )
      {
      hOldDefault = pPropHdr->hwndFocus;
      if( hOldDefault != NULL )
         dwOldDefault = SendMessage( hOldDefault, WM_GETDLGCODE, 0, 0L );
      if( !( dwOldDefault & (DLGC_DEFPUSHBUTTON|DLGC_UNDEFPUSHBUTTON ) ) )
         {
         hOldDefault = GetDlgItem( hwnd, PSBTN_OK + PSBTN_BASE );
         dwOldDefault = SendMessage( hOldDefault, WM_GETDLGCODE, 0, 0L );
         }
      }

   if( IsChild( hwnd, hFocusAfter ) )
      {
      hwndDefault = hFocusAfter;
      if( hwndDefault != NULL )
         dwDefault = SendMessage( hwndDefault, WM_GETDLGCODE, 0, 0L );
      if( !( dwDefault & (DLGC_DEFPUSHBUTTON|DLGC_UNDEFPUSHBUTTON ) ) )
         {
         hwndDefault = GetDlgItem( hwnd, PSBTN_OK + PSBTN_BASE );
         dwDefault = SendMessage( hwndDefault, WM_GETDLGCODE, 0, 0L );
         }
      }

   if( hOldDefault != hwndDefault && ( dwOldDefault & DLGC_DEFPUSHBUTTON ) )
      SendMessage( hOldDefault, BM_SETSTYLE, (WPARAM)BS_PUSHBUTTON, TRUE );

   if( dwDefault & DLGC_UNDEFPUSHBUTTON )
      SendMessage( hwndDefault, BM_SETSTYLE, (WPARAM)BS_DEFPUSHBUTTON, TRUE );

   pPropHdr->hwndDefault = ( hwndDefault == hFocusAfter ) ? hFocusAfter : NULL;
}

/* This function processes WM_CHAR messages for the property sheet.
 */
static BOOL FASTCALL ProcessChars( HWND hwnd, MSG * pmsg )
{
   PROPHDR FAR * pPropHdr;
   PROPPAGE FAR * pPropPage;
   DWORD dwDefId;

   /* If no property sheet, do nothing.
    */
   pPropHdr = GetBlock( hwnd );
   if( !pPropHdr )
      return( FALSE );

   /* If no active page, do nothing.
    */
   pPropPage = GetActivePage( pPropHdr );
   if( NULL == pPropPage )
      return( FALSE );

   /* If there is no active window for this
    * message, do nothing.
    */
   if( NULL == pmsg->hwnd )
      return( FALSE );
   switch( pmsg->message )
      {
      case WM_SYSCHAR:
         if( GetFocus() == NULL && GetKeyState( VK_MENU ) > 0 )
            break;
         goto CH1;
   
      case WM_CHAR: {
         HWND hwndCtl;

         if( SendMessage( pmsg->hwnd, WM_GETDLGCODE, 0, 0L ) & (DLGC_WANTALLKEYS|DLGC_WANTMESSAGE|DLGC_WANTCHARS) )
            return( FALSE );
CH1:     hwndCtl = FindNextControl( hwnd, pPropHdr, pmsg->hwnd, (char)pmsg->wParam );
         if( !hwndCtl )
            return( FALSE );
         if( CallMsgFilter( pmsg, MSGF_DIALOGBOX ) )
            return( TRUE );
         GotoControl( hwnd, pPropHdr, hwndCtl, (char)pmsg->wParam );
         return( TRUE );
         }

      case WM_KEYDOWN:
         switch( pmsg->wParam )
            {
            case VK_ESCAPE:
               /* If the user pressed the escape key and the control doesn't want
                * to swallow this key itself, then translate it to a PSBTN_CANCEL
                * command.
                */
               if( 0 == ( SendMessage( pmsg->hwnd, WM_GETDLGCODE, 0, 0L ) & (DLGC_WANTALLKEYS|DLGC_WANTMESSAGE) ) )
                  {
                  PostMessage( hwnd, WM_COMMAND, (WPARAM)PSBTN_CANCEL + PSBTN_BASE, 0L );
                  return( TRUE );
                  }
               break;

            case VK_RETURN:
               if( 0 == ( SendMessage( pmsg->hwnd, WM_GETDLGCODE, 0, 0L ) & (DLGC_WANTALLKEYS|DLGC_WANTMESSAGE) ) )
                  {
                  dwDefId = SendMessage( pPropPage->hwnd, DM_GETDEFID, 0, 0L );
                  if( DC_HASDEFID == HIWORD( dwDefId ) &&
                      NULL != GetDlgItem( pPropPage->hwnd, LOWORD( dwDefId ) ) &&
                      IsWindowEnabled( GetDlgItem( pPropPage->hwnd, LOWORD( dwDefId ) ) ) )
                     {
                     PostMessage( pPropPage->hwnd, WM_COMMAND, (WPARAM)LOWORD( dwDefId ), 0L );
                     return( TRUE );
                     }
                  dwDefId = SendMessage( hwnd, DM_GETDEFID, 0, 0L );
                  if( DC_HASDEFID == HIWORD( dwDefId ) &&
                      NULL != GetDlgItem( hwnd, LOWORD( dwDefId ) ) &&
                      IsWindowEnabled( GetDlgItem( hwnd, LOWORD( dwDefId ) ) ) )
                     {
                     PostMessage( hwnd, WM_COMMAND, (WPARAM)LOWORD( dwDefId ), 0L );
                     return( TRUE );
                     }
                  }
               break;
            }
         }
   return( FALSE );
}

/* This function locates the next window control with the
 * specified character in it's short-cut string.
 */
static HWND FASTCALL FindNextControl( HWND hwnd, PROPHDR FAR * pPropHdr, HWND hwndCtl, char ch )
{
   PROPPAGE FAR * pPropPage;
   HWND hwndFoundCtl;
   HWND hwndFocusPage;

   /* Are we on the property page? If so, scan the page
    * for the next control otherwise scan the sheet. If one
    * scan fails, switch to the other.
    */
   pPropPage = GetActivePage( pPropHdr );
   hwndFocusPage = pPropPage->hwnd;

   /* Now try and find the next control with ch in it's short
    * cut key sequence.
    */
   hwndFoundCtl = FindControlWithChar( hwndCtl, ch );
   if( NULL == hwndFoundCtl )
      {
      if( IsChild( pPropPage->hwnd, hwndCtl ) )
         hwndFocusPage = hwnd;
      else
         hwndFocusPage = pPropPage->hwnd;
      hwndCtl = GetWindow( hwndFocusPage, GW_CHILD );
      hwndFoundCtl = FindControlWithChar( hwndCtl, ch );
      }
   return( hwndFoundCtl );
}

/* This function scans all the child controls of the specified hwnd, starting
 * from hwndCtl, looking for the one whose short-cut key matches the specified
 * character in ch.
 */
static HWND FASTCALL FindControlWithChar( HWND hwndCtl, char ch )
{
   HWND hwndStart;
   HWND hwndParent;

   ASSERT( IsWindow( hwndCtl ) );
   hwndStart = hwndCtl;
   hwndParent = GetParent( hwndCtl );
   for(;;)
      {
      DWORD dwDlgCode;

      /* Start from the next control. If we've reached the
       * last child control, start from the first one.
       */
      hwndCtl = GetWindow( hwndCtl, GW_HWNDNEXT );
      if( NULL == hwndCtl )
         hwndCtl = GetWindow( hwndParent, GW_CHILD );
      ASSERT( IsWindow( hwndCtl ) );

      /* Don't look at the string for this control if it processes all
       * characters itself or it is a static and it has no prefix (and thus no
       * short-cut key).
       */
      dwDlgCode = (DWORD)SendMessage( hwndCtl, WM_GETDLGCODE, 0, 0L );
      if( !( dwDlgCode & DLGC_WANTCHARS ) && ( !( dwDlgCode & DLGC_STATIC ) || !( GetWindowLong( hwndCtl, GWL_STYLE ) & SS_NOPREFIX ) ) )
         {
         static char szText[ 64 ];

         GetWindowText( hwndCtl, szText, sizeof(szText) );
         if( IsCharAfterAmpersand( szText, ch ) )
            break;
         }
      if( hwndCtl == hwndStart )
         return( NULL );
      }
   return( hwndCtl );
}

/* This simple function checks whether the specified character appears
 * after an ampersand (&) in the given string. Two consecutive ampersands
 * are treated as a single ampersand and are ignored.
 */
static BOOL FASTCALL IsCharAfterAmpersand( char * psz, char chFind )
{
   AnsiLowerBuff( &chFind, 1 );
   while( *psz )
      {
      if( *psz == '&' )
         {
         ++psz;
         if( *psz != '&' )
            {
            char ch;

            ch = *psz;
            AnsiLowerBuff( &ch, 1 );
            return( ch == chFind );
            }
         }
      ++psz;
      }
   return( FALSE );
}

/* This function sets the focus at the specified control. It first
 * checks that the control is not disabled and that it is not a static.
 * In either of these cases, it skips to the next valid control.
 */
static void FASTCALL GotoControl( HWND hwnd, PROPHDR FAR * pPropHdr, HWND hwndCtl, char ch )
{
   HWND hwndCtlLast;
   HWND hwndFirst;

   hwndCtlLast = NULL;
   for( hwndFirst = NULL; hwndFirst != hwndCtl; hwndCtl = FindControlWithChar( hwndCtl, ch ) )
      {
      WORD code;

      /* If same as last control, exit now.
       */
      if( hwndCtlLast == hwndCtl )
         break;
      hwndCtlLast = hwndCtl;

      /* Remember the first control after the first compare
       * in the loop.
       */
      if( hwndFirst == NULL )
         hwndFirst = hwndCtl;

      /* Is this a static control (ie. a text label)? If so then skip
       * to the next tabbed control with which this is associated.
       */
      code = (WORD)SendMessage( hwndCtl, WM_GETDLGCODE, 0, 0L );
      if( ( code & DLGC_STATIC ) && IsWindowEnabled( hwndCtl ) )
         {
         PROPPAGE FAR * pPropPage;

         pPropPage = GetActivePage( pPropHdr );
         if( IsChild( pPropPage->hwnd, hwndCtl ) )
            hwndCtl = GetNextDlgTabItem( pPropPage->hwnd, hwndCtl, FALSE );
         else
            hwndCtl = GetNextDlgTabItem( hwnd, hwndCtl, FALSE );
         code = (WORD)SendMessage( hwndCtl, WM_GETDLGCODE, 0, 0L );
         }

      /* Ignore disabled controls.
       */
      if( IsWindowEnabled( hwndCtl ) )
         {
         /* This isn't a button, so just pop the focus there.
          */
         if( !( code & DLGC_BUTTON ) )
            SetCtrlFocus( pPropHdr->hwndFocus = hwndCtl );
         else
            {
            /* This is a push button, so flash the control to simulate it
             * being pressed, then fire a WM_COMMAND at the parent.
             */
            if( ( code & DLGC_DEFPUSHBUTTON ) || ( code & DLGC_UNDEFPUSHBUTTON ) )
               {
               SendMessage( hwndCtl, BM_SETSTATE, TRUE, 0L );
               SendMessage( hwndCtl, BM_SETSTATE, FALSE, 0L );
               SendDlgCommand( GetParent( hwndCtl ), GetDlgCtrlID( hwndCtl ), BN_CLICKED );
               }
            else
               {
               /* For any other control, just set the focus there and only
                * activate it if there are no other controls with the same
                * short-cut key.
                */
               SetFocus( pPropHdr->hwndFocus = hwndCtl );
               if( FindNextControl( hwnd, pPropHdr, hwndCtl, ch ) == hwndCtl )
                  {
                  SendMessage( hwndCtl, WM_LBUTTONDOWN, 0, 0L );
                  SendMessage( hwndCtl, WM_LBUTTONUP, 0, 0L );
                  SendDlgCommand( GetParent( hwndCtl ), GetDlgCtrlID( hwndCtl ), BN_CLICKED );
                  }
               }
            }
         break;
         }
      }
}

/* This function sets the focus to the specified control. If the
 * control is an edit control with the DLGC_HASSETSEL attribute,
 * highlight the entire text in the edit control.
 */
static void FASTCALL SetCtrlFocus( HWND hwnd )
{
   if( SendMessage( hwnd, WM_GETDLGCODE, 0, 0L) & DLGC_HASSETSEL )
      SendMessage( hwnd, EM_SETSEL, 0, -1 );
   SetFocus( hwnd );
}

static BOOL FASTCALL ProcessTab( HWND hwnd, MSG * pmsg )
{
   if( WM_KEYDOWN == pmsg->message && VK_TAB == pmsg->wParam )
      {
      PROPHDR FAR * pPropHdr;
      PROPPAGE FAR * pPropPage;
      HWND hwndFocus;

      hwndFocus = GetFocus();
      pPropHdr = GetBlock( hwnd );
      if( !pPropHdr )
         return( FALSE );
      pPropPage = GetActivePage( pPropHdr );
      if( 0 == ( SendMessage( hwndFocus, WM_GETDLGCODE, 0, 0L ) & (DLGC_WANTALLKEYS|DLGC_WANTMESSAGE|DLGC_WANTTAB ) ) )
         {
         BOOL fShift;

         fShift = GetKeyState( VK_SHIFT ) < 0;
         if( GetKeyState( VK_CONTROL ) < 0 )
            {
            if( pPropHdr->hwndTab )
               {
               int index;

               index = TabCtrl_GetCurSel( pPropHdr->hwndTab );
               do {
                  if( fShift )
                     index = ( index ? index : TabCtrl_GetItemCount( pPropHdr->hwndTab ) ) - 1;
                  else
                     index = ( index < TabCtrl_GetItemCount( pPropHdr->hwndTab ) - 1 ) ? index + 1 : 0;
                  }
               while( !TabCtrl_GetEnable( pPropHdr->hwndTab, index ) );
               TabCtrl_SetCurSel( pPropHdr->hwndTab, index );
               return( TRUE );
               }
            return( FALSE );
            }
         else if( fShift )
            {
            HWND hwndLast;

            hwndLast = GetWindow( pPropPage->hwnd, GW_CHILD );
            hwndLast = GetNextDlgTabItem( pPropPage->hwnd, hwndLast, TRUE );
            if( IsChild( pPropPage->hwnd, pmsg->hwnd ) && GetNextDlgTabItem( pPropPage->hwnd, pmsg->hwnd, TRUE ) == hwndLast )
               {
               if( pPropHdr->hwndTab )
                  SetFocus( pPropHdr->hwndTab );
               else
                  {
                  HWND hwndCtrl;

                  hwndCtrl = pPropHdr->rghwndButton[ pPropHdr->cButtons - 1 ];
                  SetFocus( pPropHdr->hwndFocus = hwndCtrl );
                  }
               return( TRUE );
               }
            else if( pmsg->hwnd == pPropHdr->rghwndButton[ 0 ] )
               {
               SetFocus( pPropHdr->hwndFocus = hwndLast );
               return( TRUE );
               }
            }
         else
            {
            HWND hwndFirst;

            hwndFirst = GetWindow( pPropPage->hwnd, GW_CHILD );
            if( !( GetWindowStyle( hwndFirst ) & WS_TABSTOP ) )
               hwndFirst = GetNextDlgTabItem( pPropPage->hwnd, hwndFirst, FALSE );
            if( IsChild( pPropPage->hwnd, pmsg->hwnd ) && GetNextDlgTabItem( pPropPage->hwnd, pmsg->hwnd, FALSE ) == hwndFirst )
               {
               HWND hwndCtrl;

               hwndCtrl = pPropHdr->rghwndButton[ 0 ];
               SetFocus( pPropHdr->hwndFocus = hwndCtrl );
               return( TRUE );
               }
            else if( pPropHdr->hwndTab && pmsg->hwnd == pPropHdr->hwndTab )
               {
               SetFocus( pPropHdr->hwndFocus = hwndFirst );
               return( TRUE );
               }
            }
         }
      }
   return( FALSE );
}

LRESULT EXPORT CALLBACK PropDlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   PROPHDR FAR * pPropHdr;
   PROPPAGE FAR * pPropPage;

#ifdef _DEBUG
   /* Help us to catch NULL pointers...
    */
   pPropHdr = NULL;
   pPropPage = NULL;
#endif
   switch( message )
      {
      case WM_CREATE: {
         static PROPHDR FAR * pNewPropHdr;
         LPCREATESTRUCT lpCreateStruct;
         LPCAMPROPSHEETHEADER lppsph;
         DWORD dwTabStyle;
         BOOL fWizard;
         HMENU hSysMenu;
         RECT rect;
         int nCount, i;

         lpCreateStruct = (LPCREATESTRUCT)lParam;
         lppsph = (LPCAMPROPSHEETHEADER)lpCreateStruct->lpCreateParams;

         INITIALISE_PTR(pNewPropHdr);
         if( !fNewMemory( &pNewPropHdr, sizeof( PROPHDR ) ) )
            return( -1 );
         pNewPropHdr->psh = *lppsph;
         pNewPropHdr->hwndTab = NULL;
         pNewPropHdr->hwndDefault = NULL;
         pNewPropHdr->hwndFocus = NULL;
         pNewPropHdr->cButtons = 0;
         pNewPropHdr->cPages = 0;
         pNewPropHdr->pFirstPropPage = NULL;
         pNewPropHdr->pLastPropPage = NULL;
         SetRectEmpty( &pNewPropHdr->rectLargest );
         PutBlock( hwnd, pNewPropHdr );
         pNewPropHdr->nCurPage = (UINT)-1;

         fWizard = ( pNewPropHdr->psh.dwFlags & PSH_WIZARD ) != 0;

         hSysMenu = GetSystemMenu( hwnd, FALSE );
         nCount = GetMenuItemCount( hSysMenu );
         for( i = 0; i < nCount; ++i )
            {
            UINT nID;

            nID = GetMenuItemID( hSysMenu, i );
            if( nID != SC_MOVE && nID != SC_CLOSE )
               {
               DeleteMenu( hSysMenu, i, MF_BYPOSITION );
               i--;
               nCount--;
               }
            }

         SetPropertySheetTitle( hwnd, pNewPropHdr->psh.hInstance, pNewPropHdr->psh.dwFlags, pNewPropHdr->psh.pszCaption );
         CreatePropertySheetFont( pNewPropHdr );

         if( !fWizard )
            {
            if( pNewPropHdr->psh.dwFlags & PSH_MULTILINETABS )
               dwTabStyle = TCS_MULTILINE|TCS_FIXEDWIDTH;
            else
               dwTabStyle = TCS_RAGGEDRIGHT;
            dwTabStyle |= WS_CHILD|WS_VISIBLE|WS_GROUP|WS_TABSTOP;
            pNewPropHdr->hwndTab = CreateWindow( WC_TABCONTROL, "", dwTabStyle, 0, 0, 0, 0, hwnd, (HMENU)ID_TABCTRL, hLibInst, 0L );
            if( !pNewPropHdr->hwndTab )
               return( -1 );
            SetWindowFont( pNewPropHdr->hwndTab, pNewPropHdr->hFont, FALSE );
            }

         if( !CreatePropertyPages( hwnd, pNewPropHdr ) )
            return( -1 );
         if( !CreateStandardButtons( hwnd, pNewPropHdr ) )
            return( -1 );

         if( pNewPropHdr->psh.dwFlags & PSH_USECALLBACK )
            (*pNewPropHdr->psh.pfnCallback)( hwnd, PSCB_INITIALIZED, 0L );

         /* If no pages, fail
          */
         if( 0 == pNewPropHdr->cPages )
            return( -1 );

         if( pNewPropHdr->psh.U2(nStartPage) >= pNewPropHdr->cPages )
            pNewPropHdr->psh.U2(nStartPage) = pNewPropHdr->cPages - 1;
         SetActivePage( hwnd, pNewPropHdr, pNewPropHdr->psh.U2(nStartPage) );
         pPropPage = GetActivePage( pNewPropHdr );

         SetRect( &rect, 3, 0, 43, 14 );
         MapDialogRect( pPropPage->hwnd, &rect );
         pNewPropHdr->sizeButton.cx = rect.right;
         pNewPropHdr->sizeButton.cy = rect.bottom;
         pNewPropHdr->cxButtonGap = rect.left;

         rect.bottom = rect.right = 4;
         MapDialogRect( pPropPage->hwnd, &rect );
         pNewPropHdr->sizeTabMargin.cx = rect.right;
         pNewPropHdr->sizeTabMargin.cy = rect.bottom;

         RecalcLayout( hwnd, pNewPropHdr );
         if( !fWizard )
            {
            HWND hwndApply;
   
            TabCtrl_SetCurSel( pNewPropHdr->hwndTab, pNewPropHdr->nCurPage );
            if( hwndApply = GetDlgItem( hwnd, PSBTN_APPLYNOW + PSBTN_BASE ) )
               EnableWindow( hwndApply, FALSE );
            }
         return( 0 );
         }

      case WM_SETFOCUS:
         pPropHdr = GetBlock( hwnd );
         if( NULL != pPropHdr->hwndFocus )
            SetFocus( pPropHdr->hwndFocus );
         return( 0L );

      case WM_PAINT:
         pPropHdr = GetBlock( hwnd );
         if( pPropHdr->psh.dwFlags & PSH_WIZARD )
            {
            PAINTSTRUCT ps;
            RECT rc;
            HDC hdc;
            int y;

            hdc = BeginPaint( hwnd, &ps );
            GetClientRect( hwnd, &rc );
            y = rc.bottom - ( pPropHdr->sizeButton.cy + ( 2 * pPropHdr->sizeTabMargin.cy ) );
            SelectPen( hdc, GetStockPen( BLACK_PEN ) );
            MoveToEx( hdc, rc.left + pPropHdr->sizeTabMargin.cx, y, NULL );
            LineTo( hdc, rc.right - pPropHdr->sizeTabMargin.cx, y );
            SelectPen( hdc, GetStockPen( WHITE_PEN ) );
            MoveToEx( hdc, rc.left + pPropHdr->sizeTabMargin.cx, y + 1, NULL );
            LineTo( hdc, rc.right - pPropHdr->sizeTabMargin.cx, y + 1 );
            EndPaint( hwnd, &ps );
            }
         break;

      case DM_GETDEFID:
         pPropHdr = GetBlock( hwnd );
         if( pPropHdr->psh.dwFlags & PSH_WIZARD )
            if( pPropHdr->nCurPage == pPropHdr->cPages - 1 )
               return( MAKELPARAM( PSBTN_FINISH + PSBTN_BASE, DC_HASDEFID ) );
            else
               return( MAKELPARAM( PSBTN_NEXT + PSBTN_BASE, DC_HASDEFID ) );
         return( MAKELPARAM( PSBTN_OK + PSBTN_BASE, DC_HASDEFID ) );

      case PSM_ADDPAGE: {
         LPCAMPROPSHEETPAGE ppsp;
         PROPPAGE FAR * pNewPage;

         pPropHdr = GetBlock( hwnd );
         ppsp = (LPCAMPROPSHEETPAGE)lParam;
         if( ( pNewPage = AddPropertyPage( hwnd, pPropHdr, ppsp ) ) != NULL )
            {
            RECT rc;

            if( -1 == pPropHdr->nCurPage )
               pPropHdr->nCurPage = 0;
            GetClientRect( pNewPage->hwnd, &rc );
            if( rc.right > pPropHdr->rectLargest.right )
               pPropHdr->rectLargest.right = rc.right;
            if( rc.bottom > pPropHdr->rectLargest.bottom )
               pPropHdr->rectLargest.bottom = rc.bottom;
            RecalcLayout( hwnd, pPropHdr );
            InvalidateRect( hwnd, NULL, TRUE );
            UpdateWindow( hwnd );
            }
         return( pPropHdr->cPages - 1 );
         }

      case PSM_APPLY:
         return( ApplyButton( hwnd ) );

      case PSM_CANCELTOCLOSE: {
         HWND hwndCancel;

         if( hwndCancel = GetDlgItem( hwnd, PSBTN_CANCEL + PSBTN_BASE ) )
            SetWindowText( hwndCancel, "Close" );
         return( 0L );
         }

      case PSM_CHANGED: {
         HWND hwndApply;

         if( hwndApply = GetDlgItem( hwnd, PSBTN_APPLYNOW + PSBTN_BASE ) )
            EnableWindow( hwndApply, TRUE );
         return( 0L );
         }

      case PSM_GETTABCONTROL:
         pPropHdr = GetBlock( hwnd );
         return( (LRESULT)(LPSTR)pPropHdr->hwndTab );

      case PSM_QUERYSIBLINGS:
         pPropHdr = GetBlock( hwnd );
         pPropPage = pPropHdr->pFirstPropPage;
         while( pPropPage )
            {
            LRESULT r;

            if( ( r = SendMessage( pPropPage->hwnd, PSM_QUERYSIBLINGS, wParam, lParam ) ) != 0 )
               return( r );
            pPropPage = pPropPage->pNextPropPage;
            }
         return( 0L );

      case PSM_SETCURSEL:
         pPropHdr = GetBlock( hwnd );
         if( wParam >= pPropHdr->cPages )
            wParam = pPropHdr->cPages - 1;
         if( wParam != pPropHdr->nCurPage )
            TabCtrl_SetCurSel( pPropHdr->hwndTab, (int)wParam );
         break;

      case PSM_SETFINISHTEXT: {
         HWND hwndFinish;

         if( hwndFinish = GetDlgItem( hwnd, PSBTN_FINISH + PSBTN_BASE ) )
            {
            SetWindowText( hwndFinish, (LPSTR)lParam );
            ShowWindow( hwndFinish, SW_SHOW );
            EnableWindow( hwndFinish, TRUE );
            ShowWindow( GetDlgItem( hwnd, PSBTN_NEXT + PSBTN_BASE ), SW_HIDE );
            ShowWindow( GetDlgItem( hwnd, PSBTN_BACK + PSBTN_BASE ), SW_HIDE );
            }
         return( 0L );
         }

      case PSM_SETTITLE:
         pPropHdr = GetBlock( hwnd );
         SetPropertySheetTitle( hwnd, pPropHdr->psh.hInstance, (DWORD)wParam, (LPCSTR)lParam );
         return( 0L );

      case PSM_SETWIZBUTTONS: {
         DWORD dwFlags = lParam;

         EnableWindow( GetDlgItem( hwnd, PSBTN_BACK + PSBTN_BASE ), ( dwFlags & PSWIZB_BACK ) != 0 );
         EnableWindow( GetDlgItem( hwnd, PSBTN_NEXT + PSBTN_BASE ), ( dwFlags & PSWIZB_NEXT ) != 0 );
         EnableWindow( GetDlgItem( hwnd, PSBTN_FINISH + PSBTN_BASE ), ( dwFlags & PSWIZB_FINISH ) != 0 );
         return( 0L );
         }

      case PSM_UNCHANGED: {
         HWND hwndApply;

         if( hwndApply = GetDlgItem( hwnd, PSBTN_APPLYNOW + PSBTN_BASE ) )
            {
            if( GetFocus() == hwndApply )
               SetFocus( GetDlgItem( hwnd, PSBTN_CANCEL + PSBTN_BASE ) );
            EnableWindow( hwndApply, FALSE );
            }
         return( 0L );
         }

      case PSM_PRESSBUTTON:
         wParam += PSBTN_BASE;

      case WM_COMMAND:
         switch( wParam )
            {
            case PSBTN_OK + PSBTN_BASE:
               if( ApplyButton( hwnd ) )
                  {
                  ClosePropertySheet( hwnd, 0 );
                  return( 0L );
                  }
               break;

            case PSBTN_CANCEL + PSBTN_BASE:
               if( CancelButton( hwnd ) )
                  ClosePropertySheet( hwnd, -1 );
               return( 0L );

            case PSBTN_APPLYNOW + PSBTN_BASE:
               ApplyButton( hwnd );
               break;

            case PSBTN_HELP + PSBTN_BASE:
               HelpButton( hwnd );
               break;

            case PSBTN_BACK + PSBTN_BASE:
               WizardBackButton( hwnd );
               break;

            case PSBTN_NEXT + PSBTN_BASE:
               WizardNextButton( hwnd );
               break;

            case PSBTN_FINISH + PSBTN_BASE:
               if( !WizardFinishButton( hwnd ) )
                  {
                  ClosePropertySheet( hwnd, 0 );
                  return( 0L );
                  }
               break;
            }
         break;

      case WM_NOTIFY: {
         LPNMHDR lpNmHdr;

         lpNmHdr = (LPNMHDR)lParam;
         switch( lpNmHdr->code )
            {
            case TCN_SELCHANGE:
               {
               int nCurPage;
         
               pPropHdr = GetBlock( hwnd );
               nCurPage = TabCtrl_GetCurSel( lpNmHdr->hwndFrom );
               SetActivePage( hwnd, pPropHdr, nCurPage );
               return( 0 );
               }

            case TCN_SELCHANGING:
               {
               int nCurPage;
               NMHDR nmh;

               pPropHdr = GetBlock( hwnd );
               nCurPage = TabCtrl_GetCurSel( lpNmHdr->hwndFrom );
               pPropPage = GetPage( pPropHdr, nCurPage );

               if( Amctl_SendNotify( pPropPage->hwnd, hwnd, PSN_KILLACTIVE, (LPNMHDR)&nmh ) )
                  return( TRUE );
               if( pPropPage->hwnd )
                  ShowWindow( pPropPage->hwnd, SW_HIDE );
               return( 0 );
               }
            }
         return( FALSE );
         }

      case WM_DESTROY:
         pPropHdr = GetBlock( hwnd );
         FreeMemory( &pPropHdr );
         return( 0L );

      case WM_CLOSE:
         ClosePropertySheet( hwnd, -1 );
         return( 0L );
      }
   return( DefWindowProc( hwnd, message, wParam, lParam ) );
}

static void FASTCALL SetDefaultButton( HWND hwnd, PROPHDR FAR * pPropHdr )
{
   PROPPAGE FAR * pPropPage;
   DWORD dwDefIDInfo;
   HWND hwndDefault;
   BOOL fDefID = FALSE;

   pPropPage = GetActivePage( pPropHdr );
   dwDefIDInfo = SendMessage( pPropPage->hwnd, DM_GETDEFID, 0, 0L );
   hwndDefault = NULL;
   if( DC_HASDEFID == HIWORD( dwDefIDInfo ) )
      if( NULL != ( hwndDefault = GetDlgItem( pPropPage->hwnd, LOWORD( dwDefIDInfo ) ) ) )
         if( SendMessage( hwndDefault, WM_GETDLGCODE, 0, 0L ) & DLGC_DEFPUSHBUTTON )
            {
            hwndDefault = NULL;
            fDefID = TRUE;
            }

   if( !fDefID )
      {
      if( pPropHdr->psh.dwFlags & PSH_WIZARD )
         {
         hwndDefault = FindButton( pPropHdr, PSBTN_NEXT + PSBTN_BASE );
         if( !( hwndDefault && IsWindowEnabled( hwndDefault ) ) )
            hwndDefault = FindButton( pPropHdr, PSBTN_FINISH + PSBTN_BASE );
         }
      else
         hwndDefault = FindButton( pPropHdr, PSBTN_OK + PSBTN_BASE );
      }

   if( pPropHdr->hwndDefault != hwndDefault )
      {
      if( pPropHdr->hwndDefault )
         SendMessage( pPropHdr->hwndDefault, BM_SETSTYLE, (WPARAM)BS_PUSHBUTTON, TRUE );
      pPropHdr->hwndDefault = hwndDefault;
      if( hwndDefault && IsWindowEnabled( hwndDefault ) )
         {
         SendMessage( hwndDefault, BM_SETSTYLE, (WPARAM)BS_DEFPUSHBUTTON, TRUE );
         SetFocus( pPropHdr->hwndFocus = hwndDefault );
         }
      }
}

static HWND FASTCALL FindButton( PROPHDR FAR * pPropHdr, int wID )
{
   register int c;

   for( c = 0; c < pPropHdr->cButtons; ++c )
      if( GetDlgCtrlID( pPropHdr->rghwndButton[ c ] ) == wID )
         return( pPropHdr->rghwndButton[ c ] );
   return( NULL );
}

static void FASTCALL ClosePropertySheet( HWND hwnd, int nCode )
{
   PROPHDR FAR * pPropHdr;
   PROPPAGE FAR * pPropPage;
   HWND hwndParent;
   register int c;

   pPropHdr = GetBlock( hwnd );
   if( !IsWindowEnabled( pPropHdr->psh.hwndParent ) )
      EnableWindow( pPropHdr->psh.hwndParent, TRUE );

   SetWindowPos( hwnd, NULL, 0, 0, 0, 0, SWP_HIDEWINDOW|SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER );

   if( hwndParent = GetParent( hwnd ) )
      SetActiveWindow( hwndParent );
   if( pPropHdr->hwndTab )
      DestroyWindow( pPropHdr->hwndTab );

   pPropPage = pPropHdr->pLastPropPage;
   while( pPropPage )
      {
      PROPPAGE FAR * pPrevPropPage;

      pPrevPropPage = pPropPage->pPrevPropPage;
      if( pPropPage->psp.dwFlags & PSP_USECALLBACK )
         (*pPropPage->psp.pfnCallback)( hwnd, PSPCB_RELEASE, &pPropPage->psp );
      DestroyWindow( pPropPage->hwnd );
      if( pPropPage->pszCaption )
         FreeMemory( &pPropPage->pszCaption );
      FreeMemory( &pPropPage );
      pPropPage = pPrevPropPage;
      }
   for( c = 0; c < pPropHdr->cButtons; ++c )
      DestroyWindow( pPropHdr->rghwndButton[ c ] );
   DeleteFont( pPropHdr->hFont );
   DeleteFont( pPropHdr->hButtonFont );
   DestroyWindow( hwnd );
   nRetCode = nCode;
}

static void FASTCALL RecalcLayout( HWND hwnd, PROPHDR FAR * pPropHdr )
{
   PROPPAGE FAR * pPropPage;
   int nWidth;
   BOOL fWizard;
   RECT rectTab;
   RECT rectSheet;
   RECT rectPage;
   int x, y, i;

   CopyRect( &rectPage, &pPropHdr->rectLargest );

   nWidth = rectPage.right - rectPage.left;
   nWidth = max( nWidth, 4 * pPropHdr->sizeButton.cx + 3 * pPropHdr->cxButtonGap );
   rectPage.right = rectPage.left + nWidth;

   fWizard = ( pPropHdr->psh.dwFlags & PSH_WIZARD ) != 0;

   CopyRect( &rectTab, &rectPage );
   if( !fWizard )
      TabCtrl_AdjustRect( pPropHdr->hwndTab, TRUE, &rectTab );

   CopyRect( &rectSheet, &rectTab );
   rectSheet.left -= pPropHdr->sizeTabMargin.cx;
   rectSheet.top -= pPropHdr->sizeTabMargin.cy;
   rectSheet.right += pPropHdr->sizeTabMargin.cx;
   rectSheet.bottom += pPropHdr->sizeTabMargin.cy + pPropHdr->sizeButton.cy + pPropHdr->sizeTabMargin.cy;

   if( fWizard )
      rectSheet.bottom += pPropHdr->sizeTabMargin.cy + 2;
   else
      {
      OffsetRect( &rectTab, -rectSheet.left, -rectSheet.top );
      SetWindowPos( pPropHdr->hwndTab,
                    NULL,
                    pPropHdr->sizeTabMargin.cx,
                    pPropHdr->sizeTabMargin.cy,
                    rectTab.right - rectTab.left,
                    rectTab.bottom - rectTab.top,
                    SWP_NOACTIVATE|SWP_NOZORDER );
      CopyRect( &rectTab, &rectPage );
      TabCtrl_AdjustRect( pPropHdr->hwndTab, TRUE, &rectTab );

      CopyRect( &rectSheet, &rectTab );
      rectSheet.left -= pPropHdr->sizeTabMargin.cx;
      rectSheet.top -= pPropHdr->sizeTabMargin.cy;
      rectSheet.right += pPropHdr->sizeTabMargin.cx;
      rectSheet.bottom += pPropHdr->sizeTabMargin.cy + pPropHdr->sizeButton.cy + pPropHdr->sizeTabMargin.cy;
      }

   OffsetRect( &rectTab, -rectSheet.left, -rectSheet.top );
   OffsetRect( &rectPage, -rectSheet.left, -rectSheet.top );
   OffsetRect( &rectSheet, -rectSheet.left, -rectSheet.top );

   SetWindowPos( pPropHdr->hwndTab,
                 NULL,
                 pPropHdr->sizeTabMargin.cx,
                 pPropHdr->sizeTabMargin.cy,
                 rectTab.right - rectTab.left,
                 rectTab.bottom - rectTab.top,
                 SWP_NOACTIVATE|SWP_NOZORDER );

   AdjustWindowRectEx( &rectSheet, GetWindowStyle( hwnd ), FALSE, GetWindowExStyle( hwnd ) );
   SetWindowPos( hwnd,
                 NULL,
                 0, 0,
                 rectSheet.right - rectSheet.left,
                 rectSheet.bottom - rectSheet.top,
                 SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER );
   CenterWindow( hwnd );

   pPropPage = GetActivePage( pPropHdr );
   SetWindowPos( pPropPage->hwnd,
                 NULL,
                 rectPage.left,
                 rectPage.top,
                 rectPage.right - rectPage.left,
                 rectPage.bottom - rectPage.top,
                 SWP_NOACTIVATE|SWP_NOZORDER );

   GetClientRect( hwnd, &rectSheet );
   x = ( rectSheet.right - pPropHdr->sizeTabMargin.cx ) - pPropHdr->sizeButton.cx;
   y = ( ( rectSheet.bottom - rectSheet.top ) - pPropHdr->sizeTabMargin.cy ) - pPropHdr->sizeButton.cy;
   for( i = pPropHdr->cButtons - 1; i >=0; --i )
      {
      int id;

      SetWindowPos( pPropHdr->rghwndButton[ i ],
                    NULL,
                    x, y,
                    pPropHdr->sizeButton.cx,
                    pPropHdr->sizeButton.cy,
                    SWP_NOACTIVATE|SWP_NOZORDER );
      id = GetDlgCtrlID( pPropHdr->rghwndButton[ i ] );
      if( id != PSBTN_FINISH + PSBTN_BASE )
         {
         x -= pPropHdr->sizeButton.cx;
         if( id != PSBTN_NEXT + PSBTN_BASE )
            x -= pPropHdr->cxButtonGap;
         }
      }
}

static BOOL FASTCALL SetActivePage( HWND hwnd, PROPHDR FAR * pPropHdr, UINT nCurPage )
{
   PROPPAGE FAR * pPropPage;
   RECT rect;
   NMHDR nmh;
   UINT nLastPage;

   /* If there is an active page, get the page
    * dimensions and hide the page if it hasn't
    * already been hidden.
    */
   pPropPage = NULL;
   if( pPropHdr->nCurPage != -1 )
      {
      pPropPage = GetActivePage( pPropHdr );
      GetWindowRect( pPropPage->hwnd, &rect );
      ScreenToClient( hwnd, (LPPOINT)&rect );
      ScreenToClient( hwnd, ((LPPOINT)&rect) + 1 );
      if( IsWindowVisible( pPropPage->hwnd ) )
         ShowWindow( pPropPage->hwnd, SW_HIDE );
      }
   if( (int)nCurPage >= 0 )
      pPropPage = GetPage( pPropHdr, nCurPage );

   /* Tell the page owner that the page is being set active.
    */
   if( NULL != pPropPage )
      {
      nLastPage = pPropHdr->nCurPage;
      pPropHdr->nCurPage = nCurPage;
      Amctl_SendNotify( pPropPage->hwnd, hwnd, PSN_SETACTIVE, (LPNMHDR)&nmh );
   
      /* If we've a page to switch to, show the
       * new page.
       */
      if( (int)nCurPage >= 0 )
         {
         if( !IsRectEmpty( &rect ) )
            {
            int iWidth;
            int iHeight;
   
            iWidth = rect.right - rect.left;
            iHeight = rect.bottom - rect.top;
            SetWindowPos( pPropPage->hwnd, NULL, rect.left, rect.top, iWidth, iHeight, SWP_NOACTIVATE|SWP_NOZORDER );
            if( pPropHdr->hwndTab ) 
               SetWindowPos( pPropPage->hwnd, pPropHdr->hwndTab, 0, 0, 0, 0, SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE );
            }
         ShowWindow( pPropPage->hwnd, SW_SHOW );
         if( pPropHdr->psh.dwFlags & PSH_WIZARD )
            {
            SetWindowText( hwnd, pPropPage->pszCaption );
            EnableWindow( GetDlgItem( hwnd, PSBTN_BACK + PSBTN_BASE ), nCurPage > 0 );
            if( nCurPage == pPropHdr->cPages - 1 )
               {
               ShowWindow( GetDlgItem( hwnd, PSBTN_NEXT + PSBTN_BASE ), SW_HIDE );
               ShowWindow( GetDlgItem( hwnd, PSBTN_FINISH + PSBTN_BASE ), SW_SHOW );
               EnableWindow( GetDlgItem( hwnd, PSBTN_NEXT + PSBTN_BASE ), FALSE );
               EnableWindow( GetDlgItem( hwnd, PSBTN_FINISH + PSBTN_BASE ), TRUE );
               }
            else if( pPropHdr->cPages > 1 && nLastPage == pPropHdr->cPages - 1 )
               {
               ShowWindow( GetDlgItem( hwnd, PSBTN_NEXT + PSBTN_BASE ), SW_SHOW );
               ShowWindow( GetDlgItem( hwnd, PSBTN_FINISH + PSBTN_BASE ), SW_HIDE );
               EnableWindow( GetDlgItem( hwnd, PSBTN_NEXT + PSBTN_BASE ), TRUE );
               EnableWindow( GetDlgItem( hwnd, PSBTN_FINISH + PSBTN_BASE ), FALSE );
               }
            }
         EnableWindow( pPropPage->hwnd, TRUE );
         SetDefaultButton( hwnd, pPropHdr );
         SetFocusOnFirstControl( pPropHdr, pPropPage );
         }
      }
   return( TRUE );
}

/* This function sets the focus to the first control in the wizard or property
 * sheet that has the WS_TABSTOP property.
 */
void FASTCALL SetFocusOnFirstControl( PROPHDR FAR * pPropHdr, PROPPAGE FAR * pPropPage )
{
   HWND hwndFirst;

   hwndFirst = GetWindow( pPropPage->hwnd, GW_CHILD );
   if( !( GetWindowStyle( hwndFirst ) & WS_TABSTOP ) )
      hwndFirst = GetNextDlgTabItem( pPropPage->hwnd, hwndFirst, FALSE );
   if( hwndFirst == NULL )
      {
      HWND hwndCtrl;

      hwndCtrl = pPropHdr->rghwndButton[ 0 ];
      SetFocus( pPropHdr->hwndFocus = hwndCtrl );
      }
   else
      SetFocus( pPropHdr->hwndFocus = hwndFirst );
}

/* This function returns the handle of the active page.
 */
static PROPPAGE FAR * FASTCALL GetActivePage( PROPHDR FAR * pPropHdr )
{
   return( GetPage( pPropHdr, pPropHdr->nCurPage ) );
}

/* This function returns the handle of a page given the index
 * of that page, where the first page has an index of zero.
 */
static PROPPAGE FAR * FASTCALL GetPage( PROPHDR FAR * pPropHdr, int nPage )
{
   PROPPAGE FAR * pPropPage;

   pPropPage = pPropHdr->pFirstPropPage;
   while( pPropPage && nPage-- )
      pPropPage = pPropPage->pNextPropPage;
   return( pPropPage );
}

static BOOL FASTCALL CreateStandardButtons( HWND hwnd, PROPHDR FAR * pPropHdr )
{
   static UINT wizId[] = { PSBTN_BACK, PSBTN_NEXT, PSBTN_FINISH, PSBTN_CANCEL, PSBTN_HELP };
   static UINT btnId[] = { PSBTN_OK, PSBTN_CANCEL, PSBTN_APPLYNOW, PSBTN_HELP };
   static char * wizStr[] = { "< &Back", "&Next >", "Finish", "Cancel", "&Help" };
   static char * btnStr[] = { "OK", "Cancel", "&Apply", "&Help" };
   int cWizardButtons = sizeof( wizId ) / sizeof( UINT );
   int cButtons = sizeof( btnId ) / sizeof( UINT );
   register int c;
   DWORD dwStyles;
   BOOL fNoApplyNow;
   BOOL fHasHelp;

   pPropHdr->cButtons = 0;
   fNoApplyNow = ( pPropHdr->psh.dwFlags & PSH_NOAPPLYNOW ) != 0;
   fHasHelp = ( pPropHdr->psh.dwFlags & PSH_HASHELP ) != 0;

   if( pPropHdr->psh.dwFlags & PSH_WIZARD )
      {
      for( c = 0; c < cWizardButtons; ++c )
         {
         HWND hwndBtn;

         if( PSBTN_HELP == wizId[ c ] && !fHasHelp )
            continue;
         dwStyles = WS_CHILD|WS_TABSTOP|WS_GROUP|BS_PUSHBUTTON;
         if( PSBTN_FINISH != wizId[ c ] )
            dwStyles |= WS_VISIBLE;
         hwndBtn = CreateWindow( "button", wizStr[ c ], dwStyles, 0, 0, 0, 0, hwnd, (HMENU)( wizId[ c ] + PSBTN_BASE ), hLibInst, NULL );
         if( !hwndBtn )
            return( FALSE );
         SetWindowFont( hwndBtn, pPropHdr->hButtonFont, FALSE );
         pPropHdr->rghwndButton[ pPropHdr->cButtons++ ] = hwndBtn;
         }
      }
   else
      {
      for( c = 0; c < cButtons; ++c )
         {
         HWND hwndBtn;

         if( PSBTN_HELP == btnId[ c ] && !fHasHelp )
            continue;
         if( PSBTN_APPLYNOW == btnId[ c ] && fNoApplyNow )
            continue;
         dwStyles = WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_GROUP|BS_PUSHBUTTON;
         hwndBtn = CreateWindow( "button", btnStr[ c ], dwStyles, 0, 0, 0, 0, hwnd, (HMENU)( btnId[ c ] + PSBTN_BASE ), hLibInst, NULL );
         if( !hwndBtn )
            return( FALSE );
         SetWindowFont( hwndBtn, pPropHdr->hButtonFont, FALSE );
         pPropHdr->rghwndButton[ pPropHdr->cButtons++ ] = hwndBtn;
         }
      }
   return( TRUE );
}

static BOOL FASTCALL CreatePropertyPages( HWND hwnd, PROPHDR FAR * pPropHdr )
{
   register UINT i;
   BOOL fError;

   fError = FALSE;
   for( i = 0; !fError && i < pPropHdr->psh.nPages; ++i )
      {
      LPCAMPROPSHEETPAGE ppsp;
      PROPPAGE FAR * pNewPage;

      if( pPropHdr->psh.dwFlags & PSH_PROPSHEETPAGE )
         ppsp = pPropHdr->psh.U3(ppsp) + i;
      else
         ppsp = (LPCAMPROPSHEETPAGE)LocalLock( (HLOCAL)LOWORD( pPropHdr->psh.U3(phpage)[ i ] ) );
      if( pNewPage = AddPropertyPage( hwnd, pPropHdr, ppsp ) )
         {
         RECT rc;

         GetClientRect( pNewPage->hwnd, &rc );
         if( rc.right > pPropHdr->rectLargest.right )
            pPropHdr->rectLargest.right = rc.right;
         if( rc.bottom > pPropHdr->rectLargest.bottom )
            pPropHdr->rectLargest.bottom = rc.bottom;
         }
      else if( NULL == pNewPage )
         fError = TRUE;
      if( !( pPropHdr->psh.dwFlags & PSH_PROPSHEETPAGE ) )
         LocalUnlock( (HLOCAL)LOWORD( pPropHdr->psh.U3(phpage)[ i ] ) );
      }
   return( !fError );
}

/* This function creates a new property page.
 */
static PROPPAGE FAR * FASTCALL AddPropertyPage( HWND hwnd, PROPHDR FAR * pPropHdr, LPCAMPROPSHEETPAGE ppsp )
{
   static PROPPAGE FAR * pNewPage;
   NMHDR nmhdr;

   /* If PSP_USECALLBACK is specified, then we call back through the
    * pfnCallback pointer to a user defined function which decides whether
    * or not we create this page. Note that we do not treat a zero return from
    * the callback as an error, otherwise the calling function will not
    * create the entire property sheet!
    */
   if( ppsp->dwFlags & PSP_USECALLBACK )
      if( 0 == (*ppsp->pfnCallback)( hwnd, PSPCB_CREATE, (LPAMPROPSHEETPAGE)ppsp ) )
         return( (PROPPAGE FAR *)-1 );

   /* Create a new property sheet page.
    */
   INITIALISE_PTR(pNewPage);
   if( fNewMemory( &pNewPage, sizeof( PROPPAGE ) ) )
      {
      LPVOID pResource;
      static char sz[ 40 ];

      /* Link this record to the rest of them in this
       * property sheet.
       */
      if( NULL == pPropHdr->pFirstPropPage )
         pPropHdr->pFirstPropPage = pNewPage;
      else
         pPropHdr->pLastPropPage->pNextPropPage = pNewPage;
      pNewPage->pPrevPropPage = pPropHdr->pLastPropPage;
      pNewPage->pNextPropPage = NULL;
      pPropHdr->pLastPropPage = pNewPage;
      pNewPage->psp = *ppsp;

      /* If the dialog template is loaded from the owner resource, then
       * load it into memory and get a handle to the dialog template suitable
       * for passing to CreateDialogIndirectParam.
       *
       * BUGBUG: Does not handle PSP_DLGINDIRECT.
       */
      pResource = NULL;
      if( !( ppsp->dwFlags & PSP_DLGINDIRECT ) )
         pResource = LockDlgResource( ppsp->hInstance, ppsp->U1(pszTemplate) );
      if( NULL == pResource )
         return( NULL );

      /* If the property sheet specifies PSP_USETITLE, then the
       * ppsp->pszTitle is either a pointer to the title or the resource
       * ID of a string in the owner resource space. Otherwise we get
       * the title from the dialog caption.
       */
      if( ppsp->dwFlags & PSP_USETITLE )
         {
         if( 0 == HIWORD( ppsp->pszTitle ) )
            LoadString( ppsp->hInstance, LOWORD( ppsp->pszTitle ), sz, sizeof( sz ) );
         else
            lstrcpy( sz, ppsp->pszTitle );
         }
      else
         GetCaptionFromDialog( pResource, sz, sizeof( sz ) - 1 );

      /* If this is not a wizard, add the title of the property
       * sheet to the tab control.
       */
      if( !( pPropHdr->psh.dwFlags & PSH_WIZARD ) )
         {
         TC_ITEM tie;

         ASSERT(IsWindow(pPropHdr->hwndTab));
         tie.mask = TCIF_TEXT;
         tie.pszText = sz;
         TabCtrl_InsertItem( pPropHdr->hwndTab, pPropHdr->cPages, &tie );
         pNewPage->pszCaption = NULL;
         }
      else
         {
         static LPSTR lpszText;

         /* This is a wizard, so the title is displayed on the dialog caption
          * when the page is made visible. So we need to allocate storage for this
          * title in the page record.
          */
         INITIALISE_PTR(lpszText);
         if( fNewMemory( &lpszText, strlen( sz ) + 1 ) )
            {
            pNewPage->pszCaption = lpszText;
            lstrcpy( pNewPage->pszCaption, sz );
            }
         else
            Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_OUTOFMEMORY, &nmhdr );
         }

      /* Create the property sheet page.
       */
      pNewPage->hwnd = CreateDialogIndirectParam( ppsp->hInstance, pResource, hwnd, ppsp->pfnDlgProc, (LPARAM)ppsp );
      UnlockDlgResource( pResource );
      if( !pNewPage->hwnd )
         {
         if( !( pPropHdr->psh.dwFlags & PSH_WIZARD ) )
            TabCtrl_DeleteItem( pPropHdr->hwndTab, pPropHdr->cPages );
         else
            {
            if( pNewPage->pszCaption )
               FreeMemory( &pNewPage->pszCaption );
            }
         return( NULL );
         }

      /* Bump the number of valid pages we've got.
       */
      ++pPropHdr->cPages;
      }
   else
      Amctl_SendNotify( GetParent( hwnd ), hwnd, NM_OUTOFMEMORY, &nmhdr );
   return( pNewPage );
}

/* This function centers the specified window on the screen.
 */
static void FASTCALL CenterWindow( HWND hwnd )
{
#ifdef NOUSEMM
   RECT rc;
   RECT rcScreen;
   int x, y;

   SetRect( &rcScreen, 0, 0, GetSystemMetrics( SM_CXSCREEN ), GetSystemMetrics( SM_CYSCREEN ) );
   GetWindowRect( hwnd, &rc );
   x = ( ( rcScreen.right - rcScreen.left ) - ( rc.right - rc.left ) ) / 2;
   y = ( ( rcScreen.bottom - rcScreen.top ) - ( rc.bottom - rc.top ) ) / 3;
   SetWindowPos( hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE );
#else NOUSEMM
   CenterWindowToMonitor(hwnd, hwnd, TRUE);
#endif NOUSEMM
}

/* This function loads a dialog box resource into memory and returns a pointer
 * to the resource template, or NULL if the resource could not be locked.
 */
static LPVOID FASTCALL LockDlgResource( HINSTANCE hInst, LPCSTR pszTemplate )
{
   HRSRC hDlgRes;
   HANDLE hRes;

   if( !( hDlgRes = FindResource( hInst, pszTemplate, RT_DIALOG ) ) )
      return( NULL );
   if( NULL == ( hRes = LoadResource( hInst, hDlgRes ) ) )
      return( NULL );
   return( LockResource( hRes ) );
}

/* This function unlocks a dialog box resource loaded by LockDlgResource.
 * Changed: spalmer: 27/10/95: Not needed under Win32.
 */
static void FASTCALL UnlockDlgResource( LPCSTR pszTemplate )
{
#ifndef WIN32
   HANDLE hRes;

   hRes = GlobalPtrHandle( pszTemplate );
   UnlockResource( hRes );
   FreeResource( hRes );
#endif
}

/* This function extracts the caption from a dialog box resource. The caption
 * appears as the third string in the resource header.
 */
static void FASTCALL GetCaptionFromDialog( LPVOID pResource, char * psz, int cMax )
{
#ifdef WIN32
   WORD FAR * lpDlgRes;
#else
   LPSTR lpDlgRes;
#endif
   register int c;

   lpDlgRes = pResource;
#ifdef WIN32
   lpDlgRes += 18;
#else
   lpDlgRes += 14;
#endif
   while( (char)*lpDlgRes++ );
   while( (char)*lpDlgRes++ );
   for( c = 0; (char)*lpDlgRes && c < cMax; ++c )
      psz[ c ] = (char)*lpDlgRes++;
   psz[ c ] = '\0';
}

static void FASTCALL HelpButton( HWND hwnd )
{
   PROPHDR FAR * pPropHdr;
   PROPPAGE FAR * pPropPage;
   NMHDR nmh;

   pPropHdr = GetBlock( hwnd );
   pPropPage = GetActivePage( pPropHdr );
   Amctl_SendNotify( pPropPage->hwnd, hwnd, PSN_HELP, (LPNMHDR)&nmh );
}

/* This function implements the Cancel button. We first send PSN_QUERYCANCEL to
 * each page asking it to verify the cancel. If any page returns TRUE, we don't
 * go ahead. Then we send PSN_RESET to do the actual cancel.
 */
static BOOL FASTCALL CancelButton( HWND hwnd )
{
   PROPHDR FAR * pPropHdr;
   PROPPAGE FAR * pPropPage;
   NMHDR nmh;

   pPropHdr = GetBlock( hwnd );
   pPropPage = GetActivePage( pPropHdr );
   if( TRUE == Amctl_SendNotify( pPropPage->hwnd, hwnd, PSN_QUERYCANCEL, (LPNMHDR)&nmh ) )
      return( FALSE );

   pPropPage = pPropHdr->pFirstPropPage;
   while( pPropPage )
      {
      Amctl_SendNotify( pPropPage->hwnd, hwnd, PSN_RESET, (LPNMHDR)&nmh );
      pPropPage = pPropPage->pNextPropPage;
      }
   return( TRUE );
}

static BOOL FASTCALL ApplyButton( HWND hwnd )
{
   PROPHDR FAR * pPropHdr;
   PROPPAGE FAR * pPropPage;
   BOOL fKillpage;
   NMHDR nmh;

   pPropHdr = GetBlock( hwnd );
   pPropPage = GetActivePage( pPropHdr );
   fKillpage = FALSE;
   if( FALSE == Amctl_SendNotify( pPropPage->hwnd, hwnd, PSN_KILLACTIVE, (LPNMHDR)&nmh ) )
      {
      UINT nPage;

      pPropPage = pPropHdr->pFirstPropPage;
      for( nPage = 0; pPropPage; ++nPage )
         {
         int ret;

         ret = (int)Amctl_SendNotify( pPropPage->hwnd, hwnd, PSN_APPLY, (LPNMHDR)&nmh );
         if( PSNRET_NOERROR != ret )
            {
            if( ret == PSNRET_INVALID && nPage != pPropHdr->nCurPage )
               SetActivePage( hwnd, pPropHdr, nPage );
            return( FALSE );
            }
         pPropPage = pPropPage->pNextPropPage;
         }
      return( TRUE );
      }
   return( FALSE );
}

static void FASTCALL WizardBackButton( HWND hwnd )
{
   PROPHDR FAR * pPropHdr;
   PROPPAGE FAR * pPropPage;
   NMHDR nmh;

   pPropHdr = GetBlock( hwnd );
   pPropPage = GetActivePage( pPropHdr );
   if( pPropHdr->nCurPage > 0 )
      {
      int nPage;

      if( ( nPage = (int)Amctl_SendNotify( pPropPage->hwnd, hwnd, PSN_WIZBACK, (LPNMHDR)&nmh ) ) != -1 )
         {
         nPage = ResourceIDToPage( pPropHdr, MAKEINTRESOURCE( nPage ) );
         SetActivePage( hwnd, pPropHdr, nPage );
         }
      }
}

static void FASTCALL WizardNextButton( HWND hwnd )
{
   PROPHDR FAR * pPropHdr;
   PROPPAGE FAR * pPropPage;
   NMHDR nmh;

   pPropHdr = GetBlock( hwnd );
   pPropPage = GetActivePage( pPropHdr );
   if( pPropHdr->nCurPage < pPropHdr->cPages - 1 )
      {
      int nPage;

      if( ( nPage = (int)Amctl_SendNotify( pPropPage->hwnd, hwnd, PSN_WIZNEXT, (LPNMHDR)&nmh ) ) != -1 )
         {
         nPage = ResourceIDToPage( pPropHdr, MAKEINTRESOURCE( nPage ) );
         SetActivePage( hwnd, pPropHdr, nPage );
         }
      }
}

static BOOL FASTCALL WizardFinishButton( HWND hwnd )
{
   PROPHDR FAR * pPropHdr;
   PROPPAGE FAR * pPropPage;
   NMHDR nmh;

   pPropHdr = GetBlock( hwnd );
   pPropPage = GetActivePage( pPropHdr );
   return( Amctl_SendNotify( pPropPage->hwnd, hwnd, PSN_WIZFINISH, (LPNMHDR)&nmh ) != 0 );
}

static int FASTCALL ResourceIDToPage( PROPHDR FAR * pPropHdr, LPCSTR lpszResource )
{
   PROPPAGE FAR * pPropPage;
   int cPage;

   pPropPage = pPropHdr->pFirstPropPage;
   for( cPage = 0; pPropPage; ++cPage )
      {
      if( pPropPage->psp.U1(pszTemplate) == lpszResource )
         return( cPage );
      pPropPage = pPropPage->pNextPropPage;
      }
   return( 0 );
}

/* This function sets the property sheet title as defined by lpszText. If the top word
 * of lpszText is zero, then lpszText is an resource integer and it is used to retrieve the
 * actual text of the title from the application string table. If PSH_PROPTITLE is set in the
 * header style, the title is prefixed "Properties for ".
 */
static void FASTCALL SetPropertySheetTitle( HWND hwnd, HINSTANCE hInst, DWORD dwStyle, LPCSTR lpszText )
{
   static char sz[ 100 ];
   register int len;

   sz[ 0 ] = '\0';
   if( dwStyle & PSH_PROPTITLE )
      strcpy( sz, "Properties for " );
   len = strlen( sz );
   if( 0 == HIWORD( lpszText ) )
      LoadString( hInst, LOWORD( lpszText ), sz + len, sizeof( sz ) - len );
   else
      lstrcpy( sz + len, lpszText );
   SetWindowText( hwnd, sz );
}

/* This function creates the default font for a property sheet. Under Windows 95,
 * the font is non-bold, otherwise it is bold.
 */
static void FASTCALL CreatePropertySheetFont( PROPHDR FAR * pPropHdr )
{
   HDC hdc;
   int cPixelsPerInch;
   int cyPixels;

   hdc = GetDC( NULL );
   cPixelsPerInch = GetDeviceCaps( hdc, LOGPIXELSY );
   cyPixels = -MulDiv( 8, cPixelsPerInch, 72 );
   pPropHdr->hFont = CreateFont( cyPixels, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                                 FALSE, ANSI_CHARSET, OUT_CHARACTER_PRECIS, CLIP_DEFAULT_PRECIS,
                                 PROOF_QUALITY, VARIABLE_PITCH | FF_SWISS, "Helv" );
   pPropHdr->hButtonFont = CreateFont( cyPixels, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                                 FALSE, ANSI_CHARSET, OUT_CHARACTER_PRECIS, CLIP_DEFAULT_PRECIS,
                                 PROOF_QUALITY, VARIABLE_PITCH | FF_SWISS, "Helv" );
   ReleaseDC( NULL, hdc );
}
