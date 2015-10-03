/* EVENT.C - Handles the event log and the event log dialog
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
#include "amlib.h"
#include <commdlg.h>
#include <string.h>
#include "amevent.h"

#define  THIS_FILE   __FILE__

char NEAR szEventLogWndClass[] = "amctl_eventlogwndcls";
char NEAR szEventLogWndName[] = "Event Log";

static BOOL fDefDlgEx = FALSE;            /* DefDlg recursion flag trap */
static BOOL fRegistered = FALSE;          /* TRUE if we've registered the event log window */
static HWND hwndEventLog = NULL;          /* The event log window */
static HWND hwndCtlFocus = NULL;          /* The last control with the focus */
static HBITMAP hbmpEventLog = NULL;       /* Event log bitmaps */

LRESULT EXPORT CALLBACK EventLogWndProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL EventLog_OnCreate( HWND, LPCREATESTRUCT );
void FASTCALL EventLog_OnClose( HWND );
void FASTCALL EventLog_OnMove( HWND, int, int );
void FASTCALL EventLog_OnSize( HWND, UINT, int, int );
void FASTCALL EventLog_OnDestroy( HWND );
void FASTCALL EventLog_OnAdjustWindows( HWND, int, int );
void FASTCALL EventLog_OnCommand( HWND, int, HWND, UINT );
void FASTCALL EventLog_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
void FASTCALL EventLog_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
void FASTCALL EventLog_MDIActivate( HWND, BOOL, HWND, HWND );
void FASTCALL EventLog_OnSetFocus( HWND, HWND );

BOOL EXPORT CALLBACK EventLogOptions( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL EventLogOptions_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL EventLogOptions_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL EventLogOptions_DlgProc( HWND, UINT, WPARAM, LPARAM );

void FASTCALL UpdateEventListStatus( HWND );
BOOL WINAPI EXPORT EventLogEvents( int, LPARAM, LPARAM );

/* This function displays the Event Log dialog.
 */
BOOL FASTCALL CmdEventLog( HWND hwnd )
{
   if( NULL != hwndEventLog )
      BringWindowToTop( hwndEventLog );
   else
      {
      DWORD dwState;
      RECT rc;

      /* Register the group list window class if we have
       * not already done so.
       */
      if( !fRegistered )
         {
         WNDCLASS wc;

         wc.style          = CS_HREDRAW|CS_VREDRAW;
         wc.lpfnWndProc    = EventLogWndProc;
         wc.hIcon          = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_EVENTLOG) );
         wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
         wc.lpszMenuName   = NULL;
         wc.cbWndExtra     = DLGWINDOWEXTRA;
         wc.cbClsExtra     = 0;
         wc.hbrBackground  = (HBRUSH)( COLOR_APPWORKSPACE + 1 );
         wc.lpszClassName  = szEventLogWndClass;
         wc.hInstance      = hInst;
         if( !RegisterClass( &wc ) )
            return( FALSE );
         fRegistered = TRUE;
         }
   
      /* The window will appear in the left half of the
       * client area.
       */
      GetClientRect( hwndMDIClient, &rc );
      InflateRect( &rc, -50, -50 );
      dwState = 0;
   
      /* Get the actual window size, position and state.
       */
      ReadProperWindowState( szEventLogWndName, &rc, &dwState );
   
      /* Load the event log bitmaps.
       */
      ASSERT( NULL == hbmpEventLog );
      hbmpEventLog = LoadBitmap( hRscLib, MAKEINTRESOURCE(IDB_EVENTLOGBMPS) );

      /* Create the window.
       */
      hwndEventLog = Adm_CreateMDIWindow( szEventLogWndName, szEventLogWndClass, hInst, &rc, dwState, 0L );
      if( NULL == hwndEventLog )
         return( FALSE );

      /* Show the window.
       */
      SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndEventLog, 0L );

      /* Set the focus
       */
      hwndCtlFocus = GetDlgItem( hwndEventLog, IDD_LIST );
      SetFocus( hwndCtlFocus );
      }
   return( TRUE );
}

/* This function handles the dialog box messages passed to the EventLog
 * dialog.
 */
