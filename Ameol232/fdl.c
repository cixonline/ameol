/* FDL.C - Handles the CIX file download command
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

#include "main.h"
#include "palrc.h"
#include "resource.h"
#include <ctype.h>
#include <memory.h>
#include <string.h>
#include "amlib.h"
#include "cix.h"
#include "ameol2.h"
#include "cookie.h"
#include "rules.h"
#include "webapi.h"

#define  THIS_FILE   __FILE__

static BOOL fDefDlgEx = FALSE;

BOOL FASTCALL Download_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL Download_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL Download_DlgProc( HWND, UINT, WPARAM, LPARAM );

BOOL EXPORT CALLBACK SelectFile( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL SelectFile_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL SelectFile_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL SelectFile_DlgProc( HWND, UINT, WPARAM, LPARAM );

/* This function handles the CIX File Download command.
 */
void FASTCALL CmdCIXFileDownload( HWND hwnd )
{
   CURMSG curmsg;

   if( fFileListWndActive )
      SendCommand( hwndActive, IDD_DOWNLOAD, 0L );
   else
      {
      Ameol2_GetCurrentMsg( &curmsg );
      if( NULL != curmsg.ptl )
         if( Amdb_GetTopicType( curmsg.ptl ) == FTYPE_CIX )
            {
            char szCIXFileName[ LEN_CIXFILENAME+1 ];
            char szDirectory[ _MAX_PATH ];
            TOPICINFO topicinfo;
            FDLOBJECT fd;

            /* Make sure this topic has an flist and warn otherwise.
             */
            Amdb_GetTopicInfo( curmsg.ptl, &topicinfo );
            if( !(topicinfo.dwFlags & TF_HASFILES) )
               if( fMessageBox( hwnd, 0, GS(IDS_STR425), MB_YESNO|MB_ICONQUESTION ) == IDNO )
                  return;

            /* If a filename is marked, extract it.
             */
            szCIXFileName[ 0 ] = '\0';
            if( NULL != curmsg.pth )
               {
               GetMarkedFilename( hwndTopic, szCIXFileName, LEN_CIXFILENAME+1 );
               if( szCIXFileName[ 0 ] == '\0' )
                  {
                  MSGINFO msginfo;
                  register int c;
                  BOOL fHasDot;
                  char * psz;
            
                  /* The following logic attempts to parse off a filename from the message title. It
                   * looks at whether the message begins with Title:, then skips that. Then it tries
                   * to get a valid DOS filename from the rest of the line. It has one caveat - it
                   * always expects the filename to contain a period delimiter.
                   */
                  Amdb_GetMsgInfo( curmsg.pth, &msginfo );
                  fHasDot = FALSE;
                  psz = msginfo.szTitle;
                  while( isspace( *psz ) )
                     ++psz;
                  if( strstr( psz, "new files available" ) != NULL )
                     {
                     HPSTR lpTextBuf;
                     HPSTR lpText;
                     int index;

                     /* This is a multi-file notification. Parse off every
                      * file we find and concatenate their names to lpPathBuf.
                      */
                     lpTextBuf = lpText = Amdb_GetMsgText( curmsg.pth );
                     lpPathBuf[ 0 ] = '\0';
                     while( -1 != ( index = HStrMatch( lpText, "Filename:", TRUE, TRUE ) ) )
                        {
                        lpText += index + 9;
                        while( *lpText == ' ' || *lpText == '\t' )
                           ++lpText;
                        for( c = 0; *lpText && !isspace( *lpText ) && c < LEN_CIXFILENAME; ++c )
                           if( !( *lpText == '.' || ValidFileNameChr( *lpText ) ) )
                              {
                              c = 0;
                              break;
                              }
                           else
                              szCIXFileName[ c ] = *lpText++;
                        szCIXFileName[ c ] = '\0';
                        strcat( lpPathBuf, szCIXFileName );
                        strcat( lpPathBuf, " " );
                        }
                     Amdb_FreeMsgTextBuffer( lpTextBuf );

                     /* Fire off a dialog so the user can select which file to
                      * actually download.
                      */
                     if( !Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_SELECTFILE), SelectFile, (LPARAM)lpPathBuf ) )
                        return;
                     strcpy( szCIXFileName, lpPathBuf );
                     }
                  else {
                     if( _strnicmp( psz, "title:", 6 ) == 0 )
                        {
                        psz += 6;
                        while( isspace( *psz ) )
                           ++psz;
                        }
                     for( c = 0; *psz && !isspace( *psz ) && c < LEN_CIXFILENAME; ++c )
                        /* If the character is not a valid filename chr,
                         * give up.
                         */
                        if( !( *psz == '.' || ValidFileNameChr( *psz ) ) )
                           {
                           c = 0;
                           break;
                           }
                        else
                           szCIXFileName[ c ] = *psz++;
                     szCIXFileName[ c ] = '\0';
                     }
                  }
               }

            /* Get the directory associated with this
             * filename.
             */
            GetAssociatedDir( curmsg.ptl, szDirectory, szCIXFileName );

            /* Create a new FDLOBJECT type object.
             */
            Amob_New( OBTYPE_DOWNLOAD, &fd );
            Amob_CreateRefString( &fd.recCIXFileName, szCIXFileName );
            Amob_CreateRefString( &fd.recDirectory, szDirectory );
            Amob_CreateRefString( &fd.recConfName, (LPSTR)Amdb_GetFolderName( curmsg.pcl ) );
            Amob_CreateRefString( &fd.recTopicName, (LPSTR)Amdb_GetTopicName( curmsg.ptl ) );
            if( Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_FDL), Download, (LPARAM)(LPSTR)&fd ) )
               {
               if( Amob_Find( OBTYPE_DOWNLOAD, &fd ) )
                  {
                  wsprintf( lpTmpBuf, GS(IDS_STR424), szCIXFileName );
                  fMessageBox( hwnd, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
                  }
               else
                  Amob_Commit( NULL, OBTYPE_DOWNLOAD, &fd );
               }
            Amob_Delete( OBTYPE_DOWNLOAD, &fd );
            }
      }
}

