/* SIG.C - Implements Signatures
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
#include "amlib.h"
#include <string.h>
#include <dos.h>
#include <io.h>
#include <limits.h>
#include "ameol2.h"
#include "admin.h"
#include "editmap.h"

#define  THIS_FILE   __FILE__

static BOOL fDefDlgEx = FALSE;   /* DefDlg recursion flag trap */

char NEAR szGlobalSig[] = "GLOBAL";             /* Name of global signature */

BOOL FASTCALL NewSig_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL NewSig_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL NewSig_DlgProc( HWND, UINT, WPARAM, LPARAM );
BOOL EXPORT CALLBACK NewSig( HWND, UINT, WPARAM, LPARAM );

BOOL FASTCALL CmdSignatureDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL CmdSignatureDlg_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL CmdSignatureDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );
BOOL EXPORT CALLBACK CmdSignatureDlg( HWND, UINT, WPARAM, LPARAM );

void FASTCALL CmdSignatureDlg_DisplaySig( HWND, int );
BOOL FASTCALL CmdSignatureDlg_SaveSig( HWND, int );

/* This function displays the Signatures dialog.
 */
void FASTCALL CmdSignature( HWND hwnd, char * pDefSig )
{
   if( TestPerm(PF_CANCONFIGURE) )
      Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_SIGNATURE), CmdSignatureDlg, (LPARAM)pDefSig );
}

/* Handles the Signatures dialog.
 */
BOOL EXPORT CALLBACK CmdSignatureDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, CmdSignatureDlg_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function dispatches messages for the About dialog.
 */
