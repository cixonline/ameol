/* EDITRSM.C - Edit my own resume
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
#include <dos.h>
#include <ctype.h>
#include <memory.h>
#include <string.h>
#include "print.h"
#include "amlib.h"
#include "cix.h"
#include <io.h>
#include "ameol2.h"
#include "editmap.h"

#ifndef USEBIGEDIT
#include "scintilla.h"
#include "scilexer.h"
#endif USEBIGEDIT

#define  THIS_FILE   __FILE__

extern BOOL fWordWrap;                 /* TRUE if we word wrap messages */

char NEAR szEditResumeClass[] = "amctl_editrsmcls";
char NEAR szEditResumeName[] = "EditResume";

static BOOL fDefDlgEx = FALSE;

static BOOL fRegistered = FALSE;       /* TRUE if we've registered the edit resume window */
static WNDPROC lpEditCtrlProc = NULL;  /* Edit control window procedure address */

typedef struct tagEDITRESUMEINFO {
   LPOB lpob;
   HWND hwndFocus;
   LPSTR lpszLastOn;
} EDITRESUMEINFO, FAR * LPEDITRESUMEINFO;

#define  GetEditResumeInfo(h)    (LPEDITRESUMEINFO)GetWindowLong((h),DWL_USER)
#define  SetEditResumeInfo(h,p)  SetWindowLong((h),DWL_USER,(long)(p))

BOOL FASTCALL CreateEditResume( HWND, RESUMEOBJECT FAR *, LPOB );
BOOL FASTCALL EditResume_OnCreate( HWND, LPCREATESTRUCT );
void FASTCALL EditResume_OnClose( HWND );
void FASTCALL EditResume_OnSize( HWND, UINT, int, int );
void FASTCALL EditResume_OnMove( HWND, int, int );
void FASTCALL EditResume_MDIActivate( HWND, BOOL, HWND, HWND );
void FASTCALL EditResume_OnCommand( HWND, int, HWND, UINT );
void FASTCALL EditResume_OnSetFocus( HWND, HWND );
void FASTCALL EditResume_OnAdjustWindows( HWND, int, int );
LRESULT FASTCALL EditResume_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr );

void FASTCALL CompactEditFormatText( LPSTR );

LRESULT EXPORT CALLBACK EditResumeProc( HWND, UINT, WPARAM, LPARAM );
LRESULT EXPORT CALLBACK EditResumeEditProc( HWND, UINT, WPARAM, LPARAM );

/* This function edits my own resume.
 */
void FASTCALL CmdEditResume( HWND hwnd )
{
   RESUMEOBJECT ao;
   BOOL fEditResume;

   /* Create the pathname to the resume file.
    */
   wsprintf( lpPathBuf, "%s\\", pszResumesDir );
   GetResumeFilename( szCIXNickname, lpPathBuf + strlen( lpPathBuf ) );

   /* Is resume available? If not, offer to retrieve it
    * first.
    */
   fEditResume = TRUE;
   if( !Amfile_QueryFile( lpPathBuf ) )
      {
      Amob_New( OBTYPE_RESUME, &ao );
      Amob_CreateRefString( &ao.recUserName, szCIXNickname );
      Amob_CreateRefString( &ao.recFileName, lpPathBuf );
      if( Amob_Find( OBTYPE_RESUME, &ao ) )
         {
         wsprintf( lpTmpBuf, GS(IDS_STR396), szCIXNickname );
         fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
         fEditResume = FALSE;
         }
      else
         {
         wsprintf( lpTmpBuf, GS(IDS_STR395), szCIXNickname );
         if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION ) == IDYES )
            {
            Amob_Commit( NULL, OBTYPE_RESUME, &ao );
            fEditResume = FALSE;
            }
         }
      Amob_Delete( OBTYPE_RESUME, &ao );
      }

   /* Create a new PUTRESUME object.
    */
   if( fEditResume )
      {
      LPOB lpob;

      Amob_New( OBTYPE_PUTRESUME, &ao );
      Amob_CreateRefString( &ao.recUserName, szCIXNickname );
      Amob_CreateRefString( &ao.recFileName, lpPathBuf );
      if( NULL != ( lpob = Amob_Find( OBTYPE_PUTRESUME, &ao ) ) )
         {
         OBINFO obinfo;

         Amob_GetObInfo( lpob, &obinfo );
         CreateEditResume( hwndFrame, obinfo.lpObData, lpob );
         }
      else
         CreateEditResume( hwndFrame, &ao, lpob );
      Amob_Delete( OBTYPE_PUTRESUME, &ao );
      }
}

