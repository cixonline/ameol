/* ADMIN.C - Administrator Interface
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
#include "pdefs.h"
#include <dos.h>
#include <io.h>
#include <string.h>
#include "ameol2.h"
#include "admin.h"

static BOOL fDefDlgEx = FALSE;

#define  THIS_FILE   __FILE__

/* Validation errors.
 */
#define  VALIDATE_OK                0
#define  VALIDATE_BAD_USERNAME      1
#define  VALIDATE_BAD_PASSWORD      2

/* Permissions table.
 */
typedef struct tagPERMS {
   char * szDescription;            /* Pointer to description of permission */
   DWORD dwPerm;                    /* Permission value */
} PERMS;

static PERMS FAR PermTable[] = {
   "Can be Administrator",    PF_ADMINISTRATE,
   "Can run purge",           PF_CANPURGE,
   "Can create folders",      PF_CANCREATEFOLDERS,
   "Can use Internet",        PF_CANUSECIXIP,
   "Can change preferences",  PF_CANCONFIGURE
   };

/* Users table.
 */
#pragma pack(1)
typedef struct tagUSERS {
   char szUsername[ LEN_LOGINNAME ];            /* User name */
   char szPassword[ LEN_LOGINPASSWORD ];        /* User password */
   char szDir[ LEN_DIR ];                       /* Name of user directory (8 max. but rounded up) */
   DWORD dwPerms;                               /* Permissions assigned to user */
   WORD fOnline;                                /* Flag TRUE if user is online */
   WORD fDeleted;                               /* Flag TRUE if user is deleted */
} USERS;
#pragma pack()

typedef struct tagUSERLIST {
   struct tagUSERLIST FAR * lpuPrev;            /* Previous user */
   struct tagUSERLIST FAR * lpuNext;            /* Next user */
   DWORD dwOffset;                              /* Offset of user record in file */
   USERS us;                                    /* User details */
} USERLIST, FAR * LPUSERLIST;

static LPUSERLIST lpuFirst = NULL;              /* Pointer to first user */
static LPUSERLIST lpuLast = NULL;               /* Pointer to last user */
static BOOL fUsersListOpen = FALSE;             /* TRUE if users list is open */
static HNDFILE fhUsers = HNDFILE_ERROR;         /* Users data file handle */
static int cUsers;                              /* Number of users */
static WORD wUsersDate;                         /* Current modify date of users file */
static WORD wUsersTime;                         /* Current modify time of users file */

static HBITMAP hAdminBmps;                      /* Admin dialog bitmaps */
static HFONT hTitleFont;                        /* Font used for title in Login dialog */
static char szNoPassword[ LEN_LOGINPASSWORD ];  /* Empty password string */
static RECT rcList1;
static RECT rcList2;
static HFONT hTitleFont;                        /* Title font */
static BOOL fURLHandle = FALSE;

#ifdef WIN32
#pragma data_seg(".sdata")
extern LONG cInstances;                         /* Usage count */
#pragma data_seg()
#endif

LPUSERLIST lpuActive;                           /* Handle of active user */

static char szUsersFile[] = "users.dat";
static char szTmpUsersFile[] = "users.tmp";

#define  cMaxPerms   (sizeof(PermTable)/sizeof(PermTable[0]))

