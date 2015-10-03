/* BUILDSCR.C - Builds an Ameol script
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
#include <memory.h>
#include <string.h>
#include "cix.h"
#include <io.h>
#include <commdlg.h>
#include "intcomms.h"
#include "lbf.h"

extern BOOL fIsAmeol2Scratchpad;

static BOOL fDefDlgEx = FALSE;

static char FAR szFileName[ _MAX_PATH ];
static char FAR szOutFileName[ _MAX_PATH ];
static char FAR szInBuf[ 150 ];
static char FAR szOutBuf[ 300 ];

typedef struct tagINPUT {
   char szCaption[ 40 ];
   char szMessage[ 100 ];
   LPSTR lpszBuf;
   int cMax;
} INPUT;

typedef INPUT FAR * LPINPUT;

BOOL EXPORT CALLBACK Input( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL Input_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL Input_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL Input_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL FASTCALL BuildScript( HWND, LPSCRIPT, LPSTR, BOOL FAR * );
BOOL FASTCALL ScrStrInput( HWND, char **, char ** );
BOOL FASTCALL ScrPickConference( HWND, char **, char ** );
BOOL FASTCALL ScrOpenFileProc( HWND, char **, char ** );
char * FASTCALL LoadParm( char *, char *, char *, int );
void FASTCALL MakePath( LPSTR, LPSTR, LPSTR );

/* This function compiles the specified script and stores it in the 
 * out-basket or executes the script immediately.
 */
BOOL FASTCALL CompileScript( HWND hwnd, LPSCRIPT lpScript )
{
   BOOL fInline;
   BOOL fOk;

   if( fOk = BuildScript( hwnd, lpScript, szOutFileName, &fInline ) )
      if( fInline )
         {
         INLINEOBJECT ino;
   
         Amob_New( OBTYPE_INLINE, &ino );
         Amob_CreateRefString( &ino.recFileName, szOutFileName );
         Amob_CreateRefString( &ino.recDescription, lpScript->szDescription );
         Amob_Commit( NULL, OBTYPE_INLINE, &ino );
         Amob_Delete( OBTYPE_INLINE, &ino );
         }
      else
         {
         INCLUDEOBJECT io;
   
         Amob_New( OBTYPE_INCLUDE, &io );
         Amob_CreateRefString( &io.recFileName, szOutFileName );
         Amob_CreateRefString( &io.recDescription, lpScript->szDescription );
         Amob_Commit( NULL, OBTYPE_INCLUDE, &io );
         Amob_Delete( OBTYPE_INCLUDE, &io );
         }
   return( fOk );
}

/* This function 'compiles' a script
 */
BOOL FASTCALL BuildScript( HWND hwnd, LPSCRIPT lpScript, LPSTR lpszOutFileName, BOOL FAR * lpfInline )
{
   BOOL fInline = TRUE;
   LPVOID hBuffer;
   HNDFILE fhOut;
   BOOL fAbort;
   HNDFILE fhIn;
   WORD wExt;

   /* Create an unique file name for the script file.
    */
   MakePath( szOutFileName, pszScriptDir, lpScript->szFileName );
   wExt = 0;
   do {
      char szExt[ 4 ];

      wsprintf( szExt, "%3.3d", wExt++ );
      ChangeExtension( szOutFileName, szExt );
      }
   while( Amfile_QueryFile( szOutFileName ) );
   if( ( fhOut = Amfile_Create( szOutFileName, 0 ) ) == HNDFILE_ERROR )
      {
      FileCreateError( hwnd, lpScript->szFileName, FALSE, FALSE );
      return( FALSE );
      }

   /* Open the source file
    */
   MakePath( szFileName, pszScriptDir, lpScript->szFileName );
   if( ( fhIn = Amfile_Open( szFileName, AOF_READ ) ) == HNDFILE_ERROR )
      {
      FileOpenError( hwnd, szFileName, FALSE, FALSE );
      return( FALSE );
      }

   hBuffer = Amlbf_Open( fhIn, AOF_READ );
   fAbort = FALSE;
   while( !fAbort && Amlbf_Read( hBuffer, szInBuf, sizeof( szInBuf ) - 1, NULL, NULL, &fIsAmeol2Scratchpad ) )
      {
      char *psz, *psz2;

      psz = szInBuf;
      psz2 = szOutBuf;
      while( !fAbort && *psz )
         {
         if( *psz == '%' && *( psz + 1 ) != '%' )
            {
            switch( *++psz )
               {
               case 's':
                  ++psz;
                  if( !ScrStrInput( hwnd, &psz, &psz2 ) )
                     fAbort = TRUE;
                  break;

               case 'c':
                  ++psz;
                  if( !ScrPickConference( hwnd, &psz, &psz2 ) )
                     fAbort = TRUE;
                  break;

               case 'f':
                  ++psz;
                  if( !ScrOpenFileProc( hwnd, &psz, &psz2 ) )
                     fAbort = TRUE;
                  break;
               }
            continue;
            }
         *psz2++ = *psz++;
         }
      *psz2++ = CR;
      *psz2++ = LF;
      *psz2 = '\0';
      while( Amfile_Write( fhOut, szOutBuf, strlen( szOutBuf ) ) != strlen( szOutBuf ) )
         if( !DiskWriteError( hwnd, szOutFileName, FALSE, FALSE ) )
            {
            fAbort = TRUE;
            break;
            }
      if( !IsInlineToken( szOutBuf ) )
         fInline = FALSE;
      }
   Amlbf_Close( hBuffer );
   Amfile_Close( fhOut );
   if( fAbort )
      {
      Amfile_Delete( szOutFileName );
      return( FALSE );
      }
   strcpy( lpszOutFileName, szOutFileName );
   if( lpfInline )
      *lpfInline = fInline;
   return( TRUE );
}