/* This function creates the resume editor window.
 */
BOOL FASTCALL CreateEditResume( HWND hwnd, RESUMEOBJECT FAR * lproInit, LPOB lpob )
{
   LPEDITRESUMEINFO lperi;
   HWND hwndEditRsm;
   HWND hwndFocus;
   DWORD dwState;
   int cxBorder;
   int cyBorder;
   RECT rc;

   /* Register the group list window class if we have
    * not already done so.
    */
   if( !fRegistered )
      {
      WNDCLASS wc;

      wc.style          = CS_HREDRAW | CS_VREDRAW;
      wc.lpfnWndProc    = EditResumeProc;
      wc.hIcon          = LoadIcon( hRscLib, MAKEINTRESOURCE(IDI_RESUMES) );
      wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
      wc.lpszMenuName   = NULL;
      wc.cbWndExtra     = DLGWINDOWEXTRA;
      wc.cbClsExtra     = 0;
      wc.hbrBackground  = (HBRUSH)( COLOR_APPWORKSPACE + 1 );
      wc.lpszClassName  = szEditResumeClass;
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
   ReadProperWindowState( szEditResumeName, &rc, &dwState );

   /* Create the window.
    */
   hwndEditRsm = Adm_CreateMDIWindow( szEditResumeName, szEditResumeClass, hInst, &rc, dwState, (LPARAM)lproInit );
   if( NULL == hwndEditRsm )
      return( FALSE );

   /* Store the handle of the destination structure.
    */
   lperi = GetEditResumeInfo( hwndEditRsm );
   lperi->lpob = lpob;

   /* Show the window.
    */
   SendMessage( hwndMDIClient, WM_MDIACTIVATE, (WPARAM)hwndEditRsm, 0L );

   /* Determine where we put the focus.
    */
   hwndFocus = GetDlgItem( hwndEditRsm, IDD_EDIT );
   SetFocus( hwndFocus );
   lperi->hwndFocus = hwndFocus;
   return( TRUE );
}

/* This is the main window procedure.
 */
LRESULT EXPORT CALLBACK EditResumeProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_CREATE, EditResume_OnCreate );
      HANDLE_MSG( hwnd, WM_CLOSE, EditResume_OnClose );
      HANDLE_MSG( hwnd, WM_SIZE, EditResume_OnSize );
      HANDLE_MSG( hwnd, WM_MOVE, EditResume_OnMove );
      HANDLE_MSG( hwnd, WM_ERASEBKGND, Common_OnEraseBkgnd );
      HANDLE_MSG( hwnd, WM_MDIACTIVATE, EditResume_MDIActivate );
      HANDLE_MSG( hwnd, WM_COMMAND, EditResume_OnCommand );
      HANDLE_MSG( hwnd, WM_MENUSELECT, MainWnd_OnMenuSelect );
      HANDLE_MSG( hwnd, WM_SETFOCUS, EditResume_OnSetFocus );
      HANDLE_MSG( hwnd, WM_ADJUSTWINDOWS, EditResume_OnAdjustWindows );
      HANDLE_MSG( hwnd, WM_CHANGEFONT, Common_ChangeFont );
      HANDLE_MSG( hwnd, WM_NOTIFY, EditResume_OnNotify );

      case WM_APPCOLOURCHANGE:
      case WM_TOGGLEHEADERS: { // 2.55.2031
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
         lpMinMaxInfo->ptMinTrackSize.x = 220;
         lpMinMaxInfo->ptMinTrackSize.y = 120;
         return( lResult );
         }

      default:
         break;
      }
   return( Adm_DefMDIDlgProc( hwnd, message, wParam, lParam ) );
}

