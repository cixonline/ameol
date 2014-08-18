/* LAYER.C - Parameter validation layer
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

#include "warnings.h"
#include <windows.h>
#include <windowsx.h>
#include "amlib.h"
#include "ameolrc.h"
#include "layer.h"

static BOOL fDefDlgEx = FALSE;            /* DefDlg recursion flag trap */

#pragma data_seg("_LAYER_DATA")

/* Error messages. */
static char szBadCodeAddr[] = "Illegal code address";
static char szBadDataAddr[] = "Illegal data address";
static char szBadCatHandle[] = "Invalid category handle";
static char szBadConfHandle[] = "Invalid forum handle";
static char szBadTopicHandle[] = "Invalid topic handle";
static char szBadMessageHandle[] = "Invalid message handle";
static char szBadWndHandle[] = "Invalid window handle";
static char szBadMenuHandle[] = "Invalid menu handle";

/* API function names */
char szFNPVCreateCategory[] = "CreateCategory";
char szFNPVOpenCategory[] = "OpenCategory";
char szFNPVDeleteCategory[] = "DeleteCategory";
char szFNPVGetCategory[] = "GetCategory";
char szFNPVGetPreviousCategory[] = "GetPreviousCategory";
char szFNPVGetCategoryName[] = "GetCategoryName";
char szFNPVGetCategoryInfo[] = "GetCategoryInfo";
char szFNPVCategoryFromConference[] = "CategoryFromConference";
char szFNPVCreateConferenceInCategory[] = "CreateConferenceInCategory";
char szFNPVCreateConference[] = "CreateConference";
char szFNPVOpenConference[] = "OpenConference";
char szFNPVDeleteConference[] = "DeleteConference";
char szFNPVGetConference[] = "GetConference";
char szFNPVGetPreviousConference[] = "GetPreviousConference";
char szFNPVGetConferenceName[] = "GetConferenceName";
char szFNPVCreateTopic[] = "CreateTopic";
char szFNPVOpenTopic[] = "OpenTopic";
char szFNPVDeleteTopic[] = "DeleteTopic";
char szFNPVGetTopicName[] = "GetTopicName";
char szFNPVGetTopic[] = "GetTopic";
char szFNPVGetPreviousTopic[] = "GetPreviousTopic";
char szFNPVGetLastTopic[] = "GetLastTopic";
char szFNPVGetTopicInfo[] = "GetTopicInfo";
char szFNPVConferenceFromTopic[] = "ConferenceFromTopic";
char szFNPVTopicFromMsg[] = "TopicFromMsg";
char szFNPVGetNextMsg[] = "GetNextMsg";
char szFNPVGetPreviousMsg[] = "GetPreviousMsg";
char szFNPVGetLastMsg[] = "GetLastMsg";
char szFNPVGetRootMsg[] = "GetRootMsg";
char szFNPVGetNearestMsg[] = "GetNearestMsg";
char szFNPVGetNextUnReadMsg[] = "GetNextUnReadMsg";
char szFNPVGetMsg[] = "GetMsg";
char szFNPVGetMsgInfo[] = "GetMsgInfo";
char szFNPVGetMsgText[] = "GetMsgText";
char szFNPVIsRead[] = "IsRead";
char szFNPVMarkMsgRead[] = "MarkMsgRead";
char szFNPVIsKept[] = "IsKept";
char szFNPVMarkMsgKeep[] = "MarkMsgKeep";
char szFNPVIsMarked[] = "IsMarked";
char szFNPVMarkMsg[] = "MarkMsg";
char szFNPVIsHdrOnly[] = "IsHdrOnly";
char szFNPVMarkMsgHdrOnly[] = "MarkMsgHdrOnly";
char szFNPVIsAttachment[] = "IsAttachment";
char szFNPVGetSetPurgeOptions[] = "GetSetPurgeOptions";
char szFNPVMarkMsgDelete[] = "MarkMsgDelete";
char szFNPVIsDeleted[] = "IsDeleted";
char szFNPVMarkMsgWithdrawn[] = "MarkMsgWithdrawn";
char szFNPVIsWithdrawn[] = "IsWithdrawn";
char szFNPVMarkMsgIgnore[] = "MarkMsgIgnore";
char szFNPVIsIgnored[] = "IsIgnored";
char szFNPVMarkMsgTagged[] = "MarkMsgTagged";
char szFNPVIsTagged[] = "IsTagged";
char szFNPVMarkMsgWatch[] = "MarkMsgWatch";
char szFNPVIsWatch[] = "IsWatch";
char szFNPVMarkMsgBeingRetrieved[] = "MarkMsgBeingRetrieved";
char szFNPVIsBeingRetrieved[] = "IsBeingRetrieved";
char szFNPVMarkMsgUnavailable[] = "MarkMsgUnavailable";
char szFNPVIsUnavailable[] = "IsUnavailable";
char szFNPVLockTopic[] = "LockTopic";
char szFNPVUnlockTopic[] = "UnlockTopic";
char szFNPVDeleteMsg[] = "DeleteMsg";
char szFNPVFreeMsgTextBuffer[] = "FreeMsgTextBuffer";
char szFNPVIsReadLock[] = "IsReadLock";
char szFNPVMarkMsgReadLock[] = "MarkMsgReadLock";
char szFNPVGetMsgTextLength[] = "GetMsgTextLength";
char szFNPVGetMarked[] = "GetMarked";
char szFNPVIsPriority[] = "IsPriority";
char szFNPVMarkMsgPriority[] = "MarkMsgPriority";
char szFNPVIsModerator[] = "IsModerator";
char szFNPVSetTopicFlags[] = "SetTopicFlags";
char szFNPVGetConferenceInfo[] = "GetConferenceInfo";
char szFNPVGetSetModeratorList[] = "GetSetModeratorList";
char szFNPVCreateMsg[] = "CreateMsg";
char szFNPVAddEventLogItem[] = "AddEventLogItem";
char szFNPVGetNextEventLogItem[] = "GetNextEventLogItem";
char szFNPVGetRegistry[] = "GetRegistry";
char szFNPVGetInitialisationFile[] = "GetInitialisationFile";
char szFNPVGetAmeolWindows[] = "GetAmeolWindows";
char szFNPVGetCurrentMsg[] = "GetCurrentMsg";
char szFNPVSetCurrentMsg[] = "SetCurrentMsg";
char szFNPVEnumAllTools[] = "EnumAllTools";
char szFNPVInsertAmeolMenu[] = "InsertAmeolMenu";
char szFNPVRegisterEvent[] = "RegisterEvent";
char szFNPVUnregisterEvent[] = "UnregisterEvent";
char szFNPVStdDialogBox[] = "StdDialogBox";
char szFNPVStdDialogBoxParam[] = "StdDialogBoxParam";
char szFNPVStdCreateDialog[] = "StdCreateDialog";
char szFNPVStdCreateDialogParam[] = "StdCreateDialogParam";
char szFNPVStdMDIDialogBoxFrame[] = "StdMDIDialogBoxFrame";
char szFNPVStdMDIDialogBox[] = "StdMDIDialogBox";
char szFNPVStdEndMDIDialog[] = "StdEndMDIDialog";
char szFNPVStdOpenMDIDialog[] = "StdOpenMDIDialog";
char szFNPVStdInitDialog[] = "StdInitDialog";
char szFNPVStdEndDialog[] = "StdEndDialog";
char szFNPVStdCtlColor[] = "StdCtlColor";
char szFNPVPutObject[] = "PutObject";
char szFNPVFindObject[] = "FindObject";
char szFNPVGetObjectInfo[] = "GetObjectInfo";
char szFNPVGetOutBasketObjectType[] = "GetOutBasketObjectType";
char szFNPVGetOutBasketObject[] = "GetOutBasketObject";
char szFNPVGetObjectText[] = "GetObjectText";
char szFNPVRemoveObject[] = "RemoveObject";
char szFNPVAmOpenFile[] = "AmOpenFile";
char szFNPVAmCreateFile[] = "AmCreateFile";
char szFNPVAmQueryFileExists[] = "AmQueryFileExists";
char szFNPVAmDeleteFile[] = "AmDeleteFile";
char szFNPVAmRenameFile[] = "AmRenameFile";
char szFNPVAmFindFirst[] = "AmFindFirst";
char szFNPVCreateOSCompatibleFileName[] = "CreateOSCompatibleFileName";
char szFNPVGetAddressBook[] = "GetAddressBook";
char szFNPVGetAddressBookGroup[] = "GetAddressBookGroup";
char szFNPVGetNextAddressBookGroup[] = "GetNextAddressBookGroup";
char szFNPVDeleteAddressBookGroup[] = "DeleteAddressBookGroup";
char szFNPVCreateAddressBookGroup[] = "CreateAddressBookGroup";
char szFNPVAddAddressBookGroupItem[] = "AddAddressBookGroupItem";
char szFNPVSetAddressBook[] = "SetAddressBook";
char szFNPVDeleteAmeolMenu[] = "DeleteAmeolMenu";
char szFNPVChangeDirectory[] = "ChangeDirectory";
char szFNPVAm_GetFileTime[] = "Am_GetFileTime";
char szFNPVGetTopicType[] = "GetTopicType";