BOOL FASTCALL ScrStrInput( HWND hwnd, char * * pnpsz, char * * pnpsz2 )
{
   char * psz = *pnpsz;
   char * psz2 = * pnpsz2;
   INPUT ip;

   /* Skip to the start of the first parameter
    */
   if( *psz == '(' )
      ++psz;

   /* Get the parameters.
    */
   psz = LoadParm( psz, ip.szCaption, GS(IDS_STR333), sizeof( ip.szCaption ) );
   psz = LoadParm( psz, ip.szMessage, "", sizeof( ip.szMessage ) );

   /* Now call the standard input dialog
    */
   ip.lpszBuf = psz2;
   ip.cMax = 80;
   if( !Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_INPUT), Input, (LPARAM)(LPSTR)&ip ) )
      return( FALSE );

   /* Skip to the end of the parameter list
    */
   psz2 += strlen( psz2 );
   while( *psz && *psz != ')' ) ++psz;
   if( *psz )
      ++psz;
   *pnpsz = psz;
   *pnpsz2 = psz2;
   return( TRUE );
}

BOOL FASTCALL ScrPickConference( HWND hwnd, char * * pnpsz, char * * pnpsz2 )
{
   char * psz = *pnpsz;
   char * psz2 = * pnpsz2;
   char szType[ 2 ];
   PICKER cp;

   /* Skip to the start of the first parameter
    */
   if( *psz == '(' )
      ++psz;

   /* Get the parameters.
    */
   psz = LoadParm( psz, cp.szCaption, GS(IDS_STR334), 40 );
   psz = LoadParm( psz, szType, "1", 2 );

   /* Call the Conference Picker to pick a conference
    */
   cp.wType = ( *szType == '1' ) ? CPP_TOPICS : CPP_CONFERENCES;
   cp.wHelpID = idsFORUMPICKER;
   if( hwndMsg )
      {
      LPWINDINFO lpWindInfo;

      lpWindInfo = GetBlock( hwndMsg );
      cp.ptl = lpWindInfo->lpTopic;
      cp.pcl = Amdb_FolderFromTopic( cp.ptl );
      }
   else
      {
      cp.pcl = Amdb_GetFirstFolder( Amdb_GetFirstCategory() );
      cp.ptl = cp.pcl ? Amdb_GetFirstTopic( cp.pcl ) : NULL;
      }
   if( !Amdb_ConfPicker( hwnd, &cp ) )
      return( FALSE );

   /* Skip to the end of the parameter list
    */
   strcpy( psz2, Amdb_GetFolderName( cp.pcl ) );
   if( CPP_TOPICS == cp.wType )
      {
      strcat( psz2, "/" );
      strcat( psz2, Amdb_GetTopicName( cp.ptl ) );
      }
   psz2 += strlen( psz2 );
   while( *psz && *psz != ')' )
      ++psz;
   if( *psz )
      ++psz;
   *pnpsz = psz;
   *pnpsz2 = psz2;
   return( TRUE );
}