BOOL EXPORT FAR PASCAL LoginDlg( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL LoginDlg_DlgProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL LoginDlg_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL LoginDlg_OnCommand( HWND, int, HWND, UINT );

BOOL EXPORT FAR PASCAL AddUser( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL AddUser_DlgProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL AddUser_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL AddUser_OnCommand( HWND, int, HWND, UINT );
void FASTCALL AddUser_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );

BOOL EXPORT FAR PASCAL Admin( HWND, UINT, WPARAM, LPARAM );
LRESULT FASTCALL Admin_DlgProc( HWND, UINT, WPARAM, LPARAM );
BOOL FASTCALL Admin_OnInitDialog( HWND, HWND, LPARAM );
void FASTCALL Admin_OnCommand( HWND, int, HWND, UINT );
void FASTCALL Admin_OnTimer( HWND, UINT );
void FASTCALL Admin_OnMeasureItem( HWND, MEASUREITEMSTRUCT FAR * );
int FASTCALL Admin_OnCompareItem( HWND, const COMPAREITEMSTRUCT FAR * );
void FASTCALL Admin_OnDrawItem( HWND, const DRAWITEMSTRUCT FAR * );
int FASTCALL Admin_OnCharToItem( HWND, UINT, HWND, int );

void FASTCALL CreateUserDirectory( LPUSERLIST );
void FASTCALL FillAdminLists( HWND );
void FASTCALL AdminMakeDirField( HWND );
BOOL FASTCALL LoginUser( LPUSERLIST );
BOOL FASTCALL LogoutUser( LPUSERLIST );
BOOL FASTCALL RebuildUsersList( HWND );
BOOL FASTCALL UpdateUsersInfo( LPUSERLIST );
int FASTCALL ValidateLogin( char *, char * );
void FASTCALL ShowUserPermissions( HWND );
void FASTCALL ShowPermissionUsers( HWND );
void FASTCALL DeleteUser( LPUSERLIST );
void FASTCALL DeleteUserDatabase( LPUSERLIST );
void FASTCALL DeleteUserDirectory( LPUSERLIST );
HWND FASTCALL IsLoggedOn( char * );

BOOL CALLBACK EXPORT DeleteUserDatabaseEnumProc( DATABASEINFO FAR *, LPARAM );

#define MAX_HWNDARRAY 64

typedef struct
{ 
  int numwnds;
  HWND hwnd[MAX_HWNDARRAY];

} HWNDARRAY;

typedef HWNDARRAY FAR * LPHWNDARRAY;

HWND CheckFunc(LPSTR userName);
BOOL CALLBACK FindAmeol2Windows(HWND hWnd, LPARAM lParam);

/* This function opens the users list. No login can occur
 * until this is done.
 */
BOOL FASTCALL OpenUsersList( BOOL fSaveRegistry )
{
   /* Create the encrypted no-password string.
    */
   memset( szNoPassword, 0, LEN_LOGINPASSWORD );
   Amuser_Encrypt( szNoPassword, rgEncodeKey );

   /* Open the users data file for sharing if it
    * is not already open.
    */
   if( HNDFILE_ERROR == fhUsers )
      {
      wsprintf( lpTmpBuf, "%s\\%s", szUsersDir, szUsersFile );
      fhUsers = Amfile_Open( lpTmpBuf, AOF_SHARE_READWRITE );
      }

   /* File open, so off we go.
    */
   if( HNDFILE_ERROR != fhUsers )
      {
      DWORD dwOffset;
      USERS usr;

      /* Start from the beginning
       */
      Amfile_SetPosition( fhUsers, 0L, ASEEK_BEGINNING );

      /* Loop, reading each record from the file.
       */
      cUsers = 0;
      dwOffset = 0L;
      while( Amfile_Read( fhUsers, &usr, sizeof(USERS) ) == sizeof(USERS) )
         {
         LPUSERLIST lpu;

         /* Locate this user in the existing list and if found,
          * just update the details.
          */
         for( lpu = lpuFirst; lpu; lpu = lpu->lpuNext )
            if( _strcmpi( lpu->us.szUsername, usr.szUsername ) == 0 )
               {
               lpu->us = usr;
               break;
               }

         /* If the deleted flag is set, this is a deleted user.
          */
         if( usr.fDeleted && NULL != lpu )
            DeleteUser( lpu );

         /* This is a new user, so create a new record
          * and pop it into the list.
          */
         if( !usr.fDeleted && NULL == lpu )
            if( !fNewMemory( &lpu, sizeof(USERLIST) ) )
               return( FALSE );
            else
               {
               if( NULL == lpuFirst )
                  lpuFirst = lpu;
               else
                  lpuLast->lpuNext = lpu;
               lpu->lpuPrev = lpuLast;
               lpu->lpuNext = NULL;
               lpu->dwOffset = dwOffset;
               lpuLast = lpu;
               lpu->us = usr;
               if (fSaveRegistry)
                  Amuser_SaveRegistry(lpu->us.szDir);
               ++cUsers;
               }
         dwOffset += sizeof(USERS);
         }
      fUsersListOpen = TRUE;
      }

   /* If, after all this, there are no users, then create
    * one with all privileges and no password.
    */
   if( NULL == lpuFirst )
      {
      /* Close the file if it was opened (but corrupted)
       */
      if( HNDFILE_ERROR != fhUsers )
         Amfile_Close( fhUsers );

      /* Create the Users directory. May fail, but
       * never mind about that.
       */
      Amdir_Create( szUsersDir );

      /* First, create the user file.
       */
      fhUsers = Amfile_Create( lpTmpBuf, 0 );
      if( HNDFILE_ERROR == fhUsers )
         return( FALSE );
      fUsersListOpen = TRUE;

      /* Next create the ADMIN record.
       */
      if( fNewMemory( &lpuFirst, sizeof(USERLIST) ) )
         {
         lpuFirst->lpuPrev = lpuLast;
         lpuFirst->lpuNext = NULL;
         lpuFirst->dwOffset = 0L;
         lpuLast = lpuFirst;
         lstrcpy( lpuFirst->us.szUsername, "Administrator" );
         lstrcpy( lpuFirst->us.szDir, "ADMIN" );
         memcpy( lpuFirst->us.szPassword, szNoPassword, LEN_LOGINPASSWORD );
         lpuFirst->us.dwPerms = PF_ALL;
         lpuFirst->us.fOnline = FALSE;
         lpuFirst->us.fDeleted = FALSE;

         /* Write this record to the file.
          */
         if( Amfile_Write( fhUsers, (LPCSTR)&lpuFirst->us, sizeof(USERS) ) != sizeof(USERS) )
            return( FALSE );

         /* Make ADMIN the active user.
          */
         CreateUserDirectory( lpuFirst );
         Amuser_SetActiveUser( lpuFirst->us.szDir );
         }
      }

   /* Save the date/time of this file.
    */
   Amfile_GetFileTime( fhUsers, &wUsersDate, &wUsersTime );
   return( lpuFirst != NULL );
}

/* This function updates the users settings in the
 * users file for the specified user.
 */
BOOL FASTCALL UpdateUsersInfo( LPUSERLIST lpu )
{
   if( -1 == Amfile_SetPosition( fhUsers, lpu->dwOffset, ASEEK_BEGINNING ) )
      return( FALSE );
   if( Amfile_Write( fhUsers, (LPCSTR)&lpu->us, sizeof(USERS) ) != sizeof(USERS) )
      return( FALSE );
   return( TRUE );
}

/* This function adds a new user to the users file. It
 * appends to the end of the users file.
 */
BOOL FASTCALL AddUsersInfo( LPUSERLIST lpu )
{
   lpu->dwOffset = Amfile_SetPosition( fhUsers, 0L, ASEEK_END );
   if( Amfile_Write( fhUsers, (LPCSTR)&lpu->us, sizeof(USERS) ) != sizeof(USERS) )
      return( FALSE );
   return( TRUE );
}

/* This function removes a user from the users file. Just
 * mark the entry as deleted and the file will get compacted
 * later.
 */
BOOL FASTCALL RemoveUsersInfo( LPUSERLIST lpu )
{
   lpu->us.fDeleted = TRUE;
   if( -1 == Amfile_SetPosition( fhUsers, lpu->dwOffset, ASEEK_BEGINNING ) )
      return( FALSE );
   if( Amfile_Write( fhUsers, (LPCSTR)&lpu->us, sizeof(USERS) ) != sizeof(USERS) )
      return( FALSE );
   return( TRUE );
}

/* This function saves the users list.
 */
BOOL FASTCALL RebuildUsersList( HWND hwnd )
{
   HNDFILE fh;
   BOOL fOk;

   /* Close the file, if it is open.
    */
   if( HNDFILE_ERROR != fhUsers )
      Amfile_Close( fhUsers );

   /* Need exclusive access to the file, so try and
    * open it like that.
    */
   wsprintf( lpTmpBuf, "%s\\%s", szUsersDir, szUsersFile );
   fh = Amfile_Open( lpTmpBuf, AOF_READ );
   if( HNDFILE_ERROR == fh )
      return( FALSE );
   Amfile_Close( fh );

   /* Now rebuild it.
    */
   fOk = TRUE;
   wsprintf( lpTmpBuf, "%s\\%s", szUsersDir, szTmpUsersFile );
   if( ( fh = Amfile_Create( lpTmpBuf, 0 ) ) == HNDFILE_ERROR )
      fOk = FALSE;
   else
      {
      LPUSERLIST lpu;
      DWORD dwOffset;

      /* Loop and write each user entry.
       */
      dwOffset = 0;
      for( lpu = lpuFirst; lpu; lpu = lpu->lpuNext )
         if( !lpu->us.fDeleted )
            {
            HWND hwndOther;

            hwndOther = IsLoggedOn( lpu->us.szUsername  );
            if( NULL == hwndOther )
               lpu->us.fOnline = FALSE;
            while( Amfile_Write( fh, (LPCSTR)&lpu->us, sizeof(USERS) ) != sizeof(USERS) )
               if( !DiskWriteError( hwnd, lpTmpBuf, TRUE, FALSE ) )
                  {
                  fOk = FALSE;
                  break;
                  }
            if( !fOk )
               break;
            lpu->dwOffset = dwOffset;
            dwOffset += sizeof(USERS);
            }
      Amfile_Close( fh );

      /* If all saved okay, rename the temporary file to
       * the actual file. Otherwise delete the file we
       * created.
       */
      if( fOk )
         {
         char sz[ _MAX_PATH ];

         wsprintf( lpTmpBuf, "%s\\%s", szUsersDir, szTmpUsersFile );
         wsprintf( sz, "%s\\%s", szUsersDir, szUsersFile );
         Amfile_Delete( sz );
         Amfile_Rename( lpTmpBuf, sz );
         fhUsers = Amfile_Open( sz, AOF_SHARE_READWRITE );
         }
      else
         Amfile_Delete( lpTmpBuf );
      }
   return( fOk );
}

/* This function closes the current user list.
 */
void FASTCALL CloseUsersList( void )
{
   LPUSERLIST lpu;
   LPUSERLIST lpuNext;

   ASSERT( fUsersListOpen );

   /* Log-out the current user
    */
   if( lpuActive )
      LogoutUser( lpuActive );

   /* Close the users file, if one is
    * open.
    */
   if( HNDFILE_ERROR != fhUsers )
      {
      Amfile_Close( fhUsers );
      fhUsers = HNDFILE_ERROR;
      }

   /* Try and recompact the file if we have exclusive
    * access to it.
    */
   if( RebuildUsersList( hwndFrame ) )
      Amfile_Close( fhUsers );

   /* Delete the list from memory.
    */
   for( lpu = lpuFirst; lpu; lpu = lpuNext )
      {
      lpuNext = lpu->lpuNext;
      FreeMemory( &lpu );
      }
   lpuFirst = lpuLast = NULL;
   fUsersListOpen = FALSE;
}

/* This function queries the number of active users in
 * the user's database.
 */
int FASTCALL QueryUserCount( void )
{
   return( cUsers );
}

/* This function returns the name of the active user
 */
LPSTR FASTCALL GetActiveUsername( void )
{
   if( lpuActive )
      return( lpuActive->us.szUsername );
   return( "" );
}

/* This function returns the password the active user
 */
LPSTR FASTCALL GetActivePassword( void )
{
   if( lpuActive )
      return( lpuActive->us.szPassword );
   return( "" );
}

/* This function updates the users list. It checks the
 * timestamp on the file and if they differ, it reloads
 * the entire list.
 */
BOOL FASTCALL RefreshUsersList( void )
{
   WORD date;
   WORD time;

   ASSERT( fUsersListOpen );
   Amfile_GetFileTime( fhUsers, &date, &time );
   if( date != wUsersDate || time != wUsersTime )
      {
      OpenUsersList( FALSE );
      return( TRUE );
      }
   return( FALSE );
}

/* This function handles the Login sequence. Return
 * FALSE if the login failed.
 */
BOOL FASTCALL Login( char * pszUsername, char * pszPassword )
{
   LPUSERLIST lpuLogin;
   BOOL fFoundAdmin = FALSE;

   /* If user name and/or password supplied, validate them.
    */
   lpuLogin = NULL;
   if( _strcmpi( pszUsername, "Admin" ) == 0 )
   {
      LPUSERLIST lpu;
      for( lpu = lpuFirst; lpu; lpu = lpu->lpuNext )
      {
         if( _strcmpi( lpu->us.szUsername, "Admin" ) == 0 )
            {
            fFoundAdmin = TRUE;
            break;
            }
      }
      if( !fFoundAdmin )
         pszUsername = "Administrator";
   }

   if( !*pszUsername )
      {
      if( NULL == lpuFirst->lpuNext )
         lpuLogin = lpuFirst;
      }
   else
      {
      LPUSERLIST lpu;

      for( lpu = lpuFirst; lpu; lpu = lpu->lpuNext )
         if( _strcmpi( lpu->us.szUsername, pszUsername ) == 0 )
            {
            lpuLogin = lpu;
            break;
            }
      if( NULL == lpuLogin )
         {
         wsprintf( lpTmpBuf, "%s is an invalid login name", pszUsername );
         fMessageBox( NULL, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
         return( FALSE );
         }
      }

   /* If only one user and no password, OK.
    */
   if( lpuLogin && memcmp( lpuLogin->us.szPassword, szNoPassword, LEN_LOGINPASSWORD ) == 0 )
      return( LoginUser( lpuLogin ) );

   /* If both username and password supplied, validate them.
    */
   if( *pszUsername && *pszPassword )
      {
      int nRetCode;

      nRetCode = ValidateLogin( pszUsername, pszPassword );
      if( VALIDATE_OK == nRetCode )
         return( LoginUser( lpuLogin ) );
      if( VALIDATE_BAD_PASSWORD == nRetCode )
         wsprintf( lpTmpBuf, "Invalid password for %s", pszUsername );
      else
         wsprintf( lpTmpBuf, "%s is an invalid login name", pszUsername );
      fMessageBox( NULL, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
      }


   Amuser_GetLMString( szInfo, "DefaultUser", "", lpTmpBuf, LEN_TEMPBUF );
   if( ( fInitNewsgroup || fMailto || fCixLink ) && *lpTmpBuf )
      {
         HWND hwndOther;

         strcpy( pszUsername, lpTmpBuf );
         hwndOther = IsLoggedOn( lpTmpBuf );

         if( hwndOther )
         {
      #ifdef WIN32
            COPYDATASTRUCT cds;

            cds.dwData = WM_PARSEURL;
            cds.lpData = fInitNewsgroup ? szNewsgroup : fCixLink ? szCixLink : szMailtoAddress;
            cds.cbData = strlen( (char *)cds.lpData ) + 1;
            SendMessage( hwndOther, WM_COPYDATA, (WPARAM)hwndFrame, (LPARAM)&cds );
            fURLHandle = TRUE;
      #else
         if( fInitNewsgroup )
            {
            SendMessage( hwndOther, WM_PARSEURL, 0, (LPARAM)(LPSTR)szNewsgroup );
            fURLHandle = TRUE;
            }
         else if( fCixLink )
            {
            SendMessage( hwndOther, WM_PARSEURL, 0, (LPARAM)(LPSTR)szCixLink );
            fURLHandle = TRUE;
            }
         else if( fMailto )
            {
            SendMessage( hwndOther, WM_PARSEURL, 0, (LPARAM)(LPSTR)szMailtoAddress );
            fURLHandle = TRUE;
            }
      #endif
         return( FALSE );
         }
         else
            {
            LPUSERLIST lpu;

            for( lpu = lpuFirst; lpu; lpu = lpu->lpuNext )
               if( _strcmpi( lpu->us.szUsername, pszUsername ) == 0 )
                  {
                  lpuLogin = lpu;
                  break;
                  }
            if( NULL == lpuLogin )
               {
               wsprintf( lpTmpBuf, "%s is an invalid login name", pszUsername );
               fMessageBox( NULL, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION );
               return( FALSE );
               }
            }
         /* If only one user and no password, OK.
          */
         if( lpuLogin && memcmp( lpuLogin->us.szPassword, szNoPassword, LEN_LOGINPASSWORD ) == 0 )
            return( LoginUser( lpuLogin ) );
      }


   /* In any other circumstance, we need the password.
    */
   while( TRUE )
      {
      if( !(BOOL)Adm_Dialog( hInst, NULL, MAKEINTRESOURCE(IDDLG_LOGIN), LoginDlg, (LPARAM)lpuLogin ) )
         return( FALSE );
      if( LoginUser( lpuActive ) )
         break;
      else
      {
         HWND hwndCur;

         hwndCur = CheckFunc(lpuActive->us.szUsername);                  // 2.55.2032
         if(IsWindow(hwndCur))            
         {
            SetForegroundWindow(hwndCur);
            if(IsMinimized(hwndCur))
               ShowWindow(hwndCur, SW_RESTORE);
            return( FALSE );
         }
      }
      if( fURLHandle )
         return( FALSE );
      }
   return( TRUE );
}

/* This function tests a permission flag against the logged in
 * user.
 */
BOOL FASTCALL TestPerm( DWORD dwFlag )
{
   if( NULL == lpuActive )
      return( FALSE );
   return( ( lpuActive->us.dwPerms & dwFlag ) == dwFlag );
}

/* This function logs in the specified user.
 */
BOOL FASTCALL LoginUser( LPUSERLIST lpu )
{
   /* Assume nobody logged on
    */
   lpuActive = NULL;
   fURLHandle = FALSE;

   /* Get this user's record from the file.
    */
   if( -1 == Amfile_SetPosition( fhUsers, lpu->dwOffset, ASEEK_BEGINNING ) )
      return( FALSE );
   if( Amfile_Read( fhUsers, &lpu->us, sizeof(USERS) ) != sizeof(USERS) )
      return( FALSE );

   /* Already logged in? If this is not the first instance, then
    * fail the login.
    */
   if( lpu->us.fOnline )
      {
      HWND hwndOther;

      hwndOther = IsLoggedOn( lpu->us.szUsername );
      if( !hwndOther )
         lpu->us.fOnline = FALSE;
      else
         {
      #ifdef WIN32
         if( fInitNewsgroup || fMailto || fCixLink )
            {
            COPYDATASTRUCT cds;

            cds.dwData = WM_PARSEURL;
            cds.lpData = fInitNewsgroup ? szNewsgroup : fCixLink ? szCixLink : szMailtoAddress;
            cds.cbData = strlen( (char *)cds.lpData ) + 1;
            SendMessage( hwndOther, WM_COPYDATA, (WPARAM)hwndFrame, (LPARAM)&cds );
            fURLHandle = TRUE;
            }
      #else
         if( fInitNewsgroup )
            {
            SendMessage( hwndOther, WM_PARSEURL, 0, (LPARAM)(LPSTR)szNewsgroup );
            fURLHandle = TRUE;
            }
         else if( fCixLink )
            {
            SendMessage( hwndOther, WM_PARSEURL, 0, (LPARAM)(LPSTR)szCixLink );
            fURLHandle = TRUE;
            }
         else if( fMailto )
            {
            SendMessage( hwndOther, WM_PARSEURL, 0, (LPARAM)(LPSTR)szMailtoAddress );
            fURLHandle = TRUE;
            }
      #endif
         else
            {
               wsprintf( lpTmpBuf, GS(IDS_STR1060), lpu->us.szUsername );
               lpuActive = lpu;
//             fMessageBox( NULL, 0, lpTmpBuf, MB_OK|MB_ICONEXCLAMATION ); //!!SM!! 2029
            }
         return( FALSE );
         }
      }

   /* Make this user the active user and update the
    * record in the file.
    */
   lpu->us.fOnline = TRUE;
   if( -1 == Amfile_SetPosition( fhUsers, lpu->dwOffset, ASEEK_BEGINNING ) )
      return( FALSE );
   if( Amfile_Write( fhUsers, (LPCSTR)&lpu->us, sizeof(USERS) ) != sizeof(USERS) )
      return( FALSE );

   /* Update our last-write time and date info.
    */
   Amfile_GetFileTime( fhUsers, &wUsersDate, &wUsersTime );

   /* Tell AMUSER that this is the active user.
    */
   lpuActive = lpu;
   Amuser_SetActiveUser( lpu->us.szDir );
   return( TRUE );
}

/* Returns whether or not the specified user is logged
 * on. If the user is logged on, it returns the frame window
 * handle of that instance.
 */
HWND FASTCALL IsLoggedOn( char * pszUsername )
{
   HWND hwndOther;

// CreateAppCaption( lpTmpBuf, pszUsername );
// hwndOther = FindWindow( szFrameWndClass, lpTmpBuf );
   hwndOther = CheckFunc( pszUsername );
#ifdef WIN32
   if( NULL == hwndOther && 1 == cInstances )
      return( hwndOther );
#endif
   return( hwndOther );
}

/* This function creates the application caption bar. Under Windows 95,
 * the order is username:appname.
 */
void FASTCALL CreateAppCaption( char * pszBuf, char * pszUsername )
{
   if( wWinVer >= 0x35F )
      wsprintf( lpTmpBuf, "%s - %s", pszUsername, szAppName );
   else
      wsprintf( lpTmpBuf, "%s - %s", szAppName, pszUsername );
}

/* This function logs out the specified user.
 */
BOOL FASTCALL LogoutUser( LPUSERLIST lpu )
{
   /* Clear the online flag.
    */
   lpu->us.fOnline = FALSE;

   /* Get this user's record from the file.
    */
   if( -1 == Amfile_SetPosition( fhUsers, lpu->dwOffset, ASEEK_BEGINNING ) )
      return( FALSE );
   lpu->us.fOnline = FALSE;
   if( Amfile_Read( fhUsers, &lpu->us, sizeof(USERS) ) != sizeof(USERS) )
      return( FALSE );
   lpu->us.fOnline = FALSE;

   if( -1 == Amfile_SetPosition( fhUsers, lpu->dwOffset, ASEEK_BEGINNING ) )
      return( FALSE );
   lpu->us.fOnline = FALSE;
   if( Amfile_Write( fhUsers, (LPCSTR)&lpu->us, sizeof(USERS) ) != sizeof(USERS) )
      return( FALSE );
   lpu->us.fOnline = FALSE;
   return( TRUE );
}

/* This function validates the specified login and password.
 */
int FASTCALL ValidateLogin( char * pszUsername, char * pszPassword )
{
   char szLclPassword[ LEN_LOGINPASSWORD ];
   LPUSERLIST lpu;
   BOOL fFoundAdmin = FALSE;

   /* Copy and encrypt password.
    */
   memset( szLclPassword, 0, LEN_LOGINPASSWORD );
   strcpy( szLclPassword, pszPassword );
   Amuser_Encrypt( szLclPassword, rgEncodeKey );

   /* Scan for the specified login name in the list. If found,
    * compare passwords if required.
    */

   /* If user name and/or password supplied, validate them.
    */
   if( _strcmpi( pszUsername, "Admin" ) == 0 )
   {
      for( lpu = lpuFirst; lpu; lpu = lpu->lpuNext )
      {
         if( _strcmpi( lpu->us.szUsername, "Admin" ) == 0 )
            {
            fFoundAdmin = TRUE;
            break;
            }
      }
      if( !fFoundAdmin )
         pszUsername = "Administrator";
   }
   for( lpu = lpuFirst; lpu; lpu = lpu->lpuNext )
      if( _strcmpi( lpu->us.szUsername, pszUsername ) == 0 )
         {
         if( memcmp( lpu->us.szPassword, szLclPassword, LEN_LOGINPASSWORD ) == 0 )
            {
            lpuActive = lpu;
            return( VALIDATE_OK );
            }
         return( VALIDATE_BAD_PASSWORD );
         }
   return( VALIDATE_BAD_USERNAME );
}

/* This function tests whether the specified password is
 * blank. A blank password is still encoded.
 */
BOOL FASTCALL BlankPassword( char * pPassword )
{
   char szPassword[ LEN_LOGINPASSWORD ];

   memset( szPassword, 0, LEN_LOGINPASSWORD );
   Amuser_Encrypt( szPassword, rgEncodeKey );
   return( memcmp( szPassword, pPassword, LEN_LOGINPASSWORD ) == 0 );
}

/* This function handles the Admin dialog box.
 */
BOOL EXPORT CALLBACK LoginDlg( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = LoginDlg_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the Admin
 *  dialog.
 */
LRESULT FASTCALL LoginDlg_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, LoginDlg_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, LoginDlg_OnCommand );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsLOGIN );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL LoginDlg_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   DWORD dwVersion;

   /* Set the edit field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_USER ), LEN_LOGINNAME - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_PASSWORD ), LEN_LOGINPASSWORD - 1 );

   /* Set the font for the Ameol logo.
    */
   hTitleFont = CreateFont( 28, 0, 0, 0, FW_BOLD, FALSE, FALSE,
                           FALSE, ANSI_CHARSET, OUT_CHARACTER_PRECIS,
                           CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
                           VARIABLE_PITCH | FF_SWISS, "Times New Roman" );
   SetWindowFont( GetDlgItem( hwnd, IDD_PAD1 ), hTitleFont, FALSE );

   /* Show version details.
    */
   dwVersion = Ameol2_GetVersion();
   wsprintf( lpTmpBuf, "Version %s", Ameol2_ExpandVersion( dwVersion ) );
   SetWindowText( GetDlgItem( hwnd, IDD_VERSION ), lpTmpBuf );

   /* Save pointer to login handle.
    */
   SetWindowLong( hwnd, DWL_USER, lParam );

   /* If there's just one user, set the focus to the Password
    * field and fill out the username.
    */
   if( 0L != lParam )
      {
      LPUSERLIST lpu;

      lpu = (LPUSERLIST)lParam;
      Edit_SetText( GetDlgItem( hwnd, IDD_USER ), lpu->us.szUsername );
      SetFocus( GetDlgItem( hwnd, IDD_PASSWORD ) );
      return( FALSE );
      }
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL LoginDlg_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDOK: {
         char szUsername[ LEN_LOGINNAME ];
         char szPassword[ LEN_LOGINPASSWORD ];
         int nRetCode;

         /* Get the login and username.
          */
         Edit_GetText( GetDlgItem( hwnd, IDD_USER ), szUsername, LEN_LOGINNAME );
         Edit_GetText( GetDlgItem( hwnd, IDD_PASSWORD ), szPassword, LEN_LOGINPASSWORD );
         StripLeadingTrailingSpaces( szUsername );

         /* Validate them
          */
         nRetCode = ValidateLogin( szUsername, szPassword );
         if( VALIDATE_BAD_PASSWORD == nRetCode )
            {
            HighlightField( hwnd, IDD_PASSWORD );
            fMessageBox( hwnd, 0, "Incorrect password", MB_OK|MB_ICONEXCLAMATION );
            break;
            }
         else if( VALIDATE_BAD_USERNAME == nRetCode )
            {
            HighlightField( hwnd, IDD_USER );
            fMessageBox( hwnd, 0, "Invalid user name", MB_OK|MB_ICONEXCLAMATION );
            break;
            }
         }

      case IDCANCEL:
         DeleteFont( hTitleFont );
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* This function displays the Admin dialog.
 */
void FASTCALL CmdAdmin( HWND hwnd )
{
   /* Load the bitmaps we're going to use.
    */
   hAdminBmps = LoadBitmap( hRscLib, MAKEINTRESOURCE(IDB_THREADBITMAPS) );
   Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_ADMIN), Admin, 0L );
}