/* This function handles the WM_SETFOCUS message.
 */
void FASTCALL EditResume_OnSetFocus( HWND hwnd, HWND hwndOldFocus )
{
   LPEDITRESUMEINFO lperi;
   HWND hwndFocus;

   lperi = GetEditResumeInfo( hwnd );
   hwndFocus = lperi->hwndFocus;
   if( hwndFocus )
      SetFocus( hwndFocus );
   iActiveMode = WIN_EDITS;
}

/* This function handles the WM_CREATE message.
 */
BOOL FASTCALL EditResume_OnCreate( HWND hwnd, LPCREATESTRUCT lpCreateStruct )
{
   LPMDICREATESTRUCT lpMDICreateStruct;
   LPEDITRESUMEINFO lperi;
   RESUMEOBJECT FAR * lpro;
   HNDFILE fh;

   INITIALISE_PTR(lperi);

   /* Create the dialog window.
    */
   lpMDICreateStruct = (LPMDICREATESTRUCT)lpCreateStruct->lpCreateParams;
   lpro = (RESUMEOBJECT FAR *)lpMDICreateStruct->lParam;
   Adm_MDIDialog( hwnd, MAKEINTRESOURCE(IDDLG_EDITRESUME), lpMDICreateStruct );

   /* Set the edit window font.
    */
#ifdef USEBIGEDIT
   SetWindowFont( GetDlgItem( hwnd, IDD_EDIT ), hEditFont, FALSE );
#else USEBIGEDIT
   SetEditorDefaults(GetDlgItem( hwnd, IDD_EDIT ), FALSE, lfEditFont, fWordWrap, fHotLinks , fMsgStyles, crEditText, crEditWnd );
#endif USEBIGEDIT

   /* Create a structure to store the mail window info.
    */
   if( !fNewMemory( &lperi, sizeof(EDITRESUMEINFO) ) )
      return( FALSE );
   lperi->lpszLastOn = NULL;
   SetEditResumeInfo( hwnd, lperi );

   /* Subclass the edit control so we can do indents.
    */
   lpEditCtrlProc = SubclassWindow( GetDlgItem( hwnd, IDD_EDIT ), EditResumeEditProc );

   /* Setup the WordWrap function !!SM!!
   */
#ifdef SMWORDBREAK
   SendDlgItemMessage(hwnd, IDD_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)(EDITWORDBREAKPROC)WordBreakProc) ;
#endif SMWORDBREAK


   /* Fill the edit control with the current resume
    * contents.
    */
   if( HNDFILE_ERROR != ( fh = Amfile_Open( DRF(lpro->recFileName), AOF_READ ) ) )
      {
      DWORD dwFileSize;
      LPSTR lpszText;

      INITIALISE_PTR(lpszText);
      dwFileSize = Amfile_GetFileSize( fh );
      if( 0 < dwFileSize )
         if( !fNewMemory( &lpszText, LOWORD(dwFileSize) + 1 ) )
            OutOfMemoryError( hwnd, FALSE, FALSE );
         else
            {
            if( LOWORD(dwFileSize) != Amfile_Read( fh, lpszText, LOWORD(dwFileSize) ) )
               DiskReadError( hwnd, DRF(lpro->recFileName), FALSE, FALSE );
            else
               {
               LPSTR lpszStart;

               /* Locate the " was last on " line and skip to the third line
                * if found.
                */
               lpszText[ LOWORD(dwFileSize) ] = '\0';
               lpszStart = lpszText;
               if( strstr( lpszStart, " was last on " ) )
                  {
                  lpszStart = strstr( lpszText, "\r\n\r\n" );
                  if( NULL != lpszStart )
                     {
                     int length;

                     /* Make a copy of the first two lines.
                      */
                     lpszStart += 4;
                     length = lpszStart - lpszText;
                     if( fNewMemory( &lperi->lpszLastOn, length + 1 ) )
                        {
                        memcpy( lperi->lpszLastOn, lpszText, length );
                        lperi->lpszLastOn[ length ] = '\0';
                        }
                     }
                  else
                     lpszStart = lpszText;
                  }
               lpszStart = ExpandUnixTextToPCFormat( lpszStart );
               SetEditText( hwnd, NULL, IDD_EDIT, lpszStart, FALSE );
               }
            FreeMemory( &lpszText );
            }
      Amfile_Close( fh );
      }
   Amob_FreeRefObject( &lpro->recFileName );
