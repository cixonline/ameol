/* AMDB.C - The Palantir database engine
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

#include "warnings.h"
#include <windows.h>
#include <windowsx.h>
#include <assert.h>
#include "amdbrc.h"
#include "amlib.h"
#include "amuser.h"
#include "amctrls.h"
#include "amdbi.h"
#include <memory.h>
#include <ctype.h>
#include <io.h>
#include <dos.h>
#include <sys\stat.h>
#include <stdlib.h>
#include <string.h>
#include "amevent.h"
#include "common.bld"

#define  THIS_FILE   __FILE__

char szGlobalSig[] = "GLOBAL";            /* Name of global signature */

static BOOL fDefDlgEx = FALSE;

/* Threshold for TXT files is 1Mb short of the 2Gb limit. When the TXT file
 * size reaches this, any further writes result in an AMDBERR_OUTOFDISKSPACE error.
 */
#define SIZE_THRESHOLD   ((2ull * 1024 * 1024 * 1024) - (1024ull * 1024))

HINSTANCE hLibInst;                       /* Handle of library */
HINSTANCE hRscLib;                        /* Handle of resourcelibrary */
HINSTANCE hRegLib;                        /* Regular Expression and Mail Library */
MSGCOUNT cTotalMsgc;                      /* Total message counts */
PTL ptlCachedThd = NULL;                  /* Cached PTL of THD file for fast CreateMsg */
HNDFILE fhCachedThd = HNDFILE_ERROR;      /* Cached THD file handle */
PTL ptlCachedTxt = NULL;                  /* Cached PTL of TXT file for fast GetMsgText */
HNDFILE fhCachedTxt = HNDFILE_ERROR;      /* Cached TXT file handle */
BOOL fHasShare = FALSE;                   /* Set to TRUE if SHARE.EXE loaded */
VIEWDATA voDefault;                       /* Default viewdata options */
PURGEOPTIONS poDefault;                   /* Default purge options */
char szDataDir[ _MAX_DIR ];               /* Path of database directory */
char szUserDir[ _MAX_DIR ];               /* Path of user's directory */
BOOL fInitDataDirectory;                  /* TRUE if paths initialised */
WORD wBufferUpperLimit;                   /* Paging buffer upper limit */
INTERFACE_PROPERTIES ip;                  /* Interface properties */

static DWORD pow2[] = {
   0x00000001, 0x00000002, 0x00000004, 0x00000008,
   0x00000010, 0x00000020, 0x00000040, 0x00000080,
   0x00000100, 0x00000200, 0x00000400, 0x00000800,
   0x00001000, 0x00002000, 0x00004000, 0x00008000,
   0x00010000, 0x00020000, 0x00040000, 0x00080000,
   0x00100000, 0x00200000, 0x00400000, 0x00800000,
   0x01000000, 0x02000000, 0x04000000, 0x08000000,
   0x10000000, 0x20000000, 0x40000000, 0x80000000
   };

static char str1[] = "Repaired database for %s/%s";
static char str2[] = "File open failure: %s";
static char str3[] = "File create failure: %s";
static char str4[] = "Database write failure: %s";
static char str5[] = "Database unlock failure: %s";
static char str6[] = "Database lock failure: %s";
static char str7[] = "Database seek failure: %s";
static char str8[] = "%s/%s: Total time to page in topic = %lums";
const char NEAR szSettings[] = "Settings";            /* Section for settings */

void FASTCALL SetSignature( LPSTR, LPSTR );
void FASTCALL BuildThreadLines( PTL );
BOOL FASTCALL WriteMsgText( HNDFILE, PTH, BOOL );
BOOL FASTCALL WriteMsgTextLine( HNDFILE, LPSTR, BOOL );
BOOL FASTCALL WriteHugeMsgText( HNDFILE, HPSTR, DWORD );
HNDFILE FASTCALL LockMessage( PTH );
void FASTCALL UnlockMessage( HNDFILE, PTH );
BOOL FASTCALL TryLock( HNDFILE, DWORD, int );
BOOL FASTCALL TryUnlock( HNDFILE, DWORD, int );
void FASTCALL InternalDeleteMsg( PTH, BOOL, BOOL );
BOOL FASTCALL DiskSaveError( HWND, HNDFILE );
BOOL FASTCALL BackupPurgedMessage( PTH );
BOOL FASTCALL LoadAttachments( PTL );
int FASTCALL WriteMsgTextBuffer( PTH, HPSTR, DWORD );
void FASTCALL SendNotification( int, WPARAM, LPARAM );
BOOL FASTCALL MarkOneMsgRead( PTH );
BOOL FASTCALL ValidFileNameChr( char );
int FASTCALL PurgeAllDeleted( PTL, PURGEOPTIONS *, PURGEPROC, PURGECALLBACKINFO * );
int FASTCALL PurgeAllIgnored( PTL, PURGEOPTIONS *, PURGEPROC, PURGECALLBACKINFO * );
int FASTCALL Amdb_FastLoadChronological( HWND, PTL );
int FASTCALL Amdb_FastLoadReference( HWND, PTL, BOOL );
int FASTCALL Amdb_FastLoadRoots( HWND, PTL, BOOL, BOOL );

PCAT pcatFirst = NULL;                             /* First database */
PCAT pcatLast = NULL;                              /* Next database */

/* This is the database DLL entry point. Two separate functions are
 * provided: one for Windows 32 and one for 16-bit Windows.
 */
BOOL WINAPI EXPORT DllMain( HANDLE hInstance, DWORD fdwReason, LPVOID lpvReserved )
{
   switch( fdwReason )
      {
      case DLL_PROCESS_ATTACH:
         hLibInst = hInstance;
         hRscLib = hInstance;
         hRegLib = NULL;
         fInitDataDirectory = FALSE;
         memset( &cTotalMsgc, 0, sizeof(MSGCOUNT) );
         Amdb_SetDefaultPurgeOptions( NULL );
         Amdb_SetDefaultViewData( NULL );
         return( 1 );

      case DLL_PROCESS_DETACH:
         return( 1 );

      default:
         return( 1 );
      }
}

/* This function returns the version number of Amctrls.
 */
DWORD WINAPI EXPORT GetBuildVersion( void )
{
   return( MAKEVERSION( PRODUCT_MAX_VER, PRODUCT_MIN_VER, PRODUCT_BUILD ) );
}

void ShowTHDFileError(int pNo, char * pFile, char * pTopic)
{
   char chTmpBuf[ 254 ];

   wsprintf( chTmpBuf, "%d:Error Opening %s.THD File for Topic %s", pNo, pFile, pTopic);
   Amevent_AddEventLogItem( ETYP_SEVERE|ETYP_DATABASE, chTmpBuf );
   MessageBox( GetFocus(), chTmpBuf, NULL, MB_OK|MB_ICONEXCLAMATION );
}

/* This function changes the default purge options.
 */
void WINAPI EXPORT Amdb_SetDefaultPurgeOptions( PURGEOPTIONS FAR * lppo )
{
   PURGEOPTIONS po;

   if( NULL == lppo )
      {
      po.wFlags = PO_DELETEONLY;
      po.wMaxSize = 100;
      po.wMaxDate = 30;
      lppo = &po;
      }
   poDefault = *lppo;
}

/* This function changes the default view data.
 */
void WINAPI EXPORT Amdb_SetDefaultViewData( VIEWDATA FAR * lpvo )
{
   VIEWDATA vo;

   if( NULL == lpvo )
      {
      vo.nSortMode = VSM_REFERENCE|VSM_ASCENDING;
      vo.nViewMode = VM_VIEWREF;
      vo.wHeaderFlags = VH_MSGNUM|VH_SENDER|VH_SUBJECT;
      vo.nThreadIndentPixels = 6;
      vo.nHdrDivisions[ 0 ] = 120;
      vo.nHdrDivisions[ 1 ] = 200;
      vo.nHdrDivisions[ 2 ] = 136;
      vo.nHdrDivisions[ 3 ] = 46;
      vo.nHdrDivisions[ 4 ] = 600;
      lpvo = &vo;
      }
   voDefault = *lpvo;
}

/* This function creates a new, empty, folder and adds it to the
 * folder list.
 */
PCL WINAPI EXPORT Amdb_CreateFolder( PCAT pcat, LPSTR lpszFolderName, int position )
{
   static CLITEM clItem;
   PCL pclBefore;
   static PCL pcl;
   BOOL fKeepAtTop;

   /* Check that caller is permitted to create a folder.
    */
   ASSERT( NULL != pcat );

   /* Initialise the folder record.
    */
   memset( &clItem, 0, sizeof( CLITEM ) );
   lstrcpy( clItem.szFolderName, lpszFolderName );
   clItem.cTopics = 0;

   /* Test for the 'keep at top' flag.
    */
   fKeepAtTop = ( position & 0x8000 ) != 0;

   /* Scan the database to locate the folder after which we insert
    * this one.
    */
   switch( position & 0x7FFF )
      {
      default:
         return( NULL );

      case CFF_FIRST:
         pclBefore = NULL;
         break;

      case CFF_SORT:
         pclBefore = NULL;
         for( pcl = pcat->pclFirst; pcl; pcl = pcl->pclNext )
            {
            if( fKeepAtTop )
               {
               if( !( pcl->clItem.dwFlags & CF_KEEPATTOP ) )
                  break;
               if( ( _stricmp( pcl->clItem.szFolderName, lpszFolderName ) > 0 ) && ( _stricmp( pcl->clItem.szFolderName, "Mail" ) != 0 ) )
                  break;
               }
            else
               {
               if( !( pcl->clItem.dwFlags & CF_KEEPATTOP ) )
                  if( _stricmp( pcl->clItem.szFolderName, lpszFolderName ) > 0 )
                     break;
               }
            pclBefore = pcl;
            }
         break;

      case CFF_LAST:
         pclBefore = pcat->pclLast;
         break;
      }

   /* Create the new folder.
    */
   pcl = pclBefore;
   if( fNewFolder( pcat, &pcl, NULL, &clItem ) )
      {
      pcl->clItem.dwFlags = CF_EXPANDED;
      pcl->clItem.po = poDefault;
      pcl->clItem.dateCreated = Amdate_GetPackedCurrentDate();
      pcl->clItem.timeCreated = Amdate_GetPackedCurrentTime();
      strcpy( pcl->clItem.szSigFile, szGlobalSig );
      CreateUniqueFileMulti( lpszFolderName, "CNF", "TXT", TRUE, pcl->clItem.szFileName );
      if( fKeepAtTop )
         pcl->clItem.dwFlags |= CF_KEEPATTOP;
      fDataChanged = TRUE;
      }
   return( pcl );
}

/* This function allocates and initialises the data structures for a new
 * folder.
 */
BOOL FASTCALL fNewFolder( PCAT pcat, PCL * ppcl, char * pszRealName, CLITEM * pclItem )
{
   PCL pclBefore;
   PCL pcl;

   INITIALISE_PTR(pcl);
   if( fNewSharedMemory( &pcl, sizeof( CNL ) ) )
      {
      pclBefore = *ppcl;
      pcl->wID = STYP_CL;
      if( NULL == pclBefore )
         {
         if( NULL == pcat->pclFirst )
            pcat->pclLast = pcl;
         else
            pcat->pclFirst->pclPrev = pcl;
         pcl->pclNext = pcat->pclFirst;
         pcl->pclPrev = NULL;
         pcat->pclFirst = pcl;
         }
      else
         {
         if( pclBefore->pclNext )
            pclBefore->pclNext->pclPrev = pcl;
         else
            pcat->pclLast = pcl;
         pcl->pclNext = pclBefore->pclNext;
         pclBefore->pclNext = pcl;
         pcl->pclPrev = pclBefore;
         }
      pcl->pcat = pcat;
      pcl->ptlFirst = NULL;
      pcl->ptlLast = NULL;
      memset( &pcl->msgc, 0, sizeof(MSGCOUNT) );
      pcl->clItem = *pclItem;
      pcl->lpModList = NULL;
      ++pcat->catItem.cFolders;
      *ppcl = pcl;
      Amuser_CallRegistered( AE_NEWFOLDER, (LPARAM)pcl, 0 );
      }
   return( pcl != NULL );
}

/* This function locates the specified folder.
 */
PCL WINAPI EXPORT Amdb_OpenFolder( PCAT pcat, LPSTR lpszFolderName )
{
   PCAT pcatStart;
   PCAT pcatEnd;
   PCL pcl;

   pcatStart = pcat ? pcat : pcatFirst;
   pcatEnd = pcat ? pcat->pcatNext : NULL;
   for( pcat = pcatStart; pcat != pcatEnd; pcat = pcat->pcatNext )
      for( pcl = pcat->pclFirst; pcl; pcl = pcl->pclNext )
         {
         if( lstrcmp( pcl->clItem.szFolderName, lpszFolderName ) == 0 )
            return( pcl );
         }
   return( NULL );
}

/* This function sets the list of moderators for the
 * specified conference.
 */
LPSTR WINAPI EXPORT Amdb_SetModeratorList( PCL pcl, LPSTR lp )
{
   char szFileName[ 13 ];
   register int c;
   HNDFILE fh;

   wsprintf( szFileName, "%s.CNF", pcl->clItem.szFileName );
   if( ( fh = Amdb_CreateFile( szFileName, 0 ) ) != HNDFILE_ERROR )
      {
      LPSTR lp2;

      INITIALISE_PTR(lp2);
      if( fNewSharedMemory( &lp2, lstrlen( lp ) + 1 ) )
         {
         for( c = 0; *lp; )
            {
            while( *lp && ( *lp == '.' || *lp == ' ' || *lp == ';' || *lp == ',' ) )
               lp++;
            while( *lp && *lp != '.' && *lp != ' ' && *lp != ';' && *lp != ',' )
               lp2[ c++ ] = *lp++;
            while( *lp && ( *lp == '.' || *lp == ' ' || *lp == ';' || *lp == ',' ) )
               lp++;
            if( *lp )
               lp2[ c++ ] = ' ';
            }
         lp2[ c ] = '\0';
         Amfile_Write( fh, lp2, lstrlen( lp2 ) + 1 );
         if( pcl->lpModList )
            FreeMemory( &pcl->lpModList );
         pcl->lpModList = lp2;
         }
      Amfile_Close( fh );
      return( lp );
      }
   return( NULL );
}

/* This function retrieves the list of moderators for the
 * specified conference.
 */
UINT WINAPI EXPORT Amdb_GetModeratorList( PCL pcl, LPSTR lp, UINT cbMax )
{
   UINT wSize;

   ASSERT( cbMax > 0 );
   wSize = 0;
   if( pcl->lpModList )
      {
      wSize = min( cbMax, (UINT)lstrlen( pcl->lpModList ) + 1 );
      if( lp && wSize > 0 )
         {
         lstrcpyn( lp, pcl->lpModList, wSize );
         lp[ wSize - 1 ] = '\0';
         }
      }
   else
      {
      char szFileName[ 13 ];
      DWORD dwSize;
      HNDFILE fh;

      wsprintf( szFileName, "%s.CNF", pcl->clItem.szFileName );
      if( ( fh = Amdb_OpenFile( szFileName, AOF_READ ) ) != HNDFILE_ERROR )
         {
         if( ( dwSize = Amfile_GetFileSize( fh ) ) > 0 )
            {
            wSize = (UINT)min( cbMax, dwSize );
            if( lp )
               {
               Amfile_Read( fh, lp, wSize );
               lp[ wSize - 1 ] = '\0';
               }
            }
         Amfile_Close( fh );
         }
      }
   return( wSize );
}

/* This function checks whether the specified user is a moderator
 * of the given conference.
 */
BOOL EXPORT WINAPI Amdb_IsModerator( PCL pcl, LPSTR lpszUserName )
{
   LPSTR lp;
   LPSTR lpModList;
   BOOL fFound;

   /* If the list of moderators for this conference has not been
    * loaded, load it now.
    */
   ASSERT( NULL != lpszUserName );
   lpModList = NULL;
   fFound = FALSE;
   if( ( lp = pcl->lpModList ) == NULL )
      {
      char szFileName[ 13 ];
      DWORD dwSize;
      HNDFILE fh;

      wsprintf( szFileName, "%s.CNF", pcl->clItem.szFileName );
      if( ( fh = Amdb_OpenFile( szFileName, AOF_READ ) ) != HNDFILE_ERROR )
         {
         dwSize = Amfile_GetFileSize( fh );
         if( dwSize > 0 && fNewSharedMemory( &lpModList, LOWORD( dwSize ) ) )
            {
            Amfile_Read( fh, lpModList, LOWORD( dwSize ) );
            lp = lpModList;
            }
         else
            lp = NULL;
         Amfile_Close( fh );
         pcl->lpModList = lp;
         }
      }

   /* Scan the list, looking for the moderators.
    */
   if( lp )
      while( *lp != '\0' )
         {
         register int c;
         register int n;

         while( *lp == ' ' || *lp == ';' || *lp == ',' )
            ++lp;
         for( c = 0; lp[ c ] && lp[ c ] != '.' && lp[ c ] != ' ' && lp[ c ] != ';' && lp[ c ] != ','; ++c );
         n = max( lstrlen( lpszUserName ), c );
         if( fFound = ( _fstrnicmp( lpszUserName, lp, n ) == 0 ) )
            break;
         lp += c;
         while( *lp == ' ' || *lp == ';' || *lp == ',' )
            ++lp;
         }
   return( fFound );
}

/* Returns the flags for the specified folder.
 */
DWORD WINAPI EXPORT Amdb_GetFolderFlags( PCL pcl, DWORD dwMask )
{
   return( pcl->clItem.dwFlags & dwMask );
}

/* Sets or unsets the flags for the specified folder.
 */
void WINAPI EXPORT Amdb_SetFolderFlags( PCL pcl, DWORD dwMask, BOOL fSet )
{
   if( fSet )
      pcl->clItem.dwFlags |= dwMask;
   else
      pcl->clItem.dwFlags &= ~dwMask;
}

/* Sets the creation date and time of the specified folder.
 */
void WINAPI EXPORT Amdb_SetFolderDateTime( PCL pcl, WORD wDate, WORD wTime )
{
   pcl->clItem.dateCreated = wDate;
   pcl->clItem.timeCreated = wTime;
   fDataChanged = TRUE;
}

/* This function returns a pointer to the first category.
 */
PCAT WINAPI EXPORT Amdb_GetFirstCategory( void )
{
   return( pcatFirst );
}

/* This function returns a pointer to the last category.
 */
PCAT WINAPI EXPORT Amdb_GetLastCategory( void )
{
   return( pcatLast );
}

/* This function returns a pointer to the next category after pcat.
 */
PCAT WINAPI EXPORT Amdb_GetNextCategory( PCAT pcat )
{
   ASSERT( NULL != pcat );
   return( pcat->pcatNext );
}

/* This function returns a pointer to the previous database
 */
PCAT WINAPI EXPORT Amdb_GetPreviousCategory( PCAT pcat )
{
   ASSERT( NULL != pcat );
   return( pcat->pcatPrev );
}

/* This function returns a pointer to the ASCIIZ string of the category
 * name. The name must not be altered.
 */
LPCSTR WINAPI EXPORT Amdb_GetCategoryName( PCAT pcat )
{
   ASSERT( NULL != pcat );
   return( (LPCSTR)pcat->catItem.szCategoryName );
}

/* This function retrieves the specified database flags.
 */
DWORD WINAPI EXPORT Amdb_GetCategoryFlags( PCAT pcat, DWORD dwMask )
{
   ASSERT( NULL != pcat );
   return( pcat->catItem.dwFlags & dwMask );
}

/* This function changes the specified category flags.
 */
void WINAPI EXPORT Amdb_SetCategoryFlags( PCAT pcat, DWORD dwMask, BOOL fSet )
{
   ASSERT( NULL != pcat );
   if( fSet )
      pcat->catItem.dwFlags |= dwMask;
   else
      pcat->catItem.dwFlags &= ~dwMask;
   fDataChanged = TRUE;
}

/* This function creates a new, empty, database and adds it to the
 * database list.
 */
PCAT WINAPI EXPORT Amdb_CreateCategory( LPSTR lpszName )
{
   static CATITEM catItem;
   PCAT pcat;

   /* Initialise the category record.
    */
   memset( &catItem, 0, sizeof( CATITEM ) );
   lstrcpy( catItem.szCategoryName, lpszName );
   catItem.cFolders = 0;

   /* Create the category.
    */
   if( fNewCategory( &pcat, &catItem ) )
      {
      pcat->catItem.dwFlags = DF_EXPANDED;
      pcat->catItem.dateCreated = Amdate_GetPackedCurrentDate();
      pcat->catItem.timeCreated = Amdate_GetPackedCurrentTime();
      fDataChanged = TRUE;
      }
   return( pcat );
}

/* This function allocates and initialises the data structures for a new
 * category.
 */
BOOL FASTCALL fNewCategory( PCAT * ppcat, CATITEM * pcatItem )
{
   PCAT pcat;

   INITIALISE_PTR(pcat);
   if( fNewSharedMemory( &pcat, sizeof( CAT ) ) )
      {
      pcat->wID = STYP_CAT;
      pcat->pcatNext = NULL;
      pcat->pcatPrev = pcatLast;
      if( NULL != pcatLast )
         pcatLast->pcatNext = pcat;
      else
         pcatLast = pcat;
      if( NULL == pcatFirst )
         pcatFirst = pcat;
      pcatLast = pcat;
      pcat->pclFirst = NULL;
      pcat->pclLast = NULL;
      memset( &pcat->msgc, 0, sizeof(MSGCOUNT) );
      pcat->catItem = *pcatItem;
      *ppcat = pcat;
      ++dbHdr.cCategories;
      Amuser_CallRegistered( AE_NEWCATEGORY, (LPARAM)pcat, 0 );
      }
   return( pcat != NULL );
}

/* This function locates the specified category.
 */
PCAT WINAPI EXPORT Amdb_OpenCategory( LPSTR lpszName )
{
   PCAT pcat;

   ASSERT( NULL != lpszName );
   for( pcat = pcatFirst; pcat; pcat = pcat->pcatNext )
      if( lstrcmp( pcat->catItem.szCategoryName, lpszName ) == 0 )
         break;
   return( pcat );
}

/* This function deletes a category.
 */
int WINAPI EXPORT Amdb_DeleteCategory( PCAT pcat )
{
   PCL pcl;

   /* First check that caller is permitted to
    * delete database.
    */
   ASSERT( NULL != pcat );

   /* Safe to delete.
    */
   if( pcat->pcatNext )
      pcat->pcatNext->pcatPrev = pcat->pcatPrev;
   else
      pcatLast = pcat->pcatPrev;
   if( pcat->pcatPrev )
      pcat->pcatPrev->pcatNext = pcat->pcatNext;
   else
      pcatFirst = pcat->pcatNext;
   pcl = pcat->pclFirst;
   while( pcl )
      {
      PCL pclNext;

      pclNext = pcl->pclNext;
      fDeleteFolder( pcl );
      pcl = pclNext;
      }

   /* Delete the database disk file.
    */
   Amuser_CallRegistered( AE_DELETECATEGORY, (LPARAM)pcat, 0 );
   FreeMemory( &pcat );
   --dbHdr.cCategories;
   fDataChanged = TRUE;
   Amdb_CommitDatabase( FALSE );
   Amuser_CallRegistered( AE_UNREADCHANGED, 0, 0 );
   return( AMDBERR_NOERROR );
}

/* This function sets the category name.
 */
int WINAPI EXPORT Amdb_SetCategoryName( PCAT pcat, LPCSTR lpcstrName )
{
   ASSERT( NULL != pcat );
   ASSERT( NULL != lpcstrName );

   /* Raise event.
    */
   if( Amuser_CallRegistered( AE_CATEGORY_CHANGING, (LPARAM)pcat, (LPARAM)AECHG_NAME ) )
   {
      lstrcpyn( pcat->catItem.szCategoryName, lpcstrName, sizeof(pcat->catItem.szCategoryName) );
      fDataChanged = TRUE;
      Amuser_CallRegistered( AE_CATEGORY_CHANGED, (LPARAM)pcat, (LPARAM)AECHG_NAME );
      return( AMDBERR_NOERROR );
   }
   return( AMDBERR_NOPERMISSION );
}

/* This function fills a CATEGORYINFO structure with
 * information about the specified database.
 */
void WINAPI EXPORT Amdb_GetCategoryInfo( PCAT pcat, CATEGORYINFO FAR * pCategoryInfo )
{
   ASSERT( NULL != pcat );
   ASSERT( NULL != pCategoryInfo );
   lstrcpy( pCategoryInfo->szCategoryName, pcat->catItem.szCategoryName );
   pCategoryInfo->wCreationTime = pcat->catItem.timeCreated;
   pCategoryInfo->wCreationDate = pcat->catItem.dateCreated;
   pCategoryInfo->cFolders = pcat->catItem.cFolders;
   pCategoryInfo->cMsgs = pcat->catItem.cMsgs;
   pCategoryInfo->cUnRead = pcat->msgc.cUnRead;
   pCategoryInfo->dwFlags = pcat->catItem.dwFlags;
}

/* Returns the total number of unread messages.
 */
DWORD WINAPI EXPORT Amdb_GetTotalUnread( void )
{
   return( cTotalMsgc.cUnRead );
}

/* Returns the total number of tagged messages.
 */
DWORD WINAPI EXPORT Amdb_GetTotalTagged( void )
{
   return( cTotalMsgc.cTagged );
}

/* Returns the total number of unread priority messages.
 */
DWORD WINAPI EXPORT Amdb_GetTotalUnreadPriority( void )
{
   return( cTotalMsgc.cUnReadPriority );
}

/* This function deletes a folder, all topics within the folder
 * and all threads within all topics. Then it deletes the topic files and
 * associated thread files from disk.
 */
BOOL WINAPI EXPORT Amdb_DeleteFolder( PCL pcl )
{
   /* First check to see if we're deleting the last folder in the database
    * and delete the database if so. Saves mucho time.
    */
   fDataChanged = TRUE;
   fDeleteFolder( pcl );
   Amdb_CommitDatabase( FALSE );
   Amuser_CallRegistered( AE_UNREADCHANGED, 0, 0 );
   return( AMDBERR_NOERROR );
}

/* This function deletes the specified folder.
 */
BOOL FASTCALL fDeleteFolder( PCL pcl )
{
   char szFileName[ 26 ];
   PTL ptl;
   HCURSOR hOldCursor;

   /* Broadcast the happy event.
    */
   Amuser_CallRegistered( AE_DELETEFOLDER, (LPARAM)pcl, 0 );

   hOldCursor = SetCursor( LoadCursor( NULL, IDC_WAIT ) );

   /* Unhook this folder from the database.
    */
   if( pcl->pclNext )
      pcl->pclNext->pclPrev = pcl->pclPrev;
   else
      pcl->pcat->pclLast = pcl->pclPrev;
   if( pcl->pclPrev )
      pcl->pclPrev->pclNext = pcl->pclNext;
   else
      pcl->pcat->pclFirst = pcl->pclNext;

   /* Delete all topics. This will also clean up
    * redundant files for the topics.
    */
   ptl = pcl->ptlFirst;
   while( ptl )
      {
      PTL ptlNext;

      ptlNext = ptl->ptlNext;
      fDeleteTopic( ptl );
      ptl = ptlNext;
      }

   /* Delete the list of moderators.
    */
   wsprintf( szFileName, "%s.CNF", (LPSTR)pcl->clItem.szFileName );
   Amdb_DeleteFile( szFileName );

   /* Delete the conference note file
    */
   wsprintf( szFileName, "%s.CNO", (LPSTR)pcl->clItem.szFileName );
   Amdb_DeleteFile( szFileName );

   wsprintf( szFileName, "%s.CXN", (LPSTR)pcl->clItem.szFileName );
   Amdb_DeleteFile( szFileName );

   /* Delete the list of participants.
    * CAUTION: Hardcodes the participants list directory!
    */
   wsprintf( szFileName, "PARLIST\\%s.PAR", (LPSTR)pcl->clItem.szFileName );
   Amdb_DeleteFile( szFileName );

   /* One fewer folder in database.
    */
   --pcl->pcat->catItem.cFolders;
   FreeMemory( &pcl );
   SetCursor( hOldCursor );

   return( TRUE );
}

/* This function deletes a topic and all threads within all topics. Then it
 * deletes the topic files and associated thread files from disk.
 */
int WINAPI EXPORT Amdb_DeleteTopic( PTL ptl )
{
   /* First check to see if we're deleting the last topic in the folder,
    * and delete the folder if so. Saves mucho time.
    */
   fDataChanged = TRUE;
   fDeleteTopic( ptl );
   Amdb_CommitDatabase( FALSE );
   Amuser_CallRegistered( AE_UNREADCHANGED, 0, 0 );
   return( AMDBERR_NOERROR );
}