/* This function handles the Admin dialog box.
 */
BOOL EXPORT CALLBACK Admin( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = Admin_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the Admin
 *  dialog.
 */
LRESULT FASTCALL Admin_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, Admin_OnInitDialog );
      HANDLE_MSG( hwnd, WM_COMMAND, Admin_OnCommand );
      HANDLE_MSG( hwnd, WM_DRAWITEM, Admin_OnDrawItem );
      HANDLE_MSG( hwnd, WM_MEASUREITEM, Admin_OnMeasureItem );
      HANDLE_MSG( hwnd, WM_COMPAREITEM, Admin_OnCompareItem );
      HANDLE_MSG( hwnd, WM_TIMER, Admin_OnTimer );

      case WM_ADMHELP:
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, idsADMIN );
         break;

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL Admin_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndList1;
   HWND hwndList2;

   /* Get the listboxes and their
    * dialog coordinates.
    */
   hwndList1 = GetDlgItem( hwnd, IDD_LIST1 );
   hwndList2 = GetDlgItem( hwnd, IDD_LIST2 );
   GetWindowRect( hwndList1, &rcList1 );
   GetWindowRect( hwndList2, &rcList2 );
   MapWindowPoints( NULL, hwnd, (LPPOINT)&rcList1, 2 );
   MapWindowPoints( NULL, hwnd, (LPPOINT)&rcList2, 2 );

   /* Start by showing permissions.
    */
   CheckDlgButton( hwnd, IDD_SHOWUSERS, TRUE );
   FillAdminLists( hwnd );

   /* Start a refresh timer going.
    */
   SetTimer( hwnd, 0, 5000, NULL );
   return( TRUE );
}