/* This is the File Download out-basket class procedure.
 */
LRESULT EXPORT CALLBACK ObProc_Download( UINT uType, HNDFILE fh, LPVOID dwData, LPSTR lpBuf )
{
   FDLOBJECT FAR * lpfdl;

   lpfdl = (FDLOBJECT FAR *)dwData;
   switch( uType )
      {
      case OBEVENT_EDITABLE:
      case OBEVENT_PERSISTENCE:
         return( TRUE );

      case OBEVENT_EXEC:
         if (fUseWebServices){ // !!SM!! 2.56.2053
            return (Am2Download(szCIXNickname, szCIXPassword, DRF(lpfdl->recConfName ), DRF(lpfdl->recTopicName ), DRF(lpfdl->recCIXFileName), DRF(lpfdl->recFileName)));
         }
         else
            return( POF_HELDBYSCRIPT );

      case OBEVENT_DESCRIBE:
         wsprintf( lpBuf, GS(IDS_STR239), DRF(lpfdl->recCIXFileName), DRF(lpfdl->recConfName ), DRF(lpfdl->recTopicName ) );
         return( TRUE );

      case OBEVENT_GETCLSID:
         return( Amob_GetNextObjectID() );

      case OBEVENT_NEW:
         memset( lpfdl, 0, sizeof( FDLOBJECT ) );
         return( TRUE );

      case OBEVENT_LOAD: {
         FDLOBJECT fdl;

         INITIALISE_PTR(lpfdl);
         Amob_LoadDataObject( fh, &fdl, sizeof( FDLOBJECT ) );
         if( fNewMemory( &lpfdl, sizeof( FDLOBJECT ) ) )
            {
            *lpfdl = fdl;
            lpfdl->recConfName.hpStr = NULL;
            lpfdl->recTopicName.hpStr = NULL;
            lpfdl->recCIXFileName.hpStr = NULL;
            lpfdl->recFileName.hpStr = NULL;
            lpfdl->recDirectory.hpStr = NULL;
            }
         return( (LRESULT)lpfdl );
         }

      case OBEVENT_EDIT: {
         OBINFO obinfo;

         Amob_GetObInfo( (LPOB)dwData, &obinfo );
         Adm_Dialog( hInst, hwndFrame, MAKEINTRESOURCE(IDDLG_FDL), Download, (LPARAM)obinfo.lpObData );
         Amob_Commit( (LPOB)dwData, OBTYPE_DOWNLOAD, obinfo.lpObData );
         return( TRUE );
         }

      case OBEVENT_SAVE:
         Amob_SaveRefObject( &lpfdl->recConfName );
         Amob_SaveRefObject( &lpfdl->recTopicName );
         Amob_SaveRefObject( &lpfdl->recCIXFileName );
         Amob_SaveRefObject( &lpfdl->recFileName );
         Amob_SaveRefObject( &lpfdl->recDirectory );
         return( Amob_SaveDataObject( fh, lpfdl, sizeof( FDLOBJECT ) ) );

      case OBEVENT_COPY: {
         FDLOBJECT FAR * lpfdlNew;

         INITIALISE_PTR( lpfdlNew );
         lpfdl = (FDLOBJECT FAR *)dwData;
         if( fNewMemory( &lpfdlNew, sizeof( FDLOBJECT ) ) )
            {
            INITIALISE_PTR( lpfdlNew->recConfName.hpStr );
            INITIALISE_PTR( lpfdlNew->recTopicName.hpStr );
            INITIALISE_PTR( lpfdlNew->recFileName.hpStr );
            INITIALISE_PTR( lpfdlNew->recCIXFileName.hpStr );
            INITIALISE_PTR( lpfdlNew->recDirectory.hpStr );
            Amob_CopyRefObject( &lpfdlNew->recConfName, &lpfdl->recConfName );
            Amob_CopyRefObject( &lpfdlNew->recTopicName, &lpfdl->recTopicName );
            Amob_CopyRefObject( &lpfdlNew->recFileName, &lpfdl->recFileName );
            Amob_CopyRefObject( &lpfdlNew->recCIXFileName, &lpfdl->recCIXFileName );
            Amob_CopyRefObject( &lpfdlNew->recDirectory, &lpfdl->recDirectory );
            lpfdlNew->wFlags = lpfdl->wFlags;
            }
         return( (LRESULT)lpfdlNew );
         }

      case OBEVENT_FIND: {
         FDLOBJECT FAR * lpfdl1;
         FDLOBJECT FAR * lpfdl2;

         lpfdl1 = (FDLOBJECT FAR *)dwData;
         lpfdl2 = (FDLOBJECT FAR *)lpBuf;
         if( strcmp( DRF(lpfdl1->recConfName), DRF(lpfdl2->recConfName) ) != 0 )
            return( FALSE );
         if( strcmp( DRF(lpfdl1->recTopicName), DRF(lpfdl2->recTopicName) ) != 0 )
            return( FALSE );
         if( strcmp( DRF(lpfdl1->recCIXFileName), DRF(lpfdl2->recCIXFileName) ) != 0 )
            return( FALSE );
         return( TRUE );
         }

      case OBEVENT_REMOVE:
         FreeMemory( &lpfdl );
         return( TRUE );

      case OBEVENT_DELETE:
         Amob_FreeRefObject( &lpfdl->recConfName );
         Amob_FreeRefObject( &lpfdl->recTopicName );
         Amob_FreeRefObject( &lpfdl->recCIXFileName );
         Amob_FreeRefObject( &lpfdl->recFileName );
         Amob_FreeRefObject( &lpfdl->recDirectory );
         return( TRUE );
      }
   return( 0L );
}

