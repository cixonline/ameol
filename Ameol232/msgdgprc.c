/* MSGDGPRC.C - MessageBox dialog procedure
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
#include <string.h>

static BOOL fDefDlgEx = FALSE;

static int nHelpCode;
static DWORD dwGlbTimeout;
static int idGlbDef;

BOOL FASTCALL MsgDlgBoxProc_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL MsgDlgBoxProc_OnCommand( HWND, int, HWND, UINT );
void FASTCALL MsgDlgBoxProc_OnTimer( HWND, UINT );
LRESULT FASTCALL MsgDlgBoxProc_DlgProc( HWND, UINT, WPARAM, LPARAM );

/* This function handles the new style Ameol message box, complete with
 * Help button.
 */
int FASTCALL fDlgMessageBox( HWND hwnd, int nContext, int idDlg, LPSTR lpStr, DWORD dwTimeout, int idDef )
{
   /* If the frame window is minimised, flash it until the user
    * restores the window.
    */
   if( !fQuitting && IsIconic( hwndFrame ) )
      {
      DWORD dwTime;

      SetTimer( hwndFrame, TID_FLASHICON, 500, NULL );
      dwTime = GetTickCount();
      while( IsIconic( hwndFrame ) )
         {
         if( dwTimeout && ( GetTickCount() - dwTime ) > dwTimeout )
            return( idDef );
         TaskYield();
         }
      KillTimer( hwndFrame, TID_FLASHICON );
      }

   /* Save the timeout globally as we can't pass it to the
    * dialog procedure.
    */
   dwGlbTimeout = dwTimeout;
   idGlbDef = idDef;

   /* Display the message dialog box.
    */
   nHelpCode = nContext;
   HideIntroduction();
   return( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(idDlg), (DLGPROC)MsgDlgBoxProc, (LPARAM)lpStr ) );
}

/* This function handles the Message Box dialog.
 */
BOOL EXPORT CALLBACK MsgDlgBoxProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, MsgDlgBoxProc_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the MsgDlgBoxProc
 * dialog.
 */
LRESULT FASTCALL MsgDlgBoxProc_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, MsgDlgBoxProc_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, MsgDlgBoxProc_OnCommand );
      HANDLE_MSG( hwnd, WM_TIMER, MsgDlgBoxProc_OnTimer );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, nHelpCode );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL MsgDlgBoxProc_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   /* Set the dialog title.
    */
   SetWindowText( hwnd, szAppName );

   /* If any text is supplied, use it to fill out the
    * dialog message.
    */
   if( lParam )
      {
      LPSTR lpPad;
      HWND hwndPad;
      UINT cbPad;

      INITIALISE_PTR(lpPad);

      hwndPad = GetDlgItem( hwnd, IDD_PAD1 );
      cbPad = GetWindowTextLength( hwndPad ) + strlen( (LPSTR)lParam ) + 1;
      if( fNewMemory( &lpPad, cbPad ) )
         {
         GetWindowText( hwndPad, lpPad, cbPad );
         wsprintf( lpTmpBuf, lpPad, (LPSTR)lParam );
         SetWindowText( hwndPad, lpTmpBuf );
         FreeMemory( &lpPad );
         }
      }

   /* Start a timer running if necessary.
    */
   if( 0 != dwGlbTimeout )
      SetTimer( hwnd, 98, (UINT)dwGlbTimeout, NULL );
   return( TRUE );
}

/* This function handles the WM_TIMER message.
 * A timeout has occurred, so return the default command ID.
 */
void FASTCALL MsgDlgBoxProc_OnTimer( HWND hwnd, UINT id )
{
   PostDlgCommand( hwnd, idGlbDef, 0L );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL MsgDlgBoxProc_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   if( 0 != dwGlbTimeout )
      KillTimer( hwnd, 98 );
   switch( id ) 
   {
      case IDYES:
         {
            EndDialog( hwnd, IDYES );
            return;
         }
      case IDNO:
         {
            EndDialog( hwnd, IDNO );
            return;
         }
      case IDD_RENAME:
         {
            EndDialog( hwnd, IDD_RENAME );
            return;
         }
   }
   EndDialog( hwnd, id );
}