/* This function handles the WM_TIMER message.
 */
void FASTCALL Admin_OnTimer( HWND hwnd, UINT id )
{
   if( RefreshUsersList() )
      FillAdminLists( hwnd );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL Admin_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_SHOWPERMISSIONS:
      case IDD_SHOWUSERS:
         FillAdminLists( hwnd );
         break;

      case IDD_LIST1:
         if( codeNotify == LBN_DBLCLK )
            goto E1;
         else if( IsDlgButtonChecked( hwnd, IDD_SHOWUSERS ) && codeNotify == LBN_SELCHANGE )
            ShowUserPermissions( hwnd );
         break;

      case IDD_LIST2:
         if( codeNotify == LBN_SELCHANGE && IsDlgButtonChecked( hwnd, IDD_SHOWPERMISSIONS ) )
            ShowPermissionUsers( hwnd );
         break;

      case IDD_ADD:
         Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_ADDUSER), AddUser, 0L );
         PostMessage( hwnd, WM_NEXTDLGCTL, (WPARAM)GetDlgItem( hwnd, IDCANCEL ), (LPARAM)TRUE );
         break;

      case IDD_EDIT: {
         LPUSERLIST lpu;
         HWND hwndList;
         int index;

         /* Get the selected user.
          */
E1:      hwndList = GetDlgItem( hwnd, IDD_LIST1 );
         if( LB_ERR != ( index = ListBox_GetCurSel( hwndList ) ) )
            {
            lpu = (LPUSERLIST)ListBox_GetItemData( hwndList, index );
            ASSERT( NULL != lpu );
            if( Adm_Dialog( hInst, hwnd, MAKEINTRESOURCE(IDDLG_EDITUSER), AddUser, (LPARAM)lpu ) )
               {
               UpdateUsersInfo( lpu );
               if( IsDlgButtonChecked( hwnd, IDD_SHOWUSERS ) )
                  ShowUserPermissions( hwnd );
               else
                  ShowPermissionUsers( hwnd );
               }
            }
         SetFocus( hwndList );
         break;
         }

      case IDD_REMOVE: {
         LPUSERLIST lpu;
         HWND hwndList;
         int index;
         int count;

         /* Remove specified user. Some points to bear in mind:
          * - Cannot remove ALL users.
          * - Cannot remove ALL administrators.
          * These are handled easily by prohibiting the user from removing
          * himself!
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST1 );
         index = ListBox_GetCurSel( hwndList );
         lpu = (LPUSERLIST)ListBox_GetItemData( hwndList, index );
         ASSERT( NULL != lpu );

         /* Is user online? Cannot remove if so.
          */
         if( lpu->us.fOnline )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR1062), MB_OK|MB_ICONEXCLAMATION );
            break;
            }

         /* Is this the current user?
          */
         if( strcmp( lpu->us.szUsername, lpuActive->us.szUsername ) == 0 )
            {
            fMessageBox( hwnd, 0, GS(IDS_STR806), MB_OK|MB_ICONEXCLAMATION );
            break;
            }

         /* All conditions satisified. Prompt whether or not to delete
          * the actual user.
          */
         wsprintf( lpTmpBuf, GS(IDS_STR807), lpu->us.szUsername );
         if( fMessageBox( hwnd, 0, lpTmpBuf, MB_YESNO|MB_ICONINFORMATION ) == IDNO )
            break;

         /* Remove this user from the index.
          */
         RemoveUsersInfo( lpu );

         /* Now delete the user.
          */
         DeleteUserDatabase( lpu );
         DeleteUserDirectory( lpu );
         DeleteUser( lpu );

         /* Remove this listbox item.
          */
         count = ListBox_DeleteString( hwndList, index );
         if( index == count )
            --index;
         ListBox_SetCurSel( hwndList, index );
         ShowUserPermissions( hwnd );
         SetFocus( hwndList );
         break;
         }

      case IDOK:
      case IDCANCEL:
         DeleteBitmap( hAdminBmps );
         KillTimer( hwnd, 0 );
         EndDialog( hwnd, id == IDOK );
         break;
      }
}