#ifndef USEBIGEDIT
   SendMessage( GetDlgItem( hwnd, IDD_EDIT ), SCI_SETSAVEPOINT,  (WPARAM)0, (LPARAM)0L ); 
#endif USEBIGEDIT
   return( TRUE );
}

/* This function handles indents in the edit window.
 */
LRESULT EXPORT CALLBACK EditResumeEditProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
   return( EditCtrlIndent( lpEditCtrlProc, hwnd, msg, wParam, lParam ) );
}

LRESULT FASTCALL EditResume_OnNotify( HWND hwnd, int idCode, LPNMHDR lpNmHdr )
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
void FASTCALL EditResume_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_HELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsEDITRESUME );
         break;

      case IDM_QUICKPRINT:
      case IDM_PRINT:
         CmdComposedPrint( hwnd );
         break;

      case IDD_EDIT:
         if( codeNotify == EN_SETFOCUS )
            {
            LPEDITRESUMEINFO lperi;

            lperi = GetEditResumeInfo( hwnd );
            lperi->hwndFocus = hwndCtl;
            }
         break;

      case IDM_SPELLCHECK:
         Ameol2_SpellCheckDocument( hwnd, GetDlgItem( hwnd, IDD_EDIT ), SC_FL_DIALOG );
         break;

      case IDOK: {
         LPEDITRESUMEINFO lperi;
         RESUMEOBJECT FAR * lpro;
         RESUMEOBJECT ao;
         HWND hwndEdit;
         LPSTR lpText;
         HNDFILE fh;
#ifndef USEBIGEDIT
         HEDIT hedit;
#endif USEBIGEDIT

         INITIALISE_PTR(lpText);

         /* Ask whether to update the resume?
          */
         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         if( EditMap_GetModified( hwndEdit ) )
            if( fMessageBox( hwnd, 0, GS(IDS_STR416), MB_YESNO|MB_ICONINFORMATION ) == IDNO )
               break;

         Edit_SetSel( hwndEdit, 0, 0 );

         /* First spell check the document.
          */
         if( fAutoSpellCheck && EditMap_GetModified( hwndEdit ) )
            {
            if( Ameol2_SpellCheckDocument( hwnd, hwndEdit, 0 ) == SP_CANCEL )
               break;
            }

         /* Dereference the RESUMEOBJECT data structure.
          */
         lperi = GetEditResumeInfo( hwnd );
         if( NULL == lperi->lpob )
            {
            /* Create a new PUTRESUME object.
             */
            lpro = &ao;
            Amob_New( OBTYPE_PUTRESUME, &ao );

            /* Create the pathname to the resume file.
             */
            wsprintf( lpPathBuf, "%s\\", pszResumesDir );
            GetResumeFilename( szCIXNickname, lpPathBuf + strlen( lpPathBuf ) );

            /* Finally set the PUTRESUME object fields.
             */
            Amob_CreateRefString( &ao.recUserName, szCIXNickname );
            Amob_CreateRefString( &ao.recFileName, lpPathBuf );
            Amob_SaveRefObject( &ao.recUserName );
            Amob_SaveRefObject( &ao.recFileName );
            }
         else
            {
            OBINFO obinfo;

            Amob_GetObInfo( lperi->lpob, &obinfo );
            lpro = obinfo.lpObData;
            }

         /* Get and update the Text field.
          */
         if( HNDFILE_ERROR != ( fh = Amfile_Create( DRF(lpro->recFileName), 0 ) ) )
            {
            UINT cbRsmLength;
            UINT cbTextLength;

            /* Recreate the " was last on " line.
             */
            hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
#ifdef USEBIGEDIT
            Edit_FmtLines( hwndEdit, TRUE );
#else USEBIGEDIT
            hedit = Editmap_Open( hwndEdit );
            Editmap_FormatLines( hedit, hwndEdit);
#endif USEBIGEDIT
            cbTextLength = Edit_GetTextLength( hwndEdit );
            cbRsmLength = cbTextLength;
            if( lperi->lpszLastOn )
               cbRsmLength += strlen( lperi->lpszLastOn );
            if( fNewMemory( &lpText, cbRsmLength + 1 ) )
               {
               /* Save new edited resumes.
                */
               lpText[ 0 ] = '\0';
               if( lperi->lpszLastOn )
                  strcpy( lpText, lperi->lpszLastOn );
               Edit_GetText( hwndEdit, lpText + strlen( lpText ), cbTextLength + 1 );
               CompactEditFormatText( lpText );
               if( cbRsmLength != Amfile_Write( fh, lpText, cbRsmLength ) )
                  DiskWriteError( hwnd, DRF(lpro->recFileName), FALSE, FALSE );
               FreeMemory( &lpText );
               }
            Amfile_Close( fh );
            }
         Amob_Commit( lperi->lpob, OBTYPE_PUTRESUME, lpro );

         /* If resumes window open on this user's resume, update it.
          */
         if( NULL != hwndResumes )
            {
            char szName[ 16 ];

            GetResumeUserName( hwndResumes, szName, sizeof(szName) );
            if( strcmp( szName, szCIXNickname ) == 0 )
               CmdShowResume( hwndResumes, szCIXNickname, TRUE );
            }

         /* Finally destroy editor window.
          */
         Adm_DestroyMDIWindow( hwnd );
         break;
         }

      case IDCANCEL:
         /* Destroy the article window.
          */
         EditResume_OnClose( hwnd );
         break;
      }
}