/* This function deletes the specified topic from memory
 * and disk. It is very destructive, so wear rubber gloves.
 */
BOOL FASTCALL fDeleteTopic( PTL ptl )
{
   char szFileName[ 26 ];
   PTH pth;
   PAH pah;
   HCURSOR hOldCursor;

   /* Broadcast the happy event.
    */
   Amuser_CallRegistered( AE_DELETETOPIC, (LPARAM)ptl, 0 );

   hOldCursor = SetCursor( LoadCursor( NULL, IDC_WAIT ) );

   /* Delete all messages in this topic.
    */
   pth = ptl->pthFirst;
   while( pth )
      {
      PTH pthNext = pth->pthNext;

      FreeMemory( &pth );
      pth = pthNext;
      }

   /* Delete all loaded attachments
    */
   pah = ptl->pahFirst;
   while( pah )
      {
      PAH pahNext = pah->pahNext;

      FreeMemory( &pah );
      pah = pahNext;
      }

   /* Now unlink this topic from the linked list
    * of topics for the conference.
    */
   if( ptl->ptlNext )
      ptl->ptlNext->ptlPrev = ptl->ptlPrev;
   else
      ptl->pcl->ptlLast = ptl->ptlPrev;
   if( ptl->ptlPrev )
      ptl->ptlPrev->ptlNext = ptl->ptlNext;
   else
      ptl->pcl->ptlFirst = ptl->ptlNext;
   --ptl->pcl->clItem.cTopics;

   /* If this topic has been cached, clear the
    * caches.
    */
   if( ptl == ptlCachedThd )
      {
      Amfile_Close( fhCachedThd );
      fhCachedThd = HNDFILE_ERROR;
      ptlCachedThd = NULL;
      }
   if( ptl == ptlCachedTxt )
      {
      Amfile_Close( fhCachedTxt );
      fhCachedTxt = HNDFILE_ERROR;
      ptlCachedTxt = NULL;
      }

   /* Delete the thread and text files.
    */
   wsprintf( szFileName, "%s.THD", (LPSTR)ptl->tlItem.szFileName );
   Amdb_DeleteFile( szFileName );
   wsprintf( szFileName, "%s.TXT", (LPSTR)ptl->tlItem.szFileName );
   Amdb_DeleteFile( szFileName );

   /* Delete the attachments file.
    */
   wsprintf( szFileName, "%s.ATC", (LPSTR)ptl->tlItem.szFileName );
   Amdb_DeleteFile( szFileName );

   /* Delete any cookies.
    */
   wsprintf( szFileName, "%s.CKE", (LPSTR)ptl->tlItem.szFileName );
   Amdb_DeleteFile( szFileName );
   Amdb_MoveCookies( ptl, NULL );

   /* Delete the file lists.
    * CAUTION: Hardcodes the file list directory!
    */
   wsprintf( szFileName, "FLIST\\%s.FLS", (LPSTR)ptl->tlItem.szFileName );
   Amdb_DeleteFile( szFileName );
   wsprintf( szFileName, "FLIST\\%s.FLD", (LPSTR)ptl->tlItem.szFileName );
   Amdb_DeleteFile( szFileName );
   wsprintf( szFileName, "FLIST\\%s.FPR", (LPSTR)ptl->tlItem.szFileName );
   Amdb_DeleteFile( szFileName );

   /* Adjust message counts.
    */
   ptl->pcl->clItem.cMsgs -= ptl->tlItem.cMsgs;
   ptl->pcl->pcat->catItem.cMsgs -= ptl->tlItem.cMsgs;

   /* Adjust message counts.
    */
   ReduceMessageCounts( &cTotalMsgc, &ptl->tlItem.msgc );
   ReduceMessageCounts( &ptl->pcl->msgc, &ptl->tlItem.msgc );
   ReduceMessageCounts( &ptl->pcl->pcat->msgc, &ptl->tlItem.msgc );
   FreeMemory( &ptl );
   SetCursor( hOldCursor );
   return( TRUE );
}

BOOL WINAPI EXPORT Amdb_IsCategoryPtr( PCAT pcat )
{
   ASSERT( NULL != pcat );
   return( pcat->wID == STYP_CAT );
}

BOOL WINAPI EXPORT Amdb_IsFolderPtr( PCL pcl )
{
   ASSERT( NULL != pcl );
   return( pcl->wID == STYP_CL );
}

BOOL WINAPI EXPORT Amdb_IsTopicPtr( PTL ptl )
{
   ASSERT( NULL != ptl );
   return( ptl->wID == STYP_TL );
}

BOOL WINAPI EXPORT Amdb_IsMessagePtr( PTH pth )
{
   ASSERT( NULL != pth );
   ASSERT( pth->ptl->wLockCount > 0 );
   return( pth->wID == STYP_TH );
}

/* This function sets the topic type.
 */
void WINAPI EXPORT Amdb_SetTopicType( PTL ptl, WORD wType )
{
   ASSERT( NULL != ptl );
   ptl->tlItem.wTopicType = wType;
   fDataChanged = TRUE;
}

/* This function retrieves the topic type.
 */
WORD WINAPI EXPORT Amdb_GetTopicType( PTL ptl )
{
   ASSERT( NULL != ptl );
   return( ptl->tlItem.wTopicType );
}

/* Marks a topic as in-use. Topics which are in-use cannot be
 * deleted.
 */
void WINAPI EXPORT Amdb_MarkTopicInUse( PTL ptl, BOOL fInUse )
{
   ASSERT( NULL != ptl );
   if( fInUse )
      ptl->tlItem.dwFlags |= TF_TOPICINUSE;
   else
      ptl->tlItem.dwFlags &= ~TF_TOPICINUSE;
}

/* This function returns a pointer to the first folder.
 */
PCL WINAPI EXPORT Amdb_GetFirstFolder( PCAT pcat )
{
   ASSERT( NULL != pcat );
   return( pcat->pclFirst );
}

/* This function returns a pointer to the last folder.
 */
PCL WINAPI EXPORT Amdb_GetLastFolder( PCAT pcat )
{
   ASSERT( NULL != pcat );
   return( pcat->pclLast );
}

/* This function returns a pointer to the first folder.
 */
PCL WINAPI EXPORT Amdb_GetFirstRealFolder( PCAT pcat )
{
   ASSERT( NULL != pcat );
   while ( NULL == pcat->pclFirst )
   {
      PCAT pcat2 = Amdb_GetNextCategory( pcat );
      if (pcat2 == NULL)
         return NULL;
      pcat = pcat2;
   }
   return( pcat->pclFirst );
}

/* This function returns a pointer to the last folder.
 */
PCL WINAPI EXPORT Amdb_GetLastRealFolder( PCAT pcat )
{
   ASSERT( NULL != pcat );
   while( NULL == pcat->pclLast )
      pcat = Amdb_GetPreviousCategory( pcat );
   return( pcat->pclLast );
}

/* This function returns a pointer to the next folder after pcl.
 */
PCL WINAPI EXPORT Amdb_GetNextFolder( PCL pcl )
{
   ASSERT( NULL != pcl );
   return( pcl->pclNext );
}

/* Returns a pointer to the folder preceding pcl.
 */
PCL WINAPI EXPORT Amdb_GetPreviousFolder( PCL pcl )
{
   ASSERT( NULL != pcl );
   return( pcl->pclPrev );
}

/* This function returns a pointer to the next folder after pcl.
 */
PCL WINAPI EXPORT Amdb_GetNextUnconstrainedFolder( PCL pcl )
{
   ASSERT( NULL != pcl );
   if( NULL == pcl->pclNext )
      {
      PCAT pcat;

      pcat = pcl->pcat->pcatNext;
      while( pcat && NULL == pcat->pclFirst )
         pcat = pcat->pcatNext;
      if( pcat )
         return( pcat->pclFirst );
      }
   return( pcl->pclNext );
}

/* Returns a pointer to the folder preceding pcl.
 */
PCL WINAPI EXPORT Amdb_GetPreviousUnconstrainedFolder( PCL pcl )
{
   ASSERT( NULL != pcl );
   if( NULL == pcl->pclNext )
      {
      PCAT pcat;

      pcat = pcl->pcat->pcatPrev;
      while( pcat && NULL == pcat->pclLast )
         pcat = pcat->pcatPrev;
      if( pcat )
         return( pcat->pclLast );
      }
   return( pcl->pclPrev );
}

/* This function returns a pointer to the ASCIIZ string of the folder
 * name. The name must not be altered.
 */
LPCSTR WINAPI EXPORT Amdb_GetFolderName( PCL pcl )
{
   return( (LPCSTR)pcl->clItem.szFolderName );
}

/* This function sets the folder name.
 */
int WINAPI EXPORT Amdb_SetFolderName( PCL pcl, LPCSTR lpcstrName )
{
   /* Check that caller is permitted to change folder name.
    */
   ASSERT( NULL != pcl );
   ASSERT( NULL != lpcstrName );

   /* Broadcast event.
    */
   if( Amuser_CallRegistered( AE_FOLDER_CHANGING, (LPARAM)pcl, (LPARAM)AECHG_NAME ) )
      {
      /* If this folder had name collision, then take the
       * easy way out.
       */
      lstrcpyn( pcl->clItem.szFolderName, lpcstrName, 80 );
      fDataChanged = TRUE;
      Amuser_CallRegistered( AE_FOLDER_CHANGED, (LPARAM)pcl, (LPARAM)AECHG_NAME );
      return( AMDBERR_NOERROR );
      }
   return( AMDBERR_NOPERMISSION );
}

BOOL WINAPI EXPORT Amdb_IsResignedFolder( PCL pcl )
{
   PTL ptl;

   ASSERT( NULL != pcl );
   ptl = pcl->ptlFirst;
   while( ptl )
      {
      if( !( ptl->tlItem.dwFlags & TF_RESIGNED ) )
         return( FALSE );
      ptl = ptl->ptlNext;
      }
   return( TRUE );
}

BOOL WINAPI EXPORT Amdb_IsResignedTopic( PTL ptl )
{
   ASSERT( NULL != ptl );
   return( ( ptl->tlItem.dwFlags & TF_RESIGNED ) == TF_RESIGNED );
}

/* This function deletes all messages from the specified
 * topic.
 */
BOOL WINAPI EXPORT Amdb_EmptyTopic( PTL ptl )
{
   char szPath[ _MAX_PATH ];
   HNDFILE fh;

   /* Lock count must be zero.
    */
   if( ptl->wLockCount != 0 )
      return( FALSE );

   /* Discard the topic.
    */
   Amdb_DiscardTopic( ptl );

   /* Delete the TXT file.
    */
   wsprintf( szPath, "%s.TXT", ptl->tlItem.szFileName );
   Amdb_DeleteFile( szPath );

   /* Delete the THD file.
    */
   wsprintf( szPath, "%s.THD", ptl->tlItem.szFileName );
   fh = Amdb_CreateFile( szPath, AOF_WRITE );
   Amfile_Close( fh );

   /* Reset the lowest and highest message
    * numbers.
    */
   ptl->tlItem.dwMinMsg = 0;
   ptl->tlItem.dwMaxMsg = 0;
   ptl->tlItem.dwUpperMsg = 0;
   return( HNDFILE_ERROR != fh );
}

/* This function creates a new topic within the specified folder.
 */
PTL WINAPI EXPORT Amdb_CreateTopic( PCL pcl, LPSTR lpszTopicName, WORD wTopicType )
{
   TLITEM tlItem;
   PTL ptl;

   /* Check that caller is permitted to create a topic.
    */
   ASSERT( NULL != pcl );
   ASSERT( NULL != lpszTopicName );

   /* Create the topic.
    */
   memset( &tlItem, 0, sizeof( TLITEM ) );
   lstrcpy( tlItem.szTopicName, lpszTopicName );
   tlItem.wTopicType = wTopicType;
   if( fNewTopic( &ptl, pcl, &tlItem ) )
   {
      char szFileName[ _MAX_PATH ];
      HNDFILE fh;

      /* Set defaults for a new topic.
       */
      memset( &ptl->tlItem.msgc, 0, sizeof(MSGCOUNT) );
      ptl->tlItem.po = pcl->clItem.po;
      ptl->tlItem.vd = voDefault;
      lstrcpy( ptl->tlItem.szSigFile, pcl->clItem.szSigFile );
      if( lstrcmpi( ptl->tlItem.szTopicName, "files" ) == 0 )
         {
         ptl->tlItem.po.wFlags &= ~(PO_DELETEONLY|PO_SIZEPRUNE|PO_DATEPRUNE|PO_PURGE_IGNORED);
         ptl->tlItem.po.wFlags |= PO_SIZEPRUNE;
         ptl->tlItem.po.wMaxSize = DEF_PURGE_SIZE;
         ptl->tlItem.po.wMaxDate = DEF_PURGE_DATE;
         }

      if( lstrcmpi( ptl->tlItem.szTopicName, "faq" ) == 0 )
         {
         ptl->tlItem.po.wFlags &= ~(PO_DELETEONLY|PO_SIZEPRUNE|PO_DATEPRUNE|PO_PURGE_IGNORED);
         ptl->tlItem.po.wFlags |= PO_DELETEONLY;
         ptl->tlItem.po.wMaxSize = DEF_PURGE_SIZE;
         ptl->tlItem.po.wMaxDate = DEF_PURGE_DATE;
         }

      if( ( ptl->tlItem.po.wFlags & (PO_PURGE_IGNORED|PO_SIZEPRUNE|PO_DATEPRUNE|PO_DELETEONLY) ) == 0 )
         {
         ptl->tlItem.po.wFlags = poDefault.wFlags;
         ptl->tlItem.po.wMaxSize = poDefault.wMaxSize;
         ptl->tlItem.po.wMaxDate = poDefault.wMaxDate;
         }

      /* Close any cached TXT file.
       */
      if( fhCachedTxt != HNDFILE_ERROR )
         {
         Amfile_Close( fhCachedTxt );
         fhCachedTxt = HNDFILE_ERROR;
         ptlCachedTxt = NULL;
         }
//    CreateUniqueFile( lpszTopicName, "TXT", TRUE, ptl->tlItem.szFileName ); //!!SM!! 2038
      CreateUniqueFileMulti( lpszTopicName, "TXT", "THD", TRUE, ptl->tlItem.szFileName );
      wsprintf( szFileName, "%s.TXT", (LPSTR)ptl->tlItem.szFileName );
      if( ( fh = Amdb_OpenFile( szFileName, AOF_WRITE ) ) != HNDFILE_ERROR )
      {
         char sz[ 8192 ];

         /* Write unique info to the header so that we can recover it
          * when we rebuild the database.
          */
         wsprintf( sz, "!TX %s %s %s\r\n", pcl->clItem.szFolderName, lpszTopicName, szFileName );
         Amfile_Write( fh, sz, strlen( sz ) );
         Amfile_Close( fh );
      }
      ptl->tlItem.dateCreated = Amdate_GetPackedCurrentDate();
      ptl->tlItem.timeCreated = Amdate_GetPackedCurrentTime();
      ptl->tlItem.dwMinMsg = 0;
      ptl->tlItem.dwMaxMsg = 0;
      ptl->tlItem.dwUpperMsg = 0;
      ptl->wLockCount = 0;
      ptl->hwnd = NULL;
      ptl->tlItem.dwFlags = 0;
      fDataChanged = TRUE;
   }
   return( ptl );
}

/* This function creates a new topic.
 */
BOOL FASTCALL fNewTopic( PTL * pptl, PCL pcl, TLITEM * ptlItem )
{
   PTL ptl;

   ASSERT( NULL != pcl );
   ASSERT( NULL != pptl );
   ASSERT( NULL != ptlItem );
   INITIALISE_PTR(ptl);
   if( fNewSharedMemory( &ptl, sizeof( TL ) ) )
      {
      if( !pcl->ptlFirst )
         pcl->ptlFirst = ptl;
      else
         pcl->ptlLast->ptlNext = ptl;
      ptl->wID = STYP_TL;
      ptl->ptlNext = NULL;
      ptl->ptlPrev = pcl->ptlLast;
      ptl->pthFirst = NULL;
      ptl->pthLast = NULL;
      ptl->pthRefFirst = NULL;
      ptl->pthRefLast = NULL;
      ptl->pthCreateLast = NULL;
      ptl->pahFirst = NULL;
      ptl->pcl = pcl;
      ptl->tlItem = *ptlItem;
      if( ptl->tlItem.cMsgs )
         ptl->tlItem.dwFlags |= TF_ONDISK;
      ptl->wLockCount = 0;
      ptl->hwnd = NULL;
      pcl->ptlLast = ptl;
      *pptl = ptl;
      ++pcl->clItem.cTopics;
      Amuser_CallRegistered( AE_NEWTOPIC, (LPARAM)ptl, 0 );
      }
   return( ptl != NULL );
}

/* This function moves the specified category
 */
int WINAPI EXPORT Amdb_MoveCategory( PCAT pcat, PCAT pcatInsertAfter )
{
   /* First unlink the category from its current
    * position.
    */
   if( pcat->pcatPrev )
      pcat->pcatPrev->pcatNext = pcat->pcatNext;
   else
      pcatFirst= pcat->pcatNext;
   if( pcat->pcatNext )
      pcat->pcatNext->pcatPrev = pcat->pcatPrev;
   else
      pcatLast= pcat->pcatPrev;

   /* Now move it to its new position.
    */
   pcat->pcatPrev = pcatInsertAfter;
   if( pcatInsertAfter )
      {
      pcat->pcatNext = pcatInsertAfter->pcatNext;
      pcatInsertAfter->pcatNext = pcat;
      }
   else
      pcat->pcatNext = pcatFirst;
   if( pcat->pcatNext )
      pcat->pcatNext->pcatPrev = pcat;
   if( NULL == pcat->pcatNext )
      pcatLast = pcat;
   if( NULL == pcat->pcatPrev )
      pcatFirst = pcat;

   /* Mark the database as having been changed.
    */
   fDataChanged = TRUE;
   Amuser_CallRegistered( AE_MOVECATEGORY, (LPARAM)pcat, (LPARAM)-1 );
   return( AMDBERR_NOERROR );
}

/* This function moves the specified folder.
 */
int WINAPI EXPORT Amdb_MoveFolder( PCL pcl, PCAT pcatNewCategory, PCL pclInsertAfter )
{
   /* First unlink the topic from the original folder.
    */
   if( pcl->pclPrev )
      pcl->pclPrev->pclNext = pcl->pclNext;
   else
      pcl->pcat->pclFirst = pcl->pclNext;
   if( pcl->pclNext )
      pcl->pclNext->pclPrev = pcl->pclPrev;
   else
      pcl->pcat->pclLast = pcl->pclPrev;
   --pcl->pcat->catItem.cFolders;
   fDataChanged = TRUE;
   pcl->pcat->catItem.cMsgs -= pcl->clItem.cMsgs;
   ReduceMessageCounts( &pcl->pcat->msgc, &pcl->msgc );

   /* Is this a Keep At Top folder? If so, make sure the destination
    * is within a Keep At Top range.
    */
   if( ( pcl->clItem.dwFlags & CF_KEEPATTOP ) && NULL != pclInsertAfter )
      while( NULL != pclInsertAfter )
         {
         if( pclInsertAfter->clItem.dwFlags & CF_KEEPATTOP )
            break;
         pclInsertAfter = pclInsertAfter->pclPrev;
         }
   else if( !( pcl->clItem.dwFlags & CF_KEEPATTOP ) )
      {
      PCL pclBefore;

      /* This is not a Keep At Top folder we're moving, but make
       * sure we're not inserting BEFORE a Keep At Top folder.
       */
      pclBefore = pclInsertAfter ? pclInsertAfter->pclNext : pcatNewCategory->pclFirst;
      while( NULL != pclBefore && ( pclBefore->clItem.dwFlags & CF_KEEPATTOP ) )
         {
         pclInsertAfter = pclBefore;
         pclBefore = pclBefore->pclNext;
         }
      }

   /* Then link the topic into the new folder.
    */
   pcl->pclPrev = pclInsertAfter;
   if( pclInsertAfter )
      {
      pcl->pclNext = pclInsertAfter->pclNext;
      pclInsertAfter->pclNext = pcl;
      }
   else
      pcl->pclNext = pcatNewCategory->pclFirst;
   if( pcl->pclNext )
      pcl->pclNext->pclPrev = pcl;
   if( NULL == pcl->pclNext )
      pcatNewCategory->pclLast = pcl;
   if( NULL == pcl->pclPrev )
      pcatNewCategory->pclFirst = pcl;
   pcl->pcat = pcatNewCategory;
   ++pcatNewCategory->catItem.cFolders;
   pcatNewCategory->catItem.cMsgs += pcl->clItem.cMsgs;
   UpdateMessageCounts( &pcatNewCategory->msgc, &pcl->msgc );

   /* Mark the database as having been changed.
    */
   fDataChanged = TRUE;
   Amuser_CallRegistered( AE_MOVEFOLDER, (LPARAM)pcl, (LPARAM)-1 );
   return( AMDBERR_NOERROR );
}

/* This function moves the specified topic to the beginning of the new folder.
 */
int WINAPI EXPORT Amdb_MoveTopic( PTL ptl, PCL pclNewFolder, PTL ptlInsertAfter )
{
   PCL pclOld;

   /* First unlink the topic from the original folder.
    */
   if( ptl->ptlPrev )
      ptl->ptlPrev->ptlNext = ptl->ptlNext;
   else
      ptl->pcl->ptlFirst = ptl->ptlNext;
   if( ptl->ptlNext )
      ptl->ptlNext->ptlPrev = ptl->ptlPrev;
   else
      ptl->pcl->ptlLast = ptl->ptlPrev;
   --ptl->pcl->clItem.cTopics;
   fDataChanged = TRUE;
   ReduceMessageCounts( &ptl->pcl->msgc, &ptl->tlItem.msgc );
   ReduceMessageCounts( &ptl->pcl->pcat->msgc, &ptl->tlItem.msgc );
   ptl->pcl->clItem.cMsgs -= ptl->tlItem.cMsgs;
   ptl->pcl->pcat->catItem.cMsgs -= ptl->tlItem.cMsgs;

   /* Then link the topic into the new folder.
    */
   ptl->ptlPrev = ptlInsertAfter;
   if( ptlInsertAfter )
      {
      ptl->ptlNext = ptlInsertAfter->ptlNext;
      ptlInsertAfter->ptlNext = ptl;
      }
   else
      ptl->ptlNext = pclNewFolder->ptlFirst;
   if( ptl->ptlNext )
      ptl->ptlNext->ptlPrev = ptl;
   if( NULL == ptl->ptlNext )
      pclNewFolder->ptlLast = ptl;
   if( NULL == ptl->ptlPrev )
      pclNewFolder->ptlFirst = ptl;
   pclOld = ptl->pcl;
   ptl->pcl = pclNewFolder;
   ++pclNewFolder->clItem.cTopics;
   UpdateMessageCounts( &pclNewFolder->msgc, &ptl->tlItem.msgc );
   UpdateMessageCounts( &pclNewFolder->pcat->msgc, &ptl->tlItem.msgc );
   pclNewFolder->clItem.cMsgs += ptl->tlItem.cMsgs;
   pclNewFolder->pcat->catItem.cMsgs += ptl->tlItem.cMsgs;

   /* Migrate cookies.
    */
   if( pclOld != ptl->pcl )
      Amdb_MoveCookies( ptl, pclOld );

   /* Mark the database as having been changed.
    */
   fDataChanged = TRUE;
   Amuser_CallRegistered( AE_MOVETOPIC, (LPARAM)ptl, (LPARAM)-1 );
   return( AMDBERR_NOERROR );
}

/* Return the threshold for topic text files
 */
DWORD WINAPI EXPORT Amdb_GetTextFileThreshold( void )
{
   return SIZE_THRESHOLD;
}

/* This function returns the size of the topic text file
 */
DWORD WINAPI EXPORT Amdb_GetTopicTextFileSize( PTL ptl )
{
   char szFileName[ _MAX_PATH ];
   DWORD dwFileSize = 0L;
   struct _stat st;

   wsprintf( szFileName, "%s\\%s.TXT", szDataDir, ptl->tlItem.szFileName );
   if( _stat( szFileName, &st ) != -1 )
      dwFileSize = st.st_size;
   return dwFileSize;
}

/* This function does a fast mark-all-read of every message in
 * the specified topic.
 */
DWORD WINAPI EXPORT Amdb_MarkTopicRead( PTL ptl )
{
   DWORD dwMarkedRead;

   if( dwMarkedRead = ptl->tlItem.msgc.cUnRead )
      {
      DWORD cUnRead;
      PTH pth;

      cUnRead = 0L;
      if( ptl->tlItem.dwFlags & TF_ONDISK )
         {
         /* BUGBUG
          * This doesn't take read-locked messages
          * into account!
          */
         cUnRead = ptl->tlItem.msgc.cUnRead;
         ptl->tlItem.dwFlags |= TF_MARKALLREAD;
         }
      else
         {
         HNDFILE fh;

         for( pth = ptl->pthFirst; pth; pth = pth->pthNext )
            if( !( pth->thItem.dwFlags & (MF_READ|MF_READLOCK) ) )
               {
               if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
                  {
                  pth->thItem.dwFlags |= MF_READ;
                  if( pth->thItem.dwFlags & MF_PRIORITY )
                     {
                     --pth->ptl->tlItem.msgc.cUnReadPriority;
                     --cTotalMsgc.cUnReadPriority;
                     }
                  UnlockMessage( fh, pth );
                  if( pth->pthRoot )
                     --pth->pthRoot->wUnRead;
                  ++cUnRead;
                  }
               }
         }
      cTotalMsgc.cUnRead -= cUnRead;
      ptl->pcl->msgc.cUnRead -= cUnRead;
      ptl->pcl->pcat->msgc.cUnRead -= cUnRead;
      ptl->tlItem.msgc.cUnRead -= cUnRead;
      fDataChanged = TRUE;
      Amuser_CallRegistered( AE_UNREADCHANGED, (LPARAM)ptl, 0 );
      }
   return( dwMarkedRead );
}

/* This function marks read every message up to the supplied date
 * of every message in the specified topic.
 */
DWORD WINAPI EXPORT Amdb_MarkTopicDateRead( PTL ptl, WORD wDate )
{
   DWORD dwMarkedRead;

   if( dwMarkedRead = ptl->tlItem.msgc.cUnRead )
      {
      DWORD cUnRead;
      PTH pth;
      HNDFILE fh;

      cUnRead = 0L;
      if( ptl->tlItem.dwFlags & TF_ONDISK )
         {
         /* BUGBUG
          * This doesn't take read-locked messages
          * into account!
          */
         cUnRead = ptl->tlItem.msgc.cUnRead;
         CommitTopicMessages( ptl, FALSE );
         }

      
      for( pth = ptl->pthFirst; pth; pth = pth->pthNext )
         if( !( pth->thItem.dwFlags & (MF_READ|MF_READLOCK) ) && ( pth->thItem.wDate < wDate ) )
            {
            if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
               {
               pth->thItem.dwFlags |= MF_READ;
               if( pth->thItem.dwFlags & MF_PRIORITY )
                  {
                  --pth->ptl->tlItem.msgc.cUnReadPriority;
                  --cTotalMsgc.cUnReadPriority;
                  }
               UnlockMessage( fh, pth );
               if( pth->pthRoot )
                  --pth->pthRoot->wUnRead;
               ++cUnRead;
               }
            }
      cTotalMsgc.cUnRead -= cUnRead;
      ptl->pcl->msgc.cUnRead -= cUnRead;
      ptl->pcl->pcat->msgc.cUnRead -= cUnRead;
      ptl->tlItem.msgc.cUnRead -= cUnRead;
      fDataChanged = TRUE;
      Amuser_CallRegistered( AE_UNREADCHANGED, (LPARAM)ptl, 0 );
      }
   return( dwMarkedRead );
}

void WINAPI EXPORT Amdb_SetTopicSel( PTL ptl, HWND hwnd )
{
   ptl->hwnd = hwnd;
}

HWND WINAPI EXPORT Amdb_GetTopicSel( PTL ptl )
{
   return( ptl->hwnd );
}

/* This function opens the specified topic within the given folder.
 */
PTL WINAPI EXPORT Amdb_OpenTopic( PCL pcl, LPSTR lpszTopicName )
{
   PTL ptl;

   for( ptl = pcl->ptlFirst; ptl; ptl = ptl->ptlNext )
      {
      if( lstrcmp( ptl->tlItem.szTopicName, lpszTopicName ) == 0 )
         break;
      }
   return( ptl );
}

void WINAPI EXPORT Amdb_LockTopic( PTL ptl )
{
   if( ptl->wLockCount < 0xFFFF )
      ++ptl->wLockCount;
}

void WINAPI EXPORT Amdb_UnlockTopic( PTL ptl )
{
   Amdb_InternalUnlockTopic( ptl );
   Amdb_SweepAndDiscard();
}