LRESULT CALLBACK EXPORT EventLogWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, EventLog_OnCreate );
      HANDLE_MSG( hwnd, WM_CLOSE, EventLog_OnClose );
      HANDLE_MSG( hwnd, WM_DESTROY, EventLog_OnDestroy );
      HANDLE_MSG( hwnd, WM_SIZE, EventLog_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, EventLog_OnMove );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, Common_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, EventLog_MDIActivate );
      HANDLE_MSG( hwnd, WM_COMMAND, EventLog_OnCommand );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, EventLog_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_DRAWITEM, EventLog_OnDrawItem );
      HANDLE_MSG( hwnd, WM_SETFOCUS, EventLog_OnSetFocus );
      HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, EventLog_OnAdjustWindows );

      case WM_GETMINMAXINFO: {
         MINMAXINFO FAR * lpMinMaxInfo;
         LRESULT lResult;

         /* Set the minimum tracking size so the client window can never
          * be resized below 24 lines.
          */
         lResult = Adm_DefMDIDlgProc( hwnd, message, wParam, lParam );
         lpMinMaxInfo = (MINMAXINFO FAR *)lParam;
         lpMinMaxInfo->ptMinTrackSize.x = 185;
         lpMinMaxInfo->ptMinTrackSize.y = 185;
         return( lResult );
         }

      default:
         break;
      }
   return( Adm_DefMDIDlgProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_SETFOCUS message.
 */
void FASTCALL EventLog_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   if( NULL != hwndCtlFocus )
      SetFocus( hwndCtlFocus );
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL EventLog_OnCreate( HWND hwnd, LPCREATESTRUCT lpCreateStruct )
{
   LPMDICREATESTRUCT lpMDICreateStruct;
   LPEVENT lpEvent;
   HWND hwndList;

   /* Create the dialog window.
    */
   lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
   Adm_MDIDialog( hwnd, MAKEINTRESOURCE(IDDLG_EVENTLOG), lpMDICreateStruct );

   /* Fill the list control with the event log.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   for( lpEvent = Amevent_GetEvent( NULL ); lpEvent; lpEvent = Amevent_GetEvent( lpEvent ) )
      ListBox_AddString( hwndList, (LPSTR)lpEvent );
   ListBox_SetCurSel( hwndList, 0 );

   /* Enable the relevant buttons.
    */
   UpdateEventListStatus( hwnd );
   
   /* Register to receive some events.
    */
   Amuser_RegisterEvent( AE_EVENTLOG_CLEARED, EventLogEvents );
   Amuser_RegisterEvent( AE_EVENTLOG_CHANGED, EventLogEvents );
   return( TRUE );
}

/* This function updates the status portion of the
 * Event log.
 */
void FASTCALL UpdateEventListStatus( HWND hwnd )
{
   HWND hwndList;
   int count;

   hwndList = GetDlgItem( hwnd, IDD_LIST );
   SetFocus( hwndList );
   if( 0 == ( count = ListBox_GetCount( hwndList ) ) )
      {
      EnableControl( hwnd, IDD_SAVE, FALSE );
      EnableControl( hwnd, IDD_CLEAR, FALSE );
      PostMessage( hwnd, WM_NEXTDLGCTL, (WPARAM)GetDlgItem( hwnd, IDOK ), (LPARAM)TRUE );
      }
   if( count > 1 )
      wsprintf( lpTmpBuf, GS(IDS_STR926), count );
   else if( count == 1 )
      wsprintf( lpTmpBuf, GS(IDS_STR927) );
   else
      wsprintf( lpTmpBuf, GS(IDS_STR928) );
   SetDlgItemText( hwnd, IDD_PAD1, lpTmpBuf );
}

/* This function handles newsgroup events.
 */
BOOL WINAPI EXPORT EventLogEvents( int wEvent, LPARAM lParam1, LPARAM lParam2 )
{
   switch( wEvent )
      {
      case AE_EVENTLOG_CHANGED:
         if( hwndEventLog )
            {
            LPEVENT lpNewEvent;
            LPEVENT lpOldEvent;
            HWND hwndList;
            int index;

            /* A new event was added to the log, so insert it into
             * the listbox at the top and remove any old event from
             * the end.
             */
            lpNewEvent = (LPEVENT)lParam1;
            lpOldEvent = (LPEVENT)lParam2;
            hwndList = GetDlgItem( hwndEventLog, IDD_LIST );

            /* If an event was deleted, locate it from the listbox
             * and remove it.
             */
            if( lpOldEvent )
               if( LB_ERR != ( index = ListBox_FindItemData( hwndList, -1, (LPARAM)lpOldEvent ) ) )
                  ListBox_DeleteString( hwndList, index );

            /* Insert the new event at the top of the list.
             */
            if( lpNewEvent )
               if( LB_ERR != ( index = ListBox_InsertString( hwndList, 0, (LPARAM)lpNewEvent ) ) )
                  ListBox_SetCurSel( hwndList, index );
            }
         break;

      case AE_EVENTLOG_CLEARED:
         /* The event log has been cleared, so empty the
          * list window.
          */
         if( hwndEventLog )
            {
            HWND hwndList;

            hwndList = GetDlgItem( hwndEventLog, IDD_LIST );
            ListBox_ResetContent( hwndList );
            }
         break;
      }
   return( TRUE );
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL EventLog_OnAdjustWindows( HWND hwnd, int dx, int dy )
{
   Adm_MoveWnd( GetDlgItem( hwnd, IDOK ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_SAVE ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_OPTIONS ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_CLEAR ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_HELP ), dx, 0 );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_LIST ), dx, dy );
}