/* This function handles the File Download dialog box.
 */
BOOL EXPORT CALLBACK Download( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, Download_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the Download
 * dialog.
 */
LRESULT FASTCALL Download_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, Download_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, Download_OnCommand );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, FolderCombo_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_DRAWITEM, FolderCombo_OnDrawItem );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsFDL );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL Download_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   FDLOBJECT FAR * lpfdl;

   /* Dereference and save the pointer to the current
    * FDLOBJECT structure.
    */
   lpfdl = (FDLOBJECT FAR *)lParam;
   ASSERT( NULL != lpfdl );
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* Set some input field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_EDIT ), LEN_CIXFILENAME );
   Edit_LimitText( GetDlgItem( hwnd, IDD_DIRECTORY ), _MAX_PATH-1 );

   /* Select the Conference field.
    */
   FillConfList( hwnd, DRF(lpfdl->recConfName) );
   FillTopicList( hwnd, DRF(lpfdl->recConfName), DRF(lpfdl->recTopicName) );

   /* If lpfdl points to an existing FDLOBJECT, then
    * fill out the dialog box from the existing values.
    */
   Edit_SetText( GetDlgItem( hwnd, IDD_EDIT ), DRF(lpfdl->recCIXFileName) );
   Edit_SetText( GetDlgItem( hwnd, IDD_DIRECTORY ), DRF(lpfdl->recDirectory) );

   /* Set the Browse picture button.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_BROWSE ), hInst, IDB_FOLDERBUTTON );

   /* Disable the OK button if no filename or directory/
    */
   EnableControl( hwnd, IDOK, *lpfdl->recCIXFileName.hpStr && &lpfdl->recDirectory.hpStr );
   SetFocus( GetDlgItem( hwnd, IDD_EDIT ) );
   return( FALSE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL Download_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_CONFNAME:
         if( codeNotify == CBN_SELCHANGE )
            {
            char szTopicName[ LEN_TOPICNAME+1 ];
            char sz[ 14 ];
            HWND hwndList;
            int nSel;

            hwndList = GetDlgItem( hwnd, IDD_CONFNAME );
            nSel = ComboBox_GetCurSel( hwndList );
            ComboBox_GetLBText( hwndList, nSel, sz );
            GetDlgItemText( hwnd, IDD_TOPICNAME, szTopicName, LEN_TOPICNAME+1 );
            FillTopicList( hwnd, sz, szTopicName );
            }
         break;

      case IDD_DIRECTORY:
      case IDD_EDIT:
         if( codeNotify == EN_CHANGE )
            EnableControl( hwnd, IDOK,
               Edit_GetTextLength( GetDlgItem( hwnd, IDD_DIRECTORY ) ) > 0 &&
               Edit_GetTextLength( GetDlgItem( hwnd, IDD_EDIT ) ) > 0 );
         break;

      case IDD_BROWSE: {
         CHGDIR chgdir;
         HWND hwndEdit;

         hwndEdit = GetDlgItem( hwnd, IDD_DIRECTORY );
         if( fFileDebug )
            MessageBox( hwnd, "Got directory handle", "FileDebug", MB_OK|MB_ICONQUESTION );
         Edit_GetText( hwndEdit, chgdir.szPath, 144 );
         if( fFileDebug )
            MessageBox( hwnd, "Got edit text", "FileDebug", MB_OK|MB_ICONQUESTION );
         chgdir.hwnd = hwnd;
         strcpy( chgdir.szTitle, GS(IDS_STR539) );
         if( fFileDebug )
            MessageBox( hwnd, "strcpy title", "FileDebug", MB_OK|MB_ICONQUESTION );
         strcpy( chgdir.szPrompt, GS(IDS_STR540) );
         if( fFileDebug )
            MessageBox( hwnd, "strcpy prompt", "FileDebug", MB_OK|MB_ICONQUESTION );
         if( Amuser_ChangeDirectory( &chgdir ) )
         {
            if( fFileDebug )
               MessageBox( hwnd, "Returned from Amuser_ChangeDirectory", "FileDebug", MB_OK|MB_ICONQUESTION );
            Edit_SetText( hwndEdit, chgdir.szPath );
            if( fFileDebug )
               MessageBox( hwnd, "Set new directory text", "FileDebug", MB_OK|MB_ICONQUESTION );
         }
         break;
         }

      case IDOK: {
         char szTopicName[ LEN_TOPICNAME+1 ];
         char szConfName[ LEN_CONFNAME+1 ];
         char szFileName[ _MAX_FNAME ];
         char szDirectory[ _MAX_DIR ];
         FDLOBJECT FAR * lpfdl;
         TOPICINFO topicinfo;
         HWND hwndEdit;
         int length;
         PCL pcl;
         PTL ptl;

         /* Get the current FDLOBJECT structure.
          */
         lpfdl = (FDLOBJECT FAR *)GetWindowLong( hwnd, DWL_USER );
         ASSERT( NULL != lpfdl );

         /* Get the filename.
          */
         hwndEdit = GetDlgItem( hwnd, IDD_EDIT );
         length = Edit_GetTextLength( hwndEdit );
         if( Amob_CreateRefObject( &lpfdl->recCIXFileName, length + 1 ) )
            {
            Edit_GetText( hwndEdit, lpfdl->recCIXFileName.hpStr, length + 1 );
            Amob_SaveRefObject( &lpfdl->recCIXFileName );
            }

         /* Make sure the directory exists.
          */
         VERIFY( hwndEdit = GetDlgItem( hwnd, IDD_DIRECTORY ) );
         Edit_GetText( hwndEdit, szDirectory, _MAX_DIR );
         
         /* ADD a trailing backslash, if none exists
            or the check under Win3.x fails */

         length = strlen( szDirectory );
         if( szDirectory[ length - 1 ] != '\\' )
         {  
            strcat(szDirectory, "\\");
         }

         /* Make sure the directory exists.
          */
         if( !ValidateChangedDir( hwnd, szDirectory ) )
         break;
         
         /* Remove any trailing separator from the end of the
          * directory path.
          */
         length = strlen( szDirectory );
         if( szDirectory[ length - 1 ] == '\\' )
            szDirectory[ length - 1 ] = '\0';

         /* Save the directory.
          */
         Amob_CreateRefString( &lpfdl->recDirectory, szDirectory );

         /* Check that the topic from which we're writing this message is not
          * resigned.
          */
         GetDlgItemText( hwnd, IDD_CONFNAME, szConfName, LEN_CONFNAME+1 );
         GetDlgItemText( hwnd, IDD_TOPICNAME, szTopicName, LEN_TOPICNAME+1 );
         pcl = Amdb_OpenFolder( NULL, szConfName );
         ptl = NULL;
         topicinfo.dwFlags = 0; // Keeps compiler happy...
         if( NULL != pcl )
            {
            ptl = Amdb_OpenTopic( pcl, szTopicName );
            if( NULL != ptl )
               Amdb_GetTopicInfo( ptl, &topicinfo );
            }
         if( NULL == ptl || ( NULL != ptl && topicinfo.dwFlags & TF_RESIGNED ) )
            {
            wsprintf( lpTmpBuf, GS(IDS_STR201), szConfName, szTopicName );
            if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONQUESTION ) == IDNO )
               break;
            lpfdl->wFlags |= FDLF_JOINANDRESIGN;
            }

         /* Okay to save say, so update in out-basket.
          */
         Amob_CreateRefString( &lpfdl->recTopicName, szTopicName );
         Amob_CreateRefString( &lpfdl->recConfName, szConfName );
         Amob_SaveRefObject( &lpfdl->recConfName );
         Amob_SaveRefObject( &lpfdl->recTopicName );

         /* Create a local filename for this download.
          */
         if( !Ameol2_MakeLocalFileName( hwnd, lpfdl->recCIXFileName.hpStr, szFileName, TRUE ) )
            return;
         Amob_CreateRefString( &lpfdl->recFileName, szFileName );

         /* Check whether or not the file exists before downloading. If
          * so, set the overwrite flag.
          */
         wsprintf( lpTmpBuf, "%s\\%s", lpfdl->recDirectory.hpStr, lpfdl->recFileName.hpStr );
         if( Amfile_QueryFile( lpTmpBuf ) )
            {
            if( fMessageBox( hwnd, 0, GS(IDS_STR674), MB_YESNO|MB_ICONQUESTION ) == IDNO )
               break;
            lpfdl->wFlags |= FDLF_OVERWRITE;
            }

         /* Save directory in cookie for this topic.
          */
         if( NULL != ptl )
            Amdb_SetCookie( ptl, FDL_PATH_COOKIE, lpfdl->recDirectory.hpStr );
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* This function handles the File SelectFile dialog box.
 */