#pragma data_seg()

/* Helper functions */
static BOOL NEAR PVFail( LPCSTR, LPCSTR, LPVOID );

/* External functions (in AMDB.C) */
BOOL FAR PASCAL Amdb_IsCategoryPtr( LPVOID );
BOOL FAR PASCAL Amdb_IsFolderPtr( LPVOID );
BOOL FAR PASCAL Amdb_IsTopicPtr( LPVOID );
BOOL FAR PASCAL Amdb_IsMessagePtr( LPVOID );

BOOL EXPORT FAR PASCAL PVDialogBox( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL PVDialogBox_DlgProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL PVDialogBox_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL PVDialogBox_OnCommand( HWND, int, HWND, UINT );

extern HINSTANCE hLibInst;

BOOL EXPORT CALLBACK PVDialogBox( HWND, UINT, WPARAM, LPARAM );

/* Ensure that lpv is a valid code pointer.
 */
BOOL FAR PASCAL PVCodeAddress( FARPROC lpv, LPSTR lpszFuncName )
{
   if( !IsBadCodePtr( lpv ) )
      return( TRUE );
   return( PVFail( lpszFuncName, szBadCodeAddr, lpv ) );
}

/* Ensure that lpv is a valid read-write data pointer.
 */
BOOL FAR PASCAL PVDataAddress( LPVOID lpv, LPSTR lpszFuncName )
{
   if( !IsBadReadPtr( lpv, 1 ) && !IsBadWritePtr( lpv, 1 ) )
      return( TRUE );
   return( PVFail( lpszFuncName, szBadDataAddr, lpv ) );
}

/* Ensure that lpv is a valid read-only data pointer.
 */
BOOL FAR PASCAL PVConstDataAddress( LPVOID lpv, LPSTR lpszFuncName )
{
   if( !IsBadReadPtr( lpv, 1 ) )
      return( TRUE );
   return( PVFail( lpszFuncName, szBadDataAddr, lpv ) );
}

/* Ensure that lpv is a valid category pointer.
 */
BOOL FAR PASCAL PVCategoryHandle( LPVOID lpv, LPSTR lpszFuncName )
{
   if( IsBadReadPtr( lpv, 1 ) )
      return( PVFail( lpszFuncName, szBadDataAddr, lpv ) );
   if( !Amdb_IsCategoryPtr( lpv ) )
      return( PVFail( lpszFuncName, szBadCatHandle, lpv ) );
   return( TRUE );
}

/* Ensure that lpv is a valid conference pointer.
 */
BOOL FAR PASCAL PVConferenceHandle( LPVOID lpv, LPSTR lpszFuncName )
{
   if( IsBadReadPtr( lpv, 1 ) )
      return( PVFail( lpszFuncName, szBadDataAddr, lpv ) );
   if( !Amdb_IsFolderPtr( lpv ) )
      return( PVFail( lpszFuncName, szBadConfHandle, lpv ) );
   return( TRUE );
}

/* Ensure that lpv is a valid topic pointer.
 */
BOOL FAR PASCAL PVTopicHandle( LPVOID lpv, LPSTR lpszFuncName )
{
   if( IsBadReadPtr( lpv, 1 ) )
      return( PVFail( lpszFuncName, szBadDataAddr, lpv ) );
   if( !Amdb_IsTopicPtr( lpv ) )
      return( PVFail( lpszFuncName, szBadTopicHandle, lpv ) );
   return( TRUE );
}

/* Ensure that lpv is a valid message pointer.
 */
BOOL FAR PASCAL PVMsgHandle( LPVOID lpv, LPSTR lpszFuncName )
{
   if( IsBadReadPtr( lpv, 1 ) )
      return( PVFail( lpszFuncName, szBadDataAddr, lpv ) );
   if( !Amdb_IsMessagePtr( lpv ) )
      return( PVFail( lpszFuncName, szBadMessageHandle, lpv ) );
   return( TRUE );
}

/* Ensure that lpv is a valid window handle
 */
BOOL FAR PASCAL PVWindowHandle( HWND hwnd, LPSTR lpszFuncName )
{
   if( hwnd && !IsWindow( hwnd ) )
      return( PVFail( lpszFuncName, szBadWndHandle, (LPVOID)hwnd ) );
   return( TRUE );
}

/* Ensure that lpv is a valid menu handle
 */
BOOL FAR PASCAL PVMenuHandle( HMENU hmenu, LPSTR lpszFuncName )
{
   if( !IsMenu( hmenu ) )
      return( PVFail( lpszFuncName, szBadMenuHandle, (LPVOID)hmenu ) );
   return( TRUE );
}

/* This function displays the parameter validation failure message box,
 * showing the MW API function in which the validation failed, the
 * reason for the failure and a single OK button.
 *
 * Computation of the user caller address is determined by several things,
 * all of which can easily break.
 */
BOOL NEAR PVFail( LPCSTR lpFuncName, LPCSTR lpErrMsg, LPVOID lpv )
{
   LPCSTR alpstr[ 3 ];

   alpstr[ 0 ] = lpFuncName;
   alpstr[ 1 ] = lpErrMsg;
   alpstr[ 2 ] = lpv;
   switch( DialogBoxParam( hLibInst, MAKEINTRESOURCE(IDDLG_PVFAIL), GetFocus(), PVDialogBox, (LPARAM)(LPSTR)&alpstr ) )
      {
      case IDD_FAIL:
         return( FALSE );

      case IDD_IGNORE:
         return( TRUE );

      case IDD_BREAK:
         DebugBreak();
         break;
      }
   return( FALSE );
}

/* This function handles the Parameter Validation dialog box.
 */
BOOL EXPORT CALLBACK PVDialogBox( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = PVDialogBox_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the PVDialogBox
 * dialog.
 */
LRESULT FASTCALL PVDialogBox_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, PVDialogBox_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, PVDialogBox_OnCommand );

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL PVDialogBox_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   char sz[ 100 ];
   LPSTR * alpstr;

   alpstr = (LPSTR *)lParam;
   SetDlgItemText( hwnd, IDD_PAD1, alpstr[ 0 ] );
   wsprintf( sz, "%s (0x%8.8lX)", alpstr[ 1 ], (DWORD)alpstr[ 2 ] );
   SetDlgItemText( hwnd, IDD_PAD2, sz );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL PVDialogBox_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_FAIL:
      case IDD_IGNORE:
      case IDD_BREAK:
         EndDialog( hwnd, id );
         break;
      }
}