int WINAPI EXPORT Amdb_GetLockCount( PTL ptl )
{
   return( ptl->wLockCount );
}

/* Unlock a topic. Also clear the in-use flag.
 */
void WINAPI EXPORT Amdb_InternalUnlockTopic( PTL ptl )
{
   if( ptl->wLockCount > 0 )
      --ptl->wLockCount;
   if( 0 == ptl->wLockCount )
      ptl->tlItem.dwFlags &= ~TF_TOPICINUSE;
}

void WINAPI EXPORT Amdb_SetTopicFlags( PTL ptl, DWORD dwFlags, BOOL fSet )
{
   if( fSet ) {
      if( ( ptl->tlItem.dwFlags & dwFlags ) != dwFlags )
         {
         ptl->tlItem.dwFlags |= dwFlags;
         ptl->tlItem.dwFlags |= TF_CHANGED;
         fDataChanged = TRUE;
         Amuser_CallRegistered( AE_TOPIC_CHANGED, (LPARAM)ptl, (LPARAM)AECHG_FLAGS );
         }
      }
   else {
      if( ptl->tlItem.dwFlags & dwFlags )
         {
         ptl->tlItem.dwFlags &= ~dwFlags;
         ptl->tlItem.dwFlags |= TF_CHANGED;
         fDataChanged = TRUE;
         Amuser_CallRegistered( AE_TOPIC_CHANGED, (LPARAM)ptl, (LPARAM)AECHG_FLAGS );
         }
      }
}

DWORD WINAPI EXPORT Amdb_GetTopicFlags( PTL ptl, DWORD dwMask )
{
   return( (ptl->tlItem.dwFlags|ptl->tlItem.dwFlags) & dwMask );
}

/* This function scans through all topics in all folders and discards any
 * which are unlocked whose message base is not on disk.
 */
void WINAPI EXPORT Amdb_SweepAndDiscard( void )
{
   PCAT pcat;
   PCL pcl;
   PTL ptl;

   for( pcat = pcatFirst; ( pcat && pcat->pclFirst ); pcat = pcat->pcatNext )
      for( pcl = pcat->pclFirst; ( pcl && pcl->ptlFirst ); pcl = pcl->pclNext )
         for( ptl = pcl->ptlFirst; ptl; ptl = ptl->ptlNext ) {
            if( ptl->wLockCount == 0 )
               Amdb_DiscardTopic( ptl );
            }
}

/* This function returns a pointer to the ASCIIZ string of the topic
 * name. The name must not be altered.
 */
LPCSTR WINAPI EXPORT Amdb_GetTopicName( PTL ptl )
{
   return( (LPCSTR)ptl->tlItem.szTopicName );
}

/* This function sets the topic name.
 */
int WINAPI EXPORT Amdb_SetTopicName( PTL ptl, LPCSTR lpcstrName )
{
   /* Broadcast event.
    */
   if( Amuser_CallRegistered( AE_TOPIC_CHANGING, (LPARAM)ptl, (LPARAM)AECHG_NAME ) )
      {
      lstrcpyn( ptl->tlItem.szTopicName, lpcstrName, 78 );
      ptl->tlItem.dwFlags |= TF_CHANGED;
      fDataChanged = TRUE;
      Amuser_CallRegistered( AE_TOPIC_CHANGED, (LPARAM)ptl, (LPARAM)AECHG_NAME );
      return( AMDBERR_NOERROR );
      }
   return( AMDBERR_NOPERMISSION );
}

void WINAPI EXPORT Amdb_GetInternalTopicName( PTL ptl, LPSTR lpsz )
{
   lstrcpy( lpsz, ptl->tlItem.szFileName );
}

/* This function returns a pointer to the first topic in the folder.
 */
PTL WINAPI EXPORT Amdb_GetFirstTopic( PCL pcl )
{
   ASSERT( NULL != pcl );
   return( pcl->ptlFirst );
}

/* This function returns a pointer to the last topic in the folder.
 */
PTL WINAPI EXPORT Amdb_GetLastTopic( PCL pcl )
{
   ASSERT( NULL != pcl );
   return( pcl->ptlLast );
}

/* This function returns a pointer to the next topic.
 */
PTL WINAPI EXPORT Amdb_GetNextTopic( PTL ptl )
{
   ASSERT( NULL != ptl );
   return( ptl->ptlNext );
}

/* This function returns a pointer to the previous topic.
 */
PTL WINAPI EXPORT Amdb_GetPreviousTopic( PTL ptl )
{
   ASSERT( NULL != ptl );
   return( ptl->ptlPrev );
}

void WINAPI EXPORT Amdb_GetFolderInfo( PCL pcl, FOLDERINFO FAR * pFolderInfo )
{
   ASSERT( NULL != pcl );
   lstrcpy( pFolderInfo->szFileName, pcl->clItem.szFileName );
   lstrcpy( pFolderInfo->szFolderName, pcl->clItem.szFolderName );
   lstrcpy( pFolderInfo->szSigFile, pcl->clItem.szSigFile );
   pFolderInfo->po = pcl->clItem.po;
   pFolderInfo->wCreationTime = pcl->clItem.timeCreated;
   pFolderInfo->wCreationDate = pcl->clItem.dateCreated;
   pFolderInfo->cTopics = pcl->clItem.cTopics;
   pFolderInfo->cMsgs = pcl->clItem.cMsgs;
   pFolderInfo->cUnRead = pcl->msgc.cUnRead;
   pFolderInfo->wLastPurgeDate = pcl->clItem.wLastPurgeDate;
   pFolderInfo->wLastPurgeTime = pcl->clItem.wLastPurgeTime;
}

void WINAPI EXPORT Amdb_SetTopicSignature( PTL ptl, LPSTR lpszFileName )
{
   ASSERT( NULL != ptl );
   ASSERT( NULL != lpszFileName );
   SetSignature( ptl->tlItem.szSigFile, lpszFileName );
   fDataChanged = TRUE;
}

void FASTCALL SetSignature( LPSTR lpszSigBuf, LPSTR lpszFileName )
{
   register int c;
   register int d;

   for( c = d = 0; lpszFileName[ c ]; ++c )
      if( lpszFileName[ c ] == ':' || lpszFileName[ c ] == '\\' )
         d = c + 1;
   for( c = 0; c < 8 && lpszFileName[ d ] && lpszFileName[ d ] != '.'; ++c, ++d )
      lpszSigBuf[ c ] = lpszFileName[ d ];
   lpszSigBuf[ c ] = '\0';
}

void WINAPI EXPORT Amdb_SetFolderSignature( PCL pcl, LPSTR lpszFileName )
{
   ASSERT( NULL != pcl );
   ASSERT( NULL != lpszFileName );
   SetSignature( pcl->clItem.szSigFile, lpszFileName );
   fDataChanged = TRUE;
}

void WINAPI EXPORT Amdb_GetFolderSignature( PCL pcl, LPSTR lpszFileName )
{
   ASSERT( NULL != pcl );
   lstrcpy( lpszFileName, pcl->clItem.szSigFile );
}

/* This function returns information about the specified topic.
 */
void WINAPI EXPORT Amdb_GetTopicInfo( PTL ptl, TOPICINFO FAR * pTopicInfo )
{
   ASSERT( NULL != ptl );
   ASSERT( NULL != pTopicInfo );
   lstrcpy( pTopicInfo->szTopicName, ptl->tlItem.szTopicName );
   lstrcpy( pTopicInfo->szFileName, ptl->tlItem.szFileName );
   lstrcpy( pTopicInfo->szSigFile, ptl->tlItem.szSigFile );
   pTopicInfo->vd = ptl->tlItem.vd;
   pTopicInfo->po = ptl->tlItem.po;
   pTopicInfo->wCreationTime = ptl->tlItem.timeCreated;
   pTopicInfo->wCreationDate = ptl->tlItem.dateCreated;
   pTopicInfo->dwFlags = ptl->tlItem.dwFlags|ptl->tlItem.dwFlags;
   pTopicInfo->cMsgs = ptl->tlItem.cMsgs;
   pTopicInfo->cUnRead = ptl->tlItem.msgc.cUnRead;
   pTopicInfo->cMarked = ptl->tlItem.msgc.cMarked;
   pTopicInfo->cPriority = ptl->tlItem.msgc.cPriority;
   pTopicInfo->dwMinMsg = ptl->tlItem.dwMinMsg;
   pTopicInfo->dwMaxMsg = ptl->tlItem.dwMaxMsg;
   pTopicInfo->dwUpperMsg = ptl->tlItem.dwUpperMsg;
   pTopicInfo->wLastPurgeDate = ptl->tlItem.wLastPurgeDate;
   pTopicInfo->wLastPurgeTime = ptl->tlItem.wLastPurgeTime;
}

/* This function computes the disk space used by the topic. It
 * tries to be as accurate as possible.
 */
DWORD WINAPI EXPORT Amdb_GetDiskSpaceUsed( PTL ptl )
{
   char szFileName[ _MAX_PATH ];
   DWORD dwFileSize;
   struct _stat st;

   /* Get the TXT file size.
    */
   dwFileSize = 0L;
   wsprintf( szFileName, "%s\\%s.TXT", szDataDir, ptl->tlItem.szFileName );
   if( _stat( szFileName, &st ) != -1 )
      dwFileSize += st.st_size;

   /* Add the THD file size.
    */
   wsprintf( szFileName, "%s\\%s.THD", szDataDir, ptl->tlItem.szFileName );
   if( _stat( szFileName, &st ) != -1 )
      dwFileSize += st.st_size;

   /* Add the CKE file size.
    */
   wsprintf( szFileName, "%s\\%s.CKE", szDataDir, ptl->tlItem.szFileName );
   if( _stat( szFileName, &st ) != -1 )
      dwFileSize += st.st_size;

   /* Add the ATC file size.
    */
   wsprintf( szFileName, "%s\\%s.ATC", szDataDir, ptl->tlItem.szFileName );
   if( _stat( szFileName, &st ) != -1 )
      dwFileSize += st.st_size;
   return( dwFileSize );
}

/* This function sets the topic VIEWDATA information.
 */
void WINAPI EXPORT Amdb_SetTopicViewData( PTL ptl, VIEWDATA FAR * pvd )
{
   ASSERT( NULL != ptl );
   ASSERT( NULL != pvd );
   ptl->tlItem.vd = *pvd;
   fDataChanged = TRUE;
}

/* This function gets the topic PURGEOPTIONS information.
 */
void WINAPI EXPORT Amdb_GetTopicPurgeOptions( PTL ptl, PURGEOPTIONS FAR * ppo )
{
   ASSERT( NULL != ptl );
   ASSERT( NULL != ppo );
   *ppo = ptl->tlItem.po;
}

/* This function sets the topic PURGEOPTIONS information.
 */
void WINAPI EXPORT Amdb_SetTopicPurgeOptions( PTL ptl, PURGEOPTIONS FAR * ppo )
{
   ASSERT( NULL != ptl );
   ASSERT( NULL != ppo );
   ptl->tlItem.po = *ppo;
   fDataChanged = TRUE;
}

/* This function gets the folder PURGEOPTIONS information.
 */
void WINAPI EXPORT Amdb_GetFolderPurgeOptions( PCL pcl, PURGEOPTIONS FAR * ppo )
{
   ASSERT( NULL != pcl );
   ASSERT( NULL != ppo );
   *ppo = pcl->clItem.po;
}

/* This function sets the folder PURGEOPTIONS information.
 */
void WINAPI EXPORT Amdb_SetFolderPurgeOptions( PCL pcl, PURGEOPTIONS FAR * ppo )
{
   ASSERT( NULL != pcl );
   ASSERT( NULL != ppo );
   pcl->clItem.po = *ppo;
   fDataChanged = TRUE;
}

/* Discards a topic by flushing the threads to disk, then deleting all memory used by the
 * message threads. The topic header remains intact.
 */
void WINAPI EXPORT Amdb_DiscardTopic( PTL ptl )
{
   PTH pth;
   PAH pah;
   PCL pcl;

   /* Don't bother if the topic is not paged in, the lock
    * count is non-zero or the topic is empty.
    */
   ASSERT( NULL != ptl );
   if( ptl->tlItem.dwFlags & TF_ONDISK )
      return;
   if( ptl->wLockCount > 0 )
      return;
   if( ptl->tlItem.cMsgs == 0 )
      return;

   /* Remove all messages
    */
   pth = ptl->pthFirst;
   while( pth )
      {
      PTH pthNext = pth->pthNext;

      FreeMemory( &pth );
      pth = pthNext;
      }

   /* Save any modified attachments.
    */
   SaveAttachments( ptl );

   /* Discard attachments.
    */
   pah = ptl->pahFirst;
   while( pah )
      {
      PAH pahNext = pah->pahNext;

      FreeMemory( &pah );
      pah = pahNext;
      }

   /* If this topic has been cached, delete
    * the cache.
    */
   if( ptl == ptlCachedThd )
      {
      Amfile_Close( fhCachedThd );
      fhCachedThd = HNDFILE_ERROR;
      ptlCachedThd = NULL;
      }
   if( ptl == ptlCachedTxt )
      {
      Amfile_Close( fhCachedTxt );
      fhCachedTxt = HNDFILE_ERROR;
      ptlCachedTxt = NULL;
      }

   /* If the conference for this topic has
    * a list of moderators, remove it.
    */
   pcl = ptl->pcl;
   if( pcl->lpModList )
      {
      FreeMemory( &pcl->lpModList );
      pcl->lpModList = NULL;
      }

   /* Vape pointers, so that the next reference to
    * this topic will cause a reload.
    */
   ptl->pthFirst = NULL;
   ptl->pahFirst = NULL;
   ptl->pthLast = NULL;
   ptl->pthRefFirst = NULL;
   ptl->pthRefLast = NULL;
   ptl->pthCreateLast = NULL;
   ptl->tlItem.dwFlags |= TF_ONDISK;
}

/* Given a pointer to a folder, this function returns a pointer to
 * the category to which this folder belongs.
 */
PCAT WINAPI EXPORT Amdb_CategoryFromFolder( PCL pcl )
{
   ASSERT( NULL != pcl );
   return( pcl->pcat );
}

/* Given a pointer to a topic, this function returns a pointer to the
 * folder to which this topic belongs.
 */
PCL WINAPI EXPORT Amdb_FolderFromTopic( PTL ptl )
{
   ASSERT( NULL != ptl );
   return( ptl->pcl );
}

/* Given a pointer to a message, this function returns a pointer to the
 * topic to which this message belongs.
 */
PTL WINAPI EXPORT Amdb_TopicFromMsg( PTH pth )
{
   ASSERT( NULL != pth );
   ASSERT( pth->ptl->wLockCount > 0 );
   return( pth->ptl );
}

DWORD WINAPI EXPORT Amdb_MessageIdToComment( PTL ptl, LPSTR lpszReference )
{
   PTH pth;

   ASSERT( NULL != ptl );
   ASSERT( NULL != lpszReference );
   CommitTopicMessages( ptl, TRUE );
   for( pth = ptl->pthRefLast; pth; pth = pth->pthRefPrev )
      if( lstrcmpi( pth->thItem.szMessageId, lpszReference ) == 0 )
         return( pth->thItem.dwMsg );
   return( 0L );
}

DWORD WINAPI EXPORT Amdb_LookupSubjectReference( PTL ptl, LPSTR lpszSubject, LPSTR lpszIgnoreSubject )
{
   LPSTR lpszRefSubject;
   PTH pth;

   ASSERT( NULL != ptl );
   ASSERT( NULL != lpszSubject );
   lpszRefSubject = NULL;
   if( *lpszIgnoreSubject )
   {
      if( _strnicmp( lpszSubject, lpszIgnoreSubject, strlen( lpszIgnoreSubject ) ) == 0 )
      {
      lpszSubject = lpszSubject + strlen( lpszIgnoreSubject );
      while( *lpszSubject == ' ' )
         ++lpszSubject;
      }
   }
   if( ( _strnicmp( lpszSubject, "Re:", 3 ) == 0 ) || ( _strnicmp( lpszSubject, "SV:", 3 ) == 0 ) )
      {
      lpszRefSubject = lpszSubject + 3;
      while( *lpszRefSubject == ' ' )
         ++lpszRefSubject;
      }
   else if( ( _strnicmp( lpszSubject, "Re[", 3 ) == 0 || _strnicmp( lpszSubject, "SV[", 3 ) == 0 ) && isdigit( lpszSubject[3] ) )
      {
      lpszRefSubject = lpszSubject + 3;
      while( *lpszRefSubject && *lpszRefSubject != ']' )
         ++lpszRefSubject;
      if( *lpszRefSubject == ':' )
         ++lpszRefSubject;
      if( *lpszRefSubject == ']' )
         ++lpszRefSubject;
      while( *lpszRefSubject == ' ' )
         ++lpszRefSubject;
      }
   CommitTopicMessages( ptl, TRUE );
   for( pth = ptl->pthRefLast; pth; pth = pth->pthRefPrev )
      {
      LPSTR lpszNewSubj;

      lpszNewSubj = pth->thItem.szTitle;
      if( *lpszIgnoreSubject )
      {
         if( _strnicmp( lpszNewSubj, lpszIgnoreSubject, strlen( lpszIgnoreSubject ) ) == 0 )
         {
         lpszNewSubj = lpszNewSubj + strlen( lpszIgnoreSubject );
         while( *lpszNewSubj == ' ' )
            ++lpszNewSubj;
         }
      }
      if( lpszRefSubject && lstrcmpi( lpszNewSubj, lpszRefSubject ) == 0 )
         return( pth->thItem.dwMsg );
      if( lstrcmpi( lpszNewSubj, lpszSubject ) == 0 )
         return( pth->thItem.dwMsg );
      }
   return( 0L );
}

/* This function locates a header field in a mail message. It doesn't attempt
 * to check that the message is in a mail folder.
 */
BOOL WINAPI EXPORT Amdb_GetMailHeaderField( PTH pth, LPSTR pszHdr, LPSTR pBuf, int nMax, BOOL fGetAll )
{
   BOOL fGotAll;
   HPSTR hpText;

   ASSERT( NULL != pth );
   ASSERT( NULL != pszHdr );
   ASSERT( NULL != pBuf );
   ASSERT( pth->ptl->wLockCount > 0 );
   *pBuf = '\0';
   fGotAll = FALSE;
   if( hpText = Amdb_GetMsgText( pth ) )
      {
      fGotAll = Amdb_GetMailHeaderFieldInText( hpText, pszHdr, pBuf, nMax, fGetAll );
      Amdb_FreeMsgTextBuffer( hpText );
      }
   return( fGotAll );
}

/* This function locates a header field in the text of a mail message.
 */
/* This is the old version

BOOL WINAPI EXPORT Amdb_GetMailHeaderFieldInText( HPSTR hpText, LPSTR pszHdr, LPSTR pBuf, int nMax, BOOL fGetAll )
{
   register int c;
   BOOL fGotAll;

   fGotAll = TRUE;
   c = 0;
   while( *hpText )
      {
      char szType[ LEN_RFCTYPE + 1 ];

      if( *hpText == 13 || *hpText == 10 )
         break;
      if( *hpText != ' ' && *hpText != '\t' )
         {
         int m;

         for( m = 0; m < LEN_RFCTYPE && *hpText != 13 && *hpText && *hpText != ':'; ++m )
            szType[ m ] = *hpText++;
         szType[ m ] = '\0';
         if( *hpText == ':' ) ++hpText;
         while( *hpText == ' ' )
            ++hpText;
         if( _strcmpi( pszHdr, szType ) == 0 )
            {
            LPSTR pBufStart;

            pBufStart = pBuf;
            for( ; *hpText && c < nMax - 1; ++c )
               {
               if( *hpText == 13 )
                  {
                  if( *++hpText == 10 )
                     ++hpText;
                  if( *hpText != ' ' && *hpText != '\t' )
                     break;
                  while( *hpText == ' ' || *hpText == '\t' )
                     ++hpText;
                  if( *hpText == '\0' || *hpText == 13 )
                     break;
                  *pBuf++ = ' ';
                  if( ++c == nMax - 1 )
                     break;
                  }
               *pBuf++ = *hpText++;
               pBufStart = pBuf;
               }
            if( c == nMax - 1 && *hpText )
               {
               pBuf = pBufStart;
               fGotAll = FALSE;
               while( ( *( pBuf - 1 ) != ',' && *( pBuf - 1 ) != '>' && *( pBuf - 1 ) != ' ' ) )
                  --pBuf;
               --pBuf;
               break;
               }
            if( !fGetAll )
               break;
            }
         }
      while( *hpText && *hpText != 10 )
         ++hpText;
      if( *hpText == 10 )
         ++hpText;
      }     
   *pBuf = '\0';
   return( fGotAll );
}
*/

/* This is the new one courtesy of Stephen Mott. Thanks! */

BOOL WINAPI EXPORT Amdb_GetMailHeaderFieldInText( HPSTR hpText, LPSTR pszHdr, LPSTR pBuf, int nMax, BOOL fGetAll )
{
        register int c;
        BOOL fGotAll;
        BOOL lFound;
        BOOL lMaxedOut;
      BOOL lFirstChar;
        int m;
                                
        fGotAll = TRUE;
        lFound = FALSE;
      lMaxedOut = FALSE;
      lFirstChar = TRUE;
      pBuf[0] = '\x0';
        c = 0;
        while( *hpText && !lFound)
        {
                char szType[ LEN_RFCTYPE + 1 ];
                
                if( *hpText == 13 || *hpText == 10 )
                        break;
                if( *hpText != ' ' && *hpText != '\t' )
                {
                        lMaxedOut = FALSE;
                        for( m = 0; m < LEN_RFCTYPE && *hpText != 13 && *hpText && *hpText != ':'; ++m )
                                szType[ m ] = *hpText++;
        
                        if( m >= LEN_RFCTYPE )
                                lMaxedOut = TRUE;

                        szType[ m ] = '\0';
                        
                        if( *hpText == ':' ) ++hpText;
                        
                        while( *hpText == ' ' )
                                ++hpText;
                        
                        if( _strcmpi( pszHdr, szType ) == 0 )
                        {
                                LPSTR pBufStart;
                                
                                pBufStart = pBuf;
                                
                        lFound = TRUE; // !!SM!! 2038

                                for( ; *hpText && c < nMax - 1; ++c )
                                {
                                        if( *hpText == 13 )
                                        {
                                                if( *++hpText == 10 )
                                                        ++hpText;
                                                if( *hpText != ' ' && *hpText != '\t' )
                                                        break;
                                                while( *hpText == ' ' || *hpText == '\t' )
                                                        ++hpText;
                                                if( *hpText == '\0' || *hpText == 13 )
                                                        break;
                                                *pBuf++ = ' ';
                                                if( ++c == nMax - 1 )
                                                        break;
                                        }
                                        *pBuf++ = *hpText++;
                                        pBufStart = pBuf;
                                }
                                if( c == nMax - 1 && *hpText )
                                {
                                        pBuf = pBufStart;
                                        fGotAll = FALSE;
                                        while( ( *( pBuf - 1 ) != ',' && *( pBuf - 1 ) != '>' && *( pBuf - 1 ) != ' ' ) )
                                                --pBuf;
                                        --pBuf;
                                        break;
                                }
                                
                                if( !fGetAll )
                                        break;
                                else
                                {
                                        *pBuf++ = ','; // Added by SM to allow breakout of the while
                                        *pBuf++ = ' '; // Added by SM to allow breakout of the while
                                }
                        }
                }
                while( *hpText && *hpText != 10) // Removed by SM to stop the skipping of valid headers
                      ++hpText;               // Added back 2004-10-27 SM because it finds header mid way through a line, e.g. Subject: From: Fred
                
                if( lMaxedOut )
                        ++hpText;
                while( *hpText && *hpText != 10 && ( *hpText == 9 || *hpText == 32 ))
                        ++hpText;
                if( *hpText == 13)
                        ++hpText;
                if( *hpText == 10)
                        ++hpText;
        }                               
      if (lFound)
      {
         *pBuf = '\0';

         /*
          Trim off extra <comma><space>
          */
         while( *pBuf == '\0' || *pBuf == ',' || *pBuf == ' ')
         {
               *pBuf = '\0';
               *pBuf--;
         }

      }
        return( fGotAll );
}


void WINAPI EXPORT Amdb_SetMsgSel( PTH pth, int nSel )
{
   ASSERT( NULL != pth );
   ASSERT( pth->ptl->wLockCount > 0 );
   pth->nSel = nSel;
}

int WINAPI EXPORT Amdb_GetMsgSel( PTH pth )
{
   ASSERT( NULL != pth );
   ASSERT( pth->ptl->wLockCount > 0 );
   return( pth->nSel );
}

/* This function sets the dwRealSize field of the message to
 * the number of lines. The top bit shows that this is a count
 * of lines and not bytes.
 */
void WINAPI EXPORT Amdb_SetLines( PTH pth, DWORD cLines )
{
   HNDFILE fh;

   if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
      {
      pth->thItem.dwRealSize = cLines | 0x80000000;
      UnlockMessage( fh, pth );
      }
}

/* This function stores a message in the specified topic.
 */