/* This function handles the WM_CLOSE message.
 */
void FASTCALL EventLog_OnClose( HWND hwnd )
{
   Adm_DestroyMDIWindow( hwnd );
   hwndCtlFocus = NULL;
}

/* This function handles the WM_DESTROY message.
 */
void FASTCALL EventLog_OnDestroy( HWND hwnd )
{
   /* Unregister events.
    */
   Amuser_UnregisterEvent( AE_EVENTLOG_CLEARED, EventLogEvents );
   Amuser_UnregisterEvent( AE_EVENTLOG_CHANGED, EventLogEvents );
   DeleteBitmap( hbmpEventLog );
   hbmpEventLog = NULL;
   hwndEventLog = NULL;
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL EventLog_OnMove( HWND hwnd, int x, int y )
{
   Amuser_WriteWindowState( szEventLogWndName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL EventLog_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   Amuser_WriteWindowState( szEventLogWndName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* This function handles the WM_MDIACTIVATE message.
 */
void FASTCALL EventLog_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_GRPLIST ), (LPARAM)(LPSTR)hwnd );
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, Adm_DefMDIDlgProc );
}

/* This function handles the WM_MEASUREITEM message
 */
void FASTCALL EventLog_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   HFONT hFont;
   BITMAP bmp;
   HDC hdc;

   hdc = GetDC( hwnd );
   hFont = SelectFont( hdc, hHelvB8Font );
   GetTextMetrics( hdc, &tm );
   GetObject( hbmpEventLog, sizeof( BITMAP ), &bmp );
   lpMeasureItem->itemHeight = max( tm.tmHeight, bmp.bmHeight );
   SelectFont( hdc, hFont );
   ReleaseDC( hwnd, hdc );
}

/* This function handles the WM_DRAWITEM message
 */