/* This function compacts edit text which has lines delimited
 * with CRCRLF to CRLF
 */
void FASTCALL CompactEditFormatText( LPSTR lpSrc )
{
   LPSTR lpDest;

   lpDest = lpSrc;
   while( *lpSrc )
      if( *lpSrc == 13 && *(lpSrc + 1) == 13 && *(lpSrc + 2) == 10 )
         {
         *lpDest++ = 13;
         *lpDest++ = 10;
         lpSrc += 3;
         }
      else
         *lpDest++ = *lpSrc++;
   *lpDest = '\0';
}

/* This function handles the WM_ADJUSTWINDOWS message.
 */
void FASTCALL EditResume_OnAdjustWindows( HWND hwnd, int dx, int dy )
{
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_EDIT ), dx, dy );
   Adm_InflateWnd( GetDlgItem( hwnd, IDD_PAD1 ), dx, 0 );
   Adm_MoveWnd( GetDlgItem( hwnd, IDOK ), dx / 2, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDD_PAD1 ), 0, dy );
   Adm_MoveWnd( GetDlgItem( hwnd, IDCANCEL ), dx / 2, dy );
   SetEditWindowWidth( GetDlgItem( hwnd, IDD_EDIT ), iWrapCol );
}

/* This function handles the WM_CLOSE message.
 */
void FASTCALL EditResume_OnClose( HWND hwnd )
{
   LPEDITRESUMEINFO lperi;
   BOOL fModified;

   /* Set fModified to TRUE if any of the input fields have been
    * modified.
    */
   fModified = EditMap_GetModified( GetDlgItem( hwnd, IDD_EDIT ) );

   /* If anything modified, then get confirmation before closing
    * the dialog.
    */
   if( fModified )
      {
      int nRetCode;
      int nCode;

      if( hwndActive != hwnd )
         ShowWindow( hwnd, SW_RESTORE );
      nCode = fQuitting ? MB_YESNO : MB_YESNOCANCEL;
      nRetCode = fMessageBox( hwnd, 0, GS(IDS_STR418), nCode|MB_ICONQUESTION );
      if( nRetCode == IDCANCEL )
         {
         fQuitting = FALSE;
         return;
         }
      if( nRetCode == IDYES )
         {
         SendDlgCommand( hwnd, IDOK, 0 );
         return;
         }
      }
   lperi = GetEditResumeInfo( hwnd );
   if( NULL != lperi->lpob )
      Amob_Uncommit( lperi->lpob );
   Adm_DestroyMDIWindow( hwnd );
}