int WINAPI EXPORT Amdb_CreateMsg( PTL ptl, PTH FAR * lppth, DWORD dwMsg,
   DWORD dwComment, LPSTR lpszTitle, LPSTR lpszAuthor, LPSTR lpszMessageId, AM_DATE FAR * lpDate,
   AM_TIME FAR * lpTime, HPSTR lpszMsg, DWORD cbsz, DWORD dwLocalFlag )
{
   static BOOL fCallback = FALSE;
   NEWMSGSTRUCT nms;
   DWORD dwMsgPtr;
   register int c;
   int retryCount = 0;
   THITEM thItem;
   BOOL fExists;
   int wError;
   PTH pth;

   /* Check that caller is permitted to create a new message.
    */
   ASSERT( NULL != ptl );
   ASSERT( NULL != lpszAuthor );
   ASSERT( NULL != lpszTitle );
   ASSERT( NULL != lpDate );
   ASSERT( NULL != lpTime );

   /* Allow addons which hooked AE_ADDNEWMSG to get first bite of the
    * cherry.
    */
   if( !fCallback )
      {
      nms.ptl = ptl;
      nms.dwMsg = dwMsg;
      nms.dwComment = dwComment;
      nms.lpszTitle = lpszTitle;
      nms.lpszAuthor = lpszAuthor;
      nms.lpszMessageId = lpszMessageId;
      nms.lpDate = lpDate;
      nms.lpTime = lpTime;
      nms.lpszMsg = lpszMsg;
      nms.cbsz = cbsz;
      nms.dwFlags = (WORD)dwLocalFlag;
      fCallback = TRUE;
      if( !Amuser_CallRegistered( AE_ADDNEWMSG, (LPARAM)(LPSTR)&nms, 0 ) )
         {
         fCallback = FALSE;
         return( AMDBERR_IGNOREDERROR );
         }
      fCallback = FALSE;
      }

   /* Okay, now create the message.
    */
   wError = AMDBERR_NOERROR;
   memset( &thItem, 0, sizeof( THITEM ) );
   thItem.dwMsg = dwMsg;
   thItem.dwComment = dwComment;
   for( c = 0; lpszAuthor[ c ] && c < 79; ++c )
      thItem.szAuthor[ c ] = lpszAuthor[ c ];
   thItem.szAuthor[ c ] = '\0';
   lstrcpyn( thItem.szMessageId, lpszMessageId, 79 );
   if( *lpszTitle )
      lstrcpyn( thItem.szTitle, lpszTitle, sizeof( thItem.szTitle ) );
   else
      StoreTitle( thItem.szTitle, (LPSTR)lpszMsg );
   thItem.dwOffset = 0L;
   thItem.dwSize = 0L;
   thItem.dwFlags = 0;
   thItem.wDate = Amdate_PackDate( lpDate );
   thItem.wTime = Amdate_PackTime( lpTime );
   CommitTopicMessages( ptl, TRUE );
   Amdb_LockTopic( ptl );

   /* If adding a message to the mail topic, then convert the message number and id to
    * internal format. Remember to scan the topic first for an existing message with the
    * same ID.
    */
   if( ptl->tlItem.wTopicType == FTYPE_MAIL )
      {
      PTH pth;
      BOOL fFound;

      fFound = FALSE;
      for( pth = ptl->pthLast; !fFound && pth; pth = pth->pthPrev )
         {
         /* If we've found the existing message number AND the author and
          * title match, then this is a replacement.
          */
         if( dwMsg != -1 && pth->thItem.dwActualMsg == dwMsg )
            if( lstrcmpi( pth->thItem.szAuthor, thItem.szAuthor ) == 0 &&
                lstrcmpi( pth->thItem.szTitle, thItem.szTitle ) == 0 )
               {
               thItem.dwMsg = pth->thItem.dwMsg;
               fFound = TRUE;
               }
         }
      if( !fFound )
         thItem.dwMsg = ptl->tlItem.dwMaxMsg + 1;
      }
   if( ( pth = CreateMsgIndirect( ptl, &thItem, &dwMsgPtr, &fExists ) ) == NULL )
      wError = AMDBERR_OUTOFMEMORY;
   else
      {
      char szFileName[ 13 ];
      HNDFILE fh;

      wError = WriteMsgTextBuffer( pth, lpszMsg, cbsz );
      if( wError == AMDBERR_NOERROR )
      {
         if( !fExists )
            {
            ++ptl->tlItem.cMsgs;
            ++ptl->pcl->clItem.cMsgs;
            ++ptl->pcl->pcat->catItem.cMsgs;
            }
         if( pth->thItem.dwFlags & MF_PRIORITY )
            {
            ++ptl->tlItem.msgc.cPriority;
            ++cTotalMsgc.cPriority;
            }
         if( !( pth->thItem.dwFlags & MF_READ ) )
            {
            ++ptl->tlItem.msgc.cUnRead;
            ++ptl->pcl->msgc.cUnRead;
            ++ptl->pcl->pcat->msgc.cUnRead;
            ++cTotalMsgc.cUnRead;
            if( pth->thItem.dwFlags & MF_PRIORITY )
               {
               ++ptl->tlItem.msgc.cUnReadPriority;
               ++cTotalMsgc.cUnReadPriority;
               }
            }
         if( ptl->tlItem.dwMinMsg == 0 )
            ptl->tlItem.dwMinMsg = thItem.dwMsg;
         if( ptl->tlItem.dwMaxMsg == 0 )
            ptl->tlItem.dwMaxMsg = thItem.dwMsg;
         if( thItem.dwMsg < ptl->tlItem.dwMinMsg )
            ptl->tlItem.dwMinMsg = thItem.dwMsg;
         if( thItem.dwMsg > ptl->tlItem.dwMaxMsg )
            ptl->tlItem.dwMaxMsg = thItem.dwMsg;
         if( ptl->tlItem.dwMaxMsg > ptl->tlItem.dwUpperMsg )
            ptl->tlItem.dwUpperMsg = ptl->tlItem.dwMaxMsg;
         ptl->tlItem.dwFlags |= TF_CHANGED | TF_BUILDTHREADLINES;
         ptl->tlItem.dwFlags &= ~TF_RESIGNED;

         if( ptl == ptlCachedThd )
            fh = fhCachedThd;
         else
         {
RetryCreate:
            wsprintf( szFileName, "%s.THD", (LPSTR)ptl->tlItem.szFileName );
            if( ( fh = Amdb_OpenFile( szFileName, AOF_WRITE ) ) == HNDFILE_ERROR )
            {
               char chTmpBuf[ 200 ];
               //ShowTHDFileError(1, ptl->tlItem.szFileName, ptl->tlItem.szTopicName);
               if(!Amdb_QueryFileExists( szFileName ))
               {
                  wsprintf( chTmpBuf, "Creating New topic THD file %s", (LPSTR)ptl->tlItem.szFileName);
                  Amevent_AddEventLogItem( ETYP_WARNING|ETYP_DATABASE, chTmpBuf);
                  fh = Amdb_CreateFile( szFileName, AOF_WRITE );
               }
               else
               {
                  DWORD dwCurTime;

                  if(retryCount > 5 ) 
                  {
                     wsprintf( chTmpBuf, "The thread file %s for topic %s cannot be opened. Click OK to create an empty one, or Cancel to abort adding this message",
                        (LPSTR)ptl->tlItem.szFileName, (LPSTR)ptl->tlItem.szTopicName );
                     if( MessageBox( GetFocus(), chTmpBuf, NULL, MB_OKCANCEL|MB_ICONEXCLAMATION ) == IDOK )
                     {
                        fh = Amdb_CreateFile( szFileName, AOF_WRITE );
                     }
                  }
                  else
                  {
                     dwCurTime = GetTickCount();
                     while( ( GetTickCount() - dwCurTime ) < 2000 );
                     retryCount++;
                     goto RetryCreate;
                  }
               }
            }
            if( fh != HNDFILE_ERROR )
               {
               if( fhCachedThd != HNDFILE_ERROR )
                  Amfile_Close( fhCachedThd );
               fhCachedThd = fh;
               ptlCachedThd = ptl;
               }
            }
         if( fh != HNDFILE_ERROR )
         {
            ASSERT( fhCachedThd != HNDFILE_ERROR );
            if( dwMsgPtr == 0L )
               pth->thItem.dwMsgPtr = (DWORD)Amfile_SetPosition( fhCachedThd, 0L, ASEEK_END );
            else {
               pth->thItem.dwMsgPtr = dwMsgPtr;
               Amfile_SetPosition( fhCachedThd, pth->thItem.dwMsgPtr, ASEEK_BEGINNING );
               }
            pth->thItem.dwActualMsg = dwMsg;
            if( Amfile_Write( fhCachedThd, (LPCSTR)&pth->thItem, sizeof( THITEM ) ) != sizeof( THITEM ) )
               wError = AMDBERR_OUTOFDISKSPACE;
         }
         else
            wError = AMDBERR_CANNOTCREATEFILE;
      }
   }
   if( wError == AMDBERR_NOERROR )
   {
      if( lppth )
         *lppth = pth;
      Amuser_CallRegistered( AE_NEWMSG, (LPARAM)pth, 0 );
   }
   Amdb_InternalUnlockTopic( ptl );
   fDataChanged = TRUE;
   return( wError );
}

/* Creates a new message structure in the linked list of messages for the
 * specified topic.
 */
PTH FASTCALL CreateMsgIndirect( PTL ptl, THITEM * pthItem, DWORD * lpOffset, BOOL * lpfExists )
{
   PTH pth;

   INITIALISE_PTR(pth);
   if( fNewSharedMemory( &pth, sizeof( TH ) ) )
      {
      PTH pth1;
      PTH pth2;
      PTH pth3;
      PTH pth4;
      PTH pth5;
      PTH pth6;
      BYTE wLevel;
      BOOL fPriority = FALSE;
      BOOL fIgnore = FALSE;
      BOOL fIsLinked = FALSE;
      BOOL fUnRead = FALSE;
      BOOL fWatched = FALSE;
      BOOL fReadLock = FALSE;
      WORD wComments;

      pth3 = pth4 = NULL;
      pth->wID = STYP_TH;
      pth->pthRefNext = NULL;
      pth->pthRefPrev = NULL;
      pth->wUnRead = 0;
      if( lpOffset )
         *lpOffset = 0L;
      if( lpfExists )
         *lpfExists = FALSE;
      if( pthItem->dwMsg == -1 )
         pthItem->dwMsg = ptl->tlItem.dwMaxMsg + 1;

      /* To improve performance, assume that the new message, whether or not it is a comment,
       * has a message number higher than the last one created.
       */
      if( ptl->pthCreateLast )
         {
         pth2 = ptl->pthCreateLast;
         if( pthItem->dwMsg < pth2->thItem.dwMsg )
            {
            while( pth2->pthPrev && pthItem->dwMsg < pth2->thItem.dwMsg )
               pth2 = pth2->pthPrev;
            }
         if( pthItem->dwComment )
            {
            while( pth2->pthPrev && pthItem->dwComment < pth2->thItem.dwMsg )
               pth2 = pth2->pthPrev;
            }
         }
      else
         pth2 = ptl->pthFirst;
      while( pth2 && pth2->thItem.dwMsg < pthItem->dwMsg )
         {
         if( pthItem->dwComment == pth2->thItem.dwMsg )
            pth4 = pth2;
         pth3 = pth2;
         pth2 = pth2->pthNext;
         }
      if( pth2 && pth2->thItem.dwMsg == pthItem->dwMsg )
         {
         /* There is an existing message with the same message number as
          * the one being replaced. Delete the old one.
          */
         if( lpOffset )
            *lpOffset = pth2->thItem.dwMsgPtr;
         FreeMemory( &pth );
         if( !( pth2->thItem.dwFlags & MF_READ ) )
            {
            --ptl->tlItem.msgc.cUnRead;
            if( pth2->pthRoot )
               --pth2->pthRoot->wUnRead;
            --ptl->pcl->msgc.cUnRead;
            --ptl->pcl->pcat->msgc.cUnRead;
            --cTotalMsgc.cUnRead;
            fUnRead = TRUE;
            }
         if( pth2->thItem.dwFlags & MF_MARKED )
            {
            --ptl->tlItem.msgc.cMarked;
            --cTotalMsgc.cMarked;
            }
         if( pth2->thItem.dwFlags & MF_TAGGED )
            {
            --ptl->tlItem.msgc.cTagged;
            --cTotalMsgc.cTagged;
            fUnRead = TRUE;
            }
         if( pth2->thItem.dwFlags & MF_PRIORITY )
            {
            --ptl->tlItem.msgc.cPriority;
            --cTotalMsgc.cPriority;
            if( !( pth2->thItem.dwFlags & MF_READ ) )
               {
               --ptl->tlItem.msgc.cUnReadPriority;
               --cTotalMsgc.cUnReadPriority;
               }
            }
         fReadLock = ( pth2->thItem.dwFlags & MF_READLOCK ) == MF_READLOCK;
         fIgnore = ( pth2->thItem.dwFlags & MF_IGNORE ) == MF_IGNORE;
         fWatched = ( pth2->thItem.dwFlags & MF_WATCH ) == MF_WATCH;
         pth = pth2;
         if( lpfExists )
            *lpfExists = TRUE;
         fIsLinked = TRUE;
         if( pth4 )
            {
            fIgnore = ( pth4->thItem.dwFlags & MF_IGNORE ) == MF_IGNORE;
            fPriority = ( pth4->thItem.dwFlags & MF_PRIORITY ) == MF_PRIORITY;
            if( !fWatched )
               fWatched = ( pth4->thItem.dwFlags & MF_WATCH ) == MF_WATCH;
            }
         }
      else 
         {
         if( !pth3 )
            ptl->pthFirst = pth;
         else
            pth3->pthNext = pth;
         if( !pth2 )
            ptl->pthLast = pth;
         else
            pth2->pthPrev = pth;
         pth->pthNext = pth2;
         pth->pthPrev = pth3;
         }

      /* The following code deals with adding a message to the chain of
       * references. This is fairly complex, so listen carefully. If the
       * message to which this message is a comment is in the message base,
       * then pth4 will point to that message. Walk all subsequent messages
       * from pth4 onwards (via the reference link) until we find a message
       * whose level is above ours. Insert our message just before it.
       */
      if( !fIsLinked ) {
         wLevel = 0;
         if( pth4 ) {
            PTH pth5;
   
            wLevel = pth4->wLevel + 1; /* Comments are one level down...*/
            ++pth4->wComments;
            fIgnore = ( pth4->thItem.dwFlags & MF_IGNORE ) == MF_IGNORE;
            fPriority = ( pth4->thItem.dwFlags & MF_PRIORITY ) == MF_PRIORITY;
            fWatched = ( pth4->thItem.dwFlags & MF_WATCH ) == MF_WATCH;
            pth5 = pth4->pthRefNext;
            while( pth5 && pth5->wLevel >= wLevel )
               {
               pth4 = pth5;
               pth5 = pth5->pthRefNext;
               }
            pth2 = pth5;
            pth3 = pth4;
            }
         else
            {
            PTH pth6;
            PTH pth7;

            pth7 = pth3 = ptl->pthRefLast;
            pth6 = pth2 = NULL;
            if( pth3 ) {
               if( !pth3->pthRefPrev ) {
                  if( pth3->thItem.dwMsg > pthItem->dwMsg )
                     {
                     pth6 = pth7;
                     pth7 = NULL;
                     }
                  }
               else while( pth3 /*pth3->pthRefPrev*/ )
                  {
                  while( pth3->pthRefPrev && pth3->wLevel ) {
                     pth2 = pth3;
                     pth3 = pth3->pthRefPrev;
                     }
                  if( pth3->wLevel == 0 )
                     if( pth3->thItem.dwMsg < pthItem->dwMsg )
                        break;
                  if( !pth3->pthRefPrev ) {
                     pth7 = NULL;
                     pth6 = ptl->pthRefFirst;
                     break;
                     }
                  pth3 = pth3->pthRefPrev;
                  pth7 = pth3;
                  pth6 = pth2;
                  }
               pth3 = pth7;
               pth2 = pth6;
               }
            }
         pth->wLevel = wLevel;
         pth->fExpanded = TRUE;
         if( pth3 ) {
            pth2 = pth3->pthRefNext;
            pth3->pthRefNext = pth;
            }
         if( pth2 ) {
            pth3 = pth2->pthRefPrev;
            pth2->pthRefPrev = pth;
            }
         if( pth2 == NULL )
            ptl->pthRefLast = pth;
         if( pth3 == NULL )
            ptl->pthRefFirst = pth;
         pth->pthRefNext = pth2;
         pth->pthRefPrev = pth3;
         }

      /* Scan from this message onwards to find a root message which
       * was previously orphaned, if any.
       */
      pth1 = pth2 = pth->pthRefNext;
      pth5 = pth6 = pth;
      wComments = 0;
      while( pth2 ) {
         if( pth2->thItem.dwComment == pthItem->dwMsg && pth2->wLevel == 0 ) {
            wLevel = pth->wLevel + 1;
            ++wComments;
            pth3 = pth2;
            do {
               pth2->wLevel += wLevel;
               pth4 = pth2;
               pth2 = pth2->pthRefNext;
               }
            while( pth2 && pth2->wLevel );
            if( pth6->pthRefNext != pth3 )
               {
               if( pth1 == pth3 )
                  pth1 = pth2;
               pth4->pthRefNext = pth1;
               if( pth1 )
                  pth1->pthRefPrev = pth4;
               pth6->pthRefNext = pth3;
               pth3->pthRefPrev = pth6;
               pth5->pthRefNext = pth2;
               if( pth2 )
                  pth2->pthRefPrev = pth5;
               else
                  ptl->pthRefLast = pth5;
               }
            pth6 = pth4;
            pth1 = pth6->pthRefNext;
            }
         else {
            pth5 = pth2;
            pth2 = pth2->pthRefNext;
            }
         }
      pth->thItem = *pthItem;
      pth->nLockCount = 0;
      _fmemset( &pth->dwThdMask, 0, MAX_MASKLEVEL * sizeof( DWORD ) );
      pth->cPriority = 0;
      pth->wComments = wComments;
      if( pth->wLevel && pth->pthRefPrev )
         {
         if( pth->pthRefPrev->wLevel == 0 )
            pth->pthRoot = pth->pthRefPrev;
         else
            pth->pthRoot = pth->pthRefPrev->pthRoot;
         }
      else
         {
         /* Fix: 16/2/95
          * For root messages backpatched, the pthRoot field for all
          * comments need to be patched to point to the newly inserted
          * root message.
          */
         pth->pthRoot = NULL;
         for( pth2 = pth->pthRefNext; pth2 && pth2->wLevel; pth2 = pth2->pthRefNext )
            pth2->pthRoot = pth;
         }
      if( pth->thItem.dwFlags & MF_PRIORITY )
         if( pth->pthRoot )
            ++pth->pthRoot->cPriority;
      if( fPriority )
         pth->thItem.dwFlags |= MF_PRIORITY;
      if( fWatched && !fIgnore )
         pth->thItem.dwFlags |= MF_WATCH;
      if( fReadLock )
         {
         pth->thItem.dwFlags |= MF_READLOCK;
         pth->thItem.dwFlags &= ~MF_READ;
         }
      if( pth->thItem.dwFlags & MF_READLOCK )
         fUnRead = fReadLock = TRUE;
      if( fIgnore )
         pth->thItem.dwFlags |= ( MF_IGNORE | MF_READ );
      if( ( ( ptl->tlItem.dwFlags & TF_IGNORED ) || ( lpfExists && *lpfExists && !fUnRead ) ) && !fReadLock )
         pth->thItem.dwFlags |= MF_READ;
      if( pth->pthRoot && !( pth->thItem.dwFlags & MF_READ ) )
         ++pth->pthRoot->wUnRead;
      pth->ptl = ptl;
      ptl->pthCreateLast = pth;
      }
   return( pth );
}

/* This function retrieves information about any attachment associated with
 * this message. There may be multiple attachments, in which case the pahFirst
 * handle is used to step through them all.
 */
PAH WINAPI EXPORT Amdb_GetMsgAttachment( PTH pth, PAH pahFirst, ATTACHMENT FAR * patch )
{
   if( NULL == pth )
      *patch = pahFirst->atItem;
   else
      {
      ASSERT( pth->ptl->wLockCount > 0 );
      if( NULL == pahFirst )
         {
         LoadAttachments( pth->ptl );
         pahFirst = pth->ptl->pahFirst;
         }
      else
         pahFirst = pahFirst->pahNext;
      while( pahFirst )
         {
         if( pahFirst->atItem.dwMsg == pth->thItem.dwMsg )
            {
            *patch = pahFirst->atItem;
            return( pahFirst );
            }
         pahFirst = pahFirst->pahNext;
         }
      }
   return( NULL );
}

/* This function deletes one attachment. If this is the last
 * attachment for this message, it returns TRUE.
 */
BOOL WINAPI EXPORT Amdb_DeleteAttachment( PTH pth, ATTACHMENT FAR * lpatItem, BOOL fDeleteAll )
{
   PAH pahPrev;
   PAH pah;

   ASSERT( pth->ptl->wLockCount > 0 );
   pah = pth->ptl->pahFirst;
   pahPrev = NULL;
   while( pah )
      {
      PAH pahNext;

      pahNext = pah->pahNext;
      if( _fmemcmp( lpatItem, &pah->atItem, sizeof( ATTACHMENT ) ) == 0 )
         {
         if( NULL == pahPrev )
            pth->ptl->pahFirst = pah->pahNext;
         else
            pahPrev->pahNext = pah->pahNext;
         FreeMemory( &pah );
         if( fDeleteAll )
            pth->thItem.dwFlags &= ~MF_ATTACHMENTS;
         pth->ptl->tlItem.dwFlags |= TF_ATTACHCHANGED;
         fDataChanged = TRUE;
         }
      else
         pahPrev = pah;
      pah = pahNext;
      }
   return( TRUE );
}

/* This function deletes one attachment. If this is the last
 * attachment for this message, it returns TRUE.
 */
BOOL WINAPI EXPORT Amdb_RenameAttachment( PTH pth, ATTACHMENT FAR * lpatItem, LPSTR lpszFilename )
{
   HNDFILE fh;
   PAH pahPrev;
   PAH pah;

   ASSERT( pth->ptl->wLockCount > 0 );
   pah = pth->ptl->pahFirst;
   pahPrev = NULL;
   while( pah )
      {
      PAH pahNext;

      pahNext = pah->pahNext;
      if( _fmemcmp( lpatItem, &pah->atItem, sizeof( ATTACHMENT ) ) == 0 )
         {
            lstrcpy( pah->atItem.szPath, lpszFilename );

            /* Mark the current message to show that it now has
             * an attachment.
             */   
            if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
               {
               if( !( pth->thItem.dwFlags & MF_ATTACHMENTS ) )
                  {
                  pth->thItem.dwFlags |= MF_ATTACHMENTS;
                  pth->ptl->tlItem.dwFlags |= TF_CHANGED;
                  }
               UnlockMessage( fh, pth );
               }
            pth->ptl->tlItem.dwFlags |= TF_ATTACHCHANGED;
            fDataChanged = TRUE;
         }
      else
         pahPrev = pah;
      pah = pahNext;
      }
   return( TRUE );
}

/* This function deletes any attachment records for the
 * specified message.
 */
BOOL WINAPI EXPORT Amdb_DeleteAllAttachments( PTH pth )
{
   PAH pahPrev;
   PAH pah;

   /* Scan to check firstly whether or not the attachment is already
    * in the list, and secondly for the position of the new attachment
    * considering that the list is ordered numerically.
    */
   ASSERT( pth->ptl->wLockCount > 0 );
   LoadAttachments( pth->ptl );
   pah = pth->ptl->pahFirst;
   pahPrev = NULL;
   while( pah )
      {
      if( pah->atItem.dwMsg == pth->thItem.dwMsg )
         {
         if( NULL == pahPrev )
            pth->ptl->pahFirst = pah->pahNext;
         else
            pahPrev->pahNext = pah->pahNext;
         FreeMemory( &pah );
         pth->thItem.dwFlags &= ~MF_ATTACHMENTS;
         pth->ptl->tlItem.dwFlags |= TF_ATTACHCHANGED;
         break;
         }
      else if( pah->atItem.dwMsg > pth->thItem.dwMsg )
         break;
      pahPrev = pah;
      pah = pah->pahNext;
      }
   return( TRUE );
}

/* This function adds the specified file to the list of
 * attachments for the specified message.
 */
BOOL WINAPI EXPORT Amdb_CreateAttachment( PTH pth, LPSTR lpszFilename )
{
   HNDFILE fh;
   PAH pahNew;
   PAH pahPrev;
   PAH pah;

   /* Scan to check firstly whether or not the attachment is already
    * in the list, and secondly for the position of the new attachment
    * considering that the list is ordered numerically.
    */
   ASSERT( pth->ptl->wLockCount > 0 );
   LoadAttachments( pth->ptl );
   pah = pth->ptl->pahFirst;
   pahPrev = NULL;
   while( pah )
      {
      if( pah->atItem.dwMsg == pth->thItem.dwMsg )
         {
         /* If message number and filename match, then
          * just exit.
          */
         if( lstrcmpi( pah->atItem.szPath, lpszFilename ) == 0 )
            return( TRUE );
         }
      else if( pah->atItem.dwMsg > pth->thItem.dwMsg )
         break;
      pahPrev = pah;
      pah = pah->pahNext;
      }

   /* Create a new attachment record and link it into
    * the list.
    */
   INITIALISE_PTR(pahNew);
   if( fNewSharedMemory( &pahNew, sizeof( AH ) ) )
      {
      if( NULL == pahPrev )
         pth->ptl->pahFirst = pahNew;
      else
         pahPrev->pahNext = pahNew;
      pahNew->pahNext = pah;
      pahNew->atItem.dwMsg = pth->thItem.dwMsg;
      lstrcpy( pahNew->atItem.szPath, lpszFilename );

      /* Mark the current message to show that it now has
       * an attachment.
       */   
      if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
         {
         if( !( pth->thItem.dwFlags & MF_ATTACHMENTS ) )
            {
            pth->thItem.dwFlags |= MF_ATTACHMENTS;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            }
         UnlockMessage( fh, pth );
         }
      pth->ptl->tlItem.dwFlags |= TF_ATTACHCHANGED;
      fDataChanged = TRUE;
      return( TRUE );
      }
   return( FALSE );
}

/* This function loads the list of attachments for all messages in
 * the specified topic if they are not already loaded.
 */
BOOL FASTCALL LoadAttachments( PTL ptl )
{
   if( NULL == ptl->pahFirst )
      {
      char szFileName[ 13 ];
      HNDFILE fh;

      wsprintf( szFileName, "%s.ATC", (LPSTR)ptl->tlItem.szFileName );
      if( ( fh = Amdb_OpenFile( szFileName, AOF_READ ) ) != HNDFILE_ERROR )
         {
         PAH pahLast;
         ATTACHMENT at;

         pahLast = NULL;
         while( Amfile_Read( fh, &at, sizeof( ATTACHMENT ) ) != 0 )
            {
            PAH pah;

            INITIALISE_PTR(pah);
            if( fNewSharedMemory( &pah, sizeof( AH ) ) )
               {
               if( NULL == ptl->pahFirst )
                  ptl->pahFirst = pah;
               else
                  pahLast->pahNext = pah;
               pah->pahNext = NULL;
               pah->atItem = at;
               pahLast = pah;
               }
            }
         Amfile_Close( fh );
         ptl->tlItem.dwFlags &= ~TF_ATTACHCHANGED;
         return( TRUE );
         }
      return( FALSE );
      }
   return( TRUE );
}

/* This function saves the list of attachments if this topic has any
 * and they have changed.
 */
BOOL FASTCALL SaveAttachments( PTL ptl )
{
   if( ptl->pahFirst && ( ptl->tlItem.dwFlags & TF_ATTACHCHANGED ) )
      {
      char szFileName[ 13 ];
      HNDFILE fh;

      wsprintf( szFileName, "%s.ATC", (LPSTR)ptl->tlItem.szFileName );
      if( ( fh = Amdb_CreateFile( szFileName, AOF_WRITE ) ) != HNDFILE_ERROR )
         {
         PAH pah;

         for( pah = ptl->pahFirst; pah; pah = pah->pahNext )
            Amfile_Write( fh, (LPCSTR)&pah->atItem, sizeof( ATTACHMENT ) );
         Amfile_Close( fh );
         ptl->tlItem.dwFlags &= ~TF_ATTACHCHANGED;
         return( TRUE );
         }
      }
   return( FALSE );
}

int WINAPI EXPORT Amdb_GetComments( PTH pth )
{
   ASSERT( pth->ptl->wLockCount > 0 );
   return( pth->wComments );
}

DWORD WINAPI EXPORT Amdb_GetMessageNumber( PTH pth )
{
   ASSERT( pth->ptl->wLockCount > 0 );
   return( pth->thItem.dwMsg );
}

BOOL WINAPI EXPORT Amdb_IsExpanded( PTH pth )
{
   ASSERT( pth->ptl->wLockCount > 0 );
   while( pth && pth->wLevel > 0 )
      pth = pth->pthRefPrev;
   return( pth ? pth->fExpanded : FALSE );
}

BOOL WINAPI EXPORT Amdb_IsRootMessage( PTH pth )
{
   ASSERT( pth->ptl->wLockCount > 0 );
   return( pth->wLevel == 0 );
}

void WINAPI EXPORT Amdb_ExpandMessage( PTH pth, BOOL fExpand )
{
   ASSERT( pth->ptl->wLockCount > 0 );
   pth->fExpanded = fExpand;
}

PTH WINAPI EXPORT Amdb_ExpandThread( PTH pth, BOOL fExpand )
{
   ASSERT( pth->ptl->wLockCount > 0 );
   while( pth && pth->wLevel > 0 )
      pth = pth->pthRefPrev;
   if( pth )
      {
      if( fExpand && pth->fExpanded )
         return( NULL );
      if( !fExpand && !pth->fExpanded )
         return( NULL );
      if( pth->wComments == 0 )
         return( NULL );
      pth->fExpanded = fExpand;
      }
   return( pth );
}

/* This function retrieves the handle of the specified message
 * given a message number.
 */
PTH WINAPI EXPORT Amdb_OpenMsg( PTL ptl, DWORD dwMsg )
{
   PTH pth;

   ASSERT( NULL != ptl );
   ASSERT( ptl->wLockCount > 0 );
   CommitTopicMessages( ptl, TRUE );
   if( dwMsg < ( ptl->tlItem.dwMaxMsg - ptl->tlItem.dwMinMsg ) / 2 )
      {
      /* dwMsg is closer to dwMinMsg than dwMaxMsg, so search from the
       * first message onwards.
       */
      for( pth = ptl->pthFirst; pth; pth = pth->pthNext )
         if( pth->thItem.dwMsg == dwMsg )
            break;
      }
   else
      {
      /* dwMsg is closer to dwMaxMsg than dwMinMsg, so search from the
       * last message backwards.
       */
      for( pth = ptl->pthLast; pth; pth = pth->pthPrev )
         if( pth->thItem.dwMsg == dwMsg )
            break;
      }
   return( pth );
}

/* This function retrieves the handle of the first message in the
 * specified topic.
 */
PTH WINAPI EXPORT Amdb_GetFirstMsg( PTL ptl, int nMode )
{
   PTH pth;

   ASSERT( NULL != ptl );
   CommitTopicMessages( ptl, TRUE );
   if( nMode == VM_VIEWREF || nMode == VM_VIEWREFEX || nMode == VM_VIEWROOTS )
      pth = ptl->pthRefFirst;
   else
      pth = ptl->pthFirst;
   return( pth );
}

/* This function retrieves the handle of the last message in topic ptl.
 * The message handle returned depends on the setting of nMode:
 */
PTH WINAPI EXPORT Amdb_GetLastMsg( PTL ptl, int nMode )
{
   PTH pth;

   ASSERT( NULL != ptl );
   CommitTopicMessages( ptl, TRUE );
   if( nMode == VM_VIEWREF || nMode == VM_VIEWREFEX || nMode == VM_VIEWROOTS )
      pth = ptl->pthRefLast;
   else
      pth = ptl->pthLast;
   return( pth );
}

/* This function retrieves the handle of the next message following pth,
 * The next message returned depends on the setting of nMode:
 */