/* This function handles the WM_CHARTOITEM message.
 */
int FASTCALL Admin_OnCharToItem( HWND hwnd, UINT ch, HWND hwndList, int iCaret )
{
   register int c;
   register int count;
   int index;

   count = ListBox_GetCount( hwndList );
   index = ListBox_GetCurSel( hwndList );
   c = index;
   do {
      LPUSERLIST lpc;

      if( ++c == count )
         c = 0;
      lpc = (LPUSERLIST)ListBox_GetItemData( hwndList, c );
      if( lpc->us.szUsername[ 0 ] == (char)ch )
         return( c );
   } while( c != index );
   return( -1 );
}

/* This function handles the WM_COMPAREITEM message.
 */
int FASTCALL Admin_OnCompareItem( HWND hwnd, const COMPAREITEMSTRUCT FAR * lpCompareItem )
{
   LPUSERLIST lpu2;
   LPUSERLIST lpu1;

   lpu2 = (LPUSERLIST)lpCompareItem->itemData2;
   lpu1 = (LPUSERLIST)lpCompareItem->itemData1;
   return( lstrcmpi( lpu1->us.szUsername, lpu2->us.szUsername ) );
}

/* This function handles the WM_MEASUREITEM message.
 */
void FASTCALL Admin_OnMeasureItem( HWND hwnd, MEASUREITEMSTRUCT FAR * lpMeasureItem )
{
   TEXTMETRIC tm;
   BITMAP bmp;
   HFONT hFont;
   HDC hdc;

   hdc = GetDC( hwnd );
   hFont = SelectFont( hdc, hHelvB8Font );
   GetTextMetrics( hdc, &tm );
   GetObject( hAdminBmps, sizeof( BITMAP ), &bmp );
   lpMeasureItem->itemHeight = max( tm.tmHeight, bmp.bmHeight );
   SelectFont( hdc, hFont );
   ReleaseDC( hwnd, hdc );
}

/* This function handles the WM_DRAWITEM message.
 */
void FASTCALL Admin_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   if( lpDrawItem->itemID != -1 )
      {
      COLORREF tmpT;
      COLORREF tmpB;
      COLORREF T;
      COLORREF B;
      LPUSERLIST lpu;
      HFONT hOldFont;
      HBRUSH hbr;
      BOOL fAdmin;
      RECT rc;
      HWND hwndOther;

      /* Get listbox colours.
       */
      lpu = (LPUSERLIST)ListBox_GetItemData( lpDrawItem->hwndItem, lpDrawItem->itemID );
      rc = lpDrawItem->rcItem;
      GetOwnerDrawListItemColours( lpDrawItem, &T, &B );
      hbr = CreateSolidBrush( B );

      /* Erase the entire drawing area
       */
      FillRect( lpDrawItem->hDC, &rc, hbr );
      tmpT = SetTextColor( lpDrawItem->hDC, T );
      tmpB = SetBkColor( lpDrawItem->hDC, B );

      /* Draw the bitmap first 
       */
      fAdmin = ( lpu->us.dwPerms & PF_ADMINISTRATE ) == PF_ADMINISTRATE;
      Amuser_DrawBitmap( lpDrawItem->hDC, rc.left, rc.top, 16, 16, hAdminBmps, fAdmin ? ABMP_ADMIN : ABMP_USER );
      rc.left += 18;

      /* Now draw the participants CIX name
       */
      hOldFont = SelectFont( lpDrawItem->hDC, hHelv8Font );
      hwndOther = IsLoggedOn( lpu->us.szUsername  );
      if( NULL != hwndOther )
         lpu->us.fOnline = TRUE;
      else
         lpu->us.fOnline = FALSE;
      strcpy( lpTmpBuf, lpu->us.szUsername );
      if( lpu->us.fOnline )
         strcat( lpTmpBuf, " (Logged on)" );
      DrawText( lpDrawItem->hDC, lpTmpBuf, -1, &rc, DT_NOPREFIX|DT_SINGLELINE|DT_VCENTER );
      SelectFont( lpDrawItem->hDC, hOldFont );

      /* Draw a focus if needed.
       */
      if( lpDrawItem->itemState & ODS_FOCUS )
         DrawFocusRect( lpDrawItem->hDC, &lpDrawItem->rcItem );

      /* Restore things back to normal.
       */
      SetTextColor( lpDrawItem->hDC, tmpT );
      SetBkColor( lpDrawItem->hDC, tmpB );
      DeleteBrush( hbr );
      }
}

