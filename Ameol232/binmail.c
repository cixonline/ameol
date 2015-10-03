/* BINMAIL.C - Send files by binary mail
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

#include <string.h>
#include "main.h"
#include "palrc.h"
#include "resource.h"
#include "ameol2.h"
#include "amaddr.h"
#include "shared.h"
#include "cix.h"

#define  THIS_FILE   __FILE__

static BOOL fDefDlgEx = FALSE;   /* DefDlg recursion flag trap */

static char szFileName[ LEN_CIXFILENAME+1 ];
static char szDirectory[ _MAX_DIR ];

BOOL EXPORT CALLBACK BinMailDlg( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL BinMailDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL BinMailDlg_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL BinMailDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL FASTCALL CheckAttachmentSize( HWND, LPSTR, int );

/* This function handles the Send File command.
 */
BOOL FASTCALL CmdBinmail( HWND hwnd, LPSTR lpFilename )
{
   BINMAILOBJECT bmo;
   CURMSG cur;

   /* Create a new binmail object type.
    */
   Amob_New( OBTYPE_BINMAIL, &bmo );

   /* If a filename is supplied, use that.
    */
   if( lpFilename )
      Amob_CreateRefString( &bmo.recFileName, lpFilename );

   /* If a message window is open, take the author of the message as
    * the default recipient of this file.
    */
   Ameol2_GetCurrentMsg( &cur );
   if( NULL != cur.pth )
      {
      MSGINFO msginfo;

      Amdb_GetMsgInfo( cur.pth, &msginfo );
      Amob_CreateRefString( &bmo.recUserName, msginfo.szAuthor );
      }

   /* Default options.
    */
   bmo.wFlags = BINF_SENDCOVERNOTE;
   if( NULL == lpFilename )
      bmo.wFlags |= BINF_UPLOAD;
   bmo.wEncodingScheme = ENCSCH_BINARY;

   /* Fire up the dialog.
    */
   if( Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_BINMAIL), BinMailDlg, (LPARAM)(LPSTR)&bmo ) )
      {
      if( !Amob_Find( OBTYPE_BINMAIL, &bmo ) )
         Amob_Commit( NULL, OBTYPE_BINMAIL, &bmo );
      }
   Amob_Delete( OBTYPE_BINMAIL, &bmo );
   return( TRUE );
}

/* This function handles the Binmail dialog box.
 */
BOOL EXPORT CALLBACK BinMailDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = BinMailDlg_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the BinMailDlg
 * dialog.
 */