PTH WINAPI EXPORT Amdb_GetNextMsg( PTH pth, int nMode )
{
   ASSERT( NULL != pth );
   switch( nMode )
      {
      case VM_VIEWREFEX:
         pth = pth->pthRefNext;
         break;

      case VM_VIEWROOTS:
      case VM_VIEWREF:
         if( pth->fExpanded == FALSE )
            {
            pth = pth->pthRefNext;
            while( pth && pth->wLevel )
               pth = pth->pthRefNext;
            }
         else
            pth = pth->pthRefNext;
         break;

      case VM_VIEWCHRON:
         pth = pth->pthNext;
         break;
      }
   return( pth );
}

/* This function retrieves the handle of the message that precedes pth.
 * The next message returned depends on the setting of nMode:
 */
PTH WINAPI EXPORT Amdb_GetPreviousMsg( PTH pth, int nMode )
{
   switch( nMode )
      {
      case VM_VIEWROOTS:
      case VM_VIEWREF:
      case VM_VIEWREFEX:
         if( pth )
            pth = pth->pthRefPrev;
         break;

      case VM_VIEWCHRON:
         if( pth )
            pth = pth->pthPrev;
         break;
      }
   return( pth );
}

/* This function retrieves the handle of the nearest visible
 * message after pth IF pth is not a root message AND we're
 * in roots mode.
 */
PTH WINAPI EXPORT Amdb_GetNearestMsg( PTH pth, int nMode )
{
   ASSERT( pth->ptl->wLockCount > 0 );
   if( nMode == VM_VIEWROOTS )
      {
      PTH pthv2;

      pthv2 = pth;
      while( pth && pth->wLevel )
         pth = pth->pthRefPrev;
      if( pth == NULL )
         for( pth = pthv2; pth && pth->wLevel; )
            pth = pth->pthRefNext;
      }
   return( pth );
}

PTH WINAPI EXPORT Amdb_GetPriorityUnRead( PTL ptlv, PTH pth, int nMode, BOOL fFwd )
{
   PTL ptl = ptlv;
   PTH pthStart = pth;
   PTL ptlStart;
   PCAT pcat;
   PCL pcl;

   ptlStart = ptl;
   pcl = ptl->pcl;
   pcat = pcl->pcat;
   if( fFwd ) {
      do {
         if( pth == NULL || ( pth = ( nMode == VM_VIEWCHRON ) ? pth->pthNext : pth->pthRefNext ) == NULL )
            {
            if( ptl != ptlStart )
               {
               ASSERT( ptl->wLockCount > 0 );
               Amdb_UnlockTopic( ptl );
               Amdb_DiscardTopic( ptl );
               }
            do {
               if( ( ptl = ptl->ptlNext ) == NULL )
                  {
                  do {
                     if( ( pcl = pcl->pclNext ) == NULL )
                        {
                        do {
                           if(( ( pcat = pcat->pcatNext ) == NULL ) || ( NULL == pcat->pclFirst )  )
                              pcat = pcatFirst;
                           pcl = pcat->pclFirst;
                           }
                        while( pcl == NULL );
                        }
                     ptl = pcl->ptlFirst;
                     }
                  while( ptl == NULL );
                  }
               if( ptl == ptlStart )
                  break;
               }
            while( ptl->tlItem.msgc.cUnReadPriority == 0 );
            if( ptl != ptlStart )
               Amdb_LockTopic( ptl );
            CommitTopicMessages( ptl, TRUE );
            if( cTotalMsgc.cUnReadPriority == 0 )
               return( NULL );
            pth = ( nMode == VM_VIEWCHRON ) ? ptl->pthFirst : ptl->pthRefFirst;
            if( pth == NULL )
               return( NULL );
            }
         }
      while( pth != pthStart && (!(pth->thItem.dwFlags & MF_PRIORITY) || (pth->thItem.dwFlags & MF_READ)) );
      }
   else {
      do {
         if( pth == NULL || ( pth = ( nMode == VM_VIEWCHRON ) ? pth->pthPrev : pth->pthRefPrev ) == NULL )
            {
            if( ptl != ptlStart )
               {
               ASSERT( ptl->wLockCount > 0 );
               Amdb_UnlockTopic( ptl );
               Amdb_DiscardTopic( ptl );
               }
            do {
               if( ( ptl = ptl->ptlPrev ) == NULL )
                  {
                  do {
                     if( ( pcl = pcl->pclPrev ) == NULL )
                        {
                        do {
                           if(( ( pcat = pcat->pcatPrev ) == NULL ) || ( NULL == pcat->pclLast )  )
                              pcat = pcatLast;
                           pcl = pcat->pclLast;
                           }
                        while( pcl == NULL );
                        }
                     ptl = pcl->ptlLast;
                     }
                  while( ptl == NULL );
                  }
               if( ptl == ptlStart )
                  break;
               }
            while( ptl->tlItem.msgc.cUnReadPriority == 0 );
            if( ptl != ptlStart )
               Amdb_LockTopic( ptl );
            CommitTopicMessages( ptl, TRUE );
            if( cTotalMsgc.cUnReadPriority == 0 )
               return( NULL );
            pth = ( nMode == VM_VIEWCHRON ) ? ptl->pthLast : ptl->pthRefLast;
            if( pth == NULL )
               return( NULL );
            }
         }
      while( pth != pthStart && (!(pth->thItem.dwFlags & MF_PRIORITY) || (pth->thItem.dwFlags & MF_READ)) );
      }
   if( pth == pthStart )
      pth = NULL;
   return( pth );
}

PTH WINAPI EXPORT Amdb_GetMarked( PTL ptlv, PTH pth, int nMode, BOOL fFwd )
{
   PTH pthStart = pth;
   PTL ptl = ptlv;
   PTL ptlStart;
   PCAT pcat;
   PCL pcl;

   ptlStart = ptl;
   pcl = ptl->pcl;
   pcat = pcl->pcat;
   if( fFwd ) {
      do {
         if( pth == NULL || ( pth = ( nMode == VM_VIEWCHRON ) ? pth->pthNext : pth->pthRefNext ) == NULL )
            {
            if( ptl != ptlStart )
               {
               ASSERT( ptl->wLockCount > 0 );
               Amdb_UnlockTopic( ptl );
               Amdb_DiscardTopic( ptl );
               }
            do {
               if( ( ptl = ptl->ptlNext ) == NULL )
                  {
                  do {
                     if( ( pcl = pcl->pclNext ) == NULL )
                        {
                        do {
                           if(( ( pcat = pcat->pcatNext ) == NULL ) || ( NULL == pcat->pclFirst )  )
                              pcat = pcatFirst;
                           pcl = pcat->pclFirst;
                           }
                        while( pcl == NULL );
                        }
                     ptl = pcl->ptlFirst;
                     }
                  while( ptl == NULL );
                  }
               if( ptl == ptlStart )
                  break;
               }
            while( ptl->tlItem.msgc.cMarked == 0 );
            if( ptl != ptlStart )
               Amdb_LockTopic( ptl );
            CommitTopicMessages( ptl, TRUE );
            if( cTotalMsgc.cMarked == 0 )
               return( NULL );
            pth = ( nMode == VM_VIEWCHRON ) ? ptl->pthFirst : ptl->pthRefFirst;
            if( pth == NULL )
               return( NULL );
            }
         }
      while( pth != pthStart && !( pth->thItem.dwFlags & MF_MARKED ) );
      }
   else {
      do {
         if( pth == NULL || ( pth = ( nMode == VM_VIEWCHRON ) ? pth->pthPrev : pth->pthRefPrev ) == NULL )
            {
            if( ptl != ptlStart )
               {
               ASSERT( ptl->wLockCount > 0 );
               Amdb_UnlockTopic( ptl );
               Amdb_DiscardTopic( ptl );
               }
            do {
               if( ( ptl = ptl->ptlPrev ) == NULL )
                  {
                  do {
                     if( ( pcl = pcl->pclPrev ) == NULL )
                        {
                        do {
                           if(( ( pcat = pcat->pcatPrev ) == NULL ) || ( NULL == pcat->pclLast )  )
                              pcat = pcatLast;
                           pcl = pcat->pclLast;
                           }
                        while( pcl == NULL );
                        }
                     ptl = pcl->ptlLast;
                     }
                  while( ptl == NULL );
                  }
               if( ptl == ptlStart )
                  break;
               }
            while( ptl->tlItem.msgc.cMarked == 0 );
            if( ptl != ptlStart )
               Amdb_LockTopic( ptl );
            CommitTopicMessages( ptl, TRUE );
            if( cTotalMsgc.cMarked == 0 )
               return( NULL );
            pth = ( nMode == VM_VIEWCHRON ) ? ptl->pthLast : ptl->pthRefLast;
            if( pth == NULL )
               return( NULL );
            }
         }
      while( pth != pthStart && !( pth->thItem.dwFlags & MF_MARKED ) );
      }
   return( pth );
}

/* Returns the handle of the next tagged message in the topic.
 * If pth is NULL, it returns the handle of the first tagged
 * message. Otherwise it returns the handle of the next tagged
 * message AFTER pth. It returns NULL if there are no further
 * tagged messages.
 */
PTH WINAPI EXPORT Amdb_GetTagged( PTL ptl, PTH pth )
{
   if( cTotalMsgc.cTagged == 0 )
      return( NULL );
   if( 0 == ptl->tlItem.msgc.cTagged )
      return( NULL );
   if( NULL == pth )
      {
      CommitTopicMessages( ptl, TRUE );
      pth = ptl->pthFirst;
      }
   else
      pth = pth->pthNext;
   if( cTotalMsgc.cTagged == 0 )
      return( NULL );
   while( pth )
      {
      if( pth->thItem.dwFlags & MF_TAGGED )
         break;
      pth = pth->pthNext;
      }
   return( pth );
}

PTH WINAPI EXPORT Amdb_GetRootMsg( PTH pth, BOOL fPrev )
{
   ASSERT( pth ? pth->ptl->wLockCount > 0 : TRUE );
   if( fPrev )
      while( pth && pth->wLevel )
         pth = pth->pthRefPrev;
   else
      while( pth && pth->wLevel != 0 )
         pth = pth->pthRefNext;
   return( pth );
}

PTH WINAPI EXPORT Amdb_ExGetRootMsg( PTH pth, BOOL fPrev, int nMode )
{
   ASSERT( pth ? pth->ptl->wLockCount > 0 : TRUE );
   if( fPrev ) {
      while( pth && pth->wLevel )
         pth = pth->pthRefPrev;
      }
   else {
      while( pth && pth->wLevel != 0 )
         pth = pth->pthRefNext;
      }
   return( pth );
}

/* This function scans the messagebase for the next unread message
 */
BOOL WINAPI EXPORT Amdb_GetNextUnReadMsg( CURMSG FAR * lpunr, int nMode )
{
   PTH pthStart;
   BOOL fFirst;
   PTL ptlOriginal;
   PTH pth = NULL;
   PTL ptl = NULL;
   PCL pcl = NULL;
   PCAT pcat = NULL;

   /* Return now if no unread messages at all.
    */
   if( cTotalMsgc.cUnRead == 0 )
      return( FALSE );

   /* If a starting point is provided, use that.
    */
   if( lpunr )
      {
      pth = lpunr->pth;
      ptl = lpunr->ptl;
      if( ptl && ptl->tlItem.msgc.cUnRead )
         CommitTopicMessages( ptl, TRUE );
      pcl = lpunr->pcl;
      pcat = lpunr->pcat;
      }

   /* If no starting point, work one out.
    */
   if( !pcat )
      if( !( pcat = pcatFirst ) )
         return( FALSE );
   if( !pcl )
      {
      while( !( pcl = pcat->pclFirst ) )
         {
         if( !( pcat = pcat->pcatNext ) )
            return( FALSE );
         }
      if( NULL == pcl )
         return( FALSE );
      }
   if( !ptl)
      {
      while( !( ptl = pcl->ptlFirst ) )
         {
         pcl = pcl->pclNext;
         while( !pcl )
            {
            if( !( pcat = pcat->pcatNext ) )
               return( FALSE );
            pcl = pcat->pclFirst;
            }
         }
      if( NULL == ptl )
         return( FALSE );
      if( ptl->tlItem.msgc.cUnRead )
         CommitTopicMessages( ptl, TRUE );
      if( cTotalMsgc.cUnRead == 0 )
         return( FALSE );
      }

   /* Lock the starting topic.
    */
   Amdb_LockTopic( ptl );
   ptlOriginal = ptl;

   /* If VM_USE_ORIGINAL, then we use the viewing mode
    * for the starting topic.
    */
   if( VM_USE_ORIGINAL == nMode )
      nMode = ptl->tlItem.vd.nViewMode;

   /* If no starting message, start from the beginning of
    * the topic. Otherwise start from the message AFTER the
    * starting message.
    */
   if( !pth ) {
      if( nMode == VM_VIEWREF || nMode == VM_VIEWREFEX )
         pth = ptl->pthRefFirst;
      else
         pth = ptl->pthFirst;
      }
   else {
      if( nMode == VM_VIEWREF || nMode == VM_VIEWREFEX || nMode == VM_VIEWROOTS )
         pth = pth->pthRefNext;
      else
         pth = pth->pthNext;
      }

   /* Remember the starting message.
    */
   pthStart = pth;
   fFirst = TRUE;

   /* Now loop until we've scanned the entire messagebase OR
    * we've found an unread message.
    */
   do {
      /* Last message in the topic?
       */
      if( !pth )
         {
         PTL ptlStart;

         /* Try the next topic in the conference.
          */
         ptlStart = ptl;
         Amdb_InternalUnlockTopic( ptl );
TryNext: ptl = ptl->ptlNext;

         /* If we've reached the end of the
          * conference, advance to the next
          */
         if( NULL == ptl )
            {
            PCL pclStart = pcl;

            /* Find the next topic with unread
             * messages.
             */
            do {
               pcl = pcl->pclNext;
               if( !pcl )
                  {
                  do {
                     pcat = pcat->pcatNext;
                     if( !pcat )
                        {
                        pcat = pcatFirst;
                        if( !pcat )
                           break;
                        }
                     pcl = pcat->pclFirst;
                     }
                  while( pcl == NULL );
                  if( !pcat )
                     break;
                  }
               ptl = pcl->ptlFirst;
               }
            while( ptl == NULL && pcl != pclStart );
            }

         /* Check the total count again in case an auto-repair
          * of the topic adjusted this.
          */
         if( cTotalMsgc.cUnRead == 0 )
            return( FALSE );
         if( NULL == ptl )
            return( FALSE );

         /* Skip this topic if it has no unread messages
          */
         if( ptl->tlItem.msgc.cUnRead == 0 )
            {
            if( ptlOriginal == ptl )
               return( FALSE );
            goto TryNext;
            }

         /* Page in this topic and check the topic cUnRead
          * count again in case an auto-repair adjusted this.
          * Also handle the rare case where the global unread
          * count went to zero and exit now.
          */
         CommitTopicMessages( ptl, TRUE );
         if( ptl->tlItem.msgc.cUnRead == 0 )
            {
            if( ptlOriginal == ptl )
               return( FALSE );
            goto TryNext;
            }
         if( cTotalMsgc.cUnRead == 0 )
            {
            Amdb_InternalUnlockTopic( ptl );
            return( FALSE );
            }

         /* If we get here, the topic genuinely has unread
          * messages. Now scan for them starting from the
          * first message.
          */
         Amdb_LockTopic( ptl );
         nMode = ptl->tlItem.vd.nViewMode;
         if( nMode == VM_VIEWREF || nMode == VM_VIEWREFEX )
            pth = ptl->pthRefFirst;
         else
            pth = ptl->pthFirst;
         }

      /* If we get a NULL message handle here, then we're
       * finished.
       */
      if( !pth )
         {
         Amdb_InternalUnlockTopic( ptl );
         return( FALSE );
         }

      /* Found an unread message? Exit now if so.
       */
      if( !( pth->thItem.dwFlags & MF_READ ) )
         break;
      if( !fFirst && pth == pthStart )
         break;

      /* Advance to the next message
       */
      if( nMode == VM_VIEWREF || nMode == VM_VIEWREFEX || nMode == VM_VIEWROOTS )
         pth = pth->pthRefNext;
      else
         pth = pth->pthNext;
      fFirst = FALSE;
      }
   while( TRUE );

   /* If a starting point structure was provided, update
    * it with the handles of the unread message and the topic,
    * conference and category.
    */
   if( lpunr ) {
      lpunr->pth = pth;
      lpunr->ptl = ptl;
      lpunr->pcl = pcl;
      lpunr->pcat = pcat;
      }
   return( TRUE );
}

static void FASTCALL DeletePurgeMsg( PTH pth )
{
   ASSERT( pth->ptl->wLockCount > 0 );
   pth->thItem.dwSize = 0;
   pth->thItem.dwFlags &= ~MF_DELETED;
   pth->thItem.dwFlags |= MF_PURGED|MF_HDRONLY;
   lstrcpy( pth->thItem.szTitle, GS(IDS_STR520) );
}

void WINAPI EXPORT Amdb_DeleteMsg( PTH pth, BOOL fUpdate )
{
   BOOL fMsgWasUnread;
   PTL ptl;

   fMsgWasUnread = FALSE;
   ASSERT( pth->ptl->wLockCount > 0 );
   ptl = pth->ptl;
   fDataChanged = TRUE;
   if( !( pth->thItem.dwFlags & MF_READ ) )
      fMsgWasUnread = TRUE;
   InternalDeleteMsg( pth, TRUE, fUpdate );
   if( fMsgWasUnread )
      Amuser_CallRegistered( AE_UNREADCHANGED, (LPARAM)ptl, 0 );
}

/* Delete a message from a topic. This is complicated but very basic code
 * that has two purposes: remove the message from every linked list and other
 * cached pointer references, and reduce the internal 'counts' depending on
 * the message attributes.
 */
void FASTCALL InternalDeleteMsg( PTH pth, BOOL fDiskDel, BOOL fUpdate )
{
   PTH pth2;

   /* If this message was a comment, then find the original message and decrement
    * the number of comments to that message.
    */
   pth2 = pth;
   ASSERT( pth->ptl->wLockCount > 0 );
   Amuser_CallRegistered( AE_DELETEMSG, (LPARAM)pth, fUpdate );
   if( pth2->wLevel > 0 )
      {
      pth2 = pth2->pthRefPrev;
      while( pth2 && pth2->wLevel == pth->wLevel )
         pth2 = pth2->pthRefPrev;
      if( pth2 )
         --pth2->wComments;
      }

   /* If this message has any comments, then forcibly orphan them, then move all of them to
    * just before the next root message.
    */
   if( pth->wComments )
      {
      PTH pth3;
      PTH pth4;
      PTH pth5;

      pth2 = pth4 = pth->pthRefNext;
      pth5 = NULL;
      while( pth2 && pth2->wLevel > pth->wLevel )
         {
         pth2->wLevel = ( pth2->wLevel - pth->wLevel ) - 1;
         if( pth2->pthRoot == pth )
            pth2->pthRoot = NULL;
         pth5 = pth2;
         pth2 = pth2->pthRefNext;
         }
      if( pth->wLevel > 0 )
         if( pth2 && pth5 ) {
            if( pth2->wLevel )
               {
               pth3 = pth2;
               while( ( pth2->pthRefNext ) && pth2->pthRefNext->wLevel )
                  pth2 = pth2->pthRefNext;
               pth5->pthRefNext = pth2->pthRefNext;
               pth->pthRefNext = pth3;
               pth3->pthRefPrev = pth;
               if( pth2->pthRefNext )
                  pth2->pthRefNext->pthRefPrev = pth5;
               else
                  pth->ptl->pthRefLast = pth2;
               pth2->pthRefNext = pth4;
               pth4->pthRefPrev = pth2;
               }
            }
      }

   /* Adjust the minimum and maximum message number in the topic.
    */
   if( pth->thItem.dwMsg <= pth->ptl->tlItem.dwMinMsg )
      if( pth->pthNext )
         pth->ptl->tlItem.dwMinMsg = pth->pthNext->thItem.dwMsg;
   if( pth->thItem.dwMsg >= pth->ptl->tlItem.dwMaxMsg )
      if( pth->pthPrev )
         pth->ptl->tlItem.dwMaxMsg = pth->pthPrev->thItem.dwMsg;

   /* If there are no more messages in the topic, zero the maximum
    * and minimum message numbers.
    */
   if( NULL == pth->pthPrev && NULL == pth->pthNext )
      {
      pth->ptl->tlItem.dwMaxMsg = 0;
      pth->ptl->tlItem.dwMinMsg = 0;
      }

   /* Unlink from both the normal and reference linked list.
    */
   if( !pth->pthPrev )
      pth->ptl->pthFirst = pth->pthNext;
   else
      pth->pthPrev->pthNext = pth->pthNext;
   if( pth->pthNext )
      pth->pthNext->pthPrev = pth->pthPrev;
   else
      pth->ptl->pthLast = pth->pthPrev;
   if( pth->pthRefPrev )
      pth->pthRefPrev->pthRefNext = pth->pthRefNext;
   else
      pth->ptl->pthRefFirst = pth->pthRefNext;
   if( pth->pthRefNext )
      pth->pthRefNext->pthRefPrev = pth->pthRefPrev;
   else
      pth->ptl->pthRefLast = pth->pthRefPrev;

   /* If this was the last message in the thread, zero the
    * last created pointer.
    */
   if( pth == pth->ptl->pthCreateLast )
      pth->ptl->pthCreateLast = NULL;

   /* This message is unread, so reduce the count of unread
    * messages in the global, topic, folder and database. Also reduce
    * the count of unread messages in the thread if this is
    * a comment.
    */
   if( !( pth->thItem.dwFlags & MF_READ ) )
      {
      --pth->ptl->tlItem.msgc.cUnRead;
      --pth->ptl->pcl->msgc.cUnRead;
      --pth->ptl->pcl->pcat->msgc.cUnRead;
      if( pth->pthRoot )
         --pth->pthRoot->wUnRead;
      --cTotalMsgc.cUnRead;
      }

   /* This message is marked, so reduce the count of
    * marked messages in the topic and the global count
    * of marked messages.
    */
   if( pth->thItem.dwFlags & MF_MARKED )
      {
      --pth->ptl->tlItem.msgc.cMarked;
      --cTotalMsgc.cMarked;
      }

   /* This message is tagged, so reduce the count of
    * tagged messages in the topic and the global count
    * of tagged messages.
    */
   if( pth->thItem.dwFlags & MF_TAGGED )
      {
      --pth->ptl->tlItem.msgc.cTagged;
      --cTotalMsgc.cTagged;
      }

   /* This message is priority, so reduce the count of
    * priority messages in the topic and the global count
    * of priority messages. Also, if this is not a root message,
    * reduce the count of priority messages in the thread.
    */
   if( pth->thItem.dwFlags & MF_PRIORITY )
      {
      --pth->ptl->tlItem.msgc.cPriority;
      --cTotalMsgc.cPriority;
      if( pth->pthRoot )
         --pth->pthRoot->cPriority;

      /* If this message is also unread, reduce the count
       * of unread priority messages.
       */
      if( !( pth->thItem.dwFlags & MF_READ ) )
         {
         --pth->ptl->tlItem.msgc.cUnReadPriority;
         --cTotalMsgc.cUnReadPriority;
         }
      }

   /* Reduce the count of entire messages in the topic, folder
    * and database.
    */
   --pth->ptl->tlItem.cMsgs;
   --pth->ptl->pcl->clItem.cMsgs;
   --pth->ptl->pcl->pcat->catItem.cMsgs;

   /* This topic has now changed. May also need to rebuild the
    * thread lines.
    */
   pth->ptl->tlItem.dwFlags |= TF_BUILDTHREADLINES | TF_CHANGED;

   /* If we're deleting the message from disk, mark it deleted in
    * the data file. Purge and compact will get rid of it permanently.
    */   
   if( fDiskDel )
      {
      HNDFILE fh;

      if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
         {
         pth->thItem.dwFlags |= MF_DESTROYED;
         UnlockMessage( fh, pth );
         }
      }

   /* If this message had an attachment, delete them.
    */
   if( pth->thItem.dwFlags & MF_ATTACHMENTS )
      Amdb_DeleteAllAttachments( pth );

   /* Finally, free up memory used by the message.
    */
   FreeMemory( &pth );
}

PTH WINAPI EXPORT Amdb_GetMsg( PTH pth, DWORD dwMsg )
{
   ASSERT( pth->ptl->wLockCount > 0 );
   if( pth->thItem.dwMsg < dwMsg )
      while( pth && pth->thItem.dwMsg != dwMsg )
         pth = pth->pthNext;
   else
      while( pth && pth->thItem.dwMsg != dwMsg )
         pth = pth->pthPrev;
   return( pth );
}

/* This function returns the message number.
 */
DWORD WINAPI EXPORT Amdb_GetMsgNumber( PTH pth )
{
   ASSERT( pth != NULL );
   ASSERT( pth->ptl->wLockCount > 0 );
   return( pth->thItem.dwMsg );
}

void WINAPI EXPORT Amdb_GetMsgInfo( PTH pth, MSGINFO FAR * pMsgInfo )
{
   if( pth != NULL )
      {
      ASSERT( pth->ptl->wLockCount > 0 );
      pMsgInfo->dwMsg = pth->thItem.dwMsg;
      pMsgInfo->dwComment = pth->thItem.dwComment;
      pMsgInfo->dwFlags = pth->thItem.dwFlags;
      pMsgInfo->wDate = pth->thItem.wDate;
      pMsgInfo->wTime = pth->thItem.wTime;
      pMsgInfo->cUnRead = pth->ptl->tlItem.msgc.cUnRead;
      pMsgInfo->wLevel = pth->wLevel;
      pMsgInfo->cPriority = pth->cPriority;
      pMsgInfo->wComments = pth->wComments;
      pMsgInfo->cUnReadInThread = pth->wUnRead;
      pMsgInfo->dwActual = pth->thItem.dwActualMsg;
      pMsgInfo->dwSize = max( pth->thItem.dwRealSize, pth->thItem.dwSize );
      lstrcpy( pMsgInfo->szTitle, pth->thItem.szTitle );
      lstrcpy( pMsgInfo->szAuthor, pth->thItem.szAuthor );
      lstrcpyn( pMsgInfo->szReply, pth->thItem.szMessageId, 79 );
      }
}

BOOL WINAPI EXPORT Amdb_ReplaceMsgText( PTH pth, HPSTR hpTextBuf, DWORD dwMsgLength )
{
   HNDFILE fh;

   if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
      {
      if( WriteMsgTextBuffer( pth, hpTextBuf, dwMsgLength ) == AMDBERR_NOERROR )
         {
         UnlockMessage( fh, pth );
         return( TRUE );
         }
      UnlockMessage( fh, pth );
      }
   return( FALSE );
}

int FASTCALL WriteMsgTextBuffer( PTH pth, HPSTR hpTextBuf, DWORD dwMsgLength )
{
   char szFileName[ _MAX_PATH ];
   DWORD dwRetries;
   DWORD dwMaxRetries;
   int wError;
   HNDFILE fh;
   PTL ptl;

   ptl = pth->ptl;
   dwMaxRetries = Amuser_GetPPLong( szSettings, "FileRetries", 5 );
   dwRetries = 0;
Retry:


   wError = AMDBERR_NOERROR;
   if( ptl == ptlCachedTxt )
      {
      fh = fhCachedTxt;
      Amfile_SetPosition( fh, 0L, ASEEK_END );
      }
   else {
      wsprintf( szFileName, "%s.TXT", (LPSTR)ptl->tlItem.szFileName );
      if( Amdb_QueryFileExists( szFileName ) )
      {
         struct _stat st;
         if( _stat( szFileName, &st ) != -1 && ((unsigned long)st.st_size) > SIZE_THRESHOLD )
            return AMDBERR_OUTOFDISKSPACE;

         if( ( fh = Amdb_OpenFile( szFileName, AOF_SHARE_READWRITE ) ) != HNDFILE_ERROR )
            Amfile_SetPosition( fh, 0L, ASEEK_END );
      }
      else
         {
         char sz[ 80 ];

         /* Write unique info to the header so that we can recover it
          * when we rebuild the database.
          */
         fh = Amdb_CreateFile( szFileName, AOF_SHARE_WRITE );
         if( HNDFILE_ERROR != fh )
            {
            wsprintf( sz, "!TX %s %s %s\r\n", ptl->pcl->clItem.szFolderName, ptl->tlItem.szTopicName, szFileName );
            Amfile_Write( fh, sz, strlen( sz ) );
            }
         }
      if( fh != HNDFILE_ERROR && wError == AMDBERR_NOERROR )
         {
         if( fhCachedTxt != HNDFILE_ERROR )
            Amfile_Close( fhCachedTxt );
         fhCachedTxt = fh;
         ptlCachedTxt = ptl;
         }
      }
   if( fh == HNDFILE_ERROR )
      wError = AMDBERR_CANNOTCREATEFILE;
   if( wError == AMDBERR_NOERROR )
   {
      pth->thItem.dwSize = dwMsgLength;
      pth->thItem.dwOffset = (DWORD)Amfile_SetPosition( fh, 0L, ASEEK_CURRENT );
      if( (DWORD)Amfile_Write32( fh, hpTextBuf, dwMsgLength ) != dwMsgLength )
         {
         pth->thItem.dwSize = 0L;
         pth->thItem.dwOffset = 0L;
         wError = AMDBERR_OUTOFDISKSPACE;
         }
   }
   else if( wError == AMDBERR_CANNOTCREATEFILE && ( dwRetries < dwMaxRetries ) )
   {
      DWORD dwCurTime;
      char sz[ 128 ];

      /* Can't open the file, may be in use by another
       * process, so wait 2 seconds and try again.
       */
      wsprintf( sz, GS(IDS_STR531), Amdb_GetFolderName( ptl->pcl ), Amdb_GetTopicName( ptl ) );
      Amevent_AddEventLogItem( ETYP_WARNING|ETYP_DATABASE, sz );
      dwCurTime = GetTickCount();
      while( ( GetTickCount() - dwCurTime ) < 2000 );
      dwRetries++;
      goto Retry;
   }
   return( wError );
}