/* This function handles the WM_MOVE message. It saves the new
 * window state to the configuration file.
 */
void FASTCALL EditResume_OnMove( HWND hwnd, int x, int y )
{
   Amuser_WriteWindowState( szEditResumeName, hwnd );
   FORWARD_WM_MOVE( hwnd, x, y, Adm_DefMDIDlgProc );
}

/* This function handles the WM_SIZE message.
 */
void FASTCALL EditResume_OnSize( HWND hwnd, UINT state, int cx, int cy )
{
   /* Save the new window state to the configuration file.
    */
   Amuser_WriteWindowState( szEditResumeName, hwnd );
   FORWARD_WM_SIZE( hwnd, state, cx, cy, Adm_DefMDIDlgProc );
}

/* This function handles the WM_MDIACTIVATE message.
 */
void FASTCALL EditResume_MDIActivate( HWND hwnd, BOOL fActive, HWND hwndActivate, HWND hwndDeactivate )
{
   Amuser_CallRegistered( AE_MDI_WINDOW_ACTIVATED, MAKELPARAM( fActive, WIN_EDITS ), (LPARAM)(LPSTR)hwnd );
   FORWARD_WM_MDIACTIVATE( hwnd, fActive, hwndActivate, hwndDeactivate, Adm_DefMDIDlgProc );
}

/* This is the Put Resume out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_PutResume( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   RESUMEOBJECT FAR * lpro;

   lpro = (RESUMEOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_EDITABLE:
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_DESCRIBE:
         strcpy( lpBuf, GS(IDS_STR246) );
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_NEW:
         _fmemset( lpro, 0, sizeof( RESUMEOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         RESUMEOBJECT gpl;

         INITIALISE_PTR(lpro);
         Amob_LoadDataObject( fh, &gpl, sizeof( RESUMEOBJECT ) );
         if( fNewMemory( &lpro, sizeof( RESUMEOBJECT ) ) )
            {
            gpl.recUserName.hpStr = NULL;
            gpl.recFileName.hpStr = NULL;
            *lpro = gpl;
            }
         return( (LRESULT)lpro );
         }

      case OBEVENT_EDIT: {
         OBINFO obinfo;

         Amob_GetObInfo( (LPOB)dwData, &obinfo );
         CreateEditResume( hwndFrame, obinfo.lpObData, (LPOB)dwData );
         return( TRUE );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpro->recUserName );
         Amob_SaveRefObject( &lpro->recFileName );
         return( Amob_SaveDataObject( fh, lpro, sizeof( RESUMEOBJECT ) ) );

      case OBEVENT_COPY: {
         RESUMEOBJECT FAR * lproNew;
      
         INITIALISE_PTR( lproNew );
         lpro = (RESUMEOBJECT FAR *)dwData;
         if( fNewMemory( &lproNew, sizeof( RESUMEOBJECT ) ) )
            {
            INITIALISE_PTR( lproNew->recUserName.hpStr );
            INITIALISE_PTR( lproNew->recFileName.hpStr );
            Amob_CopyRefObject( &lproNew->recUserName, &lpro->recUserName );
            Amob_CopyRefObject( &lproNew->recFileName, &lpro->recFileName );
            }
         return( (LRESULT)lproNew );
         }

      case OBEVENT_FIND: {
         RESUMEOBJECT FAR * lpro1;
         RESUMEOBJECT FAR * lpro2;

         lpro1 = (RESUMEOBJECT FAR *)dwData;
         lpro2 = (RESUMEOBJECT FAR *)lpBuf;
         return( strcmp( DRF(lpro1->recUserName), DRF(lpro2->recUserName) ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpro );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpro->recUserName );
         Amob_FreeRefObject( &lpro->recFileName );
         return( TRUE );
      }
   return( 0L );
}
