/* ERRORS.C - Functions that report errors
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
#include "amlib.h"
#include "amevent.h"

static char FAR szErrMsgBuf[ 256 ];

/* This function reports an out-of memory error.
 */
BOOL FASTCALL OutOfMemoryError( HWND hwnd, BOOL fRetryable, BOOL fOnline )
{
   wsprintf( szErrMsgBuf, GS(IDS_STR187) );
   if ( fOnline )
      {
      Amevent_AddEventLogItem( ETYP_SEVERE|ETYP_DISKFILE, szErrMsgBuf );
      return( FALSE );
      }
   if( !fRetryable )
      {
      fMessageBox( hwnd, 0, GS(IDS_STR187), MB_OK|MB_ICONEXCLAMATION );
      return( FALSE );
      }
   return( fMessageBox( hwnd, 0, GS(IDS_STR187), MB_RETRYCANCEL|MB_ICONEXCLAMATION ) == IDRETRY );
}

/* This function reports a disk write error.
 */
BOOL FASTCALL DiskWriteError( HWND hwnd, char * pszFilename, BOOL fRetryable, BOOL fOnline )
{
   wsprintf( szErrMsgBuf, GS(IDS_STR690), pszFilename );
   if ( fOnline )
      {
      Amevent_AddEventLogItem( ETYP_SEVERE|ETYP_DISKFILE, szErrMsgBuf );
      return( FALSE );
      }
   if( !fRetryable )
      {
      fMessageBox( hwnd, 0, szErrMsgBuf, MB_OK|MB_ICONEXCLAMATION );
      return( FALSE );
      }
   return( fMessageBox( hwnd, 0, szErrMsgBuf, MB_RETRYCANCEL|MB_ICONEXCLAMATION ) == IDRETRY );
}

/* This function reports a disk read error.
 */
BOOL FASTCALL DiskReadError( HWND hwnd, char * pszFilename, BOOL fRetryable, BOOL fOnline )
{
   wsprintf( szErrMsgBuf, GS(IDS_STR689), pszFilename );
   if ( fOnline )
      {
      Amevent_AddEventLogItem( ETYP_SEVERE|ETYP_DISKFILE, szErrMsgBuf );
      return( FALSE );
      }
   if( !fRetryable )
      {
      fMessageBox( hwnd, 0, szErrMsgBuf, MB_OK|MB_ICONEXCLAMATION );
      return( FALSE );
      }
   return( fMessageBox( hwnd, 0, szErrMsgBuf, MB_RETRYCANCEL|MB_ICONEXCLAMATION ) == IDRETRY );
}

/* This function reports an error opening a file.
 */
BOOL FASTCALL FileOpenError( HWND hwnd, char * pszFilename, BOOL fRetryable, BOOL fOnline )
{
   wsprintf( szErrMsgBuf, GS(IDS_STR78), pszFilename );
   if ( fOnline )
      {
      Amevent_AddEventLogItem( ETYP_SEVERE|ETYP_DISKFILE, szErrMsgBuf );
      return( FALSE );
      }
   if( !fRetryable )
      {
      fMessageBox( hwnd, 0, szErrMsgBuf, MB_OK|MB_ICONEXCLAMATION );
      return( FALSE );
      }
   return( fMessageBox( hwnd, 0, szErrMsgBuf, MB_RETRYCANCEL|MB_ICONEXCLAMATION ) );
}

/* This function reports an error creating a file.
 */
BOOL FASTCALL FileCreateError( HWND hwnd, char * pszFilename, BOOL fRetryable, BOOL fOnline )
{
   wsprintf( szErrMsgBuf, GS(IDS_STR691), pszFilename );
   if ( fOnline )
      {
      Amevent_AddEventLogItem( ETYP_SEVERE|ETYP_DISKFILE, szErrMsgBuf );
      return( FALSE );
      }
   if( !fRetryable )
      {
      fMessageBox( hwnd, 0, szErrMsgBuf, MB_OK|MB_ICONEXCLAMATION );
      return( FALSE );
      }
   return( fMessageBox( hwnd, 0, szErrMsgBuf, MB_RETRYCANCEL|MB_ICONEXCLAMATION ) );
}

/* This function reports an error saving to the disk. The user is
 * offered the opportunity to retry the action that raised
 * the error condition.
 */
BOOL FASTCALL DiskSaveError( HWND hwnd, char * pszDir, BOOL fRetryable, BOOL fOnline )
{
   char chDrive;

   chDrive = Amdir_GetDrive( pszDir );
   wsprintf( szErrMsgBuf, GS(IDS_STR188), chDrive );
   if ( fOnline )
      {
      Amevent_AddEventLogItem( ETYP_SEVERE|ETYP_DISKFILE, szErrMsgBuf );
      return( FALSE );
      }
   if( !fRetryable )
      {
      fMessageBox( hwnd, 0, szErrMsgBuf, MB_OK|MB_ICONEXCLAMATION );
      return( FALSE );
      }
   return( fMessageBox( hwnd, 0, szErrMsgBuf, MB_RETRYCANCEL|MB_ICONEXCLAMATION ) == IDRETRY );
}

/* Tells the user that a feature is not yet implemented.
 */
void FASTCALL NotImplementedYet( HWND hwnd )
{
   fMessageBox( hwnd, 0, "Not yet implemented", MB_OK|MB_ICONEXCLAMATION );
}