/* This function retreives the text of the specified message. It loads the
 * text into a globally allocated memory buffer and returns a pointer to
 * the buffer. It is the responsibility of the caller to free the memory
 * buffer when it is no longer required.
 */
HPSTR WINAPI EXPORT Amdb_GetMsgText( PTH pth )
{
   char szFileName[ 13 ];
   HPSTR hpText;
   HNDFILE fh;
   DWORD dwRetries = 0;
   DWORD dwMaxRetries;

   ASSERT( pth->ptl->wLockCount > 0 );
   INITIALISE_PTR(hpText);
   dwMaxRetries = Amuser_GetPPLong( szSettings, "GetMsgTextRetries", 5 );
   if( pth->thItem.dwSize )
      {
   Retry:
      if( pth->ptl == ptlCachedTxt )
         fh = fhCachedTxt;
      else
         {
         wsprintf( szFileName, "%s.TXT", (LPSTR)pth->ptl->tlItem.szFileName );
         if( ( fh = Amdb_OpenFile( szFileName, AOF_READWRITE ) ) != HNDFILE_ERROR )
            {
            if( fhCachedTxt != HNDFILE_ERROR )
               Amfile_Close( fhCachedTxt );
            ptlCachedTxt = pth->ptl;
            fhCachedTxt = fh;
            }
         }
      if( fh == HNDFILE_ERROR && ( dwRetries < dwMaxRetries )  )
         {
         DWORD dwCurTime;
         char sz[ 128 ];

         /* Can't open the file, may be in use by another
          * process, so wait 2 seconds and try again.
          */
         wsprintf( sz, GS(IDS_STR531), Amdb_GetFolderName( pth->ptl->pcl ), Amdb_GetTopicName( pth->ptl ) );
         Amevent_AddEventLogItem( ETYP_WARNING|ETYP_DATABASE, sz );
         dwCurTime = GetTickCount();
         while( ( GetTickCount() - dwCurTime ) < 2000 );
         dwRetries++;
         goto Retry;
         }
      else if ( dwRetries >= dwMaxRetries )
      {
      int cbErrorMsg;
      char * npErrorMsg;

      npErrorMsg = GS(IDS_STR55);
      cbErrorMsg = strlen( npErrorMsg );
      if( fNewSharedMemory32( &hpText, cbErrorMsg + 3 ) )
         {
         lstrcpy( hpText, npErrorMsg );
         lstrcat( hpText, "\r\n" );
         }
      }
      else
         {
         DWORD dwSize;

         if( Amfile_SetPosition( fh, pth->thItem.dwOffset, ASEEK_BEGINNING ) != -1 )
            {
            dwSize = pth->thItem.dwSize;
            if( fNewSharedMemory32( &hpText, dwSize + 1 ) )
               {
               if( (DWORD)Amfile_Read32( fh, hpText, dwSize ) != dwSize )
                  {
                  FreeMemory32( &hpText );
                  hpText = NULL;
                  }
               else
                  hpText[ pth->thItem.dwSize ] = '\0';
               }
            }
         }
      }
   else if( pth->thItem.dwFlags & MF_PURGED )
      {
      int cbPurgeMsg;
      char * npPurgeMsg;

      npPurgeMsg = GS(IDS_STR520);
      cbPurgeMsg = strlen( npPurgeMsg );
      if( fNewSharedMemory32( &hpText, cbPurgeMsg + 3 ) )
         {
         lstrcpy( hpText, npPurgeMsg );
         lstrcat( hpText, "\r\n" );
         }
      }
   return( hpText );
}

/* This function returns the size of the specified
 * message, in bytes. The size includes the NULL terminator
 * at the end.
 */
DWORD WINAPI EXPORT Amdb_GetMsgTextLength( PTH pth )
{
   return( pth->thItem.dwSize );
}

/* This function deallocates a message text buffer allocated
 * with the GetMsgText command.
 */
void WINAPI EXPORT Amdb_FreeMsgTextBuffer( HPSTR lpMsgText )
{
   FreeMemory32( &lpMsgText );
}

/* This function tests the specified message flags.
 */
BOOL WINAPI EXPORT Amdb_TestMsgFlags( PTH pth, DWORD wFlags )
{
   return( ( pth->thItem.dwFlags & wFlags ) == wFlags );
}

/* This function marks a message as read
 */
BOOL WINAPI EXPORT Amdb_MarkMsgRead( PTH pth, BOOL fRead )
{
   BOOL fWasRead = FALSE;
   HNDFILE fh;

   fWasRead = ( pth->thItem.dwFlags & MF_READ ) == MF_READ;
   if( pth->thItem.dwFlags & MF_READLOCK )
      return( fWasRead );
   if( fRead ) {
      if( !fWasRead ) {
         if( pth->ptl->tlItem.msgc.cUnRead )
            {
            if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
               {
               pth->thItem.dwFlags |= MF_READ;
               --pth->ptl->tlItem.msgc.cUnRead;
               --cTotalMsgc.cUnRead;
               --pth->ptl->pcl->msgc.cUnRead;
               --pth->ptl->pcl->pcat->msgc.cUnRead;
               if( pth->thItem.dwFlags & MF_PRIORITY )
                  {
                  --pth->ptl->tlItem.msgc.cUnReadPriority;
                  --cTotalMsgc.cUnReadPriority;
                  }
               pth->ptl->tlItem.dwFlags |= TF_CHANGED;
               UnlockMessage( fh, pth );
               if( pth->pthRoot )
                  --pth->pthRoot->wUnRead;
               fDataChanged = TRUE;
               Amuser_CallRegistered( AE_UNREADCHANGED, (LPARAM)pth->ptl, 0 );
               }
            }
         }
      }
   else {
      if( fWasRead )
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            pth->thItem.dwFlags &= ~MF_READ;
            ++pth->ptl->tlItem.msgc.cUnRead;
            ++pth->ptl->pcl->msgc.cUnRead;
            ++pth->ptl->pcl->pcat->msgc.cUnRead;
            if( pth->thItem.dwFlags & MF_PRIORITY )
               {
               ++pth->ptl->tlItem.msgc.cUnReadPriority;
               ++cTotalMsgc.cUnReadPriority;
               }
            ++cTotalMsgc.cUnRead;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            if( pth->pthRoot )
               ++pth->pthRoot->wUnRead;
            fDataChanged = TRUE;
            Amuser_CallRegistered( AE_UNREADCHANGED, (LPARAM)pth->ptl, 0 );
            }
      }
   return( fWasRead );
}

/* This function marks a message as having been withdrawn
 * on the host side.
 */
BOOL EXPORT WINAPI Amdb_MarkMsgWithdrawn( PTH pth, BOOL fWithdrawn )
{
   BOOL fWasWithdrawn = FALSE;
   HNDFILE fh;

   fWasWithdrawn = ( pth->thItem.dwFlags & MF_WITHDRAWN ) == MF_WITHDRAWN;
   if( fWithdrawn ) {
      if( !fWasWithdrawn )
         {
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            pth->thItem.dwFlags |= MF_WITHDRAWN;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            fDataChanged = TRUE;
            }
         }
      }
   else {
      if( fWasWithdrawn )
         {
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            pth->thItem.dwFlags &= ~MF_WITHDRAWN;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            fDataChanged = TRUE;
            }
         }
      }
   return( fWasWithdrawn );
}

BOOL WINAPI EXPORT Amdb_MarkMsgReadLock( PTH pth, BOOL fReadLock )
{
   BOOL fWasReadLock = FALSE;
   HNDFILE fh;

   fWasReadLock = ( pth->thItem.dwFlags & MF_READLOCK ) == MF_READLOCK;
   if( fReadLock ) {
      if( !fWasReadLock )
         {
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            pth->thItem.dwFlags |= MF_READLOCK;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            fDataChanged = TRUE;
            }
         }
      }
   else {
      if( fWasReadLock )
         {
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            pth->thItem.dwFlags &= ~MF_READLOCK;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            fDataChanged = TRUE;
            }
         }
      }
   return( fWasReadLock );
}

BOOL WINAPI EXPORT Amdb_MarkMsgCrossReferenced( PTH pth, BOOL fXref )
{
   BOOL fWasXref = FALSE;
   HNDFILE fh;

   fWasXref = ( pth->thItem.dwFlags & MF_XPOSTED ) == MF_XPOSTED;
   if( fXref ) {
      if( !fWasXref )
         {
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            pth->thItem.dwFlags |= MF_XPOSTED;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            fDataChanged = TRUE;
            }
         }
      }
   else {
      if( fWasXref )
         {
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            pth->thItem.dwFlags &= ~MF_XPOSTED;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            fDataChanged = TRUE;
            }
         }
      }
   return( fWasXref );
}

void WINAPI EXPORT Amdb_MarkMsgPriority( PTH pth, BOOL fPriority, BOOL fThread )
{
   HNDFILE fh;

   if( fPriority ) {
      if( !( pth->thItem.dwFlags & MF_PRIORITY ) )
         {
         BYTE wLevel;
         PTL ptl;

         ptl = pth->ptl;
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            pth->thItem.dwFlags |= MF_PRIORITY;
            ++pth->ptl->tlItem.msgc.cPriority;
            ++cTotalMsgc.cPriority;
            if( !( pth->thItem.dwFlags & MF_READ ) )
               {
               ++pth->ptl->tlItem.msgc.cUnReadPriority;
               ++cTotalMsgc.cUnReadPriority;
               }
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            }
         if( fThread )
            {
            wLevel = pth->wLevel;
            if( pth->pthRoot )
               ++pth->pthRoot->cPriority;
            else
               ++pth->cPriority;
            pth = pth->pthRefNext;
            while( pth && pth->wLevel > wLevel )
               {
               if( !( pth->thItem.dwFlags & MF_PRIORITY ) )
                  if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
                     {
                     pth->thItem.dwFlags |= MF_PRIORITY;
                     ++pth->ptl->tlItem.msgc.cPriority;
                     ++cTotalMsgc.cPriority;
                     if( !( pth->thItem.dwFlags & MF_READ ) )
                        {
                        ++pth->ptl->tlItem.msgc.cUnReadPriority;
                        ++cTotalMsgc.cUnReadPriority;
                        }
                     if( pth->pthRoot )
                        ++pth->pthRoot->cPriority;
                     UnlockMessage( fh, pth );
                     }
               pth = pth->pthRefNext;
               }
            }
         }
      }
   else {
      if( pth->thItem.dwFlags & MF_PRIORITY )
         {
         BYTE wLevel;
         PTL ptl;

         ptl = pth->ptl;
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            pth->thItem.dwFlags &= ~MF_PRIORITY;
            --pth->ptl->tlItem.msgc.cPriority;
            --cTotalMsgc.cPriority;
            if( !( pth->thItem.dwFlags & MF_READ ) )
               {
               --pth->ptl->tlItem.msgc.cUnReadPriority;
               --cTotalMsgc.cUnReadPriority;
               }
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            }
         if( fThread )
            {
            wLevel = pth->wLevel;
            if( pth->pthRoot )
               --pth->pthRoot->cPriority;
            else
               --pth->cPriority;
            pth = pth->pthRefNext;
            while( pth && pth->wLevel > wLevel )
               {
               if( pth->thItem.dwFlags & MF_PRIORITY )
                  if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
                     {
                     pth->thItem.dwFlags &= ~MF_PRIORITY;
                     --pth->ptl->tlItem.msgc.cPriority;
                     --cTotalMsgc.cPriority;
                     if( !( pth->thItem.dwFlags & MF_READ ) )
                        {
                        --pth->ptl->tlItem.msgc.cUnReadPriority;
                        --cTotalMsgc.cUnReadPriority;
                        }
                     if( pth->pthRoot )
                        --pth->pthRoot->cPriority;
                     UnlockMessage( fh, pth );
                     }
               pth = pth->pthRefNext;
               }
            }
         }
      }
}

void WINAPI EXPORT Amdb_MarkMsgIgnore( PTH pth, BOOL fIgnore, BOOL fThread )
{
   HNDFILE fh;

   if( fIgnore ) {
      if( !( pth->thItem.dwFlags & MF_IGNORE ) )
         {
         BOOL fHadUnread = FALSE;
         PTL ptl;
         BOOL fReadLock = FALSE;

         fReadLock = ( pth->thItem.dwFlags & MF_READLOCK ) == MF_READLOCK;
         ptl = pth->ptl;
         if( !fReadLock )
            fHadUnread = MarkOneMsgRead( pth );
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            pth->thItem.dwFlags |= fReadLock ? ( MF_IGNORE ) : ( MF_IGNORE | MF_READ ) ;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            }
         if( fThread )
            {
            BYTE wLevel;

            wLevel = pth->wLevel;
            pth = pth->pthRefNext;
            while( pth && pth->wLevel > wLevel )
               {
               fReadLock = ( pth->thItem.dwFlags & MF_READLOCK ) == MF_READLOCK;
               if( !fReadLock )
                  if( MarkOneMsgRead( pth ) )
                     fHadUnread = TRUE;
               if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
                  {
                  pth->thItem.dwFlags |= fReadLock ? ( MF_IGNORE ) : ( MF_IGNORE | MF_READ ) ;
                  UnlockMessage( fh, pth );
                  }
               pth = pth->pthRefNext;
               }
            }
         if( fHadUnread )
            Amuser_CallRegistered( AE_UNREADCHANGED, (LPARAM)ptl, 0 );
         }
      }
   else {
      if( pth->thItem.dwFlags & MF_IGNORE )
         {
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            pth->thItem.dwFlags &= ~MF_IGNORE;
            if( pth->thItem.dwFlags & MF_READLOCK )
               pth->thItem.dwFlags &= ~MF_READ;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            }
         if( fThread )
            {
            BYTE wLevel;

            wLevel = pth->wLevel;
            pth = pth->pthRefNext;
            while( pth && pth->wLevel > wLevel )
               {
               if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
                  {
                  pth->thItem.dwFlags &= ~MF_IGNORE;
                  if( pth->thItem.dwFlags & MF_READLOCK )
                     pth->thItem.dwFlags &= ~MF_READ;
                  UnlockMessage( fh, pth );
                  }
               pth = pth->pthRefNext;
               }
            }
         }
      }
}

/* This function marks the specified message read and
 * adjusts unread counters.
 */
BOOL FASTCALL MarkOneMsgRead( PTH pth )
{
   if( !( pth->thItem.dwFlags & MF_READ ) )
      {
      --pth->ptl->tlItem.msgc.cUnRead;
      --pth->ptl->pcl->msgc.cUnRead;
      --pth->ptl->pcl->pcat->msgc.cUnRead;
      if( pth->pthRoot )
         --pth->pthRoot->wUnRead;
      --cTotalMsgc.cUnRead;
      if( pth->thItem.dwFlags & MF_PRIORITY )
         {
         --pth->ptl->tlItem.msgc.cUnReadPriority;
         --cTotalMsgc.cUnReadPriority;
         }
      return( TRUE );
      }
   return( FALSE );
}

/* This function marks a message to be watched. Watched messages
 * have any comments automatically tagged as watched.
 */
void WINAPI EXPORT Amdb_MarkMsgWatch( PTH pth, BOOL fWatch, BOOL fThread )
{
   HNDFILE fh;

   if( fWatch ) {
      if( !( pth->thItem.dwFlags & MF_WATCH ) )
         {
         BOOL fHadUnread;
         BYTE wLevel;
         PTL ptl;

         ptl = pth->ptl;
         fHadUnread = MarkOneMsgRead( pth );
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            pth->thItem.dwFlags |= (MF_WATCH|MF_READ);
            if( pth->thItem.dwFlags & MF_HDRONLY )
               pth->thItem.dwFlags |= MF_TAGGED;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            ++pth->ptl->tlItem.msgc.cTagged;
            ++cTotalMsgc.cTagged;
            UnlockMessage( fh, pth );
            }
         if( fThread )
            {
            wLevel = pth->wLevel;
            pth = pth->pthRefNext;
            while( pth && pth->wLevel > wLevel )
               {
               if( MarkOneMsgRead( pth ) )
                  fHadUnread = TRUE;
               if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
                  {
                  pth->thItem.dwFlags |= (MF_WATCH|MF_READ);
                  if( pth->thItem.dwFlags & MF_HDRONLY )
                     pth->thItem.dwFlags |= MF_TAGGED;
                  ++pth->ptl->tlItem.msgc.cTagged;
                  ++cTotalMsgc.cTagged;
                  UnlockMessage( fh, pth );
                  }
               pth = pth->pthRefNext;
               }
            }
         if( fHadUnread )
            Amuser_CallRegistered( AE_UNREADCHANGED, (LPARAM)ptl, 0 );
         }
      }
   else {
      if( pth->thItem.dwFlags & MF_WATCH )
         {
         BYTE wLevel;

         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            pth->thItem.dwFlags &= ~(MF_WATCH|MF_TAGGED);
            --pth->ptl->tlItem.msgc.cTagged;
            --cTotalMsgc.cTagged;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            }
         if( fThread )
            {
            wLevel = pth->wLevel;
            pth = pth->pthRefNext;
            while( pth && pth->wLevel > wLevel )
               {
               if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
                  {
                  pth->thItem.dwFlags &= ~(MF_WATCH|MF_TAGGED);
                  --pth->ptl->tlItem.msgc.cTagged;
                  --cTotalMsgc.cTagged;
                  UnlockMessage( fh, pth );
                  }
               pth = pth->pthRefNext;
               }
            }
         }
      }
}

void WINAPI EXPORT Amdb_MarkThreadRead( PTH pth )
{
   HNDFILE fh;
   BYTE wLevel;
   PTL ptl;

   ptl = pth->ptl;
   wLevel = pth->wLevel;
   if( !( pth->thItem.dwFlags & (MF_READ|MF_READLOCK) ) )
      {
      --pth->ptl->tlItem.msgc.cUnRead;
      --pth->ptl->pcl->msgc.cUnRead;
      --pth->ptl->pcl->pcat->msgc.cUnRead;
      if( pth->pthRoot )
         --pth->pthRoot->wUnRead;
      --cTotalMsgc.cUnRead;
      if( pth->thItem.dwFlags & MF_PRIORITY )
         {
         --pth->ptl->tlItem.msgc.cUnReadPriority;
         --cTotalMsgc.cUnReadPriority;
         }
      if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
         {
         pth->thItem.dwFlags |= MF_READ;
         pth->ptl->tlItem.dwFlags |= TF_CHANGED;
         UnlockMessage( fh, pth );
         fDataChanged = TRUE;
         }
      }
   pth = pth->pthRefNext;
   while( pth && pth->wLevel > wLevel )
      {
      if( !( pth->thItem.dwFlags & (MF_READ|MF_READLOCK) ) )
         {
         --pth->ptl->tlItem.msgc.cUnRead;
         --pth->ptl->pcl->msgc.cUnRead;
         --pth->ptl->pcl->pcat->msgc.cUnRead;
         if( pth->pthRoot )
            --pth->pthRoot->wUnRead;
         if( pth->thItem.dwFlags & MF_PRIORITY )
            {
            --pth->ptl->tlItem.msgc.cUnReadPriority;
            --cTotalMsgc.cUnReadPriority;
            }
         --cTotalMsgc.cUnRead;
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            pth->thItem.dwFlags |= MF_READ;
            UnlockMessage( fh, pth );
            }
         }
      pth = pth->pthRefNext;
      }
   Amuser_CallRegistered( AE_UNREADCHANGED, (LPARAM)ptl, 0 );
}

/* This function sets or clears the notification flag for
 * the specified message
 */
BOOL EXPORT WINAPI Amdb_GetSetNotificationBit( PTH pth, int nSet )
{
   HNDFILE fh;
   
   if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
      {
      switch( nSet )
         {
         case BSB_CLRBIT:
            pth->thItem.dwFlags &= ~MF_NOTIFICATION;
            break;
   
         case BSB_SETBIT:
            pth->thItem.dwFlags |= MF_NOTIFICATION;
            break;
         }
      UnlockMessage( fh, pth );
      }
   return( ( pth->thItem.dwFlags & MF_NOTIFICATION ) == MF_NOTIFICATION );
}

/* This function sets or clears the cover note flag for
 * the specified message
 */
BOOL EXPORT WINAPI Amdb_GetSetCoverNoteBit( PTH pth, int nSet )
{
   HNDFILE fh;
   
   if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
      {
      switch( nSet )
         {
         case BSB_CLRBIT:
            pth->thItem.dwFlags &= ~MF_COVERNOTE;
            break;
   
         case BSB_SETBIT:
            pth->thItem.dwFlags |= MF_COVERNOTE;
            break;
         }
      UnlockMessage( fh, pth );
      }
   return( ( pth->thItem.dwFlags & MF_COVERNOTE ) == MF_COVERNOTE );
}

void WINAPI EXPORT Amdb_MarkMsgDelete( PTH pth, BOOL fDelete )
{
   HNDFILE fh;

   if( fDelete ) {
      if( !( pth->thItem.dwFlags & MF_DELETED ) )
         {
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            Amdb_MarkMsg( pth, FALSE );
            Amdb_MarkMsgKeep( pth, FALSE );
            Amdb_MarkMsgRead( pth, TRUE );
            pth->thItem.dwFlags |= MF_DELETED;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            fDataChanged = TRUE;
            }
         }
      }
   else {
      if( pth->thItem.dwFlags & MF_DELETED )
         {
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            pth->thItem.dwFlags &= ~MF_DELETED;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            fDataChanged = TRUE;
            }
         }
      }
}

void WINAPI EXPORT Amdb_MarkMsgKeep( PTH pth, BOOL fKeep )
{
   HNDFILE fh;

   if( fKeep ) {
      if( !( pth->thItem.dwFlags & MF_KEEP ) )
         {
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            pth->thItem.dwFlags |= MF_KEEP;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            fDataChanged = TRUE;
            }
         }
      }
   else {
      if( pth->thItem.dwFlags & MF_KEEP )
         {
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            pth->thItem.dwFlags &= ~MF_KEEP;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            fDataChanged = TRUE;
            }
         }
      }
}

void WINAPI EXPORT Amdb_MarkHdrOnly( PTH pth, BOOL fHdrOnly )
{
   HNDFILE fh;

   if( fHdrOnly )
      {
      if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
         {
         pth->thItem.dwFlags |= MF_HDRONLY;
         pth->ptl->tlItem.dwFlags |= TF_CHANGED;
         UnlockMessage( fh, pth );
         fDataChanged = TRUE;
         }
      }
   else
      {
      if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
         {
         pth->thItem.dwFlags &= ~MF_HDRONLY;
         pth->ptl->tlItem.dwFlags |= TF_CHANGED;
         UnlockMessage( fh, pth );
         fDataChanged = TRUE;
         }
      }
}

void WINAPI EXPORT Amdb_MarkMsg( PTH pth, BOOL fMark )
{
   HNDFILE fh;

   if( fMark ) {
      if( !( pth->thItem.dwFlags & MF_MARKED ) )
         {
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            ++pth->ptl->tlItem.msgc.cMarked;
            ++cTotalMsgc.cMarked;
            pth->thItem.dwFlags |= MF_MARKED;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            fDataChanged = TRUE;
            }
         }
      }
   else {
      if( pth->thItem.dwFlags & MF_MARKED )
         {
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            --pth->ptl->tlItem.msgc.cMarked;
            --cTotalMsgc.cMarked;
            pth->thItem.dwFlags &= ~MF_MARKED;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            fDataChanged = TRUE;
            }
         }
      }
}

void WINAPI EXPORT Amdb_MarkMsgBeingRetrieved( PTH pth, BOOL fMark )
{
   HNDFILE fh;

   if( fMark ) {
      if( !( pth->thItem.dwFlags & MF_BEINGRETRIEVED ) )
         {
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            pth->thItem.dwFlags |= MF_BEINGRETRIEVED;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            fDataChanged = TRUE;
            }
         }
      }
   else {
      if( pth->thItem.dwFlags & MF_BEINGRETRIEVED )
         {
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            pth->thItem.dwFlags &= ~MF_BEINGRETRIEVED;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            fDataChanged = TRUE;
            }
         }
      }
}

void WINAPI EXPORT Amdb_MarkMsgUnavailable( PTH pth, BOOL fMark )
{
   HNDFILE fh;

   if( fMark ) {
      if( !( pth->thItem.dwFlags & MF_UNAVAILABLE ) )
         {
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            pth->thItem.dwFlags |= MF_UNAVAILABLE;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            fDataChanged = TRUE;
            }
         }
      }
   else {
      if( pth->thItem.dwFlags & MF_UNAVAILABLE )
         {
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            pth->thItem.dwFlags &= ~MF_UNAVAILABLE;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            fDataChanged = TRUE;
            }
         }
      }
}

void WINAPI EXPORT Amdb_MarkMsgTagged( PTH pth, BOOL fTag )
{
   HNDFILE fh;

   if( fTag ) {
      if( !( pth->thItem.dwFlags & MF_TAGGED ) )
         {
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            ++pth->ptl->tlItem.msgc.cTagged;
            ++cTotalMsgc.cTagged;
            pth->thItem.dwFlags |= MF_TAGGED;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            fDataChanged = TRUE;
            }
         }
      }
   else {
      if( pth->thItem.dwFlags & MF_TAGGED )
         {
         if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
            {
            --pth->ptl->tlItem.msgc.cTagged;
            --cTotalMsgc.cTagged;
            pth->thItem.dwFlags &= ~MF_TAGGED;
            pth->ptl->tlItem.dwFlags |= TF_CHANGED;
            UnlockMessage( fh, pth );
            fDataChanged = TRUE;
            }
         }
      }
}

/* This function updates threading for the specified topic if
 * required.
 */
void EXPORT WINAPI Amdb_UpdateThreading( PTL ptl )
{
   if( ptl->tlItem.dwFlags & TF_BUILDTHREADLINES )
      {
      BuildThreadLines( ptl );
      ptl->tlItem.dwFlags &= ~TF_BUILDTHREADLINES;
      }
}

/* This function loads a topic of messages into memory. It is, unquestionably,
 * the bottleneck of the Ameol code and should be optimised as heavily as
 * possible.
 */
