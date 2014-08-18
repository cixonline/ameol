/* SCREXPRT.C - Exports a CIX scratchpad
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
#include "resource.h"
#include <string.h>
#include "palrc.h"
#include <io.h>
#include "amlib.h"
#include "ameol2.h"
#include "rules.h"

#define  THIS_FILE   __FILE__

#define  IDD_APPENDUPDATE     12

static BOOL fDefDlgEx = FALSE;            /* DefDlg recursion flag trap */
static DWORD cbToExport;                  /* Total number of messages to write */
static DWORD cWritten;                    /* Count of messages written so far */
static DWORD cLastWritten;                /* Last count of messages written so far */
static int wMode;                         /* File mode */
static AM_DATE dtFrom;                    /* Range: date from */
static AM_DATE dtTo;                      /* Range: date to */
static WORD wFromDate;                    /* Range: packed date from */
static WORD wToDate;                      /* Range: packed date to */
static char FAR szFileName[ _MAX_PATH ];  /* Store file name */
static DWORD dwFrom;                      /* Range: message from */
static DWORD dwTo;                        /* Range: message to */
static BOOL fAppend;                      /* TRUE if we append to existing file */
static char szAuthor[ 40 ];               /* Author's name */
static LPVOID pData;                      /* Folder from which to file */
static HWND hDlg;                         /* Progress dialog */
static WORD wOldCount;                    /* Export progress percentage */
static BOOL fWriteAttributes;             /* TRUE if we prepend message attributes */

BOOL EXPORT CALLBACK ScrExportDlg( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL ScrExportDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL ScrExportDlg_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL ScrExportDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );
void FASTCALL ScrExportDlg_OnTimer( HWND, UINT );

void FASTCALL ScrExportDlg_TopicChange( HWND );
BOOL FASTCALL ScrExportCategory( HNDFILE, PCAT, BOOL );
BOOL FASTCALL ScrExportFolder( HNDFILE, LPVOID, BOOL );
BOOL FASTCALL ScrExportTopic( HNDFILE, LPVOID, BOOL );
BOOL FASTCALL WriteMsgText( HNDFILE, PTH, BOOL );
HPSTR FASTCALL SkipToEndOfLine( HPSTR );

void FASTCALL BeginExport( DWORD );
void FASTCALL EndExport( void );
BOOL FASTCALL ExportToTextFile( HWND, BOOL );