LRESULT FASTCALL BinMailDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, BinMailDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, BinMailDlg_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsBINMAIL );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL BinMailDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   BINMAILOBJECT FAR * lpbmo;
   HWND hwndFile;
   HWND hwndUser;

   /* Save the handle to the BINMAILOBJECT object.
    */
   SetWindowLong( hwnd, DWL_USER, lParam );
   lpbmo = (BINMAILOBJECT FAR *)lParam;

   /* If a filename is supplied, then show the filename in the edit field
    * and hide the Mail Directory picker button. In this mode, the user
    * selects the file from his/her hard disk. If no filename is supplied
    * then the user is picking the filename from the Mail Directory (ie.
    * the file already exists in his/her CIX mail directory).
    */
   if( lpbmo->wFlags & BINF_UPLOAD )
      {
      ShowWindow( GetDlgItem( hwnd, IDD_FILENAMEBTN ), SW_SHOW );
      ShowWindow( GetDlgItem( hwnd, IDD_PAD2 ), SW_HIDE );
      EnableControl( hwnd, IDD_FILENAMEBTN, TRUE );
      }
   else
      {
      ShowWindow( GetDlgItem( hwnd, IDD_BMFORMAT ), SW_HIDE );
      ShowWindow( GetDlgItem( hwnd, IDD_FILENAMEBTN ), SW_HIDE );
      ShowWindow( GetDlgItem( hwnd, IDD_PAD2 ), SW_SHOW );
      ShowWindow( GetDlgItem( hwnd, IDD_ENCODING ), SW_HIDE );
//    EnableControl( hwnd, IDD_FILENAMEBTN, FALSE );
      ShowWindow( GetDlgItem( hwnd, IDD_UUENCODE ), FALSE );
      }

   /* Fill in the filename field.
    */
   hwndFile = GetDlgItem( hwnd, IDD_FILENAME );
   Edit_SetText( hwndFile, DRF(lpbmo->recFileName) );
   Edit_LimitText( hwndFile, _MAX_PATH );

   /* Disable filename field if sending from mail directory */

   EnableControl( hwnd, IDD_FILENAME, ( lpbmo->wFlags & BINF_UPLOAD ) );

   /* Fill in the username field.
    */
   hwndUser = GetDlgItem( hwnd, IDD_USERNAME );
   Edit_SetText( hwndUser, DRF(lpbmo->recUserName) );
   Edit_LimitText( hwndUser, LEN_AUTHORLIST );

   /* Fill the list of available encoding schemes.
    */
   FillEncodingScheme( hwnd, IDD_ENCODING, lpbmo->wEncodingScheme, FALSE, FALSE );

   /* Set focus to username IF we have filename
    */
   if( Edit_GetTextLength( hwndFile ) )
      hwndFocus = hwndUser;

   /* Always send cover note.
    */
   CheckDlgButton( hwnd, IDD_SENDCOVER, lpbmo->wFlags & BINF_SENDCOVERNOTE );
   CheckDlgButton( hwnd, IDD_DELETE, lpbmo->wFlags & BINF_DELETE );

   /* Disable OK button unless both filenamd and recipient fields are
    * filled in.
    */
   EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndUser ) && Edit_GetTextLength( hwndFile ) );
   SetFocus( hwndFocus );
   return( FALSE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL BinMailDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_SENDTO: {
         AMADDRPICKLIST aPickList;
         HWND hwndEdit;
         LPSTR lpszTo;
         int cbEdit;

         /* Initialise To field.
          */
         aPickList.lpszTo = lpszTo = NULL;
         aPickList.wFlags = AMPCKFLG_NOCC;

         /* Fill To array
          */
         hwndEdit = GetDlgItem( hwnd, IDD_USERNAME );
         if( ( cbEdit = Edit_GetTextLength( hwndEdit ) ) > 0 )
            if( fNewMemory( &lpszTo, cbEdit + 1 ) )
               {
               Edit_GetText( hwndEdit, lpszTo, cbEdit + 1 );
               aPickList.lpszTo = MakeCompactString( lpszTo );
               FreeMemory( &lpszTo );
               lpszTo = aPickList.lpszTo;
               }

         /* Throw up picker dialog.
          */
         if( Amaddr_PickEntry( hwnd, &aPickList ) )
            {
            /* Set the To string.
             */
            if( aPickList.lpszTo )
               {
               UncompactString( aPickList.lpszTo );
               SetWindowText( hwndEdit, aPickList.lpszTo );
               FreeMemory( &aPickList.lpszTo );
               }
            }

         /* Whatever happens, deallocate our own strings.
          */
         if( lpszTo )
            FreeMemory( &lpszTo );
         break;
         }

      case IDD_FILENAMEBTN: {
         HWND hwndEdit;

         BEGIN_PATH_BUF;
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_FILENAME ) );
         Edit_GetText( hwndEdit, lpPathBuf, _MAX_PATH+1 );
         if( CommonGetOpenFilename( hwnd, lpPathBuf, "Send File", NULL, "CachedBinmailDir" ) )
            {
            Edit_SetText( hwndEdit, lpPathBuf );
            EnableControl( hwnd, IDOK,
               Edit_GetTextLength( GetDlgItem( hwnd, IDD_FILENAME ) ) > 0 &&
               Edit_GetTextLength( GetDlgItem( hwnd, IDD_USERNAME ) ) > 0 );
            }
         END_PATH_BUF;
         break;
         }

      case IDD_UUENCODE: {
         BOOL f;

         f = IsDlgButtonChecked( hwnd, id );
         EnableControl( hwnd, IDD_DELETE, !f );
         EnableControl( hwnd, IDD_SENDCOVER, !f );
         break;
         }

      case IDD_FILENAME:
      case IDD_USERNAME:
         if( codeNotify == EN_CHANGE )
            EnableControl( hwnd, IDOK,
               Edit_GetTextLength( GetDlgItem( hwnd, IDD_FILENAME ) ) > 0 &&
               Edit_GetTextLength( GetDlgItem( hwnd, IDD_USERNAME ) ) > 0 );
         break;

      case IDOK: {
         char szUserName[ LEN_AUTHORLIST+1 ];
         char sz[ _MAX_PATH ];
         BINMAILOBJECT FAR * lpbmo;
         register int c;
         DWORD dwSize;
         BOOL fUpload;
         BOOL fFail;

         /* Get the binmail object handle.
          */
         lpbmo = (BINMAILOBJECT FAR *)GetWindowLong( hwnd, DWL_USER );

         /* Get the encoding scheme.
          */
         if( lpbmo->wFlags & BINF_UPLOAD )
            lpbmo->wEncodingScheme = GetEncodingScheme( hwnd, IDD_ENCODING );
         else 
            lpbmo->wEncodingScheme = ENCSCH_BINARY;

         /* Get the filename and ensure that it is valid.
          */
         Edit_GetText( GetDlgItem( hwnd, IDD_FILENAME ), sz, sizeof( sz ) );
         if( lpbmo->wEncodingScheme == ENCSCH_BINARY )
            if( !CheckValidCixFilename( hwnd, sz ) )
               {
               HighlightField( hwnd, IDD_FILENAME );
               break;
               }

         /* Check and warn user if attachment is too large.
          */
         if( ( lpbmo->wFlags & BINF_UPLOAD ) && lpbmo->wEncodingScheme != ENCSCH_BINARY )
            if( !CheckAttachmentSize( hwnd, sz, lpbmo->wEncodingScheme ) )
               break;

         /* Make sure that the file being binmailed exists if we're sending
          * locally.
          */
         if( lpbmo->wFlags & BINF_UPLOAD )
            if( !VerifyFilenamesExist( hwnd, sz ) )
               {
               HighlightField( hwnd, IDD_FILENAME );
               break;
               }

         /* Get the recipient name(s). There may be multiple recipients, but if
          * any are external to CIX, set the uuencode flag.
          */
         fFail = FALSE;
         Edit_GetText( GetDlgItem( hwnd, IDD_USERNAME ), szUserName, LEN_AUTHORLIST+1 );
         for( c = 0; !fFail && szUserName[ c ]; )
            {
            char szName[ LEN_INTERNETNAME+1 ];
            char szGroupName[ LEN_INTERNETNAME+1 ];
            char * pExt;
            int i;

            /* Extract one address.
             */
            while( szUserName[ c ] == ' ' )
               ++c;
            for( i = 0; szUserName[ c ] && szUserName[ c ] != ' '; ++c )
               if( i < LEN_INTERNETNAME )
                  szName[ i++ ] = szUserName[ c ];
            szName[ i ] = '\0';
            strcpy( szGroupName, szName );
            StripLeadingTrailingQuotes( szGroupName );

            if( Amaddr_IsGroup( szGroupName ) )
            {
            int index;

            for( index = 0; Amaddr_ExpandGroup( szGroupName, index, szName ); ++index )
               {

               /* If this is a qualified address, check it is a local CIX domain
                * before permitting binary binmail.
                */
            if( NULL != ( pExt = strchr( szName, '@' ) ) )
               if( !IsCompulinkDomain( ++pExt ) )
                  {
                  if( !( lpbmo->wFlags & BINF_UPLOAD ) )
                     {
                     /* External recipient and we're sending from the
                      * mail directory. This isn't possible!
                      */
                     fMessageBox( hwnd, 0, GS(IDS_STR1131), MB_OK|MB_ICONINFORMATION );
                     fFail = TRUE;
                     break;
                     }
                  if( lpbmo->wEncodingScheme != ENCSCH_UUENCODE )
                     fMessageBox( hwnd, 0, GS(IDS_STR1124), MB_OK|MB_ICONINFORMATION );
                  lpbmo->wEncodingScheme = ENCSCH_UUENCODE;
                  ++c;
                  }
               else
                  {
                  register int d;

                  for( d = c; szUserName[ c ] && szUserName[ c ] != ' '; ++c );
                  strcpy( szUserName + d, szUserName + c );
                  c = d;
                  }
            
               }
            }
            
            else
            {
            /* If this is a qualified address, check it is a local CIX domain
             * before permitting binary binmail.
             */
            if( NULL != ( pExt = strchr( szName, '@' ) ) )
               if( !IsCompulinkDomain( ++pExt ) )
                  {
                  if( !( lpbmo->wFlags & BINF_UPLOAD ) )
                     {
                     /* External recipient and we're sending from the
                      * mail directory. This isn't possible!
                      */
                     fMessageBox( hwnd, 0, GS(IDS_STR1131), MB_OK|MB_ICONINFORMATION );
                     fFail = TRUE;
                     break;
                     }
                  if( lpbmo->wEncodingScheme != ENCSCH_UUENCODE )
                     fMessageBox( hwnd, 0, GS(IDS_STR1124), MB_OK|MB_ICONINFORMATION );
                  lpbmo->wEncodingScheme = ENCSCH_UUENCODE;
                  ++c;
                  }
               else
                  {
                  register int d;

                  for( d = c; szUserName[ c ] && szUserName[ c ] != ' '; ++c );
                  strcpy( szUserName + d, szUserName + c );
                  c = d;
                  }
            }
            while( szUserName[ c ] == ' ' ) ++c;
            }
         if( fFail )
            break;

         /* If we're sending the file from our CIX mail directory,
          * get the file size.
          */
         dwSize = 0L;
         fUpload = FALSE;
         if( lpbmo->wFlags & BINF_UPLOAD )
            fUpload = TRUE;
         else
            dwSize = GetFileSizeFromMailDirectory( sz );

         /* Okay, we got all the info we need, so create a MAILOBJECT
          * object and initialise it.
          */
         Amob_CreateRefString( &lpbmo->recUserName, szUserName );
         Amob_CreateRefString( &lpbmo->recFileName, sz );
         lpbmo->dwSize = dwSize;

         /* Set the binmail flags.
          */
         lpbmo->wFlags = 0;
         if( IsDlgButtonChecked( hwnd, IDD_SENDCOVER ) )
            lpbmo->wFlags |= BINF_SENDCOVERNOTE;
         if( IsDlgButtonChecked( hwnd, IDD_DELETE ) )
            lpbmo->wFlags |= BINF_DELETE;
         if( fUpload )
            lpbmo->wFlags |= BINF_UPLOAD;
         EndDialog( hwnd, TRUE );
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}