BOOL FASTCALL CommitTopicMessages( PTL ptl, BOOL fRepair )
{
   if( ptl->tlItem.dwFlags & TF_ONDISK )
   {
      char szFileName[ 13 ];
      HNDFILE fh;

      wsprintf( szFileName, "%s.THD", (LPSTR)ptl->tlItem.szFileName );
      if( ( fh = Amdb_OpenFile( szFileName, AOF_READ|AOF_SEQUENTIAL_IO ) ) != HNDFILE_ERROR )
      {
         HCURSOR hOldCursor;
         LPSTR lpFileBufferEnd;
         LPSTR lpFileBuffer;
         LPSTR lpFileBufPtr;
         DWORD dwFileSize;
         WORD wBufferSize;
         WORD wDefBufSize;
         DWORD cUnRead = 0;
         DWORD cMsgs = 0;
         DWORD cMarked = 0;
         DWORD cTagged = 0;
         DWORD cPriority = 0;
         DWORD cUnReadPriority = 0;
         BOOL fRepaired;
         BOOL fCorrupted;
         THITEM thItem;
         char sz[ 200 ];
         WORD cbMsg;
         long diff;
         PCL pcl;

         INITIALISE_PTR(lpFileBuffer);
         INITIALISE_PTR(lpFileBufferEnd);
         INITIALISE_PTR(lpFileBufPtr);

         /* Get the appropriate message size from
          * the topic header.
          */
         cbMsg = dbHdr.cbMsg;
         fRepaired = FALSE;
         hOldCursor = NULL;

         /* Get the file size and try and allocate a block of memory
          * to read the entire file.
          */
         if( 0 == wBufferUpperLimit )
            {
            lpFileBuffer = NULL;
            wBufferSize = 0;
            dwFileSize = 0;
            }
         else
            {
            dwFileSize = Amfile_GetFileSize( fh );
            wDefBufSize = ( wBufferUpperLimit / cbMsg ) * cbMsg;
            wBufferSize = (WORD)min( dwFileSize, wDefBufSize );
            if( wBufferSize > 0 && fNewMemory( &lpFileBuffer, wBufferSize ) )
               {
               DWORD cRead;

               cRead = Amfile_Read( fh, lpFileBuffer, wBufferSize );
               lpFileBufPtr = lpFileBuffer;
               lpFileBufferEnd = lpFileBuffer + cRead;
               dwFileSize -= cRead;
               }
            }

         /* Show the cursor.
          */
         if( ip.hwndFrame == GetActiveWindow() )
            hOldCursor = SetCursor( LoadCursor( NULL, IDC_WAIT ) );

         /* Loop until end of THD file.
          */
         fCorrupted = FALSE;
         while( TRUE )
            {
            PTH pth;

            if( NULL != lpFileBuffer )
               {
               /* Read from the buffer.
                */
               if( lpFileBufPtr == lpFileBufferEnd )
                  {
                  DWORD cRead;

                  /* File empty.
                   */
                  if( 0 == dwFileSize )
                     break;

                  /* Reload the buffer.
                   */
                  wBufferSize = (WORD)min( dwFileSize, wBufferSize );
                  cRead = Amfile_Read( fh, lpFileBuffer, wBufferSize );
                  lpFileBufPtr = lpFileBuffer;
                  lpFileBufferEnd = lpFileBuffer + cRead;
                  dwFileSize -= cRead;
                  }
               if( lpFileBufPtr + cbMsg > lpFileBufferEnd )
                  {
                  fCorrupted = TRUE;
                  break;
                  }
               memcpy( &thItem, lpFileBufPtr, cbMsg );
               lpFileBufPtr += cbMsg;
               }
            else
               {
               /* No buffer, so take the slow approach and read
                * from the file itself.
                */
               if( Amfile_Read( fh, &thItem, cbMsg ) != cbMsg )
                  break;
               }

            /* Sanity check.
             */
            if( thItem.dwMsg == 0 )
               {
               fCorrupted = TRUE;
               thItem.dwFlags |= MF_DESTROYED;
               }

            /* Skip message if marked as deleted.
             */
            if( !( thItem.dwFlags & MF_DESTROYED ) )
               {
               BOOL fExists;

               /* Create the message internally. BOTTLENECK ALERT! If this
                * fails, don't load the rest of the topic.
                */
               pth = CreateMsgIndirect( ptl, &thItem, NULL, &fExists );
               if( !pth )
                  {
                  Amdb_DiscardTopic( ptl );
                  Amfile_Close( fh );
                  return( FALSE );
                  }

               /* If the message didn't previously exist, adjust the
                * internal counts.
                */
               if( !fExists )
                  {
                  if( thItem.dwMsg > ptl->tlItem.dwMaxMsg )
                     ptl->tlItem.dwMaxMsg = thItem.dwMsg;
                  if( thItem.dwMsg < ptl->tlItem.dwMinMsg )
                     ptl->tlItem.dwMinMsg = thItem.dwMsg;
                  if( thItem.dwFlags & MF_MARKED )
                     ++cMarked;
                  if( thItem.dwFlags & MF_TAGGED )
                     ++cTagged;
                  if( thItem.dwFlags & MF_PRIORITY )
                     ++cPriority;
                  if( !( pth->thItem.dwFlags & MF_READ ) )
                     {
                     ++cUnRead;;
                     if( thItem.dwFlags & MF_PRIORITY )
                        ++cUnReadPriority;
                     }
                  ++cMsgs;
                  }
               }
            }
         Amfile_Close( fh );

         /* Done, so report to event log.
          */
         pcl = Amdb_FolderFromTopic( ptl );

         /* Delete buffer if required.
          */
         if( NULL != lpFileBuffer )
            FreeMemory( &lpFileBuffer );

         /* If topic was corrupted, need to rebuild.
          */
         if( fCorrupted && fRepair )
         {
            char chTmpBuf[ 200 ];

            wsprintf( chTmpBuf, GS(IDS_STR532), pcl->clItem.szFolderName, ptl->tlItem.szTopicName );
            MessageBox( GetFocus(), chTmpBuf, NULL, MB_OK|MB_ICONEXCLAMATION );
         }

         /* If asked to do so, check the message counts and
          * make any repairs.
          */
         else if( fRepair ) 
            {
            if( diff = cMsgs - ptl->tlItem.cMsgs )
               {
               ptl->tlItem.cMsgs += diff;
               ptl->pcl->clItem.cMsgs += diff;
               ptl->pcl->pcat->catItem.cMsgs += diff;
               fRepaired = TRUE;
               }
            if( diff = cMarked - ptl->tlItem.msgc.cMarked )
               {
               ptl->tlItem.msgc.cMarked += diff;
               cTotalMsgc.cMarked += diff;
               fRepaired = TRUE;
               }
            if( diff = cTagged - ptl->tlItem.msgc.cTagged )
               {
               ptl->tlItem.msgc.cTagged += diff;
               cTotalMsgc.cTagged += diff;
               fRepaired = TRUE;
               }
            if( diff = cPriority - ptl->tlItem.msgc.cPriority )
               {
               ptl->tlItem.msgc.cPriority += diff;
               cTotalMsgc.cPriority += diff;
               fRepaired = TRUE;
               }
            }

         /* Fixup the read/unread count. Do this even if fRepair is FALSE
          */
         if( diff = cUnRead - ptl->tlItem.msgc.cUnRead )
         {
            ptl->tlItem.msgc.cUnRead += diff;
            ptl->pcl->msgc.cUnRead += diff;
            ptl->pcl->pcat->msgc.cUnRead += diff;
            cTotalMsgc.cUnRead += diff;
            Amuser_CallRegistered( AE_UNREADCHANGED, (LPARAM)ptl, 0 );
            if( !( ptl->tlItem.dwFlags & TF_MARKALLREAD ) )
               fRepaired = TRUE;
            else
               fRepaired = FALSE;
         }

         /* Unread priority count is also affected by repair, so
          * check and adjust it if required.
          */
         if( diff = cUnReadPriority - ptl->tlItem.msgc.cUnReadPriority )
         {
            ptl->tlItem.msgc.cUnReadPriority += diff;
            cTotalMsgc.cUnReadPriority += diff;
            fRepaired = fRepair;
         }

         /* Log any repairs to the event log.
          */
         if( fRepaired )
         {
            wsprintf( sz, str1, Amdb_GetFolderName( pcl ), Amdb_GetTopicName( ptl ) );
            Amevent_AddEventLogItem( ETYP_WARNING|ETYP_DATABASE, sz );
            fDataChanged = TRUE;
         }

         /* Topic is no longer on disk.
          */
         ptl->tlItem.dwFlags &= ~TF_ONDISK;
         if( ptl->tlItem.dwFlags & TF_MARKALLREAD )
         {
            ptl->tlItem.dwFlags &= ~TF_MARKALLREAD;
            Amdb_MarkTopicRead( ptl );
         }

         /* Rebuild thread lines. This ought to be deferred until
          * we actually need to draw the topic.
          */
         ptl->tlItem.dwFlags |= TF_BUILDTHREADLINES;
         if( hOldCursor )
            SetCursor( hOldCursor );
         return( TRUE );
      }/*else
         ShowTHDFileError(2, ptl->tlItem.szFileName, ptl->tlItem.szTopicName);*/

      if( ptl->tlItem.cMsgs )
      {
         char chTmpBuf[ 200 ];

         _strupr( szFileName );
         wsprintf( chTmpBuf, GS(IDS_STR521), (LPSTR)szFileName );
         if( MessageBox( GetFocus(), chTmpBuf, NULL, MB_OKCANCEL|MB_ICONEXCLAMATION ) == IDOK )
         {
            ptl->tlItem.cMsgs = 0;
            if( ptl->tlItem.msgc.cUnRead )
               {
               cTotalMsgc.cUnRead -= ptl->tlItem.msgc.cUnRead;
               ptl->tlItem.msgc.cUnRead = 0;
               Amuser_CallRegistered( AE_UNREADCHANGED, (LPARAM)ptl, 0 );
               }
            cTotalMsgc.cMarked -= ptl->tlItem.msgc.cMarked;
            cTotalMsgc.cTagged -= ptl->tlItem.msgc.cTagged;
            ptl->tlItem.msgc.cMarked = 0;
            ptl->tlItem.msgc.cTagged = 0;
            ptl->tlItem.dwMinMsg = 0;
            ptl->tlItem.dwMaxMsg = 0;
            cTotalMsgc.cPriority -= ptl->tlItem.msgc.cPriority;
            cTotalMsgc.cUnReadPriority -= ptl->tlItem.msgc.cUnReadPriority;
            ptl->tlItem.msgc.cPriority = 0;
            ptl->tlItem.msgc.cUnReadPriority = 0;
            //ShowTHDFileError(3, ptl->tlItem.szFileName, ptl->tlItem.szTopicName);
            wsprintf( szFileName, "%s.THD", (LPSTR)ptl->tlItem.szFileName );
            Amdb_DeleteFile( szFileName );
         }

         /* Only log error if failure to open .THD file was genuine, and not
          * because topic has zero messages.
          */
         wsprintf( chTmpBuf, str2, (LPSTR)szFileName );
         Amevent_AddEventLogItem( ETYP_SEVERE|ETYP_DATABASE, chTmpBuf );
         return( FALSE );
      }
   }
   return( TRUE );
}

/* This function builds the thread lines mask for the messages in
 * the specified topic.
 */
void FASTCALL BuildThreadLines( PTL ptl )
{
   DWORD dwLast[ MAX_MASKLEVEL ];
   register int c;
   PTH pth;

   memset( &dwLast, 0, MAX_MASKLEVEL * sizeof( DWORD ) );
   for( pth = ptl->pthRefLast; pth; pth = pth->pthRefPrev )
      {
      DWORD dwMask;
      int nMinMask;

      nMinMask = 0;
      for( c = 0; c < MAX_MASKLEVEL; ++c )
         {
         if( pth->wLevel > nMinMask && pth->wLevel <= nMinMask + 32 )
            {
            dwLast[ c ] = dwLast[ c ] & ( ( pow2[ ( pth->wLevel - 1 ) & 31 ] ) - 1 );
            dwMask = pow2[ ( pth->wLevel - 1 ) & 31 ] | dwLast[ c ];
            }
         else if( pth->wLevel > nMinMask )
            dwMask = dwLast[ c ];
         else
            dwMask = 0L;
         pth->dwThdMask[ c ] = dwMask;
         dwLast[ c ] = dwMask;
         nMinMask += 32;
         }
      }
}

void WINAPI EXPORT Amdb_DrawThreadLines( HDC hdc, PTH pth, int x, int x2, int y1, int y2, int nThreadIndentPixels )
{
   register int c;
   int w2, h2;

   w2 = 2;
   h2 = ( y2 - y1 ) / 2;
   for( c = 0; c < MAX_MASKLEVEL && x < x2; ++c )
      {
      DWORD dwMask;
      DWORD dwNext;
      DWORD dwMaskNext;
      int d;

      dwMask = pth->dwThdMask[ c ];
      dwNext = ( c < MAX_MASKLEVEL - 1 ) ? pth->dwThdMask[ c + 1 ] : 0;
      dwMaskNext = pth->pthRefNext ? pth->pthRefNext->dwThdMask[ c ] : 0;
      for( d = 0; d < 32 && x < x2; ++d )
         {
         if( dwMask & 1 )
            {
            dwMask >>= 1;
            if( dwMask || dwNext )
               {
               MoveToEx( hdc, x + w2, y1, NULL );
               LineTo( hdc, x + w2, y2 );
               }
            else if( dwMaskNext & 1 )
               {
               MoveToEx( hdc, x + w2, y1, NULL );
               LineTo( hdc, x + w2, y2 );
               MoveToEx( hdc, x + w2, y1 + h2, NULL );
               LineTo( hdc, x + nThreadIndentPixels, y1 + h2 );
               }
            else if( dwNext == 0 )
               {
               MoveToEx( hdc, x + w2, y1, NULL );
               LineTo( hdc, x + w2, y1 + h2 );
               LineTo( hdc, x + nThreadIndentPixels, y1 + h2 );
               }
            }
         else
            dwMask >>= 1;
         dwMaskNext >>= 1;
         x += nThreadIndentPixels;
         }
      }
}

/* This function purges all unwanted messages from one folder following
 * the predefined purge criteria in the folder header.
 */
int WINAPI EXPORT Amdb_PurgeFolder( PCL pcl, PURGEPROC lpfCallBack )
{
   int cErrors;
   PTL ptl;

   cErrors = 0;
   for( ptl = pcl->ptlFirst; 0 == cErrors && ptl; )
      {
      PTL ptlNext;

      ptlNext = ptl->ptlNext;
      cErrors += Amdb_PurgeTopic( ptl, lpfCallBack );
      ptl = ptlNext;
      }
   pcl->clItem.wLastPurgeDate = Amdate_GetPackedCurrentDate();
   pcl->clItem.wLastPurgeTime = Amdate_GetPackedCurrentTime();
   return( cErrors );
}

int WINAPI EXPORT Amdb_PurgeTopic( PTL ptlv, PURGEPROC lpfCallBack )
{
   PURGECALLBACKINFO pci;
   PURGEOPTIONS po;
   PTL ptl;
   char szOldName[ 13 ];
   char szWorkName[ 13 ];
   BOOL fError;
   int cErrors = 0;
   HNDFILE fh;

   ptl = (PTL)ptlv;
   if( ptl == ptlCachedThd )
      {
      if( fhCachedThd != HNDFILE_ERROR )
         Amfile_Close( fhCachedThd );
      ptlCachedThd = NULL;
      fhCachedThd = HNDFILE_ERROR;
      }
   if( ptl == ptlCachedTxt )
      {
      if( fhCachedTxt != HNDFILE_ERROR )
         Amfile_Close( fhCachedTxt );
      ptlCachedTxt = NULL;
      fhCachedTxt = HNDFILE_ERROR;
      }
   po = ptl->tlItem.po;
   if( !( po.wFlags & PO_NOPURGE ) )
      {
      BOOL fWriteThdFile;

      /* The PO_NOPURGE bit is not set, so this topic gets
       * purged.
       */
      if( !CommitTopicMessages( ptl, TRUE ) )
         return( 1 );
      pci.pcl = Amdb_FolderFromTopic( ptl );
      fError = FALSE;
      Amdb_LockTopic( ptl );
      pci.ptl = ptl;
      pci.cMsgs = ptl->tlItem.cMsgs;
      if( (*lpfCallBack)( PCI_PURGING, &pci ) == FALSE )
         {
         Amdb_InternalUnlockTopic( ptl );
         return( 1 );
         }
      wsprintf( szOldName, "%s.THD", (LPSTR)ptl->tlItem.szFileName );
      wsprintf( szWorkName, "%s.WRK", (LPSTR)ptl->tlItem.szFileName );
      fWriteThdFile = FALSE;
      if( po.wFlags & PO_PURGE_IGNORED )
         {
         /* Purge all ignored messages.
          */
         cErrors += PurgeAllIgnored( ptl, &po, lpfCallBack, &pci );
         cErrors += PurgeAllDeleted( ptl, &po, lpfCallBack, &pci );
         fWriteThdFile = TRUE;
         }
      else if( po.wFlags & PO_DELETEONLY )
         {
         /* Purge all messages marked for deletion.
          */
         cErrors += PurgeAllDeleted( ptl, &po, lpfCallBack, &pci );
         fWriteThdFile = TRUE;
         }
      else if( po.wFlags & PO_SIZEPRUNE )
         {
         PTH pth;
         PTH pth2;
         WORD c;

         cErrors += PurgeAllDeleted( ptl, &po, lpfCallBack, &pci );
         pth = ptl->pthRefLast;
         for( c = 1; pth && c < po.wMaxSize; )
            {
            ++c;
            pth = pth->pthRefPrev;
            }
         /* If fewer messages in topic than MaxSize, start from first message */
         if( pth == NULL )
            pth = ptl->pthRefFirst;
         /* If we're going to empty the topic, set the target topic handle to NULL */
         if( po.wMaxSize == 0 )
            pth = NULL;
         while( pth && pth->wLevel > 0 )
            pth = pth->pthRefPrev;
         pth2 = ptl->pthRefFirst;
         while( pth2 != pth ) {
            PTH pthNext;
   
            pci.pth = pth2;
            pci.dwMsg = pth2->thItem.dwMsg;
            if( (*lpfCallBack)( PCI_YIELD, &pci ) == FALSE )
               {
               Amdb_InternalUnlockTopic( ptl );
               return( 1 );
               }
            pthNext = pth2->pthRefNext;
            if( !( pth2->thItem.dwFlags & (MF_KEEP|MF_READLOCK|MF_MARKED) ) && ( pth2->thItem.dwFlags & MF_READ ) )
               {
               if( po.wFlags & PO_BACKUP )
                  if( !BackupPurgedMessage( pth2 ) ) {
                     ++cErrors;
                     break;
                     }
               InternalDeleteMsg( pth2, FALSE, FALSE );
               }
            pth2 = pthNext;
            }
         fWriteThdFile = TRUE;
         }
      else if( po.wFlags & PO_DATEPRUNE )
         {
         PTH pth;
         PTH pthLastRoot;
         AM_DATE date;
         WORD wDate;

         /* Purging by date also deletes all messages explicitly marked
          * to be deleted.
          */
         cErrors += PurgeAllDeleted( ptl, &po, lpfCallBack, &pci );

         /* Now start from the last referenced message and work
          * backward.
          */
         pth = ptl->pthRefLast;
         pthLastRoot = NULL;
         Amdate_GetCurrentDate( &date );
         Amdate_AdjustDate( &date, -((int)po.wMaxDate) );
         wDate = Amdate_PackDate( &date );
         while( pth )
            {
            PTH pthPrev;

            pthPrev = pth->pthRefPrev;
            if( pth->thItem.wDate >= wDate )
               {
               /* This is a message in a thread which is newer than
                * the purge date, so skip the entire thread and continue
                * scanning from the end of the previous thread.
                */
               if( pth->pthRoot )
                  pth = pth->pthRoot;
               pthLastRoot = pth;
               if( pth )
                  pth = pth->pthRefPrev;
               }
            else
               {
               /* Have we reached the root. If so, then all messages in
                * the thread are older than the purge date. Zap the lot!
                */
               if( pth->pthRoot == NULL )
                  {
                  do {
                     PTH pthNext;

                     pthNext = pth->pthRefNext;
                     if( !( pth->thItem.dwFlags & (MF_KEEP|MF_READLOCK|MF_MARKED) ) && ( pth->thItem.dwFlags & MF_READ ) )
                        {
                        if( po.wFlags & PO_BACKUP )
                           if( !BackupPurgedMessage( pth ) ) {
                              ++cErrors;
                              break;
                              }
                        InternalDeleteMsg( pth, FALSE, FALSE );
                        }
                     pth = pthNext;
                     }
                  while( pth && pth != pthLastRoot );
                  }
               pth = pthPrev;
               }
            }
         fWriteThdFile = TRUE;
         }

      /* Done purging this topic, so rebuild the THD file if
       * required to do so.
       */
      if( fWriteThdFile )
         {
         if( ptl == ptlCachedThd )
            {
            Amfile_Close( fhCachedThd );
            fhCachedThd = HNDFILE_ERROR;
            ptlCachedThd = NULL;
            }
         if( NULL == ptl->pthFirst )
            {
            /* Topic shrank to nothing, so create a NULL size
             * topic.
             */
            if( ( fh = Amdb_CreateFile( szWorkName, AOF_WRITE ) ) != HNDFILE_ERROR )
               {
               Amfile_Close( fh );
               Amdb_DeleteFile( szOldName );
               Amdb_RenameFile( szWorkName, szOldName );
               }
            else
               ++cErrors;
            }
         else if( ( fh = Amdb_CreateFile( szWorkName, AOF_WRITE ) ) != HNDFILE_ERROR )
            {
            PTH pth;


            /* First purge all messages marked for deletion, then
             * write out the new THD file.
             */
            for( pth = ptl->pthFirst; pth; pth = pth->pthNext )
               {                 
               pci.pth = pth;
               pci.dwMsg = pth->thItem.dwMsg;
               if( (*lpfCallBack)( PCI_YIELD, &pci ) == FALSE )
                  {
                  Amdb_InternalUnlockTopic( ptl );
                  Amdb_DiscardTopic( ptl );
                  Amfile_Close( fh );
                  Amdb_DeleteFile( szWorkName );
                  return( 1 );
                  }
               pth->thItem.dwMsgPtr = Amfile_SetPosition( fh, 0L, ASEEK_CURRENT );
               while( !fError && Amfile_Write( fh, (LPCSTR)&pth->thItem, sizeof( THITEM ) ) != sizeof( THITEM ) )
                  if( !DiskSaveError( GetFocus(), fh ) )
                     fError = TRUE;
               }
            Amfile_Close( fh );
            if( fError )
               Amdb_DeleteFile( szWorkName );
            else {
               Amdb_DeleteFile( szOldName );
               Amdb_RenameFile( szWorkName, szOldName );
               }
            }
         }
      Amdb_InternalUnlockTopic( ptl );
      if( ptl->pthFirst )
         Amdb_DiscardTopic( ptl );
      cErrors += Amdb_CompactTopic( ptl, lpfCallBack );
      (*lpfCallBack)( PCI_PURGINGFINISHED, &pci );
      }
   fDataChanged = TRUE;
   ptl->tlItem.wLastPurgeDate = Amdate_GetPackedCurrentDate();
   ptl->tlItem.wLastPurgeTime = Amdate_GetPackedCurrentTime();
   return( cErrors );
}

/* This function purges all ignored messages.
 */
int FASTCALL PurgeAllIgnored( PTL ptl, PURGEOPTIONS * npo, PURGEPROC lpfCallBack, PURGECALLBACKINFO * ppci )
{
   PTH pthRefNext;
   int cErrors;
   PTH pth;

   cErrors = 0;
   for( pth = ptl->pthRefFirst; pth; pth = pthRefNext )
      {
      pthRefNext = pth->pthRefNext;
      if( pth->thItem.dwFlags & MF_IGNORE )
         {
         /* Save purged message in the backup file if
          * required.
          */
         if( npo->wFlags & PO_BACKUP )
            if( !BackupPurgedMessage( pth ) )
               {
               ++cErrors;
               break;
               }

         /* Physically purge this message.
          */
         ppci->pth = pth;
         ppci->dwMsg = pth->thItem.dwMsg;
         if( (*lpfCallBack)( PCI_YIELD, ppci ) == FALSE )
            return( 1 );
         InternalDeleteMsg( pth, FALSE, FALSE );
         }
      }
   return( cErrors );
}

/* This function purges all deleted messages from the specified
 * topic. This code is complicated by the need to preserve threading
 * so it either physically deletes all messages in a thread if they
 * qualify or marks just those that qualify as '*** Deleted by Ameol
 * Purge ***' otherwise.
 */
int FASTCALL PurgeAllDeleted( PTL ptl, PURGEOPTIONS * npo, PURGEPROC lpfCallBack, PURGECALLBACKINFO * ppci )
{
   UINT cDeleted;
   UINT cMsgs;
   BOOL fDone;
   PTH pth;
   PTH pth2;
   int cErrors;

   cDeleted = cMsgs = 0;
   cErrors = 0;
   pth2 = pth = ptl->pthRefFirst;
   fDone = FALSE;
   while( !fDone )
      if( pth == NULL || pth->wLevel == 0 )
         {
         PTH pth3;
         PTH pthRefNext;

         for( pth3 = pth2; pth3 != pth; pth3 = pthRefNext )
            {
            if( pth3 == NULL )
               break;
            pthRefNext = pth3->pthRefNext;
            if( pth3->thItem.dwFlags & (MF_DELETED|MF_PURGED) )
               {
               if( npo->wFlags & PO_BACKUP )
                  if( !BackupPurgedMessage( pth3 ) )
                     {
                     ++cErrors;
                     break;
                     }
               if( cDeleted == cMsgs )
                  {
                  ppci->pth = pth3;
                  ppci->dwMsg = pth3->thItem.dwMsg;
                  if( (*lpfCallBack)( PCI_YIELD, ppci ) == FALSE )
                     return( 1 );
                  InternalDeleteMsg( pth3, FALSE, FALSE );
                  }
               else if( !( pth3->thItem.dwFlags & MF_PURGED ) )
                  DeletePurgeMsg( pth3 );
               else
                  {
                  pth3->thItem.dwFlags &= ~MF_DELETED;
                  pth3->ptl->tlItem.dwFlags |= TF_CHANGED;
                  }
               }
            }
         if( pth == NULL )
            fDone = TRUE;
         else
            {
            pth2 = pth;
            cDeleted = cMsgs = 0;
            pth = pth->pthRefNext;
            }
         }
      else
         {
         if( pth->thItem.dwFlags & (MF_DELETED|MF_PURGED) )
            ++cDeleted;
         ++cMsgs;
         pth = pth->pthRefNext;
         }
   fDataChanged = TRUE;
   return( cErrors );
}

/* This function compacts the .TXT file for all topics in the specified
 * folder by removing messages not referenced in the header, such as
 * deleted, duplicate or purged messages. It also re-organises the order
 * of the messages to follow the index, improving access performance.
 *
 * The second parameter must point to a callback function that will be
 * called by the compacter when it starts a new topic, or when it needs
 * to check for the compaction being aborted. The format of this function
 * must be as follows:
 *
 *       BOOL WINAPI EXPORT Amdb_UserProc( int nType, PURGECALLBACKINFO FAR * lpci )
 *
 * Where:   nType    will be set to one of the following:
 *                      PCI_COMPACTING
 *                      PCI_YIELD
 *          lpci     is a FAR pointer to a PURGECALLBACKINFO structure.
 *
 * The callback is called with nType set to PCI_COMPACTING whenever the
 * compacter is starting a new topic, and the PURGECALLBACKINFO structure
 * will be filled out with pointers to the folder and topic. (The other
 * two fields will be undefined.)
 *
 * The callback is called with nType set to PCI_YIELD whenever the compacter
 * needs to check for the user interrupting the compaction. The PURGECALLBACKINFO
 * structure will have all four fields filled out. The callback should return
 * TRUE if the compacter can carry on compacting, or FALSE if it must abort.
 */