BOOL EXPORT CALLBACK ScrExportDlgStatus( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL ScrExportDlgStatus_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL ScrExportDlgStatus_OnCommand( HWND, int, HWND, UINT );
LRESULT FASTCALL ScrExportDlgStatus_DlgProc( HWND, UINT, WPARAM, LPARAM );

/* This function writes a selected range of messages to a data file. The
 * format of the messages is the same as for the original scratchpad from
 * which they were read.
 */
BOOL EXPORT WINAPI CIXExport( HWND hwnd )
{
   return( ExportToTextFile( hwnd, TRUE ) );
}

/* This function writes a selected range of messages to a data file. The
 * format of the messages is a plaintext file.
 */
BOOL EXPORT WINAPI TextFileExport( HWND hwnd )
{
   return( ExportToTextFile( hwnd, FALSE ) );
}

/* This function writes a selected range of messages to a data file. The
 * format of the messages is a plaintext file.
 */
BOOL FASTCALL ExportToTextFile( HWND hwnd, BOOL fHeaders )
{
   BOOL fOk;
   BOOL fGotFilename;
   char sz[ 100 ];
   char * psz;
   HNDFILE fh;

   /* Get and open the export file.
    */
   fGotFilename = FALSE;
   fWriteAttributes = FALSE;
   fh = HNDFILE_ERROR;
   while( !fGotFilename )
      {
      /* Show and get the export information.
       */
      if( !Adm_Dialog( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_SCRPAD_EXPORT), ScrExportDlg, (LPARAM)fHeaders ) )
         return( FALSE );

      /* If we're appending, attempt to open the existing file and seek
       * to the end.
       */
      if( !fAppend )
         {
         if( Amfile_QueryFile( szFileName ) )
            {
            wsprintf( sz, GS(IDS_STR673), (LPSTR)szFileName );
            if( IDNO == fMessageBox( hwnd, 0, sz, MB_YESNO|MB_ICONEXCLAMATION ) )
               continue;
            }
         }
      else if( ( fh = Amfile_Open( szFileName, AOF_WRITE ) ) != HNDFILE_ERROR )
         {
         Amfile_SetPosition( fh, 0L, ASEEK_END );
         break;
         }

      /* If we couldn't open the existing file, create it anew.
       */
      if( ( fh = Amfile_Create( szFileName, 0 ) ) == HNDFILE_ERROR )
         {
         wsprintf( sz, GS(IDS_STR89), (LPSTR)szFileName );
         if( IDCANCEL == fMessageBox( hwnd, 0, sz, MB_RETRYCANCEL|MB_ICONEXCLAMATION ) )
            return( FALSE );
         }
      else
         fGotFilename = TRUE;
      }
   ASSERT( fh != HNDFILE_ERROR );

   /* The main export code starts here.
    */
   cWritten = cLastWritten = 0;
   fOk = TRUE;
   hDlg = NULL;
   switch( wMode )
      {
      case IDD_CURRENTTHREAD: {
         LPWINDINFO lpWindInfo;
         PTH pth;
         PCL pcl;

         /* Export the current thread from the current topic. If this option
          * is choosen, pData is ignored.
          */
         ASSERT( NULL != hwndTopic );
         lpWindInfo = GetBlock( hwndTopic );
         pcl = Amdb_FolderFromTopic( lpWindInfo->lpTopic );
         if( pth = lpWindInfo->lpMessage )
            {
            pth = Amdb_GetRootMsg( pth, TRUE );
            while( pth && !fMainAbort )
               {
               if( !( fOk = WriteMsgText( fh, pth, fHeaders ) ) )
                  break;
               ++cWritten;
               pth = Amdb_GetNextMsg( pth, VM_VIEWREFEX );
               if( pth )
                  {
                  MSGINFO msginfo;

                  Amdb_GetMsgInfo( pth, &msginfo );
                  if( msginfo.wLevel == 0 )
                     pth = NULL;
                  }
               }
            }
         break;
         }

      case IDD_CURRENT: {
         LPINT lpi;
         PTH pth;

         /* Export the current message from the current topic. If this option
          * is choosen, pData is ignored.
          * Now exports selected range of messages.
          */
         cbToExport = 1;
         ASSERT( NULL != hwndTopic );
         if( NULL != ( lpi = GetSelectedItems( hwndTopic ) ) )
            {
            register int c;
            HWND hwndList;

            hwndList = GetDlgItem( hwndTopic, IDD_LIST );
            for( c = 0; lpi[ c ] != LB_ERR; ++c )
               {
               pth = (PTH)ListBox_GetItemData( hwndList, lpi[ c ] );
               if( fOk = WriteMsgText( fh, pth, fHeaders ) )
                  ++cWritten;
               }
            FreeMemory( &lpi );
            }
         break;
         }

      case IDD_RANGE:
         /* Export a range of messages from the selected folder. If this option is
          * choosen, pData must be a topic.
          */
         ASSERT( Amdb_IsTopicPtr( pData ) );
         fOk = ScrExportTopic( fh, (PTL)pData, fHeaders );
         break;

      case IDD_ALL:
         /* Export all messages from the selected folder. If this option is choosen,
          * pData can be a database, folder or topic.
          */
         if( Amdb_IsTopicPtr( pData ) )
            {
            TOPICINFO topicinfo;

            Amdb_GetTopicInfo( (PTL)pData, &topicinfo );
            cbToExport = topicinfo.cMsgs;
            BeginExport( cbToExport );
            fOk = ScrExportTopic( fh, (PTL)pData, fHeaders );
            }
         else if( Amdb_IsFolderPtr( pData ) )
            {
            PTL ptl;

            /* Count how many messages we're exporting.
             */
            cbToExport = 0;
            for( ptl = Amdb_GetFirstTopic( (PCL)pData ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
               {
               TOPICINFO topicinfo;
   
               Amdb_GetTopicInfo( ptl, &topicinfo );
               cbToExport += topicinfo.cMsgs;
               }
            BeginExport( cbToExport );
            fOk = ScrExportFolder( fh, (PCL)pData, fHeaders );
            }
         else
            {
            PCL pcl;
            PTL ptl;

            /* Count how many messages we're exporting.
             */
            cbToExport = 0;
            for( pcl = Amdb_GetFirstFolder( (PCAT)pData ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
               for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
                  {
                  TOPICINFO topicinfo;

                  Amdb_GetTopicInfo( ptl, &topicinfo );
                  cbToExport += topicinfo.cMsgs;
                  }
            BeginExport( cbToExport );
            fOk = ScrExportCategory( fh, (PCAT)pData, fHeaders );
            }
         break;
      }

   /* Close the file handle and report on the
    * result to the user.
    */
   EndExport();
   Amfile_Close( fh );
   if( !fOk )
      psz = GS( IDS_STR88 );
   else if( cWritten == 0 )
      {
      psz = GS( IDS_STR87 );
      if( !fAppend )
         Amfile_Delete( szFileName );
      }
   else
      wsprintf( psz = sz, GS( IDS_STR86 ), cWritten );
   fMessageBox( hwnd, 0, psz, MB_OK|MB_ICONEXCLAMATION );
   return( TRUE );
}

/* This function displays the progress gauge if the number of messages
 * being exported is or is greater than 25.
 */
void FASTCALL BeginExport( DWORD dwMsgs )
{
   if( dwMsgs > 24 )
      {
      HWND hwndGauge;

      /* Create the dialog box.
       */
      EnableWindow( hwndFrame, FALSE );
      hDlg = Adm_CreateDialog( hRscLib, hwndFrame, MAKEINTRESOURCE(IDDLG_EXPORTPROGRESS), ScrExportDlgStatus, 0L );
      UpdateWindow( hwndFrame );
      hwndGauge = GetDlgItem( hDlg, IDD_GAUGE );

      /* Set the progress bar gauge range.
       */
      SendMessage( hwndGauge, PBM_SETRANGE, 0, MAKELPARAM( 0, 100 ) );
      SendMessage( hwndGauge, PBM_SETSTEP, 1, 0L );
      wOldCount = 0;
      }
}

/* End of export. Delete progress dialog if there was one.
 */
void FASTCALL EndExport( void )
{
   if( hDlg )
      {
      EnableWindow( hwndFrame, TRUE );
      DestroyWindow( hDlg );
      }
}

/* This function writes the text of the specified message to the
 * export file.
 */
BOOL FASTCALL WriteMsgText( HNDFILE fh, PTH pth, BOOL fHeaders )
{
   HPSTR hpText;

   /* Strip out CR from the message and compute the actual size.
    */
   hpText = Amdb_GetMsgText( pth );
   if( NULL != hpText )
      {
      MSGINFO msginfo;
      HPSTR hpTextBuf;
      BOOL fWriteOk;

      /* Get the message info.
       */
      fWriteOk = TRUE;
      hpTextBuf = hpText;
      Amdb_GetMsgInfo( pth, &msginfo );

      /* If we write the message attributes, write them now.
       */
      if( fWriteAttributes )
         {
         char sz[ 40 ];

         strcpy( sz, "!MF:" );
         if( !( msginfo.dwFlags & MF_READ ) )
            strcat( sz, "U" );
         if( msginfo.dwFlags & MF_MARKED )
            strcat( sz, "M" );
         if( msginfo.dwFlags & MF_READLOCK )
            strcat( sz, "L" );
         if( msginfo.dwFlags & MF_DELETED )
            strcat( sz, "D" );
         if( msginfo.dwFlags & MF_IGNORE )
            strcat( sz, "I" );
         if( msginfo.dwFlags & MF_KEEP )
            strcat( sz, "K" );
         if( msginfo.dwFlags & MF_TAGGED )
            strcat( sz, "G" );
         if( msginfo.dwFlags & MF_WATCH )
            strcat( sz, "H" );
         if( msginfo.dwFlags & MF_WITHDRAWN )
            strcat( sz, "W" );
         if( msginfo.dwFlags & MF_PRIORITY )
            strcat( sz, "A" );
         strcat( sz, "\r\n" );
         fWriteOk = Amfile_Write( fh, sz, strlen(sz) ) == strlen(sz);
         }

      /* Write the body of the message
       */
      if( fWriteOk )
         {
         switch( Amdb_GetTopicType( Amdb_TopicFromMsg( pth ) ) )
            {
            case FTYPE_MAIL:
               if( strncmp( hpText, "Memo #", 5 ) == 0 )
                  hpText = SkipToEndOfLine( hpText );
               if( fHeaders )
                  {
                  DWORD cbRealSize;
                  char sz[ 40 ];

                  cbRealSize = CountSize( hpText );
                  wsprintf( sz, "Memo #%lu (%lu)\r\n", msginfo.dwMsg, cbRealSize );
                  fWriteOk = Amfile_Write( fh, sz, strlen(sz) ) == strlen(sz);
                  }
               break;

            case FTYPE_LOCAL:
            case FTYPE_CIX:
               if( strncmp( hpText, ">>>", 3 ) == 0 )
                  hpText = SkipToEndOfLine( hpText );
               if( fHeaders )
                  {
                  AM_DATE date;
                  AM_TIME time;
                  char sz[ 128 ];
                  PTL ptl;

                  /* Reconstruct the header from the current topic/folder
                   * name rather than whatever is in the original header.
                   */
                  ptl = Amdb_TopicFromMsg( pth );
                  Amdate_UnpackDate( msginfo.wDate, &date );
                  Amdate_UnpackTime( msginfo.wTime, &time );
                  wsprintf( sz, szCompactHdrTmpl,
                           Amdb_GetFolderName( Amdb_FolderFromTopic( ptl ) ),
                           Amdb_GetTopicName( ptl ),
                           msginfo.dwMsg,
                           msginfo.szAuthor,
                           CountSize( hpText ),
                           date.iDay,
                           (LPSTR)pszCixMonths[ date.iMonth - 1 ],
                           date.iYear % 100,
                           time.iHour,
                           time.iMinute );
                  if( msginfo.dwComment )
                     wsprintf( sz + strlen( sz ), " c%lu*", msginfo.dwComment );
                  strcat( sz, "\r\n" );
                  fWriteOk = Amfile_Write( fh, sz, strlen(sz) ) == strlen(sz);
                  }
               break;

            case FTYPE_NEWS:
               if( strncmp( hpText, "#! rnews", 8 ) == 0 )
                  hpText = SkipToEndOfLine( hpText );
               if( fHeaders )
                  {
                  DWORD cbRealSize;
                  char sz[ 40 ];

                  cbRealSize = CountSize( hpText );
                  wsprintf( sz, "#! rnews %lu\r\n", cbRealSize );
                  fWriteOk = Amfile_Write( fh, sz, strlen(sz) ) == strlen(sz);
                  }
               break;
            }
         }
      if( fWriteOk )
         {
         DWORD cbText;

         cbText = hstrlen( hpText );
         fWriteOk = Amfile_Write32( fh, hpText, cbText ) == cbText;
         if( fWriteOk )
            fWriteOk = Amfile_Write( fh, "\r\n", 2 ) == 2;
         }
      Amdb_FreeMsgTextBuffer( hpTextBuf );
      return( fWriteOk );
      }
   return( FALSE );
}

/* This function skips to the end of the current line
 * pointed by hpText.
 */
HPSTR FASTCALL SkipToEndOfLine( HPSTR hpText )
{
   while( *hpText && *hpText != 13 && *hpText != 10 )
      ++hpText;
   if( *hpText == 13 ) ++hpText;
   if( *hpText == 10 ) ++hpText;
   return( hpText );
}

/* This function exports all messages from all topics in the specified
 * folder.
 */
BOOL FASTCALL ScrExportCategory( HNDFILE fh, PCAT pcat, BOOL fHeaders )
{
   PCL pcl;

   for( pcl = Amdb_GetFirstFolder( pcat ); pcl; pcl = Amdb_GetNextFolder( pcl ) )
      {
      if( !ScrExportFolder( fh, pcl, fHeaders ) )
         return( FALSE );
      }
   return( TRUE );
}

/* This function exports all messages from all topics in the specified
 * folder.
 */
BOOL FASTCALL ScrExportFolder( HNDFILE fh, PCL pcl, BOOL fHeaders )
{
   PTL ptl;

   for( ptl = Amdb_GetFirstTopic( pcl ); ptl; ptl = Amdb_GetNextTopic( ptl ) )
      {
      if( !ScrExportTopic( fh, ptl, fHeaders ) )
         return( FALSE );
      }
   return( TRUE );
}

/* This function exports all messages in the specified folder.
 */
BOOL FASTCALL ScrExportTopic( HNDFILE fh, PTL ptl, BOOL fHeaders )
{
   PTH pth;
   BOOL fOk = TRUE;

   /* Show the topic name in the export dialog
    */
   if( NULL != hDlg )
      {
      WriteFolderPathname( lpTmpBuf, ptl );
      SetDlgItemText( hDlg, IDD_STATUS, lpTmpBuf );
      }

   /* Export this topic.
    */
   Amdb_LockTopic( ptl );
   pth = Amdb_GetFirstMsg( ptl, VM_VIEWCHRON );
   for( ; fOk && pth; pth = Amdb_GetNextMsg( pth, VM_VIEWCHRON ) )
      {
      MSGINFO msginfo;

      /* Get information about the specified message.
       */
      Amdb_GetMsgInfo( pth, &msginfo );

      /* Update progress gauge.
       */
      TaskYield();
      if( NULL != hDlg )
         {
         WORD wCount;

         wCount = (WORD)( ( cWritten * 100 ) / cbToExport );
         if( wCount != wOldCount )
            {
            HWND hwndGauge;

            hwndGauge = GetDlgItem( hDlg, IDD_GAUGE );
            SendMessage( hwndGauge, PBM_SETPOS, wCount, 0L );
            wOldCount = wCount;
            }
         }

      /* If an author field was defined, validate this author
       * against the specified author.
       */
      if( *szAuthor )
         if( _strcmpi( szAuthor, msginfo.szAuthor ) != 0 )
            continue;

      /* If a date range was defined, ensure that the date of
       * this message falls within that range.
       */
      if( wFromDate && wToDate )
         if( msginfo.wDate < wFromDate || msginfo.wDate > wToDate )
            continue;

      /* Handle the abort key.
       */
      if( fMainAbort )
         fOk = FALSE;
      else switch( wMode )
         {
         case IDD_ALL:
            if( fOk = WriteMsgText( fh, pth, fHeaders ) )
               ++cWritten;
            break;

         case IDD_RANGE:
            if( msginfo.dwMsg >= dwFrom && msginfo.dwMsg <= dwTo ) {
               if( fOk = WriteMsgText( fh, pth, fHeaders ) )
                  ++cWritten;
               }
            break;
         }
      }
   Amdb_UnlockTopic( ptl );
   return( fOk );
}

/* This function handles the Export dialog.
 */
BOOL EXPORT CALLBACK ScrExportDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = ScrExportDlg_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the ExportDlg
 * dialog.
 */
LRESULT FASTCALL ScrExportDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, ScrExportDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, ScrExportDlg_OnCommand );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, FolderCombo_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_DRAWITEM, FolderCombo_OnDrawItem );
      HANDLE_MSG( hwnd, WM_TIMER, ScrExportDlg_OnTimer );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsSCRPAD_EXPORT );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL ScrExportDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndCombo;
   HWND hwndList;
   CURMSG curmsg;
   register int c;
   int index;

   /* Set the input field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_AUTHORNAME ), sizeof( szAuthor ) - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_TO1 ), 10 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_FROM1 ), 10 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_TO2 ), 20 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_FROM2 ), 20 );

   /* Set the current export filename.
    */
   VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_FILELIST ) );
   ComboBox_SetText( hwndCombo, szFileName );
   ComboBox_LimitText( hwndCombo, sizeof( szFileName ) - 1 );
   EnableControl( hwnd, IDOK, *szFileName != '\0' );

   /* Fill the combo box with previous scratchpads.
    */
   for( c = 0; c < 10; ++c )
      {
      char szScratch[ _MAX_DIR ];
      char sz[ 30 ];

      wsprintf( sz, "File%u", c + 1 );
      if( !Amuser_GetPPString( szScrExportFiles, sz, "", szScratch, sizeof( szScratch ) ) )
         break;
      ComboBox_InsertString( hwndCombo, -1, szScratch );
      }

   VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_AUTHORNAME ) );
   ComboBox_SetText( hwndCombo, szAuthor );
   ComboBox_LimitText( hwndCombo, sizeof( szAuthor ) - 1 );

   /* Fill the combo box with previous authors.
    */
   for( c = 0; c < 10; ++c )
      {
      char szScratch[ _MAX_DIR ];
      char sz[ 30 ];

      wsprintf( sz, "File%u", c + 1 );
      if( !Amuser_GetPPString( szScrExportAuthors, sz, "", szScratch, sizeof( szScratch ) ) )
         break;
      ComboBox_InsertString( hwndCombo, -1, szScratch );
      }

   /* Fill the list of folders combo box.
    */
   FillListWithTopics( hwnd, IDD_LIST, FTYPE_ALL|FTYPE_TOPICS );

   /* Select the default folder.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   Ameol2_GetCurrentMsg( &curmsg );
   if( NULL != curmsg.pFolder )
      if( CB_ERR != ( index = RealComboBox_FindItemData( hwndList, -1, (LPARAM)curmsg.pFolder ) ) )
         {
         ComboBox_SetCurSel( hwndList, index );
         EnableScrExportOptions( hwnd, curmsg.pFolder );
         }

   /* Default to Current Message if topic window open,
    * All otherwise.
    */
   if( hwndTopic )
      {
      LPWINDINFO lpWindInfo;

      lpWindInfo = GetBlock( hwndTopic );
      if( lpWindInfo->lpMessage )
         {
         hwndList = GetDlgItem( hwndTopic, IDD_LIST );
         if( ListBox_GetSelCount( hwndList ) > 1 )
            SetDlgItemText( hwnd, IDD_CURRENT, GS(IDS_STR858) );
         CheckDlgButton( hwnd, IDD_CURRENT, TRUE );
         }
      }

   /* Set the attributes flag.
    */
   fWriteAttributes = Amuser_GetPPInt( szSettings, "ExportAttributes", TRUE );
   if( lParam )
      CheckDlgButton( hwnd, IDD_ATTRIBUTES, fWriteAttributes );
   else
      ShowEnableControl( hwnd, IDD_ATTRIBUTES, FALSE );

   /* Start the timer.
    */
   ScrExportDlg_OnTimer( hwnd, IDD_APPENDUPDATE );

   /* Set the picture button.
    */
   PicButton_SetBitmap( GetDlgItem( hwnd, IDD_PICKER ), hInst, IDB_FILEBUTTON );
   return( TRUE );
}