void FASTCALL EventLog_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   SIZE size;         

   if( lpDrawItem->itemID != -1 )
      if( lpDrawItem->itemAction == ODA_FOCUS )
         DrawFocusRect( lpDrawItem->hDC, (LPRECT)&lpDrawItem->rcItem );
      else {
         LPEVENT lpEvent;
         EVENTLOGITEM eli;
         COLORREF tmpT;
         COLORREF tmpB;
         COLORREF T;
         COLORREF B;
         AM_DATE date;
         AM_TIME time;
         char sz[ 100 ];
         HBRUSH hbr;
         HFONT hOldFont;
         RECT rc;

         /* Initialise.
          */
         lpEvent = (LPEVENT)ListBox_GetItemData( lpDrawItem->hwndItem, lpDrawItem->itemID );
         Amevent_GetEventInfo( lpEvent, &eli );
         rc = lpDrawItem->rcItem;
         if( lpDrawItem->itemState & ODS_SELECTED )
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

         /* Erase the entire drawing area
          */
         FillRect( lpDrawItem->hDC, &rc, hbr );
         tmpT = SetTextColor( lpDrawItem->hDC, T );
         tmpB = SetBkColor( lpDrawItem->hDC, B );

         /* Display the type of the event
          */
         switch( eli.wType & ETYP_MASK2 )
            {
            case ETYP_WARNING:   Amuser_DrawBitmap( lpDrawItem->hDC, rc.left + 2, rc.top, 16, 16, hbmpEventLog, 1 ); break;
            case ETYP_SEVERE:    Amuser_DrawBitmap( lpDrawItem->hDC, rc.left + 2, rc.top, 16, 16, hbmpEventLog, 2 ); break;
            case ETYP_MESSAGE:   Amuser_DrawBitmap( lpDrawItem->hDC, rc.left + 2, rc.top, 16, 16, hbmpEventLog, 0 ); break;
            }

         /* Display the event date
          */
         hOldFont = SelectFont( lpDrawItem->hDC, hHelv8Font ); 
         rc.left += 22;                              
         Amdate_UnpackDate( eli.wDate, &date );
         Amdate_FormatShortDate( &date, sz );
         ExtTextOut( lpDrawItem->hDC, rc.left, rc.top, ETO_OPAQUE, &rc, sz, strlen( sz ), NULL );


         /* Display the event time
          */
         GetTextExtentPoint (lpDrawItem->hDC, sz, strlen( sz ), &size);   // !!SM!! 2.55
         rc.left += (10 + size.cx); //54;                          // !!SM!! 2.55
         Amdate_UnpackTime( eli.wTime, &time );            
         Amdate_FormatTime( &time, sz );
         ExtTextOut( lpDrawItem->hDC, rc.left, rc.top, ETO_OPAQUE, &rc, sz, strlen( sz ), NULL );

         /* Display the event description
          */
         GetTextExtentPoint (lpDrawItem->hDC, sz, strlen( sz ), &size);   // !!SM!! 2.55      
         rc.left += (10 + size.cx); //70;                       // !!SM!! 2.55
         ExtTextOut( lpDrawItem->hDC, rc.left, rc.top, ETO_OPAQUE, &rc, eli.szDescription,
                     lstrlen( eli.szDescription ), NULL );

         /* Restore things back to normal.
          */
         SelectFont( lpDrawItem->hDC, hOldFont );
         SetTextColor( lpDrawItem->hDC, tmpT );
         SetBkColor( lpDrawItem->hDC, tmpB );
         DeleteBrush( hbr );
         }
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL EventLog_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsEVENTLOG );
         break;

      case IDD_SAVE: {
         char sz[ _MAX_PATH ];
         OPENFILENAME ofn;
         static char strFilters[] = "Log Files (*.LOG)\0*.log\0All Files (*.*)\0*.*\0\0";

         sz[ 0 ] = '\0';
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
         ofn.lpstrTitle = "Save Event Log";
         ofn.Flags = OFN_OVERWRITEPROMPT|OFN_NOCHANGEDIR|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST;
         ofn.nFileOffset = 0;
         ofn.nFileExtension = 0;
         ofn.lpstrDefExt = NULL;
         ofn.lCustData = 0;
         ofn.lpfnHook = NULL;
         ofn.lpTemplateName = 0;
         if( GetSaveFileName( &ofn ) )
            {
            LPEVENT lpEvent;
            BOOL fError;
            HNDFILE fh;

            /* Create the event log output file.
             */
            if( ( fh = Amfile_Create( sz, 0 ) ) == HNDFILE_ERROR )
               {
               FileCreateError( hwnd, sz, FALSE, FALSE );
               break;
               }

            /* Loop for all events and write them to the file with the
             * most recent event first.
             */
            fError = FALSE;
            for( lpEvent = Amevent_GetEvent( NULL ); !fError && lpEvent; lpEvent = Amevent_GetEvent( lpEvent ) )
               {
               EVENTLOGITEM eli;
               AM_DATE date;
               AM_TIME time;

               Amevent_GetEventInfo( lpEvent, &eli );
               Amdate_UnpackDate( eli.wDate, &date );
               Amdate_UnpackTime( eli.wTime, &time );
               wsprintf( sz, "%c %s %s %s\r\n",
                         ( (eli.wType & ETYP_SEVERE) ? 'E' : (eli.wType & ETYP_WARNING) ? 'W' : 'M' ),
                         Amdate_FormatShortDate( &date, NULL ),
                         Amdate_FormatTime( &time, NULL ),
                         eli.szDescription );
               while( Amfile_Write( fh, sz, strlen( sz ) ) != strlen( sz ) )
                  if( !DiskWriteError( hwnd, sz, TRUE, FALSE ) )
                     {
                     fError = TRUE;
                     break;
                     }
               }
            Amfile_Close( fh );
            SetFocus( GetDlgItem( hwnd, IDD_LIST ) );
            }
         break;
         }

      case IDD_OPTIONS:
         Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE( IDDLG_EVENTLOGOPTIONS ), EventLogOptions, 0 );
         SetFocus( GetDlgItem( hwnd, IDD_LIST ) );
         break;

      case IDD_CLEAR:
         /* Clear the event log. The list box will automatically be
          * emptied via the AE_EVENTLOG_CLEARED event.
          */
         if( fMessageBox( hwnd, 0, GS(IDS_STR580), MB_YESNO|MB_ICONQUESTION ) == IDNO )
            break;
         Amevent_ClearEventLog();
         UpdateEventListStatus( hwnd );
         break;

      case IDCANCEL:
      case IDOK:
         Adm_DestroyMDIWindow( hwnd );
         break;
      }
}