/* Scan thru each filename and make sure that they
 * all exist.
 */
BOOL FASTCALL VerifyFilenamesExist( HWND hwnd, LPSTR lpFilename )
{
   while( *lpFilename )
      {
      lpFilename = ExtractFilename( lpPathBuf, lpFilename );
      if( !Amfile_QueryFile( lpPathBuf ) )
         {
         wsprintf( lpTmpBuf, GS(IDS_STR273), lpPathBuf );
         fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
         return( FALSE );
         }
      }
   return( TRUE );
}

/* This is the Send File out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_Binmail( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   BINMAILOBJECT FAR * lpbmo;

   lpbmo = (BINMAILOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_EDITABLE:
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_EXEC:
         return( POF_HELDBYSCRIPT );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR233), DRF(lpbmo->recFileName), DRF(lpbmo->recUserName) );
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( lpbmo->wEncodingScheme == ENCSCH_BINARY ? Amob_GetNextObjectID() : 2 );

      case OBEVENT_NEW:
         memset( lpbmo, 0, sizeof( BINMAILOBJECT ) );
         lpbmo->wEncodingScheme = ENCSCH_BINARY;
         return( TRUE );

      case OBEVENT_LOAD: {
         BINMAILOBJECT bmo;
      
         INITIALISE_PTR(lpbmo);
         Amob_LoadDataObject( fh, &bmo, sizeof( BINMAILOBJECT ) );
         if( fNewMemory( &lpbmo, sizeof( BINMAILOBJECT ) ) )
            {
            *lpbmo = bmo;
            lpbmo->recFileName.hpStr = NULL;
            lpbmo->recUserName.hpStr = NULL;
            lpbmo->recText.hpStr = NULL;
            }
         return( (LRESULT)lpbmo );
         }

      case OBEVENT_EDIT: {
         OBINFO obinfo;

         Amob_GetObInfo( (LPOB)dwData, &obinfo );
         Adm_Dialog( hInst, hwndFrame, MAKEINTRESOURCE(IDDLG_BINMAIL), BinMailDlg, (LPARAM)obinfo.lpObData );
         Amob_Commit( (LPOB)dwData, OBTYPE_BINMAIL, obinfo.lpObData );
         return( TRUE );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpbmo->recFileName );
         Amob_SaveRefObject( &lpbmo->recUserName );
         Amob_SaveRefObject( &lpbmo->recText );
         return( Amob_SaveDataObject( fh, lpbmo, sizeof( BINMAILOBJECT ) ) );

      case OBEVENT_COPY: {
         BINMAILOBJECT FAR * lpbmoNew;

         INITIALISE_PTR( lpbmoNew );
         if( fNewMemory( &lpbmoNew, sizeof( BINMAILOBJECT ) ) )
            {
            INITIALISE_PTR( lpbmoNew->recFileName.hpStr );
            INITIALISE_PTR( lpbmoNew->recUserName.hpStr );
            INITIALISE_PTR( lpbmoNew->recText.hpStr );
            Amob_CopyRefObject( &lpbmoNew->recFileName, &lpbmo->recFileName );
            Amob_CopyRefObject( &lpbmoNew->recUserName, &lpbmo->recUserName );
            Amob_CopyRefObject( &lpbmoNew->recText, &lpbmo->recText );
            lpbmoNew->dwSize = lpbmo->dwSize;
            lpbmoNew->wFlags = lpbmo->wFlags;
            lpbmoNew->wEncodingScheme = lpbmo->wEncodingScheme;
            }
         return( (LRESULT)lpbmoNew );
         }

      case OBEVENT_FIND: {
         BINMAILOBJECT FAR * lpbmo1;
         BINMAILOBJECT FAR * lpbmo2;

         lpbmo1 = (BINMAILOBJECT FAR *)dwData;
         lpbmo2 = (BINMAILOBJECT FAR *)lpBuf;
         if( strcmp( Amob_DerefObject( &lpbmo1->recUserName ), Amob_DerefObject( &lpbmo2->recUserName ) ) != 0 )
            return( FALSE );
         return( strcmp( Amob_DerefObject( &lpbmo1->recFileName ), Amob_DerefObject( &lpbmo2->recFileName ) ) == 0 );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpbmo );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpbmo->recUserName );
         Amob_FreeRefObject( &lpbmo->recFileName );
         Amob_FreeRefObject( &lpbmo->recText );
         return( TRUE );
      }
   return( 0L );
}