/* This function handles the WM_TIMER message.
 */
void FASTCALL ScrExportDlg_OnTimer( HWND hwnd, UINT id )
{
   if( id == IDD_APPENDUPDATE )
      {
      char szFileName[ _MAX_PATH ];
      BOOL fFileExists;
   
      /* Timer has expired, so get the latest filename, load
       * the bitmap if the filename points to a valid picture
       * and update the picture image.
       */
      KillTimer( hwnd, id );
      Edit_GetText( GetDlgItem( hwnd, IDD_FILELIST ), szFileName, sizeof(szFileName ) );
      fFileExists = Amfile_QueryFile( szFileName );
      EnableControl( hwnd, IDD_APPEND, fFileExists );
      CheckDlgButton( hwnd, IDD_APPEND, fFileExists );
      }
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL ScrExportDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_PICKER: {
         char sz[ _MAX_PATH ];
         OPENFILENAME ofn;
         HWND hwndCombo;
         static char strFilters[] = "All Files (*.*)\0*.*\0\0";

         VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_FILELIST ) );
         ComboBox_GetText( hwndCombo, sz, sizeof( sz ) );
         if( ( strlen( sz) > 1 ) && ( sz[ ( strlen( sz ) - 1 ) ] == '\\' ) && ( sz[ ( strlen( sz ) - 2 ) ] != ':' ) )
            sz[ ( strlen( sz ) - 1 ) ] = '\0';
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
         ofn.lpstrTitle = "Export";
         ofn.Flags = OFN_NOCHANGEDIR|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST;
         ofn.nFileOffset = 0;
         ofn.nFileExtension = 0;
         ofn.lpstrDefExt = NULL;
         ofn.lCustData = 0;
         ofn.lpfnHook = NULL;
         ofn.lpTemplateName = 0;
         if( GetSaveFileName( &ofn ) )
            {
            ComboBox_SetText( hwndCombo, sz );
            EnableControl( hwnd, IDOK, *sz != '\0' );
            ScrExportDlg_OnTimer( hwnd, IDD_APPENDUPDATE );
            }
         SetFocus( hwndCombo );
         ComboBox_SetEditSel( hwndCombo, 32767, 32767 );
         break;
         }

      case IDD_FROM1:
      case IDD_TO1:
         if( codeNotify == EN_SETFOCUS )
            CheckRadioButton( hwnd, IDD_ALL, IDD_RANGE, IDD_RANGE );
         break;

      case IDD_LIST:
         if( codeNotify == CBN_SELCHANGE )
            {
            LPVOID pData;
            int index;

            /* Get the selected item.
             */
            index = ComboBox_GetCurSel( hwndCtl );
            pData = (LPVOID)ComboBox_GetItemData( hwndCtl, index );
            ASSERT( pData != NULL );
            EnableScrExportOptions( hwnd, pData );
            }
         break;

      case IDD_FILELIST:
         if( codeNotify == CBN_EDITUPDATE )
            {
            EnableControl( hwnd, IDOK, Edit_GetTextLength( hwndCtl ) );
            SetTimer( hwnd, IDD_APPENDUPDATE, 200, 0L );
            }
         else if( codeNotify == CBN_SELCHANGE )
            {
            EnableControl( hwnd, IDOK, ComboBox_GetCurSel( hwndCtl ) != CB_ERR );
            SetTimer( hwnd, IDD_APPENDUPDATE, 500, 0L );
            }
         break;

      case IDOK: {
         HWND hwndCombo;
         register int n;
         register int c;
         int index;

         /* Get the export filename, and ensure that it is valid.
          */
         VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_FILELIST ) );
         ComboBox_GetText( hwndCombo, szFileName, _MAX_PATH );
         if( !IsValidDOSFileName( szFileName ) )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR133), MB_OK|MB_ICONEXCLAMATION );
            HighlightField( hwnd, IDD_FILELIST );
            break;
            }

         /* Now save the entire list.
          */
         Amuser_WritePPString( szScrExportFiles, NULL, NULL );
         Amuser_WritePPString( szScrExportFiles, "File1", szFileName );
         for( n = 0, c = 2; n < 10; ++n )
            {
            char szExportFile[ _MAX_DIR ];

            if( CB_ERR == ComboBox_GetLBText( hwndCombo, n, szExportFile ) )
               break;
            if( strcmp( szExportFile, szFileName ) )
               {
               char sz[ 10 ];

               wsprintf( sz, "File%u", c++ );
               Amuser_WritePPString( szScrExportFiles, sz, szExportFile );
               }
            }

         /* Set Append.
          */
         fAppend = FALSE;
         if( IsWindowEnabled( GetDlgItem( hwnd, IDD_APPEND ) ) )
            fAppend = IsDlgButtonChecked( hwnd, IDD_APPEND );

         /* Get attributes flag.
          */
         fWriteAttributes = FALSE;
         if( IsWindowEnabled( GetDlgItem( hwnd, IDD_ATTRIBUTES ) ) )
            {
            fWriteAttributes = IsDlgButtonChecked( hwnd, IDD_ATTRIBUTES );
            Amuser_WritePPInt( szSettings, "ExportAttributes", fWriteAttributes );
            }

         

         /* Get the export folder.
          */
         VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_LIST ) );
         index = ComboBox_GetCurSel( hwndCombo );
         pData = (LPVOID)ComboBox_GetItemData( hwndCombo, index );

         /* Get the author name.
          */
         VERIFY( hwndCombo = GetDlgItem( hwnd, IDD_AUTHORNAME ) );
         ComboBox_GetText( hwndCombo, szAuthor, 40 );

         /* Now save the entire list.
          */
         Amuser_WritePPString( szScrExportAuthors, NULL, NULL );
         Amuser_WritePPString( szScrExportAuthors, "File1", szAuthor );
         for( n = 0, c = 2; n < 10; ++n )
            {
            char szAuthorFile[ _MAX_DIR ];

            if( CB_ERR == ComboBox_GetLBText( hwndCombo, n, szAuthorFile ) )
               break;
            if( strcmp( szAuthorFile, szAuthor ) )
               {
               char sz[ 10 ];

               wsprintf( sz, "File%u", c++ );
               Amuser_WritePPString( szScrExportAuthors, sz, szAuthorFile );
               }
            }

         /* If message range selected, get it and validate the
          * ranges.
          */
         if( IsDlgButtonChecked( hwnd, IDD_RANGE ) )
            {
            if( !GetDlgLongInt( hwnd, IDD_FROM1, &dwFrom, 1, 99999999 ) )
               break;
            if( !GetDlgLongInt( hwnd, IDD_TO1, &dwTo, 1, 99999999 ) )
               break;
            if( dwFrom > dwTo )
               {
               fMessageBox( hwnd, 0, GS(IDS_STR91), MB_OK|MB_ICONEXCLAMATION );
               SetFocus( GetDlgItem( hwnd, IDD_FROM1 ) );
               Edit_SetSel( GetDlgItem( hwnd, IDD_FROM1 ), 0, 32767 );
               break;
               }
            wMode = IDD_RANGE;
            }

         /* Handle other ranges.
          */
         else if( IsDlgButtonChecked( hwnd, IDD_ALL ) )
            wMode = IDD_ALL;
         else if( IsDlgButtonChecked( hwnd, IDD_CURRENTTHREAD ) )
            wMode = IDD_CURRENTTHREAD;
         else
            wMode = IDD_CURRENT;

         /* If date range selected, get the date range and validate
          * each date individually.
          */
         wFromDate = wToDate = 0;
         dtFrom.iDay = 1;
         dtFrom.iMonth = 1;
         dtFrom.iYear = 1970;
         if( Edit_GetTextLength( GetDlgItem( hwnd, IDD_FROM2 ) ) || Edit_GetTextLength( GetDlgItem( hwnd, IDD_TO2 ) ) )
            {
            if( !BreakDate( hwnd, IDD_FROM2, &dtFrom ) )
               break;
            Amdate_GetCurrentDate( &dtTo );
            if( !BreakDate( hwnd, IDD_TO2, &dtTo ) )
               break;
            wFromDate = Amdate_PackDate( &dtFrom );
            wToDate = Amdate_PackDate( &dtTo );
            if( wFromDate > wToDate )
               {
               fMessageBox( hwnd, 0, GS(IDS_STR92), MB_OK|MB_ICONEXCLAMATION );
               SetFocus( GetDlgItem( hwnd, IDD_FROM2 ) );
               Edit_SetSel( GetDlgItem( hwnd, IDD_FROM2 ), 0, 32767 );
               break;
               }
            }
         EndDialog( hwnd, TRUE );
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, FALSE );
         break;
      }
}