BOOL EXPORT CALLBACK SelectFile( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   CheckDefDlgRecursion( &fDefDlgEx );
   return( SetDlgMsgResult( hwnd, message, SelectFile_DlgProc( hwnd, message, wParam, lParam ) ) );
}

/* This function handles the dialog box messages passed to the SelectFile
 * dialog.
 */
LRESULT FASTCALL SelectFile_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, SelectFile_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, SelectFile_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsFDL );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL SelectFile_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   char szCIXFileName[ LEN_CIXFILENAME+1 ];
   HWND hwndList;
   LPSTR lpBuf;

   lpBuf = (LPSTR)lParam;
   SetWindowLong( hwnd, DWL_USER, (LPARAM)lpBuf );
   VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
   while( *lpBuf )
      {
      lpBuf = ExtractFilename( szCIXFileName, lpBuf );
      ListBox_AddString( hwndList, szCIXFileName );
      }
   ListBox_SetCurSel( hwndList, 0 );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL SelectFile_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDOK: {
         LPSTR lpBuf;
         HWND hwndList;
         int index;

         lpBuf = (LPSTR)GetWindowLong( hwnd, DWL_USER );
         VERIFY( hwndList = GetDlgItem( hwnd, IDD_LIST ) );
         index = ListBox_GetCurSel( hwndList );
         ASSERT( index != LB_ERR );
         ListBox_GetText( hwndList, index, lpBuf );
         }

      case IDCANCEL:
         EndDialog( hwnd, id == IDOK );
         break;
      }
}