/* This function fills the Admin dialog list boxes depending
 * on which of the buttons are checked.
 */
void FASTCALL FillAdminLists( HWND hwnd )
{
   HWND hwndList1;
   HWND hwndList2;

   /* Get the listbox handles.
    */
   hwndList1 = GetDlgItem( hwnd, IDD_LIST1 );
   hwndList2 = GetDlgItem( hwnd, IDD_LIST2 );

   /* Now fill the list boxes as appropriate.
    */
   if( IsDlgButtonChecked( hwnd, IDD_SHOWPERMISSIONS ) )
      {
      register int c;
      int sel;

      /* Shuffle the windows.
       */
      SetWindowPos( hwndList2, NULL, rcList1.left, rcList1.top, 0, 0, SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE );
      SetWindowPos( hwndList1, NULL, rcList2.left, rcList2.top, 0, 0, SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE );

      /* Fill the first list box with the list of
       * permissions.
       */
      sel = ListBox_GetCurSel( hwndList2 );
      if( LB_ERR == sel )
         sel = 0;
      ListBox_ResetContent( hwndList2 );
      for( c = 0; c < cMaxPerms; ++c )
         {
         int index;

         index = ListBox_AddString( hwndList2, PermTable[ c ].szDescription );
         ListBox_SetItemData( hwndList2, index, PermTable[ c ].dwPerm );
         }
      if( sel >= ListBox_GetCount( hwndList2 ) )
         sel = ListBox_GetCount( hwndList2 ) - 1;
      ListBox_SetCurSel( hwndList2, sel );
      ShowPermissionUsers( hwnd );
      SetFocus( hwndList2 );
      }
   else
      {
      LPUSERLIST lpu;
      int sel;

      /* Shuffle the windows.
       */
      SetWindowPos( hwndList1, NULL, rcList1.left, rcList1.top, 0, 0, SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE );
      SetWindowPos( hwndList2, NULL, rcList2.left, rcList2.top, 0, 0, SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE );

      /* Fill the first list box with the list of
       * users.
       */
      sel = ListBox_GetCurSel( hwndList1 );
      if( LB_ERR == sel )
         sel = 0;
      ListBox_ResetContent( hwndList1 );
      lpu = lpuFirst;
      while( lpu )
         {
         ListBox_AddString( hwndList1, (LPARAM)lpu );
         lpu = lpu->lpuNext;
         }
      if( sel >= ListBox_GetCount( hwndList1 ) )
         sel = ListBox_GetCount( hwndList1 ) - 1;
      ListBox_SetCurSel( hwndList1, sel );
      ShowUserPermissions( hwnd );
      SetFocus( hwndList1 );
      }
}

/* This function shows the permissions for the selected user.
 */
void FASTCALL ShowUserPermissions( HWND hwnd )
{
   HWND hwndList1;
   HWND hwndList2;
   LPUSERLIST lpu;
   register int c;
   int index;

   /* Get the selected user.
    */
   hwndList1 = GetDlgItem( hwnd, IDD_LIST1 );
   ASSERT( NULL != hwndList1 );
   index = ListBox_GetCurSel( hwndList1 );
   ASSERT( index != LB_ERR );
   lpu = (LPUSERLIST)ListBox_GetItemData( hwndList1, index );

   /* Fill the listbox with the users permissions.
    */
   hwndList2 = GetDlgItem( hwnd, IDD_LIST2 );
   ListBox_ResetContent( hwndList2 );
   for( c = 0; c < cMaxPerms; ++c )
      if( lpu->us.dwPerms & PermTable[ c ].dwPerm )
         ListBox_InsertString( hwndList2, -1, PermTable[ c ].szDescription );
}

/* This function shows the users assigned the selected permission.
 */
void FASTCALL ShowPermissionUsers( HWND hwnd )
{
   HWND hwndList1;
   HWND hwndList2;
   LPUSERLIST lpu;
   DWORD dwPerms;
   int index;
   int count;

   /* Get the selected permission.
    */
   hwndList2 = GetDlgItem( hwnd, IDD_LIST2 );
   ASSERT( NULL != hwndList2 );
   index = ListBox_GetCurSel( hwndList2 );
   ASSERT( index != LB_ERR );
   dwPerms = ListBox_GetItemData( hwndList2, index );

   /* Fill the listbox with those users assigned the
    * selected permission.
    */
   hwndList1 = GetDlgItem( hwnd, IDD_LIST1 );
   ListBox_ResetContent( hwndList1 );
   for( lpu = lpuFirst; lpu; lpu = lpu->lpuNext )
      if( lpu->us.dwPerms & dwPerms )
         ListBox_AddString( hwndList1, (LPARAM)lpu );

   /* Set the selection.
    */
   if( ( count = ListBox_GetCount( hwndList1 ) ) > 0 )
      ListBox_SetCurSel( hwndList1, 0 );
   EnableControl( hwnd, IDD_REMOVE, count > 0 );
   EnableControl( hwnd, IDD_EDIT, count > 0 );
}

/* This function ensures that the specified user directory
 * actually exists.
 */
void FASTCALL CreateUserDirectory( LPUSERLIST lpu )
{
   char szDir[ _MAX_DIR ];

   Amuser_GetUserDirectory( szDir, _MAX_DIR );
   strcat( szDir, "\\" );
   strcat( szDir, lpu->us.szDir );
   if( !Amdir_QueryDirectory( szDir ) )
      Amdir_Create( szDir );
}

/* This function deletes files belonging to the specified
 * user from the main data directory.
 */
void FASTCALL DeleteUserDatabase( LPUSERLIST lpu )
{
   /* Enumerate all databases in the main data directory
    * and for each one whose owner is the user being
    * deleted, delete it.
    */
   Amdb_EnumDatabases( DeleteUserDatabaseEnumProc, (LPARAM)lpu );
}

/* This is the callback function which checks each enumerated
 * database against the one named by lParam.
 */
BOOL CALLBACK EXPORT DeleteUserDatabaseEnumProc( DATABASEINFO FAR * lpdbi, LPARAM lParam )
{
   LPUSERLIST lpu;

   lpu = (LPUSERLIST)lParam;
   if( _strcmpi( lpu->us.szDir, lpdbi->szOwner ) == 0 )
      {
      wsprintf( lpTmpBuf, GS(IDS_STR970), lpdbi->szName );
      if( IDYES == fMessageBox( hwndFrame, 0, lpTmpBuf, MB_YESNO|MB_ICONINFORMATION ) )
         Amdb_PhysicalDeleteDatabase( lpdbi->szFilename );
      }
   return( TRUE );
}

/* This function removes all files from the user's
 * directory, then deletes the user directory.
 */
