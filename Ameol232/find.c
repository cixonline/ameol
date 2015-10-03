/* FIND.C - Handles the Find and Replace commands
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

#define  THIS_FILE   __FILE__

#include "main.h"
#include "palrc.h"
#include "resource.h"
#include "amlib.h"
#include <memory.h>
#include "editmap.h"

static char FAR szFindWhat[ 80 ] = "";       /* Current search string */
static char FAR szReplaceWhat[ 80 ] = "";    /* Current replace string */

static int iSearchOffset = 0;             /* Current search offset */

UINT EXPORT CALLBACK FindReplaceHook( HWND, UINT, WPARAM, LPARAM );

/* This function handles the Find command.
 */
void FASTCALL CmdFind( HWND hwnd )
{
   HWND hwndEdit;
   HEDIT hedit;

   /* Make sure there is a window with the
    * focus.
    */
   if( hFindDlg )
      BringWindowToTop( hFindDlg );
   else if( ( hwndEdit = GetFocus() ) != NULL )
      if( ( hedit = Editmap_Open( hwndEdit ) ) != ETYP_NOTEDIT )
         {
         static FINDREPLACE fr;

         iSearchOffset = 0;
         memset(&fr, 0, sizeof(FINDREPLACE));
         fr.lStructSize = sizeof(FINDREPLACE);
         fr.hwndOwner = hwnd;
         fr.lpstrFindWhat = szFindWhat;
         fr.wFindWhatLen = sizeof(szFindWhat);
         fr.Flags = FR_ENABLEHOOK|FR_HIDEUPDOWN;
         fr.lCustData = (LPARAM)(LPSTR)hwndEdit;
         fr.lpfnHook = FindReplaceHook;
         hFindDlg = FindText( &fr );
         }
}

/* This function handles the Replace command.
 */
void FASTCALL CmdReplace( HWND hwnd )
{
   HWND hwndEdit;
   HEDIT hedit;

   /* Make sure there is a window with the
    * focus.
    */
   if( hFindDlg )
      BringWindowToTop( hFindDlg );
   else if( ( hwndEdit = GetFocus() ) != NULL )
      if( ( hedit = Editmap_Open( hwndEdit ) ) != ETYP_NOTEDIT )
         {
         static FINDREPLACE fr;

         iSearchOffset = 0;
         memset(&fr, 0, sizeof(FINDREPLACE));
         fr.lStructSize = sizeof(FINDREPLACE);
         fr.hwndOwner = hwnd;
         fr.lpstrFindWhat = szFindWhat;
         fr.wFindWhatLen = sizeof(szFindWhat);
         fr.lpstrReplaceWith = szReplaceWhat;
         fr.wReplaceWithLen = sizeof(szReplaceWhat);
         fr.Flags = FR_ENABLEHOOK|FR_HIDEUPDOWN;
         fr.lCustData = (LPARAM)(LPSTR)hwndEdit;
         fr.lpfnHook = FindReplaceHook;
         hFindDlg = ReplaceText( &fr );
         }
}

/* This function searches for the next occurrence of the specified
 * text in the specified edit window.
 */
BOOL FASTCALL CmdFindNext( FINDREPLACE FAR * lpfr )
{
   HWND hwndEdit;
   int iPos;
   BOOL fFound;
   UINT cbEdit;
   LPSTR lpstrDoc;

   INITIALISE_PTR(lpstrDoc);

   /* Get a pointer to the edit document
    */
   hwndEdit = (HWND)lpfr->lCustData;
   cbEdit = Edit_GetTextLength( hwndEdit );
   if( fNewMemory( &lpstrDoc, cbEdit + 1 ) )
      {
      Edit_GetText( hwndEdit, lpstrDoc, cbEdit );
      lpstrDoc[ cbEdit ] = '\0';

      /* Search the document for the find string
       */
      iPos = FStrMatch( lpstrDoc + iSearchOffset,
                        lpfr->lpstrFindWhat,
                        (lpfr->Flags & FR_MATCHCASE) != 0,
                        (lpfr->Flags & FR_WHOLEWORD) != 0 );

      /* Select the found string
       */
      if( iPos != -1 )
         {
         iPos = iSearchOffset + iPos;
         Edit_SetSel( hwndEdit, iPos, iPos + lstrlen( lpfr->lpstrFindWhat ) );
         Edit_ScrollCaret( hwndEdit );
         MakeEditSelectionVisible( GetParent( lpfr->hwndOwner ), hwndEdit );
         iSearchOffset = iPos + lstrlen( lpfr->lpstrFindWhat );
         fFound = TRUE;
         }
      else
         {
         /* No match found.
          */
         iSearchOffset = 0;
         fFound = FALSE;
         }
      FreeMemory( &lpstrDoc );
      }
   else
      {
      OutOfMemoryError( hwndEdit, FALSE, FALSE );
      fFound = FALSE;
      }
   return( fFound );
}

/* This function searches for the next occurrence of the specified
 * text in the specified edit window and replaces it with the
 * replace text.
 */
BOOL FASTCALL CmdReplaceText( FINDREPLACE FAR * lpfr )
{
   HWND hwndEdit;
   DWORD dwSel;

   hwndEdit = (HWND)lpfr->lCustData;
   dwSel = Edit_GetSel( hwndEdit );
   if( LOWORD( dwSel ) != HIWORD( dwSel ) )
      Edit_ReplaceSel( hwndEdit, lpfr->lpstrReplaceWith );
   return( CmdFindNext( lpfr ) );
}

/* This function is a callback for all messages sent to the Find and
 * Replace dialogs.
 */
UINT EXPORT CALLBACK FindReplaceHook( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   switch( msg )
      {
      case WM_INITDIALOG:
         return( TRUE );
      }
   return( FALSE );
}