LRESULT FASTCALL CmdSignatureDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, CmdSignatureDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, CmdSignatureDlg_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSIGNATURE );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL CmdSignatureDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   FINDDATA ft;
   HWND hwndList;
   HWND hwndEdit;
   register int n;
   LPSTR pDefSig;
   HFIND r;

   /* Get the default signature.
    */
   pDefSig = (LPSTR)lParam;

   /* Fill the list box with all available signatures.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   for( n = r = AmFindFirst( "*.sig", DSD_SIG, _A_NORMAL, &ft ); n != -1; n = Amuser_FindNext( r, &ft ) )
      {
      register int c;

      for( c = 0; ft.name[ c ] && ft.name[ c ] != '.'; ++c );
      ft.name[ c ] = '\0';
      ComboBox_AddString( hwndList, ft.name );
      }

   /* Select either the default signature or the first.
    */
   if( ComboBox_GetCount( hwndList ) )
      {
      int index;

      /* Show the default signature.
       */
      index = 0;
      if( *pDefSig )
         if( CB_ERR == ( index = ComboBox_FindStringExact( hwndList, -1, pDefSig ) ) )
            index = 0;
      ComboBox_SetCurSel( hwndList, index );
      CmdSignatureDlg_DisplaySig( hwnd, index );
      }
   else {
      EnableControl( hwnd, IDD_DELETE, FALSE );
      EnableControl( hwnd, IDOK, FALSE );
      }
   Amuser_FindClose( r );

   /* Set the font.
    */
   hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
   SetWindowFont( hwndEdit, hEditFont, FALSE );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL CmdSignatureDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_LIST:
         if( codeNotify == CBN_SELCHANGE )
            {
            HWND hwndList;
            HWND hwndEdit;
            int index;

            index = (int)GetWindowLong( hwnd, DWL_USER );
            hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
            if( EditMap_GetModified( hwndEdit ) )
               {
               if( !CmdSignatureDlg_SaveSig( hwnd, index ) )
                  break;
               fCancelToClose( hwnd, IDCANCEL );
               }

            hwndList = GetDlgItem( hwnd, IDD_LIST );
            index = ComboBox_GetCurSel( hwndList );
            CmdSignatureDlg_DisplaySig( hwnd, index );
            }
         break;

      case IDD_NEW: {
         char sz[ 9 ];

         if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE( IDDLG_NEWSIG ), NewSig, (LPARAM)(LPSTR)sz ) )
            {
            HWND hwndList;
            HWND hwndEdit;
            int index;

            index = (int)GetWindowLong( hwnd, DWL_USER );
            hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
            hwndList = GetDlgItem( hwnd, IDD_LIST );
            if( EditMap_GetModified( hwndEdit ) && ComboBox_GetCount( hwndList ) > 0 )
               if( !CmdSignatureDlg_SaveSig( hwnd, index ) )
                  break;

            AnsiUpper( sz );
            if( ( index = ComboBox_FindStringExact( hwndList, -1, sz ) ) == LB_ERR )
               index = ComboBox_AddString( hwndList, sz );
            EnableControl( hwnd, IDOK, TRUE );
            EnableControl( hwnd, IDD_DELETE, TRUE );
            ComboBox_SetCurSel( hwndList, index );
            CmdSignatureDlg_DisplaySig( hwnd, index );
            fCancelToClose( hwnd, IDCANCEL );
            SetFocus( hwndEdit );
            }
         break;
         }

      case IDD_DELETE: {
         HWND hwndList;
         int index;

         hwndList = GetDlgItem( hwnd, IDD_LIST );
         if( ( index = ComboBox_GetCurSel( hwndList ) ) != CB_ERR )
            {
            char sz[ _MAX_FNAME ];
            PCAT pcat;
            PCL pcl;
            PTL ptl;

            /* Get confirmation first.
             */
            if( IDNO == fMessageBox( hwnd, 0, GS(IDS_STR975), MB_YESNO|MB_ICONINFORMATION ) )
               break;

            /* Get the signature and delete the signature
             * file itself.
             */
            ComboBox_GetLBText( hwndList, index, sz );
            lstrcat( sz, ".sig" );
            Ameol2_DeleteFile( sz, DSD_SIG );

            /* Remove the signature from the listbox and display the
             * next one, if there is one.
             */
            if( ComboBox_DeleteString( hwndList, index ) == index )
               --index;
            if( index != CB_ERR )
               {
               ComboBox_SetCurSel( hwndList, index );
               CmdSignatureDlg_DisplaySig( hwnd, index );
               }
            else
               {
               InvalidateRect( hwndList, NULL, FALSE );
               UpdateWindow( hwndList );
               EnableControl( hwnd, IDD_DELETE, FALSE );
               EnableControl( hwnd, IDOK, FALSE );
               Edit_SetText( GetDlgItem( hwnd, IDD_EDIT ), "" );
               }

            /* For every topic which used the signature just deleted,
             * change it to the global signature.
             */
            ComboBox_GetLBText( hwndList, index, sz );
            for( pcat = Amdb_GetFirstCategory(); pcat; pcat = Amdb_GetNextCategory( pcat ) )
               for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
                  for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
                     {
                     TOPICINFO topicinfo;
   
                     Amdb_GetTopicInfo( ptl, &topicinfo );
                     if( lstrcmp( topicinfo.szSigFile, sz ) == 0 )
                        Amdb_SetTopicSignature( ptl, szGlobalSig );
                     }
            fCancelToClose( hwnd, IDCANCEL );
            }
         break;
         }

      case IDOK: {
         HWND hwndEdit;
         int index;

         index = (int)GetWindowLong( hwnd, DWL_USER );
         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         if( EditMap_GetModified( hwndEdit ) )
            if( !CmdSignatureDlg_SaveSig( hwnd, index ) )
               break;
         EndDialog( hwnd, TRUE );
         break;
         }

      case IDCANCEL: {
         HWND hwndEdit;

         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         if( EditMap_GetModified( hwndEdit ) )
            {
            register int r;

            r = fMessageBox( hwnd, 0, GS(IDS_STR516), MB_YESNOCANCEL|MB_ICONINFORMATION );
            if( r == IDCANCEL )
               break;
            if( r == IDYES )
               {
               int index;
         
               index = (int)GetWindowLong( hwnd, DWL_USER );
               if( !CmdSignatureDlg_SaveSig( hwnd, index ) )
                  break;
               }
            }
         EndDialog( hwnd, FALSE );
         break;
         }
      }
}

void FASTCALL CmdSignatureDlg_DisplaySig( HWND hwnd, int index )
{
   char sz[ _MAX_FNAME ];
   HWND hwndList;
   HWND hwndEdit;
   HNDFILE fh;

   hwndList = GetDlgItem( hwnd, IDD_LIST );
   hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
   ComboBox_GetLBText( hwndList, index, sz );
   lstrcat( sz, ".sig" );
   if( ( fh = Ameol2_OpenFile( sz, DSD_SIG, AOF_READ ) ) != HNDFILE_ERROR )
      {
      WORD wSize;
      LPSTR lpText;

      INITIALISE_PTR(lpText);
      wSize = LOWORD( Amfile_GetFileSize( fh ) );
      if( fNewMemory( &lpText, wSize + 1 ) )
         {
         if( Amfile_Read( fh, lpText, wSize ) != wSize )
            DiskReadError( hwnd, sz, FALSE, FALSE );
         else
            {
            lpText[ wSize ] = '\0';
            Edit_SetText( hwndEdit, lpText );
            }
         FreeMemory( &lpText );
         }
      Amfile_Close( fh );
      }
   else
      Edit_SetText( hwndEdit, "" );
   SetWindowLong( hwnd, DWL_USER, (LPARAM)index );
}