/* This function enables or disables the various export
 * settings depending on the specified folder.
 */
void FASTCALL EnableScrExportOptions( HWND hwnd, LPVOID pData )
{
   BOOL fIsTopic;

   /* If pData is not a topic, disable Range
    */
   fIsTopic = Amdb_IsTopicPtr( pData );
   EnableControl( hwnd, IDD_RANGE, fIsTopic );
   EnableControl( hwnd, IDD_FROM1, fIsTopic );
   EnableControl( hwnd, IDD_TO1, fIsTopic );

   /* If it is not the current topic in an open topic
    * window, disable the Current Message and Current Thread
    * commands.
    */
   if( hwndTopic )
      {
      LPWINDINFO lpWindInfo;

      lpWindInfo = GetBlock( hwndTopic );
      if( lpWindInfo->lpTopic == pData )
         {
         EnableControl( hwnd, IDD_CURRENT, TRUE );
         EnableControl( hwnd, IDD_CURRENTTHREAD, TRUE );
         return;
         }
      }

   /* No topic window open.
    */
   EnableControl( hwnd, IDD_CURRENT, FALSE );
   EnableControl( hwnd, IDD_CURRENTTHREAD, FALSE );
   CheckRadioButton( hwnd, IDD_ALL, IDD_RANGE, IDD_ALL );
}

/* This function handles the dialog box messages passed to the ExportDlgStatus
 * dialog.
 */
BOOL EXPORT CALLBACK ScrExportDlgStatus( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = ScrExportDlgStatus_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the ExportDlgStatus
 * dialog.
 */
LRESULT FASTCALL ScrExportDlgStatus_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, ScrExportDlgStatus_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, ScrExportDlgStatus_OnCommand );

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL ScrExportDlgStatus_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDCANCEL:
         if( fMessageBox( hwnd, 0, GS(IDS_STR145), MB_YESNO|MB_ICONQUESTION ) == IDYES )
            fMainAbort = TRUE;
         break;
      }
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL ScrExportDlgStatus_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   fMainAbort = FALSE;
   return( TRUE );
}