int WINAPI EXPORT Amdb_CompactTopic( PTL ptlv, PURGEPROC lpfCallBack )
{
   PURGECALLBACKINFO pci;
   int cErrors;
   char szOldName[ 13 ];
   char szWorkName[ 13 ];
   PURGEOPTIONS po;
   HNDFILE fhOld;
   HNDFILE fhWork;
   PTL ptl;
   BOOL fOk;
   PTH pth;

   cErrors = 0;
   ptl = (PTL)ptlv;
   if( ptl == ptlCachedTxt )
      {
      if( fhCachedTxt != HNDFILE_ERROR )
         {
         Amfile_Close( fhCachedTxt );
         fhCachedTxt = HNDFILE_ERROR;
         }
      ptlCachedTxt = NULL;
      }
   pci.pcl = Amdb_FolderFromTopic( ptl );
   CommitTopicMessages( ptl, TRUE );
   po = ptl->tlItem.po;
   pci.ptl = ptl;
   pci.cMsgs = ptl->tlItem.cMsgs;
   if( ( (*lpfCallBack)( PCI_COMPACTING, &pci ) ) == FALSE )
      return( 1 );
   wsprintf( szOldName, "%s.TXT", (LPSTR)ptl->tlItem.szFileName );
   wsprintf( szWorkName, "%s.WRK", (LPSTR)ptl->tlItem.szFileName );
   fOk = TRUE;
   pth = NULL;
   Amdb_LockTopic( ptl );
   if( ( fhOld = Amdb_OpenFile( szOldName, AOF_READ ) ) != HNDFILE_ERROR )
      {
      if( ( fhWork = Amdb_CreateFile( szWorkName, AOF_WRITE ) ) != HNDFILE_ERROR )
         {
         char sz[ 80 ];
         PCL pcl;

         /* Write TXT file header.
          */
         pcl = (PCL)pci.pcl;
         wsprintf( sz, "!TX %s %s %s\r\n", pcl->clItem.szFolderName, ptl->tlItem.szTopicName, szOldName );
         Amfile_Write( fhWork, sz, strlen( sz ) );

         /* Save the text file message header to the new file.
          */
         for( pth = ptl->pthFirst; fOk && pth; pth = pth->pthNext )
            {
            pci.pth = pth;
            pci.dwMsg = pth->thItem.dwMsg;
            if( (*lpfCallBack)( PCI_YIELD, &pci ) == FALSE )
               fOk = FALSE;
            else {
               if( pth->thItem.dwSize )
                  {
                  HPSTR hpText;
                  HNDFILE fh;

                  INITIALISE_PTR(hpText);
                  if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
                     {
                     Amfile_SetPosition( fhOld, pth->thItem.dwOffset, ASEEK_BEGINNING );
                     if( ( fNewMemory32( &hpText, pth->thItem.dwSize ) ) == 0 )
                        fOk = FALSE;
                     else
                        {
                        pth->dwTmpOffset = pth->thItem.dwOffset;
                        pth->thItem.dwOffset = (DWORD)Amfile_SetPosition( fhWork, 0L, ASEEK_CURRENT );
                        if( (DWORD)Amfile_Read32( fhOld, hpText, pth->thItem.dwSize ) != pth->thItem.dwSize )
                           fOk = FALSE;
                        while( fOk && (DWORD)Amfile_Write32( fhWork, hpText, pth->thItem.dwSize ) != pth->thItem.dwSize )
                           if( !DiskSaveError( GetFocus(), fhWork ) )
                              fOk = FALSE;
                        FreeMemory32( &hpText );
                        ptl->tlItem.dwFlags |= TF_CHANGED;
                        }
                     UnlockMessage( fh, pth );
                     }
                  }
               }
            }
         Amfile_Close( fhWork );
         }
      Amfile_Close( fhOld );
      }
   if( fOk ) {
      Amdb_DeleteFile( szOldName );
      Amdb_RenameFile( szWorkName, szOldName );
      }
   else if( pth ) {
      PTH pth2;

      for( pth2 = ptl->pthFirst; pth2 && pth2 != pth; pth2 = pth2->pthNext )
         {
         HNDFILE fh;

         if( ( fh = LockMessage( pth2 ) ) != HNDFILE_ERROR )
            {
            pth2->thItem.dwOffset = pth2->dwTmpOffset;
            UnlockMessage( fh, pth2 );
            }
         }
      Amdb_DeleteFile( szWorkName );
      ++cErrors;
      }
   Amdb_InternalUnlockTopic( ptl );
   Amdb_DiscardTopic( ptl );
   fDataChanged = TRUE;
   return( cErrors );
}

BOOL FASTCALL BackupPurgedMessage( PTH pth )
{
   char szFileName[ 13 ];
   BOOL fOk = FALSE;
   HNDFILE fh;

   strcpy( szFileName, "PURGE.BAK" );
   if( ( fh = Amdb_OpenFile( szFileName, AOF_WRITE ) ) != HNDFILE_ERROR )
      Amfile_SetPosition( fh, 0L, ASEEK_END );
   else
      fh = Amdb_CreateFile( szFileName, AOF_WRITE );
   if( fh != HNDFILE_ERROR )
      {
      fOk = WriteMsgText( fh, pth, FALSE );
      Amfile_Close( fh );
      }
   else
      {
      char sz[ 100 ];

      wsprintf( sz, str3, (LPSTR)szFileName );
      Amevent_AddEventLogItem( ETYP_SEVERE|ETYP_DATABASE, sz );
      }
   return( fOk );
}

BOOL FASTCALL WriteMsgText( HNDFILE fh, PTH pth, BOOL fLFOnly )
{
   HPSTR hpText;
   HPSTR lpSrc;
   HPSTR lpDest;
   DWORD dwSize = 0;

   /* Strip out CR from the message and compute the actual size.
    */
   hpText = lpDest = lpSrc = Amdb_GetMsgText( pth );
   if( hpText )
      {
      dwSize = 0;
      while( *lpSrc )
         {
         if( !( *lpSrc == 13 && fLFOnly ) )
            {
            *lpDest++ = *lpSrc;
            ++dwSize;
            }
         ++lpSrc;
         }
      *lpDest = '\0';
      }

   /* If the associated message text was found, write it. Otherwise insert
    * a message saying that it couldn't be found.
    */
   if( hpText ) {
      if( !WriteHugeMsgText( fh, hpText, dwSize ) )
         return( FALSE );
      Amdb_FreeMsgTextBuffer( hpText );
      }
   else {
      if( !WriteMsgTextLine( fh, "xxx", fLFOnly ) )
         return( FALSE );
      }
   if( !WriteMsgTextLine( fh, "", fLFOnly ) )
      return( FALSE );
   return( TRUE );
}

BOOL FASTCALL WriteMsgTextLine( HNDFILE fh, LPSTR lpText, BOOL fLFOnly )
{
   register UINT n;
   char chCRLF[2] = { 13, 10 };
   char chLF[1] = { 10 };

   n = lstrlen( lpText );
   while( Amfile_Write( fh, lpText, n ) != n ) {
      if( !DiskSaveError( GetFocus(), fh ) )
         return( FALSE );
      }
   if( !fLFOnly )
      while( Amfile_Write( fh, (LPSTR)&chCRLF, 2 ) != 2 )
         {
         if( !DiskSaveError( GetFocus(), fh ) )
            return( FALSE );
         }
   else
      while( Amfile_Write( fh, (LPSTR)&chLF, 1 ) != 1 )
         {
         if( !DiskSaveError( GetFocus(), fh ) )
            return( FALSE );
         }
   return( TRUE );
}

BOOL FASTCALL WriteHugeMsgText( HNDFILE fh, HPSTR hpText, DWORD dwSize )
{
   while( (DWORD)Amfile_Write32( fh, hpText, dwSize ) != dwSize )
      {
      if( !DiskSaveError( GetFocus(), fh ) )
         return( FALSE );
      }
   return( TRUE );
}

/* Returns whether or not ch is a valid character in a DOS filename.
 * Knows nothing about Windows/NT filenames, though.
 */
BOOL FASTCALL ValidFileNameChr( char ch )
{
   if( isalnum( ch ) || ch == '_' || ch == '^' || ch == '$' || ch == '~'
      || ch == '!' || ch == '#' || ch == '%' || ch == '&' || ch == '-'
      || ch == '{' || ch == '}' || ch == '(' || ch == ')' || ch == '@'
      || ch == '\'' || ch == '`' )
      return( TRUE );
   return( FALSE );
}

HNDFILE FASTCALL LockMessage( PTH pth )
{
   char szFileName[ 13 ];
   char sz[ 100 ];

   if( pth->ptl != ptlCachedThd || fhCachedThd == HNDFILE_ERROR )
      {
      if( fhCachedThd != HNDFILE_ERROR )
         Amfile_Close( fhCachedThd );
      wsprintf( szFileName, "%s.THD", (LPSTR)pth->ptl->tlItem.szFileName );
      fhCachedThd = Amdb_OpenFile( szFileName, AOF_WRITE );
      }
   if( fhCachedThd != HNDFILE_ERROR )
      {
      if( pth->nLockCount == 0 )
         {
         if( Amfile_SetPosition( fhCachedThd, pth->thItem.dwMsgPtr, ASEEK_BEGINNING ) == -1 )
            {
            wsprintf( szFileName, "%s.THD", (LPSTR)pth->ptl->tlItem.szFileName );
            wsprintf( sz, str7, (LPSTR)szFileName );
            Amevent_AddEventLogItem( ETYP_SEVERE|ETYP_DATABASE, sz );
            return( HNDFILE_ERROR );
            }
         else if( !TryLock( fhCachedThd, pth->thItem.dwMsgPtr, dbHdr.cbMsg ) )
            {
            wsprintf( szFileName, "%s.THD", (LPSTR)pth->ptl->tlItem.szFileName );
            wsprintf( sz, str6, (LPSTR)szFileName );
            Amevent_AddEventLogItem( ETYP_SEVERE|ETYP_DATABASE, sz );
            return( HNDFILE_ERROR );
            }
         }
      ptlCachedThd = pth->ptl;
      ++pth->nLockCount;
      }
   else {
      wsprintf( sz, str2, (LPSTR)szFileName );
      Amevent_AddEventLogItem( ETYP_SEVERE|ETYP_DATABASE, sz );
      }
   return( fhCachedThd );
}

void FASTCALL UnlockMessage( HNDFILE fh, PTH pth )
{
   char szFileName[ 13 ];
   char sz[ 100 ];

   if( pth->nLockCount > 0 )
      if( --pth->nLockCount == 0 )
         {
         WORD cbMsg;

         cbMsg = dbHdr.cbMsg;
         if( !TryUnlock( fh, pth->thItem.dwMsgPtr, cbMsg ) )
            {
            wsprintf( szFileName, "%s.THD", (LPSTR)pth->ptl->tlItem.szFileName );
            wsprintf( sz, str5, (LPSTR)szFileName );
            Amevent_AddEventLogItem( ETYP_SEVERE|ETYP_DATABASE, sz );
            }
         else if( Amfile_Write( fh, (LPCSTR)&pth->thItem, cbMsg ) != cbMsg )
            {
            wsprintf( szFileName, "%s.THD", (LPSTR)pth->ptl->tlItem.szFileName );
            wsprintf( sz, str4, (LPSTR)szFileName );
            Amevent_AddEventLogItem( ETYP_SEVERE|ETYP_DATABASE, sz );
            }
         }
}

BOOL FASTCALL TryLock( HNDFILE fh, DWORD dwPos, int size )
{
   if( fHasShare )
      if( !Amfile_Lock( fh, dwPos, size ) )
         return( FALSE );
   return( TRUE );
}

BOOL FASTCALL TryUnlock( HNDFILE fh, DWORD dwPos, int size )
{
   if( fHasShare )
      if( !Amfile_Unlock( fh, dwPos, size ) )
         return( FALSE );
   return( TRUE );
}

BOOL FASTCALL CreateUniqueFile( LPSTR lpszPrefix, LPSTR lpszExt, BOOL fCreate, LPSTR lpszFileName )
{
   char szFileName[ _MAX_PATH ];
   int id = 1000;

   do {
      char ch1, ch2;

      /* Use the first two characters of the topic name as the first two
       * characters of the filename, replacing any characters not legal
       * in a filename with an underscore.
       */
      ch1 = lpszPrefix[ 0 ];
      ch2 = lpszPrefix[ 1 ];
      if( !ValidFileNameChr( ch1 ) )   ch1 = '_';
      if( !ValidFileNameChr( ch2 ) )   ch2 = '_';
      wsprintf( lpszFileName, "%c%c%u", ch1, ch2, id );
      lstrcpy( szFileName, lpszFileName );
      if( *lpszExt != '.' )
         strcat( szFileName, "." );
      lstrcat( szFileName, lpszExt );
      ++id;
      }
   while( Amdb_QueryFileExists( szFileName ) );
   if( fCreate )
      {
      HNDFILE fh;

      /* Create the file in the data directory.
       */
      fh = Amdb_CreateFile( szFileName, AOF_WRITE );
      Amfile_Close( fh );
      }
   return( TRUE );
}

BOOL FASTCALL CreateUniqueFileMulti( LPSTR lpszPrefix, LPSTR lpszExt1, LPSTR lpszExt2, BOOL fCreate, LPSTR lpszFileName )
{
   char szFileName[ _MAX_PATH ];
   char szFileName2[ _MAX_PATH ];
   int id = 1000;

   do {
      char ch1, ch2;

      /* Use the first two characters of the topic name as the first two
       * characters of the filename, replacing any characters not legal
       * in a filename with an underscore.
       */
      ch1 = lpszPrefix[ 0 ];
      ch2 = lpszPrefix[ 1 ];
      if( !ValidFileNameChr( ch1 ) )   ch1 = '_';
      if( !ValidFileNameChr( ch2 ) )   ch2 = '_';
      wsprintf( lpszFileName, "%c%c%u", ch1, ch2, id );
      lstrcpy( szFileName, lpszFileName );
      lstrcpy( szFileName2, lpszFileName );
      if( *lpszExt1 != '.' )
         strcat( szFileName, "." );
      if( *lpszExt2 != '.' )
         strcat( szFileName2, "." );
      lstrcat( szFileName, lpszExt1 );
      lstrcat( szFileName2, lpszExt2 );
      ++id;
      }
   while( Amdb_QueryFileExists( szFileName ) || Amdb_QueryFileExists( szFileName2 ) );
   if( fCreate )
      {
      HNDFILE fh;

      /* Create the file in the data directory.
       */
      fh = Amdb_CreateFile( szFileName, AOF_WRITE );
      Amfile_Close( fh );
      }
   return( TRUE );
}

BOOL FASTCALL DiskSaveError( HWND hwnd, HNDFILE handle )
{
   return( MessageBox( hwnd, GS(IDS_STR522), NULL, MB_RETRYCANCEL|MB_ICONEXCLAMATION ) == IDRETRY );
}

BOOL WINAPI EXPORT Amdb_SetOutboxMsgBit( PTH pth, BOOL fSet )
{
   HNDFILE fh;

   if( ( fh = LockMessage( pth ) ) != HNDFILE_ERROR )
      {
      if( fSet )
         pth->thItem.dwFlags |= MF_OUTBOXMSG;
      else
         pth->thItem.dwFlags &= ~MF_OUTBOXMSG;
      UnlockMessage( fh, pth );
      return( TRUE );
      }
   return( FALSE );
}

BOOL WINAPI EXPORT Amdb_GetOutboxMsgBit( PTH pth )
{
   return( ( pth->thItem.dwFlags & MF_OUTBOXMSG ) == MF_OUTBOXMSG );
}

/* This function opens a file of the specified type from the
 * Ameol data directory.
 */
HNDFILE FASTCALL Amdb_OpenFile( LPCSTR lpszFileName, int wMode )
{
   char sz[ _MAX_PATH ];

   ASSERT( fInitDataDirectory );
   wsprintf( sz, "%s\\%s", (LPSTR)szDataDir, lpszFileName );
   return( Amfile_Open( sz, wMode ) );
}

/* This function opens a file of the specified type from the
 * Ameol active user directory.
 */
HNDFILE FASTCALL Amdb_UserOpenFile( LPCSTR lpszFileName, int wMode )
{
   char sz[ _MAX_PATH ];

   ASSERT( fInitDataDirectory );
   wsprintf( sz, "%s\\%s", (LPSTR)szUserDir, lpszFileName );
   return( Amfile_Open( sz, wMode ) );
}

/* This function deletes the specified file from the Ameol
 * user directory.
 */
BOOL FASTCALL Amdb_UserDeleteFile( LPCSTR lpszFileName )
{
   char sz[ _MAX_PATH ];

   ASSERT( fInitDataDirectory );
   wsprintf( sz, "%s\\%s", (LPSTR)szUserDir, lpszFileName );
   return( Amfile_Delete( sz ) );
}

/* This function creates a file of the specified type in the
 * Ameol data directory.
 */
HNDFILE FASTCALL Amdb_CreateFile( LPCSTR lpszFileName, int wMode )
{
   char sz[ _MAX_PATH ];

   ASSERT( fInitDataDirectory );
   wsprintf( sz, "%s\\%s", (LPSTR)szDataDir, lpszFileName );
   return( Amfile_Create( sz, 0 ) );
}

/* This function determines whether or not the specified file
 * already exists in the Ameol data directory.
 */
BOOL FASTCALL Amdb_QueryFileExists( LPCSTR lpszFileName )
{
   char sz[ _MAX_PATH ];

   ASSERT( fInitDataDirectory );
   wsprintf( sz, "%s\\%s", (LPSTR)szDataDir, lpszFileName );
   return( Amfile_QueryFile( sz ) );
}

/* This function deletes the specified file from the Ameol
 * data directory.
 */
BOOL FASTCALL Amdb_DeleteFile( LPCSTR lpszFileName )
{
   char sz[ _MAX_PATH ];

   ASSERT( fInitDataDirectory );
   wsprintf( sz, "%s\\%s", (LPSTR)szDataDir, lpszFileName );
   return( Amfile_Delete( sz ) );
}

BOOL FASTCALL Amdb_CopyFile( LPCSTR lpszOldName, LPCSTR lpszNewName )
{
   char sz1[ _MAX_PATH ];
   char sz2[ _MAX_PATH ];

   ASSERT( fInitDataDirectory );
   wsprintf( sz1, "%s\\%s", (LPSTR)szDataDir, lpszOldName );
   wsprintf( sz2, "%s\\%s", (LPSTR)szDataDir, lpszNewName );
   return( Amfile_Copy( sz1, sz2 ) );
}

/* This function renames the specified file in the Ameol
 * data directory.
 */
BOOL FASTCALL Amdb_RenameFile( LPCSTR lpszOldName, LPCSTR lpszNewName )
{
   char sz1[ _MAX_PATH ];
   char sz2[ _MAX_PATH ];

   ASSERT( fInitDataDirectory );
   wsprintf( sz1, "%s\\%s", (LPSTR)szDataDir, lpszOldName );
   wsprintf( sz2, "%s\\%s", (LPSTR)szDataDir, lpszNewName );
   return( Amfile_Rename( sz1, sz2 ) );
}

/* This function fills a listbox with the handles of all messages in the
 * specified topic.
 */
BOOL WINAPI Amdb_FastRealFastLoadListBox( HWND hwndList, PTL ptl, int nViewMode, BOOL f1 )
{
   register int c;
   int count;
   PTH pth;

   /* First fill the list box.
    */
   switch( nViewMode )
      {
      case VM_VIEWCHRON:
         count = Amdb_FastLoadChronological( hwndList, ptl );
         break;

      case VM_VIEWREF:
         count = Amdb_FastLoadRoots( hwndList, ptl, f1, TRUE );
         break;

      case VM_VIEWROOTS:
         count = Amdb_FastLoadRoots( hwndList, ptl, f1, FALSE );
         break;

      case VM_VIEWREFEX:
         count = Amdb_FastLoadReference( hwndList, ptl, f1 );
         break;

      default:
         ASSERT(FALSE);
         return( FALSE );
      }

   /* Set the index of every visible item in the list.
    */
   for( c = 0; c < count; ++c )
      {
      pth = (PTH)ListBox_GetItemData( hwndList, c );
      pth->nSel = c;
      }

   /* Update threading for this topic.
    */
   Amdb_UpdateThreading( ptl );
   return( TRUE );
}

/* This function fills the listbox window chronologically
 */
int FASTCALL Amdb_FastLoadChronological( HWND hwndList, PTL ptl )
{
   int count;
   PTH pth;

   /* First fill the listbox.
    */
   count = 0;
   for( pth = ptl->pthFirst; pth; pth = pth->pthNext )
      {
      int index;
      
      index = ListBox_AddString( hwndList, (LPARAM)(LPCSTR)pth );
      if( LB_ERR == index || LB_ERRSPACE == index )
         break;
      ++count;
      }
   for( ; pth; pth = pth->pthNext )
      pth->nSel = LB_ERR;
   return( count );
}

/* This function fills the listbox window by reference
 * mode.
 */
int FASTCALL Amdb_FastLoadReference( HWND hwndList, PTL ptl, BOOL f1 )
{
   int count;
   PTH pth;

   /* First fill the listbox.
    */
   count = 0;
   for( pth = ptl->pthRefFirst; pth; pth = pth->pthRefNext )
      {
      int index;
      
      if( f1 )
         pth->fExpanded = FALSE;
      index = ListBox_InsertString( hwndList, -1, (LPARAM)(LPCSTR)pth );
      if( LB_ERR == index || LB_ERRSPACE == index )
         break;
      ++count;
      }
   for( ; pth; pth = pth->pthRefNext )
      pth->nSel = LB_ERR;
   return( count );
}

/* This function fills the listbox window by roots or
 * reference mode.
 */
int FASTCALL Amdb_FastLoadRoots( HWND hwndList, PTL ptl, BOOL f1, BOOL fExpand )
{
   int count;
   PTH pth;

   /* First fill the listbox.
    */
   count = 0;
   for( pth = ptl->pthRefFirst; pth; )
      {
      int index;

      if( f1 )
         pth->fExpanded = fExpand;
      index = ListBox_InsertString( hwndList, -1, (LPARAM)(LPCSTR)pth );
      if( LB_ERR == index || LB_ERRSPACE == index )
         break;
      if( pth->fExpanded == FALSE )
         {
         pth = pth->pthRefNext;
         while( pth && pth->wLevel )
            {
            pth->nSel = LB_ERR;
            pth = pth->pthRefNext;
            }
         }
      else
         pth = pth->pthRefNext;
      ++count;
      }
   for( ; pth; pth = pth->pthRefNext )
      pth->nSel = LB_ERR;
   return( count );
}

/* This function attempts to re-build the ADB database index
 * file from the TXT and THD files.
 */
BOOL WINAPI EXPORT Amdb_RebuildDatabase( PCAT pcat, HWND hwnd )
{
   FINDDATA ft;
   register int n;
   HWND hDlg;
   char sz[ 8192 ];
   HFIND r;

   /* Destroy the current database.
    */
   Amdb_CloseDatabase();
   hDlg = Adm_Billboard( hRscLib, hwnd, MAKEINTRESOURCE(IDDLG_DBBUILD) );
   YieldToSystem();

   /* Locate all TXT files in the data
    * directory.
    */
   wsprintf( sz, "%s\\??????.TXT", (LPSTR)szDataDir );
   for( n = r = Amuser_FindFirst( sz, _A_NORMAL, &ft ); n != -1; n = Amuser_FindNext( r, &ft ) )
      {
      char szFileName[ 13 ];
      HNDFILE fh;
      PCL pcl;
      PTL ptl;

      /* Initialise.
       */
      pcl = NULL;
      ptl = NULL;

      /* For each one found, open it and read the TXT header.
       */
      if( ( fh = Amdb_OpenFile( ft.name, OF_READ|OF_SHARE_EXCLUSIVE ) ) != HNDFILE_ERROR )
         {
         char * pszTxtHdr;
         char * pszFolderName;
         char * pszTopicName;
         char * pszFileName;
         int cRead;

         /* Read up to 80 characters, the maximum size of the TXT header.
          * It'll be less though.
          */
         cRead = Amfile_Read( fh, sz, sizeof( sz ) - 1 );
         Amfile_Close( fh );
         sz[ cRead ] = '\0';

         /* Now parse the sz buffer and set up pointers to the conference,
          * topic and filename.
          */
         pszTxtHdr = sz;
         pszFolderName = sz;
         while( *pszFolderName && !isspace( *pszFolderName ) )
            ++pszFolderName;
         if( *pszFolderName )
            {
            /* Found end of folder name, so find
             * file name.
             */
            *pszFolderName++ = '\0';
            pszTopicName = pszFolderName;
            while( *pszTopicName && !isspace( *pszTopicName ) )
               ++pszTopicName;
            if( *pszTopicName )
               {
               /* Found end of topic name, so find
                * file name.
                */
               *pszTopicName++ = '\0';
               pszFileName = pszTopicName;
               while( *pszFileName && *pszFileName != '\r' )
                  ++pszFileName;
               *pszFileName = '\0';
               if( *pszFileName && strcmp( pszTxtHdr, "!TX" ) == 0 )
                  {
                  PCL pcl;
                  PTL ptl;

                  /* Found everything, so start working on what
                   * we have.
                   */
                  if( ( pcl = Amdb_OpenFolder( pcat, pszFolderName ) ) == NULL )
                     {
                     CLITEM clItem;

                     /* No such folder, so create it anew.
                      */
                     strcpy( clItem.szFolderName, pszFolderName );
                     strcpy( clItem.szFileName, pszFileName );
                     clItem.cTopics = 0;
                     clItem.szFileName[ 0 ] = '\0';
                     clItem.dwFlags = 0;
                     if( !fNewFolder( pcat, &pcl, NULL, &clItem ) )
                        continue;
                     strcpy( pcl->clItem.szFileName, pszFileName );
                     pcl->clItem.dateCreated = Amdate_GetPackedCurrentDate();
                     pcl->clItem.timeCreated = Amdate_GetPackedCurrentTime();
                     }

                  /* Look for and create the topic within the folder.
                   */
                  if( ( ptl = Amdb_OpenTopic( pcl, pszTopicName ) ) == NULL )
                     {
                     TLITEM tlItem;
                     register int c;

                     /* Create the topic.
                      */
                     strcpy( tlItem.szTopicName, pszTopicName );
                     tlItem.dwFlags = 0;
                     for( c = 0; c < 8 && ft.name[ c ] != '.'; ++c )
                        tlItem.szFileName[ c ] = ft.name[ c ];
                     tlItem.szFileName[ c ] = '\0';
                     tlItem.cMsgs = 0;
                     tlItem.dwMinMsg = 0;
                     tlItem.dwMaxMsg = 0;
                     if( !fNewTopic( &ptl, pcl, &tlItem ) )
                        continue;

                     /* Set the user defaults.
                      */
                     strcpy( ptl->tlItem.szSigFile, szGlobalSig );

                     /* Set the flist flag if we have an flist for this
                      * topic.
                      */
                     wsprintf( sz, "FLIST\\%s.FLS", ptl->tlItem.szFileName );
                     if( Amfile_QueryFile( sz ) )
                        ptl->tlItem.dwFlags |= TF_HASFILES;
                     ptl->wLockCount = 0;
                     SetDlgItemText( hDlg, IDD_CONFERENCE, Amdb_GetFolderName( pcl ) );
                     SetDlgItemText( hDlg, IDD_TOPIC, Amdb_GetTopicName( ptl ) );
                     }
                  }
               }
            }
         }

      /* If the .THD file exists, then we're in luck and we can build the DATABASE.DAT
       * file more comprehensively.
       */
      if( NULL != ptl )
         {
         Amdb_LockTopic( ptl );
         wsprintf( szFileName, "%s.THD", (LPSTR)ptl->tlItem.szFileName );
         if( ( fh = Amdb_OpenFile( szFileName, OF_READ|OF_SHARE_EXCLUSIVE ) ) != HNDFILE_ERROR )
            {
            DWORD cUnRead = 0;
            DWORD cMsgs = 0;
            DWORD cPriority = 0;
            DWORD cUnReadPriority = 0;
            DWORD cMarked = 0;
            THITEM thItem;

            while( TRUE )
               {
               PTH pth;

               YieldToSystem();
               if( Amfile_Read( fh, &thItem, sizeof( THITEM ) ) != sizeof( THITEM ) )
                  break;
               if( !( thItem.dwFlags & MF_DESTROYED ) )
                  {
                  DWORD dwMsgPtr;
                  BOOL fExists;

                  pth = CreateMsgIndirect( ptl, &thItem, &dwMsgPtr, &fExists );
                  if( !pth )
                     break;
                  if( thItem.dwFlags & MF_MARKED )
                     ++cMarked;
                  if( thItem.dwFlags & MF_PRIORITY )
                     {
                     ++cPriority;
                     if( !( pth->thItem.dwFlags & MF_READ ) )
                        ++cUnReadPriority;
                     }
                  if( !( pth->thItem.dwFlags & MF_READ ) )
                     ++cUnRead;;
                  if( cMsgs % 10 == 0 )
                     {
                     char sz[ 20 ];

                     wsprintf( sz, "%lu", thItem.dwMsg );
                     SetDlgItemText( hwnd, IDD_MESSAGE, sz );
                     }
                  if( !fExists )
                     ++cMsgs;
                  if( ptl->tlItem.dwMinMsg == 0 )
                     ptl->tlItem.dwMinMsg = thItem.dwMsg;
                  if( ptl->tlItem.dwMaxMsg == 0 )
                     ptl->tlItem.dwMaxMsg = thItem.dwMsg;
                  if( thItem.dwMsg < ptl->tlItem.dwMinMsg )
                     ptl->tlItem.dwMinMsg = thItem.dwMsg;
                  if( thItem.dwMsg > ptl->tlItem.dwMaxMsg )
                     ptl->tlItem.dwMaxMsg = thItem.dwMsg;
                  }
               }
            Amfile_Close( fh );
            ptl->tlItem.dwFlags &= ~TF_ONDISK;
            ptl->tlItem.cMsgs += cMsgs;
            pcl->clItem.cMsgs += cMsgs;
            ptl->tlItem.msgc.cMarked += cMarked;
            ptl->tlItem.msgc.cPriority += cPriority;
            ptl->tlItem.msgc.cUnReadPriority += cUnReadPriority;
            cTotalMsgc.cMarked += cMarked;
            cTotalMsgc.cPriority += cPriority;
            cTotalMsgc.cUnReadPriority += cUnReadPriority;
            ptl->tlItem.msgc.cUnRead += cUnRead;
            cTotalMsgc.cUnRead += cUnRead;
            }
         Amdb_UnlockTopic( ptl );
         }
      }
   Amuser_FindClose( r );

   /* All done!
    */
   DestroyWindow( hDlg );
   Amuser_FindClose( r );

   /* Save the new database.
    */
   fDataChanged = TRUE;
   Amdb_CommitDatabase( FALSE );
   return( TRUE );
}