void FASTCALL DeleteUserDirectory( LPUSERLIST lpu )
{
   char szDir[ _MAX_DIR ];
   char * pDirBase;
   register int n;
   FINDDATA ft;
   HFIND r;

   /* Get the user's directory.
    */
   Amuser_GetUserDirectory( szDir, _MAX_DIR );
   strcat( szDir, "\\" );
   strcat( szDir, lpu->us.szDir );

   /* First delete the user so that AMUSER no longer
    * contains any reference to the directory we're
    * about to vape.
    */
   Amuser_DeleteUser( lpu->us.szDir );

   /* Okay. szDir points to the user directory. Now
    * loop and kill everything in it!
    */
   pDirBase = szDir + strlen( szDir );
   strcpy( pDirBase, "\\*.*" );
   for( n = r = Amuser_FindFirst( szDir, _A_NORMAL, &ft ); n != -1; n = Amuser_FindNext( r, &ft ) )
      {
      strcpy( pDirBase + 1, ft.name );
      Amfile_Delete( szDir );
      }
   Amuser_FindClose( r );

   /* Get the user's directory and delete it.
    */
   *pDirBase = '\0';
   Amdir_Remove( szDir );
}

/* This function deletes an individual user. Currently it only
 * removes the user from the user list. Later we need to extend it
 * to remove associated files.
 */
void FASTCALL DeleteUser( LPUSERLIST lpu )
{
   if( NULL == lpu->lpuPrev )
      lpuFirst = lpu->lpuNext;
   else
      lpu->lpuPrev->lpuNext = lpu->lpuNext;
   if( NULL == lpu->lpuNext )
      lpuLast = lpu->lpuPrev;
   else
      lpu->lpuNext->lpuPrev = lpu->lpuPrev;
   FreeMemory( &lpu );
}

/* This function handles the Add User dialog box.
 */
BOOL EXPORT CALLBACK AddUser( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT lResult;

   CheckDefDlgRecursion( &fDefDlgEx );
   lResult = AddUser_DlgProc( hwnd, message, wParam, lParam );
   return( SetDlgMsgResult( hwnd, message, lResult ) );
}

/* This function handles the dialog box messages passed to the Admin
 *  dialog.
 */
LRESULT FASTCALL AddUser_DlgProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
   switch( message )
      {
      HANDLE_MSG( hwnd, WM_INITDIALOG, AddUser_OnInitDialog );
      HANDLE_MSG( hwnd, WM_DRAWITEM, AddUser_OnDrawItem );
      HANDLE_MSG( hwnd, WM_COMMAND, AddUser_OnCommand );

      case WM_ADMHELP: {
         LPUSERLIST lpuCur;

         /* Different help depending on whether we're editing a user
          * details or adding a new user.
          */
         lpuCur = (LPUSERLIST)GetWindowLong( hwnd, DWL_USER );
         HtmlHelp( hwnd, szHelpFile, HH_HELP_CONTEXT, lpuCur ? idsEDITUSER : idsADDUSER );
         break;
         }

      default:
         return( DefDlgProcEx( hwnd, message, wParam, lParam, &fDefDlgEx ) );
      }
   return( FALSE );
}

/* This function handles the WM_INITDIALOG message.
 */
BOOL FASTCALL AddUser_OnInitDialog( HWND hwnd, HWND hwndFocus, LPARAM lParam )
{
   HWND hwndList;
   DWORD dwPerms;
   register int c;

   /* Set the edit field limits.
    */
   Edit_LimitText( GetDlgItem( hwnd, IDD_USER ), LEN_LOGINNAME - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_DIR ), LEN_DIR - 1 );
   Edit_LimitText( GetDlgItem( hwnd, IDD_PASSWORD ), LEN_LOGINPASSWORD - 1 );

   /* Fill the permissions list box.
    */
   hwndList = GetDlgItem( hwnd, IDD_LIST );
   for( c = 0; c < cMaxPerms; ++c )
      ListBox_InsertString( hwndList, -1, PermTable[ c ].szDescription );

   /* Are we editing a user details?
    */
   if( 0 != lParam )
      {
      LPUSERLIST lpUser;
      char szPassword[ LEN_LOGINPASSWORD ];

      /* Set the specified user name and password.
       */
      lpUser = (LPUSERLIST)lParam;
      Edit_SetText( GetDlgItem( hwnd, IDD_USER ), lpUser->us.szUsername );
      Edit_SetText( GetDlgItem( hwnd, IDD_DIR ), lpUser->us.szDir );
      memcpy( szPassword, lpUser->us.szPassword, LEN_LOGINPASSWORD );
      Amuser_Decrypt( szPassword, rgEncodeKey );
      Edit_SetText( GetDlgItem( hwnd, IDD_PASSWORD ), szPassword );
      memset( szPassword, 0, LEN_LOGINPASSWORD );
      dwPerms = lpUser->us.dwPerms;
      }
   else
      dwPerms = PF_BASIC;

   /* Set the appropriate preferences.
    */
   for( c = 0; c < cMaxPerms; ++c )
      if( dwPerms & PermTable[ c ].dwPerm )
         ListBox_SetSel( hwndList, TRUE, c );

   SetWindowLong( hwnd, DWL_USER, lParam );
   return( TRUE );
}

/* This function handles the WM_COMMAND message.
 */