BOOL FASTCALL CmdSignatureDlg_SaveSig( HWND hwnd, int index )
{
   char sz[ _MAX_FNAME ];
   HWND hwndList;
   register int c;
   HNDFILE fh;

   hwndList = GetDlgItem( hwnd, IDD_LIST );
   ComboBox_GetLBText( hwndList, index, sz );
   for( c = 0; sz[ c ]; ++c )
      if( !ValidFileNameChr( sz[ c ] ) )
         {
         wsprintf( lpTmpBuf, GS( IDS_STR136 ), sz[ c ] );
         fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
         return( FALSE );
         }
   lstrcat( sz, ".sig" );
   if( ( fh = Ameol2_CreateFile( sz, DSD_SIG, 0 ) ) == HNDFILE_ERROR )
      fMessageBox( hwnd, 0, GS( IDS_STR137 ), MB_OK|MB_ICONEXCLAMATION );
   else
      {
      WORD wSize;
      LPSTR lpText;
      HWND hwndEdit;

      INITIALISE_PTR(lpText);
      hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
      wSize = Edit_GetTextLength( hwndEdit ) + 1;
      if( !fNewMemory( &lpText, wSize ) )
         OutOfMemoryError( hwnd, FALSE, FALSE );
      else
         {
         Edit_GetText( hwndEdit, lpText, wSize );
         if( Amfile_Write( fh, lpText, wSize ) == wSize )
            {
            FreeMemory( &lpText );
            Amfile_Close( fh );
            return( TRUE );
            }
         FreeMemory( &lpText );
         DiskWriteError( hwnd, sz, FALSE, FALSE );
         }
      Amfile_Close( fh );
      Amfile_Delete( sz );
      }
   return( FALSE );
}

/* This function handles the NewSig dialog box
 */