/* This function handles the Event Log Options dialog
 */
BOOL EXPORT CALLBACK EventLogOptions( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = EventLogOptions_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the EventLogOptions
 * dialog.
 */
LRESULT FASTCALL EventLogOptions_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, EventLogOptions_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, EventLogOptions_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsEVENTLOGOPTIONS );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL EventLogOptions_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   WORD wMaxEventLogLines;
   WORD wPermittedEvents;
   HWND hwndEdit;
   HWND hwndSpin;

   /* Get the current settings.
    */
   wPermittedEvents = Amevent_GetPermittedEventsMask();
   wMaxEventLogLines = Amevent_GetEventLogMaximumSize();

   /* Enable the permitted events options.
    */
   CheckDlgButton( hwnd, IDD_DISKFILE, wPermittedEvents & ETYP_DISKFILE );
   CheckDlgButton( hwnd, IDD_DATABASE, wPermittedEvents & ETYP_DATABASE );
   CheckDlgButton( hwnd, IDD_COMMS, wPermittedEvents & ETYP_COMMS );
   CheckDlgButton( hwnd, IDD_INITTERM, wPermittedEvents & ETYP_INITTERM );
   CheckDlgButton( hwnd, IDD_MAINTENANCE, wPermittedEvents & ETYP_MAINTENANCE );
   CheckDlgButton( hwnd, IDD_ERROR, wPermittedEvents & ETYP_SEVERE );
   CheckDlgButton( hwnd, IDD_WARNING, wPermittedEvents & ETYP_WARNING );
   CheckDlgButton( hwnd, IDD_MESSAGE, wPermittedEvents & ETYP_MESSAGE );

   /* Set the edit field with the maximum events which can
    * be recorded.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
   SetDlgItemInt( hwnd, IDD_EDIT, wMaxEventLogLines, FALSE );
   Edit_LimitText( hwndEdit, 5 );

   /* Initialise the up-down control.
    */
   hwndSpin = GetDlgItem( hwnd, IDD_SPIN );
   SendMessage( hwndSpin, UDM_SETBUDDY, (WPARAM)hwndEdit, 0L );
   SendMessage( hwndSpin, UDM_SETRANGE, 0, MAKELPARAM( 30000, 1 ) );
   SendMessage( hwndSpin, UDM_SETPOS, 9, MAKELPARAM( wMaxEventLogLines, 0 ) );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL EventLogOptions_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDOK: {
         WORD wMaxEventLogLines;
         WORD wPermittedEvents;
         int iMaxEventLogLines;

         /* Get the new event log maximum size.
          */
         if( !GetDlgInt( hwnd, IDD_EDIT, (int *)&iMaxEventLogLines, 1, 30000 ) )
            break;
         wMaxEventLogLines = LOWORD( iMaxEventLogLines );

         /* Get the new event options.
          */
         wPermittedEvents = 0;
         if( IsDlgButtonChecked( hwnd, IDD_DISKFILE ) ) wPermittedEvents |= ETYP_DISKFILE;
         if( IsDlgButtonChecked( hwnd, IDD_DATABASE ) ) wPermittedEvents |= ETYP_DATABASE;
         if( IsDlgButtonChecked( hwnd, IDD_COMMS ) ) wPermittedEvents |= ETYP_COMMS;
         if( IsDlgButtonChecked( hwnd, IDD_INITTERM ) ) wPermittedEvents |= ETYP_INITTERM;
         if( IsDlgButtonChecked( hwnd, IDD_MAINTENANCE ) ) wPermittedEvents |= ETYP_MAINTENANCE;
         if( IsDlgButtonChecked( hwnd, IDD_ERROR ) ) wPermittedEvents |= ETYP_SEVERE;
         if( IsDlgButtonChecked( hwnd, IDD_WARNING ) ) wPermittedEvents |= ETYP_WARNING;
         if( IsDlgButtonChecked( hwnd, IDD_MESSAGE ) ) wPermittedEvents |= ETYP_MESSAGE;

         /* Call the event manager to store the new settings.
          */
         Amevent_SetPermittedEventsMask( wPermittedEvents );
         Amevent_SetEventLogMaximumSize( wMaxEventLogLines );
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}