void FASTCALL AddUser_OnCommand( HWND hwnd, int id, HWND hwndCtl, UINT codeNotify )
{
   switch( id )
      {
      case IDD_USER:
         /* If we've moved focus off the user name, and the directory field is
          * empty AND there's something in the user name field, create an unique
          * directory name.
          */
         if( codeNotify == EN_KILLFOCUS )
            {
            if( Edit_GetTextLength( GetDlgItem( hwnd, IDD_DIR ) ) == 0 )
               if( Edit_GetTextLength( hwndCtl ) > 0 )
                  AdminMakeDirField( hwnd );
            }
         else if( codeNotify == EN_CHANGE )
            {
            if( Edit_GetTextLength( hwndCtl ) > 0 )
               ChangeDefaultButton( hwnd, IDOK );
            else
               ChangeDefaultButton( hwnd, IDCANCEL );
            }
         break;

      case IDOK: {
         char szUsername[ LEN_LOGINNAME ];
         char szPassword[ LEN_LOGINPASSWORD ];
         char szDir[ LEN_DIR ];
         int cItems[ cMaxPerms ];
         LPUSERLIST lpuCur;
         BOOL fMatchSame;
         LPUSERLIST lpu;
         register int c;
         DWORD dwPerms;
         HWND hwndList;
         HWND hwndParent;
         int count;

         /* Make sure the directory field is non-blank.
          */
         AdminMakeDirField( hwnd );

         /* Get and validate the username and password
          * fields. They must not be empty.
          */
         memset( szPassword, 0, LEN_LOGINPASSWORD );
         Edit_GetText( GetDlgItem( hwnd, IDD_USER ), szUsername, LEN_LOGINNAME );
         Edit_GetText( GetDlgItem( hwnd, IDD_DIR ), szDir, LEN_DIR );
         Edit_GetText( GetDlgItem( hwnd, IDD_PASSWORD ), szPassword, LEN_LOGINPASSWORD );
         Amuser_Encrypt( szPassword, rgEncodeKey );
         StripLeadingTrailingSpaces( szUsername );
         if( *szUsername == '\0' )
            {
            HighlightField( hwnd, IDD_USER );
            break;
            }

         /* Get the current user details, if any.
          */
         lpuCur = (LPUSERLIST)GetWindowLong( hwnd, DWL_USER );

         /* Make sure that this user doesn't already appear
          * in the list.
          */
         fMatchSame = FALSE;
         for( lpu = lpuFirst; lpu; lpu = lpu->lpuNext )
            if( _strcmpi( lpu->us.szUsername, szUsername ) == 0 && lpu != lpuCur )
               {
               HighlightField( hwnd, IDD_USER );
               fMatchSame = TRUE;
               break;
               }
         if( fMatchSame )
            break;

         /* Make sure that the directory isn't already used.
          */
         fMatchSame = FALSE;
         for( lpu = lpuFirst; lpu; lpu = lpu->lpuNext )
            if( _strcmpi( lpu->us.szDir, szDir ) == 0 && lpu != lpuCur )
               {
               HighlightField( hwnd, IDD_USER );
               fMatchSame = TRUE;
               break;
               }
         if( fMatchSame )
            break;

         /* Read the list of permissions.
          */
         hwndList = GetDlgItem( hwnd, IDD_LIST );
         dwPerms = 0;
         count = ListBox_GetSelItems( hwndList, cMaxPerms, &cItems );
         for( c = 0; c < count; ++c )
            dwPerms |= PermTable[ cItems[ c ] ].dwPerm;

         /* If this is the first user, the Administrator permission
          * must be assigned!
          */
         if( !(dwPerms & PF_ADMINISTRATE) )
            {
            for( lpu = lpuFirst; lpu; lpu = lpu->lpuNext )
               if( lpu != lpuCur && (lpu->us.dwPerms & PF_ADMINISTRATE) )
                  break;
            if( NULL == lpu )
               {
               fMessageBox( NULL, 0, GS(IDS_STR920), MB_OK|MB_ICONEXCLAMATION );
               break;
               }
            }

         /* If we're editing an existing user's details, just
          * update the record in-situ and exit now.
          */
         if( NULL != lpuCur )
            {
            strcpy( lpuCur->us.szUsername, szUsername );
            strcpy( lpuCur->us.szDir, szDir );
            memcpy( lpuCur->us.szPassword, szPassword, LEN_LOGINPASSWORD );
            lpuCur->us.dwPerms = dwPerms;
            if( strcmp( lpuCur->us.szUsername, lpuActive->us.szUsername ) == 0 )
               /* If we've updated the login user details, update the local
                * information. We'll only be able to change the password and
                * permissions.
                */
               lpuActive->us = lpuCur->us;
            EndDialog( hwnd, TRUE );
            break;
            }

         /* A new user, so link this user into the list.
          */
         if( !fNewMemory( &lpuCur, sizeof(USERLIST) ) )
            {
            OutOfMemoryError( hwnd, FALSE, FALSE );
            break;
            }
         if( NULL == lpuFirst )
            lpuFirst = lpuCur;
         else
            lpuLast->lpuNext = lpuCur;
         lpuCur->lpuPrev = lpuLast;
         lpuCur->lpuNext = NULL;
         lpuLast = lpuCur;
         strcpy( lpuCur->us.szUsername, szUsername );
         strcpy( lpuCur->us.szDir, szDir );
         memcpy( lpuCur->us.szPassword, szPassword, LEN_LOGINPASSWORD );
         lpuCur->us.dwPerms = dwPerms;
         lpuCur->us.fOnline = FALSE;
         lpuCur->us.fDeleted = FALSE;

         /* Add this user to the users list.
          */
         AddUsersInfo( lpuCur );

         /* Create the directory.
          */
         CreateUserDirectory( lpuCur );

         /* Update the list box of the parent window.
          */
         hwndParent = GetParent( hwnd );
         if( !IsDlgButtonChecked( hwndParent, IDD_SHOWPERMISSIONS ) )
            ListBox_AddString( GetDlgItem( hwndParent, IDD_LIST1 ), lpuCur );
         else
            ShowPermissionUsers( hwndParent );

         /* Change the Cancel button to Close.
          */
         Edit_SetText( GetDlgItem( hwnd, IDD_USER ), "" );
         Edit_SetText( GetDlgItem( hwnd, IDD_PASSWORD ), "" );
         Edit_SetText( GetDlgItem( hwnd, IDD_DIR ), "" );
         SetFocus( GetDlgItem( hwnd, IDD_USER ) );
         fCancelToClose( hwnd, IDCANCEL );
         ChangeDefaultButton( hwnd, IDCANCEL );
         break;
         }

      case IDCANCEL:
         EndDialog( hwnd, 0 );
         break;
      }
}

/* This function fills in the directory field if
 * one hasn't been set.
 */
void FASTCALL AdminMakeDirField( HWND hwnd )
{
   /* Only do this if directory field is empty.
    */
   if( 0 == Edit_GetTextLength( GetDlgItem( hwnd, IDD_DIR ) ) )
      {
      LPUSERLIST lpuNext;
      LPUSERLIST lpu;
      char szDir[ LEN_DIR ];
      char * pchLast;
      char chLobber;
      HWND hwndCtl;

      /* First, catch your elephant...
       */
      hwndCtl = GetDlgItem( hwnd, IDD_USER );
      Edit_GetText( hwndCtl, szDir, LEN_DIR-1 );
      szDir[ LEN_DIR-1 ] = '\0';
      _strlwr( szDir );
      if( szDir[ 0] >= 'a' )
         szDir[ 0 ] -= 32;
      pchLast = &szDir[ strlen( szDir ) - 1 ];

      /* Scan the user list to make sure that this
       * directory name is unique.
       */
      chLobber = 'A';
      for( lpu = lpuFirst; lpu; lpu = lpuNext )
         {
         lpuNext = lpu->lpuNext;
         if( _strcmpi( szDir, lpu->us.szDir ) == 0 )
            {
            *pchLast = chLobber++;
            lpuNext = lpuFirst;
            }
         }
      Edit_SetText( GetDlgItem( hwnd, IDD_DIR ), szDir );
      }
}

/* This function handles the WM_DRAWITEM message
 */
void FASTCALL AddUser_OnDrawItem( HWND hwnd, const DRAWITEMSTRUCT FAR * lpDrawItem )
{
   if( lpDrawItem->itemID != -1 )
      {
      char sz[ 100 ];
      HFONT hOldFont;
      HBRUSH hbr;
      int nMode;
      SIZE size;
      int y;
      RECT rc;

      /* Save and fill the rectangle.
       */
      rc = lpDrawItem->rcItem;
      hbr = CreateSolidBrush( GetSysColor( COLOR_WINDOW ) );
      FillRect( lpDrawItem->hDC, &rc, hbr );
      DeleteBrush( hbr );

      /* Set the checkbox mode.
       */
      nMode = CXB_NORMAL;
      if( lpDrawItem->itemState & ODS_SELECTED )
         nMode |= CXB_SELECTED;

      /* Draw the checkbox.
       */
      DrawCheckBox( lpDrawItem->hDC, rc.left + 2, rc.top + 1, nMode, &size );
      rc.left += size.cx + 4;

      /* Get and draw the text.
       */
      hOldFont = SelectFont( lpDrawItem->hDC, hHelvB8Font );
      ListBox_GetText( lpDrawItem->hwndItem, lpDrawItem->itemID, sz );
      y = rc.top + ( ( rc.bottom - rc.top ) - size.cy ) / 2;
      ExtTextOut( lpDrawItem->hDC, rc.left, y, ETO_CLIPPED|ETO_OPAQUE, &rc, sz, strlen( sz ), NULL );

      /* Do we have focus?
       */
      if( lpDrawItem->itemState & ODS_FOCUS )
         DrawFocusRect( lpDrawItem->hDC, &lpDrawItem->rcItem );

      /* Restore things back to normal. */
      SelectFont( lpDrawItem->hDC, hOldFont );
      }
}

HWND CheckFunc(LPSTR userName)
{
HWNDARRAY ha;
int i, j;
char s[255];
char szClsName[ 16 ]; // !!SM!! 2045

  ha.numwnds = 0;

  if (userName && userName[0] != '\x0')  // 2.55.2032
  {
     EnumWindows(FindAmeol2Windows, (LPARAM)(LPHWNDARRAY)&ha);

     for(i=0;i<ha.numwnds;i++)
     {
      GetWindowText(ha.hwnd[i], s, 128);
      j=0;
      while(s[j] != ' ' && s[j] != '\x0')j++;
      s[j] = '\x0';
      GetClassName( ha.hwnd[i], szClsName, sizeof(szClsName) ); // !!SM!! 2045
      if((_stricmp(userName, s) == 0) && (_stricmp(szFrameWndClass, szClsName) == 0)) // !!SM!! 2045
         return( ha.hwnd[i] );
     }
  }
  return( NULL );
}

BOOL CALLBACK FindAmeol2Windows(HWND hWnd, LPARAM lParam)
{ 
LPHWNDARRAY lpha;

  lpha = (LPHWNDARRAY) lParam;

  if (!IsWindowVisible(hWnd))
    return(TRUE);

  if (GetParent(hWnd))
    return(TRUE);

  if (lpha->numwnds < MAX_HWNDARRAY)
  { 
    lpha->hwnd[lpha->numwnds++] = hWnd;
    return(TRUE);
  }
  else
  { 
     return(FALSE);
  }
}