BOOL EXPORT CALLBACK NewSig( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, NewSig_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the Login
 *  dialog.
 */
LRESULT FASTCALL NewSig_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, NewSig_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, NewSig_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsNEWSIG );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL NewSig_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   Edit_LimitText( GetDlgItem( hwnd, IDD_EDIT ), 8 );
   EnableControl( hwnd, IDOK, FALSE );
   SetWindowLong( hwnd, DWL_USER, lParam );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL NewSig_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EDIT:
         if( codeNotify == EN_CHANGE )
            EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndCtl ) > 0 );
         break;

      case IDOK: {
         LPSTR lpText;
         HWND hwndEdit;
         register int c;

         lpText = (LPSTR)GetWindowLong( hwnd, DWL_USER );
         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         Edit_GetText( hwndEdit, lpText, 9 );
         for( c = 0; lpText[ c ]; ++c )
            if( !ValidFileNameChr( lpText[ c ] ) )
               {
               wsprintf( lpTmpBuf, GS( IDS_STR136 ), lpText[ c ] );
               fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
               break;
               }
         if( lpText[ c ] )
            {
            SetFocus( hwndEdit );
            Edit_SetSel( hwndEdit, c, c + 1 );
            Edit_ScrollCaret( hwndEdit );
            break;
            }
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* This function fills the specified combo box with a list of all
 * signatures.
 */
void FASTCALL FillSignatureList( HWND hwnd, int id )
{
   register int n;
   FINDDATA ft;
   HWND hwndList;
   HFIND r;

   VERIFY( hwndList = GetDlgItem( hwnd, id ) );
   ComboBox_ResetContent( hwndList );
   for( n = r = AmFindFirst( "*.sig", DSD_SIG, _A_NORMAL, &ft ); n != -1; n = Amuser_FindNext( r, &ft ) )
      {
      register int i;

      for( i = 0; ft.name[ i ] && ft.name[ i ] != '.'; ++i );
      ft.name[ i ] = '\0';
      ComboBox_AddString( hwndList, ft.name );
      }
   Amuser_FindClose( r );
   ComboBox_InsertString( hwndList, 0, GS(IDS_STR946) );
}

/* This function writes the appropriate signature for the specified
 * topic.
 */
void FASTCALL SetEditSignature( HWND hwndEdit, PTL ptl )
{
   LPSTR lpszSignature;
   char * pszSig;

   pszSig = NULL;
   if( NULL == ptl )
      pszSig = szGlobalSig;
   else
      {
      TOPICINFO topicinfo;

      Amdb_GetTopicInfo( ptl, &topicinfo );
      if( topicinfo.szSigFile[ 0 ] )
         pszSig = topicinfo.szSigFile;
      }
   if( NULL != pszSig )
      if( NULL != ( lpszSignature = GetEditSignature( pszSig ) ) )
         {
         DWORD dwSig1Sel;

         /* Save the caret position and append the signature to the end of the
          * current text. Then put the caret back where it was.
          */
         dwSig1Sel = Edit_GetSel( hwndEdit );
         if( fEditAtTop )
         {
         Edit_SetSel( hwndEdit, 0, 0 );
         Edit_ReplaceSel( hwndEdit, lpszSignature );
         Edit_SetSel( hwndEdit, 0, 0 );
         Edit_ScrollCaret( hwndEdit );
         }
         else
         {
         Edit_SetSel( hwndEdit, USHRT_MAX, USHRT_MAX );
         Edit_ReplaceSel( hwndEdit, lpszSignature );
         Edit_SetSel( hwndEdit, LOWORD(dwSig1Sel), HIWORD(dwSig1Sel) );
         }

         FreeMemory( &lpszSignature );
         }
}

/* This function returns the text of the specified signature, or NULL
 * if the signature could not be found or loaded.
 */
LPSTR FASTCALL GetEditSignature( char * pSig )
{
   char sz[ _MAX_FNAME ];
   LPSTR lpszSignature;
   HNDFILE fh;

   lpszSignature = NULL;
   if( 0 != strcmp( pSig, GS(IDS_STR946) ) )
      {
      wsprintf( sz, "%s.sig", (LPSTR)pSig );
      if( ( fh = Ameol2_OpenFile( sz, DSD_SIG, AOF_READ ) ) != HNDFILE_ERROR )
         {
         WORD cbSignature;
   
         cbSignature = (WORD)Amfile_GetFileSize( fh );
         if( fNewMemory( &lpszSignature, cbSignature + 8 ) )
            {
            lpszSignature[ 0 ] = CR;
            lpszSignature[ 1 ] = LF;
            lpszSignature[ 2 ] = CR;
            lpszSignature[ 3 ] = LF;
            if( Amfile_Read( fh, lpszSignature + 4, cbSignature ) != cbSignature )
               {
               DiskReadError( hwndFrame, sz, FALSE, FALSE );
               FreeMemory( &lpszSignature );
               }
            else
            {
               if( fEditAtTop )
               {
               lpszSignature[ cbSignature + 3 ] = CR;
               lpszSignature[ cbSignature + 4 ] = LF;
               lpszSignature[ cbSignature + 5 ] = CR;
               lpszSignature[ cbSignature + 6 ] = LF;
               lpszSignature[ cbSignature + 7 ] = '\0';
               }
               else
                  lpszSignature[ cbSignature + 4 ] = CR;
            }
            }
         Amfile_Close( fh );
         return( lpszSignature );
         }
      }
   if( fNewMemory( &lpszSignature, 1 ) )
      lpszSignature[ 0 ] = '\0';
   return( lpszSignature );
}

/* This function locates the old signature in the specified edit
 * control and replaces it with the new one.
 */
void FASTCALL ReplaceEditSignature( HWND hwndEdit, char * pszOldSig, char * pszNewSig, BOOL fTop )
{
   LPSTR lpszOldSigText;
   LPSTR lpszNewSigText;
   UINT cbEdit;

   /* Get some lengths. Skip the replacement if
    * the signature is smaller than the edit text.
    */
   cbEdit = Edit_GetTextLength( hwndEdit );
   if( NULL != ( lpszOldSigText = GetEditSignature( pszOldSig ) ) )
      {
      if( NULL != ( lpszNewSigText = GetEditSignature( pszNewSig ) ) )
         {
         if( strlen(lpszOldSigText) <= cbEdit )
            {
            LPSTR lpText;
         
            INITIALISE_PTR(lpText);
            if( fNewMemory( &lpText, cbEdit + 1 ) )
               {
               DWORD sel;
               int nOldSig;
               int selEnd;
               int selStart;
         
               /* Get the edit control contents.
                */
               sel = Edit_GetSel( hwndEdit );
               selStart = LOWORD(sel);
               selEnd = HIWORD(sel);
               Edit_GetText( hwndEdit, lpText, cbEdit+1 );
               nOldSig = FStrMatch( lpText, lpszOldSigText, TRUE, FALSE );
#ifdef USEBIGEDIT
               SetWindowRedraw( hwndEdit, FALSE );
#endif USEBIGEDIT
               if( -1 != nOldSig )
                  {
                  Edit_SetSel( hwndEdit, nOldSig, nOldSig + strlen(lpszOldSigText) );
                  Edit_ReplaceSel( hwndEdit, lpszNewSigText );
                  }
               else
                  {
                  /* Not found, so go to end of edit text and append new
                   * signature there.
                   */
                  if( fTop )
                     Edit_SetSel( hwndEdit, 0, 0 );
                  else
                     Edit_SetSel( hwndEdit, 32767, 32767 );
                  Edit_ReplaceSel( hwndEdit, lpszNewSigText );
                  }
               Edit_SetSel( hwndEdit, selStart, selEnd );
#ifdef USEBIGEDIT
               SetWindowRedraw( hwndEdit, TRUE );
#endif USEBIGEDIT
               UpdateWindow( hwndEdit );
               FreeMemory( &lpText );
               }
            }
         FreeMemory( &lpszNewSigText );
         }
      FreeMemory( &lpszOldSigText );
      }
}