BOOL FASTCALL ScrOpenFileProc( HWND hwnd, char * * pnpsz, char * * pnpsz2 )
{
   static char strFilters[] = "All Files (*.*)\f*.*\f";
   char * psz2 = * pnpsz2;
   char szCaption[ 40 ];
   char szFilters[ 80 ];
   char * psz = *pnpsz;
   char szType[ 10 ];
   OPENFILENAME ofn;
   register int c;
   DWORD dwFlags;
   char * dummy;
   BOOL fSave;

   /* First letter after 'f' determines whether we're a Save or Open dialog.
    */
   fSave = ( *psz++ == 's' );

   /* Skip to the start of the first parameter
    */
   if( *psz == '(' ) ++psz;

   /* Get the parameters.
    */
   psz = LoadParm( psz, szCaption, GS(IDS_STR335), 40 );
   psz = LoadParm( psz, szType, "0", 10 );
   psz = LoadParm( psz, szFilters, "", 60 );

   /* Initialise from the parameters 
    */
   *psz2 = '\0';
   dwFlags = OFN_NOCHANGEDIR|OFN_HIDEREADONLY;
   if( fSave )
      dwFlags |= OFN_OVERWRITEPROMPT|OFN_CREATEPROMPT|OFN_PATHMUSTEXIST;
   else
      dwFlags |= OFN_FILEMUSTEXIST;
   dwFlags |= strtoul( szType, &dummy, 0 );

   /* Fix up the filters string by replacing all \f characters with NULLs.
    */
   strcat( szFilters, strFilters );
   for( c = 0; szFilters[ c ]; ++c )
      if( szFilters[ c ] == '\f' )
         szFilters[ c ] = '\0';

   /* Get the filename
    */
   ofn.lStructSize = sizeof( OPENFILENAME );
   ofn.hwndOwner = hwnd;
   ofn.hInstance = NULL;
   ofn.lpstrFilter = szFilters;
   ofn.lpstrCustomFilter = NULL;
   ofn.nMaxCustFilter = 0;
   ofn.nFilterIndex = 1;
   ofn.lpstrFile = psz2;
   ofn.nMaxFile = 144;
   ofn.lpstrFileTitle = NULL;
   ofn.nMaxFileTitle = 0;
   ofn.lpstrInitialDir = NULL;
   ofn.lpstrTitle = szCaption;
   ofn.Flags = dwFlags;
   ofn.nFileOffset = 0;
   ofn.nFileExtension = 0;
   ofn.lpstrDefExt = NULL;
   ofn.lCustData = 0;
   ofn.lpfnHook = NULL;
   ofn.lpTemplateName = 0;
   if( fSave ) {
      if( !GetSaveFileName( &ofn ) )
         return( FALSE );
      }
   else {
      if( !GetOpenFileName( &ofn ) )
         return( FALSE );
      }

   /* Skip to the end of the parameter list */
   psz2 += strlen( psz2 );
   while( *psz && *psz != ')' ) ++psz;
   if( *psz )
      ++psz;
   *pnpsz = psz;
   *pnpsz2 = psz2;
   return( TRUE );
}

/* This function retreives one parameter from the parameter list pointed to by the psz
 * parameter. If no parameter appears at the list, the pszDefault parameter is returned
 * instead.
 */
char * FASTCALL LoadParm( char * psz, char * pszBuf, char * pszDefault, int max )
{
   register int c;

   strcpy( pszBuf, pszDefault );
   while( *psz == ' ' || *psz == '\t' )
      ++psz;
   if( *psz == ')' )
      return( psz );
   if( *psz != ',' ) {
      register char ch;

      if( *psz == '\'' )
         ++psz;
      for( c = 0; ( ch = *psz ) && ch != '\''; )
         {
         if( ch == '\\' && *( psz + 1 ) )
            {
            switch( ch = *++psz ) {
               case 'f':   ch = 12;    break;
               case 'n':   ch = 10;    break;
               case 'r':   ch = 13;    break;
               case '0':   ch = 12;    break;
               }
            }
         if( c < max - 1 )
            pszBuf[ c++ ] = ch;
         ++psz;
         }
      if( ch == '\'' )
         ++psz;
      if( *psz == ',' )
         ++psz;
      pszBuf[ c ] = '\0';
      return( psz );
      }
   ++psz;
   return( psz );
}

/* This function handles the Input dialog.
 */
BOOL EXPORT CALLBACK Input( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, Input_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the Input
 * dialog.
 */
LRESULT FASTCALL Input_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, Input_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, Input_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsINPUT );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL Input_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   LPINPUT lpi;
   HWND hwndEdit;

   lpi = (LPINPUT)lParam;
   SetWindowText( hwnd, lpi->szCaption );
   SetDlgItemText( hwnd, IDD_PAD1, lpi->szMessage );

   hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
   lpTmpBuf[ 0 ] = '\0';
   if( hwndTopic )
      GetMarkedName( hwndTopic, lpTmpBuf, LEN_TEMPBUF, FALSE );
   Edit_LimitText( hwndEdit, lpi->cMax - 1 );
   Edit_SetText( hwndEdit, lpTmpBuf );
   Edit_SetSel( hwndEdit, 0, 32767 );
   SetFocus( hwndEdit );
   EnableControl( hwnd, IDOK, *lpTmpBuf != '\0' );

   SetWindowLong( hwnd, DWL_USER, lParam );
   return( FALSE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL Input_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_EDIT:
         if( codeNotify == EN_UPDATE )
            EnableControl( hwnd, IDOK, Edit_GetTextLength( GetDlgItem( hwnd, IDD_EDIT ) ) > 0 );
         break;

      case IDOK: {
         LPINPUT lpi;
         register int c, d;

         lpi = (LPINPUT)GetWindowLong( hwnd, DWL_USER );
         Edit_GetText( GetDlgItem( hwnd, IDD_EDIT ), lpi->lpszBuf, lpi->cMax );
         for( c = d = 0; lpi->lpszBuf[ c ]; ++c )
            if( lpi->lpszBuf[ c ] != CR && lpi->lpszBuf[ c ] != LF )
               lpi->lpszBuf[ d++ ] = lpi->lpszBuf[ c ];
         lpi->lpszBuf[ d ] = '\0';
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* Given a filename, this function checks whether or not the filename has
 * a path (ie. it is qualified) and if not, it prepends lpszDir to make
 * it a qualified path.
 */
void FASTCALL MakePath( LPSTR lpszPath, LPSTR lpszDir, LPSTR lpszFName )
{
   register int c;

   for( c = 0; lpszFName[ c ]; ++c )
      if( lpszFName[ c ] == '\\' || lpszFName[ c ] == ':' )
         {
         strcpy( lpszPath, lpszFName );
         return;
         }
   if (lpszDir[strlen(lpszDir) - 1] == '\\')
      wsprintf( lpszPath, "%s%s", lpszDir, lpszFName );
   else
      wsprintf( lpszPath, "%s\\%s", lpszDir, lpszFName );
}

/* This is the Inline out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_Inline( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   INLINEOBJECT FAR * lpio;

   lpio = (INLINEOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_EDITABLE:
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_EDIT: {
         OBINFO obinfo;

         Amob_GetObInfo( (LPOB)dwData, &obinfo );
         lpio = (INLINEOBJECT FAR *)obinfo.lpObData;
         wsprintf( lpTmpBuf, "notepad %s", DRF(lpio->recFileName) );
         WinExec( lpTmpBuf, SW_SHOWNORMAL );
         Amob_Uncommit( (LPOB)dwData );
         break;
         }

      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_DESCRIBE:
         strcpy( lpBuf, Amob_DerefObject( &lpio->recDescription ) );
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( 0 );

      case OBEVENT_NEW:
         _fmemset( lpio, 0, sizeof( INLINEOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         INLINEOBJECT io;

         INITIALISE_PTR(lpio);
         Amob_LoadDataObject( fh, &io, sizeof( INLINEOBJECT ) );
         if( fNewMemory( &lpio, sizeof( INLINEOBJECT ) ) )
            {
            *lpio = io;
            lpio->recFileName.hpStr = NULL;
            lpio->recDescription.hpStr = NULL;
            }
         return( (LRESULT)lpio );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpio->recFileName );
         Amob_SaveRefObject( &lpio->recDescription );
         return( Amob_SaveDataObject( fh, lpio, sizeof( INLINEOBJECT ) ) );

      case OBEVENT_COPY: {
         INLINEOBJECT FAR * lpioNew;

         INITIALISE_PTR( lpioNew );
         if( fNewMemory( &lpioNew, sizeof( INLINEOBJECT ) ) )
            {
            INITIALISE_PTR( lpioNew->recFileName.hpStr );
            INITIALISE_PTR( lpioNew->recDescription.hpStr );
            Amob_CopyRefObject( &lpioNew->recDescription, &lpio->recDescription );
            Amob_CopyRefObject( &lpioNew->recFileName, &lpio->recFileName );
            }
         return( (LRESULT)lpioNew );
         }

      case OBEVENT_FIND: {
         INLINEOBJECT FAR * lpio1;
         INLINEOBJECT FAR * lpio2;

         lpio1 = (INLINEOBJECT FAR *)dwData;
         lpio2 = (INLINEOBJECT FAR *)lpBuf;
         if( strcmp( DRF(lpio1->recDescription), DRF(lpio2->recDescription) ) != 0 )
            return( FALSE );
         return( strcmp( DRF(lpio1->recFileName), DRF(lpio2->recFileName) ) == 0 );
         }

      case OBEVENT_REMOVE:
         Amfile_Delete( DRF(lpio->recFileName) );
         FreeMemory( &lpio );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpio->recDescription );
         Amob_FreeRefObject( &lpio->recFileName );
         return( TRUE );
      }
   return( 0L );
}

/* This is the Include out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_Include( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   INCLUDEOBJECT FAR * lpio;

   lpio = (INCLUDEOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_EDITABLE:
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_EDIT: {
         OBINFO obinfo;

         Amob_GetObInfo( (LPOB)dwData, &obinfo );
         lpio = (INCLUDEOBJECT FAR *)obinfo.lpObData;
         wsprintf( lpTmpBuf, "notepad %s", DRF(lpio->recFileName) );
         WinExec( lpTmpBuf, SW_SHOWNORMAL );
         Amob_Uncommit( (LPOB)dwData );
         break;
         }

      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_DESCRIBE:
         strcpy( lpBuf, Amob_DerefObject( &lpio->recDescription ) );
         return( TRUE );

      case OBEVENT_NEW:
         _fmemset( lpio, 0, sizeof( INCLUDEOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         INCLUDEOBJECT io;

         INITIALISE_PTR(lpio);
         Amob_LoadDataObject( fh, &io, sizeof( INCLUDEOBJECT ) );
         if( fNewMemory( &lpio, sizeof( INCLUDEOBJECT ) ) )
            {
            *lpio = io;
            lpio->recFileName.hpStr = NULL;
            lpio->recDescription.hpStr = NULL;
            }
         return( (LRESULT)lpio );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpio->recFileName );
         Amob_SaveRefObject( &lpio->recDescription );
         return( Amob_SaveDataObject( fh, lpio, sizeof( INCLUDEOBJECT ) ) );

      case OBEVENT_COPY: {
         INCLUDEOBJECT FAR * lpioNew;

         INITIALISE_PTR( lpioNew );
         if( fNewMemory( &lpioNew, sizeof( INCLUDEOBJECT ) ) )
            {
            INITIALISE_PTR( lpioNew->recFileName.hpStr );
            INITIALISE_PTR( lpioNew->recDescription.hpStr );
            Amob_CopyRefObject( &lpioNew->recDescription, &lpio->recDescription );
            Amob_CopyRefObject( &lpioNew->recFileName, &lpio->recFileName );
            }
         return( (LRESULT)lpioNew );
         }

      case OBEVENT_FIND: {
         INCLUDEOBJECT FAR * lpio1;
         INCLUDEOBJECT FAR * lpio2;

         lpio1 = (INCLUDEOBJECT FAR *)dwData;
         lpio2 = (INCLUDEOBJECT FAR *)lpBuf;
         if( strcmp( DRF(lpio1->recDescription), DRF(lpio2->recDescription) ) != 0 )
            return( FALSE );
         return( strcmp( DRF(lpio1->recFileName), DRF(lpio2->recFileName) ) == 0 );
         }

      case OBEVENT_REMOVE:
         Amfile_Delete( DRF(lpio->recFileName) );
         FreeMemory( &lpio );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpio->recDescription );
         Amob_FreeRefObject( &lpio->recFileName );
         return( TRUE );
      }
   return( 0L );
}
